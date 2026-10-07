#pragma once

// The tagged userspace Arduino shim omits yield(), which CRC 1.0.4 uses.
// A using declaration preserves the standard header; do not macro-rewrite it.
#include <thread>
using std::this_thread::yield;
