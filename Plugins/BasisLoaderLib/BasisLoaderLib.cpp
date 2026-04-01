//#include "pch.h"

#include "transcoder/basisu_transcoder.h"

#include "BASISLoaderLib.h"

#include <string>

using namespace basist;


const char *tex_type[] = {
    "2D",
    "2DArray",
    "CubemapArray",
    "VideoFrames",
    "Volume",
};
const char *tex_format[] = { // from basisu_file_headers.h:basis_tex_format
	"ETC1S",
	"UASTC_LDR_4x4",
	"UASTC_HDR_4x4",
	"ASTC_HDR_6x6",
	"UASTC_HDR_6x6", // TODO: rename to UASTC_HDR_6x6
	"XUASTC_LDR_4x4",
	"XUASTC_LDR_5x4",
	"XUASTC_LDR_5x5",
	"XUASTC_LDR_6x5",
	"XUASTC_LDR_6x6",
	"XUASTC_LDR_8x5",
	"UASTC_LDR_8x6",
	"XUASTC_LDR_10x5",
	"XUASTC_LDR_10x6",
	"XUASTC_LDR_8x8",
	"XUASTC_LDR_10x8",
	"XUASTC_LDR_10x10",
	"XUASTC_LDR_12x10",
	"XUASTC_LDR_12x12",
	"ASTC_LDR_4x4",
	"ASTC_LDR_5x4",
	"ASTC_LDR_5x5",
	"ASTC_LDR_6x5",
	"ASTC_LDR_6x6",
	"ASTC_LDR_8x5",
	"ASTC_LDR_8x6",
	"ASTC_LDR_10x5",
	"ASTC_LDR_10x6",
	"ASTC_LDR_8x8",
	"ASTC_LDR_10x8",
	"ASTC_LDR_10x10",
	"ASTC_LDR_12x10",
	"ASTC_LDR_12x12",
};


extern "C" {

    //------------------------------------------------------------------------------
    BASISLOADERLIB_API const char *get_version()
    {
        static const char *buf = BASISD_VERSION_STRING;
        return buf;
    }

    //------------------------------------------------------------------------------
    void setResult(LOAD_RESULT *outResult, basis_tex_format fmt, std::string info) {
        std::string format_info = std::string(tex_format[(int)fmt]);
        const std::string::size_type fsize = format_info.size();
        outResult->format_string = (uint32_t *)new char[fsize + 1];
        memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

        const std::string::size_type size = info.size();
        outResult->loader_string = (uint32_t *)new char[size + 1];
        memcpy(outResult->loader_string, info.c_str(), size + 1);
    }

    BASISLOADERLIB_API int load_byte(const unsigned char *data, int size, LOAD_RESULT *outResult)
    {
        basisu_transcoder_init();

        int index = 0;
        basisu_transcoder transcoder;
        std::string loader_info="";

        uint32_t dstSize;
        unsigned char *dst;
        uint32_t num;

        if (transcoder.validate_header(data, size)){
            loader_info += "Type: Basis";
            basisu_file_info fileInfo;
            if (!transcoder.get_file_info(data, size, fileInfo)) return ERR_UNKNOWN;
            //loader_info += std::format("version: {}\n", fileInfo.m_version);
            loader_info += "\nversion: "+std::to_string(fileInfo.m_version);
            loader_info += "\ncodec: "+std::string(fileInfo.m_etc1s?"ETC1S":"UASTC");
            loader_info += "\ntexture_type: "+std::string(tex_type[fileInfo.m_tex_type]);
            loader_info += "\ntexture_format: "+std::string(tex_format[(int)fileInfo.m_tex_format]);
            loader_info += "\ny_flipped: "+std::string(fileInfo.m_y_flipped?"true":"false");
            //loader_info += std::format("has_alpha_slices: {}\n", fileInfo.m_has_alpha_slices?"true":"false");

            num = transcoder.get_total_images(data, size);
            basisu_image_info info;
            if (!transcoder.get_image_info(data, size, info, index)) return ERR_UNKNOWN;

            loader_info += "\ntotal_images: "+std::to_string(num);
            loader_info += "\norig_width: "+std::to_string(info.m_orig_width);
            loader_info += "\norig_height: "+std::to_string(info.m_orig_height);
            loader_info += "\nwidth: "+std::to_string(info.m_width);
            loader_info += "\nheight: "+std::to_string(info.m_height);
            loader_info += "\nblock_width: "+std::to_string(info.m_block_width);
            loader_info += "\nblock_height: "+std::to_string(info.m_block_height);
            loader_info += "\nnum_blocks_x: "+std::to_string(info.m_num_blocks_x);
            loader_info += "\nnum_blocks_y: "+std::to_string(info.m_num_blocks_y);
            loader_info += "\nhas_alpha: "+std::string(info.m_alpha_flag?"true":"false");
            loader_info += "\nis_iframe: "+std::string(info.m_iframe_flag?"true":"false");
            loader_info += "\n";

            if (basis_tex_format_is_hdr(fileInfo.m_tex_format)){
                loader_info += "is_hdr: true\n";
                if (!transcoder.start_transcoding(data, size)) return ERR_UNKNOWN;
                dstSize = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA_HALF) * info.m_orig_width * info.m_orig_height;
                dst = (unsigned char *)malloc(dstSize * num);
                if (dst==0) return ERR_NOMEM;

                for (uint32_t i=0; i< num; i++){
                    unsigned char *p = dst+i*dstSize;
                    if (transcoder.transcode_image_level(data, size, i, 0, p, info.m_orig_width * info.m_orig_height, transcoder_texture_format::cTFRGBA_HALF)){
                        if (fileInfo.m_y_flipped){
                            int stride = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA_HALF) * info.m_orig_width;
                            unsigned char *buf = (unsigned char *)malloc(stride);
                            unsigned int h = info.m_orig_height;
                            for (unsigned int y=0; y<h/2; y++){
                                memcpy_s(buf, stride, p+y*stride, stride);
                                memcpy_s(p+y*stride, stride, p+(h-y-1)*stride, stride);
                                memcpy_s(p+(h-y-1)*stride, stride, buf, stride);
                            }
                            free(buf);
                        }
                    }else{
                        free(dst);
                        return ERR_UNSUPPORTED;
                    }
                }
                outResult->format = FMT_RGBA16F;
            }else{
                loader_info += "is_hdr: false\n";
                if (!transcoder.start_transcoding(data, size)) return ERR_UNKNOWN;
                dstSize = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA32) * info.m_orig_width * info.m_orig_height;
                dst = (unsigned char *)malloc(dstSize * num);
                if (dst==0) return ERR_NOMEM;
                for (uint32_t i=0; i< num; i++){
                    unsigned char *p = dst+i*dstSize;
                    if (transcoder.transcode_image_level(data, size, index, 0, p, info.m_orig_width * info.m_orig_height, transcoder_texture_format::cTFRGBA32)){
                        if (fileInfo.m_y_flipped){
                            int stride = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA32) * info.m_orig_width;
                            unsigned char *buf = (unsigned char *)malloc(stride);
                            unsigned int h = info.m_orig_height;
                            for (unsigned int y=0; y<h/2; y++){
                                memcpy_s(buf, stride, p+y*stride, stride);
                                memcpy_s(p+y*stride, stride, p+(h-y-1)*stride, stride);
                                memcpy_s(p+(h-y-1)*stride, stride, buf, stride);
                            }
                            free(buf);
                        }
                    }else{
                        free(dst);
                        return ERR_UNSUPPORTED;
                    }
                }
                outResult->format = FMT_RGBA8888;
            }
            outResult->colorspace = fileInfo.m_etc1s?CS_SRGB:CS_LINEAR;

            outResult->dst = (unsigned int *)dst;
            outResult->dst_len = dstSize * num;
            outResult->width = info.m_orig_width; //info.m_width;
            outResult->height = info.m_orig_height; //info.m_height;
            outResult->numFrames = num;
            outResult->flag = 0;
            setResult(outResult, fileInfo.m_tex_format, loader_info);
            return ERR_OK;

        }else{
            loader_info += "Type: Ktx2";

            ktx2_transcoder ktx2transcoder;
            if (ktx2transcoder.init(data, size)){
                const char *transfer_func[] = {
                    "unknown",
                    "LINEAR",
                    "SRGB",
                };
                loader_info += "\nwidth: "+std::to_string(ktx2transcoder.get_width());
                loader_info += "\nheight: "+std::to_string(ktx2transcoder.get_height());
                loader_info += "\nlevels: "+std::to_string(ktx2transcoder.get_levels());
                loader_info += "\nfaces: "+std::to_string(ktx2transcoder.get_faces());
                loader_info += "\nlayers: "+std::to_string(ktx2transcoder.get_layers());
                loader_info += "\nformat: "+std::string(tex_format[(int)ktx2transcoder.get_basis_tex_format()]);
                loader_info += "\nis_hdr: "+std::string(ktx2transcoder.is_hdr()?"true":"false");
                loader_info += "\nblock_width: "+std::to_string(ktx2transcoder.get_block_width());
                loader_info += "\nblock_height: "+std::to_string(ktx2transcoder.get_block_height());
                loader_info += "\nhas_alpha: "+std::string(ktx2transcoder.get_has_alpha()?"true":"false");
                loader_info += "\ndfd_color_model: "+std::to_string(ktx2transcoder.get_dfd_color_model());
                loader_info += "\ndfd_color_primaries: "+std::string(ktx2_get_df_color_primaries_str(ktx2transcoder.get_dfd_color_primaries()));
                loader_info += "\ndfd_color_transfer_func: "+std::string(transfer_func[ktx2transcoder.get_dfd_transfer_func()]);
                loader_info += "\ndfd_flags: "+std::to_string(ktx2transcoder.get_dfd_flags());
                if (ktx2transcoder.get_basis_tex_format() == basis_tex_format::cETC1S){
                    loader_info += "\ndfd_channel_id0: "+std::string(ktx2_get_etc1s_df_channel_id_str(ktx2transcoder.get_dfd_channel_id0()));
                    loader_info += "\ndfd_channel_id1: "+std::string(ktx2_get_etc1s_df_channel_id_str(ktx2transcoder.get_dfd_channel_id1()));
                }else{
                    loader_info += "\ndfd_channel_id0: "+std::string(ktx2_get_uastc_df_channel_id_str(ktx2transcoder.get_dfd_channel_id0()));
                    loader_info += "\ndfd_channel_id1: "+std::string(ktx2_get_uastc_df_channel_id_str(ktx2transcoder.get_dfd_channel_id1()));
                }
                loader_info += "\nnit_multiplier: "+std::to_string(ktx2transcoder.get_ldr_hdr_upconversion_nit_multiplier());
                const ktx2_transcoder::key_value_vec keyValueVec = ktx2transcoder.get_key_values();
                bool yflip = false;
                if (!keyValueVec.empty()) {
                    loader_info += "\nKey Value Data:";
                    for(ktx2_transcoder::key_value kv : keyValueVec){
                        std::string key = std::string(kv.m_key.begin(), kv.m_key.end());
                        key.erase(std::find(key.begin(), key.end(), '\0'), key.end());
                        std::string value = std::string(kv.m_value.begin(), kv.m_value.end());
                        value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                        loader_info += "\n  "+key+" : "+value;
                        if (key=="KTXorientation" && value=="ru") yflip = true;
                    }
                }
                loader_info += "\n";

                num = ktx2transcoder.get_faces();
                const uint32_t width = ktx2transcoder.get_width();
                const uint32_t height = ktx2transcoder.get_height();
                if (ktx2transcoder.is_hdr()){
                    if (!ktx2transcoder.start_transcoding()) return ERR_UNKNOWN;
                    dstSize = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA_HALF) * width * height;
                    dst = (unsigned char *)malloc(dstSize * num);
                    if (dst==0) return ERR_NOMEM;
                    for (uint32_t face=0; face< num; face++){
                        unsigned char *p = dst+face*dstSize;
                        if (!ktx2transcoder.transcode_image_level(index, 0, face, p, width * height, transcoder_texture_format::cTFRGBA_HALF)){
                            free(dst);
                            return ERR_UNSUPPORTED;
                        }
                        if (yflip){
                            int stride = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA_HALF) * width;
                            unsigned char *buf = (unsigned char *)malloc(stride);
                            unsigned int h = height;
                            for (unsigned int y=0; y<h/2; y++){
                                memcpy_s(buf, stride, p+y*stride, stride);
                                memcpy_s(p+y*stride, stride, p+(h-y-1)*stride, stride);
                                memcpy_s(p+(h-y-1)*stride, stride, buf, stride);
                            }
                            free(buf);
                        }
                    }
                    outResult->format = FMT_RGBA16F;
                }else{
                    if (!ktx2transcoder.start_transcoding()) return ERR_UNKNOWN;
                    dstSize = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA32) * width * height;
                    dst = (unsigned char *)malloc(dstSize);
                    if (dst==0) return ERR_NOMEM;
                    for (uint32_t face=0; face< num; face++){
                        unsigned char *p = dst+face*dstSize;
                        if (!ktx2transcoder.transcode_image_level(index, 0, face, p, width * height, transcoder_texture_format::cTFRGBA32)){
                            free(dst);
                            return ERR_UNSUPPORTED;
                        }
                        if (yflip){
                            int stride = basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format::cTFRGBA32) * width;
                            unsigned char *buf = (unsigned char *)malloc(stride);
                            unsigned int h = height;
                            for (unsigned int y=0; y<h/2; y++){
                                memcpy_s(buf, stride, p+y*stride, stride);
                                memcpy_s(p+y*stride, stride, p+(h-y-1)*stride, stride);
                                memcpy_s(p+(h-y-1)*stride, stride, buf, stride);
                            }
                            free(buf);
                        }
                        outResult->format = FMT_RGBA8888;
                    }
                }

                int tf = TF_DEFAULT;
                switch(ktx2transcoder.get_dfd_transfer_func()){
                  case KTX2_KHR_DF_TRANSFER_LINEAR: tf = TF_LINEAR; break;
                  case KTX2_KHR_DF_TRANSFER_SRGB:   tf = TF_SRGB;   break;
                  default:
                    break;
                }
                int cs = CS_UNKNOWN;
                switch(ktx2transcoder.get_dfd_color_primaries()){
                  case KTX2_DF_PRIMARIES_BT709:        cs = CS_BT709;      break; //same as SRGB
                  case KTX2_DF_PRIMARIES_BT601_EBU:    cs = CS_BT601;      break;
                  case KTX2_DF_PRIMARIES_BT601_SMPTE:  cs = CS_BT601;      break;
                  case KTX2_DF_PRIMARIES_BT2020:       cs = CS_BT2020;     break;
                  case KTX2_DF_PRIMARIES_DISPLAYP3:    cs = CS_DISPLAY_P3; break;
                  case KTX2_DF_PRIMARIES_ADOBERGB:     cs = CS_ADOBE98;    break;

                  case KTX2_DF_PRIMARIES_ACES:     // ACES AP0 原色
                  case KTX2_DF_PRIMARIES_ACESCC:   // ACEScc / AP1 原色
                  case KTX2_DF_PRIMARIES_NTSC1953: // 初期 NTSC。Rec.709 とは緑が違う
                  case KTX2_DF_PRIMARIES_PAL525:   // BT.601 EBU 系に近いらしい？
                  default:
                    break;
                }

                outResult->colorspace = cs | tf;
                outResult->dst = (unsigned int *)dst;
                outResult->dst_len = dstSize * num;
                outResult->width = width;
                outResult->height = height;
                outResult->numFrames = num;
                outResult->flag = 0;
                setResult(outResult, ktx2transcoder.get_basis_tex_format(), loader_info);
                return ERR_OK;
            }
        }
        return ERR_UNSUPPORTED;
    }


    //------------------------------------------------------------------------------
    BASISLOADERLIB_API void free_mem(LOAD_RESULT *result)
    {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }

}
