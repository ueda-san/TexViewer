#ifndef IMAGE_LOAD_INCLUDED
#define IMAGE_LOAD_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "astcenc.h"
#include "astcenc_mathlib.h"

struct astc_compressed_image
{
    /** @brief The block width in texels. */
    unsigned int block_x;

    /** @brief The block height in texels. */
    unsigned int block_y;

    /** @brief The block depth in texels. */
    unsigned int block_z;

    /** @brief The image width in texels. */
    unsigned int dim_x;

    /** @brief The image height in texels. */
    unsigned int dim_y;

    /** @brief The image depth in texels. */
    unsigned int dim_z;

    /** @brief The binary data payload. */
    uint8_t* data;

    /** @brief The binary data length in bytes. */
    size_t data_len;
};

astcenc_image *alloc_image(unsigned int bitness, unsigned int dim_x, unsigned int dim_y, unsigned int dim_z);
void free_image(astcenc_image * img);
int load_cimage(const wchar_t* filename, astc_compressed_image& img);
bool load_ktx_compressed_image(const wchar_t* filename, bool& is_srgb,  astc_compressed_image& img, std::string& info);


#endif //IMAGE_LOAD_INCLUDED
