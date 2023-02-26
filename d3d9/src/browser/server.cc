#include "../internal.h"
#include <regex>

extern cef_browser_t *browser_;
static cef_server_t *server_;

class InternalServerHandler : public CefRefCount<cef_server_handler_t>
{
public:
    InternalServerHandler() : CefRefCount(this)
    {
        cef_server_handler_t::on_server_created = _on_server_created;
        cef_server_handler_t::on_server_destroyed = _on_server_destroyed;
        cef_server_handler_t::on_client_connected = _on_client_connected;
        cef_server_handler_t::on_client_disconnected = _on_client_disconnected;
        cef_server_handler_t::on_http_request = _on_http_request;
        cef_server_handler_t::on_web_socket_request = _on_web_socket_request;
        cef_server_handler_t::on_web_socket_connected = _on_web_socket_connected;
        cef_server_handler_t::on_web_socket_message = _on_web_socket_message;
    }

private:
    static void CEF_CALLBACK _on_server_created(struct _cef_server_handler_t* self,
        struct _cef_server_t* server)
    {
        auto addr = CefScopedStr{ server->get_address(server) }.cstr();
        size_t pos = addr.find(L":");

        if (pos != string::npos)
        {
            int port = wcstol(addr.substr(pos + 1).c_str(), nullptr, 10);

            auto frame = browser_->get_main_frame(browser_);
            auto message = CefProcessMessage_Create(&"__server_port"_s);
            auto args = message->get_argument_list(message);

            args->set_int(args, 0, port);
            frame->send_process_message(frame, PID_RENDERER, message);
        }

#if _DEBUG
        wprintf(L"internal server litening on: %s\n", addr.c_str());
#endif
    }

    static void CEF_CALLBACK _on_server_destroyed(struct _cef_server_handler_t* self,
        struct _cef_server_t* server)
    {
        server_ = nullptr;

#if _DEBUG
        wprintf(L"internal server closed.\n");
#endif
    }

    static void CEF_CALLBACK _on_client_connected(struct _cef_server_handler_t* self,
        struct _cef_server_t* server,
        int connection_id)
    {
    }

    static void CEF_CALLBACK _on_client_disconnected(struct _cef_server_handler_t* self,
        struct _cef_server_t* server,
        int connection_id)
    {
    }

    static void CEF_CALLBACK _on_http_request(struct _cef_server_handler_t* self,
        struct _cef_server_t* server,
        int connection_id,
        const cef_string_t* client_address,
        struct _cef_request_t* request)
    {
        wstring url = CefScopedStr{ request->get_url(request) }.cstr();
        CefScopedStr method{ request->get_method(request) };

        std::wregex callback_re( L"^(http://[\\d.]+:\\d+/callback/\\d+/)(.*)$" );
        std::wsmatch match;

        if (method == L"GET" && std::regex_search(url, match, callback_re))
        {
            // Close opened tab.
            string data = "<html><script>setTimeout(window.close, 200);</script></html>";
            server->send_http200response(server, connection_id, &"text/html"_s, data.c_str(), data.length());

            auto frame = browser_->get_main_frame(browser_);
            auto message = CefProcessMessage_Create(&"__auth_response"_s);
            auto args = message->get_argument_list(message);

            // Send response to renderer.
            args->set_string(args, 0, &CefStr(match[1].str()));
            args->set_string(args, 1, &CefStr(match[2].str()));
            frame->send_process_message(frame, PID_RENDERER, message);

            return;
        }

        server->send_http404response(server, connection_id);
    }

    static void CEF_CALLBACK _on_web_socket_request(struct _cef_server_handler_t* self,
        struct _cef_server_t* server,
        int connection_id,
        const cef_string_t* client_address,
        struct _cef_request_t* request,
        struct _cef_callback_t* callback)
    {
    }

    static void CEF_CALLBACK _on_web_socket_connected(
        struct _cef_server_handler_t* self,
        struct _cef_server_t* server,
        int connection_id)
    {
    }

    static void CEF_CALLBACK _on_web_socket_message(struct _cef_server_handler_t* self,
        struct _cef_server_t* server,
        int connection_id,
        const void* data,
        size_t data_size)
    {
    }
};

void OpenInternalServer()
{
    server_ = nullptr;
    CefServer_Create(&"127.0.0.1"_s, 0, 1024, new InternalServerHandler());
}

void CloseInternalServer()
{
    if (server_ != nullptr)
        server_->shutdown(server_);
}