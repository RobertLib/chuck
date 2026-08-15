#include "level_art.h"

/*
 * Themed wall materials and backdrops.
 *
 * Everything in this file is presentation. It reads the immutable LevelMap to
 * know where solid tiles are and nothing else, so a theme can never change how
 * a level plays — only how the same floor plan feels to walk through.
 */

/* ---- Palettes ------------------------------------------------------- */

static const LevelThemeArt THEME_ART[LEVEL_THEME_COUNT] = {
    /* PLANT — the mechanical floor the game shipped with: cold steel
     * plating over a hall of half-lit machinery. */
    [LEVEL_THEME_PLANT] = {
        WALL_STYLE_PLATE, FLOOR_SCREED, BACKDROP_PLANT,
        {41, 51, 64, 255}, {20, 27, 39, 255}, {88, 104, 120, 255},
        {104, 122, 136, 255}, {168, 184, 192, 255}, {168, 112, 40, 255},
        {7, 10, 18, 255}, {20, 30, 42, 255},
        {13, 18, 29, 255}, {18, 25, 38, 255},
        {248, 202, 118, 255}, {130, 162, 170, 255}, 70},

    /* LOBBY — the ground floor Chuck walks in through: stone, brass and a
     * glazed street front, the only sector still lit for the public. */
    [LEVEL_THEME_LOBBY] = {
        WALL_STYLE_MARBLE, FLOOR_STONE, BACKDROP_LOBBY,
        {88, 84, 80, 255}, {38, 36, 36, 255}, {162, 154, 140, 255},
        {158, 132, 86, 255}, {228, 206, 158, 255}, {228, 186, 104, 255},
        {14, 17, 26, 255}, {32, 34, 42, 255},
        {18, 22, 33, 255}, {30, 33, 44, 255},
        {250, 222, 164, 255}, {186, 176, 150, 255}, 96},

    /* OFFICE — open plan after hours: painted partitions, blinds, and the
     * one bank of lights nobody switched off. */
    [LEVEL_THEME_OFFICE] = {
        WALL_STYLE_DRYWALL, FLOOR_CARPET, BACKDROP_OFFICE,
        {58, 66, 78, 255}, {26, 31, 40, 255}, {112, 124, 140, 255},
        {150, 160, 172, 255}, {198, 208, 216, 255}, {84, 150, 168, 255},
        {10, 16, 26, 255}, {26, 36, 50, 255},
        {15, 21, 33, 255}, {24, 32, 46, 255},
        {206, 226, 236, 255}, {140, 166, 182, 255}, 84},

    /* SERVER — a cold aisle. Almost no white light; the room is lit by the
     * equipment it exists to hold. */
    [LEVEL_THEME_SERVER] = {
        WALL_STYLE_PLATE, FLOOR_PANEL, BACKDROP_SERVER,
        {26, 34, 42, 255}, {10, 15, 20, 255}, {62, 84, 96, 255},
        {66, 116, 122, 255}, {148, 214, 208, 255}, {74, 222, 212, 255},
        {4, 10, 16, 255}, {9, 23, 31, 255},
        {8, 16, 22, 255}, {12, 24, 32, 255},
        {96, 226, 220, 255}, {90, 180, 190, 255}, 58},

    /* CANTEEN — glazed tile and stainless steel under heat lamps; the
     * warmest, brightest sector of the climb. */
    [LEVEL_THEME_CANTEEN] = {
        WALL_STYLE_TILE, FLOOR_CHEQUER, BACKDROP_CANTEEN,
        {126, 132, 128, 255}, {52, 58, 57, 255}, {200, 204, 192, 255},
        {126, 120, 104, 255}, {212, 202, 172, 255}, {236, 178, 84, 255},
        {18, 20, 22, 255}, {36, 38, 35, 255},
        {24, 26, 26, 255}, {42, 44, 41, 255},
        {252, 214, 146, 255}, {170, 168, 150, 255}, 104},

    /* LAB — clean room: pale tile, green-lit cabinets, everything sterile
     * and slightly too bright. */
    [LEVEL_THEME_LAB] = {
        WALL_STYLE_TILE, FLOOR_CERAMIC, BACKDROP_LAB,
        {150, 164, 162, 255}, {58, 72, 74, 255}, {218, 228, 222, 255},
        {112, 150, 148, 255}, {212, 238, 230, 255}, {110, 230, 182, 255},
        {14, 24, 28, 255}, {32, 50, 52, 255},
        {20, 32, 36, 255}, {32, 50, 52, 255},
        {216, 246, 238, 255}, {160, 200, 196, 255}, 110},

    /* ARCHIVE — old brick shell kept from the original building, filled
     * floor to ceiling with paper and dust. */
    [LEVEL_THEME_ARCHIVE] = {
        WALL_STYLE_BRICK, FLOOR_BOARDS, BACKDROP_ARCHIVE,
        {92, 62, 48, 255}, {40, 27, 22, 255}, {146, 102, 74, 255},
        {126, 96, 58, 255}, {192, 152, 96, 255}, {224, 164, 72, 255},
        {16, 12, 12, 255}, {36, 27, 23, 255},
        {22, 17, 15, 255}, {36, 28, 23, 255},
        {244, 196, 116, 255}, {180, 152, 110, 255}, 78},

    /* SECURITY — the control wing: dark steel, a wall of monitors and a
     * standby beacon that never quite stops turning. */
    [LEVEL_THEME_SECURITY] = {
        WALL_STYLE_PLATE, FLOOR_CARPET, BACKDROP_SECURITY,
        {46, 40, 44, 255}, {18, 14, 17, 255}, {92, 76, 80, 255},
        {112, 68, 62, 255}, {202, 130, 114, 255}, {232, 74, 62, 255},
        {10, 6, 10, 255}, {28, 15, 17, 255},
        {16, 10, 13, 255}, {26, 16, 19, 255},
        {236, 120, 96, 255}, {170, 120, 120, 255}, 64},

    /* DUCTS — the plenum between floors: galvanised trunking, no fixtures,
     * the darkest sector in the game. */
    [LEVEL_THEME_DUCTS] = {
        WALL_STYLE_PLATE, FLOOR_CHEQUER, BACKDROP_DUCTS,
        {44, 48, 54, 255}, {18, 20, 24, 255}, {104, 112, 120, 255},
        {120, 130, 138, 255}, {186, 196, 202, 255}, {206, 166, 92, 255},
        {6, 8, 11, 255}, {17, 21, 26, 255},
        {12, 15, 19, 255}, {20, 24, 30, 255},
        {198, 214, 222, 255}, {120, 134, 146, 255}, 40},

    /* PENTHOUSE — the executive floor: hardwood panelling, sconces and
     * money, one storey below the roof. */
    [LEVEL_THEME_PENTHOUSE] = {
        WALL_STYLE_WOOD, FLOOR_PARQUET, BACKDROP_PENTHOUSE,
        {74, 48, 32, 255}, {32, 20, 14, 255}, {134, 90, 56, 255},
        {172, 138, 80, 255}, {230, 196, 132, 255}, {236, 196, 120, 255},
        {18, 13, 14, 255}, {40, 29, 26, 255},
        {26, 18, 18, 255}, {42, 30, 26, 255},
        {250, 208, 140, 255}, {190, 164, 128, 255}, 92},

    /* ROOF — the last sector: raw concrete plant room behind a curtain wall
     * with the whole city on the other side of the glass. */
    [LEVEL_THEME_ROOF] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_ROOF,
        {68, 72, 78, 255}, {28, 31, 36, 255}, {124, 130, 136, 255},
        {96, 104, 112, 255}, {170, 180, 188, 255}, {110, 200, 220, 255},
        {6, 10, 20, 255}, {18, 26, 42, 255},
        {12, 17, 28, 255}, {20, 28, 42, 255},
        {190, 220, 240, 255}, {140, 170, 196, 255}, 50},

    /* RESTROOM — the sublevel. Its interior is derived from the room's own
     * wall ring in game_render.c; only the tile material comes from here. */
    [LEVEL_THEME_RESTROOM] = {
        WALL_STYLE_TILE, FLOOR_CERAMIC, BACKDROP_RESTROOM,
        {96, 122, 120, 255}, {38, 56, 58, 255}, {186, 206, 196, 255},
        {112, 146, 141, 255}, {203, 211, 196, 255}, {110, 230, 170, 255},
        {7, 13, 22, 255}, {19, 31, 38, 255},
        {13, 23, 31, 255}, {29, 43, 49, 255},
        {202, 235, 222, 255}, {150, 180, 178, 255}, 90},

    /* ---- The four exterior climbs.  Same fields, exterior meanings:
     * air_* is sky, far_shape the distant towers, wall* the building face,
     * trim* the cornice stone, lamp a lit window, accent the signage. */

    /* NIGHT — the first climb: clear, cold, city lights below. */
    [LEVEL_THEME_FACADE_NIGHT] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_NIGHT,
        {43, 43, 47, 255}, {30, 31, 35, 255}, {59, 58, 60, 255},
        {88, 84, 78, 255}, {126, 120, 108, 255}, {228, 54, 48, 255},
        {7, 12, 29, 255}, {25, 32, 48, 255},
        {10, 16, 28, 255}, {70, 69, 55, 255},
        {220, 158, 76, 255}, {155, 194, 218, 255}, 0},

    /* STORM — the second: rain, wet stone and lightning off the skyline. */
    [LEVEL_THEME_FACADE_STORM] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_STORM,
        {36, 38, 42, 255}, {24, 26, 30, 255}, {58, 62, 68, 255},
        {70, 72, 74, 255}, {114, 118, 120, 255}, {236, 84, 64, 255},
        {10, 13, 20, 255}, {32, 36, 44, 255},
        {13, 16, 23, 255}, {58, 62, 66, 255},
        {198, 178, 120, 255}, {170, 196, 214, 255}, 0},

    /* DAWN — the third: the sun comes up mid-climb and the stone goes warm. */
    [LEVEL_THEME_FACADE_DAWN] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_DAWN,
        {86, 72, 64, 255}, {62, 52, 48, 255}, {118, 100, 86, 255},
        {158, 132, 104, 255}, {214, 186, 148, 255}, {232, 96, 72, 255},
        {44, 38, 74, 255}, {236, 150, 96, 255},
        {52, 44, 62, 255}, {120, 96, 74, 255},
        {252, 214, 150, 255}, {252, 206, 158, 255}, 0},

    /* HIGH — the last: above the weather, a sea of cloud below and the
     * building's own neon signage for company. */
    [LEVEL_THEME_FACADE_HIGH] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_HIGH,
        {50, 48, 58, 255}, {34, 33, 42, 255}, {76, 72, 86, 255},
        {104, 100, 112, 255}, {156, 150, 166, 255}, {236, 72, 168, 255},
        {6, 8, 24, 255}, {38, 32, 74, 255},
        {18, 18, 44, 255}, {96, 64, 148, 255},
        {150, 220, 248, 255}, {186, 196, 236, 255}, 0}};

const LevelThemeArt *level_art(LevelTheme theme)
{
    if (theme < 0 || theme >= LEVEL_THEME_COUNT)
        return &THEME_ART[LEVEL_THEME_PLANT];
    return &THEME_ART[theme];
}

/* ---- Scores ---------------------------------------------------------- */

/*
 * What the sector sounds like, decided by the same thing that decides what it
 * looks like. One track per theme, so a floor is never scored by the corridor
 * two sectors down and no two consecutive levels can share a loop — the
 * themes already never repeat back to back
 * (`test_campaign_themes_keep_changing`). The tracks themselves are described
 * in the plan table in audio.c.
 */
static const MusicTrack THEME_MUSIC[LEVEL_THEME_COUNT] = {
    [LEVEL_THEME_PLANT] = MUSIC_PLANT,
    [LEVEL_THEME_LOBBY] = MUSIC_LOBBY,
    [LEVEL_THEME_OFFICE] = MUSIC_OFFICE,
    [LEVEL_THEME_SERVER] = MUSIC_SERVER,
    [LEVEL_THEME_CANTEEN] = MUSIC_CANTEEN,
    [LEVEL_THEME_LAB] = MUSIC_LAB,
    [LEVEL_THEME_ARCHIVE] = MUSIC_ARCHIVE,
    [LEVEL_THEME_SECURITY] = MUSIC_SECURITY,
    [LEVEL_THEME_DUCTS] = MUSIC_DUCTS,
    [LEVEL_THEME_PENTHOUSE] = MUSIC_PENTHOUSE,
    [LEVEL_THEME_ROOF] = MUSIC_ROOF,
    [LEVEL_THEME_RESTROOM] = MUSIC_RESTROOM,
    [LEVEL_THEME_FACADE_NIGHT] = MUSIC_FACADE_NIGHT,
    [LEVEL_THEME_FACADE_STORM] = MUSIC_FACADE_STORM,
    [LEVEL_THEME_FACADE_DAWN] = MUSIC_FACADE_DAWN,
    [LEVEL_THEME_FACADE_HIGH] = MUSIC_FACADE_HIGH};

MusicTrack level_theme_music(LevelTheme theme)
{
    if (theme < 0 || theme >= LEVEL_THEME_COUNT)
        return THEME_MUSIC[LEVEL_THEME_PLANT];
    return THEME_MUSIC[theme];
}

/* ---- Shared helpers -------------------------------------------------- */

static unsigned art_hash(int x, int y)
{
    return fx_hash((unsigned)x * 0x8da6b343u ^ (unsigned)y * 0xd8163841u);
}

/* Deterministic 0..1 from a hash slice, for per-tile and per-prop variation. */
static float art_unit(unsigned h, unsigned shift)
{
    return (float)((h >> shift) & 255u) / 255.0f;
}

/* A layer's scroll offset, always negative so the loop starts off-screen. */
static float art_scroll(float cam_x, float factor, float period)
{
    float shift = fmodf(-cam_x * factor, period);
    return shift > 0.0f ? shift - period : shift;
}

/*
 * The world index of the first repeat a scrolling layer draws; the loop counts
 * up from it.
 *
 * Everything a layer varies per repeat - which blind is shut, which bank of
 * ceiling lights is on, what colour a file spine is - is keyed to this index,
 * so the index has to belong to the repeat rather than to where the repeat
 * currently sits on screen. Recovering it as `(int)(x + cam_x * factor) /
 * period` looks like it does: that sum is the repeat's world position, an exact
 * multiple of the period. In floats it lands a hair either side of the multiple
 * instead, and truncation then hands one repeat two different indices as the
 * camera moves - which is why the backdrop used to boil while the level
 * scrolled rather than sliding with it.
 */
static int art_repeat(float cam_x, float factor, float period)
{
    return (int)floorf(cam_x * factor / period);
}

/*
 * A smooth low-frequency value drift, one value per tile over a four-tile
 * lattice.
 *
 * Per-tile noise on its own makes a wall grainy but no less flat: every tile
 * averages to the same value, so a twenty-tile wall is twenty identical tiles
 * wearing different dirt. Interpolating one value per four tiles gives the
 * surface broad patches of light and shade instead — which is what reads as a
 * real wall standing in real light — and because it resolves to a single value
 * per tile it costs one blend rather than anything per pixel.
 */
static float art_drift(int col, int row)
{
    const float cell = 4.0f;
    float gx = (float)col / cell;
    float gy = (float)row / cell;
    int x0 = (int)floorf(gx);
    int y0 = (int)floorf(gy);
    float tx = gx - (float)x0;
    float ty = gy - (float)y0;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    float top = art_unit(art_hash(x0, y0), 0) +
                (art_unit(art_hash(x0 + 1, y0), 0) -
                 art_unit(art_hash(x0, y0), 0)) *
                    tx;
    float bottom = art_unit(art_hash(x0, y0 + 1), 0) +
                   (art_unit(art_hash(x0 + 1, y0 + 1), 0) -
                    art_unit(art_hash(x0, y0 + 1), 0)) *
                       tx;
    return top + (bottom - top) * ty;
}

/* ---- Tile surroundings ----------------------------------------------- */

/*
 * Which of a tile's neighbours are open air.
 *
 * Nearly everything that stops a tile reading as a 32px stamp is a question
 * about this mask: how deep inside a mass the tile sits, which of its faces
 * point at the light, and whether a slab ends here and therefore has to show
 * its own thickness.
 */
enum
{
    OPEN_UP = 1u << 0,
    OPEN_DOWN = 1u << 1,
    OPEN_LEFT = 1u << 2,
    OPEN_RIGHT = 1u << 3
};

/*
 * How deep inside a solid mass a tile sits, saturating at three.
 *
 * One darkening step for "buried" turns a wide mass into a single flat darker
 * rectangle, which is only half the problem solved. What a mass wants is to
 * fall away from its own surface, so the shell reads as the part standing in
 * the room and the middle reads as the part behind it. Rings are searched
 * outward and the search stops at the first open tile, so the common case — a
 * two-row slab, open above or below — costs one ring.
 */
static int tile_depth(const Level *level, int col, int row)
{
    for (int radius = 1; radius <= 3; ++radius)
    {
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                /* Only the ring itself: everything inside it was searched by
                 * the previous pass. */
                if (dx > -radius && dx < radius && dy > -radius && dy < radius)
                    continue;
                if (!level_is_solid(level, col + dx, row + dy))
                    return radius - 1;
            }
        }
    }
    return 3;
}

static unsigned tile_open_mask(const Level *level, int col, int row)
{
    unsigned mask = 0u;
    if (!level_is_solid(level, col, row - 1))
        mask |= OPEN_UP;
    if (!level_is_solid(level, col, row + 1))
        mask |= OPEN_DOWN;
    if (!level_is_solid(level, col - 1, row))
        mask |= OPEN_LEFT;
    if (!level_is_solid(level, col + 1, row))
        mask |= OPEN_RIGHT;
    return mask;
}

/*
 * Form shading: what turns the material into a lit solid.
 *
 * The material functions describe a surface — plating, brick, ceramic — but a
 * surface with no light on it is a texture swatch, and fifteen sectors of
 * swatches is what a flat tile grid looks like. Three cues on top of the
 * material do the rest, and none of them care which material it was:
 *
 *  - broad patches of light and shade across the whole wall (art_drift),
 *  - a mass falling away from its own surface, so thickness is visible,
 *  - one light direction, from the ceiling fixtures down, so each exposed face
 *    is shaded by the way it points.
 */
static void wall_form_shading(SDL_Renderer *r, const LevelThemeArt *art,
                              int col, int row, float x, float y,
                              unsigned open, int depth)
{
    /* A mass has to fall away from its own face, or how thick it is stays a
     * thing the player can only find out by walking into it. */
    static const Uint8 DEPTH_SHADE[4] = {0u, 36u, 58u, 74u};

    float drift = art_drift(col, row);
    if (drift > 0.56f)
        fx_rect_a(r, art->wall_light, (Uint8)((drift - 0.56f) * 58.0f),
                  x, y, TILE_SIZE, TILE_SIZE);
    else if (drift < 0.44f)
        fx_rect_a(r, FX_INK, (Uint8)((0.44f - drift) * 64.0f),
                  x, y, TILE_SIZE, TILE_SIZE);

    if (depth > 0)
        fx_rect_a(r, FX_INK, DEPTH_SHADE[depth], x, y, TILE_SIZE, TILE_SIZE);

    if (open & OPEN_UP)
        fx_vgrad(r, x, y + 6.0f, TILE_SIZE, 13.0f,
                 art->wall_light, 26, art->wall_light, 0);
    if (open & OPEN_DOWN)
        fx_vgrad(r, x, y + TILE_SIZE - 16.0f, TILE_SIZE, 16.0f,
                 FX_INK, 0, FX_INK, 88);
    if (open & OPEN_LEFT)
        fx_hgrad(r, x, y, 13.0f, TILE_SIZE,
                 art->wall_light, 24, art->wall_light, 0);
    if (open & OPEN_RIGHT)
        fx_hgrad(r, x + TILE_SIZE - 13.0f, y, 13.0f, TILE_SIZE,
                 FX_INK, 0, FX_INK, 68);
}

/*
 * The walkable top of a slab.
 *
 * Every sector's floor used to be the same three-pixel bright line. That line
 * is a legibility cue and nothing else: the one surface Chuck spends an entire
 * level standing on said nothing about which floor of the building he was on.
 * The bright arris stays exactly where it was — where you can stand has to
 * read identically in all fifteen sectors — and the pixels beneath it carry
 * the finish instead.
 */
#define ART_FLOOR_BAND 5.0f

static void floor_finish(SDL_Renderer *r, const LevelThemeArt *art,
                         int col, int row, float x, float y, unsigned h)
{
    const float top = y + 2.0f;
    /* The deck sits below the lit arris, so it is the trim in its own shade
     * rather than another highlight competing with the line above it. */
    SDL_Color deck = fx_mix(art->trim, art->wall_dark, 0.45f);
    SDL_Color joint = fx_mix(art->trim, art->wall_dark, 0.82f);

    switch (art->floor_style)
    {
    case FLOOR_STONE:
        /* Polished stone is dark and gives the light back: the deck sits well
         * under the brass arris so the sheen travelling along it has something
         * to be brighter than. It is the only floor in the building that
         * reflects anything. */
        deck = fx_mix(art->trim, FX_INK, 0.52f);
        fx_rect(r, deck, x, top, TILE_SIZE, ART_FLOOR_BAND);
        fx_vgrad(r, x, top, TILE_SIZE, ART_FLOOR_BAND,
                 art->trim_hi, 44, art->trim_hi, 0);
        fx_rect_a(r, art->trim_hi, 64, x + (float)(h % 11u), top + 1.0f,
                  15.0f, 1.0f);
        if ((col & 1) == 0)
            fx_rect_a(r, FX_INK, 120, x, top, 1.0f, ART_FLOOR_BAND);
        break;

    case FLOOR_CARPET:
        /* Matte, so it takes no specular at all: it is the one floor that has
         * to sit darker than the wall above it, or the office reads as tiled.
         * The nap and the tile joints are the whole of its texture. */
        deck = fx_mix(art->trim, art->wall_dark, 0.8f);
        fx_rect(r, deck, x, top, TILE_SIZE, ART_FLOOR_BAND);
        for (unsigned nap = 0; nap < 5u; ++nap)
        {
            unsigned nh = h >> (nap * 5u);
            fx_rect_a(r, (nh & 1u) ? art->trim : FX_INK, 26,
                      x + (float)(nh % 30u) + 1.0f,
                      top + (float)((nh >> 6) % 4u), 2.0f, 1.0f);
        }
        if ((col & 1) == 0)
            fx_rect_a(r, FX_INK, 70, x, top, 1.0f, ART_FLOOR_BAND);
        break;

    case FLOOR_PANEL:
        /* Raised access floor: every tile is a liftable panel, so the joint is
         * on the tile pitch by definition, and the perforation is what tells
         * you the cold air comes up through it. */
        fx_rect(r, deck, x, top, TILE_SIZE, ART_FLOOR_BAND);
        fx_rect(r, joint, x, top, 1.0f, ART_FLOOR_BAND);
        fx_rect(r, joint, x + TILE_SIZE - 1.0f, top, 1.0f, ART_FLOOR_BAND);
        for (int hole = 0; hole < 6; ++hole)
            fx_rect(r, fx_mix(deck, FX_INK, 0.55f),
                    x + 3.0f + (float)hole * 5.0f, top + 2.0f, 2.0f, 2.0f);
        fx_rect_a(r, art->accent, 40, x + 2.0f, top + 1.0f,
                  TILE_SIZE - 4.0f, 1.0f);
        break;

    case FLOOR_CHEQUER:
        /* Treadplate: raised diamonds, each with a lit top and a shadow under
         * it. Two rows staggered is all the pattern needs at this size. */
        fx_rect(r, fx_mix(deck, art->wall_dark, 0.2f), x, top, TILE_SIZE,
                ART_FLOOR_BAND);
        for (int stud = 0; stud < 5; ++stud)
        {
            float sx = x + 2.0f + (float)stud * 6.0f;
            float sy = top + ((stud & 1) ? 2.0f : 0.0f);
            fx_rect(r, fx_mix(deck, art->trim_hi, 0.45f), sx, sy + 1.0f,
                    4.0f, 1.0f);
            fx_rect(r, fx_mix(deck, FX_INK, 0.5f), sx, sy + 2.0f, 4.0f, 1.0f);
        }
        break;

    case FLOOR_CERAMIC:
        /* Grouted: the joints are the whole read, so they run on the material's
         * own eight-pixel module rather than on the tile grid. */
        fx_rect(r, fx_mix(deck, art->trim_hi, 0.2f), x, top, TILE_SIZE,
                ART_FLOOR_BAND);
        for (int grout = 0; grout < 4; ++grout)
            fx_rect(r, joint, x + (float)grout * 8.0f, top, 1.0f,
                    ART_FLOOR_BAND);
        fx_rect_a(r, art->trim_hi, 46, x + 1.0f, top, 7.0f, 1.0f);
        break;

    case FLOOR_BOARDS:
        /* Timber: butt joints land where they land, and the grain runs the
         * length of the board rather than across it. */
        fx_rect(r, fx_mix(deck, art->wall, 0.3f), x, top, TILE_SIZE,
                ART_FLOOR_BAND);
        fx_rect(r, joint, x + (float)(h % 20u) + 4.0f, top, 1.0f,
                ART_FLOOR_BAND);
        fx_rect_a(r, FX_INK, 44, x, top + 2.0f, TILE_SIZE, 1.0f);
        fx_rect_a(r, art->trim_hi, 30, x + 2.0f, top, TILE_SIZE - 4.0f, 1.0f);
        break;

    case FLOOR_PARQUET:
        /* Blocks laid in alternating pairs, with the brass strip the executive
         * floor puts along everything. */
        for (int block = 0; block < 4; ++block)
        {
            float bx = x + (float)block * 8.0f;
            bool across = ((col * 4 + block + row) & 1) != 0;
            fx_rect(r, fx_mix(deck, art->wall, across ? 0.42f : 0.2f), bx, top,
                    8.0f, ART_FLOOR_BAND);
            fx_rect(r, joint, bx, top, 1.0f, ART_FLOOR_BAND);
            if (across)
                fx_rect_a(r, art->trim_hi, 34, bx + 1.0f, top + 1.0f, 6.0f,
                          1.0f);
        }
        fx_rect_a(r, art->trim_hi, 90, x, top + ART_FLOOR_BAND - 1.0f,
                  TILE_SIZE, 1.0f);
        break;

    case FLOOR_SCREED:
    default:
        /* Power-floated concrete: nothing on it but its own laitance and a saw
         * cut every few metres to tell it where to crack. */
        fx_rect(r, deck, x, top, TILE_SIZE, ART_FLOOR_BAND);
        for (unsigned fleck = 0; fleck < 3u; ++fleck)
        {
            unsigned fh = h >> (fleck * 6u);
            fx_rect_a(r, (fh & 2u) ? art->trim_hi : FX_INK, 40,
                      x + (float)(fh % 29u) + 1.0f,
                      top + (float)((fh >> 7) % 4u), 2.0f, 1.0f);
        }
        if ((col & 3) == 0)
            fx_rect(r, joint, x + 1.0f, top, 1.0f, ART_FLOOR_BAND);
        break;
    }

    /* The nosing shadow: the line that separates the surface you stand on from
     * the face of the slab holding it up. */
    fx_rect_a(r, FX_INK, 130, x, top + ART_FLOOR_BAND, TILE_SIZE, 1.0f);
}

/* ---- Wall materials -------------------------------------------------- */

static void wall_plate(SDL_Renderer *r, const LevelThemeArt *art,
                       int col, int row, float x, float y, unsigned h)
{
    /* Plates span 2x2 tiles: seams and bevels only appear on panel borders,
     * so the wall reads as riveted plating rather than a 32px checkerboard. */
    unsigned ph = art_hash(col >> 1, row >> 1);
    SDL_Color base = fx_mix(art->wall, art->wall_light,
                            (float)(ph % 7u) * 0.018f);
    bool left_edge = (col & 1) == 0;
    bool top_edge = (row & 1) == 0;

    fx_rect(r, base, x, y, TILE_SIZE, TILE_SIZE);

    /* Quiet per-tile wear keeps large walls from feeling machine-stamped. */
    fx_rect(r, fx_mix(base, art->wall_dark, 0.45f),
            x + (float)(h % 21u) + 4.0f, y + (float)((h >> 6) % 22u) + 4.0f,
            4.0f, 2.0f);
    if ((h & 3u) == 0u)
        fx_rect(r, fx_mix(base, art->wall_light, 0.35f),
                x + (float)((h >> 4) % 18u) + 6.0f,
                y + (float)((h >> 9) % 18u) + 8.0f, 6.0f, 1.0f);

    /* Panel bevel: lit top/left edge, shaded bottom/right edge, dark seam. */
    if (top_edge)
    {
        fx_rect(r, art->wall_dark, x, y, TILE_SIZE, 1.0f);
        fx_rect(r, fx_mix(base, art->wall_light, 0.6f), x, y + 1.0f,
                TILE_SIZE, 1.0f);
    }
    else
    {
        fx_rect(r, fx_mix(base, art->wall_dark, 0.3f), x,
                y + TILE_SIZE - 2.0f, TILE_SIZE, 2.0f);
    }
    if (left_edge)
    {
        fx_rect(r, art->wall_dark, x, y, 1.0f, TILE_SIZE);
        fx_rect(r, fx_mix(base, art->wall_light, 0.45f), x + 1.0f, y + 1.0f,
                1.0f, TILE_SIZE - 1.0f);
    }
    else
    {
        fx_rect(r, fx_mix(base, art->wall_dark, 0.24f),
                x + TILE_SIZE - 2.0f, y, 2.0f, TILE_SIZE);
    }

    /* One rivet per panel corner. */
    if (top_edge && left_edge)
    {
        fx_rect(r, fx_mix(art->wall_light, art->trim_hi, 0.5f),
                x + 3.0f, y + 3.0f, 2.0f, 2.0f);
        fx_rect(r, art->wall_dark, x + 4.0f, y + 4.0f, 1.0f, 1.0f);
    }
    if (!top_edge && !left_edge)
        fx_rect(r, art->wall_light, x + TILE_SIZE - 6.0f,
                y + TILE_SIZE - 6.0f, 2.0f, 2.0f);

    /* A bolted stiffener rib every fourth course. The panel grid tells the
     * player how big a panel is; only something on a longer module tells them
     * how big the wall is, and a wall with no scale is what makes a plated
     * corridor read as wallpaper. */
    if ((row & 3) == 0)
    {
        fx_rect(r, fx_mix(base, art->wall_dark, 0.5f), x, y + 22.0f,
                TILE_SIZE, 5.0f);
        fx_rect(r, fx_mix(base, art->wall_light, 0.55f), x, y + 22.0f,
                TILE_SIZE, 1.0f);
        fx_rect(r, fx_mix(base, art->wall_dark, 0.85f), x, y + 27.0f,
                TILE_SIZE, 1.0f);
        for (int bolt = 0; bolt < 3; ++bolt)
            fx_rect(r, fx_mix(art->wall_light, art->trim_hi, 0.3f),
                    x + 5.0f + (float)bolt * 11.0f, y + 24.0f, 2.0f, 2.0f);
    }

    /* Rare full-tile variants: a vent grille or a hairline crack. */
    if ((h % 23u) == 0u)
    {
        for (int slit = 0; slit < 4; ++slit)
        {
            fx_rect(r, art->wall_dark, x + 8.0f, y + 9.0f + (float)slit * 4.0f,
                    16.0f, 2.0f);
            fx_rect(r, fx_mix(base, art->wall_light, 0.5f),
                    x + 8.0f, y + 11.0f + (float)slit * 4.0f, 16.0f, 1.0f);
        }
    }
    else if ((h % 11u) == 0u)
    {
        SDL_Color crack = fx_mix(base, art->wall_dark, 0.6f);
        fx_set(r, crack);
        SDL_RenderLine(r, x + 12.0f, y + 9.0f, x + 16.0f, y + 14.0f);
        SDL_RenderLine(r, x + 16.0f, y + 14.0f, x + 14.0f, y + 20.0f);
        fx_set(r, fx_mix(base, art->wall_light, 0.4f));
        SDL_RenderLine(r, x + 13.0f, y + 9.0f, x + 17.0f, y + 14.0f);
    }
}

static void wall_concrete(SDL_Renderer *r, const LevelThemeArt *art,
                          int col, int row, float x, float y, unsigned h)
{
    /* Poured in lifts: a horizontal shuttering joint every half tile and the
     * form-tie holes the panels were bolted through. */
    SDL_Color base = fx_mix(art->wall, art->wall_dark,
                            art_unit(h, 3) * 0.14f);
    fx_rect(r, base, x, y, TILE_SIZE, TILE_SIZE);

    if ((row & 1) == 0)
    {
        fx_rect(r, fx_mix(base, art->wall_dark, 0.55f), x, y, TILE_SIZE, 1.0f);
        fx_rect(r, fx_mix(base, art->wall_light, 0.35f), x, y + 1.0f,
                TILE_SIZE, 1.0f);
    }
    /* Every fourth course is where one day's pour met the next: a deeper
     * recess with the grout that leaked out of it, and the only line on the
     * wall that belongs to the building rather than to the shuttering. */
    bool day_joint = (row & 3) == 0;
    fx_rect(r, fx_mix(base, art->wall_dark, day_joint ? 0.62f : 0.25f), x,
            y + 16.0f, TILE_SIZE, day_joint ? 2.0f : 1.0f);
    if (day_joint)
        fx_rect(r, fx_mix(base, art->wall_light, 0.4f), x, y + 18.0f,
                TILE_SIZE, 1.0f);

    /* Aggregate speckle: three flecks is enough to break up a flat pour. */
    for (unsigned fleck = 0; fleck < 3u; ++fleck)
    {
        unsigned fh = h >> (fleck * 7u);
        fx_rect(r, fx_mix(base, (fh & 1u) ? art->wall_light : art->wall_dark,
                          0.3f),
                x + (float)(fh % 27u) + 2.0f,
                y + (float)((fh >> 5) % 27u) + 2.0f, 2.0f, 2.0f);
    }

    if ((col & 1) == 0 && (row & 1) == 0)
    {
        /* Form ties come in pairs across the panel, plugged and stained. */
        for (int tie = 0; tie < 2; ++tie)
        {
            float tx = x + 7.0f + (float)tie * 34.0f;
            fx_rect(r, fx_mix(base, art->wall_dark, 0.7f), tx, y + 12.0f,
                    3.0f, 3.0f);
            fx_rect(r, fx_mix(base, art->wall_dark, 0.28f), tx, y + 15.0f,
                    3.0f, 7.0f);
        }
    }

    if ((h % 17u) == 0u)
    {
        /* Water staining runs down from a joint; it always starts at one. */
        fx_rect_a(r, art->wall_dark, 70, x + (float)(h % 20u) + 4.0f, y + 16.0f,
                  5.0f, TILE_SIZE - 16.0f);
    }
    else if ((h % 13u) == 0u)
    {
        fx_set(r, fx_mix(base, art->wall_dark, 0.5f));
        SDL_RenderLine(r, x + 9.0f, y + 4.0f, x + 13.0f, y + 13.0f);
        SDL_RenderLine(r, x + 13.0f, y + 13.0f, x + 11.0f, y + 24.0f);
    }
}

static void wall_tile_material(SDL_Renderer *r, const LevelThemeArt *art,
                               int row, float x, float y, unsigned h)
{
    /* Glazed 8px tiles on a grout bed. The sheen on each tile's top-left is
     * what separates ceramic from flat paint at this size. */
    fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.55f), x, y,
            TILE_SIZE, TILE_SIZE);
    for (int ty = 0; ty < 4; ++ty)
    {
        for (int tx = 0; tx < 4; ++tx)
        {
            unsigned th = h >> ((unsigned)(ty * 4 + tx) & 15u);
            float px = x + (float)tx * 8.0f + 1.0f;
            float py = y + (float)ty * 8.0f + 1.0f;
            SDL_Color face = fx_mix(art->wall, art->wall_light,
                                    art_unit(th, 2) * 0.22f);
            if ((th % 29u) == 0u)
                face = fx_mix(face, art->wall_dark, 0.45f); /* a dead tile */
            fx_rect(r, face, px, py, 6.0f, 6.0f);
            fx_rect(r, fx_mix(face, art->wall_light, 0.5f), px, py, 6.0f, 1.0f);
            fx_rect(r, fx_mix(face, art->wall_dark, 0.3f), px, py + 5.0f,
                    6.0f, 1.0f);
            if ((th % 7u) == 0u)
                fx_rect_a(r, art->trim_hi, 90, px + 1.0f, py + 1.0f, 2.0f, 2.0f);
        }
    }
    /* A border course every fourth row. Without one, a tiled wall is an even
     * field of eight-pixel squares whatever its size, which is exactly what
     * makes the clean sectors read as graph paper; with one, the tiling has
     * been set out by somebody. The band takes the theme's accent, so the lab
     * gets a green line and the galley an amber one. */
    if ((row & 3) == 3)
    {
        SDL_Color band = fx_mix(art->wall, art->accent, 0.4f);
        fx_rect(r, fx_mix(band, art->wall_dark, 0.45f), x, y + 16.0f,
                TILE_SIZE, 8.0f);
        for (int strip = 0; strip < 4; ++strip)
        {
            float sx = x + (float)strip * 8.0f + 1.0f;
            fx_rect(r, band, sx, y + 17.0f, 6.0f, 6.0f);
            fx_rect(r, fx_mix(band, art->wall_light, 0.45f), sx, y + 17.0f,
                    6.0f, 1.0f);
            fx_rect(r, fx_mix(band, art->wall_dark, 0.4f), sx, y + 22.0f,
                    6.0f, 1.0f);
        }
    }

    if ((h % 19u) == 0u)
    {
        /* A cracked tile, drawn across the grout so it reads as damage. */
        fx_set(r, fx_mix(art->wall, art->wall_dark, 0.8f));
        SDL_RenderLine(r, x + 6.0f, y + 7.0f, x + 14.0f, y + 17.0f);
        SDL_RenderLine(r, x + 14.0f, y + 17.0f, x + 11.0f, y + 26.0f);
    }
}

/*
 * One vein, drawn in slab space and clipped to the tile being drawn.
 *
 * Veining is what separates stone from grey paint, and it is also the fastest
 * way to give the whole lobby away as a stamp: one ruled diagonal per tile,
 * every tile, leaning the same way. A vein has to wander, has to sit under the
 * polish rather than being scratched on top of it, and above all has to belong
 * to the slab — so the two tiles sharing a slab agree about where it goes and
 * the joint is the only line that repeats.
 */
static void marble_vein(SDL_Renderer *r, SDL_Color c, Uint8 alpha,
                        float tile_x, float slab_x, float y,
                        float start, float slope, float width, unsigned h)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, alpha);
    float px = slab_x + start;
    for (int step = 0; step < TILE_SIZE / 2; ++step)
    {
        float t = (float)step * 2.0f;
        unsigned sh = h >> ((unsigned)step & 15u);
        px += slope * 2.0f + ((float)(sh & 3u) - 1.5f) * 0.7f;
        float left = px < tile_x ? tile_x : px;
        float right = px + width > tile_x + TILE_SIZE ? tile_x + TILE_SIZE
                                                      : px + width;
        if (right > left)
            fx_fill(r, left, y + t, right - left, 2.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void wall_marble(SDL_Renderer *r, const LevelThemeArt *art,
                        int col, int row, float x, float y, unsigned h)
{
    /* Slabs are two tiles wide, so the joint pattern is architectural rather
     * than tile-sized, and every slab is polished top to bottom. */
    unsigned sh = art_hash(col >> 1, row);
    SDL_Color base = fx_mix(art->wall, art->wall_light,
                            art_unit(sh, 1) * 0.25f);
    fx_rect(r, base, x, y, TILE_SIZE, TILE_SIZE);
    fx_vgrad(r, x, y, TILE_SIZE, TILE_SIZE,
             fx_mix(base, art->wall_light, 0.35f), 255,
             fx_mix(base, art->wall_dark, 0.25f), 255);

    /* Two or three veins per slab: a broad soft one that carries the figure,
     * a fine bright one beside it, and a dark counter-vein leaning the other
     * way. Which way the slab was cut comes off its own hash, so consecutive
     * slabs are not combed in the same direction. */
    float slab_x = x - (float)(col & 1) * TILE_SIZE;
    float lean = (sh & 16u) ? 1.0f : -1.0f;
    float slope = lean * (0.28f + art_unit(sh, 11) * 0.5f);
    float start = 8.0f + art_unit(sh, 5) * 44.0f;
    marble_vein(r, fx_mix(base, art->wall_light, 0.55f), 90, x, slab_x, y,
                start, slope, 4.0f, sh);
    marble_vein(r, fx_mix(base, art->wall_light, 0.95f), 130, x, slab_x, y,
                start + 2.0f, slope, 1.0f, sh >> 3);
    marble_vein(r, fx_mix(base, art->wall_dark, 0.55f), 80, x, slab_x, y,
                start + 22.0f + art_unit(sh, 19) * 16.0f, -slope * 0.7f, 2.0f,
                sh >> 7);

    /* Slab joints: a fine dark line, lit on the near side. */
    if ((col & 1) == 0)
    {
        fx_rect(r, fx_mix(base, art->wall_dark, 0.7f), x, y, 1.0f, TILE_SIZE);
        fx_rect(r, fx_mix(base, art->wall_light, 0.5f), x + 1.0f, y,
                1.0f, TILE_SIZE);
    }
    fx_rect(r, fx_mix(base, art->wall_dark, 0.6f), x, y, TILE_SIZE, 1.0f);

    /* Every third course carries a brass reveal — the lobby's one flourish. */
    if ((row % 3) == 1)
    {
        fx_rect(r, art->trim, x, y + 22.0f, TILE_SIZE, 3.0f);
        fx_rect(r, art->trim_hi, x, y + 22.0f, TILE_SIZE, 1.0f);
    }
    if ((h % 31u) == 0u)
        fx_rect_a(r, art->trim_hi, 60, x + 6.0f, y + 5.0f, 12.0f, 3.0f);
}

static void wall_drywall(SDL_Renderer *r, const LevelThemeArt *art,
                         int col, int row, float x, float y, unsigned h)
{
    /* Painted partition boarding: broad flat faces, a taped joint every two
     * tiles, and the odd socket or notice to give the flat a sense of scale. */
    SDL_Color base = fx_mix(art->wall, art->wall_light,
                            art_unit(art_hash(col >> 1, row >> 1), 4) * 0.12f);
    fx_rect(r, base, x, y, TILE_SIZE, TILE_SIZE);
    fx_vgrad(r, x, y, TILE_SIZE, TILE_SIZE,
             fx_mix(base, art->wall_light, 0.18f), 255, base, 255);

    if ((col & 1) == 0)
    {
        fx_rect(r, fx_mix(base, art->wall_dark, 0.35f), x, y, 2.0f, TILE_SIZE);
        fx_rect(r, fx_mix(base, art->wall_light, 0.3f), x + 2.0f, y,
                1.0f, TILE_SIZE);
    }
    if ((row & 1) == 0)
        fx_rect(r, fx_mix(base, art->wall_dark, 0.22f), x, y, TILE_SIZE, 1.0f);

    /* A shadow-gap reveal every third course, with the aluminium trim it is
     * formed with. It is the one line a plasterboard partition has, and a
     * broad flat wall with no line on it has no size. */
    if ((row % 3) == 2)
    {
        fx_rect(r, fx_mix(base, FX_INK, 0.6f), x, y + 24.0f, TILE_SIZE, 2.0f);
        fx_rect(r, fx_mix(base, art->wall_light, 0.65f), x, y + 26.0f,
                TILE_SIZE, 1.0f);
    }

    if ((h % 37u) == 0u)
    {
        /* Socket and switch plate. */
        fx_rect(r, fx_mix(base, art->wall_light, 0.55f), x + 12.0f, y + 12.0f,
                9.0f, 11.0f);
        fx_rect(r, art->wall_dark, x + 15.0f, y + 15.0f, 3.0f, 2.0f);
        fx_rect(r, art->wall_dark, x + 15.0f, y + 19.0f, 3.0f, 2.0f);
    }
    else if ((h % 23u) == 0u)
    {
        /* A notice taped to the wall — the only paper anyone reads here. */
        fx_rect(r, art->trim_hi, x + 9.0f, y + 8.0f, 14.0f, 18.0f);
        for (int line = 0; line < 4; ++line)
            fx_rect(r, fx_mix(art->trim_hi, art->wall_dark, 0.55f),
                    x + 11.0f, y + 12.0f + (float)line * 4.0f, 10.0f, 1.0f);
        fx_rect(r, art->accent, x + 11.0f, y + 10.0f, 6.0f, 1.0f);
    }
    else if ((h % 17u) == 0u)
    {
        /* Scuffing along the traffic line, which is always waist height. */
        fx_rect_a(r, art->wall_dark, 60, x, y + 18.0f, TILE_SIZE, 3.0f);
    }
}

static void wall_brick(SDL_Renderer *r, const LevelThemeArt *art,
                       int col, int row, float x, float y)
{
    /* Stretcher bond: four courses per tile, offset half a brick each course,
     * so the pattern runs continuously across the whole wall. */
    fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.75f), x, y,
            TILE_SIZE, TILE_SIZE);
    for (int course = 0; course < 4; ++course)
    {
        int world_course = row * 4 + course;
        float by = y + (float)course * 8.0f;
        float offset = (world_course & 1) ? 8.0f : 0.0f;
        /* Bricks are indexed in world space, not screen space, so a brick
         * keeps its colour as the camera moves and the bond lines up with
         * the neighbouring tile instead of restarting at every tile edge. */
        for (int slot = -1; slot <= 1; ++slot)
        {
            int brick = col * 2 + slot;
            float bx = x + (float)slot * 16.0f + offset;
            unsigned bh = art_hash(brick, world_course);
            SDL_Color face = fx_mix(art->wall, art->wall_light,
                                    art_unit(bh, 3) * 0.3f);
            if ((bh % 11u) == 0u)
                face = fx_mix(face, art->wall_dark, 0.4f);
            float left = bx > x ? bx : x;
            float right = bx + 15.0f < x + TILE_SIZE ? bx + 15.0f
                                                     : x + TILE_SIZE;
            if (right <= left)
                continue;
            fx_rect(r, face, left, by + 1.0f, right - left, 6.0f);
            fx_rect(r, fx_mix(face, art->wall_light, 0.35f), left, by + 1.0f,
                    right - left, 1.0f);
        }
    }
    /* A header course every fifth row: the string course any real brick shell
     * has, and the thing that stops twenty courses of stretcher bond reading
     * as wallpaper. Bricks laid end-on are half as wide and sit forward, so
     * the course catches the light along its whole length. */
    if ((row % 5) == 4)
    {
        fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.45f), x, y + 24.0f,
                TILE_SIZE, 8.0f);
        for (int header = 0; header < 4; ++header)
        {
            float hx = x + (float)header * 8.0f + 1.0f;
            unsigned hh = art_hash(col * 4 + header, row * 4 + 7);
            fx_rect(r, fx_mix(art->wall, art->wall_light,
                              art_unit(hh, 3) * 0.34f),
                    hx, y + 25.0f, 6.0f, 6.0f);
            fx_rect(r, fx_mix(art->wall, art->wall_light, 0.5f), hx, y + 25.0f,
                    6.0f, 1.0f);
        }
    }

    /* Efflorescence and soot: the two things that age brick. */
    unsigned h = art_hash(col, row);
    if ((h % 13u) == 0u)
        fx_rect_a(r, art->trim_hi, 40, x + 4.0f, y + 6.0f, 14.0f, 12.0f);
    else if ((h % 19u) == 0u)
        fx_rect_a(r, art->wall_dark, 90, x + 8.0f, y, 16.0f, TILE_SIZE);
}

static void wall_wood(SDL_Renderer *r, const LevelThemeArt *art,
                      int col, int row, float x, float y, unsigned h)
{
    /* Stile-and-rail panelling: a raised panel inside a frame, with the grain
     * running vertically the way real panelling is cut. */
    SDL_Color base = fx_mix(art->wall, art->wall_light,
                            art_unit(art_hash(col, row >> 1), 2) * 0.2f);
    fx_rect(r, base, x, y, TILE_SIZE, TILE_SIZE);

    SDL_Color inset = fx_mix(base, art->wall_dark, 0.35f);
    fx_rect(r, inset, x + 4.0f, y + 3.0f, TILE_SIZE - 8.0f, TILE_SIZE - 6.0f);
    fx_rect(r, fx_mix(base, art->wall_light, 0.5f), x + 4.0f, y + 3.0f,
            TILE_SIZE - 8.0f, 1.0f);
    fx_rect(r, fx_mix(base, art->wall_dark, 0.6f), x + 4.0f,
            y + TILE_SIZE - 4.0f, TILE_SIZE - 8.0f, 1.0f);

    for (int grain = 0; grain < 4; ++grain)
    {
        unsigned gh = h >> (unsigned)(grain * 5);
        fx_rect(r, fx_mix(inset, (gh & 1u) ? art->wall_light : art->wall_dark,
                          0.22f),
                x + 6.0f + (float)(gh % 19u), y + 4.0f,
                1.0f, TILE_SIZE - 8.0f);
    }

    /* Vertical stiles every other column keep the panels reading as joinery. */
    if ((col & 1) == 0)
    {
        fx_rect(r, fx_mix(base, art->wall_dark, 0.5f), x, y, 2.0f, TILE_SIZE);
        fx_rect(r, fx_mix(base, art->wall_light, 0.4f), x + 2.0f, y,
                1.0f, TILE_SIZE);
    }
    if ((row % 3) == 0)
    {
        fx_rect(r, art->trim, x, y + 1.0f, TILE_SIZE, 2.0f);
        fx_rect(r, art->trim_hi, x, y + 1.0f, TILE_SIZE, 1.0f);
    }
}

/*
 * A blocked-up opening.
 *
 * A weak wall has to say two things at a glance: that it is a wall, and that it
 * is not the wall the rest of the sector was built out of. The material under
 * it stays the theme's, so the patch belongs to the room it is in; over that
 * goes the reveal all the way round — the joint where somebody filled an
 * opening in — and blockwork on a module twice as coarse as any of the seven
 * materials, because a change of scale reads as different work where a change
 * of colour would just read as a dirty tile. The cracks are the affordance:
 * this is the one tile in the grid drawn cracked, and it is where it goes.
 */
static void wall_weak_patch(SDL_Renderer *r, const LevelThemeArt *art,
                            int col, int row, float x, float y, unsigned h)
{
    /* The joint it was let into, dark and unbroken, so the fill reads as
     * sitting inside a hole rather than as paint over one. */
    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.55f), x + 1.0f, y + 1.0f,
            TILE_SIZE - 2.0f, TILE_SIZE - 2.0f);
    /* The mortar the blocks are bedded in sits well back from them: every joint
     * in the patch is this colour showing through, and a joint the same value as
     * the block beside it leaves the coarse module invisible — which is the
     * whole point of drawing blockwork instead of another swatch. */
    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.30f), x + 3.0f, y + 3.0f,
            TILE_SIZE - 6.0f, TILE_SIZE - 6.0f);

    /* Two courses of blockwork, offset half a block, indexed in world space so
     * a run of patches side by side stays one piece of work. */
    for (int course = 0; course < 2; ++course)
    {
        float by = y + 4.0f + (float)course * 12.5f;
        float offset = course ? -4.5f : 0.0f;
        for (int slot = 0; slot < 4; ++slot)
        {
            unsigned bh = art_hash(col * 4 + slot, row * 2 + course);
            SDL_Color face = fx_mix(art->wall, art->wall_light,
                                    0.14f + art_unit(bh, 3) * 0.26f);
            float left = x + 3.0f + (float)slot * 9.0f + offset;
            float right = left + 8.0f;
            if (left < x + 3.0f)
                left = x + 3.0f;
            if (right > x + TILE_SIZE - 3.0f)
                right = x + TILE_SIZE - 3.0f;
            if (right <= left)
                continue;
            fx_rect(r, face, left, by, right - left, 9.5f);
            /* Each block is its own little solid: lit along the top it was
             * laid to and in shade along the bed under it. */
            fx_rect(r, fx_mix(face, art->wall_light, 0.55f), left, by,
                    right - left, 1.0f);
            fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.45f), left,
                    by + 8.5f, right - left, 1.0f);
        }
    }

    /*
     * Cracks, and they run across the joints rather than along them: a fracture
     * that stops at every block edge is a block edge. Each is laid as short
     * steps, because a straight line reads as scoring, and each carries one
     * bright pixel of spalled edge beside it — the same reason a limb gets a lit
     * pixel along the top. At thirty-two pixels a dark line alone is dirt.
     */
    for (int i = 0; i < 3; ++i)
    {
        unsigned ch = h >> (unsigned)(i * 6);
        float cx = x + 6.0f + (float)(ch % 17u);
        float cy = y + 5.0f + (float)((ch >> 5) % 10u);
        int steps = 3 + (int)((ch >> 9) % 2u);
        float lean = ((ch >> 11) & 1u) ? 1.0f : -1.0f;
        for (int step = 0; step < steps; ++step)
        {
            fx_rect(r, fx_mix(art->wall_light, art->wall, 0.30f), cx - lean, cy,
                    1.0f, 3.0f);
            fx_rect(r, FX_INK, cx, cy, 1.0f, 3.0f);
            cy += 3.0f;
            cx += lean;
            if (((ch >> (unsigned)(step + 12)) & 1u) != 0u)
                fx_rect(r, FX_INK, cx, cy, 2.0f, 1.0f);
        }
    }
}

void level_art_broken_wall_tile(SDL_Renderer *r, const Level *level,
                                int col, int row, float x, float y)
{
    /* Rubble only rests on something. A hole blown through a wall with air
     * under it keeps nothing, which is right: it all went to the floor below. */
    if (!level_is_solid(level, col, row + 1))
        return;

    const LevelThemeArt *art = level_art(level->map.theme);
    unsigned h = art_hash(col, row);
    SDL_Color base = fx_mix(art->wall_dark, FX_INK, 0.35f);

    fx_rect(r, base, x + 1.0f, y + TILE_SIZE - 4.0f, TILE_SIZE - 2.0f, 4.0f);
    for (int chip = 0; chip < 5; ++chip)
    {
        unsigned chh = h >> (unsigned)(chip * 5);
        float cw = 3.0f + (float)(chh % 3u);
        float ch_h = 2.0f + (float)((chh >> 3) % 4u);
        float cx = x + 2.0f + (float)((chh >> 6) % 25u);
        if (cx + cw > x + TILE_SIZE - 1.0f)
            cx = x + TILE_SIZE - 1.0f - cw;
        fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.55f), cx,
                y + TILE_SIZE - 3.0f - ch_h, cw, ch_h);
        fx_rect(r, fx_mix(art->wall, art->wall_light, 0.22f), cx,
                y + TILE_SIZE - 3.0f - ch_h, cw, 1.0f);
    }
    /* Dust still hanging in the opening, so the hole is not simply air. */
    fx_vgrad(r, x, y + TILE_SIZE - 14.0f, TILE_SIZE, 11.0f,
             art->haze, 0, art->haze, 46);
}

void level_art_wall_tile(SDL_Renderer *r, const Level *level,
                         int col, int row, float x, float y)
{
    const LevelThemeArt *art = level_art(level->map.theme);
    unsigned h = art_hash(col, row);

    switch (art->wall_style)
    {
    case WALL_STYLE_CONCRETE:
        wall_concrete(r, art, col, row, x, y, h);
        break;
    case WALL_STYLE_TILE:
        wall_tile_material(r, art, row, x, y, h);
        break;
    case WALL_STYLE_MARBLE:
        wall_marble(r, art, col, row, x, y, h);
        break;
    case WALL_STYLE_DRYWALL:
        wall_drywall(r, art, col, row, x, y, h);
        break;
    case WALL_STYLE_BRICK:
        wall_brick(r, art, col, row, x, y);
        break;
    case WALL_STYLE_WOOD:
        wall_wood(r, art, col, row, x, y, h);
        break;
    case WALL_STYLE_PLATE:
    default:
        wall_plate(r, art, col, row, x, y, h);
        break;
    }

    /* A patched opening is drawn over its sector's own material, so the wall it
     * was let into still belongs to this floor of the building — and it goes
     * before the shading, because it is masonry to be lit and not a decal. */
    if (level_tile(level, col, row) == TILE_WEAK_WALL)
        wall_weak_patch(r, art, col, row, x, y, h);

    /* Form shading goes over the material and under the edges: the arris is a
     * highlight, and a highlight that gets dimmed by the shading pass stops
     * being one. */
    unsigned open = tile_open_mask(level, col, row);
    wall_form_shading(r, art, col, row, x, y, open,
                      tile_depth(level, col, row));

    /* Edge treatment is shared by every material: the surfaces the player
     * actually stands on, walks past and jumps under have to read the same
     * way in all fifteen sectors or the level stops being legible. */
    if (open & OPEN_UP)
    {
        fx_rect(r, art->trim, x, y, TILE_SIZE, 2.0f);
        floor_finish(r, art, col, row, x, y, h);
        fx_rect(r, art->trim_hi, x + 1.0f, y, TILE_SIZE - 2.0f, 1.0f);
        if ((h & 3u) == 0u)
            fx_rect(r, art->accent, x + (float)(h % 14u) + 4.0f, y + 1.0f,
                    9.0f, 1.0f);
    }
    if (open & OPEN_DOWN)
    {
        /* The soffit: a dark line for the shadow it sits in, and one dim line
         * of bounce above it so the underside is a surface rather than a hole
         * cut in the level. */
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.55f), x,
                y + TILE_SIZE - 2.0f, TILE_SIZE, 2.0f);
        fx_rect_a(r, art->wall_light, 40, x, y + TILE_SIZE - 3.0f,
                  TILE_SIZE, 1.0f);
    }
    if (open & OPEN_LEFT)
    {
        fx_rect(r, fx_mix(art->wall_light, art->trim, 0.4f), x, y,
                2.0f, TILE_SIZE);
        fx_rect_a(r, FX_INK, 60, x + 2.0f, y, 1.0f, TILE_SIZE);
    }
    if (open & OPEN_RIGHT)
    {
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.25f),
                x + TILE_SIZE - 2.0f, y, 2.0f, TILE_SIZE);
        fx_rect_a(r, art->wall_light, 34, x + TILE_SIZE - 3.0f, y,
                  1.0f, TILE_SIZE);
    }

    /* A slab that stops in mid-air has to show how thick it is: the lip
     * returns a short way down the exposed flank instead of ending dead at the
     * tile boundary, which is the difference between a ledge Chuck can stand
     * on and a rectangle that happens to be lighter along the top. */
    if ((open & OPEN_UP) && (open & OPEN_LEFT))
    {
        fx_rect(r, art->trim, x, y, 3.0f, ART_FLOOR_BAND + 3.0f);
        fx_rect(r, art->trim_hi, x, y, 1.0f, ART_FLOOR_BAND + 1.0f);
    }
    if ((open & OPEN_UP) && (open & OPEN_RIGHT))
    {
        fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.35f),
                x + TILE_SIZE - 3.0f, y, 3.0f, ART_FLOOR_BAND + 3.0f);
        fx_rect(r, art->trim_hi, x + TILE_SIZE - 3.0f, y, 3.0f, 1.0f);
    }
}

/* ---- Interior backdrops ---------------------------------------------- */

/* Fine airborne dust, shared by every interior: it is what stops an empty
 * room from looking like a still image. */
static void backdrop_motes(const LevelArtScene *s, const LevelThemeArt *art,
                           float oy, int count)
{
    SDL_Renderer *r = s->renderer;
    float span = (float)(s->win_w + 80);
    /* A window can be resized down to nothing; the drift band must stay at
     * least one pixel or the modulo below divides by zero. */
    int band = s->win_h - (int)oy - 36;
    if (band < 1)
        band = 1;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < count; ++i)
    {
        unsigned h = art_hash(i + s->level_index * 29, 91);
        float x = fmodf((float)(h % (unsigned)span) +
                            s->time * (4.0f + (float)(i % 5)),
                        span) -
                  40.0f;
        float y = oy + 18.0f + (float)((h >> 8) % (unsigned)band) +
                  sinf(s->time * 0.8f + (float)i) * 2.0f;
        SDL_SetRenderDrawColor(r, art->haze.r, art->haze.g, art->haze.b,
                               (Uint8)(22 + (h % 20u)));
        fx_fill(r, x, y, (i % 3 == 0) ? 2.0f : 1.0f, 1.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void backdrop_plant(const LevelArtScene *s, const LevelThemeArt *art,
                           float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* Farthest layer: hulking silhouettes of plant machinery, barely lit. */
    int bank = art_repeat(s->cam_x, 0.10f, 224.0f);
    for (float x = art_scroll(s->cam_x, 0.10f, 224.0f) - 48.0f;
         x < (float)s->win_w + 224.0f; x += 224.0f, ++bank)
    {
        unsigned seed = (unsigned)bank;
        unsigned h = fx_hash(seed * 31u + (unsigned)s->level_index * 7u);
        float tank_h = 96.0f + (float)(h % 60u);
        fx_rect(r, art->far_shape, x + 24.0f, oy + fh - tank_h, 74.0f, tank_h);
        fx_rect(r, fx_mix(art->far_shape, art->near_shape, 0.5f),
                x + 30.0f, oy + fh - tank_h - 14.0f, 62.0f, 14.0f);
        fx_rect(r, art->far_shape, x + 128.0f, oy + fh - 64.0f, 70.0f, 64.0f);
        fx_rect(r, fx_mix(art->far_shape, art->near_shape, 0.3f),
                x + 150.0f, oy + 34.0f, 10.0f, fh - 98.0f);
        float blink = sinf(s->time * 1.4f + (float)(h % 7u)) > 0.86f ? 1.0f
                                                                    : 0.28f;
        /* The stack's warning light is FX_RED knocked back by distance. */
        fx_rect(r, fx_dim(FX_RED, 0.85f * blink),
                x + 153.0f, oy + 30.0f, 4.0f, 3.0f);
    }

    /* Mid layer: service bays with pilasters, pipe runs and dim windows. */
    for (float x = art_scroll(s->cam_x, 0.18f, 192.0f) - 32.0f;
         x < (float)s->win_w + 192.0f; x += 192.0f)
    {
        fx_rect(r, art->near_shape, x + 16.0f, oy + 26.0f, 144.0f, fh - 44.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.35f),
                x + 16.0f, oy + 26.0f, 144.0f, 3.0f);
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.3f),
                x + 28.0f, oy + 52.0f, 120.0f, 78.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.5f),
                x + 28.0f, oy + 52.0f, 120.0f, 2.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.2f),
                x + 16.0f, oy + 146.0f, 144.0f, 5.0f);
        for (int b = 0; b < 3; ++b)
            fx_rect(r, art->far_shape, x + 34.0f + (float)b * 48.0f,
                    oy + 130.0f, 3.0f, 16.0f);

        /* Dim equipment lights; the warm ones flicker very rarely. */
        for (int lamp = 0; lamp < 4; ++lamp)
        {
            unsigned h = art_hash(bank * 4 + lamp, s->level_index + 7);
            bool warm = (h & 1u) != 0u;
            float flicker = warm && ((h >> 3) & 7u) == 0u &&
                                    fmodf(s->time * 1.7f + (float)lamp, 4.0f) <
                                        0.09f
                                ? 0.3f
                                : 1.0f;
            /* Warm fittings are street sodium, cool ones the fluorescent
             * lamp, both dimmed to backdrop distance. */
            SDL_Color lc = warm
                               ? fx_dim(FX_SODIUM, 0.70f * flicker)
                               : fx_dim(FX_LAMP, 0.45f);
            fx_rect(r, lc, x + 36.0f + (float)lamp * 26.0f, oy + 70.0f,
                    12.0f, 4.0f);
            fx_rect_a(r, lc, 26, x + 34.0f + (float)lamp * 26.0f, oy + 68.0f,
                      16.0f, 8.0f);
        }
    }

    /* Slow volumetric light shafts falling between the far bays. */
    for (float x = art_scroll(s->cam_x, 0.22f, 384.0f) - 96.0f;
         x < (float)s->win_w + 384.0f; x += 384.0f)
    {
        float sway = sinf(s->time * 0.21f + x * 0.01f) * 14.0f;
        fx_light_cone(r, x + 150.0f + sway, oy - 6.0f, 14.0f, 52.0f, fh * 0.9f,
                      FX_LAMP, 13);
    }

    /* Near layer: wall seams and conduit shadows at a faster parallax. */
    for (float x = art_scroll(s->cam_x, 0.30f, 96.0f);
         x < (float)s->win_w + 96.0f; x += 96.0f)
    {
        fx_rect_a(r, fx_mix(art->near_shape, art->wall, 0.5f), 130, x, oy,
                  2.0f, fh);
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.2f), x + 66.0f, oy + 8.0f,
                4.0f, 86.0f);
        fx_rect(r, fx_mix(art->wall, art->wall_light, 0.4f), x + 67.0f,
                oy + 8.0f, 1.0f, 86.0f);
    }
}

/*
 * The way in off the street, drawn once.
 *
 * Everything else on this facade tiles, because a curtain wall genuinely
 * repeats along a building. An entrance does not — a lobby has one — so it is
 * anchored to a fixed point on the glazing layer instead of being stamped
 * along it, and the anchor is a multiple of the mullion pitch so the jambs
 * land on the curtain wall's own grid. The portal then takes over two bays of
 * glass rather than cutting across the middle of them.
 *
 * It also has to stay the dark half of the picture: the light belongs to the
 * canopy soffit above the doors, not to the street behind them, or the
 * entrance turns into a lamp standing at the back of the hall.
 */
static void lobby_entrance(const LevelArtScene *s, const LevelThemeArt *art,
                           float street)
{
    SDL_Renderer *r = s->renderer;
    /* The mullions' own parallax and pitch: 192 is two of their bays. */
    const float span = 192.0f;
    float ex = -s->cam_x * 0.14f + 96.0f;
    float cx = ex + span * 0.5f;
    float base = street + 10.0f;      /* threshold, sat on the glazing sill */
    float fascia = street - 88.0f;    /* underside of the transom */
    float head = street - 76.0f;      /* door head; the gap is the canopy */
    float opening = base - head;
    SDL_Color jamb = fx_mix(art->near_shape, FX_INK, 0.55f);
    SDL_Color glass = fx_mix(art->far_shape, FX_INK, 0.35f);

    /* The reveal: jambs and head only. An entrance is a hole with depth, but
     * filling it in is what turned the doors into a black panel bolted onto
     * the glazing — the leaves have to keep showing the same city the rest of
     * the wall shows, or the way out reads as the one place with no outside
     * behind it. */
    fx_rect(r, FX_INK, ex - 3.0f, fascia, 3.0f, base + 6.0f - fascia);
    fx_rect(r, FX_INK, ex + span, fascia, 3.0f, base + 6.0f - fascia);

    /* Canopy soffit and the strip light under it. */
    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.3f), ex, fascia, span,
            head - fascia);
    fx_rect(r, art->trim, ex, fascia, span, 2.0f);
    fx_rect_a(r, art->trim_hi, 150, ex, fascia, span, 1.0f);
    fx_rect(r, art->lamp, ex + 8.0f, head - 3.0f, span - 16.0f, 2.0f);

    /* Jambs, on the mullion grid and the same width as one. */
    fx_rect(r, jamb, ex, head, 8.0f, opening);
    fx_rect(r, jamb, ex + span - 8.0f, head, 8.0f, opening);
    fx_rect_a(r, art->trim, 110, ex + 2.0f, head, 4.0f, opening);
    fx_rect_a(r, art->trim, 110, ex + span - 6.0f, head, 4.0f, opening);

    /* A fixed sidelight on one flank and a swing leaf on the other — the door
     * a lobby keeps beside the drum for whatever will not fit through it. The
     * stile, the mid rail, the kick plate and the pull handle are the only
     * cues for which half of the two opens. */
    fx_rect_a(r, glass, 130, ex + 8.0f, head, 50.0f, opening);
    fx_rect_a(r, glass, 130, ex + 134.0f, head, 50.0f, opening);
    fx_rect_a(r, art->trim_hi, 14, ex + 14.0f, head, 16.0f, opening);
    fx_rect_a(r, art->trim_hi, 14, ex + 140.0f, head, 16.0f, opening);
    fx_rect(r, jamb, ex + 134.0f, head, 3.0f, opening);
    fx_rect_a(r, jamb, 210, ex + 137.0f, head + opening * 0.52f, 47.0f, 3.0f);
    fx_rect_a(r, fx_mix(jamb, art->trim, 0.35f), 220, ex + 137.0f,
              base - 13.0f, 47.0f, 13.0f);
    fx_rect_a(r, art->trim, 200, ex + 172.0f, head + 26.0f, 3.0f, 30.0f);

    /* The revolving door. It turns on its own: an entrance frozen still reads
     * as a shut building, and this is the only moving thing out at the street.
     * A wing seen face-on is a pane and edge-on is a line, so the panel swept
     * between the hub and the leading edge is what sells the rotation. */
    float dl = ex + 58.0f;
    const float dw = 76.0f;
    float dcx = dl + dw * 0.5f;

    /* Two skins of glass and an enclosed drum, so it sits denser than the
     * single-glazed leaves beside it. */
    fx_rect_a(r, fx_mix(glass, FX_INK, 0.25f), 190, dl, head, dw, opening);
    fx_rect(r, jamb, dl, head, 4.0f, opening);
    fx_rect(r, jamb, dl + dw - 4.0f, head, 4.0f, opening);
    fx_rect_a(r, art->trim, 150, dl + 1.0f, head, 2.0f, opening);
    fx_rect_a(r, art->trim, 150, dl + dw - 3.0f, head, 2.0f, opening);
    for (int wing = 0; wing < 3; ++wing)
    {
        float angle = s->time * 0.5f + (float)wing * 2.0944f;
        float wx = dcx + sinf(angle) * (dw * 0.5f - 6.0f);
        bool front = cosf(angle) > 0.0f;
        float x0 = wx < dcx ? wx : dcx;

        fx_rect_a(r, front ? art->lamp : art->far_shape, front ? 30 : 42, x0,
                  head + 2.0f, fabsf(wx - dcx), opening - 4.0f);
        fx_rect_a(r, art->trim, front ? 210 : 90, wx - 1.0f, head + 2.0f, 2.0f,
                  opening - 4.0f);
    }
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.2f), dcx - 1.0f, head, 2.0f, opening);
    /* The drum's canopy, stepped so the enclosure reads as a cylinder rather
     * than as one more flat pane. */
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.35f), dl - 2.0f, head - 6.0f,
            dw + 4.0f, 6.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.15f), dl + 6.0f, head - 10.0f,
            dw - 12.0f, 5.0f);
    fx_rect_a(r, art->trim_hi, 130, dl + 6.0f, head - 10.0f, dw - 12.0f, 1.0f);

    /* Threshold: the one bright line at the foot of the opening, and the step
     * Chuck came in over. */
    fx_rect(r, FX_INK, ex, base, span, 5.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.2f), ex + 2.0f, base, span - 4.0f,
            3.0f);
    fx_rect_a(r, art->trim_hi, 170, ex + 2.0f, base, span - 4.0f, 1.0f);

    /* The canopy light last, over the doors it falls on. */
    fx_glow(r, cx, head, 58.0f, art->lamp, 44);
    fx_light_cone(r, cx, head, 86.0f, 104.0f, opening + 6.0f, art->lamp, 20);
}

/*
 * LOBBY — a glazed street front, the city behind it, and the atrium ceiling.
 *
 * Two rules hold this composition together, and both were learned the hard
 * way. First: only architecture repeats. A curtain wall genuinely runs the
 * length of a facade, so tiling mullions reads as a building; a reception desk
 * tiled every few hundred pixels reads as a mistake, and the sector is barely
 * wider than the window, so every repeat is on screen at once. The furniture
 * lives in the map as real props instead.
 *
 * Second: the glazing is a hole in the wall, so it carries its own values. The
 * night sky behind the glass has to sit clearly above the interior air or the
 * towers vanish into it and all that is left of the city is its lit windows,
 * floating in the dark like specks of dirt.
 */
static void backdrop_lobby(const LevelArtScene *s, const LevelThemeArt *art,
                           float oy, float fh)
{
    SDL_Renderer *r = s->renderer;
    /* The pavement outside sits just above the sector's floor slab, so the
     * street reads as continuous with the ground Chuck is standing on. */
    float street = oy + fh - 46.0f;
    float head = oy + 52.0f; /* underside of the ceiling soffit */
    /* Seen from a lit room at night the outside is the dark half of the
     * picture, so the sky stays down near the interior air and only the lit
     * windows are bright. Getting this the wrong way round makes the skyline
     * read as masonry standing inside the hall. What separates a tower from
     * the sky is the city glow gathered along the horizon plus a rim on its
     * own edges, not a brighter sky. */
    SDL_Color sky_high = {11, 15, 26, 255};
    SDL_Color sky_low = {38, 42, 58, 255};
    SDL_Color tower = {15, 19, 31, 255};

    fx_rect(r, sky_high, 0.0f, head, (float)s->win_w, street - head);
    fx_vgrad(r, 0.0f, head, (float)s->win_w, street - head,
             sky_high, 255, sky_low, 255);

    /* Farthest towers: a low, even band that gives the skyline a floor. */
    int far_tower = art_repeat(s->cam_x, 0.04f, 96.0f);
    for (float x = art_scroll(s->cam_x, 0.04f, 96.0f) - 48.0f;
         x < (float)s->win_w + 96.0f; x += 96.0f, ++far_tower)
    {
        unsigned h = fx_hash((unsigned)far_tower *
                             2246822519u);
        float slab = 46.0f + (float)(h % 40u);
        fx_rect(r, fx_mix(tower, sky_low, 0.45f), x, street - slab, 88.0f, slab);
    }

    /* Nearer towers, tall enough to break the horizon, with lit floors. A
     * bright cap on the roofline keeps each mass separate from the next. */
    int near_tower = art_repeat(s->cam_x, 0.07f, 176.0f);
    for (float x = art_scroll(s->cam_x, 0.07f, 176.0f) - 40.0f;
         x < (float)s->win_w + 176.0f; x += 176.0f, ++near_tower)
    {
        unsigned h = fx_hash((unsigned)near_tower *
                             2654435761u);
        /* Kept below the transom: a skyline that climbs into the upper atrium
         * competes with the room instead of sitting behind it. */
        float tower_h = 62.0f + (float)(h % 96u);
        float top = street - tower_h;
        fx_rect(r, tower, x + 14.0f, top, 96.0f, street - top);
        fx_rect(r, fx_mix(tower, sky_low, 0.7f), x + 14.0f, top, 96.0f, 1.0f);
        fx_rect(r, fx_mix(tower, sky_low, 0.45f), x + 14.0f, top, 2.0f,
                street - top);
        int window_row = 0;
        for (float wy = top + 10.0f; wy < street - 9.0f;
             wy += 14.0f, ++window_row)
        {
            int window_col = 0;
            for (float wx = x + 22.0f; wx < x + 106.0f;
                 wx += 12.0f, ++window_col)
            {
                /* A window belongs to a tower, floor and bay. Screen-space
                 * coordinates change whenever the camera scrolls, so hashing
                 * wx/wy here made the entire skyline switch its lights while
                 * Chuck walked. Keep the identity in the layer's stable
                 * repeat space instead. */
                unsigned wh = art_hash(near_tower * 11 + window_col,
                                       window_row + s->level_index * 17);
                if ((wh % 3u) == 0u)
                    continue;
                /* The lit windows are the only bright thing outside, so they
                 * carry the whole read of "a city out there". Most are warm;
                 * a handful are cold office fluorescent. */
                SDL_Color lit = (wh & 8u) ? art->lamp : FX_LAMP;
                /* One light in a few has a brief, time-driven fluorescent
                 * flutter. It is deliberately rare and keyed to the same
                 * stable identity, so movement can never trigger it. */
                if (((wh >> 9) % 19u) == 0u)
                {
                    float period = 7.0f + (float)((wh >> 15) % 5u);
                    float phase = (float)((wh >> 20) & 255u) / 255.0f * period;
                    if (fmodf(s->time + phase, period) < 0.08f)
                        lit = fx_dim(lit, 0.35f);
                }
                fx_rect_a(r, lit, (Uint8)(96 + (wh >> 5) % 96u), wx, wy,
                          5.0f, 7.0f);
            }
        }
        /* One rooftop beacon per block, so the skyline is not a still image. */
        if ((h & 3u) == 0u)
        {
            float pulse = sinf(s->time * 1.9f + (float)(h % 9u)) > 0.72f ? 1.0f
                                                                        : 0.2f;
            /* Rooftop beacon: FX_RED, a step back for the distance. */
            fx_rect(r, fx_dim(FX_RED, 0.92f * pulse),
                    x + 60.0f, top - 4.0f, 4.0f, 4.0f);
        }
    }

    /* The street: pavement, kerb, carriageway and a broken centre line. */
    fx_rect(r, fx_mix(sky_low, FX_INK, 0.42f), 0.0f, street,
            (float)s->win_w, oy + fh - street);
    fx_rect(r, fx_mix(sky_low, FX_INK, 0.25f), 0.0f, street,
            (float)s->win_w, 7.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.4f), 0.0f, street + 7.0f,
            (float)s->win_w, 2.0f);
    fx_rect(r, fx_mix(sky_low, FX_INK, 0.62f), 0.0f, street + 9.0f,
            (float)s->win_w, oy + fh - street - 9.0f);
    for (float mark = fmodf(-s->cam_x * 0.1f, 44.0f); mark < (float)s->win_w;
         mark += 44.0f)
    {
        /* Lane paint gone yellow under years of sodium light. */
        fx_rect_a(r, fx_mix(FX_CREAM, FX_AMBER_DK, 0.35f), 60, mark,
                  street + 26.0f, 16.0f, 2.0f);
    }

    /* Traffic. A car needs a body: a bare pair of glows in the dark reads as
     * lens flare, not as a vehicle passing the window. */
    /* Two dark saloon paints, kept below sky_low so a passing body reads by
     * its lights rather than its panels; and one headlamp white for the lamp,
     * its core and the wet-kerb reflection, so they can never disagree. */
    static const SDL_Color SALOON_WARM = {44, 40, 52, 255};
    static const SDL_Color SALOON_COOL = {36, 42, 50, 255};
    SDL_Color headlamp = fx_mix(FX_WARM, FX_CREAM, 0.70f);
    for (int car = 0; car < 3; ++car)
    {
        bool leftward = (car & 1) != 0;
        float speed = 58.0f + (float)car * 26.0f;
        float span = (float)s->win_w + 190.0f;
        float travel = fmodf(s->time * speed + (float)car * 210.0f, span);
        float x = leftward ? span - travel - 95.0f : travel - 95.0f;
        float y = street + (leftward ? 12.0f : 26.0f);
        SDL_Color body = car == 1 ? SALOON_WARM : SALOON_COOL;
        fx_rect(r, FX_INK, x + 2.0f, y + 12.0f, 62.0f, 4.0f);
        fx_rect(r, body, x + 4.0f, y + 4.0f, 58.0f, 9.0f);
        fx_rect(r, fx_mix(body, sky_low, 0.5f), x + 4.0f, y + 4.0f, 58.0f, 2.0f);
        fx_rect(r, fx_mix(body, sky_high, 0.7f), x + 20.0f, y, 26.0f, 5.0f);
        fx_rect(r, FX_INK, x + 12.0f, y + 14.0f, 8.0f, 4.0f);
        fx_rect(r, FX_INK, x + 46.0f, y + 14.0f, 8.0f, 4.0f);
        float lead = leftward ? x + 2.0f : x + 64.0f;
        float tail = leftward ? x + 64.0f : x + 2.0f;
        fx_glow(r, lead, y + 8.0f, 22.0f, headlamp, 70);
        fx_rect(r, fx_mix(FX_WARM, FX_CREAM, 0.90f), lead - 2.0f, y + 6.0f,
                4.0f, 3.0f);
        fx_glow(r, tail, y + 8.0f, 14.0f, FX_RED, 52);
        /* Wet asphalt carries the lights back up at the kerb. */
        fx_rect_a(r, headlamp, 26, lead - 14.0f,
                  y + 17.0f, 28.0f, 2.0f);
    }

    /* One cool veil over the whole opening. This is the single cue that does
     * the most work: without something between the room and the view, a lit
     * window in a tower two streets away sits at exactly the same depth as a
     * lamp on the wall behind Chuck. */
    /* Bluer than any lamp in the palette on purpose: this is night sky
     * filtered through glass, not a light source of its own. */
    const SDL_Color veil = {96, 126, 158, 255};
    fx_rect_a(r, veil, 30, 0.0f, head,
              (float)s->win_w, street + 10.0f - head);

    /* The curtain wall. Mullions land on a three-tile rhythm so the glazing
     * agrees with the masonry it is set into instead of cutting across it, and
     * they stay dark: a bright vertical line the height of the hall reads as a
     * pole standing in the room. */
    for (float x = art_scroll(s->cam_x, 0.14f, 96.0f); x < (float)s->win_w + 96.0f;
         x += 96.0f)
    {
        fx_rect(r, FX_INK, x, head, 8.0f, street + 10.0f - head);
        fx_rect(r, art->near_shape, x + 1.0f, head, 6.0f,
                street + 10.0f - head);
        /* The brass catches the light only at the head and the sill. A lit
         * line running the full storey height stops reading as a mullion and
         * starts reading as a pole standing in the hall. */
        fx_rect_a(r, art->trim, 120, x + 1.0f, head, 6.0f, 3.0f);
        fx_rect_a(r, art->trim, 90, x + 1.0f, street - 2.0f, 6.0f, 3.0f);
        /* A raking sheen across each bay: flat fill alone never reads as
         * glass, and it is the only cue that the street is behind something. */
        fx_rect_a(r, art->trim_hi, 11, x + 16.0f, head, 30.0f,
                  street - head);
        fx_rect_a(r, art->trim_hi, 7, x + 58.0f, head, 14.0f, street - head);
    }
    /* Transom at door head height, and the base channel the glass sits in. */
    fx_rect_a(r, FX_INK, 190, 0.0f, street - 96.0f, (float)s->win_w, 8.0f);
    fx_rect_a(r, art->near_shape, 210, 0.0f, street - 95.0f,
              (float)s->win_w, 6.0f);
    fx_rect_a(r, fx_mix(art->near_shape, art->trim, 0.5f), 150, 0.0f,
              street - 95.0f, (float)s->win_w, 1.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.25f), 0.0f, street + 6.0f,
            (float)s->win_w, 5.0f);
    fx_rect_a(r, art->trim_hi, 170, 0.0f, street + 6.0f, (float)s->win_w, 2.0f);

    /* The main entrance, set into the glazing that has just been drawn: it
     * interrupts the curtain wall, so it goes on after the bays, the transom
     * and the base channel rather than being framed by them. */
    lobby_entrance(s, art, street);

    /* The atrium ceiling. Without it the top of a triple-height hall is just
     * unexplained air, which is the one thing a tall room must not look like.
     * It rides a near parallax so it reads as part of the room, not the view. */
    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.35f), 0.0f, oy,
            (float)s->win_w, head - oy);
    fx_vgrad(r, 0.0f, oy, (float)s->win_w, head - oy,
             fx_mix(art->wall, art->wall_dark, 0.55f), 255,
             fx_mix(art->wall_dark, FX_INK, 0.5f), 255);
    for (float x = art_scroll(s->cam_x, 0.3f, 64.0f); x < (float)s->win_w + 64.0f;
         x += 64.0f)
    {
        /* Coffers: a recessed panel between two beams. Only shaded enough to
         * show the relief — a black recess reads as a hole in the ceiling. */
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.15f), x + 6.0f, oy + 6.0f,
                52.0f, head - oy - 18.0f);
        fx_rect(r, fx_mix(art->wall_dark, art->wall, 0.5f), x + 6.0f, oy + 6.0f,
                52.0f, 2.0f);
        fx_rect(r, fx_mix(art->wall, art->wall_light, 0.3f), x, oy,
                5.0f, head - oy);
        fx_rect(r, fx_mix(art->wall_light, art->trim_hi, 0.35f), x + 1.0f, oy,
                2.0f, head - oy);
    }
    fx_rect(r, art->trim, 0.0f, head - 6.0f, (float)s->win_w, 6.0f);
    fx_rect(r, art->trim_hi, 0.0f, head - 6.0f, (float)s->win_w, 2.0f);

    /* Banners suspended in the volume. A hall this tall needs something
     * hanging in it at mid height, or the space between the ceiling and the
     * galleries is just distance. */
    int banner = art_repeat(s->cam_x, 0.24f, 288.0f);
    for (float x = art_scroll(s->cam_x, 0.24f, 288.0f) - 144.0f;
         x < (float)s->win_w + 288.0f; x += 288.0f, ++banner)
    {
        unsigned h = fx_hash((unsigned)banner *
                             1103515245u);
        float bx = x + 118.0f;
        float by = head + 14.0f;
        float bh = 132.0f + (float)(h % 3u) * 34.0f;
        /* A slow breathing sway, as if the air handling were running. */
        float lean = sinf(s->time * 0.5f + (float)(h % 7u)) * 2.0f;
        float sx = bx + lean * 0.3f;
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.45f), bx - 12.0f, by, 26.0f, 3.0f);
        fx_rect(r, FX_INK, sx - 10.0f, by + 3.0f, 21.0f, bh);
        /* Deep bronze cloth. A banner is background: lit up to the value of
         * the brass it hangs from, it becomes the brightest thing in the hall
         * and pulls the eye off the floor Chuck is walking on. */
        fx_rect(r, fx_mix(art->wall_dark, art->accent, 0.13f),
                sx - 9.0f, by + 4.0f, 19.0f, bh - 2.0f);
        fx_vgrad(r, sx - 9.0f, by + 4.0f, 19.0f, bh - 2.0f,
                 fx_mix(art->wall_dark, art->accent, 0.2f), 255,
                 fx_mix(art->wall_dark, FX_INK, 0.35f), 255);
        /* Slack folds down one side, and the light edge facing the glass. */
        fx_rect_a(r, art->trim_hi, 26, sx - 9.0f, by + 4.0f, 3.0f, bh - 2.0f);
        fx_rect_a(r, FX_INK, 70, sx + 3.0f, by + 4.0f, 4.0f, bh - 2.0f);
        for (float fold = by + 26.0f; fold < by + bh - 8.0f; fold += 21.0f)
            fx_rect_a(r, FX_INK, 55, sx - 8.0f, fold, 17.0f, 2.0f);
        /* The building's mark, woven rather than printed on. */
        fx_rect_a(r, art->trim, 120, sx - 5.0f, by + 12.0f, 11.0f, 11.0f);
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.2f), sx - 3.0f,
                by + 15.0f, 7.0f, 5.0f);
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.4f), sx - 10.0f + lean * 0.7f,
                by + bh + 1.0f, 21.0f, 3.0f);
    }

    /* Pendant luminaires dropped into the volume, and the light they put on
     * the atrium floor. This is what a lobby looks like at night. */
    int pendant = art_repeat(s->cam_x, 0.3f, 192.0f);
    for (float x = art_scroll(s->cam_x, 0.3f, 192.0f) - 96.0f;
         x < (float)s->win_w + 192.0f; x += 192.0f, ++pendant)
    {
        unsigned h = fx_hash((unsigned)pendant * 40503u);
        float drop = 26.0f + (float)(h % 3u) * 14.0f;
        float cx = x + 96.0f;
        float cy = head + drop;
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.3f), cx - 1.0f, head - 2.0f,
                2.0f, drop);
        fx_rect(r, FX_INK, cx - 13.0f, cy, 26.0f, 5.0f);
        fx_rect(r, art->trim, cx - 12.0f, cy, 24.0f, 3.0f);
        fx_rect(r, art->lamp, cx - 10.0f, cy + 3.0f, 20.0f, 2.0f);
        fx_glow(r, cx, cy + 4.0f, 40.0f, art->lamp, 58);
        fx_light_cone(r, cx, cy + 4.0f, 18.0f, 92.0f, street - cy - 10.0f,
                      art->lamp, 15);
    }
}

static void backdrop_office(const LevelArtScene *s, const LevelThemeArt *art,
                            float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* The window wall, as a band rather than as full-height texture. The
     * glazing needs a head and a sill: run the blinds the whole depth of the
     * floor and they stop reading as something hanging in a window and start
     * reading as corrugation laid over the sector. */
    const float glass_top = oy + 26.0f;
    const float glass_h = fh - 188.0f;
    const float glass_bottom = glass_top + glass_h;

    /* A window band with the neighbouring tower behind it: enough city to
     * place the floor in the building, dim enough to stay background. */
    int tower = art_repeat(s->cam_x, 0.07f, 150.0f);
    for (float x = art_scroll(s->cam_x, 0.07f, 150.0f);
         x < (float)s->win_w + 150.0f; x += 150.0f, ++tower)
    {
        fx_rect(r, art->far_shape, x, glass_top + 8.0f, 128.0f,
                glass_h - 8.0f);
        /* Which windows are lit is a property of the tower, not of where the
         * tower currently is: keying it to the screen made the lights crawl
         * across the facade as the level scrolled. */
        int floor_index = 0;
        for (float wy = glass_top + 20.0f; wy < glass_bottom - 10.0f;
             wy += 22.0f, ++floor_index)
        {
            int bay = 0;
            for (float wx = x + 10.0f; wx < x + 118.0f; wx += 20.0f, ++bay)
                if ((fx_hash((unsigned)(tower * 64 + floor_index * 8 + bay)) %
                     7u) == 0u)
                    fx_rect_a(r, art->lamp, 70, wx, wy, 6.0f, 8.0f);
        }
    }
    /* Vertical blinds, half of them turned closed, hung inside that band. */
    int blind = art_repeat(s->cam_x, 0.12f, 22.0f);
    for (float x = art_scroll(s->cam_x, 0.12f, 22.0f);
         x < (float)s->win_w + 22.0f; x += 22.0f, ++blind)
    {
        unsigned h = fx_hash((unsigned)blind);
        fx_rect_a(r, art->trim_hi, (h & 1u) ? 30 : 13, x, glass_top + 5.0f,
                  13.0f, glass_h - 10.0f);
    }
    /* Head rail and sill, and the mullions the glazing is divided by. */
    int mullion = art_repeat(s->cam_x, 0.12f, 132.0f);
    for (float x = art_scroll(s->cam_x, 0.12f, 132.0f);
         x < (float)s->win_w + 132.0f; x += 132.0f, ++mullion)
    {
        fx_rect_a(r, art->near_shape, 210, x, glass_top, 5.0f, glass_h);
    }
    fx_rect(r, art->near_shape, 0.0f, glass_top, (float)s->win_w, 6.0f);
    fx_rect(r, fx_mix(art->near_shape, art->wall, 0.45f), 0.0f, glass_top,
            (float)s->win_w, 2.0f);
    fx_rect(r, art->near_shape, 0.0f, glass_bottom - 5.0f, (float)s->win_w,
            5.0f);
    fx_rect_a(r, art->trim_hi, 40, 0.0f, glass_bottom - 5.0f,
              (float)s->win_w, 1.0f);

    /* The cubicle farm. Screens left logged in are the only light source at
     * this hour, so they carry the layer. */
    int cubicle = art_repeat(s->cam_x, 0.22f, 176.0f);
    for (float x = art_scroll(s->cam_x, 0.22f, 176.0f) - 20.0f;
         x < (float)s->win_w + 176.0f; x += 176.0f, ++cubicle)
    {
        float top = oy + fh - 150.0f;
        unsigned bay = fx_hash((unsigned)cubicle + (unsigned)s->level_index);
        fx_rect(r, art->near_shape, x, top, 160.0f, 96.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.4f), x, top,
                160.0f, 3.0f);
        for (int divider = 1; divider < 3; ++divider)
            fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.35f),
                    x + (float)divider * 53.0f, top, 3.0f, 96.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.25f), x, top + 62.0f,
                160.0f, 4.0f);

        for (int desk = 0; desk < 3; ++desk)
        {
            if (((bay >> (unsigned)desk) & 3u) == 0u)
                continue;
            float mx = x + 16.0f + (float)desk * 53.0f;
            float my = top + 32.0f;
            fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), mx, my,
                    22.0f, 16.0f);
            /* A screensaver pulse rather than a hard blink: less distracting
             * behind a fight, still clearly alive. */
            float pulse = 0.55f + 0.45f * sinf(s->time * 1.3f + (float)desk +
                                               (float)(bay % 7u));
            fx_rect(r, fx_dim(art->accent, 0.4f + pulse * 0.5f), mx + 2.0f,
                    my + 2.0f, 18.0f, 12.0f);
            fx_glow(r, mx + 11.0f, my + 8.0f, 22.0f, art->accent,
                    (Uint8)(24.0f * pulse));
        }
    }

    /* Suspended ceiling: a grid of panels with one bank still on. */
    fx_rect(r, art->near_shape, 0.0f, oy, (float)s->win_w, 20.0f);
    int panel = art_repeat(s->cam_x, 0.3f, 60.0f);
    for (float x = art_scroll(s->cam_x, 0.3f, 60.0f);
         x < (float)s->win_w + 60.0f; x += 60.0f, ++panel)
    {
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), x, oy, 2.0f, 20.0f);
        unsigned h = fx_hash((unsigned)panel);
        if ((h % 3u) != 0u)
            continue;
        fx_rect(r, art->lamp, x + 8.0f, oy + 6.0f, 44.0f, 5.0f);
        fx_glow(r, x + 30.0f, oy + 9.0f, 40.0f, art->lamp, 34);
    }
}

static void backdrop_server(const LevelArtScene *s, const LevelThemeArt *art,
                            float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* Rows of racks receding into the dark. The only white light in the room
     * is the aisle strip; everything else is status LEDs. */
    for (float x = art_scroll(s->cam_x, 0.09f, 96.0f) - 20.0f;
         x < (float)s->win_w + 96.0f; x += 96.0f)
    {
        fx_rect(r, art->far_shape, x, oy + 34.0f, 78.0f, fh - 78.0f);
        fx_rect(r, fx_mix(art->far_shape, art->accent, 0.12f), x, oy + 34.0f,
                78.0f, 2.0f);
    }

    int row = art_repeat(s->cam_x, 0.24f, 132.0f);
    for (float x = art_scroll(s->cam_x, 0.24f, 132.0f) - 30.0f;
         x < (float)s->win_w + 132.0f; x += 132.0f, ++row)
    {
        float top = oy + 46.0f;
        float height = fh - 108.0f;
        unsigned rack = fx_hash((unsigned)row +
                                (unsigned)s->level_index * 13u);
        fx_rect(r, art->near_shape, x, top, 104.0f, height);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.4f), x, top,
                104.0f, 2.0f);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), x + 4.0f, top + 6.0f,
                96.0f, height - 12.0f);

        /* Each unit gets a pair of LEDs whose blink rate is fixed per slot,
         * so the wall shimmers instead of strobing in unison. */
        int units = (int)((height - 16.0f) / 11.0f);
        for (int unit = 0; unit < units; ++unit)
        {
            float uy = top + 10.0f + (float)unit * 11.0f;
            unsigned uh = fx_hash(rack + (unsigned)unit * 2654435761u);
            fx_rect(r, fx_mix(art->near_shape, art->wall, 0.5f), x + 6.0f, uy,
                    92.0f, 8.0f);
            fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), x + 8.0f,
                    uy + 2.0f, 60.0f, 4.0f);
            float rate = 1.1f + (float)(uh % 9u) * 0.4f;
            bool lit = fmodf(s->time * rate + art_unit(uh, 5) * 4.0f, 2.0f) <
                       1.2f;
            SDL_Color led = (uh & 4u) ? art->accent : FX_GREEN;
            fx_rect(r, lit ? led : fx_dim(led, 0.16f), x + 88.0f, uy + 2.0f,
                    3.0f, 4.0f);
            if (lit && (uh % 5u) == 0u)
                fx_glow(r, x + 89.0f, uy + 4.0f, 9.0f, led, 60);
        }
        fx_glow(r, x + 52.0f, top + height * 0.5f, 90.0f, art->accent, 16);
    }

    /* Overhead cable tray: the room's ceiling, and its only horizontal line. */
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.2f), 0.0f, oy + 12.0f,
            (float)s->win_w, 14.0f);
    for (float x = art_scroll(s->cam_x, 0.3f, 26.0f);
         x < (float)s->win_w + 26.0f; x += 26.0f)
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), x,
                oy + 12.0f, 2.0f, 14.0f);
    fx_rect_a(r, art->accent, 40, 0.0f, oy + 26.0f, (float)s->win_w, 2.0f);
}

static void backdrop_canteen(const LevelArtScene *s, const LevelThemeArt *art,
                             float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* Tiled back wall of the servery, then the stainless counter, then the
     * pendant lamps: three flat bands that read instantly as a kitchen. */
    for (float y = oy + 24.0f; y < oy + fh - 120.0f; y += 14.0f)
        fx_rect_a(r, art->wall_light, 26, 0.0f, y, (float)s->win_w, 1.0f);

    for (float x = art_scroll(s->cam_x, 0.14f, 210.0f) - 30.0f;
         x < (float)s->win_w + 210.0f; x += 210.0f)
    {
        float top = oy + 46.0f;
        fx_rect(r, art->far_shape, x, top, 176.0f, 74.0f);
        fx_rect(r, fx_mix(art->far_shape, art->trim_hi, 0.4f), x, top,
                176.0f, 2.0f);
        /* A menu board with its letters long since rearranged by somebody. */
        for (int line = 0; line < 3; ++line)
            for (int word = 0; word < 3; ++word)
                fx_rect_a(r, art->trim_hi, 90,
                          x + 16.0f + (float)word * 52.0f,
                          top + 16.0f + (float)line * 18.0f,
                          22.0f + (float)((word + line) % 3) * 12.0f, 5.0f);
    }

    for (float x = art_scroll(s->cam_x, 0.26f, 240.0f);
         x < (float)s->win_w + 240.0f; x += 240.0f)
    {
        float counter = oy + fh - 96.0f;
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.35f), x, counter,
                200.0f, 60.0f);
        fx_rect(r, art->trim_hi, x, counter, 200.0f, 3.0f);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), x, counter + 22.0f,
                200.0f, 2.0f);
        /* Gastronorm wells under heat lamps; the glow is the warmest thing
         * in the game and the reason this sector feels different. */
        for (int well = 0; well < 3; ++well)
        {
            float wx = x + 22.0f + (float)well * 60.0f;
            fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), wx, counter + 6.0f,
                    44.0f, 12.0f);
            fx_rect(r, art->accent, wx + 6.0f, counter - 26.0f, 32.0f, 3.0f);
            fx_glow(r, wx + 22.0f, counter - 8.0f, 34.0f, art->lamp, 52);
        }
    }

    /* Pendant lamps on long flexes, swaying just enough to be noticed. */
    for (float x = art_scroll(s->cam_x, 0.34f, 130.0f);
         x < (float)s->win_w + 130.0f; x += 130.0f)
    {
        float sway = sinf(s->time * 0.7f + x * 0.02f) * 4.0f;
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), x + 40.0f, oy,
                2.0f, 40.0f);
        fx_rect(r, fx_mix(art->wall_light, art->trim_hi, 0.4f),
                x + 30.0f + sway, oy + 40.0f, 22.0f, 8.0f);
        fx_glow(r, x + 41.0f + sway, oy + 50.0f, 46.0f, art->lamp, 46);
        fx_light_cone(r, x + 41.0f + sway, oy + 48.0f, 10.0f, 40.0f,
                      fh * 0.7f, art->lamp, 22);
    }
}

static void backdrop_lab(const LevelArtScene *s, const LevelThemeArt *art,
                         float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* Glazed partitions dividing the floor into bays. Everything is pale and
     * evenly lit — the opposite of the plant rooms below. */
    for (float x = art_scroll(s->cam_x, 0.1f, 190.0f) - 20.0f;
         x < (float)s->win_w + 190.0f; x += 190.0f)
    {
        fx_rect(r, art->far_shape, x, oy + 22.0f, 170.0f, fh - 60.0f);
        fx_rect_a(r, art->trim_hi, 30, x + 8.0f, oy + 30.0f, 154.0f,
                  fh - 84.0f);
        fx_rect(r, fx_mix(art->far_shape, art->wall_light, 0.45f), x,
                oy + 22.0f, 170.0f, 3.0f);
        fx_rect(r, fx_mix(art->far_shape, art->wall_light, 0.3f), x + 82.0f,
                oy + 22.0f, 4.0f, fh - 60.0f);
    }

    int cell = art_repeat(s->cam_x, 0.24f, 216.0f);
    for (float x = art_scroll(s->cam_x, 0.24f, 216.0f);
         x < (float)s->win_w + 216.0f; x += 216.0f, ++cell)
    {
        float bench = oy + fh - 104.0f;
        unsigned bay = fx_hash((unsigned)cell);

        /* Fume cabinet: a lit glass box, sash half open, extract duct up. */
        fx_rect(r, art->near_shape, x + 14.0f, bench - 92.0f, 96.0f, 92.0f);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.45f), x + 20.0f,
                bench - 84.0f, 84.0f, 60.0f);
        fx_rect_a(r, art->accent, 70, x + 20.0f, bench - 84.0f, 84.0f, 60.0f);
        fx_glow(r, x + 62.0f, bench - 54.0f, 62.0f, art->accent, 44);
        fx_rect(r, art->trim_hi, x + 18.0f, bench - 54.0f, 88.0f, 4.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), x + 50.0f,
                oy, 20.0f, bench - 92.0f - oy);

        /* Glassware on the bench, and a centrifuge that is still running. */
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.5f), x, bench,
                200.0f, 8.0f);
        fx_rect(r, art->trim_hi, x, bench, 200.0f, 2.0f);
        for (int flask = 0; flask < 4; ++flask)
        {
            float fx_pos = x + 124.0f + (float)flask * 17.0f;
            float height = 10.0f + (float)((bay >> (unsigned)flask) % 12u);
            fx_rect_a(r, art->trim_hi, 120, fx_pos, bench - height, 9.0f,
                      height);
            fx_rect_a(r, art->accent, 150, fx_pos + 1.0f, bench - height * 0.4f,
                      7.0f, height * 0.4f);
        }
        float spin = fmodf(s->time * 3.0f, 1.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.25f),
                x + 118.0f, bench - 26.0f, 26.0f, 18.0f);
        fx_rect(r, art->accent, x + 122.0f + spin * 18.0f, bench - 22.0f,
                3.0f, 4.0f);
    }

    /* Continuous ceiling strip: clean rooms are lit edge to edge. */
    fx_rect(r, art->near_shape, 0.0f, oy, (float)s->win_w, 14.0f);
    fx_rect(r, art->lamp, 0.0f, oy + 8.0f, (float)s->win_w, 4.0f);
    fx_vgrad(r, 0.0f, oy + 12.0f, (float)s->win_w, 70.0f, art->lamp, 30,
             art->lamp, 0);
}

static void backdrop_archive(const LevelArtScene *s, const LevelThemeArt *art,
                             float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* Stacks running back into the dark. Two ranks at different parallax do
     * the whole job: the aisle depth comes from the offset, not from detail. */
    for (int rank = 0; rank < 2; ++rank)
    {
        float factor = rank == 0 ? 0.09f : 0.2f;
        float period = rank == 0 ? 118.0f : 156.0f;
        float top = oy + (rank == 0 ? 40.0f : 62.0f);
        float height = fh - (rank == 0 ? 76.0f : 110.0f);
        SDL_Color body = rank == 0 ? art->far_shape : art->near_shape;
        int stack = art_repeat(s->cam_x, factor, period);
        for (float x = art_scroll(s->cam_x, factor, period) - 30.0f;
             x < (float)s->win_w + period; x += period, ++stack)
        {
            fx_rect(r, body, x, top, period - 26.0f, height);
            fx_rect(r, fx_mix(body, art->wall_light, 0.3f), x, top,
                    period - 26.0f, 2.0f);
            for (float shelf = top + 22.0f; shelf < top + height;
                 shelf += 26.0f)
            {
                fx_rect(r, fx_mix(body, FX_INK, 0.4f), x, shelf,
                        period - 26.0f, 3.0f);
                /* Box spines: a row of narrow blocks in slightly different
                 * papers, which is all a shelf of files ever looks like. The
                 * key is the bay and the slot, so a spine does not change
                 * colour when the shelf scrolls. */
                unsigned bay = (unsigned)stack + (unsigned)rank * 977u;
                int slot = 0;
                for (float box = x + 3.0f; box < x + period - 32.0f;
                     box += 9.0f, ++slot)
                {
                    unsigned bh = fx_hash(bay * 2654435761u +
                                          (unsigned)slot * 40503u +
                                          (unsigned)(shelf - top));
                    fx_rect(r, fx_mix(art->far_shape, art->trim,
                                      0.14f + art_unit(bh, 3) * 0.26f),
                            box, shelf - 20.0f, 7.0f, 20.0f);
                    if ((bh % 6u) == 0u)
                        fx_rect_a(r, art->trim_hi, 45, box + 1.0f,
                                  shelf - 16.0f, 5.0f, 3.0f);
                }
            }
        }
    }

    /* Bare bulbs on cords with heavy dust in the beam — the archive's whole
     * mood is one warm cone in a lot of brown dark. */
    for (float x = art_scroll(s->cam_x, 0.3f, 208.0f);
         x < (float)s->win_w + 208.0f; x += 208.0f)
    {
        float sway = sinf(s->time * 0.5f + x * 0.013f) * 3.0f;
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.4f), x + 60.0f, oy,
                1.0f, 46.0f);
        fx_rect(r, art->lamp, x + 57.0f + sway, oy + 46.0f, 7.0f, 7.0f);
        fx_glow(r, x + 60.0f + sway, oy + 50.0f, 40.0f, art->lamp, 66);
        fx_light_cone(r, x + 60.0f + sway, oy + 50.0f, 6.0f, 58.0f,
                      fh * 0.85f, art->lamp, 26);
    }
}

static void backdrop_security(const LevelArtScene *s, const LevelThemeArt *art,
                              float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* The monitor wall. Feeds are the light source, and a couple of them are
     * dead — the building watches itself badly. */
    int wall = art_repeat(s->cam_x, 0.16f, 150.0f);
    for (float x = art_scroll(s->cam_x, 0.16f, 150.0f) - 20.0f;
         x < (float)s->win_w + 150.0f; x += 150.0f, ++wall)
    {
        float top = oy + 34.0f;
        fx_rect(r, art->far_shape, x, top, 132.0f, 132.0f);
        for (int screen = 0; screen < 9; ++screen)
        {
            float sx = x + 6.0f + (float)(screen % 3) * 42.0f;
            float sy = top + 6.0f + (float)(screen / 3) * 42.0f;
            unsigned h = fx_hash((unsigned)wall * 31u + (unsigned)screen);
            fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.6f), sx, sy,
                    36.0f, 36.0f);
            if ((h % 9u) == 0u)
            {
                /* Static: a handful of scan rows at random brightness. */
                for (int line = 0; line < 6; ++line)
                {
                    unsigned lh = fx_hash(h + (unsigned)line +
                                          (unsigned)(s->time * 12.0f));
                    fx_rect_a(r, art->haze, (Uint8)(30u + lh % 90u), sx + 2.0f,
                              sy + 3.0f + (float)line * 5.0f, 32.0f, 3.0f);
                }
                continue;
            }
            SDL_Color feed = fx_mix(art->far_shape, art->near_shape, 0.6f);
            fx_rect(r, feed, sx + 2.0f, sy + 2.0f, 32.0f, 32.0f);
            fx_rect(r, fx_mix(feed, art->trim, 0.5f), sx + 2.0f, sy + 24.0f,
                    32.0f, 10.0f);
            /* Something crosses one feed now and then. */
            float walker = fmodf(s->time * 0.4f + art_unit(h, 4) * 6.0f, 6.0f);
            if (walker < 1.0f)
                fx_rect(r, fx_mix(feed, FX_INK, 0.7f),
                        sx + 4.0f + walker * 26.0f, sy + 18.0f, 4.0f, 10.0f);
            fx_rect_a(r, art->haze, 22, sx + 2.0f,
                      sy + 2.0f + fmodf(s->time * 26.0f + (float)screen * 9.0f,
                                        32.0f),
                      32.0f, 2.0f);
        }
        fx_glow(r, x + 66.0f, top + 66.0f, 96.0f,
                fx_mix(art->near_shape, art->haze, 0.5f), 26);
    }

    /* Console desks below, and a standby beacon sweeping the whole room. */
    for (float x = art_scroll(s->cam_x, 0.3f, 190.0f);
         x < (float)s->win_w + 190.0f; x += 190.0f)
    {
        float desk = oy + fh - 84.0f;
        fx_rect(r, art->near_shape, x, desk, 160.0f, 40.0f);
        fx_rect(r, fx_mix(art->near_shape, art->trim, 0.4f), x, desk,
                160.0f, 3.0f);
        for (int key = 0; key < 5; ++key)
            fx_rect(r, art->accent, x + 18.0f + (float)key * 28.0f,
                    desk + 12.0f, 8.0f, 3.0f);
    }
    float sweep = fmodf(s->time * 0.55f, 1.0f);
    fx_glow(r, sweep * (float)(s->win_w + 200) - 100.0f, oy + fh * 0.42f,
            190.0f, art->accent, 26);
}

static void backdrop_ducts(const LevelArtScene *s, const LevelThemeArt *art,
                           float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* Galvanised trunking crossing the void, with the flange rings and hanger
     * rods that make a duct read as a duct and not as a pipe. */
    for (int run = 0; run < 3; ++run)
    {
        float y = oy + 34.0f + (float)run * (fh - 90.0f) / 3.0f;
        float depth = 0.08f + (float)run * 0.09f;
        float thickness = 26.0f + (float)run * 12.0f;
        SDL_Color body = fx_mix(art->far_shape, art->wall,
                                0.25f + (float)run * 0.25f);
        fx_rect(r, body, 0.0f, y, (float)s->win_w, thickness);
        fx_vgrad(r, 0.0f, y, (float)s->win_w, thickness,
                 fx_mix(body, art->wall_light, 0.4f), 255,
                 fx_mix(body, FX_INK, 0.35f), 255);
        for (float x = art_scroll(s->cam_x, depth, 96.0f);
             x < (float)s->win_w + 96.0f; x += 96.0f)
        {
            fx_rect(r, fx_mix(body, art->wall_light, 0.55f), x, y - 2.0f,
                    5.0f, thickness + 4.0f);
            fx_rect(r, fx_mix(body, FX_INK, 0.4f), x + 5.0f, y - 2.0f,
                    2.0f, thickness + 4.0f);
            fx_rect(r, fx_mix(body, FX_INK, 0.5f), x + 44.0f, oy,
                    2.0f, y - oy);
        }
    }

    /* An extract fan turning slowly behind a grille: the sector's only
     * moving light, and a reminder that the air here is going somewhere. */
    for (float x = art_scroll(s->cam_x, 0.26f, 340.0f);
         x < (float)s->win_w + 340.0f; x += 340.0f)
    {
        float cx = x + 70.0f;
        float cy = oy + fh * 0.5f;
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.3f), cx - 32.0f,
                cy - 32.0f, 64.0f, 64.0f);
        fx_glow(r, cx, cy, 46.0f, art->lamp, 22);
        float spin = s->time * 1.1f;
        for (int blade = 0; blade < 4; ++blade)
        {
            float angle = spin + (float)blade * 1.5707963f;
            float ex = cosf(angle) * 24.0f;
            float ey = sinf(angle) * 24.0f;
            fx_set(r, fx_mix(art->wall, art->wall_light, 0.4f));
            SDL_RenderLine(r, cx, cy, cx + ex, cy + ey);
        }
        for (int bar = 0; bar < 7; ++bar)
            fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.15f), cx - 32.0f,
                    cy - 28.0f + (float)bar * 9.0f, 64.0f, 3.0f);
    }

    /* Cable bundles sagging between hangers, close to camera. */
    for (float x = art_scroll(s->cam_x, 0.36f, 120.0f);
         x < (float)s->win_w + 120.0f; x += 120.0f)
    {
        for (int strand = 0; strand < 3; ++strand)
        {
            float sag = 8.0f + (float)strand * 5.0f;
            float y = oy + 16.0f + (float)strand * 4.0f;
            fx_set(r, fx_mix(art->far_shape, art->wall, 0.3f));
            for (int seg = 0; seg < 8; ++seg)
            {
                float t0 = (float)seg / 8.0f;
                float t1 = (float)(seg + 1) / 8.0f;
                SDL_RenderLine(r, x + t0 * 120.0f,
                               y + sinf(t0 * 3.14159265f) * sag,
                               x + t1 * 120.0f,
                               y + sinf(t1 * 3.14159265f) * sag);
            }
        }
    }
}

static void backdrop_penthouse(const LevelArtScene *s,
                               const LevelThemeArt *art, float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* A panelled hall hung with pictures and lit by sconces. Warm, still, and
     * expensive: the last place in the building that looks lived in. */
    fx_rect(r, fx_mix(art->far_shape, art->wall, 0.35f), 0.0f, oy,
            (float)s->win_w, fh);
    for (float x = art_scroll(s->cam_x, 0.1f, 78.0f);
         x < (float)s->win_w + 78.0f; x += 78.0f)
    {
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.3f), x, oy, 4.0f, fh);
        fx_rect(r, fx_mix(art->far_shape, art->trim, 0.25f), x + 4.0f, oy,
                1.0f, fh);
    }
    fx_rect(r, art->trim, 0.0f, oy + 26.0f, (float)s->win_w, 4.0f);
    fx_rect(r, art->trim_hi, 0.0f, oy + 26.0f, (float)s->win_w, 1.0f);

    int room = art_repeat(s->cam_x, 0.2f, 260.0f);
    for (float x = art_scroll(s->cam_x, 0.2f, 260.0f);
         x < (float)s->win_w + 260.0f; x += 260.0f, ++room)
    {
        unsigned h = fx_hash((unsigned)room +
                             (unsigned)s->level_index);
        /* Framed canvas: gilt frame, dark painting, picture light above. */
        float py = oy + 62.0f;
        float pw = 74.0f + (float)(h % 30u);
        float ph = 58.0f + (float)((h >> 5) % 26u);
        fx_rect(r, art->trim, x + 40.0f, py, pw, ph);
        fx_rect(r, art->trim_hi, x + 40.0f, py, pw, 2.0f);
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.45f), x + 45.0f, py + 5.0f,
                pw - 10.0f, ph - 10.0f);
        fx_rect_a(r, art->lamp, 34, x + 45.0f, py + 5.0f, pw - 10.0f,
                  (ph - 10.0f) * 0.5f);
        fx_glow(r, x + 40.0f + pw * 0.5f, py + 6.0f, 46.0f, art->lamp, 30);

        /* A display cabinet with lit shelves further down the hall. */
        float cy = oy + fh - 132.0f;
        fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.3f), x + 168.0f, cy,
                62.0f, 108.0f);
        fx_rect(r, art->trim, x + 168.0f, cy, 62.0f, 3.0f);
        for (int shelf = 0; shelf < 3; ++shelf)
        {
            float sy = cy + 22.0f + (float)shelf * 28.0f;
            fx_rect_a(r, art->lamp, 46, x + 172.0f, sy - 18.0f, 54.0f, 18.0f);
            fx_rect(r, art->trim_hi, x + 172.0f, sy, 54.0f, 2.0f);
            fx_rect(r, fx_mix(art->far_shape, art->trim, 0.4f),
                    x + 186.0f + (float)shelf * 5.0f, sy - 14.0f, 10.0f, 14.0f);
        }
    }

    /* Wall sconces: pools of light with a slow candle-like breath. */
    for (float x = art_scroll(s->cam_x, 0.3f, 156.0f);
         x < (float)s->win_w + 156.0f; x += 156.0f)
    {
        float breathe = 0.9f + 0.1f * sinf(s->time * 1.6f + x * 0.02f);
        float ly = oy + 48.0f;
        fx_rect(r, art->trim, x + 24.0f, ly, 12.0f, 16.0f);
        fx_rect(r, fx_dim(art->lamp, breathe), x + 26.0f, ly + 2.0f,
                8.0f, 10.0f);
        fx_glow(r, x + 30.0f, ly + 6.0f, 54.0f, art->lamp,
                (Uint8)(58.0f * breathe));
        fx_vgrad(r, x + 6.0f, ly + 14.0f, 48.0f, 90.0f, art->lamp, 26,
                 art->lamp, 0);
    }
}

static void backdrop_roof(const LevelArtScene *s, const LevelThemeArt *art,
                          float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* The city seen through a curtain wall. After four climbs on the outside
     * of this building, the last sector puts the drop behind glass. */
    int star_band = (int)(fh * 0.45f);
    if (star_band < 1)
        star_band = 1;
    for (int i = 0; i < 40; ++i)
    {
        unsigned h = art_hash(i * 17 + 3, 401);
        float x = (float)(h % (unsigned)s->win_w);
        float y = oy + (float)((h >> 9) % (unsigned)star_band);
        fx_rect_a(r, art->haze, (Uint8)(60 + h % 90u), x, y, 1.0f, 1.0f);
    }
    /*
     * The city glow gathered along the horizon. This is the layer that lets the
     * towers exist at all: the sky behind them has to stay darker than the room
     * or the skyline walks inside the building, so what separates one tower from
     * the next cannot be a brighter sky — it has to be the haze they stand in
     * and a rim on their own edges.
     */
    float horizon = oy + fh * 0.78f;
    fx_vgrad(r, 0.0f, horizon - 190.0f, (float)s->win_w, 190.0f,
             art->lamp, 0, art->lamp, 34);

    /* Two ranks. The far one is a low even band that gives the skyline a floor;
     * the near one breaks the horizon and carries the lit windows. */
    int distant = art_repeat(s->cam_x, 0.03f, 88.0f);
    for (float x = art_scroll(s->cam_x, 0.03f, 88.0f) - 44.0f;
         x < (float)s->win_w + 88.0f; x += 88.0f, ++distant)
    {
        unsigned h = fx_hash((unsigned)distant * 2246822519u);
        float slab = 54.0f + (float)(h % 52u);
        fx_rect(r, fx_mix(art->far_shape, art->lamp, 0.1f), x, horizon - slab,
                80.0f, slab);
        fx_rect_a(r, art->haze, 40, x, horizon - slab, 80.0f, 1.0f);
    }

    int block = art_repeat(s->cam_x, 0.05f, 132.0f);
    for (float x = art_scroll(s->cam_x, 0.05f, 132.0f) - 40.0f;
         x < (float)s->win_w + 132.0f; x += 132.0f, ++block)
    {
        unsigned h = fx_hash((unsigned)block * 71u);
        float tower_h = 120.0f + (float)(h % 150u);
        float top = horizon - tower_h;
        fx_rect(r, art->far_shape, x, top, 84.0f, tower_h);
        /* The rim: a lit parapet and one lit flank. Without them a tower drawn
         * at the value of the air behind it disappears, and all that is left of
         * the city is its windows floating in the dark like dirt on the glass. */
        fx_rect_a(r, art->haze, 90, x, top, 84.0f, 1.0f);
        fx_rect_a(r, art->haze, 55, x, top, 1.0f, tower_h);
        fx_rect(r, fx_mix(art->far_shape, art->accent, 0.2f), x, top,
                84.0f, 2.0f);
        int floor_index = 0;
        for (float wy = top + 10.0f; wy < horizon - 6.0f;
             wy += 16.0f, ++floor_index)
        {
            int bay = 0;
            for (float wx = x + 8.0f; wx < x + 76.0f; wx += 14.0f, ++bay)
            {
                /* Keyed to the tower, floor and bay rather than to where the
                 * window currently is on screen: hashing the screen position
                 * made the whole skyline switch its lights while Chuck walked. */
                unsigned wh = art_hash(block * 13 + bay, floor_index + 71);
                if ((wh % 5u) < 2u)
                    continue;
                SDL_Color lit = (wh & 8u) ? art->lamp
                                          : fx_mix(art->lamp, art->accent, 0.6f);
                fx_rect_a(r, lit, (Uint8)(70u + (wh >> 6) % 80u), wx, wy,
                          4.0f, 5.0f);
            }
        }
        /* Aircraft warning light on the tallest neighbours. */
        if ((h % 3u) == 0u)
        {
            float pulse = sinf(s->time * 1.6f + (float)(h % 5u)) > 0.7f ? 1.0f
                                                                       : 0.15f;
            fx_glow(r, x + 42.0f, top - 4.0f, 14.0f,
                    FX_RED, (Uint8)(90.0f * pulse));
        }
    }
    /* Ground haze thick enough to lose the foot of the towers in, but not so
     * thick that it takes the towers with it. */
    fx_vgrad(r, 0.0f, oy + fh * 0.62f, (float)s->win_w, fh * 0.38f,
             art->far_shape, 0, art->far_shape, 150);

    /* The glazing: mullions, transoms, and rain running down the outside. */
    for (float x = art_scroll(s->cam_x, 0.18f, 74.0f);
         x < (float)s->win_w + 74.0f; x += 74.0f)
    {
        fx_rect(r, art->near_shape, x, oy, 8.0f, fh);
        fx_rect(r, fx_mix(art->near_shape, art->trim_hi, 0.4f), x + 1.0f, oy,
                2.0f, fh);
        fx_rect_a(r, art->trim_hi, 12, x + 14.0f, oy, 30.0f, fh);
    }
    for (float y = oy + 60.0f; y < oy + fh; y += 96.0f)
    {
        fx_rect(r, art->near_shape, 0.0f, y, (float)s->win_w, 6.0f);
        fx_rect(r, fx_mix(art->near_shape, art->trim_hi, 0.3f), 0.0f, y,
                (float)s->win_w, 2.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int drop = 0; drop < 34; ++drop)
    {
        unsigned h = art_hash(drop * 23, 907);
        float x = (float)(h % (unsigned)s->win_w);
        float speed = 40.0f + (float)(h % 60u);
        float y = oy + fmodf(s->time * speed + (float)((h >> 7) % 400u), fh);
        SDL_SetRenderDrawColor(r, art->haze.r, art->haze.g, art->haze.b, 60);
        fx_fill(r, x, y, 1.0f, 7.0f + (float)(h % 6u));
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* Structural bracing in front of the glass, close and unlit. */
    for (float x = art_scroll(s->cam_x, 0.32f, 220.0f);
         x < (float)s->win_w + 220.0f; x += 220.0f)
    {
        fx_set(r, fx_mix(art->near_shape, FX_INK, 0.4f));
        SDL_RenderLine(r, x, oy, x + 110.0f, oy + fh);
        SDL_RenderLine(r, x + 1.0f, oy, x + 111.0f, oy + fh);
        SDL_RenderLine(r, x + 110.0f, oy, x, oy + fh);
    }
}

/* ---- Exterior backdrops ---------------------------------------------- */

/*
 * The four climbs share one building: the same masonry shell, floor bands and
 * structural bays. What changes is the hour and the weather, which is what a
 * climber would actually notice between one wall and the next.
 */
static void facade_shell(const LevelArtScene *s, const LevelThemeArt *art,
                         float top, float height)
{
    SDL_Renderer *r = s->renderer;
    float face_left = FACADE_BUILDING_SIDE_INSET - s->cam_x;
    float face_width = (float)s->level->map.width * (float)TILE_SIZE -
                       FACADE_BUILDING_SIDE_INSET * 2.0f;
    float face_right = face_left + face_width;

    fx_rect(r, art->wall, face_left, top, face_width, height);

    int first_course = (int)floorf(s->cam_y / 16.0f) - 1;
    int last_course = first_course + (int)(height / 16.0f) + 3;
    for (int course = first_course; course <= last_course; ++course)
    {
        float y = (float)course * 16.0f + HUD_HEIGHT - s->cam_y;
        float joint_offset = (course & 1) != 0 ? 32.0f : 0.0f;

        /*
         * Ashlar, one block at a time. Ruling joints over a single flat fill
         * gives a grid, not masonry: what makes a stone wall stone is that no
         * two blocks came out of the quarry the same colour. A block's identity
         * is its course and its index along that course — both in world space,
         * so a block keeps its colour while the climb scrolls past it rather
         * than shimmering as the camera moves.
         */
        int block_count = (int)(face_width / 64.0f) + 2;
        int first_block = (int)floorf((s->cam_x - FACADE_BUILDING_SIDE_INSET -
                                       joint_offset) /
                                      64.0f) -
                          1;
        if (first_block < 0)
            first_block = 0;
        for (int block = first_block; block < block_count; ++block)
        {
            /* The face is anchored to the building rather than scrolling like a
             * parallax layer, so a block's index is its position along the
             * course and has nothing to do with where the camera is. Deriving
             * the index from cam_x instead would hand one block a new colour
             * every time the climb moved, and the whole wall would crawl. */
            float x = face_left + joint_offset + (float)block * 64.0f;
            if (x > (float)s->win_w)
                break;
            unsigned bh = art_hash(block, course);
            float left = x > face_left ? x : face_left;
            float right = x + 64.0f < face_right ? x + 64.0f : face_right;
            if (right <= left)
                continue;
            fx_rect_a(r, (bh & 1u) ? art->wall_light : FX_INK,
                      (Uint8)(10u + (bh >> 4) % 26u), left, y, right - left,
                      16.0f);
            /* The bed joint, and the light catching the top arris of the
             * course below it. */
            fx_rect(r, art->wall_dark, left, y, right - left, 1.0f);
            fx_rect_a(r, art->wall_light, 40, left, y + 1.0f, right - left,
                      1.0f);
            if (x >= face_left && x + 1.0f <= face_right)
                fx_rect(r, fx_mix(art->wall_dark, art->wall, 0.5f), x, y,
                        1.0f, 16.0f);
        }
    }

    /* Window bays separated by shallow stone pilasters, each throwing a soft
     * shadow into the bay beside it — the only thing that gives the face any
     * relief at all before the cornices are drawn on top of it. */
    for (int col = 6; col < s->level->map.width - 3; col += 4)
    {
        float x = ((float)col + 0.5f) * (float)TILE_SIZE - s->cam_x;
        fx_hgrad(r, x + 5.0f, top, 14.0f, height, FX_INK, 54, FX_INK, 0);
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.25f), x - 4.0f, top,
                9.0f, height);
        fx_rect(r, art->wall_light, x - 3.0f, top, 2.0f, height);
        fx_rect_a(r, FX_INK, 70, x + 3.0f, top, 2.0f, height);
    }

    /* Each band sits in the wall gap between two window rows. */
    for (int row = 3; row < s->level->map.height; row += 3)
    {
        float y = (float)(row + 2) * (float)TILE_SIZE + HUD_HEIGHT - s->cam_y;
        if (y < top - 8.0f || y > (float)s->win_h + 8.0f)
            continue;
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.35f), face_left, y,
                face_width, 6.0f);
        fx_rect(r, art->trim, face_left, y, face_width, 2.0f);
        /* Sixty years of rain coming off that band. Staining is what tells the
         * player the wall is stone that has been outside, and it hangs from a
         * band because that is the only place water can leave a wall. */
        int streak = (int)floorf(s->cam_x / 96.0f) - 1;
        for (float sx = art_scroll(s->cam_x, 1.0f, 96.0f) - 96.0f;
             sx < (float)s->win_w; sx += 96.0f, ++streak)
        {
            unsigned sh = art_hash(streak, row * 7);
            if ((sh & 3u) == 0u)
                continue;
            float sw = 5.0f + (float)(sh % 11u);
            float sl = 30.0f + (float)((sh >> 6) % 70u);
            float px = sx + (float)((sh >> 12) % 70u);
            if (px < face_left || px + sw > face_right)
                continue;
            fx_vgrad(r, px, y + 6.0f, sw, sl, FX_INK,
                     (Uint8)(30u + (sh >> 18) % 26u), FX_INK, 0);
        }
    }

    /* One rainwater downpipe every four bays, bracketed to the stone. A blank
     * facade with no service on it is a drawing of a facade. */
    int pipe_bay = (int)floorf(s->cam_x / 512.0f) - 1;
    for (float px = art_scroll(s->cam_x, 1.0f, 512.0f) - 512.0f;
         px < (float)s->win_w; px += 512.0f, ++pipe_bay)
    {
        float x = px + 40.0f;
        if (x < face_left + 10.0f || x + 6.0f > face_right - 10.0f)
            continue;
        fx_rect_a(r, FX_INK, 90, x + 6.0f, top, 5.0f, height);
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.2f), x, top, 6.0f, height);
        fx_rect(r, fx_mix(art->wall, art->wall_light, 0.4f), x + 1.0f, top,
                1.0f, height);
        for (float bracket = fmodf(HUD_HEIGHT - s->cam_y, 96.0f) - 96.0f;
             bracket < (float)s->win_h; bracket += 96.0f)
        {
            fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.3f), x - 2.0f,
                    bracket, 10.0f, 3.0f);
            fx_rect_a(r, art->trim_hi, 90, x - 2.0f, bracket, 10.0f, 1.0f);
        }
    }

    /* The two returns. A face lit the same all the way to its own corner has
     * no depth: the left one goes into shadow and the right one catches the
     * light, and both fade inward instead of stopping at a line. */
    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.4f), face_left, top,
            7.0f, height);
    fx_hgrad(r, face_left + 7.0f, top, 34.0f, height, FX_INK, 64, FX_INK, 0);
    fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.4f), face_right - 7.0f, top,
            7.0f, height);
    fx_hgrad(r, face_right - 41.0f, top, 34.0f, height,
             art->wall_light, 0, art->wall_light, 34);

    float roof_y = HUD_HEIGHT - s->cam_y;
    if (roof_y >= top - 10.0f && roof_y <= (float)s->win_h)
    {
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.5f), face_left - 5.0f,
                roof_y, face_width + 10.0f, 10.0f);
        fx_rect(r, art->trim_hi, face_left - 5.0f, roof_y, face_width + 10.0f,
                3.0f);
    }
}

/* Distant towers, at a parallax slow enough to sell the height. */
static void facade_skyline(const LevelArtScene *s, const LevelThemeArt *art,
                           float lit_alpha)
{
    SDL_Renderer *r = s->renderer;
    float skyline_shift = fmodf(s->cam_y * 0.14f, 110.0f);
    for (int i = 0; i < 9; ++i)
    {
        unsigned h = art_hash(i, 307);
        float tower_w = 74.0f + (float)(h % 58u);
        float tower_h = 100.0f + (float)((h >> 7) % 180u);
        float x = (float)i * 104.0f - fmodf(s->cam_x * 0.08f, 104.0f);
        float y = (float)s->win_h - tower_h + skyline_shift;
        fx_rect(r, art->far_shape, x, y, tower_w, tower_h);
        if (lit_alpha <= 0.0f)
            continue;
        for (float wy = y + 18.0f; wy < (float)s->win_h; wy += 28.0f)
            for (float wx = x + 13.0f; wx < x + tower_w - 8.0f; wx += 22.0f)
                if (((int)(wx + wy) + i) % 3 == 0)
                    fx_rect_a(r, art->near_shape, (Uint8)lit_alpha, wx, wy,
                              5.0f, 8.0f);
    }
}

/*
 * The cordon, seen from the wall.
 *
 * The demand broadcast at 00:20 was theatre, and this is what it bought: every
 * unit in the city ringing the block and not one of them inside the building.
 * Chuck is the only part of that plan nobody accounted for, and out on the
 * masonry he is the only person in the city who can see both sides of it at
 * once — which is a thing worth being able to look down at.
 *
 * It is drawn as light rather than as vehicles because there is no street in
 * frame: the climb starts several storeys up, and a row of little cars would
 * have to be invented somewhere below the bottom edge. What a cordon actually
 * does to a tower is wash the lower face in blue from underneath, out of step
 * with itself because a dozen light bars are never in phase. The strength
 * falls away with the hour: the third sector is the lowest wall and still in
 * the thick of it, the storm climb is higher and wetter, and by the dawn
 * climb it is a suggestion. Above the weather there is nothing to see at all.
 */
static void facade_cordon(const LevelArtScene *s, float top, float height,
                          float strength)
{
    if (strength <= 0.0f)
        return;
    SDL_Renderer *r = s->renderer;
    /* Emergency-beacon blue. The palette has no such colour and should not:
     * FX_CYAN is the game's technology accent and FX_LAMP is a fluorescent
     * tube, and a police bar is neither — it is a saturated signal blue that
     * exists nowhere else in the building. Named once, here, so it cannot
     * spread. The red half of a light bar is FX_RED, which is exactly what
     * the palette's danger red is for. */
    static const SDL_Color CORDON_BLUE = {60, 116, 236, 255};

    float floor_y = top + height;
    /* Six bars, each on its own beat and its own patch of street. Two rates
     * that do not divide into one another is the whole trick: in phase they
     * would read as one lamp behind the camera. */
    for (int bar = 0; bar < 6; ++bar)
    {
        unsigned h = art_hash(bar * 23, 401);
        float x = (float)s->win_w * (0.08f + (float)(h % 88u) * 0.01f);
        bool red_half = (bar & 1) != 0;
        float rate = red_half ? 2.6f : 3.3f;
        float beat = fmodf(s->time * rate + (float)(h % 100u) * 0.01f, 1.0f);
        if (beat > 0.34f)
            continue;
        float flash = 1.0f - beat / 0.34f;
        SDL_Color c = red_half ? FX_RED : CORDON_BLUE;
        fx_glow(r, x, floor_y + 30.0f, 150.0f + (float)(h % 60u), c,
                (Uint8)(52.0f * flash * strength));
    }
    /* And the light that actually lands on the wall: a cold rise off the
     * bottom edge, steady, because the sum of a dozen bars is steady even
     * where each one of them is not. */
    fx_vgrad(r, 0.0f, floor_y - 130.0f, (float)s->win_w, 130.0f,
             CORDON_BLUE, 0, CORDON_BLUE, (Uint8)(30.0f * strength));
}

/*
 * The one aircraft the cordon lets near the tower.
 *
 * A demand goes out on the wire at 00:20 and a news ship is over the block
 * within the hour; a police one would be a problem, because the helicopter on
 * this roof at the end of the night is the crew's ride out and nothing in the
 * sky can be allowed to contradict that. So it holds station well off the
 * building with its beacon going, drifts slowly, and passes behind the face
 * Chuck is climbing rather than over it — which is why it is drawn before the
 * shell and not after.
 */
static void facade_news_helicopter(const LevelArtScene *s, float top)
{
    SDL_Renderer *r = s->renderer;
    /* A long, slow traverse: it is holding a shot, not going anywhere. The
     * wrap happens well off the side of the frame, so it is never seen. */
    float cycle = fmodf(s->time * 0.045f, 1.0f);
    float x = -90.0f + cycle * ((float)s->win_w + 180.0f);
    /* It holds one altitude over the city rather than scrolling with the sky,
     * so the climb goes past it: high in the frame from the bottom of the
     * wall, level with Chuck somewhere in the middle, and below him by the
     * top. Wrapping the height the way the stars wrap would put a single
     * recognisable object through a visible jump every few hundred pixels. */
    float y = top + 240.0f - (s->cam_y - 500.0f) * 0.45f +
              sinf(s->time * 0.5f) * 7.0f;
    if (y < top - 20.0f || y > top + 470.0f)
        return;

    /* Body, boom and skids, all in the depth haze's own dark so it sits at the
     * same distance as the towers behind it. */
    SDL_Color hull = fx_mix(FX_SHADOW, FX_STEEL_DK, 0.45f);
    fx_rect(r, hull, x - 7.0f, y - 3.0f, 15.0f, 6.0f);
    fx_rect(r, hull, x + 7.0f, y - 1.0f, 13.0f, 2.0f);
    fx_rect(r, fx_mix(hull, FX_STEEL_LT, 0.3f), x + 18.0f, y - 5.0f, 2.0f, 6.0f);
    fx_rect(r, hull, x - 6.0f, y + 3.0f, 12.0f, 1.0f);
    /* The rotor is a blur, not blades: two blades at this size would strobe. */
    fx_rect_a(r, fx_mix(hull, FX_PALE, 0.4f), 90, x - 17.0f, y - 6.0f,
              32.0f, 1.0f);
    /* Anti-collision beacon underneath, and the cabin light in the door where
     * somebody is leaning out of it with a camera. */
    float beacon = fmodf(s->time * 1.35f, 1.0f);
    if (beacon < 0.16f)
    {
        Uint8 a = (Uint8)((1.0f - beacon / 0.16f) * 190.0f);
        fx_rect_a(r, FX_RED, a, x - 1.0f, y + 4.0f, 2.0f, 2.0f);
        fx_glow(r, x, y + 5.0f, 13.0f, FX_RED, (Uint8)(a / 2u));
    }
    fx_rect_a(r, FX_WARM, 130, x - 4.0f, y - 2.0f, 3.0f, 3.0f);
    fx_glow(r, x - 3.0f, y - 1.0f, 16.0f, FX_WARM, 34);
}

static void backdrop_facade(const LevelArtScene *s, const LevelThemeArt *art,
                            float top, float height)
{
    SDL_Renderer *r = s->renderer;

    switch (art->backdrop)
    {
    case BACKDROP_FACADE_STORM:
    {
        /* The storm's one cold light: the discharge itself, with the rain
         * lit by the same colour a step down so the two can never disagree.
         * Bluer than FX_LAMP on purpose — a discharge is not a fixture. */
        static const SDL_Color STORM_FLASH = {186, 206, 236, 255};
        /* Lightning is scheduled off a coarse slice of the clock so the whole
         * sky flashes at once; the wall is repainted wet underneath it. */
        float beat = fmodf(s->time, 7.3f);
        float flash = beat < 0.09f ? 1.0f
                                   : (beat < 0.24f && beat > 0.16f ? 0.6f
                                                                   : 0.0f);
        if (flash > 0.0f)
            fx_rect_a(r, STORM_FLASH,
                      (Uint8)(70.0f * flash), 0.0f, top, (float)s->win_w,
                      height);
        facade_skyline(s, art, 40.0f);
        /* Low cloud drifting across the towers. */
        for (int band = 0; band < 4; ++band)
        {
            float y = top + 40.0f + (float)band * 74.0f -
                      fmodf(s->cam_y * 0.05f, 300.0f);
            float x = fmodf(s->time * (7.0f + (float)band * 3.0f), 460.0f) -
                      230.0f;
            fx_rect_a(r, fx_mix(FX_SHADOW, FX_STEEL_DK, 0.8f), 90, x, y,
                      (float)s->win_w + 260.0f, 26.0f + (float)band * 7.0f);
        }
        facade_news_helicopter(s, top);
        facade_shell(s, art, top, height);
        /* Rain in two sheets: a fast near one and a slow far one. */
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_Color rain = fx_dim(STORM_FLASH, 0.94f);
        for (int drop = 0; drop < 150; ++drop)
        {
            unsigned h = art_hash(drop * 31, 613);
            bool near_sheet = (h & 1u) != 0u;
            float speed = near_sheet ? 900.0f : 480.0f;
            float x = fmodf((float)(h % 900u) - s->time * 130.0f + 900.0f,
                            900.0f) -
                      100.0f;
            float y = top + fmodf(s->time * speed + (float)((h >> 6) % 800u),
                                  height);
            SDL_SetRenderDrawColor(r, rain.r, rain.g, rain.b,
                                   near_sheet ? 90 : 45);
            fx_fill(r, x, y, 1.0f, near_sheet ? 14.0f : 8.0f);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        break;
    }
    case BACKDROP_FACADE_DAWN:
    {
        /* The sun sits just off the corner of the building; everything the
         * light touches goes warm, and the towers become silhouettes. */
        float sun_x = (float)s->win_w * 0.78f;
        float sun_y = (float)s->win_h * 0.72f;
        /* The sun is the game's warm light writ large: FX_WARM for the halo,
         * lifted toward cream at the core. */
        fx_glow(r, sun_x, sun_y, 300.0f, FX_WARM, 110);
        fx_glow(r, sun_x, sun_y, 120.0f, fx_mix(FX_WARM, FX_CREAM, 0.65f), 150);
        facade_skyline(s, art, 0.0f);
        for (int band = 0; band < 3; ++band)
        {
            float y = (float)s->win_h - 150.0f + (float)band * 34.0f;
            fx_rect_a(r, fx_mix(FX_WARM, FX_CREAM, 0.20f), 60, 0.0f, y,
                      (float)s->win_w, 12.0f);
        }
        facade_shell(s, art, top, height);
        /* Birds, far enough out to be scenery rather than the gameplay kind. */
        for (int bird = 0; bird < 5; ++bird)
        {
            unsigned h = art_hash(bird * 7, 55);
            float x = fmodf(s->time * (13.0f + (float)(h % 9u)) +
                                (float)(h % 640u),
                            (float)s->win_w + 90.0f) -
                      45.0f;
            float y = top + 40.0f + (float)((h >> 5) % 130u) +
                      sinf(s->time * 1.4f + (float)bird) * 5.0f;
            float flap = sinf(s->time * 7.0f + (float)bird) * 3.0f;
            /* A bird against the dawn is a warm dark, not interface ink. */
            fx_set(r, fx_mix(FX_SHADOW, FX_WOOD, 0.3f));
            SDL_RenderLine(r, x - 5.0f, y - flap, x, y);
            SDL_RenderLine(r, x, y, x + 5.0f, y - flap);
        }
        break;
    }
    case BACKDROP_FACADE_HIGH:
    {
        /* The one purple in the game, and it is deliberate: seen from above
         * the weather, the sodium-and-neon city sums to a violet haze that no
         * single fixture in the palette owns — anchoring it to FX_LAMP or
         * FX_CYAN would hang a lamp in the sky. Owned here as one named trio
         * (the glow, the cloud deck it soaks, the deck's lit tops) so the hue
         * has exactly one home and cannot multiply. */
        static const SDL_Color CITY_GLOW = {150, 130, 220, 255};
        static const SDL_Color CLOUD_DECK = {58, 56, 104, 255};
        static const SDL_Color CLOUD_DECK_LIT = {96, 92, 156, 255};
        /* Above the weather: thin air, hard stars, and the city reduced to a
         * glow coming up through a floor of cloud. */
        for (int i = 0; i < 60; ++i)
        {
            unsigned h = art_hash(i * 11 + 5, 733);
            float x = (float)(h % (unsigned)s->win_w);
            float y = top + fmodf((float)((h >> 8) % 900u) -
                                      s->cam_y * 0.05f + 900.0f,
                                  900.0f);
            fx_rect_a(r, art->haze, (Uint8)(110 + h % 120u), x, y,
                      (h & 7u) == 0u ? 2.0f : 1.0f, 1.0f);
        }
        float deck = (float)s->win_h - 90.0f + fmodf(s->cam_y * 0.1f, 60.0f);
        fx_glow(r, (float)s->win_w * 0.4f, deck + 40.0f, 260.0f,
                CITY_GLOW, 70);
        for (int puff = 0; puff < 7; ++puff)
        {
            unsigned h = art_hash(puff * 19, 811);
            float x = fmodf(s->time * (5.0f + (float)(h % 5u)) +
                                (float)(h % 700u),
                            (float)s->win_w + 320.0f) -
                      160.0f;
            float w = 150.0f + (float)(h % 160u);
            fx_rect_a(r, CLOUD_DECK, 150, x,
                      deck + (float)((h >> 4) % 40u), w, 30.0f);
            fx_rect_a(r, CLOUD_DECK_LIT, 90, x,
                      deck + (float)((h >> 4) % 40u), w, 6.0f);
        }
        facade_shell(s, art, top, height);
        /* The building's own signage, mounted on the face he is climbing. */
        float sign_y = 260.0f + HUD_HEIGHT - s->cam_y;
        if (sign_y > top - 90.0f && sign_y < (float)s->win_h)
        {
            float buzz = fmodf(s->time * 3.1f, 6.0f) < 0.12f ? 0.35f : 1.0f;
            float sign_x = FACADE_BUILDING_SIDE_INSET + 40.0f - s->cam_x;
            for (int letter = 0; letter < 4; ++letter)
            {
                float lx = sign_x + (float)letter * 40.0f;
                fx_rect(r, fx_dim(art->accent, buzz), lx, sign_y, 6.0f, 54.0f);
                fx_rect(r, fx_dim(art->accent, buzz), lx, sign_y, 28.0f, 6.0f);
                fx_rect(r, fx_dim(art->accent, buzz), lx, sign_y + 24.0f,
                        24.0f, 6.0f);
                fx_glow(r, lx + 14.0f, sign_y + 27.0f, 46.0f, art->accent,
                        (Uint8)(56.0f * buzz));
            }
        }
        break;
    }
    case BACKDROP_FACADE_NIGHT:
    default:
    {
        /* Stars move more slowly than the climb, selling the height without
         * letting the backdrop interfere with the route. */
        for (int i = 0; i < 30; ++i)
        {
            unsigned h = art_hash(i * 13 + 7, 211);
            float x = (float)(h % (unsigned)s->win_w);
            float y = top + fmodf((float)((h >> 8) % 700u) -
                                      s->cam_y * 0.08f + 700.0f,
                                  700.0f);
            fx_rect_a(r, art->haze, (Uint8)(80 + h % 100u), x, y,
                      (h & 3u) == 0u ? 2.0f : 1.0f, 1.0f);
        }
        facade_skyline(s, art, 255.0f);
        facade_news_helicopter(s, top);
        facade_shell(s, art, top, height);
        break;
    }
    }

    /* Every climb keeps the same aircraft beacon so the four walls read as
     * the same tower at different hours. */
    float beacon = 0.45f + 0.55f * sinf(s->time * 2.2f);
    fx_glow(r, (float)s->win_w - 42.0f, top + 25.0f, 20.0f, art->accent,
            (Uint8)(35.0f + beacon * 55.0f));

    /* The street the cordon is standing in, last of all, because it is light
     * falling on the wall rather than something behind it. */
    float cordon = 0.0f;
    switch (art->backdrop)
    {
    case BACKDROP_FACADE_NIGHT:
        cordon = 1.0f; /* the first climb, and the lowest */
        break;
    case BACKDROP_FACADE_STORM:
        cordon = 0.60f;
        break;
    case BACKDROP_FACADE_DAWN:
        cordon = 0.26f;
        break;
    default:
        break; /* above the cloud deck there is nothing down there to see */
    }
    facade_cordon(s, top, height, cordon);
}

/* ---- Entry point ----------------------------------------------------- */

void level_art_backdrop(const LevelArtScene *s)
{
    const LevelThemeArt *art = level_art(s->level->map.theme);
    SDL_Renderer *r = s->renderer;
    const float oy = HUD_HEIGHT;
    const float fh = (float)s->win_h - oy;

    fx_rect(r, art->air_top, 0.0f, oy, (float)s->win_w, fh);
    fx_vgrad(r, 0.0f, oy, (float)s->win_w, fh, art->air_top, 255,
             art->air_bottom, 255);

    switch (art->backdrop)
    {
    case BACKDROP_FACADE_NIGHT:
    case BACKDROP_FACADE_STORM:
    case BACKDROP_FACADE_DAWN:
    case BACKDROP_FACADE_HIGH:
        backdrop_facade(s, art, oy, fh);
        return; /* Exteriors get no interior dust or floor haze. */
    case BACKDROP_LOBBY:
        backdrop_lobby(s, art, oy, fh);
        break;
    case BACKDROP_OFFICE:
        backdrop_office(s, art, oy, fh);
        break;
    case BACKDROP_SERVER:
        backdrop_server(s, art, oy, fh);
        break;
    case BACKDROP_CANTEEN:
        backdrop_canteen(s, art, oy, fh);
        break;
    case BACKDROP_LAB:
        backdrop_lab(s, art, oy, fh);
        break;
    case BACKDROP_ARCHIVE:
        backdrop_archive(s, art, oy, fh);
        break;
    case BACKDROP_SECURITY:
        backdrop_security(s, art, oy, fh);
        break;
    case BACKDROP_DUCTS:
        backdrop_ducts(s, art, oy, fh);
        break;
    case BACKDROP_PENTHOUSE:
        backdrop_penthouse(s, art, oy, fh);
        break;
    case BACKDROP_ROOF:
        backdrop_roof(s, art, oy, fh);
        break;
    case BACKDROP_RESTROOM:
        return; /* game_render.c derives the room from its own wall ring. */
    case BACKDROP_PLANT:
    default:
        backdrop_plant(s, art, oy, fh);
        break;
    }

    backdrop_motes(s, art, oy, 26);

    /* Depth haze pooling at the bottom of the frame. */
    fx_vgrad(r, 0.0f, oy + fh - 90.0f, (float)s->win_w, 90.0f, art->air_top, 0,
             art->air_top, 120);
}
