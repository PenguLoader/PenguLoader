#include "../internal.h"
#include <fstream>

bool utils::fileExist(const std::wstring &path, bool folder)
{
    DWORD attr = GetFileAttributesW(path.c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    bool isDir = (attr & FILE_ATTRIBUTE_DIRECTORY);
    return folder ? isDir : !isDir;
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

vector<std::wstring> utils::getFiles(const std::wstring &dir, const std::wstring &search)
{
    vector<wstring> files;
    auto searchPath = dir + L"\\" + search;

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                files.push_back(fd.cFileName);
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }

    return files;
}