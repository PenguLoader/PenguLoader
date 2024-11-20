#include "pengu.h"

#if OS_MAC
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif

bool file::is_symlink(const path &path)
{
#if OS_WIN
    DWORD attr = GetFileAttributesW(path.wstring().c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    return attr & FILE_ATTRIBUTE_REPARSE_POINT;
#elif OS_MAC
    return false;
#endif
}

bool file::is_dir(const path &path)
{
#if OS_WIN
    DWORD attr = GetFileAttributesW(path.wstring().c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;
    return attr & FILE_ATTRIBUTE_DIRECTORY;
#elif OS_MAC
    struct stat buffer;
    if (stat(path.string().c_str(), &buffer) == 0) {
        return S_ISDIR(buffer.st_mode);
    }
    return false;
#endif
}

bool file::is_file(const path &path)
{
#if OS_WIN
    DWORD attr = GetFileAttributesW(path.wstring().c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;
    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
#elif OS_MAC
    struct stat buffer;
    if (stat(path.string().c_str(), &buffer) == 0) {
        return S_ISREG(buffer.st_mode);
    }
    return false;
#endif
}

bool file::read_file(const path &path, void **buffer, size_t *length)
{
#if OS_WIN
    FILE* fp = _wfopen(path.c_str(), L"rb");
#else
    FILE* fp = fopen(path.c_str(), "rb");
#endif
    if (fp != nullptr)
    {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        *buffer = malloc(size + 1);
        if (length) *length = size;

        fread(*buffer, 1, size, fp);
        reinterpret_cast<uint8_t *>(*buffer)[size] = '\0';

        fclose(fp);
        return true;
    }

    return false;
}

bool file::write_file(const path &path, const void *buffer, size_t length)
{
#if OS_WIN
    FILE *fp = _wfopen(path.c_str(), L"wb");
#else
    FILE *fp = fopen(path.c_str(), "wb");
#endif

    if (fp != nullptr)
    {
        fwrite(buffer, 1, length, fp);
        fclose(fp);
        return true;
    }

    return false;
}

std::vector<path> file::read_dir(const path &dir)
{
    std::vector<path> files;
    files.clear();

#if OS_WIN
    std::wstring target = dir.wstring() + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(target.c_str(), &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            files.push_back(fd.cFileName);
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
#elif OS_MAC
    if (DIR *pdir = opendir(dir.string().c_str())) {
        struct dirent *entry = readdir(pdir);
        while (entry != NULL) {
            if (entry->d_type & (DT_REG | DT_DIR)) {
                files.push_back(entry->d_name);
            }
            entry = readdir(pdir);
        }
        closedir(pdir);
    }
#endif

    return files;
}

bool file::make_dir(const path &dir)
{
    bool success = false;

#if OS_WIN
    success = CreateDirectoryW(dir.c_str(), NULL) != FALSE;
#elif OS_MAC
    success = mkdir(dir.string().c_str(), 0777) == 0;
#endif

    return success;
}