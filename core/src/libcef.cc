#include "pengu.h"
#include "hook.h"
#include "include/cef_version.h"

// CefContext::GetBackgroundColor()
static cef_color_t get_background_color(void *rcx, cef_browser_settings_t *, cef_state_t)
{
    return 0; // SK_ColorTRANSPARENT
}

static void fix_browser_background(const void *rladdr)
{
#if OS_WIN
    const char *pattern = "41 83 F8 01 74 0B 41 83 F8 02 75 0A 45 31 C0";
#elif OS_MAC
    const char *pattern = "55 48 89 E5 83 FA 01 74 ?? 83 FA 02 75 ??";
#endif
    using Fn = decltype(&get_background_color);
    static hook::Hook<Fn> GetBackgroundColor;
    // Find CefContext::GetBackgroundColor()
    auto func = reinterpret_cast<Fn>(dylib::find_memory(rladdr, pattern));

    if (func != nullptr)
        GetBackgroundColor.hook(func, get_background_color);
}

bool check_libcef_version(bool is_browser)
{
    void *libcef = dylib::find_lib(LIBCEF_MODULE_NAME);

    if (libcef != nullptr)
    {
        auto get_version = reinterpret_cast<decltype(&cef_version_info)>(dylib::find_proc(libcef, "cef_version_info"));

        // Check CEF version
        if (get_version == nullptr || get_version(0) != CEF_VERSION_MAJOR)
        {
            if (is_browser)
                dialog::alert("Pengu does not support your Client version.", "Pengu Loader");
            return false;
        }

        if (is_browser)
            fix_browser_background((const void *)get_version);

        return true;
    }
    else
    {
        if (is_browser)
            dialog::alert("Failed to load Chromium Embedded Framework.", "Pengu Loader");
        return false;
    }
}