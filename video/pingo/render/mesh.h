#pragma once

#include <stdint.h>

#include "../math/vec2.h"
#include "../math/vec3.h"

typedef struct Mesh {
    int indexes_count;
    uint16_t * pos_indices;
    Vec3f * positions;
} Mesh;


