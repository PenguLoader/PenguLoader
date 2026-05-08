#include "pengu.h"
#include "v8_wrapper.h"

#include <atomic>
#include <mutex>
#include <string>

namespace
{
    std::mutex g_datastore_write_mutex;
    std::atomic<uint64_t> g_datastore_save_sequence{ 0 };
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

static void save_datastore(std::string json)
{
    auto path = config::datastore_path();

    transform_data(json.data(), json.size());
    file::write_file(path, json.data(), json.size());
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

            std::string payload(utf8.str, utf8.length);
            auto sequence = g_datastore_save_sequence.fetch_add(1) + 1;
            v8_async::enqueue([sequence, payload = std::move(payload)]() mutable {
                std::lock_guard<std::mutex> lock(g_datastore_write_mutex);
                if (sequence != g_datastore_save_sequence.load())
                    return;

                save_datastore(std::move(payload));
            });

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
