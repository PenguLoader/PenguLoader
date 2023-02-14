using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace LeagueLoader
{
    internal class Module
    {
        const string NAME = "d3d9.dll";
        static string ThisPath => Path.Combine(Directory.GetCurrentDirectory(), NAME);

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        static extern IntPtr FindWindow(string classn, IntPtr name);

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool PostMessage(IntPtr hwnd, int msg, IntPtr wp, IntPtr lp);

        public static void OpenDevTools(bool remote)
        {
            var procs = Process.GetProcessesByName("LeagueClientUx");
            foreach (var proc in procs)
            {
                var msg = FindWindow("LL.MSG." + proc.Id, IntPtr.Zero);
                if (msg != IntPtr.Zero)
                {
                    PostMessage(msg, 0x8000 + 0x101, (IntPtr)(remote ? 1 : 0), IntPtr.Zero);
                    return;
                }
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