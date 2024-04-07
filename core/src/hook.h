#ifndef _HOOK_H_
#define _HOOK_H_

#include "platform.h"
#include <stdint.h>
#include <string.h>
#include <mutex>

#if OS_WIN
#include <windows.h>
#elif OS_MAC
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <dlfcn.h>
#endif

namespace hook
{
    struct Shellcode
    {
#ifdef OS_WIN
        uint8_t opcodes[12];
#elif OS_MAC
        uint8_t opcodes[16];
#endif
        Shellcode(intptr_t addr)
        {
            memset(opcodes, 0, sizeof(opcodes));
#ifdef OS_WIN
            // movabs rax [addr]
            opcodes[0] = 0x48;
            opcodes[1] = 0xB8;
            memcpy(&opcodes[2], &addr, sizeof(intptr_t));
            // push rax
            opcodes[10] = 0x50;
            // ret
            opcodes[11] = 0xC3;
#elif OS_MAC
            // jmp qword ptr [rip + offset] ; pad 2
            opcodes[0] = 0xFF;
            opcodes[1] = 0x25;
            memset(&opcodes[2], 0, sizeof(int32_t));
            memcpy(&opcodes[6], &addr, sizeof(intptr_t));
#endif
        }
    };

    struct Restorable
    {
        Restorable(void *func, const void *code, size_t size)
            : func_(func), size_(size), backup_(new uint8_t[size])
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

        static bool memcpy_safe(void *dst, const void *src, size_t size)
        {
#ifdef OS_WIN
            DWORD op;
            BOOL success = VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &op);
            if (success == 0)
                return false;
            memcpy(dst, src, size);
            success = VirtualProtect(dst, size, op, &op);
            return success != 0;
#elif OS_MAC
            kern_return_t kr;
            kr = mach_vm_protect(mach_task_self(),
                                 (mach_vm_address_t)dst, (mach_vm_size_t)size,
                                 FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE | VM_PROT_COPY);
            if (kr != KERN_SUCCESS)
                return false;

            kr = mach_vm_write(mach_task_self(),
                               (mach_vm_address_t)dst, (vm_offset_t)src, size);
            if (kr != KERN_SUCCESS)
                return false;

            kr = mach_vm_protect(mach_task_self(),
                                 (mach_vm_address_t)dst, (mach_vm_size_t)size,
                                 FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
            return kr == KERN_SUCCESS;
#endif
        }
    };

    template <typename>
    class Hook;

    template <typename R, typename... Args>
    class Hook<R (*)(Args...)>
    {
    public:
        using Fn = R (*)(Args...);

        Hook() : orig_(nullptr), rest_(nullptr), mutex_{}
        {
        }

        ~Hook()
        {
            if (rest_)
            {
                std::lock_guard<std::mutex> _l(mutex_);
                {
                    delete rest_;
                }
            }
        }

        bool hook(Fn orig, Fn hook)
        {
            if (!orig || !hook)
                return false;

            orig_ = orig;
            Shellcode code(reinterpret_cast<intptr_t>(hook));
            rest_ = new Restorable((void *)orig, code.opcodes, sizeof(code.opcodes));
            return true;
        }

        bool hook(const char *lib, const char *proc, Fn hook)
        {
#if OS_WIN
            if (HMODULE mod = GetModuleHandleA(lib))
                if (Fn orig = reinterpret_cast<Fn>(GetProcAddress(mod, proc)))
#elif OS_MAC
            if (void *mod = dlopen(lib, RTLD_NOLOAD | RTLD_LAZY))
                if (Fn orig = reinterpret_cast<Fn>(dlsym(mod, proc)))
#endif
                    return this->hook(orig, hook);
            return false;
        }

        R operator()(Args... args)
        {
            std::lock_guard<std::mutex> _l(mutex_);
            {
                Restorable _t = rest_->swap();
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

#endif