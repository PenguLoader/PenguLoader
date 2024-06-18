#include "commons.h"
#include "hook.h"
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_render_process_handler_capi.h"

static const char *PL_VERSION {
#   include "../loader/version.cs"
};

// RENDERER PROCESS ONLY.

extern HWND RCLIENT_WINDOW;
static bool is_main_ = false;

V8Value *native_LoadDataStore(const vec<V8Value *> &args);
V8Value *native_SaveDataStore(const vec<V8Value *> &args);

//V8Value* native_ReadFile(const vec<V8Value*>& args);
//V8Value* native_WriteFile(const vec<V8Value*>& args);
//V8Value* native_MkDir(const vec<V8Value*>& args);
//V8Value* native_Stat(const vec<V8Value*>& args);
//V8Value* native_Ls(const vec<V8Value*>& args);
//V8Value* native_Remove(const vec<V8Value*>& args);

V8Value *native_GetWindowEffect(const vec<V8Value *> &args);
V8Value *native_SetWindowEffect(const vec<V8Value *> &args);
V8Value *native_SetWindowTheme(const vec<V8Value *> &args);

static vec<wstr> GetPluginEntries()
{
    vec<wstr> entries{};

    /*
        plugins/
          |__@author
            |__plugin-1
              |__index.js       <-- by author plugin
          |__plugin-2
            |__index.js         <-- normal plugin
          |__plugin-3.js        <-- top-level plugin
    */

    auto pluginsDir = config::pluginsDir();
    if (utils::isDir(pluginsDir))
    {
        // Scan plugins dir.
        for (const auto &name : utils::readDir(pluginsDir))
        {
            // Skip name starts with underscore or dot.
            if (name[0] == '_' || name[0] == '.')
                continue;

            auto path = pluginsDir / name;

            // Top-level JS file.
            if (std::regex_search(name, std::wregex(L"\\.js$", std::regex::icase)) && utils::isFile(path))
            {
                entries.push_back(name);
            }
            // Group by @author.
            else if (name[0] == '@' && utils::isDir(path))
            {
                for (const auto &subname : utils::readDir(path))
                {
                    if (subname[0] == '_' || subname[0] == '.')
                        continue;

                    if (utils::isFile(path / subname / "index.js"))
                    {
                        entries.push_back(name + L"/" + subname + L"/index.js");
                    }
                }
            }
            // Sub-folder with index.
            else if (utils::isFile(path / "index.js"))
            {
                entries.push_back(name + L"/index.js");
            }
        }
    }

    return entries;
}

static V8Value *native_OpenDevTools(const vec<V8Value *> &args)
{
    bool remote = args.size() > 0 && args[0]->asBool();

    auto context = cef_v8context_get_current_context();
    auto frame = context->get_frame(context);

    // IPC to browser process.
    auto name = remote ? u"__open_remote_devtools"_s : u"__open_devtools"_s;
    auto msg = cef_process_message_create(&name);
    frame->send_process_message(frame, PID_BROWSER, msg);

    return nullptr;
}

static V8Value *native_OpenPluginsFolder(const vec<V8Value *> &args)
{   
    wstr destPath = config::pluginsDir();

    if (args.size() > 0)
    {
        CefScopedStr path = args[0]->asString();
        destPath += L"\\" + path.cstr();

        if (!utils::isDir(destPath))
            return V8Value::boolean(false);
    }

    shell::open_folder(destPath.c_str());
    return V8Value::boolean(true);
}

static V8Value *native_ReloadClient(const vec<V8Value *> &args)
{
    auto context = cef_v8context_get_current_context();
    auto frame = context->get_frame(context);

    // IPC to browser process.
    auto msg = cef_process_message_create(&CefStr("__reload_client"));
    frame->send_process_message(frame, PID_BROWSER, msg);

    return nullptr;
}

static map<wstr, V8FunctionHandler> m_nativeDelegateMap
{
    { L"OpenDevTools", native_OpenDevTools },
    { L"OpenPluginsFolder", native_OpenPluginsFolder },
    { L"ReloadClient", native_ReloadClient },

    { L"LoadDataStore", native_LoadDataStore },
    { L"SaveDataStore", native_SaveDataStore },

    //{ L"ReadFile", native_ReadFile},
    //{ L"WriteFile", native_WriteFile},
    //{ L"MkDir", native_MkDir},
    //{ L"Stat", native_Stat},
    //{ L"Ls", native_Ls},
    //{ L"Remove", native_Remove},

    { L"GetWindowEffect", native_GetWindowEffect },
    { L"SetWindowEffect", native_SetWindowEffect },
    { L"SetWindowTheme", native_SetWindowTheme },
};

struct NativeV8Handler : CefRefCount<cef_v8handler_t>
{
    NativeV8Handler() : CefRefCount(this)
    {
        cef_v8handler_t::execute = Execute;
    }

private:
    static int CALLBACK Execute(cef_v8handler_t* self,
        const cef_string_t* name,
        cef_v8value_t* object,
        size_t argc,
        cef_v8value_t* const* argv,
        cef_v8value_t** retval,
        cef_string_t* exception)
    {
        wstr func{ name->str, name->length };

#if _DEBUG
        wprintf(L"native invoke: %s(%zu)\n", func.c_str(), argc);
#endif

        auto it = m_nativeDelegateMap.find(func);
        if (it != m_nativeDelegateMap.end())
        {
            const auto argv_ = (V8Value **)(argv);
            vec<V8Value *> args{ argv_, argv_ + argc };

            auto result = it->second(args);
            if (result != nullptr)
            {
                *retval = (cef_v8value_t *)result;
            }

            return true;
        }

        return false;
    }
};

static void ExposeNativeFunctions(V8Object *window)
{
    auto native = V8Object::create();

    for (const auto &it : m_nativeDelegateMap)
    {
        auto name = CefStr(it.first);
        auto function = V8Value::function(&name, new NativeV8Handler());
        native->set(&name, function, V8_PROPERTY_ATTRIBUTE_READONLY);
    }

    window->set(&u"__native"_s, native, V8_PROPERTY_ATTRIBUTE_READONLY);

    window->set(&u"__llver"_s, V8Value::string(&CefStr(PL_VERSION)), V8_PROPERTY_ATTRIBUTE_READONLY);
}

static void LoadPlugins(V8Object *window)
{
    auto pengu = V8Object::create();

    // Pengu.version
    auto version = V8Value::string(&CefStr(PL_VERSION));
    pengu->set(&u"version"_s, version, V8_PROPERTY_ATTRIBUTE_READONLY);

    // Pengu.superPotato
    auto superPotato = V8Value::boolean(config::options::SuperLowSpecMode());
    pengu->set(&u"superPotato"_s, superPotato, V8_PROPERTY_ATTRIBUTE_READONLY);

    // Pengu.silentMode
    auto silentMode = V8Value::boolean(config::options::SilentMode());
    pengu->set(&u"silentMode"_s, silentMode, V8_PROPERTY_ATTRIBUTE_READONLY);

    pengu->set(&u"os"_s, V8Value::string(&u"win"_s), V8_PROPERTY_ATTRIBUTE_READONLY);
    pengu->set(&u"osVersion"_s, V8Value::string(&u"10"_s), V8_PROPERTY_ATTRIBUTE_READONLY);

    // Pengu.entries
    auto entries = GetPluginEntries();
    auto pluginEntries = V8Array::create((int)entries.size());

    for (int index = 0; index < (int)entries.size(); index++)
    {
        auto entry = V8Value::string(&CefStr(entries[index]));
        pluginEntries->set(index, entry);
    }

    // Should add to parent objet after init.
    pengu->set(&u"plugins"_s, pluginEntries, V8_PROPERTY_ATTRIBUTE_READONLY);

    // Add Pengu to window.
    window->set(&u"Pengu"_s, pengu, V8_PROPERTY_ATTRIBUTE_READONLY);
}

static void ExecutePreloadScript(cef_frame_t *frame)
{
#ifdef _DEBUG
    str script{};
    if (utils::readFile(config::loaderDir() / "../plugins/dist/preload.js", script))
    {
        CefStr code{ script.c_str(), script.length() };
        frame->execute_java_script(frame, &code, &u"https://plugins/@/preload"_s, 1);
    }
    else
    {
        printf("preload is not found, please start dev server and reload your client\n");
    }
#else
#   include "../plugins/dist/preload.g.h"
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
    OnContextCreated(self, browser, frame, context);

    // Detect main page.
    if (is_main_ && url.search(L"^https:\\/\\/riot:.+\\/index\\.html", true))
    {
        // Open console window.
#if _DEBUG
        AllocConsole();
        SetConsoleTitleA("League Client (main renderer process)");
        freopen("CONOUT$", "w", stdout);

        wprintf(L"main frame: %.*s\n", (int)url.length, url.str);
#endif

        auto window = context->get_global(context);

        ExposeNativeFunctions(reinterpret_cast<V8Object *>(window));
        LoadPlugins(reinterpret_cast<V8Object *>(window));
        ExecutePreloadScript(frame);
    }
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
        CefScopedStr msg = message->get_name(message);

        if (msg.equal(L"__rclient"))
        {
            // Received RCLIENT HWND.
            auto args = message->get_argument_list(message);
            RCLIENT_WINDOW = reinterpret_cast<HWND>((intptr_t)args->get_int(args, 0));
            return 1;
        }
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

        // Hook OnContextReleased().
        OnContextReleased = handler->on_context_released;
        handler->on_context_released = Hooked_OnContextReleased;

        // Hook OnBrowserCreated().
        OnBrowserCreated = handler->on_browser_created;
        handler->on_browser_created = Hooked_OnBrowserCreated;

        // Hook OnProcessMessageReceived().
        OnProcessMessageReceived = handler->on_process_message_received;
        handler->on_process_message_received = Hooked_OnProcessMessageReceived;

        return handler;
    };

    return CefExecuteProcess(args, app, windows_sandbox_info);
}

void HookRendererProcess()
{
    // Hook CefExecuteProcess().
    CefExecuteProcess.hook("libcef.dll", "cef_execute_process", Hooked_CefExecuteProcess);
}