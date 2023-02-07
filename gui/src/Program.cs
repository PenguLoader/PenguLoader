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

                    Updater.CheckUpdate().ContinueWith(async res =>
                    {
                        var update = await res;
                        if (update != null)
                        {
                            var ret = MessageBox.Show(
                                $"New version available, v{update.Version}!\n\n" +
                                "Update changes:\n\n" + (string.IsNullOrEmpty(update.Changes) ? "- None" : update.Changes) +
                                "\n\nDo you want to download it now?",
                                "League Loader Update", MessageBoxButtons.YesNo, MessageBoxIcon.Information);

                            if (ret == DialogResult.Yes)
                                Updater.OpenDownload();
                        }
                    });

                    Application.Run(new GUI());
                }
            }
        }
    }
}
