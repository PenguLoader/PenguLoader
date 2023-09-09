using System;
using System.Diagnostics;
using System.IO;
using System.Windows;

namespace PenguLoader.Main
{
    internal static class DataStore
    {
        public static bool IsDataStore(string path)
        {
            if (string.IsNullOrEmpty(path))
                return false;

            return Path.GetFileName(path).Equals("datastore", StringComparison.OrdinalIgnoreCase)
                   && File.Exists(path);
        }

        public static void DumpDataStore(string path)
        {
            try
            {
                var output = Path.GetTempFileName();
                var bytes = File.ReadAllBytes(path);

                if (bytes.Length == 0)
                {
                    MessageBox.Show("Your DataStore is empty!",
                        Program.Name, MessageBoxButton.OK, MessageBoxImage.Information);
                    return;
                }

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
                MessageBox.Show("Failed to dump your DataStore.\nPlease make sure the input is correct.",
                    Program.Name, MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private static byte[] Transform(byte[] bytes)
        {
            if (bytes == null || bytes.Length <= 0) return bytes;
            const string key = "A5dgY6lz9fpG9kGNiH1mZ";

            for (var i = 0; i < bytes.Length; i++) bytes[i] ^= (byte)key[i % key.Length];

            return bytes;
        }
    }
}