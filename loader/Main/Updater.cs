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
    internal static class Updater
    {
        private const string UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)" +
                                         " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.0.0 Safari/537.36";

        static Updater()
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;
        }

        private static string ApiUrl => $"https://api.github.com/repos/{Program.GithubRepo}/releases/latest";
        private static string DownloadUrl => $"https://github.com/{Program.GithubRepo}/releases/latest";

        public static async void CheckUpdate()
        {
            var update = await FetchUpdate();
            if (update == null) return;

            using (var taskDialog = new TaskDialog())
            {
                taskDialog.WindowTitle = Program.Name;
                taskDialog.MainInstruction = $@"v{update.Version} update available!";
                taskDialog.Content = update.Changes;

                var acceptButton = new TaskDialogButton("Update now");
                taskDialog.Buttons.Add(acceptButton);
                taskDialog.Buttons.Add(new TaskDialogButton("Later"));

                if (taskDialog.ShowDialog(MainWindow.Instance) != acceptButton) return;
            }

            var dialog = new ProgressDialog
            {
                WindowTitle = Program.Name + @" v" + update.Version,
                ShowTimeRemaining = false,
                ProgressBarStyle = ProgressBarStyle.MarqueeProgressBar
            };

            var progress = 0;
            var message = "Downloading...";
            var cancellation = new CancellationTokenSource();

            dialog.DoWork += (s, e) =>
            {
                var progressDialog = s as ProgressDialog; // Cast s to ProgressDialog

                while (progress < 100) // LoopVariableIsNeverChangedInsideLoop
                {
                    if (progressDialog != null &&
                        (cancellation.IsCancellationRequested || progressDialog.CancellationPending))
                    {
                        e.Cancel = true;
                        return;
                    }

                    progressDialog?.ReportProgress(progress, "Downloading update...", message);
                    Thread.Sleep(100);
                }

                Thread.Sleep(100);
                progressDialog?.ReportProgress(100, "Updating...", "Done.");
            };

            dialog.ShowDialog(MainWindow.Instance, cancellation.Token);

            try
            {
                var updateDir = Path.Combine(Directory.GetCurrentDirectory(), ".update");

                var tempFile = Path.GetTempFileName();
                await DownloadFile(update.DownloadUrl, tempFile, (downloaded, total, percent) =>
                {
                    progress = percent;
                    message = $"{(double)downloaded / 1024 / 1024:0.##} / {(double)total / 1024 / 1024:0.##} MB";
                });

                Utils.DeletePath(updateDir, true);
                ZipFile.ExtractToDirectory(tempFile, updateDir);
                Utils.DeletePath(tempFile);

                while (Module.IsLoaded)
                    MessageBox.Show("Please close your League of Legends Client to apply update.",
                        Program.Name, MessageBoxButton.OK, MessageBoxImage.Information);

                ApplyUpdate();
                Environment.Exit(0);
            }
            catch
            {
                cancellation.Cancel();

                MainWindow.Instance.Show();
                MessageBox.Show(MainWindow.Instance,
                    "Failed to download update. Please try downloading the update on GitHub releases page.",
                    Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);

                Utils.OpenLink(DownloadUrl);
            }
            finally
            {
                dialog.Dispose();
            }
        }

        private static async Task<Update> FetchUpdate()
        {
            try
            {
                var json = await DownloadString(ApiUrl);
                var match = new Regex("\"tag_name\":\\s+\"(.*)\"").Match(json);

                if (!match.Success || match.Groups.Count <= 1) return null;
                var version = match.Groups[1].Value.ToLower();
                if (version.StartsWith("v"))
                    version = version.Substring(1);

                var remote = new Version(version);
                var local = new Version(Program.Version);

                if (remote.CompareTo(local) <= 0) return null;
                var changes = "";
                match = new Regex("\"body\":\\s+\"(.*)\"").Match(json);
                if (match.Success && match.Groups.Count > 1)
                    changes = Regex.Unescape(match.Groups[1].Value);

                var downloadUrl = "";
                match =
                    new Regex("\"browser_download_url\":\\s+\"(.*(\\d+\\.?)(?:-stable)?\\.zip)\"").Match(json);
                if (match.Success && match.Groups.Count > 1)
                    downloadUrl = Regex.Unescape(match.Groups[1].Value);

                return new Update
                {
                    Version = version,
                    Changes = changes,
                    DownloadUrl = downloadUrl
                };
            }
            catch
            {
                return null;
            }
        }

        private static async Task<string> DownloadString(string url)
        {
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;
            request.UserAgent = UserAgent;

            using (var response = (HttpWebResponse)await request.GetResponseAsync())
            using (var stream = response.GetResponseStream())
            {
                if (stream == null) return null;
                using (var reader = new StreamReader(stream))
                {
                    return await reader.ReadToEndAsync();
                }
            }
        }

        private static async Task DownloadFile(string url, string path, Action<long, long, int> onProgress)
        {
            using (var client = new WebClient())
            {
                client.Headers.Add("User-Agent", UserAgent);
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
                $"start /d \"{dir}\" \"\" \"{dir}\\{exe}\""
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

        private class Update
        {
            public string Changes;
            public string DownloadUrl; // MemberHidesStaticFromOuterClass
            public string Version;
        }
    }
}