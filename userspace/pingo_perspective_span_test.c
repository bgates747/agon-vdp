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

static void requireInt32(
        const char * label, int32_t actual, int32_t expected)
{
    if (actual != expected) {
        fprintf(
            stderr, "%s: actual %ld expected %ld\n",
            label, (long)actual, (long)expected);
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

static void requireFixedSamples(
        const char * label,
        const PingoPerspectiveSpanBlock * block,
        const int32_t * expectedU,
        const int32_t * expectedV)
{
    int32_t u = block->fixedU;
    int32_t v = block->fixedV;

    for (uint32_t offset = 0; offset < block->length; offset++) {
        if (u < 0 || v < 0) {
            fail("fixed sample helper received a negative coordinate");
        }

        const int32_t sampledU = u >> 16;
        const int32_t sampledV = v >> 16;
        if (sampledU != expectedU[offset] ||
            sampledV != expectedV[offset]) {
            fprintf(
                stderr,
                "%s sample %lu: actual (%ld,%ld) expected (%ld,%ld)\n",
                label, (unsigned long)offset,
                (long)sampledU, (long)sampledV,
                (long)expectedU[offset], (long)expectedV[offset]);
            exit(1);
        }

        if (offset + 1u < block->length) {
            u += block->fixedUStep;
            v += block->fixedVStep;
        }
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
                current, step, remaining, 0, &block)) {
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
            step, 1, 0, &block)) {
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
                current, step, remaining, 0, &block)) {
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
                current, step, remaining, 0, &block)) {
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
                current, step, remaining, 0, &block)) {
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
            step, 8, 0, &block)) {
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
            step, 9, 0, &block)) {
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
            noStep, 1, 0, &block)) {
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
            noStep, 1, 0, &block)) {
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
            tailStep, 5, 0, &block)) {
        fail("zero tail endpoint was accepted");
    }
    if (block.length != 5u) {
        fail("zero-tail rejection lost its length");
    }
    requireFiniteBlock(&block);

    if (pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(tailStart),
            tailStep, 0, 0, &block)) {
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
            boundary, step, 9, 0, &block)) {
        fail("block with invalid left boundary was accepted");
    }
    if (!block.end.valid) {
        fail("valid right boundary did not recover");
    }
    requireFiniteBlock(&block);

    boundary = block.end;
    if (!pingoPerspectiveSpanBlockPrepare(
            boundary, step, 1, 0, &block)) {
        fail("block after invalid boundary did not resume");
    }
    requireNear("recovered boundary U", block.u, boundary.u);
    requireNear("recovered boundary V", block.v, boundary.v);
}

static void testRoundingModifierBranches(void)
{
    requireInt32(
        "positive X modifier",
        pingoPerspectiveRoundingModifier(
            2.0f, 0.5f, 0.1f, 0.5f, 0.0f, 0.0f),
        PINGO_FIXED16_16_POSITIVE_MODIFIER);
    requireInt32(
        "negative X modifier",
        pingoPerspectiveRoundingModifier(
            2.0f, 0.5f, 0.1f, -0.5f, 0.0f, 0.0f),
        PINGO_FIXED16_16_NEGATIVE_MODIFIER);
    requireInt32(
        "positive Y tie-break modifier",
        pingoPerspectiveRoundingModifier(
            2.0f, 0.5f, 0.0f, 0.0f, 0.1f, 0.5f),
        PINGO_FIXED16_16_POSITIVE_MODIFIER);
    requireInt32(
        "negative Y tie-break modifier",
        pingoPerspectiveRoundingModifier(
            2.0f, 0.5f, 0.0f, 0.0f, 0.1f, -0.5f),
        PINGO_FIXED16_16_NEGATIVE_MODIFIER);
    requireInt32(
        "stationary modifier",
        pingoPerspectiveRoundingModifier(
            2.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f),
        PINGO_FIXED16_16_POSITIVE_MODIFIER);

    requireInt32(
        "reversed positive X modifier",
        pingoPerspectiveReversedRoundingModifier(
            2.0f, 0.5f, 0.1f, 0.5f, 0.0f, 0.0f),
        PINGO_FIXED16_16_NEGATIVE_MODIFIER);
    requireInt32(
        "reversed negative X modifier",
        pingoPerspectiveReversedRoundingModifier(
            2.0f, 0.5f, 0.1f, -0.5f, 0.0f, 0.0f),
        PINGO_FIXED16_16_POSITIVE_MODIFIER);
    requireInt32(
        "reversed positive Y tie-break modifier",
        pingoPerspectiveReversedRoundingModifier(
            2.0f, 0.5f, 0.0f, 0.0f, 0.1f, 0.5f),
        PINGO_FIXED16_16_NEGATIVE_MODIFIER);
    requireInt32(
        "reversed negative Y tie-break modifier",
        pingoPerspectiveReversedRoundingModifier(
            2.0f, 0.5f, 0.0f, 0.0f, 0.1f, -0.5f),
        PINGO_FIXED16_16_POSITIVE_MODIFIER);
    requireInt32(
        "reversed stationary modifier",
        pingoPerspectiveReversedRoundingModifier(
            2.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f),
        PINGO_FIXED16_16_POSITIVE_MODIFIER);
}

static void requireFixedConversionRejected(
        const char * label, float value)
{
    int32_t fixed = 123;
    if (pingoFixed16_16FromFloat(value, &fixed)) {
        fprintf(stderr, "%s: conversion was accepted\n", label);
        exit(1);
    }
    requireInt32(label, fixed, 0);
}

static void testFixedConversion(void)
{
    int32_t fixed = 0;

    if (!pingoFixed16_16FromFloat(1.5f, &fixed)) {
        fail("positive fixed conversion was rejected");
    }
    requireInt32("positive fixed conversion", fixed, 98304);

    if (!pingoFixed16_16FromFloat(-1.5f, &fixed)) {
        fail("negative fixed conversion was rejected");
    }
    requireInt32("negative fixed conversion", fixed, -98304);

    if (!pingoFixed16_16FromFloat(0.75f / 65536.0f, &fixed)) {
        fail("positive sub-LSB conversion was rejected");
    }
    requireInt32("positive conversion truncates", fixed, 0);

    if (!pingoFixed16_16FromFloat(-0.75f / 65536.0f, &fixed)) {
        fail("negative sub-LSB conversion was rejected");
    }
    requireInt32("negative conversion truncates", fixed, 0);

    if (!pingoFixed16_16FromFloat(-32768.0f, &fixed)) {
        fail("INT32_MIN fixed conversion was rejected");
    }
    requireInt32("INT32_MIN fixed conversion", fixed, INT32_MIN);

    const float largestPositive =
        nextafterf(32768.0f, 0.0f);
    if (!pingoFixed16_16FromFloat(largestPositive, &fixed)) {
        fail("largest positive fixed conversion was rejected");
    }
    requireInt32(
        "largest positive fixed conversion", fixed, 2147483520);

    requireFixedConversionRejected(
        "positive fixed overflow", 32768.0f);
    requireFixedConversionRejected(
        "negative fixed overflow",
        nextafterf(-32768.0f, -INFINITY));
    requireFixedConversionRejected("positive infinity", INFINITY);
    requireFixedConversionRejected("negative infinity", -INFINITY);
    requireFixedConversionRejected("NaN", NAN);
}

static void testFixedFullBlock(void)
{
    static const int32_t expectedU[] = {
        0, 1, 2, 3, 4, 4, 5, 6
    };
    static const int32_t expectedV[] = {
        0, 0, 1, 1, 2, 2, 2, 3
    };
    const PingoPerspectiveAttributes start = {1.0f, 0.0f, 1.0f};
    const PingoPerspectiveAttributes step = {0.0f, 0.125f, -0.125f};
    const PingoPerspectiveFixedMapping mapping = {
        .uScale = 7.0f,
        .vScale = 3.0f,
        .uModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER,
        .vModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER
    };
    PingoPerspectiveSpanBlock block;

    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(start),
            step, 9, &mapping, &block)) {
        fail("fixed full block was rejected");
    }
    if (block.length != 8u || !block.fixedValid) {
        fail("fixed full block was not prepared");
    }
    requireInt32("full fixed U", block.fixedU, 32768);
    requireInt32("full fixed V", block.fixedV, 32768);
    requireInt32("full fixed U step", block.fixedUStep, 57344);
    requireInt32("full fixed V step", block.fixedVStep, 24576);
    requireFixedSamples("full fixed block", &block, expectedU, expectedV);
}

static void testFixedTailBlock(void)
{
    static const int32_t expectedU[] = {0, 1, 2, 3, 4};
    static const int32_t expectedV[] = {0, 0, 1, 1, 2};
    const PingoPerspectiveAttributes start = {1.0f, 0.0f, 1.0f};
    const PingoPerspectiveAttributes step = {0.0f, 0.125f, -0.125f};
    const PingoPerspectiveFixedMapping mapping = {
        .uScale = 7.0f,
        .vScale = 3.0f,
        .uModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER,
        .vModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER
    };
    PingoPerspectiveSpanBlock block;

    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(start),
            step, 5, &mapping, &block)) {
        fail("fixed tail block was rejected");
    }
    if (block.length != 5u || !block.fixedValid) {
        fail("fixed tail block was not prepared");
    }
    requireInt32("tail fixed U", block.fixedU, 32768);
    requireInt32("tail fixed V", block.fixedV, 32768);
    requireInt32("tail fixed U step", block.fixedUStep, 57344);
    requireInt32("tail fixed V step", block.fixedVStep, 24576);
    requireFixedSamples("tail fixed block", &block, expectedU, expectedV);
}

static void requireExhaustiveFixedTails(
        const char * direction,
        PingoPerspectiveAttributes start,
        PingoPerspectiveAttributes xStep,
        PingoPerspectiveFixedMapping mapping,
        int32_t oracleStartU,
        int32_t oracleStartV,
        int32_t oraclePerPixelU,
        int32_t oraclePerPixelV)
{
    for (uint32_t length = 1u;
         length <= PINGO_PERSPECTIVE_BLOCK_LENGTH;
         length++) {
        const uint32_t endpointDistance = length - 1u;
        PingoPerspectiveSpanBlock block;

        if (!pingoPerspectiveSpanBlockPrepare(
                pingoPerspectiveBoundaryRecover(start),
                xStep, length, &mapping, &block)) {
            fprintf(
                stderr, "%s tail length %lu was rejected\n",
                direction, (unsigned long)length);
            exit(1);
        }
        if (!block.fixedValid || block.length != length) {
            fprintf(
                stderr, "%s tail length %lu was not fixed\n",
                direction, (unsigned long)length);
            exit(1);
        }

        /*
         * A tail's endpoint is its final covered sample, N-1 pixels from
         * the start. These exact binary fractions keep the integer oracle
         * independent of the production float-to-fixed conversion.
         */
        requireNear(
            "tail endpoint reciprocal W",
            block.end.attributes.reciprocalW,
            start.reciprocalW +
                xStep.reciprocalW * (float)endpointDistance);
        requireNear(
            "tail endpoint U/W",
            block.end.attributes.uOverW,
            start.uOverW +
                xStep.uOverW * (float)endpointDistance);
        requireNear(
            "tail endpoint V/W",
            block.end.attributes.vOverW,
            start.vOverW +
                xStep.vOverW * (float)endpointDistance);

        const int32_t oracleDeltaNumeratorU =
            oraclePerPixelU * (int32_t)endpointDistance;
        const int32_t oracleDeltaNumeratorV =
            oraclePerPixelV * (int32_t)endpointDistance;
        const int32_t oracleStepU = endpointDistance == 0u
            ? 0
            : oracleDeltaNumeratorU / (int32_t)endpointDistance;
        const int32_t oracleStepV = endpointDistance == 0u
            ? 0
            : oracleDeltaNumeratorV / (int32_t)endpointDistance;

        if (block.fixedU != oracleStartU ||
            block.fixedV != oracleStartV ||
            block.fixedUStep != oracleStepU ||
            block.fixedVStep != oracleStepV) {
            fprintf(
                stderr,
                "%s tail length %lu: fixed start/step "
                "(%ld,%ld)+(%ld,%ld), expected "
                "(%ld,%ld)+(%ld,%ld)\n",
                direction, (unsigned long)length,
                (long)block.fixedU, (long)block.fixedV,
                (long)block.fixedUStep, (long)block.fixedVStep,
                (long)oracleStartU, (long)oracleStartV,
                (long)oracleStepU, (long)oracleStepV);
            exit(1);
        }

        int32_t actualU = block.fixedU;
        int32_t actualV = block.fixedV;
        for (uint32_t offset = 0u; offset < length; offset++) {
            const int32_t oracleU =
                oracleStartU + oracleStepU * (int32_t)offset;
            const int32_t oracleV =
                oracleStartV + oracleStepV * (int32_t)offset;
            const int32_t actualSampleU = actualU / 65536;
            const int32_t actualSampleV = actualV / 65536;
            const int32_t oracleSampleU = oracleU / 65536;
            const int32_t oracleSampleV = oracleV / 65536;

            if (actualU != oracleU || actualV != oracleV ||
                actualSampleU != oracleSampleU ||
                actualSampleV != oracleSampleV) {
                fprintf(
                    stderr,
                    "%s tail length %lu sample %lu: "
                    "fixed (%ld,%ld), texel (%ld,%ld); "
                    "expected fixed (%ld,%ld), texel (%ld,%ld)\n",
                    direction, (unsigned long)length,
                    (unsigned long)offset,
                    (long)actualU, (long)actualV,
                    (long)actualSampleU, (long)actualSampleV,
                    (long)oracleU, (long)oracleV,
                    (long)oracleSampleU, (long)oracleSampleV);
                exit(1);
            }

            if (offset + 1u < length) {
                actualU += block.fixedUStep;
                actualV += block.fixedVStep;
            }
        }
    }
}

static void testAllFixedTailLengths(void)
{
    /*
     * Forward U advances 3/4 texel per pixel. Because fixed V stores the
     * top-down memory row, increasing normalized V walks backward by 1/2
     * texel per pixel and exercises a negative fixed delta.
     */
    requireExhaustiveFixedTails(
        "forward",
        (PingoPerspectiveAttributes){1.0f, 0.125f, 0.25f},
        (PingoPerspectiveAttributes){0.0f, 0.046875f, 0.03125f},
        (PingoPerspectiveFixedMapping){
            .uScale = 16.0f,
            .vScale = 16.0f,
            .uModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER,
            .vModifier = PINGO_FIXED16_16_NEGATIVE_MODIFIER
        },
        163840, 819199, 49152, -32768);

    /*
     * Reverse the paths as a second oracle: U now has the negative delta,
     * while the top-down V memory row advances positively.
     */
    requireExhaustiveFixedTails(
        "reverse",
        (PingoPerspectiveAttributes){1.0f, 0.75f, 0.75f},
        (PingoPerspectiveAttributes){0.0f, -0.046875f, -0.03125f},
        (PingoPerspectiveFixedMapping){
            .uScale = 16.0f,
            .vScale = 16.0f,
            .uModifier = PINGO_FIXED16_16_NEGATIVE_MODIFIER,
            .vModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER
        },
        819199, 294912, -49152, 32768);
}

static void testFixedOnePixelBlock(void)
{
    static const int32_t expectedU[] = {2};
    static const int32_t expectedV[] = {1};
    const PingoPerspectiveAttributes start = {1.0f, 0.25f, 0.75f};
    const PingoPerspectiveAttributes step = {0.5f, 100.0f, -100.0f};
    const PingoPerspectiveFixedMapping mapping = {
        .uScale = 8.0f,
        .vScale = 4.0f,
        .uModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER,
        .vModifier = PINGO_FIXED16_16_NEGATIVE_MODIFIER
    };
    PingoPerspectiveSpanBlock block;

    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(start),
            step, 1, &mapping, &block)) {
        fail("fixed one-pixel block was rejected");
    }
    if (block.length != 1u || !block.fixedValid) {
        fail("fixed one-pixel block was not prepared");
    }
    requireInt32("one-pixel fixed U", block.fixedU, 163840);
    requireInt32("one-pixel fixed V", block.fixedV, 98303);
    requireInt32("one-pixel fixed U step", block.fixedUStep, 0);
    requireInt32("one-pixel fixed V step", block.fixedVStep, 0);
    requireFixedSamples(
        "one-pixel fixed block", &block, expectedU, expectedV);
}

static void testFixedVRowInversion(void)
{
    const PingoPerspectiveFixedMapping mapping = {
        .uScale = 7.0f,
        .vScale = 3.0f,
        .uModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER,
        .vModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER
    };
    const PingoPerspectiveAttributes noStep = {0.0f, 0.0f, 0.0f};
    PingoPerspectiveSpanBlock top;
    PingoPerspectiveSpanBlock bottom;

    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(
                (PingoPerspectiveAttributes){1.0f, 0.0f, 1.0f}),
            noStep, 1, &mapping, &top) ||
        !top.fixedValid) {
        fail("top-row fixed block was rejected");
    }
    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(
                (PingoPerspectiveAttributes){1.0f, 0.0f, 0.0f}),
            noStep, 1, &mapping, &bottom) ||
        !bottom.fixedValid) {
        fail("bottom-row fixed block was rejected");
    }

    requireInt32("UV V=1 maps to top row", top.fixedV, 32768);
    requireInt32(
        "UV V=0 maps to bottom row", bottom.fixedV, 229376);
}

static void testFixedOverflowFallsBack(void)
{
    const PingoPerspectiveFixedMapping oversizedMapping = {
        .uScale = 40000.0f,
        .vScale = 1.0f,
        .uModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER,
        .vModifier = PINGO_FIXED16_16_POSITIVE_MODIFIER
    };
    const PingoPerspectiveAttributes start = {1.0f, 1.0f, 0.5f};
    const PingoPerspectiveAttributes noStep = {0.0f, 0.0f, 0.0f};
    PingoPerspectiveSpanBlock block;

    if (!pingoPerspectiveSpanBlockPrepare(
            pingoPerspectiveBoundaryRecover(start),
            noStep, 2, &oversizedMapping, &block)) {
        fail("float fallback block was rejected");
    }
    if (block.fixedValid) {
        fail("unrepresentable block retained fixed-point sampling");
    }
    requireNear("fallback U", block.u, 1.0f);
    requireNear("fallback V", block.v, 0.5f);
    requireNear("fallback U step", block.uStep, 0.0f);
    requireNear("fallback V step", block.vStep, 0.0f);

    int32_t value = 123;
    int32_t step = 123;
    if (pingoPerspectiveFixedComponentPrepare(
            nextafterf(32768.0f, 0.0f),
            nextafterf(32768.0f, 0.0f),
            PINGO_FIXED16_16_POSITIVE_MODIFIER,
            0, 1, &value, &step)) {
        fail("modifier overflow retained fixed-point sampling");
    }
    requireInt32("modifier-overflow value reset", value, 0);
    requireInt32("modifier-overflow step reset", step, 0);

    value = 123;
    step = 123;
    if (pingoPerspectiveFixedComponentPrepare(
            32760.0f, 32768.0f,
            PINGO_FIXED16_16_POSITIVE_MODIFIER,
            7, 8, &value, &step)) {
        fail("accumulator overflow retained fixed-point sampling");
    }
    requireInt32("accumulator-overflow value reset", value, 0);
    requireInt32("accumulator-overflow step reset", step, 0);
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
    testRoundingModifierBranches();
    testFixedConversion();
    testFixedFullBlock();
    testFixedTailBlock();
    testAllFixedTailLengths();
    testFixedOnePixelBlock();
    testFixedVRowInversion();
    testFixedOverflowFallsBack();
    puts("Pingo perspective-span test passed");
    return 0;
}
