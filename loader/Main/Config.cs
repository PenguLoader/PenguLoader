using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace PenguLoader.Main
{
    static class Config
    {
        public static string ConfigPath => GetPath("config");
        public static string DataStorePath => GetPath("datastore");
        public static string AssetsDir => GetPath("assets");
        public static string PluginsDir => GetPath("plugins");

        static Config()
        {
            EnsureDirectoryExists(AssetsDir);
            EnsureDirectoryExists(PluginsDir);
            EnsureFileExists(ConfigPath);
            EnsureFileExists(DataStorePath);
        }

        public static string Language
        {
            get => Get("Language");
            set => Set("Language", value);
        }

        public static int RemoteDebuggingPort
        {
            get => GetInt("RemoteDebuggingPort");
            set => Set("RemoteDebuggingPort", value.ToString());
        }

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        private static extern long WritePrivateProfileString(string section, string key, string value, string file);

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        private static extern int GetPrivateProfileString(string section, string key, string @default, StringBuilder retval, int size, string file);

        private static string GetPath(string folderName)
        {
            return Path.Combine(Directory.GetCurrentDirectory(), folderName);
        }

        private static void EnsureDirectoryExists(string path)
        {
            if (!Directory.Exists(path))
            {
                Directory.CreateDirectory(path);
            }
        }

        private static void EnsureFileExists(string path)
        {
            if (!File.Exists(path))
            {
                File.Create(path).Close();
            }
        }

        private static string Get(string key)
        {
            var value = new StringBuilder(2048);
            GetPrivateProfileString("Main", key, "", value, 2048, ConfigPath);
            return value.ToString();
        }

        private static void Set(string key, string value)
        {
            WritePrivateProfileString("Main", key, value, ConfigPath);
        }

        private static int GetInt(string key)
        {
            return int.TryParse(Get(key), out int result) ? result : 0;
        }
    }
}
