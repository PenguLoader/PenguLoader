#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// This is C++ version of vibe
// https://github.com/pykeio/vibe

enum ACCENT_STATE
{
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

struct ACCENT_POLICY
{
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
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

#define DWMSBT_DISABLE                  1
#define DWMSBT_MAINWINDOW               2   // Mica
#define DWMSBT_TRANSIENTWINDOW          3   // Acrylic

typedef NTSTATUS (WINAPI *RtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation);
typedef BOOL (WINAPI *SetWindowCompositionAttribute)(HWND hwnd, const WINDOWCOMPOSITIONATTRIBDATA *data);

#define GetFunction(library, fn) _GetFunction<fn>(library, #fn)
template<typename T> T _GetFunction(const char *library, const char *func)
{
    static T fn = nullptr;
    if (fn != nullptr) return fn;

    auto module = LoadLibraryA(library);
    if (module == NULL) return nullptr;

    return fn = reinterpret_cast<T>(GetProcAddress(module, func));
}

DWORD WinVer(int index)
{
    static DWORD version[3];

    if (index >= 3)
        return 0;

    if (version[0] != 0)
        return version[index];

    OSVERSIONINFOW vi{};
    GetFunction("ntdll.dll", RtlGetVersion)(&vi);

    version[0] = vi.dwMajorVersion;
    version[1] = vi.dwMinorVersion;
    version[2] = vi.dwBuildNumber;

    return version[index];
}

bool IsWin7Plus()
{
    return WinVer(0) > 6 || (WinVer(1) == 6 && WinVer(1) == 1);
}

bool IsWin10_1809()
{
    return WinVer(2) >= 17763 && WinVer(2) < 22000;
}

bool IsWin11()
{
    return WinVer(2) >= 22000;
}

bool IsWin11_22H2()
{
    return WinVer(2) >= 22621;
}

void ExtendClientArea(HWND hwnd)
{
    MARGINS margins{ -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void ResetClientArea(HWND hwnd)
{
    MARGINS margins{ 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void SetAccentPolicy(HWND hwnd, ACCENT_STATE accent_state, COLORREF option_color)
{
    auto fn_SetWindowCompositionAttribute = GetFunction("user32.dll", SetWindowCompositionAttribute);
    if (fn_SetWindowCompositionAttribute == nullptr)
        return;

    bool is_acrylic = accent_state == ACCENT_ENABLE_ACRYLICBLURBEHIND;
    if (is_acrylic && ((option_color >> 24) & 0xFF) == 0)
    {
        // acrylic doesn't like to have 0 alpha
        option_color |= 1 << 24;
    } 

    ACCENT_POLICY policy{};
    policy.AccentState = accent_state;
    policy.AccentFlags = is_acrylic ? 0 : 2;
    policy.GradientColor = option_color;
    policy.AnimationId = 0;

    WINDOWCOMPOSITIONATTRIBDATA data{};
    data.dwAttrib = 0x13,
    data.pvData = &policy;
    data.cbData = sizeof(policy);

    fn_SetWindowCompositionAttribute(hwnd, &data);
}

void ApplyAcrylic(HWND hwnd, bool unified, bool acrylic_blurbehind, COLORREF option_color)
{
    if (!unified && IsWin11_22H2())
    {
        DWORD value = DWMSBT_TRANSIENTWINDOW;
        ExtendClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
    }
    else if (IsWin7Plus())
    {
        auto accent_state = acrylic_blurbehind ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_ENABLE_BLURBEHIND;
        option_color = option_color != 0 ? option_color : RGB(40, 40, 40);
        SetAccentPolicy(hwnd, accent_state, option_color);
    }
    else
    {
        // not reachable
        //return Err(VibeError::UnsupportedPlatform("\"apply_acrylic()\" is only available on Windows 7+"));
    }
}

void ClearAcrylic(HWND hwnd, bool unified)
{
    if (!unified && IsWin11_22H2())
    {
        DWORD value = DWMSBT_DISABLE;
        ResetClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
    }
    else if (IsWin7Plus())
    {
        SetAccentPolicy(hwnd, ACCENT_DISABLED, 0);
    }
    else
    {
        // not reachable
        //return Err(VibeError::UnsupportedPlatform("\"clear_acrylic()\" is only available on Windows 7+"));
    }
}

void ForceDarkTheme(HWND hwnd)
{
    if (IsWin11())
    {
        DWORD value = 1;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    }
    else if (IsWin10_1809())
    {
        DWORD value = 1;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE - 1, &value, sizeof(value));
    }
    else
    {
        throw "Dark theme is only available on Windows 10 v1809+ or Windows 11.";
    }
}

void ForceLightTheme(HWND hwnd)
{
    if (IsWin11())
    {
        DWORD value = 0;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    }
    else if (IsWin10_1809())
    {
        DWORD value = 0;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE - 1, &value, sizeof(value));
    }
    else
    {
        throw "Light theme is only available on Windows 10 v1809+ or Windows 11.";
    }
}


void ApplyMica(HWND hwnd)
{
    if (IsWin11_22H2())
    {
        DWORD value = DWMSBT_MAINWINDOW;
        ExtendClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
    }
    else if (IsWin11())
    {
        DWORD value = DWMSBT_MAINWINDOW;
        ExtendClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &value, sizeof(value));
    }
    else
    {
        throw "Mica effect is only available on Windows 11.";
    }
}

void ClearMica(HWND hwnd)
{
    if (IsWin11_22H2())
    {
        DWORD value = DWMSBT_DISABLE;
        ResetClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
    }
    else if (IsWin11()) {
        DWORD value = 0;
        ResetClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &value, sizeof(value));
    }
    else
    {
        throw "Mica effect is only available on Windows 11.";
    }
}

enum Effects
{
    MICA,
    ACRYLIC,
    UNIFIED_ACRYLIC,
    BLURBEHIND
};

const char *ApplyEffects(HWND hwnd, Effects effect, COLORREF option_color)
{
    try
    {
        switch (effect)
        {
            case MICA:
                ApplyMica(hwnd);
                break;

            case ACRYLIC:
                ApplyAcrylic(hwnd, false, true, option_color);
                break;

            case UNIFIED_ACRYLIC:
                ApplyAcrylic(hwnd, true, true, option_color);
                break;

            case BLURBEHIND:
                ApplyAcrylic(hwnd, true, false, option_color);
                break;
        }

        return nullptr;
    }
    catch (const char *err)
    {
        return err;
    }
}

const char *ClearEffects(HWND hwnd, Effects effect)
{
    try
    {
        switch (effect)
        {
            case MICA:
                ClearMica(hwnd);
                break;

            case ACRYLIC:
            case UNIFIED_ACRYLIC:
            case BLURBEHIND:
                ClearAcrylic(hwnd, effect != ACRYLIC);
                break;
        }

        return nullptr;
    }
    catch (const char *err)
    {
        return err;
    }
}