#pragma once

#include "../math/mat4.h"

#include "mesh.h"
#include "renderable.h"
#include "material.h"

typedef struct Object {
    Mesh * mesh;
    Mat4 transform;
    Material * material;
    Vec2f * textCoord;
    uint32_t textCoord_count;
    uint8_t texture_mapping_valid;

    /*
     * Zero is the backward-compatible active state for statically initialized
     * and legacy objects. The VDU object-active command stores the inverse so
     * an inactive object can be rejected before any renderer work or
     * diagnostic accounting.
     */
    uint8_t inactive;
} Object;

Renderable object_as_renderable(Object * object);
int objectUpdateTextureMappingValidity(Object * object);
