using System;
using System.Linq;
using System.Threading;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader
{
    public static class Program
    {
        public const string Name = "Pengu Loader";
        public static string HomepageUrl => "https://pengu.lol";
        public static string DiscordUrl => "https://chat.pengu.lol";
        public static string GithubRepo => "PenguLoader/PenguLoader";
        public static string GithubUrl => $"https://github.com/{GithubRepo}";
        public static string GithubIssuesUrl => $"https://github.com/{GithubRepo}/issues";

        [STAThread]
        static int Main(string[] args)
        {
            bool isUninstall = args.Contains("--uninstall");
            bool createdNew;

            using (var mutex = new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out createdNew))
            {
                if (isUninstall)
                {
                    return HandleUninstall(createdNew);
                }
                else
                {
                    return RunApplication(createdNew);
                }
            }
        }

        private static int RunApplication(bool createdNew)
        {
            if (createdNew)
            {
                App.Main();
                return 0;
            }
            else
            {
                Native.SetFocusToPreviousInstance(Name);
                return 0;
            }
        }

        private static int HandleUninstall(bool createdNew)
        {
            if (!createdNew || Module.IsLoaded())
            {
                MessageBox.Show("Please close the running League Client and Loader menu before uninstalling it.",
                    Name, MessageBoxButton.OK, MessageBoxImage.Information);
                return -1;
            }

            Module.Deactivate();
            return 0;
        }

        static Program()
        {
            CosturaUtility.Initialize();
        }
    }
}