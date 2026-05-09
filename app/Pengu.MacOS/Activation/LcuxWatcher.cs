using Pengu.Logging;
using Pengu.MacOS.Native;

namespace Pengu.MacOS.Activation;

/// <summary>
/// Background poller that watches for <c>LeagueClientUx</c> spawning. Diff-based:
/// a 5 ms tick reads <c>proc_listpids</c>, compares to the seen-set, and
/// invokes <see cref="_onMatch"/> for each new pid whose path matches LCUX.
///
/// <para>Polling instead of <c>kqueue NOTE_TRACK</c> because RCS is hardened,
/// which causes <c>NOTE_TRACK</c> on RCS's children to fail with
/// <c>ENOTSUP</c> — see <see cref="docs/macos-port.md"/> §3.1. The 5 ms
/// cadence catches LCUX comfortably before <c>cef_initialize</c> (~50–200 ms
/// into LCUX startup).</para>
///
/// <para>Thread model: a single dedicated background thread runs the poll
/// loop. <see cref="AddSeen"/> is thread-safe via the same lock so
/// <see cref="RespawnAction"/> can register its respawned pid without
/// racing the watcher itself catching it.</para>
/// </summary>
internal sealed class LcuxWatcher
{
    private const string  LcuxBasename     = "LeagueClientUx";
    private const string  LcuxPathSuffix   = "/League of Legends.app/Contents/MacOS/LeagueClientUx";
    private static readonly TimeSpan PollInterval = TimeSpan.FromMilliseconds(5);

    private readonly Action<int, string> _onMatch;
    private readonly HashSet<int>        _seen = [];
    private readonly object              _lock = new();

    private CancellationTokenSource? _cts;
    private Thread?                  _thread;

    public LcuxWatcher(Action<int, string> onMatch)
    {
        _onMatch = onMatch;
    }

    public bool IsRunning => _thread is { IsAlive: true };

    /// <summary>Start polling. Captures the current pid set as the baseline so
    /// only future spawns trigger <see cref="_onMatch"/>.</summary>
    public void Start()
    {
        if (IsRunning) return;
        _cts = new CancellationTokenSource();
        lock (_lock)
        {
            _seen.Clear();
            foreach (var p in LibProc.EnumeratePids()) _seen.Add(p);
        }
        Log.Info("LcuxWatcher starting (baseline {0} pids, poll={1}ms)",
                 _seen.Count, (int)PollInterval.TotalMilliseconds);

        _thread = new Thread(() => Loop(_cts.Token))
        {
            Name        = "LcuxWatcher",
            IsBackground = true,
        };
        _thread.Start();
    }

    public void Stop()
    {
        _cts?.Cancel();
        _thread?.Join(TimeSpan.FromSeconds(2));
        _thread = null;
        _cts?.Dispose();
        _cts = null;
        Log.Info("LcuxWatcher stopped");
    }

    /// <summary>Mark a pid as already-seen so the watcher doesn't fire on it.
    /// <see cref="RespawnAction"/> calls this for the pid it just spawned so
    /// we don't recursively catch our own respawn.</summary>
    public void AddSeen(int pid)
    {
        lock (_lock) _seen.Add(pid);
    }

    private void Loop(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            try
            {
                foreach (var pid in LibProc.EnumeratePids())
                {
                    bool isNew;
                    lock (_lock) isNew = _seen.Add(pid);
                    if (!isNew) continue;

                    var path = LibProc.GetPath(pid);
                    if (path is null) continue;

                    int slash    = path.LastIndexOf('/');
                    var basename = slash >= 0 ? path[(slash + 1)..] : path;
                    if (basename != LcuxBasename) continue;

                    if (!path.EndsWith(LcuxPathSuffix, StringComparison.Ordinal))
                    {
                        Log.Debug("LcuxWatcher skip pid={0} path={1} (basename matched but path didn't)",
                                  pid, path);
                        continue;
                    }

                    try { _onMatch(pid, path); }
                    catch (Exception ex) { Log.Error(ex, "LcuxWatcher onMatch threw for pid={0}", pid); }
                }
            }
            catch (Exception ex)
            {
                Log.Error(ex, "LcuxWatcher loop iteration threw");
            }

            try { Thread.Sleep(PollInterval); }
            catch (ThreadInterruptedException) { /* shutting down */ }
        }
    }
}
