using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader.Views
{
    public partial class PluginsPage : INotifyPropertyChanged
    {
        private bool _loading;

        public PluginsPage()
        {
            InitializeComponent();
            Loaded += RefreshPluginsClick;
            DataContext = this;
        }

        public bool IsDone => !_loading;

        public bool IsLoading
        {
            get => _loading;
            set
            {
                _loading = value;
                OnPropertyChanged(nameof(IsLoading));
                OnPropertyChanged(nameof(IsDone));
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        private void OnPropertyChanged(string name)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }

        private void ShowFolderClick(object sender, RoutedEventArgs e)
        {
            Utils.OpenFolder(Config.PluginsDir);
        }

        private async void RefreshPluginsClick(object sender, RoutedEventArgs e)
        {
            if (IsLoading) return;

            IsLoading = true;
            Plugins.Items.Clear();

            var plugins = Main.Plugins.All();
            Count.Text = plugins.Count.ToString();

            await Task.Delay(500);

            foreach (var item in plugins.Select(plugin => new PluginItem(plugin))) Plugins.Items.Add(item);

            IsLoading = false;
        }
    }
}