#pragma once

#ifdef WICLOADERLIB_EXPORTS
#define WICLOADERLIB_API __declspec(dllexport)
#else
#define WICLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    WICLOADERLIB_API const char *get_version();
    WICLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult, uint32_t opt);
    WICLOADERLIB_API int save_jxr(const wchar_t *file, unsigned int width, unsigned int height, int dataSize, void *data, uint32_t opt);
    WICLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
