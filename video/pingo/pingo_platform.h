#ifndef PINGO_PLATFORM_H
#define PINGO_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef USERSPACE

#ifdef __cplusplus
extern "C" {
#endif

void *pingo_platform_alloc(size_t size);
void pingo_platform_free(void *ptr);
void pingo_platform_rgba2222_frame_ready(
    const uint8_t *pixels, uint16_t width, uint16_t height);

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

static inline void pingo_platform_rgba2222_frame_ready(
    const uint8_t *pixels, uint16_t width, uint16_t height) {
    (void)pixels;
    (void)width;
    (void)height;
}

#endif

#endif
