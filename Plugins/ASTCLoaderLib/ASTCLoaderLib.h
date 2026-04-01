#pragma once

#ifdef ASTCLOADERLIB_EXPORTS
#define ASTCLOADERLIB_API __declspec(dllexport)
#else
#define ASTCLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    ASTCLOADERLIB_API const char *get_version();
    ASTCLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult);
    ASTCLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
