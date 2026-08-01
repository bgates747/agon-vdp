#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct tag_TriangleRowSpan {
    uint32_t begin;
    uint32_t end;
} TriangleRowSpan;

static inline uint32_t triangleSpanMagnitude(int32_t value)
{
    // Unsigned negation is defined for INT32_MIN.
    return value < 0
        ? 0u - (uint32_t)value
        : (uint32_t)value;
}

static inline bool triangleRowSpanConstrain(
        int32_t edgeAtStart, int32_t xStep,
        bool positiveArea, TriangleRowSpan * span)
{
    const bool startsInside = positiveArea
        ? edgeAtStart >= 0
        : edgeAtStart <= 0;

    if (xStep == 0) {
        return startsInside && span->begin < span->end;
    }

    // Equivalent to sign(area) * xStep > 0, without signed overflow.
    const bool increasesTowardInside = positiveArea
        ? xStep > 0
        : xStep < 0;

    if (increasesTowardInside) {
        if (!startsInside) {
            const uint32_t numerator =
                triangleSpanMagnitude(edgeAtStart);
            const uint32_t denominator =
                triangleSpanMagnitude(xStep);
            // ceil(numerator / denominator), without addition overflow.
            const uint32_t first =
                1u + (numerator - 1u) / denominator;
            if (first > span->begin) {
                span->begin = first;
            }
        }
    } else {
        if (!startsInside) {
            return false;
        }
        // The exact zero crossing remains part of the current fill rule.
        const uint32_t pastLast =
            triangleSpanMagnitude(edgeAtStart) /
                triangleSpanMagnitude(xStep) +
            1u;
        if (pastLast < span->end) {
            span->end = pastLast;
        }
    }

    return span->begin < span->end;
}

/*
 * Intersect the three affine edge inequalities for one integer sample row.
 * begin/end are relative to the caller's clipped bounding-box minimum, and
 * end is exclusive. Zero-valued edges remain included for both windings,
 * deliberately preserving Pingo's current shared-edge ownership.
 */
static inline bool triangleRowSpanFind(
        int32_t area,
        int32_t w0, int32_t w1, int32_t w2,
        int32_t step0, int32_t step1, int32_t step2,
        uint32_t width, TriangleRowSpan * span)
{
    span->begin = 0;
    span->end = width;

    const bool positiveArea = area > 0;
    return triangleRowSpanConstrain(
               w0, step0, positiveArea, span) &&
           triangleRowSpanConstrain(
               w1, step1, positiveArea, span) &&
           triangleRowSpanConstrain(
               w2, step2, positiveArea, span);
}
