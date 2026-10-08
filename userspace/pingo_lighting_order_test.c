/* Link with --wrap=vec3Normalize: no production counters or ABI changes. */
#define main existing_diagnostics_tests
#include "pingo_renderer_diagnostics_test.c"
#undef main

static unsigned normalizations;
static unsigned prior_fan_rejections;
static Renderer * observed_renderer;
Vec3f __real_vec3Normalize(Vec3f);
Vec3f __wrap_vec3Normalize(Vec3f value) {
    normalizations++;
    if (observed_renderer) {
        const RendererDiagnostics *d = &observed_renderer->diagnostics;
        prior_fan_rejections = d->triangles_projection_rejected +
            d->triangles_backface_rejected + d->triangles_degenerate +
            d->triangles_bbox_rejected;
    }
    return __real_vec3Normalize(value);
}

static uint32_t seed = 0x390040;
static float random_coordinate(void) {
    seed = seed * 1664525u + 1013904223u;
    return ((float)(seed >> 8) / 16777215.0f - 0.5f) * 6.0f;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    assert(existing_diagnostics_tests() == 0);
    FILE *witness = fopen(argv[1], "wb");
    assert(witness);
    unsigned rejected = 0, contributing = 0, disabled = 0, later_fan = 0;
    unsigned long total_normals = 0;
    for (unsigned trial = 0; trial < 30000; ++trial) {
        Renderer r;
        Scene scene;
        BackEnd backend;
        TestBuffers buffers;
        Mesh mesh;
        Object object;
        Vec3f positions[3];
        for (unsigned v = 0; v < 3; ++v)
            positions[v] = (Vec3f){random_coordinate(), random_coordinate(),
                                  random_coordinate() * 0.5f - 0.5f};
        uint16_t indices[] = {0,1,2};
        Vec2f uv[] = {{0,0},{1,0},{0,1}};
        Pixel pixels[] = {{0xFF},{0xCB},{0xD5},{0xEC}};
        Texture texture;
        Material material = {.texture = &texture};
        initialize_renderer(&r, &scene, &backend, &buffers);
        add_mesh_object(&scene, &object, &mesh, positions, indices, 3);
        if (trial % 5 == 0) {
            use_production_projection(&r);
            object.transform = mat4Translate((Vec3f){0,0,-4});
        }
        if (trial % 7 == 0) {
            r.camera_view = mat4RotateY(random_coordinate());
            Mat4 pitch = mat4RotateX(random_coordinate());
            r.camera_view = mat4MultiplyM(&r.camera_view, &pitch);
        }
        if (trial % 11 == 0)
            object.transform = mat4Scale((Vec3f){-2,0.5f,1.25f});
        if (trial % 101 == 0) {
            positions[2] = positions[1];
            assert(meshUpdateGeometryValidity(&mesh));
        }
        if (trial % 103 == 0) {
            positions[0].x = NAN;
            assert(!meshUpdateGeometryValidity(&mesh));
        }
        if (trial % 107 == 0) {
            positions[1].z = 1000000.0f;
            meshUpdateGeometryValidity(&mesh);
        }
        if (trial % 109 == 0) {
            indices[2] = 3;
            assert(!meshUpdateGeometryValidity(&mesh));
        }
        assert(texture_init(&texture, (Vec2i){2,2}, pixels) == 0);
        if (trial % 3) {
            object.material = &material;
            mesh.textCoord = uv; mesh.texture_coordinates_count = 3;
            mesh.tex_indices = indices; mesh.texture_indexes_count = 3;
            mesh.shading_mode = trial % 3 == 1 ? MESH_SHADING_FLAT_PALETTE : 0;
            objectUpdateTextureMappingValidity(&object);
        }
        if (trial % 4 == 0) r.illuminationEnabled = 0;
        if (trial % 4 == 1) mesh.illumination_policy = MESH_ILLUMINATION_SELF_ILLUMINATED;
        assert(rendererSetLightDirection(&r,
            (Vec3f){random_coordinate(),random_coordinate(),random_coordinate()}) == 0);
        rendererSetLightIntensity(&r, (uint8_t)trial);
        rendererSetAmbientLight(&r, (uint8_t)(trial / 7));
        normalizations = prior_fan_rejections = 0;
        observed_renderer = &r;
        assert(rendererRender(&r) == 0);
        observed_renderer = NULL;
        assert_diagnostic_invariants(&r.diagnostics);
        const int lit = r.illuminationEnabled &&
            mesh.illumination_policy != MESH_ILLUMINATION_SELF_ILLUMINATED;
        assert(normalizations <= 1);
        if (!lit) { assert(!normalizations); disabled++; }
#ifdef P039_EXPECT_DEFERRED
        assert(normalizations == (unsigned)(lit && r.diagnostics.triangles_rasterized > 0));
#endif
        if (lit && !r.diagnostics.triangles_rasterized) rejected++;
        if (lit && r.diagnostics.triangles_rasterized) contributing++;
        if (normalizations && prior_fan_rejections && r.diagnostics.triangles_rasterized) later_fan++;
        total_normals += normalizations;
        assert(fwrite(buffers.frame, sizeof(buffers.frame), 1, witness) == 1);
        assert(fwrite(buffers.depth, sizeof(buffers.depth), 1, witness) == 1);
    }
    assert(!fclose(witness));
    assert(rejected > 1000 && contributing > 1000 && disabled > 1000);
#ifdef P039_EXPECT_DEFERRED
    assert(later_fan > 0);
#endif
    printf("30000 exact-witness states: normals=%lu rejected=%u contributing=%u disabled=%u later-fan=%u\n",
        total_normals, rejected, contributing, disabled, later_fan);
    return 0;
}
