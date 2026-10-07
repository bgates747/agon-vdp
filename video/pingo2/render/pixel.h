#pragma once

#include <stdint.h>
#include <stdlib.h>

#if defined(P2C_PIXEL_BGRA8888) && defined(P2C_PIXEL_RGBA2222)
#error "select exactly one Pingo 2 pixel format"
#elif !defined(P2C_PIXEL_BGRA8888) && !defined(P2C_PIXEL_RGBA2222)
#define P2C_PIXEL_BGRA8888 1
#endif

#ifdef P2C_PIXEL_RGBA2222
typedef struct Pixel {
    uint8_t c;
} Pixel;

#define PIXELBLACK (Pixel){0xC0}
#define PIXELWHITE (Pixel){0xFF}
#else
typedef struct Pixel {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} Pixel;

#define PIXELBLACK (Pixel){0, 0, 0, 255}
#define PIXELWHITE (Pixel){255, 255, 255, 255}
#endif

#ifdef P2C_PIXEL_RGBA2222
_Static_assert(sizeof(Pixel) == 1, "RGBA2222 Pixel must be one byte");
#else
_Static_assert(sizeof(Pixel) == 4, "BGRA8888 Pixel must be four bytes");
#endif

Pixel pixelRandom(void);
Pixel pixelFromUInt8(uint8_t gray);
uint8_t pixelToUInt8(Pixel *pixel);
Pixel pixelFromRGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
Pixel pixelMul(Pixel pixel, float factor);

/* R02 supplies a finite nonnegative factor, up to 255/127. Saturation before
 * conversion avoids out-of-range float-to-byte conversion under overdrive. */
static inline uint8_t pixelLitChannel(uint8_t value, float factor) {
    float result = value * factor;
    return result >= 255.0f ? 255u : (uint8_t)result;
}

/* The retained lighting contract supplies a finite factor in [0,255/127]. Keep
 * expansion, float multiplication, truncation and requantization in exactly
 * that order. This is also the implementation of the external ABI wrapper. */
static inline Pixel pixelMulInline(Pixel pixel, float factor) {
#ifdef P2C_PIXEL_RGBA2222
    uint8_t red = pixelLitChannel((pixel.c & 3u) * 85u, factor);
    uint8_t green = pixelLitChannel(((pixel.c >> 2) & 3u) * 85u, factor);
    uint8_t blue = pixelLitChannel(((pixel.c >> 4) & 3u) * 85u, factor);
    return (Pixel){(uint8_t)((pixel.c & 0xC0u) | ((blue >> 6) << 4) |
                            ((green >> 6) << 2) | (red >> 6))};
#else
    return (Pixel){pixelLitChannel(pixel.b, factor),
                   pixelLitChannel(pixel.g, factor),
                   pixelLitChannel(pixel.r, factor), pixel.a};
#endif
}

#ifdef P2C_PIXEL_RGBA2222
/* Each channel has only four inputs. Preserve R01's exact float multiplication
 * and uint8 truncation once per triangle, not once per shaded component. */
static inline void pixelShadePrepare(uint8_t levels[4], float factor) {
    levels[0] = 0;
    for (unsigned i = 1; i < 4; ++i)
        levels[i] = pixelLitChannel(i * 85u, factor) >> 6;
}

static inline Pixel pixelShadeLookup(Pixel pixel, const uint8_t levels[4]) {
    return (Pixel){(uint8_t)((pixel.c & 0xC0u) |
        (levels[(pixel.c >> 4) & 3u] << 4) |
        (levels[(pixel.c >> 2) & 3u] << 2) | levels[pixel.c & 3u])};
}
#endif
