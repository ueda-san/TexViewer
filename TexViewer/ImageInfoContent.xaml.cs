using Microsoft.UI.Xaml.Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace TexViewer
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class ImageInfoContent : Page
    {
        public ImageInfoContent(string gInfo, string lInfo, string tInfo)
        {
            this.InitializeComponent();
            GeneralInfo.Text = gInfo;
            LoaderInfo.Text = lInfo;
            TexViewerInfo.Text = tInfo;

        }
    }
}
