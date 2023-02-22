#include "../internal.h"
#include <regex>
#include <unordered_set>

// BROWSER PROCESS ONLY.

static const std::unordered_set<wstring> known_assets
{
    // images
    L"bmp", L"png",
    L"jpg", L"jpeg", L"jfif",
    L"pjpeg", L"pjp", L"gif",
    L"svg", L"ico", L"webp",

    // media
    L"avif", L"mp4", L"webm",
    L"ogg", L"mp3", L"wav",
    L"flac", L"aac",

    // fonts
    L"woff", L"woff2",
    L"eot", L"ttf", L"otf",
};

static const auto SCRIPT_IMPORT_CSS = u8R"(
(async function () {
    if (document.readyState !== 'complete')
        await new Promise(res => window.addEventListener('load', res));

    const url = import.meta.url.replace(/\?.*$/, '');
    const link = document.createElement('link');
    link.setAttribute('rel', 'stylesheet');
    link.setAttribute('href', url);

    document.body.appendChild(link);
})();
)";

static const auto SCRIPT_IMPORT_JSON = u8R"(
const url = import.meta.url.replace(/\?.*$/, '');
const content = window.requireFile(url);
export default JSON.parse(content);
)";

static const auto SCRIPT_IMPORT_RAW = u8R"(
const url = import.meta.url.replace(/\?.*$/, '');
const content = window.requireFile(url);
export default content;
)";

static const auto SCRIPT_IMPORT_URL = u8R"(
const url = import.meta.url.replace(/\?.*$/, '');
export default url;
)";

enum ImportType
{
    IMPORT_DEFAULT = 0,
    IMPORT_CSS,
    IMPORT_JSON,
    IMPORT_RAW,
    IMPORT_URL
};

class ModuleStreamReader : public CefRefCount<cef_stream_reader_t>
{
public:
    ModuleStreamReader(ImportType type) : CefRefCount(this), data_{}
    {
        cef_stream_reader_t::read = _read;
        cef_stream_reader_t::seek = _seek;
        cef_stream_reader_t::tell = _tell;

        switch (type)
        {
            case IMPORT_CSS:
                data_.assign(SCRIPT_IMPORT_CSS);
                break;
            case IMPORT_JSON:
                data_.assign(SCRIPT_IMPORT_JSON);
                break;
            case IMPORT_RAW:
                data_.assign(SCRIPT_IMPORT_RAW);
                break;
            case IMPORT_URL:
                data_.assign(SCRIPT_IMPORT_URL);
                break;
        }

        stream_ = CefStreamReader_CreateForData(
            const_cast<char *>(data_.c_str()), data_.length());
    }

    ~ModuleStreamReader()
    {
        data_.clear();
        if (stream_ != nullptr)
            stream_->base.release(&stream_->base);
    }

private:
    string data_;
    cef_stream_reader_t *stream_;

    static size_t CEF_CALLBACK _read(struct _cef_stream_reader_t* _,
        void* ptr,
        size_t size,
        size_t n)
    {
        auto self = static_cast<ModuleStreamReader *>(_);
        return self->stream_->read(self->stream_, ptr, size, n);
    }

    static int CEF_CALLBACK _seek(struct _cef_stream_reader_t* _,
        int64 offset,
        int whence)
    {
        auto self = static_cast<ModuleStreamReader *>(_);
        return self->stream_->seek(self->stream_, offset, whence);
    }

    static int64 CEF_CALLBACK _tell(struct _cef_stream_reader_t* _)
    {
        auto self = static_cast<ModuleStreamReader *>(_);
        return self->stream_->tell(self->stream_);
    }
};

// Custom resource handler for local assets.
class AssetsResourceHandler : public CefRefCount<cef_resource_handler_t>
{
public:
    AssetsResourceHandler(const wstring &path, bool plugin) : CefRefCount(this)
        , path_(path)
        , mime_{}
        , stream_(nullptr)
        , length_(0)
        , is_plugin_(plugin)
        , no_cache_(false)
    {
        cef_resource_handler_t::open = _Open;
        cef_resource_handler_t::process_request = _ProcessRequest;
        cef_resource_handler_t::get_response_headers = _GetResponseHeaders;
        cef_resource_handler_t::skip = _Skip;
        cef_resource_handler_t::read = _Read;
        cef_resource_handler_t::read_response = _ReadResponse;
        cef_resource_handler_t::cancel = _Cancel;
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
    wstring mime_;
    bool is_plugin_;
    bool no_cache_;

    int CEF_CALLBACK Open(cef_request_t* request, int* handle_request, cef_callback_t* callback)
    {
        size_t pos;
        wstring query_part{};
        wstring path_ = this->path_;
        bool js_mime = false;

        // Check query part.
        if ((pos = path_.find(L'?')) != string::npos)
        {
            // Extract it.
            query_part = path_.substr(pos + 1);
            // Remove it from path.
            path_ = path_.substr(0, pos);
        }

        // Get final path.
        if (is_plugin_)
        {
            path_ = config::getPluginsDir().append(path_);

            // Trailing slash.
            if (path_[path_.length() - 1] == '/' || path_[path_.length() - 1] == L'\\')
            {
                js_mime = true;
                path_.append(L"index.js");
            }
            else
            {
                size_t pos = path_.find_last_of(L"//\\");
                wstring sub = path_.substr(pos + 1);

                // No extension.
                if (sub.find_last_of(L'.') == wstring::npos)
                {
                    // peek .js
                    if (js_mime = utils::fileExist(path_ + L".js"))
                        path_.append(L".js");
                    // peek folder
                    else if (js_mime = utils::dirExist(path_))
                        path_.append(L"/index.js");
                }
            }

            if (utils::fileExist(path_))
            {
                auto import = IMPORT_DEFAULT;

                static const std::wregex module_pattern{ L"^https:\\/\\/plugins.*\\.js(?:\\?.*)?$" };
                CefScopedStr referer{ request->get_referrer_url(request) };

                // Detect relative plugin imports by referer //plugins.
                if (!referer.empty() && std::regex_search(wstring(referer.str, referer.length), module_pattern))
                {
                    static const std::wregex raw_pattern{ L"\\braw\\b" };
                    static const std::wregex url_pattern{ L"\\burl\\b" };

                    if (std::regex_search(query_part, url_pattern))
                        import = IMPORT_URL;
                    else if (std::regex_search(query_part, raw_pattern))
                        import = IMPORT_RAW;
                    else if ((pos = path_.find_last_of(L'.')) != string::npos)
                    {
                        auto ext = path_.substr(pos + 1);
                        if (ext == L"css")
                            import = IMPORT_CSS;
                        else if (ext == L"json")
                            import = IMPORT_JSON;
                        else if (known_assets.find(ext) != known_assets.end())
                            import = IMPORT_URL;
                    }
                }

                if (import != IMPORT_DEFAULT)
                {
                    js_mime = true;
                    stream_ = new ModuleStreamReader(import);
                }
                else
                {
                    stream_ = CefStreamReader_CreateForFile(&CefStr(path_));
                }
            }
        }
        else
        {
            path_ = config::getAssetsDir().append(path_);
            stream_ = CefStreamReader_CreateForFile(&CefStr(path_));
        }

        if (stream_ != nullptr)
        {
            stream_->seek(stream_, 0, SEEK_END);
            length_ = stream_->tell(stream_);
            stream_->seek(stream_, 0, SEEK_SET);

            if (js_mime)
            {
                // Already known JavaScript module.
                mime_.assign(L"text/javascript");
                no_cache_ = true;
            }
            else if ((pos = path_.find_last_of(L'.')) != string::npos)
            {
                // Get MIME type from file extension.
                auto ext = path_.substr(pos + 1);
                CefScopedStr type{ CefGetMimeType(&CefStr(ext)) };
                if (!type.empty())
                    mime_.assign(type.str, type.length);
            }
        }

        *handle_request = true;
        callback->cont(callback);
        return true;
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
        }
        else
        {
            response->set_status(response, 200);
            response->set_error(response, ERR_NONE);

            // Set MIME type.
            if (!self->mime_.empty())
                response->set_mime_type(response, &CefStr(self->mime_));

            response->set_header_by_name(response, &"Access-Control-Allow-Origin"_s, &"*"_s, 1);

            if (self->no_cache_ || self->mime_ == L"text/javascript")
                response->set_header_by_name(response, &"Cache-Control"_s, &"no-cache, no-store, must-revalidate"_s, 1);
            else
                response->set_header_by_name(response, &"Cache-Control"_s, &"public, max-age=86400"_s, 1);

            *response_length = self->length_;
        }
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

    static int CEF_CALLBACK _Open(cef_resource_handler_t* _,
        struct _cef_request_t* request,
        int* handle_request,
        struct _cef_callback_t* callback)
    {
        return static_cast<AssetsResourceHandler *>(_)->Open(request, handle_request, callback);
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
