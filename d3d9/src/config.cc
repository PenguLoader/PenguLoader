#include "internal.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

wstring config::getLoaderDir()
{
    static wstring cachedPath{};
    if (!cachedPath.empty()) return cachedPath;

    // Get this dll path.
    WCHAR thisPath[2048];
    GetModuleFileNameW((HINSTANCE)&__ImageBase, thisPath, 2048);

    OFSTRUCT of{};
    WCHAR finalPath[2048];
    // Get final path.
    HANDLE file = CreateFileW(thisPath, GENERIC_READ, 0x1, NULL, OPEN_EXISTING, 0, NULL);
    DWORD pathLength = GetFinalPathNameByHandleW(file, finalPath, 2048, FILE_NAME_OPENED);
    CloseHandle(file);

    wstring dir(finalPath, pathLength);

    // Remove prepended '\\?\' by GetFinalPathNameByHandle()
    if (dir.rfind(L"\\\\?\\", 0) == 0)
        dir.erase(0, 4);

    // Get parent folder.
    return cachedPath = dir.substr(0, dir.find_last_of(L"/\\"));
}

wstring config::getAssetsDir()
{
    return getLoaderDir() + L"\\assets";
}

wstring config::getPluginsDir()
{
    return getLoaderDir() + L"\\plugins";
}

wstring config::getConfigValue(const wstring &key)
{
    auto path = getLoaderDir() + L"\\config";

    WCHAR value[1024]{};
    DWORD length = GetPrivateProfileStringW(L"Main",
        key.c_str(), L"", value, 1024, path.c_str());

    return wstring(value, length);
}