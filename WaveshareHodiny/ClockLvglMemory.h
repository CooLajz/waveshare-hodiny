#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *clockLvglAlloc(size_t size);
void *clockLvglRealloc(void *pointer, size_t size);
void clockLvglFree(void *pointer);

#ifdef __cplusplus
}
#endif
