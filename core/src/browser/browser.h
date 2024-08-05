#pragma once
#include "commons.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_frame_capi.h"

namespace browser
{
    extern void *view_handle;
    void open_devtools(cef_browser_t *browser);

    void register_riotclient_domain();
    void set_riotclient_credentials(const char *port, const char *token);

    void register_plugins_domain();
}