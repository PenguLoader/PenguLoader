using System;
using System.Linq;
using System.Threading;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader
{
    public static class Program
    {
        public const string NAME = "Pengu Loader";
        public static string HOMEPAGE_URL => "https://pengu.lol";
        public static string DISCORD_URL => "https://chat.pengu.lol";
        public static string GITHUB_REPO => "PenguLoader/PenguLoader";
        public static string GITHUB_URL => $"https://github.com/{GITHUB_REPO}";
        public static string GITHUB_ISSUES_URL => $"https://github.com/{GITHUB_REPO}/issues";

        [STAThread]
        static int Main(string[] args)
        {
            bool isUninstall = args.Contains("--uninstall");

            bool createdNew = true;
            using (var mutex = new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out createdNew))
            {
                if (createdNew && !isUninstall)
                {
                    App.Main();
                    return 0;
                }
                else if (!createdNew && !isUninstall)
                {
                    Native.SetFocusToPreviousInstance(NAME);
                }
                else if (isUninstall)
                {
                    if (!createdNew || Module.IsLoaded())
                    {
                        MessageBox.Show("Please close the running League Client and Loader menu before uninstalling it.",
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