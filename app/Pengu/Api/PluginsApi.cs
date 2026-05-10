using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;
using Pengu.Native;
using Pengu.Plugins;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
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

    [JsInvokable]
    public async Task<StoreInstallResult> InstallManifest(ManifestInstallRequest request)
    {
        var repo = ParseGithubRepo(request.Repo);
        if (repo is null)
            return StoreInstallResult.Fail("Enter a GitHub repo URL or owner/repo.");

        var manifestFetch = await FetchPenguManifest(repo.Value).ConfigureAwait(false);
        if (manifestFetch is null)
            return StoreInstallResult.Fail("Could not find pengu.yml in that GitHub repo.");

        var manifest = ParsePenguManifest(manifestFetch.Value.Content, manifestFetch.Value.Url);
        if (manifest is null)
            return StoreInstallResult.Fail("pengu.yml is missing required id or install.index fields.");

        var folderName = ResolveStoreFolderName(null, manifest.Id);
        if (folderName is null)
            return StoreInstallResult.Fail("Manifest id is not a safe plugin folder name.");

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

        try
        {
            Directory.CreateDirectory(pluginsDir);
            if (File.Exists(targetDir))
                File.Delete(targetDir);
            if (Directory.Exists(targetDir))
                Directory.Delete(targetDir, recursive: true);

            Directory.CreateDirectory(targetDir);
            await File.WriteAllTextAsync(installedIndex, EnsureTrailingNewline(manifest.Index), Encoding.UTF8).ConfigureAwait(false);
            await File.WriteAllTextAsync(
                System.IO.Path.Combine(targetDir, "pengu.plugin.json"),
                BuildManifestMetadataJson(manifest, manifestFetch.Value.Content),
                Encoding.UTF8).ConfigureAwait(false);

            return new StoreInstallResult(
                true,
                installedIndex,
                folderName,
                true);
        }
        catch (Exception ex)
        {
            Log.Warn("Manifest install failed for {0}: {1}", request.Repo, ex.Message);
            return StoreInstallResult.Fail(ex.Message, folderName);
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

    private static GithubRepo? ParseGithubRepo(string input)
    {
        input = input.Trim();
        if (Regex.IsMatch(input, @"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$"))
        {
            var parts = input.Split('/');
            return new GithubRepo(parts[0], parts[1].Replace(".git", "", StringComparison.OrdinalIgnoreCase), null, null);
        }

        if (!Uri.TryCreate(input, UriKind.Absolute, out var uri))
            return null;
        if (!uri.Host.Equals("github.com", StringComparison.OrdinalIgnoreCase) && !uri.Host.Equals("www.github.com", StringComparison.OrdinalIgnoreCase))
            return null;

        var segments = uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length < 2)
            return null;

        string? branch = null;
        string? manifestUrl = null;
        if (segments.Length >= 4 && segments[2].Equals("tree", StringComparison.OrdinalIgnoreCase))
            branch = string.Join('/', segments.Skip(3));
        else if (segments.Length >= 5 && segments[2].Equals("blob", StringComparison.OrdinalIgnoreCase))
        {
            branch = segments[3];
            var path = string.Join('/', segments.Skip(4));
            if (path.Equals("pengu.yml", StringComparison.OrdinalIgnoreCase))
                manifestUrl = $"https://raw.githubusercontent.com/{segments[0]}/{segments[1].Replace(".git", "", StringComparison.OrdinalIgnoreCase)}/{branch}/pengu.yml";
        }

        return new GithubRepo(segments[0], segments[1].Replace(".git", "", StringComparison.OrdinalIgnoreCase), branch, manifestUrl);
    }

    private static async Task<ManifestFetch?> FetchPenguManifest(GithubRepo repo)
    {
        var urls = new List<string>();
        if (!string.IsNullOrWhiteSpace(repo.ManifestUrl))
            urls.Add(repo.ManifestUrl);

        var branches = string.IsNullOrWhiteSpace(repo.Branch)
            ? new[] { "main", "master" }
            : new[] { repo.Branch };

        foreach (var branch in branches)
            urls.Add($"https://raw.githubusercontent.com/{repo.Owner}/{repo.Name}/{branch}/pengu.yml");

        foreach (var url in urls.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            try
            {
                StoreInstallHttp.DefaultRequestHeaders.UserAgent.Clear();
                StoreInstallHttp.DefaultRequestHeaders.UserAgent.ParseAdd($"Pengu/{AppEnv.AppVersion}");
                var response = await StoreInstallHttp.GetAsync(url).ConfigureAwait(false);
                if (!response.IsSuccessStatusCode)
                {
                    Log.Debug("Manifest fetch skipped {0}: HTTP {1}", url, (int)response.StatusCode);
                    continue;
                }

                return new ManifestFetch(url, await response.Content.ReadAsStringAsync().ConfigureAwait(false));
            }
            catch (Exception ex)
            {
                Log.Debug("Manifest fetch failed {0}: {1}", url, ex.Message);
                continue;
            }
        }

        return null;
    }

    private static PenguManifest? ParsePenguManifest(string yaml, string manifestUrl)
    {
        var id = ReadScalar(yaml, "id");
        var name = ReadScalar(yaml, "name");
        var description = ReadScalar(yaml, "description");
        var repo = ReadScalar(yaml, "repo");
        var discord = ReadScalar(yaml, "discord");
        var autoUpdate = ReadScalar(yaml, "auto_update")?.Equals("true", StringComparison.OrdinalIgnoreCase) == true;
        var authorName = ReadNestedScalar(yaml, "author", "name");
        var authorGithub = ReadNestedScalar(yaml, "author", "github");
        var index = ReadInstallIndex(yaml);

        if (string.IsNullOrWhiteSpace(id) || string.IsNullOrWhiteSpace(index))
            return null;

        return new PenguManifest(
            id.Trim(),
            string.IsNullOrWhiteSpace(name) ? id.Trim() : name.Trim(),
            description?.Trim(),
            repo?.Trim(),
            discord?.Trim(),
            autoUpdate,
            authorName?.Trim(),
            authorGithub?.Trim(),
            manifestUrl,
            index.TrimEnd());
    }

    private static string? ReadScalar(string yaml, string key)
    {
        var match = Regex.Match(yaml, $"(?m)^{Regex.Escape(key)}:\\s*(.+?)\\s*$");
        if (!match.Success)
            return null;
        return Unquote(match.Groups[1].Value.Trim());
    }

    private static string? ReadNestedScalar(string yaml, string parent, string key)
    {
        var match = Regex.Match(yaml, $"(?ms)^{Regex.Escape(parent)}:\\s*\\r?\\n(?<body>(?:\\s+[^\\r\\n]+\\r?\\n?)*)");
        if (!match.Success)
            return null;

        var nested = Regex.Match(match.Groups["body"].Value, $"(?m)^\\s+{Regex.Escape(key)}:\\s*(.+?)\\s*$");
        return nested.Success ? Unquote(nested.Groups[1].Value.Trim()) : null;
    }

    private static string? ReadInstallIndex(string yaml)
    {
        var match = Regex.Match(yaml, "(?ms)^install:\\s*\\r?\\n(?<body>(?:\\s+[^\\r\\n]*\\r?\\n?)*)");
        if (!match.Success)
            return null;

        var lines = match.Groups["body"].Value.Replace("\r\n", "\n").Split('\n');
        var indexLine = Array.FindIndex(lines, line => Regex.IsMatch(line, "^\\s+index:\\s*\\|\\s*$"));
        if (indexLine < 0)
            return null;

        var indexIndent = lines[indexLine].Length - lines[indexLine].TrimStart().Length;
        var blockIndent = indexIndent + 2;
        var block = new List<string>();
        for (var i = indexLine + 1; i < lines.Length; i++)
        {
            var line = lines[i];
            if (string.IsNullOrWhiteSpace(line))
            {
                block.Add("");
                continue;
            }

            var leading = line.Length - line.TrimStart().Length;
            if (leading <= indexIndent)
                break;
            block.Add(line.Length >= blockIndent ? line[blockIndent..] : line.TrimStart());
        }

        return string.Join('\n', block).TrimEnd();
    }

    private static string Unquote(string value)
    {
        if ((value.StartsWith('"') && value.EndsWith('"')) || (value.StartsWith('\'') && value.EndsWith('\'')))
            return value[1..^1];
        return value;
    }

    private static string EnsureTrailingNewline(string value)
        => value.EndsWith('\n') ? value : value + "\n";

    private static string BuildManifestMetadataJson(PenguManifest manifest, string manifestContent)
    {
        var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(manifestContent))).ToLowerInvariant();
        return $$"""
        {
          "source": "github-manifest",
          "id": {{JsonSerializer.Serialize(manifest.Id, PenguJsonContext.Default.String)}},
          "name": {{JsonSerializer.Serialize(manifest.Name, PenguJsonContext.Default.String)}},
          "description": {{JsonSerializer.Serialize(manifest.Description, PenguJsonContext.Default.String)}},
          "repo": {{JsonSerializer.Serialize(manifest.Repo, PenguJsonContext.Default.String)}},
          "discord": {{JsonSerializer.Serialize(manifest.Discord, PenguJsonContext.Default.String)}},
          "author": {
            "name": {{JsonSerializer.Serialize(manifest.AuthorName, PenguJsonContext.Default.String)}},
            "github": {{JsonSerializer.Serialize(manifest.AuthorGithub, PenguJsonContext.Default.String)}}
          },
          "autoUpdate": {{manifest.AutoUpdate.ToString().ToLowerInvariant()}},
          "manifestUrl": {{JsonSerializer.Serialize(manifest.ManifestUrl, PenguJsonContext.Default.String)}},
          "manifestHash": {{JsonSerializer.Serialize(hash, PenguJsonContext.Default.String)}}
        }
        """;
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

    private readonly record struct GithubRepo(string Owner, string Name, string? Branch, string? ManifestUrl);

    private readonly record struct ManifestFetch(string Url, string Content);

    private sealed record PenguManifest(
        string Id,
        string Name,
        string? Description,
        string? Repo,
        string? Discord,
        bool AutoUpdate,
        string? AuthorName,
        string? AuthorGithub,
        string ManifestUrl,
        string Index);
}

public sealed record StoreInstallCheckRequest(string ListingName, string? Repo);

public sealed record ManifestInstallRequest(string Repo, bool Replace);

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
