#include "t3d_gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DYN_MAT_POOL_SIZE  96
#define DYN_VERT_PAIR_POOL 1024
#define MB_MAX_CHUNK_VERTS 68

typedef struct {
    uint32_t pair_offset;
    uint16_t vert_count;
    uint16_t tri_offset;
    uint16_t tri_count;
    const T3DTexture *tex;
    bool cull_back;
    bool lit;
    bool blend;
    bool depth_write;
} MBChunk;

typedef struct {
    uint8_t i0, i1, i2;
} MBTri;

struct T3DMeshBuilder {
    float vert_scale;
    const T3DTexture *cur_tex;
    bool cur_cull_back;
    bool cur_lit;
    bool cur_blend;
    bool cur_depth_write;
    float cur_nx, cur_ny, cur_nz;
    float cur_r, cur_g, cur_b, cur_a;

    T3DBuildVert *verts;
    uint32_t vert_count;
    uint32_t vert_cap;

    MBTri *tris;
    uint32_t tri_count;
    uint32_t tri_cap;

    MBChunk *chunks;
    uint32_t chunk_count;
    uint32_t chunk_cap;

    uint32_t chunk_vert_start;
    uint32_t chunk_tri_start;
};

static T3DMat4FP *s_room_mat_fp = NULL;
static T3DMat4FP *s_dyn_mats[T3D_FB_COUNT] = { NULL, NULL, NULL };
static T3DVertPacked *s_dyn_verts[T3D_FB_COUNT] = { NULL, NULL, NULL };
static int s_frame_idx = 0;
static uint32_t s_dyn_mat_used = 0;
static uint32_t s_dyn_vert_used = 0; // in vertices (2 per pair)

static T3DMat4 s_xf_stack[16];
static int s_xf_top = 0;

// Active dynamic batch state
static const T3DTexture *s_dyn_tex = NULL;
static float s_dyn_scale = T3D_ROOM_SCALE;
static bool s_dyn_is_quads = false;
static uint32_t s_dyn_batch_start_pair = 0;
static uint32_t s_dyn_batch_verts = 0;

static uint8_t f32_to_u8(float v) {
    int iv = (int)roundf(v * 255.0f);
    if (iv < 0) return 0;
    if (iv > 255) return 255;
    return (uint8_t)iv;
}

static uint32_t pack_rgba(float r, float g, float b, float a) {
    return ((uint32_t)f32_to_u8(r) << 24)
         | ((uint32_t)f32_to_u8(g) << 16)
         | ((uint32_t)f32_to_u8(b) << 8)
         | ((uint32_t)f32_to_u8(a));
}

static void pack_vert_slot(T3DVertPacked *dst, int slot,
                           const T3DBuildVert *v, float vert_scale,
                           const T3DTexture *tex) {
    int16_t px = (int16_t)roundf(v->x * vert_scale);
    int16_t py = (int16_t)roundf(v->y * vert_scale);
    int16_t pz = (int16_t)roundf(v->z * vert_scale);
    uint16_t norm = t3d_vert_pack_normal(&(T3DVec3){{ v->nx, v->ny, v->nz }});
    uint32_t rgba = pack_rgba(v->r, v->g, v->b, v->a);
    int16_t s = 0, t = 0;
    if (tex) {
        s = (int16_t)roundf(v->u * (float)tex->w * 32.0f);
        t = (int16_t)roundf(v->v * (float)tex->h * 32.0f);
    }

    if ((slot & 1) == 0) {
        dst->posA[0] = px; dst->posA[1] = py; dst->posA[2] = pz;
        dst->normA   = norm;
        dst->rgbaA   = rgba;
        dst->stA[0]  = s;  dst->stA[1]  = t;
    } else {
        dst->posB[0] = px; dst->posB[1] = py; dst->posB[2] = pz;
        dst->normB   = norm;
        dst->rgbaB   = rgba;
        dst->stB[0]  = s;  dst->stB[1]  = t;
    }
}

static void apply_rdp_draw_state(const T3DTexture *tex, bool cull_back, bool lit,
                                 bool blend, bool depth_write) {
    rdpq_mode_begin();
        rdpq_mode_combiner(tex ? RDPQ_COMBINER_TEX_SHADE : RDPQ_COMBINER_SHADE);
        rdpq_mode_blender(blend ? RDPQ_BLENDER_MULTIPLY : 0);
        rdpq_mode_zbuf(true, depth_write);
        rdpq_mode_antialias(blend ? AA_NONE : AA_STANDARD);
    rdpq_mode_end();

    if (tex) {
        t3d_tex_bind(tex);
    }

    enum T3DDrawFlags flags = T3D_FLAG_DEPTH | T3D_FLAG_SHADED;
    if (tex)       flags |= T3D_FLAG_TEXTURED;
    if (cull_back) flags |= T3D_FLAG_CULL_BACK;
    if (!lit)      flags |= T3D_FLAG_NO_LIGHT;
    t3d_state_set_drawflags(flags);
}

void t3d_gfx_init(void) {
    if (s_room_mat_fp) return;

    s_room_mat_fp = malloc_uncached(sizeof(T3DMat4FP));
    T3DMat4 room_m;
    t3d_mat4_identity(&room_m);
    float s = T3D_WORLD_SCALE / T3D_ROOM_SCALE;
    t3d_mat4_scale(&room_m, s, s, s);
    t3d_mat4_to_fixed(s_room_mat_fp, &room_m);

    for (int i = 0; i < T3D_FB_COUNT; i++) {
        s_dyn_mats[i]  = malloc_uncached(sizeof(T3DMat4FP) * DYN_MAT_POOL_SIZE);
        s_dyn_verts[i] = malloc_uncached(sizeof(T3DVertPacked) * DYN_VERT_PAIR_POOL);
    }
    t3d_xf_reset();
}

void t3d_gfx_frame_begin(int frame_idx) {
    s_frame_idx = frame_idx % T3D_FB_COUNT;
    s_dyn_mat_used = 0;
    s_dyn_vert_used = 0;
    t3d_xf_reset();
}

const T3DMat4FP *t3d_gfx_room_mat(void) {
    return s_room_mat_fp;
}

void t3d_tex_load(T3DTexture *out, const char *path, bool repeat) {
    sprite_t *sp = sprite_load(path);
    out->sprite = sp;
    out->w      = sp->width;
    out->h      = sp->height;
    out->repeat = repeat;
}

void t3d_tex_bind(const T3DTexture *tex) {
    if (!tex || !tex->sprite) return;
    rdpq_texparms_t parms = {
        .s = { .repeats = tex->repeat ? REPEAT_INFINITE : 1, .mirror = false },
        .t = { .repeats = tex->repeat ? REPEAT_INFINITE : 1, .mirror = false },
    };
    rdpq_sprite_upload(TILE0, tex->sprite, &parms);
}

// ---------------------------------------------------------
// Static Mesh Builder
// ---------------------------------------------------------
static void mb_flush_chunk(T3DMeshBuilder *b) {
    uint32_t vc = b->vert_count - b->chunk_vert_start;
    uint32_t tc = b->tri_count  - b->chunk_tri_start;
    if (vc == 0 || tc == 0) {
        b->vert_count = b->chunk_vert_start;
        b->tri_count  = b->chunk_tri_start;
        return;
    }

    if (b->chunk_count >= b->chunk_cap) {
        b->chunk_cap = b->chunk_cap ? b->chunk_cap * 2 : 16;
        b->chunks = realloc(b->chunks, sizeof(MBChunk) * b->chunk_cap);
    }

    MBChunk *c = &b->chunks[b->chunk_count++];
    c->pair_offset = b->chunk_vert_start; // temporarily store vertex index; patched in end
    c->vert_count  = (uint16_t)vc;
    c->tri_offset  = (uint16_t)b->chunk_tri_start;
    c->tri_count   = (uint16_t)tc;
    c->tex         = b->cur_tex;
    c->cull_back   = b->cur_cull_back;
    c->lit         = b->cur_lit;
    c->blend       = b->cur_blend;
    c->depth_write = b->cur_depth_write;

    b->chunk_vert_start = b->vert_count;
    b->chunk_tri_start  = b->tri_count;
}

static uint8_t mb_push_raw_vert(T3DMeshBuilder *b, const T3DBuildVert *v) {
    if (b->vert_count >= b->vert_cap) {
        b->vert_cap = b->vert_cap ? b->vert_cap * 2 : 128;
        b->verts = realloc(b->verts, sizeof(T3DBuildVert) * b->vert_cap);
    }
    uint32_t local_idx = b->vert_count - b->chunk_vert_start;
    b->verts[b->vert_count++] = *v;
    return (uint8_t)local_idx;
}

static void mb_push_raw_tri(T3DMeshBuilder *b, uint8_t i0, uint8_t i1, uint8_t i2) {
    if (b->tri_count >= b->tri_cap) {
        b->tri_cap = b->tri_cap ? b->tri_cap * 2 : 128;
        b->tris = realloc(b->tris, sizeof(MBTri) * b->tri_cap);
    }
    b->tris[b->tri_count++] = (MBTri){ i0, i1, i2 };
}

T3DMeshBuilder *t3d_mb_begin(float vert_scale) {
    T3DMeshBuilder *b = calloc(1, sizeof(T3DMeshBuilder));
    b->vert_scale      = vert_scale;
    b->cur_cull_back   = true;
    b->cur_lit         = true;
    b->cur_blend       = false;
    b->cur_depth_write = true;
    b->cur_nz          = 1.0f;
    b->cur_r = b->cur_g = b->cur_b = b->cur_a = 1.0f;
    return b;
}

void t3d_mb_set_texture(T3DMeshBuilder *b, const T3DTexture *tex) {
    if (b->cur_tex == tex) return;
    mb_flush_chunk(b);
    b->cur_tex = tex;
}

void t3d_mb_set_cull(T3DMeshBuilder *b, bool cull_back) {
    if (b->cur_cull_back == cull_back) return;
    mb_flush_chunk(b);
    b->cur_cull_back = cull_back;
}

void t3d_mb_set_lit(T3DMeshBuilder *b, bool lit) {
    if (b->cur_lit == lit) return;
    mb_flush_chunk(b);
    b->cur_lit = lit;
}

void t3d_mb_set_blend(T3DMeshBuilder *b, bool blend, bool depth_write) {
    if (b->cur_blend == blend && b->cur_depth_write == depth_write) return;
    mb_flush_chunk(b);
    b->cur_blend       = blend;
    b->cur_depth_write = depth_write;
}

void t3d_mb_set_color(T3DMeshBuilder *b, float r, float g, float b_c, float a) {
    b->cur_r = r;
    b->cur_g = g;
    b->cur_b = b_c;
    b->cur_a = a;
}

void t3d_mb_get_color(const T3DMeshBuilder *b, float *r, float *g, float *b_c, float *a) {
    if (r) *r = b->cur_r;
    if (g) *g = b->cur_g;
    if (b_c) *b_c = b->cur_b;
    if (a) *a = b->cur_a;
}

void t3d_mb_set_normal(T3DMeshBuilder *b, float nx, float ny, float nz) {
    b->cur_nx = nx;
    b->cur_ny = ny;
    b->cur_nz = nz;
}

void t3d_mb_push_tri_vc(T3DMeshBuilder *b,
                        const T3DBuildVert *v0,
                        const T3DBuildVert *v1,
                        const T3DBuildVert *v2) {
    if ((b->vert_count - b->chunk_vert_start) + 3 > MB_MAX_CHUNK_VERTS) {
        mb_flush_chunk(b);
    }
    uint8_t i0 = mb_push_raw_vert(b, v0);
    uint8_t i1 = mb_push_raw_vert(b, v1);
    uint8_t i2 = mb_push_raw_vert(b, v2);
    mb_push_raw_tri(b, i0, i1, i2);
}

void t3d_mb_push_quad_vc(T3DMeshBuilder *b,
                         const T3DBuildVert *v0,
                         const T3DBuildVert *v1,
                         const T3DBuildVert *v2,
                         const T3DBuildVert *v3) {
    if ((b->vert_count - b->chunk_vert_start) + 4 > MB_MAX_CHUNK_VERTS) {
        mb_flush_chunk(b);
    }
    uint8_t i0 = mb_push_raw_vert(b, v0);
    uint8_t i1 = mb_push_raw_vert(b, v1);
    uint8_t i2 = mb_push_raw_vert(b, v2);
    uint8_t i3 = mb_push_raw_vert(b, v3);
    mb_push_raw_tri(b, i0, i1, i2);
    mb_push_raw_tri(b, i0, i2, i3);
}

void t3d_mb_push_tri(T3DMeshBuilder *b,
                     float x0, float y0, float z0,
                     float x1, float y1, float z1,
                     float x2, float y2, float z2) {
    T3DBuildVert v0 = { x0, y0, z0, b->cur_nx, b->cur_ny, b->cur_nz, 0.0f, 0.0f, b->cur_r, b->cur_g, b->cur_b, b->cur_a };
    T3DBuildVert v1 = { x1, y1, z1, b->cur_nx, b->cur_ny, b->cur_nz, 0.0f, 0.0f, b->cur_r, b->cur_g, b->cur_b, b->cur_a };
    T3DBuildVert v2 = { x2, y2, z2, b->cur_nx, b->cur_ny, b->cur_nz, 0.0f, 0.0f, b->cur_r, b->cur_g, b->cur_b, b->cur_a };
    t3d_mb_push_tri_vc(b, &v0, &v1, &v2);
}

void t3d_mb_push_quad(T3DMeshBuilder *b,
                      float x0, float y0, float z0,
                      float x1, float y1, float z1,
                      float x2, float y2, float z2,
                      float x3, float y3, float z3) {
    T3DBuildVert v0 = { x0, y0, z0, b->cur_nx, b->cur_ny, b->cur_nz, 0.0f, 0.0f, b->cur_r, b->cur_g, b->cur_b, b->cur_a };
    T3DBuildVert v1 = { x1, y1, z1, b->cur_nx, b->cur_ny, b->cur_nz, 0.0f, 0.0f, b->cur_r, b->cur_g, b->cur_b, b->cur_a };
    T3DBuildVert v2 = { x2, y2, z2, b->cur_nx, b->cur_ny, b->cur_nz, 0.0f, 0.0f, b->cur_r, b->cur_g, b->cur_b, b->cur_a };
    T3DBuildVert v3 = { x3, y3, z3, b->cur_nx, b->cur_ny, b->cur_nz, 0.0f, 0.0f, b->cur_r, b->cur_g, b->cur_b, b->cur_a };
    t3d_mb_push_quad_vc(b, &v0, &v1, &v2, &v3);
}

T3DMesh t3d_mb_end(T3DMeshBuilder *b) {
    mb_flush_chunk(b);

    T3DMesh mesh = { NULL, NULL };
    if (b->chunk_count == 0) {
        free(b->verts);
        free(b->tris);
        free(b->chunks);
        free(b);
        return mesh;
    }

    uint32_t total_pairs = 0;
    for (uint32_t i = 0; i < b->chunk_count; i++) {
        uint32_t pairs = (b->chunks[i].vert_count + 1) / 2;
        total_pairs += pairs;
    }

    mesh.verts = malloc_uncached(sizeof(T3DVertPacked) * total_pairs);
    memset(mesh.verts, 0, sizeof(T3DVertPacked) * total_pairs);

    uint32_t pair_cursor = 0;
    for (uint32_t c = 0; c < b->chunk_count; c++) {
        MBChunk *ch = &b->chunks[c];
        uint32_t src_start = ch->pair_offset;
        ch->pair_offset = pair_cursor;

        for (uint32_t v = 0; v < ch->vert_count; v++) {
            pack_vert_slot(&mesh.verts[pair_cursor + (v / 2)], (int)(v & 1),
                           &b->verts[src_start + v], b->vert_scale, ch->tex);
        }
        pair_cursor += (ch->vert_count + 1) / 2;
    }

    rspq_block_begin();
    const T3DTexture *last_tex = (const T3DTexture *)(uintptr_t)0x1; // sentinel
    bool last_cull = !b->chunks[0].cull_back;
    bool last_lit  = !b->chunks[0].lit;
    bool last_blend = !b->chunks[0].blend;
    bool last_depth = !b->chunks[0].depth_write;

    for (uint32_t c = 0; c < b->chunk_count; c++) {
        const MBChunk *ch = &b->chunks[c];
        if (c == 0 ||
            ch->tex != last_tex ||
            ch->cull_back != last_cull ||
            ch->lit != last_lit ||
            ch->blend != last_blend ||
            ch->depth_write != last_depth) {
            apply_rdp_draw_state(ch->tex, ch->cull_back, ch->lit, ch->blend, ch->depth_write);
            last_tex   = ch->tex;
            last_cull  = ch->cull_back;
            last_lit   = ch->lit;
            last_blend = ch->blend;
            last_depth = ch->depth_write;
        }

        t3d_vert_load(&mesh.verts[ch->pair_offset], 0, ch->vert_count);
        for (uint32_t t = 0; t < ch->tri_count; t++) {
            const MBTri *tri = &b->tris[ch->tri_offset + t];
            t3d_tri_draw(tri->i0, tri->i1, tri->i2);
        }
        t3d_tri_sync();
    }
    mesh.block = rspq_block_end();

    free(b->verts);
    free(b->tris);
    free(b->chunks);
    free(b);
    return mesh;
}

void t3d_mesh_draw(const T3DMesh *m) {
    if (m && m->block) {
        rspq_block_run(m->block);
    }
}

// ---------------------------------------------------------
// Hierarchical CPU Matrix Stack
// ---------------------------------------------------------
static void mat4_mul_rhs(T3DMat4 *cur, const T3DMat4 *rhs) {
    T3DMat4 tmp;
    t3d_mat4_mul(&tmp, cur, rhs);
    *cur = tmp;
}

void t3d_xf_reset(void) {
    s_xf_top = 0;
    t3d_mat4_identity(&s_xf_stack[0]);
    t3d_mat4_scale(&s_xf_stack[0], T3D_WORLD_SCALE, T3D_WORLD_SCALE, T3D_WORLD_SCALE);
}

void t3d_xf_push(void) {
    if (s_xf_top < 15) {
        s_xf_stack[s_xf_top + 1] = s_xf_stack[s_xf_top];
        s_xf_top++;
    }
}

void t3d_xf_pop(void) {
    if (s_xf_top > 0) {
        s_xf_top--;
    }
}

void t3d_xf_translate(float tx, float ty, float tz) {
    T3DMat4 m;
    t3d_mat4_identity(&m);
    m.m[3][0] = tx;
    m.m[3][1] = ty;
    m.m[3][2] = tz;
    mat4_mul_rhs(&s_xf_stack[s_xf_top], &m);
}

void t3d_xf_rotate_x(float deg) {
    float rad = T3D_DEG_TO_RAD(deg);
    float c = cosf(rad), s = sinf(rad);
    T3DMat4 m;
    t3d_mat4_identity(&m);
    m.m[1][1] =  c; m.m[1][2] = s;
    m.m[2][1] = -s; m.m[2][2] = c;
    mat4_mul_rhs(&s_xf_stack[s_xf_top], &m);
}

void t3d_xf_rotate_y(float deg) {
    float rad = T3D_DEG_TO_RAD(deg);
    float c = cosf(rad), s = sinf(rad);
    T3DMat4 m;
    t3d_mat4_identity(&m);
    m.m[0][0] = c; m.m[0][2] = -s;
    m.m[2][0] = s; m.m[2][2] =  c;
    mat4_mul_rhs(&s_xf_stack[s_xf_top], &m);
}

void t3d_xf_rotate_z(float deg) {
    float rad = T3D_DEG_TO_RAD(deg);
    float c = cosf(rad), s = sinf(rad);
    T3DMat4 m;
    t3d_mat4_identity(&m);
    m.m[0][0] =  c; m.m[0][1] = s;
    m.m[1][0] = -s; m.m[1][1] = c;
    mat4_mul_rhs(&s_xf_stack[s_xf_top], &m);
}

void t3d_xf_scale(float sx, float sy, float sz) {
    T3DMat4 m;
    t3d_mat4_identity(&m);
    m.m[0][0] = sx;
    m.m[1][1] = sy;
    m.m[2][2] = sz;
    mat4_mul_rhs(&s_xf_stack[s_xf_top], &m);
}

void t3d_xf_apply(float mesh_scale) {
    if (s_dyn_mat_used >= DYN_MAT_POOL_SIZE) return;
    T3DMat4 final_m = s_xf_stack[s_xf_top];
    float inv = 1.0f / mesh_scale;
    t3d_mat4_scale(&final_m, inv, inv, inv);
    T3DMat4FP *fp = &s_dyn_mats[s_frame_idx][s_dyn_mat_used++];
    t3d_mat4_to_fixed_3x4(fp, &final_m);
    t3d_matrix_set(fp, true);
}

// ---------------------------------------------------------
// Dynamic Per-Frame Vertex Batcher
// ---------------------------------------------------------
static void dyn_flush(void) {
    if (s_dyn_batch_verts == 0) return;

    uint32_t pairs = (s_dyn_batch_verts + 1) / 2;
    T3DVertPacked *vptr = &s_dyn_verts[s_frame_idx][s_dyn_batch_start_pair];
    t3d_vert_load(vptr, 0, s_dyn_batch_verts);

    if (s_dyn_is_quads) {
        t3d_quad_draw_unindexed(0, s_dyn_batch_verts / 4);
    } else {
        t3d_tri_draw_unindexed(0, s_dyn_batch_verts / 3);
    }

    s_dyn_vert_used = s_dyn_batch_start_pair + pairs;
    s_dyn_batch_start_pair = s_dyn_vert_used;
    s_dyn_batch_verts = 0;
}

void t3d_dyn_begin_quads(const T3DTexture *tex, float vert_scale,
                         bool blend, bool depth_write, bool cull_back, bool lit) {
    apply_rdp_draw_state(tex, cull_back, lit, blend, depth_write);
    s_dyn_tex              = tex;
    s_dyn_scale            = vert_scale;
    s_dyn_is_quads         = true;
    s_dyn_batch_start_pair = s_dyn_vert_used;
    s_dyn_batch_verts      = 0;
}

void t3d_dyn_quad_vc(const T3DBuildVert *v0, const T3DBuildVert *v1,
                     const T3DBuildVert *v2, const T3DBuildVert *v3) {
    if (s_dyn_batch_verts + 4 > MB_MAX_CHUNK_VERTS) {
        dyn_flush();
    }
    if (s_dyn_batch_start_pair + (s_dyn_batch_verts + 4) / 2 > DYN_VERT_PAIR_POOL) {
        return;
    }

    // Order [v0(BL), v1(BR), v3(TL), v2(TR)]: T3DCmd_TriDraw_Seq passes (a0=b+2, a1=b+1, a2=b+0)
    // to RDPQ_Triangle_Send_Async, corresponding to t3d_tri_draw(b+2, b+0, b+1) = (TL, BL, BR) CCW
    // in Pass 1 and (TR, BR, TL) CW (passing flipped cull) in Pass 2 along the (BR, TL) diagonal.
    T3DVertPacked *pool = &s_dyn_verts[s_frame_idx][s_dyn_batch_start_pair];
    uint32_t idx = s_dyn_batch_verts;
    pack_vert_slot(&pool[(idx + 0) / 2], (int)((idx + 0) & 1), v0, s_dyn_scale, s_dyn_tex);
    pack_vert_slot(&pool[(idx + 1) / 2], (int)((idx + 1) & 1), v1, s_dyn_scale, s_dyn_tex);
    pack_vert_slot(&pool[(idx + 2) / 2], (int)((idx + 2) & 1), v3, s_dyn_scale, s_dyn_tex);
    pack_vert_slot(&pool[(idx + 3) / 2], (int)((idx + 3) & 1), v2, s_dyn_scale, s_dyn_tex);
    s_dyn_batch_verts += 4;
}

void t3d_dyn_end_quads(void) {
    dyn_flush();
}

void t3d_dyn_begin_tris(const T3DTexture *tex, float vert_scale,
                        bool blend, bool depth_write, bool cull_back, bool lit) {
    apply_rdp_draw_state(tex, cull_back, lit, blend, depth_write);
    s_dyn_tex              = tex;
    s_dyn_scale            = vert_scale;
    s_dyn_is_quads         = false;
    s_dyn_batch_start_pair = s_dyn_vert_used;
    s_dyn_batch_verts      = 0;
}

void t3d_dyn_tri_vc(const T3DBuildVert *v0, const T3DBuildVert *v1, const T3DBuildVert *v2) {
    if (s_dyn_batch_verts + 3 > 66) {
        dyn_flush();
    }
    if (s_dyn_batch_start_pair + (s_dyn_batch_verts + 4) / 2 > DYN_VERT_PAIR_POOL) {
        return;
    }

    T3DVertPacked *pool = &s_dyn_verts[s_frame_idx][s_dyn_batch_start_pair];
    uint32_t idx = s_dyn_batch_verts;
    pack_vert_slot(&pool[(idx + 0) / 2], (int)((idx + 0) & 1), v0, s_dyn_scale, s_dyn_tex);
    pack_vert_slot(&pool[(idx + 1) / 2], (int)((idx + 1) & 1), v1, s_dyn_scale, s_dyn_tex);
    pack_vert_slot(&pool[(idx + 2) / 2], (int)((idx + 2) & 1), v2, s_dyn_scale, s_dyn_tex);
    s_dyn_batch_verts += 3;
}

void t3d_dyn_end_tris(void) {
    dyn_flush();
}
