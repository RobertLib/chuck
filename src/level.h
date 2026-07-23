#ifndef CHUCK_LEVEL_H
#define CHUCK_LEVEL_H

#include "game_config.h"
#include "rng.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    TILE_EMPTY = 0,
    TILE_WALL,
    TILE_LADDER,
    TILE_DOOR,
    TILE_ELEVATOR_SHAFT /* visual only – not solid */,
    TILE_FALL_PLATFORM /* single-tile one-way platform that can fall */
} TileType;

/* A moving elevator platform within a vertical shaft. */
typedef struct
{
    int col;         /* tile column of the shaft */
    float y;         /* current top of the moving platform (world px) */
    float top_limit; /* minimum y (top of shaft) */
    float bot_limit; /* maximum y the platform top may reach */
    float vy;        /* current vertical velocity (positive = downward) */
} Elevator;

/* Single-tile platform that falls when stepped on. */
typedef struct
{
    int col, row;   /* tile grid position */
    float y;        /* current top position in world px */
    float vy;       /* vertical velocity (positive = down) */
    float timer;    /* time since triggered */
    bool triggered; /* true when player/enemy stepped on it */
    bool removed;   /* true when it has fallen away */
} FallPlatform;

/* Horizontally moving single-tile platform that patrols between two x-limits. */
typedef struct
{
    int row;           /* tile row where platform resides */
    float x;           /* current left position (world px) */
    float left_limit;  /* minimum x (world px) */
    float right_limit; /* maximum x (world px) */
    float vx;          /* current horizontal velocity (px/s, positive = right) */
} MovingPlatform;

/* A door tile position. Doors are paired by index: door[0]<->door[1], door[2]<->door[3], etc. */
typedef struct
{
    int col, row;
} Door;

/* A wall-mounted access terminal. Exactly one terminal is active per level. */
typedef struct
{
    int col, row;
} Terminal;

/* Background props are visual only and never participate in collision. */
typedef enum
{
    DECOR_OFFICE_CHAIR = 0,
    DECOR_OFFICE_DESK,
    DECOR_OFFICE_EQUIPMENT
} DecorationType;

typedef struct
{
    int col, row;
    DecorationType type;
} Decoration;

typedef enum
{
    ITEM_CARD = 0,
    ITEM_GUN,
    ITEM_GRENADE,
    ITEM_MEDKIT,
    ITEM_BAZOOKA
} ItemType;

typedef struct
{
    float x, y; /* center position of the item */
    bool collected;
    ItemType type;
    float respawn_timer; /* seconds until this item reappears (if collected) */
} Item;

typedef struct
{
    float x, y; /* spawn position (top-left of entity box) */
    bool has_dog;
} EnemySpawn;

typedef struct
{
    float x, y; /* spawn position (top-left of the visual-only NPC box) */
} JanitorSpawn;

typedef struct
{
    float x, y; /* spawn position (top-left of mine box) */
} MineSpawn;

typedef struct
{
    float x, y; /* spawn position (top-left of spike tile) */
} SpikeSpawn;

typedef struct
{
    float x, y; /* world-space centre of the rotating blades */
} CeilingFan;

typedef struct
{
    float x, y;
    float vx, vy;
    bool active;
    bool on_ground;
} Crate;

typedef struct
{
    float x, y;
    bool active;
} GasCanister;

typedef struct
{
    int width;
    int height;
    TileType tiles[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    float start_x, start_y;
    bool has_exit;
    int exit_col, exit_row;
    Terminal terminals[MAX_TERMINALS];
    int terminal_count;
    Decoration decorations[MAX_DECORATIONS];
    int decoration_count;
    EnemySpawn enemy_spawns[MAX_ENEMIES];
    int enemy_count;
    JanitorSpawn janitor_spawns[MAX_JANITORS];
    int janitor_count;
    MineSpawn mine_spawns[MAX_MINES];
    int mine_count;
    SpikeSpawn spike_spawns[MAX_SPIKES];
    int spike_count;
    CeilingFan ceiling_fans[MAX_CEILING_FANS];
    int ceiling_fan_count;
    Door doors[MAX_DOORS];
    int door_count;
    int door_spawn_counts[MAX_DOORS];
} LevelMap;

typedef struct
{
    bool exit_unlocked;
    Item items[MAX_ITEMS];
    int item_count;
    int card_count;
    int active_card_index;
    int items_remaining;
    int active_terminal_index;
    bool terminal_hacked;
    Crate crates[MAX_CRATES];
    int crate_count;
    GasCanister gas_canisters[MAX_GAS_CANISTERS];
    int gas_canister_count;

    Elevator elevators[MAX_ELEVATORS];
    int elevator_count;
    FallPlatform fall_platforms[MAX_FALL_PLATFORMS];
    int fall_platform_count;
    MovingPlatform moving_platforms[MAX_MOVING_PLATFORMS];
    int moving_platform_count;
} LevelRuntime;

typedef struct
{
    bool tiles_visible[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    int next_row;
    int next_col;
    float timer;
    float interval;
    bool done;
} LevelReveal;

typedef struct
{
    LevelMap map;
    LevelRuntime runtime;
    LevelReveal reveal;
} Level;

/* Parse a level from data embedded in the executable. Returns true on success. */
bool level_load_data(Level *level, const char *name,
                     const char *data, size_t size, Rng *rng);

/* Tile queries. Out-of-bounds is treated as solid wall. */
TileType level_tile(const Level *level, int col, int row);
bool level_is_solid(const Level *level, int col, int row);
bool level_is_ladder(const Level *level, int col, int row);
void level_update_elevators(Level *level, float dt);
void level_update_falling_platforms(Level *level, float dt);
void level_update_moving_platforms(Level *level, float dt);

/* Initialise level reveal state (hide all tiles and start timer). */
void level_reveal_init(Level *level);
/* Advance reveal animation by dt seconds; returns true when reveal just completed. */
bool level_reveal_step(Level *level, float dt);

/*
 * Move an axis-aligned box by its velocity and resolve collisions against
 * the tile map. Walls are fully solid. Ladders behave as one-way platforms
 * (you can stand on their top), unless 'climbing' is true, in which case the
 * box passes freely through ladder tiles.
 */
void level_move(Level *level, float *x, float *y, float *vx, float *vy,
                float w, float h, float dt, bool climbing, bool *on_ground,
                bool triggers_falling);

#endif /* CHUCK_LEVEL_H */
