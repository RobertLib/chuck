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
#define TERMINAL_REINFORCEMENT_MIN_COUNT 1
#define TERMINAL_REINFORCEMENT_MAX_COUNT 2
#define TERMINAL_REINFORCEMENT_FIRST_MIN 0.65f
#define TERMINAL_REINFORCEMENT_FIRST_MAX 2.40f
#define TERMINAL_REINFORCEMENT_GAP_MIN 1.40f
#define TERMINAL_REINFORCEMENT_GAP_MAX 3.50f

/* Building-wide security alarm. Guards that choose not to engage immediately
 * run to one of these wall switches. The countdown is refreshed whenever a
 * guard or dog can still see Chuck, so it only expires after the scene has
 * actually been quiet for a while. */
#define MAX_ALARM_SWITCHES 16
#define ALARM_CALM_TIME 9.0f
/* Backwards-compatible name for code treating terminal noise as the alarm. */
#define TERMINAL_ALARM_TIME ALARM_CALM_TIME
#define ALARM_SWITCH_USE_TIME 0.65f
#define ALARM_SWITCH_USE_RANGE 18.0f
#define ALARM_SWITCH_STAND_DISTANCE 14.0f
#define ALARM_SIREN_INTERVAL 1.15f
#define GUARD_ALARM_CHANCE 45
#define GUARD_ENCOUNTER_RESET_TIME 2.5f

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
#define ENEMY_OBSTACLE_AVOID_TIME 1.25f
#define ENEMY_HP 3

/* Perception. Guards no longer see only along their exact floor row: they have
 * a forward vision cone (wide field of view, diagonal sight up and down) with a
 * ray-cast line of sight that walls, floors, and crates block. A short
 * peripheral radius lets a guard notice something right next to or behind it. */
#define ENEMY_VIEW_RANGE (7 * TILE_SIZE)
/* cos of the cone half-angle. 0.34 => ~70deg half-angle (~140deg total FOV). */
#define ENEMY_VIEW_CONE_COS 0.34f
#define ENEMY_PERIPHERAL_RANGE (1.6f * TILE_SIZE)
/* A crawling player is stealthier: spotted only at closer range. */
#define ENEMY_CRAWL_VIEW_FACTOR 0.55f
#define ENEMY_LOS_STEP 6.0f

/* Hearing: gunfire and explosions draw nearby guards to investigate. */
#define ENEMY_HEAR_RADIUS_SHOT (7.0f * TILE_SIZE)
#define ENEMY_HEAR_RADIUS_BLAST (11.0f * TILE_SIZE)

/* Suspicion / investigation: a soft alert short of a full building alarm. A
 * guard walks warily to the disturbance, scans, then resumes its patrol. */
#define ENEMY_INVESTIGATE_TIME 5.0f
#define ENEMY_INVESTIGATE_LOOK_TIME 1.2f
#define ENEMY_INVESTIGATE_REACH 20.0f
#define ENEMY_INVESTIGATE_SCAN_FLIP 0.55f
/* Discovering a fallen comrade is a strong signal to raise the alarm. */
#define ENEMY_BODY_NOTICE_RANGE (3.0f * TILE_SIZE)
#define GUARD_BODY_ALARM_CHANCE 65

/* Combat tactics */
#define ENEMY_AIM_LEAD 0.26f /* seconds of target velocity to lead a shot */
#define ENEMY_VERTICAL_SHOOT_RANGE (4 * TILE_SIZE)
#define ENEMY_VERTICAL_SHOOT_HALF_W (TILE_SIZE * 0.55f)
/* Posted-up guards hold and fire from range instead of crowding into melee. */
#define ENEMY_KEEP_DISTANCE (3.2f * TILE_SIZE)

/* Pursuit movement: guards hop small gaps while chasing (patrol is unchanged). */
#define ENEMY_JUMP_SPEED 300.0f
#define ENEMY_JUMP_MIN_SPEED 120.0f
#define ENEMY_JUMP_MAX_GAP_TILES 2
/* A search party fans out around the last sighting instead of clustering. */
#define ENEMY_SEARCH_FAN 1.5f
#define ENEMY_ALARM_SPEED_MULTIPLIER 1.28f
#define ENEMY_ALARM_AIM_MULTIPLIER 0.62f
#define ENEMY_ALARM_COOLDOWN_MULTIPLIER 0.55f
#define ENEMY_ALARM_INITIAL_SHOT_DELAY 0.55f
#define ENEMY_ALARM_SEARCH_RADIUS (2.0f * TILE_SIZE)
#define ENEMY_ALARM_SEARCH_NEAR_RADIUS (2.4f * TILE_SIZE)

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
#define DOG_STEP_DOWN_MAX_TILES 2
#define DOG_TURN_COOLDOWN 0.4f
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
