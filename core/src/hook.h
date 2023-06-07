#pragma once

#include <mutex>
#include <windows.h>

// Special thanks to https://github.com/nbqofficial/divert/

template<typename>
class Hook;

template<typename R, typename ...Args>
class Hook<R(Args...)>
{
public:
    using Fn = R(*)(Args...);

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
        code.addr = reinterpret_cast<intptr_t>(hook);
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
            RestoreGuard temp(orig_func_, orig_code_);
            {
                return orig_func_(args...);
            }
        }
    }

private:
    Fn orig_func_;
    void *orig_code_;
    std::mutex mutex_;

#   pragma pack(push, 1)
    struct Shellcode
    {
#       ifdef _WIN64
        uint8_t movabs = 0x48;      // x86                  x86_64                 
#       endif                       //
        uint8_t mov_eax = 0xB8;     // mov eax [addr]   |   movabs rax [addr]
        intptr_t addr;              //
        uint8_t push_eax = 0x50;    // push eax         |   push rax
        uint8_t ret = 0xC3;         // ret              |   ret
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

    private:
        void *func_;
        uint8_t backup_[sizeof(Shellcode)];
    };
};