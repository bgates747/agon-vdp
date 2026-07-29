#include "mesh.h"

#include <math.h>

int meshUpdateBounds(Mesh * mesh) {
    if (!mesh) {
        return 0;
    }

    mesh->bounds_valid = 0;
    if (!mesh->positions || mesh->positions_count == 0) {
        return 0;
    }

    Vec3f minimum = mesh->positions[0];
    Vec3f maximum = mesh->positions[0];
    if (!isfinite(minimum.x) ||
        !isfinite(minimum.y) ||
        !isfinite(minimum.z)) {
        return 0;
    }

    for (uint32_t i = 1; i < mesh->positions_count; i++) {
        Vec3f position = mesh->positions[i];
        if (!isfinite(position.x) ||
            !isfinite(position.y) ||
            !isfinite(position.z)) {
            return 0;
        }

        if (position.x < minimum.x) minimum.x = position.x;
        if (position.y < minimum.y) minimum.y = position.y;
        if (position.z < minimum.z) minimum.z = position.z;
        if (position.x > maximum.x) maximum.x = position.x;
        if (position.y > maximum.y) maximum.y = position.y;
        if (position.z > maximum.z) maximum.z = position.z;
    }

    mesh->bounds_min = minimum;
    mesh->bounds_max = maximum;
    mesh->bounds_valid = 1;
    return 1;
}
