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
