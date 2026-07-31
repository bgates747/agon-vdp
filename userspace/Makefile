CC ?= cc
CXX ?= c++
AR ?= ar

FAB_ROOT_INPUT := $(strip $(FAB_ROOT))
ADAPTER_MAKEFILE := $(lastword $(MAKEFILE_LIST))
FAB_FREE_GOALS := clean renderer-test
FAB_REQUIRED_GOALS := $(filter-out $(FAB_FREE_GOALS),$(MAKECMDGOALS))
ifeq ($(strip $(MAKECMDGOALS)),)
FAB_REQUIRED_GOALS := all
endif

ifneq ($(strip $(FAB_REQUIRED_GOALS)),)
ifeq ($(FAB_ROOT_INPUT),)
$(error Set FAB_ROOT to a Fab Agon Emulator checkout)
endif
endif

VDP_ROOT := $(abspath $(CURDIR)/..)
FAB_CHECKOUT := $(abspath $(FAB_ROOT_INPUT))
FAB_VDP_DIR := $(FAB_CHECKOUT)/src/vdp
VDP_GL_SRC := $(FAB_VDP_DIR)/userspace-vdp-gl/src

BUILD_DIR ?= $(VDP_ROOT)/video/build/userspace
TARGET ?= $(BUILD_DIR)/vdp_wolf3d.so
SMOKE_TARGET ?= $(BUILD_DIR)/wolf3d_smoke
RENDERER_TEST_TARGET ?= $(BUILD_DIR)/wolf3d_renderer_test
VDP_GL_LIB := $(BUILD_DIR)/libvdp-gl.a

ifneq ($(strip $(FAB_REQUIRED_GOALS)),)
ifeq ($(wildcard $(FAB_VDP_DIR)/rust_glue.cpp),)
$(error FAB_ROOT does not contain src/vdp/rust_glue.cpp)
endif
ifeq ($(wildcard $(VDP_GL_SRC)/Makefile),)
$(error FAB_ROOT does not contain the userspace-vdp-gl submodule)
endif
endif

COMMON_INCLUDES := \
	-I$(FAB_CHECKOUT)/src \
	-I$(FAB_VDP_DIR) \
	-I$(VDP_ROOT)/video \
	-I$(VDP_GL_SRC) \
	-I$(VDP_GL_SRC)/comdrivers \
	-I$(VDP_GL_SRC)/dispdrivers \
	-I$(VDP_GL_SRC)/userspace-platform \
	-I$(VDP_GL_SRC)/userspace-platform/matrix \
	-I$(VDP_ROOT)/.pio/libdeps/esp32dev/CRC/src \
	-I$(VDP_ROOT)/.pio/libdeps/esp32dev/ESP32Time

VDP_CXXFLAGS := \
	-DUSERSPACE \
	$(COMMON_INCLUDES) \
	-Wall \
	-Werror=return-type \
	-O2 \
	-g \
	-fPIC \
	-fdiagnostics-color=always \
	-std=c++17 \
	-MMD \
	-MP

VDP_GL_CXXFLAGS := \
	-DUSERSPACE \
	-DWIFI_TASK_CORE_ID=0 \
	$(COMMON_INCLUDES) \
	-Wall \
	-Werror=return-type \
	-O2 \
	-g \
	-fPIC \
	-fdiagnostics-color=always \
	-std=c++11 \
	-MMD \
	-MP

RENDERER_TEST_CXXFLAGS := \
	-I$(VDP_ROOT) \
	-Wall \
	-Wextra \
	-Werror \
	-O2 \
	-g \
	-std=c++17

VDP_GL_CXX_SOURCES := \
	canvas.cpp \
	codepages.cpp \
	collisiondetector.cpp \
	comdrivers/ps2device.cpp \
	comdrivers/ps2controller.cpp \
	devdrivers/kbdlayouts.cpp \
	devdrivers/keyboard.cpp \
	devdrivers/mouse.cpp \
	devdrivers/soundgen.cpp \
	dispdrivers/vga16controller.cpp \
	dispdrivers/vga2controller.cpp \
	dispdrivers/vga4controller.cpp \
	dispdrivers/vga8controller.cpp \
	dispdrivers/vga64controller.cpp \
	dispdrivers/vgabasecontroller.cpp \
	dispdrivers/vgacontroller.cpp \
	dispdrivers/vgapalettedcontroller.cpp \
	dispdrivers/vgadirectcontroller.cpp \
	displaycontroller.cpp \
	fabfonts.cpp \
	fabutils.cpp \
	terminal.cpp \
	terminfo.cpp \
	userspace-platform/esp32time.cpp \
	userspace-platform/fake_misc.cpp \
	userspace-platform/heap_allocator.cpp \
	userspace-platform/Stream.cpp \
	userspace-platform/HardwareSerial.cpp \
	userspace-platform/matrix/mat.cpp

VDP_GL_OBJECTS := $(addprefix $(BUILD_DIR)/fabgl/,$(VDP_GL_CXX_SOURCES:.cpp=.o))
VDP_OBJECTS := \
	$(BUILD_DIR)/vdp_wolf3d.o \
	$(BUILD_DIR)/rust_glue.o \
	$(BUILD_DIR)/CRC16.o \
	$(BUILD_DIR)/CRC32.o \
	$(BUILD_DIR)/CrcFastReverse.o
OBJECTS := $(VDP_OBJECTS)
DEPS := $(OBJECTS:.o=.d) $(VDP_GL_OBJECTS:.o=.d)

.PHONY: all clean smoke renderer-test

all: $(TARGET)

$(TARGET): $(OBJECTS) $(VDP_GL_LIB) $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) -shared -Wl,-z,defs $(LDFLAGS) $(OBJECTS) $(VDP_GL_LIB) \
		-pthread -o $@

$(VDP_GL_LIB): $(VDP_GL_OBJECTS) $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(RM) $@
	$(AR) rcs $@ $(VDP_GL_OBJECTS)

$(BUILD_DIR)/vdp_wolf3d.o: vdp_wolf3d.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/rust_glue.o: $(FAB_VDP_DIR)/rust_glue.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/CRC16.o: $(VDP_ROOT)/.pio/libdeps/esp32dev/CRC/src/CRC16.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_CXXFLAGS) -include $(CURDIR)/arduino_yield_shim.h -c $< -o $@

$(BUILD_DIR)/CRC32.o: $(VDP_ROOT)/.pio/libdeps/esp32dev/CRC/src/CRC32.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_CXXFLAGS) -include $(CURDIR)/arduino_yield_shim.h -c $< -o $@

$(BUILD_DIR)/CrcFastReverse.o: $(VDP_ROOT)/.pio/libdeps/esp32dev/CRC/src/CrcFastReverse.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/fabgl/%.o: $(VDP_GL_SRC)/%.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_GL_CXXFLAGS) -c $< -o $@

$(SMOKE_TARGET): wolf3d_smoke.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) -Wall -Werror -O2 -g -std=c++17 $< -ldl -pthread -o $@

smoke: $(TARGET) $(SMOKE_TARGET)
	$(SMOKE_TARGET) $(TARGET)

$(RENDERER_TEST_TARGET): wolf3d_renderer_test.cpp \
		$(VDP_ROOT)/video/wolf3d/render/wolf3d_draw.h \
		$(VDP_ROOT)/video/wolf3d/wolf3d_world.h \
		$(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(RENDERER_TEST_CXXFLAGS) $< -o $@

renderer-test: $(RENDERER_TEST_TARGET)
	$(RENDERER_TEST_TARGET)

clean:
	$(RM) $(TARGET) $(SMOKE_TARGET) $(RENDERER_TEST_TARGET) $(VDP_GL_LIB) $(OBJECTS) $(VDP_GL_OBJECTS) $(DEPS)

-include $(DEPS)
