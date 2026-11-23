#include "model_transform.h"
#include <assert.h>
#include <math.h>

static void close_to(float actual, float expected)
{
    assert(fabsf(actual-expected) < 0.002f);
}

static void dimensions(const Mesh3D *m, float x, float y, float z)
{
    close_to(m->max_bounds.x-m->min_bounds.x, x);
    close_to(m->max_bounds.y-m->min_bounds.y, y);
    close_to(m->max_bounds.z-m->min_bounds.z, z);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    /* Offset source geometry: transformations must not depend on file origin. */
    Triangle3D triangle = {{10,20,30},{30,20,30},{10,30,35},{0,0,1}};
    Triangle3D original = triangle;
    Mesh3D source = {0}, out = {0};
    source.triangles = &triangle;
    source.triangle_count = 1;
    source.vertex_count = 3;
    RecalculateMeshBounds(&source);
    ModelTransform t;
    ResetModelTransform(&t);

    assert(BuildTransformedMesh(&source,&out,&t,1));
    dimensions(&out,20,10,5);
    close_to(out.min_bounds.x,-10); close_to(out.max_bounds.x,10);
    close_to(out.min_bounds.y,-5); close_to(out.min_bounds.z,0);
    assert(MeshFitsBuildVolume(&out,220,220,250));
    FreeMesh3D(&out);

    t.position = (Vector3D){25,-15,7};
    t.rotation.z = 90;
    assert(BuildTransformedMesh(&source,&out,&t,1));
    dimensions(&out,10,20,5);
    close_to(out.min_bounds.x,20); close_to(out.min_bounds.y,-25);
    close_to(out.min_bounds.z,7);
    /* The real exporter/parser must preserve the exact preview coordinates. */
    assert(ExportMeshToSTL(&out,argv[1],1));
    Mesh3D roundtrip = {0};
    assert(ParseSTL(argv[1],&roundtrip));
    close_to(roundtrip.triangles[0].v0.x,out.triangles[0].v0.x);
    close_to(roundtrip.triangles[0].v1.y,out.triangles[0].v1.y);
    close_to(roundtrip.triangles[0].v2.z,out.triangles[0].v2.z);
    close_to(roundtrip.min_bounds.x,20); close_to(roundtrip.min_bounds.z,7);
    FreeMesh3D(&roundtrip); FreeMesh3D(&out);

    ResetModelTransform(&t);
    t.rotation.x=90;
    assert(BuildTransformedMesh(&source,&out,&t,1)); dimensions(&out,20,5,10); FreeMesh3D(&out);
    t.rotation=(Vector3D){0,90,0};
    assert(BuildTransformedMesh(&source,&out,&t,1)); dimensions(&out,5,10,20); FreeMesh3D(&out);
    t.rotation=(Vector3D){90,0,90};
    assert(BuildTransformedMesh(&source,&out,&t,1)); dimensions(&out,5,20,10); FreeMesh3D(&out);

    ResetModelTransform(&t);
    t.scale=(Vector3D){2,3,4};
    assert(BuildTransformedMesh(&source,&out,&t,1)); dimensions(&out,40,30,20);
    Vector3D n=out.triangles[0].normal;
    close_to(hypotf(hypotf(n.x,n.y),n.z),1);
    Vector3D edge={out.triangles[0].v2.x-out.triangles[0].v0.x,
        out.triangles[0].v2.y-out.triangles[0].v0.y,out.triangles[0].v2.z-out.triangles[0].v0.z};
    close_to(n.x*edge.x+n.y*edge.y+n.z*edge.z,0);
    FreeMesh3D(&out);

    float units[]={10,100,1000,10,1};
    ResetModelTransform(&t);
    for (unsigned int i=0;i<sizeof(units)/sizeof(units[0]);i++) {
        assert(BuildTransformedMesh(&source,&out,&t,units[i]));
        dimensions(&out,20*units[i],10*units[i],5*units[i]); FreeMesh3D(&out);
    }
    assert(BuildTransformedMesh(&source,&out,&t,ResolveMeshScale(&source,-1)));
    dimensions(&out,200,100,50); FreeMesh3D(&out);
    assert(memcmp(&triangle,&original,sizeof(triangle))==0);

    t.position.x=105;
    assert(BuildTransformedMesh(&source,&out,&t,1));
    assert(!MeshFitsBuildVolume(&out,220,220,250)); FreeMesh3D(&out);
    t.position.x=0; t.position.z=-1;
    assert(BuildTransformedMesh(&source,&out,&t,1));
    assert(!MeshFitsBuildVolume(&out,220,220,250)); FreeMesh3D(&out);
    t.position.z=0;
    assert(BuildTransformedMesh(&source,&out,&t,1));
    assert(MeshFitsBuildVolume(&out,20,10,5));
    assert(!MeshFitsBuildVolume(&out,20,10,4));
    assert(!MeshFitsBuildVolume(&out,NAN,10,5)); FreeMesh3D(&out);

    t.scale.x=0; assert(!BuildTransformedMesh(&source,&out,&t,1));
    t.scale.x=-1; assert(!BuildTransformedMesh(&source,&out,&t,1));
    t.scale.x=NAN; assert(!BuildTransformedMesh(&source,&out,&t,1));
    ResetModelTransform(&t); t.position.x=INFINITY;
    assert(!BuildTransformedMesh(&source,&out,&t,1));
    ResetModelTransform(&t);
    assert(!BuildTransformedMesh(&source,&out,&t,1e30f));
    assert(!out.triangles);
    assert(memcmp(&triangle,&original,sizeof(triangle))==0);
    puts("PASS: placement, rotations, scaling, normals, original preservation, STL round-trip and bounds");
    return 0;
}
