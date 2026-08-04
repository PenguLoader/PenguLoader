using System.Runtime.InteropServices;
using System.Security.Principal;
using Pengu;
using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;
using Pengu.Migration;
using Pengu.Pack;
using Pengu.State;
using Pengu.Windows.Browser;
using Pengu.Windows.Native;
using Pengu.Windows.Window;
using Vanara.PInvoke;
using static Vanara.PInvoke.User32;

namespace Pengu.Windows;

/// <summary>
/// Windows implementation of <see cref="IHost"/>. Driven by
/// <see cref="Pengu.AppHost.RunAsync"/> from <see cref="Program.Main"/>.
/// </summary>
internal sealed class WindowsHost : IHost
{
    public string DataRoot { get; }
    public string ExeDirectory { get; }

    /// <summary><see cref="IHost.CoreModulePath"/> — <c>core.dll</c> alongside
    /// the host exe. macOS counterpart is
    /// <c>Pengu.app/Contents/Resources/core.dylib</c>.</summary>
    public string CoreModulePath => Path.Combine(ExeDirectory, "core.dll");

    /// <summary>
    /// The borderless window currently hosting the hub UI, captured during
    /// <see cref="OpenMainWindowAsync"/>. <see cref="MinimizeMainWindow"/> /
    /// <see cref="CloseMainWindow"/> / <see cref="StartDragging"/> act on
    /// this window. Null until the first window opens.
    /// </summary>
    private BorderlessWindow? _mainWindow;

    /// <summary>
    /// Packed asset reader for <c>app.dat</c>, opened lazily when the first
    /// window navigates to <c>app://hub/</c>. Null in dev mode (DevUrl set)
    /// since we never read from the pack there.
    /// </summary>
    private AppDat? _appDat;

    public WindowsHost()
    {
        ExeDirectory = AppContext.BaseDirectory;

        // %LOCALAPPDATA%\.pengu\ — everything Pengu owns for this user:
        // config, plugins, datastore, WebView2 cache, window placement.
        //
        // This used to be %PROGRAMDATA%\.pengu with an ACL granting
        // Authenticated Users: Modify, so that Universal mode (IFEO is
        // HKLM-scoped, therefore every account) saw one shared plugins
        // folder. That made the plugins directory writable by every user on
        // the machine — and plugins are executed by whoever launches the
        // client, so any account could run code in any other account's LCUX.
        // Per-user is both the safe layout and the one macOS already uses.
        DataRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            ".pengu");
        Directory.CreateDirectory(DataRoot);

        // One-shot: pull state out of the old machine-wide root. Runs before
        // AppHost's install-dir migration so the newer machine-wide state
        // wins over anything still sitting next to the exe.
        MigrateLegacyMachineRoot();
    }

    /// <summary>
    /// Move config / datastore / plugins out of the legacy machine-wide
    /// <c>%PROGRAMDATA%\.pengu\</c> into the per-user root, then drop the old
    /// directory if nothing else is left in it.
    ///
    /// <para>Best-effort and idempotent: <see cref="InstallMigrator"/> skips
    /// any item that already exists at the destination, so a user who has
    /// already migrated no-ops, and a second account on the same machine
    /// finds nothing left to take.</para>
    /// </summary>
    private void MigrateLegacyMachineRoot()
    {
        var legacy = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            ".pengu");

        if (string.Equals(legacy, DataRoot, StringComparison.OrdinalIgnoreCase))
            return;

        if (!Directory.Exists(legacy))
            return;

        InstallMigrator.Run(legacy, DataRoot);

        try
        {
            // Only if we emptied it — never recurse. Anything the user put
            // there themselves stays, and stays visible.
            if (!Directory.EnumerateFileSystemEntries(legacy).Any())
            {
                Directory.Delete(legacy);
                Log.Info("Removed empty legacy data root {0}", legacy);
            }
        }
        catch (Exception ex)
        {
            Log.Warn("Could not remove legacy data root {0}: {1}", legacy, ex.Message);
        }
    }

    public bool IsWebViewRuntimeAvailable() => WebView2Loader.IsRuntimeAvailable();

    public Task ShowMissingRuntimeDialogAsync()
    {
        // Real impl will use TaskDialogIndirect with a clickable hyperlink to
        // the MS installer per §5.3 of docs/app-hub.md. Skeleton uses MessageBox
        // as a stand-in until we wire ComCtl32 task dialog plumbing.
        const uint MB_OK = 0x0;
        const uint MB_ICONWARNING = 0x30;
        MessageBoxW(IntPtr.Zero,
            "WebView2 is not installed on your system.\n" +
            "Please install WebView2 from https://developer.microsoft.com/microsoft-edge/webview2/",
            AppEnv.AppName,
            MB_OK | MB_ICONWARNING);
        return Task.CompletedTask;
    }

    public Task InitializeBrowserEnvironmentAsync()
    {
        // WebView2 user-data folder (cookies, IndexedDB, GPU shader cache),
        // under %LOCALAPPDATA%\.pengu\WebView2\ alongside the rest of this
        // user's state.
        var userData = Path.Combine(DataRoot, "WebView2");
        return WebView2Environment.InitializeAsync(userData);
    }

    public async Task OpenMainWindowAsync(string url, IReadOnlyList<IJsInteropDispatcher> bridgeHandlers, EventBus bus)
    {
        // Restore prior placement (size + position + maximize) if we have
        // it. First-launch falls back to centered defaults inside the window.
        var savedState = WindowStateStore.TryLoad(WindowStatePath);
        var window = new BorderlessWindow(AppEnv.AppName, savedState);

        // Persist on close. Subscribed before InitializeBrowserAsync runs
        // so even an early WM_DESTROY catches the save.
        window.Closing += state =>
        {
            WindowStateStore.Save(WindowStatePath, state);
        };

        await window.InitializeBrowserAsync().ConfigureAwait(true);

        // The WebView2 controller inserts its own child HWND on top of the
        // parent on creation; bring the resize-frame back over the top so
        // its 8-px border catches resize-edge clicks before WebView2 does.
        window.EnableResizeFrame();

        var bridge = new JsBridge(window.Browser, bus);
        foreach (var h in bridgeHandlers)
            bridge.Register(h);
        bridge.InjectScript();

        // Packed mode: open app.dat once and wire the scheme handler before
        // the first navigation. Dev mode (--dev=<url>) skips this entirely
        // and goes straight to the Vite server.
        if (AppEnv.DevUrl is null)
        {
            try
            {
                _appDat = AppDat.OpenEmbedded(typeof(WindowsHost).Assembly);
                if (_appDat is not null)
                {
                    AppSchemeHandler.Attach(window.Browser, _appDat, WebView2Environment.Instance.Native);
                    Log.Info("app:// scheme handler attached (embedded resource)");
                }
                else
                {
                    Log.Warn("app.dat not embedded in assembly; running without hub bundle");
                }
            }
            catch (Exception ex)
            {
                Log.Error(ex, "Failed to open embedded app.dat; navigation to app:// will 404");
            }
        }

        window.Browser.ResizeToFill();
        window.Show();
        window.Browser.Navigate(url);

        _mainWindow = window;
        Log.Info("Main window shown ({0} handlers registered)", bridgeHandlers.Count);
    }

    private string WindowStatePath => Path.Combine(DataRoot, "window.json");

    // ---------- A.3 ----------

    public bool IsAdmin()
    {
        if (!OperatingSystem.IsWindows()) return false;
        try
        {
            using var identity = WindowsIdentity.GetCurrent();
            var principal = new WindowsPrincipal(identity);
            return principal.IsInRole(WindowsBuiltInRole.Administrator);
        }
        catch
        {
            return false;
        }
    }

    public void MinimizeMainWindow()
    {
        var hwnd = MainHandle;
        if (hwnd.IsNull) return;
        ShowWindow(hwnd, ShowWindowCommand.SW_MINIMIZE);
    }

    public void CloseMainWindow()
    {
        var hwnd = MainHandle;
        if (hwnd.IsNull) return;
        PostMessage(hwnd, (uint)WindowMessage.WM_CLOSE);
    }

    public void StartDragging()
    {
        // Mid-drag programmatic window-drag: the standard Win32 trick is to
        // release the current mouse capture and synthesize a non-client
        // left-button-down on the caption. The window then enters the
        // Win32 drag loop as if the user clicked the title bar.
        var hwnd = MainHandle;
        if (hwnd.IsNull) return;
        ReleaseCapture();
        SendMessage(hwnd, (uint)WindowMessage.WM_NCLBUTTONDOWN, (IntPtr)(int)HitTestValues.HTCAPTION, IntPtr.Zero);
    }

    public Task<string?> PickFolderAsync(string? initialPath)
    {
        // Bridge calls dispatch through the UI message loop, so we're already
        // on the UI thread. SHBrowseForFolderW pumps the same loop while
        // modal — calling it directly is correct and simplest.
        var owner = MainHandle.IsNull ? IntPtr.Zero : MainHandle.DangerousGetHandle();
        var picked = FolderPicker.Pick(owner, "Select a folder", initialPath);
        return Task.FromResult(picked);
    }

    public bool StartupIsEnabled() => StartupRegistry.IsEnabled();

    public void SetStartupEnabled(bool enabled)
    {
        var exe = Environment.ProcessPath ?? Path.Combine(ExeDirectory, "Pengu.exe");
        StartupRegistry.SetEnabled(enabled, exe);
    }

    public void RegisterActivationActions(ActivationActionRegistry registry, ConfigStore config, EventBus bus)
    {
        // Universal mode: IFEO Debugger value via cmd /c reg add (runas).
        // OnDemand is intentionally not registered on Windows — IFEO is
        // strictly more reliable here (kernel-side image-load redirect, no
        // daemon required, survives reboots). OnDemand stays a macOS-only
        // mode (see Pengu.MacOS in milestone E).
        registry.Register(new Pengu.Windows.Activation.IfeoAction(ExeDirectory, DataRoot));

        _ = config; _ = bus;
    }

    private HWND MainHandle => _mainWindow?.Handle ?? HWND.NULL;

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);
}
