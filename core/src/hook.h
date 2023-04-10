#pragma once

#include <mutex>
#include <utility> 
#include <windows.h>

// Special thanks to https://github.com/nbqofficial/divert/

template<typename Fn>
class Hook
{
public:
    Hook() : orig_func_(nullptr)
    {
    }

    ~Hook()
    {
        if (orig_func_ != nullptr)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            memcpy_safe(orig_func_, orig_code_, sizeof(Shellcode));
            free(orig_code_);
        }
    }

    bool hook(Fn orig, Fn hook)
    {
        if (orig == nullptr || hook == nullptr)
            return false;

        orig_func_ = orig;
        orig_code_ = malloc(sizeof(Shellcode));
        memcpy(orig_code_, orig, sizeof(Shellcode));

        Shellcode code;
        code.addr = static_cast<void *>(hook);
        memcpy_safe(orig, &code, sizeof(Shellcode));
    }

    bool hook(const char *lib, const char *proc, Fn hook)
    {
        if (HMODULE mod = GetModuleHandleA(lib))
            if (Fn orig = reinterpret_cast<Fn>(GetProcAddress(mod, proc)))
                return this->hook(orig, hook);

        return false;
    }

    template<typename ...Args>
    auto operator ()(Args &&...args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        {
            RestoreGuard temp(orig_func_, orig_code_);
            return orig_func_(std::forward<Args>(args)...);
        }
    }

private:
    Fn orig_func_;
    void *orig_code_;
    std::mutex mutex_;

    using byte = unsigned char;

#   pragma pack(push, 1)
    struct Shellcode
    {
#       ifdef _WIN64
        byte movabs = 0x48;     // eax -> rax
#       endif
        byte mov_eax = 0xB8;
        void *addr;
        byte push_eax = 0x50;
        byte ret = 0xC3;
    };
#   pragma pack(pop)

    static void memcpy_safe(void *src, const void *dst, size_t size)
    {
        DWORD op;
        VirtualProtect(src, size, PAGE_EXECUTE_READWRITE, &op);
        memcpy(src, dst, size);
        VirtualProtect(src, size, op, &op);
    }

    struct RestoreGuard
    {
        RestoreGuard(void *func, const void *code) : func_(func)
        {
            memcpy(backup_, func, sizeof(Shellcode));
            memcpy_safe(func, code, sizeof(Shellcode));
        }

        ~RestoreGuard()
        {
            memcpy_safe(func_, backup_, sizeof(Shellcode));
        }

        void *func_;
        byte backup_[sizeof(Shellcode)];
    };
};