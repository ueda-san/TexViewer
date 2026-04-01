using ImageMagick;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.Windows.ApplicationModel.Resources;
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using TexViewerPlugin;

using Windows.Graphics.Imaging;
using Windows.Storage;
using WinRT.Interop;

namespace TexViewer
{
    public struct FormatInfo(Format f, uint b, uint d, MagickFormat m, StorageType s, string p)
    {
        public Format format = f;
        public uint bytePerPixel = b;
        public uint depth = d;
        public MagickFormat magickFormat = m;
        public StorageType storageType = s;
        public string pixelMappig = p;
    }


    public class Util
    {
        static readonly ResourceLoader resourceLoader = new();
        public static SolidColorBrush textBrush = new(Microsoft.UI.Colors.Black);

        public static SolidColorBrush SetThemeColors(Window window)
        {
            Application app = Application.Current;
            var fg = (SolidColorBrush)app.Resources["MyForeground"];
            var bg = (SolidColorBrush) app.Resources["MyBackground"];
            var fgInactive = (SolidColorBrush) app.Resources["MyForegroundDisabled"];
            var bgInactive = (SolidColorBrush) app.Resources["MyBackgroundDisabled"];
            var bgHover = (SolidColorBrush) app.Resources["MyHoverBackground"];
            var bgPressed = (SolidColorBrush) app.Resources["MyPressedBackground"];
            textBrush = fg;

            if (AppWindowTitleBar.IsCustomizationSupported()) {
                IntPtr hWnd = WindowNative.GetWindowHandle(window);
                WindowId wndId = Win32Interop.GetWindowIdFromWindow(hWnd);
                AppWindow appWindow = AppWindow.GetFromWindowId(wndId);
                AppWindowTitleBar titleBar = appWindow.TitleBar;

                titleBar.ForegroundColor = fg.Color;
                titleBar.BackgroundColor = bg.Color;
                titleBar.InactiveForegroundColor = fgInactive.Color;
                titleBar.InactiveBackgroundColor = bgInactive.Color;

                titleBar.ButtonForegroundColor = fg.Color;
                titleBar.ButtonBackgroundColor = bg.Color;
                titleBar.ButtonInactiveForegroundColor = fgInactive.Color;
                titleBar.ButtonInactiveBackgroundColor = bgInactive.Color;
                titleBar.ButtonHoverForegroundColor = fg.Color;
                titleBar.ButtonHoverBackgroundColor = bgHover.Color;
                titleBar.ButtonPressedForegroundColor = fg.Color;
                titleBar.ButtonPressedBackgroundColor = bgPressed.Color;

                Grid grid = (Grid)window.Content;
                ElementTheme theme = grid.ActualTheme;
                if (grid.ActualTheme == ElementTheme.Default){
                    ApplicationTheme appTheme = App.Current.RequestedTheme;
                    theme = (appTheme==ApplicationTheme.Dark)?ElementTheme.Dark:ElementTheme.Light;
                }
                string iconFileName = "Assets/TexViewer.ico";
                string path = AppContext.BaseDirectory;
                string iconPath = Path.Combine(path, iconFileName);
                appWindow.SetIcon(iconPath);
            }
            return bg;
        }

        public static async Task<byte[]> LoadBytesAsync(string filename)
        {
            try {
                Uri uri = new("ms-appx:///Assets/" + filename);
                var file = await StorageFile.GetFileFromApplicationUriAsync(uri);
                using (var stream = await file.OpenStreamForReadAsync()) {
                    var bytes = new byte[(int)stream.Length];
                    await stream.ReadExactlyAsync(bytes.AsMemory(0, (int)stream.Length));
                    return bytes;
                }
            }catch (Exception e){
                Debug.WriteLine("LoadBytesAsync failed:" + filename + " " + e.Message);
                throw new Exception("shader load failed");
            }
        }

        public static string GetResourceString(string key)
        {
            return resourceLoader.GetString(key);
        }


        public static Format ConvertFormat(BitmapPixelFormat fmt){ // for Ctrl-V
            switch (fmt){
              case BitmapPixelFormat.Gray8:
                return Format.A8;
              case BitmapPixelFormat.Gray16:
                return Format.R16;
              case BitmapPixelFormat.Bgra8:
                return Format.BGRA8888;
              case BitmapPixelFormat.Rgba8:
                return Format.RGBA8888;
              case BitmapPixelFormat.Rgba16:
                return Format.RGBA16;
              default:
                break;
            }
            return Format.UNKNOWN;
        }

        public static FormatInfo GetFormatInfo(Format fmt) {
            FormatInfo[] formatInfos = {
                new (Format.UNKNOWN,   0,  0, MagickFormat.Unknown, StorageType.Undefined, "?"),
                new (Format.AUTO,      0,  0, MagickFormat.Unknown, StorageType.Undefined, "?"),
                new (Format.A8,        1,  8, MagickFormat.A,       StorageType.Char,      "A"),
                new (Format.R8,        1,  8, MagickFormat.R,       StorageType.Char,      "R"),
                new (Format.R16,       2, 16, MagickFormat.R,       StorageType.Short,     "R"),
                new (Format.RG16,      4, 16, MagickFormat.R,       StorageType.Short,     "RG"),
                new (Format.RGB565,    2,  0, MagickFormat.Unknown, StorageType.Undefined, "RGB"),
                new (Format.RGBA5551,  2,  0, MagickFormat.Unknown, StorageType.Undefined, "RGBA"),
                new (Format.RGBA4444,  2,  0, MagickFormat.Unknown, StorageType.Undefined, "RGBA"),
                new (Format.RGB888,    3,  8, MagickFormat.Rgb,     StorageType.Char,      "RGB"),
                new (Format.RGBA8888,  4,  8, MagickFormat.Rgba,    StorageType.Char,      "RGBA"),
                new (Format.BGRA8888,  4,  8, MagickFormat.Rgba,    StorageType.Char,      "BGRA"),
                new (Format.RGB16,     6, 16, MagickFormat.Rgb,     StorageType.Short,     "RGB"),
                new (Format.RGB16F,    6, 16, MagickFormat.Unknown, StorageType.Undefined, "RGB"),
                new (Format.RGB32F,   12, 32, MagickFormat.Rgb,     StorageType.Float,     "RGB"),
                new (Format.RGBA16,    8, 16, MagickFormat.Rgba,    StorageType.Short,     "RGBA"),
                new (Format.RGBA16F,   8, 16, MagickFormat.Unknown, StorageType.Undefined, "RGBA"),
                new (Format.RGBA32F,  16, 32, MagickFormat.Rgba,    StorageType.Float,     "RGBA"),
                new (Format.RGBA1010102,8,10, MagickFormat.Rgba,    StorageType.Undefined, "RGBA"),
            };

            foreach (FormatInfo finfo in formatInfos) {
                if (finfo.format == fmt) return finfo;
            }
            return formatInfos[0];
        }

        public static uint GetBytePerPixel(Format fmt)  {
            return GetFormatInfo(fmt).bytePerPixel;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr LoadLibrary(string lpFileName);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool FreeLibrary(IntPtr hModule);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr FindResource(IntPtr hModule, IntPtr lpName, IntPtr lpType);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr LoadResource(IntPtr hModule, IntPtr hResInfo);
        [DllImport("kernel32.dll")]
        private static extern IntPtr LockResource(IntPtr hResData);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint SizeofResource(IntPtr hModule, IntPtr hResInfo);

        public static byte[] LoadResourceFromDll(string dllPath, int resourceId) {
            IntPtr RT_RCDATA = new IntPtr(10);
            IntPtr hModule = LoadLibrary(dllPath);
            if (hModule == IntPtr.Zero) throw new Exception("DLL load failed: " + Marshal.GetLastWin32Error());
            try {
                IntPtr hResInfo = FindResource(hModule, resourceId, RT_RCDATA);
                if (hResInfo == IntPtr.Zero) throw new Exception("resource not found: " + Marshal.GetLastWin32Error());

                IntPtr hResData = LoadResource(hModule, hResInfo);
                if (hResData == IntPtr.Zero) throw new Exception("resource load failed: " + Marshal.GetLastWin32Error());

                IntPtr pResData = LockResource(hResData);
                uint size = SizeofResource(hModule, hResInfo);

                byte[] bytes = new byte[size];
                Marshal.Copy(pResData, bytes, 0, (int)size);
                return bytes;
            }finally{
                FreeLibrary(hModule);
            }
        }

    }
}
