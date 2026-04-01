using System.Runtime.InteropServices;
using TexViewerPlugin;

namespace BasisLoader
{
    public class BasisLoader : ITexViewerPlugin
    {
        public string PluginName => "Basis Loader";
        public string PluginDescription => "Load images using Binomial basis_universal";
        public string SupportedExtensions => "basis,ktx2";
        public UInt16 DefaultPriority => 210;

        [DllImport("BasisLoaderLib.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr get_version();
        [DllImport("BasisLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern int load_byte(byte[] data, int size, out LoadResult result);
        [DllImport("BasisLoaderLib.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern void free_mem(LoadResult result);

        public string PluginVersion {
            get {
                IntPtr ptr = get_version();
                string version = Marshal.PtrToStringAnsi(ptr) ?? "version unknown";
                return version;
            }
        }

        public bool TryLoad(byte[] data, out TexInfo? texInfo, UInt32 _) {
            texInfo = null;
            LoadResult result;
            if (load_byte(data, data.Length, out result) == 0){
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
