#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PINGO_PERSPECTIVE_BLOCK_LENGTH 8u

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

typedef struct tag_PingoPerspectiveSpanBlock {
    uint32_t length;
    PingoPerspectiveBoundary end;
    float u;
    float v;
    float uStep;
    float vStep;
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

    if (endpointDistance == 0u) {
        return true;
    }

    const float inverseDistance =
        endpointDistance == PINGO_PERSPECTIVE_BLOCK_LENGTH
            ? 1.0f / (float)PINGO_PERSPECTIVE_BLOCK_LENGTH
            : 1.0f / (float)endpointDistance;
    block->uStep = (block->end.u - block->u) * inverseDistance;
    block->vStep = (block->end.v - block->v) * inverseDistance;
    return true;
}
