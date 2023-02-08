#include "../internal.h"

void utils::openFilesExplorer(const wstring &path)
{
    ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOW);
}