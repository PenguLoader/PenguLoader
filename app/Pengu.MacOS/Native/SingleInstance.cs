using AppKit;
using Foundation;
using Pengu;
using Pengu.Logging;

namespace Pengu.MacOS.Native;

/// <summary>
/// Same-machine single-instance lock via a named Mutex. Mirrors the Tauri
/// loader's <c>named-lock</c> crate usage and the Windows side's <c>Mutex</c>
/// pattern; the same UUID (<see cref="AppEnv.SingleInstanceMutex"/>) is used
/// across all three tools, so a Tauri+CLR pair, or any Windows-VM-on-Mac
/// scenario, hit the same lock.
///
/// <para>Unlike the Windows side, we do not broadcast a "show me" message
/// to the running instance. The macOS convention is: clicking the Dock icon
/// (which fires <c>applicationShouldHandleReopen</c>) is how the user
/// re-summons the window. Our <see cref="AppDelegate"/> handles that callback
/// to un-hide the BorderlessWindow if it was orderOut'd via the close button.</para>
/// </summary>
internal static class SingleInstance
{
    private static Mutex? s_mutex;

    /// <summary>
    /// Try to acquire the single-instance mutex. Returns true if this is the
    /// first instance; false if another is already running.
    /// </summary>
    public static bool TryAcquire()
    {
        s_mutex = new Mutex(initiallyOwned: true, AppEnv.SingleInstanceMutex, out var createdNew);
        if (createdNew)
        {
            Log.Info("Single-instance acquired");
            return true;
        }

        Log.Info("Another Pengu instance is running; signaling it and exiting");
        s_mutex.Dispose();
        s_mutex = null;

        // Wake the running instance so its window respawns even after the user
        // closed it earlier. NSRunningApplication.Activate fires
        // applicationShouldHandleReopen on the receiver, which is wired in
        // AppDelegate to call MacOSHost.BringMainWindowToFront.
        try
        {
            int selfPid = Environment.ProcessId;
            var others = NSRunningApplication
                .GetRunningApplications("com.pengu.lol")
                .Where(app => app.ProcessIdentifier != selfPid);
            foreach (var app in others)
            {
                app.Activate(NSApplicationActivationOptions.ActivateAllWindows);
                break;
            }
        }
        catch (Exception ex)
        {
            // Activation failure isn't fatal — we still acquired-and-released
            // the mutex correctly, so the second instance just exits silently.
            Log.Warn("Failed to activate running Pengu instance: {0}", ex.Message);
        }

        return false;
    }

    public static void Release()
    {
        s_mutex?.ReleaseMutex();
        s_mutex?.Dispose();
        s_mutex = null;
    }
}
