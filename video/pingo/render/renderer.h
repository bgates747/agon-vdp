#pragma once

#include "texture.h"
#include "renderable.h"
#include "pixel.h"
#include "../math/vec4.h"
#include "../math/vec3.h"

#ifndef PINGO_RENDER_DIAGNOSTICS
#define PINGO_RENDER_DIAGNOSTICS 0
#endif
#if PINGO_RENDER_DIAGNOSTICS != 0 && PINGO_RENDER_DIAGNOSTICS != 1
#error "PINGO_RENDER_DIAGNOSTICS must be 0 or 1"
#endif

typedef struct tag_Scene Scene;
typedef struct tag_BackEnd BackEnd;

enum {
    PINGO_FLAT_PATTERN_WIDTH = 4,
    PINGO_FLAT_PATTERN_HEIGHT = 4,
    PINGO_FLAT_PATTERN_PIXELS =
        PINGO_FLAT_PATTERN_WIDTH * PINGO_FLAT_PATTERN_HEIGHT,
    PINGO_FLAT_PATTERN_MAX_PATTERNS = 256
};

/*
 * Borrowed, renderer-facing view of a precomputed flat-pattern resource.
 *
 * `patterns` contains pattern_count consecutive 4x4 opaque native RGBA2222
 * images. `lookup` is material-major and contains one uint8 pattern ID for
 * every material/illumination-band pair; at least two endpoint-inclusive
 * bands are required. The renderer never owns either allocation; both must
 * remain stable for every render using this binding.
 */
typedef struct tag_PingoFlatPatternLibrary {
    const uint8_t * patterns;
    uint32_t patterns_size;
    const uint8_t * lookup;
    uint32_t lookup_size;
    uint16_t pattern_count;
    uint8_t material_count;
    uint8_t illumination_band_count;
} PingoFlatPatternLibrary;

#if PINGO_RENDER_DIAGNOSTICS
typedef uint32_t (*RendererDiagnosticsClock)(void);

typedef struct tag_RendererDiagnostics {
    uint64_t clear_ticks;
    uint64_t transform_ticks;
    uint64_t triangle_setup_ticks;
    uint64_t raster_ticks;

    uint32_t objects;
    uint32_t objects_bounds_tested;
    uint32_t objects_frustum_rejected;
    uint32_t triangles_avoided;
    uint32_t triangles_submitted;
    uint32_t triangles_z_rejected;
    uint32_t triangles_frustum_rejected;
    uint32_t triangles_clipped;
    uint32_t triangles_unclipped;
    uint32_t triangles_generated;
    uint32_t triangles_projection_rejected;
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
    int frustumCulling;

    /* Scene-wide directional illumination. Unity is encoded as 127. */
    Vec3f lightDirection;
    float lightIntensity;
    float ambientLight;
    int illuminationEnabled;

    PingoFlatPatternLibrary flatPatternLibrary;
    uint8_t flatPatternLibraryValid;

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

/*
 * Stream a frame without first materializing a Scene renderable array.
 *
 * rendererBeginFrame() resets diagnostics, clears the depth/color targets,
 * and calls the backend's beforeRender hook. Submit zero or more renderables
 * with rendererRenderRenderable(), then call rendererEndFrame() exactly once
 * to run the backend's afterRender hook. The caller owns traversal and the
 * lifetime of every submitted renderable.
 *
 * rendererRender() remains the compatibility entry point for a bound Scene.
 */
extern int rendererBeginFrame(Renderer *);

extern void rendererRenderRenderable(
    Renderer *, Mat4 transform, Renderable renderable);

extern int rendererEndFrame(Renderer *);

extern int rendererRender(Renderer *);

extern int rendererInit(Renderer *, Vec2i size, struct tag_BackEnd * backEnd);

extern int rendererSetScene(Renderer *r, Scene *s);

extern int rendererSetCamera(Renderer *r, Vec4i camera);

/*
 * Toggle the conservative per-object bounds test. Per-triangle homogeneous
 * rejection and clipping are correctness requirements and remain active.
 */
extern void rendererSetFrustumCulling(Renderer *r, int enabled);

/* Returns nonzero and leaves the previous value intact for an invalid vector. */
extern int rendererSetLightDirection(Renderer *r, Vec3f direction);

extern void rendererSetLightIntensity(Renderer *r, uint8_t intensity);

extern void rendererSetAmbientLight(Renderer *r, uint8_t ambient);

extern void rendererSetIlluminationEnabled(Renderer *r, int enabled);

/*
 * Bind borrowed flat-pattern data after validating sizes and every lookup ID.
 * A NULL library clears the binding. An invalid non-NULL candidate returns
 * nonzero without changing the previous binding.
 */
extern int rendererSetFlatPatternLibrary(
    Renderer *r, const PingoFlatPatternLibrary *library);
