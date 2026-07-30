#include "object.h"

#include <math.h>

Renderable object_as_renderable(Object * object)
{
    return (Renderable){.renderableType = RENDERABLE_OBJECT, .impl = object};
}

int objectUpdateTextureMappingValidity(Object * object)
{
    if (!object) {
        return 0;
    }

    object->texture_mapping_valid = 0;
    if (!object->material) {
        return 1;
    }
    if (!object->mesh ||
        !object->mesh->tex_indices ||
        object->mesh->texture_indexes_count == 0 ||
        object->mesh->texture_indexes_count !=
            (uint32_t)object->mesh->indexes_count) {
        return 0;
    }

    Vec2f * coordinates = object->textCoord;
    uint32_t coordinate_count = object->textCoord_count;
    if (!coordinates) {
        coordinates = object->mesh->textCoord;
        coordinate_count = object->mesh->texture_coordinates_count;
    }
    if (!coordinates || coordinate_count == 0) {
        return 0;
    }

    for (uint32_t i = 0;
         i < object->mesh->texture_indexes_count;
         i++) {
        uint16_t index = object->mesh->tex_indices[i];
        if (index >= coordinate_count ||
            !isfinite(coordinates[index].x) ||
            !isfinite(coordinates[index].y)) {
            return 0;
        }
    }

    object->texture_mapping_valid = 1;
    return 1;
}
