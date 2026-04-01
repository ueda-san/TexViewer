using Microsoft.UI.Xaml.Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace TexViewer
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class ColorPickerContent : Page
    {
        MainWindow mainWindow;
        public Windows.UI.Color PickedColor { get; set; }

        public ColorPickerContent(MainWindow _mainWindow) {
            this.InitializeComponent();
            mainWindow = _mainWindow;
        }

        private void ColorPicker_ColorChanged(ColorPicker sender, ColorChangedEventArgs args) {
            mainWindow.outsideColor = PickedColor;
            mainWindow.DrawImage();
        }
    }
}
