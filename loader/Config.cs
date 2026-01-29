using System;
using System.Collections.Generic;
using System.IO;
using CsToml;

namespace Pengu.Loader
{
    [TomlSerializedObject(NamingConvention = TomlNamingConvention.None)]
    partial class Config
    {
        public static Config I { get; } = new();
        static string _path = Path.Join(UserDir, "config.toml");

        public static string UserDir => Path.Join(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "pengu_loader");

        public static string BaseDir
            => Environment.GetEnvironmentVariable("LOADER_BASE_DIR")
            ?? AppContext.BaseDirectory;

        public static string PluginsDir
            => !string.IsNullOrEmpty(I.plugins_dir)
            ? Path.GetFullPath(I.plugins_dir, BaseDir)
            : Path.Join(UserDir, "plugins");

        static Config()
        {
            if (File.Exists(_path))
            {
                try
                {
                    using var fs = File.OpenRead(_path);
                    I = CsTomlSerializer.Deserialize<Config>(fs);

                    return;
                }
                catch (Exception ex)
                {
                    Log.Debug("Failed to load config, using defaults. {0}", ex.Message);
                }
            }
        }

        public static void Load()
        {
        }

        public static void Save()
        {
            var userDir = UserDir;
            if (!Directory.Exists(userDir))
                Directory.CreateDirectory(userDir);

            try
            {
                using var fs = File.OpenWrite(_path);
                CsTomlSerializer.Serialize(fs, I);
            }
            catch (Exception ex)
            {
                Log.Debug("Failed to save config. Exception: {0}", ex);
            }
        }


        /// APP CONFIG

        [TomlValueOnSerialized(NullHandling = TomlNullHandling.Ignore)]
        public string? plugins_dir { get; set; }

        [TomlValueOnSerialized(NullHandling = TomlNullHandling.Ignore)]
        public HashSet<string>? disabled_plugins { get; set; }


        /// RIOT CLIENT CONFIG

        [TomlValueOnSerialized]
        public bool riot_potato_mode { get; set; } = false;

        [TomlValueOnSerialized]
        public bool riot_disable_sentry { get; set; } = false;

        [TomlValueOnSerialized]
        public string? riot_theme { get; set; }

        /// LEAGUE CLIENT CONFIG

        [TomlValueOnSerialized]
        public bool optimized_client { get; set; } = true;

        [TomlValueOnSerialized]
        public bool super_potato { get; set; } = false;

        [TomlValueOnSerialized]
        public bool use_hotkey { get; set; } = true;

        [TomlValueOnSerialized]
        public bool use_devtools { get; set; } = false;

        [TomlValueOnSerialized]
        public bool insecure_mode { get; set; } = false;

        [TomlValueOnSerialized]
        public bool use_riotclient { get; set; } = false;

        [TomlValueOnSerialized]
        public bool use_proxy { get; set; } = false;
    }
}