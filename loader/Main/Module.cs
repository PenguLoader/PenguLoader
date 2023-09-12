using System;
using System.IO;

namespace PenguLoader.Main
{
    static class Module
    {
        const string ModuleName = "core.dll";
        const string TargetName = "LeagueClientUx.exe";
        static string ModulePath => Path.Combine(Directory.GetCurrentDirectory(), ModuleName);
        static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";

        public static bool IsLoaded => Utils.IsFileInUse(ModulePath);

        public static bool IsActivated => DebuggerValue.Equals(IFEO.GetDebugger(TargetName), StringComparison.OrdinalIgnoreCase);

        public static bool IsFound => File.Exists(ModulePath);

        public static void SetActive(bool active)
        {
            if (active)
            {
                if (IsFound)
                {
                    IFEO.SetDebugger(TargetName, DebuggerValue);
                }
            }
            else
            {
                IFEO.RemoveDebugger(TargetName);
            }
        }
    }
}