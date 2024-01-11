#pragma once

#if defined(_WIN32) || defined(_WIN64)
#ifndef OS_WIN
#define OS_WIN 1
#endif
#elif defined(__APPLE__) || defined(__MACH__)
#ifndef OS_MAC
#define OS_MAC 1
#endif
#else
#error "Your platform is not supported."
#endif

#if !(defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__))
#error "Target 64-bit (x86-64/AMD64) only."
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

#include <type_traits>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <regex>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#define CEF_STRING_TYPE_UTF16 1
#include "include/internal/cef_string.h"
#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_v8_capi.h"

using str = std::string;
using wstr = std::wstring;
using path = std::filesystem::path;

template <typename T>
using vec = std::vector<T>;

template <typename V>
using set = std::unordered_set<V>;

template <typename K, typename V>
using map = std::unordered_map<K, V>;

template<typename T>
struct remove_arg1;

template<typename R, typename Arg1, typename... Args>
struct remove_arg1<R(*)(Arg1, Args...)>
{
    using type = R(*)(Args...);
    using self = Arg1;
};

template <typename T>
struct method_traits;

template <typename T, typename R, typename... Args>
struct method_traits<R(T::*)(Args...)>
{
    using type = R(*)(Args...);
    using klass = T;
};

template <int id, typename This, typename M, typename R, typename Self, typename... Args>
struct self_bind_traits_base
{
    static M m_;

    static inline R CALLBACK invoke(Self self, Args ...args) noexcept {
        return (reinterpret_cast<This *>(self)->*m_)(args...);
    }
};

template <int id, typename This, typename M, typename R, typename Self, typename... Args>
typename M self_bind_traits_base<id, This, M, R, Self, Args...>::m_ = nullptr;

template <int id, typename, typename, typename>
struct self_bind_traits;

template <int id, typename This, typename M, typename R, typename Self, typename... Args>
struct self_bind_traits<id, This, M, R(*)(Self, Args...)>
    : self_bind_traits_base<id, This, M, R, Self, Args...> {};

template <int id, typename M, typename To>
static inline void self_bind(M from, To &to) noexcept
{
    using traits = self_bind_traits<id, method_traits<M>::klass, M, To>;
    if (traits::m_ == nullptr) traits::m_ = from;
    to = traits::invoke;
}

#define cef_bind_method(klass, m)                                                   \
    do {                                                                            \
        static_assert(std::is_same<method_traits<decltype(&klass::_##m)>::type,     \
            remove_arg1<decltype(m)>::type>::value, "Invalid method.");        \
        self_bind<__COUNTER__>(&klass::_##m, m);                                    \
    } while (0)

template <typename T>
struct CefRefCount : public T
{
    template <typename U>
    CefRefCount(const U *) noexcept : T{}, ref_(1) {
        T::base.size = sizeof(U);
        T::base.add_ref = _Base_AddRef;
        T::base.release = _Base_Release;
        T::base.has_one_ref = _Base_HasOneRef;
        T::base.has_at_least_one_ref = _Base_HasAtLeastOneRef;
        self_delete_ = [](void *self) noexcept { delete static_cast<U *>(self); };
    }

    CefRefCount(nullptr_t) noexcept : CefRefCount(static_cast<T *>(nullptr)) {}

private:
    void(*self_delete_)(void *);
    std::atomic<size_t> ref_;

    static void CALLBACK _Base_AddRef(cef_base_ref_counted_t *_) noexcept {
        ++reinterpret_cast<CefRefCount *>(_)->ref_;
    }

    static int CALLBACK _Base_Release(cef_base_ref_counted_t *_) noexcept {
        CefRefCount *self = reinterpret_cast<CefRefCount *>(_);
        if (--self->ref_ == 0) {
            self->self_delete_(_);
            return 1;
        }
        return 0;
    }

    static int CALLBACK _Base_HasOneRef(cef_base_ref_counted_t *_) noexcept {
        return reinterpret_cast<CefRefCount *>(_)->ref_ == 1;
    }

    static int CALLBACK _Base_HasAtLeastOneRef(cef_base_ref_counted_t *_) noexcept {
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
        return (V8Value *)cef_v8value_create_undefined();
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
    path loaderDir();
    path pluginsDir();
    path datastorePath();

    path cacheDir();
    path leagueDir();

    wstr getConfigValue(const wstr &key, const wstr &fallback = L"");
    bool getConfigValueBool(const wstr &key, bool fallback);
    int getConfigValueInt(const wstr &key, int fallback);
}

namespace utils
{
    bool isDir(const path &path);
    bool isFile(const path &path);
    bool isSymlink(const path &path);
    bool readFile(const path &path, str &out);
    vec<wstr> readDir(const path &dir);

    void *patternScan(const HMODULE module, const char *pattern);
    float getWindowScale(void *handle);
}

namespace dialog
{
    enum Level
    {
        DIALOG_NONE,
        DIALOG_INFO,
        DIALOG_WARNING,
        DIALOG_ERROR,
        DIALOG_QUESTION
    };

    void alert(const char *message, const char *title, Level level = DIALOG_NONE, const void *owner = nullptr);
    bool confirm(const char *message, const char *title, Level level = DIALOG_QUESTION, const void *owner = nullptr);
}

namespace shell
{
    void open_url(const char *url);
    void open_url(const wchar_t *url);

    void open_folder(const char *path);
    void open_folder(const wchar_t *path);
}

namespace hook
{
#   pragma pack(push, 1)
    struct Shellcode
    {
        Shellcode(intptr_t addr) : addr(addr) {}

    private:
        // Special thanks to https://github.com/nbqofficial/divert/
        uint8_t movabs = 0x48;      //                 
        uint8_t mov_rax = 0xB8;     // movabs rax [addr]
        intptr_t addr;              //
        uint8_t push_rax = 0x50;    // push rax
        uint8_t ret = 0xC3;         // ret
    };
#   pragma pack(pop)

    struct Restorable
    {
        Restorable(void *func, const void *code, size_t size)
            : func_(func)
            , backup_(new uint8_t[size]{})
            , size_(size)
        {
            memcpy(backup_, func, size);
            memcpy_safe(func, code, size);
        }

        ~Restorable()
        {
            memcpy_safe(func_, backup_, size_);
            delete[] backup_;
        }

        Restorable swap()
        {
            return Restorable(func_, backup_, size_);
        }

    private:
        void *func_;
        uint8_t *backup_;
        size_t size_;

        static void memcpy_safe(void *dst, const void *src, size_t size)
        {
            DWORD op;
            VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &op);
            memcpy(dst, src, size);
            VirtualProtect(dst, size, op, &op);
        }
    };

    template<typename>
    class Hook;

    template<typename R, typename ...Args>
    class Hook<R(*)(Args...)>
    {
    public:
        using Fn = R(*)(Args...);

        Hook()
            : orig_(nullptr)
            , rest_(nullptr)
            , mutex_{}
        {
        }

        ~Hook()
        {
            if (rest_ != nullptr)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                {
                    delete rest_;
                }
            }
        }

        bool hook(Fn orig, Fn hook)
        {
            if (orig == nullptr || hook == nullptr)
                return false;

            orig_ = orig;

            Shellcode code(reinterpret_cast<intptr_t>(hook));
            rest_ = new Restorable(orig, &code, sizeof(code));

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
                auto _t = rest_->swap();
                {
                    return orig_(args...);
                }
            }
        }

    protected:
        Fn orig_;
        Restorable *rest_;
        std::mutex mutex_;
    };
}