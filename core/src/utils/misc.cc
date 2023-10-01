#include "commons.h"

void *utils::patternScan(const HMODULE module, const char *pattern)
{
    if (!module)
        return nullptr;

    static auto PatternToBytes = [](const char *Pattern)
    {
        vec<int> Bytes{};
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