using System.Diagnostics;
using System.Runtime.InteropServices;
using TexViewerPlugin;

namespace PVRLoader
{
    public class PVRLoader : ITexViewerPlugin
    {
        public string PluginName => "PVR Loader";
        public string PluginDescription => "Load images using Imagination Technologies PVRTexTool";
        public string SupportedExtensions => "astc,dds,ktx,ktx2,pvr,exr,hdr,psd";
        //psdはファイルのよってはおかしい。MagickLoaderで読めない場合に備えて残す？
        //basisは一応読めるけど微妙
        //ppm,pgm,pic は問題なさそうだけどMagickLoaderで良いかな？
        public UInt16 DefaultPriority => 230;

        [DllImport("PVRLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_file(string file, out LoadResult result);
        [DllImport("PVRLoaderLib.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void free_mem(LoadResult result);
        [DllImport("PVRLoaderLib.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr get_version();

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
            int ret = load_file(path, out result);
            if (ret == 0){
                texInfo = new TexInfo((TexViewerPlugin.Format)result.format, result.width, result.height, result.numFrames, result.colorspace, result.flag) {
                    Buf = new byte[result.dst_len],
                    FormatString = Marshal.PtrToStringAnsi(result.format_string)??"",
                    AdditionalString = Marshal.PtrToStringAnsi(result.loader_string)??"",
                };
                Marshal.Copy(result.dst, texInfo.Buf, 0, (int)result.dst_len);
                free_mem(result);
                return true;
            }else if (ret == -99){
                Trace.WriteLine("loader return CHECK_ME");
            }
            return false;
        }
    }
}
