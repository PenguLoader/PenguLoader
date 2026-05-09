using System.Runtime.InteropServices;
using System.Text;

namespace Pengu.MacOS.Native;

/// <summary>
/// libproc bindings used by <c>LcuxWatcher</c> to enumerate pids and resolve
/// process paths. Both calls work for same-UID targets without entitlements.
/// </summary>
internal static partial class LibProc
{
    private const string libSystem = "/usr/lib/libSystem.dylib";

    /// <summary><c>PROC_ALL_PIDS</c> — return every pid on the system.</summary>
    public const uint PROC_ALL_PIDS = 1;

    /// <summary><c>PROC_PIDPATHINFO_MAXSIZE</c> — buffer size for proc_pidpath
    /// per <c>&lt;sys/proc_info.h&gt;</c>. = 4 * MAXPATHLEN.</summary>
    public const int PROC_PIDPATHINFO_MAXSIZE = 4096;

    [LibraryImport(libSystem, SetLastError = true)]
    private static partial int proc_listpids(uint type, uint typeinfo, IntPtr buffer, int buffersize);

    [LibraryImport(libSystem, SetLastError = true)]
    private static partial int proc_pidpath(int pid, IntPtr buffer, uint buffersize);

    /// <summary>
    /// Snapshot of every pid on the system. Used by <c>LcuxWatcher</c>'s 5 ms
    /// polling loop to diff against a seen-set and detect new
    /// <c>LeagueClientUx</c> spawns.
    /// </summary>
    public static int[] EnumeratePids()
    {
        // First call sizes the buffer; second call fills it. Race-tolerant —
        // if the pid list grows between calls we just truncate.
        int byteCount = proc_listpids(PROC_ALL_PIDS, 0, IntPtr.Zero, 0);
        if (byteCount <= 0) return [];

        var bytes = new byte[byteCount];
        int filled;
        unsafe
        {
            fixed (byte* p = bytes)
                filled = proc_listpids(PROC_ALL_PIDS, 0, (IntPtr)p, byteCount);
        }
        if (filled <= 0) return [];

        int count = filled / sizeof(int);
        var pids = new int[count];
        Buffer.BlockCopy(bytes, 0, pids, 0, count * sizeof(int));
        // Filter sentinels (0/-1) — proc_listpids reserves these for kernel pids
        // that don't have a path or have been reaped between snapshot calls.
        return pids.Where(p => p > 0).ToArray();
    }

    /// <summary>
    /// Resolve the executable path of a pid. Returns null if the pid is dead
    /// or we lack permission to read its path.
    /// </summary>
    public static string? GetPath(int pid)
    {
        Span<byte> buf = stackalloc byte[PROC_PIDPATHINFO_MAXSIZE];
        int n;
        unsafe
        {
            fixed (byte* p = buf)
                n = proc_pidpath(pid, (IntPtr)p, (uint)PROC_PIDPATHINFO_MAXSIZE);
        }
        return n <= 0 ? null : Encoding.UTF8.GetString(buf[..n]);
    }
}
