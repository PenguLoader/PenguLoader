#include "browser.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_scheme_capi.h"
#include "include/capi/cef_urlrequest_capi.h"

// BROWSER PROCESS ONLY.

static std::string url_origin_;
static std::string authorization_;

class RiotClientURLRequestClient : public CefRefCount<cef_urlrequest_client_t>
{
public:
    RiotClientURLRequestClient(std::string *data, cef_callback_t *response_callback)
        : CefRefCount(this), data_(data), done_(false), response_length_(-1), response_callback_(response_callback)
    {
        cef_urlrequest_client_t::on_request_complete = _on_request_complete;
        cef_urlrequest_client_t::on_download_progress = _on_download_progress;
        cef_urlrequest_client_t::on_download_data = _on_download_data;
    }

private:
    std::string *data_;
    cef_callback_t *response_callback_;
    int64 response_length_;
    bool done_;

    friend struct RiotClientResourceHandler;

    static void CEF_CALLBACK _on_request_complete(cef_urlrequest_client_t *_, struct _cef_urlrequest_t *request)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->done_ = true;
    }

    static void CEF_CALLBACK _on_download_progress(cef_urlrequest_client_t *_,
                                                   struct _cef_urlrequest_t *request, int64 current, int64 total)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->response_length_ = total;
    }

    static void CEF_CALLBACK _on_download_data(cef_urlrequest_client_t *_,
                                               struct _cef_urlrequest_t *request, const void *data, size_t data_length)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->data_->append(static_cast<const char *>(data), data_length);

        if (self->response_callback_)
        {
            self->response_callback_->cont(self->response_callback_);
            self->response_callback_ = nullptr;
        }
    }
};

struct RiotClientResourceHandler : CefRefCount<cef_resource_handler_t>
{
    RiotClientResourceHandler(cef_frame_t *frame)
        : CefRefCount(this), frame_(frame), bytes_read_(0), client_(nullptr), data_{}
    {
        cef_bind_method(RiotClientResourceHandler, open);
        cef_bind_method(RiotClientResourceHandler, process_request);
        cef_bind_method(RiotClientResourceHandler, get_response_headers);
        cef_bind_method(RiotClientResourceHandler, read);
    }

private:
    cef_frame_t *frame_;
    RiotClientURLRequestClient *client_;
    cef_urlrequest_t *url_request_;
    std::string data_;
    int64 bytes_read_;

    int _open(struct _cef_request_t *request, int *handle_request, struct _cef_callback_t *callback)
    {
        *handle_request = 0;
        return 0;
    }

    int _process_request(struct _cef_request_t *request, struct _cef_callback_t *callback)
    {
        CefScopedStr url{request->get_url(request)};
        CefScopedStr method{request->get_method(request)};

        std::u16string real_url;
        real_url.append(std::u16string(url_origin_.begin(), url_origin_.end()));
        real_url.append((char16_t *)url.str + 18, url.length - 18);
        cef_string_t url2{(char16 *)&real_url[0], real_url.length(), nullptr};

        auto body = request->get_post_data(request);
        auto headers = cef_string_multimap_alloc();
        request->get_header_map(request, headers);

        auto request2 = cef_request_create();
        request2->set(request2, &url2, &method, body, headers);
        request2->set_header_by_name(request2, &u"Authorization"_s, &CefStr(authorization_), 1);

        client_ = new RiotClientURLRequestClient(&data_, callback);
        url_request_ = frame_->create_urlrequest(frame_, request2, client_);

        cef_string_multimap_free(headers);
        return 1;
    }

    void _get_response_headers(struct _cef_response_t *response, int64 *response_length, cef_string_t *redirectUrl)
    {
        // Forward response
        if (auto res = url_request_->get_response(url_request_))
        {
            auto status = res->get_status(res);
            auto error = res->get_error(res);
            auto headers = cef_string_multimap_alloc();
            res->get_header_map(res, headers);

            response->set_header_map(response, headers);
            response->set_status(response, status);

            cef_string_multimap_free(headers);
        }

        // Bypass cors
        response->set_header_by_name(response, &u"Access-Control-Allow-Origin"_s, &u"*"_s, 1);
        *response_length = client_->response_length_;
    }

    int _read(void *data_out, int bytes_to_read, int *bytes_read, cef_resource_read_callback_t *callback)
    {
        if ((client_->response_length_ > 0 && bytes_read_ >= client_->response_length_) || (bytes_read_ >= (int64)data_.length() && client_->done_))
        {
            *bytes_read = 0;
            return false;
        }

        int read = min_(bytes_to_read, static_cast<int>(data_.length() - bytes_read_));
        memcpy(data_out, data_.c_str() + bytes_read_, read);

        bytes_read_ += read;
        *bytes_read = read;
        return true;
    }

    static inline int min_(int a, int b)
    {
        return a < b ? a : b;
    }
};

struct RiotClientSchemeHandlerFactory : CefRefCount<cef_scheme_handler_factory_t>
{
    RiotClientSchemeHandlerFactory() : CefRefCount(this)
    {
        cef_scheme_handler_factory_t::create = create;
    }

    static cef_resource_handler_t *CEF_CALLBACK create(
        struct _cef_scheme_handler_factory_t *self,
        struct _cef_browser_t *browser,
        struct _cef_frame_t *frame,
        const cef_string_t *scheme_name,
        struct _cef_request_t *request)
    {
        return new RiotClientResourceHandler(frame);
    }
};

void browser::register_riotclient_domain()
{
    if (!config::options::use_riotclient())
        return;

    auto scheme = u"https"_s;
    auto domain = u"riotclient"_s;
    auto factory = new RiotClientSchemeHandlerFactory();
    cef_register_scheme_handler_factory(&scheme, &domain, factory);
}

void browser::set_riotclient_credentials(const char *port, const char *token)
{
    if (!config::options::use_riotclient())
        return;

    url_origin_.assign("https://127.0.0.1:");
    url_origin_.append(port);

    char buffer[128];
    strcpy(buffer, "riot:");
    strcat(buffer, token);

    CefScopedStr base64{cef_base64encode(buffer, strlen(buffer))};
    authorization_.assign("Basic ");
    authorization_.append(base64.to_utf8());
}