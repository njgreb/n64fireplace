#ifndef HEARTH_T3D_GFX_H
#define HEARTH_T3D_GFX_H

#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <stdbool.h>
#include <stdint.h>

#define T3D_FB_COUNT    3
#define T3D_WORLD_SCALE 32.0f
#define T3D_ROOM_SCALE  256.0f
#define T3D_PROP_SCALE  1024.0f

typedef struct {
    sprite_t *sprite;
    uint16_t w;
    uint16_t h;
    bool repeat;
} T3DTexture;

typedef struct {
    T3DVertPacked *verts;
    rspq_block_t *block;
} T3DMesh;

typedef struct {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float r, g, b, a;
} T3DBuildVert;

typedef struct T3DMeshBuilder T3DMeshBuilder;

void t3d_gfx_init(void);
void t3d_gfx_frame_begin(int frame_idx);
const T3DMat4FP *t3d_gfx_room_mat(void);

void t3d_tex_load(T3DTexture *out, const char *path, bool repeat);
void t3d_tex_bind(const T3DTexture *tex);

T3DMeshBuilder *t3d_mb_begin(float vert_scale);
void t3d_mb_set_texture(T3DMeshBuilder *b, const T3DTexture *tex);
void t3d_mb_set_cull(T3DMeshBuilder *b, bool cull_back);
void t3d_mb_set_lit(T3DMeshBuilder *b, bool lit);
void t3d_mb_set_blend(T3DMeshBuilder *b, bool blend, bool depth_write);
void t3d_mb_set_color(T3DMeshBuilder *b, float r, float g, float b_c, float a);
void t3d_mb_get_color(const T3DMeshBuilder *b, float *r, float *g, float *b_c, float *a);
void t3d_mb_set_normal(T3DMeshBuilder *b, float nx, float ny, float nz);

void t3d_mb_push_tri_vc(T3DMeshBuilder *b,
                        const T3DBuildVert *v0,
                        const T3DBuildVert *v1,
                        const T3DBuildVert *v2);
void t3d_mb_push_quad_vc(T3DMeshBuilder *b,
                         const T3DBuildVert *v0,
                         const T3DBuildVert *v1,
                         const T3DBuildVert *v2,
                         const T3DBuildVert *v3);
void t3d_mb_push_tri(T3DMeshBuilder *b,
                     float x0, float y0, float z0,
                     float x1, float y1, float z1,
                     float x2, float y2, float z2);
void t3d_mb_push_quad(T3DMeshBuilder *b,
                      float x0, float y0, float z0,
                      float x1, float y1, float z1,
                      float x2, float y2, float z2,
                      float x3, float y3, float z3);
T3DMesh t3d_mb_end(T3DMeshBuilder *b);
void t3d_mesh_draw(const T3DMesh *m);

void t3d_xf_reset(void);
void t3d_xf_push(void);
void t3d_xf_pop(void);
void t3d_xf_translate(float tx, float ty, float tz);
void t3d_xf_rotate_x(float deg);
void t3d_xf_rotate_y(float deg);
void t3d_xf_rotate_z(float deg);
void t3d_xf_scale(float sx, float sy, float sz);
void t3d_xf_apply(float mesh_scale);

void t3d_dyn_begin_quads(const T3DTexture *tex, float vert_scale,
                         bool blend, bool depth_write, bool cull_back, bool lit);
void t3d_dyn_quad_vc(const T3DBuildVert *v0, const T3DBuildVert *v1,
                     const T3DBuildVert *v2, const T3DBuildVert *v3);
void t3d_dyn_end_quads(void);

void t3d_dyn_begin_tris(const T3DTexture *tex, float vert_scale,
                        bool blend, bool depth_write, bool cull_back, bool lit);
void t3d_dyn_tri_vc(const T3DBuildVert *v0, const T3DBuildVert *v1, const T3DBuildVert *v2);
void t3d_dyn_end_tris(void);

#endif
