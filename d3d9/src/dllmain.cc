#include "internal.h"
#include "include/cef_version.h"

#pragma comment(lib, "version.lib")

using namespace league_loader;

void LoadD3d9Dll();
void LoadLibcefDll();

void HookBrowserProcess();
void HookRendererProcess();

static bool IsValidLibcefVersion()
{
    bool valid = false;

    DWORD  verHandle = 0;
    UINT   size = 0;
    LPBYTE lpBuffer = NULL;

    if (DWORD verSize = GetFileVersionInfoSize(L"libcef.dll", &verHandle))
    {
        LPSTR verData = new char[verSize];

        if (GetFileVersionInfo(L"libcef.dll", verHandle, verSize, verData)
            && VerQueryValue(verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size)
            && size > 0)
        {
            VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
            if (verInfo->dwSignature == 0xfeef04bd)
                valid = ((verInfo->dwFileVersionMS >> 16) & 0xffff) == CEF_VERSION_MAJOR;
        }

        delete[] verData;
    }

    return valid;
}

static void Attach()
{
    // Load D3D9.
    LoadD3d9Dll();

    // Get exe path.
    WCHAR path[2048];
    GetModuleFileNameW(NULL, path, 2048);

    // Determine which process to be hooked.

    // Browser process.
    if (str_contain(path, L"LeagueClientUx.exe"))
    {
        if (IsValidLibcefVersion())
        {
            LoadLibcefDll();
            HookBrowserProcess();
        }
        else
        {
            CreateThread(NULL, 0, [](LPVOID) -> DWORD
            {
                MessageBoxA(NULL,
                    "This League version is not supported.\nPlease check existing issues or open new issue about that.",
                    "League Loader", MB_TOPMOST | MB_OK | MB_ICONWARNING);
                ShellExecuteA(NULL, "open", "https://git.leagueloader.app", NULL, NULL, NULL);

                return 0;
            }, 0, 0, 0);
        }
    }
    // Renderer process.
    else if (str_contain(path, L"LeagueClientUxRender.exe") && str_contain(GetCommandLineW(), L"--type=renderer"))
    {
        if (IsValidLibcefVersion())
        {
            LoadLibcefDll();
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