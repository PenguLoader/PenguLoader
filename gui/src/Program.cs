using System;
using System.Threading;
using System.Windows.Forms;

namespace LeagueLoader
{
    static class Program
    {
        [STAThread]
        static void Main()
        {
            bool createdNew = true;
            using (Mutex mutex = new Mutex(true, "LeagueLoader", out createdNew))
            {
                if (createdNew)
                {
                    Application.EnableVisualStyles();
                    Application.SetCompatibleTextRenderingDefault(false);
                    Application.Run(new GUI());
                }
            }
        }
    }
}
