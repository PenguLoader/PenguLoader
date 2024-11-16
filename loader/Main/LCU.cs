using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace PenguLoader.Main
{
    static class LCU
    {
        private static readonly HttpClient Http;

        static LCU()
        {
            Http = new HttpClient();
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;
            ServicePointManager.ServerCertificateValidationCallback += (a, b, c, d) => true;
        }

        public static bool IsRunning() => Process.GetProcessesByName("LeagueClientUx").Length > 0;

        public static string GetDir()
        {
            var procs = Process.GetProcessesByName("LeagueClientUx");

            if (procs.Length == 0) return "";

            var lcux = procs[0];
            return Directory.GetParent(lcux.MainModule.FileName).FullName;
        }

        public static async Task<string> Request(string api, string method, string body = null)
        {
            var lcPath = GetDir();

            if (string.IsNullOrEmpty(lcPath) || !GetCredentials(lcPath, out var port, out var pass))
                return null;

            var uri = $"https://127.0.0.1:{port}{api}";
            var authToken = Encoding.ASCII.GetBytes("riot:" + pass);
            var authorization = "Basic " + Convert.ToBase64String(authToken);

            try
            {
                using (var req = new HttpRequestMessage(new HttpMethod(method), uri))
                {
                    req.Headers.Add("Authorization", authorization);

                    if (!string.IsNullOrEmpty(body))
                        req.Content = new StringContent(body, Encoding.UTF8, "application/json");

                    using (var res = await Http.SendAsync(req))
                    {
                        return await res.Content.ReadAsStringAsync();
                    }
                }
            }
            catch
            {
                return null;
            }
        }

        public static Task KillUxAndRestart() => Request("/riotclient/kill-and-restart-ux", "POST");

        private static bool GetCredentials(string lcPath, out string port, out string pass)
        {
            try
            {
                var lockfilePath = Path.Combine(lcPath, "lockfile");

                using (var fileStream = new FileStream(lockfilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    using (var reader = new StreamReader(fileStream))
                    {
                        var content = reader.ReadToEnd();

                        if (!string.IsNullOrEmpty(content))
                        {
                            var tokens = content.Split(':');
                            port = tokens[2];
                            pass = tokens[3];
                            return true;
                        }
                    }
                }
            }
            catch
            {
            }

            port = pass = string.Empty;
            return false;
        }
    }
}