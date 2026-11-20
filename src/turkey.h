#ifndef HEARTH_TURKEY_H
#define HEARTH_TURKEY_H

#include <stdbool.h>
#include <GL/gl.h>

typedef enum {
    VISITOR_TURKEY = 0,
    VISITOR_FROG   = 1,
    VISITOR_CAT    = 2,
    VISITOR_COUNT  = 3
} VisitorSpecies;

typedef enum {
    TURKEY_HIDDEN = 0,
    TURKEY_WALK_IN,
    TURKEY_PAUSE_GOBBLE,
    TURKEY_WALK_OUT,
    TURKEY_SCURRY,
    TURKEY_WALK_TO_FIRE,
    TURKEY_ROASTING,
    TURKEY_ROAST_POP,
    TURKEY_SERVED,
    TURKEY_FROG_CAST,
    TURKEY_CAT_NAP
} TurkeyState;

typedef struct {
    VisitorSpecies species;
    TurkeyState state;
    float x, y, z;
    float dir;          // +1.0 (left -> right) or -1.0 (right -> left)
    float yaw;          // Facing angle in degrees
    float walk_phase;   // Stride / breathing animation phase
    float state_timer;  // Countdown for pause, roasting, served display, nap, or next spawn
    float pause_x;      // Spot in front of the fire to pause & call out
    float hop_y;        // Jump height
    float hop_vy;
    float hop_pause;    // Brief crouch beat between frog hops
    float tail_flare;   // 0..1 tail puff (turkey) / throat sac (frog) / tail swish excitement (cat)
    float head_turn;    // Look toward camera while pausing
    float pop_vx;       // Horizontal velocity during roast pop-out arc
    float pop_vz;
    float spin_angle;   // Playful spin while popping out / gently showcasing roast

    // Frog wizard pumpkin-to-pie Easter egg state (persists even if another visitor arrives!)
    float cast_elapsed;     // 0..1.7s during TURKEY_FROG_CAST
    bool  cast_transformed; // Has the magic bolt hit the pumpkin yet?
    float pie_timer;        // > 0 means left hearth pumpkin is currently a Pumpkin Pie!
    float pie_pop_y;        // Cute bounce when pumpkin pops into pie
    float pie_pop_vy;
    float magic_burst;      // > 0 sparkle burst timer around the pumpkin/pie

    bool  just_gobbled;      // Consumed by main.c to play gobble.wav64
    bool  just_ribbit;       // Consumed by main.c to play ribbit.wav64
    bool  just_meowed;       // Consumed by main.c to play meow.wav64
    bool  just_entered_fire; // Consumed by main.c to flare fire & play whoosh
    bool  just_dinged;       // Consumed by main.c to play ding.wav64 + toast
    bool  just_cast_magic;   // Consumed by main.c to play magic.wav64
    bool  just_made_pie;     // Consumed by main.c to show pie toast
} Turkey;

void turkey_init(Turkey *tk);
void turkey_update(Turkey *tk, float dt);
void turkey_summon_species(Turkey *tk, VisitorSpecies sp);
void turkey_cycle_summon(Turkey *tk, int delta); // Cycle Turkey / Frog / Cat on demand!
void turkey_startle(Turkey *tk);
bool turkey_trigger_roast(Turkey *tk);        // A+B on turkey: walks into fire & pops out roasted!
bool turkey_trigger_frog_magic(Turkey *tk);   // A+B on frog: waves wand -> pumpkin pie!
bool turkey_trigger_cat_nap(Turkey *tk);      // A+B on cat: curls up for a cozy fireside nap!
void turkey_render(const Turkey *tk);
void turkey_render_magic_fx(const Turkey *tk, GLuint tex_spark, float cam_x, float cam_z);

#endif
