#pragma once

#include <stdint.h>

enum {
    ERR_OK = 0,
    ERR_UNKNOWN = -1,
    ERR_UNSUPPORTED = -2,
    ERR_NOTIMPLEMENTED = -3,
    ERR_NOTFOUND = -4,
    ERR_NOMEM = -5,

    ERR_CHECKME = -99,
};

enum {
    FMT_UNKNOWN,
    FMT_AUTO, // Contains a header that imagemagick recognizes
    FMT_A8,
    FMT_R8,
    FMT_R16,
    FMT_RG16,
    FMT_RGB565,
    FMT_RGBA5551,
    FMT_RGBA4444,
    FMT_RGB888,
    FMT_BGR888,
    FMT_RGBA8888,
    FMT_BGRA8888,
    FMT_RGB16,
    FMT_RGB16F,
    FMT_RGB32F,
    FMT_RGBA16,
    FMT_RGBA16F,
    FMT_RGBA32F,
    FMT_RGBA1010102,
};

enum {
    CS_UNKNOWN,
    CS_LINEAR,  // == BT2020
    CS_SRGB,    // == BT709
    CS_BT601,
    CS_BT709,
    CS_BT2020,
    CS_ADOBE98,
    CS_DCI_P3,     //smpte431
    CS_DISPLAY_P3, //smpte432

    TF_DEFAULT    = 0<<8,
    TF_LINEAR     = 1<<8,
    TF_SRGB       = 2<<8,
    TF_BT601      = 3<<8,
    TF_BT709      = 4<<8,
    TF_BT2020     = 5<<8,
    TF_ADOBE98    = 6<<8, //== gamma 2.2
    TF_DCI_P3     = 7<<8, //== TF_BT709,
    TF_DISPLAY_P3 = 8<<8, //== TF_BT709,
    TF_PQ         = 100<<8,
    TF_HLG        = 101<<8,
    TF_GAMMA_N    = 102<<8,

    //TF_PARAMETRIC = 103<<8,
    //TF_LUT        = 104<<8,


    //CS_BT1886, ?
    //CS_LINEAR_SRGB ?
    //CS_SCRGB ?
};
static const char *csList[] = {
    "UNKNOWN", "Linear", "sRGB", "BT601", "BT709", "BT2020",
    "Adobe98", "DCI_P3", "Display_P3"
};
static const char *tfList[] = {
    "DEFAULT", "Linear", "sRGB", "BT601", "BT709", "BT2020",
    "Adobe98", "DCI_P3", "Display_P3", //"PQ", "HLG", "Gamma"
};


typedef struct {
    unsigned short top;
    unsigned short left;
    unsigned short width;
    unsigned short height;
    unsigned short delay;
}PAGE;


typedef struct {
    uint32_t *dst;
    uint32_t dst_len;

    uint32_t width;
    uint32_t height;
    uint32_t numFrames;
    uint32_t format;      // FMT_*
    uint32_t colorspace;  // TF_* | CS_*
    uint32_t flag;        // bit 0:PMA  1:needCoalesce  2:hasGainMap 3:hasPrimaries
    uint32_t maxCLL;      // 0: no data
    uint32_t param1;
    uint32_t orientation; // Exif:orientation (1:normal ... 8:Rotate 270 CW)

    uint32_t *pages;    // if numFrame>1
    float gamma;        // if TF==TF_GAMMA_N
    float rX, rY, rZ; // if flag & 8
    float gX, gY, gZ;
    float bX, bY, bZ;
    float wX, wY, wZ;
    float RGBScale;
    //float parametricParam[8];
    //uint16_t *LookUpTable;

    uint32_t *format_string;
    uint32_t *loader_string;
}LOAD_RESULT;
