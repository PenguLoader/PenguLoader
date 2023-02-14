using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace LeagueLoader
{
    internal class LCU
    {
        static HttpClient _client = new HttpClient();

        static LCU()
        {
            // Enable TLS 1.1/1.2
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;

            // Ignore invalid SSL certs.
            ServicePointManager.ServerCertificateValidationCallback += (a, b, c, d) => true;
        }

        public static bool IsOpened()
        {
            return Process.GetProcessesByName("LeagueClientUx").Length > 0;
        }

        public static bool IsValidDir(string path)
        {
            return File.Exists(Path.Combine(path, "LeagueClient.exe"));
        }

        public static string GetDir()
        {
            var procs = Process.GetProcessesByName("LeagueClientUx");

            if (procs.Length > 0)
            {
                var lcux = procs[0];
                return Directory.GetParent(lcux.MainModule.FileName).FullName;
            }

            return "";
        }

        public static async Task<string> Request(string api, string method, string body = null)
        {
            var dir = GetDir();

            if (!string.IsNullOrEmpty(dir))
            {
                var lockfilePath = Path.Combine(dir, "lockfile");

                if (File.Exists(lockfilePath))
                {
                    var lockfileData = LoadFile(lockfilePath);

                    if (!string.IsNullOrEmpty(lockfileData))
                    {
                        var tokens = lockfileData.Split(':');

                        if (tokens.Length >= 5)
                        {
                            var port = tokens[2];
                            var auth = tokens[3];

                            var uri = $"https://127.0.0.1:{port}{api}";
                            var authorization = Convert.ToBase64String(Encoding.ASCII.GetBytes("riot:" + auth));

                            try
                            {
                                var req = new HttpRequestMessage(new HttpMethod(method), uri);
                                req.Headers.Add("Authorization", $"Basic {authorization}");

                                if (!string.IsNullOrEmpty(body))
                                    req.Content = new StringContent(body, Encoding.UTF8, "application/json");

                                var res = await _client.SendAsync(req);
                                return await res.Content.ReadAsStringAsync();
                            }
                            catch
                            {
                            }
                        }
                    }
                }
            }

            return null;
        }

        public static void KillUxAndRestart()
        {
            Task.Run(() => Request("/riotclient/kill-and-restart-ux", "POST"));
        }

        static string LoadFile(string path)
        {
            try
            {
                using (FileStream fileStream = new FileStream(path,
                    FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    using (StreamReader reader = new StreamReader(fileStream))
                    {
                        return reader.ReadToEnd();
                    }
                }
            }
            catch
            {
                return "";
            }
        }
    }
}