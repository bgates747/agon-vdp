#include "texture.h"
#include "math.h"

int texture_init( Texture *f, Vec2i size, Pixel *buf )
{
    if(size.x * size.y == 0)
        return 1; // 0 sized rect

    if(buf == 0)
        return 2; // null ptr buffer

    f->frameBuffer = (Pixel *)buf;
    f->size = size;

    return 0;
}

void texture_draw(Texture *f, Vec2i pos, Pixel color)
{
    f->frameBuffer[pos.x + pos.y * f->size.x] = color;
}

Pixel texture_read(Texture *f, Vec2i pos)
{
    return f->frameBuffer[pos.x + pos.y * f->size.x];
}

Pixel texture_readF(Texture *f, Vec2f pos)
{
    float u = fminf(fmaxf(pos.x, 0.0f), 1.0f);
    float v = fminf(fmaxf(pos.y, 0.0f), 1.0f);
    uint16_t x = (uint16_t)(u * (f->size.x - 1));
    // UV V grows upward; texture memory begins with the top image row.
    uint16_t y = (uint16_t)((1.0f - v) * (f->size.y - 1));
    uint32_t index = x + y * f->size.x;
    Pixel value = f->frameBuffer[index];
    return value;
}



