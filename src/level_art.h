#ifndef CHUCK_LEVEL_ART_H
#define CHUCK_LEVEL_ART_H

/*
 * Per-level art direction.
 *
 * The campaign is one building, but a player who sees the same riveted wall
 * and the same machine hall for fifteen sectors stops reading the levels as
 * places. A LevelTheme picks a wall material, a backdrop composition and a
 * palette; everything here is presentation only and reads nothing but the
 * immutable LevelMap, so no gameplay behaviour depends on it.
 *
 * Palettes still come from the fx.h vocabulary — themes shift hue and value
 * inside that system rather than inventing a new one per level.
 *
 * The theme also names the level's score, for the same reason it names the
 * palette: a server aisle and a rooftop are not the same place, and one loop
 * for the whole building would say they were.
 */

#include "audio.h"
#include "fx.h"
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
    BACKDROP_FACADE_NIGHT,  /* the four exterior climbs */
    BACKDROP_FACADE_STORM,
    BACKDROP_FACADE_DAWN,
    BACKDROP_FACADE_HIGH
} BackdropStyle;

/*
 * One palette drives both the tiles and the backdrop so a level holds together.
 * Facade themes reuse the same fields for the exterior: the gradient is sky,
 * `far_shape` is the distant towers, `wall*` is the building face and `trim*`
 * is the cornice stone.
 */
typedef struct
{
    WallStyle wall_style;
    BackdropStyle backdrop;

    SDL_Color wall;       /* base fill of a solid tile */
    SDL_Color wall_dark;  /* seams, joints and shadowed faces */
    SDL_Color wall_light; /* lit bevels and highlights */
    SDL_Color trim;       /* exposed floor lip / skirting */
    SDL_Color trim_hi;    /* the bright line along that lip */
    SDL_Color accent;     /* paint marks, signage, small fittings */

    SDL_Color air_top;    /* backdrop gradient, top */
    SDL_Color air_bottom; /* backdrop gradient, floor level */
    SDL_Color far_shape;  /* deepest silhouettes */
    SDL_Color near_shape; /* nearer structure */
    SDL_Color lamp;       /* ceiling fixture / lit window colour */
    SDL_Color haze;       /* dust motes and the depth haze */
    Uint8 lamp_alpha;     /* how brightly ceilings are lit; 0 leaves them dark */
} LevelThemeArt;

/* Art for a theme. Never NULL: an out-of-range theme falls back to the
 * interior default, so a renderer never has to branch on validity. */
const LevelThemeArt *level_art(LevelTheme theme);

/* The music the sector is scored with. One track per theme. */
MusicTrack level_theme_music(LevelTheme theme);

/* Everything a themed layer needs to place itself on screen. */
typedef struct
{
    SDL_Renderer *renderer;
    const Level *level;
    float cam_x, cam_y; /* camera position in world pixels */
    int win_w, win_h;
    float time;      /* wall-clock seconds; visual only, never gameplay */
    int level_index; /* campaign index, for stable per-level variation */
} LevelArtScene;

/* Draw the parallax backdrop for the scene's level theme. */
void level_art_backdrop(const LevelArtScene *scene);

/* Draw one solid tile in the level's wall material. */
void level_art_wall_tile(SDL_Renderer *r, const Level *level,
                         int col, int row, float x, float y);

#endif /* CHUCK_LEVEL_ART_H */
