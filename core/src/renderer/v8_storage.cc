#include "pengu.h"
#include "v8_wrapper.h"
#include "../storage_key.h"

#include "sqlite3.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// =============================================================================
// `window.__native.Storage*` — the per-plugin key/value store.
//
// See docs/plugin-storage.md. Shape mirrors PluginFS: `StorageGrant` mints an
// unguessable token bound to one plugin and is then deleted from the native
// object by the preload, so after startup nothing can mint another. Every
// other entry point takes that token first.
//
// It is a bearer capability, with the same consequence as `context.fs`:
// handing it to imported code hands over that plugin's data. The blast radius
// is one plugin's store, and that containment rests on the filename hash being
// collision-resistant — see storage_key.h and section 5.2 of the design.
//
// Values are opaque. The renderer hands over whatever JSON.stringify produced
// and gets the same bytes back; nothing here parses or builds JSON.
//
// Threading: all database work runs on one dedicated thread, not V8AsyncPool.
// A single connection cannot serve three pool workers concurrently without
// SQLite serialising them anyway, and parking pool threads on that mutex would
// starve the other async APIs that share them. Section 10.
// =============================================================================

namespace
{
    constexpr int SCHEMA_VERSION = 1;

    struct Capability
    {
        path        db_path;
        std::string plugin;     // canonical entry path, for the meta table
    };

    std::mutex g_capabilities_mutex;
    std::unordered_map<std::string, Capability> g_capabilities;
    std::atomic<uint64_t> g_token_counter{ 1 };

    // -------------------------------------------------------------------------
    // Storage thread
    // -------------------------------------------------------------------------

    std::mutex                        g_queue_mutex;
    std::condition_variable           g_queue_cv;
    std::deque<std::function<void()>> g_queue;
    bool                              g_running = false;
    bool                              g_busy = false;

    /// How long the queue must stay empty before we fold the write-ahead log
    /// back into the database.
    constexpr auto IDLE_CHECKPOINT_AFTER = std::chrono::seconds(2);

    void checkpoint_idle_stores();

    void storage_loop()
    {
        for (;;)
        {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(g_queue_mutex);

                // Wait with a timeout rather than indefinitely: going idle is
                // the signal to checkpoint. `wal_autocheckpoint` is set high
                // because checkpointing on every transaction costs ~3.4x on
                // writes, but the price of that is a write-ahead log which
                // grows without bound -- measured at 64 MB against an 8 KB
                // database after a burst. This is the policy section 9.1 says
                // has to accompany the pragma.
                if (!g_queue_cv.wait_for(lock, IDLE_CHECKPOINT_AFTER,
                                         [] { return !g_queue.empty(); }))
                {
                    lock.unlock();
                    checkpoint_idle_stores();
                    continue;
                }

                job = std::move(g_queue.front());
                g_queue.pop_front();
                g_busy = true;
            }

            if (job) job();

            {
                std::lock_guard<std::mutex> lock(g_queue_mutex);
                g_busy = false;
            }
            g_queue_cv.notify_all();
        }
    }

    void post(std::function<void()> &&job)
    {
        {
            std::lock_guard<std::mutex> lock(g_queue_mutex);
            if (!g_running)
            {
                g_running = true;
                std::thread(storage_loop).detach();
            }
            g_queue.push_back(std::move(job));
        }
        g_queue_cv.notify_one();
    }

    // -------------------------------------------------------------------------
    // Connections. Only ever touched from the storage thread, so no lock.
    // -------------------------------------------------------------------------

    std::unordered_map<std::string, sqlite3 *> g_handles;

    sqlite3 *open_store(const Capability &capability)
    {
        const std::string key = capability.db_path.string();

        if (auto it = g_handles.find(key); it != g_handles.end())
            return it->second;

        std::error_code ec;
        std::filesystem::create_directories(config::storage_dir(), ec);

        auto utf8_path = capability.db_path.u8string();
        std::string utf8(utf8_path.begin(), utf8_path.end());

        sqlite3 *db = nullptr;
        if (sqlite3_open_v2(utf8.c_str(), &db,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        {
            if (db) sqlite3_close(db);
            g_handles[key] = nullptr;
            return nullptr;
        }

        // page_size must precede the first write to take effect at all.
        sqlite3_exec(db, "PRAGMA page_size=8192;", nullptr, nullptr, nullptr);

        // WAL can silently not engage — it needs shared memory, which network
        // filesystems do not provide, and the data root can be a redirected
        // UNC path on a roaming profile. Trust the pragma's return value, not
        // the fact that it did not error. Section 9.2.
        bool wal = false;
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(db, "PRAGMA journal_mode=WAL;", -1, &st, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(st) == SQLITE_ROW)
            {
                auto *mode = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
                wal = mode != nullptr && std::strcmp(mode, "wal") == 0;
            }
            sqlite3_finalize(st);
        }
        if (!wal)
            sqlite3_exec(db, "PRAGMA journal_mode=TRUNCATE;", nullptr, nullptr, nullptr);

        sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA busy_timeout=3000;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA wal_autocheckpoint=20000;", nullptr, nullptr, nullptr);

        sqlite3_exec(db,
            "CREATE TABLE IF NOT EXISTS kv ("
            "  k     TEXT PRIMARY KEY,"
            "  fmt   INTEGER NOT NULL,"
            "  v     BLOB NOT NULL,"
            "  mtime INTEGER NOT NULL"
            ") WITHOUT ROWID;"
            "CREATE TABLE IF NOT EXISTS meta ("
            "  k TEXT PRIMARY KEY,"
            "  v TEXT NOT NULL"
            ") WITHOUT ROWID;",
            nullptr, nullptr, nullptr);

        // The filename is a hash, so the file alone says nothing about who owns
        // it. Recording that inside beats a readable filename: it survives the
        // file being copied, renamed, or sent to someone for debugging, and it
        // is how the hub identifies orphaned stores. Section 5.3.
        if (sqlite3_prepare_v2(db,
                "INSERT OR IGNORE INTO meta(k,v) VALUES('plugin',?1),"
                "('created',?2),('schema',?3);", -1, &st, nullptr) == SQLITE_OK)
        {
            const auto created = std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            const auto schema = std::to_string(SCHEMA_VERSION);

            sqlite3_bind_text(st, 1, capability.plugin.data(),
                              static_cast<int>(capability.plugin.size()), SQLITE_STATIC);
            sqlite3_bind_text(st, 2, created.data(), static_cast<int>(created.size()), SQLITE_STATIC);
            sqlite3_bind_text(st, 3, schema.data(), static_cast<int>(schema.size()), SQLITE_STATIC);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }

        g_handles[key] = db;
        return db;
    }

    /// Fold each open store's write-ahead log back into its database and
    /// truncate it to nothing.
    ///
    /// TRUNCATE rather than PASSIVE: passive leaves the file at its high-water
    /// mark, which is the thing being fixed. Runs only on the storage thread,
    /// so touching g_handles needs no lock, and only when the queue is empty,
    /// so it never delays real work. A failure is uninteresting -- a reader
    /// holding the store open just means we try again in two seconds.
    void checkpoint_idle_stores()
    {
        for (auto &[key, db] : g_handles)
        {
            if (db != nullptr)
                sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE,
                                          nullptr, nullptr);
        }
    }

    std::optional<Capability> get_capability(const std::string &token)
    {
        std::lock_guard<std::mutex> lock(g_capabilities_mutex);
        auto it = g_capabilities.find(token);
        if (it == g_capabilities.end())
            return std::nullopt;
        return it->second;
    }

    std::string make_token()
    {
        static std::random_device rd;
        static std::mutex random_mutex;

        uint64_t first, second;
        {
            std::lock_guard<std::mutex> lock(random_mutex);
            first = (static_cast<uint64_t>(rd()) << 32) ^ rd();
            second = (static_cast<uint64_t>(rd()) << 32) ^ rd();
        }

        std::ostringstream stream;
        stream << std::hex << first << second << g_token_counter.fetch_add(1);
        return stream.str();
    }

    int64_t now_millis()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    /// Run `work` against the plugin's connection on the storage thread, then
    /// settle `task` with whatever it produced.
    ///
    /// A bad token settles rather than rejecting: the JS side cannot hold one
    /// it was not given, so this is unreachable in practice, and the
    /// "undefined / false" convention matches PluginFS.
    template <typename T>
    void run(V8PromiseTask *task, const std::string &token,
             std::function<T(sqlite3 *)> &&work,
             std::function<V8Value *(T)> &&settle, T fallback)
    {
        auto capability = get_capability(token);
        if (!capability.has_value())
        {
            task->resolve([settle, fallback]() -> V8Value * { return settle(fallback); });
            return;
        }

        post([task, capability, work = std::move(work), settle = std::move(settle), fallback] {
            sqlite3 *db = open_store(capability.value());
            T result = db != nullptr ? work(db) : fallback;
            task->resolve([settle, result]() -> V8Value * { return settle(result); });
        });
    }
}

// =============================================================================
// V8 bindings
// =============================================================================

static V8Value *v8_storage_version(V8Value *const args[], int argc)
{
    const char *v = sqlite3_libversion();
    CefStr version(v, std::strlen(v));
    return V8Value::string(&version);
}

/// Mint a capability for one folder plugin.
///
/// Validation mirrors PluginFSGrant: exactly `<plugin>` or `@<author>/<plugin>`,
/// a real directory containing index.js, no symlinks on the way down. Anything
/// deeper or shallower would let a grant straddle plugins or target the root.
static V8Value *v8_storage_grant(V8Value *const args[], int argc)
{
    if (argc < 1 || !args[0]->isString())
        return V8Value::undefined();

    CefScopedStr raw = args[0]->asString();
    std::string plugin_root = raw.to_utf8();

    std::replace(plugin_root.begin(), plugin_root.end(), '\\', '/');
    while (plugin_root.starts_with("./"))
        plugin_root.erase(0, 2);

    if (plugin_root.empty() || plugin_root.size() > 4096)
        return V8Value::undefined();

    // Split and sanity-check the segments.
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= plugin_root.size())
    {
        size_t end = plugin_root.find('/', start);
        std::string part = plugin_root.substr(
            start, end == std::string::npos ? std::string::npos : end - start);

        if (part.empty() || part == "." || part == ".." || part.size() > 255)
            return V8Value::undefined();
        if (part.find(':') != std::string::npos || part.find('\0') != std::string::npos)
            return V8Value::undefined();

        parts.push_back(std::move(part));
        if (end == std::string::npos) break;
        start = end + 1;
    }

    if (parts.size() != 1 && parts.size() != 2)
        return V8Value::undefined();
    if (parts.size() == 2 && !parts.front().starts_with("@"))
        return V8Value::undefined();

    std::error_code ec;
    auto plugins_root = std::filesystem::weakly_canonical(config::plugins_dir(), ec);
    if (ec || !file::is_dir(plugins_root))
        return V8Value::undefined();

    path root = plugins_root;
    for (const auto &part : parts)
    {
        root /= path(std::u8string(part.begin(), part.end()));
        if (file::is_symlink(root))
            return V8Value::undefined();
    }

    // A grant is only ever issued for something the loader recognises as a
    // folder plugin.
    auto canonical_root = std::filesystem::weakly_canonical(root, ec);
    if (ec || !file::is_dir(canonical_root) || !file::is_file(canonical_root / "index.js"))
        return V8Value::undefined();

    // The identity is the *entry path*, matching what disabled_plugins hashes.
    const std::string identity = storage::canonical(plugin_root + "/index.js");
    const std::string filename = storage::db_name(plugin_root + "/index.js");

    auto token = make_token();
    {
        std::lock_guard<std::mutex> lock(g_capabilities_mutex);
        g_capabilities[token] = Capability{ config::storage_dir() / filename, identity };
    }

    CefStr value(token);
    return V8Value::string(&value);
}

/// Two strings out of one statement, or nothing. Values are returned as text;
/// binary values are refused at the setter, so `fmt` is always 0 for now.
static V8Value *v8_storage_get(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
    {
        task->resolve([] { return V8Value::undefined(); });
        return promise;
    }

    std::string token = CefScopedStr(args[0]->asString()).to_utf8();
    std::string key = CefScopedStr(args[1]->asString()).to_utf8();

    run<std::optional<std::string>>(task, token,
        [key](sqlite3 *db) -> std::optional<std::string> {
            sqlite3_stmt *st = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT v FROM kv WHERE k=?;", -1, &st, nullptr) != SQLITE_OK)
                return std::nullopt;

            sqlite3_bind_text(st, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);

            std::optional<std::string> out;
            if (sqlite3_step(st) == SQLITE_ROW)
            {
                auto *p = static_cast<const char *>(sqlite3_column_blob(st, 0));
                if (p != nullptr)
                    out.emplace(p, sqlite3_column_bytes(st, 0));
            }
            sqlite3_finalize(st);
            return out;
        },
        [](std::optional<std::string> value) -> V8Value * {
            if (!value.has_value())
                return V8Value::undefined();
            CefStr text(value.value());
            return V8Value::string(&text);
        },
        std::nullopt);

    return promise;
}

static V8Value *v8_storage_set(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    if (argc < 3 || !args[0]->isString() || !args[1]->isString() || !args[2]->isString())
    {
        task->resolve([] { return V8Value::boolean(false); });
        return promise;
    }

    std::string token = CefScopedStr(args[0]->asString()).to_utf8();
    std::string key = CefScopedStr(args[1]->asString()).to_utf8();
    std::string value = CefScopedStr(args[2]->asString()).to_utf8();

    run<bool>(task, token,
        [key, value](sqlite3 *db) -> bool {
            sqlite3_stmt *st = nullptr;
            if (sqlite3_prepare_v2(db,
                    "INSERT INTO kv(k,fmt,v,mtime) VALUES(?,0,?,?)"
                    " ON CONFLICT(k) DO UPDATE SET fmt=0, v=excluded.v, mtime=excluded.mtime;",
                    -1, &st, nullptr) != SQLITE_OK)
                return false;

            sqlite3_bind_text(st, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
            sqlite3_bind_blob(st, 2, value.data(), static_cast<int>(value.size()), SQLITE_STATIC);
            sqlite3_bind_int64(st, 3, now_millis());

            const bool ok = sqlite3_step(st) == SQLITE_DONE;
            sqlite3_finalize(st);
            return ok;
        },
        [](bool ok) -> V8Value * { return V8Value::boolean(ok); },
        false);

    return promise;
}

static V8Value *v8_storage_has(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
    {
        task->resolve([] { return V8Value::boolean(false); });
        return promise;
    }

    std::string token = CefScopedStr(args[0]->asString()).to_utf8();
    std::string key = CefScopedStr(args[1]->asString()).to_utf8();

    run<bool>(task, token,
        [key](sqlite3 *db) -> bool {
            sqlite3_stmt *st = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT 1 FROM kv WHERE k=?;", -1, &st, nullptr) != SQLITE_OK)
                return false;

            sqlite3_bind_text(st, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
            const bool found = sqlite3_step(st) == SQLITE_ROW;
            sqlite3_finalize(st);
            return found;
        },
        [](bool found) -> V8Value * { return V8Value::boolean(found); },
        false);

    return promise;
}

static V8Value *v8_storage_delete(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
    {
        task->resolve([] { return V8Value::boolean(false); });
        return promise;
    }

    std::string token = CefScopedStr(args[0]->asString()).to_utf8();
    std::string key = CefScopedStr(args[1]->asString()).to_utf8();

    run<bool>(task, token,
        [key](sqlite3 *db) -> bool {
            sqlite3_stmt *st = nullptr;
            if (sqlite3_prepare_v2(db, "DELETE FROM kv WHERE k=?;", -1, &st, nullptr) != SQLITE_OK)
                return false;

            sqlite3_bind_text(st, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
            sqlite3_step(st);
            sqlite3_finalize(st);

            // "did something go" rather than "did the statement run".
            return sqlite3_changes(db) > 0;
        },
        [](bool removed) -> V8Value * { return V8Value::boolean(removed); },
        false);

    return promise;
}

static V8Value *v8_storage_keys(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    if (argc < 1 || !args[0]->isString())
    {
        task->resolve([] { return V8Value::undefined(); });
        return promise;
    }

    std::string token = CefScopedStr(args[0]->asString()).to_utf8();

    run<std::vector<std::string>>(task, token,
        [](sqlite3 *db) -> std::vector<std::string> {
            std::vector<std::string> keys;
            sqlite3_stmt *st = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT k FROM kv ORDER BY k;", -1, &st, nullptr) == SQLITE_OK)
            {
                while (sqlite3_step(st) == SQLITE_ROW)
                {
                    auto *k = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
                    if (k != nullptr)
                        keys.emplace_back(k, sqlite3_column_bytes(st, 0));
                }
                sqlite3_finalize(st);
            }
            return keys;
        },
        [](std::vector<std::string> keys) -> V8Value * {
            auto array = V8Array::create(static_cast<int>(keys.size()));
            for (int i = 0; i < static_cast<int>(keys.size()); ++i)
            {
                CefStr value(keys[i]);
                array->set(i, V8Value::string(&value));
            }
            return reinterpret_cast<V8Value *>(array);
        },
        {});

    return promise;
}

static V8Value *v8_storage_clear(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    if (argc < 1 || !args[0]->isString())
    {
        task->resolve([] { return V8Value::number(0); });
        return promise;
    }

    std::string token = CefScopedStr(args[0]->asString()).to_utf8();

    run<int>(task, token,
        [](sqlite3 *db) -> int {
            // meta is deliberately untouched: clearing a plugin's data should
            // not erase the record of whose data it was.
            sqlite3_exec(db, "DELETE FROM kv;", nullptr, nullptr, nullptr);
            return sqlite3_changes(db);
        },
        [](int removed) -> V8Value * { return V8Value::number(removed); },
        0);

    return promise;
}

/// Bytes the store occupies on disk, WAL included — what a quota would count.
static V8Value *v8_storage_size(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    if (argc < 1 || !args[0]->isString())
    {
        task->resolve([] { return V8Value::number(0); });
        return promise;
    }

    std::string token = CefScopedStr(args[0]->asString()).to_utf8();
    auto capability = get_capability(token);
    if (!capability.has_value())
    {
        task->resolve([] { return V8Value::number(0); });
        return promise;
    }

    const path db_path = capability->db_path;

    post([task, db_path] {
        std::error_code ec;
        uintmax_t total = 0;

        for (const char *suffix : { "", "-wal", "-shm" })
        {
            path p = db_path;
            p += suffix;
            auto size = std::filesystem::file_size(p, ec);
            if (!ec) total += size;
        }

        task->resolve([total]() -> V8Value * {
            return V8Value::number(static_cast<double>(total));
        });
    });

    return promise;
}

V8HandlerFunctionEntry v8_StorageEntries[]
{
    { "StorageVersion", v8_storage_version },
    { "StorageGrant",   v8_storage_grant   },
    { "StorageGet",     v8_storage_get     },
    { "StorageSet",     v8_storage_set     },
    { "StorageHas",     v8_storage_has     },
    { "StorageDelete",  v8_storage_delete  },
    { "StorageKeys",    v8_storage_keys    },
    { "StorageClear",   v8_storage_clear   },
    { "StorageSize",    v8_storage_size    },
    { nullptr }
};
