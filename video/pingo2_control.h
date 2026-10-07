#ifndef PINGO2_CONTROL_H
#define PINGO2_CONTROL_H

/* Typed Pingo-only IDs share the buffered-command namespace, never the byte
 * storage. Thus no font/audio/bitmap/alias can retain or expose a control.
 * One VDU thread owns this list; no global 'current renderer' is used. */
#ifdef PINGO2_SCENE_BRIDGE
#include "pingo2_commands.h"
#include "types.h"
#include <cstring>
#include <new>
#ifdef USERSPACE
#include <time.h>
#else
#include <esp_timer.h>
#endif

#ifdef PINGO2_BRIDGE_DIAGNOSTICS
static int64_t pingoAllocationFailAfter = -1;
static size_t pingoLiveAllocations = 0;
#endif
extern "C" void *p2bridge_calloc(size_t count, size_t size) {
    if (!count || !size || count > SIZE_MAX / size) return nullptr;
#ifdef PINGO2_BRIDGE_DIAGNOSTICS
    if (pingoAllocationFailAfter == 0) return nullptr;
    if (pingoAllocationFailAfter > 0) --pingoAllocationFailAfter;
#endif
    void *p = PreferPSRAMAlloc(count * size);
    if (p) {
        memset(p, 0, count * size);
#ifdef PINGO2_BRIDGE_DIAGNOSTICS
        ++pingoLiveAllocations;
#endif
    }
    return p;
}
extern "C" void p2bridge_free(void *p) {
    if (!p) return;
#ifdef PINGO2_BRIDGE_DIAGNOSTICS
    --pingoLiveAllocations;
#endif
    free(p);
}
extern "C" uint64_t p2bridge_now_ns(void) {
#ifdef USERSPACE
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return uint64_t(t.tv_sec) * 1000000000u + t.tv_nsec;
#else
    return uint64_t(esp_timer_get_time()) * 1000u;
#endif
}

struct PingoControl {
    PingoControl *next;
    uint16_t id;
    P2Commands *owner;
};
static PingoControl *pingoControls = nullptr;
static PingoControl **pingoControlSlot(uint16_t id) {
    auto p = &pingoControls;
    while (*p && (*p)->id < id) p = &(*p)->next;
    return p;
}
static PingoControl *pingoControlFind(uint16_t id) {
    auto p = *pingoControlSlot(id);
    return p && p->id == id ? p : nullptr;
}
static bool pingoIsControl(uint16_t id) { return pingoControlFind(id) != nullptr; }
static void pingoControlRelease(PingoControl *control) {
    if (!control) return;
    p2cmd_destroy(control->owner);
    control->~PingoControl();
    p2bridge_free(control);
}
static void pingoControlDestroy(uint16_t id) {
    auto slot = pingoControlSlot(id);
    if (*slot && (*slot)->id == id) {
        auto old = *slot;
        *slot = old->next;
        pingoControlRelease(old);
    }
}
static void pingoControlDestroyAll() {
    while (pingoControls) pingoControlDestroy(pingoControls->id);
}
#else
static inline bool pingoIsControl(uint16_t) { return false; }
static inline void pingoControlDestroy(uint16_t) {}
static inline void pingoControlDestroyAll() {}
#endif
#endif
