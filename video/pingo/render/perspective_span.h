#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define PINGO_PERSPECTIVE_BLOCK_LENGTH 8u
#define PINGO_FIXED16_16_SCALE 65536.0f
#define PINGO_FIXED16_16_POSITIVE_MODIFIER ((int32_t)0x00008000)
#define PINGO_FIXED16_16_NEGATIVE_MODIFIER ((int32_t)0x00007fff)

typedef struct tag_PingoPerspectiveAttributes {
    float reciprocalW;
    float uOverW;
    float vOverW;
} PingoPerspectiveAttributes;

typedef struct tag_PingoPerspectiveBoundary {
    PingoPerspectiveAttributes attributes;
    float u;
    float v;
    bool valid;
} PingoPerspectiveBoundary;

typedef struct tag_PingoPerspectiveFixedMapping {
    float uScale;
    float vScale;
    int32_t uModifier;
    int32_t vModifier;
} PingoPerspectiveFixedMapping;

typedef struct tag_PingoPerspectiveSpanBlock {
    uint32_t length;
    PingoPerspectiveBoundary end;
    float u;
    float v;
    float uStep;
    float vStep;
    int32_t fixedU;
    int32_t fixedV;
    int32_t fixedUStep;
    int32_t fixedVStep;
    bool fixedValid;
} PingoPerspectiveSpanBlock;

static inline PingoPerspectiveAttributes
pingoPerspectiveAttributesAdvance(
        PingoPerspectiveAttributes value,
        PingoPerspectiveAttributes step,
        uint32_t distance)
{
    const float scale = (float)distance;
    value.reciprocalW += step.reciprocalW * scale;
    value.uOverW += step.uOverW * scale;
    value.vOverW += step.vOverW * scale;
    return value;
}

static inline PingoPerspectiveBoundary
pingoPerspectiveBoundaryRecover(PingoPerspectiveAttributes attributes)
{
    PingoPerspectiveBoundary boundary = {
        .attributes = attributes,
        .u = 0.0f,
        .v = 0.0f,
        .valid = false
    };
    if (attributes.reciprocalW == 0.0f) {
        return boundary;
    }

    const float w = 1.0f / attributes.reciprocalW;
    boundary.u = attributes.uOverW * w;
    boundary.v = attributes.vOverW * w;
    boundary.valid = true;
    return boundary;
}

/*
 * Chris Hecker's directional modifier chooses the side of a half-texel tie
 * from the sign of the perspective-correct coordinate gradient. q is 1/W
 * and c is either U/W or texture-memory-row/W. The numerator below has the
 * same sign as dc/dX without requiring another reciprocal.
 */
static inline int32_t pingoPerspectiveRoundingModifier(
        float q,
        float c,
        float dqdx,
        float dcdx,
        float dqdy,
        float dcdy)
{
    const float xIndicator = dcdx * q - c * dqdx;
    if (xIndicator > 0.0f) {
        return PINGO_FIXED16_16_POSITIVE_MODIFIER;
    }
    if (xIndicator < 0.0f) {
        return PINGO_FIXED16_16_NEGATIVE_MODIFIER;
    }

    const float yIndicator = dcdy * q - c * dqdy;
    return yIndicator >= 0.0f
        ? PINGO_FIXED16_16_POSITIVE_MODIFIER
        : PINGO_FIXED16_16_NEGATIVE_MODIFIER;
}

/*
 * Select the modifier after negating the source coordinate, as required by
 * Pingo's texture-memory row (1-V). Preserve the zero-gradient rule: a truly
 * stationary coordinate still receives the positive modifier.
 */
static inline int32_t pingoPerspectiveReversedRoundingModifier(
        float q,
        float c,
        float dqdx,
        float dcdx,
        float dqdy,
        float dcdy)
{
    const float xIndicator = dcdx * q - c * dqdx;
    if (xIndicator > 0.0f) {
        return PINGO_FIXED16_16_NEGATIVE_MODIFIER;
    }
    if (xIndicator < 0.0f) {
        return PINGO_FIXED16_16_POSITIVE_MODIFIER;
    }

    const float yIndicator = dcdy * q - c * dqdy;
    return yIndicator > 0.0f
        ? PINGO_FIXED16_16_NEGATIVE_MODIFIER
        : PINGO_FIXED16_16_POSITIVE_MODIFIER;
}

/*
 * Convert a finite, representable float to signed 16.16 by truncating toward
 * zero, matching Hecker's portable-C mapper without invoking float-to-int
 * overflow undefined behaviour.
 */
static inline bool pingoFixed16_16FromFloat(
        float value, int32_t * fixed)
{
    const float scaled = value * PINGO_FIXED16_16_SCALE;
    if (!isfinite(scaled) ||
        scaled >= 2147483648.0f ||
        scaled < -2147483648.0f) {
        *fixed = 0;
        return false;
    }

    *fixed = (int32_t)scaled;
    return true;
}

/*
 * Quantize one texel-space block component. The start receives the
 * direction-dependent half-texel modifier; the delta is independently
 * converted before division, as in Hecker's portable-C implementation.
 */
static inline bool pingoPerspectiveFixedComponentPrepare(
        float start,
        float end,
        int32_t modifier,
        uint32_t endpointDistance,
        uint32_t length,
        int32_t * value,
        int32_t * step)
{
    int32_t startFixed = 0;
    int32_t deltaNumerator = 0;
    *value = 0;
    *step = 0;

    if (length == 0u) {
        return false;
    }
    if (!pingoFixed16_16FromFloat(start, &startFixed)) {
        return false;
    }

    const int64_t modifiedStart =
        (int64_t)startFixed + (int64_t)modifier;
    if (modifiedStart < INT32_MIN || modifiedStart > INT32_MAX) {
        return false;
    }

    int32_t delta = 0;
    if (endpointDistance != 0u) {
        if (!pingoFixed16_16FromFloat(
                end - start, &deltaNumerator)) {
            return false;
        }
        delta = deltaNumerator / (int32_t)endpointDistance;
    }

    /*
     * The renderer does not perform the unused add after a block's final
     * sample. Proving the last sampled accumulator fits therefore proves
     * every executed signed addition is defined.
     */
    const int64_t finalValue =
        modifiedStart + (int64_t)delta * (int64_t)(length - 1u);
    if (finalValue < INT32_MIN || finalValue > INT32_MAX) {
        return false;
    }

    *value = (int32_t)modifiedStart;
    *step = delta;
    return true;
}

static inline bool pingoPerspectiveFixedBlockPrepare(
        const PingoPerspectiveBoundary * start,
        const PingoPerspectiveBoundary * end,
        const PingoPerspectiveFixedMapping * mapping,
        uint32_t endpointDistance,
        uint32_t length,
        PingoPerspectiveSpanBlock * block)
{
    if (mapping->uScale < 0.0f ||
        mapping->vScale < 0.0f ||
        mapping->uScale > 32767.0f ||
        mapping->vScale > 32767.0f) {
        return false;
    }

    const float startU = start->u * mapping->uScale;
    const float endU = end->u * mapping->uScale;
    /*
     * UV V grows upward while texture memory starts at the top row. Keep the
     * fixed accumulator in directly sampled memory-row coordinates.
     */
    const float startV = (1.0f - start->v) * mapping->vScale;
    const float endV = (1.0f - end->v) * mapping->vScale;

    if (!pingoPerspectiveFixedComponentPrepare(
            startU, endU, mapping->uModifier,
            endpointDistance, length,
            &block->fixedU, &block->fixedUStep)) {
        return false;
    }
    if (!pingoPerspectiveFixedComponentPrepare(
            startV, endV, mapping->vModifier,
            endpointDistance, length,
            &block->fixedV, &block->fixedVStep)) {
        return false;
    }
    return true;
}

/*
 * Prepare one block of an unconditionally subdivided-affine texture span.
 *
 * Full blocks use perspective-correct endpoints eight samples apart and draw
 * offsets 0..7. The final 1..8-pixel block instead ends on its last covered
 * sample. This keeps an exact multiple-of-eight span from consulting its
 * exclusive right endpoint.
 *
 * The recovered right endpoint becomes the next block's left endpoint, so a
 * shared boundary is divided only once. length and end are initialized even
 * when either required endpoint is invalid. A valid right endpoint can
 * therefore resume the following block after an invalid left endpoint.
 */
static inline bool pingoPerspectiveSpanBlockPrepare(
        PingoPerspectiveBoundary start,
        PingoPerspectiveAttributes xStep,
        uint32_t remaining,
        const PingoPerspectiveFixedMapping * mapping,
        PingoPerspectiveSpanBlock * block)
{
    block->length = remaining > PINGO_PERSPECTIVE_BLOCK_LENGTH
        ? PINGO_PERSPECTIVE_BLOCK_LENGTH
        : remaining;
    block->end = start;
    block->u = start.valid ? start.u : 0.0f;
    block->v = start.valid ? start.v : 0.0f;
    block->uStep = 0.0f;
    block->vStep = 0.0f;
    block->fixedU = 0;
    block->fixedV = 0;
    block->fixedUStep = 0;
    block->fixedVStep = 0;
    block->fixedValid = false;

    if (block->length == 0u) {
        return false;
    }

    const uint32_t endpointDistance =
        remaining > PINGO_PERSPECTIVE_BLOCK_LENGTH
            ? PINGO_PERSPECTIVE_BLOCK_LENGTH
            : block->length - 1u;
    if (endpointDistance != 0u) {
        block->end = pingoPerspectiveBoundaryRecover(
            pingoPerspectiveAttributesAdvance(
                start.attributes, xStep, endpointDistance));
    }

    if (!start.valid || !block->end.valid) {
        return false;
    }

    if (mapping != 0) {
        block->fixedValid = pingoPerspectiveFixedBlockPrepare(
            &start, &block->end, mapping,
            endpointDistance, block->length, block);
        if (block->fixedValid) {
            return true;
        }
    }

    /*
     * The ordinary path never pays these floating divisions. Retain them
     * solely for the defined non-finite/out-of-range safety fallback.
     */
    if (endpointDistance != 0u) {
        const float inverseDistance =
            endpointDistance == PINGO_PERSPECTIVE_BLOCK_LENGTH
                ? 1.0f / (float)PINGO_PERSPECTIVE_BLOCK_LENGTH
                : 1.0f / (float)endpointDistance;
        block->uStep =
            (block->end.u - block->u) * inverseDistance;
        block->vStep =
            (block->end.v - block->v) * inverseDistance;
    }
    return true;
}
