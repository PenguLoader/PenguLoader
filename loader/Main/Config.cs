using System;
using System.IO;
using System.Text;

using MadMilkman.Ini;

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
            Utils.EnsureDirectoryExists(AssetsDir);
            Utils.EnsureDirectoryExists(PluginsDir);
            Utils.EnsureFileExists(ConfigPath);
            Utils.EnsureFileExists(DataStorePath);

            Ini = new IniFile(new IniOptions
            {
                Encoding = Encoding.UTF8
            });

            Ini.Load(ConfigPath);
        }

        public static string Language
        {
            get => Get("Language", "English");
            set => Set("Language", value);
        }

        public static bool OptimizeClient
        {
            get => GetBool("OptimizeClient", true);
            set => SetBool("OptimizeClient", value);
        }

        public static bool SuperLowSpecMode
        {
            get => GetBool("SuperLowSpecMode", false);
            set => SetBool("SuperLowSpecMode", value);
        }

        private static string GetPath(string folderName)
        {
            return Path.Combine(Environment.CurrentDirectory, folderName);
        }

        static IniFile Ini;

        private static string Get(string key, string @default = "")
        {
            try
            {
                return Ini.Sections["Main"].Keys[key].Value;
            }
            catch
            {
                return @default;
            }
        }

        private static void Set(string key, string value)
        {
            var main = Ini.Sections["Main"];
            if (main == null)
                main = Ini.Sections.Add("Main");

            var k = main.Keys[key];
            if (k == null)
                k = main.Keys.Add(key);

            k.Value = value;
            Ini.Save(ConfigPath);
        }

        static bool GetBool(string key, bool @default)
        {
            var value = Get(key);
            if (value == "true" || value == "1")
                return true;
            else if (value == "false" || value == "0")
                return false;

            return @default;
        }

        static void SetBool(string key, bool value) => Set(key, value ? "true" : "false");
    }
}