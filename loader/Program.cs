using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pengu.Loader
{
    static partial class Program
    {
        [UnmanagedCallersOnly(EntryPoint = nameof(NativeMain))]
        public static int NativeMain()
        {


            return Main(["-native"]);
        }

        [STAThread]
        static int Main(string[] args)
        {
            Logger.Info("Pengu Loader started.");
            Config.Load();

            var debugger = new RiotClient.Debugger(8889, 3000, true);

            Task.Run(async () =>
            {
                Logger.Info("Connecting to Riot Client...");

                await debugger.Connect();

                Logger.Info("Press Q to quit, R to reload, I or D to open devtools.");

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

            Logger.Info("Loader exiting...");
            Logger.Shutdown();

            return 0;
        }
    }
}