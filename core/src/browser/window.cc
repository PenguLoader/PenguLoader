#include "browser.h"
#include "hook.h"

cef_window_handle_t browser::window = NULL;

#if OS_WIN
static hook::Hook<decltype(&ShowWindow)> Old_ShowWindow;
static BOOL WINAPI Hooked_ShowWindow(HWND hWnd, int nCmdShow)
{
    if (hWnd == browser::window
        && (nCmdShow == SW_SHOWNOACTIVATE || nCmdShow == SW_SHOW))
    {
        nCmdShow = SW_SHOWNA;
    }

    return Old_ShowWindow(hWnd, nCmdShow);
}

static hook::Hook<decltype(&SetWindowPos)> Old_SetWindowPos;
static BOOL WINAPI Hooked_SetWindowPos(HWND hWnd,
    HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    if (hWnd == browser::window && hWndInsertAfter == HWND_TOPMOST)
    {
        return TRUE;
    }

    return Old_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

static WNDPROC Old_WndProc;
static LRESULT Hooked_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_WINDOWPOSCHANGING:
        {
            auto pos = reinterpret_cast<WINDOWPOS *>(lp);
            if (pos->hwndInsertAfter == HWND_TOPMOST)
            {
                pos->hwndInsertAfter = NULL;
                return TRUE;
            }
            break;
        }
    }

    return CallWindowProc(Old_WndProc, hwnd, msg, wp, lp);
}
#endif

///
/// The window stacks on Windows look like:
///   RCLIENT                               <- Riot Client Window
///     CefBrowserWindow                    <- default window by CreateBrowser()
///       Chrome_WidgetWin_0                <- Chromium browser window
///         Chrome_RenderWidgetHostHWND
///         Intermediate D3D Window         <- Direct3D window
/// 
/// To obtain transparency effect like Electron,
/// just move the Chrome_WidgetWin_0 out of CefBrowserWindow
/// and to be the top-level child of RCLIENT.
/// 
/// On MacOS, just do nothing,
/// then use NSVisualEffectView API to handle that.
/// 

void browser::setup_window(cef_browser_t *browser)
{
    if (browser::window) return;
    auto host = browser->get_host(browser);

#if OS_WIN
    // Get needed windows.
    HWND browserWin = host->get_window_handle(host);
    // Retrieve top-level window (RCLIENT).
    HWND rclient = browser::window = GetAncestor(browserWin, GA_ROOT);

    HWND widgetWin = FindWindowExA(browserWin, NULL, "Chrome_WidgetWin_0", NULL);
    //HWND widgetHost = FindWindowExA(widgetWin, NULL, "Chrome_RenderWidgetHostHWND", NULL);

    // Ensure transparency effect.
    //   hide Chrome_RenderWidgetHostHWND
    //ShowWindow(widgetHost, SW_HIDE);
    //   hide CefBrowserWindow
    ShowWindow(browserWin, SW_HIDE);
    //   bring Chrome_WidgetWin_0 to top-level children
    SetParent(widgetWin, rclient);
#elif OS_MAC
    browser::window = host->get_window_handle(host);
#endif

    window::set_theme(browser::window, true);
    window::enable_shadow(browser::window);

    if (config::options::silent_mode())
    {
#if OS_WIN
        // LCUX calls ShowWindow to show itself
        Old_ShowWindow.hook(&ShowWindow, Hooked_ShowWindow);
        // it calls ShowWindow to make annoying topmost
        Old_SetWindowPos.hook(&SetWindowPos, Hooked_SetWindowPos);

        // must hook the wndproc to prevent external topmost from LCU
        Old_WndProc = (WNDPROC)GetWindowLongPtr(browser::window, GWLP_WNDPROC);
        SetWindowLongPtr(browser::window, GWLP_WNDPROC, (LONG_PTR)Hooked_WndProc);
        
        // note that post-game will not show the client window
#endif
    }

    host->base.release(&host->base);
}