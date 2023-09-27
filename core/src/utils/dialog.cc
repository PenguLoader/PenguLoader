#include "commons.h"

static int level_to_mbicon(dialog::Level level)
{
    switch (level) {
        case dialog::DIALOG_INFO:
            return MB_ICONINFORMATION;
        case dialog::DIALOG_WARNING:
            return MB_ICONWARNING;
        case dialog::DIALOG_ERROR:
            return MB_ICONERROR;
        case dialog::DIALOG_QUESTION:
            return MB_ICONQUESTION;
        case dialog::DIALOG_NONE:
        default:
            return 0;
    }
}

void dialog::alert(const char *message, const char *title, Level level, const void *owner)
{
    MessageBoxA((HWND)owner, message, title,
        level_to_mbicon(level) | MB_OK | MB_TOPMOST);
}

bool dialog::confirm(const char *message, const char *title, Level level, const void *owner)
{
    int result = MessageBoxA((HWND)owner, message, title,
        level_to_mbicon(level) | MB_YESNO | MB_TOPMOST);

    return result == IDYES;
}