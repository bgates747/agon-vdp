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

static inline Pixel texture_read_index_inline(
      const Texture * f, uint32_t index)
{
   if (f->format == TEXTURE_FORMAT_RGBA2222) {
      return (Pixel){((const uint8_t *)f->frameBuffer)[index]};
   }

   const uint8_t * rgba =
      ((const uint8_t *)f->frameBuffer) + index * 4;
   return pixelFromRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
}

static inline float texture_clamp_coordinate_inline(float value)
{
   // Match fminf(fmaxf(value, 0), 1), including its NaN-to-zero result.
   if (!(value >= 0.0f))
      return 0.0f;
   if (value > 1.0f)
      return 1.0f;
   return value;
}

static inline Pixel texture_readFInline(
      const Texture * f, Vec2f pos)
{
   float u = texture_clamp_coordinate_inline(pos.x);
   float v = texture_clamp_coordinate_inline(pos.y);
   uint16_t x = (uint16_t)(u * (f->size.x - 1));
   // UV V grows upward; texture memory begins with the top image row.
   uint16_t y = (uint16_t)((1.0f - v) * (f->size.y - 1));
   uint32_t index = x + y * f->size.x;
   return texture_read_index_inline(f, index);
}

extern int texture_init( Texture * f, Vec2i size, Pixel *);

extern int texture_init_format(Texture * f, Vec2i size, void *, TextureFormat);

extern Renderable texture_as_renderable( Texture * s);

extern void  texture_draw(Texture * f, Vec2i pos, Pixel color);

extern Pixel texture_read(Texture * f, Vec2i pos);

extern Pixel texture_readF(Texture * f, Vec2f pos);
