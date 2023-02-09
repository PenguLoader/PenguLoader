#include "../internal.h"

// RENDERER PROCESS ONLY.

void LoadPlugins(cef_frame_t *frame, cef_v8context_t *context)
{
    auto pluginsPath = config::getPluginsDir();

    if (!utils::fileExist(pluginsPath, true))
        return;

    // Iterate through plugins folder.
    for (const auto &file : utils::getFiles(pluginsPath, L"*.js"))
    {
        // Skip file name starts with underscore.
        if (file[0] == L'_') continue;

        // Create require script.
        std::wstring script;
        script += L"__require(\"";
        script += file.substr(0, file.length() - 3);    // Remove .js extension.
        script += L"\");";

        // Execute.
        frame->execute_java_script(frame, &CefStr(script), &""_s, 0);
    }
}

enum RequireType
{
    JS_MODULE = 0,
    INDEX_MODULE,
    TEXT_CONTENT
};

static bool NativeRequire(const std::wstring &genericPath, std::string &source, RequireType &type)
{
    auto path = config::getPluginsDir() + genericPath;
    auto filePath = path + L".js";

    // Check .js file.
    if (utils::fileExist(filePath, false))
    {
        utils::readFile(filePath, source);
        type = JS_MODULE;
        return true;
    }
    // Check if the given path is folder.
    else if (utils::fileExist(path, true))
    {
        filePath = path + L"/index.js";
        // And it contains index.js.
        if (utils::fileExist(filePath, false))
        {
            utils::readFile(filePath, source);
            type = INDEX_MODULE;
            return true;
        }
    }
    else if (utils::fileExist(path, false))
    {
        utils::readFile(path, source);
        type = TEXT_CONTENT;
        return true;
    }

    return false;
}

bool HandlePlugins(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval)
{
    if (fn == L"Require")
    {
        RequireType type;
        std::string source;
        CefStr path(args[0]->get_string_value(args[0]));

#if _DEBUG
        wprintf(L"  | < %s\n", path.str);
#endif

        if (NativeRequire(path.str, source, type))
        {
            auto data = CefV8Value_CreateArray(2);
            data->set_value_byindex(data, 0, CefV8Value_CreateString(&CefStr(source)));
            data->set_value_byindex(data, 1, CefV8Value_CreateInt(type));
            retval = data;
        }
        else
        {
            retval = CefV8Value_CreateNull();
        }

        return true;
    }

    return false;
}