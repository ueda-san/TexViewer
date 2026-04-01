#include <string>
#include <format>
#include <memory>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

#include "UltraHDRLoaderLib.h"
#include "ultrahdr_api.h"
#include "EXIFParser.h"

using namespace EXIFParser;

std::string colorGamutString(uhdr_color_gamut_t val) {
    switch(val){
      case UHDR_CG_UNSPECIFIED: return "UNSPECIFIED"; break;
      case UHDR_CG_BT_709:      return "BT_709";      break;
      case UHDR_CG_DISPLAY_P3:  return "DISPLAY_P3";  break;
      case UHDR_CG_BT_2100:     return "BT_2100";     break;
      default: break;
    }
    return "Unknown";
}

std::string colorTransferString(uhdr_color_transfer_t val) {
    switch(val){
      case UHDR_CT_UNSPECIFIED: return "UNSPECIFIED"; break;
      case UHDR_CT_LINEAR:      return "LINEAR";      break;
      case UHDR_CT_HLG:         return "HLG";         break;
      case UHDR_CT_PQ:          return "PQ";          break;
      case UHDR_CT_SRGB:        return "SRGB";        break;
      default: break;
    }
    return "Unknown";
}

std::string colorRangeString(uhdr_color_range_t val) {
    switch(val){
      case UHDR_CR_UNSPECIFIED:   return "UNSPECIFIED";   break;
      case UHDR_CR_LIMITED_RANGE: return "LIMITED_RANGE"; break;
      case UHDR_CR_FULL_RANGE:    return "FULL_RANGE";    break;
      default: break;
    }
    return "Unknown";
}


extern "C" {
    ULTRAHDRLOADERLIB_API const char* get_version()
    {
        static const char *buf = UHDR_LIB_VERSION_STR;
        return buf;
    }

    //------------------------------------------------------------------------------
#define RET_IF_ERR(x) {                                          \
        uhdr_error_info_t status = (x);                          \
        if (status.error_code != UHDR_CODEC_OK) {                \
            uhdr_release_decoder(handle);                        \
            return ERR_UNKNOWN;                                  \
        }                                                        \
    }


    // currently there is no plan to add full support for 3-channel gainmap with XML metadata.
    // https://github.com/google/libultrahdr/issues/116

    ULTRAHDRLOADERLIB_API int load_byte(const unsigned char *data, int size, LOAD_RESULT* outResult, uint32_t opt)
    {
        std::string loader_info = "";

        uhdr_compressed_image_t mUhdrImage{};
        mUhdrImage.capacity = size;
        mUhdrImage.data_sz = size;
        mUhdrImage.data = (void *)data;
        mUhdrImage.cg = UHDR_CG_UNSPECIFIED;
        mUhdrImage.ct = UHDR_CT_UNSPECIFIED;
        mUhdrImage.range = UHDR_CR_UNSPECIFIED;

        uhdr_codec_private_t* handle = uhdr_create_decoder();
        RET_IF_ERR(uhdr_dec_set_image(handle, &mUhdrImage))
        //RET_IF_ERR(uhdr_dec_set_out_color_transfer(handle, UHDR_CT_LINEAR))
        //RET_IF_ERR(uhdr_dec_set_out_img_format(handle, UHDR_IMG_FMT_64bppRGBAHalfFloat))
        if ((opt&0x0002)==0){ // requestSDR
            RET_IF_ERR(uhdr_dec_set_out_color_transfer(handle, UHDR_CT_PQ))
            RET_IF_ERR(uhdr_dec_set_out_img_format(handle, UHDR_IMG_FMT_32bppRGBA1010102))
        }else{
            RET_IF_ERR(uhdr_dec_set_out_color_transfer(handle, UHDR_CT_SRGB))
            RET_IF_ERR(uhdr_dec_set_out_img_format(handle, UHDR_IMG_FMT_32bppRGBA8888))
        }
        RET_IF_ERR(uhdr_dec_probe(handle))
        RET_IF_ERR(uhdr_decode(handle))

        uhdr_raw_image_t* output = uhdr_get_decoded_image(handle);

        loader_info += std::format("width: {}\n", output->w);
        loader_info += std::format("height: {}\n", output->h);
        int gw = uhdr_dec_get_gainmap_width(handle);
        int gh = uhdr_dec_get_gainmap_height(handle);
        loader_info += std::format("gainmap size: {}x{}\n", gw, gh);

        loader_info += std::format("color gamut: {}\n", colorGamutString(output->cg));
        loader_info += std::format("color transfer: {}\n", colorTransferString(output->ct));
        loader_info += std::format("color range: {}\n", colorRangeString(output->range));

        size_t bpp = (output->fmt == UHDR_IMG_FMT_64bppRGBAHalfFloat) ? 8 : 4;
        size_t dst_size = bpp * output->w * output->h;
        unsigned char *dst = (unsigned char *)malloc(dst_size);

        char *inData = static_cast<char*>(output->planes[UHDR_PLANE_PACKED]);
        unsigned char *outData = dst;
        const size_t inStride = output->stride[UHDR_PLANE_PACKED] * bpp;
        const size_t outStride = output->w * bpp;
        const size_t length = output->w * bpp;
        for (unsigned i = 0; i < output->h; i++, inData += inStride, outData += outStride) {
            memcpy(outData, inData, length);
        }

        if (output->fmt == UHDR_IMG_FMT_64bppRGBAHalfFloat){
            outResult->format = FMT_RGBA16F;
        }else if (output->fmt == UHDR_IMG_FMT_32bppRGBA8888){
            outResult->format = FMT_RGBA8888;
        }else if (output->fmt == UHDR_IMG_FMT_32bppRGBA1010102){
            outResult->format = FMT_RGBA1010102;
        }
        outResult->flag = (gw != 0 && gh != 0)?4:0;
        outResult->dst = (unsigned int *)dst;
        outResult->dst_len = (unsigned int)dst_size;
        outResult->width = output->w;
        outResult->height = output->h;
        outResult->numFrames = 1;

        int tf;
        if (output->ct == UHDR_CT_LINEAR){
            tf = TF_LINEAR;
        }else if (output->ct == UHDR_CT_HLG){
            tf = TF_HLG;
        }else if (output->ct == UHDR_CT_PQ){
            tf = TF_PQ;
        }else if (output->ct == UHDR_CT_SRGB){
            tf = TF_SRGB;
        }else{
            tf = TF_DEFAULT;
        }

        int cs;
        if (output->cg == UHDR_CG_BT_709){
            cs = CS_BT709;
        }else if (output->cg == UHDR_CG_DISPLAY_P3){
            cs = CS_DISPLAY_P3;
        }else if (output->cg == UHDR_CG_BT_2100){
            cs = CS_BT2020;  //
        }else{
            cs = CS_UNKNOWN;
        }
        outResult->colorspace = cs | tf;

        ExifParser parser = ExifParser((opt&0x0001)?0:ExifParser::getSimpleTagOfInterest());
        if (parser.parseJpeg(data, (size_t)size, IFD_Exif) > 0){
            std::string exif_info = parser.dumpExif();
            if (exif_info != "") loader_info += "\nEXIF:\n" + exif_info;
        }

        std::string format_info = "jpeg with gainmap";
        const std::string::size_type fsize = format_info.size();
        outResult->format_string = (uint32_t *)new char[fsize + 1];
        memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

        const std::string::size_type lsize = loader_info.size();
        outResult->loader_string = (uint32_t *)new char[lsize + 1];
        memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);

        uhdr_release_decoder(handle);

        return ERR_OK;
    }

    //------------------------------------------------------------------------------
    ULTRAHDRLOADERLIB_API void free_mem(LOAD_RESULT* result) {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }
}
