using System.Globalization;
using System.Text.Json;
using Pengu.Bridge;
using Pengu.DataStore;
using Pengu.Native;

namespace Pengu.Api;

/// <summary>
/// Application-level surface exposed as <c>window.pengu.host</c>. Most calls
/// delegate to <see cref="IHost"/> for platform-specific behavior; the
/// platform-agnostic operations (system info, openExternal, datastore read)
/// live here directly.
/// </summary>
[JsInterop("host")]
public partial class HostApi
{
    private readonly IHost _host;
    private readonly DataStoreReader _datastore;

    public HostApi(IHost host)
    {
        _host = host;
        _datastore = new DataStoreReader(host.DataRoot);
    }

    [JsInvokable]
    public Task<HostInfo> GetInfo()
    {
        var os = OperatingSystem.IsMacOS() ? "mac" : "win";
        var version = Environment.OSVersion.Version.ToString();
        var build = Environment.OSVersion.Version.Build.ToString(CultureInfo.InvariantCulture);
        var locale = CultureInfo.CurrentUICulture.Name;
        var info = new HostInfo(
            Os: os,
            Version: version,
            Build: build,
            IsMac: OperatingSystem.IsMacOS(),
            IsAdmin: _host.IsAdmin(),
            Locale: locale);
        return Task.FromResult(info);
    }

    [JsInvokable]
    public Task Minimize()
    {
        _host.MinimizeMainWindow();
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task Close()
    {
        _host.CloseMainWindow();
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task StartDragging()
    {
        _host.StartDragging();
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task OpenExternal(string url)
    {
        // Scheme enforcement lives in Shell.OpenExternal — the hub's
        // Shell.openLink check is a convenience, not a boundary.
        if (!string.IsNullOrEmpty(url))
            Shell.OpenExternal(url);
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task OpenFolder(string path)
    {
        if (!string.IsNullOrEmpty(path))
            Shell.OpenFolder(path);
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task RevealFile(string path)
    {
        if (!string.IsNullOrEmpty(path))
            Shell.RevealFile(path);
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task<string?> PickFolder(string? initial) => _host.PickFolderAsync(initial);

    [JsInvokable]
    public Task<bool> StartupGetEnabled() => Task.FromResult(_host.StartupIsEnabled());

    [JsInvokable]
    public Task StartupSetEnabled(bool enabled)
    {
        _host.SetStartupEnabled(enabled);
        return Task.CompletedTask;
    }

    [JsInvokable]
    public Task<JsonElement> ReadDataStore() => Task.FromResult(_datastore.Read());
}
