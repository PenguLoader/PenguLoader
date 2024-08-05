#include "browser.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_keyboard_handler_capi.h"

// BROWSER PROCESS ONLY.

#ifndef OS_WIN
#define VK_F12    0x7B
#define VK_RETURN 0x0D
#endif

static decltype(cef_keyboard_handler_t::on_pre_key_event) OnPreKeyEvent;
static int CEF_CALLBACK Hooked_OnPreKeyEvent(
    struct _cef_keyboard_handler_t* self,
    struct _cef_browser_t* browser,
    const struct _cef_key_event_t* event,
    cef_event_handle_t os_event,
    int* is_keyboard_shortcut)
{
    int code = event->windows_key_code;
    bool ctrl_shift =
#if OS_MAC
        // use command + options (alt) on mac
        (event->modifiers & (EVENTFLAG_COMMAND_DOWN | EVENTFLAG_ALT_DOWN));
#else
        (event->modifiers & (EVENTFLAG_CONTROL_DOWN | EVENTFLAG_SHIFT_DOWN));
#endif

    if (event->focus_on_editable_field)
        goto _next;

    if (code == VK_F12 || (ctrl_shift && code == 'I'))
    {
        browser::open_devtools(browser);
        return true;
    }
    else if (ctrl_shift && code == 'R')
    {
        browser->reload_ignore_cache(browser);
        return true;
    }
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

_next:
    return OnPreKeyEvent(self, browser, event, os_event, is_keyboard_shortcut);
}

void HookKeyboardHandler(cef_client_t *client)
{
    static auto GetKeyboardHandler = client->get_keyboard_handler;
    client->get_keyboard_handler = [](cef_client_t *self) -> cef_keyboard_handler_t *
    {
        auto handler = GetKeyboardHandler(self);

        OnPreKeyEvent = handler->on_pre_key_event;
        handler->on_pre_key_event = Hooked_OnPreKeyEvent;

        return handler;
    };
}