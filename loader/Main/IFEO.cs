using System;
using System.Security.AccessControl;
using Microsoft.Win32;

namespace PenguLoader.Main
{
    internal static class Ifeo
    {
        private const string IfeoPath = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options";
        private const string ValueName = "Debugger";

        public static string GetDebugger(string target)
        {
            using (var key = OpenIfeoKey())
            {
                using (var image = key?.OpenSubKey(target))
                {
                    return image?.GetValue(ValueName) as string;
                }
            }
        }

        public static void SetDebugger(string target, string debugger)
        {
            using (var key = OpenIfeoKey(true))
            {
                if (key == null) return;

                using (var image = key.CreateSubKey(target, RegistryKeyPermissionCheck.ReadWriteSubTree))
                {
                    if (image == null) return;

                    try
                    {
                        var user = Environment.UserDomainName + "\\" + Environment.UserName;
                        var rule = new RegistryAccessRule(user, RegistryRights.FullControl, AccessControlType.Allow);
                        var security = new RegistrySecurity();
                        security.AddAccessRule(rule);
                        image.SetAccessControl(security);
                    }
                    catch
                    {
                        // ignored
                    }

                    image.SetValue(ValueName, debugger, RegistryValueKind.String);
                }
            }
        }

        public static void RemoveDebugger(string target)
        {
            using (var key = OpenIfeoKey(true))
            {
                using (var image = key?.OpenSubKey(target, true))
                {
                    if (image == null) return;

                    image.DeleteValue(ValueName);
                    key.DeleteSubKey(target, false);
                }
            }
        }

        private static RegistryKey OpenIfeoKey(bool writable = false)
        {
            return Registry.LocalMachine.OpenSubKey(IfeoPath, writable);
        }
    }
}