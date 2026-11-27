#include "fire.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

const FlameTheme FLAME_THEMES[NUM_FLAME_THEMES] = {
    {
        .name    = "Classic Hearth",
        .outer_r = 1.00f, .outer_g = 0.24f, .outer_b = 0.03f,
        .mid_r   = 1.00f, .mid_g   = 0.62f, .mid_b   = 0.08f,
        .core_r  = 1.00f, .core_g  = 0.94f, .core_b  = 0.58f,
        .light_r = 1.00f, .light_g = 0.58f, .light_b = 0.22f,
        .coal_r  = 1.00f, .coal_g  = 0.36f, .coal_b  = 0.08f,
        .spark_r = 1.00f, .spark_g = 0.78f, .spark_b = 0.28f,
    },
    {
        .name    = "Driftwood Magic",
        .outer_r = 0.06f, .outer_g = 0.34f, .outer_b = 1.00f,
        .mid_r   = 0.14f, .mid_g   = 0.80f, .mid_b   = 1.00f,
        .core_r  = 0.76f, .core_g  = 0.98f, .core_b  = 1.00f,
        .light_r = 0.34f, .light_g = 0.78f, .light_b = 1.00f,
        .coal_r  = 0.16f, .coal_g  = 0.68f, .coal_b  = 1.00f,
        .spark_r = 0.55f, .spark_g = 0.96f, .spark_b = 1.00f,
    },
    {
        .name    = "Wildfire Emerald",
        .outer_r = 0.06f, .outer_g = 0.68f, .outer_b = 0.18f,
        .mid_r   = 0.36f, .mid_g   = 0.96f, .mid_b   = 0.20f,
        .core_r  = 0.88f, .core_g  = 1.00f, .core_b  = 0.62f,
        .light_r = 0.42f, .light_g = 0.96f, .light_b = 0.36f,
        .coal_r  = 0.24f, .coal_g  = 0.86f, .coal_b  = 0.24f,
        .spark_r = 0.72f, .spark_g = 1.00f, .spark_b = 0.45f,
    },
    {
        .name    = "Twilight Amethyst",
        .outer_r = 0.56f, .outer_g = 0.10f, .outer_b = 0.88f,
        .mid_r   = 0.92f, .mid_g   = 0.28f, .mid_b   = 0.78f,
        .core_r  = 1.00f, .core_g  = 0.84f, .core_b  = 0.76f,
        .light_r = 0.88f, .light_g = 0.46f, .light_b = 0.92f,
        .coal_r  = 0.78f, .coal_g  = 0.22f, .coal_b  = 0.70f,
        .spark_r = 1.00f, .spark_g = 0.64f, .spark_b = 0.96f,
    },
    {
        .name    = "Late-Night Amber",
        .outer_r = 0.86f, .outer_g = 0.16f, .outer_b = 0.02f,
        .mid_r   = 0.96f, .mid_g   = 0.44f, .mid_b   = 0.05f,
        .core_r  = 1.00f, .core_g  = 0.78f, .core_b  = 0.34f,
        .light_r = 0.96f, .light_g = 0.44f, .light_b = 0.14f,
        .coal_r  = 0.92f, .coal_g  = 0.26f, .coal_b  = 0.05f,
        .spark_r = 1.00f, .spark_g = 0.60f, .spark_b = 0.18f,
    },
};

static uint32_t rng_state = 0x19960623u;

static float frand01(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (float)((rng_state >> 8) & 0xFFFFu) / 65535.0f;
}

static float frand_range(float lo, float hi) {
    return lo + (hi - lo) * frand01();
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void spawn_flame(FlameParticle *p, float eff_int, bool random_age) {
    // Spread lush flames across the entire log stack
    float spread_x = 0.44f * (0.78f + 0.28f * eff_int);
    float rx = (frand01() + frand01() - 1.0f) * spread_x;
    float rz = -0.44f + (frand01() - 0.5f) * 0.24f;

    p->x = rx;
    p->y = 0.34f + frand_range(0.0f, 0.14f) - fabsf(rx) * 0.08f;
    p->z = rz;

    float speed = frand_range(0.45f, 0.82f) * (0.72f + 0.36f * eff_int);
    p->vx = -rx * 0.24f + frand_range(-0.06f, 0.06f);
    p->vy = speed;
    p->vz = frand_range(-0.04f, 0.04f);

    p->life = frand_range(0.60f, 1.20f);
    p->age  = random_age ? frand_range(0.0f, p->life) : 0.0f;

    // Layer distribution: 0 = big rich outer, 1 = vibrant mid, 2 = bright hot core
    float r = frand01();
    if (r < 0.40f) {
        p->layer  = 0;
        p->base_w = frand_range(0.36f, 0.52f);
        p->base_h = frand_range(0.56f, 0.86f);
    } else if (r < 0.76f) {
        p->layer  = 1;
        p->base_w = frand_range(0.26f, 0.40f);
        p->base_h = frand_range(0.44f, 0.72f);
    } else {
        p->layer  = 2;
        p->base_w = frand_range(0.18f, 0.28f);
        p->base_h = frand_range(0.30f, 0.48f);
        p->x *= 0.68f;
        p->y -= 0.03f;
    }

    p->phase    = frand_range(0.0f, 6.28318f);
    p->sway_amp = frand_range(0.05f, 0.13f);
}

static void spawn_spark(SparkParticle *s, float eff_int, bool burst) {
    s->x = frand_range(-0.38f, 0.38f);
    s->y = frand_range(0.34f, 0.62f);
    s->z = -0.44f + frand_range(-0.18f, 0.18f);

    float mult = burst ? frand_range(1.4f, 2.5f) : 1.0f;
    s->vx = frand_range(-0.22f, 0.22f) * mult;
    s->vy = frand_range(0.48f, 1.05f) * (0.75f + 0.35f * eff_int) * mult;
    s->vz = frand_range(-0.14f, 0.18f) * mult;

    s->life  = frand_range(0.9f, 2.2f);
    s->age   = 0.0f;
    s->size  = frand_range(0.018f, 0.036f);
    s->phase = frand_range(0.0f, 6.28318f);
    s->swirl = frand_range(0.25f, 0.65f) * (frand01() > 0.5f ? 1.0f : -1.0f);
}

static void spawn_smoke(SmokeParticle *sm, float eff_int, bool random_age) {
    sm->x       = frand_range(-0.34f, 0.34f);
    sm->y       = frand_range(0.86f, 1.06f);
    sm->z       = -0.50f + frand_range(-0.12f, 0.08f);
    sm->vx      = -sm->x * 0.22f + frand_range(-0.04f, 0.04f);
    sm->vy      = frand_range(0.28f, 0.46f) * (0.80f + 0.25f * eff_int);
    sm->life    = frand_range(1.1f, 1.9f);
    sm->age     = random_age ? frand_range(0.0f, sm->life) : 0.0f;
    sm->base_sz = frand_range(0.11f, 0.19f);
    sm->phase   = frand_range(0.0f, 6.28318f);
}

static void spawn_steam(SteamParticle *st, bool random_age) {
    // Mug sits on right side of mantle around (1.08, 2.18, 0.24)
    st->x = 1.08f + frand_range(-0.025f, 0.025f);
    st->y = 2.19f;
    st->z = 0.24f + frand_range(-0.025f, 0.025f);
    st->life = frand_range(2.4f, 3.8f);
    st->age  = random_age ? frand_range(0.0f, st->life) : 0.0f;
    st->phase = frand_range(0.0f, 6.28318f);
}

void fire_init(FireSystem *fs) {
    memset(fs, 0, sizeof(*fs));
    fs->theme_idx   = 0;
    fs->intensity   = 0.85f;
    fs->stoke_boost = 0.0f;
    fs->flicker     = 1.0f;

    float eff = fire_effective_intensity(fs);
    for (int i = 0; i < MAX_FLAME_PARTICLES; i++) {
        spawn_flame(&fs->flames[i], eff, true);
    }
    for (int i = 0; i < MAX_SPARK_PARTICLES; i++) {
        spawn_spark(&fs->sparks[i], eff, false);
        fs->sparks[i].age = frand_range(0.0f, fs->sparks[i].life);
    }
    for (int i = 0; i < MAX_SMOKE_PARTICLES; i++) {
        spawn_smoke(&fs->smoke[i], eff, true);
    }
    for (int i = 0; i < MAX_STEAM_PARTICLES; i++) {
        spawn_steam(&fs->steam[i], true);
    }
}

float fire_effective_intensity(const FireSystem *fs) {
    return clampf(fs->intensity + fs->stoke_boost, 0.22f, 1.55f);
}

void fire_stoke(FireSystem *fs) {
    fs->stoke_boost = clampf(fs->stoke_boost + 0.52f, 0.0f, 0.72f);
    float eff = fire_effective_intensity(fs);
    // Launch a shower of excited sparks!
    int burst_count = 0;
    for (int i = 0; i < MAX_SPARK_PARTICLES && burst_count < 36; i++) {
        if (fs->sparks[i].age > fs->sparks[i].life * 0.35f || (i & 1) == 0) {
            spawn_spark(&fs->sparks[i], eff, true);
            burst_count++;
        }
    }
}

void fire_toss_log(FireSystem *fs) {
    // Pick an inactive slot or overwrite the oldest burning tossed log
    int slot = 0;
    float oldest = -1.0f;
    for (int i = 0; i < MAX_TOSSED_LOGS; i++) {
        if (!fs->tossed_logs[i].active) {
            slot = i;
            break;
        }
        if (fs->tossed_logs[i].burn_age > oldest) {
            oldest = fs->tossed_logs[i].burn_age;
            slot = i;
        }
    }

    TossedLog *tl = &fs->tossed_logs[slot];
    tl->active    = true;
    tl->landed    = false;
    tl->x         = frand_range(-0.18f, 0.18f);
    tl->y         = 1.25f;
    tl->z         = 0.22f;
    tl->vx        = -tl->x * 0.55f + frand_range(-0.08f, 0.08f);
    tl->vy        = 0.55f;
    tl->vz        = -1.35f;
    tl->rot_y     = frand_range(-38.0f, 38.0f);
    tl->rot_z     = frand_range(-16.0f, 16.0f);
    tl->target_y  = 0.64f + slot * 0.055f;
    tl->burn_age  = 0.0f;
    tl->burn_life = 48.0f;

    // Gentle immediate intensity boost
    fs->intensity = clampf(fs->intensity + 0.12f, 0.25f, 1.30f);
}

void fire_cycle_theme(FireSystem *fs, int delta) {
    fs->theme_idx = (fs->theme_idx + delta + NUM_FLAME_THEMES) % NUM_FLAME_THEMES;
}

void fire_adjust_intensity(FireSystem *fs, float delta) {
    fs->intensity = clampf(fs->intensity + delta, 0.25f, 1.30f);
}

void fire_update(FireSystem *fs, float dt) {
    fs->time += dt;
    fs->log_just_landed = false;

    // Smooth decay of stoke boost
    if (fs->stoke_boost > 0.0f) {
        fs->stoke_boost -= dt * 0.26f;
        if (fs->stoke_boost < 0.0f) fs->stoke_boost = 0.0f;
    }

    float eff = fire_effective_intensity(fs);
    float t = fs->time;

    // Multi-frequency organic hearth flicker (smooth, warm, never stroby on CRT!)
    float raw_flicker = 1.0f
        + 0.10f * sinf(t * 7.3f)
        + 0.06f * sinf(t * 13.7f + 1.1f)
        + 0.04f * sinf(t * 21.1f + 2.4f)
        + 0.05f * sinf(t * 3.1f + 0.6f);
    float target_fl = raw_flicker * (0.72f + 0.34f * eff);
    fs->flicker += (target_fl - fs->flicker) * clampf(dt * 12.0f, 0.0f, 1.0f);

    fs->flicker_x = 0.07f * sinf(t * 5.9f) + 0.03f * cosf(t * 11.3f);
    fs->flicker_y = 0.06f * sinf(t * 8.4f + 0.7f) + 0.04f * eff;
    fs->flicker_z = 0.05f * cosf(t * 6.7f + 1.9f);

    // Update tossed logs
    for (int i = 0; i < MAX_TOSSED_LOGS; i++) {
        TossedLog *tl = &fs->tossed_logs[i];
        if (!tl->active) continue;
        if (!tl->landed) {
            tl->vy -= 4.8f * dt;
            tl->x  += tl->vx * dt;
            tl->y  += tl->vy * dt;
            tl->z  += tl->vz * dt;
            if (tl->z < -0.48f) {
                tl->z = -0.48f;
                tl->vz = 0.0f;
            }
            if (tl->y <= tl->target_y) {
                tl->y = tl->target_y;
                tl->landed = true;
                tl->vx = tl->vy = tl->vz = 0.0f;
                fs->log_just_landed = true;
                fire_stoke(fs);
            }
        } else {
            tl->burn_age += dt;
            if (tl->burn_age >= tl->burn_life) {
                tl->active = false;
            }
        }
    }

    // Update flame tongues
    for (int i = 0; i < MAX_FLAME_PARTICLES; i++) {
        FlameParticle *p = &fs->flames[i];
        p->age += dt;
        if (p->age >= p->life) {
            spawn_flame(p, eff, false);
            continue;
        }
        float u = p->age / p->life;
        // Upward buoyancy + inward convergence toward chimney draft
        p->x += (p->vx + sinf(t * 5.2f + p->phase + u * 4.5f) * p->sway_amp) * dt;
        p->y += p->vy * (0.85f + 0.35f * u) * dt;
        p->z += (p->vz + cosf(t * 4.1f + p->phase) * p->sway_amp * 0.45f) * dt;
        // Gently draw toward chimney center as flames climb
        p->x *= (1.0f - 0.55f * dt);
    }

    // Update rising sparks & embers
    for (int i = 0; i < MAX_SPARK_PARTICLES; i++) {
        SparkParticle *s = &fs->sparks[i];
        s->age += dt;
        if (s->age >= s->life) {
            spawn_spark(s, eff, false);
            continue;
        }
        // Helical convection swirl upward
        float ang = t * 3.8f + s->phase;
        s->x += (s->vx + cosf(ang) * s->swirl * 0.25f) * dt;
        s->y += s->vy * dt;
        s->z += (s->vz + sinf(ang) * s->swirl * 0.18f) * dt;
        // Gentle air drag on lateral velocity
        s->vx *= (1.0f - 0.8f * dt);
        s->vz *= (1.0f - 0.8f * dt);
        // Bounce off firebox brick side and back walls
        if (s->y < 1.38f) {
            if (s->x < -0.66f) { s->x = -0.66f; s->vx = fabsf(s->vx) * 0.55f; }
            else if (s->x > 0.66f) { s->x = 0.66f; s->vx = -fabsf(s->vx) * 0.55f; }
        }
        if (s->z < -0.88f) { s->z = -0.88f; s->vz = fabsf(s->vz) * 0.5f; }
        // Keep sparks inside chimney draft when rising high
        if (s->y > 1.25f) {
            s->z += (-0.48f - s->z) * 1.4f * dt;
        }
    }

    // Update rising chimney flue smoke wisps
    for (int i = 0; i < MAX_SMOKE_PARTICLES; i++) {
        SmokeParticle *sm = &fs->smoke[i];
        sm->age += dt;
        if (sm->age >= sm->life || sm->y >= 1.48f) {
            spawn_smoke(sm, eff, false);
            continue;
        }
        sm->x += (sm->vx + sinf(t * 2.6f + sm->phase) * 0.055f) * dt;
        sm->y += sm->vy * dt;
        sm->z += (-0.62f - sm->z) * 1.2f * dt;
        sm->x *= (1.0f - 0.45f * dt);
    }

    // Update mug steam wisps
    for (int i = 0; i < MAX_STEAM_PARTICLES; i++) {
        SteamParticle *st = &fs->steam[i];
        st->age += dt;
        if (st->age >= st->life) {
            spawn_steam(st, false);
        } else {
            st->y += 0.095f * dt;
            st->x += sinf(t * 1.8f + st->phase) * 0.025f * dt;
        }
    }
}

static void draw_billboard_quad(float x, float y, float z,
                                float half_w, float h_down, float h_up,
                                float rx, float rz,
                                float r, float g, float b, float a) {
    // Cylindrical Y-axis billboard using horizontal right vector (rx, 0, rz)
    float dx = rx * half_w;
    float dz = rz * half_w;

    T3DBuildVert v0 = { x - dx, y - h_down, z - dz,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,  r, g, b, a };
    T3DBuildVert v1 = { x + dx, y - h_down, z + dz,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,  r, g, b, a };
    T3DBuildVert v2 = { x + dx, y + h_up,   z + dz,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,  r, g, b, a };
    T3DBuildVert v3 = { x - dx, y + h_up,   z - dz,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,  r, g, b, a };
    t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
}

void fire_render_effects(const FireSystem *fs,
                         const T3DTexture *tex_flame,
                         const T3DTexture *tex_glow,
                         const T3DTexture *tex_spark,
                         float cam_x, float cam_y, float cam_z) {
    const FlameTheme *th = &FLAME_THEMES[fs->theme_idx];
    float eff = fire_effective_intensity(fs);
    float t = fs->time;

    // Horizontal right vector perpendicular to camera view direction toward hearth (0, 0.7, -0.4)
    float vx = cam_x - 0.0f;
    float vz = cam_z - (-0.42f);
    float len = sqrtf(vx * vx + vz * vz);
    if (len < 0.001f) len = 1.0f;
    float rx = vz / len;
    float rz = -vx / len;
    (void)cam_y;

    // ---------------------------------------------------------
    // 1. Soft Warm Core Hearth Glow, Floor/Rug Glow Pools, Candle Halos & Flue Smoke
    // ---------------------------------------------------------
    t3d_dyn_begin_quads(tex_glow, T3D_ROOM_SCALE, true, false, false, false);
    {
        float g_scale = (0.76f + 0.32f * eff) * (0.93f + 0.08f * fs->flicker);
        float g_alpha = clampf(0.32f + 0.12f * eff, 0.18f, 0.48f);
        draw_billboard_quad(fs->flicker_x * 0.4f, 0.58f, -0.50f,
                            0.70f * g_scale, 0.34f * g_scale, 0.56f * g_scale, rx, rz,
                            th->outer_r, th->outer_g, th->outer_b, g_alpha);

        // Animated stone hearth apron firelight pool (y=0.192 cleanly above y=0.18 slab)
        {
            float hx = fs->flicker_x * 1.1f;
            float hz = 0.22f + fs->flicker_z * 0.8f;
            float hw = 1.05f * g_scale;
            float hd = 0.52f * g_scale;
            float ha = clampf(clampf(0.16f + 0.09f * eff, 0.10f, 0.28f) * fs->flicker, 0.0f, 0.34f);
            T3DBuildVert h0 = { hx - hw, 0.192f, hz + hd, 0,1,0, 0,1, th->light_r, th->light_g, th->light_b, ha };
            T3DBuildVert h1 = { hx + hw, 0.192f, hz + hd, 0,1,0, 1,1, th->light_r, th->light_g, th->light_b, ha };
            T3DBuildVert h2 = { hx + hw, 0.192f, hz - hd, 0,1,0, 1,0, th->light_r, th->light_g, th->light_b, ha };
            T3DBuildVert h3 = { hx - hw, 0.192f, hz - hd, 0,1,0, 0,0, th->light_r, th->light_g, th->light_b, ha };
            t3d_dyn_quad_vc(&h0, &h1, &h2, &h3);
        }

        // Animated woven hearth rug firelight pool (y=0.044 cleanly above y=0.035 rug)
        {
            float rxp = fs->flicker_x * 1.4f;
            float rzp = 1.62f + fs->flicker_z * 0.9f;
            float rhw = 1.22f * g_scale;
            float rhd = 0.68f * g_scale;
            float ra  = clampf(clampf(0.12f + 0.07f * eff, 0.07f, 0.22f) * fs->flicker, 0.0f, 0.28f);
            T3DBuildVert g0 = { rxp - rhw, 0.044f, rzp + rhd, 0,1,0, 0,1, th->light_r, th->light_g, th->light_b, ra };
            T3DBuildVert g1 = { rxp + rhw, 0.044f, rzp + rhd, 0,1,0, 1,1, th->light_r, th->light_g, th->light_b, ra };
            T3DBuildVert g2 = { rxp + rhw, 0.044f, rzp - rhd, 0,1,0, 1,0, th->light_r, th->light_g, th->light_b, ra };
            T3DBuildVert g3 = { rxp - rhw, 0.044f, rzp - rhd, 0,1,0, 0,0, th->light_r, th->light_g, th->light_b, ra };
            t3d_dyn_quad_vc(&g0, &g1, &g2, &g3);
        }

        // Subtle dark warm chimney flue smoke wisps (rendered before flames)
        for (int i = 0; i < MAX_SMOKE_PARTICLES; i++) {
            const SmokeParticle *sm = &fs->smoke[i];
            float u = sm->age / sm->life;
            if (u < 0.0f || u > 1.0f) continue;
            float sa = sinf(u * 3.14159f) * 0.14f;
            float ssz = sm->base_sz * (0.75f + 0.85f * u);
            draw_billboard_quad(sm->x, sm->y, sm->z,
                                ssz, ssz, ssz, rx, rz,
                                0.18f, 0.14f, 0.12f, sa);
        }

        // Warm mantle candle halos (3 pillar candles)
        static const float candles[3][3] = {
            {-1.18f, 2.34f, 0.26f},
            {-0.96f, 2.24f, 0.30f},
            { 0.82f, 2.30f, 0.26f},
        };
        for (int c = 0; c < 3; c++) {
            float cf = 0.88f + 0.12f * sinf(t * (7.5f + c * 2.1f) + c * 1.7f);
            draw_billboard_quad(candles[c][0], candles[c][1], candles[c][2],
                                0.15f * cf, 0.14f * cf, 0.16f * cf, rx, rz,
                                1.00f, 0.62f, 0.16f, 0.32f * cf);
        }
    }
    t3d_dyn_end_quads();

    // ---------------------------------------------------------
    // 2. Layered Animated 3D Flame Tongues (Outer -> Mid -> Hot Core!)
    // ---------------------------------------------------------
    t3d_dyn_begin_quads(tex_flame, T3D_ROOM_SCALE, true, false, false, false);
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < MAX_FLAME_PARTICLES; i++) {
            const FlameParticle *p = &fs->flames[i];
            if (p->layer != pass) continue;
            float u = p->age / p->life;
            if (u < 0.0f || u > 1.0f) continue;

            float env_in  = clampf(u * 4.8f, 0.0f, 1.0f);
            float env_out = powf(1.0f - u, 1.25f);
            float alpha   = env_in * env_out;

            float scale_int = 0.68f + 0.44f * eff;
            float w = p->base_w * scale_int * (1.06f - 0.50f * u);
            float h = p->base_h * scale_int * (0.82f + 0.48f * sinf(u * 3.14159f));

            float r, g, b;
            float z_bias = 0.0f;
            if (pass == 0) {
                // Lush outer red-orange flames (behind)
                r = th->outer_r;
                g = th->outer_g * (1.0f - 0.30f * u);
                b = th->outer_b;
                alpha *= 0.72f;
                z_bias = -0.04f;
            } else if (pass == 1) {
                // Vibrant golden-orange mid flames
                float m = clampf(u * 1.15f, 0.0f, 1.0f);
                r = th->mid_r * (1.0f - m) + th->outer_r * m;
                g = th->mid_g * (1.0f - m) + th->outer_g * m;
                b = th->mid_b * (1.0f - m) + th->outer_b * m;
                alpha *= 0.80f;
                z_bias = 0.02f;
            } else {
                // Incandescent hot yellow-white inner core flames (front)
                float m = clampf(u * 1.35f, 0.0f, 1.0f);
                r = th->core_r * (1.0f - m) + th->mid_r * m;
                g = th->core_g * (1.0f - m) + th->mid_g * m;
                b = th->core_b * (1.0f - m) + th->mid_b * m;
                alpha *= 0.88f;
                z_bias = 0.07f;
            }

            if (eff < 0.42f && (i % 3 == 0)) {
                alpha *= 0.35f;
            }

            draw_billboard_quad(p->x, p->y, p->z + z_bias, w * 0.5f, h * 0.18f, h * 0.82f, rx, rz,
                                r, g, b, clampf(alpha, 0.0f, 0.96f));
        }
    }

    // Crisp golden flickering flame teardrops atop the 3 mantle candles!
    {
        static const float candles[3][3] = {
            {-1.18f, 2.31f, 0.26f},
            {-0.96f, 2.21f, 0.30f},
            { 0.82f, 2.27f, 0.26f},
        };
        for (int c = 0; c < 3; c++) {
            float sway = 0.008f * sinf(t * (6.2f + c * 1.9f) + c);
            float stretch = 0.90f + 0.16f * sinf(t * (9.1f + c * 2.3f));
            draw_billboard_quad(candles[c][0] + sway, candles[c][1], candles[c][2],
                                0.028f, 0.008f, 0.078f * stretch, rx, rz,
                                1.00f, 0.84f, 0.34f, 0.95f);
        }
    }
    t3d_dyn_end_quads();

    // ---------------------------------------------------------
    // 3. Swirling Sparks & Rising Embers
    // ---------------------------------------------------------
    t3d_dyn_begin_quads(tex_spark, T3D_ROOM_SCALE, true, false, false, false);
    for (int i = 0; i < MAX_SPARK_PARTICLES; i++) {
        const SparkParticle *s = &fs->sparks[i];
        float u = s->age / s->life;
        if (u < 0.0f || u > 1.0f) continue;

        float twinkle = 0.65f + 0.35f * sinf(t * 19.0f + s->phase);
        float alpha = clampf(u * 8.0f, 0.0f, 1.0f) * (1.0f - u * u) * twinkle;

        float cool = clampf(u * 1.3f, 0.0f, 1.0f);
        float r = th->spark_r * (1.0f - cool) + th->outer_r * cool;
        float g = th->spark_g * (1.0f - cool) + th->outer_g * cool;
        float b = th->spark_b * (1.0f - cool) + th->outer_b * cool;

        float sz = s->size * (1.0f - 0.45f * u);
        float streak = sz * (1.4f + clampf(s->vy * 0.8f, 0.0f, 1.4f));
        draw_billboard_quad(s->x, s->y, s->z, sz, sz, streak, rx, rz,
                            r, g, b, clampf(alpha, 0.0f, 1.0f));
    }
    t3d_dyn_end_quads();

    // ---------------------------------------------------------
    // 4. Cozy Mug Steam Wisps
    // ---------------------------------------------------------
    t3d_dyn_begin_quads(tex_glow, T3D_ROOM_SCALE, true, false, false, false);
    for (int i = 0; i < MAX_STEAM_PARTICLES; i++) {
        const SteamParticle *st = &fs->steam[i];
        float u = st->age / st->life;
        if (u < 0.0f || u > 1.0f) continue;
        float alpha = sinf(u * 3.14159f) * 0.16f;
        float sz = 0.032f + 0.065f * u;
        draw_billboard_quad(st->x, st->y, st->z, sz, sz, sz, rx, rz,
                            0.92f, 0.88f, 0.84f, alpha);
    }
    t3d_dyn_end_quads();
}
