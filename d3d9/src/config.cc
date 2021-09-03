#include "internal.h"

using namespace league_loader;

std::wstring league_loader::GetConfigDir()
{
    WCHAR path[1024];
    DWORD length = GetEnvironmentVariableW(L"LEAGUE_LOADER_DIR", path, 1024);

    if (length > 0) {
        return std::wstring(path, length);
    }

    return L"";
}

std::wstring league_loader::GetPluginsDir()
{
    return GetConfigDir() + L"\\plugins";
}

std::wstring league_loader::GetConfigValue(const std::wstring &key)
{
    auto path = GetConfigDir() + L"\\config.cfg";

    WCHAR tmp[1024]{};
    DWORD len = GetPrivateProfileStringW(L"GENERAL",
        key.c_str(), L"", tmp, 1024, path.c_str());

    return std::wstring(tmp, len);
}