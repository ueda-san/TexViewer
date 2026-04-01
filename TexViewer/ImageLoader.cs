using ImageMagick;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using TexViewerPlugin;
using Windows.Storage;

namespace TexViewer
{
    public class ImageLoader
    {
        //------------------------------------------------------------------------------
        public static TexImage? Load(StorageFile file, uint opt, bool withDisabledPlugin=false) {
            return Load(file.Path, opt, withDisabledPlugin);
        }

        public static TexImage? Load(string path, uint opt, bool withDisabledPlugin=false) {
            var pluginManager = PluginManager.Instance;
            string ext = Path.GetExtension(path);
            TexImage? texImage = null;
            byte[]? data = null;

            List<string> pluginNames = pluginManager.GetPreferredList(ext, withDisabledPlugin);
            foreach (var name in pluginNames) {
                if (name.Equals("Magick Loader")) {
                    texImage = new TexImage(path);
                    texImage.Plugin = name;
                } else {

                    var pluginInfo = pluginManager.GetPluginInfo(name);
                    if (pluginInfo != null) {
                        TexInfo? texInfo = null;
                        if (pluginInfo.CanLoadFile) {
                            if (!pluginInfo.Plugin.TryLoad(path, out texInfo, opt)) continue;
                        } else if (pluginInfo.CanLoadByte) {
                            if (data == null) data = File.ReadAllBytes(path);
                            if (!pluginInfo.Plugin.TryLoad(data, out texInfo, opt)) continue;
                        }
                        if (texInfo == null) continue;

                        ConvertFormat(ref texInfo);
                        var finfo = Util.GetFormatInfo(texInfo.BufFormat);
                        if (finfo.storageType == StorageType.Undefined) {
                            texImage = new TexImage(texInfo.Buf, finfo.magickFormat){
                                FullPath = path
                            };
                        } else {
                            texImage = new TexImage(texInfo){
                                FullPath = path
                            };
                        }
                        texImage.Plugin = name;
                        texImage.OrigFormat = texInfo.FormatString;
                    }
                }
                if ((texImage != null) && (texImage.valid)) {
                    Debug.WriteLine("**" + name + "/" + texImage.DebugInfo());
                    return texImage;
                }
            }
            return null;
        }


        //------------------------------------------------------------------------------
        static byte bit4to8(int val){ return (byte)((val<<4)|val); }
        static byte bit5to8(int val){ return (byte)((val<<3)|(val>>2)); }
        static byte bit6to8(int val){ return (byte)((val<<2)|(val>>4)); }
        static ushort bit10to16(uint val){ return (ushort)((val<<6)|(val>>4)); }
        static ushort bit2to16(uint val){
            ushort[] conv = { 0x0000,0x5555,0xaaaa,0xffff };
            return conv[val%0x03];
        }

        static void ConvertFormat(ref TexInfo texInfo)
        {
            Format fmt = texInfo.BufFormat;

            if (fmt == Format.A8) {
            }else if (fmt == Format.R8) {
            }else if (fmt == Format.R16) {
            }else if (fmt == Format.RG16) {
            }else if (fmt == Format.RGB565) {
                int pixels = texInfo.Buf.Length / 2;
                byte[] newBuf = new byte[pixels * 3];
                unsafe {
                    fixed (byte *_src = texInfo.Buf, _dst = newBuf) {
                        ushort *src = (ushort*)_src;
                        byte *dst = (byte*)_dst;
                        for (int i=0; i<pixels; i++) {
                            ushort col = *src++;
                            *dst++ = bit5to8((col>>11)&0x1f);
                            *dst++ = bit6to8((col>>5)&0x3f);
                            *dst++ = bit5to8(col&0x1f);
                        }
                    }
                }
                texInfo.Buf = newBuf;
                texInfo.BufFormat = Format.RGB888;
            }else if (fmt == Format.RGBA5551) {
                int pixels = texInfo.Buf.Length / 2;
                byte[] newBuf = new byte[pixels * 4];
                unsafe {
                    fixed (byte *_src = texInfo.Buf, _dst = newBuf) {
                        ushort *src = (ushort*)_src;
                        byte *dst = (byte*)_dst;
                        for (int i=0; i<pixels; i++) {
                            ushort col = *src++;
                            *dst++ = bit5to8((col>>11)&0x1f);
                            *dst++ = bit5to8((col>>6)&0x1f);
                            *dst++ = bit5to8((col>>1)&0x1f);
                            *dst++ = (byte)(((col&0x0001)!=0)?0xff:0x00);
                        }
                    }
                }
                texInfo.Buf = newBuf;
                texInfo.BufFormat = Format.RGBA8888;
            }else if (fmt == Format.RGBA4444) {
                int pixels = texInfo.Buf.Length / 2;
                byte[] newBuf = new byte[pixels * 4];
                unsafe {
                    fixed (byte *_src = texInfo.Buf, _dst = newBuf) {
                        ushort *src = (ushort*)_src;
                        byte *dst = (byte*)_dst;
                        for (int i=0; i<pixels; i++) {
                            ushort col = *src++;
                            *dst++ = bit4to8((col>>12)&0x1f);
                            *dst++ = bit4to8((col>>8)&0x1f);
                            *dst++ = bit4to8((col>>4)&0x1f);
                            *dst++ = bit4to8(col&0x1f);
                        }
                    }
                }
                texInfo.Buf = newBuf;
                texInfo.BufFormat = Format.RGBA8888;
            }else if (fmt == Format.RGB888) {
            }else if (fmt == Format.RGBA8888) {
            }else if (fmt == Format.BGR888) {
                for (int i=0; i<texInfo.Buf.Length; i+=3){
                    byte x = texInfo.Buf[i];
                    texInfo.Buf[i] = texInfo.Buf[i+2];
                    texInfo.Buf[i+2] = x;
                }
                texInfo.BufFormat = Format.RGB888;
            }else if (fmt == Format.BGRA8888) {
                for (int i=0; i<texInfo.Buf.Length; i+=4){
                    byte x = texInfo.Buf[i];
                    texInfo.Buf[i] = texInfo.Buf[i+2];
                    texInfo.Buf[i+2] = x;
                }
                texInfo.BufFormat = Format.RGBA8888;
            }else if (fmt == Format.RGB16) {
            }else if (fmt == Format.RGB16F) {
                int pixels = texInfo.Buf.Length / 6;
                byte[] newBuf = new byte[pixels * 12];
                unsafe {
                    fixed (byte *_src = texInfo.Buf, _dst = newBuf) {
                        //Half max = (Half)0;
                        Half *src = (Half*)_src;
                        float *dst = (float*)_dst;
                        for (int i=0; i<pixels*3; i++){
                            //if (*src > max) max = *src;
                            *dst++ = (float)(*src++);
                        }
                        //Debug.WriteLine("Half max:"+max);
                    }
                }
                texInfo.Buf = newBuf;
                texInfo.BufFormat = Format.RGB32F;
            }else if (fmt == Format.RGB32F) {
            }else if (fmt == Format.RGBA16) {
            }else if (fmt == Format.RGBA16F) {
                int pixels = texInfo.Buf.Length / 8;
                byte[] newBuf = new byte[pixels * 16];
                unsafe {
                    fixed (byte *_src = texInfo.Buf, _dst = newBuf) {
                        //Half max = (Half)0;
                        Half *src = (Half*)_src;
                        float *dst = (float*)_dst;
                        for (int i=0; i<pixels*4; i++){
                            //if (*src > max) max = *src;
                            *dst++ = (float)(*src++);
                        }
                        //Debug.WriteLine("Half max:"+max);
                    }
                }
                texInfo.Buf = newBuf;
                texInfo.BufFormat = Format.RGBA32F;
            }else if (fmt == Format.RGBA32F) {
                /*
                unsafe {
                    fixed (byte *_src = texInfo.Buf) {
                        //float max = 0.0f;
                        //float sum = 0.0f;
                        float *src = (float*)_src;
                        for (int i=0; i<texInfo.Buf.Length; i+=4){
                            //sum += *src;
                            //if (*src > max) max = *src;
                            //*src = *src/3.0f;
                            src++;
                        }
                        //Debug.WriteLine("float max:"+max+" avr:"+sum/(texInfo.Buf.Length/4));
                    }
                }
                */
            }else if (fmt == Format.RGBA1010102) {
                int pixels = texInfo.Buf.Length / 4;
                byte[] newBuf = new byte[pixels * 8];
                unsafe {
                    fixed (byte *_src = texInfo.Buf, _dst = newBuf) {
                        uint *src = (uint*)_src;
                        ushort *dst = (ushort*)_dst;
                        for (int i=0; i<pixels; i++){
                            uint col = *src++; //libultrahdr: R9:0, G19:10, B29:20, A31:30.
                            *dst++ = bit10to16(col&0x03ff);
                            *dst++ = bit10to16((col>>10)&0x03ff);
                            *dst++ = bit10to16((col>>20)&0x03ff);
                            *dst++ = 0xffff; ///*bit2to16*/(ushort)((col>>30)&0x0003);
                        }
                    }
                }
                texInfo.Buf = newBuf;
                texInfo.BufFormat = Format.RGBA16;
            }
        }


        public static bool Save(TexImage texImage, string path) {
            var pluginManager = PluginManager.Instance;
            string ext = Path.GetExtension(path);
            List<string> pluginNames = pluginManager.GetPreferredList(ext);
            foreach (var name in pluginNames) {
                if (name.Equals("Magick Loader")) {
                    if (MagickNET.SupportedFormats.Any(format => format.SupportsWriting && format.Format.ToString().Equals(ext.Substring(1), StringComparison.OrdinalIgnoreCase))){
                        return texImage.SaveIM(path);
                    }
                }else{
                    var pInfo = pluginManager.GetPluginInfo(name);
                    if (pInfo != null){
                        if (pInfo.CanSaveByte){
                            uint width = texImage.Width;
                            uint height = texImage.Height;
                            TexInfo texInfo;
                            if (texImage.IsSDR){
                                texInfo  = new TexInfo(Format.RGBA8888, width, height){
                                    Buf = new Byte[width * height * 4]
                                };
                                uint depth;
                                Buffer.BlockCopy(texImage.ToByteArray(out depth, TexImage.ConvOption.AsIs),
                                                 0, texInfo.Buf, 0, texInfo.Buf.Length);
                            }else{
                                Format dstFmt = (texImage.BitDepth==16)?Format.RGBA16F:Format.RGBA32F;
                                int bytesPerPixel = ((texImage.BitDepth==16)?8:16);
                                texInfo  = new TexInfo(dstFmt, width, height){
                                    Buf = new Byte[width * height * bytesPerPixel]
                                };
                                uint depth;
                                Buffer.BlockCopy(texImage.ToByteArray(out depth, TexImage.ConvOption.HalforFloat),
                                                 0, texInfo.Buf, 0, texInfo.Buf.Length);
                            }
                            uint saveOptions = 0;
                            if (pInfo.Plugin.TrySave(texInfo, path, saveOptions)){
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
