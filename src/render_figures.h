#ifndef CHUCK_RENDER_FIGURES_H
#define CHUCK_RENDER_FIGURES_H

/*
 * Everybody the player sees moving, and nothing else.
 *
 * Chuck, the guards and their bodies, the dogs and theirs, the janitor, the
 * fleeing civilians, the receptionist, and the two things thrown at a climber.
 * They are one module because they are one drawing problem: every one of them
 * is a MODE 2 sprite out of [bbc_cast.h](bbc_cast.h), stood on the floor of
 * its own collision box, and every one of them says what it is doing by which
 * frame it is on rather than by a limb that moves — a guard bringing his rifle
 * up, a dog dropping into its crouch, a man on a rung reaching out sideways.
 * What is laid over a figure (the pistol in a fist, a muzzle flash, a fuse, a
 * health pip) is a whole BBC pixel at a whole cell of that figure, so it moves
 * with him and mirrors with him. A figure added here inherits all of that; a
 * figure added anywhere else would look like it came from a different game.
 *
 * This is the shell side of the SDL boundary: `make test` reaches none of it.
 * What holds it is the cast itself, which is SDL-free and which the suite
 * walks sprite by sprite.
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
