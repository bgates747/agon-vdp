#include "texture.h"

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
    return texture_init_format(f, size, buf, TEXTURE_FORMAT_RGBA2222);
}

static Pixel texture_read_index(Texture *f, uint32_t index)
{
    if (f->format == TEXTURE_FORMAT_RGBA2222) {
        return (Pixel){((uint8_t *)f->frameBuffer)[index]};
    }

    const uint8_t *rgba = ((const uint8_t *)f->frameBuffer) + index * 4;
    return pixelFromRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void texture_draw(Texture *f, Vec2i pos, Pixel color)
{
    // Renderer destinations use Pingo's native one-byte working format.
    f->frameBuffer[pos.x + pos.y * f->size.x] = color;
}

Pixel texture_read(Texture *f, Vec2i pos)
{
    return texture_read_index(f, pos.x + pos.y * f->size.x);
}

static inline float texture_clamp_coordinate(float value)
{
    // Match fminf(fmaxf(value, 0), 1), including its NaN-to-zero result.
    if (!(value >= 0.0f))
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

Pixel texture_readF(Texture *f, Vec2f pos)
{
    float u = texture_clamp_coordinate(pos.x);
    float v = texture_clamp_coordinate(pos.y);
    uint16_t x = (uint16_t)(u * (f->size.x - 1));
    // UV V grows upward; texture memory begins with the top image row.
    uint16_t y = (uint16_t)((1.0f - v) * (f->size.y - 1));
    uint32_t index = x + y * f->size.x;
    return texture_read_index(f, index);
}
