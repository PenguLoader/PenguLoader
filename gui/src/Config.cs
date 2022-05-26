using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace LeagueLoader
{
    class Config
    {
        public static string LeaguePath
        {
            get => Get("LeaguePath");
            set => Set("LeaguePath", value);
        }

        public static bool DisableWebSecurity
        {
            get
            {
                var value = Get("DisableWebSecurity");
                return !string.IsNullOrEmpty(value) && value != "0";
            }
            set => Set("DisableWebSecurity", value ? "1" : "0");
        }

        public static bool IgnoreCertificateErrors
        {
            get
            {
                var value = Get("IgnoreCertificateErrors");
                return !string.IsNullOrEmpty(value) && value != "0";
            }
            set => Set("IgnoreCertificateErrors", value ? "1" : "0");
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