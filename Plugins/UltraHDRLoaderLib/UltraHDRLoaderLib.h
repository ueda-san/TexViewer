#pragma once

#ifdef ULTRAHDRLOADERLIB_EXPORTS
#define ULTRAHDRLOADERLIB_API __declspec(dllexport)
#else
#define ULTRAHDRLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    ULTRAHDRLOADERLIB_API const char *get_version();
    ULTRAHDRLOADERLIB_API int load_byte(const unsigned char *data, int size, LOAD_RESULT *outResult, uint32_t opt);
    ULTRAHDRLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
