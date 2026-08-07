#include "pengu.h"

#if OS_WIN
#include <io.h>
#elif OS_MAC
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
    // lstat, not stat: stat follows the link and would report the target's
    // type, so every symlink would answer false. PluginFS's sandbox relies on
    // this being accurate at every path component.
    struct stat buffer;
    if (lstat(path.string().c_str(), &buffer) == 0)
        return S_ISLNK(buffer.st_mode);

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

/// Temp names must not collide, or two concurrent writers to the same target
/// each clobber the other's partial content and the rename publishes whichever
/// half-written file won. The counter covers threads within this process; the
/// pid covers the several renderer processes LCUX spawns, which share the
/// plugins folder and the data root.
static path make_temp_path(const path &target)
{
    static std::atomic<uint64_t> counter{ 1 };
    auto nth = counter.fetch_add(1);

    path temp(target);
#if OS_WIN
    temp += L".pengu-tmp." + std::to_wstring(GetCurrentProcessId())
          + L"." + std::to_wstring(nth);
#else
    temp += ".pengu-tmp." + std::to_string(getpid())
          + "." + std::to_string(nth);
#endif
    return temp;
}

bool file::atomic_write(const path &target, const void *buffer, size_t length)
{
    path temp = make_temp_path(target);

#if OS_WIN
    FILE *fp = _wfopen(temp.c_str(), L"wb");
#else
    FILE *fp = fopen(temp.c_str(), "wb");
#endif

    if (fp == nullptr)
        return false;

    // fwrite's return is the whole point -- a short write (disk full, quota)
    // otherwise renames a truncated file over a good one.
    bool ok = length == 0 || fwrite(buffer, 1, length, fp) == length;

    // Push the bytes past our own buffering and the OS cache *before* the
    // rename. Without this the rename can reach disk first, replacing a good
    // file with a zero-length one -- losing the original, which is the exact
    // failure this function exists to prevent.
    //
    // Deliberately not F_FULLFSYNC on macOS: that is a full drive-cache
    // barrier costing tens of ms to seconds, and the threat model here is a
    // process crash, not power loss.
    if (ok)
        ok = fflush(fp) == 0;

    if (ok)
    {
#if OS_WIN
        ok = _commit(_fileno(fp)) == 0;
#else
        ok = fsync(fileno(fp)) == 0;
#endif
    }

    if (fclose(fp) != 0)
        ok = false;

    std::error_code ec;

    if (ok)
    {
#if OS_WIN
        // MoveFileExW rather than std::filesystem::rename for
        // MOVEFILE_WRITE_THROUGH, which holds the call until the rename itself
        // is on disk. Both replace an existing target.
        ok = MoveFileExW(temp.c_str(), target.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        std::filesystem::rename(temp, target, ec);
        ok = !ec;
#endif
    }

    // On success the temp no longer exists under that name; on failure it is
    // ours to clean up. Best-effort -- an orphan is untidy, not harmful.
    if (!ok)
        std::filesystem::remove(temp, ec);

    return ok;
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