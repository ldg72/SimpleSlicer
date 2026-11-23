#ifndef MODEL_TRANSFORM_H
#define MODEL_TRANSFORM_H

#include "lwo_parser.h"

typedef struct {
    /* XY = bounding-box centre relative to bed centre; Z = bottom height. */
    Vector3D position;
    Vector3D rotation; /* degrees, applied X then Y then Z */
    Vector3D scale;    /* positive ratios in model axes, before rotation */
} ModelTransform;

void ResetModelTransform(ModelTransform *transform);
/* Output must be empty. The source is never changed, including on failure. */
BOOL BuildTransformedMesh(const Mesh3D *source, Mesh3D *output,
                          const ModelTransform *transform, float unit_scale);
BOOL MeshFitsBuildVolume(const Mesh3D *mesh, float bed_x, float bed_y, float bed_z);

#endif
