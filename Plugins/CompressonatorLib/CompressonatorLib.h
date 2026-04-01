#pragma once

#ifdef COMPRESSONATORLIB_EXPORTS
#define COMPRESSONATOR_API __declspec(dllexport)
#else
#define COMPRESSONATOR_API __declspec(dllimport)
#endif

#include "../LoadResult.h"

extern "C" {
	COMPRESSONATOR_API const char *get_version();
	COMPRESSONATOR_API int load_file(const wchar_t *file, LOAD_RESULT *outResult);
	COMPRESSONATOR_API void free_mem(LOAD_RESULT *result);
}
