using Microsoft.Win32;
using Pengu.Activation;
using Pengu.Logging;
using Pengu.Windows.State;

namespace Pengu.Windows.Activation;

/// <summary>
/// Universal-mode activation.
///
/// <para>IFEO's <c>Debugger</c> value under
/// <c>HKLM\...\Image File Execution Options\LeagueClientUx.exe</c> holds
/// <c>rundll32 "%ProgramData%\.pengu\boot.dll", #6000</c> — a fixed,
/// administrator-owned path that never changes. The boot then reads
/// <c>%LOCALAPPDATA%\.pengu\active</c> to decide which install to load.</para>
///
/// <para>That indirection buys three things the old
/// <c>rundll32 "&lt;install&gt;\core.dll", #6000</c> value could not:</para>
///
/// <list type="bullet">
///   <item><description>Deleting a portable folder can't break the client.
///     The registry pointed straight at that folder, so removing it left IFEO
///     redirecting into nothing and LCUX never launched — a broken client that
///     only an HKLM edit could fix. A stale pointer now just means no plugin
///     runtime.</description></item>
///   <item><description>Only the first activation needs administrator rights.
///     Afterwards, on and off is a per-user file write.</description></item>
///   <item><description>Activation is per user. IFEO is machine-wide, so the
///     old value forced one account's install on everyone.</description></item>
/// </list>
///
/// <para>Reads use <see cref="RegistryKey"/> directly — read APIs don't trip
/// AV static analysis, only writes to IFEO do. Writes shell out to
/// <c>reg.exe</c> under <c>Verb=runas</c>, which is system-trusted and avoids
/// the signature heuristics that flag direct <c>RegSetValueEx</c>-on-IFEO
/// calls. Same approach v1.1.6's WPF loader uses.</para>
/// </summary>
internal sealed class IfeoAction : IActivationAction, IBootStub
{
    private const string IfeoSubKey = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options";
    private const string TargetExe  = "LeagueClientUx.exe";
    private const string ValueName  = "Debugger";

    /// <summary>Ordinal of <c>_BootstrapEntry</c>, exported from
    /// <c>boot.dll</c> via <c>boot/res/module.def</c>. <c>rundll32</c> calls it
    /// with the client's own command line. Kept at 6000 to match the value
    /// this project has shipped since v1.1.6; if it ever moves, the .def and
    /// this constant change in lockstep.</summary>
    private const string BootstrapOrdinal = "#6000";

    private readonly string _exeDir;
    private readonly string _dataRoot;

    public IfeoAction(string exeDir, string dataRoot)
    {
        _exeDir = exeDir;
        _dataRoot = dataRoot;
    }

    public ActivationMode Mode => ActivationMode.Universal;

    /// <summary>
    /// Active means both halves hold: the machine is wired to our stub, and
    /// this user's pointer names this install. Either alone does nothing.
    /// </summary>
    public Task<bool> IsActiveAsync(CancellationToken ct)
    {
        try
        {
            if (!IsWiredToStub())
                return Task.FromResult(false);

            var pointer = ActivePointer.Read(_dataRoot);
            return Task.FromResult(pointer is not null && SamePath(pointer, _exeDir));
        }
        catch (Exception ex)
        {
            Log.Warn("IfeoAction.IsActiveAsync failed: {0}", ex.Message);
            return Task.FromResult(false);
        }
    }

    public Task<ActivationResult> SetActiveAsync(bool active, CancellationToken ct)
    {
        if (!active)
        {
            // Deactivating never touches machine-wide state. The stub stays
            // installed and IFEO stays wired; with no pointer it just launches
            // the client untouched. Removing the boot is a separate, explicit
            // action — see RemoveBootAsync.
            return Task.FromResult(ActivePointer.Clear(_dataRoot)
                ? ActivationResult.Success
                : ActivationResult.Fail("Could not clear the activation pointer", "ClearPointer"));
        }

        // Elevate at most once, and only when something machine-wide is
        // actually missing or stale.
        var steps = new List<string>();
        var stage = "Install";

        if (BootStubInstaller.NeedsUpdate(_exeDir))
        {
            var install = BootStubInstaller.BuildInstallCommand(_exeDir);
            if (install is null)
                return Task.FromResult(ActivationResult.Fail(
                    $"{BootStubInstaller.FileName} is missing from the install folder", "Install"));

            steps.Add(install);
        }

        if (!IsWiredToStub())
        {
            // cmd needs the outer "..." around the whole /d argument; the
            // inner \" survive cmd untouched and are unescaped by the argv
            // parser on reg.exe's side, so the value lands as
            // `rundll32 "<path>", #6000`. Same shape as v1.1.6's IFEO write.
            steps.Add($"reg add \"HKLM\\{IfeoSubKey}\\{TargetExe}\" /v {ValueName} /t REG_SZ " +
                      $"/d \"rundll32 \\\"{BootStubInstaller.InstalledPath}\\\", {BootstrapOrdinal}\" /f");
            stage = steps.Count > 1 ? "InstallAndWire" : "Wire";
        }

        if (steps.Count > 0)
        {
            var result = BootStubInstaller.RunElevated(string.Join(" && ", steps), stage);
            if (!result.Ok)
                return Task.FromResult(result);
        }

        // The part that doesn't need admin, and the only part a user repeats.
        if (!ActivePointer.Write(_dataRoot, _exeDir))
            return Task.FromResult(ActivationResult.Fail("Could not write the activation pointer", "WritePointer"));

        Log.Info("IfeoAction: activated for this user ({0})", _exeDir);
        return Task.FromResult(ActivationResult.Success);
    }

    public Task<BootStubState> GetBootStateAsync(CancellationToken ct)
    {
        var installed = BootStubInstaller.VersionOf(BootStubInstaller.InstalledPath);
        var shipped = BootStubInstaller.VersionOf(BootStubInstaller.ShippedPath(_exeDir));

        return Task.FromResult(new BootStubState(
            Installed: installed is not null,
            Wired: IsWiredToStub(),
            InstalledVersion: installed,
            ShippedVersion: shipped,
            UpdateAvailable: BootStubInstaller.NeedsUpdate(_exeDir),
            LastRefusal: ActivePointer.ReadLastRefusal(_dataRoot)));
    }

    public Task<ActivationResult> RemoveBootAsync(CancellationToken ct)
    {
        // Registry first, stub second — see IBootStub for why the order is
        // load-bearing. Chained with && so an interrupted run can only stop
        // in the safe place.
        var command =
            $"reg delete \"HKLM\\{IfeoSubKey}\\{TargetExe}\" /f && " +
            $"del /f /q \"{BootStubInstaller.InstalledPath}\"";

        var result = BootStubInstaller.RunElevated(command, "RemoveBoot");
        if (!result.Ok)
            return Task.FromResult(result);

        // This user's pointer is meaningless now; other users' pointers go
        // inert on their own.
        ActivePointer.Clear(_dataRoot);

        Log.Info("IfeoAction: boot removed");
        return Task.FromResult(ActivationResult.Success);
    }

    // Daemon callbacks are no-ops for Universal mode — IFEO fires from
    // kernel-side image-load redirection, not from RCS announcements.
    public Task OnSessionCreatedAsync(LcuxSession session, CancellationToken ct) => Task.CompletedTask;
    public Task OnSessionDeletedAsync(LcuxSession session, CancellationToken ct) => Task.CompletedTask;

    /// <summary>Does the IFEO value name our boot?</summary>
    private static bool IsWiredToStub()
    {
        using var key = Registry.LocalMachine.OpenSubKey($@"{IfeoSubKey}\{TargetExe}", writable: false);
        if (key is null) return false;

        if (key.GetValue(ValueName) is not string debugger || string.IsNullOrWhiteSpace(debugger))
            return false;

        if (!debugger.TrimStart().StartsWith("rundll32", StringComparison.OrdinalIgnoreCase))
            return false;

        // The value is `rundll32 "<path to boot.dll>", #6000`. A pre-boot
        // install points the same way at its own core.dll, which is exactly
        // the case that has to come back false so activation rewrites it.
        var quoted = ExtractQuotedPath(debugger);
        return quoted is not null && SamePath(quoted, BootStubInstaller.InstalledPath);
    }

    /// <summary>Contents of the first <c>"..."</c> pair, or null when unquoted.</summary>
    private static string? ExtractQuotedPath(string s)
    {
        int first = s.IndexOf('"');
        if (first < 0) return null;
        int second = s.IndexOf('"', first + 1);
        if (second < 0) return null;
        return s.Substring(first + 1, second - first - 1);
    }

    private static bool SamePath(string a, string b)
    {
        static string Normalize(string p) =>
            p.Replace('/', '\\').TrimEnd('\\').ToLowerInvariant();

        return Normalize(a) == Normalize(b);
    }
}
