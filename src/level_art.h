#ifndef CHUCK_LEVEL_ART_H
#define CHUCK_LEVEL_ART_H

/*
 * Per-level art direction.
 *
 * The campaign is one building, but a player who sees the same riveted wall
 * and the same machine hall for seventeen sectors stops reading the levels as
 * places. A LevelTheme picks a wall material, a backdrop composition and a
 * palette; everything here is presentation only and reads nothing but the
 * immutable LevelMap, so no gameplay behaviour depends on it.
 *
 * Everything this file draws is drawn in the eight colours of a BBC Micro in
 * MODE 2, on its grid of fat pixels, through [bbc.h](bbc.h) and nothing else.
 * Which of the eight a theme's masonry, lip and back wall are is one row of
 * [level_paint.c](level_paint.c), because the world renderer has to agree with
 * it; what this file owns is the *pattern* a theme is drawn in — the wall
 * material, the floor finish, the room behind — which is the LevelThemeArt
 * table's WallStyle, FloorStyle and BackdropStyle.
 *
 * The theme also names the level's score, for the same reason it names the
 * palette: a server aisle and a rooftop are not the same place, and one loop
 * for the whole building would say they were.
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "level.h"

/* How a solid tile is drawn. Several themes share a style and differ only in
 * palette, which is enough to read as a different floor of the building. */
typedef enum
{
    WALL_STYLE_PLATE = 0, /* riveted steel plating, 2x2 tile panels */
    WALL_STYLE_CONCRETE,  /* poured concrete with shuttering and form ties */
    WALL_STYLE_TILE,      /* small glazed ceramic tiling */
    WALL_STYLE_MARBLE,    /* polished veined slabs with a metal inlay */
    WALL_STYLE_DRYWALL,   /* painted partition boarding */
    WALL_STYLE_BRICK,     /* stretcher-bond masonry */
    WALL_STYLE_WOOD       /* stile-and-rail hardwood panelling */
} WallStyle;

/*
 * What the walkable top of a slab is finished in.
 *
 * Seen from the side a floor is a sliver a few pixels deep, which is exactly
 * why it needs naming: it is the one surface the player looks at all the time,
 * and the sliver is the difference between "a sector of the building" and "the
 * top edge of a block". The bright arris line above it stays the theme's trim
 * in every sector, so where Chuck can stand reads the same way throughout.
 */
typedef enum
{
    FLOOR_SCREED = 0, /* power-floated concrete or steel deck */
    FLOOR_STONE,      /* polished stone, with the sheen that implies */
    FLOOR_CARPET,     /* matte contract carpet tile */
    FLOOR_PANEL,      /* perforated raised access floor */
    FLOOR_CHEQUER,    /* stainless treadplate */
    FLOOR_CERAMIC,    /* grouted ceramic */
    FLOOR_BOARDS,     /* butt-jointed timber boards */
    FLOOR_PARQUET     /* hardwood blocks with a metal inlay */
} FloorStyle;

/* Which parallax composition sits behind the tile grid. */
typedef enum
{
    BACKDROP_PLANT = 0,
    BACKDROP_LOBBY,
    BACKDROP_OFFICE,
    BACKDROP_SERVER,
    BACKDROP_CANTEEN,
    BACKDROP_LAB,
    BACKDROP_ARCHIVE,
    BACKDROP_SECURITY,
    BACKDROP_DUCTS,
    BACKDROP_PENTHOUSE,
    BACKDROP_ROOF,
    BACKDROP_RESTROOM,      /* drawn by game_render.c from the room's own walls */
    /* Four backdrops for five climbs: FACADE_SLEET borrows the storm's,
     * because it is the same weather at a later hour. Anything that wants
     * one answer per *climb* — the cordon's height fade below — has to ask
     * the theme rather than this enum, or the fifth climb silently answers
     * as the second. */
    BACKDROP_FACADE_NIGHT,
    BACKDROP_FACADE_STORM,
    BACKDROP_FACADE_MOON,
    BACKDROP_FACADE_HIGH
} BackdropStyle;

/*
 * A theme's art direction: which of the patterns above its masonry, its
 * walkway lip and its room are drawn in.
 *
 * The colours are not here. They are the theme's row in
 * [level_paint.c](level_paint.c) — eight colours, on the side of the boundary
 * the suite can see — and both renderers that draw a floor read them from
 * there. This table used to carry twelve `SDL_Color`s and a lamp alpha as well,
 * from the renderer that drew with light; the patterns are all that survived it.
 */
typedef struct
{
    WallStyle wall_style;
    FloorStyle floor_style;
    BackdropStyle backdrop;
} LevelThemeArt;

/* Art for a theme. Never NULL: an out-of-range theme falls back to the
 * interior default, so a renderer never has to branch on validity. */
const LevelThemeArt *level_art(LevelTheme theme);

/* Everything a themed layer needs to place itself on screen. */
typedef struct
{
    SDL_Renderer *renderer;
    const Level *level;
    float cam_x, cam_y; /* camera position in world pixels */
    int win_w, win_h;
    float time;      /* wall-clock seconds; visual only, never gameplay */
    int level_index; /* campaign index, for stable per-level variation */
    /* The player asked for reduced motion, so anything in the backdrop that
     * blinks holds still instead: a flashing lamp stays on one half of its
     * beat (which is what `bbc_flash` does under the same switch), a beacon
     * stays lit, and the storm has no lightning. Only the blinking goes. */
    bool steady_lights;
} LevelArtScene;

/* Draw the parallax backdrop for the scene's level theme. */
void level_art_backdrop(const LevelArtScene *scene);

/* Draw one solid tile in the level's wall material. A weak wall gets the same
 * material with the blocked-up opening over it, so the two can never drift
 * apart into different-looking walls. */
void level_art_wall_tile(SDL_Renderer *r, const Level *level,
                         int col, int row, float x, float y);

/* What a weak wall leaves once a blast has opened it: the hole is the route,
 * and the rubble in the bottom of it is what says the route was made rather
 * than built. Draws nothing where the debris would have no floor to lie on. */
void level_art_broken_wall_tile(SDL_Renderer *r, const Level *level,
                                int col, int row, float x, float y);

#endif /* CHUCK_LEVEL_ART_H */
