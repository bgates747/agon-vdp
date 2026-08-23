#include "pingo2_target_probe.h"

#ifdef PINGO2_TARGET_PROBE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pingo2/math/mat4.h"
#include "pingo2/render/backend.h"
#include "pingo2/render/depth.h"
#include "pingo2/render/entity.h"
#include "pingo2/render/material.h"
#include "pingo2/render/mesh.h"
#include "pingo2/render/object.h"
#include "pingo2/render/pixel.h"
#include "pingo2/render/renderer.h"
#include "pingo2/render/state.h"
#include "pingo2/render/texture.h"

enum {
    PROBE_WIDTH = 32,
    PROBE_HEIGHT = 24,
    PROBE_PIXELS = PROBE_WIDTH * PROBE_HEIGHT,
};

static Pixel probe_framebuffer[PROBE_PIXELS];
static PingoDepth probe_depth[PROBE_PIXELS];

static void probe_backend_init(Renderer *renderer, Backend *backend, Vec4i rect) {
    (void)renderer;
    (void)backend;
    (void)rect;
}

static void probe_backend_before(Renderer *renderer, Backend *backend) {
    (void)renderer;
    (void)backend;
}

static void probe_backend_after(Renderer *renderer, Backend *backend) {
    (void)renderer;
    (void)backend;
}

static Pixel *probe_backend_framebuffer(Renderer *renderer, Backend *backend) {
    (void)renderer;
    (void)backend;
    return probe_framebuffer;
}

static PingoDepth *probe_backend_depth(Renderer *renderer, Backend *backend) {
    (void)renderer;
    (void)backend;
    return probe_depth;
}

static int probe_root_render(void *payload, Mat4 transform, Renderer *renderer) {
    (void)payload;
    (void)transform;
    (void)renderer;
    return OK;
}

static uint32_t fnv1a(const void *data, size_t bytes) {
    const uint8_t *cursor = (const uint8_t *)data;
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < bytes; ++index) {
        hash ^= cursor[index];
        hash *= UINT32_C(16777619);
    }
    return hash;
}

int pingo2_target_probe_run(Pingo2TargetProbeResult *result) {
    static Vec3f positions[3] = {
        {-2.0f, -1.5f, -6.0f},
        { 2.0f, -1.5f, -6.0f},
        { 0.0f,  1.5f, -6.0f},
    };
    static uint16_t position_indices[3] = {0, 1, 2};
    static Vec2f texcoords[3] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.5f, 1.0f},
    };
    static uint16_t texcoord_indices[3] = {0, 1, 2};
    static Pixel texture_pixels[4];
    Backend backend;
    Renderer renderer;
    Renderable root_payload;
    Entity root;
    Entity child;
    Mesh mesh;
    Texture texture;
    Material material;
    Object object;
    Pixel clear_color;

    if (result == NULL) {
        return INIT_ERROR;
    }
    memset(result, 0, sizeof(*result));
    memset(probe_framebuffer, 0, sizeof(probe_framebuffer));
    memset(probe_depth, 0, sizeof(probe_depth));

    backend.init = probe_backend_init;
    backend.beforeRender = probe_backend_before;
    backend.afterRender = probe_backend_after;
    backend.getFrameBuffer = probe_backend_framebuffer;
    backend.getZetaBuffer = probe_backend_depth;

    if (renderer_init(&renderer, (Vec2i){PROBE_WIDTH, PROBE_HEIGHT},
                      &backend) != OK) {
        return INIT_ERROR;
    }

    root_payload.render = probe_root_render;
    if (entity_init(&root, &root_payload, mat4Identity()) != OK) {
        return INIT_ERROR;
    }
    root.children_entities.data = &child;
    root.children_entities.size = 1;

    texture_pixels[0] = pixelFromRGBA(255, 0, 0, 255);
    texture_pixels[1] = pixelFromRGBA(0, 255, 0, 255);
    texture_pixels[2] = pixelFromRGBA(0, 0, 255, 255);
    texture_pixels[3] = pixelFromRGBA(255, 255, 255, 255);
    mesh.indexes_count = 3;
    mesh.pos_indices = position_indices;
    mesh.tex_indices = texcoord_indices;
    mesh.positions = positions;
    mesh.textCoord = texcoords;

    if (texture_init(&texture, (Vec2i){2, 2}, texture_pixels) != OK ||
        material_init(&material, &texture) != OK ||
        object_init(&object, &mesh, &material) != OK ||
        entity_init(&child, (Renderable *)&object, mat4Identity()) != OK ||
        renderer_set_root_renderable(&renderer, (Renderable *)&root) != OK) {
        return INIT_ERROR;
    }

    renderer.camera_view = mat4Identity();
    renderer.camera_projection = mat4Perspective(
        1.0f, 100.0f, (float)PROBE_WIDTH / (float)PROBE_HEIGHT, 0.5f);
    renderer.clear = true;
    clear_color = pixelFromRGBA(17, 34, 51, 255);
    renderer.clear_color = clear_color;
    result->render_status = renderer_render(&renderer);

    for (uint32_t index = 0; index < PROBE_PIXELS; ++index) {
        if (memcmp(&probe_framebuffer[index], &clear_color,
                   sizeof(clear_color)) != 0) {
            ++result->drawn_pixels;
        }
        if (probe_depth[index].d != 0) {
            ++result->depth_pixels;
        }
    }
    result->framebuffer_fnv1a = fnv1a(
        probe_framebuffer, sizeof(probe_framebuffer));
    result->depth_fnv1a = fnv1a(probe_depth, sizeof(probe_depth));

    if (result->render_status != OK || result->drawn_pixels == 0 ||
        result->drawn_pixels != result->depth_pixels) {
        return RENDER_ERROR;
    }
    return OK;
}

#endif
