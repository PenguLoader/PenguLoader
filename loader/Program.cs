using System;
using System.Threading;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader
{
    public static partial class Program
    {
        public static string Name => "Pengu Loader";
        public static string HomePageUrl => "https://pengu.lol";
        public static string DiscordUrl => "https://chat.pengu.lol";
        public static string GithubRepo => "PenguLoader/PenguLoader";
        public static string GithubUrl => $"https://github.com/{GithubRepo}";
        public static string GithubIssuesUrl => $"https://github.com/{GithubRepo}/issues";

        [STAThread]
        static int Main(string[] args)
        {
            bool isUninstall = false;

            if (args.Length > 0)
            {
                if (DataStore.IsDataStore(args[0]))
                {
                    DataStore.DumpDataStore(args[0]);
                    return 0;
                }
                else if (args[0].Equals("--uninstall"))
                {
                    isUninstall = true;
                }
            }

            using (var mutex = new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out var createdNew))
            {
                return isUninstall ? HandleUninstall(createdNew) : RunApplication(createdNew);
            }
        }

        static int RunApplication(bool createdNew)
        {
            if (!createdNew)
            {
                Native.SetFocusToPreviousInstance();
                return 0;
            }

            if (!Environment.Is64BitOperatingSystem)
            {
                MessageBox.Show("32-BIT CLIENT DEPRECATION\n\n" +
                    "Starting with LoL patch 13.8, 32-bit Windows is no longer supported. Please upgrade your Windows to 64-bit.",
                    Name, MessageBoxButton.OK, MessageBoxImage.Warning);
                return 1;
            }

            App.Main();
            return 0;
        }

        static int HandleUninstall(bool createdNew)
        {
            if (!createdNew || Module.IsLoaded)
            {
                MessageBox.Show("Please close the running League Client and Loader menu before uninstalling it.",
                    Name, MessageBoxButton.OK, MessageBoxImage.Information);
                return -1;
            }

            Module.SetActive(false);
            return 0;
        }

        static Program()
        {
            CosturaUtility.Initialize();
        }
    }
}