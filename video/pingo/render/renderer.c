#include <string.h>
#include <stdio.h>
#include <math.h>
#include "renderer.h"

#ifndef PINGO_DISABLE_ILLUMINATION
#define PINGO_DISABLE_ILLUMINATION 0
#endif
#include "sprite.h"
#include "pixel.h"
#include "depth.h"
#include "perspective_span.h"
#include "triangle_span.h"
#include "backend.h"
#include "scene.h"
#include "rasterizer.h"
#include "object.h"
/*#include "../backend/ttgobackend.h"*/

#if DEBUG
extern void show_pixel(float x, float y, uint8_t a, uint8_t b, uint8_t g, uint8_t r);
#endif

#if PINGO_RENDER_DIAGNOSTICS
static uint32_t rendererDiagnosticsNow(Renderer * r) {
    return r->diagnostics_clock ? r->diagnostics_clock() : 0;
}

static uint32_t rendererDiagnosticsFinishPhase(
        Renderer * r, uint32_t started, uint64_t * accumulator) {
    uint32_t finished = rendererDiagnosticsNow(r);
    *accumulator += (uint32_t)(finished - started);
    return finished;
}
#endif

int renderFrame(Renderer * r, Renderable ren) {
    Texture * f = ren.impl;
    return rasterizer_draw_pixel_perfect((Vec2i) { 0, 0 }, r, f);
};

int renderSprite(Mat4 transform, Renderer * r, Renderable ren) {
    Sprite * s = ren.impl;
    Mat4 backUp = s->t;

    //Apply parent transform to the local transform
    s->t = mat4MultiplyM( & s->t, & transform);

    //Apply camera translation
    Mat4 newMat = mat4Translate((Vec3f) { -r->camera.x, -r->camera.y, 0 });
    s->t = mat4MultiplyM( & s->t, & newMat);

    /*
  if (mat4IsOnlyTranslation(&s->t)) {
      Vec2i off = {s->t.elements[2], s->t.elements[5]};
      rasterizer_draw_pixel_perfect(off,r, &s->frame);
      s->t = backUp;
      return 0;
  }

  if (mat4IsOnlyTranslationDoubled(&s->t)) {
      Vec2i off = {s->t.elements[2], s->t.elements[5]};
      rasterizer_draw_pixel_perfect_doubled(off,r, &s->frame);
      s->t = backUp;
      return 0;
  }*/

    rasterizer_draw_transformed(s->t, r, & s->frame);
    s->t = backUp;
    return 0;
};

void renderRenderable(Mat4 transform, Renderer * r, Renderable ren) {
    renderingFunctions[ren.renderableType](transform, r, ren);
};

int renderScene(Mat4 transform, Renderer * r, Renderable ren) {
    Scene * s = ren.impl;
    if (!s->visible)
        return 0;

    //Apply hierarchy transfom
    Mat4 newTransform = mat4MultiplyM( & s->transform, & transform);
    for (int i = 0; i < s->numberOfRenderables; i++) {
        renderRenderable(newTransform, r, s->renderables[i]);
    }
    return 0;
};

#define MIN(a, b)(((a) < (b)) ? (a) : (b))
#define MAX(a, b)(((a) > (b)) ? (a) : (b))

int edgeFunction(const Vec2f * a, const Vec2f * b, const Vec2f * c) {
    return (c->x - a->x) * (b->y - a->y) - (c->y - a->y) * (b->x - a->x);
}

float isClockWise(float x1, float y1, float x2, float y2, float x3, float y3) {
    return (y2 - y1) * (x3 - x2) - (y3 - y2) * (x2 - x1);
}

int orient2d( Vec2i a,  Vec2i b,  Vec2i c)
{
    return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
}

/*
 * Clip-space Z is [-W, 0] for Pingo's qualified projection. The existing
 * all-Z-positive test retains responsibility for the near/camera plane.
 * This helper adds only the far and four lateral trivial rejects.
 */
static int triangleOutsideRemainingClipPlanes(
        Vec4f a, Vec4f b, Vec4f c) {
    /*
     * A triangle wholly on or behind the eye plane cannot be projected.
     * Keep this explicit: mixed canonical outcodes alone do not guarantee
     * that a nonpositive W will satisfy one common plane test.
     */
    if (a.w <= 0.0f && b.w <= 0.0f && c.w <= 0.0f)
        return 1;
    if (a.z < -a.w && b.z < -b.w && c.z < -c.w)
        return 1;
    if (a.x < -a.w && b.x < -b.w && c.x < -c.w)
        return 1;
    if (a.x > a.w && b.x > b.w && c.x > c.w)
        return 1;
    if (a.y < -a.w && b.y < -b.w && c.y < -c.w)
        return 1;
    if (a.y > a.w && b.y > b.w && c.y > c.w)
        return 1;
    return 0;
}

enum {
    PINGO_CLIP_OUTSIDE_EYE = 1 << 0,
    PINGO_CLIP_OUTSIDE_NEAR = 1 << 1,
    PINGO_CLIP_OUTSIDE_FAR = 1 << 2,
    PINGO_CLIP_OUTSIDE_LEFT = 1 << 3,
    PINGO_CLIP_OUTSIDE_RIGHT = 1 << 4,
    PINGO_CLIP_OUTSIDE_BOTTOM = 1 << 5,
    PINGO_CLIP_OUTSIDE_TOP = 1 << 6,
    PINGO_CLIP_OUTSIDE_ALL = (1 << 7) - 1
};

static uint8_t clipOutcode(Vec4f point) {
    /*
     * A malformed transform must never make a valid object disappear. A
     * zero outcode makes the common-plane test fail open for NaN or Inf.
     */
    if (!isfinite(point.x) ||
        !isfinite(point.y) ||
        !isfinite(point.z) ||
        !isfinite(point.w)) {
        return 0;
    }

    uint8_t outcode = 0;
    if (point.w <= 0.0f) outcode |= PINGO_CLIP_OUTSIDE_EYE;
    if (point.z > 0.0f) outcode |= PINGO_CLIP_OUTSIDE_NEAR;
    if (point.z < -point.w) outcode |= PINGO_CLIP_OUTSIDE_FAR;
    if (point.x < -point.w) outcode |= PINGO_CLIP_OUTSIDE_LEFT;
    if (point.x > point.w) outcode |= PINGO_CLIP_OUTSIDE_RIGHT;
    if (point.y < -point.w) outcode |= PINGO_CLIP_OUTSIDE_BOTTOM;
    if (point.y > point.w) outcode |= PINGO_CLIP_OUTSIDE_TOP;
    return outcode;
}

static int meshBoundsOutsideClipPlanes(
        const Mesh * mesh, Mat4 * model, Mat4 * viewProjection) {
    if (!mesh || !mesh->bounds_valid) {
        return 0;
    }
    if (!isfinite(mesh->bounds_min.x) ||
        !isfinite(mesh->bounds_min.y) ||
        !isfinite(mesh->bounds_min.z) ||
        !isfinite(mesh->bounds_max.x) ||
        !isfinite(mesh->bounds_max.y) ||
        !isfinite(mesh->bounds_max.z) ||
        mesh->bounds_min.x > mesh->bounds_max.x ||
        mesh->bounds_min.y > mesh->bounds_max.y ||
        mesh->bounds_min.z > mesh->bounds_max.z) {
        return 0;
    }

    uint8_t commonOutcode = PINGO_CLIP_OUTSIDE_ALL;
    for (uint8_t corner = 0; corner < 8; corner++) {
        Vec4f position = {
            (corner & 1) ? mesh->bounds_max.x : mesh->bounds_min.x,
            (corner & 2) ? mesh->bounds_max.y : mesh->bounds_min.y,
            (corner & 4) ? mesh->bounds_max.z : mesh->bounds_min.z,
            1.0f
        };
        /*
         * Match the established triangle path's two matrix-vector products
         * exactly. Precomposing MVP changes floating-point association and
         * can falsely reject a grazing boundary in rare rounding cases.
         */
        Vec4f world = mat4MultiplyVec4(&position, model);
        Vec4f clip = mat4MultiplyVec4(&world, viewProjection);
        commonOutcode &= clipOutcode(clip);

        /*
         * Intersection can only clear bits. Once no common outside plane
         * remains, the other corners cannot make the box rejectable.
         */
        if (commonOutcode == 0) {
            return 0;
        }
    }
    return 1;
}

static inline void backendDrawPixel(
        Renderer * r, Texture * f, Vec2i pos,
        int32_t pixelIndex, Pixel color, float illumination,
        const PixelShadeLut * shadeLut) {
    // If backend specifies something..
    if (r->backEnd->drawPixel != 0) {
        // Draw using the backend
#if PINGO_DISABLE_ILLUMINATION
        r->backEnd->drawPixel(f, pos, color, 1.0f);
#else
        r->backEnd->drawPixel(f, pos, color, illumination);
#endif
    }
    else {
        // By default call this
#if PINGO_DISABLE_ILLUMINATION
        f->frameBuffer[pixelIndex] = color;
#else
        f->frameBuffer[pixelIndex] =
            pixelMulLut(color, shadeLut);
#endif
    }
}

int renderObject(Mat4 object_transform, Renderer * r, Renderable ren) {

    Object * o = ren.impl;
#if PINGO_RENDER_DIAGNOSTICS
    r->diagnostics.objects++;
#endif
    if (!o || !o->mesh ||
        !o->mesh->positions ||
        !o->mesh->pos_indices ||
        o->mesh->indexes_count < 3) {
        return 0;
    }

    Vec2f * tex_coords = o->textCoord;
    if (!tex_coords) {
        tex_coords = o->mesh->textCoord;
    }

    // MODEL MATRIX
    Mat4 m = mat4MultiplyM( &o->transform, &object_transform  );

    // Locally derived from upstream Pingo's transform-composition lineage
    // (notably a0ed0cb). Preserve this port's model-space lighting convention,
    // but compose view and projection once per object instead of performing
    // both matrix-vector products for every triangle vertex. Despite its
    // argument order, mat4MultiplyM(&v, &p) returns p * v.
    Mat4 vp = mat4MultiplyM(
        &r->camera_view, &r->camera_projection);

    if (r->frustumCulling && o->mesh->bounds_valid) {
#if PINGO_RENDER_DIAGNOSTICS
        r->diagnostics.objects_bounds_tested++;
        uint32_t bounds_started = rendererDiagnosticsNow(r);
#endif
        int outside = meshBoundsOutsideClipPlanes(o->mesh, &m, &vp);
#if PINGO_RENDER_DIAGNOSTICS
        rendererDiagnosticsFinishPhase(
            r, bounds_started, &r->diagnostics.transform_ticks);
#endif
        if (outside) {
#if PINGO_RENDER_DIAGNOSTICS
            r->diagnostics.objects_frustum_rejected++;
            if (o->mesh->indexes_count > 0) {
                r->diagnostics.triangles_avoided +=
                    (uint32_t)o->mesh->indexes_count / 3;
            }
#endif
            return 0;
        }
    }

    const Vec2i scrSize = r->frameBuffer.size;
    BackEnd * const backEnd = r->backEnd;
    PingoDepth * const zetaBuffer =
        backEnd->getZetaBuffer(r, backEnd);

#if !PINGO_DISABLE_ILLUMINATION
    // The light direction is constant for the whole object. Normalizing it
    // once preserves the existing value while avoiding a square root and
    // division for every submitted triangle.
    const Vec3f light = vec3Normalize((Vec3f){-8,-5,5});
#endif

    for (int i = 0; i + 2 < o->mesh->indexes_count; i += 3) {
#if PINGO_RENDER_DIAGNOSTICS
        r->diagnostics.triangles_submitted++;
        uint32_t phase_started = rendererDiagnosticsNow(r);
#endif

        Vec3f * ver1 = &o->mesh->positions[o->mesh->pos_indices[i+0]];
        Vec3f * ver2 = &o->mesh->positions[o->mesh->pos_indices[i+1]];
        Vec3f * ver3 = &o->mesh->positions[o->mesh->pos_indices[i+2]];

        Vec2f tca = {0,0};
        Vec2f tcb = {0,0};
        Vec2f tcc = {0,0};

        if (o->material != 0) {
            tca = tex_coords[o->mesh->tex_indices[i+0]];
            tcb = tex_coords[o->mesh->tex_indices[i+1]];
            tcc = tex_coords[o->mesh->tex_indices[i+2]];
        }

        Vec4f a =  { ver1->x, ver1->y, ver1->z, 1 };
        Vec4f b =  { ver2->x, ver2->y, ver2->z, 1 };
        Vec4f c =  { ver3->x, ver3->y, ver3->z, 1 };

        a = mat4MultiplyVec4( &a, &m);
        b = mat4MultiplyVec4( &b, &m);
        c = mat4MultiplyVec4( &c, &m);

        // Calculate face illumination unless this experimental build removes
        // the lighting path to measure its complete renderer cost.
#if PINGO_DISABLE_ILLUMINATION
        const float diffuseLight = 1.0f;
#else
        Vec3f na = vec3fsubV(*((Vec3f*)(&a)), *((Vec3f*)(&b)));
        Vec3f nb = vec3fsubV(*((Vec3f*)(&a)), *((Vec3f*)(&c)));
        Vec3f normal = vec3Normalize(vec3Cross(na, nb));
        float diffuseLight = (1.0 + vec3Dot(normal, light)) *0.5;
        diffuseLight = MIN(1.0, MAX(diffuseLight, 0));
#endif

        a = mat4MultiplyVec4( &a, &vp);
        b = mat4MultiplyVec4( &b, &vp);
        c = mat4MultiplyVec4( &c, &vp);

#if PINGO_RENDER_DIAGNOSTICS
        phase_started = rendererDiagnosticsFinishPhase(
            r, phase_started, &r->diagnostics.transform_ticks);
#endif

        //Triangle is completely behind camera
        if (a.z > 0 && b.z > 0 && c.z > 0) {
#if PINGO_RENDER_DIAGNOSTICS
            r->diagnostics.triangles_z_rejected++;
            rendererDiagnosticsFinishPhase(
                r, phase_started, &r->diagnostics.triangle_setup_ticks);
#endif
           continue;
        }

        if (r->frustumCulling &&
            triangleOutsideRemainingClipPlanes(a, b, c)) {
#if PINGO_RENDER_DIAGNOSTICS
            r->diagnostics.triangles_frustum_rejected++;
            rendererDiagnosticsFinishPhase(
                r, phase_started, &r->diagnostics.triangle_setup_ticks);
#endif
            continue;
        }

        // convert to device coordinates by perspective division
        a.w = 1.0 / a.w;
        b.w = 1.0 / b.w;
        c.w = 1.0 / c.w;
        a.x *= a.w; a.y *= a.w; a.z *= a.w;
        b.x *= b.w; b.y *= b.w; b.z *= b.w;
        c.x *= c.w; c.y *= c.w; c.z *= c.w;

        float clocking = isClockWise(a.x, a.y, b.x, b.y, c.x, c.y);
        if (clocking >= 0) {
#if PINGO_RENDER_DIAGNOSTICS
            r->diagnostics.triangles_backface_rejected++;
            rendererDiagnosticsFinishPhase(
                r, phase_started, &r->diagnostics.triangle_setup_ticks);
#endif
            continue;
        }

        //Compute Screen coordinates
        float halfX = scrSize.x/2;
        float halfY = scrSize.y/2;
        Vec2i a_s = {a.x * halfX + halfX, -a.y * halfY + halfY};
        Vec2i b_s = {b.x * halfX + halfX, -b.y * halfY + halfY};
        Vec2i c_s = {c.x * halfX + halfX, -c.y * halfY + halfY};

        int32_t minX = MIN(MIN(a_s.x, b_s.x), c_s.x);
        int32_t minY = MIN(MIN(a_s.y, b_s.y), c_s.y);
        int32_t maxX = MAX(MAX(a_s.x, b_s.x), c_s.x);
        int32_t maxY = MAX(MAX(a_s.y, b_s.y), c_s.y);

#if PINGO_RENDER_DIAGNOSTICS
        int32_t unclippedMinX = minX;
        int32_t unclippedMinY = minY;
        int32_t unclippedMaxX = maxX;
        int32_t unclippedMaxY = maxY;
#endif

        minX = MIN(MAX(minX, 0), r->frameBuffer.size.x);
        minY = MIN(MAX(minY, 0), r->frameBuffer.size.y);
        maxX = MIN(MAX(maxX, 0), r->frameBuffer.size.x);
        maxY = MIN(MAX(maxY, 0), r->frameBuffer.size.y);

#if PINGO_RENDER_DIAGNOSTICS
        if (minX != unclippedMinX || minY != unclippedMinY ||
            maxX != unclippedMaxX || maxY != unclippedMaxY) {
            r->diagnostics.triangles_bbox_clamped++;
        }
#endif

        // Barycentric coordinates at minX/minY corner
        Vec2i minTriangle = { minX, minY };

        int32_t area =  orient2d( a_s, b_s, c_s);
        if (area == 0) {
#if PINGO_RENDER_DIAGNOSTICS
            r->diagnostics.triangles_degenerate++;
            rendererDiagnosticsFinishPhase(
                r, phase_started, &r->diagnostics.triangle_setup_ticks);
#endif
            continue;
        }
        float areaInverse = 1.0/area;

        int32_t A01 = ( a_s.y - b_s.y); //Barycentric coordinates steps
        int32_t B01 = ( b_s.x - a_s.x); //Barycentric coordinates steps
        int32_t A12 = ( b_s.y - c_s.y); //Barycentric coordinates steps
        int32_t B12 = ( c_s.x - b_s.x); //Barycentric coordinates steps
        int32_t A20 = ( c_s.y - a_s.y); //Barycentric coordinates steps
        int32_t B20 = ( a_s.x - c_s.x); //Barycentric coordinates steps

        int32_t w0_row = orient2d( b_s, c_s, minTriangle);
        int32_t w1_row = orient2d( c_s, a_s, minTriangle);
        int32_t w2_row = orient2d( a_s, b_s, minTriangle);

        PingoPerspectiveAttributes textureStepX = {0.0f, 0.0f, 0.0f};
        if (o->material != 0) {
            // a.w/b.w/c.w retain reciprocal clip-space W.
            tca.x *= a.w;
            tca.y *= a.w;
            tcb.x *= b.w;
            tcb.y *= b.w;
            tcc.x *= c.w;
            tcc.y *= c.w;

            /*
             * A12 + A20 + A01 == 0. Use the equivalent difference form to
             * reduce cancellation while preparing the perspective-attribute
             * X gradients once per triangle.
             */
            textureStepX.reciprocalW =
                (A12 * (a.w - c.w) + A20 * (b.w - c.w)) *
                areaInverse;
            textureStepX.uOverW =
                (A12 * (tca.x - tcc.x) +
                 A20 * (tcb.x - tcc.x)) *
                areaInverse;
            textureStepX.vOverW =
                (A12 * (tca.y - tcc.y) +
                 A20 * (tcb.y - tcc.y)) *
                areaInverse;
        }

#if PINGO_RENDER_DIAGNOSTICS
        bool viewportEmpty = minX >= maxX || minY >= maxY;
        if (viewportEmpty) {
            r->diagnostics.triangles_bbox_rejected++;
        } else {
            r->diagnostics.triangles_rasterized++;
            r->diagnostics.fragments_bbox +=
                (uint64_t)(maxX - minX) * (uint64_t)(maxY - minY);
        }
        phase_started = rendererDiagnosticsFinishPhase(
            r, phase_started, &r->diagnostics.triangle_setup_ticks);

        if (viewportEmpty) {
            continue;
        }

        uint32_t fragmentsCovered = 0;
        uint32_t fragmentsDepthRangeRejected = 0;
        uint32_t fragmentsDepthTestRejected = 0;
        uint32_t fragmentsReciprocalWRejected = 0;
        uint32_t fragmentsShaded = 0;
#endif

        const uint32_t rowWidth = (uint32_t)(maxX - minX);
        /*
         * A12 + A20 + A01 == 0. Calculate the exact first fragment in each
         * accepted span below, then advance depth with the stable difference
         * form instead of repeating the barycentric products per fragment.
         */
        const float depthStepX =
            -(A12 * (a.z - c.z) + A20 * (b.z - c.z)) *
            areaInverse;

#if PINGO_DISABLE_ILLUMINATION
        const PixelShadeLut * shadeLut = 0;
#else
        PixelShadeLut shadeLutStorage =
            pixelShadeLut(diffuseLight);
        const PixelShadeLut * shadeLut = &shadeLutStorage;
#endif

        for (int16_t y = minY; y < maxY; y++, w0_row += B12,w1_row += B20,w2_row += B01) {
            TriangleRowSpan span;
            if (!triangleRowSpanFind(
                    area,
                    w0_row, w1_row, w2_row,
                    A12, A20, A01,
                    rowWidth, &span)) {
                continue;
            }

            const int32_t offset = (int32_t)span.begin;
            const int32_t spanMinX = minX + offset;
            const int32_t spanMaxX =
                minX + (int32_t)span.end;
            int32_t w0 = (int32_t)(
                (int64_t)w0_row + (int64_t)A12 * offset);
            int32_t w1 = (int32_t)(
                (int64_t)w1_row + (int64_t)A20 * offset);
            int32_t w2 = (int32_t)(
                (int64_t)w2_row + (int64_t)A01 * offset);
            float depth =
                -(w0 * a.z + w1 * b.z + w2 * c.z) *
                areaInverse;

            PingoPerspectiveBoundary textureBoundary = {
                .attributes = {0.0f, 0.0f, 0.0f},
                .u = 0.0f,
                .v = 0.0f,
                .valid = false
            };
            float textCoordx = 0.0f;
            float textCoordy = 0.0f;
            float textCoordStepX = 0.0f;
            float textCoordStepY = 0.0f;
            uint32_t textureSpanRemaining = 0u;
            uint32_t textureBlockRemaining = 0u;
            bool textureBlockValid = false;

            if (o->material != 0) {
                PingoPerspectiveAttributes textureAttributes;
                textureAttributes.reciprocalW =
                    (w0 * a.w + w1 * b.w + w2 * c.w) *
                    areaInverse;
                textureAttributes.uOverW =
                    (w0 * tca.x + w1 * tcb.x + w2 * tcc.x) *
                    areaInverse;
                textureAttributes.vOverW =
                    (w0 * tca.y + w1 * tcb.y + w2 * tcc.y) *
                    areaInverse;
                textureBoundary =
                    pingoPerspectiveBoundaryRecover(textureAttributes);
                textureSpanRemaining =
                    (uint32_t)(spanMaxX - spanMinX);
            }

            for (int32_t x = spanMinX; x < spanMaxX;
                 x++, w0 += A12, w1 += A20, w2 += A01,
                 depth += depthStepX) {
                if (o->material != 0 &&
                    textureBlockRemaining == 0u) {
                    PingoPerspectiveSpanBlock block;
                    textureBlockValid =
                        pingoPerspectiveSpanBlockPrepare(
                            textureBoundary,
                            textureStepX,
                            textureSpanRemaining,
                            &block);
                    textureBlockRemaining = block.length;
                    textureSpanRemaining -= block.length;
                    textureBoundary = block.end;
                    textCoordx = block.u;
                    textCoordy = block.v;
                    textCoordStepX = block.uStep;
                    textCoordStepY = block.vStep;
                }

#if PINGO_RENDER_DIAGNOSTICS
                fragmentsCovered++;
#endif

                if (depth < 0.0 || depth > 1.0) {
#if PINGO_RENDER_DIAGNOSTICS
                    fragmentsDepthRangeRejected++;
#endif
                } else {
                    int32_t pixelIndex = x + y * scrSize.x;
                    if (!depth_try_write(
                            zetaBuffer, pixelIndex, 1-depth )) {
#if PINGO_RENDER_DIAGNOSTICS
                        fragmentsDepthTestRejected++;
#endif
                    } else if (o->material != 0) {
                        /*
                         * A zero required block endpoint makes the affine UV
                         * undefined. Fail closed after the unchanged depth
                         * write. For valid blocks, retain the exact old
                         * per-fragment projective-pole expression and
                         * evaluation order.
                         */
                        bool reciprocalWRejected =
                            !textureBlockValid;
                        if (!reciprocalWRejected) {
                            const float fragmentReciprocalW =
                                (w0 * a.w + w1 * b.w + w2 * c.w) *
                                areaInverse;
                            reciprocalWRejected =
                                fragmentReciprocalW == 0.0f;
                        }
                        if (reciprocalWRejected) {
#if PINGO_RENDER_DIAGNOSTICS
                            fragmentsReciprocalWRejected++;
#endif
                        } else {
                            Pixel text = texture_readFInline(
                                o->material->texture,
                                (Vec2f){textCoordx,textCoordy});
#if DEBUG
                            //show_pixel(textCoordx, textCoordy, text.a, text.b, text.g, text.r);
#endif

                            backendDrawPixel(
                                r, &r->frameBuffer, (Vec2i){x,y},
                                pixelIndex, text, diffuseLight, shadeLut);

#if PINGO_RENDER_DIAGNOSTICS
                            fragmentsShaded++;
#endif
                        }
                    } else {
                        Pixel pixel = pixelFromRGBA(255, 0, 255, 255);
                        backendDrawPixel(
                            r, &r->frameBuffer, (Vec2i){x,y},
                            pixelIndex, pixel, diffuseLight, shadeLut);

#if PINGO_RENDER_DIAGNOSTICS
                        fragmentsShaded++;
#endif
                    }
                }

                /*
                 * Attribute position follows covered X, not visibility.
                 * Depth rejection must not freeze the texture mapper.
                 */
                if (o->material != 0) {
                    textCoordx += textCoordStepX;
                    textCoordy += textCoordStepY;
                    textureBlockRemaining--;
                }
            }

        }

#if PINGO_RENDER_DIAGNOSTICS
        rendererDiagnosticsFinishPhase(
            r, phase_started, &r->diagnostics.raster_ticks);
        r->diagnostics.fragments_covered += fragmentsCovered;
        r->diagnostics.fragments_depth_range_rejected +=
            fragmentsDepthRangeRejected;
        r->diagnostics.fragments_depth_test_rejected +=
            fragmentsDepthTestRejected;
        r->diagnostics.fragments_reciprocal_w_rejected +=
            fragmentsReciprocalWRejected;
        r->diagnostics.fragments_shaded += fragmentsShaded;
#endif
    }

    return 0;
};

int rendererInit(Renderer * r, Vec2i size, BackEnd * backEnd) {
    renderingFunctions[RENDERABLE_SPRITE] = & renderSprite;
    renderingFunctions[RENDERABLE_SCENE] = & renderScene;
    renderingFunctions[RENDERABLE_OBJECT] = & renderObject;

    r->scene = 0;
    r->clear = 1;
    r->clearColor = PIXELBLACK;
    r->backEnd = backEnd;
    r->frustumCulling = 1;

#if PINGO_RENDER_DIAGNOSTICS
    r->diagnostics_clock = 0;
    r->diagnostics_clock_hz = 0;
    memset(&r->diagnostics, 0, sizeof(r->diagnostics));
#endif

    r->backEnd->init(r, r->backEnd, (Vec4i) { 0, 0, 0, 0 });

    int e = 0;
    e = texture_init( & (r->frameBuffer), size, backEnd->getFrameBuffer(r, backEnd));
    if (e) return e;

    return 0;
}

void rendererSetFrustumCulling(Renderer * r, int enabled) {
    if (r) {
        r->frustumCulling = enabled != 0;
    }
}

int rendererRender(Renderer * r) {

#if PINGO_RENDER_DIAGNOSTICS
    memset(&r->diagnostics, 0, sizeof(r->diagnostics));
    uint32_t clear_started = rendererDiagnosticsNow(r);
#endif

    int pixels = r->frameBuffer.size.x * r->frameBuffer.size.y;
    memset(r->backEnd->getZetaBuffer(r,r->backEnd), 0, pixels * sizeof (PingoDepth));

    r->backEnd->beforeRender(r, r->backEnd);

    //get current framebuffe from backend
    r->frameBuffer.frameBuffer = r->backEnd->getFrameBuffer(r, r->backEnd);

    //Clear draw buffer before rendering
    if (r->clear) {
        memset(r->backEnd->getFrameBuffer(r,r->backEnd), 0, pixels * sizeof (Pixel));
    }

#if PINGO_RENDER_DIAGNOSTICS
    rendererDiagnosticsFinishPhase(
        r, clear_started, &r->diagnostics.clear_ticks);
#endif

    renderScene(mat4Identity(), r, sceneAsRenderable(r->scene));

    r->backEnd->afterRender(r, r->backEnd);

    return 0;
}

int rendererSetScene(Renderer * r, Scene * s) {
    if (s == 0)
        return 1; //nullptr scene

    r->scene = s;
    return 0;
}

int rendererSetCamera(Renderer * r, Vec4i rect) {
    r->camera = rect;
    r->backEnd->init(r, r->backEnd, rect);
    r->frameBuffer.size = (Vec2i) {
            rect.z, rect.w
};
    return 0;
}
