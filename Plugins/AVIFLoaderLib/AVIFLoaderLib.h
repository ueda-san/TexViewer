#pragma once

#ifdef AVIFLOADERLIB_EXPORTS
#define AVIFLOADERLIB_API __declspec(dllexport)
#else
#define AVIFLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    AVIFLOADERLIB_API const char *get_version();
    AVIFLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult, uint32_t opt);
    AVIFLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
