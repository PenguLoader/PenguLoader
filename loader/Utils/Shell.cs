using System;
using System.IO;
using System.Diagnostics;
using Microsoft.Win32;

#pragma warning disable CS8600
#pragma warning disable CA1416

namespace Pengu.Loader.Utils
{
    static class Shell
    {
        public static void OpenUrl(string url)
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = $"/c start \"open\" \"{url}\"",
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden,
                UseShellExecute = false,
            });
        }

        public static void OpenUrlAsBrowserApp(string url)
        {
            var chromiumExe = FindChromiumExePath();
            if (!string.IsNullOrEmpty(chromiumExe))
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = chromiumExe,
                    Arguments = $"\"--app={url}\" --new-window",
                    UseShellExecute = true,
                });
            }
            else
            {
                OpenUrl(url);
            }
        }

        static string? FindChromiumExePath()
        {
            var appKeys = new string[]
            {
                @"\Software\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe",
                @"\Software\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe",
            };

            foreach (var appKey in appKeys)
            {
                var path = (string)(Registry.GetValue("HKEY_LOCAL_MACHINE" + appKey, "", null)
                    ?? Registry.GetValue("HKEY_CURRENT_USER" + appKey, "", null));

                if (path != null && File.Exists(path))
                    return path;
            }

            return null;
        }
    }
}