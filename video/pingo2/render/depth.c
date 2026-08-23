#include "depth.h"

void depth_write(PingoDepth *depth, int index, float value) {
    uint32_t candidate;
    if (depth && depth_quantize32(value, &candidate)) {
        depth[index].d = candidate;
    }
}

bool depth_check(PingoDepth *depth, int index, float value) {
    uint32_t candidate;
    return !depth || !depth_quantize32(value, &candidate) ||
        candidate < depth[index].d;
}
