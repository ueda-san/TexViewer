using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using System;
using System.Diagnostics;
using System.Text.Json;


namespace TexViewer
{
    public class Preference {
        public bool ShowTutorial { get; set; } = true;
        public bool ScaleLargeImage { get; set; } = true;
        public bool NearestNeighbor { get; set; } = true;
        public bool ShowAllExif { get; set; } = false;
        public double MosaicSize { get; set; } = 14.0;
        public int AlphaMode { get; set; } = 2;
        public bool FlipAstc { get; set; } = false;
        public bool FlipBasis { get; set; } = false;
        public bool FlipDds { get; set; } = false;
        public bool FlipKtx { get; set; } = false;
        public bool FlipKtx2 { get; set; } = false;
        public bool FlipPvr { get; set; } = false;
        public int MaxLuminanceIndex { get; set; } = 0;
        //public int WhiteLevelIndex { get; set; } = 0;
        public int ToneMapType { get; set; } = 4;
        public double Exposure { get; set; } = 0.0;

        public float MaxLuminance { get { return maxLuminance[MaxLuminanceIndex]; } }
        float[] maxLuminance = {
            0f,  // auto
            80f,
            250f,
            400f,
            600f,
            1000f,
            1400f,
        };
    }

    public class FloatValueConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is double d) return d.ToString("F3");
            string? str = value.ToString();
            if (str == null) return "";
            return str;
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }

    public sealed partial class PreferenceContent : Page
    {
        const string SaveDataKey = "Preference";
        MainWindow mainWindow;
        Preference preference;

        public PreferenceContent(MainWindow _mainWindow, bool enableHDR)
        {
            this.InitializeComponent();
            mainWindow = _mainWindow;
            preference = mainWindow.preference;

            AlphaMode.Items.Add(new ComboBoxItem {
                    Content = Util.GetResourceString("PreferenceAlphaModeIgnore") });
            AlphaMode.Items.Add(new ComboBoxItem {
                    Content = Util.GetResourceString("PreferenceAlphaModeLinear") });
            AlphaMode.Items.Add(new ComboBoxItem {
                    Content = Util.GetResourceString("PreferenceAlphaModeGamma") });
            AlphaMode.SelectedIndex = preference.AlphaMode;

            MaxLuminance.Items.Add(new ComboBoxItem {
                    Content = Util.GetResourceString("PreferenceMaxLuminanceAuto") });
            MaxLuminance.Items.Add(new ComboBoxItem { Content = "80nit" });
            MaxLuminance.Items.Add(new ComboBoxItem { Content = "250nit" });
            MaxLuminance.Items.Add(new ComboBoxItem { Content = "400nit" });
            MaxLuminance.Items.Add(new ComboBoxItem { Content = "600nit" });
            MaxLuminance.Items.Add(new ComboBoxItem { Content = "1000nit" });
            MaxLuminance.Items.Add(new ComboBoxItem { Content = "1400nit" });
            MaxLuminance.SelectedIndex = preference.MaxLuminanceIndex;

            ToneMap.Items.Add(new ComboBoxItem {
                    Content = Util.GetResourceString("PreferenceTonemapDisable") });
            ToneMap.Items.Add(new ComboBoxItem { Content = "Reinhard" });
            ToneMap.Items.Add(new ComboBoxItem { Content = "ReinhardJodie" });
            ToneMap.Items.Add(new ComboBoxItem { Content = "Hable" });
            ToneMap.Items.Add(new ComboBoxItem { Content = "ACES" });
            ToneMap.Items.Add(new ComboBoxItem { Content = "Lottes" });
            ToneMap.SelectedIndex = preference.ToneMapType;

            Tutorial.IsChecked = preference.ShowTutorial;
            LargeImage.IsChecked = preference.ScaleLargeImage;
            NearestNeighbor.IsChecked = preference.NearestNeighbor;
            ShowAllExif.IsChecked = preference.ShowAllExif;
            MosaicSize.Value = preference.MosaicSize;
            FlipAstc.IsChecked = preference.FlipAstc;
            FlipBasis.IsChecked = preference.FlipBasis;
            FlipDds.IsChecked = preference.FlipDds;
            FlipKtx.IsChecked = preference.FlipKtx;
            FlipKtx2.IsChecked = preference.FlipKtx2;
            FlipPvr.IsChecked = preference.FlipPvr;
            Exposure.Value = preference.Exposure;

            grayedoutHDR(enableHDR);
        }

        public static Preference LoadPreference(){
            Preference preference = new ();
            var localSettings = Windows.Storage.ApplicationData.Current.LocalSettings;
            Object x = localSettings.Values[SaveDataKey];
            if (x != null){
                try {
                    var pref = JsonSerializer.Deserialize((string)x, PreferenceJsonContext.Default.Preference);
                    if (pref != null) preference = pref;
                }catch(Exception ex){
                    Debug.WriteLine(ex.Message);
                }
            }
            return preference;
        }

        public void SavePreference(){
            preference.ShowTutorial = Tutorial.IsChecked ?? preference.ShowTutorial;
            preference.ScaleLargeImage = LargeImage.IsChecked ?? preference.ScaleLargeImage;
            preference.NearestNeighbor = NearestNeighbor.IsChecked ?? preference.NearestNeighbor;
            preference.ShowAllExif = ShowAllExif.IsChecked ?? preference.ShowAllExif;
            preference.MosaicSize = MosaicSize.Value;
            preference.AlphaMode = AlphaMode.SelectedIndex;
            preference.FlipAstc = FlipAstc.IsChecked ?? preference.FlipAstc;
            preference.FlipBasis = FlipBasis.IsChecked ?? preference.FlipBasis;
            preference.FlipDds = FlipDds.IsChecked ?? preference.FlipDds;
            preference.FlipKtx = FlipKtx.IsChecked ?? preference.FlipKtx;
            preference.FlipKtx2 = FlipKtx2.IsChecked ?? preference.FlipKtx2;
            preference.FlipPvr = FlipPvr.IsChecked ?? preference.FlipPvr;
            preference.MaxLuminanceIndex = MaxLuminance.SelectedIndex;
            //preference.WhiteLevelIndex = WhiteLevel.SelectedIndex;
            preference.ToneMapType = ToneMap.SelectedIndex;
            preference.Exposure = Exposure.Value;

            var localSettings = Windows.Storage.ApplicationData.Current.LocalSettings;
            try {
                string json = JsonSerializer.Serialize(preference, PreferenceJsonContext.Default.Preference);
                localSettings.Values[SaveDataKey] = json;
            }catch(Exception ex){
                Debug.WriteLine(ex.Message);
            }

        }

		void grayedoutHDR(bool isEnable){
			MaxLuminance.IsEnabled = isEnable;
			ToneMap.IsEnabled = isEnable;
		}

        //------------------------------------------------------------------------------
        private void NearestNeighbor_Checked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e) {
            if (preference != null){
                preference.NearestNeighbor = true;
                mainWindow.SetSampler();
                mainWindow.DrawImage();
            }
        }

        private void NearestNeighbor_Unchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e) {
            if (preference != null){
                preference.NearestNeighbor = false;
                mainWindow.SetSampler();
                mainWindow.DrawImage();
            }
        }

        private void AlphaMode_SelectionChanged(object sender, SelectionChangedEventArgs e) {
            if (preference != null){
                preference.AlphaMode = AlphaMode.SelectedIndex;
                mainWindow.DrawImage();
            }
        }

        private void MaxLuminance_SelectionChanged(object sender, Microsoft.UI.Xaml.RoutedEventArgs e) {
            if (preference != null){
                preference.MaxLuminanceIndex = MaxLuminance.SelectedIndex;
                mainWindow.DrawImage();
            }
        }

        private void Tonemap_SelectionChanged(object sender, SelectionChangedEventArgs e) {
            if (preference != null){
                preference.ToneMapType = ToneMap.SelectedIndex;
                mainWindow.DrawImage();
            }
        }

        private void ExposureTextBlock_PointerPressed(object sender, PointerRoutedEventArgs e) {
            Exposure.Value = 0.0f;
        }

        private void Exposure_ValueChanged(object sender, RangeBaseValueChangedEventArgs e) {
            if (preference != null){
                preference.Exposure = e.NewValue;
                mainWindow.DrawImage();
            }
        }

    }
}
