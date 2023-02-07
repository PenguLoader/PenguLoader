#include <vector>
#include <fstream>
#include "internal.h"

using namespace league_loader;

// RENDERER PROCESS ONLY.

static bool FileExist(const std::wstring &path, bool requireFolder = false)
{
    DWORD attr = GetFileAttributesW(path.c_str());

    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    bool isDir = (attr & FILE_ATTRIBUTE_DIRECTORY);
    return requireFolder ? isDir : !isDir;
}

static std::vector<std::wstring> GetFiles(const std::wstring &dir, const std::wstring &search)
{
    std::vector<std::wstring> files;
    auto searchPath = dir + L"\\" + search;

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(fd.cFileName);
            }
        } while (FindNextFileW(hFind, &fd));
        
        FindClose(hFind);
    }

    return files;
}

void LoadPlugins(cef_frame_t *frame, cef_v8context_t *context)
{
    auto pluginsPath = GetPluginsDir();

    if (!FileExist(pluginsPath, true))
        return;

    // Iterate through plugins folder.
    for (const auto &file : GetFiles(pluginsPath, L"*.js"))
    {
        // Skip file name starts with underscore.
        if (file[0] == L'_') continue;

        // Create require script.
        std::wstring script;
        script += L"require(\"";
        script += file.substr(0, file.length() - 3);    // Remove .js extension.
        script += L"\");";

        // Execute.
        frame->execute_java_script(frame, &CefStr(script), &""_s, 0);
    }
}

static void ReadFile(const std::wstring &path, std::string &out)
{
    std::ifstream input(path, std::ios::binary);
    out.assign((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));
    input.close();
}

enum RequireType
{
    JS_MODULE = 0,
    INDEX_MODULE,
    TEXT_CONTENT
};

bool NativeRequire(const std::wstring &genericPath, std::string &source, int &type)
{
    auto path = GetPluginsDir() + genericPath;
    auto filePath = path + L".js";

    // Check .js file.
    if (FileExist(filePath))
    {
        ReadFile(filePath, source);
        type = JS_MODULE;
        return true;
    }
    // Check if the given path is folder.
    else if (FileExist(path, true))
    {
        filePath = path + L"/index.js";
        // And it contains index.js.
        if (FileExist(filePath))
        {
            ReadFile(filePath, source);
            type = INDEX_MODULE;
            return true;
        }
    }
    else if (FileExist(path, false))
    {
        ReadFile(path, source);
        type = TEXT_CONTENT;
        return true;
    }

    return false;
}
