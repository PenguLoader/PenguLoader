#include "commons.h"
#include "hook.h"

bool check_libcef_version(bool is_browser);
void HookBrowserProcess();
void HookRendererProcess();

#if OS_WIN

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

void InjectThisDll(HANDLE hProcess);

static hook::Hook<decltype(&CreateProcessW)> Old_CreateProcessW;
static BOOL WINAPI Hooked_CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
    bool is_renderer = std::regex_search(lpCommandLine,
        std::wregex(L"LeagueClientUxRender\\.exe.+--type=renderer", std::wregex::icase));

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
    if (std::regex_search(exe_path,
        std::wregex(L"LeagueClientUx\\.exe$", std::wregex::icase)))
    {
        if (LoadLibcefDll(true))
        {
            HookBrowserProcess();

            // Hook CreateProcessW.
            Old_CreateProcessW.hook(&CreateProcessW, Hooked_CreateProcessW);
        }
    }
    // Render process.
    else if (std::regex_search(exe_path,
        std::wregex(L"LeagueClientUxRender\\.exe$", std::wregex::icase)))
    {
        // Renderer only.
        if (wcsstr(GetCommandLineW(), L"--type=renderer") != nullptr)
        {
            if (LoadLibcefDll(false))
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

static DWORD64 CompactPEVersion(LPCWSTR file)
{
    DWORD64 version = 0;
    DWORD  verHandle = 0;
    UINT   size = 0;
    LPBYTE lpBuffer = NULL;

    if (DWORD verSize = GetFileVersionInfoSize(file, &verHandle))
    {
        LPSTR verData = new char[verSize];

        if (GetFileVersionInfo(file, verHandle, verSize, verData)
            && VerQueryValue(verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size)
            && size > 0)
        {
            VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
            if (verInfo->dwSignature == 0xfeef04bd)
                version = verInfo->dwFileVersionMS
                | (static_cast<DWORD64>(verInfo->dwFileVersionLS) << 32u);
        }

        delete[] verData;
    }

    return version;
}

static void RemoveOldModule()
{
    WCHAR path[2048]{};
    GetCurrentDirectoryW(_countof(path), path);
    lstrcatW(path, L"\\d3d9.dll");

    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return;

    if ((attr & FILE_ATTRIBUTE_REPARSE_POINT)           // symlink
        || CompactPEVersion(path) == 0x4A610907000A0000ULL) // old module 
    {
        DeleteFileW(path);
    }
}

void InjectThisDll(HANDLE hProcess)
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
    RemoveOldModule();

    NTSTATUS (NTAPI *NtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    NTSTATUS (NTAPI *NtRemoveProcessDebug)(HANDLE, HANDLE);
    NTSTATUS (NTAPI *NtClose)(HANDLE Handle);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    if (!CreateProcessW(NULL, commandLine, NULL, NULL, FALSE,
        CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi))
    {
        char msg[128];
        sprintf_s(msg, "Failed to create LeagueClientUx process, last error: 0x%08X.", GetLastError());
        dialog::alert(msg, "Pengu Loader bootstrapper", dialog::DIALOG_WARNING);
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

    if (prog == "LeagueClientUx") {
        if (check_libcef_version(true)) {
            dialog::alert("Continue debugging...", "Debug me");
            HookBrowserProcess();
        }
        else {
            _exit(0);
        }
    }
    else if (prog == "LeagueClientUx Helper (Renderer)") {
        // if (check_libcef_version(false)) {
        //     //HookRendererProcess();
        // }
    }
}

#endif