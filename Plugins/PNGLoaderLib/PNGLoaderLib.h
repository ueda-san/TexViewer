#pragma once

#ifdef PNGLOADERLIB_EXPORTS
#define PNGLOADERLIB_API __declspec(dllexport)
#else
#define PNGLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    PNGLOADERLIB_API const char *get_version();
    PNGLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult, uint32_t opt);
    PNGLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
