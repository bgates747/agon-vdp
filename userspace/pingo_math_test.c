#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../video/pingo/math/mat4.h"
#include "../video/pingo/math/vec3.h"

static void assert_close(float actual, float expected)
{
    assert(fabsf(actual - expected) <= 0.000001f);
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
    Mat4 view_projection = mat4MultiplyM(&view, &projection);
    Vec4f point = {1.5f, -2.0f, -12.0f, 1.0f};
    Vec4f sequential = mat4MultiplyVec4(&point, &view);
    sequential = mat4MultiplyVec4(&sequential, &projection);
    Vec4f composed = mat4MultiplyVec4(&point, &view_projection);
    assert_vector4_close(composed, sequential);

    puts("Pingo vector and matrix math passed");
    return 0;
}
