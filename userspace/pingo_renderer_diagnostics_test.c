#include <assert.h>
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
    *object = (Object) {
        .mesh = mesh,
        .transform = mat4Identity(),
        .material = 0,
        .textCoord = 0
    };
    assert(sceneAddRenderable(scene, object_as_renderable(object)) == 0);
}

static void assert_diagnostic_invariants(
        const RendererDiagnostics * diagnostics) {
    assert(
        diagnostics->triangles_submitted ==
        diagnostics->triangles_z_rejected +
        diagnostics->triangles_frustum_rejected +
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
    renderer.camera_projection =
        mat4Perspective(1.0f, 2500.0f, 4.0f / 3.0f, 0.6f);
    add_mesh_object(
        &scene, &object, &mesh, positions, indices, 3);

    assert(rendererRender(&renderer) == 0);
    assert(renderer.diagnostics.triangles_z_rejected == 1);
    assert(renderer.diagnostics.triangles_frustum_rejected == 0);
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
    assert(disabled.diagnostics.triangles_frustum_rejected == 0);
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
    assert(renderer.diagnostics.triangles_rasterized == 1);
    assert(renderer.diagnostics.triangles_bbox_clamped == 1);
    assert(renderer.diagnostics.fragments_bbox > 0);
    assert_diagnostic_invariants(&renderer.diagnostics);
}

static void test_depth_range_rejection(void) {
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
    assert(renderer.diagnostics.fragments_covered > 0);
    assert(
        renderer.diagnostics.fragments_depth_range_rejected ==
        renderer.diagnostics.fragments_covered);
    assert(renderer.diagnostics.fragments_shaded == 0);
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
    test_empty_scene();
    test_front_triangle_and_reset();
    test_reversed_winding_is_rejected();
    test_overdraw_is_counted();
    test_z_rejection();
    test_remaining_frustum_planes_are_rejected();
    test_clip_boundaries_and_crossings_are_retained();
    test_actual_projection_uses_zero_as_near_clip_boundary();
    test_all_nonpositive_w_is_rejected_before_division();
    test_enabled_and_disabled_outputs_match();
    test_integer_screen_degeneracy();
    test_bbox_rejection_and_clamping();
    test_depth_range_rejection();
    test_wrapping_clock();
    puts("Pingo renderer diagnostics test passed");
    return 0;
}
