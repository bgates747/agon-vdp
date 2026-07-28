CC ?= cc
CXX ?= c++
AR ?= ar

FAB_ROOT_INPUT := $(strip $(FAB_ROOT))
ADAPTER_MAKEFILE := $(lastword $(MAKEFILE_LIST))

ifeq ($(filter clean,$(MAKECMDGOALS)),)
ifeq ($(FAB_ROOT_INPUT),)
$(error Set FAB_ROOT to a Fab Agon Emulator checkout)
endif
endif

VDP_ROOT := $(abspath $(CURDIR)/..)
FAB_CHECKOUT := $(abspath $(FAB_ROOT_INPUT))
FAB_VDP_DIR := $(FAB_CHECKOUT)/src/vdp
VDP_GL_SRC := $(FAB_VDP_DIR)/userspace-vdp-gl/src

BUILD_DIR ?= $(VDP_ROOT)/video/build/userspace
TARGET ?= $(BUILD_DIR)/vdp_pingo.so
SMOKE_TARGET ?= $(BUILD_DIR)/pingo_smoke
TEXTURE_TEST_TARGET ?= $(BUILD_DIR)/pingo_texture_test
VDP_GL_LIB := $(BUILD_DIR)/libvdp-gl.a

ifeq ($(filter clean,$(MAKECMDGOALS)),)
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
	-I$(VDP_GL_SRC)/userspace-platform/matrix

PINGO_CFLAGS := \
	-DUSERSPACE \
	$(COMMON_INCLUDES) \
	-Wall \
	-Wno-error=return-type \
	-O2 \
	-g \
	-fPIC \
	-fdiagnostics-color=always \
	-std=c11 \
	-MMD \
	-MP

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

PINGO_C_SOURCES := \
	math/mat3.c \
	math/mat4.c \
	math/vec2.c \
	math/vec3.c \
	math/vec4.c \
	render/depth.c \
	render/material.c \
	render/mesh.c \
	render/object.c \
	render/pixel.c \
	render/rasterizer.c \
	render/renderable.c \
	render/renderer.c \
	render/scene.c \
	render/sprite.c \
	render/texture.c

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

PINGO_OBJECTS := $(addprefix $(BUILD_DIR)/pingo/,$(PINGO_C_SOURCES:.c=.o))
VDP_GL_OBJECTS := $(addprefix $(BUILD_DIR)/fabgl/,$(VDP_GL_CXX_SOURCES:.cpp=.o))
VDP_OBJECTS := \
	$(BUILD_DIR)/vdp_pingo.o \
	$(BUILD_DIR)/rust_glue.o
OBJECTS := $(VDP_OBJECTS) $(PINGO_OBJECTS)
DEPS := $(OBJECTS:.o=.d) $(VDP_GL_OBJECTS:.o=.d)

.PHONY: all clean smoke texture-test

all: $(TARGET)

$(TARGET): $(OBJECTS) $(VDP_GL_LIB) $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) -shared -Wl,-z,defs $(LDFLAGS) $(OBJECTS) $(VDP_GL_LIB) \
		-pthread -o $@

$(VDP_GL_LIB): $(VDP_GL_OBJECTS) $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(RM) $@
	$(AR) rcs $@ $(VDP_GL_OBJECTS)

$(BUILD_DIR)/vdp_pingo.o: vdp_pingo.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/rust_glue.o: $(FAB_VDP_DIR)/rust_glue.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/pingo/%.o: $(VDP_ROOT)/video/pingo/%.c $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PINGO_CFLAGS) -c $< -o $@

$(BUILD_DIR)/fabgl/%.o: $(VDP_GL_SRC)/%.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VDP_GL_CXXFLAGS) -c $< -o $@

$(SMOKE_TARGET): pingo_smoke.cpp $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) -Wall -Werror -O2 -g -std=c++17 $< -ldl -pthread -o $@

$(TEXTURE_TEST_TARGET): pingo_texture_test.c \
		$(VDP_ROOT)/video/pingo/render/texture.c $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo/render \
		-I$(VDP_ROOT)/video/pingo/math \
		$< $(VDP_ROOT)/video/pingo/render/texture.c -lm -o $@

texture-test: $(TEXTURE_TEST_TARGET)
	$(TEXTURE_TEST_TARGET)

smoke: $(TARGET) $(SMOKE_TARGET) texture-test
	$(SMOKE_TARGET) $(TARGET)

clean:
	$(RM) $(TARGET) $(SMOKE_TARGET) $(TEXTURE_TEST_TARGET) $(VDP_GL_LIB) \
		$(OBJECTS) $(VDP_GL_OBJECTS) $(DEPS)

-include $(DEPS)
