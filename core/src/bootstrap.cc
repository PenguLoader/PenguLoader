#include "bootstrap.h"

// See bootstrap.h: the macOS build compiles every .cc under src/.
#ifdef _WIN32

#include <tlhelp32.h>
#include <stdio.h>
#include <string>

// =============================================================================
// IFEO bootstrap.
//
// Windows redirects LeagueClientUx.exe through whatever the IFEO Debugger
// value names — `rundll32 "<boot.dll>", #6000` — handing it the original
// command line. rundll32 passes on only the arguments that follow the entry
// point, so what reaches launch() is already the client's own command line. We
// re-create the real process ourselves, inject the plugin runtime, and stay
// alive until it exits: LeagueClient.exe waits on the process it created to
// know when the client is gone, so exiting early would look like a crash to
// the launcher.
//
// DEBUG_ONLY_THIS_PROCESS is what stops IFEO firing again on the process we
// create; the debugger is detached immediately afterwards so CEF doesn't pay
// the debuggee penalty for the whole session.
// =============================================================================

namespace
{
    DWORD parent_process_id(DWORD processId)
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

    // The snapshot -> OpenProcess sequence isn't atomic. If the parent exits in
    // the gap and Windows recycles its PID, we would reparent LCUX onto an
    // unrelated process. A recycled PID is always younger than the process that
    // read it, so reject any candidate created after us.
    bool plausible_parent(HANDLE process)
    {
        FILETIME parentCreated, selfCreated, ignored;

        if (!GetProcessTimes(process, &parentCreated, &ignored, &ignored, &ignored)
            || !GetProcessTimes(GetCurrentProcess(), &selfCreated, &ignored, &ignored, &ignored))
            return false;

        return CompareFileTime(&parentCreated, &selfCreated) <= 0;
    }

    // Open whoever launched us. Under IFEO that is the process that tried to
    // start LeagueClientUx.exe -- normally LeagueClient.exe.
    // PROCESS_QUERY_LIMITED_INFORMATION is requested alongside the create right
    // because plausible_parent needs it; it is the most broadly grantable query
    // right, so it does not narrow where the reparent applies in practice.
    HANDLE open_inherited_parent()
    {
        DWORD parentId = parent_process_id(GetCurrentProcessId());
        if (parentId == 0)
            return NULL;

        HANDLE process = OpenProcess(PROCESS_CREATE_PROCESS | PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE, parentId);

        if (process && !plausible_parent(process))
        {
            CloseHandle(process);
            return NULL;
        }

        return process;
    }

    void free_attribute_list(LPPROC_THREAD_ATTRIBUTE_LIST list)
    {
        DeleteProcThreadAttributeList(list);
        HeapFree(GetProcessHeap(), 0, list);
    }

    // Build the attribute list that reparents the child onto our own parent.
    // Returns NULL (leaving *parentProcess NULL) when reparenting isn't
    // available, in which case the caller just creates the process normally.
    LPPROC_THREAD_ATTRIBUTE_LIST create_parent_attribute_list(HANDLE *parentProcess)
    {
        *parentProcess = open_inherited_parent();
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
}

bool bootstrap::inject(HANDLE process, const std::filesystem::path &dll)
{
    if (dll.empty())
        return false;

    HMODULE kernel32 = GetModuleHandleW(L"kernel32");
    if (kernel32 == nullptr)
        return false;

    auto pVirtualAllocEx = (decltype(&VirtualAllocEx))GetProcAddress(kernel32, "VirtualAllocEx");
    auto pWriteProcessMemory = (decltype(&WriteProcessMemory))GetProcAddress(kernel32, "WriteProcessMemory");
    auto pCreateRemoteThread = (decltype(&CreateRemoteThread))GetProcAddress(kernel32, "CreateRemoteThread");

    if (!pVirtualAllocEx || !pWriteProcessMemory || !pCreateRemoteThread)
        return false;

    auto native = dll.wstring();
    size_t pathSize = (native.size() + 1) * sizeof(wchar_t);

    LPVOID pathAddr = pVirtualAllocEx(process, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    if (pathAddr == nullptr)
        return false;

    if (!pWriteProcessMemory(process, pathAddr, native.c_str(), pathSize, NULL))
        return false;

    HANDLE loader = pCreateRemoteThread(process, NULL, 0,
        (LPTHREAD_START_ROUTINE)&LoadLibraryW, pathAddr, 0, NULL);

    if (loader == NULL)
        return false;

    // Bounded, never INFINITE. LoadLibraryW runs the DLL's DllMain, and core's
    // own CEF check puts up a modal dialog when the version doesn't match or
    // libcef is missing -- which holds the loader thread open for as long as
    // the dialog is on screen. Waiting forever would leave the client
    // suspended behind it, which is exactly the "client never launches"
    // failure this path exists to prevent.
    //
    // Injection normally completes in milliseconds, so this only trips on
    // pathological cases. When it does, we resume the client anyway and let it
    // run without the plugin runtime. The remote thread is left to finish on
    // its own; closing our handle doesn't disturb it.
    constexpr DWORD INJECT_TIMEOUT_MS = 10000;

    if (WaitForSingleObject(loader, INJECT_TIMEOUT_MS) != WAIT_OBJECT_0)
    {
        CloseHandle(loader);
        return false;
    }

    // LoadLibraryW's return value doubles as the thread exit code: 0 means the
    // DLL failed to load. Worth distinguishing from "couldn't start a thread",
    // because the client runs either way and only the runtime is missing.
    DWORD exitCode = 0;
    GetExitCodeThread(loader, &exitCode);
    CloseHandle(loader);

    return exitCode != 0;
}

int bootstrap::launch(wchar_t *commandLine, const std::filesystem::path &injectDll)
{
    LONG (NTAPI *NtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    LONG (NTAPI *NtRemoveProcessDebug)(HANDLE, HANDLE);
    LONG (NTAPI *NtClose)(HANDLE Handle);

    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    // Reparent LeagueClientUx.exe onto our own parent (LeagueClient.exe) so the
    // bootstrapper doesn't sit between them in the process tree. Discord keys
    // its League Client -> in-game stream handoff off that ancestry and fails
    // to switch when something is in the middle. We stay alive as a sibling
    // either way -- LeagueClient.exe waits on us to know when the client
    // exits, so we can't simply exit early. See #106.
    HANDLE parentProcess = NULL;
    si.lpAttributeList = create_parent_attribute_list(&parentProcess);

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
        free_attribute_list(si.lpAttributeList);
        CloseHandle(parentProcess);
        parentProcess = NULL;

        ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(si.StartupInfo);

        created = CreateProcessW(NULL, commandLine, NULL, NULL, FALSE,
            CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si.StartupInfo, &pi);
        error = created ? 0 : GetLastError();
    }

    if (si.lpAttributeList)
        free_attribute_list(si.lpAttributeList);
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

    // Best-effort. An empty path is the passthrough case: no runtime was
    // resolved, and the client still has to start.
    inject(pi.hProcess, injectDll);

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

#endif // _WIN32
