#include "browser.h"
#include "assets_shims.h"
#include "assets_path.h"

#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_scheme_capi.h"
#include "include/capi/cef_stream_capi.h"
#include "include/capi/cef_resource_handler_capi.h"

// BROWSER PROCESS ONLY.

// Length of the leading "https://plugins" prefix in every incoming URL.
// register_plugins_domain pins both the scheme and the synthetic host, so
// every request the handler sees begins with exactly these 15 characters.
static constexpr size_t URL_PREFIX_LEN = 15;

// Custom resource handler for local assets.
class AssetsResourceHandler : public CefRefCount<cef_resource_handler_t>
{
public:
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

    int _open(cef_request_t* request, int* handle_request, cef_callback_t* callback)
    {
        size_t pos;
        bool js_mime = false;

        CefScopedStr url = request->get_url(request);

        // Defensive: a malformed URL shorter than the scheme+host prefix means
        // something violated the contract our scheme registration set up.
        // Treat as 404 rather than reading garbage past the end of the buffer.
        if (url.length < URL_PREFIX_LEN)
        {
            *handle_request = 1;
            return 1;
        }

        std::u16string fs_path, query_part;
        fs_path.assign((char16_t *)url.str + URL_PREFIX_LEN,
                       url.length - URL_PREFIX_LEN);

        // Query strings start at the FIRST `?` per RFC 3986. rfind would
        // grab the last one if a filename ever contained an escaped `?`.
        if ((pos = fs_path.find('?')) != std::u16string::npos)
        {
            query_part = fs_path.substr(pos + 1);
            fs_path = fs_path.substr(0, pos);
        }

        // Decode URI (path separators stay escaped — see assets_path.h).
        assets::decode_uri(fs_path);

        // Join with the plugins directory.
        fs_path = config::plugins_dir().u16string().append(fs_path);

        // `?dir` imports resolve on path validity, not existence — the
        // Directory handed back may legitimately point at a folder the user
        // hasn't created yet, since reveal() creates it on demand. That means
        // skipping both the leaf rewriting below (which would turn `images`
        // into `images/index.js`) and the is_file() gate further down, which
        // a directory can never satisfy.
        const bool dir_import = query_part == u"dir"
            && request->get_resource_type(request) == RT_SCRIPT;

        // Trailing slash → serve <dir>/index.js.
        if (dir_import)
        {
            // No path rewriting; fs_path is already the directory.
        }
        else if (fs_path[fs_path.length() - 1] == '/' || fs_path[fs_path.length() - 1] == '\\')
        {
            js_mime = true;
            fs_path.append(u"index.js");
        }
        else
        {
            size_t sep = fs_path.find_last_of(u"/\\");
            std::u16string leaf = fs_path.substr(sep + 1);

            // No extension on the leaf — peek .js then folder/index.js.
            if (leaf.rfind('.') == std::u16string::npos)
            {
                if ((js_mime = file::is_file(fs_path + u".js")))
                    fs_path.append(u".js");
                else if ((js_mime = file::is_dir(fs_path)))
                    fs_path.append(u"/index.js");
            }
        }

        // Path-traversal defense. After all the .js / index.js appending the
        // resolved path must still sit inside plugins_dir — `..` segments are
        // collapsed by `lexically_normal` before the prefix check. CEF/Chromium
        // usually normalizes `..` in URLs before we see them, but this is the
        // explicit guarantee.
        if (!assets::is_inside(config::plugins_dir(), path{ fs_path }))
        {
            *handle_request = 1;
            return 1;
        }

        if (dir_import)
        {
            js_mime = true;
            stream_ = cef_stream_reader_create_for_data(
                (void *)assets::SCRIPT_IMPORT_DIR, strlen(assets::SCRIPT_IMPORT_DIR));
        }
        else if (file::is_file(fs_path))
        {
            const char *module_code = nullptr;
            if (request->get_resource_type(request) == RT_SCRIPT)
            {
                if (query_part == u"url")
                    module_code = assets::SCRIPT_IMPORT_URL;
                else if (query_part == u"raw")
                    module_code = assets::SCRIPT_IMPORT_RAW;
                else if ((pos = fs_path.rfind('.')) != std::u16string::npos)
                {
                    // Lowercase the extension for matching — `.JSON` should
                    // produce the same shim as `.json`.
                    auto ext = fs_path.substr(pos + 1);
                    for (auto &ch : ext)
                        if (ch >= u'A' && ch <= u'Z') ch = ch - u'A' + u'a';

                    if (ext == u"css")
                        module_code = assets::SCRIPT_IMPORT_CSS;
                    else if (ext == u"json")
                        module_code = assets::SCRIPT_IMPORT_JSON;
                    else if (assets::KNOWN_ASSETS_SET.find(
                                 assets::fnv32_1a(ext.c_str(), ext.length()))
                             != assets::KNOWN_ASSETS_SET.end())
                        module_code = assets::SCRIPT_IMPORT_URL;
                }
            }

            if (module_code != nullptr)
            {
                js_mime = true;
                stream_ = cef_stream_reader_create_for_data(
                    (void *)module_code, strlen(module_code));
            }
            else
            {
                // Stash CefStr::wrap in a named local so the cef_string_t
                // backing the call is unambiguously alive for the full
                // duration of cef_stream_reader_create_for_file. Passing
                // `&CefStr::wrap(path)` worked in practice (CEF reads it
                // synchronously) but the lifetime was relying on
                // implementation behavior.
                auto path_str = CefStr::wrap(fs_path);
                stream_ = cef_stream_reader_create_for_file(&path_str);
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
            else if ((pos = fs_path.rfind(u'.')) != std::u16string::npos)
            {
                // Get MIME type from file extension.
                auto ext = fs_path.substr(pos + 1);
                CefScopedStr type{ cef_get_mime_type(&CefStr::wrap(ext)) };
                type.copy(mime_);
            }
        }

        // Save range header for later.
        CefScopedStr range{ request->get_header_by_name(request, &u"Range"_s) };
        if (!range.empty())
        {
            range_header_.assign(range.to_utf8());
        }

        *handle_request = 1;
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

            // application/json gets no-store too — the writable-JSON `$write`
            // path mutates files behind the cache, and a stale cached response
            // would surface old content on the next plain fetch.
            if (no_cache_ || mime_ == u"text/javascript" || mime_ == u"application/json")
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
            stream_->seek(stream_, bytes_to_skip, SEEK_CUR);
            int position = static_cast<int>(stream_->tell(stream_));

            // Report the *actual* delta — clamped at EOF if the seek didn't
            // reach the requested target. Previously we overwrote this with
            // `bytes_to_skip`, which lied to the caller in the clamped case.
            *bytes_skipped = position - oldPosition;
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
        uint32_t hash = assets::fnv32_1a(url.str, url.length);

        char etag[64];
        size_t etag_length = snprintf(etag, sizeof(etag) - 1, "\"%08x\"", hash);

        auto name = u"ETag"_s;
        CefStr value{ etag, etag_length };
        response->set_header_by_name(response, &name, &value, 1);
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
