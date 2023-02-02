#include "internal.h"
#include "include/cef_version.h"

#pragma comment(lib, "version.lib")

using namespace league_loader;

void LoadD3d9Dll();
bool LoadLibcefDll();

void HookBrowserProcess();
void HookRendererProcess();

static void Initialize()
{
    // Load D3D9.
    LoadD3d9Dll();

    // Get exe path.
    WCHAR _path[2048];
    std::wstring name(_path, GetModuleFileNameW(NULL, _path, _countof(_path)));
    name = name.substr(name.find_last_of(L"\\/") + 1);

    // Determine which process to be hooked.

    // Browser process.
    if (!_wcsicmp(name.c_str(), L"LeagueClientUx.exe"))
    {
        if (LoadLibcefDll())
            HookBrowserProcess();
    }
    // Renderer process.
    else if (!_wcsicmp(name.c_str(), L"LeagueClientUxRender.exe") && str_contain(GetCommandLineW(), L"--type=renderer"))
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