#pragma once

#include "pixel.h"
#include "renderable.h"
#include "../math/vec2.h"

typedef uint8_t TextureFormat;
enum {
   TEXTURE_FORMAT_RGBA8888 = 0,
   TEXTURE_FORMAT_RGBA2222 = 1
};

typedef struct  Texture {
   Vec2i size;
   Pixel * frameBuffer;
   TextureFormat format;
} Texture;

extern int texture_init( Texture * f, Vec2i size, Pixel *);

extern int texture_init_format(Texture * f, Vec2i size, void *, TextureFormat);

extern Renderable texture_as_renderable( Texture * s);

extern void  texture_draw(Texture * f, Vec2i pos, Pixel color);

extern Pixel texture_read(Texture * f, Vec2i pos);

extern Pixel texture_readF(Texture * f, Vec2f pos);
