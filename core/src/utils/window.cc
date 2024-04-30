#include "commons.h"

#if OS_WIN

struct MARGINS
{
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
};

enum ACCENT_STATE
{
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

struct ACCENT_POLICY
{
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    COLORREF GradientColor;
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    DWORD dwAttrib;
    LPVOID pvData;
    DWORD cbData;
};

#define DWMWA_USE_IMMERSIVE_DARK_MODE   20
#define DWMWA_MICA_EFFECT               1029
#define DWMWA_SYSTEMBACKDROP_TYPE       38

enum DWM_SYSTEMBACKDROP_TYPE
{
    DWMSBT_AUTO = 0,            // Auto
    DWMSBT_DISABLE = 1,         // None
    DWMSBT_MAINWINDOW = 2,      // Mica
    DWMSBT_TRANSIENTWINDOW = 3, // Acrylic
    DWMSBT_TABBEDWINDOW = 4,    // Tabbed
};

typedef LONG (WINAPI *RtlGetVersion)(PRTL_OSVERSIONINFOW);
typedef HRESULT (WINAPI *GetDpiForMonitor)(HMONITOR hmonitor, int dpiType, UINT* dpiX, UINT* dpiY);
typedef BOOL (WINAPI *SetWindowCompositionAttribute)(HWND, const WINDOWCOMPOSITIONATTRIBDATA *);
typedef HRESULT (WINAPI *DwmExtendFrameIntoClientArea)(HWND hWnd, const MARGINS *pMarInset);
typedef HRESULT (WINAPI *DwmSetWindowAttribute)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

template <typename T> 
static T _getfunc(const char *lib, const char *fn)
{
    static T proc = nullptr;
    if (!proc)
    {
        auto mod = LoadLibraryA(lib);
        if (mod) proc = reinterpret_cast<T>(GetProcAddress(mod, fn));
    }
    return proc;
}

#define getfunc(lib, fn) _getfunc<fn>(lib, #fn)

static auto winver()
{
    static struct { int major, minor, build; } version = { 0 };
    if (version.major == 0)
    {
        if (auto func = getfunc("ntdll.dll", RtlGetVersion))
        {
            OSVERSIONINFOW vi{};
            func(&vi);
            version.major = vi.dwMajorVersion;
            version.minor = vi.dwMinorVersion;
            version.build = vi.dwBuildNumber;
        }
    }
    return version;
}

#define IsWin7Plus()    (winver().major > 6 || (winver().minor == 6 && winver().minor == 1))
#define IsWin10_20H1()  (winver().build >= 19041 && winver().build < 22000)
#define IsWin10_1809()  (winver().build >= 17763 && winver().build < 22000)
#define IsWin11()       (winver().build >= 22000)
#define IsWin11_22H2()  (winver().build >= 22523/*22621*/)

static void extend_client_area(HWND hwnd, int inset)
{
    if (auto func = getfunc("dwmapi.dll", DwmExtendFrameIntoClientArea))
    {
        MARGINS margins{ inset, inset, inset, inset };
        if (inset > 0) margins = { 0, 0, inset, 0 };
        func(hwnd, &margins);
    }
}

static void set_window_attribute(HWND hwnd, DWORD attr, DWORD value)
{
    if (auto func = getfunc("dwmapi.dll", DwmSetWindowAttribute))
        func(hwnd, attr, &value, sizeof(DWORD));
}

static void set_accent_policy(HWND hwnd, ACCENT_STATE state, COLORREF color)
{
    if (auto func = getfunc("user32.dll", SetWindowCompositionAttribute))
    {
        bool acrylic = state == ACCENT_ENABLE_ACRYLICBLURBEHIND;
        if (acrylic && ((color >> 24) & 0xFF) == 0)
        {
            // acrylic doesn't like to have 0 alpha
            color |= 1 << 24;
        }

        ACCENT_POLICY policy{};
        policy.AccentState = state;
        policy.AccentFlags = acrylic ? 0 : 2;
        policy.GradientColor = color;
        policy.AnimationId = 0;

        WINDOWCOMPOSITIONATTRIBDATA data{};
        data.dwAttrib = 0x13;
        data.pvData = &policy;
        data.cbData = sizeof(policy);

        func(hwnd, &data);
    }
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
    auto getDpiForMonitor = getfunc("shcore.dll", GetDpiForMonitor);

    int dpi = 0;
    if (getDpiForMonitor) {
        UINT hdpi_uint, vdpi_uint;
        HMONITOR hmom = MonitorFromWindow(static_cast<HWND>(hwnd), MONITOR_DEFAULTTONEAREST);
        if (getDpiForMonitor(hmom, MDT_EFFECTIVE_DPI, &hdpi_uint, &vdpi_uint) == S_OK) {
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

 	set_window_attribute(window, 2, 2);
 	extend_client_area(window, 1);

 	SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED
        | SWP_NOMOVE | SWP_NOSIZE | SWP_DRAWFRAME | SWP_NOACTIVATE);
}

bool window::is_dark_theme()
{
    char buffer[4]{};
    DWORD cbData = sizeof(buffer);
    auto res = _getfunc<decltype(&RegGetValueW)>("advapi32.dll", "RegGetValueW")(HKEY_CURRENT_USER,
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

void window::set_theme(void *hwnd, bool dark)
{
    HWND window = static_cast<HWND>(hwnd);

    if (IsWin11() || IsWin10_20H1() || IsWin10_1809())
    {
        DWORD attr = DWMWA_USE_IMMERSIVE_DARK_MODE;
        if (IsWin10_1809()) attr -= 1;
        set_window_attribute(window, attr, dark ? 1 : 0);
    }
}

enum class BackdropType
{
    Transparent,
    BlurBehind,
    Acrylic,
    Unified,
    Mica,
};

void window::apply_vibrancy(void* handle, uint32_t _type, uint32_t param)
{
    clear_vibrancy(handle);
    HWND window = static_cast<HWND>(handle);
    auto type = (BackdropType)_type;
    bool success = false;

    switch (type)
    {
        case BackdropType::Transparent:
            if (IsWin7Plus())
            {
                set_accent_policy(window, ACCENT_ENABLE_TRANSPARENTGRADIENT, (COLORREF)param);
                success = true;
            }
            break;

        case BackdropType::BlurBehind:
            if (IsWin7Plus())
            {
                set_accent_policy(window, ACCENT_ENABLE_BLURBEHIND, (COLORREF)param);
                success = true;
            }
            break;

        case BackdropType::Acrylic:
        case BackdropType::Unified:
            if (IsWin7Plus())
            {
                set_accent_policy(window, ACCENT_ENABLE_ACRYLICBLURBEHIND, (COLORREF)param);
                success = true;
            }
            break;

        case BackdropType::Mica:
            if (IsWin11())
            {
                DWORD attr = DWMWA_MICA_EFFECT;
                DWORD value = DWMSBT_MAINWINDOW;
                if (IsWin11_22H2())
                {
                    attr = DWMWA_SYSTEMBACKDROP_TYPE;
                    value = (DWM_SYSTEMBACKDROP_TYPE)param;
                }

                extend_client_area(window, -1);
                set_window_attribute(window, attr, value);
                success = true;
            }
            break;
    }

    if (success)
        SetPropA(window, "BackdropType", (HANDLE)(intptr_t)(_type + 1));
}

void window::clear_vibrancy(void *handle)
{
    HWND window = static_cast<HWND>(handle);
    HANDLE value = RemovePropA(window, "BackdropType");
    if (value == NULL)
        return;

    auto type = (BackdropType)((intptr_t)(value) - 1);
    switch (type)
    {
        case BackdropType::Transparent:
        case BackdropType::BlurBehind:
        case BackdropType::Acrylic:
        case BackdropType::Unified:
            if (IsWin7Plus())
            {
                set_accent_policy(window, ACCENT_DISABLED, 0);
            }
            break;

        case BackdropType::Mica:
            if (IsWin11())
            {
                extend_client_area(window, 1);
                set_window_attribute(window,
                    IsWin11_22H2() ? DWMWA_SYSTEMBACKDROP_TYPE : DWMWA_MICA_EFFECT,
                    IsWin11_22H2() ? DWMSBT_DISABLE : 0);
            }
            break;
    }
}

namespace platform
{
    const char *get_os_version()
    {
        static char output[24];
        snprintf(output, sizeof(output), "%d.%d.%d", winver().major, winver().minor, winver().build);
        return output;
    }

    const char *get_os_build()
    {
        static char output[12];
        snprintf(output, sizeof(output), "%d", winver().build);
        return output;
    }
}

#endif