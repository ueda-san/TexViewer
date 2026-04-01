#pragma once


// Typedefs
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef short int16;

typedef struct KTX_header_t
{
    uint8  identifier[12];
    unsigned int endianness;
    unsigned int glType;
    unsigned int glTypeSize;
    unsigned int glFormat;
    unsigned int glInternalFormat;
    unsigned int glBaseInternalFormat;
    unsigned int pixelWidth;
    unsigned int pixelHeight;
    unsigned int pixelDepth;
    unsigned int numberOfArrayElements;
    unsigned int numberOfFaces;
    unsigned int numberOfMipmapLevels;
    unsigned int bytesOfKeyValueData;
} KTX_header;

enum{ETC1_RGB_NO_MIPMAPS,ETC2PACKAGE_RGB_NO_MIPMAPS,ETC2PACKAGE_RGBA_NO_MIPMAPS_OLD,ETC2PACKAGE_RGBA_NO_MIPMAPS,ETC2PACKAGE_RGBA1_NO_MIPMAPS,ETC2PACKAGE_R_NO_MIPMAPS,ETC2PACKAGE_RG_NO_MIPMAPS,ETC2PACKAGE_R_SIGNED_NO_MIPMAPS,ETC2PACKAGE_RG_SIGNED_NO_MIPMAPS,ETC2PACKAGE_sRGB_NO_MIPMAPS,ETC2PACKAGE_sRGBA_NO_MIPMAPS,ETC2PACKAGE_sRGBA1_NO_MIPMAPS};
enum {MODE_COMPRESS, MODE_UNCOMPRESS, MODE_PSNR};
enum {SPEED_SLOW, SPEED_FAST, SPEED_MEDIUM};
enum {METRIC_PERCEPTUAL, METRIC_NONPERCEPTUAL};
enum {CODEC_ETC, CODEC_ETC2};

enum {GL_R=0x1903,GL_RG=0x8227,GL_RGB=0x1907,GL_RGBA=0x1908};

#define GL_SRGB                                          0x8C40
#define GL_SRGB8                                         0x8C41
#define GL_SRGB8_ALPHA8                                  0x8C43
#define GL_ETC1_RGB8_OES                                 0x8d64
#define GL_COMPRESSED_R11_EAC                            0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC                     0x9271
#define GL_COMPRESSED_RG11_EAC                           0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC                    0x9273
#define GL_COMPRESSED_RGB8_ETC2                          0x9274
#define GL_COMPRESSED_SRGB8_ETC2                         0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2      0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2     0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC                     0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC              0x9279

extern int formatSigned;

//extern bool readCompressParams(void);
//extern void setupAlphaTableAndValtab();

// Functions needed for decrompession ---- in etcdec.cxx
void unstuff57bits(unsigned int planar_word1, unsigned int planar_word2, unsigned int &planar57_word1, unsigned int &planar57_word2);
void unstuff59bits(unsigned int thumbT_word1, unsigned int thumbT_word2, unsigned int &thumbT59_word1, unsigned int &thumbT59_word2);
void unstuff58bits(unsigned int thumbH_word1, unsigned int thumbH_word2, unsigned int &thumbH58_word1, unsigned int &thumbH58_word2);
void decompressColor(int R_B, int G_B, int B_B, uint8 (colors_RGB444)[2][3], uint8 (colors)[2][3]);
void calculatePaintColors59T(uint8 d, uint8 p, uint8 (colors)[2][3], uint8 (possible_colors)[4][3]);
void calculatePaintColors58H(uint8 d, uint8 p, uint8 (colors)[2][3], uint8 (possible_colors)[4][3]);
void decompressBlockTHUMB59T(unsigned int block_part1, unsigned int block_part2, uint8 *img,int width,int height,int startx,int starty);
void decompressBlockTHUMB58H(unsigned int block_part1, unsigned int block_part2, uint8 *img,int width,int height,int startx,int starty);
void decompressBlockPlanar57(unsigned int compressed57_1, unsigned int compressed57_2, uint8 *img,int width,int height,int startx,int starty);
void decompressBlockDiffFlip(unsigned int block_part1, unsigned int block_part2, uint8 *img,int width,int height,int startx,int starty);
void decompressBlockETC2(unsigned int block_part1, unsigned int block_part2, uint8 *img,int width,int height,int startx,int starty);
void decompressBlockDifferentialWithAlpha(unsigned int block_part1,unsigned int block_part2, uint8* img, uint8* alpha, int width, int height, int startx, int starty);
void decompressBlockETC21BitAlpha(unsigned int block_part1, unsigned int block_part2, uint8 *img, uint8* alphaimg, int width,int height,int startx,int starty);
void decompressBlockTHUMB58HAlpha(unsigned int block_part1, unsigned int block_part2, uint8 *img, uint8* alpha,int width,int height,int startx,int starty);
void decompressBlockTHUMB59TAlpha(unsigned int block_part1, unsigned int block_part2, uint8 *img, uint8* alpha,int width,int height,int startx,int starty);
uint8 getbit(uint8 input, int frompos, int topos);
int clamp(int val);
void decompressBlockAlpha(uint8* data,uint8* img,int width,int height,int ix,int iy);
uint16 get16bits11bits(int base, int table, int mul, int index);
void decompressBlockAlpha16bit(uint8* data,uint8* img,int width,int height,int ix,int iy);
int16 get16bits11signed(int base, int table, int mul, int index);
void setupAlphaTable();
