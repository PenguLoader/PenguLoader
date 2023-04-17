#include "internal.h"
#include "include/cef_version.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

bool LoadLibcefDll(bool is_browser);
void HookBrowserProcess();
void HookRendererProcess();

static void Initialize()
{
    // Get exe path.
    WCHAR _path[2048];
    wstring name(_path, GetModuleFileNameW(NULL, _path, _countof(_path)));
    name = name.substr(name.find_last_of(L"\\/") + 1);

    // Determine which process to be hooked.

    // Browser process.
    if (utils::strEqual(name, L"LeagueClientUx.exe", false))
    {
        if (LoadLibcefDll(true))
            HookBrowserProcess();
    }
    // Renderer process.
    else if (utils::strEqual(name, L"LeagueClientUxRender.exe", false)
        && utils::strContain(GetCommandLineW(), L"--type=renderer", false))
    {
        if (LoadLibcefDll(false))
            HookRendererProcess();
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

int APIENTRY _GetCefVersion()
{
    return CEF_VERSION_MAJOR;
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
    LPVOID pLoadLibraryW = &LoadLibraryW;

    WCHAR thisDllPath[2048]{};
    GetModuleFileNameW((HMODULE)&__ImageBase, thisDllPath, _countof(thisDllPath));

#ifdef _WIN64
    BOOL wow64 = FALSE;
    if (IsWow64Process(hProcess, &wow64) && wow64)
    {
        size_t length = lstrlenW(thisDllPath);
        lstrcpyW(thisDllPath + length - 4, L"32.dll");

        WCHAR rundll32[MAX_PATH];
        GetSystemWow64DirectoryW(rundll32, MAX_PATH);
        lstrcatW(rundll32, L"\\rundll32.exe");

        WCHAR cmdLine[2048]{};
        lstrcatW(cmdLine, L"\"");
        lstrcatW(cmdLine, rundll32);
        lstrcatW(cmdLine, L"\" \"");
        lstrcatW(cmdLine, thisDllPath);
        lstrcatW(cmdLine, L"\", #6001");

        STARTUPINFOW si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);

        if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, reinterpret_cast<DWORD *>(&pLoadLibraryW));

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
#endif

    size_t pathSize = (wcslen(thisDllPath) + 1) * sizeof(WCHAR);
    LPVOID pathAddr = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pathAddr, thisDllPath, pathSize, NULL);

    HANDLE loader = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pathAddr, 0, NULL);
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
        char msg[512];
        sprintf_s(msg, "Failed to create LeagueClientUx process, last error: 0x%08X.", GetLastError());
        MessageBoxA(0, msg, "Pengu Loader bootstraper", MB_ICONWARNING | MB_OK);
        return 1;
    }

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
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

    return 0;
}

int APIENTRY _GetLoadLibraryEntry(HWND, HINSTANCE, LPWSTR, int)
{
#ifndef _WIN64
    ExitProcess((int)reinterpret_cast<intptr_t>(&LoadLibraryW));
#endif

    return 0;
}