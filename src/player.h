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
    bool interact; /* held: operate the active terminal */
    bool confirm;  /* edge-triggered: accept/start (space, enter, or a menu click) */
    bool restart;  /* edge-triggered: replay after an ending */
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
    bool crawling; /* true while player is crawling (lower) */
    float anim_time;    /* local visual animation clock */
    float action_timer; /* short recoil/throw follow-through timer */
    int shot_vertical;  /* -1 = last shot went up, +1 = down, 0 = horizontal */
} Player;

void player_reset(Player *player, const Level *level);
void player_update(Player *player, Level *level, const Input *input, float dt);

#endif /* CHUCK_PLAYER_H */
