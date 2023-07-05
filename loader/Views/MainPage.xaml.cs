using System;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;

using PenguLoader.Main;

namespace PenguLoader.Views
{
    public partial class MainPage : Page, INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        void TriggerPropertyChanged(string name) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        Window Owner => Window.GetWindow(this);

        public bool OptimizeClient
        {
            get => Config.OptimizeClient;
            set
            {
                if (value == true)
                {
                    var caption = App.GetTranslation("t_optimize_client");
                    var message = App.GetTranslation("t_msg_optimize_client_prompt");

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
                if (value == true)
                {
                    var caption = App.GetTranslation("t_super_potato_mode");
                    var message = App.GetTranslation("t_msg_super_potato_mode_prompt");

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
                        MessageBox.Show(Owner, App.GetTranslation("t_msg_module_not_found"),
                             Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);
                    }
                    else
                    {
                        Module.SetActive(value);
                        TriggerPropertyChanged(nameof(IsActivated));

                        if ((value && LCU.IsRunning) || (!value && Module.IsLoaded))
                        {
                            if (MessageBox.Show(Owner, App.GetTranslation("t_msg_restart_client"),
                                Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                            {
                                LCU.KillUxAndRestart();
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    var msg = App.GetTranslation("t_msg_activation_fail");
                    msg += string.Format("\n\nERR: {0}\n{1}", ex.Message, ex.StackTrace);

                    if (ex.InnerException != null)
                        msg += string.Format("\n\nERR2: {0}\n{1}", ex.InnerException.Message, ex.InnerException.StackTrace);

                    msg += "\n\n";

                    if (MessageBox.Show(Owner, msg, Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes)
                    {
                        Utils.OpenLink(Program.GithubIssuesUrl);
                    }
                }
            }
        }

        public MainPage()
        {
            InitializeComponent();
            DataContext = this;
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