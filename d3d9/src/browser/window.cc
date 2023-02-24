#include "../internal.h"

extern cef_browser_t *browser_;
void OpenDevTools_Internal(bool remote);

#define HK_DEVTOOLS     0x101
#define HK_RELOAD       0x102
#define WM_DEVTOOLS     (WM_APP + HK_DEVTOOLS)

static LRESULT CALLBACK MsgWin_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_HOTKEY:
        {
            auto rclient = (HWND)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (GetForegroundWindow() != rclient)
                return 0;

            switch (wp)
            {
                case HK_DEVTOOLS:
                    OpenDevTools_Internal(false);
                    break;
                case HK_RELOAD:
                    if (browser_ != nullptr)
                        browser_->reload_ignore_cache(browser_);
                    break;
            }

            return 0;
        }

        case WM_DEVTOOLS:
        {
            OpenDevTools_Internal(wp != 0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

static void SetUptHotkeys(HWND rclient)
{
    wstring classn = L"LL.MSG.";
    classn.append(std::to_wstring(GetCurrentProcessId()));

    WNDCLASS wc{};
    wc.lpfnWndProc = MsgWin_WndProc;
    wc.lpszClassName = classn.c_str();

    auto msg = CreateWindowEx(0, MAKEINTATOM(RegisterClass(&wc)),
        NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    SetWindowLongPtr(msg, GWLP_USERDATA, (LONG_PTR)rclient);

    RegisterHotKey(msg, HK_DEVTOOLS, MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, 'I');
    RegisterHotKey(msg, HK_RELOAD, MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, 'R');
}

void SetUpBrowserWindow(cef_browser_t *browser, cef_frame_t *frame)
{
    auto host = browser->get_host(browser);
    // Get needed windows.
    HWND browserWin = host->get_window_handle(host);
    HWND rclient = GetParent(browserWin);
    HWND widgetWin = FindWindowExA(browserWin, NULL, "Chrome_WidgetWin_0", NULL);
    HWND widgetHost = FindWindowExA(widgetWin, NULL, "Chrome_RenderWidgetHostHWND", NULL);

    // Hide Chrome_RenderWidgetHostHWND.
    ShowWindow(widgetHost, SW_HIDE);
    // Hide CefBrowserWindow.
    ShowWindow(browserWin, SW_HIDE);
    // Bring Chrome_WidgetWin_0 into top-level children.
    SetParent(widgetWin, rclient);

    // Send RCLIENT HWND to renderer.
    auto msg = CefProcessMessage_Create(&"__rclient"_s);
    auto args = msg->get_argument_list(msg);
    args->set_int(args, 0, static_cast<int>((DWORD)rclient));
    frame->send_process_message(frame, PID_RENDERER, msg);

    // Set hotkeys.
    SetUptHotkeys(rclient);
}