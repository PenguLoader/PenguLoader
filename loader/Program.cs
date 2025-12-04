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

                RiotClient.Window.SetupWindow(dbg);

                await dbg.Connect();

                //await Debugger.DevTools.SetBypassCSP(true);
                //await Debugger.DevTools.ReloadPage(true);
                //await InjectScripts(Debugger.DevTools);
            });

            return 0;
        }
    }
}