using System;
using System.IO;

namespace PenguLoader.Main
{
    internal static class Module
    {
        private const string ModuleName = "core.dll";
        private const string TargetName = "LeagueClientUx.exe";
        private static string ModulePath => Path.Combine(Directory.GetCurrentDirectory(), ModuleName);
        private static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";

        public static bool IsLoaded => Utils.IsFileInUse(ModulePath);

        public static bool IsActivated =>
            DebuggerValue.Equals(Ifeo.GetDebugger(TargetName), StringComparison.OrdinalIgnoreCase);

        public static bool IsFound => File.Exists(ModulePath);

        public static void SetActive(bool active)
        {
            if (active)
            {
                if (IsFound) Ifeo.SetDebugger(TargetName, DebuggerValue);
            }
            else
            {
                Ifeo.RemoveDebugger(TargetName);
            }
        }
    }
}