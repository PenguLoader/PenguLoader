#include "../internal.h"

// RENDERER PROCESS ONLY.

void LoadPlugins(cef_frame_t *frame, cef_v8context_t *context)
{
    auto pluginsDir = config::getPluginsDir();

    if (!utils::dirExist(pluginsDir))
        return;

    std::wstring script = L"(() => { ";

    // Iterate through plugins folder.
    utils::readDir(pluginsDir + L"\\*", [&script, &pluginsDir](const wstring &name, bool isDir)
    {
        // Skip name starts with underscore and dot.
        if (name[0] == '_' || name[0] == '.')
            return;

        // Skip non-JS file.
        if (!isDir && !utils::strEndWith(name, L".js"))
            return;

        // Skip folder has no index.
        if (isDir && !utils::fileExist(pluginsDir + L"\\" + name + L"\\index.js"))
            return;

        script += L"import(\"//plugins/";
        script += isDir ? (name + L"/index.js") : name;
        script += L"\"); ";
    });

    script.append(L"})();");

    // Execute.
    frame->execute_java_script(frame, &CefStr(script), &""_s, 1);
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
    if (utils::fileExist(filePath))
    {
        utils::readFile(filePath, source);
        type = JS_MODULE;
        return true;
    }
    // Check if the given path is folder.
    else if (utils::dirExist(path))
    {
        filePath = path + L"/index.js";
        // And it contains index.js.
        if (utils::fileExist(filePath))
        {
            utils::readFile(filePath, source);
            type = INDEX_MODULE;
            return true;
        }
    }
    else if (utils::fileExist(path))
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
        CefScopedStr path{ args[0]->get_string_value(args[0]) };

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