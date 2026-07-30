#include "depth.h"

#ifdef ZBUFFER32
void depth_write (PingoDepth * d, int idx, float value) {
    uint32_t candidate;
    if (d && depth_quantize32(value, &candidate)) {
        d[idx].d = candidate;
    }
}

bool depth_check(PingoDepth * d, int idx, float value){
    uint32_t candidate;
    return !d ||
        !depth_quantize32(value, &candidate) ||
        candidate < d[idx].d;
}

#endif

#ifdef ZBUFFER16
void depth_write (PingoDepth * d, int idx, float value) {
    d[idx].d = (uint16_t)(value * UINT16_MAX);
}

bool depth_check(PingoDepth * d, int idx, float value){
    return (uint16_t)(value * UINT16_MAX) < d[idx].d;
}
#endif

#ifdef ZBUFFER8
void depth_write (PingoDepth * d, int idx, float value) {
    d[idx].d = (uint8_t)(value * UINT8_MAX);
}

bool depth_check(PingoDepth * d, int idx, float value){
    return (uint8_t)(value * UINT8_MAX) > d[idx].d;
}
#endif
