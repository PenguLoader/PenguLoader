using AppKit;
using Foundation;
using Pengu;
using Pengu.Logging;
using Pengu.MacOS.Native;

namespace Pengu.MacOS;

internal static class Program
{
    private static int Main(string[] args)
    {
        AppEnv.ParseCommandLine(args);
        Log.Initialize(AppContext.BaseDirectory, AppEnv.Verbose);

        try
        {
            // NSApplication.Init wires up the AppKit runtime and is required
            // before any AppKit symbol is touched, including NSRunningApplication
            // queries inside SingleInstance.
            NSApplication.Init();

            // Reject a second launch via the named-Mutex lock (same UUID as
            // Pengu.Windows + the Tauri loader, so cross-tool single-instance
            // works). Second instance exits silently; the user re-summons the
            // running instance by clicking the Dock icon, which AppKit routes
            // to applicationShouldHandleReopen → un-hide the window.
            if (!SingleInstance.TryAcquire())
            {
                return 0;
            }

            try
            {
                var app = NSApplication.SharedApplication;
                app.Delegate = new AppDelegate();
                app.ActivationPolicy = NSApplicationActivationPolicy.Regular;
                app.Run();

                return 0;
            }
            finally
            {
                SingleInstance.Release();
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Unhandled exception in Program.Main");
            return 1;
        }
        finally
        {
            Log.Info("Pengu exiting");
            Log.Shutdown();
        }
    }
}
