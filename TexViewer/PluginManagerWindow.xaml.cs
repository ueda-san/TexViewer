using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Windows.Graphics;
using WinRT.Interop;

namespace TexViewer
{
    public class Plugin(String name, String version, String description, string extensions)
    {
        public String PluginName { get; set; } = name;
        public String PluginVersion { get; set; } = version;
        public String PluginDescription { get; set; } = description;
        public String Extensions { get; set; } = extensions;
        public bool Enabled { get; set; } = true;
    }

    public sealed partial class PluginManagerWindow : Window
    {
        public ObservableCollection<Plugin> PluginSource { get; } = new();

        public PluginManagerWindow(AppWindow parent) {
            InitializeComponent();
            PluginManagerHint.Background = Util.SetThemeColors(this);

            RectInt32 rect = new RectInt32() { Width = 850, Height = 900, X = parent.Position.X + 40, Y = parent.Position.Y + 40 };
            this.AppWindow.MoveAndResize(rect);

            var pluginManager = PluginManager.Instance;
            var pluginList = pluginManager.GetPluginList(true);
            foreach (var name in pluginList) {
                var p = pluginManager.GetPluginInfo(name);
                if (p != null) {
                    PluginSource.Add(new Plugin(p.Plugin.PluginName,
                                                p.Plugin.PluginVersion,
                                                p.Plugin.PluginDescription,
                                                p.Plugin.SupportedExtensions) { Enabled = p.Enabled });
                }
            }
        }

        public IntPtr GetWindowHandle() {
            return WindowNative.GetWindowHandle(this);
        }

        private void ToggleSwitch_Toggled(object sender, RoutedEventArgs e) {
            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;
            if (toggleSwitch != null) {
                var plugin = toggleSwitch.DataContext as Plugin;
                if (plugin != null) {
                    var pluginManager = PluginManager.Instance;
                    pluginManager.SetPluginEnabled(plugin.PluginName, toggleSwitch.IsOn);

                    int idx = PluginSource.IndexOf(plugin);
                    if (idx >= 0) PluginSource[idx].Enabled = toggleSwitch.IsOn;
                }
            }
        }

        private void PluginList_DragItemsCompleted(ListViewBase sender, DragItemsCompletedEventArgs args) {
            var pluginManager = PluginManager.Instance;
            List<string> order = new();
            foreach (var item in PluginSource) {
                order.Add(item.PluginName);
            }
            pluginManager.UpdateOrder(order);
        }

        private void PluginManagerOK_Click(object sender, RoutedEventArgs e) {
            var pluginManager = PluginManager.Instance;
            pluginManager.SaveOrder();
            this.Close();
        }

        private void Grid_ActualThemeChanged(FrameworkElement sender, object args) {
            PluginManagerHint.Background = Util.SetThemeColors(this);
        }
    }
}
