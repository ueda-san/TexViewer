#include <stdio.h>
#include <windows.h>
#include <stringapiset.h>

#include <string>
#include <format>
#include <algorithm>

#include "AVIFLoaderLib.h"
#include "ICCParser.h"
#include "avif.h"
//#include "internal.h"

using namespace ICCParser;

std::string BoolStr(int val) {
    return (val == 0)?"False":"True";
}

std::string ColorPrimariesStr(int val) {
    std::string _ColorPrimariesStr[] = {
        "UNKNOWN",      // 0
        "BT709 / SRGB",
        "UNSPECIFIED",
        "-",
        "BT470M",
        "BT470BG",
        "BT601",
        "SMPTE240",
        "GENERIC_FILM",
        "BT2020 / BT2100",
        "XYZ",
        "SMPTE431",
        "SMPTE432 / DCI_P3", // 12
    };
    if (val <= 12) return _ColorPrimariesStr[val];
    if (val == 22) return "EBU3213";
    return "(Unknown)";
}

std::string TransferCharacteristicsStr(int val) {
    std::string _TransferCharacteristicsStr[] = {
        "UNKNOWN", // 0
        "BT709",
        "UNSPECIFIED",
        "-",
        "BT470M",
        "BT470BG",
        "BT601",
        "SMPTE240",
        "LINEAR",
        "LOG100",
        "LOG100_SQRT10",
        "IEC61966",
        "BT1361",
        "SRGB",
        "BT2020_10BIT",
        "BT2020_12BIT",
        "PQ / SMPTE2084",
        "SMPTE428",
        "HLG",
    };
    if (val <= 18) return _TransferCharacteristicsStr[val];
    return "(Unknown)";
}

std::string MatrixCoefficientsStr(int val) {
    std::string _MatrixCoefficientsStr[] = {
        "IDENTITY", //0
        "BT709",
        "UNSPECIFIED",
        "-",
        "FCC",
        "BT470BG",
        "BT601",
        "SMPTE240",
        "YCGCO",
        "BT2020_NCL",
        "BT2020_CL",
        "SMPTE2085",
        "CHROMA_DERIVED_NCL",
        "CHROMA_DERIVED_CL",
        "ICTCP",
        "-",
        "YCGCO_RE",
        "YCGCO_RO", //17
    };
    if (val <= 17) return _MatrixCoefficientsStr[val];
    return "(Unknown)";
}


#define F16_MULTIPLIER 1.9259299444e-34f

typedef union avifF16
{
    float f;
    uint32_t u32;
} avifF16;

static inline uint16_t avifFloatToF16(float v)
{
    avifF16 f16;
    f16.f = v * F16_MULTIPLIER;
    return (uint16_t)(f16.u32 >> 13);
}

static inline float avifF16ToFloat(uint16_t v)
{
    avifF16 f16;
    f16.u32 = v << 13;
    return f16.f / F16_MULTIPLIER;
}



extern "C" {
    //------------------------------------------------------------------------------
    AVIFLOADERLIB_API const char *get_version()
    {
        return avifVersion();
    }

    //------------------------------------------------------------------------------
    AVIFLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult, uint32_t opt)
    {
        FILE *fp;
        if (_wfopen_s(&fp, file, L"rb") != 0) return ERR_NOTFOUND;
        if (fp == 0) return ERR_NOTFOUND;
        fseek(fp, 0, SEEK_END);
        size_t len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char *data = (unsigned char *)malloc(len);
        if (data == NULL) {
            fclose(fp);
            return ERR_NOMEM;
        }
        size_t bytes_read = fread(data, 1, len, fp);
        fclose(fp);

        avifRGBImage rgb;
        memset(&rgb, 0, sizeof(rgb));

        avifDecoder * decoder = avifDecoderCreate();
        if (decoder == NULL){
            free(data);
            return ERR_NOMEM;
        }
        decoder->strictFlags = 0;
        decoder->imageCountLimit = 1;
        decoder->imageContentToDecode = AVIF_IMAGE_CONTENT_ALL;
        decoder->codecChoice = AVIF_CODEC_CHOICE_AOM;
        //decoder->codecChoice = AVIF_CODEC_CHOICE_DAV1D;
        //decoder->codecChoice = AVIF_CODEC_CHOICE_LIBGAV1;

        int errCode = ERR_UNSUPPORTED;
        avifResult result = avifDecoderSetIOMemory(decoder, data, len);
        if (result == AVIF_RESULT_OK){
            result = avifDecoderParse(decoder);
            if (result == AVIF_RESULT_OK){
                //char buf[256];
                //avifCodecVersions(buf);
                std::string loader_info = ""; //"AvailableCodec: " + std::string(buf) + "\n";

                while (avifDecoderNextImage(decoder) == AVIF_RESULT_OK) {
                    loader_info += std::format("Format: Avif ({})\n", avifPixelFormatToString(decoder->image->yuvFormat));
                    loader_info += std::format("has GainMap: {}\n", (decoder->image->gainMap==0)?"False":"True");
                    loader_info += std::format("OwnsAlphaPlane: {}\n", BoolStr(decoder->image->imageOwnsAlphaPlane));
                    loader_info += std::format("Premultiplied: {}\n", BoolStr(decoder->image->alphaPremultiplied));
                    loader_info += std::format("Range: {}\n", (decoder->image->yuvRange==0)?"LIMITED":"FULL");
                    loader_info += std::format("ColorPrimaries: {}\n",
                                               ColorPrimariesStr(decoder->image->colorPrimaries));
                    loader_info += std::format("TransferCharacteristics: {}\n",
                                               TransferCharacteristicsStr(decoder->image->transferCharacteristics));
                    loader_info += std::format("matrixCoefficients: {}\n",
                                               MatrixCoefficientsStr(decoder->image->matrixCoefficients));
                    loader_info += std::format("maxCLL: {}\n", decoder->image->clli.maxCLL);

                    avifRGBImageSetDefaults(&rgb, decoder->image);
                    rgb.isFloat = AVIF_TRUE;
                    rgb.depth = 16;
                    if (decoder->alphaPresent){
                        rgb.format = AVIF_RGB_FORMAT_RGBA;
                    }else{
                        rgb.format = AVIF_RGB_FORMAT_RGB;
                        rgb.ignoreAlpha = AVIF_TRUE;
                    }
                    //rgb.avoidLibYUV = AVIF_TRUE;
                    rgb.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC; //_BEST_QUALITY;

                    int channel = avifRGBFormatChannelCount(rgb.format);
                    const uint32_t rowBytes = rgb.width * channel * 2;
                    rgb.pixels = (uint8_t *)malloc((size_t)rowBytes * rgb.height);
                    if (rgb.pixels == 0) {
                        errCode = ERR_NOMEM;
                        break;
                    }
                    rgb.rowBytes = rowBytes;

                    struct CicpValues {
                        avifColorPrimaries color_primaries;
                        avifTransferCharacteristics transfer_characteristics;
                        avifMatrixCoefficients matrix_coefficients;
                    };

                    CicpValues cicp = {decoder->image->colorPrimaries, decoder->image->transferCharacteristics, decoder->image->matrixCoefficients};

                    if (decoder->image->gainMap != 0){
                        avifImage* image = decoder->image;

                        const float base_hdr_hreadroom =
                            static_cast<float>(image->gainMap->baseHdrHeadroom.n) /
                            image->gainMap->baseHdrHeadroom.d;
                        const float alternate_hdr_hreadroom =
                            static_cast<float>(image->gainMap->alternateHdrHeadroom.n) /
                            image->gainMap->alternateHdrHeadroom.d;
                        float headroom = 0.0f;
                        if ((opt&0x0002)==0){
                            headroom = max(base_hdr_hreadroom, alternate_hdr_hreadroom);
                        }
                        const bool tone_mapping_to_hdr = (headroom > 0.0f);
                        // We are either tone mapping to the base image (i.e. leaving it as is),
                        // or tone mapping to the alternate image (i.e. fully applying the gain map),
                        // or tone mapping in between (partially applying the gain map).
                        const bool tone_mapping_to_base =
                            (headroom <= base_hdr_hreadroom &&
                             base_hdr_hreadroom <= alternate_hdr_hreadroom) ||
                            (headroom >= base_hdr_hreadroom &&
                             base_hdr_hreadroom >= alternate_hdr_hreadroom);
                        const bool tone_mapping_to_alternate =
                            (headroom <= alternate_hdr_hreadroom &&
                             alternate_hdr_hreadroom <= base_hdr_hreadroom) ||
                            (headroom >= alternate_hdr_hreadroom &&
                             alternate_hdr_hreadroom >= base_hdr_hreadroom);
                        const bool base_is_hdr = (base_hdr_hreadroom != 0.0f);

                        if (tone_mapping_to_base || (tone_mapping_to_hdr && base_is_hdr)) {
                            cicp = {image->colorPrimaries, image->transferCharacteristics,
                                image->matrixCoefficients};
                        } else {
                            cicp = {image->gainMap->altColorPrimaries,
                                image->gainMap->altTransferCharacteristics,
                                image->gainMap->altMatrixCoefficients};
                        }
                        if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
                            // TODO(maryla): for now avifImageApplyGainMap always uses the primaries of
                            // the base image, but it should take into account the metadata's
                            // useBaseColorSpace property.
                            cicp.color_primaries = image->colorPrimaries;
                        }
                        if (cicp.transfer_characteristics ==
                            AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
                            cicp.transfer_characteristics = static_cast<avifTransferCharacteristics>(
                                tone_mapping_to_hdr ? AVIF_TRANSFER_CHARACTERISTICS_PQ
                                : AVIF_TRANSFER_CHARACTERISTICS_SRGB);
                        }

                        avifContentLightLevelInformationBox clli_box = {};
                        if (tone_mapping_to_base) {
                            clli_box = image->clli;
                        } else if (tone_mapping_to_alternate) {
                            clli_box = image->gainMap->image->clli;
                        }
                        bool clli_set = (clli_box.maxCLL != 0) || (clli_box.maxPALL != 0);

                        avifDiagnostics diag;
                        avifRGBImage toneMappedImage;
                        memset(&toneMappedImage, 0, sizeof(toneMappedImage));
                        toneMappedImage.format = AVIF_RGB_FORMAT_RGB;
                        toneMappedImage.depth = 16;
                        toneMappedImage.isFloat = AVIF_TRUE;
                        result = avifImageApplyGainMap(decoder->image,
                                                       image->gainMap,
                                                       headroom,
                                                       cicp.color_primaries,
                                                       cicp.transfer_characteristics,
                                                       &toneMappedImage,
                                                       clli_set ? nullptr : &clli_box,
                                                       &diag);
                        if (result == AVIF_RESULT_OK &&
                            rgb.width == toneMappedImage.width &&
                            rgb.height == toneMappedImage.height &&
                            rgb.depth == toneMappedImage.depth &&
                            rgb.rowBytes == toneMappedImage.rowBytes){
                            memcpy(rgb.pixels, toneMappedImage.pixels, rgb.rowBytes * rgb.height);
                            avifRGBImageFreePixels(&toneMappedImage);
                        }
                    }else{
                        result = avifImageYUVToRGB(decoder->image, &rgb);
                    }

                    if (result == AVIF_RESULT_OK) {
                        int cs = CS_UNKNOWN;
                        if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_BT709){
                            cs = CS_BT709;
                        }else if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_BT601){
                            cs = CS_BT601;
                        }else if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_SRGB){
                            cs = CS_SRGB;
                        }else if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_BT2020){
                            cs = CS_BT2020;
                        }else if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_SMPTE431){
                            cs = CS_DCI_P3;
                        }else if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_SMPTE432){
                            cs = CS_DISPLAY_P3;
                        }else{
                            float prims[8];
                            avifColorPrimariesGetValues(cicp.color_primaries, prims);
                            Primaries primaries;
                            primaries.rx = prims[0]; primaries.ry = prims[1];
                            primaries.gx = prims[2]; primaries.gy = prims[3];
                            primaries.bx = prims[4]; primaries.by = prims[5];
                            primaries.wx = prims[6]; primaries.wy = prims[7];
                            cs = IccParser::colorSpaceFromPrimaries(primaries);
                            loader_info += IccParser::dumpPrimaries(primaries, cs);
                        }
                        int tf = TF_DEFAULT;
                        //switch (decoder->image->transferCharacteristics){
                        switch (cicp.transfer_characteristics){
                          case AVIF_TRANSFER_CHARACTERISTICS_LINEAR:       tf = TF_LINEAR; break;
                          case AVIF_TRANSFER_CHARACTERISTICS_SRGB:         tf = TF_SRGB; break;
                          case AVIF_TRANSFER_CHARACTERISTICS_BT601:        tf = TF_BT601; break;
                          case AVIF_TRANSFER_CHARACTERISTICS_BT709:        tf = TF_BT709; break;
                          case AVIF_TRANSFER_CHARACTERISTICS_BT2020_10BIT: tf = TF_BT2020; break;
                          case AVIF_TRANSFER_CHARACTERISTICS_BT2020_12BIT: tf = TF_BT2020; break;
                          case AVIF_TRANSFER_CHARACTERISTICS_PQ:           tf = TF_PQ; break;
                          case AVIF_TRANSFER_CHARACTERISTICS_HLG:          tf = TF_HLG; break;
                        }

                        float maxCLL = decoder->image->clli.maxCLL;
                        if (maxCLL == 0.0f) maxCLL = 80.0f;

                        outResult->dst = (unsigned int *)rgb.pixels;
                        outResult->dst_len = rgb.rowBytes * rgb.height;
                        outResult->width = decoder->image->width;
                        outResult->height = decoder->image->height;
                        outResult->numFrames = 1; //FIXME
                        outResult->flag = ((decoder->image->alphaPremultiplied?1:0)+
                                           (decoder->image->gainMap?4:0));
                        if (channel == 3){
                            outResult->format = FMT_RGB16F;
                        }else{
                            outResult->format = FMT_RGBA16F;
                        }
                        outResult->colorspace = cs | tf;
                        outResult->param1 = (uint32_t)decoder->image->clli.maxCLL;

                        std::string format_info = std::format("Avif ({})", avifPixelFormatToString(decoder->image->yuvFormat));
                        const std::string::size_type fsize = format_info.size();
                        outResult->format_string = (uint32_t *)new char[fsize + 1];
                        memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

                        const std::string::size_type size = loader_info.size();
                        outResult->loader_string = (uint32_t *)new char[size + 1];
                        memcpy(outResult->loader_string, loader_info.c_str(), size + 1);

                        errCode = ERR_OK;
                        break;
                    }else{
                        free(rgb.pixels);
                    }
                }
            }
        }else{
            errCode = ERR_NOTFOUND;
        }
        avifDecoderDestroy(decoder);
        free(data);
        return errCode;
    }

    //------------------------------------------------------------------------------
    AVIFLOADERLIB_API void free_mem(LOAD_RESULT *result)
    {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }
}
