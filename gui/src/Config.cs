using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace LeagueLoader
{
    internal class Config
    {
        public static string LeaguePath
        {
            get => Get("LeaguePath");
            set => Set("LeaguePath", value);
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

        public static void Init()
        {
            if (!File.Exists(IniPath))
            {
                File.Create(IniPath).Close();
            }
        }

        static readonly string IniPath = Path.Combine(Directory.GetCurrentDirectory(), "config.cfg");

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        private static extern long WritePrivateProfileString(string section,
            string key, string value, string file);

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        private static extern int GetPrivateProfileString(string section,
            string key, string @default, StringBuilder retval, int size, string file);

        static string Get(string key)
        {
            var retVal = new StringBuilder(2048);
            GetPrivateProfileString("Main", key, "", retVal, 2048, IniPath);
            return retVal.ToString();
        }

        static void Set(string key, string value)
        {
            WritePrivateProfileString("Main", key, value, IniPath);
        }
    }
}