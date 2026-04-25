#include "pch.h"

#include <fstream>
#include <string>
#include <format>
#include <memory>
#include <DirectXTex.h>

#include "DirectXTexLoaderLib.h"
#include "ICCParser.h"
#include "dxgiformatStr.h"
#include <wrl/client.h>
#include <wincodec.h>

using namespace DirectX;
using namespace ICCParser;
using Microsoft::WRL::ComPtr;

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


std::string FourCCStr(DWORD fourCC){
    if (fourCC==0) return "Undefined";
    return std::format("'{:c}','{:c}','{:c}','{:c}'", fourCC&0xff, (fourCC>>8)&0xff, (fourCC>>16)&0xff, (fourCC>>24)&0xff);
}


extern "C" {
    DIRECTXTEXLOADERLIB_API const char *get_version()
    {
        static char buf[32]; //---------+---------+---------+--
        snprintf(buf, 32,     "2026.4.1.1 (%d)", DIRECTX_TEX_VERSION);
        return buf;
    }


    //------------------------------------------------------------------------------*
    // DDS
    DIRECTXTEXLOADERLIB_API int load_dds(const wchar_t *file, LOAD_RESULT *outResult)
    {
        DDS_HEADER ddsHeader={};
        DDS_HEADER_DXT10 dx10Header={};
        std::ifstream fileStream(file, std::ios::binary);
        if (fileStream) {
            fileStream.read(reinterpret_cast<char*>(&ddsHeader), sizeof(DDS_HEADER));
            if (ddsHeader.ddspf.dwFourCC == MAKEFOURCC('D','X','1','0')){
                fileStream.read(reinterpret_cast<char*>(&dx10Header), sizeof(DDS_HEADER_DXT10));
            }
            fileStream.close();
            //_RPT4(_CRT_WARN, "FourCC:'%c%c%c%c'\n", (ddsHeader.ddspf.dwFourCC)&0xff, (ddsHeader.ddspf.dwFourCC>>8)&0xff, (ddsHeader.ddspf.dwFourCC>>16)&0xff, (ddsHeader.ddspf.dwFourCC>>24)&0xff);
        }

        DWORD blackList[] = {
            MAKEFOURCC('A','2','D','5'), // ATI2N_DXT5 (v2024.10.29.1 bad image)
        };

        for (int i = 0; i < sizeof(blackList) / sizeof(DWORD); i++) {
            if (ddsHeader.ddspf.dwFourCC == blackList[i]) return ERR_UNSUPPORTED;
        }


        TexMetadata info;
        std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);
        DDS_FLAGS ddsFlags = DDS_FLAGS_NO_16BPP; // 5:6:5, 5:5:5:1, 4:4:4:4 to 8:8:8:8

        HRESULT hr = LoadFromDDSFile(file, ddsFlags, &info, *image);
        if (FAILED(hr)) return ERR_UNSUPPORTED;

        std::string loader_info="";
        loader_info += std::format("SubLoader: DDS\n");
        loader_info += std::format("width: {}\n", ddsHeader.dwWidth);
        loader_info += std::format("height: {}\n", ddsHeader.dwHeight);
        loader_info += std::format("depth: {}\n", ddsHeader.dwDepth);
        loader_info += std::format("MipMapCount: {}\n", ddsHeader.dwMipMapCount);
        loader_info += std::format("PixelFormat : {}\n", dxgiFormatStr((uint32_t)info.format));
        //loader_info += std::format("fourCC : {}\n", FourCCStr(ddsHeader.ddspf.dwFourCC));
        int flag = ddsHeader.ddspf.dwFlags;
        loader_info += std::format("PixelFormatFlags : 0x{:08x}\n", flag);
        if (flag & 0x01){ // DDPF_ALPHAPIXELS
            loader_info += std::format("  ABitMask : 0x{:08x}\n", ddsHeader.ddspf.dwABitMask);
        }
        if (flag & 0x02){ // DDPF_ALPHA
            loader_info += std::format("  RGBBitCount : {}\n", ddsHeader.ddspf.dwRGBBitCount);
        }
        if (flag & 0x04){ // DDPF_FOURCC
            loader_info += std::format("  fourCC : {}\n", FourCCStr(ddsHeader.ddspf.dwFourCC));
        }
        if (flag & 0x40){ // DDPF_RGB
            loader_info += std::format("  RBitMask : 0x{:08x}\n", ddsHeader.ddspf.dwRBitMask);
            loader_info += std::format("  GBitMask : 0x{:08x}\n", ddsHeader.ddspf.dwGBitMask);
            loader_info += std::format("  BBitMask : 0x{:08x}\n", ddsHeader.ddspf.dwBBitMask);
        }
        if (flag & 0x200){ // DDPF_YUV
            loader_info += std::format("  YUVBitCount : {}\n", ddsHeader.ddspf.dwRGBBitCount);
            loader_info += std::format("  YBitMask : 0x{:08x}\n", ddsHeader.ddspf.dwRBitMask);
            loader_info += std::format("  UBitMask : 0x{:08x}\n", ddsHeader.ddspf.dwGBitMask);
            loader_info += std::format("  VBitMask : 0x{:08x}\n", ddsHeader.ddspf.dwBBitMask);
        }
        if (flag & 0x20000){ // DDPF_LUMINANCE
            loader_info += std::format("  LUMINANCE BitCount : {}\n", ddsHeader.ddspf.dwRGBBitCount);
            loader_info += std::format("  LUMINANCE BitMask : 0x{:08x}\n", ddsHeader.ddspf.dwRBitMask);
        }

        outResult->flag = 0;
        if (ddsHeader.ddspf.dwFourCC == MAKEFOURCC('D','X','1','0')){
            loader_info += std::format(" DXT10 dxgiFormat: {}\n", dxgiFormatStr(dx10Header.dxgiFormat));
            loader_info += std::format(" DXT10 miscFlag: 0x{:08x}\n", dx10Header.miscFlag);
            const char * alphaMode[] = {
                "ALPHA_MODE_UNKNOWN",
                "ALPHA_MODE_STRAIGHT",
                "ALPHA_MODE_PREMULTIPLIED",
                "ALPHA_MODE_OPAQUE",
                "ALPHA_MODE_CUSTOM",
            };
            loader_info += std::format(" DXT10 miscFlags2: {}\n", alphaMode[dx10Header.miscFlags2]);
            if ((dx10Header.miscFlags2 & 0x07) == 0x02) outResult->flag |= 0x0001;

            outResult->colorspace = IsSRGB((DXGI_FORMAT)dx10Header.dxgiFormat)?CS_SRGB:CS_LINEAR;
        }else{
            outResult->colorspace = (isSRGB(info.format))?CS_SRGB:CS_LINEAR;
        }

        size_t page_size = info.width*info.height*4;
        size_t dst_size = page_size * info.depth * info.arraySize;
        unsigned char *dst = (unsigned char *)malloc(dst_size);
        if (dst == 0) return ERR_NOMEM;

        for (int j=0; j<info.depth; j++){
            for (int i=0; i<info.arraySize; i++){
                auto img = image->GetImage(0, i, j); // mip, item. slice
                ScratchImage image2;
                hr = Decompress(*img, DXGI_FORMAT_R8G8B8A8_UNORM, image2);
                if (FAILED(hr)){
                    free(dst);
                    return ERR_UNSUPPORTED;
                }
                auto img2 = image2.GetImage(0, 0, 0);
                memcpy(dst+page_size*i, img2->pixels, page_size);
            }
        }
        outResult->dst = (unsigned int *)dst;
        outResult->dst_len = (int32_t)dst_size;

        outResult->width = (int32_t)info.width;
        outResult->height = (int32_t)info.height;
        outResult->numFrames = (int32_t)(info.depth * info.arraySize);
        outResult->format = FMT_RGBA8888;

        std::string format_info = dxgiFormatStr((uint32_t)info.format);
        const std::string::size_type fsize = format_info.size();
        outResult->format_string = (uint32_t *)new char[fsize + 1];
        memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

        const std::string::size_type lsize = loader_info.size();
        outResult->loader_string = (uint32_t *)new char[lsize + 1];
        memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);

        return ERR_OK;
    }

/*
    int parseMetadata(ComPtr<IWICMetadataQueryReader> metareader){
        HRESULT hr;

        ComPtr<IEnumString> enumStrings;
        if (SUCCEEDED(metareader->GetEnumerator(&enumStrings))) {
            LPOLESTR name = nullptr;
            ULONG fetched = 0;
            while (enumStrings->Next(1, &name, &fetched) == S_OK) {
                _RPTFWN(_CRT_WARN, L"Key: %s\n", name);
                CoTaskMemFree(name);
            }
        }


        PROPVARIANT value;
        PropVariantInit(&value);

        hr = metareader->GetMetadataByName(L"/ifd/{ushort=34675}", &value);
        if (SUCCEEDED(hr) && value.vt == (VT_VECTOR | VT_UI1)) {
            const BYTE* iccData = value.caub.pElems;
            size_t iccSize = value.caub.cElems;
        }

        hr = metareader->GetMetadataByName(L"/app2/icc", &value);
        if (SUCCEEDED(hr) && (value.vt == (VT_VECTOR | VT_UI1))) {
            const BYTE* iccData = value.caub.pElems;
            size_t iccSize = value.caub.cElems;
        }

        hr = metareader->GetMetadataByName(L"/app2/icc/profile", &value);
        if (SUCCEEDED(hr) && (value.vt == (VT_VECTOR | VT_UI1))) {
            const BYTE* iccData = value.caub.pElems;
            size_t iccSize = value.caub.cElems;
        }


        hr = metareader->GetMetadataByName(L"/app1/ifd/exif/{ushort=40961}", &value);
        if (SUCCEEDED(hr) && value.vt == VT_UI2) {
            USHORT cs = value.uiVal;
            switch (cs) {
              case 1:  // sRGB
                // sRGB 色域
                break;
              case 2:  // Adobe RGB (Exif 2.21以降)
                // AdobeRGB
                break;
              case 0xFFFF:
                // Uncalibrated
                break;
            }
        }

        PropVariantClear(&value);


        return 0;
    }
*/

    //------------------------------------------------------------------------------*
    int load_sub(TexMetadata &info, std::unique_ptr<ScratchImage> &image, LOAD_RESULT *outResult, std::string loader, IccParser *iccParser=0) {
        int fmt = FMT_UNKNOWN;
        switch(info.format){
          case 2: // R32G32B32A32_FLOAT
            fmt = FMT_RGBA32F;
            break;
          case 6: // R32G32B32_FLOAT
            fmt = FMT_RGB32F;
            break;
          case 10: // R16G16B16A16_FLOAT
            fmt = FMT_RGBA16F;
            break;
          case 12: // R16G16B16A16_UINT
            fmt = FMT_RGBA16;
            break;
          case 27: // R8G8B8A8_TYPELESS
          case 28: // R8G8B8A8_UNORM
          case 29: // R8G8B8A8_UNORM_SRGB
          case 30: // R8G8B8A8_UINT
            fmt = FMT_RGBA8888;
            break;
          case 87: // B8G8R8A8_UNORM
          case 90: // B8G8R8A8_TYPELESS
          case 91: // B8G8R8A8_UNORM_SRGB
          case 88: // B8G8R8X8_UNORM
          case 92: // B8G8R8X8_TYPELESS
          case 93: // B8G8R8X8_UNORM_SRGB
            fmt = FMT_BGRA8888;
            break;
          case 33: // R16G16_TYPELESS
          case 35: // R16G16_UNORM
          case 36: // R16G16_UINT
            fmt = FMT_RG16;
            break;
          case 53: // R16_TYPELESS
          case 56: // R16_UNORM
          case 57: // R16_UINT
            fmt = FMT_R16;
            break;
          case 60: // R8_TYPELESS
          case 61: // R8_UNORM
          case 62: // R8_UINT
            fmt = FMT_R8;
            break;
          case 65: // A8_UNORM
            fmt = FMT_A8;
            break;
          default:
            break;
        }
        if (fmt == FMT_UNKNOWN) return ERR_UNSUPPORTED;

        std::string loader_info="";
        loader_info += std::format("SubLoader: {}\n", loader);
        loader_info += std::format("width: {}\n", info.width);
        loader_info += std::format("height: {}\n", info.height);
        loader_info += std::format("format : {}\n", dxgiFormatStr(info.format));
        loader_info += std::format("mipLevels : {}\n", info.mipLevels);
        loader_info += std::format("miscFlags : 0x{:08x}\n", info.miscFlags);
        loader_info += std::format("miscFlags2 : 0x{:08x}\n", info.miscFlags2);

        int numImages = (int)info.arraySize; //image->GetImageCount();
        size_t dst_size = image->GetPixelsSize();
        size_t page_size = dst_size / numImages;
        unsigned char *dst = (unsigned char *)malloc(dst_size);
        if (dst == 0) return ERR_NOMEM;

        for (int i=0; i<numImages; i++){
            auto img = image->GetImage(0, i, 0);
            memcpy(dst+page_size*i, img->pixels, page_size);
        }

        outResult->format = fmt;
        outResult->dst = (unsigned int *)dst;
        outResult->dst_len = (int32_t)dst_size;

        outResult->width = (int32_t)info.width;
        outResult->height = (int32_t)info.height;
        outResult->numFrames = numImages;
        int cs, tf;
        if (iccParser){
            cs = iccParser->getColorSpace();
            tf = iccParser->getTransferFunction();
            outResult->gamma = (float)iccParser->getGamma();
            loader_info += iccParser->dumpIccInfo();
        }else{
            cs = isSRGB(info.format)?CS_SRGB:CS_UNKNOWN;
            tf = isSRGB(info.format)?TF_SRGB:TF_LINEAR;
        }
        outResult->colorspace = cs | tf;
        outResult->flag |= info.IsPMAlpha()?0x0001:0x0000;


        std::string format_info = dxgiFormatStr(info.format);
        const std::string::size_type fsize = format_info.size();
        outResult->format_string = (uint32_t *)new char[fsize + 1];
        memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

        const std::string::size_type lsize = loader_info.size();
        outResult->loader_string = (uint32_t *)new char[lsize + 1];
        memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);

        return ERR_OK;
    }

    //------------------------------------------------------------------------------*
    DIRECTXTEXLOADERLIB_API int load_hdr(const wchar_t *file, LOAD_RESULT *outResult)
    {
        TexMetadata info;
        std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);

        HRESULT hr = LoadFromHDRFile(file, &info, *image);
        if (FAILED(hr)) return ERR_UNSUPPORTED;

        return load_sub(info, image, outResult, "HDR");
    }

    //------------------------------------------------------------------------------*
    DIRECTXTEXLOADERLIB_API int load_tga(const wchar_t *file, LOAD_RESULT *outResult)
    {
        TexMetadata info;
        std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);

        TGA_FLAGS flags = TGA_FLAGS_DEFAULT_SRGB;
        HRESULT hr = LoadFromTGAFile(file, flags, &info, *image);
        if (FAILED(hr)) return ERR_UNSUPPORTED;

        return load_sub(info, image, outResult, "TGA");
    }

    //------------------------------------------------------------------------------*
    DIRECTXTEXLOADERLIB_API int load_wic(const wchar_t *file, LOAD_RESULT *outResult)
    {
        TexMetadata info;
        std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);
        WIC_FLAGS flags = WIC_FLAGS_ALL_FRAMES | WIC_FLAGS_DEFAULT_SRGB;

        HRESULT hr = LoadFromWICFile(file, flags, &info, *image);
        if (FAILED(hr)) return ERR_UNSUPPORTED;

        int cs = CS_UNKNOWN;
        IccParser *iccParser = new IccParser();
        if (iccParser->parseIcc(file) == 1) {
            return load_sub(info, image, outResult, "WIC", iccParser);
        }
        return load_sub(info, image, outResult, "WIC");
    }

    //------------------------------------------------------------------------------*
    DIRECTXTEXLOADERLIB_API void free_mem(LOAD_RESULT *result){
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }

}
