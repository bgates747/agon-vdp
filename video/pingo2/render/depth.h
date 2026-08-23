#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ZBUFFER32

typedef struct PingoDepth {
    uint32_t d;
} PingoDepth;

void depth_write(PingoDepth *depth, int index, float value);
bool depth_check(PingoDepth *depth, int index, float value);

static inline bool depth_quantize32(float value, uint32_t *encoded) {
    if (!encoded || !(value >= 0.0f && value <= 1.0f)) {
        return false;
    }
    if (value == 1.0f) {
        *encoded = UINT32_MAX;
        return true;
    }
    *encoded = (uint32_t)(value * 0x1p32f);
    return true;
}

static inline bool depth_try_write(PingoDepth *depth, int index, float value) {
    uint32_t candidate;
    if (!depth || !depth_quantize32(value, &candidate)) {
        return false;
    }
    if (candidate < depth[index].d) {
        return false;
    }
    depth[index].d = candidate;
    return true;
}
