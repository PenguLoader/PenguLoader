using Pengu.State;
using Pengu.Windows.Browser;
using Pengu.Windows.Native;
using Vanara.PInvoke;
using static Vanara.PInvoke.User32;

namespace Pengu.Windows.Window;

/// <summary>
/// Frameless Win32 window that hosts a single WebView2 filling the client
/// area. The SolidJS hub paints all visible chrome (titlebar buttons,
/// drag region) inside the page; this class only:
///
/// <list type="bullet">
///   <item><description>Removes the standard non-client area via <c>WM_NCCALCSIZE</c>.</description></item>
///   <item><description>Returns <c>HTCLIENT</c> for everything in <c>WM_NCHITTEST</c> (drag is via <c>app-region: drag</c> CSS).</description></item>
///   <item><description>Applies DWM dark titlebar / shadow / rounded corners.</description></item>
///   <item><description>Resizes the WebView2 + <see cref="ResizeFrame"/> on <c>WM_SIZE</c> / <c>WM_DPICHANGED</c>.</description></item>
///   <item><description>Handles <c>WM_SHOWME</c> from <see cref="SingleInstance"/> by restoring + foregrounding.</description></item>
///   <item><description>Tracks current placement and reports it on close via <see cref="Closing"/>.</description></item>
/// </list>
///
/// <para>The window is resizable: <c>WS_THICKFRAME</c> + <c>WS_MAXIMIZEBOX</c>
/// are kept. <see cref="ResizeFrame"/> sits on top of WebView2 with a
/// frame-shaped HRGN so only the outer ~8 px exposes resize handles;
/// interior clicks fall through to the page. Hub UI flows fluidly inside.</para>
/// </summary>
public sealed class BorderlessWindow : Win32Window
{
    private readonly Browser.Browser _browser;
    private ResizeFrame? _resizeFrame;
    public Browser.Browser Browser => _browser;

    /// <summary>Fires once during <c>WM_DESTROY</c> with the window's final
    /// placement so the host can persist it. Subscribers can read the
    /// fields freely; the window is being torn down.</summary>
    public event Action<WindowState>? Closing;

    /// <summary>Last-known position and size, updated on every WM_SIZE /
    /// WM_MOVE that isn't the maximized state. Saved values are what the
    /// window will restore to next launch even if the user closed while
    /// maximized.</summary>
    private int _restoreX, _restoreY, _restoreWidth, _restoreHeight;

    public BorderlessWindow(string title, WindowState? initial)
    {
        // WS_OVERLAPPEDWINDOW gives us the standard window-list-and-taskbar
        // hookup; WM_NCCALCSIZE then removes the visible non-client area.
        // Keep WS_THICKFRAME (resize edges) + WS_MAXIMIZEBOX (so the user
        // can maximize via Win+Up / drag-to-edge / Aero Snap). The actual
        // resize-handle hit-test happens in the ResizeFrame overlay.
        var style = WindowStyles.WS_OVERLAPPEDWINDOW
                  | WindowStyles.WS_CLIPCHILDREN;

        // Resolve initial placement: saved → clamp to a visible monitor;
        // first-run → centered defaults.
        var (x, y, w, h) = ResolveInitialPlacement(initial);
        _restoreX = x;
        _restoreY = y;
        _restoreWidth = w;
        _restoreHeight = h;

        Create(title, x, y, w, h, style);

        if (initial?.Maximized == true)
        {
            // Defer the maximize until after WM_CREATE / SetWindowPos
            // sequence settles. Posting here works because we're still in
            // the constructor before InitializeBrowserAsync runs.
            ShowWindow(Handle, ShowWindowCommand.SW_MAXIMIZE);
        }

        _browser = new Browser.Browser(Handle);
    }

    public Task InitializeBrowserAsync()
    {
        // Fully qualified: the Browser property name shadows the Browser
        // namespace in this file's scope.
        var env = Pengu.Windows.Browser.WebView2Environment.Instance.Native;
        return _browser.InitializeAsync(env);
    }

    /// <summary>Create the resize-frame overlay and z-order it above the
    /// WebView2 controller's child HWND. Call once after
    /// <see cref="InitializeBrowserAsync"/> completes; the controller
    /// inserts a child window of its own and we need to come back on top.</summary>
    public void EnableResizeFrame()
    {
        if (_resizeFrame is not null) return;
        _resizeFrame = new ResizeFrame(Handle);
    }

    /// <summary>Capture the current placement as a <see cref="WindowState"/>
    /// for persistence. Honours the "remember the unmaximized size when the
    /// user closes while maximized" UX — restore-fields are populated from
    /// the last non-zoomed WM_SIZE.</summary>
    public WindowState GetWindowState()
    {
        bool maximized = IsZoomed(Handle);
        return new WindowState(
            X: _restoreX,
            Y: _restoreY,
            Width: _restoreWidth,
            Height: _restoreHeight,
            Maximized: maximized);
    }

    protected override IntPtr WndProc(uint msg, IntPtr wParam, IntPtr lParam)
    {
        // Single-instance broadcast — focus this window if any other Pengu
        // instance posts WM_SHOWME.
        if (msg == SingleInstance.WM_SHOWME)
        {
            if (IsIconic(Handle))
                ShowWindow(Handle, ShowWindowCommand.SW_RESTORE);
            else
                ShowWindow(Handle, ShowWindowCommand.SW_SHOW);
            SetForegroundWindow(Handle);
            return IntPtr.Zero;
        }

        switch ((WindowMessage)msg)
        {
            case WindowMessage.WM_CREATE:
                Dwm.EnableDarkTitleBar(Handle);
                Dwm.EnableShadowFrameless(Handle);
                Dwm.RoundCorners(Handle);
                // Force WM_NCCALCSIZE to fire with the new frame so client area expands.
                SetWindowPos(Handle, HWND.NULL, 0, 0, 0, 0,
                    SetWindowPosFlags.SWP_FRAMECHANGED | SetWindowPosFlags.SWP_NOMOVE |
                    SetWindowPosFlags.SWP_NOSIZE | SetWindowPosFlags.SWP_NOZORDER |
                    SetWindowPosFlags.SWP_NOACTIVATE);
                return IntPtr.Zero;

            case WindowMessage.WM_NCCALCSIZE:
                if (wParam != IntPtr.Zero)
                {
                    // Default frameless behaviour: accept rgrc[0] as-is so the
                    // client rect spans the entire window. But when maximized,
                    // Windows positions the window at (-SM_CXFRAME, -SM_CYFRAME)
                    // with size = work-area + 2*frame on each axis, so the
                    // resize border tucks under the monitor edges. Without
                    // shrinking the client rect, the WebView2 controller fills
                    // that overflow region and its top-left ends up clipped
                    // off the visible monitor (the symptom: window looks
                    // maximized but the page's top corner is missing).
                    //
                    // Fix: when zoomed, inset rgrc[0] by frame+padded-border
                    // on each side so the client rect matches the visible
                    // monitor work area exactly. DPI-aware so mixed-DPI
                    // multi-monitor setups still get correct frame metrics.
                    if (IsZoomed(Handle))
                    {
                        unsafe
                        {
                            uint dpi = GetDpiForWindow(Handle);
                            if (dpi == 0) dpi = 96;
                            int frameX = GetSystemMetricsForDpi(SystemMetric.SM_CXFRAME, dpi)
                                       + GetSystemMetricsForDpi(SystemMetric.SM_CXPADDEDBORDER, dpi);
                            int frameY = GetSystemMetricsForDpi(SystemMetric.SM_CYFRAME, dpi)
                                       + GetSystemMetricsForDpi(SystemMetric.SM_CXPADDEDBORDER, dpi);
                            var rect = (RECT*)lParam;
                            rect->left   += frameX;
                            rect->right  -= frameX;
                            rect->top    += frameY;
                            rect->bottom -= frameY;
                        }
                    }
                    return IntPtr.Zero;
                }
                break;

            case WindowMessage.WM_NCHITTEST:
                // Everything is client area; drag is via app-region:drag CSS in
                // the page. Resize edges are handled by the ResizeFrame overlay
                // sitting on top — its WM_NCHITTEST returns the right HT* code
                // and forwards WM_NCLBUTTONDOWN here, which DefWindowProc then
                // turns into the standard resize loop.
                return (IntPtr)(int)HitTestValues.HTCLIENT;

            case WindowMessage.WM_SETFOCUS:
                // Parent HWND received keyboard focus — forward it into the
                // WebView2 child so the page becomes the actual focus target.
                //
                // We hook WM_SETFOCUS rather than WM_ACTIVATE: WM_ACTIVATE
                // fires *before* the window actually has keyboard focus, so
                // a MoveFocus() there sets WebView2 focus, then Windows
                // completes the activation by routing WM_SETFOCUS to the
                // parent — which steals focus back from the child and
                // produces a visible inactive→active→inactive flicker (the
                // hub watches `window.focus`/`blur` to grey the appbar).
                // WM_SETFOCUS is the post-activation signal: parent has the
                // focus *now*, redirecting to the child here is the final
                // state.
                //
                // Cases this handles vs. doesn't:
                //   - Taskbar / title-bar click  → parent gets WM_SETFOCUS;
                //     forward to child here.
                //   - Click directly in page     → child gets WM_SETFOCUS,
                //     parent doesn't, no-op (already focused). ✓
                //   - Alt-tab to a window where the child last held focus →
                //     Windows restores child focus directly, parent doesn't
                //     get WM_SETFOCUS, no-op. ✓
                _browser?.Focus();
                break;

            case WindowMessage.WM_SIZE:
                _browser?.ResizeToFill();
                _resizeFrame?.OnParentResized();
                CapturePlacement();
                break;

            case WindowMessage.WM_MOVE:
                CapturePlacement();
                break;

            case WindowMessage.WM_DPICHANGED:
                _browser?.ResizeToFill();
                _resizeFrame?.OnParentResized();
                break;

            case WindowMessage.WM_DESTROY:
                // Snapshot placement and notify before tearing down. The
                // browser close races aren't a problem since we already
                // captured the state we care about.
                try { Closing?.Invoke(GetWindowState()); }
                catch (Exception ex) { Pengu.Logging.Log.Error(ex, "BorderlessWindow Closing handler threw"); }

                _resizeFrame?.Dispose();
                _resizeFrame = null;
                _browser?.Close();
                Dispatcher.UIThread.Exit(0);
                return IntPtr.Zero;
        }

        return DefaultProc(msg, wParam, lParam);
    }

    /// <summary>Update the cached restore-position fields when the window
    /// moves or resizes in the *normal* (non-maximized) state. Skipping
    /// the update while zoomed means closing-while-maximized restores to
    /// the prior un-maximized size rather than full-screen-minus-1.</summary>
    private void CapturePlacement()
    {
        if (IsZoomed(Handle) || IsIconic(Handle)) return;
        if (!GetWindowRect(Handle, out var r)) return;
        _restoreX = r.left;
        _restoreY = r.top;
        _restoreWidth  = Math.Max(1, r.right  - r.left);
        _restoreHeight = Math.Max(1, r.bottom - r.top);
    }

    private static (int x, int y, int w, int h) ResolveInitialPlacement(WindowState? state)
    {
        const int defaultW = 1180;
        const int defaultH = 720;

        if (state is null || state.Width <= 0 || state.Height <= 0)
        {
            var (cx, cy) = CenterOnPrimary(defaultW, defaultH);
            return (cx, cy, defaultW, defaultH);
        }

        // Clamp to a sensible visible region. If the saved coords are off
        // every monitor (user disconnected the display they were on), recenter.
        if (!IsRectMostlyVisible(state.X, state.Y, state.Width, state.Height))
        {
            var (cx, cy) = CenterOnPrimary(state.Width, state.Height);
            return (cx, cy, state.Width, state.Height);
        }

        return (state.X, state.Y, state.Width, state.Height);
    }

    private static bool IsRectMostlyVisible(int x, int y, int w, int h)
    {
        unsafe
        {
            RECT work;
            if (!SystemParametersInfo(SPI.SPI_GETWORKAREA, 0, (IntPtr)(&work), 0))
                return true; // can't check; trust saved placement
            // Top-left within work area is the simplest proxy; cheap and
            // catches the "monitor went away" case. Aero Snap, multi-mon,
            // and full-monitor restores all stay valid.
            return x >= work.left - 8 && x <= work.right - 64
                && y >= work.top  - 8 && y <= work.bottom - 32;
        }
    }

    private static (int x, int y) CenterOnPrimary(int w, int h)
    {
        unsafe
        {
            RECT work;
            if (!SystemParametersInfo(SPI.SPI_GETWORKAREA, 0, (IntPtr)(&work), 0))
                return (unchecked((int)0x80000000), unchecked((int)0x80000000)); // CW_USEDEFAULT
            int areaW = work.right - work.left;
            int areaH = work.bottom - work.top;
            int x = work.left + Math.Max(0, (areaW - w) / 2);
            int y = work.top  + Math.Max(0, (areaH - h) / 2);
            return (x, y);
        }
    }
}
