#include "internal.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

using namespace league_loader;

extern HWND RCLIENT_WINDOW;
extern HANDLE BROWSER_PROCESS;

static HWND GetMainWindow()
{
    if (RCLIENT_WINDOW == nullptr)
        IPC_READ(BROWSER_PROCESS, &RCLIENT_WINDOW, sizeof(HWND));
    return RCLIENT_WINDOW;
}

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

bool ApplyAcrylic(HWND hwnd, bool unified, bool acrylic_blurbehind, COLORREF option_color)
{
    if (!unified && IsWin11_22H2())
    {
        DWORD value = DWMSBT_TRANSIENTWINDOW;
        ExtendClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
        return true;
    }
    else if (IsWin7Plus())
    {
        auto accent_state = acrylic_blurbehind ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_ENABLE_BLURBEHIND;
        option_color = option_color != 0 ? option_color : RGB(40, 40, 40);
        SetAccentPolicy(hwnd, accent_state, option_color);
        return true;
    }

    //return Err(VibeError::UnsupportedPlatform("\"apply_acrylic()\" is only available on Windows 7+"));
    return false;
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

    //return Err(VibeError::UnsupportedPlatform("\"clear_acrylic()\" is only available on Windows 7+"));
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

bool ApplyMica(HWND hwnd)
{
    if (IsWin11_22H2())
    {
        DWORD value = DWMSBT_MAINWINDOW;
        ExtendClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
        return true;
    }
    else if (IsWin11())
    {
        DWORD value = DWMSBT_MAINWINDOW;
        ExtendClientArea(hwnd);
        DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &value, sizeof(value));
        return true;
    }

    //throw "Mica effect is only available on Windows 11.";
    return false;
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

    //throw "Mica effect is only available on Windows 11.";
}


bool ApplyEffect(std::wstring name, uint32_t option_color)
{
    if (name == L"mica")
        return ApplyMica(GetMainWindow());
    else if (name == L"acrylic")
        return ApplyAcrylic(GetMainWindow(), false, true, option_color);
    else if (name == L"unified")
        return ApplyAcrylic(GetMainWindow(), true, true, option_color);
    else if (name == L"blurbehind")
        return ApplyAcrylic(GetMainWindow(), true, false, option_color);

    return false;
}

bool ClearEffect(const std::wstring &name)
{
    if (name == L"mica")
    {
        ClearMica(GetMainWindow());
        return true;
    }
    else if (name == L"acrylic")
    {
        ClearAcrylic(GetMainWindow(), false);
        return true;
    }
    else if (name == L"unified" || name == L"blurbehind")
    {
        ClearAcrylic(GetMainWindow(), true);
        return true;
    }

    return false;
}

// Low-level hex color parser.
uint32_t ParseHexColor(std::wstring value)
{
    unsigned a = 0, r = 0, g = 0, b = 0;

    if (value.empty())
        goto _done;
    if (value.length() == 1 && value[0] == '#')
        goto _done;
    if (value.length() > 1 && value[0] == '#')
        value.erase(0, 1);

    if (value.length() == 6 || value.length() == 8)
    {
        wchar_t tmp[3]{ 0 };

        wcsncpy(tmp, value.c_str(), 2);
        r = wcstol(tmp, nullptr, 16);

        wcsncpy(tmp, value.c_str() + 2, 2);
        g = wcstol(tmp, nullptr, 16);

        wcsncpy(tmp, value.c_str() + 4, 2);
        b = wcstol(tmp, nullptr, 16);

        if (value.length() == 8)
        {
            wcsncpy(tmp, value.c_str() + 6, 2);
            a = wcstol(tmp, nullptr, 16);
        }
        else
            a = 0xFF;
    }
    else if (value.length() == 3 || value.length() == 4)
    {
        static const auto ParseHexChar = [](int chr)
            { return (chr >= 'A') ? (chr - 'A' + 10) : (chr - '0'); };

        unsigned n;
        r = ((n = ParseHexChar(toupper(value[0]))) << 4) | n;
        g = ((n = ParseHexChar(toupper(value[1]))) << 4) | n;
        b = ((n = ParseHexChar(toupper(value[2]))) << 4) | n;

        if (value.length() == 4)
            a = ParseHexChar(toupper(value[3])) << 4;
        else
            a = 0xFF;
    }

_done:
    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

bool HandleWindowEffect(const CefStr &fn, int argc, cef_v8value_t *const *args, cef_v8value_t **retval)
{
    static std::wstring current = L"";

    if (fn == L"GetEffect")
    {
        *retval = CefV8Value_CreateString(&CefStr(current));
        return true;
    }
    else if (fn == L"ApplyEffect")
    {
        bool success = false;

        if (argc >= 1 && args[0]->is_string(args[0]))
        {
            CefStr name = args[0]->get_string_value(args[0]);
            uint32_t tintColor = 0;

            if (argc >= 2 && args[1]->is_object(args[1]))
            {
                if (args[1]->has_value_bykey(args[1], &"color"_s))
                {
                    auto color = args[1]->get_value_bykey(args[1], &"color"_s);
                    if (color->is_string(color))
                    {
                        CefStr value = color->get_string_value(color);
                        tintColor = ParseHexColor(value.str);
                    }
                }
            }

            if (ClearEffect(current))
                current = L"";

            if (success = ApplyEffect(name.str, tintColor))
                current.assign(name.str, name.length);
        }
        
        *retval = CefV8Value_CreateBool(success);
        return true;
    }
    else if (fn == L"ClearEffect")
    {
        if (!current.empty())
            ClearEffect(current);
        current.clear();
        return true;
    }

    return false;
}