using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace LeagueLoader
{
    internal class Dll
    {
        const string NAME = "d3d9.dll";
        static string ThisPath => Path.Combine(Directory.GetCurrentDirectory(), NAME);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
        static extern IntPtr OpenEvent(uint dwDesiredAccess, [MarshalAs(UnmanagedType.I1)] bool bInheritHandle, string lpName);

        [DllImport("kernel32.dll")]
        [return: MarshalAs(UnmanagedType.I1)]
        static extern bool SetEvent(IntPtr hEvent);

        [DllImport("kernel32.dll")]
        [return: MarshalAs(UnmanagedType.I1)]
        static extern bool CloseHandle(IntPtr handle);

        const uint EVENT_MODIFY_STATE = 0x0002;

        public static void OpenDevTools(bool remote)
        {
            var eventName = "Global\\LeagueLoader.Open"
                + (remote ? "RemoteDevTools" : "DevTools");

            var hEvent = OpenEvent(EVENT_MODIFY_STATE, false, eventName);
            if (hEvent != IntPtr.Zero)
            {
                SetEvent(hEvent);
                CloseHandle(hEvent);
            }
        }

        public static bool IsLoaded()
        {
            var procs = Process.GetProcessesByName("LeagueClientUx");
            if (procs.Length > 0)
            {
                var lcux = procs[0];
                var lcuxDir = Directory.GetParent(lcux.MainModule.FileName).FullName;
                var dllPath = Path.Combine(lcuxDir, NAME);

                foreach (ProcessModule module in lcux.Modules)
                {
                    if (string.Equals(module.FileName, dllPath, StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        public static bool IsInstalled(string lcDir)
        {
            if (string.IsNullOrEmpty(lcDir))
                return false;

            if (!File.Exists(ThisPath))
                return false;

            var linkPath = Path.Combine(lcDir, NAME);
            if (!File.Exists(linkPath))
                return false;

            linkPath = Symlink.Resolve(linkPath);
            if (!File.Exists(linkPath))
                return false;

            return NormalizePath(linkPath) == NormalizePath(ThisPath);
        }

        public static void Install(string lcDir)
        {
            var linkPath = Path.Combine(lcDir, NAME);
            if (File.Exists(linkPath))
                File.Delete(linkPath);

            Symlink.Create(linkPath, ThisPath);
        }

        public static void Uninstall(string lcDir)
        {
            if (IsInstalled(lcDir))
            {
                var linkPath = Path.Combine(lcDir, NAME);
                File.Delete(linkPath);
            }
        }

        static string NormalizePath(string path)
        {
            return Path.GetFullPath(new Uri(path).LocalPath)
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
                .ToUpperInvariant();
        }
    }
}