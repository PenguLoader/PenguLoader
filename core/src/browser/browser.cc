#include "browser.h"
#include "hook.h"
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_browser_capi.h"

// BROWSER PROCESS ONLY.

void *browser::view_handle = NULL;

static void enhance_browser_window(cef_browser_t *browser)
{
    if (browser::view_handle) return;
    auto host = browser->get_host(browser);

#if OS_WIN
    // Get needed windows.
    HWND browserWin = host->get_window_handle(host);
    // Retrieve top-level window (RCLIENT).
    HWND rclient = GetAncestor(browserWin, GA_ROOT);
    browser::view_handle = (void *)rclient;

    HWND widgetWin = FindWindowExA(browserWin, NULL, "Chrome_WidgetWin_0", NULL);
    //HWND widgetHost = FindWindowExA(widgetWin, NULL, "Chrome_RenderWidgetHostHWND", NULL);

    // Ensure transparency effect.
    //   hide Chrome_RenderWidgetHostHWND
    //ShowWindow(widgetHost, SW_HIDE);
    //   hide CefBrowserWindow
    ShowWindow(browserWin, SW_HIDE);
    //   bring Chrome_WidgetWin_0 to top-level children
    SetParent(widgetWin, rclient);
#elif OS_MAC
    browser::view_handle = (void*)host->get_window_handle(host);
#endif

    window::set_theme(browser::view_handle, true);
    window::enable_shadow(browser::view_handle);

    host->base.release(&host->base);
}

static decltype(cef_life_span_handler_t::on_after_created) OnAfterCreated;
static void CEF_CALLBACK Hooked_OnAfterCreated(struct _cef_life_span_handler_t* self,
    struct _cef_browser_t* browser)
{
    OnAfterCreated(self, browser);
    enhance_browser_window(browser);
}

static void HookMainBrowserClient(cef_client_t *client)
{
    void HookKeyboardHandler(cef_client_t *client);
    HookKeyboardHandler(client);

    // Hook LifeSpanHandler.
    static auto GetLifeSpanHandler = client->get_life_span_handler;
    // Don't worry about calling convention here (stdcall).
    client->get_life_span_handler =  [](struct _cef_client_t* self) -> cef_life_span_handler_t*
    {
        auto handler = GetLifeSpanHandler(self);

        // Hook OnAfterCreated().
        OnAfterCreated = handler->on_after_created;
        handler->on_after_created = Hooked_OnAfterCreated;

        return handler;
    };

    static auto OnProcessMessageReceived = client->on_process_message_received;
    client->on_process_message_received = [](struct _cef_client_t* self,
        struct _cef_browser_t* browser,
        struct _cef_frame_t* frame,
        cef_process_id_t source_process,
        struct _cef_process_message_t* message) -> int
    {
        if (source_process == PID_RENDERER)
        {
            CefScopedStr name{ message->get_name(message) };
            auto margs = message->get_argument_list(message);

            if (name.equal("@open-devtools"))
            {
                browser::open_devtools(browser);
                return 1;
            }
            else if (name.equal("@reload-client"))
            {
                browser->reload_ignore_cache(browser);
                return 1;
            }
            else if (name.equal("@set-window-vibrancy"))
            {
                if (margs->get_type(margs, 0) == VTYPE_NULL)
                    window::clear_vibrancy(browser::view_handle);
                else
                {
                    uint32_t param1 = (uint32_t)margs->get_double(margs, 0);
                    uint32_t param2 = (uint32_t)margs->get_double(margs, 1);
                    window::apply_vibrancy(browser::view_handle, param1, param2);
                }
                return 1;
            }
            else if (name.equal("@set-window-theme"))
            {
                bool dark = margs->get_bool(margs, 0);
                window::set_theme(browser::view_handle, dark);
                return 1;
            }
        }

        return OnProcessMessageReceived(self, browser, frame, source_process, message);
    };
}

static hook::Hook<decltype(&cef_browser_host_create_browser)> CefBrowserHost_CreateBrowser;
static int Hooked_CefBrowserHost_CreateBrowser(
    const cef_window_info_t* windowInfo,
    struct _cef_client_t* client,
    const cef_string_t* url,
    const struct _cef_browser_settings_t* settings,
    struct _cef_dictionary_value_t* extra_info,
    struct _cef_request_context_t* request_context)
{
    auto url_ = CefStr::borrow(url);

    // Hook main browser only.
    if (url_.startw("https://riot:") && url_.endw("/bootstrap.html"))
    {
        // Create extra info if null.
        if (extra_info == nullptr)
            extra_info = cef_dictionary_value_create();

        // Set as main browser.
        extra_info->set_null(extra_info, &u"is_main"_s);

        // Hook client.
        HookMainBrowserClient(client);
    }

    return CefBrowserHost_CreateBrowser(windowInfo, client, url, settings, extra_info, nullptr);
}

static decltype(cef_app_t::on_before_command_line_processing) OnBeforeCommandLineProcessing;
static void CEF_CALLBACK Hooked_OnBeforeCommandLineProcessing(
    struct _cef_app_t* self,
    const cef_string_t* process_type,
    struct _cef_command_line_t* command_line)
{
    CefScopedStr rc_port = command_line->get_switch_value(command_line, &u"riotclient-app-port"_s);
    CefScopedStr rc_token = command_line->get_switch_value(command_line, &u"riotclient-auth-token"_s);
    browser::set_riotclient_credentials(rc_port.to_utf8().c_str(), rc_token.to_utf8().c_str());

#if OS_WIN
    // Extract args string.
    auto args = CefScopedStr(command_line->get_command_line_string(command_line)).to_utf16();

    if (config::options::AllowProxyServer())
    {
        size_t pos = args.find(u"--no-proxy-server");
        if (pos != std::wstring::npos)
            args.replace(pos, 17, u"");
    }

    // Rebuild it.
    command_line->reset(command_line);
    command_line->init_from_string(command_line, &CefStr(args));

    OnBeforeCommandLineProcessing(self, process_type, command_line);
#endif

#if OS_WIN
    if (int rdport = config::options::RemoteDebuggingPort())
    {
        // Set remote debugging port.
        command_line->append_switch_with_value(command_line,
            &u"remote-debugging-port"_s, &CefStr(std::to_string(rdport)));
    }

    if (config::options::DisableWebSecurity())
    {
        // Disable web security.
        command_line->append_switch(command_line, &u"disable-web-security"_s);
    }

    if (config::options::IgnoreCertificateErrors())
    {
        // Ignore invalid certs.
        command_line->append_switch(command_line, &u"ignore-certificate-errors"_s);
    }
#endif

    if (config::options::OptimizeClient())
    {
        // Optimize Client.
        command_line->append_switch(command_line, &u"disable-async-dns"_s);
        command_line->append_switch(command_line, &u"disable-plugins"_s);
        command_line->append_switch(command_line, &u"disable-extensions"_s);
        command_line->append_switch(command_line, &u"disable-background-networking"_s);
        command_line->append_switch(command_line, &u"disable-background-timer-throttling"_s);
        command_line->append_switch(command_line, &u"disable-backgrounding-occluded-windows"_s);
        command_line->append_switch(command_line, &u"disable-renderer-backgrounding"_s);
        command_line->append_switch(command_line, &u"disable-metrics"_s);
        command_line->append_switch(command_line, &u"disable-component-update"_s);
        command_line->append_switch(command_line, &u"disable-domain-reliability"_s);
        command_line->append_switch(command_line, &u"disable-translate"_s);
        command_line->append_switch(command_line, &u"disable-gpu-watchdog"_s);
        command_line->append_switch(command_line, &u"disable-renderer-accessibility"_s);
        command_line->append_switch(command_line, &u"enable-parallel-downloading"_s);
        command_line->append_switch(command_line, &u"enable-new-download-backend"_s);
        command_line->append_switch(command_line, &u"enable-quic"_s);
        command_line->append_switch(command_line, &u"no-pings"_s);
        command_line->append_switch(command_line, &u"no-sandbox"_s);
    }

    if (config::options::SuperLowSpecMode())
    {
        // Super Low Spec Mode.
        command_line->append_switch(command_line, &u"disable-smooth-scrolling"_s);
        command_line->append_switch(command_line, &u"wm-window-animations-disabled"_s);
        command_line->append_switch_with_value(command_line, &u"animation-duration-scale"_s, &u"0"_s);
    }

#if OS_MAC
    OnBeforeCommandLineProcessing(self, process_type, command_line);
#endif
}

static hook::Hook<decltype(&cef_initialize)> CefInitialize;
static int Hooked_CefInitialize(const struct _cef_main_args_t* args,
    const struct _cef_settings_t* settings, cef_app_t* app, void* windows_sandbox_info)
{
    // Hook command line.
    OnBeforeCommandLineProcessing = app->on_before_command_line_processing;
    app->on_before_command_line_processing = Hooked_OnBeforeCommandLineProcessing;

    const_cast<cef_settings_t *>(settings)->cache_path
        = CefStr::from_path(config::cache_dir()).forward();

    static auto GetBrowserProcessHandler = app->get_browser_process_handler;
    app->get_browser_process_handler = [](cef_app_t *self)
    {
        auto handler = GetBrowserProcessHandler(self);
        
        static auto OnContextIntialized = handler->on_context_initialized;
        handler->on_context_initialized = [](cef_browser_process_handler_t *self)
        {
            browser::register_plugins_domain();
            browser::register_riotclient_domain();
            OnContextIntialized(self);
        };

        return handler;
    };

    return CefInitialize(args, settings, app, windows_sandbox_info);
}

void HookBrowserProcess()
{
#if OS_WIN && _DEBUG
    // Open console window.
    AllocConsole();
    SetConsoleTitleA("League Client (browser process)");
    freopen("CONOUT$", "w", stdout);
#endif

    // Hook CefInitialize().
    CefInitialize.hook(LIBCEF_MODULE_NAME,
        "cef_initialize", Hooked_CefInitialize);

    // Hook CefBrowserHost::CreateBrowser().
    CefBrowserHost_CreateBrowser.hook(LIBCEF_MODULE_NAME,
        "cef_browser_host_create_browser", Hooked_CefBrowserHost_CreateBrowser);
}