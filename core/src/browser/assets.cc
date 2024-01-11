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
    L"avif",

    // media
    L"mp4", L"webm",
    L"ogg", L"mp3", L"wav",
    L"flac", L"aac",

    // fonts
    L"woff", L"woff2",
    L"eot", L"ttf", L"otf",
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
const content = await fetch(url).then(r => r.text());
export default JSON.parse(content);
)";

static const auto SCRIPT_IMPORT_TOML = R"(
const { parse } = __p('toml');
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
export default parse(content);
)";

static const auto SCRIPT_IMPORT_YAML = R"(
const { parse } = __p('yaml');
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
export default parse(content);
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
class AssetsResourceHandler : public CefRefCount<cef_resource_handler_t>
{
public:
    AssetsResourceHandler(const wstr &path)
        : CefRefCount(this)
        , path_(path)
        , mime_{}
        , stream_(nullptr)
        , length_(0)
        , no_cache_(false)
    {
        cef_bind_method(AssetsResourceHandler, open);
        cef_bind_method(AssetsResourceHandler, get_response_headers);
        cef_bind_method(AssetsResourceHandler, read);
    }

    ~AssetsResourceHandler()
    {
        if (stream_ != nullptr)
            stream_->base.release(&stream_->base);
    }

private:
    cef_stream_reader_t *stream_;
    int64 length_;
    wstr path_;
    wstr mime_;
    bool no_cache_;

    int _open(cef_request_t* request, int* handle_request, cef_callback_t* callback)
    {
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
        path_ = config::pluginsDir().wstring().append(path_);

        // Trailing slash.
        if (path_[path_.length() - 1] == '/' || path_[path_.length() - 1] == L'\\')
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
            const char *module_code = nullptr;

            // Detect relative plugin imports by referer //plugins.
            if (request->get_resource_type(request) == RT_SCRIPT)
            {
                static const std::wregex raw_pattern{ L"\\braw\\b" };
                static const std::wregex url_pattern{ L"\\burl\\b" };

                if (std::regex_search(query_part, url_pattern))
                    module_code = SCRIPT_IMPORT_URL;
                else if (std::regex_search(query_part, raw_pattern))
                    module_code = SCRIPT_IMPORT_RAW;
                else if ((pos = path_.find_last_of(L'.')) != wstr::npos)
                {
                    auto ext = path_.substr(pos + 1);
                    if (ext == L"css")
                        module_code = SCRIPT_IMPORT_CSS;
                    else if (ext == L"json")
                        module_code = SCRIPT_IMPORT_JSON;
                    else if (ext == L"toml")
                        module_code = SCRIPT_IMPORT_TOML;
                    else if (ext == L"yml" || ext == L"yaml")
                        module_code = SCRIPT_IMPORT_YAML;
                    else if (KNOWN_ASSETS.find(ext) != KNOWN_ASSETS.end())
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

        *handle_request = true;
        callback->cont(callback);
        return true;
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
                response->set_mime_type(response, &CefStr(mime_));

            if (no_cache_ || mime_ == L"text/javascript")
                response->set_header_by_name(response, &u"Cache-Control"_s, &u"no-store"_s, 1);
            else
            {
                response->set_header_by_name(response, &u"Cache-Control"_s, &u"max-age=31536000, immutable"_s, 1);
                set_etag(response, path_);
            }

            *response_length = length_;
        }
    }

    int _read(void* data_out, int bytes_to_read, int* bytes_read, struct _cef_resource_read_callback_t* callback)
    {
        *bytes_read = 0;

        if (stream_ == nullptr)
            return false;

        *bytes_read = static_cast<int>(stream_->read(stream_, data_out, 1, bytes_to_read));
        return (*bytes_read > 0);
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