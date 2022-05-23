#include "internal.h"
#include "detours.h"

#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_v8_capi.h"

using namespace league_loader;

// RENDERER PROCESS ONLY.

extern DWORD BROWSER_PROCESS_ID;
extern DWORD RENDERER_PROCESS_ID;

static bool IS_CLIENT = false;
static HANDLE BROWSER_PROCESS;

void LoadPlugins(cef_frame_t *frame, cef_v8context_t *context);
bool NativeRequire(const std::wstring &path, std::wstring &source, int &flag);

void OpenDevTools(BOOL remote);

// Static V8Handler for our extension.
static struct V8Handler : CefRefCountStatic<cef_v8handler_t>
{
    V8Handler() {
        execute = _Execute;
    }

    static int CALLBACK _Execute(cef_v8handler_t* self,
        const cef_string_t* _name,
        cef_v8value_t* object,
        size_t argc,
        cef_v8value_t* const* args,
        cef_v8value_t** retval,
        cef_string_t* exception)
    {
        CefStr name(_name);

        if (name == L"OpenDevTools") {
            if (argc > 0 && args[0]->get_bool_value(args[0])) {
                // Open remote DevTools.
                IPC_CALL(BROWSER_PROCESS, &OpenDevTools, TRUE);
            }
            else {
                // Open built-in DevTools.
                IPC_CALL(BROWSER_PROCESS, &OpenDevTools, FALSE);
            }
            return true;
        }
        else if (name == L"Require") {
            int flag;
            std::wstring source;
            CefStr path(args[0]->get_string_value(args[0]));

            if (NativeRequire(path.str, source, flag)) {
                auto data = CefV8Value_CreateArray(2);
                data->set_value_byindex(data, 0, CefV8Value_CreateString(&CefStr(source)));
                data->set_value_byindex(data, 1, CefV8Value_CreateInt(flag));
                *retval = data;
            }
            else {
                *retval = CefV8Value_CreateNull();
            }

            return true;
        }

        return false;
    }

} V8_HANDLER;

static decltype(cef_render_process_handler_t::on_web_kit_initialized) Old_OnWebKitInitialized;
static void CEF_CALLBACK Hooked_OnWebKitInitialized(cef_render_process_handler_t* self)
{
    Old_OnWebKitInitialized(self);

    const char *ext_code = ""
#   include "ext_code.h"
        ;

    // Register our extension.
    CefRegisterExtension(&CefStr("v8/LeagueLoader"), &CefStr(ext_code), &V8_HANDLER);
}

static decltype(cef_render_process_handler_t::on_context_created) Old_OnContextCreated;
static void CEF_CALLBACK Hooked_OnContextCreated(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    struct _cef_v8context_t* context)
{
    Old_OnContextCreated(self, browser, frame, context);

    if (IS_CLIENT) {
        CefStr url(frame->get_url(frame));
        if (url.contain(L"riot:") && url.contain(L"index.html")) {
            // Load plugins.
            LoadPlugins(frame, context);
        }
    }
}

static decltype(cef_render_process_handler_t::on_browser_created) Old_OnBrowserCreated;
static void CEF_CALLBACK Hooked_OnBrowserCreated(
    struct _cef_render_process_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_dictionary_value_t* extra_info)
{
    // Detect hooked client.
    IS_CLIENT = extra_info && extra_info->has_key(extra_info, &CefStr("BROWSER_PROCESS_ID"));

    if (IS_CLIENT) {
        BROWSER_PROCESS_ID = extra_info->get_int(extra_info, &CefStr("BROWSER_PROCESS_ID"));
        BROWSER_PROCESS = OpenProcess(PROCESS_ALL_ACCESS, FALSE, BROWSER_PROCESS_ID);

        // Write renderer process ID (current process) to browser process.
        RENDERER_PROCESS_ID = GetCurrentProcessId();
        IPC_WRITE(BROWSER_PROCESS, &RENDERER_PROCESS_ID, sizeof(DWORD));
    }

    Old_OnBrowserCreated(self, browser, extra_info);
}

static int Hooked_CefExecteProcess(const cef_main_args_t* args, cef_app_t* app, void* windows_sandbox_info)
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

        // Hook OnBrowserCreated().
        Old_OnBrowserCreated = handler->on_browser_created;
        handler->on_browser_created = Hooked_OnBrowserCreated;

        return handler;
    };

    return CefExecteProcess(args, app, windows_sandbox_info);
}

void HookRendererProcess()
{
    DetourRestoreAfterWith();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // Hook CefExecuteProcess().
    DetourAttach(&(PVOID &)CefExecteProcess, Hooked_CefExecteProcess);

    DetourTransactionCommit();
}