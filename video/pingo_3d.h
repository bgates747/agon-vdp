#ifndef PINGO_3D_H
#define PINGO_3D_H

#ifndef PINGO_RENDER_DIAGNOSTICS
#define PINGO_RENDER_DIAGNOSTICS 0
#endif

#ifndef PINGO_RENDER_TARGET_HASH
#define PINGO_RENDER_TARGET_HASH 0
#endif

#ifndef PINGO_RENDER_TARGET_DUMP
#define PINGO_RENDER_TARGET_DUMP 0
#endif

#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <agon.h>
#include <map>
#include <memory>
#include <new>
#include <type_traits>
#ifdef USERSPACE
#include <atomic>
#include <chrono>
#if PINGO_RENDER_TARGET_DUMP
#include <stdio.h>
#include <stdlib.h>
#endif
#else
#include <esp_timer.h>
#if PINGO_RENDER_DIAGNOSTICS
#include <xtensa/hal.h>
#endif
#endif
#include "esp_heap_caps.h"
#include "buffer_stream.h"
#include "sprites.h"

#ifdef USERSPACE
static std::atomic<int32_t> pingo_userspace_allocation_failure_countdown{-1};
static std::atomic<uint32_t> pingo_userspace_owned_allocations{0};

extern "C" void pingo_userspace_fail_allocation_after(
        int32_t successful_allocations) {
    pingo_userspace_allocation_failure_countdown.store(
        successful_allocations);
}

extern "C" uint32_t pingo_userspace_get_owned_allocation_count() {
    return pingo_userspace_owned_allocations.load();
}
#endif

static bool pingo_allocation_permitted() {
#ifdef USERSPACE
    auto countdown = pingo_userspace_allocation_failure_countdown.load();
    if (countdown < 0) {
        return true;
    }
    if (countdown == 0) {
        return false;
    }
    pingo_userspace_allocation_failure_countdown.fetch_sub(1);
#endif
    return true;
}

static void * pingo_owned_malloc(size_t size) {
    if (!pingo_allocation_permitted()) {
        return nullptr;
    }
    void * allocation = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
#ifdef USERSPACE
    if (allocation) {
        pingo_userspace_owned_allocations.fetch_add(1);
    }
#endif
    return allocation;
}

static void pingo_owned_free(void * allocation) {
    if (!allocation) {
        return;
    }
    heap_caps_free(allocation);
#ifdef USERSPACE
    pingo_userspace_owned_allocations.fetch_sub(1);
#endif
}

template<typename T>
static T * pingo_owned_new() {
    if (!pingo_allocation_permitted()) {
        return nullptr;
    }
    T * allocation = new (std::nothrow) T;
#ifdef USERSPACE
    if (allocation) {
        pingo_userspace_owned_allocations.fetch_add(1);
    }
#endif
    return allocation;
}

template<typename T>
static void pingo_owned_delete(T * allocation) {
    if (!allocation) {
        return;
    }
    delete allocation;
#ifdef USERSPACE
    pingo_userspace_owned_allocations.fetch_sub(1);
#endif
}

static uint64_t pingo_render_clock_us() {
#ifdef USERSPACE
    using clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::microseconds>(
        clock::now().time_since_epoch()).count();
#else
    return (uint64_t) esp_timer_get_time();
#endif
}

#if defined(USERSPACE) && PINGO_RENDER_TARGET_HASH
static uint64_t pingo_fnv1a64(
        const uint8_t * data, uint32_t byte_count) {
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t i = 0; i < byte_count; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}
#endif

#if defined(USERSPACE) && PINGO_RENDER_TARGET_DUMP
static bool pingo_target_dump_selected(uint16_t bmid, uint32_t sequence) {
    const char * cursor = getenv("PINGO_RENDER_TARGET_DUMP_FRAMES");
    if (cursor == nullptr || *cursor == '\0') {
        return false;
    }

    while (*cursor != '\0') {
        char * end = nullptr;
        unsigned long selected_bmid = strtoul(cursor, &end, 10);
        if (end == cursor || *end != ':') {
            return false;
        }

        cursor = end + 1;
        unsigned long selected_sequence = strtoul(cursor, &end, 10);
        if (end == cursor || (*end != ',' && *end != '\0')) {
            return false;
        }

        if (selected_bmid == bmid && selected_sequence == sequence) {
            return true;
        }
        cursor = *end == ',' ? end + 1 : end;
    }
    return false;
}

static bool pingo_target_dump_bytes(
        const char * directory,
        uint16_t bmid,
        uint32_t sequence,
        const char * suffix,
        const void * data,
        uint32_t byte_count) {
    char path[1024];
    int length = snprintf(
        path, sizeof(path), "%s/pingo-%u-%u.%s",
        directory, bmid, sequence, suffix);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        return false;
    }

    FILE * output = fopen(path, "wb");
    if (output == nullptr) {
        return false;
    }
    bool success =
        fwrite(data, 1, byte_count, output) == byte_count &&
        fclose(output) == 0;
    return success;
}

static void pingo_dump_render_target(
        uint16_t bmid,
        uint32_t sequence,
        const void * target,
        uint32_t target_bytes,
        bool rgba2222,
        const void * zeta,
        uint32_t zeta_bytes) {
    if (!pingo_target_dump_selected(bmid, sequence)) {
        return;
    }

    const char * directory = getenv("PINGO_RENDER_TARGET_DUMP_DIR");
    bool success = directory != nullptr && *directory != '\0' &&
        pingo_target_dump_bytes(
            directory, bmid, sequence,
            rgba2222 ? "rgba2" : "rgba8",
            target, target_bytes) &&
        pingo_target_dump_bytes(
            directory, bmid, sequence, "z32le",
            zeta, zeta_bytes);
    force_debug_log(
        "PINGO_DUMP seq=%u bmid=%u status=%s\n",
        sequence, bmid, success ? "ok" : "failed");
}
#endif

#if PINGO_RENDER_DIAGNOSTICS
/*
 * Detailed renderer diagnostics use a cheap wrapping tick source so that
 * phase boundaries do not pay the much larger cost of a formatted log or an
 * ESP timer conversion. Each individually measured clear or per-triangle
 * phase must take less than one 32-bit wrap (approximately 17.9 seconds at
 * 240 MHz); frame totals accumulate in 64 bits.
 */
static uint32_t pingo_render_diagnostics_clock_ticks() {
#ifdef USERSPACE
    using clock = std::chrono::steady_clock;
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(
        clock::now().time_since_epoch()).count();
#else
    return (uint32_t)xthal_get_ccount();
#endif
}

static uint32_t pingo_render_diagnostics_clock_hz() {
#ifdef USERSPACE
    return 1000000U;
#else
    return (uint32_t)F_CPU;
#endif
}

static uint32_t pingo_render_diagnostics_elapsed_us(
        uint64_t started, uint64_t finished) {
    uint64_t elapsed = finished - started;
    return elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
}

static uint32_t pingo_render_diagnostics_ticks_to_us(
        uint64_t ticks, uint32_t clock_hz) {
    if (!clock_hz) {
        return 0;
    }
    uint64_t microseconds =
        (ticks / clock_hz) * 1000000ULL +
        ((ticks % clock_hz) * 1000000ULL + clock_hz / 2) / clock_hz;
    return microseconds > UINT32_MAX
        ? UINT32_MAX
        : (uint32_t)microseconds;
}
#endif

namespace p3d {

    extern "C" {

        #include "pingo/render/mesh.h"
        #include "pingo/render/object.h"
        #include "pingo/render/pixel.h"
        #include "pingo/render/renderer.h"
        #include "pingo/render/scene.h"
        #include "pingo/render/backend.h"
        #include "pingo/render/depth.h"

    } // extern "C"

} // namespace p3d

#define PINGO_3D_CONTROL_TAG    0x43443350 // "P3DC"
#define PINGO_RENDER_NOTIFY_DISABLED 0
#define PINGO_RENDER_NOTIFY_KEYCODE  1
#define PINGO_RENDER_NOTIFY_VERSION  1
#define PINGO_RENDER_NOTIFY_COMPLETE 1
#define PINGO_PROJECTION_NEAR_UNITS 1.0f
#define PINGO_PROJECTION_FAR_DEFAULT_UNITS 2500
#define PINGO_PROJECTION_FAR_MIN_UNITS 2

class VDUStreamProcessor;

typedef struct tag_Transformable {
    p3d::Vec3f      m_scale;
    p3d::Vec3f      m_rotation;
    p3d::Vec3f      m_translation;
    p3d::Mat4       m_transform;
    bool            m_modified;

    void initialize_scale() {
        m_scale.x = 1.0f;
        m_scale.y = 1.0f;
        m_scale.z = 1.0f;
        m_modified = true;
    }

    void initialize() {
        memset(this, 0, sizeof(struct tag_Transformable));
        initialize_scale();
    }

    void compute_transformation_matrix() {
        m_transform = p3d::mat4Scale(m_scale);
        if (m_rotation.x) {
            auto t = p3d::mat4RotateX(m_rotation.x);
            m_transform = mat4MultiplyM(&m_transform, &t);
        }
        if (m_rotation.y) {
            auto t = p3d::mat4RotateY(m_rotation.y);
            m_transform = mat4MultiplyM(&m_transform, &t);
        }
        if (m_rotation.z) {
            auto t = p3d::mat4RotateZ(m_rotation.z);
            m_transform = mat4MultiplyM(&m_transform, &t);
        }
        if (m_translation.x || m_translation.y || m_translation.z) {
            auto t = p3d::mat4Translate(m_translation);
            m_transform = mat4MultiplyM(&m_transform, &t);
        }
        m_modified = false;
    }

    void dump() {
        for (int i = 0; i < 16; i++) {
            debug_log("        [%i] %f\n", i, m_transform.elements[i]);
        }
        debug_log("Scale: %f %f %f\n", m_scale.x, m_scale.y, m_scale.z);
        debug_log("Rotation: %f %f %f\n", m_rotation.x, m_rotation.y, m_rotation.z);
        debug_log("Translation: %f %f %f\n", m_translation.x, m_translation.y, m_translation.z);
    }
} Transformable;

typedef struct tag_PingoTextureBinding {
    /*
     * VDP bitmaps normally borrow their pixels from a BufferStream. Retain
     * both owners: keeping only the Bitmap wrapper alive would not keep its
     * non-owning data pointer valid after the source buffer is cleared.
     */
    std::shared_ptr<Bitmap> m_bitmap;
    std::shared_ptr<BufferStream> m_storage;
} PingoTextureBinding;

typedef struct tag_TexObject : public Transformable {
    p3d::Object     m_object;
    p3d::Texture    m_texture;
    p3d::Material   m_material;
    uint16_t        m_oid;
    PingoTextureBinding* m_texture_binding;

    void bind() {
        m_object.material = &m_material;
        m_material.texture = &m_texture;
    }

    void initialize() {
        Transformable::initialize();
        bind();
    }

    void update_transformation_matrix() {
        compute_transformation_matrix();
        m_object.transform = m_transform;
    }

    void dump() {
        Transformable::dump();
        debug_log("TObject: %p %u\n", this, m_oid);
        debug_log("Object: %p %p %p %p\n", &m_object, m_object.material, m_object.mesh,
                    m_object.transform.elements);
        debug_log("Texture: %p %u %u %p\n", &m_texture, m_texture.size.x, m_texture.size.y, m_texture.frameBuffer);
        debug_log("Material: %p %p %u %u %p\n", &m_material, m_material.texture, m_material.texture->size.x,
                    m_material.texture->size.y, m_material.texture->frameBuffer);
    }
} TexObject;

typedef struct tag_PingoFlatPatternBinding {
    /*
     * Immutable private snapshot of client-uploaded generic buffers. Generic
     * buffered operations can mutate their source blocks in place after a
     * bind, so borrowing those bytes would invalidate the accepted lookup.
     */
    uint8_t* m_storage;
    p3d::PingoFlatPatternLibrary m_library;
} PingoFlatPatternBinding;

struct tag_Pingo3dControl;

extern "C" {

    void static_init(p3d::Renderer* ren, p3d::BackEnd* backEnd, p3d::Vec4i _rect);

    void static_before_render(p3d::Renderer* ren, p3d::BackEnd* backEnd);

    void static_after_render(p3d::Renderer* ren, p3d::BackEnd* backEnd);

    p3d::Pixel* static_get_frame_buffer(p3d::Renderer* ren, p3d::BackEnd* backEnd);

    p3d::PingoDepth* static_get_zeta_buffer(p3d::Renderer* ren, p3d::BackEnd* backEnd);

} // extern "C"

typedef struct tag_Pingo3dControl {
    uint32_t            m_tag;              // Used to verify the existence of this structure
    uint32_t            m_size;             // Used to verify the existence of this structure
    VDUStreamProcessor* m_proc;             // Used by subcommands to obtain more data
    p3d::BackEnd        m_backend;          // Used by the renderer
    p3d::Pixel*         m_frame;            // Frame buffer for rendered pixels
    p3d::PingoDepth*    m_zeta;             // Zeta buffer for depth information
    uint16_t            m_width;            // Width of final render in pixels
    uint16_t            m_height;           // Height of final render in pixels
    uint16_t            m_projection_far_units; // Per-control perspective far plane
    Transformable       m_camera;           // Camera transformation settings
    Transformable       m_scene;            // Scene transformation settings
    std::map<uint16_t, p3d::Mesh>* m_meshes;    // Map of meshes for use by objects
    std::map<uint16_t, TexObject>* m_objects;   // Map of textured objects that use meshes and have transforms
    uint32_t            m_render_sequence;  // Diagnostic sequence for render timing records
    uint8_t             m_render_notify_mode;   // Opt-in render-completion transport
    uint16_t            m_render_notify_token;  // Caller-supplied completion token
    p3d::Vec3f          m_light_direction;  // Scene-wide normalized directional light
    uint8_t             m_light_intensity;  // 127 is unity; 128..255 overdrive
    uint8_t             m_ambient_light;    // Minimum shade; 127 is unity
    uint8_t             m_illumination_enabled; // Zero writes native texture colors
    PingoFlatPatternBinding* m_flat_pattern_binding;

    void show_free_ram() {
        debug_log("Free PSRAM: %u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }

    // VDU 23, 0, &A0, sid; &48, 0, 1 :  Initialize Control Structure
    bool initialize(VDUStreamProcessor& processor, uint16_t width, uint16_t height) {
        debug_log("initialize: pingo creating control structure for %ux%u scene\n", width, height);
        memset(this, 0, sizeof(tag_Pingo3dControl));

        /*
         * Bitmap dimensions are signed 16-bit in vdp-gl. Reject values that
         * cannot be represented there, and check every byte-size multiply
         * before allocating. In particular, a valid uint16_t width*height can
         * still overflow when multiplied by the 32-bit depth element size.
         */
        if (!width || !height || width > INT16_MAX || height > INT16_MAX) {
            debug_log("initialize: invalid dimensions %ux%u\n", width, height);
            return false;
        }
        size_t frame_size = (size_t)width * (size_t)height;
        if (frame_size > SIZE_MAX / sizeof(p3d::Pixel) ||
            frame_size > SIZE_MAX / sizeof(p3d::PingoDepth) ||
            frame_size * sizeof(p3d::Pixel) > UINT32_MAX ||
            frame_size * sizeof(p3d::PingoDepth) > UINT32_MAX) {
            debug_log("initialize: dimensions %ux%u overflow buffer sizes\n",
                width, height);
            return false;
        }

        size_t frame_bytes = frame_size * sizeof(p3d::Pixel);
        size_t zeta_bytes = frame_size * sizeof(p3d::PingoDepth);
        auto frame = (p3d::Pixel*)pingo_owned_malloc(frame_bytes);
        auto zeta = (p3d::PingoDepth*)pingo_owned_malloc(zeta_bytes);
        auto meshes = pingo_owned_new<std::map<uint16_t, p3d::Mesh>>();
        auto objects = pingo_owned_new<std::map<uint16_t, TexObject>>();

        if (!frame || !zeta || !meshes || !objects) {
            debug_log(
                "initialize: failed to allocate control resources for %ux%u\n",
                width, height);
            show_free_ram();
            pingo_owned_delete(objects);
            pingo_owned_delete(meshes);
            pingo_owned_free(zeta);
            pingo_owned_free(frame);
            return false;
        }

        m_width = width;
        m_height = height;
        m_projection_far_units = PINGO_PROJECTION_FAR_DEFAULT_UNITS;
        m_proc = &processor;
        m_frame = frame;
        m_zeta = zeta;
        m_meshes = meshes;
        m_objects = objects;
        m_camera.initialize();
        m_scene.initialize();
        m_light_direction = (p3d::Vec3f){
            0.0f, 0.7071067811865475244f, -0.7071067811865475244f
        };
        m_light_intensity = 127;
        m_ambient_light = 0;
        m_illumination_enabled = 1;

        m_backend.init = &static_init;
        m_backend.beforeRender = &static_before_render;
        m_backend.afterRender = &static_after_render;
        m_backend.getFrameBuffer = &static_get_frame_buffer;
        m_backend.getZetaBuffer = &static_get_zeta_buffer;
        m_backend.drawPixel = NULL;
        m_backend.clientCustomData = (void*) this;

        // Publish validity only after every owned resource is ready.
        m_size = sizeof(tag_Pingo3dControl);
        m_tag = PINGO_3D_CONTROL_TAG;
        return true;
    }

    void release_flat_pattern_binding() {
        if (!m_flat_pattern_binding) {
            return;
        }
        pingo_owned_free(m_flat_pattern_binding->m_storage);
        m_flat_pattern_binding->m_storage = nullptr;
        pingo_owned_delete(m_flat_pattern_binding);
        m_flat_pattern_binding = nullptr;
    }

    // VDU 23, 0, &A0, sid; &48, 0, 0 :  Deinitialize Control Structure
    void deinitialize(VDUStreamProcessor& processor) {
        (void)processor;

        // Invalidate first so a repeated teardown is harmless.
        m_tag = 0;
        m_size = 0;

        release_flat_pattern_binding();

        if (m_objects) {
            for (auto& entry : *m_objects) {
                pingo_owned_free(entry.second.m_object.textCoord);
                entry.second.m_object.textCoord = nullptr;
                entry.second.m_object.textCoord_count = 0;
                pingo_owned_delete(entry.second.m_texture_binding);
                entry.second.m_texture_binding = nullptr;
            }
        }

        if (m_meshes) {
            for (auto& entry : *m_meshes) {
                auto& mesh = entry.second;
                pingo_owned_free(mesh.positions);
                pingo_owned_free(mesh.pos_indices);
                pingo_owned_free(mesh.textCoord);
                pingo_owned_free(mesh.tex_indices);
                memset(&mesh, 0, sizeof(mesh));
            }
        }

        pingo_owned_delete(m_objects);
        pingo_owned_delete(m_meshes);
        pingo_owned_free(m_zeta);
        pingo_owned_free(m_frame);

        m_objects = nullptr;
        m_meshes = nullptr;
        m_zeta = nullptr;
        m_frame = nullptr;
        m_width = 0;
        m_height = 0;
        m_proc = nullptr;
        memset(&m_backend, 0, sizeof(m_backend));
    }

    bool validate() {
        return (m_tag == PINGO_3D_CONTROL_TAG &&
                m_size == sizeof(tag_Pingo3dControl) &&
                m_width && m_height && m_frame && m_zeta &&
                m_meshes && m_objects);
    }

    bool is_registered_as(uint16_t buffer_id) {
        return m_proc && isPingo3dControlBuffer(buffer_id);
    }

    void handle_subcommand(VDUStreamProcessor& processor, uint8_t subcmd) {
        //debug_log("P3D: handle_subcommand(%hu)\n", subcmd);
        m_proc = &processor;
        switch (subcmd) {
            case 1: define_mesh_vertices(); break;
            case 2: set_mesh_vertex_indexes(); break;
            case 3: define_mesh_texture_coordinates(); break;
            case 4: set_texture_coordinate_indexes(); break;
            case 5: create_object(); break;
            case 40: define_object_texture_coordinates(); break;
            case 6: set_object_x_scale_factor(); break;
            case 7: set_object_y_scale_factor(); break;
            case 8: set_object_z_scale_factor(); break;
            case 9: set_object_xyz_scale_factors(); break;
            case 10: set_object_x_rotation_angle(); break;
            case 11: set_object_y_rotation_angle(); break;
            case 12: set_object_z_rotation_angle(); break;
            case 13: set_object_xyz_rotation_angles(); break;
            case 14: set_object_x_translation_distance(); break;
            case 15: set_object_y_translation_distance(); break;
            case 16: set_object_z_translation_distance(); break;
            case 17: set_object_xyz_translation_distances(); break;
            case 18: set_camera_x_rotation_angle(); break;
            case 19: set_camera_y_rotation_angle(); break;
            case 20: set_camera_z_rotation_angle(); break;
            case 21: set_camera_xyz_rotation_angles(); break;
            case 22: set_camera_x_translation_distance(); break;
            case 23: set_camera_y_translation_distance(); break;
            case 24: set_camera_z_translation_distance(); break;
            case 25: set_camera_xyz_translation_distances(); break;
            case 26: set_scene_x_scale_factor(); break;
            case 27: set_scene_y_scale_factor(); break;
            case 28: set_scene_z_scale_factor(); break;
            case 29: set_scene_xyz_scale_factors(); break;
            case 30: set_scene_x_rotation_angle(); break;
            case 31: set_scene_y_rotation_angle(); break;
            case 32: set_scene_z_rotation_angle(); break;
            case 33: set_scene_xyz_rotation_angles(); break;
            case 34: set_scene_x_translation_distance(); break;
            case 35: set_scene_y_translation_distance(); break;
            case 36: set_scene_z_translation_distance(); break;
            case 37: set_scene_xyz_translation_distances(); break;
            case 38: render_to_bitmap(); break;
            case 41: set_render_notification(); break;
            case 43: set_light_direction(); break;
            case 44: set_light_intensity(); break;
            case 45: set_ambient_light(); break;
            case 46: set_illumination_enabled(); break;
            case 47: set_mesh_shading_mode(); break;
            case 48: set_mesh_illumination_policy(); break;
            case 49: set_flat_pattern_library(); break;
            case 50: replace_mesh_from_buffer(); break;
            case 51: set_object_active(); break;
            case 52: set_object_xyz_translation_distances_wide(); break;
            case 53: set_projection_far_distance(); break;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 41, mode, token;
    // mode 0 disables notification; mode 1 emits a stock MOS keyboard packet.
    void set_render_notification() {
        auto mode = m_proc->readByte_t();
        auto token = m_proc->readWord_t();
        if (mode < 0 || token < 0) {
            return;
        }
        m_render_notify_mode =
            mode == PINGO_RENDER_NOTIFY_KEYCODE
                ? PINGO_RENDER_NOTIFY_KEYCODE
                : PINGO_RENDER_NOTIFY_DISABLED;
        m_render_notify_token = (uint16_t)token;
    }

    void send_render_complete(uint32_t sequence) {
        if (m_render_notify_mode != PINGO_RENDER_NOTIFY_KEYCODE) {
            return;
        }

        // Keep the complete wire frame below the eZ80's 16-byte UART FIFO.
        // Twelve bytes matches the largest stock VDP event (mouse packet).
        uint8_t packet[10] = {
            'P', '3', 'D', 'R',
            PINGO_RENDER_NOTIFY_VERSION,
            PINGO_RENDER_NOTIFY_COMPLETE,
            (uint8_t)(m_render_notify_token & 0xFF),
            (uint8_t)(m_render_notify_token >> 8),
            (uint8_t)(sequence & 0xFF),
            (uint8_t)((sequence >> 8) & 0xFF),
        };
        m_proc->send_packet(PACKET_KEYCODE, sizeof(packet), packet);
    }

    // VDU 23, 0, &A0, sid; &49, 43, x; y; z;
    // Signed 16-bit components describe a direction ratio and are normalized
    // once here. A zero vector is rejected without changing the current light.
    void set_light_direction() {
        auto x = m_proc->readWord_t();
        if (x < 0) {
            return;
        }
        auto y = m_proc->readWord_t();
        if (y < 0) {
            return;
        }
        auto z = m_proc->readWord_t();
        if (z < 0) {
            return;
        }
        p3d::Vec3f candidate = {
            (p3d::F_TYPE)(int16_t)(uint16_t)x,
            (p3d::F_TYPE)(int16_t)(uint16_t)y,
            (p3d::F_TYPE)(int16_t)(uint16_t)z
        };
        float magnitude_squared = p3d::vec3Dot(candidate, candidate);
        if (!(magnitude_squared > 0.0f) || !isfinite(magnitude_squared)) {
            return;
        }
        m_light_direction = p3d::vec3Normalize(candidate);
    }

    // VDU 23, 0, &A0, sid; &49, 44, intensity
    // 127 is unity; larger values deliberately overdrive toward saturation.
    void set_light_intensity() {
        auto intensity = m_proc->readByte_t();
        if (intensity >= 0) {
            m_light_intensity = (uint8_t)intensity;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 45, ambient
    // Ambient is a minimum shade floor using the same 127-unity scale.
    void set_ambient_light() {
        auto ambient = m_proc->readByte_t();
        if (ambient >= 0) {
            m_ambient_light = (uint8_t)ambient;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 46, enabled
    void set_illumination_enabled() {
        auto enabled = m_proc->readByte_t();
        if (enabled == 0 || enabled == 1) {
            m_illumination_enabled = (uint8_t)enabled;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 47, mesh_id; mode
    // Mode 0 is textured; 1 is one palette color per face; 2 is one uploaded
    // 4x4 flat pattern per material/illumination pair.
    void set_mesh_shading_mode() {
        auto mesh_id = m_proc->readWord_t();
        if (mesh_id < 0) {
            return;
        }
        auto mode = m_proc->readByte_t();
        if (mode != p3d::MESH_SHADING_TEXTURED &&
            mode != p3d::MESH_SHADING_FLAT_PALETTE &&
            mode != p3d::MESH_SHADING_FLAT_PATTERN) {
            return;
        }
        auto mesh = establish_mesh((uint16_t)mesh_id);
        if (mesh) {
            mesh->shading_mode = (uint8_t)mode;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 48, mesh_id; mode
    // Mode 0 inherits scene lighting; mode 1 emits native mesh colors.
    void set_mesh_illumination_policy() {
        auto mesh_id = m_proc->readWord_t();
        if (mesh_id < 0) {
            return;
        }
        auto mode = m_proc->readByte_t();
        if (mode != p3d::MESH_ILLUMINATION_INHERIT_SCENE &&
            mode != p3d::MESH_ILLUMINATION_SELF_ILLUMINATED) {
            return;
        }
        auto mesh = establish_mesh((uint16_t)mesh_id);
        if (mesh) {
            mesh->illumination_policy = (uint8_t)mode;
        }
    }

    /*
     * VDU 23,0,&A0,sid; &49,49,
     *     pattern_buffer; lookup_buffer; pattern_count;
     *     material_count, illumination_band_count
     *
     * Both source buffers must already be consolidated into one exact-sized
     * block. A completely zeroed payload clears the binding. Every other
     * candidate is validated and privately copied before the accepted
     * binding changes, so malformed input and allocation failure preserve the
     * previous usable resource.
     */
    void set_flat_pattern_library() {
        auto pattern_buffer_id = m_proc->readWord_t();
        auto lookup_buffer_id = m_proc->readWord_t();
        auto pattern_count = m_proc->readWord_t();
        auto material_count = m_proc->readByte_t();
        auto illumination_band_count = m_proc->readByte_t();
        if (pattern_buffer_id < 0 || lookup_buffer_id < 0 ||
            pattern_count < 0 || material_count < 0 ||
            illumination_band_count < 0) {
            return;
        }

        bool clear = pattern_buffer_id == 0 && lookup_buffer_id == 0 &&
            pattern_count == 0 && material_count == 0 &&
            illumination_band_count == 0;
        if (clear) {
            release_flat_pattern_binding();
            return;
        }

        if (pattern_buffer_id == 0 || lookup_buffer_id == 0 ||
            pattern_buffer_id == 65535 || lookup_buffer_id == 65535 ||
            pattern_buffer_id == lookup_buffer_id ||
            pattern_count < 1 ||
            pattern_count > p3d::PINGO_FLAT_PATTERN_MAX_PATTERNS ||
            material_count < 1 || illumination_band_count < 2 ||
            isPingo3dControlBuffer((uint16_t)pattern_buffer_id) ||
            isPingo3dControlBuffer((uint16_t)lookup_buffer_id)) {
            debug_log("set_flat_pattern_library: invalid IDs or counts\n");
            return;
        }

        size_t patterns_size =
            (size_t)pattern_count * p3d::PINGO_FLAT_PATTERN_PIXELS;
        size_t lookup_size =
            (size_t)material_count * (size_t)illumination_band_count;
        auto patterns_iter = buffers.find((uint16_t)pattern_buffer_id);
        auto lookup_iter = buffers.find((uint16_t)lookup_buffer_id);
        if (patterns_iter == buffers.end() ||
            lookup_iter == buffers.end() ||
            patterns_iter->second.size() != 1 ||
            lookup_iter->second.size() != 1 ||
            !patterns_iter->second.front() ||
            !lookup_iter->second.front() ||
            patterns_iter->second.front()->size() != patterns_size ||
            lookup_iter->second.front()->size() != lookup_size ||
            !patterns_iter->second.front()->getBuffer() ||
            !lookup_iter->second.front()->getBuffer()) {
            debug_log("set_flat_pattern_library: source buffers are not exact consolidated resources\n");
            return;
        }

        const uint8_t* patterns =
            patterns_iter->second.front()->getBuffer();
        const uint8_t* lookup =
            lookup_iter->second.front()->getBuffer();
        for (size_t i = 0; i < patterns_size; i++) {
            if ((patterns[i] & 0xC0) != 0xC0) {
                debug_log("set_flat_pattern_library: pattern pixels must be opaque RGBA2222\n");
                return;
            }
        }
        for (size_t i = 0; i < lookup_size; i++) {
            if (lookup[i] >= pattern_count) {
                debug_log("set_flat_pattern_library: lookup pattern ID is out of range\n");
                return;
            }
        }

        auto replacement = pingo_owned_new<PingoFlatPatternBinding>();
        if (!replacement) {
            debug_log("set_flat_pattern_library: could not allocate binding\n");
            return;
        }
        replacement->m_storage =
            (uint8_t*)pingo_owned_malloc(patterns_size + lookup_size);
        if (!replacement->m_storage) {
            pingo_owned_delete(replacement);
            debug_log("set_flat_pattern_library: could not copy resource\n");
            return;
        }
        memcpy(replacement->m_storage, patterns, patterns_size);
        memcpy(
            replacement->m_storage + patterns_size,
            lookup, lookup_size);
        replacement->m_library = (p3d::PingoFlatPatternLibrary){
            replacement->m_storage,
            (uint32_t)patterns_size,
            replacement->m_storage + patterns_size,
            (uint32_t)lookup_size,
            (uint16_t)pattern_count,
            (uint8_t)material_count,
            (uint8_t)illumination_band_count
        };

        auto previous = m_flat_pattern_binding;
        m_flat_pattern_binding = replacement;
        if (previous) {
            pingo_owned_free(previous->m_storage);
            previous->m_storage = nullptr;
            pingo_owned_delete(previous);
        }
    }

    /*
     * VDU 23,0,&A0,sid; &49,50,
     *     stage_bmid; mid; vertex_count; position_index_count;
     *     texture_coordinate_count; texture_index_count;
     *
     * The source is one exact consolidated generic-buffer block containing,
     * in order, packed little-endian signed-s16 XYZ positions, u16 position
     * indices, u16 UV pairs, and u16 UV indices. The target mesh slot must
     * already exist. This permits an allocation-free commit into its stable
     * map address, so every object bound to the slot observes either the
     * complete old mesh or the complete replacement.
     *
     * The staging buffer remains an ordinary caller-owned buffer and is not
     * cleared by this command. Accepted geometry is converted into four
     * private Pingo-owned arrays, so later generic-buffer mutation cannot
     * affect the mesh.
    */
    void replace_mesh_from_buffer() {
#if PINGO_RENDER_DIAGNOSTICS
        uint64_t command_started_us = pingo_render_clock_us();
#endif
        auto stage_buffer_id = m_proc->readWord_t();
        auto mesh_id = m_proc->readWord_t();
        auto vertex_count = m_proc->readWord_t();
        auto position_index_count = m_proc->readWord_t();
        auto texture_coordinate_count = m_proc->readWord_t();
        auto texture_index_count = m_proc->readWord_t();
        if (stage_buffer_id < 0 || mesh_id < 0 || vertex_count < 0 ||
            position_index_count < 0 || texture_coordinate_count < 0 ||
            texture_index_count < 0) {
            return;
        }

        const uint32_t vertices = (uint16_t)vertex_count;
        const uint32_t position_indices =
            (uint16_t)position_index_count;
        const uint32_t texture_coordinates =
            (uint16_t)texture_coordinate_count;
        const uint32_t texture_indices =
            (uint16_t)texture_index_count;
        if (stage_buffer_id == 0 || stage_buffer_id == 65535 ||
            isPingo3dControlBuffer((uint16_t)stage_buffer_id) ||
            vertices < 3 || position_indices < 3 ||
            (position_indices % 3U) != 0 ||
            texture_coordinates == 0 ||
            texture_indices != position_indices) {
            debug_log(
                "replace_mesh_from_buffer: invalid source ID or counts\n");
            return;
        }

        /*
         * Replacement is intentionally slot-oriented. Requiring an existing
         * entry avoids a std::map node allocation at the commit boundary and
         * preserves the address already held by every bound object.
         */
        auto mesh_iter = m_meshes->find((uint16_t)mesh_id);
        if (mesh_iter == m_meshes->end()) {
            debug_log(
                "replace_mesh_from_buffer: mesh slot %u is not established\n",
                (uint16_t)mesh_id);
            return;
        }

        size_t required_size =
            (size_t)vertices * 6U +
            (size_t)position_indices * 2U +
            (size_t)texture_coordinates * 4U +
            (size_t)texture_indices * 2U;
        auto source_iter = buffers.find((uint16_t)stage_buffer_id);
        if (source_iter == buffers.end() ||
            source_iter->second.size() != 1 ||
            !source_iter->second.front() ||
            source_iter->second.front()->size() != required_size ||
            !source_iter->second.front()->getBuffer()) {
            debug_log(
                "replace_mesh_from_buffer: source is not one exact consolidated block\n");
            return;
        }

        p3d::Mesh replacement = mesh_iter->second;
        replacement.positions = nullptr;
        replacement.pos_indices = nullptr;
        replacement.textCoord = nullptr;
        replacement.tex_indices = nullptr;
        replacement.positions_count = 0;
        replacement.indexes_count = 0;
        replacement.texture_coordinates_count = 0;
        replacement.texture_indexes_count = 0;
        replacement.geometry_valid = 0;
        replacement.bounds_valid = 0;

        auto release_replacement = [&replacement]() {
            pingo_owned_free(replacement.tex_indices);
            pingo_owned_free(replacement.textCoord);
            pingo_owned_free(replacement.pos_indices);
            pingo_owned_free(replacement.positions);
            replacement.tex_indices = nullptr;
            replacement.textCoord = nullptr;
            replacement.pos_indices = nullptr;
            replacement.positions = nullptr;
        };

        replacement.positions = (p3d::Vec3f*)pingo_owned_malloc(
            (size_t)vertices * sizeof(*replacement.positions));
        if (!replacement.positions) {
            debug_log(
                "replace_mesh_from_buffer: position allocation failed\n");
            return;
        }
        replacement.pos_indices = (uint16_t*)pingo_owned_malloc(
            (size_t)position_indices * sizeof(*replacement.pos_indices));
        if (!replacement.pos_indices) {
            debug_log(
                "replace_mesh_from_buffer: position-index allocation failed\n");
            release_replacement();
            return;
        }
        replacement.textCoord = (p3d::Vec2f*)pingo_owned_malloc(
            (size_t)texture_coordinates * sizeof(*replacement.textCoord));
        if (!replacement.textCoord) {
            debug_log(
                "replace_mesh_from_buffer: UV allocation failed\n");
            release_replacement();
            return;
        }
        replacement.tex_indices = (uint16_t*)pingo_owned_malloc(
            (size_t)texture_indices * sizeof(*replacement.tex_indices));
        if (!replacement.tex_indices) {
            debug_log(
                "replace_mesh_from_buffer: UV-index allocation failed\n");
            release_replacement();
            return;
        }

        const uint8_t * source =
            source_iter->second.front()->getBuffer();
        size_t offset = 0;
        auto read_word = [&source, &offset]() {
            uint16_t value = (uint16_t)source[offset] |
                ((uint16_t)source[offset + 1] << 8);
            offset += 2;
            return value;
        };

        bool valid = true;
        for (uint32_t i = 0; i < vertices; i++) {
            replacement.positions[i] = (p3d::Vec3f){
                convert_position_value(read_word()),
                convert_position_value(read_word()),
                convert_position_value(read_word())
            };
            if (!isfinite(replacement.positions[i].x) ||
                !isfinite(replacement.positions[i].y) ||
                !isfinite(replacement.positions[i].z)) {
                valid = false;
            }
        }
        for (uint32_t i = 0; i < position_indices; i++) {
            replacement.pos_indices[i] = read_word();
            if (replacement.pos_indices[i] >= vertices) {
                valid = false;
            }
        }
        for (uint32_t i = 0; i < texture_coordinates; i++) {
            replacement.textCoord[i] = (p3d::Vec2f){
                convert_texture_coordinate_value(read_word()),
                convert_texture_coordinate_value(read_word())
            };
            if (!isfinite(replacement.textCoord[i].x) ||
                !isfinite(replacement.textCoord[i].y)) {
                valid = false;
            }
        }
        for (uint32_t i = 0; i < texture_indices; i++) {
            replacement.tex_indices[i] = read_word();
            if (replacement.tex_indices[i] >= texture_coordinates) {
                valid = false;
            }
        }
        if (offset != required_size) {
            valid = false;
        }

        /*
         * Flat modes use UV0 as the face selector and intentionally do not
         * interpolate it. Their packed form must nevertheless describe one
         * unambiguous selector: all three indexed UV pairs of every source
         * face are required to be identical.
         */
        if (valid &&
            (replacement.shading_mode == p3d::MESH_SHADING_FLAT_PALETTE ||
             replacement.shading_mode == p3d::MESH_SHADING_FLAT_PATTERN)) {
            for (uint32_t i = 0; i < texture_indices; i += 3) {
                p3d::Vec2f selector = replacement.textCoord[
                    replacement.tex_indices[i]];
                for (uint32_t corner = 1; corner < 3; corner++) {
                    p3d::Vec2f candidate = replacement.textCoord[
                        replacement.tex_indices[i + corner]];
                    if (candidate.x != selector.x ||
                        candidate.y != selector.y) {
                        valid = false;
                    }
                }
            }
        }

        replacement.positions_count = vertices;
        replacement.indexes_count = (int)position_indices;
        replacement.texture_coordinates_count = texture_coordinates;
        replacement.texture_indexes_count = texture_indices;
        if (!valid || !p3d::meshUpdateBounds(&replacement) ||
            !p3d::meshUpdateGeometryValidity(&replacement)) {
            debug_log(
                "replace_mesh_from_buffer: staged mesh failed validation\n");
            release_replacement();
            return;
        }

        /*
         * Single-threaded VDU dispatch makes this struct publication the
         * logical atomic swap. The stable map element address is unchanged;
         * old storage is released only after dependent objects have observed
         * the complete replacement.
         */
        p3d::Mesh previous = mesh_iter->second;
        mesh_iter->second = replacement;
        refresh_mesh_dependents(&mesh_iter->second);
        pingo_owned_free(previous.tex_indices);
        pingo_owned_free(previous.textCoord);
        pingo_owned_free(previous.pos_indices);
        pingo_owned_free(previous.positions);
#if PINGO_RENDER_DIAGNOSTICS
        force_debug_log(
            "PINGO_STREAM mesh_replace=ok stage=%u mid=%u bytes=%u "
            "v=%u i=%u u=%u t=%u us=%u\n",
            (uint16_t)stage_buffer_id, (uint16_t)mesh_id,
            (uint32_t)required_size,
            vertices, position_indices, texture_coordinates, texture_indices,
            pingo_render_diagnostics_elapsed_us(
                command_started_us, pingo_render_clock_us()));
#endif
    }

    // VDU 23,0,&A0,sid; &49,51, oid; active
    // Only 0 and 1 are valid. The object must already exist.
    void set_object_active() {
        auto object_id = m_proc->readWord_t();
        auto active = m_proc->readByte_t();
        if (object_id < 0 || active < 0 || (active != 0 && active != 1)) {
            return;
        }
        auto object = m_objects->find((uint16_t)object_id);
        if (object == m_objects->end()) {
            return;
        }
        object->second.m_object.inactive = active ? 0 : 1;
#if PINGO_RENDER_DIAGNOSTICS
        force_debug_log(
            "PINGO_STREAM object_active=ok oid=%u active=%u\n",
            (uint16_t)object_id, (uint8_t)active);
#endif
    }

    p3d::Mesh* establish_mesh(uint16_t mid) {
        auto mesh_iter = m_meshes->find(mid);
        if (mesh_iter == m_meshes->end()) {
            p3d::Mesh mesh;
            memset(&mesh, 0, sizeof(mesh));
            (*m_meshes).insert(std::pair<uint16_t, p3d::Mesh>(mid, mesh));
            return &m_meshes->find(mid)->second;
        } else {
            return &mesh_iter->second;
        }
    }

    p3d::Mesh* get_mesh() {
        auto mid = m_proc->readWord_t();
        if (mid >= 0) {
            return establish_mesh(mid);
        }
        return NULL;
    }

    TexObject* establish_object(uint16_t oid) {
        auto object_iter = m_objects->find(oid);
        if (object_iter == m_objects->end()) {
            /*
             * Bind the map-resident object. Initializing a stack temporary
             * before copying it would leave m_object.material and
             * m_material.texture pointing back into the dead temporary.
             */
            auto inserted = m_objects->insert(
                std::pair<uint16_t, TexObject>(oid, TexObject{}));
            auto object = &inserted.first->second;
            memset(object, 0, sizeof(*object));
            object->m_oid = oid;
            object->initialize();
            return object;
        } else {
            return &object_iter->second;
        }
    }

    TexObject* get_object() {
        auto oid = m_proc->readWord_t();
        if (oid >= 0) {
            return establish_object(oid);
        }
        return NULL;
    }

    /*
     * Mesh components are independent VDU uploads and may arrive out of
     * order. Recompute renderability after every successful replacement so a
     * later complementary upload can make an existing mesh/object valid.
     */
    void refresh_mesh_dependents(p3d::Mesh* mesh) {
        if (!mesh) {
            return;
        }
        p3d::meshUpdateGeometryValidity(mesh);
        if (!m_objects) {
            return;
        }
        for (auto& entry : *m_objects) {
            if (entry.second.m_object.mesh == mesh) {
                p3d::objectUpdateTextureMappingValidity(
                    &entry.second.m_object);
            }
        }
    }

    bool checked_upload_size(
            uint32_t count, size_t element_size, size_t * byte_count) {
        if (!byte_count || (count && element_size > SIZE_MAX / count)) {
            return false;
        }
        *byte_count = (size_t)count * element_size;
        return true;
    }

    /*
     * Structurally invalid or allocation-failed commands still consume a
     * complete declared payload to preserve alignment. A truncated payload,
     * however, stops after the first timeout; repeatedly waiting for every
     * absent element could otherwise hold the VDP task for hours.
     */
    bool drain_upload_words(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            if (m_proc->readWord_t() < 0) {
                return false;
            }
        }
        return true;
    }

    // VDU 23, 0, &A0, sid; &48, 1, mid; n; x0; y0; z0; ... :  Define Mesh Vertices
    void define_mesh_vertices() {
        auto mesh = get_mesh();
        if (!mesh) {
            return;
        }
        auto vertex_count = m_proc->readWord_t();
        if (vertex_count < 0) {
            return;
        }
        auto n = (uint32_t)vertex_count;
        p3d::Vec3f* replacement = NULL;
        size_t size = 0;
        if (!checked_upload_size(n, sizeof(p3d::Vec3f), &size)) {
            debug_log("define_mesh_vertices: size overflow for %u vertices\n", n);
            drain_upload_words(n * 3U);
            return;
        }
        if (n > 0) {
            replacement = (p3d::Vec3f*)pingo_owned_malloc(size);
            if (!replacement) {
                debug_log(
                    "define_mesh_vertices: failed to allocate %u bytes\n",
                    (uint32_t)size);
                show_free_ram();
                drain_upload_words(n * 3U);
                return;
            }
            debug_log("Reading %u vertices\n", n);
            for (uint32_t i = 0; i < n; i++) {
                auto x = m_proc->readWord_t();
                if (x < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                auto y = m_proc->readWord_t();
                if (y < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                auto z = m_proc->readWord_t();
                if (z < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                replacement[i].x =
                    convert_position_value((uint16_t)x);
                replacement[i].y =
                    convert_position_value((uint16_t)y);
                replacement[i].z =
                    convert_position_value((uint16_t)z);
                if (!(i & 0x1F)) {
                    debug_log(
                        "%u %f %f %f\n", i,
                        replacement[i].x,
                        replacement[i].y,
                        replacement[i].z);
                }
            }
            debug_log("\n");
        }

        auto previous = mesh->positions;
        mesh->positions = replacement;
        mesh->positions_count = n;
        mesh->bounds_valid = 0;
        if (n > 0) {
            p3d::meshUpdateBounds(mesh);
        }
        if (previous) {
            pingo_owned_free(previous);
        }
        refresh_mesh_dependents(mesh);
    }

    // VDU 23, 0, &A0, sid; &48, 2, mid; n; i0; ... :  Set Mesh Vertex Indexes
    void set_mesh_vertex_indexes() {
        auto mesh = get_mesh();
        if (!mesh) {
            return;
        }
        auto index_count = m_proc->readWord_t();
        if (index_count < 0) {
            return;
        }
        auto n = (uint32_t)index_count;
        uint16_t* replacement = NULL;
        if ((n % 3U) != 0) {
            debug_log(
                "set_mesh_vertex_indexes: count %u is not a triangle triplet\n",
                n);
            drain_upload_words(n);
            return;
        }
        size_t size = 0;
        if (!checked_upload_size(n, sizeof(uint16_t), &size)) {
            debug_log("set_mesh_vertex_indexes: size overflow for %u indexes\n", n);
            drain_upload_words(n);
            return;
        }
        if (n > 0) {
            replacement = (uint16_t*)pingo_owned_malloc(size);
            if (!replacement) {
                debug_log(
                    "set_mesh_vertex_indexes: failed to allocate %u bytes\n",
                    (uint32_t)size);
                show_free_ram();
                drain_upload_words(n);
                return;
            }
            debug_log("Reading %u vertex indexes\n", n);
            for (uint32_t i = 0; i < n; i++) {
                auto index = m_proc->readWord_t();
                if (index < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                replacement[i] = (uint16_t)index;
                if (!(i & 0x1F)) {
                    debug_log("%u %hu\n", i, (uint16_t)index);
                }
            }
            debug_log("\n");
        }

        auto previous = mesh->pos_indices;
        mesh->pos_indices = replacement;
        mesh->indexes_count = (int)n;
        if (previous) {
            pingo_owned_free(previous);
        }
        refresh_mesh_dependents(mesh);
    }

    // VDU 23, 0, &A0, sid; &48, 3, mid; n; u0; v0; ... :  Define Mesh Texture Coordinates
    void define_mesh_texture_coordinates() {
        auto mesh = get_mesh();
        if (!mesh) {
            return;
        }
        auto coordinate_count = m_proc->readWord_t();
        if (coordinate_count < 0) {
            return;
        }
        auto n = (uint32_t)coordinate_count;
        p3d::Vec2f* replacement = NULL;
        size_t size = 0;
        if (!checked_upload_size(n, sizeof(p3d::Vec2f), &size)) {
            debug_log(
                "define_mesh_texture_coordinates: size overflow for %u coordinates\n",
                n);
            drain_upload_words(n * 2U);
            return;
        }
        if (n > 0) {
            replacement = (p3d::Vec2f*)pingo_owned_malloc(size);
            if (!replacement) {
                debug_log(
                    "define_mesh_texture_coordinates: failed to allocate %u bytes\n",
                    (uint32_t)size);
                show_free_ram();
                drain_upload_words(n * 2U);
                return;
            }
            debug_log("Reading %u texture coordinates\n", n);
            for (uint32_t i = 0; i < n; i++) {
                auto u = m_proc->readWord_t();
                if (u < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                auto v = m_proc->readWord_t();
                if (v < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                replacement[i].x =
                    convert_texture_coordinate_value((uint16_t)u);
                replacement[i].y =
                    convert_texture_coordinate_value((uint16_t)v);
            }
        }

        auto previous = mesh->textCoord;
        mesh->textCoord = replacement;
        mesh->texture_coordinates_count = n;
        if (previous) {
            pingo_owned_free(previous);
        }
        refresh_mesh_dependents(mesh);
    }

    // VDU 23, 0, &A0, sid; &48, 40, oid; n; u0; v0; ... :  Define Object Texture Coordinates
    void define_object_texture_coordinates() {
        auto object = get_object();
        if (!object) {
            return;
        }
        auto coordinate_count = m_proc->readWord_t();
        if (coordinate_count < 0) {
            return;
        }
        auto n = (uint32_t)coordinate_count;
        p3d::Vec2f* replacement = NULL;
        size_t size = 0;
        if (!checked_upload_size(n, sizeof(p3d::Vec2f), &size)) {
            debug_log(
                "define_object_texture_coordinates: size overflow for %u coordinates\n",
                n);
            drain_upload_words(n * 2U);
            return;
        }
        if (n > 0) {
            replacement = (p3d::Vec2f*)pingo_owned_malloc(size);
            if (!replacement) {
                debug_log(
                    "define_object_texture_coordinates: failed to allocate %u bytes\n",
                    (uint32_t)size);
                show_free_ram();
                drain_upload_words(n * 2U);
                return;
            }
            debug_log("Reading %u texture coordinates\n", n);
            for (uint32_t i = 0; i < n; i++) {
                auto u = m_proc->readWord_t();
                if (u < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                auto v = m_proc->readWord_t();
                if (v < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                replacement[i].x =
                    convert_texture_coordinate_value((uint16_t)u);
                replacement[i].y =
                    convert_texture_coordinate_value((uint16_t)v);
            }
        }

        auto previous = object->m_object.textCoord;
        object->m_object.textCoord = replacement;
        object->m_object.textCoord_count = n;
        if (previous) {
            pingo_owned_free(previous);
        }
        p3d::objectUpdateTextureMappingValidity(&object->m_object);
    }

    // VDU 23, 0, &A0, sid; &48, 4, mid; n; i0; ... :  Set Texture Coordinate Indexes
    void set_texture_coordinate_indexes() {
        auto mesh = get_mesh();
        if (!mesh) {
            return;
        }
        auto index_count = m_proc->readWord_t();
        if (index_count < 0) {
            return;
        }
        auto n = (uint32_t)index_count;
        uint16_t* replacement = NULL;
        if ((n % 3U) != 0) {
            debug_log(
                "set_texture_coordinate_indexes: count %u is not a triangle triplet\n",
                n);
            drain_upload_words(n);
            return;
        }
        size_t size = 0;
        if (!checked_upload_size(n, sizeof(uint16_t), &size)) {
            debug_log(
                "set_texture_coordinate_indexes: size overflow for %u indexes\n",
                n);
            drain_upload_words(n);
            return;
        }
        if (n > 0) {
            replacement = (uint16_t*)pingo_owned_malloc(size);
            if (!replacement) {
                debug_log(
                    "set_texture_coordinate_indexes: failed to allocate %u bytes\n",
                    (uint32_t)size);
                show_free_ram();
                drain_upload_words(n);
                return;
            }
            debug_log("Reading %u texture coordinate indexes\n", n);
            for (uint32_t i = 0; i < n; i++) {
                auto index = m_proc->readWord_t();
                if (index < 0) {
                    pingo_owned_free(replacement);
                    return;
                }
                replacement[i] = (uint16_t)index;
                if (!(i & 0x1F)) {
                    debug_log("%u %hu\n", i, (uint16_t)index);
                }
            }
        }

        auto previous = mesh->tex_indices;
        mesh->tex_indices = replacement;
        mesh->texture_indexes_count = n;
        if (previous) {
            pingo_owned_free(previous);
        }
        refresh_mesh_dependents(mesh);
    }

    // VDU 23, 0, &A0, sid; &48, 5, oid; mid; bmid; :  Create Object
    void create_object() {
        auto object = get_object();
        auto mesh = get_mesh();
        auto bmid = m_proc->readWord_t();
        if (object && mesh && bmid) {
            if (isPingo3dControlBuffer((uint16_t)bmid)) {
                debug_log(
                    "create_object: refusing live Pingo control %u as texture\n",
                    bmid);
                return;
            }
            debug_log("Creating 3D object %u with bitmap %u\n", object->m_oid, bmid);
            auto stored_bitmap = getBitmap(bmid);
            if (stored_bitmap) {
                auto bitmap = stored_bitmap.get();
                if (bitmap) {
                    if (bitmap->width <= 0 || bitmap->height <= 0 ||
                        !bitmap->data) {
                        debug_log(
                            "Creating 3D object %u failed: bitmap %u has invalid dimensions or data\n",
                            object->m_oid, bmid);
                        return;
                    }
                    std::shared_ptr<BufferStream> bitmap_storage;
                    if (!bitmap->dataAllocated) {
                        auto storage_iter = buffers.find(bmid);
                        if (storage_iter == buffers.end() ||
                            storage_iter->second.size() != 1 ||
                            !storage_iter->second.front() ||
                            storage_iter->second.front()->getBuffer() !=
                                bitmap->data) {
                            debug_log(
                                "Creating 3D object %u failed: bitmap %u backing storage is unavailable\n",
                                object->m_oid, bmid);
                            return;
                        }
                        bitmap_storage = storage_iter->second.front();
                    }

                    p3d::TextureFormat texture_format;
                    switch (bitmap->format) {
                        case PixelFormat::RGBA8888:
                            texture_format = p3d::TEXTURE_FORMAT_RGBA8888;
                            break;
                        case PixelFormat::RGBA2222:
                            texture_format = p3d::TEXTURE_FORMAT_RGBA2222;
                            break;
                        default:
                            debug_log("Creating 3D object %u failed: bitmap %u has unsupported format %u\n",
                                object->m_oid, bmid, (uint8_t)bitmap->format);
                            return;
                    }
                    auto size = p3d::Vec2i{(p3d::I_TYPE)bitmap->width, (p3d::I_TYPE)bitmap->height};
                    p3d::Texture replacement_texture = {};
                    if (p3d::texture_init_format(
                            &replacement_texture, size, bitmap->data,
                            texture_format)) {
                        debug_log("Creating 3D object %u failed: invalid texture bitmap %u\n",
                            object->m_oid, bmid);
                        return;
                    }

                    auto replacement_binding =
                        pingo_owned_new<PingoTextureBinding>();
                    if (!replacement_binding) {
                        debug_log(
                            "Creating 3D object %u failed: could not retain bitmap %u\n",
                            object->m_oid, bmid);
                        show_free_ram();
                        return;
                    }
                    replacement_binding->m_bitmap = stored_bitmap;
                    replacement_binding->m_storage = bitmap_storage;

                    /*
                     * Pin the bitmap metadata and its separately owned pixels
                     * before publishing the raw Texture pointer. Clearing or
                     * replacing bmid now leaves this binding valid until the
                     * object is explicitly rebound or its control is deleted.
                     */
                    auto previous_binding = object->m_texture_binding;
                    object->m_texture_binding = replacement_binding;
                    object->m_texture = replacement_texture;
                    object->bind();
                    object->m_object.mesh = mesh;
                    p3d::objectUpdateTextureMappingValidity(
                        &object->m_object);
                    pingo_owned_delete(previous_binding);
                    auto pixel = p3d::texture_read(
                        &object->m_texture, p3d::Vec2i{0, 0});
                    debug_log("Texture format %u data: %02hX\n",
                        (uint8_t)texture_format, pixel.c);
                }
            }
        }
    }

    p3d::F_TYPE convert_scale_value(int32_t value) {
        static const p3d::F_TYPE factor = 1.0f / 256.0f;
        return ((p3d::F_TYPE) value) * factor;
    }

    p3d::F_TYPE convert_rotation_value(int32_t value) {
        if (value & 0x8000) {
            value = (int32_t)(int16_t)(uint16_t) value;
        }
        static const p3d::F_TYPE factor = (2.0f * 3.1415926f) / 32767.0f;
        return ((p3d::F_TYPE) value) * factor;
    }

    p3d::F_TYPE convert_translation_value(int32_t value) {
        if (value & 0x8000) {
            value = (int32_t)(int16_t)(uint16_t) value;
        }
        static const p3d::F_TYPE factor = 256.0f / 32767.0f;
        return ((p3d::F_TYPE) value) * factor;
    }

    int32_t sign_extend_24(int32_t value) {
        value &= 0xFFFFFF;
        return (value ^ 0x800000) - 0x800000;
    }

    p3d::F_TYPE convert_wide_translation_value(int32_t value) {
        static const p3d::F_TYPE factor = 256.0f / 32767.0f;
        return ((p3d::F_TYPE) sign_extend_24(value)) * factor;
    }

    p3d::F_TYPE convert_position_value(int32_t value) {
        if (value & 0x8000) {
            value = (int32_t)(int16_t)(uint16_t) value;
        }
        static const p3d::F_TYPE factor = 1.0f / 32767.0f;
        return ((p3d::F_TYPE) value) * factor;
    }

    p3d::F_TYPE convert_texture_coordinate_value(int32_t value) {
        static const p3d::F_TYPE factor = 1.0f / 65535.0f;
        return ((p3d::F_TYPE) value) * factor;
    }

    // VDU 23, 0, &A0, sid; &48, 6, oid; scalex; :  Set Object X Scale Factor
    void set_object_x_scale_factor() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object && (value >= 0)) {
            object->m_scale.x = convert_scale_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 7, oid; scaley; :  Set Object Y Scale Factor
    void set_object_y_scale_factor() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object && (value >= 0)) {
            object->m_scale.y = convert_scale_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 8, oid; scalez; :  Set Object Z Scale Factor
    void set_object_z_scale_factor() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object && (value >= 0)) {
            object->m_scale.z = convert_scale_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 9, oid; scalex; scaley; scalez :  Set Object XYZ Scale Factors
    void set_object_xyz_scale_factors() {
        auto object = get_object();
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        if (object && (valuex >= 0) && (valuey >= 0) && (valuez >= 0)) {
            object->m_scale.x = convert_scale_value(valuex);
            object->m_scale.y = convert_scale_value(valuey);
            object->m_scale.z = convert_scale_value(valuez);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 10, oid; anglex; :  Set Object X Rotation Angle
    void set_object_x_rotation_angle() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_rotation.x = convert_rotation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 11, oid; angley; :  Set Object Y Rotation Angle
    void set_object_y_rotation_angle() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_rotation.y = convert_rotation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 12, oid; anglez; :  Set Object Z Rotation Angle
    void set_object_z_rotation_angle() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_rotation.z = convert_rotation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 13, oid; anglex; angley; anglez; :  Set Object XYZ Rotation Angles
    void set_object_xyz_rotation_angles() {
        auto object = get_object();
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        if (object) {
            object->m_rotation.x = convert_rotation_value(valuex);
            object->m_rotation.y = convert_rotation_value(valuey);
            object->m_rotation.z = convert_rotation_value(valuez);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 14, oid; distx; :  Set Object X Translation Distance
    void set_object_x_translation_distance() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_translation.x = convert_translation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 15, oid; disty; :  Set Object Y Translation Distance
    void set_object_y_translation_distance() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_translation.y = convert_translation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 16, oid; distz; :  Set Object Z Translation Distance
    void set_object_z_translation_distance() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_translation.z = convert_translation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 17, oid; distx; disty; distz :  Set Object XYZ Translation Distances
    void set_object_xyz_translation_distances() {
        auto object = get_object();
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        if (object) {
            object->m_translation.x = convert_translation_value(valuex);
            object->m_translation.y = convert_translation_value(valuey);
            object->m_translation.z = convert_translation_value(valuez);
            object->m_modified = true;
#if PINGO_RENDER_DIAGNOSTICS
            force_debug_log(
                "PINGO_POSE object_translation oid=%u "
                "raw_x=%d raw_y=%d raw_z=%d\n",
                object->m_oid,
                (int)(int16_t)(uint16_t)valuex,
                (int)(int16_t)(uint16_t)valuey,
                (int)(int16_t)(uint16_t)valuez);
#endif
        }
    }

    /*
     * VDU 23, 0, &A0, sid; &49, 52, oid; distx:s24; disty:s24;
     * distz:s24 : Set an existing object's XYZ translation using signed
     * little-endian 24-bit values. The raw unit is identical to command 17.
     */
    void set_object_xyz_translation_distances_wide() {
        auto object_id = m_proc->readWord_t();
        if (object_id < 0) {
            return;
        }
        auto valuex = m_proc->read24_t();
        if (valuex < 0) {
            return;
        }
        auto valuey = m_proc->read24_t();
        if (valuey < 0) {
            return;
        }
        auto valuez = m_proc->read24_t();
        if (valuez < 0) {
            return;
        }

        /*
         * Unlike the legacy setters, this streaming-oriented command must
         * not allocate a placeholder object. Still consume the complete
         * fixed payload before deciding whether its stable slot exists.
         */
        auto object = m_objects->find((uint16_t)object_id);
        if (object == m_objects->end()) {
            return;
        }
        object->second.m_translation.x =
            convert_wide_translation_value(valuex);
        object->second.m_translation.y =
            convert_wide_translation_value(valuey);
        object->second.m_translation.z =
            convert_wide_translation_value(valuez);
        object->second.m_modified = true;
#if PINGO_RENDER_DIAGNOSTICS
        int32_t signed_x = sign_extend_24(valuex);
        int32_t signed_y = sign_extend_24(valuey);
        int32_t signed_z = sign_extend_24(valuez);
        force_debug_log(
            "PINGO_WIDE object_translation=ok oid=%u "
            "raw_x=%d raw_y=%d raw_z=%d\n",
            (uint16_t)object_id,
            (int)signed_x, (int)signed_y, (int)signed_z);
#endif
    }

    // VDU 23,0,&A0,sid; &49,53, far_units:u16
    // Retain the existing one-unit near plane and require far > near.
    void set_projection_far_distance() {
        auto far_units = m_proc->readWord_t();
        if (far_units < PINGO_PROJECTION_FAR_MIN_UNITS ||
            far_units > UINT16_MAX) {
            return;
        }
        m_projection_far_units = (uint16_t)far_units;
#if PINGO_RENDER_DIAGNOSTICS
        force_debug_log(
            "PINGO_PROJECTION far_set=ok units=%u\n",
            m_projection_far_units);
#endif
    }

    // VDU 23, 0, &A0, sid; &48, 18, oid; anglex; :  Set Camera X Rotation Angle
    void set_camera_x_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_camera.m_rotation.x = convert_rotation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 19, oid; angley; :  Set Camera Y Rotation Angle
    void set_camera_y_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_camera.m_rotation.y = convert_rotation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 20, oid; anglez; :  Set Camera Z Rotation Angle
    void set_camera_z_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_camera.m_rotation.z = convert_rotation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 21, oid; anglex; angley; anglez; :  Set Camera XYZ Rotation Angles
    void set_camera_xyz_rotation_angles() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_camera.m_rotation.x = convert_rotation_value(valuex);
        m_camera.m_rotation.y = convert_rotation_value(valuey);
        m_camera.m_rotation.z = convert_rotation_value(valuez);
        m_camera.m_modified = true;
#if PINGO_RENDER_DIAGNOSTICS
        force_debug_log(
            "PINGO_POSE camera_rotation raw_x=%d raw_y=%d raw_z=%d\n",
            (int)(int16_t)(uint16_t)valuex,
            (int)(int16_t)(uint16_t)valuey,
            (int)(int16_t)(uint16_t)valuez);
#endif
    }

    // VDU 23, 0, &A0, sid; &48, 22, oid; distx; :  Set Camera X Translation Distance
    void set_camera_x_translation_distance() {
        auto value = m_proc->readWord_t();
        m_camera.m_translation.x = convert_translation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 23, oid; disty; :  Set Camera Y Translation Distance
    void set_camera_y_translation_distance() {
        auto value = m_proc->readWord_t();
        m_camera.m_translation.y = convert_translation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 24, oid; distz; :  Set Camera Z Translation Distance
    void set_camera_z_translation_distance() {
        auto value = m_proc->readWord_t();
        m_camera.m_translation.z = convert_translation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 25, oid; distx; disty; distz :  Set Camera XYZ Translation Distances
    void set_camera_xyz_translation_distances() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_camera.m_translation.x = convert_translation_value(valuex);
        m_camera.m_translation.y = convert_translation_value(valuey);
        m_camera.m_translation.z = convert_translation_value(valuez);
        m_camera.m_modified = true;
#if PINGO_RENDER_DIAGNOSTICS
        force_debug_log(
            "PINGO_POSE camera_translation raw_x=%d raw_y=%d raw_z=%d\n",
            (int)(int16_t)(uint16_t)valuex,
            (int)(int16_t)(uint16_t)valuey,
            (int)(int16_t)(uint16_t)valuez);
#endif
    }

    // VDU 23, 0, &A0, sid; &48, 26, oid; scalex; :  Set Scene X Scale Factor
    void set_scene_x_scale_factor() {
        auto value = m_proc->readWord_t();
        if (value >= 0) {
            m_scene.m_scale.x = convert_scale_value(value);
            m_scene.m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 27, oid; scaley; :  Set Scene Y Scale Factor
    void set_scene_y_scale_factor() {
        auto value = m_proc->readWord_t();
        if (value >= 0) {
            m_scene.m_scale.y = convert_scale_value(value);
            m_scene.m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 28, oid; scalez; :  Set Scene Z Scale Factor
    void set_scene_z_scale_factor() {
        auto value = m_proc->readWord_t();
        if (value >= 0) {
            m_scene.m_scale.z = convert_scale_value(value);
            m_scene.m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 29, oid; scalex; scaley; scalez :  Set Scene XYZ Scale Factors
    void set_scene_xyz_scale_factors() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        if ((valuex >= 0) && (valuey >= 0) && (valuez >= 0)) {
            m_scene.m_scale.x = convert_scale_value(valuex);
            m_scene.m_scale.y = convert_scale_value(valuey);
            m_scene.m_scale.z = convert_scale_value(valuez);
            m_scene.m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &48, 30, oid; anglex; :  Set Scene X Rotation Angle
    void set_scene_x_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_scene.m_rotation.x = convert_rotation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 31, oid; angley; :  Set Scene Y Rotation Angle
    void set_scene_y_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_scene.m_rotation.y = convert_rotation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 32, oid; anglez; :  Set Scene Z Rotation Angle
    void set_scene_z_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_scene.m_rotation.z = convert_rotation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 33, oid; anglex; angley; anglez; :  Set Scene XYZ Rotation Angles
    void set_scene_xyz_rotation_angles() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_scene.m_rotation.x = convert_rotation_value(valuex);
        m_scene.m_rotation.y = convert_rotation_value(valuey);
        m_scene.m_rotation.z = convert_rotation_value(valuez);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 34, oid; distx; :  Set Scene X Translation Distance
    void set_scene_x_translation_distance() {
        auto value = m_proc->readWord_t();
        m_scene.m_translation.x = convert_translation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 35, oid; disty; :  Set Scene Y Translation Distance
    void set_scene_y_translation_distance() {
        auto value = m_proc->readWord_t();
        m_scene.m_translation.y = convert_translation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 36, oid; distz; :  Set Scene Z Translation Distance
    void set_scene_z_translation_distance() {
        auto value = m_proc->readWord_t();
        m_scene.m_translation.z = convert_translation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 37, oid; distx; disty; distz :  Set Scene XYZ Translation Distances
    void set_scene_xyz_translation_distances() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_scene.m_translation.x = convert_translation_value(valuex);
        m_scene.m_translation.y = convert_translation_value(valuey);
        m_scene.m_translation.z = convert_translation_value(valuez);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &48, 38, bmid; :  Render To Bitmap
    void render_to_bitmap() {
#if PINGO_RENDER_DIAGNOSTICS
        uint64_t command_started_us = pingo_render_clock_us();
#endif
        auto bmid = m_proc->readWord_t();
        if (bmid < 0) {
            return;
        }
        if (isPingo3dControlBuffer((uint16_t)bmid)) {
            debug_log(
                "render_to_bitmap: refusing live Pingo control %u as output\n",
                bmid);
            return;
        }

        auto stored_bitmap = getBitmap(bmid);
        auto bitmap = stored_bitmap.get();
        if (!bitmap || !bitmap->data ||
            bitmap->width != m_width || bitmap->height != m_height ||
            (bitmap->format != PixelFormat::RGBA2222 &&
             bitmap->format != PixelFormat::RGBA8888)) {
            debug_log("render_to_bitmap: output bitmap %u not found or invalid\n", bmid);
            return;
        }

#if PINGO_RENDER_DIAGNOSTICS
        uint64_t prepare_started_us = pingo_render_clock_us();
#endif

        // Native RGBA2222 targets are Pingo's working format, so render
        // directly into them. Keep the private frame for RGBA8888 targets,
        // which require an explicit compatibility expansion after rendering.
        auto private_frame = m_frame;
        if (bitmap->format == PixelFormat::RGBA2222) {
            m_frame = (p3d::Pixel *)bitmap->data;
        }

        //auto start = millis();
        auto size = p3d::Vec2i{(p3d::I_TYPE)m_width, (p3d::I_TYPE)m_height};
        p3d::Renderer renderer;
        rendererInit(&renderer, size, &m_backend );
        /* Control state is normalized transactionally when command 43 lands. */
        renderer.lightDirection = m_light_direction;
        p3d::rendererSetLightIntensity(&renderer, m_light_intensity);
        p3d::rendererSetAmbientLight(&renderer, m_ambient_light);
        p3d::rendererSetIlluminationEnabled(
            &renderer, m_illumination_enabled);
        if (m_flat_pattern_binding &&
            p3d::rendererSetFlatPatternLibrary(
                &renderer,
                &m_flat_pattern_binding->m_library) != 0) {
            debug_log(
                "render_to_bitmap: retained flat-pattern resource is invalid\n");
        }
#if PINGO_RENDER_DIAGNOSTICS
        renderer.diagnostics_clock = pingo_render_diagnostics_clock_ticks;
        renderer.diagnostics_clock_hz =
            pingo_render_diagnostics_clock_hz();
#endif
        rendererSetCamera(&renderer,(p3d::Vec4i){0,0,size.x,size.y});

        /*
         * Consume dirty object transforms during command preparation, as the
         * former transient-Scene path did. Rendering later walks the stable
         * uint16_t-keyed map directly, so active-object capacity is bounded
         * by the complete object ID space and available registry memory, not
         * by MAX_SCENE_RENDERABLES.
         */
        for (auto object = m_objects->begin(); object != m_objects->end(); object++) {
            if (object->second.m_object.inactive) {
                continue;
            }
            object->second.bind();
            if (object->second.m_modified) {
                object->second.update_transformation_matrix();
                //object->second.dump();
            }
        }

        // Set the projection matrix
        renderer.camera_projection =
            p3d::mat4Perspective(
                PINGO_PROJECTION_NEAR_UNITS,
                (p3d::F_TYPE)m_projection_far_units,
                (p3d::F_TYPE)size.x / (p3d::F_TYPE)size.y,
                0.6);

        if (m_camera.m_modified) {
            m_camera.compute_transformation_matrix();
        }
        //debug_log("Camera:\n");
        //m_camera.dump();
        // VDU camera transforms describe its world pose; rendering needs the
        // inverse world-to-view transform.
        renderer.camera_view = p3d::mat4Inverse(&m_camera.m_transform);

        if (m_scene.m_modified) {
            m_scene.compute_transformation_matrix();
        }

#if PINGO_RENDER_DIAGNOSTICS
        uint32_t prepare_us = pingo_render_diagnostics_elapsed_us(
            prepare_started_us, pingo_render_clock_us());
#endif

        //debug_log("Frame data:  %02hX %02hX %02hX %02hX\n", m_frame->r, m_frame->g, m_frame->b, m_frame->a);
        //debug_log("Destination: %02hX %02hX %02hX %02hX\n", dst_pix->r, dst_pix->g, dst_pix->b, dst_pix->a);

        // Time only Pingo's renderer. Bitmap copying and diagnostic output are
        // intentionally outside the measured interval.
        uint64_t render_start_us = pingo_render_clock_us();
        p3d::rendererBeginFrame(&renderer);
        for (auto object = m_objects->begin();
             object != m_objects->end(); object++) {
            if (object->second.m_object.inactive) {
                continue;
            }
            p3d::rendererRenderRenderable(
                &renderer,
                m_scene.m_transform,
                p3d::object_as_renderable(&object->second.m_object));
        }
        p3d::rendererEndFrame(&renderer);
        uint32_t render_elapsed_us =
            (uint32_t)(pingo_render_clock_us() - render_start_us);

#if PINGO_RENDER_DIAGNOSTICS
        uint64_t output_started_us = pingo_render_clock_us();
#endif
        if (bitmap->format == PixelFormat::RGBA8888) {
            auto dst_pix = (uint32_t *)bitmap->data;
            uint32_t frame_size = (uint32_t)m_width * m_height;
            for (uint32_t i = 0; i < frame_size; i++) {
                dst_pix[i] = p3d::pixelToRGBA8888(m_frame[i]);
            }
        }
        m_frame = private_frame;

#if PINGO_RENDER_DIAGNOSTICS
        uint64_t output_finished_us = pingo_render_clock_us();
        auto sequence = m_render_sequence++;
#if defined(USERSPACE) && PINGO_RENDER_TARGET_DUMP
        pingo_dump_render_target(
            bmid, sequence,
            bitmap->data,
            (uint32_t)m_width * m_height *
                (bitmap->format == PixelFormat::RGBA2222 ? 1U : 4U),
            bitmap->format == PixelFormat::RGBA2222,
            m_zeta,
            (uint32_t)m_width * m_height * sizeof(p3d::PingoDepth));
#endif
#if defined(USERSPACE) && PINGO_RENDER_TARGET_HASH
        force_debug_log(
            "PINGO_TARGET seq=%u bmid=%u bytes=%u fnv1a64=%016llx "
            "zbytes=%u zfnv1a64=%016llx\n",
            sequence, bmid,
            (uint32_t)m_width * m_height *
                (bitmap->format == PixelFormat::RGBA2222 ? 1U : 4U),
            (unsigned long long)pingo_fnv1a64(
                (const uint8_t *)bitmap->data,
                (uint32_t)m_width * m_height *
                    (bitmap->format == PixelFormat::RGBA2222 ? 1U : 4U)),
            (uint32_t)m_width * m_height * sizeof(p3d::PingoDepth),
            (unsigned long long)pingo_fnv1a64(
                (const uint8_t *)m_zeta,
                (uint32_t)m_width * m_height *
                    sizeof(p3d::PingoDepth)));
#endif
        uint32_t output_us = pingo_render_diagnostics_elapsed_us(
            output_started_us, output_finished_us);
        uint32_t command_us = pingo_render_diagnostics_elapsed_us(
            command_started_us, output_finished_us);
        uint32_t diagnostics_clock_hz = renderer.diagnostics_clock_hz;

        uint32_t clear_us = pingo_render_diagnostics_ticks_to_us(
            renderer.diagnostics.clear_ticks, diagnostics_clock_hz);
        uint32_t transform_us = pingo_render_diagnostics_ticks_to_us(
            renderer.diagnostics.transform_ticks, diagnostics_clock_hz);
        uint32_t triangle_setup_us =
            pingo_render_diagnostics_ticks_to_us(
                renderer.diagnostics.triangle_setup_ticks,
                diagnostics_clock_hz);
        uint32_t raster_us = pingo_render_diagnostics_ticks_to_us(
            renderer.diagnostics.raster_ticks, diagnostics_clock_hz);

        force_debug_log(
            "PINGO_RENDER seq=%u bmid=%u render_us=%u "
            "d=4 w=%u h=%u fmt=%u far=%u "
            "cmd=%u pre=%u clr=%u xf=%u ts=%u "
            "ras=%u out=%u ob=%u obt=%u ofr=%u ta=%u "
            "ti=%u tz=%u tfr=%u tc=%u tu=%u tg=%u "
            "tp=%u tf=%u td=%u to=%u tr=%u tv=%u "
            "pt=%llu pc=%llu pz=%llu pd=%llu pu=%llu ps=%llu\n",
            sequence, bmid, render_elapsed_us,
            m_width, m_height,
            bitmap->format == PixelFormat::RGBA2222 ? 2 : 8,
            m_projection_far_units,
            command_us, prepare_us, clear_us, transform_us,
            triangle_setup_us, raster_us, output_us,
            renderer.diagnostics.objects,
            renderer.diagnostics.objects_bounds_tested,
            renderer.diagnostics.objects_frustum_rejected,
            renderer.diagnostics.triangles_avoided,
            renderer.diagnostics.triangles_submitted,
            renderer.diagnostics.triangles_z_rejected,
            renderer.diagnostics.triangles_frustum_rejected,
            renderer.diagnostics.triangles_clipped,
            renderer.diagnostics.triangles_unclipped,
            renderer.diagnostics.triangles_generated,
            renderer.diagnostics.triangles_projection_rejected,
            renderer.diagnostics.triangles_backface_rejected,
            renderer.diagnostics.triangles_degenerate,
            renderer.diagnostics.triangles_bbox_rejected,
            renderer.diagnostics.triangles_rasterized,
            renderer.diagnostics.triangles_bbox_clamped,
            (unsigned long long)renderer.diagnostics.fragments_bbox,
            (unsigned long long)renderer.diagnostics.fragments_covered,
            (unsigned long long)
                renderer.diagnostics.fragments_depth_range_rejected,
            (unsigned long long)
                renderer.diagnostics.fragments_depth_test_rejected,
            (unsigned long long)
                renderer.diagnostics.fragments_reciprocal_w_rejected,
            (unsigned long long)renderer.diagnostics.fragments_shaded);
        // Completion callbacks may synchronously clear or repurpose this control.
        // Keep notification last so diagnostics never dereference a torn-down
        // in-place Pingo object after send_packet() dispatches those callbacks.
        send_render_complete(sequence);
#else
        auto sequence = m_render_sequence++;
#if defined(USERSPACE) && PINGO_RENDER_TARGET_DUMP
        pingo_dump_render_target(
            bmid, sequence,
            bitmap->data,
            (uint32_t)m_width * m_height *
                (bitmap->format == PixelFormat::RGBA2222 ? 1U : 4U),
            bitmap->format == PixelFormat::RGBA2222,
            m_zeta,
            (uint32_t)m_width * m_height * sizeof(p3d::PingoDepth));
#endif
#if defined(USERSPACE) && PINGO_RENDER_TARGET_HASH
        force_debug_log(
            "PINGO_TARGET seq=%u bmid=%u bytes=%u fnv1a64=%016llx "
            "zbytes=%u zfnv1a64=%016llx\n",
            sequence, bmid,
            (uint32_t)m_width * m_height *
                (bitmap->format == PixelFormat::RGBA2222 ? 1U : 4U),
            (unsigned long long)pingo_fnv1a64(
                (const uint8_t *)bitmap->data,
                (uint32_t)m_width * m_height *
                    (bitmap->format == PixelFormat::RGBA2222 ? 1U : 4U)),
            (uint32_t)m_width * m_height * sizeof(p3d::PingoDepth),
            (unsigned long long)pingo_fnv1a64(
                (const uint8_t *)m_zeta,
                (uint32_t)m_width * m_height *
                    sizeof(p3d::PingoDepth)));
#endif
        force_debug_log("PINGO_RENDER seq=%u bmid=%u render_us=%u\n",
            sequence, bmid, render_elapsed_us);
        // Completion is deliberately last: RGBA8888 compatibility expansion
        // and restoration of Pingo's private frame have both finished.
        send_render_complete(sequence);
#endif
        //debug_log("Frame data:  %02hX %02hX %02hX %02hX\n", m_frame->r, m_frame->g, m_frame->b, m_frame->a);
        //debug_log("Final data:  %02hX %02hX %02hX %02hX\n", dst_pix->r, dst_pix->g, dst_pix->b, dst_pix->a);
    }

} Pingo3dControl;

static_assert(std::is_trivially_copyable<Pingo3dControl>::value,
    "Pingo3dControl initialization clears its complete object representation");
static_assert(std::is_trivially_destructible<Pingo3dControl>::value,
    "Pingo3dControl uses explicit in-place lifetime management");

#ifdef USERSPACE
static Pingo3dControl * pingo_userspace_get_control(uint16_t buffer_id) {
    auto buffer_iter = buffers.find(buffer_id);
    if (buffer_iter == buffers.end()) {
        return nullptr;
    }
    auto& blocks = buffer_iter->second;
    if (blocks.size() != 1 || !blocks.front() ||
        blocks.front()->size() < sizeof(Pingo3dControl) ||
        !blocks.front()->getBuffer()) {
        return nullptr;
    }
    auto control = reinterpret_cast<Pingo3dControl *>(
        blocks.front()->getBuffer());
    return control->validate() && control->is_registered_as(buffer_id)
        ? control
        : nullptr;
}

extern "C" bool pingo_userspace_control_exists(uint16_t buffer_id) {
    return pingo_userspace_get_control(buffer_id) != nullptr;
}

extern "C" uint32_t pingo_userspace_control_size() {
    return sizeof(Pingo3dControl);
}

extern "C" bool pingo_userspace_get_projection_far_units(
        uint16_t buffer_id, uint16_t * far_units) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !far_units) {
        return false;
    }
    *far_units = control->m_projection_far_units;
    return true;
}

extern "C" bool pingo_userspace_get_object_scale(
        uint16_t buffer_id, uint16_t object_id, float * scale) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !scale) {
        return false;
    }
    auto object = control->m_objects->find(object_id);
    if (object == control->m_objects->end()) {
        return false;
    }
    scale[0] = object->second.m_scale.x;
    scale[1] = object->second.m_scale.y;
    scale[2] = object->second.m_scale.z;
    return true;
}

extern "C" bool pingo_userspace_get_object_translation(
        uint16_t buffer_id, uint16_t object_id,
        float * translation, float * matrix_translation,
        uint8_t * modified) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !translation || !matrix_translation || !modified) {
        return false;
    }
    auto object = control->m_objects->find(object_id);
    if (object == control->m_objects->end()) {
        return false;
    }
    translation[0] = object->second.m_translation.x;
    translation[1] = object->second.m_translation.y;
    translation[2] = object->second.m_translation.z;
    matrix_translation[0] = object->second.m_transform.elements[3];
    matrix_translation[1] = object->second.m_transform.elements[7];
    matrix_translation[2] = object->second.m_transform.elements[11];
    *modified = object->second.m_modified ? 1 : 0;
    return true;
}

extern "C" bool pingo_userspace_get_object_active(
        uint16_t buffer_id, uint16_t object_id, uint8_t * active) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !active) {
        return false;
    }
    auto object = control->m_objects->find(object_id);
    if (object == control->m_objects->end()) {
        return false;
    }
    *active = object->second.m_object.inactive ? 0 : 1;
    return true;
}

extern "C" bool pingo_userspace_object_uses_mesh(
        uint16_t buffer_id, uint16_t object_id, uint16_t mesh_id) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control) {
        return false;
    }
    auto object = control->m_objects->find(object_id);
    auto mesh = control->m_meshes->find(mesh_id);
    return object != control->m_objects->end() &&
        mesh != control->m_meshes->end() &&
        object->second.m_object.mesh == &mesh->second;
}

extern "C" bool pingo_userspace_get_mesh_stream_state(
        uint16_t buffer_id, uint16_t mesh_id,
        uint32_t * vertex_count, uint32_t * position_index_count,
        uint32_t * texture_coordinate_count, uint32_t * texture_index_count,
        uint8_t * geometry_valid, uint8_t * bounds_valid) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !vertex_count || !position_index_count ||
        !texture_coordinate_count || !texture_index_count ||
        !geometry_valid || !bounds_valid) {
        return false;
    }
    auto mesh = control->m_meshes->find(mesh_id);
    if (mesh == control->m_meshes->end()) {
        return false;
    }
    *vertex_count = mesh->second.positions_count;
    *position_index_count =
        mesh->second.indexes_count < 0
            ? 0U
            : (uint32_t)mesh->second.indexes_count;
    *texture_coordinate_count =
        mesh->second.texture_coordinates_count;
    *texture_index_count = mesh->second.texture_indexes_count;
    *geometry_valid = mesh->second.geometry_valid;
    *bounds_valid = mesh->second.bounds_valid;
    return true;
}

extern "C" bool pingo_userspace_get_mesh_stream_vertex(
        uint16_t buffer_id, uint16_t mesh_id, uint32_t vertex_index,
        float * position) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !position) {
        return false;
    }
    auto mesh = control->m_meshes->find(mesh_id);
    if (mesh == control->m_meshes->end() ||
        !mesh->second.positions ||
        vertex_index >= mesh->second.positions_count) {
        return false;
    }
    position[0] = mesh->second.positions[vertex_index].x;
    position[1] = mesh->second.positions[vertex_index].y;
    position[2] = mesh->second.positions[vertex_index].z;
    return true;
}

extern "C" bool pingo_userspace_get_mesh_stream_uv(
        uint16_t buffer_id, uint16_t mesh_id, uint32_t coordinate_index,
        float * coordinate) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !coordinate) {
        return false;
    }
    auto mesh = control->m_meshes->find(mesh_id);
    if (mesh == control->m_meshes->end() ||
        !mesh->second.textCoord ||
        coordinate_index >= mesh->second.texture_coordinates_count) {
        return false;
    }
    coordinate[0] = mesh->second.textCoord[coordinate_index].x;
    coordinate[1] = mesh->second.textCoord[coordinate_index].y;
    return true;
}

extern "C" bool pingo_userspace_get_mesh_stream_indices(
        uint16_t buffer_id, uint16_t mesh_id, uint32_t index_offset,
        uint16_t * position_index, uint16_t * texture_index) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !position_index || !texture_index) {
        return false;
    }
    auto mesh = control->m_meshes->find(mesh_id);
    if (mesh == control->m_meshes->end() ||
        !mesh->second.pos_indices || !mesh->second.tex_indices ||
        mesh->second.indexes_count < 0 ||
        index_offset >= (uint32_t)mesh->second.indexes_count ||
        index_offset >= mesh->second.texture_indexes_count) {
        return false;
    }
    *position_index = mesh->second.pos_indices[index_offset];
    *texture_index = mesh->second.tex_indices[index_offset];
    return true;
}

extern "C" bool pingo_userspace_get_scene_scale(
        uint16_t buffer_id, float * scale) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !scale) {
        return false;
    }
    scale[0] = control->m_scene.m_scale.x;
    scale[1] = control->m_scene.m_scale.y;
    scale[2] = control->m_scene.m_scale.z;
    return true;
}

static uint64_t pingo_userspace_hash_upload_bytes(
        uint64_t hash, const void * data, size_t size) {
    auto bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

extern "C" bool pingo_userspace_get_upload_state_hash(
        uint16_t buffer_id, uint16_t mesh_id, uint16_t object_id,
        uint64_t * state_hash) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !state_hash) {
        return false;
    }
    auto mesh_iter = control->m_meshes->find(mesh_id);
    auto object_iter = control->m_objects->find(object_id);
    if (mesh_iter == control->m_meshes->end() ||
        object_iter == control->m_objects->end()) {
        return false;
    }

    auto& mesh = mesh_iter->second;
    auto& object = object_iter->second.m_object;
    if ((mesh.positions_count && !mesh.positions) ||
        (mesh.indexes_count && !mesh.pos_indices) ||
        (mesh.texture_coordinates_count && !mesh.textCoord) ||
        (mesh.texture_indexes_count && !mesh.tex_indices) ||
        (object.textCoord_count && !object.textCoord)) {
        return false;
    }

    uint64_t hash = 14695981039346656037ULL;
#define PINGO_HASH_UPLOAD_FIELD(field) \
    hash = pingo_userspace_hash_upload_bytes( \
        hash, &(field), sizeof(field))
    PINGO_HASH_UPLOAD_FIELD(mesh.positions_count);
    PINGO_HASH_UPLOAD_FIELD(mesh.indexes_count);
    PINGO_HASH_UPLOAD_FIELD(mesh.texture_coordinates_count);
    PINGO_HASH_UPLOAD_FIELD(mesh.texture_indexes_count);
    PINGO_HASH_UPLOAD_FIELD(mesh.geometry_valid);
    PINGO_HASH_UPLOAD_FIELD(mesh.bounds_valid);
    PINGO_HASH_UPLOAD_FIELD(mesh.bounds_min.x);
    PINGO_HASH_UPLOAD_FIELD(mesh.bounds_min.y);
    PINGO_HASH_UPLOAD_FIELD(mesh.bounds_min.z);
    PINGO_HASH_UPLOAD_FIELD(mesh.bounds_max.x);
    PINGO_HASH_UPLOAD_FIELD(mesh.bounds_max.y);
    PINGO_HASH_UPLOAD_FIELD(mesh.bounds_max.z);
    PINGO_HASH_UPLOAD_FIELD(mesh.shading_mode);
    PINGO_HASH_UPLOAD_FIELD(mesh.illumination_policy);
    PINGO_HASH_UPLOAD_FIELD(object.textCoord_count);
    PINGO_HASH_UPLOAD_FIELD(object.texture_mapping_valid);
#undef PINGO_HASH_UPLOAD_FIELD
    hash = pingo_userspace_hash_upload_bytes(
        hash, mesh.positions,
        (size_t)mesh.positions_count * sizeof(*mesh.positions));
    hash = pingo_userspace_hash_upload_bytes(
        hash, mesh.pos_indices,
        (size_t)mesh.indexes_count * sizeof(*mesh.pos_indices));
    hash = pingo_userspace_hash_upload_bytes(
        hash, mesh.textCoord,
        (size_t)mesh.texture_coordinates_count * sizeof(*mesh.textCoord));
    hash = pingo_userspace_hash_upload_bytes(
        hash, mesh.tex_indices,
        (size_t)mesh.texture_indexes_count * sizeof(*mesh.tex_indices));
    hash = pingo_userspace_hash_upload_bytes(
        hash, object.textCoord,
        (size_t)object.textCoord_count * sizeof(*object.textCoord));
    *state_hash = hash;
    return true;
}

extern "C" bool pingo_userspace_get_object_texture_pixel(
        uint16_t buffer_id, uint16_t object_id, uint32_t pixel_index,
        uint8_t * pixel) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !pixel) {
        return false;
    }
    auto object = control->m_objects->find(object_id);
    if (object == control->m_objects->end() ||
        !object->second.m_texture_binding ||
        !object->second.m_texture_binding->m_bitmap ||
        !object->second.m_texture.frameBuffer) {
        return false;
    }
    auto bitmap = object->second.m_texture_binding->m_bitmap;
    uint32_t pixel_count =
        (uint32_t)bitmap->width * (uint32_t)bitmap->height;
    if (pixel_index >= pixel_count) {
        return false;
    }
    *pixel = p3d::texture_read(
        &object->second.m_texture,
        p3d::Vec2i{
            (p3d::I_TYPE)(pixel_index % (uint32_t)bitmap->width),
            (p3d::I_TYPE)(pixel_index / (uint32_t)bitmap->width)
        }).c;
    return true;
}

extern "C" bool pingo_userspace_get_bitmap2222_pixel(
        uint16_t bitmap_id, uint32_t pixel_index, uint8_t * pixel) {
    auto bitmap = getBitmap(bitmap_id);
    if (!bitmap || !pixel || !bitmap->data ||
        bitmap->format != PixelFormat::RGBA2222 ||
        bitmap->width <= 0 || bitmap->height <= 0) {
        return false;
    }
    uint32_t pixel_count =
        (uint32_t)bitmap->width * (uint32_t)bitmap->height;
    if (pixel_index >= pixel_count) {
        return false;
    }
    *pixel = bitmap->data[pixel_index];
    return true;
}

extern "C" bool pingo_userspace_get_lighting_state(
        uint16_t buffer_id, float * direction,
        uint8_t * intensity, uint8_t * ambient, uint8_t * enabled) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !direction || !intensity || !ambient || !enabled) {
        return false;
    }
    direction[0] = control->m_light_direction.x;
    direction[1] = control->m_light_direction.y;
    direction[2] = control->m_light_direction.z;
    *intensity = control->m_light_intensity;
    *ambient = control->m_ambient_light;
    *enabled = control->m_illumination_enabled;
    return true;
}

extern "C" bool pingo_userspace_get_mesh_shading_mode(
        uint16_t buffer_id, uint16_t mesh_id, uint8_t * mode) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !mode) {
        return false;
    }
    auto mesh = control->m_meshes->find(mesh_id);
    if (mesh == control->m_meshes->end()) {
        return false;
    }
    *mode = mesh->second.shading_mode;
    return true;
}

extern "C" bool pingo_userspace_get_mesh_illumination_policy(
        uint16_t buffer_id, uint16_t mesh_id, uint8_t * policy) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !policy) {
        return false;
    }
    auto mesh = control->m_meshes->find(mesh_id);
    if (mesh == control->m_meshes->end()) {
        return false;
    }
    *policy = mesh->second.illumination_policy;
    return true;
}

extern "C" bool pingo_userspace_get_flat_pattern_state(
        uint16_t buffer_id,
        uint16_t * pattern_count,
        uint8_t * material_count,
        uint8_t * illumination_band_count,
        uint8_t * first_pattern_pixel,
        uint8_t * first_lookup_id) {
    auto control = pingo_userspace_get_control(buffer_id);
    if (!control || !control->m_flat_pattern_binding ||
        !pattern_count || !material_count || !illumination_band_count ||
        !first_pattern_pixel || !first_lookup_id) {
        return false;
    }
    auto& library = control->m_flat_pattern_binding->m_library;
    if (!library.patterns || !library.lookup ||
        !library.pattern_count || !library.material_count ||
        !library.illumination_band_count) {
        return false;
    }
    *pattern_count = library.pattern_count;
    *material_count = library.material_count;
    *illumination_band_count = library.illumination_band_count;
    *first_pattern_pixel = library.patterns[0];
    *first_lookup_id = library.lookup[0];
    return true;
}
#endif

extern "C" {

    void static_init(p3d::Renderer* ren, p3d::BackEnd* backEnd, p3d::Vec4i _rect) {
        //rect = _rect;
    }

    void static_before_render(p3d::Renderer* ren, p3d::BackEnd* backEnd) {
    }

    void static_after_render(p3d::Renderer* ren, p3d::BackEnd* backEnd) {
    }

    p3d::Pixel* static_get_frame_buffer(p3d::Renderer* ren, p3d::BackEnd* backEnd) {
        auto p_this = (struct tag_Pingo3dControl*) backEnd->clientCustomData;
        return p_this->m_frame;
    }

    p3d::PingoDepth* static_get_zeta_buffer(p3d::Renderer* ren, p3d::BackEnd* backEnd) {
        auto p_this = (struct tag_Pingo3dControl*) backEnd->clientCustomData;
        return p_this->m_zeta;
    }

#if DEBUG
    void show_pixel(float x, float y, uint8_t a, uint8_t b, uint8_t g, uint8_t r) {
        debug_log("%f %f %02hX %02hX %02hX %02hX\n", x, y, a, b, g, r);
    }
#endif

} // extern "C"

#endif // PINGO_3D_H
