#ifndef PINGO_PLATFORM_H
#define PINGO_PLATFORM_H

#include <stddef.h>

#ifdef USERSPACE

#ifdef __cplusplus
extern "C" {
#endif

void *pingo_platform_alloc(size_t size);
void pingo_platform_free(void *ptr);

#ifdef __cplusplus
}
#endif

#else

#include <esp_heap_caps.h>

static inline void *pingo_platform_alloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

static inline void pingo_platform_free(void *ptr) {
    heap_caps_free(ptr);
}

#endif

#endif
