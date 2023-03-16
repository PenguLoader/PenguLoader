using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace PenguLoader.Main
{
    static class Config
    {
        public static string ConfigPath => Path.Combine(Directory.GetCurrentDirectory(), "config");
        public static string DataStorePath => Path.Combine(Directory.GetCurrentDirectory(), "datastore");
        public static string AssetsDir => Path.Combine(Directory.GetCurrentDirectory(), "assets");
        public static string PluginsDir => Path.Combine(Directory.GetCurrentDirectory(), "plugins");

        static Config()
        {
            if (!Directory.Exists(AssetsDir))
                Directory.CreateDirectory(AssetsDir);

            if (!Directory.Exists(PluginsDir))
                Directory.CreateDirectory(PluginsDir);

            if (!File.Exists(ConfigPath))
                File.Create(ConfigPath).Close();

            if (!File.Exists(DataStorePath))
                File.Create(DataStorePath).Close();

            Utils.RemoveAdminPerm(AssetsDir);
            Utils.RemoveAdminPerm(PluginsDir);
            Utils.RemoveAdminPerm(DataStorePath);
        }

        public static string Language
        {
            get => Get("Language");
            set => Set("Language", value);
        }

        public static int RemoteDebuggingPort
        {
            get
            {
                var port = 0;
                int.TryParse(Get("RemoteDebuggingPort"), out port);
                return port;
            }
            set => Set("RemoteDebuggingPort", value.ToString());
        }

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        static extern long WritePrivateProfileString(string section,
            string key, string value, string file);

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        static extern int GetPrivateProfileString(string section,
            string key, string @default, StringBuilder retval, int size, string file);

        static string Get(string key)
        {
            var value = new StringBuilder(2048);
            GetPrivateProfileString("Main", key, "", value, 2048, ConfigPath);
            return value.ToString();
        }

        static void Set(string key, string value)
        {
            WritePrivateProfileString("Main", key, value, ConfigPath);
        }
    }
}