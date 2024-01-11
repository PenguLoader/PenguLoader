#include "commons.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_scheme_capi.h"
#include "include/capi/cef_urlrequest_capi.h"

// BROWSER PROCESS ONLY.

static wstr origin_;
static wstr authorization_;

static inline int min_(int a, int b)
{
    return a < b ? a : b;
}

class RiotClientURLRequestClient : public CefRefCount<cef_urlrequest_client_t>
{
public:
    RiotClientURLRequestClient(str *data, cef_callback_t *response_callback)
        : CefRefCount(this)
        , data_(data)
        , done_(false)
        , response_length_(-1)
        , response_callback_(response_callback)
    {
        cef_urlrequest_client_t::on_request_complete = _on_request_complete;
        cef_urlrequest_client_t::on_download_progress = _on_download_progress;
        cef_urlrequest_client_t::on_download_data = _on_download_data;
    }

private:
    str *data_;
    cef_callback_t *response_callback_;
    int64 response_length_;
    bool done_;

    friend struct RiotClientResourceHandler;

    static void CEF_CALLBACK _on_request_complete(cef_urlrequest_client_t *_, struct _cef_urlrequest_t* request)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->done_ = true;
    }

    static void CEF_CALLBACK _on_download_progress(cef_urlrequest_client_t *_,
        struct _cef_urlrequest_t* request, int64 current, int64 total)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->response_length_ = total;
    }

    static void CEF_CALLBACK _on_download_data(cef_urlrequest_client_t *_,
        struct _cef_urlrequest_t* request, const void* data, size_t data_length)
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
    RiotClientResourceHandler(cef_frame_t *frame, const wstr &path)
        : CefRefCount(this)
        , frame_(frame)
        , path_(path)
        , bytes_read_(0)
        , client_(nullptr)
        , data_{}
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
    str data_;
    wstr path_;
    int64 bytes_read_;

    int _open(struct _cef_request_t* request, int* handle_request, struct _cef_callback_t* callback)
    {
        *handle_request = 0;
        return 0;
    }

    int _process_request(struct _cef_request_t* request, struct _cef_callback_t* callback)
    {
        CefStr url{ origin_ + path_ };
        CefScopedStr method = request->get_method(request);
        auto body = request->get_post_data(request);
        auto headers = cef_string_multimap_alloc();
        request->get_header_map(request, headers);

        auto request_ = cef_request_create();
        request_->set(request_, &url, &method, body, headers);
        request_->set_header_by_name(request_, &u"Authorization"_s, &CefStr(authorization_), 1);

        client_ = new RiotClientURLRequestClient(&data_, callback);
        url_request_ = frame_->create_urlrequest(frame_, request_, client_);

        cef_string_multimap_free(headers);
        return 1;
    }

    void _get_response_headers(struct _cef_response_t* response, int64* response_length, cef_string_t* redirectUrl)
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
        if ((client_->response_length_ > 0 && bytes_read_ >= client_->response_length_)
            || (bytes_read_ >= (int64)data_.length() && client_->done_))
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
};

struct RiotClientSchemeHandlerFactory : CefRefCount<cef_scheme_handler_factory_t>
{
    RiotClientSchemeHandlerFactory() : CefRefCount(this)
    {
        cef_scheme_handler_factory_t::create = create;
    }

    static cef_resource_handler_t* CEF_CALLBACK create(
        struct _cef_scheme_handler_factory_t* self,
        struct _cef_browser_t* browser,
        struct _cef_frame_t* frame,
        const cef_string_t* scheme_name,
        struct _cef_request_t* request)
    {
        CefScopedStr url{ request->get_url(request) };
        auto path = url.str + 18;

        return new RiotClientResourceHandler(frame, path);
    }
};

void RegisterRiotClientSchemeHandlerFactory()
{
    cef_register_scheme_handler_factory(&u"https"_s,
        &CefStr("riotclient"), new RiotClientSchemeHandlerFactory());
}

void SetRiotClientCredentials(const wstr &appPort, const wstr &authToken)
{
    origin_.assign(L"https://127.0.0.1:");
    origin_.append(appPort);

    str cred = "riot:";
    for (wchar_t c : authToken)
        cred.append(1, (char)c);

    CefScopedStr base64 = cef_base64encode(cred.data(), cred.length());

    authorization_.assign(L"Basic ");
    authorization_.append(base64.cstr());
}