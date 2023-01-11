using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace LeagueLoader
{
    internal class Dll
    {
        static string D3d9DllPath => Path.Combine(Directory.GetCurrentDirectory(), "d3d9.dll");

        [DllImport("kernel32.dll", EntryPoint = "CreateSymbolicLinkW", CharSet = CharSet.Unicode)]
        static extern int CreateSymbolicLink(string lpSymlinkFileName, string lpTargetFileName, uint dwFlags = 0);

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

        public static bool Exist()
        {
            return File.Exists(D3d9DllPath);
        }

        public static bool IsLoaded()
        {
            var procs = Process.GetProcessesByName("LeagueClientUx");
            if (procs.Length > 0)
            {
                var lcux = procs[0];
                var lcuxDir = Directory.GetParent(lcux.MainModule.FileName).FullName;
                var dllPath = Path.Combine(lcuxDir, "d3d9.dll");

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
            if (string.IsNullOrEmpty(lcDir)) return false;

            var linkPath = Path.Combine(lcDir, "d3d9.dll");
            if (!File.Exists(linkPath)) return false;

            var info = new FileInfo(linkPath);
            return info.Attributes.HasFlag(FileAttributes.ReparsePoint);
        }

        public static void Install(string lcDir)
        {
            if (!IsInstalled(lcDir))
            {
                var linkPath = Path.Combine(lcDir, "d3d9.dll");
                CreateSymbolicLink(linkPath, D3d9DllPath);
            }
        }

        public static void Uninstall(string lcDir)
        {
            if (IsInstalled(lcDir))
            {
                var linkPath = Path.Combine(lcDir, "d3d9.dll");
                File.Delete(linkPath);
            }
        }
    }
}