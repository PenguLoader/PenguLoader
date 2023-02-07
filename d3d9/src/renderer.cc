#include "internal.h"
#include "detours.h"

#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_v8_capi.h"

using namespace league_loader;

// RENDERER PROCESS ONLY.

extern DWORD BROWSER_PROCESS_ID;

static bool IS_CLIENT = false;
HANDLE BROWSER_PROCESS;

void LoadPlugins(cef_frame_t *frame, cef_v8context_t *context);
bool NativeRequire(const std::wstring &path, std::string &source, int &flag);

bool HandleWindowEffect(const CefStr &fn, int argc, cef_v8value_t * const *args, cef_v8value_t **retval);

// Custom V8 handler for extenstion
struct ExtensionHandler : CefRefCount<cef_v8handler_t>
{
public:
    ExtensionHandler() : CefRefCount(this)
    {
        execute = _Execute;
    }

private:
    static int CALLBACK _Execute(cef_v8handler_t* self,
        const cef_string_t* _name,
        cef_v8value_t* object,
        size_t argc,
        cef_v8value_t* const* args,
        cef_v8value_t** retval,
        cef_string_t* exception)
    {
        CefStr name(_name);

        if (name == L"OpenDevTools")
        {
            OpenDevTools(false);
            return true;
        }
        else if (name == L"OpenPluginsFolder")
        {
            ShellExecuteW(NULL, L"open", GetPluginsDir().c_str(), NULL, NULL, SW_SHOW);
            return true;
        }
        else if (name == L"Require")
        {
            int flag;
            std::string source;
            CefStr path(args[0]->get_string_value(args[0]));

            if (NativeRequire(path.str, source, flag))
            {
                auto data = CefV8Value_CreateArray(2);
                data->set_value_byindex(data, 0, CefV8Value_CreateString(&CefStr(source)));
                data->set_value_byindex(data, 1, CefV8Value_CreateInt(flag));
                *retval = data;
            }
            else
            {
                *retval = CefV8Value_CreateNull();
            }

            return true;
        }
        else if (HandleWindowEffect(name, argc, args, retval))
        {
            return true;
        }

        return false;
    }
};

static decltype(cef_render_process_handler_t::on_web_kit_initialized) Old_OnWebKitInitialized;
static void CEF_CALLBACK Hooked_OnWebKitInitialized(cef_render_process_handler_t* self)
{
    Old_OnWebKitInitialized(self);

    std::string ext_code = u8""
#   include "ext_code.h"
        ;

    const char *version =
#   include "../gui/Version.cs"
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

    if (IS_CLIENT)
    {
        CefStr url(frame->get_url(frame));
        if (url.contain(L"riot:") && url.contain(L"index.html"))
        {
            // Open console window.
#if _DEBUG
            AllocConsole();
            SetConsoleTitleA("League Client (renderer process)");
            freopen("CONOUT$", "w", stdout);
#endif

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
    IS_CLIENT = extra_info && extra_info->has_key(extra_info, &"BROWSER_PROCESS_ID"_s);

    if (IS_CLIENT)
    {
        BROWSER_PROCESS_ID = extra_info->get_int(extra_info, &"BROWSER_PROCESS_ID"_s);
        BROWSER_PROCESS = OpenProcess(PROCESS_ALL_ACCESS, FALSE, BROWSER_PROCESS_ID);
    }

    Old_OnBrowserCreated(self, browser, extra_info);
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

        // Hook OnBrowserCreated().
        Old_OnBrowserCreated = handler->on_browser_created;
        handler->on_browser_created = Hooked_OnBrowserCreated;

        return handler;
    };

    return CefExecuteProcess(args, app, windows_sandbox_info);
}

void HookRendererProcess()
{
    DetourRestoreAfterWith();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // Hook CefExecuteProcess().
    DetourAttach(&(PVOID &)CefExecuteProcess, Hooked_CefExecuteProcess);

    DetourTransactionCommit();
}