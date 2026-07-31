#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../video/pingo/render/depth.h"
#include "../video/pingo/render/mesh.h"
#include "../video/pingo/render/object.h"
#include "../video/pingo/render/renderer.h"
#include "../video/pingo/render/backend.h"
#include "../video/pingo/render/scene.h"

#if !PINGO_RENDER_DIAGNOSTICS
#error "pingo_renderer_diagnostics_test requires PINGO_RENDER_DIAGNOSTICS"
#endif

enum {
    TEST_WIDTH = 8,
    TEST_HEIGHT = 8,
    TEST_PIXELS = TEST_WIDTH * TEST_HEIGHT
};

typedef struct tag_TestBuffers {
    Pixel frame[TEST_PIXELS];
    PingoDepth depth[TEST_PIXELS];
} TestBuffers;

static uint32_t fake_clock_ticks;
static uint32_t captured_pixels;
static float captured_minimum_illumination;
static float captured_maximum_illumination;

static uint32_t fake_clock(void) {
    fake_clock_ticks += 10;
    return fake_clock_ticks;
}

static void test_backend_init(
        Renderer * renderer, BackEnd * backend, Vec4i rect) {
    (void)renderer;
    (void)backend;
    (void)rect;
}

static void test_backend_render(Renderer * renderer, BackEnd * backend) {
    (void)renderer;
    (void)backend;
}

static Pixel * test_frame_buffer(Renderer * renderer, BackEnd * backend) {
    (void)renderer;
    TestBuffers * buffers = backend->clientCustomData;
    return buffers->frame;
}

static PingoDepth * test_depth_buffer(
        Renderer * renderer, BackEnd * backend) {
    (void)renderer;
    TestBuffers * buffers = backend->clientCustomData;
    return buffers->depth;
}

static void capture_backend_pixel(
        Texture * frame, Vec2i position, Pixel color, float illumination) {
    (void)frame;
    (void)position;
    (void)color;
    captured_pixels++;
    captured_minimum_illumination = fminf(
        captured_minimum_illumination, illumination);
    captured_maximum_illumination = fmaxf(
        captured_maximum_illumination, illumination);
}

static void reset_backend_capture(void) {
    captured_pixels = 0;
    captured_minimum_illumination = INFINITY;
    captured_maximum_illumination = -INFINITY;
}

static void initialize_renderer(
        Renderer * renderer,
        Scene * scene,
        BackEnd * backend,
        TestBuffers * buffers) {
    memset(buffers, 0, sizeof(*buffers));
    *backend = (BackEnd) {
        .init = test_backend_init,
        .beforeRender = test_backend_render,
        .afterRender = test_backend_render,
        .getFrameBuffer = test_frame_buffer,
        .drawPixel = 0,
        .getZetaBuffer = test_depth_buffer,
        .clientCustomData = buffers
    };

    assert(rendererInit(
        renderer, (Vec2i){TEST_WIDTH, TEST_HEIGHT}, backend) == 0);
    assert(rendererSetCamera(
        renderer, (Vec4i){0, 0, TEST_WIDTH, TEST_HEIGHT}) == 0);
    assert(sceneInit(scene) == 0);
    assert(rendererSetScene(renderer, scene) == 0);

    renderer->camera_projection = mat4Identity();
    renderer->camera_view = mat4Identity();
    assert(renderer->frustumCulling == 1);
    renderer->diagnostics_clock = fake_clock;
    renderer->diagnostics_clock_hz = 1000000;
}

static void use_production_projection(Renderer * renderer) {
    renderer->camera_projection =
        mat4Perspective(1.0f, 2500.0f, 4.0f / 3.0f, 0.6f);
}

static void add_mesh_object(
        Scene * scene,
        Object * object,
        Mesh * mesh,
        Vec3f * positions,
        uint16_t * indices,
        int index_count) {
    *mesh = (Mesh) {
        .indexes_count = index_count,
        .pos_indices = indices,
        .tex_indices = 0,
        .positions = positions,
        .textCoord = 0
    };
    for (int i = 0; i < index_count; i++) {
        uint32_t required_count = (uint32_t)indices[i] + 1U;
        if (required_count > mesh->positions_count) {
            mesh->positions_count = required_count;
        }
    }
    assert(meshUpdateGeometryValidity(mesh) == 1);
    *object = (Object) {
        .mesh = mesh,
        .transform = mat4Identity(),
        .material = 0,
        .textCoord = 0
    };
    assert(sceneAddRenderable(scene, object_as_renderable(object)) == 0);
}

static void add_bounded_mesh_object(
        Scene * scene,
        Object * object,
        Mesh * mesh,
        Vec3f * positions,
        uint32_t position_count,
        uint16_t * indices,
        int index_count) {
    add_mesh_object(
        scene, object, mesh, positions, indices, index_count);
    mesh->positions_count = position_count;
    assert(meshUpdateGeometryValidity(mesh) == 1);
    assert(meshUpdateBounds(mesh) == 1);
}

static void assert_diagnostic_invariants(
        const RendererDiagnostics * diagnostics) {
    assert(
        diagnostics->objects_frustum_rejected <=
        diagnostics->objects_bounds_tested);
    assert(
        diagnostics->objects_bounds_tested <=
        diagnostics->objects);
    assert(
        diagnostics->triangles_submitted ==
        diagnostics->triangles_z_rejected +
        diagnostics->triangles_frustum_rejected +
        diagnostics->triangles_clipped +
        diagnostics->triangles_unclipped);
    assert(
        diagnostics->triangles_generated ==
        diagnostics->triangles_projection_rejected +
        diagnostics->triangles_backface_rejected +
        diagnostics->triangles_degenerate +
        diagnostics->triangles_bbox_rejected +
        diagnostics->triangles_rasterized);
    assert(
        diagnostics->fragments_covered ==
        diagnostics->fragments_depth_range_rejected +
        diagnostics->fragments_depth_test_rejected +
        diagnostics->fragments_reciprocal_w_rejected +
        diagnostics->fragments_shaded);
    assert(
        diagnostics->fragments_covered <=
        diagnostics->fragments_bbox);
}

static void test_mesh_bounds_cache(void) {
    Vec3f positions[] = {
        { 2.0f, -3.0f,  4.0f},
        {-5.0f,  6.0f, -7.0f},
        { 1.0f, -0.0f,  3.0f},
        { 0.0f,  2.0f, -1.0f}
    };
    Mesh mesh = {
        .positions = positions,
        .positions_count = 4
    };

    assert(meshUpdateBounds(&mesh) == 1);
    assert(mesh.bounds_valid == 1);
    assert(mesh.bounds_min.x == -5.0f);
    assert(mesh.bounds_min.y == -3.0f);
    assert(mesh.bounds_min.z == -7.0f);
    assert(mesh.bounds_max.x == 2.0f);
    assert(mesh.bounds_max.y == 6.0f);
    assert(mesh.bounds_max.z == 4.0f);

    positions[3].x = NAN;
    assert(meshUpdateBounds(&mesh) == 0);
    assert(mesh.bounds_valid == 0);
    positions[3].x = 0.0f;

    positions[3].y = INFINITY;
    assert(meshUpdateBounds(&mesh) == 0);
    assert(mesh.bounds_valid == 0);
    positions[3].y = 2.0f;

    mesh.positions_count = 0;
    assert(meshUpdateBounds(&mesh) == 0);
    assert(mesh.bounds_valid == 0);
    mesh.positions_count = 4;

    mesh.positions = 0;
    assert(meshUpdateBounds(&mesh) == 0);
    assert(mesh.bounds_valid == 0);
    assert(meshUpdateBounds(0) == 0);
}

static void test_empty_scene(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    initialize_renderer(&renderer, &scene, &backend, &buffers);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.clear_ticks > 0);
    assert(renderer.diagnostics.objects == 0);
    assert(renderer.diagnostics.triangles_submitted == 0);
    assert(renderer.diagnostics.fragments_bbox == 0);
    assert(renderer.diagnostics.fragments_shaded == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_front_triangle_and_reset(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.75f, -0.75f, -0.5f},
        { 0.75f, -0.75f, -0.5f},
        {-0.75f,  0.75f, -0.5f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    RendererDiagnostics first = renderer.diagnostics;
    assert(first.objects == 1);
    assert(first.triangles_submitted == 1);
    assert(first.triangles_rasterized == 1);
    assert(first.fragments_bbox > 0);
    assert(first.fragments_covered > 0);
    assert(first.fragments_shaded > 0);
    assert(first.transform_ticks > 0);
    assert(first.triangle_setup_ticks > 0);
    assert(first.raster_ticks > 0);
    assert_diagnostic_invariants(&first);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.objects == first.objects);
    assert(
        renderer.diagnostics.triangles_submitted ==
        first.triangles_submitted);
    assert(
        renderer.diagnostics.fragments_bbox ==
        first.fragments_bbox);
    assert(
        renderer.diagnostics.fragments_covered ==
        first.fragments_covered);
    assert(
        renderer.diagnostics.fragments_shaded ==
        first.fragments_shaded);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_reversed_winding_is_rejected(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.75f, -0.75f, -0.5f},
        { 0.75f, -0.75f, -0.5f},
        {-0.75f,  0.75f, -0.5f}
    };
    uint16_t indices[] = {0, 2, 1};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 1);
    assert(renderer.diagnostics.triangles_backface_rejected == 1);
    assert(renderer.diagnostics.triangles_rasterized == 0);
    assert(renderer.diagnostics.fragments_bbox == 0);
    assert(renderer.diagnostics.fragments_shaded == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_overdraw_is_counted(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.75f, -0.75f, -0.5f},
        { 0.75f, -0.75f, -0.5f},
        {-0.75f,  0.75f, -0.5f},
        {-0.75f, -0.75f, -0.8f},
        { 0.75f, -0.75f, -0.8f},
        {-0.75f,  0.75f, -0.8f}
    };
    uint16_t indices[] = {0, 1, 2, 3, 4, 5};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 6);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 2);
    assert(renderer.diagnostics.triangles_rasterized == 2);
    assert(renderer.diagnostics.fragments_depth_test_rejected > 0);
    assert(renderer.diagnostics.fragments_shaded > 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_z_rejection(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.75f, -0.75f, 0.5f},
        { 0.75f, -0.75f, 0.5f},
        {-0.75f,  0.75f, 0.5f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_z_rejected == 1);
    assert(renderer.diagnostics.triangles_rasterized == 0);
    assert(renderer.diagnostics.fragments_bbox == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_remaining_frustum_planes_are_rejected(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.75f, -0.75f, -2.0f},
        { 0.75f, -0.75f, -2.0f},
        {-0.75f,  0.75f, -2.0f},
        {-2.00f, -0.75f, -0.5f},
        {-1.25f, -0.75f, -0.5f},
        {-2.00f,  0.75f, -0.5f},
        { 1.25f, -0.75f, -0.5f},
        { 2.00f, -0.75f, -0.5f},
        { 1.25f,  0.75f, -0.5f},
        {-0.75f, -2.00f, -0.5f},
        { 0.75f, -2.00f, -0.5f},
        {-0.75f, -1.25f, -0.5f},
        {-0.75f,  1.25f, -0.5f},
        { 0.75f,  1.25f, -0.5f},
        {-0.75f,  2.00f, -0.5f}
    };
    uint16_t indices[] = {
         0,  1,  2,
         3,  4,  5,
         6,  7,  8,
         9, 10, 11,
        12, 13, 14
    };

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 15);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 5);
    assert(renderer.diagnostics.triangles_z_rejected == 0);
    assert(renderer.diagnostics.triangles_frustum_rejected == 5);
    assert(renderer.diagnostics.triangles_rasterized == 0);
    assert(renderer.diagnostics.fragments_bbox == 0);
    assert(renderer.diagnostics.fragments_shaded == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_object_bounds_common_planes_are_rejected(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh meshes[6];
    Object objects[6];
    Vec3f positions[6][3] = {
        {
            {-0.25f, -0.25f,  0.25f},
            { 0.25f, -0.25f,  0.25f},
            {-0.25f,  0.25f,  0.25f}
        },
        {
            {-0.25f, -0.25f, -2.00f},
            { 0.25f, -0.25f, -2.00f},
            {-0.25f,  0.25f, -2.00f}
        },
        {
            {-2.00f, -0.25f, -0.50f},
            {-1.25f, -0.25f, -0.50f},
            {-2.00f,  0.25f, -0.50f}
        },
        {
            { 1.25f, -0.25f, -0.50f},
            { 2.00f, -0.25f, -0.50f},
            { 1.25f,  0.25f, -0.50f}
        },
        {
            {-0.25f, -2.00f, -0.50f},
            { 0.25f, -2.00f, -0.50f},
            {-0.25f, -1.25f, -0.50f}
        },
        {
            {-0.25f,  1.25f, -0.50f},
            { 0.25f,  1.25f, -0.50f},
            {-0.25f,  2.00f, -0.50f}
        }
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    for (uint32_t i = 0; i < 6; i++) {
        add_bounded_mesh_object(
            &scene, &objects[i], &meshes[i],
            positions[i], 3, indices, 3);
    }

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.objects == 6);
    assert(renderer.diagnostics.objects_bounds_tested == 6);
    assert(renderer.diagnostics.objects_frustum_rejected == 6);
    assert(renderer.diagnostics.triangles_avoided == 6);
    assert(renderer.diagnostics.triangles_submitted == 0);
    assert(renderer.diagnostics.fragments_bbox == 0);
    assert(renderer.diagnostics.fragments_shaded == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_object_bounds_eye_plane_is_rejected(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.25f, -0.25f, -0.50f},
        { 0.25f, -0.25f, -0.50f},
        {-0.25f,  0.25f, -0.50f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    renderer.camera_projection.elements[15] = 0.0f;
    add_bounded_mesh_object(
        &scene, &object, &mesh, positions, 3, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.objects_bounds_tested == 1);
    assert(renderer.diagnostics.objects_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_avoided == 1);
    assert(renderer.diagnostics.triangles_submitted == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_object_bounds_boundaries_and_crossings_are_retained(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh meshes[7];
    Object objects[7];
    const float right_outside = nextafterf(1.0f, INFINITY);
    const float near_outside = nextafterf(0.0f, INFINITY);
    const float far_outside = nextafterf(-1.0f, -INFINITY);
    Vec3f positions[7][3] = {
        {
            { 1.00f, -0.25f, -0.50f},
            { 1.00f,  0.25f, -0.50f},
            { 1.00f,  0.00f, -0.50f}
        },
        {
            { right_outside, -0.25f, -0.50f},
            { right_outside + 0.25f, -0.25f, -0.50f},
            { right_outside,  0.25f, -0.50f}
        },
        {
            {-0.25f, -0.25f,  0.00f},
            { 0.25f, -0.25f,  0.00f},
            {-0.25f,  0.25f,  0.00f}
        },
        {
            {-0.25f, -0.25f, near_outside},
            { 0.25f, -0.25f, near_outside},
            {-0.25f,  0.25f, near_outside}
        },
        {
            {-0.25f, -0.25f, -1.00f},
            { 0.25f, -0.25f, -1.00f},
            {-0.25f,  0.25f, -1.00f}
        },
        {
            {-0.25f, -0.25f, far_outside},
            { 0.25f, -0.25f, far_outside},
            {-0.25f,  0.25f, far_outside}
        },
        {
            { 0.75f, -0.25f, -0.50f},
            { 1.25f, -0.25f, -0.50f},
            { 0.75f,  0.25f, -0.50f}
        }
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    for (uint32_t i = 0; i < 7; i++) {
        add_bounded_mesh_object(
            &scene, &objects[i], &meshes[i],
            positions[i], 3, indices, 3);
    }

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.objects_bounds_tested == 7);
    assert(renderer.diagnostics.objects_frustum_rejected == 3);
    assert(renderer.diagnostics.triangles_avoided == 3);
    assert(renderer.diagnostics.triangles_submitted == 4);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_object_bounds_transform_and_reference_outputs_match(void) {
    Renderer enabled;
    Renderer reference;
    Scene enabled_scene;
    Scene reference_scene;
    BackEnd enabled_backend;
    BackEnd reference_backend;
    TestBuffers enabled_buffers;
    TestBuffers reference_buffers;
    Mesh enabled_mesh;
    Mesh reference_mesh;
    Object enabled_object;
    Object reference_object;
    Vec3f positions[] = {
        {-2.0f, -0.1f, -0.5f},
        { 2.0f, -0.1f, -0.5f},
        {-2.0f,  0.1f, -0.5f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(
        &enabled, &enabled_scene, &enabled_backend, &enabled_buffers);
    initialize_renderer(
        &reference, &reference_scene, &reference_backend, &reference_buffers);
    add_bounded_mesh_object(
        &enabled_scene, &enabled_object, &enabled_mesh,
        positions, 3, indices, 3);
    add_mesh_object(
        &reference_scene, &reference_object, &reference_mesh,
        positions, indices, 3);

    enabled_object.transform = mat4RotateZ(1.57079632679f);
    reference_object.transform = enabled_object.transform;
    enabled_scene.transform = mat4Translate((Vec3f){1.5f, 0.0f, 0.0f});
    reference_scene.transform = enabled_scene.transform;

    assert(rendererRender(&enabled) == 0);
    assert(rendererRender(&reference) == 0);
    assert(enabled.diagnostics.objects_frustum_rejected == 1);
    assert(enabled.diagnostics.triangles_avoided == 1);
    assert(enabled.diagnostics.triangles_submitted == 0);
    assert(reference.diagnostics.objects_bounds_tested == 0);
    assert(reference.diagnostics.triangles_submitted == 1);
    assert(reference.diagnostics.triangles_frustum_rejected == 1);
    assert(
        memcmp(
            enabled_buffers.frame,
            reference_buffers.frame,
            sizeof(enabled_buffers.frame)) == 0);
    assert(
        memcmp(
            enabled_buffers.depth,
            reference_buffers.depth,
            sizeof(enabled_buffers.depth)) == 0);
    assert_diagnostic_invariants(&enabled.diagnostics);
    assert_diagnostic_invariants(&reference.diagnostics);
}

static void test_object_bounds_nonuniform_negative_scale_boundary(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh meshes[2];
    Object objects[2];
    Vec3f positions[] = {
        {-0.25f, -0.25f, -0.50f},
        { 0.50f, -0.25f, -0.50f},
        {-0.25f,  0.25f, -0.50f}
    };
    uint16_t indices[] = {0, 1, 2};
    Mat4 scale = mat4Scale((Vec3f){-2.0f, 0.5f, 1.0f});
    Mat4 boundary_translation =
        mat4Translate((Vec3f){2.0f, 0.0f, 0.0f});
    Mat4 outside_translation = mat4Translate(
        (Vec3f){nextafterf(2.0f, INFINITY), 0.0f, 0.0f});

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_bounded_mesh_object(
        &scene, &objects[0], &meshes[0], positions, 3, indices, 3);
    add_bounded_mesh_object(
        &scene, &objects[1], &meshes[1], positions, 3, indices, 3);
    objects[0].transform =
        mat4MultiplyM(&scale, &boundary_translation);
    objects[1].transform =
        mat4MultiplyM(&scale, &outside_translation);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.objects_bounds_tested == 2);
    assert(renderer.diagnostics.objects_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_avoided == 1);
    assert(renderer.diagnostics.triangles_submitted == 1);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_object_bounds_invalid_and_disabled_fail_open(void) {
    Renderer invalid;
    Renderer disabled;
    Scene invalid_scene;
    Scene disabled_scene;
    BackEnd invalid_backend;
    BackEnd disabled_backend;
    TestBuffers invalid_buffers;
    TestBuffers disabled_buffers;
    Mesh invalid_meshes[2];
    Mesh disabled_mesh;
    Object invalid_objects[2];
    Object disabled_object;
    Vec3f positions[] = {
        {1.25f, -0.25f, -0.50f},
        {2.00f, -0.25f, -0.50f},
        {1.25f,  0.25f, -0.50f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(
        &invalid, &invalid_scene, &invalid_backend, &invalid_buffers);
    initialize_renderer(
        &disabled, &disabled_scene, &disabled_backend, &disabled_buffers);
    for (uint32_t i = 0; i < 2; i++) {
        add_bounded_mesh_object(
            &invalid_scene, &invalid_objects[i], &invalid_meshes[i],
            positions, 3, indices, 3);
    }
    add_bounded_mesh_object(
        &disabled_scene, &disabled_object, &disabled_mesh,
        positions, 3, indices, 3);

    invalid_meshes[0].bounds_min.x = NAN;
    invalid_meshes[1].bounds_min.x = 2.0f;
    invalid_meshes[1].bounds_max.x = 1.0f;
    rendererSetFrustumCulling(&disabled, 0);

    assert(rendererRender(&invalid) == 0);
    assert(rendererRender(&disabled) == 0);
    assert(invalid.diagnostics.objects_bounds_tested == 2);
    assert(invalid.diagnostics.objects_frustum_rejected == 0);
    assert(invalid.diagnostics.triangles_avoided == 0);
    assert(invalid.diagnostics.triangles_submitted == 2);
    assert(invalid.diagnostics.triangles_frustum_rejected == 2);
    assert(disabled.diagnostics.objects_bounds_tested == 0);
    assert(disabled.diagnostics.objects_frustum_rejected == 0);
    assert(disabled.diagnostics.triangles_avoided == 0);
    assert(disabled.diagnostics.triangles_submitted == 1);
    assert_diagnostic_invariants(&invalid.diagnostics);
    assert_diagnostic_invariants(&disabled.diagnostics);
}

static void test_clip_boundaries_and_crossings_are_retained(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        /* Exact right, left, top, bottom, far, and near boundaries. */
        { 1.00f, -0.75f, -0.5f},
        { 1.00f,  0.75f, -0.5f},
        { 1.00f,  0.00f, -0.5f},
        {-1.00f, -0.75f, -0.5f},
        {-1.00f,  0.75f, -0.5f},
        {-1.00f,  0.00f, -0.5f},
        {-0.75f,  1.00f, -0.5f},
        { 0.75f,  1.00f, -0.5f},
        { 0.00f,  1.00f, -0.5f},
        {-0.75f, -1.00f, -0.5f},
        { 0.75f, -1.00f, -0.5f},
        { 0.00f, -1.00f, -0.5f},
        {-0.75f, -0.75f, -1.0f},
        { 0.75f, -0.75f, -1.0f},
        {-0.75f,  0.75f, -1.0f},
        {-0.75f, -0.75f,  0.0f},
        { 0.75f, -0.75f,  0.0f},
        {-0.75f,  0.75f,  0.0f},
        /* Two vertices outside right, one inside: not a trivial reject. */
        { 0.00f, -0.50f, -0.5f},
        { 1.50f, -0.50f, -0.5f},
        { 1.50f,  0.50f, -0.5f},
        /* Vertices outside different planes: no common outside plane. */
        {-2.00f, -0.50f, -0.5f},
        { 2.00f, -0.50f, -0.5f},
        { 0.00f,  2.00f, -0.5f}
    };
    uint16_t indices[] = {
         0,  1,  2,
         3,  4,  5,
         6,  7,  8,
         9, 10, 11,
        12, 13, 14,
        15, 16, 17,
        18, 19, 20,
        21, 22, 23
    };

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 24);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 8);
    assert(renderer.diagnostics.triangles_z_rejected == 0);
    assert(renderer.diagnostics.triangles_frustum_rejected == 0);
    assert(renderer.diagnostics.triangles_rasterized > 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_actual_projection_uses_zero_as_near_clip_boundary(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.25f, -0.25f, -0.75f},
        { 0.25f, -0.25f, -0.75f},
        {-0.25f,  0.25f, -0.75f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    use_production_projection(&renderer);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_z_rejected == 1);
    assert(renderer.diagnostics.triangles_frustum_rejected == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_actual_projection_clips_near_crossings(void) {
    Renderer one_outside;
    Renderer two_outside;
    Scene one_outside_scene;
    Scene two_outside_scene;
    BackEnd one_outside_backend;
    BackEnd two_outside_backend;
    TestBuffers one_outside_buffers;
    TestBuffers two_outside_buffers;
    Mesh one_outside_mesh;
    Mesh two_outside_mesh;
    Object one_outside_object;
    Object two_outside_object;
    Vec3f one_outside_positions[] = {
        {-0.60f, -0.50f, -2.00f},
        { 0.60f, -0.50f, -2.00f},
        { 0.00f,  0.00f, -0.50f}
    };
    Vec3f two_outside_positions[] = {
        {-0.60f, -0.50f, -2.00f},
        { 0.30f, -0.10f, -0.50f},
        {-0.20f,  0.40f, -0.50f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(
        &one_outside, &one_outside_scene,
        &one_outside_backend, &one_outside_buffers);
    initialize_renderer(
        &two_outside, &two_outside_scene,
        &two_outside_backend, &two_outside_buffers);
    use_production_projection(&one_outside);
    use_production_projection(&two_outside);
    add_mesh_object(
        &one_outside_scene, &one_outside_object,
        &one_outside_mesh, one_outside_positions, indices, 3);
    add_mesh_object(
        &two_outside_scene, &two_outside_object,
        &two_outside_mesh, two_outside_positions, indices, 3);

    assert(rendererRender(&one_outside) == 0);
    assert(one_outside.diagnostics.triangles_submitted == 1);
    assert(one_outside.diagnostics.triangles_clipped == 1);
    assert(one_outside.diagnostics.triangles_unclipped == 0);
    assert(one_outside.diagnostics.triangles_generated == 2);
    assert(
        one_outside.diagnostics.triangles_projection_rejected == 0);
    assert(one_outside.diagnostics.triangles_rasterized > 0);
    assert(one_outside.diagnostics.fragments_shaded > 0);
    assert_diagnostic_invariants(&one_outside.diagnostics);

    assert(rendererRender(&two_outside) == 0);
    assert(two_outside.diagnostics.triangles_submitted == 1);
    assert(two_outside.diagnostics.triangles_clipped == 1);
    assert(two_outside.diagnostics.triangles_unclipped == 0);
    assert(two_outside.diagnostics.triangles_generated == 1);
    assert(
        two_outside.diagnostics.triangles_projection_rejected == 0);
    assert(two_outside.diagnostics.triangles_rasterized == 1);
    assert(two_outside.diagnostics.fragments_shaded > 0);
    assert_diagnostic_invariants(&two_outside.diagnostics);
}

static void test_actual_projection_clips_huge_lateral_triangle(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-1.0e9f, -0.20f, -2.0f},
        { 1.0e9f, -0.20f, -2.0f},
        { 0.0f,     1.0e9f, -2.0f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    use_production_projection(&renderer);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 1);
    assert(renderer.diagnostics.triangles_clipped == 1);
    assert(renderer.diagnostics.triangles_generated > 0);
    assert(renderer.diagnostics.triangles_projection_rejected == 0);
    assert(renderer.diagnostics.triangles_rasterized > 0);
    assert(renderer.diagnostics.fragments_shaded > 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_actual_projection_rejects_nonfinite_input_safely(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.60f, -0.50f, -2.0f},
        { 0.60f, -0.50f, -2.0f},
        {-0.60f,  0.50f, -2.0f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    use_production_projection(&renderer);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    renderer.camera_projection.elements[0] = NAN;
    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 1);
    assert(renderer.diagnostics.triangles_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_generated == 0);
    assert(renderer.diagnostics.fragments_bbox == 0);
    assert(renderer.diagnostics.fragments_shaded == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);

    renderer.camera_projection.elements[0] = INFINITY;
    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 1);
    assert(renderer.diagnostics.triangles_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_generated == 0);
    assert(renderer.diagnostics.fragments_bbox == 0);
    assert(renderer.diagnostics.fragments_shaded == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_all_nonpositive_w_is_rejected_before_division(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        /*
         * Deliberately produce mixed canonical outcodes. No far or lateral
         * plane alone rejects all three vertices when W is zero.
         */
        {-2.0f,  0.0f, -0.5f},
        { 2.0f,  0.0f,  0.5f},
        { 0.0f,  2.0f,  0.0f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    renderer.camera_projection = mat4Identity();
    renderer.camera_projection.elements[15] = 0.0f;
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_rasterized == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);

    positions[0] = (Vec3f){ 2.0f, -2.0f, -2.0f};
    positions[1] = (Vec3f){-2.0f,  2.0f,  2.0f};
    positions[2] = (Vec3f){ 0.0f,  0.0f,  0.0f};
    renderer.camera_projection.elements[15] = -1.0f;
    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_rasterized == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_enabled_and_disabled_outputs_match(void) {
    Renderer enabled;
    Renderer disabled;
    Scene enabled_scene;
    Scene disabled_scene;
    BackEnd enabled_backend;
    BackEnd disabled_backend;
    TestBuffers enabled_buffers;
    TestBuffers disabled_buffers;
    Mesh enabled_mesh;
    Mesh disabled_mesh;
    Object enabled_object;
    Object disabled_object;
    Vec3f positions[] = {
        /* Visible. */
        {-0.75f, -0.75f, -0.5f},
        { 0.75f, -0.75f, -0.5f},
        {-0.75f,  0.75f, -0.5f},
        /* Crosses the right plane and must remain eligible. */
        { 0.00f, -0.50f, -0.5f},
        { 1.50f, -0.50f, -0.5f},
        { 1.50f,  0.50f, -0.5f},
        /* Wholly beyond the right plane. */
        { 1.25f, -0.75f, -0.5f},
        { 2.00f, -0.75f, -0.5f},
        { 1.25f,  0.75f, -0.5f},
        /* Wholly beyond the far plane. */
        {-0.75f, -0.75f, -2.0f},
        { 0.75f, -0.75f, -2.0f},
        {-0.75f,  0.75f, -2.0f}
    };
    uint16_t indices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    initialize_renderer(
        &enabled, &enabled_scene, &enabled_backend, &enabled_buffers);
    initialize_renderer(
        &disabled, &disabled_scene, &disabled_backend, &disabled_buffers);
    rendererSetFrustumCulling(&disabled, 0);
    assert(disabled.frustumCulling == 0);
    add_mesh_object(
        &enabled_scene,
        &enabled_object,
        &enabled_mesh,
        positions,
        indices,
        12);
    add_mesh_object(
        &disabled_scene,
        &disabled_object,
        &disabled_mesh,
        positions,
        indices,
        12);

    assert(rendererRender(&enabled) == 0);
    assert(rendererRender(&disabled) == 0);
    assert(
        memcmp(
            enabled_buffers.frame,
            disabled_buffers.frame,
            sizeof(enabled_buffers.frame)) == 0);
    assert(
        memcmp(
            enabled_buffers.depth,
            disabled_buffers.depth,
            sizeof(enabled_buffers.depth)) == 0);
    assert(enabled.diagnostics.triangles_frustum_rejected == 2);
    assert(disabled.diagnostics.triangles_frustum_rejected == 2);
    assert_diagnostic_invariants(&enabled.diagnostics);
    assert_diagnostic_invariants(&disabled.diagnostics);
}

static void test_integer_screen_degeneracy(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {0.10f, 0.10f, -0.5f},
        {0.15f, 0.10f, -0.5f},
        {0.10f, 0.15f, -0.5f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_degenerate == 1);
    assert(renderer.diagnostics.triangles_backface_rejected == 0);
    assert(renderer.diagnostics.triangles_rasterized == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_bbox_rejection_and_clamping(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {1.25f, -0.75f, -0.5f},
        {2.00f, -0.75f, -0.5f},
        {1.25f,  0.75f, -0.5f},
        {-1.50f, -0.75f, -0.5f},
        { 0.75f, -0.75f, -0.5f},
        {-0.75f,  0.75f, -0.5f}
    };
    uint16_t indices[] = {0, 1, 2, 3, 4, 5};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 6);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_bbox_rejected == 0);
    assert(renderer.diagnostics.triangles_clipped == 1);
    assert(renderer.diagnostics.triangles_generated == 2);
    assert(renderer.diagnostics.triangles_rasterized == 2);
    assert(renderer.diagnostics.triangles_bbox_clamped == 0);
    assert(renderer.diagnostics.fragments_bbox > 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_far_clip_rejection_prevents_out_of_range_depth(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Vec3f positions[] = {
        {-0.75f, -0.75f, -2.0f},
        { 0.75f, -0.75f, -2.0f},
        {-0.75f,  0.75f, -2.0f}
    };
    uint16_t indices[] = {0, 1, 2};

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    rendererSetFrustumCulling(&renderer, 0);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_submitted == 1);
    assert(renderer.diagnostics.triangles_frustum_rejected == 1);
    assert(renderer.diagnostics.triangles_generated == 0);
    assert(renderer.diagnostics.fragments_covered == 0);
    assert(renderer.diagnostics.fragments_depth_range_rejected == 0);
    assert(renderer.diagnostics.fragments_shaded == 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static uint32_t count_frame_color(
        const TestBuffers * buffers, uint8_t color) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TEST_PIXELS; i++) {
        if (buffers->frame[i].c == color) {
            count++;
        }
    }
    return count;
}

static uint32_t count_nonzero_frame_pixels(const TestBuffers * buffers) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TEST_PIXELS; i++) {
        if (buffers->frame[i].c != 0) {
            count++;
        }
    }
    return count;
}

static void test_runtime_lighting_and_flat_palette_shading(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object object;
    Material material;
    Vec3f positions[] = {
        {-1.50f, -0.75f, -0.5f},
        { 0.75f, -0.75f, -0.5f},
        {-0.75f,  0.75f, -0.5f}
    };
    uint16_t indices[] = {0, 1, 2};
    Vec2f textureCoordinates[] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {0.0f, 0.0f}
    };
    Vec2f objectTextureCoordinates[] = {
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f}
    };
    uint16_t textureIndices[] = {0, 1, 2};
    Pixel texturePixels[] = {
        {0xEA}, {0xD5},
        {0xF3}, {0xFC}
    };
    Texture texture;
    uint8_t rgba8888Pixel[] = {255, 64, 128, 255};
    Texture rgba8888Texture;

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    assert(texture_init_format(
        &texture, (Vec2i){2, 2}, texturePixels,
        TEXTURE_FORMAT_RGBA2222) == 0);
    mesh = (Mesh) {
        .indexes_count = 3,
        .pos_indices = indices,
        .tex_indices = textureIndices,
        .positions = positions,
        .textCoord = textureCoordinates,
        .positions_count = 3,
        .texture_coordinates_count = 3,
        .texture_indexes_count = 3,
        .shading_mode = MESH_SHADING_TEXTURED,
        .illumination_policy = MESH_ILLUMINATION_INHERIT_SCENE
    };
    assert(meshUpdateGeometryValidity(&mesh) == 1);
    material = (Material){.texture = &texture};
    object = (Object) {
        .mesh = &mesh,
        .transform = mat4Identity(),
        .material = &material,
        .textCoord = 0
    };
    assert(objectUpdateTextureMappingValidity(&object) == 1);
    assert(sceneAddRenderable(&scene, object_as_renderable(&object)) == 0);

    // The setter normalizes once and rejects a zero vector without changing
    // the previous usable direction.
    assert(rendererSetLightDirection(
        &renderer, (Vec3f){0.0f, 0.0f, 4.0f}) == 0);
    assert(fabsf(renderer.lightDirection.z - 1.0f) < 0.0001f);
    assert(rendererSetLightDirection(
        &renderer, (Vec3f){0.0f, 0.0f, 0.0f}) != 0);
    assert(fabsf(renderer.lightDirection.z - 1.0f) < 0.0001f);

    // Textured, unlit rendering samples more than the source triangle's
    // first palette color.
    rendererSetIlluminationEnabled(&renderer, 0);
    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_clipped == 1);
    assert(renderer.diagnostics.triangles_generated == 2);
    uint32_t shaded = renderer.diagnostics.fragments_shaded;
    assert(shaded > 0);
    assert(count_nonzero_frame_pixels(&buffers) > 0);
    assert(count_frame_color(&buffers, 0xEA) < shaded);
    uint32_t sampledSourceColors = 0;
    for (uint32_t i = 0; i < 4; i++) {
        sampledSourceColors +=
            count_frame_color(&buffers, texturePixels[i].c) > 0;
    }
    assert(sampledSourceColors >= 2);

    // Flat mode samples the first source UV once. Both primitives generated
    // by clipping carry exactly that one color, with no reciprocal-W rejects.
    mesh.shading_mode = MESH_SHADING_FLAT_PALETTE;
    assert(rendererRender(&renderer) == 0);
    shaded = renderer.diagnostics.fragments_shaded;
    assert(shaded > 0);
    assert(renderer.diagnostics.triangles_generated == 2);
    assert(renderer.diagnostics.fragments_reciprocal_w_rejected == 0);
    assert(count_nonzero_frame_pixels(&buffers) > 0);
    assert(
        count_frame_color(&buffers, 0xEA) ==
        count_nonzero_frame_pixels(&buffers));

    // Mesh mode remains authoritative while an object's established UV
    // override still chooses that object's constant face color.
    object.textCoord = objectTextureCoordinates;
    object.textCoord_count = 3;
    assert(objectUpdateTextureMappingValidity(&object) == 1);
    assert(rendererRender(&renderer) == 0);
    assert(
        count_frame_color(&buffers, 0xFC) ==
        count_nonzero_frame_pixels(&buffers));

    // Legacy RGBA8888 source textures enter the same flat path and are
    // quantized through the existing one-byte working-pixel conversion.
    assert(texture_init_format(
        &rgba8888Texture, (Vec2i){1, 1}, rgba8888Pixel,
        TEXTURE_FORMAT_RGBA8888) == 0);
    material.texture = &rgba8888Texture;
    assert(rendererRender(&renderer) == 0);
    uint8_t rgba8888Expected = pixelFromRGBA(255, 64, 128, 255).c;
    assert(
        count_frame_color(&buffers, rgba8888Expected) ==
        count_nonzero_frame_pixels(&buffers));
    material.texture = &texture;
    object.textCoord = 0;
    object.textCoord_count = 0;
    assert(objectUpdateTextureMappingValidity(&object) == 1);

    // A custom backend receives unity when illumination is disabled and the
    // computed overdrive factor when it is enabled; no null LUT is exposed.
    backend.drawPixel = capture_backend_pixel;
    reset_backend_capture();
    assert(rendererRender(&renderer) == 0);
    assert(captured_pixels > 0);
    assert(captured_minimum_illumination == 1.0f);
    assert(captured_maximum_illumination == 1.0f);

    rendererSetIlluminationEnabled(&renderer, 1);
    rendererSetLightIntensity(&renderer, 255);
    rendererSetAmbientLight(&renderer, 0);
    reset_backend_capture();
    assert(rendererRender(&renderer) == 0);
    assert(captured_pixels > 0);
    assert(captured_minimum_illumination > 2.0f);
    assert(captured_maximum_illumination > 2.0f);

    // A self-illuminated mesh bypasses scene light computation and shading
    // without changing its independent flat/textured mode. The backend sees
    // unity and the framebuffer retains the source palette color even while
    // the scene remains enabled and overdriven.
    mesh.illumination_policy = MESH_ILLUMINATION_SELF_ILLUMINATED;
    reset_backend_capture();
    assert(rendererRender(&renderer) == 0);
    assert(captured_pixels > 0);
    assert(captured_minimum_illumination == 1.0f);
    assert(captured_maximum_illumination == 1.0f);
    backend.drawPixel = 0;
    assert(rendererRender(&renderer) == 0);
    assert(
        count_frame_color(&buffers, 0xEA) ==
        count_nonzero_frame_pixels(&buffers));
    mesh.illumination_policy = MESH_ILLUMINATION_INHERIT_SCENE;

    // Unity ambient reproduces the native color even with zero directional
    // intensity. Two-times overdrive saturates every RGB channel to white.
    rendererSetIlluminationEnabled(&renderer, 1);
    rendererSetLightIntensity(&renderer, 0);
    rendererSetAmbientLight(&renderer, 127);
    assert(rendererRender(&renderer) == 0);
    assert(
        count_frame_color(&buffers, 0xEA) ==
        count_nonzero_frame_pixels(&buffers));

    rendererSetLightIntensity(&renderer, 255);
    rendererSetAmbientLight(&renderer, 0);
    assert(rendererRender(&renderer) == 0);
    assert(
        count_frame_color(&buffers, 0xFF) ==
        count_nonzero_frame_pixels(&buffers));

    // A flat source triangle crossing the actual near plane produces a fan
    // but retains one source color without perspective attributes.
    rendererSetIlluminationEnabled(&renderer, 0);
    positions[0] = (Vec3f){-0.75f, -0.75f, 0.5f};
    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_clipped == 1);
    assert(renderer.diagnostics.triangles_generated == 2);
    assert(renderer.diagnostics.fragments_reciprocal_w_rejected == 0);
    assert(
        count_frame_color(&buffers, 0xEA) ==
        count_nonzero_frame_pixels(&buffers));
}

static void test_flat_mode_is_shared_by_mesh_instances(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    Mesh mesh;
    Object leftObject;
    Object rightObject;
    Material material;
    Vec3f positions[] = {
        {-0.30f, -0.40f, -0.5f},
        { 0.30f, -0.40f, -0.5f},
        {-0.30f,  0.40f, -0.5f}
    };
    uint16_t indices[] = {0, 1, 2};
    uint16_t textureIndices[] = {0, 1, 2};
    Vec2f meshCoordinates[] = {
        {0.0f, 1.0f},
        {0.0f, 1.0f},
        {0.0f, 1.0f}
    };
    Vec2f overrideCoordinates[] = {
        {1.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 0.0f}
    };
    Pixel texturePixels[] = {
        {0xEA}, {0xD5},
        {0xF3}, {0xFC}
    };
    Texture texture;

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    assert(texture_init_format(
        &texture, (Vec2i){2, 2}, texturePixels,
        TEXTURE_FORMAT_RGBA2222) == 0);
    mesh = (Mesh) {
        .indexes_count = 3,
        .pos_indices = indices,
        .tex_indices = textureIndices,
        .positions = positions,
        .textCoord = meshCoordinates,
        .positions_count = 3,
        .texture_coordinates_count = 3,
        .texture_indexes_count = 3,
        .shading_mode = MESH_SHADING_FLAT_PALETTE
    };
    assert(meshUpdateGeometryValidity(&mesh) == 1);
    material = (Material){.texture = &texture};
    leftObject = (Object) {
        .mesh = &mesh,
        .transform = mat4Translate((Vec3f){-0.50f, 0.0f, 0.0f}),
        .material = &material
    };
    rightObject = (Object) {
        .mesh = &mesh,
        .transform = mat4Translate((Vec3f){0.50f, 0.0f, 0.0f}),
        .material = &material,
        .textCoord = overrideCoordinates,
        .textCoord_count = 3
    };
    assert(objectUpdateTextureMappingValidity(&leftObject) == 1);
    assert(objectUpdateTextureMappingValidity(&rightObject) == 1);
    assert(sceneAddRenderable(
        &scene, object_as_renderable(&leftObject)) == 0);
    assert(sceneAddRenderable(
        &scene, object_as_renderable(&rightObject)) == 0);

    rendererSetIlluminationEnabled(&renderer, 0);
    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.objects == 2);
    assert(renderer.diagnostics.triangles_submitted == 2);
    assert(renderer.diagnostics.triangles_rasterized == 2);
    assert(count_frame_color(&buffers, 0xEA) > 0);
    assert(count_frame_color(&buffers, 0xFC) > 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_wrapping_clock(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    fake_clock_ticks = UINT32_MAX - 5;

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.clear_ticks == 10);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

int main(void) {
    test_mesh_bounds_cache();
    test_empty_scene();
    test_front_triangle_and_reset();
    test_reversed_winding_is_rejected();
    test_overdraw_is_counted();
    test_z_rejection();
    test_remaining_frustum_planes_are_rejected();
    test_object_bounds_common_planes_are_rejected();
    test_object_bounds_eye_plane_is_rejected();
    test_object_bounds_boundaries_and_crossings_are_retained();
    test_object_bounds_transform_and_reference_outputs_match();
    test_object_bounds_nonuniform_negative_scale_boundary();
    test_object_bounds_invalid_and_disabled_fail_open();
    test_clip_boundaries_and_crossings_are_retained();
    test_actual_projection_uses_zero_as_near_clip_boundary();
    test_actual_projection_clips_near_crossings();
    test_actual_projection_clips_huge_lateral_triangle();
    test_actual_projection_rejects_nonfinite_input_safely();
    test_all_nonpositive_w_is_rejected_before_division();
    test_enabled_and_disabled_outputs_match();
    test_integer_screen_degeneracy();
    test_bbox_rejection_and_clamping();
    test_far_clip_rejection_prevents_out_of_range_depth();
    test_runtime_lighting_and_flat_palette_shading();
    test_flat_mode_is_shared_by_mesh_instances();
    test_wrapping_clock();
    puts("Pingo renderer diagnostics test passed");
    return 0;
}
