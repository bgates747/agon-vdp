// Arduino normally supplies forward declarations; a native translation unit
// needs these explicitly. All implementation remains in the owned firmware.
#include <algorithm>
#include <malloc.h> // Before Arduino installs allocation wrapper macros.
#include "Arduino.h"
#include "fabgl.h"
#include "fake_fabgl.h"
#include "vdp.h"

using std::max;

// The tagged Linux shim allocates with ::malloc but does not expose this
// ESP-IDF diagnostic. Report the real host allocation size, not a fake budget.
static size_t heap_caps_get_allocated_size(void *pointer) {
    return malloc_usable_size(pointer);
}

void processLoop(void *parameter);
void copy_font();
void boot_screen();
bool processTerminal();
void printFmt(const char *format, ...);

#include "../video/video.ino"

fabgl::SoundGenerator *getVDPSoundGenerator() {
    return soundGenerator;
}

#ifdef PINGO2_BRIDGE_TEST
/* Synchronous entry into the REAL VDU decoder/registry, without starting Fab,
 * MOS or the background UART worker. Never exported by the review module.
 * A fresh process owns this test adapter, as with the native ABI smoke. */
class PingoTestStream : public Stream {
public:
    const uint8_t *data = nullptr;
    size_t length = 0, position = 0, written = 0;
    uint8_t output[4096] = {};
    int available() override { return length - position; }
    int read() override { return position < length ? data[position++] : -1; }
    int peek() override { return position < length ? data[position] : -1; }
    size_t write(uint8_t b) override {
        if (written >= sizeof output) return 0;
        output[written++] = b;
        return 1;
    }
};
static PingoTestStream *pingoTestStream = nullptr;
static VDUStreamProcessor *pingoTestProcessor = nullptr;
extern "C" void pingo2_test_init() {
    if (pingoTestProcessor) return;
    changeMode(0);
    copy_font();
    pingoTestStream = new PingoTestStream;
    pingoTestProcessor = new VDUStreamProcessor(pingoTestStream);
}
extern "C" size_t pingo2_test_execute(const uint8_t *data, size_t length) {
    pingoTestStream->data = data;
    pingoTestStream->length = length;
    pingoTestStream->position = 0;
    pingoTestProcessor->processAllAvailable();
    return pingoTestStream->position;
}
extern "C" size_t pingo2_test_output(uint8_t *target, size_t capacity) {
    size_t n = pingoTestStream->written;
    if (capacity < n) return n;
    memcpy(target, pingoTestStream->output, n);
    pingoTestStream->written = 0;
    return n;
}
extern "C" size_t pingo2_test_bitmap(uint16_t id, uint8_t *target, size_t size) {
    auto b = getBitmap(id);
    if (!b) return 0;
    size_t count = size_t(b->width) * b->height * (b->format == PixelFormat::RGBA8888 ? 4 : 1);
    if (target && size >= count) memcpy(target, b->data, count);
    return count;
}
extern "C" size_t pingo2_test_depth(uint16_t id, uint32_t *target, size_t count) {
    auto c = pingoControlFind(id);
    if (!c) return 0;
    size_t size = 0;
    auto p = p2cmd_depth(c->owner, id, &size);
    if (target && count >= size) memcpy(target, p, size * sizeof(*p));
    return size;
}
extern "C" int pingo2_test_info(uint16_t id, P2CommandInfo *info) {
    auto c = pingoControlFind(id);
    return c ? p2cmd_info(c->owner, id, info) : P2CMD_MISSING;
}
extern "C" int pingo2_test_sequence(uint16_t id, uint32_t sequence) {
    auto c = pingoControlFind(id);
    return c ? p2cmd_set_sequence(c->owner, id, sequence) : P2CMD_MISSING;
}
extern "C" int pingo2_test_matrices(uint16_t id, uint16_t object, float *out) {
    auto c = pingoControlFind(id);
    return c ? p2cmd_matrices(c->owner, id, object, out) : P2CMD_MISSING;
}
extern "C" unsigned pingo2_test_kinds(uint16_t id) {
    return (pingoIsControl(id) ? 1u : 0u) | (buffers.count(id) ? 2u : 0u) |
        (bitmaps.count(id) ? 4u : 0u) | (fonts.count(id) ? 8u : 0u) |
        (samples.count(id) ? 16u : 0u);
}
#ifdef PINGO2_BRIDGE_DIAGNOSTICS
extern "C" int pingo2_test_status() { return pingoLastStatus; }
extern "C" void pingo2_test_fail(int64_t count) { pingoAllocationFailAfter = count; }
extern "C" size_t pingo2_test_live() { return pingoLiveAllocations; }
extern "C" void pingo2_test_memory(size_t out[3], bool reset) {
    out[0] = pingoLiveBytes; out[1] = pingoPeakBytes; out[2] = pingoLargestAllocation;
    if (reset) { pingoPeakBytes = pingoLiveBytes; pingoLargestAllocation = 0; }
}
#endif
#endif
