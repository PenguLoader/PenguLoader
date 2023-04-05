#include "../internal.h"

#include <regex>
#include <memory>
#include <sstream>

struct PatternMask
{
    std::string binaryPattern;
    std::string mask;
};

// Converts user-friendly hex pattern string into a byte array
// and generate corresponding mask
NOINLINE PatternMask getPatternAndMask(std::string pattern)
{
    // Remove whitespaces
    pattern = std::regex_replace(pattern, std::regex("\\s+"), "");

    // Convert hex to binary
    std::stringstream patternStream;
    std::stringstream maskStream;
    for (size_t i = 0; i < pattern.length(); i += 2)
    {
        std::string byteString = pattern.substr(i, 2);

        maskStream << (byteString == "??" ? '?' : 'x');

        // Handle wildcards ourselves, rest goes to strtol
        patternStream << (byteString == "??" ? '?' : (char)strtol(byteString.c_str(), nullptr, 16));
    }

    return { patternStream.str(), maskStream.str() };
}

// Credit: superdoc1234
// Source: https://www.unknowncheats.me/forum/1364641-post150.html
NOINLINE PVOID find(PCSTR pBaseAddress, size_t memLength, PCSTR pattern, PCSTR mask)
{
    auto DataCompare = [](const auto* pData, const auto* mask, const auto* cmask, auto chLast, size_t iEnd) -> bool {
        if (pData[iEnd] != chLast) return false;
        for (size_t i = 0; i <= iEnd; ++i)
        {
            if (cmask[i] == 'x' && pData[i] != mask[i])
            {
                return false;
            }
        }

        return true;
    };

    auto iEnd = strlen(mask) - 1;
    auto chLast = pattern[iEnd];

    for (size_t i = 0; i < memLength - strlen(mask); ++i)
    {
        if (DataCompare(pBaseAddress + i, pattern, mask, chLast, iEnd))
        {
            return (PVOID)(pBaseAddress + i);
        }
    }

    return nullptr;
}

// Credit: Rake
// Source: https://guidedhacking.com/threads/external-internal-pattern-scanning-guide.14112/
NOINLINE void *utils::scanInternal(void *image, size_t length, const string &pattern)
{
    // logger->debug("DLL Base: {}, Image Size: {:X}", lpBaseOfDll, SizeOfImage);

    LPVOID match = nullptr;
    MEMORY_BASIC_INFORMATION mbi{};

    auto pm = getPatternAndMask(pattern);

    auto pMemory = static_cast<const char *>(image);
    auto pCurrentRegion = pMemory;

    do
    {
        // Skip irrelevant code regions
        auto result = VirtualQuery((LPCVOID)pCurrentRegion, &mbi, sizeof(mbi));
        if (result && mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS)
        {
            // logger->debug("Current Region: {}, Region Size: {:X}", (void*) pCurrentRegion, mbi.RegionSize);
            match = find(pCurrentRegion, mbi.RegionSize, pm.binaryPattern.c_str(), pm.mask.c_str());

            if (match != nullptr)
                break;
        }

        pCurrentRegion += mbi.RegionSize;
    } while (pCurrentRegion < pMemory + length);

    return match;
}

#pragma pack(push, 1)
struct Shellcode
{
    uint8_t jmp[1]{ 0xE9 };
    intptr_t offset;
};
#pragma pack(pop)

static bool Detour32(char* src, char* dst, const intptr_t len)
{
    if (len < sizeof(Shellcode))
        return false;

    DWORD op;
    VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &op);

    Shellcode code;
    code.offset = (intptr_t)(dst - (intptr_t)src) - sizeof(Shellcode);
    memcpy(src, &code, sizeof(Shellcode));

    VirtualProtect(src, len, op, &op);
    return true;
}

// Ref: https://guidedhacking.com/threads/simple-x86-c-trampoline-hook.14188/
static char* TrampHook32(char* src, char* dst, const int len)
{
    if (len < sizeof(Shellcode))
        return nullptr;

    void* gateway = VirtualAlloc(0, len + sizeof(Shellcode),
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    memcpy(gateway, src, len);

    Shellcode code;
    code.offset = ((intptr_t)src - (intptr_t)gateway) - sizeof(Shellcode);
    memcpy((char *)gateway + len, &code, sizeof(Shellcode));

    Detour32(src, dst, len);

    return (char*)gateway;
}

void utils::hookFunc(void **orig, void *hooked)
{
    if (orig == nullptr || *orig == nullptr || hooked == nullptr)
        return;

    *orig = TrampHook32(static_cast<char *>(*orig), static_cast<char *>(hooked), 5);
}
