#include "filter.h"

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_response_filter_capi.h"

#include "filters/indexPageFilter.h"

#include <map>

struct __RequestHandlersItem {
    struct _cef_request_handler_t* (CEF_CALLBACK* getHandler)(
        struct _cef_client_t* self);

    int(CEF_CALLBACK* release)(struct _cef_base_ref_counted_t* self);
};

struct __ResourceRequestHandlersItem {
    struct _cef_resource_request_handler_t* (CEF_CALLBACK* getHandler)(
        struct _cef_request_handler_t* self,
        struct _cef_browser_t* browser,
        struct _cef_frame_t* frame,
        struct _cef_request_t* request,
        int is_navigation,
        int is_download,
        const cef_string_t* request_initiator,
        int* disable_default_handling);

    int(CEF_CALLBACK* release)(struct _cef_base_ref_counted_t* self);
};

struct __ResourceResponseFiltersItem {
    struct _cef_response_filter_t* (CEF_CALLBACK* getHandler)(
        struct _cef_resource_request_handler_t* self,
        struct _cef_browser_t* browser,
        struct _cef_frame_t* frame,
        struct _cef_request_t* request,
        struct _cef_response_t* response);

    int(CEF_CALLBACK* release)(struct _cef_base_ref_counted_t* self);
};

struct __FiltersItem {
    cef_response_filter_status_t(CEF_CALLBACK* filter)(
        struct _cef_response_filter_t* self,
        void* data_in,
        size_t data_in_size,
        size_t* data_in_read,
        void* data_out,
        size_t data_out_size,
        size_t* data_out_written);

    int(CEF_CALLBACK* release)(struct _cef_base_ref_counted_t* self);
};

static std::map<void*, __FiltersItem> __Filters;

static std::map<void*, __RequestHandlersItem> __RequestHandlers;
static std::map<void*, __ResourceRequestHandlersItem> __ResourceRequestHandlers;
static std::map<void*, __ResourceResponseFiltersItem> __ResourceResponseFilters;

int(CEF_CALLBACK releaseRequestHandler)(struct _cef_base_ref_counted_t* self) {
    auto [_, release] = __RequestHandlers[self];
    auto const result = release(self);
    if (result) __RequestHandlers.erase(self);
    return result;
};

int(CEF_CALLBACK releaseResourceRequestHandler)(struct _cef_base_ref_counted_t* self) {
    auto [_, release] = __ResourceRequestHandlers[self];
    auto const result = release(self);
    if (result) __ResourceRequestHandlers.erase(self);
    return result;
};

int(CEF_CALLBACK releaseResourceResponseFilter)(struct _cef_base_ref_counted_t* self) {
    auto [_, release] = __ResourceResponseFilters[self];
    auto const result = release(self);
    if (result) __ResourceResponseFilters.erase(self);
    return result;
};

struct _cef_response_filter_t* (CEF_CALLBACK get_resource_response_filter)(
    struct _cef_resource_request_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    struct _cef_request_t* request,
    struct _cef_response_t* response) {


    auto _url = request->get_url(request);
    cef_urlparts_t parts{};
    cef_parse_url(_url, &parts);
    cef_string_userfree_free(_url);

    std::wstring host(parts.host.str, parts.host.length);
    std::wstring path(parts.path.str, parts.path.length);

    auto [getHandler, _] = __ResourceResponseFilters[self];
    auto handler = getHandler(self, browser, frame, request, response);

    if (!host.compare(L"127.0.0.1") && !path.compare(L"/index.html")) {
        //index page
        if (handler == nullptr) {
            auto filter = new indexPageFilter();
            filter->base.base.add_ref((cef_base_ref_counted_t*)(filter));

            return (cef_response_filter_t*)filter;
        } else {
            //TODO: add proxy filter
        }
    }

    return handler;
};

struct _cef_resource_request_handler_t* (CEF_CALLBACK get_resource_request_handler)(
    struct _cef_request_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    struct _cef_request_t* request,
    int is_navigation,
    int is_download,
    const cef_string_t* request_initiator,
    int* disable_default_handling) {
    auto [getHandler, _] = __ResourceRequestHandlers[self];
    auto handler = getHandler(self, browser, frame, request, is_download, is_download, request_initiator, disable_default_handling);

    __ResourceResponseFilters[handler] = { handler->get_resource_response_filter, handler->base.release };
    handler->base.release = releaseResourceResponseFilter;
    handler->get_resource_response_filter = get_resource_response_filter;

    return handler;
};

struct _cef_request_handler_t* (CEF_CALLBACK get_request_handler)(struct _cef_client_t* self) {
    auto [getHandler, _] = __RequestHandlers[self];
    auto handler = getHandler(self);

    __ResourceRequestHandlers[handler] = { handler->get_resource_request_handler, handler->base.release };
    handler->base.release = releaseResourceRequestHandler;
    handler->get_resource_request_handler = get_resource_request_handler;

    return handler;
};

void HookRequestHandler(cef_client_t* client) {
    __RequestHandlers[client] = { client->get_request_handler, client->base.release };
    client->base.release = releaseRequestHandler;
    client->get_request_handler = get_request_handler;
}