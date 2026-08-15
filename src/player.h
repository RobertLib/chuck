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
    bool jump;          /* edge-triggered: set on input press, consumed each frame */
    bool jump_held;     /* level-triggered: true while the jump key is down */
    bool shoot;         /* edge-triggered: set on input press, consumed each frame */
    bool use_door;      /* edge-triggered: enter a door while standing in it */
    bool interact;      /* held: operate the active terminal */
    bool confirm;       /* edge-triggered: accept/start/skip */
    bool restart;       /* edge-triggered: replay after an ending */
    bool switch_weapon; /* edge-triggered: select the next usable weapon */
} Input;

typedef enum
{
    PLAYER_WEAPON_PISTOL = 0,
    PLAYER_WEAPON_KNIFE,
    PLAYER_WEAPON_GRENADE,
    PLAYER_WEAPON_BAZOOKA,
    PLAYER_WEAPON_COUNT
} PlayerWeapon;

typedef struct
{
    float x, y; /* top-left of the player box */
    float vx, vy;
    bool on_ground;
    bool on_ladder;
    /* -1/+1 = last vertical aim; 0 = use the remembered horizontal facing. */
    int ladder_direction;
    float ladder_lockout_timer; /* > 0 briefly blocks re-grabbing a ladder */
    bool facade_climbing;       /* dedicated exterior mode; independent of ladders */
    int facing;                 /* -1 = left, +1 = right */
    int hp;                     /* hearts within the current life */
    /* Forgiving jump input. The coyote timer keeps a ledge jumpable for a
     * beat after the boots leave it; the buffer keeps a press alive until the
     * boots arrive. jump_cut_ok marks a rise the player started, so releasing
     * the button can cut a jump without ever cutting a stomp bounce. */
    float coyote_timer;
    float jump_buffer_timer;
    bool jump_cut_ok;
    bool jumped; /* transient: a jump started this frame (shell plays it) */
    int bullets;                /* current ammo, 0..MAX_AMMO */
    int grenades;               /* current grenades carried (0 or 1) */
    int bazooka_rockets;        /* one-shot bazooka carried when non-zero */
    PlayerWeapon active_weapon;
    bool dying; /* true while death animation plays */
    float death_timer;
    bool crawling;         /* true while player is crawling (lower) */
    float anim_time;       /* local visual animation clock */
    float action_timer;    /* short attack/throw follow-through timer */
    bool knife_attacking;  /* current action is a close-range knife swing */
    bool grenade_throwing; /* current action is a grenade throw */
    bool bazooka_firing;   /* current action is the bazooka launch recoil */
    int shot_vertical;     /* -1 = last attack went up, +1 = down, 0 = horizontal */
} Player;

void player_reset(Player *player, const Level *level);
/* Returns the downward speed immediately before collision resolution, so the
 * caller can classify a landing that occurred on any kind of platform. */
float player_update(Player *player, Level *level, const Input *input, float dt);
bool player_weapon_available(const Player *player, PlayerWeapon weapon);
void player_select_next_weapon(Player *player);

#endif /* CHUCK_PLAYER_H */
