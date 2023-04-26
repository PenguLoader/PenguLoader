#include "../internal.h"
#include "../hook.h"

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_browser_capi.h"

// BROWSER PROCESS ONLY.

UINT REMOTE_DEBUGGING_PORT = 0;

cef_browser_t *browser_ = nullptr;
static int browser_id_ = -1;

extern LPCWSTR DEVTOOLS_WINDOW_NAME;

void PrepareDevTools();
void OpenDevTools_Internal(bool remote);
void SetUpBrowserWindow(cef_browser_t *browser, cef_frame_t *frame);

void RegisterAssetsSchemeHandlerFactory();
void RegisterRiotClientSchemeHandlerFactory();
void SetRiotClientCredentials(const wstring &appPort, const wstring &authToken);

void OpenInternalServer();
void CloseInternalServer();

cef_jsdialog_handler_t *CreateCustomJSDialogHandler();

static decltype(cef_life_span_handler_t::on_after_created) Old_OnAfterCreated;
static void CEF_CALLBACK Hooked_OnAfterCreated(struct _cef_life_span_handler_t* self,
    struct _cef_browser_t* browser)
{
    if (browser_ == nullptr)
    {
        // Add ref.
        browser->base.add_ref(&browser_->base);
        // Save client browser.
        browser_ = browser;
        browser_id_ = browser->get_identifier(browser);

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
    if (browser->get_identifier(browser) == browser_id_)
    {
        browser_ = nullptr;
        CloseInternalServer();
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
    if (patched) return;
    patched = true;

    SetUpBrowserWindow(browser, frame);
    OpenInternalServer();
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

    static auto GetJSDialogHandler = client->get_jsdialog_handler;
    client->get_jsdialog_handler = [](struct _cef_client_t* self)
    {
        return CreateCustomJSDialogHandler();
    };

    static auto OnProcessMessageReceived = client->on_process_message_received;
    client->on_process_message_received = [](struct _cef_client_t* self,
        struct _cef_browser_t* browser,
        struct _cef_frame_t* frame,
        cef_process_id_t source_process,
        struct _cef_process_message_t* message)
    {
        if (source_process == PID_RENDERER)
        {
            CefScopedStr name{ message->get_name(message) };
            if (name == L"__open_devtools")
                OpenDevTools_Internal(false);
            else if (name == L"__open_remote_devtools")
                OpenDevTools_Internal(true);
            else if (name == L"__reload_client")
                browser->reload_ignore_cache(browser);
        }

        return OnProcessMessageReceived(self, browser, frame, source_process, message);
    };
}

static Hook<decltype(&cef_browser_host_create_browser)> Old_CefBrowserHost_CreateBrowser;
static int Hooked_CefBrowserHost_CreateBrowser(
    const cef_window_info_t* windowInfo,
    struct _cef_client_t* client,
    const cef_string_t* url,
    const struct _cef_browser_settings_t* settings,
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
        extra_info->set_null(extra_info, &"is_main"_s);

        // Hook client.
        HookClient(client);
    }

    return Old_CefBrowserHost_CreateBrowser(windowInfo, client, url, settings, extra_info, nullptr);
}

static decltype(cef_app_t::on_before_command_line_processing) Old_OnBeforeCommandLineProcessing;
static void CEF_CALLBACK Hooked_OnBeforeCommandLineProcessing(
    struct _cef_app_t* self,
    const cef_string_t* process_type,
    struct _cef_command_line_t* command_line)
{
    CefScopedStr rc_port{ command_line->get_switch_value(command_line, &"riotclient-app-port"_s) };
    CefScopedStr rc_token{ command_line->get_switch_value(command_line, &"riotclient-auth-token"_s) };
    SetRiotClientCredentials(rc_port.cstr(), rc_token.cstr());

    // Extract args string.
    auto args = CefScopedStr{ command_line->get_command_line_string(command_line) }.cstr();

    auto chromiumArgs = config::getConfigValue(L"ChromiumArgs");
    if (!chromiumArgs.empty())
    {
        args += L" " + chromiumArgs;
    }

    if (!config::getConfigValueBool(L"NoProxyServer", true))
    {
        size_t pos = args.find(L"--no-proxy-server");
        if (pos != std::wstring::npos)
            args.replace(pos, 17, L"");
    }

    // Rebuild it.
    command_line->reset(command_line);
    command_line->init_from_string(command_line, &CefStr(args));

    Old_OnBeforeCommandLineProcessing(self, process_type, command_line);

    if (REMOTE_DEBUGGING_PORT = config::getConfigValueInt(L"RemoteDebuggingPort", 0))
    {
        // Set remote debugging port.
        command_line->append_switch_with_value(command_line,
            &"remote-debugging-port"_s, &CefStr(std::to_string(REMOTE_DEBUGGING_PORT)));
    }

    if (config::getConfigValueBool(L"DisableWebSecurity", false))
    {
        // Disable web security.
        command_line->append_switch(command_line, &"disable-web-security"_s);
    }

    if (config::getConfigValueBool(L"IgnoreCertificateErrors", false))
    {
        // Ignore invalid certs.
        command_line->append_switch(command_line, &"ignore-certificate-errors"_s);
    }

    if (config::getConfigValueBool(L"SuperLowSpecMode", false))
    {
        // Super Low Spec Mode.
        command_line->append_switch(command_line, &"disable-smooth-scrolling"_s);
        command_line->append_switch(command_line, &"wm-window-animations-disabled"_s);
        command_line->append_switch_with_value(command_line, &"animation-duration-scale"_s, &"0"_s);
    }
}

static Hook<decltype(&cef_initialize)> Old_CefInitialize;
static int Hooked_CefInitialize(const struct _cef_main_args_t* args,
    const struct _cef_settings_t* settings, cef_app_t* app, void* windows_sandbox_info)
{
    // Hook command line.
    Old_OnBeforeCommandLineProcessing = app->on_before_command_line_processing;
    app->on_before_command_line_processing = Hooked_OnBeforeCommandLineProcessing;

    wchar_t cachePath[1024];
    GetEnvironmentVariableW(L"LOCALAPPDATA", cachePath, _countof(cachePath));
    lstrcatW(cachePath, L"\\Riot Games\\League of Legends\\Cache");
    const_cast<cef_settings_t *>(settings)->cache_path = CefStr(cachePath).forawrd();
    const_cast<cef_settings_t *>(settings)->log_severity = LOGSEVERITY_DISABLE;

    static auto GetBrowserProcessHandler = app->get_browser_process_handler;
    app->get_browser_process_handler = [](cef_app_t *self)
    {
        auto handler = GetBrowserProcessHandler(self);
        
        static auto OnContextIntialized = handler->on_context_initialized;
        handler->on_context_initialized = [](cef_browser_process_handler_t *self)
        {
            RegisterAssetsSchemeHandlerFactory();
            RegisterRiotClientSchemeHandlerFactory();

            OnContextIntialized(self);
        };

        return handler;
    };

    return Old_CefInitialize(args, settings, app, windows_sandbox_info);
}

static Hook<decltype(&CreateProcessW)> Old_CreateProcessW;
static BOOL WINAPI Hooked_CreateProcessW(
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation)
{
    bool shouldHook = utils::strContain(lpCommandLine, L"LeagueClientUxRender.exe", false)
        && utils::strContain(lpCommandLine, L"--type=renderer", false);

    if (shouldHook)
        dwCreationFlags |= CREATE_SUSPENDED;

    BOOL ret = Old_CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    if (ret && shouldHook)
    {
        void InjectThisDll(HANDLE hProcess);
        InjectThisDll(lpProcessInformation->hProcess);
        ResumeThread(lpProcessInformation->hThread);
    }

    return ret;
}

static Hook<decltype(&CreateWindowExW)> Old_CreateWindowExW;
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

        extern HWND devtools_window_;
        bool IsWindowsLightTheme();
        void ForceDarkTheme(HWND);

        if (!IsWindowsLightTheme())
        {
            // Force dark theme.
            ForceDarkTheme(hwnd);

            RECT rc;
            GetClientRect(hwnd, &rc);
            // Fix titlebar issue.
            SetWindowPos(hwnd, NULL, 0, 0, rc.right - 5, rc.bottom, SWP_NOMOVE | SWP_FRAMECHANGED);
        }

        devtools_window_ = hwnd;
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
    Old_CefInitialize.hook("libcef.dll", "cef_initialize", Hooked_CefInitialize);

    // Hook CefBrowserHost::CreateBrowser().
    Old_CefBrowserHost_CreateBrowser.hook("libcef.dll", "cef_browser_host_create_browser", Hooked_CefBrowserHost_CreateBrowser);

    // Hook CreateProcessW().
    Old_CreateProcessW.hook(&CreateProcessW, Hooked_CreateProcessW);

    // Hook CreateWindowExW().
    Old_CreateWindowExW.hook(CreateWindowExW, Hooked_CreateWindowExW);
}
