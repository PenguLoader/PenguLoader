#include "pengu.h"
#include "v8_wrapper.h"
#include <mutex>

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

static void save_datastore(cef_string_utf8_t *json)
{
    static std::mutex _mutex{};
    std::lock_guard<std::mutex> _lock(_mutex);
    {
        auto path = config::datastore_path();

        transform_data(json->str, json->length);
        file::write_file(path, json->str, json->length);
    }
}

static V8Value *v8_load_datastore(V8Value *const args[], int argc)
{
    auto task = new V8PromiseTask();
    return task->execute([task]
        {
            auto json = new cef_string_t{};
            load_datastore(json);

            task->resolve([json]
                {
                    auto value = V8Value::string(json);
                    cef_string_clear(json);
                    return value;
                }
            );
        }
    );
}

static V8Value *v8_save_datastore(V8Value *const args[], int argc)
{
    cef_string_userfree_t json = args[0]->asString();

    auto task = new V8PromiseTask();
    return task->execute([task, json]
        {
            auto utf8 = new cef_string_utf8_t{};
            if (json && json->length)
                cef_string_to_utf8(json->str, json->length, utf8);

            save_datastore(utf8);
            task->resolve();

            cef_string_utf8_clear(utf8);
            cef_string_userfree_free(json);
        });
}

V8HandlerFunctionEntry v8_DataStoreEntries[]
{
    { "LoadDataStore", v8_load_datastore },
    { "SaveDataStore", v8_save_datastore },
    { nullptr }
};