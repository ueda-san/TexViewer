//#include "pch.h"

#include <string>
#include <format>

#include "ASTCLoaderLib.h"
#include "image_load.h"

#include <windows.h>
#include <stringapiset.h>


int scanBlock(unsigned char *data, size_t len)
{
    unsigned char *p = data;
    bool hasHDR = false;
    bool hasAlpha = false;

    //int cemCount[16];
    //for (int i=0; i<16; i++) cemCount[i] = 0;

    while (len >= 16){
        int blockMode = (p[0] | (p[1]<<8)) & 0x07ff;
        if ((blockMode & 0x01ff) != 0x01fc){ // !void-extent block
            if (blockMode & 0x0400){ // dual-plane mode
                // TODO
            }else{
                int partitionNum = ((p[1]&0x18)>>3)+1;
                if (partitionNum == 1){
                    int cem = ((p[1] | (p[2]<<8))&0x01e0)>>5; // Color Endpoint Mode
                    if (cem == 2 || cem == 3 || cem == 7 || cem == 11 || cem == 14 || cem == 15){
                        hasHDR = true;
                    }
                    if (cem == 4 || cem == 5 || cem == 12 || cem == 13 || cem == 14 || cem == 15){
                        hasAlpha = true;
                    }
                    //cemCount[cem]++;
                }else{
                    // TODO
                }

            }
        }
        p += 16;
        len -= 16;
    }

    return (hasHDR?1:0) + (hasAlpha?2:0);
}

static const char *cemStr[] = {
    "0 LDR Luminance, direct",
    "1 LDR Luminance, base+offset",
    "2 HDR Luminance, large range",
    "3 HDR Luminance, small range",
    "4 LDR Luminance+Alpha, direct",
    "5 LDR Luminance+Alpha, base+offset",
    "6 LDR RGB, base+scale",
    "7 HDR RGB, base+scale",
    "8 LDR RGB, direct",
    "9 LDR RGB, base+offset",
    "10 LDR RGB, base+scale plus two A",
    "11 HDR RGB, direct",
    "12 LDR RGBA, direct",
    "13 LDR RGBA, base+offset",
    "14 HDR RGB, direct + LDR Alpha",
    "15 HDR RGB, direct + HDR Alpha",
};



extern "C" {
    //------------------------------------------------------------------------------
    ASTCLOADERLIB_API const char *get_version()
    {
        static const char *buf = "5.3.0";
        return buf;
    }

    //------------------------------------------------------------------------------
    ASTCLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult)
    {
        int err = 1;
        std::string loader_info="";
        astc_compressed_image image_comp {};
        std::wstring_view fileView(file, std::wcslen(file));
        //int out_bitness = 8;
        if (fileView.ends_with(L".astc") || fileView.ends_with(L".ASTC")) {
            err = load_cimage(file, image_comp);
            loader_info += "Type: ASTC\n";
            outResult->colorspace = CS_UNKNOWN;
        }else if (fileView.ends_with(L".ktx") || fileView.ends_with(L".KTX")) {
            bool is_srgb;
            std::string info;
            err = load_ktx_compressed_image(file, is_srgb, image_comp, info);
            loader_info += "Type: KTX\n";
            loader_info += std::format("srgb: {}\n", is_srgb?"true":"false");
            loader_info += info;
            outResult->colorspace = is_srgb?CS_SRGB:CS_LINEAR;
        }

        int scanResult = scanBlock(image_comp.data, image_comp.data_len);

        if (err != 0) return ERR_UNSUPPORTED;

        loader_info += std::format("Block: {}x{}x{}\n",
                              image_comp.block_x, image_comp.block_y,image_comp.block_z);
        loader_info += std::format("Dimension: {}x{}x{}\n",
                              image_comp.dim_x, image_comp.dim_y,image_comp.dim_z);

        int out_bitness = (scanResult&1)?16:8; // 8,16,32
        astcenc_image* img = alloc_image(out_bitness, image_comp.dim_x, image_comp.dim_y, image_comp.dim_z);

        astcenc_config config {};
        unsigned int flags = ASTCENC_FLG_DECOMPRESS_ONLY;
        astcenc_profile profile = ASTCENC_PRF_HDR_RGB_LDR_A; // LDR/LDR_SRGB/HDR_RGB_LDR_A/HDR
        astcenc_error status = astcenc_config_init(profile, image_comp.block_x, image_comp.block_y, image_comp.block_z, ASTCENC_PRE_MEDIUM, flags, &config);
        if (status == ASTCENC_ERR_BAD_BLOCK_SIZE) {
            //print_error("ERROR: Block size '%s' is invalid\n", argv[4]);
            return ERR_UNSUPPORTED;
        }else if (status == ASTCENC_ERR_BAD_DECODE_MODE){
            //print_error("ERROR: Decode_unorm8 is not supported by HDR profiles\n", argv[4]);
            return ERR_UNSUPPORTED;
        }else if (status == ASTCENC_ERR_BAD_CPU_FLOAT) {
            //print_error("ERROR: astcenc must not be compiled with -ffast-math\n");
            return ERR_UNSUPPORTED;
        }else if (status != ASTCENC_SUCCESS){
            //print_error("ERROR: Init config failed with %s\n", astcenc_get_error_string(status));
            return ERR_UNSUPPORTED;
        }

        astcenc_context* codec_context;
        astcenc_error codec_status = astcenc_context_alloc(&config, 1, &codec_context);
        astcenc_swizzle swizzle = astcenc_swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
        astcenc_error error = astcenc_decompress_image(codec_context, image_comp.data, image_comp.data_len, img, &swizzle, 0);
        astcenc_decompress_reset(codec_context);
        if (error != ASTCENC_SUCCESS) {
            //print_error("ERROR: Codec decompress failed: %s\n", astcenc_get_error_string(codec_status));
            return ERR_UNSUPPORTED;
        }


        int stride;
        if (img->data_type == ASTCENC_TYPE_U8){
            stride = img->dim_x * 4;
            outResult->format = FMT_RGBA8888;
        }else if (img->data_type == ASTCENC_TYPE_F16){
            stride = img->dim_x * 8;
            outResult->format = FMT_RGBA16F;
        }else{
            stride = img->dim_x * 16;
            outResult->format = FMT_RGBA32F;
        }
        int page = (img->dim_z == 0)?1:img->dim_z;
        size_t page_size = stride * img->dim_y;
        size_t dst_size = page_size * page;
        unsigned char *dst = (unsigned char *)malloc(dst_size);
        if (dst==0) return ERR_NOMEM;

        for (int z=0; z<page; z++){
            memcpy(dst+page_size*z, img->data[z], page_size);
        }

        outResult->dst = (unsigned int *)dst;
        outResult->dst_len = (uint32_t)dst_size;

        outResult->width = img->dim_x;
        outResult->height = img->dim_y;
        outResult->numFrames = page;
        outResult->flag = 0;

        std::string format_info;
        if (image_comp.block_z >= 2){
            format_info = std::format("ASTC{}x{}x{}", image_comp.block_x, image_comp.block_y, image_comp.block_z);
        }else{
            format_info = std::format("ASTC{}x{}", image_comp.block_x, image_comp.block_y);
        }
        const std::string::size_type fsize = format_info.size();
        outResult->format_string = (uint32_t *)new char[fsize + 1];
        memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

        const std::string::size_type size = loader_info.size();
        outResult->loader_string = (uint32_t *)new char[size + 1];
        memcpy(outResult->loader_string, loader_info.c_str(), size + 1);

        free_image(img);
        astcenc_context_free(codec_context);
        delete[] image_comp.data;
        return ERR_OK;
    }

    //------------------------------------------------------------------------------
    ASTCLOADERLIB_API void free_mem(LOAD_RESULT *result)
    {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }

}
