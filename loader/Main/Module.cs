using System;
using System.IO;

namespace PenguLoader.Main
{
    static class Module
    {
        const string ModuleName = "core.dll";
        const string TargetName = "LeagueClientUx.exe";
        const string SymlinkName = "version.dll";

        static string ModulePath => Path.Combine(Environment.CurrentDirectory, ModuleName);
        static string DebuggerValue => $"rundll32 \"{ModulePath}\", #6000 ";
        static string SymlinkPath => Path.Combine(Config.LeaguePath, SymlinkName);

        public static bool UseSymlink { get; private set; } = false;

        static Module()
        {
            if (Config.UseSymlink)
            {
                UseSymlink = true;
                return;
            }

            try
            {
                // uncomment it to test
                //throw new UnauthorizedAccessException();

                var old = IFEO.GetDebugger(TargetName);
                if (old != null)
                {
                    IFEO.RemoveDebugger(TargetName);
                    IFEO.SetDebugger(TargetName, old);
                }
                else if (Symlink.IsSymbolic(SymlinkPath)) 
                {
                    //if  can create  "LeagueClientUx.exe" but not "Debugger" because of UnauthorizedAccessException
                    //already use SymlinkMode
                    SymlinkMode = true;
                }
            }
            catch (UnauthorizedAccessException)
            {
                UseSymlink = true;
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
                if (UseSymlink)
                {
                    var lcPath = Config.LeaguePath;
                    if (!LCU.IsValidDir(lcPath)) return false;

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

        public static void SetActive(bool active)
        {
            if (UseSymlink)
            {
                var path = SymlinkPath;
                Utils.DeletePath(path);

                if (active)
                {
                    Symlink.Create(path, ModulePath);
                }
            }
            else
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
}
