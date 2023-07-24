#include "commons.h"

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
            if (browser_ != nullptr
                && (HIWORD(lp) & KF_REPEAT) == 0)  // no repeat
            {
                bool ctrl_shift = GetKeyState(VK_CONTROL) < 0
                    && GetKeyState(VK_SHIFT) < 0;

                if (wp == VK_F12 || (ctrl_shift && wp == 'I')) // F12 or Ctrl Shift I
                {
                    OpenDevTools_Internal(false);
                }
                else if (ctrl_shift && wp == 'R') // Ctrl Shift R
                {
                    browser_->reload_ignore_cache(browser_);
                }
                else if (ctrl_shift && wp == VK_RETURN) // Ctrl Shift Enter
                {
                    if (MessageBoxA(rclient_window_, "Would you like to restart your League Client?",
                        "Pengu Loader", MB_ICONQUESTION | MB_YESNO) == IDYES)
                    {
                        auto frame = browser_->get_main_frame(browser_);
                        frame->execute_java_script(frame,
                            &L"fetch('/riotclient/kill-and-restart-ux', { method: 'POST' })"_s, nullptr, 1);
                    }
                }
            }

            break;
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
    if (rclient_window_ != nullptr)
        return;

    auto host = browser->get_host(browser);
    // Get needed windows.
    HWND browserWin = host->get_window_handle(host);
    HWND rclient = GetParent(browserWin);
    HWND widgetWin = FindWindowExA(browserWin, NULL, "Chrome_WidgetWin_0", NULL);
    HWND widgetHost = FindWindowExA(widgetWin, NULL, "Chrome_RenderWidgetHostHWND", NULL);

    // Ensure transparency effect.

    // Hide Chrome_RenderWidgetHostHWND.
    ShowWindow(widgetHost, SW_HIDE);
    // Hide CefBrowserWindow.
    ShowWindow(browserWin, SW_HIDE);
    // Bring Chrome_WidgetWin_0 into top-level children.
    SetParent(widgetWin, rclient);

    // Send RCLIENT HWND to renderer.
    auto msg = cef_process_message_create(&L"__rclient"_s);
    auto args = msg->get_argument_list(msg);
    args->set_int(args, 0, (int32_t)reinterpret_cast<intptr_t>(rclient));
    frame->send_process_message(frame, PID_RENDERER, msg);

    rclient_window_ = rclient;
    HooKWidgetWindow(widgetWin);

    args->base.release(&args->base);
    host->base.release(&host->base);
}