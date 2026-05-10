using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;
using Pengu.Native;
using Pengu.Plugins;
using System.IO.Compression;
using System.Text.RegularExpressions;

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
    private static readonly HttpClient StoreInstallHttp = new()
    {
        Timeout = TimeSpan.FromSeconds(60),
    };

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

    [JsInvokable]
    public Task<StoreInstallResult> CheckStoreInstall(StoreInstallCheckRequest request)
    {
        var folderName = ResolveStoreFolderName(request.Repo, request.ListingName);
        if (folderName is null)
            return Task.FromResult(StoreInstallResult.Fail("Could not derive a safe plugin folder name."));

        var snapshot = _config.Read();
        var pluginsDir = ResolvePluginsDir(snapshot.App.PluginsDir);
        var targetDir = System.IO.Path.Combine(pluginsDir, folderName);
        var installed = File.Exists(System.IO.Path.Combine(targetDir, "index.js"));

        return Task.FromResult(new StoreInstallResult(
            true,
            installed ? System.IO.Path.Combine(targetDir, "index.js") : null,
            folderName,
            installed));
    }

    [JsInvokable]
    public async Task<StoreInstallResult> InstallStoreAsset(StoreInstallRequest request)
    {
        var folderName = ResolveStoreFolderName(request.Repo, request.ListingName);
        if (folderName is null)
            return StoreInstallResult.Fail("Could not derive a safe plugin folder name.");

        if (!TryGetSupportedAssetKind(request.AssetName, request.DownloadUrl, out var assetKind))
            return StoreInstallResult.Fail("Unsupported asset. Only .js and .zip releases can be installed.");

        if (!Uri.TryCreate(request.DownloadUrl, UriKind.Absolute, out var uri) || uri.Scheme != Uri.UriSchemeHttps)
            return StoreInstallResult.Fail("Unsupported download URL. Store installs require an HTTPS asset URL.");

        var snapshot = _config.Read();
        var pluginsDir = ResolvePluginsDir(snapshot.App.PluginsDir);
        var targetDir = System.IO.Path.Combine(pluginsDir, folderName);
        var installedIndex = System.IO.Path.Combine(targetDir, "index.js");
        var alreadyInstalled = File.Exists(installedIndex);

        if ((Directory.Exists(targetDir) || File.Exists(targetDir)) && !request.Replace)
        {
            return new StoreInstallResult(
                false,
                alreadyInstalled ? installedIndex : null,
                folderName,
                alreadyInstalled,
                true,
                $"Plugin folder '{folderName}' already exists.");
        }

        var tempRoot = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "pengu_store_install_" + Guid.NewGuid().ToString("N"));
        var downloadPath = System.IO.Path.Combine(tempRoot, "asset" + (assetKind == StoreAssetKind.Zip ? ".zip" : ".js"));
        var extractDir = System.IO.Path.Combine(tempRoot, "extract");
        var preparedDir = System.IO.Path.Combine(tempRoot, "prepared");

        try
        {
            Directory.CreateDirectory(tempRoot);
            Directory.CreateDirectory(preparedDir);

            StoreInstallHttp.DefaultRequestHeaders.UserAgent.Clear();
            StoreInstallHttp.DefaultRequestHeaders.UserAgent.ParseAdd($"Pengu/{AppEnv.AppVersion}");

            await using (var input = await StoreInstallHttp.GetStreamAsync(uri).ConfigureAwait(false))
            await using (var output = File.Create(downloadPath))
            {
                await input.CopyToAsync(output).ConfigureAwait(false);
            }

            if (assetKind == StoreAssetKind.Js)
            {
                File.Copy(downloadPath, System.IO.Path.Combine(preparedDir, "index.js"));
            }
            else
            {
                Directory.CreateDirectory(extractDir);
                ValidateZipEntries(downloadPath, extractDir);
                ZipFile.ExtractToDirectory(downloadPath, extractDir);

                var sourceRoot = ResolveZipPluginRoot(extractDir);
                if (sourceRoot is null)
                    return StoreInstallResult.Fail("Zip asset does not contain index.js at the root or inside one top-level folder.", folderName);

                CopyDirectory(sourceRoot, preparedDir);
            }

            if (!File.Exists(System.IO.Path.Combine(preparedDir, "index.js")))
                return StoreInstallResult.Fail("Installed asset did not produce a folder plugin with index.js.", folderName);

            Directory.CreateDirectory(pluginsDir);
            if (File.Exists(targetDir))
                File.Delete(targetDir);
            if (Directory.Exists(targetDir))
                Directory.Delete(targetDir, recursive: true);

            Directory.Move(preparedDir, targetDir);

            return new StoreInstallResult(
                true,
                installedIndex,
                folderName,
                true);
        }
        catch (Exception ex)
        {
            Log.Warn("Store install failed for {0}: {1}", request.ListingName, ex.Message);
            return StoreInstallResult.Fail(ex.Message, folderName);
        }
        finally
        {
            try
            {
                if (Directory.Exists(tempRoot))
                    Directory.Delete(tempRoot, recursive: true);
            }
            catch (Exception ex)
            {
                Log.Debug("Store install temp cleanup failed: {0}", ex.Message);
            }
        }
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

    private static string? ResolveStoreFolderName(string? repo, string listingName)
    {
        var source = TryGetRepoName(repo) ?? listingName;
        var sanitized = Regex.Replace(source.Trim(), @"[^A-Za-z0-9._-]+", "-").Trim('-', '.', '_');
        if (string.IsNullOrWhiteSpace(sanitized) || sanitized is "." or "..")
            return null;

        var reserved = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
        };
        return reserved.Contains(sanitized) ? null : sanitized;
    }

    private static string? TryGetRepoName(string? repo)
    {
        if (string.IsNullOrWhiteSpace(repo) || !Uri.TryCreate(repo, UriKind.Absolute, out var uri))
            return null;

        var segments = uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length < 2)
            return null;

        var name = segments[^1];
        return name.EndsWith(".git", StringComparison.OrdinalIgnoreCase) ? name[..^4] : name;
    }

    private static bool TryGetSupportedAssetKind(string assetName, string downloadUrl, out StoreAssetKind kind)
    {
        var name = assetName;
        if (string.IsNullOrWhiteSpace(name) && Uri.TryCreate(downloadUrl, UriKind.Absolute, out var uri))
            name = System.IO.Path.GetFileName(uri.AbsolutePath);

        if (name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
        {
            kind = StoreAssetKind.Zip;
            return true;
        }

        if (name.EndsWith(".js", StringComparison.OrdinalIgnoreCase))
        {
            kind = StoreAssetKind.Js;
            return true;
        }

        kind = StoreAssetKind.Unsupported;
        return false;
    }

    private static void ValidateZipEntries(string zipPath, string extractDir)
    {
        var root = System.IO.Path.GetFullPath(extractDir) + System.IO.Path.DirectorySeparatorChar;
        using var archive = ZipFile.OpenRead(zipPath);
        foreach (var entry in archive.Entries)
        {
            var destination = System.IO.Path.GetFullPath(System.IO.Path.Combine(extractDir, entry.FullName));
            if (!destination.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Zip asset contains an unsafe path.");
        }
    }

    private static string? ResolveZipPluginRoot(string extractDir)
    {
        if (File.Exists(System.IO.Path.Combine(extractDir, "index.js")))
            return extractDir;

        var topLevelFiles = Directory.GetFiles(extractDir).Where(path => !IsIgnoredZipEntryName(System.IO.Path.GetFileName(path))).ToArray();
        var topLevelDirs = Directory.GetDirectories(extractDir).Where(path => !IsIgnoredZipEntryName(System.IO.Path.GetFileName(path))).ToArray();
        if (topLevelFiles.Length == 0 && topLevelDirs.Length == 1 && File.Exists(System.IO.Path.Combine(topLevelDirs[0], "index.js")))
            return topLevelDirs[0];

        return null;
    }

    private static bool IsIgnoredZipEntryName(string name)
    {
        return name.Equals("__MACOSX", StringComparison.OrdinalIgnoreCase) || name.Equals(".DS_Store", StringComparison.OrdinalIgnoreCase);
    }

    private static void CopyDirectory(string sourceDir, string targetDir)
    {
        Directory.CreateDirectory(targetDir);
        foreach (var directory in Directory.GetDirectories(sourceDir, "*", SearchOption.AllDirectories))
        {
            var relative = System.IO.Path.GetRelativePath(sourceDir, directory);
            Directory.CreateDirectory(System.IO.Path.Combine(targetDir, relative));
        }

        foreach (var file in Directory.GetFiles(sourceDir, "*", SearchOption.AllDirectories))
        {
            var relative = System.IO.Path.GetRelativePath(sourceDir, file);
            File.Copy(file, System.IO.Path.Combine(targetDir, relative), overwrite: true);
        }
    }

    private enum StoreAssetKind
    {
        Unsupported,
        Js,
        Zip,
    }
}

public sealed record StoreInstallCheckRequest(string ListingName, string? Repo);

public sealed record StoreInstallRequest(
    string ListingId,
    string ListingName,
    string Kind,
    string? Repo,
    string AssetName,
    string DownloadUrl,
    string? ContentType,
    bool Replace);

public sealed record StoreInstallResult(
    bool Ok,
    string? InstalledPath,
    string? FolderName,
    bool AlreadyInstalled,
    bool Conflict = false,
    string? Error = null)
{
    public static StoreInstallResult Fail(string error, string? folderName = null)
        => new(false, null, folderName, false, false, error);
}
