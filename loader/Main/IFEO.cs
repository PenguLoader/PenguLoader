using System;
using Microsoft.Win32;

namespace PenguLoader.Main
{
    static class IFEO
    {
        const string IFEO_PATH = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options";
        const string VALUE_NAME = "Debugger";

        public static string GetDebugger(string target)
        {
            using (var key = Registry.LocalMachine.OpenSubKey(IFEO_PATH))
                if (key != null)
                    using (var image = key.OpenSubKey(target))
                        if (image != null)
                            return image.GetValue(VALUE_NAME, null) as string;

            return null;
        }

        public static bool SetDebugger(string target, string debugger)
        {
            using (var key = Registry.LocalMachine.OpenSubKey(IFEO_PATH, true))
                if (key != null)
                {
                    var image = key.OpenSubKey(target, true);
                    if (image == null)
                    {
                        image = key.CreateSubKey(target);
                    }

                    image.SetValue(VALUE_NAME, debugger, RegistryValueKind.String);
                    image.Dispose();
                    return true;
                }

            return false;
        }

        public static void RemoveDegubber(string target)
        {
            using (var key = Registry.LocalMachine.OpenSubKey(IFEO_PATH, true))
                if (key != null)
                {
                    var image = key.OpenSubKey(target, true);
                    if (image == null) return;

                    image.DeleteValue(VALUE_NAME);
                    key.DeleteSubKey(target);

                    image.Dispose();
                }
        }
    }
}