#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "math/vec2.h"
#include "math/vec3.h"
#include "math/mat4.h"

enum { PINGO_TEXTURED = 0, PINGO_FLAT_PALETTE = 1 };
enum { PINGO_INHERIT_LIGHT = 0, PINGO_SELF_LIT = 1 };

/* Zero-initialize before filling. Arrays are caller-owned and immutable during
 * render. positions_count declares real allocated Vec3f elements; indices do
 * not establish an allocation extent. Invalidate BEFORE any in-place edit,
 * then refresh at the update boundary. Replacing pointers/counts fails open
 * automatically; undetectable in-place edits still require invalidation. */
typedef struct Mesh {
    int indexes_count;
    uint16_t *pos_indices;
    uint16_t *tex_indices;
    Vec3f *positions;
    Vec2f *textCoord;
    uint32_t positions_count;
    Vec3f bounds_min;
    Vec3f bounds_max;
    const Vec3f *bounds_positions;
    const uint16_t *bounds_indices;
    uint32_t bounds_positions_count;
    int bounds_indexes_count;
    bool bounds_valid;
    /* Zero initialization preserves textured, scene-lit rendering. Flat
     * palette faces sample the original first UV once, before clipping. */
    uint8_t shading_mode;
    uint8_t illumination_policy;
} Mesh;

#ifdef __cplusplus
extern "C" {
#endif
void mesh_invalidate_bounds(Mesh *mesh);
/* Returns 1 only for a nonempty, finite mesh with validated position indices.
 * Failure disables the optimization, retaining the declared position extent. */
int mesh_prepare_bounds(Mesh *mesh, uint32_t positions_count);
int mesh_bounds_current(const Mesh *mesh);
/* A zero return means use the normal triangle path, including on bad bounds. */
int mesh_bounds_outside(const Mesh *mesh, Mat4 *view_model, Mat4 *projection);
#ifdef __cplusplus
}
#endif
