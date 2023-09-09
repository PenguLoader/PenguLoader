using System;
using System.IO;
using System.Text;
using MadMilkman.Ini;

namespace PenguLoader.Main
{
    internal static class Config
    {
        private static readonly IniFile Ini;

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

        private static string ConfigPath => GetPath("config");
        private static string DataStorePath => GetPath("datastore");
        private static string AssetsDir => GetPath("assets");
        public static string PluginsDir => GetPath("plugins");

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
            var main = Ini.Sections["Main"] ?? Ini.Sections.Add("Main");

            var k = main.Keys[key] ?? main.Keys.Add(key);

            k.Value = value;
            Ini.Save(ConfigPath);
        }

        private static bool GetBool(string key, bool @default)
        {
            var value = Get(key);
            switch (value)
            {
                case "true":
                case "1":
                    return true;
                case "false":
                case "0":
                    return false;
                default:
                    return @default;
            }
        }

        private static void SetBool(string key, bool value)
        {
            Set(key, value ? "true" : "false");
        }
    }
}