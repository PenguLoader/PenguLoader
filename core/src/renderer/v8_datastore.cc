#include "pengu.h"
#include "v8_wrapper.h"

#include <mutex>
#include <optional>
#include <string>

namespace
{
    std::mutex g_datastore_pending_mutex;
    std::mutex g_datastore_write_mutex;
    std::optional<std::string> g_datastore_pending_save;
    bool g_datastore_save_queued = false;
}

static void transform_data(void *data, size_t length)
{
    static const char key[] = "A5dgY6lz9fpG9kGNiH1mZ";
    const int key_length = sizeof(key) - 1;

    uint8_t *buffer = reinterpret_cast<uint8_t *>(data);
    
    for (size_t i = 0; i < length; i++)
    {
        buffer[i] ^= static_cast<uint8_t>(key[i % key_length]);
    }
}

static void load_datastore(cef_string_t *json)
{
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
    else
    {
        cef_string_from_ascii("{}", 2, json);
    }
}

static void save_datastore(std::string &json)
{
    auto path = config::datastore_path();

    transform_data(json.data(), json.size());
    file::write_file(path, json.data(), json.size());
}

static void drain_datastore_saves()
{
    for (;;)
    {
        std::optional<std::string> payload;

        {
            std::lock_guard<std::mutex> lock(g_datastore_pending_mutex);
            if (!g_datastore_pending_save.has_value())
            {
                g_datastore_save_queued = false;
                return;
            }

            payload.emplace(std::move(g_datastore_pending_save.value()));
            g_datastore_pending_save.reset();
        }

        std::lock_guard<std::mutex> lock(g_datastore_write_mutex);
        save_datastore(payload.value());
    }
}

static V8Value *v8_load_datastore(V8Value *const args[], int argc)
{
    cef_string_t json{};
    load_datastore(&json);

    auto ret = V8Value::string(&json);
    cef_string_clear(&json);

    return ret;
}

static V8Value *v8_save_datastore(V8Value *const args[], int argc)
{
    if (argc > 0 && args[0]->isString())
    {
        CefScopedStr json = args[0]->asString();

        if (!json.empty())
        {
            cef_string_utf8_t utf8{};
            cef_string_to_utf8(json.str, json.length, &utf8);

            bool should_queue = false;
            {
                std::lock_guard<std::mutex> lock(g_datastore_pending_mutex);
                g_datastore_pending_save.emplace(utf8.str, utf8.length);

                if (!g_datastore_save_queued)
                {
                    g_datastore_save_queued = true;
                    should_queue = true;
                }
            }

            if (should_queue)
                v8_async::enqueue(drain_datastore_saves);

            cef_string_utf8_clear(&utf8);
        }
    }

    return nullptr;
}

V8HandlerFunctionEntry v8_DataStoreEntries[]
{
    { "LoadDataStore", v8_load_datastore },
    { "SaveDataStore", v8_save_datastore },
    { nullptr }
};
