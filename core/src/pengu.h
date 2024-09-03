#ifndef _PENGU_H_
#define _PENGU_H_
#include "platform.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef OS_WIN
#include <windows.h>
#elif OS_MAC
#include <unistd.h>
#define CALLBACK
#endif

#include <type_traits>
#include <atomic>
#include <string>
#include <vector>
#include <filesystem>

using path = std::filesystem::path;

/// LCUX used UTF-16 CEF strings.
#define CEF_STRING_TYPE_UTF16 1
#include "include/internal/cef_string.h"
#include "include/capi/cef_base_capi.h"

///
/// We don't use the C++ API libcef_wrapper for some reasons,
/// most notably are performance and hooking.
/// 
/// C++ CEF uses virtual methods,
/// CEF CAPI just wraps them around function pointer with self.
/// 
/// For example, a virtual method should be CefV8Handler::Execute(a, b, c)
/// then we get CAPI method: cef_v8handler_t::execute(cef_v8handler_t *self, a, b, c)
/// when constructing a C object, cef_v8handler_t::execute must be assigned a function pointer,
/// but with these helpers, we can bind the method from C++ class directly to this fp
/// and skip the `self` to access the class.
/// 
/// ** Native C++ **
/// 
/// class MyHandler : public CefV8Handler {
/// public:
///     virtual Execute(...) override {
///         // impl    
///     }
///     IMPLEMENT_REFCOUNTING(MyHandler);
/// };
/// 
/// ** Basic CAPI in C++ **
/// 
/// struct MyHandler : cef_v8handler_t {
///     MyHandler() {
///         // impl ref-counter
///         cef_v8handler_t::execute = _execute;
///     }
///     static _execute(cef_v8handler_t *_self, ...) {
///         auto MyHandler *self = (MyHandler *)_self;
///         // impl
///     }
/// }
/// 
/// ** Our bindings **
/// 
/// class MyHandler : CefRefCount<cef_v8handler_t> {
/// public:
///     MyHandler() : CefRefCount(this) {
///         cef_bind_method(MyHandler, execute);
///     }
///     _execute(...) {
///         // impl
///     }
/// }
/// 

template<typename T>
struct remove_arg1;

/// Remove the first arg (self) from function sig.
template<typename R, typename Arg1, typename... Args>
struct remove_arg1<R(*)(Arg1, Args...)>
{
    using type = R(*)(Args...);
    using self = Arg1;
};

template <typename T>
struct method_traits;

/// Used to extract the method pairs with class.
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
/* typename */ M self_bind_traits_base<id, This, M, R, Self, Args...>::m_ = nullptr;

template <int id, typename, typename, typename>
struct self_bind_traits;

template <int id, typename This, typename M, typename R, typename Self, typename... Args>
struct self_bind_traits<id, This, M, R(*)(Self, Args...)>
    : self_bind_traits_base<id, This, M, R, Self, Args...> {};

template <int id, typename M, typename To>
static inline void self_bind(M from, To &to) noexcept
{
    using traits = self_bind_traits<id, typename method_traits<M>::klass, M, To>;
    if (traits::m_ == nullptr) traits::m_ = from;
    to = traits::invoke;
}

/// Use __COUNTER__ to make unique static variables on the same funtion sig.
/// `static_assert` to check method type when updating headers.
#define cef_bind_method(klass, m)                                                   \
    do {                                                                            \
        static_assert(std::is_same<method_traits<decltype(&klass::_##m)>::type,     \
            remove_arg1<decltype(m)>::type>::value, "Invalid method.");             \
        self_bind<__COUNTER__>(&klass::_##m, m);                                    \
    } while (0)

///
/// Basic reference counting for CAPI CEF objects.
/// Use it with the method bindings above.
/// 
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

/// cef string interface
struct CefStrBase : cef_string_t
{
    CefStrBase();

    ///
    /// Check if the string is empty.
    /// 
    bool empty() const;

    // some string manipulations

    bool equal(const char *that) const;
    bool contain(const char *sub) const;
    bool startw(const char *sub) const;
    bool endw(const char *sub) const;

    ///
    /// Copy the string to utf-16 std string.
    /// 
    void copy(std::u16string &to) const;

    ///
    /// Convert the string to utf-8 std string.
    /// 
    std::string to_utf8() const;

    ///
    /// Convert the string to utf-16 std string.
    /// 
    std::u16string to_utf16() const;

    ///
    /// Convert the string to fs path.
    /// 
    std::filesystem::path to_path() const;
};

struct CefStr : CefStrBase
{
    CefStr();
    ~CefStr();

    CefStr(const char *s, size_t len);
    CefStr(const char16_t *s, size_t len);
    CefStr(const std::string &s);
    CefStr(const std::u16string &s);

    cef_string_t forward();
    static CefStrBase borrow(const cef_string_t *s);
    static CefStr from_path(const path &path);

    // wrap u16string in cef_string_t on stack
    static cef_string_t wrap(const std::u16string &utf16) {
        return cef_string_t{
            (char16 *)utf16.data(),
            utf16.length(),
            nullptr
        };
    }
};

///
/// A wrapper of `cef_string_userfree_t` to auto-free.
/// Do not free userfree pointer after passed to this struct.
///
struct CefScopedStr : CefStrBase
{
    ///
    /// @param uf CEF userfree string,
    ///   the `ptr()` method will return empty string when `uf` is null.
    /// 
    CefScopedStr(cef_string_userfree_t uf);
    ~CefScopedStr();

    const cef_string_t *ptr() {
        return str_;
    }

private:
    // underlying pointer
    cef_string_userfree_t str_;
};

/**
 * CefString UTF-16 literal.
*/
static inline cef_string_t operator""_s(const char16_t *s, size_t l)
{
    return cef_string_t{ (char16 *)s, l, nullptr };
}

namespace platform
{
    ///
    /// Get OS version.
    /// @returns Semver version string.
    const char *get_os_version();

    /// 
    /// Get OS build number.
    /// @returns Build number string.
    /// 
    const char *get_os_build();
}

namespace config
{
    ///
    /// Get the loader dir (root pengu folder).
    /// @returns Path to dir.
    /// 
    path loader_dir();

    ///
    /// Get the datastore path.
    /// @returns Path to datastore file.
    /// 
    path datastore_path();

    ///
    /// Get the cache dir for League Client.
    /// @returns Path to possible cache dir.
    /// 
    path cache_dir();

    ///
    /// Get the League Client dir.
    /// @returns Path to League dir.
    /// 
    path league_dir();

    ///
    /// Get the plugins dir.
    /// By default, it's a child of root dir.
    /// It could be replaced by `plugins_dir` in config.
    /// @returns Path to plugins dir.
    /// 
    path plugins_dir();

    ///
    /// Get the list of disabled plugins in hex-hashed path splitted by commas.
    /// @returns A list in string.
    /// 
    std::string disabled_plugins();

    namespace options
    {
        bool use_hotkeys();
        bool optimized_client();
        bool silent_mode();
        bool super_potato();
        bool isecure_mode();
        bool use_devtools();
        bool use_riotclient();
        bool use_proxy();

        // undocumented
        int debug_port();
    }
}

namespace file
{
    ///
    /// Check if the path is a directory.
    /// @param path Path to dir.
    /// @returns true if the dir exists.
    /// 
    bool is_dir(const path &path);

    ///
    /// Check if the path is a file.
    /// @param path Path to file.
    /// @returns true if the file exists.
    /// 
    bool is_file(const path &path);

    ///
    /// Check if the path is a symlink.
    /// @param path Path to symlink file.
    /// @returns true if the path is symlink.
    /// 
    bool is_symlink(const path &path);

    ///
    /// Read content of a file.
    /// @param path Path to file.
    /// @param buffer Output buffer, must be freed when success.
    /// @param length Output buffer length.
    /// @returns true if success.
    /// 
    bool read_file(const path &path, void **buffer, size_t *length);

    ///
    /// Read content of a file.
    /// @param path Path to file.
    /// @param buffer Output buffer.
    /// @param length Output buffer length.
    /// @returns true if success.
    /// 
    bool write_file(const path &path, const void *buffer, size_t length);

    ///
    /// Get files inside a dir.
    /// @param path Path to dir.
    /// @returns A vector of file paths.
    /// 
    std::vector<path> read_dir(const path &dir);
}

namespace dialog
{
    /// 
    /// Show a system message box, it will block the executing thread.
    /// 
    void alert(const char *message, const char *caption);

    ///
    /// Show a system message box with Yes-No buttons.
    /// @returns true if user pressed `Yes`.
    /// 
    bool confirm(const char *message, const char *caption);

#if OS_WIN
    static void alert(const char *message, const char *caption) {
        MessageBoxA(NULL, message, caption,
            MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
    }
    static bool confirm(const char *message, const char *caption) {
        return IDYES == MessageBoxA(NULL, message, caption,
            MB_ICONWARNING/* MB_ICONQUESTION */ | MB_YESNO | MB_TOPMOST);
    }
#endif
}

namespace shell
{
    /// 
    /// Open an URL in browser.
    /// @param url A string should start with `https://`.
    /// 
    void open_url(const char *url);

    /// 
    /// Open a folder path in Explorer/Finder.
    /// @param path Absolute path of folder.
    /// 
    void open_folder(const path &path);
}

namespace window
{
    /// 
    /// Get window rectangle on screen.
    /// @param handle `NSView*` on macOS, or `HWND` on Windows.
    /// 
    void get_rect(void *handle, int *x, int *y, int *w, int *h);

    /// 
    /// Get window DPI scale factor.
    /// @param handle `NSView*` on macOS, or `HWND` on Windows.
    /// 
    float get_scaling(void *handle);

    /// 
    /// Bring window to foreground.
    /// @param handle `NSView*` on macOS, or `HWND` on Windows.
    /// 
    void make_foreground(void *handle);

    /// 
    /// Set window vibrancy effect.
    /// @param handle `NSView*` on macOS, or `HWND` on Windows.
    /// @param material `NSVisualEffectMaterial` enum on macOS.
    ///      On Windows, it should be one of following values:
    ///        `0` (transparent - Windows 7+),
    ///        `1` (blurbehind -Windows 7+),
    ///        `2` (acrylic - Windows 10+),
    ///        `3` (unified - Windows 10+),
    ///        `4` (mica - Windows 11).
    /// @param state `NSVisualEffectState` enum on macOS, or accent background color on Windows.
    /// 
    void apply_vibrancy(void *handle, uint32_t material, uint32_t state);

    /// 
    /// Remove applied vibrancy effect in window.
    /// @param handle `NSView*` on macOS, or `HWND` on Windows.
    /// 
    void clear_vibrancy(void *handle);

    /// 
    /// Enable window drop shadow.
    /// @param handle `NSView*` on macOS, or `HWND` on Windows.
    /// @note Windows 7 requires aero enabled.
    /// 
    void enable_shadow(void *handle);

    /// 
    /// Check the current system appearance is dark mode.
    /// @note Support Windows 10 1607+, macOS 10.14+.
    /// 
    bool is_dark_theme();

    /// 
    /// Set default window theme.
    /// @param handle `NSView*` on macOS, or `HWND` on Windows.
    /// @note Support Windows 10 1809+, macOS 10.14+.
    /// 
    void set_theme(void *handle, bool dark);
}

namespace dylib
{
    ///
    /// Find the loaded dylib/dll.
    /// @param name Library name or full `.framework` name on macOS.
    ///
    void *find_lib(const char *name);

    ///
    /// Find symbol/proc in lib.
    /// @param lib A handle from `find_lib()`.
    /// @param proc Symbol name.
    ///
    void *find_proc(void *lib, const char *proc);

    ///
    /// Find memory in a module with matching pattern.
    /// @param rladdr Relative address near to the module's address space.
    /// @param pattern Matching pattern e.g `AA BB CC 00`, also allows wildcard `AA ?? FF`.
    ///
    void *find_memory(const void *rladdr, const char *pattern);
}

#endif