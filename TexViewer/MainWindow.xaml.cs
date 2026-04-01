using Microsoft.UI.Input;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.Windows.Storage.Pickers;
using NaturalSort.Extension;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Security.Principal;
using System.Threading;
using System.Threading.Tasks;
using TexViewerPlugin;
using Windows.ApplicationModel.DataTransfer;
using Windows.Foundation;
using Windows.Foundation.Metadata;
using Windows.Graphics;
using Windows.Graphics.Imaging;
using Windows.Storage;
using Windows.Storage.Streams;
using WinRT;


// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace TexViewer
{
    public sealed partial class MainWindow : Window
    {
        public Preference preference { get; private set; }
        public Windows.UI.Color outsideColor { get; set; } = Windows.UI.Color.FromArgb(255, 0x00, 0x80, 0x80);

        uint clientWidth = 256;
        uint clientHeight = 256;
        float zoom = 1.0f;
        float imageOffsetX = 0.0f;
        float imageOffsetY = 0.0f;
        bool fitToWindow = false;

        Size prevSize = new(0, 0);
        Point prevDrag;
        bool drag = false;
        float prevW = -1.0f;
        float prevH = -1.0f;
        bool readyToDraw = false;
        bool hideChecker = false;
        uvPos selectionStart = new(0,0);
        bool isSelecting = false;
        uvRect selectionRect = new();
        int selectionResizing;
        bool animationEnable = true;

        double commandBarHeight = 48; // place holder
        TexImage texImage = null!;
        List<string> dirCache = new();
        List<string> cmdlineArgs;
        DispatcherTimer timer;
        PointInt32? windowPos = null;
        HdrSwapChain hdrSwapChain;
        nint currentMonitor = 0;
        byte[]? bitmapCache;
        bool notifyHDRChanged = false;

        public MainWindow()
        {
            this.InitializeComponent();
            MainCommandBar.Background = Util.SetThemeColors(this);
            preference = PreferenceContent.LoadPreference();
            hdrSwapChain = new HdrSwapChain();
            hdrSwapChain.CheckHDR(this.AppWindow);
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            var monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            hdrSwapChain.CreateD3DDevice(monitor, preference.NearestNeighbor);

            this.SizeChanged += Window_SizeChanged;
            this.Closed += Window_Closed;
            this.Activated += Window_Activated;
            this.AppWindow.Changed += Window_Changed;

            RootGrid.SizeChanged += RootGrid_SizeChanged;
            RootGrid.PointerPressed += RootGrid_PointerPressed;
            RootGrid.PointerMoved += RootGrid_PointerMoved;
            RootGrid.PointerReleased += RootGrid_PointerReleased;
            //RootGrid.PointerExited += RootGrid_PointerExited;
            RootGrid.PointerWheelChanged += RootGrid_PointerWheelChanged;

            SetTitle();

            timer = new DispatcherTimer();
            timer.Tick += Animation;

            float ds = GetDisplayScale();
            AppWindow.ResizeClient(new SizeInt32((int)(clientWidth * ds), (int)(clientHeight * ds + commandBarHeight * ds)));

            cmdlineArgs = Environment.GetCommandLineArgs().ToList();
            string str = "# ";
            foreach(string x in cmdlineArgs) str += " "+x;
            Debug.WriteLine(str);

            cmdlineArgs.RemoveAt(0);
            for(int i=0; i<cmdlineArgs.Count-1; i++){
                if (cmdlineArgs[i].Equals("--pos")){  // TexViewer.exe --pos 10,10 a.png b.png
                    if (cmdlineArgs[i+1].Contains(',')){
                        var pos = cmdlineArgs[i+1].Split(',');
                        int x,y;
                        if (int.TryParse(pos[0], out x) && int.TryParse(pos[1], out y)){
                            windowPos = new (x, y);
                        }
                        cmdlineArgs.RemoveAt(i+1);
                    }
                    cmdlineArgs.RemoveAt(i);
                    break;
                }
            }
        }

        private bool IsRunningAsAdmin() {
            var identity = WindowsIdentity.GetCurrent();
            var principal = new WindowsPrincipal(identity);
            return principal.IsInRole(WindowsBuiltInRole.Administrator);
        }

        public void ShowError(string msg, InfoBarSeverity severity=InfoBarSeverity.Error){
            ErrorBar.Message = msg;
            ErrorBar.IsOpen = true;
            ErrorBar.Severity = severity;
            ErrorBar.Title = severity.ToString();
        }


        private void SetBitmap(bool sizeChanged=false){
            bitmapCache = texImage.ToByteArray(out var depth, TexImage.ConvOption.HalforRGBA8);
            hdrSwapChain.SetBitmap(texImage.Width, texImage.Height, bitmapCache);
        }

        private void Animation(object? sender, object e){
            if ((texImage != null)&&(texImage.HasAnimation)){
                texImage.NextFrame();
                SetBitmap();
                DrawImage(true);
                SetTitle();
                SetInfoText();
                if (animationEnable) {
                    uint delay = texImage.GetAnimationDelay();
                    if (delay == 0) delay = 1;
                    timer.Interval = TimeSpan.FromMilliseconds(delay * 10);
                }
            }
        }

        public void SetSampler(){
            hdrSwapChain.SetSampler(preference.NearestNeighbor);
        }

        public void DrawImage(bool newImage = false)
        {
            if (!readyToDraw) return;
            if (hdrSwapChain.CheckDXGIFactory()){
                var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
                var monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                hdrSwapChain.CreateD3DDevice(monitor, preference.NearestNeighbor);
                if (hdrSwapChain.CreateSwapChain(this.AppWindow, ref ImagePanel, texImage.IsSDR, clientWidth, clientHeight, true)){
                    if (bitmapCache != null){
                        hdrSwapChain.SetBitmap(texImage.Width, texImage.Height, bitmapCache);
                        texImage.gamutScaleChecked = false;
                        texImage.gamutScale = 1.0f;
                    }
                }
            }

            StayOnScreen();

            float rootW;
            float rootH;
            if (newImage && !fitToWindow) {
                rootW = Math.Max(texImage.Width, clientWidth);
                rootH = Math.Max(texImage.Height, clientHeight);
            } else {
                rootW = clientWidth;
                rootH = clientHeight;
            }
            if ((rootW <= 0) || (rootH <= 0)) return;

            float texW = (float)texImage.Width;
            float texH = (float)texImage.Height;
            float ofsX = imageOffsetX;
            float ofsY = imageOffsetY;

            float dstX = Math.Max(0, ofsX);
            float dstY = Math.Max(0, ofsY);
            float dstW = Math.Min(rootW, ofsX + texW * zoom) - dstX;
            float dstH = Math.Min(rootH, ofsY + texH * zoom) - dstY;
            float srcX = (dstX - ofsX) / zoom;
            float srcY = (dstY - ofsY) / zoom;
            float srcW = dstW / zoom;
            float srcH = dstH / zoom;
            float selX = ofsX + selectionRect.X * zoom;
            float selY = ofsY + selectionRect.Y * zoom;
            float selW = Math.Abs(selectionRect.W * zoom);
            float selH = Math.Abs(selectionRect.H * zoom);

            try {
                if (newImage || prevW != rootW || prevH != rootH) {
                    ImagePanel.Width = rootW;
                    ImagePanel.Height = rootH;
                    hdrSwapChain.Resize((uint)rootW, (uint)rootH);
                    prevW = rootW;
                    prevH = rootH;
                }

                float maxLuminance = preference.MaxLuminance;
                if (maxLuminance == 0.0f) maxLuminance = hdrSwapChain.MaxLuminance;
                int inputSDR = texImage.IsSDR?0x04000000:0;
                int outputPQ = hdrSwapChain.IsHDR?0x08000000:0;
                int _flags = ((int)texImage.GetColorSpace() +
                              (int)texImage.GetTransferFunc() +
                              (preference.AlphaMode<<16) +
                              (preference.ToneMapType<<20) +
                              (hideChecker?0x01000000:0) +
                              (texImage.Premultiplied?0x02000000:0)+
                              inputSDR + outputPQ);
                if (outputPQ == 0 && inputSDR != 0 && !texImage.gamutScaleChecked) _flags |= 0x10000000;
                Mat3x3 toBT2020 = texImage.toBT2020;
                HdrSwapChain.AllInOneParams allInOneParams = new() {
                    srcRect = new(srcX / texW, srcY / texH, srcW / texW, srcH / texH),
                    dstRect = new(dstX / rootW, dstY / rootH, dstW / rootW, dstH / rootH),
                    selRect = new(selX / rootW, selY / rootH, selW / rootW, selH / rootH),
                    canvasColor = new(outsideColor.R / 255.0f, outsideColor.G / 255.0f, outsideColor.B / 255.0f, 1.0f),
                    param = new (texW / 8, texH / 8, texImage.gamutScale, texImage.GetGamma()),
                    toBT2020_0 = new(toBT2020.M11, toBT2020.M12, toBT2020.M13, 0.0f),
                    toBT2020_1 = new(toBT2020.M21, toBT2020.M22, toBT2020.M23, 0.0f),
                    toBT2020_2 = new(toBT2020.M31, toBT2020.M32, toBT2020.M33, 0.0f),
                    selThickness = new(2.0f / rootW, 2.0f / rootH),
                    maxCLL = texImage.maxCLL,
                    displayMaxNits = maxLuminance,
                    exposure = MathF.Pow(2.0f, (float)preference.Exposure),
                    whiteLevel = hdrSwapChain.SDRWhiteLevel,
                    flags = (uint)_flags,
                };

                float ret = hdrSwapChain.Draw(allInOneParams, rootW, rootH);

                if (!texImage.gamutScaleChecked){
                    texImage.gamutScaleChecked = true;
                    if (ret > 1.0f && preference.ToneMapType != 0){
                        texImage.gamutScale = 1.0f / ret;
                        Debug.WriteLine("#### gamutScale"+texImage.gamutScale);
                        allInOneParams.param.Z = texImage.gamutScale;
                        hdrSwapChain.Draw(allInOneParams, rootW, rootH);
                    }
                }
            }catch (Exception ex) {
                Trace.WriteLine(string.Format("Exception.HResult:{0:X8}",ex.HResult));
            }
        }



        private void UpdateClip() {
            RootGrid.Clip = new RectangleGeometry {
                Rect = new Rect(0, 0, clientWidth, clientHeight)
            };
        }

        private void UndoPush()
        {
            texImage.Push();
            SetTitle();
        }

        void LoadNext(bool prev=false){
            if (texImage == null || texImage.valid == false) return;

            string? dir = Path.GetDirectoryName(texImage.FullPath);
            if (string.IsNullOrEmpty(dir)) return;

            string ext = Path.GetExtension(texImage.FullPath);
            if (string.IsNullOrEmpty(ext)) return;

            try {
                int idx = dirCache.IndexOf(texImage.FullPath);
                if (idx < 0){
                    dirCache = Directory.GetFiles(dir, "*"+ext).OrderBy(x => x, StringComparer.OrdinalIgnoreCase.WithNaturalSort()).ToList();
                    idx = dirCache.IndexOf(texImage.FullPath);
                    if (idx < 0) return;
                }
                idx += prev?-1:1;
                if (idx < 0) idx = dirCache.Count-1;
                if (idx >= dirCache.Count) idx = 0;
                OpenFileAsync(dirCache[idx]);
                //GC.Collect(); // →押しっぱなして送るとギガ食ったのでGC。遅いかな?
            } catch (Exception e) {
                Debug.WriteLine(e.Message);
            }
        }


        //==============================================================================
        // Events

        private void MainCommandBar_Loaded(object sender, RoutedEventArgs e) {
            commandBarHeight = MainCommandBar.ActualHeight;
            Debug.WriteLine("***** MainCommandBar.Loaded  height:"+commandBarHeight);
            if (ApiInformation.IsPropertyPresent("Windows.UI.Xaml.Controls.AppBarButton", "DynamicOverflowOrder")) {
                FitToWindow.DynamicOverflowOrder = 1;
                ZoomElementContainer.DynamicOverflowOrder = 2;
            }
            if (IsRunningAsAdmin()){
                ShowError(Util.GetResourceString("InfoBarMessage_Admin"), InfoBarSeverity.Warning);
            }
        }


        private void Grid_ActualThemeChanged(FrameworkElement sender, object args) {
            MainCommandBar.Background = Util.SetThemeColors(this);
        }

        private async void ImagePanel_Loaded(object sender, RoutedEventArgs e) {
            texImage?.Dispose();
            if (cmdlineArgs.Count == 0 && preference.ShowTutorial){
                Uri uri = new("ms-appx:///Assets/tutorial256.png");
                var file = await StorageFile.GetFileFromApplicationUriAsync(uri);
                texImage = ImageLoader.Load(file.Path, 0x0002, true)!;
                texImage.valid = false;
                SetBitmap(true);
            }else{
                texImage = new TexImage(clientWidth, clientHeight);
            }

            hdrSwapChain.CreateSwapChain(this.AppWindow, ref ImagePanel, texImage.IsSDR, clientWidth, clientHeight, true);
            readyToDraw = true;
            DrawImage();

            if (cmdlineArgs.Count >= 1){
                OpenFiles(cmdlineArgs, true);
            }
            Debug.WriteLine("***** ImagePanel_Loaded");
        }

        private void ImagePanel_Unloaded(object sender, RoutedEventArgs e) {
            hdrSwapChain?.Dispose();
        }


        //------------------------------------------------------------------------------
        private void RootGrid_SizeChanged(object sender, SizeChangedEventArgs e) {
            clientWidth = (uint)Math.Round(e.NewSize.Width);
            clientHeight = (uint)Math.Round(e.NewSize.Height);
            UpdateClip();
            DrawImage();
            Debug.WriteLine($"***** RootGrid_SizeChanged {clientWidth}x{clientHeight}");
        }

        private void RootGrid_PointerPressed(object sender, PointerRoutedEventArgs e) {
            e.Handled = true;
            RootGrid.Focus(FocusState.Pointer);
            var pointer = e.GetCurrentPoint(RootGrid);
            if (pointer.Properties.IsMiddleButtonPressed) {
                drag = true;
                prevDrag = pointer.Position;
            } else if (pointer.Properties.IsLeftButtonPressed) {
                if (!selectionRect.IsEmpty) {
                    selectionResizing = CheckRectResize(pointer.Position);
                    selectionStart = Client2uv(pointer.Position, true);
                    DrawImage();
                    RecalculateOverFlow();
                }
                if (selectionResizing == 0 && texImage.valid) {

                    selectionStart = Client2uv(pointer.Position, true);
                    selectionStart.X = Math.Clamp(selectionStart.X, 0, (int)texImage.Width);
                    selectionStart.Y = Math.Clamp(selectionStart.Y, 0, (int)texImage.Height);
                    isSelecting = true;
                    selectionRect = new uvRect(selectionStart.X, selectionStart.Y, 0, 0);
                    DrawImage();
                }
                ((CustomGrid)sender).CapturePointer(e.Pointer);
            }
            SetSelectionText();
            UpdateContextMenu();
        }


        private void RootGrid_PointerMoved(object sender, PointerRoutedEventArgs e) {
            var pointer = e.GetCurrentPoint(RootGrid);

            if (isSelecting || selectionResizing != 0){
                if (!pointer.IsInContact){
                    //Debug.WriteLine("huh?");
                    return;
                }
            }

            e.Handled = true;
            uvPos pos = Client2uv(pointer.Position, true);
            if (pointer.Properties.IsMiddleButtonPressed) {
                if (drag) {
                    imageOffsetX += (float)(pointer.Position.X - prevDrag.X); ///zoom;
                    imageOffsetY += (float)(pointer.Position.Y - prevDrag.Y); ///zoom;
                    DrawImage();
                }
                prevDrag = pointer.Position;
            }
            if (selectionResizing != 0) {
                SelectionResize(pointer.Position);
                DrawImage();
            } else if (isSelecting) {
                if (pos.X < 0) pos.X = 0;
                if (pos.X > texImage.Width) pos.X = (int)texImage.Width;
                if (pos.Y < 0) pos.Y = 0;
                if (pos.Y > texImage.Height) pos.Y = (int)texImage.Height;
                selectionRect = new uvRect(Math.Min(selectionStart.X, pos.X),
                                           Math.Min(selectionStart.Y, pos.Y),
                                           (int)Math.Abs(selectionStart.X - pos.X),
                                           (int)Math.Abs(selectionStart.Y - pos.Y));
                DrawImage();
            }
            if (selectionResizing == 0) CheckRectResize(pointer.Position);
            SetSelectionText();
            UpdateContextMenu();
            SetInfoText(Client2uv(pointer.Position, false));
        }

        private void RootGrid_PointerReleased(object sender, PointerRoutedEventArgs e) {
            e.Handled = true;
            var pointer = e.GetCurrentPoint(RootGrid);
            if (pointer.Properties.IsMiddleButtonPressed) {
                drag = false;
            }
            if (selectionResizing != 0) {
                selectionResizing = 0;
            } else if (isSelecting) {
                isSelecting = false;
            }
            if (selectionRect.W == 0 || selectionRect.H == 0){
                selectionRect.W = 0;
                selectionRect.H = 0;
            }
            //((CustomGrid)sender).ReleasePointerCapture(e.Pointer);

            SetSelectionText();
            UpdateContextMenu();
            DrawImage();
            RecalculateOverFlow();
        }

        void RootGrid_PointerWheelChanged(object sender, PointerRoutedEventArgs e) {
            e.Handled = true;
            var pointer = e.GetCurrentPoint(RootGrid);
            var delta = pointer.Properties.MouseWheelDelta;
            float oldScale = zoom;

            if (delta > 0) {
                zoom *= 1.02f;
                if (zoom > 100.0f) zoom = 100.0f;
            } else if (delta < 0) {
                zoom *= 0.98f;
                if (zoom < 0.01f) zoom = 0.01f;
            }
            SetZoomText();

            float scaleChange = zoom / oldScale;
            float cx = (float)pointer.Position.X;
            float cy = (float)pointer.Position.Y;
            imageOffsetX = cx - (cx - imageOffsetX) * scaleChange;
            imageOffsetY = cy - (cy - imageOffsetY) * scaleChange;

            DrawImage();
        }


        //------------------------------------------------------------------------------
        void DragOver(object sender, DragEventArgs e) {
            if (RootGrid.IsBusy()) {
                e.AcceptedOperation = DataPackageOperation.None;
            }else{
                e.AcceptedOperation = (e.DataView.Contains(StandardDataFormats.StorageItems)) ? DataPackageOperation.Copy : DataPackageOperation.None;
            }
        }



        //------------------------------------------------------------------------------
        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern ushort RegisterClipboardFormat(string lpszFormat);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr GlobalLock(IntPtr hMem);
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GlobalUnlock(IntPtr hMem);
        [DllImport("ole32.dll")]
        private static extern void ReleaseStgMedium(ref STGMEDIUM pmedium);

        [StructLayout(LayoutKind.Sequential)]
        public struct FORMATETC
        {
            public short cfFormat;
            public IntPtr ptd;
            public uint dwAspect;
            public int lindex;
            public uint tymed;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct STGMEDIUM
        {
            public int tymed;
            public IntPtr unionmember;
            public IntPtr pUnkForRelease;
        }

        [ComImport]
        [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        [System.Runtime.InteropServices.Guid("0000010e-0000-0000-C000-000000000046")]
        public interface IDataObject
        {
            void GetData(ref FORMATETC pFormatEtc, out STGMEDIUM pMedium);
            void GetDataHere(ref FORMATETC pFormatEtc, ref STGMEDIUM pMedium);
            int  QueryGetData(ref FORMATETC pFormatEtc);
            void GetCanonicalFormatEtc(ref FORMATETC pFormatEtc, out FORMATETC pFormatEtcOut);
            void SetData(ref FORMATETC pFormatEtc, ref STGMEDIUM pMedium, bool fRelease);
            void EnumFormatEtc(uint dwDirection, out object ppenumFormatEtc);
            void DAdvise(ref FORMATETC pFormatEtc, uint advf, object pAdvSink, out uint pdwConnection);
            void DUnadvise(uint dwConnection);
            void EnumDAdvise(out object ppenumAdvise);
        }
        public const uint TYMED_HGLOBAL = 1;
        public const uint TYMED_ISTREAM = 4;
        public const uint DVASPECT_CONTENT = 1;


        async void DropAsync(object sender, DragEventArgs e) {
            if (RootGrid.IsBusy()) return;
            e.Handled = true;
            var deferral = e.GetDeferral();
            try {
                bool virtualFile = false;
                if (e.DataView.Contains(StandardDataFormats.StorageItems)) {
                    var items = await e.DataView.GetStorageItemsAsync();
                    List<string> itemList = new();
                    foreach(var item in items.OfType<StorageFile>()){
                        // 7zip は Path に一時ファイル(Temporary フラグ付き)がある
                        // Exploler は Path が空
                        if (string.IsNullOrEmpty(item.Path)){
                            virtualFile = true;
                        }else{
                            itemList.Add(item.Path);
                        }
                    }
                    if (!virtualFile){
                        if (itemList.Count > 0) {
                            OpenFiles(itemList);
                        }
                        return;
                    }
                }

                var dataObject = e.DataView.As<IDataObject>();
                if (dataObject == null) return;

                var formatDescriptor = new FORMATETC {
                    cfFormat = (short)RegisterClipboardFormat("FileGroupDescriptorW"),
                    dwAspect = DVASPECT_CONTENT,
                    lindex = -1,
                    tymed = TYMED_HGLOBAL
                };

                if (dataObject.QueryGetData(ref formatDescriptor) != 0) return;
                dataObject.GetData(ref formatDescriptor, out var stgMediumDescriptor);

                IntPtr hGlobal = stgMediumDescriptor.unionmember;
                IntPtr pSource = GlobalLock(hGlobal);
                if (pSource == IntPtr.Zero) return;

                List<string> fileList = new();
                try {
                    int fileCount = Marshal.ReadInt32(pSource);
                    IntPtr currentFileDescriptorPtr = IntPtr.Add(pSource, 4);

                    var formatContents = new FORMATETC {
                        cfFormat = (short)RegisterClipboardFormat("FileContents"),
                        dwAspect = DVASPECT_CONTENT,
                        tymed = TYMED_ISTREAM
                    };

                    for (int i = 0; i < fileCount; i++) {
                        formatContents.lindex = i;
                        dataObject.GetData(ref formatContents, out var stgMediumContent);

                        object? streamObj = null;
                        try {
                            streamObj = Marshal.GetObjectForIUnknown(stgMediumContent.unionmember);
                            if (streamObj is IStream iStream) {
                                string fileName = Marshal.PtrToStringUni(IntPtr.Add(currentFileDescriptorPtr, 72), 260).Split('\0')[0];
                                byte[] fileBytes = ReadIStreamToBytes(iStream);
                                string tempPath = Path.Combine(ApplicationData.Current.TemporaryFolder.Path, fileName);
                                File.WriteAllBytes(tempPath, fileBytes);
                                Debug.WriteLine($"saved: {tempPath} {fileBytes.Length}bytes");
                                fileList.Add(tempPath);
                            }
                        }finally{
                            if (streamObj != null) Marshal.ReleaseComObject(streamObj);
                            ReleaseStgMedium(ref stgMediumContent);
                        }

                        // \\?\ や longPathAware でも cFileName[MAX_PATH] のサイズは変わらない
                        currentFileDescriptorPtr = IntPtr.Add(currentFileDescriptorPtr, 592);
                    }
                }finally{
                    GlobalUnlock(hGlobal);
                    ReleaseStgMedium(ref stgMediumDescriptor);
                }
                if (fileList.Count > 0){
                    OpenFiles(fileList);
                }
            }catch (Exception ex){
                Debug.WriteLine(ex.Message);
            }finally{
                deferral.Complete();
            }
        }

        private byte[] ReadIStreamToBytes(IStream iStream)
        {
            iStream.Stat(out STATSTG statstg, 1); // 1 = STATFLAG_NONAME
            long streamSize = statstg.cbSize;
            byte[] buffer = new byte[streamSize];
            iStream.Read(buffer, buffer.Length, IntPtr.Zero);
            return buffer;
        }


        //------------------------------------------------------------------------------
        void Window_Activated(object sender, WindowActivatedEventArgs e) {
            if (e.WindowActivationState != WindowActivationState.Deactivated){
                Debug.WriteLine($"***** Window_Activated {AppWindow.Position.X},{AppWindow.Position.Y}");
                var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
                currentMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                if (hdrSwapChain != null && hdrSwapChain.CheckHDR(this.AppWindow)){
                    ShowError(Util.GetResourceString("InfoBarMessage_ChangeHDR"), InfoBarSeverity.Warning);
                }
            }
            if (windowPos is PointInt32 val){
                AppWindow.Move(val);
                windowPos = null;
            }
        }

        void Window_Closed(object sender, WindowEventArgs e) {
            pluginManagerWindow?.Close();
            texImage?.Dispose();
        }

        void Window_SizeChanged(object sender, WindowSizeChangedEventArgs e) {
            if (fitToWindow) ZoomToWindow();
            if ((prevSize.Width != 0) && (prevSize.Height != 0)) {
                imageOffsetX += (float)(e.Size.Width - prevSize.Width) / 2;
                imageOffsetY += (float)(e.Size.Height - prevSize.Height) / 2;
            }
            prevSize = e.Size;
            Debug.WriteLine($"***** Window_SizeChanged {prevSize.Width}x{prevSize.Height}");
        }

        void Window_Changed(object sender, AppWindowChangedEventArgs args)
        {
            if (args.DidPositionChange){
                var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
                var monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                if (monitor != currentMonitor){
                    Debug.WriteLine($"***** Monitor Changed {monitor}");
                    currentMonitor = monitor;
                    bool hdrChanged = hdrSwapChain.CheckHDR(this.AppWindow);
                    bool force = hdrChanged | hdrSwapChain.CheckDXGIFactory();
                    hdrSwapChain.CreateD3DDevice(monitor, preference.NearestNeighbor);
                    if (hdrSwapChain.CreateSwapChain(this.AppWindow, ref ImagePanel, texImage.IsSDR, clientWidth, clientHeight, force)){
                        if (bitmapCache != null){
                            hdrSwapChain.SetBitmap(texImage.Width, texImage.Height, bitmapCache);
                            if (hdrChanged && texImage.hasGainMap && !notifyHDRChanged){
                                //notifyHDRChanged = true;
                                ShowError(Util.GetResourceString("InfoBarMessage_ChangeHDR"), InfoBarSeverity.Warning);
                            }
                            texImage.gamutScaleChecked = false;
                            texImage.gamutScale = 1.0f;
                        }
                        DrawImage();
                    }
                }
            }
        }

        private bool moveDialog(ContentDialog dialog, ManipulationDelta delta) {
            if (dialog.Content is FrameworkElement content){
                double footerHeight = 120;
                double hw = (dialog.Width - content.ActualWidth)/2.0;
                double hh = (dialog.Height - content.ActualHeight - footerHeight)/2.0;
                var left = dialog.Margin.Left + delta.Translation.X;
                var top = dialog.Margin.Top + delta.Translation.Y;
                var right = dialog.Margin.Right - delta.Translation.X;
                var bottom = dialog.Margin.Bottom - delta.Translation.Y;
                if (left >= -hw && top >= -hh && right >= -hw && bottom >= -hh){
                    dialog.Margin = new Thickness(
                        left, top, right, bottom
                    );
                    return true;
                }
            }
            return false;
        }


        //==============================================================================
        // Context Menu

        private void MenuFlyout_Opening(object sender, object e) {
            if (animationEnable){
                FlyoutAnimation.Visibility = Visibility.Visible;
                FlyoutAnimationPause.Visibility = Visibility.Collapsed;
            }else{
                FlyoutAnimation.Visibility = Visibility.Collapsed;
                FlyoutAnimationPause.Visibility = Visibility.Visible;
            }
        }

        private void MenuFlyout_Closed(object sender, object e) {
            //TODO: MenuFlyoutSubItem が残ることがある
        }

        private void ContextMenu_Checker(object sender, RoutedEventArgs e) {
            var toggleChecker = sender as ToggleMenuFlyoutItem;
            if (toggleChecker != null) {
                hideChecker = toggleChecker.IsChecked;
                DrawImage();
            }
        }

        private void ContextMenu_Flip(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            UndoPush();
            texImage.FlipV();
            SetBitmap();
            SetSelectionText(true);
            UpdateContextMenu();
            DrawImage();
        }

        private void ContextMenu_Flop(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            UndoPush();
            texImage.FlipH();
            SetBitmap();
            SetSelectionText(true);
            UpdateContextMenu();
            DrawImage();
        }

        private void ContextMenu_Rotate(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            UndoPush();
            texImage.Rotate();
            SetBitmap(true);
            SetTitle();
            SetSelectionText(true);
            UpdateContextMenu();
            FitWindowToImage();
            DrawImage();
        }

        private void ContextMenu_Crop(object sender, RoutedEventArgs e) {
            if (selectionRect.IsEmpty) return;
            if (RootGrid.IsBusy()) return;
            UndoPush();
            texImage.Crop(selectionRect.X, selectionRect.Y, selectionRect.W, selectionRect.H);
            SetBitmap(true);
            SetTitle();
            zoom = GetSuitableZoom(zoom);
            SetSelectionText(true);
            UpdateContextMenu();
            FitWindowToImage();
            DrawImage();
        }

        private async void ContextMenu_Mosaic(object sender, RoutedEventArgs e) {
            if (selectionRect.IsEmpty) return;
            if (RootGrid.IsBusy()) return;
            UndoPush();
            RootGrid.SetBusy(true);
            await Task.Run(() => {
                texImage.Mosaic(selectionRect.X, selectionRect.Y,
                                selectionRect.W, selectionRect.H,
                                preference.MosaicSize);
            });
            RootGrid.SetBusy(false);
            SetBitmap();
            UpdateContextMenu();
            DrawImage();
        }

        private void ContextMenu_Animation(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            var toggleAnimation = sender as ToggleMenuFlyoutItem;
            if (toggleAnimation == null) return;
            animationEnable = toggleAnimation.IsChecked;
            //timer.Enabled = animationEnable;
            FlyoutAnimation.IsChecked = animationEnable;
            FlyoutAnimationPause.IsChecked = animationEnable;
            if (animationEnable){
                FlyoutAnimation.Visibility = Visibility.Visible;
                FlyoutAnimationPause.Visibility = Visibility.Collapsed;
                timer.Start();
            }else{
                FlyoutAnimation.Visibility = Visibility.Collapsed;
                FlyoutAnimationPause.Visibility = Visibility.Visible;
                timer.Stop();
            }
        }

        private async void ContextMenu_Resize(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            EnlargeWindow(350, 480);
            ContentDialog dialog = new () {
                XamlRoot = this.Content.XamlRoot,
                Title = Util.GetResourceString("TitleResize"),
                RequestedTheme = ((FrameworkElement)this.Content).RequestedTheme,
                PrimaryButtonText = Util.GetResourceString("DialogResize"),
                CloseButtonText = Util.GetResourceString("DialogCancel"),
                ManipulationMode = ManipulationModes.TranslateX | ManipulationModes.TranslateY,
            };

            var content = new ResizeDialogContent() {
                ImageWidth = texImage.Width,
                ImageHeight = texImage.Height,
                ManipulationMode = ManipulationModes.None,
            };
            dialog.Content = content;
            dialog.ManipulationDelta += delegate (object sender, ManipulationDeltaRoutedEventArgs e) { if (!e.IsInertial) moveDialog(dialog, e.Delta); };

            ContentDialogResult result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary) {
                UndoPush();
                texImage.Resize(content.DecidedWidth, content.DecidedHeight,
                                content.DecidedFilterType);
                SetBitmap(true);
                SetTitle();
                zoom = GetSuitableZoom(zoom);
                FitWindowToImage();
                DrawImage();
            }
        }

        private async void ContextMenu_Info(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            EnlargeWindow(450, 400);
            ContentDialog dialog = new () {
                XamlRoot = this.Content.XamlRoot,
                Title = Util.GetResourceString("TitleImageInfo"),
                RequestedTheme = ((FrameworkElement)this.Content).RequestedTheme,
                PrimaryButtonText = Util.GetResourceString("DialogCopyToClipboard"),
                CloseButtonText = Util.GetResourceString("DialogOK"),
                ManipulationMode = ManipulationModes.TranslateX | ManipulationModes.TranslateY,
            };

            var version = Assembly.GetExecutingAssembly().GetName().Version;
            string generalInfo = $"{texImage.FullPath}\n\nSize: {texImage.Width}x{texImage.Height}";
            if (texImage.FrameCount > 1) {
                generalInfo += $"  ({texImage.FrameCount}frames)";
            }

            if (File.Exists(texImage.FullPath)) {
                DateTime creationTime = File.GetCreationTime(texImage.FullPath);
                generalInfo += $"\nCreation time: {creationTime}";
                DateTime lastWriteTime = File.GetLastWriteTime(texImage.FullPath);
                generalInfo += $"\nLast updated: {lastWriteTime}";
            }

            generalInfo += $"\nOriginal Format: {texImage.OrigFormat}";
            if (texImage.Premultiplied){
                generalInfo += $"\nPremultiplied: True";
            }


            generalInfo += $"\nColorSpace: {texImage.ColorSpace}";
            TransferFunc tf = texImage.GetTransferFunc();
            if (tf != TransferFunc.Default){
                if (tf == TransferFunc.Gamma_N){
                    float g = texImage.GetGamma();
                    generalInfo += $"\nTransferFunction: Gamma {g:F5} (1/{1.0/g:F2})";
                }else{
                    generalInfo += $"\nTransferFunction: {texImage.GetTransferFunc()}";
                }
            }

            string loaderInfo = texImage.Plugin + " plugin:\n" + texImage.AdditionalInfo;
            if (texImage.LoadByPlugin){
                loaderInfo += $"\nIntermidiate format: {texImage.PluginFormat}";
            }
            string texViewerInfo = $"TexViewer {version?.Major}.{version?.Minor}.{version?.Build}.{version?.Revision}";
            texViewerInfo += $"\nRender Format: {hdrSwapChain.RenderFormat}";
            texViewerInfo += $"\nOutput Format: {hdrSwapChain.OutputFormat}";
            texViewerInfo += $"\nOutput ColorSpace: {hdrSwapChain.ColorSpace}";
            texViewerInfo += $"\nDisplay MaxLuminance: {hdrSwapChain.MaxLuminance}";
            texViewerInfo += $"\nDisplay MaxFullFrameLuminance: {hdrSwapChain.MaxFullFrameLuminance}";

            string tonemap="";
            float lum = preference.MaxLuminance;
            if (lum == 0.0f) tonemap = $"{Util.GetResourceString("PreferenceMaxLuminanceAuto")} ({hdrSwapChain.MaxLuminance}nit)";
            if (lum > 0.0f) tonemap = $"{lum}nit";
            texViewerInfo += $"\n Tonemap max luminance: {tonemap}";

            var content = new ImageInfoContent(generalInfo, loaderInfo, texViewerInfo) {
                ManipulationMode = ManipulationModes.None,
            };
            dialog.Content = content;
            dialog.ManipulationDelta += delegate (object sender, ManipulationDeltaRoutedEventArgs e) { if (!e.IsInertial) moveDialog(dialog, e.Delta); };

            ContentDialogResult result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary) {
                DataPackage dataPackage = new DataPackage();
                dataPackage.RequestedOperation = DataPackageOperation.Copy;
                dataPackage.SetText(generalInfo+"\n\n"+loaderInfo+"\n\n"+texViewerInfo);
                Clipboard.SetContent(dataPackage);
            }
        }

        private async void ContextMenu_ColorPicker(object sender, RoutedEventArgs e) {
            EnlargeWindow(400, 550);
            ContentDialog dialog = new () {
                XamlRoot = this.Content.XamlRoot,
                Title = Util.GetResourceString("TitleColorPicker"),
                RequestedTheme = ((FrameworkElement)this.Content).RequestedTheme,
                PrimaryButtonText = Util.GetResourceString("DialogOK"),
                CloseButtonText = Util.GetResourceString("DialogCancel"),
                ManipulationMode = ManipulationModes.TranslateX | ManipulationModes.TranslateY,
            };

            Windows.UI.Color prevColor = outsideColor;
            var content = new ColorPickerContent(this) {
                PickedColor = outsideColor,
                ManipulationMode = ManipulationModes.None,
            };
            dialog.Content = content;
            dialog.ManipulationDelta += delegate (object sender, ManipulationDeltaRoutedEventArgs e) { if (!e.IsInertial) moveDialog(dialog, e.Delta); };

            ContentDialogResult result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary) {
                outsideColor = content.PickedColor;
                DrawImage();
            }else{
                outsideColor = prevColor;
                DrawImage();
            }
        }

        // executes twice
        // https://github.com/microsoft/microsoft-ui-xaml/issues/8212
        DateTime triggered = DateTime.UnixEpoch; // workaround
        private void ContextMenu_Undo(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            if (texImage == null) return;
            DateTime now = DateTime.Now;
            TimeSpan delta = now - triggered;
            Debug.WriteLine("undo workaround: "+delta.TotalMilliseconds);
            if (delta.TotalMilliseconds > 300.0f){ // 300ms
                triggered = now;
                if (texImage.Pop()){
                    zoom = GetSuitableZoom(zoom);
                    UpdateContextMenu();
                    SetTitle();
                }
                SetBitmap(true);
                FitWindowToImage();
                DrawImage(true);
            }
        }

        private async void ContextMenu_Open(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            var openPicker = new FileOpenPicker(this.AppWindow.Id);
            openPicker.ViewMode = PickerViewMode.Thumbnail;
            //openPicker.FileTypeFilter.Add("*");
            var results = await openPicker.PickMultipleFilesAsync();
            if (results.Count > 0){
                List<string> pickList = new();
                foreach(var item in results){
                    pickList.Add(item.Path);
                }
                OpenFiles(pickList);
            }
        }

        bool saving = false; // workaround
        private async void ContextMenu_Save(object sender, RoutedEventArgs e) {
            if (RootGrid.IsBusy()) return;
            if (!saving){
                saving = true;
                var savePicker = new FileSavePicker(this.AppWindow.Id);

                string ext = Path.GetExtension(texImage.FullPath).ToLower();
                if ((ext == ".jpeg")||(ext == ".jpg")||(ext == ".png")){
                    ext = Path.GetExtension(texImage.FullPath);
                }else if (string.IsNullOrEmpty(ext)){
                    ext = ".png";
                }

                string suggestedFilename = Path.GetFileNameWithoutExtension(texImage.FullPath) + ext;
                savePicker.SuggestedFileName = suggestedFilename;
                savePicker.FileTypeChoices.Add("All files", ["."]);

                var file = await savePicker.PickSaveFileAsync();
                if (file != null) {
                    var path = file.Path;
                    if (string.IsNullOrEmpty(Path.GetExtension(path))){
                        DeleteIfEmpty(path);
                        path += ext;
                    }

                    if (ImageLoader.Save(texImage, path)){
                        if (DeleteIfEmpty(path)){
                            EnlargeWindow(400,200);
                            var info = new ContentDialog() {
                                XamlRoot = this.Content.XamlRoot,
                                Title = "Save Error",
                                Content = Util.GetResourceString("ErrorSave")+" '"+file.Path+"'",
                                PrimaryButtonText = "Ok",
                            };

                            await info.ShowAsync();
                        }
                    }else{
                        DeleteIfEmpty(path);
                        EnlargeWindow(400,200);
                        var info = new ContentDialog() {
                            XamlRoot = this.Content.XamlRoot,
                            Title = "Save Error",
                            Content = Util.GetResourceString("NoPluginToWrite")+" '"+file.Path+"'",
                            PrimaryButtonText = "Ok",
                        };
                        await info.ShowAsync();
                    }
                }
                saving = false;
            }
        }

        static bool DeleteIfEmpty(string filePath) {
            try {
                if (File.Exists(filePath)) {
                    FileInfo fi = new FileInfo(filePath);
                    if (fi.Length == 0) {
                        fi.Delete();
                        return true;
                    }
                }
            } catch (Exception ex) {
                Debug.WriteLine(ex.Message);
            }
            return false;
        }

        private void ContextMenu_Dummy(object sender, RoutedEventArgs e) {
        }


        //==============================================================================
        // CommandBar

        private PluginManagerWindow? pluginManagerWindow = null;
        private void PluginManager_Click(object sender, RoutedEventArgs e) {
            if (pluginManagerWindow == null) {
                pluginManagerWindow = new PluginManagerWindow(this.AppWindow);
                pluginManagerWindow.Closed += (s, args) => {
                    pluginManagerWindow = null;
                };
            }
            pluginManagerWindow.Activate();
        }

        private async void Preference_Click(object sender, RoutedEventArgs e) {
            EnlargeWindow(420, 600);
            var content = new PreferenceContent(this, hdrSwapChain.enableHDR) {
                ManipulationMode = ManipulationModes.None,
            };
            ContentDialog dialog = new () {
                XamlRoot = this.Content.XamlRoot,
                Title = Util.GetResourceString("TitlePreference"),
                RequestedTheme = ((FrameworkElement)this.Content).RequestedTheme,
                PrimaryButtonText = Util.GetResourceString("DialogOK"),
                CloseButtonText = Util.GetResourceString("DialogCancel"),
                ManipulationMode = ManipulationModes.TranslateX | ManipulationModes.TranslateY,
            };

            dialog.Content = content;
            dialog.ManipulationDelta += delegate (object sender, ManipulationDeltaRoutedEventArgs e) { if (!e.IsInertial) moveDialog(dialog, e.Delta); };

            bool prevNN = preference.NearestNeighbor;
            bool prevShowAllExif = preference.ShowAllExif;
            int prevAlphaMode = preference.AlphaMode;
            int prevMaxLuminanceIndex = preference.MaxLuminanceIndex;
            int prevToneMapType = preference.ToneMapType;
            double prevExposure = preference.Exposure;

            ContentDialogResult result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary) {
                content.SavePreference();
                if (preference.NearestNeighbor != prevNN){
                    hdrSwapChain.SetSampler(preference.NearestNeighbor);
                }
                if (preference.ShowAllExif != prevShowAllExif){
                    ShowError(Util.GetResourceString("InfoBarMessage_NextOpen"), InfoBarSeverity.Warning);
                }
                if (preference.ToneMapType != prevToneMapType ||
                    preference.MaxLuminanceIndex != prevMaxLuminanceIndex){
                    texImage.gamutScaleChecked = false;
                    texImage.gamutScale = 1.0f;
                }
                DrawImage();
            }else{
                preference.NearestNeighbor = prevNN;
                hdrSwapChain.SetSampler(preference.NearestNeighbor);
                preference.AlphaMode = prevAlphaMode;
                preference.MaxLuminanceIndex = prevMaxLuminanceIndex;
                preference.ToneMapType = prevToneMapType;
                preference.Exposure = prevExposure;
                texImage.gamutScaleChecked = false;
                texImage.gamutScale = 1.0f;
                DrawImage();
            }
        }

        private async void Help_Click(object sender, RoutedEventArgs e) {
            Uri uri;
            if (Thread.CurrentThread.CurrentCulture.Name.StartsWith("ja")){
                uri = new Uri("https://github.com/ueda-san/TexViewer/wiki/How-to-use-%E2%80%90-ja");
            }else{
                uri = new Uri("https://github.com/ueda-san/TexViewer/wiki/How-to-use-%E2%80%90-en");
            }
            _ = await Windows.System.Launcher.LaunchUriAsync(uri);
        }


        private void FitToWindow_Checked(object sender, RoutedEventArgs e) {
            fitToWindow = true;
            ZoomToWindow();
            DrawImage();
        }

        private void FitToWindow_Unchecked(object sender, RoutedEventArgs e) {
            fitToWindow = false;
            if (zoom > 100.0f){
                zoom = 100.0f;
                imageOffsetX = (float)(clientWidth-texImage.Width*zoom)/2.0f;
                imageOffsetY = (float)(clientHeight-texImage.Height*zoom)/2.0f;
                SetZoomText();
                DrawImage();
            }
        }

        private void ZoomText_Tapped(object sender, TappedRoutedEventArgs e) {
            if (!fitToWindow){
                zoom = 1.0f;
                FitWindowToImage();
            }else{
                ZoomToWindow();
           }
            DrawImage();
        }


        //==============================================================================
        // Accelerator

        private async void OnPasteInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args) {
            if (RootGrid.IsBusy()) return;
            args.Handled = true;
            DataPackageView dataPackageView = Clipboard.GetContent();
            if (dataPackageView.Contains(StandardDataFormats.Bitmap)){
                var streamRef = await dataPackageView.GetBitmapAsync();
                var streamContent = await streamRef.OpenReadAsync();
                var decoder = await BitmapDecoder.CreateAsync(streamContent);
                uint width = decoder.OrientedPixelWidth;
                uint height = decoder.OrientedPixelHeight;
                Format fmt = Util.ConvertFormat(decoder.BitmapPixelFormat);
                if (fmt == Format.UNKNOWN) return;

                var pixelData = await decoder.GetPixelDataAsync();
                texImage?.Dispose();
                texImage = new TexImage(pixelData.DetachPixelData(), fmt, width, height) {
                    Premultiplied = (decoder.BitmapAlphaMode == BitmapAlphaMode.Premultiplied),
                    FullPath = "Clipboard",
                    OrigFormat = "Clipboard"
                };
                AfterLoadSetup();
            }
        }

        private async void OnCopyInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args) {
            if (RootGrid.IsBusy()) return;
            if (texImage == null) return;
            args.Handled = true;

            var stream = new InMemoryRandomAccessStream();
            var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, stream);
            uint depth;
            encoder.SetPixelData(texImage.IsSDR?BitmapPixelFormat.Rgba8:BitmapPixelFormat.Rgba16,
                                 texImage.Premultiplied?BitmapAlphaMode.Premultiplied:BitmapAlphaMode.Straight,
                                 texImage.Width,
                                 texImage.Height,
                                 96,96,
                                 texImage.ToByteArray(out depth, TexImage.ConvOption.RGBA8or16));
            await encoder.FlushAsync();
            DataPackage dataPackage = new();
            dataPackage.SetBitmap(RandomAccessStreamReference.CreateFromStream(stream));
            dataPackage.Properties.Title = Path.GetFileName(texImage.FullPath);
            Clipboard.SetContent(dataPackage);
            Clipboard.Flush();
        }

        private void OnNextKeyInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args) {
            Debug.WriteLine("OnNextKeyInvoked");
            if (RootGrid.IsBusy()) return;
            if (RootGrid.ContextFlyout.IsOpen) return;
            if (texImage == null) return;

            args.Handled = true;
            if (texImage.HasAnimation && !animationEnable){
                texImage.NextFrame();
                SetBitmap(true);
                DrawImage();
                SetTitle();
                SetInfoText();
            }else{
                LoadNext(false);
            }
        }

        private void OnPrevKeyInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args) {
            Debug.WriteLine("OnPrevKeyInvoked");
            if (RootGrid.IsBusy()) return;
            if (RootGrid.ContextFlyout.IsOpen) return;
            if (texImage == null) return;

            args.Handled = true;
            if (texImage.HasAnimation && !animationEnable){
                texImage.PrevFrame();
                SetBitmap(true);
                DrawImage();
                SetTitle();
                SetInfoText();
            }else{
                LoadNext(true);
            }
        }

        private void OnEscKeyInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args) {
            if (!selectionRect.IsEmpty) {
                SetSelectionText(true);
                DrawImage();
            }
        }

    }
}
