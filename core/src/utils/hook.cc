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

static bool Detour32(char* src, char* dst, const intptr_t len)
{
    if (len < 5) return false;

    DWORD  curProtection;
    VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &curProtection);

    intptr_t  relativeAddress = (intptr_t)(dst - (intptr_t)src) - 5;

    *src = '\xE9';
    *(intptr_t*)((intptr_t)src + 1) = relativeAddress;

    VirtualProtect(src, len, curProtection, &curProtection);
    return true;
}

// Ref: https://guidedhacking.com/threads/simple-x86-c-trampoline-hook.14188/
static char* TrampHook32(char* src, char* dst, const int len)
{
    // Create the gateway (len + 5 for the overwritten bytes + the jmp)
    void* gateway = VirtualAlloc(0, len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    //Write the stolen bytes into the gateway
    memcpy(gateway, src, len);

    // Get the gateway to destination addy
    intptr_t  gatewayRelativeAddr = ((intptr_t)src - (intptr_t)gateway) - 5;

    // Add the jmp opcode to the end of the gateway
    *(char*)((intptr_t)gateway + len) = '\xE9';

    // Add the address to the jmp
    *(intptr_t*)((intptr_t)gateway + len + 1) = gatewayRelativeAddr;

    // Perform the detour
    Detour32(src, dst, len);

    return (char*)gateway;
}

void utils::hookFunc(void **orig, void *hooked)
{
    if (orig == nullptr || *orig == nullptr || hooked == nullptr)
        return;

    *orig = TrampHook32(static_cast<char *>(*orig), static_cast<char *>(hooked), 5);
}
