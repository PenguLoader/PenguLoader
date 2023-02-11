#include "../internal.h"

// BROWSER PROCESS ONLY.

// Custom resource handler for local assets.
class AssetsResourceHandler : public CefRefCount<cef_resource_handler_t>
{
public:
    AssetsResourceHandler(const wstring &path, bool plugin) : CefRefCount(this),
        path_(path), stream_(nullptr), length_(0), is_plugin_(plugin)
    {
        cef_resource_handler_t::open = _Open;
        cef_resource_handler_t::process_request = _ProcessRequest;
        cef_resource_handler_t::get_response_headers = _GetResponseHeaders;
        cef_resource_handler_t::skip = _Skip;
        cef_resource_handler_t::read = _Read;
        cef_resource_handler_t::read_response = _ReadResponse;
        cef_resource_handler_t::cancel = _Cancel;

        // Remove query part.
        size_t pos;
        if ((pos = path_.find_last_of(L'?')) != string::npos)
            path_ = path_.substr(0, pos);
    }

    ~AssetsResourceHandler()
    {
        if (stream_ != nullptr)
            stream_->base.release(&stream_->base);
    }

private:
    cef_stream_reader_t *stream_;
    int64 length_;
    wstring path_;
    bool is_plugin_;

    static int CEF_CALLBACK _Open(cef_resource_handler_t* _,
        struct _cef_request_t* request,
        int* handle_request,
        struct _cef_callback_t* callback)
    {
        *handle_request = 1;

        auto self = static_cast<AssetsResourceHandler *>(_);
        auto &path = self->path_;
        auto &stream = self->stream_;

        // Get final path.
        if (self->is_plugin_)
        {
            path = config::getPluginsDir().append(path);

            // Trailing slash.
            if (path[path.length() - 1] == '/' || path[path.length() - 1] == L'\\')
            {
                path.append(L"index.js");
            }
            else
            {
                size_t pos = path.find_last_of(L"//\\");
                wstring sub = path.substr(pos + 1);

                // No extension.
                if (sub.find_last_of(L'.') == wstring::npos)
                {
                    // peek .js
                    if (utils::fileExist(path + L".js"))
                        path.append(L".js");
                    // peek folder
                    else if (utils::dirExist(path))
                        path.append(L"/index.js");
                }
            }
        }
        else
        {
            path = config::getAssetsDir().append(path);
        }

        stream = CefStreamReader_CreateForFile(&CefStr(path));

        if (stream != nullptr)
        {
            stream->seek(stream, 0, SEEK_END);
            self->length_ = stream->tell(stream);
            stream->seek(stream, 0, SEEK_SET);
        }

        callback->cont(callback);
        return 1;
    }

    static void CEF_CALLBACK _GetResponseHeaders(cef_resource_handler_t* _,
        struct _cef_response_t* response,
        int64* response_length,
        cef_string_t* redirectUrl)
    {
        auto self = static_cast<AssetsResourceHandler *>(_);

        // File not found.
        if (self->stream_ == nullptr)
        {
            response->set_status(response, 404);
            response->set_error(response, ERR_FILE_NOT_FOUND);

            *response_length = -1;
            return;
        }

        // Extract extension to get MIME type.
        size_t pos;
        if ((pos = self->path_.find_last_of(L'.')) != string::npos)
        {
            auto ext = self->path_.substr(pos + 1);
            auto type = CefGetMimeType(&CefStr(ext));

            if (type != nullptr)
            {
                response->set_mime_type(response, type);
                CefString_UserFree_Free(type);
            }
        }

        response->set_status(response, 200);
        response->set_error(response, ERR_NONE);

        response->set_header_by_name(response, &"Access-Control-Allow-Origin"_s, &"*"_s, 1);
        response->set_header_by_name(response, &"Cache-Control"_s, &"public, max-age=86400"_s, 1);

        *response_length = self->length_;
    }

    static int CEF_CALLBACK _Read(cef_resource_handler_t* _,
        void* data_out,
        int bytes_to_read,
        int* bytes_read,
        struct _cef_resource_read_callback_t* callback)
    {
        auto self = static_cast<AssetsResourceHandler *>(_);

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
    static int CEF_CALLBACK _ProcessRequest(cef_resource_handler_t* self,
        struct _cef_request_t* request,
        struct _cef_callback_t* callback) { return 0; }

    // Ignored
    static int CEF_CALLBACK _Skip(cef_resource_handler_t* self,
        int64 bytes_to_skip,
        int64* bytes_skipped,
        struct _cef_resource_skip_callback_t* callback) { return 0; }

    // Deprecated
    static int CEF_CALLBACK _ReadResponse(cef_resource_handler_t* self,
        void* data_out,
        int bytes_to_read,
        int* bytes_read,
        struct _cef_callback_t* callback) { return 0; }

    static void CEF_CALLBACK _Cancel(cef_resource_handler_t* self) { }
};

cef_resource_handler_t *CreateAssetsResourceHandler(const wstring &path, bool plugin)
{
    return new AssetsResourceHandler(path, plugin);
}