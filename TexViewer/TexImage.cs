using ImageMagick;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Text.RegularExpressions;
using TexViewerPlugin;


namespace TexViewer
{
    public class TexImage : IDisposable
    {
        private bool disposedValue;
        public bool valid = false;
        public uint Width { get { return images[animationIndex].Width; } }
        public uint Height { get { return images[animationIndex].Height; } }
        public uint BitDepth { get { return getImageDepth(); } }
        public bool IsSDR { get { return (BitDepth == 8/* || !images[animationIndex].IsOpaque*/); } }
        public float maxCLL  = 0.0f;
        public float gamutScale = 1.0f;
        public bool gamutScaleChecked = false;
        public Mat3x3 toBT2020 = Mat3x3.Identity;

        public bool LoadByPlugin = false;
        public Format PluginFormat = Format.UNKNOWN;
        TexViewerPlugin.ColorSpace PluginColorSpace = TexViewerPlugin.ColorSpace.Unknown;
        TexViewerPlugin.TransferFunc PluginTransferFunc = TexViewerPlugin.TransferFunc.Default;
        float PluginGamma = 1.0f;
        public TexViewerPlugin.ColorSpace ColorSpace { get { return GetColorSpace(); } }

        public string Plugin { get; set; } = "(Unknown)";
        public string FullPath { get; set; } = "(Unknown)";
        public string OrigFormat { get; set; } = "(Unknown)";

        public bool HasAnimation { get { return (images.Count >= 2); } }
        public int FrameCount { get { return images.Count; } }
        public bool Premultiplied = false;
        public bool hasGainMap = false;
        public string AdditionalInfo = "";
        public int CurrenPage { get { return animationIndex+1; } }
        public int TotalPages { get { return images.Count; } }

        public uint GetAnimationDelay(int frame=-1) {  // 1/100s
            if (frame < 0) frame = animationIndex;
            if (frame >= images.Count) frame = images.Count - 1;
            uint delay = images[frame].AnimationDelay;
            //Debug.WriteLine("AnimationDelay "+delay);
            //if (delay == 0) delay = 1;
            return delay;
        }
        public void NextFrame() {
            if (++animationIndex >= images.Count) animationIndex = 0;
        }
        public void PrevFrame() {
            if (--animationIndex < 0) animationIndex = images.Count-1;
        }

        public TexViewerPlugin.TransferFunc GetTransferFunc(){
            if (LoadByPlugin){
                return PluginTransferFunc;
            }else{
                if (FullPath.EndsWith(".jxr")) return TexViewerPlugin.TransferFunc.Linear; //FixMe!
                return TexViewerPlugin.TransferFunc.Gamma_N;
            }
        }
        public float GetGamma(){
            if (LoadByPlugin){
                return PluginGamma;
            }else{
                return (float)images[animationIndex].Gamma;
            }
        }

        public TexViewerPlugin.ColorSpace GetColorSpace(){
            if (LoadByPlugin){
                return PluginColorSpace;
            }else{
                switch(images[animationIndex].ColorSpace){
                  case ImageMagick.ColorSpace.sRGB:
                    return TexViewerPlugin.ColorSpace.sRGB;
                  case ImageMagick.ColorSpace.LinearGray:
                  case ImageMagick.ColorSpace.scRGB:
                  case ImageMagick.ColorSpace.RGB:
                    return TexViewerPlugin.ColorSpace.Linear;
                  case ImageMagick.ColorSpace.Adobe98:
                    return TexViewerPlugin.ColorSpace.Adobe98;
                  case ImageMagick.ColorSpace.DisplayP3:
                    return TexViewerPlugin.ColorSpace.DisplayP3;
                  default:
                    break;
                }
            }
            return TexViewerPlugin.ColorSpace.Unknown;
        }


        MagickImageCollection images = new();
        Stack<MagickImageCollection> undoBuffer = new();
        int animationIndex = 0;

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    images.Dispose();
                    foreach(var i in undoBuffer)
                    {
                        i.Dispose();
                    }
                    undoBuffer.Clear();
                }
                disposedValue = true;
            }
        }

        ~TexImage()
        {
            Dispose(disposing: false);
        }

        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        public void Push() {
            undoBuffer.Push((MagickImageCollection)images.Clone());
        }

        public bool Pop() {
            if (undoBuffer.Count > 0){
                images = undoBuffer.Pop();
            }
            return (undoBuffer.Count == 0); // last one
        }

        public int UndoableCount() {
            return undoBuffer.Count;
        }

        public TexImage(uint w, uint h) // create blank image
        {
            images = new MagickImageCollection();
            images.Add(new MagickImage(MagickColors.Transparent, w, h));
        }

        uint getImageDepth(){
            if (images[0].ColorType == ColorType.Palette ||
                images[0].ColorType == ColorType.PaletteAlpha){
                return 8; // Tiffなど理論的には16もありうる？
            }
            return images[0].Depth;
        }

        static Mat3x3 BT709toBT2020     = new Mat3x3( 0.6274039f, 0.3292830f, 0.0433131f,
                                                      0.0690973f, 0.9195404f, 0.0113623f,
                                                      0.0163914f, 0.0880133f, 0.8955953f);
        static Mat3x3 Adobe98toBT2020   = new Mat3x3( 0.8773338f, 0.0774937f, 0.0451725f,
                                                      0.0966226f, 0.8915273f, 0.0118501f,
                                                      0.0229211f, 0.0430367f, 0.9340423f);
        static Mat3x3 DciP3toBT2020     = new Mat3x3( 0.6896961f, 0.2071707f, 0.0413459f,
                                                      0.0418510f, 0.9824106f, 0.0108460f,
                                                     -0.0011075f, 0.0183642f, 0.8550411f);
        static Mat3x3 DisplayP3toBT2020 = new Mat3x3( 0.7538330f, 0.1985974f, 0.0475696f,
                                                      0.0457438f, 0.9417772f, 0.0124789f,
                                                     -0.0012103f, 0.0176017f, 0.9836086f);
        public TexImage(string path) {
            try {
                string[] movies = { // too heavy
                    ".avi",
                    ".m2v",
                    ".mkv",
                    ".mov",
                    ".mp4",
                    ".mpeg",
                    ".mpg",
                    ".webm",
                    ".wmv",
                };
                string ext = Path.GetExtension(path);
                foreach(var mov in movies){
                    if (String.Compare(ext, mov, true)==0) throw new();
                }
                if (path.EndsWith(".apng")){
                    //png v3でアニメーションが正式サポートされたので将来的に要らなくなると思う
                    images = new MagickImageCollection(path, MagickFormat.APng);
                }else if (path.EndsWith(".pdf")){
                    //75dpiだとちょっと解像度が低いので150ぐらいに
                    MagickReadSettings settings = new MagickReadSettings();
					settings.Density = new Density(150);
                    settings.Depth = 8;
					images = new MagickImageCollection(path, settings);
                }else{
                    images = new MagickImageCollection(path);
                }
                if (images.Count > 1){
                    if (!path.EndsWith(".pdf")){
                        images.Coalesce();
                    }
                }
                foreach (MagickImage img in images.Cast<MagickImage>()) {
                    img.AutoOrient();
                }

                if (images[0].Chromaticity.White.X != 0.0 && images[0].Chromaticity.White.Y != 0.0 && images[0].Chromaticity.White.Z != 0.0){
                    toBT2020 = PrimariesToBT2020(new float[] {
                            (float)images[0].Chromaticity.Red.X,
                            (float)images[0].Chromaticity.Red.Y,
                            (float)images[0].Chromaticity.Red.Z,
                            (float)images[0].Chromaticity.Green.X,
                            (float)images[0].Chromaticity.Green.Y,
                            (float)images[0].Chromaticity.Green.Z,
                            (float)images[0].Chromaticity.Blue.X,
                            (float)images[0].Chromaticity.Blue.Y,
                            (float)images[0].Chromaticity.Blue.Z,
                            (float)images[0].Chromaticity.White.X,
                            (float)images[0].Chromaticity.White.Y,
                            (float)images[0].Chromaticity.White.Z,
                        });
                }else{
                    switch(images[0].ColorSpace){
                    case ImageMagick.ColorSpace.sRGB:
                        toBT2020 = BT709toBT2020;
                        break;
                    case ImageMagick.ColorSpace.Adobe98:
                        toBT2020 = Adobe98toBT2020;
                        break;
                    case ImageMagick.ColorSpace.DisplayP3:
                        toBT2020 = DisplayP3toBT2020;
                        break;
                    default:
                        toBT2020 = Mat3x3.Identity;
                        break;
                    }
                }
                OrigFormat = images[0].Format.ToString();
                AdditionalInfo = GetInfo();
                valid = true;
            } catch (MagickMissingDelegateErrorException ex) {
                Console.WriteLine(ex);
            } catch (Exception ex) {
                Console.WriteLine(ex);
            }
            FullPath = path;
        }

        public TexImage(TexInfo texInfo) {
            try {
                if (texInfo.RGBScale != 1.0f){
                    // WICLoaderでHLG画像を読んだ時、明るすぎるので謎補正する。
                    if (texInfo.BufFormat == Format.RGB16F){
                        unsafe{
                            fixed (byte *buf = texInfo.Buf) {
                                Half *p = (Half *)buf;
                                for (int i=0; i<texInfo.Buf.Length/2; i++){
                                    *p++ *= (Half)texInfo.RGBScale;
                                }
                            }
                        }
                    }else if (texInfo.BufFormat == Format.RGBA16F) {
                        unsafe{
                            fixed (byte *buf = texInfo.Buf) {
                                Half *p = (Half *)buf;
                                for (int i=0; i<texInfo.Buf.Length/2; i+=4){
                                    *p++ *= (Half)texInfo.RGBScale; // R
                                    *p++ *= (Half)texInfo.RGBScale; // G
                                    *p++ *= (Half)texInfo.RGBScale; // B
                                    p++; // A
                                }
                            }
                        }
                    }else if (texInfo.BufFormat == Format.RGB32F){
                        unsafe{
                            fixed (byte *buf = texInfo.Buf) {
                                float *p = (float *)buf;
                                for (int i=0; i<texInfo.Buf.Length/4; i++){
                                    *p++ *= texInfo.RGBScale;
                                }
                            }
                        }
                    }else if (texInfo.BufFormat == Format.RGBA32F){
                        unsafe{
                            fixed (byte *buf = texInfo.Buf) {
                                float *p = (float *)buf;
                                for (int i=0; i<texInfo.Buf.Length/4; i+=4){
                                    *p++ *= texInfo.RGBScale; // R
                                    *p++ *= texInfo.RGBScale; // G
                                    *p++ *= texInfo.RGBScale; // B
                                    p++; // A
                                }
                            }
                        }
                    }
                }

                images = new MagickImageCollection();
                if (texInfo.BufFormat == Format.AUTO){
                    images.Read(texInfo.Buf);
                }else{
                    PluginFormat = texInfo.BufFormat;
                    FormatInfo finfo = Util.GetFormatInfo(texInfo.BufFormat);
                    PixelReadSettings settings = new(
                        texInfo.Width,
                        texInfo.Height,
                        finfo.storageType,
                        finfo.pixelMappig
                    );
                    uint frameSize = finfo.bytePerPixel * texInfo.Width * texInfo.Height;
                    UInt32 offset = 0;
                    for (uint i=0; i<texInfo.numFrames; i++){
                        var image = new MagickImage();
                        if (i > 0 && texInfo.Pages != null){
                            ushort width = texInfo.Pages[i].Width;
                            ushort height = texInfo.Pages[i].Height;
                            settings.ReadSettings.Width = width;
                            settings.ReadSettings.Height = height;
                            frameSize = finfo.bytePerPixel * width * height;
                        }

                        image.ReadPixels(texInfo.Buf, offset, frameSize, settings);
                        if (texInfo.numFrames > 1 && texInfo.Pages != null){
                            image.AnimationDelay = texInfo.Pages[i].Delay;
                            image.GifDisposeMethod = GifDisposeMethod.None;
                            image.Page = new MagickGeometry(texInfo.Pages[i].Left,
                                                            texInfo.Pages[i].Top,
                                                            texInfo.Pages[i].Width,
                                                            texInfo.Pages[i].Height);
                        }

                        if (texInfo.Pages == null){ // animation with rotate not supported yet.
                            switch(texInfo.orientation){
                              default:
                              case 1: // Horizontal (normal)
                                break;
                              case 2: // Mirror horizontal
                                image.Flop();
                                break;
                              case 3: // Rotate 180
                                image.Rotate(180);
                                break;
                              case 4: // Mirror vertical
                                image.Flip();
                                break;
                              case 5: // Mirror horizontal and rotate 270 CW
                                image.Flop();
                                image.Rotate(270);
                                break;
                              case 6: // Rotate 90 CW
                                image.Rotate(90);
                                break;
                              case 7: // Mirror horizontal and rotate 90 CW
                                image.Flop();
                                image.Rotate(90);
                                break;
                              case 8: // Rotate 270 CW
                                image.Rotate(270);
                                break;
                            }
                        }

                        images.Add(image);
                        offset += frameSize;
                    }
                }

                if (images.Count > 1 && texInfo.needCoalesce) images.Coalesce();
                Premultiplied = texInfo.Premultiplied;
                hasGainMap = texInfo.hasGainMap;
                maxCLL = texInfo.maxCLL;
                AdditionalInfo = texInfo.AdditionalString;
                LoadByPlugin = true;
                PluginColorSpace = texInfo.colorSpace;
                PluginTransferFunc = texInfo.transFunc;
                PluginGamma = texInfo.gamma;
                valid = true;
                if (texInfo.hasPrimaries && texInfo.rgbwXYZ != null){
                    toBT2020 = PrimariesToBT2020(texInfo.rgbwXYZ);
                }else{
                    switch(texInfo.colorSpace){
                      default:
                      case TexViewerPlugin.ColorSpace.Linear:
                        toBT2020 = Mat3x3.Identity;
                        break;
                      case TexViewerPlugin.ColorSpace.Unknown:
                      case TexViewerPlugin.ColorSpace.sRGB:
                      case TexViewerPlugin.ColorSpace.BT601: //厳密には少し違う。NTSCとPALでも違う
                      case TexViewerPlugin.ColorSpace.BT709:
                        toBT2020 = BT709toBT2020;
                        break;
                      case TexViewerPlugin.ColorSpace.BT2020:
                        toBT2020 = Mat3x3.Identity;
                        break;
                      case TexViewerPlugin.ColorSpace.Adobe98:
                        toBT2020 = Adobe98toBT2020;
                        break;
                      case TexViewerPlugin.ColorSpace.DciP3:
                        toBT2020 = DciP3toBT2020;
                        break;
                      case TexViewerPlugin.ColorSpace.DisplayP3:
                        toBT2020 = DisplayP3toBT2020;
                        break;
                    }
                }
            } catch (Exception ex) {
                Console.WriteLine(ex);
            }
        }

        public static Mat3x3 PrimariesToBT2020(float[] rgbwXYZ) {
            Mat3x3 src =  new Mat3x3(rgbwXYZ[0], rgbwXYZ[3], rgbwXYZ[6],
                                     rgbwXYZ[1], rgbwXYZ[4], rgbwXYZ[7],
                                     rgbwXYZ[2], rgbwXYZ[5], rgbwXYZ[8]);
            Vec3 wtptNorm = new Vec3(rgbwXYZ[9]/rgbwXYZ[10], 1.0f, rgbwXYZ[11]/rgbwXYZ[10]);
            Mat3x3 BT2020toXYZ = new Mat3x3(0.6369580f, 0.1446169f, 0.1688809f,
                                            0.2627002f, 0.6779981f, 0.0593017f,
                                            0.0000000f, 0.0280727f, 1.0609851f);
            Mat3x3 XYZtoBT2020 = Mat3x3.Invert(BT2020toXYZ);
            Mat3x3 Bradford = new Mat3x3( 0.8951f,  0.2664f, -0.1614f,
                                         -0.7502f,  1.7135f,  0.0367f,
                                          0.0389f, -0.0685f,  1.0296f);
            Mat3x3 BradfordInv = Mat3x3.Invert(Bradford);
            Vec3 D65 = new Vec3(0.95047f, 1.00000f, 1.08883f);
            Vec3 coneSrc = Bradford * wtptNorm;
            Vec3 coneD65 = Bradford * D65;
            Vec3 scale = new Vec3(coneD65.X / coneSrc.X,
                                  coneD65.Y / coneSrc.Y,
                                  coneD65.Z / coneSrc.Z);
            Mat3x3 adapt = BradfordInv * Mat3x3.Diag(scale) * Bradford;
            Vec3 s = Mat3x3.Invert(src) * wtptNorm;
            Mat3x3 m = Mat3x3.ScaleColumns(src, s);
            return XYZtoBT2020 * adapt * m;
        }


        public TexImage(byte[] data, Format format, uint width, uint height) { // for Ctrl-V
            FormatInfo finfo = Util.GetFormatInfo(format);
            PixelReadSettings settings = new(
                width,
                height,
                finfo.storageType,
                finfo.pixelMappig
            );
            images = new MagickImageCollection();
            images.Add(new MagickImage(data, settings));
            valid = true;
        }

        public TexImage(byte[] data, MagickFormat fmt) { // obsolate
            try {
                images = new MagickImageCollection(data, fmt);
                if (images.Count > 1) images.Coalesce();
                valid = true;
            } catch (MagickMissingDelegateErrorException ex) {
                Console.WriteLine(ex);
            } catch (Exception ex) {
                Console.WriteLine(ex);
            }
        }

        public enum ConvOption{
            AsIs, // for Save
            RGBA8or16, // for Ctrl-C
            RGBA8orHalf, // for CanvasBitmap
            HalforRGBA8, // for HDR
            HalforFloat, // for save JpegXR
        }

        // TODO: 厳密には UNORM か UINT で変換式が違う。
        // https://learn.microsoft.com/ja-jp/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-data-conversion
        public Byte[] ToByteArray(out uint depth, ConvOption opt) {
            var image = images[animationIndex];
            depth = image.Depth;

            if (opt == ConvOption.HalforFloat && (depth == 16 || depth == 32)){
                using (var pixels = image.GetPixels()){
                    float[] floatPixels = pixels.ToArray()!;
                    int numPixels = (int)(image.Width * image.Height);
                    int srcChannels = (int)pixels.Channels;
                    if (depth == 16){
                        Half[] halfData = new Half[numPixels * 4]; // RGBA
                        for (int i = 0; i < numPixels; i++) {
                            float r = (srcChannels > 0)? floatPixels[i * srcChannels + 0] : 0;
                            float g = (srcChannels > 1)? floatPixels[i * srcChannels + 1] : 0;
                            float b = (srcChannels > 2)? floatPixels[i * srcChannels + 2] : 0;
                            float a = (srcChannels > 3)? floatPixels[i * srcChannels + 3] : 65535f;

                            if (float.IsInfinity(r)) r = (float)Half.MaxValue;
                            if (float.IsInfinity(g)) g = (float)Half.MaxValue;
                            if (float.IsInfinity(b)) b = (float)Half.MaxValue;
                            if (float.IsInfinity(a)) a = (float)Half.MaxValue;
                            if (float.IsNaN(r)) r = 0.0f;
                            if (float.IsNaN(g)) g = 0.0f;
                            if (float.IsNaN(b)) b = 0.0f;
                            if (float.IsNaN(a)) a = 0.0f;
                            halfData[i * 4 + 0] = (Half)(r/65535f);
                            halfData[i * 4 + 1] = (Half)(g/65535f);
                            halfData[i * 4 + 2] = (Half)(b/65535f);
                            halfData[i * 4 + 3] = (Half)(a/65535f); //1.0f;
                        }
                        var byteData = new byte[halfData.Length * 2];
                        unsafe {
                            fixed (void* srcPtr = &halfData[0], dstPtr = &byteData[0]) {
                                System.Runtime.CompilerServices.Unsafe.CopyBlock(dstPtr, srcPtr, (uint)byteData.Length);
                            }
                        }
                        return byteData;
                    }else{
                        float[] floatData = new float[numPixels * 4]; // RGBA
                        for (int i = 0; i < numPixels; i++) {
                            float r = (srcChannels > 0)? floatPixels[i * srcChannels + 0] : 0;
                            float g = (srcChannels > 1)? floatPixels[i * srcChannels + 1] : 0;
                            float b = (srcChannels > 2)? floatPixels[i * srcChannels + 2] : 0;
                            float a = (srcChannels > 3)? floatPixels[i * srcChannels + 3] : 65535f;

                            if (float.IsInfinity(r)) r = (float)Half.MaxValue;
                            if (float.IsInfinity(g)) g = (float)Half.MaxValue;
                            if (float.IsInfinity(b)) b = (float)Half.MaxValue;
                            if (float.IsInfinity(a)) a = (float)Half.MaxValue;
                            if (float.IsNaN(r)) r = 0.0f;
                            if (float.IsNaN(g)) g = 0.0f;
                            if (float.IsNaN(b)) b = 0.0f;
                            if (float.IsNaN(a)) a = 0.0f;
                            floatData[i * 4 + 0] = (r/65535f);
                            floatData[i * 4 + 1] = (g/65535f);
                            floatData[i * 4 + 2] = (b/65535f);
                            floatData[i * 4 + 3] = (a/65535f); //1.0f;
                        }
                        var byteData = new byte[floatData.Length * 4];
                        unsafe {
                            fixed (void* srcPtr = &floatData[0], dstPtr = &byteData[0]) {
                                System.Runtime.CompilerServices.Unsafe.CopyBlock(dstPtr, srcPtr, (uint)byteData.Length);
                            }
                        }
                        return byteData;
                    }
                }
            }else if (opt == ConvOption.HalforRGBA8 && depth >= 10){
                depth = 16; // Output is R16G16B16A16_Float (using Half)
                using (var pixels = image.GetPixels()){
                    float[] floatPixels = pixels.ToArray()!;
                    int numPixels = (int)(image.Width * image.Height);
                    var halfData = new Half[numPixels * 4]; // RGBA
                    int srcChannels = (int)pixels.Channels;
                    for (int i = 0; i < numPixels; i++) {
                        float r = (srcChannels > 0) ? floatPixels[i * srcChannels + 0] : 0;
                        float g = (srcChannels > 1) ? floatPixels[i * srcChannels + 1] : 0;
                        float b = (srcChannels > 2) ? floatPixels[i * srcChannels + 2] : 0;
                        float a = (srcChannels > 3) ? floatPixels[i * srcChannels + 3] : 65535f;

                        if (float.IsInfinity(r)) r = (float)Half.MaxValue;
                        if (float.IsInfinity(g)) g = (float)Half.MaxValue;
                        if (float.IsInfinity(b)) b = (float)Half.MaxValue;
                        if (float.IsInfinity(a)) a = (float)Half.MaxValue;
                        if (float.IsNaN(r)) r = 0.0f;
                        if (float.IsNaN(g)) g = 0.0f;
                        if (float.IsNaN(b)) b = 0.0f;
                        if (float.IsNaN(a)) a = 0.0f;

                        r /= 65535f;
                        g /= 65535f;
                        b /= 65535f;
                        a /= 65535f;

                        halfData[i * 4 + 0] = (Half)r;
                        halfData[i * 4 + 1] = (Half)g;
                        halfData[i * 4 + 2] = (Half)b;
                        halfData[i * 4 + 3] = (Half)a; //1.0f;
                    }
                    var byteData = new byte[halfData.Length * 2];
                    unsafe {
                        fixed (void* srcPtr = &halfData[0], dstPtr = &byteData[0]) {
                            System.Runtime.CompilerServices.Unsafe.CopyBlock(dstPtr, srcPtr, (uint)byteData.Length);
                        }
                    }
                    return byteData;
                }
            }


            byte[] pixelData = image.ToByteArray(MagickFormat.Rgba);
            if (opt == ConvOption.AsIs) return pixelData;

            int width = (int)image.Width;
            int height = (int)image.Height;

            if (depth == 1) { // 4bpp to 32bpp
                byte[] newData = new byte[width * height * 4];
                unsafe {
                    const byte _ff = 0xff;
                    const byte _00 = 0x00;
                    fixed (byte* _src = pixelData, _dst = newData) {
                        byte* src = _src, dst = _dst;
                        for (int i= 0; i< pixelData.Length; i++) {
                            byte x = *src++;
                            *dst++ = ((x & 0x80)!=0)?_ff:_00;
                            *dst++ = ((x & 0x40)!=0)?_ff:_00;
                            *dst++ = ((x & 0x20)!=0)?_ff:_00;
                            *dst++ = ((x & 0x10)!=0)?_ff:_00;
                            *dst++ = ((x & 0x08)!=0)?_ff:_00;
                            *dst++ = ((x & 0x04)!=0)?_ff:_00;
                            *dst++ = ((x & 0x02)!=0)?_ff:_00;
                            *dst++ = ((x & 0x01)!=0)?_ff:_00;
                        }
                    }
                }

                pixelData = newData;
                depth = 8;
            } else if (depth == 4) { // 16bpp
                byte[] newData = new byte[width * height * 4];
                unsafe {
                    fixed (byte* _src = pixelData, _dst = newData) {
                        byte* src = _src, dst = _dst;
                        for (int i= 0; i< pixelData.Length; i++) {
                            byte x = *src++;
                            byte[] conv = [0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                                           0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff];
                            *dst++ = conv[x>>4];
                            *dst++ = conv[x&0x0f];
                        }
                    }
                }

                pixelData = newData;
                depth = 8;
            } else if (depth == 8) { // 32bpp
            } else if (depth == 10) { // 40bpp to 64bpp
                byte[] newData = new byte[width * height * 8];
                unsafe {
                    fixed (byte *_src = pixelData) {
                        fixed (byte *_dst = newData) {
                            byte* src = _src;
                            UInt16 *dst = (UInt16 *)_dst;
                            for (int i= 0; i< pixelData.Length; i+=5, src+=5) {
                                UInt64 x = ((UInt64)src[0]<<32)+((UInt64)src[1]<<24)+((UInt64)src[2]<<16)+((UInt64)src[3]<<8)+((UInt64)src[4]);
                                ulong r = ((x >> 30) & 0x03ff) * 0xffff / 0x03ff;
                                ulong g = ((x >> 20) & 0x03ff) * 0xffff / 0x03ff;
                                ulong b = ((x >> 10) & 0x03ff) * 0xffff / 0x03ff;
                                ulong a = ((x >> 0) & 0x03ff) * 0xffff / 0x03ff;
                                *dst++ = (UInt16)r;
                                *dst++ = (UInt16)g;
                                *dst++ = (UInt16)b;
                                *dst++ = (UInt16)a;
                            }
                        }
                    }
                }

                pixelData = newData;
                depth = 16;
            } else if (depth == 16) { // 64bpp
            } else if (depth == 32) { // 128bpp
                byte[] newData = new byte[width * height * 8];
                for (int j = 0, i = 0; i < pixelData.Length; i += 16) {
                    newData[j++] = pixelData[i+2];
                    newData[j++] = pixelData[i+3];
                    newData[j++] = pixelData[i+6];
                    newData[j++] = pixelData[i+7];
                    newData[j++] = pixelData[i+10];
                    newData[j++] = pixelData[i+11];
                    newData[j++] = pixelData[i+14];
                    newData[j++] = pixelData[i+15];
                }
                pixelData = newData;
                depth = 16;
            } else {
                Debug.WriteLine("unknown depth;" + depth);
            }

            if (depth == 16 && opt == ConvOption.RGBA8orHalf) { // uint16 to Half & Normalize
                unsafe {
                    fixed (byte *_src = pixelData) {
                        UInt16* src = (UInt16*)_src;
                        Half* dst = (Half*)_src;
                        for (int i = 0; i < width * height * 4 * 2; i += 2) {
                            *dst++ = (Half)(*src++ / 65535.0f);
                        }
                    }
                }
            }
            return pixelData;
        }


        //------------------------------------------------------------------------------
        public string GetRGB(int u, int v) {
            if ((u < 0) || (u >= images[animationIndex].Width) ||
                (v < 0) || (v >= images[animationIndex].Height)) return "";
            using (var pixels = images[animationIndex].GetPixels()) {
                var color = pixels.GetPixel(u, v).ToColor();
                if (color == null ||
                    float.IsNaN(color.R) || float.IsNaN(color.G) ||
                    float.IsNaN(color.B) || float.IsNaN(color.A)) {
                    return "";
                }
                //Debug.WriteLine(color.R+" "+color.G+" "+color.B+" "+color.A);
                if (color.R < 0) color.R = 0;  // psd は謎の負の値が来る
                if (color.G < 0) color.G = 0;
                if (color.B < 0) color.B = 0;
                if (images[animationIndex].IsOpaque) {
                    return string.Format("#{0:X2}{1:X2}{2:X2}",
                                         (int)(Math.Min(color.R, 0xffff) / 256),
                                          (int)(Math.Min(color.G, 0xffff) / 256),
                                          (int)(Math.Min(color.B, 0xffff) / 256));
                } else {
                    return string.Format("#{0:X2}{1:X2}{2:X2}_{3:X2}",
                                          (int)(Math.Min(color.R, 0xffff) / 256),
                                          (int)(Math.Min(color.G, 0xffff) / 256),
                                          (int)(Math.Min(color.B, 0xffff) / 256),
                                          (int)(Math.Min(color.A, 0xffff) / 256));
                }
            }
        }

        public int FlipH() {
            foreach (MagickImage img in images.Cast<MagickImage>()) {
                img.Flop();
            }
            return 0;
        }
        public int FlipV() {
            foreach (MagickImage img in images.Cast<MagickImage>()) {
                img.Flip();
            }
            return 0;
        }
        public int Rotate(float degree = 90.0f) {
            foreach (MagickImage img in images.Cast<MagickImage>()) {
                img.Rotate(degree);
            }
            return 0;
        }

        public int Resize(uint w, uint h, ImageMagick.FilterType fType) {
            if (images.Count > 1) images.Coalesce();
            MagickGeometry geo = new MagickGeometry(0,0, w,h);
            geo.IgnoreAspectRatio = true;
            foreach (MagickImage img in images.Cast<MagickImage>()) {
                img.FilterType = fType;
                img.Resize(geo);
                img.FilterType = FilterType.Undefined;
            }
            return 0;
        }

        public int Crop(int x, int y, int w, int h) {
            foreach (MagickImage img in images.Cast<MagickImage>()) {
                img.Crop(new MagickGeometry(x, y, (uint)w, (uint)h));
                img.ResetPage();
            }
            return 0;
        }

        public int Mosaic(int x, int y, int w, int h, double slider) {
            foreach (MagickImage img in images.Cast<MagickImage>()) {
                var im2 = img.CloneArea(new MagickGeometry(x, y, (uint)w, (uint)h));
                im2.BackgroundColor = MagickColor.FromRgba(0,0,0,0);

                double rate = 1.0 / slider;
                // 0.01 = 1/100 = 100dot
                // 0.10 = 1/10  =  10dot
                // 0.25 = 1/4   =   4dot

                Debug.WriteLine($"0: {im2.Width}x{im2.Height} {rate}");
                im2.Rotate(45);
                im2.ResetPage();
                uint rx = im2.Width;
                uint ry = im2.Height;
                if (rx*rate < 1 || ry*rate < 1){
                    Debug.WriteLine("too small");
                    rate = Math.Min(1.0/rx, 1.0/ry);
                }
                im2.Sample(new Percentage(100*rate));
                im2.Sample(rx, ry);
                im2.Rotate(-45);
                im2.ResetPage();

                MagickGeometry geo = new((int)(im2.Width/2-w/2), (int)(im2.Height/2-h/2), (uint)w, (uint)h);
                im2.Crop(geo);

                img.Composite(im2, x, y, CompositeOperator.Over);
                im2.Dispose();
            }
            return 0;
        }

        public bool SaveIM(string path){
            try {
                images.Write(path);
                return true;
            }catch(Exception){
                return false;
            }
        }

        public string GetInfo() {
            if ((images == null) || (images.Count == 0)) {
                return "(not loaded)";
            }
            //int  numFrame = images.Count;
            MagickImage image = (MagickImage)images[animationIndex];
            string str ="";
            if (image.Density.X != 0 || image.Density.Y != 0) {
                str += "Density: " + image.Density.ToString() + "\n";
            }
            str += "ColorSpace: " + image.ColorSpace.ToString() + "\n";
            str += "ColorType: " + image.ColorType.ToString() + "\n";
            str += "ChannelCount: " + image.ChannelCount.ToString() + "\n";
            //str += "ColormapSize: " + image.ColormapSize.ToString() + "\n";
            str += "Depth: " + image.Depth.ToString() + "\n";
            if (image.Endian != Endian.Undefined) {
                str += "Endian: " + image.Endian.ToString() + "\n";
            }
            str += "Format: " + image.Format.ToString() + "\n";
            double g = image.Gamma;
            if (g != 0.0){
                g = 1/g;
                str += "Gamma: " + g.ToString() + "\n";
            }
            str += "Interlace: " + image.Interlace.ToString() + "\n";
            str += "IsOpaque: " + image.IsOpaque.ToString() + "\n";
            str += "Orientation: " + image.Orientation.ToString() + "\n";
            str += "Page: " + image.Page.ToString() + "\n";
            if (image.Quality != 0) {
                str += "Quality: " + image.Quality.ToString() + "\n";
            }
            //str += "TotalColors: " + image.TotalColors.ToString() + "\n";
            //str += "Comment: " + image.Comment?.ToString() + "\n";

            ColorProfile? colp = (ColorProfile?)image.GetColorProfile();
            if (colp != null){
                str += "\nColorProfile:\n";
                str += "ColorSpace: " + colp.ColorSpace.ToString() + "\n";
                if (!string.IsNullOrEmpty(colp.Copyright)){
                    str += "Copyright: '" + colp.Copyright+"'\n";
                }
                if (!string.IsNullOrEmpty(colp.Description)){
                    str += "Description: '" + colp.Description+"'\n";
                }
                if (!string.IsNullOrEmpty(colp.Manufacturer)){
                    str += "Manufacturer: '" + colp.Manufacturer+"'\n";
                }
                if (!string.IsNullOrEmpty(colp.Model)){
                    str += "Model: '" + colp.Model+"'\n";
                }
                if (!string.IsNullOrEmpty(colp.Name)){
                    str += "Name: " + colp.Name+"\n";
                }
            }

            ExifProfile? exif = (ExifProfile?)image.GetExifProfile();
            if (exif != null){
                str += "\nExif:\n";
                foreach (var val in exif.Values) {
                    if (!val.IsArray){
                        str += val.Tag + ": " + val.ToString() + "\n";
                    }
                }
            }

            /* https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
            EightBimProfile? ebim = (EightBimProfile?)image.Get8BimProfile();
            if (ebim != null){
                str += "\n8Bim:\n";
                foreach (var val in ebim.Values) {
                    str += val.Id + ": " + val.ToString(System.Text.Encoding.UTF8) + "\n";
                }
            }
            */

            IptcProfile? iptc = (IptcProfile?)image.GetIptcProfile();
            if (iptc != null){
                str += "\nIptc:\n";
                foreach (var val in iptc.Values) {
                    string valstr = Regex.Replace(val.Value.ToString(), "\0", string.Empty, RegexOptions.Multiline);
                    valstr = Regex.Replace(valstr, @"[^\P{C}\n]+", string.Empty, RegexOptions.Multiline);
                    str += val.Tag.ToString() + ": " + valstr + "\n";
                }
            }

            XmpProfile? xmp = (XmpProfile?)image.GetXmpProfile();
            if (xmp != null){
                /*
                System.Xml.Linq.XDocument? xdoc = xmp.ToXDocument();
                if (xdoc != null){
                    str += "\nXmp:\n";
                    string xmpstr = Regex.Replace(xdoc.ToString(), @"\s{16,}", string.Empty, RegexOptions.Multiline);
                    xmpstr = Regex.Replace(xmpstr, "^[\r\n]+", string.Empty, RegexOptions.Multiline);
                    str += xmpstr;
                }
                */
                str += "(XMP metadata)\n";
            }

            return str;
        }


        public string DebugInfo() {
            if ((images == null) || (images.Count == 0)) {
                return "(not loaded)";
            }
            uint w = images[animationIndex].Width;
            uint h = images[animationIndex].Height;
            uint d = images[animationIndex].Depth;
            string name = images[animationIndex].FileName ?? "(unknown)";
            return $"{name} ({w}x{h}) {d}bit";
        }

        void fillImage(ref byte[] buf, int width, int height,
                       int x, int y, int w, int h,
                       float r_, float g_, float b_) {
            byte[] r = BitConverter.GetBytes(r_);
            byte[] g = BitConverter.GetBytes(g_);
            byte[] b = BitConverter.GetBytes(b_);
            if (x+w > width) w = width-x;
            if (y+h > height) h = height-y;
            for (int i=y; i<y+h; i++){
                for (int j=x; j<x+w; j++){
                    Buffer.BlockCopy(r, 0, buf, (width * i + j)*12+0, 4);
                    Buffer.BlockCopy(g, 0, buf, (width * i + j)*12+4, 4);
                    Buffer.BlockCopy(b, 0, buf, (width * i + j)*12+8, 4);
                }
            }
        }

    }
}
