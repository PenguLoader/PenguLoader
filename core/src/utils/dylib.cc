#include "pengu.h"
#include <algorithm>
#include <vector>

#if OS_MAC
#define dylib __dylib
#include <dlfcn.h>
#include <mach-o/dyld.h>
#undef dylib
#endif

void *dylib::find_lib(const char *name)
{
#if OS_WIN
    return (void *)GetModuleHandleA(name);
#elif OS_MAC
    return dlopen(name, RTLD_NOLOAD | RTLD_LAZY);
#endif
}

void *dylib::find_proc(void *lib, const char *proc)
{
#if OS_WIN
    return (void *)GetProcAddress((HMODULE)lib, proc);
#elif OS_MAC
    return dlsym(lib, proc);
#endif
}

static auto pattern_to_bytes(const char *pattern, bool *wildcard)
{
    *wildcard = false;
    std::vector<int> bytes;
    const char *end = pattern + strlen(pattern);

    for (const char *cur = pattern; cur < end; ++cur)
    {
        if (*cur == '?')
        {
            ++cur;
            if (*cur == '?')
                ++cur;
            bytes.push_back(-1);
            *wildcard = true;
        }
        else
        {
            char *end;
            bytes.push_back(strtol(cur, &end, 16));
            cur = end;
        }
    }
    return bytes;
}

static void *scan_memory_pattern(void *data, size_t length, const std::vector<int> &pattern)
{
    size_t pattern_size = pattern.size();
    auto patern_bytes = pattern.data();

    size_t find_size = length - pattern_size;
    auto scan_bytes = reinterpret_cast<uint8_t *>(data);

    for (size_t i = 0; i < find_size; ++i)
    {
        for (size_t j = 0; j < pattern_size; ++j)
            if (scan_bytes[i + j] != patern_bytes[j] && patern_bytes[j] != -1)
                goto next;
        return scan_bytes + i;
    next:;
    }

    return nullptr;
}

static void *scan_memory_bytes(void *data, size_t length, const std::vector<int> &find_bytes)
{
    auto image_start = (uint8_t *)data;
    auto image_end = image_start + length;

    auto occurrence = std::search(image_start, image_end, find_bytes.begin(), find_bytes.end());
    return (occurrence != image_end) ? occurrence : nullptr;
}

static void *get_base_address(const void *rladdr)
{
#if OS_WIN
    HMODULE module;
    DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExA(flags, (LPCSTR)rladdr, &module))
    {
        return (void *)module;
    }
#elif OS_MAC
    Dl_info info;
    if (dladdr(rladdr, &info))
    {
        return info.dli_fbase;
    }
#endif
    return nullptr;
}

static size_t get_lib_size(void *base_address)
{
#if OS_WIN
    auto dos_header = (IMAGE_DOS_HEADER *)base_address;
    auto nt_headers = (IMAGE_NT_HEADERS *)((uint8_t *)(base_address) + dos_header->e_lfanew);
    return nt_headers->OptionalHeader.SizeOfImage;
#elif OS_MAC
    struct mach_header_64 *header = (struct mach_header_64 *)base_address;

    size_t size = sizeof(*header); // Size of the header
    size += header->sizeofcmds;    // Size of the load commands

    struct load_command *lc = (struct load_command *)(header + 1);
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (lc->cmd == LC_SEGMENT_64)
        {
            size += ((struct segment_command_64 *)lc)->vmsize; // Size of segments
        }
        lc = (struct load_command *)((char *)lc + lc->cmdsize);
    }
    return size;
#endif
}

void *dylib::find_memory(const void *rladdr, const char *pattern)
{
    bool wildcard;
    auto patern_bytes = pattern_to_bytes(pattern, &wildcard);
    if (!patern_bytes.size())
        return nullptr;

    void *base_address = get_base_address(rladdr);
    if (!base_address)
        return nullptr;

    size_t lib_size = get_lib_size(base_address);
    if (!lib_size)
        return nullptr;

    if (wildcard)
        return scan_memory_pattern(base_address, lib_size, patern_bytes);
    else
        return scan_memory_bytes(base_address, lib_size, patern_bytes);
}