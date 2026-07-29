#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "render/perspective_span.h"

#define ARRAY_LENGTH(values) (sizeof(values) / sizeof((values)[0]))

static void fail(const char * message)
{
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static void requireNear(
        const char * label, float actual, float expected)
{
    const float tolerance =
        1.0e-5f * fmaxf(1.0f, fabsf(expected));
    if (!isfinite(actual) || fabsf(actual - expected) > tolerance) {
        fprintf(
            stderr, "%s: actual %.9g expected %.9g\n",
            label, actual, expected);
        exit(1);
    }
}

static void requireFiniteBlock(
        const PingoPerspectiveSpanBlock * block)
{
    if (!isfinite(block->u) || !isfinite(block->v) ||
        !isfinite(block->uStep) || !isfinite(block->vStep) ||
        !isfinite(block->end.u) || !isfinite(block->end.v)) {
        fail("block contains a non-finite initialized output");
    }
}

static void requirePartition(
        uint32_t width,
        const uint32_t * expected,
        uint32_t expectedCount)
{
    PingoPerspectiveBoundary current =
        pingoPerspectiveBoundaryRecover(
            (PingoPerspectiveAttributes){1.0f, 0.0f, 0.0f});
    const PingoPerspectiveAttributes step = {0.0f, 0.1f, 0.2f};
    uint32_t remaining = width;
    uint32_t blockIndex = 0;

    while (remaining > 0u) {
        PingoPerspectiveSpanBlock block;
        if (!pingoPerspectiveSpanBlockPrepare(
                current, step, remaining, &block)) {
            fail("valid partition block was rejected");
        }
        if (blockIndex >= expectedCount ||
            block.length != expected[blockIndex]) {
            fail("unexpected perspective-span partition");
        }
        remaining -= block.length;
        current = block.end;
        blockIndex++;
    }

    if (blockIndex != expectedCount) {
        fail("perspective-span partition ended early");
    }
}

static void testPartitions(void)
{
    static const uint32_t width1[] = {1};
    static const uint32_t width5[] = {5};
    static const uint32_t width8[] = {8};
    static const uint32_t width9[] = {8, 1};
    static const uint32_t width13[] = {8, 5};
    static const uint32_t width16[] = {8, 8};
    static const uint32_t width17[] = {8, 8, 1};

    requirePartition(1, width1, ARRAY_LENGTH(width1));
    requirePartition(5, width5, ARRAY_LENGTH(width5));
    requirePartition(8, width8, ARRAY_LENGTH(width8));
    requirePartition(9, width9, ARRAY_LENGTH(width9));
    requirePartition(13, width13, ARRAY_LENGTH(width13));
    requirePartition(16, width16, ARRAY_LENGTH(width16));
    requirePartition(17, width17, ARRAY_LENGTH(width17));
}

static void testOnePixel(void)
{
    const PingoPerspectiveAttributes start = {2.0f, 0.5f, 1.0f};
    const PingoPerspectiveAttributes step = {0.25f, 0.5f, 0.75f};
    PingoPerspectiveSpanBlock block;

    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(start),
            step, 1, &block)) {
        fail("one-pixel block was rejected");
    }
    if (block.length != 1u) {
        fail("one-pixel block has wrong length");
    }
    requireNear("one-pixel U", block.u, 0.25f);
    requireNear("one-pixel V", block.v, 0.5f);
    requireNear("one-pixel U step", block.uStep, 0.0f);
    requireNear("one-pixel V step", block.vStep, 0.0f);
}

static void testConstantReciprocalW(void)
{
    const PingoPerspectiveAttributes first = {2.0f, 0.4f, 1.2f};
    PingoPerspectiveBoundary current =
        pingoPerspectiveBoundaryRecover(first);
    const PingoPerspectiveAttributes step = {0.0f, 0.06f, -0.04f};
    uint32_t remaining = 17;
    uint32_t x = 0;

    while (remaining > 0u) {
        PingoPerspectiveSpanBlock block;
        if (!pingoPerspectiveSpanBlockPrepare(
                current, step, remaining, &block)) {
            fail("constant-q block was rejected");
        }

        float u = block.u;
        float v = block.v;
        for (uint32_t offset = 0; offset < block.length; offset++, x++) {
            requireNear(
                "constant-q U", u,
                (first.uOverW + step.uOverW * (float)x) /
                    first.reciprocalW);
            requireNear(
                "constant-q V", v,
                (first.vOverW + step.vOverW * (float)x) /
                    first.reciprocalW);
            u += block.uStep;
            v += block.vStep;
        }

        remaining -= block.length;
        current = block.end;
    }
}

static void testPerspectiveBoundaries(void)
{
    const PingoPerspectiveAttributes first = {0.4f, 0.12f, 0.32f};
    PingoPerspectiveBoundary current =
        pingoPerspectiveBoundaryRecover(first);
    const PingoPerspectiveAttributes step = {0.07f, 0.025f, -0.011f};
    uint32_t remaining = 17;
    uint32_t x = 0;

    while (remaining > 0u) {
        PingoPerspectiveSpanBlock block;
        if (!pingoPerspectiveSpanBlockPrepare(
                current, step, remaining, &block)) {
            fail("positive perspective block was rejected");
        }
        requireFiniteBlock(&block);

        const uint32_t endpointDistance =
            remaining > PINGO_PERSPECTIVE_BLOCK_LENGTH
                ? PINGO_PERSPECTIVE_BLOCK_LENGTH
                : block.length - 1u;
        const float startU = current.u;
        const float startV = current.v;
        float u = block.u;
        float v = block.v;
        for (uint32_t offset = 0; offset < block.length; offset++, x++) {
            const float fraction = endpointDistance == 0u
                ? 0.0f
                : (float)offset / (float)endpointDistance;
            requireNear(
                "perspective affine U", u,
                startU + (block.end.u - startU) * fraction);
            requireNear(
                "perspective affine V", v,
                startV + (block.end.v - startV) * fraction);
            if (x == 0u || x == 8u || x == 16u) {
                const float q =
                    first.reciprocalW + step.reciprocalW * (float)x;
                requireNear(
                    "perspective boundary U", u,
                    (first.uOverW + step.uOverW * (float)x) / q);
                requireNear(
                    "perspective boundary V", v,
                    (first.vOverW + step.vOverW * (float)x) / q);
            }
            u += block.uStep;
            v += block.vStep;
        }

        remaining -= block.length;
        current = block.end;
    }
}

static void testRapidPositivePerspective(void)
{
    PingoPerspectiveBoundary current =
        pingoPerspectiveBoundaryRecover(
            (PingoPerspectiveAttributes){0.05f, 0.2f, -0.3f});
    const PingoPerspectiveAttributes step = {0.4f, 0.03f, 0.02f};
    uint32_t remaining = 13;

    while (remaining > 0u) {
        PingoPerspectiveSpanBlock block;
        if (!pingoPerspectiveSpanBlockPrepare(
                current, step, remaining, &block)) {
            fail("rapid positive-q block was rejected");
        }
        requireFiniteBlock(&block);
        remaining -= block.length;
        current = block.end;
    }
}

static void testExclusiveEndpointRule(void)
{
    const PingoPerspectiveAttributes start = {1.0f, 0.25f, 0.75f};
    const PingoPerspectiveAttributes step = {-0.125f, 0.0f, 0.0f};
    PingoPerspectiveSpanBlock block;

    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(start),
            step, 8, &block)) {
        fail("width-eight tail consulted the exclusive endpoint");
    }
    if (block.length != 8u) {
        fail("width-eight tail has wrong length");
    }
    requireNear(
        "width-eight endpoint q",
        block.end.attributes.reciprocalW, 0.125f);

    if (pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(start),
            step, 9, &block)) {
        fail("width-nine full block accepted a zero endpoint");
    }
    if (block.length != 8u) {
        fail("rejected full block has wrong length");
    }
    requireFiniteBlock(&block);
}

static void testZeroEndpointRejection(void)
{
    const PingoPerspectiveAttributes noStep = {0.0f, 0.0f, 0.0f};
    PingoPerspectiveSpanBlock block;

    const PingoPerspectiveAttributes positiveZero = {
        0.0f, 0.25f, 0.75f
    };
    if (pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(positiveZero),
            noStep, 1, &block)) {
        fail("positive-zero start was accepted");
    }
    if (block.length != 1u) {
        fail("positive-zero rejection lost its length");
    }
    requireFiniteBlock(&block);

    const PingoPerspectiveAttributes negativeZero = {
        -0.0f, 0.25f, 0.75f
    };
    if (pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(negativeZero),
            noStep, 1, &block)) {
        fail("negative-zero start was accepted");
    }
    requireFiniteBlock(&block);

    const PingoPerspectiveAttributes tailStart = {
        1.0f, 0.25f, 0.75f
    };
    const PingoPerspectiveAttributes tailStep = {
        -0.25f, 0.0f, 0.0f
    };
    if (pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(tailStart),
            tailStep, 5, &block)) {
        fail("zero tail endpoint was accepted");
    }
    if (block.length != 5u) {
        fail("zero-tail rejection lost its length");
    }
    requireFiniteBlock(&block);

    if (pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(tailStart),
            tailStep, 0, &block)) {
        fail("empty block was accepted");
    }
    if (block.length != 0u) {
        fail("empty block has nonzero length");
    }
    requireFiniteBlock(&block);
}

static void testInvalidBoundaryCanRecover(void)
{
    const PingoPerspectiveAttributes step = {0.25f, 0.1f, -0.05f};
    PingoPerspectiveBoundary boundary =
        pingoPerspectiveBoundaryRecover(
            (PingoPerspectiveAttributes){0.0f, 0.3f, 0.4f});
    PingoPerspectiveSpanBlock block;

    if (pingoPerspectiveSpanBlockPrepare(
            boundary, step, 9, &block)) {
        fail("block with invalid left boundary was accepted");
    }
    if (!block.end.valid) {
        fail("valid right boundary did not recover");
    }
    requireFiniteBlock(&block);

    boundary = block.end;
    if (!pingoPerspectiveSpanBlockPrepare(
            boundary, step, 1, &block)) {
        fail("block after invalid boundary did not resume");
    }
    requireNear("recovered boundary U", block.u, boundary.u);
    requireNear("recovered boundary V", block.v, boundary.v);
}

int main(void)
{
    testPartitions();
    testOnePixel();
    testConstantReciprocalW();
    testPerspectiveBoundaries();
    testRapidPositivePerspective();
    testExclusiveEndpointRule();
    testZeroEndpointRejection();
    testInvalidBoundaryCanRecover();
    puts("Pingo perspective-span test passed");
    return 0;
}
