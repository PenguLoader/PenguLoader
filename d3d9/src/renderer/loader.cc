#include "../internal.h"
#include <chrono>

// RENDERER PROCESS ONLY.

void LoadPlugins(cef_frame_t *frame, cef_v8context_t *context)
{
    auto pluginsDir = config::getPluginsDir();

    if (!utils::dirExist(pluginsDir))
        return;

    using namespace std::chrono;
    auto current_time = system_clock::now();
    auto current_ms = duration_cast<milliseconds>(current_time.time_since_epoch()).count();

    std::wstring script = L"(() => { ";

    // Iterate through plugins folder.
    utils::readDir(pluginsDir + L"\\*", [&script, &pluginsDir, &current_ms](const wstring &name, bool isDir)
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

        script.append(L"import(\"//plugins/");
        script.append(isDir ? (name + L"/index.js") : name);
        script.append(L"?t=" + std::to_wstring(current_ms));
        script.append(L"&v=2\"); ");
    });

    script.append(L"})();");

    // Execute.
    frame->execute_java_script(frame, &CefStr(script), &"https://plugins/"_s, 1);
}

bool HandlePlugins(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval)
{
    if (fn == L"RequireFile")
    {
        if (args.size() > 0 && args[0]->is_string(args[0]))
        {
            string content{};
            CefScopedStr path{ args[0]->get_string_value(args[0]) };
            wstring _path{ path.str, path.length };

            size_t pos = _path.find(L"//");
            if (pos != string::npos)
                _path = _path.substr(pos + 2);

            if (_path.length() > 1 && _path[0] == L'/')
                _path = _path.substr(1);

            _path = config::getLoaderDir()
                .append(L"/").append(_path);

            if (utils::readFile(_path, content))
            {
                retval = CefV8Value_CreateString(&CefStr(content));
                return true;
            }
        }

        retval = CefV8Value_CreateNull();
        return true;
    }

    return false;
}