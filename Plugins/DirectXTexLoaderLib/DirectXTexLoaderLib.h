#pragma once

#ifdef DIRECTXTEXLOADERLIB_EXPORTS
#define DIRECTXTEXLOADERLIB_API __declspec(dllexport)
#else
#define DIRECTXTEXLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    DIRECTXTEXLOADERLIB_API const char *get_version();
    DIRECTXTEXLOADERLIB_API int load_dds(const wchar_t *file, LOAD_RESULT *outResult);
    DIRECTXTEXLOADERLIB_API int load_hdr(const wchar_t *file, LOAD_RESULT *outResult);
    DIRECTXTEXLOADERLIB_API int load_tga(const wchar_t *file, LOAD_RESULT *outResult);
    DIRECTXTEXLOADERLIB_API int load_wic(const wchar_t *file, LOAD_RESULT *outResult);
    DIRECTXTEXLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
