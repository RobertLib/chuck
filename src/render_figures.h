#ifndef CHUCK_RENDER_FIGURES_H
#define CHUCK_RENDER_FIGURES_H

/*
 * Everybody the player sees moving, and nothing else.
 *
 * Chuck, the guards and their bodies, the dogs and theirs, the janitor, the
 * fleeing civilians, the receptionist, and the two things thrown at a climber.
 * They are one module because they are one drawing problem: every one of them
 * is built from the tapered, lit forms in
 * [render_sprite.h](render_sprite.h), every one of them puts its legs a long
 * way under its torso so a thirty-two pixel body has a mass the eye can land
 * on, and every one of them casts its shadow on the floor rather than under
 * its own boots. A figure added here inherits all three; a figure added
 * anywhere else would not, and would look like a figure from a different
 * game.
 *
 * They came out of game_render.c, which was six thousand lines and the one
 * file in the tree with no test coverage at all — `make test` links no SDL and
 * so reaches none of it. What holds this side of the split is the shared
 * vocabulary itself: one name per rule, and `make lint` refusing any literal
 * that respells one of them.
 */

#include <SDL3/SDL.h>

#include "enemy.h"
#include "gameplay_state.h"
#include "level.h"
#include "player.h"

/*
 * A grenade, drawn where it is: in a hand mid-throw, and lying on the floor
 * waiting to be picked up. One drawing serves both, and it lives on this side
 * because the throwing pose is what decides how it has to look.
 */
void draw_grenade(SDL_Renderer *r, float x, float y, float fuse);
void draw_decoy(SDL_Renderer *r, float x, float y);
void draw_flashbang(SDL_Renderer *r, float x, float y, float fuse);
void draw_evidence_pickup(SDL_Renderer *r, float x, float y);

void draw_player(SDL_Renderer *r, const Player *p, const Level *level,
                 float cam_x, float oy, bool hacking, float hacking_time,
                 float land_squash);

void draw_enemy(SDL_Renderer *r, const Enemy *e, const Level *level,
                float cam_x, float oy);
void draw_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
              float cam_x, float oy);

/*
 * The fallen, laid along the floor: dead visor, no health pips, no speech
 * bubble. They are drawn because the AI already reads them — a calm guard who
 * sees a body goes to look and often on to the nearest alarm switch — and a
 * rule that is simulated, documented and punishing must not have an invisible
 * trigger.
 */
void draw_downed_enemy(SDL_Renderer *r, const Enemy *e, const Level *level,
                       float cam_x, float oy);
void draw_downed_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
                     float cam_x, float oy);

/* The three who take no part in the fight: no perception, no damage, no
 * collision against the player, no scoring. */
void draw_janitor(SDL_Renderer *r, const Janitor *janitor, const Level *level,
                  float cam_x, float oy);
void draw_civilian(SDL_Renderer *r, const Civilian *civilian,
                   const Level *level, float cam_x, float oy);
void draw_receptionist(SDL_Renderer *r, const Receptionist *rec,
                       const Level *level, float cam_x, float oy);

/* What comes off the wall at a climber. */
void draw_thrown_object(SDL_Renderer *r, const ThrownObject *object,
                        float cam_x, float oy);
void draw_bird(SDL_Renderer *r, const Bird *bird, float cam_x, float oy);

#endif /* CHUCK_RENDER_FIGURES_H */
