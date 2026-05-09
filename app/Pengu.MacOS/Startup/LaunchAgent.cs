using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security;
using System.Text;
using Pengu.Logging;

namespace Pengu.MacOS.Startup;

/// <summary>
/// Login-time auto-start via a per-user <c>LaunchAgent</c> plist. Counterpart
/// of Pengu.Windows's <c>HKCU\...\Run</c> registry entry.
///
/// <para>Plist lives at
/// <c>~/Library/LaunchAgents/com.pengu.lol.plist</c> and is registered with
/// the user's GUI launchd domain via <c>launchctl bootstrap gui/&lt;uid&gt; …</c>.
/// On disable, it's <c>bootout</c>'d and the plist is deleted.</para>
///
/// <para>Failures shell out to <c>launchctl</c> via <see cref="Process"/>;
/// stderr surfaces in <see cref="Log"/> on bootstrap failure (e.g. corrupt
/// plist or duplicate label). Bootout failures are ignored — the service may
/// not have been loaded if the user removed the plist by hand or never
/// enabled startup.</para>
/// </summary>
internal static partial class LaunchAgent
{
    private const string Label = "com.pengu.lol";

    /// <summary><c>~/Library/LaunchAgents/com.pengu.lol.plist</c>.</summary>
    public static string PlistPath { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
        "Library", "LaunchAgents", $"{Label}.plist");

    public static bool IsEnabled() => File.Exists(PlistPath);

    public static void Enable(string programPath)
    {
        if (string.IsNullOrEmpty(programPath))
            throw new ArgumentException("programPath is required", nameof(programPath));
        if (!File.Exists(programPath))
            throw new InvalidOperationException($"Cannot enable startup: {programPath} does not exist");

        var uid = geteuid();

        // If a stale plist is loaded from an earlier install, unload it so
        // the new one takes effect on next bootstrap. Ignored if not loaded.
        Run("launchctl", $"bootout gui/{uid}/{Label}", ignoreErrors: true);

        Directory.CreateDirectory(Path.GetDirectoryName(PlistPath)!);
        File.WriteAllText(PlistPath, BuildPlist(programPath));

        Run("launchctl", $"bootstrap gui/{uid} \"{PlistPath}\"");
        Log.Info("LaunchAgent enabled: {0} → {1}", PlistPath, programPath);
    }

    public static void Disable()
    {
        var uid = geteuid();
        Run("launchctl", $"bootout gui/{uid}/{Label}", ignoreErrors: true);
        if (File.Exists(PlistPath))
        {
            try { File.Delete(PlistPath); }
            catch (Exception ex)
            {
                Log.Warn("LaunchAgent: failed to delete {0}: {1}", PlistPath, ex.Message);
            }
        }
        Log.Info("LaunchAgent disabled");
    }

    private static string BuildPlist(string programPath)
    {
        // Hand-roll the plist XML; it's small, schema-stable, and we'd
        // otherwise pull in a plist serializer just for this. SecurityElement.Escape
        // covers the rare case where the path contains an ampersand, quote,
        // or angle bracket — all legal in macOS paths.
        var sb = new StringBuilder(512);
        sb.AppendLine("""<?xml version="1.0" encoding="UTF-8"?>""");
        sb.AppendLine("""<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">""");
        sb.AppendLine("""<plist version="1.0">""");
        sb.AppendLine("<dict>");
        sb.AppendLine("    <key>Label</key>");
        sb.AppendLine($"    <string>{Label}</string>");
        sb.AppendLine("    <key>Program</key>");
        sb.AppendLine($"    <string>{SecurityElement.Escape(programPath)}</string>");
        sb.AppendLine("    <key>RunAtLoad</key>");
        sb.AppendLine("    <true/>");
        sb.AppendLine("    <key>KeepAlive</key>");
        sb.AppendLine("    <false/>");
        sb.AppendLine("    <key>ProcessType</key>");
        sb.AppendLine("    <string>Interactive</string>");
        sb.AppendLine("</dict>");
        sb.AppendLine("</plist>");
        return sb.ToString();
    }

    private static int Run(string fileName, string args, bool ignoreErrors = false)
    {
        var psi = new ProcessStartInfo
        {
            FileName               = fileName,
            Arguments              = args,
            UseShellExecute        = false,
            RedirectStandardOutput = true,
            RedirectStandardError  = true,
            CreateNoWindow         = true,
        };
        using var p = Process.Start(psi)
            ?? throw new InvalidOperationException($"Failed to start {fileName}");
        var stderr = p.StandardError.ReadToEnd();
        p.WaitForExit();
        if (p.ExitCode != 0 && !ignoreErrors)
            throw new InvalidOperationException(
                $"{fileName} {args} failed (exit={p.ExitCode}): {stderr.Trim()}");
        return p.ExitCode;
    }

    [LibraryImport("/usr/lib/libSystem.dylib")]
    private static partial uint geteuid();
}
