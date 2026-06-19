#ifndef CHUCK_COMMON_H
#define CHUCK_COMMON_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/* Tile / world geometry */
#define TILE_SIZE 32
#define HUD_HEIGHT 40

#define MAX_LEVEL_WIDTH 128
#define MAX_LEVEL_HEIGHT 48
#define MAX_ITEMS 128
#define MAX_ENEMIES 16
#define MAX_DOGS 12
/* Seconds before ammo/grenade pickups reappear after being collected */
#define ITEM_RESPAWN_TIME 10.0f

/* Mines */
#define MAX_MINES 32
#define MINE_W 16
#define MINE_H 10
#define MINE_TRIGGER_DELAY 0.45f /* seconds between stepping and explosion */
#define MINE_DAMAGE 1
#define MINE_RADIUS (36.0f) /* damage radius in pixels */

/* Pushable/destructible crates */
#define MAX_CRATES 64
#define CRATE_W 28
#define CRATE_H 28
#define CRATE_PUSH_SPEED 95.0f
#define CRATE_FRICTION 8.0f

/* Grenades */
#define MAX_GRENADES 8
#define GRENADE_W 10
#define GRENADE_H 10
#define GRENADE_FUSE_TIME 1.4f     /* seconds until detonation after thrown */
#define GRENADE_RADIUS (48.0f)     /* explosion radius in pixels */
#define GRENADE_THROW_SPEED 260.0f /* initial horizontal speed when thrown */

/* Player tuning (pixels per second) */
#define PLAYER_W 26
#define PLAYER_H 32
#define PLAYER_WALK_SPEED 135.0f
#define PLAYER_CLIMB_SPEED 100.0f
#define PLAYER_JUMP_SPEED 365.0f
#define PLAYER_LIVES 3
/* Maximum allowed lives (medkits cannot raise above this). */
#define MAX_LIVES 9

/* Crawling (duck) tuning */
#define PLAYER_CRAWL_H 18
#define PLAYER_CRAWL_SPEED 60.0f

/* Enemy tuning */
#define ENEMY_W 26
#define ENEMY_H 32
#define ENEMY_WALK_SPEED 62.0f
#define ENEMY_CLIMB_SPEED 60.0f
#define ENEMY_CLIMB_COOLDOWN 1.8f
#define ENEMY_CLIMB_CHANCE 3 /* percent per eligible frame */
#define ENEMY_HP 3

/* Guard dog tuning */
#define DOG_W 24
#define DOG_H 16
#define DOG_HP 1
#define DOG_PATROL_SPEED 90.0f
#define DOG_RETURN_SPEED 135.0f
#define DOG_CHASE_SPEED 185.0f
#define DOG_JUMP_SPEED 285.0f
#define DOG_JUMP_MIN_SPEED 150.0f
#define DOG_JUMP_MAX_GAP_TILES 2
#define DOG_HANDLER_DISTANCE 34.0f
#define DOG_ROAM_RADIUS 90.0f
#define DOG_RETURN_RADIUS 130.0f
#define DOG_VIEW_RANGE (6 * TILE_SIZE)
#define DOG_BACK_SENSE_RANGE (2 * TILE_SIZE)
#define DOG_BITE_RANGE 16.0f
#define DOG_BITE_COOLDOWN 0.75f
#define DOG_LOST_TIME 2.0f
#define DOG_DOOR_HANDLER_CHANCE 30 /* percent chance a door-spawned enemy has a dog */

/* Reaction: when player is nearby and enemy is moving away while player is
 * not crawling, there's a per-frame chance the enemy will turn and attack. */
#define ENEMY_RETALIATE_RADIUS (3 * TILE_SIZE) /* px horizontal radius */
#define ENEMY_RETALIATE_CHANCE 8               /* percent per eligible frame */

/* Enemy conversation: when two enemies meet they talk for a short time and
 * will ignore the player unless the player comes very close. */
#define ENEMY_TALK_DURATION 4.0f                    /* seconds enemies chat when meeting */
#define ENEMY_TALK_NOTICE_RADIUS (TILE_SIZE * 1.5f) /* px player must be within to be noticed while talking */
/* Chance (percent) that two nearby enemies will stop and talk when they meet. */
#define ENEMY_TALK_CHANCE 30
/* Cooldown after a conversation finishes during which the enemy won't start
 * another conversation. */
#define ENEMY_TALK_COOLDOWN 5.0f

/* Bullet tuning */
#define MAX_BULLETS 8
#define BULLET_SPEED 600.0f
#define BULLET_W 8
#define BULLET_H 4
#define MAX_AMMO 3

/* Enemy shooting */
#define MAX_ENEMY_BULLETS 16
#define ENEMY_BULLET_SPEED 380.0f
#define ENEMY_SHOOT_RANGE (7 * TILE_SIZE) /* max horizontal range in pixels */
#define ENEMY_SHOOT_COOLDOWN 2.5f         /* base seconds between shots */
#define ENEMY_AIM_TIME 0.45f              /* seconds enemy stands still before firing */
/* Keep the adaptive muzzle height within the guard's torso. */
#define ENEMY_MUZZLE_MIN_Y_FACTOR 0.30f
#define ENEMY_MUZZLE_MAX_Y_FACTOR 0.70f

/* Enemy speed factors when damaged (multiplied by ENEMY_WALK_SPEED) */
#define ENEMY_SPEED_HP2 0.60f /* one hit taken */
#define ENEMY_SPEED_HP1 0.30f /* two hits taken */

/* Shared physics */
#define GRAVITY 980.0f
#define MAX_FALL_SPEED 620.0f

/* Time the player is invulnerable after being hit (seconds) */
#define INVULN_TIME 1.5f

/* Doors */
#define MAX_DOORS 8
#define DOOR_SPAWN_INTERVAL 5.0f /* seconds between enemy spawns from a door */
#define TELEPORT_COOLDOWN 0.3f   /* seconds before the player can teleport again */

/* Elevators */
#define MAX_ELEVATORS 8
#define ELEVATOR_SPEED 72.0f /* pixels per second */
#define ELEVATOR_PLAT_H 6    /* pixel height of the moving platform */

/* Falling platforms */
#define MAX_FALL_PLATFORMS 64
#define FALL_PLATFORM_H 6                 /* pixel height of the falling platform */
#define FALL_PLATFORM_TRIGGER_DELAY 0.25f /* seconds before falling starts after trigger */
#define FALL_PLATFORM_ACCEL 420.0f        /* pixels/sec^2 applied once falling */

/* Moving horizontal platforms */
#define MAX_MOVING_PLATFORMS 64
#define MOVING_PLATFORM_H 6         /* pixel height (visual) */
#define MOVING_PLATFORM_SPEED 72.0f /* default horizontal speed (px/s) */

/* Spikes (hazard tiles) */
#define MAX_SPIKES 128
#define SPIKE_W TILE_SIZE
#define SPIKE_H TILE_SIZE

#endif /* CHUCK_COMMON_H */
