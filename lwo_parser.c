/*
** =========================================================================
** lwo_parser.c - LightWave 5 (IFF LWOB) & Wavefront OBJ 3D Parser for AmigaOS
** =========================================================================
*/

#include "lwo_parser.h"
#include <GL/glut.h>
#include <math.h>

/* Helper Macros for IFF ID comparisons */
#ifndef ID_FORM
#define ID_FORM 0x464F524DL /* 'FORM' */
#endif
#ifndef ID_LWOB
#define ID_LWOB 0x4C574F42L /* 'LWOB' */
#endif
#ifndef ID_PNTS
#define ID_PNTS 0x504E5453L /* 'PNTS' */
#endif
#ifndef ID_POLS
#define ID_POLS 0x504F4C53L /* 'POLS' */
#endif

/* Helper functions for byte swapping to Little Endian (required for Binary STL format) */
static inline unsigned int Swap32LE(unsigned int val)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return val;
#else
    return ((val & 0x000000FF) << 24) |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0xFF000000) >> 24);
#endif
}

static float SwapFloatLE(float f)
{
    union {
        float f;
        unsigned int i;
    } u;
    u.f = f;
    u.i = Swap32LE(u.i);
    return u.f;
}

/* Big Endian Read Helpers (IFF format is natively Big-Endian) */
static unsigned int ReadBE32(FILE *fp)
{
    unsigned char b[4];
    if (fread(b, 1, 4, fp) != 4) return 0;
    return ((unsigned int)b[0] << 24) | ((unsigned int)b[1] << 16) | ((unsigned int)b[2] << 8) | (unsigned int)b[3];
}

static unsigned short ReadBE16(FILE *fp)
{
    unsigned char b[2];
    if (fread(b, 1, 2, fp) != 2) return 0;
    return ((unsigned short)b[0] << 8) | (unsigned short)b[1];
}

static float ReadBEFloat(FILE *fp)
{
    union {
        float f;
        unsigned int i;
    } u;
    u.i = ReadBE32(fp);
    return u.f;
}

/* Calculate triangle surface normal */
static Vector3D CalculateNormal(Vector3D v0, Vector3D v1, Vector3D v2)
{
    Vector3D n;
    float ax = v1.x - v0.x;
    float ay = v1.y - v0.y;
    float az = v1.z - v0.z;

    float bx = v2.x - v0.x;
    float by = v2.y - v0.y;
    float bz = v2.z - v0.z;

    n.x = ay * bz - az * by;
    n.y = az * bx - ax * bz;
    n.z = ax * by - ay * bx;

    float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 0.000001f)
    {
        n.x /= len;
        n.y /= len;
        n.z /= len;
    }
    else
    {
        n.x = 0.0f; n.y = 0.0f; n.z = 1.0f;
    }
    return n;
}

void InitMesh3D(Mesh3D *mesh)
{
    if (!mesh) return;
    mesh->triangles = NULL;
    mesh->triangle_count = 0;
    mesh->vertex_count = 0;
    mesh->min_bounds.x = mesh->min_bounds.y = mesh->min_bounds.z = 999999.0f;
    mesh->max_bounds.x = mesh->max_bounds.y = mesh->max_bounds.z = -999999.0f;
}

void FreeMesh3D(Mesh3D *mesh)
{
    if (!mesh) return;
    if (mesh->triangles)
    {
        free(mesh->triangles);
        mesh->triangles = NULL;
    }
    mesh->triangle_count = 0;
    mesh->vertex_count = 0;
}

void RecalculateMeshBounds(Mesh3D *mesh)
{
    if (!mesh || mesh->triangle_count == 0) return;

    mesh->min_bounds.x = mesh->min_bounds.y = mesh->min_bounds.z = 999999.0f;
    mesh->max_bounds.x = mesh->max_bounds.y = mesh->max_bounds.z = -999999.0f;

    for (ULONG i = 0; i < mesh->triangle_count; i++)
    {
        Vector3D *v[3] = { &mesh->triangles[i].v0, &mesh->triangles[i].v1, &mesh->triangles[i].v2 };
        for (int k = 0; k < 3; k++)
        {
            if (v[k]->x < mesh->min_bounds.x) mesh->min_bounds.x = v[k]->x;
            if (v[k]->x > mesh->max_bounds.x) mesh->max_bounds.x = v[k]->x;
            if (v[k]->y < mesh->min_bounds.y) mesh->min_bounds.y = v[k]->y;
            if (v[k]->y > mesh->max_bounds.y) mesh->max_bounds.y = v[k]->y;
            if (v[k]->z < mesh->min_bounds.z) mesh->min_bounds.z = v[k]->z;
            if (v[k]->z > mesh->max_bounds.z) mesh->max_bounds.z = v[k]->z;
        }
    }
}

void RotateMeshX(Mesh3D *mesh, float angle_deg)
{
    if (!mesh || mesh->triangle_count == 0) return;

    float rad = angle_deg * 3.14159265f / 180.0f;
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    for (ULONG i = 0; i < mesh->triangle_count; i++)
    {
        Triangle3D *tri = &mesh->triangles[i];

        float y0 = tri->v0.y; float z0 = tri->v0.z;
        tri->v0.y = y0 * cos_a - z0 * sin_a;
        tri->v0.z = y0 * sin_a + z0 * cos_a;

        float y1 = tri->v1.y; float z1 = tri->v1.z;
        tri->v1.y = y1 * cos_a - z1 * sin_a;
        tri->v1.z = y1 * sin_a + z1 * cos_a;

        float y2 = tri->v2.y; float z2 = tri->v2.z;
        tri->v2.y = y2 * cos_a - z2 * sin_a;
        tri->v2.z = y2 * sin_a + z2 * cos_a;

        float ny = tri->normal.y; float nz = tri->normal.z;
        tri->normal.y = ny * cos_a - nz * sin_a;
        tri->normal.z = ny * sin_a + nz * cos_a;
    }

    RecalculateMeshBounds(mesh);
    printf("Rotated Mesh %.1f deg around X-axis (Y-Up to Z-Up conversion)\n", angle_deg);
}

BOOL IsLWOFile(const char *filename)
{
    if (!filename) return FALSE;

    char *ext = strrchr(filename, '.');
    if (ext && (strcasecmp(ext, ".lwo") == 0 || strcasecmp(ext, ".lwob") == 0))
    {
        return TRUE;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) return FALSE;

    unsigned char buf[12];
    size_t n = fread(buf, 1, 12, fp);
    fclose(fp);

    if (n >= 12 &&
        buf[0] == 'F' && buf[1] == 'O' && buf[2] == 'R' && buf[3] == 'M' &&
        buf[8] == 'L' && buf[9] == 'W' && buf[10] == 'O' && buf[11] == 'B')
    {
        return TRUE;
    }

    return FALSE;
}

BOOL IsOBJFile(const char *filename)
{
    if (!filename) return FALSE;

    char *ext = strrchr(filename, '.');
    if (ext && strcasecmp(ext, ".obj") == 0)
    {
        return TRUE;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) return FALSE;

    char line[256];
    int v_count = 0;
    while (fgets(line, sizeof(line), fp) && v_count < 3)
    {
        if (line[0] == 'v' && line[1] == ' ') v_count++;
    }
    fclose(fp);

    return (v_count >= 3);
}

BOOL IsSTLFile(const char *filename)
{
    if (!filename) return FALSE;

    char *ext = strrchr(filename, '.');
    if (ext && strcasecmp(ext, ".stl") == 0)
    {
        return TRUE;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) return FALSE;

    char header[80];
    size_t read_bytes = fread(header, 1, 80, fp);
    fclose(fp);

    if (read_bytes == 80 && strncasecmp(header, "solid", 5) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

/*
** Parse LightWave 5 (IFF LWOB) File
*/
BOOL ParseLWO(const char *filename, Mesh3D *mesh)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        printf("Error: Could not open LWO file '%s'\n", filename);
        return FALSE;
    }

    InitMesh3D(mesh);

    unsigned int form_id = ReadBE32(fp);
    unsigned int form_size = ReadBE32(fp);
    unsigned int lwob_id = ReadBE32(fp);

    if (form_id != ID_FORM || lwob_id != ID_LWOB)
    {
        printf("Error: File '%s' is not a valid IFF LWOB (LightWave 5) object\n", filename);
        fclose(fp);
        return FALSE;
    }

    Vector3D *pts = NULL;
    ULONG num_pts = 0;

    ULONG tri_capacity = 256;
    mesh->triangles = (Triangle3D *)malloc(sizeof(Triangle3D) * tri_capacity);
    if (!mesh->triangles)
    {
        fclose(fp);
        return FALSE;
    }

    long form_end = 8 + form_size;

    while (ftell(fp) < form_end && !feof(fp))
    {
        unsigned int chunk_id = ReadBE32(fp);
        unsigned int chunk_size = ReadBE32(fp);
        long chunk_start = ftell(fp);
        long chunk_end = chunk_start + chunk_size;

        if (chunk_id == ID_PNTS)
        {
            num_pts = chunk_size / 12;
            pts = (Vector3D *)malloc(sizeof(Vector3D) * num_pts);
            if (pts)
            {
                for (ULONG i = 0; i < num_pts; i++)
                {
                    pts[i].x = ReadBEFloat(fp);
                    pts[i].y = ReadBEFloat(fp);
                    pts[i].z = ReadBEFloat(fp);

                    if (pts[i].x < mesh->min_bounds.x) mesh->min_bounds.x = pts[i].x;
                    if (pts[i].x > mesh->max_bounds.x) mesh->max_bounds.x = pts[i].x;
                    if (pts[i].y < mesh->min_bounds.y) mesh->min_bounds.y = pts[i].y;
                    if (pts[i].y > mesh->max_bounds.y) mesh->max_bounds.y = pts[i].y;
                    if (pts[i].z < mesh->min_bounds.z) mesh->min_bounds.z = pts[i].z;
                    if (pts[i].z > mesh->max_bounds.z) mesh->max_bounds.z = pts[i].z;
                }
                mesh->vertex_count = num_pts;
            }
        }
        else if (chunk_id == ID_POLS && pts != NULL)
        {
            while (ftell(fp) < chunk_end && !feof(fp))
            {
                unsigned short num_v = ReadBE16(fp);
                if (num_v < 3)
                {
                    /* Skip invalid or 1-2 vertex polygons */
                    for (unsigned short i = 0; i < num_v; i++) ReadBE16(fp);
                    ReadBE16(fp); /* surf_id */
                    continue;
                }

                unsigned short *v_idx = (unsigned short *)malloc(sizeof(unsigned short) * num_v);
                if (!v_idx) break;

                for (unsigned short i = 0; i < num_v; i++)
                {
                    v_idx[i] = ReadBE16(fp);
                }

                short surf_id = (short)ReadBE16(fp);
                if (surf_id < 0)
                {
                    /* Detail polygon sub-chunks follow, skip detail count */
                    ReadBE16(fp);
                }

                /*
                ** Triangulation of Polygons (Triangles, Quads -> 2 Triangles, N-Gons -> Fan)
                */
                unsigned short num_tris = num_v - 2;
                for (unsigned short t = 0; t < num_tris; t++)
                {
                    unsigned short idx0 = v_idx[0];
                    unsigned short idx1 = v_idx[t + 1];
                    unsigned short idx2 = v_idx[t + 2];

                    if (idx0 < num_pts && idx1 < num_pts && idx2 < num_pts)
                    {
                        if (mesh->triangle_count >= tri_capacity)
                        {
                            tri_capacity *= 2;
                            Triangle3D *new_tris = (Triangle3D *)realloc(mesh->triangles, sizeof(Triangle3D) * tri_capacity);
                            if (!new_tris) break;
                            mesh->triangles = new_tris;
                        }

                        Triangle3D *tri = &mesh->triangles[mesh->triangle_count];
                        tri->v0 = pts[idx0];
                        tri->v1 = pts[idx1];
                        tri->v2 = pts[idx2];
                        tri->normal = CalculateNormal(tri->v0, tri->v1, tri->v2);
                        mesh->triangle_count++;
                    }
                }

                free(v_idx);
            }
        }

        /* Seek to end of chunk (handle odd padding byte) */
        long seek_pos = chunk_end;
        if (chunk_size & 1) seek_pos++;
        fseek(fp, seek_pos, SEEK_SET);
    }

    if (pts) free(pts);
    fclose(fp);

    printf("LWO Loaded: %lu vertices, %lu triangulated faces\n", mesh->vertex_count, mesh->triangle_count);
    return (mesh->triangle_count > 0);
}

/*
** Parse Wavefront ASCII OBJ File
*/
BOOL ParseOBJ(const char *filename, Mesh3D *mesh)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        printf("Error: Could not open OBJ file '%s'\n", filename);
        return FALSE;
    }

    InitMesh3D(mesh);

    ULONG vert_capacity = 512;
    Vector3D *verts = (Vector3D *)malloc(sizeof(Vector3D) * vert_capacity);
    ULONG vert_count = 0;

    ULONG tri_capacity = 512;
    mesh->triangles = (Triangle3D *)malloc(sizeof(Triangle3D) * tri_capacity);

    if (!verts || !mesh->triangles)
    {
        if (verts) free(verts);
        fclose(fp);
        return FALSE;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == 'v' && line[1] == ' ')
        {
            Vector3D v;
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3)
            {
                if (vert_count >= vert_capacity)
                {
                    vert_capacity *= 2;
                    Vector3D *new_verts = (Vector3D *)realloc(verts, sizeof(Vector3D) * vert_capacity);
                    if (!new_verts) break;
                    verts = new_verts;
                }
                verts[vert_count++] = v;

                if (v.x < mesh->min_bounds.x) mesh->min_bounds.x = v.x;
                if (v.x > mesh->max_bounds.x) mesh->max_bounds.x = v.x;
                if (v.y < mesh->min_bounds.y) mesh->min_bounds.y = v.y;
                if (v.y > mesh->max_bounds.y) mesh->max_bounds.y = v.y;
                if (v.z < mesh->min_bounds.z) mesh->min_bounds.z = v.z;
                if (v.z > mesh->max_bounds.z) mesh->max_bounds.z = v.z;
            }
        }
        else if (line[0] == 'f' && line[1] == ' ')
        {
            int f_indices[32];
            int f_count = 0;

            char *token = strtok(line + 2, " \t\r\n");
            while (token && f_count < 32)
            {
                int v_idx = 0;
                sscanf(token, "%d", &v_idx);
                if (v_idx > 0) f_indices[f_count++] = v_idx - 1; /* 1-based to 0-based */
                else if (v_idx < 0) f_indices[f_count++] = vert_count + v_idx;
                token = strtok(NULL, " \t\r\n");
            }

            /* Triangulate polygon faces */
            if (f_count >= 3)
            {
                for (int t = 0; t < f_count - 2; t++)
                {
                    int idx0 = f_indices[0];
                    int idx1 = f_indices[t + 1];
                    int idx2 = f_indices[t + 2];

                    if (idx0 < (int)vert_count && idx1 < (int)vert_count && idx2 < (int)vert_count)
                    {
                        if (mesh->triangle_count >= tri_capacity)
                        {
                            tri_capacity *= 2;
                            Triangle3D *new_tris = (Triangle3D *)realloc(mesh->triangles, sizeof(Triangle3D) * tri_capacity);
                            if (!new_tris) break;
                            mesh->triangles = new_tris;
                        }

                        Triangle3D *tri = &mesh->triangles[mesh->triangle_count];
                        tri->v0 = verts[idx0];
                        tri->v1 = verts[idx1];
                        tri->v2 = verts[idx2];
                        tri->normal = CalculateNormal(tri->v0, tri->v1, tri->v2);
                        mesh->triangle_count++;
                    }
                }
            }
        }
    }

    mesh->vertex_count = vert_count;
    free(verts);
    fclose(fp);

    printf("OBJ Loaded: %lu vertices, %lu triangulated faces\n", mesh->vertex_count, mesh->triangle_count);
    return (mesh->triangle_count > 0);
}

/*
** Parse Binary or ASCII STL File into Mesh3D
*/
BOOL ParseSTL(const char *filename, Mesh3D *mesh)
{
    if (!filename || !mesh) return FALSE;
    InitMesh3D(mesh);

    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        printf("Error: Could not open STL file '%s'\n", filename);
        return FALSE;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 84)
    {
        fclose(fp);
        return FALSE;
    }

    char header[80];
    if (fread(header, 1, 80, fp) != 80)
    {
        fclose(fp);
        return FALSE;
    }

    unsigned int num_triangles_le = 0;
    if (fread(&num_triangles_le, 4, 1, fp) != 1)
    {
        fclose(fp);
        return FALSE;
    }

    unsigned int num_triangles = Swap32LE(num_triangles_le);
    long expected_binary_size = 84 + (long)num_triangles * 50;

    /* Check if this is a valid Binary STL file */
    if (file_size == expected_binary_size || strncasecmp(header, "solid", 5) != 0)
    {
        if (num_triangles == 0)
        {
            fclose(fp);
            return FALSE;
        }

        mesh->triangles = (Triangle3D *)malloc(sizeof(Triangle3D) * num_triangles);
        if (!mesh->triangles)
        {
            fclose(fp);
            return FALSE;
        }
        mesh->triangle_count = num_triangles;

        for (unsigned int i = 0; i < num_triangles; i++)
        {
            float data[12];
            unsigned short attr;
            if (fread(data, sizeof(float), 12, fp) != 12) break;
            fread(&attr, 2, 1, fp);

            /* Byte swap LE floats to native PPC Big-Endian */
            for (int k = 0; k < 12; k++)
            {
                data[k] = SwapFloatLE(data[k]);
            }

            mesh->triangles[i].normal.x = data[0];
            mesh->triangles[i].normal.y = data[1];
            mesh->triangles[i].normal.z = data[2];

            mesh->triangles[i].v0.x = data[3];
            mesh->triangles[i].v0.y = data[4];
            mesh->triangles[i].v0.z = data[5];

            mesh->triangles[i].v1.x = data[6];
            mesh->triangles[i].v1.y = data[7];
            mesh->triangles[i].v1.z = data[8];

            mesh->triangles[i].v2.x = data[9];
            mesh->triangles[i].v2.y = data[10];
            mesh->triangles[i].v2.z = data[11];
        }
        fclose(fp);

        RecalculateMeshBounds(mesh);
        printf("Binary STL Loaded: %lu triangles\n", mesh->triangle_count);
        return TRUE;
    }

    /* Fallback: Parse ASCII STL File */
    fseek(fp, 0, SEEK_SET);
    char line[256];
    ULONG tri_capacity = 1000;
    mesh->triangles = (Triangle3D *)malloc(sizeof(Triangle3D) * tri_capacity);
    mesh->triangle_count = 0;

    Vector3D current_normal = {0.0f, 0.0f, 1.0f};
    Vector3D verts[3];
    int v_idx = 0;

    while (fgets(line, sizeof(line), fp))
    {
        char *ptr = line;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (strncasecmp(ptr, "facet normal", 12) == 0)
        {
            sscanf(ptr + 12, "%f %f %f", &current_normal.x, &current_normal.y, &current_normal.z);
            v_idx = 0;
        }
        else if (strncasecmp(ptr, "vertex", 6) == 0 && v_idx < 3)
        {
            sscanf(ptr + 6, "%f %f %f", &verts[v_idx].x, &verts[v_idx].y, &verts[v_idx].z);
            v_idx++;
            if (v_idx == 3)
            {
                if (mesh->triangle_count >= tri_capacity)
                {
                    tri_capacity *= 2;
                    Triangle3D *new_tris = (Triangle3D *)realloc(mesh->triangles, sizeof(Triangle3D) * tri_capacity);
                    if (!new_tris) break;
                    mesh->triangles = new_tris;
                }
                Triangle3D *tri = &mesh->triangles[mesh->triangle_count++];
                tri->normal = current_normal;
                tri->v0 = verts[0];
                tri->v1 = verts[1];
                tri->v2 = verts[2];
            }
        }
    }
    fclose(fp);

    RecalculateMeshBounds(mesh);
    printf("ASCII STL Loaded: %lu triangles\n", mesh->triangle_count);
    return (mesh->triangle_count > 0);
}

/*
** Export Mesh3D to Binary STL File (Little Endian required for CuraEngine)
*/
float ResolveMeshScale(const Mesh3D *mesh, float scale_factor)
{
    if (!mesh || mesh->triangle_count == 0) return 1.0f;

    float scale = scale_factor;
    if (scale < 0.0f)
    {
        /* Auto-Fit mode: scale model so max dimension equals 200.0 mm (20 cm Ender 3 bed) */
        float dim_x = mesh->max_bounds.x - mesh->min_bounds.x;
        float dim_y = mesh->max_bounds.y - mesh->min_bounds.y;
        float dim_z = mesh->max_bounds.z - mesh->min_bounds.z;

        float max_dim = dim_x;
        if (dim_y > max_dim) max_dim = dim_y;
        if (dim_z > max_dim) max_dim = dim_z;

        if (max_dim > 0.0001f)
        {
            scale = 200.0f / max_dim;
        }
        else
        {
            scale = 1.0f;
        }
    }
    else if (scale < 0.00001f)
    {
        scale = 1.0f;
    }

    return scale;
}

void ScaleMesh3D(Mesh3D *mesh, float scale_factor)
{
    if (!mesh || mesh->triangle_count == 0) return;
    float scale = ResolveMeshScale(mesh, scale_factor);
    for (ULONG i = 0; i < mesh->triangle_count; i++)
    {
        Vector3D *vertices[3] = {
            &mesh->triangles[i].v0, &mesh->triangles[i].v1, &mesh->triangles[i].v2
        };
        for (int j = 0; j < 3; j++)
        {
            vertices[j]->x *= scale;
            vertices[j]->y *= scale;
            vertices[j]->z *= scale;
        }
    }
    RecalculateMeshBounds(mesh);
}

BOOL ExportMeshToSTL(const Mesh3D *mesh, const char *stl_filename, float scale_factor)
{
    if (!mesh || mesh->triangle_count == 0 || !stl_filename) return FALSE;
    float scale = ResolveMeshScale(mesh, scale_factor);

    FILE *fp = fopen(stl_filename, "wb");
    if (!fp)
    {
        printf("Error: Could not create temp STL file '%s'\n", stl_filename);
        return FALSE;
    }

    /* 80-byte header */
    char header[80] = "SimpleSlicer LightWave/OBJ Converted Binary STL";
    fwrite(header, 1, 80, fp);

    /* Triangle count (32-bit Little Endian) */
    unsigned int num_tris_le = Swap32LE(mesh->triangle_count);
    fwrite(&num_tris_le, 4, 1, fp);

    /* Triangles */
    for (ULONG i = 0; i < mesh->triangle_count; i++)
    {
        const Triangle3D *tri = &mesh->triangles[i];

        float n[3]  = { SwapFloatLE(tri->normal.x), SwapFloatLE(tri->normal.y), SwapFloatLE(tri->normal.z) };
        float v0[3] = { SwapFloatLE(tri->v0.x * scale), SwapFloatLE(tri->v0.y * scale), SwapFloatLE(tri->v0.z * scale) };
        float v1[3] = { SwapFloatLE(tri->v1.x * scale), SwapFloatLE(tri->v1.y * scale), SwapFloatLE(tri->v1.z * scale) };
        float v2[3] = { SwapFloatLE(tri->v2.x * scale), SwapFloatLE(tri->v2.y * scale), SwapFloatLE(tri->v2.z * scale) };

        fwrite(n, sizeof(float), 3, fp);
        fwrite(v0, sizeof(float), 3, fp);
        fwrite(v1, sizeof(float), 3, fp);
        fwrite(v2, sizeof(float), 3, fp);

        unsigned short attr_byte_count = 0;
        fwrite(&attr_byte_count, 2, 1, fp);
    }

    BOOL written = !ferror(fp);
    if (fclose(fp) != 0) written = FALSE;
    if (!written) return FALSE;
    printf("Exported temp STL file '%s' (%lu triangles, scale x%.1f)\n", stl_filename, mesh->triangle_count, scale);
    return TRUE;
}

/*
** MiniGL 3D Display List Builder (Hardware Accelerated)
*/
GLuint BuildMeshDisplayList(const Mesh3D *mesh)
{
    if (!mesh || mesh->triangle_count == 0) return 0;

    GLuint list_id = glGenLists(1);
    if (list_id == 0) return 0;

    glNewList(list_id, GL_COMPILE);

    GLfloat mat_ambient[]   = { 0.1f, 0.3f, 0.4f, 1.0f };
    GLfloat mat_diffuse[]   = { 0.2f, 0.7f, 0.9f, 1.0f };
    GLfloat mat_specular[]  = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat mat_shininess[] = { 40.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

    glBegin(GL_TRIANGLES);
    for (ULONG i = 0; i < mesh->triangle_count; i++)
    {
        const Triangle3D *tri = &mesh->triangles[i];
        glNormal3f(tri->normal.x, tri->normal.y, tri->normal.z);
        glVertex3f(tri->v0.x, tri->v0.y, tri->v0.z);
        glVertex3f(tri->v1.x, tri->v1.y, tri->v1.z);
        glVertex3f(tri->v2.x, tri->v2.y, tri->v2.z);
    }
    glEnd();

    glEndList();
    printf("Compiled MiniGL 3D Display List ID %u (%lu triangles)\n", list_id, mesh->triangle_count);
    return list_id;
}

/*
** Render Compiled MiniGL 3D Display List with Camera Framing and 3D Print Bed Grid
*/
void RenderMeshDisplayList(GLuint display_list_id, const Mesh3D *mesh, float rot_x, float rot_y, float pan_x, float pan_y, float zoom, float bed_size_x, float bed_size_y)
{
    if (!mesh || mesh->triangle_count == 0) return;

    glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Keep the camera fixed on the bed: moving/scaling a model must not
       silently follow it or resize the bed on screen. */
    float bed_hx = (isfinite(bed_size_x) && bed_size_x > 0.0f ? bed_size_x : 220.0f) * 0.5f;
    float bed_hy = (isfinite(bed_size_y) && bed_size_y > 0.0f ? bed_size_y : 220.0f) * 0.5f;
    float cam_dist = fmaxf(bed_hx, bed_hy) * 3.6f * (zoom > 0.1f ? zoom : 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(0.0, -cam_dist, cam_dist * 0.6f,
              0.0, 0.0, 0.0,
              0.0, 0.0, 1.0);

    /*
    ** Frontal Viewport Lighting (Eye Space / Headlight)
    ** Placed AFTER gluLookAt so light direction is locked to the camera view
    */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    GLfloat light_position[] = { 0.0f, 0.0f, 1.0f, 0.0f }; /* Directional light straight from camera into scene */
    GLfloat ambient_light[]  = { 0.35f, 0.35f, 0.38f, 1.0f };
    GLfloat diffuse_light[]  = { 0.90f, 0.90f, 0.90f, 1.0f };
    GLfloat specular_light[] = { 0.40f, 0.40f, 0.40f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient_light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse_light);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular_light);

    glPushMatrix();

    /* Camera orbit/pan affects the complete scene, never exported geometry. */
    glTranslatef(pan_x, pan_y, 0.0f);

    glRotatef(rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(rot_y, 0.0f, 0.0f, 1.0f);

    /* Draw 3D Printer Bed Grid */
    glDisable(GL_LIGHTING);
    glColor3f(0.3f, 0.35f, 0.4f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (float x = -bed_hx; x <= bed_hx; x += 20.0f)
    {
        glVertex3f(x, -bed_hy, 0.0f);
        glVertex3f(x, bed_hy, 0.0f);
    }
    for (float y = -bed_hy; y <= bed_hy; y += 20.0f)
    {
        glVertex3f(-bed_hx, y, 0.0f);
        glVertex3f(bed_hx, y, 0.0f);
    }
    glEnd();

    /* Explicit perimeter makes out-of-bed placement visible. */
    glColor3f(0.6f, 0.65f, 0.7f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-bed_hx, -bed_hy, 0.0f);
    glVertex3f(bed_hx, -bed_hy, 0.0f);
    glVertex3f(bed_hx, bed_hy, 0.0f);
    glVertex3f(-bed_hx, bed_hy, 0.0f);
    glEnd();

    /* Render 3D Model with Frontal Light */
    glEnable(GL_LIGHTING);
    if (display_list_id > 0)
    {
        glCallList(display_list_id);
    }
    else
    {
        GLfloat mat_ambient[]   = { 0.15f, 0.35f, 0.45f, 1.0f };
        GLfloat mat_diffuse[]   = { 0.25f, 0.75f, 0.95f, 1.0f };
        GLfloat mat_specular[]  = { 0.80f, 0.80f, 0.80f, 1.0f };
        GLfloat mat_shininess[] = { 40.0f };

        glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

        glBegin(GL_TRIANGLES);
        for (ULONG i = 0; i < mesh->triangle_count; i++)
        {
            const Triangle3D *tri = &mesh->triangles[i];
            glNormal3f(tri->normal.x, tri->normal.y, tri->normal.z);
            glVertex3f(tri->v0.x, tri->v0.y, tri->v0.z);
            glVertex3f(tri->v1.x, tri->v1.y, tri->v1.z);
            glVertex3f(tri->v2.x, tri->v2.y, tri->v2.z);
        }
        glEnd();
    }

    glPopMatrix();
}

/*
** Interactive Hardware-Accelerated MiniGL 3D Viewport Window
*/
static const Mesh3D *g_view_mesh = NULL;
static float g_view_bed_x = 220.0f;
static float g_view_bed_y = 220.0f;
static float g_rot_x = 25.0f;
static float g_rot_y = 35.0f;

static void ViewportDisplayCallback(void)
{
    if (g_view_mesh)
    {
        RenderMeshDisplayList(0, g_view_mesh, g_rot_x, g_rot_y, 0.0f, 0.0f, 1.0f, g_view_bed_x, g_view_bed_y);
        glutSwapBuffers();
    }
}

static void ViewportReshapeCallback(int width, int height)
{
    if (height == 0) height = 1;
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (GLfloat)width / (GLfloat)height, 1.0, 5000.0);
}

static void ViewportIdleCallback(void)
{
    g_rot_y += 0.5f;
    if (g_rot_y >= 360.0f) g_rot_y -= 360.0f;
    glutPostRedisplay();
}

static void ViewportKeyboardCallback(unsigned char key, int x, int y)
{
    (void)x; (void)y;
    if (key == 27 || key == 'q' || key == 'Q')
    {
        /* Close viewport window */
    }
}

void ShowMiniGLViewport(const Mesh3D *mesh, float bed_size_x, float bed_size_y)
{
    if (!mesh || mesh->triangle_count == 0) return;

    g_view_mesh = mesh;
    g_view_bed_x = (bed_size_x > 10.0f) ? bed_size_x : 220.0f;
    g_view_bed_y = (bed_size_y > 10.0f) ? bed_size_y : 220.0f;

    static BOOL glut_initialized = FALSE;
    if (!glut_initialized)
    {
        int dummy_argc = 1;
        char *dummy_argv[] = { "SimpleSlicer3D", NULL };
        glutInit(&dummy_argc, dummy_argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(700, 550);
        glutInitWindowPosition(100, 100);
        glutCreateWindow("SimpleSlicer 3D Viewport - MiniGL / Warp3D");

        glutDisplayFunc(ViewportDisplayCallback);
        glutReshapeFunc(ViewportReshapeCallback);
        glutIdleFunc(ViewportIdleCallback);
        glutKeyboardFunc(ViewportKeyboardCallback);

        glut_initialized = TRUE;
    }

    glutPostRedisplay();
    glutMainLoop();
}
