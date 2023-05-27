using System;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader
{
    public partial class PluginItem : UserControl
    {
        public PluginItem()
        {
            InitializeComponent();
        }

        public PluginItem(Plugins.PluginInfo plugin)
        {
            InitializeComponent();

            tName.Content = plugin.Name;
            tName.IsChecked = plugin.Enabled;
            tName.Click += delegate { Plugins.Toggle(plugin); };

            if (string.IsNullOrEmpty(plugin.Author) && string.IsNullOrEmpty(plugin.Link))
            {
                tLink.Visibility = Visibility.Collapsed;
            }
            else
            {
                if (!string.IsNullOrEmpty(plugin.Author))
                {
                    tLink.Text = plugin.Author;
                }
                else
                {
                    tLink.Text = "Link";
                }

                if (!string.IsNullOrEmpty(plugin.Link))
                {
                    tLink.Cursor = Cursors.Hand;
                    tLink.MouseUp += delegate { Utils.OpenLink(plugin.Link); };
                    tLink.Foreground = new SolidColorBrush(Colors.DeepSkyBlue);
                }
            }
        }
    }
}