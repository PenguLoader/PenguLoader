using System;
using System.IO;

namespace PenguLoader.Main
{
    static class Module
    {
        const string MODULE_NAME = "core.dll";
        const string TARGET_NAME = "LeagueClientUx.exe";

        static string ModulePath => Path.Combine(Directory.GetCurrentDirectory(), MODULE_NAME);
        static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";

        public static bool IsLoaded()
        {
            return Utils.IsFileInUse(ModulePath);
        }

        public static bool IsActivated()
        {
            var value = IFEO.GetDebugger(TARGET_NAME);
            return DebuggerValue.Equals(value, StringComparison.OrdinalIgnoreCase);
        }

        public static bool Exist()
        {
            return File.Exists(ModulePath);
        }

        public static bool Activate()
        {
            if (!Exist())
                return false;

            return IFEO.SetDebugger(TARGET_NAME, DebuggerValue);
        }

        public static void Deactivate()
        {
            IFEO.RemoveDegubber(TARGET_NAME);
        }
    }
}