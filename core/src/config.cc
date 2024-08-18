#include "pengu.h"
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

static void trim_tring(std::string &str)
{
    str.erase(str.find_last_not_of(' ') + 1);
    str.erase(0, str.find_first_not_of(' '));
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
                // ignore empty line or comment
                if (line.empty() || line[0] == ';' || line[0] == '#')
                    continue;

                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);

                    trim_tring(key);
                    trim_tring(value);

                    map[key] = value;
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

path config::plugins_dir()
{
    std::string cpath = get_config_value(__func__, "");
    if (!cpath.empty())
        return (const char8_t *)cpath.c_str();

    return loader_dir() / "plugins";
}

std::string config::disabled_plugins()
{
    return get_config_value(__func__, "");
}

namespace config::options
{
    bool use_hotkeys()
    {
        return get_config_value_bool(__func__, true);
    }

    bool optimed_client()
    {
        return get_config_value_bool(__func__, true);
    }

    bool super_potato()
    {
        return get_config_value_bool(__func__, false);
    }

    bool silent_mode()
    {
        return get_config_value_bool(__func__, false);
    }

    bool isecure_mode()
    {
        return get_config_value_bool(__func__, false);
    }

    bool use_devtools()
    {
        return get_config_value_bool(__func__, false);
    }

    bool use_riotclient()
    {
        return get_config_value_bool(__func__, false);
    }

    bool use_proxy()
    {
        return get_config_value_bool(__func__, false);
    }

    int debug_port()
    {
        return get_config_value_int(__func__, 0);
    }
}