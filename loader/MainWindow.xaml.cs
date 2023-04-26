using System;
using System.Windows;
using System.Windows.Interop;
using ModernWpf;
using PenguLoader.Main;

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
            SourceInitialized += InitializeWindowHook;
        }

        private void ConfigureWindow()
        {
            WindowStyle = WindowStyle.ToolWindow;
            ShowInTaskbar = true;

            btnPlugins.Content = $"Open plugins ({Plugins.CountEntries()})";
            txtVersion.Text = $"v{Version.VERSION}.{Version.BUILD_NUMBER}";

            chkOptimizeClient.IsChecked = Config.OptimizeClient;
            chkSuperLowSpecMode.IsChecked = Config.SuperLowSpecMode;
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
            chkOptimizeClient.Click += ChkOptimizeClient_Click;
            chkSuperLowSpecMode.Click += ChkSuperLowSpecMode_Click;
        }

        private void ChkOptimizeClient_Click(object sender, RoutedEventArgs e)
        {
            if (chkOptimizeClient.IsChecked.Value)
            {
                // Prevent checked flickering.
                chkOptimizeClient.IsChecked = false;

                if (MessageBox.Show(this,
                    "Optimize Client\n\n" +
                    "It is recommended to enable this option. Enabling it will disable GPU rendering, " +
                    "disable some unused background tasks, and reduce lag.\n\n" +
                    "Do you want to continue?",
                    Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Information) == MessageBoxResult.Yes)
                {
                    chkOptimizeClient.IsChecked = true;
                }
            }

            Config.OptimizeClient = chkOptimizeClient.IsChecked.Value;
        }

        private void ChkSuperLowSpecMode_Click(object sender, RoutedEventArgs e)
        {
            if (chkSuperLowSpecMode.IsChecked.Value)
            {
                // Prevent checked flickering.
                chkSuperLowSpecMode.IsChecked = false;

                if (MessageBox.Show(this,
                    "Super Low Spec Mode\n\n" +
                    "This option extends the default Low Spec Mode. " +
                    "Enabling it will disable all transition and animation effects, " +
                    "also greatly reduce lag and increase response speed.\n\n" +
                    "It's very helpful for low PC, but may cause bug.\n" +
                    "Do you want to continue?",
                    Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Information) == MessageBoxResult.Yes)
                {
                    chkSuperLowSpecMode.IsChecked = true;
                }
            }

            Config.SuperLowSpecMode = chkSuperLowSpecMode.IsChecked.Value;
        }

        private void OnMainWindowLoaded(object sender, RoutedEventArgs e)
        {
            Loaded -= OnMainWindowLoaded;

            UpdateActiveState();
            Show();

            // Fix window style issue.
            var hwnd = new WindowInteropHelper(this).Handle;
            var oldEx = Native.GetWindowLongPtr(hwnd, -0x14).ToInt32();
            Native.SetWindowLongPtr(hwnd, -0x14, (IntPtr)(oldEx & ~0x80));

            Updater.CheckUpdate();
        }

        private void BtnTheme_Click(object sender, RoutedEventArgs e)
        {
            var tm = ThemeManager.Current;
            var isLight = tm.ApplicationTheme == null
                ? tm.ActualApplicationTheme == ApplicationTheme.Light
                : tm.ApplicationTheme == ApplicationTheme.Light;

            tm.ApplicationTheme = isLight
                ? ApplicationTheme.Dark
                : ApplicationTheme.Light;
        }

        private void BtnAssets_Click(object sender, RoutedEventArgs e) => Utils.OpenFolder(Config.AssetsDir);

        private void BtnPlugins_Click(object sender, RoutedEventArgs e) => Utils.OpenFolder(Config.PluginsDir);

        private void BtnDataStore_Click(object sender, RoutedEventArgs e) => DataStore.Dump();

        private void BtnActivate_Toggled(object sender, RoutedEventArgs e)
        {
            try
            {
                ToggleActivation();
            }
            catch (Exception ex)
            {
                var message = $"Failed to perform activation.\nError: {ex.Message}\n\nPlease capture the error message and click Yes to report it.";
                var result = ShowMessage(message, Program.Name, MessageBoxImage.Warning, MessageBoxButton.YesNo);

                if (result == MessageBoxResult.Yes)
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
                    ShowMessage("Failed to activate the Loader: \"core.dll\" not found.", Program.Name, MessageBoxImage.Error);
                }
                else if (Module.Activate())
                {
                    PromptRestart("The Loader has been activated successfully.", false);
                }
                else
                {
                    ShowMessage("Failed to activate the Loader.", Program.Name, MessageBoxImage.Error);
                }
            }
            else
            {
                Module.Deactivate();
                PromptRestart("The Loader has been deactivated successfully.", true);
            }
        }

        private MessageBoxResult ShowMessage(string message, string caption, MessageBoxImage icon, MessageBoxButton button = MessageBoxButton.OK)
            => icon == MessageBoxImage.Question
                ? MessageBox.Show(this, message, caption, MessageBoxButton.YesNo, icon)
                : MessageBox.Show(this, message, caption, MessageBoxButton.OK, icon);

        private void PromptRestart(string message, bool isDeactivaed)
        {
            if ((LCU.IsRunning() && !isDeactivaed) || (Module.IsLoaded() && isDeactivaed))
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

        private void InitializeWindowHook(object sender, EventArgs e)
        {
            var source = HwndSource.FromHwnd(new WindowInteropHelper(this).Handle);
            source.AddHook(new HwndSourceHook(WndProc));
        }

        private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wp, IntPtr lp, ref bool handled)
        {
            if (msg == Native.WM_SHOWME)
            {
                if (WindowState == WindowState.Minimized)
                {
                    WindowState = WindowState.Normal;
                }

                if (!IsVisible)
                {
                    Show();
                }

                Activate();

                handled = true;
            }

            return IntPtr.Zero;
        }
    }
}