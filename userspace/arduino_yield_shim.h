// Minimal compatibility shim for the global Arduino `yield()` function,
// used by the vendored CRC16.cpp/CRC32.cpp (PlatformIO lib_deps: "CRC")
// but not provided by Fab's userspace-vdp-gl Arduino.h shim.
#pragma once
#include <thread>

inline void yield() {
	std::this_thread::yield();
}
