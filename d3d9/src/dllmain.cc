#include "internal.h"

using namespace league_loader;

void LoadD3D9APIs();
void LoadLibCEFAPIs();

static void Attach()
{
    // Load D3D9.
    LoadD3D9APIs();

    // Get exe path.
    WCHAR name[1024];
    GetModuleFileNameW(NULL, name, 1024);

    // Determine which process to be hooked.
    // Browser process.
    if (wcsstr(name, L"LeagueClientUx.exe") != NULL) {
        LoadLibCEFAPIs();
        HookBrowser();
    }
    else if (wcsstr(name, L"LeagueClientUxRender.exe") != NULL) {
        LPCWSTR commandLine = GetCommandLineW();
        // Renderer process only.
        if (wcsstr(GetCommandLineW(), L"--type=renderer")) {
            LoadLibCEFAPIs();
            HookRenderer();
        }
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason) {
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
