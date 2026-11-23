/*
** =========================================================================
** lwo_parser.h - LightWave 5 (IFF LWOB) & Wavefront OBJ 3D Parser for AmigaOS
** =========================================================================
*/

#ifndef LWO_PARSER_H
#define LWO_PARSER_H

#include <exec/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y, z;
} Vector3D;

typedef struct {
    Vector3D v0;
    Vector3D v1;
    Vector3D v2;
    Vector3D normal;
} Triangle3D;

typedef struct {
    Triangle3D *triangles;
    ULONG triangle_count;
    ULONG vertex_count;
    Vector3D min_bounds;
    Vector3D max_bounds;
} Mesh3D;

/*
** Function Prototypes
*/

/* Initialize an empty Mesh3D structure */
void InitMesh3D(Mesh3D *mesh);

/* Free memory allocated for a Mesh3D */
void FreeMesh3D(Mesh3D *mesh);

/* Check if a file is an IFF LWOB (LightWave 5) model */
BOOL IsLWOFile(const char *filename);

/* Check if a file is a Wavefront OBJ model */
BOOL IsOBJFile(const char *filename);

/* Check if a file is an STL model */
BOOL IsSTLFile(const char *filename);

/* Parse LightWave 5 IFF LWOB file into Mesh3D */
BOOL ParseLWO(const char *filename, Mesh3D *mesh);

/* Parse Wavefront OBJ file into Mesh3D */
BOOL ParseOBJ(const char *filename, Mesh3D *mesh);

/* Parse Binary or ASCII STL file into Mesh3D */
BOOL ParseSTL(const char *filename, Mesh3D *mesh);

/* Rotate Mesh3D around X axis (e.g. -90 degrees to convert LightWave Y-Up to Z-Up) */
void RotateMeshX(Mesh3D *mesh, float angle_deg);

/* Recalculate min/max 3D bounding box for Mesh3D */
void RecalculateMeshBounds(Mesh3D *mesh);

/* Resolve explicit scale or the negative Auto-Fit sentinel (200 mm). */
float ResolveMeshScale(const Mesh3D *mesh, float scale_factor);

/* Scale geometry uniformly and update its bounds; normals remain unchanged. */
void ScaleMesh3D(Mesh3D *mesh, float scale_factor);

/* Export Mesh3D to binary STL file (with optional scaling factor for CuraEngine) */
BOOL ExportMeshToSTL(const Mesh3D *mesh, const char *stl_filename, float scale_factor);

#include <GL/gl.h>
#include <GL/glu.h>

/* MiniGL 3D Display List Builder (Hardware Accelerated) */
GLuint BuildMeshDisplayList(const Mesh3D *mesh);

/* Render Compiled MiniGL 3D Display List with Camera Framing and 3D Print Bed Grid */
void RenderMeshDisplayList(GLuint display_list_id, const Mesh3D *mesh, float rot_x, float rot_y, float pan_x, float pan_y, float zoom, float bed_size_x, float bed_size_y);

/* Open Hardware-Accelerated MiniGL 3D Viewport Window */
void ShowMiniGLViewport(const Mesh3D *mesh, float bed_size_x, float bed_size_y);

#ifdef __cplusplus
}
#endif

#endif /* LWO_PARSER_H */
