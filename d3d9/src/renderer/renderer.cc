#include "../internal.h"

#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_v8_capi.h"

#include "extension.h"

// RENDERER PROCESS ONLY.

extern HWND RCLIENT_WINDOW;
static bool is_main_ = false;

int server_port_ = 0;

void LoadPlugins(cef_frame_t *frame, cef_v8context_t *context);
bool HandlePlugins(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval);
bool HandleDataStore(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval);
bool HandleWindowEffect(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval);

void TriggerAuthCallback(const wstring &url, int browser_id, const wstring &response);
bool HandleAuthCallback(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval);
void ClearAuthCallbacks(cef_v8context_t *context);

// Custom V8 handler for extenstion
struct ExtensionHandler : CefRefCount<cef_v8handler_t>
{
public:
    ExtensionHandler() : CefRefCount(this)
    {
        cef_v8handler_t::execute = _Execute;
    }

private:
    static int CALLBACK _Execute(cef_v8handler_t* self,
        const cef_string_t* name,
        cef_v8value_t* object,
        size_t argc,
        cef_v8value_t* const* argv,
        cef_v8value_t** retval,
        cef_string_t* exception)
    {
        wstring fn(name->str, name->length);
        vector<cef_v8value_t *> args(argv, argv + argc);

#if _DEBUG
        wprintf(L">> exec: %s\n", fn.c_str());
#endif

        if (fn == L"OpenDevTools")
        {
            auto context = CefV8Context_GetCurrentContext();
            auto frame = context->get_frame(context);
            // IPC to browser process.
            auto msg = CefProcessMessage_Create(&"__OPEN_DEVTOOLS"_s);
            frame->send_process_message(frame, PID_BROWSER, msg);

            return true;
        }
        else if (fn == L"OpenAssetsFolder")
        {
            utils::openFilesExplorer(config::getAssetsDir());
            return true;
        }
        else if (fn == L"OpenPluginsFolder")
        {
            utils::openFilesExplorer(config::getPluginsDir());
            return true;
        }
        else if (HandlePlugins(fn, args, *retval))
            return true;
        else if (HandleDataStore(fn, args, *retval))
            return true;
        else if (HandleWindowEffect(fn, args, *retval))
            return true;
        else if (HandleAuthCallback(fn, args, *retval))
            return true;

        return false;
    }
};

static decltype(cef_render_process_handler_t::on_web_kit_initialized) Old_OnWebKitInitialized;
static void CEF_CALLBACK Hooked_OnWebKitInitialized(cef_render_process_handler_t* self)
{
    Old_OnWebKitInitialized(self);

    std::string ext_code{ _ext_code, (size_t)_ext_code_length };

    const char *version =
#   include "../LeagueLoader/Version.cs"
        ;

    ext_code.append("\nvar __llver = \"");
    ext_code.append(version);
    ext_code.append("\"");

    // Register our extension.
    CefRegisterExtension(&"v8/LeagueLoader"_s, &CefStr(ext_code), new ExtensionHandler());
}

static decltype(cef_render_process_handler_t::on_context_created) Old_OnContextCreated;
static void CEF_CALLBACK Hooked_OnContextCreated(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    struct _cef_v8context_t* context)
{
    Old_OnContextCreated(self, browser, frame, context);

    if (is_main_)
    {
        CefScopedStr url{ frame->get_url(frame) };

        if (url.contain(L"riot:") && url.contain(L"index.html"))
        {
            // Open console window.
#if _DEBUG
            AllocConsole();
            SetConsoleTitleA("League Client (main renderer process)");
            freopen("CONOUT$", "w", stdout);
#endif

            // Load plugins.
            LoadPlugins(frame, context);
        }
    }
}

static decltype(cef_render_process_handler_t::on_context_released) Old_OnContextReleased;
static void CEF_CALLBACK Hooked_OnContextReleased(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    struct _cef_v8context_t* context)
{
    if (is_main_)
    {
        ClearAuthCallbacks(context);
    }
}

static decltype(cef_render_process_handler_t::on_browser_created) Old_OnBrowserCreated;
static void CEF_CALLBACK Hooked_OnBrowserCreated(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_dictionary_value_t* extra_info)
{
    // Detect hooked client.
    is_main_ = extra_info && extra_info->has_key(extra_info, &"is_main"_s);

    Old_OnBrowserCreated(self, browser, extra_info);
}

static decltype(cef_render_process_handler_t::on_process_message_received) Old_OnProcessMessageReceived;
static int CEF_CALLBACK Hooked_OnProcessMessageReceived(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    cef_process_id_t source_process,
    struct _cef_process_message_t* message)
{
    if (is_main_ && source_process == PID_BROWSER)
    {
        CefScopedStr msg{ message->get_name(message) };
        if (msg == L"__rclient")
        {
            // Received RCLIENT HWND.
            auto args = message->get_argument_list(message);
            RCLIENT_WINDOW = reinterpret_cast<HWND>(args->get_int(args, 0));
            return 1;
        }
        else if (msg == L"__server_port")
        {
            auto args = message->get_argument_list(message);
            server_port_ = args->get_int(args, 0);
            return 1;
        }
        else if (msg == L"__auth_response")
        {
            auto args = message->get_argument_list(message);

            int id = browser->get_identifier(browser);
            CefScopedStr url{ args->get_string(args, 0) };
            CefScopedStr response{ args->get_string(args, 1) };

            TriggerAuthCallback(url.cstr(), id, response.cstr());
            return 1;
        }
    }

    return Old_OnProcessMessageReceived(self, browser, frame, source_process, message);
}

static int Hooked_CefExecuteProcess(const cef_main_args_t* args, cef_app_t* app, void* windows_sandbox_info)
{
    // Hook RenderProcessHandler.
    static auto Old_GetRenderProcessHandler = app->get_render_process_handler;
    app->get_render_process_handler = [](cef_app_t* self) -> cef_render_process_handler_t*
    {
        // Get default handler.
        auto handler = Old_GetRenderProcessHandler(self);

        // Hook OnWebKitInitialized().
        Old_OnWebKitInitialized = handler->on_web_kit_initialized;
        handler->on_web_kit_initialized = Hooked_OnWebKitInitialized;

        // Hook OnContextCreated().
        Old_OnContextCreated = handler->on_context_created;
        handler->on_context_created = Hooked_OnContextCreated;

        // Hook OnContextReleased().
        Old_OnContextReleased = handler->on_context_released;
        handler->on_context_released = Hooked_OnContextReleased;

        // Hook OnBrowserCreated().
        Old_OnBrowserCreated = handler->on_browser_created;
        handler->on_browser_created = Hooked_OnBrowserCreated;

        // Hook OnProcessMessageReceived().
        Old_OnProcessMessageReceived = handler->on_process_message_received;
        handler->on_process_message_received = Hooked_OnProcessMessageReceived;

        return handler;
    };

    return CefExecuteProcess(args, app, windows_sandbox_info);
}

void HookRendererProcess()
{
    // Hook CefExecuteProcess().
    utils::hookFunc(&CefExecuteProcess, Hooked_CefExecuteProcess);
}