using System;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace PenguLoader.Main
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

                Process.Start(new ProcessStartInfo
                {
                    FileName = "notepad.exe",
                    Arguments = $"\"{output}\"",
                    UseShellExecute = false
                });
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