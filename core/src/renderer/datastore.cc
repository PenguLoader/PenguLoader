#include "../internal.h"
#include <fstream>

static void TransformData(string &data)
{
    const std::string key = "A5dgY6lz9fpG9kGNiH1mZ";
    for (size_t i = 0; i < data.length(); i++)
        data[i] = (uint8_t)data[i] ^ (uint8_t)key[i % key.length()];
}

static wstring GetDataPath()
{
    return config::getLoaderDir() + L"\\datastore";
}

void LoadData(string &data)
{
    std::ifstream stream(GetDataPath());

    if (stream.good())
    {
        data.assign((std::istreambuf_iterator<char>(stream)),
            (std::istreambuf_iterator<char>()));
        TransformData(data);
    }

    stream.close();
}

void SaveData(string &data)
{
    std::ofstream stream(GetDataPath());

    if (stream.good())
    {
        TransformData(data);
        stream.write(data.c_str(), data.length());
    }

    stream.close();
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