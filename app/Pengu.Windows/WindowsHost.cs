using System.Runtime.InteropServices;
using System.Security.Principal;
using Pengu;
using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;
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

    /// <summary>Per-user state root: <c>%LOCALAPPDATA%\.pengu\</c>. Holds
    /// per-user concerns that don't belong in the machine-wide
    /// <see cref="DataRoot"/> — WebView2 cache, window placement, anything
    /// else that should follow the user (not the machine).</summary>
    public string UserDataRoot { get; }

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

        // %PROGRAMDATA%\.pengu\ — machine-wide so Universal mode (IFEO is
        // HKLM-scoped) sees consistent state across users. See
        // docs/app-hub.md §11.
        DataRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            ".pengu");

        // Create + grant Authenticated Users: Modify with inheritance so
        // any user on the machine can read/write Pengu's shared state.
        // First-launch wins (creator owns the dir and can set ACLs without
        // admin); subsequent launches verify and no-op.
        ProgramDataAcl.EnsureWritableByEveryone(DataRoot);

        // Per-user state lives separately under %LOCALAPPDATA%\.pengu\:
        // WebView2 cache (cookies / IndexedDB / GPU shader cache for THIS
        // user) plus window placement. No ACL gymnastics — each user's
        // LocalAppData is their own.
        UserDataRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            ".pengu");
        Directory.CreateDirectory(UserDataRoot);
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
        // WebView2 user-data folder is per-user (cookies, IndexedDB, GPU
        // shader cache for THIS user's session). Lives under
        // %LOCALAPPDATA%\.pengu\WebView2\ alongside other per-user state
        // (window placement, etc.) — separate from the machine-wide
        // DataRoot in %PROGRAMDATA%\.pengu\.
        var userData = Path.Combine(UserDataRoot, "WebView2");
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

    private string WindowStatePath => Path.Combine(UserDataRoot, "window.json");

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
        registry.Register(new Pengu.Windows.Activation.IfeoAction(ExeDirectory));

        _ = config; _ = bus;
    }

    private HWND MainHandle => _mainWindow?.Handle ?? HWND.NULL;

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);
}
