#include "commons.h"
#include "v8_wrapper.h"

static V8Value *v8_OpenDevTools(V8Value *const *args, int argc)
{
    auto context = cef_v8context_get_current_context();
    auto frame = context->get_frame(context);

    // IPC to browser process.
    auto msg = cef_process_message_create(&u"@open-devtools"_s);
    frame->send_process_message(frame, PID_BROWSER, msg);

    return nullptr;
}

static V8Value *v8_OpenPluginsFolder(V8Value *const *args, int argc)
{   
    bool found = true;
    path dir = config::plugins_dir();

    if (argc > 0)
    {
        CefScopedStr path = args[0]->asString();
        dir /= path.to_path();

        if (!file::is_dir(dir))
            found = false;
    }

    shell::open_folder(dir);
    return V8Value::boolean(found);
}

static V8Value *v8_ReloadClient(V8Value *const *args, int argc)
{
    auto context = cef_v8context_get_current_context();
    auto frame = context->get_frame(context);

    // IPC to browser process.
    auto msg = cef_process_message_create(&u"@reload-client"_s);
    frame->send_process_message(frame, PID_BROWSER, msg);

    return nullptr;
}

V8HandlerFunctionEntry v8_HelperEntries[]
{
    { "OpenDevTools", v8_OpenDevTools },
    { "OpenPluginsFolder", v8_OpenPluginsFolder },
    { "ReloadClient", v8_ReloadClient },
    { nullptr },
};