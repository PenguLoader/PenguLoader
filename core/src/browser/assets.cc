#include "commons.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_scheme_capi.h"
#include "include/capi/cef_stream_capi.h"
#include "include/capi/cef_resource_handler_capi.h"

// BROWSER PROCESS ONLY.

static const set<wstr> KNOWN_ASSETS
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
        await new Promise(res => document.addEventListener('DOMContentLoaded', res));

    const url = import.meta.url.replace(/\?.*$/, '');
    const link = document.createElement('link');
    link.setAttribute('rel', 'stylesheet');
    link.setAttribute('href', url);

    document.body.appendChild(link);
})();
)";

static const auto SCRIPT_IMPORT_JSON = u8R"(
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
export default JSON.parse(content);
)";

static const auto SCRIPT_IMPORT_TOML = u8R"(
import { parse } from 'https://esm.sh/smol-toml@1.1.1';
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
export default parse(content);
)";

static const auto SCRIPT_IMPORT_YAML = u8R"(
import { load } from 'https://esm.sh/js-yaml@4.1.0';
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
export default load(content);
)";

static const auto SCRIPT_IMPORT_RAW = u8R"(
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
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
    IMPORT_TOML,
    IMPORT_YAML,
    IMPORT_RAW,
    IMPORT_URL
};

class ModuleStreamReader : public CefRefCount<cef_stream_reader_t>
{
public:
    ModuleStreamReader(ImportType type) : CefRefCount(this)
    {
        cef_bind_method(ModuleStreamReader, read);
        cef_bind_method(ModuleStreamReader, seek);
        cef_bind_method(ModuleStreamReader, tell);
        cef_bind_method(ModuleStreamReader, eof);
        cef_bind_method(ModuleStreamReader, may_block);

        data_.clear();

        switch (type)
        {
            case IMPORT_CSS:
                data_.assign(SCRIPT_IMPORT_CSS);
                break;
            case IMPORT_JSON:
                data_.assign(SCRIPT_IMPORT_JSON);
                break;
            case IMPORT_TOML:
                data_.assign(SCRIPT_IMPORT_TOML);
                break;
            case IMPORT_YAML:
                data_.assign(SCRIPT_IMPORT_YAML);
                break;
            case IMPORT_RAW:
                data_.assign(SCRIPT_IMPORT_RAW);
                break;
            case IMPORT_URL:
                data_.assign(SCRIPT_IMPORT_URL);
                break;
        }

        stream_ = cef_stream_reader_create_for_data(
            const_cast<char *>(data_.c_str()), data_.length());
    }

    ~ModuleStreamReader()
    {
        if (stream_ != nullptr)
            stream_->base.release(&stream_->base);

        data_.clear();
    }

private:
    cef_stream_reader_t *stream_;
    str data_;

    size_t _read(void* ptr, size_t size, size_t n)
    {
        return stream_->read(stream_, ptr, size, n);
    }

    int _seek(int64 offset, int whence)
    {
        return stream_->seek(stream_, offset, whence);
    }

    int64 _tell()
    {
        return stream_->tell(stream_);
    }

    int _eof()
    {
        return stream_->eof(stream_);
    }

    int _may_block()
    {
        return stream_->may_block(stream_);
    }
};

// Custom resource handler for local assets.
class AssetsResourceHandler : public CefRefCount<cef_resource_handler_t>
{
public:
    AssetsResourceHandler(const wstr &path)
        : CefRefCount(this)
        , path_(path)
        , mime_{}
        , stream_(nullptr)
        , length_(0)
        , offset_(0)
        , no_cache_(false)
        , is_opened_(false)
    {
        cef_bind_method(AssetsResourceHandler, open);
        cef_bind_method(AssetsResourceHandler, get_response_headers);
        cef_bind_method(AssetsResourceHandler, read);
        cef_bind_method(AssetsResourceHandler, skip);

        range_header_.clear();
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
    str range_header_;

    wstr path_;
    wstr mime_;
    bool no_cache_;
    bool is_opened_;

    int _open(cef_request_t* request, int* handle_request, cef_callback_t* callback)
    {
        //if (is_opened_)
        //{
        //    *handle_request = 0;
        //    return 0;
        //}

        is_opened_ = true;

        size_t pos;
        wstr query_part{};
        wstr path_ = this->path_;
        bool js_mime = false;

        // Check query part.
        if ((pos = path_.find(L'?')) != wstr::npos)
        {
            // Extract it.
            query_part = path_.substr(pos + 1);
            // Remove it from path.
            path_ = path_.substr(0, pos);
        }
        
        // Decode URI.
        decode_uri(path_);

        // Get final path.
        path_ = config::pluginsDir().append(path_);

        // Trailing slash.
        if (path_[path_.length() - 1] == L'/' || path_[path_.length() - 1] == L'\\')
        {
            js_mime = true;
            path_.append(L"index.js");
        }
        else
        {
            size_t pos = path_.find_last_of(L"//\\");
            wstr sub = path_.substr(pos + 1);

            // No extension.
            if (sub.find_last_of(L'.') == wstr::npos)
            {
                // peek .js
                if (js_mime = utils::isFile(path_ + L".js"))
                    path_.append(L".js");
                // peek folder
                else if (js_mime = utils::isDir(path_))
                    path_.append(L"/index.js");
            }
        }

        if (utils::isFile(path_))
        {
            auto import = IMPORT_DEFAULT;

            if (request->get_resource_type(request) == RT_SCRIPT)
            {
                static const std::wregex raw_pattern{ L"\\braw\\b" };
                static const std::wregex url_pattern{ L"\\burl\\b" };

                if (std::regex_search(query_part, url_pattern))
                    import = IMPORT_URL;
                else if (std::regex_search(query_part, raw_pattern))
                    import = IMPORT_RAW;
                else if ((pos = path_.find_last_of(L'.')) != wstr::npos)
                {
                    auto ext = path_.substr(pos + 1);
                    if (ext == L"css")
                        import = IMPORT_CSS;
                    else if (ext == L"json")
                        import = IMPORT_JSON;
                    else if (ext == L"toml")
                        import = IMPORT_TOML;
                    else if (ext == L"yml" || ext == L"yaml")
                        import = IMPORT_YAML;
                    else if (KNOWN_ASSETS.find(ext) != KNOWN_ASSETS.end())
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
                stream_ = cef_stream_reader_create_for_file(&CefStr(path_));
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
                mime_.assign(L"text/javascript");
                no_cache_ = true;
            }
            else if ((pos = path_.find_last_of(L'.')) != wstr::npos)
            {
                // Get MIME type from file extension.
                auto ext = path_.substr(pos + 1);
                CefScopedStr type{ cef_get_mime_type(&CefStr(ext)) };
                if (!type.empty())
                    mime_.assign(type.str, type.length);
            }
        }

#if _DEBUG
        wprintf(L"[%s] assets request: %s\n", stream_ != nullptr ? L"OK" : L"FAIL", path_.c_str());
        wprintf(L"  method: %s\n", CefScopedStr{ request->get_method(request) }.cstr().c_str());
        wprintf(L"  ref: %s\n", CefScopedStr{ request->get_referrer_url(request) }.cstr().c_str());
        wprintf(L"  res: %d, flags: %x\n", request->get_resource_type(request), request->get_flags(request));

        auto headers = cef_string_multimap_alloc();
        request->get_header_map(request, headers);

        for (int i = 0; i < cef_string_multimap_size(headers); i++)
        {
            cef_string_t key{};
            cef_string_multimap_key(headers, i, &key);

            wprintf(L"  %s:", key.str);

            for (int j = 0; j < cef_string_multimap_find_count(headers, &key); j++)
            {
                cef_string_t value{};
                cef_string_multimap_enumerate(headers, &key, j, &value);

                wprintf(L" %s,", value.str);
            }

            printf("\n");
        }

        printf("\n");
        cef_string_multimap_free(headers);
#endif

        CefScopedStr range{ request->get_header_by_name(request, &u"Range"_s) };
        if (!range.empty())
        {
            range_header_ = CefStrUtf8(range.ptr()).cstr();
        }

        *handle_request = 1;
        //callback->cont(callback);
        return 1;
    }

    void _get_response_headers(struct _cef_response_t* response, int64* response_length, cef_string_t* redirectUrl)
    {
        response->set_header_by_name(response, &u"Access-Control-Allow-Origin"_s, &u"*"_s, 1);

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
                response->set_mime_type(response, &CefStr(mime_));

            response->set_header_by_name(response, &u"Access-Control-Allow-Origin"_s, &u"*"_s, 1);

            if (no_cache_ || mime_ == L"text/javascript")
                response->set_header_by_name(response, &u"Cache-Control"_s, &u"no-store"_s, 1);
            else
            {
                response->set_header_by_name(response, &u"Cache-Control"_s, &u"max-age=31536000, immutable"_s, 1);
                set_etag(response, path_);
            }

            if (!range_header_.empty())
            {
                str contentRange;
                int contentLength;

                if (try_get_range_header(contentRange, contentLength))
                {
                    response->set_header_by_name(response, &u"Content-Length"_s, &CefStr(std::to_string(contentLength)), 1);
                    response->set_header_by_name(response, &u"Content-Range"_s, &CefStr(contentRange), 1);
                    response->set_header_by_name(response, &u"Accept-Ranges"_s, &CefStr("bytes"), 1);

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
                *response_length = length_;
            }
        }
    }

    int _skip(int64 bytes_to_skip, int64* bytes_skipped, cef_resource_skip_callback_t* callback)
    {
        if (stream_ == nullptr || stream_->eof(stream_))
        {
            *bytes_skipped = -2;
        }
        else if (stream_->tell(stream_) == (length_ - 1))
        {
            *bytes_skipped = 0;
        }
        else
        {
            int oldPosition = static_cast<int>(stream_->tell(stream_));
            int result = stream_->seek(stream_, bytes_to_skip, SEEK_CUR);
            int position = static_cast<int>(stream_->tell(stream_));
            *bytes_skipped = position - oldPosition;
            *bytes_skipped = bytes_to_skip;

            offset_ = position;
        }

        return *bytes_skipped > 0;
    }

    int _read(void* data_out, int bytes_to_read, int* bytes_read, struct _cef_resource_read_callback_t* callback)
    {
        int read = 0;
        *bytes_read = 0;

        if (stream_ == nullptr)
            return false;

        *bytes_read = static_cast<int>(stream_->read(stream_, data_out, 1, bytes_to_read));
        offset_ += *bytes_read;

        return (*bytes_read > 0);
    }

    bool try_get_range_header(str &contentRange, int &contentLength)
    {
        contentRange.clear();
        contentLength = 0;

        str range = range_header_.substr(6);

        int rangeStart = std::atoi(range.c_str());
        int rangeEnd = 0;

        size_t pos = range.rfind('-');
        if (pos != str::npos)
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
        size_t len = sprintf_s(buf, "bytes %d-%d/%d", rangeStart, rangeEnd, totalBytes);

        contentRange.assign(buf, len);
        contentLength = totalBytes - rangeStart;

        return true;
    }

    static void set_etag(cef_response_t *res, const wstr &path)
    {
        uint64_t hash = hash_fnv1a(path.c_str(), path.length() * sizeof(wstr::traits_type::char_type));

        char etag[64];
        size_t length = sprintf_s(etag, "\"%016llx\"", hash);

        res->set_header_by_name(res, &u"Etag"_s, &CefStr(etag, length), 1);
    }

    static uint64_t hash_fnv1a(const void *data, size_t len)
    {
        const uint8_t *bytes = (const uint8_t *)data;
        uint64_t hash = 14695981039346656037ULL;

        for (size_t i = 0; i < len; ++i)
        {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }

        return hash;
    }

    static void decode_uri(wstr &uri)
    {
        auto rule = cef_uri_unescape_rule_t(UU_SPACES | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

        cef_string_t input{ uri.data(), uri.length(), nullptr };
        CefScopedStr ret{ cef_uridecode(&input, true, rule) };

        uri.assign(ret.str, ret.length);
    }
};

struct AssetsSchemeHandlerFactory : CefRefCount<cef_scheme_handler_factory_t>
{
    AssetsSchemeHandlerFactory() : CefRefCount(this)
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
        wstr path = url.cstr().substr(15);

        return new AssetsResourceHandler(path);
    }
};

void RegisterAssetsSchemeHandlerFactory()
{
    CefStr scheme{ "https" };
    CefStr domain{ "plugins" };

    cef_register_scheme_handler_factory(&scheme, &domain,
        new AssetsSchemeHandlerFactory());
}
