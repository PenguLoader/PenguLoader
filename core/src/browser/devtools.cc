#include "commons.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_urlrequest_capi.h"

// BROWSER PROCESS ONLY.

extern HWND rclient_window_;
extern cef_browser_t *browser_;
extern UINT REMOTE_DEBUGGING_PORT;
static wstr REMOTE_DEVTOOLS_URL{};

HWND devtools_window_ = nullptr;
LPCWSTR DEVTOOLS_WINDOW_NAME = L"DevTools - League Client";

void OpenDevTools_Internal(bool remote)
{
    if (remote)
    {
        if (REMOTE_DEBUGGING_PORT == 0) return;
        if (REMOTE_DEVTOOLS_URL.empty()) return;

        utils::openLink(REMOTE_DEVTOOLS_URL);
    }
    else if (browser_ != nullptr)
    {
        // This function can be called from non-UI thread,
        // so CefBrowserHost::HasDevTools has no effect.

        DWORD processId;
        GetWindowThreadProcessId(devtools_window_, &processId);

        if (processId == GetCurrentProcessId())
        {
            // Restore if minimized.
            if (IsIconic(devtools_window_))
                ShowWindow(devtools_window_, SW_RESTORE);
            else
                ShowWindow(devtools_window_, SW_SHOWNORMAL);

            SetForegroundWindow(devtools_window_);
        }
        else
        {
            cef_window_info_t wi{};
            wi.x = CW_USEDEFAULT;
            wi.y = CW_USEDEFAULT;
            wi.width = CW_USEDEFAULT;
            wi.height = CW_USEDEFAULT;
            wi.ex_style = WS_EX_APPWINDOW;
            wi.style = WS_OVERLAPPEDWINDOW
                | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
            wi.window_name = CefStr(DEVTOOLS_WINDOW_NAME).forward();

            cef_browser_settings_t settings{};
            auto host = browser_->get_host(browser_);
            host->show_dev_tools(host, &wi, new CefRefCount<cef_client_t>(nullptr), &settings, nullptr);
            //                              ^--- We use new client to keep DevTools
            //                                   from being scaled by League Client (e.g 0.8, 1.6).

            host->base.release(&host->base);
        }
    }
}

void PrepareDevTools()
{
    struct RequestClient : CefRefCount<cef_urlrequest_client_t>
    {
        cef_urlrequest_t *url_request_;
        str data_;

        RequestClient() : CefRefCount(this), url_request_(nullptr), data_{}
        {
            cef_urlrequest_client_t::on_request_complete = on_request_complete;
            cef_urlrequest_client_t::on_download_data = on_download_data;
        }

        ~RequestClient()
        {
            if (url_request_ != nullptr)
                url_request_->base.release(&url_request_->base);
        }

        void request()
        {
            str url{ "http://127.0.0.1:" };
            url.append(std::to_string(REMOTE_DEBUGGING_PORT));
            url.append("/json/list");

            auto request_ = cef_request_create();
            request_->set_url(request_, &CefStr(url));

            url_request_ = cef_urlrequest_create(request_, this, nullptr);
        }

        static void CEF_CALLBACK on_request_complete(struct _cef_urlrequest_client_t* _,
            struct _cef_urlrequest_t* request)
        {
            auto self = static_cast<RequestClient *>(_);

            auto response = self->url_request_->get_response(self->url_request_);
            auto status = self->url_request_->get_request_status(self->url_request_);

            if (status == UR_SUCCESS && response->get_status(response) == 200)
            {
                const CHAR pattern[] = "\"devtoolsFrontendUrl\": \"";
                if (auto pos = strstr(self->data_.c_str(), pattern))
                {
                    auto start = pos + sizeof(pattern) - 1;
                    auto end = strstr(start, "\"");

                    str link = "http://127.0.0.1:";
                    link.append(std::to_string(REMOTE_DEBUGGING_PORT));
                    link.append(str(start, end - start));

                    REMOTE_DEVTOOLS_URL = wstr(link.begin(), link.end());
                }
            }

            response->base.release(&response->base);
        }

        static void CEF_CALLBACK on_download_data(struct _cef_urlrequest_client_t* _,
            struct _cef_urlrequest_t* request,
            const void* data,
            size_t data_length)
        {
            auto self = static_cast<RequestClient *>(_);
            self->data_.append(static_cast<const char *>(data), data_length);
        }
    };

    if (REMOTE_DEBUGGING_PORT != 0)
    {
        auto client = new RequestClient();
        client->request();
    }
}