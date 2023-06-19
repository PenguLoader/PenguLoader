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

private:
#   pragma pack(push, 1)
    struct Shellcode
    {
#   ifdef _WIN64
        uint8_t movabs    = 0x48;   // x86                  x86_64                 
#   endif                           //
        uint8_t mov_eax   = 0xB8;   // mov eax [addr]   |   movabs rax [addr]
        intptr_t addr;              //
        uint8_t push_eax  = 0x50;   // push eax         |   push rax
        uint8_t ret       = 0xC3;   // ret              |   ret

        Shellcode(intptr_t addr) : addr(addr) {}
    };
#   pragma pack(pop)

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