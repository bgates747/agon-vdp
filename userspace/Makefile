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

DIAGNOSTICS ?= 0
ifeq ($(DIAGNOSTICS),0)
DEFAULT_BUILD_DIR := $(VDP_ROOT)/video/build/userspace
DIAGNOSTIC_FLAGS :=
else
DEFAULT_BUILD_DIR := $(VDP_ROOT)/video/build/userspace-diagnostics
DIAGNOSTIC_FLAGS := -DPINGO_RENDER_DIAGNOSTICS=$(DIAGNOSTICS)
endif

BUILD_DIR ?= $(DEFAULT_BUILD_DIR)
TARGET ?= $(BUILD_DIR)/vdp_pingo.so
SMOKE_TARGET ?= $(BUILD_DIR)/pingo_smoke
BRIDGE_ROBUSTNESS_TEST_TARGET ?= $(BUILD_DIR)/pingo_bridge_robustness_test
TEXTURE_TEST_TARGET ?= $(BUILD_DIR)/pingo_texture_test
MATH_TEST_TARGET ?= $(BUILD_DIR)/pingo_math_test
SPAN_TEST_TARGET ?= $(BUILD_DIR)/pingo_triangle_span_test
PERSPECTIVE_SPAN_TEST_TARGET ?= $(BUILD_DIR)/pingo_perspective_span_test
DEPTH_TEST_TARGET ?= $(BUILD_DIR)/pingo_depth_test
MESH_VALIDATION_TEST_TARGET ?= $(BUILD_DIR)/pingo_mesh_validation_test
CLIP_TEST_TARGET ?= $(BUILD_DIR)/pingo_clip_test
DIAGNOSTICS_TEST_TARGET ?= $(BUILD_DIR)/pingo_renderer_diagnostics_test
SANITIZER_TEST_TARGET ?= $(BUILD_DIR)/pingo_renderer_sanitizer_test
BRIDGE_SANITIZER_BUILD_DIR := $(VDP_ROOT)/video/build/userspace-bridge-sanitizer
BRIDGE_SANITIZER_FLAGS := \
	-fno-omit-frame-pointer \
	-fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow
BRIDGE_UBSAN_SUPPRESSIONS := $(abspath $(CURDIR)/bridge_ubsan.supp)
BRIDGE_TEST_TIMEOUT ?= 30s
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
	$(DIAGNOSTIC_FLAGS) \
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
	$(DIAGNOSTIC_FLAGS) \
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
PINGO_C_SOURCE_PATHS := \
	$(addprefix $(VDP_ROOT)/video/pingo/,$(PINGO_C_SOURCES))

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

.PHONY: all clean smoke texture-test math-test span-test \
	perspective-span-test depth-test mesh-validation-test \
	clip-test diagnostics-test diagnostics-smoke sanitizer-test \
	bridge-sanitizer-test

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
	$(CXX) $(CXXFLAGS) -Wall -Werror -O2 -g -std=c++17 \
		$< $(LDFLAGS) -ldl -pthread -o $@

$(BRIDGE_ROBUSTNESS_TEST_TARGET): pingo_bridge_robustness_test.cpp \
		$(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Wall -Werror -O2 -g -std=c++17 \
		$< $(LDFLAGS) -ldl -pthread -o $@

$(TEXTURE_TEST_TARGET): pingo_texture_test.c \
		$(VDP_ROOT)/video/pingo/render/pixel.c \
		$(VDP_ROOT)/video/pingo/render/texture.c $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo/render \
		-I$(VDP_ROOT)/video/pingo/math \
		$< \
		$(VDP_ROOT)/video/pingo/render/pixel.c \
		$(VDP_ROOT)/video/pingo/render/texture.c -lm -o $@

$(MATH_TEST_TARGET): pingo_math_test.c \
		$(VDP_ROOT)/video/pingo/math/mat4.c \
		$(VDP_ROOT)/video/pingo/math/vec3.c $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo/math \
		$< \
		$(VDP_ROOT)/video/pingo/math/mat4.c \
		$(VDP_ROOT)/video/pingo/math/vec3.c -lm -o $@

$(SPAN_TEST_TARGET): pingo_triangle_span_test.c \
		$(VDP_ROOT)/video/pingo/render/triangle_span.h $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo \
		$< -o $@

$(PERSPECTIVE_SPAN_TEST_TARGET): pingo_perspective_span_test.c \
		$(VDP_ROOT)/video/pingo/render/perspective_span.h $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo \
		$< -lm -o $@

$(DEPTH_TEST_TARGET): pingo_depth_test.c \
		$(VDP_ROOT)/video/pingo/render/depth.c \
		$(VDP_ROOT)/video/pingo/render/depth.h $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo/render \
		$< $(VDP_ROOT)/video/pingo/render/depth.c -lm -o $@

$(MESH_VALIDATION_TEST_TARGET): pingo_mesh_validation_test.c \
		$(VDP_ROOT)/video/pingo/render/mesh.c \
		$(VDP_ROOT)/video/pingo/render/object.c \
		$(VDP_ROOT)/video/pingo/render/mesh.h \
		$(VDP_ROOT)/video/pingo/render/object.h $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo/render \
		-I$(VDP_ROOT)/video/pingo/math \
		$< \
		$(VDP_ROOT)/video/pingo/render/mesh.c \
		$(VDP_ROOT)/video/pingo/render/object.c -lm -o $@

$(CLIP_TEST_TARGET): pingo_clip_test.c \
		$(VDP_ROOT)/video/pingo/render/clip.h $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -O2 -g -std=c11 \
		-I$(VDP_ROOT)/video/pingo \
		$< -lm -o $@

$(DIAGNOSTICS_TEST_TARGET): pingo_renderer_diagnostics_test.c \
		$(PINGO_OBJECTS) $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) $(DIAGNOSTIC_FLAGS) -Wall -Werror -O2 -g -std=c11 \
		$(COMMON_INCLUDES) $< $(PINGO_OBJECTS) -lm -o $@

$(SANITIZER_TEST_TARGET): pingo_renderer_diagnostics_test.c \
		$(PINGO_C_SOURCE_PATHS) $(ADAPTER_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CC) -DPINGO_RENDER_DIAGNOSTICS=1 \
		$(COMMON_INCLUDES) \
		-Wall -Werror=return-type -O1 -g -std=c11 \
		-fno-omit-frame-pointer \
		-fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow \
		$< $(PINGO_C_SOURCE_PATHS) -lm -o $@

texture-test: $(TEXTURE_TEST_TARGET)
	$(TEXTURE_TEST_TARGET)

math-test: $(MATH_TEST_TARGET)
	$(MATH_TEST_TARGET)

span-test: $(SPAN_TEST_TARGET)
	$(SPAN_TEST_TARGET)

perspective-span-test: $(PERSPECTIVE_SPAN_TEST_TARGET)
	$(PERSPECTIVE_SPAN_TEST_TARGET)

depth-test: $(DEPTH_TEST_TARGET)
	$(DEPTH_TEST_TARGET)

mesh-validation-test: $(MESH_VALIDATION_TEST_TARGET)
	$(MESH_VALIDATION_TEST_TARGET)

clip-test: $(CLIP_TEST_TARGET)
	$(CLIP_TEST_TARGET)

smoke: $(TARGET) $(SMOKE_TARGET) $(BRIDGE_ROBUSTNESS_TEST_TARGET) \
		texture-test math-test span-test \
		perspective-span-test depth-test mesh-validation-test clip-test
	$(SMOKE_TARGET) $(TARGET)
	timeout $(BRIDGE_TEST_TIMEOUT) \
		$(BRIDGE_ROBUSTNESS_TEST_TARGET) $(TARGET)

ifeq ($(DIAGNOSTICS),0)
diagnostics-test:
	@echo "diagnostics-test requires DIAGNOSTICS=1" >&2
	@false
else
diagnostics-test: $(DIAGNOSTICS_TEST_TARGET)
	$(DIAGNOSTICS_TEST_TARGET)
endif

diagnostics-smoke: smoke diagnostics-test

sanitizer-test: $(SANITIZER_TEST_TARGET)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
	$(SANITIZER_TEST_TARGET)

bridge-sanitizer-test:
	$(MAKE) \
		BUILD_DIR=$(BRIDGE_SANITIZER_BUILD_DIR) \
		CFLAGS="$(BRIDGE_SANITIZER_FLAGS)" \
		CXXFLAGS="$(BRIDGE_SANITIZER_FLAGS)" \
		LDFLAGS="$(BRIDGE_SANITIZER_FLAGS)" \
		$(BRIDGE_SANITIZER_BUILD_DIR)/vdp_pingo.so \
		$(BRIDGE_SANITIZER_BUILD_DIR)/pingo_bridge_robustness_test
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:alloc_dealloc_mismatch=0 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:suppressions=$(BRIDGE_UBSAN_SUPPRESSIONS) \
	timeout $(BRIDGE_TEST_TIMEOUT) \
		$(BRIDGE_SANITIZER_BUILD_DIR)/pingo_bridge_robustness_test \
		$(BRIDGE_SANITIZER_BUILD_DIR)/vdp_pingo.so

clean:
	$(RM) $(TARGET) $(SMOKE_TARGET) $(TEXTURE_TEST_TARGET) $(MATH_TEST_TARGET) \
		$(BRIDGE_ROBUSTNESS_TEST_TARGET) \
		$(SPAN_TEST_TARGET) \
		$(PERSPECTIVE_SPAN_TEST_TARGET) \
		$(DEPTH_TEST_TARGET) \
		$(MESH_VALIDATION_TEST_TARGET) \
		$(CLIP_TEST_TARGET) \
		$(DIAGNOSTICS_TEST_TARGET) \
		$(SANITIZER_TEST_TARGET) $(VDP_GL_LIB) \
		$(OBJECTS) $(VDP_GL_OBJECTS) $(DEPS)

-include $(DEPS)
