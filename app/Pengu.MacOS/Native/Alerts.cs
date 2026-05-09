using AppKit;
using Foundation;
using Pengu.Logging;

namespace Pengu.MacOS.Native;

/// <summary>
/// Native NSAlert helpers. Used to surface failures the hub UI can't or
/// won't render itself (e.g. activation prerequisite missing —
/// <c>core.dylib</c> not on disk).
///
/// <para>All entry points marshal to the main thread before invoking AppKit,
/// because callers may be on the LcuxWatcher polling thread or a Task
/// continuation.</para>
/// </summary>
internal static class Alerts
{
    /// <summary>
    /// Show a modal warning alert. <paramref name="title"/> goes in the
    /// large-text label, <paramref name="detail"/> in the smaller body
    /// underneath. Returns when the user clicks OK.
    /// </summary>
    public static void ShowWarning(string title, string? detail = null)
    {
        Show(NSAlertStyle.Warning, title, detail);
    }

    /// <summary>
    /// Show a modal error alert (red-themed icon).
    /// </summary>
    public static void ShowError(string title, string? detail = null)
    {
        Show(NSAlertStyle.Critical, title, detail);
    }

    private static void Show(NSAlertStyle style, string title, string? detail)
    {
        try
        {
            NSApplication.SharedApplication.InvokeOnMainThread(() =>
            {
                using var alert = new NSAlert
                {
                    MessageText     = title,
                    InformativeText = detail ?? string.Empty,
                    AlertStyle      = style,
                };
                alert.AddButton("OK");
                alert.RunModal();
            });
        }
        catch (Exception ex)
        {
            Log.Warn("Alerts.Show threw ({0}): {1}", style, ex.Message);
        }
    }
}
