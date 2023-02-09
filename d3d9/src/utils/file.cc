#include "../internal.h"
#include <fstream>

bool utils::dirExist(const std::wstring &path)
{
    DWORD attr = GetFileAttributesW(path.c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    return attr & FILE_ATTRIBUTE_DIRECTORY;
}

bool utils::fileExist(const std::wstring &path)
{
    DWORD attr = GetFileAttributesW(path.c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool utils::readFile(const std::wstring &path, std::string &out)
{
    bool result = false;
    std::ifstream input(path, std::ios::binary);

    if (result = input.good())
    {
        out.assign((std::istreambuf_iterator<char>(input)),
            (std::istreambuf_iterator<char>()));
    }

    input.close();
    return result;
}

void utils::readDir(const std::wstring &dir, const std::function<void (const wstring &, bool)> &callback)
{
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(dir.c_str(), &fd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            callback(fd.cFileName, isDir);
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }
}