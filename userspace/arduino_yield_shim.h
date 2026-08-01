// Minimal compatibility shim for the global Arduino `yield()` function,
// used by the PlatformIO CRC sources but absent from Fab's userspace Arduino
// compatibility layer.
#pragma once

#include <thread>

inline void yield() {
	std::this_thread::yield();
}
