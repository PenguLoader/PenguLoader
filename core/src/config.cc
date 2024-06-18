#include "commons.h"
#include <fstream>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

path config::loaderDir()
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

path config::pluginsDir()
{
    return loaderDir() / "plugins";
}

path config::datastorePath()
{
    return loaderDir() / "datastore";
}

path config::cacheDir()
{
    wchar_t path[2048];
    size_t length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, _countof(path));

    if (length == 0)
        return leagueDir() / "Cache";

    lstrcatW(path, L"\\Riot Games\\League of Legends\\Cache");
    return path;
}

path config::leagueDir()
{
    wchar_t buf[2048];
    size_t length = GetModuleFileNameW(nullptr, buf, _countof(buf));

    wstr path(buf, length);
    return path.substr(0, path.find_last_of(L"/\\"));
}

static map<str, str> getConfigMap()
{
    static bool cached = false;
    static map<str, str> map{};

    if (!cached)
    {
        auto path = config::loaderDir() / "config";
        std::ifstream file(path);

        if (file.is_open())
        {
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line[0] != ';')
                {
                    size_t pos = line.find("=");
                    if (pos != std::string::npos)
                    {
                        std::string key = line.substr(0, pos);
                        std::string value = line.substr(pos + 1);
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

static str getConfigValue(const char *key, const char *fallback)
{
    auto map = getConfigMap();
    auto it = map.find(key);
    str value = fallback;

    if (it != map.end())
        value = it->second;

    return value;
}

static bool getConfigValueBool(const char *key, bool fallback)
{
    auto map = getConfigMap();
    auto it = map.find(key);
    bool value = fallback;

    if (it != map.end())
    {
        if (it->second == "0" || it->second == "false")
            value = false;
        else if (it->second == "1" || it->second == "true")
            value = true;
    }

    return value;
}

static int getConfigValueInt(const char *key, int fallback)
{
    auto map = getConfigMap();
    auto it = map.find(key);
    int value = fallback;

    if (it != map.end())
        value = std::stoi(it->second);

    return value;
}

namespace config::options
{
    bool AllowProxyServer()
    {
        return getConfigValueBool("AllowProxyServer", false)
            || !getConfigValueBool("NoProxyServer", true);
    }

    int RemoteDebuggingPort()
    {
        return getConfigValueInt("RemoteDebuggingPort", 0);
    }
    bool DisableWebSecurity()
    {
        return getConfigValueBool("DisableWebSecurity", false);
    }
    bool IgnoreCertificateErrors()
    {
        return getConfigValueBool("IgnoreCertificateErrors", false);
    }

    bool OptimizeClient()
    {
        return getConfigValueBool("OptimizeClient", true);
    }
    bool SuperLowSpecMode()
    {
        return getConfigValueBool("SuperLowSpecMode", false);
    }
    bool SilentMode()
    {
        return getConfigValueBool("SilentMode", false);
    }
}