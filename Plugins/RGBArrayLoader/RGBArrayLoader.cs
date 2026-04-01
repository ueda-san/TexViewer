using System.Text.RegularExpressions;
using TexViewerPlugin;

/*
  Load RGB Array without header

  test1x2.bin
  -> width:1, height:2

  if filesize / (width*height) ==
  3  -> format:RGB   8bit
  4  -> format:RGBA  8bit
  6  -> format:RGB  16bit
  8  -> format:RGBA 16bit
  12 -> format:RGB  32bit float
  16 -> format:RGBA 32bit float
 */

namespace RGBArrayLoader
{
    public class RGBArrayLoader : ITexViewerPlugin
    {
        public string PluginName => "RGBArray Loader";
        public string PluginDescription => "Load rgb(a) array whose filenames contain (W)x(H)";
        public string SupportedExtensions => "*";
        public UInt16 DefaultPriority => 990;

        public string PluginVersion { get { return "0.01"; } }

        public bool TryLoad(string path, out TexInfo? texInfo, UInt32 _) {
            texInfo = null;
            string pattern = @"(\d+)x(\d+)";
            Match match = Regex.Match(Path.GetFileName(path), pattern);
            if (match.Success) {
                int w,h;
                if (int.TryParse(match.Groups[1].Value, out w) &&
                    int.TryParse(match.Groups[2].Value, out h)){
                    if (w > 0 && w < 9999 && h > 0 && h < 9999){
                        if (File.Exists(path)) {
                            using (var fs = new FileStream(path, FileMode.Open))
                            using (var reader = new BinaryReader(fs)) {
                                int len = (int)fs.Length;
                                byte[] data = new byte[len];
                                reader.Read(data, 0, len);

                                if (len % (w*h) != 0) return false;
                                Format fmt = Format.UNKNOWN;
                                switch(len / (w*h)){
                                  case 3:  fmt = Format.RGB888;   break;
                                  case 4:  fmt = Format.RGBA8888; break;
                                  case 6:  fmt = Format.RGB16;    break;
                                  case 8:  fmt = Format.RGBA16;   break;
                                  case 12: fmt = Format.RGB32F;   break;
                                  case 16: fmt = Format.RGBA32F;  break;
                                  default:
                                    break;
                                }
                                if (fmt == Format.UNKNOWN) return false;

                                texInfo = new TexInfo(fmt, (uint)w, (uint)h) {
                                    Buf = data,
                                    FormatString = fmt.ToString(),
                                    AdditionalString = $"size: {w}x{h}",
                                };
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    }
}
