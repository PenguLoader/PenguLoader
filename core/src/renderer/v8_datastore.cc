#include "pengu.h"
#include "v8_wrapper.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

// =============================================================================
// DataStore — on-disk JSON, XOR'd, persisted asynchronously.
//
// Reads (Load) run on the renderer's worker pool and return a Promise<string>.
// Writes (Save) are fire-and-forget: the renderer hands us a JSON blob, we
// stash it in a single-slot "pending" buffer and signal the writer thread.
// Multiple saves coalesce — only the most recent blob ever hits disk.
// Flush waits until the writer is idle (no pending blob + no in-flight write).
// =============================================================================

static void transform_data(void *data, size_t length)
{
    static const char key[] = "A5dgY6lz9fpG9kGNiH1mZ";
    const int key_length = sizeof(key) - 1;

    uint8_t *buffer = reinterpret_cast<uint8_t *>(data);
    for (size_t i = 0; i < length; i++)
        buffer[i] ^= static_cast<uint8_t>(key[i % key_length]);
}

// =============================================================================
// Writer thread — one persistent worker, single-slot pending buffer.
// =============================================================================

namespace ds_writer
{
    static std::mutex                mu;
    static std::condition_variable   cv;
    static std::string               pending;     // latest blob waiting to be written
    static bool                      has_pending = false;
    static bool                      writing = false; // a write is in flight on this thread
    static bool                      started = false;

    static void worker_loop()
    {
        for (;;)
        {
            std::string snapshot;
            {
                std::unique_lock<std::mutex> lock(mu);
                cv.wait(lock, [] { return has_pending; });
                snapshot = std::move(pending);
                has_pending = false;
                writing = true;
            }

            // XOR + write outside the lock so disk I/O doesn't block enqueue
            // of newer blobs (which will overwrite `pending` for next loop).
            transform_data(snapshot.data(), snapshot.size());

            // Atomic, not a plain truncating write: this file is the whole
            // store, and a crash part-way through rewriting it would take
            // every plugin's settings with it. There is nowhere to report a
            // failure from this thread -- the JS side already treats Save as
            // fire-and-forget -- but at least a failed write now leaves the
            // previous contents readable instead of a truncated file.
            file::atomic_write(
                config::datastore_path(),
                snapshot.data(),
                snapshot.size());

            {
                std::lock_guard<std::mutex> lock(mu);
                writing = false;
            }
            cv.notify_all(); // wake any flush() waiters
        }
    }

    static void ensure_started()
    {
        std::lock_guard<std::mutex> lock(mu);
        if (started) return;
        started = true;
        std::thread(worker_loop).detach();
    }

    void enqueue(std::string &&blob)
    {
        ensure_started();
        {
            std::lock_guard<std::mutex> lock(mu);
            pending = std::move(blob); // latest-wins: overwrite any unprocessed prior
            has_pending = true;
        }
        cv.notify_one();
    }

    /// Block until the writer has no pending blob and no in-flight write.
    void wait_idle()
    {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [] { return !has_pending && !writing; });
    }
}

// =============================================================================
// V8 bindings
// =============================================================================

static V8Value *v8_load_datastore(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task] {
        // Heap-allocate the string struct so the renderer-side resolver can
        // own it across the thread hop without lifetime games.
        auto *json = new cef_string_t{};
        auto path = config::datastore_path();

        if (file::is_file(path))
        {
            void *buffer; size_t length;
            if (file::read_file(path, &buffer, &length))
            {
                transform_data(buffer, length);
                cef_string_from_utf8((char *)buffer, length, json);
                free(buffer);
            }
        }

        if (json->length == 0)
            cef_string_from_ascii("{}", 2, json);

        task->resolve([json]() -> V8Value * {
            V8Value *v = V8Value::string(json);
            cef_string_clear(json);
            delete json;
            return v;
        });
    });

    return promise;
}

static V8Value *v8_save_datastore(V8Value *const args[], int argc)
{
    if (argc > 0 && args[0]->isString())
    {
        CefScopedStr js = args[0]->asString();
        if (!js.empty())
        {
            // Convert UTF-16 → UTF-8 once on the renderer thread, then hand
            // the bytes off to the writer. Latest-wins coalescing happens in
            // ds_writer::enqueue.
            std::string utf8;
            js.to_utf8_into(utf8);
            ds_writer::enqueue(std::move(utf8));
        }
    }
    // Fire-and-forget — undefined to JS.
    return nullptr;
}

static V8Value *v8_flush_datastore(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    task->execute([task] {
        ds_writer::wait_idle();
        task->resolve();
    });

    return promise;
}

V8HandlerFunctionEntry v8_DataStoreEntries[]
{
    { "LoadDataStore",  v8_load_datastore  },
    { "SaveDataStore",  v8_save_datastore  },
    { "FlushDataStore", v8_flush_datastore },
    { nullptr }
};
