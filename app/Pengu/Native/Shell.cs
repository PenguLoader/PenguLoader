using System.Diagnostics;
using Pengu.Logging;

namespace Pengu.Native;

/// <summary>
/// Cross-platform shell invocations. Static helpers because the operations
/// are stateless and called from both Pengu (core) and the head's IHost
/// implementations.
///
/// <para>Platform branching at call-time via <see cref="OperatingSystem.IsWindows"/> /
/// <see cref="OperatingSystem.IsMacOS"/>. Both head csprojs target a
/// platform-suffixed TFM, so the inactive branch is effectively dead code at
/// publish time — AOT linker drops it.</para>
/// </summary>
public static class Shell
{
    /// <summary>
    /// Reject strings that can't be safely embedded in the quoted argument we
    /// hand to explorer.exe / open. A quote breaks out of our quoting and lets
    /// the caller append arguments — and <c>explorer.exe &lt;some.exe&gt;</c>
    /// executes that path. None of these characters are legal in a Windows
    /// path, so refusing them costs nothing and closes the injection.
    /// </summary>
    private static bool IsSafePathArgument(string path)
        => !string.IsNullOrEmpty(path)
            && !path.Contains('"')
            && path.IndexOfAny(['\r', '\n', '\0']) < 0;

    /// <summary>Open a folder in Explorer (Windows) / Finder (macOS). Creates
    /// the folder if it doesn't exist; failure is logged but not thrown.</summary>
    public static void OpenFolder(string path)
    {
        try
        {
            if (!IsSafePathArgument(path))
            {
                Log.Warn("Shell.OpenFolder rejected unsafe path {0}", path);
                return;
            }

            // Explicit, rather than relying on CreateDirectory throwing: handing
            // a file path to explorer.exe would execute it.
            if (File.Exists(path))
            {
                Log.Warn("Shell.OpenFolder rejected file path {0}", path);
                return;
            }

            Directory.CreateDirectory(path);
            if (OperatingSystem.IsWindows())
            {
                Process.Start(new ProcessStartInfo("explorer.exe", $"\"{path.Replace('/', '\\')}\"")
                {
                    UseShellExecute = false,
                });
            }
            else if (OperatingSystem.IsMacOS())
            {
                Process.Start(new ProcessStartInfo("open", $"\"{path}\"")
                {
                    UseShellExecute = false,
                });
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Shell.OpenFolder failed for {0}", path);
        }
    }

    /// <summary>Reveal a file (highlighted in its folder) — Explorer
    /// <c>/select,</c> on Windows, <c>open -R</c> on macOS.</summary>
    public static void RevealFile(string path)
    {
        try
        {
            if (!IsSafePathArgument(path))
            {
                Log.Warn("Shell.RevealFile rejected unsafe path {0}", path);
                return;
            }

            if (OperatingSystem.IsWindows())
            {
                Process.Start(new ProcessStartInfo("explorer.exe", $"/select,\"{path.Replace('/', '\\')}\"")
                {
                    UseShellExecute = false,
                });
            }
            else if (OperatingSystem.IsMacOS())
            {
                Process.Start(new ProcessStartInfo("open", $"-R \"{path}\"")
                {
                    UseShellExecute = false,
                });
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Shell.RevealFile failed for {0}", path);
        }
    }

    /// <summary>
    /// Open an external URL in the user's default browser. The scheme check is
    /// enforced here, not just at the JS facade: the bridge is reachable from
    /// any script running in the webview, and the plugin store renders repo
    /// links straight out of the remote registry. Windows uses
    /// <c>UseShellExecute = true</c>, so an unvetted string like
    /// <c>C:\x\evil.bat</c> would be executed rather than browsed to; macOS
    /// <c>open</c> behaves the same way.
    /// </summary>
    public static void OpenExternal(string url)
    {
        try
        {
            if (!Uri.TryCreate(url, UriKind.Absolute, out var uri)
                || (uri.Scheme != Uri.UriSchemeHttp && uri.Scheme != Uri.UriSchemeHttps))
            {
                Log.Warn("Shell.OpenExternal rejected non-http(s) url {0}", url);
                return;
            }

            if (OperatingSystem.IsWindows())
            {
                // UseShellExecute hands the string to the shell, which is what
                // routes an https:// url to the default browser — and exactly
                // why the scheme check above is not optional.
                Process.Start(new ProcessStartInfo
                {
                    // AbsoluteUri, not the raw string: it is the normalised,
                    // percent-escaped form, so nothing survives that could break
                    // out of the macOS argument quoting below.
                    FileName = uri.AbsoluteUri,
                    UseShellExecute = true,
                });
            }
            else if (OperatingSystem.IsMacOS())
            {
                Process.Start(new ProcessStartInfo("open", $"\"{uri.AbsoluteUri}\"")
                {
                    UseShellExecute = false,
                });
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Shell.OpenExternal failed for {0}", url);
        }
    }
}
