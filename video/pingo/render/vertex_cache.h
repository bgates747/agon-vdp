#pragma once

#include <stdint.h>
#include "../math/vec4.h"

enum { PINGO_VERTEX_CACHE_SLOTS = 256 };

/* Caller-owned scratch. Zero-initialize before first use; never place on the
 * render task stack. The renderer leases it synchronously for one object and
 * resets tags on every lease. Tags are uint16 source index + 1, so zero is
 * empty and source index 65535 is representable. Values are complete clip
 * coordinates, copied before any clipping or perspective mutation. */
typedef struct tag_PingoVertexCache {
    uint32_t tags[PINGO_VERTEX_CACHE_SLOTS];
    Vec4f clips[PINGO_VERTEX_CACHE_SLOTS];
    uint32_t busy;
} PingoVertexCache;
