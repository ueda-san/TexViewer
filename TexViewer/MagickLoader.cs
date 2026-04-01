using ImageMagick;
using System;
using TexViewerPlugin;

namespace MagickLoader
{
    public class MagickLoader : ITexViewerPlugin
    {
        public string PluginName => "Magick Loader";
        public string PluginDescription => "Load images in over 100 formats using ImageMagick";
        public string SupportedExtensions => "*";
        public UInt16 DefaultPriority => 900;
        public string PluginVersion { get { return MagickNET.Version; } }

        // Dummy. Only use PluginManager
    }
}
