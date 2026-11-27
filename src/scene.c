#include "scene.h"
#include <libdragon.h>
#include <math.h>

static bool  s_bake_active = false;
static float s_base_r = 1.0f, s_base_g = 1.0f, s_base_b = 1.0f;

static void set_base_color(T3DMeshBuilder *b, float r, float g, float b_c) {
    s_base_r = r;
    s_base_g = g;
    s_base_b = b_c;
    t3d_mb_set_color(b, r, g, b_c, 1.0f);
}

static float bake_fire_falloff(float px, float py, float pz, float nx, float ny, float nz) {
    float dx = 0.0f   - px;
    float dy = 0.78f  - py;
    float dz = -0.16f - pz;
    float d2 = dx * dx + dy * dy + dz * dz;
    float d  = sqrtf(d2 + 1e-6f);
    float ndotl = (nx * dx + ny * dy + nz * dz) / d;
    if (ndotl < 0.0f) ndotl = 0.0f;
    float att = 1.0f / (0.58f + 0.20f * d + 0.09f * d2);
    float l1 = nx * (-0.249f) + ny * 0.549f + nz * 0.798f;
    if (l1 < 0.0f) l1 = 0.0f;
    float shade = 0.16f + 0.24f * l1 + att * (0.20f + 0.52f * ndotl);
    return fminf(1.0f, shade);
}

static void baked_rgb(float px, float py, float pz, float nx, float ny, float nz,
                      float *or_r, float *or_g, float *or_b) {
    if (!s_bake_active) {
        *or_r = s_base_r;
        *or_g = s_base_g;
        *or_b = s_base_b;
        return;
    }
    float b = bake_fire_falloff(px, py, pz, nx, ny, nz);
    *or_r = fminf(1.0f, s_base_r * b);
    *or_g = fminf(1.0f, s_base_g * b);
    *or_b = fminf(1.0f, s_base_b * b);
}

static uint8_t f_to_u8(float v) {
    int iv = (int)roundf(v * 255.0f);
    if (iv < 0) return 0;
    if (iv > 255) return 255;
    return (uint8_t)iv;
}

static void build_base_scene_mesh(T3DMeshBuilder *b, const SceneTextures *tex);
static void build_spring_mesh(T3DMeshBuilder *b);
static void build_summer_mesh(T3DMeshBuilder *b);
static void build_thanksgiving_mesh(T3DMeshBuilder *b);
static void build_christmas_mesh(T3DMeshBuilder *b);
static void build_snowglobe_opaque_mesh(T3DMeshBuilder *b, Season season);
static void build_snowglobe_dome_mesh(T3DMeshBuilder *b);
static void build_fire_logs_mesh(T3DMeshBuilder *b, const SceneTextures *tex);
static void draw_flower_pot(T3DMeshBuilder *b, float cx, float cy, float cz,
                            float scale, float fl_r, float fl_g, float fl_b);
static void draw_camp_lantern(T3DMeshBuilder *b, float cx, float cy, float cz);
static void draw_gift_box(T3DMeshBuilder *b,
                          float cx, float cy, float cz,
                          float w, float h, float d,
                          float br, float bg, float bb,
                          float rr, float rg, float rb);
static void draw_milk_and_cookies(T3DMeshBuilder *b, float cx, float cy, float cz);
static void draw_pumpkin(T3DMeshBuilder *b, float cx, float cy, float cz,
                         float rx, float ry, float r, float g, float b_c);
static void draw_cone_6(T3DMeshBuilder *b,
                        float cx, float y0, float y1, float cz,
                        float r_bot, float r_top, bool cap_bottom);
static void draw_pumpkin_pie(T3DMeshBuilder *b, float cx, float cy, float cz, float r);
static void draw_berry_tart(T3DMeshBuilder *b, float cx, float cy, float cz, float r);
static void draw_smores_platter(T3DMeshBuilder *b, float cx, float cy, float cz);

void scene_load_textures(SceneTextures *tex) {
    t3d_tex_load(&tex->stone,           "rom:/stone.sprite",           true);
    t3d_tex_load(&tex->brick,           "rom:/brick.sprite",           true);
    t3d_tex_load(&tex->wood_mantle,     "rom:/wood_mantle.sprite",     true);
    t3d_tex_load(&tex->floor,           "rom:/floor.sprite",           true);
    t3d_tex_load(&tex->bark,            "rom:/bark.sprite",            true);
    t3d_tex_load(&tex->log_end,         "rom:/log_end.sprite",         false);
    t3d_tex_load(&tex->coals,           "rom:/coals.sprite",           true);
    t3d_tex_load(&tex->rug,             "rom:/rug.sprite",             false);
    t3d_tex_load(&tex->iron,            "rom:/iron.sprite",            true);
    t3d_tex_load(&tex->painting,        "rom:/painting.sprite",        false);
    t3d_tex_load(&tex->painting_spring, "rom:/painting_spring.sprite", false);
    t3d_tex_load(&tex->painting_summer, "rom:/painting_summer.sprite", false);
    t3d_tex_load(&tex->painting_xmas,   "rom:/painting_xmas.sprite",   false);
    t3d_tex_load(&tex->flame,           "rom:/flame.sprite",           false);
    t3d_tex_load(&tex->glow,            "rom:/glow.sprite",            false);
    t3d_tex_load(&tex->spark,           "rom:/spark.sprite",           false);

    T3DMeshBuilder *b;

    b = t3d_mb_begin(T3D_ROOM_SCALE);
    build_base_scene_mesh(b, tex);
    tex->base_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_ROOM_SCALE);
    build_spring_mesh(b);
    tex->spring_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_ROOM_SCALE);
    build_summer_mesh(b);
    tex->summer_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_ROOM_SCALE);
    build_thanksgiving_mesh(b);
    tex->thanksgiving_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_ROOM_SCALE);
    build_christmas_mesh(b);
    tex->christmas_mesh = t3d_mb_end(b);

    for (int s = 0; s < SEASON_COUNT; s++) {
        b = t3d_mb_begin(T3D_PROP_SCALE);
        build_snowglobe_opaque_mesh(b, (Season)s);
        tex->snowglobe_mesh[s] = t3d_mb_end(b);
    }

    b = t3d_mb_begin(T3D_PROP_SCALE);
    build_snowglobe_dome_mesh(b);
    tex->snowglobe_dome_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_ROOM_SCALE);
    build_fire_logs_mesh(b, tex);
    tex->fire_logs_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_gift_box(b, -1.46f, 0.18f, 0.38f, 0.24f, 0.20f, 0.22f,
                  0.84f, 0.12f, 0.14f, 0.96f, 0.82f, 0.22f);
    tex->left_gift_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_milk_and_cookies(b, -1.46f, 0.18f, 0.38f);
    tex->left_cookies_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_pumpkin(b, -1.46f, 0.18f, 0.34f, 0.145f, 0.115f, 0.92f, 0.42f, 0.08f);
    tex->left_pumpkin_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_pumpkin_pie(b, -1.46f, 0.18f, 0.34f, 0.175f);
    tex->left_pie_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_flower_pot(b, -1.46f, 0.18f, 0.36f, 1.15f, 0.96f, 0.44f, 0.66f);
    tex->left_spring_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_camp_lantern(b, -1.46f, 0.18f, 0.36f);
    tex->left_summer_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_berry_tart(b, -1.46f, 0.18f, 0.36f, 0.170f);
    tex->left_berry_tart_mesh = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_texture(b, NULL);
    draw_smores_platter(b, -1.46f, 0.18f, 0.36f);
    tex->left_smores_mesh = t3d_mb_end(b);
}

static void draw_grid_quad(T3DMeshBuilder *b,
                           float x0, float y0, float z0,
                           float ux, float uy, float uz,
                           float vx, float vy, float vz,
                           float nx, float ny, float nz,
                           float u_max, float v_max,
                           int seg_u, int seg_v) {
    for (int j = 0; j < seg_v; j++) {
        float tv0 = (float)j / (float)seg_v;
        float tv1 = (float)(j + 1) / (float)seg_v;
        for (int i = 0; i < seg_u; i++) {
            float tu0 = (float)i / (float)seg_u;
            float tu1 = (float)(i + 1) / (float)seg_u;

            float p00x = x0 + ux * tu0 + vx * tv0;
            float p00y = y0 + uy * tu0 + vy * tv0;
            float p00z = z0 + uz * tu0 + vz * tv0;

            float p10x = x0 + ux * tu1 + vx * tv0;
            float p10y = y0 + uy * tu1 + vy * tv0;
            float p10z = z0 + uz * tu1 + vz * tv0;

            float p11x = x0 + ux * tu1 + vx * tv1;
            float p11y = y0 + uy * tu1 + vy * tv1;
            float p11z = z0 + uz * tu1 + vz * tv1;

            float p01x = x0 + ux * tu0 + vx * tv1;
            float p01y = y0 + uy * tu0 + vy * tv1;
            float p01z = z0 + uz * tu0 + vz * tv1;

            float r00, g00, b00, r10, g10, b10, r11, g11, b11, r01, g01, b01;
            baked_rgb(p00x, p00y, p00z, nx, ny, nz, &r00, &g00, &b00);
            baked_rgb(p10x, p10y, p10z, nx, ny, nz, &r10, &g10, &b10);
            baked_rgb(p11x, p11y, p11z, nx, ny, nz, &r11, &g11, &b11);
            baked_rgb(p01x, p01y, p01z, nx, ny, nz, &r01, &g01, &b01);

            T3DBuildVert q0 = { p00x, p00y, p00z, nx, ny, nz, tu0 * u_max, (1.0f - tv0) * v_max, r00, g00, b00, 1.0f };
            T3DBuildVert q1 = { p10x, p10y, p10z, nx, ny, nz, tu1 * u_max, (1.0f - tv0) * v_max, r10, g10, b10, 1.0f };
            T3DBuildVert q2 = { p11x, p11y, p11z, nx, ny, nz, tu1 * u_max, (1.0f - tv1) * v_max, r11, g11, b11, 1.0f };
            T3DBuildVert q3 = { p01x, p01y, p01z, nx, ny, nz, tu0 * u_max, (1.0f - tv1) * v_max, r01, g01, b01, 1.0f };
            t3d_mb_push_quad_vc(b, &q0, &q1, &q2, &q3);
        }
    }
}

static void emit_solid_box(T3DMeshBuilder *b,
                           float x0, float y0, float z0,
                           float x1, float y1, float z1,
                           bool include_bottom) {
    t3d_mb_set_normal(b, 0.0f, 0.0f, 1.0f);
    t3d_mb_push_quad(b, x0, y0, z1,  x1, y0, z1,  x1, y1, z1,  x0, y1, z1);
    t3d_mb_set_normal(b, -1.0f, 0.0f, 0.0f);
    t3d_mb_push_quad(b, x0, y0, z0,  x0, y0, z1,  x0, y1, z1,  x0, y1, z0);
    t3d_mb_set_normal(b, 1.0f, 0.0f, 0.0f);
    t3d_mb_push_quad(b, x1, y0, z1,  x1, y0, z0,  x1, y1, z0,  x1, y1, z1);
    t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
    t3d_mb_push_quad(b, x0, y1, z1,  x1, y1, z1,  x1, y1, z0,  x0, y1, z0);
    if (include_bottom) {
        t3d_mb_set_normal(b, 0.0f, -1.0f, 0.0f);
        t3d_mb_push_quad(b, x0, y0, z0,  x1, y0, z0,  x1, y0, z1,  x0, y0, z1);
    }
}

static void emit_front_quad(T3DMeshBuilder *b, float x0, float y0, float x1, float y1, float z) {
    t3d_mb_set_normal(b, 0.0f, 0.0f, 1.0f);
    t3d_mb_push_quad(b, x0, y0, z,  x1, y0, z,  x1, y1, z,  x0, y1, z);
}

static void draw_box(T3DMeshBuilder *b,
                     float x0, float y0, float z0,
                     float x1, float y1, float z1,
                     float uv_scale, int subdiv) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float dz = z1 - z0;
    int su = (subdiv <= 1) ? 1 : subdiv;
    int sv = (subdiv <= 1) ? 1 : subdiv;

    draw_grid_quad(b, x0, y0, z1,  dx, 0, 0,   0, dy, 0,   0, 0, 1,  dx * uv_scale, dy * uv_scale, su, sv);
    draw_grid_quad(b, x0, y0, z0,  0, 0, dz,   0, dy, 0,  -1, 0, 0,  dz * uv_scale, dy * uv_scale,  1, sv);
    draw_grid_quad(b, x1, y0, z1,  0, 0, -dz,  0, dy, 0,   1, 0, 0,  dz * uv_scale, dy * uv_scale,  1, sv);
    draw_grid_quad(b, x0, y1, z1,  dx, 0, 0,   0, 0, -dz,  0, 1, 0,  dx * uv_scale, dz * uv_scale, su,  1);
    if (y0 > 0.01f) {
        draw_grid_quad(b, x0, y0, z0,  dx, 0, 0,  0, 0, dz,  0, -1, 0,  dx * uv_scale, dz * uv_scale, 1, 1);
    }
}

static const float LOG_SIN[7] = { 0.0f, 0.8660254f,  0.8660254f,  0.0f, -0.8660254f, -0.8660254f, 0.0f };
static const float LOG_COS[7] = { 1.0f, 0.5000000f, -0.5000000f, -1.0f, -0.5000000f,  0.5000000f, 1.0f };

static void xform_log_pt(float lx, float ly, float lz,
                         float cx, float cy, float cz,
                         float cos_y, float sin_y, float cos_z, float sin_z,
                         float *ox, float *oy, float *oz) {
    float x1 = lx * cos_z - ly * sin_z;
    float y1 = lx * sin_z + ly * cos_z;
    *ox = cx + x1 * cos_y + lz * sin_y;
    *oy = cy + y1;
    *oz = cz - x1 * sin_y + lz * cos_y;
}

static void emit_static_log_to_builder(T3DMeshBuilder *b, const SceneTextures *tex,
                                       float cx, float cy, float cz,
                                       float length, float radius,
                                       float rot_y, float rot_z,
                                       float glow_amt, const FlameTheme *th) {
    const int sides = 6;
    const float deg2rad = 0.0174532925f;
    float half_l = length * 0.5f;
    float bs = s_bake_active ? bake_fire_falloff(cx, cy, cz, 0.0f, 0.5f, 0.866f) : 1.0f;
    float cos_y = cosf(rot_y * deg2rad), sin_y = sinf(rot_y * deg2rad);
    float cos_z = cosf(rot_z * deg2rad), sin_z = sinf(rot_z * deg2rad);

    t3d_mb_set_texture(b, &tex->bark);
    for (int i = 0; i < sides; i++) {
        float ny0 = LOG_SIN[i],     nz0 = LOG_COS[i];
        float ny1 = LOG_SIN[i + 1], nz1 = LOG_COS[i + 1];
        float y0 = ny0 * radius, z0 = nz0 * radius;
        float y1 = ny1 * radius, z1 = nz1 * radius;

        float g0 = (ny0 < 0.1f) ? glow_amt * (0.25f - ny0 * 0.75f) : 0.0f;
        float g1 = (ny1 < 0.1f) ? glow_amt * (0.25f - ny1 * 0.75f) : 0.0f;
        float v0 = (float)i * (2.0f / 6.0f);
        float v1 = (float)(i + 1) * (2.0f / 6.0f);

        float wnx0, wny0, wnz0, wnx1, wny1, wnz1;
        xform_log_pt(0.0f, ny0, nz0, 0, 0, 0, cos_y, sin_y, cos_z, sin_z, &wnx0, &wny0, &wnz0);
        xform_log_pt(0.0f, ny1, nz1, 0, 0, 0, cos_y, sin_y, cos_z, sin_z, &wnx1, &wny1, &wnz1);

        float r0 = fminf(1.0f, (0.76f + th->coal_r * g0) * bs);
        float g_0 = fminf(1.0f, (0.68f + th->coal_g * g0) * bs);
        float b0 = fminf(1.0f, (0.62f + th->coal_b * g0) * bs);

        float r1 = fminf(1.0f, (0.76f + th->coal_r * g1) * bs);
        float g_1 = fminf(1.0f, (0.68f + th->coal_g * g1) * bs);
        float b1 = fminf(1.0f, (0.62f + th->coal_b * g1) * bs);

        float p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z;
        xform_log_pt(-half_l, y0, z0, cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p0x, &p0y, &p0z);
        xform_log_pt( half_l, y0, z0, cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p1x, &p1y, &p1z);
        xform_log_pt( half_l, y1, z1, cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p2x, &p2y, &p2z);
        xform_log_pt(-half_l, y1, z1, cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p3x, &p3y, &p3z);

        T3DBuildVert q0 = { p0x, p0y, p0z, wnx0, wny0, wnz0, 0.0f, v0, r0, g_0, b0, 1.0f };
        T3DBuildVert q1 = { p1x, p1y, p1z, wnx0, wny0, wnz0, 2.0f, v0, r0, g_0, b0, 1.0f };
        T3DBuildVert q2 = { p2x, p2y, p2z, wnx1, wny1, wnz1, 2.0f, v1, r1, g_1, b1, 1.0f };
        T3DBuildVert q3 = { p3x, p3y, p3z, wnx1, wny1, wnz1, 0.0f, v1, r1, g_1, b1, 1.0f };
        t3d_mb_push_quad_vc(b, &q0, &q1, &q2, &q3);
    }

    t3d_mb_set_texture(b, &tex->log_end);
    float er = 0.90f * bs, eg = 0.84f * bs, eb = 0.78f * bs;
    float npx, npy, npz, nmx, nmy, nmz;
    xform_log_pt( 1.0f, 0.0f, 0.0f, 0, 0, 0, cos_y, sin_y, cos_z, sin_z, &npx, &npy, &npz);
    xform_log_pt(-1.0f, 0.0f, 0.0f, 0, 0, 0, cos_y, sin_y, cos_z, sin_z, &nmx, &nmy, &nmz);

    float pcx, pcy, pcz, mcx, mcy, mcz;
    xform_log_pt( half_l, 0.0f, 0.0f, cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &pcx, &pcy, &pcz);
    xform_log_pt(-half_l, 0.0f, 0.0f, cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &mcx, &mcy, &mcz);

    for (int i = sides; i > 0; i--) {
        float p1x, p1y, p1z, p2x, p2y, p2z;
        xform_log_pt(half_l, LOG_SIN[i] * radius, LOG_COS[i] * radius,
                     cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p1x, &p1y, &p1z);
        xform_log_pt(half_l, LOG_SIN[i - 1] * radius, LOG_COS[i - 1] * radius,
                     cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p2x, &p2y, &p2z);
        T3DBuildVert v0 = { pcx, pcy, pcz, npx, npy, npz, 0.5f, 0.5f, er, eg, eb, 1.0f };
        T3DBuildVert v1 = { p1x, p1y, p1z, npx, npy, npz, 0.5f + 0.48f * LOG_COS[i],     0.5f - 0.48f * LOG_SIN[i],     er, eg, eb, 1.0f };
        T3DBuildVert v2 = { p2x, p2y, p2z, npx, npy, npz, 0.5f + 0.48f * LOG_COS[i - 1], 0.5f - 0.48f * LOG_SIN[i - 1], er, eg, eb, 1.0f };
        t3d_mb_push_tri_vc(b, &v0, &v1, &v2);
    }
    for (int i = 0; i < sides; i++) {
        float p1x, p1y, p1z, p2x, p2y, p2z;
        xform_log_pt(-half_l, LOG_SIN[i] * radius, LOG_COS[i] * radius,
                     cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p1x, &p1y, &p1z);
        xform_log_pt(-half_l, LOG_SIN[i + 1] * radius, LOG_COS[i + 1] * radius,
                     cx, cy, cz, cos_y, sin_y, cos_z, sin_z, &p2x, &p2y, &p2z);
        T3DBuildVert v0 = { mcx, mcy, mcz, nmx, nmy, nmz, 0.5f, 0.5f, er, eg, eb, 1.0f };
        T3DBuildVert v1 = { p1x, p1y, p1z, nmx, nmy, nmz, 0.5f + 0.48f * LOG_COS[i],     0.5f - 0.48f * LOG_SIN[i],     er, eg, eb, 1.0f };
        T3DBuildVert v2 = { p2x, p2y, p2z, nmx, nmy, nmz, 0.5f + 0.48f * LOG_COS[i + 1], 0.5f - 0.48f * LOG_SIN[i + 1], er, eg, eb, 1.0f };
        t3d_mb_push_tri_vc(b, &v0, &v1, &v2);
    }
}

static void build_fire_logs_mesh(T3DMeshBuilder *b, const SceneTextures *tex) {
    typedef struct {
        float cx, cy, cz, length, radius, rot_y, rot_z, glow_amt;
    } StaticLogSpec;

    static const StaticLogSpec logs[5] = {
        { -0.04f, 0.41f, -0.34f, 0.96f, 0.095f,   7.0f,  2.0f, 0.72f },
        {  0.03f, 0.42f, -0.56f, 0.92f, 0.092f,  -9.0f, -2.0f, 0.72f },
        { -0.16f, 0.54f, -0.44f, 0.78f, 0.082f,  28.0f, 14.0f, 0.60f },
        {  0.18f, 0.53f, -0.46f, 0.76f, 0.080f, -32.0f,-13.0f, 0.60f },
        {  0.01f, 0.63f, -0.45f, 0.72f, 0.074f,  11.0f,  5.0f, 0.50f },
    };
    const FlameTheme *neutral_th = &FLAME_THEMES[0];
    for (int k = 0; k < 5; k++) {
        emit_static_log_to_builder(b, tex,
                                   logs[k].cx, logs[k].cy, logs[k].cz,
                                   logs[k].length, logs[k].radius,
                                   logs[k].rot_y, logs[k].rot_z,
                                   logs[k].glow_amt, neutral_th);
    }
}

static void draw_dynamic_log(const SceneTextures *tex,
                             float cx, float cy, float cz,
                             float length, float radius,
                             float rot_y, float rot_z,
                             float glow_amt, const FlameTheme *th) {
    const int sides = 6;
    float half_l = length * 0.5f;

    t3d_matrix_push_pos(1);
    t3d_xf_reset();
    t3d_xf_translate(cx, cy, cz);
    t3d_xf_rotate_y(rot_y);
    t3d_xf_rotate_z(rot_z);
    t3d_xf_apply(T3D_ROOM_SCALE);

    t3d_dyn_begin_quads(&tex->bark, T3D_ROOM_SCALE, false, true, true, true);
    for (int i = 0; i < sides; i++) {
        float ny0 = LOG_SIN[i],     nz0 = LOG_COS[i];
        float ny1 = LOG_SIN[i + 1], nz1 = LOG_COS[i + 1];
        float y0 = ny0 * radius, z0 = nz0 * radius;
        float y1 = ny1 * radius, z1 = nz1 * radius;

        float g0 = (ny0 < 0.1f) ? glow_amt * (0.25f - ny0 * 0.75f) : 0.0f;
        float g1 = (ny1 < 0.1f) ? glow_amt * (0.25f - ny1 * 0.75f) : 0.0f;
        float v0 = (float)i * (2.0f / 6.0f);
        float v1 = (float)(i + 1) * (2.0f / 6.0f);

        float r0 = fminf(1.0f, 0.76f + th->coal_r * g0);
        float g_0 = fminf(1.0f, 0.68f + th->coal_g * g0);
        float b0 = fminf(1.0f, 0.62f + th->coal_b * g0);

        float r1 = fminf(1.0f, 0.76f + th->coal_r * g1);
        float g_1 = fminf(1.0f, 0.68f + th->coal_g * g1);
        float b1 = fminf(1.0f, 0.62f + th->coal_b * g1);

        T3DBuildVert q0 = { -half_l, y0, z0,  0, ny0, nz0,  0.0f, v0,  r0, g_0, b0, 1.0f };
        T3DBuildVert q1 = {  half_l, y0, z0,  0, ny0, nz0,  2.0f, v0,  r0, g_0, b0, 1.0f };
        T3DBuildVert q2 = {  half_l, y1, z1,  0, ny1, nz1,  2.0f, v1,  r1, g_1, b1, 1.0f };
        T3DBuildVert q3 = { -half_l, y1, z1,  0, ny1, nz1,  0.0f, v1,  r1, g_1, b1, 1.0f };
        t3d_dyn_quad_vc(&q0, &q1, &q2, &q3);
    }
    t3d_dyn_end_quads();

    t3d_dyn_begin_tris(&tex->log_end, T3D_ROOM_SCALE, false, true, true, true);
    for (int i = sides; i > 0; i--) {
        T3DBuildVert t0 = { half_l, 0.0f, 0.0f, 1, 0, 0, 0.5f, 0.5f, 0.90f, 0.84f, 0.78f, 1.0f };
        T3DBuildVert t1 = { half_l, LOG_SIN[i] * radius,     LOG_COS[i] * radius,     1, 0, 0, 0.5f + 0.48f * LOG_COS[i],     0.5f - 0.48f * LOG_SIN[i],     0.90f, 0.84f, 0.78f, 1.0f };
        T3DBuildVert t2 = { half_l, LOG_SIN[i - 1] * radius, LOG_COS[i - 1] * radius, 1, 0, 0, 0.5f + 0.48f * LOG_COS[i - 1], 0.5f - 0.48f * LOG_SIN[i - 1], 0.90f, 0.84f, 0.78f, 1.0f };
        t3d_dyn_tri_vc(&t0, &t1, &t2);
    }
    for (int i = 0; i < sides; i++) {
        T3DBuildVert t0 = { -half_l, 0.0f, 0.0f, -1, 0, 0, 0.5f, 0.5f, 0.90f, 0.84f, 0.78f, 1.0f };
        T3DBuildVert t1 = { -half_l, LOG_SIN[i] * radius,     LOG_COS[i] * radius,     -1, 0, 0, 0.5f + 0.48f * LOG_COS[i],     0.5f - 0.48f * LOG_SIN[i],     0.90f, 0.84f, 0.78f, 1.0f };
        T3DBuildVert t2 = { -half_l, LOG_SIN[i + 1] * radius, LOG_COS[i + 1] * radius, -1, 0, 0, 0.5f + 0.48f * LOG_COS[i + 1], 0.5f - 0.48f * LOG_SIN[i + 1], 0.90f, 0.84f, 0.78f, 1.0f };
        t3d_dyn_tri_vc(&t0, &t1, &t2);
    }
    t3d_dyn_end_tris();

    t3d_matrix_pop(1);
}

static void draw_cylinder_y(T3DMeshBuilder *b, float cx, float y0, float y1, float cz, float r, int sides) {
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / (float)sides * 6.2831853f;
        float a1 = (float)(i + 1) / (float)sides * 6.2831853f;
        float nx0 = cosf(a0), nz0 = sinf(a0);
        float nx1 = cosf(a1), nz1 = sinf(a1);
        float px0 = cx + nx0 * r, pz0 = cz + nz0 * r;
        float px1 = cx + nx1 * r, pz1 = cz + nz1 * r;

        float r0b, g0b, b0b, r0t, g0t, b0t, r1t, g1t, b1t, r1b, g1b, b1b;
        baked_rgb(px0, y0, pz0, nx0, 0.0f, nz0, &r0b, &g0b, &b0b);
        baked_rgb(px0, y1, pz0, nx0, 0.0f, nz0, &r0t, &g0t, &b0t);
        baked_rgb(px1, y1, pz1, nx1, 0.0f, nz1, &r1t, &g1t, &b1t);
        baked_rgb(px1, y0, pz1, nx1, 0.0f, nz1, &r1b, &g1b, &b1b);

        T3DBuildVert q0 = { px0, y0, pz0, nx0, 0.0f, nz0, (float)i / sides,       0.0f, r0b, g0b, b0b, 1.0f };
        T3DBuildVert q1 = { px0, y1, pz0, nx0, 0.0f, nz0, (float)i / sides,       1.0f, r0t, g0t, b0t, 1.0f };
        T3DBuildVert q2 = { px1, y1, pz1, nx1, 0.0f, nz1, (float)(i + 1) / sides, 1.0f, r1t, g1t, b1t, 1.0f };
        T3DBuildVert q3 = { px1, y0, pz1, nx1, 0.0f, nz1, (float)(i + 1) / sides, 0.0f, r1b, g1b, b1b, 1.0f };
        t3d_mb_push_quad_vc(b, &q0, &q1, &q2, &q3);
    }

    // Top disk (CCW from +Y: descending angle)
    float rc, gc, bc;
    baked_rgb(cx, y1, cz, 0.0f, 1.0f, 0.0f, &rc, &gc, &bc);
    for (int i = sides; i > 0; i--) {
        float a0 = (float)i / (float)sides * 6.2831853f;
        float a1 = (float)(i - 1) / (float)sides * 6.2831853f;
        float px0 = cx + cosf(a0) * r, pz0 = cz + sinf(a0) * r;
        float px1 = cx + cosf(a1) * r, pz1 = cz + sinf(a1) * r;
        float r0, g0, b0, r1, g1, b1;
        baked_rgb(px0, y1, pz0, 0.0f, 1.0f, 0.0f, &r0, &g0, &b0);
        baked_rgb(px1, y1, pz1, 0.0f, 1.0f, 0.0f, &r1, &g1, &b1);

        T3DBuildVert t0 = { cx,  y1, cz,  0, 1, 0, 0.5f, 0.5f, rc, gc, bc, 1.0f };
        T3DBuildVert t1 = { px0, y1, pz0, 0, 1, 0, 0.5f + 0.4f * cosf(a0), 0.5f + 0.4f * sinf(a0), r0, g0, b0, 1.0f };
        T3DBuildVert t2 = { px1, y1, pz1, 0, 1, 0, 0.5f + 0.4f * cosf(a1), 0.5f + 0.4f * sinf(a1), r1, g1, b1, 1.0f };
        t3d_mb_push_tri_vc(b, &t0, &t1, &t2);
    }
}

static void draw_coal_mound(const SceneTextures *tex, const FireSystem *fs, float dim) {
    const FlameTheme *th = &FLAME_THEMES[fs->theme_idx];
    float eff = fire_effective_intensity(fs);
    float pulse = (0.58f + 0.40f * eff) * (0.88f + 0.14f * fs->flicker) * dim;

    const int ring = 10;
    float cx = 0.0f, cz = -0.46f;
    float rx = 0.62f, rz = 0.36f;

    float cr = fminf(1.0f, 0.35f + th->coal_r * pulse * 1.15f);
    float cg = fminf(1.0f, 0.18f + th->coal_g * pulse * 1.10f);
    float cb = fminf(1.0f, 0.12f + th->coal_b * pulse * 0.95f);

    float rr = fminf(1.0f, 0.18f + th->coal_r * pulse * 0.45f);
    float rg = fminf(1.0f, 0.12f + th->coal_g * pulse * 0.35f);
    float rb = fminf(1.0f, 0.10f + th->coal_b * pulse * 0.30f);

    t3d_dyn_begin_tris(&tex->coals, T3D_ROOM_SCALE, false, true, true, false);
    for (int i = ring; i > 0; i--) {
        float a0 = (float)i / (float)ring * 6.2831853f;
        float a1 = (float)(i - 1) / (float)ring * 6.2831853f;
        float p0x = cx + cosf(a0) * rx, p0z = cz + sinf(a0) * rz;
        float p1x = cx + cosf(a1) * rx, p1z = cz + sinf(a1) * rz;

        T3DBuildVert t0 = { cx,  0.31f,  cz,  0, 1, 0, 1.0f, 1.0f, cr, cg, cb, 1.0f };
        T3DBuildVert t1 = { p0x, 0.202f, p0z, 0, 1, 0, 1.0f + cosf(a0) * 1.1f, 1.0f + sinf(a0) * 1.1f, rr, rg, rb, 1.0f };
        T3DBuildVert t2 = { p1x, 0.202f, p1z, 0, 1, 0, 1.0f + cosf(a1) * 1.1f, 1.0f + sinf(a1) * 1.1f, rr, rg, rb, 1.0f };
        t3d_dyn_tri_vc(&t0, &t1, &t2);
    }
    t3d_dyn_end_tris();
}

static inline float sc_clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

static void emit_clamped_win_quad(float x0, float y0, float x1, float y1, float z,
                                  float wx0, float wy0, float wx1, float wy1,
                                  float r, float g, float b, float a, bool has_uv) {
    float cx0 = sc_clampf(x0, wx0, wx1), cx1 = sc_clampf(x1, wx0, wx1);
    float cy0 = sc_clampf(y0, wy0, wy1), cy1 = sc_clampf(y1, wy0, wy1);
    if (cx1 <= cx0 + 0.002f || cy1 <= cy0 + 0.002f) return;

    float u0 = 0.0f, u1 = 0.0f, v0 = 0.0f, v1 = 0.0f;
    if (has_uv) {
        float dx = x1 - x0;
        float dy = y1 - y0;
        u0 = (cx0 - x0) / dx;
        u1 = (cx1 - x0) / dx;
        v0 = 1.0f - (cy0 - y0) / dy;
        v1 = 1.0f - (cy1 - y0) / dy;
    }

    T3DBuildVert q0 = { cx0, cy0, z, 0,0,1, u0, v0, r, g, b, a };
    T3DBuildVert q1 = { cx1, cy0, z, 0,0,1, u1, v0, r, g, b, a };
    T3DBuildVert q2 = { cx1, cy1, z, 0,0,1, u1, v1, r, g, b, a };
    T3DBuildVert q3 = { cx0, cy1, z, 0,0,1, u0, v1, r, g, b, a };
    t3d_dyn_quad_vc(&q0, &q1, &q2, &q3);
}

static void draw_cabin_window_sky(const SceneTextures *tex, float time,
                                  bool rain_enabled, Season season, float dim, float cam_x) {
    float wx0 = -2.72f, wx1 = -1.86f;
    float wy0 =  1.14f, wy1 =  2.26f;
    float ix0 = wx0 + 0.01f, ix1 = wx1 - 0.01f;
    float iy0 = wy0 + 0.01f, iy1 = wy1 - 0.01f;
    float wz  = -0.155f;

    float px_far  = sc_clampf(cam_x * 0.018f, -0.045f, 0.045f);
    float px_near = sc_clampf(cam_x * 0.034f, -0.075f, 0.075f);

    // Subtle distant storm sheet-lightning pulse (Summer storm & Thanksgiving rain)
    float flash = 0.0f;
    if (rain_enabled && season != SEASON_CHRISTMAS) {
        float period = (season == SEASON_SUMMER) ? 9.0f : 14.0f;
        float scyc = fmodf(time + 5.0f, period);
        if (scyc < 0.28f) {
            float p1 = expf(-scyc * 28.0f);
            float p2 = (scyc > 0.11f) ? 0.72f * expf(-(scyc - 0.11f) * 24.0f) : 0.0f;
            flash = sc_clampf(p1 + p2, 0.0f, 1.0f) * ((season == SEASON_SUMMER) ? 1.15f : 1.0f);
        }
    }

    // 1. Night sky backdrop gradient (opaque, depth-writing at wz = -0.155)
    t3d_dyn_begin_quads(NULL, T3D_ROOM_SCALE, false, true, false, false);
    {
        float br, bg, bb, tr, tg, tb;
        if (season == SEASON_CHRISTMAS) {
            br = 0.08f * dim; bg = 0.14f * dim; bb = 0.28f * dim;
            tr = 0.03f * dim; tg = 0.06f * dim; tb = 0.16f * dim;
        } else if (rain_enabled) {
            float fl_lit = flash * 0.35f * dim;
            br = 0.05f * dim + fl_lit * 0.70f; bg = 0.08f * dim + fl_lit * 0.80f; bb = 0.15f * dim + fl_lit;
            tr = 0.02f * dim + fl_lit * 0.55f; tg = 0.04f * dim + fl_lit * 0.65f; tb = 0.09f * dim + fl_lit * 0.90f;
        } else if (season == SEASON_SPRING) {
            br = 0.16f * dim; bg = 0.13f * dim; bb = 0.28f * dim;
            tr = 0.05f * dim; tg = 0.05f * dim; tb = 0.15f * dim;
        } else if (season == SEASON_SUMMER) {
            br = 0.14f * dim; bg = 0.11f * dim; bb = 0.24f * dim;
            tr = 0.03f * dim; tg = 0.05f * dim; tb = 0.15f * dim;
        } else {
            br = 0.07f * dim; bg = 0.12f * dim; bb = 0.24f * dim;
            tr = 0.02f * dim; tg = 0.04f * dim; tb = 0.12f * dim;
        }
        T3DBuildVert q0 = { wx0, wy0, wz, 0,0,1, 0,0, br, bg, bb, 1.0f };
        T3DBuildVert q1 = { wx1, wy0, wz, 0,0,1, 0,0, br, bg, bb, 1.0f };
        T3DBuildVert q2 = { wx1, wy1, wz, 0,0,1, 0,0, tr, tg, tb, 1.0f };
        T3DBuildVert q3 = { wx0, wy1, wz, 0,0,1, 0,0, tr, tg, tb, 1.0f };
        t3d_dyn_quad_vc(&q0, &q1, &q2, &q3);
    }
    t3d_dyn_end_quads();

    // 2. Moon glow halo + soft drifting night clouds (clamped inside window)
    float mx = sc_clampf(-2.08f + px_far * 0.6f, ix0 + 0.09f, ix1 - 0.09f);
    float my = 2.02f;
    t3d_dyn_begin_quads(&tex->glow, T3D_ROOM_SCALE, true, false, false, false);
    {
        float m_alpha = rain_enabled ? 0.12f : ((season == SEASON_CHRISTMAS) ? 0.34f : 0.30f);
        emit_clamped_win_quad(mx - 0.20f, my - 0.20f, mx + 0.20f, my + 0.20f, wz + 0.008f,
                              ix0, iy0, ix1, iy1,
                              0.76f * dim, 0.86f * dim, 1.00f * dim, m_alpha, true);

        static const float clouds[3][4] = {
            // speed, y, half_w, half_h
            { 0.028f, 2.08f, 0.26f, 0.075f },
            { 0.019f, 1.91f, 0.30f, 0.085f },
            { 0.036f, 1.80f, 0.22f, 0.065f },
        };
        float span = (ix1 - ix0) + 0.60f;
        for (int c = 0; c < 3; c++) {
            float cx = (ix0 - 0.30f) + fmodf(time * clouds[c][0] + c * 0.48f, span) + px_far * 0.35f;
            float cy = clouds[c][1];
            float ca = rain_enabled ? (0.28f + flash * 0.32f) : 0.14f;
            float cr = rain_enabled ? (0.42f + flash * 0.45f) * dim : 0.56f * dim;
            float cg = rain_enabled ? (0.48f + flash * 0.48f) * dim : 0.66f * dim;
            float cb = rain_enabled ? (0.62f + flash * 0.42f) * dim : 0.86f * dim;
            emit_clamped_win_quad(cx - clouds[c][2], cy - clouds[c][3], cx + clouds[c][2], cy + clouds[c][3],
                                  wz + 0.009f, ix0, iy0, ix1, iy1, cr, cg, cb, ca, true);
        }
    }
    t3d_dyn_end_quads();

    // 3. Crisp Moon disk, twinkling stars & clear-night shooting star
    t3d_dyn_begin_quads(&tex->spark, T3D_ROOM_SCALE, true, false, false, false);
    {
        float md_a = rain_enabled ? 0.16f : 0.90f;
        float msz = 0.056f;
        emit_clamped_win_quad(mx - msz, my - msz, mx + msz, my + msz, wz + 0.012f,
                              ix0, iy0, ix1, iy1,
                              0.94f * dim, 0.96f * dim, 1.00f * dim, md_a, true);

        if (!rain_enabled) {
            static const float stars[10][3] = {
                {-2.58f, 2.12f, 0.0f}, {-2.40f, 1.96f, 1.2f}, {-2.24f, 2.15f, 2.4f},
                {-1.98f, 1.86f, 0.8f}, {-2.60f, 1.66f, 3.1f}, {-2.44f, 1.54f, 1.9f},
                {-2.16f, 1.72f, 4.2f}, {-1.96f, 1.62f, 2.7f}, {-2.32f, 2.04f, 5.0f},
                {-2.52f, 1.82f, 3.7f},
            };
            int nstars = (season == SEASON_CHRISTMAS) ? 8 : 10;
            for (int s = 0; s < nstars; s++) {
                float tw = 0.45f + 0.55f * sinf(time * (1.8f + (s % 4) * 0.5f) + stars[s][2]);
                float sz = 0.017f + 0.007f * (s % 2);
                float a = tw * 0.85f;
                float sx = sc_clampf(stars[s][0] + px_far * 0.25f, ix0 + sz, ix1 - sz);
                float sy = stars[s][1];
                T3DBuildVert v0 = { sx - sz, sy - sz, wz + 0.012f, 0,0,1, 0,1, 0.84f * dim, 0.92f * dim, 1.00f * dim, a };
                T3DBuildVert v1 = { sx + sz, sy - sz, wz + 0.012f, 0,0,1, 1,1, 0.84f * dim, 0.92f * dim, 1.00f * dim, a };
                T3DBuildVert v2 = { sx + sz, sy + sz, wz + 0.012f, 0,0,1, 1,0, 0.84f * dim, 0.92f * dim, 1.00f * dim, a };
                T3DBuildVert v3 = { sx - sz, sy + sz, wz + 0.012f, 0,0,1, 0,0, 0.84f * dim, 0.92f * dim, 1.00f * dim, a };
                t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
            }

            // Shooting star streaking across upper panes every 11s
            float scyc = fmodf(time + 3.2f, 11.0f);
            if (scyc < 0.70f) {
                float u = scyc / 0.70f;
                float sa = sinf(u * 3.14159f) * 0.95f;
                float shx = -2.62f + u * 0.64f;
                float shy =  2.18f - u * 0.34f;
                for (int k = 0; k < 4; k++) {
                    float f = (float)k * 0.055f;
                    float kx = shx - f * 0.64f;
                    float ky = shy + f * 0.34f;
                    float ks = 0.022f - k * 0.004f;
                    float ka = sa * (1.0f - k * 0.24f);
                    emit_clamped_win_quad(kx - ks, ky - ks, kx + ks, ky + ks, wz + 0.013f,
                                          ix0, iy0, ix1, iy1,
                                          0.96f * dim, 0.98f * dim, 1.00f * dim, ka, true);
                }
            }
        }
    }
    t3d_dyn_end_quads();

    // 4. Parallax Distant Alpine Mountains & Foreground Pine Silhouettes (depth_write=false at wz+0.016)
    t3d_dyn_begin_tris(NULL, T3D_ROOM_SCALE, false, false, false, false);
    {
        float mr, mg, mb;
        if (season == SEASON_CHRISTMAS) {
            mr = 0.14f * dim; mg = 0.18f * dim; mb = 0.30f * dim;
        } else if (season == SEASON_SPRING) {
            mr = 0.11f * dim; mg = 0.16f * dim; mb = 0.24f * dim;
        } else if (season == SEASON_SUMMER) {
            mr = 0.10f * dim; mg = 0.13f * dim; mb = 0.24f * dim;
        } else {
            mr = 0.08f * dim; mg = 0.11f * dim; mb = 0.21f * dim;
        }
        if (rain_enabled && season != SEASON_CHRISTMAS) {
            mr += flash * 0.14f * dim; mg += flash * 0.16f * dim; mb += flash * 0.22f * dim;
        }
        static const float mtns[3][3] = {
            // left_x, peak_x, peak_h (width implied by symmetric/wide base)
            { -2.78f, -2.50f, 0.44f },
            { -2.54f, -2.24f, 0.50f },
            { -2.26f, -1.96f, 0.41f },
        };
        for (int m = 0; m < 3; m++) {
            float x0 = sc_clampf(mtns[m][0] + px_far, wx0, wx1);
            float xp = sc_clampf(mtns[m][1] + px_far, wx0, wx1);
            float x1 = sc_clampf(mtns[m][1] + (mtns[m][1] - mtns[m][0]) + px_far, wx0, wx1);
            float yp = wy0 + mtns[m][2];
            T3DBuildVert t0 = { x0, wy0, wz + 0.015f, 0,0,1, 0,0, mr, mg, mb, 1.0f };
            T3DBuildVert t1 = { x1, wy0, wz + 0.015f, 0,0,1, 0,0, mr, mg, mb, 1.0f };
            T3DBuildVert t2 = { xp, yp,  wz + 0.015f, 0,0,1, 0,0, mr * 1.18f, mg * 1.18f, mb * 1.18f, 1.0f };
            t3d_dyn_tri_vc(&t0, &t1, &t2);
        }

        // Foreground evergreen pine silhouettes
        float pr = (season == SEASON_SPRING) ? 0.05f * dim : 0.03f * dim;
        float pg = (season == SEASON_SPRING || season == SEASON_SUMMER) ? 0.11f * dim : 0.07f * dim;
        float pb = 0.05f * dim;
        static const float pines[5][3] = {
            // center_x, half_w, height
            { -2.62f, 0.10f, 0.34f },
            { -2.44f, 0.12f, 0.42f },
            { -2.25f, 0.09f, 0.30f },
            { -2.09f, 0.12f, 0.39f },
            { -1.92f, 0.10f, 0.32f },
        };
        for (int p = 0; p < 5; p++) {
            float cx = sc_clampf(pines[p][0] + px_near, wx0 + 0.02f, wx1 - 0.02f);
            float x0 = sc_clampf(cx - pines[p][1], wx0, wx1);
            float x1 = sc_clampf(cx + pines[p][1], wx0, wx1);
            float yp = wy0 + pines[p][2];
            T3DBuildVert t0 = { x0, wy0, wz + 0.017f, 0,0,1, 0,0, pr, pg, pb, 1.0f };
            T3DBuildVert t1 = { x1, wy0, wz + 0.017f, 0,0,1, 0,0, pr, pg, pb, 1.0f };
            T3DBuildVert t2 = { cx, yp,  wz + 0.017f, 0,0,1, 0,0, pr, pg, pb, 1.0f };
            t3d_dyn_tri_vc(&t0, &t1, &t2);

            if (season == SEASON_CHRISTMAS) {
                float sy = wy0 + pines[p][2] * 0.68f;
                float sx0 = cx + (x0 - cx) * 0.32f;
                float sx1 = cx + (x1 - cx) * 0.32f;
                T3DBuildVert c0 = { sx0, sy, wz + 0.018f, 0,0,1, 0,0, 0.60f * dim, 0.68f * dim, 0.80f * dim, 1.0f };
                T3DBuildVert c1 = { sx1, sy, wz + 0.018f, 0,0,1, 0,0, 0.60f * dim, 0.68f * dim, 0.80f * dim, 1.0f };
                T3DBuildVert c2 = { cx,  yp, wz + 0.018f, 0,0,1, 0,0, 0.72f * dim, 0.80f * dim, 0.92f * dim, 1.0f };
                t3d_dyn_tri_vc(&c0, &c1, &c2);
            }
        }
    }
    t3d_dyn_end_tris();

    // 5a. Christmas frosted sill snow ledge & corner frost vignettes
    if (season == SEASON_CHRISTMAS) {
        t3d_dyn_begin_quads(NULL, T3D_ROOM_SCALE, false, false, false, false);
        T3DBuildVert s0 = { wx0, wy0,          wz + 0.020f, 0,0,1, 0,0, 0.76f * dim, 0.84f * dim, 0.94f * dim, 1.0f };
        T3DBuildVert s1 = { wx1, wy0,          wz + 0.020f, 0,0,1, 0,0, 0.76f * dim, 0.84f * dim, 0.94f * dim, 1.0f };
        T3DBuildVert s2 = { wx1, wy0 + 0.075f, wz + 0.020f, 0,0,1, 0,0, 0.58f * dim, 0.68f * dim, 0.84f * dim, 1.0f };
        T3DBuildVert s3 = { wx0, wy0 + 0.075f, wz + 0.020f, 0,0,1, 0,0, 0.58f * dim, 0.68f * dim, 0.84f * dim, 1.0f };
        t3d_dyn_quad_vc(&s0, &s1, &s2, &s3);
        t3d_dyn_end_quads();

        t3d_dyn_begin_quads(&tex->glow, T3D_ROOM_SCALE, true, false, false, false);
        static const float corners[4][2] = {
            { -2.72f, 2.26f }, { -1.86f, 2.26f }, { -2.72f, 1.14f }, { -1.86f, 1.14f },
        };
        for (int c = 0; c < 4; c++) {
            emit_clamped_win_quad(corners[c][0] - 0.18f, corners[c][1] - 0.18f,
                                  corners[c][0] + 0.18f, corners[c][1] + 0.18f,
                                  wz + 0.021f, ix0, iy0, ix1, iy1,
                                  0.76f * dim, 0.88f * dim, 1.00f * dim, 0.16f, true);
        }
        t3d_dyn_end_quads();
    }

    // 5b. Two-depth Snow (Christmas), Rain/Storm (Spring/Summer/Thanksgiving), Spring Petals, or Summer Fireflies
    if (season == SEASON_CHRISTMAS) {
        t3d_dyn_begin_quads(&tex->spark, T3D_ROOM_SCALE, true, false, false, false);
        int num_flakes = rain_enabled ? 24 : 10;
        for (int f = 0; f < num_flakes; f++) {
            float seed = (float)f * 0.413f;
            bool near_plane = (f & 1) == 0;
            float base_spd = rain_enabled ? (near_plane ? 0.38f : 0.24f) : (near_plane ? 0.18f : 0.11f);
            float spd = base_spd + (f % 3) * 0.035f;
            float frac = fmodf(time * spd + seed * 2.7f, 1.0f);
            float fx = ix0 + 0.04f + fmodf(seed * 6.9f + sinf(time * (near_plane ? 1.6f : 1.0f) + seed * 9.0f) * 0.04f + frac * 0.18f, (ix1 - ix0 - 0.08f));
            float fy = iy1 - frac * (iy1 - iy0 - 0.04f);
            float sz = (rain_enabled ? (near_plane ? 0.021f : 0.013f) : (near_plane ? 0.016f : 0.011f));
            float alpha = (rain_enabled ? (near_plane ? 0.80f : 0.50f) : (near_plane ? 0.52f : 0.34f)) * sinf(frac * 3.14159f);
            T3DBuildVert v0 = { fx - sz, fy - sz, wz + 0.023f, 0,0,1, 0,1, 0.92f * dim, 0.96f * dim, 1.00f * dim, alpha };
            T3DBuildVert v1 = { fx + sz, fy - sz, wz + 0.023f, 0,0,1, 1,1, 0.92f * dim, 0.96f * dim, 1.00f * dim, alpha };
            T3DBuildVert v2 = { fx + sz, fy + sz, wz + 0.023f, 0,0,1, 1,0, 0.92f * dim, 0.96f * dim, 1.00f * dim, alpha };
            T3DBuildVert v3 = { fx - sz, fy + sz, wz + 0.023f, 0,0,1, 0,0, 0.92f * dim, 0.96f * dim, 1.00f * dim, alpha };
            t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
        }
        t3d_dyn_end_quads();
    } else if (rain_enabled) {
        int rcount = (season == SEASON_SUMMER) ? 28 : 24;
        t3d_dyn_begin_quads(NULL, T3D_ROOM_SCALE, true, false, false, false);
        for (int r = 0; r < rcount; r++) {
            float seed = (float)r * 0.371f;
            bool near_plane = (r & 1) == 0;
            float speed = (near_plane ? 1.65f : 1.15f) + (r % 4) * 0.16f;
            float frac = fmodf(time * speed + seed * 3.1f, 1.0f);
            float rx = ix0 + 0.05f + fmodf(seed * 7.3f + frac * 0.22f, (ix1 - ix0 - 0.10f));
            float ry = iy1 - frac * (iy1 - iy0);
            float rlen = near_plane ? 0.12f : 0.08f;
            if (ry - rlen < iy0) continue;
            float alpha = (near_plane ? 0.36f : 0.20f) + flash * 0.25f;
            float cr = (0.55f + flash * 0.35f) * dim;
            float cg = (0.72f + flash * 0.24f) * dim;
            float cb = (0.94f + flash * 0.06f) * dim;
            T3DBuildVert v0 = { rx - 0.034f, ry - rlen, wz + 0.023f, 0,0,1, 0,0, cr, cg, cb, alpha };
            T3DBuildVert v1 = { rx - 0.024f, ry - rlen, wz + 0.023f, 0,0,1, 0,0, cr, cg, cb, alpha };
            T3DBuildVert v2 = { rx + 0.005f, ry,        wz + 0.023f, 0,0,1, 0,0, cr, cg, cb, alpha };
            T3DBuildVert v3 = { rx - 0.005f, ry,        wz + 0.023f, 0,0,1, 0,0, cr, cg, cb, alpha };
            t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
        }
        t3d_dyn_end_quads();
    } else if (season == SEASON_SPRING) {
        t3d_dyn_begin_quads(&tex->spark, T3D_ROOM_SCALE, true, false, false, false);
        for (int p = 0; p < 10; p++) {
            float seed = (float)p * 0.47f;
            float spd = 0.14f + (p % 3) * 0.04f;
            float frac = fmodf(time * spd + seed * 2.3f, 1.0f);
            float fx = ix0 + 0.04f + fmodf(seed * 6.2f + frac * 0.26f + sinf(time * 1.5f + seed * 7.0f) * 0.05f, (ix1 - ix0 - 0.08f));
            float fy = iy1 - frac * (iy1 - iy0 - 0.04f);
            float sz = 0.015f + 0.005f * (p & 1);
            float alpha = 0.62f * sinf(frac * 3.14159f);
            emit_clamped_win_quad(fx - sz, fy - sz, fx + sz, fy + sz, wz + 0.023f,
                                  ix0, iy0, ix1, iy1,
                                  1.00f * dim, 0.72f * dim, 0.86f * dim, alpha, true);
        }
        t3d_dyn_end_quads();
    } else if (season == SEASON_SUMMER) {
        t3d_dyn_begin_quads(&tex->spark, T3D_ROOM_SCALE, true, false, false, false);
        for (int f = 0; f < 10; f++) {
            float seed = (float)f * 0.71f;
            float fx = ix0 + 0.07f + fmodf(seed * 5.7f, ix1 - ix0 - 0.14f) + 0.06f * sinf(time * (0.9f + (f % 3) * 0.25f) + seed * 6.0f);
            float fy = wy0 + 0.08f + ((f % 4) * 0.11f) + 0.05f * cosf(time * 1.3f + seed * 4.0f);
            float pulse = 0.25f + 0.75f * fmaxf(0.0f, sinf(time * (2.2f + (f % 3) * 0.6f) + seed * 9.0f));
            float sz = 0.016f + 0.006f * pulse;
            emit_clamped_win_quad(fx - sz, fy - sz, fx + sz, fy + sz, wz + 0.023f,
                                  ix0, iy0, ix1, iy1,
                                  0.86f * dim, 1.00f * dim, 0.36f * dim, 0.82f * pulse, true);
        }
        t3d_dyn_end_quads();
    }
}

static void draw_pumpkin(T3DMeshBuilder *b, float cx, float cy, float cz,
                         float rx, float ry, float r, float g, float b_c) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };
    float mid_y = cy + ry;

    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        float rib = (i & 1) ? 0.90f : 1.04f;
        float pr = r * rib, pg = g * rib, pb = b_c * rib;
        T3DBuildVert q0 = { cx + c0 * rx * 0.82f, cy + ry * 0.18f, cz + s0 * rx * 0.82f, c0, -0.25f, s0, 0, 0, pr, pg, pb, 1.0f };
        T3DBuildVert q1 = { cx + c1 * rx * 0.82f, cy + ry * 0.18f, cz + s1 * rx * 0.82f, c1, -0.25f, s1, 0, 0, pr, pg, pb, 1.0f };
        T3DBuildVert q2 = { cx + c1 * rx,         mid_y + ry * 0.45f, cz + s1 * rx,      c1,  0.25f, s1, 0, 0, pr, pg, pb, 1.0f };
        T3DBuildVert q3 = { cx + c0 * rx,         mid_y + ry * 0.45f, cz + s0 * rx,      c0,  0.25f, s0, 0, 0, pr, pg, pb, 1.0f };
        t3d_mb_push_quad_vc(b, &q0, &q1, &q2, &q3);
    }

    t3d_mb_set_color(b, r * 0.96f, g * 0.92f, b_c * 0.90f, 1.0f);
    t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 6; i++) {
        t3d_mb_push_tri(b,
                        cx, cy + ry * 1.72f, cz,
                        cx + cs[i]     * rx, mid_y + ry * 0.45f, cz + sn[i]     * rx,
                        cx + cs[i + 1] * rx, mid_y + ry * 0.45f, cz + sn[i + 1] * rx);
    }

    t3d_mb_set_color(b, 0.28f, 0.40f, 0.16f, 1.0f);
    draw_cylinder_y(b, cx, cy + ry * 1.65f, cy + ry * 2.18f, cz, rx * 0.16f, 4);
}

static void draw_pumpkin_pie(T3DMeshBuilder *b, float cx, float cy, float cz, float r) {
    static const float cs[9] = { 1.0f,  0.7071f,  0.0f, -0.7071f, -1.0f, -0.7071f,  0.0f,  0.7071f, 1.0f };
    static const float sn[9] = { 0.0f, -0.7071f, -1.0f, -0.7071f,  0.0f,  0.7071f,  1.0f,  0.7071f, 0.0f };
    float h = 0.064f;

    for (int i = 0; i < 8; i++) {
        T3DBuildVert q0 = { cx + cs[i] * r * 0.78f,     cy,     cz + sn[i] * r * 0.78f,     cs[i],     -0.2f, sn[i],     0, 0, 0.78f, 0.80f, 0.84f, 1.0f };
        T3DBuildVert q1 = { cx + cs[i + 1] * r * 0.78f, cy,     cz + sn[i + 1] * r * 0.78f, cs[i + 1], -0.2f, sn[i + 1], 0, 0, 0.78f, 0.80f, 0.84f, 1.0f };
        T3DBuildVert q2 = { cx + cs[i + 1] * r,         cy + h, cz + sn[i + 1] * r,         cs[i + 1],  0.2f, sn[i + 1], 0, 0, 0.78f, 0.80f, 0.84f, 1.0f };
        T3DBuildVert q3 = { cx + cs[i] * r,             cy + h, cz + sn[i] * r,             cs[i],      0.2f, sn[i],     0, 0, 0.78f, 0.80f, 0.84f, 1.0f };
        t3d_mb_push_quad_vc(b, &q0, &q1, &q2, &q3);
    }

    t3d_mb_set_color(b, 0.92f, 0.72f, 0.36f, 1.0f);
    t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 8; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        float rim_h = h + ((i & 1) ? 0.022f : 0.012f);
        t3d_mb_push_quad(b,
                         cx + c0 * r * 0.82f, cy + h + 0.008f, cz + s0 * r * 0.82f,
                         cx + c0 * r * 1.06f, cy + rim_h,      cz + s0 * r * 1.06f,
                         cx + c1 * r * 1.06f, cy + rim_h,      cz + s1 * r * 1.06f,
                         cx + c1 * r * 0.82f, cy + h + 0.008f, cz + s1 * r * 0.82f);
    }

    t3d_mb_set_color(b, 0.86f, 0.38f, 0.08f, 1.0f);
    t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 8; i++) {
        t3d_mb_push_tri(b,
                        cx, cy + h + 0.010f, cz,
                        cx + cs[i]     * r * 0.84f, cy + h + 0.009f, cz + sn[i]     * r * 0.84f,
                        cx + cs[i + 1] * r * 0.84f, cy + h + 0.009f, cz + sn[i + 1] * r * 0.84f);
    }

    t3d_mb_set_color(b, 0.98f, 0.96f, 0.92f, 1.0f);
    float wy = cy + h + 0.010f;
    for (int i = 0; i < 8; i++) {
        t3d_mb_set_normal(b, cs[i], 0.55f, sn[i]);
        t3d_mb_push_tri(b,
                        cx, wy + 0.072f, cz,
                        cx + cs[i]     * r * 0.34f, wy, cz + sn[i]     * r * 0.34f,
                        cx + cs[i + 1] * r * 0.34f, wy, cz + sn[i + 1] * r * 0.34f);
    }
}

static void draw_berry_tart(T3DMeshBuilder *b, float cx, float cy, float cz, float r) {
    // Porcelain plate
    t3d_mb_set_color(b, 0.96f, 0.95f, 0.98f, 1.0f);
    draw_cylinder_y(b, cx, cy, cy + 0.016f, cz, r * 1.05f, 8);
    // Golden fluted tart crust
    t3d_mb_set_color(b, 0.88f, 0.68f, 0.34f, 1.0f);
    draw_cylinder_y(b, cx, cy + 0.016f, cy + 0.068f, cz, r * 0.86f, 8);
    // Vanilla cream filling
    t3d_mb_set_color(b, 0.99f, 0.96f, 0.88f, 1.0f);
    draw_cylinder_y(b, cx, cy + 0.064f, cy + 0.074f, cz, r * 0.76f, 8);

    // Ring of glossy strawberries + center berry + mint leaves
    static const float bcs[6] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f };
    static const float bsn[6] = { 0.0f, -0.866f, -0.866f, 0.0f, 0.866f, 0.866f };
    for (int i = 0; i < 6; i++) {
        float bx = cx + bcs[i] * r * 0.44f;
        float bz = cz + bsn[i] * r * 0.44f;
        t3d_mb_set_color(b, (i & 1) ? 0.92f : 0.86f, 0.14f, (i & 1) ? 0.28f : 0.18f, 1.0f);
        draw_cone_6(b, bx, cy + 0.072f, cy + 0.122f, bz, 0.032f, 0.008f, false);
    }
    t3d_mb_set_color(b, 0.94f, 0.12f, 0.22f, 1.0f);
    draw_cone_6(b, cx, cy + 0.074f, cy + 0.136f, cz, 0.038f, 0.010f, false);
    t3d_mb_set_color(b, 0.18f, 0.64f, 0.26f, 1.0f);
    emit_solid_box(b, cx - 0.032f, cy + 0.128f, cz - 0.010f, cx + 0.032f, cy + 0.140f, cz + 0.010f, false);
}

static void draw_smores_platter(T3DMeshBuilder *b, float cx, float cy, float cz) {
    // Rustic wooden summer platter
    t3d_mb_set_color(b, 0.62f, 0.40f, 0.20f, 1.0f);
    draw_cylinder_y(b, cx, cy, cy + 0.020f, cz, 0.185f, 8);

    // Two toasted campfire s'mores stacks
    static const float sx[2] = { -0.056f, 0.056f };
    static const float sz[2] = {  0.042f, 0.038f };
    for (int i = 0; i < 2; i++) {
        float px = cx + sx[i], pz = cz + sz[i], py = cy + 0.020f;
        // Bottom graham cracker
        t3d_mb_set_color(b, 0.84f, 0.62f, 0.30f, 1.0f);
        emit_solid_box(b, px - 0.040f, py,          pz - 0.040f, px + 0.040f, py + 0.012f, pz + 0.040f, false);
        // Dark chocolate square
        t3d_mb_set_color(b, 0.26f, 0.14f, 0.08f, 1.0f);
        emit_solid_box(b, px - 0.036f, py + 0.012f, pz - 0.036f, px + 0.036f, py + 0.024f, pz + 0.036f, false);
        // Golden toasted marshmallow center
        t3d_mb_set_color(b, 0.94f, 0.78f, 0.52f, 1.0f);
        draw_cylinder_y(b, px, py + 0.024f, py + 0.056f, pz, 0.030f, 6);
        // Top graham cracker
        t3d_mb_set_color(b, 0.86f, 0.64f, 0.32f, 1.0f);
        emit_solid_box(b, px - 0.040f, py + 0.056f, pz - 0.040f, px + 0.040f, py + 0.068f, pz + 0.040f, false);
    }

    // Chilled summer watermelon slice
    float wx = cx, wz = cz - 0.058f, wy = cy + 0.020f;
    t3d_mb_set_color(b, 0.14f, 0.54f, 0.20f, 1.0f);
    emit_solid_box(b, wx - 0.072f, wy, wz - 0.024f, wx + 0.072f, wy + 0.022f, wz + 0.024f, false);
    t3d_mb_set_color(b, 0.94f, 0.22f, 0.28f, 1.0f);
    draw_cone_6(b, wx, wy + 0.022f, wy + 0.092f, wz, 0.064f, 0.008f, false);
}

static void draw_cone_6(T3DMeshBuilder *b,
                        float cx, float y0, float y1, float cz,
                        float r_bot, float r_top, bool cap_bottom) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };
    float h = y1 - y0;
    float ny = (r_bot - r_top) / sqrtf(h * h + (r_bot - r_top) * (r_bot - r_top) + 1e-6f);
    float nr = sqrtf(fmaxf(0.0f, 1.0f - ny * ny));
    float cr, cg, cb;
    t3d_mb_get_color(b, &cr, &cg, &cb, NULL);

    if (r_top <= 0.002f) {
        for (int i = 0; i < 6; i++) {
            t3d_mb_set_normal(b, cs[i] * nr, ny, sn[i] * nr);
            t3d_mb_push_tri(b,
                            cx, y1, cz,
                            cx + cs[i] * r_bot,     y0, cz + sn[i] * r_bot,
                            cx + cs[i + 1] * r_bot, y0, cz + sn[i + 1] * r_bot);
        }
    } else {
        for (int i = 0; i < 6; i++) {
            float c0 = cs[i], s0 = sn[i];
            float c1 = cs[i + 1], s1 = sn[i + 1];
            T3DBuildVert q0 = { cx + c0 * r_bot, y0, cz + s0 * r_bot, c0 * nr, ny, s0 * nr, 0, 0, cr, cg, cb, 1.0f };
            T3DBuildVert q1 = { cx + c1 * r_bot, y0, cz + s1 * r_bot, c1 * nr, ny, s1 * nr, 0, 0, cr, cg, cb, 1.0f };
            T3DBuildVert q2 = { cx + c1 * r_top, y1, cz + s1 * r_top, c1 * nr, ny, s1 * nr, 0, 0, cr, cg, cb, 1.0f };
            T3DBuildVert q3 = { cx + c0 * r_top, y1, cz + s0 * r_top, c0 * nr, ny, s0 * nr, 0, 0, cr, cg, cb, 1.0f };
            t3d_mb_push_quad_vc(b, &q0, &q1, &q2, &q3);
        }
    }

    if (cap_bottom) {
        t3d_mb_set_normal(b, 0.0f, -1.0f, 0.0f);
        for (int i = 6; i > 0; i--) {
            t3d_mb_push_tri(b,
                            cx, y0, cz,
                            cx + cs[i]     * r_bot, y0, cz + sn[i]     * r_bot,
                            cx + cs[i - 1] * r_bot, y0, cz + sn[i - 1] * r_bot);
        }
    }
}

static void draw_gift_box(T3DMeshBuilder *b,
                          float cx, float cy, float cz,
                          float w, float h, float d,
                          float br, float bg, float bb,
                          float rr, float rg, float rb) {
    float hw = w * 0.5f;
    float hd = d * 0.5f;
    float rw = fminf(w, d) * 0.14f;
    float off = 0.006f;
    float by = cy + h + off;

    t3d_mb_set_color(b, br, bg, bb, 1.0f);
    emit_solid_box(b, cx - hw, cy, cz - hd,  cx + hw, cy + h, cz + hd, false);

    t3d_mb_set_color(b, rr, rg, rb, 1.0f);
    emit_front_quad(b, cx - rw, cy + 0.004f, cx + rw, by, cz + hd + off);
    t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
    t3d_mb_push_quad(b, cx - rw, by, cz + hd + off, cx + rw, by, cz + hd + off,
                        cx + rw, by, cz - hd - off, cx - rw, by, cz - hd - off);
    t3d_mb_push_quad(b, cx - hw - off, by, cz + rw, cx + hw + off, by, cz + rw,
                        cx + hw + off, by, cz - rw, cx - hw - off, by, cz - rw);
    t3d_mb_set_normal(b, -1.0f, 0.0f, 0.0f);
    t3d_mb_push_quad(b, cx - hw - off, cy + 0.004f, cz - rw, cx - hw - off, cy + 0.004f, cz + rw,
                        cx - hw - off, by,          cz + rw, cx - hw - off, by,          cz - rw);
    emit_solid_box(b, cx - rw * 2.2f, by, cz - rw * 0.9f,  cx + rw * 2.2f, by + rw * 1.4f, cz + rw * 0.9f, false);
}

static void draw_milk_and_cookies(T3DMeshBuilder *b, float cx, float cy, float cz) {
    t3d_mb_set_color(b, 0.95f, 0.96f, 0.98f, 1.0f);
    draw_cylinder_y(b, cx, cy, cy + 0.018f, cz, 0.175f, 8);

    static const float ck[3][2] = {
        {-0.055f,  0.052f},
        { 0.055f,  0.048f},
        {-0.010f, -0.055f},
    };
    for (int i = 0; i < 3; i++) {
        float kx = cx + ck[i][0];
        float kz = cz + ck[i][1];
        float ky = cy + 0.018f + (i == 2 ? 0.006f : 0.0f);
        t3d_mb_set_color(b, 0.82f, 0.56f, 0.26f, 1.0f);
        draw_cylinder_y(b, kx, ky, ky + 0.020f, kz, 0.048f, 6);
    }

    float gx = cx + 0.072f, gz = cz - 0.052f;
    t3d_mb_set_color(b, 0.84f, 0.90f, 0.96f, 1.0f);
    draw_cylinder_y(b, gx, cy + 0.018f, cy + 0.150f, gz, 0.040f, 8);
    t3d_mb_set_color(b, 0.99f, 0.98f, 0.96f, 1.0f);
    draw_cylinder_y(b, gx, cy + 0.138f, cy + 0.154f, gz, 0.036f, 8);

    t3d_mb_set_color(b, 0.24f, 0.12f, 0.06f, 1.0f);
    for (int i = 0; i < 3; i++) {
        float kx = cx + ck[i][0];
        float kz = cz + ck[i][1];
        float ky = cy + 0.018f + (i == 2 ? 0.006f : 0.0f);
        emit_solid_box(b, kx - 0.020f, ky + 0.020f, kz - 0.014f,  kx - 0.006f, ky + 0.028f, kz,          false);
        emit_solid_box(b, kx + 0.010f, ky + 0.020f, kz + 0.006f,  kx + 0.024f, ky + 0.028f, kz + 0.020f, false);
        emit_solid_box(b, kx - 0.006f, ky + 0.020f, kz + 0.018f,  kx + 0.008f, ky + 0.028f, kz + 0.032f, false);
    }
    t3d_mb_set_color(b, 0.92f, 0.12f, 0.16f, 1.0f);
    emit_solid_box(b, cx - 0.125f, cy + 0.018f, cz - 0.030f,  cx - 0.100f, cy + 0.038f, cz + 0.048f, false);
    t3d_mb_set_color(b, 0.98f, 0.98f, 0.98f, 1.0f);
    emit_solid_box(b, cx - 0.126f, cy + 0.019f, cz - 0.008f,  cx - 0.099f, cy + 0.039f, cz + 0.012f, false);
    t3d_mb_set_color(b, 0.92f, 0.12f, 0.16f, 1.0f);
    emit_solid_box(b, cx - 0.125f, cy + 0.018f, cz - 0.056f,  cx - 0.078f, cy + 0.038f, cz - 0.030f, false);
}

static void build_snowglobe_opaque_mesh(T3DMeshBuilder *b, Season season) {
    t3d_mb_set_texture(b, NULL);

    t3d_mb_set_color(b, 0.32f, 0.16f, 0.08f, 1.0f);
    draw_cylinder_y(b, 0.0f, 0.0f, 0.046f, 0.0f, 0.088f, 8);
    t3d_mb_set_color(b, 0.88f, 0.70f, 0.22f, 1.0f);
    draw_cylinder_y(b, 0.0f, 0.046f, 0.058f, 0.0f, 0.092f, 8);

    if (season == SEASON_CHRISTMAS) {
        t3d_mb_set_color(b, 0.95f, 0.97f, 1.00f, 1.0f);
        draw_cylinder_y(b, 0.0f, 0.058f, 0.074f, 0.0f, 0.078f, 8);

        t3d_mb_set_color(b, 0.12f, 0.50f, 0.22f, 1.0f);
        draw_cone_6(b, -0.026f, 0.074f, 0.155f, -0.010f, 0.038f, 0.001f, false);

        t3d_mb_set_color(b, 0.98f, 0.98f, 1.00f, 1.0f);
        draw_cylinder_y(b, 0.032f, 0.074f, 0.106f, 0.016f, 0.022f, 6);
        draw_cylinder_y(b, 0.032f, 0.106f, 0.130f, 0.016f, 0.016f, 6);

        t3d_mb_set_color(b, 1.00f, 0.86f, 0.22f, 1.0f);
        emit_solid_box(b, -0.034f, 0.154f, -0.018f, -0.018f, 0.170f, -0.002f, false);
        t3d_mb_set_color(b, 0.92f, 0.12f, 0.16f, 1.0f);
        emit_solid_box(b,  0.014f, 0.102f,  0.002f,  0.050f, 0.110f,  0.034f, false);
    } else if (season == SEASON_SPRING) {
        t3d_mb_set_color(b, 0.22f, 0.58f, 0.24f, 1.0f);
        draw_cylinder_y(b, 0.0f, 0.058f, 0.074f, 0.0f, 0.078f, 8);

        // Mini cherry blossom tree
        t3d_mb_set_color(b, 0.36f, 0.22f, 0.12f, 1.0f);
        draw_cylinder_y(b, -0.018f, 0.074f, 0.116f, -0.008f, 0.011f, 5);
        t3d_mb_set_color(b, 0.96f, 0.62f, 0.78f, 1.0f);
        draw_cone_6(b, -0.018f, 0.108f, 0.170f, -0.008f, 0.044f, 0.004f, true);

        // Tiny yellow spring blossom
        t3d_mb_set_color(b, 0.98f, 0.86f, 0.22f, 1.0f);
        emit_solid_box(b, 0.022f, 0.074f, 0.012f, 0.044f, 0.096f, 0.034f, false);
    } else if (season == SEASON_SUMMER) {
        t3d_mb_set_color(b, 0.18f, 0.52f, 0.20f, 1.0f);
        draw_cylinder_y(b, 0.0f, 0.058f, 0.074f, 0.0f, 0.078f, 8);

        // Emerald pine & miniature A-frame camp tent + tiny ember campfire
        t3d_mb_set_color(b, 0.10f, 0.42f, 0.18f, 1.0f);
        draw_cone_6(b, -0.026f, 0.074f, 0.158f, -0.010f, 0.034f, 0.001f, false);
        t3d_mb_set_color(b, 0.90f, 0.52f, 0.16f, 1.0f);
        draw_cone_6(b,  0.026f, 0.074f, 0.120f,  0.010f, 0.028f, 0.002f, false);
        t3d_mb_set_color(b, 1.00f, 0.64f, 0.14f, 1.0f);
        emit_solid_box(b, -0.006f, 0.074f, 0.024f, 0.012f, 0.090f, 0.042f, false);
    } else {
        t3d_mb_set_color(b, 0.54f, 0.36f, 0.14f, 1.0f);
        draw_cylinder_y(b, 0.0f, 0.058f, 0.074f, 0.0f, 0.078f, 8);

        draw_pumpkin(b, -0.028f, 0.074f, 0.014f, 0.026f, 0.020f, 0.94f, 0.44f, 0.08f);

        t3d_mb_set_color(b, 0.36f, 0.20f, 0.10f, 1.0f);
        draw_cylinder_y(b, 0.028f, 0.074f, 0.108f, -0.010f, 0.010f, 5);
        t3d_mb_set_color(b, 0.88f, 0.38f, 0.08f, 1.0f);
        draw_cone_6(b, 0.028f, 0.102f, 0.165f, -0.010f, 0.038f, 0.001f, false);
    }
}

static void build_snowglobe_dome_mesh(T3DMeshBuilder *b) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };
    static const float lat_y[4] = { -0.62f, 0.25f, 0.72f, 1.00f };
    static const float lat_r[4] = {  0.78f, 0.98f, 0.68f, 0.00f };
    float cy = 0.132f;
    float r = 0.112f;

    t3d_mb_set_texture(b, NULL);
    t3d_mb_set_cull(b, false);
    t3d_mb_set_lit(b, false);
    t3d_mb_set_blend(b, true, false);
    t3d_mb_set_color(b, 0.76f, 0.90f, 1.00f, 0.24f);
    t3d_mb_set_normal(b, 0.0f, 0.0f, 1.0f);

    for (int band = 0; band < 2; band++) {
        for (int i = 0; i < 6; i++) {
            t3d_mb_push_quad(b,
                             cs[i]     * lat_r[band]     * r, cy + lat_y[band]     * r, sn[i]     * lat_r[band]     * r,
                             cs[i + 1] * lat_r[band]     * r, cy + lat_y[band]     * r, sn[i + 1] * lat_r[band]     * r,
                             cs[i + 1] * lat_r[band + 1] * r, cy + lat_y[band + 1] * r, sn[i + 1] * lat_r[band + 1] * r,
                             cs[i]     * lat_r[band + 1] * r, cy + lat_y[band + 1] * r, sn[i]     * lat_r[band + 1] * r);
        }
    }
    for (int i = 0; i < 6; i++) {
        t3d_mb_push_tri(b,
                        0.0f, cy + r, 0.0f,
                        cs[i]     * lat_r[2] * r, cy + lat_y[2] * r, sn[i]     * lat_r[2] * r,
                        cs[i + 1] * lat_r[2] * r, cy + lat_y[2] * r, sn[i + 1] * lat_r[2] * r);
    }
}

static void draw_snowglobe_opaque(const SceneTextures *tex, Season season, const SnowglobeTransition *sg) {
    int sidx = ((int)season >= 0 && (int)season < SEASON_COUNT) ? (int)season : (int)SEASON_THANKSGIVING;
    t3d_matrix_push_pos(1);
    t3d_xf_reset();
    t3d_xf_translate(-1.44f, 2.02f + sg->lift_y, 0.28f);
    t3d_xf_rotate_z(sg->shake_angle);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&tex->snowglobe_mesh[sidx]);
    t3d_matrix_pop(1);
}

static void draw_snowglobe_glass(const SceneTextures *tex,
                                 float time,
                                 Season season,
                                 const SnowglobeTransition *sg,
                                 float cam_x, float cam_z) {
    float vx = cam_x - (-1.44f);
    float vz = cam_z - 0.28f;
    float len = sqrtf(vx * vx + vz * vz);
    if (len < 0.001f) len = 1.0f;
    float rx = vz / len;
    float rz = -vx / len;

    t3d_matrix_push_pos(1);
    t3d_xf_reset();
    t3d_xf_translate(-1.44f, 2.02f + sg->lift_y, 0.28f);
    t3d_xf_rotate_z(sg->shake_angle);
    t3d_xf_apply(T3D_PROP_SCALE);

    // 1. Swirling miniature seasonal motes inside snowglobe
    int mote_count = (sg->phase == SNOWGLOBE_IDLE) ? 4 : 14;
    t3d_dyn_begin_quads(&tex->spark, T3D_PROP_SCALE, true, false, false, false);
    float swirl_spd = 1.2f + 6.5f * sg->shake_intensity;
    for (int i = 0; i < mote_count; i++) {
        float seed = (float)i * 0.73f;
        float ang = time * swirl_spd * (0.7f + 0.12f * (i % 4)) + seed * 4.2f;
        float rad = (0.025f + 0.055f * ((i % 5) / 4.0f)) * (0.65f + 0.35f * sg->shake_intensity);
        float px = cosf(ang) * rad;
        float pz = sinf(ang) * rad;
        float py = 0.132f + sinf(time * (1.8f + sg->shake_intensity * 6.0f) + seed * 3.1f) * (0.036f + 0.034f * sg->shake_intensity);
        float sz = 0.007f + 0.005f * sg->shake_intensity;
        float dx = rx * sz;
        float dz = rz * sz;
        float cr, cg, cb, ca;
        Season eff_s = (sg->shake_intensity > 0.3f && sg->swapped) ? sg->target_season : season;
        if (eff_s == SEASON_CHRISTMAS) {
            cr = 0.94f; cg = 0.98f; cb = 1.00f; ca = 0.65f + 0.32f * sg->shake_intensity;
        } else if (eff_s == SEASON_SPRING) {
            cr = 1.00f; cg = 0.74f; cb = 0.86f; ca = 0.70f + 0.26f * sg->shake_intensity;
        } else if (eff_s == SEASON_SUMMER) {
            cr = 0.86f; cg = 1.00f; cb = 0.40f; ca = 0.72f + 0.25f * sg->shake_intensity;
        } else {
            cr = 1.00f; cg = 0.78f; cb = 0.28f; ca = 0.65f + 0.30f * sg->shake_intensity;
        }
        T3DBuildVert v0 = { px - dx, py - sz, pz - dz, 0,0,1, 0, 1, cr, cg, cb, ca };
        T3DBuildVert v1 = { px + dx, py - sz, pz + dz, 0,0,1, 1, 1, cr, cg, cb, ca };
        T3DBuildVert v2 = { px + dx, py + sz, pz + dz, 0,0,1, 1, 0, cr, cg, cb, ca };
        T3DBuildVert v3 = { px - dx, py + sz, pz - dz, 0,0,1, 0, 0, cr, cg, cb, ca };
        t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
    }
    t3d_dyn_end_quads();

    // 2. Pre-compiled translucent ice-blue glass dome
    t3d_mesh_draw(&tex->snowglobe_dome_mesh);

    // 3. Soft glass highlight sheen only when zoomed/transitioning
    if (sg->phase != SNOWGLOBE_IDLE) {
        t3d_dyn_begin_quads(&tex->glow, T3D_PROP_SCALE, true, false, false, false);
        float sx = -0.042f, sy = 0.175f, szp = 0.088f, ss = 0.042f;
        T3DBuildVert v0 = { sx - rx * ss, sy - ss, szp - rz * ss, 0,0,1, 0, 1, 0.95f, 0.98f, 1.00f, 0.34f };
        T3DBuildVert v1 = { sx + rx * ss, sy - ss, szp + rz * ss, 0,0,1, 1, 1, 0.95f, 0.98f, 1.00f, 0.34f };
        T3DBuildVert v2 = { sx + rx * ss, sy + ss, szp + rz * ss, 0,0,1, 1, 0, 0.95f, 0.98f, 1.00f, 0.34f };
        T3DBuildVert v3 = { sx - rx * ss, sy + ss, szp - rz * ss, 0,0,1, 0, 0, 0.95f, 0.98f, 1.00f, 0.34f };
        t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
        t3d_dyn_end_quads();
    }

    t3d_matrix_pop(1);
}

static void draw_christmas_lights(const SceneTextures *tex,
                                  float time,
                                  float dim,
                                  float cam_x, float cam_z) {
    static const float bulbs[14][3] = {
        {-1.52f, 2.00f, 0.56f}, {-1.14f, 1.98f, 0.56f}, {-0.76f, 1.97f, 0.56f},
        {-0.38f, 1.98f, 0.56f}, { 0.00f, 2.00f, 0.56f}, { 0.38f, 1.98f, 0.56f},
        { 0.76f, 1.97f, 0.56f}, { 1.14f, 1.98f, 0.56f}, { 1.52f, 2.00f, 0.56f},
        { 1.28f, 0.44f, 0.56f}, { 1.64f, 0.44f, 0.55f}, { 1.46f, 0.50f, 0.68f},
        { 1.34f, 0.68f, 0.56f}, { 1.58f, 0.68f, 0.55f},
    };
    static const float colors[4][3] = {
        {1.00f, 0.20f, 0.18f},
        {1.00f, 0.86f, 0.22f},
        {0.20f, 0.92f, 0.36f},
        {0.38f, 0.86f, 1.00f},
    };

    float vx = cam_x;
    float vz = cam_z;
    float len = sqrtf(vx * vx + vz * vz);
    if (len < 0.001f) len = 1.0f;
    float rx = vz / len;
    float rz = -vx / len;

    t3d_dyn_begin_quads(&tex->spark, T3D_ROOM_SCALE, true, false, false, false);
    for (int i = 0; i < 14; i++) {
        float tw = (0.55f + 0.45f * sinf(time * (2.6f + (i % 5) * 0.45f) + (float)i * 1.35f)) * dim;
        const float *c = colors[i % 4];
        float sz = 0.036f * (0.90f + 0.16f * tw);
        float dx = rx * sz, dz = rz * sz;
        float cr = fminf(1.0f, c[0] + 0.18f);
        float cg = fminf(1.0f, c[1] + 0.18f);
        float cb = fminf(1.0f, c[2] + 0.18f);
        float ca = 0.92f * tw;
        T3DBuildVert v0 = { bulbs[i][0] - dx, bulbs[i][1] - sz, bulbs[i][2] - dz, 0,0,1, 0, 1, cr, cg, cb, ca };
        T3DBuildVert v1 = { bulbs[i][0] + dx, bulbs[i][1] - sz, bulbs[i][2] + dz, 0,0,1, 1, 1, cr, cg, cb, ca };
        T3DBuildVert v2 = { bulbs[i][0] + dx, bulbs[i][1] + sz, bulbs[i][2] + dz, 0,0,1, 1, 0, cr, cg, cb, ca };
        T3DBuildVert v3 = { bulbs[i][0] - dx, bulbs[i][1] + sz, bulbs[i][2] - dz, 0,0,1, 0, 0, cr, cg, cb, ca };
        t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
    }
    t3d_dyn_end_quads();
}

static void draw_summer_fireflies(const SceneTextures *tex,
                                  float time,
                                  float dim,
                                  float cam_x, float cam_z) {
    static const float anchors[6][3] = {
        { -2.54f, 1.38f, 0.10f }, { -2.14f, 1.54f, 0.14f }, { -1.96f, 1.28f, 0.12f },
        {  1.32f, 2.18f, 0.28f }, {  1.42f, 2.24f, 0.26f }, {  1.46f, 2.14f, 0.30f },
    };
    float vx = cam_x, vz = cam_z;
    float len = sqrtf(vx * vx + vz * vz);
    if (len < 0.001f) len = 1.0f;
    float rx = vz / len;
    float rz = -vx / len;

    t3d_dyn_begin_quads(&tex->spark, T3D_ROOM_SCALE, true, false, false, false);
    for (int i = 0; i < 6; i++) {
        float seed = (float)i * 1.37f;
        float fx = anchors[i][0] + 0.06f * sinf(time * 1.1f + seed);
        float fy = anchors[i][1] + 0.05f * cosf(time * 1.4f + seed * 1.5f);
        float fz = anchors[i][2] + 0.04f * sinf(time * 0.9f + seed * 2.1f);
        float glow = 0.20f + 0.80f * fmaxf(0.0f, sinf(time * (2.4f + (i % 3) * 0.5f) + seed * 3.0f));
        float sz = 0.024f + 0.010f * glow;
        float dx = rx * sz, dz = rz * sz;
        float cr = 0.88f * dim, cg = 1.00f * dim, cb = 0.38f * dim, ca = 0.86f * glow;
        T3DBuildVert v0 = { fx - dx, fy - sz, fz - dz, 0,0,1, 0,1, cr, cg, cb, ca };
        T3DBuildVert v1 = { fx + dx, fy - sz, fz + dz, 0,0,1, 1,1, cr, cg, cb, ca };
        T3DBuildVert v2 = { fx + dx, fy + sz, fz + dz, 0,0,1, 1,0, cr, cg, cb, ca };
        T3DBuildVert v3 = { fx - dx, fy + sz, fz - dz, 0,0,1, 0,0, cr, cg, cb, ca };
        t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
    }
    t3d_dyn_end_quads();
}

static void build_base_scene_mesh(T3DMeshBuilder *b, const SceneTextures *tex) {
    const FlameTheme *neutral_th = &FLAME_THEMES[0];
    s_bake_active = true;

    // 1. Cabin Wood Floor & Woven Hearth Rug
    set_base_color(b, 0.95f, 0.90f, 0.86f);
    t3d_mb_set_texture(b, &tex->floor);
    draw_grid_quad(b, -4.2f, 0.0f, 4.4f,  8.4f, 0.0f, 0.0f,  0.0f, 0.0f, -4.8f,
                   0.0f, 1.0f, 0.0f,  5.0f, 3.6f, 4, 3);

    set_base_color(b, 0.98f, 0.94f, 0.90f);
    t3d_mb_set_texture(b, &tex->rug);
    draw_grid_quad(b, -1.55f, 0.035f, 2.48f,  3.10f, 0.0f, 0.0f,  0.0f, 0.0f, -1.56f,
                   0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 3, 2);
    draw_grid_quad(b, -1.55f, 0.0f, 2.48f,  3.10f, 0.0f, 0.0f,  0.0f, 0.035f, 0.0f,
                   0.0f, 0.0f, 1.0f,  1.0f, 0.08f, 1, 1);
    draw_grid_quad(b, -1.55f, 0.0f, 0.92f,  0.0f, 0.0f, 1.56f,  0.0f, 0.035f, 0.0f,
                  -1.0f, 0.0f, 0.0f,  1.0f, 0.08f, 1, 1);
    draw_grid_quad(b,  1.55f, 0.0f, 2.48f,  0.0f, 0.0f,-1.56f,  0.0f, 0.035f, 0.0f,
                   1.0f, 0.0f, 0.0f,  1.0f, 0.08f, 1, 1);

    // 2. Cozy Timber Cabin Back Walls & Window Frame
    set_base_color(b, 0.76f, 0.70f, 0.66f);
    t3d_mb_set_texture(b, &tex->wood_mantle);
    draw_grid_quad(b, -4.2f, 0.0f, -0.18f,  2.78f, 0.0f, 0.0f,  0.0f, 3.6f, 0.0f,
                   0.0f, 0.0f, 1.0f,  2.2f, 3.2f, 3, 3);
    draw_grid_quad(b,  1.42f, 0.0f, -0.18f,  2.78f, 0.0f, 0.0f,  0.0f, 3.6f, 0.0f,
                   0.0f, 0.0f, 1.0f,  2.2f, 3.2f, 3, 3);
    draw_box(b, -4.2f, 0.0f, -0.18f, -1.45f, 0.18f, -0.10f, 1.0f, 1);
    draw_box(b,  1.45f, 0.0f, -0.18f,  4.2f, 0.18f, -0.10f, 1.0f, 1);

    float wx0 = -2.72f, wx1 = -1.86f;
    float wy0 =  1.14f, wy1 =  2.26f;
    set_base_color(b, 0.75f, 0.68f, 0.62f);
    draw_box(b, wx0 - 0.09f, wy0 - 0.08f, -0.18f,  wx1 + 0.09f, wy0,        -0.06f, 1.2f, 1);
    draw_box(b, wx0 - 0.08f, wy1,         -0.18f,  wx1 + 0.08f, wy1 + 0.08f,-0.08f, 1.2f, 1);
    draw_box(b, wx0 - 0.08f, wy0,         -0.18f,  wx0,         wy1,        -0.08f, 1.2f, 1);
    draw_box(b, wx1,         wy0,         -0.18f,  wx1 + 0.08f, wy1,        -0.08f, 1.2f, 1);
    float mx = (wx0 + wx1) * 0.5f;
    float my = (wy0 + wy1) * 0.5f;
    draw_box(b, mx - 0.024f, wy0, -0.16f,  mx + 0.024f, wy1, -0.11f, 1.2f, 1);
    draw_box(b, wx0, my - 0.024f, -0.16f,  wx1, my + 0.024f, -0.11f, 1.2f, 1);

    // 3. Stone Fireplace Surround, Hearth Slab & Chimney Breast
    set_base_color(b, 0.96f, 0.94f, 0.92f);
    t3d_mb_set_texture(b, &tex->stone);
    draw_box(b, -1.74f, 0.0f, -0.98f,  1.74f, 0.18f, 0.76f, 0.95f, 2);
    draw_box(b, -1.46f, 0.18f, -0.22f, -0.82f, 1.82f, 0.24f, 1.05f, 2);
    draw_box(b,  0.82f, 0.18f, -0.22f,  1.46f, 1.82f, 0.24f, 1.05f, 2);
    draw_box(b, -0.84f, 1.42f, -0.22f,  0.84f, 1.82f, 0.24f, 1.05f, 2);
    draw_box(b, -1.38f, 2.02f, -0.22f,  1.38f, 3.65f, 0.12f, 0.95f, 2);

    // 4. Recessed Sooty Firebrick Firebox Interior
    set_base_color(b, 0.95f, 0.88f, 0.84f);
    t3d_mb_set_texture(b, &tex->brick);
    draw_grid_quad(b, -0.68f, 0.18f, -0.94f,  1.36f, 0.0f, 0.0f,  0.0f, 1.28f, 0.0f,
                   0.0f, 0.0f, 1.0f,  2.2f, 2.4f, 3, 3);
    draw_grid_quad(b, -0.82f, 0.18f,  0.02f,  0.14f, 0.0f, -0.96f, 0.0f, 1.28f, 0.0f,
                   0.989f, 0.0f, 0.145f,  1.6f, 2.4f, 2, 3);
    draw_grid_quad(b,  0.68f, 0.18f, -0.94f,  0.14f, 0.0f,  0.96f, 0.0f, 1.28f, 0.0f,
                  -0.989f, 0.0f, 0.145f,  1.6f, 2.4f, 2, 3);
    set_base_color(b, 0.45f, 0.40f, 0.38f);
    draw_grid_quad(b,  0.82f, 1.42f, 0.02f, -1.64f, 0.0f, 0.0f,  0.0f, 0.24f, -0.96f,
                   0.0f, -0.97f, -0.24f,  2.0f, 1.5f, 1, 1);

    // 5. Heavy Oak Timber Mantle, Corbels & Framed Painting
    set_base_color(b, 0.95f, 0.90f, 0.86f);
    t3d_mb_set_texture(b, &tex->wood_mantle);
    draw_box(b, -1.76f, 1.80f, -0.04f,  1.76f, 2.02f, 0.50f, 0.85f, 2);
    draw_box(b, -1.26f, 1.54f, 0.16f, -1.04f, 1.80f, 0.42f, 1.2f, 1);
    draw_box(b,  1.04f, 1.54f, 0.16f,  1.26f, 1.80f, 0.42f, 1.2f, 1);

    set_base_color(b, 0.84f, 0.76f, 0.68f);
    draw_box(b, -0.90f, 2.12f, 0.12f,  0.90f, 2.20f, 0.23f, 1.2f, 1);
    draw_box(b, -0.90f, 2.96f, 0.12f,  0.90f, 3.04f, 0.23f, 1.2f, 1);
    draw_box(b, -0.90f, 2.20f, 0.12f, -0.82f, 2.96f, 0.23f, 1.2f, 1);
    draw_box(b,  0.82f, 2.20f, 0.12f,  0.90f, 2.96f, 0.23f, 1.2f, 1);

    // 6. Cozy Mantle Props: Pillar Candles
    t3d_mb_set_texture(b, NULL);
    set_base_color(b, 0.95f, 0.88f, 0.72f);
    draw_cylinder_y(b, -1.18f, 2.02f, 2.30f, 0.26f, 0.052f, 6);
    draw_cylinder_y(b, -0.96f, 2.02f, 2.20f, 0.30f, 0.046f, 6);
    draw_cylinder_y(b,  0.82f, 2.02f, 2.26f, 0.26f, 0.050f, 6);

    // 7. Wrought-Iron Fire Grate & Andirons
    set_base_color(b, 0.85f, 0.82f, 0.80f);
    t3d_mb_set_texture(b, &tex->iron);
    draw_box(b, -0.56f, 0.30f, -0.24f,  0.56f, 0.34f, -0.19f, 2.0f, 1);
    draw_box(b, -0.56f, 0.30f, -0.68f,  0.56f, 0.34f, -0.63f, 2.0f, 1);
    for (int i = -2; i <= 2; i++) {
        float bx = i * 0.22f;
        draw_box(b, bx - 0.022f, 0.31f, -0.69f, bx + 0.022f, 0.35f, -0.18f, 2.0f, 1);
    }
    for (int side = -1; side <= 1; side += 2) {
        float ax = side * 0.56f;
        draw_cylinder_y(b, ax, 0.18f, 0.62f, -0.10f, 0.030f, 6);
        draw_box(b, ax - 0.044f, 0.60f, -0.144f, ax + 0.044f, 0.69f, -0.056f, 2.0f, 1);
        draw_box(b, ax - 0.045f, 0.18f, -0.14f, ax + 0.045f, 0.23f, 0.02f, 2.0f, 1);
        draw_box(b, ax - 0.030f, 0.22f, -0.68f, ax + 0.030f, 0.27f, -0.10f, 2.0f, 1);
    }

    // 8. Neat Stacked Firewood Pile Beside the Hearth
    static const float pile[6][3] = {
        {1.92f, 0.09f, 0.20f},
        {2.10f, 0.09f, 0.22f},
        {2.28f, 0.09f, 0.19f},
        {2.01f, 0.24f, 0.21f},
        {2.19f, 0.24f, 0.20f},
        {2.10f, 0.38f, 0.21f},
    };
    for (int p = 0; p < 6; p++) {
        emit_static_log_to_builder(b, tex, pile[p][0], pile[p][1], pile[p][2],
                                   0.56f, 0.082f, 90.0f + (p - 2) * 2.5f, 0.0f, 0.0f, neutral_th);
    }

    s_bake_active = false;
}

static void draw_flower_pot(T3DMeshBuilder *b, float cx, float cy, float cz,
                            float scale, float fl_r, float fl_g, float fl_b) {
    float pr = 0.088f * scale;
    float ph = 0.130f * scale;
    // Terracotta pot & rim
    t3d_mb_set_color(b, 0.72f, 0.34f, 0.20f, 1.0f);
    draw_cylinder_y(b, cx, cy, cy + ph, cz, pr * 0.86f, 6);
    t3d_mb_set_color(b, 0.80f, 0.40f, 0.24f, 1.0f);
    draw_cylinder_y(b, cx, cy + ph * 0.78f, cy + ph, cz, pr, 6);

    // Rich dark soil + green leaves
    t3d_mb_set_color(b, 0.24f, 0.16f, 0.10f, 1.0f);
    draw_cylinder_y(b, cx, cy + ph, cy + ph + 0.010f * scale, cz, pr * 0.76f, 6);
    t3d_mb_set_color(b, 0.16f, 0.54f, 0.22f, 1.0f);
    draw_cone_6(b, cx, cy + ph + 0.008f * scale, cy + ph + 0.090f * scale, cz, pr * 0.90f, pr * 0.25f, false);

    // Center stem + tulip blossom trio
    t3d_mb_set_color(b, 0.20f, 0.62f, 0.24f, 1.0f);
    draw_cylinder_y(b, cx, cy + ph + 0.06f * scale, cy + ph + 0.15f * scale, cz, 0.012f * scale, 4);
    t3d_mb_set_color(b, fl_r, fl_g, fl_b, 1.0f);
    draw_cone_6(b, cx, cy + ph + 0.13f * scale, cy + ph + 0.20f * scale, cz, 0.044f * scale, 0.012f * scale, true);
    t3d_mb_set_color(b, 0.98f, 0.84f, 0.26f, 1.0f);
    draw_cone_6(b, cx - 0.038f * scale, cy + ph + 0.09f * scale, cy + ph + 0.15f * scale, cz + 0.018f * scale, 0.032f * scale, 0.008f * scale, true);
    t3d_mb_set_color(b, 0.78f, 0.56f, 0.96f, 1.0f);
    draw_cone_6(b, cx + 0.038f * scale, cy + ph + 0.10f * scale, cy + ph + 0.16f * scale, cz + 0.014f * scale, 0.032f * scale, 0.008f * scale, true);
}

static void draw_camp_lantern(T3DMeshBuilder *b, float cx, float cy, float cz) {
    // Vintage brass base + glass chamber + domed cap
    t3d_mb_set_color(b, 0.76f, 0.58f, 0.20f, 1.0f);
    draw_cylinder_y(b, cx, cy, cy + 0.055f, cz, 0.092f, 6);
    t3d_mb_set_color(b, 1.00f, 0.84f, 0.42f, 1.0f);
    draw_cylinder_y(b, cx, cy + 0.055f, cy + 0.195f, cz, 0.064f, 6);
    t3d_mb_set_color(b, 0.76f, 0.58f, 0.20f, 1.0f);
    draw_cone_6(b, cx, cy + 0.195f, cy + 0.255f, cz, 0.088f, 0.022f, true);
    draw_cylinder_y(b, cx, cy + 0.255f, cy + 0.280f, cz, 0.018f, 5);
}

static void build_spring_mesh(T3DMeshBuilder *b) {
    t3d_mb_set_texture(b, NULL);

    // 1. Mint ceramic teacup & handle on right mantle
    t3d_mb_set_color(b, 0.52f, 0.82f, 0.68f, 1.0f);
    draw_cylinder_y(b, 1.08f, 2.02f, 2.18f, 0.24f, 0.056f, 6);
    draw_box(b, 1.13f, 2.06f, 0.22f, 1.18f, 2.14f, 0.26f, 1.0f, 1);

    // 2. Cerulean ceramic flower vase + cherry blossom blooms on right mantle
    t3d_mb_set_color(b, 0.34f, 0.62f, 0.88f, 1.0f);
    draw_cylinder_y(b, 1.40f, 2.02f, 2.20f, 0.26f, 0.058f, 6);
    t3d_mb_set_color(b, 0.20f, 0.58f, 0.24f, 1.0f);
    draw_cylinder_y(b, 1.40f, 2.19f, 2.27f, 0.26f, 0.022f, 5);
    t3d_mb_set_color(b, 0.98f, 0.64f, 0.80f, 1.0f);
    emit_solid_box(b, 1.35f, 2.25f, 0.22f, 1.45f, 2.33f, 0.30f, false);
    t3d_mb_set_color(b, 0.98f, 0.88f, 0.34f, 1.0f);
    emit_solid_box(b, 1.33f, 2.22f, 0.24f, 1.38f, 2.28f, 0.29f, false);

    // 3. Leafy spring blossom mantle garland + frame blossom sprig
    for (int i = 0; i < 4; i++) {
        float gx = -1.32f + (float)i * 0.88f;
        float dip = (i & 1) ? -0.018f : 0.0f;
        float x0 = gx - 0.46f, x1 = gx + 0.46f;
        float y0 = 1.96f + dip, y1 = 2.03f + dip;
        t3d_mb_set_color(b, 0.20f, 0.58f, 0.26f, 1.0f);
        emit_front_quad(b, x0, y0, x1, y1, 0.544f);
        t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
        t3d_mb_push_quad(b, x0, y1, 0.544f, x1, y1, 0.544f, x1, y1, 0.478f, x0, y1, 0.478f);
        t3d_mb_set_color(b, (i & 1) ? 0.98f : 0.98f, (i & 1) ? 0.84f : 0.66f, (i & 1) ? 0.36f : 0.82f, 1.0f);
        emit_front_quad(b, gx - 0.028f, 1.982f + dip, gx + 0.028f, 2.020f + dip, 0.548f);
    }
    t3d_mb_set_color(b, 0.20f, 0.58f, 0.26f, 1.0f);
    emit_front_quad(b, -0.14f, 2.98f, 0.14f, 3.06f, 0.26f);
    t3d_mb_set_color(b, 0.98f, 0.66f, 0.82f, 1.0f);
    emit_front_quad(b, -0.05f, 2.99f, 0.05f, 3.07f, 0.28f);

    // 4. Potted spring tulips on hearth
    draw_flower_pot(b, -1.24f, 0.18f, 0.32f, 0.80f, 0.98f, 0.82f, 0.22f);
    draw_flower_pot(b,  1.44f, 0.18f, 0.34f, 1.05f, 0.96f, 0.42f, 0.64f);
}

static void build_summer_mesh(T3DMeshBuilder *b) {
    t3d_mb_set_texture(b, NULL);

    // 1. Cobalt blue enamel camp mug with silver rim & handle
    t3d_mb_set_color(b, 0.16f, 0.34f, 0.76f, 1.0f);
    draw_cylinder_y(b, 1.08f, 2.02f, 2.18f, 0.24f, 0.056f, 6);
    t3d_mb_set_color(b, 0.88f, 0.90f, 0.94f, 1.0f);
    draw_cylinder_y(b, 1.08f, 2.165f, 2.182f, 0.24f, 0.058f, 6);
    draw_box(b, 1.13f, 2.06f, 0.22f, 1.18f, 2.14f, 0.26f, 1.0f, 1);

    // 2. Brass-lidded firefly jar on right mantle
    t3d_mb_set_color(b, 0.74f, 0.88f, 0.82f, 1.0f);
    draw_cylinder_y(b, 1.40f, 2.02f, 2.18f, 0.26f, 0.054f, 6);
    t3d_mb_set_color(b, 0.78f, 0.60f, 0.22f, 1.0f);
    draw_cylinder_y(b, 1.40f, 2.18f, 2.205f, 0.26f, 0.056f, 6);
    t3d_mb_set_color(b, 0.88f, 1.00f, 0.36f, 1.0f);
    emit_front_quad(b, 1.38f, 2.08f, 1.41f, 2.11f, 0.316f);

    // 3. Sunflower sprig atop picture frame
    t3d_mb_set_color(b, 0.18f, 0.50f, 0.22f, 1.0f);
    emit_front_quad(b, -0.14f, 2.98f, 0.14f, 3.06f, 0.26f);
    t3d_mb_set_color(b, 0.98f, 0.78f, 0.14f, 1.0f);
    emit_front_quad(b, -0.055f, 2.99f, 0.055f, 3.08f, 0.28f);

    // 4. Potted green cabin fern & woven picnic basket on hearth
    t3d_mb_set_color(b, 0.56f, 0.36f, 0.20f, 1.0f);
    draw_cylinder_y(b, -1.24f, 0.18f, 0.28f, 0.32f, 0.068f, 6);
    t3d_mb_set_color(b, 0.14f, 0.54f, 0.22f, 1.0f);
    draw_cone_6(b, -1.24f, 0.27f, 0.44f, 0.32f, 0.11f, 0.02f, false);

    // Woven picnic basket on right hearth
    t3d_mb_set_color(b, 0.78f, 0.58f, 0.30f, 1.0f);
    emit_solid_box(b, 1.30f, 0.18f, 0.26f, 1.62f, 0.34f, 0.48f, false);
    t3d_mb_set_color(b, 0.86f, 0.20f, 0.18f, 1.0f);
    emit_front_quad(b, 1.36f, 0.34f, 1.56f, 0.37f, 0.482f);
}

static void build_thanksgiving_mesh(T3DMeshBuilder *b) {
    t3d_mb_set_texture(b, NULL);

    t3d_mb_set_color(b, 0.74f, 0.30f, 0.22f, 1.0f);
    draw_cylinder_y(b, 1.08f, 2.02f, 2.18f, 0.24f, 0.056f, 6);
    draw_box(b, 1.13f, 2.06f, 0.22f, 1.18f, 2.14f, 0.26f, 1.0f, 1);

    draw_pumpkin(b, -1.24f, 0.18f, 0.32f, 0.086f, 0.070f, 0.94f, 0.68f, 0.16f);
    draw_pumpkin(b,  1.44f, 0.18f, 0.34f, 0.125f, 0.098f, 0.88f, 0.36f, 0.07f);
    draw_pumpkin(b,  1.38f, 2.02f, 0.26f, 0.092f, 0.074f, 0.92f, 0.44f, 0.08f);
}

static void build_christmas_mesh(T3DMeshBuilder *b) {
    t3d_mb_set_texture(b, NULL);

    // 1. Holiday Hot Cocoa Mug
    t3d_mb_set_color(b, 0.78f, 0.12f, 0.14f, 1.0f);
    draw_cylinder_y(b, 1.08f, 2.02f, 2.180f, 0.24f, 0.056f, 6);
    t3d_mb_set_color(b, 0.96f, 0.96f, 0.98f, 1.0f);
    draw_cylinder_y(b, 1.08f, 2.165f, 2.182f, 0.24f, 0.058f, 6);

    // 2. Mini Low-Poly Christmas Tree on Right Hearth
    float tx = 1.46f, ty = 0.18f, tz = 0.48f;
    t3d_mb_set_color(b, 0.76f, 0.14f, 0.14f, 1.0f);
    draw_cylinder_y(b, tx, ty, ty + 0.14f, tz, 0.115f, 6);
    t3d_mb_set_color(b, 0.38f, 0.22f, 0.10f, 1.0f);
    draw_cylinder_y(b, tx, ty + 0.14f, ty + 0.22f, tz, 0.038f, 5);
    t3d_mb_set_color(b, 0.11f, 0.44f, 0.19f, 1.0f);
    draw_cone_6(b, tx, ty + 0.18f, ty + 0.46f, tz, 0.22f, 0.055f, true);
    draw_cone_6(b, tx, ty + 0.38f, ty + 0.64f, tz, 0.17f, 0.040f, false);
    draw_cone_6(b, tx, ty + 0.56f, ty + 0.82f, tz, 0.12f, 0.000f, false);

    // 3. Mug handle & marshmallows
    t3d_mb_set_color(b, 0.96f, 0.96f, 0.98f, 1.0f);
    emit_solid_box(b, 1.13f, 2.06f, 0.22f, 1.18f, 2.14f, 0.26f, true);
    t3d_mb_set_color(b, 0.98f, 0.97f, 0.96f, 1.0f);
    t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
    t3d_mb_push_quad(b, 1.055f, 2.185f, 0.243f, 1.075f, 2.185f, 0.243f,
                        1.075f, 2.185f, 0.223f, 1.055f, 2.185f, 0.223f);
    t3d_mb_push_quad(b, 1.086f, 2.185f, 0.258f, 1.106f, 2.185f, 0.258f,
                        1.106f, 2.185f, 0.238f, 1.086f, 2.185f, 0.238f);

    // Evergreen Pine Mantle Garland (4 wider segments + 4 holly berries)
    for (int i = 0; i < 4; i++) {
        float gx = -1.32f + (float)i * 0.88f;
        float dip = (i & 1) ? -0.020f : 0.0f;
        float x0 = gx - 0.46f, x1 = gx + 0.46f;
        float y0 = 1.955f + dip, y1 = 2.030f + dip;
        t3d_mb_set_color(b, (i & 1) ? 0.10f : 0.14f, (i & 1) ? 0.38f : 0.46f, 0.16f, 1.0f);
        emit_front_quad(b, x0, y0, x1, y1, 0.545f);
        t3d_mb_set_normal(b, 0.0f, 1.0f, 0.0f);
        t3d_mb_push_quad(b, x0, y1, 0.545f, x1, y1, 0.545f, x1, y1, 0.475f, x0, y1, 0.475f);
        t3d_mb_set_normal(b, 0.0f, -1.0f, 0.0f);
        t3d_mb_push_quad(b, x0, y0, 0.475f, x1, y0, 0.475f, x1, y0, 0.545f, x0, y0, 0.545f);
        t3d_mb_set_color(b, 0.88f, 0.12f, 0.16f, 1.0f);
        emit_front_quad(b, gx - 0.022f, 1.982f + dip, gx + 0.022f, 2.018f + dip, 0.548f);
    }

    // Three Knit Christmas Stockings
    static const float stock_x[3] = { -0.56f, 0.00f, 0.56f };
    for (int s = 0; s < 3; s++) {
        float sx = stock_x[s];
        t3d_mb_set_color(b, 0.96f, 0.96f, 0.98f, 1.0f);
        emit_solid_box(b, sx - 0.068f, 1.82f, 0.465f,  sx + 0.068f, 1.90f, 0.535f, false);
        if (s == 1) t3d_mb_set_color(b, 0.14f, 0.52f, 0.22f, 1.0f);
        else        t3d_mb_set_color(b, 0.84f, 0.12f, 0.15f, 1.0f);
        emit_solid_box(b, sx - 0.058f, 1.57f, 0.470f,  sx + 0.058f, 1.82f, 0.525f, true);
        if (s != 2) {
            emit_front_quad(b, sx - 0.058f, 1.50f, sx + 0.135f, 1.60f, 0.526f);
            t3d_mb_set_color(b, 0.92f, 0.76f, 0.24f, 1.0f);
            emit_front_quad(b, sx + 0.095f, 1.50f, sx + 0.142f, 1.60f, 0.528f);
        } else {
            emit_front_quad(b, sx - 0.135f, 1.50f, sx + 0.058f, 1.60f, 0.526f);
            t3d_mb_set_color(b, 0.92f, 0.76f, 0.24f, 1.0f);
            emit_front_quad(b, sx - 0.142f, 1.50f, sx - 0.095f, 1.60f, 0.528f);
        }
    }

    // Holly Leaves & Red Ribbon Bow atop Picture Frame
    t3d_mb_set_color(b, 0.12f, 0.46f, 0.20f, 1.0f);
    emit_front_quad(b, -0.16f, 2.98f, -0.02f, 3.07f, 0.26f);
    emit_front_quad(b,  0.02f, 2.98f,  0.16f, 3.07f, 0.26f);
    t3d_mb_set_color(b, 0.88f, 0.12f, 0.16f, 1.0f);
    emit_front_quad(b, -0.04f, 2.99f,  0.04f, 3.07f, 0.28f);
    emit_front_quad(b, -0.14f, 3.01f, -0.04f, 3.08f, 0.27f);
    emit_front_quad(b,  0.04f, 3.01f,  0.14f, 3.08f, 0.27f);
    emit_front_quad(b, -0.07f, 2.86f, -0.01f, 2.99f, 0.27f);
    emit_front_quad(b,  0.01f, 2.86f,  0.07f, 2.99f, 0.27f);

    // Ornaments & golden star topper on mini Christmas tree
    t3d_mb_set_color(b, 0.92f, 0.14f, 0.16f, 1.0f);
    emit_front_quad(b, tx - 0.16f, ty + 0.28f, tx - 0.11f, ty + 0.33f, tz + 0.13f);
    emit_front_quad(b, tx + 0.08f, ty + 0.48f, tx + 0.13f, ty + 0.53f, tz + 0.14f);
    t3d_mb_set_color(b, 0.96f, 0.82f, 0.20f, 1.0f);
    emit_front_quad(b, tx + 0.12f, ty + 0.29f, tx + 0.17f, ty + 0.34f, tz + 0.13f);
    emit_front_quad(b, tx - 0.10f, ty + 0.52f, tx - 0.05f, ty + 0.57f, tz + 0.11f);
    t3d_mb_set_color(b, 0.32f, 0.78f, 0.96f, 1.0f);
    emit_front_quad(b, tx - 0.03f, ty + 0.36f, tx + 0.02f, ty + 0.41f, tz + 0.18f);
    emit_front_quad(b, tx + 0.03f, ty + 0.64f, tx + 0.07f, ty + 0.68f, tz + 0.09f);
    t3d_mb_set_color(b, 1.00f, 0.86f, 0.22f, 1.0f);
    emit_front_quad(b, tx - 0.028f, ty + 0.82f, tx + 0.028f, ty + 0.89f, tz + 0.020f);
    emit_front_quad(b, tx - 0.044f, ty + 0.84f, tx + 0.044f, ty + 0.87f, tz + 0.015f);

    // 4. Wrapped Gifts
    draw_gift_box(b, 1.40f, 2.02f, 0.26f, 0.14f, 0.12f, 0.14f, 0.14f, 0.56f, 0.24f, 0.96f, 0.82f, 0.22f);
    draw_gift_box(b, 1.96f, 0.00f, 0.64f, 0.22f, 0.18f, 0.20f, 0.16f, 0.36f, 0.82f, 0.96f, 0.82f, 0.22f);
    draw_gift_box(b, 2.24f, 0.00f, 0.58f, 0.18f, 0.15f, 0.18f, 0.84f, 0.14f, 0.18f, 0.96f, 0.96f, 0.98f);
}

static void marshmallow_tip_pos(const MarshmallowState *roast, float time,
                                float *ox, float *oy, float *oz) {
    float e = sc_clampf(roast->extend, 0.0f, 1.0f);
    float s = e * e * (3.0f - 2.0f * e);
    float sway_x = 0.032f * sinf(time * 1.4f);
    float sway_y = 0.018f * cosf(time * 2.1f);
    *ox = 1.20f + ((0.14f + sway_x) - 1.20f) * s;
    *oy = 0.48f + ((0.86f + sway_y) - 0.48f) * s;
    *oz = 1.75f + (-0.32f - 1.75f) * s;
}

static void draw_skewer_cylinder(float bx, float by, float bz,
                                 float ux, float uy, float uz,
                                 float rx, float ry, float rz,
                                 float upx, float upy, float upz,
                                 float t0, float t1, float rad, float spin,
                                 int sides, float cr, float cg, float cb, bool cap_ends) {
    t3d_dyn_begin_quads(NULL, T3D_ROOM_SCALE, false, true, true, true);
    for (int i = 0; i < sides; i++) {
        float a0 = spin + (float)i / (float)sides * 6.2831853f;
        float a1 = spin + (float)(i + 1) / (float)sides * 6.2831853f;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);
        float nx0 = rx * c0 + upx * s0, ny0 = ry * c0 + upy * s0, nz0 = rz * c0 + upz * s0;
        float nx1 = rx * c1 + upx * s1, ny1 = ry * c1 + upy * s1, nz1 = rz * c1 + upz * s1;

        T3DBuildVert q0 = { bx + ux * t0 + nx0 * rad, by + uy * t0 + ny0 * rad, bz + uz * t0 + nz0 * rad, nx0, ny0, nz0, 0,0, cr, cg, cb, 1.0f };
        T3DBuildVert q1 = { bx + ux * t0 + nx1 * rad, by + uy * t0 + ny1 * rad, bz + uz * t0 + nz1 * rad, nx1, ny1, nz1, 0,0, cr, cg, cb, 1.0f };
        T3DBuildVert q2 = { bx + ux * t1 + nx1 * rad, by + uy * t1 + ny1 * rad, bz + uz * t1 + nz1 * rad, nx1, ny1, nz1, 0,0, cr, cg, cb, 1.0f };
        T3DBuildVert q3 = { bx + ux * t1 + nx0 * rad, by + uy * t1 + ny0 * rad, bz + uz * t1 + nz0 * rad, nx0, ny0, nz0, 0,0, cr, cg, cb, 1.0f };
        t3d_dyn_quad_vc(&q0, &q1, &q2, &q3);
    }
    t3d_dyn_end_quads();

    if (cap_ends) {
        t3d_dyn_begin_tris(NULL, T3D_ROOM_SCALE, false, true, true, true);
        float cx0 = bx + ux * t0, cy0 = by + uy * t0, cz0 = bz + uz * t0;
        float cx1 = bx + ux * t1, cy1 = by + uy * t1, cz1 = bz + uz * t1;
        for (int i = 0; i < sides; i++) {
            float a0 = spin + (float)i / (float)sides * 6.2831853f;
            float a1 = spin + (float)(i + 1) / (float)sides * 6.2831853f;
            float c0 = cosf(a0), s0 = sinf(a0);
            float c1 = cosf(a1), s1 = sinf(a1);
            float nx0 = rx * c0 + upx * s0, ny0 = ry * c0 + upy * s0, nz0 = rz * c0 + upz * s0;
            float nx1 = rx * c1 + upx * s1, ny1 = ry * c1 + upy * s1, nz1 = rz * c1 + upz * s1;

            T3DBuildVert f0 = { cx1, cy1, cz1, ux, uy, uz, 0,0, cr, cg, cb, 1.0f };
            T3DBuildVert f1 = { cx1 + nx0 * rad, cy1 + ny0 * rad, cz1 + nz0 * rad, ux, uy, uz, 0,0, cr, cg, cb, 1.0f };
            T3DBuildVert f2 = { cx1 + nx1 * rad, cy1 + ny1 * rad, cz1 + nz1 * rad, ux, uy, uz, 0,0, cr, cg, cb, 1.0f };
            t3d_dyn_tri_vc(&f0, &f1, &f2);

            T3DBuildVert b0 = { cx0, cy0, cz0, -ux, -uy, -uz, 0,0, cr * 0.92f, cg * 0.92f, cb * 0.92f, 1.0f };
            T3DBuildVert b1 = { cx0 + nx1 * rad, cy0 + ny1 * rad, cz0 + nz1 * rad, -ux, -uy, -uz, 0,0, cr * 0.92f, cg * 0.92f, cb * 0.92f, 1.0f };
            T3DBuildVert b2 = { cx0 + nx0 * rad, cy0 + ny0 * rad, cz0 + nz0 * rad, -ux, -uy, -uz, 0,0, cr * 0.92f, cg * 0.92f, cb * 0.92f, 1.0f };
            t3d_dyn_tri_vc(&b0, &b1, &b2);
        }
        t3d_dyn_end_tris();
    }
}

static void draw_marshmallow_skewer(const MarshmallowState *roast, const FireSystem *fs) {
    if (!roast || roast->extend <= 0.02f) return;

    float tx, ty, tz;
    marshmallow_tip_pos(roast, fs->time, &tx, &ty, &tz);

    // Axis from handle toward marshmallow tip (pointing gently up/left/into hearth)
    float ux = -0.49f, uy = 0.17f, uz = -0.855f;
    float ulen = sqrtf(ux * ux + uy * uy + uz * uz);
    ux /= ulen; uy /= ulen; uz /= ulen;

    // Right-handed basis (r, up, u) with up = u x r => r x up = u
    float rx = uz, ry = 0.0f, rz = -ux;
    float rlen = sqrtf(rx * rx + rz * rz);
    rx /= rlen; rz /= rlen;
    float upx = uy * rz - uz * ry;
    float upy = uz * rx - ux * rz;
    float upz = ux * ry - uy * rx;

    // Slender wooden skewer stick (-1.55m .. +0.02m around tip)
    draw_skewer_cylinder(tx, ty, tz, ux, uy, uz, rx, ry, rz, upx, upy, upz,
                         -1.55f, 0.02f, 0.011f, 0.0f, 4, 0.62f, 0.42f, 0.24f, false);

    // Interpolate marshmallow color across toast stages
    float tl = roast->toast_level;
    float mr, mg, mb;
    if (tl < 0.68f) {
        float k = sc_clampf(tl / 0.68f, 0.0f, 1.0f);
        mr = 0.99f * (1.0f - k) + 0.84f * k;
        mg = 0.97f * (1.0f - k) + 0.54f * k;
        mb = 0.92f * (1.0f - k) + 0.20f * k;
    } else if (tl < 1.15f) {
        float k = sc_clampf((tl - 0.68f) / 0.47f, 0.0f, 1.0f);
        mr = 0.84f * (1.0f - k) + 0.36f * k;
        mg = 0.54f * (1.0f - k) + 0.18f * k;
        mb = 0.20f * (1.0f - k) + 0.07f * k;
    } else {
        float ember = roast->on_fire ? (0.14f + 0.10f * sinf(fs->time * 14.0f)) : 0.0f;
        mr = 0.15f + ember;
        mg = 0.12f + ember * 0.30f;
        mb = 0.11f;
    }

    // Plump 6-sided marshmallow centered near the tip
    draw_skewer_cylinder(tx, ty, tz, ux, uy, uz, rx, ry, rz, upx, upy, upz,
                         -0.065f, 0.045f, 0.044f, roast->spin, 6, mr, mg, mb, true);
}

static void draw_marshmallow_flame(const SceneTextures *tex, const MarshmallowState *roast,
                                   const FireSystem *fs, float cam_x, float cam_z) {
    if (!roast || roast->extend <= 0.02f || !roast->on_fire) return;

    float tx, ty, tz;
    marshmallow_tip_pos(roast, fs->time, &tx, &ty, &tz);

    float vx = cam_x - tx;
    float vz = cam_z - tz;
    float len = sqrtf(vx * vx + vz * vz);
    if (len < 0.001f) len = 1.0f;
    float rx = vz / len;
    float rz = -vx / len;

    float t = fs->time;
    float flk = 0.90f + 0.14f * sinf(t * 16.0f);
    float sway = 0.012f * sinf(t * 11.0f);

    t3d_dyn_begin_quads(&tex->glow, T3D_ROOM_SCALE, true, false, false, false);
    {
        float gs = 0.16f * flk;
        T3DBuildVert v0 = { tx - rx * gs, ty - 0.06f, tz - rz * gs, 0,0,1, 0,1, 1.0f, 0.56f, 0.14f, 0.42f };
        T3DBuildVert v1 = { tx + rx * gs, ty - 0.06f, tz + rz * gs, 0,0,1, 1,1, 1.0f, 0.56f, 0.14f, 0.42f };
        T3DBuildVert v2 = { tx + rx * gs, ty + 0.18f, tz + rz * gs, 0,0,1, 1,0, 1.0f, 0.56f, 0.14f, 0.42f };
        T3DBuildVert v3 = { tx - rx * gs, ty + 0.18f, tz - rz * gs, 0,0,1, 0,0, 1.0f, 0.56f, 0.14f, 0.42f };
        t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
    }
    t3d_dyn_end_quads();

    t3d_dyn_begin_quads(&tex->flame, T3D_ROOM_SCALE, true, false, false, false);
    {
        float w = 0.068f * flk;
        float h = 0.155f * (0.88f + 0.22f * sinf(t * 19.0f));
        T3DBuildVert v0 = { (tx + sway) - rx * w, ty - 0.01f, tz - rz * w, 0,0,1, 0,1, 1.0f, 0.72f, 0.22f, 0.94f };
        T3DBuildVert v1 = { (tx + sway) + rx * w, ty - 0.01f, tz + rz * w, 0,0,1, 1,1, 1.0f, 0.72f, 0.22f, 0.94f };
        T3DBuildVert v2 = { (tx + sway) + rx * w, ty + h,     tz + rz * w, 0,0,1, 1,0, 1.0f, 0.92f, 0.48f, 0.94f };
        T3DBuildVert v3 = { (tx + sway) - rx * w, ty + h,     tz - rz * w, 0,0,1, 0,0, 1.0f, 0.92f, 0.48f, 0.94f };
        t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
    }
    t3d_dyn_end_quads();
}

void scene_render(const SceneTextures *tex,
                  const FireSystem *fs,
                  const Turkey *turkey,
                  Season season,
                  const SnowglobeTransition *sg,
                  const MarshmallowState *roast,
                  bool rain_enabled,
                  float cam_x, float cam_y, float cam_z,
                  float dim_factor) {
    const FlameTheme *th = &FLAME_THEMES[fs->theme_idx];
    float eff = fire_effective_intensity(fs);
    float fl  = fs->flicker * dim_factor;

    t3d_light_set_exposure(1.0f);

    // Shared Cool Cabin-Window Moonlight + Warm Hearth Under-Bounce vectors
    float moon_cloud = (rain_enabled ? 0.72f : 1.0f) * (0.90f + 0.10f * sinf(fs->time * 0.28f)) * dim_factor;
    float moon_r, moon_g, moon_b;
    switch (season) {
        case SEASON_SPRING:    moon_r = 0.36f; moon_g = 0.42f; moon_b = 0.82f; break;
        case SEASON_SUMMER:    moon_r = 0.32f; moon_g = 0.44f; moon_b = 0.80f; break;
        case SEASON_CHRISTMAS: moon_r = 0.34f; moon_g = 0.48f; moon_b = 0.86f; break;
        case SEASON_THANKSGIVING:
        default:               moon_r = 0.26f; moon_g = 0.40f; moon_b = 0.78f; break;
    }
    T3DVec3 dir_moon = {{ -0.84f, 0.42f, 0.34f }};
    t3d_vec3_norm(&dir_moon);
    T3DVec3 dir_under = {{ fs->flicker_x * 1.4f, -0.86f, 0.50f }};
    t3d_vec3_norm(&dir_under);

    // Pass A: Baked radial-firelight room/hearth + 3-light warm fire / cool moonlight / under-bounce rig
    float fire_gain = (0.85f + 0.25f * eff) * fl;
    uint8_t amb_base[4] = {
        f_to_u8(fminf(1.0f, 0.90f * ((0.12f + 0.06f * th->light_r) * dim_factor + 0.28f * dim_factor + 0.42f * th->light_r * fire_gain))),
        f_to_u8(fminf(1.0f, 0.90f * ((0.10f + 0.05f * th->light_g) * dim_factor + 0.25f * dim_factor + 0.38f * th->light_g * fire_gain))),
        f_to_u8(fminf(1.0f, 0.90f * ((0.10f + 0.04f * th->light_b) * dim_factor + 0.22f * dim_factor + 0.34f * th->light_b * fire_gain))),
        255
    };
    uint8_t diff_base[4] = {
        f_to_u8(fminf(1.0f, th->light_r * 0.38f * fire_gain)),
        f_to_u8(fminf(1.0f, th->light_g * 0.36f * fire_gain)),
        f_to_u8(fminf(1.0f, th->light_b * 0.34f * fire_gain)),
        0
    };
    uint8_t diff_moon_base[4] = {
        f_to_u8(fminf(1.0f, moon_r * 0.38f * moon_cloud)),
        f_to_u8(fminf(1.0f, moon_g * 0.38f * moon_cloud)),
        f_to_u8(fminf(1.0f, moon_b * 0.38f * moon_cloud)),
        0
    };
    uint8_t diff_under_base[4] = {
        f_to_u8(fminf(1.0f, th->light_r * 0.42f * fire_gain)),
        f_to_u8(fminf(1.0f, th->light_g * 0.42f * fire_gain)),
        f_to_u8(fminf(1.0f, th->light_b * 0.42f * fire_gain)),
        0
    };
    T3DVec3 dir_base = {{ fs->flicker_x * 2.4f, 0.45f + fs->flicker_y * 1.6f, 0.88f }};
    t3d_vec3_norm(&dir_base);

    t3d_light_set_ambient(amb_base);
    t3d_light_set_directional(0, diff_base, &dir_base);
    t3d_light_set_directional(1, diff_moon_base, &dir_moon);
    t3d_light_set_directional(2, diff_under_base, &dir_under);
    t3d_light_set_count(3);

    t3d_matrix_push(t3d_gfx_room_mat());
    t3d_mesh_draw(&tex->base_mesh);

    // Pass B: Crisp 3D 3-light rig for seasonal props, burning logs, snowglobe & visitors
    uint8_t amb_prop[4] = {
        f_to_u8(fminf(1.0f, 0.88f * ((0.12f + 0.06f * th->light_r) * dim_factor + 0.22f * dim_factor + 0.18f * th->light_r * fl))),
        f_to_u8(fminf(1.0f, 0.88f * ((0.10f + 0.05f * th->light_g) * dim_factor + 0.20f * dim_factor + 0.16f * th->light_g * fl))),
        f_to_u8(fminf(1.0f, 0.88f * ((0.10f + 0.04f * th->light_b) * dim_factor + 0.20f * dim_factor + 0.14f * th->light_b * fl))),
        255
    };
    uint8_t diff_prop[4] = {
        f_to_u8(fminf(1.0f, th->light_r * (0.74f + 0.24f * eff) * fl)),
        f_to_u8(fminf(1.0f, th->light_g * (0.74f + 0.24f * eff) * fl)),
        f_to_u8(fminf(1.0f, th->light_b * (0.74f + 0.24f * eff) * fl)),
        0
    };
    uint8_t diff_moon_prop[4] = {
        f_to_u8(fminf(1.0f, moon_r * 0.56f * moon_cloud)),
        f_to_u8(fminf(1.0f, moon_g * 0.56f * moon_cloud)),
        f_to_u8(fminf(1.0f, moon_b * 0.56f * moon_cloud)),
        0
    };
    uint8_t diff_under_prop[4] = {
        f_to_u8(fminf(1.0f, th->light_r * 0.48f * (0.65f + 0.35f * eff) * fl)),
        f_to_u8(fminf(1.0f, th->light_g * 0.48f * (0.65f + 0.35f * eff) * fl)),
        f_to_u8(fminf(1.0f, th->light_b * 0.48f * (0.65f + 0.35f * eff) * fl)),
        0
    };
    T3DVec3 dir_prop = {{ fs->flicker_x * 2.2f - 0.18f, 0.52f + fs->flicker_y * 1.4f, 0.83f }};
    t3d_vec3_norm(&dir_prop);

    t3d_light_set_ambient(amb_prop);
    t3d_light_set_directional(0, diff_prop, &dir_prop);
    t3d_light_set_directional(1, diff_moon_prop, &dir_moon);
    t3d_light_set_directional(2, diff_under_prop, &dir_under);
    t3d_light_set_count(3);

    switch (season) {
        case SEASON_SPRING:    t3d_mesh_draw(&tex->spring_mesh);       break;
        case SEASON_SUMMER:    t3d_mesh_draw(&tex->summer_mesh);       break;
        case SEASON_CHRISTMAS: t3d_mesh_draw(&tex->christmas_mesh);    break;
        case SEASON_THANKSGIVING:
        default:               t3d_mesh_draw(&tex->thanksgiving_mesh); break;
    }

    // 1b. Warm candlelit seasonal painting above the mantle
    {
        float art_warm = (0.82f + 0.06f * fs->flicker) * dim_factor;
        float pr = fminf(1.0f, art_warm * 1.04f);
        float pg = fminf(1.0f, art_warm * 0.99f);
        float pb = fminf(1.0f, art_warm * 0.96f);
        const T3DTexture *paint_tex;
        switch (season) {
            case SEASON_SPRING:    paint_tex = &tex->painting_spring; break;
            case SEASON_SUMMER:    paint_tex = &tex->painting_summer; break;
            case SEASON_CHRISTMAS: paint_tex = &tex->painting_xmas;   break;
            case SEASON_THANKSGIVING:
            default:               paint_tex = &tex->painting;        break;
        }
        t3d_dyn_begin_quads(paint_tex, T3D_ROOM_SCALE, false, true, true, false);
        T3DBuildVert v0 = { -0.82f, 2.20f, 0.19f, 0, 0, 1, 0.0f, 1.0f, pr, pg, pb, 1.0f };
        T3DBuildVert v1 = {  0.82f, 2.20f, 0.19f, 0, 0, 1, 1.0f, 1.0f, pr, pg, pb, 1.0f };
        T3DBuildVert v2 = {  0.82f, 2.96f, 0.19f, 0, 0, 1, 1.0f, 0.0f, pr, pg, pb, 1.0f };
        T3DBuildVert v3 = { -0.82f, 2.96f, 0.19f, 0, 0, 1, 0.0f, 0.0f, pr, pg, pb, 1.0f };
        t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
        t3d_dyn_end_quads();
    }

    // 2. Animated cabin window sky
    draw_cabin_window_sky(tex, fs->time, rain_enabled, season, dim_factor, cam_x);

    // 3. Glowing pulsing coal & ash bed
    draw_coal_mound(tex, fs, dim_factor);

    // 4. Pre-compiled burning firewood logs
    t3d_mesh_draw(&tex->fire_logs_mesh);

    // 4a. Interactive 3D marshmallow roasting skewer
    draw_marshmallow_skewer(roast, fs);

    // Pop room matrix before world-space objects that push their own matrix from matCamera
    t3d_matrix_pop(1);

    // Dynamic tossed logs
    float glow_amt = (0.45f + 0.45f * eff) * (0.90f + 0.12f * fs->flicker);
    for (int i = 0; i < MAX_TOSSED_LOGS; i++) {
        const TossedLog *tl = &fs->tossed_logs[i];
        if (!tl->active) continue;
        float life_left = 1.0f - (tl->burn_age / tl->burn_life);
        float rad = 0.072f * (0.45f + 0.55f * life_left);
        float lg  = glow_amt * (tl->landed ? (0.35f + 0.65f * (1.0f - life_left)) : 0.15f);
        draw_dynamic_log(tex, tl->x, tl->y, tl->z, 0.70f, rad, tl->rot_y, tl->rot_z, lg, th);
    }

    // 4b. Left hearth dynamic prop
    {
        t3d_matrix_push_pos(1);
        t3d_xf_reset();
        if (turkey->pie_timer > 0.0f && turkey->pie_pop_y > 0.0005f) {
            t3d_xf_translate(0.0f, turkey->pie_pop_y, 0.0f);
        }
        t3d_xf_apply(T3D_PROP_SCALE);
        if (turkey->pie_timer > 0.0f) {
            switch (season) {
                case SEASON_SPRING:    t3d_mesh_draw(&tex->left_berry_tart_mesh); break;
                case SEASON_SUMMER:    t3d_mesh_draw(&tex->left_smores_mesh);     break;
                case SEASON_CHRISTMAS: t3d_mesh_draw(&tex->left_cookies_mesh);    break;
                case SEASON_THANKSGIVING:
                default:               t3d_mesh_draw(&tex->left_pie_mesh);        break;
            }
        } else {
            switch (season) {
                case SEASON_SPRING:    t3d_mesh_draw(&tex->left_spring_mesh);  break;
                case SEASON_SUMMER:    t3d_mesh_draw(&tex->left_summer_mesh);  break;
                case SEASON_CHRISTMAS: t3d_mesh_draw(&tex->left_gift_mesh);    break;
                case SEASON_THANKSGIVING:
                default:               t3d_mesh_draw(&tex->left_pumpkin_mesh); break;
            }
        }
        t3d_matrix_pop(1);
    }

    // 4c. 3D Mantle Snowglobe base & miniature interior diorama
    draw_snowglobe_opaque(tex, season, sg);

    // 5. Hearth visitors & Mini Snowman!
    turkey_render(turkey);

    // 6-8a. Room-space translucent effects: flames, magic fx, seasonal lights
    t3d_matrix_push(t3d_gfx_room_mat());
    fire_render_effects(fs, &tex->flame, &tex->glow, &tex->spark, cam_x, cam_y, cam_z);
    draw_marshmallow_flame(tex, roast, fs, cam_x, cam_z);
    turkey_render_magic_fx(turkey, &tex->spark, cam_x, cam_z);
    if (season == SEASON_CHRISTMAS) {
        draw_christmas_lights(tex, fs->time, dim_factor, cam_x, cam_z);
    } else if (season == SEASON_SUMMER) {
        draw_summer_fireflies(tex, fs->time, dim_factor, cam_x, cam_z);
    }
    t3d_matrix_pop(1);

    // 8b. 3D Snowglobe translucent glass & swirling flakes
    draw_snowglobe_glass(tex, fs->time, season, sg, cam_x, cam_z);

    t3d_tri_sync();
}
