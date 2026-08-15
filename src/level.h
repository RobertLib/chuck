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
    /* A patched-up opening: solid in every way a wall is until an explosion
     * takes it out. The hole it leaves is per-run state and lives in
     * LevelRuntime, so the parsed map stays exactly what the file says. */
    TILE_WEAK_WALL,
    TILE_LADDER,
    TILE_DOOR,
    TILE_SUBLEVEL_DOOR,
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

/* A guard-operated wall switch connected to the building alarm. */
typedef struct
{
    int col, row;
} AlarmSwitch;

/* Background props are visual only and never participate in collision. */
typedef enum
{
    DECOR_OFFICE_CHAIR = 0,
    DECOR_OFFICE_DESK,
    DECOR_OFFICE_EQUIPMENT,
    DECOR_RESTROOM_TOILET,
    DECOR_RESTROOM_BASIN,
    DECOR_RESTROOM_URINAL,
    DECOR_RESTROOM_PARTITION,
    DECOR_RESTROOM_STALL_OPEN,
    DECOR_RESTROOM_STALL_CLOSED,
    /* Front-of-house fittings. A lobby furnished out of the office set reads
     * as an office, so the public floor gets its own vocabulary. */
    DECOR_LOBBY_COUNTER,
    DECOR_LOBBY_SOFA,
    DECOR_LOBBY_PLANTER,
    DECOR_LOBBY_TURNSTILE,
    /* The two props that belong to this night rather than to the building.
     * The case is what the crew wheeled in through the goods entrance; the
     * clock is the deadline they are working to, and it is the only one of
     * the two that hangs rather than stands. */
    DECOR_FLIGHT_CASE,
    DECOR_WALL_CLOCK
} DecorationType;

/* A clock is fixed to the slab above it, every other prop stands on the one
 * below. The loader drops whichever is unsupported, so this is the one thing
 * about a decoration the parser has to ask before keeping it. */
static inline bool decoration_hangs(DecorationType type)
{
    return type == DECOR_WALL_CLOCK;
}

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
    float x, y; /* spawn position (top-left of the visual-only NPC box) */
} CivilianSpawn;

typedef struct
{
    float x, y; /* the post: top-left of the visual-only NPC box */
} ReceptionistSpawn;

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
    /* Underside of the first solid tile above the fan. A fan hung in a hall
     * rather than tight under a slab still has to read as hung, so the
     * renderer drops a rod from here instead of leaving it in mid-air. */
    float mount_y;
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

typedef enum
{
    LEVEL_MODE_INTERIOR = 0,
    LEVEL_MODE_FACADE
} LevelMode;

/*
 * Art direction for one level. The theme changes nothing about the simulation
 * — it only tells the renderer which masonry, backdrop and lighting to use, so
 * that fifteen sectors of the same building read as a journey through it
 * instead of fifteen runs down the same corridor. Themes are authored with a
 * `THEME <name>` metadata line; a map that omits one keeps the default look
 * for its mode.
 */
typedef enum
{
    LEVEL_THEME_PLANT = 0, /* interior default: service / machine floor */
    LEVEL_THEME_LOBBY,
    LEVEL_THEME_OFFICE,
    LEVEL_THEME_SERVER,
    LEVEL_THEME_CANTEEN,
    LEVEL_THEME_LAB,
    LEVEL_THEME_ARCHIVE,
    LEVEL_THEME_SECURITY,
    LEVEL_THEME_DUCTS,
    LEVEL_THEME_PENTHOUSE,
    LEVEL_THEME_ROOF,
    LEVEL_THEME_RESTROOM, /* implied by restroom fittings in the map */
    LEVEL_THEME_FACADE_NIGHT, /* facade default */
    LEVEL_THEME_FACADE_STORM,
    LEVEL_THEME_FACADE_DAWN,
    LEVEL_THEME_FACADE_HIGH,
    LEVEL_THEME_COUNT
} LevelTheme;

typedef enum
{
    FACADE_HAZARD_THROWN_OBJECT = 0,
    FACADE_HAZARD_BIRD
} FacadeHazardType;

typedef struct
{
    float x, y; /* center of the source window or bird entry point */
    FacadeHazardType type;
} FacadeHazardSpawn;

typedef struct
{
    int width;
    int height;
    TileType tiles[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    float start_x, start_y;
    LevelMode mode;
    bool has_exit;
    int exit_col, exit_row;
    bool has_window;
    int window_col, window_row;
    FacadeHazardSpawn facade_hazard_spawns[MAX_FACADE_HAZARD_SPAWNS];
    int facade_hazard_spawn_count;
    bool has_sublevel_entrance;
    int sublevel_entrance_col, sublevel_entrance_row;
    bool has_sublevel_return;
    int sublevel_return_col, sublevel_return_row;
    LevelTheme theme;
    Terminal terminals[MAX_TERMINALS];
    int terminal_count;
    AlarmSwitch alarm_switches[MAX_ALARM_SWITCHES];
    int alarm_switch_count;
    Decoration decorations[MAX_DECORATIONS];
    int decoration_count;
    EnemySpawn enemy_spawns[MAX_ENEMIES];
    int enemy_count;
    JanitorSpawn janitor_spawns[MAX_JANITORS];
    int janitor_count;
    CivilianSpawn civilian_spawns[MAX_CIVILIANS];
    int civilian_count;
    ReceptionistSpawn receptionist_spawns[MAX_RECEPTIONISTS];
    int receptionist_count;
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
    /* Which weak walls have been blown open, by tile. A hole is per-run state
     * like a fallen panel or a broken crate: it outlives a lost life and is
     * gone the moment the sector is loaded again. */
    bool wall_broken[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
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

/* Resolve a `THEME` metadata name such as "SERVER". False if unknown. */
bool level_theme_from_name(const char *name, size_t length, LevelTheme *out);

/* The authoring name a theme is written with. Never NULL. */
const char *level_theme_name(LevelTheme theme);

/* Tile queries. Out-of-bounds is treated as solid wall. */
TileType level_tile(const Level *level, int col, int row);
bool level_is_solid(const Level *level, int col, int row);
bool level_is_ladder(const Level *level, int col, int row);

/* True when this tile is a weak wall that has already been blown open.
 * Out-of-bounds is never broken — it is the world edge, not a wall. */
bool level_wall_broken(const Level *level, int col, int row);

/* Open the weak wall at this tile. False when there was none left standing,
 * so a caller can tell whether the blast actually took something out. */
bool level_break_wall(Level *level, int col, int row);
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
