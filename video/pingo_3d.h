#ifndef PINGO_3D_H
#define PINGO_3D_H

#include <stdint.h>
#include <string.h>
#include <agon.h>
#include <map>
#include "esp_heap_caps.h"
#include "sprites.h"
#include "vdu_stream_processor.h"

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

#define PI2                    6.283185307179586476925286766559f

class VDUStreamProcessor;typedef struct tag_Transformable {
    p3d::Vec3f      m_scale;
    p3d::Vec3f      m_rotation;
    p3d::Vec3f      m_translation;
    p3d::Mat4       m_transform;
    bool            m_modified;

    p3d::Vec3f      m_rotation_loc;
    p3d::Vec3f      m_translation_loc;

    bool            m_modified_loc;
    bool            m_is_camera;

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

    // Helper function to get the forward direction of the transformable
    p3d::Vec3f get_forward_direction() const {
        p3d::Mat4 rotation_matrix = p3d::mat4Identity();
        if (m_rotation.x) {
            auto t = p3d::mat4RotateX(m_rotation.x);
            rotation_matrix = mat4MultiplyM(&rotation_matrix, &t);
        }
        if (m_rotation.y) {
            auto t = p3d::mat4RotateY(m_rotation.y);
            rotation_matrix = mat4MultiplyM(&rotation_matrix, &t);
        }
        if (m_rotation.z) {
            auto t = p3d::mat4RotateZ(m_rotation.z);
            rotation_matrix = mat4MultiplyM(&rotation_matrix, &t);
        }

        // Forward direction is typically -Z in a right-handed coordinate system
        p3d::Vec3f forward = {0.0f, 0.0f, -1.0f};
        return p3d::mat4MultiplyVec3(&forward, &rotation_matrix);
    }

    void compute_transformation_matrix() {
        m_transform = p3d::mat4Scale(m_scale);
        if (m_is_camera) {
            if (m_rotation.x) {
                auto t = p3d::mat4RotateX(m_rotation.x);
                m_transform = mat4MultiplyM(&t, &m_transform); // arguments reversed
            }
            if (m_rotation.y) {
                auto t = p3d::mat4RotateY(m_rotation.y);
                m_transform = mat4MultiplyM(&t, &m_transform); // arguments reversed
            }
            if (m_rotation.z) {
                auto t = p3d::mat4RotateZ(m_rotation.z);
                m_transform = mat4MultiplyM(&t, &m_transform); // arguments reversed
            }
            if (m_translation.x || m_translation.y || m_translation.z) {
                auto t = p3d::mat4Translate(m_translation);
                m_transform = mat4MultiplyM(&t, &m_transform); // arguments reversed
            }

        } else {
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
        }

        m_modified = false;
    }

    void compute_transformation_matrix_local() {
        // Initialize the local transformation matrix
        p3d::Mat4 m_transform_loc = p3d::mat4Scale(m_scale);

        if (m_is_camera) {
            if (m_rotation_loc.x) {
                auto t = p3d::mat4RotateX(m_rotation_loc.x);
                m_transform_loc = mat4MultiplyM(&t, &m_transform_loc); // arguments reversed
            }
            if (m_rotation_loc.y) {
                auto t = p3d::mat4RotateY(m_rotation_loc.y);
                m_transform_loc = mat4MultiplyM(&t, &m_transform_loc); // arguments reversed
            }
            if (m_rotation_loc.z) {
                auto t = p3d::mat4RotateZ(m_rotation_loc.z);
                m_transform_loc = mat4MultiplyM(&t, &m_transform_loc); // arguments reversed
            }
            if (m_translation_loc.x || m_translation_loc.y || m_translation_loc.z) {
                auto t = p3d::mat4Translate(m_translation_loc);
                m_transform_loc = mat4MultiplyM(&t, &m_transform_loc); // arguments reversed
            }
            // Apply the local transformation matrix to the initial transform
            m_transform = mat4MultiplyM(&m_transform, &m_transform_loc); // arguments reversed

        } else {
            if (m_rotation_loc.x) {
                auto t = p3d::mat4RotateX(m_rotation_loc.x);
                m_transform_loc = mat4MultiplyM(&m_transform_loc, &t);
            }
            if (m_rotation_loc.y) {
                auto t = p3d::mat4RotateY(m_rotation_loc.y);
                m_transform_loc = mat4MultiplyM(&m_transform_loc, &t);
            }
            if (m_rotation_loc.z) {
                auto t = p3d::mat4RotateZ(m_rotation_loc.z);
                m_transform_loc = mat4MultiplyM(&m_transform_loc, &t);
            }
            if (m_translation_loc.x || m_translation_loc.y || m_translation_loc.z) {
                auto t = p3d::mat4Translate(m_translation_loc);
                m_transform_loc = mat4MultiplyM(&m_transform_loc, &t);
            }
            // Apply the local transformation matrix to the initial transform
            m_transform = mat4MultiplyM(&m_transform_loc, &m_transform);

        }

        // Clear local transformation values
        m_rotation_loc = {0.0f, 0.0f, 0.0f};
        m_translation_loc = {0.0f, 0.0f, 0.0f};

        m_modified_loc = false;
    }

    void update_transformation() {
        if (m_modified) {
            compute_transformation_matrix();
        }
    }

    void dump() {
        for (int i = 0; i < 16; i++) {
            debug_log("        [%i] %f\n", i, m_transform.elements[i]);
        }
        debug_log("Scale:       %f %f %f\n", m_scale.x, m_scale.y, m_scale.z);
        debug_log("Rotation:    %f %f %f\n", m_rotation.x * (180.0 / M_PI), m_rotation.y * (180.0 / M_PI), m_rotation.z * (180.0 / M_PI));
        debug_log("Translation: %f %f %f\n", m_translation.x, m_translation.y, m_translation.z);
    }
} Transformable;

typedef struct tag_TexObject : public Transformable {
    p3d::Object     m_object;
    p3d::Texture    m_texture;
    p3d::Material   m_material;
    uint16_t        m_oid;

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

    void update_transformation_matrix_loc() {
        compute_transformation_matrix_local();
        m_object.transform = m_transform;
    }

    void dump() {
        Transformable::dump();
        debug_log("TObject: %p %u\n", this, m_oid);
        debug_log("Object: %p %p %p %p\n", &m_object, m_object.material, m_object.mesh,
                    m_object.transform.elements);
        debug_log("Texture: %p %u %u %p\n", &m_texture, m_texture.size.x, m_texture.size.y, m_texture.pixels);
        debug_log("Material: %p %p %u %u %p\n", &m_material, m_material.texture, m_material.texture->size.x,
                    m_material.texture->size.y, m_material.texture->pixels);
    }
} TexObject;

struct tag_Pingo3dControl;

extern "C" {
    p3d::Pixel* static_get_frame_buffer(p3d::Renderer* ren, p3d::BackEnd* backEnd);
} // extern "C"

typedef struct tag_Pingo3dControl {
    uint32_t            m_tag;              // Used to verify the existence of this structure
    uint32_t            m_size;             // Used to verify the existence of this structure
    VDUStreamProcessor* m_proc;             // Used by subcommands to obtain more data
    p3d::BackEnd        m_backend;          // Used by the renderer
    p3d::Pixel          m_clearColor;        // Color to clear the frame buffer
    int                 m_clear;            // Clear type for the frame buffer
    uint16_t            m_width;            // Width of final render in pixels
    uint16_t            m_height;           // Height of final render in pixels
    p3d::Renderer       m_renderer;         // Renderer settings
    Transformable       m_camera;           // Camera transformation settings
    Transformable       m_scene;            // Scene transformation settings
    std::map<uint16_t, p3d::Mesh>* m_meshes;    // Map of meshes for use by objects
    std::map<uint16_t, TexObject>* m_objects;   // Map of textured objects that use meshes and have transforms
    uint8_t             m_dither_type;      // Dithering type and options to be applied to rendered bitmap

    void show_free_ram() {
        debug_log("Free PSRAM: %u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }

    // VDU 23, 0, &A0, sid; &49, 0, 1 :  Initialize Control Structure
    void initialize(VDUStreamProcessor& processor, uint16_t width, uint16_t height) {
        printf("initialize: pingo creating control structure for %ux%u scene\n", width, height);
        memset(this, 0, sizeof(tag_Pingo3dControl));
        m_tag = PINGO_3D_CONTROL_TAG;
        m_size = sizeof(tag_Pingo3dControl);
        m_width = width;
        m_height = height;
        m_camera.initialize_scale();
        m_scene.initialize_scale();

        auto frame_size = (uint32_t) m_width * (uint32_t) m_width;
        auto frame_dims = p3d::Vec2i{(int)m_width, (int)m_height};

        p3d::Camera camera;
        camera.near = 1.0;
        camera.far = 2500.0;
        // camera.fov = 1.5707963267948966f/2.0f;
        camera.fov = 0.5f;
        camera.aspect = (float)frame_dims.x / (float)frame_dims.y;
        camera.projection = p3d::mat4Perspective(camera.near, camera.far, camera.aspect, camera.fov);
        m_renderer.camera = camera;
        rendererSetCamera(&m_renderer, frame_dims);
        printf("Camera set to %ux%u\n", frame_dims.x, frame_dims.y);

        auto tgtbmp = getBitmap(257).get();
        m_renderer.frameBuffer.pixels = (p3d::Pixel*) tgtbmp->data;
        printf("Frame buffer set to %p\n", m_renderer.frameBuffer.pixels);

        m_renderer.clear = p3d::REND_CLEAR;
        m_renderer.clearColor = p3d::PIXELBLACK;
        auto bkgbmp = getBitmap(258).get();
        if (bkgbmp) {
            m_renderer.background.pixels = (p3d::Pixel*) bkgbmp->data;
            m_renderer.clear = p3d::REND_BACKGROUND;
        }
        printf("Background set to %p\n", m_renderer.background.pixels);

        rendererInit(&m_renderer, frame_dims, &m_backend );

        m_backend.getFrameBuffer = &static_get_frame_buffer;
        m_backend.drawPixel = NULL;
        m_backend.clientCustomData = (void*) this;

        m_meshes = new std::map<uint16_t, p3d::Mesh>;
        m_objects = new std::map<uint16_t, TexObject>;

        printf("Pingo3dControl initialized\n");
    }

    // VDU 23, 0, &A0, sid; &49, 38, bmid; :  Render To Bitmap
    void render_to_bitmap() {
        auto bmid = m_proc->readWord_t();
        if (bmid < 0) {
            return;
        }

        auto start = millis();

        auto frame_dims = p3d::Vec2i{(int)m_width, (int)m_height};

        p3d::Scene scene;
        sceneInit(&scene);
        p3d::rendererSetScene(&m_renderer, &scene);

        for (auto object = m_objects->begin(); object != m_objects->end(); object++) {
            object->second.bind();
            if (object->second.m_modified) {
                object->second.m_is_camera = false;
                object->second.update_transformation_matrix();
                //object->second.dump();
            }
            if (object->second.m_modified_loc) {
                object->second.m_is_camera = false;
                object->second.update_transformation_matrix_loc();
                //object->second.dump();
            }
            sceneAddRenderable(&scene, p3d::object_as_renderable(&object->second.m_object));
        }

        if (m_camera.m_modified) {
            m_camera.m_is_camera = true;
            m_camera.compute_transformation_matrix();
        }
        if (m_camera.m_modified_loc) {
            m_camera.m_is_camera = true;
            m_camera.compute_transformation_matrix_local();
        }
        //debug_log("Camera:\n");
        // m_camera.dump();
        m_renderer.camera.view = m_camera.m_transform;
        // print camera translation values
        // // Assuming m_camera.m_transform is a 4x4 matrix in row-major order
        // float tx = m_camera.m_transform.elements[3];   // X translation
        // float ty = m_camera.m_transform.elements[7];   // Y translation
        // float tz = m_camera.m_transform.elements[11];  // Z translation

        // // Print camera translation values
        // printf("Camera translation from matrix: %f %f %f\n", tx, ty, tz);

        if (m_scene.m_modified) {
            m_scene.compute_transformation_matrix();
        }
        scene.transform = m_scene.m_transform;

        //debug_log("Frame data:  %02hX %02hX %02hX %02hX\n", m_frame->r, m_frame->g, m_frame->b, m_frame->a);
        //debug_log("Destination: %02hX %02hX %02hX %02hX\n", dst_pix->r, dst_pix->g, dst_pix->b, dst_pix->a);

        rendererRender(&m_renderer);

        // Apply dithering to the rendered image before copying to the destination bitmap
        switch (m_dither_type) {
            case 0:
                break; // no dithering applied
            case 1:
                dither_bayer((uint8_t*)m_renderer.frameBuffer.pixels, m_width, m_height);
                break;
            case 2:
                dither_floyd_steinberg((uint8_t*)m_renderer.frameBuffer.pixels, m_width, m_height);
                break;
            default:
                m_dither_type = 0; // no dithering applied
                debug_log("Invalid dithering type %u\n", m_dither_type);
                break;
        }

        auto stop = millis();
        auto diff = stop - start;
        float fps = 1000.0 / diff;
        printf("Render to %ux%u took %u ms (%.2f FPS)\n", m_width, m_height, diff, fps);
    }

    // VDU 23, 0, &A0, sid; &49, 0, 0 :  Deinitialize Control Structure
    void deinitialize(VDUStreamProcessor& processor) {
    }

    bool validate() {
        return (m_tag == PINGO_3D_CONTROL_TAG &&
                m_size == sizeof(tag_Pingo3dControl));
    }

    void handle_subcommand(VDUStreamProcessor& processor, uint8_t subcmd) {
        debug_log("P3D: handle_subcommand(%hu)\n", subcmd);
        m_proc = &processor;
        switch (subcmd) {
            case 1: define_mesh_vertices(); break;
            case 2: set_mesh_vertex_indexes(); break;
            case 3: define_object_texture_coordinates(); break;
            case 4: set_object_texture_coordinate_indexes(); break;
            case 5: create_object(); break;

            case 6: set_object_x_scale_factor(); break;
            case 7: set_object_y_scale_factor(); break;
            case 8: set_object_z_scale_factor(); break;
            case 9: set_object_xyz_scale_factors(); break;

            case 10: set_object_x_rotation_angle(); break;
            case 11: set_object_y_rotation_angle(); break;
            case 12: set_object_z_rotation_angle(); break;
            case 13: set_object_xyz_rotation_angles(); break;
            case 141: set_object_xyz_rotation_angles_local(); break;

            case 14: set_object_x_translation_distance(); break;
            case 15: set_object_y_translation_distance(); break;
            case 16: set_object_z_translation_distance(); break;
            case 17: set_object_xyz_translation_distances(); break;
            case 145: set_object_xyz_translation_distances_local(); break;

            case 18: set_camera_x_rotation_angle(); break;
            case 19: set_camera_y_rotation_angle(); break;
            case 20: set_camera_z_rotation_angle(); break;
            case 21: set_camera_xyz_rotation_angles(); break;
            case 149: set_camera_xyz_rotation_angles_local(); break;
            case 42: camera_track_object(); break;

            case 22: set_camera_x_translation_distance(); break;
            case 23: set_camera_y_translation_distance(); break;
            case 24: set_camera_z_translation_distance(); break;
            case 25: set_camera_xyz_translation_distances(); break;
            case 153: set_camera_xyz_translation_distances_local(); break;

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
            case 41: set_rendering_dither_type(); break;
        }
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
            TexObject object;
            memset(&object, 0, sizeof(object));
            object.m_oid = oid;
            object.initialize();
            (*m_objects).insert(std::pair<uint16_t, TexObject>(oid, object));
            return &m_objects->find(oid)->second;
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

    // VDU 23, 0, &A0, sid; &49, 1, mid; n; x0; y0; z0; ... :  Define Mesh Vertices
    void define_mesh_vertices() {
        auto mesh = get_mesh();
        if (mesh->positions) {
            heap_caps_free(mesh->positions);
            mesh->positions = NULL;
        }
        auto n = (uint32_t) m_proc->readWord_t();
        if (n > 0) {
            auto size = n*sizeof(p3d::Vec3f);
            mesh->positions = (p3d::Vec3f*) heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            auto pos = mesh->positions;
            if (!pos) {
                debug_log("define_mesh_vertices: failed to allocate %u bytes\n", size);
                show_free_ram();
            }
            debug_log("Reading %u vertices\n", n);
            for (uint32_t i = 0; i < n; i++) {
                uint16_t x = m_proc->readWord_t();
                uint16_t y = m_proc->readWord_t();
                uint16_t z = m_proc->readWord_t();
                if (pos) {
                    pos->x = convert_position_value(x);
                    pos->y = convert_position_value(y);
                    pos->z = convert_position_value(z);
                    if (!(i & 0x1F)) debug_log("%u %f %f %f\n", i, pos->x, pos->y, pos->z);
                    pos++;
                }
            }
            debug_log("\n");
        }
    }

    // VDU 23, 0, &A0, sid; &49, 2, mid; n; i0; ... :  Set Mesh Vertex Indexes
    void set_mesh_vertex_indexes() {
        auto mesh = get_mesh();
        if (mesh->pos_indices) {
            heap_caps_free(mesh->pos_indices);
            mesh->pos_indices = NULL;
            mesh->indexes_count = 0;
        }
        auto n = (uint32_t) m_proc->readWord_t();
        if (n > 0) {
            mesh->indexes_count = n;
            auto size = n*sizeof(uint16_t);
            mesh->pos_indices = (uint16_t*) heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            auto idx = mesh->pos_indices;
            if (!idx) {
                debug_log("set_mesh_vertex_indexes: failed to allocate %u bytes\n", size);
                show_free_ram();
            }
            debug_log("Reading %u vertex indexes\n", n);
            for (uint32_t i = 0; i < n; i++) {
                uint16_t index = m_proc->readWord_t();
                if (idx) {
                    *idx++ = index;
                }
                if (!(i & 0x1F)) debug_log("%u %hu\n", i, index);
            }
            debug_log("\n");
        }
    }

    // VDU 23, 0, &A0, sid; &49, 3, oid; n; u0; v0; ... :  Define Object Texture Coordinates
    void define_object_texture_coordinates() {
        auto object = get_object();
        if (object->m_object.textCoord) {
            heap_caps_free(object->m_object.textCoord);
            object->m_object.textCoord = NULL;
        }
        auto n = (uint32_t) m_proc->readWord_t();
        if (n > 0) {
            auto size = n*sizeof(p3d::Vec2f);
            object->m_object.textCoord = (p3d::Vec2f*) heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            auto coord = object->m_object.textCoord;
            if (!coord) {
                debug_log("set_mesh_vertex_indexes: failed to allocate %u bytes\n", size);
                show_free_ram();
            }
            debug_log("Reading %u texture coordinates\n", n);
            for (uint32_t i = 0; i < n; i++) {
                uint16_t u = m_proc->readWord_t();
                uint16_t v = m_proc->readWord_t();
                if (coord) {
                    coord->x = convert_texture_coordinate_value(u);
                    // coord->y = convert_texture_coordinate_value(v);
                    coord->y = 1 - convert_texture_coordinate_value(v); // uvs are normal cartesian in blender
                    coord++;
                }
            }
        }
    }

    // VDU 23, 0, &A0, sid; &49, 4, mid; n; i0; ... :  Set Object Texture Coordinate Indexes
    void set_object_texture_coordinate_indexes() {
        auto object = get_object();
        if (object->m_object.tex_indices) {
            heap_caps_free(object->m_object.tex_indices);
            object->m_object.tex_indices = NULL;
        }
        auto n = (uint32_t) m_proc->readWord_t();
        if (n > 0) {
            auto size = n*sizeof(uint16_t);
            object->m_object.tex_indices = (uint16_t*) heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            auto idx = object->m_object.tex_indices;
            if (!idx) {
                debug_log("set_texture_coordinate_indexes: failed to allocate %u bytes\n", size);
                show_free_ram();
            }
            debug_log("Reading %u texture coordinate indexes\n", n);
            for (uint32_t i = 0; i < n; i++) {
                uint16_t index = m_proc->readWord_t();
            // this check isn't strictly necessary, and if it fails we have bigger issues
            // but if we do it, adding indexes_count to Object is one way to tackle it
            // however, that requires mods to the Object creation routines and other stuff so why bother?
                // if (idx && (i < object->m_object.indexes_count)) {
                //     *idx++ = index;
                // }
                *idx++ = index;
                if (!(i & 0x1F)) debug_log("%u %hu\n", i, index);
            }
        }
    }

    // VDU 23, 0, &A0, sid; &49, 5, oid; mid; bmid; :  Create Object
    void create_object() {
        auto object = get_object();
        auto mesh = get_mesh();
        auto bmid = m_proc->readWord_t();
        // if (object && mesh && bmid) {
        if (object && mesh) {
            printf("Creating 3D object %u with bitmap %u\n", object->m_oid, bmid);
            auto stored_bitmap = getBitmap(bmid);
            if (stored_bitmap) {
                auto bitmap = stored_bitmap.get();
                if (bitmap) {
                    auto size = p3d::Vec2i{(int)bitmap->width, (int)bitmap->height};
                    auto pix = (p3d::Pixel*) bitmap->data;
                    // object->bind();
                    texture_init(&object->m_texture, size, pix);
                    // object->m_object.mesh = mesh;
                    // debug_log("Texture data:  %02hX %02hX %02hX %02hX\n", pix->r, pix->g, pix->b, pix->a);
                }
            }
            object->bind();
            object->m_object.mesh = mesh;
            printf("Object %u created\n", object->m_oid);
        }
    }

    float convert_scale_value(int32_t value) {
        static const float factor = 1.0f / 256.0f;
        return ((float) value) * factor;
    }

    float convert_rotation_value(int32_t value) {
        if (value & 0x8000) {
            value = (int32_t)(int16_t)(uint16_t) value;
        }
        static const float factor = (2.0f * 3.1415926f) / 32767.0f;
        return ((float) value) * factor;
    }

    float convert_translation_value(int32_t value) {
        if (value & 0x8000) {
            value = (int32_t)(int16_t)(uint16_t) value;
        }
        static const float factor = 256.0f / 32767.0f;
        return ((float) value) * factor;
    }

    float convert_position_value(int32_t value) {
        if (value & 0x8000) {
            value = (int32_t)(int16_t)(uint16_t) value;
        }
        static const float factor = 1.0f / 32767.0f;
        return ((float) value) * factor;
    }

    float convert_texture_coordinate_value(int32_t value) {
        static const float factor = 1.0f / 65535.0f;
        return ((float) value) * factor;
    }

    // VDU 23, 0, &A0, sid; &49, 6, oid; scalex; :  Set Object X Scale Factor
    void set_object_x_scale_factor() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object && (value >= 0)) {
            object->m_scale.x = convert_scale_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 7, oid; scaley; :  Set Object Y Scale Factor
    void set_object_y_scale_factor() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object && (value >= 0)) {
            object->m_scale.y = convert_scale_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 8, oid; scalez; :  Set Object Z Scale Factor
    void set_object_z_scale_factor() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object && (value >= 0)) {
            object->m_scale.y = convert_scale_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 9, oid; scalex; scaley; scalez :  Set Object XYZ Scale Factors
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

    // VDU 23, 0, &A0, sid; &49, 10, oid; anglex; :  Set Object X Rotation Angle
    void set_object_x_rotation_angle() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_rotation.x = convert_rotation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 11, oid; angley; :  Set Object Y Rotation Angle
    void set_object_y_rotation_angle() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_rotation.y = convert_rotation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 12, oid; anglez; :  Set Object Z Rotation Angle
    void set_object_z_rotation_angle() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_rotation.z = convert_rotation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 13, oid; anglex; angley; anglez; :  Set Object XYZ Rotation Angles
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

    // VDU 23, 0, &A0, sid; &49, 141, oid; anglex; angley; anglez; :  Set Object XYZ Rotation Angles Local
    void set_object_xyz_rotation_angles_local() {
        auto object = get_object();
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        if (object) {
            object->m_rotation_loc.x = convert_rotation_value(valuex);
            object->m_rotation_loc.y = convert_rotation_value(valuey);
            object->m_rotation_loc.z = convert_rotation_value(valuez);
            object->m_modified_loc = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 14, oid; distx; :  Set Object X Translation Distance
    void set_object_x_translation_distance() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_translation.x = convert_translation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 15, oid; disty; :  Set Object Y Translation Distance
    void set_object_y_translation_distance() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_translation.y = convert_translation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 16, oid; distz; :  Set Object Z Translation Distance
    void set_object_z_translation_distance() {
        auto object = get_object();
        auto value = m_proc->readWord_t();
        if (object) {
            object->m_translation.z = convert_translation_value(value);
            object->m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 17, oid; distx; disty; distz :  Set Object XYZ Translation Distances
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
        }
    }

    // VDU 23, 0, &A0, sid; &49, 145, oid; distx; disty; distz :  Set Object XYZ Translation Distances Local
    void set_object_xyz_translation_distances_local() {
        auto object = get_object();
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        if (object) {
            object->m_translation_loc.x = convert_translation_value(valuex);
            object->m_translation_loc.y = convert_translation_value(valuey);
            object->m_translation_loc.z = convert_translation_value(valuez);
            object->m_modified_loc = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 18, anglex; :  Set Camera X Rotation Angle
    void set_camera_x_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_camera.m_rotation.x = convert_rotation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 19, angley; :  Set Camera Y Rotation Angle
    void set_camera_y_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_camera.m_rotation.y = convert_rotation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 20, anglez; :  Set Camera Z Rotation Angle
    void set_camera_z_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_camera.m_rotation.z = convert_rotation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 21, anglex; angley; anglez; :  Set Camera XYZ Rotation Angles
    void set_camera_xyz_rotation_angles() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_camera.m_rotation.x = convert_rotation_value(valuex);
        m_camera.m_rotation.y = convert_rotation_value(valuey);
        m_camera.m_rotation.z = convert_rotation_value(valuez);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 149, anglex; angley; anglez; :  Set Camera XYZ Rotation Angles Local
    void set_camera_xyz_rotation_angles_local() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_camera.m_rotation_loc.x = convert_rotation_value(valuex);
        m_camera.m_rotation_loc.y = convert_rotation_value(valuey);
        m_camera.m_rotation_loc.z = convert_rotation_value(valuez);
        m_camera.m_modified_loc = true;
    }

    // VDU 23, 0, &A0, sid; &49, 42, oid; : Rotate Camera to track a specified object
    void camera_track_object() {
        auto object = get_object();
        if (object) {
            // Extract camera position from the camera's transformation matrix (row-major order)
            p3d::Vec3f camera_position = { 
                m_camera.m_transform.elements[3],  // x
                m_camera.m_transform.elements[7],  // y
                m_camera.m_transform.elements[11]  // z
            };

            // Extract object position from the object's transformation matrix (row-major order)
            p3d::Vec3f object_position = { 
                object->m_transform.elements[3],   // x
                object->m_transform.elements[7],   // y
                object->m_transform.elements[11]   // z
            };

            // Calculate the direction vector from the camera to the object
            p3d::Vec2f direction_to_object = {
                object_position.x - camera_position.x,
                object_position.z - camera_position.z  // Mapping z to y in Vec2f
            };

            // Normalize the direction vector
            float length = sqrt(
                direction_to_object.x * direction_to_object.x +
                direction_to_object.y * direction_to_object.y
            );
            if (length > 0) {
                direction_to_object.x /= length;
                direction_to_object.y /= length;
            }

            // Calculate yaw (rotation around the Y axis)
            float yaw = atan2(direction_to_object.x, -direction_to_object.y);

            m_camera.m_rotation.y = yaw;
            m_camera.m_rotation.x = 0;
            m_camera.m_rotation.z = 0;
            m_camera.m_modified = true;

            // // Debug output
            // printf("Camera: x=%.2f, y=%.2f, z=%.2f Target: x=%.2f, y=%.2f, z=%.2f Angle: %.2f degrees\n", 
            //     camera_position.x, camera_position.y, camera_position.z, 
            //     object_position.x, object_position.y, object_position.z, 
            //     yaw * (180.0f / M_PI));
        }
    }

    // VDU 23, 0, &A0, sid; &49, 22, distx; :  Set Camera X Translation Distance
    void set_camera_x_translation_distance() {
        auto value = m_proc->readWord_t();
        m_camera.m_translation.x = convert_translation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 23, disty; :  Set Camera Y Translation Distance
    void set_camera_y_translation_distance() {
        auto value = m_proc->readWord_t();
        m_camera.m_translation.y = convert_translation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 24, distz; :  Set Camera Z Translation Distance
    void set_camera_z_translation_distance() {
        auto value = m_proc->readWord_t();
        m_camera.m_translation.z = convert_translation_value(value);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 25, distx; disty; distz :  Set Camera XYZ Translation Distances
    void set_camera_xyz_translation_distances() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_camera.m_translation.x = convert_translation_value(valuex);
        m_camera.m_translation.y = convert_translation_value(valuey);
        m_camera.m_translation.z = convert_translation_value(valuez);
        m_camera.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 153, distx; disty; distz :  Set Camera XYZ Translation Distances Local
    void set_camera_xyz_translation_distances_local() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_camera.m_translation_loc.x = convert_translation_value(valuex);
        m_camera.m_translation_loc.y = convert_translation_value(valuey);
        m_camera.m_translation_loc.z = convert_translation_value(valuez);
        m_camera.m_modified_loc = true;
    }

    // VDU 23, 0, &A0, sid; &49, 26, scalex; :  Set Scene X Scale Factor
    void set_scene_x_scale_factor() {
        auto value = m_proc->readWord_t();
        if (value >= 0) {
            m_scene.m_scale.x = convert_scale_value(value);
            m_scene.m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 27, scaley; :  Set Scene Y Scale Factor
    void set_scene_y_scale_factor() {
        auto value = m_proc->readWord_t();
        if (value >= 0) {
            m_scene.m_scale.y = convert_scale_value(value);
            m_scene.m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 28, scalez; :  Set Scene Z Scale Factor
    void set_scene_z_scale_factor() {
        auto value = m_proc->readWord_t();
        if (value >= 0) {
            m_scene.m_scale.y = convert_scale_value(value);
            m_scene.m_modified = true;
        }
    }

    // VDU 23, 0, &A0, sid; &49, 29, scalex; scaley; scalez :  Set Scene XYZ Scale Factors
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

    // VDU 23, 0, &A0, sid; &49, 30, anglex; :  Set Scene X Rotation Angle
    void set_scene_x_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_scene.m_rotation.x = convert_rotation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 31, angley; :  Set Scene Y Rotation Angle
    void set_scene_y_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_scene.m_rotation.y = convert_rotation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 32, anglez; :  Set Scene Z Rotation Angle
    void set_scene_z_rotation_angle() {
        auto value = m_proc->readWord_t();
        m_scene.m_rotation.z = convert_rotation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 33, anglex; angley; anglez; :  Set Scene XYZ Rotation Angles
    void set_scene_xyz_rotation_angles() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_scene.m_rotation.x = convert_rotation_value(valuex);
        m_scene.m_rotation.y = convert_rotation_value(valuey);
        m_scene.m_rotation.z = convert_rotation_value(valuez);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 34, distx; :  Set Scene X Translation Distance
    void set_scene_x_translation_distance() {
        auto value = m_proc->readWord_t();
        m_scene.m_translation.x = convert_translation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 35, disty; :  Set Scene Y Translation Distance
    void set_scene_y_translation_distance() {
        auto value = m_proc->readWord_t();
        m_scene.m_translation.y = convert_translation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 36, distz; :  Set Scene Z Translation Distance
    void set_scene_z_translation_distance() {
        auto value = m_proc->readWord_t();
        m_scene.m_translation.z = convert_translation_value(value);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 37, distx; disty; distz :  Set Scene XYZ Translation Distances
    void set_scene_xyz_translation_distances() {
        auto valuex = m_proc->readWord_t();
        auto valuey = m_proc->readWord_t();
        auto valuez = m_proc->readWord_t();
        m_scene.m_translation.x = convert_translation_value(valuex);
        m_scene.m_translation.y = convert_translation_value(valuey);
        m_scene.m_translation.z = convert_translation_value(valuez);
        m_scene.m_modified = true;
    }

    // VDU 23, 0, &A0, sid; &49, 41, type : Set Rendering Dither Type
    void set_rendering_dither_type() {
        auto type = m_proc->readByte_t();
        m_dither_type = type & 0x03; // Mask the first two bits
        switch (m_dither_type) {
            case 0:
                debug_log("Dithering disabled\n");
                break;
            case 1:
                debug_log("Dithering enabled (Bayer)\n");
                break;
            case 2:
                debug_log("Dithering enabled (Floyd-Steinberg)\n");
                break;
            default:
                m_dither_type = 0;
                debug_log("Invalid dithering type %u\n", m_dither_type);
                break;
        }
    }

    void dither_bayer(uint8_t* rgba, int width, int height) {
        static const uint8_t bayer[4][4] = {
            { 15, 135,  45, 165},
            {195,  75, 225, 105},
            { 60, 180,  30, 150},
            {240, 120, 210,  90}
        };
        
        static const uint8_t quant_levels[4] = {0, 85, 170, 255};

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                uint8_t* pixel = &rgba[(y * width + x) * 4];
                if (pixel[3] < 255) {
                    // Skip dithering for pixels with alpha channel value < 255
                    continue;
                }
                for (int color = 0; color < 3; color++) { // Only process R, G, B channels
                    uint8_t normalized_pixel_value = pixel[color];
                    uint8_t threshold = bayer[x % 4][y % 4];
                    
                    // Determine the quantization level
                    int level_index = (normalized_pixel_value * 3) / 255;
                    
                    if (normalized_pixel_value > threshold) {
                        level_index = (level_index < 3) ? level_index + 1 : 3;
                    }

                    pixel[color] = quant_levels[level_index];
                }
            }
        }
    }

    // Clamp function to ensure pixel values stay within the valid range
    uint8_t clamp(int value) {
        if (value < 0) return 0;
        if (value > 255) return 255;
        return (uint8_t)value;
    }

    // Find the closest color value (0, 85, 170, 255)
    uint8_t closest_color(uint8_t value) {
        if (value < 43) return 0;
        if (value < 128) return 85;
        if (value < 213) return 170;
        return 255;
    }

    void dither_floyd_steinberg(uint8_t* rgba, int width, int height) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint8_t* pixel = &rgba[(y * width + x) * 4];
                if (pixel[3] < 255) {
                    // Skip dithering for pixels with alpha channel value < 255
                    continue;
                }
                for (int color = 0; color < 3; color++) {
                    int old_pixel = pixel[color];
                    int new_pixel = closest_color(old_pixel);
                    pixel[color] = new_pixel;
                    int error = old_pixel - new_pixel;

                    if (x + 1 < width) {
                        rgba[(y * width + (x + 1)) * 4 + color] = clamp(rgba[(y * width + (x + 1)) * 4 + color] + (error * 7 / 16));
                    }
                    if (y + 1 < height) {
                        if (x > 0) {
                            rgba[((y + 1) * width + (x - 1)) * 4 + color] = clamp(rgba[((y + 1) * width + (x - 1)) * 4 + color] + (error * 3 / 16));
                        }
                        rgba[((y + 1) * width + x) * 4 + color] = clamp(rgba[((y + 1) * width + x) * 4 + color] + (error * 5 / 16));
                        if (x + 1 < width) {
                            rgba[((y + 1) * width + (x + 1)) * 4 + color] = clamp(rgba[((y + 1) * width + (x + 1)) * 4 + color] + (error * 1 / 16));
                        }
                    }
                }
            }
        }
    }

} Pingo3dControl;

extern "C" {

    p3d::Pixel* static_get_frame_buffer(p3d::Renderer* ren, p3d::BackEnd* backEnd) {
        auto p_this = (struct tag_Pingo3dControl*) backEnd->clientCustomData;
        return p_this->m_renderer.frameBuffer.pixels;
    }

#if DEBUG
    void show_pixel(float x, float y, uint8_t a, uint8_t b, uint8_t g, uint8_t r) {
        debug_log("%f %f %02hX %02hX %02hX %02hX\n", x, y, a, b, g, r);
    }
#endif

} // extern "C"

#endif // PINGO_3D_H
