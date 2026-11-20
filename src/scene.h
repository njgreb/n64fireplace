#ifndef HEARTH_SCENE_H
#define HEARTH_SCENE_H

#include <stdbool.h>
#include <GL/gl.h>
#include "fire.h"
#include "turkey.h"

typedef struct {
    GLuint stone;
    GLuint brick;
    GLuint wood_mantle;
    GLuint floor;
    GLuint bark;
    GLuint log_end;
    GLuint coals;
    GLuint rug;
    GLuint iron;
    GLuint painting;
    GLuint flame;
    GLuint glow;
    GLuint spark;
    GLuint static_list;
} SceneTextures;

void scene_load_textures(SceneTextures *tex);
void scene_render(const SceneTextures *tex,
                  const FireSystem *fs,
                  const Turkey *turkey,
                  bool rain_enabled,
                  float cam_x, float cam_y, float cam_z,
                  float dim_factor);

#endif
