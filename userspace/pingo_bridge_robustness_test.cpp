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
    bool (*objectScale)(std::uint16_t, std::uint16_t, float *);
    bool (*sceneScale)(std::uint16_t, float *);
    bool (*uploadHash)(
        std::uint16_t, std::uint16_t, std::uint16_t, std::uint64_t *);
    bool (*texturePixel)(
        std::uint16_t, std::uint16_t, std::uint32_t, std::uint8_t *);
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
          objectScale(nullptr),
          sceneScale(nullptr),
          uploadHash(nullptr),
          texturePixel(nullptr),
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
        objectScale = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, float *)>(
            handle, "pingo_userspace_get_object_scale");
        sceneScale = loadSymbol<bool (*)(std::uint16_t, float *)>(
            handle, "pingo_userspace_get_scene_scale");
        uploadHash = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint16_t, std::uint64_t *)>(
            handle, "pingo_userspace_get_upload_state_hash");
        texturePixel = loadSymbol<bool (*)(
            std::uint16_t, std::uint16_t, std::uint32_t, std::uint8_t *)>(
            handle, "pingo_userspace_get_object_texture_pixel");
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
