using Pengu.Logging;

namespace Pengu.Windows.State;

/// <summary>
/// The per-user activation pointer at <c>%LOCALAPPDATA%\.pengu\active</c>.
///
/// <para>Contents are the absolute directory of the install to load, one line,
/// UTF-8 without a BOM. Absent or empty means "not activated for this user",
/// which <c>boot.dll</c> treats as a plain client launch.</para>
///
/// <para>This file is why activation no longer needs administrator rights.
/// IFEO is machine-wide and stays put; the boot reads whichever pointer
/// belongs to the account that launched the client, so each user activates
/// independently and can't affect anyone else.</para>
///
/// <para>It is deliberately not a trust boundary. A user can name any
/// directory here; the boot decides whether to load what it finds by checking
/// the signature embedded in that core.dll and its Authenticode. Naming
/// something is not authorising it.</para>
/// </summary>
internal static class ActivePointer
{
    public static string PathFor(string dataRoot) => Path.Combine(dataRoot, "active");

    /// <summary>The install directory currently pointed at, or null.</summary>
    public static string? Read(string dataRoot)
    {
        var file = PathFor(dataRoot);
        try
        {
            if (!File.Exists(file)) return null;

            var text = File.ReadAllText(file);
            var line = text.Split('\n', '\r')[0].Trim();
            return line.Length == 0 ? null : line;
        }
        catch (Exception ex)
        {
            Log.Warn("ActivePointer: could not read {0}: {1}", file, ex.Message);
            return null;
        }
    }

    /// <summary>
    /// Point at <paramref name="installDir"/>. Written to a temp file and
    /// moved into place, so two installs racing can't leave a half-written
    /// path that the stub would then fail to resolve.
    /// </summary>
    public static bool Write(string dataRoot, string installDir)
    {
        var file = PathFor(dataRoot);
        var temp = file + ".tmp";

        try
        {
            Directory.CreateDirectory(dataRoot);

            // No BOM: the stub reads raw bytes and would take one for part of
            // the path.
            File.WriteAllText(temp, installDir, new System.Text.UTF8Encoding(false));
            File.Move(temp, file, overwrite: true);

            Log.Info("ActivePointer: now {0}", installDir);
            return true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "ActivePointer: could not write {0}", file);
            try { if (File.Exists(temp)) File.Delete(temp); } catch { /* best effort */ }
            return false;
        }
    }

    /// <summary>Deactivate this user. The stub falls back to a plain launch.</summary>
    public static bool Clear(string dataRoot)
    {
        var file = PathFor(dataRoot);
        try
        {
            if (File.Exists(file))
                File.Delete(file);

            Log.Info("ActivePointer: cleared");
            return true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "ActivePointer: could not clear {0}", file);
            return false;
        }
    }

    /// <summary>
    /// Why the stub last refused to load a runtime, if it did.
    ///
    /// <para>The stub writes this beside the pointer. Surfacing it matters:
    /// a refusal is far more likely to be a signing or rollout mistake than an
    /// attack, and without it Pengu just silently stops working.</para>
    /// </summary>
    public static string? ReadLastRefusal(string dataRoot)
    {
        var file = Path.Combine(dataRoot, "boot.log");
        try
        {
            if (!File.Exists(file)) return null;

            var lines = File.ReadAllLines(file);
            for (int i = lines.Length - 1; i >= 0; --i)
            {
                var line = lines[i].Trim();
                if (line.Length > 0 && !line.StartsWith("WARNING", StringComparison.Ordinal))
                    return line;
            }
            return null;
        }
        catch
        {
            return null;
        }
    }
}
