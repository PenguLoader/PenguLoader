#include "pengu.h"
#include "v8_wrapper.h"

static V8Value *v8_open_devtools(V8Value *const *args, int argc)
{
    auto context = cef_v8context_get_current_context();
    auto frame = context->get_frame(context);

    // IPC to browser process.
    auto msg = cef_process_message_create(&u"@open-devtools"_s);
    frame->send_process_message(frame, PID_BROWSER, msg);

    return nullptr;
}

static V8Value *v8_open_plugins_folder(V8Value *const *args, int argc)
{   
    bool found = true;
    path plugins_dir = config::plugins_dir();
    path dir = plugins_dir;

    if (argc > 0)
    {
        CefScopedStr subpath_str = args[0]->asString();
        path subpath = subpath_str.to_path();

        // Only accept relative subpaths that stay within plugins_dir.
        // Absolute paths (e.g. "C:\Windows\calc.exe") are rejected outright via
        // is_absolute(); root-relative paths (e.g. "\Windows") and traversal
        // sequences (e.g. "..\..\Windows") are caught by the lexically_relative
        // check below.
        if (!subpath.is_absolute())
        {
            path candidate = (plugins_dir / subpath).lexically_normal();
            path rel = candidate.lexically_relative(plugins_dir.lexically_normal());
            // rel is empty when the paths share no common root (different drives),
            // and starts with ".." when the candidate escapes plugins_dir.
            // Otherwise rel contains the relative path from plugins_dir to candidate,
            // confirming safe containment.
            if (!rel.empty() && rel.begin()->string() != "..")
            {
                // Only navigate to the candidate if it is actually a directory.
                // If a file path (e.g. "plugin/evil.bat") slips through, opening it
                // with ShellExecuteW would execute it; fall back to plugins_dir.
                if (file::is_dir(candidate))
                    dir = candidate;
                else
                    found = false;
            }
        }
    }

    shell::open_folder(dir);
    return V8Value::boolean(found);
}

static V8Value *v8_reload_client(V8Value *const *args, int argc)
{
    auto context = cef_v8context_get_current_context();
    auto frame = context->get_frame(context);

    // IPC to browser process.
    auto msg = cef_process_message_create(&u"@reload-client"_s);
    frame->send_process_message(frame, PID_BROWSER, msg);

    return nullptr;
}

static V8Value* v8_set_window_vibrancy(V8Value *const *args, int argc)
{
    auto context = cef_v8context_get_current_context();

    if (argc > 0)
    {
        auto msg = cef_process_message_create(&u"@set-window-vibrancy"_s);
        auto margs = msg->get_argument_list(msg);

        if (args[0]->isNull())
            margs->set_null(margs, 0);
        else
            margs->set_double(margs, 0, args[0]->asDouble());

        if (argc >= 2)
            margs->set_double(margs, 1, args[1]->asDouble());

        auto frame = context->get_frame(context);
        frame->send_process_message(frame, PID_BROWSER, msg);
    }

    return nullptr;
}

static V8Value *v8_set_window_theme(V8Value *const *args, int argc)
{
    auto context = cef_v8context_get_current_context();

    if (argc > 0)
    {
        auto msg = cef_process_message_create(&u"@set-window-theme"_s);
        auto margs = msg->get_argument_list(msg);
        margs->set_bool(margs, 0, args[0]->asBool());

        auto frame = context->get_frame(context);
        frame->send_process_message(frame, PID_BROWSER, msg);
    }

    return nullptr;
}

V8HandlerFunctionEntry v8_HelperEntries[]
{
    { "OpenDevTools", v8_open_devtools },
    { "OpenPluginsFolder", v8_open_plugins_folder },
    { "ReloadClient", v8_reload_client },
    { "SetWindowVibrancy", v8_set_window_vibrancy },
    { "SetWindowTheme", v8_set_window_theme },
    { nullptr },
};