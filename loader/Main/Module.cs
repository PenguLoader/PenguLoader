using System;
using System.IO;

namespace PenguLoader.Main
{
    static class Module
    {
        private static string ModuleName => Environment.Is64BitOperatingSystem ? "core.dll" : "core32.dll";
        private static string TargetName => "LeagueClientUx.exe";
        private static string ModulePath => Path.Combine(Directory.GetCurrentDirectory(), ModuleName);
        private static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";

        public static bool IsLoaded() => Utils.IsFileInUse(ModulePath);

        public static bool IsActivated() => DebuggerValue.Equals(IFEO.GetDebugger(TargetName), StringComparison.OrdinalIgnoreCase);

        public static bool Exists() => File.Exists(ModulePath);

        public static bool Activate() => Exists() && IFEO.SetDebugger(TargetName, DebuggerValue);

        public static void Deactivate() => IFEO.RemoveDebugger(TargetName);
    }
}