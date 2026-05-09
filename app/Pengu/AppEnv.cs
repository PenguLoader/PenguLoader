using System.Reflection;

namespace Pengu;

/// <summary>
/// Process-wide configuration parsed from the command line at startup.
/// Set once via <see cref="ParseCommandLine"/>; everything else is immutable.
/// </summary>
public static class AppEnv
{
    public const string AppName    = "Pengu Loader";
    public const string SingleInstanceMutex = "989d2110-46da-4c8d-84c1-c4a42e43c424";

    /// <summary>
    /// Resolved at first access from this assembly's
    /// <see cref="AssemblyInformationalVersionAttribute"/>, which the SDK
    /// auto-generates from <c>$(Version)</c> in <c>app/Directory.Build.props</c>.
    /// Single source of truth: the root <c>package.json</c>'s <c>version</c>
    /// field flows into <c>$(Version)</c> via a regex read in the props
    /// file, the SDK stamps the assembly attribute, and this property
    /// reads it back.
    ///
    /// <para>Strips any <c>+commit-sha</c> suffix the SDK can append (some
    /// build configurations include the source-control metadata) so the
    /// value stays a clean SemVer.</para>
    /// </summary>
    public static string AppVersion { get; } = ResolveAppVersion();

    private static string ResolveAppVersion()
    {
        var asm = typeof(AppEnv).Assembly;
        var info = asm.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion;
        if (!string.IsNullOrEmpty(info))
        {
            int plus = info.IndexOf('+');
            return plus >= 0 ? info[..plus] : info;
        }
        return asm.GetName().Version?.ToString(3) ?? "0.0.0";
    }

    /// <summary>Vite dev server URL when running with <c>--dev[=URL]</c>. Null
    /// in Release / packed mode (host then loads <c>app://hub/</c> from
    /// <c>app.dat</c>).</summary>
    public static string? DevUrl { get; private set; }

    /// <summary>Drop log threshold to <see cref="Logging.LogLevel.Debug"/>.</summary>
    public static bool Verbose { get; private set; }

    /// <summary>Apply CLI overrides:
    /// <list type="bullet">
    ///   <item><c>--dev</c> / <c>--dev=URL</c> — enable Vite dev mode.</item>
    ///   <item><c>--verbose</c> / <c>-v</c> — drop log threshold.</item>
    /// </list></summary>
    public static void ParseCommandLine(string[] args)
    {
        const string DevDefault = "http://localhost:1420";
        foreach (var a in args)
        {
            if (a == "--dev")
                DevUrl = DevDefault;
            else if (a.StartsWith("--dev=", StringComparison.OrdinalIgnoreCase))
            {
                var url = a["--dev=".Length..];
                if (Uri.TryCreate(url, UriKind.Absolute, out _))
                    DevUrl = url.TrimEnd('/');
            }
            else if (a is "--verbose" or "-v")
                Verbose = true;
        }
    }
}
