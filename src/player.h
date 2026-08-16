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
    bool jump;      /* edge-triggered: set on input press, consumed each frame */
    bool jump_held; /* level-triggered: true while the jump key is down */
    /* The two pedals of the prologue drive. A car is not a platformer figure:
     * nobody reaches for the d-pad to make one go, so the throttle and the
     * brake are the letters under the thumbs — A and B — and the sticks and
     * arrows that already meant "up" and "down" keep working alongside them.
     * They are their own inputs rather than aliases of up/down so that binding
     * a face button to the accelerator cannot quietly make it climb a ladder. */
    bool gas;   /* held: accelerate in the prologue drive */
    bool brake; /* held: slow down in the prologue drive */
    bool shoot;         /* edge-triggered: set on input press, consumed each frame */
    bool use_door;      /* edge-triggered: enter a door while standing in it */
    bool interact;      /* held: operate the active terminal */
    bool confirm;       /* edge-triggered: accept/start/skip */
    /* Held, and read by exactly one screen: the title screen's quit chip. B is
     * the button a pad reaches for to back out of things, so a *press* of it
     * must never end the session — but a pad in fullscreen has no ESC and no
     * close box, and the hold is what turns the one button it can spare into a
     * deliberate act rather than a reflex. Nothing in a sector reads it. */
    bool cancel_held;
    bool restart;            /* edge-triggered: replay after an ending */
    bool switch_weapon;      /* edge-triggered: select the next usable weapon */
    bool switch_weapon_back; /* edge-triggered: and the one before it */
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
    /*
     * Grenades carried. A count, and spent one at a time like every other
     * weapon — it used to be *cleared* on the throw, which is the same thing
     * for the campaign and not for anybody carrying two.
     *
     * The campaign never hands over more than one: `item_would_be_wasted`
     * refuses an `N` to a player already holding one, so the pickup cannot
     * raise it and `player_begin_sector` cannot carry more than it was given.
     * The demo hand does hold two, deliberately — a grenade is thrown once on
     * the floor and once from a rung, and the vertical throw is its own drawing
     * (`render_figures.c`) rather than the horizontal one rotated. Clearing the
     * count on the first throw is why that drawing was reached by nothing at
     * all, in the file written to reach it.
     */
    int grenades;
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
/* Opening a sector: a reset, plus whatever the sector below is allowed to hand
 * over. `previous` is the man who walked out of it, or NULL when nobody did —
 * the first sector of a run, a retry after a continue, or an authored jump
 * straight into a map. See player.c for why only the explosives travel. */
void player_begin_sector(Player *player, const Level *level,
                         const Player *previous);
/* Returns the downward speed immediately before collision resolution, so the
 * caller can classify a landing that occurred on any kind of platform. */
float player_update(Player *player, Level *level, const Input *input, float dt);
bool player_weapon_available(const Player *player, PlayerWeapon weapon);
void player_select_next_weapon(Player *player);
/* The same cycle walked the other way, for the left bumper. */
void player_select_prev_weapon(Player *player);
/* Where a spent weapon hands the frame back to: the sidearm if it is loaded,
 * otherwise the knife. Never another explosive — see player.c. */
void player_fall_back_to_sidearm(Player *player);

#endif /* CHUCK_PLAYER_H */
