using AppKit;
using Foundation;
using Pengu;
using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;
using Pengu.MacOS.Browser;
using Pengu.MacOS.Startup;
using Pengu.MacOS.Tray;
using Pengu.MacOS.Window;
using Pengu.Pack;
using Pengu.State;

namespace Pengu.MacOS;

/// <summary>
/// macOS implementation of <see cref="IHost"/>. Counterpart of
/// <c>Pengu.Windows.WindowsHost</c>.
///
/// <para>WebView runtime: WKWebView ships with macOS, no install check or
/// missing-runtime dialog needed. Browser environment is per-instance, so
/// <see cref="InitializeBrowserEnvironmentAsync"/> is a no-op (vs Windows
/// where WebView2Environment is process-wide and async).</para>
/// </summary>
public sealed partial class MacOSHost : IHost
{
    private BorderlessWindow? _mainWindow;
    private WkWebViewHost?    _browser;
    private AppDat?           _appDat;
    private StatusItem?       _statusItem;

    public MacOSHost()
    {
        ExeDirectory = AppContext.BaseDirectory;

        // ~/Library/Application Support/Pengu/ — per-user data root. Distinct
        // from Windows's machine-wide %PROGRAMDATA%\.pengu\ because macOS
        // activation (Universal mode = kill-and-respawn LCUX) is per-user;
        // see docs/app-hub.md §11.
        var appSupport = NSSearchPath.GetDirectories(
            NSSearchPathDirectory.ApplicationSupportDirectory,
            NSSearchPathDomain.User,
            expandTilde: true).FirstOrDefault()
            ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                            "Library", "Application Support");
        DataRoot = Path.Combine(appSupport, "Pengu");
        Directory.CreateDirectory(DataRoot);
    }

    public string DataRoot { get; }
    public string ExeDirectory { get; }

    public bool IsWebViewRuntimeAvailable() => true;

    public Task ShowMissingRuntimeDialogAsync() => Task.CompletedTask; // never reached

    public Task InitializeBrowserEnvironmentAsync()
    {
        // No process-wide environment: WKWebView is configured per-instance.
        return Task.CompletedTask;
    }

    private string? _navUrl;

    public Task OpenMainWindowAsync(
        string url,
        IReadOnlyList<IJsInteropDispatcher> bridgeHandlers,
        EventBus bus)
    {
        // Open app.dat from the embedded managed resource. Release builds
        // ship the hub bundle inside the Pengu binary itself (no file alongside
        // the exec or in Resources/); Debug builds skip the bundle entirely
        // and rely on --dev=URL.
        if (AppEnv.DevUrl is null)
        {
            try
            {
                _appDat = AppDat.OpenEmbedded(typeof(MacOSHost).Assembly);
                if (_appDat is null)
                    Log.Warn("app.dat not embedded in assembly; running without bundle. " +
                             "Either set --dev=<url> or rebuild Release.");
            }
            catch (Exception ex)
            {
                Log.Error(ex, "Failed to open embedded app.dat");
            }
        }

        // First-call setup: WkWebViewHost (which owns the WKWebView) is
        // created once and survives across window close/reopen cycles. The
        // JsBridge subscribes to the bus, so it stays alive forever via
        // bus.Subscribe -> Action -> JsBridge -> _browser; recreating it on
        // every reopen would leak subscribers.
        _browser = new WkWebViewHost(_appDat);
        var bridge = new JsBridge(_browser, bus);
        foreach (var h in bridgeHandlers)
            bridge.Register(h);
        bridge.InjectScript();

        _browser.Navigate(url);
        _navUrl = url;

        OpenWindow();
        return Task.CompletedTask;
    }

    private void OpenWindow()
    {
        if (_browser is null) return; // not initialized yet (shouldn't happen via AppHost flow)

        if (_mainWindow is { } existing)
        {
            existing.MakeKeyAndOrderFront(null);
            BorderlessWindow.ActivateApp();
            return;
        }

        // Restore the placement saved at the last close. First launch (no
        // window.json yet) gets a centered default-sized window.
        var initial = WindowStateStore.TryLoad(WindowStatePath);

        // Fresh window: re-attach the surviving WKWebView as its contentView.
        // The hook is invoked from BorderlessWindowDelegate.WillClose so a
        // close from any source (hub button, Cmd-W, programmatic) cleans up
        // the same way.
        var window = new BorderlessWindow(initial: initial, onWillClose: HandleWindowWillClose);
        window.ContentView = _browser.View;
        _mainWindow = window;
        window.ShowAndFocus();
    }

    private void HandleWindowWillClose()
    {
        // Persist current placement before tearing down.
        if (_mainWindow is { } window)
        {
            try { WindowStateStore.Save(WindowStatePath, window.GetWindowState()); }
            catch (Exception ex) { Log.Warn("Failed to save window state: {0}", ex.Message); }
        }

        // Detach the WKWebView so it survives this window's destruction —
        // we'll re-parent it onto the next freshly-created window.
        _browser?.View.RemoveFromSuperview();
        _mainWindow = null;
        Log.Info("Main window closed; daemon continues in tray");
    }

    private string WindowStatePath => Path.Combine(DataRoot, "window.json");

    public void RegisterActivationActions(ActivationActionRegistry registry, ConfigStore config, EventBus bus)
    {
        // Universal mode: kill-and-respawn LCUX with DYLD_INSERT_LIBRARIES.
        // OnDemand fallback (legacy libEGL patch) is intentionally not
        // registered — Universal is the only supported mode on macOS.
        // onError → native NSAlert so users see "core.dylib missing" /
        // "respawn failed" failures even if the hub UI swallows the
        // ActivationResult.
        registry.Register(new Pengu.MacOS.Activation.RespawnAction(
            CoreModulePath,
            bus,
            onError: Pengu.MacOS.Native.Alerts.ShowError));

        // Menubar status item — required on macOS so the daemon stays
        // accessible after the user closes the hub window.
        _statusItem = new StatusItem(this, registry, bus);

        _ = config;
    }

    /// <summary>
    /// Path to <c>core.dylib</c> shipped inside the .app bundle. Resolves to
    /// <c>Pengu.app/Contents/Resources/core.dylib</c> in both dev and release
    /// because <see cref="ExeDirectory"/> is <c>Contents/MonoBundle/</c>
    /// (where .NET stages assemblies in a macOS bundle).
    /// </summary>
    public string CoreModulePath => Path.GetFullPath(
        Path.Combine(ExeDirectory, "..", "Resources", "core.dylib"));

    public bool IsAdmin() => geteuid() == 0;

    public void MinimizeMainWindow() => _mainWindow?.Miniaturize(_mainWindow);

    public void CloseMainWindow()
    {
        // Tauri-style close: actually destroy the window. WillClose hook
        // runs HandleWindowWillClose which detaches WKWebView + nulls the
        // window reference. The daemon continues running; the tray "Open hub"
        // menu item (or Dock icon click) recreates a fresh window with the
        // same WKWebView re-parented.
        _mainWindow?.Close();
    }

    /// <summary>
    /// Open or re-front the main window. Called from the tray's "Open hub"
    /// menu item, from <c>AppDelegate.ApplicationShouldHandleReopen</c>
    /// (Dock-icon-click), and from a second-launch via SingleInstance.
    /// Creates a fresh <see cref="BorderlessWindow"/> if none exists, else
    /// brings the existing one to front.
    /// </summary>
    public void BringMainWindowToFront() => OpenWindow();

    public void StartDragging()
    {
        var window = _mainWindow;
        if (window is null)
        {
            Log.Debug("StartDragging: no main window");
            return;
        }

        // Bridge is async, so NSApplication.CurrentEvent is stale by the time
        // we run; synthesize a fresh NSEvent at the current cursor position.
        // performWindowDrag uses the event for tracking-start coordinates and
        // timestamp — a synthesized event works as long as it's recent.
        var screenLoc = NSEvent.CurrentMouseLocation;
        var winLoc    = window.ConvertPointFromScreen(screenLoc);
        var ev = NSEvent.MouseEvent(
            NSEventType.LeftMouseDown,
            winLoc,
            0,
            NSDate.Now.SecondsSinceReferenceDate,
            window.WindowNumber,
            null,
            0, 1, 0.0f);

        if (ev is null)
        {
            Log.Warn("StartDragging: NSEvent.MouseEvent returned null (winLoc={0})", winLoc);
            return;
        }

        Log.Debug("StartDragging: PerformWindowDrag at winLoc={0} (screen={1})", winLoc, screenLoc);
        window.PerformWindowDrag(ev);
    }

    public Task<string?> PickFolderAsync(string? initialPath)
    {
        // NSOpenPanel + RunModal are marked obsolete since 10.15 (Apple's
        // recommended path is the async sheet form), but the synchronous
        // modal still works through macOS 26 and is the simplest API for
        // our host's blocking-pick-folder semantics. Async sheet refactor
        // is future work if the modal-blocks-UI behavior becomes an issue.
#pragma warning disable CA1422 // NSOpenPanel deprecated 10.15+; still functional on 12-26.
        var panel = new NSOpenPanel
        {
            CanChooseDirectories    = true,
            CanChooseFiles          = false,
            AllowsMultipleSelection = false,
            ShowsHiddenFiles        = false,
        };
        if (!string.IsNullOrEmpty(initialPath))
        {
            var url = NSUrl.FromFilename(initialPath);
            if (url is not null) panel.DirectoryUrl = url;
        }

        var rc = panel.RunModal();
#pragma warning restore CA1422
        if (rc != 1 /* NSModalResponseOK */) return Task.FromResult<string?>(null);
        return Task.FromResult(panel.Urls.FirstOrDefault()?.Path);
    }

    public bool StartupIsEnabled() => LaunchAgent.IsEnabled();

    public void SetStartupEnabled(bool enabled)
    {
        if (enabled)
        {
            // The agent's `Program` key needs the absolute path to the
            // .app's inner binary. AppContext.BaseDirectory is
            // Contents/MonoBundle/, so the binary is ../MacOS/Pengu.
            var binary = Path.GetFullPath(Path.Combine(ExeDirectory, "..", "MacOS", "Pengu"));
            LaunchAgent.Enable(binary);
        }
        else
        {
            LaunchAgent.Disable();
        }
    }

    [System.Runtime.InteropServices.LibraryImport("/usr/lib/libSystem.dylib")]
    private static partial uint geteuid();
}
