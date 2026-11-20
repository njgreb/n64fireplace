#ifndef HEARTH_FIRE_H
#define HEARTH_FIRE_H

#include <stdbool.h>
#include <GL/gl.h>

#define NUM_FLAME_THEMES 5
#define MAX_FLAME_PARTICLES 16
#define MAX_SPARK_PARTICLES 26
#define MAX_STEAM_PARTICLES 5
#define MAX_TOSSED_LOGS 3

typedef struct {
    const char *name;
    // Outer / mid / core flame colors (0..1)
    float outer_r, outer_g, outer_b;
    float mid_r,   mid_g,   mid_b;
    float core_r,  core_g,  core_b;
    // Dynamic scene point light color
    float light_r, light_g, light_b;
    // Coal / ember tint
    float coal_r,  coal_g,  coal_b;
    // Spark color
    float spark_r, spark_g, spark_b;
} FlameTheme;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float age, life;
    float base_w, base_h;
    float phase;
    float sway_amp;
    int   layer; // 0 = outer rich, 1 = mid bright, 2 = hot inner core
} FlameParticle;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float age, life;
    float size;
    float phase;
    float swirl;
} SparkParticle;

typedef struct {
    float x, y, z;
    float age, life;
    float phase;
} SteamParticle;

typedef struct {
    bool  active;
    float x, y, z;
    float vx, vy, vz;
    float rot_y, rot_z;
    float target_y;
    bool  landed;
    float burn_age;
    float burn_life;
} TossedLog;

typedef struct {
    int   theme_idx;
    float intensity;        // User target 0.25 .. 1.35
    float stoke_boost;      // Temporary flare when stoked or log tossed
    float time;

    // Smoothed flicker values for lighting & glow
    float flicker;          // ~0.75 .. 1.25
    float flicker_x;
    float flicker_y;
    float flicker_z;

    FlameParticle flames[MAX_FLAME_PARTICLES];
    SparkParticle sparks[MAX_SPARK_PARTICLES];
    SteamParticle steam[MAX_STEAM_PARTICLES];
    TossedLog     tossed_logs[MAX_TOSSED_LOGS];

    bool  log_just_landed;  // Event flag consumed by main for audio thud
} FireSystem;

extern const FlameTheme FLAME_THEMES[NUM_FLAME_THEMES];

void fire_init(FireSystem *fs);
void fire_update(FireSystem *fs, float dt);
void fire_stoke(FireSystem *fs);
void fire_toss_log(FireSystem *fs);
void fire_cycle_theme(FireSystem *fs, int delta);
void fire_adjust_intensity(FireSystem *fs, float delta);
float fire_effective_intensity(const FireSystem *fs);

// Render flames, glows, sparks, and candle flames/mug steam
void fire_render_effects(const FireSystem *fs,
                         GLuint tex_flame, GLuint tex_glow, GLuint tex_spark,
                         float cam_x, float cam_y, float cam_z);

#endif
