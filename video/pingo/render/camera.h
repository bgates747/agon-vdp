#pragma once

#include "../math/mat4.h"

typedef struct Camera {
    float near;
    float far;
    float aspect;
    float fov;
    Vec2i translation;
    Vec2i rect;
    Mat4 projection;
    Mat4 view;
} Camera;