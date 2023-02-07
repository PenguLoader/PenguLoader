using System;
using System.Linq;
using System.Threading;
using System.Windows.Forms;

namespace LeagueLoader
{
    static class Program
    {
        public const string NAME = "League Loader";

        [STAThread]
        static int Main(string[] args)
        {
            bool isUninstall = args.Contains("--uninstall");

            bool createdNew = true;
            using (Mutex mutex = new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out createdNew))
            {
                if (createdNew && !isUninstall)
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
                                $"{NAME} Update", MessageBoxButtons.YesNo, MessageBoxIcon.Information);

                            if (ret == DialogResult.Yes)
                                Updater.OpenDownload();
                        }
                    });

                    Application.Run(new GUI());
                    return 0;
                }
                else if (isUninstall)
                {
                    if (!createdNew || Dll.IsLoaded())
                    {
                        MessageBox.Show("Please close running League Client and League Loader before uninstalling it.",
                            NAME, MessageBoxButtons.OK, MessageBoxIcon.Information);
                        return -1;
                    }

                    Dll.Uninstall(Config.LeaguePath);
                }

                return 0;
            }
        }
    }
}