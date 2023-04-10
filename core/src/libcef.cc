#pragma once

#include "internal.h"
#include "hook.h"
#include "include/cef_version.h"
#include <Psapi.h>

decltype(&cef_get_mime_type) CefGetMimeType;
decltype(&cef_request_create) CefRequest_Create;
decltype(&cef_urlrequest_create) CefURLRequest_create;
decltype(&cef_string_multimap_alloc) CefStringMultimap_Alloc;
decltype(&cef_string_multimap_free) CefStringMultimap_Free;
decltype(&cef_register_extension) CefRegisterExtension;
decltype(&cef_dictionary_value_create) CefDictionaryValue_Create;
decltype(&cef_stream_reader_create_for_file) CefStreamReader_CreateForFile;
decltype(&cef_stream_reader_create_for_data) CefStreamReader_CreateForData;
decltype(&cef_process_message_create) CefProcessMessage_Create;
decltype(&cef_v8context_get_current_context) CefV8Context_GetCurrentContext;
decltype(&cef_server_create) CefServer_Create;
decltype(&cef_uridecode) CefURIDecode;

decltype(&cef_string_set) CefString_Set;
decltype(&cef_string_clear) CefString_Clear;
decltype(&cef_string_from_utf8) CefString_FromUtf8;
decltype(&cef_string_from_wide) CefString_FromWide;
decltype(&cef_string_userfree_free) CefString_UserFree_Free;
decltype(&cef_string_to_utf8) CefString_ToUtf8;
decltype(&cef_string_utf8_clear) CefString_ClearUtf8;

decltype(&cef_v8value_create_null) CefV8Value_CreateNull;
decltype(&cef_v8value_create_int) CefV8Value_CreateInt;
decltype(&cef_v8value_create_string) CefV8Value_CreateString;
decltype(&cef_v8value_create_function) CefV8Value_CreateFunction;
decltype(&cef_v8value_create_array) CefV8Value_CreateArray;
decltype(&cef_v8value_create_bool) CefV8Value_CreateBool;

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
        L"The version of your League of Legends Client is not supported.\n"
        L"Please check existing issues or open new issue about that, and wait for the new update.",
        L"Pengu Loader", MB_TOPMOST | MB_OK | MB_ICONWARNING);
    ShellExecute(NULL, L"open", L"https://git.pengu.lol", NULL, NULL, NULL);
}

#ifdef _WIN64
#define THISCALL_PARAMS void *_rcx
#else
#define THISCALL_PARAMS void *_ecx, void *_edx
#endif

typedef cef_color_t(__fastcall * GetBackgroundColor_t)(THISCALL_PARAMS, cef_browser_settings_t *, cef_state_t);

static Hook<GetBackgroundColor_t> Old_GetBackgroundColor;
static cef_color_t __fastcall Hooked_GetBackgroundColor(THISCALL_PARAMS, cef_browser_settings_t *settings, cef_state_t state)
{
    return 0; // fully transparent :)
}

bool LoadLibcefDll(bool is_browser)
{
    LPCWSTR filename = L"libcef.dll";

    if (GetFileMajorVersion(filename) != CEF_VERSION_MAJOR)
    {
        if (is_browser)
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WarnInvalidVersion, NULL, 0, NULL);
        return false;
    }

    // libcef.dll is already loaded (our module is its dependency).
    if (HMODULE libcef = GetModuleHandle(filename))
    {
        // Get CEF functions.
        (LPVOID &)CefGetMimeType = GetProcAddress(libcef, "cef_get_mime_type");
        (LPVOID &)CefRequest_Create = GetProcAddress(libcef, "cef_request_create");
        (LPVOID &)CefURLRequest_create = GetProcAddress(libcef, "cef_urlrequest_create");
        (LPVOID &)CefStringMultimap_Alloc = GetProcAddress(libcef, "cef_string_multimap_alloc");
        (LPVOID &)CefStringMultimap_Free = GetProcAddress(libcef, "cef_string_multimap_free");
        (LPVOID &)CefRegisterExtension = GetProcAddress(libcef, "cef_register_extension");
        (LPVOID &)CefDictionaryValue_Create = GetProcAddress(libcef, "cef_dictionary_value_create");
        (LPVOID &)CefStreamReader_CreateForFile = GetProcAddress(libcef, "cef_stream_reader_create_for_file");
        (LPVOID &)CefStreamReader_CreateForData = GetProcAddress(libcef, "cef_stream_reader_create_for_data");
        (LPVOID &)CefProcessMessage_Create = GetProcAddress(libcef, "cef_process_message_create");
        (LPVOID &)CefV8Context_GetCurrentContext = GetProcAddress(libcef, "cef_v8context_get_current_context");
        (LPVOID &)CefServer_Create = GetProcAddress(libcef, "cef_server_create");
        (LPVOID &)CefURIDecode = GetProcAddress(libcef, "cef_uridecode");

        (LPVOID &)CefString_Set = GetProcAddress(libcef, "cef_string_utf16_set");
        (LPVOID &)CefString_Clear = GetProcAddress(libcef, "cef_string_utf16_clear");
        (LPVOID &)CefString_FromUtf8 = GetProcAddress(libcef, "cef_string_utf8_to_utf16");
        (LPVOID &)CefString_FromWide = GetProcAddress(libcef, "cef_string_wide_to_utf16");
        (LPVOID &)CefString_UserFree_Free = GetProcAddress(libcef, "cef_string_userfree_utf16_free");
        (LPVOID &)CefString_ToUtf8 = GetProcAddress(libcef, "cef_string_utf16_to_utf8");
        (LPVOID &)CefString_ClearUtf8 = GetProcAddress(libcef, "cef_string_utf8_clear");

        (LPVOID &)CefV8Value_CreateNull = GetProcAddress(libcef, "cef_v8value_create_null");
        (LPVOID &)CefV8Value_CreateInt = GetProcAddress(libcef, "cef_v8value_create_int");
        (LPVOID &)CefV8Value_CreateString = GetProcAddress(libcef, "cef_v8value_create_string");
        (LPVOID &)CefV8Value_CreateFunction = GetProcAddress(libcef, "cef_v8value_create_function");
        (LPVOID &)CefV8Value_CreateArray = GetProcAddress(libcef, "cef_v8value_create_array");
        (LPVOID &)CefV8Value_CreateBool = GetProcAddress(libcef, "cef_v8value_create_bool");

        // Find CefContext::GetBackGroundColor().
        if (is_browser)
        {
            const char *pattern = "55 89 E5 53 56 8B 55 0C 8B 45 08 83 FA 01 74 09";
            auto GetBackgroundColor = (GetBackgroundColor_t)utils::patternScan(libcef, pattern);

            // Hook CefContext::GetBackGroundColor().
            Old_GetBackgroundColor.hook(GetBackgroundColor, Hooked_GetBackgroundColor);
        }

        return true;
    }

    return false;
}