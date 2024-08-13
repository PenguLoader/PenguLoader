#include "commons.h"
#include "hook.h"
#include "v8_wrapper.h"
#include <unordered_map>
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_render_process_handler_capi.h"

// RENDERER PROCESS ONLY.

static bool is_main_ = false;

extern V8HandlerFunctionEntry v8_DataStoreEntries[];
extern V8HandlerFunctionEntry v8_HelperEntries[];

static std::vector<path> get_plugin_entries()
{
    std::vector<path> entries;
    auto plugins_dir = config::plugins_dir();

    /*
        plugins/
          |__@author
            |__plugin-1
              |__index.js       <-- by author plugin
          |__plugin-2
            |__index.js         <-- normal plugin
          |__plugin-3.js        <-- top-level plugin
    */

    if (file::is_dir(plugins_dir))
    {
        // Scan plugins dir.
        for (const auto &name : file::read_dir(plugins_dir))
        {
            auto ch1 = name.c_str()[0];

            // Skip name starts with underscore or dot.
            if (ch1 == '_' || ch1 == '.')
                continue;

            auto path = plugins_dir / name;

            if (file::is_file(path))
            {
                // Top-level JS file.
                if (name.string().ends_with(".js"))
                {
                    entries.push_back(name);
                }
            }
            else if (file::is_dir(path))
            {
                // Group by @author.
                if (ch1 == '@')
                {
                    for (const auto &subname : file::read_dir(path))
                    {
                        auto ch1 = subname.c_str()[0];
                        if (ch1 == '_' || ch1 == '.')
                            continue;

                        if (file::is_file(path / subname / "index.js"))
                        {
                            entries.push_back(name / subname / "index.js");
                        }
                    }
                }
                // Sub-folder with index.
                else if (file::is_file(path / "index.js"))
                {
                    entries.push_back(name / "index.js");
                }
            }
        }
    }

    return entries;
}

struct NativeV8Handler : CefRefCount<cef_v8handler_t>
{
    std::unordered_map<std::string, V8FunctionHandler> map_;

    NativeV8Handler() : CefRefCount(this)
    {
        cef_bind_method(NativeV8Handler, execute);
    }

private:
    int CALLBACK _execute(
        const cef_string_t* name,
        cef_v8value_t* object,
        size_t _argc,
        cef_v8value_t* const* _args,
        cef_v8value_t** retval,
        cef_string_t* exception)
    {
        cef_string_utf8_t func{""};
        cef_string_to_utf8(name->str, name->length, &func);

        bool handled = false;
        auto it = map_.find(func.str);

        if (it != map_.end())
        {
            int argc = static_cast<int>(_argc);
            auto args = reinterpret_cast<V8Value *const *>(_args);

            auto result = it->second(args, argc);
            if (result != nullptr)
                *retval = reinterpret_cast<cef_v8value_t *>(result);

            handled = true;
        }

        cef_string_utf8_clear(&func);
        return handled;
    }
};

static void ExposeNativeFunctions(V8Object *window)
{
    auto native = V8Object::create();
    auto handler = new NativeV8Handler();

    auto list = {
        v8_DataStoreEntries,
        v8_HelperEntries,
    };

    for (auto &entries : list) {
        for (auto entry = entries; entry->name; entry++) {
            handler->map_[entry->name] = entry->func;
            auto name = CefStr(entry->name);
            auto function = V8Value::function(&name, handler);
            native->set(&name, function, V8_PROPERTY_ATTRIBUTE_READONLY);
        }
    }

    window->set(&u"__native"_s, native, V8_PROPERTY_ATTRIBUTE_READONLY);
}

static void ExposeOsObject(V8Object *window)
{
    auto object = V8Object::create();

    object->set(&u"name"_s, V8Value::string(&CefStr(PLATFORM_NAME)), V8_PROPERTY_ATTRIBUTE_READONLY);
    object->set(&u"version"_s, V8Value::string(&CefStr(platform::get_os_version())), V8_PROPERTY_ATTRIBUTE_READONLY);
    object->set(&u"build"_s, V8Value::string(&CefStr(platform::get_os_build())), V8_PROPERTY_ATTRIBUTE_READONLY);

    window->set(&u"os"_s, object, V8_PROPERTY_ATTRIBUTE_READONLY);
}

static void LoadPlugins(V8Object *window)
{
    auto pengu = V8Object::create();

    // Pengu.version, set it later
    auto version = V8Value::string(&CefStr(""));
    pengu->set(&u"version"_s, version, V8_PROPERTY_ATTRIBUTE_NONE);

    // Pengu.superPotato
    auto superPotato = V8Value::boolean(config::options::super_potato());
    pengu->set(&u"superPotato"_s, superPotato, V8_PROPERTY_ATTRIBUTE_READONLY);

    pengu->set(&u"isMac"_s,
#ifdef OS_MAC
        V8Value::boolean(true),
#else
        V8Value::boolean(false),
#endif
        V8_PROPERTY_ATTRIBUTE_READONLY);

    // Pengu.plugins
    auto entries = get_plugin_entries();
    auto pluginEntries = V8Array::create((int)entries.size());

    for (int index = 0; index < (int)entries.size(); index++)
    {
        auto entry = CefStr::from_path(entries[index]);
        auto value = V8Value::string(&entry);
        pluginEntries->set(index, value);
    }

    // array must add to parent objet after init.
    pengu->set(&u"plugins"_s, pluginEntries, V8_PROPERTY_ATTRIBUTE_READONLY);

    // Pengu.disabledPlugins
    auto disabledPlugins = CefStr(config::disabled_plugins());
    pengu->set(&u"disabledPlugins"_s, V8Value::string(&disabledPlugins), V8_PROPERTY_ATTRIBUTE_NONE);

    // Add Pengu to window.
    window->set(&u"Pengu"_s, pengu, V8_PROPERTY_ATTRIBUTE_READONLY);
}

static void ExecutePreloadScript(cef_frame_t *frame)
{
#ifdef _DEBUG
    void *buffer; size_t length;
    path preload_path = config::loader_dir() / "../plugins/dist/preload.js";

    if (file::read_file(preload_path, &buffer, &length))
    {
        CefStr script((const char *)buffer, length);
        frame->execute_java_script(frame, &script, &u"https://plugins/@/preload"_s, 1);
        free(buffer);
    }
#else
#   include "../../plugins/dist/preload.g.h"
    CefStr script{ (const char *)_preload_script, _preload_script_size };
    frame->execute_java_script(frame, &script, nullptr, 1);
#endif
}

static decltype(cef_render_process_handler_t::on_context_created) OnContextCreated;
static void CEF_CALLBACK Hooked_OnContextCreated(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    struct _cef_v8context_t* context)
{
    CefScopedStr url = frame->get_url(frame);

    // Detect main page.
    if (is_main_ && url.startw("https://riot:") && url.endw("/index.html"))
    {
#if OS_WIN && _DEBUG
        // Open console window.
        AllocConsole();
        SetConsoleTitleA("League Client (main renderer process)");
        FILE *_fp; freopen_s(&_fp, "CONOUT$", "w", stdout);
#endif
        auto window = context->get_global(context);

        ExposeOsObject(reinterpret_cast<V8Object *>(window));
        ExposeNativeFunctions(reinterpret_cast<V8Object *>(window));
        LoadPlugins(reinterpret_cast<V8Object *>(window));
        ExecutePreloadScript(frame);
    }

    OnContextCreated(self, browser, frame, context);
}

static decltype(cef_render_process_handler_t::on_context_released) OnContextReleased;
static void CEF_CALLBACK Hooked_OnContextReleased(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    struct _cef_v8context_t* context)
{
    if (is_main_)
    {
    }

    OnContextReleased(self, browser, frame, context);
}

static decltype(cef_render_process_handler_t::on_browser_created) OnBrowserCreated;
static void CEF_CALLBACK Hooked_OnBrowserCreated(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_dictionary_value_t* extra_info)
{
    // Detect main browser.
    is_main_ = extra_info && extra_info->has_key(extra_info, &u"is_main"_s);

    OnBrowserCreated(self, browser, extra_info);
}

static decltype(cef_render_process_handler_t::on_process_message_received) OnProcessMessageReceived;
static int CEF_CALLBACK Hooked_OnProcessMessageReceived(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    cef_process_id_t source_process,
    struct _cef_process_message_t* message)
{
    if (is_main_ && source_process == PID_BROWSER)
    {
        // CefScopedStr msg = message->get_name(message);
    }

    return OnProcessMessageReceived(self, browser, frame, source_process, message);
}

static hook::Hook<decltype(&cef_execute_process)> CefExecuteProcess;
static int Hooked_CefExecuteProcess(const cef_main_args_t* args, cef_app_t* app, void* windows_sandbox_info)
{
    // Hook RenderProcessHandler.
    static auto Old_GetRenderProcessHandler = app->get_render_process_handler;
    app->get_render_process_handler = [](cef_app_t* self) -> cef_render_process_handler_t*
    {
        // Get default handler.
        auto handler = Old_GetRenderProcessHandler(self);

        // Hook OnContextCreated().
        OnContextCreated = handler->on_context_created;
        handler->on_context_created = Hooked_OnContextCreated;

        // // Hook OnContextReleased().
        // OnContextReleased = handler->on_context_released;
        // handler->on_context_released = Hooked_OnContextReleased;

        // Hook OnBrowserCreated().
        OnBrowserCreated = handler->on_browser_created;
        handler->on_browser_created = Hooked_OnBrowserCreated;

        // // Hook OnProcessMessageReceived().
        // OnProcessMessageReceived = handler->on_process_message_received;
        // handler->on_process_message_received = Hooked_OnProcessMessageReceived;

        return handler;
    };

    return CefExecuteProcess(args, app, windows_sandbox_info);
}

void HookRendererProcess()
{
    // Hook CefExecuteProcess().
#if OS_WIN
    CefExecuteProcess.hook(LIBCEF_MODULE_NAME, "cef_execute_process", Hooked_CefExecuteProcess);
#elif OS_MAC
    CefExecuteProcess.hook(cef_execute_process, Hooked_CefExecuteProcess);
#endif
}