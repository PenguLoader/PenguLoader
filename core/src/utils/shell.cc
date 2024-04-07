#include "commons.h"
#include <stdlib.h>

#if OS_WIN

void shell::open_url(const char *url)
{
    static decltype(&ShellExecuteA) pShellExecuteA = nullptr;

    if (!pShellExecuteA) {
        HMODULE shell32 = LoadLibraryA("shell32.dll");
        (LPVOID &)pShellExecuteA = GetProcAddress(shell32, "ShellExecuteA");
    }

    if (pShellExecuteA)
        pShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

void shell::open_folder(const path &path)
{
    static decltype(&ShellExecuteW) pShellExecuteW = nullptr;

    if (!pShellExecuteW) {
        HMODULE shell32 = LoadLibraryA("shell32.dll");
        (LPVOID &)pShellExecuteW = GetProcAddress(shell32, "ShellExecuteW");
    }

    if (pShellExecuteW)
        pShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

#elif OS_MAC

void shell::open_folder(const path &path)
{
    extern void open_folder_utf8(const char *path);
    open_folder_utf8(path.c_str());
}

#endif