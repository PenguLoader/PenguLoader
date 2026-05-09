using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;
using Pengu.Migration;

namespace Pengu;

/// <summary>
/// Platform-neutral startup orchestration. Heads call <see cref="RunAsync"/>
/// from their <c>Program.Main</c> after wiring an <see cref="IHost"/>.
///
/// <para>Responsibilities: WebView2/WKWebView runtime check, env init,
/// resolve the main window URL (dev server vs <c>app://hub/</c>), open the
/// window with the bridge handlers attached.</para>
/// </summary>
public static class AppHost
{
    public static async Task<int> RunAsync(IHost host)
    {
        Log.Info("{0} v{1} starting (pid={2}, dataRoot={3})",
            AppEnv.AppName, AppEnv.AppVersion, Environment.ProcessId, host.DataRoot);

        // First-launch migrator: move legacy <install>/{config,datastore,plugins/}
        // into <DataRoot>/. Idempotent — only acts when the source exists and
        // the destination doesn't. Must run before ConfigStore.Load() so the
        // first read picks up the migrated file.
        InstallMigrator.Run(host.ExeDirectory, host.DataRoot);

        if (!host.IsWebViewRuntimeAvailable())
        {
            Log.Error("WebView runtime not available; surfacing dialog and exiting");
            await host.ShowMissingRuntimeDialogAsync().ConfigureAwait(true);
            return 2;
        }

        try
        {
            await host.InitializeBrowserEnvironmentAsync().ConfigureAwait(true);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "WebView environment initialization failed");
            return 3;
        }

        // Resolve the URL we'll point the main window at.
        // Debug iteration: --dev=<vite-url> bypasses app.dat and goes straight to Vite.
        // Release: navigate to app://hub/ which the scheme handler resolves against app.dat.
        var url = AppEnv.DevUrl ?? "app://hub/";
        Log.Info("Main window navigating to {0}", url);

        // Process-wide config store. Owns the on-disk config file and serves
        // the typed snapshot to bridge callers (and any C# subsystem that
        // needs config state — activation mode, plugins dir, etc.).
        var configStore = new ConfigStore(host.DataRoot);
        configStore.Load();

        // Process-wide pub/sub for C#-originated events (activation:stateChanged
        // and friends). Bridge instances subscribe; daemon / actions publish.
        var bus = new EventBus();

        // Activation action registry. Heads (Pengu.Windows / Pengu.MacOS)
        // register their platform-specific implementations in
        // RegisterActivationActions; ActivationApi resolves through the
        // registry per current config.app.activation_mode.
        var registry = new ActivationActionRegistry();
        host.RegisterActivationActions(registry, configStore, bus);

        // Bridge surface registered with every window opened by the host.
        // PingApi is the diagnostic round-trip; A.1 is ConfigApi; A.2 is
        // PluginsApi; A.3 is HostApi/LeagueApi/I18nApi/FsApi/PathApi;
        // C.1 adds ActivationApi (real actions land in C.2/C.3).
        var handlers = new List<IJsInteropDispatcher>
        {
            new Api.PingApi(),
            new Api.ConfigApi(configStore),
            new Api.PluginsApi(configStore, host.DataRoot),
            new Api.HostApi(host),
            new Api.LeagueApi(),
            new Api.I18nApi(),
            new Api.FsApi(),
            new Api.PathApi(),
            new Api.ActivationApi(configStore, registry, bus, host.CoreModulePath),
        };

        try
        {
            await host.OpenMainWindowAsync(url, handlers, bus).ConfigureAwait(true);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to open main window");
            return 4;
        }

        Log.Info("Main window opened; entering message loop");
        return 0;
    }
}
