#pragma once

#include "../math/vec4.h"
#include "depth.h"
#include "texture.h"
#include "renderable.h"
#include "pixel.h"
#include "camera.h"

typedef struct tag_Scene Scene;
typedef struct tag_BackEnd BackEnd;

typedef enum {
    REND_NO_CLEAR = 0,  // 0: No clearing
    REND_CLEAR = 1,     // 1: Clear the frameBuffer
    REND_BACKGROUND = 2 // 2: Clear with a background texture
} RenderClearType;

typedef struct Renderer{
    Camera camera;
    Scene * scene;

    PingoDepth * z_buffer;

    Texture frameBuffer;
    
    int clear;
    Pixel clearColor;
    Texture background;

    BackEnd * backEnd;

} Renderer;

extern int rendererRender(Renderer *);

extern int rendererInit(Renderer *, Vec2i size, BackEnd * backEnd);

extern int rendererSetScene(Renderer *r, Scene *s);

extern int rendererSetCamera(Renderer *r, Vec2i camera);

int renderFrame(Renderer *r, Renderable ren);

int renderSprite(Mat4 transform, Renderer *r, Renderable ren);

void renderRenderable(Mat4 transform, Renderer *r, Renderable ren);

int renderScene(Mat4 transform, Renderer *r, Renderable ren);

int renderObject(Mat4 object_transform, Renderer *r, Renderable ren);

float isClockWise(float x1, float y1, float x2, float y2, float x3, float y3);

int orient2d( Vec2i a,  Vec2i b,  Vec2i c);

void backendDrawPixel (Renderer * r, Texture * f, Vec2i pos, Pixel color, float illumination);


// SCRATCHPIXEL FUNCTIONS
// https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/perspective-correct-interpolation-vertex-attributes.html
static inline void persp_divide(struct Vec3f* p);
static inline void to_raster(const Vec2i size, struct Vec3f* const p);
static inline void tri_bbox(const Vec3f* const p0, const Vec3f* const p1, const Vec3f* const p2, float* const bbox);
static inline float edge(const Vec3f* const a, const Vec3f* const b, const Vec3f* const test);
static Pixel shade(const Texture* texture, Vec2f uv);
static inline void rasterize(int x0, int y0, int x1, int y1, const Vec3f* const p0, const Vec3f* const p1, const Vec3f* const p2, const Vec2f* const uv0, const Vec2f* const uv1, const Vec2f* const uv2, const Texture* const texture, const Vec2i scrSize, Renderer* r, float near, float diffuseLight);
static inline int clip_edge(float y, const Vec3f* const v0, const Vec3f* const v1, Vec2f* out);
static void find_scanline_intersections(const Vec3f* p0, const Vec3f* p1, const Vec3f* p2, int scanline_y, Vec2f* out_intersections, int* count);