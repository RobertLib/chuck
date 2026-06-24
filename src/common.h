#ifndef CHUCK_COMMON_H
#define CHUCK_COMMON_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/* Tile / world geometry */
#define TILE_SIZE 32
#define HUD_HEIGHT 40

#define MAX_LEVEL_WIDTH 64
#define MAX_LEVEL_HEIGHT 48
#define MAX_ITEMS 128
#define MAX_ENEMIES 16

/* Player tuning (pixels per second) */
#define PLAYER_W 22
#define PLAYER_H 28
#define PLAYER_WALK_SPEED 135.0f
#define PLAYER_CLIMB_SPEED 100.0f
#define PLAYER_JUMP_SPEED 365.0f
#define PLAYER_LIVES 3

/* Enemy tuning */
#define ENEMY_W 22
#define ENEMY_H 26
#define ENEMY_WALK_SPEED 62.0f
#define ENEMY_CLIMB_SPEED 60.0f
#define ENEMY_CLIMB_COOLDOWN 1.8f
#define ENEMY_CLIMB_CHANCE 3 /* percent per eligible frame */
#define ENEMY_HP 3

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

#endif /* CHUCK_COMMON_H */

/* Falling platforms */
#define MAX_FALL_PLATFORMS 64
#define FALL_PLATFORM_H 6                 /* pixel height of the falling platform */
#define FALL_PLATFORM_TRIGGER_DELAY 0.25f /* seconds before falling starts after trigger */
#define FALL_PLATFORM_ACCEL 420.0f        /* pixels/sec^2 applied once falling */
