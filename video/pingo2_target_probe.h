#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Pingo2TargetProbeResult {
    int render_status;
    uint32_t drawn_pixels;
    uint32_t depth_pixels;
    uint32_t framebuffer_fnv1a;
    uint32_t depth_fnv1a;
} Pingo2TargetProbeResult;

int pingo2_target_probe_run(Pingo2TargetProbeResult *result);

#ifdef __cplusplus
}
#endif
