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
            ConfigureWindow();
            InitializeButtons();
            Loaded += OnMainWindowLoaded;
        }

        private void ConfigureWindow()
        {
            WindowStyle = WindowStyle.ToolWindow;
            ShowInTaskbar = true;

            btnPlugins.Content = $"Open plugins ({Plugins.CountEntries()})";
            txtVersion.Text = $"v{Version.VERSION}.{Version.BUILD_NUMBER}";
        }

        private void InitializeButtons()
        {
            btnGitHub.Click += (sender, args) => Utils.OpenLink(Program.GithubUrl);
            btnDiscord.Click += (sender, args) => Utils.OpenLink(Program.DiscordUrl);
            btnHomepage.Click += (sender, args) => Utils.OpenLink(Program.HomepageUrl);
            btnTheme.Click += BtnTheme_Click;
            btnAssets.Click += BtnAssets_Click;
            btnPlugins.Click += BtnPlugins_Click;
            btnDataStore.Click += BtnDataStore_Click;
            btnActivate.Toggled += BtnActivate_Toggled;
        }

        private async void OnMainWindowLoaded(object sender, RoutedEventArgs e)
        {
            Loaded -= OnMainWindowLoaded;
            Show();

            var hwnd = new WindowInteropHelper(this).Handle;
            var oldEx = Native.GetWindowLongPtr(hwnd, -0x14).ToInt32();
            Native.SetWindowLongPtr(hwnd, -0x14, (IntPtr)(oldEx & ~0x80));

            await Updater.CheckUpdate();
        }

        private void BtnTheme_Click(object sender, RoutedEventArgs e)
        {
            var tm = ThemeManager.Current;
            tm.ApplicationTheme = (tm.ApplicationTheme == ApplicationTheme.Light) 
                ? ApplicationTheme.Dark 
                : ApplicationTheme.Light;
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
                    Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes)
                {
                    Utils.OpenLink(Program.GithubIssuesUrl);
                }
            }
            finally
            {
                UpdateActiveState();
            }
        }

        private void ToggleActivation()
        {
            if (!Module.IsActivated())
            {
                if (!Module.Exists())
                {
                    ShowMessage("Failed to activate the Loader. core.dll not found.", Program.Name, MessageBoxImage.Error);
                }
                else if (Module.Activate())
                {
                    PromptRestart("The Loader has been activated successfully.");
                }
                else
                {
                    ShowMessage("Failed to activate the Loader.", Program.Name, MessageBoxImage.Error);
                }
            }
            else
            {
                Module.Deactivate();

                PromptRestart(Module.IsLoaded() ? "The Loader has been deactivated successfully." :
                                                    "The Loader has been deactivated successfully.");
            }
        }

        private MessageBoxResult ShowMessage(string message, string caption, MessageBoxImage icon)
        {
            if (icon == MessageBoxImage.Question)
            {
                return MessageBox.Show(this, message, caption, MessageBoxButton.YesNo, icon);
            }
            else
            {
                return MessageBox.Show(this, message, caption, MessageBoxButton.OK, icon);
            }
        }

        private void PromptRestart(string message)
        {
            if (LCU.IsRunning())
            {
                if (MessageBox.Show(this, "Do you want to restart the running League of Legends Client now?",
                    Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                {
                    LCU.KillUxAndRestart();
                    ShowMessage(message, Program.Name, MessageBoxImage.Information);
                }
            }
            else
            {
                ShowMessage(message, Program.Name, MessageBoxImage.Information);
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
