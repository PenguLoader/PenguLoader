#include "../internal.h"
#include <psapi.h>

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_browser_capi.h"

// BROWSER PROCESS ONLY.

UINT REMOTE_DEBUGGING_PORT = 0;
HWND DEVTOOLS_HWND = 0;
cef_browser_t *CLIENT_BROWSER = nullptr;

extern LPCWSTR DEVTOOLS_WINDOW_NAME;

void PrepareDevTools();
cef_resource_handler_t *CreateAssetsResourceHandler(const wstring &path);
cef_resource_handler_t *CreateRiotClientResourceHandler(cef_frame_t *frame, wstring path);
void SetRiotClientCredentials(const wstring &appPort, const wstring &authToken);

static int64 _mainBrowserId = 0;
static decltype(cef_life_span_handler_t::on_after_created) Old_OnAfterCreated;
static void CEF_CALLBACK Hooked_OnAfterCreated(struct _cef_life_span_handler_t* self,
    struct _cef_browser_t* browser)
{
    if (CLIENT_BROWSER == nullptr)
    {
        // Add ref.
        browser->base.add_ref(&CLIENT_BROWSER->base);
        // Save client browser.
        _mainBrowserId = browser->get_identifier(browser);
        CLIENT_BROWSER = browser;

        // Initialize DevTools opener.
        PrepareDevTools();
    }

    Old_OnAfterCreated(self, browser);
}

static decltype(cef_life_span_handler_t::on_before_close) Old_OnBeforeClose;
static void CEF_CALLBACK Hooked_OnBeforeClose(cef_life_span_handler_t* self,
    struct _cef_browser_t* browser)
{
    // Check main browser.
    if (browser->get_identifier(browser) == _mainBrowserId)
    {
        CLIENT_BROWSER = nullptr;
    }

    Old_OnBeforeClose(self, browser);
}

static decltype(cef_load_handler_t::on_load_start) Old_OnLoadStart;
static void CALLBACK Hooked_OnLoadStart(struct _cef_load_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    cef_transition_type_t transition_type)
{
    Old_OnLoadStart(self, browser, frame, transition_type);
    if (!frame->is_main(frame)) return;

    // Patch once.
    static bool patched = false;
    if (patched || (patched = true, false)) return;

    auto host = browser->get_host(browser);
    // Get needed windows.
    HWND browserWin = host->get_window_handle(host);
    HWND rclient = GetParent(browserWin);
    HWND widgetWin = FindWindowExA(browserWin, NULL, "Chrome_WidgetWin_0", NULL);
    HWND widgetHost = FindWindowExA(widgetWin, NULL, "Chrome_RenderWidgetHostHWND", NULL);

    // Hide Chrome_RenderWidgetHostHWND.
    ShowWindow(widgetHost, SW_HIDE);
    // Hide CefBrowserWindow.
    ShowWindow(browserWin, SW_HIDE);
    // Bring Chrome_WidgetWin_0 into top-level children.
    SetParent(widgetWin, rclient);
    SetWindowPos(widgetWin, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // Send RCLIENT HWND to renderer.
    auto msg = cef_process_message_create(&"__RCLIENT"_s);
    auto args = msg->get_argument_list(msg);
    args->set_int(args, 0, static_cast<int>((DWORD)rclient));
    frame->send_process_message(frame, PID_RENDERER, msg);
};

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

    // Hook LoadHandler;
    static auto Old_GetLoadHandler = client->get_load_handler;
    client->get_load_handler = [](struct _cef_client_t* self)
    {
        auto handler = Old_GetLoadHandler(self);

        // Hook OnLoadStart().
        Old_OnLoadStart = handler->on_load_start;
        handler->on_load_start = Hooked_OnLoadStart;

        return handler;
    };

    // Hook RequestHandler.
    static auto Old_GetRequestHandler = client->get_request_handler;
    client->get_request_handler = [](struct _cef_client_t* self) -> cef_request_handler_t*
    {
        auto handler = Old_GetRequestHandler(self);

        static auto Old_GetResourceRequestHandler = handler->get_resource_request_handler;
        handler->get_resource_request_handler = [](
            struct _cef_request_handler_t* self,
            struct _cef_browser_t* browser,
            struct _cef_frame_t* frame,
            struct _cef_request_t* request,
            int is_navigation,
            int is_download,
            const cef_string_t* request_initiator,
            int* disable_default_handling) -> cef_resource_request_handler_t*
        {
            auto handler = Old_GetResourceRequestHandler(self, browser, frame, request,
                is_navigation, is_download, request_initiator, disable_default_handling);

            static auto Old_GetResourceHandler = handler->get_resource_handler;
            handler->get_resource_handler = [](
                struct _cef_resource_request_handler_t* self,
                struct _cef_browser_t* browser,
                struct _cef_frame_t* frame,
                struct _cef_request_t* request) -> cef_resource_handler_t*
            {
                CefStr url = request->get_url(request);
                cef_resource_handler_t *handler = nullptr;

                if (wcsncmp(url.str, L"https://assets/", 15) == 0)
                    return CreateAssetsResourceHandler(url.str + 14);
                else if (wcsncmp(url.str, L"https://riotclient/", 19) == 0)
                    return CreateRiotClientResourceHandler(frame, url.str + 18);

                return Old_GetResourceHandler(self, browser, frame, request);
            };

            return handler;
        };

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
    if (utils::strContain(url->str, L"riot:") && utils::strContain(url->str, L"/bootstrap.html"))
    {
        // Create extra info if null.
        if (extra_info == NULL)
            extra_info = CefDictionaryValue_Create();

        // Add current process ID (browser process).
        extra_info->set_null(extra_info, &"IS_MAIN"_s);

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
    CefStr rc_port = command_line->get_switch_value(command_line, &"riotclient-app-port"_s);
    CefStr rc_token = command_line->get_switch_value(command_line, &"riotclient-auth-token"_s);
    SetRiotClientCredentials(rc_port.str, rc_token.str);

    // Keep Riot's command lines.
    Old_OnBeforeCommandLineProcessing(self, process_type, command_line);

    auto sPort = config::getConfigValue(L"RemoteDebuggingPort");
    REMOTE_DEBUGGING_PORT = wcstol(sPort.c_str(), NULL, 10);
    if (REMOTE_DEBUGGING_PORT != 0) {
        // Set remote debugging port.
        command_line->append_switch_with_value(command_line, &"remote-debugging-port"_s, &CefStr(REMOTE_DEBUGGING_PORT));
    }

    if (config::getConfigValue(L"DisableWebSecurity") == L"1") {
        // Disable web security.
        command_line->append_switch(command_line, &"disable-web-security"_s);
    }

    if (config::getConfigValue(L"IgnoreCertificateErrors") == L"1") {
        // Ignore invalid certs.
        command_line->append_switch(command_line, &"ignore-certificate-errors"_s);
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

static auto Old_CreateWindowExW = &CreateWindowExW;
static HWND WINAPI Hooked_CreateWindowExW(
    _In_ DWORD dwExStyle,
    _In_opt_ LPCWSTR lpClassName,
    _In_opt_ LPCWSTR lpWindowName,
    _In_ DWORD dwStyle,
    _In_ int X,
    _In_ int Y,
    _In_ int nWidth,
    _In_ int nHeight,
    _In_opt_ HWND hWndParent,
    _In_opt_ HMENU hMenu,
    _In_opt_ HINSTANCE hInstance,
    _In_opt_ LPVOID lpParam)
{
    HWND hwnd = Old_CreateWindowExW(dwExStyle, lpClassName,
        lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

    // Avoid ATOM value.
    if ((uintptr_t)lpClassName <= UINT16_MAX)
        return hwnd;

    // Detect DevTools window.
    if (!wcscmp(lpClassName, L"CefBrowserWindow") && !wcscmp(lpWindowName, DEVTOOLS_WINDOW_NAME))
    {
        // Get League icon.
        HWND hClient = FindWindowW(L"RCLIENT", L"League of Legends");
        HICON icon = (HICON)SendMessageW(hClient, WM_GETICON, ICON_BIG, 0);

        // Set window icon.
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);

        DEVTOOLS_HWND = hwnd;
    }

    return hwnd;
}

void HookBrowserProcess()
{
    // Open console window.
#if _DEBUG
    AllocConsole();
    SetConsoleTitleA("League Client (browser process)");
    freopen("CONOUT$", "w", stdout);
#endif

    // Hook CefInitialize().
    utils::hookFunc((void **)&CefInitialize, Hooked_CefInitialize);
    // Hook CefBrowserHost::CreateBrowser().
    utils::hookFunc((void **)&CefBrowserHost_CreateBrowser, Hooked_CefBrowserHost_CreateBrowser);

    // Hook CreateWindowExW().
    utils::hookFunc((void **)&Old_CreateWindowExW, Hooked_CreateWindowExW);
}
