#include "scene.h"
#include <libdragon.h>
#include <GL/gl_integration.h>
#include <math.h>

static GLuint load_n64_tex(const char *path, bool repeat) {
    sprite_t *sp = sprite_load(path);
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    rdpq_texparms_t parms = {
        .s = { .repeats = repeat ? REPEAT_INFINITE : 1, .mirror = false },
        .t = { .repeats = repeat ? REPEAT_INFINITE : 1, .mirror = false },
    };
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSpriteTextureN64(GL_TEXTURE_2D, sp, &parms);
    // Note: libdragon OpenGL keeps pointer to sprite surface data, so keep sprite alive in RDRAM
    return id;
}

static void build_static_scene_list(const SceneTextures *tex);

void scene_load_textures(SceneTextures *tex) {
    tex->stone       = load_n64_tex("rom:/stone.sprite",       true);
    tex->brick       = load_n64_tex("rom:/brick.sprite",       true);
    tex->wood_mantle = load_n64_tex("rom:/wood_mantle.sprite", true);
    tex->floor       = load_n64_tex("rom:/floor.sprite",       true);
    tex->bark        = load_n64_tex("rom:/bark.sprite",        true);
    tex->log_end     = load_n64_tex("rom:/log_end.sprite",     false);
    tex->coals       = load_n64_tex("rom:/coals.sprite",       true);
    tex->rug         = load_n64_tex("rom:/rug.sprite",         false);
    tex->iron        = load_n64_tex("rom:/iron.sprite",        true);
    tex->painting    = load_n64_tex("rom:/painting.sprite",    false);
    tex->flame       = load_n64_tex("rom:/flame.sprite",       false);
    tex->glow        = load_n64_tex("rom:/glow.sprite",        false);
    tex->spark       = load_n64_tex("rom:/spark.sprite",       false);

    tex->static_list = glGenLists(1);
    glNewList(tex->static_list, GL_COMPILE);
    build_static_scene_list(tex);
    glEndList();
}

/*
 * Draws a subdivided axis-aligned planar grid so per-vertex N64 Gouraud
 * lighting creates smooth radial firelight pools across walls, stones & floors.
 */
static void draw_grid_quad(float x0, float y0, float z0,
                           float ux, float uy, float uz,
                           float vx, float vy, float vz,
                           float nx, float ny, float nz,
                           float u_max, float v_max,
                           int seg_u, int seg_v) {
    glNormal3f(nx, ny, nz);
    glBegin(GL_QUADS);
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

            glTexCoord2f(tu0 * u_max, (1.0f - tv0) * v_max); glVertex3f(p00x, p00y, p00z);
            glTexCoord2f(tu1 * u_max, (1.0f - tv0) * v_max); glVertex3f(p10x, p10y, p10z);
            glTexCoord2f(tu1 * u_max, (1.0f - tv1) * v_max); glVertex3f(p11x, p11y, p11z);
            glTexCoord2f(tu0 * u_max, (1.0f - tv1) * v_max); glVertex3f(p01x, p01y, p01z);
        }
    }
    glEnd();
}

static void draw_box(float x0, float y0, float z0,
                     float x1, float y1, float z1,
                     float uv_scale, int subdiv) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float dz = z1 - z0;
    int su = subdiv;
    int sv = subdiv;

    // Front (+Z)
    draw_grid_quad(x0, y0, z1,  dx, 0, 0,  0, dy, 0,  0, 0, 1,
                   dx * uv_scale, dy * uv_scale, su, sv);
    // Left (-X)
    draw_grid_quad(x0, y0, z0,  0, 0, dz,  0, dy, 0, -1, 0, 0,
                   dz * uv_scale, dy * uv_scale, 1, sv);
    // Right (+X)
    draw_grid_quad(x1, y0, z1,  0, 0, -dz, 0, dy, 0,  1, 0, 0,
                   dz * uv_scale, dy * uv_scale, 1, sv);
    // Top (+Y)
    draw_grid_quad(x0, y1, z1,  dx, 0, 0,  0, 0, -dz, 0, 1, 0,
                   dx * uv_scale, dz * uv_scale, su, 1);
    // Bottom (-Y) only needed for elevated elements (skip when flush on floor y0 <= 0.01)
    if (y0 > 0.01f) {
        draw_grid_quad(x0, y0, z0,  dx, 0, 0,  0, 0, dz,  0, -1, 0,
                       dx * uv_scale, dz * uv_scale, 1, 1);
    }
}

/*
 * Draws a low-poly 7-sided cylindrical log along the local X axis,
 * with warm charred underbelly glow responding to fire intensity!
 */
static const float LOG_SIN[7] = { 0.0f, 0.8660254f,  0.8660254f,  0.0f, -0.8660254f, -0.8660254f, 0.0f };
static const float LOG_COS[7] = { 1.0f, 0.5000000f, -0.5000000f, -1.0f, -0.5000000f,  0.5000000f, 1.0f };

static void draw_log_3d(const SceneTextures *tex,
                        float cx, float cy, float cz,
                        float length, float radius,
                        float rot_y, float rot_z,
                        float glow_amt, const FlameTheme *th) {
    const int sides = 6;
    float half_l = length * 0.5f;

    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glRotatef(rot_y, 0.0f, 1.0f, 0.0f);
    glRotatef(rot_z, 0.0f, 0.0f, 1.0f);

    // Bark cylinder (precomputed unit normals — zero sinf/cosf per frame!)
    glBindTexture(GL_TEXTURE_2D, tex->bark);
    glBegin(GL_QUADS);
    for (int i = 0; i < sides; i++) {
        float ny0 = LOG_SIN[i],     nz0 = LOG_COS[i];
        float ny1 = LOG_SIN[i + 1], nz1 = LOG_COS[i + 1];
        float y0 = ny0 * radius, z0 = nz0 * radius;
        float y1 = ny1 * radius, z1 = nz1 * radius;

        float g0 = (ny0 < 0.1f) ? glow_amt * (0.25f - ny0 * 0.75f) : 0.0f;
        float g1 = (ny1 < 0.1f) ? glow_amt * (0.25f - ny1 * 0.75f) : 0.0f;

        float v0 = (float)i * (2.0f / 6.0f);
        float v1 = (float)(i + 1) * (2.0f / 6.0f);

        glColor3f(0.76f + th->coal_r * g0, 0.68f + th->coal_g * g0, 0.62f + th->coal_b * g0);
        glNormal3f(0.0f, ny0, nz0);
        glTexCoord2f(0.0f, v0); glVertex3f(-half_l, y0, z0);
        glTexCoord2f(2.0f, v0); glVertex3f( half_l, y0, z0);

        glColor3f(0.76f + th->coal_r * g1, 0.68f + th->coal_g * g1, 0.62f + th->coal_b * g1);
        glNormal3f(0.0f, ny1, nz1);
        glTexCoord2f(2.0f, v1); glVertex3f( half_l, y1, z1);
        glTexCoord2f(0.0f, v1); glVertex3f(-half_l, y1, z1);
    }
    glEnd();

    // Cut log ends (growth rings)
    glColor3f(0.90f, 0.84f, 0.78f);
    glBindTexture(GL_TEXTURE_2D, tex->log_end);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0.5f, 0.5f); glVertex3f(half_l, 0.0f, 0.0f);
    for (int i = sides; i >= 0; i--) {
        glTexCoord2f(0.5f + 0.48f * LOG_COS[i], 0.5f - 0.48f * LOG_SIN[i]);
        glVertex3f(half_l, LOG_SIN[i] * radius, LOG_COS[i] * radius);
    }
    glEnd();

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0.5f, 0.5f); glVertex3f(-half_l, 0.0f, 0.0f);
    for (int i = 0; i <= sides; i++) {
        glTexCoord2f(0.5f + 0.48f * LOG_COS[i], 0.5f - 0.48f * LOG_SIN[i]);
        glVertex3f(-half_l, LOG_SIN[i] * radius, LOG_COS[i] * radius);
    }
    glEnd();

    glPopMatrix();
}

static void draw_cylinder_y(float cx, float y0, float y1, float cz, float r, int sides) {
    glBegin(GL_QUADS);
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / (float)sides * 6.2831853f;
        float a1 = (float)(i + 1) / (float)sides * 6.2831853f;
        float nx0 = cosf(a0), nz0 = sinf(a0);
        float nx1 = cosf(a1), nz1 = sinf(a1);
        glNormal3f(nx0, 0.0f, nz0);
        glTexCoord2f((float)i / sides, 0.0f); glVertex3f(cx + nx0 * r, y0, cz + nz0 * r);
        glTexCoord2f((float)i / sides, 1.0f); glVertex3f(cx + nx0 * r, y1, cz + nz0 * r);
        glNormal3f(nx1, 0.0f, nz1);
        glTexCoord2f((float)(i + 1) / sides, 1.0f); glVertex3f(cx + nx1 * r, y1, cz + nz1 * r);
        glTexCoord2f((float)(i + 1) / sides, 0.0f); glVertex3f(cx + nx1 * r, y0, cz + nz1 * r);
    }
    glEnd();

    // Top disk
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0.5f, 0.5f); glVertex3f(cx, y1, cz);
    for (int i = sides; i >= 0; i--) {
        float a = (float)i / (float)sides * 6.2831853f;
        glTexCoord2f(0.5f + 0.4f * cosf(a), 0.5f + 0.4f * sinf(a));
        glVertex3f(cx + cosf(a) * r, y1, cz + sinf(a) * r);
    }
    glEnd();
}

static void draw_coal_mound(const SceneTextures *tex, const FireSystem *fs, float dim) {
    const FlameTheme *th = &FLAME_THEMES[fs->theme_idx];
    float eff = fire_effective_intensity(fs);
    float pulse = (0.58f + 0.40f * eff) * (0.88f + 0.14f * fs->flicker) * dim;

    // Coals are self-illuminated so they glow deep orange/red even when embers are low!
    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, tex->coals);

    // Sculpted low-poly ember mound (center apex + 10 perimeter vertices)
    const int ring = 10;
    float cx = 0.0f, cz = -0.46f;
    float rx = 0.62f, rz = 0.36f;

    glBegin(GL_TRIANGLE_FAN);
    // Hot glowing core center
    glColor3f(
        fminf(1.0f, 0.35f + th->coal_r * pulse * 1.15f),
        fminf(1.0f, 0.18f + th->coal_g * pulse * 1.10f),
        fminf(1.0f, 0.12f + th->coal_b * pulse * 0.95f)
    );
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(cx, 0.31f, cz);

    for (int i = ring; i >= 0; i--) {
        float a = (float)i / (float)ring * 6.2831853f;
        float px = cx + cosf(a) * rx;
        float pz = cz + sinf(a) * rz;
        // Cooler sooty rim around the edge of the ash bed
        glColor3f(
            fminf(1.0f, 0.18f + th->coal_r * pulse * 0.45f),
            fminf(1.0f, 0.12f + th->coal_g * pulse * 0.35f),
            fminf(1.0f, 0.10f + th->coal_b * pulse * 0.30f)
        );
        glTexCoord2f(1.0f + cosf(a) * 1.1f, 1.0f + sinf(a) * 1.1f);
        glVertex3f(px, 0.202f, pz);
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

static void draw_cabin_window_sky(const SceneTextures *tex, float time, bool rain_enabled, float dim) {
    // Cozy 4-pane cabin window sky/stars/rain on the left timber wall
    float wx0 = -2.72f, wx1 = -1.86f;
    float wy0 =  1.14f, wy1 =  2.26f;
    float wz  = -0.155f;

    // 1. Night sky glass backdrop (unlit deep midnight gradient)
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    if (rain_enabled) {
        glColor3f(0.05f * dim, 0.08f * dim, 0.15f * dim);
        glVertex3f(wx0, wy0, wz);
        glVertex3f(wx1, wy0, wz);
        glColor3f(0.02f * dim, 0.04f * dim, 0.09f * dim);
        glVertex3f(wx1, wy1, wz);
        glVertex3f(wx0, wy1, wz);
    } else {
        glColor3f(0.07f * dim, 0.11f * dim, 0.22f * dim);
        glVertex3f(wx0, wy0, wz);
        glVertex3f(wx1, wy0, wz);
        glColor3f(0.02f * dim, 0.04f * dim, 0.11f * dim);
        glVertex3f(wx1, wy1, wz);
        glVertex3f(wx0, wy1, wz);
    }
    glEnd();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    if (!rain_enabled) {
        glBindTexture(GL_TEXTURE_2D, tex->spark);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        static const float stars[10][3] = {
            {-2.58f, 2.08f, 0.0f}, {-2.38f, 1.92f, 1.2f}, {-2.06f, 2.12f, 2.4f},
            {-1.98f, 1.84f, 0.8f}, {-2.60f, 1.56f, 3.1f}, {-2.44f, 1.42f, 1.9f},
            {-2.12f, 1.62f, 4.2f}, {-1.96f, 1.38f, 2.7f}, {-2.22f, 2.02f, 5.0f},
            {-2.52f, 1.78f, 3.7f},
        };
        for (int s = 0; s < 10; s++) {
            float tw = 0.45f + 0.55f * sinf(time * (1.8f + (s % 4) * 0.5f) + stars[s][2]);
            float sz = 0.018f + 0.008f * (s % 2);
            glColor4f(0.82f * dim, 0.90f * dim, 1.00f * dim, tw * 0.85f);
            glTexCoord2f(0, 1); glVertex3f(stars[s][0] - sz, stars[s][1] - sz, wz + 0.022f);
            glTexCoord2f(1, 1); glVertex3f(stars[s][0] + sz, stars[s][1] - sz, wz + 0.022f);
            glTexCoord2f(1, 0); glVertex3f(stars[s][0] + sz, stars[s][1] + sz, wz + 0.022f);
            glTexCoord2f(0, 0); glVertex3f(stars[s][0] - sz, stars[s][1] + sz, wz + 0.022f);
        }
        glEnd();
    } else {
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        for (int r = 0; r < 18; r++) {
            float seed = (float)r * 0.371f;
            float speed = 1.45f + (r % 5) * 0.22f;
            float frac = fmodf(time * speed + seed * 3.1f, 1.0f);
            float rx = wx0 + 0.06f + fmodf(seed * 7.3f + frac * 0.22f, (wx1 - wx0 - 0.12f));
            float ry = wy1 - frac * (wy1 - wy0);
            float rlen = 0.11f;
            if (ry - rlen < wy0) continue;
            float alpha = 0.22f + 0.18f * ((r % 3) * 0.5f);
            glColor4f(0.55f * dim, 0.72f * dim, 0.92f * dim, alpha);
            glVertex3f(rx - 0.005f, ry,        wz + 0.022f);
            glVertex3f(rx + 0.005f, ry,        wz + 0.022f);
            glVertex3f(rx - 0.025f, ry - rlen, wz + 0.022f);
            glVertex3f(rx - 0.035f, ry - rlen, wz + 0.022f);
        }
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

// Helper: cute 6-rib low-poly Thanksgiving pumpkin + stem (outward CCW, N64-optimized!)
static void draw_pumpkin(float cx, float cy, float cz, float rx, float ry, float r, float g, float b) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };
    float mid_y = cy + ry;

    glBegin(GL_QUADS);
    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        float rib = (i & 1) ? 0.90f : 1.04f;
        glColor3f(r * rib, g * rib, b * rib);
        glNormal3f(c0, -0.25f, s0);
        glVertex3f(cx + c0 * rx * 0.82f, cy + ry * 0.18f, cz + s0 * rx * 0.82f);
        glNormal3f(c1, -0.25f, s1);
        glVertex3f(cx + c1 * rx * 0.82f, cy + ry * 0.18f, cz + s1 * rx * 0.82f);
        glNormal3f(c1,  0.25f, s1);
        glVertex3f(cx + c1 * rx,         mid_y + ry * 0.45f, cz + s1 * rx);
        glNormal3f(c0,  0.25f, s0);
        glVertex3f(cx + c0 * rx,         mid_y + ry * 0.45f, cz + s0 * rx);
    }
    glEnd();

    glColor3f(r * 0.96f, g * 0.92f, b * 0.90f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(cx, cy + ry * 1.72f, cz);
    for (int i = 0; i <= 6; i++) {
        glVertex3f(cx + cs[i] * rx, mid_y + ry * 0.45f, cz + sn[i] * rx);
    }
    glEnd();

    glColor3f(0.28f, 0.40f, 0.16f);
    draw_cylinder_y(cx, cy + ry * 1.65f, cy + ry * 2.18f, cz, rx * 0.16f, 4);
}

// Helper: delicious low-poly Thanksgiving Pumpkin Pie with golden crust & whipped cream!
static void draw_pumpkin_pie(float cx, float cy, float cz, float r) {
    static const float cs[9] = { 1.0f,  0.7071f,  0.0f, -0.7071f, -1.0f, -0.7071f,  0.0f,  0.7071f, 1.0f };
    static const float sn[9] = { 0.0f, -0.7071f, -1.0f, -0.7071f,  0.0f,  0.7071f,  1.0f,  0.7071f, 0.0f };
    float h = 0.064f;

    // 1. Silver pie tin tapered base
    glColor3f(0.78f, 0.80f, 0.84f);
    glBegin(GL_QUADS);
    for (int i = 0; i < 8; i++) {
        glNormal3f(cs[i], -0.2f, sn[i]);
        glVertex3f(cx + cs[i] * r * 0.78f, cy,        cz + sn[i] * r * 0.78f);
        glNormal3f(cs[i + 1], -0.2f, sn[i + 1]);
        glVertex3f(cx + cs[i + 1] * r * 0.78f, cy,    cz + sn[i + 1] * r * 0.78f);
        glNormal3f(cs[i + 1], 0.2f, sn[i + 1]);
        glVertex3f(cx + cs[i + 1] * r,     cy + h,    cz + sn[i + 1] * r);
        glNormal3f(cs[i], 0.2f, sn[i]);
        glVertex3f(cx + cs[i] * r,         cy + h,    cz + sn[i] * r);
    }
    glEnd();

    // 2. Golden fluted crimped pie crust ring around the edge!
    glColor3f(0.92f, 0.72f, 0.36f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    for (int i = 0; i < 8; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        float rim_h = h + ((i & 1) ? 0.022f : 0.012f);
        glVertex3f(cx + c0 * r * 0.82f, cy + h + 0.008f, cz + s0 * r * 0.82f);
        glVertex3f(cx + c0 * r * 1.06f, cy + rim_h,      cz + s0 * r * 1.06f);
        glVertex3f(cx + c1 * r * 1.06f, cy + rim_h,      cz + s1 * r * 1.06f);
        glVertex3f(cx + c1 * r * 0.82f, cy + h + 0.008f, cz + s1 * r * 0.82f);
    }
    glEnd();

    // 3. Rich spiced cinnamon-orange pumpkin pie filling disk!
    glColor3f(0.86f, 0.38f, 0.08f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(cx, cy + h + 0.010f, cz);
    for (int i = 0; i <= 8; i++) {
        glVertex3f(cx + cs[i] * r * 0.84f, cy + h + 0.009f, cz + sn[i] * r * 0.84f);
    }
    glEnd();

    // 4. Big fluffy white whipped-cream swirl dollop right in the center!
    glColor3f(0.98f, 0.96f, 0.92f);
    float wy = cy + h + 0.010f;
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 8; i++) {
        glNormal3f(cs[i], 0.55f, sn[i]);
        glVertex3f(cx, wy + 0.072f, cz);
        glVertex3f(cx + cs[i] * r * 0.34f, wy, cz + sn[i] * r * 0.34f);
        glVertex3f(cx + cs[i + 1] * r * 0.34f, wy, cz + sn[i + 1] * r * 0.34f);
    }
    glEnd();
}

static void build_static_scene_list(const SceneTextures *tex) {
    const FlameTheme *neutral_th = &FLAME_THEMES[0];

    // ---------------------------------------------------------
    // 1. Cabin Wood Floor & Woven Hearth Rug
    // ---------------------------------------------------------
    glColor3f(0.95f, 0.90f, 0.86f);
    glBindTexture(GL_TEXTURE_2D, tex->floor);
    draw_grid_quad(-4.2f, 0.0f, 4.4f,  8.4f, 0.0f, 0.0f,  0.0f, 0.0f, -4.8f,
                   0.0f, 1.0f, 0.0f,  5.0f, 3.6f, 4, 3);

    // Cozy plush braided hearth rug (real 3.5cm thickness + bevel sides — zero 16-bit Z-fighting!)
    glColor3f(0.98f, 0.94f, 0.90f);
    glBindTexture(GL_TEXTURE_2D, tex->rug);
    draw_grid_quad(-1.55f, 0.035f, 2.48f,  3.10f, 0.0f, 0.0f,  0.0f, 0.0f, -1.56f,
                   0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 3, 2);
    // Subtle woven rug edges down to floor
    draw_grid_quad(-1.55f, 0.0f, 2.48f,  3.10f, 0.0f, 0.0f,  0.0f, 0.035f, 0.0f,
                   0.0f, 0.0f, 1.0f,  1.0f, 0.08f, 1, 1);
    draw_grid_quad(-1.55f, 0.0f, 0.92f,  0.0f, 0.0f, 1.56f,  0.0f, 0.035f, 0.0f,
                  -1.0f, 0.0f, 0.0f,  1.0f, 0.08f, 1, 1);
    draw_grid_quad( 1.55f, 0.0f, 2.48f,  0.0f, 0.0f,-1.56f,  0.0f, 0.035f, 0.0f,
                   1.0f, 0.0f, 0.0f,  1.0f, 0.08f, 1, 1);

    // ---------------------------------------------------------
    // 2. Cozy Timber Cabin Back Walls & Window Frame
    // ---------------------------------------------------------
    glColor3f(0.76f, 0.70f, 0.66f);
    glBindTexture(GL_TEXTURE_2D, tex->wood_mantle);
    draw_grid_quad(-4.2f, 0.0f, -0.18f,  2.78f, 0.0f, 0.0f,  0.0f, 3.6f, 0.0f,
                   0.0f, 0.0f, 1.0f,  2.2f, 3.2f, 3, 3);
    draw_grid_quad( 1.42f, 0.0f, -0.18f,  2.78f, 0.0f, 0.0f,  0.0f, 3.6f, 0.0f,
                   0.0f, 0.0f, 1.0f,  2.2f, 3.2f, 3, 3);
    draw_box(-4.2f, 0.0f, -0.18f, -1.45f, 0.18f, -0.10f, 1.0f, 1);
    draw_box( 1.45f, 0.0f, -0.18f,  4.2f, 0.18f, -0.10f, 1.0f, 1);

    // Dark Oak Window Frame & Mullion Crossbars
    float wx0 = -2.72f, wx1 = -1.86f;
    float wy0 =  1.14f, wy1 =  2.26f;
    glColor3f(0.75f, 0.68f, 0.62f);
    draw_box(wx0 - 0.09f, wy0 - 0.08f, -0.18f,  wx1 + 0.09f, wy0,        -0.06f, 1.2f, 1);
    draw_box(wx0 - 0.08f, wy1,         -0.18f,  wx1 + 0.08f, wy1 + 0.08f,-0.08f, 1.2f, 1);
    draw_box(wx0 - 0.08f, wy0,         -0.18f,  wx0,         wy1,        -0.08f, 1.2f, 1);
    draw_box(wx1,         wy0,         -0.18f,  wx1 + 0.08f, wy1,        -0.08f, 1.2f, 1);
    float mx = (wx0 + wx1) * 0.5f;
    float my = (wy0 + wy1) * 0.5f;
    draw_box(mx - 0.024f, wy0, -0.16f,  mx + 0.024f, wy1, -0.11f, 1.2f, 1);
    draw_box(wx0, my - 0.024f, -0.16f,  wx1, my + 0.024f, -0.11f, 1.2f, 1);

    // ---------------------------------------------------------
    // 3. Stone Fireplace Surround, Hearth Slab & Chimney Breast
    // ---------------------------------------------------------
    glColor3f(0.96f, 0.94f, 0.92f);
    glBindTexture(GL_TEXTURE_2D, tex->stone);
    draw_box(-1.74f, 0.0f, -0.98f,  1.74f, 0.18f, 0.76f, 0.95f, 2);
    draw_box(-1.46f, 0.18f, -0.22f, -0.82f, 1.82f, 0.24f, 1.05f, 2);
    draw_box( 0.82f, 0.18f, -0.22f,  1.46f, 1.82f, 0.24f, 1.05f, 2);
    draw_box(-0.84f, 1.42f, -0.22f,  0.84f, 1.82f, 0.24f, 1.05f, 2);
    draw_box(-1.38f, 2.02f, -0.22f,  1.38f, 3.65f, 0.12f, 0.95f, 2);

    // ---------------------------------------------------------
    // 4. Recessed Sooty Firebrick Firebox Interior
    // ---------------------------------------------------------
    glColor3f(0.95f, 0.88f, 0.84f);
    glBindTexture(GL_TEXTURE_2D, tex->brick);
    draw_grid_quad(-0.68f, 0.18f, -0.94f,  1.36f, 0.0f, 0.0f,  0.0f, 1.28f, 0.0f,
                   0.0f, 0.0f, 1.0f,  2.2f, 2.4f, 3, 3);
    draw_grid_quad(-0.82f, 0.18f,  0.02f,  0.14f, 0.0f, -0.96f, 0.0f, 1.28f, 0.0f,
                   0.989f, 0.0f, 0.145f,  1.6f, 2.4f, 2, 3);
    draw_grid_quad( 0.68f, 0.18f, -0.94f,  0.14f, 0.0f,  0.96f, 0.0f, 1.28f, 0.0f,
                  -0.989f, 0.0f, 0.145f,  1.6f, 2.4f, 2, 3);
    glColor3f(0.45f, 0.40f, 0.38f);
    draw_grid_quad( 0.82f, 1.42f, 0.02f, -1.64f, 0.0f, 0.0f,  0.0f, 0.24f, -0.96f,
                   0.0f, -0.97f, -0.24f,  2.0f, 1.5f, 1, 1);

    // ---------------------------------------------------------
    // 5. Heavy Oak Timber Mantle, Corbels & Framed Painting
    // ---------------------------------------------------------
    glColor3f(0.95f, 0.90f, 0.86f);
    glBindTexture(GL_TEXTURE_2D, tex->wood_mantle);
    draw_box(-1.76f, 1.80f, -0.04f,  1.76f, 2.02f, 0.50f, 0.85f, 2);
    draw_box(-1.26f, 1.54f, 0.16f, -1.04f, 1.80f, 0.42f, 1.2f, 1);
    draw_box( 1.04f, 1.54f, 0.16f,  1.26f, 1.80f, 0.42f, 1.2f, 1);

    // 3D Wooden picture frame (4 perimeter rails — zero overlapping backing plane so 16-bit Z-buffer never Z-fights!)
    glColor3f(0.84f, 0.76f, 0.68f);
    draw_box(-0.90f, 2.12f, 0.12f,  0.90f, 2.20f, 0.23f, 1.2f, 1); // bottom rail
    draw_box(-0.90f, 2.96f, 0.12f,  0.90f, 3.04f, 0.23f, 1.2f, 1); // top rail
    draw_box(-0.90f, 2.20f, 0.12f, -0.82f, 2.96f, 0.23f, 1.2f, 1); // left rail
    draw_box( 0.82f, 2.20f, 0.12f,  0.90f, 2.96f, 0.23f, 1.2f, 1); // right rail

    // ---------------------------------------------------------
    // 6. Cozy Mantle Props: Pillar Candles & Steaming Ceramic Mug
    // ---------------------------------------------------------
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.95f, 0.88f, 0.72f);
    draw_cylinder_y(-1.18f, 2.02f, 2.30f, 0.26f, 0.052f, 6);
    draw_cylinder_y(-0.96f, 2.02f, 2.20f, 0.30f, 0.046f, 6);
    draw_cylinder_y( 0.82f, 2.02f, 2.26f, 0.26f, 0.050f, 6);

    glColor3f(0.74f, 0.30f, 0.22f);
    draw_cylinder_y(1.08f, 2.02f, 2.18f, 0.24f, 0.056f, 6);
    draw_box(1.13f, 2.06f, 0.22f, 1.18f, 2.14f, 0.26f, 1.0f, 1);

    // Festive Thanksgiving pumpkins (big left hearth pumpkin is rendered dynamically so frog can turn it into Pumpkin Pie!)
    draw_pumpkin(-1.24f, 0.18f, 0.32f, 0.086f, 0.070f, 0.94f, 0.68f, 0.16f);
    draw_pumpkin( 1.44f, 0.18f, 0.34f, 0.125f, 0.098f, 0.88f, 0.36f, 0.07f);
    draw_pumpkin( 1.38f, 2.02f, 0.26f, 0.092f, 0.074f, 0.92f, 0.44f, 0.08f);
    glEnable(GL_TEXTURE_2D);

    // ---------------------------------------------------------
    // 7. Wrought-Iron Fire Grate & Andirons
    // ---------------------------------------------------------
    glColor3f(0.85f, 0.82f, 0.80f);
    glBindTexture(GL_TEXTURE_2D, tex->iron);
    draw_box(-0.56f, 0.30f, -0.24f,  0.56f, 0.34f, -0.19f, 2.0f, 1);
    draw_box(-0.56f, 0.30f, -0.68f,  0.56f, 0.34f, -0.63f, 2.0f, 1);
    for (int b = -2; b <= 2; b++) {
        float bx = b * 0.22f;
        draw_box(bx - 0.022f, 0.31f, -0.69f, bx + 0.022f, 0.35f, -0.18f, 2.0f, 1);
    }
    for (int side = -1; side <= 1; side += 2) {
        float ax = side * 0.56f;
        draw_cylinder_y(ax, 0.18f, 0.62f, -0.10f, 0.030f, 6);
        draw_box(ax - 0.044f, 0.60f, -0.144f, ax + 0.044f, 0.69f, -0.056f, 2.0f, 1);
        draw_box(ax - 0.045f, 0.18f, -0.14f, ax + 0.045f, 0.23f, 0.02f, 2.0f, 1);
        draw_box(ax - 0.030f, 0.22f, -0.68f, ax + 0.030f, 0.27f, -0.10f, 2.0f, 1);
    }

    // ---------------------------------------------------------
    // 8. Neat Stacked Firewood Pile Beside the Hearth
    // ---------------------------------------------------------
    static const float pile[6][3] = {
        {1.92f, 0.09f, 0.20f},
        {2.10f, 0.09f, 0.22f},
        {2.28f, 0.09f, 0.19f},
        {2.01f, 0.24f, 0.21f},
        {2.19f, 0.24f, 0.20f},
        {2.10f, 0.38f, 0.21f},
    };
    for (int p = 0; p < 6; p++) {
        draw_log_3d(tex, pile[p][0], pile[p][1], pile[p][2],
                    0.56f, 0.082f, 90.0f + (p - 2) * 2.5f, 0.0f, 0.0f, neutral_th);
    }
}

void scene_render(const SceneTextures *tex,
                  const FireSystem *fs,
                  const Turkey *turkey,
                  bool rain_enabled,
                  float cam_x, float cam_y, float cam_z,
                  float dim_factor) {
    const FlameTheme *th = &FLAME_THEMES[fs->theme_idx];
    float eff = fire_effective_intensity(fs);
    float fl  = fs->flicker * dim_factor;

    // ---------------------------------------------------------
    // Dynamic N64 Hardware OpenGL Lighting (Flickering Hearth!)
    // ---------------------------------------------------------
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glDisable(GL_NORMALIZE); // All normals are already unit-length — big RSP speedup!
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    GLfloat global_amb[4] = {
        (0.11f + 0.05f * th->light_r) * dim_factor,
        (0.09f + 0.04f * th->light_g) * dim_factor,
        (0.09f + 0.04f * th->light_b) * dim_factor,
        1.0f
    };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_amb);

    // LIGHT0: Primary flickering fire point light inside the firebox
    glEnable(GL_LIGHT0);
    GLfloat l0_pos[4]  = { fs->flicker_x, 0.78f + fs->flicker_y, -0.16f + fs->flicker_z, 1.0f };
    GLfloat l0_diff[4] = {
        fminf(1.5f, th->light_r * (1.05f + 0.42f * eff) * fl),
        fminf(1.5f, th->light_g * (1.05f + 0.42f * eff) * fl),
        fminf(1.5f, th->light_b * (1.05f + 0.42f * eff) * fl),
        1.0f
    };
    GLfloat l0_amb[4]  = {
        th->light_r * 0.16f * fl,
        th->light_g * 0.16f * fl,
        th->light_b * 0.16f * fl,
        1.0f
    };
    glLightfv(GL_LIGHT0, GL_POSITION, l0_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  l0_diff);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  l0_amb);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION,  0.58f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION,    0.20f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.09f);

    // LIGHT1: Cheap directional warm mantle candle bounce fill (w = 0.0 => zero per-vertex sqrt!)
    glEnable(GL_LIGHT1);
    float c_fl = (0.92f + 0.08f * sinf(fs->time * 6.4f)) * dim_factor;
    GLfloat l1_dir[4]  = { -0.25f, 0.55f, 0.80f, 0.0f };
    GLfloat l1_diff[4] = { 0.30f * c_fl, 0.21f * c_fl, 0.12f * c_fl, 1.0f };
    GLfloat l1_amb[4]  = { 0.04f * dim_factor, 0.03f * dim_factor, 0.02f * dim_factor, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, l1_dir);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  l1_diff);
    glLightfv(GL_LIGHT1, GL_AMBIENT,  l1_amb);

    // 1. Pre-compiled static hearth, cabin room, mantle, iron grate & wood pile!
    glCallList(tex->static_list);

    // 1b. Warm candlelit alpine night painting above the mantle (cleanly proud of chimney at z=0.19)
    glDisable(GL_LIGHTING);
    float art_warm = (0.82f + 0.06f * fs->flicker) * dim_factor;
    glColor3f(fminf(1.0f, art_warm * 1.04f), fminf(1.0f, art_warm * 0.99f), fminf(1.0f, art_warm * 0.96f));
    glBindTexture(GL_TEXTURE_2D, tex->painting);
    draw_grid_quad(-0.82f, 2.20f, 0.19f,  1.64f, 0.0f, 0.0f,  0.0f, 0.76f, 0.0f,
                   0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1, 1);
    glEnable(GL_LIGHTING);

    // 2. Animated cabin window sky (stars or falling rain streaks)
    draw_cabin_window_sky(tex, fs->time, rain_enabled, dim_factor);

    // 3. Glowing pulsing coal & ash bed
    draw_coal_mound(tex, fs, dim_factor);

    // 4. Burning firewood logs + dynamically tossed logs
    float glow_amt = (0.45f + 0.45f * eff) * (0.90f + 0.12f * fs->flicker);
    draw_log_3d(tex, -0.04f, 0.41f, -0.34f, 0.96f, 0.095f,   7.0f,  2.0f, glow_amt, th);
    draw_log_3d(tex,  0.03f, 0.42f, -0.56f, 0.92f, 0.092f,  -9.0f, -2.0f, glow_amt, th);
    draw_log_3d(tex, -0.16f, 0.54f, -0.44f, 0.78f, 0.082f,  28.0f, 14.0f, glow_amt * 0.85f, th);
    draw_log_3d(tex,  0.18f, 0.53f, -0.46f, 0.76f, 0.080f, -32.0f,-13.0f, glow_amt * 0.85f, th);
    draw_log_3d(tex,  0.01f, 0.63f, -0.45f, 0.72f, 0.074f,  11.0f,  5.0f, glow_amt * 0.70f, th);

    for (int i = 0; i < MAX_TOSSED_LOGS; i++) {
        const TossedLog *tl = &fs->tossed_logs[i];
        if (!tl->active) continue;
        float life_left = 1.0f - (tl->burn_age / tl->burn_life);
        float rad = 0.072f * (0.45f + 0.55f * life_left);
        float lg  = glow_amt * (tl->landed ? (0.35f + 0.65f * (1.0f - life_left)) : 0.15f);
        draw_log_3d(tex, tl->x, tl->y, tl->z, 0.70f, rad, tl->rot_y, tl->rot_z, lg, th);
    }

    // 4b. Left hearth pumpkin — magically transforms into a Pumpkin Pie via Frog Wizard Easter Egg!
    glDisable(GL_TEXTURE_2D);
    if (turkey->pie_timer > 0.0f) {
        draw_pumpkin_pie(-1.46f, 0.18f + turkey->pie_pop_y, 0.34f, 0.175f);
    } else {
        draw_pumpkin(-1.46f, 0.18f, 0.34f, 0.145f, 0.115f, 0.92f, 0.42f, 0.08f);
    }
    glEnable(GL_TEXTURE_2D);

    // 5. Thanksgiving turkey / frog visitor!
    turkey_render(turkey);

    // 6. Volumetric layered flames, hearth glow, sparks & mug steam
    fire_render_effects(fs, tex->flame, tex->glow, tex->spark, cam_x, cam_y, cam_z);

    // 7. Frog wizard wand beam & transformation sparkle poof!
    turkey_render_magic_fx(turkey, tex->spark, cam_x, cam_z);
}
