#include "texture.h"
#include "math.h"

int texture_init( Texture *f, Vec2i size, Pixel *buf )
{
    if(size.x * size.y == 0)
        return 1; // 0 sized rect

    if(buf == 0)
        return 2; // null ptr buffer

    f->pixels = (Pixel *)buf;
    f->size = size;

    return 0;
}

void texture_draw(const Texture *f, Vec2i pos, Pixel color)
{
    f->pixels[pos.x + pos.y * f->size.x] = color;
}

Pixel texture_read(const Texture *f, Vec2i pos)
{
    return f->pixels[pos.x + pos.y * f->size.x];
}

Pixel texture_readF(const Texture *f, Vec2f pos)
{
    uint16_t x = (uint16_t)(pos.x * f->size.x) % f->size.x;
    uint16_t y = (uint16_t)(pos.y * f->size.y) % f->size.x;
    uint32_t index = x + y * f->size.x;
    Pixel value = f->pixels[index];
    return value;
}




