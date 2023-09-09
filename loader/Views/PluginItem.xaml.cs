using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using PenguLoader.Main;

namespace PenguLoader.Views
{
    public partial class PluginItem
    {
        public PluginItem()
        {
            InitializeComponent();
        }

        public PluginItem(Plugins.PluginInfo plugin)
        {
            InitializeComponent();

            TName.IsChecked = plugin.Enabled;
            ((TextBlock)TName.Content).Text = plugin.Name;
            TName.Click += delegate { Plugins.Toggle(plugin); };

            if (string.IsNullOrEmpty(plugin.Author) && string.IsNullOrEmpty(plugin.Link))
            {
                TLink.Visibility = Visibility.Collapsed;
            }
            else
            {
                TLink.Text = !string.IsNullOrEmpty(plugin.Author) ? plugin.Author : "Source";

                if (string.IsNullOrEmpty(plugin.Link)) return;
                TLink.Cursor = Cursors.Hand;
                TLink.MouseUp += delegate { Utils.OpenLink(plugin.Link); };
                TLink.Foreground = new SolidColorBrush(Colors.SeaGreen);
            }
        }
    }
}