#include "pengu.h"
#include "v8_wrapper.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

// =============================================================================
// `window.__native.PluginFS*` — scoped filesystem for folder plugins.
// Adapted from PR #140/#141 by Ku-Tadao.
//
// Capability model, and how it differs from the rest of the runtime:
//
//   `PluginFSGrant(root)` mints an unguessable token bound to one plugin root
//   and is then DELETED from the native object by the preload, so after
//   startup nothing can mint another. Every other entry point takes that token
//   as its first argument.
//
//   This is a bearer capability, unlike `$write` and `?dir`, which derive
//   their target from the calling script's URL and cannot be delegated. The
//   difference is deliberate: a plugin's fs methods live in the shared preload
//   bundle, not in a per-module shim, so frame 0 would always be the preload
//   and caller identity would have to be inferred by walking the stack for the
//   first plugins-scheme frame — which misattributes the moment a plugin calls
//   fs from inside a callback another plugin invoked.
//
//   The cost of a bearer token is that it CAN be delegated: a plugin that
//   hands `context.fs` to imported remote code hands over that folder. That is
//   the plugin's choice to make, and the blast radius is its own directory.
//
// Path safety is enforced per component, never by string prefix alone:
// absolute paths, `.`, `..`, empty segments, `:`, NUL, over-long names and
// Windows reserved device names are all rejected before touching disk, every
// component is checked for symlinks, and the resolved path is re-verified to
// sit inside the capability root.
// =============================================================================

namespace
{
    constexpr size_t MAX_TEXT_BYTES = 16 * 1024 * 1024;

    struct Capability
    {
        path root;
        std::string plugin_root;
    };

    struct FileStatResult
    {
        std::string file_name;
        uintmax_t size;
        bool is_dir;
        bool is_file;
    };

    std::mutex g_capabilities_mutex;
    std::mutex g_write_mutex;
    std::unordered_map<std::string, Capability> g_capabilities;
    std::atomic<uint64_t> g_token_counter{ 1 };

    static std::string to_utf8(V8Value *value)
    {
        CefScopedStr str = value->asString();
        return str.to_utf8();
    }

    static bool is_windows_reserved_name(const std::string &component)
    {
#if OS_WIN
        if (component.empty())
            return true;

        // Trailing dots and spaces are silently stripped by Win32, so "a " and
        // "a" would resolve to the same file despite comparing differently.
        if (component.back() == ' ' || component.back() == '.')
            return true;

        for (unsigned char ch : component)
        {
            if (ch < 32)
                return true;

            switch (ch)
            {
            case '<': case '>': case ':': case '"': case '|': case '?': case '*':
                return true;
            default:
                break;
            }
        }

        auto base = component;
        auto dot = base.find('.');
        if (dot != std::string::npos)
            base = base.substr(0, dot);

        std::transform(base.begin(), base.end(), base.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });

        if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" ||
            base == "CONIN$" || base == "CONOUT$")
        {
            return true;
        }

        if (base.size() == 4 && (base.starts_with("COM") || base.starts_with("LPT")) &&
            base[3] >= '1' && base[3] <= '9')
        {
            return true;
        }
#else
        (void)component;
#endif
        return false;
    }

    static bool path_exists(const path &target)
    {
        std::error_code ec;
        return std::filesystem::exists(target, ec);
    }

    static path path_from_utf8_component(const std::string &component)
    {
        std::u8string utf8_component;
        utf8_component.reserve(component.size());

        for (unsigned char ch : component)
            utf8_component.push_back(static_cast<char8_t>(ch));

        return path(utf8_component);
    }

    static std::optional<std::vector<path>> split_relative_path(const std::string &input, bool allow_empty)
    {
        if (input.size() > 4096)
            return std::nullopt;

        if (input.empty())
        {
            if (allow_empty)
                return std::vector<path>{};
            return std::nullopt;
        }

        if (input[0] == '/' || input[0] == '\\')
            return std::nullopt;

        std::vector<path> parts;
        size_t start = 0;

        while (start <= input.size())
        {
            size_t end = input.find_first_of("/\\", start);
            std::string component = input.substr(start, end == std::string::npos ? std::string::npos : end - start);

            if (component.empty() || component == "." || component == ".." || component.size() > 255)
                return std::nullopt;

            if (component.find(':') != std::string::npos || component.find('\0') != std::string::npos)
                return std::nullopt;

            if (is_windows_reserved_name(component))
                return std::nullopt;

            parts.push_back(path_from_utf8_component(component));

            if (end == std::string::npos)
                break;
            start = end + 1;
        }

        return parts;
    }

    static std::string normalize_plugin_input(const std::string &input)
    {
        std::string normalized = input;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        while (normalized.starts_with("./"))
            normalized.erase(0, 2);

        return normalized;
    }

    static std::string path_to_utf8(const path &value)
    {
        auto utf8 = value.u8string();
        std::string output;
        output.reserve(utf8.size());

        for (auto ch : utf8)
            output.push_back(static_cast<char>(ch));

        return output;
    }

    static std::string make_token()
    {
        static std::random_device rd;
        static std::mutex random_mutex;

        uint64_t first;
        uint64_t second;

        {
            std::lock_guard<std::mutex> lock(random_mutex);
            first = (static_cast<uint64_t>(rd()) << 32) ^ rd();
            second = (static_cast<uint64_t>(rd()) << 32) ^ rd();
        }

        auto counter = g_token_counter.fetch_add(1);
        auto ticks = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

        std::ostringstream stream;
        stream << std::hex << first << second << counter << ticks;
        return stream.str();
    }

    static bool is_same_or_child_path(const path &base, const path &target)
    {
        std::error_code ec;
        auto base_normal = std::filesystem::weakly_canonical(base, ec);
        if (ec)
            return false;

        auto target_normal = std::filesystem::weakly_canonical(target, ec);
        if (ec)
            return false;

        auto relative = target_normal.lexically_relative(base_normal);
        if (relative.empty())
            return target_normal == base_normal;

        auto first = *relative.begin();
        return first != "..";
    }

    /// The plugin's own entry point. It is the ONLY file under the capability
    /// root that Pengu auto-executes at launch, which makes it the one target
    /// where a write turns a transient compromise -- a bad remote import, one
    /// malicious update -- into a permanent one. Writing and removing it are
    /// both refused; blocking only writes would leave remove-then-recreate.
    static bool is_entry_point(const Capability &capability, const path &target)
    {
        std::error_code ec;

        auto entry = std::filesystem::weakly_canonical(capability.root / "index.js", ec);
        if (ec)
            return false;

        auto candidate = std::filesystem::weakly_canonical(target, ec);
        if (ec)
            return false;

        return entry == candidate;
    }

    static std::optional<std::string> grant_capability(const std::string &plugin_root_input)
    {
        auto normalized = normalize_plugin_input(plugin_root_input);
        auto parts = split_relative_path(normalized, false);
        if (!parts.has_value())
            return std::nullopt;

        // Exactly `<plugin>` or `@<author>/<plugin>`. Anything deeper or
        // shallower would let a grant straddle plugins or target the root.
        if (parts->size() != 1 && parts->size() != 2)
            return std::nullopt;

        if (parts->size() == 2 && !parts->front().string().starts_with("@"))
            return std::nullopt;

        std::error_code ec;
        auto plugins_root = std::filesystem::weakly_canonical(config::plugins_dir(), ec);
        if (ec || !file::is_dir(plugins_root))
            return std::nullopt;

        path root = plugins_root;
        for (const auto &part : parts.value())
        {
            root /= part;
            if (file::is_symlink(root))
                return std::nullopt;
        }

        // index.js must exist: a grant is only ever issued for something the
        // loader actually recognises as a folder plugin.
        auto canonical_root = std::filesystem::weakly_canonical(root, ec);
        if (ec || !file::is_dir(canonical_root) || !file::is_file(canonical_root / "index.js"))
            return std::nullopt;

        if (!is_same_or_child_path(plugins_root, canonical_root))
            return std::nullopt;

        auto token = make_token();
        {
            std::lock_guard<std::mutex> lock(g_capabilities_mutex);
            g_capabilities[token] = Capability{ canonical_root, normalized };
        }

        return token;
    }

    static std::optional<Capability> get_capability(const std::string &token)
    {
        std::lock_guard<std::mutex> lock(g_capabilities_mutex);
        auto it = g_capabilities.find(token);
        if (it == g_capabilities.end())
            return std::nullopt;
        return it->second;
    }

    static bool ensure_root_is_available(const Capability &capability)
    {
        return file::is_dir(capability.root) && !file::is_symlink(capability.root);
    }

    static std::optional<path> resolve_existing_path(const Capability &capability, const std::string &relative_input, bool allow_root)
    {
        if (!ensure_root_is_available(capability))
            return std::nullopt;

        auto normalized = normalize_plugin_input(relative_input);
        auto parts = split_relative_path(normalized, allow_root);
        if (!parts.has_value())
            return std::nullopt;

        path current = capability.root;
        if (parts->empty())
            return current;

        for (const auto &part : parts.value())
        {
            current /= part;
            if (!path_exists(current) || file::is_symlink(current))
                return std::nullopt;
        }

        if (!is_same_or_child_path(capability.root, current))
            return std::nullopt;

        return current;
    }

    static std::optional<path> resolve_write_path(const Capability &capability, const std::string &relative_input)
    {
        if (!ensure_root_is_available(capability))
            return std::nullopt;

        auto normalized = normalize_plugin_input(relative_input);
        auto parts = split_relative_path(normalized, false);
        if (!parts.has_value() || parts->empty())
            return std::nullopt;

        path current = capability.root;
        for (size_t index = 0; index + 1 < parts->size(); ++index)
        {
            current /= parts.value()[index];
            if (!file::is_dir(current) || file::is_symlink(current))
                return std::nullopt;
        }

        auto target = current / parts->back();
        if (path_exists(target) && (file::is_symlink(target) || file::is_dir(target)))
            return std::nullopt;

        if (!is_same_or_child_path(capability.root, current))
            return std::nullopt;

        return target;
    }

    static std::optional<std::string> read_text(const std::string &token, const std::string &relative_path)
    {
        auto capability = get_capability(token);
        if (!capability.has_value())
            return std::nullopt;

        auto target = resolve_existing_path(capability.value(), relative_path, false);
        if (!target.has_value() || !file::is_file(target.value()))
            return std::nullopt;

        std::error_code ec;
        auto size = std::filesystem::file_size(target.value(), ec);
        if (ec || size > MAX_TEXT_BYTES)
            return std::nullopt;

        void *buffer = nullptr;
        size_t length = 0;
        if (!file::read_file(target.value(), &buffer, &length))
            return std::nullopt;

        std::string content(reinterpret_cast<char *>(buffer), length);
        free(buffer);
        return content;
    }

    static bool write_text(const std::string &token, const std::string &relative_path, const std::string &content, bool append)
    {
        if (content.size() > MAX_TEXT_BYTES)
            return false;

        auto capability = get_capability(token);
        if (!capability.has_value())
            return false;

        auto target = resolve_write_path(capability.value(), relative_path);
        if (!target.has_value())
            return false;

        if (is_entry_point(capability.value(), target.value()))
            return false;

        std::lock_guard<std::mutex> write_lock(g_write_mutex);

        if (append)
        {
            std::ofstream stream(target.value(), std::ios::binary | std::ios::app);
            if (!stream.good())
                return false;

            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
            return stream.good();
        }

        // Replacement writes go through a temp beside the target, so a crash
        // mid-write leaves the original intact.
        return file::atomic_write(target.value(), content.data(), content.size());
    }

    static bool make_dir(const std::string &token, const std::string &relative_path)
    {
        auto capability = get_capability(token);
        if (!capability.has_value() || !ensure_root_is_available(capability.value()))
            return false;

        auto normalized = normalize_plugin_input(relative_path);
        auto parts = split_relative_path(normalized, false);
        if (!parts.has_value() || parts->empty())
            return false;

        path current = capability->root;

        for (const auto &part : parts.value())
        {
            current /= part;
            if (path_exists(current))
            {
                if (!file::is_dir(current) || file::is_symlink(current))
                    return false;
                continue;
            }

            std::error_code ec;
            if (!std::filesystem::create_directory(current, ec) || ec)
                return false;
        }

        // Idempotent: success means "the directory now exists inside the
        // root", not "this call is what created it". Reporting false for an
        // existing directory reads as an error at the call site.
        return file::is_dir(current) && is_same_or_child_path(capability->root, current);
    }

    static std::optional<FileStatResult> stat_path(const std::string &token, const std::string &relative_path)
    {
        auto capability = get_capability(token);
        if (!capability.has_value())
            return std::nullopt;

        auto target = resolve_existing_path(capability.value(), relative_path, true);
        if (!target.has_value())
            return std::nullopt;

        bool is_dir = file::is_dir(target.value());
        bool is_file = file::is_file(target.value());
        if (!is_dir && !is_file)
            return std::nullopt;

        uintmax_t size = 0;
        if (is_file)
        {
            std::error_code ec;
            size = std::filesystem::file_size(target.value(), ec);
            if (ec)
                size = 0;
        }

        return FileStatResult{ path_to_utf8(target->filename()), size, is_dir, is_file };
    }

    static std::optional<std::vector<std::string>> list_dir(const std::string &token, const std::string &relative_path)
    {
        auto capability = get_capability(token);
        if (!capability.has_value())
            return std::nullopt;

        auto target = resolve_existing_path(capability.value(), relative_path, true);
        if (!target.has_value() || !file::is_dir(target.value()))
            return std::nullopt;

        auto entries = file::read_dir(target.value());
        std::vector<std::string> names;

        for (const auto &entry : entries)
        {
            auto name = path_to_utf8(entry.filename());
            if (name == "." || name == "..")
                continue;

            auto full = target.value() / entry;
            if (file::is_symlink(full))
                continue;

            if (file::is_file(full) || file::is_dir(full))
                names.push_back(name);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    static uintmax_t remove_path(const std::string &token, const std::string &relative_path, bool recursive)
    {
        auto capability = get_capability(token);
        if (!capability.has_value())
            return 0;

        auto target = resolve_existing_path(capability.value(), relative_path, false);
        if (!target.has_value() || target.value() == capability->root)
            return 0;

        if (is_entry_point(capability.value(), target.value()))
            return 0;

        std::error_code ec;
        if (file::is_dir(target.value()))
        {
            if (recursive)
                return std::filesystem::remove_all(target.value(), ec);
            return std::filesystem::remove(target.value(), ec) ? 1 : 0;
        }

        if (file::is_file(target.value()))
            return std::filesystem::remove(target.value(), ec) ? 1 : 0;

        return 0;
    }

    /// Settle a promise immediately with a fixed value, without burning a
    /// worker slot. Used for argument-validation failures, which must still
    /// hand back a promise so the JS side can `await` uniformly.
    static V8Value *settled(std::function<V8Value *()> &&value)
    {
        auto *task = new V8PromiseTask();
        auto *promise = task->promise();
        task->resolve(std::move(value));
        return promise;
    }
}

static V8Value *v8_pluginfs_grant(V8Value *const args[], int argc)
{
    if (argc < 1 || !args[0]->isString())
        return V8Value::undefined();

    auto token = grant_capability(to_utf8(args[0]));
    if (!token.has_value())
        return V8Value::undefined();

    CefStr value(token.value());
    return V8Value::string(&value);
}

static V8Value *v8_pluginfs_read(V8Value *const args[], int argc)
{
    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
        return settled([] { return V8Value::undefined(); });

    auto token = to_utf8(args[0]);
    auto relative_path = to_utf8(args[1]);

    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task, token, relative_path] {
        auto content = read_text(token, relative_path);
        task->resolve([content]() -> V8Value * {
            if (!content.has_value())
                return V8Value::undefined();

            CefStr value(content.value());
            return V8Value::string(&value);
        });
    });

    return promise;
}

static V8Value *v8_pluginfs_write(V8Value *const args[], int argc)
{
    if (argc < 3 || !args[0]->isString() || !args[1]->isString() || !args[2]->isString())
        return settled([] { return V8Value::boolean(false); });

    auto token = to_utf8(args[0]);
    auto relative_path = to_utf8(args[1]);
    auto content = to_utf8(args[2]);
    bool append = argc > 3 && args[3]->isBool() && args[3]->asBool();

    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task, token, relative_path, content, append] {
        bool result = write_text(token, relative_path, content, append);
        task->resolve([result]() -> V8Value * { return V8Value::boolean(result); });
    });

    return promise;
}

static V8Value *v8_pluginfs_mkdir(V8Value *const args[], int argc)
{
    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
        return settled([] { return V8Value::boolean(false); });

    auto token = to_utf8(args[0]);
    auto relative_path = to_utf8(args[1]);

    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task, token, relative_path] {
        bool result = make_dir(token, relative_path);
        task->resolve([result]() -> V8Value * { return V8Value::boolean(result); });
    });

    return promise;
}

static V8Value *v8_pluginfs_stat(V8Value *const args[], int argc)
{
    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
        return settled([] { return V8Value::undefined(); });

    auto token = to_utf8(args[0]);
    auto relative_path = to_utf8(args[1]);

    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task, token, relative_path] {
        auto result = stat_path(token, relative_path);
        task->resolve([result]() -> V8Value * {
            if (!result.has_value())
                return V8Value::undefined();

            auto object = V8Object::create();
            auto name = CefStr(result->file_name);
            object->set(&u"fileName"_s, V8Value::string(&name), V8_PROPERTY_ATTRIBUTE_READONLY);
            object->set(&u"length"_s, V8Value::number(static_cast<double>(result->size)), V8_PROPERTY_ATTRIBUTE_READONLY);
            object->set(&u"isDir"_s, V8Value::boolean(result->is_dir), V8_PROPERTY_ATTRIBUTE_READONLY);
            object->set(&u"isFile"_s, V8Value::boolean(result->is_file), V8_PROPERTY_ATTRIBUTE_READONLY);
            return reinterpret_cast<V8Value *>(object);
        });
    });

    return promise;
}

static V8Value *v8_pluginfs_ls(V8Value *const args[], int argc)
{
    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
        return settled([] { return V8Value::undefined(); });

    auto token = to_utf8(args[0]);
    auto relative_path = to_utf8(args[1]);

    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task, token, relative_path] {
        auto entries = list_dir(token, relative_path);
        task->resolve([entries]() -> V8Value * {
            if (!entries.has_value())
                return V8Value::undefined();

            auto array = V8Array::create(static_cast<int>(entries->size()));
            for (int index = 0; index < static_cast<int>(entries->size()); ++index)
            {
                auto value = CefStr(entries.value()[index]);
                array->set(index, V8Value::string(&value));
            }

            return reinterpret_cast<V8Value *>(array);
        });
    });

    return promise;
}

static V8Value *v8_pluginfs_remove(V8Value *const args[], int argc)
{
    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
        return settled([] { return V8Value::number(0); });

    auto token = to_utf8(args[0]);
    auto relative_path = to_utf8(args[1]);
    bool recursive = argc > 2 && args[2]->isBool() && args[2]->asBool();

    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task, token, relative_path, recursive] {
        auto result = remove_path(token, relative_path, recursive);
        task->resolve([result]() -> V8Value * {
            return V8Value::number(static_cast<double>(result));
        });
    });

    return promise;
}

V8HandlerFunctionEntry v8_PluginFSEntries[]
{
    { "PluginFSGrant",  v8_pluginfs_grant  },
    { "PluginFSRead",   v8_pluginfs_read   },
    { "PluginFSWrite",  v8_pluginfs_write  },
    { "PluginFSMkdir",  v8_pluginfs_mkdir  },
    { "PluginFSStat",   v8_pluginfs_stat   },
    { "PluginFSLs",     v8_pluginfs_ls     },
    { "PluginFSRemove", v8_pluginfs_remove },
    { nullptr }
};
