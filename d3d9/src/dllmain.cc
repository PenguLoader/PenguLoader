#include "internal.h"
#include "include/cef_version.h"

using namespace league_loader;

void LoadD3d9Dll();
void LoadLibcefDll();

void HookBrowserProcess();
void HookRendererProcess();

static void Attach()
{
    // Load D3D9.
    LoadD3d9Dll();

    // Get exe path.
    WCHAR path[2048];
    GetModuleFileNameW(NULL, path, 2048);

    // Determine which process to be hooked.
    if (str_contain(path, L"LeagueClientUx.exe"))
    {
        // Browser process.
        LoadLibcefDll();
        HookBrowserProcess();
    }
    else if (str_contain(path, L"LeagueClientUxRender.exe") && str_contain(GetCommandLineW(), L"--type=renderer"))
    {
        // Renderer process.
        LoadLibcefDll();
        HookRendererProcess();
    }
}

// DLL entry point.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            Attach();
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