#include "internal.h"
#include "include/cef_version.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

bool LoadLibcefDll();
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
        if (LoadLibcefDll())
            HookBrowserProcess();
    }
    // Renderer process.
    else if (utils::strEqual(name, L"LeagueClientUxRender.exe", false)
        && utils::strContain(GetCommandLineW(), L"--type=renderer", false))
    {
        if (LoadLibcefDll())
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
    WCHAR thisDllPath[2048]{};
    GetModuleFileNameW((HMODULE)&__ImageBase, thisDllPath, _countof(thisDllPath));

    size_t pathSize = (wcslen(thisDllPath) + 1) * sizeof(WCHAR);
    LPVOID pathAddr = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pathAddr, thisDllPath, pathSize, NULL);

    HANDLE loader = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)&LoadLibraryW, pathAddr, 0, NULL);
    WaitForSingleObject(loader, INFINITE);
    CloseHandle(loader);
}

int APIENTRY _BootstrapEntry(HWND, HINSTANCE, LPWSTR commandLine, int)
{
    RemoveOldModule();

    NTSTATUS (NTAPI *NtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    NTSTATUS (NTAPI *NtRemoveProcessDebug)(HANDLE, HANDLE);
    NTSTATUS (NTAPI *NtClose)(HANDLE Handle);

    HMODULE ntdll = LoadLibraryA("ntdll.dll");
    (LPVOID &)NtQueryInformationProcess = GetProcAddress(ntdll, "NtQueryInformationProcess");
    (LPVOID &)NtRemoveProcessDebug = GetProcAddress(ntdll, "NtRemoveProcessDebug");
    (LPVOID &)NtClose = GetProcAddress(ntdll, "NtClose");

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    if (!CreateProcessW(NULL, commandLine, NULL, NULL, FALSE,
        CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi))
    {
        char msg[512];
        sprintf_s(msg, "Failed to create LeagueClientUx process, last error: %08X.", GetLastError());
        MessageBoxA(0, msg, "Pengu Loader bootstraper", MB_ICONWARNING | MB_OK);
    }

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