#include "object.h"
#include "backend.h"
#include "clip.h"
#include "depth.h"
#include "math/fun.h"
#include "math/mat4.h"
#include "mesh.h"
#include "render/material.h"
#include "renderer.h"
#include "state.h"

#include <math.h>
#include <stdint.h>

static int project_clip_vertex(Vec4f *point, Vec2i screen_size,
                               Vec2i *screen) {
    if (!point || !screen || !isfinite(point->x) || !isfinite(point->y) ||
        !isfinite(point->z) || !isfinite(point->w) || !(point->w > 0.0f)) {
        return 0;
    }

    float reciprocal_w = 1.0f / point->w;
    point->x *= reciprocal_w;
    point->y *= reciprocal_w;
    point->z *= reciprocal_w;
    point->w = reciprocal_w;
    if (!isfinite(point->x) || !isfinite(point->y) ||
        !isfinite(point->z) || !isfinite(point->w)) {
        return 0;
    }

    float half_x = screen_size.x * 0.5f;
    float half_y = screen_size.y * 0.5f;
    float screen_x = point->x * half_x + half_x;
    float screen_y = -point->y * half_y + half_y;
    if (!isfinite(screen_x) || !isfinite(screen_y) ||
        screen_x < -2147483648.0f || screen_x > 2147483520.0f ||
        screen_y < -2147483648.0f || screen_y > 2147483520.0f) {
        return 0;
    }

    screen->x = (int32_t)screen_x;
    screen->y = (int32_t)screen_y;
    return 1;
}

static void rasterize_triangle(Object *object, Renderer *renderer,
                               PingoClipVertex first,
                               PingoClipVertex second,
                               PingoClipVertex third,
                               float diffuse_light) {
    Vec4f a = first.position;
    Vec4f b = second.position;
    Vec4f c = third.position;
    Vec2i a_screen;
    Vec2i b_screen;
    Vec2i c_screen;
    if (!project_clip_vertex(&a, renderer->framebuffer.size, &a_screen) ||
        !project_clip_vertex(&b, renderer->framebuffer.size, &b_screen) ||
        !project_clip_vertex(&c, renderer->framebuffer.size, &c_screen)) {
        return;
    }

    if (isClockWise(a.x, a.y, b.x, b.y, c.x, c.y) >= 0.0f) {
        return;
    }

    int32_t min_x = MIN(MIN(a_screen.x, b_screen.x), c_screen.x);
    int32_t min_y = MIN(MIN(a_screen.y, b_screen.y), c_screen.y);
    int32_t max_x = MAX(MAX(a_screen.x, b_screen.x), c_screen.x);
    int32_t max_y = MAX(MAX(a_screen.y, b_screen.y), c_screen.y);
    min_x = MIN(MAX(min_x, 0), renderer->framebuffer.size.x);
    min_y = MIN(MAX(min_y, 0), renderer->framebuffer.size.y);
    max_x = MIN(MAX(max_x, 0), renderer->framebuffer.size.x);
    max_y = MIN(MAX(max_y, 0), renderer->framebuffer.size.y);

    Vec2i minimum = {min_x, min_y};
    int32_t area = orient2d(a_screen, b_screen, c_screen);
    if (area == 0) {
        return;
    }
    float inverse_area = 1.0f / area;

    int32_t a01 = a_screen.y - b_screen.y;
    int32_t b01 = b_screen.x - a_screen.x;
    int32_t a12 = b_screen.y - c_screen.y;
    int32_t b12 = c_screen.x - b_screen.x;
    int32_t a20 = c_screen.y - a_screen.y;
    int32_t b20 = a_screen.x - c_screen.x;
    int32_t w0_row = orient2d(b_screen, c_screen, minimum);
    int32_t w1_row = orient2d(c_screen, a_screen, minimum);
    int32_t w2_row = orient2d(a_screen, b_screen, minimum);

    Vec2f first_over_w = {
        first.texture.x * a.w, first.texture.y * a.w
    };
    Vec2f second_over_w = {
        second.texture.x * b.w, second.texture.y * b.w
    };
    Vec2f third_over_w = {
        third.texture.x * c.w, third.texture.y * c.w
    };
    PingoDepth *depth = renderer->backend->getZetaBuffer(
        renderer, renderer->backend);

    for (int32_t y = min_y; y < max_y;
         ++y, w0_row += b12, w1_row += b20, w2_row += b01) {
        int32_t w0 = w0_row;
        int32_t w1 = w1_row;
        int32_t w2 = w2_row;
        for (int32_t x = min_x; x < max_x;
             ++x, w0 += a12, w1 += a20, w2 += a01) {
            if ((area > 0 && (w0 < 0 || w1 < 0 || w2 < 0)) ||
                (area < 0 && (w0 > 0 || w1 > 0 || w2 > 0))) {
                continue;
            }

            float ndc_z = (w0 * a.z + w1 * b.z + w2 * c.z) * inverse_area;
            float distance = -ndc_z;
            if (!isfinite(distance) || distance < 0.0f || distance > 1.0f) {
                continue;
            }
            Pixel color;
            if (object->material != 0) {
                float reciprocal_w =
                    (w0 * a.w + w1 * b.w + w2 * c.w) * inverse_area;
                if (!isfinite(reciprocal_w) || !(reciprocal_w > 0.0f)) {
                    continue;
                }
                float u = (w0 * first_over_w.x +
                           w1 * second_over_w.x +
                           w2 * third_over_w.x) * inverse_area / reciprocal_w;
                float v = (w0 * first_over_w.y +
                           w1 * second_over_w.y +
                           w2 * third_over_w.y) * inverse_area / reciprocal_w;
                if (!isfinite(u) || !isfinite(v)) {
                    continue;
                }
                Pixel texel = texture_readF(object->material->texture,
                                             (Vec2f){u, v});
                color = pixelMul(texel, diffuse_light);
            } else {
                color = pixelMul(pixelFromUInt8(255), diffuse_light);
            }
            int32_t pixel_index = x + y * renderer->framebuffer.size.x;
            if (!depth_try_write(depth, pixel_index, 1.0f - distance)) {
                continue;
            }
            texture_draw(&renderer->framebuffer, (Vec2i){x, y}, color);
        }
    }
}

int object_render(void *this, Mat4 model, Renderer *renderer)
{
    Object *object = this;

    IF_NULL_RETURN(object, RENDER_ERROR);
    IF_NULL_RETURN(renderer, RENDER_ERROR);

    Mat4 view = mat4Inverse(&renderer->camera_view);
    Mat4 projection = renderer->camera_projection;

    for (int i = 0; i < object->mesh->indexes_count; i += 3) {
        Vec3f *position_a =
            &object->mesh->positions[object->mesh->pos_indices[i + 0]];
        Vec3f *position_b =
            &object->mesh->positions[object->mesh->pos_indices[i + 1]];
        Vec3f *position_c =
            &object->mesh->positions[object->mesh->pos_indices[i + 2]];
        Vec2f uv_a = {0.0f, 0.0f};
        Vec2f uv_b = {0.0f, 0.0f};
        Vec2f uv_c = {0.0f, 0.0f};
        if (object->material != 0) {
            uv_a = object->mesh->textCoord[object->mesh->tex_indices[i + 0]];
            uv_b = object->mesh->textCoord[object->mesh->tex_indices[i + 1]];
            uv_c = object->mesh->textCoord[object->mesh->tex_indices[i + 2]];
        }

        Vec4f a = {position_a->x, position_a->y, position_a->z, 1.0f};
        Vec4f b = {position_b->x, position_b->y, position_b->z, 1.0f};
        Vec4f c = {position_c->x, position_c->y, position_c->z, 1.0f};
        Mat4 view_model = mat4MultiplyM(&model, &view);
        a = mat4MultiplyVec4(&a, &view_model);
        b = mat4MultiplyVec4(&b, &view_model);
        c = mat4MultiplyVec4(&c, &view_model);

        Vec3f normal_a = vec3fsubV(
            (Vec3f){a.x, a.y, a.z}, (Vec3f){b.x, b.y, b.z});
        Vec3f normal_b = vec3fsubV(
            (Vec3f){a.x, a.y, a.z}, (Vec3f){c.x, c.y, c.z});
        Vec3f normal = vec3Normalize(vec3Cross(normal_a, normal_b));
        Vec3f light = vec3Normalize((Vec3f){-8.0f, 5.0f, 5.0f});
        float diffuse_light = (1.0f + vec3Dot(normal, light)) * 0.5f;
        diffuse_light = MIN(1.0f, MAX(diffuse_light, 0.0f));

        a = mat4MultiplyVec4(&a, &projection);
        b = mat4MultiplyVec4(&b, &projection);
        c = mat4MultiplyVec4(&c, &projection);
        PingoClipVertex input[3] = {
            {.position = a, .texture = uv_a},
            {.position = b, .texture = uv_b},
            {.position = c, .texture = uv_c},
        };
        PingoClipVertex clipped[PINGO_CLIP_MAX_VERTICES];
        uint8_t clipped_count = pingoClipTriangle(
            input, PINGO_CLIP_PLANES, clipped);
        for (uint8_t fan = 1; fan + 1 < clipped_count; ++fan) {
            rasterize_triangle(object, renderer, clipped[0], clipped[fan],
                               clipped[fan + 1], diffuse_light);
        }
    }

    return OK;
};

int object_init(Object *this, Mesh *mesh, Material *material)
{
    IF_NULL_RETURN(this, INIT_ERROR);
    IF_NULL_RETURN(mesh, INIT_ERROR);

    this->material = material;
    this->mesh = mesh;
    this->renderable.render = &object_render;

    return OK;
}
