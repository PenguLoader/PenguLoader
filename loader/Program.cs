using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pengu.Loader
{
    static partial class Program
    {
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate nint GetCommandLineWFn();

        static nint CommandLine;
        static string? ExtraArgs;

        static Utils.Hook<GetCommandLineWFn> GetCommandLineW = new();
        static GetCommandLineWFn NewGetCommandLineW = () =>
        {
            if (CommandLine == 0)
            {
                using var call = GetCommandLineW.GetCall();
                var full = Marshal.PtrToStringUni(call.Func());
                full += ExtraArgs;

                CommandLine = Marshal.StringToHGlobalUni(full);
            }

            return CommandLine;
        };

        [UnmanagedCallersOnly(EntryPoint = nameof(NativeMain))]
        public static int NativeMain()
        {
            Config.Load();

            var p1 = Utils.Network.GetFreeTcpPort(out int debugPort);
            var p2 = Utils.Network.GetFreeTcpPort(out int webPort);

            ExtraArgs = $" --remote-debugging-port={debugPort}";

            if (Config.I.riot_potato_mode)
            {
                ExtraArgs += " --disable-smooth-scrolling --force-prefers-reduced-motion";
                ExtraArgs += " --wm-window-animations-disabled --animation-duration-scale=0";
            }

            GetCommandLineW.Install("kernel32", "GetCommandLineW", NewGetCommandLineW);

            p1.Dispose();
            p2.Dispose();

            var server = new App.WebServer(webPort);
            var debugger = new RiotClient.Debugger(debugPort, webPort, false);

            server.Listen();
            Task.Run(debugger.Connect);

            RiotClient.Window.SetupWindow(debugger);

            return 0;
        }

        [STAThread]
        static int Main(string[] args)
        {
            Config.Load();
            Log.Info("Pengu Loader started with args: {0}", Environment.CommandLine);

            var debugger = new RiotClient.Debugger(8889, 3000, true);

            Task.Run(async () =>
            {
                Log.Info("Connecting to Riot Client...");

                await debugger.Connect();

                Log.Info("Press Q to quit, R to reload, I or D to open devtools.");

                //RiotClient.Window.SetupWindow(debugger);
            });

            bool running = true;
            Console.CancelKeyPress += (s, e) =>
            {
                e.Cancel = true;
                running = false;
            };

            while (running)
            {
                if (Console.KeyAvailable)
                {
                    var key = Console.ReadKey(true);

                    switch (key.Key)
                    {
                        case ConsoleKey.Q:
                            running = false;
                            break;

                        case ConsoleKey.R:
                            debugger.ReloadPage();
                            break;

                        case ConsoleKey.I:
                        case ConsoleKey.D:
                            debugger.OpenRemoteDevTools();
                            break;
                    }
                }

                Thread.Sleep(50);
            }

            Log.Info("Loader exiting...");
            Log.Shutdown();

            return 0;
        }
    }
}