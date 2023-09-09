﻿using System;
using System.Threading;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader
{
    public static partial class Program
    {
        static Program()
        {
            CosturaUtility.Initialize();
        }

        public static string Name => "Pengu Loader";
        public static string HomePageUrl => "https://pengu.lol";
        public static string DiscordUrl => "https://chat.pengu.lol";
        public static string GithubRepo => "PenguLoader/PenguLoader";
        public static string GithubUrl => $"https://github.com/{GithubRepo}";
        public static string GithubIssuesUrl => $"https://github.com/{GithubRepo}/issues";

        [STAThread]
        private static int Main(string[] args)
        {
            var arg = args.Length > 0 ? args[0] : null;

            if (DataStore.IsDataStore(arg))
            {
                DataStore.DumpDataStore(arg);
                return 0;
            }

            using (new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out var createdNew))
            {
                if (arg == null) return RunApplication(createdNew);
                switch (arg)
                {
                    case "--install":
                        return HandleInstall(createdNew, true);
                    case "--uninstall":
                        return HandleInstall(createdNew, false);
                }

                return RunApplication(createdNew);
            }
        }

        private static int RunApplication(bool createdNew)
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

        private static int HandleInstall(bool createdNew, bool active)
        {
            if (!createdNew || Module.IsLoaded)
            {
                var action = active ? "installing" : "uninstalling";
                MessageBox.Show($"Please close the running League Client and Loader menu before {action} it.",
                    Name, MessageBoxButton.OK, MessageBoxImage.Information);
                return -1;
            }

            Module.SetActive(active);
            return 0;
        }
    }
}