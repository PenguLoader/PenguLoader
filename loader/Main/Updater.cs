using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;
using Ookii.Dialogs.Wpf;

namespace PenguLoader.Main
{
    static class Updater
    {
        static string StableApiUrl => $"https://api.github.com/repos/{Program.GithubRepo}/releases/latest";
        static string ReleasesApiUrl => $"https://api.github.com/repos/{Program.GithubRepo}/releases?per_page=20";
        static string ReleasesUrl => $"https://github.com/{Program.GithubRepo}/releases";

        const string USER_AGENT = "PenguLoader-Updater/1.0";

        static bool _checking;
        internal static string BuildChannel => ReadBuildInfo().Channel;

        class Update
        {
            public string Version;
            public string DownloadUrl;
            public string ReleaseUrl;
        }

        class BuildInfo
        {
            public Version Version;
            public string Commit;
            public string Channel;
        }

        public class GitHubAsset
        {
            public string name { get; set; }
            public string browser_download_url { get; set; }
        }

        public class GitHubRelease
        {
            public string tag_name { get; set; }
            public string target_commitish { get; set; }
            public string html_url { get; set; }
            public bool draft { get; set; }
            public bool prerelease { get; set; }
            public GitHubAsset[] assets { get; set; }
        }

        static Updater()
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;
        }

        public static async void CheckUpdate()
        {
            if (_checking)
                return;

            _checking = true;
            try
            {
                await CheckUpdateCore();
            }
            finally
            {
                _checking = false;
            }
        }

        static async Task CheckUpdateCore()
        {
            var update = await FetchUpdate();
            if (update == null)
                return;

            var dialog = new ProgressDialog()
            {
                WindowTitle = Program.Name + " v" + update.Version,
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
                var rnd = new Random().Next().ToString("x");
                var updateDir = Path.Combine(Path.GetTempPath(), "pengu_update_" + rnd);
                Directory.CreateDirectory(updateDir);

                var tempFile = Path.GetTempFileName();
                await DownloadFile(update.DownloadUrl, tempFile, (downloaded, total, percent_) =>
                {
                    percent = percent_;
                    message = string.Format("{0:0.##} / {1:0.##} MB received.",
                        (double)downloaded / 1024 / 1024,
                        (double)total / 1024 / 1024);
                });

                ZipFile.ExtractToDirectory(tempFile, updateDir);
                Utils.DeletePath(tempFile);

                while (Module.IsLoaded)
                {
                    MessageBox.Show("Please close your League of Legends Client to apply update.",
                        Program.Name, MessageBoxButton.OK, MessageBoxImage.Information);
                }

                ApplyUpdate(updateDir);
                Environment.Exit(0);
            }
            catch
            {
                cancel = true;

                MainWindow.Instance.Show();
                MessageBox.Show(MainWindow.Instance,
                    "Failed to download update. Please try downloading the update on GitHub releases page.",
                    Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);

                Utils.OpenLink(update.ReleaseUrl ?? ReleasesUrl);
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
                var channel = Config.UpdateChannel;
                var serializer = new JavaScriptSerializer();
                GitHubRelease release;

                if (channel == "dev")
                {
                    var releases = serializer.Deserialize<GitHubRelease[]>(await DownloadString(ReleasesApiUrl));
                    release = releases.FirstOrDefault(item => item.prerelease && !item.draft
                        && item.assets != null
                        && item.assets.Any(candidate => candidate.name.EndsWith("-dev-windows.zip", StringComparison.OrdinalIgnoreCase)));
                }
                else
                {
                    release = serializer.Deserialize<GitHubRelease>(await DownloadString(StableApiUrl));
                }

                if (release == null || release.assets == null)
                    return null;

                var remoteVersion = ParseVersion(release.tag_name);
                var local = ReadBuildInfo();
                if (!ShouldUpdate(local, remoteVersion, release.target_commitish, channel))
                    return null;

                var suffix = "-" + channel + "-windows.zip";
                var asset = release.assets.FirstOrDefault(item =>
                    item.name.EndsWith(suffix, StringComparison.OrdinalIgnoreCase));

                if (asset == null)
                {
                    asset = release.assets.FirstOrDefault(item =>
                        item.name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)
                        && item.name.IndexOf("macos", StringComparison.OrdinalIgnoreCase) < 0);
                }

                if (asset == null)
                    throw new InvalidOperationException("The selected release has no Windows ZIP asset.");

                return new Update
                {
                    Version = remoteVersion + " (" + (channel == "dev" ? "Dev" : "Stable") + ")",
                    DownloadUrl = asset.browser_download_url,
                    ReleaseUrl = release.html_url
                };
            }
            catch (Exception ex)
            {
                MessageBox.Show(MainWindow.Instance,
                    "Failed to check update.\n" + ex.Message,
                    Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);
                return null;
            }
        }

        static Version ParseVersion(string tag)
        {
            var match = Regex.Match(tag ?? "", @"\d+(?:\.\d+){1,3}");
            Version version;
            return match.Success && Version.TryParse(match.Value, out version)
                ? version
                : new Version(0, 0);
        }

        static BuildInfo ReadBuildInfo()
        {
            var value = Program.VERSION;
            var path = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "version");
            if (File.Exists(path))
                value = File.ReadAllText(path).Trim();

            return ParseBuildInfo(value);
        }

        static BuildInfo ParseBuildInfo(string value)
        {
            var parts = value.Split('+');
            Version version;

            return new BuildInfo
            {
                Version = Version.TryParse(parts[0], out version) ? version : new Version(0, 0),
                Commit = parts.Length > 1 ? parts[1] : "",
                Channel = parts.Length > 2 && parts[2] == "dev" ? "dev" : "stable"
            };
        }

        static bool ShouldUpdate(BuildInfo local, Version remoteVersion, string remoteCommit, string channel)
        {
            if (local.Channel != channel)
                return true;

            if (IsCommit(remoteCommit))
            {
                if (SameCommit(local.Commit, remoteCommit))
                    return false;

                return remoteVersion.CompareTo(local.Version) >= 0;
            }

            return remoteVersion.CompareTo(local.Version) > 0;
        }

        static bool IsCommit(string value)
        {
            return Regex.IsMatch(value ?? "", "^[0-9a-fA-F]{7,40}$");
        }

        static bool SameCommit(string left, string right)
        {
            if (string.IsNullOrEmpty(left) || string.IsNullOrEmpty(right))
                return false;

            return left.StartsWith(right, StringComparison.OrdinalIgnoreCase)
                || right.StartsWith(left, StringComparison.OrdinalIgnoreCase);
        }

        internal static bool SelfTest()
        {
            var stable = new BuildInfo
            {
                Version = new Version(1, 2, 3),
                Commit = "abcdef12",
                Channel = "stable"
            };
            var dev = ParseBuildInfo("1.3.0+12345678+dev");

            return ParseVersion("v1.2.3-dev.42") == new Version(1, 2, 3)
                && dev.Version == new Version(1, 3, 0)
                && dev.Channel == "dev"
                && Config.ResolveUpdateChannel("", dev.Channel) == "dev"
                && Config.ResolveUpdateChannel("stable", dev.Channel) == "stable"
                && !ShouldUpdate(stable, new Version(1, 2, 3), "abcdef1234567890", "stable")
                && ShouldUpdate(stable, new Version(1, 2, 3), "1234567890abcdef", "stable")
                && ShouldUpdate(stable, new Version(1, 1, 0), "1234567890abcdef", "dev")
                && ShouldUpdate(dev, new Version(1, 2, 3), "release/v1.2.3", "stable");
        }

        static async Task<string> DownloadString(string url)
        {
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;
            request.UserAgent = USER_AGENT;
            request.Accept = "application/vnd.github+json";
            request.Headers.Add("X-GitHub-Api-Version", "2022-11-28");

            using (var response = (HttpWebResponse)await request.GetResponseAsync())
            using (var stream = response.GetResponseStream())
            using (var reader = new StreamReader(stream))
            {
                return await reader.ReadToEndAsync();
            }
        }

        static async Task DownloadFile(string url, string path, Action<long, long, int> onProgress)
        {
            using (var client = new WebClient())
            {
                client.Headers.Add("User-Agent", USER_AGENT);
                client.DownloadProgressChanged += (s, e) =>
                {
                    onProgress.Invoke(e.BytesReceived, e.TotalBytesToReceive, e.ProgressPercentage);
                };

                await client.DownloadFileTaskAsync(new Uri(url), path);
            }
        }

        static void ApplyUpdate(string updateDir)
        {
            var domain = AppDomain.CurrentDomain;
            var exe = domain.FriendlyName;
            var dir = domain.BaseDirectory;

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
