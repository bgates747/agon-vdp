#include "vec3.h"
#include <math.h>

Vec3f vec3fmul(Vec3f a, float b)
{
    a.x = a.x * b;
    a.y = a.y * b;
    a.z = a.z * b;

    return a;
}

Vec3f vec3fsumV(Vec3f a, Vec3f b)
{
    a.x = a.x + b.x;
    a.y = a.y + b.y;
    a.z = a.z + b.z;

    return a;
}

Vec3f vec3fsubV(Vec3f a, Vec3f b)
{
    a.x = a.x - b.x;
    a.y = a.y - b.y;
    a.z = a.z - b.z;

    return a;
}

Vec3f vec3fsum(Vec3f a, float b)
{
    a.x = a.x + b;
    a.y = a.y + b;
    a.z = a.z + b;

    return a;
}

float vec3Dot(Vec3f a, Vec3f b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3f vec3f(float x, float y, float z)
{
    return (Vec3f){x,y,z};
}

Vec3f vec3Cross(Vec3f a, Vec3f b)
{

    return (Vec3f) {a.y * b.z - b.y * a.z,
                    a.z * b.x - b.z * a.x,
                    a.x * b.y - b.x * a.y};
}

Vec3f vec3Normalize(Vec3f v)
{
    /*
     * Adapted from upstream Pingo fb67d951: keep the zero/unit fast paths
     * and replace three floating-point divisions with one reciprocal and
     * three multiplies.
     */
    float length_squared = v.x * v.x + v.y * v.y + v.z * v.z;

    if (length_squared == 0.0f) {
        return (Vec3f){0.0f, 0.0f, 0.0f};
    }

    if (length_squared == 1.0f) {
        return v;
    }

    float inverse_length = 1.0f / sqrtf(length_squared);
    return (Vec3f){
        v.x * inverse_length,
        v.y * inverse_length,
        v.z * inverse_length
    };
}
