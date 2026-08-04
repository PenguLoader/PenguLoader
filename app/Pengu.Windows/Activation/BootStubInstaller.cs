using System.ComponentModel;
using System.Diagnostics;
using Pengu.Activation;
using Pengu.Logging;

namespace Pengu.Windows.Activation;

/// <summary>
/// Places <c>boot.dll</c> in an administrator-owned directory and takes it
/// away again. IFEO names it through <c>rundll32</c>, permanently.
///
/// <para><c>%ProgramData%\.pengu\</c> and not the data root: created by an
/// elevated process it inherits ProgramData's default ACL, which gives normal
/// users read and execute but no write. That matters — the IFEO value is
/// admin-gated, so whatever it points at must be too, or an unprivileged
/// process could replace the code every account's client loads. The per-user
/// data root at <c>%LOCALAPPDATA%\.pengu\</c> shares the name and nothing
/// else; the two never meet.</para>
///
/// <para>A DLL rather than an executable of our own because Defender
/// quarantines the executable — registering an unknown binary as an IFEO
/// Debugger is scored behaviourally the moment the client triggers it, and
/// code signing buys no exemption. <c>rundll32</c> loads it instead.</para>
///
/// <para>Elevation goes through <c>cmd /c</c> with <c>Verb=runas</c>, matching
/// <see cref="IfeoAction"/>: writes to IFEO from a normal API call trip AV
/// signature heuristics, while the same work via the system-trusted
/// <c>reg.exe</c> and <c>copy</c> does not. Install and wire-up are issued as
/// a single command so activation costs one prompt, not two.</para>
/// </summary>
internal static class BootStubInstaller
{
    public const string FileName = "boot.dll";

    /// <summary>Administrator-owned install directory.</summary>
    public static string InstallDir { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
        ".pengu");

    /// <summary>Where IFEO points, permanently.</summary>
    public static string InstalledPath { get; } = Path.Combine(InstallDir, FileName);

    /// <summary>The copy this build carries, next to the host exe.</summary>
    public static string ShippedPath(string exeDir) => Path.Combine(exeDir, FileName);

    public static string? VersionOf(string path)
    {
        try
        {
            if (!File.Exists(path)) return null;
            var v = FileVersionInfo.GetVersionInfo(path).FileVersion;
            return string.IsNullOrWhiteSpace(v) ? null : v;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// Should we replace the installed stub with the one we ship?
    ///
    /// <para>Version-forward only. An older portable build running against a
    /// newer installed boot is fine and must not nag — the boot knows only how
    /// to find and inject a runtime, so a v1 boot can drive a v5 core. In
    /// practice this should almost never fire.</para>
    ///
    /// <para>The one thing that does force it is a trust-key rotation, and
    /// that is why the key has two slots: the successor ships in a boot a
    /// release before any core is signed with it, so an install that never
    /// takes the update keeps working instead of silently refusing the new
    /// core.</para>
    /// </summary>
    public static bool NeedsUpdate(string exeDir)
    {
        var shipped = VersionOf(ShippedPath(exeDir));
        if (shipped is null) return false;                 // nothing to install from

        var installed = VersionOf(InstalledPath);
        if (installed is null) return true;                // absent

        return Version.TryParse(shipped, out var s)
            && Version.TryParse(installed, out var i)
            && s > i;
    }

    /// <summary>
    /// The <c>cmd</c> fragment that copies the boot into place, or null when
    /// the shipped copy is missing. Caller composes it with the IFEO write so
    /// both happen under one elevation.
    ///
    /// <para>Fails while a client is running: rundll32 holds the DLL mapped
    /// for the whole session, so the copy hits a sharing violation and
    /// <see cref="RunElevated"/> surfaces the exit code. Closing the client
    /// and retrying is the fix, and it only ever comes up on a boot update.
    /// </para>
    /// </summary>
    public static string? BuildInstallCommand(string exeDir)
    {
        var shipped = ShippedPath(exeDir);
        if (!File.Exists(shipped)) return null;

        // md is idempotent enough for our purposes: it fails when the
        // directory exists, which is why the copy is chained with & rather
        // than &&.
        return $"md \"{InstallDir}\" & copy /y \"{shipped}\" \"{InstalledPath}\"";
    }

    /// <summary>Run a composed command elevated. One UAC prompt.</summary>
    public static ActivationResult RunElevated(string command, string stage)
    {
        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = "/c " + command,
                Verb = "runas",
                UseShellExecute = true,
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden,
            };

            using var p = Process.Start(psi);
            if (p is null)
                return ActivationResult.Fail("Process.Start returned null", stage);

            p.WaitForExit();
            if (p.ExitCode != 0)
            {
                Log.Warn("BootStubInstaller: {0} exited with {1}", stage, p.ExitCode);
                return ActivationResult.Fail($"elevated step exited with code {p.ExitCode}", stage);
            }

            return ActivationResult.Success;
        }
        catch (Win32Exception ex) when (ex.NativeErrorCode == 1223)
        {
            // ERROR_CANCELLED — the user said no to UAC. A decision, not a bug.
            return ActivationResult.Fail("Elevation cancelled by user", stage);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "BootStubInstaller.RunElevated({0}) threw", stage);
            return ActivationResult.Fail(ex.Message, stage);
        }
    }
}
