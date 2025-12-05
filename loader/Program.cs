using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Pengu.Loader
{
    static partial class Program
    {
        static int Main(string[] args)
        {
            Task.Run(async () =>
            {
                var dbg = new RiotClient.Debugger(8889);

                await dbg.Connect();
                await dbg.DevTools.SetBypassCSP(true);

                dbg.DevTools.PageLoaded += () =>
                {
                    _ = dbg.DevTools.SendMethod("Page.enable");
                    InjectFrontend(dbg.DevTools);
                };

                await dbg.DevTools.SendMethod("Page.enable");
                await dbg.DevTools.ReloadPage();

                RiotClient.Window.SetupWindow(dbg);
            });

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