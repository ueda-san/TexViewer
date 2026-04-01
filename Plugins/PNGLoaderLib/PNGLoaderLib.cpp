#include <string>
#include <format>
#include <vector>
#include <algorithm>
#include <sstream>
#include <numeric>

#include <stdio.h>
#include <locale.h>
#include <windows.h>
#include <stringapiset.h>

#include "PNGLoaderLib.h"
#include "ICCParser.h"
#include "EXIFParser.h"
#include "formatDesc.h"
#include "png.h"
#include <iostream>


#define PNG_BYTES_TO_CHECK 8



/*
 TODO:

  IDAT
  PLTE
  bKGD 背景色 不要？
  hIST histogram
  iTXt text UTF8
  mDCV
  oFFs
  pCAL
  sBIT 有効ビット数
  sCAL
  sPLT 代替パレット
  sRGB sRGB

 Done
  tRNS 透過色
  tIME 更新日時
  pHYs 物理サイズ
  eXIF exif
  acTL フレーム数やリピート回数
  fcTL ディレイ時間など
  fdAT フレームのデータ

  tEXt text ISO 8859-1
  zTXt text ISO 8859-1 (圧縮)
  cHRM primaries
  gAMA gamma
  iCCP ICC
  cICP CICP
  cLLI maxCLL
*/

using namespace EXIFParser;
using namespace ICCParser;

std::vector<unsigned char> rawProfile2bin(char *text, size_t len)
{
    int sect = 0;
    char *p = text;
    char *from = p;
    std::string name;
    int num = 0;

    while(char c = *p++){
        if (--len <= 0) break;
        if (c == '\n'){
            sect++;
            if (sect==1){
                from = p;
                continue;
            }else if (sect==2){
                name = std::string(from, p-from-1);
                from = p;
            }else if (sect==3){
                num = std::stoi(std::string(from, p-from-1));
                break;
            }
        }
    }
    if (name == "exif"){ // skip APP1 marker
        len -= 12;
        p += 12;
        num -= 6;
    }
    int val=0;
    bool high=true;
    std::vector<unsigned char> ret;
    while(num > 0 && len > 0){
        len--;
        char c = *p++;
        if (c == '\n') continue;
        if (c >='0' && c <= '9') {
            val = val*16 + (c-'0'); high = !high;
        }else if (c >='a' && c <= 'f') {
            val = val*16 + (c-'a'+10);  high = !high;
        }else if (c >='A' && c <= 'F') {
            val = val*16 + (c-'A'+10);  high = !high;
        }else{
            continue;
        }
        if (high){
            ret.push_back(val);
            num--;
            val = 0;
        }
    }
    return ret;
}



//------------------------------------------------------------------------------
int determinColorSpace(png_const_structrp png_ptr, png_inforp info_ptr, std::string &info, IccParser *iccParser, double *outGamma) {
    int cs = CS_SRGB; //UNKNOWN;
    int tf = TF_SRGB; //DEFAULT;

    //--------------------
    // check gAMA (Priority:4)
    if (png_get_gAMA(png_ptr, info_ptr, outGamma)) {
        info += std::format("gAMA chunk:\n");
        if (*outGamma == 1.0){
            tf = TF_LINEAR;   //convert srgb by libpng
            info += std::format(" {:.1f} (Linear)\n", *outGamma);
        }else{
            tf = TF_GAMMA_N;  //convert srgb by libpng
            info += std::format(" {:.5f} (1.0/{:.2f})\n", *outGamma, 1.0 / *outGamma);
        }
    }


    //--------------------
    // check cICP (Priority:1)
    png_byte primariesBytes, transfer, matrix, full_range;
    if (png_get_cICP(png_ptr, info_ptr, &primariesBytes, &transfer, &matrix, &full_range)){
        // H.273 : Coding-independent code points for video signal type identification
        info += "cICP chunk:\n";
        info += "ColorPrimaries: ";
        cs = IccParser::colorSpaceFromCICP(primariesBytes);
        info += IccParser::csStringFromCICP(primariesBytes);

        info += "\nTransferCharacteristics: ";
        tf = IccParser::transferFunctionFromCICP(transfer);
        info += IccParser::tfStringFromCICP(transfer);

        info += "\nMatrixCoefficients: ";
        switch(matrix){
          case 0:  info += "RGB(Identity)";                   break; //SMPTE ST 428-1
          case 1:  info += "YCbCr(BT.709)";                   break;
          case 5:  info += "YCbCr(BT.601)";                   break;
          case 6:  info += "BT.601";                          break;
          case 9:  info += "BT.2020(non-constant luminance)"; break;
          case 10: info += "BT.2020(constant luminance)";     break;
          case 14: info += "BT.2100";                         break;
          default:
            info += std::format("Unknown({})", matrix);
            break;
        }

        //info += std::format("\nVideoFullRangeFlag: ({})", full_range);
        info += std::format("\n");
        return cs | tf;
    }

    //--------------------
    // check iCCP (Priority:2)
    png_charp name;
    int comp_type;
    png_bytep profile;
    png_uint_32 len;
    if (png_get_iCCP(png_ptr, info_ptr, &name, &comp_type, &profile, &len)) {
        info += "iCCP chunk:\n";
        if (iccParser->parseIcc(profile, len) == 1) {
            cs = iccParser->getColorSpace();
            int _tf = iccParser->getTransferFunction();
            if (_tf != TF_DEFAULT) tf = _tf;
            info += iccParser->dumpIccInfo();
        }
        return cs | tf;
    }


    //--------------------
    // check sRGB (Priority:3)
    int intent;
    if (png_get_sRGB(png_ptr, info_ptr, &intent)){
        info += "sRGB chunk:\n";
        std::string intents[] = {"Perceptual", "Relative colorimetric", "Saturation", "Absolute colorimetric" };
        if (intent >= 0 && intent <= 3){
            info += std::format("intent: {}\n", intents[intent]);
        }else{
            info += std::format("intent: {}\n", intent);
        }
        cs = CS_SRGB;
        tf = TF_SRGB;
        return cs | tf;
    }


    //--------------------
    // check cHRM (Priority:4)
    Primaries primaries;
    if (png_get_cHRM(png_ptr, info_ptr,
                     &primaries.wx, &primaries.wy,
                     &primaries.rx, &primaries.ry,
                     &primaries.gx, &primaries.gy,
                     &primaries.bx, &primaries.by)){
        info += "cHRM chunk:\n";
        cs = IccParser::colorSpaceFromPrimaries(primaries);
        info += IccParser::dumpPrimaries(primaries, cs);
        return cs | tf;
    }


    return cs | tf;
}

double getMaxCLL(png_const_structrp png_ptr, png_const_inforp info_ptr){
    double maxCLL;
    double frame_average;
    if (png_get_cLLI(png_ptr, info_ptr, &maxCLL, &frame_average)){
        return maxCLL;
    }
    return 0.0;
}

struct PNGPAGE : PAGE
{
    unsigned char dispose_op;
    unsigned char blend_op;
};

int getBytesPerPixel(int bit_depth, int channels){
    return (bit_depth * channels + 7)/8;
}

unsigned char *createCanvas(int w, int h, int channels, bool is16bit)
{
    int bytesPerPixel = channels * (is16bit?2:1);
    return (unsigned char *)malloc(w * h * bytesPerPixel);
}
unsigned char *copyFromCanvas(unsigned char *canvas, int stride, PNGPAGE page, int channels, bool is16bit)
{
    int bytesPerPixel = channels * (is16bit?2:1);
    unsigned char *buf = (unsigned char *)malloc(page.width * page.height * bytesPerPixel);
    unsigned char *p = buf;
    for (int i=0; i<page.height; i++, p+=bytesPerPixel*page.width){
        memcpy(p, canvas + stride*(page.top+i) + bytesPerPixel*page.left, bytesPerPixel*page.width);
    }
    return buf;
}
void disposeOpCanvas(unsigned char* canvas, int stride, PNGPAGE page, int channels, bool is16bit, unsigned char *restoreBuf)
{
    int bytesPerPixel = channels * (is16bit?2:1);
    if (page.dispose_op == 1){ // DISPOSE_OP_BACKGROUND
        for (int i=0; i<page.height; i++){
            memset(canvas + stride*(page.top+i) + bytesPerPixel*page.left, 0, bytesPerPixel*page.width);
        }
    }else if (page.dispose_op == 2){; // DISPOSE_OP_PREVIOUS
        if (restoreBuf == 0) return;
        unsigned char *p = restoreBuf;
        for (int i=0; i<page.height; i++, p+=bytesPerPixel*page.width){
            memcpy(canvas + stride*(page.top+i) + bytesPerPixel*page.left, p, bytesPerPixel*page.width);
        }
    }
}

unsigned char *drawCanvas(unsigned char* canvas, int stride, PNGPAGE page, int channels, bool is16bit, unsigned char *data /*, float gamma*/)
{
    unsigned char *restoreBuf = 0;
    int bytesPerPixel = channels * (is16bit?2:1);
    if (page.dispose_op == 2){ // DISPOSE_OP_PREVIOUS
        restoreBuf = copyFromCanvas(canvas, stride, page, channels, is16bit);
    }
    if (page.blend_op == 1 && channels == 4){
        if (is16bit){
            unsigned short *src = (unsigned short *)data;
            for (int i=0; i<page.height; i++){
                unsigned short* dst = (unsigned short*)(canvas + stride * (page.top+i) + bytesPerPixel * page.left);
                for (int j=0; j<page.width; j++){
                    float alpha = (channels == 4)?(src[3]/0x0ffff):1.0f;
                    if (alpha == 0.0f){
                        // nothing
                        dst += channels;
                        src += channels;
                    }else if (alpha == 1.0f){
                        *dst++ = *src++; // R
                        *dst++ = *src++; // G
                        *dst++ = *src++; // B
                        if (channels == 4) *dst++ = *src++;
                    }else{
                        // 本当は linear 空間にしてから合成だけど BLEND_OP_OVER な apng は少ない…
                        float val;
                        val = *dst * (1.0f-alpha) + src[0] * alpha;
                        *dst++ = (unsigned short)(val+0.5f);
                        val = *dst * (1.0f-alpha) + src[1] * alpha;
                        *dst++ = (unsigned short)(val+0.5f);
                        val = *dst * (1.0f-alpha) + src[2] * alpha;
                        *dst++ = (unsigned short)(val+0.5f);
                        *dst++ = src[3];
                        src += 4;
                    }
                }
            }
        }else{

            unsigned char *src = data;
            for (int i=0; i<page.height; i++){
                unsigned char* dst = (unsigned char*)(canvas + stride * (page.top+i) + bytesPerPixel * page.left);
                for (int j=0; j<page.width; j++){
                    float alpha = (channels == 4)?(src[3]/0x0ff):1.0f;
                    if (alpha == 0.0f){
                        // nothing
                        dst += channels;
                        src += channels;
                    }else if (alpha == 1.0f){
                        *dst++ = *src++; // R
                        *dst++ = *src++; // G
                        *dst++ = *src++; // B
                        if (channels == 4) *dst++ = *src++;
                    }else{
                        // 本当は linear 空間にしてから合成だけど BLEND_OP_OVER な apng は少ない…
                        float val;
                        val = *dst * (1.0f-alpha) + src[0] * alpha;
                        *dst++ = (unsigned char)(val+0.5f);
                        val = *dst * (1.0f-alpha) + src[1] * alpha;
                        *dst++ = (unsigned char)(val+0.5f);
                        val = *dst * (1.0f-alpha) + src[2] * alpha;
                        *dst++ = (unsigned char)(val+0.5f);
                        *dst++ = src[3];
                        src += 4;
                    }
                }
            }
        }
    }else{
        unsigned char *p = data;
        for (int i=0; i<page.height; i++, p+=bytesPerPixel*page.width){
            memcpy(canvas + stride*(page.top+i) + bytesPerPixel*page.left, p, bytesPerPixel*page.width);
        }
    }
    return restoreBuf;
}


extern "C" {
    //------------------------------------------------------------------------------
    PNGLOADERLIB_API const char* get_version()
    {
        static const char* buf = PNG_LIBPNG_VER_STRING;
        return buf;
    }

    PNGLOADERLIB_API int load_file(const wchar_t* file, LOAD_RESULT* outResult, uint32_t opt)
    {
        FILE *fp;
        png_byte sig[PNG_BYTES_TO_CHECK];
        if (_wfopen_s(&fp, file, L"rb") != 0) return ERR_NOTFOUND;
        if (fread(sig, PNG_BYTES_TO_CHECK, 1, fp) != 1 || png_sig_cmp(sig, 0, PNG_BYTES_TO_CHECK)) {
            fclose(fp);
            return ERR_UNSUPPORTED;
        }

        int ret = ERR_UNKNOWN;
        outResult->flag = 0;
        outResult->param1 = 0;
        std::string loader_info="";

        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (png_ptr != NULL) {
            png_infop info_ptr = png_create_info_struct(png_ptr);
            if (info_ptr != NULL) {
                if (setjmp(png_jmpbuf(png_ptr))){
                    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
                    fclose(fp);
                    return ERR_UNKNOWN;
                }

                png_init_io(png_ptr, fp);
                png_set_sig_bytes(png_ptr, PNG_BYTES_TO_CHECK);
                png_read_info(png_ptr, info_ptr);

                png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
                png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
                png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
                png_byte channels = png_get_channels(png_ptr, info_ptr);

                png_byte color_type = png_get_color_type(png_ptr, info_ptr);
                int bytes_per_pixel = getBytesPerPixel(bit_depth, channels);

                static const char *png_type[] = {
                    "grayscale","INVALID","RGB","palette","grayscale+alpha","INVALID","RGB+alpha"
                };

                loader_info += std::format("Width: {}\n", width);
                loader_info += std::format("Height: {}\n", height);
                loader_info += std::format("Depth: {}\n", bit_depth);
                loader_info += std::format("Channels: {}\n", channels);
                loader_info += std::format("Color Type: {}\n", png_type[color_type]);


                png_set_alpha_mode(png_ptr, PNG_ALPHA_PNG, PNG_DEFAULT_sRGB);

                if (color_type == PNG_COLOR_TYPE_PALETTE){
                    png_set_palette_to_rgb(png_ptr);
                }
                if (color_type == PNG_COLOR_TYPE_GRAY /* && bit_depth < 8*/) {
                    png_set_strip_16(png_ptr);
                    png_set_gray_to_rgb(png_ptr);
                }
                if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                    png_set_strip_16(png_ptr);
                    png_set_gray_to_rgb(png_ptr);
                }

                if (bit_depth == 16) {
                    png_set_swap(png_ptr);
                }

                if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)){
                    png_set_tRNS_to_alpha(png_ptr);
                }

                //png_set_alpha_mode(png_ptr, PNG_ALPHA_PNG, PNG_DEFAULT_sRGB);

//https://www.w3.org/Graphics/PNG/all_seven.html
//030: ff ee de cf c1 b5 a8 9d 93 89 80 77 6f 68 61 5a 54 4f 4a 45 40 00
//045: ff e6 cf bb a8 98 89 7b 6f 64 5a 52 4a 42 3c 36 31 2c 27 24 20 00
//100: ff cb a1 80 66 51 40 33 28 20 1a 14 10 0d 0a 08 06 05 04 03 02 00

                double gamma=1.0/2.2;
                IccParser *iccParser = new IccParser();
                int cs = determinColorSpace(png_ptr, info_ptr, loader_info, iccParser, &gamma);
                png_set_gamma(png_ptr, 1.0, 1.0);  // no gamma collect

                if (png_get_interlace_type(png_ptr, info_ptr) == PNG_INTERLACE_ADAM7) {
                    //int number_of_passes = png_set_interlace_handling(png_ptr);
                }


                png_read_update_info(png_ptr, info_ptr);

                bit_depth = png_get_bit_depth(png_ptr, info_ptr);
                channels = png_get_channels(png_ptr, info_ptr);
                color_type = png_get_color_type(png_ptr, info_ptr);
                bytes_per_pixel = getBytesPerPixel(bit_depth, channels);

                png_textp       text_ptr;
                int             num_text;
                if (png_get_text(png_ptr, info_ptr, &text_ptr, &num_text)){
                    loader_info += "text chunk:\n";
                    for (int i=0; i<num_text; i++) {
                        if (std::string(text_ptr[i].key).starts_with("Raw profile type")){
                            loader_info += std::format("({})\n", text_ptr[i].key);
                            if (std::string(text_ptr[i].key).ends_with("APP1") ||
                                std::string(text_ptr[i].key).ends_with("exif")){
                                unsigned short root = IFD_Exif;
                                size_t len = (text_ptr[i].text_length==0)?text_ptr[i].itxt_length:text_ptr[i].text_length;
                                std::vector<unsigned char> bin = rawProfile2bin(text_ptr[i].text, len);
                                ExifParser parser = ExifParser((opt&0x0001)?0:ExifParser::getSimpleTagOfInterest());
                                if (parser.parseExif(bin.data(), bin.size(), root) > 0){
                                    loader_info += parser.dumpExif();
                                }
                            }
                        }else if (std::string(text_ptr[i].key)=="XML:com.adobe.xmp"){
                            //std::string xml = std::string(text_ptr[i].text);
                            loader_info += std::format("(XMP metadata)\n");
                        }else{
                            loader_info += std::format("{}: {}\n", text_ptr[i].key, text_ptr[i].text);
                        }
                    }
                }

                png_bytep exif_ptr;
                png_uint_32 exif_size;
                if (png_get_eXIf_1(png_ptr, info_ptr, &exif_size, &exif_ptr)){
                    ExifParser parser = ExifParser();
                    if (parser.parseExif(exif_ptr, exif_size, IFD_Exif) > 0){
                        loader_info += "eXIf chunk:\n";
                        loader_info += parser.dumpExif();
                    }
                }

                //
                png_uint_32 res_x, res_y;
                int unit_type;
                if (png_get_pHYs(png_ptr, info_ptr, &res_x, &res_y, &unit_type)){
                    loader_info += "pHYs chunk:\n";
                    if (unit_type == 0){
                        int gcd = std::gcd(res_x, res_y);
                        loader_info += std::format("aspect ratio: {}:{}\n", res_x/gcd, res_y/gcd);
                    }else if (unit_type == 1){
                        loader_info += std::format("DPI: {:.0f} x {:.0f}\n",
                                                   res_x/39.37007874, res_y/39.37007874);
                    }
                }

                png_timep last_mod;
                if (png_get_tIME(png_ptr, info_ptr, &last_mod)){
                    loader_info += "tIME chunk:\n";
                    loader_info += std::format("last-modification time: {}-{}-{} {}:{:02}:{:02} (GMT)\n",
                                               last_mod->year, last_mod->month, last_mod->day,
                                               last_mod->hour, last_mod->minute, last_mod->second);
                }

                if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL)){
                    png_uint_32 num_frames;// = png_get_num_frames(png_ptr, info_ptr);
                    png_uint_32 num_plays;
                    if (png_get_acTL(png_ptr, info_ptr, &num_frames, &num_plays)){
                        png_byte is_hidden = png_get_first_frame_is_hidden(png_ptr, info_ptr);
                    }

                    bool is16bit = (bit_depth > 8);
                    int stride = width * bytes_per_pixel;
                    unsigned char *canvas = createCanvas(width, height, channels, is16bit);
                    unsigned char *buf = (unsigned char *)malloc(width * height * bytes_per_pixel * num_frames);
                    unsigned int offset = 0;
                    PAGE *pages = 0;
                    PNGPAGE *pngpages = 0;
                    if (num_frames > 1){
                        pages = (PAGE *)calloc(sizeof(PAGE), num_frames);
                        pngpages = (PNGPAGE *)calloc(sizeof(PNGPAGE), num_frames);
                    }
                    png_uint_32 frame_width, frame_height;
                    png_uint_32 x_offset, y_offset;
                    png_uint_16 delay_num, delay_den;
                    png_byte dispose_op , blend_op;
                    for (unsigned int i=0; i<num_frames; i++){
                        png_read_frame_head(png_ptr, info_ptr);
                        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_fcTL)) {
                            png_get_next_frame_fcTL(png_ptr, info_ptr,
                                                    &frame_width, &frame_height,
                                                    &x_offset, &y_offset,
                                                    &delay_num, &delay_den,
                                                    &dispose_op, &blend_op);
                        } else {
                            frame_width = width;
                            frame_height = height;
                            x_offset = 0;
                            y_offset = 0;
                            delay_num = 1;
                            delay_den = 1;
                            dispose_op = PNG_fcTL_DISPOSE_OP_NONE;
                            blend_op = PNG_fcTL_BLEND_OP_SOURCE;
                        }
                        if (pngpages){
                            pngpages[i].top = y_offset;
                            pngpages[i].left = x_offset;
                            pngpages[i].width = frame_width;
                            pngpages[i].height = frame_height;
                            pngpages[i].dispose_op = dispose_op;
                            pngpages[i].blend_op = blend_op;
                        }
                        if (pages){
                            pages[i].top = 0;
                            pages[i].left = 0;
                            pages[i].width = width;
                            pages[i].height = height;
                            if (delay_den == 0) delay_den = 100;
                            pages[i].delay = 100*delay_num/delay_den;
                        }
                        int frame_stride = frame_width * bytes_per_pixel;
                        unsigned char *frame_buf = (unsigned char *)malloc(frame_stride * frame_height);
                        png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * frame_height);
                        for (unsigned int j=0; j<frame_height; j++){
                            row_pointers[j] = frame_buf + frame_stride * j;
                        }
                        png_read_image(png_ptr, row_pointers);
                        unsigned char *restoreBuf = drawCanvas(canvas, stride, pngpages[i], channels, is16bit, frame_buf);
                        memcpy(buf+offset, canvas, stride * height);
                        disposeOpCanvas(canvas, stride, pngpages[i], channels, is16bit, restoreBuf);
                        if (restoreBuf) free(restoreBuf);
                        free(row_pointers);
                        free(frame_buf);
                        offset += stride * height;
#if 0
                        FILE *fpw;
                        std::string pathDebug = std::format("d:/tmp/pngtest{}_{}x{}",
                                                            i, frame_width, frame_height);
                        if (fopen_s(&fpw, pathDebug.c_str(), "wb") == 0){
                            fwrite(frame_buf, frame_stride * frame_height, 1, fpw);
                            fclose(fpw);
                        }
#endif
                    }
                    free(canvas);
                    outResult->dst = (uint32_t *)buf;
                    outResult->dst_len = offset;
                    outResult->numFrames = num_frames;
                    outResult->pages = (uint32_t *)pages;
                    free(pngpages);
                }else{
                    int stride = width * bytes_per_pixel;
                    unsigned char *buf = (unsigned char *)malloc(stride * height);

                    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
                    for (unsigned int i=0; i<height; i++){
                        row_pointers[i] = buf+stride*i;
                    }
                    png_read_image(png_ptr, row_pointers);
                    free(row_pointers);

                    outResult->dst = (uint32_t *)buf;
                    outResult->dst_len = stride * height;
                    outResult->numFrames = 1;
                }


                int fmt;
                if (bit_depth == 8){
                    fmt = (channels==3)?FMT_RGB888:FMT_RGBA8888;
                }else{
                    fmt = (channels==3)?FMT_RGB16:FMT_RGBA16;
                }

                outResult->width = width;
                outResult->height = height;
                outResult->format = fmt;
                outResult->colorspace = cs;
                outResult->maxCLL = (uint32_t)getMaxCLL(png_ptr, info_ptr);
                if (outResult->maxCLL != 0){
                    loader_info += std::format("cLLI: maxCLL={}\n", outResult->maxCLL);
                }
                outResult->gamma = (float)gamma;
                if (iccParser->hasPrimaries()){
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
                }else {
                    Primaries prim;
                    if (png_get_cHRM(png_ptr, info_ptr,
                                     &prim.wx, &prim.wy,
                                     &prim.rx, &prim.ry,
                                     &prim.gx, &prim.gy,
                                     &prim.bx, &prim.by)){
                        outResult->flag |= 0x08;
                        outResult->rX = (float)(prim.rx / prim.ry);
                        outResult->rY = 1.0f;
                        outResult->rZ = (float)((1.0 - prim.rx - prim.ry)/prim.ry);
                        outResult->gX = (float)(prim.gx / prim.gy);
                        outResult->gY = 1.0f;
                        outResult->gZ = (float)((1.0 - prim.gx - prim.gy)/prim.gy);
                        outResult->bX = (float)(prim.bx / prim.by);
                        outResult->bY = 1.0f;
                        outResult->bZ = (float)((1.0 - prim.bx - prim.by)/prim.by);
                        outResult->wX = (float)(prim.wx / prim.wy);
                        outResult->wY = 1.0f;
                        outResult->wZ = (float)((1.0 - prim.wx - prim.wy)/prim.wy);
                    }
                }

                std::string format_info = "png";

                const std::string::size_type fsize = format_info.size();
                outResult->format_string = (uint32_t*)new char[fsize + 1];
                memcpy(outResult->format_string, format_info.c_str(), fsize + 1);

                const std::string::size_type lsize = loader_info.size();
                outResult->loader_string = (uint32_t*)new char[lsize + 1];
                memcpy(outResult->loader_string, loader_info.c_str(), lsize + 1);
                ret = ERR_OK;

                png_destroy_info_struct(png_ptr, &info_ptr);
            }
            png_destroy_read_struct(&png_ptr, NULL, NULL);
        }
        fclose(fp);

        return ret;
    }

    PNGLOADERLIB_API void free_mem(LOAD_RESULT* result)
    {
        delete[] result->format_string;
        delete[] result->loader_string;
        free(result->dst);
        free(result->pages);
    }
}
