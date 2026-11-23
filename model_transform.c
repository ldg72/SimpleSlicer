#include "model_transform.h"
#include <float.h>
#include <math.h>

void ResetModelTransform(ModelTransform *t)
{
    memset(t, 0, sizeof(*t));
    t->scale.x = t->scale.y = t->scale.z = 1.0f;
}

static Vector3D RotateVector(Vector3D v, const float c[3], const float s[3])
{
    Vector3D x = {v.x, c[0]*v.y-s[0]*v.z, s[0]*v.y+c[0]*v.z};
    Vector3D y = {c[1]*x.x+s[1]*x.z, x.y, -s[1]*x.x+c[1]*x.z};
    Vector3D z = {c[2]*y.x-s[2]*y.y, s[2]*y.x+c[2]*y.y, y.z};
    return z;
}

static void IncludePoint(Mesh3D *mesh, Vector3D v)
{
    if (v.x < mesh->min_bounds.x) mesh->min_bounds.x = v.x;
    if (v.y < mesh->min_bounds.y) mesh->min_bounds.y = v.y;
    if (v.z < mesh->min_bounds.z) mesh->min_bounds.z = v.z;
    if (v.x > mesh->max_bounds.x) mesh->max_bounds.x = v.x;
    if (v.y > mesh->max_bounds.y) mesh->max_bounds.y = v.y;
    if (v.z > mesh->max_bounds.z) mesh->max_bounds.z = v.z;
}

static BOOL ValidPoint(Vector3D v)
{
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z) &&
        fabsf(v.x) <= 1000000.0f && fabsf(v.y) <= 1000000.0f && fabsf(v.z) <= 1000000.0f;
}

BOOL BuildTransformedMesh(const Mesh3D *source, Mesh3D *output,
                          const ModelTransform *t, float units)
{
    if (!source || !output || !t || source == output || output->triangles ||
        !source->triangles || !source->triangle_count ||
        source->triangle_count > ((size_t)-1) / sizeof(Triangle3D) ||
        !isfinite(units) || units <= 0 || !ValidPoint(t->position) ||
        !ValidPoint(t->rotation) || !ValidPoint(t->scale) ||
        t->scale.x <= 0 || t->scale.y <= 0 || t->scale.z <= 0) return FALSE;

    Mesh3D result = *source;
    result.triangles = malloc(source->triangle_count * sizeof(Triangle3D));
    if (!result.triangles) return FALSE;
    result.min_bounds = (Vector3D){FLT_MAX, FLT_MAX, FLT_MAX};
    result.max_bounds = (Vector3D){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    Vector3D pivot = {
        (source->min_bounds.x + source->max_bounds.x) * 0.5f,
        (source->min_bounds.y + source->max_bounds.y) * 0.5f,
        (source->min_bounds.z + source->max_bounds.z) * 0.5f
    };
    float angles[3] = {t->rotation.x, t->rotation.y, t->rotation.z};
    float c[3], s[3];
    for (int i=0; i<3; i++) {
        float radians = fmodf(angles[i], 360.0f) * 0.0174532925199433f;
        c[i] = cosf(radians); s[i] = sinf(radians);
    }
    for (ULONG i=0; i<source->triangle_count; i++) {
        result.triangles[i] = source->triangles[i];
        Vector3D *v[3] = {&result.triangles[i].v0, &result.triangles[i].v1, &result.triangles[i].v2};
        for (int j=0; j<3; j++) {
            Vector3D p = {(v[j]->x-pivot.x)*units*t->scale.x,
                          (v[j]->y-pivot.y)*units*t->scale.y,
                          (v[j]->z-pivot.z)*units*t->scale.z};
            *v[j] = RotateVector(p, c, s);
            if (!ValidPoint(*v[j])) { free(result.triangles); return FALSE; }
            IncludePoint(&result, *v[j]);
        }
        /* Recompute normals after non-uniform scaling; preserve winding. */
        Vector3D a = {v[1]->x-v[0]->x, v[1]->y-v[0]->y, v[1]->z-v[0]->z};
        Vector3D b = {v[2]->x-v[0]->x, v[2]->y-v[0]->y, v[2]->z-v[0]->z};
        Vector3D n = {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
        float length = hypotf(hypotf(n.x, n.y), n.z);
        if (length > 0) { n.x /= length; n.y /= length; n.z /= length; }
        result.triangles[i].normal = n;
    }
    Vector3D offset = {
        t->position.x - (result.min_bounds.x+result.max_bounds.x)*0.5f,
        t->position.y - (result.min_bounds.y+result.max_bounds.y)*0.5f,
        t->position.z - result.min_bounds.z
    };
    for (ULONG i=0; i<result.triangle_count; i++) {
        Vector3D *v[3] = {&result.triangles[i].v0, &result.triangles[i].v1, &result.triangles[i].v2};
        for (int j=0; j<3; j++) {
            v[j]->x += offset.x; v[j]->y += offset.y; v[j]->z += offset.z;
            if (!ValidPoint(*v[j])) { free(result.triangles); return FALSE; }
        }
    }
    result.min_bounds.x += offset.x; result.max_bounds.x += offset.x;
    result.min_bounds.y += offset.y; result.max_bounds.y += offset.y;
    result.min_bounds.z += offset.z; result.max_bounds.z += offset.z;
    *output = result;
    return TRUE;
}

BOOL MeshFitsBuildVolume(const Mesh3D *mesh, float x, float y, float z)
{
    const float epsilon = 0.001f;
    return mesh && mesh->triangle_count && isfinite(x) && isfinite(y) && isfinite(z) &&
        x > 0 && y > 0 && z > 0 &&
        mesh->min_bounds.x >= -x*0.5f-epsilon && mesh->max_bounds.x <= x*0.5f+epsilon &&
        mesh->min_bounds.y >= -y*0.5f-epsilon && mesh->max_bounds.y <= y*0.5f+epsilon &&
        mesh->min_bounds.z >= -epsilon && mesh->max_bounds.z <= z+epsilon;
}
