using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using Ookii.Dialogs.Wpf;

namespace LeagueLoader.Main
{
    static class Updater
    {
        const string API_URL = "https://api.github.com/repos/nomi-san/league-loader/releases/latest";
        const string DOWNLOAD_URL = "https://github.com/nomi-san/league-loader/releases/latest";
        const string USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)" +
            " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.0.0 Safari/537.36";

        static System.Version CurrentVersion => Assembly.GetExecutingAssembly().GetName().Version;

        class Update
        {
            public string Version;
            public string Changes;
            public string DownloadUrl;
        }

        static Updater()
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;
        }

        public static async void CheckUpdate()
        {
            var update = await FetchUpdate();
            if (update == null) return;

            var dialog = new ProgressDialog()
            {
                WindowTitle = "League Loader v" + update.Version,
                ShowTimeRemaining = false,
                ProgressBarStyle = ProgressBarStyle.MarqueeProgressBar
            };

            var cancel = false;
            var percent = 0;
            var message = "Downloading...";

            dialog.DoWork += (s, e) =>
            {
                while (percent < 100)
                {
                    if (cancel || dialog.CancellationPending)
                    {
                        e.Cancel = true;
                        return;
                    }

                    dialog.ReportProgress(percent, "Downloading update...", message);
                    Thread.Sleep(100);
                }

                Thread.Sleep(100);
                dialog.ReportProgress(100, "Updating...", "Done.");
            };

            MainWindow.Instance.Hide();
            dialog.Show();

            try
            {
                var updateDir = Path.Combine(Directory.GetCurrentDirectory(), ".update");

                var tempFile = Path.GetTempFileName();
                await DownloadFile(update.DownloadUrl, tempFile, (downloaded, total, percent_) =>
                {
                    percent = percent_;
                    message = String.Format("{0:0.##} / {1:0.##} MB received.",
                        (double)downloaded / 1024 / 1024,
                        (double)total / 1024 / 1024);
                });

                Utils.DeletePath(updateDir, true);
                ZipFile.ExtractToDirectory(tempFile, updateDir);
                Utils.DeletePath(tempFile);

                while (Module.IsLoaded())
                {
                    MessageBox.Show("Please close your League of Legends Client to apply update.",
                        Program.NAME, MessageBoxButton.OK, MessageBoxImage.Information);
                }

                ApplyUpdate();
                Environment.Exit(0);
            }
            catch
            {
                cancel = true;

                MainWindow.Instance.Show();
                MessageBox.Show(MainWindow.Instance,
                    "Failed to download update. Please try downloading the update on GitHub releases page.",
                    Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);

                Utils.OpenLink(DOWNLOAD_URL);
            }
            finally
            {
                dialog.Dispose();
            }
        }

        static async Task<Update> FetchUpdate()
        {
            try
            {
                var json = await DownloadString(API_URL);
                var match = new Regex("\"tag_name\":\\s+\"(.*)\"").Match(json);

                if (match.Success && match.Groups.Count > 1)
                {
                    var vtag = match.Groups[1].Value.ToLower();
                    if (vtag.StartsWith("v"))
                        vtag = vtag.Substring(1);

                    var remote = new System.Version(vtag);
                    var local = CurrentVersion;

                    if (remote.CompareTo(local) > 0)
                    {
                        string changes = "";
                        match = new Regex("\"body\":\\s+\"(.*)\"").Match(json);
                        if (match.Success && match.Groups.Count > 1)
                            changes = Regex.Unescape(match.Groups[1].Value);

                        string downloadUrl = "";
                        match = new Regex("\"browser_download_url\":\\s+\"(.*(\\d+\\.?)(?:-stable)?\\.zip)\"").Match(json);
                        if (match.Success && match.Groups.Count > 1)
                            downloadUrl = Regex.Unescape(match.Groups[1].Value);

                        return new Update
                        {
                            Version = vtag,
                            Changes = changes,
                            DownloadUrl = downloadUrl
                        };
                    }
                }

                return null;
            }
            catch (Exception ex)
            {
                MessageBox.Show(MainWindow.Instance,
                    "Failed to check update.\n" + ex.Message,
                    Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);
                return null;
            }
        }

        static async Task<string> DownloadString(string url)
        {
            HttpWebRequest request = (HttpWebRequest)WebRequest.Create(url);
            request.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;
            request.UserAgent = USER_AGENT;

            using (HttpWebResponse response = (HttpWebResponse)await request.GetResponseAsync())
            using (Stream stream = response.GetResponseStream())
            using (StreamReader reader = new StreamReader(stream))
            {
                return await reader.ReadToEndAsync();
            }
        }

        static async Task DownloadFile(string url, string path, Action<long, long, int> onProgress)
        {
            using (WebClient client = new WebClient())
            {
                client.Headers.Add("User-Agent", USER_AGENT);
                client.DownloadProgressChanged += (s, e) =>
                {
                    onProgress.Invoke(e.BytesReceived, e.TotalBytesToReceive, e.ProgressPercentage);
                };

                await client.DownloadFileTaskAsync(new Uri(url), path);
            }
        }

        static void ApplyUpdate()
        {
            var exe = AppDomain.CurrentDomain.FriendlyName;
            var dir = Directory.GetCurrentDirectory();
            var updateDir = Path.Combine(dir, ".update");

            var args = new[]
            {
                $"xcopy /s /y \"{updateDir}\" \"{dir}\"",
                $"rd /s /q \"{updateDir}\"",
                $"start /d \"{dir}\" \"\" \"{dir}\\{exe}\"",
            };

            Process.Start(new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = "/c " + string.Join(" & ", args),
                Verb = "runas",
                UseShellExecute = true,
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden
            });
        }
    }
}