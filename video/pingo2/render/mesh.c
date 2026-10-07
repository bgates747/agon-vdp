#include "mesh.h"
#include "clip.h"

#include <math.h>

static int finite3(Vec3f v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

void mesh_invalidate_bounds(Mesh *mesh) {
    if (mesh) mesh->bounds_valid = false;
}

int mesh_prepare_bounds(Mesh *mesh, uint32_t positions_count) {
    if (!mesh) return 0;
    mesh->positions_count = positions_count;
    mesh_invalidate_bounds(mesh);
    if (!mesh->positions || !mesh->pos_indices || !positions_count ||
        positions_count > UINT32_C(65536) || mesh->indexes_count <= 0 ||
        mesh->indexes_count % 3 != 0) return 0;
    for (int i = 0; i < mesh->indexes_count; ++i) {
        if (mesh->pos_indices[i] >= positions_count) return 0;
    }
    Vec3f low = mesh->positions[0], high = low;
    for (uint32_t i = 0; i < positions_count; ++i) {
        Vec3f v = mesh->positions[i];
        if (!finite3(v)) return 0;
        if (v.x < low.x) low.x = v.x;
        if (v.y < low.y) low.y = v.y;
        if (v.z < low.z) low.z = v.z;
        if (v.x > high.x) high.x = v.x;
        if (v.y > high.y) high.y = v.y;
        if (v.z > high.z) high.z = v.z;
    }
    mesh->bounds_min = low;
    mesh->bounds_max = high;
    mesh->bounds_positions = mesh->positions;
    mesh->bounds_indices = mesh->pos_indices;
    mesh->bounds_positions_count = positions_count;
    mesh->bounds_indexes_count = mesh->indexes_count;
    mesh->bounds_valid = true;
    return 1;
}

int mesh_bounds_current(const Mesh *mesh) {
    return mesh && mesh->bounds_valid && mesh->positions && mesh->pos_indices &&
        mesh->positions_count && mesh->positions_count <= UINT32_C(65536) &&
        mesh->indexes_count > 0 && mesh->indexes_count % 3 == 0 &&
        mesh->bounds_positions == mesh->positions &&
        mesh->bounds_indices == mesh->pos_indices &&
        mesh->bounds_positions_count == mesh->positions_count &&
        mesh->bounds_indexes_count == mesh->indexes_count &&
        finite3(mesh->bounds_min) && finite3(mesh->bounds_max) &&
        mesh->bounds_min.x <= mesh->bounds_max.x &&
        mesh->bounds_min.y <= mesh->bounds_max.y &&
        mesh->bounds_min.z <= mesh->bounds_max.z;
}

static float component(Vec4f v, int axis) {
    switch (axis) {
        case 0: return v.x;
        case 1: return v.y;
        case 2: return v.z;
        default: return v.w;
    }
}

int mesh_bounds_outside(const Mesh *mesh, Mat4 *view_model, Mat4 *projection) {
    if (!mesh_bounds_current(mesh) || !view_model || !projection) return 0;
    uint8_t common = PINGO_CLIP_PLANES;
    float view_low[4] = {0}, view_high[4] = {0};
    for (unsigned corner = 0; corner < 8; ++corner) {
        Vec4f p = {
            corner & 1 ? mesh->bounds_max.x : mesh->bounds_min.x,
            corner & 2 ? mesh->bounds_max.y : mesh->bounds_min.y,
            corner & 4 ? mesh->bounds_max.z : mesh->bounds_min.z, 1.0f,
        };
        /* Preserve the vertex path's two transforms; do not precombine them. */
        Vec4f v = mat4MultiplyVec4(&p, view_model);
        PingoClipVertex c = {.position = mat4MultiplyVec4(&v, projection),
                             .texture = {0, 0}};
        if (!pingoClipVertexFinite(c)) return 0;
        for (int axis = 0; axis < 4; ++axis) {
            float value = component(v, axis);
            if (!isfinite(value)) return 0;
            if (!corner || value < view_low[axis]) view_low[axis] = value;
            if (!corner || value > view_high[axis]) view_high[axis] = value;
        }
        uint8_t outside = 0;
        for (uint8_t plane = 1; plane <= PINGO_CLIP_OUTSIDE_TOP; plane <<= 1) {
            float distance = pingoClipDistance(c, plane);
            if (!isfinite(distance)) return 0;
            if (distance < 0.0f) outside |= plane;
        }
        common &= outside;
        if (!common) return 0;
    }

    /* Certify rejection against rounded vertex arithmetic, not only real-
     * number convexity. Each first-stage component is monotone in each input
     * coordinate, so the eight view corners bound it. Project that enclosing
     * 4D interval using the SAME multiply routine. Independent component
     * extrema make clip-plane upper bounds conservative under cancellation.
     * Only potential rejections pay this extra work. No epsilon changes the
     * clip contract; an exact boundary is retained. */
    float low[4], high[4];
    for (int axis = 0; axis < 4; ++axis) {
        float a[4], b[4];
        for (int j = 0; j < 4; ++j) {
            float coefficient = projection->elements[axis * 4 + j];
            if (!isfinite(coefficient)) return 0;
            a[j] = coefficient < 0 ? view_high[j] : view_low[j];
            b[j] = coefficient < 0 ? view_low[j] : view_high[j];
        }
        Vec4f a4 = {a[0], a[1], a[2], a[3]};
        Vec4f b4 = {b[0], b[1], b[2], b[3]};
        low[axis] = component(mat4MultiplyVec4(&a4, projection), axis);
        high[axis] = component(mat4MultiplyVec4(&b4, projection), axis);
        if (!isfinite(low[axis]) || !isfinite(high[axis])) return 0;
    }
    /* Earlier clipping can interpolate new vertices before reaching the
     * common outside plane. Preserve even the fallback's rounding behavior:
     * interpolation from a huge endpoint can overshoot a small endpoint.
     * Walk the clipper's plane order, enclosing any possible intersections.
     * This deliberately loses correlation (false negatives are harmless). */
    for (int i = 0; i < 6; ++i) {
        float lower, upper;
        switch (i) {
            case 0: lower = -high[2]; upper = -low[2]; break;
            case 1: lower = low[2] + low[3]; upper = high[2] + high[3]; break;
            case 2: lower = low[0] + low[3]; upper = high[0] + high[3]; break;
            case 3: lower = low[3] - high[0]; upper = high[3] - low[0]; break;
            case 4: lower = low[1] + low[3]; upper = high[1] + high[3]; break;
            default: lower = low[3] - high[1]; upper = high[3] - low[1]; break;
        }
        if (!isfinite(lower) || !isfinite(upper)) return 0;
        if ((common & (1u << i)) && upper < 0.0f) return 1;
        if (lower >= 0.0f) continue; /* No intersection arithmetic on this plane. */
        for (int axis = 0; axis < 4; ++axis) {
            if (low[axis] == high[axis]) continue;
            /* For from + (to - from) * t, 0 <= t <= 1. Outward rounding
             * covers separate operations and fused multiply/add alike. */
            float span = nextafterf(high[axis] - low[axis], INFINITY);
            low[axis] = nextafterf(low[axis] - span, -INFINITY);
            high[axis] = nextafterf(high[axis] + span, INFINITY);
            if (!isfinite(low[axis]) || !isfinite(high[axis])) return 0;
        }
        /* pingoClipInterpolate snaps the intersected coordinate to its plane.
         * Keep the union with original vertices (the clipper retains those). */
        int axis = i < 2 ? 2 : (i < 4 ? 0 : 1);
        float snap_low = 0, snap_high = 0;
        if (i == 1 || i == 2 || i == 4) {
            snap_low = -high[3]; snap_high = -low[3];
        } else if (i == 3 || i == 5) {
            snap_low = low[3]; snap_high = high[3];
        }
        if (snap_low < low[axis]) low[axis] = snap_low;
        if (snap_high > high[axis]) high[axis] = snap_high;
    }
    return 0;
}
