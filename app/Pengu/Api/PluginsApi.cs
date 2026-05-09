using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;
using Pengu.Native;
using Pengu.Plugins;

namespace Pengu.Api;

/// <summary>
/// Bridge surface for plugin discovery and disabled-state. Exposed as
/// <c>window.pengu.plugins</c>. Reads the typed snapshot from
/// <see cref="ConfigStore"/> for the plugins-dir / disabled-list, walks the
/// folder, and returns the flat list the hub renders.
///
/// <para>Plugin discovery + JSDoc tag parse runs entirely C#-side; the hub's
/// <c>lib/plugins.ts</c> is a thin passthrough.</para>
/// </summary>
[JsInterop("plugins")]
public partial class PluginsApi
{
    private readonly ConfigStore _config;
    private readonly PluginDiscovery _discovery;
    private readonly string _baseDir;

    public PluginsApi(ConfigStore config, string baseDir)
    {
        _config = config;
        _baseDir = baseDir;
        _discovery = new PluginDiscovery();
    }

    [JsInvokable]
    public Task<PluginInfo[]> List()
    {
        var snapshot = _config.Read();
        var pluginsDir = ResolvePluginsDir(snapshot.App.PluginsDir);
        var disabled = DisabledList.Parse(snapshot.App.DisabledPlugins);
        var list = _discovery.List(pluginsDir, disabled);
        return Task.FromResult(list.ToArray());
    }

    /// <summary>Toggle a plugin's enabled state, returning the new state.
    /// Resolves the path via discovery (so the hash matches the canonical
    /// form), updates the disabled csv, and flushes config.
    ///
    /// <para>v1.1.6 compat: if the on-disk file is the legacy
    /// <c>.js_</c> / <c>index.js_</c> rename-to-disable form, enabling renames
    /// it to the live form (and defensively clears the hash from the csv).
    /// Subsequent toggles on the same plugin run pure hash add/remove. We
    /// never produce a legacy <c>_</c> file here — disabling always uses the
    /// hash csv.</para></summary>
    [JsInvokable]
    public Task<bool> ToggleEnabled(string path)
    {
        var snapshot = _config.Read();
        var pluginsDir = ResolvePluginsDir(snapshot.App.PluginsDir);
        var disabled = DisabledList.Parse(snapshot.App.DisabledPlugins);

        // Path is the canonical form already (matches PluginInfo.Path), so we
        // hash it directly. lowercase + Fnv1a matches DisabledList semantics.
        var hash = Fnv1a.Hash(path.ToLowerInvariant());

        var (entryPath, isLegacyDisabled) = ResolveEntryPath(pluginsDir, path);

        bool enabledNow;
        if (isLegacyDisabled)
        {
            var live = entryPath[..^1]; // strip trailing _
            try
            {
                File.Move(entryPath, live);
            }
            catch (Exception ex)
            {
                Log.Warn("Toggle: failed to rename {0} -> {1}: {2}", entryPath, live, ex.Message);
                return Task.FromResult(false);
            }
            disabled.Remove(hash);
            enabledNow = true;
        }
        else if (disabled.Contains(hash))
        {
            disabled.Remove(hash);
            enabledNow = true;
        }
        else
        {
            disabled.Add(hash);
            enabledNow = false;
        }

        var patched = snapshot with
        {
            App = snapshot.App with { DisabledPlugins = DisabledList.Format(disabled) },
        };
        _config.Write(patched);

        Log.Debug("Toggled plugin {0} -> enabled={1}{2}",
                  path, enabledNow, isLegacyDisabled ? " (legacy rename)" : "");
        return Task.FromResult(enabledNow);
    }

    /// <summary>
    /// Resolve a canonical plugin path to its on-disk entry. Prefers the live
    /// <c>.js</c> / <c>index.js</c> form; falls back to the legacy
    /// <c>.js_</c> / <c>index.js_</c> if only that exists. Returns the
    /// best-guess live path with <c>isLegacy=false</c> if neither exists, so
    /// callers can fail loud rather than silently consume a no-op.
    /// </summary>
    private static (string entryPath, bool isLegacy) ResolveEntryPath(string pluginsDir, string canonicalPath)
    {
        var native = canonicalPath.Replace('/', System.IO.Path.DirectorySeparatorChar);
        var live = System.IO.Path.Combine(pluginsDir, native);
        if (File.Exists(live)) return (live, false);
        var legacy = live + "_";
        if (File.Exists(legacy)) return (legacy, true);
        return (live, false);
    }

    [JsInvokable]
    public Task OpenFolder()
    {
        var snapshot = _config.Read();
        var pluginsDir = ResolvePluginsDir(snapshot.App.PluginsDir);
        Shell.OpenFolder(pluginsDir);
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task RevealInFolder(string path)
    {
        var snapshot = _config.Read();
        var pluginsDir = ResolvePluginsDir(snapshot.App.PluginsDir);
        // path is canonical (forward-slashed, no leading separator); resolve
        // against pluginsDir and let Shell.RevealFile handle path normalisation.
        var full = System.IO.Path.Combine(pluginsDir, path.Replace('/', System.IO.Path.DirectorySeparatorChar));
        Shell.RevealFile(full);
        return Task.CompletedTask;
    }

    /// <summary>
    /// Fetch the upstream plugin store registry as raw YAML text. The hub
    /// parses it client-side (the bridge wire stays string so we don't drag
    /// a YAML library into the AOT host build). Failures bubble as bridge
    /// errors so the hub can render an error state without a separate
    /// "loaded but empty" code path.
    /// </summary>
    [JsInvokable]
    public async Task<string> FetchStoreRegistry()
    {
        const string url = "https://raw.githack.com/PenguLoader/plugin-store/main/registry/plugins.yml";
        using var http = new HttpClient
        {
            // Keep the timeout tight — the hub blocks on this for the
            // store tab and we'd rather show an error than a long spinner.
            Timeout = TimeSpan.FromSeconds(10),
        };
        http.DefaultRequestHeaders.UserAgent.ParseAdd($"Pengu/{AppEnv.AppVersion}");
        var resp = await http.GetAsync(url).ConfigureAwait(false);
        resp.EnsureSuccessStatusCode();
        return await resp.Content.ReadAsStringAsync().ConfigureAwait(false);
    }

    /// <summary>
    /// Resolve the on-disk plugins directory. Empty / dot-prefixed values in
    /// config mean "default": <c>&lt;DataRoot&gt;/plugins</c>.
    /// </summary>
    private string ResolvePluginsDir(string configValue)
    {
        if (string.IsNullOrWhiteSpace(configValue) || configValue.StartsWith('.'))
            return System.IO.Path.Combine(_baseDir, "plugins");
        return configValue;
    }
}
