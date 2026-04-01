#pragma once

#ifdef ETCPACKLOADERLIB_EXPORTS
#define ETCPACKLOADERLIB_API __declspec(dllexport)
#else
#define ETCPACKLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    ETCPACKLOADERLIB_API const char *get_version();
    ETCPACKLOADERLIB_API int load_byte(const unsigned char *data, int size, LOAD_RESULT *outResult);
    ETCPACKLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
