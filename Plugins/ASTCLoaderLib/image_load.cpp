#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <format>

#include "image_load.h"
#include "glFormatStr.h"


astcenc_image *alloc_image(unsigned int bitness, unsigned int dim_x, unsigned int dim_y, unsigned int dim_z) {
    astcenc_image *img = new astcenc_image;
    img->dim_x = dim_x;
    img->dim_y = dim_y;
    img->dim_z = dim_z;

    void** data = new void*[dim_z];
    img->data = data;

    if (bitness == 8)
    {
        img->data_type = ASTCENC_TYPE_U8;
        for (unsigned int z = 0; z < dim_z; z++)
        {
            data[z] = new uint8_t[dim_x * dim_y * 4];
        }
    }
    else if (bitness == 16)
    {
        img->data_type = ASTCENC_TYPE_F16;
        for (unsigned int z = 0; z < dim_z; z++)
        {
            data[z] = new uint16_t[dim_x * dim_y * 4];
        }
    }
    else // if (bitness == 32)
    {
        assert(bitness == 32);
        img->data_type = ASTCENC_TYPE_F32;
        for (unsigned int z = 0; z < dim_z; z++)
        {
            data[z] = new float[dim_x * dim_y * 4];
        }
    }

    return img;
}

/* See header for documentation. */
void free_image(astcenc_image * img)
{
    if (img == nullptr)
    {
        return;
    }

    for (unsigned int z = 0; z < img->dim_z; z++)
    {
        delete[] reinterpret_cast<char*>(img->data[z]);
    }

    delete[] img->data;
    delete img;
}





/* ============================================================================
    ASTC compressed file loading
============================================================================ */
struct astc_header
{
    uint8_t magic[4];
    uint8_t block_x;
    uint8_t block_y;
    uint8_t block_z;
    uint8_t dim_x[3];           // dims = dim[0] + (dim[1] << 8) + (dim[2] << 16)
    uint8_t dim_y[3];           // Sizes are given in texels;
    uint8_t dim_z[3];           // block count is inferred
};

static const uint32_t ASTC_MAGIC_ID = 0x5CA1AB13;

static unsigned int unpack_bytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (static_cast<unsigned int>(a)      ) +
        (static_cast<unsigned int>(b) <<  8) +
        (static_cast<unsigned int>(c) << 16) +
        (static_cast<unsigned int>(d) << 24);
}

int load_cimage(const wchar_t* filename, astc_compressed_image& img) {
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file) {
        //print_error("ERROR: File open failed '%s'\n", filename);
        return 1;
    }

    astc_header hdr;
    file.read(reinterpret_cast<char*>(&hdr), sizeof(astc_header));
    if (file.fail()) {
        //print_error("ERROR: File read failed '%s'\n", filename);
        return 1;
    }

    unsigned int magicval = unpack_bytes(hdr.magic[0], hdr.magic[1], hdr.magic[2], hdr.magic[3]);
    if (magicval != ASTC_MAGIC_ID) {
        //print_error("ERROR: File not recognized '%s'\n", filename);
        return 1;
    }

    // Ensure these are not zero to avoid div by zero
    unsigned int block_x = astc::max(static_cast<unsigned int>(hdr.block_x), 1u);
    unsigned int block_y = astc::max(static_cast<unsigned int>(hdr.block_y), 1u);
    unsigned int block_z = astc::max(static_cast<unsigned int>(hdr.block_z), 1u);

    unsigned int dim_x = unpack_bytes(hdr.dim_x[0], hdr.dim_x[1], hdr.dim_x[2], 0);
    unsigned int dim_y = unpack_bytes(hdr.dim_y[0], hdr.dim_y[1], hdr.dim_y[2], 0);
    unsigned int dim_z = unpack_bytes(hdr.dim_z[0], hdr.dim_z[1], hdr.dim_z[2], 0);

    if (dim_x == 0 || dim_y == 0 || dim_z == 0)
    {
        //print_error("ERROR: Image header corrupt '%s'\n", filename);
        return 1;
    }

    unsigned int xblocks = (dim_x + block_x - 1) / block_x;
    unsigned int yblocks = (dim_y + block_y - 1) / block_y;
    unsigned int zblocks = (dim_z + block_z - 1) / block_z;

    size_t data_size = xblocks * yblocks * zblocks * 16;
    uint8_t *buffer = new uint8_t[data_size];

    file.read(reinterpret_cast<char*>(buffer), data_size);
    if (file.fail())
    {
        //print_error("ERROR: Image data size exceeded file size '%s'\n", filename);
        delete[] buffer;
        return 1;
    }

    img.data = buffer;
    img.data_len = data_size;
    img.block_x = block_x;
    img.block_y = block_y;
    img.block_z = block_z;
    img.dim_x = dim_x;
    img.dim_y = dim_y;
    img.dim_z = dim_z;
    return 0;
}

//------------------------------------------------------------------------------
static uint32_t u32_byterev(uint32_t v)
{
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}


// Khronos enums
#define GL_RED                                      0x1903
#define GL_RG                                       0x8227
#define GL_RGB                                      0x1907
#define GL_RGBA                                     0x1908
#define GL_BGR                                      0x80E0
#define GL_BGRA                                     0x80E1
#define GL_LUMINANCE                                0x1909
#define GL_LUMINANCE_ALPHA                          0x190A

#define GL_R8                                       0x8229
#define GL_RG8                                      0x822B
#define GL_RGB8                                     0x8051
#define GL_RGBA8                                    0x8058

#define GL_R16F                                     0x822D
#define GL_RG16F                                    0x822F
#define GL_RGB16F                                   0x881B
#define GL_RGBA16F                                  0x881A

#define GL_UNSIGNED_BYTE                            0x1401
#define GL_UNSIGNED_SHORT                           0x1403
#define GL_HALF_FLOAT                               0x140B
#define GL_FLOAT                                    0x1406

#define GL_COMPRESSED_RGBA_ASTC_4x4                 0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4                 0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5                 0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5                 0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6                 0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5                 0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6                 0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8                 0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5                0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6                0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8                0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10               0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10               0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12               0x93BD

#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4         0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4         0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5         0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5         0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6         0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5         0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6         0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8         0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5        0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6        0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8        0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10       0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10       0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12       0x93DD

#define GL_COMPRESSED_RGBA_ASTC_3x3x3_OES           0x93C0
#define GL_COMPRESSED_RGBA_ASTC_4x3x3_OES           0x93C1
#define GL_COMPRESSED_RGBA_ASTC_4x4x3_OES           0x93C2
#define GL_COMPRESSED_RGBA_ASTC_4x4x4_OES           0x93C3
#define GL_COMPRESSED_RGBA_ASTC_5x4x4_OES           0x93C4
#define GL_COMPRESSED_RGBA_ASTC_5x5x4_OES           0x93C5
#define GL_COMPRESSED_RGBA_ASTC_5x5x5_OES           0x93C6
#define GL_COMPRESSED_RGBA_ASTC_6x5x5_OES           0x93C7
#define GL_COMPRESSED_RGBA_ASTC_6x6x5_OES           0x93C8
#define GL_COMPRESSED_RGBA_ASTC_6x6x6_OES           0x93C9

#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES   0x93E0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES   0x93E1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES   0x93E2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES   0x93E3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES   0x93E4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES   0x93E5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES   0x93E6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES   0x93E7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES   0x93E8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES   0x93E9

struct format_entry
{
    unsigned int x;
    unsigned int y;
    unsigned int z;
    bool is_srgb;
    unsigned int format;
};

static const std::array<format_entry, 48> ASTC_FORMATS =
{{
    // 2D Linear RGB
    { 4,  4,  1, false, GL_COMPRESSED_RGBA_ASTC_4x4},
    { 5,  4,  1, false, GL_COMPRESSED_RGBA_ASTC_5x4},
    { 5,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_5x5},
    { 6,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_6x5},
    { 6,  6,  1, false, GL_COMPRESSED_RGBA_ASTC_6x6},
    { 8,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_8x5},
    { 8,  6,  1, false, GL_COMPRESSED_RGBA_ASTC_8x6},
    { 8,  8,  1, false, GL_COMPRESSED_RGBA_ASTC_8x8},
    {10,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_10x5},
    {10,  6,  1, false, GL_COMPRESSED_RGBA_ASTC_10x6},
    {10,  8,  1, false, GL_COMPRESSED_RGBA_ASTC_10x8},
    {10, 10,  1, false, GL_COMPRESSED_RGBA_ASTC_10x10},
    {12, 10,  1, false, GL_COMPRESSED_RGBA_ASTC_12x10},
    {12, 12,  1, false, GL_COMPRESSED_RGBA_ASTC_12x12},
    // 2D SRGB
    { 4,  4,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4},
    { 5,  4,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4},
    { 5,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5},
    { 6,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5},
    { 6,  6,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6},
    { 8,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5},
    { 8,  6,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6},
    { 8,  8,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8},
    {10,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5},
    {10,  6,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6},
    {10,  8,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8},
    {10, 10,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10},
    {12, 10,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10},
    {12, 12,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12},
    // 3D Linear RGB
    { 3,  3,  3, false, GL_COMPRESSED_RGBA_ASTC_3x3x3_OES},
    { 4,  3,  3, false, GL_COMPRESSED_RGBA_ASTC_4x3x3_OES},
    { 4,  4,  3, false, GL_COMPRESSED_RGBA_ASTC_4x4x3_OES},
    { 4,  4,  4, false, GL_COMPRESSED_RGBA_ASTC_4x4x4_OES},
    { 5,  4,  4, false, GL_COMPRESSED_RGBA_ASTC_5x4x4_OES},
    { 5,  5,  4, false, GL_COMPRESSED_RGBA_ASTC_5x5x4_OES},
    { 5,  5,  5, false, GL_COMPRESSED_RGBA_ASTC_5x5x5_OES},
    { 6,  5,  5, false, GL_COMPRESSED_RGBA_ASTC_6x5x5_OES},
    { 6,  6,  5, false, GL_COMPRESSED_RGBA_ASTC_6x6x5_OES},
    { 6,  6,  6, false, GL_COMPRESSED_RGBA_ASTC_6x6x6_OES},
    // 3D SRGB
    { 3,  3,  3,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES},
    { 4,  3,  3,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES},
    { 4,  4,  3,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES},
    { 4,  4,  4,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES},
    { 5,  4,  4,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES},
    { 5,  5,  4,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES},
    { 5,  5,  5,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES},
    { 6,  5,  5,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES},
    { 6,  6,  5,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES},
    { 6,  6,  6,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES}
}};

static const format_entry* get_format(
    unsigned int format
) {
    for (auto& it : ASTC_FORMATS)
    {
        if (it.format == format)
        {
            return &it;
        }
    }
    return nullptr;
}

static unsigned int get_format(
    unsigned int x,
    unsigned int y,
    unsigned int z,
    bool is_srgb
) {
    for (auto& it : ASTC_FORMATS)
    {
        if ((it.x == x) && (it.y == y) && (it.z == z) && (it.is_srgb == is_srgb))
        {
            return it.format;
        }
    }
    return 0;
}

struct ktx_header
{
    uint8_t magic[12];
    uint32_t endianness;                // should be 0x04030201; if it is instead 0x01020304, then the endianness of everything must be switched.
    uint32_t gl_type;                   // 0 for compressed textures, otherwise value from table 3.2 (page 162) of OpenGL 4.0 spec
    uint32_t gl_type_size;              // size of data elements to do endianness swap on (1=endian-neutral data)
    uint32_t gl_format;                 // 0 for compressed textures, otherwise value from table 3.3 (page 163) of OpenGL spec
    uint32_t gl_internal_format;        // sized-internal-format, corresponding to table 3.12 to 3.14 (pages 182-185) of OpenGL spec
    uint32_t gl_base_internal_format;   // unsized-internal-format: corresponding to table 3.11 (page 179) of OpenGL spec
    uint32_t pixel_width;               // texture dimensions; not rounded up to block size for compressed.
    uint32_t pixel_height;              // must be 0 for 1D textures.
    uint32_t pixel_depth;               // must be 0 for 1D, 2D and cubemap textures.
    uint32_t number_of_array_elements;  // 0 if not a texture array
    uint32_t number_of_faces;           // 6 for cubemaps, 1 for non-cubemaps
    uint32_t number_of_mipmap_levels;   // 0 or 1 for non-mipmapped textures; 0 indicates that auto-mipmap-gen should be done at load time.
    uint32_t bytes_of_key_value_data;   // size in bytes of the key-and-value area immediately following the header.
};

// Magic 12-byte sequence that must appear at the beginning of every KTX file.
static uint8_t ktx_magic[12] {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static void ktx_header_switch_endianness(ktx_header * kt)
{
    #define REV(x) kt->x = u32_byterev(kt->x)
    REV(endianness);
    REV(gl_type);
    REV(gl_type_size);
    REV(gl_format);
    REV(gl_internal_format);
    REV(gl_base_internal_format);
    REV(pixel_width);
    REV(pixel_height);
    REV(pixel_depth);
    REV(number_of_array_elements);
    REV(number_of_faces);
    REV(number_of_mipmap_levels);
    REV(bytes_of_key_value_data);
    #undef REV
}


/**
 * @brief Load a KTX compressed image using the local custom loader.
 *
 * @param      filename          The name of the file to load.
 * @param[out] is_srgb           @c true if this is an sRGB image, @c false otherwise.
 * @param[out] img               The output image to populate.
 *
 * @return @c true on error, @c false otherwise.
 */
bool load_ktx_compressed_image(
    const wchar_t* filename,
    bool& is_srgb,
    astc_compressed_image& img,
    std::string& info
) {
    FILE *f;
    if (_wfopen_s(&f, filename, L"rb") != 0) return true;
    if (!f)
    {
        printf("Failed to open file %ls\n", filename);
        return true;
    }

    ktx_header hdr;
    size_t actual = fread(&hdr, 1, sizeof(hdr), f);
    if (actual != sizeof(hdr))
    {
        printf("Failed to read header from %ls\n", filename);
        fclose(f);
        return true;
    }

    if (memcmp(hdr.magic, ktx_magic, 12) != 0 ||
        (hdr.endianness != 0x04030201 && hdr.endianness != 0x01020304))
    {
        printf("File %ls does not have a valid KTX header\n", filename);
        fclose(f);
        return true;
    }

    bool switch_endianness = false;
    if (hdr.endianness == 0x01020304)
    {
        switch_endianness = true;
        ktx_header_switch_endianness(&hdr);
    }

    if (hdr.gl_type != 0 || hdr.gl_format != 0 || hdr.gl_type_size != 1 ||
        hdr.gl_base_internal_format != GL_RGBA)
    {
        printf("File %ls is not a compressed ASTC file\n", filename);
        fclose(f);
        return true;
    }

    const format_entry* fmt = get_format(hdr.gl_internal_format);
    if (!fmt)
    {
        printf("File %ls is not a compressed ASTC file\n", filename);
        fclose(f);
        return true;
    }

    // Skip over any key-value pairs
    int seekerr;
    seekerr = fseek(f, hdr.bytes_of_key_value_data, SEEK_CUR);
    if (seekerr)
    {
        printf("Failed to skip key-value pairs in %ls\n", filename);
        fclose(f);
        return true;
    }

    // Read the length of the data and endianess convert
    unsigned int data_len;
    actual = fread(&data_len, 1, sizeof(data_len), f);
    if (actual != sizeof(data_len))
    {
        printf("Failed to read mip 0 size from %ls\n", filename);
        fclose(f);
        return true;
    }

    if (switch_endianness)
    {
        data_len = u32_byterev(data_len);
    }

    // Read the data
    unsigned char* data = new unsigned char[data_len];
    actual = fread(data, 1, data_len, f);
    if (actual != data_len)
    {
        printf("Failed to read mip 0 data from %ls\n", filename);
        fclose(f);
        delete[] data;
        return true;
    }

    img.block_x = fmt->x;
    img.block_y = fmt->y;
    img.block_z = fmt->z == 0 ? 1 : fmt->z;

    img.dim_x = hdr.pixel_width;
    img.dim_y = hdr.pixel_height;
    img.dim_z = hdr.pixel_depth == 0 ? 1 : hdr.pixel_depth;

    img.data_len = data_len;
    img.data = data;

    is_srgb = fmt->is_srgb;

    info += std::format("gl_type: {}\n", hdr.gl_type);
    info += std::format("gl_format: {}\n", glFormatStr(hdr.gl_format));
    info += std::format("gl_internal_format: {}\n", glFormatStr(hdr.gl_internal_format));
    info += std::format("gl_base_internal_format: {}\n", glFormatStr(hdr.gl_base_internal_format));
    info += std::format("pixel_width: {}\n", hdr.pixel_width);
    info += std::format("pixel_height: {}\n", hdr.pixel_height);
    info += std::format("pixel_depth: {}\n", hdr.pixel_depth);
    info += std::format("number_of_array_elements: {}\n", hdr.number_of_array_elements);
    info += std::format("number_of_faces: {}\n", hdr.number_of_faces);
    info += std::format("number_of_mipmap_levels: {}\n", hdr.number_of_mipmap_levels);

    fclose(f);
    return false;
}
