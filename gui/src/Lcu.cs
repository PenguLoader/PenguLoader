using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;

namespace LeagueLoader
{
    class Lcu
    {
        static HttpClient _client = new HttpClient();

        static string GetBaseUrl()
        {
            var procs = Process.GetProcessesByName("LeagueClientUx");

            if (procs.Length > 0)
            {
                var lcux = procs[0];
                var lcuxDir = Directory.GetParent(lcux.MainModule.FileName).FullName;
                var lockfilePath = Path.Combine(lcuxDir, "lockfile");

                if (File.Exists(lockfilePath))
                {
                    var lockfileData = File.ReadAllText(lockfilePath);

                    if (!string.IsNullOrEmpty(lockfileData))
                    {
                        var tokens = lockfileData.Split(':');

                        if (tokens.Length >= 5)
                        {
                            var port = tokens[2];
                            var auth = tokens[3];
                            return $"https://riot:{auth}@127.0.0.1:{port}";
                        }
                    }
                }
            }

            return "";
        }

        public static bool IsReady()
        {
            return !string.IsNullOrEmpty(GetBaseUrl());
        }

        public static void KillUxAndRestart()
        {
            var uri = GetBaseUrl() + "/riotclient/kill-and-restart-ux";
            _client.PostAsync(uri, null);
        }
    }
}