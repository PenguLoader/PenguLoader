#include "internal.h"

using namespace league_loader;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

static std::wstring GetLoaderDir()
{
    // Get this dll path.
    WCHAR thisPath[2048];
    GetModuleFileNameW((HINSTANCE)&__ImageBase, thisPath, 2048);

    OFSTRUCT of{};
    WCHAR finalPath[2048];
    // Get final path.
    HANDLE file = CreateFileW(thisPath, GENERIC_READ, 0x1, NULL, OPEN_EXISTING, 0, NULL);
    DWORD pathLength = GetFinalPathNameByHandleW(file, finalPath, 2048, FILE_NAME_OPENED);
    CloseHandle(file);

    std::wstring dir(finalPath, pathLength);

    // Remove prepended '\\?\' by GetFinalPathNameByHandle()
    if (dir.rfind(L"\\\\?\\", 0) == 0)
        dir.erase(0, 4);

    // Get parent folder.
    return dir.substr(0, dir.find_last_of(L"/\\"));
}

std::wstring league_loader::GetPluginsDir()
{
    return GetLoaderDir() + L"\\plugins";
}

std::wstring league_loader::GetConfigValue(const std::wstring &key)
{
    auto path = GetLoaderDir() + L"\\config.cfg";

    WCHAR value[1024]{};
    DWORD length = GetPrivateProfileStringW(L"Main",
        key.c_str(), L"", value, 1024, path.c_str());

    return std::wstring(value, length);
}