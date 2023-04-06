using System;
using System.Windows;

using ModernWpf;
using PenguLoader.Main;
using System.Windows.Interop;

namespace PenguLoader
{
    public partial class MainWindow : Window
    {
        public static MainWindow Instance { get; private set; }

        public MainWindow()
        {
            Instance = this;
            InitializeComponent();
            WindowStyle = WindowStyle.ToolWindow;
            ShowInTaskbar = true;

            UpdateActiveState();

            btnPlugins.Content = $"Open plugins ({Plugins.CountEntries()})";
            txtVersion.Text = $"v{Version.VERSION}.{Version.BUILD_NUMBER}";

            btnGitHub.Click += delegate { Utils.OpenLink(Program.GITHUB_URL); };
            btnDiscord.Click += delegate { Utils.OpenLink(Program.DISCORD_URL); };
            btnHomepage.Click += delegate { Utils.OpenLink(Program.HOMEPAGE_URL); };

            Loaded += MainWindow_Loaded;
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            Loaded -= MainWindow_Loaded;

            Show();

            Window window = Window.GetWindow(this);
            var hwnd = new WindowInteropHelper(window).Handle;

            var oldEx = Native.GetWindowLongPtr(hwnd, -0x14).ToInt32();
            Native.SetWindowLongPtr(hwnd, -0x14, (IntPtr)(oldEx & ~0x80));

            Updater.CheckUpdate();
        }

        private void BtnTheme_Click(object sender, RoutedEventArgs e)
        {
            var tm = ThemeManager.Current;
            if (tm.ApplicationTheme == null)
                tm.ApplicationTheme = (tm.ActualApplicationTheme == ApplicationTheme.Light) ? ApplicationTheme.Light : ApplicationTheme.Dark;
            tm.ApplicationTheme = (tm.ActualApplicationTheme == ApplicationTheme.Light) ? ApplicationTheme.Dark : ApplicationTheme.Light;
        }

        private void BtnGitHub_Click(object sender, RoutedEventArgs e)
        {
            Utils.OpenLink(Program.GITHUB_URL);
        }

        private void BtnHomepage_Click(object sender, RoutedEventArgs e)
        {
            Utils.OpenLink(Program.HOMEPAGE_URL);
        }

        private void BtnAssets_Click(object sender, RoutedEventArgs e)
        {
            Utils.OpenFolder(Config.AssetsDir);
        }

        private void BtnPlugins_Click(object sender, RoutedEventArgs e)
        {
            Utils.OpenFolder(Config.PluginsDir);
        }

        private void BtnDataStore_Click(object sender, RoutedEventArgs e)
        {
            DataStore.Dump();
        }

        private void BtnActivate_Toggled(object sender, RoutedEventArgs e)
        {
            try
            {
                ToggleActivation();
            }
            catch (Exception err)
            {
                if (MessageBox.Show(this,
                    "Failed to perform activation.\n" +
                    "Error: " + err.Message + "\n\n" +
                    "Please capture the error message and click Yes to report it.",
                    Program.NAME, MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes)
                {
                    Utils.OpenLink(Program.GITHUB_ISSUES_URL);
                }
            }
            finally
            {
                UpdateActiveState();
            }
        }

        void ToggleActivation()
        {
            if (!Module.IsActivated())
            {
                if (!Module.Exist())
                {
                    MessageBox.Show(this,
                        "Failed to activate the Loader. The core.dll is not found.",
                        Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);
                }
                else if (Module.Activate())
                {
                    if (LCU.IsRunning())
                    {
                        if (MessageBox.Show(this,
                            "The Loader has been activated successfully.\n" +
                            "Do you want to restart the running League of Legends Client now?",
                            Program.NAME, MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                        {
                            LCU.KillUxAndRestart();
                        }
                    }
                }
                else
                {
                    MessageBox.Show(this,
                        "Failed to activate the Loader.\n",
                        Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);
                }
            }
            else
            {
                Module.Deactivate();

                if (Module.IsLoaded())
                {
                    if (MessageBox.Show(this,
                        "The Loader has been deactivated successfully.\n" +
                        "Do you want to restart running League of Legends Client to apply?",
                        Program.NAME, MessageBoxButton.YesNo, MessageBoxImage.Information) == MessageBoxResult.Yes)
                    {
                        LCU.KillUxAndRestart();
                    }
                }
            }
        }

        void UpdateActiveState()
        {
            btnActivate.Toggled -= BtnActivate_Toggled;
            btnActivate.IsOn = Module.IsActivated();
            btnActivate.Toggled += BtnActivate_Toggled;
        }
    }
}