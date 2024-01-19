#pragma once

#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <mutex>

namespace hook
{
    struct Shellcode
    {
        uint8_t opcodes[12];

        Shellcode(intptr_t addr)
        {
            // movabs rax [addr]
            opcodes[0] = 0x48;
            opcodes[1] = 0xB8;
            memcpy(&opcodes[2], &addr, sizeof(intptr_t));

            // push rax
            opcodes[10] = 0x50;

            // ret
            opcodes[11] = 0xC3;

            // TODO: macOS amd64
            // jmp qword ptr [rip + offset]
            // 0xFF 0x25 [offset 4] [addr 8] [pad 2]
        }
    };

    struct Restorable
    {
        Restorable(void* func, const void* code, size_t size)
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
        void* func_;
        uint8_t* backup_;
        size_t size_;

        static void memcpy_safe(void* dst, const void* src, size_t size)
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
                std::lock_guard<std::mutex> _lock(mutex_);
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
            rest_ = new Restorable(orig, &code.opcodes, sizeof(code.opcodes));

            return true;
        }

        bool hook(const char* lib, const char* proc, Fn hook)
        {
            if (HMODULE mod = GetModuleHandleA(lib))
                if (Fn orig = reinterpret_cast<Fn>(GetProcAddress(mod, proc)))
                    return this->hook(orig, hook);

            return false;
        }

        R operator ()(Args ...args)
        {
            std::lock_guard<std::mutex> _lock(mutex_);
            {
                Restorable _t = rest_->swap();
                {
                    return orig_(args...);
                }
            }
        }

    protected:
        Fn orig_;
        Restorable* rest_;
        std::mutex mutex_;
    };
}