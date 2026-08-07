#include "pengu.h"
#include "config_parse.h"
#include <fstream>
#include <unordered_map>

#if OS_WIN
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#elif OS_MAC
#include <dlfcn.h>
#include <libgen.h>
#endif

path config::known_data_dir()
{
#if OS_WIN
    // Mirrors the host's WindowsHost.DataRoot: %LOCALAPPDATA%\.pengu.
    //
    // Per-user, and resolved from the environment of the process we're loaded
    // into — which is LCUX, running as whoever launched the client. So each
    // user's client reads that user's plugins, even though Universal mode's
    // IFEO key is machine-wide.
    //
    // This was %PROGRAMDATA%\.pengu, shared across accounts. Plugins are code
    // executed by the launching user, so a shared, everyone-writable plugins
    // folder let any account run code in any other account's client.
    //
    // GetEnvironmentVariable rather than SHGetKnownFolderPath so we don't pull
    // shell32 into the renderer.
    static std::wstring cached;
    if (cached.empty())
    {
        wchar_t buf[2048];
        size_t length = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, _countof(buf));
        if (length == 0)
            return {}; // no LOCALAPPDATA -> caller falls back to module_dir
        cached = buf;
        cached += L"\\.pengu";
    }
    return cached;
#elif OS_MAC
    // macOS doesn't have a ProgramData equivalent for "machine-wide,
    // user-writable" state; OnDemand is the only mode and is naturally
    // per-user (each user runs their own hub). Per-user data root is
    // correct here.
    static std::string cached;
    if (cached.empty())
    {
        const char *home = getenv("HOME");
        if (home == nullptr || *home == '\0')
            return {};
        cached = home;
        cached += "/Library/Application Support/Pengu";
    }
    return cached;
#else
    return {};
#endif
}

path config::module_dir()
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
        if (dladdr((const void *)&module_dir, &info))
        {
            path = info.dli_fname;
            path = path.substr(0, path.rfind('/'));
        }
    }
#endif
    return path;
}

path config::loader_dir()
{
    // Try the user data dir first; fall back to the module's directory for
    // legacy installs (v1.1.6 + the discarded Tauri v1.2.0 layout) where
    // config/datastore/plugins lived next to the loader exe.
    //
    // Probe with `config` because that's the file the host always creates on
    // first interaction; if it's there, app/'s migration has run (or the user
    // is actively using the new host) and data_root is canonical.
    //
    // OnDemand on Windows lands the running module at <LoL>\dwrite.dll where
    // there's no config — so without this prefer-data_root rule, the core
    // would resolve loader_dir to the LoL folder and find nothing. After
    // app/ has run once, data_root has config and we get it right.
    static path resolved;
    if (resolved.empty())
    {
        path data = known_data_dir();
        if (!data.empty() && std::filesystem::exists(data / "config"))
            resolved = data;
        else
            resolved = module_dir();
    }
    return resolved;
}

path config::datastore_path()
{
    return loader_dir() / "datastore";
}

path config::storage_dir()
{
    // Outside the plugins folder on purpose: uninstall deletes a plugin's
    // directory, and `context.fs` is scoped to it, so a store living there
    // would be destroyed by a reinstall and reachable by a leaked capability.
    // See docs/plugin-storage.md section 5.
    return loader_dir() / "storage";
}

path config::datastore_db_path()
{
    // Deliberately a sibling of the legacy blob rather than a replacement for
    // it. The old file stays readable after migration, so a user who downgrades
    // gets their data back instead of an empty store.
    return loader_dir() / "datastore.db";
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

static const std::unordered_map<std::string, std::string> &get_config_map()
{
    // Returned by reference so each reader (we have many — one per option
    // getter) doesn't copy the whole map on every call.
    static bool cached = false;
    static std::unordered_map<std::string, std::string> map;

    if (!cached)
    {
        auto path = config::loader_dir() / "config";
        std::ifstream file(path);

        if (file.is_open())
        {
            std::string line, key, value;
            while (std::getline(file, line))
            {
                if (config::parse::ini_line(line, key, value))
                    map[key] = value;
            }
            file.close();
        }

        cached = true;
    }

    return map;
}

static std::string get_config_value(const char *key, const char *fallback)
{
    const auto &map = get_config_map();
    auto it = map.find(key);
    return it != map.end() ? it->second : std::string(fallback);
}

static bool get_config_value_bool(const char *key, bool fallback)
{
    const auto &map = get_config_map();
    auto it = map.find(key);
    if (it == map.end()) return fallback;

    return config::parse::as_bool(it->second, fallback);
}

static int get_config_value_int(const char *key, int fallback)
{
    const auto &map = get_config_map();
    auto it = map.find(key);
    if (it == map.end()) return fallback;

    return config::parse::as_int(it->second, fallback);
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

    bool optimized_client()
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

    bool insecure_mode()
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

    bool auto_update_check()
    {
        return get_config_value_bool(__func__, true);
    }

    bool use_transparency()
    {
        return get_config_value_bool(__func__, true);
    }

    bool use_decorations()
    {
        return get_config_value_bool(__func__, true);
    }

    bool use_logging()
    {
        return get_config_value_bool(__func__, true);
    }

    int debug_port()
    {
        return get_config_value_int(__func__, 0);
    }
}