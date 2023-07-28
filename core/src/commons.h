#pragma once

#ifndef _WIN64
#error "Build 64-bit only."
#endif

#ifdef _MSC_VER
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef COUNT_OF
#define COUNT_OF(arr) (sizeof(arr) / sizeof(*arr))
#endif

#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <regex>
#include <unordered_set>
#include <unordered_map>

#include "include/internal/cef_string.h"
#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_v8_capi.h"

using str = std::string;
using wstr = std::wstring;

template <typename T>
using vec = std::vector<T>;

template <typename V>
using set = std::unordered_set<V>;

template <typename K, typename V>
using map = std::unordered_map<K, V>;

template <typename T>
struct CefRefCount : public T
{
    template <typename U>
    CefRefCount(const U *) : T{}, ref_(1) {
        base.size = sizeof(U);
        base.add_ref = _Base_AddRef;
        base.release = _Base_Release;
        base.has_one_ref = _Base_HasOneRef;
        base.has_at_least_one_ref = _Base_HasAtLeastOneRef;
        self_delete_ = [](void *self) { delete static_cast<U *>(self); };
    }

    CefRefCount(nullptr_t) : CefRefCount(static_cast<T *>(nullptr)) {}

private:
    void(*self_delete_)(void *);
    std::atomic<size_t> ref_;

    static void CALLBACK _Base_AddRef(cef_base_ref_counted_t *_) {
        ++reinterpret_cast<CefRefCount *>(_)->ref_;
    }

    static int CALLBACK _Base_Release(cef_base_ref_counted_t *_) {
        CefRefCount *self = reinterpret_cast<CefRefCount *>(_);
        if (--self->ref_ == 0) {
            self->self_delete_(_);
            return 1;
        }
        return 0;
    }

    static int CALLBACK _Base_HasOneRef(cef_base_ref_counted_t *_) {
        return reinterpret_cast<CefRefCount *>(_)->ref_ == 1;
    }

    static int CALLBACK _Base_HasAtLeastOneRef(cef_base_ref_counted_t *_) {
        return reinterpret_cast<CefRefCount *>(_)->ref_ > 0;
    }
};

struct CefStrUtf8 : cef_string_utf8_t
{
    CefStrUtf8();
    CefStrUtf8(const cef_string_t *s);
    ~CefStrUtf8();

    ::str cstr() const;
};

struct CefStrBase : cef_string_t
{
    CefStrBase();

    bool empty() const;
    bool equal(const wchar_t *s) const;
    bool equal(const wstr &s) const;
    bool equali(const wchar_t *s) const;
    bool equali(const wstr &s) const;

    bool search(const wstr &regex, bool icase = false) const;

    wstr cstr() const;
};

// cef_string_t wrapper.
struct CefStr : CefStrBase
{
    CefStr();

    CefStr(const char *s, size_t l);
    CefStr(const wchar_t *s, size_t l);
    CefStr(const ::str &s);
    CefStr(const wstr &s);
    ~CefStr();

    cef_string_t forward();
    static CefStrBase borrow(const cef_string_t *s);
};

struct CefScopedStr : CefStrBase
{
    CefScopedStr(cef_string_userfree_t uf);
    ~CefScopedStr();

    const cef_string_t *ptr() {
        return str_;
    }

private:
    cef_string_userfree_t str_;
};

static inline cef_string_t operator""_s(const wchar_t *s, size_t l)
{
    return cef_string_t{ (char16 *)s, l, nullptr };
}

static inline cef_string_t operator""_s(const char16_t *s, size_t l)
{
    return cef_string_t{ (char16 *)s, l, nullptr };
}

struct V8ValueBase
{
    inline cef_v8value_t *ptr() {
        return &_;
    }

protected:
    cef_v8value_t _;
};

struct V8Value : V8ValueBase
{
    inline bool isUndefined() { return _.is_undefined(&_); }
    inline bool isNull() { return _.is_null(&_); }

    inline bool isBool() { return _.is_bool(&_); }
    inline bool isInt() { return _.is_int(&_); }
    inline bool isUint() { return _.is_uint(&_); }
    inline bool isDouble() { return _.is_double(&_); }
    inline bool isString() { return _.is_string(&_); }
    inline bool isObject() { return _.is_object(&_); }
    inline bool isArray() { return _.is_array(&_); }
    inline bool isFunction() { return _.is_function(&_); }

    inline bool asBool() { return _.get_bool_value(&_); }
    inline int asInt() { return _.get_int_value(&_); }
    inline uint32_t asUint() { return _.get_uint_value(&_); }
    inline double asDouble() { return _.get_double_value(&_); }
    inline cef_string_userfree_t asString() { return _.get_string_value(&_); }

    inline struct V8Array *asArray() { return reinterpret_cast<struct V8Array *>(&_); }
    inline struct V8Object *asObject() { return reinterpret_cast<struct V8Object *>(&_); }

    static inline V8Value *undefined() {
        return (V8Value *)cef_v8value_create_null();
    }

    static inline V8Value *null() {
        return (V8Value *)cef_v8value_create_null();
    }

    static inline V8Value *boolean(bool value) {
        return (V8Value *)cef_v8value_create_bool(value);
    }

    static inline V8Value *number(double value) {
        return (V8Value *)cef_v8value_create_double(value);
    }

    static inline V8Value *number(int value) {
        return (V8Value *)cef_v8value_create_int(value);
    }

    static inline V8Value *string(const cef_string_t *value) {
        return (V8Value *)cef_v8value_create_string(value);
    }

    static inline V8Value *function(const cef_string_t *name, cef_v8handler_t *handler) {
        return (V8Value *)cef_v8value_create_function(name, handler);
    }
};

struct V8Array : V8ValueBase
{
    inline int length() {
        return _.get_array_length(&_);
    }

    inline V8Value *get(int index) {
        _.get_value_byindex(&_, index);
    }

    inline void set(int index, V8ValueBase *value) {
        _.set_value_byindex(&_, index, (cef_v8value_t *)value);
    }

    static inline V8Array *create(int length) {
        return (V8Array *)cef_v8value_create_array(length);
    }
};

struct V8Object : V8ValueBase
{
    inline bool has(const cef_string_t *key) {
        return _.has_value_bykey(&_, key);
    }

    inline V8Value *get(const cef_string_t *key) {
        return (V8Value *)_.get_value_bykey(&_, key);
    }

    inline void set(const cef_string_t *key, V8ValueBase *value, cef_v8_propertyattribute_t attr) {
        _.set_value_bykey(&_, key, (cef_v8value_t *)value, attr);
    }

    static inline V8Object *create() {
        return (V8Object *)cef_v8value_create_object(nullptr, nullptr);
    }
};

typedef V8Value* (*V8FunctionHandler)(const vec<V8Value *> &args);

namespace config
{
    wstr loaderDir();
    wstr assetsDir();
    wstr pluginsDir();
    wstr datastorePath();

    wstr cacheDir();
    wstr leagueDir();

    wstr getConfigValue(const wstr &key, const wstr &fallback = L"");
    bool getConfigValueBool(const wstr &key, bool fallback);
    int getConfigValueInt(const wstr &key, int fallback);
}

namespace utils
{
    bool isDir(const wstr &path);
    bool isFile(const wstr &path);
    bool isSymlink(const wstr &path);
    bool readFile(const wstr &path, str &out);
    vec<wstr> readDir(const wstr &dir);

    void openLink(const wstr &link);
    void *patternScan(const HMODULE module, const char *pattern);
}

namespace hook
{
    // Special thanks to https://github.com/nbqofficial/divert/

    template<typename>
    class Hook;

    template<typename R, typename ...Args>
    class Hook<R(*)(Args...)>
    {
    public:
        using Fn = R(*)(Args...);

        Hook() : orig_func_(nullptr), mutex_{}
        {
        }

        ~Hook()
        {
            if (orig_func_ != nullptr)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                {
                    memcpy_safe(orig_func_, orig_code_, sizeof(Shellcode));
                }
            }
        }

        bool hook(Fn orig, Fn hook)
        {
            if (orig == nullptr || hook == nullptr)
                return false;

            orig_func_ = orig;
            memcpy(orig_code_, orig, sizeof(Shellcode));

            Shellcode code(reinterpret_cast<intptr_t>(hook));
            memcpy_safe(orig, &code, sizeof(Shellcode));

            return true;
        }

        bool hook(const char *lib, const char *proc, Fn hook)
        {
            if (HMODULE mod = GetModuleHandleA(lib))
                if (Fn orig = reinterpret_cast<Fn>(GetProcAddress(mod, proc)))
                    return this->hook(orig, hook);

            return false;
        }

        R operator ()(Args ...args)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            {
                RestoreGuard<sizeof(Shellcode)> _t(orig_func_, orig_code_);
                {
                    return orig_func_(args...);
                }
            }
        }

    protected:
#       pragma pack(push, 1)
        struct Shellcode
        {
#       ifdef _WIN64
            uint8_t movabs = 0x48;      // x86                  x86_64                 
#       endif                           //
            uint8_t mov_eax = 0xB8;     // mov eax [addr]   |   movabs rax [addr]
            intptr_t addr;              //
            uint8_t push_eax = 0x50;    // push eax         |   push rax
            uint8_t ret = 0xC3;         // ret              |   ret

            Shellcode(intptr_t addr) : addr(addr) {}
        };
#       pragma pack(pop)

        Fn orig_func_;
        uint8_t orig_code_[sizeof(Shellcode)];
        std::mutex mutex_;

        static void memcpy_safe(void *dst, const void *src, size_t size)
        {
            DWORD op;
            VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &op);
            memcpy(dst, src, size);
            VirtualProtect(dst, size, op, &op);
        }

        template<int size>
        struct RestoreGuard
        {
            RestoreGuard(void *func, const void *code) : func_(func)
            {
                memcpy(backup_, func, size);
                memcpy_safe(func, code, size);
            }

            ~RestoreGuard()
            {
                memcpy_safe(func_, backup_, size);
            }

        private:
            void *func_;
            uint8_t backup_[size];
        };
    };

#ifndef _WIN64
    template<typename R, typename ...Args>
    class Hook<R(__stdcall*)(Args...)> : public Hook<R(*)(Args...)>
    {
    public:
        using FnStd = R(__stdcall*)(Args...);

        bool hook(FnStd orig, FnStd hook)
        {
            return Hook<R(*)(Args...)>::hook((Fn)orig, (Fn)hook);
        }

        bool hook(const char *lib, const char *proc, FnStd hook)
        {
            return Hook<R(*)(Args...)>::hook(lib, proc, (Fn)hook);
        }

        R operator ()(Args ...args)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            {
                RestoreGuard<sizeof(Shellcode)> _t(orig_func_, orig_code_);
                {
                    return reinterpret_cast<FnStd>(orig_func_)(args...);
                }
            }
        }
    };
#endif
}