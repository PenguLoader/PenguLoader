using System.Runtime.InteropServices;
using System.Text;

namespace Pengu.MacOS.Native;

/// <summary>
/// Reads argv + envp from another same-UID process via
/// <c>sysctl(CTL_KERN, KERN_PROCARGS2)</c>. No entitlements required.
/// Used by <c>RespawnAction</c> to capture the original LCUX's launch
/// parameters before re-spawning with <c>DYLD_INSERT_LIBRARIES</c> added.
///
/// <para>Buffer layout per <c>man 3 ps</c> / xnu source:
/// <code>
///     int32  argc
///     char[] exec_path (null-terminated)
///     char[] padding (null bytes for alignment)
///     char[] argv[0]   (null-terminated)
///     char[] argv[1]   (null-terminated)
///     ... argc total ...
///     char[] envp[0]   (null-terminated)
///     char[] envp[1]   (null-terminated)
///     ... until empty entry or end of buffer ...
/// </code>
/// </para>
/// </summary>
internal static partial class ProcArgs
{
    private const int CTL_KERN       = 1;
    private const int KERN_PROCARGS2 = 49;

    [LibraryImport("/usr/lib/libSystem.dylib", SetLastError = true)]
    private static unsafe partial int sysctl(int* name, uint namelen,
                                             void* oldp, ulong* oldlenp,
                                             void* newp, ulong newlen);

    public sealed record Snapshot(string ExePath, string[] Argv, string[] Envp);

    public static unsafe Snapshot Read(int pid)
    {
        Span<int> mib = stackalloc int[3] { CTL_KERN, KERN_PROCARGS2, pid };

        ulong size = 0;
        fixed (int* mibPtr = mib)
        {
            if (sysctl(mibPtr, 3, null, &size, null, 0) != 0)
                throw new InvalidOperationException(
                    $"sysctl(KERN_PROCARGS2) size query failed errno={Marshal.GetLastSystemError()} pid={pid}");
        }

        var buf = new byte[size];
        fixed (int*  mibPtr = mib)
        fixed (byte* bufPtr = buf)
        {
            if (sysctl(mibPtr, 3, bufPtr, &size, null, 0) != 0)
                throw new InvalidOperationException(
                    $"sysctl(KERN_PROCARGS2) data fetch failed errno={Marshal.GetLastSystemError()} pid={pid}");
        }

        int argc = BitConverter.ToInt32(buf, 0);
        int pos  = sizeof(int);

        // Executable path (null-terminated).
        int end = Array.IndexOf(buf, (byte)0, pos);
        string exePath = Encoding.UTF8.GetString(buf, pos, end - pos);
        pos = end + 1;

        // Alignment padding (consecutive nulls between exec path and argv[0]).
        while (pos < buf.Length && buf[pos] == 0) pos++;

        var argv = new string[argc];
        for (int i = 0; i < argc; i++)
        {
            end = Array.IndexOf(buf, (byte)0, pos);
            if (end < 0) end = buf.Length;
            argv[i] = Encoding.UTF8.GetString(buf, pos, end - pos);
            pos = end + 1;
        }

        var envList = new List<string>();
        while (pos < buf.Length)
        {
            end = Array.IndexOf(buf, (byte)0, pos);
            if (end < 0) end = buf.Length;
            if (end == pos) break; // empty entry terminates envp
            envList.Add(Encoding.UTF8.GetString(buf, pos, end - pos));
            pos = end + 1;
        }

        return new Snapshot(exePath, argv, envList.ToArray());
    }
}
