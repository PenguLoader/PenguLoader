#include "internal.h"
#include "include/cef_version.h"

using namespace league_loader;

decltype(&cef_get_mime_type) league_loader::CefGetMimeType;
decltype(&cef_register_extension) league_loader::CefRegisterExtension;
decltype(&cef_dictionary_value_create) league_loader::CefDictionaryValue_Create;
decltype(&cef_stream_reader_create_for_file) league_loader::CefStreamReader_CreateForFile;

decltype(&cef_string_set) league_loader::CefString_Set;
decltype(&cef_string_clear) league_loader::CefString_Clear;
decltype(&cef_string_from_utf8) league_loader::CefString_FromUtf8;
decltype(&cef_string_from_wide) league_loader::CefString_FromWide;
decltype(&cef_string_userfree_free) league_loader::CefString_UserFree_Free;

decltype(&cef_v8value_create_null) league_loader::CefV8Value_CreateNull;
decltype(&cef_v8value_create_int) league_loader::CefV8Value_CreateInt;
decltype(&cef_v8value_create_string) league_loader::CefV8Value_CreateString;
decltype(&cef_v8value_create_function) league_loader::CefV8Value_CreateFunction;
decltype(&cef_v8value_create_array) league_loader::CefV8Value_CreateArray;

decltype(&cef_initialize) league_loader::CefInitialize;
decltype(&cef_execute_process) league_loader::CefExecuteProcess;
decltype(&cef_browser_host_create_browser) league_loader::CefBrowserHost_CreateBrowser;

CefStr::CefStr(const std::string &s) : cef_string_t{}
{
    CefString_FromUtf8(s.c_str(), s.length(), this);
}

CefStr::CefStr(const std::wstring &s) : cef_string_t{}
{
    CefString_FromWide(s.c_str(), s.length(), this);
}

CefStr::CefStr(const cef_string_t *s) : cef_string_t{}
{
    if (s) CefString_Set(s->str, s->length, this, true);
    else CefStr("");
}

CefStr::CefStr(cef_string_userfree_t uf) : cef_string_t{}
{
    if (uf)
    {
        CefString_Set(uf->str, uf->length, this, true);
        CefString_UserFree_Free(uf);
    }
    else CefStr("");
}

CefStr::CefStr(int32_t i) : CefStr(std::to_string(i))
{
}

CefStr::CefStr(uint32_t u) : CefStr(std::to_string(u))
{
}

bool CefStr::equal(const wchar_t *s) const
{
    return wcscmp(str, s) == 0;
}

bool CefStr::equal(const std::wstring &s) const
{
    return wcsncmp(str, s.c_str(), length) == 0;
}

bool CefStr::equali(const wchar_t *s) const
{
    return _wcsicmp(str, s) == 0;
}

bool CefStr::equali(const std::wstring &s) const
{
    return _wcsnicmp(str, s.c_str(), length) == 0;
}

bool CefStr::contain(const wchar_t *s) const
{
    return str_contain(str, s);
}

bool CefStr::contain(const std::wstring &s) const
{
    return str_contain(str, s);
}

static int GetFileMajorVersion(LPCWSTR file)
{
    int version = 0;

    DWORD  verHandle = 0;
    UINT   size = 0;
    LPBYTE lpBuffer = NULL;

    if (DWORD verSize = GetFileVersionInfoSize(file, &verHandle))
    {
        LPSTR verData = new char[verSize];

        if (GetFileVersionInfo(file, verHandle, verSize, verData)
            && VerQueryValue(verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size)
            && size > 0)
        {
            VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
            if (verInfo->dwSignature == 0xfeef04bd)
                version = ((verInfo->dwFileVersionMS >> 16) & 0xffff);
        }

        delete[] verData;
    }

    return version;
}

static void WarnInvalidVersion()
{
    MessageBox(NULL,
        L"This League of Legends Client version is not supported.\n"
        L"Please check existing issues or open new issue about that, and wait for the new update.",
        L"League Loader", MB_TOPMOST | MB_OK | MB_ICONWARNING);
    ShellExecute(NULL, L"open", L"https://git.leagueloader.app", NULL, NULL, NULL);
}

bool LoadLibcefDll()
{
    LPCWSTR filename = L"libcef.dll";

    if (GetFileMajorVersion(filename) != CEF_VERSION_MAJOR)
    {
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WarnInvalidVersion, NULL, 0, NULL);
        return false;
    }

    // libcef.dll is already loaded (our module is its dependency).
    if (HMODULE libcef = GetModuleHandle(filename))
    {
        // Get CEF functions.
        (LPVOID &)CefGetMimeType = GetProcAddress(libcef, "cef_get_mime_type");
        (LPVOID &)CefRegisterExtension = GetProcAddress(libcef, "cef_register_extension");
        (LPVOID &)CefDictionaryValue_Create = GetProcAddress(libcef, "cef_dictionary_value_create");
        (LPVOID &)CefStreamReader_CreateForFile = GetProcAddress(libcef, "cef_stream_reader_create_for_file");

        (LPVOID &)CefString_Set = GetProcAddress(libcef, "cef_string_utf16_set");
        (LPVOID &)CefString_Clear = GetProcAddress(libcef, "cef_string_utf16_clear");
        (LPVOID &)CefString_FromUtf8 = GetProcAddress(libcef, "cef_string_utf8_to_utf16");
        (LPVOID &)CefString_FromWide = GetProcAddress(libcef, "cef_string_wide_to_utf16");
        (LPVOID &)CefString_UserFree_Free = GetProcAddress(libcef, "cef_string_userfree_utf16_free");

        (LPVOID &)CefV8Value_CreateNull = GetProcAddress(libcef, "cef_v8value_create_null");
        (LPVOID &)CefV8Value_CreateInt = GetProcAddress(libcef, "cef_v8value_create_int");
        (LPVOID &)CefV8Value_CreateString = GetProcAddress(libcef, "cef_v8value_create_string");
        (LPVOID &)CefV8Value_CreateFunction = GetProcAddress(libcef, "cef_v8value_create_function");
        (LPVOID &)CefV8Value_CreateArray = GetProcAddress(libcef, "cef_v8value_create_array");

        (LPVOID &)CefInitialize = GetProcAddress(libcef, "cef_initialize");
        (LPVOID &)CefExecuteProcess = GetProcAddress(libcef, "cef_execute_process");
        (LPVOID &)CefBrowserHost_CreateBrowser = GetProcAddress(libcef, "cef_browser_host_create_browser");

        return true;
    }

    return false;
}