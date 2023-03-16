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

    if (result = !input.fail())
    {
        out.assign((std::istreambuf_iterator<char>(input)),
            (std::istreambuf_iterator<char>()));
    }

    input.close();
    return result;
}

vector<wstring> utils::readDir(const std::wstring &dir)
{
    vector<wstring> files{};

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(dir.c_str(), &fd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            files.push_back(fd.cFileName);
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }

    return files;
}