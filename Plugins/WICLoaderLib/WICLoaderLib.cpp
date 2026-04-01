#include <cassert>
#include <comdef.h>
#include <format>
#include <string>
#include <unordered_map>
#include <vector>

#include <limits.h>
#include <malloc.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "WICLoaderLib.h"
#include "ICCParser.h"
#include "EXIFParser.h"
#include "formatDesc.h"


using namespace Microsoft::WRL;
using namespace ICCParser;
using namespace EXIFParser;

static IWICImagingFactory* GetWICFactory() {
    static IWICImagingFactory* factory = nullptr;
    if (!factory) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    }
    return factory;
}

typedef struct {
    const GUID guid;
    const char* name;
    const int format;
    const int pma;
} FormatInfo;

FormatInfo formatInfo[] = {
    // https://learn.microsoft.com/ja-jp/windows/win32/wic/-wic-codec-native-pixel-formats
    { GUID_WICPixelFormat8bppAlpha,        "WICPixelFormat8bppAlpha",        FMT_A8,       0 },
    { GUID_WICPixelFormat8bppGray,         "WICPixelFormat8bppGray",         FMT_R8,       0 },
    { GUID_WICPixelFormat16bppGray,        "WICPixelFormat16bppGray",        FMT_R16,      0 },
    { GUID_WICPixelFormat24bppRGB,         "WICPixelFormat24bppRGB",         FMT_RGB888,   0 },
    { GUID_WICPixelFormat24bppBGR,         "WICPixelFormat24bppBGR",         FMT_BGR888,   0 },
    { GUID_WICPixelFormat32bppRGB,         "WICPixelFormat32bppRGB",         FMT_RGB888,   0 },
    { GUID_WICPixelFormat32bppRGBA,        "WICPixelFormat32bppRGBA",        FMT_RGBA8888, 0 },
    { GUID_WICPixelFormat32bppBGR,         "WICPixelFormat32bppBGR",         FMT_BGR888,   0 },
    { GUID_WICPixelFormat32bppBGRA,        "WICPixelFormat32bppBGRA",        FMT_BGRA8888, 0 },
    { GUID_WICPixelFormat48bppRGB,         "WICPixelFormat48bppRGB",         FMT_RGB16,    0 },
    { GUID_WICPixelFormat48bppRGBHalf,     "WICPixelFormat48bppRGBHalf",     FMT_RGB16F,   0 },
    { GUID_WICPixelFormat96bppRGBFloat,    "WICPixelFormat96bppRGBFloat",    FMT_RGB32F,   0 },
    { GUID_WICPixelFormat64bppRGBA,        "WICPixelFormat64bppRGBA",        FMT_RGBA16,   0 },
    { GUID_WICPixelFormat64bppRGBAHalf,    "WICPixelFormat64bppRGBAHalf",    FMT_RGBA16F,  0 },
    { GUID_WICPixelFormat128bppRGBAFloat,  "WICPixelFormat128bppRGBAFloat",  FMT_RGBA32F,  0 },

    // pre-multiplied alpha
    { GUID_WICPixelFormat32bppPRGBA,       "WICPixelFormat32bppPRGBA",       FMT_RGBA8888, 1 },
    { GUID_WICPixelFormat32bppPBGRA,       "WICPixelFormat32bppPBGRA",       FMT_BGRA8888, 1 },
    { GUID_WICPixelFormat64bppPRGBA,       "WICPixelFormat64bppPRGBA",       FMT_RGBA16,   1 },
    { GUID_WICPixelFormat64bppPRGBAHalf,   "WICPixelFormat64bppPRGBAHalf",   FMT_RGBA16F,  1 },
    { GUID_WICPixelFormat128bppPRGBAFloat, "WICPixelFormat128bppPRGBAFloat", FMT_RGBA32F,  1 },

    // convert to WICPixelFormat32bppRGBA
    { GUID_WICPixelFormat1bppIndexed,           "WICPixelFormat1bppIndexed",           0, 0 },
    { GUID_WICPixelFormat2bppIndexed,           "WICPixelFormat2bppIndexed",           0, 0 },
    { GUID_WICPixelFormat4bppIndexed,           "WICPixelFormat4bppIndexed",           0, 0 },
    { GUID_WICPixelFormat8bppIndexed,           "WICPixelFormat8bppIndexed",           0, 0 },
    { GUID_WICPixelFormatBlackWhite,            "WICPixelFormatBlackWhite",            0, 0 },
    { GUID_WICPixelFormat2bppGray,              "WICPixelFormat2bppGray",              0, 0 },
    { GUID_WICPixelFormat4bppGray,              "WICPixelFormat4bppGray",              0, 0 },
    { GUID_WICPixelFormat8bppDepth,             "WICPixelFormat8bppDepth",             0, 0 },
    { GUID_WICPixelFormat8bppGain,              "WICPixelFormat8bppGain",              0, 0 },
    { GUID_WICPixelFormat24bppRGBGain,          "WICPixelFormat24bppRGBGain",          0, 0 },
    { GUID_WICPixelFormat32bppBGRGain,          "WICPixelFormat32bppBGRGain",          0, 0 },
    { GUID_WICPixelFormat16bppBGR555,           "WICPixelFormat16bppBGR555",           0, 0 },
    { GUID_WICPixelFormat16bppBGR565,           "WICPixelFormat16bppBGR565",           0, 0 },
    { GUID_WICPixelFormat16bppBGRA5551,         "WICPixelFormat16bppBGRA5551",         0, 0 },
    { GUID_WICPixelFormat32bppGrayFloat,        "WICPixelFormat32bppGrayFloat",        0, 0 },
    { GUID_WICPixelFormat48bppBGR,              "WICPixelFormat48bppBGR",              0, 0 },
    { GUID_WICPixelFormat64bppRGB,              "WICPixelFormat64bppRGB",              0, 0 },
    { GUID_WICPixelFormat64bppBGRA,             "WICPixelFormat64bppBGRA",             0, 0 },
    { GUID_WICPixelFormat64bppPBGRA,            "WICPixelFormat64bppPBGRA",            0, 0 },
    { GUID_WICPixelFormat16bppGrayFixedPoint,   "WICPixelFormat16bppGrayFixedPoint",   0, 0 },
    { GUID_WICPixelFormat32bppBGR101010,        "WICPixelFormat32bppBGR101010",        0, 0 },
    { GUID_WICPixelFormat48bppRGBFixedPoint,    "WICPixelFormat48bppRGBFixedPoint",    0, 0 },
    { GUID_WICPixelFormat48bppBGRFixedPoint,    "WICPixelFormat48bppBGRFixedPoint",    0, 0 },
    { GUID_WICPixelFormat96bppRGBFixedPoint,    "WICPixelFormat96bppRGBFixedPoint",    0, 0 },
    { GUID_WICPixelFormat128bppRGBFloat,        "WICPixelFormat128bppRGBFloat",        0, 0 },
    { GUID_WICPixelFormat32bppCMYK,             "WICPixelFormat32bppCMYK",             0, 0 },
    { GUID_WICPixelFormat64bppRGBAFixedPoint,   "WICPixelFormat64bppRGBAFixedPoint",   0, 0 },
    { GUID_WICPixelFormat64bppBGRAFixedPoint,   "WICPixelFormat64bppBGRAFixedPoint",   0, 0 },
    { GUID_WICPixelFormat64bppRGBFixedPoint,    "WICPixelFormat64bppRGBFixedPoint",    0, 0 },
    { GUID_WICPixelFormat128bppRGBAFixedPoint,  "WICPixelFormat128bppRGBAFixedPoint",  0, 0 },
    { GUID_WICPixelFormat128bppRGBFixedPoint,   "WICPixelFormat128bppRGBFixedPoint",   0, 0 },
    { GUID_WICPixelFormat64bppRGBHalf,          "WICPixelFormat64bppRGBHalf",          0, 0 },
    { GUID_WICPixelFormat32bppRGBE,             "WICPixelFormat32bppRGBE",             0, 0 },
    { GUID_WICPixelFormat16bppGrayHalf,         "WICPixelFormat16bppGrayHalf",         0, 0 },
    { GUID_WICPixelFormat32bppGrayFixedPoint,   "WICPixelFormat32bppGrayFixedPoint",   0, 0 },
    { GUID_WICPixelFormat32bppRGBA1010102,      "WICPixelFormat32bppRGBA1010102",      0, 0 },
    { GUID_WICPixelFormat32bppRGBA1010102XR,    "WICPixelFormat32bppRGBA1010102XR",    0, 0 },
    { GUID_WICPixelFormat32bppR10G10B10A2,      "WICPixelFormat32bppR10G10B10A2",      0, 0 },
    { GUID_WICPixelFormat32bppR10G10B10A2HDR10, "WICPixelFormat32bppR10G10B10A2HDR10", 0, 0 },
    { GUID_WICPixelFormat64bppCMYK,             "WICPixelFormat64bppCMYK",             0, 0 },
    { GUID_WICPixelFormat24bpp3Channels,        "WICPixelFormat24bpp3Channels",        0, 0 },
    { GUID_WICPixelFormat32bpp4Channels,        "WICPixelFormat32bpp4Channels",        0, 0 },
    { GUID_WICPixelFormat40bppCMYKAlpha,        "WICPixelFormat40bppCMYKAlpha",        0, 0 },
    { GUID_WICPixelFormat80bppCMYKAlpha,        "WICPixelFormat80bppCMYKAlpha",        0, 0 },
    { GUID_WICPixelFormat8bppY,                 "WICPixelFormat8bppY",                 0, 0 },
    { GUID_WICPixelFormat8bppCb,                "WICPixelFormat8bppCb",                0, 0 },
    { GUID_WICPixelFormat8bppCr,                "WICPixelFormat8bppCr",                0, 0 },
    { GUID_WICPixelFormat16bppCbCr,             "WICPixelFormat16bppCbCr",             0, 0 },
};

FormatInfo containerInfo[] = {
    { GUID_ContainerFormatBmp,    "Bmp",    0,0 },
    { GUID_ContainerFormatPng,    "Png",    0,0 },
    { GUID_ContainerFormatIco,    "Ico",    0,0 },
    { GUID_ContainerFormatJpeg,   "Jpeg",   0,0 },
    { GUID_ContainerFormatTiff,   "Tiff",   0,0 },
    { GUID_ContainerFormatGif,    "Gif",    0,0 },
    { GUID_ContainerFormatWmp,    "Wmp",    0,0 },
    { GUID_ContainerFormatDds,    "Dds",    0,0 },
    { GUID_ContainerFormatAdng,   "Adng",   0,0 },
    { GUID_ContainerFormatHeif,   "Heif",   0,0 },
    { GUID_ContainerFormatWebp,   "Webp",   0,0 },
    { GUID_ContainerFormatRaw,    "Raw",    0,0 },
    { GUID_ContainerFormatJpegXL, "JpegXL", 0,0 },
};

static const int GetFormat(const GUID& guid) {
    for (int i = 0; i < sizeof(formatInfo) / sizeof(FormatInfo); i++) {
        if (formatInfo[i].guid == guid) return formatInfo[i].format;
    }
    return FMT_UNKNOWN;
}
static const GUID GetFormat(const int fmt) {
    for (int i = 0; i < sizeof(formatInfo) / sizeof(FormatInfo); i++) {
        if (formatInfo[i].format == fmt) return formatInfo[i].guid;
    }
    return GUID_WICPixelFormat32bppRGBA;
}

static const char* GetFormatStr(const GUID& guid) {
    for (int i = 0; i < sizeof(formatInfo) / sizeof(FormatInfo); i++) {
        if (formatInfo[i].guid == guid) return formatInfo[i].name;
    }
    return "(unknown)";
}
static const char* GetContainerFormatStr(const GUID& guid) {
    for (int i = 0; i < sizeof(containerInfo) / sizeof(FormatInfo); i++) {
        if (containerInfo[i].guid == guid) return containerInfo[i].name;
    }
    return "(unknown)";
}
static const int GetPreMultiplied(const GUID& guid) {
    for (int i = 0; i < sizeof(formatInfo) / sizeof(FormatInfo); i++) {
        if (formatInfo[i].guid == guid) return formatInfo[i].pma;
    }
    return 0;
}

double UI8toDouble(UINT64 val){
    UINT32 num = (UINT32)(val & 0xFFFFFFFF);
    UINT32 denom = (UINT32)(val >> 32);
    if (denom != 0) return static_cast<double>(num) / denom;
    return 0.0;
}


std::string dumpExif(ComPtr<IWICMetadataQueryReader> mqr, TagOfInterest *tagList, bool isJpeg){
    PROPVARIANT val;
    PropVariantInit(&val);
    std::string ret = "";
    if (tagList == 0){
        tagList = ExifParser::getDefaultTagOfInterest();
    }else{

    }
    int i = 0;
    while(1){
        if (tagList[i].parentIFD == 0 && tagList[i].tag == 0) break;
        std::wstring path = isJpeg?L"/app1":L"";
        if (tagList[i].parentIFD == IFD_Exif){
            path += std::format(L"/ifd/exif/{{ushort={}}}", tagList[i].tag);
        }else if (tagList[i].parentIFD == IFD_GPS){
            path += std::format(L"/ifd/gps/{{ushort={}}}", tagList[i].tag);
        }else{
            path += std::format(L"/ifd/{{ushort={}}}", tagList[i].tag);
        }
        HRESULT hr = mqr->GetMetadataByName(path.c_str(), &val);
        if (SUCCEEDED(hr)){

            switch(val.vt){
              case VT_LPSTR:
                ret += std::format("{}: {}\n", tagList[i].name, val.pszVal);
                break;
              case VT_UI2:
              case VT_UI4:
                ret += std::format("{}: {}\n", tagList[i].name, ExifParser::formatInt(tagList[i].fmt, val.uiVal, tagList[i].tag));
                break;
              case VT_UI8:
                {
                    UINT64 v = val.uhVal.QuadPart;
                    UINT32 num = (UINT32)(v & 0xFFFFFFFF);
                    UINT32 denom = (UINT32)(v >> 32);
                    ret += std::format("{}: {}\n", tagList[i].name, ExifParser::formatDouble(tagList[i].fmt, num, denom));
                }
                break;
              case VT_I8:
                {
                    INT64 v = val.uhVal.QuadPart;
                    INT32 num = (INT32)(v & 0xFFFFFFFF);
                    INT32 denom = (INT32)(v >> 32);
                    ret += std::format("{}: {}\n", tagList[i].name, ExifParser::formatDouble(tagList[i].fmt, num, denom));
                }
                break;
              case VT_VECTOR|VT_UI8:
                if (tagList[i].fmt == PrintFormat::LatLon){
                    ULONG count = val.cauh.cElems;
                    ULARGE_INTEGER* elems = val.cauh.pElems;
                    if (count == 3){
                        double dd = UI8toDouble(elems[0].QuadPart);
                        double mm = UI8toDouble(elems[1].QuadPart);
                        double ss = UI8toDouble(elems[2].QuadPart);
                        ret += std::format("{}: {}deg {}' {:.2f}\"\n", tagList[i].name, dd, mm, ss);
                        break;
                    }
                }
                break;
              case VT_VECTOR|VT_UI1:
                {
                    ULONG count = val.caub.cElems;
                    BYTE *data  = val.caub.pElems;
                    if (tagList[i].fmt == PrintFormat::Version){
                        ret += std::format("{}: ", tagList[i].name);
                        for (unsigned int j=0; j<count; j++){
                            if (j != 0) ret +=".";
                            char val = (char)data[j];
                            if (val <= 9) val+='0';
                            ret += std::format("{}", val);
                        }
                        ret += "\n";
                    }else if (tagList[i].fmt == PrintFormat::Enum){
                        ret += std::format("{}: ", tagList[i].name);
                        for (unsigned int j=0; j<count; j++){
                            if (i != 0) ret +=", ";
                            ret += std::format("{}", ExifParser::formatInt(tagList[i].fmt, data[j], tagList[i].tag));
                        }
                        ret += "\n";
                    }else{
                        ret += std::format("{}: ", tagList[i].name);
                        for (unsigned int j=0; j<count; j++){
                            if (i != 0) ret +=", ";
                            ret += std::format("{}", data[j]);
                        }
                        ret += "\n";
                    }
                }
                break;
            }
        }
        PropVariantClear(&val);
        i++;
    }
    return ret;
}

std::string setIccResult(IccParser *iccParser, LOAD_RESULT *outResult, int *cs, int *tf){
    *cs = iccParser->getColorSpace();
    *tf = iccParser->getTransferFunction();
    outResult->gamma = (float)iccParser->getGamma();
    outResult->flag |= 0x08;
    XYZ rxyz = iccParser->getRxyz();
    outResult->rX = (float)rxyz.x;
    outResult->rY = (float)rxyz.y;
    outResult->rZ = (float)rxyz.z;
    XYZ gxyz = iccParser->getGxyz();
    outResult->gX = (float)gxyz.x;
    outResult->gY = (float)gxyz.y;
    outResult->gZ = (float)gxyz.z;
    XYZ bxyz = iccParser->getBxyz();
    outResult->bX = (float)bxyz.x;
    outResult->bY = (float)bxyz.y;
    outResult->bZ = (float)bxyz.z;
    XYZ wtpt = iccParser->getWtpt();
    outResult->wX = (float)wtpt.x;
    outResult->wY = (float)wtpt.y;
    outResult->wZ = (float)wtpt.z;
    return iccParser->dumpIccInfo();
}

extern "C" {

    //------------------------------------------------------------------------------
    WICLOADERLIB_API const char* get_version()
    {
        static const char* buf = "1.0";
        return buf;
    }


    //------------------------------------------------------------------------------
    WICLOADERLIB_API int load_file(const wchar_t* file, LOAD_RESULT* outResult, uint32_t opt)
    {
        std::string loader_info = "";
        std::string primariesInfo = "";
        std::string itemPropertyInfo = "";
        std::string exif_info = "";

        bool use16F = false;
        outResult->RGBScale = 1.0f;
        outResult->flag = 0;
        outResult->gamma = 1.0f;

        IWICImagingFactory* factory = GetWICFactory();
        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(file, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        if (SUCCEEDED(hr)) {
            UINT frameCount = 0;
            hr = decoder->GetFrameCount(&frameCount);
            if (SUCCEEDED(hr)) {
                GUID containerFormat;
                int cs = CS_UNKNOWN;
                int tf = TF_DEFAULT;
                decoder->GetContainerFormat(&containerFormat);

                // pre-scan
                IccParser *iccParser = new IccParser();
                if (iccParser->parseIcc(file) == 1){
                    primariesInfo = setIccResult(iccParser, outResult, &cs, &tf);
                }else{
                    UINT count = 0;
                    hr = decoder->GetColorContexts(0, nullptr, &count);
                    if (SUCCEEDED(hr) && count > 0) {
                        std::vector<ComPtr<IWICColorContext>> contexts(count);
                        for (unsigned int i=0; i<count; i++){
                            factory->CreateColorContext(&contexts[i]);
                        }

                        hr = decoder->GetColorContexts(count, contexts[0].GetAddressOf(), &count);
                        if (SUCCEEDED(hr) && count > 0) {
                            for (unsigned int i=0; i<count; i++){
                                WICColorContextType type;
                                hr = contexts[i]->GetType(&type);
                                if (SUCCEEDED(hr) && type == WICColorContextProfile){
                                    UINT size = 0;
                                    hr = contexts[i]->GetProfileBytes(0, nullptr, &size);
                                    if (SUCCEEDED(hr) && size> 0){
                                        std::vector<BYTE> icc(size);
                                        contexts[i]->GetProfileBytes(size, icc.data(), &size);
                                        IccParser *iccParser = new IccParser();
                                        if (iccParser->parseIcc(icc.data(), size) == 1){
                                            primariesInfo = setIccResult(iccParser, outResult, &cs, &tf);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ExifParser exifParser = ExifParser((opt&1)?0:ExifParser::getSimpleTagOfInterest());
                // FIXME: WICが何をどう変換しているのかよくわからないので適当に補正
                if (containerFormat == GUID_ContainerFormatJpeg){
                    if (exifParser.parseJpeg(file, IFD_Exif) > 0){
                        exif_info += exifParser.dumpExif();
                    }
                    switch(tf) {
                      case TF_PQ:
                        tf = TF_SRGB; //LINEAR;
                        use16F = true;
                        outResult->RGBScale = 2.0f;  //?
                        break;
                      case TF_HLG:
                        tf = TF_LINEAR;
                        use16F = true;
                        outResult->RGBScale = 203.0f/1000.0f;
                        break;
                    }
                }else if (containerFormat == GUID_ContainerFormatTiff){
                    if (exifParser.parseTiff(file, IFD_Exif) > 0){
                        exif_info += exifParser.dumpExif();
                    }
                }else if (containerFormat == GUID_ContainerFormatWmp){ // JpegXR
                    cs = CS_SRGB;
                    tf = TF_LINEAR;
                }else if (containerFormat == GUID_ContainerFormatHeif){
                    cs = CS_SRGB;
                    tf = TF_DEFAULT; //LINEAR;
                    unsigned short cp=0, tc=0;
                    if (exifParser.parseHeif(file, &cp, &tc, IFD_Exif) > 0){
                        exif_info += exifParser.dumpExif();
                        if (cp != 0 && tc != 0){
                            itemPropertyInfo = std::format("ItemPropertyContainer CS:{} TF:{}\n", IccParser::csStringFromCICP(cp), IccParser::tfStringFromCICP(tc));
                        }

                        int _cs = IccParser::colorSpaceFromCICP(cp);
                        int _tf = IccParser::transferFunctionFromCICP(tc);
                        switch(_cs) {
                          case CS_BT709:
                          case CS_BT601:
                            cs = _cs;
                            break;
                          default:
                          case CS_BT2020:
                            break;
                        }
                        switch(_tf) {
                          case TF_PQ:
                            tf = TF_LINEAR;
                            use16F = true;
                            break;
                          case TF_HLG:
                            tf = TF_LINEAR;
                            use16F = true;
                            outResult->RGBScale = 203.0f/1000.0f;
                            break;
                          case TF_BT709:
                          case TF_BT601:
                          case TF_LINEAR:
                          case TF_SRGB:
                            tf = _tf;
                            break;
                          default:
                            break;
                        }
                    }
                }

                unsigned char *dst=0;
                UINT offset = 0;
                UINT w = 0, h = 0;
                int format = FMT_UNKNOWN;
                WICPixelFormatGUID sourceFormat;
                PAGE *pages = 0;

                if (frameCount > 1){
                    pages = (PAGE *)calloc(sizeof(PAGE), frameCount);
                }
                for (unsigned int i=0; i<frameCount; i++){
                    ComPtr<IWICBitmapFrameDecode> frame;
                    hr = decoder->GetFrame(i, &frame);
                    if (SUCCEEDED(hr)) {
                        frame->GetSize(&w, &h);
                        UINT16 orientation = 1;
                        ComPtr<IWICMetadataQueryReader> mqr;
                        if (SUCCEEDED(frame->GetMetadataQueryReader(&mqr))) {
                            PROPVARIANT val;
                            PropVariantInit(&val);
                            if (SUCCEEDED(mqr->GetMetadataByName(L"/app1/ifd/{ushort=274}", &val))){
                                if (val.vt == VT_UI2) orientation = val.uiVal;
                            }
                            PropVariantClear(&val);
                        }
                        outResult->orientation = orientation;

                        frame->GetPixelFormat(&sourceFormat);
                        format = GetFormat(sourceFormat);
                        if (format == FMT_UNKNOWN) format = FMT_RGBA8888;
                        if (use16F){
                            format = fmt2a(format)?FMT_RGBA16F:FMT_RGB16F;
                        }
                        GUID targetFormat = GetFormat(format);
                        UINT bytePerPixel = fmt2bpp(format);
                        if (i == 0){
                            dst = (unsigned char*)malloc(w * h * bytePerPixel * frameCount);
                            if (dst == 0) return ERR_NOMEM;
                            outResult->width = w;
                            outResult->height = h;

                            if (primariesInfo == "") {
                                UINT count = 0;
                                hr = frame->GetColorContexts(0, nullptr, &count);
                                if (SUCCEEDED(hr) && count > 0) {
                                    std::vector<ComPtr<IWICColorContext>> contexts(count);
                                    for (unsigned int i=0; i<count; i++){
                                        factory->CreateColorContext(&contexts[i]);
                                    }
                                    hr = frame->GetColorContexts(count, contexts[0].GetAddressOf(), &count);
                                    if (SUCCEEDED(hr) && count > 0) {
                                        for (unsigned int i=0; i<count; i++){
                                            WICColorContextType type;
                                            hr = contexts[i]->GetType(&type);
                                            if (SUCCEEDED(hr) && type == WICColorContextProfile){
                                                UINT size = 0;
                                                hr = contexts[i]->GetProfileBytes(0, nullptr, &size);
                                                if (SUCCEEDED(hr) && size> 0){
                                                    std::vector<BYTE> icc(size);
                                                    contexts[i]->GetProfileBytes(size, icc.data(), &size);
                                                    IccParser *iccParser = new IccParser();
                                                    if (iccParser->parseIcc(icc.data(), size) == 1){
                                                        primariesInfo = setIccResult(iccParser, outResult, &cs, &tf);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ComPtr<IWICFormatConverter> converter;
                        hr = factory->CreateFormatConverter(&converter);
                        if (SUCCEEDED(hr)) {
                            hr = converter->Initialize(frame.Get(), targetFormat, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);
                            if (SUCCEEDED(hr)) {
                                ComPtr<IWICMetadataQueryReader> mqr;
                                hr = frame->GetMetadataQueryReader(&mqr);
                                if (SUCCEEDED(hr)) {
                                    if (frameCount > 1){
                                        PROPVARIANT v;
                                        PropVariantInit(&v);
                                        if (SUCCEEDED(mqr->GetMetadataByName(L"/imgdesc/Top", &v))){
                                            pages[i].top = (v.vt == VT_UI2) ? v.uiVal : 0;
                                        }else{
                                            pages[i].top = 0;
                                        }
                                        PropVariantClear(&v);
                                        if (SUCCEEDED(mqr->GetMetadataByName(L"/imgdesc/Left", &v))){
                                            pages[i].left = (v.vt == VT_UI2) ? v.uiVal : 0;
                                        }else{
                                            pages[i].left = 0;
                                        }
                                        PropVariantClear(&v);
                                        if (SUCCEEDED(mqr->GetMetadataByName(L"/imgdesc/Width", &v))){
                                            pages[i].width = (v.vt == VT_UI2) ? v.uiVal : 0;
                                            w = pages[i].width;
                                        }else{
                                            pages[i].width = w;
                                        }
                                        PropVariantClear(&v);
                                        if (SUCCEEDED(mqr->GetMetadataByName(L"/imgdesc/Height", &v))){
                                            pages[i].height = (v.vt == VT_UI2) ? v.uiVal : 0;
                                            h = pages[i].height;
                                        }else{
                                            pages[i].height = h;
                                        }
                                        PropVariantClear(&v);
                                        if (SUCCEEDED(mqr->GetMetadataByName(L"/grctlext/Delay", &v))){
                                            pages[i].delay = (v.vt == VT_UI2) ? v.uiVal : 0;
                                        }else{
                                            pages[i].delay = 0;
                                        }
                                        PropVariantClear(&v);
                                    }

                                    if (exif_info == ""){
                                        // use WIC metadata
                                        if (containerFormat == GUID_ContainerFormatJpeg){
                                            exif_info = dumpExif(mqr, 0, true);
                                        }else{
                                            exif_info = dumpExif(mqr, 0, false);
                                        }
                                    }
                                }

                                UINT stride = w * bytePerPixel;
                                hr = converter->CopyPixels(nullptr, stride, stride*h, dst+offset);
                                // heif画像などで WinRT originate error - 0x80004001 : 'Conversion unavailable: No suitable scaler found.' が大量に出るが S_OK なら無害らしい?
                                if (SUCCEEDED(hr)) {
                                    offset += stride*h;
                                }
                            }
                        }
                    }
                }

                void *tmp = realloc(dst, offset);
                if (tmp != NULL){
                    outResult->dst = (uint32_t *)tmp;
                }else{
                    outResult->dst = (uint32_t *)dst;
                }
                outResult->dst_len = offset;

                outResult->numFrames = frameCount;
                outResult->format = format;
                outResult->flag |= GetPreMultiplied(sourceFormat);
                if (containerFormat == GUID_ContainerFormatGif) outResult->flag |= 0x02;
                outResult->pages = (uint32_t *)pages;
                outResult->colorspace = cs | tf;

                loader_info += std::format("Container Format: {}\n", GetContainerFormatStr(containerFormat));
                loader_info += std::format("Width: {}\n", outResult->width);
                loader_info += std::format("Height: {}\n", outResult->height);
                loader_info += std::format("PreMultiplied: {}\n", outResult->flag?"True":"False");

                loader_info += std::format("Pixel Format: {}\n", GetFormatStr(sourceFormat));
                if (itemPropertyInfo != "") loader_info += itemPropertyInfo;
                if (primariesInfo != "") loader_info += primariesInfo;
                if (exif_info != "") loader_info += "\nEXIF:\n" + exif_info;

                std::string format_info = GetFormatStr(sourceFormat);
                const std::string::size_type fsize = format_info.size();
                outResult->format_string = (uint32_t*)new char[fsize + 1];
                memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

                const std::string::size_type lsize = loader_info.size();
                outResult->loader_string = (uint32_t*)new char[lsize + 1];
                memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);

                return ERR_OK;
            }
        }
        return ERR_UNSUPPORTED;
    }


    //------------------------------------------------------------------------------
    // TODO: meta data etc.
    WICLOADERLIB_API int save_jxr(const wchar_t *file, unsigned int width, unsigned int height, int dataSize, void *data, uint32_t opt) {
        const wchar_t* ext = wcsrchr(file, L'.');
        if (ext == NULL || _wcsicmp(ext, L".jxr") != 0) return ERR_UNSUPPORTED;

        GUID format;
        if (dataSize == (width*height*6)){
            format = GUID_WICPixelFormat48bppRGBHalf;
        }else if (dataSize == (width*height*8)){
            format = GUID_WICPixelFormat64bppRGBAHalf;
        }else if (dataSize == (width*height*12)){
            format = GUID_WICPixelFormat96bppRGBFloat;
        }else if (dataSize == (width*height*16)){
            format = GUID_WICPixelFormat128bppRGBAFloat;
        }else{
            return ERR_UNSUPPORTED;
        }

        IWICImagingFactory* spFactory = GetWICFactory();
        if (!spFactory) {
            return ERR_UNKNOWN;
        }
        HRESULT hr;

        ComPtr<IWICBitmap> spBitmap;
        hr = spFactory->CreateBitmap(
            width,
            height,
            format,
            WICBitmapCacheOnDemand,
            &spBitmap);
        if (FAILED(hr)) return ERR_UNKNOWN;

        ComPtr<IWICBitmapLock> spLock;
        WICRect rect = {0, 0, (int)width, (int)height};
        hr = spBitmap->Lock(&rect, WICBitmapLockWrite, &spLock);
        if (FAILED(hr)) return ERR_UNKNOWN;

        BYTE* pBits = NULL;
        UINT cbBufferSize = 0;
        hr = spLock->GetDataPointer(&cbBufferSize, &pBits);
        if (SUCCEEDED(hr)) {
            if (dataSize == cbBufferSize){
                memcpy(pBits, data, dataSize);
            }
            spLock.Reset();
        }
        if (FAILED(hr) || dataSize != cbBufferSize)  return ERR_UNKNOWN;


        ComPtr<IWICBitmapEncoder> spEncoder;
        hr = spFactory->CreateEncoder(GUID_ContainerFormatWmp, NULL, &spEncoder);
        if (FAILED(hr)) return ERR_UNSUPPORTED;

        ComPtr<IStream> spStream;
        hr = CreateStreamOnHGlobal(NULL, TRUE, &spStream);
        if (FAILED(hr)) return ERR_UNKNOWN;

        hr = spEncoder->Initialize(spStream.Get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) return ERR_UNKNOWN;

        ComPtr<IWICBitmapFrameEncode> spFrameEncode;
        hr = spEncoder->CreateNewFrame(&spFrameEncode, NULL);
        if (FAILED(hr)) return ERR_UNKNOWN;

        hr = spFrameEncode->Initialize(NULL);
        if (FAILED(hr)) return ERR_UNKNOWN;

        hr = spFrameEncode->WriteSource(spBitmap.Get(), NULL);
        if (FAILED(hr)) return ERR_UNKNOWN;

        hr = spFrameEncode->Commit();
        if (FAILED(hr)) return ERR_UNKNOWN;

        hr = spEncoder->Commit();
        if (FAILED(hr)) return ERR_UNKNOWN;

        HGLOBAL hMem = NULL;
        GetHGlobalFromStream(spStream.Get(), &hMem);
        BOOL success = 0;
        if (hMem) {
            LPVOID pMem = GlobalLock(hMem);
            if (pMem) {
                HANDLE hFile = CreateFile(
                    file,
                    GENERIC_WRITE,
                    0,
                    NULL,
                    CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD bytesWritten;
                    success = WriteFile(hFile, pMem, (DWORD)GlobalSize(hMem), &bytesWritten, NULL);
                    CloseHandle(hFile);
                }
                GlobalUnlock(hMem);
            }
        }
        if (success == 0) return ERR_UNKNOWN;
        return ERR_OK;
    }



    //------------------------------------------------------------------------------
    WICLOADERLIB_API void free_mem(LOAD_RESULT* result) {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
        free(result->pages);
    }
}
