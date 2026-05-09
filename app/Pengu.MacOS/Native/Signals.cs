using System.Runtime.InteropServices;

namespace Pengu.MacOS.Native;

/// <summary>
/// POSIX signal numbers and <c>kill(2)</c> P/Invoke. Used by
/// <c>RespawnAction</c> to SIGSTOP a freshly-spawned LCUX before it reaches
/// <c>cef_initialize</c>, and (deliberately) NOT to SIGKILL it — the
/// kill-and-respawn design leaves the original SIGSTOP'd forever so
/// <c>LeagueClient</c>'s <c>SIGCHLD</c>-based child-watch keeps Foundation
/// alive. SIGKILL is exposed for cleanup of orphan stopped pids on shutdown
/// (Phase K ZombieReaper).
/// </summary>
internal static partial class Signals
{
    public const int SIGKILL = 9;
    public const int SIGSTOP = 17;
    public const int SIGCONT = 19;

    [LibraryImport("/usr/lib/libSystem.dylib", SetLastError = true)]
    public static partial int kill(int pid, int sig);
}
