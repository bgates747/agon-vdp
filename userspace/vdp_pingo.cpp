#include "Arduino.h"
#include "vdp-console8.h"

fabgl::SoundGenerator *getVDPSoundGenerator() {
	return &*soundGenerator;
}

#include "../video/video.ino"
