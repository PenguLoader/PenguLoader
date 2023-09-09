using System;
using System.ComponentModel;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader.Views
{
    public partial class MainPage : INotifyPropertyChanged
    {
        public MainPage()
        {
            InitializeComponent();
            DataContext = this;
        }

        private Window Owner => Window.GetWindow(this);

        public bool OptimizeClient
        {
            get => Config.OptimizeClient;
            set
            {
                if (value)
                {
                    var caption = App.GetTranslation("TOptimizeClient");
                    var message = App.GetTranslation("TMsgOptimizeClientPrompt");

                    value = MessageBox.Show(Owner, message, caption,
                        MessageBoxButton.YesNo, MessageBoxImage.Information) == MessageBoxResult.Yes;
                }

                Config.OptimizeClient = value;
                TriggerPropertyChanged(nameof(OptimizeClient));
            }
        }

        public bool SuperLowSpecMode
        {
            get => Config.SuperLowSpecMode;
            set
            {
                if (value)
                {
                    var caption = App.GetTranslation("TSuperPotatoMode");
                    var message = App.GetTranslation("TMsgSuperPotatoModePrompt");

                    value = MessageBox.Show(Owner, message, caption,
                        MessageBoxButton.YesNo, MessageBoxImage.Information) == MessageBoxResult.Yes;
                }

                Config.SuperLowSpecMode = value;
                TriggerPropertyChanged(nameof(SuperLowSpecMode));
            }
        }

        public bool IsActivated
        {
            get => Module.IsFound && Module.IsActivated;
            set
            {
                try
                {
                    if (!Module.IsFound)
                    {
                        MessageBox.Show(Owner, App.GetTranslation("TMsgModuleNotFound"),
                            Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);
                    }
                    else
                    {
                        Module.SetActive(value);
                        TriggerPropertyChanged(nameof(IsActivated));

                        if ((!value || !Lcu.IsRunning) && (value || !Module.IsLoaded)) return;
                        if (MessageBox.Show(Owner, App.GetTranslation("TMsgRestartClient"),
                                Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Question) ==
                            MessageBoxResult.Yes)
                            Lcu.KillUxAndRestart();
                    }
                }
                catch (Exception ex)
                {
                    var msg = App.GetTranslation("TMsgActivationFail");
                    msg += $"\n\nERR: {ex.Message}\n{ex.StackTrace}";

                    if (ex.InnerException != null)
                        msg += $"\n\nERR2: {ex.InnerException.Message}\n{ex.InnerException.StackTrace}";

                    msg += "\n\n";

                    if (MessageBox.Show(Owner, msg, Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Warning) ==
                        MessageBoxResult.Yes) Utils.OpenLink(Program.GithubIssuesUrl);
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        private void TriggerPropertyChanged(string name)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }

        private void DiscordButtonClick(object sender, RoutedEventArgs e)
        {
            Utils.OpenLink(Program.DiscordUrl);
        }

        private void GitHubButtonClick(object sender, RoutedEventArgs e)
        {
            Utils.OpenLink(Program.GithubUrl);
        }

        private void HomePageButtonClick(object sender, RoutedEventArgs e)
        {
            Utils.OpenLink(Program.HomePageUrl);
        }
    }
}