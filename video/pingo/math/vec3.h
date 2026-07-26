#pragma once



#ifdef __cplusplus
extern "C" {
#endif

typedef struct Vec3i {
    int x;
    int y;
    int z;
} Vec3i;

typedef struct Vec3f {
    float x;
    float y;
    float z;
} Vec3f;

Vec3f vec3f(float,float,float);
Vec3f vec3fmul(Vec3f,float);
Vec3f vec3fsumV(Vec3f,Vec3f);
Vec3f vec3fsubV(Vec3f,Vec3f);
Vec3f vec3fsum(Vec3f,float);
float vec3Dot(Vec3f,Vec3f);
Vec3f vec3Cross(Vec3f,Vec3f);
Vec3f vec3Normalize(Vec3f);

#ifdef __cplusplus
}
#endif
