#pragma once

#include "pixel.h"
#include "renderable.h"
#include "../math/vec2.h"

typedef struct  Texture {
   Vec2i size;
   Pixel * pixels;
} Texture;

extern int texture_init( Texture * f, Vec2i size, Pixel *);

extern Renderable texture_as_renderable( Texture * s);

extern void  texture_draw(const Texture * f, Vec2i pos, Pixel color);

extern Pixel texture_read(const Texture * f, Vec2i pos);

extern Pixel texture_readF(const Texture * f, Vec2f pos);

