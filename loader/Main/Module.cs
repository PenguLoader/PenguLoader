using System;
using System.IO;

namespace PenguLoader.Main
{
    static class Module
    {
        private const string ModuleName = "core.dll";
        private const string TargetName = "LeagueClientUx.exe";
        private static readonly string ModulePath = Path.Combine(Directory.GetCurrentDirectory(), ModuleName);
        private static readonly string DebuggerValue = $"rundll32 \"{ModulePath}\", #6000 ";

        public static bool IsLoaded() => Utils.IsFileInUse(ModulePath);

        public static bool IsActivated()
        {
            var value = IFEO.GetDebugger(TargetName);
            return DebuggerValue.Equals(value, StringComparison.OrdinalIgnoreCase);
        }

        public static bool Exists() => File.Exists(ModulePath);

        public static bool Activate() => Exists() && IFEO.SetDebugger(TargetName, DebuggerValue);

        public static void Deactivate() => IFEO.RemoveDebugger(TargetName);
    }
}
