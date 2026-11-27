#ifndef HEARTH_SEASON_H
#define HEARTH_SEASON_H

#include <stdbool.h>

typedef enum {
    SEASON_SPRING       = 0,
    SEASON_SUMMER       = 1,
    SEASON_THANKSGIVING = 2,
    SEASON_CHRISTMAS    = 3,
    SEASON_COUNT        = 4
} Season;

typedef enum {
    SNOWGLOBE_IDLE = 0,
    SNOWGLOBE_ZOOM_IN,
    SNOWGLOBE_SHAKE,
    SNOWGLOBE_ZOOM_OUT
} SnowglobePhase;

typedef struct {
    SnowglobePhase phase;
    Season target_season;
    float timer;
    float shake_intensity;
    float lift_y;
    float shake_angle;
    float flurry_alpha;
    bool swapped;
} SnowglobeTransition;

static inline const char *season_name(Season s) {
    switch (s) {
        case SEASON_SPRING:       return "Spring Blossom";
        case SEASON_SUMMER:       return "Summer Cabin";
        case SEASON_CHRISTMAS:    return "Cozy Christmas";
        case SEASON_THANKSGIVING:
        default:                  return "Thanksgiving";
    }
}

#endif
