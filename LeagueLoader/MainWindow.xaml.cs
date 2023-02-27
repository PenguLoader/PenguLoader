using System;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;

using ModernWpf;
using LeagueLoader.Main;
using System.Windows.Media;
using System.Windows.Interop;

namespace LeagueLoader
{
    public partial class MainWindow : Window
    {
        public static MainWindow Instance { get; private set; }

        public MainWindow()
        {
            Instance = this;
            InitializeComponent();
            WindowStyle = WindowStyle.ToolWindow;

            var port = Config.RemoteDebuggingPort;
            chkRemoteDebugger.IsChecked = port != 0;
            txtRemotePort.Text = port != 0 ? port.ToString() : "8888";

            SetActiveState(Module.IsActivated());
            btnPlugins.Content = $"Open plugins ({Plugins.CountEntries()})";

            txtVersion.Text = $"v{Version.VERSION} build {Version.BUILD_NUMBER}";

            Loaded += delegate
            {
                Show();
                Updater.CheckUpdate();
            };
        }

        protected override void OnSourceInitialized(EventArgs e)
        {
            base.OnSourceInitialized(e);

            Window window = Window.GetWindow(this);
            var hwnd = new WindowInteropHelper(window).Handle;

            var oldEx = Native.GetWindowLongPtr(hwnd, -0x14).ToInt32();
            Native.SetWindowLongPtr(hwnd, -0x14, (IntPtr)(oldEx & ~0x80));
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

        private async void BtnRestart_Click(object sender, RoutedEventArgs e)
        {
            if (LCU.IsRunning())
            {
                faRestart.Spin = true;
                btnRestartUX.IsEnabled = false;

                LCU.KillUxAndRestart();
                await Task.Delay(5000);

                faRestart.Spin = false;
                btnRestartUX.IsEnabled = true;
            }
            else
            {
                MessageBox.Show(this, "League of Legends Client is not running.",
                    Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void BtnDevTools_Click(object sender, RoutedEventArgs e)
        {
            if (Module.IsLoaded())
            {
                Module.OpenDevTools(remote: false);
            }
            else
            {
                MessageBox.Show(this, "League Loader is not loaded by any Client.",
                    Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void BtnRemoteDevTools_Click(object sender, RoutedEventArgs e)
        {
            if (Module.IsLoaded())
            {
                Module.OpenDevTools(remote: true);
            }
            else
            {
                MessageBox.Show(this, "League Loader is not loaded by any Client.",
                    Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void ChkRemoteDebugger_Checked(object sender, RoutedEventArgs e)
        {
            if (chkRemoteDebugger.IsChecked.Value)
            {
                int port = 0;
                int.TryParse(txtRemotePort.Text, out port);
                Config.RemoteDebuggingPort = port;
            }
            else
            {
                Config.RemoteDebuggingPort = 0;
            }
        }

        private void TxtRemotePort_PreviewTextInput(object sender, System.Windows.Input.TextCompositionEventArgs e)
        {
            var regex = new Regex("[^0-9]{1,5}");
            e.Handled = regex.IsMatch(e.Text);
        }

        private void BtnActivate_Click(object sender, RoutedEventArgs e)
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
        }

        void ToggleActivation()
        {
            if (!Module.IsActivated())
            {
                if (Module.Activate())
                {
                    SetActiveState(true);

                    if (LCU.IsRunning())
                    {
                        if (MessageBox.Show(this,
                            "League Loader has been activated successfully.\n" +
                            "Do you want to restart the running League of Legends Client now?",
                            Program.NAME, MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                        {
                            LCU.KillUxAndRestart();
                        }
                    }
                    else
                    {
                        MessageBox.Show(this,
                            "League Loader has been activated successfully.\n" +
                            "Let's launch your League of Legends Client to enjoy!",
                            Program.NAME, MessageBoxButton.OK, MessageBoxImage.Information);
                    }
                }
                else
                {
                    MessageBox.Show(this,
                        "League Loader failed to activate.\n",
                        Program.NAME, MessageBoxButton.OK, MessageBoxImage.Warning);
                }
            }
            else
            {
                if (MessageBox.Show(this,
                    "Do you want to deactivate League Loader?",
                    Program.NAME, MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                {
                    Module.Deactivate();
                    SetActiveState(false);

                    if (Module.IsLoaded())
                    {
                        if (MessageBox.Show(this,
                            "League Loader has been deactivated successfully.\n" +
                            "Do you want to restart running League of Legends Client to apply?",
                            Program.NAME, MessageBoxButton.YesNo, MessageBoxImage.Information) == MessageBoxResult.Yes)
                        {
                            LCU.KillUxAndRestart();
                        }
                    }
                    else
                    {
                        MessageBox.Show(this,
                            "League Loader has been deactivated successfully.",
                            Program.NAME, MessageBoxButton.OK, MessageBoxImage.Information);
                    }
                }
            }
        }

        void SetActiveState(bool activated)
        {
            if (activated)
            {
                txtActivate.Text = "ACTIVATED";
                faActivate.Icon = FontAwesome.WPF.FontAwesomeIcon.Check;
            }
            else
            {
                txtActivate.Text = "ACTIVATE";
                faActivate.Icon = FontAwesome.WPF.FontAwesomeIcon.Rocket;
            }
        }
    }
}