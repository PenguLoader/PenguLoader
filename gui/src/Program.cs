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
            using (Mutex mutex = new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out createdNew))
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
