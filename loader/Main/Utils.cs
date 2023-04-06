using System;
using System.Diagnostics;
using System.IO;

namespace PenguLoader.Main
{
    static class Utils
    {
        public static void OpenFolder(string path)
        {
            Process.Start(new ProcessStartInfo()
            {
                FileName = "explorer.exe",
                Arguments = $"\"{path}\"",
                UseShellExecute = true
            });
        }

        public static void OpenLink(string url)
        {
            Process.Start(url);
        }

        public static bool IsFileInUse(string path)
        {
            try
            {
                if (!File.Exists(path))
                    return false;

                using (new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                {
                    return false;
                }
            }
            catch
            {
                return true;
            }
        }

        public static void DeletePath(string path, bool isDir = false)
        {
            try
            {
                if (isDir && Directory.Exists(path))
                    Directory.Delete(path, true);
                else if (File.Exists(path))
                    File.Delete(path);
            }
            catch { }
        }
    }
}