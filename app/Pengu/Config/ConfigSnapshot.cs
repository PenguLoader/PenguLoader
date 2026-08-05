using System.Text.Json.Serialization;
using Pengu.Activation;

namespace Pengu.Config;

/// <summary>
/// Strongly-typed view of the <c>config</c> file the hub reads and writes.
/// Mirrors the original <c>defaultConfig</c> shape from
/// <c>packages/hub/src/lib/config.ts</c> so the migration is mechanical;
/// property names use <c>[JsonPropertyName]</c> with snake_case to match
/// both the on-disk ini key names and the existing TS interface.
///
/// <para>Records (init-only properties) so partial merges via
/// <c>Pengu.Config.ConfigStore.Merge</c> can express "leave unset values
/// alone" using nullable fields without losing immutability semantics.</para>
/// </summary>
public sealed record ConfigSnapshot(
    [property: JsonPropertyName("app")]    ConfigApp App,
    [property: JsonPropertyName("client")] ConfigClient Client);

public sealed record ConfigApp(
    [property: JsonPropertyName("language")]          string Language,
    [property: JsonPropertyName("plugins_dir")]       string PluginsDir,
    [property: JsonPropertyName("disabled_plugins")]  string DisabledPlugins,
    [property: JsonPropertyName("activation_mode")]   ActivationMode ActivationMode,
    [property: JsonPropertyName("auto_update_check")] bool AutoUpdateCheck);

public sealed record ConfigClient(
    [property: JsonPropertyName("use_hotkeys")]      bool UseHotkeys,
    [property: JsonPropertyName("optimized_client")] bool OptimizedClient,
    [property: JsonPropertyName("silent_mode")]      bool SilentMode,
    [property: JsonPropertyName("super_potato")]     bool SuperPotato,
    [property: JsonPropertyName("insecure_mode")]    bool InsecureMode,
    [property: JsonPropertyName("use_devtools")]     bool UseDevtools,
    [property: JsonPropertyName("use_riotclient")]   bool UseRiotclient,
    [property: JsonPropertyName("use_proxy")]        bool UseProxy,
    [property: JsonPropertyName("use_transparency")] bool UseTransparency,
    [property: JsonPropertyName("use_decorations")]  bool UseDecorations,
    [property: JsonPropertyName("use_logging")]      bool UseLogging);

public static class ConfigDefaults
{
    public static ConfigSnapshot Snapshot { get; } = new(
        App: new ConfigApp(
            Language: "en",
            PluginsDir: string.Empty,
            DisabledPlugins: string.Empty,
            ActivationMode: ActivationMode.Universal,
            AutoUpdateCheck: true),
        Client: new ConfigClient(
            UseHotkeys: true,
            OptimizedClient: true,
            SilentMode: false,
            SuperPotato: false,
            InsecureMode: false,
            UseDevtools: false,
            UseRiotclient: false,
            UseProxy: false,
            UseTransparency: true,
            UseDecorations: true,
            UseLogging: true));
}
