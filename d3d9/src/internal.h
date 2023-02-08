#ifndef _LEAGUE_LOADER_H
#define _LEAGUE_LOADER_H

#ifdef _WIN64
#error "Build 32-bit only."
#endif

#ifdef _MSC_VER
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef NOINLINE
#define NOINLINE __declspec(noinline)
#endif

#define HOOK_METHOD(fn, ...) __fastcall fn(void *EDX, void *ECX, __VA_ARGS__)

#ifndef COUNT_OF
#define COUNT_OF(arr) (sizeof(arr) / sizeof(*arr))
#endif

#include <stdint.h>
#include <stdio.h>
#include <atomic>
#include <string>
#include <vector>
#include <windows.h>

#include "include/internal/cef_string.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_v8_capi.h"
#include "include/capi/cef_request_capi.h"

using std::string;
using std::wstring;
using std::vector;

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

static cef_string_t operator""_s(const char *s, size_t l) {
    return CefStr(std::string(s, l));
}

// CEF functions.
extern decltype(&cef_get_mime_type) CefGetMimeType;
extern decltype(&cef_request_create) CefRequest_Create;
extern decltype(&cef_string_multimap_alloc) CefStringMultimap_Alloc;
extern decltype(&cef_string_multimap_free) CefStringMultimap_Free;
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
extern decltype(&cef_v8value_create_bool) CefV8Value_CreateBool;

// Hooking entries.
extern decltype(&cef_initialize) CefInitialize;
extern decltype(&cef_execute_process) CefExecuteProcess;
extern decltype(&cef_browser_host_create_browser) CefBrowserHost_CreateBrowser;

namespace config
{
    wstring getLoaderDir();
    wstring getAssetsDir();
    wstring getPluginsDir();
    wstring getConfigValue(const wstring &key);
}

namespace utils
{
    wstring toWide(const string &str);
    string toNarrow(const wstring &wstr);
    wstring encodeBase64(const wstring &str);

    bool strEqual(const wstring &a, const wstring &b, bool sensitive = true);
    bool strContain(const wstring &str, const wstring &sub, bool sensitive = true);
    bool strStartWith(const wstring &str, const wstring &sub);
    bool strEndWith(const wstring &str, const wstring &sub);

    bool fileExist(const wstring &path, bool folder);
    vector<wstring> getFiles(const wstring &dir, const wstring &search);
    bool readFile(const wstring &path, string &out);

    void hookFunc(void **orig, void *hooked);
    void hookFuncs(void **funcs[], int count);
    void *scanInternal(void *image, size_t length, const string &pattern);

    void openFilesExplorer(const wstring &path);
}

#endif