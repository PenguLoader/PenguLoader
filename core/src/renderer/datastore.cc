#include "../internal.h"
#include <fstream>

static void TransformData(vector<char> &data)
{
    static const std::string key = "A5dgY6lz9fpG9kGNiH1mZ";

    for (size_t i = 0; i < data.size(); i++)
    {
        (uint8_t &)data[i] ^= (uint8_t)key[i % key.length()];
    }
}

static wstring GetDataPath()
{
    return config::getLoaderDir() + L"\\datastore";
}

static void LoadData(string &json)
{
    std::ifstream stream(GetDataPath(), std::ios::binary);

    if (stream.good())
    {
        stream.seekg(0, std::ios::end);
        size_t fileSize = stream.tellg();
        stream.seekg(0, std::ios::beg);

        vector<char> buffer(fileSize);
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

static void SaveData(string &json)
{
    std::ofstream stream(GetDataPath(), std::ios::binary);

    if (stream.good())
    {
        vector<char> buffer(json.begin(), json.end());
        TransformData(buffer);
        stream.write(buffer.data(), buffer.size());
    }

    stream.close();

#if _DEBUG
    printf("datastore.save: %.*s\n", (int)json.length(), json.c_str());
#endif
}

bool HandleDataStore(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval)
{
    if (fn == L"LoadData")
    {
        string json{};
        LoadData(json);

        cef_string_t result{};
        CefString_FromUtf8(json.c_str(), json.length(), &result);
        retval = CefV8Value_CreateString(&result);

        CefString_Clear(&result);
        return true;
    }
    else if (fn == L"SaveData")
    {
        if (args.size() > 0 && args[0]->is_string(args[0]))
        {
            auto json = args[0]->get_string_value(args[0]);
            if (!json || json->length == 0)
                return true;

            cef_string_utf8_t output{};
            CefString_ToUtf8(json->str, json->length, &output);
            string data(output.str, output.length);

            SaveData(data);
            CefString_UserFree_Free(json);
            CefString_ClearUtf8(&output);
        }
        return true;
    }

    return false;
}