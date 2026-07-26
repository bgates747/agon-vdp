#include <cstddef>

#include "fake_fabgl.h"

extern "C" void *pingo_platform_alloc(std::size_t size) {
	return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

extern "C" void pingo_platform_free(void *ptr) {
	heap_caps_free(ptr);
}
