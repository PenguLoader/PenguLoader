using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using Ookii.Dialogs.Wpf;

namespace PenguLoader.Main
{
    static class Updater
    {
        private const string ApiUrl = "https://api.github.com/repos/{0}/releases/latest";
        private const string DownloadUrl = "https://github.com/{0}/releases/latest";
        private const string UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)" +
            " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.0.0 Safari/537.36";

        static Updater()
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;
        }

        public static async Task CheckUpdate()
        {
            var update = await FetchUpdate();
            if (update == null) return;

            await ShowProgressDialog(update);
        }

        private static async Task<Update> FetchUpdate()
        {
            try
            {
                var apiUrl = string.Format(ApiUrl, Program.GithubRepo);
                var json = await DownloadString(apiUrl);
                var remoteVersion = ExtractVersion(json);
                var localVersion = new System.Version(Version.VERSION);

                if (remoteVersion.CompareTo(localVersion) > 0)
                {
                    return new Update
                    {
                        Version = remoteVersion.ToString(),
                        Changes = ExtractChanges(json),
                        DownloadUrl = ExtractDownloadUrl(json)
                    };
                }

                return null;
            }
            catch (Exception ex)
            {
                MessageBox.Show(MainWindow.Instance,
                    "Failed to check update.\n" + ex.Message,
                    Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);
                return null;
            }
        }

        private static async Task<string> DownloadString(string url)
        {
            using (var client = new WebClient())
            {
                client.Headers.Add(HttpRequestHeader.UserAgent, UserAgent);
                return await client.DownloadStringTaskAsync(url);
            }
        }

        private static async Task DownloadFile(string url, string path, Action<long, long, int> onProgress)
        {
            using (var client = new WebClient())
            {
                client.Headers.Add(HttpRequestHeader.UserAgent, UserAgent);
                client.DownloadProgressChanged += (s, e) =>
                {
                    onProgress.Invoke(e.BytesReceived, e.TotalBytesToReceive, e.ProgressPercentage);
                };

                await client.DownloadFileTaskAsync(new Uri(url), path);
            }
        }

        private static void ApplyUpdate()
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

        private static System.Version ExtractVersion(string json)
        {
            var match = new Regex("\"tag_name\":\\s+\"(.*)\"").Match(json);
            return match.Success && match.Groups.Count > 1
                ? new System.Version(match.Groups[1].Value.TrimStart('v').ToLower())
                : null;
        }

        private static string ExtractChanges(string json)
        {
            var match = new Regex("\"body\":\\s+\"(.*)\"").Match(json);
            return match.Success && match.Groups.Count > 1
                ? Regex.Unescape(match.Groups[1].Value)
                : string.Empty;
        }

        private static string ExtractDownloadUrl(string json)
        {
            var match = new Regex("\"browser_download_url\":\\s+\"(.*(\\d+\\.?)(?:-stable)?\\.zip)\"").Match(json);
            return match.Success && match.Groups.Count > 1
                ? Regex.Unescape(match.Groups[1].Value)
                : string.Empty;
        }
        private static async Task ShowProgressDialog(Update update)
        {
            var dialog = new ProgressDialog()
            {
                WindowTitle = $"{Program.Name} v{update.Version}",
                ShowTimeRemaining = false,
                ProgressBarStyle = ProgressBarStyle.MarqueeProgressBar
            };

            var cancellationTokenSource = new CancellationTokenSource();
            IProgress<DownloadProgress> progress = new Progress<DownloadProgress>(p => dialog.ReportProgress(p.Percent, $"Downloading update...",
                $"{p.DownloadedMb:0.##} / {p.TotalMb:0.##} MB received."));

            var tcs = new TaskCompletionSource<bool>();

            dialog.DoWork += async (s, e) =>
            {
                try
                {
                    var tempFile = Path.GetTempFileName();
                    var updateDir = Path.Combine(Directory.GetCurrentDirectory(), ".update");

                    await DownloadFile(update.DownloadUrl, tempFile, (downloaded, total, percent) =>
                    {
                        if (cancellationTokenSource.Token.IsCancellationRequested)
                        {
                            cancellationTokenSource.Cancel();
                            return;
                        }
                        progress.Report(new DownloadProgress(downloaded, total, percent));
                    });

                    Utils.DeletePath(updateDir, true);
                    ZipFile.ExtractToDirectory(tempFile, updateDir);
                    Utils.DeletePath(tempFile);

                    while (Module.IsLoaded())
                    {
                        MessageBox.Show("Please close your League of Legends Client to apply the update.",
                            Program.Name, MessageBoxButton.OK, MessageBoxImage.Information);
                    }

                    ApplyUpdate();
                    Environment.Exit(0);
                }
                catch
                {
                    if (!cancellationTokenSource.Token.IsCancellationRequested)
                    {
                        cancellationTokenSource.Cancel();

                        MainWindow.Instance.Show();
                        MessageBox.Show(MainWindow.Instance,
                            "Failed to download update. Please try downloading the update on GitHub releases page.",
                            Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);

                        Utils.OpenLink(string.Format(DownloadUrl, Program.GithubRepo));
                    }
                }
                finally
                {
                    tcs.SetResult(true);
                }
            };

            MainWindow.Instance.Hide();
            dialog.Show(MainWindow.Instance);

            await tcs.Task;
        }

        private class Update
        {
            public string Version;
            public string Changes;
            public string DownloadUrl;
        }

        private class DownloadProgress
        {
            public DownloadProgress(long downloaded, long total, int percent)
            {
                DownloadedMb = (double)downloaded / 1024 / 1024;
                TotalMb = (double)total / 1024 / 1024;
                Percent = percent;
            }

            public double DownloadedMb { get; }
            public double TotalMb { get; }
            public int Percent { get; }
        }
    }
}