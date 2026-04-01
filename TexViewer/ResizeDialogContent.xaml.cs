using ImageMagick;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace TexViewer
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class ResizeDialogContent : Page
    {
        public List<string> FilterCollection { get; } = new();

        public float ImageWidth { set; get; }
        public float ImageHeight { set; get; }

        public uint DecidedWidth { set; get; }
        public uint DecidedHeight { set; get; }
        public FilterType DecidedFilterType { set; get; } = FilterType.Undefined;
        public bool IgnoreAspectRatio { get { return !keepAspect; } }

        uint widthPix = 0;
        uint heightPix = 0;
        float widthPer = 100.0f;
        float heightPer = 100.0f;
        bool loaded = false;
        bool keepAspect = true;
        Brush wPixBrush;
        Brush hPixBrush;
        SolidColorBrush errBrush;

        public ResizeDialogContent() {
            this.InitializeComponent();
            this.Loaded += ResizeDialogContent_Loaded;

            var strAuto = Util.GetResourceString("ResizeAuto");
            FilterCollection.Add(strAuto);
            foreach (FilterType type in Enum.GetValues(typeof(FilterType))) {
                if (type.ToString() != "Undefined") {
                    FilterCollection.Add(type.ToString());
                }
            }
            //this.DataContext = this;

            wPixBrush = ResizeWidthPix.BorderBrush;
            hPixBrush = ResizeHeightPix.BorderBrush;
            errBrush = new SolidColorBrush(Colors.Red);
        }

        private void ResizeDialogContent_Loaded(object sender, RoutedEventArgs e) {
            widthPix = (uint)ImageWidth;
            heightPix = (uint)ImageHeight;
            ResizeWidthPix.Text = widthPix.ToString();
            ResizeHeightPix.Text = heightPix.ToString();
            DecidedWidth = widthPix;
            DecidedHeight = heightPix;
            loaded = true;
        }
        private void RadioButton1_Checked(object sender, RoutedEventArgs e) {
            if (!loaded) return;
            ResizeWidthPix.IsEnabled = false;
            ResizeHeightPix.IsEnabled = false;
            ResizeWidthPercent.IsEnabled = true;
            ResizeHeightPercent.IsEnabled = true;
        }
        private void RadioButton2_Checked(object sender, RoutedEventArgs e) {
            if (!loaded) return;
            ResizeWidthPix.IsEnabled = true;
            ResizeHeightPix.IsEnabled = true;
            ResizeWidthPercent.IsEnabled = false;
            ResizeHeightPercent.IsEnabled = false;
        }

        private void ResizeFilter_Loaded(object sender, RoutedEventArgs e) {

        }


        float checkPercent(float val) {
            if (val < 0f) return 0f;       //    0.1% = /1000
            return val;
        }
        uint checkPixel(int val) {
            if (val < 0) return 0;         // min 1x1
            return (uint)val;
        }


        void updateValue(int fromIdx) {
            float fVal;
            float wPer = widthPer;
            float hPer = heightPer;
            int iVal;
            uint wPix = widthPix;
            uint hPix = heightPix;

            switch(fromIdx) {
              case 0:
                if (float.TryParse(ResizeWidthPercent.Text, out fVal)) wPer = checkPercent(fVal);
                wPix = (uint)(wPer * ImageWidth / 100.0f);
                if (keepAspect){
                    hPer = wPer;
                    hPix = (uint)(hPer * ImageHeight / 100.0f);
                }
                break;
              case 1:
                if (float.TryParse(ResizeHeightPercent.Text, out fVal)) hPer = checkPercent(fVal);
                hPix = (uint)(hPer * ImageHeight / 100.0f);
                if (keepAspect){
                    wPer = hPer;
                    wPix = (uint)(wPer * ImageWidth / 100.0f);
                }
                break;
              case 2:
                if (int.TryParse(ResizeWidthPix.Text, out iVal)) wPix = checkPixel(iVal);
                wPer = wPix / ImageWidth *100.0f;
                if (keepAspect){
                    hPix = (uint)(wPix * (ImageHeight / ImageWidth));
                    hPer = hPix / ImageHeight * 100.0f;
                }
                break;
              case 3:
                if (int.TryParse(ResizeHeightPix.Text, out iVal)) hPix = checkPixel(iVal);
                hPer = hPix / ImageHeight *100.0f;
                if (keepAspect){
                    wPix = (uint)(hPix * (ImageWidth / ImageHeight));
                    wPer = wPix / ImageWidth * 100.0f;
                }
                break;
            }

            if (isPair(0, fromIdx)) ResizeWidthPercent.Text = wPer.ToString();
            if (isPair(1, fromIdx)) ResizeHeightPercent.Text = hPer.ToString();
            if (isPair(2, fromIdx)) ResizeWidthPix.Text = wPix.ToString();
            if (isPair(3, fromIdx)) ResizeHeightPix.Text = hPix.ToString();

            checkError();
        }
        bool isPair(int idx, int fromIdx){
            return (fromIdx != idx)&&(((idx ^ 0x02) == fromIdx) | keepAspect);
        }

        void validate(){
            float fVal;
            float wPer = widthPer;
            float hPer = heightPer;
            int iVal;
            uint wPix = widthPix;
            uint hPix = heightPix;

            if (float.TryParse(ResizeWidthPercent.Text, out fVal)) wPer = checkPercent(fVal);
            if (float.TryParse(ResizeHeightPercent.Text, out fVal)) hPer = checkPercent(fVal);
            if (int.TryParse(ResizeWidthPix.Text, out iVal)) wPix = checkPixel(iVal);
            if (int.TryParse(ResizeHeightPix.Text, out iVal)) hPix = checkPixel(iVal);

            ResizeWidthPercent.Text = wPer.ToString();
            ResizeHeightPercent.Text = hPer.ToString();
            ResizeWidthPix.Text = wPix.ToString();
            ResizeHeightPix.Text = hPix.ToString();

            if(!checkError()) {
                DecidedWidth = wPix;
                DecidedHeight = hPix;
            }
        }

        bool checkError(){
            int iVal;
            uint wPix = widthPix;
            uint hPix = heightPix;
            if (int.TryParse(ResizeWidthPix.Text, out iVal)) wPix = checkPixel(iVal);
            if (int.TryParse(ResizeHeightPix.Text, out iVal)) hPix = checkPixel(iVal);
            ContentDialog dialog = (ContentDialog)Parent;
            if (dialog == null) return true;

            if (wPix == 0 || hPix==0){
                ResizeWidthPix.BorderBrush = (wPix == 0)?errBrush:wPixBrush;
                ResizeHeightPix.BorderBrush = (hPix == 0)?errBrush:hPixBrush;
                dialog.IsPrimaryButtonEnabled = false;
                return true;
            }else{
                ResizeWidthPix.BorderBrush = wPixBrush;
                ResizeHeightPix.BorderBrush = hPixBrush;
                dialog.IsPrimaryButtonEnabled = true;
                return false;
            }
        }


        //------------------------------------------------------------------------------
        private void ResizeWidthPercent_TextChanged(object sender, TextChangedEventArgs e) {
            if (!loaded) return;
            TextBox? textBox = sender as TextBox;
            if (textBox == null) return;
            if (textBox.FocusState != FocusState.Unfocused){
                updateValue(0);
            }
        }
        private void ResizeWidthPercent_LostFocus(object sender, RoutedEventArgs e) {
            if (!loaded) return;
            validate();
        }

        private void ResizeHeightPercent_TextChanged(object sender, TextChangedEventArgs e) {
            if (!loaded) return;
            TextBox? textBox = sender as TextBox;
            if (textBox == null) return;
            if (textBox.FocusState != FocusState.Unfocused){
                updateValue(1);
            }
        }
        private void ResizeHeightPercent_LostFocus(object sender, RoutedEventArgs e) {
            if (!loaded) return;
            validate();
        }


        //------------------------------------------------------------------------------
        private void ResizeWidthPix_TextChanged(object sender, TextChangedEventArgs e) {
            if (!loaded) return;
            TextBox? textBox = sender as TextBox;
            if (textBox == null) return;
            if (textBox.FocusState != FocusState.Unfocused){
                updateValue(2);
            }
        }
        private void ResizeWidthPix_LostFocus(object sender, RoutedEventArgs e) {
            if (!loaded) return;
            validate();
        }

        private void ResizeHeightPix_TextChanged(object sender, TextChangedEventArgs e) {
            if (!loaded) return;
            TextBox? textBox = sender as TextBox;
            if (textBox == null) return;
            if (textBox.FocusState != FocusState.Unfocused){
                updateValue(3);
            }
        }
        private void ResizeHeightPix_LostFocus(object sender, RoutedEventArgs e) {
            if (!loaded) return;
            validate();
        }

        //------------------------------------------------------------------------------
        private void ResizeAspect_Checked(object sender, RoutedEventArgs e) {
            keepAspect = true;
        }
        private void ResizeAspect_Unchecked(object sender, RoutedEventArgs e) {
            keepAspect = false;
        }

        private void ComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e) {
            if (!loaded) return;
            ComboBox? cBox = sender as ComboBox;
            if (cBox == null) return;

            if (cBox.SelectedIndex == 0) {
                DecidedFilterType = FilterType.Undefined;
            } else {
                DecidedFilterType = Enum.Parse<FilterType>((string)cBox.SelectedItem);
            }
        }

    }
}
