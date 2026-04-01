using System.Runtime.InteropServices;
using TexViewerPlugin;

namespace Compressonator
{
    public class Compressonator : ITexViewerPlugin
    {
        public string PluginName => "Compressonator DDS Loader";
        public string PluginDescription => "Load images using AMD Compressonator";
        public string SupportedExtensions => "dds";
        public UInt16 DefaultPriority => 240;

        [DllImport("CompressonatorLib.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr get_version();
        [DllImport("CompressonatorLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_file(string file, out LoadResult result);
        [DllImport("CompressonatorLib.dll", CallingConvention = CallingConvention.Cdecl)]
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
            if (load_file(path, out result) == 0){
                texInfo = new TexInfo((TexViewerPlugin.Format)result.format, result.width, result.height, result.numFrames, result.colorspace, result.flag) {
                    Buf = new byte[result.dst_len],
                    FormatString = Marshal.PtrToStringAnsi(result.format_string)??"",
                    AdditionalString = Marshal.PtrToStringAnsi(result.loader_string)??"",
                };
                Marshal.Copy(result.dst, texInfo.Buf, 0, (int)result.dst_len);
                free_mem(result);
                return true;
            }
            return false;
        }
    }
}
