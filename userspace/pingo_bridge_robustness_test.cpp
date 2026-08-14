#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <thread>
#include <vector>

namespace {

template<typename T>
T loadSymbol(void * handle, const char * name) {
    dlerror();
    auto symbol = reinterpret_cast<T>(dlsym(handle, name));
    if (const char * error = dlerror()) {
        std::fprintf(stderr, "missing ABI symbol %s: %s\n", name, error);
        std::exit(2);
    }
    return symbol;
}

bool approximately(float actual, float expected) {
    return std::fabs(actual - expected) < 0.0001f;
}

bool approximatelyWide(float actual, float expected) {
    return std::fabs(actual - expected) < 0.02f;
}

struct Harness {
    void * handle;
    void (*send)(std::uint8_t);
    bool (*receive)(std::uint8_t *);
    bool (*isCts)();
    void (*setup)();
    void (*shutdown)();
    void (*failAllocationAfter)(std::int32_t);
    std::uint32_t (*ownedAllocations)();
    bool (*controlExists)(std::uint16_t);
    std::uint32_t (*controlSize)();
    bool (*projectionFar)(std::uint16_t, std::uint16_t *);
    bool (*objectScale)(std::uint16_t, std::uint16_t, float *);
    bool (*objectTranslation)(
        std::uint16_t, std::uint16_t, float *, float *, std::uint8_t *);
    bool (*objectActive)(
        std::uint16_t, std::uint16_t, std::uint8_t *);
    bool (*objectUsesMesh)(
        std::uint16_t, std::uint16_t, std::uint16_t);
    bool (*meshStreamState)(
        std::uint16_t, std::uint16_t,
        std::uint32_t *, std::uint32_t *, std::uint32_t *,
        std::uint32_t *, std::uint8_t *, std::uint8_t *);
    bool (*meshStreamVertex)(
        std::uint16_t, std::uint16_t, std::uint32_t, float *);
    bool (*meshStreamUv)(
        std::uint16_t, std::uint16_t, std::uint32_t, float *);
    bool (*meshStreamIndices)(
        std::uint16_t, std::uint16_t, std::uint32_t,
        std::uint16_t *, std::uint16_t *);
    bool (*sceneScale)(std::uint16_t, float *);
    bool (*lightingState)(
        std::uint16_t, float *, std::uint8_t *,
        std::uint8_t *, std::uint8_t *);
    bool (*meshShadingMode)(
        std::uint16_t, std::uint16_t, std::uint8_t *);
    bool (*meshIlluminationPolicy)(
        std::uint16_t, std::uint16_t, std::uint8_t *);
    bool (*flatPatternState)(
        std::uint16_t, std::uint16_t *, std::uint8_t *,
        std::uint8_t *, std::uint8_t *, std::uint8_t *);
    bool (*uploadHash)(
        std::uint16_t, std::uint16_t, std::uint16_t, std::uint64_t *);
    bool (*texturePixel)(
        std::uint16_t, std::uint16_t, std::uint32_t, std::uint8_t *);
    bool (*bitmapPixel)(std::uint16_t, std::uint32_t, std::uint8_t *);
    std::uint8_t nextEcho;

    explicit Harness(const char * library)
        : handle(dlopen(library, RTLD_NOW | RTLD_LOCAL)),
          send(nullptr),
          receive(nullptr),
          isCts(nullptr),
          setup(nullptr),
          shutdown(nullptr),
          failAllocationAfter(nullptr),
          ownedAllocations(nullptr),
          controlExists(nullptr),
          controlSize(nullptr),
          projectionFar(nullptr),
          objectScale(nullptr),
          objectTranslation(nullptr),
          objectActive(nullptr),
          objectUsesMesh(nullptr),
          meshStreamState(nullptr),
          meshStreamVertex(nullptr),
          meshStreamUv(nullptr),
          meshStreamIndices(nullptr),
          sceneScale(nullptr),
          lightingState(nullptr),
          meshShadingMode(nullptr),
          meshIlluminationPolicy(nullptr),
          flatPatternState(nullptr),
          uploadHash(nullptr),
          texturePixel(nullptr),
          bitmapPixel(nullptr),
          nextEcho(0x40) {
        if (!handle) {
            std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
            std::exit(2);
        }
        send = loadSymbol<void (*)(std::uint8_t)>(
            handle, "z80_send_to_vdp");
        receive = loadSymbol<bool (*)(std::uint8_t *)>(
            handle, "z80_recv_from_vdp");
        isCts = loadSymbol<bool (*)()>(handle, "z80_uart0_is_cts");
        setup = loadSymbol<void (*)()>(handle, "vdp_setup");
        shutdown = loadSymbol<void (*)()>(handle, "vdp_shutdown");
        failAllocationAfter = loadSymbol<void (*)(std::int32_t)>(
            handle, "pingo_userspace_fail_allocation_after");
        ownedAllocations = loadSymbol<std::uint32_t (*)()>(
            handle, "pingo_userspace_get_owned_allocation_count");
        controlExists = loadSymbol<bool (*)(std::uint16_t)>(
            handle, "pingo_userspace_control_exists");
        controlSize = loadSymbol<std::uint32_t (*)()>(
            handle, "pingo_userspace_control_size");
        projectionFar = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t *)>(
            handle, "pingo_userspace_get_projection_far_units");
        objectScale = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, float *)>(
            handle, "pingo_userspace_get_object_scale");
        objectTranslation = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t,
            float *, float *, std::uint8_t *)>(
            handle, "pingo_userspace_get_object_translation");
        objectActive = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint8_t *)>(
            handle, "pingo_userspace_get_object_active");
        objectUsesMesh = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint16_t)>(
            handle, "pingo_userspace_object_uses_mesh");
        meshStreamState = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t,
            std::uint32_t *, std::uint32_t *, std::uint32_t *,
            std::uint32_t *, std::uint8_t *, std::uint8_t *)>(
            handle, "pingo_userspace_get_mesh_stream_state");
        meshStreamVertex = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint32_t, float *)>(
            handle, "pingo_userspace_get_mesh_stream_vertex");
        meshStreamUv = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint32_t, float *)>(
            handle, "pingo_userspace_get_mesh_stream_uv");
        meshStreamIndices = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint32_t,
            std::uint16_t *, std::uint16_t *)>(
            handle, "pingo_userspace_get_mesh_stream_indices");
        sceneScale = loadSymbol<bool (*)(std::uint16_t, float *)>(
            handle, "pingo_userspace_get_scene_scale");
        lightingState = loadSymbol<bool (*)(
            std::uint16_t, float *, std::uint8_t *,
            std::uint8_t *, std::uint8_t *)>(
            handle, "pingo_userspace_get_lighting_state");
        meshShadingMode = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint8_t *)>(
            handle, "pingo_userspace_get_mesh_shading_mode");
        meshIlluminationPolicy = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint8_t *)>(
            handle, "pingo_userspace_get_mesh_illumination_policy");
        flatPatternState = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t *, std::uint8_t *,
            std::uint8_t *, std::uint8_t *, std::uint8_t *)>(
            handle, "pingo_userspace_get_flat_pattern_state");
        uploadHash = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint16_t, std::uint64_t *)>(
            handle, "pingo_userspace_get_upload_state_hash");
        texturePixel = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint32_t, std::uint8_t *)>(
            handle, "pingo_userspace_get_object_texture_pixel");
        bitmapPixel = loadSymbol<bool (*)(
            std::uint16_t, std::uint32_t, std::uint8_t *)>(
            handle, "pingo_userspace_get_bitmap2222_pixel");
    }

    void sendBytes(const std::vector<std::uint8_t>& bytes) {
        for (auto byte : bytes) {
            while (!isCts()) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(50));
            }
            send(byte);
        }
    }

    static void appendWord(
            std::vector<std::uint8_t>& bytes, std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    static void append24(
            std::vector<std::uint8_t>& bytes, std::uint32_t value) {
        value &= 0xFFFFFFU;
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    }

    std::vector<std::uint8_t> pingoPrefix(
            std::uint16_t control, std::uint8_t subcommand) {
        return {
            23, 0, 0xA0,
            static_cast<std::uint8_t>(control),
            static_cast<std::uint8_t>(control >> 8),
            0x49, subcommand,
        };
    }

    void sendPingo(
            std::uint16_t control,
            std::uint8_t subcommand,
            const std::vector<std::uint16_t>& words = {}) {
        auto bytes = pingoPrefix(control, subcommand);
        for (auto word : words) {
            appendWord(bytes, word);
        }
        sendBytes(bytes);
    }

    void sendPingoBytes(
            std::uint16_t control,
            std::uint8_t subcommand,
            const std::vector<std::uint8_t>& payload) {
        auto bytes = pingoPrefix(control, subcommand);
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        sendBytes(bytes);
    }

    void clearBuffer(std::uint16_t buffer) {
        sendBytes({
            23, 0, 0xA0,
            static_cast<std::uint8_t>(buffer),
            static_cast<std::uint8_t>(buffer >> 8),
            2,
        });
    }

    void appendBuffer(
            std::uint16_t buffer, const std::vector<std::uint8_t>& payload) {
        std::vector<std::uint8_t> bytes = {
            23, 0, 0xA0,
            static_cast<std::uint8_t>(buffer),
            static_cast<std::uint8_t>(buffer >> 8),
            0,
        };
        appendWord(bytes, static_cast<std::uint16_t>(payload.size()));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        sendBytes(bytes);
    }

    void copyBuffer(std::uint16_t target, std::uint16_t source) {
        std::vector<std::uint8_t> bytes = {
            23, 0, 0xA0,
            static_cast<std::uint8_t>(target),
            static_cast<std::uint8_t>(target >> 8),
            0x0D,
        };
        appendWord(bytes, source);
        appendWord(bytes, 0xFFFF);
        sendBytes(bytes);
    }

    void copyBufferReference(
            std::uint16_t target, std::uint16_t source) {
        std::vector<std::uint8_t> bytes = {
            23, 0, 0xA0,
            static_cast<std::uint8_t>(target),
            static_cast<std::uint8_t>(target >> 8),
            0x19,
        };
        appendWord(bytes, source);
        appendWord(bytes, 0xFFFF);
        sendBytes(bytes);
    }

    void spreadBufferInto(
            std::uint16_t source, std::uint16_t target) {
        std::vector<std::uint8_t> bytes = {
            23, 0, 0xA0,
            static_cast<std::uint8_t>(source),
            static_cast<std::uint8_t>(source >> 8),
            0x15,
        };
        appendWord(bytes, target);
        appendWord(bytes, 0xFFFF);
        sendBytes(bytes);
    }

    void reverseBuffer(std::uint16_t buffer) {
        sendBytes({
            23, 0, 0xA0,
            static_cast<std::uint8_t>(buffer),
            static_cast<std::uint8_t>(buffer >> 8),
            0x18, 0,
        });
    }

    void callBuffer(std::uint16_t buffer) {
        sendBytes({
            23, 0, 0xA0,
            static_cast<std::uint8_t>(buffer),
            static_cast<std::uint8_t>(buffer >> 8),
            1,
        });
    }

    void jumpToBuffer(std::uint16_t buffer) {
        sendBytes({
            23, 0, 0xA0,
            static_cast<std::uint8_t>(buffer),
            static_cast<std::uint8_t>(buffer >> 8),
            7,
        });
    }

    void createBitmapFromBuffer(
            std::uint16_t bitmap, std::uint16_t width,
            std::uint16_t height) {
        sendBytes({
            23, 27, 0x20,
            static_cast<std::uint8_t>(bitmap),
            static_cast<std::uint8_t>(bitmap >> 8),
            23, 27, 0x21,
            static_cast<std::uint8_t>(width),
            static_cast<std::uint8_t>(width >> 8),
            static_cast<std::uint8_t>(height),
            static_cast<std::uint8_t>(height >> 8),
            1,
        });
    }

    void createBitmap2222(
            std::uint16_t bitmap, std::uint16_t width,
            std::uint16_t height, std::uint8_t color) {
        sendBytes({
            23, 27, 0x20,
            static_cast<std::uint8_t>(bitmap),
            static_cast<std::uint8_t>(bitmap >> 8),
        });
        sendBytes({
            23, 27, 0x22,
            static_cast<std::uint8_t>(width),
            static_cast<std::uint8_t>(width >> 8),
            static_cast<std::uint8_t>(height),
            static_cast<std::uint8_t>(height >> 8),
            color,
        });
        settle();
    }

    void settle(std::chrono::milliseconds duration =
            std::chrono::milliseconds(75)) {
        std::this_thread::sleep_for(duration);
    }

    bool synchronize() {
        const std::uint8_t echo = nextEcho++;
        const std::uint8_t expected[] = {0x80, 1, echo};
        drain();
        sendBytes({23, 0, 0x80, echo});
        std::vector<std::uint8_t> received;
        auto deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline) {
            std::uint8_t byte;
            if (receive(&byte)) {
                received.push_back(byte);
                if (received.size() >= sizeof(expected)) {
                    auto start = received.size() - sizeof(expected);
                    bool match = true;
                    for (std::size_t i = 0; i < sizeof(expected); i++) {
                        if (received[start + i] != expected[i]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        return true;
                    }
                }
            } else {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(100));
            }
        }
        return false;
    }

    void drain() {
        std::uint8_t byte;
        while (receive(&byte)) {
        }
    }

    bool waitForCompletion(
            std::uint16_t token, std::uint16_t sequence,
            std::chrono::milliseconds timeout =
                std::chrono::milliseconds(750)) {
        const std::uint8_t expected[] = {
            0x81, 10, 'P', '3', 'D', 'R', 1, 1,
            static_cast<std::uint8_t>(token),
            static_cast<std::uint8_t>(token >> 8),
            static_cast<std::uint8_t>(sequence),
            static_cast<std::uint8_t>(sequence >> 8),
        };
        std::vector<std::uint8_t> received;
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            std::uint8_t byte;
            if (receive(&byte)) {
                received.push_back(byte);
                if (received.size() >= sizeof(expected)) {
                    auto start = received.size() - sizeof(expected);
                    bool match = true;
                    for (std::size_t i = 0; i < sizeof(expected); i++) {
                        if (received[start + i] != expected[i]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        return true;
                    }
                }
            } else {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(100));
            }
        }
        return false;
    }
};

void require(bool condition, const char * message, Harness& harness) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        harness.shutdown();
        std::exit(1);
    }
}

void populateObject(
        Harness& harness, std::uint16_t control,
        std::uint16_t object, std::uint16_t mesh,
        std::uint16_t bitmap) {
    harness.sendPingo(control, 1, {
        mesh, 3,
        0xC000, 0xC000, 0xC000,
        0x4000, 0xC000, 0xC000,
        0x0000, 0x4000, 0xC000,
    });
    harness.sendPingo(control, 2, {mesh, 3, 0, 1, 2});
    harness.sendPingo(control, 3, {
        mesh, 3,
        0x0000, 0x0000,
        0xFFFF, 0x0000,
        0x0000, 0xFFFF,
    });
    harness.sendPingo(control, 4, {mesh, 3, 0, 1, 2});
    harness.sendPingo(control, 5, {object, mesh, bitmap});
    harness.sendPingo(control, 40, {
        object, 3,
        0x0000, 0x0000,
        0xFFFF, 0x0000,
        0x0000, 0xFFFF,
    });
    harness.settle();
}

struct PackedMesh {
    std::uint16_t vertices;
    std::uint16_t positionIndices;
    std::uint16_t textureCoordinates;
    std::uint16_t textureIndices;
    std::vector<std::uint8_t> bytes;
};

PackedMesh packMesh(
        const std::vector<std::int16_t>& xyz,
        const std::vector<std::uint16_t>& positionIndices,
        const std::vector<std::uint16_t>& uv,
        const std::vector<std::uint16_t>& textureIndices) {
    if ((xyz.size() % 3) != 0 || (uv.size() % 2) != 0 ||
        xyz.size() / 3 > UINT16_MAX ||
        positionIndices.size() > UINT16_MAX ||
        uv.size() / 2 > UINT16_MAX ||
        textureIndices.size() > UINT16_MAX) {
        std::fprintf(stderr, "invalid packed-mesh test fixture\n");
        std::exit(2);
    }
    PackedMesh packed = {
        static_cast<std::uint16_t>(xyz.size() / 3),
        static_cast<std::uint16_t>(positionIndices.size()),
        static_cast<std::uint16_t>(uv.size() / 2),
        static_cast<std::uint16_t>(textureIndices.size()),
        {}
    };
    for (auto value : xyz) {
        Harness::appendWord(
            packed.bytes, static_cast<std::uint16_t>(value));
    }
    for (auto value : positionIndices) {
        Harness::appendWord(packed.bytes, value);
    }
    for (auto value : uv) {
        Harness::appendWord(packed.bytes, value);
    }
    for (auto value : textureIndices) {
        Harness::appendWord(packed.bytes, value);
    }
    return packed;
}

PackedMesh makeTexturedStreamingMesh(std::int16_t peak) {
    return packMesh(
        {
            INT16_MIN, 0, -16384,
            INT16_MAX, 0, -16384,
            0, peak, -16384,
            0, 0, INT16_MAX,
        },
        {0, 1, 2, 2, 1, 3},
        {
            0x0000, 0x0000,
            0xFFFF, 0x0000,
            0x0000, 0xFFFF,
            0x8000, 0x4000,
        },
        {0, 1, 2, 2, 1, 3});
}

PackedMesh makeFlatStreamingMesh(std::int16_t peak) {
    return packMesh(
        {
            INT16_MIN, 0, -16384,
            INT16_MAX, 0, -16384,
            0, peak, -16384,
            0, 0, INT16_MAX,
        },
        {0, 1, 2, 2, 1, 3},
        {
            0x0000, 0x0000,
            0x8000, 0x4000,
        },
        {0, 0, 0, 1, 1, 1});
}

void testScaleSetters(Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1100;
    constexpr std::uint16_t object = 7;
    harness.sendPingo(control, 0, {64, 64});
    harness.sendPingo(control, 9, {object, 256, 512, 768});
    harness.sendPingo(control, 8, {object, 1024});
    harness.sendPingo(control, 29, {256, 512, 768});
    harness.sendPingo(control, 28, {1024});
    require(
        harness.synchronize(),
        "general-poll barrier failed after scale commands", harness);

    float scale[3] = {};
    require(
        harness.objectScale(control, object, scale),
        "could not inspect object scale", harness);
    std::fprintf(
        stderr, "object scale after subcommands 9/8: %.6f %.6f %.6f\n",
        scale[0], scale[1], scale[2]);
    require(
        approximately(scale[0], 1.0f) &&
        approximately(scale[1], 2.0f) &&
        approximately(scale[2], 4.0f),
        "single-axis object Z scale changed the wrong component", harness);
    require(
        harness.sceneScale(control, scale),
        "could not inspect scene scale", harness);
    std::fprintf(
        stderr, "scene scale after subcommands 29/28: %.6f %.6f %.6f\n",
        scale[0], scale[1], scale[2]);
    require(
        approximately(scale[0], 1.0f) &&
        approximately(scale[1], 2.0f) &&
        approximately(scale[2], 4.0f),
        "single-axis scene Z scale changed the wrong component", harness);

    harness.sendPingo(control, 39);
    require(
        harness.synchronize(),
        "general-poll barrier failed after explicit teardown", harness);
    require(!harness.controlExists(control), "explicit teardown failed", harness);
    require(
        harness.ownedAllocations() == baseline,
        "explicit teardown leaked Pingo-owned allocations", harness);
}

void testWideObjectTranslation(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1110;
    constexpr std::uint16_t object = 7;
    constexpr std::uint16_t absentObject = 8;
    constexpr std::uint16_t mesh = 7;
    constexpr std::uint16_t texture = 410;
    constexpr std::uint16_t target = 411;
    constexpr float translationFactor = 256.0f / 32767.0f;

    auto sendWide = [&](std::uint16_t targetObject,
                        std::uint32_t x,
                        std::uint32_t y,
                        std::uint32_t z) {
        auto command = harness.pingoPrefix(control, 52);
        Harness::appendWord(command, targetObject);
        Harness::append24(command, x);
        Harness::append24(command, y);
        Harness::append24(command, z);
        harness.sendBytes(command);
    };
    auto inspect = [&](float * translation,
                       float * matrixTranslation,
                       std::uint8_t * modified) {
        return harness.objectTranslation(
            control, object, translation, matrixTranslation, modified);
    };

    harness.createBitmap2222(texture, 4, 4, 0xFC);
    harness.createBitmap2222(target, 32, 32, 0);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    harness.sendPingo(control, 38, {target});
    require(
        harness.synchronize(),
        "wide-translation fixture initialization disrupted alignment",
        harness);

    float translation[3] = {};
    float matrixTranslation[3] = {};
    std::uint8_t modified = 1;
    require(
        inspect(translation, matrixTranslation, &modified) &&
        modified == 0,
        "initial render did not consume the object's dirty transform",
        harness);

    // Exercise little-endian sign endpoints and the complete three-axis
    // publication in one fixed-width packet.
    sendWide(object, 0x7FFFFFU, 0x800000U, 0xFF8000U);
    require(
        harness.synchronize() &&
        inspect(translation, matrixTranslation, &modified) &&
        modified == 1 &&
        approximatelyWide(
            translation[0], 8388607.0f * translationFactor) &&
        approximatelyWide(
            translation[1], -8388608.0f * translationFactor) &&
        approximatelyWide(
            translation[2], -32768.0f * translationFactor),
        "wide XYZ translation did not decode signed 24-bit endpoints",
        harness);

    harness.sendPingo(control, 38, {target});
    require(
        harness.synchronize() &&
        inspect(translation, matrixTranslation, &modified) &&
        modified == 0 &&
        approximatelyWide(matrixTranslation[0], translation[0]) &&
        approximatelyWide(matrixTranslation[1], translation[1]) &&
        approximatelyWide(matrixTranslation[2], translation[2]),
        "wide XYZ translation did not reach the rendered object matrix",
        harness);

    // Sign-extending a legacy word into 24 bits must retain command 17's
    // established physical unit exactly.
    harness.sendPingo(control, 17, {object, 0x8000, 0x0000, 0x7FFF});
    require(
        harness.synchronize() &&
        inspect(translation, matrixTranslation, &modified),
        "legacy translation comparison command lost alignment", harness);
    float legacy[3] = {
        translation[0], translation[1], translation[2]
    };
    sendWide(object, 0xFF8000U, 0x000000U, 0x007FFFU);
    require(
        harness.synchronize() &&
        inspect(translation, matrixTranslation, &modified) &&
        approximately(translation[0], legacy[0]) &&
        approximately(translation[1], legacy[1]) &&
        approximately(translation[2], legacy[2]),
        "wide XYZ translation changed the legacy raw-count unit", harness);

    // A numeric ID without an established stable object still consumes its
    // complete packet, but must neither allocate nor change accepted state.
    sendWide(absentObject, 0x123456U, 0x654321U, 0xFEDCBAU);
    require(
        harness.synchronize() &&
        !harness.objectTranslation(
            control, absentObject,
            translation, matrixTranslation, &modified) &&
        inspect(translation, matrixTranslation, &modified) &&
        approximately(translation[0], legacy[0]) &&
        approximately(translation[1], legacy[1]) &&
        approximately(translation[2], legacy[2]),
        "absent-object wide command created state or lost alignment",
        harness);

    // Fixed-width truncation times out once at the first missing byte and
    // preserves the last complete transform.
    auto truncated = harness.pingoPrefix(control, 52);
    Harness::appendWord(truncated, object);
    Harness::append24(truncated, 0x123456U);
    Harness::append24(truncated, 0x654321U);
    truncated.push_back(0xBA);
    truncated.push_back(0xDC);
    harness.sendBytes(truncated);
    harness.settle(std::chrono::milliseconds(450));
    require(
        harness.synchronize() &&
        inspect(translation, matrixTranslation, &modified) &&
        approximately(translation[0], legacy[0]) &&
        approximately(translation[1], legacy[1]) &&
        approximately(translation[2], legacy[2]),
        "truncated wide translation changed state or lost alignment",
        harness);

    harness.sendPingo(control, 39);
    harness.clearBuffer(texture);
    harness.clearBuffer(target);
    require(
        harness.synchronize(),
        "wide-translation teardown disrupted command alignment", harness);
    require(
        harness.ownedAllocations() == baseline,
        "wide-translation test leaked Pingo-owned allocations", harness);
}

void testProjectionFarPlane(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1111;
    constexpr std::uint16_t isolatedControl = 1112;
    constexpr std::uint16_t target = 412;
    constexpr std::uint16_t texture = 413;
    constexpr std::uint16_t object = 7;
    constexpr std::uint16_t mesh = 7;
    constexpr std::uint16_t defaultFar = 2500;
    constexpr std::uint32_t centerPixel = 32U * 64U + 32U;

    auto sendFar = [&](std::uint16_t targetControl, std::uint16_t farUnits) {
        auto command = harness.pingoPrefix(targetControl, 53);
        Harness::appendWord(command, farUnits);
        harness.sendBytes(command);
    };
    auto inspect = [&](std::uint16_t targetControl, std::uint16_t expected) {
        std::uint16_t actual = 0;
        return harness.projectionFar(targetControl, &actual) &&
            actual == expected;
    };
    auto sendWideZ = [&](std::uint32_t rawZ) {
        auto command = harness.pingoPrefix(control, 52);
        Harness::appendWord(command, object);
        Harness::append24(command, 0);
        Harness::append24(command, 0);
        Harness::append24(command, rawZ);
        harness.sendBytes(command);
    };

    std::uint16_t absent = 0;
    require(
        !harness.projectionFar(control, &absent),
        "projection getter accepted an absent control", harness);

    harness.createBitmap2222(target, 64, 64, 0);
    harness.createBitmap2222(texture, 4, 4, 0xFC);
    harness.sendPingo(control, 0, {64, 64});
    harness.sendPingo(isolatedControl, 0, {64, 64});
    require(
        harness.synchronize() &&
        inspect(control, defaultFar) &&
        inspect(isolatedControl, defaultFar),
        "projection far plane did not initialize per control", harness);

    // Establish the renderer's actual cleared RGBA2222 value rather than
    // assuming anything about the bitmap allocator's initial fill.
    harness.sendPingo(isolatedControl, 38, {target});
    std::uint8_t clearPixel = 0xFF;
    require(
        harness.synchronize() &&
        harness.bitmapPixel(target, centerPixel, &clearPixel),
        "could not inspect a freshly cleared render target", harness);

    // Put a large, centered triangle beyond the default plane.  Its complete
    // geometry is around Z=-3128 after scale, safely outside 2500 but inside
    // 8000.  This makes command 53 prove visible renderer behavior rather
    // than only retained control state.
    populateObject(harness, control, object, mesh, texture);
    harness.sendPingo(control, 9, {object, UINT16_MAX, UINT16_MAX, UINT16_MAX});
    harness.sendPingoBytes(control, 46, {0});
    sendWideZ(0xFA2400U); // -384000 raw = approximately -3000 units.
    harness.sendPingo(control, 38, {target});
    std::uint8_t pixel = 0xFF;
    bool defaultRead = harness.synchronize() &&
        harness.bitmapPixel(target, centerPixel, &pixel);
    if (!defaultRead || pixel != clearPixel) {
        std::fprintf(
            stderr,
            "default projection center pixel: clear=%02X actual=%02X\n",
            clearPixel, pixel);
    }
    require(
        defaultRead && pixel == clearPixel,
        "default projection rendered geometry beyond 2500 units", harness);

    sendFar(control, 8000);
    harness.sendPingo(control, 38, {target});
    require(
        harness.synchronize() &&
        inspect(control, 8000) &&
        inspect(isolatedControl, defaultFar) &&
        harness.bitmapPixel(target, centerPixel, &pixel) &&
        pixel != clearPixel,
        "valid projection did not extend visibility or crossed controls",
        harness);

    // Both invalid complete values must drain normally and preserve state.
    sendFar(control, 0);
    sendFar(control, 1);
    require(
        harness.synchronize() &&
        inspect(control, 8000) &&
        inspect(isolatedControl, defaultFar),
        "projection accepted a far plane at or behind the near plane",
        harness);

    // Both accepted endpoints reach the renderer after the explicit
    // default-versus-8000 visibility witness above.
    sendFar(control, UINT16_MAX);
    harness.sendPingo(control, 38, {target});
    require(
        harness.synchronize() &&
        inspect(control, UINT16_MAX),
        "maximum projection far plane did not survive a render", harness);
    sendFar(control, 2);
    harness.sendPingo(control, 38, {target});
    require(
        harness.synchronize() &&
        inspect(control, 2) &&
        harness.bitmapPixel(target, centerPixel, &pixel) &&
        pixel == clearPixel,
        "minimum projection far plane was rejected or not rendered",
        harness);

    // A mid-word timeout is fail-closed and recovers before the next command.
    sendFar(control, 8000);
    require(
        harness.synchronize() && inspect(control, 8000),
        "projection truncation fixture setup failed", harness);
    auto truncated = harness.pingoPrefix(control, 53);
    truncated.push_back(0x34);
    harness.sendBytes(truncated);
    harness.settle(std::chrono::milliseconds(450));
    require(
        harness.synchronize() &&
        inspect(control, 8000) &&
        inspect(isolatedControl, defaultFar),
        "truncated projection command mutated state or lost alignment",
        harness);

    harness.sendPingo(control, 39);
    harness.sendPingo(isolatedControl, 39);
    harness.clearBuffer(texture);
    harness.clearBuffer(target);
    require(
        harness.synchronize(),
        "projection teardown disrupted command alignment", harness);
    require(
        harness.ownedAllocations() == baseline,
        "projection test leaked Pingo-owned allocations", harness);

    // Reinitialization must restore backward-compatible behavior rather than
    // retaining the previous control's experimental setting.
    harness.sendPingo(control, 0, {16, 16});
    require(
        harness.synchronize() && inspect(control, defaultFar),
        "projection default did not return after reinitialization", harness);
    harness.sendPingo(control, 39);
    require(
        harness.synchronize() && harness.ownedAllocations() == baseline,
        "projection reinitialization teardown leaked state", harness);
}

void testLightingAndShadingCommands(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1107;
    constexpr std::uint16_t mesh = 23;
    harness.sendPingo(control, 0, {64, 64});
    require(
        harness.synchronize(),
        "general-poll barrier failed after lighting initialization", harness);

    float direction[3] = {};
    std::uint8_t intensity = 0;
    std::uint8_t ambient = 0;
    std::uint8_t enabled = 0;
    require(
        harness.lightingState(
            control, direction, &intensity, &ambient, &enabled),
        "could not inspect default lighting state", harness);
    const float inverseSqrtTwo = 0.70710678f;
    require(
        approximately(direction[0], 0.0f) &&
        approximately(direction[1], inverseSqrtTwo) &&
        approximately(direction[2], -inverseSqrtTwo) &&
        intensity == 127 && ambient == 0 && enabled == 1,
        "lighting initialization did not publish qualified defaults", harness);

    // Establishing a mesh through an older rendering-policy command must
    // retain the zero-valued, backward-compatible scene-lighting policy.
    auto texturedMode = harness.pingoPrefix(control, 47);
    Harness::appendWord(texturedMode, mesh);
    texturedMode.push_back(0);
    harness.sendBytes(texturedMode);
    require(
        harness.synchronize(),
        "general-poll barrier failed after default mesh policy setup",
        harness);
    std::uint8_t illuminationPolicy = 0xFF;
    require(
        harness.meshIlluminationPolicy(
            control, mesh, &illuminationPolicy) &&
        illuminationPolicy == 0,
        "new mesh did not default to inherited scene illumination", harness);

    harness.sendPingo(control, 43, {100, 0, 0});
    harness.sendPingoBytes(control, 44, {255});
    harness.sendPingoBytes(control, 45, {63});
    harness.sendPingoBytes(control, 46, {0});
    auto flatMode = harness.pingoPrefix(control, 47);
    Harness::appendWord(flatMode, mesh);
    flatMode.push_back(1);
    harness.sendBytes(flatMode);
    auto selfIlluminated = harness.pingoPrefix(control, 48);
    Harness::appendWord(selfIlluminated, mesh);
    selfIlluminated.push_back(1);
    harness.sendBytes(selfIlluminated);
    require(
        harness.synchronize(),
        "general-poll barrier failed after lighting commands", harness);
    require(
        harness.lightingState(
            control, direction, &intensity, &ambient, &enabled) &&
        approximately(direction[0], 1.0f) &&
        approximately(direction[1], 0.0f) &&
        approximately(direction[2], 0.0f) &&
        intensity == 255 && ambient == 63 && enabled == 0,
        "lighting commands did not update scene-wide state", harness);

    // Direction components are signed little-endian words. Verify the wire
    // representation, not merely the all-positive convenience case above.
    harness.sendPingo(control, 43, {0xFF9C, 100, 0xFF9C});
    require(
        harness.synchronize(),
        "signed light direction disrupted command alignment", harness);
    const float inverseSqrtThree = 0.57735027f;
    require(
        harness.lightingState(
            control, direction, &intensity, &ambient, &enabled) &&
        approximately(direction[0], -inverseSqrtThree) &&
        approximately(direction[1], inverseSqrtThree) &&
        approximately(direction[2], -inverseSqrtThree),
        "signed light direction was decoded incorrectly", harness);

    std::uint8_t shadingMode = 0;
    require(
        harness.meshShadingMode(control, mesh, &shadingMode) &&
        shadingMode == 1 &&
        harness.meshIlluminationPolicy(
            control, mesh, &illuminationPolicy) &&
        illuminationPolicy == 1,
        "mesh rendering-policy commands did not update mesh state", harness);

    // Mode may be selected before any geometry arrives. Later component
    // uploads update the same mesh and must not reset its rendering policy.
    harness.sendPingo(control, 1, {
        mesh, 3,
        0xC000, 0xC000, 0xC000,
        0x4000, 0xC000, 0xC000,
        0x0000, 0x4000, 0xC000,
    });
    harness.sendPingo(control, 2, {mesh, 3, 0, 1, 2});
    require(
        harness.synchronize(),
        "mesh upload after shading selection disrupted alignment", harness);
    require(
        harness.meshShadingMode(control, mesh, &shadingMode) &&
        shadingMode == 1 &&
        harness.meshIlluminationPolicy(
            control, mesh, &illuminationPolicy) &&
        illuminationPolicy == 1,
        "mesh upload reset its previously selected rendering policy", harness);

    // Pattern shading may be selected before its pattern library is bound.
    // The renderer will fail closed until a valid library is installed.
    auto patternMode = harness.pingoPrefix(control, 47);
    Harness::appendWord(patternMode, mesh);
    patternMode.push_back(2);
    harness.sendBytes(patternMode);
    require(
        harness.synchronize(),
        "flat-pattern mesh mode disrupted command alignment", harness);
    require(
        harness.meshShadingMode(control, mesh, &shadingMode) &&
        shadingMode == 2,
        "flat-pattern mesh mode was not accepted", harness);

    // Invalid values are consumed but preserve the previous state. In
    // particular, a zero direction must not destroy the usable light.
    harness.sendPingo(control, 43, {0, 0, 0});
    harness.sendPingoBytes(control, 46, {2});
    auto invalidMode = harness.pingoPrefix(control, 47);
    Harness::appendWord(invalidMode, mesh);
    invalidMode.push_back(3);
    harness.sendBytes(invalidMode);
    auto invalidPolicy = harness.pingoPrefix(control, 48);
    Harness::appendWord(invalidPolicy, mesh);
    invalidPolicy.push_back(2);
    harness.sendBytes(invalidPolicy);
    require(
        harness.synchronize(),
        "invalid lighting commands disrupted command alignment", harness);
    require(
        harness.lightingState(
            control, direction, &intensity, &ambient, &enabled) &&
        approximately(direction[0], -inverseSqrtThree) &&
        approximately(direction[1], inverseSqrtThree) &&
        approximately(direction[2], -inverseSqrtThree) &&
        enabled == 0 &&
        harness.meshShadingMode(control, mesh, &shadingMode) &&
        shadingMode == 2 &&
        harness.meshIlluminationPolicy(
            control, mesh, &illuminationPolicy) &&
        illuminationPolicy == 1,
        "invalid lighting command changed accepted state", harness);

    // Fixed-size commands commit only after their complete payload arrives.
    // A timeout must preserve accepted state and release the stream for the
    // next VDU command. Exercise both a mid-word direction timeout and a
    // missing mesh-mode byte.
    auto truncatedDirection = harness.pingoPrefix(control, 43);
    Harness::appendWord(truncatedDirection, 200);
    truncatedDirection.push_back(0x34);
    harness.sendBytes(truncatedDirection);
    harness.settle(std::chrono::milliseconds(450));
    require(
        harness.synchronize(),
        "truncated light direction did not recover alignment", harness);
    require(
        harness.lightingState(
            control, direction, &intensity, &ambient, &enabled) &&
        approximately(direction[0], -inverseSqrtThree) &&
        approximately(direction[1], inverseSqrtThree) &&
        approximately(direction[2], -inverseSqrtThree),
        "truncated light direction changed accepted state", harness);

    constexpr std::uint16_t truncatedMesh = 24;
    auto truncatedMode = harness.pingoPrefix(control, 47);
    Harness::appendWord(truncatedMode, truncatedMesh);
    harness.sendBytes(truncatedMode);
    harness.settle(std::chrono::milliseconds(450));
    require(
        harness.synchronize(),
        "truncated mesh shading command did not recover alignment", harness);
    require(
        !harness.meshShadingMode(control, truncatedMesh, &shadingMode) &&
        harness.meshShadingMode(control, mesh, &shadingMode) &&
        shadingMode == 2,
        "truncated mesh shading command created or changed mesh state",
        harness);

    constexpr std::uint16_t truncatedPolicyMesh = 25;
    auto truncatedPolicy = harness.pingoPrefix(control, 48);
    Harness::appendWord(truncatedPolicy, truncatedPolicyMesh);
    harness.sendBytes(truncatedPolicy);
    harness.settle(std::chrono::milliseconds(450));
    require(
        harness.synchronize(),
        "truncated mesh illumination command did not recover alignment",
        harness);
    require(
        !harness.meshIlluminationPolicy(
            control, truncatedPolicyMesh, &illuminationPolicy) &&
        harness.meshIlluminationPolicy(
            control, mesh, &illuminationPolicy) &&
        illuminationPolicy == 1,
        "truncated mesh illumination command created or changed mesh state",
        harness);

    harness.sendPingo(control, 39);
    harness.sendPingo(control, 0, {32, 32});
    require(
        harness.synchronize(),
        "general-poll barrier failed after lighting reset", harness);
    require(
        harness.lightingState(
            control, direction, &intensity, &ambient, &enabled) &&
        approximately(direction[0], 0.0f) &&
        approximately(direction[1], inverseSqrtTwo) &&
        approximately(direction[2], -inverseSqrtTwo) &&
        intensity == 127 && ambient == 0 && enabled == 1,
        "control reinitialization did not restore lighting defaults", harness);
    require(
        !harness.meshShadingMode(control, mesh, &shadingMode) &&
        !harness.meshIlluminationPolicy(
            control, mesh, &illuminationPolicy),
        "control reinitialization retained stale mesh rendering state",
        harness);
    harness.sendPingo(control, 39);
    require(
        harness.synchronize(),
        "general-poll barrier failed after lighting teardown", harness);
    require(
        harness.ownedAllocations() == baseline,
        "lighting command test leaked Pingo-owned allocations", harness);
}

void testFlatPatternLibrary(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1108;
    constexpr std::uint16_t patternsBuffer = 400;
    constexpr std::uint16_t lookupBuffer = 401;

    const std::vector<std::uint8_t> patterns = {
        // Two opaque 4x4 RGBA2222 patterns.
        0xC0, 0xC1, 0xC2, 0xC3,
        0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB,
        0xCC, 0xCD, 0xCE, 0xCF,
        0xFF, 0xFE, 0xFD, 0xFC,
        0xFB, 0xFA, 0xF9, 0xF8,
        0xF7, 0xF6, 0xF5, 0xF4,
        0xF3, 0xF2, 0xF1, 0xF0,
    };
    const std::vector<std::uint8_t> lookup = {0, 1, 1, 0};

    auto sendBinding = [&](std::uint16_t patternBuffer,
                           std::uint16_t tableBuffer,
                           std::uint16_t patternCount,
                           std::uint8_t materialCount,
                           std::uint8_t bandCount) {
        auto command = harness.pingoPrefix(control, 49);
        Harness::appendWord(command, patternBuffer);
        Harness::appendWord(command, tableBuffer);
        Harness::appendWord(command, patternCount);
        command.push_back(materialCount);
        command.push_back(bandCount);
        harness.sendBytes(command);
    };
    auto requireBinding = [&](std::uint16_t expectedPatternCount,
                              std::uint8_t expectedMaterialCount,
                              std::uint8_t expectedBandCount,
                              std::uint8_t expectedPatternPixel,
                              std::uint8_t expectedLookup,
                              const char * failure) {
        std::uint16_t actualPatternCount = 0;
        std::uint8_t actualMaterialCount = 0;
        std::uint8_t actualBandCount = 0;
        std::uint8_t actualPatternPixel = 0;
        std::uint8_t actualLookup = 0;
        require(
            harness.flatPatternState(
                control, &actualPatternCount, &actualMaterialCount,
                &actualBandCount, &actualPatternPixel, &actualLookup) &&
            actualPatternCount == expectedPatternCount &&
            actualMaterialCount == expectedMaterialCount &&
            actualBandCount == expectedBandCount &&
            actualPatternPixel == expectedPatternPixel &&
            actualLookup == expectedLookup,
            failure, harness);
    };
    auto requireOriginalBinding = [&](const char * failure) {
        requireBinding(2, 2, 2, patterns.front(), lookup.front(), failure);
    };
    auto replaceSources = [&](const std::vector<std::uint8_t>& newPatterns,
                              const std::vector<std::uint8_t>& newLookup) {
        harness.clearBuffer(patternsBuffer);
        harness.clearBuffer(lookupBuffer);
        harness.appendBuffer(patternsBuffer, newPatterns);
        harness.appendBuffer(lookupBuffer, newLookup);
    };

    harness.sendPingo(control, 0, {32, 32});
    harness.appendBuffer(patternsBuffer, patterns);
    harness.appendBuffer(lookupBuffer, lookup);
    sendBinding(patternsBuffer, lookupBuffer, 2, 2, 2);
    require(
        harness.synchronize(),
        "flat-pattern binding disrupted command alignment", harness);
    requireOriginalBinding("valid flat-pattern binding was not published");
    const auto allocationsWithBinding = harness.ownedAllocations();

    // The accepted library is an immutable private snapshot. Generic clear,
    // same-ID replacement, and in-place mutation of its former sources must
    // not change it behind the renderer's back.
    replaceSources(std::vector<std::uint8_t>(32, 0xFC), {1, 0, 0, 1});
    harness.reverseBuffer(patternsBuffer);
    harness.reverseBuffer(lookupBuffer);
    require(
        harness.synchronize(),
        "source mutation after pattern binding disrupted alignment", harness);
    requireOriginalBinding("source mutation changed the accepted snapshot");

    // Rebuild canonical sources before exercising validation failures.
    replaceSources(patterns, lookup);

    auto requireRejected = [&](const char * alignmentFailure,
                               const char * stateFailure) {
        require(harness.synchronize(), alignmentFailure, harness);
        requireOriginalBinding(stateFailure);
        require(
            harness.ownedAllocations() == allocationsWithBinding,
            "rejected flat-pattern binding changed allocation ownership",
            harness);
    };

    // Counts, IDs, exact one-block sizes, opacity, and lookup bounds are all
    // validated before the accepted resource changes.
    sendBinding(0, lookupBuffer, 2, 2, 2);
    requireRejected(
        "mixed-zero flat-pattern command disrupted alignment",
        "mixed-zero flat-pattern command replaced accepted state");
    sendBinding(patternsBuffer, lookupBuffer, 2, 2, 1);
    requireRejected(
        "one-band flat-pattern command disrupted alignment",
        "one-band flat-pattern command replaced accepted state");
    sendBinding(control, lookupBuffer, 2, 2, 2);
    requireRejected(
        "control-backed flat-pattern command disrupted alignment",
        "control-backed flat-pattern command replaced accepted state");

    auto nonOpaque = patterns;
    nonOpaque[7] &= 0x3F;
    replaceSources(nonOpaque, lookup);
    sendBinding(patternsBuffer, lookupBuffer, 2, 2, 2);
    requireRejected(
        "non-opaque flat-pattern command disrupted alignment",
        "non-opaque flat-pattern command replaced accepted state");

    replaceSources(patterns, {0, 2, 1, 0});
    sendBinding(patternsBuffer, lookupBuffer, 2, 2, 2);
    requireRejected(
        "out-of-range pattern lookup disrupted alignment",
        "out-of-range pattern lookup replaced accepted state");

    replaceSources(patterns, lookup);
    harness.appendBuffer(patternsBuffer, {0xFC});
    sendBinding(patternsBuffer, lookupBuffer, 2, 2, 2);
    requireRejected(
        "multi-block pattern resource disrupted alignment",
        "multi-block pattern resource replaced accepted state");

    replaceSources(patterns, {0, 1, 1});
    sendBinding(patternsBuffer, lookupBuffer, 2, 2, 2);
    requireRejected(
        "short pattern lookup disrupted alignment",
        "short pattern lookup replaced accepted state");

    // Both owned allocations used by a replacement are transactional.
    replaceSources(patterns, lookup);
    for (std::int32_t failAfter = 0; failAfter < 2; failAfter++) {
        harness.failAllocationAfter(failAfter);
        sendBinding(patternsBuffer, lookupBuffer, 2, 2, 2);
        require(
            harness.synchronize(),
            "allocation-failed pattern binding disrupted alignment", harness);
        harness.failAllocationAfter(-1);
        requireOriginalBinding(
            "allocation-failed pattern binding replaced accepted state");
        require(
            harness.ownedAllocations() == allocationsWithBinding,
            "allocation-failed pattern binding leaked ownership", harness);
    }

    // A fixed-width command times out once at its first missing field, then
    // releases the stream without publishing a partial resource.
    auto truncated = harness.pingoPrefix(control, 49);
    Harness::appendWord(truncated, patternsBuffer);
    Harness::appendWord(truncated, lookupBuffer);
    Harness::appendWord(truncated, 2);
    truncated.push_back(2);
    harness.sendBytes(truncated);
    harness.settle(std::chrono::milliseconds(450));
    requireRejected(
        "truncated flat-pattern command did not recover alignment",
        "truncated flat-pattern command replaced accepted state");

    sendBinding(0, 0, 0, 0, 0);
    require(
        harness.synchronize(),
        "explicit flat-pattern clear disrupted alignment", harness);
    std::uint16_t patternCount = 0;
    std::uint8_t materialCount = 0;
    std::uint8_t bandCount = 0;
    std::uint8_t firstPatternPixel = 0;
    std::uint8_t firstLookup = 0;
    require(
        !harness.flatPatternState(
            control, &patternCount, &materialCount, &bandCount,
            &firstPatternPixel, &firstLookup),
        "explicit flat-pattern clear retained the resource", harness);
    require(
        harness.ownedAllocations() + 2 == allocationsWithBinding,
        "explicit flat-pattern clear did not release owned snapshot",
        harness);

    harness.sendPingo(control, 39);
    harness.clearBuffer(patternsBuffer);
    harness.clearBuffer(lookupBuffer);
    require(
        harness.synchronize(),
        "flat-pattern teardown disrupted command alignment", harness);
    require(
        harness.ownedAllocations() == baseline,
        "flat-pattern teardown leaked Pingo-owned allocations", harness);
}

void testStreamingMeshReplacementAndVisibility(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1109;
    constexpr std::uint16_t stage = 402;
    constexpr std::uint16_t texture = 403;
    constexpr std::uint16_t object = 31;
    constexpr std::uint16_t mesh = 31;

    auto sendReplacement = [&](std::uint16_t source,
                               std::uint16_t target,
                               std::uint16_t vertices,
                               std::uint16_t positionIndices,
                               std::uint16_t textureCoordinates,
                               std::uint16_t textureIndices) {
        harness.sendPingo(control, 50, {
            source, target, vertices, positionIndices,
            textureCoordinates, textureIndices,
        });
    };
    auto sendPacked = [&](const PackedMesh& packed) {
        sendReplacement(
            stage, mesh, packed.vertices, packed.positionIndices,
            packed.textureCoordinates, packed.textureIndices);
    };
    auto replaceStage = [&](const std::vector<std::uint8_t>& bytes) {
        harness.clearBuffer(stage);
        harness.appendBuffer(stage, bytes);
    };

    harness.createBitmap2222(texture, 4, 4, 0xFC);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    require(
        harness.synchronize(),
        "streaming fixture initialization disrupted alignment", harness);
    require(
        harness.objectUsesMesh(control, object, mesh),
        "streaming fixture did not bind its stable mesh slot", harness);
    std::uint8_t active = 0;
    require(
        harness.objectActive(control, object, &active) && active == 1,
        "legacy object did not default active", harness);

    std::uint64_t originalHash = 0;
    require(
        harness.uploadHash(control, mesh, object, &originalHash),
        "could not hash original streaming slot", harness);
    const auto allocationsWithMesh = harness.ownedAllocations();

    auto textured = makeTexturedStreamingMesh(24576);
    replaceStage(textured.bytes);
    sendPacked(textured);
    require(
        harness.synchronize(),
        "valid streaming replacement disrupted alignment", harness);

    std::uint32_t vertexCount = 0;
    std::uint32_t positionIndexCount = 0;
    std::uint32_t textureCoordinateCount = 0;
    std::uint32_t textureIndexCount = 0;
    std::uint8_t geometryValid = 0;
    std::uint8_t boundsValid = 0;
    require(
        harness.meshStreamState(
            control, mesh, &vertexCount, &positionIndexCount,
            &textureCoordinateCount, &textureIndexCount,
            &geometryValid, &boundsValid) &&
        vertexCount == textured.vertices &&
        positionIndexCount == textured.positionIndices &&
        textureCoordinateCount == textured.textureCoordinates &&
        textureIndexCount == textured.textureIndices &&
        geometryValid == 1 && boundsValid == 1,
        "valid streaming replacement published incorrect counts or validity",
        harness);

    float position[3] = {};
    float uv[2] = {};
    std::uint16_t positionIndex = UINT16_MAX;
    std::uint16_t textureIndex = UINT16_MAX;
    require(
        harness.meshStreamVertex(control, mesh, 0, position) &&
        approximately(position[0], -32768.0f / 32767.0f) &&
        approximately(position[1], 0.0f) &&
        approximately(position[2], -16384.0f / 32767.0f) &&
        harness.meshStreamUv(control, mesh, 3, uv) &&
        approximately(uv[0], 32768.0f / 65535.0f) &&
        approximately(uv[1], 16384.0f / 65535.0f) &&
        harness.meshStreamIndices(
            control, mesh, 5, &positionIndex, &textureIndex) &&
        positionIndex == 3 && textureIndex == 3,
        "streaming replacement did not use established wire conversions",
        harness);

    std::uint64_t acceptedHash = 0;
    require(
        harness.uploadHash(control, mesh, object, &acceptedHash) &&
        acceptedHash != originalHash &&
        harness.objectUsesMesh(control, object, mesh) &&
        harness.ownedAllocations() == allocationsWithMesh,
        "valid streaming replacement was not atomic in its existing slot",
        harness);

    auto requireAcceptedState = [&](const char * failure) {
        std::uint64_t actualHash = 0;
        require(
            harness.uploadHash(control, mesh, object, &actualHash) &&
            actualHash == acceptedHash &&
            harness.objectUsesMesh(control, object, mesh) &&
            harness.ownedAllocations() == allocationsWithMesh,
            failure, harness);
    };
    auto requireRejected = [&](const char * alignmentFailure,
                               const char * stateFailure) {
        require(harness.synchronize(), alignmentFailure, harness);
        requireAcceptedState(stateFailure);
    };

    // The accepted arrays are a private converted snapshot of the stage.
    harness.reverseBuffer(stage);
    harness.clearBuffer(stage);
    harness.appendBuffer(stage, std::vector<std::uint8_t>(64, 0xA5));
    require(
        harness.synchronize(),
        "staging-buffer mutation disrupted alignment", harness);
    requireAcceptedState(
        "staging-buffer mutation changed the accepted private mesh");
    replaceStage(textured.bytes);

    // Source identity/layout and target-slot validation all fail closed.
    sendReplacement(
        777, mesh, textured.vertices, textured.positionIndices,
        textured.textureCoordinates, textured.textureIndices);
    requireRejected(
        "missing staging buffer disrupted alignment",
        "missing staging buffer changed the accepted mesh");
    sendReplacement(
        control, mesh, textured.vertices, textured.positionIndices,
        textured.textureCoordinates, textured.textureIndices);
    requireRejected(
        "control-backed staging buffer disrupted alignment",
        "control-backed staging buffer changed the accepted mesh");
    sendReplacement(
        0, mesh, textured.vertices, textured.positionIndices,
        textured.textureCoordinates, textured.textureIndices);
    requireRejected(
        "zero staging-buffer ID disrupted alignment",
        "zero staging-buffer ID changed the accepted mesh");
    sendReplacement(
        UINT16_MAX, mesh, textured.vertices, textured.positionIndices,
        textured.textureCoordinates, textured.textureIndices);
    requireRejected(
        "current-buffer staging sentinel disrupted alignment",
        "current-buffer staging sentinel changed the accepted mesh");
    sendReplacement(
        stage, 99, textured.vertices, textured.positionIndices,
        textured.textureCoordinates, textured.textureIndices);
    requireRejected(
        "missing target slot disrupted alignment",
        "missing target slot changed the accepted mesh");
    std::uint8_t absentMode = 0;
    require(
        !harness.meshShadingMode(control, 99, &absentMode),
        "rejected replacement established its missing target slot", harness);

    harness.clearBuffer(stage);
    harness.appendBuffer(
        stage, std::vector<std::uint8_t>(
            textured.bytes.begin(), textured.bytes.end() - 1));
    sendPacked(textured);
    requireRejected(
        "short staging block disrupted alignment",
        "short staging block changed the accepted mesh");
    harness.clearBuffer(stage);
    auto stageMiddle = textured.bytes.begin() + textured.bytes.size() / 2;
    harness.appendBuffer(
        stage,
        std::vector<std::uint8_t>(textured.bytes.begin(), stageMiddle));
    harness.appendBuffer(
        stage,
        std::vector<std::uint8_t>(stageMiddle, textured.bytes.end()));
    sendPacked(textured);
    requireRejected(
        "multi-block staging resource disrupted alignment",
        "multi-block staging resource changed the accepted mesh");
    replaceStage(textured.bytes);
    auto oversized = textured.bytes;
    oversized.push_back(0);
    replaceStage(oversized);
    sendPacked(textured);
    requireRejected(
        "oversized staging block disrupted alignment",
        "oversized staging block changed the accepted mesh");

    // Every structural count constraint is checked before publication.
    replaceStage(textured.bytes);
    sendReplacement(stage, mesh, 0, 3, 1, 3);
    requireRejected(
        "zero vertex count disrupted alignment",
        "zero vertex count changed the accepted mesh");
    sendReplacement(stage, mesh, 3, 2, 1, 2);
    requireRejected(
        "short position-index count disrupted alignment",
        "short position-index count changed the accepted mesh");
    sendReplacement(stage, mesh, 3, 4, 1, 4);
    requireRejected(
        "non-triangle index count disrupted alignment",
        "non-triangle index count changed the accepted mesh");
    sendReplacement(stage, mesh, 3, 3, 0, 3);
    requireRejected(
        "zero UV count disrupted alignment",
        "zero UV count changed the accepted mesh");
    sendReplacement(stage, mesh, 3, 3, 1, 6);
    requireRejected(
        "mismatched UV-index count disrupted alignment",
        "mismatched UV-index count changed the accepted mesh");

    auto badPositionIndex = textured.bytes;
    const std::size_t positionIndexOffset =
        static_cast<std::size_t>(textured.vertices) * 6;
    badPositionIndex[positionIndexOffset] = textured.vertices;
    badPositionIndex[positionIndexOffset + 1] = 0;
    replaceStage(badPositionIndex);
    sendPacked(textured);
    requireRejected(
        "out-of-range position index disrupted alignment",
        "out-of-range position index changed the accepted mesh");

    auto badTextureIndex = textured.bytes;
    const std::size_t textureIndexOffset =
        static_cast<std::size_t>(textured.vertices) * 6 +
        static_cast<std::size_t>(textured.positionIndices) * 2 +
        static_cast<std::size_t>(textured.textureCoordinates) * 4;
    badTextureIndex[textureIndexOffset] = textured.textureCoordinates;
    badTextureIndex[textureIndexOffset + 1] = 0;
    replaceStage(badTextureIndex);
    sendPacked(textured);
    requireRejected(
        "out-of-range UV index disrupted alignment",
        "out-of-range UV index changed the accepted mesh");

    // Flat slots require all three UVs of each face to name one selector.
    auto flatMode = harness.pingoPrefix(control, 47);
    Harness::appendWord(flatMode, mesh);
    flatMode.push_back(1);
    harness.sendBytes(flatMode);
    require(
        harness.synchronize(),
        "flat-mode setup disrupted alignment", harness);
    require(
        harness.uploadHash(control, mesh, object, &acceptedHash),
        "could not hash flat-mode streaming state", harness);
    replaceStage(textured.bytes);
    sendPacked(textured);
    requireRejected(
        "ambiguous flat selectors disrupted alignment",
        "ambiguous flat selectors changed the accepted mesh");

    auto flat = makeFlatStreamingMesh(16384);
    replaceStage(flat.bytes);
    sendPacked(flat);
    require(
        harness.synchronize(),
        "valid flat streaming replacement disrupted alignment", harness);
    std::uint64_t flatHash = 0;
    require(
        harness.uploadHash(control, mesh, object, &flatHash) &&
        flatHash != acceptedHash &&
        harness.objectUsesMesh(control, object, mesh) &&
        harness.ownedAllocations() == allocationsWithMesh,
        "valid flat replacement did not commit in the stable slot", harness);
    acceptedHash = flatHash;

    // All four private component allocations are individually transactional.
    for (std::int32_t failAfter = 0; failAfter < 4; failAfter++) {
        harness.failAllocationAfter(failAfter);
        sendPacked(flat);
        require(
            harness.synchronize(),
            "allocation-failed mesh replacement disrupted alignment", harness);
        harness.failAllocationAfter(-1);
        requireAcceptedState(
            "allocation-failed mesh replacement changed accepted state");
    }

    // Repeated success replaces values but neither slot address nor ownership
    // count. This is the normal streaming reuse path.
    auto secondFlat = makeFlatStreamingMesh(8192);
    replaceStage(secondFlat.bytes);
    sendPacked(secondFlat);
    require(
        harness.synchronize(),
        "repeated streaming replacement disrupted alignment", harness);
    std::uint64_t secondHash = 0;
    require(
        harness.uploadHash(control, mesh, object, &secondHash) &&
        secondHash != acceptedHash &&
        harness.objectUsesMesh(control, object, mesh) &&
        harness.ownedAllocations() == allocationsWithMesh,
        "repeated streaming replacement was not stable", harness);
    acceptedHash = secondHash;

    auto truncated = harness.pingoPrefix(control, 50);
    Harness::appendWord(truncated, stage);
    Harness::appendWord(truncated, mesh);
    Harness::appendWord(truncated, secondFlat.vertices);
    Harness::appendWord(truncated, secondFlat.positionIndices);
    Harness::appendWord(truncated, secondFlat.textureCoordinates);
    harness.sendBytes(truncated);
    harness.settle(std::chrono::milliseconds(450));
    requireRejected(
        "truncated streaming command did not recover alignment",
        "truncated streaming command changed accepted state");

    auto sendActive = [&](std::uint16_t targetObject,
                          std::uint8_t value) {
        auto command = harness.pingoPrefix(control, 51);
        Harness::appendWord(command, targetObject);
        command.push_back(value);
        harness.sendBytes(command);
    };
    sendActive(object, 0);
    require(
        harness.synchronize() &&
        harness.objectActive(control, object, &active) && active == 0 &&
        harness.objectUsesMesh(control, object, mesh),
        "object-active command did not deactivate the bound object", harness);
    sendActive(object, 1);
    require(
        harness.synchronize() &&
        harness.objectActive(control, object, &active) && active == 1,
        "object-active command did not reactivate the object", harness);
    sendActive(object, 2);
    sendActive(999, 0);
    require(
        harness.synchronize() &&
        harness.objectActive(control, object, &active) && active == 1 &&
        !harness.objectActive(control, 999, &active),
        "invalid visibility command changed or created object state", harness);

    truncated = harness.pingoPrefix(control, 51);
    Harness::appendWord(truncated, object);
    harness.sendBytes(truncated);
    harness.settle(std::chrono::milliseconds(450));
    require(
        harness.synchronize() &&
        harness.objectActive(control, object, &active) && active == 1,
        "truncated visibility command changed state or lost alignment",
        harness);

    harness.sendPingo(control, 39);
    harness.clearBuffer(stage);
    harness.clearBuffer(texture);
    require(
        harness.synchronize(),
        "streaming fixture teardown disrupted alignment", harness);
    require(
        harness.ownedAllocations() == baseline,
        "streaming replacement or visibility teardown leaked ownership",
        harness);
}

void testUnboundedObjectTraversalAndHighIds(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1113;
    constexpr std::uint16_t target = 414;
    constexpr std::uint16_t texture = 415;
    constexpr std::uint16_t fillerMesh = 0;
    constexpr std::uint16_t highId = UINT16_MAX;
    constexpr std::uint16_t token = 0x6A43;
    constexpr std::uint32_t centerPixel = 32U * 64U + 32U;

    auto sendActive = [&](std::uint16_t object, std::uint8_t active) {
        auto command = harness.pingoPrefix(control, 51);
        Harness::appendWord(command, object);
        command.push_back(active);
        harness.sendBytes(command);
    };
    auto sendWideZ = [&](std::uint16_t object, std::uint32_t z) {
        auto command = harness.pingoPrefix(control, 52);
        Harness::appendWord(command, object);
        Harness::append24(command, 0);
        Harness::append24(command, 0);
        Harness::append24(command, z);
        harness.sendBytes(command);
    };

    harness.createBitmap2222(target, 64, 64, 0);
    harness.createBitmap2222(texture, 4, 4, 0xFC);
    harness.sendPingo(control, 0, {64, 64});
    auto notification = harness.pingoPrefix(control, 41);
    notification.push_back(1);
    Harness::appendWord(notification, token);
    harness.sendBytes(notification);
    harness.drain();

    std::uint16_t sequence = 0;
    auto render = [&](const char * failure) {
        harness.sendPingo(control, 38, {target});
        require(
            harness.waitForCompletion(token, sequence++),
            failure, harness);
    };

    render("empty high-ID fixture render did not complete");
    std::uint8_t clearPixel = 0xFF;
    require(
        harness.bitmapPixel(target, centerPixel, &clearPixel) &&
        clearPixel != 0xFC,
        "could not establish the high-ID fixture clear pixel", harness);

    /*
     * These 36 active objects deliberately bind an incomplete mesh. They
     * draw nothing, but the former transient 32-entry scene consumed all of
     * its slots before object geometry validation and therefore never
     * reached the valid high-ID witness below.
     */
    for (std::uint16_t object = 0; object < 36; object++) {
        harness.sendPingo(
            control, 5, {object, fillerMesh, texture});
    }

    /*
     * Use both endpoints of the 16-bit identity domain in the same fixture:
     * object zero exists above, while this centered drawable uses 65535 for
     * both its object and mesh IDs. Its geometry and pose duplicate the
     * already-qualified projection-far visibility witness.
     */
    populateObject(harness, control, highId, highId, texture);
    harness.sendPingo(
        control, 9, {highId, UINT16_MAX, UINT16_MAX, UINT16_MAX});
    harness.sendPingoBytes(control, 46, {0});
    harness.sendPingo(control, 53, {8000});
    sendWideZ(highId, 0xFA2400U);

    std::uint8_t active = 0;
    require(
        harness.synchronize() &&
        harness.objectUsesMesh(control, highId, highId) &&
        harness.objectActive(control, highId, &active) && active == 1,
        "maximum-ID object or mesh did not retain its complete identity",
        harness);
    const auto allocationsBeforeRenders = harness.ownedAllocations();

    render("render did not traverse beyond 32 active objects");
    std::uint8_t pixel = 0;
    require(
        harness.bitmapPixel(target, centerPixel, &pixel) &&
        pixel == 0xFC &&
        harness.ownedAllocations() == allocationsBeforeRenders,
        "maximum-ID object was not rendered allocation-free", harness);

    sendActive(highId, 0);
    render("render after maximum-ID deactivation did not complete");
    require(
        harness.objectActive(control, highId, &active) && active == 0 &&
        harness.bitmapPixel(target, centerPixel, &pixel) &&
        pixel == clearPixel &&
        harness.ownedAllocations() == allocationsBeforeRenders,
        "maximum-ID object deactivation changed ownership or remained visible",
        harness);

    sendActive(highId, 1);
    render("render after maximum-ID reactivation did not complete");
    require(
        harness.objectActive(control, highId, &active) && active == 1 &&
        harness.bitmapPixel(target, centerPixel, &pixel) &&
        pixel == 0xFC &&
        harness.ownedAllocations() == allocationsBeforeRenders,
        "maximum-ID object reactivation changed ownership or stayed hidden",
        harness);

    harness.sendPingo(control, 39);
    harness.clearBuffer(texture);
    harness.clearBuffer(target);
    require(
        harness.synchronize(),
        "unbounded object fixture teardown disrupted alignment", harness);
    require(
        harness.ownedAllocations() == baseline,
        "unbounded object fixture teardown leaked Pingo ownership", harness);
}

void testAtomicInitialization(Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1101;
    harness.sendPingo(control, 0, {0xFFFF, 0xFFFF});
    require(
        harness.synchronize(),
        "general-poll barrier failed after invalid initialization", harness);
    require(
        !harness.controlExists(control),
        "invalid dimensions published a Pingo control", harness);
    require(
        harness.ownedAllocations() == baseline,
        "invalid dimensions leaked Pingo-owned allocations", harness);

    for (std::int32_t failAfter = 0; failAfter < 4; failAfter++) {
        harness.failAllocationAfter(failAfter);
        harness.sendPingo(control, 0, {64, 64});
        require(
            harness.synchronize(),
            "general-poll barrier failed after injected init failure",
            harness);
        harness.failAllocationAfter(-1);
        require(
            !harness.controlExists(control),
            "partial initialization published a Pingo control", harness);
        require(
            harness.ownedAllocations() == baseline,
            "partial initialization leaked Pingo-owned allocations", harness);

        harness.sendPingo(control, 0, {64, 64});
        require(
            harness.synchronize(),
            "general-poll barrier failed after recovered initialization",
            harness);
        require(
            harness.controlExists(control),
            "failed initialization left its control buffer behind", harness);
        harness.sendPingo(control, 39);
        require(
            harness.synchronize(),
            "general-poll barrier failed after recovered teardown",
            harness);
        require(
            harness.ownedAllocations() == baseline,
            "recovered control teardown leaked allocations", harness);
    }
}

void testTeardownAndTextureLifetime(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1102;
    constexpr std::uint16_t texture = 360;
    constexpr std::uint16_t target = 361;
    constexpr std::uint16_t token = 0x6A42;
    constexpr std::uint16_t object = 9;
    constexpr std::uint16_t mesh = 9;

    harness.createBitmap2222(texture, 4, 4, 0x03);
    harness.createBitmap2222(target, 64, 64, 0);
    harness.sendPingo(control, 0, {64, 64});
    populateObject(harness, control, object, mesh, texture);
    auto notification = harness.pingoPrefix(control, 41);
    notification.push_back(1);
    Harness::appendWord(notification, token);
    harness.sendBytes(notification);
    harness.drain();

    std::uint16_t sequence = 0;
    auto render = [&](const char * failure) {
        harness.sendPingo(control, 38, {target});
        require(
            harness.waitForCompletion(token, sequence++),
            failure, harness);
    };

    std::uint8_t pixel = 0;
    render("initial texture render did not complete");
    require(
        harness.texturePixel(control, object, 0, &pixel) && pixel == 0x03,
        "initial pinned texture is incorrect", harness);

    harness.clearBuffer(texture);
    render("render after clearing a bound texture did not complete");
    require(
        harness.texturePixel(control, object, 0, &pixel) && pixel == 0x03,
        "clearing a bound bitmap invalidated its Pingo texture", harness);

    harness.createBitmap2222(texture, 4, 4, 0xFC);
    render("render after same-ID bitmap replacement did not complete");
    require(
        harness.texturePixel(control, object, 0, &pixel) && pixel == 0x03,
        "replacing a bitmap silently rebound an existing object", harness);

    harness.failAllocationAfter(0);
    harness.sendPingo(control, 5, {object, mesh, texture});
    render("render after failed texture rebind did not complete");
    harness.failAllocationAfter(-1);
    require(
        harness.texturePixel(control, object, 0, &pixel) && pixel == 0x03,
        "failed texture binding replaced the previous binding", harness);

    harness.sendPingo(control, 5, {object, mesh, texture});
    render("render after explicit texture rebind did not complete");
    require(
        harness.texturePixel(control, object, 0, &pixel) && pixel == 0xFC,
        "explicit object rebind did not select the replacement bitmap", harness);
    harness.clearBuffer(texture);
    render("render after clearing the replacement texture did not complete");
    require(
        harness.texturePixel(control, object, 0, &pixel) && pixel == 0xFC,
        "replacement texture storage was not pinned", harness);

    harness.clearBuffer(control);
    require(
        harness.synchronize(),
        "general-poll barrier failed after generic control clear", harness);
    require(
        !harness.controlExists(control),
        "generic single-buffer clear did not deinitialize Pingo", harness);
    require(
        harness.ownedAllocations() == baseline,
        "generic single-buffer clear leaked Pingo resources", harness);

    for (int cycle = 0; cycle < 16; cycle++) {
        harness.sendPingo(control, 0, {32, 32});
        harness.sendPingo(control, 39);
    }
    require(
        harness.synchronize(),
        "general-poll barrier failed after create/delete cycles", harness);
    require(
        harness.ownedAllocations() == baseline,
        "repeated create/delete cycles leaked Pingo resources", harness);

    // Exercise explicit teardown with every owned mesh/object component and
    // a pinned bitmap, not only an empty control.
    harness.createBitmap2222(texture, 4, 4, 0x3C);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    harness.sendPingo(control, 39);
    require(
        harness.synchronize(),
        "general-poll barrier failed after populated explicit teardown",
        harness);
    require(
        harness.ownedAllocations() == baseline,
        "populated explicit teardown leaked Pingo resources", harness);

    // Exercise global teardown with the same complete ownership graph.
    harness.clearBuffer(texture);
    harness.createBitmap2222(texture, 4, 4, 0xC3);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    harness.clearBuffer(0xFFFF);
    require(
        harness.synchronize(),
        "general-poll barrier failed after global clear", harness);
    require(
        !harness.controlExists(control),
        "global buffer clear left a Pingo control registered", harness);
    require(
        harness.ownedAllocations() == baseline,
        "global buffer clear leaked Pingo resources", harness);
}

void testRegisteredControlIsolation(
        Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1104;
    constexpr std::uint16_t clone = 1105;
    constexpr std::uint16_t texture = 380;
    constexpr std::uint16_t mesh = 12;
    constexpr std::uint16_t object = 12;
    constexpr std::uint16_t commandBuffer = 1106;

    harness.createBitmap2222(texture, 4, 4, 0xCC);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    require(
        harness.synchronize(),
        "general-poll barrier failed before control-isolation tests",
        harness);

    std::uint64_t expectedUploadHash = 0;
    require(
        harness.uploadHash(control, mesh, object, &expectedUploadHash),
        "could not inspect registered control before clone tests", harness);

    // A live control is opaque state, never a buffered VDU program. Exercise
    // direct call and the nested jump path, then prove its uploaded state is
    // unchanged and command processing remains aligned.
    harness.callBuffer(control);
    harness.appendBuffer(commandBuffer, {
        23, 0, 0xA0,
        static_cast<std::uint8_t>(control),
        static_cast<std::uint8_t>(control >> 8),
        7,
    });
    harness.callBuffer(commandBuffer);
    require(
        harness.synchronize(),
        "call/jump rejection disrupted command processing", harness);
    std::uint64_t actualUploadHash = 0;
    require(
        harness.controlExists(control) &&
        harness.uploadHash(control, mesh, object, &actualUploadHash) &&
        actualUploadHash == expectedUploadHash,
        "call/jump interpreted or corrupted a registered control", harness);
    harness.clearBuffer(commandBuffer);

    // A deep copy contains a plausible tag and copied owner pointers, but it
    // is not authoritative without a successful initialization registration.
    harness.copyBuffer(clone, control);
    require(
        harness.synchronize(),
        "general-poll barrier failed after copying a control buffer", harness);
    require(
        !harness.controlExists(clone),
        "copied control bytes were accepted as a live Pingo control", harness);
    harness.sendPingo(clone, 39);
    require(
        harness.synchronize(),
        "unregistered copied control disrupted command processing", harness);
    require(
        harness.controlExists(control),
        "copied-control rejection damaged the original control", harness);
    harness.clearBuffer(clone);

    // Reference-copy must not alias the live control block into another ID;
    // otherwise a generic mutation through the alias corrupts the original.
    harness.copyBufferReference(clone, control);
    harness.reverseBuffer(clone);
    require(
        harness.synchronize(),
        "general-poll barrier failed after reference-copy isolation", harness);
    actualUploadHash = 0;
    require(
        harness.controlExists(control) &&
        harness.uploadHash(control, mesh, object, &actualUploadHash) &&
        actualUploadHash == expectedUploadHash,
        "reference-copy alias corrupted the registered control", harness);
    harness.clearBuffer(clone);

    harness.sendPingo(control, 39);
    require(
        harness.synchronize(),
        "general-poll barrier failed after clone-isolation teardown", harness);
    require(
        harness.ownedAllocations() == baseline,
        "clone-isolation teardown leaked Pingo resources", harness);

    // Appending an ordinary block commits to repurposing the registered
    // buffer, so it must first release all external Pingo ownership.
    harness.createBitmap2222(texture, 4, 4, 0x55);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    harness.appendBuffer(control, {0xA5});
    require(
        harness.synchronize(),
        "general-poll barrier failed after appending to a control", harness);
    require(
        !harness.controlExists(control),
        "buffer append left a Pingo control registered", harness);
    require(
        harness.ownedAllocations() == baseline,
        "buffer append leaked external Pingo resources", harness);
    harness.sendPingo(control, 39);
    require(
        harness.synchronize(),
        "repurposed control bytes disrupted command processing", harness);
    harness.clearBuffer(control);
    harness.clearBuffer(texture);
    require(
        harness.synchronize(),
        "general-poll barrier failed after isolation cleanup", harness);

    // Spreading a source into itself used to keep a reference to a map value
    // across bufferClear(source), producing a generic iterator/use-after-free.
    // A live control exercises both that edge and external-resource teardown.
    harness.createBitmap2222(texture, 4, 4, 0xAA);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    harness.spreadBufferInto(control, control);
    require(
        harness.synchronize(),
        "self-spread disrupted command processing", harness);
    require(
        !harness.controlExists(control),
        "self-spread left a Pingo control registered", harness);
    require(
        harness.ownedAllocations() == baseline,
        "self-spread leaked Pingo resources", harness);
    harness.clearBuffer(control);
    harness.clearBuffer(texture);

    // A bitmap backed by the control block would let a render overwrite the
    // control structure itself. Match the scene dimensions to the exact
    // control byte size so this is an end-to-end regression of that route.
    auto controlBytes = harness.controlSize();
    require(
        controlBytes > 0 && controlBytes <= 0x7FFF,
        "userspace control size is outside bitmap dimensions", harness);
    harness.sendPingo(
        control, 0,
        {static_cast<std::uint16_t>(controlBytes), 1});
    harness.createBitmapFromBuffer(
        control, static_cast<std::uint16_t>(controlBytes), 1);
    harness.sendPingo(control, 38, {control});
    require(
        harness.synchronize(),
        "control-backed bitmap rejection disrupted command processing",
        harness);
    require(
        harness.controlExists(control),
        "bitmap wrapping/render target corrupted a registered control",
        harness);
    harness.sendPingo(control, 39);
    require(
        harness.synchronize(),
        "general-poll barrier failed after bitmap isolation teardown",
        harness);
    require(
        harness.ownedAllocations() == baseline,
        "bitmap-isolation teardown leaked Pingo resources", harness);
}

void testTruncatedUploads(Harness& harness, std::uint32_t baseline) {
    constexpr std::uint16_t control = 1103;
    constexpr std::uint16_t target = 370;
    constexpr std::uint16_t texture = 371;
    constexpr std::uint16_t token = 0x7B31;
    constexpr std::uint16_t mesh = 11;
    constexpr std::uint16_t object = 11;

    harness.createBitmap2222(target, 32, 32, 0);
    harness.createBitmap2222(texture, 4, 4, 0x33);
    harness.sendPingo(control, 0, {32, 32});
    populateObject(harness, control, object, mesh, texture);
    auto notification = harness.pingoPrefix(control, 41);
    notification.push_back(1);
    Harness::appendWord(notification, token);
    harness.sendBytes(notification);
    require(
        harness.synchronize(),
        "general-poll barrier failed before truncated-upload tests",
        harness);

    std::uint64_t expectedUploadHash = 0;
    require(
        harness.uploadHash(
            control, mesh, object, &expectedUploadHash),
        "could not capture valid upload state", harness);
    auto allocationsWithUploads = harness.ownedAllocations();
    auto requirePreservedState = [&](const char * failure) {
        std::uint64_t actual = 0;
        require(
            harness.uploadHash(control, mesh, object, &actual) &&
                actual == expectedUploadHash,
            failure, harness);
        require(
            harness.ownedAllocations() == allocationsWithUploads,
            "failed replacement changed Pingo allocation ownership",
            harness);
    };

    auto recover = [&](std::vector<std::uint8_t> truncated,
                       std::uint16_t sequence,
                       const char * failure) {
        harness.sendBytes(truncated);
        // The stream is count-delimited. A fresh command is recoverable only
        // after the truncated command has reached its field timeout.
        harness.settle(std::chrono::milliseconds(450));
        harness.sendPingo(control, 38, {target});
        require(
            harness.waitForCompletion(token, sequence),
            failure, harness);
        requirePreservedState(
            "truncated replacement changed previously valid upload state");
    };

    auto truncated = harness.pingoPrefix(control, 1);
    Harness::appendWord(truncated, mesh);
    Harness::appendWord(truncated, 2);
    Harness::appendWord(truncated, 0x1234);
    recover(truncated, 0, "vertex upload repeated timeouts or lost alignment");

    truncated = harness.pingoPrefix(control, 2);
    Harness::appendWord(truncated, mesh);
    Harness::appendWord(truncated, 6);
    Harness::appendWord(truncated, 0);
    recover(truncated, 1, "vertex-index upload repeated timeouts or lost alignment");

    truncated = harness.pingoPrefix(control, 3);
    Harness::appendWord(truncated, mesh);
    Harness::appendWord(truncated, 2);
    Harness::appendWord(truncated, 0x1234);
    recover(truncated, 2, "mesh-UV upload repeated timeouts or lost alignment");

    truncated = harness.pingoPrefix(control, 4);
    Harness::appendWord(truncated, mesh);
    Harness::appendWord(truncated, 6);
    truncated.push_back(0x34);
    recover(truncated, 3, "UV-index mid-word timeout did not recover");

    truncated = harness.pingoPrefix(control, 40);
    Harness::appendWord(truncated, object);
    Harness::appendWord(truncated, 2);
    Harness::appendWord(truncated, 0x1234);
    recover(truncated, 4, "object-UV upload repeated timeouts or lost alignment");

    // The largest protocol count must still abandon the first absent field
    // after one timeout, not repeat 65,535 timeouts.
    truncated = harness.pingoPrefix(control, 1);
    Harness::appendWord(truncated, mesh);
    Harness::appendWord(truncated, 0xFFFF);
    recover(truncated, 5, "maximum-count truncation did not recover promptly");

    // Allocation rejection must drain a complete declared payload so an
    // immediately following render remains aligned. Exercise every staged
    // component replacement while preserving the prior valid state.
    const std::vector<std::pair<
        std::uint8_t, std::vector<std::uint16_t>>> replacements = {
        {1, {mesh, 1, 0x1000, 0x2000, 0x3000}},
        {2, {mesh, 3, 2, 1, 0}},
        {3, {mesh, 1, 0x1000, 0x2000}},
        {4, {mesh, 3, 2, 1, 0}},
        {40, {object, 1, 0x1000, 0x2000}},
    };
    std::uint16_t sequence = 6;
    for (const auto& replacement : replacements) {
        harness.failAllocationAfter(0);
        harness.sendPingo(
            control, replacement.first, replacement.second);
        harness.sendPingo(control, 38, {target});
        require(
            harness.waitForCompletion(token, sequence++),
            "allocation-failed upload did not drain its complete payload",
            harness);
        harness.failAllocationAfter(-1);
        requirePreservedState(
            "allocation-failed replacement changed valid upload state");
    }

    // Allocation rejection uses the same bounded drain helper: when the
    // declared payload itself is truncated, it must stop after one timeout.
    harness.failAllocationAfter(0);
    truncated = harness.pingoPrefix(control, 1);
    Harness::appendWord(truncated, mesh);
    Harness::appendWord(truncated, 0xFFFF);
    Harness::appendWord(truncated, 0x1234);
    recover(
        truncated, sequence,
        "allocation-failed truncated upload did not recover promptly");
    harness.failAllocationAfter(-1);

    harness.sendPingo(control, 39);
    require(
        harness.synchronize(),
        "general-poll barrier failed after truncated-upload teardown",
        harness);
    require(
        harness.ownedAllocations() == baseline,
        "truncated-upload teardown leaked Pingo resources", harness);
}

} // namespace

int main(int argc, char ** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s VDP_SO\n", argv[0]);
        return 2;
    }

    Harness harness(argv[1]);
    harness.setup();
    require(
        harness.synchronize(),
        "initial general-poll handshake failed", harness);
    harness.failAllocationAfter(-1);

    auto baseline = harness.ownedAllocations();
    testScaleSetters(harness, baseline);
    testWideObjectTranslation(harness, baseline);
    testProjectionFarPlane(harness, baseline);
    testLightingAndShadingCommands(harness, baseline);
    testFlatPatternLibrary(harness, baseline);
    testStreamingMeshReplacementAndVisibility(harness, baseline);
    testUnboundedObjectTraversalAndHighIds(harness, baseline);
    testAtomicInitialization(harness, baseline);
    testTeardownAndTextureLifetime(harness, baseline);
    testRegisteredControlIsolation(harness, baseline);
    testTruncatedUploads(harness, baseline);

    require(
        harness.ownedAllocations() == baseline,
        "Pingo robustness suite ended with owned allocations", harness);
    std::puts("Pingo bridge robustness test passed");
    harness.shutdown();
    return 0;
}
