#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../video/pingo/render/clip.h"

static PingoClipVertex vertex(
        float x, float y, float z, float w,
        float u, float v) {
    return (PingoClipVertex) {
        .position = {x, y, z, w},
        .texture = {u, v}
    };
}

static void assert_inside(PingoClipVertex point) {
    assert(pingoClipDistance(
        point, PINGO_CLIP_OUTSIDE_NEAR) >= 0.0f);
    assert(pingoClipDistance(
        point, PINGO_CLIP_OUTSIDE_FAR) >= 0.0f);
    assert(pingoClipDistance(
        point, PINGO_CLIP_OUTSIDE_LEFT) >= 0.0f);
    assert(pingoClipDistance(
        point, PINGO_CLIP_OUTSIDE_RIGHT) >= 0.0f);
    assert(pingoClipDistance(
        point, PINGO_CLIP_OUTSIDE_BOTTOM) >= 0.0f);
    assert(pingoClipDistance(
        point, PINGO_CLIP_OUTSIDE_TOP) >= 0.0f);
}

static void test_unclipped_triangle_is_exact(void) {
    PingoClipVertex input[3] = {
        vertex(-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f),
        vertex( 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f)
    };
    PingoClipVertex output[PINGO_CLIP_MAX_VERTICES];
    uint8_t count =
        pingoClipTriangle(input, 0, output);

    assert(count == 3);
    for (uint8_t i = 0; i < count; i++) {
        assert(pingoClipSamePosition(input[i], output[i]));
        assert(input[i].texture.x == output[i].texture.x);
        assert(input[i].texture.y == output[i].texture.y);
    }
}

static void test_near_plane_one_inside(void) {
    PingoClipVertex input[3] = {
        vertex(-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f),
        vertex( 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f)
    };
    PingoClipVertex output[PINGO_CLIP_MAX_VERTICES];
    uint8_t count = pingoClipTriangle(
        input, PINGO_CLIP_OUTSIDE_NEAR, output);

    assert(count == 3);
    for (uint8_t i = 0; i < count; i++) {
        assert(output[i].position.z <= 0.0f);
    }
    assert(output[0].position.z == 0.0f);
    assert(output[0].texture.x == 0.0f);
    assert(output[0].texture.y == 0.5f);
    assert(output[2].position.z == 0.0f);
    assert(output[2].texture.x == 0.5f);
    assert(output[2].texture.y == 0.0f);
}

static void test_near_plane_two_inside(void) {
    PingoClipVertex input[3] = {
        vertex(-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f),
        vertex( 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f)
    };
    PingoClipVertex output[PINGO_CLIP_MAX_VERTICES];
    uint8_t count = pingoClipTriangle(
        input, PINGO_CLIP_OUTSIDE_NEAR, output);

    assert(count == 4);
    for (uint8_t i = 0; i < count; i++) {
        assert(output[i].position.z <= 0.0f);
    }
}

static void test_exact_and_adjacent_near_boundaries(void) {
    const float inside =
        nextafterf(0.0f, -INFINITY);
    const float outside =
        nextafterf(0.0f, INFINITY);
    PingoClipVertex input[3] = {
        vertex(-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f),
        vertex( 0.5f, -0.5f, inside, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f, outside, 1.0f, 0.0f, 1.0f)
    };
    PingoClipVertex output[PINGO_CLIP_MAX_VERTICES];
    uint8_t count = pingoClipTriangle(
        input, PINGO_CLIP_OUTSIDE_NEAR, output);

    assert(count >= 3);
    for (uint8_t i = 0; i < count; i++) {
        assert(output[i].position.z <= 0.0f);
        if (i > 0) {
            assert(!pingoClipSamePosition(
                output[i - 1], output[i]));
        }
    }
}

static void test_all_planes_bound_output(void) {
    PingoClipVertex input[3] = {
        vertex(-1000000.0f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f),
        vertex( 1000000.0f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f),
        vertex(0.0f, 1000000.0f, -2.0f, 1.0f, 0.5f, 1.0f)
    };
    PingoClipVertex output[PINGO_CLIP_MAX_VERTICES];
    uint8_t count = pingoClipTriangle(
        input, PINGO_CLIP_PLANES, output);

    assert(count >= 3);
    assert(count <= PINGO_CLIP_MAX_VERTICES);
    for (uint8_t i = 0; i < count; i++) {
        assert_inside(output[i]);
    }
}

static void test_rejections(void) {
    PingoClipVertex outside[3] = {
        vertex(-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f),
        vertex( 0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f, 0.5f, 1.0f, 0.0f, 1.0f)
    };
    PingoClipVertex output[PINGO_CLIP_MAX_VERTICES];
    assert(pingoClipTriangle(
        outside, PINGO_CLIP_OUTSIDE_NEAR, output) == 0);

    outside[0].position.x = NAN;
    assert(pingoClipTriangle(
        outside, PINGO_CLIP_PLANES, output) == 0);
    outside[0].position.x = 0.0f;
    outside[0].texture.x = INFINITY;
    assert(pingoClipTriangle(
        outside, PINGO_CLIP_PLANES, output) == 0);
}

int main(void) {
    test_unclipped_triangle_is_exact();
    test_near_plane_one_inside();
    test_near_plane_two_inside();
    test_exact_and_adjacent_near_boundaries();
    test_all_planes_bound_output();
    test_rejections();
    puts("Pingo homogeneous clip test passed");
    return 0;
}
