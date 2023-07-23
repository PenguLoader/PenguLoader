#include "commons.h"

HWND RCLIENT_WINDOW = nullptr;

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
typedef LSTATUS (WINAPI *RegGetValueW_t)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);

#define GetFunction(library, fn) _GetFunction<fn>(library, #fn)
#define GetFunction2(library, fn) _GetFunction<decltype(&fn)>(library, #fn)
template<typename T> T _GetFunction(const char *library, const char *func)
{
    static T fn = nullptr;
    if (fn != nullptr) return fn;

    auto module = LoadLibraryA(library);
    if (module == NULL) return nullptr;

    return fn = reinterpret_cast<T>(GetProcAddress(module, func));
}

static HRESULT WINAPI DwmExtendFrameIntoClientArea(
    HWND hWnd,
    const MARGINS *pMarInset)
{
    return GetFunction2("dwmapi.dll", DwmExtendFrameIntoClientArea)(hWnd, pMarInset);
}

static HRESULT WINAPI DwmSetWindowAttribute(
    HWND hwnd,
    DWORD dwAttribute,
    _In_reads_bytes_(cbAttribute) LPCVOID pvAttribute,
    DWORD cbAttribute)
{
    return GetFunction2("dwmapi.dll", DwmSetWindowAttribute)(hwnd, dwAttribute, pvAttribute, cbAttribute);
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

bool IsWin10_20H1()
{
    return WinVer(2) >= 19041 && WinVer(2) < 22000;
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

bool IsWindowsLightTheme()
{
    // based on https://stackoverflow.com/questions/51334674/how-to-detect-windows-10-light-dark-mode-in-win32-application

    // The value is expected to be a REG_DWORD, which is a signed 32-bit little-endian
    auto buffer = std::vector<char>(4);
    auto cbData = static_cast<DWORD>(buffer.size() * sizeof(char));
    auto res = _GetFunction<RegGetValueW_t>("advapi32.dll", "RegGetValueW")(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD, // expected value type
        nullptr,
        buffer.data(),
        &cbData);

    if (res != ERROR_SUCCESS)
        return true;

    // convert bytes written to our buffer to an int, assuming little-endian
    auto i = int(buffer[3] << 24 |
        buffer[2] << 16 |
        buffer[1] << 8 |
        buffer[0]);

    return i == 1;
}

void ForceDarkTheme(HWND hwnd)
{
    if (IsWin11() || IsWin10_20H1())
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
        //throw "Dark theme is only available on Windows 10 v1809+ or Windows 11.";
    }
}

void ForceLightTheme(HWND hwnd)
{
    if (IsWin11() || IsWin10_20H1())
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
        //throw "Light theme is only available on Windows 10 v1809+ or Windows 11.";
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
    if (RCLIENT_WINDOW == nullptr)
        return false;

    if (name == L"mica")
        return ApplyMica(RCLIENT_WINDOW);
    else if (name == L"acrylic")
        return ApplyAcrylic(RCLIENT_WINDOW, false, true, option_color);
    else if (name == L"unified")
        return ApplyAcrylic(RCLIENT_WINDOW, true, true, option_color);
    else if (name == L"blurbehind")
        return ApplyAcrylic(RCLIENT_WINDOW, true, false, option_color);

    return false;
}

bool ClearEffect(const std::wstring &name)
{
    if (RCLIENT_WINDOW == nullptr)
        return false;

    if (name == L"mica")
    {
        ClearMica(RCLIENT_WINDOW);
        return true;
    }
    else if (name == L"acrylic")
    {
        ClearAcrylic(RCLIENT_WINDOW, false);
        return true;
    }
    else if (name == L"unified" || name == L"blurbehind")
    {
        ClearAcrylic(RCLIENT_WINDOW, true);
        return true;
    }

    return false;
}

// Low-level hex color parser.
uint32_t ParseHexColor(std::wstring value)
{
    unsigned a = 0, r = 0, g = 0, b = 0;

    if (value.empty() || (value.length() == 1 && value[0] == '#'))
        return 0;

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

    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

static wstr m_current = L"";

V8Value *native_GetWindowEffect(const vec<V8Value *> &args)
{
    if (m_current.empty())
        return V8Value::undefined();
    else
        return V8Value::string(&CefStr(m_current));
}

V8Value *native_SetWindowEffect(const vec<V8Value *> &args)
{
    bool success = false;

    if (args.size() > 0)
    {
        if (args[0]->isBool() && args[0]->asBool() == false)
        {
            success = ClearEffect(m_current);
            m_current.clear();
        }
        else if (args[0]->isString())
        {
            CefScopedStr name{ args[0]->asString() };
            uint32_t tintColor = 0;

            if (args.size() >= 2 && args[1]->isObject())
            {
                auto options = args[1]->asObject();
                if (options->has(&L"color"_s))
                {
                    auto color = options->get(&L"color"_s);
                    if (color->isString())
                    {
                        CefScopedStr value{ color->asString() };
                        tintColor = ParseHexColor(value.str);
                    }
                }
            }

            //if (ClearEffect(m_current))
            //    m_current = L"";

            if (success = ApplyEffect(name.str, tintColor))
                m_current.assign(name.str, name.length);
        }
    }

    return V8Value::boolean(success);
}