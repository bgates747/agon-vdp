#pragma once

#include <stdint.h>
#include <stdlib.h>

//What format to use [ UINT8 | RGB565 | RGBA8888 | BGRA8888 | RGB888 | RGBA2222P ]
// #define BGRA8888
#define RGBA2222P

//Formats definitions:
#ifdef RGBA2222P
typedef struct tag_Pixel {
    uint8_t c;
} Pixel;

#ifdef __cplusplus
static_assert(sizeof(Pixel) == 1, "Pingo working pixels must be one byte");
#else
_Static_assert(sizeof(Pixel) == 1, "Pingo working pixels must be one byte");
#endif

#define PIXELBLACK (Pixel){0xC0}
#define PIXELWHITE (Pixel){0xFF}

static inline Pixel pixelMulInline(Pixel p, float f)
{
    uint8_t r = (uint8_t)((p.c & 0x03) * 85 * f);
    uint8_t g = (uint8_t)(((p.c >> 2) & 0x03) * 85 * f);
    uint8_t b = (uint8_t)(((p.c >> 4) & 0x03) * 85 * f);
    return (Pixel){
        (uint8_t)(
            (p.c & 0xC0) |
            ((b >> 6) << 4) |
            ((g >> 6) << 2) |
            (r >> 6))
    };
}
#endif

#ifdef UINT8
typedef struct tag_Pixel {
    uint8_t g;
}Pixel;
#define PIXELBLACK (Pixel){0}
#define PIXELWHITE (Pixel){255}
#endif

#ifdef RGB565
typedef struct tag_Pixel {
    uint8_t red:5;
    uint8_t green:6;
    uint8_t blue:5;
}Pixel;
#define PIXELBLACK (Pixel){0}
#define PIXELWHITE (Pixel){255}
#endif

#ifdef RGB888
typedef struct tag_Pixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Pixel;

#define PIXELBLACK (Pixel){0,0,0}
#define PIXELWHITE (Pixel){255,255,255}
#endif

#ifdef RGBA8888
typedef struct tag_Pixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Pixel;

#define PIXELBLACK (Pixel){0,0,0,255}
#define PIXELWHITE (Pixel){255,255,255,255}
#endif

#ifdef BGRA8888
typedef struct tag_Pixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Pixel;

#define PIXELBLACK (Pixel){0,0,0,255}
#define PIXELWHITE (Pixel){255,255,255,255}
#endif



//Interface 
extern Pixel pixelRandom();
extern Pixel pixelFromUInt8( uint8_t);
extern uint8_t pixelToUInt8( Pixel *);
extern Pixel pixelFromRGBA( uint8_t r, uint8_t g, uint8_t b, uint8_t a);
extern Pixel pixelFromRGBA8888( uint32_t rgba8888);
extern uint32_t pixelToRGBA8888( Pixel pixel);
extern Pixel pixelMul( Pixel p, float f);
