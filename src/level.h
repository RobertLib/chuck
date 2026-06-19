#ifndef CHUCK_LEVEL_H
#define CHUCK_LEVEL_H

#include "game_config.h"
#include "music_id.h"
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
    /* There is deliberately no tile for a falling panel. `F` parses to
     * TILE_EMPTY plus a `FallPlatform` in LevelRuntime, because the panel is
     * a thing with a position and a velocity rather than a property of a
     * cell — and the map has to stay exactly what the file said, so that a
     * hole is per-visit state and a reload puts the panel back. A
     * `TILE_FALL_PLATFORM` sat here for a long time, named in this enum and
     * mentioned in no other line of the tree, with a comment describing the
     * mechanic the runtime list actually implements. A tile type nothing
     * parses to and nothing tests for is a claim about the map format that
     * is not true of it. */
    /* A ventilation duct: trunking let into the wall. Masonry to everything in
     * the building — sight, rounds, blasts, props, guards, dogs — and a way
     * through to a man on his elbows. It is the one tile the two solidity
     * questions answer differently; see `Stance` below. */
    TILE_VENT,
    TILE_TYPE_COUNT
} TileType;

/*
 * How tall the thing being moved through the map is carrying itself.
 *
 * It exists because solidity is two questions rather than one, and only one of
 * them a posture can change: what stops a bullet, a line of sight, a crate or
 * a guard is the building, and what stops a *body* is the building as that
 * body happens to be shaped. Every tile the game has answers both the same
 * way, which is why this parameter has been `UPRIGHT` everywhere for as long
 * as there was nothing to ask.
 *
 * `UPRIGHT` is nought so a zero-filled caller gets the building's own answer:
 * the safe reading of an uninitialised stance is that everything is wall.
 *
 * It is an enum rather than a bool because `level_move` already ends in two of
 * those, and a third would make every one of its call sites a coin flip.
 */
typedef enum
{
    STANCE_UPRIGHT = 0, /* everyone in the building, nearly every frame */
    STANCE_CRAWLING     /* flat on the elbows */
} Stance;

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
    /* True once Chuck has stood on it, and only Chuck: every other body in the
     * building passes `triggers_falling` as false to `level_move`. The comment
     * here used to say "player/enemy", which was true of neither — a guard has
     * never armed one — and read as licence for the dog that was. */
    bool triggered;
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

/*
 * A ceiling camera. Like the terminal and the switch it is a map position and
 * nothing else — the sweep, what it has seen and whether it is still working
 * are simulation and live in `GameplayState`, because a camera is a thing that
 * is *happening* rather than a thing the map says.
 *
 * It hangs, so it asks the tile above it for support the way the wall clock
 * does; a camera bolted to thin air is the same mistake as a desk with no floor
 * under it, and the loader and the editor both refuse it.
 */
typedef struct
{
    int col, row;
} SecurityCamera;

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
    DECOR_WALL_CLOCK,
    /*
     * The plant set, and why the building needed a third vocabulary.
     *
     * Counted by prop, the campaign used to split clean in two: the office and
     * front-of-house themes carried sixteen to twenty-three apiece and PLANT,
     * LAB, DUCTS, VAULT and ROOF carried three to nine. It read like the top of
     * the tower running out of attention and it was not — sector 5 is as bare as
     * sector 12 — because what those five have in common is a *room* rather than
     * a position: a machine hall, a clean room, a duct run, a strongroom and an
     * open service deck. There was no prop for any of them. `c d i` is an office
     * and `n s t g` is a foyer, so the only way to dress a strongroom was to put
     * a swivel chair in it, which [levels/LEGEND.md](../levels/LEGEND.md)
     * explicitly forbids and explicitly asked for this instead.
     *
     * Four, because four is what the page named and what those five rooms
     * actually contain: something stacked, something spooled, something plumbed
     * and something that keeps a vehicle off a wall. All of them stand on the
     * slab below like every prop but the clock, none of them collides, and none
     * is read by the simulation.
     */
    DECOR_PLANT_PALLET,
    DECOR_PLANT_CABLE_REEL,
    DECOR_PLANT_PIPE_RAIL,
    DECOR_PLANT_BOLLARD
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
    ITEM_BAZOOKA,
    /*
     * A sheet off Meridian's own docket, and the only pickup in the game that
     * is worth nothing to the man carrying it.
     *
     * Everything else on this list is ammunition, hearts or a door. This is
     * paper: it costs a detour, it changes no counter the simulation reads, and
     * what it buys is entirely outside the sector. The fiction is why it is
     * here at all — the 00:04 broadcast has the whole city believing this is a
     * political siege, and the one thing nobody outside the building has is
     * proof that twelve men badged in as a maintenance contractor came for six
     * hundred and forty million in bearer bonds. Chuck picking that up as he
     * climbs is the only way the night is ever explained to anybody but him.
     */
    ITEM_EVIDENCE,
    /* A flash charge: one at a time, and the only thing Chuck throws that is
     * meant to be survived by everybody in the room. See MAX_FLASHBANGS. */
    ITEM_FLASHBANG
} ItemType;

typedef struct
{
    float x, y; /* center position of the item */
    bool collected;
    ItemType type;
    float respawn_timer; /* seconds until this item reappears (if collected) */
} Item;

/*
 * Which kind of man this is.
 *
 * Kept as an enum on the guard rather than as a second array, because
 * everything about him except three numbers is identical — he patrols with the
 * same code, sees with the same cone, investigates the same bodies and runs for
 * the same switch. A separate `HeavyGuard` type would be a second copy of
 * eleven hundred lines of AI to keep in step with the first, and the whole
 * lesson of `apply_blast` is what happens to two copies of one rule.
 */
typedef enum
{
    ENEMY_KIND_GUARD = 0,
    /* Plate carrier and a full helmet: more rounds from the front, slower on
     * his feet, and **not stompable** — which is the answer he is here to take
     * away. The blade behind him still works, because that is a knife across a
     * throat rather than damage. */
    ENEMY_KIND_HEAVY
} EnemyKind;

/* The three numbers that differ, asked once so no call site has to branch. */
static inline int enemy_kind_hp(EnemyKind kind)
{
    return kind == ENEMY_KIND_HEAVY ? ENEMY_HEAVY_HP : ENEMY_HP;
}

static inline float enemy_kind_speed(EnemyKind kind)
{
    return kind == ENEMY_KIND_HEAVY ? ENEMY_HEAVY_SPEED : 1.0f;
}

static inline bool enemy_kind_can_be_stomped(EnemyKind kind)
{
    return kind != ENEMY_KIND_HEAVY;
}

typedef struct
{
    float x, y; /* spawn position (top-left of entity box) */
    bool has_dog;
    /* Which kind of man stands here. A field on the spawn rather than a second
     * array, for the reason `EnemyKind` itself is a field on the guard: they
     * are the same entity with three numbers different. */
    EnemyKind kind;
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
 * that seventeen sectors of the same building read as a journey through it
 * instead of seventeen runs down the same corridor. Themes are authored with a
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
    /* The sub-vault: the room the whole night is about, and the one the player
     * had never seen. Steel boxes, open grilles and the trolley rails they
     * emptied it onto. */
    LEVEL_THEME_VAULT,
    LEVEL_THEME_RESTROOM, /* implied by restroom fittings in the map */
    LEVEL_THEME_FACADE_NIGHT, /* facade default */
    LEVEL_THEME_FACADE_STORM,
    LEVEL_THEME_FACADE_MOON,
    LEVEL_THEME_FACADE_HIGH,
    /* The fifth climb, between the penthouse and the roof: the weather has
     * come back in off the sea and the last stretch is wet again. */
    LEVEL_THEME_FACADE_SLEET,
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
    SecurityCamera cameras[MAX_CAMERAS];
    int camera_count;
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
    /*
     * Which way the front moves: down the rows, or across the columns.
     *
     * It sweeps along the map's *shorter* axis, because that is the axis a
     * viewport can hold. An interior is 34 to 60 tiles wide and 17 to 23 tall
     * against a 25x16 view, so a front moving down covers the screen for very
     * nearly the whole walk — which is what the reveal has always done and what
     * it still does. A climb is 25 wide and 41 to 53 tall, so the same front
     * spends its first two thirds above the camera: the wall it is revealing is
     * off the top of the frame, and the player watches a blank backdrop for
     * 2.3s of a 3.8s beat before anything appears. Across the columns it is
     * visible from the first tick, because a climb is exactly as wide as the
     * view.
     *
     * Derived from the map's shape rather than from `LevelMode`, because the
     * shape is the reason: a mode is a proxy that happens to agree today, and
     * the tall narrow map is the thing the rule is actually about.
     */
    bool by_column;
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

/* The room a sector's `U` opens on, as a file stem under `levels/sublevels/`.
 * Never NULL: a theme with no room of its own gets the lobby's, because a door
 * that opens on nothing is worse than one that opens on a familiar room. */
#define LEVEL_FALLBACK_SUBLEVEL "restroom_lobby"
const char *level_theme_sublevel(LevelTheme theme);

/* The score a sector is played under. One track per theme, never the title's:
 * see the note on the table in level.c and the twin one in music_id.h. */
MusicTrack level_theme_music(LevelTheme theme);

/*
 * How much of the police cordon still reaches this wall, 1.0 on the first and
 * lowest climb and 0.0 once the street has stopped reading at all.
 *
 * Presentation — `facade_cordon` in level_art.c is the only caller — but it
 * lives here for the reason `THEME_MUSIC` does, and it is the same shape of
 * near miss. It was a `switch` inside that renderer keyed on the *backdrop*,
 * and there are four backdrops for five climbs because `FACADE_SLEET` borrows
 * the storm's: the highest wall in the game answered as the second one and
 * washed its face with more street than the two climbs below it. A value per
 * climb has to be asked of the thing there is one of per climb, and on this
 * side of the SDL line the suite can say so — a sixth climb added with no row
 * zero-fills to "no cordon", which is at least the honest end of the range,
 * and `test_the_cordon_fades_as_the_climb_rises` fails rather than shipping it.
 * Zero for every interior: there is no wall to wash.
 */
float level_theme_cordon(LevelTheme theme);

/*
 * How far a backdrop layer has sunk down the frame by the time the camera has
 * climbed this high: nought at the foot of the wall, and `factor` times the
 * camera's whole run at the top of it.
 *
 * Presentation — the three parallax layers behind a climb are the only callers
 * — and it lives on this side of the SDL line for the reason
 * `level_theme_cordon` does, on the same kind of near miss. A climb is the one
 * place in the game where the camera travels on the axis none of those layers
 * handled: a facade map is exactly one viewport wide, so cam_x is nought on all
 * five walls and the only motion out there is the vertical one. Both things
 * they did with cam_y were wrong, and nothing the suite can reach could say so.
 *
 * The sign was inverted. `+ cam_y * factor` moves a layer *up* the frame as the
 * camera rises, against the wall drawn in front of it, which is a backdrop
 * sliding the wrong way past the thing it is meant to be behind. A distant
 * tower sits at eye level whatever storey Chuck is on, so what a climb does to
 * the city is **sink** it.
 *
 * And the offset was wrapped — `fmodf(cam_y * 0.14f, 110.0f)` — which puts the
 * entire skyline through a 110px snap partway up three of the five walls, and
 * the HIGH climb's cloud deck through a 60px one. `facade_news_helicopter` has
 * the argument written above it already: wrapping the height of one
 * recognisable object is a visible jump, and a skyline is one object. The stars
 * are the one layer that may wrap and do — they are a field, and their period
 * is wider than the frame, so a star turns over off screen.
 *
 * Which makes the two things worth asking of it properties rather than
 * arithmetic — it only ever runs one way, and it never jumps — and
 * `test_a_backdrop_layer_sinks_as_the_climb_rises` asks both of all seventeen
 * shipped maps. `view_h` is the frame under the HUD, the same height the camera
 * clamps against, so the foot of the wall is the camera's own resting place and
 * not a second copy of that clamp.
 */
float level_backdrop_sink(const LevelMap *map, float cam_y, float view_h,
                          float factor);

/* True when an embedded sublevel path is the file that stem names. */
bool level_sublevel_name_is(const char *path, const char *stem);

/*
 * How many hostiles the plan puts on this floor: every guard, plus the dog each
 * of them may bring.
 *
 * A ceiling on what a clear of this sector can have counted, and it is here
 * rather than beside the one thing that asks it because it is a fact about the
 * map. Deliberately the *authored* population and not what a run can reach — a
 * console's reinforcements and a `SPAWNS` drip both add to it — so anything
 * held under this number is a number a clear could have produced, which is what
 * the staged screens' fixture needs and all it needs.
 */
int level_authored_hostiles(const LevelMap *map);

/* Tile queries. Out-of-bounds is treated as solid wall. */
TileType level_tile(const Level *level, int col, int row);
bool level_is_solid(const Level *level, int col, int row);
/* Whether this tile stops a body carrying itself that way. Ask `level_is_solid`
 * for everything else — sight, rounds, blasts, shading, props, pathing —
 * because those are questions about the building and this is one about a body.
 * The two agree on every tile the game currently has; see `Stance` above. */
bool level_blocks_stance(const Level *level, int col, int row, Stance stance);
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
/*
 * Stretch the reveal so the whole walk takes `seconds`, and never speed it up.
 *
 * **The reveal is the vehicle for whatever is written over it**, and until this
 * existed it was the other way round: the between-sectors line
 * ([sector_tally.h](sector_tally.h)) is drawn while `STATE_LEVEL_START` is on
 * screen, that state lasts exactly as long as this animation, and the animation
 * is `width * height / 3000` seconds — so a line of about 120 characters was
 * readable for **0.18s** on the smallest sector and 0.43s on the tallest climb,
 * and how long the player got to read the plot depended on how big the next
 * floor happened to be. See `SECTOR_TALLY_HOLD_TIME`.
 *
 * A duration rather than an interval, because the interval is the thing that
 * cannot be reasoned about: it is per *tile*, and the tile count is what varies.
 * `test_a_stretched_reveal_lasts_the_same_on_every_map` holds the difference.
 *
 * Only ever slower, so a call with a duration shorter than the map's own snappy
 * default leaves the default alone. Nothing wants a reveal that flashes past
 * faster than the one somebody already tuned.
 */
void level_reveal_hold_for(Level *level, float seconds);
/* Advance reveal animation by dt seconds; returns true when reveal just completed. */
bool level_reveal_step(Level *level, float dt);

/*
 * Move an axis-aligned box by its velocity and resolve collisions against
 * the tile map. Walls are fully solid. Ladders behave as one-way platforms
 * (you can stand on their top), unless 'climbing' is true, in which case the
 * box passes freely through ladder tiles.
 *
 * 'stance' is how the box is carrying itself, and it is the box's own business:
 * everything else this function consults — the ladders, the two kinds of
 * platform — is the building, and the building does not care. Anything that is
 * not a body in a posture passes `STANCE_UPRIGHT`.
 */
void level_move(Level *level, float *x, float *y, float *vx, float *vy,
                float w, float h, float dt, bool climbing, bool *on_ground,
                bool triggers_falling, Stance stance);

#endif /* CHUCK_LEVEL_H */
