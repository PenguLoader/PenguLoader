#include "pengu.h"
#include "v8_wrapper.h"
#include <thread>
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

struct DataStoreTask : CefRefCount<cef_task_t>
{
    V8Promise *promise;
    cef_string_t *json;
    cef_v8context_t *context;

    DataStoreTask() : CefRefCount(this), json(nullptr)
    {
        context = cef_v8context_get_current_context();
        context->base.add_ref(&context->base);

        context->enter(context);
        {
            promise = V8Promise::create();
            promise->addRef();
        }
        context->exit(context);

        cef_bind_method(DataStoreTask, execute);
    }

    ~DataStoreTask()
    {
        if (json != nullptr)
        {
            cef_string_clear(json);
            delete json;
        }

        context->base.release(&context->base);
    }

    void CEF_CALLBACK _execute()
    {
        context->enter(context);
        {
            V8Value *value = nullptr;
            if (json != nullptr)
                value = V8Value::string(json);

            promise->resolve(value);
            promise->release();
        }
        context->exit(context);
    }
};

static V8Value *v8_load_datastore(V8Value *const args[], int argc)
{
    auto task = new DataStoreTask();
    std::thread runner([task]
        {
            task->json = new cef_string_t{};
            load_datastore(task->json);

            cef_post_task(TID_RENDERER, task);
        });

    runner.detach();
    return (V8Value *)task->promise;
}

static V8Value *v8_save_datastore(V8Value *const args[], int argc)
{
    if (argc > 0 && args[0]->isString())
    {
        CefScopedStr json = args[0]->asString();
        auto task = new DataStoreTask();

        auto utf8 = new cef_string_utf8_t{};
        if (!json.empty())
            cef_string_to_utf8(json.str, json.length, utf8);

        std::thread runner([task, utf8]
            {
                save_datastore(utf8);
                cef_string_utf8_clear(utf8);

                cef_post_task(TID_RENDERER, task);
            });

        runner.detach();
        return (V8Value *)task->promise;
    }

    return nullptr;
}

V8HandlerFunctionEntry v8_DataStoreEntries[]
{
    { "LoadDataStore", v8_load_datastore },
    { "SaveDataStore", v8_save_datastore },
    { nullptr }
};