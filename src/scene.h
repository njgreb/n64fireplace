#ifndef HEARTH_SCENE_H
#define HEARTH_SCENE_H

#include <stdbool.h>
#include "fire.h"
#include "season.h"
#include "t3d_gfx.h"
#include "turkey.h"

typedef struct {
    T3DTexture stone;
    T3DTexture brick;
    T3DTexture wood_mantle;
    T3DTexture floor;
    T3DTexture bark;
    T3DTexture log_end;
    T3DTexture coals;
    T3DTexture rug;
    T3DTexture iron;
    T3DTexture painting;
    T3DTexture painting_spring;
    T3DTexture painting_summer;
    T3DTexture painting_xmas;
    T3DTexture flame;
    T3DTexture glow;
    T3DTexture spark;
    T3DMesh base_mesh;
    T3DMesh spring_mesh;
    T3DMesh summer_mesh;
    T3DMesh thanksgiving_mesh;
    T3DMesh christmas_mesh;
    T3DMesh snowglobe_mesh[SEASON_COUNT];
    T3DMesh snowglobe_dome_mesh;
    T3DMesh fire_logs_mesh;
    T3DMesh left_gift_mesh;
    T3DMesh left_cookies_mesh;
    T3DMesh left_pumpkin_mesh;
    T3DMesh left_pie_mesh;
    T3DMesh left_spring_mesh;
    T3DMesh left_summer_mesh;
    T3DMesh left_berry_tart_mesh;
    T3DMesh left_smores_mesh;
} SceneTextures;

typedef struct {
    bool  active;
    float extend;        // 0.0 (retracted) .. 1.0 (over flames)
    float toast_level;   // 0.0 (raw) -> 0.68 (golden) -> 1.15+ (on fire)
    float spin;
    bool  on_fire;
    bool  dinged_golden;
} MarshmallowState;

void scene_load_textures(SceneTextures *tex);
void scene_render(const SceneTextures *tex,
                  const FireSystem *fs,
                  const Turkey *turkey,
                  Season season,
                  const SnowglobeTransition *sg,
                  const MarshmallowState *roast,
                  bool rain_enabled,
                  float cam_x, float cam_y, float cam_z,
                  float dim_factor);

#endif
