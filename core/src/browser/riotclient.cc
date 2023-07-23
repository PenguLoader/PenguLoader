#include "commons.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_scheme_capi.h"
#include "include/capi/cef_urlrequest_capi.h"

// BROWSER PROCESS ONLY.

static wstr origin_;
static wstr authorization_;

static int min_(int a, int b)
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
        cef_resource_handler_t::open = _open;
        cef_resource_handler_t::process_request = _process_request;
        cef_resource_handler_t::get_response_headers = _get_response_headers;
        cef_resource_handler_t::skip = _skip;
        cef_resource_handler_t::read = _read;
    }

private:
    cef_frame_t *frame_;
    RiotClientURLRequestClient *client_;
    cef_urlrequest_t *url_request_;
    str data_;
    wstr path_;
    int64 bytes_read_;

    static int CEF_CALLBACK _open(cef_resource_handler_t *_,
        struct _cef_request_t* request, int* handle_request, struct _cef_callback_t* callback)
    {
        *handle_request = 0;
        return 0;
    }

    static int CEF_CALLBACK _process_request(cef_resource_handler_t *_,
        struct _cef_request_t* request, struct _cef_callback_t* callback)
    {
        auto self = static_cast<RiotClientResourceHandler *>(_);

        CefStr url{ origin_ + self->path_ };
        CefScopedStr method = request->get_method(request);
        auto body = request->get_post_data(request);
        auto headers = cef_string_multimap_alloc();
        request->get_header_map(request, headers);

        auto request_ = cef_request_create();
        request_->set(request_, &url, &method, body, headers);
        request_->set_header_by_name(request_, &L"Authorization"_s, &CefStr(authorization_), 1);

        self->client_ = new RiotClientURLRequestClient(&self->data_, callback);
        self->url_request_ = self->frame_->create_urlrequest(self->frame_, request_, self->client_);

        cef_string_multimap_free(headers);
        return 1;
    }

    static void CEF_CALLBACK _get_response_headers(cef_resource_handler_t *_,
        struct _cef_response_t* response, int64* response_length, cef_string_t* redirectUrl)
    {
        auto self = static_cast<RiotClientResourceHandler *>(_);
        
        // Forward response
        if (auto res = self->url_request_->get_response(self->url_request_))
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
        response->set_header_by_name(response, &L"Access-Control-Allow-Origin"_s, &L"*"_s, 1);
        *response_length = self->client_->response_length_;
    }

    bool Read(void *data_out, int bytes_to_read, int &bytes_read, cef_resource_read_callback_t *callback)
    {
        if ((client_->response_length_ > 0 && bytes_read_ >= client_->response_length_)
            || (bytes_read_ >= (int64)data_.length() && client_->done_))
        {
            bytes_read = 0;
            return false;
        }

        int read = min_(bytes_to_read, static_cast<int>(data_.length() - bytes_read_));
        memcpy(data_out, data_.c_str() + bytes_read_, read);

        bytes_read_ += read;
        bytes_read = read;
        return true;
    }

    static int CEF_CALLBACK _read(cef_resource_handler_t *_,
        void* data_out, int bytes_to_read, int* bytes_read, struct _cef_resource_read_callback_t* callback)
    {
        return static_cast<RiotClientResourceHandler *>(_)
            ->Read(data_out, bytes_to_read, *bytes_read, callback);
    }

    static int CEF_CALLBACK _skip(cef_resource_handler_t *_,
        int64 bytes_to_skip, int64* bytes_skipped, struct _cef_resource_skip_callback_t* callback)
    {
        *bytes_skipped = 0;
        return 1;
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
    cef_register_scheme_handler_factory(&L"https"_s,
        &CefStr("riotclient"), new RiotClientSchemeHandlerFactory());
}

void SetRiotClientCredentials(const wstr &appPort, const wstr &authToken)
{
    origin_.assign(L"https://127.0.0.1:");
    origin_.append(appPort);

    auto cred = L"riot:" + authToken;
    CefScopedStr base64 = cef_base64encode(str{ cred.begin(), cred.end() }.data(), cred.length());

    authorization_.assign(L"Basic ");
    authorization_.append(base64.cstr());
}