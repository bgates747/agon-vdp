#pragma once

#define PINGO_LIGHTING_CONTROLS 1

#include "pixel.h"
#include "texture.h"
#include <stdbool.h>

typedef struct Backend Backend;

typedef struct Renderer {
  Renderable *root_renderable;

  Texture framebuffer;
  Pixel clear_color;
  bool clear;

  Mat4 camera_projection;
  Mat4 camera_view;

  /* Frame-owned inverse of camera_view, refreshed by renderer_render(). */
  Mat4 prepared_view;

  Backend *backend;

  Vec3f light_direction; /* World-fixed direction; transformed with w=0. */
  float light_intensity, ambient_light; /* Finite 0..255/127. */
  bool illumination_enabled;

} Renderer;

extern int renderer_render(Renderer *);

extern int renderer_init(Renderer *, Vec2i size, Backend *backend);

extern int renderer_set_root_renderable(Renderer *renderer, Renderable *root);

/* Return zero on success. Reject nonfinite/zero input without changing state.
 * Scale before normalization so large/small finite directions are safe. */
extern int renderer_set_light_direction(Renderer *renderer, Vec3f direction);
