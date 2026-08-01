#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../video/pingo/render/material.h"
#include "../video/pingo/render/mesh.h"
#include "../video/pingo/render/object.h"

static void test_geometry_validity(void) {
    Vec3f positions[] = {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f}
    };
    uint16_t indices[] = {0, 1, 2};
    Mesh mesh;
    memset(&mesh, 0, sizeof(mesh));

    mesh.pos_indices = indices;
    mesh.indexes_count = 3;
    assert(meshUpdateGeometryValidity(&mesh) == 0);
    assert(mesh.geometry_valid == 0);

    mesh.positions = positions;
    mesh.positions_count = 3;
    assert(meshUpdateGeometryValidity(&mesh) == 1);
    assert(mesh.geometry_valid == 1);

    mesh.indexes_count = 2;
    assert(meshUpdateGeometryValidity(&mesh) == 0);
    mesh.indexes_count = 3;

    indices[2] = 3;
    assert(meshUpdateGeometryValidity(&mesh) == 0);
    indices[2] = 2;

    positions[2].x = NAN;
    assert(meshUpdateGeometryValidity(&mesh) == 0);
    positions[2].x = -1.0f;
    assert(meshUpdateGeometryValidity(&mesh) == 1);

    assert(meshUpdateGeometryValidity(NULL) == 0);
}

static void test_texture_mapping_validity(void) {
    Vec3f positions[] = {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f}
    };
    Vec2f mesh_coordinates[] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f}
    };
    Vec2f object_coordinates[] = {
        {0.25f, 0.25f},
        {0.75f, 0.25f},
        {0.25f, 0.75f}
    };
    uint16_t position_indices[] = {0, 1, 2};
    uint16_t texture_indices[] = {0, 1, 2};
    Material material = {0};
    Mesh mesh = {
        .indexes_count = 3,
        .pos_indices = position_indices,
        .tex_indices = texture_indices,
        .positions = positions,
        .textCoord = mesh_coordinates,
        .positions_count = 3,
        .texture_coordinates_count = 3,
        .texture_indexes_count = 3
    };
    Object object = {
        .mesh = &mesh,
        .material = &material
    };

    assert(meshUpdateGeometryValidity(&mesh) == 1);
    assert(objectUpdateTextureMappingValidity(&object) == 1);
    assert(object.texture_mapping_valid == 1);

    mesh.texture_indexes_count = 2;
    assert(objectUpdateTextureMappingValidity(&object) == 0);
    mesh.texture_indexes_count = 3;

    texture_indices[2] = 3;
    assert(objectUpdateTextureMappingValidity(&object) == 0);
    texture_indices[2] = 2;

    object.textCoord = object_coordinates;
    object.textCoord_count = 3;
    assert(objectUpdateTextureMappingValidity(&object) == 1);

    object.textCoord_count = 2;
    assert(objectUpdateTextureMappingValidity(&object) == 0);
    object.textCoord_count = 3;

    object_coordinates[1].y = INFINITY;
    assert(objectUpdateTextureMappingValidity(&object) == 0);
    object_coordinates[1].y = 0.25f;
    assert(objectUpdateTextureMappingValidity(&object) == 1);

    object.material = NULL;
    assert(objectUpdateTextureMappingValidity(&object) == 1);
    assert(object.texture_mapping_valid == 0);
    assert(objectUpdateTextureMappingValidity(NULL) == 0);
}

int main(void) {
    test_geometry_validity();
    test_texture_mapping_validity();
    puts("Pingo mesh validation test passed");
    return 0;
}
