#include "commons.h"
#include "include/cef_version.h"

#ifdef _MSC_VER
#pragma comment(lib, "libcef.lib")
#endif

static void WarnInvalidVersion()
{
    MessageBoxW(NULL,
        L"The version of your League of Legends Client is not supported.",
        L"Pengu Loader", MB_TOPMOST | MB_OK | MB_ICONWARNING);
}

static void WarnLoadingFails()
{
    MessageBoxW(NULL,
        L"Failed to load libcef.",
        L"Pengu Loader", MB_TOPMOST | MB_OK | MB_ICONWARNING);
}

static cef_color_t Hooked_GetBackgroundColor(void *rcx, cef_browser_settings_t *, cef_state_t)
{
    return 0; // SK_ColorTRANSPARENT
}

bool LoadLibcefDll(bool is_browser)
{
    if (HMODULE module = GetModuleHandleA("libcef.dll"))
    {
        auto GetVersion = reinterpret_cast<decltype(&cef_version_info)>
            (GetProcAddress(module, "cef_version_info"));

        // Check CEF version
        if (GetVersion == nullptr || GetVersion(0) != CEF_VERSION_MAJOR)
        {
            if (is_browser)
                CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WarnInvalidVersion, NULL, 0, NULL);

            return false;
        }

        if (is_browser)
        {
            // Find CefContext::GetBackgroundColor().
            const char *pattern = "41 83 F8 01 74 0B 41 83 F8 02 75 0A 45 31 C0";
            static hook::Hook<decltype(&Hooked_GetBackgroundColor)> GetBackgroundColor;
            auto delegate = (decltype(&Hooked_GetBackgroundColor))utils::patternScan(module, pattern);

            // Hook CefContext::GetBackgroundColor().
            GetBackgroundColor.hook(delegate, Hooked_GetBackgroundColor);
        }

        return true;
    }
    else
    {
        if (is_browser)
            WarnLoadingFails();

        return false;
    }
}