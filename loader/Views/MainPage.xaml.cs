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
                    if (MessageBox.Show(Owner, "Optimize Client\n\n" +
                        "It is recommended to enable this option. Enabling it will disable some unused things, " +
                        "unused background tasks, and reduce lag.\n\n" +
                        "Do you want to continue?",
                        Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Information) != MessageBoxResult.Yes)
                    {
                        value = false;
                    }
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
                    if (MessageBox.Show(Owner, "Super Low Spec Mode\n\n" +
                        "This option extends the default Low Spec Mode. " +
                        "Enabling it will disable all transition and animation effects, " +
                        "also greatly reduce lag and increase response speed.\n\n" +
                        "It's very helpful for low PC, but may cause bug. Please report your issues to help us improve this mode.\n\n" +
                        "Do you want to continue?",
                        Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Information) != MessageBoxResult.Yes)
                    {
                        value = false;
                    }
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
                        MessageBox.Show(Owner, "The \"core.dll\" is not found, please re-install the program.",
                             Program.Name, MessageBoxButton.OK, MessageBoxImage.Warning);
                    }
                    else
                    {
                        Module.SetActive(value);
                        TriggerPropertyChanged(nameof(IsActivated));

                        if ((value && LCU.IsRunning) || (!value && Module.IsLoaded))
                        {
                            if (MessageBox.Show(Owner, "Do you want to restart the running League of Legends Client now?",
                                Program.Name, MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                            {
                                LCU.KillUxAndRestart();
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    var msg = "Failed to perform activation.\nPlease capture the error message and click Yes to report it.";
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