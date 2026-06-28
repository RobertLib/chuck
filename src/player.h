#ifndef CHUCK_PLAYER_H
#define CHUCK_PLAYER_H

#include "common.h"
#include "level.h"

typedef struct
{
    bool left;
    bool right;
    bool up;
    bool down;
    bool jump;     /* edge-triggered: set on key press, consumed each frame */
    bool shoot;    /* edge-triggered: set on key press, consumed each frame */
    bool use_door; /* edge-triggered: enter a door (down-key press while on ground) */
} Input;

typedef struct
{
    float x, y; /* top-left of the player box */
    float vx, vy;
    bool on_ground;
    bool on_ladder;
    int facing;   /* -1 = left, +1 = right */
    int bullets;  /* current ammo, 0..MAX_AMMO */
    int grenades; /* current grenades carried (0 or 1) */
    bool dying;   /* true while death animation plays */
    float death_timer;
    bool crawling;    /* true while player is crawling (lower) */
    float anim_timer; /* accumulator for animation timing */
} Player;

void player_reset(Player *player, const Level *level);
void player_update(Player *player, Level *level, const Input *input, float dt);

#endif /* CHUCK_PLAYER_H */
