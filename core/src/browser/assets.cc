#include "browser.h"
#include <functional>
#include <unordered_set>
#include "include/capi/cef_task_capi.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_scheme_capi.h"
#include "include/capi/cef_stream_capi.h"
#include "include/capi/cef_resource_handler_capi.h"

// BROWSER PROCESS ONLY.

template <typename T>
static constexpr uint32_t fnv32_1a(const T *in, size_t len)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        hash ^= in[i];
        hash *= 16777619u;
    }
    return hash;
}

static constexpr uint32_t operator""_hash(const char *in, size_t len)
{
    return fnv32_1a(in, len);
}

static const std::unordered_set<uint32> KNOWN_ASSETS_SET
{
    // images
    "bmp"_hash, "png"_hash,
    "jpg"_hash, "jpeg"_hash, "jfif"_hash,
    "pjpeg"_hash, "pjp"_hash, "gif"_hash,
    "svg"_hash, "ico"_hash, "webp"_hash,
    "avif"_hash,

    // media
    "mp4"_hash, "webm"_hash,
    "ogg"_hash, "mp3"_hash, "wav"_hash,
    "flac"_hash, "aac"_hash,

    // fonts
    "woff"_hash, "woff2"_hash,
    "eot"_hash, "ttf"_hash, "otf"_hash,
};

static const auto SCRIPT_IMPORT_CSS = R"(
(async function () {
    if (document.readyState !== 'complete')
        await new Promise(res => document.addEventListener('DOMContentLoaded', res));

    const url = import.meta.url.replace(/\?.*$/, '');
    const link = document.createElement('link');
    link.setAttribute('rel', 'stylesheet');
    link.setAttribute('href', url);

    document.body.appendChild(link);
})();
)";

static const auto SCRIPT_IMPORT_JSON = R"(
const url = import.meta.url.replace(/\?.*$/, '');
const json = await fetch(url).then(r => r.json());

json.$write = async (space) => {
    if (!space) {
        space = '\t';
    }
    await fetch(url, {
        method: 'PUT',
        body: JSON.stringify(json, null, space),
    });
};

export default json;
)";

static const auto SCRIPT_IMPORT_RAW = R"(
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
export default content;
)";

static const auto SCRIPT_IMPORT_URL = R"(
const url = import.meta.url.replace(/\?.*$/, '');
export default url;
)";

// Custom resource handler for local assets.
struct AssetsResourceHandler : CefRefCount<cef_resource_handler_t>
{
    AssetsResourceHandler()
        : CefRefCount(this)
        , stream_(nullptr)
        , offset_(0)
        , length_(0)
        , no_cache_(false)
    {
        cef_bind_method(AssetsResourceHandler, open);
        cef_bind_method(AssetsResourceHandler, get_response_headers);
        cef_bind_method(AssetsResourceHandler, skip);
        cef_bind_method(AssetsResourceHandler, read);
    }

    ~AssetsResourceHandler()
    {
        if (stream_ != nullptr)
            stream_->base.release(&stream_->base);
    }

private:
    cef_stream_reader_t *stream_;
    int64 offset_;
    int64 length_;
    std::string range_header_;
    std::u16string mime_;
    bool no_cache_;
public:

    int _open(cef_request_t* request, int* handle_request, cef_callback_t* callback)
    {
        size_t pos;
        bool js_mime = false;

        CefScopedStr url = request->get_url(request);
        std::u16string path, query_part;
        path.assign((char16_t *)url.str + 15, url.length - 15); // skip 'https://plugins'

        // Check query part.
        if ((pos = path.rfind('?')) != std::u16string::npos)
        {
            // Extract it.
            query_part = path.substr(pos + 1);
            // Remove it from path.
            path = path.substr(0, pos);
        }

        // Decode URI.
        decode_uri(path);

        // Get final path.
        path = config::plugins_dir().u16string().append(path);

        // Trailing slash.
        if (path[path.length() - 1] == '/' || path[path.length() - 1] == '\\')
        {
            js_mime = true;
            path.append(u"index.js");
        }
        else
        {
            size_t pos = path.find_last_of(u"//\\");
            std::u16string sub = path.substr(pos + 1);

            // No extension.
            if (sub.rfind('.') == std::u16string::npos)
            {
                // peek .js
                if ((js_mime = file::is_file(path + u".js")))
                    path.append(u".js");
                // peek folder
                else if ((js_mime = file::is_dir(path)))
                    path.append(u"/index.js");
            }
        }

        if (file::is_file(path))
        {
            const char *module_code = nullptr;
            if (request->get_resource_type(request) == RT_SCRIPT)
            {
                if (query_part == u"url")
                    module_code = SCRIPT_IMPORT_URL;
                else if (query_part == u"raw")
                    module_code = SCRIPT_IMPORT_RAW;
                else if ((pos = path.rfind('.')) != std::u16string::npos)
                {
                    auto ext = path.substr(pos + 1);
                    if (ext == u"css")
                        module_code = SCRIPT_IMPORT_CSS;
                    else if (ext == u"json")
                        module_code = SCRIPT_IMPORT_JSON;
                    else if (KNOWN_ASSETS_SET.find(fnv32_1a(ext.c_str(), ext.length())) != KNOWN_ASSETS_SET.end())
                        module_code = SCRIPT_IMPORT_URL;
                }
            }

            if (module_code != nullptr)
            {
                js_mime = true;
                stream_ = cef_stream_reader_create_for_data((void *)module_code, strlen(module_code));
            }
            else
            {
                stream_ = cef_stream_reader_create_for_file(&CefStr::wrap(path));
            }
        }

        if (stream_ != nullptr)
        {
            stream_->seek(stream_, 0, SEEK_END);
            length_ = stream_->tell(stream_);
            stream_->seek(stream_, 0, SEEK_SET);

            if (js_mime)
            {
                // Already known JavaScript module.
                mime_.assign(u"text/javascript");
                no_cache_ = true;
            }
            else if ((pos = path.rfind(u'.')) != std::u16string::npos)
            {
                // Get MIME type from file extension.
                auto ext = path.substr(pos + 1);
                CefScopedStr type{ cef_get_mime_type(&CefStr::wrap(ext)) };
                type.copy(mime_);
            }
        }

        // get range header
        CefScopedStr range{ request->get_header_by_name(request, &u"Range"_s) };
        if (!range.empty())
        {
            // save it
            range_header_.assign(range.to_utf8());
        }

        *handle_request = 1;
        //callback->cont(callback);
        return 1;
    }

    void _get_response_headers(struct _cef_response_t* response, int64* response_length, cef_string_t* redirectUrl)
    {
        response->set_header_by_name(response, &u"Access-Control-Allow-Origin"_s, &u"*"_s, 1);

        // File not found.
        if (stream_ == nullptr)
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
            if (!mime_.empty())
                response->set_mime_type(response, &CefStr::wrap(mime_));

            if (no_cache_ || mime_ == u"text/javascript")
                response->set_header_by_name(response, &u"Cache-Control"_s, &u"no-store"_s, 1);
            else
            {
                response->set_header_by_name(response, &u"Cache-Control"_s, &u"max-age=31536000, immutable"_s, 1);
                set_etag(response);
            }

            if (!range_header_.empty())
            {
                std::string contentRange;
                int contentLength;

                // parse range header
                if (try_get_range_header(contentRange, contentLength))
                {
                    response->set_header_by_name(response, &u"Accept-Ranges"_s, &CefStr("bytes"), 1);
                    response->set_header_by_name(response, &u"Content-Length"_s, &CefStr(std::to_string(contentLength)), 1);
                    response->set_header_by_name(response, &u"Content-Range"_s, &CefStr(contentRange), 1);

                    *response_length = contentLength;
                    response->set_status(response, 206);
                    response->set_status_text(response, &u"Partial Content"_s);
                }
                else
                {
                    *response_length = -1;
                    response->set_status(response, 416);
                    response->set_status_text(response, &u"Requested Range Not Satisfiable"_s);
                }
            }
            else
            {
                // normal content length
                *response_length = length_;
            }
        }
    }

    int _skip(int64 bytes_to_skip, int64 *bytes_skipped, struct _cef_resource_skip_callback_t *callback)
    {
        if (stream_ == nullptr || stream_->eof(stream_))
        {
            // eof
            *bytes_skipped = -2;
        }
        else if (stream_->tell(stream_) == (length_ - 1))
        {
            // done
            *bytes_skipped = 0;
        }
        else
        {
            int oldPosition = static_cast<int>(stream_->tell(stream_));
            int result = stream_->seek(stream_, bytes_to_skip, SEEK_CUR);
            int position = static_cast<int>(stream_->tell(stream_));
            *bytes_skipped = position - oldPosition;
            *bytes_skipped = bytes_to_skip;

            // skip
            offset_ = position;
        }

        return *bytes_skipped > 0;
    }

    int _read(void* data_out, int bytes_to_read, int* bytes_read, struct _cef_resource_read_callback_t* callback)
    {
        *bytes_read = 0;

        if (stream_ == nullptr)
            return false;

        int read = static_cast<int>(stream_->read(stream_, data_out, 1, bytes_to_read));
        *bytes_read = read;
        offset_ += read;

        return (*bytes_read > 0);
    }

    bool try_get_range_header(std::string &contentRange, int &contentLength)
    {
        contentRange.clear();
        contentLength = 0;
        
        // skip 'bytes='
        auto range = range_header_.substr(6);

        // 'start-end'
        int rangeStart = std::atoi(range.c_str());
        int rangeEnd = 0;

        size_t pos = range.rfind('-');
        if (pos != std::string::npos)
        {
            rangeEnd = std::atoi(range.substr(pos + 1).c_str());
        }

        int totalBytes = static_cast<int>(length_);
        if (totalBytes == 0)
            return false;

        if (rangeEnd == 0)
            rangeEnd = totalBytes - 1;

        if (rangeStart > rangeEnd)
            return false;

        if (rangeStart != offset_)
        {
            stream_->seek(stream_, rangeStart, SEEK_SET);
            offset_ = rangeStart;
        }

        char buf[64];
        size_t len = snprintf(buf, sizeof(buf) - 1, "bytes %d-%d/%d", rangeStart, rangeEnd, totalBytes);

        contentRange.assign(buf, len);
        contentLength = totalBytes - rangeStart;

        return true;
    }

    static void set_etag(cef_response_t *response)
    {
        CefScopedStr url = response->get_url(response);
        uint32_t hash = fnv32_1a(url.str, url.length);

        char etag[64];
        size_t etag_length = snprintf(etag, sizeof(etag) - 1, "\"%08x\"", hash);

        auto name = u"ETag"_s;
        CefStr value{ etag, etag_length };
        response->set_header_by_name(response, &name, &value, 1);
    }

    static void decode_uri(std::u16string &uri)
    {
        auto rule = cef_uri_unescape_rule_t(UU_SPACES
            | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

        cef_string_t input{ (char16 *)uri.data(), uri.length(), nullptr };
        CefScopedStr output{ cef_uridecode(&input, true, rule) };

        uri.assign((char16_t *)output.str, output.length);
    }
};

struct PluginBackgroundTask : CefRefCount<cef_task_t>
{
    std::function<void()> func_;

    PluginBackgroundTask(std::function<void()> func) : CefRefCount(this), func_(func)
    {
        cef_bind_method(PluginBackgroundTask, execute);
    }

    void CEF_CALLBACK _execute()
    {
        func_();
    }
};

struct PluginResourceWriter : CefRefCount<cef_resource_handler_t>
{
    void *data_;
    size_t length_;
    cef_stream_writer_t *writer_;

    PluginResourceWriter() : CefRefCount(this), data_(nullptr), length_(0), writer_(nullptr)
    {
        cef_bind_method(PluginResourceWriter, open);
        cef_bind_method(PluginResourceWriter, get_response_headers);
    }

    ~PluginResourceWriter()
    {
        if (writer_)
            writer_->base.release(&writer_->base);

        if (data_)
            free(data_);
    }

    void parse_path(cef_request_t *request, std::u16string &path)
    {
        CefScopedStr url = request->get_url(request);
        path.assign((char16_t *)url.str + 15, url.length - 15);

        size_t pos;
        if ((pos = path.rfind('?')) != std::u16string::npos)
            path = path.substr(0, pos);

        AssetsResourceHandler::decode_uri(path);
        path = config::plugins_dir().u16string().append(path);
    }

    void parse_body(cef_request_t *request)
    {
        auto body = request->get_post_data(request);
        size_t count = body->get_element_count(body);
        auto elements = new cef_post_data_element_t *[count];

        memset(elements, 0, count * sizeof(cef_post_data_element_t *));
        body->get_elements(body, &count, elements);

        length_ = elements[0]->get_bytes_count(elements[0]);
        data_ = malloc(length_);
        elements[0]->get_bytes(elements[0], length_, data_);

        body->base.release(&body->base);
    }

    int CEF_CALLBACK _open(cef_request_t *request, int *handle_request, cef_callback_t *callback)
    {
        std::u16string path;
        parse_path(request, path);
        parse_body(request);

        cef_string_t _path = CefStr::wrap(path);
        writer_ = cef_stream_writer_create_for_file(&_path);

        cef_post_task(TID_FILE_BACKGROUND, new PluginBackgroundTask([this, callback]
            {
                writer_->write(writer_, data_, 1, length_);
                callback->cont(callback);
            }));

        *handle_request = false;
        return 1;
    }

    void CEF_CALLBACK _get_response_headers(cef_response_t *response, int64 *response_length, cef_string_t *)
    {
        response->set_header_by_name(response, &u"Access-Control-Allow-Origin"_s, &u"*"_s, 1);
        *response_length = 0;
    }
};

struct AssetsSchemeHandlerFactory : CefRefCount<cef_scheme_handler_factory_t>
{
    AssetsSchemeHandlerFactory() : CefRefCount(this)
    {
        cef_bind_method(AssetsSchemeHandlerFactory, create);
    }

    cef_resource_handler_t* CEF_CALLBACK _create(
        struct _cef_browser_t* browser,
        struct _cef_frame_t* frame,
        const cef_string_t* scheme_name,
        struct _cef_request_t* request)
    {
        CefScopedStr method = request->get_method(request);
        if (method.equal("PUT"))
        {
            return new PluginResourceWriter();
        }

        return new AssetsResourceHandler();
    }
};

void browser::register_plugins_domain(cef_request_context_t *ctx)
{
    auto scheme = u"https"_s;
    auto domain = u"plugins"_s;
    auto factory = new AssetsSchemeHandlerFactory();

    ctx->register_scheme_handler_factory(ctx, &scheme, &domain, factory);
}