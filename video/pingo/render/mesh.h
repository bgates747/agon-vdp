#pragma once

#include <stdint.h>

#include "../math/vec2.h"
#include "../math/vec3.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mesh {
    int indexes_count;
    uint16_t * pos_indices;
    uint16_t * tex_indices;
    Vec3f * positions;
    Vec2f * textCoord;

    /*
     * Array counts and validity are published only after complete VDU
     * uploads. Component uploads may arrive out of order: geometry_valid is
     * recomputed whenever positions or position indices change, while an
     * object's texture mapping is validated against its selected coordinate
     * array separately.
     */
    uint32_t positions_count;
    uint32_t texture_coordinates_count;
    uint32_t texture_indexes_count;
    uint8_t geometry_valid;

    /*
     * Model-space bounds are cached when a vertex array is installed. A
     * missing or non-finite array leaves bounds_valid clear so renderers can
     * conservatively fall back to their existing per-triangle path.
     */
    Vec3f bounds_min;
    Vec3f bounds_max;
    uint8_t bounds_valid;

    /*
     * Selects how this mesh interprets its existing texture coordinates.
     * Zero preserves perspective-correct textured rendering. Flat palette
     * mode samples the source triangle's first UV once and carries that
     * constant color through clipping and rasterization.
     */
    uint8_t shading_mode;

    /*
     * Selects whether this mesh inherits the scene-wide illumination state or
     * emits its texture/palette colors unchanged. Zero preserves the existing
     * scene-lit behavior for every mesh created by older applications.
     */
    uint8_t illumination_policy;
} Mesh;

typedef uint8_t MeshShadingMode;
enum {
    MESH_SHADING_TEXTURED = 0,
    MESH_SHADING_FLAT_PALETTE = 1
};

typedef uint8_t MeshIlluminationPolicy;
enum {
    MESH_ILLUMINATION_INHERIT_SCENE = 0,
    MESH_ILLUMINATION_SELF_ILLUMINATED = 1
};

int meshUpdateBounds(Mesh * mesh);
int meshUpdateGeometryValidity(Mesh * mesh);

#ifdef __cplusplus
}
#endif
