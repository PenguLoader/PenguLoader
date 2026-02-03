#include "browser.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_keyboard_handler_capi.h"

// BROWSER PROCESS ONLY.

#ifndef OS_WIN
#define VK_F12    0x7B
#define VK_RETURN 0x0D
#endif

static bool PreKeyEvent(cef_browser_t *browser, const cef_key_event_t *event)
{
    int code = event->windows_key_code;
    bool ctrl_shift = event->modifiers &
#if OS_MAC
        // Use Command + Options (Alt) on macOS.
        (EVENTFLAG_COMMAND_DOWN | EVENTFLAG_ALT_DOWN);
#else
        (EVENTFLAG_CONTROL_DOWN | EVENTFLAG_SHIFT_DOWN);
#endif

    // Ignore if focus is on editable field.
    if (event->focus_on_editable_field)
        return false;

    // Open DevTools: F12 or Ctrl + Shift + I
    if (code == VK_F12 || (ctrl_shift && code == 'I'))
    {
        browser::open_devtools(browser);
        return true;
    }
    // Reload ignoring cache: Ctrl + Shift + R
    else if (ctrl_shift && code == 'R')
    {
        browser->reload_ignore_cache(browser);
        return true;
    }
    // Full Client Restart: Ctrl + Shift + Enter
    else if (ctrl_shift && code == VK_RETURN)
    {
        if (dialog::confirm("Do you want to do a full League Client restart?", "Pengu Loader"))
        {
            auto frame = browser->get_main_frame(browser);
            frame->execute_java_script(frame,
                &u"fetch('/riotclient/kill-and-restart-ux', { method: 'POST' })"_s, nullptr, 1);
        }
        return true;
    }

    return false;
}

void HookKeyboardHandler(cef_client_t *client)
{
    if (!config::options::use_hotkeys())
        return;

    static auto GetKeyboardHandler = client->get_keyboard_handler;
    client->get_keyboard_handler = [](cef_client_t *self) -> cef_keyboard_handler_t *
        {
            auto handler = GetKeyboardHandler(self);

            static auto OnPreKeyEvent = handler->on_pre_key_event;
            handler->on_pre_key_event = [](struct _cef_keyboard_handler_t *self,
                struct _cef_browser_t *browser,
                const cef_key_event_t *event,
                cef_event_handle_t os_event,
                int *is_keyboard_shortcut) -> int
                {
                    if (PreKeyEvent(browser, event))
                        return 1; // handled
                    return OnPreKeyEvent(self, browser, event, os_event, is_keyboard_shortcut);
                };

            return handler;
        };
}