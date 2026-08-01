#include "pengu.h"
#include "hook.h"
#include "include/cef_version.h"

bool check_libcef_version(bool is_browser);
void HookBrowserProcess();
void HookRendererProcess();

#if OS_WIN

#include <tlhelp32.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
void InjectThisDll(HANDLE hProcess);

static DWORD GetParentProcessId(DWORD processId)
{
    DWORD parentId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (entry.th32ProcessID == processId)
            {
                parentId = entry.th32ParentProcessID;
                break;
            }
        }
        while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return parentId;
}

// The snapshot -> OpenProcess sequence above is not atomic. If the parent exits
// in the gap and Windows recycles its PID, we would reparent LCUX onto an
// unrelated process. A recycled PID is always younger than the process that
// read it, so reject any candidate created after us.
static bool IsPlausibleParent(HANDLE process)
{
    FILETIME parentCreated, selfCreated, ignored;

    if (!GetProcessTimes(process, &parentCreated, &ignored, &ignored, &ignored)
        || !GetProcessTimes(GetCurrentProcess(), &selfCreated, &ignored, &ignored, &ignored))
        return false;

    return CompareFileTime(&parentCreated, &selfCreated) <= 0;
}

// Open whoever launched this bootstrapper. Under IFEO that is the process that
// tried to start LeagueClientUx.exe -- normally LeagueClient.exe.
// PROCESS_QUERY_LIMITED_INFORMATION is requested alongside the create right
// because IsPlausibleParent needs it; it is the most broadly grantable query
// right, so it does not narrow where the reparent applies in practice.
static HANDLE OpenInheritedParentProcess()
{
    DWORD parentId = GetParentProcessId(GetCurrentProcessId());
    if (parentId == 0)
        return NULL;

    HANDLE process = OpenProcess(PROCESS_CREATE_PROCESS | PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE, parentId);

    if (process && !IsPlausibleParent(process))
    {
        CloseHandle(process);
        return NULL;
    }

    return process;
}

static void FreeAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST list)
{
    DeleteProcThreadAttributeList(list);
    HeapFree(GetProcessHeap(), 0, list);
}

// Build the attribute list that reparents the child onto our own parent.
// Returns NULL (leaving *parentProcess NULL) when reparenting isn't available,
// in which case the caller just creates the process normally.
static LPPROC_THREAD_ATTRIBUTE_LIST CreateParentAttributeList(HANDLE *parentProcess)
{
    *parentProcess = OpenInheritedParentProcess();
    if (*parentProcess == NULL)
        return NULL;

    SIZE_T size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);

    auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, size));
    if (list)
    {
        if (InitializeProcThreadAttributeList(list, 1, 0, &size))
        {
            // UpdateProcThreadAttribute stores the pointer, not the value: the
            // HANDLE has to stay alive until DeleteProcThreadAttributeList runs.
            // That's why it lives in the caller's frame and not in this one.
            if (UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
                parentProcess, sizeof(*parentProcess), NULL, NULL))
                return list;

            DeleteProcThreadAttributeList(list);
        }

        HeapFree(GetProcessHeap(), 0, list);
    }

    CloseHandle(*parentProcess);
    *parentProcess = NULL;
    return NULL;
}

static bool wcsfindi(const wchar_t *str, const wchar_t *sub)
{
    size_t str_len = wcslen(str), sub_len = wcslen(sub);
    if (sub_len > str_len)
        return false;
    for (size_t i = 0; i <= str_len - sub_len; ++i) {
        for (size_t j = 0; j < sub_len; ++j)
            if (towlower(str[i + j]) != towlower(sub[j]))
                goto next;
        return true;
        next:;
    }
    return false;
}

static hook::Hook<decltype(&CreateProcessW)> Old_CreateProcessW;
static BOOL WINAPI Hooked_CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
    bool is_renderer = wcsfindi(lpCommandLine, L"LeagueClientUxRender.exe")
        && wcsfindi(lpCommandLine, L"--type=renderer");

    if (is_renderer)
        dwCreationFlags |= CREATE_SUSPENDED;

    BOOL success = Old_CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    if (success && is_renderer)
    {
        InjectThisDll(lpProcessInformation->hProcess);
        ResumeThread(lpProcessInformation->hThread);
    }

    return success;
}

static void Initialize()
{
    WCHAR exe_path[2048]{};
    GetModuleFileNameW(nullptr, exe_path, _countof(exe_path));

    // Determine which process to be hooked.
    // Browser process.
    if (wcsfindi(exe_path, L"LeagueClientUx.exe"))
    {
        if (check_libcef_version(true))
        {
            HookBrowserProcess();

            // Hook CreateProcessW.
            Old_CreateProcessW.hook(&CreateProcessW, Hooked_CreateProcessW);
        }
    }
    // Render process.
    else if (wcsfindi(exe_path, L"LeagueClientUxRender.exe"))
    { 
        // Renderer only.
        if (wcsstr(GetCommandLineW(), L"--type=renderer") != nullptr)
        {
            if (check_libcef_version(false))
                HookRendererProcess();
        }
    }
}

// DLL entry point.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            Initialize();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

static void InjectThisDll(HANDLE hProcess)
{
    HMODULE kernel32 = GetModuleHandleA("kernel32");
    auto pVirtualAllocEx = (decltype(&VirtualAllocEx))GetProcAddress(kernel32, "VirtualAllocEx");
    auto pWriteProcessMemory = (decltype(&WriteProcessMemory))GetProcAddress(kernel32, "WriteProcessMemory");
    auto pCreateRemoteThread = (decltype(&CreateRemoteThread))GetProcAddress(kernel32, "CreateRemoteThread");

    WCHAR thisDllPath[2048]{};
    GetModuleFileNameW((HMODULE)&__ImageBase, thisDllPath, _countof(thisDllPath));

    size_t pathSize = (wcslen(thisDllPath) + 1) * sizeof(WCHAR);
    LPVOID pathAddr = pVirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    pWriteProcessMemory(hProcess, pathAddr, thisDllPath, pathSize, NULL);

    HANDLE loader = pCreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)&LoadLibraryW, pathAddr, 0, NULL);
    WaitForSingleObject(loader, INFINITE);
    CloseHandle(loader);
}

int APIENTRY _BootstrapEntry(HWND, HINSTANCE, LPWSTR commandLine, int)
{
    LONG (NTAPI *NtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    LONG (NTAPI *NtRemoveProcessDebug)(HANDLE, HANDLE);
    LONG (NTAPI *NtClose)(HANDLE Handle);

    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    // Reparent LeagueClientUx.exe onto our own parent (LeagueClient.exe) so the
    // rundll32 bootstrapper doesn't sit between them in the process tree.
    // Discord keys its League Client -> in-game stream handoff off that
    // ancestry and fails to switch when rundll32 is in the middle. rundll32
    // stays alive as a sibling either way -- LeagueClient.exe waits on it to
    // know when the client exits, so it can't simply exit early. See #106.
    HANDLE parentProcess = NULL;
    si.lpAttributeList = CreateParentAttributeList(&parentProcess);

    DWORD creationFlags = CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS;
    if (si.lpAttributeList)
        creationFlags |= EXTENDED_STARTUPINFO_PRESENT;

    // cb has to match the flag: the extended struct only when we pass it.
    si.StartupInfo.cb = si.lpAttributeList ? sizeof(si) : sizeof(si.StartupInfo);

    BOOL created = CreateProcessW(NULL, commandLine, NULL, NULL, FALSE,
        creationFlags, NULL, NULL, &si.StartupInfo, &pi);
    DWORD error = created ? 0 : GetLastError();

    // Reparenting can fail even when the handle was granted -- most plausibly
    // when the parent sits in a job object that restricts child processes.
    // Losing the Discord handoff is acceptable; failing to start the client is
    // not, so retry once with normal parenting.
    if (!created && si.lpAttributeList)
    {
        FreeAttributeList(si.lpAttributeList);
        CloseHandle(parentProcess);
        parentProcess = NULL;

        ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(si.StartupInfo);

        created = CreateProcessW(NULL, commandLine, NULL, NULL, FALSE,
            CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si.StartupInfo, &pi);
        error = created ? 0 : GetLastError();
    }

    if (si.lpAttributeList)
        FreeAttributeList(si.lpAttributeList);
    if (parentProcess)
        CloseHandle(parentProcess);

    if (!created)
    {
        char msg[128];
        sprintf_s(msg, "Failed to create LeagueClientUx process, last error: 0x%08X.", error);
        MessageBoxA(NULL, msg, "Pengu Loader bootstrapper", MB_ICONWARNING | MB_OK | MB_TOPMOST);
        return 1;
    }

    HMODULE ntdll = GetModuleHandleA("ntdll");
    (LPVOID &)NtQueryInformationProcess = GetProcAddress(ntdll, "NtQueryInformationProcess");
    (LPVOID &)NtRemoveProcessDebug = GetProcAddress(ntdll, "NtRemoveProcessDebug");
    (LPVOID &)NtClose = GetProcAddress(ntdll, "NtClose");

    HANDLE hDebug;
    if (NtQueryInformationProcess(pi.hProcess, 30, &hDebug, sizeof(HANDLE), 0) >= 0)
    {
        NtRemoveProcessDebug(pi.hProcess, hDebug);
        NtClose(hDebug);
    }

    InjectThisDll(pi.hProcess);
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

#elif OS_MAC

__attribute__((constructor)) static void dllmain(int argc, const char **argv)
{
    std::string prog(argv[0]);
    prog = prog.substr(prog.rfind('/') + 1);

    if (prog == "LeagueClientUx")
    {
#if _DEBUG
        char msg[128];
        snprintf(msg, sizeof(msg)-1, "Debug me: %d", getpid());
        dialog::alert("Continue debugging...", msg);
#endif
        if (check_libcef_version(true))
        {
            HookBrowserProcess();
        }
    }
    else if (prog == "LeagueClientUx Helper (Renderer)")
    {
        if (check_libcef_version(false))
        {
            HookRendererProcess();
        }
    }
}

#endif

int _GetCefVersion()
{
    return CEF_VERSION_MAJOR;
}