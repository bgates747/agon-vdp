/* P043: exact model arithmetic plus full-render witnesses, no production hooks. */
#ifdef P043_TEST_PRIVATE
#include "../video/pingo/render/renderer.c"
#endif
#define main existing_diagnostics_tests
#include "pingo_renderer_diagnostics_test.c"
#undef main
#include <float.h>

static unsigned transform_calls;
Vec4f __real_mat4MultiplyVec4(Vec4f *, Mat4 *);
Vec4f __wrap_mat4MultiplyVec4(Vec4f *v, Mat4 *m) {
    transform_calls++;
    return __real_mat4MultiplyVec4(v,m);
}
static uint32_t seed = 0x430045;
static uint32_t random_bits(void) {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
#ifdef P043_TEST_PRIVATE
static float finite_value(void) {
    uint32_t bits = random_bits();
    if ((bits & 0x7f800000u) == 0x7f800000u) bits ^= 0x00800000u;
    float value;
    memcpy(&value,&bits,sizeof(value));
    return value;
}
#endif
static float coordinate(void) {
    return ((float)(random_bits() >> 8) / 16777215.0f - .5f) * 6;
}
#ifdef P043_TEST_PRIVATE
static void arithmetic(void) {
    const float edges[] = {0,-0.0f,FLT_MIN,-FLT_MIN,FLT_TRUE_MIN,
        -FLT_TRUE_MIN,FLT_MAX,-FLT_MAX,1,-1,128,16384,.5f};
    for (unsigned n=0;n<1000000;++n) {
        Mat4 m=mat4Identity();
        Vec4f v={finite_value(),finite_value(),finite_value(),1};
        for(unsigned row=0;row<3;++row) {
            m.elements[row*5]=n%3 ? finite_value():edges[(n+row)%13];
            m.elements[row*4+3]=n%5 ? finite_value():edges[(n+row*3)%13];
            if (m.elements[row*4+3]==0) m.elements[row*4+3]=0;
        }
        if(n%7==0) v=(Vec4f){edges[n%13],edges[(n+1)%13],edges[(n+2)%13],1};
        if(n%11==0) m.elements[1]=m.elements[6]=m.elements[12]=-0.0f;
        assert(modelMatrixIsDiagonalAffine(&m));
        Vec4f expected=mat4MultiplyVec4(&v,&m);
        Vec4f actual=modelTransformDiagonalAffine(&v,&m);
        assert(memcmp(&expected,&actual,sizeof(actual))==0);
    }
    for(unsigned i=0;i<16;++i) {
        Mat4 m=mat4Identity();m.elements[i]=NAN;
        assert(!modelMatrixIsDiagonalAffine(&m));
        m.elements[i]=INFINITY;assert(!modelMatrixIsDiagonalAffine(&m));
    }
    for(unsigned row=0;row<3;++row) {
        Mat4 m=mat4Identity();m.elements[row*4+3]=-0.0f;
        assert(!modelMatrixIsDiagonalAffine(&m));
    }
    Mat4 m=mat4RotateY(.3f);assert(!modelMatrixIsDiagonalAffine(&m));
    m=mat4Identity();m.elements[1]=.01f;assert(!modelMatrixIsDiagonalAffine(&m));
    m=mat4Identity();m.elements[14]=.01f;assert(!modelMatrixIsDiagonalAffine(&m));
    puts("1000000 exact Vec4 states and fallback predicates PASS");
}
#endif
int main(int argc,char **argv) {
    assert(argc==2);
    assert(existing_diagnostics_tests()==0);
#ifdef P043_TEST_PRIVATE
    arithmetic();
#endif
    seed=0x430045;
    FILE *out=fopen(argv[1],"wb");assert(out);
    for(unsigned n=0;n<30000;++n) {
        Renderer r;Scene scene;BackEnd backend;TestBuffers buffers;
        Mesh mesh;Object object;
        Vec3f positions[3];
        for(unsigned v=0;v<3;++v)
            positions[v]=(Vec3f){coordinate(),coordinate(),coordinate()*.5f-.5f};
        uint16_t indices[]={0,1,2};
        initialize_renderer(&r,&scene,&backend,&buffers);
        add_mesh_object(&scene,&object,&mesh,positions,indices,3);
        r.frustumCulling=0;
        r.illuminationEnabled=(n%4)==0;
        if(n%4==1) mesh.illumination_policy=MESH_ILLUMINATION_SELF_ILLUMINATED;
        object.transform=mat4Scale((Vec3f){-2,.5f,1.25f});
        object.transform.elements[3]=coordinate();
        object.transform.elements[7]=coordinate();
        object.transform.elements[11]=coordinate();
        if(n%3==0) object.transform=mat4RotateY(coordinate());
        if(n%5==0) scene.transform=mat4RotateX(coordinate());
        if(n%7==0) { use_production_projection(&r);r.camera_view=mat4RotateY(coordinate()); }
        if(n%101==0) {positions[2]=positions[1];meshUpdateGeometryValidity(&mesh);}
        transform_calls=0;
        assert(rendererRender(&r)==0);
        assert_diagnostic_invariants(&r.diagnostics);
#ifdef P043_EXPECT_SPECIALIZED
        if(n%3 && n%5) assert(transform_calls==3);
#else
        assert(transform_calls==6);
#endif
        assert(fwrite(buffers.frame,sizeof(buffers.frame),1,out)==1);
        assert(fwrite(buffers.depth,sizeof(buffers.depth),1,out)==1);
    }
    assert(!fclose(out));
    puts("30000 renderer witnesses and transform call counts PASS");
    return 0;
}
