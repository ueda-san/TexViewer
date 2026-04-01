#include "pch.h"

#include <windows.h>
#include <stringapiset.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <format>

#include "PVRTexLib.h"
#include "PVRTexLibDefines.h"
#include "PVRTextureVersion.h"
#include "PVRLoaderLib.h"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                  \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif

struct DDS_PIXELFORMAT {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwFourCC;
  DWORD dwRGBBitCount;
  DWORD dwRBitMask;
  DWORD dwGBitMask;
  DWORD dwBBitMask;
  DWORD dwABitMask;
};

typedef struct {
  DWORD dwMagic;
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwHeight;
  DWORD dwWidth;
  DWORD dwPitchOrLinearSize;
  DWORD dwDepth;
  DWORD dwMipMapCount;
  DWORD dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  DWORD dwCaps;
  DWORD dwCaps2;
  DWORD dwCaps3;
  DWORD dwCaps4;
  DWORD dwReserved2;
} DDS_HEADER;

typedef struct {
  UINT dxgiFormat;
  UINT resourceDimension;
  UINT miscFlag;
  UINT arraySize;
  UINT miscFlags2;
} DDS_HEADER_DXT10;


const char* pvrFormat[] = {
    "PVRTCI_2bpp_RGB",
    "PVRTCI_2bpp_RGBA",
    "PVRTCI_4bpp_RGB",
    "PVRTCI_4bpp_RGBA",
    "PVRTCII_2bpp",
    "PVRTCII_4bpp",
    "ETC1",
    "DXT1",
    "DXT2",
    "DXT3",
    "DXT4",
    "DXT5",
    "BC4",
    "BC5",
    "BC6",
    "BC7",
    "UYVY_422",
    "YUY2_422",
    "BW1bpp",
    "SharedExponentR9G9B9E5",
    "RGBG8888",
    "GRGB8888",
    "ETC2_RGB",
    "ETC2_RGBA",
    "ETC2_RGB_A1",
    "EAC_R11",
    "EAC_RG11",
    "ASTC_4x4",
    "ASTC_5x4",
    "ASTC_5x5",
    "ASTC_6x5",
    "ASTC_6x6",
    "ASTC_8x5",
    "ASTC_8x6",
    "ASTC_8x8",
    "ASTC_10x5",
    "ASTC_10x6",
    "ASTC_10x8",
    "ASTC_10x10",
    "ASTC_12x10",
    "ASTC_12x12",
    "ASTC_3x3x3",
    "ASTC_4x3x3",
    "ASTC_4x4x3",
    "ASTC_4x4x4",
    "ASTC_5x4x4",
    "ASTC_5x5x4",
    "ASTC_5x5x5",
    "ASTC_6x5x5",
    "ASTC_6x6x5",
    "ASTC_6x6x6",
    "BASISU_ETC1S",
    "BASISU_UASTC",
    "RGBM",
    "RGBD",
    "PVRTCI_HDR_6bpp",
    "PVRTCI_HDR_8bpp",
    "PVRTCII_HDR_6bpp",
    "PVRTCII_HDR_8bpp",
    "VYUA10MSB_444",
    "VYUA10LSB_444",
    "VYUA12MSB_444",
    "VYUA12LSB_444",
    "UYV10A2_444",
    "UYVA16_444",
    "YUYV16_422",
    "UYVY16_422",
    "YUYV10MSB_422",
    "YUYV10LSB_422",
    "UYVY10MSB_422",
    "UYVY10LSB_422",
    "YUYV12MSB_422",
    "YUYV12LSB_422",
    "UYVY12MSB_422",
    "UYVY12LSB_422",
};

const char *pvrFormat270[] = {
    "YUV_3P_444",
    "YUV10MSB_3P_444",
    "YUV10LSB_3P_444",
    "YUV12MSB_3P_444",
    "YUV12LSB_3P_444",
    "YUV16_3P_444",
    "YUV_3P_422",
    "YUV10MSB_3P_422",
    "YUV10LSB_3P_422",
    "YUV12MSB_3P_422",
    "YUV12LSB_3P_422",
    "YUV16_3P_422",
    "YUV_3P_420",
    "YUV10MSB_3P_420",
    "YUV10LSB_3P_420",
    "YUV12MSB_3P_420",
    "YUV12LSB_3P_420",
    "YUV16_3P_420",
    "YVU_3P_420",
    "YUV_2P_422 = 480",
    "YUV10MSB_2P_422",
    "YUV10LSB_2P_422",
    "YUV12MSB_2P_422",
    "YUV12LSB_2P_422",
    "YUV16_2P_422",
    "YUV_2P_420",
    "YUV10MSB_2P_420",
    "YUV10LSB_2P_420",
    "YUV12MSB_2P_420",
    "YUV12LSB_2P_420",
    "YUV16_2P_420",
    "YUV_2P_444",
    "YVU_2P_444",
    "YUV10MSB_2P_444",
    "YUV10LSB_2P_444",
    "YVU10MSB_2P_444",
    "YVU10LSB_2P_444",
    "YVU_2P_422",
    "YVU10MSB_2P_422",
    "YVU10LSB_2P_422",
    "YVU_2P_420",
    "YVU10MSB_2P_420",
    "YVU10LSB_2P_420",
};

static const char *GetFormatStr(uint64_t fmt, std::string path) {
    if (fmt >=270){
        fmt -= 270;
        if (fmt < sizeof(pvrFormat270)/sizeof(const char*)){
            return pvrFormat270[fmt];
        }
    }else{
        if (fmt < sizeof(pvrFormat)/sizeof(const char*)){
            return pvrFormat[fmt];
        }
    }
    if (path.ends_with(".exr")) return ".exr";
    if (path.ends_with(".hdr")) return ".hdr";

    return "unknown";
}

const char *colourSpace[] = {
    "Linear",
    "sRGB",
    "BT601",
    "BT709",
    "BT2020",
};
const char *channelType[] = {
    "UnsignedByteNorm",
    "SignedByteNorm",
    "UnsignedByte",
    "SignedByte",
    "UnsignedShortNorm",
    "SignedShortNorm",
    "UnsignedShort",
    "SignedShort",
    "UnsignedIntegerNorm",
    "SignedIntegerNorm",
    "UnsignedInteger",
    "SignedInteger",
    "SignedFloat",
    "UnsignedFloat",
};


extern "C" {
    //------------------------------------------------------------------------------
    PVRLOADERLIB_API const char *get_version()
    {
        static char buf[32]; //---------+---------+---------+--
        snprintf(buf, 32,     "%s", PVRTEXLIB_STRINGVERSION); // "5.6.0"
        return buf;
    }

    //------------------------------------------------------------------------------
    PVRLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult)
    {
        int requiredSize = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
        if (requiredSize == 0) return ERR_UNKNOWN;
        char* path = new char[requiredSize];
        int convResult = WideCharToMultiByte(CP_UTF8, 0, file, -1, path, requiredSize, nullptr, nullptr);
        if (convResult == 0){
            delete[] path;
            return ERR_NOTFOUND;
        }

        std::string loader_info="";
        std::string pathStr = std::string(path);
        if (pathStr.ends_with(".dds")) {
            DDS_HEADER ddsHeader={};
            DDS_HEADER_DXT10 dx10Header={};
            std::ifstream fileStream(file, std::ios::binary);
            if (fileStream) {
                fileStream.read(reinterpret_cast<char*>(&ddsHeader), sizeof(DDS_HEADER));
                _RPT4(_CRT_WARN, "FourCC:'%c%c%c%c'\n", (ddsHeader.ddspf.dwFourCC)&0xff, (ddsHeader.ddspf.dwFourCC>>8)&0xff, (ddsHeader.ddspf.dwFourCC>>16)&0xff, (ddsHeader.ddspf.dwFourCC>>24)&0xff);
                if (ddsHeader.ddspf.dwFourCC == MAKEFOURCC('D','X','1','0')){
                    fileStream.read(reinterpret_cast<char*>(&dx10Header), sizeof(DDS_HEADER_DXT10));
                    _RPT1(_CRT_WARN, " DXGI:%d\n", dx10Header.dxgiFormat);
                }
                fileStream.close();
            }

            // PVRTexTool GUI v5.6.0 でも読めなかったり表示がおかしい物を除外
            if (ddsHeader.ddspf.dwFourCC == MAKEFOURCC('D','X','1','0')){
                UINT blackList[] = {
                    72, // DXGI_FORMAT_BC1_UNORM_SRGB
                    75, // DXGI_FORMAT_BC2_UNORM_SRGB
                    78, // DXGI_FORMAT_BC3_UNORM_SRGB
                    95, // DXGI_FORMAT_BC6H_UF16
                    96, // DXGI_FORMAT_BC6H_SF16
                    98, // DXGI_FORMAT_BC7_UNORM
                    99, // DXGI_FORMAT_BC7_UNORM_SRGB
                };
                for (int i = 0; i < sizeof(blackList) / sizeof(UINT); i++) {
                    if (dx10Header.dxgiFormat == blackList[i]) return ERR_UNSUPPORTED;
                }
            }else{
                DWORD blackList[] = {
                    MAKEFOURCC('A','T','C',' '), // ATC_RGB
                    MAKEFOURCC('A','T','C','A'), // ATC_RGBA_Explicit
                    MAKEFOURCC('A','T','C','I'), // ATC_RGBA_Interpolated
                    MAKEFOURCC('A','T','I','1'), // ATI1N
                    MAKEFOURCC('A','T','I','2'), // ATI2N
                    MAKEFOURCC('A','2','D','5'), // ATI2N_DXT5
                    MAKEFOURCC('B','C','4','S'), // BC4_SNORM
                    MAKEFOURCC('B','C','4','U'), // BC4_UNORM
                    MAKEFOURCC('B','C','5','S'), // BC5_SNORM
                    MAKEFOURCC('B','C','5','U'), // BC5_UNORM
                };
                for (int i = 0; i < sizeof(blackList) / sizeof(DWORD); i++) {
                    if (ddsHeader.ddspf.dwFourCC == blackList[i]) return ERR_UNSUPPORTED;
                }
            }
        }

        PVRTexLib_PVRTexture tex = PVRTexLib_CreateTextureFromFile(path);
        delete[] path;
        if (tex == NULL) return ERR_UNSUPPORTED;

        PVRTexLib_PVRTextureHeader hdr = PVRTexLib_GetTextureHeaderW(tex);
        PVRTuint32 width = PVRTexLib_GetTextureWidth(hdr, 0);
        PVRTuint32 height = PVRTexLib_GetTextureHeight(hdr, 0);
        PVRTuint32 depth = PVRTexLib_GetTextureDepth(hdr, 0);
        PVRTuint64 format = PVRTexLib_GetTexturePixelFormat(hdr);
        PVRTexLibColourSpace cs = PVRTexLib_GetTextureColourSpace(hdr);

        loader_info += std::format("width: {}\n", width);
        loader_info += std::format("height: {}\n", height);
        if (depth >= 2){
            loader_info += std::format("depth: {}\n", depth);
        }
        loader_info += std::format("format: {}\n", GetFormatStr(format, pathStr));
        loader_info += std::format("color space: {}\n", colourSpace[cs]);
        loader_info += std::format("BitsPerPixel: {}\n", PVRTexLib_GetTextureBitsPerPixel(hdr));
        loader_info += std::format("ChannelCount: {}\n", PVRTexLib_GetTextureChannelCount(hdr));
        loader_info += std::format("ChannelType: {}\n", channelType[PVRTexLib_GetTextureChannelType(hdr)]);
        loader_info += std::format("IsPreMultiplied: {}\n", PVRTexLib_GetTextureIsPreMultiplied(hdr));
        loader_info += std::format("IsFileCompressed: {}\n", PVRTexLib_GetTextureIsFileCompressed(hdr));
        loader_info += std::format("IsBumpMap: {}\n", PVRTexLib_GetTextureIsBumpMap(hdr));
        loader_info += std::format("MipMapLevels: {}\n", PVRTexLib_GetTextureNumMipMapLevels(hdr));
        loader_info += std::format("NumFaces: {}\n", PVRTexLib_GetTextureNumFaces(hdr));

        PVRTexLib_TranscoderOptions opt{};
        opt.sizeofStruct = sizeof(PVRTexLib_TranscoderOptions);
        int bytePerPixel;
        if (PVRTexLib_GetTextureChannelType(hdr) < PVRTLVT_UnsignedShortNorm){
            opt.pixelFormat = PVRTGENPIXELID4('r', 'g', 'b', 'a', 8, 8, 8, 8);
            opt.channelType[0]=PVRTLVT_UnsignedByte;
            opt.channelType[1]=PVRTLVT_UnsignedByte;
            opt.channelType[2]=PVRTLVT_UnsignedByte;
            opt.channelType[3]=PVRTLVT_UnsignedByte;
            outResult->format = FMT_RGBA8888;
            bytePerPixel = 4;
        }else if (PVRTexLib_GetTextureChannelType(hdr) < PVRTLVT_UnsignedIntegerNorm){
            opt.pixelFormat = PVRTGENPIXELID4('r', 'g', 'b', 'a', 16, 16, 16, 16);
            opt.channelType[0]=PVRTLVT_UnsignedShort;
            opt.channelType[1]=PVRTLVT_UnsignedShort;
            opt.channelType[2]=PVRTLVT_UnsignedShort;
            opt.channelType[3]=PVRTLVT_UnsignedShort;
            outResult->format = FMT_RGBA16;
            bytePerPixel = 8;
        }else{
            opt.pixelFormat = PVRTGENPIXELID4('r', 'g', 'b', 'a', 32, 32, 32, 32);
            opt.channelType[0]=PVRTLVT_Float;
            opt.channelType[1]=PVRTLVT_Float;
            opt.channelType[2]=PVRTLVT_Float;
            opt.channelType[3]=PVRTLVT_Float;
            outResult->format = FMT_RGBA32F;
            bytePerPixel = 16;
        }
        opt.colourspace = cs; //PVRTLCS_sRGB;
        opt.quality = PVRTLCQ_PVRTCNormal;
        opt.doDither = false;
        opt.maxRange = 0.0f;
        opt.maxThreads = 0;

        outResult->flag = 0;
        if (PVRTexLib_GetTextureIsPreMultiplied(hdr)) outResult->flag |= 0001;

        if (PVRTexLib_TranscodeTexture(tex, opt)){

            int metaSize = PVRTexLib_GetTextureMetaDataSize(hdr);
            _RPT1(_CRT_WARN, "metaSize:%d\n", metaSize);
            PVRTexLib_Orientation orientation;
            PVRTexLib_GetTextureOrientation(hdr, &orientation);
            _RPT1(_CRT_WARN, "Orientation X:%d\n", orientation.x);
            _RPT1(_CRT_WARN, "Orientation Y:%d\n", orientation.y);
            _RPT1(_CRT_WARN, "Orientation Z:%d\n", orientation.z);
            if (orientation.x == PVRTLO_Left) PVRTexLib_FlipTexture(tex, PVRTLA_X);
            if (orientation.y == PVRTLO_Up) PVRTexLib_FlipTexture(tex, PVRTLA_Y);
            if (orientation.z == PVRTLO_Out) PVRTexLib_FlipTexture(tex, PVRTLA_Z);

            int numFaces = PVRTexLib_GetTextureNumFaces(hdr);
            int pageSize = width * height * bytePerPixel;
            int dst_size = pageSize * depth * numFaces;
            unsigned char *dst = (unsigned char *)malloc(dst_size);
            if (dst == 0) {
                PVRTexLib_DestroyTexture(tex);
                return ERR_NOMEM;
            }
            unsigned char *p = dst;
            for (uint32_t z=0; z<depth; z++){
                for (int i=0; i<numFaces; i++){
                    const char* data = (const char *)PVRTexLib_GetTextureDataConstPtr(tex, 0, 0, i, z);
                    memcpy(p, data, pageSize);
                    p += pageSize;
                }
            }

            outResult->dst = (unsigned int *)dst;
            outResult->dst_len = dst_size;

            outResult->width = width;
            outResult->height = height;
            outResult->numFrames = depth * numFaces;
            outResult->colorspace = cs+1;

            std::string format_info = GetFormatStr(format, pathStr);
            const std::string::size_type fsize = format_info.size();
            outResult->format_string = (uint32_t *)new char[fsize + 1];
            memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

            const std::string::size_type lsize = loader_info.size();
            outResult->loader_string = (uint32_t *)new char[lsize + 1];
            memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);

            PVRTexLib_DestroyTexture(tex);
            return ERR_OK;
        }
        PVRTexLib_DestroyTexture(tex);
        return ERR_UNSUPPORTED;
    }

    //------------------------------------------------------------------------------
    PVRLOADERLIB_API void free_mem(LOAD_RESULT *result) {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }


}
