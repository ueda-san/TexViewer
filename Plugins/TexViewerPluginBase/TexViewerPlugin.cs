// TexViewer plugin interface

using System.Runtime.InteropServices;


// Plugin filenames must not end with *Lib.dll (will be skipped)
namespace TexViewerPlugin
{
    public enum Format
    {
        UNKNOWN,
        AUTO,      // auto detect from data
        A8,
        R8,
        R16,
        RG16,

        RGB565,
        RGBA5551,
        RGBA4444,
        RGB888,
        BGR888,
        RGBA8888,
        BGRA8888,

        RGB16,
        RGB16F,
        RGB32F,
        RGBA16,
        RGBA16F,
        RGBA32F,
        RGBA1010102,

        numFormat
    }

    public enum ColorSpace
    {
        Unknown,
        Linear, // scRGB?
        sRGB,   // sRGB / Gamma
        BT601,
        BT709,
        BT2020,
        Adobe98,
        DciP3,
        DisplayP3,

        //BT1886, ?
        //BT2100, ?
        //LINEAR_SRGB ?
        //SCRGB ?
    }

    public enum TransferFunc
    {
        Default    = 0,
        Linear     = 1<<8,
        sRGB       = 2<<8,
        BT601      = 3<<8,
        BT709      = 4<<8,
        BT2020     = 5<<8,
        Adobe98    = 6<<8,
        DCI_P3     = 7<<8,
        DISPLAY_P3 = 8<<8, //==sRGB,
        PQ         = 100<<8,
        HLG        = 101<<8,
        Gamma_N    = 102<<8,
    }

    public struct Page
    {
        public UInt16 Top;
        public UInt16 Left;
        public UInt16 Width;
        public UInt16 Height;
        public UInt16 Delay;
    }

    public class TexInfo
    {
        public TexInfo(Format fmt = Format.UNKNOWN, UInt32 w = 0, UInt32 h = 0, UInt32 p=1, UInt32 c=0, UInt32 flag=0) {
            BufFormat = fmt;
            Width = w;
            Height = h;
            numFrames = (ushort)p;
            colorSpace =  (ColorSpace)(c & 0x00ff);
            transFunc = (TransferFunc)(c & 0xff00);
            Premultiplied = (flag & 0x0001) != 0;
            needCoalesce  = (flag & 0x0002) != 0;
            hasGainMap    = (flag & 0x0004) != 0;
            hasPrimaries  = (flag & 0x0008) != 0;
        }
        public Format BufFormat;
        public UInt32 Width;
        public UInt32 Height;
        public UInt16 numFrames;
        public UInt32 orientation = 1;
        public ColorSpace colorSpace;
        public TransferFunc transFunc;
        public UInt32 maxCLL = 0;
        public float gamma = 1.0f;
        public bool Premultiplied = false;
        public bool needCoalesce = false;
        public bool hasGainMap = false;
        public bool hasPrimaries = false;
        public float[]? rgbwXYZ = null;
        public Page[]? Pages = null;      // for animation gif
        public required byte[] Buf;
        public string FormatString = "";     // human readable source format description
        public string AdditionalString = ""; // human readable metadata, exif etc.
        public float RGBScale = 1.0f;
    }

    public interface ITexViewerPlugin
    {
        private const int PluginAPIVersion = 1;
        public int APIVersion { get => PluginAPIVersion; }

        public string PluginName { get; }           // "Super Deluxe Jpeg Loader"
        public string PluginVersion { get; }        // "0.0.1 alpha1"
        public string PluginDescription { get; }    // "Load Jpeg files"
        public string SupportedExtensions { get; }  // "jpeg,jpg,jxr" without space
        public UInt16 DefaultPriority { get; }      // 0..999 default order of plugin manager

        // Implement at least one or more Load methods
        // load opt:  bit0:ShowAllExif  bit1:requestSDR
        public bool TryLoad(byte[] data, out TexInfo? texInfo, UInt32 opt) { texInfo = null; return false; }
        public bool TryLoad(string path, out TexInfo? texInfo, UInt32 opt) { texInfo = null; return false; }

        // save opt:  not use yet.
        public bool TrySave(TexInfo texInfo, string path, UInt32 opt) { return false; }
    }

    //------------------------------------------------------------------------------
    // for c++ dll
    [StructLayout(LayoutKind.Sequential)]
    public struct LoadResult
    {
        public IntPtr dst;
        public UInt32 dst_len;
        public UInt32 width;
        public UInt32 height;
        public UInt32 numFrames;
        public UInt32 format;
        public UInt32 colorspace;
        public UInt32 flag;
        public UInt32 maxCLL;
        public UInt32 param1;
        public UInt32 orientation;

        public IntPtr pages;
        public float gamma;
        public float rX,rY,rZ;
        public float gX,gY,gZ;
        public float bX,bY,bZ;
        public float wX,wY,wZ;
        public float RGBScale;

        public IntPtr format_string;
        public IntPtr loader_string;
    }

}
