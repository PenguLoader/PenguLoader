using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace LeagueLoader.Main
{
    static class DataStore
    {
        public static void Dump()
        {
            try
            {
                var bytes = File.ReadAllBytes(Config.DataStorePath);
                Transform(bytes);

                var text = Encoding.UTF8.GetString(bytes);
                var output = Config.DataStorePath + ".d";
                File.WriteAllText(output, text);

                Process.Start("notepad.exe", $"\"{output}\"");
            }
            catch
            {
            }
        }

        static void Transform(byte[] bytes)
        {
            if (bytes == null || bytes.Length == 0)
                return;

            const string key = "A5dgY6lz9fpG9kGNiH1mZ";

            for (int i = 0; i < bytes.Length; i++)
            {
                bytes[i] = (byte)(bytes[i] ^ (byte)key[i % key.Length]);
            }
        }
    }
}