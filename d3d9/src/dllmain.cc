#include "internal.h"

using namespace league_loader;

void LoadD3D9APIs();
void LoadLibCEFAPIs();

void HookBrowserProcess();
void HookRendererProcess();

static void Attach()
{
    // Load D3D9.
    LoadD3D9APIs();

    // Get exe path.
    WCHAR path[1024];
    GetModuleFileNameW(NULL, path, 1024);

    // Determine which process to be hooked.
    if (wcsstr(path, L"LeagueClientUx.exe"))
    {
        // Browser process.
        LoadLibCEFAPIs();
        HookBrowserProcess();
    }
    else if (wcsstr(path, L"LeagueClientUxRender.exe") && wcsstr(GetCommandLineW(), L"--type=renderer"))
    {
        // Renderer process.
        LoadLibCEFAPIs();
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