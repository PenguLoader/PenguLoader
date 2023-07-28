#include "commons.h"
#include <fstream>

static void TransformData(vec<char> &data)
{
    static const std::string key = "A5dgY6lz9fpG9kGNiH1mZ";

    for (size_t i = 0; i < data.size(); i++)
    {
        (uint8_t &)data[i] ^= (uint8_t)key[i % key.length()];
    }
}

static void LoadData(str &json)
{
    std::ifstream stream(config::datastorePath(), std::ios::binary);

    if (stream.good())
    {
        stream.seekg(0, std::ios::end);
        size_t fileSize = (size_t)stream.tellg();
        stream.seekg(0, std::ios::beg);

        vec<char> buffer(fileSize);
        stream.read(buffer.data(), fileSize);

        TransformData(buffer);
        json.assign(buffer.begin(), buffer.end());
    }
    else
    {
        json.assign("{}");
    }

    stream.close();

#if _DEBUG
    printf("datastore.load: %.*s\n", (int)json.length(), json.c_str());
#endif
}

static void SaveData(str &json)
{
    std::ofstream stream(config::datastorePath(), std::ios::binary);

    if (stream.good())
    {
        vec<char> buffer(json.begin(), json.end());
        TransformData(buffer);
        stream.write(buffer.data(), buffer.size());
    }

    stream.close();

#if _DEBUG
    printf("datastore.save: %.*s\n", (int)json.length(), json.c_str());
#endif
}

V8Value *native_LoadDataStore(const vec<V8Value *> &args)
{
    str json{};
    LoadData(json);

    CefStr result{};
    cef_string_from_utf8(json.c_str(), json.length(), &result);

    return V8Value::string(&result);
}

V8Value *native_SaveDataStore(const vec<V8Value *> &args)
{
    if (args.size() > 0 && args[0]->isString())
    {
        CefScopedStr json = args[0]->asString();

        if (!json.empty())
        {
            CefStrUtf8 output(json.ptr());
            cef_string_to_utf8(json.str, json.length, &output);

            str data = output.cstr();
            SaveData(data);
        }
    }

    return nullptr;
}