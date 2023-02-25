using System;
using System.Diagnostics;
using System.IO;

namespace LeagueLoader.Main
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

        public static void RemoveAdminPerm(string path)
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = $"/c echo Y| cacls \"{path}\" /grant \"{Environment.UserName}\":f",
                    UseShellExecute = true,
                    CreateNoWindow = true,
                    Verb = "runas",
                    WindowStyle = ProcessWindowStyle.Hidden
                });
            }
            catch { }
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
    }
}