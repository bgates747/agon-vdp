/* P027 target owner: accepted P026 transaction and P030 wire revision.
 * Engine remains unchanged. Platform storage and borrowed lifetimes stay here. */
#include "pingo2_commands.h"
#ifndef PINGO2_BRIDGE_DIAGNOSTICS
#define P2CMD_NO_DIAGNOSTICS 1
#endif
#include "math/mat4.h"
#include "render/backend.h"
#include "render/depth.h"
#include "render/entity.h"
#include "render/material.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/pixel.h"
#include "render/renderer.h"
#include "render/texture.h"
#include "render/state.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if !defined(P2C_PIXEL_RGBA2222) || defined(P2C_DIAGNOSTICS)
#error "command proof uses the native engine without facade-specific diagnostics"
#endif

typedef struct Backing {
    size_t refs; uint8_t *data; size_t bytes;
    void *owner; void (*drop)(void *);
} Backing;
typedef struct Buffer {
    struct Buffer *next;
    uint16_t id;
    uint8_t *data;
    size_t bytes, blocks;
    void *owner; void (*drop)(void *);
} Buffer;
typedef struct Bitmap {
    struct Bitmap *next;
    size_t refs;
    uint16_t id, width, height;
    unsigned format;
    Backing *backing;
} Bitmap;
typedef struct Geometry {
    struct Geometry *next;
    uint16_t id;
    Mesh mesh;
    uint32_t uv_count, uv_index_count;
} Geometry;
typedef struct Pose { Vec3f scale, rotation, translation; } Pose;
typedef struct Instance {
    struct Instance *next;
    uint16_t id;
    int active, valid;
    Pose pose;
    Geometry *geometry;
    Vec2f *uv;
    uint32_t uv_count;
    Mesh view;
    Object object;
    Material material;
    Texture texture;
    Bitmap *bitmap;
    Backing *backing;
    Pixel *converted;
} Instance;
typedef struct Scene Scene;
/* Standard-layout owner wrapper. No offset into a C++ control, no global. */
typedef struct OwnedBackend { Backend base; Scene *owner; } OwnedBackend;
_Static_assert(offsetof(OwnedBackend, base) == 0, "embedded backend is first");
_Static_assert(sizeof(PingoDepth) == sizeof(uint32_t), "qualified depth ABI");
struct Scene {
    Scene *next;
    P2Commands *host;
    uint16_t id, width, height, token;
    uint8_t notify;
    P2CommandInfo info;
    P2CommandWork work;
    Pose camera, root_pose;
    Geometry *meshes;
    Instance *objects;
    Entity *children;
    size_t capacity;
    OwnedBackend backend;
    Renderer renderer;
    Renderable empty;
    Entity root;
    Pixel *scratch, *frame;
    PingoDepth *depth;
};
struct P2Commands {
    Scene *scenes;
    Bitmap *bitmaps;
    Buffer *buffers;
    int64_t fail_after;
    size_t live;
    int busy;
    P2Completion completion;
    void *context;
    P2RenderHook render_hook;
    void *render_context;
    P2CommandStats stats;
    int staging;
};

static uint64_t now_ns(void);
uint32_t p2cmd_wire_revision(void) { return P2CMD_WIRE_REVISION; }
#ifndef P2CMD_NO_DIAGNOSTICS
typedef union AllocationHeader {
    max_align_t alignment;
    struct { size_t bytes; int staged; } meta;
} AllocationHeader;
#endif

static void *allocate(P2Commands *h, size_t n, size_t size) {
    if (!n || !size || n > SIZE_MAX / size || h->fail_after == 0) return NULL;
    if (h->fail_after > 0) --h->fail_after;
    void *p;
#ifndef P2CMD_NO_DIAGNOSTICS
    size_t bytes = n * size;
    if (bytes > SIZE_MAX - sizeof(AllocationHeader)) return NULL;
    AllocationHeader *header = p2bridge_calloc(1, sizeof(*header) + bytes);
    p = header ? header + 1 : NULL;
    if (header) {
        header->meta.bytes = bytes; header->meta.staged = h->staging;
        h->stats.live_bytes += bytes;
        if (h->stats.live_bytes > h->stats.peak_bytes) h->stats.peak_bytes = h->stats.live_bytes;
        if (bytes > h->stats.largest_allocation) h->stats.largest_allocation = bytes;
        if (h->staging) {
            h->stats.staged_bytes += bytes;
            if (h->stats.staged_bytes > h->stats.peak_staged_bytes)
                h->stats.peak_staged_bytes = h->stats.staged_bytes;
        }
    }
#else
    p = p2bridge_calloc(n, size);
#endif
    if (p) ++h->live;
    return p;
}
static void release(P2Commands *h, void *p) {
    if (!p) return;
    --h->live;
#ifndef P2CMD_NO_DIAGNOSTICS
    AllocationHeader *header = (AllocationHeader *)p - 1;
    h->stats.live_bytes -= header->meta.bytes;
    if (header->meta.staged) h->stats.staged_bytes -= header->meta.bytes;
    p = header;
#endif
    p2bridge_free(p);
}
static void publish_allocation(P2Commands *h, void *p) {
#ifndef P2CMD_NO_DIAGNOSTICS
    AllocationHeader *header = (AllocationHeader *)p - 1;
    h->stats.staged_bytes -= header->meta.bytes;
    header->meta.staged = 0;
#else
    (void)h; (void)p;
#endif
}
static void backing_release(P2Commands *h, Backing *b) {
    if (b && --b->refs == 0) {
        if (b->drop) b->drop(b->owner);
        else release(h, b->data);
        release(h, b);
    }
}
static void bitmap_release(P2Commands *h, Bitmap *b) {
    if (b && --b->refs == 0) { backing_release(h, b->backing); release(h, b); }
}
static Scene **scene_slot(P2Commands *h, uint16_t id) {
    Scene **p = &h->scenes;
    while (*p && (*p)->id < id) p = &(*p)->next;
    return p;
}
static Scene *scene_find(P2Commands *h, uint16_t id) {
    if (!h) return NULL;
    Scene *s = *scene_slot(h, id);
    return s && s->id == id ? s : NULL;
}
static Bitmap **bitmap_slot(P2Commands *h, uint16_t id) {
    Bitmap **p = &h->bitmaps;
    while (*p && (*p)->id < id) p = &(*p)->next;
    return p;
}
static Bitmap *bitmap_find(P2Commands *h, uint16_t id) {
    if (!h) return NULL;
    Bitmap *b = *bitmap_slot(h, id);
    return b && b->id == id ? b : NULL;
}
static Buffer **buffer_slot(P2Commands *h, uint16_t id) {
    Buffer **p = &h->buffers;
    while (*p && (*p)->id < id) p = &(*p)->next;
    return p;
}
static void buffer_release(P2Commands *h, Buffer *b) {
    if (b->drop) b->drop(b->owner);
    else release(h, b->data);
    release(h, b);
}
static Geometry **mesh_slot(Scene *s, uint16_t id) {
    Geometry **p = &s->meshes;
    while (*p && (*p)->id < id) p = &(*p)->next;
    return p;
}
static Instance **object_slot(Scene *s, uint16_t id) {
    Instance **p = &s->objects;
    while (*p && (*p)->id < id) p = &(*p)->next;
    return p;
}
static Instance *object_find(Scene *s, uint16_t id) {
    Instance *o = *object_slot(s, id);
    return o && o->id == id ? o : NULL;
}
static Pose identity_pose(void) {
    return (Pose){.scale = {1, 1, 1}};
}
static Mat4 matrix(Pose p) {
    Mat4 m = mat4Scale(p.scale), t;
    t = mat4RotateX(p.rotation.x); m = mat4MultiplyM(&m, &t);
    t = mat4RotateY(p.rotation.y); m = mat4MultiplyM(&m, &t);
    t = mat4RotateZ(p.rotation.z); m = mat4MultiplyM(&m, &t);
    t = mat4Translate(p.translation); return mat4MultiplyM(&m, &t);
}
static void object_release(P2Commands *h, Instance *o) {
    bitmap_release(h, o->bitmap);
    backing_release(h, o->backing);
    release(h, o->converted); release(h, o->uv); release(h, o);
}
static void scene_release(P2Commands *h, Scene *s) {
    if (!s) return;
    while (s->objects) {
        Instance *o = s->objects; s->objects = o->next; object_release(h, o);
    }
    while (s->meshes) {
        Geometry *g = s->meshes; s->meshes = g->next;
        release(h, g->mesh.positions); release(h, g->mesh.pos_indices);
        release(h, g->mesh.textCoord); release(h, g->mesh.tex_indices);
        release(h, g);
    }
    release(h, s->children); release(h, s->depth);
    release(h, s->scratch); release(h, s);
}
P2Commands *p2cmd_create(void) {
    P2Commands *h = p2bridge_calloc(1, sizeof(*h));
    if (h) h->fail_after = -1;
    return h;
}
void p2cmd_destroy(P2Commands *h) {
    if (!h || h->busy) return;
    while (h->scenes) {
        Scene *s = h->scenes; h->scenes = s->next; scene_release(h, s);
    }
    while (h->bitmaps) {
        Bitmap *b = h->bitmaps; h->bitmaps = b->next; bitmap_release(h, b);
    }
    while (h->buffers) {
        Buffer *b = h->buffers; h->buffers = b->next; buffer_release(h, b);
    }
    p2bridge_free(h);
}
void p2cmd_fail_after(P2Commands *h, int64_t n) { if (h && !h->busy) h->fail_after = n; }
size_t p2cmd_live_allocations(P2Commands *h) { return h ? h->live : 0; }
void p2cmd_completion(P2Commands *h, P2Completion f, void *ctx) {
    if (h && !h->busy) { h->completion = f; h->context = ctx; }
}
void p2cmd_render_hook(P2Commands *h, P2RenderHook hook, void *ctx) {
    if (h && !h->busy) { h->render_hook = hook; h->render_context = ctx; }
}
int p2cmd_stats(P2Commands *h, P2CommandStats *stats, int reset) {
    if (!h || !stats) return P2CMD_INVALID;
    if (reset && h->busy) return P2CMD_BUSY;
    memset(stats, 0, sizeof(*stats));
#ifndef P2CMD_NO_DIAGNOSTICS
    *stats = h->stats; stats->enabled = 1;
    if (reset) {
        memset(&h->stats, 0, sizeof(h->stats));
        h->stats.live_bytes = h->stats.peak_bytes = stats->live_bytes;
        h->stats.staged_bytes = h->stats.peak_staged_bytes = stats->staged_bytes;
    }
#endif
    return P2CMD_OK;
}
int p2cmd_work(P2Commands *h, uint16_t sid, P2CommandWork *work) {
    if (!h || !work) return P2CMD_INVALID;
    Scene *s = scene_find(h, sid);
    if (!s) return P2CMD_MISSING;
    *work = s->work;
    return P2CMD_OK;
}
static void backend_init(Renderer *r, Backend *b, Vec4i rect) {
    (void)r; (void)b; (void)rect;
}
static void backend_step(Renderer *r, Backend *b) { (void)r; (void)b; }
static void backend_before(Renderer *r, Backend *b) {
    (void)r;
    Scene *s = ((OwnedBackend *)b)->owner;
    P2Commands *h = s->host;
    if (h->render_hook) h->render_hook(h->render_context, s->id);
}
static Pixel *backend_pixels(Renderer *r, Backend *b) {
    (void)r; return ((OwnedBackend *)b)->owner->frame;
}
static PingoDepth *backend_depth(Renderer *r, Backend *b) {
    (void)r; return ((OwnedBackend *)b)->owner->depth;
}
static int empty_render(void *r, Mat4 m, Renderer *renderer) {
    (void)r; (void)m; (void)renderer; return OK;
}
static int scene_create(P2Commands *h, uint16_t id, uint16_t w, uint16_t height) {
    if (id == UINT16_MAX || !w || !height || w > INT16_MAX || height > INT16_MAX)
        return P2CMD_INVALID;
    Scene *s = allocate(h, 1, sizeof(*s));
    if (!s) return P2CMD_ALLOC;
    size_t n = (size_t)w * height;
    s->scratch = allocate(h, n, sizeof(Pixel));
    s->depth = allocate(h, n, sizeof(PingoDepth));
    if (!s->scratch || !s->depth) { scene_release(h, s); return P2CMD_ALLOC; }
    s->frame = s->scratch; s->id = id; s->width = w; s->height = height; s->host = h;
    s->camera = s->root_pose = identity_pose();
    s->info.near_plane = 1; s->info.far_plane = 2500; s->info.fov = 0.6f;
    s->info.clear = 1; s->info.color = 0xC0;
    s->backend.owner = s;
    s->backend.base = (Backend){.init = backend_init,
        .beforeRender = backend_before, .afterRender = backend_step,
        .getFrameBuffer = backend_pixels, .getZetaBuffer = backend_depth};
    renderer_init(&s->renderer, (Vec2i){w, height}, &s->backend.base);
    s->empty.render = empty_render;
    entity_init(&s->root, &s->empty, mat4Identity());
    renderer_set_root_renderable(&s->renderer, &s->root.renderable);
    Scene **p = scene_slot(h, id), *old = *p;
    if (old && old->id == id) { s->next = old->next; *p = s; scene_release(h, old); }
    else { s->next = old; *p = s; }
    /* The host model fences a control ID from ordinary bitmap use. Real VDP
       must additionally fence every generic buffer operation (P023). */
    Bitmap **bp = bitmap_slot(h, id);
    if (*bp && (*bp)->id == id) {
        Bitmap *b = *bp; *bp = b->next; bitmap_release(h, b);
    }
    p2cmd_release_buffer(h, id);
    return P2CMD_OK;
}
int p2cmd_bitmap(P2Commands *h, uint16_t id, uint16_t w, uint16_t height,
                 unsigned format, const uint8_t *data, size_t bytes) {
    if (!h || !data || !w || !height || w > INT16_MAX || height > INT16_MAX ||
        (format != 1 && format != 4) || (size_t)w * height > SIZE_MAX / format ||
        bytes != (size_t)w * height * format || scene_find(h, id)) return P2CMD_INVALID;
    if (h->busy) return P2CMD_BUSY;
    Bitmap *b = allocate(h, 1, sizeof(*b));
    Backing *v = allocate(h, 1, sizeof(*v));
    uint8_t *pixels = allocate(h, bytes, 1);
    if (!b || !v || !pixels) {
        release(h, pixels); release(h, v); release(h, b); return P2CMD_ALLOC;
    }
    memcpy(pixels, data, bytes);
    v->refs = 1; v->data = pixels; v->bytes = bytes;
    b->refs = 1; b->backing = v; b->id = id;
    b->width = w; b->height = height; b->format = format;
    Bitmap **p = bitmap_slot(h, id), *old = *p;
    if (old && old->id == id) { b->next = old->next; *p = b; bitmap_release(h, old); }
    else { b->next = old; *p = b; }
    p2cmd_release_buffer(h, id);
    return P2CMD_OK;
}
int p2cmd_buffer(P2Commands *h, uint16_t id, const uint8_t *data, size_t bytes, size_t blocks) {
    if (!h) return P2CMD_INVALID;
    if (h->busy) return P2CMD_BUSY;
    if ((!data && bytes) || scene_find(h, id)) return P2CMD_INVALID;
    uint64_t start = now_ns();
    Buffer *b = allocate(h, 1, sizeof(*b));
    uint8_t *copy = bytes ? allocate(h, bytes, 1) : NULL;
    int status = P2CMD_ALLOC;
    if (b && (!bytes || copy)) {
        if (bytes) memcpy(copy, data, bytes);
        b->id = id; b->data = copy; b->bytes = bytes; b->blocks = blocks;
        Buffer **slot = buffer_slot(h, id), *old = *slot;
        b->next = old;
        if (old && old->id == id) { b->next = old->next; buffer_release(h, old); }
        *slot = b;
        p2cmd_release_bitmap(h, id);
        status = P2CMD_OK;
    } else { release(h, copy); release(h, b); }
    h->stats.upload_ns = now_ns() - start;
    return status;
}
int p2cmd_release_buffer(P2Commands *h, uint16_t id) {
    if (!h) return P2CMD_INVALID;
    if (h->busy) return P2CMD_BUSY;
    Buffer **slot = buffer_slot(h, id), *b = *slot;
    if (!b || b->id != id) return P2CMD_MISSING;
    *slot = b->next; buffer_release(h, b); return P2CMD_OK;
}
const uint8_t *p2cmd_buffer_data(P2Commands *h, uint16_t id, size_t *bytes) {
    if (bytes) *bytes = 0;
    if (!h || scene_find(h, id)) return NULL;
    Buffer *b = *buffer_slot(h, id);
    if (b && b->id == id) { if (bytes) *bytes = b->bytes; return b->data; }
    return p2cmd_bitmap_data(h, id, bytes);
}
int p2cmd_release_bitmap(P2Commands *h, uint16_t id) {
    if (!h) return P2CMD_INVALID;
    if (h->busy) return P2CMD_BUSY;
    Bitmap **p = bitmap_slot(h, id), *b = *p;
    if (!b || b->id != id) return P2CMD_MISSING;
    *p = b->next; bitmap_release(h, b); return P2CMD_OK;
}
int p2cmd_borrow_bitmap(P2Commands *h, uint16_t id, uint16_t w,
    uint16_t height, unsigned format, uint8_t *data, size_t bytes,
    void *owner, void (*drop)(void *)) {
    /* Ownership transfers only on success. The C++ owner retains BOTH the
       FabGL wrapper and BufferStream. No pixel copy on the native path. */
    if (!h || !data || !owner || !drop || !w || !height ||
        w > INT16_MAX || height > INT16_MAX || (format != 1 && format != 4) ||
        (size_t)w * height > SIZE_MAX / format ||
        bytes != (size_t)w * height * format || scene_find(h, id)) return P2CMD_INVALID;
    if (h->busy) return P2CMD_BUSY;
    Bitmap *b = allocate(h, 1, sizeof(*b));
    Backing *v = allocate(h, 1, sizeof(*v));
    if (!b || !v) { release(h, b); release(h, v); return P2CMD_ALLOC; }
    v->refs = 1; v->data = data; v->bytes = bytes; v->owner = owner; v->drop = drop;
    b->refs = 1; b->backing = v; b->id = id;
    b->width = w; b->height = height; b->format = format;
    Bitmap **p = bitmap_slot(h, id), *old = *p;
    if (old && old->id == id) { b->next = old->next; *p = b; bitmap_release(h, old); }
    else { b->next = old; *p = b; }
    p2cmd_release_buffer(h, id);
    return P2CMD_OK;
}
int p2cmd_borrow_buffer(P2Commands *h, uint16_t id, uint8_t *data,
    size_t bytes, size_t blocks, void *owner, void (*drop)(void *)) {
    if (!h || !data || !bytes || !owner || !drop || scene_find(h, id)) return P2CMD_INVALID;
    if (h->busy) return P2CMD_BUSY;
    uint64_t start = now_ns();
    Buffer *b = allocate(h, 1, sizeof(*b));
    if (!b) { h->stats.upload_ns = now_ns() - start; return P2CMD_ALLOC; }
    b->id = id; b->data = data; b->bytes = bytes; b->blocks = blocks;
    b->owner = owner; b->drop = drop;
    Buffer **slot = buffer_slot(h, id), *old = *slot;
    b->next = old;
    if (old && old->id == id) { b->next = old->next; buffer_release(h, old); }
    *slot = b;
    p2cmd_release_bitmap(h, id);
    h->stats.upload_ns = now_ns() - start;
    return P2CMD_OK;
}
const uint8_t *p2cmd_bitmap_data(P2Commands *h, uint16_t id, size_t *bytes) {
    Bitmap *b = bitmap_find(h, id);
    if (bytes) *bytes = b ? b->backing->bytes : 0;
    return b ? b->backing->data : NULL;
}
const uint32_t *p2cmd_depth(P2Commands *h, uint16_t id, size_t *count) {
    Scene *s = scene_find(h, id);
    if (count) *count = s ? (size_t)s->width * s->height : 0;
    return s ? (const uint32_t *)s->depth : NULL;
}
static Instance *new_object(P2Commands *h, uint16_t id) {
    Instance *o = allocate(h, 1, sizeof(*o));
    if (o) { o->id = id; o->active = 1; o->pose = identity_pose(); }
    return o;
}
static Instance *ensure_object(P2Commands *h, Scene *s, uint16_t id) {
    Instance **p = object_slot(s, id);
    if (*p && (*p)->id == id) return *p;
    Instance *o = new_object(h, id);
    if (o) { o->next = *p; *p = o; ++s->info.objects; }
    return o;
}
static int indices_valid(const uint16_t *idx, uint32_t n, uint32_t extent) {
    if (!idx || !n || n % 3 || !extent) return 0;
    for (uint32_t i = 0; i < n; ++i) if (idx[i] >= extent) return 0;
    return 1;
}
static void refresh(Instance *o) {
    memset(&o->view, 0, sizeof(o->view)); o->valid = 0;
    Geometry *g = o->geometry;
    if (!g) return;
    o->view = g->mesh;
    if (o->uv_count) o->view.textCoord = o->uv;
    uint32_t uv_count = o->uv_count ? o->uv_count : g->uv_count;
    o->valid = o->bitmap && o->view.positions && o->view.textCoord &&
        g->uv_index_count == (uint32_t)o->view.indexes_count &&
        indices_valid(o->view.pos_indices, (uint32_t)o->view.indexes_count,
                      o->view.positions_count) &&
        indices_valid(o->view.tex_indices, g->uv_index_count, uv_count);
    if (o->valid) {
        texture_init(&o->texture, (Vec2i){o->bitmap->width, o->bitmap->height},
                     o->converted ? o->converted : (Pixel *)o->backing->data);
        material_init(&o->material, &o->texture);
        object_init(&o->object, &o->view, &o->material);
    }
}
static void refresh_mesh(Scene *s, Geometry *g) {
    for (Instance *o = s->objects; o; o = o->next) if (o->geometry == g) refresh(o);
}
static uint16_t word(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t triple(const uint8_t *p) { return (uint32_t)word(p) | ((uint32_t)p[2] << 16); }
static int32_t signed_triple(const uint8_t *p) {
    uint32_t n = triple(p);
    return n & 0x800000u ? (int32_t)n - 16777216 : (int32_t)n;
}
static int32_t signed_word(const uint8_t *p) {
    uint32_t n = word(p); return n & 0x8000u ? (int32_t)n - 65536 : (int32_t)n;
}
static float rotated(const uint8_t *p) { return signed_word(p) * ((2.0f * 3.1415926f) / 32767.0f); }
static float wide(const uint8_t *p) { return signed_triple(p) * (256.0f / 32767.0f); }
static int checked_bytes(size_t *total, size_t count, size_t width) {
    if (count > SIZE_MAX / width || *total > SIZE_MAX - count * width) return 0;
    *total += count * width; return 1;
}
static int uv_view_valid(const Mesh *m, const Vec2f *uv, uint32_t count) {
    if (!indices_valid(m->tex_indices, (uint32_t)m->indexes_count, count)) return 0;
    for (uint32_t i = 0; i < count; ++i)
        if (!isfinite(uv[i].x) || !isfinite(uv[i].y)) return 0;
    if (m->shading_mode == 1) for (int i = 0; i < m->indexes_count; i += 3) {
        Vec2f a = uv[m->tex_indices[i]];
        for (int j = 1; j < 3; ++j) {
            Vec2f b = uv[m->tex_indices[i+j]];
            if (a.x != b.x || a.y != b.y) return 0;
        }
    }
    return 1;
}
static void arrays_release(P2Commands *h, Mesh *m) {
    release(h, m->positions); release(h, m->pos_indices);
    release(h, m->textCoord); release(h, m->tex_indices);
}
static int replace_mesh(P2Commands *h, Scene *s, const uint8_t *p) {
    uint64_t validation = now_ns();
    h->stats.validation_ns = h->stats.stage_ns = h->stats.commit_ns = 0;
    uint16_t source_id = word(p), mid = word(p+2);
    uint32_t vertices = triple(p+4), indices = triple(p+7), uvs = triple(p+10), uv_indices = triple(p+13);
    int status = P2CMD_INVALID;
    if (!source_id || source_id == UINT16_MAX || scene_find(h, source_id) ||
        vertices < 3 || vertices > 65536 || indices < 3 || indices % 3 ||
        !uvs || uvs > 65536 || indices != uv_indices)
        goto reject;
    Geometry *g = *mesh_slot(s, mid);
    if (!g || g->id != mid) { status = P2CMD_MISSING; goto reject; }
    size_t expected = 0, bytes = 0;
    if (!checked_bytes(&expected, vertices, 9) || !checked_bytes(&expected, indices, 2) ||
        !checked_bytes(&expected, uvs, 4) || !checked_bytes(&expected, uv_indices, 2)) goto reject;
    Buffer *buffer = *buffer_slot(h, source_id);
    if (buffer && buffer->id == source_id && buffer->blocks != 1) goto reject;
    const uint8_t *data = p2cmd_buffer_data(h, source_id, &bytes);
    if (!data) { status = P2CMD_MISSING; goto reject; }
    if (bytes != expected) goto reject;
    h->stats.validation_ns = now_ns() - validation;

    /* No callbacks/dispatch while staging: single-threaded borrowed source
       stays immutable. Four private arrays sever its lifetime before return. */
    uint64_t stage = now_ns();
    Mesh candidate = g->mesh;
    h->staging = 1;
    candidate.positions = allocate(h, vertices, sizeof(Vec3f));
    candidate.pos_indices = allocate(h, indices, sizeof(uint16_t));
    candidate.textCoord = allocate(h, uvs, sizeof(Vec2f));
    candidate.tex_indices = allocate(h, uv_indices, sizeof(uint16_t));
    h->staging = 0;
    if (!candidate.positions || !candidate.pos_indices || !candidate.textCoord || !candidate.tex_indices) {
        arrays_release(h, &candidate);
        h->stats.stage_ns = now_ns() - stage;
        return P2CMD_ALLOC;
    }
    candidate.positions_count = vertices; candidate.indexes_count = (int)indices;
    for (uint32_t i = 0; i < vertices; ++i, data += 9)
        candidate.positions[i] = (Vec3f){signed_triple(data)*(1.0f/32767.0f),
            signed_triple(data+3)*(1.0f/32767.0f), signed_triple(data+6)*(1.0f/32767.0f)};
    for (uint32_t i = 0; i < indices; ++i, data += 2) candidate.pos_indices[i] = word(data);
    for (uint32_t i = 0; i < uvs; ++i, data += 4)
        candidate.textCoord[i] = (Vec2f){word(data)*(1.0f/65535.0f),word(data+2)*(1.0f/65535.0f)};
    for (uint32_t i = 0; i < uv_indices; ++i, data += 2) candidate.tex_indices[i] = word(data);
    h->stats.stage_ns = now_ns() - stage;
    validation = now_ns();
    int valid = mesh_prepare_bounds(&candidate, vertices) &&
        uv_view_valid(&candidate, candidate.textCoord, uvs);
    for (Instance *o = s->objects; valid && o; o = o->next)
        if (o->geometry == g && o->uv_count)
            valid = uv_view_valid(&candidate, o->uv, o->uv_count);
    h->stats.validation_ns += now_ns() - validation;
    if (!valid) { arrays_release(h, &candidate); return P2CMD_INVALID; }

    /* Allocation-free publication: stable Geometry address, refreshed views
       and O02 bounds before release of anything visible in the old mesh. */
    uint64_t commit = now_ns();
    Mesh previous = g->mesh;
    g->mesh = candidate; g->uv_count = uvs; g->uv_index_count = uv_indices;
    refresh_mesh(s, g);
    publish_allocation(h, candidate.positions); publish_allocation(h, candidate.pos_indices);
    publish_allocation(h, candidate.textCoord); publish_allocation(h, candidate.tex_indices);
    arrays_release(h, &previous);
    h->stats.commit_ns = now_ns() - commit;
    return P2CMD_OK;
reject:
    h->stats.validation_ns = now_ns() - validation;
    return status;
}
static int upload(P2Commands *h, Scene *s, unsigned command, const uint8_t *p) {
    uint16_t id = word(p);
    uint32_t n = triple(p + 2);
    if ((command == 2 || command == 4) && n % 3) return P2CMD_INVALID;
    if ((command == 1 || command == 3 || command == 40) && n > 65536) return P2CMD_INVALID;
    size_t size = command == 1 ? sizeof(Vec3f) :
        (command == 3 || command == 40) ? sizeof(Vec2f) : sizeof(uint16_t);
    void *replacement = n ? allocate(h, n, size) : NULL;
    if (n && !replacement) return P2CMD_ALLOC;
    const uint8_t *data = p + 5;
    for (uint32_t i = 0; i < n; ++i) {
        if (command == 1) {
            ((Vec3f *)replacement)[i] = (Vec3f){
                signed_triple(data) * (1.0f / 32767.0f),
                signed_triple(data + 3) * (1.0f / 32767.0f),
                signed_triple(data + 6) * (1.0f / 32767.0f)};
            data += 9;
        } else if (command == 3 || command == 40) {
            ((Vec2f *)replacement)[i] = (Vec2f){
                word(data) * (1.0f / 65535.0f), word(data + 2) * (1.0f / 65535.0f)};
            data += 4;
        } else { ((uint16_t *)replacement)[i] = word(data); data += 2; }
    }
    if (command == 40) {
        Instance *o = ensure_object(h, s, id);
        if (!o) { release(h, replacement); return P2CMD_ALLOC; }
        Vec2f *old = o->uv; o->uv = replacement; o->uv_count = n;
        refresh(o); release(h, old); return P2CMD_OK;
    }
    Geometry **slot = mesh_slot(s, id), *g = *slot;
    if (!g || g->id != id) {
        g = allocate(h, 1, sizeof(*g));
        if (!g) { release(h, replacement); return P2CMD_ALLOC; }
        g->id = id; g->next = *slot; *slot = g;
    }
    void *old = NULL;
    if (command == 1 || command == 2) mesh_invalidate_bounds(&g->mesh);
    if (command == 1) { old = g->mesh.positions; g->mesh.positions = replacement; g->mesh.positions_count = n; }
    if (command == 2) { old = g->mesh.pos_indices; g->mesh.pos_indices = replacement; g->mesh.indexes_count = n; }
    if (command == 3) { old = g->mesh.textCoord; g->mesh.textCoord = replacement; g->uv_count = n; }
    if (command == 4) { old = g->mesh.tex_indices; g->mesh.tex_indices = replacement; g->uv_index_count = n; }
    if (command == 1 || command == 2) mesh_prepare_bounds(&g->mesh, g->mesh.positions_count);
    refresh_mesh(s, g); release(h, old); return P2CMD_OK;
}
static void convert_texture(const Bitmap *b, Pixel *converted) {
    for (size_t i = 0; i < (size_t)b->width * b->height; ++i) {
        const uint8_t *rgba = b->backing->data + i * 4;
        converted[i] = pixelFromRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
    }
}
static int bind_object(P2Commands *h, Scene *s, const uint8_t *p) {
    uint16_t oid = word(p), mid = word(p + 2), bid = word(p + 4);
    Bitmap *b = bitmap_find(h, bid);
    if (!b || scene_find(h, bid)) return P2CMD_MISSING;
    Instance **op = object_slot(s, oid), *o = *op;
    Geometry **gp = mesh_slot(s, mid), *g = *gp;
    int new_o = !o || o->id != oid, new_g = !g || g->id != mid;
    if (new_o) o = new_object(h, oid);
    if (new_g) g = allocate(h, 1, sizeof(*g));
    Pixel *converted = NULL;
    if (b->format == 4) converted = allocate(h, (size_t)b->width * b->height, sizeof(Pixel));
    if (!o || !g || (b->format == 4 && !converted)) {
        if (new_o) release(h, o);
        if (new_g) release(h, g);
        release(h, converted); return P2CMD_ALLOC;
    }
    if (converted) convert_texture(b, converted);
    ++b->refs; ++b->backing->refs;
    bitmap_release(h, o->bitmap); backing_release(h, o->backing); release(h, o->converted);
    o->bitmap = b; o->backing = b->backing; o->converted = converted; o->geometry = g;
    if (new_o) { o->next = *op; *op = o; ++s->info.objects; }
    if (new_g) { g->id = mid; g->next = *gp; *gp = g; }
    refresh(o); return P2CMD_OK;
}
int p2cmd_remove_object(P2Commands *h, uint16_t sid, uint16_t oid) {
    Scene *s = scene_find(h, sid);
    if (!s) return P2CMD_MISSING;
    if (h->busy) return P2CMD_BUSY;
    Instance **p = object_slot(s, oid), *o = *p;
    if (!o || o->id != oid) return P2CMD_MISSING;
    *p = o->next; --s->info.objects; object_release(h, o); return P2CMD_OK;
}
int p2cmd_views(P2Commands *h, uint16_t sid, uint16_t oid, uintptr_t output[3]) {
    Scene *s = scene_find(h, sid); Instance *o = s ? object_find(s, oid) : NULL;
    if (!o || !output) return P2CMD_MISSING;
    output[0] = (uintptr_t)o->view.positions; output[1] = (uintptr_t)o->view.textCoord;
    output[2] = (uintptr_t)o->texture.frameBuffer; return P2CMD_OK;
}
int p2cmd_info(P2Commands *h, uint16_t sid, P2CommandInfo *info) {
    Scene *s = scene_find(h, sid);
    if (!s || !info) return P2CMD_MISSING;
    *info = s->info; info->drawable = 0;
    for (Instance *o = s->objects; o; o = o->next) if (o->active && o->valid) ++info->drawable;
    return P2CMD_OK;
}
int p2cmd_set_sequence(P2Commands *h, uint16_t sid, uint32_t sequence) {
    Scene *s = scene_find(h, sid);
    if (!s) return P2CMD_MISSING;
    if (h->busy) return P2CMD_BUSY;
    s->info.sequence = sequence; return P2CMD_OK;
}
int p2cmd_matrices(P2Commands *h, uint16_t sid, uint16_t oid, float output[64]) {
    Scene *s = scene_find(h, sid);
    if (!s || !output) return P2CMD_MISSING;
    Instance *o = object_find(s, oid);
    Mat4 matrices[4] = {matrix(s->camera), matrix(s->root_pose),
        o ? matrix(o->pose) : mat4Identity(), mat4Perspective(s->info.near_plane,
        s->info.far_plane, (float)s->width / s->height, s->info.fov)};
    for (unsigned i = 0; i < 4; ++i) memcpy(output + 16 * i, matrices[i].elements, sizeof(float) * 16);
    return P2CMD_OK;
}
static uint64_t now_ns(void) {
#ifdef P2CMD_NO_DIAGNOSTICS
    return 0;
#else
    return p2bridge_now_ns();
#endif
}
static uint64_t hash_bytes(const uint8_t *data, size_t n) {
    uint64_t v = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < n; ++i) { v ^= data[i]; v *= UINT64_C(1099511628211); }
    return v;
}
static int render(P2Commands *h, Scene *s, uint16_t bid) {
    uint64_t preparation = now_ns();
    Bitmap *b = bitmap_find(h, bid);
    if (!b || scene_find(h, bid) || b->width != s->width || b->height != s->height)
        return P2CMD_INVALID;
    /* Native no-clear is lossless. Four-byte compatibility no-clear is
       lossless only on the native palette (including alpha). Reject before
       drawing rather than quantizing supposedly preserved background bytes. */
    if (b->format == 4 && !s->info.clear)
        for (size_t i = 0; i < b->backing->bytes; ++i)
            if (b->backing->data[i] % 85u) return P2CMD_INVALID;
    for (Instance *o = s->objects; o; o = o->next)
        if (o->active && o->valid && o->backing->data == b->backing->data) return P2CMD_INVALID;
    if (s->capacity < s->info.objects) {
        Entity *next = allocate(h, s->info.objects, sizeof(Entity));
        if (!next) return P2CMD_ALLOC;
        release(h, s->children); s->children = next; s->capacity = s->info.objects;
    }
    size_t count = 0, n = (size_t)s->width * s->height;
    memset(&s->work, 0, sizeof(s->work));
    for (Instance *o = s->objects; o; o = o->next) if (o->active && o->valid) {
#ifndef P2CMD_NO_DIAGNOSTICS
        s->work.submitted_vertices += o->view.positions_count;
        s->work.submitted_triangles += o->view.indexes_count / 3;
#endif
        /* Another scene may have rendered into this retained RGBA texture.
           Refresh the explicit boundary conversion, never a fragment format
           dispatch or an obsolete bind-time snapshot. */
        if (o->converted) convert_texture(o->bitmap, o->converted);
        entity_init(&s->children[count++], &o->object.renderable, matrix(o->pose));
    }
    s->root.children_entities.data = s->children;
    s->root.children_entities.size = count;
    s->root.transform = matrix(s->root_pose);
    s->renderer.camera_view = matrix(s->camera);
    s->renderer.camera_projection = mat4Perspective(s->info.near_plane,
        s->info.far_plane, (float)s->width / s->height, s->info.fov);
    s->renderer.clear = s->info.clear; s->renderer.clear_color = (Pixel){s->info.color};
    /* No-clear compatibility targets must seed from THIS output, not a stale
       private framebuffer belonging to a different previously rendered ID. */
    if (b->format == 4 && !s->info.clear) for (size_t i = 0; i < n; ++i) {
        uint8_t *p = b->backing->data + 4 * i;
        s->scratch[i] = pixelFromRGBA(p[0], p[1], p[2], p[3]);
    }
    s->frame = b->format == 1 ? (Pixel *)b->backing->data : s->scratch;
    ++b->refs; ++b->backing->refs; h->busy = 1;
    s->info.phase_count = 0;
    s->work.prepare_ns = now_ns() - preparation;
    uint64_t start = now_ns();
    int status = renderer_render(&s->renderer);
    s->info.render_ns = now_ns() - start;
    s->info.phases[s->info.phase_count++] = 1;
    start = now_ns();
    if (status == OK && b->format == 4) for (size_t i = 0; i < n; ++i) {
        uint8_t value = s->frame[i].c;
        for (unsigned c = 0; c < 4; ++c) b->backing->data[4*i+c] = ((value >> (2*c)) & 3u) * 85u;
    }
    s->frame = s->scratch;
    s->info.output_ns = now_ns() - start;
    if (status == OK) {
        s->info.phases[s->info.phase_count++] = 2;
#ifndef P2CMD_NO_DIAGNOSTICS
        s->info.color_hash = hash_bytes(b->backing->data, b->backing->bytes);
        /* Canonical depth bytes are little-endian, independent of host order. */
        uint64_t hash = UINT64_C(14695981039346656037);
        for (size_t i = 0; i < n; ++i) for (unsigned j = 0; j < 4; ++j) {
            hash ^= (s->depth[i].d >> (j*8)) & 255u; hash *= UINT64_C(1099511628211);
        }
        s->info.depth_hash = hash;
#endif
    }
    backing_release(h, b->backing); bitmap_release(h, b); h->busy = 0;
    if (status != OK) return P2CMD_INVALID;
    uint32_t seq = s->info.sequence++;
    uint16_t sid = s->id;
    start = now_ns();
    uint8_t packet[10] = {'P','3','D','R',1,1,(uint8_t)s->token,
        (uint8_t)(s->token >> 8),(uint8_t)seq,(uint8_t)(seq >> 8)};
    P2Completion callback = h->completion; void *ctx = h->context;
    h->stats.notification_ns = now_ns() - start;
    if (s->notify) {
        s->info.phases[s->info.phase_count++] = 3;
        /* Callback may destroy/reinitialize even the entire host. No access
           to h, s, b or their storage is allowed after this call. */
        if (callback) callback(ctx, sid, packet);
    }
    return P2CMD_OK;
}
int p2cmd_payload_length(unsigned c, const uint8_t *p, size_t n, size_t *want) {
    static const uint8_t fixed[56] = {
        4,0,0,0,0,6,5,5,5,11,4,4,4,8,5,5,5,11,
        2,2,2,6,3,3,3,9,3,3,3,9,2,2,2,6,3,3,3,9,2,0,0,3,
        0,6,1,1,1,3,3,8,16,3,11,3,5,2};
    if (c > 55 || c == 42) { *want = 0; return P2CMD_UNSUPPORTED; }
    if ((c >= 1 && c <= 4) || c == 40) {
        if (n < 5) return P2CMD_TRUNCATED;
        *want = 5;
        if (!checked_bytes(want, triple(p + 2), c == 1 ? 9 : (c == 3 || c == 40) ? 4 : 2))
            return P2CMD_INVALID;
    } else *want = fixed[c];
    return n < *want ? P2CMD_TRUNCATED : P2CMD_OK;
}
static void set_component(Vec3f *v, unsigned axis, float value) {
    if (axis == 0) v->x = value;
    if (axis == 1) v->y = value;
    if (axis == 2) v->z = value;
}
int p2cmd_execute(P2Commands *h, const uint8_t *wire, size_t length, size_t *consumed) {
    if (consumed) *consumed = 0;
    if (!h || !wire || !consumed) return P2CMD_INVALID;
    if (h->busy) return P2CMD_BUSY;
    if (length < 7) { *consumed = length; return P2CMD_TRUNCATED; }
    *consumed = 7;
    if (wire[0] != 23 || wire[1] != 0 || wire[2] != 0xA0 || wire[5] != 0x49)
        return P2CMD_INVALID;
    unsigned c = wire[6]; const uint8_t *p = wire + 7;
    size_t want = 0;
    int status = p2cmd_payload_length(c, p, length - 7, &want);
    if (status == P2CMD_TRUNCATED) { *consumed = length; return status; }
    *consumed += want;
    if (status != P2CMD_OK) return status;
    if (c == 49) return P2CMD_UNSUPPORTED;
    uint16_t sid = word(wire + 3);
    if (c == 0) return scene_create(h, sid, word(p), word(p + 2));
    Scene *s = scene_find(h, sid);
    if (!s) return P2CMD_MISSING;
    if (c == 39) {
        *scene_slot(h, sid) = s->next; scene_release(h, s); return P2CMD_OK;
    }
    if ((c >= 1 && c <= 4) || c == 40) return upload(h, s, c, p);
    if (c == 5) return bind_object(h, s, p);
    if (c == 50) return replace_mesh(h, s, p);
    if (c >= 6 && c <= 37) {
        Pose *pose; unsigned relative;
        if (c <= 17) {
            Instance *o = ensure_object(h, s, word(p));
            if (!o) return P2CMD_ALLOC;
            pose = &o->pose; p += 2; relative = c - 6;
        } else if (c <= 25) { pose = &s->camera; relative = c - 18 + 4; }
        else { pose = &s->root_pose; relative = c - 26; }
        unsigned group = relative / 4, axis = relative % 4;
        Vec3f *v = group == 0 ? &pose->scale : group == 1 ? &pose->rotation : &pose->translation;
        for (unsigned i = 0; i < (axis == 3 ? 3u : 1u); ++i) {
            float value = group == 0 ? triple(p + 3*i) * (1.0f/256.0f) :
                group == 1 ? rotated(p + 2*i) : wide(p + 3*i);
            set_component(v, axis == 3 ? i : axis, value);
        }
        return P2CMD_OK;
    }
    if (c == 38) return render(h, s, word(p));
    if (c == 41) { s->notify = p[0] == 1; s->token = word(p + 1); return P2CMD_OK; }
    if (c == 43) {
        return renderer_set_light_direction(&s->renderer, (Vec3f){
            signed_word(p), signed_word(p+2), signed_word(p+4)}) ?
            P2CMD_INVALID : P2CMD_OK;
    }
    if (c == 44) { s->renderer.light_intensity = p[0] / 127.0f; return P2CMD_OK; }
    if (c == 45) { s->renderer.ambient_light = p[0] / 127.0f; return P2CMD_OK; }
    if (c == 46) {
        if (p[0] > 1) return P2CMD_INVALID;
        s->renderer.illumination_enabled = p[0] != 0; return P2CMD_OK;
    }
    if (c == 47 || c == 48) {
        if (p[2] > 1) return P2CMD_INVALID;
        uint16_t mid = word(p);
        Geometry **gp = mesh_slot(s, mid), *g = *gp;
        if (!g || g->id != mid) {
            g = allocate(h, 1, sizeof(*g));
            if (!g) return P2CMD_ALLOC;
            g->id = mid; g->next = *gp; *gp = g;
        }
        if (c == 47) g->mesh.shading_mode = p[2];
        else g->mesh.illumination_policy = p[2];
        refresh_mesh(s, g); return P2CMD_OK;
    }
    if (c == 51 || c == 52) {
        if (c == 51 && p[2] > 1) return P2CMD_INVALID;
        Instance *o = object_find(s, word(p));
        if (!o) return P2CMD_MISSING;
        if (c == 51) o->active = p[2];
        else o->pose.translation = (Vec3f){wide(p+2),wide(p+5),wide(p+8)};
        return P2CMD_OK;
    }
    if (c == 53) {
        uint32_t far = triple(p);
        if (far < 2 || far <= s->info.near_plane) return P2CMD_INVALID;
        s->info.far_plane = far; return P2CMD_OK;
    }
    if (c == 54) {
        uint32_t near = triple(p);
        uint16_t fov = word(p+3);
        if (!near || near * (1.0f/256.0f) >= s->info.far_plane || !fov || fov > 51471)
            return P2CMD_INVALID;
        s->info.near_plane = near * (1.0f/256.0f);
        s->info.fov = fov * (1.0f/16384.0f); return P2CMD_OK;
    }
    if (c == 55) {
        if (p[0] > 1) return P2CMD_INVALID;
        s->info.clear = p[0]; s->info.color = p[1]; return P2CMD_OK;
    }
    return P2CMD_UNSUPPORTED;
}
