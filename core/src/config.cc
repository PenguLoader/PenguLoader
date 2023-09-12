#include "commons.h"
#include <fstream>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

wstr config::loaderDir()
{
    static wstr cachedPath{};
    if (!cachedPath.empty()) return cachedPath;

    // Get this dll path.
    WCHAR thisPath[2048];
    GetModuleFileNameW((HINSTANCE)&__ImageBase, thisPath, 2048);

    DWORD attr = GetFileAttributesW(thisPath);
    if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) != FILE_ATTRIBUTE_REPARSE_POINT)
    {
        cachedPath = thisPath;
        return cachedPath = cachedPath.substr(0, cachedPath.find_last_of(L"/\\"));
    }

    OFSTRUCT of{};
    WCHAR finalPath[2048];
    // Get final path.
    HANDLE file = CreateFileW(thisPath, GENERIC_READ, 0x1, NULL, OPEN_EXISTING, 0, NULL);
    DWORD pathLength = GetFinalPathNameByHandleW(file, finalPath, 2048, FILE_NAME_OPENED);
    CloseHandle(file);

    wstr dir{ finalPath, pathLength };

    // Remove prepended '\\?\' by GetFinalPathNameByHandle()
    if (dir.rfind(L"\\\\?\\", 0) == 0)
        dir.erase(0, 4);

    // Get parent folder.
    return cachedPath = dir.substr(0, dir.find_last_of(L"/\\"));
}

wstr config::assetsDir()
{
    return loaderDir() + L"\\assets";
}

wstr config::pluginsDir()
{
    return loaderDir() + L"\\plugins";
}

wstr config::datastorePath()
{
    return loaderDir() + L"\\datastore";
}

wstr config::cacheDir()
{
    wchar_t path[2048];
    size_t length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, _countof(path));

    if (length == 0)
        return leagueDir() + L"\\Cache";

    lstrcatW(path, L"\\Riot Games\\League of Legends\\Cache");
    return path;
}

wstr config::leagueDir()
{
    wchar_t buf[2048];
    size_t length = GetModuleFileNameW(nullptr, buf, _countof(buf));

    wstr path(buf, length);
    return path.substr(0, path.find_last_of(L"/\\"));
}

static map<wstr, wstr> getConfigMap()
{
    static bool cached = false;
    static map<wstr, wstr> map{};

    if (!cached)
    {
        auto path = config::loaderDir() + L"\\config";
        std::wifstream file(path);

        if (file.is_open())
        {
            std::wstring line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line[0] != L';')
                {
                    size_t pos = line.find(L"=");
                    if (pos != std::wstring::npos)
                    {
                        std::wstring key = line.substr(0, pos);
                        std::wstring value = line.substr(pos + 1);
                        map[key] = value;
                    }
                }
            }
            file.close();
        }

        cached = true;
    }

    return map;
}

wstr config::getConfigValue(const wstr &key, const wstr &fallback)
{
    auto map = getConfigMap();
    auto it = map.find(key);
    auto value = fallback;

    if (it != map.end())
        value = it->second;

#ifdef _DEBUG
    wprintf(L"config: %s -> %s\n", key.c_str(), value.c_str());
#endif

    return value;
}

bool config::getConfigValueBool(const wstr &key, bool fallback)
{
    auto map = getConfigMap();
    auto it = map.find(key);
    auto value = fallback;

    if (it != map.end())
    {
        if (it->second == L"0" || it->second == L"false")
            value = false;
        else if (it->second == L"1" || it->second == L"true")
            value = true;
    }

#ifdef _DEBUG
    wprintf(L"config: %s -> %s\n", key.c_str(), value ? L"true" : L"false");
#endif

    return value;
}

int config::getConfigValueInt(const wstr &key, int fallback)
{
    auto map = getConfigMap();
    auto it = map.find(key);
    auto value = fallback;

    if (it != map.end())
        value = std::stoi(it->second);
    
#ifdef _DEBUG
    wprintf(L"config: %s -> %d\n", key.c_str(), value);
#endif

    return value;
}