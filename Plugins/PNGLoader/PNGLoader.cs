using System.Runtime.InteropServices;
using TexViewerPlugin;

namespace PNGLoader
{
    public class PNGLoader : ITexViewerPlugin
    {
        public string PluginName => "PNG Loader";
        public string PluginDescription => "Load images using libpng";
        public string SupportedExtensions => "png,apng";
        public UInt16 DefaultPriority => 300;

        [DllImport("PNGLoaderLib.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr get_version();
        [DllImport("PNGLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_file(string file, out LoadResult result, UInt32 opt);
        [DllImport("PNGLoaderLib.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void free_mem(LoadResult result);


        public string PluginVersion {
            get {
                IntPtr ptr = get_version();
                string version = Marshal.PtrToStringAnsi(ptr) ?? "version unknown";
                return version;
            }
        }

        public bool TryLoad(string path, out TexInfo? texInfo, UInt32 opt) {
            texInfo = null;
            LoadResult result;
            if (load_file(path, out result, opt) == 0){
                texInfo = new TexInfo((TexViewerPlugin.Format)result.format, result.width, result.height, result.numFrames, result.colorspace, result.flag) {
                    Buf = new byte[result.dst_len],
                    FormatString = Marshal.PtrToStringAnsi(result.format_string)??"",
                    AdditionalString = Marshal.PtrToStringAnsi(result.loader_string)??"",
                    maxCLL = result.maxCLL,
                    gamma = result.gamma,
                    Pages = (result.numFrames>1)?new Page[result.numFrames]:null,
                };
                if (texInfo.hasPrimaries){
                    texInfo.rgbwXYZ = new float[] {
                        result.rX, result.rY, result.rZ,
                        result.gX, result.gY, result.gZ,
                        result.bX, result.bY, result.bZ,
                        result.wX, result.wY, result.wZ,
                    };
                }
                Marshal.Copy(result.dst, texInfo.Buf, 0, (int)result.dst_len);
                if (texInfo.Pages != null) {
                    unsafe {
                        ushort *p = (ushort *)result.pages;
                        for (int i=0; i<result.numFrames; i++){
                            texInfo.Pages[i].Top = *p++;
                            texInfo.Pages[i].Left = *p++;
                            texInfo.Pages[i].Width = *p++;
                            texInfo.Pages[i].Height = *p++;
                            texInfo.Pages[i].Delay = *p++;
                        }
                    }
                }

                free_mem(result);
                return true;
            }
            return false;
        }
    }
}
