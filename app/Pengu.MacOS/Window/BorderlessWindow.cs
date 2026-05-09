using AppKit;
using CoreGraphics;
using Foundation;
using Pengu.Logging;
using Pengu.State;

namespace Pengu.MacOS.Window;

/// <summary>
/// Frameless NSWindow that the SolidJS hub paints inside. No native titlebar
/// — the hub renders its own. Close button hides the window instead of
/// closing it (the daemon must outlive the window so LcuxWatcher / RcsDaemon
/// stay armed for the next LoL launch). Cmd-Q still terminates normally.
/// </summary>
internal sealed class BorderlessWindow : NSWindow
{
    private const float DefaultWidth  = 1000f;
    private const float DefaultHeight = 700f;

    /// <summary>
    /// Construct the borderless window. <paramref name="onWillClose"/> is
    /// invoked from the window's NSWindowDelegate.WillClose right before the
    /// window is dismissed — same path whether triggered by hub close button,
    /// Cmd-W, or a programmatic <c>Close()</c>. The host uses this hook to
    /// detach the shared WKWebView (so it survives across window cycles),
    /// persist <see cref="WindowState"/>, and null its reference to the window.
    /// <paramref name="initial"/> restores the placement saved at the previous
    /// close; null centers a default-sized window on the main screen.
    /// </summary>
    public BorderlessWindow(WindowState? initial = null, Action? onWillClose = null)
        : base(
            ResolveInitialFrame(initial),
            // .Titled is required so the native traffic-light buttons exist
            // in the accessibility tree (custom taskbars, VoiceOver, scripts
            // call AXPress on the close button to close windows). With
            // .FullSizeContentView + transparent titlebar the hub UI extends
            // under the titlebar normally — the visible traffic lights sit
            // on top of the hub's content (standard macOS hub pattern, same
            // as Tauri's TitleBarStyle::Overlay).
            NSWindowStyle.Titled
                | NSWindowStyle.Resizable
                | NSWindowStyle.Closable
                | NSWindowStyle.Miniaturizable
                | NSWindowStyle.FullSizeContentView,
            NSBackingStore.Buffered,
            deferCreation: false)
    {
        // Hub renders its own titlebar; hide the native one but keep the
        // traffic-light buttons (close/min/zoom) visible at their default
        // location — the hub layout reserves space for them.
        TitlebarAppearsTransparent = true;
        TitleVisibility = NSWindowTitleVisibility.Hidden;
        Title = "Pengu";

        // Transparent compositor so the hub's CSS background paints through;
        // also lets NSVisualEffectView (Phase D bridge: window.Effect.apply)
        // be installed beneath the WKWebView for vibrancy/blur.
        BackgroundColor = NSColor.Clear;
        IsOpaque = false;
        HasShadow = true;

        // Allow click-anywhere-and-drag for the body of the frameless window
        // when no element claims the click. Plays nice with `app-region: drag`
        // CSS in the hub for the explicit dragbar regions.
        MovableByWindowBackground = true;

        // Keep the NSObject alive across hide/show. Default would dealloc on
        // close (orderOut) and re-show would crash.
        ReleaseWhenClosed(false);

        if (initial is null || !IsOnAnyScreen(Frame))
            Center();
        if (initial?.Maximized == true && !IsZoomed)
            Zoom(this);

        // Hide the native traffic-light buttons — hub renders its own
        // titlebar UI and the visible traffic lights would overlay it.
        // Hidden=true keeps them in the accessibility tree, so external
        // agents (custom taskbars, VoiceOver) calling AXPress on the close
        // button still close the window via PerformClose. Same trick the
        // Tauri Rust loader uses (packages/hub/src-tauri/src/macos/utils.rs).
        var close    = StandardWindowButton(NSWindowButton.CloseButton);
        var minimize = StandardWindowButton(NSWindowButton.MiniaturizeButton);
        var zoom     = StandardWindowButton(NSWindowButton.ZoomButton);
        if (close    is not null) close.Hidden    = true;
        if (minimize is not null) minimize.Hidden = true;
        if (zoom     is not null) zoom.Hidden     = true;

        Delegate = new BorderlessWindowDelegate(onWillClose);
    }

    // NSWindow defaults canBecomeKeyWindow/Main to NO when the styleMask lacks
    // .Titled — which means MakeKeyAndOrderFront only orders front, never
    // makes us key. Without key status the WKWebView gets no mouse tracking
    // (hover doesn't fire) and the menu bar shows no active app. We dropped
    // .Titled deliberately so the hub paints its own titlebar; opt back in
    // here so the window is fully interactive.
    public override bool CanBecomeKeyWindow  => true;
    public override bool CanBecomeMainWindow => true;

    // PerformClose on a borderless NSWindow normally beeps and bails — it
    // requires a native close button to "perform" against, which we don't
    // have (no .Titled). External agents (custom taskbars, accessibility
    // tools) send performClose: to the front window expecting it to close.
    // Bypass the button check and run our normal close path: WindowShouldClose
    // → WillClose hook → window destroyed.
    public override void PerformClose(NSObject? sender)
    {
        if (Delegate is null || Delegate.WindowShouldClose(this))
            Close();
    }

    public void ShowAndFocus()
    {
        MakeKeyAndOrderFront(null);
        ActivateApp();
    }

    /// <summary>
    /// Activate the app. <c>NSApplication.Activate()</c> is macOS 14+; older
    /// systems use the deprecated <c>ActivateIgnoringOtherApps(true)</c>,
    /// which is the documented way until 14 lands and the only call shape
    /// that exists pre-14.
    /// </summary>
    internal static void ActivateApp()
    {
        if (OperatingSystem.IsMacOSVersionAtLeast(14, 0))
        {
            NSApplication.SharedApplication.Activate();
        }
        else
        {
#pragma warning disable CA1422 // deprecated-on-14 but the *only* form available on 12-13
            NSApplication.SharedApplication.ActivateIgnoringOtherApps(true);
#pragma warning restore CA1422
        }
    }

    /// <summary>
    /// Capture current placement to be persisted on close. AppKit's window
    /// frame uses bottom-up Y origin; we round to int and store as-is — values
    /// re-fed to <see cref="ResolveInitialFrame"/> on the next launch land
    /// in the same place because both ends are AppKit-native.
    /// </summary>
    public WindowState GetWindowState()
    {
        var f = Frame;
        return new WindowState(
            X:         (int)Math.Round(f.X),
            Y:         (int)Math.Round(f.Y),
            Width:     (int)Math.Round(f.Width),
            Height:    (int)Math.Round(f.Height),
            Maximized: IsZoomed);
    }

    private static CGRect ResolveInitialFrame(WindowState? state)
    {
        if (state is null) return new CGRect(0, 0, DefaultWidth, DefaultHeight);
        // Sanity check on stored size — refuse degenerate/absurd values.
        var w = Math.Clamp(state.Width,  400, 8000);
        var h = Math.Clamp(state.Height, 300, 8000);
        return new CGRect(state.X, state.Y, w, h);
    }

    private static bool IsOnAnyScreen(CGRect frame)
    {
        // Center-of-frame test is robust against single-pixel edge cases and
        // avoids needing rect-intersection helpers — if the user dragged the
        // window mostly off-screen, we still consider it "on" if the center
        // is on a screen and let macOS clamp.
        var center = new CGPoint(frame.X + frame.Width / 2.0, frame.Y + frame.Height / 2.0);
        foreach (var screen in NSScreen.Screens)
        {
            if (screen.Frame.Contains(center)) return true;
        }
        return false;
    }

    private sealed class BorderlessWindowDelegate : NSWindowDelegate
    {
        private readonly Action? _onWillClose;

        public BorderlessWindowDelegate(Action? onWillClose)
        {
            _onWillClose = onWillClose;
        }

        public override void WillClose(NSNotification notification)
        {
            // Hub close button, Cmd-W, and programmatic Close() all funnel
            // here. The host hook detaches the WKWebView so it survives the
            // close (we re-attach to a fresh window on next "Open hub") and
            // nulls its window reference. Daemon survives because of
            // ApplicationShouldTerminateAfterLastWindowClosed = false in
            // AppDelegate.
            try { _onWillClose?.Invoke(); }
            catch (Exception ex) { Log.Error(ex, "BorderlessWindow WillClose hook threw"); }
        }
    }
}
