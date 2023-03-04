#include "internal.h"
#include "include/cef_version.h"

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