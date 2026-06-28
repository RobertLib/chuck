#ifndef CHUCK_LEVEL_H
#define CHUCK_LEVEL_H

#include "common.h"

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

typedef enum
{
    ITEM_CARD = 0,
    ITEM_GUN,
    ITEM_GRENADE,
    ITEM_MEDKIT
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
} EnemySpawn;

typedef struct
{
    float x, y; /* spawn position (top-left of mine box) */
} MineSpawn;

typedef struct
{
    int width;
    int height;
    TileType tiles[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];

    float start_x, start_y; /* player spawn (top-left of player box) */

    bool has_exit;
    int exit_col, exit_row;

    Item items[MAX_ITEMS];
    int item_count;
    int card_count;        /* items of type ITEM_CARD */
    int active_card_index; /* index into items[] of the randomly chosen key card */
    int items_remaining;   /* 1 = key card not yet found, 0 = key card collected */

    EnemySpawn enemy_spawns[MAX_ENEMIES];
    int enemy_count;
    MineSpawn mine_spawns[MAX_MINES];
    int mine_count;

    Door doors[MAX_DOORS];
    int door_count;
    int door_spawn_counts[MAX_DOORS]; /* enemies queued to spawn from each door */

    Elevator elevators[MAX_ELEVATORS];
    int elevator_count;

    FallPlatform fall_platforms[MAX_FALL_PLATFORMS];
    int fall_platform_count;
    /* Horizontally moving platforms */
    MovingPlatform moving_platforms[MAX_MOVING_PLATFORMS];
    int moving_platform_count;

    /* Reveal/intro animation state: whether each tile has been revealed yet.
     * At level start we animate tiles appearing; items/enemies are shown after
     * the reveal completes. */
    bool tiles_visible[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    int reveal_next_row;   /* next tile row to reveal */
    int reveal_next_col;   /* next tile col to reveal */
    float reveal_timer;    /* accumulator for reveal timing */
    float reveal_interval; /* seconds between revealing individual tiles */
    bool reveal_done;      /* true when full reveal finished */
} Level;

/* Load a level from a text file. Returns true on success. */
bool level_load(Level *level, const char *path);

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
