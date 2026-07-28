#include "texture.h"
#include "math.h"

// Shared RGBA2222 (AABBGGRR) to RGBA8888 expansion table. Keeping this const
// places it in firmware read-only storage rather than allocating a table for
// every texture. The row macro keeps all 256 compile-time entries auditable
// without runtime initialization.
#define RGBA2222_PIXEL(value) { \
    (uint8_t)(((value) & 0x03) * 85), \
    (uint8_t)((((value) >> 2) & 0x03) * 85), \
    (uint8_t)((((value) >> 4) & 0x03) * 85), \
    (uint8_t)((((value) >> 6) & 0x03) * 85) \
}
#define RGBA2222_ROW(high) \
    RGBA2222_PIXEL((high) | 0x0), RGBA2222_PIXEL((high) | 0x1), \
    RGBA2222_PIXEL((high) | 0x2), RGBA2222_PIXEL((high) | 0x3), \
    RGBA2222_PIXEL((high) | 0x4), RGBA2222_PIXEL((high) | 0x5), \
    RGBA2222_PIXEL((high) | 0x6), RGBA2222_PIXEL((high) | 0x7), \
    RGBA2222_PIXEL((high) | 0x8), RGBA2222_PIXEL((high) | 0x9), \
    RGBA2222_PIXEL((high) | 0xA), RGBA2222_PIXEL((high) | 0xB), \
    RGBA2222_PIXEL((high) | 0xC), RGBA2222_PIXEL((high) | 0xD), \
    RGBA2222_PIXEL((high) | 0xE), RGBA2222_PIXEL((high) | 0xF)

static const Pixel rgba2222_to_rgba8888[256] = {
    RGBA2222_ROW(0x00), RGBA2222_ROW(0x10),
    RGBA2222_ROW(0x20), RGBA2222_ROW(0x30),
    RGBA2222_ROW(0x40), RGBA2222_ROW(0x50),
    RGBA2222_ROW(0x60), RGBA2222_ROW(0x70),
    RGBA2222_ROW(0x80), RGBA2222_ROW(0x90),
    RGBA2222_ROW(0xA0), RGBA2222_ROW(0xB0),
    RGBA2222_ROW(0xC0), RGBA2222_ROW(0xD0),
    RGBA2222_ROW(0xE0), RGBA2222_ROW(0xF0)
};

#undef RGBA2222_ROW
#undef RGBA2222_PIXEL

int texture_init_format(Texture *f, Vec2i size, void *buf, TextureFormat format)
{
    if(size.x * size.y == 0)
        return 1; // 0 sized rect

    if(buf == 0)
        return 2; // null ptr buffer

    if(format != TEXTURE_FORMAT_RGBA8888 &&
       format != TEXTURE_FORMAT_RGBA2222)
        return 3; // unsupported source format

    f->frameBuffer = (Pixel *)buf;
    f->size = size;
    f->format = format;

    return 0;
}

int texture_init(Texture *f, Vec2i size, Pixel *buf)
{
    return texture_init_format(f, size, buf, TEXTURE_FORMAT_RGBA8888);
}

static Pixel texture_read_index(Texture *f, uint32_t index)
{
    if (f->format == TEXTURE_FORMAT_RGBA2222) {
        uint8_t packed = ((uint8_t *)f->frameBuffer)[index];
        return rgba2222_to_rgba8888[packed];
    }

    return f->frameBuffer[index];
}

void texture_draw(Texture *f, Vec2i pos, Pixel color)
{
    f->frameBuffer[pos.x + pos.y * f->size.x] = color;
}

Pixel texture_read(Texture *f, Vec2i pos)
{
    return texture_read_index(f, pos.x + pos.y * f->size.x);
}

Pixel texture_readF(Texture *f, Vec2f pos)
{
    float u = fminf(fmaxf(pos.x, 0.0f), 1.0f);
    float v = fminf(fmaxf(pos.y, 0.0f), 1.0f);
    uint16_t x = (uint16_t)(u * (f->size.x - 1));
    // UV V grows upward; texture memory begins with the top image row.
    uint16_t y = (uint16_t)((1.0f - v) * (f->size.y - 1));
    uint32_t index = x + y * f->size.x;
    return texture_read_index(f, index);
}

