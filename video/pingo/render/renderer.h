#pragma once

#include "texture.h"
#include "renderable.h"
#include "pixel.h"
#include "../math/vec4.h"

#ifndef PINGO_RENDER_DIAGNOSTICS
#define PINGO_RENDER_DIAGNOSTICS 0
#endif
#if PINGO_RENDER_DIAGNOSTICS != 0 && PINGO_RENDER_DIAGNOSTICS != 1
#error "PINGO_RENDER_DIAGNOSTICS must be 0 or 1"
#endif

typedef struct tag_Scene Scene;
typedef struct tag_BackEnd BackEnd;

#if PINGO_RENDER_DIAGNOSTICS
typedef uint32_t (*RendererDiagnosticsClock)(void);

typedef struct tag_RendererDiagnostics {
    uint64_t clear_ticks;
    uint64_t transform_ticks;
    uint64_t triangle_setup_ticks;
    uint64_t raster_ticks;

    uint32_t objects;
    uint32_t triangles_submitted;
    uint32_t triangles_z_rejected;
    uint32_t triangles_backface_rejected;
    uint32_t triangles_degenerate;
    uint32_t triangles_bbox_rejected;
    uint32_t triangles_rasterized;
    uint32_t triangles_bbox_clamped;

    uint64_t fragments_bbox;
    uint64_t fragments_covered;
    uint64_t fragments_depth_range_rejected;
    uint64_t fragments_depth_test_rejected;
    uint64_t fragments_reciprocal_w_rejected;
    uint64_t fragments_shaded;
} RendererDiagnostics;
#endif

typedef struct tag_Renderer{
    Vec4i camera;
    Scene * scene;

    Texture frameBuffer;
    Pixel clearColor;
    int clear;

    Mat4 camera_projection;
    Mat4 camera_view;

    BackEnd * backEnd;

#if PINGO_RENDER_DIAGNOSTICS
    /*
     * The bridge supplies a cheap wrapping tick counter and its frequency.
     * Keeping that platform detail outside Pingo's C renderer makes the
     * instrumentation usable in both the ESP32 and native builds.
     */
    RendererDiagnosticsClock diagnostics_clock;
    uint32_t diagnostics_clock_hz;
    RendererDiagnostics diagnostics;
#endif

} Renderer;

extern int rendererRender(Renderer *);

extern int rendererInit(Renderer *, Vec2i size, struct tag_BackEnd * backEnd);

extern int rendererSetScene(Renderer *r, Scene *s);

extern int rendererSetCamera(Renderer *r, Vec4i camera);
