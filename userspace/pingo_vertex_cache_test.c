/* P046: exact uncached oracle, adversarial reuse and operation counts. */
#define main retained_diagnostics_tests
#include "pingo_renderer_diagnostics_test.c"
#undef main
#include <stdlib.h>

_Static_assert(sizeof(Vec4f) == 16, "clip value layout");
_Static_assert(sizeof(PingoVertexCache) == 5124, "payload plus lease");
static PingoVertexCache scratch;
static unsigned acquisitions, transform_calls;
static int unavailable;
static Renderer * nested;
static unsigned nested_draws;
Vec4f __real_mat4MultiplyVec4(Vec4f *, Mat4 *);
Vec4f __wrap_mat4MultiplyVec4(Vec4f *v, Mat4 *m) {
    transform_calls++;
    if (nested && scratch.busy) {
        Renderer * draw = nested;
        nested = NULL;
        PingoVertexCache before = scratch;
        assert(rendererRender(draw) == 0);
        assert(memcmp(&before, &scratch, sizeof(before)) == 0);
        assert(draw->diagnostics.vertex_cache_draws == 0);
        nested_draws++;
    }
    return __real_mat4MultiplyVec4(v, m);
}
static PingoVertexCache * acquire(Renderer *r, BackEnd *b) {
    (void)r; (void)b;
    acquisitions++;
    return unavailable ? NULL : &scratch;
}
static uint32_t seed = 0x460047;
static uint32_t random_bits(void) {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
static float coordinate(void) {
    return ((float)(random_bits() >> 8) / 16777215.0f - .5f) * 6;
}
static void palette(Object *o, Mesh *mesh, Texture *texture, Material *material,
        Vec2f *uv, uint16_t *uv_indices, Pixel *pixel) {
    assert(texture_init(texture, (Vec2i){1,1}, pixel) == 0);
    memset(material, 0, sizeof(*material));
    material->texture = texture;
    o->material = material;
    mesh->textCoord = uv;
    mesh->texture_coordinates_count = 1;
    mesh->tex_indices = uv_indices;
    mesh->texture_indexes_count = mesh->indexes_count;
    mesh->shading_mode = MESH_SHADING_FLAT_PALETTE;
    objectUpdateTextureMappingValidity(o);
    assert(o->texture_mapping_valid);
}
static uint32_t trace_misses(const uint16_t *indices, unsigned count) {
    uint32_t tags[256] = {0}, misses = 0;
    for (unsigned i=0; i<count; i++) {
        unsigned slot = indices[i] & 255;
        uint32_t tag = (uint32_t)indices[i] + 1;
        if (tags[slot] != tag) { misses++; tags[slot] = tag; }
    }
    return misses;
}
static void paired(Renderer *r, TestBuffers *buffers, int eligible,
        const uint16_t *indices, unsigned count) {
    r->acquireVertexCache = NULL;
    assert(rendererRender(r) == 0);
    TestBuffers expected = *buffers;
    r->acquireVertexCache = acquire;
    acquisitions = transform_calls = 0;
    assert(rendererRender(r) == 0);
    assert(memcmp(&expected, buffers, sizeof(expected)) == 0);
    assert(scratch.busy == 0);
    assert_diagnostic_invariants(&r->diagnostics);
    if (eligible) {
        uint32_t misses = trace_misses(indices, count);
        assert(acquisitions == 1);
        assert(r->diagnostics.vertex_cache_draws == 1);
        assert(r->diagnostics.vertex_cache_misses == misses);
        assert(r->diagnostics.vertex_cache_hits == count - misses);
        assert(transform_calls == misses);
    } else {
        assert(r->diagnostics.vertex_cache_draws == 0);
        assert(r->diagnostics.vertex_cache_hits == 0);
        assert(r->diagnostics.vertex_cache_misses == 0);
    }
}
static void terrain(const char *path) {
    FILE *input=fopen(path,"rb"); assert(input);
    uint32_t total_misses=0,total_hits=0;
    for (unsigned part=0;part<4;part++) {
        uint32_t count,refs;
        assert(fread(&count,4,1,input)==1 && fread(&refs,4,1,input)==1);
        Vec3f *positions=malloc(count*sizeof(*positions));
        uint16_t *indices=malloc(refs*sizeof(*indices));
        uint16_t *uv_indices=calloc(refs,sizeof(*uv_indices));
        assert(positions && indices && uv_indices);
        assert(fread(positions,sizeof(*positions),count,input)==count);
        assert(fread(indices,sizeof(*indices),refs,input)==refs);
        Renderer r;Scene scene;BackEnd backend;TestBuffers buffers;
        Mesh mesh;Object object;Texture texture;Material material;
        Vec2f uv={0,0};Pixel pixel=PIXELWHITE;
        initialize_renderer(&r,&scene,&backend,&buffers);
        add_mesh_object(&scene,&object,&mesh,positions,indices,refs);
        palette(&object,&mesh,&texture,&material,&uv,uv_indices,&pixel);
        r.illuminationEnabled=0;r.frustumCulling=0;
        paired(&r,&buffers,1,indices,refs);
        total_misses+=r.diagnostics.vertex_cache_misses;
        total_hits+=r.diagnostics.vertex_cache_hits;
        free(positions);free(indices);free(uv_indices);
    }
    assert(fgetc(input)==EOF);fclose(input);
    assert(total_misses==66489 && total_hits==102021);
    printf("Actual terrain: %u misses, %u hits; exact pixels/depth/call counts PASS\n",
        total_misses,total_hits);
}
int main(int argc, char **argv) {
    assert(retained_diagnostics_tests() == 0);
    Vec3f *positions = calloc(65536, sizeof(*positions));
    assert(positions);
    const uint16_t pool[] = {0,1,2,255,256,257,511,512,1024,65279,65535};
    uint16_t indices[96], uv_indices[96] = {0};
    Vec2f uv = {0,0}; Pixel pixel = PIXELWHITE;
    for (unsigned n=0; n<12000; n++) {
        for (unsigned v=0; v<sizeof(pool)/sizeof(pool[0]); v++)
            positions[pool[v]] = (Vec3f){coordinate(),coordinate(),coordinate()*.5f-.5f};
        for (unsigned i=0; i<96; i++)
            indices[i] = pool[(random_bits() >> 16) % (sizeof(pool)/sizeof(pool[0]))];
        Renderer r; Scene scene; BackEnd backend; TestBuffers buffers;
        Object object; Mesh mesh; Texture texture; Material material;
        initialize_renderer(&r,&scene,&backend,&buffers);
        assert(r.acquireVertexCache == NULL);
        add_mesh_object(&scene,&object,&mesh,positions,indices,96);
        mesh.positions_count = 65536; /* Sparse and unused indices. */
        assert(meshUpdateGeometryValidity(&mesh));
        palette(&object,&mesh,&texture,&material,&uv,uv_indices,&pixel);
        r.frustumCulling = 0;
        r.illuminationEnabled = 0;
        object.transform = mat4Scale((Vec3f){-2,.5f,1.25f});
        object.transform.elements[3] = coordinate();
        object.transform.elements[7] = coordinate();
        object.transform.elements[11] = coordinate();
        int eligible = 1;
        switch (n % 6) {
            case 0: break;
            case 1: r.illuminationEnabled = 1; eligible = 0; break;
            case 2: mesh.shading_mode = MESH_SHADING_TEXTURED; eligible = 0; break;
            case 3: object.transform = mat4RotateY(coordinate()); eligible = 0; break;
            case 4: scene.transform = mat4RotateX(coordinate()); eligible = 0; break;
            case 5: mesh.illumination_policy = MESH_ILLUMINATION_SELF_ILLUMINATED; break;
        }
        if (n % 7 == 0) {
            r.camera_projection = mat4Perspective(1,2500,4.0f/3.0f,.4f + (n%9)*.1f);
            r.camera_view = mat4RotateY(coordinate());
        }
        paired(&r,&buffers,eligible,indices,96);
        /* Reuse the very same scratch after source/model/camera replacement. */
        positions[pool[n%11]].x = coordinate();
        mesh.positions = positions;
        object.transform = mat4Translate((Vec3f){coordinate(),coordinate(),coordinate()});
        scene.transform = mat4Identity();
        r.camera_view = mat4Translate((Vec3f){coordinate(),coordinate(),coordinate()});
        r.illuminationEnabled = 0;
        mesh.shading_mode = MESH_SHADING_FLAT_PALETTE;
        paired(&r,&buffers,1,indices,96);
        /* Failed acquisition and a busy lease must preserve exact fallback. */
        if (n % 100 == 0) {
            unavailable = 1; paired(&r,&buffers,0,indices,96); unavailable = 0;
            scratch.busy = 1;
            PingoVertexCache before = scratch;
            r.acquireVertexCache = acquire;
            assert(rendererRender(&r) == 0);
            assert(memcmp(&before,&scratch,sizeof(before)) == 0);
            scratch.busy = 0;
            object.inactive = 1;
            acquisitions = 0; assert(rendererRender(&r) == 0); assert(acquisitions == 0);
            object.inactive = 0;
            mesh.geometry_valid = 0;
            acquisitions = 0; assert(rendererRender(&r) == 0); assert(acquisitions == 0);
            mesh.geometry_valid = 1;
            object.texture_mapping_valid = 0;
            acquisitions = 0; assert(rendererRender(&r) == 0); assert(acquisitions == 0);
            object.texture_mapping_valid = 1;
            /* Actual nested render while outer scratch is leased. */
            Renderer inner = r; BackEnd inner_backend = backend; TestBuffers inner_buffers;
            inner_backend.clientCustomData = &inner_buffers;
            inner.backEnd = &inner_backend;
            inner.frameBuffer.frameBuffer = inner_buffers.frame;
            nested = &inner;
            assert(rendererRender(&r) == 0);
            assert(nested == NULL && scratch.busy == 0);
            Object second = object;
            second.transform = mat4Scale((Vec3f){.3f,-1.5f,2});
            assert(sceneAddRenderable(&scene,object_as_renderable(&second)) == 0);
            r.acquireVertexCache = NULL;
            assert(rendererRender(&r) == 0);
            TestBuffers expected = buffers;
            r.acquireVertexCache = acquire;
            assert(rendererRender(&r) == 0);
            assert(memcmp(&expected,&buffers,sizeof(expected)) == 0);
            assert(r.diagnostics.vertex_cache_draws == 2);
            assert(r.diagnostics.vertex_cache_misses == 2*trace_misses(indices,96));
            assert(scratch.busy == 0);
        }
    }
    free(positions);
    assert(nested_draws == 120);
    puts("24000 exact paired draws; collision/index65535/source/camera/fallback/reentry PASS");
    if (argc==2) terrain(argv[1]); else assert(argc==1);
    return 0;
}
