using System;
using System.IO;

namespace Pengu.Loader.Core
{
    internal class Module
    {
        private static string ModuleName => "core.dll";
        private static string TargetName => "LeagueClientUx.exe";
        private static string ModulePath => Path.Combine(Config.BaseDir, ModuleName);
        private static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";

        public static bool IsFound()
        { 
            return File.Exists(ModulePath);
        }

        public static bool IsLoaded()
        {
            var path = ModulePath;
            if (!File.Exists(path)) return false;
            try
            {
                using var _ = new FileStream(path, FileMode.Open,
                    FileAccess.ReadWrite, FileShare.None);
            }
            catch
            {
                return true;
            }
            return false;
        }

        public static bool IsActive()
        {
            var param = IFEO.GetDebugger(TargetName);
            return DebuggerValue.Equals(param, StringComparison.OrdinalIgnoreCase);
        }

        /// <exception cref="Exception"></exception>
        /// <exception cref="InvalidOperationException"></exception>
        /// <exception cref="UnauthorizedAccessException"></exception>
        public static bool SetActive(bool active)
        {
            if (IsActive() == active)
                return true;

            if (active)
            {
                IFEO.SetDebugger(TargetName, DebuggerValue);
            }
            else
            {
                IFEO.RemoveDebugger(TargetName);
            }

            return IsActive() == active;
        }
    }
}