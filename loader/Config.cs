using System;
using System.IO;
using CsToml;

namespace Pengu.Loader
{
    [TomlSerializedObject]
    partial class Config
    {
        public static Config I { get; } = new();
        static string _path = Path.Join(UserDir, "config");

        public static string UserDir => Path.Join(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "pengu_loader");

        static Config()
        {
            if (File.Exists(_path))
            {
                try
                {
                    using var fs = File.Open(_path, FileMode.Open, FileAccess.Read);
                    I = CsTomlSerializer.Deserialize<Config>(fs);

                    return;
                }
                catch (Exception ex)
                {
                    Logger.Debug("Failed to load config, using defaults. Exception: {0}", ex);
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
                using var mem = CsTomlSerializer.Serialize(I);
                File.WriteAllBytes(_path, mem.ByteSpan);
            }
            catch (Exception ex)
            {
                Logger.Debug("Failed to save config. Exception: {0}", ex);
            }
        }

        /// APP CONFIG



        /// RIOT CLIENT CONFIG

        [TomlValueOnSerialized]
        public bool riot_potato_mode { get; set; } = false;

        [TomlValueOnSerialized]
        public bool riot_disable_sentry { get; set; } = false;


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