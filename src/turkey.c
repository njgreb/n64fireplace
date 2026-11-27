#include "turkey.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static uint32_t tk_rng = 0x7096E123u;

static float tkrand01(void) {
    tk_rng = tk_rng * 1664525u + 1013904223u;
    return (float)((tk_rng >> 8) & 0xFFFFu) / 65535.0f;
}

static float tkrand(float lo, float hi) {
    return lo + (hi - lo) * tkrand01();
}

static void trigger_voice(Turkey *tk) {
    if (tk->species == VISITOR_FROG) {
        tk->just_ribbit = true;
    } else if (tk->species == VISITOR_CAT) {
        tk->just_meowed = true;
    } else if (tk->species == VISITOR_PENGUIN) {
        tk->just_penguined = true;
    } else {
        tk->just_gobbled = true;
    }
}

static void start_visit(Turkey *tk, VisitorSpecies species) {
    tk->species     = species;
    tk->dir         = (tkrand01() > 0.5f) ? 1.0f : -1.0f;
    tk->x           = -tk->dir * 2.25f;
    tk->y           = 0.18f; // On the raised stone hearth slab
    tk->z           = 0.50f + tkrand(-0.04f, 0.08f);
    tk->yaw         = (tk->dir > 0.0f) ? 90.0f : -90.0f;
    tk->pause_x     = tkrand(-0.28f, 0.28f);
    tk->walk_phase  = 0.0f;
    tk->state_timer = 0.0f;
    tk->hop_y       = 0.0f;
    tk->hop_vy      = 0.0f;
    tk->hop_pause   = 0.12f;
    tk->tail_flare  = 0.0f;
    tk->head_turn   = 0.0f;
    tk->state       = TURKEY_WALK_IN;
}

void turkey_init(Turkey *tk) {
    memset(tk, 0, sizeof(*tk));
    tk->season      = SEASON_THANKSGIVING;
    tk->species     = VISITOR_TURKEY;
    tk->state       = TURKEY_HIDDEN;
    // First visitor arrives ~7 seconds after boot
    tk->state_timer = 7.0f;
}

void turkey_set_season(Turkey *tk, Season season) {
    tk->season        = season;
    tk->pie_timer     = 0.0f;
    tk->snowman_timer = 0.0f;
    tk->magic_burst   = 0.0f;

    if (tk->state == TURKEY_HIDDEN) {
        if (tk->state_timer > 4.0f) tk->state_timer = 4.0f;
        return;
    }

    if ((season == SEASON_CHRISTMAS && tk->species == VISITOR_TURKEY) ||
        (season != SEASON_CHRISTMAS && tk->species == VISITOR_PENGUIN)) {
        tk->state       = TURKEY_HIDDEN;
        tk->state_timer = 3.5f;
    }
}

void turkey_summon_species(Turkey *tk, VisitorSpecies sp) {
    start_visit(tk, sp);
}

void turkey_cycle_summon(Turkey *tk, int delta) {
    static const VisitorSpecies roster[SEASON_COUNT][3] = {
        { VISITOR_FROG,    VISITOR_CAT,  VISITOR_TURKEY }, // Spring
        { VISITOR_CAT,     VISITOR_FROG, VISITOR_TURKEY }, // Summer
        { VISITOR_TURKEY,  VISITOR_FROG, VISITOR_CAT    }, // Thanksgiving
        { VISITOR_PENGUIN, VISITOR_FROG, VISITOR_CAT    }, // Christmas
    };
    int sidx = ((int)tk->season >= 0 && (int)tk->season < SEASON_COUNT) ? (int)tk->season : (int)SEASON_THANKSGIVING;
    const VisitorSpecies *r = roster[sidx];
    if (tk->state == TURKEY_HIDDEN) {
        start_visit(tk, (delta >= 0) ? r[0] : r[2]);
    } else {
        int cur = 0;
        for (int i = 0; i < 3; i++) {
            if (r[i] == tk->species) {
                cur = i;
                break;
            }
        }
        int next_idx = (cur + delta + 3) % 3;
        start_visit(tk, r[next_idx]);
    }
}

void turkey_startle(Turkey *tk) {
    if (tk->state == TURKEY_HIDDEN) return;
    // Don't interrupt active Easter eggs!
    if (tk->state >= TURKEY_WALK_TO_FIRE) return;
    if (fabsf(tk->x) < 1.60f && tk->state != TURKEY_SCURRY) {
        tk->state      = TURKEY_SCURRY;
        tk->hop_vy     = (tk->species == VISITOR_FROG)    ? 2.40f
                       : (tk->species == VISITOR_CAT)     ? 2.05f
                       : (tk->species == VISITOR_PENGUIN) ? 1.50f : 1.65f;
        tk->hop_pause  = 0.0f;
        tk->tail_flare = 1.0f;
        trigger_voice(tk);
    }
}

bool turkey_trigger_roast(Turkey *tk) {
    if (tk->species != VISITOR_TURKEY) return false;
    if (tk->state != TURKEY_WALK_IN &&
        tk->state != TURKEY_PAUSE_GOBBLE &&
        tk->state != TURKEY_WALK_OUT &&
        tk->state != TURKEY_SCURRY) {
        return false;
    }
    if (tk->season == SEASON_THANKSGIVING) {
        tk->state        = TURKEY_WALK_TO_FIRE;
        tk->state_timer  = 0.0f;
        tk->hop_vy       = 0.95f;
        tk->tail_flare   = 1.0f;
        tk->just_gobbled = true;
    } else {
        tk->state        = TURKEY_STRUT_DANCE;
        tk->state_timer  = 3.6f;
        tk->hop_vy       = 1.65f;
        tk->tail_flare   = 1.0f;
        tk->just_gobbled = true;
        tk->magic_burst  = 1.15f;
        tk->burst_x      = tk->x;
        tk->burst_z      = tk->z;
    }
    return true;
}

bool turkey_trigger_frog_magic(Turkey *tk) {
    if (tk->species != VISITOR_FROG) return false;
    if (tk->state != TURKEY_WALK_IN &&
        tk->state != TURKEY_PAUSE_GOBBLE &&
        tk->state != TURKEY_WALK_OUT &&
        tk->state != TURKEY_SCURRY) {
        return false;
    }
    tk->state            = TURKEY_FROG_CAST;
    tk->cast_elapsed     = 0.0f;
    tk->cast_transformed = false;
    tk->tail_flare       = 0.8f;
    tk->just_cast_magic  = true;
    return true;
}

bool turkey_trigger_cat_nap(Turkey *tk) {
    if (tk->species != VISITOR_CAT) return false;
    if (tk->state != TURKEY_WALK_IN &&
        tk->state != TURKEY_PAUSE_GOBBLE &&
        tk->state != TURKEY_WALK_OUT &&
        tk->state != TURKEY_SCURRY) {
        return false;
    }
    tk->state       = TURKEY_CAT_NAP;
    tk->state_timer = 18.0f;
    tk->just_meowed = true;
    return true;
}

bool turkey_trigger_penguin_slide(Turkey *tk) {
    if (tk->species != VISITOR_PENGUIN) return false;
    if (tk->state != TURKEY_WALK_IN &&
        tk->state != TURKEY_PAUSE_GOBBLE &&
        tk->state != TURKEY_WALK_OUT &&
        tk->state != TURKEY_SCURRY) {
        return false;
    }
    tk->state          = TURKEY_PENGUIN_SLIDE;
    tk->slide_elapsed  = 0.0f;
    tk->hop_vy         = 1.45f;
    tk->tail_flare     = 1.0f;
    tk->just_penguined = true;
    return true;
}

void turkey_update(Turkey *tk, float dt) {
    tk->just_gobbled       = false;
    tk->just_ribbit        = false;
    tk->just_meowed        = false;
    tk->just_penguined     = false;
    tk->just_entered_fire  = false;
    tk->just_dinged        = false;
    tk->just_cast_magic    = false;
    tk->just_made_pie      = false;
    tk->just_built_snowman = false;

    // Update persistent Pumpkin Pie / Milk & Cookies, Mini Snowman & magic sparkle burst timers
    if (tk->pie_timer > 0.0f) {
        tk->pie_timer -= dt;
        if (tk->pie_timer < 0.0f) tk->pie_timer = 0.0f;
    }
    if (tk->pie_pop_y > 0.0f || tk->pie_pop_vy > 0.0f) {
        tk->pie_pop_vy -= 7.5f * dt;
        tk->pie_pop_y  += tk->pie_pop_vy * dt;
        if (tk->pie_pop_y < 0.0f) {
            tk->pie_pop_y  = 0.0f;
            tk->pie_pop_vy = 0.0f;
        }
    }
    if (tk->snowman_timer > 0.0f) {
        tk->snowman_timer -= dt;
        if (tk->snowman_timer < 0.0f) tk->snowman_timer = 0.0f;
    }
    if (tk->snowman_pop_y > 0.0f || tk->snowman_pop_vy > 0.0f) {
        tk->snowman_pop_vy -= 7.5f * dt;
        tk->snowman_pop_y  += tk->snowman_pop_vy * dt;
        if (tk->snowman_pop_y < 0.0f) {
            tk->snowman_pop_y  = 0.0f;
            tk->snowman_pop_vy = 0.0f;
        }
    }
    if (tk->magic_burst > 0.0f) {
        tk->magic_burst -= dt;
        if (tk->magic_burst < 0.0f) tk->magic_burst = 0.0f;
    }

    if (tk->state == TURKEY_HIDDEN) {
        tk->state_timer -= dt;
        if (tk->state_timer <= 0.0f) {
            float r = tkrand01();
            VisitorSpecies sp;
            if (tk->season == SEASON_CHRISTMAS) {
                sp = (r < 0.45f) ? VISITOR_PENGUIN
                   : (r < 0.73f) ? VISITOR_FROG
                                 : VISITOR_CAT;
            } else if (tk->season == SEASON_SPRING) {
                sp = (r < 0.46f) ? VISITOR_FROG
                   : (r < 0.78f) ? VISITOR_CAT
                                 : VISITOR_TURKEY;
            } else if (tk->season == SEASON_SUMMER) {
                sp = (r < 0.48f) ? VISITOR_CAT
                   : (r < 0.80f) ? VISITOR_FROG
                                 : VISITOR_TURKEY;
            } else {
                sp = (r < 0.40f) ? VISITOR_TURKEY
                   : (r < 0.70f) ? VISITOR_FROG
                                 : VISITOR_CAT;
            }
            start_visit(tk, sp);
        }
        return;
    }

    // ---------------------------------------------------------
    // COZY FIRESIDE CAT NAP EASTER EGG STATE
    // ---------------------------------------------------------
    if (tk->state == TURKEY_CAT_NAP) {
        if (tk->hop_y > 0.0f || tk->hop_vy > 0.0f) {
            tk->hop_vy -= 7.2f * dt;
            tk->hop_y  += tk->hop_vy * dt;
            if (tk->hop_y < 0.0f) { tk->hop_y = 0.0f; tk->hop_vy = 0.0f; }
        }
        float ground_y = (fabsf(tk->x) <= 1.72f) ? 0.18f : 0.0f;
        tk->y += (ground_y - tk->y) * fminf(1.0f, dt * 12.0f);
        // Settle gently toward center hearth if near edges
        if (fabsf(tk->x) > 0.75f) {
            tk->x += ((tk->x > 0.0f ? 0.45f : -0.45f) - tk->x) * fminf(1.0f, dt * 2.5f);
        }
        float goal_yaw = (tk->dir > 0.0f) ? 40.0f : -40.0f;
        tk->yaw += (goal_yaw - tk->yaw) * fminf(1.0f, dt * 4.5f);
        tk->walk_phase += dt * 2.0f; // slow peaceful sleeping breath!
        tk->state_timer -= dt;
        if (tk->state_timer <= 0.0f) {
            tk->state       = TURKEY_WALK_OUT;
            tk->just_meowed = true;
        }
        return;
    }

    // ---------------------------------------------------------
    // FROG WIZARD PUMPKIN-TO-PIE EASTER EGG STATE
    // ---------------------------------------------------------
    if (tk->state == TURKEY_FROG_CAST) {
        // Settle gently onto the stone hearth if mid-hop
        if (tk->hop_y > 0.0f || tk->hop_vy > 0.0f) {
            tk->hop_vy -= 8.2f * dt;
            tk->hop_y  += tk->hop_vy * dt;
            if (tk->hop_y < 0.0f) { tk->hop_y = 0.0f; tk->hop_vy = 0.0f; }
        }
        float ground_y = (fabsf(tk->x) <= 1.72f) ? 0.18f : 0.0f;
        tk->y += (ground_y - tk->y) * fminf(1.0f, dt * 14.0f);

        // Turn toward the big left hearth pumpkin at (-1.46, 0.18, 0.34)!
        float dx = -1.46f - tk->x;
        float dz =  0.34f - tk->z;
        float goal_yaw = atan2f(dx, dz) * 57.29578f;
        tk->yaw += (goal_yaw - tk->yaw) * fminf(1.0f, dt * 10.0f);

        tk->cast_elapsed += dt;
        tk->walk_phase   += dt * 8.0f;

        // At t = 0.72s, the magic beam hits the left hearth prop -> POOF! Pie or Milk & Cookies!
        if (!tk->cast_transformed && tk->cast_elapsed >= 0.72f) {
            tk->cast_transformed = true;
            tk->pie_timer        = 45.0f; // stays transformed for 45s
            tk->pie_pop_y        = 0.04f;
            tk->pie_pop_vy       = 1.65f;
            tk->burst_x          = -1.46f;
            tk->burst_z          = 0.34f;
            tk->magic_burst      = 1.15f;
            tk->just_made_pie    = true;
        }

        // At t = 1.65s, celebratory ribbit + hop, then resume hopping!
        if (tk->cast_elapsed >= 1.65f) {
            tk->state       = TURKEY_WALK_OUT;
            tk->hop_vy      = 1.85f;
            tk->hop_pause   = 0.18f;
            tk->tail_flare  = 1.0f;
            tk->just_ribbit = true;
        }
        return;
    }

    // ---------------------------------------------------------
    // ROAST TURKEY EASTER EGG STATES
    // ---------------------------------------------------------
    if (tk->state == TURKEY_WALK_TO_FIRE) {
        // March straight toward the center of the roaring firebox (0.0, 0.22, -0.34)!
        float dx = 0.0f - tk->x;
        float dz = -0.34f - tk->z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (tk->hop_y > 0.0f || tk->hop_vy > 0.0f) {
            tk->hop_vy -= 6.8f * dt;
            tk->hop_y  += tk->hop_vy * dt;
            if (tk->hop_y < 0.0f) { tk->hop_y = 0.0f; tk->hop_vy = 0.0f; }
        }
        if (dist > 0.08f) {
            float spd = 0.92f;
            tk->x += (dx / dist) * spd * dt;
            tk->z += (dz / dist) * spd * dt;
            float goal_yaw = atan2f(dx, dz) * 57.29578f;
            tk->yaw += (goal_yaw - tk->yaw) * fminf(1.0f, dt * 8.0f);
            tk->walk_phase += dt * 11.5f;
        } else {
            // Poof! Disappear into the flames to roast!
            tk->state             = TURKEY_ROASTING;
            tk->state_timer       = 1.55f;
            tk->just_entered_fire = true;
            tk->hop_y             = 0.0f;
            tk->hop_vy            = 0.0f;
        }
        return;
    }

    if (tk->state == TURKEY_ROASTING) {
        tk->state_timer -= dt;
        if (tk->state_timer <= 0.0f) {
            // DING!! Launch the fully cooked roast turkey platter out of the fire!
            tk->state       = TURKEY_ROAST_POP;
            tk->x           = 0.0f;
            tk->y           = 0.18f;
            tk->z           = -0.34f;
            tk->hop_y       = 0.32f;
            tk->hop_vy      = 2.65f;
            tk->pop_vx      = 0.0f;
            tk->pop_vz      = 1.15f; // arcs out onto center hearth slab at z ~ 0.56!
            tk->spin_angle  = 0.0f;
            tk->just_dinged = true;
        }
        return;
    }

    if (tk->state == TURKEY_ROAST_POP) {
        tk->hop_vy     -= 7.2f * dt;
        tk->hop_y      += tk->hop_vy * dt;
        tk->x          += tk->pop_vx * dt;
        tk->z          += tk->pop_vz * dt;
        tk->spin_angle += 460.0f * dt;
        if (tk->z > 0.56f) {
            tk->z = 0.56f;
            tk->pop_vz = 0.0f;
        }
        if (tk->hop_y <= 0.0f) {
            tk->hop_y = 0.0f;
            if (tk->hop_vy < -0.95f) {
                // Cute little platter bounce on landing!
                tk->hop_vy = -tk->hop_vy * 0.34f;
            } else {
                tk->hop_vy      = 0.0f;
                tk->state       = TURKEY_SERVED;
                tk->state_timer = 15.0f;
                tk->spin_angle  = 20.0f;
            }
        }
        return;
    }

    if (tk->state == TURKEY_SERVED) {
        tk->state_timer -= dt;
        // Slow celebratory turntable rotation on the hearth so everyone can admire dinner!
        tk->spin_angle += 24.0f * dt;
        if (tk->state_timer <= 0.0f) {
            tk->state       = TURKEY_HIDDEN;
            tk->state_timer = tkrand(20.0f, 36.0f);
        }
        return;
    }

    if (tk->state == TURKEY_STRUT_DANCE) {
        tk->state_timer -= dt;
        tk->hop_vy      -= 7.2f * dt;
        tk->hop_y       += tk->hop_vy * dt;
        if (tk->hop_y <= 0.0f) {
            tk->hop_y = 0.0f;
            if (tk->state_timer > 0.35f) {
                tk->hop_vy = 1.35f;
                if (tk->state_timer > 1.6f && tk->state_timer < 2.2f) {
                    tk->just_gobbled = true;
                }
            } else {
                tk->hop_vy = 0.0f;
            }
        }
        tk->yaw        += 210.0f * dt;
        tk->tail_flare  = 1.0f;
        tk->walk_phase += dt * 14.0f;
        if (tk->state_timer <= 0.0f) {
            tk->state      = TURKEY_WALK_OUT;
            tk->hop_y      = 0.0f;
            tk->hop_vy     = 0.0f;
            tk->tail_flare = 0.35f;
        }
        return;
    }

    // ---------------------------------------------------------
    // SM64 PENGUIN BELLY-SLIDE -> MINI SNOWMAN EASTER EGG
    // ---------------------------------------------------------
    if (tk->state == TURKEY_PENGUIN_SLIDE) {
        if (tk->hop_y > 0.0f || tk->hop_vy > 0.0f) {
            tk->hop_vy -= 7.2f * dt;
            tk->hop_y  += tk->hop_vy * dt;
            if (tk->hop_y < 0.0f) { tk->hop_y = 0.0f; tk->hop_vy = 0.0f; }
        }
        float ground_y = (fabsf(tk->x) <= 1.72f) ? 0.18f : 0.0f;
        tk->y += (ground_y - tk->y) * fminf(1.0f, dt * 14.0f);

        tk->slide_elapsed += dt;
        float u = fminf(1.0f, tk->slide_elapsed / 1.85f);
        float prev_x = tk->x;
        float prev_z = tk->z;
        // Swoop across the hearth on belly, then settle beside center-right to pop a Mini Snowman!
        float gx = (u < 0.45f) ? -0.48f : 0.44f;
        float gz = (u < 0.45f) ?  0.64f : 0.52f;
        float rate = (u < 0.45f) ? 3.4f : 3.8f;
        tk->x += (gx - tk->x) * fminf(1.0f, dt * rate);
        tk->z += (gz - tk->z) * fminf(1.0f, dt * 4.5f);

        float dx = tk->x - prev_x;
        float dz = tk->z - prev_z;
        if (fabsf(dx) + fabsf(dz) > 0.0005f) {
            float goal_yaw = atan2f(dx, dz) * 57.29578f;
            tk->yaw += (goal_yaw - tk->yaw) * fminf(1.0f, dt * 8.0f);
        }
        tk->walk_phase += dt * 12.0f;

        if (tk->slide_elapsed >= 1.85f) {
            tk->state              = TURKEY_PENGUIN_SNOWMAN;
            tk->state_timer        = 2.4f;
            tk->hop_vy             = 1.55f;
            tk->snowman_x          = 0.0f;
            tk->snowman_z          = 0.54f;
            tk->snowman_timer      = 35.0f;
            tk->snowman_pop_y      = 0.04f;
            tk->snowman_pop_vy     = 1.85f;
            tk->burst_x            = 0.0f;
            tk->burst_z            = 0.54f;
            tk->magic_burst        = 1.15f;
            tk->just_built_snowman = true;
        }
        return;
    }

    if (tk->state == TURKEY_PENGUIN_SNOWMAN) {
        if (tk->hop_y > 0.0f || tk->hop_vy > 0.0f) {
            tk->hop_vy -= 7.2f * dt;
            tk->hop_y  += tk->hop_vy * dt;
            if (tk->hop_y < 0.0f) { tk->hop_y = 0.0f; tk->hop_vy = 0.0f; }
        }
        float prev_t = tk->state_timer;
        tk->state_timer -= dt;
        if (tk->hop_y <= 0.0f && tk->hop_vy <= 0.0f && tk->state_timer > 0.4f &&
            (int)(tk->state_timer * 2.0f) != (int)(prev_t * 2.0f)) {
            tk->hop_vy = 0.85f;
        }
        float goal_yaw = -42.0f;
        tk->yaw += (goal_yaw - tk->yaw) * fminf(1.0f, dt * 6.5f);
        tk->walk_phase += dt * 10.0f;
        if (tk->state_timer <= 0.0f) {
            tk->state          = TURKEY_WALK_OUT;
            tk->dir            = 1.0f;
            tk->just_penguined = true;
        }
        return;
    }

    // Hop/jump physics
    bool in_air = (tk->hop_y > 0.0f || tk->hop_vy > 0.0f);
    if (in_air) {
        float grav = (tk->species == VISITOR_FROG) ? 8.2f : 6.8f;
        tk->hop_vy -= grav * dt;
        tk->hop_y  += tk->hop_vy * dt;
        if (tk->hop_y <= 0.0f) {
            tk->hop_y  = 0.0f;
            tk->hop_vy = 0.0f;
            in_air = false;
            if (tk->species == VISITOR_FROG) {
                tk->hop_pause = (tk->state == TURKEY_SCURRY) ? 0.06f : tkrand(0.24f, 0.42f);
            }
        }
    }

    // Relax tail puff / frog throat sac
    if (tk->tail_flare > 0.0f) {
        float decay = (tk->species == VISITOR_FROG) ? 2.2f : 0.9f;
        if (tk->species == VISITOR_FROG || tk->state != TURKEY_PAUSE_GOBBLE) {
            tk->tail_flare -= dt * decay;
            if (tk->tail_flare < 0.0f) tk->tail_flare = 0.0f;
        }
    }

    // Hearth slab spans x = [-1.74 .. +1.74]; step down cleanly onto the floor outside it
    float ground_y = (fabsf(tk->x) <= 1.72f) ? 0.18f : 0.0f;
    tk->y += (ground_y - tk->y) * fminf(1.0f, dt * 14.0f);

    float target_yaw = (tk->dir > 0.0f) ? 90.0f : -90.0f;
    float target_head_turn = 0.0f;

    if (tk->species == VISITOR_FROG) {
        // ---------------------------------------------------------
        // FROG BOUNCY HOPPING AI
        // ---------------------------------------------------------
        switch (tk->state) {
            case TURKEY_WALK_IN: {
                if (in_air) {
                    tk->x += tk->dir * 0.95f * dt;
                } else {
                    tk->hop_pause -= dt;
                    if (tk->hop_pause <= 0.0f) {
                        // Reached cozy warm spot in front of flames?
                        if ((tk->dir > 0.0f && tk->x >= tk->pause_x) ||
                            (tk->dir < 0.0f && tk->x <= tk->pause_x)) {
                            tk->state       = TURKEY_PAUSE_GOBBLE;
                            tk->state_timer = tkrand(2.8f, 4.0f);
                            tk->tail_flare  = 1.0f;
                            tk->just_ribbit = true;
                        } else {
                            // Boing! Next hop forward!
                            tk->hop_vy = tkrand(1.75f, 2.15f);
                        }
                    }
                }
                tk->walk_phase += dt * 4.5f;
                break;
            }

            case TURKEY_PAUSE_GOBBLE: {
                tk->state_timer -= dt;
                // Turn proudly toward the viewer and ribbit!
                target_yaw = (tk->dir > 0.0f) ? 32.0f : -32.0f;
                tk->walk_phase += dt * 3.8f;

                // Second ribbit halfway through pause
                if (tk->state_timer < 1.55f && tk->state_timer + dt >= 1.55f) {
                    tk->tail_flare  = 1.0f;
                    tk->just_ribbit = true;
                }
                if (tk->state_timer <= 0.0f) {
                    tk->state     = TURKEY_WALK_OUT;
                    tk->hop_pause = 0.10f;
                }
                break;
            }

            case TURKEY_WALK_OUT: {
                if (in_air) {
                    tk->x += tk->dir * 1.00f * dt;
                } else {
                    tk->hop_pause -= dt;
                    if (tk->hop_pause <= 0.0f) {
                        tk->hop_vy = tkrand(1.80f, 2.20f);
                    }
                }
                tk->walk_phase += dt * 4.5f;
                if (fabsf(tk->x) > 2.65f && !in_air) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(22.0f, 40.0f);
                }
                break;
            }

            case TURKEY_SCURRY: {
                if (in_air) {
                    tk->x += tk->dir * 1.75f * dt;
                } else {
                    tk->hop_pause -= dt;
                    if (tk->hop_pause <= 0.0f) {
                        tk->hop_vy = 2.35f;
                    }
                }
                tk->walk_phase += dt * 10.0f;
                if (fabsf(tk->x) > 2.65f) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(20.0f, 36.0f);
                }
                break;
            }

            default:
                break;
        }
    } else if (tk->species == VISITOR_CAT) {
        // ---------------------------------------------------------
        // COZY ORANGE TABBY CAT AI
        // ---------------------------------------------------------
        switch (tk->state) {
            case TURKEY_WALK_IN: {
                tk->x += tk->dir * 0.64f * dt;
                tk->walk_phase += dt * 8.4f;
                if ((tk->dir > 0.0f && tk->x >= tk->pause_x) ||
                    (tk->dir < 0.0f && tk->x <= tk->pause_x)) {
                    tk->state       = TURKEY_PAUSE_GOBBLE;
                    tk->state_timer = tkrand(3.4f, 4.8f);
                    tk->just_meowed = true;
                }
                break;
            }

            case TURKEY_PAUSE_GOBBLE: {
                tk->state_timer -= dt;
                // Sit gracefully facing slightly toward the cozy room!
                target_yaw       = (tk->dir > 0.0f) ? 45.0f : -45.0f;
                target_head_turn = (tk->dir > 0.0f) ? -32.0f : 32.0f;
                tk->walk_phase  += dt * 2.8f; // relaxed tail swish!

                if (tk->state_timer < 1.5f && tk->state_timer + dt >= 1.5f && tkrand01() < 0.45f) {
                    tk->just_meowed = true;
                }
                if (tk->state_timer <= 0.0f) {
                    tk->state = TURKEY_WALK_OUT;
                }
                break;
            }

            case TURKEY_WALK_OUT: {
                tk->x += tk->dir * 0.68f * dt;
                tk->walk_phase += dt * 8.8f;
                if (fabsf(tk->x) > 2.65f) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(22.0f, 40.0f);
                }
                break;
            }

            case TURKEY_SCURRY: {
                tk->x += tk->dir * 1.65f * dt;
                tk->walk_phase += dt * 16.0f;
                if (fabsf(tk->x) > 2.65f) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(20.0f, 36.0f);
                }
                break;
            }

            default:
                break;
        }
    } else if (tk->species == VISITOR_PENGUIN) {
        // ---------------------------------------------------------
        // SM64-STYLE WADDLING PENGUIN AI
        // ---------------------------------------------------------
        switch (tk->state) {
            case TURKEY_WALK_IN: {
                tk->x += tk->dir * 0.60f * dt;
                tk->walk_phase += dt * 8.2f;
                if ((tk->dir > 0.0f && tk->x >= tk->pause_x) ||
                    (tk->dir < 0.0f && tk->x <= tk->pause_x)) {
                    tk->state          = TURKEY_PAUSE_GOBBLE;
                    tk->state_timer    = tkrand(2.8f, 4.0f);
                    tk->tail_flare     = 1.0f;
                    tk->just_penguined = true;
                }
                break;
            }

            case TURKEY_PAUSE_GOBBLE: {
                tk->state_timer -= dt;
                target_yaw       = (tk->dir > 0.0f) ? 28.0f : -28.0f;
                tk->tail_flare  += (1.0f - tk->tail_flare) * fminf(1.0f, dt * 5.0f);
                tk->walk_phase  += dt * 6.5f;

                if (tk->state_timer < 1.55f && tk->state_timer + dt >= 1.55f) {
                    tk->tail_flare     = 1.0f;
                    tk->just_penguined = true;
                }
                if (tk->state_timer <= 0.0f) {
                    tk->state = TURKEY_WALK_OUT;
                }
                break;
            }

            case TURKEY_WALK_OUT: {
                tk->x += tk->dir * 0.64f * dt;
                tk->walk_phase += dt * 8.5f;
                if (fabsf(tk->x) > 2.65f) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(22.0f, 40.0f);
                }
                break;
            }

            case TURKEY_SCURRY: {
                tk->x += tk->dir * 1.55f * dt;
                tk->walk_phase += dt * 16.0f;
                if (!in_air) {
                    tk->hop_vy = 1.25f;
                }
                if (fabsf(tk->x) > 2.65f) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(20.0f, 36.0f);
                }
                break;
            }

            default:
                break;
        }
    } else {
        // ---------------------------------------------------------
        // TURKEY STRUTTING AI
        // ---------------------------------------------------------
        switch (tk->state) {
            case TURKEY_WALK_IN: {
                tk->x += tk->dir * 0.56f * dt;
                tk->walk_phase += dt * 7.8f;
                if ((tk->dir > 0.0f && tk->x >= tk->pause_x) ||
                    (tk->dir < 0.0f && tk->x <= tk->pause_x)) {
                    tk->state        = TURKEY_PAUSE_GOBBLE;
                    tk->state_timer  = tkrand(2.6f, 3.8f);
                    tk->just_gobbled = true;
                }
                break;
            }

            case TURKEY_PAUSE_GOBBLE: {
                tk->state_timer -= dt;
                target_yaw       = (tk->dir > 0.0f) ? 55.0f : -55.0f;
                target_head_turn = (tk->dir > 0.0f) ? -35.0f : 35.0f;
                tk->tail_flare  += (1.0f - tk->tail_flare) * fminf(1.0f, dt * 5.0f);
                tk->walk_phase  += dt * 2.2f;

                if (tk->state_timer < 1.15f && tk->state_timer + dt >= 1.15f && tkrand01() < 0.45f) {
                    tk->just_gobbled = true;
                }
                if (tk->state_timer <= 0.0f) {
                    tk->state = TURKEY_WALK_OUT;
                }
                break;
            }

            case TURKEY_WALK_OUT: {
                tk->x += tk->dir * 0.60f * dt;
                tk->walk_phase += dt * 8.2f;
                if (fabsf(tk->x) > 2.65f) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(24.0f, 44.0f);
                }
                break;
            }

            case TURKEY_SCURRY: {
                tk->x += tk->dir * 1.45f * dt;
                tk->walk_phase += dt * 18.0f;
                if (fabsf(tk->x) > 2.65f) {
                    tk->state       = TURKEY_HIDDEN;
                    tk->state_timer = tkrand(22.0f, 38.0f);
                }
                break;
            }

            default:
                break;
        }
    }

    tk->yaw       += (target_yaw - tk->yaw) * fminf(1.0f, dt * 6.5f);
    tk->head_turn += (target_head_turn - tk->head_turn) * fminf(1.0f, dt * 7.0f);
}

// Mesh-builder helpers (emit vertices directly into T3DMeshBuilder)
static void emit_cbox(T3DMeshBuilder *b,
                      float x0, float y0, float z0,
                      float x1, float y1, float z1) {
    t3d_mb_set_normal(b, 0, 0, 1);
    t3d_mb_push_quad(b, x0, y0, z1,  x1, y0, z1,  x1, y1, z1,  x0, y1, z1);
    t3d_mb_set_normal(b, 0, 0, -1);
    t3d_mb_push_quad(b, x1, y0, z0,  x0, y0, z0,  x0, y1, z0,  x1, y1, z0);
    t3d_mb_set_normal(b, -1, 0, 0);
    t3d_mb_push_quad(b, x0, y0, z0,  x0, y0, z1,  x0, y1, z1,  x0, y1, z0);
    t3d_mb_set_normal(b, 1, 0, 0);
    t3d_mb_push_quad(b, x1, y0, z1,  x1, y0, z0,  x1, y1, z0,  x1, y1, z1);
    t3d_mb_set_normal(b, 0, 1, 0);
    t3d_mb_push_quad(b, x0, y1, z1,  x1, y1, z1,  x1, y1, z0,  x0, y1, z0);
    t3d_mb_set_normal(b, 0, -1, 0);
    t3d_mb_push_quad(b, x0, y0, z0,  x1, y0, z0,  x1, y0, z1,  x0, y0, z1);
}

static void rotate_zx(float lx, float ly, float lz,
                      float cz, float sz, float cx, float sx,
                      float *ox, float *oy, float *oz) {
    float x1 = lx * cz - ly * sz;
    float y1 = lx * sz + ly * cz;
    *ox = x1;
    *oy = y1 * cx - lz * sx;
    *oz = y1 * sx + lz * cx;
}

static void emit_cbox_xform(T3DMeshBuilder *b,
                            float x0, float y0, float z0,
                            float x1, float y1, float z1,
                            float deg_z, float deg_x,
                            float tx, float ty, float tz,
                            float cr, float cg, float cb) {
    const float rad = 0.0174532925f;
    float cz = cosf(deg_z * rad), sz = sinf(deg_z * rad);
    float cx = cosf(deg_x * rad), sx = sinf(deg_x * rad);

    static const float nrm[6][3] = {
        { 0, 0, 1}, { 0, 0,-1}, {-1, 0, 0}, { 1, 0, 0}, { 0, 1, 0}, { 0,-1, 0}
    };
    float pts[6][4][3] = {
        {{x0,y0,z1}, {x1,y0,z1}, {x1,y1,z1}, {x0,y1,z1}},
        {{x1,y0,z0}, {x0,y0,z0}, {x0,y1,z0}, {x1,y1,z0}},
        {{x0,y0,z0}, {x0,y0,z1}, {x0,y1,z1}, {x0,y1,z0}},
        {{x1,y0,z1}, {x1,y0,z0}, {x1,y1,z0}, {x1,y1,z1}},
        {{x0,y1,z1}, {x1,y1,z1}, {x1,y1,z0}, {x0,y1,z0}},
        {{x0,y0,z0}, {x1,y0,z0}, {x1,y0,z1}, {x0,y0,z1}},
    };

    for (int f = 0; f < 6; f++) {
        float wnx, wny, wnz;
        rotate_zx(nrm[f][0], nrm[f][1], nrm[f][2], cz, sz, cx, sx, &wnx, &wny, &wnz);
        T3DBuildVert v[4];
        for (int k = 0; k < 4; k++) {
            float wx, wy, wz;
            rotate_zx(pts[f][k][0], pts[f][k][1], pts[f][k][2], cz, sz, cx, sx, &wx, &wy, &wz);
            v[k] = (T3DBuildVert){ wx + tx, wy + ty, wz + tz, wnx, wny, wnz, 0.0f, 0.0f, cr, cg, cb, 1.0f };
        }
        t3d_mb_push_quad_vc(b, &v[0], &v[1], &v[2], &v[3]);
    }
}

static void emit_gem_tris_xform(T3DMeshBuilder *b,
                                float ox, float oy, float oz,
                                float rx, float ry, float rz,
                                float deg_z, float deg_x,
                                float tx, float ty, float tz,
                                float cr, float cg, float cb) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };
    const float rad = 0.0174532925f;
    float cz = cosf(deg_z * rad), sz = sinf(deg_z * rad);
    float cx = cosf(deg_x * rad), sx = sinf(deg_x * rad);

    for (int i = 0; i < 6; i++) {
        float p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z;
        float n0x, n0y, n0z, n1x, n1y, n1z, n2x, n2y, n2z;
        rotate_zx(ox, oy + ry, oz, cz, sz, cx, sx, &p0x, &p0y, &p0z);
        rotate_zx(ox + cs[i] * rx, oy, oz + sn[i] * rz, cz, sz, cx, sx, &p1x, &p1y, &p1z);
        rotate_zx(ox + cs[i + 1] * rx, oy, oz + sn[i + 1] * rz, cz, sz, cx, sx, &p2x, &p2y, &p2z);
        rotate_zx(0.0f, 1.0f, 0.0f, cz, sz, cx, sx, &n0x, &n0y, &n0z);
        rotate_zx(cs[i], 0.35f, sn[i], cz, sz, cx, sx, &n1x, &n1y, &n1z);
        rotate_zx(cs[i + 1], 0.35f, sn[i + 1], cz, sz, cx, sx, &n2x, &n2y, &n2z);
        T3DBuildVert v0 = { p0x + tx, p0y + ty, p0z + tz, n0x, n0y, n0z, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v1 = { p1x + tx, p1y + ty, p1z + tz, n1x, n1y, n1z, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v2 = { p2x + tx, p2y + ty, p2z + tz, n2x, n2y, n2z, 0, 0, cr, cg, cb, 1.0f };
        t3d_mb_push_tri_vc(b, &v0, &v1, &v2);
    }
    for (int i = 0; i < 6; i++) {
        float p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z;
        float n0x, n0y, n0z, n1x, n1y, n1z, n2x, n2y, n2z;
        rotate_zx(ox, oy - ry, oz, cz, sz, cx, sx, &p0x, &p0y, &p0z);
        rotate_zx(ox + cs[i + 1] * rx, oy, oz + sn[i + 1] * rz, cz, sz, cx, sx, &p1x, &p1y, &p1z);
        rotate_zx(ox + cs[i] * rx, oy, oz + sn[i] * rz, cz, sz, cx, sx, &p2x, &p2y, &p2z);
        rotate_zx(0.0f, -1.0f, 0.0f, cz, sz, cx, sx, &n0x, &n0y, &n0z);
        rotate_zx(cs[i + 1], -0.35f, sn[i + 1], cz, sz, cx, sx, &n1x, &n1y, &n1z);
        rotate_zx(cs[i], -0.35f, sn[i], cz, sz, cx, sx, &n2x, &n2y, &n2z);
        T3DBuildVert v0 = { p0x + tx, p0y + ty, p0z + tz, n0x, n0y, n0z, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v1 = { p1x + tx, p1y + ty, p1z + tz, n1x, n1y, n1z, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v2 = { p2x + tx, p2y + ty, p2z + tz, n2x, n2y, n2z, 0, 0, cr, cg, cb, 1.0f };
        t3d_mb_push_tri_vc(b, &v0, &v1, &v2);
    }
}

static void emit_gem_tris_at(T3DMeshBuilder *b,
                             float ox, float oy, float oz,
                             float rx, float ry, float rz,
                             float cr, float cg, float cb) {
    emit_gem_tris_xform(b, ox, oy, oz, rx, ry, rz, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, cr, cg, cb);
}

static void draw_dyn_gem(float rx, float ry, float rz, float cr, float cg, float cb) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };

    t3d_dyn_begin_tris(NULL, T3D_PROP_SCALE, false, true, true, true);
    for (int i = 0; i < 6; i++) {
        T3DBuildVert v0 = { 0.0f, ry, 0.0f,  0.0f, 1.0f, 0.0f,  0, 0,  cr, cg, cb, 1.0f };
        T3DBuildVert v1 = { cs[i] * rx, 0.0f, sn[i] * rz,  cs[i], 0.35f, sn[i],  0, 0,  cr, cg, cb, 1.0f };
        T3DBuildVert v2 = { cs[i + 1] * rx, 0.0f, sn[i + 1] * rz,  cs[i + 1], 0.35f, sn[i + 1],  0, 0,  cr, cg, cb, 1.0f };
        t3d_dyn_tri_vc(&v0, &v1, &v2);
    }
    for (int i = 0; i < 6; i++) {
        T3DBuildVert v0 = { 0.0f, -ry, 0.0f,  0.0f, -1.0f, 0.0f,  0, 0,  cr, cg, cb, 1.0f };
        T3DBuildVert v1 = { cs[i + 1] * rx, 0.0f, sn[i + 1] * rz,  cs[i + 1], -0.35f, sn[i + 1],  0, 0,  cr, cg, cb, 1.0f };
        T3DBuildVert v2 = { cs[i] * rx, 0.0f, sn[i] * rz,  cs[i], -0.35f, sn[i],  0, 0,  cr, cg, cb, 1.0f };
        t3d_dyn_tri_vc(&v0, &v1, &v2);
    }
    t3d_dyn_end_tris();
}

static void emit_ellipsoid_at(T3DMeshBuilder *b,
                              float ox, float oy, float oz,
                              float rx, float ry, float rz,
                              float cr, float cg, float cb) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };
    static const float lat_y[4] = { -1.0f, -0.45f, 0.45f, 1.0f };
    static const float lat_r[4] = {  0.0f,  0.89f, 0.89f, 0.0f };

    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        T3DBuildVert v0 = { ox + c0 * lat_r[1] * rx, oy + lat_y[1] * ry, oz + s0 * lat_r[1] * rz, c0 * 0.89f, -0.45f, s0 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v1 = { ox + c1 * lat_r[1] * rx, oy + lat_y[1] * ry, oz + s1 * lat_r[1] * rz, c1 * 0.89f, -0.45f, s1 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v2 = { ox + c1 * lat_r[2] * rx, oy + lat_y[2] * ry, oz + s1 * lat_r[2] * rz, c1 * 0.89f,  0.45f, s1 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v3 = { ox + c0 * lat_r[2] * rx, oy + lat_y[2] * ry, oz + s0 * lat_r[2] * rz, c0 * 0.89f,  0.45f, s0 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        t3d_mb_push_quad_vc(b, &v0, &v1, &v2, &v3);
    }

    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        T3DBuildVert v0 = { ox, oy + ry, oz, 0.0f, 1.0f, 0.0f, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v1 = { ox + c0 * lat_r[2] * rx, oy + lat_y[2] * ry, oz + s0 * lat_r[2] * rz, c0 * 0.89f, 0.45f, s0 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v2 = { ox + c1 * lat_r[2] * rx, oy + lat_y[2] * ry, oz + s1 * lat_r[2] * rz, c1 * 0.89f, 0.45f, s1 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        t3d_mb_push_tri_vc(b, &v0, &v1, &v2);
    }
    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        T3DBuildVert v0 = { ox, oy - ry, oz, 0.0f, -1.0f, 0.0f, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v1 = { ox + c1 * lat_r[1] * rx, oy + lat_y[1] * ry, oz + s1 * lat_r[1] * rz, c1 * 0.89f, -0.45f, s1 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        T3DBuildVert v2 = { ox + c0 * lat_r[1] * rx, oy + lat_y[1] * ry, oz + s0 * lat_r[1] * rz, c0 * 0.89f, -0.45f, s0 * 0.89f, 0, 0, cr, cg, cb, 1.0f };
        t3d_mb_push_tri_vc(b, &v0, &v1, &v2);
    }
}

static void emit_santa_hat(T3DMeshBuilder *b, float rx, float h) {
    static const float hcs[7] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f, 1.0f };
    static const float hsn[7] = { 0.0f,-0.866f,-0.866f, 0.0f, 0.866f, 0.866f, 0.0f };
    float tip_z = -rx * 0.35f;

    emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, rx * 1.08f, rx * 0.28f, rx * 1.08f, 0.96f, 0.96f, 0.98f);

    t3d_mb_set_color(b, 0.88f, 0.10f, 0.14f, 1.0f);
    for (int i = 0; i < 6; i++) {
        t3d_mb_set_normal(b, hcs[i], 0.45f, hsn[i]);
        t3d_mb_push_tri(b,
                        0.0f, h, tip_z,
                        hcs[i] * rx * 0.92f, rx * 0.14f, hsn[i] * rx * 0.92f,
                        hcs[i + 1] * rx * 0.92f, rx * 0.14f, hsn[i + 1] * rx * 0.92f);
    }

    emit_gem_tris_at(b, 0.0f, h, tip_z, rx * 0.32f, rx * 0.32f, rx * 0.32f, 0.98f, 0.98f, 0.98f);
}

// ---------------------------------------------------------
// Pre-compiled Tiny3D Meshes for Visitors, Hats & Easter Eggs
// ---------------------------------------------------------
static bool s_lists_ready = false;
static T3DMesh m_santa_hat_frog, m_wizard_hat_frog, m_santa_hat_cat, m_spring_flower_cat, m_summer_shades_cat;
static T3DMesh m_frog_body_head, m_frog_arm, m_frog_wand_arm, m_frog_wand_star;
static T3DMesh m_frog_leg_seated, m_frog_leg_air;
static T3DMesh m_turkey_leg, m_turkey_body, m_turkey_wing, m_turkey_tail, m_turkey_head;
static T3DMesh m_roasted_turkey;
static T3DMesh m_cat_nap, m_cat_front_leg, m_cat_back_leg, m_cat_haunch;
static T3DMesh m_cat_torso, m_cat_tail_lower, m_cat_tail_upper, m_cat_head, m_cat_ears;
static T3DMesh m_penguin_foot, m_penguin_body, m_penguin_flipper, m_penguin_scarf_tail, m_penguin_head;
static T3DMesh m_mini_snowman;

void turkey_init_lists(void) {
    if (s_lists_ready) return;
    s_lists_ready = true;

    T3DMeshBuilder *b;

    // 1. Shared Hats
    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_santa_hat(b, 0.115f, 0.21f);
    m_santa_hat_frog = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    {
        static const float hcs[7] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f, 1.0f };
        static const float hsn[7] = { 0.0f,-0.866f,-0.866f, 0.0f, 0.866f, 0.866f, 0.0f };
        emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, 0.125f, 0.018f, 0.125f, 0.16f, 0.24f, 0.82f);
        emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, 0.078f, 0.024f, 0.078f, 1.00f, 0.84f, 0.18f);
        t3d_mb_set_color(b, 0.18f, 0.28f, 0.88f, 1.0f);
        for (int i = 0; i < 6; i++) {
            t3d_mb_set_normal(b, hcs[i], 0.4f, hsn[i]);
            t3d_mb_push_tri(b,
                            0.0f, 0.21f, -0.02f,
                            hcs[i] * 0.072f, 0.01f, hsn[i] * 0.072f,
                            hcs[i + 1] * 0.072f, 0.01f, hsn[i + 1] * 0.072f);
        }
    }
    m_wizard_hat_frog = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_santa_hat(b, 0.058f, 0.130f);
    m_santa_hat_cat = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    {
        float fx = 0.062f, fy = 0.070f, fz = 0.022f;
        t3d_mb_set_color(b, 0.98f, 0.64f, 0.80f, 1.0f);
        for (int i = 0; i < 5; i++) {
            float a = (float)i * 1.256637f;
            float px = fx + cosf(a) * 0.020f;
            float py = fy + sinf(a) * 0.020f;
            emit_cbox(b, px - 0.011f, py - 0.011f, fz - 0.008f, px + 0.011f, py + 0.011f, fz + 0.010f);
        }
        t3d_mb_set_color(b, 1.00f, 0.86f, 0.24f, 1.0f);
        emit_cbox(b, fx - 0.010f, fy - 0.010f, fz - 0.006f, fx + 0.010f, fy + 0.010f, fz + 0.014f);
    }
    m_spring_flower_cat = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    {
        t3d_mb_set_color(b, 0.08f, 0.09f, 0.14f, 1.0f);
        emit_cbox(b, -0.066f, -0.004f, 0.086f, -0.018f, 0.032f, 0.098f);
        emit_cbox(b,  0.018f, -0.004f, 0.086f,  0.066f, 0.032f, 0.098f);
        emit_cbox(b, -0.020f,  0.018f, 0.088f,  0.020f, 0.028f, 0.098f);
        emit_cbox(b, -0.072f,  0.016f, 0.020f, -0.062f, 0.026f, 0.090f);
        emit_cbox(b,  0.062f,  0.016f, 0.020f,  0.072f, 0.026f, 0.090f);
        t3d_mb_set_color(b, 0.72f, 0.84f, 0.96f, 1.0f);
        emit_cbox(b, -0.056f,  0.016f, 0.096f, -0.042f, 0.026f, 0.100f);
        emit_cbox(b,  0.028f,  0.016f, 0.096f,  0.042f, 0.026f, 0.100f);
    }
    m_summer_shades_cat = t3d_mb_end(b);

    // 2. Frog parts
    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f, 0.0f, 0.0f, 0.148f, 0.105f, 0.175f, 0.24f, 0.76f, 0.24f);
    emit_gem_tris_at(b, 0.0f, -0.044f, 0.032f, 0.118f, 0.065f, 0.132f, 0.88f, 0.90f, 0.56f);
    emit_gem_tris_at(b, 0.0f,  0.025f, 0.132f, 0.122f, 0.070f, 0.100f, 0.26f, 0.80f, 0.26f);
    t3d_mb_set_color(b, 0.12f, 0.44f, 0.14f, 1.0f);
    emit_cbox(b, -0.085f, 0.078f, -0.065f, -0.025f, 0.104f,  0.005f);
    emit_cbox(b,  0.030f, 0.076f, -0.085f,  0.085f, 0.100f, -0.020f);
    t3d_mb_set_color(b, 0.10f, 0.30f, 0.10f, 1.0f);
    emit_cbox(b, -0.092f, 0.011f,  0.200f,  0.092f, 0.023f,  0.230f);
    for (int side = -1; side <= 1; side += 2) {
        float ex = side * 0.068f;
        t3d_mb_set_color(b, 0.24f, 0.78f, 0.24f, 1.0f);
        emit_cbox(b, ex - 0.042f, 0.061f, 0.102f,  ex + 0.042f, 0.139f, 0.178f);
        t3d_mb_set_color(b, 0.98f, 0.96f, 0.74f, 1.0f);
        emit_cbox(b, ex - 0.032f, 0.071f, 0.170f,  ex + 0.032f, 0.129f, 0.194f);
        t3d_mb_set_color(b, 0.05f, 0.05f, 0.07f, 1.0f);
        emit_cbox(b, ex - 0.015f, 0.085f, 0.190f,  ex + 0.015f, 0.115f, 0.204f);
    }
    m_frog_body_head = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.22f, 0.72f, 0.22f, 1.0f);
    emit_cbox(b, -0.020f, -0.105f, -0.020f,  0.020f,  0.010f, 0.020f);
    t3d_mb_set_color(b, 0.32f, 0.82f, 0.26f, 1.0f);
    emit_cbox(b, -0.034f, -0.112f, -0.016f,  0.034f, -0.092f, 0.046f);
    m_frog_arm = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.22f, 0.72f, 0.22f, 1.0f);
    emit_cbox(b, -0.020f, -0.105f, -0.020f,  0.020f,  0.010f, 0.020f);
    t3d_mb_set_color(b, 0.32f, 0.82f, 0.26f, 1.0f);
    emit_cbox(b, -0.030f, -0.112f, -0.016f,  0.030f, -0.092f, 0.040f);
    t3d_mb_set_color(b, 0.42f, 0.24f, 0.10f, 1.0f);
    emit_cbox(b, -0.012f, -0.340f, -0.012f,  0.012f, -0.095f, 0.012f);
    m_frog_wand_arm = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_lit(b, false);
    emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, 0.056f, 0.056f, 0.056f, 1.00f, 0.94f, 0.36f);
    m_frog_wand_star = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, 0.058f, 0.068f, 0.098f, 0.20f, 0.68f, 0.20f);
    t3d_mb_set_color(b, 0.34f, 0.84f, 0.26f, 1.0f);
    emit_cbox(b, -0.048f, -0.082f, -0.050f,  0.048f, -0.058f, 0.075f);
    m_frog_leg_seated = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, 0.048f, 0.048f, 0.095f, 0.20f, 0.68f, 0.20f);
    t3d_mb_set_color(b, 0.20f, 0.68f, 0.20f, 1.0f);
    emit_cbox(b, -0.022f, -0.052f, -0.220f,  0.022f, -0.008f, -0.085f);
    t3d_mb_set_color(b, 0.34f, 0.84f, 0.26f, 1.0f);
    emit_cbox(b, -0.048f, -0.044f, -0.280f,  0.048f, -0.016f, -0.205f);
    m_frog_leg_air = t3d_mb_end(b);

    // 3. Turkey parts
    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.96f, 0.60f, 0.14f, 1.0f);
    emit_cbox(b, -0.018f, -0.175f, -0.018f,  0.018f,  0.000f, 0.018f);
    emit_cbox(b, -0.038f, -0.180f, -0.025f,  0.038f, -0.158f, 0.070f);
    m_turkey_leg = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f, 0.0f, 0.0f, 0.165f, 0.150f, 0.200f, 0.64f, 0.34f, 0.15f);
    emit_gem_tris_at(b, 0.0f, -0.01f, 0.125f, 0.112f, 0.098f, 0.098f, 0.78f, 0.46f, 0.20f);
    m_turkey_body = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.48f, 0.24f, 0.10f, 1.0f);
    emit_cbox(b, -0.028f, -0.098f, -0.125f,  0.028f, 0.045f, 0.098f);
    m_turkey_wing = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_cull(b, false);
    t3d_mb_set_normal(b, 0.0f, 0.25f, 0.96f);
    {
        const float deg2rad = 0.0174532925f;
        for (int f = -3; f <= 3; f++) {
            float ang = (float)f * 21.0f * deg2rad;
            float c = cosf(ang), s = sinf(ang);
            float r, g, b_c;
            int m = (f + 3) % 3;
            if (m == 0)      { r = 0.72f; g = 0.30f; b_c = 0.09f; }
            else if (m == 1) { r = 0.94f; g = 0.48f; b_c = 0.10f; }
            else             { r = 0.86f; g = 0.64f; b_c = 0.16f; }
            float fh = (0.34f - fabsf((float)f) * 0.018f) * 1.08f;
            float fw = 0.058f * 1.08f;
            float fz = -0.005f * fabsf((float)f);
            float y1 = fh * 0.80f;

            t3d_mb_set_color(b, r, g, b_c, 1.0f);
            t3d_mb_push_quad(b,
                             (-fw * 0.42f) * c, (-fw * 0.42f) * s, fz,
                             ( fw * 0.42f) * c, ( fw * 0.42f) * s, fz,
                              fw * c - y1 * s,   fw * s + y1 * c,  fz,
                             -fw * c - y1 * s,  -fw * s + y1 * c,  fz);

            t3d_mb_set_color(b, 0.98f, 0.90f, 0.68f, 1.0f);
            t3d_mb_push_tri(b,
                            -fw * c - y1 * s, -fw * s + y1 * c, fz,
                             fw * c - y1 * s,  fw * s + y1 * c, fz,
                            -fh * s,           fh * c,          fz);
        }
    }
    m_turkey_tail = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_gem_tris_at(b, 0.0f, 0.175f, 0.015f, 0.064f, 0.060f, 0.070f, 0.90f, 0.26f, 0.22f);
    t3d_mb_set_color(b, 1.00f, 0.82f, 0.18f, 1.0f);
    t3d_mb_set_normal(b,  0.0f,  0.6f, 0.8f);
    t3d_mb_push_tri(b, -0.026f, 0.187f, 0.073f,  0.026f, 0.187f, 0.073f, 0.0f, 0.167f, 0.150f);
    t3d_mb_set_normal(b,  0.0f, -0.6f, 0.8f);
    t3d_mb_push_tri(b,  0.026f, 0.155f, 0.073f, -0.026f, 0.155f, 0.073f, 0.0f, 0.167f, 0.150f);
    t3d_mb_set_normal(b, -0.8f,  0.0f, 0.6f);
    t3d_mb_push_tri(b, -0.026f, 0.155f, 0.073f, -0.026f, 0.187f, 0.073f, 0.0f, 0.167f, 0.150f);
    t3d_mb_set_normal(b,  0.8f,  0.0f, 0.6f);
    t3d_mb_push_tri(b,  0.026f, 0.187f, 0.073f,  0.026f, 0.155f, 0.073f, 0.0f, 0.167f, 0.150f);

    t3d_mb_set_color(b, 0.84f, 0.28f, 0.20f, 1.0f);
    emit_cbox(b, -0.038f, -0.025f, -0.036f,  0.038f, 0.145f, 0.036f);
    t3d_mb_set_color(b, 1.00f, 0.14f, 0.14f, 1.0f);
    emit_cbox(b, -0.020f,  0.075f,  0.045f,  0.020f, 0.160f, 0.087f);
    emit_cbox(b, -0.014f,  0.147f,  0.083f,  0.014f, 0.197f, 0.113f);
    t3d_mb_set_color(b, 0.06f, 0.05f, 0.05f, 1.0f);
    emit_cbox(b, -0.066f,  0.181f,  0.033f, -0.052f, 0.203f, 0.057f);
    emit_cbox(b,  0.052f,  0.181f,  0.033f,  0.066f, 0.203f, 0.057f);
    m_turkey_head = t3d_mb_end(b);

    // 4. Roasted Turkey Easter Egg
    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f, 0.155f, 0.0f, 0.195f, 0.135f, 0.155f, 0.86f, 0.44f, 0.12f);
    emit_gem_tris_at(b,  0.0f,  0.022f, 0.0f, 0.32f,  0.024f, 0.25f,  0.84f, 0.86f, 0.90f);
    emit_gem_tris_at(b, -0.02f, 0.220f, 0.0f, 0.145f, 0.075f, 0.115f, 0.95f, 0.56f, 0.16f);
    t3d_mb_set_color(b, 0.95f, 0.96f, 0.98f, 1.0f);
    emit_cbox(b, -0.22f, 0.022f, -0.16f,  0.22f, 0.036f, 0.16f);
    t3d_mb_set_color(b, 0.18f, 0.68f, 0.22f, 1.0f);
    emit_cbox(b, -0.25f, 0.034f,  0.07f, -0.18f, 0.068f,  0.14f);
    emit_cbox(b,  0.18f, 0.034f,  0.06f,  0.25f, 0.068f,  0.13f);
    emit_cbox(b, -0.24f, 0.034f, -0.13f, -0.17f, 0.068f, -0.06f);
    emit_cbox(b,  0.17f, 0.034f, -0.13f,  0.24f, 0.068f, -0.06f);
    t3d_mb_set_color(b, 0.88f, 0.12f, 0.16f, 1.0f);
    emit_cbox(b, -0.22f, 0.044f,  0.02f, -0.18f, 0.078f,  0.06f);
    emit_cbox(b,  0.19f, 0.044f, -0.04f,  0.23f, 0.078f,  0.00f);
    t3d_mb_set_color(b, 0.76f, 0.36f, 0.10f, 1.0f);
    emit_cbox(b, -0.14f, 0.100f,  0.125f,  0.08f, 0.185f,  0.175f);
    emit_cbox(b, -0.14f, 0.100f, -0.175f,  0.08f, 0.185f, -0.125f);
    for (int side = -1; side <= 1; side += 2) {
        float tz = (float)side * 0.105f;
        float dx = (float)side * -24.0f;
        emit_gem_tris_xform(b, 0.0f, 0.0f, 0.0f, 0.062f, 0.095f, 0.062f,
                            -34.0f, dx, 0.105f, 0.170f, tz, 0.82f, 0.40f, 0.11f);
        emit_gem_tris_xform(b, 0.0f, 0.175f, 0.0f, 0.052f, 0.044f, 0.052f,
                            -34.0f, dx, 0.105f, 0.170f, tz, 0.98f, 0.98f, 0.98f);
        emit_cbox_xform(b, -0.018f, 0.070f, -0.018f, 0.018f, 0.165f, 0.018f,
                        -34.0f, dx, 0.105f, 0.170f, tz, 0.96f, 0.92f, 0.82f);
    }
    m_roasted_turkey = t3d_mb_end(b);

    // 5. Cat parts
    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f, 0.095f, 0.0f, 0.165f, 0.092f, 0.185f, 0.92f, 0.52f, 0.16f);
    t3d_mb_set_color(b, 0.70f, 0.34f, 0.09f, 1.0f);
    emit_cbox(b, -0.115f, 0.160f, -0.090f,  0.115f, 0.183f, -0.055f);
    emit_cbox(b, -0.125f, 0.167f, -0.015f,  0.125f, 0.190f,  0.020f);
    emit_cbox(b, -0.110f, 0.159f,  0.055f,  0.110f, 0.181f,  0.088f);
    t3d_mb_set_color(b, 0.96f, 0.94f, 0.88f, 1.0f);
    emit_cbox(b, -0.085f, 0.003f,  0.130f, -0.025f, 0.039f,  0.195f);
    emit_cbox(b,  0.025f, 0.003f,  0.130f,  0.085f, 0.039f,  0.195f);
    t3d_mb_set_color(b, 0.88f, 0.48f, 0.14f, 1.0f);
    emit_cbox(b,  0.130f, 0.013f, -0.140f,  0.185f, 0.059f,  0.100f);
    t3d_mb_set_color(b, 0.96f, 0.94f, 0.88f, 1.0f);
    emit_cbox(b,  0.110f, 0.013f,  0.090f,  0.175f, 0.059f,  0.155f);

    emit_gem_tris_xform(b, 0.0f, 0.0f, 0.0f, 0.092f, 0.078f, 0.088f,
                        0.0f, 14.0f, 0.0f, 0.115f, 0.165f, 0.94f, 0.55f, 0.18f);
    emit_cbox_xform(b, -0.042f, -0.044f, 0.052f,  0.042f, -0.008f, 0.095f,
                    0.0f, 14.0f, 0.0f, 0.115f, 0.165f, 0.96f, 0.93f, 0.86f);
    emit_cbox_xform(b, -0.015f, -0.014f, 0.088f,  0.015f,  0.006f, 0.104f,
                    0.0f, 14.0f, 0.0f, 0.115f, 0.165f, 0.96f, 0.56f, 0.62f);
    emit_cbox_xform(b, -0.058f,  0.004f, 0.066f, -0.020f,  0.015f, 0.084f,
                    0.0f, 14.0f, 0.0f, 0.115f, 0.165f, 0.18f, 0.10f, 0.06f);
    emit_cbox_xform(b,  0.020f,  0.004f, 0.066f,  0.058f,  0.015f, 0.084f,
                    0.0f, 14.0f, 0.0f, 0.115f, 0.165f, 0.18f, 0.10f, 0.06f);

    t3d_mb_set_cull(b, false);
    {
        const float rad = 0.0174532925f;
        float cx = cosf(14.0f * rad), sx = sinf(14.0f * rad);
        float wnx, wny, wnz;
        rotate_zx(0.0f, 0.4f, 0.9f, 1.0f, 0.0f, cx, sx, &wnx, &wny, &wnz);
        t3d_mb_set_normal(b, wnx, wny, wnz);
        t3d_mb_set_color(b, 0.90f, 0.48f, 0.14f, 1.0f);
        for (int side = -1; side <= 1; side += 2) {
            float ex = side * 0.052f;
            float p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z;
            rotate_zx(ex - 0.028f, 0.052f, 0.005f, 1.0f, 0.0f, cx, sx, &p0x, &p0y, &p0z);
            rotate_zx(ex + 0.028f, 0.052f, 0.005f, 1.0f, 0.0f, cx, sx, &p1x, &p1y, &p1z);
            rotate_zx(ex + side * 0.012f, 0.125f, 0.005f, 1.0f, 0.0f, cx, sx, &p2x, &p2y, &p2z);
            t3d_mb_push_tri(b, p0x, p0y + 0.115f, p0z + 0.165f,
                               p1x, p1y + 0.115f, p1z + 0.165f,
                               p2x, p2y + 0.115f, p2z + 0.165f);
        }
    }
    m_cat_nap = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.90f, 0.50f, 0.16f, 1.0f);
    emit_cbox(b, -0.022f, -0.115f, -0.022f,  0.022f,  0.015f, 0.022f);
    t3d_mb_set_color(b, 0.96f, 0.94f, 0.88f, 1.0f);
    emit_cbox(b, -0.024f, -0.155f, -0.022f,  0.024f, -0.112f, 0.036f);
    m_cat_front_leg = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.88f, 0.48f, 0.14f, 1.0f);
    emit_cbox(b, -0.024f, -0.115f, -0.024f,  0.024f,  0.015f, 0.024f);
    t3d_mb_set_color(b, 0.96f, 0.94f, 0.88f, 1.0f);
    emit_cbox(b, -0.026f, -0.155f, -0.024f,  0.026f, -0.112f, 0.036f);
    m_cat_back_leg = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, 0.052f, 0.072f, 0.082f, 0.88f, 0.48f, 0.14f);
    t3d_mb_set_color(b, 0.96f, 0.94f, 0.88f, 1.0f);
    emit_cbox(b, -0.026f, -0.085f, -0.020f,  0.026f, -0.052f, 0.075f);
    m_cat_haunch = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f, 0.0f, 0.0f, 0.105f, 0.092f, 0.205f, 0.92f, 0.52f, 0.16f);
    emit_gem_tris_at(b, 0.0f, -0.015f, 0.145f, 0.078f, 0.074f, 0.072f, 0.96f, 0.94f, 0.88f);
    t3d_mb_set_color(b, 0.68f, 0.32f, 0.08f, 1.0f);
    emit_cbox(b, -0.078f, 0.060f, -0.105f,  0.078f, 0.088f, -0.070f);
    emit_cbox(b, -0.084f, 0.068f, -0.020f,  0.084f, 0.096f,  0.015f);
    emit_cbox(b, -0.076f, 0.060f,  0.065f,  0.076f, 0.088f,  0.098f);
    m_cat_torso = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.90f, 0.50f, 0.15f, 1.0f);
    emit_cbox(b, -0.020f, 0.0f, -0.020f,  0.020f, 0.150f, 0.020f);
    m_cat_tail_lower = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.90f, 0.50f, 0.15f, 1.0f);
    emit_cbox(b, -0.018f, 0.0f,   -0.018f,  0.018f, 0.105f, 0.018f);
    t3d_mb_set_color(b, 0.96f, 0.94f, 0.88f, 1.0f);
    emit_cbox(b, -0.020f, 0.100f, -0.020f,  0.020f, 0.148f, 0.020f);
    m_cat_tail_upper = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_gem_tris_at(b, 0.0f, 0.0f, 0.0f, 0.094f, 0.084f, 0.090f, 0.94f, 0.55f, 0.18f);
    t3d_mb_set_color(b, 0.96f, 0.94f, 0.88f, 1.0f);
    emit_cbox(b, -0.044f, -0.044f, 0.052f,  0.044f, -0.008f, 0.098f);
    t3d_mb_set_color(b, 0.96f, 0.52f, 0.60f, 1.0f);
    emit_cbox(b, -0.016f, -0.014f, 0.092f,  0.016f,  0.008f, 0.108f);
    for (int side = -1; side <= 1; side += 2) {
        float ex = side * 0.042f;
        t3d_mb_set_color(b, 0.32f, 0.86f, 0.34f, 1.0f);
        emit_cbox(b, ex - 0.022f, -0.002f, 0.068f,  ex + 0.022f, 0.034f, 0.084f);
        t3d_mb_set_color(b, 0.06f, 0.06f, 0.08f, 1.0f);
        emit_cbox(b, ex - 0.009f,  0.002f, 0.080f,  ex + 0.009f, 0.030f, 0.090f);
    }
    m_cat_head = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_cull(b, false);
    t3d_mb_set_normal(b, 0.0f, 0.3f, 0.95f);
    for (int side = -1; side <= 1; side += 2) {
        float ex = side * 0.054f;
        t3d_mb_set_color(b, 0.90f, 0.48f, 0.14f, 1.0f);
        t3d_mb_push_tri(b, ex - 0.032f, 0.056f, 0.002f, ex + 0.032f, 0.056f, 0.002f, ex + side * 0.014f, 0.140f, 0.004f);
        t3d_mb_set_color(b, 0.96f, 0.62f, 0.66f, 1.0f);
        t3d_mb_push_tri(b, ex - 0.017f, 0.064f, 0.012f, ex + 0.017f, 0.064f, 0.012f, ex + side * 0.008f, 0.118f, 0.012f);
    }
    m_cat_ears = t3d_mb_end(b);

    // 6. Penguin parts
    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.96f, 0.56f, 0.12f, 1.0f);
    emit_cbox(b, -0.020f, -0.068f, -0.020f,  0.020f,  0.010f, 0.020f);
    emit_cbox(b, -0.044f, -0.075f, -0.026f,  0.044f, -0.048f, 0.088f);
    m_penguin_foot = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f,  0.0f,   0.0f,   0.152f, 0.172f, 0.142f, 0.15f, 0.22f, 0.42f);
    emit_ellipsoid_at(b, 0.0f, -0.012f, 0.048f, 0.118f, 0.138f, 0.106f, 0.96f, 0.97f, 0.99f);
    emit_gem_tris_at(b, 0.0f, 0.142f, 0.010f, 0.118f, 0.032f, 0.116f, 0.88f, 0.12f, 0.16f);
    t3d_mb_set_color(b, 0.14f, 0.20f, 0.38f, 1.0f);
    emit_cbox(b, -0.042f, -0.105f, -0.165f,  0.042f, -0.045f, -0.095f);
    t3d_mb_set_color(b, 0.96f, 0.96f, 0.98f, 1.0f);
    emit_cbox(b, -0.075f,  0.114f,  0.102f,  0.075f,  0.154f,  0.126f);
    m_penguin_body = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.14f, 0.20f, 0.38f, 1.0f);
    emit_cbox(b, -0.024f, -0.140f, -0.045f,  0.024f, 0.015f, 0.045f);
    m_penguin_flipper = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    t3d_mb_set_color(b, 0.88f, 0.12f, 0.16f, 1.0f);
    emit_cbox(b, -0.026f, -0.135f, -0.018f,  0.026f, 0.010f, 0.018f);
    m_penguin_scarf_tail = t3d_mb_end(b);

    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f, 0.0f, 0.0f, 0.116f, 0.102f, 0.112f, 0.15f, 0.22f, 0.42f);
    t3d_mb_set_color(b, 0.96f, 0.97f, 0.99f, 1.0f);
    emit_cbox(b, -0.088f, -0.062f, 0.056f, -0.018f,  0.020f, 0.104f);
    emit_cbox(b,  0.018f, -0.062f, 0.056f,  0.088f,  0.020f, 0.104f);
    emit_cbox(b, -0.055f, -0.068f, 0.064f,  0.055f, -0.018f, 0.108f);
    for (int side = -1; side <= 1; side += 2) {
        float ex = side * 0.048f;
        t3d_mb_set_color(b, 0.05f, 0.05f, 0.07f, 1.0f);
        emit_cbox(b, ex - 0.018f, 0.002f, 0.094f,  ex + 0.018f, 0.044f, 0.114f);
        t3d_mb_set_color(b, 0.98f, 0.98f, 0.99f, 1.0f);
        emit_cbox(b, ex - 0.007f, 0.024f, 0.108f,  ex + 0.007f, 0.038f, 0.118f);
    }
    t3d_mb_set_color(b, 0.98f, 0.68f, 0.12f, 1.0f);
    t3d_mb_set_normal(b,  0.0f,  0.6f, 0.8f);
    t3d_mb_push_tri(b, -0.042f,  0.002f, 0.092f,  0.042f,  0.002f, 0.092f, 0.0f, -0.016f, 0.182f);
    t3d_mb_set_normal(b,  0.0f, -0.6f, 0.8f);
    t3d_mb_push_tri(b,  0.042f, -0.034f, 0.092f, -0.042f, -0.034f, 0.092f, 0.0f, -0.016f, 0.182f);
    t3d_mb_set_normal(b, -0.8f,  0.0f, 0.6f);
    t3d_mb_push_tri(b, -0.042f, -0.034f, 0.092f, -0.042f,  0.002f, 0.092f, 0.0f, -0.016f, 0.182f);
    t3d_mb_set_normal(b,  0.8f,  0.0f, 0.6f);
    t3d_mb_push_tri(b,  0.042f,  0.002f, 0.092f,  0.042f, -0.034f, 0.092f, 0.0f, -0.016f, 0.182f);
    m_penguin_head = t3d_mb_end(b);

    // 7. Mini Snowman Easter Egg
    b = t3d_mb_begin(T3D_PROP_SCALE);
    emit_ellipsoid_at(b, 0.0f, 0.095f, 0.0f, 0.125f, 0.100f, 0.125f, 0.96f, 0.97f, 0.99f);
    emit_ellipsoid_at(b, 0.0f, 0.245f, 0.0f, 0.095f, 0.080f, 0.095f, 0.96f, 0.97f, 0.99f);
    emit_ellipsoid_at(b, 0.0f, 0.370f, 0.0f, 0.070f, 0.064f, 0.070f, 0.96f, 0.97f, 0.99f);
    emit_gem_tris_at(b, 0.0f, 0.312f, 0.0f, 0.076f, 0.022f, 0.076f, 0.88f, 0.12f, 0.16f);
    emit_gem_tris_at(b, 0.0f, 0.422f, 0.0f, 0.084f, 0.012f, 0.084f, 0.10f, 0.10f, 0.12f);
    t3d_mb_set_color(b, 0.96f, 0.44f, 0.08f, 1.0f);
    t3d_mb_set_normal(b,  0.0f,  0.6f, 0.8f);
    t3d_mb_push_tri(b, -0.018f, 0.378f, 0.058f,  0.018f, 0.378f, 0.058f, 0.0f, 0.366f, 0.128f);
    t3d_mb_set_normal(b,  0.0f, -0.6f, 0.8f);
    t3d_mb_push_tri(b,  0.018f, 0.354f, 0.058f, -0.018f, 0.354f, 0.058f, 0.0f, 0.366f, 0.128f);
    t3d_mb_set_normal(b, -0.8f,  0.0f, 0.6f);
    t3d_mb_push_tri(b, -0.018f, 0.354f, 0.058f, -0.018f, 0.378f, 0.058f, 0.0f, 0.366f, 0.128f);
    t3d_mb_set_normal(b,  0.8f,  0.0f, 0.6f);
    t3d_mb_push_tri(b,  0.018f, 0.378f, 0.058f,  0.018f, 0.354f, 0.058f, 0.0f, 0.366f, 0.128f);

    t3d_mb_set_color(b, 0.88f, 0.12f, 0.16f, 1.0f);
    emit_cbox(b, 0.026f, 0.215f, 0.068f,  0.062f, 0.308f, 0.096f);
    t3d_mb_set_color(b, 0.08f, 0.08f, 0.10f, 1.0f);
    emit_cbox(b, -0.032f, 0.382f, 0.054f, -0.014f, 0.400f, 0.072f);
    emit_cbox(b,  0.014f, 0.382f, 0.054f,  0.032f, 0.400f, 0.072f);
    emit_cbox(b, -0.012f, 0.258f, 0.082f,  0.012f, 0.278f, 0.102f);
    emit_cbox(b, -0.012f, 0.215f, 0.082f,  0.012f, 0.235f, 0.102f);
    emit_cbox(b, -0.012f, 0.115f, 0.110f,  0.012f, 0.135f, 0.130f);
    t3d_mb_set_color(b, 0.10f, 0.10f, 0.12f, 1.0f);
    emit_cbox(b, -0.048f, 0.432f, -0.048f,  0.048f, 0.517f, 0.048f);
    t3d_mb_set_color(b, 0.86f, 0.12f, 0.16f, 1.0f);
    emit_cbox(b, -0.051f, 0.436f, -0.051f,  0.051f, 0.456f, 0.051f);
    for (int side = -1; side <= 1; side += 2) {
        emit_cbox_xform(b, -0.010f, 0.0f, -0.010f, 0.010f, 0.110f, 0.010f,
                        (float)side * -42.0f, 0.0f,
                        side * 0.086f, 0.255f, 0.0f, 0.38f, 0.22f, 0.10f);
    }
    m_mini_snowman = t3d_mb_end(b);
}

static void render_frog(const Turkey *tk) {
    bool in_air  = (tk->hop_y > 0.015f);
    bool casting = (tk->state == TURKEY_FROG_CAST);
    float pitch  = casting ? -22.0f : (in_air ? (-tk->hop_vy * 14.0f) : -14.0f);
    float crouch = (!in_air && !casting && tk->hop_pause < 0.12f && tk->state != TURKEY_PAUSE_GOBBLE) ? -0.025f : 0.0f;
    float breath = sinf(tk->walk_phase) * 0.008f;
    float throat = 0.062f + 0.058f * tk->tail_flare + breath;

    t3d_xf_reset();
    t3d_xf_translate(tk->x, tk->y + tk->hop_y, tk->z);
    t3d_xf_rotate_y(tk->yaw);
    t3d_xf_translate(0.0f, 0.125f + crouch, 0.0f);
    t3d_xf_rotate_x(pitch);

    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_frog_body_head);

    // Inflatable cream throat sac
    t3d_xf_push();
    t3d_xf_translate(0.0f, -0.027f - 0.025f * tk->tail_flare, 0.150f);
    t3d_xf_apply(T3D_PROP_SCALE);
    draw_dyn_gem(throat * 1.15f, throat * 0.92f, throat * 1.05f, 0.94f, 0.92f, 0.62f);
    t3d_xf_pop();

    // Pointed Hat while casting magic
    if (casting) {
        t3d_xf_push();
        t3d_xf_translate(0.0f, 0.130f, 0.087f);
        t3d_xf_rotate_x(-12.0f);
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw((tk->season == SEASON_CHRISTMAS) ? &m_santa_hat_frog : &m_wizard_hat_frog);
        t3d_xf_pop();
    }

    // Front Arms + Glowing Magic Wand when casting
    for (int side = -1; side <= 1; side += 2) {
        t3d_xf_push();
        t3d_xf_translate(side * 0.115f, -0.020f, 0.110f);
        if (casting && side == 1) {
            float wave_x = -115.0f + 26.0f * sinf(tk->cast_elapsed * 14.0f);
            float wave_z =  20.0f * cosf(tk->cast_elapsed * 14.0f);
            t3d_xf_rotate_x(wave_x);
            t3d_xf_rotate_z(wave_z);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_frog_wand_arm);

            t3d_xf_translate(0.0f, -0.355f, 0.0f);
            t3d_xf_rotate_y(tk->cast_elapsed * 260.0f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_frog_wand_star);
        } else {
            t3d_xf_rotate_x(in_air ? -28.0f : 12.0f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_frog_arm);
        }
        t3d_xf_pop();
    }

    // Hind Legs
    for (int side = -1; side <= 1; side += 2) {
        t3d_xf_push();
        t3d_xf_translate(side * 0.135f, -0.035f, -0.075f);
        if (in_air) {
            t3d_xf_rotate_x(52.0f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_frog_leg_air);
        } else {
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_frog_leg_seated);
        }
        t3d_xf_pop();
    }
}

static void render_turkey(const Turkey *tk) {
    bool walking = (tk->state == TURKEY_WALK_IN ||
                    tk->state == TURKEY_WALK_OUT ||
                    tk->state == TURKEY_SCURRY ||
                    tk->state == TURKEY_WALK_TO_FIRE ||
                    tk->state == TURKEY_STRUT_DANCE);
    float stride  = walking ? sinf(tk->walk_phase) : 0.0f;
    float waddle  = walking ? cosf(tk->walk_phase) * 7.5f : 0.0f;
    float bob_y   = walking ? fabsf(sinf(tk->walk_phase)) * 0.022f
                            : sinf(tk->walk_phase) * 0.008f;
    float head_z  = walking ? sinf(tk->walk_phase * 2.0f) * 0.032f : 0.0f;
    float wing_fl = (tk->state == TURKEY_SCURRY || tk->state == TURKEY_STRUT_DANCE || tk->hop_y > 0.01f)
                    ? (28.0f + 26.0f * sinf(tk->walk_phase * 2.5f))
                    : (6.0f + 6.0f * tk->tail_flare);

    t3d_xf_reset();
    t3d_xf_translate(tk->x, tk->y + tk->hop_y, tk->z);
    t3d_xf_rotate_y(tk->yaw);

    for (int side = -1; side <= 1; side += 2) {
        t3d_xf_push();
        t3d_xf_translate(side * 0.068f, 0.18f, 0.0f);
        t3d_xf_rotate_x(stride * (float)side * 28.0f);
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw(&m_turkey_leg);
        t3d_xf_pop();
    }

    t3d_xf_translate(0.0f, 0.27f + bob_y, 0.0f);
    t3d_xf_rotate_z(waddle);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_turkey_body);

    for (int side = -1; side <= 1; side += 2) {
        t3d_xf_push();
        t3d_xf_translate(side * 0.148f, 0.025f, 0.0f);
        t3d_xf_rotate_z((float)side * wing_fl);
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw(&m_turkey_wing);
        t3d_xf_pop();
    }

    t3d_xf_push();
    t3d_xf_translate(0.0f, 0.04f, -0.165f);
    t3d_xf_rotate_x(-20.0f + 16.0f * tk->tail_flare);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_turkey_tail);
    t3d_xf_pop();

    t3d_xf_push();
    t3d_xf_translate(0.0f, 0.095f, 0.150f + head_z);
    t3d_xf_rotate_y(tk->head_turn);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_turkey_head);
    t3d_xf_pop();
}

static void render_roasted_turkey(const Turkey *tk) {
    t3d_xf_reset();
    t3d_xf_translate(tk->x, tk->y + tk->hop_y, tk->z);
    t3d_xf_rotate_y(tk->spin_angle);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_roasted_turkey);
}

static void render_cat(const Turkey *tk) {
    bool walking = (tk->state == TURKEY_WALK_IN ||
                    tk->state == TURKEY_WALK_OUT ||
                    tk->state == TURKEY_SCURRY);
    bool sitting = (tk->state == TURKEY_PAUSE_GOBBLE);
    bool napping = (tk->state == TURKEY_CAT_NAP);

    float stride = walking ? sinf(tk->walk_phase) : 0.0f;
    float bob_y  = walking ? fabsf(sinf(tk->walk_phase)) * 0.016f : 0.0f;
    float swish  = sinf(tk->walk_phase * (sitting ? 1.1f : 0.75f)) * (sitting ? 26.0f : 16.0f);

    t3d_xf_reset();
    t3d_xf_translate(tk->x, tk->y + tk->hop_y, tk->z);
    t3d_xf_rotate_y(tk->yaw);

    if (napping) {
        float breath_y = 0.004f * sinf(tk->walk_phase);
        t3d_xf_translate(0.0f, breath_y, 0.0f);
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw(&m_cat_nap);
        if (tk->season == SEASON_CHRISTMAS) {
            t3d_xf_translate(0.0f, 0.115f, 0.165f);
            t3d_xf_rotate_x(14.0f);
            t3d_xf_translate(0.0f, 0.068f, -0.006f);
            t3d_xf_rotate_x(-12.0f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_santa_hat_cat);
        } else if (tk->season == SEASON_SPRING) {
            t3d_xf_translate(0.0f, 0.115f, 0.165f);
            t3d_xf_rotate_x(14.0f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_spring_flower_cat);
        } else if (tk->season == SEASON_SUMMER) {
            t3d_xf_translate(0.0f, 0.115f, 0.165f);
            t3d_xf_rotate_x(14.0f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_summer_shades_cat);
        }
        return;
    }

    float body_y     = sitting ? 0.175f : (0.195f + bob_y);
    float body_pitch = sitting ? -26.0f : (tk->state == TURKEY_SCURRY ? -10.0f : 0.0f);

    for (int side = -1; side <= 1; side += 2) {
        float lx = side * 0.064f;
        t3d_xf_push();
        t3d_xf_translate(lx, 0.155f, 0.115f);
        t3d_xf_rotate_x(sitting ? -6.0f : (stride * (float)side * 26.0f));
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw(&m_cat_front_leg);
        t3d_xf_pop();

        t3d_xf_push();
        if (sitting) {
            t3d_xf_translate(lx * 1.15f, 0.085f, -0.105f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_cat_haunch);
        } else {
            t3d_xf_translate(lx, 0.155f, -0.120f);
            t3d_xf_rotate_x(-stride * (float)side * 26.0f);
            t3d_xf_apply(T3D_PROP_SCALE);
            t3d_mesh_draw(&m_cat_back_leg);
        }
        t3d_xf_pop();
    }

    t3d_xf_translate(0.0f, body_y, 0.0f);
    t3d_xf_rotate_x(body_pitch);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_cat_torso);

    t3d_xf_push();
    t3d_xf_translate(0.0f, 0.035f, -0.175f);
    t3d_xf_rotate_y(swish);
    t3d_xf_rotate_x(-body_pitch + 18.0f);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_cat_tail_lower);

    t3d_xf_translate(0.0f, 0.145f, 0.0f);
    t3d_xf_rotate_z(26.0f + swish * 0.4f);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_cat_tail_upper);
    t3d_xf_pop();

    t3d_xf_push();
    t3d_xf_translate(0.0f, 0.085f, 0.185f);
    t3d_xf_rotate_x(-body_pitch);
    t3d_xf_rotate_y(tk->head_turn);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_cat_head);
    t3d_mesh_draw(&m_cat_ears);

    if (tk->season == SEASON_CHRISTMAS) {
        t3d_xf_translate(0.0f, 0.072f, -0.006f);
        t3d_xf_rotate_x(-10.0f);
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw(&m_santa_hat_cat);
    } else if (tk->season == SEASON_SPRING) {
        t3d_mesh_draw(&m_spring_flower_cat);
    } else if (tk->season == SEASON_SUMMER) {
        t3d_mesh_draw(&m_summer_shades_cat);
    }
    t3d_xf_pop();
}

static void render_penguin(const Turkey *tk) {
    bool walking = (tk->state == TURKEY_WALK_IN ||
                    tk->state == TURKEY_WALK_OUT ||
                    tk->state == TURKEY_SCURRY);
    bool sliding = (tk->state == TURKEY_PENGUIN_SLIDE);
    float stride  = walking ? sinf(tk->walk_phase) : (sliding ? sinf(tk->walk_phase) * 0.8f : 0.0f);
    float waddle  = walking ? cosf(tk->walk_phase) * 8.5f : 0.0f;
    float bob_y   = walking ? fabsf(sinf(tk->walk_phase)) * 0.020f
                            : sinf(tk->walk_phase) * 0.006f;
    float flap    = sliding ? 38.0f
                  : (tk->state == TURKEY_SCURRY || tk->hop_y > 0.01f || tk->state == TURKEY_PAUSE_GOBBLE || tk->state == TURKEY_PENGUIN_SNOWMAN)
                    ? (26.0f + 22.0f * sinf(tk->walk_phase * 1.8f))
                    : 10.0f;

    t3d_xf_reset();
    t3d_xf_translate(tk->x, tk->y + tk->hop_y, tk->z);
    t3d_xf_rotate_y(tk->yaw);

    if (sliding) {
        t3d_xf_translate(0.0f, 0.145f, 0.0f);
        t3d_xf_rotate_x(80.0f);
    }

    for (int side = -1; side <= 1; side += 2) {
        t3d_xf_push();
        t3d_xf_translate(side * 0.072f, 0.075f, 0.0f);
        t3d_xf_rotate_x(stride * (float)side * 26.0f);
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw(&m_penguin_foot);
        t3d_xf_pop();
    }

    t3d_xf_translate(0.0f, (sliding ? 0.21f : (0.225f + bob_y)), 0.0f);
    t3d_xf_rotate_z(waddle);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_penguin_body);

    for (int side = -1; side <= 1; side += 2) {
        t3d_xf_push();
        t3d_xf_translate(side * 0.142f, 0.055f, 0.010f);
        t3d_xf_rotate_z((float)side * flap);
        if (sliding) {
            t3d_xf_rotate_x(-25.0f);
        }
        t3d_xf_apply(T3D_PROP_SCALE);
        t3d_mesh_draw(&m_penguin_flipper);
        t3d_xf_pop();
    }

    t3d_xf_push();
    t3d_xf_translate(0.078f, 0.127f, -0.030f);
    t3d_xf_rotate_x(sliding ? -65.0f : (-18.0f + sinf(tk->walk_phase) * 8.0f));
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_penguin_scarf_tail);
    t3d_xf_pop();

    t3d_xf_push();
    t3d_xf_translate(0.0f, 0.222f, 0.015f);
    t3d_xf_rotate_y(tk->head_turn);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_penguin_head);
    t3d_xf_pop();
}

static void render_mini_snowman(float cx, float cy, float cz) {
    t3d_xf_reset();
    t3d_xf_translate(cx, cy, cz);
    t3d_xf_apply(T3D_PROP_SCALE);
    t3d_mesh_draw(&m_mini_snowman);
}

void turkey_render(const Turkey *tk) {
    if (!s_lists_ready) {
        turkey_init_lists();
    }
    bool draw_visitor = (tk->state != TURKEY_HIDDEN && tk->state != TURKEY_ROASTING);
    if (tk->snowman_timer <= 0.0f && !draw_visitor) {
        return;
    }

    t3d_matrix_push_pos(1);

    if (tk->snowman_timer > 0.0f) {
        render_mini_snowman(tk->snowman_x, 0.18f + tk->snowman_pop_y, tk->snowman_z);
    }

    if (draw_visitor) {
        if (tk->state == TURKEY_ROAST_POP || tk->state == TURKEY_SERVED) {
            render_roasted_turkey(tk);
        } else if (tk->species == VISITOR_FROG) {
            render_frog(tk);
        } else if (tk->species == VISITOR_CAT) {
            render_cat(tk);
        } else if (tk->species == VISITOR_PENGUIN) {
            render_penguin(tk);
        } else {
            render_turkey(tk);
        }
    }

    t3d_matrix_pop(1);
}

static void draw_magic_quad(float x, float y, float z, float sz, float rx, float rz,
                            float r, float g, float b, float a) {
    float dx = rx * sz;
    float dz = rz * sz;
    T3DBuildVert v0 = { x - dx, y - sz, z - dz,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,  r, g, b, a };
    T3DBuildVert v1 = { x + dx, y - sz, z + dz,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,  r, g, b, a };
    T3DBuildVert v2 = { x + dx, y + sz, z + dz,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,  r, g, b, a };
    T3DBuildVert v3 = { x - dx, y + sz, z - dz,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,  r, g, b, a };
    t3d_dyn_quad_vc(&v0, &v1, &v2, &v3);
}

void turkey_render_magic_fx(const Turkey *tk, const T3DTexture *tex_spark, float cam_x, float cam_z) {
    bool casting = (tk->state == TURKEY_FROG_CAST && tk->cast_elapsed < 0.90f);
    bool burst   = (tk->magic_burst > 0.0f);
    bool napping = (tk->state == TURKEY_CAT_NAP);
    bool sliding = (tk->state == TURKEY_PENGUIN_SLIDE);
    if (!casting && !burst && !napping && !sliding) return;

    float vx = cam_x;
    float vz = cam_z - 0.34f;
    float len = sqrtf(vx * vx + vz * vz);
    if (len < 0.001f) len = 1.0f;
    float rx = vz / len;
    float rz = -vx / len;

    t3d_dyn_begin_quads(tex_spark, T3D_ROOM_SCALE, true, false, false, false);

    // 1. Sparkling magic bolt arcing from frog's wand to the left hearth prop (-1.46, 0.32, 0.34)!
    if (casting) {
        float t = tk->cast_elapsed / 0.72f;
        float sx = tk->x, sy = tk->y + 0.42f, sz_pos = tk->z;
        float ex = -1.46f, ey = 0.32f, ez = 0.34f;

        for (int i = 0; i < 10; i++) {
            float u = t - (float)i * 0.055f;
            if (u < 0.0f || u > 1.0f) continue;
            float px = sx + (ex - sx) * u;
            float py = sy + (ey - sy) * u + sinf(u * 3.14159f) * 0.36f;
            float pz = sz_pos + (ez - sz_pos) * u + sinf(u * 12.0f + i) * 0.04f;
            float size = (0.068f - i * 0.0045f);
            float alpha = 1.0f - i * 0.085f;
            if (tk->season == SEASON_SPRING) {
                if ((i & 1) == 0) draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.64f, 0.84f, alpha);
                else              draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.94f, 0.42f, alpha);
            } else if (tk->season == SEASON_SUMMER) {
                if ((i & 1) == 0) draw_magic_quad(px, py, pz, size, rx, rz, 0.86f, 1.00f, 0.36f, alpha);
                else              draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.72f, 0.22f, alpha);
            } else if (tk->season == SEASON_CHRISTMAS) {
                if ((i & 1) == 0) draw_magic_quad(px, py, pz, size, rx, rz, 0.96f, 0.98f, 1.00f, alpha);
                else              draw_magic_quad(px, py, pz, size, rx, rz, 0.45f, 0.92f, 1.00f, alpha);
            } else {
                if ((i & 1) == 0) draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.94f, 0.36f, alpha);
                else              draw_magic_quad(px, py, pz, size, rx, rz, 0.45f, 0.92f, 1.00f, alpha);
            }
        }
    }

    // 2. Celebratory seasonal sparkle poof
    if (burst) {
        float u = 1.0f - (tk->magic_burst / 1.15f);
        float alpha = (1.0f - u) * 0.95f;
        for (int i = 0; i < 14; i++) {
            float ang = (float)i * 0.4488f + u * 2.2f;
            float rad = 0.08f + u * 0.36f;
            float px = tk->burst_x + cosf(ang) * rad;
            float py = 0.26f + u * (0.28f + (i % 3) * 0.14f) - u * u * 0.16f;
            float pz = tk->burst_z + sinf(ang) * rad;
            float size = (0.062f - 0.025f * u) * (0.75f + 0.25f * (i % 2));
            if (tk->season == SEASON_SPRING) {
                if (i % 3 == 0)      draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.68f, 0.86f, alpha);
                else if (i % 3 == 1) draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.94f, 0.44f, alpha);
                else                 draw_magic_quad(px, py, pz, size, rx, rz, 0.98f, 0.92f, 0.98f, alpha);
            } else if (tk->season == SEASON_SUMMER) {
                if (i % 3 == 0)      draw_magic_quad(px, py, pz, size, rx, rz, 0.86f, 1.00f, 0.36f, alpha);
                else if (i % 3 == 1) draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.78f, 0.24f, alpha);
                else                 draw_magic_quad(px, py, pz, size, rx, rz, 0.56f, 0.96f, 0.62f, alpha);
            } else if (tk->season == SEASON_CHRISTMAS) {
                if (i % 3 == 0)      draw_magic_quad(px, py, pz, size, rx, rz, 0.96f, 0.98f, 1.00f, alpha);
                else if (i % 3 == 1) draw_magic_quad(px, py, pz, size, rx, rz, 0.50f, 0.92f, 1.00f, alpha);
                else                 draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.38f, 0.38f, alpha);
            } else {
                if (i % 3 == 0)      draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.92f, 0.32f, alpha);
                else if (i % 3 == 1) draw_magic_quad(px, py, pz, size, rx, rz, 1.00f, 0.56f, 0.88f, alpha);
                else                 draw_magic_quad(px, py, pz, size, rx, rz, 0.50f, 0.96f, 1.00f, alpha);
            }
        }
    }

    // 3. Soft floating dreamy sleep motes above the napping fireside cat!
    if (napping) {
        for (int i = 0; i < 4; i++) {
            float u = fmodf(tk->walk_phase * 0.22f + (float)i * 0.25f, 1.0f);
            float alpha = sinf(u * 3.14159f) * 0.72f;
            float px = tk->x + sinf(u * 5.0f + i) * 0.08f;
            float py = tk->y + 0.24f + u * 0.42f;
            float pz = tk->z + 0.10f;
            float size = 0.024f + 0.022f * u;
            draw_magic_quad(px, py, pz, size, rx, rz, 0.95f, 0.92f, 1.00f, alpha);
        }
    }

    // 4. Crisp frosty cyan/white snow-spray motes trailing behind the sliding SM64 Penguin!
    if (sliding) {
        float yrad = tk->yaw * 0.0174533f;
        float bx = -sinf(yrad);
        float bz = -cosf(yrad);
        for (int i = 0; i < 8; i++) {
            float u = fmodf(tk->slide_elapsed * 2.4f + (float)i * 0.125f, 1.0f);
            float alpha = (1.0f - u) * 0.88f;
            float lat = ((i & 1) ? 1.0f : -1.0f) * (0.04f + 0.12f * u);
            float px = tk->x + bx * (0.12f + u * 0.34f) + bz * lat;
            float py = tk->y + 0.06f + u * 0.20f;
            float pz = tk->z + bz * (0.12f + u * 0.34f) - bx * lat;
            float size = 0.036f * (1.0f - 0.35f * u);
            if ((i & 1) == 0) draw_magic_quad(px, py, pz, size, rx, rz, 0.88f, 0.96f, 1.00f, alpha);
            else              draw_magic_quad(px, py, pz, size, rx, rz, 0.55f, 0.90f, 1.00f, alpha);
        }
    }

    t3d_dyn_end_quads();
}
