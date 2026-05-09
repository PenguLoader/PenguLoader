using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Config;

namespace Pengu;

/// <summary>
/// Platform abstraction surface implemented by each head (Pengu.Windows /
/// Pengu.MacOS). <see cref="AppHost"/> orchestrates startup against this
/// interface so the same control flow runs on both platforms.
///
/// <para>Synchronous members can run on the calling thread (typically the
/// UI thread); long-running operations should be async.</para>
/// </summary>
public interface IHost
{
    /// <summary>Path to the user data root, e.g. <c>%LOCALAPPDATA%\.pengu\</c>
    /// (Windows) or <c>~/Library/Application Support/Pengu/</c> (macOS).</summary>
    string DataRoot { get; }

    /// <summary>Path to the directory containing the running executable. Used
    /// for finding <c>app.dat</c>; the core module path is exposed separately
    /// via <see cref="CoreModulePath"/> because macOS bundles place it in
    /// <c>Contents/Resources/</c>, not next to the inner exec.</summary>
    string ExeDirectory { get; }

    /// <summary>Absolute path to the platform-specific core shared library
    /// (<c>core.dll</c> on Windows next to the host exe;
    /// <c>core.dylib</c> on macOS inside <c>Pengu.app/Contents/Resources/</c>).
    /// Used both for activation existence checks and as the
    /// <c>DYLD_INSERT_LIBRARIES</c> target on macOS Universal mode.</summary>
    string CoreModulePath { get; }

    /// <summary>Whether WebView2 (Win) / WKWebView (Mac) is available. If false,
    /// <see cref="ShowMissingRuntimeDialogAsync"/> must be callable to surface
    /// a user-facing message.</summary>
    bool IsWebViewRuntimeAvailable();

    /// <summary>Surface a TaskDialog (Win) / NSAlert (Mac) explaining the
    /// missing runtime. Caller exits the process after this returns.</summary>
    Task ShowMissingRuntimeDialogAsync();

    /// <summary>Initialize the WebView2 / WKWebView environment. Idempotent;
    /// must be called before opening any window.</summary>
    Task InitializeBrowserEnvironmentAsync();

    /// <summary>Open the main hub window pointed at <paramref name="url"/>.
    /// The window's <see cref="IBrowserHost"/> is used to wire up a
    /// <see cref="JsBridge"/>; bridge handlers in <paramref name="bridgeHandlers"/>
    /// are registered before the JS shim is injected. The bridge subscribes
    /// to <paramref name="bus"/> so C#-originated events fan out to the
    /// renderer as <c>CustomEvent</c>s.</summary>
    Task OpenMainWindowAsync(string url, IReadOnlyList<IJsInteropDispatcher> bridgeHandlers, EventBus bus);

    /// <summary>
    /// Register platform-specific <see cref="IActivationAction"/>
    /// implementations with <paramref name="registry"/>. Called once at
    /// startup, after <see cref="ConfigStore"/> has loaded but before the
    /// main window opens. Heads register their per-mode actions here
    /// (e.g. Pengu.Windows registers <c>IfeoAction</c> + <c>CopyDllAction</c>).
    /// </summary>
    void RegisterActivationActions(ActivationActionRegistry registry, ConfigStore config, EventBus bus);

    // ---------- A.3: HostApi-driven operations ----------

    /// <summary>Whether the calling user has admin / elevation. Used by
    /// <c>pengu.host.getInfo()</c>.</summary>
    bool IsAdmin();

    /// <summary>Minimize the main window. No-op if no window is open yet.</summary>
    void MinimizeMainWindow();

    /// <summary>Post a close message to the main window. No-op if no window
    /// is open yet. Mode-conditional close behavior (hide-to-tray vs exit)
    /// is the window's responsibility, not the host's.</summary>
    void CloseMainWindow();

    /// <summary>Begin a programmatic window drag (<c>ReleaseCapture</c> +
    /// <c>SendMessage(WM_NCLBUTTONDOWN, HTCAPTION, 0)</c> on Windows). Rare —
    /// <c>app-region: drag</c> CSS handles drag for most UI elements.</summary>
    void StartDragging();

    /// <summary>Show a native folder-picker dialog. Returns the chosen path
    /// or null if the user cancelled.</summary>
    /// <param name="initialPath">Initial directory; null to use the system
    /// default starting location.</param>
    Task<string?> PickFolderAsync(string? initialPath);

    /// <summary>Whether the loader is registered to launch on user login.
    /// HKCU\...\Run on Windows; LaunchAgent plist on macOS.</summary>
    bool StartupIsEnabled();

    /// <summary>Toggle launch-on-login.</summary>
    void SetStartupEnabled(bool enabled);
}
