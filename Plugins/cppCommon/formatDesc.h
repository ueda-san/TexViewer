#pragma once

typedef struct {
    uint32_t fmt;
    uint32_t bytePerPixel;
    uint32_t glFormat;
    const char *formatString;
}FormatDesc;

FormatDesc descTbl[] {
    {FMT_UNKNOWN,    0,      0, "Unknown"},
    {FMT_AUTO,       0,      0, "AUTO"},
    {FMT_A8,         1,      0, "A8"},
    {FMT_R8,         1, 0x8229, "R8"},
    {FMT_R16,        2, 0x822A, "R16"},
    {FMT_RG16,       2, 0x822C, "RG16"},
    {FMT_RGB565,     2, 0x8363, "RGB565"},
    {FMT_RGBA5551,   2, 0x8057, "RGBA5551"},
    {FMT_RGBA4444,   2, 0x8056, "RGBA4444"},
    {FMT_RGB888,     3, 0x8051, "RGB888"},
    {FMT_BGR888,     3,      0, "BGR888"},
    {FMT_RGBA8888,   4, 0x8058, "RGBA8888"},
    {FMT_BGRA8888,   4, 0x80E1, "BGRA8888"},
    {FMT_RGB16,      6, 0x8054, "RGB16"},
    {FMT_RGB16F,     6, 0x881B, "RGB16F"},
    {FMT_RGB32F,    12,      0, "RGB32F"},
    {FMT_RGBA16,     8, 0x805B, "RGBA16"},
    {FMT_RGBA16F,    8, 0x881A, "RGBA16F"},
    {FMT_RGBA32F,   16,      0, "RGBA32F"},
};

uint32_t glformat2fmt(uint32_t glfmt){
    for (int i=0; i<sizeof(descTbl)/sizeof(FormatDesc); i++) {
        if (descTbl[i].glFormat == glfmt) return descTbl[i].fmt;
    }
    return FMT_UNKNOWN;
}

uint32_t fmt2bpp(uint32_t fmt){
    for (int i=0; i<sizeof(descTbl)/sizeof(FormatDesc); i++) {
        if (descTbl[i].fmt == fmt) return descTbl[i].bytePerPixel;
    }
    return 0;
}

bool fmt2a(uint32_t fmt){
    switch(fmt){
      case FMT_A8:
      case FMT_RGBA5551:
      case FMT_RGBA4444:
      case FMT_RGBA8888:
      case FMT_BGRA8888:
      case FMT_RGBA16:
      case FMT_RGBA16F:
      case FMT_RGBA32F:
        return true;
      default:
        break;
    }
    return false;
}

const char *fmt2string(uint32_t fmt){
    for (int i=0; i<sizeof(descTbl)/sizeof(FormatDesc); i++) {
        if (descTbl[i].fmt == fmt) return descTbl[i].formatString;
    }
    return "Invalid";
}
