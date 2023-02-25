using System;
using System.Linq;
using System.Threading;
using System.Windows;
using LeagueLoader.Main;

namespace LeagueLoader
{
    public static class Program
    {
        public const string NAME = "League Loader";
        public static string HOMEPAGE_URL => "https://leagueloader.app";
        public static string GITHUB_URL => "https://github.com/nomi-san/league-loader";
        public static string GITHUB_ISSUES_URL => "https://github.com/nomi-san/league-loader/issues";

        [STAThread]
        static int Main(string[] args)
        {
            bool isUninstall = args.Contains("--uninstall");

            bool createdNew = true;
            using (var mutex = new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out createdNew))
            {
                if (createdNew && !isUninstall)
                {
                    Updater.CheckUpdate();

                    App.Main();
                    return 0;
                }
                else if (isUninstall)
                {
                    if (!createdNew || Module.IsLoaded())
                    {
                        MessageBox.Show("Please close running League Client and League Loader before uninstalling it.",
                            NAME, MessageBoxButton.OK, MessageBoxImage.Information);
                        return -1;
                    }

                    Module.Deactivate();
                }

                return 0;
            }
        }

        static Program()
        {
            CosturaUtility.Initialize();
        }
    }
}