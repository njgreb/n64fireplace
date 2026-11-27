#include <libdragon.h>
#include <t3d/t3d.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t3d_gfx.h"
#include "fire.h"
#include "scene.h"
#include "season.h"
#include "turkey.h"

// Mixer channel layout
#define CH_FIRE_BED     0
#define CH_RAIN         1
#define CH_AMBIENT      2
#define CH_SFX_FIRST    3
#define CH_SFX_COUNT    8
#define NUM_CRACKLES    6
#define MENU_ITEM_COUNT 9

typedef enum {
    CAM_AUTO_CYCLE = 0,
    CAM_CLASSIC,
    CAM_FIRESIDE_LOW,
    CAM_PARLOR_ANGLE,
    CAM_CLOSEUP,
    CAM_SNOWGLOBE,
    CAM_MODE_COUNT
} CameraMode;

typedef enum {
    DRIFT_GENTLE = 0,
    DRIFT_WIDE,
    DRIFT_OFF,
    DRIFT_MODE_COUNT
} DriftMode;

typedef enum {
    VIDEO_HIRES_480 = 0,
    VIDEO_240P_32,
    VIDEO_MODE_COUNT
} VideoMode;

static const char *CAM_NAMES[CAM_MODE_COUNT] = {
    "Auto-Cycle", "Hearth & Mantle", "Parlor 3/4 Wide", "Fireside Rug", "Close-Up Coals", "Snowglobe Macro"
};

static const char *DRIFT_NAMES[DRIFT_MODE_COUNT] = {
    "Gentle (CRT Safe)", "Wide Orbit", "Off (Manual)"
};

static const char *DRIFT_SHORT[DRIFT_MODE_COUNT] = {
    "Gentle", "Wide", "Off"
};

static const char *VIDEO_NAMES[VIDEO_MODE_COUNT] = {
    "480i Hi-Res", "240p 32-bit"
};

typedef struct {
    float eye_x, eye_y, eye_z;
    float tgt_x, tgt_y, tgt_z;
} CamPreset;

static const CamPreset CAM_PRESETS[5] = {
    // 0: Full Hearth & Mantle (panned back to showcase the full SEI logo, mantle, pumpkins & fire!)
    {  0.00f, 1.58f, 4.65f,   0.00f, 1.56f, -0.15f },
    // 1: Parlor 3/4 Wide (shows cabin window, firewood, full logo & fireplace)
    {  1.40f, 1.66f, 4.45f,  -0.15f, 1.50f, -0.15f },
    // 2: Fireside Rug (cozy lower view looking into flames & turkey)
    { -0.40f, 0.76f, 2.65f,   0.04f, 1.02f, -0.35f },
    // 3: Close-Up Coals (intimate crackling logs & flames)
    {  0.14f, 0.86f, 1.80f,   0.00f, 0.76f, -0.45f },
    // 4: Snowglobe Macro (close-up on mantle snowglobe diorama)
    { -1.34f, 2.22f, 1.06f,  -1.44f, 2.15f,  0.28f },
};

// Audio assets
static wav64_t sfx_fire_bed;
static wav64_t sfx_rain;
static wav64_t sfx_snow_loop;
static wav64_t sfx_ambient;
static wav64_t sfx_christmas_loop;
static wav64_t sfx_stoke;
static wav64_t sfx_log_thud;
static wav64_t sfx_gobble;
static wav64_t sfx_ribbit;
static wav64_t sfx_ding;
static wav64_t sfx_magic;
static wav64_t sfx_meow;
static wav64_t sfx_penguin;
static wav64_t sfx_snowglobe;
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

static void play_penguin_sfx(float x) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_penguin, ch);
    float pan = clampf(x * 0.35f, -0.5f, 0.5f);
    float vl  = 0.76f * (1.0f - fmaxf(0.0f, pan));
    float vr  = 0.76f * (1.0f + fminf(0.0f, pan));
    mixer_ch_set_vol(ch, vl, vr);
    mixer_ch_set_freq(ch, 22050.0f * (0.95f + 0.10f * mrand01()));
}

static void play_snowglobe_sfx(void) {
    int ch = alloc_sfx_channel();
    wav64_play(&sfx_snowglobe, ch);
    mixer_ch_set_vol(ch, 0.84f, 0.84f);
    mixer_ch_set_freq(ch, 22050.0f);
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

    wav64_open(&sfx_snow_loop, "rom:/snow_loop.wav64");
    wav64_set_loop(&sfx_snow_loop, true);

    wav64_open(&sfx_ambient, "rom:/ambient_loop.wav64");
    wav64_set_loop(&sfx_ambient, true);

    wav64_open(&sfx_christmas_loop, "rom:/christmas_loop.wav64");
    wav64_set_loop(&sfx_christmas_loop, true);

    wav64_open(&sfx_stoke, "rom:/stoke.wav64");
    wav64_open(&sfx_log_thud, "rom:/log_thud.wav64");
    wav64_open(&sfx_gobble, "rom:/gobble.wav64");
    wav64_open(&sfx_ribbit, "rom:/ribbit.wav64");
    wav64_open(&sfx_ding, "rom:/ding.wav64");
    wav64_open(&sfx_magic, "rom:/magic.wav64");
    wav64_open(&sfx_meow, "rom:/meow.wav64");
    wav64_open(&sfx_penguin, "rom:/penguin.wav64");
    wav64_open(&sfx_snowglobe, "rom:/snowglobe.wav64");

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

static void start_season_transition(SnowglobeTransition *sg,
                                    Season current_season,
                                    Season next_season,
                                    bool *show_menu,
                                    float *intro_timer) {
    if (sg->phase != SNOWGLOBE_IDLE || next_season == current_season) return;
    *show_menu          = false;
    *intro_timer        = 0.0f;
    sg->phase           = SNOWGLOBE_ZOOM_IN;
    sg->target_season   = next_season;
    sg->timer           = 0.0f;
    sg->lift_y          = 0.0f;
    sg->shake_angle     = 0.0f;
    sg->shake_intensity = 0.0f;
    sg->flurry_alpha    = 0.0f;
    sg->swapped         = false;
}

static void apply_video_mode(VideoMode mode, bool exp_pak, bool reinit, T3DViewport *vp) {
    resolution_t res     = RESOLUTION_320x240;
    bitdepth_t   bit     = DEPTH_16_BPP;
    filter_options_t flt = FILTERS_RESAMPLE_ANTIALIAS_DEDITHER;

    if (exp_pak) {
        if (mode == VIDEO_HIRES_480) {
            res = RESOLUTION_640x480;
            bit = DEPTH_16_BPP;
            flt = FILTERS_RESAMPLE_ANTIALIAS_DEDITHER;
        } else {
            res = RESOLUTION_320x240;
            bit = DEPTH_32_BPP;
            // Pure bilinear VI resampling avoids VI_AA_MODE_RESAMPLE_FETCH_NEEDED / divot line glitches in 32bpp
            flt = FILTERS_RESAMPLE;
        }
    }

    if (reinit) {
        pump_audio();
        rspq_wait();
        display_change(res, bit, T3D_FB_COUNT, GAMMA_NONE, flt);
    } else {
        display_init(res, bit, T3D_FB_COUNT, GAMMA_NONE, flt);
    }

    if (vp) {
        t3d_viewport_set_area(vp, 0, 0, display_get_width(), display_get_height());
        t3d_viewport_set_projection(vp,
                                    T3D_DEG_TO_RAD(44.0f),
                                    0.50f * T3D_WORLD_SCALE,
                                    14.0f * T3D_WORLD_SCALE);
    }

    int bpp = (exp_pak && mode == VIDEO_240P_32) ? 32 : 16;
    debugf("HEARTH64 Video: %dx%d %dbpp (%s)\n",
           (int)display_get_width(), (int)display_get_height(), bpp,
           exp_pak ? "Expansion Pak" : "4MB");
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static void draw_hud_overlay(int font_id,
                             bool show_menu, int menu_cursor,
                             const char *toast, float toast_timer,
                             float intro_timer,
                             const FireSystem *fs,
                             const Turkey *tk,
                             Season season,
                             const SnowglobeTransition *sg,
                             bool rtc_detected, const rtc_time_t *rtc_now,
                             bool rain_on, bool ambient_on, bool dim_on,
                             CameraMode cam_mode, DriftMode drift_mode,
                             VideoMode video_mode, bool exp_pak) {
    int sw = (int)display_get_width();
    int sh = (int)display_get_height();
    int ox = (sw - 320) / 2;
    int oy = (sh - 240) / 2;
    int by = sh - 34;

    // 0. Snowglobe magical flurry veil during season transition
    if (sg->flurry_alpha > 0.01f) {
        float fa = clampf(sg->flurry_alpha, 0.0f, 1.0f);
        uint8_t bg_a = (uint8_t)(fa * 130.0f);
        color_t bg_col;
        switch (sg->target_season) {
            case SEASON_SPRING:    bg_col = RGBA32(236, 198, 224, bg_a); break;
            case SEASON_SUMMER:    bg_col = RGBA32(164, 212, 168, bg_a); break;
            case SEASON_CHRISTMAS: bg_col = RGBA32(205, 228, 252, bg_a); break;
            case SEASON_THANKSGIVING:
            default:               bg_col = RGBA32(246, 178, 86, bg_a);  break;
        }

        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(bg_col);
        rdpq_fill_rectangle(0, 0, sw, sh);

        int pscale = (sw >= 480) ? 2 : 1;
        for (int i = 0; i < 28; i++) {
            float seed = (float)i * 0.618f;
            float phase = fs->time * (1.8f + (i % 5) * 0.35f) + seed * 12.0f;
            float dir_x = (sg->target_season == SEASON_CHRISTMAS || sg->target_season == SEASON_SPRING) ? 38.0f : -42.0f;
            int fx = (int)(fmodf(seed * 437.0f + phase * dir_x * (float)pscale, (float)sw) + (float)sw) % sw;
            int fy = (int)(fmodf(seed * 293.0f + phase * 62.0f * (float)pscale, (float)sh) + (float)sh) % sh;
            int fsz = (2 + (i % 3)) * pscale;
            uint8_t pa = (uint8_t)(fa * (180.0f + 70.0f * (i & 1)));
            switch (sg->target_season) {
                case SEASON_SPRING:
                    rdpq_set_prim_color((i & 1) ? RGBA32(252, 188, 216, pa) : RGBA32(252, 232, 242, pa));
                    break;
                case SEASON_SUMMER:
                    rdpq_set_prim_color((i & 1) ? RGBA32(218, 252, 112, pa) : RGBA32(158, 240, 128, pa));
                    break;
                case SEASON_CHRISTMAS:
                    rdpq_set_prim_color(RGBA32(248, 252, 255, pa));
                    break;
                case SEASON_THANKSGIVING:
                default:
                    rdpq_set_prim_color((i & 1) ? RGBA32(252, 162, 48, pa) : RGBA32(228, 88, 36, pa));
                    break;
            }
            rdpq_fill_rectangle(fx, fy, fx + fsz, fy + fsz);
        }
    }

    // 1. Subtle boot-up hint banner (fades away cleanly after 5.5s)
    if (intro_timer > 0.0f && !show_menu && toast_timer <= 0.0f) {
        uint8_t a = (uint8_t)(clampf(intro_timer / 1.5f, 0.0f, 1.0f) * 165.0f);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(14, 10, 8, a));
        rdpq_fill_rectangle(ox + 24, by, ox + 296, by + 20);

        if (intro_timer > 0.3f) {
            rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 272 },
                             font_id, ox + 24, by + 13,
                             "HEARTH 64 - %s (START)", season_name(season));
        }
    }

    // 2. Brief non-intrusive toast on button press
    if (toast_timer > 0.0f && !show_menu) {
        uint8_t a = (uint8_t)(clampf(toast_timer * 1.4f, 0.0f, 1.0f) * 175.0f);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(16, 11, 8, a));
        rdpq_fill_rectangle(ox + 32, by, ox + 288, by + 20);

        if (toast_timer > 0.25f) {
            rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 256 },
                             font_id, ox + 32, by + 13, "%s", toast);
        }
    }

    // 3. Interactive translucent controls & settings panel (START toggles, auto-hides for CRT safety)
    if (show_menu) {
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(16, 11, 9, 204));
        rdpq_fill_rectangle(ox + 16, oy + 14, ox + 304, oy + 226);

        // Warm gold top, divider & bottom hairline borders
        rdpq_set_prim_color(RGBA32(214, 144, 64, 220));
        rdpq_fill_rectangle(ox + 16, oy + 14, ox + 304, oy + 16);
        rdpq_fill_rectangle(ox + 24, oy + 158, ox + 296, oy + 159);
        rdpq_fill_rectangle(ox + 16, oy + 224, ox + 304, oy + 226);

        // Highlight bar behind selected menu row
        int sel_y = oy + 52 + menu_cursor * 12;
        rdpq_set_prim_color(RGBA32(190, 118, 36, 130));
        rdpq_fill_rectangle(ox + 22, sel_y - 9, ox + 298, sel_y + 2);

        rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 288 },
                         font_id, ox + 16, oy + 27, "HEARTH 64 - COZY FIREPLACE");

        if (rtc_detected) {
            rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 288 },
                             font_id, ox + 16, oy + 39, "RTC %04u-%02u-%02u | %s",
                             (unsigned)rtc_now->year,
                             (unsigned)(rtc_now->month + 1),
                             (unsigned)rtc_now->day,
                             season_name(season));
        } else {
            rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 288 },
                             font_id, ox + 16, oy + 39, "Season: %s", season_name(season));
        }

        static const char *const v_names[VISITOR_COUNT] = {
            "Turkey", "Frog", "Cat", "Penguin"
        };
        int int_pct = (int)(fs->intensity * 100.0f + 0.5f);
        const char *vis_lbl = (tk->state == TURKEY_HIDDEN) ? "Summon" : v_names[tk->species];
        char row[40];

        for (int i = 0; i < MENU_ITEM_COUNT; i++) {
            switch (i) {
                case 0: snprintf(row, sizeof(row), "Season:  < %s >", season_name(season)); break;
                case 1: snprintf(row, sizeof(row), "Flame:   < %s >", FLAME_THEMES[fs->theme_idx].name); break;
                case 2: snprintf(row, sizeof(row), "Heat:    < %d%% >", int_pct); break;
                case 3: {
                    const char *w_lbl = (season == SEASON_CHRISTMAS) ? "Snow"
                                      : (season == SEASON_SUMMER)    ? "Storm"
                                                                     : "Rain";
                    snprintf(row, sizeof(row), "Weather: < %s: %s >", w_lbl, rain_on ? "ON" : "OFF");
                    break;
                }
                case 4: snprintf(row, sizeof(row), "Music:   < %s: %s >",
                                 (season == SEASON_CHRISTMAS) ? "Chimes" : "Synth",
                                 ambient_on ? "ON" : "OFF"); break;
                case 5: snprintf(row, sizeof(row), "Camera:  < %s >", CAM_NAMES[cam_mode]); break;
                case 6: snprintf(row, sizeof(row), "CRT:     < %s | Dim: %s >",
                                 DRIFT_SHORT[drift_mode], dim_on ? "ON" : "OFF"); break;
                case 7: snprintf(row, sizeof(row), "Visitor: < %s >", vis_lbl); break;
                case 8:
                default: snprintf(row, sizeof(row), "Video:   < %s >",
                                  exp_pak ? VIDEO_NAMES[video_mode] : "240p 16-bit"); break;
            }
            rdpq_text_printf(NULL, font_id, ox + 26, oy + 52 + i * 12, "%c %s",
                             (i == menu_cursor) ? '>' : ' ', row);
        }

        rdpq_text_printf(NULL, font_id, ox + 26, oy + 171, "A Stoke  B Log  A+B Visitor Egg");
        rdpq_text_printf(NULL, font_id, ox + 26, oy + 183, "C-U/D Weather/Music  C-L/R Cam");
        rdpq_text_printf(NULL, font_id, ox + 26, oy + 195, "Z Drift  Z+A Roast  Z+L/R Season");
        rdpq_text_printf(&(rdpq_textparms_t){ .align = ALIGN_CENTER, .width = 288 },
                         font_id, ox + 16, oy + 214, "D-U/D Pick  D-L/R/A Edit  B Exit");
    }
}
#pragma GCC diagnostic pop

int main(void) {
    debug_init_emulog();
    debug_init_usblog();

    // Default to 640x480i Hi-Res on 8MB Expansion Pak (runtime toggleable to 240p 32-bit in START menu),
    // with automatic 320x240 16-bit fallback for 4MB consoles. Automatic NTSC/PAL via libdragon!
    bool exp_pak = is_memory_expanded();
    VideoMode video_mode = VIDEO_HIRES_480;
    apply_video_mode(video_mode, exp_pak, false, NULL);
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    t3d_init((T3DInitParams){ .matrixStackSize = 8 });
    t3d_gfx_init();
    joypad_init();
    timer_init();

    // Auto-detect Season from Flashcart / Emulator Real-Time Clock across all 4 seasons
    Season current_season = SEASON_THANKSGIVING;
    bool rtc_detected = false;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    rtc_time_t rtc_now = {0};
    if (rtc_init() && rtc_get(&rtc_now) && rtc_now.year >= 1996 && rtc_now.year <= 2099) {
        rtc_detected = true;
        // rtc_time_t.month is 0-indexed [0-11]: Jan = 0 .. Dec = 11
        if (rtc_now.month >= 2 && rtc_now.month <= 4) {
            current_season = SEASON_SPRING;
        } else if (rtc_now.month >= 5 && rtc_now.month <= 7) {
            current_season = SEASON_SUMMER;
        } else if (rtc_now.month >= 8 && rtc_now.month <= 10) {
            current_season = SEASON_THANKSGIVING;
        } else {
            current_season = SEASON_CHRISTMAS;
        }
        debugf("HEARTH64 RTC: %04u-%02u-%02u -> %s\n",
               (unsigned)rtc_now.year, (unsigned)(rtc_now.month + 1), (unsigned)rtc_now.day,
               season_name(current_season));
    }
    #pragma GCC diagnostic pop

    // Load built-in pixel font for crisp zero-asset HUD
    rdpq_font_t *font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_font_style(font, 0, &(rdpq_fontstyle_t){
        .color         = RGBA32(252, 236, 208, 255),
        .outline_color = RGBA32(24, 14, 10, 255),
    });
    rdpq_text_register_font(1, font);

    // Load textures & compile static/visitor display lists & audio
    SceneTextures tex;
    scene_load_textures(&tex);
    turkey_init_lists();
    init_audio_system();

    // Fire, turkey & scene state
    FireSystem fire;
    fire_init(&fire);

    Turkey turkey;
    turkey_init(&turkey);
    turkey_set_season(&turkey, current_season);

    SnowglobeTransition sg = {0};
    MarshmallowState    roast = {0};

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
    bool  show_menu    = false;
    int   menu_cursor  = 0;
    bool  z_combo_used = false;
    float menu_timer   = 0.0f;
    float intro_timer  = 5.5f;
    float toast_timer  = 0.0f;
    char  toast[64]    = "";

    // Random wood crackle scheduler
    float next_crackle_t = 0.8f;

    // Setup Tiny3D perspective viewport once (zNear=0.50m, zFar=14.0m scaled to world units!)
    T3DViewport viewport = t3d_viewport_create_buffered(T3D_FB_COUNT);
    t3d_viewport_set_projection(&viewport,
                                T3D_DEG_TO_RAD(44.0f),
                                0.50f * T3D_WORLD_SCALE,
                                14.0f * T3D_WORLD_SCALE);

    int frame_idx = 0;
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
            joypad_buttons_t rel = joypad_get_buttons_released(JOYPAD_PORT_1);

            if (show_menu) {
                if (prs.start || prs.b) {
                    show_menu  = false;
                    menu_timer = 0.0f;
                } else if (prs.d_up) {
                    menu_cursor = (menu_cursor + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
                    menu_timer  = 14.0f;
                } else if (prs.d_down) {
                    menu_cursor = (menu_cursor + 1) % MENU_ITEM_COUNT;
                    menu_timer  = 14.0f;
                } else if (prs.d_left || prs.d_right || prs.a) {
                    int delta = prs.d_left ? -1 : +1;
                    menu_timer = 14.0f;
                    switch (menu_cursor) {
                        case 0: {
                            Season next_s = (Season)(((int)current_season + delta + SEASON_COUNT) % SEASON_COUNT);
                            start_season_transition(&sg, current_season, next_s, &show_menu, &intro_timer);
                            break;
                        }
                        case 1:
                            fire_cycle_theme(&fire, delta);
                            break;
                        case 2:
                            fire_adjust_intensity(&fire, delta * 0.15f);
                            break;
                        case 3:
                            rain_enabled = !rain_enabled;
                            break;
                        case 4:
                            ambient_enabled = !ambient_enabled;
                            break;
                        case 5:
                            cam_mode = (CameraMode)(((int)cam_mode + delta + CAM_MODE_COUNT) % CAM_MODE_COUNT);
                            auto_cam_timer = 0.0f;
                            break;
                        case 6:
                            if (prs.a) {
                                dim_enabled = !dim_enabled;
                            } else {
                                drift_mode = (DriftMode)(((int)drift_mode + delta + DRIFT_MODE_COUNT) % DRIFT_MODE_COUNT);
                            }
                            break;
                        case 7: {
                            turkey_cycle_summon(&turkey, delta);
                            static const char *const v_toasts[VISITOR_COUNT] = {
                                "Gobble gobble! (Turkey)",
                                "Ribbit ribbit! (Frog)",
                                "Meow~ (Tabby Cat)",
                                "Kwah-kwah! (Penguin)"
                            };
                            snprintf(toast, sizeof(toast), "%s", v_toasts[turkey.species]);
                            toast_timer = 2.2f;
                            show_menu   = false;
                            menu_timer  = 0.0f;
                            break;
                        }
                        case 8:
                        default:
                            if (exp_pak) {
                                video_mode = (VideoMode)(((int)video_mode + delta + VIDEO_MODE_COUNT) % VIDEO_MODE_COUNT);
                                apply_video_mode(video_mode, exp_pak, true, &viewport);
                            }
                            break;
                    }
                }
            } else {
                if (prs.start) {
                    show_menu   = true;
                    menu_timer  = 14.0f;
                    intro_timer = 0.0f;
                } else {
                    // Z+A toggles 3D marshmallow roasting; Z+L/R switches Season via Snowglobe
                    if (prs.z) {
                        z_combo_used = false;
                    }
                    if (inp.btn.z && prs.a) {
                        z_combo_used = true;
                        if (!roast.active) {
                            roast.active        = true;
                            roast.toast_level   = 0.0f;
                            roast.on_fire       = false;
                            roast.dinged_golden = false;
                            snprintf(toast, sizeof(toast), "Roasting marshmallow! (Z+A eat)");
                            toast_timer = 2.4f;
                        } else {
                            roast.active  = false;
                            roast.on_fire = false;
                            if (roast.toast_level < 0.35f) {
                                snprintf(toast, sizeof(toast), "Gooey & raw!");
                            } else if (roast.toast_level < 0.85f) {
                                snprintf(toast, sizeof(toast), "Perfect golden s'more!");
                                if (roast.toast_level >= 0.45f) play_ding_sfx();
                            } else if (roast.toast_level < 1.15f) {
                                snprintf(toast, sizeof(toast), "Toasty campfire crunch!");
                            } else {
                                snprintf(toast, sizeof(toast), "Crispy charcoal crunch!");
                            }
                            toast_timer = 2.4f;
                        }
                    } else if (!inp.btn.z) {
                        // Easter Eggs: A + B together while Turkey, Penguin, Frog, or Cat is on screen!
                        bool ab_combo = (inp.btn.a && prs.b) || (inp.btn.b && prs.a);
                        if (ab_combo && turkey_trigger_roast(&turkey)) {
                            const char *msg = (current_season == SEASON_SPRING) ? "Spring strut! Too pretty to roast!"
                                            : (current_season == SEASON_SUMMER) ? "Summer strut! Too hot to roast!"
                                                                                : "Wait... is that dinner?!";
                            snprintf(toast, sizeof(toast), "%s", msg);
                            toast_timer = 2.4f;
                        } else if (ab_combo && turkey_trigger_penguin_slide(&turkey)) {
                            snprintf(toast, sizeof(toast), "Weee! Penguin belly slide!");
                            toast_timer = 2.4f;
                        } else if (ab_combo && turkey_trigger_frog_magic(&turkey)) {
                            snprintf(toast, sizeof(toast), "Abracadabra, ribbit!");
                            toast_timer = 2.2f;
                        } else if (ab_combo && turkey_trigger_cat_nap(&turkey)) {
                            const char *msg = (current_season == SEASON_SPRING)    ? "Cozy spring blossom cat nap!"
                                            : (current_season == SEASON_SUMMER)    ? "Lazy summer cabin cat nap!"
                                            : (current_season == SEASON_CHRISTMAS) ? "Cozy Christmas cat nap!"
                                                                                   : "Cozy fireside cat nap!";
                            snprintf(toast, sizeof(toast), "%s", msg);
                            toast_timer = 2.6f;
                        } else {
                            if (prs.a) {
                                if (roast.active && roast.on_fire) {
                                    roast.on_fire     = false;
                                    roast.toast_level = 1.05f;
                                    play_stoke_sfx();
                                    snprintf(toast, sizeof(toast), "Whew! Blew it out!");
                                    toast_timer = 2.2f;
                                } else {
                                    fire_stoke(&fire);
                                    play_stoke_sfx();
                                    turkey_startle(&turkey);
                                    snprintf(toast, sizeof(toast), "Stoked the fire!");
                                    toast_timer = 2.0f;
                                }
                            }

                            if (prs.b) {
                                fire_toss_log(&fire);
                                turkey_startle(&turkey);
                                snprintf(toast, sizeof(toast), "Tossed a fresh log!");
                                toast_timer = 2.2f;
                            }
                        }
                    }

                    // Intensity adjustment (D-Pad Up / Down)
                    if (prs.d_up) {
                        fire_adjust_intensity(&fire, 0.15f);
                        snprintf(toast, sizeof(toast), "Fire Heat: %d%%", (int)(fire.intensity * 100.0f + 0.5f));
                        toast_timer = 2.2f;
                    }
                    if (prs.d_down) {
                        fire_adjust_intensity(&fire, -0.15f);
                        snprintf(toast, sizeof(toast), "Fire Heat: %d%%", (int)(fire.intensity * 100.0f + 0.5f));
                        toast_timer = 2.2f;
                    }

                    // D-Left / D-Right: Cycle & summon seasonal hearth visitors!
                    if (prs.d_left || prs.d_right) {
                        turkey_cycle_summon(&turkey, prs.d_right ? +1 : -1);
                        static const char *const v_toasts[VISITOR_COUNT] = {
                            "Gobble gobble! (Turkey)",
                            "Ribbit ribbit! (Frog)",
                            "Meow~ (Tabby Cat)",
                            "Kwah-kwah! (Penguin)"
                        };
                        snprintf(toast, sizeof(toast), "%s", v_toasts[turkey.species]);
                        toast_timer = 2.2f;
                    }

                    // Z+L/R quick-switches Season via Snowglobe; Z tap cycles CRT Drift
                    if (inp.btn.z && (prs.l || prs.r)) {
                        z_combo_used = true;
                        Season next_s = (Season)(((int)current_season + (prs.r ? 1 : -1) + SEASON_COUNT) % SEASON_COUNT);
                        start_season_transition(&sg, current_season, next_s, &show_menu, &intro_timer);
                    } else if (rel.z && !z_combo_used) {
                        drift_mode = (DriftMode)((drift_mode + 1) % DRIFT_MODE_COUNT);
                        snprintf(toast, sizeof(toast), "CRT Drift: %s", DRIFT_NAMES[drift_mode]);
                        toast_timer = 2.4f;
                    }

                    // L+R toggles CRT Night Dimmer; L or R (without Z held) cycles flame color
                    if (!inp.btn.z) {
                        if ((inp.btn.l && prs.r) || (inp.btn.r && prs.l)) {
                            dim_enabled = !dim_enabled;
                            snprintf(toast, sizeof(toast), "CRT Night Dimmer: %s", dim_enabled ? "ON" : "OFF");
                            toast_timer = 2.4f;
                        } else if (prs.r) {
                            fire_cycle_theme(&fire, +1);
                            snprintf(toast, sizeof(toast), "Flame: %s", FLAME_THEMES[fire.theme_idx].name);
                            toast_timer = 2.5f;
                        } else if (prs.l) {
                            fire_cycle_theme(&fire, -1);
                            snprintf(toast, sizeof(toast), "Flame: %s", FLAME_THEMES[fire.theme_idx].name);
                            toast_timer = 2.5f;
                        }
                    }

                    // C-Up: Toggle Seasonal Window Weather (Rain / Snowfall / Storm!)
                    if (prs.c_up) {
                        rain_enabled = !rain_enabled;
                        const char *w_name = (current_season == SEASON_CHRISTMAS) ? "Winter Snowfall"
                                           : (current_season == SEASON_SUMMER)    ? "Summer Storm"
                                           : (current_season == SEASON_SPRING)    ? "Spring Shower"
                                                                                  : "Cabin Rain";
                        snprintf(toast, sizeof(toast), "%s: %s", w_name, rain_enabled ? "ON" : "OFF");
                        toast_timer = 2.4f;
                    }

                    // C-Down: Toggle Seasonal N64 Chill Music
                    if (prs.c_down) {
                        ambient_enabled = !ambient_enabled;
                        snprintf(toast, sizeof(toast), "%s: %s",
                                 (current_season == SEASON_CHRISTMAS) ? "Winter Chimes" : "Ambient Synth",
                                 ambient_enabled ? "ON" : "OFF");
                        toast_timer = 2.4f;
                    }

                    // C-Left / C-Right: Cycle Camera Angles
                    if (prs.c_right) {
                        cam_mode = (CameraMode)((cam_mode + 1) % CAM_MODE_COUNT);
                        auto_cam_timer = 0.0f;
                        snprintf(toast, sizeof(toast), "Camera: %s", CAM_NAMES[cam_mode]);
                        toast_timer = 2.4f;
                    }
                    if (prs.c_left) {
                        cam_mode = (CameraMode)((cam_mode + CAM_MODE_COUNT - 1) % CAM_MODE_COUNT);
                        auto_cam_timer = 0.0f;
                        snprintf(toast, sizeof(toast), "Camera: %s", CAM_NAMES[cam_mode]);
                        toast_timer = 2.4f;
                    }
                }
            }

            // Analog stick manual gentle orbit (paused during Snowglobe zoom)
            if (sg.phase == SNOWGLOBE_IDLE && !show_menu) {
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
        // Update 3D Mantle Snowglobe Transition
        // ---------------------------------------------------------
        if (sg.phase == SNOWGLOBE_ZOOM_IN) {
            sg.timer += dt;
            if (sg.timer >= 0.85f) {
                sg.phase = SNOWGLOBE_SHAKE;
                sg.timer = 0.0f;
                play_snowglobe_sfx();
            }
        } else if (sg.phase == SNOWGLOBE_SHAKE) {
            sg.timer += dt;
            float u = clampf(sg.timer / 1.05f, 0.0f, 1.0f);
            sg.lift_y          = 0.085f * sinf(u * 3.14159f);
            sg.shake_angle     = sinf(u * 6.28318f * 4.0f) * 18.0f * (1.0f - 0.35f * u);
            sg.shake_intensity = sinf(u * 3.14159f);
            sg.flurry_alpha    = (u > 0.52f) ? clampf((u - 0.52f) / 0.44f, 0.0f, 1.0f) : 0.0f;

            if (u >= 0.90f && !sg.swapped) {
                sg.swapped     = true;
                current_season = sg.target_season;
                turkey_set_season(&turkey, current_season);
                fire_stoke(&fire);
                if (mixer_ch_playing(CH_RAIN))    mixer_ch_stop(CH_RAIN);
                if (mixer_ch_playing(CH_AMBIENT)) mixer_ch_stop(CH_AMBIENT);
                rain_vol    = 0.0f;
                ambient_vol = 0.0f;
            }
            if (sg.timer >= 1.05f) {
                sg.phase       = SNOWGLOBE_ZOOM_OUT;
                sg.timer       = 0.0f;
                sg.lift_y      = 0.0f;
                sg.shake_angle = 0.0f;
                snprintf(toast, sizeof(toast), "Season: %s!", season_name(current_season));
                toast_timer = 2.8f;
            }
        } else if (sg.phase == SNOWGLOBE_ZOOM_OUT) {
            sg.timer += dt;
            sg.shake_intensity = clampf(1.0f - sg.timer / 0.85f, 0.0f, 1.0f);
            sg.flurry_alpha    = clampf(1.0f - sg.timer / 0.72f, 0.0f, 1.0f);
            if (sg.timer >= 1.15f) {
                sg.phase           = SNOWGLOBE_IDLE;
                sg.shake_intensity = 0.0f;
                sg.flurry_alpha    = 0.0f;
            }
        }

        // ---------------------------------------------------------
        // Update Fire Simulation & Dynamic Audio
        // ---------------------------------------------------------
        fire_update(&fire, dt);
        turkey_update(&turkey, dt);
        float eff = fire_effective_intensity(&fire);

        // Update interactive 3D marshmallow roasting state
        {
            float tgt_ext = roast.active ? 1.0f : 0.0f;
            float diff_ext = tgt_ext - roast.extend;
            float step = dt * 3.2f;
            if (fabsf(diff_ext) <= step) {
                roast.extend = tgt_ext;
            } else {
                roast.extend += (diff_ext > 0.0f ? step : -step);
            }

            if (roast.active && roast.extend > 0.72f) {
                roast.spin += dt * 1.4f;
                roast.toast_level += dt * 0.11f * (0.65f + 0.55f * eff);
                if (!roast.dinged_golden && roast.toast_level >= 0.68f) {
                    roast.dinged_golden = true;
                    play_ding_sfx();
                    snprintf(toast, sizeof(toast), "Golden brown perfection! (Z+A)");
                    toast_timer = 2.8f;
                }
                if (!roast.on_fire && roast.toast_level >= 1.15f) {
                    roast.on_fire = true;
                    play_stoke_sfx();
                    snprintf(toast, sizeof(toast), "On fire! Press A to blow out!");
                    toast_timer = 2.8f;
                }
            }
        }

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
        if (turkey.just_penguined) {
            play_penguin_sfx(turkey.x);
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
            const char *msg = (current_season == SEASON_SPRING)    ? "POOF! Fresh Spring Berry Tart!"
                            : (current_season == SEASON_SUMMER)    ? "POOF! Campfire S'mores Platter!"
                            : (current_season == SEASON_CHRISTMAS) ? "POOF! Milk & Cookies for Santa!"
                                                                   : "POOF! Fresh Pumpkin Pie!";
            snprintf(toast, sizeof(toast), "%s", msg);
            toast_timer = 3.2f;
        }
        if (turkey.just_built_snowman) {
            play_ding_sfx();
            snprintf(toast, sizeof(toast), "Ta-da! A mini snowman!");
            toast_timer = 3.2f;
        }

        // Fire rumble volume follows intensity smoothly (keep native 22050 Hz for zero RSP resampling!)
        float bed_vol = clampf(0.28f + 0.42f * eff, 0.22f, 0.85f);
        mixer_ch_set_vol(CH_FIRE_BED, bed_vol, bed_vol);

        // On-demand streaming + smooth fade-in/out for Seasonal Weather & Music loops
        wav64_t *weather_loop = (current_season == SEASON_CHRISTMAS) ? &sfx_snow_loop : &sfx_rain;
        float rain_tgt = rain_enabled ? 0.56f : 0.0f;
        rain_vol += (rain_tgt - rain_vol) * clampf(dt * 1.8f, 0.0f, 1.0f);
        if (rain_vol > 0.008f) {
            if (!mixer_ch_playing(CH_RAIN)) {
                wav64_play(weather_loop, CH_RAIN);
            }
            mixer_ch_set_vol(CH_RAIN, rain_vol * 1.05f, rain_vol * 0.90f);
        } else if (mixer_ch_playing(CH_RAIN)) {
            mixer_ch_stop(CH_RAIN);
        }

        wav64_t *music_loop = (current_season == SEASON_CHRISTMAS) ? &sfx_christmas_loop : &sfx_ambient;
        float amb_tgt = ambient_enabled ? 0.52f : 0.0f;
        ambient_vol += (amb_tgt - ambient_vol) * clampf(dt * 1.4f, 0.0f, 1.0f);
        if (ambient_vol > 0.008f) {
            if (!mixer_ch_playing(CH_AMBIENT)) {
                wav64_play(music_loop, CH_AMBIENT);
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

        if (sg.phase == SNOWGLOBE_ZOOM_IN || sg.phase == SNOWGLOBE_SHAKE) {
            goal_ex = -1.36f;
            goal_ey =  2.22f;
            goal_ez =  1.06f;
            goal_tx = -1.44f;
            goal_ty =  2.15f;
            goal_tz =  0.28f;
        }

        // Ultra-smooth cinema glide between angles
        float smooth_rate = (sg.phase == SNOWGLOBE_ZOOM_IN || sg.phase == SNOWGLOBE_SHAKE) ? 4.5f
                          : (sg.phase == SNOWGLOBE_ZOOM_OUT) ? 3.2f : 1.65f;
        float smooth = clampf(dt * smooth_rate, 0.0f, 1.0f);
        cam_ex += (goal_ex - cam_ex) * smooth;
        cam_ey += (goal_ey - cam_ey) * smooth;
        cam_ez += (goal_ez - cam_ez) * smooth;
        cam_tx += (goal_tx - cam_tx) * smooth;
        cam_ty += (goal_ty - cam_ty) * smooth;
        cam_tz += (goal_tz - cam_tz) * smooth;

        // ---------------------------------------------------------
        // Render Frame (Tiny3D Native RSP/RDP + RDPQ HUD)
        // ---------------------------------------------------------
        int queued_before = audio_get_queued_buffers();
        pump_audio();

        frame_idx = (frame_idx + 1) % T3D_FB_COUNT;
        t3d_gfx_frame_begin(frame_idx);
        t3d_viewport_look_at(
            &viewport,
            &(T3DVec3){{ cam_ex * T3D_WORLD_SCALE, cam_ey * T3D_WORLD_SCALE, cam_ez * T3D_WORLD_SCALE }},
            &(T3DVec3){{ cam_tx * T3D_WORLD_SCALE, cam_ty * T3D_WORLD_SCALE, cam_tz * T3D_WORLD_SCALE }},
            &(T3DVec3){{ 0.0f, 1.0f, 0.0f }}
        );

        surface_t *disp;
        while (!(disp = display_try_get())) {
            pump_audio();
        }
        surface_t *zbuf = display_get_zbuf();
        rdpq_attach(disp, zbuf);

        t3d_frame_start();
        if (exp_pak && video_mode == VIDEO_240P_32) {
            rdpq_mode_begin();
                rdpq_mode_dithering(DITHER_NONE_NONE);
            rdpq_mode_end();
        }
        t3d_viewport_attach(&viewport);
        t3d_screen_clear_color(RGBA32(10, 7, 6, 255));
        t3d_screen_clear_depth();

        scene_render(&tex, &fire, &turkey, current_season, &sg, &roast,
                     rain_enabled, cam_ex, cam_ey, cam_ez, dim_factor);

        // Draw crisp 2D HUD/toasts on top via RDPQ
        draw_hud_overlay(1, show_menu, menu_cursor, toast, toast_timer, intro_timer,
                         &fire, &turkey, current_season, &sg,
                         rtc_detected, &rtc_now,
                         rain_enabled, ambient_enabled, dim_enabled,
                         cam_mode, drift_mode, video_mode, exp_pak);

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
