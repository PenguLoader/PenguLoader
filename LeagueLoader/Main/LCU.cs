using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace LeagueLoader.Main
{
    static class LCU
    {
        static HttpClient Http;

        static LCU()
        {
            Http = new HttpClient();
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;
            ServicePointManager.ServerCertificateValidationCallback += (a, b, c, d) => true;
        }

        public static bool IsRunning()
        {
            return Process.GetProcessesByName("LeagueClientUx").Length > 0;
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
            var lcPath = GetDir();

            if (string.IsNullOrEmpty(lcPath))
                return null;

            if (GetCredentials(lcPath, out var port, out var pass))
            {
                var uri = $"https://127.0.0.1:{port}{api}";
                var authToken = Encoding.ASCII.GetBytes("riot:" + pass);
                var authorization = "Basic " + Convert.ToBase64String(authToken);

                try
                {
                    var req = new HttpRequestMessage(new HttpMethod(method), uri);
                    req.Headers.Add("Authorization", authorization);

                    if (!string.IsNullOrEmpty(body))
                        req.Content = new StringContent(body, Encoding.UTF8, "application/json");

                    var res = await Http.SendAsync(req);
                    return await res.Content.ReadAsStringAsync();
                }
                catch
                {
                }
            }

            return null;
        }

        public static void KillUxAndRestart()
        {
            Task.Run(() => Request("/riotclient/kill-and-restart-ux", "POST"));
        }

        static bool GetCredentials(string lcPath, out string port, out string pass)
        {
            try
            {
                var lockfilePath = Path.Combine(lcPath, "lockfile");

                // Read permission only to avoid access denied.
                using (var fileStream = new FileStream(lockfilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (StreamReader reader = new StreamReader(fileStream))
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
            catch
            {
            }

            port = pass = "";
            return false;
        }
    }
}