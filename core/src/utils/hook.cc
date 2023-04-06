#include "../internal.h"

void *utils::patternScan(const HMODULE module, const char *pattern)
{
    if (!module)
        return nullptr;

    static auto PatternToBytes = [](const char *Pattern)
    {
        vector<int> Bytes{};
        char *StartPos = const_cast<char *>(Pattern);
        char *EndPos = const_cast<char *>(Pattern) + strlen(Pattern);

        for (auto CurrentChar = StartPos; CurrentChar < EndPos; ++CurrentChar)
        {
            if (*CurrentChar == '?')
            {
                ++CurrentChar;

                if (*CurrentChar == '?')
                    ++CurrentChar;

                Bytes.push_back(-1);
            }
            else
            {
                Bytes.push_back(strtoul(CurrentChar, &CurrentChar, 16));
            }
        }

        return Bytes;
    };

    auto DosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
    auto NtHeaders = reinterpret_cast<const IMAGE_NT_HEADERS *>(reinterpret_cast<uint8_t *>(module) + DosHeader->e_lfanew);

    auto PatternBytes = PatternToBytes(pattern);
    const size_t PatternBytesSize = PatternBytes.size();
    const int *PatternBytesData = PatternBytes.data();

    const size_t ImageSize = NtHeaders->OptionalHeader.SizeOfImage;
    uint8_t *ScanBytes = reinterpret_cast<uint8_t *>(module);

    for (size_t i = 0; i < ImageSize - PatternBytesSize; ++i)
    {
        bool Found = true;

        for (size_t j = 0; j < PatternBytesSize; ++j)
        {
            if (ScanBytes[i + j] != PatternBytesData[j] && PatternBytesData[j] != -1)
            {
                Found = false;
                break;
            }
        }

        if (Found)
            return &ScanBytes[i];
    }

    return nullptr;
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
