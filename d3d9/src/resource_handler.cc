#include "internal.h"
#include <unordered_map>

using namespace league_loader;

// BROWSER PROCESS ONLY.

// Custom resource handler for assets
struct CefResourceHandler : CefRefCount<cef_resource_handler_t>
{
    CefResourceHandler(const std::wstring &path)
        : CefRefCount(this),
        path_(path), mime_{}, stream_(nullptr)
    {
        open = (decltype(cef_resource_handler_t::open))_Open;
        process_request = (decltype(cef_resource_handler_t::process_request))_ProcessRequest;
        get_response_headers = (decltype(cef_resource_handler_t::get_response_headers))_GetResponseHeaders;
        skip = (decltype(cef_resource_handler_t::skip))_Skip;
        read = (decltype(cef_resource_handler_t::read))_Read;
        read_response = (decltype(cef_resource_handler_t::read_response))_ReadResponse;
        cancel = (decltype(cef_resource_handler_t::cancel))_Cancel;
    }

private:
    cef_stream_reader_t *stream_;
    std::wstring path_;
    std::wstring mime_;

    static int CEF_CALLBACK _Open(CefResourceHandler* self,
        struct _cef_request_t* request,
        int* handle_request,
        struct _cef_callback_t* callback)
    {
        size_t pos;
        auto path = self->path_;

        // Remove query part
        if ((pos = path.find_last_of(L'?')) != std::string::npos)
            path = path.substr(0, pos);
        
        // Extract extension to get MIME
        if ((pos = path.find_last_of(L'.')) != std::string::npos)
        {
            auto ext = path.substr(pos + 1);
            auto type = CefGetMimeType(&CefStr(ext));

            if (type != nullptr)
            {
                self->mime_.assign(type->str, type->length);
                CefString_UserFree_Free(type);
            }
        }

        path = GetAssetsDir().append(path);
        self->stream_ = CefStreamReader_CreateForFile(&CefStr(path));

        *handle_request = 1;
        return 1;
    }

    static void CEF_CALLBACK _GetResponseHeaders(CefResourceHandler* self,
        struct _cef_response_t* response,
        int64* response_length,
        cef_string_t* redirectUrl)
    {
        if (self->stream_ == nullptr)
        {
            // File not found
            response->set_status(response, 404);
            response->set_error(response, ERR_FILE_NOT_FOUND);

            *response_length = -1;
        }
        else
        {
            response->set_status(response, 200);
            response->set_status_text(response, &CefStr("OK"));

            if (!self->mime_.empty())
                response->set_mime_type(response, &CefStr(self->mime_));

            response->set_error(response, ERR_NONE);

            auto stream = self->stream_;
            stream->seek(stream, 0, SEEK_END);
            *response_length = stream->tell(stream);
            stream->seek(stream, 0, SEEK_SET);

            response->set_header_by_name(response, &CefStr("Access-Control-Allow-Origin"), &CefStr("*"), 1);
            response->set_header_by_name(response, &CefStr("Cache-Control"), &CefStr("public, max-age=86400"), 1);
            //response->set_header_by_name(response, &CefStr("ETag"), &CefStr("\"" + random_string(28) + "\""), 1);
        }
    }

    static int CEF_CALLBACK _Read(CefResourceHandler* self,
        void* data_out,
        int bytes_to_read,
        int* bytes_read,
        struct _cef_resource_read_callback_t* callback)
    {
        int read = 0;
        auto stream = self->stream_;
        *bytes_read = 0;

        do
        {
            read = static_cast<int>(stream->read(stream, static_cast<char*>(data_out) + *bytes_read, 1, bytes_to_read - *bytes_read));
            *bytes_read += read;
        } while (read != 0 && *bytes_read < bytes_to_read);

        return (*bytes_read > 0);
    }

    // Deprecated
    static int CEF_CALLBACK _ProcessRequest(CefResourceHandler* self,
        struct _cef_request_t* request,
        struct _cef_callback_t* callback) { return 0; }

    // Ignored
    static int CEF_CALLBACK _Skip(CefResourceHandler* self,
        int64 bytes_to_skip,
        int64* bytes_skipped,
        struct _cef_resource_skip_callback_t* callback) { return 0; }

    // Deprecated
    static int CEF_CALLBACK _ReadResponse(CefResourceHandler* self,
        void* data_out,
        int bytes_to_read,
        int* bytes_read,
        struct _cef_callback_t* callback) { return 0; }

    static void CEF_CALLBACK _Cancel(CefResourceHandler* self)
    {
    }
};

cef_resource_handler_t *league_loader::CreateAssetsHandler(const std::wstring &path)
{
    return new CefResourceHandler(path);
}