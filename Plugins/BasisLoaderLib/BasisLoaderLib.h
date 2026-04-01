#pragma once

#ifdef BASISLOADERLIB_EXPORTS
#define BASISLOADERLIB_API __declspec(dllexport)
#else
#define BASISLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    BASISLOADERLIB_API const char *get_version();
    BASISLOADERLIB_API int load_byte(const unsigned char *data, int size, LOAD_RESULT *outResult);
    BASISLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
