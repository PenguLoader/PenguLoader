#include "commons.h"
#include "include/cef_version.h"

#ifdef _WIN64
#pragma comment(lib, "libcef.lib")
#else
#pragma comment(lib, "libcef32.lib")
#endif

static int GetFileMajorVersion(LPCWSTR path)
{
    int version = 0;

    DWORD  verHandle = 0;
    UINT   size = 0;
    LPBYTE lpBuffer = NULL;

    if (DWORD verSize = GetFileVersionInfoSize(path, &verHandle))
    {
        LPSTR verData = new char[verSize];

        if (GetFileVersionInfo(path, verHandle, verSize, verData)
            && VerQueryValue(verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size)
            && size > 0)
        {
            VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
            if (verInfo->dwSignature == 0xfeef04bd)
                version = ((verInfo->dwFileVersionMS >> 16) & 0xffff);
        }

        delete[] verData;
    }

    return version;
}

static void WarnInvalidVersion()
{
    MessageBoxW(NULL,
        L"The version of your League of Legends Client is not supported.\n"
        L"Please check existing issues or open new issue about that, and wait for the new update.",
        L"Pengu Loader", MB_TOPMOST | MB_OK | MB_ICONWARNING);

    utils::openLink(L"https://git.pengu.lol");
}

#ifdef _WIN64
#define THISCALL_PARAMS void *          // rcx
#else
#define THISCALL_PARAMS void *, void *  // ecx edx
#endif

static cef_color_t __fastcall
Hooked_GetBackgroundColor(THISCALL_PARAMS, cef_browser_settings_t *, cef_state_t)
{
    return 0; // SK_ColorTRANSPARENT
}

bool LoadLibcefDll(bool is_browser)
{
    LPCWSTR libcef = L"libcef.dll";

    if (GetFileMajorVersion(libcef) != CEF_VERSION_MAJOR)
    {
        if (is_browser)
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WarnInvalidVersion, NULL, 0, NULL);
        return false;
    }

    if (HMODULE module = GetModuleHandle(libcef))
    {
        if (is_browser)
        {
            // Find CefContext::GetBackgroundColor().
#ifdef _WIN64
            const char *pattern = "41 83 F8 01 74 0B 41 83 F8 02 75 0A 45 31 C0";
#else
            const char *pattern = "55 89 E5 53 56 8B 55 0C 8B 45 08 83 FA 01 74 09";
#endif

            static hook::Hook<decltype(Hooked_GetBackgroundColor)> GetBackgroundColor;
            auto delegate = (decltype(&Hooked_GetBackgroundColor))utils::patternScan(module, pattern);

            // Hook CefContext::GetBackgroundColor().
            GetBackgroundColor.hook(delegate, Hooked_GetBackgroundColor);
        }

        return true;
    }

    return false;
}