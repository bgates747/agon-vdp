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
     * Model-space bounds are cached when a vertex array is installed. A
     * missing or non-finite array leaves bounds_valid clear so renderers can
     * conservatively fall back to their existing per-triangle path.
     */
    uint32_t positions_count;
    Vec3f bounds_min;
    Vec3f bounds_max;
    uint8_t bounds_valid;
} Mesh;

int meshUpdateBounds(Mesh * mesh);

#ifdef __cplusplus
}
#endif
