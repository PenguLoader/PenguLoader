#include "commons.h"

#if OS_WIN

void window::get_rect(void *hwnd, int *x, int *y, int *w, int *h)
{
    RECT rect;
    GetWindowRect((HWND)hwnd, &rect);

    if (x) *x = rect.left;
    if (y) *y = rect.top;
    if (w) *w = rect.right - rect.left;
    if (h) *h = rect.bottom - rect.top;
}

float window::get_scaling(void *hwnd)
{
    const int MDT_EFFECTIVE_DPI = 0;
    static HRESULT (WINAPI* GetDpiForMonitor)(
        HMONITOR hmonitor, int dpiType, UINT *dpiX, UINT *dpiY) = nullptr;

    if (!GetDpiForMonitor) {
        HMODULE shcore = LoadLibraryA("Shcore.dll");
        FARPROC proc = GetProcAddress(shcore, "GetDpiForMonitor");
        GetDpiForMonitor = reinterpret_cast<decltype(GetDpiForMonitor)>(proc);
    }

    HMONITOR hmom = MonitorFromWindow(static_cast<HWND>(hwnd), MONITOR_DEFAULTTONEAREST);
    int dpi = 0;

    if (GetDpiForMonitor) {
        UINT hdpi_uint, vdpi_uint;
        if (GetDpiForMonitor(hmom, MDT_EFFECTIVE_DPI, &hdpi_uint, &vdpi_uint) == S_OK) {
            dpi = static_cast<int>(hdpi_uint);
        }
    }
    else {
        HDC hdc = GetDC(NULL);
        if (hdc) {
            dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(NULL, hdc);
        }
    }

    if (dpi == 0)
        dpi = 96;

    return dpi / 96.0f;
}

void window::make_foreground(void *hwnd)
{
    auto window = (HWND)hwnd;

    // Restore if minimized.
    if (IsIconic(window))
        ShowWindow(window, SW_RESTORE);
    else
        ShowWindow(window, SW_SHOWNORMAL);

    SetForegroundWindow(window);
}

#endif