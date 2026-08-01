#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ZBUFFER32 // [ZBUFFER32 | ZBUFFER16 | ZBUFFER8]

#ifdef ZBUFFER32
typedef struct tag_PingoDepth {
    uint32_t d;
} PingoDepth;
#endif

#ifdef ZBUFFER16
typedef struct Depth {
    uint16_t d;
} Depth;
#endif

#ifdef ZBUFFER8
typedef struct Depth {
    uint8_t d;
} Depth;
#endif

void depth_write(PingoDepth * d, int idx, float value);
bool depth_check(PingoDepth * d, int idx, float value);

/*
 * Convert normalized depth without invoking an out-of-range float-to-integer
 * conversion. UINT32_MAX rounds to 2^32 as a float, so the exact 1.0f
 * endpoint must be handled before the multiply. Invalid values fail closed.
 */
static inline bool depth_quantize32(float value, uint32_t * encoded) {
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

static inline bool depth_try_write(
        PingoDepth * d, int idx, float value) {
    uint32_t candidate;
    if (!d || !depth_quantize32(value, &candidate)) {
        return false;
    }
    if (candidate < d[idx].d) {
        return false;
    }
    d[idx].d = candidate;
    return true;
}
