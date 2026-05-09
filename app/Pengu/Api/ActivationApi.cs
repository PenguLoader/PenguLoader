using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Config;
using Pengu.Logging;

namespace Pengu.Api;

/// <summary>
/// Bridge surface for activation. Exposed as <c>window.pengu.activation</c>.
/// Routes through <see cref="ActivationActionRegistry"/> to the per-mode
/// <see cref="IActivationAction"/>; emits <c>activation:stateChanged</c>
/// on the <see cref="EventBus"/> after every successful state change so
/// the hub UI stays in sync without polling.
///
/// <para>If no action is registered for the active mode (e.g. C.1 with no
/// platform impls yet, or someone selects an OS-incompatible mode in
/// config), bridge calls return a typed <see cref="ActivationResult"/>
/// failure with a clear message rather than throwing.</para>
/// </summary>
[JsInterop("activation")]
public partial class ActivationApi
{
    private readonly ConfigStore _config;
    private readonly ActivationActionRegistry _registry;
    private readonly EventBus _bus;
    private readonly string _corePath;

    public ActivationApi(ConfigStore config, ActivationActionRegistry registry, EventBus bus, string corePath)
    {
        _config = config;
        _registry = registry;
        _bus = bus;
        _corePath = corePath;
    }

    /// <summary>
    /// All known modes with availability + admin metadata. Used by the hub
    /// to render the radio group in Settings → Pengu, greying out modes
    /// the current platform / build can't satisfy.
    /// </summary>
    [JsInvokable]
    public Task<ActivationModeInfo[]> ListModes()
    {
        // Static topology: Universal needs admin, OnDemand doesn't.
        // Targeted is reserved (symlink mode dropped); never selectable.
        var infos = new[]
        {
            new ActivationModeInfo(ActivationMode.Universal, _registry.IsAvailable(ActivationMode.Universal), RequiresAdmin: true),
            new ActivationModeInfo(ActivationMode.OnDemand,  _registry.IsAvailable(ActivationMode.OnDemand),  RequiresAdmin: false),
        };
        return Task.FromResult(infos);
    }

    [JsInvokable]
    public Task<ActivationMode> GetMode()
    {
        var snapshot = _config.Read();
        return Task.FromResult(snapshot.App.ActivationMode);
    }

    [JsInvokable]
    public Task SetMode(ActivationMode mode)
    {
        var snapshot = _config.Read();
        if (snapshot.App.ActivationMode == mode) return Task.CompletedTask;

        var patched = snapshot with { App = snapshot.App with { ActivationMode = mode } };
        _config.Write(patched);
        Log.Info("Activation mode set to {0}", mode);
        return Task.CompletedTask;
    }

    [JsInvokable]
    public async Task<bool> IsActive()
    {
        var action = ResolveAction();
        if (action is null) return false;
        try
        {
            return await action.IsActiveAsync(default).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "ActivationAction.IsActiveAsync threw");
            return false;
        }
    }

    [JsInvokable]
    public async Task<ActivationResult> SetActive(bool active)
    {
        var action = ResolveAction();
        if (action is null)
        {
            var mode = _config.Read().App.ActivationMode;
            return ActivationResult.Fail(
                $"No activation action registered for mode '{mode}'. The current build may not support this mode on this OS.",
                stage: "ResolveAction");
        }

        ActivationResult result;
        try
        {
            result = await action.SetActiveAsync(active, default).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "ActivationAction.SetActiveAsync({0}) threw", active);
            return ActivationResult.Fail(ex.Message, stage: "Unhandled");
        }

        if (result.Ok)
        {
            // Refresh + announce. IsActiveAsync is the source of truth — the
            // user toggled to <active> but the action might have ended up
            // in a different state (e.g. UAC denied so nothing changed).
            bool nowActive;
            try { nowActive = await action.IsActiveAsync(default).ConfigureAwait(false); }
            catch { nowActive = active; }
            EmitStateChanged(nowActive);
        }
        return result;
    }

    [JsInvokable]
    public Task<bool> CoreExists() => Task.FromResult(File.Exists(_corePath));

    private IActivationAction? ResolveAction()
    {
        var mode = _config.Read().App.ActivationMode;
        return _registry.Get(mode);
    }

    private void EmitStateChanged(bool active)
    {
        var payload = active ? "{\"active\":true}" : "{\"active\":false}";
        _bus.Emit("activation:stateChanged", payload);
    }
}
