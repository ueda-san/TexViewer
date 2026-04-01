#pragma once

#ifdef PVRLOADERLIB_EXPORTS
#define PVRLOADERLIB_API __declspec(dllexport)
#else
#define PVRLOADERLIB_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
    PVRLOADERLIB_API const char *get_version();
    PVRLOADERLIB_API int load_file(const wchar_t *file, LOAD_RESULT *outResult);
    PVRLOADERLIB_API void free_mem(LOAD_RESULT *result);
}
