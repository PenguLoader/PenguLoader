using System;
using System.IO;

namespace PenguLoader.Main
{
    static class Module
    {
        private static string ModuleName => "core.dll";
        private static string TargetName => LCU.ClientUxProcessName;
        private static string ModulePath => Path.Combine(Directory.GetCurrentDirectory(), ModuleName);
        private static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";

        private static string SymlinkName => "version.dll";
        private static string SymlinkPath => Path.Combine(Config.LeaguePath, SymlinkName);

        static Module()
        {
            if (Config.UseSymlink)
                return;

            try
            {
                // uncomment it to test
                //throw new UnauthorizedAccessException();

                if (LCU.IsValidDir(Config.LeaguePath)
                    && Symlink.IsSymlink(SymlinkPath))
                {
                    Config.UseSymlink = true;
                }
                else
                {
                    var value = IFEO.GetDebugger(TargetName);
                    if (!string.IsNullOrEmpty(value))
                    {
                        IFEO.RemoveDebugger(TargetName);
                        IFEO.SetDebugger(TargetName, value);
                    }
                }
            }
            catch (UnauthorizedAccessException)
            {
                Config.UseSymlink = true;
            }
            catch
            {
                // TODO: handle some other errors
            }
        }

        public static bool IsFound => File.Exists(ModulePath);

        public static bool IsLoaded => Utils.IsFileInUse(ModulePath);

        public static bool IsActivated
        {
            get
            {
                if (Config.UseSymlink)
                {
                    if (!LCU.IsValidDir(Config.LeaguePath))
                        return false;

                    var resolved = Utils.NormalizePath(Symlink.Resolve(SymlinkPath));
                    var modulePath = Utils.NormalizePath(ModulePath);

                    return string.Compare(resolved, modulePath, false) == 0;
                }
                else
                {
                    var param = IFEO.GetDebugger(TargetName);
                    return DebuggerValue.Equals(param, StringComparison.OrdinalIgnoreCase);
                }
            }
        }

        public static bool SetActive(bool active)
        {
            if (IsActivated == active)
                return true;

            bool success;

            if (Config.UseSymlink)
            {
                var path = SymlinkPath;
                Utils.DeletePath(path);

                if (active)
                {
                    Symlink.Create(path, ModulePath);
                }

                success = true;
            }
            else
            {
                if (active)
                {
                    success = IFEO.SetDebugger(TargetName, DebuggerValue);
                    return success && IsActivated == true;
                }
                else
                {
                    success = true;
                    IFEO.RemoveDebugger(TargetName);
                }
            }

            return success && IsActivated == active;
        }
    }
}