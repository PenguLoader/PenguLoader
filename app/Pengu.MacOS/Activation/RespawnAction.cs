using System.Diagnostics;
using System.Runtime.InteropServices;
using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Logging;
using Pengu.MacOS.Native;

namespace Pengu.MacOS.Activation;

/// <summary>
/// macOS Universal-mode activation: kill-and-respawn LCUX with
/// <c>DYLD_INSERT_LIBRARIES=core.dylib</c>. See
/// <see cref="docs/macos-port.md"/> §3 for the design rationale.
///
/// <para>Flow when <see cref="LcuxWatcher"/> catches a new LCUX pid:
/// <list type="number">
///   <item><c>kill(pid, SIGSTOP)</c> immediately — pre-<c>cef_initialize</c>.</item>
///   <item>Re-check <c>proc_pidpath(pid)</c>. If it changed (a Helper child
///         briefly inherited LCUX's exec path during fork before its own
///         <c>execve</c>), <c>SIGCONT</c> and skip.</item>
///   <item><c>sysctl(KERN_PROCARGS2)</c> for the original argv + envp.</item>
///   <item>Parse <c>--install-directory=</c> from argv → working directory
///         for the new spawn (LCUX uses relative <c>Plugins/</c> paths and
///         crashes during <c>rcp-fe-plugin-runner</c> if cwd is wrong).</item>
///   <item><c>posix_spawn</c> a replacement LCUX with the same argv+envp +
///         added <c>DYLD_INSERT_LIBRARIES=&lt;coreDylibPath&gt;</c>.</item>
///   <item><b>Do NOT SIGKILL the original.</b> A stopped pid passes
///         <c>kill(pid, 0)</c> liveness, so LeagueClient's
///         <c>SIGCHLD</c>+<c>waitpid</c> child watch never fires and its
///         Foundation/LCDS server stays up. SIGKILL would tear that down
///         and our re-spawn would get <c>ERR_CONNECTION_REFUSED</c> on
///         <c>bootstrap.html</c>. The original sits frozen in early dyld
///         init forever (a few KB RSS) until logout.</item>
/// </list></para>
/// </summary>
public sealed class RespawnAction : IActivationAction
{
    private readonly string                         _coreDylibPath;
    private readonly EventBus                       _bus;
    private readonly LcuxWatcher                    _watcher;
    private readonly Action<string, string?>?       _onError;

    /// <summary>
    /// Construct the action. <paramref name="onError"/> is invoked from
    /// <see cref="SetActiveAsync"/> and the catch path when something the
    /// user should know about goes wrong (core.dylib missing, respawn
    /// failed). The host wires it to a native alert so failures aren't
    /// silent if the hub UI swallows the <see cref="ActivationResult"/>.
    /// </summary>
    public RespawnAction(string coreDylibPath, EventBus bus, Action<string, string?>? onError = null)
    {
        _coreDylibPath = coreDylibPath;
        _bus           = bus;
        _onError       = onError;
        _watcher       = new LcuxWatcher(Catch);
    }

    public ActivationMode Mode => ActivationMode.Universal;

    public Task<bool> IsActiveAsync(CancellationToken ct)
        => Task.FromResult(_watcher.IsRunning);

    public Task<ActivationResult> SetActiveAsync(bool active, CancellationToken ct)
    {
        if (active)
        {
            if (!File.Exists(_coreDylibPath))
            {
                Log.Warn("RespawnAction.SetActiveAsync: core.dylib not found at {0}", _coreDylibPath);
                _onError?.Invoke(
                    "Pengu can't activate",
                    $"core.dylib was not found at:\n{_coreDylibPath}\n\nBuild the core (make -C core) and re-launch Pengu.");
                return Task.FromResult(ActivationResult.Fail(
                    $"core.dylib not found at {_coreDylibPath}",
                    stage: "core-missing"));
            }
            _watcher.Start();
        }
        else
        {
            _watcher.Stop();
        }

        // Push state-change to the renderer so the hub UI updates without
        // needing to poll IsActiveAsync.
        _bus.Emit(
            "activation:stateChanged",
            $"{{\"active\":{(active ? "true" : "false")}}}");

        return Task.FromResult(ActivationResult.Success);
    }

    public Task OnSessionCreatedAsync(LcuxSession session, CancellationToken ct)
        => Task.CompletedTask; // not used in Universal mode (LcuxWatcher is the trigger)

    public Task OnSessionDeletedAsync(LcuxSession session, CancellationToken ct)
        => Task.CompletedTask;

    private void Catch(int pid, string path)
    {
        var t0 = DateTime.UtcNow;

        // 1. RACE-CRITICAL: SIGSTOP before LCUX reaches cef_initialize.
        if (Signals.kill(pid, Signals.SIGSTOP) != 0)
        {
            int errno = Marshal.GetLastSystemError();
            Log.Warn("RespawnAction: SIGSTOP failed pid={0} errno={1}", pid, errno);
            return;
        }

        // 2. Fork-window false-positive guard. proc_pidpath right after fork
        // (before execve) returns the parent's path; by the time we sysctl
        // here, exec has likely happened and the child became a Helper /
        // system tool. Skip those.
        var nowPath = LibProc.GetPath(pid);
        if (nowPath != path)
        {
            Log.Debug("RespawnAction: fork-window false-positive pid={0} path-now={1}",
                      pid, nowPath ?? "<dead>");
            Signals.kill(pid, Signals.SIGCONT);
            return;
        }

        try
        {
            // 3. Read original argv + envp.
            var snap = ProcArgs.Read(pid);

            // 4. Parse --install-directory= for cwd.
            string? installDir = null;
            foreach (var a in snap.Argv)
            {
                const string key = "--install-directory=";
                if (a.StartsWith(key, StringComparison.Ordinal))
                {
                    installDir = a[key.Length..];
                    break;
                }
            }

            // 5. Re-spawn with the same argv + envp + DYLD_INSERT_LIBRARIES.
            var psi = new ProcessStartInfo
            {
                FileName         = snap.ExePath,
                WorkingDirectory = installDir ?? Path.GetDirectoryName(snap.ExePath) ?? "",
                UseShellExecute  = false,
                CreateNoWindow   = false,
            };
            foreach (var a in snap.Argv.Skip(1))
                psi.ArgumentList.Add(a);

            psi.Environment.Clear();
            foreach (var e in snap.Envp)
            {
                int eq = e.IndexOf('=');
                if (eq > 0) psi.Environment[e[..eq]] = e[(eq + 1)..];
            }
            psi.Environment["DYLD_INSERT_LIBRARIES"] = _coreDylibPath;

            var newProc = Process.Start(psi)
                ?? throw new InvalidOperationException("Process.Start returned null");

            // Don't catch our own respawn next iteration.
            _watcher.AddSeen(newProc.Id);

            int elapsed = (int)(DateTime.UtcNow - t0).TotalMilliseconds;
            Log.Info("RespawnAction: respawned LCUX old={0} new={1} cwd={2} took={3}ms",
                     pid, newProc.Id, installDir ?? "(default)", elapsed);

            // 6. DO NOT SIGKILL the original — see class doc and macos-port.md §3.5.
        }
        catch (Exception ex)
        {
            Log.Error(ex, "RespawnAction failed for pid={0}", pid);
            // Best-effort: SIGCONT the original so it can run unmodified.
            // User sees LCUX without Pengu hooks; better than a stuck process.
            Signals.kill(pid, Signals.SIGCONT);
            _onError?.Invoke(
                "Pengu activation failed",
                $"Could not inject into LCUX (pid {pid}):\n{ex.Message}\n\nLeague will continue to run without Pengu hooks for this session.");
        }
    }
}
