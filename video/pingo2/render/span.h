#ifndef PINGO_ROW_SPAN_H
#define PINGO_ROW_SPAN_H

#include <stdint.h>

/* Intersect an inclusive offset interval with value + step*x >= 0.
 * Inputs are sign-normalized int32 values in int64, including -INT32_MIN.
 * Division operands fit uint32: do not introduce a target int64 divide.
 * Zero edges are included; no top-left ownership rule is imposed. */
static inline int pingoSpanEdge(int64_t value, int64_t step,
                                uint32_t *first, uint32_t *last) {
    if (step > 0) {
        if (value < 0) {
            uint32_t numerator = (uint32_t)-value;
            uint32_t lower = (numerator - 1u) / (uint32_t)step + 1u;
            if (lower > *first) *first = lower;
        }
    } else if (step < 0) {
        if (value < 0) return 0;
        uint32_t upper = (uint32_t)value / (uint32_t)-step;
        if (upper < *last) *last = upper;
    } else if (value < 0) {
        return 0;
    }
    return *first <= *last;
}

/* Exact offsets [first,end) within [0,width) for the three signed edges.
 * Values are evaluated at the clipped bounding-box row origin, not x=0.
 * area selects the existing winding predicate; zero area/width is empty.
 * Full int32 inputs are supported without negation/product overflow. Empty
 * output is always [0,0). Callers retain their existing edge-setup domain. */
static inline int pingoRowSpan(int32_t area, int32_t width,
                               int32_t w0, int32_t w1, int32_t w2,
                               int32_t a0, int32_t a1, int32_t a2,
                               int32_t *first, int32_t *end) {
    *first = *end = 0;
    if (!area || width <= 0) return 0;
    int64_t sign = area > 0 ? 1 : -1;
    uint32_t lower = 0, upper = (uint32_t)width - 1u;
    if (!pingoSpanEdge(sign * w0, sign * a0, &lower, &upper) ||
        !pingoSpanEdge(sign * w1, sign * a1, &lower, &upper) ||
        !pingoSpanEdge(sign * w2, sign * a2, &lower, &upper)) return 0;
    *first = (int32_t)lower;
    *end = (int32_t)(upper + 1u);
    return 1;
}

#endif
