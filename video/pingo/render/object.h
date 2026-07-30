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
} Object;

Renderable object_as_renderable(Object * object);
int objectUpdateTextureMappingValidity(Object * object);
