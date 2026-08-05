using Pengu.Activation;
using Pengu.Logging;

namespace Pengu.Config;

/// <summary>
/// Owns the on-disk <c>config</c> file at <c>&lt;data_root&gt;/config</c>.
/// Holds a lossless <see cref="IniMap"/> in memory and projects it into the
/// strongly-typed <see cref="ConfigSnapshot"/> for the bridge surface.
///
/// <para>Round-trip preservation: keys the host doesn't model in
/// <see cref="ConfigSnapshot"/> (e.g. the C++ core's undocumented
/// <c>debug_port</c>) ride along in the <see cref="IniMap"/> and survive
/// reads + writes intact.</para>
///
/// <para>Atomic write: write to <c>config.tmp</c>, fsync, rename. Eliminates
/// the half-flushed-config window the original Tauri <c>writeTextFile</c>
/// path was exposed to.</para>
/// </summary>
public sealed class ConfigStore
{
    private readonly object _lock = new();
    private readonly string _path;
    private IniMap _map = new();
    private bool _loaded;

    public string Path => _path;
    public string Directory { get; }

    public ConfigStore(string dataRoot)
    {
        Directory = dataRoot;
        _path = System.IO.Path.Combine(dataRoot, "config");
    }

    /// <summary>
    /// Load the config from disk if present, otherwise leave the store empty.
    /// Idempotent — subsequent calls re-read from disk (e.g. after the user
    /// edits the file by hand).
    /// </summary>
    public void Load()
    {
        lock (_lock)
        {
            if (File.Exists(_path))
            {
                try
                {
                    var content = File.ReadAllText(_path);
                    _map = IniReader.Parse(content);
                    Log.Debug("Config loaded from {0}", _path);
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Failed to read config at {0}; using empty map", _path);
                    _map = new IniMap();
                }
            }
            else
            {
                _map = new IniMap();
                Log.Debug("Config not present at {0}; defaults will be used", _path);
            }
            _loaded = true;
        }
    }

    /// <summary>
    /// Project the current ini map into a typed snapshot. Missing keys fall
    /// back to <see cref="ConfigDefaults.Snapshot"/>.
    /// </summary>
    public ConfigSnapshot Read()
    {
        lock (_lock)
        {
            if (!_loaded) Load();
            return Project(_map);
        }
    }

    /// <summary>
    /// Apply <paramref name="patch"/> over the current snapshot, write the
    /// resulting ini back to disk atomically. Unknown keys (anything outside
    /// the typed schema) are preserved.
    /// </summary>
    public void Write(ConfigSnapshot patch)
    {
        lock (_lock)
        {
            if (!_loaded) Load();

            // Apply to ini map. We only set keys that ConfigSnapshot models;
            // unmodeled keys in _map stay untouched.
            ApplyApp(_map, patch.App);
            ApplyClient(_map, patch.Client);

            FlushAtomic(_map, _path);
            Log.Debug("Config written to {0}", _path);
        }
    }

    // -------- projection (ini -> snapshot) --------

    private static ConfigSnapshot Project(IniMap map)
    {
        var d = ConfigDefaults.Snapshot;
        var app = new ConfigApp(
            Language: map.Get("app", "language") ?? d.App.Language,
            PluginsDir: map.Get("app", "plugins_dir") ?? d.App.PluginsDir,
            DisabledPlugins: map.Get("app", "disabled_plugins") ?? d.App.DisabledPlugins,
            ActivationMode: ParseMode(map.Get("app", "activation_mode"), d.App.ActivationMode),
            AutoUpdateCheck: IniReader.ParseBool(map.Get("app", "auto_update_check"), d.App.AutoUpdateCheck));

        var client = new ConfigClient(
            UseHotkeys: IniReader.ParseBool(map.Get("client", "use_hotkeys"), d.Client.UseHotkeys),
            OptimizedClient: IniReader.ParseBool(map.Get("client", "optimized_client"), d.Client.OptimizedClient),
            SilentMode: IniReader.ParseBool(map.Get("client", "silent_mode"), d.Client.SilentMode),
            SuperPotato: IniReader.ParseBool(map.Get("client", "super_potato"), d.Client.SuperPotato),
            InsecureMode: IniReader.ParseBool(map.Get("client", "insecure_mode"), d.Client.InsecureMode),
            UseDevtools: IniReader.ParseBool(map.Get("client", "use_devtools"), d.Client.UseDevtools),
            UseRiotclient: IniReader.ParseBool(map.Get("client", "use_riotclient"), d.Client.UseRiotclient),
            UseProxy: IniReader.ParseBool(map.Get("client", "use_proxy"), d.Client.UseProxy),
            UseTransparency: IniReader.ParseBool(map.Get("client", "use_transparency"), d.Client.UseTransparency),
            UseDecorations: IniReader.ParseBool(map.Get("client", "use_decorations"), d.Client.UseDecorations),
            UseLogging: IniReader.ParseBool(map.Get("client", "use_logging"), d.Client.UseLogging));

        return new ConfigSnapshot(app, client);
    }

    private static ActivationMode ParseMode(string? raw, ActivationMode def)
    {
        if (!int.TryParse(raw, out var n)) return def;
        // Symlink/Targeted mode (1) was dropped; fall back to default for old
        // configs that recorded it.
        if (n == (int)ActivationMode.Targeted) return def;
        return Enum.IsDefined(typeof(ActivationMode), n) ? (ActivationMode)n : def;
    }

    // -------- patch application (snapshot -> ini) --------

    private static void ApplyApp(IniMap map, ConfigApp app)
    {
        map.Set("app", "language", app.Language);
        map.Set("app", "plugins_dir", app.PluginsDir);
        map.Set("app", "disabled_plugins", app.DisabledPlugins);
        map.Set("app", "activation_mode", ((int)app.ActivationMode).ToString());
        map.Set("app", "auto_update_check", IniWriter.FormatBool(app.AutoUpdateCheck));
    }

    private static void ApplyClient(IniMap map, ConfigClient c)
    {
        map.Set("client", "use_hotkeys",      IniWriter.FormatBool(c.UseHotkeys));
        map.Set("client", "optimized_client", IniWriter.FormatBool(c.OptimizedClient));
        map.Set("client", "silent_mode",      IniWriter.FormatBool(c.SilentMode));
        map.Set("client", "super_potato",     IniWriter.FormatBool(c.SuperPotato));
        map.Set("client", "insecure_mode",    IniWriter.FormatBool(c.InsecureMode));
        map.Set("client", "use_devtools",     IniWriter.FormatBool(c.UseDevtools));
        map.Set("client", "use_riotclient",   IniWriter.FormatBool(c.UseRiotclient));
        map.Set("client", "use_proxy",        IniWriter.FormatBool(c.UseProxy));
        map.Set("client", "use_transparency", IniWriter.FormatBool(c.UseTransparency));
        map.Set("client", "use_decorations",  IniWriter.FormatBool(c.UseDecorations));
        map.Set("client", "use_logging",      IniWriter.FormatBool(c.UseLogging));
    }

    // -------- atomic flush --------

    private static void FlushAtomic(IniMap map, string path)
    {
        var dir = System.IO.Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir))
            System.IO.Directory.CreateDirectory(dir);

        var content = IniWriter.Write(map);
        var tmp = path + ".tmp";

        // Explicit Flush(true) to fsync the file before rename so a crash
        // between write and move can't leave a half-written file behind.
        using (var fs = new FileStream(tmp, FileMode.Create, FileAccess.Write, FileShare.None))
        using (var sw = new StreamWriter(fs, System.Text.Encoding.UTF8))
        {
            sw.Write(content);
            sw.Flush();
            fs.Flush(flushToDisk: true);
        }

        // File.Move with overwrite: true is atomic on the same volume on
        // Windows (MoveFileEx with MOVEFILE_REPLACE_EXISTING).
        File.Move(tmp, path, overwrite: true);
    }
}
