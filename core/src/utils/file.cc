#include "commons.h"
#include <fstream>

bool utils::isSymlink(const path &path)
{
    DWORD attr = GetFileAttributesW(path.wstring().c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    return attr & FILE_ATTRIBUTE_REPARSE_POINT;
}

bool utils::isDir(const path &path)
{
    DWORD attr = GetFileAttributesW(path.wstring().c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    return attr & FILE_ATTRIBUTE_DIRECTORY;
}

bool utils::isFile(const path &path)
{
    DWORD attr = GetFileAttributesW(path.wstring().c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool utils::readFile(const path &path, str &out)
{
    bool result = false;
    std::ifstream input(path, std::ios::binary);

    if (result = !input.fail())
    {
        out.assign((std::istreambuf_iterator<char>(input)),
            (std::istreambuf_iterator<char>()));
    }

    input.close();
    return result;
}

vec<wstr> utils::readDir(const path &dir)
{
    wstr target = dir.wstring() + L"\\*";
    vec<wstr> files{};

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(target.c_str(), &fd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            files.push_back(fd.cFileName);
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }

    return files;
}