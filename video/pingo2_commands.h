#ifndef PINGO2_COMMANDS_H
#define PINGO2_COMMANDS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Target resource owner derived from accepted fsim P026/P030. Engine types stay
 * behind this C ABI. The C++ adapter owns typed IDs, PSRAM, VDU and UART.
 * All operations are synchronous on the VDU processor thread. */
void *p2bridge_calloc(size_t count, size_t size);
void p2bridge_free(void *pointer);
uint64_t p2bridge_now_ns(void);
typedef struct P2Commands P2Commands;
/* Native tooling pin, not a wire opcode or width autodetection. */
#define P2CMD_WIRE_REVISION 30u
uint32_t p2cmd_wire_revision(void);
enum { P2CMD_OK, P2CMD_INVALID, P2CMD_TRUNCATED, P2CMD_UNSUPPORTED,
       P2CMD_MISSING, P2CMD_ALLOC, P2CMD_BUSY };
enum { P2CMD_RGBA2222 = 1, P2CMD_RGBA8888 = 4 };
typedef void (*P2Completion)(void *context, uint16_t scene,
                             const uint8_t packet[10]);
typedef void (*P2RenderHook)(void *context, uint16_t scene);
typedef struct P2CommandInfo {
    uint32_t objects, drawable, sequence;
    float near_plane, far_plane, fov;
    uint8_t clear, color, phase_count, phases[3];
    uint64_t render_ns, output_ns, color_hash, depth_hash;
} P2CommandInfo;
typedef struct P2CommandStats {
    uint32_t enabled;
    size_t live_bytes, peak_bytes, staged_bytes, peak_staged_bytes, largest_allocation;
    uint64_t upload_ns, validation_ns, stage_ns, commit_ns, notification_ns;
} P2CommandStats;
/* Last successful frame: preparation excludes engine render and publication.
 * Counts are submitted instance geometry, not clipped/rasterized fragments. */
typedef struct P2CommandWork {
    uint64_t prepare_ns, submitted_vertices, submitted_triangles;
} P2CommandWork;
int p2cmd_work(P2Commands *host, uint16_t scene, P2CommandWork *work);

P2Commands *p2cmd_create(void);
void p2cmd_destroy(P2Commands *host);
/* One complete envelope; consumed enables replay of concatenated commands.
 * Unknown commands consume ONLY the seven-byte envelope; caller must stop.
 * Truncation consumes available input without changing live scene state. */
int p2cmd_execute(P2Commands *host, const uint8_t *wire, size_t length,
                  size_t *consumed);
/* Bitmap registration copies bytes into a separately owned backing. Format
 * 4 is byte-ordered RGBA, never BGRA. Rebinding/releasing an ID leaves bound
 * textures alive. This models lifetime, not arbitrary VDP buffer mutation. */
int p2cmd_bitmap(P2Commands *host, uint16_t id, uint16_t width,
                 uint16_t height, unsigned format, const uint8_t *data,
                 size_t bytes);
int p2cmd_release_bitmap(P2Commands *host, uint16_t id);
/* Transfer a retained wrapper/backing owner only on success. */
int p2cmd_borrow_bitmap(P2Commands *host, uint16_t id, uint16_t width,
    uint16_t height, unsigned format, uint8_t *data, size_t bytes,
    void *owner, void (*drop)(void *));
int p2cmd_payload_length(unsigned command, const uint8_t *payload,
                         size_t available, size_t *wanted);
const uint8_t *p2cmd_bitmap_data(P2Commands *host, uint16_t id, size_t *bytes);
const uint32_t *p2cmd_depth(P2Commands *host, uint16_t scene, size_t *count);
int p2cmd_info(P2Commands *host, uint16_t scene, P2CommandInfo *info);
/* Row-major camera pose, root, object, projection matrices, in that order. */
int p2cmd_matrices(P2Commands *host, uint16_t scene, uint16_t object,
                   float output[64]);
void p2cmd_completion(P2Commands *host, P2Completion callback, void *context);
/* Copied source sideband for C-owner qualification. Production borrows the
 * existing stock registry backing for one synchronous command, then releases
 * this temporary registration. This is not a second VDP buffer registry. */
int p2cmd_buffer(P2Commands *host, uint16_t id, const uint8_t *data,
                 size_t bytes, size_t blocks);
int p2cmd_borrow_buffer(P2Commands *host, uint16_t id, uint8_t *data,
    size_t bytes, size_t blocks, void *owner, void (*drop)(void *));
int p2cmd_release_buffer(P2Commands *host, uint16_t id);
const uint8_t *p2cmd_buffer_data(P2Commands *host, uint16_t id, size_t *bytes);
/* Owner requested bytes exclude root host, platform wrappers/headers and
 * borrowed source/bitmap data. Platform totals are measured independently. */
int p2cmd_stats(P2Commands *host, P2CommandStats *stats, int reset);

/* Host test seams only: NOT additional wire operations. */
int p2cmd_remove_object(P2Commands *host, uint16_t scene, uint16_t object);
int p2cmd_views(P2Commands *host, uint16_t scene, uint16_t object,
                uintptr_t output[3]);
int p2cmd_set_sequence(P2Commands *host, uint16_t scene, uint32_t sequence);
void p2cmd_fail_after(P2Commands *host, int64_t successful_allocations);
size_t p2cmd_live_allocations(P2Commands *host);
void p2cmd_render_hook(P2Commands *host, P2RenderHook hook, void *context);
#ifdef __cplusplus
}
#endif
#endif
