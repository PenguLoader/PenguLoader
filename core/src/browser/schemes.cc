#include "../internal.h"

static constexpr auto scheme_name_ = "https";

struct Resource : CefRefCount<cef_resource_handler_t>
{
    Resource() : CefRefCount(this)
    {
        cef_resource_handler_t::open = open;
        cef_resource_handler_t::get_response_headers = get_response_headers;
    }

    static int CEF_CALLBACK open(struct _cef_resource_handler_t* _,
        struct _cef_request_t* request,
        int* handle_request,
        struct _cef_callback_t* callback)
    {
        *handle_request = true;
        callback->cont(callback);
        return true;
    }

    static void CEF_CALLBACK get_response_headers(struct _cef_resource_handler_t* _,
        struct _cef_response_t* response,
        int64* response_length,
        cef_string_t* redirectUrl)
    {
        *redirectUrl = CefStr("https://esm.run/@wuuyi/test").forawrd();
    }
};

struct NpmSchemeHandlerFactory : CefRefCount<cef_scheme_handler_factory_t>
{
    NpmSchemeHandlerFactory() : CefRefCount(this)
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
        wprintf(L"scheme: %s | req: %s\n", scheme_name->str, url.str);

        return new Resource();
    }
};

void RegisterNpmSchemeFactoryHandler()
{
    auto factory = new NpmSchemeHandlerFactory();
    CefStr scheme{ scheme_name_ };

    int err = CefRegisterSchemeHandlerFactory(&scheme, &CefStr("okela"), factory);
}

void RegisterNpmScheme(cef_scheme_registrar_t *registrar)
{
    CefStr scheme{ scheme_name_ };
    int options = CEF_SCHEME_OPTION_STANDARD
        | CEF_SCHEME_OPTION_SECURE
        | CEF_SCHEME_OPTION_CORS_ENABLED
        | CEF_SCHEME_OPTION_FETCH_ENABLED;

    //registrar->add_custom_scheme(registrar, &scheme, options);
}