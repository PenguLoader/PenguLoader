#ifndef LEAGUE_LOADER_H
#define LEAGUE_LOADER_H

#ifdef _WIN64
#error "Build 32-bit only."
#endif

#include <stdint.h>
#include <stdio.h>
#include <atomic>
#include <string>
#include <windows.h>

#include "include/internal/cef_string.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_v8_capi.h"

#define DEVTOOLS_WINDOW_NAME    L"DevTools - League Client"

namespace league_loader
{
    template <typename T>
    class CefRefCount : public T
    {
    public:
        template <typename U>
        CefRefCount(const U *) : ref_(1) {
            base.size = sizeof(U);
            base.add_ref = _Base_AddRef;
            base.release = _Base_Release;
            base.has_one_ref = _Base_HasOneRef;
            base.has_at_least_one_ref = _Base_HasAtLeastOneRef;
            self_delete_ = [](void *self) { delete static_cast<U *>(self); };
        }

    private:
        void(*self_delete_)(void *);
        std::atomic<size_t> ref_;

        static void CALLBACK _Base_AddRef(cef_base_ref_counted_t *_)
        { ++reinterpret_cast<CefRefCount *>(_)->ref_; }

        static int CALLBACK _Base_Release(cef_base_ref_counted_t *_)
        { return !--reinterpret_cast<CefRefCount *>(_)->ref_ ?
            (reinterpret_cast<CefRefCount *>(_)->self_delete_(_), 1) : 0; }

        static int CALLBACK _Base_HasOneRef(cef_base_ref_counted_t *_)
        { return reinterpret_cast<CefRefCount *>(_)->ref_ == 1; }

        static int CALLBACK _Base_HasAtLeastOneRef(cef_base_ref_counted_t *_)
        { return reinterpret_cast<CefRefCount *>(_)->ref_ != 0; }
    };

    // cef_string_t wrapper.
    class CefStr : public cef_string_t
    {
    public:
        CefStr(const std::string &s);
        CefStr(const std::wstring &s);
        CefStr(const cef_string_t *s);
        CefStr(cef_string_userfree_t uf);
        CefStr(int32_t i);
        CefStr(uint32_t u);

        bool equal(const wchar_t *s) const;
        bool equal(const std::wstring &s) const;
        bool equali(const wchar_t *s) const;
        bool equali(const std::wstring &s) const;
        bool contain(const wchar_t *s) const;
        bool contain(const std::wstring &s) const;
        bool operator ==(const wchar_t *s) const { return equal(s); }
        bool operator ==(const std::wstring &s) const { return equal(s); }
    };

    // CEF functions.
    extern decltype(&cef_get_mime_type) CefGetMimeType;
    extern decltype(&cef_register_extension) CefRegisterExtension;
    extern decltype(&cef_dictionary_value_create) CefDictionaryValue_Create;
    extern decltype(&cef_stream_reader_create_for_file) CefStreamReader_CreateForFile;

    // Strings helpers.
    extern decltype(&cef_string_set) CefString_Set;
    extern decltype(&cef_string_clear) CefString_Clear;
    extern decltype(&cef_string_from_ascii) CefString_FromUtf8;
    extern decltype(&cef_string_from_wide) CefString_FromWide;
    extern decltype(&cef_string_userfree_free) CefString_UserFree_Free;

    // V8 values.
    extern decltype(&cef_v8value_create_null) CefV8Value_CreateNull;
    extern decltype(&cef_v8value_create_int) CefV8Value_CreateInt;
    extern decltype(&cef_v8value_create_string) CefV8Value_CreateString;
    extern decltype(&cef_v8value_create_function) CefV8Value_CreateFunction;
    extern decltype(&cef_v8value_create_array) CefV8Value_CreateArray;

    // Hooking entries.
    extern decltype(&cef_initialize) CefInitialize;
    extern decltype(&cef_execute_process) CefExecuteProcess;
    extern decltype(&cef_browser_host_create_browser) CefBrowserHost_CreateBrowser;

#   define IPC_WRITE(process, address, size) \
        WriteProcessMemory(process, address, address, size, NULL)
#   define IPC_READ(process, address, size) \
        ReadProcessMemory(process, address, address, size, NULL)
#   define IPC_CALL(process, address, param) \
        CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)address, (LPVOID)param, 0, NULL)

    std::wstring GetPluginsDir();
    std::wstring GetAssetsDir();
    std::wstring GetConfigValue(const std::wstring &key);

    static bool str_contain(std::wstring str, std::wstring sub)
    {
        if (str.empty()) return false;
        if (str.length() < sub.length()) return false;

        for (size_t i = 0; i < str.length(); i++)
            str[i] = towlower(str[i]);

        for (size_t i = 0; i < sub.length(); i++)
            sub[i] = towlower(sub[i]);

        return wcsstr(str.c_str(), sub.c_str()) != NULL;
    }

    cef_resource_handler_t * CreateAssetsHandler(const std::wstring &path);

    void OpenDevTools(bool remote);
}

#endif