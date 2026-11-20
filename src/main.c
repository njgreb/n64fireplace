#include <libdragon.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/gl_integration.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fire.h"
#include "scene.h"
#include "turkey.h"

// Mixer channel layout
#define CH_FIRE_BED   0
#define CH_RAIN       1
#define CH_AMBIENT    2
#define CH_SFX_FIRST  3
#define CH_SFX_COUNT  8
#define NUM_CRACKLES  6

typedef enum {
    CAM_AUTO_CYCLE = 0,
    CAM_CLASSIC,
    CAM_FIRESIDE_LOW,
    CAM_PARLOR_ANGLE,
    CAM_CLOSEUP,
    CAM_MODE_COUNT
} CameraMode;

typedef enum {
    DRIFT_GENTLE = 0,
    DRIFT_WIDE,
    DRIFT_OFF,
    DRIFT_MODE_COUNT
} DriftMode;

static const char *CAM_NAMES[CAM_MODE_COUNT] = {
    "Auto-Cycle", "Full Hearth & Mantle", "Parlor 3/4 Wide", "Fireside Rug", "Close-Up Coals"
};

static const char *DRIFT_NAMES[DRIFT_MODE_COUNT] = {
    "Gentle (CRT Safe)", "Wide Orbit", "Off (Manual)"
};

typedef struct {
    float eye_x, eye_y, eye_z;
    float tgt_x, tgt_y, tgt_z;
} CamPreset;

static const CamPreset CAM_PRESETS[4] = {
    // 0: Full Hearth & Mantle (panned back to showcase the full SEI logo, mantle, pumpkins & fire!)
    {  0.00f, 1.58f, 4.65f,   0.00f, 1.56f, -0.15f },
    // 1: Parlor 3/4 Wide (shows cabin window, firewood, full logo & fireplace)
    {  1.40f, 1.66f, 4.45f,  -0.15f, 1.50f, -0.15f },
    // 2: Fireside Rug (cozy lower view looking into flames & turkey)
    { -0.40f, 0.76f, 2.65f,   0.04f, 1.02f, -0.35f },
    // 3: Close-Up Coals (intimate crackling logs & flames)
    {  0.14f, 0.86f, 1.80f,   0.00f, 0.76f, -0.45f },
};

// Audio assets
static wav64_t sfx_fire_bed;
static wav64_t sfx_rain;
static wav64_t sfx_ambient;
static wav64_t sfx_stoke;
static wav64_t sfx_log_thud;
static wav64_t sfx_gobble;
static wav64_t sfx_ribbit;
static wav64_t sfx_ding;
static wav64_t sfx_magic;
static wav64_t sfx_meow;
static wav64_t sfx_crackles[NUM_CRACKLES];
static int next_sfx_ch = CH_SFX_FIRST;

static uint32_t main_rng = 0xCAFEBABEu;
static float mrand01(void) {
    main_rng = main_rng * 1664525u + 1013904223u;
    return (float)((main_rng >> 8) & 0xFFFFu) / 65535.0f;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void pump_audio(void) {
    // Synchronous mixer_poll guarantees RSP finishes writing audio samples
    // BEFORE audio_write_end hands the buffer to AI DMA (eliminates chop/races!)
    while (audio_can_write()) {
        short *buf = audio_write_begin();
        mixer_poll(buf, audio_get_buffer_length());
        audio_write_end();
    }
}

static int alloc_sfx_channel(void) {
    int ch = next_sfx_ch;
    next_sfx_ch = CH_SFX_FIRST + ((next_sfx_ch - CH_SFX_FIRST + 1) % CH_SFX_COUNT);
    return ch;
}

static void play_random_crackle(float eff_int) {
    int idx = (int)(mrand01() * NUM_CRACKLES) % NUM_CRACKLES;
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_crackles[idx], ch);

    float vol = clampf((0.28f + 0.34f * eff_int) * (0.65f + 0.55f * mrand01()), 0.15f, 0.85f);
    float pan = (mrand01() - 0.5f) * 0.65f; // -0.32..+0.32 subtle stereo spread
    float vl  = vol * (1.0f - fmaxf(0.0f, pan));
    float vr  = vol * (1.0f + fminf(0.0f, pan));
    mixer_ch_set_vol(ch, vl, vr);

    // Organic pitch variation ±16%
    float freq = 22050.0f * (0.84f + 0.32f * mrand01());
    mixer_ch_set_freq(ch, freq);
}

static void play_stoke_sfx(void) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_stoke, ch);
    mixer_ch_set_vol(ch, 0.72f, 0.72f);
    mixer_ch_set_freq(ch, 22050.0f * (0.94f + 0.12f * mrand01()));
}

static void play_log_thud_sfx(void) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_log_thud, ch);
    mixer_ch_set_vol(ch, 0.80f, 0.76f);
    mixer_ch_set_freq(ch, 22050.0f * (0.92f + 0.16f * mrand01()));
}

static void play_gobble_sfx(float x) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_gobble, ch);
    float pan = clampf(x * 0.35f, -0.5f, 0.5f);
    float vl  = 0.72f * (1.0f - fmaxf(0.0f, pan));
    float vr  = 0.72f * (1.0f + fminf(0.0f, pan));
    mixer_ch_set_vol(ch, vl, vr);
    mixer_ch_set_freq(ch, 22050.0f * (0.94f + 0.14f * mrand01()));
}

static void play_ribbit_sfx(float x) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_ribbit, ch);
    float pan = clampf(x * 0.35f, -0.5f, 0.5f);
    float vl  = 0.76f * (1.0f - fmaxf(0.0f, pan));
    float vr  = 0.76f * (1.0f + fminf(0.0f, pan));
    mixer_ch_set_vol(ch, vl, vr);
    mixer_ch_set_freq(ch, 22050.0f * (0.90f + 0.20f * mrand01()));
}

static void play_ding_sfx(void) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_ding, ch);
    mixer_ch_set_vol(ch, 0.82f, 0.82f);
    mixer_ch_set_freq(ch, 22050.0f);
}

static void play_magic_sfx(void) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_magic, ch);
    mixer_ch_set_vol(ch, 0.84f, 0.80f);
    mixer_ch_set_freq(ch, 22050.0f);
}

static void play_meow_sfx(float x) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_meow, ch);
    float pan = clampf(x * 0.35f, -0.5f, 0.5f);
    float vl  = 0.74f * (1.0f - fmaxf(0.0f, pan));
    float vr  = 0.74f * (1.0f + fminf(0.0f, pan));
    mixer_ch_set_vol(ch, vl, vr);
    mixer_ch_set_freq(ch, 22050.0f * (0.94f + 0.14f * mrand01()));
}

static void init_audio_system(void) {
    // ~240ms audio headroom at 22050 Hz for zero underruns on real N64 hardware
    audio_init(22050, AUDIO_INIT_LATENCY_MS(240));
    mixer_init(16);
    for (int ch = 0; ch < 16; ch++) {
        mixer_ch_set_limits(ch, 0, 32000.0f, 0);
    }

    wav64_open(&sfx_fire_bed, "rom:/fire_bed.wav64");
    wav64_set_loop(&sfx_fire_bed, true);

    wav64_open(&sfx_rain, "rom:/rain_loop.wav64");
    wav64_set_loop(&sfx_rain, true);

    wav64_open(&sfx_ambient, "rom:/ambient_loop.wav64");
    wav64_set_loop(&sfx_ambient, true);

    wav64_open(&sfx_stoke, "rom:/stoke.wav64");
    wav64_open(&sfx_log_thud, "rom:/log_thud.wav64");
    wav64_open(&sfx_gobble, "rom:/gobble.wav64");
    wav64_open(&sfx_ribbit, "rom:/ribbit.wav64");
    wav64_open(&sfx_ding, "rom:/ding.wav64");
    wav64_open(&sfx_magic, "rom:/magic.wav64");
    wav64_open(&sfx_meow, "rom:/meow.wav64");

    char path[32];
    for (int i = 0; i < NUM_CRACKLES; i++) {
        snprintf(path, sizeof(path), "rom:/crackle_%d.wav64", i);
        wav64_open(&sfx_crackles[i], path);
    }

    // Start only fire_bed initially; rain & ambient start on-demand so we don't waste RSP/DMA!
    wav64_play(&sfx_fire_bed, CH_FIRE_BED);
    mixer_ch_set_vol(CH_FIRE_BED, 0.55f, 0.55f);

    // Pre-fill initial audio buffers synchronously before entering 3D loop
    pump_audio();
}

static void draw_hud_overlay(int font_id,
                             bool show_menu,
                             const char *toast, float toast_timer,
                             float intro_timer,
                             const FireSystem *fs,
                             bool rain_on, bool ambient_on, bool dim_on,
                             CameraMode cam_mode, DriftMode drift_mode) {
    // 1. Subtle boot-up hint banner (fades away cleanly after 5s)
    if (intro_timer > 0.0f && !show_menu && toast_timer <= 0.0f) {
        uint8_t a = (uint8_t)(clampf(intro_timer / 1.5f, 0.0f, 1.0f) * 165.0f);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(14, 10, 8, a));
        rdpq_fill_rectangle(36, 206, 284, 226);

        if (intro_timer > 0.3f) {
            rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 248 },
                             font_id, 36, 219,
                             "HEARTH 64  -  Press START for Controls");
        }
    }

    // 2. Brief non-intrusive toast on button press
    if (toast_timer > 0.0f && !show_menu) {
        uint8_t a = (uint8_t)(clampf(toast_timer * 1.4f, 0.0f, 1.0f) * 175.0f);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(16, 11, 8, a));
        rdpq_fill_rectangle(44, 206, 276, 226);

        if (toast_timer > 0.25f) {
            rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 232 },
                             font_id, 44, 219, "%s", toast);
        }
    }

    // 3. Full translucent controls & status panel (START toggles, auto-hides for CRT safety)
    if (show_menu) {
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(16, 11, 9, 196));
        rdpq_fill_rectangle(24, 22, 296, 218);

        // Warm gold top & bottom hairline border
        rdpq_set_prim_color(RGBA32(214, 144, 64, 220));
        rdpq_fill_rectangle(24, 22, 296, 24);
        rdpq_fill_rectangle(24, 216, 296, 218);

        int int_pct = (int)(fs->intensity * 100.0f + 0.5f);
        rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 272 },
                         font_id, 24, 38, "HEARTH 64  -  COZY FIREPLACE");

        rdpq_text_printf(NULL, font_id, 36,  56, "Flame:  %s", FLAME_THEMES[fs->theme_idx].name);
        rdpq_text_printf(NULL, font_id, 36,  68, "Heat:   %d%%   Rain: %s   Synth: %s",
                         int_pct, rain_on ? "ON" : "OFF", ambient_on ? "ON" : "OFF");
        rdpq_text_printf(NULL, font_id, 36,  80, "Camera: %s", CAM_NAMES[cam_mode]);
        rdpq_text_printf(NULL, font_id, 36,  92, "CRT:    %s  |  Dim: %s",
                         DRIFT_NAMES[drift_mode], dim_on ? "ON" : "OFF");

        rdpq_text_printf(NULL, font_id, 36, 112, "A          Stoke fire & spark burst");
        rdpq_text_printf(NULL, font_id, 36, 124, "B          Toss a fresh log in");
        rdpq_text_printf(NULL, font_id, 36, 136, "D-Up/Down  Adjust fire intensity");
        rdpq_text_printf(NULL, font_id, 36, 148, "D-L / D-R  Turkey/Frog/Cat (L/R: Flame)");
        rdpq_text_printf(NULL, font_id, 36, 160, "C-Up/Down  Toggle Rain / N64 Synth");
        rdpq_text_printf(NULL, font_id, 36, 172, "C-L / C-R  Change camera angle");
        rdpq_text_printf(NULL, font_id, 36, 184, "Z / Stick  CRT Drift / Orbit  (L+R: Dim)");
        rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 272 },
                         font_id, 24, 206, "Press START to hide (auto-hides for CRT)");
    }
}

int main(void) {
    debug_init_emulog();
    debug_init_usblog();

    // 320x240 16-bit color: classic razor-sharp 240p on CRT, 4MB N64 friendly,
    // automatic NTSC (60Hz) and PAL (50Hz) support via libdragon!
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    gl_init();
    joypad_init();
    timer_init();

    // Load built-in pixel font for crisp zero-asset HUD
    rdpq_font_t *font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_font_style(font, 0, &(rdpq_fontstyle_t){
        .color         = RGBA32(252, 236, 208, 255),
        .outline_color = RGBA32(24, 14, 10, 255),
    });
    rdpq_text_register_font(1, font);

    // Load textures & audio
    SceneTextures tex;
    scene_load_textures(&tex);
    init_audio_system();

    // Fire, turkey & scene state
    FireSystem fire;
    fire_init(&fire);

    Turkey turkey;
    turkey_init(&turkey);

    bool rain_enabled    = false;
    bool ambient_enabled = false;
    bool dim_enabled     = false;
    float rain_vol       = 0.0f;
    float ambient_vol    = 0.0f;
    float dim_factor     = 1.0f;

    CameraMode cam_mode   = CAM_AUTO_CYCLE;
    DriftMode  drift_mode = DRIFT_GENTLE;
    float auto_cam_timer  = 0.0f;
    int   auto_cam_idx    = 0;

    // Smoothed camera state
    float cam_ex = CAM_PRESETS[0].eye_x;
    float cam_ey = CAM_PRESETS[0].eye_y;
    float cam_ez = CAM_PRESETS[0].eye_z;
    float cam_tx = CAM_PRESETS[0].tgt_x;
    float cam_ty = CAM_PRESETS[0].tgt_y;
    float cam_tz = CAM_PRESETS[0].tgt_z;

    // Manual analog orbit offsets
    float manual_yaw   = 0.0f;
    float manual_pitch = 0.0f;
    float manual_idle  = 10.0f;

    // HUD & toast state
    bool  show_menu   = false;
    float menu_timer  = 0.0f;
    float intro_timer = 5.5f;
    float toast_timer = 0.0f;
    char  toast[64]   = "";

    // Random wood crackle scheduler
    float next_crackle_t = 0.8f;

    // Setup OpenGL projection matrix once (zNear=0.50, zFar=14.0 maximizes 16-bit N64 Z-buffer precision!)
    float aspect = (float)display_get_width() / (float)display_get_height();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(44.0f, aspect, 0.50f, 14.0f);
    glClearColor(0.04f, 0.028f, 0.024f, 1.0f);

    uint64_t last_ticks = get_ticks();
    float diag_timer = 0.0f;

    while (1) {
        pump_audio();

        // Compute frame delta time (clamped for safety across NTSC/PAL)
        uint64_t now_ticks = get_ticks();
        float dt = (float)(now_ticks - last_ticks) / (float)TICKS_PER_SECOND;
        last_ticks = now_ticks;
        if (dt < 0.001f) dt = 0.0167f;
        if (dt > 0.08f)  dt = 0.08f;

        // ---------------------------------------------------------
        // Poll Controller Input
        // ---------------------------------------------------------
        joypad_poll();
        if (joypad_is_connected(JOYPAD_PORT_1)) {
            joypad_inputs_t  inp = joypad_get_inputs(JOYPAD_PORT_1);
            joypad_buttons_t prs = joypad_get_buttons_pressed(JOYPAD_PORT_1);

            if (prs.start) {
                show_menu  = !show_menu;
                menu_timer = show_menu ? 12.0f : 0.0f;
                intro_timer = 0.0f;
            }

            // Easter Eggs: A + B together while Turkey, Frog, or Cat is on screen!
            bool ab_combo = (inp.btn.a && prs.b) || (inp.btn.b && prs.a);
            if (ab_combo && turkey_trigger_roast(&turkey)) {
                snprintf(toast, sizeof(toast), "Wait... is that dinner?!");
                toast_timer = 2.4f;
                if (show_menu) menu_timer = 12.0f;
            } else if (ab_combo && turkey_trigger_frog_magic(&turkey)) {
                snprintf(toast, sizeof(toast), "Abracadabra, ribbit!");
                toast_timer = 2.2f;
                if (show_menu) menu_timer = 12.0f;
            } else if (ab_combo && turkey_trigger_cat_nap(&turkey)) {
                snprintf(toast, sizeof(toast), "Cozy fireside cat nap!");
                toast_timer = 2.6f;
                if (show_menu) menu_timer = 12.0f;
            } else {
                if (prs.a) {
                    fire_stoke(&fire);
                    play_stoke_sfx();
                    turkey_startle(&turkey);
                    snprintf(toast, sizeof(toast), "Stoked the fire!");
                    toast_timer = 2.0f;
                    if (show_menu) menu_timer = 12.0f;
                }

                if (prs.b) {
                    fire_toss_log(&fire);
                    turkey_startle(&turkey);
                    snprintf(toast, sizeof(toast), "Tossed a fresh log!");
                    toast_timer = 2.2f;
                    if (show_menu) menu_timer = 12.0f;
                }
            }

            // Intensity adjustment (D-Pad Up / Down)
            if (prs.d_up) {
                fire_adjust_intensity(&fire, 0.15f);
                snprintf(toast, sizeof(toast), "Fire Heat: %d%%", (int)(fire.intensity * 100.0f + 0.5f));
                toast_timer = 2.2f;
                if (show_menu) menu_timer = 12.0f;
            }
            if (prs.d_down) {
                fire_adjust_intensity(&fire, -0.15f);
                snprintf(toast, sizeof(toast), "Fire Heat: %d%%", (int)(fire.intensity * 100.0f + 0.5f));
                toast_timer = 2.2f;
                if (show_menu) menu_timer = 12.0f;
            }

            // D-Left / D-Right: Cycle & summon hearth visitors (Turkey / Frog / Cat)!
            if (prs.d_left || prs.d_right) {
                turkey_cycle_summon(&turkey, prs.d_right ? +1 : -1);
                static const char *v_toasts[VISITOR_COUNT] = {
                    "Gobble gobble! (Turkey)",
                    "Ribbit ribbit! (Frog)",
                    "Meow~ (Tabby Cat)"
                };
                snprintf(toast, sizeof(toast), "%s", v_toasts[turkey.species]);
                toast_timer = 2.2f;
                if (show_menu) menu_timer = 12.0f;
            }

            // L+R together toggles CRT Night Dimmer; L or R cycles flame color
            if ((inp.btn.l && prs.r) || (inp.btn.r && prs.l)) {
                dim_enabled = !dim_enabled;
                snprintf(toast, sizeof(toast), "CRT Night Dimmer: %s", dim_enabled ? "ON" : "OFF");
                toast_timer = 2.4f;
                if (show_menu) menu_timer = 12.0f;
            } else if (prs.r) {
                fire_cycle_theme(&fire, +1);
                snprintf(toast, sizeof(toast), "Flame: %s", FLAME_THEMES[fire.theme_idx].name);
                toast_timer = 2.5f;
                if (show_menu) menu_timer = 12.0f;
            } else if (prs.l) {
                fire_cycle_theme(&fire, -1);
                snprintf(toast, sizeof(toast), "Flame: %s", FLAME_THEMES[fire.theme_idx].name);
                toast_timer = 2.5f;
                if (show_menu) menu_timer = 12.0f;
            }

            // C-Up: Toggle Cozy Rain (audio + animated window rain!)
            if (prs.c_up) {
                rain_enabled = !rain_enabled;
                snprintf(toast, sizeof(toast), "Cabin Rain: %s", rain_enabled ? "ON" : "OFF");
                toast_timer = 2.4f;
                if (show_menu) menu_timer = 12.0f;
            }

            // C-Down: Toggle N64 Chill Ambient Synth
            if (prs.c_down) {
                ambient_enabled = !ambient_enabled;
                snprintf(toast, sizeof(toast), "Ambient Synth: %s", ambient_enabled ? "ON" : "OFF");
                toast_timer = 2.4f;
                if (show_menu) menu_timer = 12.0f;
            }

            // C-Left / C-Right: Cycle Camera Angles
            if (prs.c_right) {
                cam_mode = (CameraMode)((cam_mode + 1) % CAM_MODE_COUNT);
                auto_cam_timer = 0.0f;
                snprintf(toast, sizeof(toast), "Camera: %s", CAM_NAMES[cam_mode]);
                toast_timer = 2.4f;
                if (show_menu) menu_timer = 12.0f;
            }
            if (prs.c_left) {
                cam_mode = (CameraMode)((cam_mode + CAM_MODE_COUNT - 1) % CAM_MODE_COUNT);
                auto_cam_timer = 0.0f;
                snprintf(toast, sizeof(toast), "Camera: %s", CAM_NAMES[cam_mode]);
                toast_timer = 2.4f;
                if (show_menu) menu_timer = 12.0f;
            }

            // Z: Cycle CRT Burn-in Drift Mode
            if (prs.z) {
                drift_mode = (DriftMode)((drift_mode + 1) % DRIFT_MODE_COUNT);
                snprintf(toast, sizeof(toast), "CRT Drift: %s", DRIFT_NAMES[drift_mode]);
                toast_timer = 2.4f;
                if (show_menu) menu_timer = 12.0f;
            }

            // Analog stick manual gentle orbit
            float sx = (abs(inp.stick_x) > 10) ? (float)inp.stick_x / 80.0f : 0.0f;
            float sy = (abs(inp.stick_y) > 10) ? (float)inp.stick_y / 80.0f : 0.0f;
            if (sx != 0.0f || sy != 0.0f) {
                manual_yaw   = clampf(manual_yaw   + sx * 1.15f * dt, -0.78f, 0.78f);
                manual_pitch = clampf(manual_pitch + sy * 0.65f * dt, -0.35f, 0.42f);
                manual_idle  = 0.0f;
            } else {
                manual_idle += dt;
            }
        } else {
            manual_idle += dt;
        }

        // Gradually relax manual camera offset back to CRT-safe drift after 8s idle
        if (manual_idle > 8.0f && drift_mode != DRIFT_OFF) {
            manual_yaw   *= (1.0f - 0.35f * dt);
            manual_pitch *= (1.0f - 0.35f * dt);
        }

        // HUD timers
        if (intro_timer > 0.0f) intro_timer -= dt;
        if (toast_timer > 0.0f) toast_timer -= dt;
        if (show_menu) {
            menu_timer -= dt;
            if (menu_timer <= 0.0f) show_menu = false;
        }

        // ---------------------------------------------------------
        // Update Fire Simulation & Dynamic Audio
        // ---------------------------------------------------------
        fire_update(&fire, dt);
        turkey_update(&turkey, dt);
        float eff = fire_effective_intensity(&fire);

        if (fire.log_just_landed) {
            play_log_thud_sfx();
        }
        if (turkey.just_gobbled) {
            play_gobble_sfx(turkey.x);
        }
        if (turkey.just_ribbit) {
            play_ribbit_sfx(turkey.x);
        }
        if (turkey.just_meowed) {
            play_meow_sfx(turkey.x);
        }
        if (turkey.just_entered_fire) {
            fire_stoke(&fire);
            play_stoke_sfx();
        }
        if (turkey.just_dinged) {
            fire_stoke(&fire);
            play_ding_sfx();
            snprintf(toast, sizeof(toast), "DING! Dinner is served!");
            toast_timer = 3.2f;
        }
        if (turkey.just_cast_magic) {
            play_magic_sfx();
        }
        if (turkey.just_made_pie) {
            snprintf(toast, sizeof(toast), "POOF! Fresh Pumpkin Pie!");
            toast_timer = 3.2f;
        }

        // Fire rumble volume follows intensity smoothly (keep native 22050 Hz for zero RSP resampling!)
        float bed_vol = clampf(0.28f + 0.42f * eff, 0.22f, 0.85f);
        mixer_ch_set_vol(CH_FIRE_BED, bed_vol, bed_vol);

        // On-demand streaming + smooth fade-in/out for Rain & Ambient Synth layers
        float rain_tgt = rain_enabled ? 0.56f : 0.0f;
        rain_vol += (rain_tgt - rain_vol) * clampf(dt * 1.8f, 0.0f, 1.0f);
        if (rain_vol > 0.008f) {
            if (!mixer_ch_playing(CH_RAIN)) {
                wav64_play(&sfx_rain, CH_RAIN);
            }
            mixer_ch_set_vol(CH_RAIN, rain_vol * 1.05f, rain_vol * 0.90f);
        } else if (mixer_ch_playing(CH_RAIN)) {
            mixer_ch_stop(CH_RAIN);
        }

        float amb_tgt = ambient_enabled ? 0.52f : 0.0f;
        ambient_vol += (amb_tgt - ambient_vol) * clampf(dt * 1.4f, 0.0f, 1.0f);
        if (ambient_vol > 0.008f) {
            if (!mixer_ch_playing(CH_AMBIENT)) {
                wav64_play(&sfx_ambient, CH_AMBIENT);
            }
            mixer_ch_set_vol(CH_AMBIENT, ambient_vol, ambient_vol);
        } else if (mixer_ch_playing(CH_AMBIENT)) {
            mixer_ch_stop(CH_AMBIENT);
        }

        // Smooth CRT Night Dimmer transition
        float dim_tgt = dim_enabled ? 0.62f : 1.0f;
        dim_factor += (dim_tgt - dim_factor) * clampf(dt * 2.5f, 0.0f, 1.0f);

        // Organic Poisson wood crackle pops
        next_crackle_t -= dt;
        if (next_crackle_t <= 0.0f) {
            play_random_crackle(eff);
            // More frequent pops when fire is hotter; gentle sparse pops when smoldering
            float rate = clampf(1.6f - 0.72f * eff, 0.35f, 1.8f);
            next_crackle_t = rate * (0.30f + 1.45f * mrand01());
        }

        // ---------------------------------------------------------
        // Update Camera & CRT Burn-In Protection Orbit
        // ---------------------------------------------------------
        int preset_idx = 0;
        if (cam_mode == CAM_AUTO_CYCLE) {
            auto_cam_timer += dt;
            if (auto_cam_timer >= 24.0f) {
                auto_cam_timer = 0.0f;
                auto_cam_idx = (auto_cam_idx + 1) % 4;
            }
            preset_idx = auto_cam_idx;
        } else {
            preset_idx = (int)cam_mode - 1;
        }

        const CamPreset *cp = &CAM_PRESETS[preset_idx];

        // CRT Burn-In Protection: slow Lissajous orbital drift so zero edges stay static!
        float drift_x = 0.0f, drift_y = 0.0f, drift_z = 0.0f;
        float drift_tx = 0.0f, drift_ty = 0.0f;
        if (drift_mode != DRIFT_OFF) {
            float amp = (drift_mode == DRIFT_WIDE) ? 2.1f : 1.0f;
            float ft = fire.time;
            drift_x  = 0.24f * amp * sinf(ft * 0.068f);
            drift_y  = 0.09f * amp * sinf(ft * 0.095f + 1.1f);
            drift_z  = 0.14f * amp * cosf(ft * 0.053f + 0.4f);
            drift_tx = 0.07f * amp * cosf(ft * 0.061f);
            drift_ty = 0.04f * amp * sinf(ft * 0.082f + 2.0f);
        }

        // Combine preset + drift + manual analog stick orbit
        float goal_tx = cp->tgt_x + drift_tx;
        float goal_ty = cp->tgt_y + drift_ty;
        float goal_tz = cp->tgt_z;

        float rel_x = (cp->eye_x + drift_x) - goal_tx;
        float rel_y = (cp->eye_y + drift_y) - goal_ty;
        float rel_z = (cp->eye_z + drift_z) - goal_tz;

        // Rotate relative vector by manual yaw/pitch
        float cy = cosf(manual_yaw), sy = sinf(manual_yaw);
        float rx1 = rel_x * cy + rel_z * sy;
        float rz1 = -rel_x * sy + rel_z * cy;
        float goal_ex = goal_tx + rx1;
        float goal_ey = clampf(goal_ty + rel_y + manual_pitch * 1.1f, 0.36f, 2.55f);
        float goal_ez = clampf(goal_tz + rz1, 1.05f, 4.95f);

        // Ultra-smooth cinema glide between angles
        float smooth = clampf(dt * 1.65f, 0.0f, 1.0f);
        cam_ex += (goal_ex - cam_ex) * smooth;
        cam_ey += (goal_ey - cam_ey) * smooth;
        cam_ez += (goal_ez - cam_ez) * smooth;
        cam_tx += (goal_tx - cam_tx) * smooth;
        cam_ty += (goal_ty - cam_ty) * smooth;
        cam_tz += (goal_tz - cam_tz) * smooth;

        // ---------------------------------------------------------
        // Render Frame (RDPQ + Libdragon OpenGL 3D + RDPQ HUD)
        // ---------------------------------------------------------
        int queued_before = audio_get_queued_buffers();
        pump_audio();

        surface_t *disp;
        while (!(disp = display_try_get())) {
            pump_audio();
        }
        surface_t *zbuf = display_get_zbuf();
        rdpq_attach(disp, zbuf);

        gl_context_begin();
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            gluLookAt(cam_ex, cam_ey, cam_ez,
                      cam_tx, cam_ty, cam_tz,
                      0.0f, 1.0f, 0.0f);

            scene_render(&tex, &fire, &turkey, rain_enabled, cam_ex, cam_ey, cam_ez, dim_factor);
        }
        gl_context_end();

        // Draw crisp 2D HUD/toasts on top via RDPQ
        draw_hud_overlay(1, show_menu, toast, toast_timer, intro_timer,
                         &fire, rain_enabled, ambient_enabled, dim_enabled,
                         cam_mode, drift_mode);

        rdpq_detach_show();
        pump_audio();

        diag_timer += dt;
        if (diag_timer >= 2.0f) {
            diag_timer = 0.0f;
            debugf("HEARTH64 FPS=%.1f audio_min_queued=%d/%d\n",
                   display_get_fps(),
                   queued_before,
                   audio_get_num_buffers());
        }
    }

    return 0;
}
