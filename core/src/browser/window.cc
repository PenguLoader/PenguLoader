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
/// Two independent switches gate the cosmetics here:
///   `use_transparency` - the re-parent above, the CefContext::GetBackgroundColor
///     patch in libcef.cc, and the `Effect.apply` vibrancy API. All three are one
///     mechanism: without the patch the surface is opaque, and without the
///     re-parent there is no top-level surface for a backdrop to show through.
///   `use_decorations` - `enable_shadow` only, and Windows-only. On the WS_POPUP
///     RCLIENT this turns on DWM non-client rendering, which brings both the drop
///     shadow and Win11's default corner rounding. macOS has no equivalent.
///
/// Of the transparency call sites, only the re-parent is Windows-specific -
/// the libcef.cc patch and the vibrancy API both apply on macOS too.
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

    if (config::options::use_transparency())
    {
        HWND widgetWin = FindWindowExA(browserWin, NULL, "Chrome_WidgetWin_0", NULL);
        //HWND widgetHost = FindWindowExA(widgetWin, NULL, "Chrome_RenderWidgetHostHWND", NULL);

        // Ensure transparency effect.
        //   hide Chrome_RenderWidgetHostHWND
        //ShowWindow(widgetHost, SW_HIDE);
        //   hide CefBrowserWindow
        ShowWindow(browserWin, SW_HIDE);
        //   bring Chrome_WidgetWin_0 to top-level children
        SetParent(widgetWin, rclient);
    }
#elif OS_MAC
    browser::window = host->get_window_handle(host);
#endif

    // Not gated - the dark frame attribute is an input to both features rather
    // than part of either, and it is what tints a Win11 backdrop material.
    window::set_theme(browser::window, true);

    // `use_decorations` is Windows-only. On macOS `enable_shadow` is just
    // [window invalidateShadow] - a refresh, with nothing to switch off - so
    // honouring the key there would only drop that refresh for no gain.
#if OS_WIN
    if (config::options::use_decorations())
        window::enable_shadow(browser::window);
#elif OS_MAC
    window::enable_shadow(browser::window);
#endif

    if (config::options::silent_mode())
    {
#if OS_WIN
        // LCUX calls ShowWindow to show itself
        Old_ShowWindow.hook(&ShowWindow, Hooked_ShowWindow);
        // it calls SetWindowPos to make annoying topmost
        Old_SetWindowPos.hook(&SetWindowPos, Hooked_SetWindowPos);

        // must hook the wndproc to prevent external topmost from LCU
        Old_WndProc = (WNDPROC)GetWindowLongPtr(browser::window, GWLP_WNDPROC);
        SetWindowLongPtr(browser::window, GWLP_WNDPROC, (LONG_PTR)Hooked_WndProc);
        
        // note that post-game will not show the client window
#endif
    }

    host->base.release(&host->base);
}