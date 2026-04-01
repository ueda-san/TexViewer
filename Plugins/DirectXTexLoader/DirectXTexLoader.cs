using System.Runtime.InteropServices;
using TexViewerPlugin;

namespace DirectXTexLoader
{
    public class DirectXTexLoader : ITexViewerPlugin
    {
        public string PluginName => "DirectX Tex Loader";
        public string PluginDescription => "Load images using DirectXTex texture processing library";
        public string SupportedExtensions => "dds,hdr,tga"; //,jxr,hdp,wdp,heic,heif,hif,webp,bmp,jpg,jpeg,png,tif,tiff,ico";
        // .gif: DecodeMultiframe()がリサイズも行うので画像の一部を書き換えるタイプに対応できない？
        public UInt16 DefaultPriority => 250;

        [DllImport("DirectXTexLoaderLib.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr get_version();
        [DllImport("DirectXTexLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_dds(string file, out LoadResult result);
        [DllImport("DirectXTexLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_hdr(string file, out LoadResult result);
        [DllImport("DirectXTexLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_tga(string file, out LoadResult result);
        [DllImport("DirectXTexLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_wic(string file, out LoadResult result);
        [DllImport("DirectXTexLoaderLib.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void free_mem(LoadResult result);


        public string PluginVersion {
            get {
                IntPtr ptr = get_version();
                string version = Marshal.PtrToStringAnsi(ptr) ?? "version unknown";
                return version;
            }
        }

        public bool TryLoad(string path, out TexInfo? texInfo, UInt32 _)
        {
            texInfo = null;
            LoadResult result;
            if (path.EndsWith(".hdr")){
                int errcode = load_hdr(path, out result);
                if (errcode != 0) return false;
            }else if (path.EndsWith(".tga")){
                int errcode = load_tga(path, out result);
                if (errcode != 0) return false;
            }else if (path.EndsWith(".dds")){
                int errcode = load_dds(path, out result);
                if (errcode != 0) return false;
            }else{
                // unused: see -> SupportedExtensions
                int errcode = load_wic(path, out result);
                if (errcode != 0) return false;
            }
            texInfo = new TexInfo((TexViewerPlugin.Format)result.format, result.width, result.height, result.numFrames, result.colorspace, result.flag) {
                Buf = new byte[result.dst_len],
                maxCLL = result.maxCLL,
                gamma = result.gamma,
                FormatString = Marshal.PtrToStringAnsi(result.format_string)??"",
                AdditionalString = Marshal.PtrToStringAnsi(result.loader_string)??"",
            };
            Marshal.Copy(result.dst, texInfo.Buf, 0, (int)result.dst_len);
            free_mem(result);

            return true;
        }

    }
}
