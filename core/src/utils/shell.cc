#include "commons.h"
#include <stdlib.h>

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

void shell::open_url(const wchar_t *url)
{
    static decltype(&ShellExecuteW) pShellExecuteW = nullptr;

    if (!pShellExecuteW) {
        HMODULE shell32 = LoadLibraryA("shell32.dll");
        (LPVOID &)pShellExecuteW = GetProcAddress(shell32, "ShellExecuteW");
    }

    if (pShellExecuteW)
        pShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

void shell::open_folder(const char *path)
{
    open_url(path);
}

void shell::open_folder(const wchar_t *path)
{
    open_url(path);
}