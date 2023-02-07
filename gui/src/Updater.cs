using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace LeagueLoader
{
    internal static class Updater
    {
        const string API_URL = "https://api.github.com/repos/nomi-san/league-loader/releases/latest";
        const string DOWNLOAD_URL = "https://github.com/nomi-san/league-loader/releases/latest";

        public static Version CurrentVersion => Assembly.GetExecutingAssembly().GetName().Version;

        public class Update
        {
            public string Version;
            public string Changes;
        }

        static Updater()
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;
        }

        public static void OpenDownload()
        {
            Process.Start(DOWNLOAD_URL);
        }

        public static async Task<Update> CheckUpdate()
        {
            try
            {
                var json = await GetAsync(API_URL);
                var match = new Regex("\"tag_name\":\\s+\"(.*)\"").Match(json);

                if (match.Success && match.Groups.Count > 1)
                {
                    var vtag = match.Groups[1].Value.ToLower();
                    if (vtag.StartsWith("v"))
                        vtag = vtag.Substring(1);

                    var remote = new Version(vtag);
                    var local = CurrentVersion;

                    if (remote.CompareTo(local) > 0)
                    {
                        string changes = null;
                        match = new Regex("\"body\":\\s+\"(.*)\"").Match(json);
                        if (match.Success && match.Groups.Count > 1)
                            changes = Regex.Unescape(match.Groups[1].Value);

                        return new Update
                        {
                            Version = vtag,
                            Changes = changes
                        };
                    }
                }

                return null;
            }
            catch
            {
                return null;
            }
        }

        static async Task<string> GetAsync(string uri)
        {
            HttpWebRequest request = (HttpWebRequest)WebRequest.Create(uri);
            request.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;
            request.UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.0.0 Safari/537.36";

            using (HttpWebResponse response = (HttpWebResponse)await request.GetResponseAsync())
            using (Stream stream = response.GetResponseStream())
            using (StreamReader reader = new StreamReader(stream))
            {
                return await reader.ReadToEndAsync();
            }
        }
    }
}