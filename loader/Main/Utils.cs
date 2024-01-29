using System;
using System.Diagnostics;
using System.IO;
using System.Security.Principal;

namespace PenguLoader.Main
{
    static class Utils
    {
        public static void OpenFolder(string path) => Process.Start("explorer.exe", $"\"{path}\"");

        public static void OpenLink(string url) => Process.Start(new ProcessStartInfo(url) { UseShellExecute = true });

        public static bool IsFileInUse(string path)
        {
            if (!File.Exists(path)) return false;

            try
            {
                using (new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None)) { }
            }
            catch
            {
                return true;
            }

            return false;
        }

        public static void DeletePath(string path, bool isDir = false)
        {
            if (isDir ? Directory.Exists(path) : File.Exists(path))
            {
                try
                {
                    if (isDir) Directory.Delete(path, true);
                    else File.Delete(path);
                }
                catch
                {
                }
            }
        }

        public static void EnsureDirectoryExists(string path)
        {
            if (!Directory.Exists(path))
                Directory.CreateDirectory(path);
        }

        public static void EnsureFileExists(string path)
        {
            if (!File.Exists(path))
                File.Create(path).Close();
        }

        public static string NormalizePath(string path)
        {
            return Path.GetFullPath(new Uri(path).LocalPath)
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
                .ToUpperInvariant();
        }

        public static bool IsAdmin()
        {
            bool isElevated;
            using (var identity = WindowsIdentity.GetCurrent())
            {
                var principal = new WindowsPrincipal(identity);
                isElevated = principal.IsInRole(WindowsBuiltInRole.Administrator);
            }

            return isElevated;
        }
    }
}