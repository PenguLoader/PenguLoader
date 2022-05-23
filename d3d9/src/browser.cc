#include "internal.h"
#include "detours.h"

#include "include\capi\cef_app_capi.h"
#include "include\capi\cef_client_capi.h"
#include "include\capi\cef_browser_capi.h"

using namespace league_loader;

// BROWSER PROCESS ONLY.

DWORD BROWSER_PROCESS_ID = 0;
DWORD RENDERER_PROCESS_ID = 0;

UINT REMOTE_DEBUGGING_PORT = 0;
cef_browser_t *CLIENT_BROWSER = nullptr;

void FetchRemoteDevToolsUrl();

static decltype(cef_life_span_handler_t::on_after_created) Old_OnAfterCreated;
static void CEF_CALLBACK Hooked_OnAfterCreated(struct _cef_life_span_handler_t* self,
    struct _cef_browser_t* browser)
{
    if (CLIENT_BROWSER == nullptr) {
        // Save client browser.
        CLIENT_BROWSER = browser;
        // Add ref.
        CLIENT_BROWSER->base.add_ref(&CLIENT_BROWSER->base);

        // Fetch remote DevTools URL.
        CreateThread(NULL, 0,
            (LPTHREAD_START_ROUTINE)&FetchRemoteDevToolsUrl, NULL, 0, NULL);
    }
    
    Old_OnAfterCreated(self, browser);
}

static decltype(cef_life_span_handler_t::on_before_close) Old_OnBeforeClose;
static void CEF_CALLBACK Hooked_OnBeforeClose(cef_life_span_handler_t* self,
    struct _cef_browser_t* browser)
{
    if (CLIENT_BROWSER && browser->is_same(browser, CLIENT_BROWSER)) {
        // Release client browser.
        CLIENT_BROWSER->base.release(&CLIENT_BROWSER->base);
        CLIENT_BROWSER = nullptr;
    }

    Old_OnBeforeClose(self, browser);
}

static void HookClient(cef_client_t *client)
{
    // Hook LifeSpanHandler.
    static auto Old_GetLifeSpanHandler = client->get_life_span_handler;
    // Don't worry about calling convention here (stdcall).
    client->get_life_span_handler =  [](struct _cef_client_t* self) -> cef_life_span_handler_t*
    {
        auto handler = Old_GetLifeSpanHandler(self);

        // Hook OnAfterCreated().
        Old_OnAfterCreated = handler->on_after_created;
        handler->on_after_created = Hooked_OnAfterCreated;

        // Hook OnBeforeClose().
        Old_OnBeforeClose = handler->on_before_close;
        handler->on_before_close = Hooked_OnBeforeClose;

        return handler;
    };
}

static int Hooked_CefBrowserHost_CreateBrowser(
    const cef_window_info_t* windowInfo,
    struct _cef_client_t* client,
    const cef_string_t* url,
    struct _cef_browser_settings_t* settings,
    struct _cef_dictionary_value_t* extra_info,
    struct _cef_request_context_t* request_context)
{
    // Hook main window only.
    if (wcsstr(url->str, L"riot:") && wcsstr(url->str, L"/bootstrap.html")) {
        // Create extra info if null.
        if (extra_info == NULL) {
            extra_info = CefDictionaryValue_Create();
        }

        // Add current process ID (browser process).
        extra_info->set_int(extra_info,
            &CefStr("BROWSER_PROCESS_ID"), GetCurrentProcessId());

        // Hook client.
        HookClient(client);
    }

    return CefBrowserHost_CreateBrowser(windowInfo, client, url, settings, extra_info, request_context);
}

static decltype(cef_app_t::on_before_command_line_processing) Old_OnBeforeCommandLineProcessing;
static void CEF_CALLBACK Hooked_OnBeforeCommandLineProcessing(
    struct _cef_app_t* self,
    const cef_string_t* process_type,
    struct _cef_command_line_t* command_line)
{
    // Keep Riot's command lines.
    Old_OnBeforeCommandLineProcessing(self, process_type, command_line);

    auto sPort = GetConfigValue(L"RemoteDebuggingPort");
    REMOTE_DEBUGGING_PORT = wcstol(sPort.c_str(), NULL, 10);
    if (REMOTE_DEBUGGING_PORT != 0) {
        // Set remote debugging port.
        command_line->append_switch_with_value(command_line,
            &CefStr("remote-debugging-port"), &CefStr(REMOTE_DEBUGGING_PORT));
    }

    if (GetConfigValue(L"DisableWebSecurity") == L"1") {
        // Disable web security.
        command_line->append_switch(command_line,
            &CefStr("disable-web-security"));
    }

    if (GetConfigValue(L"IgnoreCertificateErrors") == L"1") {
        // Ignore invalid certs.
        command_line->append_switch(command_line,
            &CefStr("ignore-certificate-errors"));
    }
}

static int Hooked_CefInitialize(const struct _cef_main_args_t* args,
    const struct _cef_settings_t* settings, cef_app_t* app, void* windows_sandbox_info)
{
    // Hook command line.
    Old_OnBeforeCommandLineProcessing = app->on_before_command_line_processing;
    app->on_before_command_line_processing = Hooked_OnBeforeCommandLineProcessing;

    return CefInitialize(args, settings, app, windows_sandbox_info);
}

void HookBrowserProcess()
{
    DetourRestoreAfterWith();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // Hook CefInitialize().
    DetourAttach(&(PVOID &)CefInitialize, Hooked_CefInitialize);
    // Hook CefBrowserHost::CreateBrowser().
    DetourAttach(&(PVOID &)CefBrowserHost_CreateBrowser, Hooked_CefBrowserHost_CreateBrowser);

    DetourTransactionCommit();
}
