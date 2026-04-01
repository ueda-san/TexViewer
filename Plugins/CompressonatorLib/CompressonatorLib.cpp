#include "pch.h"

#include <windows.h>
#include <stringapiset.h>

#include "DDS_Helpers.h"
#include "CompressonatorLib.h"

#include <string>
#include <format>


std::string formatStr(CMP_FORMAT fmt){
    switch(fmt) {
      case CMP_FORMAT_Unknown: return "Unknown";
      case CMP_FORMAT_RGBA_8888_S: return "RGBA_8888_S";
      case CMP_FORMAT_ARGB_8888_S: return "ARGB_8888_S";
      case CMP_FORMAT_ARGB_8888: return "ARGB_8888";
      case CMP_FORMAT_ABGR_8888: return "ABGR_8888";
      case CMP_FORMAT_RGBA_8888: return "RGBA_8888";
      case CMP_FORMAT_BGRA_8888: return "BGRA_8888";
      case CMP_FORMAT_RGB_888: return "RGB_888";
      case CMP_FORMAT_RGB_888_S: return "RGB_888_S";
      case CMP_FORMAT_BGR_888: return "BGR_888";
      case CMP_FORMAT_RG_8_S: return "RG_8_S";
      case CMP_FORMAT_RG_8: return "RG_8";
      case CMP_FORMAT_R_8_S: return "R_8_S";
      case CMP_FORMAT_R_8: return "R_8";
      case CMP_FORMAT_ARGB_2101010: return "ARGB_2101010";
      case CMP_FORMAT_RGBA_1010102: return "RGBA_1010102";
      case CMP_FORMAT_ARGB_16: return "ARGB_16";
      case CMP_FORMAT_ABGR_16: return "ABGR_16";
      case CMP_FORMAT_RGBA_16: return "RGBA_16";
      case CMP_FORMAT_BGRA_16: return "BGRA_16";
      case CMP_FORMAT_RG_16: return "RG_16";
      case CMP_FORMAT_R_16: return "R_16";
      case CMP_FORMAT_RGBE_32F: return "RGBE_32F";
      case CMP_FORMAT_ARGB_16F: return "ARGB_16F";
      case CMP_FORMAT_ABGR_16F: return "ABGR_16F";
      case CMP_FORMAT_RGBA_16F: return "RGBA_16F";
      case CMP_FORMAT_BGRA_16F: return "BGRA_16F";
      case CMP_FORMAT_RG_16F: return "RG_16F";
      case CMP_FORMAT_R_16F: return "R_16F";
      case CMP_FORMAT_ARGB_32F: return "ARGB_32F";
      case CMP_FORMAT_ABGR_32F: return "ABGR_32F";
      case CMP_FORMAT_RGBA_32F: return "RGBA_32F";
      case CMP_FORMAT_BGRA_32F: return "BGRA_32F";
      case CMP_FORMAT_RGB_32F: return "RGB_32F";
      case CMP_FORMAT_BGR_32F: return "BGR_32F";
      case CMP_FORMAT_RG_32F: return "RG_32F";
      case CMP_FORMAT_R_32F: return "R_32F";
      case CMP_FORMAT_BROTLIG: return "BROTLIG";
      case CMP_FORMAT_BC1: return "BC1";
      case CMP_FORMAT_BC2: return "BC2";
      case CMP_FORMAT_BC3: return "BC3";
      case CMP_FORMAT_BC4: return "BC4";
      case CMP_FORMAT_BC4_S: return "BC4_S";
      case CMP_FORMAT_BC5: return "BC5";
      case CMP_FORMAT_BC5_S: return "BC5_S";
      case CMP_FORMAT_BC6H: return "BC6H";
      case CMP_FORMAT_BC6H_SF: return "BC6H_SF";
      case CMP_FORMAT_BC7: return "BC7";
      case CMP_FORMAT_ATI1N: return "ATI1N";
      case CMP_FORMAT_ATI2N: return "ATI2N";
      case CMP_FORMAT_ATI2N_XY: return "ATI2N_XY";
      case CMP_FORMAT_ATI2N_DXT5: return "ATI2N_DXT5";
      case CMP_FORMAT_DXT1: return "DXT1";
      case CMP_FORMAT_DXT3: return "DXT3";
      case CMP_FORMAT_DXT5: return "DXT5";
      case CMP_FORMAT_DXT5_xGBR: return "DXT5_xGBR";
      case CMP_FORMAT_DXT5_RxBG: return "DXT5_RxBG";
      case CMP_FORMAT_DXT5_RBxG: return "DXT5_RBxG";
      case CMP_FORMAT_DXT5_xRBG: return "DXT5_xRBG";
      case CMP_FORMAT_DXT5_RGxB: return "DXT5_RGxB";
      case CMP_FORMAT_DXT5_xGxR: return "DXT5_xGxR";
      case CMP_FORMAT_ATC_RGB: return "ATC_RGB";
      case CMP_FORMAT_ATC_RGBA_Explicit: return "ATC_RGBA_Explicit";
      case CMP_FORMAT_ATC_RGBA_Interpolated: return "ATC_RGBA_Interpolated";
      case CMP_FORMAT_ASTC: return "ASTC";
      case CMP_FORMAT_APC: return "APC";
      case CMP_FORMAT_PVRTC: return "PVRTC";
      case CMP_FORMAT_ETC_RGB: return "ETC_RGB";
      case CMP_FORMAT_ETC2_RGB: return "ETC2_RGB";
      case CMP_FORMAT_ETC2_SRGB: return "ETC2_SRGB";
      case CMP_FORMAT_ETC2_RGBA: return "ETC2_RGBA";
      case CMP_FORMAT_ETC2_RGBA1: return "ETC2_RGBA1";
      case CMP_FORMAT_ETC2_SRGBA: return "ETC2_SRGBA";
      case CMP_FORMAT_ETC2_SRGBA1: return "ETC2_SRGBA1";
      case CMP_FORMAT_BINARY: return "BINARY";
      case CMP_FORMAT_GTC: return "GTC";
      case CMP_FORMAT_BASIS: return "BASIS";
    }
    return "Unknown";
}
std::string ChannelFormatStr(CMP_ChannelFormat fmt){
    switch(fmt) {
      case CF_8bit: return "8bit";
      case CF_Float16: return "Float16";
      case CF_Float32: return "Float32";
      case CF_Compressed: return "Compressed";
      case CF_16bit: return "16bit";
      case CF_2101010: return "2101010";
      case CF_32bit: return "32bit";
      case CF_Float9995E: return "Float9995E";
      case CF_YUV_420: return "YUV_420";
      case CF_YUV_422: return "YUV_422";
      case CF_YUV_444: return "YUV_444";
      case CF_YUV_4444: return "YUV_4444";
      case CF_1010102: return "1010102";
    }
    return "Unknown";
}
std::string TextureDataTypeStr(CMP_TextureDataType typ){
    switch(typ) {
      case TDT_XRGB: return "XRGB";
      case TDT_ARGB: return "ARGB";
      case TDT_NORMAL_MAP: return "NORMAL_MAP";
      case TDT_R: return "R";
      case TDT_RG: return "RG";
      case TDT_YUV_SD: return "YUV_SD";
      case TDT_YUV_HD: return "YUV_HD";
      case TDT_RGB: return "RGB";
      case TDT_8: return "8";
      case TDT_16: return "16";
    }
    return "Unknown";
}
std::string TextureTypeStr(CMP_TextureType typ){
    switch(typ) {
      case TT_2D: return "2D";
      case TT_CubeMap: return "CubeMap";
      case TT_VolumeTexture: return "VolumeTexture";
      case TT_2D_Block: return "2D_Block";
      case TT_1D: return "1D";
      case TT_Unknown: return "Unknown";
    }
    return "Unknown";
}

std::string FourCCStr(CMP_DWORD fourCC){
    if (fourCC==0) return "Undefined";
    return std::format("'{:c}','{:c}','{:c}','{:c}'", fourCC&0xff, (fourCC>>8)&0xff, (fourCC>>16)&0xff, (fourCC>>24)&0xff);
}


extern "C" {
    //------------------------------------------------------------------------------
    COMPRESSONATOR_API const char *get_version()
    {
        static char buf[32]; //---------+---------+---------+--
        snprintf(buf, 32,     "4.5.52 (%d.%d)",
                 AMD_COMPRESS_VERSION_MAJOR,AMD_COMPRESS_VERSION_MINOR);
        return buf;
    }

    //------------------------------------------------------------------------------

    COMPRESSONATOR_API int load_file(const wchar_t *file, LOAD_RESULT *outResult)
    {
        CMP_Texture srcTexture;
        CMP_DWORD fourCC;
        bool ret = LoadDDSFile(file, srcTexture, fourCC);

        if (ret){
            std::string loader_info="";
            CMP_Texture destTexture;
            destTexture.dwSize     = sizeof(destTexture);
            destTexture.dwWidth    = srcTexture.dwWidth;
            destTexture.dwHeight   = srcTexture.dwHeight;
            destTexture.dwPitch    = 0;
            destTexture.format     = CMP_FORMAT_RGBA_8888;
            destTexture.dwDataSize = CMP_CalculateBufferSize(&destTexture);
            destTexture.pData      = (CMP_BYTE*)malloc(destTexture.dwDataSize);
            if (destTexture.pData==0) return ERR_NOMEM;
            CMP_CompressOptions options = {0};
            options.dwSize       = sizeof(options);
            options.dwnumThreads = 8;

            loader_info += std::format("width: {}\n", srcTexture.dwWidth);
            loader_info += std::format("height: {}\n", srcTexture.dwHeight);
            loader_info += std::format("format: {}\n", formatStr(srcTexture.format));
            loader_info += std::format("fourCC: {}\n", FourCCStr(fourCC));
            if (srcTexture.format == CMP_FORMAT_BASIS){
                loader_info += std::format("transcodeFormat: {}\n", formatStr(srcTexture.transcodeFormat));
            }
            if (CMP_ConvertTexture(&srcTexture, &destTexture, &options, NULL) == CMP_OK){
                int dst_size = destTexture.dwWidth*destTexture.dwHeight*4;
                unsigned char *dst = (unsigned char *)malloc(dst_size);
                if (dst==0) return ERR_NOMEM;
                memcpy(dst, destTexture.pData, dst_size);

                outResult->dst = (unsigned int *)dst;
                outResult->dst_len = dst_size;

                outResult->width = destTexture.dwWidth;
                outResult->height = destTexture.dwHeight;
                outResult->numFrames = 1; //FIXME
                outResult->flag = 0;
                outResult->format = FMT_RGBA8888;
                outResult->colorspace = CS_UNKNOWN; //FIXME

                std::string format_info = formatStr(srcTexture.format);
                const std::string::size_type fsize = format_info.size();
                outResult->format_string = (uint32_t *)new char[fsize + 1];
                memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

                const std::string::size_type lsize = loader_info.size();
                outResult->loader_string = (uint32_t *)new char[lsize + 1];
                memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);

                free(srcTexture.pData);
                free(destTexture.pData);
                return ERR_OK;
            }
            free(srcTexture.pData);
            free(destTexture.pData);
            return ERR_UNSUPPORTED;
        }
        return ERR_UNSUPPORTED;
    }

    //------------------------------------------------------------------------------
    COMPRESSONATOR_API void free_mem(LOAD_RESULT *result){
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }

}
