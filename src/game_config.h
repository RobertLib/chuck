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
#define PLAYER_CONTINUES 3
#define CONTINUE_COUNTDOWN_TIME 10.0f
#define GAME_OVER_DISPLAY_TIME 3.0f
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
#define ENEMY_JUMP_MIN_SPEED 190.0f
#define ENEMY_JUMP_MAX_GAP_TILES 2
#define ENEMY_ELEVATOR_FLOOR_TOLERANCE 8.0f
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

/* Exterior facade climb hazards. Map-authored sources wake only while the
 * player is nearby, keeping tall levels active around the visible route. */
#define MAX_FACADE_HAZARD_SPAWNS 32
#define MAX_THROWN_OBJECTS 12
#define MAX_BIRDS 12
#define THROWN_OBJECT_SIZE 14.0f
#define THROWN_OBJECT_SPEED 205.0f
#define THROWN_OBJECT_GRAVITY 410.0f
#define THROWN_OBJECT_SPAWN_MIN 1.8f
#define THROWN_OBJECT_SPAWN_MAX 3.4f
/* A source shouts and leans out before it lets go, so every throw can be
 * answered by moving behind a ledge instead of by memorising the map. */
#define THROWN_OBJECT_WINDUP 0.60f
#define BIRD_W 26.0f
#define BIRD_H 12.0f
#define BIRD_SPEED 155.0f
#define BIRD_SPAWN_MIN 2.4f
#define BIRD_SPAWN_MAX 4.6f
/* Birds swoop rather than fly on rails; the wave is derived from their own
 * animation clock so the motion stays deterministic. */
#define BIRD_WAVE_SPEED 190.0f
#define BIRD_WAVE_RATE 3.6f
#define FACADE_HAZARD_WAKE_RANGE (10.0f * TILE_SIZE)
#define FACADE_CLIMB_SPEED 112.0f
#define FACADE_BUILDING_SIDE_INSET 80.0f
#define FACADE_CLIMB_SIDE_MARGIN FACADE_BUILDING_SIDE_INSET

/* Wind. Gusts announce themselves for a beat, then shove the climber sideways
 * along the wall unless a ledge or air-conditioning unit upwind of him breaks
 * the gust. Standing still in the open during a gust is never fatal, but it
 * costs the route, which is what makes the shelters worth using. */
#define FACADE_WIND_WARN_TIME 1.15f
#define FACADE_WIND_GUST_TIME 2.30f
#define FACADE_WIND_CALM_MIN 4.20f
#define FACADE_WIND_CALM_MAX 7.60f
#define FACADE_WIND_PUSH 82.0f
/* How far upwind a solid tile still counts as cover. */
#define FACADE_WIND_SHELTER_REACH 42.0f

/* Climb checkpoints. A tall wall would be miserable if one brick cost the
 * whole ascent, so height already earned is banked every few floors and a
 * lost life resumes from there. */
#define FACADE_CHECKPOINT_STEP (3.0f * TILE_SIZE)

/* Prologue car chase. A top-down, forward-only pursuit played once before the
 * campaign: Chuck tails the kidnappers' SUV through night traffic until it
 * reaches the building where the platformer starts. Road space is measured in
 * pixels, x across the road and y along the driving direction (y grows
 * forward), so the simulation needs no separate world scale. */
#define CHASE_ROAD_WIDTH 480.0f
#define CHASE_LANE_COUNT 4
#define CHASE_LANE_WIDTH (CHASE_ROAD_WIDTH / (float)CHASE_LANE_COUNT)
/* Lanes 0..1 carry oncoming traffic, 2..3 run with the pursuit. */
#define CHASE_FIRST_FORWARD_LANE 2
#define CHASE_KERB_MARGIN 5.0f
#define CHASE_CAR_LENGTH 84.0f
#define CHASE_CAR_WIDTH 46.0f
#define CHASE_SUV_LENGTH 98.0f
#define CHASE_SUV_WIDTH 54.0f
#define CHASE_MAX_CARS 24
#define CHASE_MAX_INTERSECTIONS 4
#define CHASE_BLOCK_LENGTH 1000.0f
#define CHASE_JUNCTION_HALF 92.0f
#define CHASE_SPAWN_MARGIN 760.0f
#define CHASE_CULL_MARGIN 280.0f
/* Never more than this many cars abreast, so at least two lanes stay open. */
#define CHASE_MAX_CARS_ABREAST 2

/* Player car handling. Coasting matches the SUV's own speed, so holding a lane
 * keeps the trail while every dodge and crash has to be paid back on the
 * accelerator. */
#define CHASE_CRUISE_SPEED 320.0f
#define CHASE_MAX_SPEED 470.0f
#define CHASE_MIN_SPEED 140.0f
#define CHASE_ACCEL 235.0f
#define CHASE_BRAKE 330.0f
#define CHASE_COAST 130.0f
#define CHASE_STEER_SPEED 250.0f
#define CHASE_CRASH_SPEED 150.0f
#define CHASE_SCRAPE_DRAG 150.0f
#define CHASE_SCRAPE_SOUND_INTERVAL 0.34f
#define CHASE_INTEGRITY 3
#define CHASE_HIT_INVULN 1.1f
#define CHASE_NEAR_MISS_SIDE 62.0f
#define CHASE_NEAR_MISS_AHEAD 130.0f
#define CHASE_HORN_INTERVAL 1.6f
#define CHASE_ENGINE_INTERVAL 1.05f

/* Traffic */
#define CHASE_TRAFFIC_SPEED_MIN 150.0f
#define CHASE_TRAFFIC_SPEED_MAX 215.0f
#define CHASE_ONCOMING_SPEED_MIN 150.0f
#define CHASE_ONCOMING_SPEED_MAX 205.0f
#define CHASE_CROSS_SPEED_MIN 195.0f
#define CHASE_CROSS_SPEED_MAX 265.0f
#define CHASE_CROSS_GAP_MIN 0.85f
#define CHASE_CROSS_GAP_MAX 1.40f
#define CHASE_CROSS_LANE_OFFSET 42.0f
#define CHASE_CROSS_ALERT_RANGE 900.0f
#define CHASE_SIGNAL_PERIOD 7.4f
#define CHASE_SIGNAL_CROSS_GREEN 3.1f
#define CHASE_WRECK_DRIFT 95.0f

/* The hunted SUV */
#define CHASE_TARGET_SPEED 300.0f
#define CHASE_TARGET_SPEED_SWING 26.0f
#define CHASE_TARGET_BOOST 65.0f
#define CHASE_TARGET_BOOST_TIME 1.6f
#define CHASE_TARGET_STEER_SPEED 205.0f
#define CHASE_TARGET_LANE_TIME_MIN 2.2f
#define CHASE_TARGET_LANE_TIME_MAX 4.8f
#define CHASE_TARGET_LOOKAHEAD 210.0f
#define CHASE_START_GAP 380.0f
#define CHASE_MIN_GAP 190.0f
#define CHASE_LOSE_GAP 1050.0f

/* The opening beat, cue by cue: the kidnappers slam a door and pull away while
 * Chuck runs up the pavement, unlocks his car and pulls out after them. The
 * renderer stages Chuck's run from the same timings the simulation uses. */
#define CHASE_KERB_X \
    (CHASE_ROAD_WIDTH - CHASE_CAR_WIDTH * 0.5f - CHASE_KERB_MARGIN)
#define CHASE_DEPARTURE_TARGET_OFFSET 260.0f
/* They leave at ordinary traffic speed: they are not being chased yet, which is
 * what lets Chuck close the distance after his late start. */
#define CHASE_DEPARTURE_TARGET_SPEED 230.0f
#define CHASE_DEPARTURE_TARGET_ACCEL 190.0f
#define CHASE_DEPARTURE_SUV_DOOR 1.20f
#define CHASE_DEPARTURE_SUV_START 1.40f
#define CHASE_DEPARTURE_CHUCK_RUN 1.60f
#define CHASE_DEPARTURE_CAR_DOOR 3.05f
#define CHASE_DEPARTURE_IGNITION 3.30f
#define CHASE_DEPARTURE_PULL_OUT 3.45f

/* Arrival geometry, measured back from the building's front face. */
#define CHASE_ARRIVAL_PLAYER_STOP 310.0f
#define CHASE_ARRIVAL_TARGET_STOP 130.0f
#define CHASE_ARRIVAL_CAMERA_LEAD 70.0f

/* Camera and pacing. The camera keeps the player's car near the bottom of the
 * view so most of the frame shows the road being driven into. */
#define CHASE_CAMERA_LEAD 135.0f
/* The opening and closing beats sit further back so the SUV can be watched
 * driving away, and so the destination fits in frame. */
#define CHASE_DEPARTURE_CAMERA_LEAD 110.0f
#define CHASE_PAVEMENT_WIDTH 26.0f
#define CHASE_DEPARTURE_DURATION 6.0f
#define CHASE_PURSUIT_DURATION 40.0f
#define CHASE_FAILED_DURATION 2.4f
#define CHASE_ARRIVAL_DURATION 5.4f
/* Both cars are on their marks before the beat ends. */
#define CHASE_ARRIVAL_BRAKE_TIME 4.2f
#define CHASE_ARRIVAL_DISTANCE 1500.0f

#endif /* CHUCK_GAME_CONFIG_H */
