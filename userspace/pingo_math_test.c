#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../video/pingo/math/mat4.h"
#include "../video/pingo/math/vec3.h"

static void assert_close(float actual, float expected)
{
    assert(fabsf(actual - expected) <= 0.000001f);
}

static void assert_within(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static void assert_relative(float actual, float expected, float tolerance)
{
    float scale = fmaxf(1.0f, fabsf(expected));
    if (fabsf(actual - expected) > tolerance * scale) {
        fprintf(
            stderr,
            "relative comparison failed: actual=%g expected=%g tolerance=%g\n",
            actual, expected, tolerance);
    }
    assert(fabsf(actual - expected) <= tolerance * scale);
}

static void assert_matrix_close(Mat4 actual, Mat4 expected)
{
    for (int i = 0; i < 16; i++) {
        assert_close(actual.elements[i], expected.elements[i]);
    }
}

static void assert_vector4_close(Vec4f actual, Vec4f expected)
{
    assert_close(actual.x, expected.x);
    assert_close(actual.y, expected.y);
    assert_close(actual.z, expected.z);
    assert_close(actual.w, expected.w);
}

int main(void)
{
    Vec3f zero = vec3Normalize((Vec3f){0.0f, 0.0f, 0.0f});
    assert(zero.x == 0.0f);
    assert(zero.y == 0.0f);
    assert(zero.z == 0.0f);

    Vec3f unit = vec3Normalize((Vec3f){0.0f, -1.0f, 0.0f});
    assert(unit.x == 0.0f);
    assert(unit.y == -1.0f);
    assert(unit.z == 0.0f);

    Vec3f normalized = vec3Normalize((Vec3f){3.0f, 4.0f, 0.0f});
    assert_close(normalized.x, 0.6f);
    assert_close(normalized.y, 0.8f);
    assert_close(normalized.z, 0.0f);
    assert_close(vec3Dot(normalized, normalized), 1.0f);

    Mat4 identity = mat4Identity();
    assert_matrix_close(mat4Inverse(&identity), identity);

    Mat4 translation = mat4Translate((Vec3f){3.0f, -4.0f, 5.0f});
    Mat4 expected_translation_inverse =
        mat4Translate((Vec3f){-3.0f, 4.0f, -5.0f});
    assert_matrix_close(
        mat4Inverse(&translation), expected_translation_inverse);

    // Keep a general-case check so the optimized exact cases cannot shadow
    // rotation-bearing camera poses.
    Mat4 rotation = mat4RotateZ(0.5f);
    Mat4 expected_rotation_inverse = mat4RotateZ(-0.5f);
    assert_matrix_close(mat4Inverse(&rotation), expected_rotation_inverse);

    Mat4 view_rotation = mat4RotateY(0.25f);
    Mat4 view_translation =
        mat4Translate((Vec3f){2.0f, -3.0f, 4.0f});
    Mat4 view = mat4MultiplyM(&view_rotation, &view_translation);
    Mat4 projection = mat4Perspective(1.0f, 100.0f, 4.0f / 3.0f, 0.6f);
    assert_close(mat4NearFromProjection(projection), 1.0f);
    assert_within(mat4FarFromProjection(projection), 100.0f, 0.001f);

    // Extraction remains useful across the entire command-53 domain.  Far
    // recovery divides by C-1, so use a relative tolerance at large ratios.
    const float production_fars[] = {2500.0f, 8000.0f, 65535.0f};
    for (unsigned int i = 0;
         i < sizeof(production_fars) / sizeof(production_fars[0]);
         i++) {
        Mat4 candidate = mat4Perspective(
            1.0f, production_fars[i], 4.0f / 3.0f, 0.6f);
        assert_relative(mat4NearFromProjection(candidate), 1.0f, 0.000001f);
        assert_relative(
            mat4FarFromProjection(candidate), production_fars[i], 0.0005f);
    }

    // mat4Perspective is a generic math API even though Pingo currently
    // fixes its production near plane at one world unit.
    Mat4 generic_projection =
        mat4Perspective(2.5f, 400.0f, 1.0f, 0.8f);
    assert_relative(
        mat4NearFromProjection(generic_projection), 2.5f, 0.000001f);
    assert_relative(
        mat4FarFromProjection(generic_projection), 400.0f, 0.00001f);

    // Pingo's production clip volume is -W <= Z <= 0.  Verify both exact
    // view-space distance boundaries and points immediately outside them;
    // this catches a projection whose far expression can never become
    // negative even though perspective division still looks plausible.
    Vec4f at_near = {0.0f, 0.0f, -1.0f, 1.0f};
    Vec4f before_near = {0.0f, 0.0f, -0.5f, 1.0f};
    Vec4f at_far = {0.0f, 0.0f, -100.0f, 1.0f};
    Vec4f beyond_far = {0.0f, 0.0f, -101.0f, 1.0f};
    at_near = mat4MultiplyVec4(&at_near, &projection);
    before_near = mat4MultiplyVec4(&before_near, &projection);
    at_far = mat4MultiplyVec4(&at_far, &projection);
    beyond_far = mat4MultiplyVec4(&beyond_far, &projection);
    assert_close(at_near.z, 0.0f);
    assert(before_near.z > 0.0f);
    assert_within(at_far.z, -at_far.w, 0.0001f);
    assert(beyond_far.z < -beyond_far.w);

    Mat4 view_projection = mat4MultiplyM(&view, &projection);
    Vec4f point = {1.5f, -2.0f, -12.0f, 1.0f};
    Vec4f sequential = mat4MultiplyVec4(&point, &view);
    sequential = mat4MultiplyVec4(&sequential, &projection);
    Vec4f composed = mat4MultiplyVec4(&point, &view_projection);
    assert_vector4_close(composed, sequential);

    puts("Pingo vector and matrix math passed");
    return 0;
}
