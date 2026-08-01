#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../video/pingo/render/depth.h"

static void assert_quantized(float value, uint32_t expected) {
    uint32_t encoded = 0xA5A5A5A5u;
    assert(depth_quantize32(value, &encoded));
    assert(encoded == expected);
}

static void assert_rejected(float value) {
    uint32_t encoded = 0xA5A5A5A5u;
    assert(!depth_quantize32(value, &encoded));
    assert(encoded == 0xA5A5A5A5u);

    PingoDepth fused = {.d = 0x12345678u};
    PingoDepth split = fused;
    assert(!depth_try_write(&fused, 0, value));
    assert(fused.d == 0x12345678u);
    assert(depth_check(&split, 0, value));
    depth_write(&split, 0, value);
    assert(split.d == 0x12345678u);
}

static void assert_fused_and_split_agree(
        uint32_t initial, float value) {
    PingoDepth fused = {.d = initial};
    PingoDepth split = {.d = initial};

    bool fused_written = depth_try_write(&fused, 0, value);
    bool split_rejected = depth_check(&split, 0, value);
    if (!split_rejected) {
        depth_write(&split, 0, value);
    }

    assert(fused_written == !split_rejected);
    assert(fused.d == split.d);
}

int main(void) {
    const float above_zero = nextafterf(0.0f, INFINITY);
    const float below_one = nextafterf(1.0f, 0.0f);

    assert_quantized(0.0f, 0u);
    assert_quantized(above_zero, 0u);
    assert_quantized(0.5f, 0x80000000u);
    assert_quantized(below_one, 0xFFFFFF00u);
    assert_quantized(1.0f, UINT32_MAX);

    assert_rejected(nextafterf(0.0f, -INFINITY));
    assert_rejected(nextafterf(1.0f, INFINITY));
    assert_rejected(NAN);
    assert_rejected(INFINITY);
    assert_rejected(-INFINITY);

    const float values[] = {
        0.0f,
        above_zero,
        0.25f,
        0.5f,
        0.75f,
        below_one,
        1.0f
    };
    const uint32_t initial_values[] = {
        0u,
        0x40000000u,
        0x80000000u,
        UINT32_MAX
    };
    for (uint32_t i = 0;
         i < sizeof(initial_values) / sizeof(initial_values[0]);
         i++) {
        for (uint32_t j = 0;
             j < sizeof(values) / sizeof(values[0]);
             j++) {
            assert_fused_and_split_agree(
                initial_values[i], values[j]);
        }
    }

    PingoDepth equal = {.d = 0x80000000u};
    assert(depth_try_write(&equal, 0, 0.5f));
    assert(equal.d == 0x80000000u);

    assert(!depth_quantize32(0.5f, NULL));
    assert(!depth_try_write(NULL, 0, 0.5f));
    assert(depth_check(NULL, 0, 0.5f));

    puts("Pingo depth quantization test passed");
    return 0;
}
