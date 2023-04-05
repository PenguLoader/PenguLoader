#include "../internal.h"

HWND rclient_window_ = nullptr;
extern cef_browser_t *browser_;
void OpenDevTools_Internal(bool remote);

static WNDPROC Old_WidgetWndProc;
static LRESULT CALLBACK Hooked_WidgetWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_KEYDOWN:
        {
            if ((HIWORD(lp) & KF_REPEAT) == 0)  // no repeat
            {
                bool ctrl_shift = GetKeyState(VK_CONTROL) < 0
                    && GetKeyState(VK_SHIFT) < 0;

                if (wp == VK_F12 || (ctrl_shift && wp == 'I'))
                {
                    OpenDevTools_Internal(false);
                }
                else if ((ctrl_shift && wp == 'R'))
                {
                    if (browser_ != nullptr)
                        browser_->reload_ignore_cache(browser_);
                }
            }

            break;
        }

        case (WM_APP + 0x101):
        {
            OpenDevTools_Internal(wp != 0);
            return 0;
        }
    }

    return Old_WidgetWndProc(hwnd, msg, wp, lp);
}

static void HooKWidgetWindow(HWND hwnd)
{
    Old_WidgetWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)Hooked_WidgetWndProc);
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

    rclient_window_ = rclient;
    HooKWidgetWindow(widgetWin);

    args->base.release(&args->base);
    host->base.release(&host->base);
}