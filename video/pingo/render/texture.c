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

void texture_draw(Texture *f, Vec2i pos, Pixel color)
{
    // Renderer destinations use Pingo's native one-byte working format.
    f->frameBuffer[pos.x + pos.y * f->size.x] = color;
}

Pixel texture_read(Texture *f, Vec2i pos)
{
    return texture_read_index_inline(
        f, pos.x + pos.y * f->size.x);
}

Pixel texture_readF(Texture *f, Vec2f pos)
{
    return texture_readFInline(f, pos);
}
