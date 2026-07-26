#include <string.h>
#include <stdio.h>
#include <math.h>

#include "renderer.h"
#include "sprite.h"
#include "pixel.h"
#include "depth.h"
#include "backend.h"
#include "scene.h"
#include "rasterizer.h"
#include "object.h"
#include <esp_heap_caps.h>

#define MIN(a, b)(((a) < (b)) ? (a) : (b))
#define MAX(a, b)(((a) > (b)) ? (a) : (b))
#define Z_THRESHOLD 0.000001f

#if DEBUG
extern void show_pixel(float x, float y, uint8_t a, uint8_t b, uint8_t g, uint8_t r);
#endif


int rendererInit(Renderer * r, Vec2i size, BackEnd * backEnd) {
    printf("Initalizing Renderer\n");
    renderingFunctions[RENDERABLE_SPRITE] = & renderSprite;
    renderingFunctions[RENDERABLE_SCENE] = & renderScene;
    renderingFunctions[RENDERABLE_OBJECT] = & renderObject;

    r->scene = 0;
    r->backEnd = backEnd;

    r->frameBuffer.size = size;
    printf("Frame buffer initialized\n");

    int zsize = sizeof(PingoDepth) * size.x * size.y;
    r->z_buffer = (PingoDepth*) heap_caps_malloc(zsize, MALLOC_CAP_SPIRAM);
    printf("Z buffer initialized\n");
    printf("Renderer initialized\n");
    return 0;
}

int rendererSetScene(Renderer * r, Scene * s) {
    if (s == 0)
        return 1; //nullptr scene

    r->scene = s;
    return 0;
}

int rendererSetCamera(Renderer * r, Vec2i rect) {
    r->camera.rect = rect;
    r->frameBuffer.size = rect;
    return 0;
}

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
    Mat4 newMat = mat4Translate((Vec3f) { -r->camera.translation.x, -r->camera.translation.y, 0 });
    s->t = mat4MultiplyM( & s->t, & newMat);

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

int rendererRender(Renderer * r) {
    int num_pixels = r->frameBuffer.size.x * r->frameBuffer.size.y;

    memset(r->z_buffer, 0, num_pixels * sizeof (PingoDepth));

    Pixel* framePixels = r->frameBuffer.pixels;
    if (r->clear == REND_CLEAR) {
        memset(framePixels, r->clearColor.c, num_pixels * sizeof(Pixel));
    } else if (r->clear == REND_BACKGROUND) {
        Pixel* backgroundPixels = r->background.pixels;
        memcpy(framePixels, backgroundPixels, num_pixels * sizeof(Pixel));
    }

    renderScene(mat4Identity(), r, sceneAsRenderable(r->scene));

    return 0;
}

int renderObject(Mat4 object_transform, Renderer * r, Renderable ren) {

    const Vec2i scrSize = r->frameBuffer.size;
    Object * o = ren.impl;
    Vec2f * tex_coords = o->textCoord;
    // printf("Texture coordinates: %p\n", tex_coords);

    // MODEL MATRIX
    Mat4 m = mat4MultiplyM( &o->transform, &object_transform  );
    // printf("Model matrix\n");

    // CAMERA VIEW AND PROJECTION MATRICES
    Mat4 v = r->camera.view;
    // printf("View matrix\n");
    Mat4 p = r->camera.projection;
    // printf("Projection matrix\n");
    float near = r->camera.near;
    // printf("Near: %f\n", near);

    // CAMERA NORMAL
    Vec3f cameraNormal = { 
        v.elements[2],  // forward.x
        v.elements[6],  // forward.y
        v.elements[10]  // forward.z
    };
    cameraNormal = vec3Normalize(cameraNormal);
    // printf("Camera normal\n");

    // printf("Preparing to render mesh %p\n", o->mesh);
    for (int i = 0; i < o->mesh->indexes_count; i += 3) {
        Vec3f * ver1 = &o->mesh->positions[o->mesh->pos_indices[i+0]];
        Vec3f * ver2 = &o->mesh->positions[o->mesh->pos_indices[i+1]];
        Vec3f * ver3 = &o->mesh->positions[o->mesh->pos_indices[i+2]];
        // printf("Got vertices\n");

        Vec4f a =  { ver1->x, ver1->y, ver1->z, 1 };
        Vec4f b =  { ver2->x, ver2->y, ver2->z, 1 };
        Vec4f c =  { ver3->x, ver3->y, ver3->z, 1 };
        // printf("Converted vertices to vec4\n");

        a = mat4MultiplyVec4( &a, &m);
        b = mat4MultiplyVec4( &b, &m);
        c = mat4MultiplyVec4( &c, &m);
        // printf("Transformed vertices\n");

        // // TODO: convert this to look up normals from the mesh
        // // FACE NORMAL
        // Vec3f na = vec3fsubV(*((Vec3f*)(&a)), *((Vec3f*)(&b)));
        // Vec3f nb = vec3fsubV(*((Vec3f*)(&a)), *((Vec3f*)(&c)));
        // Vec3f faceNormal = vec3Normalize(vec3Cross(na, nb));

        // // Cull triangles facing away from camera
        // float faceCamDot = vec3Dot(cameraNormal, (Vec3f){0,0,1});
        // if (faceCamDot < 0)
        //     continue;

        float diffuseLight = 1.0; // default to full illumination from all directions
        // if (true) { // set to true for lighting effects at the expense of performance
        //     Vec3f light = vec3Normalize((Vec3f){-3,8,5});
        //     diffuseLight = (1.0 + vec3Dot(faceNormal, light)) *0.5;
        //     diffuseLight = MIN(1.0, MAX(diffuseLight, 0));
        // }

        a = mat4MultiplyVec4( &a, &v);
        b = mat4MultiplyVec4( &b, &v);
        c = mat4MultiplyVec4( &c, &v);
        // printf("Viewed vertices\n");

        a = mat4MultiplyVec4( &a, &p);
        b = mat4MultiplyVec4( &b, &p);
        c = mat4MultiplyVec4( &c, &p);
        // printf("Projected vertices\n");

        // Don't render triangles completely behind the near clipping plane
        if (a.z > -near && b.z > -near && c.z > -near)
            continue;

        // CORRECTED WITH SCRATCHPIXEL: 
        // convert to device coordinates by perspective division
        persp_divide((Vec3f *)&a);
        persp_divide((Vec3f *)&b);
        persp_divide((Vec3f *)&c);
        // printf("Perspective division\n");

        // TODO: review this logic as face normals may obviate the need for this
        // and indeed be the better option to control exactly what faces are rendered
        float clocking = isClockWise(a.x, a.y, b.x, b.y, c.x, c.y);
        // printf("Clocking: %f\n", clocking);
        if (clocking >= 0)
            continue;

        // Convert to raster space
        to_raster(scrSize, (Vec3f *)&a);
        to_raster(scrSize, (Vec3f *)&b);
        to_raster(scrSize, (Vec3f *)&c);
        // printf("Converted to raster space\n");

        float bbox[4];
        tri_bbox((Vec3f *)&a, (Vec3f *)&b, (Vec3f *)&c, bbox);
        // printf("Got bounding box\n");

        // Bounding box constraint
        if (bbox[0] > scrSize.x - 1 || bbox[2] < 0 || bbox[1] > scrSize.y - 1 || bbox[3] < 0)
            continue;
        // printf("Bounding box constraint\n");

        int x0 = MAX(0, (int)bbox[0]);
        int y0 = MAX(0, (int)bbox[1]);
        int x1 = MIN(scrSize.x - 1, (int)bbox[2]);
        int y1 = MIN(scrSize.y - 1, (int)bbox[3]);
        // printf("Bounding box\n");

        Vec2f tca = {0, 0};
        Vec2f tcb = {0, 0};
        Vec2f tcc = {0, 0};
        if (tex_coords) {
            // printf("Trying to get texture coordinates\n");
            tca = tex_coords[o->tex_indices[i + 0]];
            tcb = tex_coords[o->tex_indices[i + 1]];
            tcc = tex_coords[o->tex_indices[i + 2]];
            // printf("Got texture coordinates\n");

            // Perspective correct texture coordinates
            tca.x /= a.z;
            tca.y /= a.z;
            tcb.x /= b.z;
            tcb.y /= b.z;
            tcc.x /= c.z;
            tcc.y /= c.z;
            // printf("Perspective correct texture coordinates\n");

        } else {
            // printf("No texture coordinates\n");
        }

        // Rasterize the triangle with the new scratchpixel logic
        rasterize(x0, y0, x1, y1, (Vec3f *)&a, (Vec3f *)&b, (Vec3f *)&c, &tca, &tcb, &tcc, o->material->texture, scrSize, r, near, diffuseLight);

    }

    return 0;
};

static inline void rasterize(int x0, int y0, int x1, int y1, const Vec3f* const p0, const Vec3f* const p1, const Vec3f* const p2, const Vec2f* const uv0, const Vec2f* const uv1, const Vec2f* const uv2, const Texture* const texture, const Vec2i scrSize, Renderer* r, float near, float diffuseLight) {
    float inv_area = 1.0f / edge(p0, p1, p2);

    Vec3f sample;
    Vec2f intersections[2];
    int intersection_count;

    // Iterate over scanlines within the bounding box
    for (int scrY = y0; scrY <= y1; ++scrY) {
        // Find the intersection points of the current scanline with the triangle edges
        find_scanline_intersections(p0, p1, p2, scrY, intersections, &intersection_count);

        // Continue only if exactly two intersection points are found
        if (intersection_count != 2) continue;

        // Sort the intersections by x-coordinate
        int x_start = (int)MAX(x0, (int)intersections[0].x);
        int x_end = (int)MIN(x1, (int)intersections[1].x);

        // Iterate over pixels between the intersections on the current scanline
        for (int scrX = x_start, index = scrY * scrSize.x + x_start; scrX <= x_end; ++scrX, ++index) {
            sample.x = scrX;
            sample.y = scrY;

            // Barycentric coordinates for pixel coverage
            float w0 = edge(p1, p2, &sample) * inv_area;
            float w1 = edge(p2, p0, &sample) * inv_area;
            float w2 = edge(p0, p1, &sample) * inv_area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float inv_z = w0 / p0->z + w1 / p1->z + w2 / p2->z;
                if (inv_z < -near) {
                    continue;
                }
                float z = 1.0f / inv_z;

                if (depth_check(r->z_buffer, scrX + scrY * scrSize.x, -inv_z)) {
                    continue;
                }

                depth_write(r->z_buffer, scrX + scrY * scrSize.x, -inv_z);

                Pixel color = {255};
                if (texture) {
                    // Interpolate the texture coordinates
                    Vec2f uv;
                    uv.x = (uv0->x * w0 + uv1->x * w1 + uv2->x * w2) * z;
                    uv.y = (uv0->y * w0 + uv1->y * w1 + uv2->y * w2) * z;

                    // Shade the pixel and update the color buffer
                    color = shade(texture, uv);
                }

                backendDrawPixel(r, &r->frameBuffer, (Vec2i) { scrX, scrY }, color, diffuseLight);
            }
        }
    }
}

float isClockWise(float x1, float y1, float x2, float y2, float x3, float y3) {
    return (y2 - y1) * (x3 - x2) - (y3 - y2) * (x2 - x1);
}

int orient2d( Vec2i a,  Vec2i b,  Vec2i c)
{
    return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
}

void backendDrawPixel (Renderer * r, Texture * f, Vec2i pos, Pixel color, float illumination) {
    // If backend specifies something..
    if (r->backEnd->drawPixel != 0) {
        // Draw using the backend
        r->backEnd->drawPixel(f, pos, color, illumination);
    }
    else {
        // By default call this
        // texture_draw(f, pos, pixelMul(color,illumination));
        texture_draw(f, pos, color);
    }
}

// SCRATCHPIXEL FUNCTIONS
// https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/perspective-correct-interpolation-vertex-attributes.html
static inline void persp_divide(struct Vec3f* p) {
    if (p->z > -Z_THRESHOLD) {
        p->z = -Z_THRESHOLD;
    }
    float inv_z = -1.0f / p->z;
    p->x *= inv_z;
    p->y *= inv_z;
}

static inline void to_raster(const Vec2i size, struct Vec3f* const p) {
    p->x = (p->x + 1) * 0.5f * (float)size.x;
    p->y = (1 - p->y) * 0.5f * (float)size.y;
}

static inline void tri_bbox(const Vec3f* const p0, const Vec3f* const p1, const Vec3f* const p2, float* const bbox) {
    bbox[0] = MIN(MIN(p0->x, p1->x), p2->x);
    bbox[1] = MIN(MIN(p0->y, p1->y), p2->y);
    bbox[2] = MAX(MAX(p0->x, p1->x), p2->x);
    bbox[3] = MAX(MAX(p0->y, p1->y), p2->y);
}

static inline float edge(const Vec3f* const a, const Vec3f* const b, const Vec3f* const test) {
    return (test->x - a->x) * (b->y - a->y) - (test->y - a->y) * (b->x - a->x);
}

static Pixel shade(const Texture* texture, Vec2f uv) {
    if (texture->pixels != NULL) {
        float u = uv.x;
        float v = uv.y;

        // Convert normalized coordinates to texel coordinates
        Vec2i texel;
		texel.x = (int)MIN(u * texture->size.x, texture->size.x - 1);
		texel.y = (int)MIN(v * texture->size.y, texture->size.y - 1);

        // Get the color from the texture at the texel position
        return texture_read(texture, texel);
    }
}

// NOT SCRATCHPIXEL BUT NOT PINGO EITHER
// Helper function to find intersection of a line segment with the screen bounds
static inline int clip_edge(float y, const Vec3f* const v0, const Vec3f* const v1, Vec2f* out) {
    // Check if the edge is horizontal (skip as no intersection along the scanline)
    if (v0->y == v1->y) return 0;

    // Calculate intersection x-coordinate of the line segment with the horizontal line y
    float t = (y - v0->y) / (v1->y - v0->y);
    if (t < 0 || t > 1) return 0; // Intersection not within the segment

    out->x = v0->x + t * (v1->x - v0->x);
    out->y = y;
    return 1; // Valid intersection found
}

// Function to find scanline intersections for triangle edges
static void find_scanline_intersections(const Vec3f* p0, const Vec3f* p1, const Vec3f* p2, int scanline_y, Vec2f* out_intersections, int* count) {
    // Initialize intersection count
    *count = 0;
    
    // Check intersection with the first edge (p0-p1)
    if (clip_edge((float)scanline_y, p0, p1, &out_intersections[*count])) {
        (*count)++;
    }

    // Check intersection with the second edge (p1-p2)
    if (clip_edge((float)scanline_y, p1, p2, &out_intersections[*count])) {
        (*count)++;
    }

    // Check intersection with the third edge (p2-p0)
    if (*count < 2 && clip_edge((float)scanline_y, p2, p0, &out_intersections[*count])) {
        (*count)++;
    }

    // Sort intersections by x-coordinate if two valid intersections are found
    if (*count == 2 && out_intersections[0].x > out_intersections[1].x) {
        Vec2f temp = out_intersections[0];
        out_intersections[0] = out_intersections[1];
        out_intersections[1] = temp;
    }
}
