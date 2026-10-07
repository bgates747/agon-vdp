#pragma once

#include "math/vec2.h"
#include "pixel.h"
#include "renderable.h"

typedef struct Texture {
    Vec2i size;
    Pixel *frameBuffer;
} Texture;

extern int texture_init(Texture *f, Vec2i size, Pixel *);
extern int texture_init_rgbafile(Texture *f, Vec2i size, char *filename);
extern Renderable texture_as_renderable(Texture *s);
extern void texture_draw(Texture *f, Vec2i pos, Pixel color);
extern Pixel texture_read(Texture *f, Vec2i pos);
extern Pixel texture_readF(Texture *f, Vec2f pos);

/* Same sampling result as fminf(fmaxf(value, 0), 1), including NaN -> 0.
 * Keep comparisons visible to the compiler without requiring fast-math. */
static inline float textureClampUnit(float value) {
    if (!(value >= 0.0f)) return 0.0f;
    return value > 1.0f ? 1.0f : value;
}

static inline Pixel textureReadFInline(const Texture *f, Vec2f pos) {
    float u = textureClampUnit(pos.x);
    float v = textureClampUnit(pos.y);
    uint16_t x = (uint16_t)(u * (f->size.x - 1));
    uint16_t y = (uint16_t)((1.0f - v) * (f->size.y - 1));
    uint32_t index = x + y * f->size.x;
    return f->frameBuffer[index];
}
