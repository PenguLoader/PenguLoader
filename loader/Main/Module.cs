using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace LeagueLoader.Main
{
    static class Module
    {
        const string MODULE_NAME = "core.dll";
        const string TARGET_NAME = "LeagueClientUx.exe";

        static string ModulePath => Path.Combine(Directory.GetCurrentDirectory(), MODULE_NAME);
        static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";

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
            return Utils.IsFileInUse(ModulePath);
        }

        public static bool IsActivated()
        {
            var value = IFEO.GetDebugger(TARGET_NAME);
            return DebuggerValue.Equals(value, StringComparison.OrdinalIgnoreCase);
        }

        public static bool Activate()
        {
            if (!File.Exists(ModulePath))
                return false;

            return IFEO.SetDebugger(TARGET_NAME, DebuggerValue);
        }

        public static void Deactivate()
        {
            IFEO.RemoveDegubber(TARGET_NAME);
        }
    }
}