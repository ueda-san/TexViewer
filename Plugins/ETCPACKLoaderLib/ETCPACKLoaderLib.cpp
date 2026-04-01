#include "pch.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

#include "ETCPACKLoaderLib.h"
#include "etcdec.h"
#include "glFormatStr.h"
#include <cassert>

int read4byte(const unsigned char **pp){
    const unsigned char *p = *pp;
    int ret = (*(p+0)<<24) + (*(p+1)<<16) + (*(p+2)<<8) + (*(p+3));
    *pp = p+4;
    return ret;
}

void copy8byte(const unsigned char **pp, unsigned char *dst){
    const unsigned char *p = *pp;
    memcpy(dst, p, 8);
    *pp = p+8;
}


extern "C" {
    //------------------------------------------------------------------------------
    ETCPACKLOADERLIB_API const char *get_version()
    {
        static const char *buf = "2.74";
        return buf;
    }

    //------------------------------------------------------------------------------
    ETCPACKLOADERLIB_API int load_byte(const unsigned char *data, int size, LOAD_RESULT *outResult)
    {
        const unsigned char *top = data;
        bool ktx;
        if (top[0]=='P' && top[1] == 'K' && top[2] == 'M' && top[3] == ' '){
            ktx = false;
        }else if (top[1]=='K' && top[2] == 'T' && top[3] == 'X' && top[4] == ' '){
            ktx = true;
        }else{
            return ERR_UNSUPPORTED;
        }

        int width,height;
        int active_width, active_height;
        unsigned int block_part1, block_part2;
        uint8 *img=0, *alphaimg=0, *alphaimg2=0;
        unsigned short texture_type;
        int format = ETC2PACKAGE_RGB_NO_MIPMAPS;
        int codec;

        const unsigned char *p;
        //readCompressParams();

        std::string texTypeStr[] = {
            "ETC1_RGB_NO_MIPMAPS",
            "ETC2PACKAGE_RGB_NO_MIPMAPS",
            "ETC2PACKAGE_RGBA_NO_MIPMAPS_OLD",
            "ETC2PACKAGE_RGBA_NO_MIPMAPS",
            "ETC2PACKAGE_RGBA1_NO_MIPMAPS",
            "ETC2PACKAGE_R_NO_MIPMAPS",
            "ETC2PACKAGE_RG_NO_MIPMAPS",
            "ETC2PACKAGE_R_SIGNED_NO_MIPMAPS",
            "ETC2PACKAGE_RG_SIGNED_NO_MIPMAPS",
            "ETC2PACKAGE_sRGB_NO_MIPMAPS",
            "ETC2PACKAGE_sRGBA_NO_MIPMAPS",
            "ETC2PACKAGE_sRGBA1_NO_MIPMAPS"
        };
        std::string loader_info="";

        int cs = CS_UNKNOWN;
        formatSigned=0;

        if (ktx){

            KTX_header *header = (KTX_header *)top;
            if (header->bytesOfKeyValueData != 0) return ERR_NOTIMPLEMENTED; // keyValuePair
            p = top+sizeof(KTX_header)+4;

            active_width = header->pixelWidth;
            active_height = header->pixelHeight;
            width = ((active_width+3)/4)*4;
            height = ((active_height+3)/4)*4;

            loader_info += "Type: KTX\n";
            loader_info += std::format("width: {}\n", width);
            loader_info += std::format("height: {}\n", height);
            loader_info += std::format("pixel_width: {}\n", header->pixelWidth);
            loader_info += std::format("pixel_height: {}\n", header->pixelHeight);
            loader_info += std::format("pixel_depth: {}\n", header->pixelDepth);

            loader_info += std::format("gl_type: {}\n", header->glType);
            loader_info += std::format("gl_format: {}\n", glFormatStr(header->glFormat));
            loader_info += std::format("gl_internal_format: {}\n", glFormatStr(header->glInternalFormat));
            loader_info += std::format("gl_base_internal_format: {}\n", glFormatStr(header->glBaseInternalFormat));
            loader_info += std::format("number_of_array_elements: {}\n", header->numberOfArrayElements);
            loader_info += std::format("number_of_faces: {}\n", header->numberOfFaces);
            loader_info += std::format("number_of_mipmap_levels: {}\n", header->numberOfMipmapLevels);

            if(header->glInternalFormat==GL_COMPRESSED_SIGNED_R11_EAC){
                format=ETC2PACKAGE_R_NO_MIPMAPS;
                formatSigned=1;
            }else if(header->glInternalFormat==GL_COMPRESSED_R11_EAC){
                format=ETC2PACKAGE_R_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_COMPRESSED_SIGNED_RG11_EAC){
                format=ETC2PACKAGE_RG_NO_MIPMAPS;
                formatSigned=1;
            }else if(header->glInternalFormat==GL_COMPRESSED_RG11_EAC){
                format=ETC2PACKAGE_RG_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_COMPRESSED_RGB8_ETC2){
                format=ETC2PACKAGE_RGB_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_COMPRESSED_SRGB8_ETC2){
                format=ETC2PACKAGE_sRGB_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_COMPRESSED_RGBA8_ETC2_EAC){
                format=ETC2PACKAGE_RGBA_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC){
                format=ETC2PACKAGE_sRGBA_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2){
                format=ETC2PACKAGE_RGBA1_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2){
                format=ETC2PACKAGE_sRGBA1_NO_MIPMAPS;
            }else if(header->glInternalFormat==GL_ETC1_RGB8_OES){
                format=ETC1_RGB_NO_MIPMAPS;
                codec=CODEC_ETC;
            }else{
                return ERR_UNSUPPORTED;
            }
            cs = isSRGB(header->glFormat);

        }else{
            p = top+16;
            if (top[4]=='1' && top[5]=='0'){ // version  1.0
                loader_info += "Type: PKM 1.0\n";
/*
4 byte magic number: "PKM "
2 byte version "10"
2 byte data type: 0 (ETC1_RGB_NO_MIPMAPS)
16 bit big endian extended width
16 bit big endian extended height
16 bit big endian original width
16 bit big endian original height
data, 64bit big endian words.
*/
                texture_type = top[6]*256+top[7];
                loader_info += std::format("texture_type: {}\n", texTypeStr[texture_type]);
                if(texture_type != ETC1_RGB_NO_MIPMAPS) return ERR_UNSUPPORTED;
            }else if (top[4]=='2' && top[5]=='0'){ // version  2.0
                loader_info += "Type: PKM 2.0\n";
                texture_type = top[6]*256+top[7];
                loader_info += std::format("orig texture_type: {}\n", texTypeStr[texture_type]);
                if(texture_type==ETC2PACKAGE_RG_SIGNED_NO_MIPMAPS){
                    texture_type=ETC2PACKAGE_RG_NO_MIPMAPS;
                    formatSigned=1;
                }
                if(texture_type==ETC2PACKAGE_R_SIGNED_NO_MIPMAPS){
                    texture_type=ETC2PACKAGE_R_NO_MIPMAPS;
                    formatSigned=1;
                }
                if(texture_type==ETC2PACKAGE_sRGB_NO_MIPMAPS){
                    // The SRGB formats are decoded just as RGB formats -- use RGB format for decompression.
                    texture_type=ETC2PACKAGE_RGB_NO_MIPMAPS;
                }
                if(texture_type==ETC2PACKAGE_sRGBA_NO_MIPMAPS){
                    // The SRGB formats are decoded just as RGB formats -- use RGB format for decompression.
                    texture_type=ETC2PACKAGE_RGBA_NO_MIPMAPS;
                }
                if(texture_type==ETC2PACKAGE_sRGBA1_NO_MIPMAPS){
                    // The SRGB formats are decoded just as RGB formats -- use RGB format for decompression.
                    texture_type=ETC2PACKAGE_sRGBA1_NO_MIPMAPS;
                }
                if(texture_type==ETC2PACKAGE_RGBA_NO_MIPMAPS_OLD){
                    //printf("\n\nThe file %s contains a compressed texture created using an old version of ETCPACK.\n",srcfile);
                    //printf("decompression is not supported in this version.\n");
                    return ERR_UNSUPPORTED;
                }
                if(!(texture_type==ETC2PACKAGE_RGB_NO_MIPMAPS||texture_type==ETC2PACKAGE_sRGB_NO_MIPMAPS||texture_type==ETC2PACKAGE_RGBA_NO_MIPMAPS||texture_type==ETC2PACKAGE_sRGBA_NO_MIPMAPS||texture_type==ETC2PACKAGE_R_NO_MIPMAPS||texture_type==ETC2PACKAGE_RG_NO_MIPMAPS||texture_type==ETC2PACKAGE_RGBA1_NO_MIPMAPS||texture_type==ETC2PACKAGE_sRGBA1_NO_MIPMAPS)){
                    //printf("\n\n The file %s does not contain a texture of known format.\n", srcfile);
                    //printf("Known formats: ETC2PACKAGE_RGB_NO_MIPMAPS.\n", srcfile);
                    return ERR_UNSUPPORTED;
                }
            }else{
                //printf("\n\n The file %s is not of version 1.0 or 2.0 but of version %c.%c.\n",srcfile, version[0], version[1]);
                return ERR_UNSUPPORTED;
            }

            format = texture_type;
            width = top[8]*256+top[9];
            height = top[10]*256+top[11];
            active_width = top[12]*256+top[13];
            active_height = top[14]*256+top[15];

            loader_info += std::format("texture_type: {}\n", texTypeStr[format]);
            loader_info += std::format("width: {}\n", width);
            loader_info += std::format("height: {}\n", height);
            loader_info += std::format("active_width: {}\n", active_width);
            loader_info += std::format("active_height: {}\n", active_height);
        }


        if(format==ETC2PACKAGE_RG_NO_MIPMAPS){
            img=(uint8*)malloc(3*width*height*2);
        }else{
            img=(uint8*)malloc(3*width*height);
        }
        if(!img) return ERR_NOMEM;

        if(format==ETC2PACKAGE_RGBA_NO_MIPMAPS||format==ETC2PACKAGE_R_NO_MIPMAPS||format==ETC2PACKAGE_RG_NO_MIPMAPS||format==ETC2PACKAGE_RGBA1_NO_MIPMAPS||format==ETC2PACKAGE_sRGBA_NO_MIPMAPS||format==ETC2PACKAGE_sRGBA1_NO_MIPMAPS){
            //printf("alpha channel decompression\n");
            alphaimg=(uint8*)malloc(width*height*2);
            setupAlphaTable();
            if(!alphaimg){
                free(img);
                return ERR_NOMEM;
            }
        }
        if(format==ETC2PACKAGE_RG_NO_MIPMAPS){
            alphaimg2=(uint8*)malloc(width*height*2);
            if(!alphaimg2){
                if (alphaimg) free(alphaimg);
                free(img);
                return ERR_NOMEM;
            }
        }

        for(int y=0;y<height/4;y++){
            for(int x=0;x<width/4;x++){
                //decode alpha channel for RGBA
                if(format==ETC2PACKAGE_RGBA_NO_MIPMAPS||format==ETC2PACKAGE_sRGBA_NO_MIPMAPS){
                    uint8 alphablock[8];
                    copy8byte(&p, alphablock); //fread(alphablock,1,8,f);
                    decompressBlockAlpha(alphablock,alphaimg,width,height,4*x,4*y);
                }
                //color channels for most normal modes
                if(format!=ETC2PACKAGE_R_NO_MIPMAPS&&format!=ETC2PACKAGE_RG_NO_MIPMAPS){
                    //we have normal ETC2 color channels, decompress these
                    block_part1=read4byte(&p); //read_big_endian_4byte_word(&block_part1,f);
                    block_part2=read4byte(&p); //read_big_endian_4byte_word(&block_part2,f);
                    if(format==ETC2PACKAGE_RGBA1_NO_MIPMAPS||format==ETC2PACKAGE_sRGBA1_NO_MIPMAPS){
                        decompressBlockETC21BitAlpha(block_part1, block_part2,img,alphaimg,width,height,4*x,4*y);
                    }else{
                        decompressBlockETC2(block_part1, block_part2,img,width,height,4*x,4*y);
                    }
                }
                //one or two 11-bit alpha channels for R or RG.
                if(format==ETC2PACKAGE_R_NO_MIPMAPS||format==ETC2PACKAGE_RG_NO_MIPMAPS){
                    uint8 alphablock[8];
                    copy8byte(&p, alphablock); //fread(alphablock,1,8,f);
                    decompressBlockAlpha16bit(alphablock,alphaimg,width,height,4*x,4*y);
                }
                if(format==ETC2PACKAGE_RG_NO_MIPMAPS){
                    uint8 alphablock[8];
                    copy8byte(&p, alphablock); //fread(alphablock,1,8,f);
                    decompressBlockAlpha16bit(alphablock,alphaimg2,width,height,4*x,4*y);
                }
            }
        }

        unsigned char *dst;
        unsigned int dstLen;
        int fmt;
        if (format==ETC2PACKAGE_R_NO_MIPMAPS){
            dstLen = active_width*active_height*2; // R16
            fmt = FMT_R16;
        }else if (format==ETC2PACKAGE_RG_NO_MIPMAPS){
            dstLen = active_width*active_height*2*2; // RG16
            fmt = FMT_RG16;
        }else if (format==ETC2PACKAGE_RGBA_NO_MIPMAPS||
                  format==ETC2PACKAGE_RGBA1_NO_MIPMAPS||
                  format==ETC2PACKAGE_sRGBA_NO_MIPMAPS||
                  format==ETC2PACKAGE_sRGBA1_NO_MIPMAPS){
            dstLen = active_width*active_height*4; // RGBA
            fmt = FMT_RGBA8888;
        }else{
            dstLen = active_width*active_height*3; // RGB
            fmt = FMT_RGB888;
        }
        dst=(unsigned char *)malloc(dstLen);
        if (!dst){
            if (img) free(img);
            if (alphaimg) free(alphaimg);
            if (alphaimg2) free(alphaimg2);
            return ERR_NOMEM;
        }
        for(int y = 0; y<active_height; y++){
            for(int x = 0; x<active_width; x++){
                if(fmt==FMT_R16){
                    dst[(y*active_width+x)*2+0] = alphaimg[(y*width+x)*2+1];
                    dst[(y*active_width+x)*2+1] = alphaimg[(y*width+x)*2+0];
                }else if (fmt==FMT_RG16){
                    dst[(y*active_width+x)*4+0] = alphaimg[(y*width+x)*2+1];
                    dst[(y*active_width+x)*4+1] = alphaimg[(y*width+x)*2+0];
                    dst[(y*active_width+x)*4+2] = alphaimg2[(y*width+x)*2+1];
                    dst[(y*active_width+x)*4+3] = alphaimg2[(y*width+x)*2+0];
                }else if (fmt==FMT_RGBA8888){
                    dst[(y*active_width+x)*4+0] = img[(y*width+x)*3+0];
                    dst[(y*active_width+x)*4+1] = img[(y*width+x)*3+1];
                    dst[(y*active_width+x)*4+2] = img[(y*width+x)*3+2];
                    dst[(y*active_width+x)*4+3] = alphaimg[(y*width+x)];
                }else {
                    assert((unsigned int)(y*active_width+x)*3+2 < dstLen); // C6386 false positive
                    dst[(y*active_width+x)*3+0] = img[(y*width+x)*3+0];
                    dst[(y*active_width+x)*3+1] = img[(y*width+x)*3+1];
                    dst[(y*active_width+x)*3+2] = img[(y*width+x)*3+2];
                }
            }
        }

        if (img) free(img);
        if (alphaimg) free(alphaimg);
        if (alphaimg2) free(alphaimg2);

        outResult->dst = (unsigned int *)dst;
        outResult->dst_len = dstLen;

        outResult->width = active_width;
        outResult->height = active_height;
        outResult->numFrames = 1;
        outResult->flag = 0;
        outResult->format = fmt;
        outResult->colorspace = cs;
        //outResult->param1 = format;

        std::string format_info = texTypeStr[format];
        const std::string::size_type fsize = format_info.size();
        outResult->format_string = (uint32_t *)new char[fsize + 1];
        memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

        const std::string::size_type lsize = loader_info.size();
        outResult->loader_string = (uint32_t *)new char[lsize + 1];
        memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);

        return ERR_OK;
    }

    //------------------------------------------------------------------------------
    ETCPACKLOADERLIB_API void free_mem(LOAD_RESULT *result)
    {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
    }
}
