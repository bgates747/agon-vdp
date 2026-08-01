#include "Arduino.h"
#include "vdp-console8.h"

// The userspace adapter must include the firmware translation unit exactly once.
// Pingo and Wolf3DOrig are both dispatched by that shared VDP instance.
fabgl::SoundGenerator *getVDPSoundGenerator() {
	return &*soundGenerator;
}

#include "../video/video.ino"
