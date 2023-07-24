#include "commons.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_keyboard_handler_capi.h"

extern HWND rclient_;
extern int main_browser_id_;

void OpenDevTools(cef_browser_t *browser);

static decltype(cef_keyboard_handler_t::on_pre_key_event) OnPreKeyEvent;
static int CEF_CALLBACK Hooked_OnPreKeyEvent(
    struct _cef_keyboard_handler_t* self,
    struct _cef_browser_t* browser,
    const struct _cef_key_event_t* event,
    cef_event_handle_t os_event,
    int* is_keyboard_shortcut)
{
    int code = event->windows_key_code;
    bool ctrl_shift = event->modifiers
        & (EVENTFLAG_CONTROL_DOWN | EVENTFLAG_SHIFT_DOWN);

    if (code == VK_F12 || (ctrl_shift && code == 'I'))
    {
        OpenDevTools(browser);
        return true;
    }
    else if (ctrl_shift && code == 'R')
    {
        browser->reload_ignore_cache(browser);
        return true;
    }
    else if (ctrl_shift && code == VK_RETURN)
    {
        if (main_browser_id_ == browser->get_identifier(browser))
        {
            static auto thread = [](LPVOID param) -> DWORD
            {
                auto browser = static_cast<cef_browser_t *>(param);
                int ret = MessageBoxA(rclient_, "Do you want to do a full League Client restart?",
                    "Pengu Loader", MB_YESNO | MB_ICONQUESTION);

                if (ret == IDYES)
                {
                    auto frame = browser->get_main_frame(browser);
                    frame->execute_java_script(frame,
                        &L"fetch('/riotclient/kill-and-restart-ux', { method: 'POST' })"_s, nullptr, 1);
                }

                return 0;
            };

            CreateThread(nullptr, 0, thread, browser, 0, nullptr);
            return true;
        }
    }

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