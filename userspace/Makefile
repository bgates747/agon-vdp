# Native Pingo 2 only: never build or write inside the official Fab checkout.
# PlatformIO may generate video.ino.cpp beside video.ino. Never let Make's
# implicit executable rules try to rebuild an included firmware source file.
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
ifeq ($(strip $(FAB_ROOT)),)
$(error supply FAB_ROOT pointing to official Fab tag 1.2.5)
endif
ifeq ($(strip $(PYTHON)),)
$(error supply PYTHON pointing to the consuming project's virtualenv Python)
endif
ROOT := $(abspath ..)
FAB_ROOT := $(abspath $(FAB_ROOT))
GL := $(FAB_ROOT)/src/vdp/userspace-vdp-gl/src
CRC_ROOT ?= $(ROOT)/.pio/libdeps/esp32dev/CRC
BUILD := $(ROOT)/video/build/userspace/pingo2
BRIDGE_DIAGNOSTICS ?= 0
BRIDGE_TEST ?= 0
ifeq ($(BRIDGE_DIAGNOSTICS),1)
BUILD := $(ROOT)/video/build/userspace/pingo2-diagnostics
endif
ifeq ($(BRIDGE_TEST),1)
BUILD := $(BUILD)-test
endif
MODULE := $(BUILD)/vdp_pingo2.so
CPPFLAGS := -DUSERSPACE -DWIFI_TASK_CORE_ID=0 -DPINGO2_TARGET_PROBE=1 -DP2C_PIXEL_RGBA2222=1 \
 -I$(GL) -I$(GL)/dispdrivers -I$(GL)/comdrivers -I$(GL)/userspace-platform \
 -I$(GL)/userspace-platform/matrix -I$(FAB_ROOT)/src/vdp -I$(CRC_ROOT)/src \
 -I$(ROOT)/video -I$(ROOT)/video/pingo2
CPPFLAGS += -DPINGO2_SCENE_BRIDGE=1
ifeq ($(BRIDGE_TEST),1)
CPPFLAGS += -DPINGO2_BRIDGE_TEST=1
endif
ifeq ($(BRIDGE_DIAGNOSTICS),1)
CPPFLAGS += -DPINGO2_BRIDGE_DIAGNOSTICS=1
endif
CFLAGS := -std=c11 -O2 -g -fPIC -Wall -Werror=return-type -MMD -MP
CXXFLAGS := -std=c++17 -O2 -g -fPIC -Wall -Werror=return-type -MMD -MP -pthread
# Match the tagged userspace-vdp-gl source list. No stock Console8 firmware,
# Pingo 1 bridge, combined registry, or Wolf source is compiled.
GL_SOURCES := canvas.cpp codepages.cpp collisiondetector.cpp \
 comdrivers/ps2device.cpp comdrivers/ps2controller.cpp \
 devdrivers/kbdlayouts.cpp devdrivers/keyboard.cpp devdrivers/mouse.cpp devdrivers/soundgen.cpp \
 dispdrivers/vga16controller.cpp dispdrivers/vga2controller.cpp dispdrivers/vga4controller.cpp \
 dispdrivers/vga8controller.cpp dispdrivers/vga64controller.cpp dispdrivers/vgabasecontroller.cpp \
 dispdrivers/vgacontroller.cpp dispdrivers/vgapalettedcontroller.cpp dispdrivers/vgadirectcontroller.cpp \
 displaycontroller.cpp fabfonts.cpp fabutils.cpp terminal.cpp terminfo.cpp \
 userspace-platform/esp32time.cpp userspace-platform/fake_misc.cpp userspace-platform/heap_allocator.cpp \
 userspace-platform/Stream.cpp userspace-platform/HardwareSerial.cpp userspace-platform/matrix/mat.cpp
ENGINE_SOURCES := math/mat3.c math/mat4.c math/vec2.c math/vec3.c math/vec4.c \
 render/array.c render/depth.c render/entity.c render/material.c render/mesh.c render/object.c \
 render/pixel.c render/rasterizer.c render/renderer.c render/sprite.c render/texture.c
CRC_SOURCES := CRC16.cpp CRC32.cpp CrcFastReverse.cpp
OBJECTS := $(BUILD)/pingo2.o $(BUILD)/rust_glue.o $(BUILD)/pingo2_target_probe.o \
 $(GL_SOURCES:%.cpp=$(BUILD)/gl/%.o) $(ENGINE_SOURCES:%.c=$(BUILD)/engine/%.o) \
 $(CRC_SOURCES:%.cpp=$(BUILD)/crc/%.o)
OBJECTS += $(BUILD)/pingo2_commands.o

.PHONY: all verify smoke FORCE
all: verify $(MODULE)
verify:
	$(PYTHON) smoke.py --verify-inputs --fab-root "$(FAB_ROOT)" --crc-root "$(CRC_ROOT)"

# Invalidate objects when invocation/compiler/flags change, without touching
# upstream sources or trusting migrated x86-64 products.
$(BUILD)/config: FORCE
	@mkdir -p "$(@D)"
	@{ printf '%s\n' '$(CC) $(CFLAGS)' '$(CXX) $(CXXFLAGS)' '$(CPPFLAGS)'; $(CC) --version; $(CXX) --version; } > "$@.tmp"
	@cmp -s "$@.tmp" "$@" || cp "$@.tmp" "$@"
	@rm "$@.tmp"
$(OBJECTS): $(BUILD)/config | verify
$(BUILD)/pingo2.o: pingo2.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/rust_glue.o: $(FAB_ROOT)/src/vdp/rust_glue.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/pingo2_target_probe.o: $(ROOT)/video/pingo2_target_probe.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/pingo2_commands.o: $(ROOT)/video/pingo2_commands.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/gl/%.o: $(GL)/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/engine/%.o: $(ROOT)/video/pingo2/%.c Makefile
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/crc/%.o: $(CRC_ROOT)/src/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -include arduino_yield.h -c $< -o $@
$(MODULE): $(OBJECTS)
	$(CXX) -shared -Wl,-z,defs -pthread -o $@ $(OBJECTS) -lm
smoke: all
	$(PYTHON) smoke.py --module "$(MODULE)" --fab-root "$(FAB_ROOT)" --crc-root "$(CRC_ROOT)"
-include $(OBJECTS:.o=.d)
