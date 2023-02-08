#include "../internal.h"

#include <algorithm>
#include "include/capi/cef_urlrequest_capi.h"

// BROWSER PROCESS ONLY.

static wstring m_rcOrigin{};
static wstring m_rcAuthorization{};

class RiotClientURLRequestClient : public CefRefCount<cef_urlrequest_client_t>
{
public:
    RiotClientURLRequestClient(string *data, cef_callback_t *response_callback)
        : CefRefCount(this), data_(data), done_(false), response_length_(-1), response_callback_(response_callback)
    {
        cef_urlrequest_client_t::on_request_complete = on_request_complete;
        cef_urlrequest_client_t::on_upload_progress = on_upload_progress;
        cef_urlrequest_client_t::on_download_progress = on_download_progress;
        cef_urlrequest_client_t::on_download_data = on_download_data;
        cef_urlrequest_client_t::get_auth_credentials = get_auth_credentials;
    }

private:
    string *data_;
    cef_callback_t *response_callback_;
    int64 response_length_;
    bool done_;

    friend struct RiotClientResourceHandler;

    static void CEF_CALLBACK on_request_complete(cef_urlrequest_client_t *_, struct _cef_urlrequest_t* request)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->done_ = true;
    }

    static void CEF_CALLBACK on_upload_progress(cef_urlrequest_client_t *_,
        struct _cef_urlrequest_t* request, int64 current, int64 total)
    {
    }

    static void CEF_CALLBACK on_download_progress(cef_urlrequest_client_t *_,
        struct _cef_urlrequest_t* request, int64 current, int64 total)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->response_length_ = total;
    }

    static void CEF_CALLBACK on_download_data(cef_urlrequest_client_t *_,
        struct _cef_urlrequest_t* request, const void* data, size_t data_length)
    {
        auto self = static_cast<RiotClientURLRequestClient *>(_);
        self->data_->append(static_cast<const char *>(data), data_length);

        if (self->response_callback_) {
            self->response_callback_->cont(self->response_callback_);
            self->response_callback_ = nullptr;
        }
    }

    static int CEF_CALLBACK get_auth_credentials(cef_urlrequest_client_t *_,
        int isProxy, const cef_string_t* host, int port, const cef_string_t* realm,
        const cef_string_t* scheme, struct _cef_auth_callback_t* callback)
    {
        return false;
    }
};

struct RiotClientResourceHandler : CefRefCount<cef_resource_handler_t>
{
    RiotClientResourceHandler(cef_frame_t *frame, const wstring &path)
        : CefRefCount(this), frame_(frame), path_(path), bytes_read_(0), client_(nullptr), data_{}
    {
        cef_resource_handler_t::open = _open;
        cef_resource_handler_t::process_request = _process_request;
        cef_resource_handler_t::get_response_headers = _get_response_headers;
        cef_resource_handler_t::skip = _skip;
        cef_resource_handler_t::read = _read;
        cef_resource_handler_t::read_response = _read_response;
        cef_resource_handler_t::cancel = _cancel;
    }

private:
    cef_frame_t *frame_;
    RiotClientURLRequestClient *client_;
    cef_urlrequest_t *url_request_;
    string data_;
    wstring path_;
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

        auto url = CefStr(m_rcOrigin + self->path_);
        auto method = CefStr(request->get_method(request));
        auto body = request->get_post_data(request);
        auto headers = CefStringMultimap_Alloc();
        request->get_header_map(request, headers);

        auto request_ = CefRequest_Create();
        request_->set(request_, &url, &method, body, headers);
        request_->set_header_by_name(request_, &"Authorization"_s, &CefStr(m_rcAuthorization), 1);

        self->client_ = new RiotClientURLRequestClient(&self->data_, callback);
        self->url_request_ = self->frame_->create_urlrequest(self->frame_, request_, self->client_);

        CefStringMultimap_Free(headers);
        return 1;
    }

    static void CEF_CALLBACK _get_response_headers(cef_resource_handler_t *_,
        struct _cef_response_t* response, int64* response_length, cef_string_t* redirectUrl)
    {
        auto self = static_cast<RiotClientResourceHandler *>(_);

        if (auto res = self->url_request_->get_response(self->url_request_))
        {
            auto status = res->get_status(res);
            auto error = res->get_error(res);
            auto headers = CefStringMultimap_Alloc();
            res->get_header_map(res, headers);

            response->set_header_map(response, headers);
            response->set_status(response, status);

            CefStringMultimap_Free(headers);
        }

        response->set_header_by_name(response, &"Access-Control-Allow-Origin"_s, &"*"_s, 1);
        *response_length = self->client_->response_length_;
    }

    bool Read(void *data_out, int bytes_to_read, int &bytes_read, cef_resource_read_callback_t *callback)
    {
        if ((client_->response_length_ > 0 && bytes_read_ >= client_->response_length_)
            || (bytes_read_ >= data_.length() && client_->done_))
        {
            bytes_read = 0;
            return false;
        }

        int read = std::min(bytes_to_read, static_cast<int>(data_.length() - bytes_read_));
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

    static int CEF_CALLBACK _read_response(cef_resource_handler_t *_,
        void* data_out, int bytes_to_read, int* bytes_read, struct _cef_callback_t* callback)
    {
        return 0;
    }

    static void CEF_CALLBACK _cancel(struct _cef_resource_handler_t* self) { }
};

cef_resource_handler_t *CreateRiotClientResourceHandler(cef_frame_t *frame, wstring path)
{
    return new RiotClientResourceHandler(frame, path);
}

void SetRiotClientCredentials(const wstring &appPort, const wstring &authToken)
{
    m_rcOrigin.assign(L"https://127.0.0.1:");
    m_rcOrigin.append(appPort);

    m_rcAuthorization.assign(L"Basic ");
    m_rcAuthorization.append(utils::encodeBase64(L"riot:" + authToken));
}