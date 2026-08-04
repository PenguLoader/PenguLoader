namespace Pengu.Activation;

/// <summary>
/// Implemented by activation actions that rely on a machine-wide boot stub —
/// today only Windows' IFEO mode.
///
/// <para>The stub is the one piece of Pengu that lives outside the install
/// folder, in an administrator-writable directory, referenced permanently by
/// the IFEO registry value. Installing it costs one elevation prompt; after
/// that, turning Pengu on and off is a per-user file write and needs no
/// admin at all. Removing it is therefore a separate, explicit action rather
/// than something that happens whenever a user deactivates.</para>
///
/// <para>Kept off <see cref="IActivationAction"/> deliberately: macOS has no
/// equivalent, and giving every action a method that only one implements
/// would push "not applicable" handling into every caller.</para>
/// </summary>
public interface IBootStub
{
    /// <summary>Is the machine-wide boot installed and wired to IFEO?</summary>
    Task<BootStubState> GetBootStateAsync(CancellationToken ct);

    /// <summary>
    /// Remove the machine-wide boot: the IFEO registry value first, then the
    /// stub itself. Requires elevation.
    ///
    /// <para>The order is not incidental. Deleting the stub while the registry
    /// still points at it is exactly the failure this design exists to remove
    /// — the client would stop launching. Interrupted after the registry
    /// delete, the leftover stub is inert rather than harmful.</para>
    ///
    /// <para>Other users' activation pointers become inert; they simply stop
    /// getting Pengu.</para>
    /// </summary>
    Task<ActivationResult> RemoveBootAsync(CancellationToken ct);
}

/// <summary>
/// What's installed machine-wide, independent of whether the current user has
/// Pengu switched on.
/// </summary>
/// <param name="Installed">The stub exists at its administrator-owned path.</param>
/// <param name="Wired">The IFEO value points at that stub.</param>
/// <param name="InstalledVersion">Version of the installed stub, null if absent.</param>
/// <param name="ShippedVersion">Version this build carries alongside it.</param>
/// <param name="UpdateAvailable">The shipped stub is newer than the installed one.</param>
/// <param name="LastRefusal">
/// Why the boot last declined to load a runtime, if it did.
///
/// <para>The boot never blocks the client — it launches it and carries on
/// without the plugin runtime. That is the right call, but it means a signing
/// or rollout mistake looks exactly like Pengu quietly not working. Surfacing
/// this is the difference between a five-minute fix and a bug report.</para>
/// </param>
public sealed record BootStubState(
    bool Installed,
    bool Wired,
    string? InstalledVersion,
    string? ShippedVersion,
    bool UpdateAvailable,
    string? LastRefusal);
