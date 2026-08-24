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
