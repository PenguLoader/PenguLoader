using System;
using Microsoft.Win32;

namespace PenguLoader.Main
{
    static class IFEO
    {
        private const string IFEO_PATH = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options";
        private const string VALUE_NAME = "Debugger";

        public static string GetDebugger(string target)
        {
            using (var key = OpenIFEOKey())
            {
                using (var image = key?.OpenSubKey(target))
                {
                    return image?.GetValue(VALUE_NAME) as string;
                }
            }
        }

        public static bool SetDebugger(string target, string debugger)
        {
            using (var key = OpenIFEOKey(writable: true))
            {
                if (key == null) return false;

                using (var image = key.OpenSubKey(target, true) ?? key.CreateSubKey(target))
                {
                    if (image == null) return false;
                    image.SetValue(VALUE_NAME, debugger, RegistryValueKind.String);
                }
            }

            return true;
        }

        public static void RemoveDebugger(string target)
        {
            using (var key = OpenIFEOKey(writable: true))
            using (var image = key?.OpenSubKey(target, true))
            {
                if (image == null) return;

                image.DeleteValue(VALUE_NAME);
                key.DeleteSubKey(target);
            }
        }

        private static RegistryKey OpenIFEOKey(bool writable = false)
        {
            return Registry.LocalMachine.OpenSubKey(IFEO_PATH, writable);
        }
    }
}