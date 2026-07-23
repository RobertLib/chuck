#ifndef CHUCK_PLAYER_H
#define CHUCK_PLAYER_H

#include "game_config.h"
#include "level.h"

#include <stdbool.h>

typedef struct
{
    bool left;
    bool right;
    bool up;
    bool down;
    bool jump;     /* edge-triggered: set on key press, consumed each frame */
    bool shoot;    /* edge-triggered: set on key press, consumed each frame */
    bool use_door; /* edge-triggered: enter a door (E press while on ground) */
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
    int bazooka_rockets; /* one-shot bazooka carried when non-zero */
    bool dying;   /* true while death animation plays */
    float death_timer;
    bool crawling; /* true while player is crawling (lower) */
    float anim_time;    /* local visual animation clock */
    float action_timer; /* short attack/throw follow-through timer */
    bool knife_attacking; /* current action is a close-range knife swing */
    bool bazooka_firing; /* current action is the bazooka launch recoil */
    int shot_vertical;  /* -1 = last shot went up, +1 = down, 0 = horizontal */
} Player;

void player_reset(Player *player, const Level *level);
void player_update(Player *player, Level *level, const Input *input, float dt);

#endif /* CHUCK_PLAYER_H */
