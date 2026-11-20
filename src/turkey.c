#include "turkey.h"
#include <GL/gl.h>
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
    tk->species     = VISITOR_TURKEY;
    tk->state       = TURKEY_HIDDEN;
    // First visitor arrives ~7 seconds after boot
    tk->state_timer = 7.0f;
}

void turkey_summon_species(Turkey *tk, VisitorSpecies sp) {
    start_visit(tk, sp);
}

void turkey_cycle_summon(Turkey *tk, int delta) {
    if (tk->state == TURKEY_HIDDEN) {
        VisitorSpecies sp = (delta >= 0) ? VISITOR_TURKEY : VISITOR_CAT;
        start_visit(tk, sp);
    } else {
        VisitorSpecies next = (VisitorSpecies)(((int)tk->species + delta + VISITOR_COUNT) % VISITOR_COUNT);
        start_visit(tk, next);
    }
}

void turkey_startle(Turkey *tk) {
    if (tk->state == TURKEY_HIDDEN) return;
    // Don't interrupt active Easter eggs!
    if (tk->state >= TURKEY_WALK_TO_FIRE) return;
    if (fabsf(tk->x) < 1.60f && tk->state != TURKEY_SCURRY) {
        tk->state      = TURKEY_SCURRY;
        tk->hop_vy     = (tk->species == VISITOR_FROG) ? 2.40f
                       : (tk->species == VISITOR_CAT)  ? 2.05f : 1.65f;
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
    tk->state        = TURKEY_WALK_TO_FIRE;
    tk->state_timer  = 0.0f;
    tk->hop_vy       = 0.95f;
    tk->tail_flare   = 1.0f;
    tk->just_gobbled = true;
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

void turkey_update(Turkey *tk, float dt) {
    tk->just_gobbled      = false;
    tk->just_ribbit       = false;
    tk->just_meowed       = false;
    tk->just_entered_fire = false;
    tk->just_dinged       = false;
    tk->just_cast_magic   = false;
    tk->just_made_pie     = false;

    // Update persistent Pumpkin Pie & magic sparkle burst timers even when visitor is hidden!
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
    if (tk->magic_burst > 0.0f) {
        tk->magic_burst -= dt;
        if (tk->magic_burst < 0.0f) tk->magic_burst = 0.0f;
    }

    if (tk->state == TURKEY_HIDDEN) {
        tk->state_timer -= dt;
        if (tk->state_timer <= 0.0f) {
            // Random hearth visitor: ~40% Turkey, ~30% Frog, ~30% Cozy Cat!
            float r = tkrand01();
            VisitorSpecies sp = (r < 0.40f) ? VISITOR_TURKEY
                              : (r < 0.70f) ? VISITOR_FROG
                                            : VISITOR_CAT;
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

        // At t = 0.72s, the magic beam hits the pumpkin -> POOF! Pumpkin Pie!
        if (!tk->cast_transformed && tk->cast_elapsed >= 0.72f) {
            tk->cast_transformed = true;
            tk->pie_timer        = 45.0f; // stays a pie for 45s so you can even roast a turkey next!
            tk->pie_pop_y        = 0.04f;
            tk->pie_pop_vy       = 1.65f;
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

// Helper: draws a clean untextured low-poly colored box with proper unit normals
static void draw_cbox(float x0, float y0, float z0,
                      float x1, float y1, float z1) {
    glBegin(GL_QUADS);
    // Front (+Z)
    glNormal3f(0, 0, 1);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    // Back (-Z)
    glNormal3f(0, 0, -1);
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
    // Left (-X)
    glNormal3f(-1, 0, 0);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    // Right (+X)
    glNormal3f(1, 0, 0);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    // Top (+Y)
    glNormal3f(0, 1, 0);
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    // Bottom (-Y)
    glNormal3f(0, -1, 0);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glEnd();
}

// Helper: authentic 90s N64 12-triangle faceted low-poly gem/ellipsoid (outward CCW, ~3x faster than 32-tri!)
static void draw_gem(float rx, float ry, float rz) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };

    glBegin(GL_TRIANGLES);
    // Top pyramid (6 tris, CCW from above)
    for (int i = 0; i < 6; i++) {
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0.0f, ry, 0.0f);
        glNormal3f(cs[i], 0.35f, sn[i]);
        glVertex3f(cs[i] * rx, 0.0f, sn[i] * rz);
        glNormal3f(cs[i + 1], 0.35f, sn[i + 1]);
        glVertex3f(cs[i + 1] * rx, 0.0f, sn[i + 1] * rz);
    }
    // Bottom pyramid (6 tris, CCW from below)
    for (int i = 0; i < 6; i++) {
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(0.0f, -ry, 0.0f);
        glNormal3f(cs[i + 1], -0.35f, sn[i + 1]);
        glVertex3f(cs[i + 1] * rx, 0.0f, sn[i + 1] * rz);
        glNormal3f(cs[i], -0.35f, sn[i]);
        glVertex3f(cs[i] * rx, 0.0f, sn[i] * rz);
    }
    glEnd();
}

// Helper: low-poly 6-sided beveled barrel ellipsoid (24 tris, outward CCW)
static void draw_ellipsoid(float rx, float ry, float rz) {
    static const float cs[7] = { 1.0f,  0.5f, -0.5f, -1.0f, -0.5f,  0.5f, 1.0f };
    static const float sn[7] = { 0.0f, -0.866f,-0.866f, 0.0f,  0.866f, 0.866f, 0.0f };
    static const float lat_y[4] = { -1.0f, -0.45f, 0.45f, 1.0f };
    static const float lat_r[4] = {  0.0f,  0.89f, 0.89f, 0.0f };

    glBegin(GL_QUADS);
    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];

        glNormal3f(c0 * 0.89f, -0.45f, s0 * 0.89f);
        glVertex3f(c0 * lat_r[1] * rx, lat_y[1] * ry, s0 * lat_r[1] * rz);

        glNormal3f(c1 * 0.89f, -0.45f, s1 * 0.89f);
        glVertex3f(c1 * lat_r[1] * rx, lat_y[1] * ry, s1 * lat_r[1] * rz);

        glNormal3f(c1 * 0.89f,  0.45f, s1 * 0.89f);
        glVertex3f(c1 * lat_r[2] * rx, lat_y[2] * ry, s1 * lat_r[2] * rz);

        glNormal3f(c0 * 0.89f,  0.45f, s0 * 0.89f);
        glVertex3f(c0 * lat_r[2] * rx, lat_y[2] * ry, s0 * lat_r[2] * rz);
    }
    glEnd();

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0.0f, ry, 0.0f);
        glNormal3f(c0 * 0.89f, 0.45f, s0 * 0.89f);
        glVertex3f(c0 * lat_r[2] * rx, lat_y[2] * ry, s0 * lat_r[2] * rz);
        glNormal3f(c1 * 0.89f, 0.45f, s1 * 0.89f);
        glVertex3f(c1 * lat_r[2] * rx, lat_y[2] * ry, s1 * lat_r[2] * rz);
    }
    for (int i = 0; i < 6; i++) {
        float c0 = cs[i], s0 = sn[i];
        float c1 = cs[i + 1], s1 = sn[i + 1];
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(0.0f, -ry, 0.0f);
        glNormal3f(c1 * 0.89f, -0.45f, s1 * 0.89f);
        glVertex3f(c1 * lat_r[1] * rx, lat_y[1] * ry, s1 * lat_r[1] * rz);
        glNormal3f(c0 * 0.89f, -0.45f, s0 * 0.89f);
        glVertex3f(c0 * lat_r[1] * rx, lat_y[1] * ry, s0 * lat_r[1] * rz);
    }
    glEnd();
}

static void render_frog(const Turkey *tk) {
    bool in_air  = (tk->hop_y > 0.015f);
    bool casting = (tk->state == TURKEY_FROG_CAST);
    float pitch  = casting ? -22.0f : (in_air ? (-tk->hop_vy * 14.0f) : -14.0f);
    float crouch = (!in_air && !casting && tk->hop_pause < 0.12f && tk->state != TURKEY_PAUSE_GOBBLE) ? -0.025f : 0.0f;
    float breath = sinf(tk->walk_phase) * 0.008f;
    float throat = 0.062f + 0.058f * tk->tail_flare + breath;

    glPushMatrix();
    glTranslatef(tk->x, tk->y + tk->hop_y, tk->z);
    glRotatef(tk->yaw, 0.0f, 1.0f, 0.0f);

    // ---------------------------------------------------------
    // 1. Plump N64 Low-Poly Frog Body, Belly & Snout (~120 tris total!)
    // ---------------------------------------------------------
    glPushMatrix();
    glTranslatef(0.0f, 0.125f + crouch, 0.0f);
    glRotatef(pitch, 1.0f, 0.0f, 0.0f);

    // Emerald green plump torso
    glColor3f(0.24f, 0.76f, 0.24f);
    draw_ellipsoid(0.148f, 0.105f, 0.175f);

    // Cream-yellow belly
    glPushMatrix();
    glTranslatef(0.0f, -0.044f, 0.032f);
    glColor3f(0.88f, 0.90f, 0.56f);
    draw_gem(0.118f, 0.065f, 0.132f);
    glPopMatrix();

    // Cute darker forest-green back spots
    glColor3f(0.12f, 0.44f, 0.14f);
    draw_cbox(-0.085f, 0.078f, -0.065f, -0.025f, 0.104f,  0.005f);
    draw_cbox( 0.030f, 0.076f, -0.085f,  0.085f, 0.100f, -0.020f);

    // Wide smiling snout & inflatable throat sac
    glPushMatrix();
    glTranslatef(0.0f, 0.025f, 0.132f);
    glColor3f(0.26f, 0.80f, 0.26f);
    draw_gem(0.122f, 0.070f, 0.100f);

    // Wide frog smile stripe
    glColor3f(0.10f, 0.30f, 0.10f);
    draw_cbox(-0.092f, -0.014f, 0.068f,  0.092f, -0.002f, 0.098f);

    // Inflatable cream throat sac (balloons out on RIBBIT!)
    glPushMatrix();
    glTranslatef(0.0f, -0.052f - 0.025f * tk->tail_flare, 0.018f);
    glColor3f(0.94f, 0.92f, 0.62f);
    draw_gem(throat * 1.15f, throat * 0.92f, throat * 1.05f);
    glPopMatrix();

    // Iconic N64 chunky googly eyes perched on top of head
    for (int side = -1; side <= 1; side += 2) {
        float ex = side * 0.068f;
        glColor3f(0.24f, 0.78f, 0.24f);
        draw_cbox(ex - 0.042f, 0.036f, -0.030f,  ex + 0.042f, 0.114f, 0.046f);
        glColor3f(0.98f, 0.96f, 0.74f);
        draw_cbox(ex - 0.032f, 0.046f,  0.038f,  ex + 0.032f, 0.104f, 0.062f);
        glColor3f(0.05f, 0.05f, 0.07f);
        draw_cbox(ex - 0.015f, 0.060f,  0.058f,  ex + 0.015f, 0.090f, 0.072f);
    }

    // Tiny Blue Pointed Wizard Hat while casting magic!
    if (casting) {
        glPushMatrix();
        glTranslatef(0.0f, 0.105f, -0.045f);
        glRotatef(-12.0f, 1.0f, 0.0f, 0.0f);
        // Wide royal-blue brim
        glColor3f(0.16f, 0.24f, 0.82f);
        draw_gem(0.125f, 0.018f, 0.125f);
        // Gold ribbon band
        glColor3f(1.00f, 0.84f, 0.18f);
        draw_gem(0.078f, 0.024f, 0.078f);
        // Pointed wizard cone!
        glColor3f(0.18f, 0.28f, 0.88f);
        glBegin(GL_TRIANGLES);
        static const float hcs[7] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f, 1.0f };
        static const float hsn[7] = { 0.0f,-0.866f,-0.866f, 0.0f, 0.866f, 0.866f, 0.0f };
        for (int i = 0; i < 6; i++) {
            glNormal3f(hcs[i], 0.4f, hsn[i]);
            glVertex3f(0.0f, 0.21f, -0.02f);
            glVertex3f(hcs[i] * 0.072f, 0.01f, hsn[i] * 0.072f);
            glVertex3f(hcs[i + 1] * 0.072f, 0.01f, hsn[i + 1] * 0.072f);
        }
        glEnd();
        glPopMatrix();
    }
    glPopMatrix(); // head

    // ---------------------------------------------------------
    // 2. Front Arms + Glowing Magic Wand when casting!
    // ---------------------------------------------------------
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.115f, -0.020f, 0.110f);
        if (casting && side == 1) {
            // Raise right arm high & wave the magic wand!
            float wave_x = -115.0f + 26.0f * sinf(tk->cast_elapsed * 14.0f);
            float wave_z =  20.0f * cosf(tk->cast_elapsed * 14.0f);
            glRotatef(wave_x, 1.0f, 0.0f, 0.0f);
            glRotatef(wave_z, 0.0f, 0.0f, 1.0f);
            glColor3f(0.22f, 0.72f, 0.22f);
            draw_cbox(-0.020f, -0.105f, -0.020f,  0.020f, 0.010f, 0.020f);
            glColor3f(0.32f, 0.82f, 0.26f);
            draw_cbox(-0.030f, -0.112f, -0.016f,  0.030f, -0.092f, 0.040f);

            // Wooden Magic Wand extending out of the frog's hand!
            glColor3f(0.42f, 0.24f, 0.10f);
            draw_cbox(-0.012f, -0.340f, -0.012f,  0.012f, -0.095f, 0.012f);

            // Bright Glowing Golden Star Tip on the wand!
            glDisable(GL_LIGHTING);
            glColor3f(1.00f, 0.94f, 0.36f);
            glPushMatrix();
            glTranslatef(0.0f, -0.355f, 0.0f);
            glRotatef(tk->cast_elapsed * 260.0f, 0.0f, 1.0f, 0.0f);
            draw_gem(0.056f, 0.056f, 0.056f);
            glPopMatrix();
            glEnable(GL_LIGHTING);
        } else {
            glRotatef(in_air ? -28.0f : 12.0f, 1.0f, 0.0f, 0.0f);
            glColor3f(0.22f, 0.72f, 0.22f);
            draw_cbox(-0.020f, -0.105f, -0.020f,  0.020f, 0.010f, 0.020f);
            glColor3f(0.32f, 0.82f, 0.26f);
            draw_cbox(-0.034f, -0.112f, -0.016f,  0.034f, -0.092f, 0.046f);
        }
        glPopMatrix();
    }

    // ---------------------------------------------------------
    // 3. Springy Hind Legs (folded when seated, kicked back mid-hop!)
    // ---------------------------------------------------------
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.135f, -0.035f, -0.075f);
        if (in_air) {
            glRotatef(52.0f, 1.0f, 0.0f, 0.0f);
            glColor3f(0.20f, 0.68f, 0.20f);
            draw_gem(0.048f, 0.048f, 0.095f);
            glTranslatef(0.0f, -0.030f, -0.125f);
            draw_cbox(-0.022f, -0.022f, -0.095f,  0.022f, 0.022f, 0.040f);
            glColor3f(0.34f, 0.84f, 0.26f);
            draw_cbox(-0.048f, -0.014f, -0.155f,  0.048f, 0.014f, -0.080f);
        } else {
            glColor3f(0.20f, 0.68f, 0.20f);
            draw_gem(0.058f, 0.068f, 0.098f);
            glColor3f(0.34f, 0.84f, 0.26f);
            draw_cbox(-0.048f, -0.082f, -0.050f,  0.048f, -0.058f, 0.075f);
        }
        glPopMatrix();
    }

    glPopMatrix(); // body
    glPopMatrix(); // frog root
}

static void render_turkey(const Turkey *tk) {
    bool walking = (tk->state == TURKEY_WALK_IN ||
                    tk->state == TURKEY_WALK_OUT ||
                    tk->state == TURKEY_SCURRY ||
                    tk->state == TURKEY_WALK_TO_FIRE);
    float stride  = walking ? sinf(tk->walk_phase) : 0.0f;
    float waddle  = walking ? cosf(tk->walk_phase) * 7.5f : 0.0f;
    float bob_y   = walking ? fabsf(sinf(tk->walk_phase)) * 0.022f
                            : sinf(tk->walk_phase) * 0.008f;
    float head_z  = walking ? sinf(tk->walk_phase * 2.0f) * 0.032f : 0.0f;
    float wing_fl = (tk->state == TURKEY_SCURRY || tk->hop_y > 0.01f)
                    ? (28.0f + 26.0f * sinf(tk->walk_phase * 2.5f))
                    : (6.0f + 6.0f * tk->tail_flare);

    glPushMatrix();
    glTranslatef(tk->x, tk->y + tk->hop_y, tk->z);
    glRotatef(tk->yaw, 0.0f, 1.0f, 0.0f);

    // 1. Animated Orange Legs & Feet
    glColor3f(0.96f, 0.60f, 0.14f);
    for (int side = -1; side <= 1; side += 2) {
        float lx = side * 0.068f;
        float swing = stride * (float)side * 28.0f;
        glPushMatrix();
        glTranslatef(lx, 0.18f, 0.0f);
        glRotatef(swing, 1.0f, 0.0f, 0.0f);
        draw_cbox(-0.018f, -0.175f, -0.018f,  0.018f, 0.0f, 0.018f);
        draw_cbox(-0.038f, -0.180f, -0.025f,  0.038f, -0.158f, 0.070f);
        glPopMatrix();
    }

    // 2. Plump Waddling Turkey Body
    glPushMatrix();
    glTranslatef(0.0f, 0.27f + bob_y, 0.0f);
    glRotatef(waddle, 0.0f, 0.0f, 1.0f);

    glColor3f(0.64f, 0.34f, 0.15f);
    draw_ellipsoid(0.165f, 0.150f, 0.200f);

    glPushMatrix();
    glTranslatef(0.0f, -0.01f, 0.125f);
    glColor3f(0.78f, 0.46f, 0.20f);
    draw_gem(0.112f, 0.098f, 0.098f);
    glPopMatrix();

    // 3. Flapping Side Wings
    glColor3f(0.48f, 0.24f, 0.10f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.148f, 0.025f, 0.0f);
        glRotatef((float)side * wing_fl, 0.0f, 0.0f, 1.0f);
        draw_cbox(-0.028f, -0.098f, -0.125f,  0.028f, 0.045f, 0.098f);
        glPopMatrix();
    }

    // 4. Glorious Fanned Autumn Tail Feathers
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glTranslatef(0.0f, 0.04f, -0.165f);
    glRotatef(-20.0f + 16.0f * tk->tail_flare, 1.0f, 0.0f, 0.0f);

    float fan_scale = 1.0f + 0.18f * tk->tail_flare;
    for (int f = -3; f <= 3; f++) {
        float ang = (float)f * (19.5f + 3.5f * tk->tail_flare);
        glPushMatrix();
        glRotatef(ang, 0.0f, 0.0f, 1.0f);
        float r, g, b;
        int m = (f + 3) % 3;
        if (m == 0)      { r = 0.72f; g = 0.30f; b = 0.09f; }
        else if (m == 1) { r = 0.94f; g = 0.48f; b = 0.10f; }
        else             { r = 0.86f; g = 0.64f; b = 0.16f; }

        float fh = (0.34f - fabsf((float)f) * 0.018f) * fan_scale;
        float fw = 0.058f * fan_scale;
        float fz = -0.005f * fabsf((float)f);

        glColor3f(r, g, b);
        glNormal3f(0.0f, 0.25f, 0.96f);
        glBegin(GL_QUADS);
        glVertex3f(-fw * 0.42f, 0.0f,       fz);
        glVertex3f( fw * 0.42f, 0.0f,       fz);
        glVertex3f( fw,         fh * 0.80f, fz);
        glVertex3f(-fw,         fh * 0.80f, fz);
        glEnd();

        glColor3f(0.98f, 0.90f, 0.68f);
        glBegin(GL_TRIANGLES);
        glVertex3f(-fw,   fh * 0.80f, fz);
        glVertex3f( fw,   fh * 0.80f, fz);
        glVertex3f( 0.0f, fh,         fz);
        glEnd();
        glPopMatrix();
    }
    glPopMatrix();
    glEnable(GL_CULL_FACE);

    // 5. Bobbing Neck, Head, Red Wattle/Snood & Golden Beak
    glPushMatrix();
    glTranslatef(0.0f, 0.095f, 0.150f + head_z);
    glRotatef(tk->head_turn, 0.0f, 1.0f, 0.0f);

    glColor3f(0.84f, 0.28f, 0.20f);
    draw_cbox(-0.038f, -0.025f, -0.036f,  0.038f, 0.145f, 0.036f);

    glPushMatrix();
    glTranslatef(0.0f, 0.175f, 0.015f);
    glColor3f(0.90f, 0.26f, 0.22f);
    draw_gem(0.064f, 0.060f, 0.070f);

    glColor3f(1.00f, 0.14f, 0.14f);
    draw_cbox(-0.020f, -0.100f, 0.030f,  0.020f, -0.015f, 0.072f);
    draw_cbox(-0.014f, -0.028f, 0.068f,  0.014f,  0.022f, 0.098f);

    glColor3f(1.00f, 0.82f, 0.18f);
    glBegin(GL_TRIANGLES);
    glNormal3f( 0.0f,  0.6f, 0.8f);
    glVertex3f(-0.026f,  0.012f, 0.058f);
    glVertex3f( 0.026f,  0.012f, 0.058f);
    glVertex3f( 0.0f,   -0.008f, 0.135f);

    glNormal3f( 0.0f, -0.6f, 0.8f);
    glVertex3f( 0.026f, -0.020f, 0.058f);
    glVertex3f(-0.026f, -0.020f, 0.058f);
    glVertex3f( 0.0f,   -0.008f, 0.135f);

    glNormal3f(-0.8f,  0.0f, 0.6f);
    glVertex3f(-0.026f, -0.020f, 0.058f);
    glVertex3f(-0.026f,  0.012f, 0.058f);
    glVertex3f( 0.0f,   -0.008f, 0.135f);

    glNormal3f( 0.8f,  0.0f, 0.6f);
    glVertex3f( 0.026f,  0.012f, 0.058f);
    glVertex3f( 0.026f, -0.020f, 0.058f);
    glVertex3f( 0.0f,   -0.008f, 0.135f);
    glEnd();

    glColor3f(0.06f, 0.05f, 0.05f);
    draw_cbox(-0.066f, 0.006f, 0.018f, -0.052f, 0.028f, 0.042f);
    draw_cbox( 0.052f, 0.006f, 0.018f,  0.066f, 0.028f, 0.042f);

    glPopMatrix();
    glPopMatrix();
    glPopMatrix();
    glPopMatrix();
}

static void render_roasted_turkey(const Turkey *tk) {
    glPushMatrix();
    glTranslatef(tk->x, tk->y + tk->hop_y, tk->z);
    glRotatef(tk->spin_angle, 0.0f, 1.0f, 0.0f);

    // 1. Gleaming Silver Serving Platter (low-poly octagonal dish!)
    glColor3f(0.84f, 0.86f, 0.90f);
    glPushMatrix();
    glTranslatef(0.0f, 0.022f, 0.0f);
    draw_gem(0.32f, 0.024f, 0.25f);
    glPopMatrix();

    // Inner bright platter rim highlight
    glColor3f(0.95f, 0.96f, 0.98f);
    draw_cbox(-0.22f, 0.022f, -0.16f,  0.22f, 0.036f, 0.16f);

    // Fresh green parsley & red cranberry garnishes tucked around the platter!
    glColor3f(0.18f, 0.68f, 0.22f);
    draw_cbox(-0.25f, 0.034f,  0.07f, -0.18f, 0.068f,  0.14f);
    draw_cbox( 0.18f, 0.034f,  0.06f,  0.25f, 0.068f,  0.13f);
    draw_cbox(-0.24f, 0.034f, -0.13f, -0.17f, 0.068f, -0.06f);
    draw_cbox( 0.17f, 0.034f, -0.13f,  0.24f, 0.068f, -0.06f);
    glColor3f(0.88f, 0.12f, 0.16f);
    draw_cbox(-0.22f, 0.044f,  0.02f, -0.18f, 0.078f,  0.06f);
    draw_cbox( 0.19f, 0.044f, -0.04f,  0.23f, 0.078f,  0.00f);

    // 2. Plump Glazed Golden-Brown Roast Turkey Body!
    glPushMatrix();
    glTranslatef(0.0f, 0.155f, 0.0f);
    glColor3f(0.86f, 0.44f, 0.12f);
    draw_ellipsoid(0.195f, 0.135f, 0.155f);

    // Rich crispy caramelized golden breast highlight on top
    glPushMatrix();
    glTranslatef(-0.02f, 0.065f, 0.0f);
    glColor3f(0.95f, 0.56f, 0.16f);
    draw_gem(0.145f, 0.075f, 0.115f);
    glPopMatrix();

    // Tucked roasted side wings
    glColor3f(0.76f, 0.36f, 0.10f);
    draw_cbox(-0.14f, -0.055f,  0.125f,  0.08f, 0.030f,  0.175f);
    draw_cbox(-0.14f, -0.055f, -0.175f,  0.08f, 0.030f, -0.125f);

    // 3. Two Iconic Cartoon Drumsticks Poking Up with White Paper Chef Frills!
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(0.105f, 0.015f, (float)side * 0.105f);
        glRotatef((float)side * -24.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(-34.0f, 0.0f, 0.0f, 1.0f);

        // Juicy roasted drumstick thigh
        glColor3f(0.82f, 0.40f, 0.11f);
        draw_gem(0.062f, 0.095f, 0.062f);

        // Exposed bone stalk
        glColor3f(0.96f, 0.92f, 0.82f);
        draw_cbox(-0.018f, 0.070f, -0.018f,  0.018f, 0.165f, 0.018f);

        // Classic puffy white paper Thanksgiving frill/cuff on the bone tip!
        glColor3f(0.98f, 0.98f, 0.98f);
        glPushMatrix();
        glTranslatef(0.0f, 0.175f, 0.0f);
        draw_gem(0.052f, 0.044f, 0.052f);
        glPopMatrix();

        glPopMatrix();
    }

    glPopMatrix(); // roast body
    glPopMatrix(); // platter root
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
    float breath = napping ? (1.0f + 0.05f * sinf(tk->walk_phase)) : 1.0f;

    glPushMatrix();
    glTranslatef(tk->x, tk->y + tk->hop_y, tk->z);
    glRotatef(tk->yaw, 0.0f, 1.0f, 0.0f);

    if (napping) {
        // ---------------------------------------------------------
        // COZY CURLED-UP SLEEPING CAT LOAF (A+B Easter Egg!)
        // ---------------------------------------------------------
        glPushMatrix();
        glTranslatef(0.0f, 0.095f, 0.0f);

        // Curled warm ginger tabby loaf body
        glColor3f(0.92f, 0.52f, 0.16f);
        draw_ellipsoid(0.165f, 0.092f * breath, 0.185f);

        // Marmalade tabby back stripes
        glColor3f(0.70f, 0.34f, 0.09f);
        draw_cbox(-0.115f, 0.065f * breath, -0.090f,  0.115f, 0.088f * breath, -0.055f);
        draw_cbox(-0.125f, 0.072f * breath, -0.015f,  0.125f, 0.095f * breath,  0.020f);
        draw_cbox(-0.110f, 0.064f * breath,  0.055f,  0.110f, 0.086f * breath,  0.088f);

        // Tucked little white paws in front
        glColor3f(0.96f, 0.94f, 0.88f);
        draw_cbox(-0.085f, -0.092f, 0.130f, -0.025f, -0.056f, 0.195f);
        draw_cbox( 0.025f, -0.092f, 0.130f,  0.085f, -0.056f, 0.195f);

        // Tail curled snugly around side
        glColor3f(0.88f, 0.48f, 0.14f);
        draw_cbox(0.130f, -0.082f, -0.140f,  0.185f, -0.036f,  0.100f);
        glColor3f(0.96f, 0.94f, 0.88f);
        draw_cbox(0.110f, -0.082f,  0.090f,  0.175f, -0.036f,  0.155f);

        // Resting head tucked neatly down on paws
        glPushMatrix();
        glTranslatef(0.0f, 0.020f, 0.165f);
        glRotatef(14.0f, 1.0f, 0.0f, 0.0f);
        glColor3f(0.94f, 0.55f, 0.18f);
        draw_gem(0.092f, 0.078f, 0.088f);
        // Cream muzzle & tiny pink nose
        glColor3f(0.96f, 0.93f, 0.86f);
        draw_cbox(-0.042f, -0.044f, 0.052f,  0.042f, -0.008f, 0.095f);
        glColor3f(0.96f, 0.56f, 0.62f);
        draw_cbox(-0.015f, -0.014f, 0.088f,  0.015f,  0.006f, 0.104f);
        // Peaceful closed sleeping eye lines!
        glColor3f(0.18f, 0.10f, 0.06f);
        draw_cbox(-0.058f, 0.004f, 0.066f, -0.020f, 0.015f, 0.084f);
        draw_cbox( 0.020f, 0.004f, 0.066f,  0.058f, 0.015f, 0.084f);
        // Pointy ears!
        glColor3f(0.90f, 0.48f, 0.14f);
        for (int side = -1; side <= 1; side += 2) {
            float ex = side * 0.052f;
            glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, 0.4f, 0.9f);
            glVertex3f(ex - 0.028f, 0.052f, 0.005f);
            glVertex3f(ex + 0.028f, 0.052f, 0.005f);
            glVertex3f(ex + side * 0.012f, 0.125f, 0.005f);
            glEnd();
        }
        glPopMatrix();

        glPopMatrix();
        glPopMatrix();
        return;
    }

    // ---------------------------------------------------------
    // WALKING OR SITTING ORANGE TABBY CAT
    // ---------------------------------------------------------
    float body_y     = sitting ? 0.175f : (0.195f + bob_y);
    float body_pitch = sitting ? -26.0f : (tk->state == TURKEY_SCURRY ? -10.0f : 0.0f);

    // 1. Four Slender Animated Legs with White Socks
    for (int side = -1; side <= 1; side += 2) {
        float lx = side * 0.064f;
        // Front legs
        glPushMatrix();
        glTranslatef(lx, 0.155f, 0.115f);
        glRotatef(sitting ? -6.0f : (stride * (float)side * 26.0f), 1.0f, 0.0f, 0.0f);
        glColor3f(0.90f, 0.50f, 0.16f);
        draw_cbox(-0.022f, -0.115f, -0.022f,  0.022f, 0.015f, 0.022f);
        glColor3f(0.96f, 0.94f, 0.88f); // cute white socks!
        draw_cbox(-0.024f, -0.155f, -0.022f,  0.024f, -0.112f, 0.036f);
        glPopMatrix();

        // Back legs
        glPushMatrix();
        if (sitting) {
            // Folded seated haunches
            glTranslatef(lx * 1.15f, 0.085f, -0.105f);
            glColor3f(0.88f, 0.48f, 0.14f);
            draw_gem(0.052f, 0.072f, 0.082f);
            glColor3f(0.96f, 0.94f, 0.88f);
            draw_cbox(-0.026f, -0.085f, -0.020f,  0.026f, -0.052f, 0.075f);
        } else {
            glTranslatef(lx, 0.155f, -0.120f);
            glRotatef(-stride * (float)side * 26.0f, 1.0f, 0.0f, 0.0f);
            glColor3f(0.88f, 0.48f, 0.14f);
            draw_cbox(-0.024f, -0.115f, -0.024f,  0.024f, 0.015f, 0.024f);
            glColor3f(0.96f, 0.94f, 0.88f);
            draw_cbox(-0.026f, -0.155f, -0.024f,  0.026f, -0.112f, 0.036f);
        }
        glPopMatrix();
    }

    // 2. Sleek Ginger Tabby Torso, Stripes & Cream Chest Ruff
    glPushMatrix();
    glTranslatef(0.0f, body_y, 0.0f);
    glRotatef(body_pitch, 1.0f, 0.0f, 0.0f);

    glColor3f(0.92f, 0.52f, 0.16f);
    draw_ellipsoid(0.105f, 0.092f, 0.205f);

    // Cream-white fuzzy chest ruff
    glPushMatrix();
    glTranslatef(0.0f, -0.015f, 0.145f);
    glColor3f(0.96f, 0.94f, 0.88f);
    draw_gem(0.078f, 0.074f, 0.072f);
    glPopMatrix();

    // Marmalade tabby back stripes
    glColor3f(0.68f, 0.32f, 0.08f);
    draw_cbox(-0.078f, 0.060f, -0.105f,  0.078f, 0.088f, -0.070f);
    draw_cbox(-0.084f, 0.068f, -0.020f,  0.084f, 0.096f,  0.015f);
    draw_cbox(-0.076f, 0.060f,  0.065f,  0.076f, 0.088f,  0.098f);

    // 3. Graceful Curving Swishing Tail (with cute white tip!)
    glPushMatrix();
    glTranslatef(0.0f, 0.035f, -0.175f);
    glRotatef(swish, 0.0f, 1.0f, 0.0f);
    glRotatef(-body_pitch + 18.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.90f, 0.50f, 0.15f);
    draw_cbox(-0.020f, 0.0f, -0.020f,  0.020f, 0.150f, 0.020f);
    // Upper tail curl + cream tip
    glTranslatef(0.0f, 0.145f, 0.0f);
    glRotatef(26.0f + swish * 0.4f, 0.0f, 0.0f, 1.0f);
    draw_cbox(-0.018f, 0.0f, -0.018f,  0.018f, 0.105f, 0.018f);
    glColor3f(0.96f, 0.94f, 0.88f);
    draw_cbox(-0.020f, 0.100f, -0.020f,  0.020f, 0.148f, 0.020f);
    glPopMatrix();

    // 4. Cat Head, Pointy Ears, Green Eyes & Pink Nose
    glPushMatrix();
    glTranslatef(0.0f, 0.085f, 0.185f);
    glRotatef(-body_pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(tk->head_turn, 0.0f, 1.0f, 0.0f);

    glColor3f(0.94f, 0.55f, 0.18f);
    draw_gem(0.094f, 0.084f, 0.090f);

    // Cream muzzle & cute pink nose
    glColor3f(0.96f, 0.94f, 0.88f);
    draw_cbox(-0.044f, -0.044f, 0.052f,  0.044f, -0.008f, 0.098f);
    glColor3f(0.96f, 0.52f, 0.60f);
    draw_cbox(-0.016f, -0.014f, 0.092f,  0.016f,  0.008f, 0.108f);

    // Bright emerald green eyes + crisp dark pupils
    for (int side = -1; side <= 1; side += 2) {
        float ex = side * 0.042f;
        glColor3f(0.32f, 0.86f, 0.34f);
        draw_cbox(ex - 0.022f, -0.002f, 0.068f,  ex + 0.022f, 0.034f, 0.084f);
        glColor3f(0.06f, 0.06f, 0.08f);
        draw_cbox(ex - 0.009f,  0.002f, 0.080f,  ex + 0.009f, 0.030f, 0.090f);
    }

    // Two pointy cat ears with pink inner triangles!
    glDisable(GL_CULL_FACE);
    for (int side = -1; side <= 1; side += 2) {
        float ex = side * 0.054f;
        glColor3f(0.90f, 0.48f, 0.14f);
        glBegin(GL_TRIANGLES);
        glNormal3f(0.0f, 0.3f, 0.95f);
        glVertex3f(ex - 0.032f, 0.056f, 0.002f);
        glVertex3f(ex + 0.032f, 0.056f, 0.002f);
        glVertex3f(ex + side * 0.014f, 0.140f, 0.004f);
        glEnd();

        // Cute pink inner ear
        glColor3f(0.96f, 0.62f, 0.66f);
        glBegin(GL_TRIANGLES);
        glNormal3f(0.0f, 0.3f, 0.95f);
        glVertex3f(ex - 0.017f, 0.064f, 0.012f);
        glVertex3f(ex + 0.017f, 0.064f, 0.012f);
        glVertex3f(ex + side * 0.008f, 0.118f, 0.012f);
        glEnd();
    }
    glEnable(GL_CULL_FACE);

    glPopMatrix(); // head
    glPopMatrix(); // body
    glPopMatrix(); // cat root
}

void turkey_render(const Turkey *tk) {
    if (tk->state == TURKEY_HIDDEN || tk->state == TURKEY_ROASTING) return;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_CULL_FACE);

    if (tk->state == TURKEY_ROAST_POP || tk->state == TURKEY_SERVED) {
        render_roasted_turkey(tk);
    } else if (tk->species == VISITOR_FROG) {
        render_frog(tk);
    } else if (tk->species == VISITOR_CAT) {
        render_cat(tk);
    } else {
        render_turkey(tk);
    }

    glEnable(GL_TEXTURE_2D);
}

static void draw_magic_quad(float x, float y, float z, float sz, float rx, float rz) {
    float dx = rx * sz;
    float dz = rz * sz;
    glTexCoord2f(0.0f, 1.0f); glVertex3f(x - dx, y - sz, z - dz);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(x + dx, y - sz, z + dz);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(x + dx, y + sz, z + dz);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x - dx, y + sz, z - dz);
}

void turkey_render_magic_fx(const Turkey *tk, GLuint tex_spark, float cam_x, float cam_z) {
    bool casting = (tk->state == TURKEY_FROG_CAST && tk->cast_elapsed < 0.90f);
    bool burst   = (tk->magic_burst > 0.0f);
    bool napping = (tk->state == TURKEY_CAT_NAP);
    if (!casting && !burst && !napping) return;

    float vx = cam_x;
    float vz = cam_z - 0.34f;
    float len = sqrtf(vx * vx + vz * vz);
    if (len < 0.001f) len = 1.0f;
    float rx = vz / len;
    float rz = -vx / len;

    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindTexture(GL_TEXTURE_2D, tex_spark);

    glBegin(GL_QUADS);

    // 1. Sparkling magic bolt arcing from frog's wand to the left hearth pumpkin (-1.46, 0.30, 0.34)!
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
            if ((i & 1) == 0) {
                glColor4f(1.00f, 0.94f, 0.36f, alpha); // gold star
            } else {
                glColor4f(0.45f, 0.92f, 1.00f, alpha); // fairy cyan star
            }
            draw_magic_quad(px, py, pz, size, rx, rz);
        }
    }

    // 2. Celebratory golden/magenta sparkle poof when the pumpkin turns into a Pumpkin Pie!
    if (burst) {
        float u = 1.0f - (tk->magic_burst / 1.15f); // 0 -> 1
        float alpha = (1.0f - u) * 0.95f;
        for (int i = 0; i < 14; i++) {
            float ang = (float)i * 0.4488f + u * 2.2f;
            float rad = 0.08f + u * 0.36f;
            float px = -1.46f + cosf(ang) * rad;
            float py =  0.26f + u * (0.28f + (i % 3) * 0.14f) - u * u * 0.16f;
            float pz =  0.34f + sinf(ang) * rad;
            float size = (0.062f - 0.025f * u) * (0.75f + 0.25f * (i % 2));
            if (i % 3 == 0)      glColor4f(1.00f, 0.92f, 0.32f, alpha);
            else if (i % 3 == 1) glColor4f(1.00f, 0.56f, 0.88f, alpha);
            else                 glColor4f(0.50f, 0.96f, 1.00f, alpha);
            draw_magic_quad(px, py, pz, size, rx, rz);
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
            glColor4f(0.95f, 0.92f, 1.00f, alpha);
            draw_magic_quad(px, py, pz, size, rx, rz);
        }
    }

    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
}
