using System;
using System.Diagnostics;
using System.IO;

namespace PenguLoader.Main
{
    static class DataStore
    {
        public static void Dump()
        {
            try
            {
                var output = Config.DataStorePath + ".d";
                var bytes = File.ReadAllBytes(Config.DataStorePath);
                File.WriteAllBytes(output, Transform(bytes));

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

        static byte[] Transform(byte[] bytes)
        {
            if (bytes != null && bytes.Length > 0)
            {
                const string key = "A5dgY6lz9fpG9kGNiH1mZ";

                for (int i = 0; i < bytes.Length; i++)
                {
                    bytes[i] ^= (byte)key[i % key.Length];
                }
            }

            return bytes;
        }
    }
}