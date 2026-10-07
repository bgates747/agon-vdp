#include <string.h>
#include <stdio.h>
#include <math.h>
#include "math/mat4.h"
#include "render/state.h"
#include "renderer.h"
#include "pixel.h"
#include "depth.h"
#include "backend.h"

#ifdef P2C_DIAGNOSTICS
void p2c_diagnostic_camera_inverse(Renderer *renderer);
#endif

int renderer_init(Renderer * r, Vec2i size, Backend * backend) {
    r->root_renderable = 0;
    r->clear = 1;
    r->clear_color = PIXELBLACK;
    /* Keep the original raw direction for byte-identical default lighting. */
    r->light_direction = (Vec3f){-8.0f, 5.0f, 5.0f};
    r->light_intensity = 1.0f;
    r->ambient_light = 0.0f;
    r->illumination_enabled = true;
    r->backend = backend;
    r->backend->init(r, r->backend, (Vec4i) { 0, 0, 0, 0 });

    int e = 0;
    e = texture_init( &r->framebuffer, size, backend->getFrameBuffer(r, backend));
    if (e) return e;

    return 0;
}



int renderer_render(Renderer *r)
{
    Backend *be = r->backend;

    int pixels = r->framebuffer.size.x * r->framebuffer.size.y;
    memset(be->getZetaBuffer(r,be), 0, pixels * sizeof (PingoDepth));

    be->beforeRender(r, be);

    //get current framebuffe from Backend
    r->framebuffer.frameBuffer = be->getFrameBuffer(r, be);

    //Clear draw buffer before rendering
    if (r->clear) {
        Pixel *framebuffer = be->getFrameBuffer(r, be);
        for (int index = 0; index < pixels; ++index) {
            framebuffer[index] = r->clear_color;
        }
    }

#ifdef P2C_DIAGNOSTICS
    p2c_diagnostic_camera_inverse(r);
#endif
    r->prepared_view = mat4Inverse(&r->camera_view);

    r->root_renderable->render(r->root_renderable, mat4Identity(), r);

    be->afterRender(r, be);

    return 0;
}

int renderer_set_root_renderable(Renderer *renderer, Renderable *root)
{
    IF_NULL_RETURN(renderer, SET_ERROR);
    IF_NULL_RETURN(root, SET_ERROR);

    renderer->root_renderable = root;
    return 0;
}

int renderer_set_light_direction(Renderer *renderer, Vec3f direction)
{
    if (!renderer || !isfinite(direction.x) || !isfinite(direction.y) ||
        !isfinite(direction.z)) return SET_ERROR;
    float scale = fmaxf(fabsf(direction.x),
                       fmaxf(fabsf(direction.y), fabsf(direction.z)));
    if (!(scale > 0.0f)) return SET_ERROR;
    direction.x /= scale; direction.y /= scale; direction.z /= scale;
    renderer->light_direction = vec3Normalize(direction);
    return 0;
}
