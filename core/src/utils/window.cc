#include "commons.h"

#if OS_WIN

#include <dwmapi.h>

typedef NTSTATUS (WINAPI *_RtlGetVersion)(PRTL_OSVERSIONINFOW);
typedef LSTATUS (WINAPI *_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
typedef HRESULT (WINAPI* _GetDpiForMonitor)(HMONITOR hmonitor, int dpiType, UINT* dpiX, UINT* dpiY);

template <typename T> 
static T getfunc(const char *lib, const char *fn)
{
    static T proc = nullptr;
    if (!proc)
    {
        auto mod = LoadLibraryA(lib);
        if (mod) proc = reinterpret_cast<T>(GetProcAddress(mod, fn));
    }
    return proc;
}

static DWORD winver(int n)
{
    static DWORD version[3]{ 0 };
    if (version[0] == 0)
    {
        if (auto rtlGetVersion = getfunc<_RtlGetVersion>("ntdll.dll", "RtlGetVersion"))
        {
            OSVERSIONINFOW vi{};
            rtlGetVersion(&vi);
            version[0] = vi.dwMajorVersion;
            version[1] = vi.dwMinorVersion;
            version[2] = vi.dwBuildNumber;
        }
    }
    return version[n];
}

#define IsWin7Plus()    (winver(0) > 6 || (winver(1) == 6 && winver(1) == 1))
#define IsWin10_20H1()  (winver(2) >= 19041 && winver(2) < 22000)
#define IsWin10_1809()  (winver(2) >= 17763 && winver(2) < 22000)
#define IsWin11()       (winver(2) >= 22000)
#define IsWin11_22H2()  (winver(2) >= 22621)

 static HRESULT WINAPI _DwmExtendFrameIntoClientArea(HWND hWnd, const MARGINS *pMarInset)
 {
     return getfunc<decltype(&DwmExtendFrameIntoClientArea)>
         ("dwmapi.dll", "DwmExtendFrameIntoClientArea")(hWnd, pMarInset);
 }

 static HRESULT WINAPI _DwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)
 {
     return getfunc<decltype(&DwmSetWindowAttribute)>
         ("dwmapi.dll", "DwmSetWindowAttribute")(hwnd, dwAttribute, pvAttribute, cbAttribute);
 }

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
    auto GetDpiForMonitor = getfunc<_GetDpiForMonitor>("shcore.dll", "GetDpiForMonitor");

    int dpi = 0;
    if (GetDpiForMonitor) {
        UINT hdpi_uint, vdpi_uint;
        HMONITOR hmom = MonitorFromWindow(static_cast<HWND>(hwnd), MONITOR_DEFAULTTONEAREST);
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

void window::enable_shadow(void *hwnd)
{
    HWND window = static_cast<HWND>(hwnd);

    DWORD policy = 2;
 	_DwmSetWindowAttribute(window, 2, &policy, sizeof(DWORD));

 	MARGINS margins = { 0, 0, 1, 0 };
 	_DwmExtendFrameIntoClientArea(window, &margins);

 	SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED
        | SWP_NOMOVE | SWP_NOSIZE | SWP_DRAWFRAME | SWP_NOACTIVATE);
}

bool window::is_dark_theme()
{
    if (auto regGetValueW = getfunc<_RegGetValueW>("advapi32.dll", "RegGetValueW"))
    {
        char buffer[4];
        DWORD cbData = sizeof(buffer);
        auto res = regGetValueW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD,
            nullptr, buffer, &cbData);

        if (res != ERROR_SUCCESS)
            return false;

        auto i = int(buffer[3] << 24 |
            buffer[2] << 16 |
            buffer[1] << 8 |
            buffer[0]);

        return i != 1;
    }
    return false;
}

bool window::set_theme(void *hwnd, bool dark)
{
    HWND window = static_cast<HWND>(hwnd);

    if (IsWin11() || IsWin10_20H1() || IsWin10_1809())
    {
        DWORD value = dark ? 1 : 0;
        DWORD attr = DWMWA_USE_IMMERSIVE_DARK_MODE;
        if (IsWin10_1809()) attr -= 1;
        _DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    }

    return false;
}

#endif