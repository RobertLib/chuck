#ifndef CHUCK_GAME_CONFIG_H
#define CHUCK_GAME_CONFIG_H

/* Tile / world geometry */
#define TILE_SIZE 32
#define HUD_HEIGHT 40

#define MAX_LEVEL_WIDTH 128
#define MAX_LEVEL_HEIGHT 48
#define MAX_ITEMS 128
#define MAX_ENEMIES 16
#define MAX_DOGS 12
#define ITEM_RESPAWN_TIME 10.0f

#define MAX_DECORATIONS 192

/* Ambient janitors are visual-only NPCs. They collide with the static level
 * geometry so their patrols stay grounded, but never participate in combat,
 * player collision, pickups, alarms, or scoring. */
#define MAX_JANITORS 8
#define JANITOR_W 26
#define JANITOR_H 32
#define JANITOR_WALK_SPEED 34.0f
#define JANITOR_WET_SPOTS 6
#define JANITOR_WET_LIFETIME 7.0f

/* Mines */
#define MAX_MINES 32
#define MINE_W 16
#define MINE_H 10
#define MINE_TRIGGER_DELAY 0.45f
#define MINE_DAMAGE 1
#define MINE_RADIUS 36.0f

/* Small floor-level gas canisters. Their low profile deliberately puts them
 * below a standing player's firing line, so Chuck must crawl to shoot them. */
#define MAX_GAS_CANISTERS 64
#define GAS_CANISTER_W 12
#define GAS_CANISTER_H 16
#define GAS_CANISTER_RADIUS 56.0f

/* Pushable/destructible crates */
#define MAX_CRATES 64
#define CRATE_W 28
#define CRATE_H 28
#define CRATE_PUSH_SPEED 95.0f
#define CRATE_FRICTION 8.0f
#define CRATE_LAND_SOUND_SPEED 90.0f

/* Exit-access terminals */
#define MAX_TERMINALS 16
#define TERMINAL_HACK_TIME 4.0f
#define TERMINAL_INTERACT_RANGE 44.0f
#define TERMINAL_MIN_START_TILES 12
#define TERMINAL_ALARM_TIME 6.0f
#define TERMINAL_REINFORCEMENT_MIN_COUNT 1
#define TERMINAL_REINFORCEMENT_MAX_COUNT 2
#define TERMINAL_REINFORCEMENT_FIRST_MIN 0.65f
#define TERMINAL_REINFORCEMENT_FIRST_MAX 2.40f
#define TERMINAL_REINFORCEMENT_GAP_MIN 1.40f
#define TERMINAL_REINFORCEMENT_GAP_MAX 3.50f

/* Grenades */
#define MAX_GRENADES 8
#define GRENADE_W 10
#define GRENADE_H 10
#define GRENADE_FUSE_TIME 1.4f
#define GRENADE_RADIUS 48.0f
#define GRENADE_THROW_SPEED 260.0f

/* Bazooka: every pickup contains one rocket. The slow, large projectile
 * detonates on the first solid surface or target it reaches. */
#define MAX_ROCKETS 1
#define BAZOOKA_AMMO 1
#define ROCKET_W 16
#define ROCKET_H 6
#define ROCKET_SPEED 460.0f
#define ROCKET_RADIUS 72.0f
#define ROCKET_ACTION_TIME 0.24f

/* Player tuning */
#define PLAYER_W 26
#define PLAYER_H 32
#define PLAYER_WALK_SPEED 135.0f
#define PLAYER_CLIMB_SPEED 100.0f
#define PLAYER_JUMP_SPEED 365.0f
#define PLAYER_LIVES 3
#define MAX_LIVES 9
#define PLAYER_CRAWL_H 18
#define PLAYER_CRAWL_SPEED 60.0f
#define PLAYER_KNIFE_RANGE 18.0f
#define PLAYER_KNIFE_ACTION_TIME 0.18f

/* Enemy tuning */
#define ENEMY_W 26
#define ENEMY_H 32
#define ENEMY_WALK_SPEED 62.0f
#define ENEMY_CLIMB_SPEED 60.0f
#define ENEMY_CLIMB_COOLDOWN 1.8f
#define ENEMY_CLIMB_CHANCE 3
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
#define DOG_DOOR_HANDLER_CHANCE 30

#define ENEMY_RETALIATE_RADIUS (3 * TILE_SIZE)
#define ENEMY_RETALIATE_CHANCE 8
#define ENEMY_TALK_DURATION 4.0f
#define ENEMY_TALK_NOTICE_RADIUS (TILE_SIZE * 1.5f)
#define ENEMY_TALK_CHANCE 30
#define ENEMY_TALK_COOLDOWN 5.0f

/* Projectiles */
#define MAX_BULLETS 8
#define BULLET_SPEED 600.0f
#define BULLET_W 8
#define BULLET_H 4
#define MAX_AMMO 6
#define MAX_ENEMY_BULLETS 16
#define ENEMY_BULLET_SPEED 380.0f
#define ENEMY_SHOOT_RANGE (7 * TILE_SIZE)
#define ENEMY_SHOOT_COOLDOWN 2.5f
#define ENEMY_AIM_TIME 0.45f
#define ENEMY_MUZZLE_MIN_Y_FACTOR 0.30f
#define ENEMY_MUZZLE_MAX_Y_FACTOR 0.70f

#define ENEMY_SPEED_HP2 0.60f
#define ENEMY_SPEED_HP1 0.30f

/* Shared physics */
#define GRAVITY 980.0f
#define MAX_FALL_SPEED 620.0f
#define INVULN_TIME 1.5f

/* Doors */
#define MAX_DOORS 8
#define DOOR_SPAWN_INTERVAL 5.0f
#define TELEPORT_COOLDOWN 0.3f

/* Platforms */
#define MAX_ELEVATORS 8
#define ELEVATOR_SPEED 72.0f
#define ELEVATOR_PLAT_H 6
#define MAX_FALL_PLATFORMS 64
#define FALL_PLATFORM_H 6
#define FALL_PLATFORM_TRIGGER_DELAY 0.25f
#define FALL_PLATFORM_ACCEL 420.0f
#define MAX_MOVING_PLATFORMS 64
#define MOVING_PLATFORM_H 6
#define MOVING_PLATFORM_SPEED 72.0f

/* Hazards */
#define MAX_SPIKES 128
#define SPIKE_W TILE_SIZE
#define SPIKE_H TILE_SIZE
#define MAX_CEILING_FANS 64
#define CEILING_FAN_BLADE_LENGTH 23.0f
#define CEILING_FAN_HIT_HEIGHT 8.0f
#define CEILING_FAN_CENTER_Y 10.0f

#endif /* CHUCK_GAME_CONFIG_H */
