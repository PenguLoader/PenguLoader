#include "commons.h"

void *utils::patternScan(const HMODULE module, const char *pattern)
{
    if (!module)
        return nullptr;

    static auto PatternToBytes = [](const char *Pattern)
    {
        vec<int> Bytes{};
        char *StartPos = const_cast<char *>(Pattern);
        char *EndPos = const_cast<char *>(Pattern) + strlen(Pattern);

        for (auto CurrentChar = StartPos; CurrentChar < EndPos; ++CurrentChar)
        {
            if (*CurrentChar == '?')
            {
                ++CurrentChar;

                if (*CurrentChar == '?')
                    ++CurrentChar;

                Bytes.push_back(-1);
            }
            else
            {
                Bytes.push_back(strtoul(CurrentChar, &CurrentChar, 16));
            }
        }

        return Bytes;
    };

    auto DosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
    auto NtHeaders = reinterpret_cast<const IMAGE_NT_HEADERS *>(reinterpret_cast<uint8_t *>(module) + DosHeader->e_lfanew);

    auto PatternBytes = PatternToBytes(pattern);
    const size_t PatternBytesSize = PatternBytes.size();
    const int *PatternBytesData = PatternBytes.data();

    const size_t ImageSize = NtHeaders->OptionalHeader.SizeOfImage;
    uint8_t *ScanBytes = reinterpret_cast<uint8_t *>(module);

    for (size_t i = 0; i < ImageSize - PatternBytesSize; ++i)
    {
        bool Found = true;

        for (size_t j = 0; j < PatternBytesSize; ++j)
        {
            if (ScanBytes[i + j] != PatternBytesData[j] && PatternBytesData[j] != -1)
            {
                Found = false;
                break;
            }
        }

        if (Found)
            return &ScanBytes[i];
    }

    return nullptr;
}

float utils::getWindowScale(void *handle)
{
#ifdef _WIN32
    const int MDT_EFFECTIVE_DPI = 0;
    static HRESULT (WINAPI* GetDpiForMonitor)(
        HMONITOR hmonitor, int dpiType, UINT *dpiX, UINT *dpiY) = nullptr;

    if (!GetDpiForMonitor) {
        HMODULE shcore = LoadLibraryA("Shcore.dll");
        FARPROC proc = GetProcAddress(shcore, "GetDpiForMonitor");
        GetDpiForMonitor = reinterpret_cast<decltype(GetDpiForMonitor)>(proc);
    }

    HWND hwnd = static_cast<HWND>(handle);
    HMONITOR hmom = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
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
#endif

    return 1.0f;
}