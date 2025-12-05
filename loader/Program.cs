using System;
using System.Threading;
using System.Threading.Tasks;

namespace Pengu.Loader
{
    static partial class Program
    {
        [STAThread]
        static int Main(string[] args)
        {
            Logger.Info("Pengu Loader started.");

            var debugger = new RiotClient.Debugger(8889);

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

        static void InjectFrontend(RiotClient.DevTools dt)
        {
            string viteDevServerUrl = "https://localhost:3000";
            _ = dt.EvaluateScript(
                $$"""
                console.log("Injecting Vite Dev Server frontend from {{viteDevServerUrl}}");
                (async () => {
                    await import(`{{viteDevServerUrl}}/@vite/client`);
                    await import(`{{viteDevServerUrl}}/src/index.tsx`);
                })();
                """);
        }
    }
}