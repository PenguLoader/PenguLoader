#include "pengu.h"
#include "v8_wrapper.h"

#include "sqlite3.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// =============================================================================
// DataStore — one row per key, in SQLite.
//
// The old implementation kept the whole store as one JSON document and
// rewrote the entire file on every commit, so writing one key cost the price
// of writing all of them: ~300 ms of native work at 100 MB, and that is before
// V8's JSON.stringify over the same data. See docs/plugin-storage.md section 2
// for the measurements.
//
// The JS side still holds an in-memory snapshot and still answers `get` and
// `has` synchronously -- Welcome.tsx and an unknown number of plugins depend
// on that, and it is the one virtue the old design had. What changed is the
// write path: `set` upserts a single row.
//
// Values are opaque here. The renderer hands us whatever JSON.stringify
// produced and gets the same bytes back; nothing in this file parses or
// constructs JSON. See docs/plugin-storage.md section 6.2.
// =============================================================================

namespace
{
    // A pending write, keyed so a burst of set() calls on one key collapses to
    // a single row write -- the same latest-wins coalescing the old writer had,
    // but per key instead of over the whole store.
    //   value present -> upsert
    //   nullopt       -> delete
    using Pending = std::map<std::string, std::optional<std::string>>;

    std::mutex              g_mutex;
    std::condition_variable g_cv;
    Pending                 g_pending;
    bool                    g_writing = false;
    bool                    g_started = false;

    // One handle, reached from two places: the writer thread below, and a pool
    // worker running the one-time load. Safe because sqlite3 is built
    // SQLITE_THREADSAFE=1 (serialized), which puts a mutex inside the
    // connection -- see core/vendor/sqlite/README.md.
    sqlite3    *g_db = nullptr;
    std::once_flag g_open_once;

    /// Open (creating if needed) and configure the database.
    ///
    /// Failure is not fatal and not reported: `g_db` stays null, reads come
    /// back empty and writes are dropped. That matches how this file has
    /// always behaved -- the JS side treats Save as fire-and-forget and there
    /// is no logging facility in core to report into.
    void open_db()
    {
        auto path = config::datastore_db_path().u8string();
        std::string utf8(path.begin(), path.end());

        if (sqlite3_open_v2(utf8.c_str(), &g_db,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        {
            if (g_db) { sqlite3_close(g_db); g_db = nullptr; }
            return;
        }

        // WAL can silently fail to engage -- it needs shared memory, which
        // network filesystems do not provide, and %LOCALAPPDATA% can be a
        // redirected UNC path on a roaming profile. The pragma's *return
        // value* is the only reliable signal, so ask for it and fall back
        // rather than assuming. See docs/plugin-storage.md section 9.2.
        bool wal = false;
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(g_db, "PRAGMA journal_mode=WAL;", -1, &st, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(st) == SQLITE_ROW)
            {
                auto *mode = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
                wal = mode != nullptr && std::string(mode) == "wal";
            }
            sqlite3_finalize(st);
        }
        if (!wal)
            sqlite3_exec(g_db, "PRAGMA journal_mode=TRUNCATE;", nullptr, nullptr, nullptr);

        sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(g_db, "PRAGMA busy_timeout=3000;", nullptr, nullptr, nullptr);
        sqlite3_exec(g_db, "PRAGMA wal_autocheckpoint=20000;", nullptr, nullptr, nullptr);

        // WITHOUT ROWID: for a pure key/value table this stores rows directly
        // in the primary-key B-tree, so a lookup is one descent with no index
        // hop. docs/plugin-storage.md section 4.
        sqlite3_exec(g_db,
            "CREATE TABLE IF NOT EXISTS kv ("
            "  k     TEXT PRIMARY KEY,"
            "  v     TEXT NOT NULL,"
            "  mtime INTEGER NOT NULL"
            ") WITHOUT ROWID;",
            nullptr, nullptr, nullptr);
    }

    sqlite3 *db()
    {
        std::call_once(g_open_once, open_db);
        return g_db;
    }

    int64_t now_millis()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    /// Drain `batch` in one transaction. One commit for the whole burst rather
    /// than one per key -- the difference between N fsyncs and one.
    void write_batch(const Pending &batch)
    {
        auto *handle = db();
        if (handle == nullptr || batch.empty())
            return;

        sqlite3_exec(handle, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);

        sqlite3_stmt *put = nullptr;
        sqlite3_stmt *del = nullptr;
        sqlite3_prepare_v2(handle,
            "INSERT INTO kv(k,v,mtime) VALUES(?,?,?)"
            " ON CONFLICT(k) DO UPDATE SET v=excluded.v, mtime=excluded.mtime;",
            -1, &put, nullptr);
        sqlite3_prepare_v2(handle, "DELETE FROM kv WHERE k=?;", -1, &del, nullptr);

        const int64_t stamp = now_millis();

        for (const auto &[key, value] : batch)
        {
            if (value.has_value())
            {
                if (put == nullptr) continue;
                sqlite3_reset(put);
                sqlite3_bind_text(put, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
                sqlite3_bind_text(put, 2, value->data(), static_cast<int>(value->size()), SQLITE_STATIC);
                sqlite3_bind_int64(put, 3, stamp);
                sqlite3_step(put);
            }
            else
            {
                if (del == nullptr) continue;
                sqlite3_reset(del);
                sqlite3_bind_text(del, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
                sqlite3_step(del);
            }
        }

        sqlite3_finalize(put);
        sqlite3_finalize(del);
        sqlite3_exec(handle, "COMMIT;", nullptr, nullptr, nullptr);
    }

    /// How long the queue must stay empty before folding the write-ahead log
    /// back into the database.
    constexpr auto IDLE_CHECKPOINT_AFTER = std::chrono::seconds(2);

    void worker_loop()
    {
        for (;;)
        {
            Pending batch;
            {
                std::unique_lock<std::mutex> lock(g_mutex);

                // Timed rather than indefinite: going idle is the signal to
                // checkpoint. wal_autocheckpoint is set high because
                // checkpointing every transaction costs ~3.4x on writes, and
                // the price of that is a write-ahead log that grows without
                // bound. TRUNCATE rather than PASSIVE because passive leaves
                // the file at its high-water mark, which is the problem.
                if (!g_cv.wait_for(lock, IDLE_CHECKPOINT_AFTER,
                                   [] { return !g_pending.empty(); }))
                {
                    lock.unlock();
                    if (g_db != nullptr)
                        sqlite3_wal_checkpoint_v2(g_db, nullptr, SQLITE_CHECKPOINT_TRUNCATE,
                                                  nullptr, nullptr);
                    continue;
                }

                batch.swap(g_pending);
                g_writing = true;
            }

            // Outside the lock so newer writes can queue while this one runs.
            write_batch(batch);

            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_writing = false;
            }
            g_cv.notify_all();  // wake flush() waiters
        }
    }

    void ensure_started()
    {
        if (g_started) return;
        g_started = true;
        std::thread(worker_loop).detach();
    }

    void enqueue(std::string &&key, std::optional<std::string> &&value)
    {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            ensure_started();
            g_pending[std::move(key)] = std::move(value);
        }
        g_cv.notify_one();
    }

    void wait_idle()
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait(lock, [] { return g_pending.empty() && !g_writing; });
    }

    /// The legacy XOR obfuscation. Not encryption -- the key is a literal in
    /// the binary. It exists so the file does not invite editing in Notepad.
    void transform_data(void *data, size_t length)
    {
        static const char key[] = "A5dgY6lz9fpG9kGNiH1mZ";
        const int key_length = sizeof(key) - 1;

        uint8_t *buffer = reinterpret_cast<uint8_t *>(data);
        for (size_t i = 0; i < length; i++)
            buffer[i] ^= static_cast<uint8_t>(key[i % key_length]);
    }
}

// =============================================================================
// V8 bindings
// =============================================================================

/// Every row, as a flat [k0, v0, k1, v1, ...] array of strings.
///
/// Flat rather than a JSON document because assembling one here would mean
/// escaping keys and concatenating value text in C++ -- constructing JSON in
/// native code, which section 6.2 of the design says not to do. It also fails
/// gracefully: a row that somehow holds junk affects that key alone, where a
/// single document would take the whole store down with it.
static V8Value *v8_load_datastore(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task] {
        auto *rows = new std::vector<std::string>();

        if (auto *handle = db())
        {
            sqlite3_stmt *st = nullptr;
            if (sqlite3_prepare_v2(handle, "SELECT k, v FROM kv;", -1, &st, nullptr) == SQLITE_OK)
            {
                while (sqlite3_step(st) == SQLITE_ROW)
                {
                    auto *k = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
                    auto *v = reinterpret_cast<const char *>(sqlite3_column_text(st, 1));
                    if (k == nullptr || v == nullptr)
                        continue;

                    rows->emplace_back(k, sqlite3_column_bytes(st, 0));
                    rows->emplace_back(v, sqlite3_column_bytes(st, 1));
                }
                sqlite3_finalize(st);
            }
        }

        task->resolve([rows]() -> V8Value * {
            auto array = V8Array::create(static_cast<int>(rows->size()));
            for (int i = 0; i < static_cast<int>(rows->size()); ++i)
            {
                CefStr value((*rows)[i]);
                array->set(i, V8Value::string(&value));
            }
            delete rows;
            return reinterpret_cast<V8Value *>(array);
        });
    });

    return promise;
}

/// The pre-SQLite store, decoded but not parsed. Returns an empty string when
/// there is nothing to migrate.
///
/// Parsing is the renderer's job: it already has the JSON.parse call, and
/// splitting a document into rows in C++ would mean a JSON parser here.
static V8Value *v8_load_legacy_datastore(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task] {
        auto *json = new std::string();
        auto path = config::datastore_path();

        if (file::is_file(path))
        {
            void *buffer = nullptr;
            size_t length = 0;
            if (file::read_file(path, &buffer, &length))
            {
                transform_data(buffer, length);
                json->assign(reinterpret_cast<char *>(buffer), length);
                free(buffer);
            }
        }

        task->resolve([json]() -> V8Value * {
            CefStr value(*json);
            delete json;
            return V8Value::string(&value);
        });
    });

    return promise;
}

/// Upsert one row. Fire-and-forget, coalesced per key by the writer.
static V8Value *v8_set_datastore(V8Value *const args[], int argc)
{
    if (argc < 2 || !args[0]->isString() || !args[1]->isString())
        return nullptr;

    CefScopedStr key = args[0]->asString();
    CefScopedStr value = args[1]->asString();

    std::string k, v;
    key.to_utf8_into(k);
    value.to_utf8_into(v);

    enqueue(std::move(k), std::move(v));
    return nullptr;
}

/// Delete one row. Fire-and-forget, coalesced per key by the writer.
static V8Value *v8_remove_datastore(V8Value *const args[], int argc)
{
    if (argc < 1 || !args[0]->isString())
        return nullptr;

    CefScopedStr key = args[0]->asString();

    std::string k;
    key.to_utf8_into(k);

    enqueue(std::move(k), std::nullopt);
    return nullptr;
}

static V8Value *v8_flush_datastore(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task] {
        wait_idle();
        task->resolve();
    });

    return promise;
}

V8HandlerFunctionEntry v8_DataStoreEntries[]
{
    { "LoadDataStore",       v8_load_datastore        },
    { "LoadLegacyDataStore", v8_load_legacy_datastore },
    { "SetDataStore",        v8_set_datastore         },
    { "RemoveDataStore",     v8_remove_datastore      },
    { "FlushDataStore",      v8_flush_datastore       },
    { nullptr }
};
