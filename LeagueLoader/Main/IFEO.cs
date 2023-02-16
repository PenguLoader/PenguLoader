using System;
using Microsoft.Win32;

namespace LeagueLoader.Main
{
    static class IFEO
    {
        const string IFEO_PATH = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options";
        const string VALUE_NAME = "Debugger";
        const string BACKUP_NAME = "OldDebugger";

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

                    var prevValue = image.GetValue(VALUE_NAME, null);
                    if (prevValue != null && prevValue is string)
                        if (!debugger.Equals(prevValue as string, StringComparison.OrdinalIgnoreCase))
                            image.SetValue(BACKUP_NAME, prevValue, RegistryValueKind.String);

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

                    var oldValue = image.GetValue(BACKUP_NAME, null);
                    if (oldValue != null && oldValue is string)
                    {
                        image.SetValue(VALUE_NAME, oldValue, RegistryValueKind.String);
                        image.DeleteValue(BACKUP_NAME);
                    }
                    else
                    {
                        image.DeleteValue(VALUE_NAME);
                        key.DeleteSubKey(target);
                    }

                    image.Dispose();
                }
        }
    }
}