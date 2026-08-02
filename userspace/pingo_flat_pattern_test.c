#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../video/pingo/render/clip.h"
#include "../video/pingo/render/depth.h"
#include "../video/pingo/render/material.h"
#include "../video/pingo/render/mesh.h"
#include "../video/pingo/render/object.h"
#include "../video/pingo/render/renderer.h"
#include "../video/pingo/render/backend.h"
#include "../video/pingo/render/scene.h"
#include "../video/pingo/render/texture.h"

enum {
    TEST_WIDTH = 8,
    TEST_HEIGHT = 8,
    TEST_PIXELS = TEST_WIDTH * TEST_HEIGHT,
    TEST_MATERIALS = 15,
    TEST_BANDS = 3,
    TEST_PATTERNS = 3
};

typedef struct tag_TestBuffers {
    Pixel frame[TEST_PIXELS];
    PingoDepth depth[TEST_PIXELS];
} TestBuffers;

typedef struct tag_TestObject {
    Mesh mesh;
    Object object;
    Material material;
    Texture selector;
    Vec3f positions[4];
    uint16_t position_indices[6];
    uint16_t texture_indices[6];
    Vec2f texture_coordinates[4];
    Pixel selector_pixels[16];
} TestObject;

static const uint8_t * callback_pattern;
static uint32_t callback_pixels;

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
    return ((TestBuffers *)backend->clientCustomData)->frame;
}

static PingoDepth * test_depth_buffer(
        Renderer * renderer, BackEnd * backend) {
    (void)renderer;
    return ((TestBuffers *)backend->clientCustomData)->depth;
}

static void capture_pattern_pixel(
        Texture * frame, Vec2i position, Pixel color, float illumination) {
    (void)frame;
    assert(callback_pattern);
    assert(illumination == 1.0f);
    uint32_t phase =
        ((uint32_t)position.y & 3u) * PINGO_FLAT_PATTERN_WIDTH +
        ((uint32_t)position.x & 3u);
    assert(color.c == callback_pattern[phase]);
    callback_pixels++;
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
}

static void initialize_pattern_object(
        Scene * scene, TestObject * fixture, uint32_t material_id) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->positions[0] = (Vec3f){-1.50f, -0.75f, -0.5f};
    fixture->positions[1] = (Vec3f){ 0.75f, -0.75f, -0.5f};
    fixture->positions[2] = (Vec3f){-0.75f,  0.75f, -0.5f};
    fixture->position_indices[0] = 0;
    fixture->position_indices[1] = 1;
    fixture->position_indices[2] = 2;
    fixture->texture_indices[0] = 0;
    fixture->texture_indices[1] = 1;
    fixture->texture_indices[2] = 2;

    /*
     * The 4x4 selector intentionally has one unused texel: IDs 0..14 are
     * valid, while texel 15 exercises per-face bounds rejection.
     */
    uint32_t selector_x = material_id & 3u;
    uint32_t selector_y = material_id >> 2;
    Vec2f coordinate = {
        (float)selector_x / 3.0f,
        1.0f - (float)selector_y / 3.0f
    };
    fixture->texture_coordinates[0] = coordinate;
    fixture->texture_coordinates[1] = coordinate;
    fixture->texture_coordinates[2] = coordinate;
    for (uint32_t i = 0; i < 16; i++) {
        /* Contents are irrelevant: the texel position is the material ID. */
        fixture->selector_pixels[i] = (Pixel){(uint8_t)(0xC0u | i)};
    }

    assert(texture_init_format(
        &fixture->selector, (Vec2i){4, 4}, fixture->selector_pixels,
        TEXTURE_FORMAT_RGBA2222) == 0);
    fixture->mesh = (Mesh) {
        .indexes_count = 3,
        .pos_indices = fixture->position_indices,
        .tex_indices = fixture->texture_indices,
        .positions = fixture->positions,
        .textCoord = fixture->texture_coordinates,
        .positions_count = 3,
        .texture_coordinates_count = 3,
        .texture_indexes_count = 3,
        .shading_mode = MESH_SHADING_FLAT_PATTERN,
        .illumination_policy = MESH_ILLUMINATION_INHERIT_SCENE
    };
    assert(meshUpdateGeometryValidity(&fixture->mesh) == 1);
    fixture->material = (Material){.texture = &fixture->selector};
    fixture->object = (Object) {
        .mesh = &fixture->mesh,
        .transform = mat4Identity(),
        .material = &fixture->material
    };
    assert(objectUpdateTextureMappingValidity(&fixture->object) == 1);
    assert(sceneAddRenderable(
        scene, object_as_renderable(&fixture->object)) == 0);
}

static PingoFlatPatternLibrary make_library(
        uint8_t * patterns, uint8_t * lookup) {
    for (uint32_t pattern = 0; pattern < TEST_PATTERNS; pattern++) {
        for (uint32_t phase = 0;
             phase < PINGO_FLAT_PATTERN_PIXELS;
             phase++) {
            patterns[pattern * PINGO_FLAT_PATTERN_PIXELS + phase] =
                (uint8_t)(0xC0u | (pattern * 16u + phase));
        }
    }
    memset(lookup, 0, TEST_MATERIALS * TEST_BANDS);
    lookup[14 * TEST_BANDS + 0] = 0;
    lookup[14 * TEST_BANDS + 1] = 1;
    lookup[14 * TEST_BANDS + 2] = 2;
    return (PingoFlatPatternLibrary) {
        .patterns = patterns,
        .patterns_size = TEST_PATTERNS * PINGO_FLAT_PATTERN_PIXELS,
        .lookup = lookup,
        .lookup_size = TEST_MATERIALS * TEST_BANDS,
        .pattern_count = TEST_PATTERNS,
        .material_count = TEST_MATERIALS,
        .illumination_band_count = TEST_BANDS
    };
}

static uint32_t assert_pattern_frame(
        const TestBuffers * buffers, const uint8_t * pattern) {
    uint32_t drawn = 0;
    uint16_t phases = 0;
    for (uint32_t y = 0; y < TEST_HEIGHT; y++) {
        for (uint32_t x = 0; x < TEST_WIDTH; x++) {
            uint8_t actual = buffers->frame[x + y * TEST_WIDTH].c;
            if (actual == 0) {
                continue;
            }
            uint32_t phase =
                (y & 3u) * PINGO_FLAT_PATTERN_WIDTH + (x & 3u);
            assert(actual == pattern[phase]);
            phases |= (uint16_t)(1u << phase);
            drawn++;
        }
    }
    assert(drawn > 0);
    assert(phases == UINT16_MAX);
    return drawn;
}

static void assert_empty_buffers(const TestBuffers * buffers) {
    for (uint32_t i = 0; i < TEST_PIXELS; i++) {
        assert(buffers->frame[i].c == 0);
        assert(buffers->depth[i].d == 0);
    }
}

static void test_library_validation(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    uint8_t patterns[TEST_PATTERNS * PINGO_FLAT_PATTERN_PIXELS];
    uint8_t lookup[TEST_MATERIALS * TEST_BANDS];
    PingoFlatPatternLibrary library = make_library(patterns, lookup);

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    assert(renderer.flatPatternLibraryValid == 0);
    assert(rendererSetFlatPatternLibrary(NULL, &library) != 0);

    PingoFlatPatternLibrary invalid = library;
    invalid.illumination_band_count = 1;
    assert(rendererSetFlatPatternLibrary(&renderer, &invalid) != 0);
    invalid = library;
    invalid.patterns_size--;
    assert(rendererSetFlatPatternLibrary(&renderer, &invalid) != 0);
    invalid = library;
    invalid.lookup_size--;
    assert(rendererSetFlatPatternLibrary(&renderer, &invalid) != 0);
    invalid = library;
    invalid.pattern_count = PINGO_FLAT_PATTERN_MAX_PATTERNS + 1;
    assert(rendererSetFlatPatternLibrary(&renderer, &invalid) != 0);

    uint8_t saved_pattern = patterns[0];
    patterns[0] &= 0x3Fu;
    assert(rendererSetFlatPatternLibrary(&renderer, &library) != 0);
    patterns[0] = saved_pattern;
    uint8_t saved_lookup = lookup[0];
    lookup[0] = TEST_PATTERNS;
    assert(rendererSetFlatPatternLibrary(&renderer, &library) != 0);
    lookup[0] = saved_lookup;

    assert(rendererSetFlatPatternLibrary(&renderer, &library) == 0);
    assert(renderer.flatPatternLibraryValid == 1);
    assert(renderer.flatPatternLibrary.patterns == patterns);

    /* Invalid replacement is transactional. */
    invalid = library;
    invalid.patterns = NULL;
    assert(rendererSetFlatPatternLibrary(&renderer, &invalid) != 0);
    assert(renderer.flatPatternLibraryValid == 1);
    assert(renderer.flatPatternLibrary.patterns == patterns);

    assert(rendererSetFlatPatternLibrary(&renderer, NULL) == 0);
    assert(renderer.flatPatternLibraryValid == 0);
}

static void test_pattern_rendering_and_failure_paths(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    TestObject fixture;
    uint8_t patterns[TEST_PATTERNS * PINGO_FLAT_PATTERN_PIXELS];
    uint8_t lookup[TEST_MATERIALS * TEST_BANDS];
    PingoFlatPatternLibrary library = make_library(patterns, lookup);

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    initialize_pattern_object(&scene, &fixture, 14);

    /* This source triangle clips to a four-vertex polygon and two fans. */
    PingoClipVertex clip_input[3] = {
        {
            .position = (Vec4f){-1.50f, -0.75f, -0.5f, 1.0f},
            .texture = fixture.texture_coordinates[0]
        },
        {
            .position = (Vec4f){ 0.75f, -0.75f, -0.5f, 1.0f},
            .texture = fixture.texture_coordinates[1]
        },
        {
            .position = (Vec4f){-0.75f,  0.75f, -0.5f, 1.0f},
            .texture = fixture.texture_coordinates[2]
        }
    };
    PingoClipVertex clipped[PINGO_CLIP_MAX_VERTICES];
    assert(pingoClipTriangle(
        clip_input, PINGO_CLIP_OUTSIDE_LEFT, clipped) == 4);

    /* Missing resources reject the pattern object before any depth write. */
    assert(rendererRender(&renderer) == 0);
    assert_empty_buffers(&buffers);
    assert(rendererSetFlatPatternLibrary(&renderer, &library) == 0);

    /* Inactive objects leave both color and depth untouched. */
    fixture.object.inactive = 1;
    assert(rendererRender(&renderer) == 0);
    assert_empty_buffers(&buffers);
    fixture.object.inactive = 0;

    /* Disabled illumination selects the endpoint/unity pattern. */
    rendererSetIlluminationEnabled(&renderer, 0);
    assert(rendererRender(&renderer) == 0);
    uint32_t drawn = assert_pattern_frame(
        &buffers, patterns + 2 * PINGO_FLAT_PATTERN_PIXELS);

    /* 64/127 rounds to the middle of three endpoint-inclusive bands. */
    rendererSetIlluminationEnabled(&renderer, 1);
    rendererSetLightIntensity(&renderer, 0);
    rendererSetAmbientLight(&renderer, 64);
    assert(rendererRender(&renderer) == 0);
    assert(assert_pattern_frame(
        &buffers, patterns + PINGO_FLAT_PATTERN_PIXELS) == drawn);

    rendererSetAmbientLight(&renderer, 0);
    assert(rendererRender(&renderer) == 0);
    assert(assert_pattern_frame(&buffers, patterns) == drawn);

    /* Self illumination deliberately retains the authored unity dither. */
    fixture.mesh.illumination_policy = MESH_ILLUMINATION_SELF_ILLUMINATED;
    assert(rendererRender(&renderer) == 0);
    assert(assert_pattern_frame(
        &buffers, patterns + 2 * PINGO_FLAT_PATTERN_PIXELS) == drawn);

    /* Backends receive the already selected native byte at unity. */
    callback_pattern = patterns + 2 * PINGO_FLAT_PATTERN_PIXELS;
    callback_pixels = 0;
    backend.drawPixel = capture_pattern_pixel;
    assert(rendererRender(&renderer) == 0);
    /* Shared fan edges can shade an equal-depth pixel twice. */
    assert(callback_pixels >= drawn);
    backend.drawPixel = 0;

    /* A later in-place lookup mutation is checked again per source face. */
    lookup[14 * TEST_BANDS + 2] = TEST_PATTERNS;
    assert(rendererRender(&renderer) == 0);
    assert_empty_buffers(&buffers);
    lookup[14 * TEST_BANDS + 2] = 2;

    /* Sparse 4x4 selector texel 15 is outside the 15-material vocabulary. */
    fixture.texture_coordinates[0] = (Vec2f){1.0f, 0.0f};
    fixture.texture_coordinates[1] = fixture.texture_coordinates[0];
    fixture.texture_coordinates[2] = fixture.texture_coordinates[0];
    assert(rendererRender(&renderer) == 0);
    assert_empty_buffers(&buffers);
}

static void test_adjacent_triangles_keep_global_phase(void) {
    Renderer renderer;
    Scene scene;
    BackEnd backend;
    TestBuffers buffers;
    TestObject fixture;
    uint8_t patterns[TEST_PATTERNS * PINGO_FLAT_PATTERN_PIXELS];
    uint8_t lookup[TEST_MATERIALS * TEST_BANDS];
    PingoFlatPatternLibrary library = make_library(patterns, lookup);

    initialize_renderer(&renderer, &scene, &backend, &buffers);
    initialize_pattern_object(&scene, &fixture, 14);

    fixture.positions[0] = (Vec3f){-0.75f, -0.75f, -0.5f};
    fixture.positions[1] = (Vec3f){ 0.75f, -0.75f, -0.5f};
    fixture.positions[2] = (Vec3f){-0.75f,  0.75f, -0.5f};
    fixture.positions[3] = (Vec3f){ 0.75f,  0.75f, -0.5f};
    const uint16_t position_indices[6] = {0, 1, 2, 2, 1, 3};
    memcpy(
        fixture.position_indices,
        position_indices,
        sizeof(position_indices));
    for (uint32_t i = 0; i < 6; i++) {
        fixture.texture_indices[i] = position_indices[i];
    }
    fixture.texture_coordinates[3] = fixture.texture_coordinates[0];
    fixture.mesh.indexes_count = 6;
    fixture.mesh.positions_count = 4;
    fixture.mesh.texture_coordinates_count = 4;
    fixture.mesh.texture_indexes_count = 6;
    assert(meshUpdateGeometryValidity(&fixture.mesh) == 1);
    assert(objectUpdateTextureMappingValidity(&fixture.object) == 1);

    assert(rendererSetFlatPatternLibrary(&renderer, &library) == 0);
    rendererSetIlluminationEnabled(&renderer, 0);
    assert(rendererRender(&renderer) == 0);

    /*
     * The two independently selected faces fill one square. Every drawn pixel
     * must use absolute screen x/y, including both sides of their diagonal.
     */
    assert_pattern_frame(
        &buffers, patterns + 2 * PINGO_FLAT_PATTERN_PIXELS);
}

int main(void) {
    test_library_validation();
    test_pattern_rendering_and_failure_paths();
    test_adjacent_triangles_keep_global_phase();
    puts("Pingo flat-pattern renderer test passed");
    return 0;
}
