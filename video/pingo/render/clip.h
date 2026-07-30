#pragma once

#include <math.h>
#include <stdint.h>

#include "../math/vec2.h"
#include "../math/vec4.h"

enum {
    PINGO_CLIP_OUTSIDE_EYE = 1 << 0,
    PINGO_CLIP_OUTSIDE_NEAR = 1 << 1,
    PINGO_CLIP_OUTSIDE_FAR = 1 << 2,
    PINGO_CLIP_OUTSIDE_LEFT = 1 << 3,
    PINGO_CLIP_OUTSIDE_RIGHT = 1 << 4,
    PINGO_CLIP_OUTSIDE_BOTTOM = 1 << 5,
    PINGO_CLIP_OUTSIDE_TOP = 1 << 6,
    PINGO_CLIP_OUTSIDE_ALL = (1 << 7) - 1,
    PINGO_CLIP_PLANES =
        PINGO_CLIP_OUTSIDE_NEAR |
        PINGO_CLIP_OUTSIDE_FAR |
        PINGO_CLIP_OUTSIDE_LEFT |
        PINGO_CLIP_OUTSIDE_RIGHT |
        PINGO_CLIP_OUTSIDE_BOTTOM |
        PINGO_CLIP_OUTSIDE_TOP,
    PINGO_CLIP_MAX_VERTICES = 9
};

typedef struct tag_PingoClipVertex {
    Vec4f position;
    Vec2f texture;
} PingoClipVertex;

static inline int pingoClipVertexFinite(PingoClipVertex vertex) {
    return isfinite(vertex.position.x) &&
        isfinite(vertex.position.y) &&
        isfinite(vertex.position.z) &&
        isfinite(vertex.position.w) &&
        isfinite(vertex.texture.x) &&
        isfinite(vertex.texture.y);
}

static inline float pingoClipDistance(
        PingoClipVertex vertex, uint8_t plane) {
    switch (plane) {
        case PINGO_CLIP_OUTSIDE_NEAR:
            return -vertex.position.z;
        case PINGO_CLIP_OUTSIDE_FAR:
            return vertex.position.z + vertex.position.w;
        case PINGO_CLIP_OUTSIDE_LEFT:
            return vertex.position.x + vertex.position.w;
        case PINGO_CLIP_OUTSIDE_RIGHT:
            return vertex.position.w - vertex.position.x;
        case PINGO_CLIP_OUTSIDE_BOTTOM:
            return vertex.position.y + vertex.position.w;
        case PINGO_CLIP_OUTSIDE_TOP:
            return vertex.position.w - vertex.position.y;
        default:
            return -1.0f;
    }
}

static inline PingoClipVertex pingoClipInterpolate(
        PingoClipVertex from,
        PingoClipVertex to,
        float amount,
        uint8_t plane) {
    PingoClipVertex result = {
        .position = {
            from.position.x +
                (to.position.x - from.position.x) * amount,
            from.position.y +
                (to.position.y - from.position.y) * amount,
            from.position.z +
                (to.position.z - from.position.z) * amount,
            from.position.w +
                (to.position.w - from.position.w) * amount
        },
        .texture = {
            from.texture.x +
                (to.texture.x - from.texture.x) * amount,
            from.texture.y +
                (to.texture.y - from.texture.y) * amount
        }
    };

    /*
     * Put the generated point exactly on its plane. This prevents a rounding
     * wobble from reclassifying it during a later common-outcode check.
     */
    switch (plane) {
        case PINGO_CLIP_OUTSIDE_NEAR:
            result.position.z = 0.0f;
            break;
        case PINGO_CLIP_OUTSIDE_FAR:
            result.position.z = -result.position.w;
            break;
        case PINGO_CLIP_OUTSIDE_LEFT:
            result.position.x = -result.position.w;
            break;
        case PINGO_CLIP_OUTSIDE_RIGHT:
            result.position.x = result.position.w;
            break;
        case PINGO_CLIP_OUTSIDE_BOTTOM:
            result.position.y = -result.position.w;
            break;
        case PINGO_CLIP_OUTSIDE_TOP:
            result.position.y = result.position.w;
            break;
    }
    return result;
}

static inline int pingoClipSamePosition(
        PingoClipVertex a, PingoClipVertex b) {
    return a.position.x == b.position.x &&
        a.position.y == b.position.y &&
        a.position.z == b.position.z &&
        a.position.w == b.position.w;
}

static inline int pingoClipAppendUnique(
        PingoClipVertex * output,
        uint8_t * output_count,
        PingoClipVertex vertex) {
    if (*output_count > 0 &&
        pingoClipSamePosition(output[*output_count - 1], vertex)) {
        return 1;
    }
    if (*output_count >= PINGO_CLIP_MAX_VERTICES) {
        return 0;
    }
    output[(*output_count)++] = vertex;
    return 1;
}

/*
 * Clip a triangle against only the planes selected by plane_mask. These
 * planes describe Pingo's production projection volume:
 *
 *     -W <= X,Y <= W
 *     -W <= Z <= 0
 *
 * With that matrix, accepting the near half-space Z <= 0 also guarantees
 * W >= near > 0, so a separate arbitrary eye-plane epsilon is neither
 * necessary nor desirable. A custom projection with a different Z/W
 * relationship would need its own explicit eye-plane contract.
 *
 * A triangle clipped by six convex planes can have at most nine output
 * vertices. Returning zero is a fail-closed rejection for malformed or
 * overflowing input.
 */
static inline uint8_t pingoClipTriangle(
        const PingoClipVertex input[3],
        uint8_t plane_mask,
        PingoClipVertex output[PINGO_CLIP_MAX_VERTICES]) {
    PingoClipVertex scratch[PINGO_CLIP_MAX_VERTICES];
    uint8_t count = 3;
    for (uint8_t i = 0; i < count; i++) {
        if (!pingoClipVertexFinite(input[i])) {
            return 0;
        }
        output[i] = input[i];
    }

    const uint8_t planes[] = {
        PINGO_CLIP_OUTSIDE_NEAR,
        PINGO_CLIP_OUTSIDE_FAR,
        PINGO_CLIP_OUTSIDE_LEFT,
        PINGO_CLIP_OUTSIDE_RIGHT,
        PINGO_CLIP_OUTSIDE_BOTTOM,
        PINGO_CLIP_OUTSIDE_TOP
    };
    for (uint8_t plane_index = 0;
         plane_index < sizeof(planes) / sizeof(planes[0]);
         plane_index++) {
        uint8_t plane = planes[plane_index];
        if (!(plane_mask & plane)) {
            continue;
        }

        uint8_t next_count = 0;
        PingoClipVertex previous = output[count - 1];
        float previous_distance =
            pingoClipDistance(previous, plane);
        int previous_inside = previous_distance >= 0.0f;

        for (uint8_t i = 0; i < count; i++) {
            PingoClipVertex current = output[i];
            float current_distance =
                pingoClipDistance(current, plane);
            int current_inside = current_distance >= 0.0f;

            if (previous_inside != current_inside) {
                float denominator =
                    previous_distance - current_distance;
                if (!isfinite(denominator) ||
                    denominator == 0.0f) {
                    return 0;
                }
                float amount = previous_distance / denominator;
                PingoClipVertex intersection =
                    pingoClipInterpolate(
                        previous, current, amount, plane);
                if (!pingoClipVertexFinite(intersection) ||
                    !pingoClipAppendUnique(
                        scratch, &next_count, intersection)) {
                    return 0;
                }
            }
            if (current_inside &&
                !pingoClipAppendUnique(
                    scratch, &next_count, current)) {
                return 0;
            }

            previous = current;
            previous_distance = current_distance;
            previous_inside = current_inside;
        }

        if (next_count > 1 &&
            pingoClipSamePosition(
                scratch[0], scratch[next_count - 1])) {
            next_count--;
        }
        if (next_count < 3) {
            return 0;
        }
        count = next_count;
        for (uint8_t i = 0; i < count; i++) {
            output[i] = scratch[i];
        }
    }
    return count;
}
