#include "commons.h"
#include <fstream>
#include <unordered_map>

#if OS_WIN
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#elif OS_MAC
#include <dlfcn.h>
#include <libgen.h>
#endif

path config::loader_dir()
{
#if OS_WIN
    static std::wstring path;
    if (path.empty())
    {
        // Get this dll path.
        WCHAR thisPath[2048];
        GetModuleFileNameW((HINSTANCE)&__ImageBase, thisPath, ARRAYSIZE(thisPath) - 1);

        DWORD attr = GetFileAttributesW(thisPath);
        if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) != FILE_ATTRIBUTE_REPARSE_POINT)
        {
            path = thisPath;
            return path = path.substr(0, path.find_last_of(L"/\\"));
        }

        OFSTRUCT of{};
        WCHAR finalPath[2048];
        // Get final path.
        HANDLE file = CreateFileW(thisPath, GENERIC_READ, 0x1, NULL, OPEN_EXISTING, 0, NULL);
        DWORD pathLength = GetFinalPathNameByHandleW(file, finalPath, 2048, FILE_NAME_OPENED);
        CloseHandle(file);

        std::wstring dir{ finalPath, pathLength };
        // Remove prepended '\\?\' by GetFinalPathNameByHandle()
        if (dir.rfind(L"\\\\?\\", 0) == 0)
            dir.erase(0, 4);

        // Get parent folder.
        return path = dir.substr(0, dir.find_last_of(L"/\\"));
    }
#elif OS_MAC
    static std::string path;
    if (path.empty())
    {
        Dl_info info;
        if (dladdr((const void *)&loader_dir, &info))
        {
            path = info.dli_fname;
            path = path.substr(0, path.rfind('/'));
        }
    }
#endif
    return path;
}

path config::plugins_dir()
{
    return loader_dir() / "plugins";
}

path config::datastore_path()
{
    return loader_dir() / "datastore";
}

path config::cache_dir()
{
#if OS_WIN
    wchar_t path[2048];
    size_t length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, _countof(path));

    if (length == 0)
        return league_dir() / "Cache";

    lstrcatW(path, L"\\Riot Games\\League of Legends\\Cache");
    return path;
#else
    // inside the RiotClient folder 
    return "/Users/Shared/Riot Games/League Client/Cache";
#endif
}

path config::league_dir()
{
#if OS_WIN
    wchar_t buf[2048];
    size_t length = GetModuleFileNameW(nullptr, buf, _countof(buf));

    std::wstring path(buf, length);
    return path.substr(0, path.find_last_of(L"/\\"));
#else
    return "";
#endif
}

static auto get_config_map()
{
    static bool cached = false;
    static std::unordered_map<std::string, std::string> map;

    if (!cached)
    {
        auto path = config::loader_dir() / "config";
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

static std::string get_config_value(const char *key, const char *fallback)
{
    auto map = get_config_map();
    auto it = map.find(key);
    std::string value = fallback;

    if (it != map.end())
        value = it->second;

    return value;
}

static bool get_config_value_bool(const char *key, bool fallback)
{
    auto map = get_config_map();
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

static int get_config_value_int(const char *key, int fallback)
{
    auto map = get_config_map();
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
        return get_config_value_bool("AllowProxyServer", false) || !get_config_value_bool("NoProxyServer", true);
    }

    int RemoteDebuggingPort()
    {
        return get_config_value_int("RemoteDebuggingPort", 0);
    }
    bool DisableWebSecurity()
    {
        return get_config_value_bool("DisableWebSecurity", false);
    }
    bool IgnoreCertificateErrors()
    {
        return get_config_value_bool("IgnoreCertificateErrors", false);
    }

    bool OptimizeClient()
    {
        return get_config_value_bool("OptimizeClient", true);
    }
    bool SuperLowSpecMode()
    {
        return get_config_value_bool("SuperLowSpecMode", false);
    }
}