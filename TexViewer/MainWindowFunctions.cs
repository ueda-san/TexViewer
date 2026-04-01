using Microsoft.UI.Input;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Graphics;
using Windows.Storage;

namespace TexViewer
{
    public partial class MainWindow : Window
    {
        //------------------------------------------------------------------------------
        [DllImport("User32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern int GetDpiForWindow(IntPtr hwnd);
        public float GetDisplayScale() {
            IntPtr hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            int dpi = GetDpiForWindow(hWnd);
            return dpi / 96.0f;
        }

        //------------------------------------------------------------------------------
        [DllImport("user32.dll")]
        private static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint dwFlags);
        private const uint MONITOR_DEFAULTTONULL = 0;
        private const uint MONITOR_DEFAULTTOPRIMARY = 1;
        private const uint MONITOR_DEFAULTTONEAREST = 2;

        //------------------------------------------------------------------------------
        // https://github.com/microsoft/microsoft-ui-xaml/issues/8437
        [DllImport("User32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern bool EnumDisplaySettingsEx(IntPtr lpszDeviceName, int iModeNum, ref DEVMODE lpDevMode, int dwFlags);

        public const int ENUM_CURRENT_SETTINGS = -1;
        public const int ENUM_REGISTRY_SETTINGS = -2;

        public const int EDS_RAWMODE = 0x00000002;
        public const int EDS_ROTATEDMODE = 0x00000004;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct DEVMODE
        {
            private const int CCHDEVICENAME = 32;
            private const int CCHFORMNAME = 32;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCHDEVICENAME)]
            public string dmDeviceName;
            public short dmSpecVersion;
            public short dmDriverVersion;
            public short dmSize;
            public short dmDriverExtra;
            public int dmFields;
            public int dmPositionX;
            public int dmPositionY;
            public DISPLAYORIENTATION dmDisplayOrientation;
            public int dmDisplayFixedOutput;
            public short dmColor;
            public short dmDuplex;
            public short dmYResolution;
            public short dmTTOption;
            public short dmCollate;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCHFORMNAME)]
            public string dmFormName;
            public short dmLogPixels;
            public int dmBitsPerPel;
            public int dmPelsWidth;
            public int dmPelsHeight;
            public int dmDisplayFlags;
            public int dmDisplayFrequency;
            public int dmICMMethod;
            public int dmICMIntent;
            public int dmMediaType;
            public int dmDitherType;
            public int dmReserved1;
            public int dmReserved2;
            public int dmPanningWidth;
            public int dmPanningHeight;
        }

        public enum DISPLAYORIENTATION : int
        {
            DMDO_DEFAULT = 0,
            DMDO_90 = 1,
            DMDO_180 = 2,
            DMDO_270 = 3
        }

        static (int w, int h) GetDisplayResolution()
        {
            DEVMODE devmode = new();
            devmode.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
            bool bRet = EnumDisplaySettingsEx(IntPtr.Zero, ENUM_CURRENT_SETTINGS, ref devmode, 0);
            if (!bRet)
            {
                devmode.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
                bRet = EnumDisplaySettingsEx(IntPtr.Zero, ENUM_REGISTRY_SETTINGS, ref devmode, 0);
            }
            Debug.WriteLine(devmode.dmPelsWidth+"x"+devmode.dmPelsHeight+" "+devmode.dmLogPixels);

            int w = devmode.dmPelsWidth * 96 / devmode.dmLogPixels;
            int h = devmode.dmPelsHeight * 96 / devmode.dmLogPixels;
            return (w,h);
        }


        //------------------------------------------------------------------------------
        class uvPos
        {
            public int X;
            public int Y;
            public uvPos(int x, int y) { X = x; Y = y; }
            public uvPos(float x, float y) { X = (int)Math.Round(x); Y = (int)Math.Round(y); }
            public uvPos(double x, double y) { X = (int)Math.Round(x); Y = (int)Math.Round(y); }
        }


        class uvRect
        {
            public int X;
            public int Y;
            public int W;
            public int H;
            public uvRect() { X = 0; Y = 0; W = 0; H = 0; }
            public uvRect(int x, int y, int w, int h) { X = x; Y = y; W = w; H = h; }
            public bool IsEmpty { get { return (W == 0 || H == 0); } }
            public void Clear() { W=0; H=0; }
        }

        uvPos Client2uv(Point p, bool round=false) {
            if (round){
                return new uvPos(Math.Round((p.X - imageOffsetX) / zoom),
                                 Math.Round((p.Y - imageOffsetY) / zoom));
            }else{
                return new uvPos(Math.Floor((p.X - imageOffsetX) / zoom),
                                 Math.Floor((p.Y - imageOffsetY) / zoom));
            }
        }

        Point Uv2client(uvPos p) { return Uv2client(p.X, p.Y); }
        Point Uv2client(int x, int y) {
            return new Point(x * zoom + imageOffsetX, y * zoom + imageOffsetY);
        }


        //------------------------------------------------------------------------------
        void SetTitle()
        {
            if (texImage != null && texImage.valid){
                string str = Path.GetFileName(texImage.FullPath);
                if (texImage.UndoableCount() > 0) str+="*";
                str += $" ({texImage.Width}x{texImage.Height})";
                if (texImage.HasAnimation){
                    str += $" [{texImage.CurrenPage}/{texImage.TotalPages}]";
                }
                Title = str;
            }else{
                var version = Assembly.GetExecutingAssembly().GetName().Version;
                Title = $"TexViewer {version?.Major}.{version?.Minor}";
            }
        }

        uvPos prevPos = new(0,0);
        void SetInfoText(){ SetInfoText(prevPos); }

        void SetInfoText(uvPos pos)
        {
            if (texImage.valid){
                string strX = ((pos.X < 0) || (pos.X >= texImage.Width)) ? "-" : pos.X.ToString();
                string strY = ((pos.Y < 0) || (pos.Y >= texImage.Height)) ? "-" : pos.Y.ToString();
                string colorStr = texImage.GetRGB(pos.X, pos.Y);
                InfoTextPos.Text = $"({strX},{strY})";
                InfoTextCol.Text = $" {colorStr}";
                if (hdrSwapChain.IsHDR){
                    InfoTextCol.Foreground = new SolidColorBrush(Microsoft.UI.Colors.CornflowerBlue);
                }else{
                    InfoTextCol.Foreground = Util.textBrush;
                }
                prevPos = pos;
            }else{
                InfoTextPos.Text = "";
                InfoTextCol.Text = "";
            }
        }

        void SetZoomText()
        {
            ZoomText.Text = string.Format("🔍{0:.00}%", zoom * 100);
        }

        void RecalculateOverFlow(){
            MainCommandBar.IsDynamicOverflowEnabled = false;
            MainCommandBar.IsDynamicOverflowEnabled = true;
        }

        void SetSelectionText(bool reset=false, bool withoutDraw=false) {
            string rectStr = "";
            if (reset){
                selectionRect.Clear();
                if (!withoutDraw) DrawImage();
                RecalculateOverFlow();
            }
            if (!selectionRect.IsEmpty) {
                rectStr = $"✏️({selectionRect.X},{selectionRect.Y}) {selectionRect.W}x{selectionRect.H}";
            }
            RectText.Text = rectStr;
        }

        void UpdateContextMenu()
        {
            bool imageLoaded = texImage.valid;
            FlyoutResize.IsEnabled = imageLoaded;
            FlyoutRotate.IsEnabled = imageLoaded;
            FlyoutFlip.IsEnabled = imageLoaded;
            FlyoutInfo.IsEnabled = imageLoaded;
            FlyoutSave.IsEnabled = imageLoaded;
            FlyoutAnimation.IsEnabled = texImage.HasAnimation;
            if (animationEnable){
                FlyoutAnimation.Visibility = Visibility.Visible;
                FlyoutAnimationPause.Visibility = Visibility.Collapsed;
            }else{
                FlyoutAnimation.Visibility = Visibility.Collapsed;
                FlyoutAnimationPause.Visibility = Visibility.Visible;
            }
            bool rectSelected = !selectionRect.IsEmpty;
            FlyoutCrop.IsEnabled = rectSelected;
            FlyoutMosaic.IsEnabled = rectSelected;

            FlyoutUndo.IsEnabled = (texImage.UndoableCount() > 0);
        }

        //------------------------------------------------------------------------------

        /// <summary>
        /// Adjust the image so it doesn't go outside the screen
        /// </summary>
        void StayOnScreen() {
            const float border = 4;
            float width = (float)clientWidth;
            float height = (float)clientHeight;
            float left = imageOffsetX;
            float right = imageOffsetX + texImage.Width * zoom;
            float top = imageOffsetY;
            float bottom = imageOffsetY + texImage.Height * zoom;
            if (left > width - border) imageOffsetX = width - border;
            if (right < border) imageOffsetX = border - texImage.Width * zoom;
            if (top > height - border) imageOffsetY = height - border;
            if (bottom < border) imageOffsetY = border - texImage.Height * zoom;
        }


        // ScaleLargeImage == true の時、
        // 拡大時は画面をはみ出さないように、縮小時ははみ出さない限り指定zoomを維持
        float GetSuitableZoom(float _zoom){
            if (fitToWindow) return _zoom;
            var res = GetDisplayResolution();
            var zoom80 = Math.Min(res.w * 0.8f / texImage.Width, res.h * 0.8f / texImage.Height);
            if (preference.ScaleLargeImage) {
                if (_zoom < 1.0f){
                    return Math.Min(1.0f, zoom80);
                }else{
                    return Math.Min(_zoom, zoom80);
                }
            }else{
                // 自動縮小なしの場合は拡大ではみ出す場合のみ補正
                if (_zoom <= 1.0f){
                    return _zoom;
                }else{
                    return Math.Min(_zoom, zoom80);
                }
            }
        }

        void ZoomToWindow() {
            float wrate = (float)clientWidth / texImage.Width;
            float hrate = (float)clientHeight / texImage.Height;
            zoom = Math.Min(wrate, hrate);
            imageOffsetX = (float)(clientWidth - texImage.Width * zoom) / 2;
            imageOffsetY = (float)(clientHeight - texImage.Height * zoom) / 2;
            SetZoomText();
            //drawZoomOrRect();
        }

        void FitWindowToImage() {
            Debug.WriteLine("***** FitWindowToImage");
            if (fitToWindow) {
                ZoomToWindow();
            } else {
                uint windowMinWidth = 117; // FIXME!

                float ds = GetDisplayScale();
                int width = (int)(texImage.Width * ds * zoom);
                int height = (int)(texImage.Height * ds * zoom + commandBarHeight * ds);
                AppWindow.ResizeClient(new SizeInt32(width, height));
                clientWidth = (uint)Math.Max(texImage.Width * zoom, windowMinWidth);
                clientHeight = (uint)(texImage.Height * zoom);

                float minWidth =  windowMinWidth * ds;
                imageOffsetX = (width < minWidth) ? (minWidth - width) / ds / 2 : 0.0f;
                imageOffsetY = 0.0f;
                //zoom = 1.0f;
                SetZoomText();

                prevSize = new Size(0, 0); // avoid re-centering
            }
        }

        /// <summary>
        /// Expand the window to be larger than the dialog
        /// </summary>
        /// <param name="minW">dialog width (w/o display scale)</param>
        /// <param name="minH">dialog height (w/o display scale)</param>
        void EnlargeWindow(uint minW, uint minH)
        {
            float ds = GetDisplayScale();
            minH += (uint)(commandBarHeight * ds);
            if (AppWindow.ClientSize.Width < minW * ds ||
                AppWindow.ClientSize.Height < minH * ds) {
                if (prevSize.Width == 0 || prevSize.Height == 0) {
                    prevSize.Width = AppWindow.ClientSize.Width / ds;
                    prevSize.Height = AppWindow.ClientSize.Height / ds;
                }
                int w = Math.Max((int)AppWindow.ClientSize.Width, (int)(minW * ds));
                int h = Math.Max((int)AppWindow.ClientSize.Height, (int)(minH * ds));
                AppWindow.ResizeClient(new SizeInt32(w, h));
            }
        }


        private int CheckRectResize(Point pos) {
            int ret = 0;

            if (!isSelecting && !selectionRect.IsEmpty) {
                Point lt = Uv2client(selectionRect.X, selectionRect.Y);
                Point rb = Uv2client(selectionRect.X + selectionRect.W, selectionRect.Y + selectionRect.H);
                double r = 3;
                if ((lt.X - r < pos.X) && (pos.X < rb.X + r)) {
                    if (Math.Abs(pos.Y - lt.Y) < r) ret |= 8;
                    if (Math.Abs(pos.Y - rb.Y) < r) ret |= 1;
                }
                if ((lt.Y - r < pos.Y) && (pos.Y < rb.Y + r)) {
                    if (Math.Abs(pos.X - lt.X) < r) ret |= 2;
                    if (Math.Abs(pos.X - rb.X) < r) ret |= 4;
                }
                // a8c
                // 2 4
                // 315        0 1 2 3 4 5 6 7 8 9 a b c d e f
                int[] vals = [0,2,4,1,6,3,0,0,8,0,7,0,9,0,0,0];
                ret = vals[ret];
                if ((ret == 0) &&
                    (pos.X > lt.X) &&
                    (pos.X < rb.X) &&
                    (pos.Y > lt.Y) &&
                    (pos.Y < rb.Y)) ret = 5;
                // 789
                // 456
                // 123
            }
            InputSystemCursorShape[] cursorType = [
                InputSystemCursorShape.Arrow,
                InputSystemCursorShape.SizeNortheastSouthwest,
                InputSystemCursorShape.SizeNorthSouth,
                InputSystemCursorShape.SizeNorthwestSoutheast,
                InputSystemCursorShape.SizeWestEast,
                InputSystemCursorShape.SizeAll,
                InputSystemCursorShape.SizeWestEast,
                InputSystemCursorShape.SizeNorthwestSoutheast,
                InputSystemCursorShape.SizeNorthSouth,
                InputSystemCursorShape.SizeNortheastSouthwest,
            ];

            RootGrid.InputCursor = InputSystemCursor.Create(cursorType[ret]);
            return ret;
        }

        private void SelectionResize(Point pos) {
            if (selectionResizing != 0) {
                int x = Math.Clamp(Client2uv(pos, true).X, 0, (int)texImage.Width);
                int y = Math.Clamp(Client2uv(pos, true).Y, 0, (int)texImage.Height);
                int top = selectionRect.Y;
                int btm = selectionRect.Y + selectionRect.H;
                int left = selectionRect.X;
                int right = selectionRect.X + selectionRect.W;

                if (selectionResizing <= 3) {
                    if (y < selectionRect.Y) selectionResizing += 6;
                    selectionRect.H = y - top;
                }else if (selectionResizing >= 7) {
                    if (y > selectionRect.Y + selectionRect.H) selectionResizing -= 6;
                    selectionRect.Y = y;
                    selectionRect.H = btm - y;
                }

                if (selectionResizing % 3 == 0) {
                    if (x < selectionRect.X) selectionResizing -= 2;
                    selectionRect.W = x - left;
                }
                if (selectionResizing % 3 == 1) {
                    if (x > selectionRect.X + selectionRect.W) selectionResizing += 2;
                    selectionRect.X = x;
                    selectionRect.W = right - x;
                }

                if (selectionResizing == 5) {
                    selectionRect.X += x - selectionStart.X;
                    selectionRect.Y += y - selectionStart.Y;
                    if (selectionRect.X < 0) selectionRect.X = 0;
                    if (selectionRect.Y < 0) selectionRect.Y = 0;
                    if (selectionRect.X + selectionRect.W > texImage.Width) {
                        selectionRect.X = (int)texImage.Width - selectionRect.W;
                    }
                    if (selectionRect.Y + selectionRect.H > texImage.Height) {
                        selectionRect.Y = (int)texImage.Height - selectionRect.H;
                    }
                    selectionStart.X = x;
                    selectionStart.Y = y;
                }
            }
        }

        //------------------------------------------------------------------------------

        void OpenFiles(List<string> files, bool withDisabledPlugin=false) {
            if (files.Count == 0) return;
            OpenFileAsync(files[0], withDisabledPlugin);
            if (files.Count == 1) return;

            int ofs = (int)(GetDisplayScale() * 24);
            PointInt32 pos = AppWindow.Position;
            for (int i=1; i<files.Count; i++){
                pos.X += ofs;
                pos.Y += ofs;
                ProcessStartInfo startInfo = new ProcessStartInfo {
                    FileName = Environment.ProcessPath,
                    Arguments = $"--pos {pos.X},{pos.Y} \"{files[i]}\"",
                    UseShellExecute = false,
                };
                try{
                    Process.Start(startInfo);
                }catch(Exception ex){
                    Debug.WriteLine(ex.Message);
                }
            }
        }

        async void OpenFileAsync(string path, bool withDisabledPlugin=false) {
            uint opt = ((preference.ShowAllExif?0x0001u:0x0000u) |
                        (!hdrSwapChain.enableHDR?0x0002u:0x0000u));
            TexImage? img = null;
            RootGrid.SetBusy(true);
            //await Task.Yield();

            await Task.Run(() => {
                img = ImageLoader.Load(path, opt, withDisabledPlugin);
            });
            RootGrid.SetBusy(false);

            if (img != null) {
                texImage?.Dispose();
                texImage = img;
                string ext = Path.GetExtension(path).ToLower();
                if ((preference.FlipAstc && ext.Equals(".astc"))||
                    (preference.FlipBasis && ext.Equals(".basis"))||
                    (preference.FlipDds && ext.Equals(".dds"))||
                    (preference.FlipKtx && ext.Equals(".ktx"))||
                    (preference.FlipKtx2 && ext.Equals(".ktx2"))||
                    (preference.FlipPvr && ext.Equals(".pvr"))){
                    texImage.FlipV();
                }
                AfterLoadSetup();
            } else {
                EnlargeWindow(400,200);
                ContentDialog info = new () {
                    XamlRoot = this.Content.XamlRoot,
                    Title = "Load Error",
                    Content = Util.GetResourceString("NoPluginToRead")+" '"+path+"'",
                    PrimaryButtonText = "Ok",
                };
                await info.ShowAsync();
            }
            if (path.StartsWith(ApplicationData.Current.TemporaryFolder.Path)){
                File.Delete(path);  // remove virtual file
            }
        }

        void AfterLoadSetup() {
            ErrorBar.Message = "";
            ErrorBar.IsOpen = false;
            hdrSwapChain.CheckHDR(this.AppWindow); //reset internal state only
            SetBitmap(true);
            if (texImage.maxCLL == 0.0f){
                if (texImage.BitDepth > 8){
                    texImage.maxCLL = hdrSwapChain.ComputeMaxCLL(texImage.Width, texImage.Height, (uint)texImage.ColorSpace, (uint)texImage.GetTransferFunc());
                }
            }

            zoom = GetSuitableZoom(1.0f);
            FitWindowToImage();
            SetSelectionText(true, true);

            hdrSwapChain.CreateSwapChain(this.AppWindow, ref ImagePanel, texImage.IsSDR, clientWidth, clientHeight, false);
            DrawImage(true);  //TODO: サイズが違うと RootGrid_SizeChanged でも呼ばれる

            if (texImage.HasAnimation) {
                FlyoutAnimation.IsEnabled = true;
                FlyoutAnimation.IsChecked = true;
                if (texImage.GetAnimationDelay() != 0){
                    animationEnable = true;
                    timer.Interval = TimeSpan.FromMilliseconds(texImage.GetAnimationDelay() * 10);
                    timer.Start();
                }else{
                    animationEnable = false;
                    timer.Stop();
                }
            }else{
                FlyoutAnimation.IsEnabled = false;
                FlyoutAnimation.IsChecked = false;
                animationEnable = true;
                timer.Stop();
            }
            UpdateContextMenu();
            SetTitle();
            SetInfoText();
            dirCache.Clear();
            InfoTextPos.Text = $"(-,-)";
            InfoTextCol.Text = "";
        }
    }
}
