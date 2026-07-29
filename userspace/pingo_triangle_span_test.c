#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "render/triangle_span.h"

typedef struct TestPoint {
    int32_t x;
    int32_t y;
} TestPoint;

static int32_t orient2d(
        TestPoint a, TestPoint b, TestPoint c)
{
    return (b.x - a.x) * (c.y - a.y) -
           (b.y - a.y) * (c.x - a.x);
}

static int32_t clampCoordinate(int32_t value, int32_t limit)
{
    if (value < 0)
        return 0;
    if (value > limit)
        return limit;
    return value;
}

static int sampleInside(
        int32_t area, int32_t w0, int32_t w1, int32_t w2)
{
    if (area > 0)
        return w0 >= 0 && w1 >= 0 && w2 >= 0;
    return w0 <= 0 && w1 <= 0 && w2 <= 0;
}

static void requireSpanMatches(
        int32_t area,
        int32_t w0, int32_t w1, int32_t w2,
        int32_t step0, int32_t step1, int32_t step2,
        uint32_t width)
{
    uint32_t expectedBegin = width;
    uint32_t expectedEnd = width;
    int found = 0;

    for (uint32_t offset = 0; offset < width; offset++) {
        int inside = sampleInside(
            area,
            w0 + step0 * (int32_t)offset,
            w1 + step1 * (int32_t)offset,
            w2 + step2 * (int32_t)offset);
        if (inside && !found) {
            expectedBegin = offset;
            found = 1;
        } else if (!inside && found && expectedEnd == width) {
            expectedEnd = offset;
        } else if (inside && expectedEnd != width) {
            fprintf(stderr, "non-contiguous reference coverage\n");
            exit(1);
        }
    }

    TriangleRowSpan actual;
    int actualFound = triangleRowSpanFind(
        area, w0, w1, w2, step0, step1, step2, width, &actual);
    if (actualFound != found ||
        (found &&
         (actual.begin != expectedBegin || actual.end != expectedEnd))) {
        fprintf(
            stderr,
            "span mismatch: area=%d width=%u "
            "w=(%d,%d,%d) step=(%d,%d,%d) "
            "expected=%s%u..%u actual=%s%u..%u\n",
            area, width, w0, w1, w2, step0, step1, step2,
            found ? "" : "empty ", expectedBegin, expectedEnd,
            actualFound ? "" : "empty ", actual.begin, actual.end);
        exit(1);
    }
}

static void testExhaustiveTriangles(void)
{
    const int32_t low = -2;
    const int32_t high = 3;

    for (int32_t ax = low; ax <= high; ax++)
    for (int32_t ay = low; ay <= high; ay++)
    for (int32_t bx = low; bx <= high; bx++)
    for (int32_t by = low; by <= high; by++)
    for (int32_t cx = low; cx <= high; cx++)
    for (int32_t cy = low; cy <= high; cy++) {
        TestPoint a = {ax, ay};
        TestPoint b = {bx, by};
        TestPoint c = {cx, cy};
        int32_t area = orient2d(a, b, c);
        if (area == 0)
            continue;

        for (int32_t viewportWidth = 1;
             viewportWidth <= 5; viewportWidth++) {
            for (int32_t viewportHeight = 1;
                 viewportHeight <= 5; viewportHeight++) {
                int32_t minX = ax < bx ? ax : bx;
                minX = minX < cx ? minX : cx;
                int32_t maxX = ax > bx ? ax : bx;
                maxX = maxX > cx ? maxX : cx;
                int32_t minY = ay < by ? ay : by;
                minY = minY < cy ? minY : cy;
                int32_t maxY = ay > by ? ay : by;
                maxY = maxY > cy ? maxY : cy;

                minX = clampCoordinate(minX, viewportWidth);
                maxX = clampCoordinate(maxX, viewportWidth);
                minY = clampCoordinate(minY, viewportHeight);
                maxY = clampCoordinate(maxY, viewportHeight);

                int32_t step0 = b.y - c.y;
                int32_t step1 = c.y - a.y;
                int32_t step2 = a.y - b.y;

                for (int32_t y = minY; y < maxY; y++) {
                    TestPoint start = {minX, y};
                    requireSpanMatches(
                        area,
                        orient2d(b, c, start),
                        orient2d(c, a, start),
                        orient2d(a, b, start),
                        step0, step1, step2,
                        (uint32_t)(maxX - minX));
                }
            }
        }
    }
}

static uint32_t randomState = 0x50494E47u;

static int32_t nextRange(int32_t low, int32_t high)
{
    randomState = randomState * 1664525u + 1013904223u;
    return low + (int32_t)(randomState % (uint32_t)(high - low + 1));
}

static void testRandomRows(void)
{
    for (uint32_t test = 0; test < 200000; test++) {
        requireSpanMatches(
            (test & 1u) ? 1 : -1,
            nextRange(-100000, 100000),
            nextRange(-100000, 100000),
            nextRange(-100000, 100000),
            nextRange(-2000, 2000),
            nextRange(-2000, 2000),
            nextRange(-2000, 2000),
            (uint32_t)nextRange(0, 96));
    }
}

static void testIntegerLimits(void)
{
    TriangleRowSpan span;

    if (!triangleRowSpanFind(
            1, INT32_MIN, 0, 0, INT32_MAX, 0, 0, 4, &span) ||
        span.begin != 2 || span.end != 4) {
        fprintf(stderr, "INT32_MIN increasing constraint failed\n");
        exit(1);
    }

    if (!triangleRowSpanFind(
            1, 0, 0, 0, INT32_MIN, 0, 0, 4, &span) ||
        span.begin != 0 || span.end != 1) {
        fprintf(stderr, "INT32_MIN decreasing constraint failed\n");
        exit(1);
    }
}

int main(void)
{
    testExhaustiveTriangles();
    testRandomRows();
    testIntegerLimits();
    puts("Pingo triangle row-span test passed");
    return 0;
}
