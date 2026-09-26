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
        {FX_STEEL_LT_RGBA}, {168, 184, 192, 255}, {FX_AMBER_DK_RGBA},
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
        {66, 116, 122, 255}, {148, 214, 208, 255}, {FX_CYAN_RGBA},
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
        {112, 68, 62, 255}, {202, 130, 114, 255}, {FX_RED_RGBA},
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

    /*
     * VAULT — the room the night was for, after they finished with it.
     *
     * It reuses the plate walls and the raised access floor rather than
     * introducing a material of its own, and that is the reading: a sub-vault
     * is a strongroom bolted into the same building everything else is, not an
     * architectural set piece. What makes it read as a vault is the palette —
     * cold steel, almost no lamp, and the one warm value in it is the emergency
     * lighting somebody left running when they walked out.
     */
    [LEVEL_THEME_VAULT] = {
        WALL_STYLE_PLATE, FLOOR_PANEL, BACKDROP_SERVER,
        {58, 64, 74, 255}, {22, 26, 33, 255}, {104, 114, 128, 255},
        {76, 84, 96, 255}, {148, 160, 176, 255}, {182, 196, 214, 255},
        {FX_INK_RGBA}, {16, 21, 29, 255},
        {9, 13, 19, 255}, {22, 28, 37, 255},
        {206, 214, 226, 255}, {126, 140, 158, 255}, 38},

    /* RESTROOM — the sublevel. Its interior is derived from the room's own
     * wall ring in game_render.c; only the tile material comes from here. */
    [LEVEL_THEME_RESTROOM] = {
        WALL_STYLE_TILE, FLOOR_CERAMIC, BACKDROP_RESTROOM,
        {96, 122, 120, 255}, {38, 56, 58, 255}, {186, 206, 196, 255},
        {112, 146, 141, 255}, {203, 211, 196, 255}, {110, 230, 170, 255},
        {7, 13, 22, 255}, {19, 31, 38, 255},
        {13, 23, 31, 255}, {29, 43, 49, 255},
        {202, 235, 222, 255}, {150, 180, 178, 255}, 90},

    /* ---- The five exterior climbs.  Same fields, exterior meanings:
     * air_* is sky, far_shape the distant towers, wall* the building face,
     * trim* the cornice stone, lamp a lit window, accent the signage.
     * Five entries and four backdrops: SLEET borrows the storm's. */

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

    /* MOON — the third: the storm has blown through, the sky has cleared, and
     * a low moon stands off the corner of the building. It is the one climb
     * lit from the side rather than from underneath, which is what makes it
     * read differently from the other three without moving the clock: the
     * whole night is thirty-eight minutes long (`NIGHT_CLOCK_*`), so the light
     * out here cannot come from the sun, and it does not. */
    [LEVEL_THEME_FACADE_MOON] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_MOON,
        {58, 64, 76, 255}, {38, 43, 53, 255}, {92, 101, 116, 255},
        {124, 134, 150, 255}, {186, 196, 210, 255}, {228, 88, 70, 255},
        {12, 17, 33, 255}, {46, 56, 82, 255},
        {20, 26, 44, 255}, {74, 84, 104, 255},
        {214, 224, 236, 255}, {162, 178, 198, 255}, 0},

    /*
     * HIGH — the fourth: the thinnest air the climb reaches, with the city
     * gone to a violet glow behind broken cloud a long way below and the
     * building's own neon signage for company.
     *
     * **Broken cloud rather than a deck, and that is the fiction rather than
     * the art direction.** This read "above the weather, a sea of cloud
     * below" while the climb *above* it is sleet, so the player climbed out
     * of the weather in sector 13 and back into it in 15 — the same shape of
     * mistake `FACADE_MOON` was corrected for, where the sun rose and set
     * inside five minutes of the night clock. What the beat is for survives
     * it whole, because what makes this climb read is one purple sky and a
     * city that has stopped being a street: scattered cloud says that just as
     * well as a floor of it, and it is what the seven drifting puffs in
     * `backdrop_facade` actually draw.
     */
    [LEVEL_THEME_FACADE_HIGH] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_HIGH,
        {50, 48, 58, 255}, {34, 33, 42, 255}, {76, 72, 86, 255},
        {104, 100, 112, 255}, {156, 150, 166, 255}, {236, 72, 168, 255},
        {6, 8, 24, 255}, {38, 32, 74, 255},
        {18, 18, 44, 255}, {96, 64, 148, 255},
        {150, 220, 248, 255}, {186, 196, 236, 255}, 0},

    /*
     * SLEET — the fifth climb, and the weather has come back in off the sea.
     *
     * It borrows the storm's backdrop rather than getting one of its own,
     * because it is the same weather at a different hour: the storm is the
     * climb in a hurry and this is the last stretch, wet again, with the roof
     * already in sight. The palette is the difference — the storm's stone is
     * warmed by the city under it and this is above that, so everything in it
     * is a step colder and a step paler.
     */
    [LEVEL_THEME_FACADE_SLEET] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_STORM,
        {56, 62, 72, 255}, {33, 38, 47, 255}, {88, 98, 112, 255},
        {110, 122, 136, 255}, {170, 184, 200, 255}, {FX_LAMP_RGBA},
        {8, 12, 22, 255}, {34, 44, 62, 255},
        {14, 20, 32, 255}, {58, 72, 94, 255},
        {198, 216, 232, 255}, {158, 176, 196, 255}, 0}};

const LevelThemeArt *level_art(LevelTheme theme)
{
    if (theme < 0 || theme >= LEVEL_THEME_COUNT)
        return &THEME_ART[LEVEL_THEME_PLANT];
    return &THEME_ART[theme];
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
 * A light that blinks, or the light it averages to.
 *
 * `live` is the brightness this frame and `mean` is what it comes to over a
 * whole cycle. With reduced motion asked for, the backdrop keeps casting the
 * same amount of light but stops modulating it — the header promises exactly
 * that, and a beacon still strobing behind a player who asked for it to stop
 * is the one kind of backdrop life that costs somebody something.
 */
static float art_pulse(const LevelArtScene *s, float live, float mean)
{
    return s->steady_lights ? mean : live;
}

/*
 * A translucent mass with its corners taken off: `fx_mass` under a blend.
 * Cloud out here is flat underneath and rounded on top, and a rectangle of
 * it reads as a stripe painted on the sky however soft its edges are.
 */
static void art_mass_a(SDL_Renderer *r, SDL_Color c, Uint8 alpha, float x,
                       float y, float w, float h, int top, int bottom)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    c.a = alpha;
    fx_mass(r, c, x, y, w, h, top, bottom);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * Rectangles laid down in one call.
 *
 * The repeating fine structure of a backdrop - tile joints, louvre slats,
 * rack perforation - is hundreds of one-pixel rects in one colour, and one
 * batch is what keeps it cheap enough to be worth drawing at all.
 */
enum
{
    ART_BATCH_MAX = 512
};

typedef struct
{
    SDL_FRect rects[ART_BATCH_MAX];
    int count;
} ArtBatch;

static void art_batch_flush(SDL_Renderer *r, ArtBatch *b, SDL_Color c,
                            Uint8 alpha)
{
    if (b->count <= 0)
        return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, alpha);
    SDL_RenderFillRects(r, b->rects, b->count);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    b->count = 0;
}

static void art_batch_add(SDL_Renderer *r, ArtBatch *b, SDL_Color c,
                          Uint8 alpha, float x, float y, float w, float h)
{
    if (b->count >= ART_BATCH_MAX)
        art_batch_flush(r, b, c, alpha);
    b->rects[b->count++] = (SDL_FRect){x, y, w, h};
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
 * surface with no light on it is a texture swatch, and seventeen sectors of
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
 * read identically in all seventeen sectors — and the pixels beneath it carry
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

/*
 * A hairline crack, placed and bent by the tile's own hash.
 *
 * Every material that cracks used to draw the same three strokes at the same
 * spot in the tile, so a wall of plating carried one crack glyph stamped at
 * the same offset in one tile of every eleven — the one mark on the wall a
 * player could count the grid by. A crack keeps its idea (a jag, a lit lip
 * on one side) and loses its address.
 */
static void wall_crack(SDL_Renderer *r, SDL_Color dark, SDL_Color lit,
                       float x, float y, unsigned h)
{
    float cx = x + 6.0f + (float)(h % 18u);
    float cy = y + 4.0f + (float)((h >> 5) % 10u);
    float lean = ((h >> 9) & 1u) ? 1.0f : -1.0f;
    int steps = 2 + (int)((h >> 10) % 3u);
    for (int step = 0; step < steps; ++step)
    {
        unsigned sh = h >> (unsigned)(12 + step * 3);
        float dx = lean * (float)(2u + sh % 3u);
        float dy = 3.0f + (float)((sh >> 1) % 4u);
        fx_set(r, dark);
        SDL_RenderLine(r, cx, cy, cx + dx, cy + dy);
        if (step == 0)
        {
            fx_set(r, lit);
            SDL_RenderLine(r, cx + 1.0f, cy, cx + dx + 1.0f, cy + dy);
        }
        cx += dx;
        cy += dy;
        lean = -lean;
    }
}

static void wall_plate(SDL_Renderer *r, const LevelThemeArt *art,
                       int col, int row, float x, float y, unsigned h,
                       bool weathered)
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
    else if ((h % 19u) == 0u)
    {
        wall_crack(r, fx_mix(base, art->wall_dark, 0.6f),
                   fx_mix(base, art->wall_light, 0.4f), x, y, h);
    }

    /* The plant floors and the plenum are the two nobody cleans, and steel
     * that has sweated for years bleeds rust down from every fixing. It
     * hangs from a rivet or a bolt because that is where the coating broke. */
    if (weathered)
    {
        if (top_edge && left_edge && (ph % 3u) == 0u)
            fx_vgrad(r, x + 3.0f, y + 5.0f, 2.0f,
                     6.0f + (float)((ph >> 4) % 12u), FX_RUST, 60, FX_RUST, 0);
        if ((row & 3) == 0 && (h % 5u) == 1u)
        {
            float bx = x + 5.0f + (float)((h >> 7) % 3u) * 11.0f;
            fx_vgrad(r, bx, y + 27.0f, 2.0f, 4.0f + (float)((h >> 11) % 5u),
                     FX_RUST, 70, FX_RUST, 0);
        }
        if ((h % 13u) == 4u)
            fx_vgrad(r, x + (float)((h >> 3) % 24u) + 4.0f, y, 5.0f,
                     TILE_SIZE, art->wall_dark, 50, art->wall_dark, 0);
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
        wall_crack(r, fx_mix(base, art->wall_dark, 0.5f),
                   fx_mix(base, art->wall_light, 0.3f), x, y, h);
    }
}

/*
 * Glazed tiling on a grout bed, in one of two settings.
 *
 * Every face used to carry a full-width lit line along its top and a dark one
 * along its bottom, which at eight pixels is not a glaze but a keycap — and a
 * wall of keycaps in a strict grid is the graph paper the border course below
 * was written to stop. A glazed tile is flat: what it shows is a thin grout
 * joint, a catch of light in one corner, and the room reflected across the
 * whole surface in broad streaks that do not care where one tile stops and the
 * next begins. The streaks are keyed to the wall rather than to the screen, so
 * they belong to it.
 *
 * The galley is set in metro tile instead — long faces in running bond with a
 * bevel, which is what a kitchen is tiled in and what tells it apart from the
 * clean room at a glance, where the two used to be the same swatch in two
 * palettes.
 */
static bool tile_sheen(float wx, float wy)
{
    float d = fmodf(wx - wy * 0.6f + 4096.0f, 176.0f);
    return d < 22.0f || (d > 30.0f && d < 36.0f);
}

static void wall_tile_material(SDL_Renderer *r, const LevelThemeArt *art,
                               int col, int row, float x, float y, unsigned h,
                               bool metro)
{
    fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.55f), x, y,
            TILE_SIZE, TILE_SIZE);
    float world_x = (float)col * TILE_SIZE;
    float world_y = (float)row * TILE_SIZE;
    if (metro)
    {
        for (int course = 0; course < 4; ++course)
        {
            int world_course = row * 4 + course;
            float by = y + (float)course * 8.0f;
            float offset = (world_course & 1) ? 8.0f : 0.0f;
            for (int slot = -1; slot <= 1; ++slot)
            {
                float bx = x + (float)slot * 16.0f + offset;
                unsigned bh = art_hash(col * 2 + slot, world_course);
                float left = bx + 1.0f > x ? bx + 1.0f : x;
                float right = bx + 16.0f < x + TILE_SIZE ? bx + 16.0f
                                                          : x + TILE_SIZE;
                if (right <= left)
                    continue;
                SDL_Color face = fx_mix(art->wall, art->wall_light,
                                        art_unit(bh, 2) * 0.18f);
                if ((bh % 23u) == 0u)
                    face = fx_mix(face, art->wall_dark, 0.35f);
                if (tile_sheen(world_x + (left - x), world_y + (by - y)))
                    face = fx_mix(face, art->wall_light, 0.22f);
                fx_rect(r, face, left, by + 1.0f, right - left, 7.0f);
                /* The bevel: lit along the top edge, in shade under the
                 * bottom one, which is the whole of a metro tile's relief. */
                fx_rect(r, fx_mix(face, art->wall_light, 0.4f), left, by + 1.0f,
                        right - left, 1.0f);
                fx_rect(r, fx_mix(face, art->wall_dark, 0.28f), left, by + 7.0f,
                        right - left, 1.0f);
                if (bx + 1.0f >= x)
                    fx_rect(r, fx_mix(face, art->wall_light, 0.25f), left,
                            by + 1.0f, 1.0f, 7.0f);
            }
        }
    }
    else
    {
        for (int ty = 0; ty < 4; ++ty)
        {
            for (int tx = 0; tx < 4; ++tx)
            {
                unsigned th = art_hash(col * 4 + tx, row * 4 + ty);
                float px = x + (float)tx * 8.0f + 1.0f;
                float py = y + (float)ty * 8.0f + 1.0f;
                SDL_Color face = fx_mix(art->wall, art->wall_light,
                                        art_unit(th, 2) * 0.16f);
                if ((th % 29u) == 0u)
                    face = fx_mix(face, art->wall_dark, 0.45f); /* a dead tile */
                if (tile_sheen(world_x + (float)tx * 8.0f,
                               world_y + (float)ty * 8.0f))
                    face = fx_mix(face, art->wall_light, 0.2f);
                fx_rect(r, face, px, py, 7.0f, 7.0f);
                /* The glaze catches the light in its top corner and the
                 * face sits a little proud of the joint under it. */
                fx_rect(r, fx_mix(face, art->wall_light, 0.55f), px, py, 2.0f,
                        1.0f);
                fx_rect(r, fx_mix(face, art->wall_dark, 0.16f), px, py + 6.0f,
                        7.0f, 1.0f);
                if ((th % 11u) == 0u)
                    fx_rect_a(r, art->trim_hi, 80, px + 1.0f, py + 1.0f, 2.0f,
                              2.0f);
            }
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
            fx_rect(r, band, sx, y + 17.0f, 7.0f, 6.0f);
            fx_rect(r, fx_mix(band, art->wall_light, 0.45f), sx, y + 17.0f,
                    7.0f, 1.0f);
            fx_rect(r, fx_mix(band, art->wall_dark, 0.4f), sx, y + 22.0f,
                    7.0f, 1.0f);
        }
    }

    /* Grime settles in the joints, and it settles where water runs: a
     * patch of darkened grout hanging under one tile in a few, as long as
     * the drip that left it. */
    if ((h % 13u) == 5u)
    {
        float gx = x + 8.0f * (float)((h >> 4) % 4u);
        float glen = 8.0f + (float)((h >> 8) % 3u) * 8.0f;
        fx_rect_a(r, art->wall_dark, 90, gx, y + 8.0f, 1.0f, glen);
        fx_vgrad(r, gx + 1.0f, y + 8.0f, 7.0f, glen, art->wall_dark, 40,
                 art->wall_dark, 0);
    }
    if ((h % 19u) == 0u)
    {
        /* A cracked tile, drawn across the grout so it reads as damage. */
        wall_crack(r, fx_mix(art->wall, art->wall_dark, 0.8f),
                   fx_mix(art->wall, art->wall_light, 0.5f), x, y, h);
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
        /* The galley is the one tiled floor that is set in metro. */
        wall_tile_material(r, art, col, row, x, y, h,
                           level->map.theme == LEVEL_THEME_CANTEEN);
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
        wall_plate(r, art, col, row, x, y, h,
                   level->map.theme == LEVEL_THEME_PLANT ||
                       level->map.theme == LEVEL_THEME_DUCTS);
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
     * way in all seventeen sectors or the level stops being legible. */
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

/*
 * One room's back wall, on screen: what an interior theme lays a wall out
 * against.
 *
 * Every interior used to draw one picture the size of the screen and let the
 * slabs cut it, so the wall did not move when the camera climbed and every
 * storey showed a slice of the same picture. `level_backdrop_plan` reads the
 * map into rooms (see level.h), and a theme is now handed one room at a time
 * with the renderer clipped to the piece of it being drawn. The room is
 * anchored to the building: it moves with the camera on both axes, exactly
 * as the slab above it and the floor under it do.
 *
 * The rules for a theme drawing into one:
 *
 * - **Stand things on `floor` and hang them from `top`**, and size anything
 *   tall to `height`: a rack or a window bay that does not fit the room is the
 *   defect this replaced. `height` runs from two tiles (the lowest storeys in
 *   the campaign) to a dozen (the canteen's hall), so a layout has to scale.
 * - **Horizontal repeats stay world-keyed** (`art_scroll` / `art_repeat` off
 *   `s->cam_x`), and a storey's variation comes off `seed` — never off
 *   `left`/`right`. Two pieces of one storey on either side of a partition get
 *   the same room and must draw the same wall, or the partition becomes a seam.
 * - **Cull to the clip**: a theme is called once per piece, so loop repeats
 *   from `clip_left` to `clip_right` rather than across the whole window.
 * - **What is far away is not on the wall.** A skyline through a window is at
 *   infinity and should barely move when the room does; draw it in the window
 *   against the screen (`s->win_h`, a small share of `s->cam_y`) rather than
 *   against the room, and let the window frame, which is on the wall, move.
 */
typedef struct
{
    float left; /* the room's own extent on screen, col0 to col1 + 1 */
    float right;
    float top;   /* the underside of the slab it hangs from */
    float floor; /* the top of the slab it stands on */
    float height;
    float clip_left; /* the part of it this call shows */
    float clip_right;
    float clip_top;
    float clip_bottom;
    int top_row; /* the rows it spans, for anything keyed to the storey */
    int floor_row;
    unsigned seed; /* one per layout, so every piece of a storey agrees */
} ArtRoom;

/*
 * Where a layer's repeats begin for the piece being drawn, and the world index
 * of that repeat.
 *
 * A theme is called once per piece of a storey, so a layer looped across the
 * whole window draws most of itself into a clip that throws it away. This
 * steps straight to the first repeat whose right-hand `reach` is still inside
 * the clip, keeping the index the repeat's own (`art_repeat` plus the whole
 * periods skipped), so nothing keyed to it changes as the piece moves.
 *
 * A non-zero `salt` starts the storey's run of that layer at a place of its
 * own, off the storey's seed, so the floor above is not the same furniture
 * stamped over this one; every piece of one storey shares the seed, so a
 * partition is still not a seam. A layer that has to stay on the building's
 * own grid — tiles, brick, mullions — passes nought.
 *
 * Every theme that arrived with the room layouts wrote its own copy of this,
 * and four of them were two identical pairs; these two are what is left.
 */
static float art_room_first(const LevelArtScene *s, const ArtRoom *room,
                            float factor, float period, float reach,
                            unsigned salt, int *index)
{
    float cam = s->cam_x;
    if (salt != 0u)
        cam += (float)(fx_hash(room->seed ^ (salt * 0x9e3779b9u)) %
                       (unsigned)period) /
               factor;
    float scroll = art_scroll(cam, factor, period);
    int skip = (int)floorf((room->clip_left - reach - scroll) / period);
    *index = art_repeat(cam, factor, period) + skip;
    return scroll + (float)skip * period;
}

/* The same step for a layer drawn `lead` pixels into each period, which only
 * ever skips forward and keeps one repeat of margin before the clip. */
static float art_room_first_lead(const LevelArtScene *s, const ArtRoom *room,
                                 float factor, float period, float lead,
                                 int *index)
{
    float x = art_scroll(s->cam_x, factor, period) + lead;
    int i = art_repeat(s->cam_x, factor, period);
    float skip = floorf((room->clip_left - x) / period) - 1.0f;
    if (skip > 0.0f)
    {
        x += skip * period;
        i += (int)skip;
    }
    *index = i;
    return x;
}

/*
 * Fine airborne dust, shared by every interior: it is what stops an empty
 * room from looking like a still image.
 *
 * It lives in the room. It used to be scattered across the whole screen in
 * screen space: x drifted with the camera once somebody noticed, and y never
 * did, so climbing a ladder slid the building past a cloud of dust that stayed
 * pinned to the glass. Keyed to the room's seed, spread over the room's own
 * height, and carried by the camera on both axes, it is dust in the air of the
 * room it is drawn in.
 */
static void room_motes(const LevelArtScene *s, const LevelThemeArt *art,
                       const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    float span = (float)(s->win_w + 80);
    float band = room->height - 14.0f;
    if (band < 1.0f)
        return;
    /* About one mote per two tiles of wall, so a low storey is not choked
     * with it and a tall hall is not bare. */
    int count = (int)(room->height * (float)s->win_w / 2048.0f) + 2;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < count; ++i)
    {
        unsigned h = fx_hash(room->seed + (unsigned)i * 0x9e3779b9u);
        float x = (float)fx_spread(h, span) +
                  s->time * (4.0f + (float)(i % 5)) - s->cam_x * 0.9f;
        x = x - span * floorf(x / span) - 40.0f;
        if (x < room->clip_left - 2.0f || x > room->clip_right + 2.0f)
            continue;
        float y = room->top + 6.0f + (float)fx_spread(h >> 8, band) +
                  sinf(s->time * 0.8f + (float)i) * 2.0f;
        SDL_SetRenderDrawColor(r, art->haze.r, art->haze.g, art->haze.b,
                               (Uint8)(22 + (h % 20u)));
        fx_fill(r, x, y, (i % 3 == 0) ? 2.0f : 1.0f, 1.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* The room's own air: darker up under its ceiling, and the depth haze pooled
 * on its floor — both of which used to be one gradient down the whole screen,
 * which put every storey's floor haze at the bottom of the frame and none of
 * it on the storeys above. */
static void room_air(const LevelArtScene *s, const LevelThemeArt *art,
                     const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    float w = room->clip_right - room->clip_left;
    fx_vgrad(r, room->clip_left, room->top, w, room->height, art->air_top, 255,
             art->air_bottom, 255);
}

static void room_floor_haze(const LevelArtScene *s, const LevelThemeArt *art,
                            const ArtRoom *room)
{
    float w = room->clip_right - room->clip_left;
    float haze = fminf(90.0f, room->height * 0.45f);
    fx_vgrad(s->renderer, room->clip_left, room->floor - haze, w, haze,
             art->air_top, 0, art->air_top, 120);
}

/*
 * PLANT — the mechanical floor: a hall of vessels and pump sets under a crane
 * runway, with a service gallery halfway up the taller storeys.
 *
 * It used to be one screen-sized picture, so every storey showed a horizontal
 * slice of the same hall: a gallery cut off by the slab above it, the head of
 * a tank carrying on in the storey overhead. Laid out per room it is a hall
 * *of that storey*: the vessels stand on its floor and rise to a height the
 * room can hold, the roof steel hangs from its ceiling, and a storey tall
 * enough for two levels of plant (eight tiles or more) gets a gallery across
 * its middle with its own cabinets on it, while a lower one is a single level
 * with its cabinets on the floor. Nothing is ever taller than the room it
 * stands in.
 */


/* A horizontal pipe as a solid: lit along its crown, rolling into shade
 * underneath, and laying a soft shadow on whatever is behind it. */
static void plant_pipe(SDL_Renderer *r, SDL_Color pipe, SDL_Color light,
                       float x, float y, float w, float thick)
{
    fx_rect(r, pipe, x, y, w, thick);
    fx_rect(r, fx_mix(pipe, light, 0.5f), x, y, w, 1.0f);
    if (thick >= 5.0f)
        fx_rect(r, fx_mix(pipe, light, 0.2f), x, y + 1.0f, w, 1.0f);
    fx_rect(r, fx_mix(pipe, FX_INK, 0.6f), x, y + thick - 1.0f, w, 1.0f);
    fx_rect_a(r, FX_INK, 50, x, y + thick, w, 3.0f);
}

/* A bolted flange on a pipe: the collar stands proud of the run, lit on the
 * face toward the light and dark on the joint behind it. */
static void plant_flange(SDL_Renderer *r, SDL_Color pipe, SDL_Color light,
                         float x, float y, float thick)
{
    fx_rect(r, fx_mix(pipe, light, 0.4f), x, y - 1.0f, 3.0f, thick + 2.0f);
    fx_rect(r, fx_mix(pipe, FX_INK, 0.5f), x + 3.0f, y - 1.0f, 1.0f,
            thick + 2.0f);
}

/* A gate valve on a vertical drop four pixels wide: the body swelling the
 * pipe, and its hand wheel turned toward the room — rim, spokes and hub, in
 * the amber the plant paints whatever somebody is meant to turn. */
static void plant_valve(SDL_Renderer *r, const LevelThemeArt *art,
                        SDL_Color pipe, float x, float y)
{
    SDL_Color wheel = fx_dim(art->accent, 0.62f);
    fx_rect(r, fx_mix(pipe, FX_INK, 0.25f), x - 1.0f, y - 5.0f, 6.0f, 11.0f);
    fx_mass(r, wheel, x - 3.0f, y - 4.0f, 10.0f, 9.0f, 2, 2);
    fx_mass(r, fx_mix(pipe, FX_INK, 0.55f), x - 1.0f, y - 2.0f, 6.0f, 5.0f,
            1, 1);
    fx_rect(r, wheel, x + 1.0f, y - 2.0f, 2.0f, 5.0f);
    fx_rect(r, wheel, x - 1.0f, y, 6.0f, 1.0f);
    fx_rect(r, fx_mix(wheel, art->trim_hi, 0.30f), x - 1.0f, y - 4.0f, 6.0f,
            1.0f);
    fx_rect(r, fx_mix(wheel, FX_INK, 0.45f), x - 1.0f, y + 4.0f, 6.0f, 1.0f);
}

/*
 * The control room at the back of a gallery, seen through its window: the
 * room's own ceiling strip, a console with its screens lit, the mullions, and
 * the glass catching the hall's light. It is the one warm, lived-in thing on
 * the plant floor, which is exactly why it is only in the odd bay.
 */
static void plant_control_room(SDL_Renderer *r, const LevelThemeArt *art,
                               float x, float top, float sill, unsigned h)
{
    SDL_Color panel = fx_mix(art->near_shape, art->wall, 0.20f);
    SDL_Color inside = fx_mix(art->far_shape, FX_INK, 0.25f);
    float gx = x + 8.0f;
    float gw = 132.0f;
    float gy = top + 6.0f;
    float gh = sill - gy;
    fx_rect(r, panel, x, top, 148.0f, sill + 22.0f - top);
    fx_rect(r, fx_mix(panel, art->wall_light, 0.30f), x, top, 148.0f, 1.0f);
    fx_rect_a(r, FX_INK, 70, x + 144.0f, top + 1.0f, 4.0f, sill + 21.0f - top);
    fx_rect(r, inside, gx, gy, gw, gh);
    /* The room's ceiling light and what it lays on the back wall. */
    SDL_Color strip = fx_dim(FX_LAMP, 0.36f);
    fx_rect(r, strip, gx + 10.0f, gy + 2.0f, gw - 20.0f, 1.0f);
    fx_vgrad(r, gx, gy + 3.0f, gw, gh * 0.7f, FX_LAMP, 20, FX_LAMP, 0);
    /* A console along the glass, and its screens. */
    SDL_Color desk = fx_mix(art->near_shape, art->wall, 0.22f);
    float dy = sill - 12.0f;
    fx_rect(r, desk, gx + 4.0f, dy, gw - 8.0f, 12.0f);
    fx_rect(r, fx_mix(desk, art->wall_light, 0.30f), gx + 4.0f, dy, gw - 8.0f,
            1.0f);
    for (int m = 0; m < 4; ++m)
    {
        unsigned mh = fx_hash(h + (unsigned)m * 0x2545f491u);
        float mx = gx + 14.0f + (float)m * 28.0f;
        fx_rect(r, fx_mix(desk, FX_INK, 0.30f), mx, dy - 9.0f, 14.0f, 9.0f);
        fx_rect(r, fx_dim(FX_CYAN, 0.26f + art_unit(mh, 3) * 0.14f), mx + 1.0f,
                dy - 8.0f, 12.0f, 6.0f);
        fx_rect_a(r, FX_CYAN, 14, mx - 3.0f, dy - 11.0f, 20.0f, 12.0f);
    }
    /* Mullions, the frame, and the hall's light on the glass. */
    for (int mul = 1; mul < 3; ++mul)
        fx_rect(r, panel, gx + (float)mul * 44.0f, gy, 2.0f, gh);
    fx_rect(r, fx_mix(panel, art->wall_light, 0.40f), gx, gy - 1.0f, gw, 1.0f);
    fx_rect(r, fx_mix(panel, FX_INK, 0.40f), gx, sill, gw, 2.0f);
    fx_rect_a(r, art->trim_hi, 10, gx + 10.0f, gy, 20.0f, gh);
    fx_rect_a(r, art->trim_hi, 7, gx + 36.0f, gy, 7.0f, gh);
    fx_rect_a(r, art->trim_hi, 7, gx + 98.0f, gy, 14.0f, gh);
}

/*
 * A shell-and-tube exchanger lying on its saddles: a horizontal vessel lit
 * along its crown and rolling into shade underneath, dished heads at either
 * end, the channel flange at the inlet end, and the two nozzles that carry its
 * water up to the header. It is the one big shape at floor level, which is
 * what the lower half of a tall hall is otherwise short of.
 */
static void plant_exchanger(SDL_Renderer *r, const LevelThemeArt *art,
                            SDL_Color pipe, float x, float w, float floor,
                            float header)
{
    const float d = 26.0f;
    const float y = floor - 9.0f - d;
    SDL_Color shell = fx_mix(art->near_shape, art->wall, 0.30f);
    SDL_Color lit = fx_mix(shell, art->wall_light, 0.35f);
    SDL_Color dark = fx_mix(shell, FX_INK, 0.45f);

    /* The nozzles first, so the body sits in front of their roots. */
    for (int n = 0; n < 2; ++n)
    {
        float nx = n == 0 ? x + 34.0f : x + w - 44.0f;
        if (header + 6.0f < y)
        {
            fx_rect(r, pipe, nx, header + 6.0f, 4.0f, y - header - 6.0f);
            fx_rect(r, fx_mix(pipe, art->wall_light, 0.5f), nx, header + 6.0f,
                    1.0f, y - header - 6.0f);
        }
        fx_rect(r, fx_mix(pipe, art->wall_light, 0.40f), nx - 2.0f, y - 3.0f,
                8.0f, 2.0f);
    }
    /* Saddles and their base plates. */
    for (int k = 0; k < 2; ++k)
    {
        float sx = x + 18.0f + (float)k * (w - 54.0f);
        fx_rect(r, fx_mix(shell, FX_INK, 0.30f), sx, y + d - 4.0f, 18.0f,
                floor - (y + d - 4.0f));
        fx_rect(r, fx_mix(shell, art->wall_light, 0.18f), sx - 2.0f,
                floor - 3.0f, 22.0f, 3.0f);
    }
    /* The shell, lit along its crown. */
    fx_rect(r, shell, x + 4.0f, y, w - 8.0f, d);
    fx_vgrad(r, x + 4.0f, y, w - 8.0f, floorf(d * 0.35f), lit, 255, shell,
             255);
    fx_vgrad(r, x + 4.0f, y + floorf(d * 0.35f), w - 8.0f,
             d - floorf(d * 0.35f), shell, 255, dark, 255);
    fx_rect_a(r, art->haze, 30, x + 4.0f, y + 3.0f, w - 8.0f, 1.0f);
    /* Dished heads, stepping in to either end. */
    static const float step[4] = {2.0f, 4.0f, 7.0f, 10.0f};
    for (int i = 0; i < 4; ++i)
    {
        float hy = y + step[i];
        float hh = d - 2.0f * step[i];
        fx_rect(r, shell, x + 3.0f - (float)i, hy, 1.0f, hh);
        fx_rect(r, lit, x + 3.0f - (float)i, hy, 1.0f, 1.0f);
        fx_rect(r, dark, x + w - 4.0f + (float)i, hy, 1.0f, hh);
    }
    /* Girth seams, the channel flange and the plate. */
    for (int seam = 1; seam < 3; ++seam)
        fx_rect_a(r, FX_INK, 80, x + 4.0f + (w - 8.0f) * (float)seam / 3.0f, y,
                  1.0f, d);
    fx_rect(r, fx_mix(shell, art->wall_light, 0.40f), x + 12.0f, y - 2.0f,
            3.0f, d + 4.0f);
    fx_rect(r, fx_mix(shell, FX_INK, 0.50f), x + 15.0f, y - 2.0f, 1.0f,
            d + 4.0f);
    fx_rect(r, fx_mix(shell, art->wall_light, 0.22f), x + floorf(w * 0.5f),
            y + 8.0f, 12.0f, 5.0f);
    fx_rect_a(r, FX_INK, 90, x - 2.0f, floor - 2.0f, w + 4.0f, 2.0f);
}

/*
 * A vessel standing on the hall floor.
 *
 * A tank drawn as a flat slab is a box, and a hall of boxes is a hall of
 * placeholders. What says "tank" at this distance is the curve of the shell —
 * a highlight a third of the way across where the ceiling light catches it,
 * the far flank rolling into shade, a thread of bounce light along its edge —
 * the hoops that hold it, a domed head, the skirt it stands on, and the caged
 * ladder somebody climbs to reach the top of it.
 */
static void plant_vessel(SDL_Renderer *r, const LevelThemeArt *art, float x,
                         float w, float top, float floor, unsigned h)
{
    const float skirt = 12.0f;
    float body = floor - skirt - top;
    SDL_Color shell = art->far_shape;
    SDL_Color lit = fx_mix(art->far_shape, art->haze, 0.18f);
    SDL_Color head = fx_mix(art->far_shape, art->near_shape, 0.5f);

    fx_rect(r, shell, x, top, w, body);
    fx_hgrad(r, x, top, w * 0.34f, body, lit, 40, lit, 255);
    fx_hgrad(r, x + w * 0.34f, top, w * 0.36f, body, lit, 255, lit, 0);
    fx_hgrad(r, x + w * 0.52f, top, w * 0.48f, body, FX_INK, 0, FX_INK, 110);
    fx_rect_a(r, art->haze, 26, x + floorf(w * 0.30f), top + 2.0f, 1.0f,
              body - 4.0f);
    fx_rect_a(r, art->haze, 16, x + w - 3.0f, top + 4.0f, 1.0f, body - 8.0f);
    /* Hoops, laid over the shading so each one turns with the shell. */
    for (float hy = top + 16.0f; hy < top + body - 8.0f; hy += 24.0f)
    {
        fx_rect_a(r, FX_INK, 96, x, hy, w, 1.0f);
        fx_rect_a(r, art->haze, 24, x, hy + 1.0f, w, 1.0f);
    }
    /* The domed head, the one line of light along its crown, and the side of
     * it the ceiling light does not reach. */
    fx_mass(r, head, x + 2.0f, top - 14.0f, w - 4.0f, 15.0f, 7, 0);
    fx_mass(r, fx_mix(art->far_shape, art->haze, 0.20f), x + 10.0f,
            top - 14.0f, w * 0.5f, 2.0f, 2, 0);
    fx_hgrad(r, x + w * 0.55f, top - 7.0f, w * 0.45f - 3.0f, 7.0f, FX_INK, 0,
             FX_INK, 80);
    fx_rect(r, fx_mix(head, art->haze, 0.10f), x, top, w, 1.0f);

    /* The skirt it stands on, with the opening into it, and the contact
     * shadow on the floor. */
    fx_rect(r, fx_mix(shell, FX_INK, 0.30f), x + 4.0f, floor - skirt,
            w - 8.0f, skirt);
    fx_rect(r, fx_mix(shell, FX_INK, 0.65f), x + floorf(w * 0.38f),
            floor - skirt + 3.0f, 10.0f, skirt - 3.0f);
    fx_rect_a(r, FX_INK, 90, x - 2.0f, floor - 2.0f, w + 6.0f, 2.0f);

    /* A manway low on the lit flank and the stencilled plate above it. */
    float mw = floor - skirt - 24.0f;
    if (mw > top + 20.0f)
    {
        fx_mass(r, fx_mix(head, art->haze, 0.10f), x + 9.0f, mw, 15.0f, 15.0f,
                4, 4);
        fx_mass(r, fx_mix(shell, FX_INK, 0.40f), x + 11.0f, mw + 2.0f, 11.0f,
                11.0f, 3, 3);
        for (int bolt = 0; bolt < 4; ++bolt)
            fx_rect(r, fx_mix(head, art->haze, 0.22f),
                    x + 10.0f + (float)(bolt & 1) * 12.0f,
                    mw + 1.0f + (float)(bolt >> 1) * 12.0f, 1.0f, 1.0f);
    }
    if (body > 70.0f)
        fx_rect(r, fx_dim(art->accent, 0.34f + art_unit(h, 5) * 0.12f),
                x + floorf(w * 0.14f), top + 24.0f, 18.0f, 5.0f);

    /* Caged access ladder down the shaded flank. */
    SDL_Color rail = fx_mix(art->far_shape, art->near_shape, 0.55f);
    float lx = x + w - 14.0f;
    fx_rect(r, rail, lx, top - 8.0f, 1.0f, floor - top + 8.0f);
    fx_rect(r, rail, lx + 7.0f, top - 8.0f, 1.0f, floor - top + 8.0f);
    for (float ry = top - 4.0f; ry < floor; ry += 6.0f)
        fx_rect(r, rail, lx + 1.0f, ry, 6.0f, 1.0f);
    for (float cy = top + 10.0f; cy < floor - 28.0f; cy += 18.0f)
        fx_rect_a(r, rail, 150, lx - 3.0f, cy, 14.0f, 1.0f);
}

/*
 * A service deck across one bay: the grating, the toe plate and the shadow the
 * deck throws on what is under it, a handrail on posts, and the struts that
 * carry it off the columns either side.
 */
static void plant_walkway(SDL_Renderer *r, const LevelThemeArt *art, float x,
                          float w, float y)
{
    SDL_Color deck = fx_mix(art->near_shape, art->wall, 0.24f);
    SDL_Color rail = fx_mix(deck, art->wall_light, 0.18f);
    fx_rect(r, deck, x, y, w, 5.0f);
    fx_rect(r, fx_mix(deck, art->wall_light, 0.34f), x, y, w, 1.0f);
    for (float g = x + 2.0f; g < x + w - 1.0f; g += 4.0f)
        fx_rect(r, fx_mix(deck, FX_INK, 0.45f), g, y + 2.0f, 2.0f, 2.0f);
    fx_rect_a(r, FX_INK, 90, x, y + 5.0f, w, 3.0f);
    fx_rect_a(r, FX_INK, 40, x, y + 8.0f, w, 5.0f);

    /* Handrail, mid-rail and posts. */
    fx_rect(r, fx_mix(deck, art->wall_light, 0.26f), x, y - 18.0f, w, 1.0f);
    fx_rect(r, fx_mix(deck, FX_INK, 0.30f), x, y - 17.0f, w, 1.0f);
    fx_rect(r, fx_mix(deck, FX_INK, 0.15f), x, y - 9.0f, w, 1.0f);
    for (float post = x + 18.0f; post < x + w - 4.0f; post += 24.0f)
        fx_rect(r, rail, post, y - 18.0f, 1.0f, 18.0f);

    /* The struts off the column on either side of the bay. */
    fx_set(r, fx_mix(art->far_shape, art->near_shape, 0.6f));
    for (int t = 0; t < 2; ++t)
    {
        float o = (float)t;
        SDL_RenderLine(r, x + 12.0f, y + 26.0f + o, x + 34.0f, y + 5.0f + o);
        SDL_RenderLine(r, x + w - 2.0f, y + 26.0f + o, x + w - 24.0f,
                       y + 5.0f + o);
    }
}

/* A column of the hall's frame, floor to roof: an I-section with its lit
 * flange, the web set back between the flanges, the shadow it throws on the
 * wall beside it, and the knee braces that tie it to the roof beam. */
static void plant_column(SDL_Renderer *r, const LevelThemeArt *art, float x,
                         float top, float floor)
{
    SDL_Color col = fx_mix(art->near_shape, art->wall, 0.30f);
    float h = floor - top;
    fx_rect(r, col, x, top, 10.0f, h);
    fx_rect(r, fx_mix(col, art->wall_light, 0.35f), x, top, 1.0f, h);
    fx_rect(r, fx_mix(col, FX_INK, 0.35f), x + 3.0f, top, 4.0f, h);
    fx_rect(r, fx_mix(col, FX_INK, 0.55f), x + 9.0f, top, 1.0f, h);
    fx_rect_a(r, FX_INK, 60, x + 10.0f, top, 4.0f, h);
    /* Stiffener plates every so often, and the base plate with its bolts. */
    for (float sy = top + 40.0f; sy < floor - 20.0f; sy += 64.0f)
        fx_rect(r, fx_mix(col, art->wall_light, 0.18f), x + 3.0f, sy, 4.0f,
                1.0f);
    fx_rect(r, fx_mix(col, art->wall_light, 0.16f), x - 3.0f, floor - 3.0f,
            16.0f, 3.0f);
    fx_rect(r, fx_mix(col, art->wall_light, 0.40f), x - 3.0f, floor - 3.0f,
            16.0f, 1.0f);
    fx_set(r, fx_mix(col, FX_INK, 0.1f));
    for (int t = 0; t < 2; ++t)
    {
        float o = (float)t;
        SDL_RenderLine(r, x, top + 28.0f + o, x - 20.0f, top + 7.0f + o);
        SDL_RenderLine(r, x + 9.0f, top + 28.0f + o, x + 29.0f,
                       top + 7.0f + o);
    }
}

/* One room of the PLANT theme; see `ArtRoom` for what a room layout owes. */
static void backdrop_plant_room(const LevelArtScene *s,
                                const LevelThemeArt *art, const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float top = room->top;
    const float floor = room->floor;
    const float height = room->height;
    const float cw = room->clip_right - room->clip_left;

    /* Eight tiles is the least a storey can be and still hold a gallery with
     * headroom over it and room for plant under it; a lower storey is one
     * level, and a hall three storeys tall does not get a third deck,
     * because the tanks are what fill a hall that height. */
    const bool gallery = height >= 240.0f;
    const float deck = floor - floorf(height * 0.5f);
    const bool runway = height >= 240.0f;
    const float pipes_y = top + (runway ? 32.0f : 13.0f);
    const float lamps_y = pipes_y + 20.0f;

    /* Farthest layer: the vessels, standing on this storey's floor and as
     * tall as it lets them be, each with its flue up through the roof and a
     * pump house beside it. The storey's seed shifts the row along, so the
     * tanks do not stack in a column from one storey to the next the way the
     * frame of the building does. */
    SDL_Color hoop = fx_mix(art->far_shape, FX_INK, 0.45f);
    SDL_Color hoop_lit = fx_mix(art->far_shape, art->near_shape, 0.7f);
    float phase = (float)(room->seed % 7u) * 32.0f;
    int bank = 0;
    for (float x = art_room_first_lead(s, room, 0.10f, 224.0f, phase - 240.0f,
                                      &bank);
         x < room->clip_right + 16.0f; x += 224.0f, ++bank)
    {
        unsigned h = fx_hash(room->seed + (unsigned)bank * 0x9e3779b9u);
        float share = gallery ? 0.60f + art_unit(h, 0) * 0.18f
                              : 0.55f + art_unit(h, 0) * 0.16f;
        float tank_top = fmaxf(floor - height * share, top + 48.0f);
        float tw = 70.0f + (float)(h % 3u) * 6.0f;
        float tx = x + 24.0f;
        plant_vessel(r, art, tx, tw, tank_top, floor, h);

        /* The flue off the head, up through the roof, with the warning light
         * where it meets the ceiling. The light is FX_RED knocked back by
         * distance and lit for about a sixth of every cycle, so that is what
         * it averages to when the lights are held steady. */
        float fxp = tx + floorf(tw * 0.42f);
        SDL_Color flue = fx_mix(art->far_shape, art->near_shape, 0.30f);
        fx_rect(r, flue, fxp, top, 10.0f, tank_top - 14.0f - top);
        fx_rect(r, fx_mix(art->far_shape, art->haze, 0.09f), fxp, top, 2.0f,
                tank_top - 14.0f - top);
        fx_rect_a(r, FX_INK, 70, fxp + 7.0f, top, 3.0f,
                  tank_top - 14.0f - top);
        for (float fy = top + 22.0f; fy < tank_top - 24.0f; fy += 40.0f)
            fx_rect(r, hoop_lit, fxp - 1.0f, fy, 12.0f, 2.0f);
        float blink = art_pulse(s,
                                sinf(s->time * 1.4f + (float)(h % 7u)) > 0.86f
                                    ? 1.0f
                                    : 0.28f,
                                0.40f);
        fx_rect(r, fx_dim(FX_RED, 0.85f * blink), fxp + 3.0f, top + 10.0f,
                4.0f, 3.0f);

        /* The pump house: louvred, its roof catching the light, fed by a
         * pipe from the vessel. */
        float house = fminf(64.0f, height * 0.30f);
        float hx = tx + tw + 30.0f;
        fx_rect(r, fx_mix(art->far_shape, art->near_shape, 0.6f), tx + tw,
                floor - house * 0.7f, 30.0f, 5.0f);
        fx_rect(r, hoop_lit, tx + tw, floor - house * 0.7f, 30.0f, 1.0f);
        fx_rect(r, hoop_lit, tx + tw + 12.0f, floor - house * 0.7f - 2.0f,
                3.0f, 9.0f);
        fx_rect(r, art->far_shape, hx, floor - house, 70.0f, house);
        fx_rect(r, hoop_lit, hx, floor - house, 70.0f, 1.0f);
        for (float ly = floor - house + 10.0f; ly < floor - 12.0f; ly += 5.0f)
            fx_rect(r, hoop, hx + 8.0f, ly, 30.0f, 2.0f);
        fx_hgrad(r, hx + 50.0f, floor - house, 20.0f, house, FX_INK, 0, FX_INK,
                 70);
    }

    /* The roof of the storey: the main beam along the ceiling, and on a
     * storey tall enough for it, the crane runway under it that every plant
     * hall is built around. */
    SDL_Color beam = fx_mix(art->near_shape, art->wall, 0.18f);
    fx_rect(r, beam, room->clip_left, top, cw, 7.0f);
    fx_rect(r, fx_mix(beam, art->wall_light, 0.30f), room->clip_left,
            top + 6.0f, cw, 1.0f);
    fx_rect_a(r, FX_INK, 70, room->clip_left, top + 7.0f, cw, 3.0f);
    if (runway)
    {
        fx_rect(r, beam, room->clip_left, top + 18.0f, cw, 6.0f);
        fx_rect(r, fx_mix(beam, art->wall_light, 0.38f), room->clip_left,
                top + 17.0f, cw, 1.0f);
        fx_rect(r, fx_mix(beam, FX_INK, 0.40f), room->clip_left, top + 23.0f,
                cw, 1.0f);
        fx_rect_a(r, FX_INK, 60, room->clip_left, top + 24.0f, cw, 3.0f);
    }

    /* Mid layer: the frame of the hall, bay by bay, and what stands in each
     * bay. */
    SDL_Color pipe = fx_mix(art->near_shape, art->wall, 0.45f);
    SDL_Color pipe_lit = art->wall_light;
    SDL_Color motor = fx_mix(art->near_shape, art->wall, 0.40f);
    SDL_Color cab = fx_mix(art->near_shape, art->wall, 0.28f);
    int bay = 0;
    for (float x = art_room_first_lead(s, room, 0.18f, 192.0f, -32.0f, &bay);
         x < room->clip_right + 32.0f; x += 192.0f, ++bay)
    {
        unsigned bh = fx_hash(room->seed ^ ((unsigned)bay * 0x85ebca6bu));

        plant_column(r, art, x, top + 7.0f, floor);

        /* Two service pipes along the head of the bay, flanged. */
        for (int run = 0; run < 2; ++run)
        {
            float py = pipes_y + (float)run * 8.0f;
            float thick = run == 0 ? 5.0f : 4.0f;
            plant_pipe(r, pipe, pipe_lit, x, py, 192.0f, thick);
            for (int flange = 0; flange < 4; ++flange)
                plant_flange(r, pipe, pipe_lit,
                             x + 20.0f + (float)flange * 48.0f +
                                 (float)run * 17.0f,
                             py, thick);
        }
        /* The pipe hangers from the roof beam. */
        for (int hang = 0; hang < 2; ++hang)
        {
            float hx = x + 52.0f + (float)hang * 96.0f;
            fx_rect(r, fx_mix(pipe, FX_INK, 0.45f), hx, top + 7.0f, 1.0f,
                    pipes_y + 14.0f - top - 7.0f);
            fx_rect(r, fx_mix(pipe, FX_INK, 0.30f), hx - 3.0f,
                    pipes_y + 12.0f, 7.0f, 1.0f);
        }

        /* The crane: in the odd bay its crab sits on the runway with the hook
         * let down on its falls, painted in the plant's amber. */
        if (runway && (bh % 5u) == 2u)
        {
            float cx = x + 96.0f + (float)(bh % 40u) - 20.0f;
            float drop = height * (0.24f + art_unit(bh, 8) * 0.14f);
            SDL_Color crab = fx_mix(art->near_shape, art->wall, 0.36f);
            fx_rect(r, crab, cx - 14.0f, top + 10.0f, 28.0f, 8.0f);
            fx_rect(r, fx_mix(crab, art->wall_light, 0.35f), cx - 14.0f,
                    top + 10.0f, 28.0f, 1.0f);
            fx_rect(r, fx_dim(art->accent, 0.40f), cx - 10.0f, top + 13.0f,
                    6.0f, 2.0f);
            fx_rect(r, fx_mix(pipe, FX_INK, 0.35f), cx - 3.0f, top + 24.0f,
                    1.0f, drop);
            fx_rect(r, fx_mix(pipe, FX_INK, 0.35f), cx + 2.0f, top + 24.0f,
                    1.0f, drop);
            float hy = top + 24.0f + drop;
            fx_mass(r, fx_dim(art->accent, 0.46f), cx - 5.0f, hy, 11.0f, 9.0f,
                    2, 2);
            fx_rect(r, fx_mix(art->accent, FX_INK, 0.75f), cx - 5.0f,
                    hy + 3.0f, 11.0f, 1.0f);
            fx_rect(r, fx_dim(art->accent, 0.30f), cx, hy + 9.0f, 2.0f, 5.0f);
            fx_rect(r, fx_dim(art->accent, 0.30f), cx - 2.0f, hy + 13.0f, 3.0f,
                    2.0f);
        }

        /* High-bay lamps hanging from the roof beam: small and dim, because
         * the lamps that light the playfield are the real ones and these are
         * the hall's own, further back. The warm ones flicker very rarely. */
        for (int lamp = 0; lamp < 2; ++lamp)
        {
            unsigned h = fx_hash(bh + (unsigned)lamp * 0x27d4eb2du);
            bool warm = (h & 1u) != 0u;
            float flicker = warm && ((h >> 3) & 7u) == 0u &&
                                    fmodf(s->time * 1.7f + (float)lamp, 4.0f) <
                                        0.09f
                                ? 0.3f
                                : 1.0f;
            flicker = art_pulse(s, flicker, 1.0f);
            SDL_Color lc = warm ? fx_dim(FX_SODIUM, 0.72f * flicker)
                                : fx_dim(FX_LAMP, 0.48f);
            float lx = x + 70.0f + (float)lamp * 72.0f;
            fx_rect(r, fx_mix(pipe, FX_INK, 0.4f), lx + 5.0f, top + 7.0f, 1.0f,
                    lamps_y - top - 7.0f);
            fx_mass(r, fx_mix(beam, art->wall_light, 0.12f), lx, lamps_y,
                    12.0f, 4.0f, 2, 0);
            fx_rect(r, lc, lx + 2.0f, lamps_y + 4.0f, 8.0f, 1.0f);
            fx_rect_a(r, lc, 30, lx, lamps_y + 3.0f, 12.0f, 4.0f);
            fx_light_cone(r, lx + 6.0f, lamps_y + 5.0f, 5.0f, 18.0f,
                          fminf(70.0f, height * 0.3f), lc, 16);
        }

        /* The level the cabinets stand on: the gallery on a two-level
         * storey, the floor on a single one. */
        float stand = gallery ? deck : floor;
        float above = stand - (lamps_y + 12.0f);
        bool cabinets = gallery || (bh & 1u) != 0u;
        /* The control room is set into the back of the gallery, so it goes
         * down before the deck and its handrail are laid across it. */
        if (gallery && (bh % 4u) == 3u && above >= 56.0f)
        {
            float room_top = stand - fminf(84.0f, above - 4.0f);
            plant_control_room(r, art, x + 22.0f, room_top, stand - 22.0f, bh);
            cabinets = false;
        }
        if (gallery)
            plant_walkway(r, art, x + 10.0f, 182.0f, deck);

        /* A row of motor control cabinets, each with its indicator pair.
         * Which lamps are lit belongs to the bay. */
        if (cabinets && above >= 40.0f)
        {
            for (int c = 0; c < 3; ++c)
            {
                unsigned ch = fx_hash(bh + (unsigned)c * 0x165667b1u);
                float cx = x + 30.0f + (float)c * 40.0f;
                float ch_h = fminf(58.0f + (float)(ch % 3u) * 6.0f,
                                   above - 6.0f);
                float cy = stand - ch_h;
                fx_rect(r, cab, cx, cy, 34.0f, ch_h);
                fx_rect(r, fx_mix(cab, art->wall_light, 0.35f), cx, cy, 34.0f,
                        1.0f);
                fx_rect(r, fx_mix(cab, FX_INK, 0.5f), cx + 16.0f, cy + 3.0f,
                        1.0f, ch_h - 5.0f);
                fx_rect_a(r, FX_INK, 70, cx + 30.0f, cy + 1.0f, 4.0f,
                          ch_h - 1.0f);
                for (int v = 0; v < 3; ++v)
                    fx_rect(r, fx_mix(cab, FX_INK, 0.45f), cx + 20.0f,
                            cy + ch_h - 16.0f + (float)v * 4.0f, 8.0f, 2.0f);
                SDL_Color run = (ch & 2u) ? FX_GREEN : FX_AMBER;
                fx_rect(r, fx_dim(run, (ch & 1u) ? 0.62f : 0.18f), cx + 5.0f,
                        cy + 8.0f, 2.0f, 2.0f);
                fx_rect(r, fx_dim(FX_RED, (ch & 4u) ? 0.5f : 0.16f), cx + 9.0f,
                        cy + 8.0f, 2.0f, 2.0f);
                fx_rect(r, fx_mix(cab, art->wall_light, 0.2f), cx + 4.0f,
                        cy + 16.0f, 8.0f, 5.0f);
                fx_rect_a(r, FX_INK, 70, cx - 1.0f, stand - 2.0f, 37.0f, 2.0f);
            }
        }

        /* The header the pumps feed, under the gallery or halfway up a single
         * storey, with its drops, their hand wheels and the one gauge on the
         * bay that somebody reads. */
        float header = gallery ? deck + 22.0f
                               : floor - fmaxf(92.0f, height * 0.42f);
        if (!gallery && cabinets)
            header = stand - fminf(64.0f, above - 6.0f) - 18.0f;
        plant_pipe(r, pipe, pipe_lit, x + 10.0f, header, 182.0f, 6.0f);
        plant_flange(r, pipe, pipe_lit, x + 104.0f, header, 6.0f);
        /* What stands on the floor of the bay: a pump set, the stair up to
         * the gallery, or an exchanger on its saddles; on a single storey,
         * nothing where the cabinets already are. */
        enum
        {
            PLANT_PUMP,
            PLANT_STAIR,
            PLANT_EXCHANGER,
            PLANT_BARE
        } kit;
        if (gallery)
            kit = (bh % 3u) == 0u   ? PLANT_PUMP
                  : (bh % 3u) == 1u ? PLANT_STAIR
                                    : PLANT_EXCHANGER;
        else
            kit = cabinets ? PLANT_BARE
                  : ((bh >> 4) & 1u) != 0u ? PLANT_EXCHANGER
                                           : PLANT_PUMP;
        bool pump = kit == PLANT_PUMP;
        if (kit == PLANT_PUMP || kit == PLANT_STAIR)
        {
            for (int drop = 0; drop < 3; ++drop)
            {
                float dx = x + 38.0f + (float)drop * 46.0f;
                float dlen =
                    26.0f + (float)((bh >> (unsigned)(drop * 3)) % 3u) * 10.0f;
                dlen = fminf(dlen, floor - header - 40.0f);
                if (dlen < 8.0f)
                    continue;
                fx_rect(r, pipe, dx, header + 6.0f, 4.0f, dlen);
                fx_rect(r, fx_mix(pipe, pipe_lit, 0.5f), dx, header + 6.0f,
                        1.0f, dlen);
                plant_valve(r, art, pipe, dx,
                            floorf(header + 6.0f + dlen * 0.5f));
            }
        }
        float gx = x + 62.0f;
        fx_rect(r, pipe, gx + 3.0f, header - 6.0f, 2.0f, 6.0f);
        fx_mass(r, fx_mix(art->near_shape, FX_INK, 0.4f), gx - 1.0f,
                header - 17.0f, 10.0f, 10.0f, 3, 3);
        fx_mass(r, fx_mix(art->haze, art->near_shape, 0.55f), gx,
                header - 16.0f, 8.0f, 8.0f, 2, 2);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), gx + 4.0f,
                header - 13.0f, 1.0f, 3.0f);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), gx + 2.0f,
                header - 13.0f, 2.0f, 1.0f);

        /* Bulkhead lamps under the gallery for the level below it. */
        if (gallery)
        {
            for (int lamp = 0; lamp < 2; ++lamp)
            {
                float lx = x + 54.0f + (float)lamp * 80.0f;
                SDL_Color lc = fx_dim(FX_SODIUM, 0.66f);
                fx_rect(r, fx_mix(pipe, FX_INK, 0.3f), lx - 3.0f, deck + 5.0f,
                        8.0f, 2.0f);
                fx_rect(r, lc, lx - 1.0f, deck + 7.0f, 4.0f, 2.0f);
                fx_light_cone(r, lx + 1.0f, deck + 9.0f, 3.0f, 16.0f,
                              fminf(60.0f, floor - deck - 20.0f), lc, 16);
            }
        }

        /* On the floor, a pump set: the motor on its plinth with its cooling
         * fins, the coupling, the volute, and the pipe it drives up to the
         * header. In a gallery bay without one, the stair up to the deck. */
        if (pump)
        {
            float my2 = floor - 26.0f;
            float px2 = x + 30.0f + (float)(bh % 24u);
            fx_rect(r, fx_mix(motor, FX_INK, 0.35f), px2 - 3.0f, floor - 4.0f,
                    96.0f, 4.0f);
            fx_mass(r, motor, px2, my2, 50.0f, 22.0f, 3, 3);
            for (float fin = px2 + 6.0f; fin < px2 + 44.0f; fin += 4.0f)
                fx_rect(r, fx_mix(motor, FX_INK, 0.35f), fin, my2 + 3.0f, 1.0f,
                        16.0f);
            fx_rect(r, fx_mix(motor, art->wall_light, 0.4f), px2 + 3.0f, my2,
                    44.0f, 1.0f);
            fx_rect(r, fx_mix(motor, FX_INK, 0.3f), px2 + 50.0f, my2 + 7.0f,
                    10.0f, 8.0f);
            fx_mass(r, motor, px2 + 60.0f, my2 - 4.0f, 28.0f, 26.0f, 6, 2);
            fx_rect(r, fx_mix(motor, art->wall_light, 0.4f), px2 + 65.0f,
                    my2 - 4.0f, 18.0f, 1.0f);
            fx_rect(r, fx_dim(art->accent, 0.55f), px2 + 70.0f, my2 + 6.0f,
                    6.0f, 2.0f);
            fx_rect_a(r, FX_INK, 90, px2 - 4.0f, floor - 2.0f, 98.0f, 2.0f);
            fx_rect(r, pipe, px2 + 72.0f, header + 6.0f, 5.0f,
                    my2 - header - 10.0f);
            fx_rect(r, fx_mix(pipe, pipe_lit, 0.5f), px2 + 72.0f,
                    header + 6.0f, 1.0f, my2 - header - 10.0f);
        }
        else if (kit == PLANT_EXCHANGER)
            plant_exchanger(r, art, pipe, x + 26.0f, 150.0f, floor, header);
        else if (kit == PLANT_STAIR)
        {
            /* A steel stair, floor to deck: stringer, treads, handrail. */
            SDL_Color steel = fx_mix(art->near_shape, art->wall, 0.3f);
            float rise = floor - deck;
            float run = fminf(150.0f, rise);
            float sx = x + 30.0f;
            fx_set(r, steel);
            for (int t = 0; t < 2; ++t)
            {
                float o = (float)t;
                SDL_RenderLine(r, sx, floor - 1.0f + o, sx + run,
                               deck + 4.0f + o);
            }
            fx_set(r, fx_mix(steel, art->wall_light, 0.2f));
            SDL_RenderLine(r, sx, floor - 20.0f, sx + run, deck - 16.0f);
            for (float k = 8.0f; k < rise - 4.0f; k += 8.0f)
            {
                float tx = sx + run * (k / rise);
                fx_rect(r, fx_mix(steel, art->wall_light, 0.32f), tx - 4.0f,
                        floor - k - 1.0f, 10.0f, 1.0f);
                fx_rect(r, fx_mix(steel, FX_INK, 0.3f), tx - 4.0f, floor - k,
                        10.0f, 1.0f);
            }
            for (float k = 24.0f; k < rise; k += 40.0f)
                fx_rect(r, fx_mix(steel, art->wall_light, 0.12f),
                        sx + run * (k / rise), floor - k - 19.0f, 1.0f, 19.0f);
        }
    }

    /* Slow volumetric light shafts falling from the roof lights between the
     * far bays. Each shaft's sway is its own, keyed to the shaft. */
    int shaft = 0;
    for (float x = art_room_first_lead(s, room, 0.22f, 384.0f, -96.0f, &shaft);
         x < room->clip_right + 80.0f; x += 384.0f, ++shaft)
    {
        float sway = sinf(s->time * 0.21f +
                          (float)(art_hash(shaft, 389) % 628u) * 0.01f) *
                     14.0f;
        fx_light_cone(r, x + 150.0f + sway, top - 6.0f, 14.0f, 52.0f,
                      height * 0.9f, FX_LAMP, 13);
    }

    /* Near layer: conduit dropping from the roof steel at a faster parallax,
     * and a junction box on every other run. */
    int seam = 0;
    float conduit = fminf(86.0f, height * 0.34f);
    for (float x = art_room_first_lead(s, room, 0.30f, 96.0f, 0.0f, &seam);
         x < room->clip_right + 8.0f; x += 96.0f, ++seam)
    {
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.2f), x + 66.0f, top + 7.0f,
                4.0f, conduit);
        fx_rect(r, fx_mix(art->wall, art->wall_light, 0.4f), x + 67.0f,
                top + 7.0f, 1.0f, conduit);
        if ((seam & 1) == 0 && conduit > 60.0f)
        {
            float jy = top + conduit - 26.0f;
            fx_rect(r, fx_mix(art->far_shape, art->wall, 0.4f), x + 62.0f, jy,
                    12.0f, 14.0f);
            fx_rect(r, fx_mix(art->wall, art->wall_light, 0.3f), x + 62.0f, jy,
                    12.0f, 1.0f);
            fx_rect_a(r, FX_INK, 90, x + 74.0f, jy + 1.0f, 2.0f, 14.0f);
        }
    }
}

/*
 * The way in off the street, drawn once.
 *
 * Everything else on this facade tiles, because a curtain wall genuinely
 * repeats along a building. An entrance does not — a lobby has one — so it is
 * anchored to a fixed point on the wall's own layer instead of being stamped
 * along it, and the anchor is a multiple of the mullion pitch so the jambs land
 * on the curtain wall's grid. The portal then takes over two bays of glass
 * rather than cutting across the middle of them.
 *
 * It also has to stay the dark half of the picture: the light belongs to the
 * canopy soffit above the doors, not to the street behind them, or the
 * entrance turns into a lamp standing at the back of the hall.
 */
static void lobby_entrance(const LevelArtScene *s, const LevelThemeArt *art,
                           float ex, float fascia, float base)
{
    SDL_Renderer *r = s->renderer;
    /* The mullions' own pitch: 192 is two of their bays. */
    const float span = 192.0f;
    float cx = ex + span * 0.5f;
    float head = fascia + 12.0f; /* door head; the gap is the canopy */
    float opening = base - head;
    SDL_Color jamb = fx_mix(art->near_shape, FX_INK, 0.55f);
    SDL_Color glass = fx_mix(art->far_shape, FX_INK, 0.35f);

    /* The reveal: jambs and head only. The leaves have to keep showing the
     * same city the rest of the wall shows, or the way out reads as the one
     * place with no outside behind it. */
    fx_rect(r, FX_INK, ex - 3.0f, fascia, 3.0f, base + 3.0f - fascia);
    fx_rect(r, FX_INK, ex + span, fascia, 3.0f, base + 3.0f - fascia);

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
     * a lobby keeps beside the drum for whatever will not fit through it. */
    fx_rect_a(r, glass, 130, ex + 8.0f, head, 50.0f, opening);
    fx_rect_a(r, glass, 130, ex + 134.0f, head, 50.0f, opening);
    fx_rect_a(r, art->trim_hi, 14, ex + 14.0f, head, 16.0f, opening);
    fx_rect_a(r, art->trim_hi, 14, ex + 140.0f, head, 16.0f, opening);
    fx_rect(r, jamb, ex + 134.0f, head, 3.0f, opening);
    fx_rect_a(r, jamb, 210, ex + 137.0f, head + opening * 0.52f, 47.0f, 3.0f);
    fx_rect_a(r, fx_mix(jamb, art->trim, 0.35f), 220, ex + 137.0f,
              base - 11.0f, 47.0f, 11.0f);
    fx_rect_a(r, art->trim, 200, ex + 172.0f, head + opening * 0.3f, 3.0f,
              opening * 0.34f);

    /* The revolving door. It turns on its own: an entrance frozen still reads
     * as a shut building. A wing seen face-on is a pane and edge-on is a line,
     * so the panel swept between the hub and the leading edge is what sells
     * the rotation. */
    float dl = ex + 58.0f;
    const float dw = 76.0f;
    float dcx = dl + dw * 0.5f;
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
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.2f), dcx - 1.0f, head, 2.0f,
            opening);
    /* The drum's canopy, stepped so the enclosure reads as a cylinder. */
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.35f), dl - 2.0f, head - 6.0f,
            dw + 4.0f, 6.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.15f), dl + 6.0f, head - 10.0f,
            dw - 12.0f, 5.0f);
    fx_rect_a(r, art->trim_hi, 130, dl + 6.0f, head - 10.0f, dw - 12.0f, 1.0f);

    /* Threshold: the one bright line at the foot of the opening. */
    fx_rect(r, FX_INK, ex, base, span, 4.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.2f), ex + 2.0f, base, span - 4.0f,
            3.0f);
    fx_rect_a(r, art->trim_hi, 170, ex + 2.0f, base, span - 4.0f, 1.0f);

    /* The canopy light last, over the doors it falls on. */
    fx_glow(r, cx, head, 50.0f, art->lamp, 40);
    fx_light_cone(r, cx, head, 80.0f, 98.0f, opening + 4.0f, art->lamp, 18);
}

/*
 * The first repeat of a world-keyed run that reaches `left`, and its index.
 *
 * `art_scroll` and `art_repeat` answer the same question for a layer that
 * starts at the left edge of the window; a room is drawn one piece at a time,
 * so the run starts at the piece instead and counts up from there. `phase`
 * slides the whole run along its own layer — a storey's seed does, so two
 * storeys of one theme do not stand their furniture in the same columns —
 * without the index ever coming from a screen position.
 */
typedef struct
{
    float x;
    int index;
} LobbyRun;

static LobbyRun lobby_run(float cam_x, float factor, float period, float phase,
                          float left)
{
    float t = cam_x * factor - phase;
    float first = floorf(t / period);
    float x0 = first * period - t;
    float k = floorf((left - x0) / period);
    LobbyRun run = {x0 + k * period, (int)first + (int)k};
    return run;
}

/* A per-storey offset along a run, off the room's seed. */
static float lobby_phase(const ArtRoom *room, unsigned salt, float period)
{
    return art_unit(fx_hash(room->seed ^ salt), 3) * period;
}

/*
 * Clip to one pane of glass inside the piece being drawn, keeping the piece's
 * own clip to put back afterwards. False when the pane is not in the piece.
 */
static bool lobby_clip_push(SDL_Renderer *r, float x, float y, float w,
                            float h, SDL_Rect *saved)
{
    SDL_GetRenderClipRect(r, saved);
    SDL_Rect pane = {(int)floorf(x), (int)floorf(y),
                     (int)ceilf(x + w) - (int)floorf(x),
                     (int)ceilf(y + h) - (int)floorf(y)};
    SDL_Rect both;
    if (saved->w <= 0 || !SDL_GetRectIntersection(saved, &pane, &both))
        return false;
    SDL_SetRenderClipRect(r, &both);
    return true;
}

/*
 * Where the horizon is on the glass: pinned to the screen, and sinking a
 * sliver as the camera climbs the building (`level_backdrop_sink`), because it
 * is a mile away and the slab the window is set in is not.
 */
static float lobby_horizon(const LevelArtScene *s, float share, float factor)
{
    return (float)s->win_h * share +
           level_backdrop_sink(&s->level->map, s->cam_y,
                               (float)s->win_h - (float)HUD_HEIGHT, factor);
}

/*
 * The city through a window, which is not on the wall.
 *
 * Everything else in the lobby, the offices and the penthouse is anchored to
 * the building and rides with the slab above it when the camera climbs. The
 * skyline is a mile off, and a mile off does not move because somebody went up
 * a ladder: it is laid out against the screen, round a horizon that sinks only
 * a sliver as the camera rises, and it drifts sideways at a small share of the
 * wall's rate. The window frame is on the wall and moves; that difference,
 * seen through every pane at once, is what makes a pane read as a hole in the
 * wall rather than a picture hung on it.
 *
 * `rise` is how far up the building the window is — nought at the street, one
 * on the executive floor. It decides how much of each tower stands above the
 * horizon, and whether there is a street grid lying below it to look down on.
 * Three themes look out on this one function because it is one city.
 *
 * The sky stays down near the interior air and only the lit windows are
 * bright: seen from a lit room at night the outside is the dark half of the
 * picture, and getting that the wrong way round turns a skyline into masonry
 * standing inside the room. What separates a tower from the sky is the city
 * glow gathered on the horizon and a rim on its own edges.
 */
static void lobby_city_view(const LevelArtScene *s, float x, float y, float w,
                            float h, float horizon, float rise)
{
    SDL_Renderer *r = s->renderer;
    SDL_Rect saved;
    if (w < 1.0f || h < 1.0f || !lobby_clip_push(r, x, y, w, h, &saved))
        return;

    const SDL_Color sky_high = {11, 15, 26, 255};
    const SDL_Color sky_low = {38, 42, 58, 255};
    const SDL_Color tower = {15, 19, 31, 255};
    SDL_Color ground = fx_mix(tower, FX_INK, 0.35f);
    const float x1 = x + w;
    const float y1 = y + h;

    fx_rect(r, sky_high, x, y, w, h);
    fx_vgrad(r, x, horizon - 240.0f, w, 240.0f, sky_high, 255, sky_low, 255);
    fx_vgrad(r, x, horizon - 44.0f, w, 44.0f, FX_SODIUM, 0, FX_SODIUM, 34);
    if (y1 > horizon)
    {
        fx_vgrad(r, x, horizon, w, 70.0f, fx_mix(sky_low, ground, 0.45f), 255,
                 ground, 255);
        if (y1 > horizon + 70.0f)
            fx_rect(r, ground, x, horizon + 70.0f, w, y1 - horizon - 70.0f);
    }

    /* Low cloud with the city's light on its belly, drifting. It is the one
     * thing in an empty pane of sky that says the sky is air and not a flat
     * fill, and it moves on the clock rather than with the camera. */
    const float cloud_span = 260.0f;
    float cloud_shift = s->steady_lights ? 0.0f : s->time * 3.0f;
    LobbyRun cloud = lobby_run(s->cam_x * 0.02f - cloud_shift, 1.0f, cloud_span,
                               0.0f, x - cloud_span);
    for (; cloud.x < x1; cloud.x += cloud_span, ++cloud.index)
    {
        unsigned ch = art_hash(cloud.index, 919 + s->level_index);
        float cy = horizon - 110.0f - (float)(ch % 150u);
        float cwid = 90.0f + (float)(ch >> 8 & 127u);
        float ch_h = 5.0f + (float)(ch >> 16 & 7u);
        if (cy > y1 || cy + ch_h < y)
            continue;
        art_mass_a(r, fx_mix(sky_low, FX_SODIUM, 0.35f), 34,
                   cloud.x + (float)(ch >> 20 & 63u), cy, cwid, ch_h, 3, 1);
        art_mass_a(r, fx_mix(sky_low, FX_SODIUM, 0.5f), 22,
                   cloud.x + (float)(ch >> 20 & 63u) + cwid * 0.2f, cy + ch_h - 2.0f,
                   cwid * 0.6f, 2.0f, 1, 0);
    }

    /* From high up the city is also something lying below the horizon: a
     * carpet of roofs and streets, each row further off than the one under
     * it — packed tight at the horizon, spaced wider and drifting faster the
     * nearer it is — with the lit windows of the low-rise blocks, the sodium
     * strings of the avenues, and the traffic moving along them. */
    if (rise > 0.6f && y1 > horizon)
    {
        SDL_Color roof = fx_mix(tower, sky_low, 0.3f);
        for (int row = 0; row < 44; ++row)
        {
            float fr = (float)row;
            float ly = horizon + 3.0f + fr * 3.0f + fr * fr * 0.14f;
            float next = horizon + 3.0f + (fr + 1.0f) * 3.0f +
                         (fr + 1.0f) * (fr + 1.0f) * 0.14f;
            if (ly > y1)
                break;
            if (next < y)
                continue;
            float band = next - ly;
            float gap = 4.0f + fr * 0.45f;
            float drift = 0.03f + 0.004f * fr;
            float dot = row < 14 ? 1.0f : 2.0f;
            bool avenue = (row % 5) == 3;
            LobbyRun run = lobby_run(s->cam_x, drift, gap, fr * 13.0f, x - gap);
            for (; run.x < x1; run.x += gap, ++run.index)
            {
                unsigned dh = art_hash(run.index, row * 31 + 5 + s->level_index);
                if (avenue)
                {
                    fx_rect_a(r, FX_SODIUM, (Uint8)(110u + dh % 60u), run.x,
                              ly + band * 0.5f, dot, 1.0f);
                    continue;
                }
                /* A roof, lit a little by the city on its parapet. */
                if ((dh & 3u) == 0u)
                    fx_rect(r, roof, run.x, ly, gap * 0.8f, band * 0.45f);
                if ((dh % 3u) == 0u)
                    continue;
                SDL_Color c = (dh & 16u) ? fx_mix(FX_WARM, FX_SODIUM, 0.4f)
                                         : ((dh & 32u) ? FX_LAMP : FX_WARM);
                fx_rect_a(r, c, (Uint8)(70u + dh % 70u + (unsigned)row * 2u),
                          run.x + (float)(dh >> 8 & 3u),
                          ly + band * 0.5f + (float)(dh >> 10 & 1u), dot,
                          row > 26 ? 2.0f : 1.0f);
            }
            /* Traffic on the avenues: headlamps one way, tail lamps the
             * other, on the clock rather than the camera. */
            if (avenue)
            {
                float span = w + 200.0f;
                for (int car = 0; car < 3; ++car)
                {
                    float speed = (14.0f + fr * 2.0f) * (car == 1 ? -1.0f : 1.0f);
                    float t = s->time * speed + (float)(car * 97 + row * 41) -
                              s->cam_x * drift;
                    float cx = x - 100.0f + (t - span * floorf(t / span));
                    SDL_Color lamp = speed > 0.0f ? fx_mix(FX_WARM, FX_CREAM, 0.6f)
                                                  : FX_RED;
                    fx_rect_a(r, lamp, 200, cx, ly + band * 0.5f - 1.0f, dot + 1.0f,
                              1.0f);
                }
            }
        }
    }

    /* The farthest towers: a low hazy band that gives the skyline a floor. */
    SDL_Color far_c = fx_mix(tower, sky_low, 0.45f);
    float far_lift = 76.0f - 52.0f * rise;
    LobbyRun far = lobby_run(s->cam_x, 0.035f, 58.0f, 0.0f, x - 58.0f);
    for (; far.x < x1; far.x += 58.0f, ++far.index)
    {
        unsigned th = art_hash(far.index, 7717 + s->level_index);
        float tw = 38.0f + (float)(th % 18u);
        float top = horizon - far_lift * (0.3f + 0.7f * art_unit(th, 8));
        /* From high up a tower stands on the carpet at its own distance
         * rather than running down out of the pane. */
        float foot = rise > 0.6f ? horizon + 4.0f : y1;
        if (top > y1 || foot <= top)
            continue;
        fx_rect(r, far_c, far.x, top, tw, foot - top);
        fx_rect(r, fx_mix(far_c, sky_low, 0.5f), far.x, top, tw, 1.0f);
        for (int lr = 0; lr < 6; ++lr)
        {
            unsigned lh = art_hash(far.index * 7 + lr, 41);
            if ((lh % 4u) != 0u)
                continue;
            fx_rect_a(r, FX_WARM, 60, far.x + 4.0f + (float)(lh >> 6 & 7u) * 4.0f,
                      top + 5.0f + (float)lr * 7.0f, 1.0f, 1.0f);
        }
    }

    /* Nearer towers, tall enough to break the horizon, with lit floors. */
    float lift_lo = 70.0f - 110.0f * rise;
    float lift_span = 190.0f - 100.0f * rise;
    LobbyRun near = lobby_run(s->cam_x, 0.075f, 164.0f, 0.0f, x - 164.0f);
    for (; near.x < x1; near.x += 164.0f, ++near.index)
    {
        unsigned h = art_hash(near.index, 2654 + s->level_index * 17);
        float tw = 84.0f + (float)(h % 40u);
        float tx = near.x + 18.0f * art_unit(h, 16);
        /* One tower in four is a landmark that stands clear of the rest
         * however high the window is — it is what an upper storey sees over
         * the roofs the street-level windows are full of. */
        float landmark = (h >> 24) % 4u == 0u ? 150.0f - 60.0f * rise : 0.0f;
        float top = horizon - (lift_lo + lift_span * art_unit(h, 8) + landmark);
        float foot = rise > 0.6f ? horizon + 8.0f + 36.0f * art_unit(h, 20) : y1;
        if (top > foot - 12.0f)
            top = foot - 12.0f;
        if (top > y1 || tx > x1 || tx + tw < x)
            continue;
        fx_rect(r, tower, tx, top, tw, foot - top);
        /* Seen from above, a roof is a lit deck rather than a cap line. */
        if (top > horizon - 24.0f)
            fx_rect(r, fx_mix(tower, sky_low, 0.5f), tx, top - 3.0f, tw, 3.0f);
        fx_rect(r, fx_mix(tower, sky_low, 0.7f), tx, top, tw, 1.0f);
        fx_rect(r, fx_mix(tower, sky_low, 0.45f), tx, top, 2.0f, foot - top);
        fx_rect_a(r, FX_INK, 90, tx + tw - 3.0f, top, 3.0f, foot - top);

        int row0 = (int)floorf((y - top - 10.0f) / 14.0f);
        if (row0 < 0)
            row0 = 0;
        for (int row = row0;; ++row)
        {
            float wy = top + 10.0f + (float)row * 14.0f;
            if (wy > y1 || wy + 7.0f > foot)
                break;
            for (int col = 0;; ++col)
            {
                float wx = tx + 8.0f + (float)col * 12.0f;
                if (wx > tx + tw - 10.0f)
                    break;
                if (wx + 5.0f < x || wx > x1)
                    continue;
                /* A window belongs to a tower, floor and bay — never to where
                 * the tower is on screen, or the whole skyline switches its
                 * lights as the camera moves. */
                unsigned wh = art_hash(near.index * 11 + col,
                                       row + s->level_index * 17);
                if ((wh % 9u) >= 3u)
                    continue;
                SDL_Color lit = (wh & 8u) ? fx_mix(FX_WARM, FX_CREAM, 0.3f)
                                          : FX_LAMP;
                /* One light in a great many has a brief fluorescent flutter,
                 * keyed to the same identity so movement can never set it
                 * off. */
                if (!s->steady_lights && ((wh >> 9) % 19u) == 0u)
                {
                    float period = 7.0f + (float)((wh >> 15) % 5u);
                    float phase = (float)((wh >> 20) & 255u) / 255.0f * period;
                    if (fmodf(s->time + phase, period) < 0.08f)
                        lit = fx_dim(lit, 0.35f);
                }
                fx_rect_a(r, lit, (Uint8)(70u + (wh >> 5) % 80u), wx, wy, 5.0f,
                          7.0f);
            }
        }
        /* One rooftop beacon in four towers, so the skyline is not a still. */
        if ((h & 3u) == 0u)
        {
            float pulse = art_pulse(
                s, sinf(s->time * 1.9f + (float)(h % 9u)) > 0.72f ? 1.0f : 0.2f,
                0.4f);
            fx_rect(r, fx_dim(FX_RED, 0.92f * pulse), tx + tw * 0.5f - 2.0f,
                    top - 4.0f, 4.0f, 4.0f);
        }
    }
    SDL_SetRenderClipRect(r, &saved);
}

/* A saloon on the carriageway outside. A car needs a body: a bare pair of
 * glows in the dark reads as lens flare, not as a vehicle passing the window.
 * The paint stays below the sky so a passing body reads by its lights. */
static void lobby_car(SDL_Renderer *r, float x, float base, bool leftward,
                      bool warm, float s)
{
    static const SDL_Color SALOON_WARM = {44, 40, 52, 255};
    static const SDL_Color SALOON_COOL = {36, 42, 50, 255};
    const SDL_Color sky_high = {11, 15, 26, 255};
    const SDL_Color sky_low = {38, 42, 58, 255};
    SDL_Color body = warm ? SALOON_WARM : SALOON_COOL;
    SDL_Color headlamp = fx_mix(FX_WARM, FX_CREAM, 0.70f);
    float y = base - 16.0f * s;
    fx_rect(r, FX_INK, x + 2.0f * s, y + 12.0f * s, 62.0f * s, 4.0f * s);
    fx_rect(r, body, x + 4.0f * s, y + 4.0f * s, 58.0f * s, 9.0f * s);
    fx_rect(r, fx_mix(body, sky_low, 0.5f), x + 4.0f * s, y + 4.0f * s,
            58.0f * s, 2.0f);
    fx_rect(r, fx_mix(body, sky_high, 0.7f), x + 20.0f * s, y, 26.0f * s,
            5.0f * s);
    /* A lit cabin: the glasshouse picks up the street, one pane of it. */
    fx_rect_a(r, FX_LAMP, 30, x + 22.0f * s, y + 1.0f, 10.0f * s, 3.0f * s);
    fx_rect(r, FX_INK, x + 12.0f * s, y + 13.0f * s, 8.0f * s, 3.0f * s);
    fx_rect(r, FX_INK, x + 46.0f * s, y + 13.0f * s, 8.0f * s, 3.0f * s);
    float lead = leftward ? x + 2.0f * s : x + 64.0f * s;
    float tail = leftward ? x + 64.0f * s : x + 2.0f * s;
    fx_glow(r, lead, y + 8.0f * s, 24.0f * s, headlamp, 96);
    fx_rect(r, fx_mix(FX_WARM, FX_CREAM, 0.90f), lead - 2.0f, y + 6.0f * s,
            4.0f, 3.0f);
    fx_glow(r, tail, y + 8.0f * s, 14.0f * s, FX_RED, 70);
    fx_rect(r, fx_dim(FX_RED, 0.8f), tail - 1.0f, y + 7.0f * s, 2.0f, 2.0f);
    /* Wet asphalt carries the lights back up at the kerb. */
    fx_rect_a(r, headlamp, 50, lead - 14.0f * s, base + 1.0f, 28.0f * s, 1.0f);
}

/*
 * The street outside the ground floor, seen through the glass: the pavement
 * level with the lobby floor, the kerb, a carriageway with traffic on it, the
 * far pavement, and the sodium lamps along it.
 *
 * It belongs to the room rather than to the far city — it is twenty metres
 * away and at the lobby's own floor level — so it stands on `floor`. The
 * towers behind it are on the screen, and reach down behind it whatever the
 * camera does, so the gap between the two can never open.
 */
static void lobby_street(const LevelArtScene *s, const LevelThemeArt *art,
                         const ArtRoom *room, float glass_bottom)
{
    SDL_Renderer *r = s->renderer;
    const SDL_Color sky_low = {38, 42, 58, 255};
    const float floor = room->floor;
    const float cl = room->clip_left;
    const float cw = room->clip_right - room->clip_left;
    const float far_kerb = floor - 36.0f;
    const float near_kerb = floor - 11.0f;

    /* The far side: the block opposite comes down to a lit shopfront line,
     * then its pavement. The shopfronts are what put a street at the foot of
     * the towers rather than a gap. */
    SDL_Color pave = fx_mix(sky_low, FX_SODIUM, 0.3f);
    fx_rect(r, fx_mix(sky_low, FX_INK, 0.35f), cl, far_kerb - 12.0f, cw, 8.0f);
    LobbyRun shop = lobby_run(s->cam_x, 0.2f, 38.0f, 0.0f, cl - 38.0f);
    for (; shop.x < room->clip_right; shop.x += 38.0f, ++shop.index)
    {
        unsigned sh = art_hash(shop.index, 97 + s->level_index);
        if ((sh % 3u) == 0u)
            continue;
        SDL_Color glow = (sh & 4u) ? FX_WARM : FX_LAMP;
        fx_rect_a(r, glow, (Uint8)(96u + sh % 70u), shop.x + 3.0f,
                  far_kerb - 11.0f, 26.0f, 6.0f);
        fx_rect_a(r, FX_INK, 120, shop.x + 3.0f, far_kerb - 12.0f, 26.0f, 1.0f);
        fx_rect_a(r, FX_INK, 90, shop.x + 15.0f, far_kerb - 11.0f, 1.0f, 6.0f);
        /* The spill of a lit shopfront across its own pavement. */
        fx_rect_a(r, glow, 40, shop.x, far_kerb - 4.0f, 32.0f, 4.0f);
    }
    fx_rect(r, pave, cl, far_kerb - 4.0f, cw, 4.0f);
    fx_rect_a(r, art->trim, 90, cl, far_kerb, cw, 1.0f);
    /* The carriageway, and the lane paint gone yellow under years of
     * sodium. */
    fx_vgrad(r, cl, far_kerb + 1.0f, cw, near_kerb - far_kerb - 1.0f,
             fx_mix(sky_low, FX_INK, 0.45f), 255, fx_mix(sky_low, FX_INK, 0.6f),
             255);
    LobbyRun mark = lobby_run(s->cam_x, 0.22f, 44.0f, 0.0f, cl - 44.0f);
    for (; mark.x < room->clip_right; mark.x += 44.0f)
        fx_rect_a(r, fx_mix(FX_CREAM, FX_AMBER_DK, 0.35f), 90, mark.x,
                  (far_kerb + near_kerb) * 0.5f, 16.0f, 1.0f);

    /* Street lamps on the far pavement, each throwing its own pool. */
    LobbyRun lamp = lobby_run(s->cam_x, 0.22f, 212.0f, 60.0f, cl - 212.0f);
    for (; lamp.x < room->clip_right + 40.0f; lamp.x += 212.0f)
    {
        float px = lamp.x + 30.0f;
        float head = far_kerb - 74.0f;
        fx_rect(r, fx_mix(sky_low, FX_INK, 0.55f), px, head, 2.0f,
                far_kerb - head);
        fx_rect(r, fx_mix(sky_low, FX_INK, 0.55f), px - 8.0f, head, 10.0f, 2.0f);
        fx_rect(r, fx_mix(FX_SODIUM, FX_CREAM, 0.35f), px - 8.0f, head + 2.0f,
                5.0f, 1.0f);
        fx_glow(r, px - 6.0f, head + 3.0f, 20.0f, FX_SODIUM, 50);
        fx_light_cone(r, px - 6.0f, head + 3.0f, 2.0f, 22.0f, far_kerb - head,
                      FX_SODIUM, 18);
        fx_rect_a(r, FX_SODIUM, 70, px - 26.0f, far_kerb - 4.0f, 40.0f, 4.0f);
    }

    /* Traffic: two lanes, the far one smaller and further up the glass. */
    float span = (float)s->win_w + 240.0f;
    for (int car = 0; car < 4; ++car)
    {
        bool leftward = (car & 1) != 0;
        float speed = 52.0f + (float)car * 19.0f;
        float drift = s->cam_x * 0.22f;
        float t = s->time * speed + (float)car * 297.0f;
        float lx = leftward ? -t - drift : t - drift;
        lx = lx - span * floorf(lx / span) - 120.0f;
        if (lx > room->clip_right || lx + 70.0f < cl)
            continue;
        if (leftward)
            lobby_car(r, lx, floor - 24.0f, true, car == 1, 0.72f);
        else
            lobby_car(r, lx, near_kerb - 1.0f, false, car == 2, 1.0f);
    }

    /* The near kerb and the pavement at the foot of the glass. */
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.35f), cl, near_kerb, cw, 2.0f);
    fx_rect_a(r, art->trim_hi, 70, cl, near_kerb, cw, 1.0f);
    fx_rect(r, pave, cl, near_kerb + 2.0f, cw, glass_bottom - near_kerb - 2.0f);
}

/* The hall's pendant luminaires, hanging in the volume in front of the glass,
 * and the light each puts on the floor. Nearer than the wall, so they drift
 * a little faster than it. */
static void lobby_pendants(const LevelArtScene *s, const LevelThemeArt *art,
                           const ArtRoom *room, float soffit)
{
    SDL_Renderer *r = s->renderer;
    float reach = fminf(40.0f, (room->floor - soffit) * 0.22f);
    LobbyRun p = lobby_run(s->cam_x, 0.6f, 192.0f, 96.0f, room->clip_left - 120.0f);
    for (; p.x < room->clip_right + 120.0f; p.x += 192.0f, ++p.index)
    {
        unsigned h = art_hash(p.index, 40503);
        float drop = reach * (0.55f + 0.45f * art_unit(h, 4));
        float cx = p.x + 96.0f;
        float cy = soffit + drop;
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.3f), cx - 1.0f, soffit, 1.0f,
                drop);
        fx_rect(r, FX_INK, cx - 11.0f, cy, 22.0f, 4.0f);
        fx_rect(r, art->trim, cx - 10.0f, cy, 20.0f, 2.0f);
        fx_rect(r, art->lamp, cx - 8.0f, cy + 2.0f, 16.0f, 2.0f);
        fx_glow(r, cx, cy + 3.0f, 30.0f, art->lamp, 46);
        fx_light_cone(r, cx, cy + 4.0f, 14.0f, 70.0f, room->floor - cy - 6.0f,
                      art->lamp, 13);
    }
}

/* Banners hung in the volume of a hall tall enough to take them: a hall this
 * tall needs something at mid height, or the space between the ceiling and
 * the floor is just distance. Deep bronze cloth: lit up to the value of the
 * brass it hangs from, it would become the brightest thing in the hall. */
static void lobby_banners(const LevelArtScene *s, const LevelThemeArt *art,
                          const ArtRoom *room, float soffit)
{
    SDL_Renderer *r = s->renderer;
    float room_h = room->floor - soffit;
    float length = fminf(170.0f, room_h * 0.46f);
    LobbyRun b = lobby_run(s->cam_x, 0.55f, 288.0f, 0.0f, room->clip_left - 160.0f);
    for (; b.x < room->clip_right + 160.0f; b.x += 288.0f, ++b.index)
    {
        unsigned h = art_hash(b.index, 1103);
        float bx = b.x + 118.0f;
        float by = soffit + 8.0f;
        float bh = length * (0.8f + 0.2f * art_unit(h, 6));
        /* A slow breathing sway, as if the air handling were running. */
        float lean = sinf(s->time * 0.5f + (float)(h % 7u)) * 2.0f;
        float sx = bx + lean * 0.3f;
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.45f), bx - 12.0f, by, 26.0f, 3.0f);
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.5f), bx - 1.0f, soffit, 1.0f,
                8.0f);
        fx_rect(r, FX_INK, sx - 10.0f, by + 3.0f, 21.0f, bh);
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
}

/*
 * The street front: the ground floor's glazed curtain wall, with the street
 * and the city behind it.
 *
 * A hall tall enough gets the whole composition — a coffered head, a transom
 * at door height, the entrance, banners and pendants in the volume. A low one
 * (the part of the ground floor under a gallery) drops everything that needs
 * height and keeps the glass: a flat soffit with downlights, the same mullions
 * on the same grid, so the bay lines carry straight on above and below the
 * gallery slab, and the street at its foot.
 *
 * Only architecture repeats. A reception desk tiled every few hundred pixels
 * reads as a mistake; the furniture lives in the map as props instead.
 */
static void lobby_street_front(const LevelArtScene *s, const LevelThemeArt *art,
                               const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float wall = 0.45f; /* the curtain wall's own drift */
    const float cl = room->clip_left;
    const float cr = room->clip_right;
    const float cw = cr - cl;
    const float floor = room->floor;
    const bool hall = room->height >= 160.0f;
    const float soffit = room->top + (hall ? 26.0f : 9.0f);
    const float glass_bottom = floor - 4.0f;
    const float transom = floor - 94.0f;

    lobby_city_view(s, cl, soffit, cw, glass_bottom - soffit,
                    lobby_horizon(s, 0.83f, 0.08f), 0.0f);
    lobby_street(s, art, room, glass_bottom);

    /* One cool veil over the whole opening. Without something between the
     * room and the view, a lit window two streets away sits at exactly the
     * same depth as a lamp on the wall behind Chuck. Bluer than any lamp in
     * the palette on purpose: this is night sky filtered through glass. */
    const SDL_Color veil = {96, 126, 158, 255};
    fx_rect_a(r, veil, 28, cl, soffit, cw, glass_bottom - soffit);

    /* The curtain wall. The mullions stay dark: a bright vertical line the
     * height of the hall reads as a pole standing in the room. */
    LobbyRun m = lobby_run(s->cam_x, wall, 96.0f, 0.0f, cl - 96.0f);
    for (; m.x < cr + 8.0f; m.x += 96.0f)
    {
        /* A raking sheen across each bay: flat fill alone never reads as
         * glass. */
        fx_rect_a(r, art->trim_hi, 11, m.x + 16.0f, soffit, 30.0f,
                  glass_bottom - soffit);
        fx_rect_a(r, art->trim_hi, 7, m.x + 58.0f, soffit, 14.0f,
                  glass_bottom - soffit);
        fx_rect(r, FX_INK, m.x, soffit, 8.0f, glass_bottom - soffit);
        fx_rect(r, art->near_shape, m.x + 1.0f, soffit, 6.0f,
                glass_bottom - soffit);
        /* The brass catches the light only at the head and the sill. */
        fx_rect_a(r, art->trim, 120, m.x + 1.0f, soffit, 6.0f, 3.0f);
        fx_rect_a(r, art->trim, 90, m.x + 1.0f, glass_bottom - 5.0f, 6.0f, 3.0f);
    }
    if (hall && transom > soffit + 20.0f)
    {
        fx_rect_a(r, FX_INK, 190, cl, transom, cw, 8.0f);
        fx_rect_a(r, art->near_shape, 210, cl, transom + 1.0f, cw, 6.0f);
        fx_rect_a(r, fx_mix(art->near_shape, art->trim, 0.5f), 150, cl,
                  transom + 1.0f, cw, 1.0f);
    }
    /* The base channel the glass sits in. */
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.25f), cl, glass_bottom, cw, 4.0f);
    fx_rect_a(r, art->trim_hi, 170, cl, glass_bottom, cw, 1.0f);

    if (hall)
    {
        /* The entrance, at a fixed point on the wall's own layer, set into
         * the glazing that has just been drawn. */
        float ex = 96.0f - s->cam_x * wall;
        if (ex < cr && ex + 196.0f > cl)
            lobby_entrance(s, art, ex, transom + 1.0f, glass_bottom - 1.0f);

        /* The head of the curtain wall: a stone fascia the glass is hung
         * from, jointed on the mullion grid, with a brass nosing and a cove
         * light tucked behind it washing down the top of the glass. It is
         * what stops the top of the hall being unexplained air — ribs drawn
         * in elevation read as a railing, not as a ceiling. */
        float depth = soffit - room->top;
        fx_vgrad(r, cl, room->top, cw, depth,
                 fx_mix(art->wall_dark, art->wall, 0.35f), 255,
                 fx_mix(art->wall_dark, art->wall, 0.6f), 255);
        fx_rect_a(r, FX_INK, 90, cl, room->top, cw, 4.0f);
        LobbyRun c = lobby_run(s->cam_x, wall, 96.0f, 0.0f, cl - 96.0f);
        for (; c.x < cr; c.x += 96.0f, ++c.index)
        {
            fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.3f), c.x + 3.0f,
                    room->top + 4.0f, 1.0f, depth - 10.0f);
            fx_rect_a(r, art->wall_light, 40, c.x + 4.0f, room->top + 4.0f,
                      1.0f, depth - 10.0f);
            unsigned vh = art_hash(c.index, 311);
            fx_rect_a(r, art->wall_light, 26, c.x + 14.0f + (float)(vh % 40u),
                      room->top + 6.0f + (float)(vh >> 8 & 7u), 26.0f, 1.0f);
        }
        fx_rect(r, art->trim, cl, soffit - 6.0f, cw, 4.0f);
        fx_rect(r, art->trim_hi, cl, soffit - 6.0f, cw, 1.0f);
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.5f), cl, soffit - 2.0f, cw, 2.0f);
        fx_rect_a(r, art->lamp, 150, cl, soffit, cw, 1.0f);
        fx_vgrad(r, cl, soffit, cw, 26.0f, art->lamp, 34, art->lamp, 0);

        if (room->height >= 192.0f)
            lobby_banners(s, art, room, soffit);
        lobby_pendants(s, art, room, soffit);
    }
    else
    {
        /* Under a gallery: the slab's own soffit, a brass edge, and a
         * recessed downlight to each bay, small and dim because the real
         * lamps are the playfield's. */
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.2f), cl, room->top, cw,
                soffit - room->top);
        fx_rect(r, art->trim, cl, soffit - 2.0f, cw, 2.0f);
        fx_rect_a(r, art->trim_hi, 150, cl, soffit - 2.0f, cw, 1.0f);
        fx_rect_a(r, FX_INK, 80, cl, soffit, cw, 3.0f);
        LobbyRun d = lobby_run(s->cam_x, wall, 96.0f, 48.0f, cl - 96.0f);
        for (; d.x < cr + 20.0f; d.x += 96.0f)
        {
            fx_rect(r, FX_INK, d.x - 4.0f, soffit - 1.0f, 8.0f, 2.0f);
            fx_rect(r, art->lamp, d.x - 3.0f, soffit, 6.0f, 1.0f);
            fx_glow(r, d.x, soffit + 1.0f, 16.0f, art->lamp, 34);
            fx_light_cone(r, d.x, soffit + 1.0f, 3.0f, 26.0f,
                          room->floor - soffit - 4.0f, art->lamp, 12);
        }
    }
}

/*
 * An upper floor of the lobby: stone-clad, with deep window openings onto the
 * same city a storey or two up, and a wall-washer between each pair.
 *
 * The stone is set a step darker than the marble the slabs are cut from, so
 * the back wall of a room never reads as more of the masonry in front of it.
 */
static void lobby_upper_floor(const LevelArtScene *s, const LevelThemeArt *art,
                              const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float wall = 0.45f;
    const float cl = room->clip_left;
    const float cr = room->clip_right;
    const float cw = cr - cl;
    const float top = room->top;
    const float floor = room->floor;
    const float high = room->height;
    SDL_Color stone = fx_mix(art->wall_dark, art->wall, 0.38f);
    SDL_Color joint = fx_mix(art->wall_dark, FX_INK, 0.35f);

    fx_rect(r, stone, cl, top, cw, high);
    fx_vgrad(r, cl, top, cw, high, FX_INK, 60, FX_INK, 0);

    /* Slab joints: the cladding is hung in panels a storey-half tall. */
    LobbyRun slab = lobby_run(s->cam_x, wall, 64.0f, 32.0f, cl - 64.0f);
    for (; slab.x < cr; slab.x += 64.0f, ++slab.index)
    {
        fx_rect(r, joint, slab.x, top, 1.0f, high);
        fx_rect_a(r, art->wall_light, 22, slab.x + 1.0f, top, 1.0f, high);
        unsigned vh = art_hash(slab.index, room->top_row * 7 + 3);
        /* A vein or two, faint: this is the room's wall, not the marble. */
        fx_rect_a(r, art->wall_light, 16, slab.x + 10.0f + (float)(vh % 30u),
                  top + high * art_unit(vh, 8), 22.0f, 1.0f);
        fx_rect_a(r, art->wall_light, 12, slab.x + 30.0f + (float)(vh >> 12 & 15u),
                  top + high * art_unit(vh, 16), 14.0f, 1.0f);
    }
    float mid = top + high * 0.5f;
    fx_rect(r, joint, cl, mid, cw, 1.0f);
    fx_rect_a(r, art->wall_light, 20, cl, mid + 1.0f, cw, 1.0f);

    /* Windows: one to every two slabs, set deep, the city a storey up behind
     * them. */
    const float wy = top + fminf(16.0f, high * 0.16f);
    const float wb = floor - fminf(22.0f, high * 0.22f);
    const float horizon = lobby_horizon(s, 0.83f, 0.08f);
    LobbyRun win = lobby_run(s->cam_x, wall, 128.0f, 12.0f, cl - 128.0f);
    for (; win.x < cr + 20.0f; win.x += 128.0f, ++win.index)
    {
        float wx = win.x + 20.0f;
        const float ww = 44.0f;
        fx_rect(r, FX_INK, wx - 4.0f, wy - 4.0f, ww + 8.0f, wb - wy + 8.0f);
        lobby_city_view(s, wx, wy, ww, wb - wy, horizon, 0.0f);
        const SDL_Color veil = {96, 126, 158, 255};
        fx_rect_a(r, veil, 26, wx, wy, ww, wb - wy);
        fx_rect_a(r, art->trim_hi, 12, wx + 6.0f, wy, 10.0f, wb - wy);
        /* The reveal: the head throws its shadow into the opening, and the
         * brass frame catches the light along its top. */
        fx_rect_a(r, FX_INK, 90, wx, wy, ww, 4.0f);
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.35f), wx + ww * 0.5f - 1.0f, wy,
                2.0f, wb - wy);
        fx_rect(r, art->trim, wx - 4.0f, wy - 4.0f, ww + 8.0f, 2.0f);
        fx_rect_a(r, art->trim_hi, 140, wx - 4.0f, wy - 4.0f, ww + 8.0f, 1.0f);
        /* The sill, lit on top. */
        fx_rect(r, fx_mix(stone, art->wall_light, 0.3f), wx - 7.0f, wb + 3.0f,
                ww + 14.0f, 3.0f);
        fx_rect(r, art->trim_hi, wx - 7.0f, wb + 3.0f, ww + 14.0f, 1.0f);
        fx_rect_a(r, FX_INK, 70, wx - 6.0f, wb + 6.0f, ww + 12.0f, 3.0f);

        /* A brass wall-washer on the pier, lighting the stone up and down. */
        float lx = win.x + 96.0f;
        float ly = top + high * 0.42f;
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.3f), lx - 4.0f, ly, 8.0f, 6.0f);
        fx_rect(r, art->trim, lx - 3.0f, ly, 6.0f, 2.0f);
        fx_rect(r, art->lamp, lx - 2.0f, ly - 1.0f, 4.0f, 1.0f);
        fx_rect(r, fx_dim(art->lamp, 0.8f), lx - 2.0f, ly + 6.0f, 4.0f, 1.0f);
        fx_light_cone(r, lx, ly - 1.0f, 3.0f, 14.0f, -(ly - top - 2.0f),
                      art->lamp, 26);
        fx_light_cone(r, lx, ly + 7.0f, 3.0f, 16.0f, floor - ly - 10.0f,
                      art->lamp, 22);
        fx_glow(r, lx, ly + 3.0f, 14.0f, art->lamp, 30);
    }

    /* Skirting in the brass the whole ground floor is trimmed in. */
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.45f), cl, floor - 5.0f, cw, 5.0f);
    fx_rect_a(r, art->trim_hi, 90, cl, floor - 5.0f, cw, 1.0f);
}

/* One room of the LOBBY theme: the ground floor is the street front, and
 * anything above it is an upper floor looking out over the same street. */
static void backdrop_lobby_room(const LevelArtScene *s, const LevelThemeArt *art,
                                const ArtRoom *room)
{
    if (room->floor_row >= s->level->map.height - 2)
        lobby_street_front(s, art, room);
    else
        lobby_upper_floor(s, art, room);
}


/* ---- OFFICE ----------------------------------------------------------- */

/*
 * The block across the street, between the office's windows and the skyline.
 *
 * An office a few floors up does not look out over the city; it looks at the
 * building opposite, near enough that its floors are rows you could count and
 * its lit windows are somebody else's late night. It is neither on the wall
 * nor at infinity, so it moves at neither rate: it slides past the glass at a
 * share of the wall's drift and rides a little over half the camera's climb,
 * and the skyline behind it — through the gaps between blocks and over the
 * lower roofs — barely moves at all. Three depths through one pane of glass.
 */
static void office_facing_blocks(const LevelArtScene *s, float x, float y,
                                 float w, float h)
{
    SDL_Renderer *r = s->renderer;
    SDL_Rect saved;
    if (w < 1.0f || h < 1.0f || !lobby_clip_push(r, x, y, w, h, &saved))
        return;
    const SDL_Color tower = {15, 19, 31, 255};
    const SDL_Color sky_low = {38, 42, 58, 255};
    SDL_Color face = fx_mix(tower, sky_low, 0.3f);
    SDL_Color glass = fx_mix(tower, sky_low, 0.55f);
    const float period = 300.0f;
    const float pitch_y = 18.0f;
    const float pitch_x = 16.0f;
    /* The street the block stands on, in the screen: below the frame, and
     * climbing out of it a little over half as fast as the camera does. */
    float street = (float)s->win_h + 60.0f - s->cam_y * 0.55f;
    LobbyRun blk = lobby_run(s->cam_x, 0.2f, period, 0.0f, x - period);
    for (; blk.x < x + w; blk.x += period, ++blk.index)
    {
        unsigned bh = art_hash(blk.index, 5501 + s->level_index);
        float bw = 196.0f + (float)(bh % 72u);
        float bx = blk.x + (float)((bh >> 8) % 24u);
        int floors = 16 + (int)((bh >> 12) % 22u);
        float top = street - (float)floors * pitch_y - 6.0f;
        if (bx > x + w || bx + bw < x || top > y + h)
            continue;
        fx_rect(r, face, bx, top, bw, street - top);
        fx_rect(r, fx_mix(face, sky_low, 0.6f), bx, top, bw, 1.0f);
        fx_rect(r, fx_mix(face, sky_low, 0.4f), bx, top, 2.0f, street - top);
        fx_rect_a(r, FX_INK, 100, bx + bw - 4.0f, top, 4.0f, street - top);
        /* A parapet and the plant on its roof. */
        fx_rect(r, fx_mix(face, FX_INK, 0.3f), bx + bw * 0.2f, top - 6.0f,
                bw * 0.18f, 6.0f);

        int row0 = (int)floorf((street - (y + h)) / pitch_y) - 1;
        if (row0 < 0)
            row0 = 0;
        for (int row = row0; row < floors; ++row)
        {
            float fy = street - (float)(row + 1) * pitch_y;
            if (fy + pitch_y < y)
                break;
            /* The slab edge between floors. */
            fx_rect_a(r, FX_INK, 70, bx, fy + pitch_y - 3.0f, bw - 4.0f, 3.0f);
            for (int col = 0;; ++col)
            {
                float wx = bx + 6.0f + (float)col * pitch_x;
                if (wx + 10.0f > bx + bw - 6.0f)
                    break;
                if (wx + 10.0f < x || wx > x + w)
                    continue;
                unsigned wh = art_hash(blk.index * 41 + col, row * 13 + 7);
                if ((wh % 5u) != 0u)
                {
                    /* Unlit: the glass holds a little of the night sky. */
                    fx_rect_a(r, glass, 90, wx, fy + 2.0f, 10.0f, 11.0f);
                    continue;
                }
                SDL_Color lit = (wh & 32u) ? FX_LAMP : fx_mix(FX_WARM, FX_CREAM, 0.3f);
                fx_rect_a(r, lit, (Uint8)(60u + (wh >> 6) % 60u), wx, fy + 2.0f,
                          10.0f, 11.0f);
                /* The ceiling light inside, brighter at the head. */
                fx_rect_a(r, lit, 110, wx, fy + 2.0f, 10.0f, 1.0f);
                /* Somebody's blind, half down, in one window of a few. */
                if ((wh >> 10) % 3u == 0u)
                    fx_rect_a(r, FX_INK, 80, wx, fy + 2.0f, 10.0f,
                              3.0f + (float)((wh >> 12) % 5u));
            }
        }
    }
    SDL_SetRenderClipRect(r, &saved);
}

/*
 * A bay of the window wall that is not a window: the core of the floor shows
 * through at intervals, painted, and carries what an open-plan office hangs on
 * the one piece of wall it has — a whiteboard, a print, a notice board, a
 * clock over a thermostat.
 */
static void office_wall_bay(const LevelArtScene *s, const LevelThemeArt *art,
                            float x, float w, float head, float sill,
                            unsigned h)
{
    SDL_Renderer *r = s->renderer;
    SDL_Color paint = fx_mix(art->wall, art->far_shape, 0.42f);
    float band = sill - head;
    fx_rect(r, paint, x, head, w, band);
    fx_vgrad(r, x, head, w, band, art->wall_light, 14, art->wall_light, 0);
    fx_rect_a(r, FX_INK, 70, x, head, 2.0f, band);
    fx_rect_a(r, FX_INK, 50, x + w - 2.0f, head, 2.0f, band);

    float cx = x + w * 0.5f;
    float item_h = fminf(34.0f, band - 8.0f);
    if (item_h < 10.0f)
        return;
    float iy = head + (band - item_h) * 0.45f;
    switch (h % 4u)
    {
    case 0:
    {
        /* A whiteboard, with the last meeting still on it. */
        float bw = fminf(76.0f, w - 20.0f);
        fx_rect_a(r, FX_INK, 80, cx - bw * 0.5f + 2.0f, iy + 2.0f, bw, item_h);
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.2f), cx - bw * 0.5f, iy, bw,
                item_h);
        /* In the dark it is the palest thing on the wall and still only
         * half-lit: a board glowing white would read as a light box. */
        fx_rect(r, fx_mix(art->wall, art->trim_hi, 0.42f),
                cx - bw * 0.5f + 1.0f, iy + 1.0f, bw - 2.0f, item_h - 3.0f);
        fx_vgrad(r, cx - bw * 0.5f + 1.0f, iy + 1.0f, bw - 2.0f, item_h - 3.0f,
                 art->lamp, 26, FX_INK, 40);
        for (int line = 0; line < 4; ++line)
        {
            unsigned lh = fx_hash(h + (unsigned)line * 97u);
            float ly = iy + 4.0f + (float)line * (item_h - 9.0f) / 3.0f;
            SDL_Color ink = (lh & 3u) == 0u ? art->accent
                                            : fx_mix(art->wall_dark, FX_INK, 0.2f);
            fx_rect_a(r, ink, 150, cx - bw * 0.5f + 5.0f + (float)(lh % 6u), ly,
                      bw * (0.3f + art_unit(lh, 8) * 0.4f), 1.0f);
        }
        /* The pen tray along the bottom. */
        fx_rect(r, fx_mix(art->trim, FX_INK, 0.35f), cx - bw * 0.3f,
                iy + item_h - 1.0f, bw * 0.6f, 2.0f);
        break;
    }
    case 1:
    {
        /* A framed print: somebody's idea of art for a meeting room. */
        float pw = fminf(40.0f, w - 30.0f);
        fx_rect_a(r, FX_INK, 80, cx - pw * 0.5f + 2.0f, iy + 2.0f, pw, item_h);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), cx - pw * 0.5f, iy,
                pw, item_h);
        fx_rect(r, fx_mix(art->trim_hi, art->wall, 0.6f), cx - pw * 0.5f + 2.0f,
                iy + 2.0f, pw - 4.0f, item_h - 4.0f);
        fx_rect(r, fx_mix(art->accent, art->wall, 0.35f), cx - pw * 0.3f,
                iy + item_h * 0.3f, pw * 0.34f, item_h * 0.42f);
        fx_rect(r, fx_mix(FX_AMBER_DK, art->wall, 0.4f), cx - pw * 0.02f,
                iy + item_h * 0.18f, pw * 0.24f, item_h * 0.22f);
        fx_rect(r, fx_mix(art->wall_dark, art->wall, 0.3f), cx - pw * 0.34f,
                iy + item_h * 0.76f, pw * 0.68f, 1.0f);
        break;
    }
    case 2:
    {
        /* A notice board: cork, and the notices nobody has read. */
        float bw = fminf(56.0f, w - 24.0f);
        SDL_Color cork = fx_mix(FX_WOOD, art->far_shape, 0.55f);
        fx_rect_a(r, FX_INK, 80, cx - bw * 0.5f + 2.0f, iy + 2.0f, bw, item_h);
        fx_rect(r, fx_mix(art->near_shape, art->trim, 0.3f), cx - bw * 0.5f, iy,
                bw, item_h);
        fx_rect(r, cork, cx - bw * 0.5f + 2.0f, iy + 2.0f, bw - 4.0f,
                item_h - 4.0f);
        for (int sheet = 0; sheet < 5; ++sheet)
        {
            unsigned sh = fx_hash(h * 31u + (unsigned)sheet);
            float sw = 7.0f + (float)(sh % 5u);
            float sx = cx - bw * 0.5f + 4.0f + art_unit(sh, 8) * (bw - sw - 8.0f);
            float sy = iy + 4.0f + art_unit(sh, 16) * (item_h - 16.0f);
            fx_rect(r, fx_mix(FX_CREAM, art->wall, 0.45f), sx, sy, sw, 9.0f);
            fx_rect(r, (sh & 1u) ? FX_RED_DK : art->accent, sx + sw * 0.5f,
                    sy, 1.0f, 1.0f);
        }
        break;
    }
    default:
    {
        /* A clock over the thermostat. */
        SDL_Color rim = fx_mix(art->near_shape, FX_INK, 0.3f);
        fx_mass(r, rim, cx - 8.0f, iy, 16.0f, 16.0f, 4, 4);
        fx_mass(r, fx_mix(FX_CREAM, art->wall, 0.3f), cx - 7.0f, iy + 1.0f,
                14.0f, 14.0f, 4, 4);
        fx_rect(r, FX_INK, cx - 0.5f, iy + 3.0f, 1.0f, 6.0f);
        fx_rect(r, FX_INK, cx, iy + 8.0f, 4.0f, 1.0f);
        if (band > 36.0f)
        {
            fx_rect(r, fx_mix(art->trim, art->wall, 0.3f), cx - 3.0f,
                    iy + item_h - 7.0f, 6.0f, 8.0f);
            fx_rect(r, fx_dim(FX_GREEN, 0.6f), cx - 1.0f, iy + item_h - 5.0f,
                    2.0f, 1.0f);
        }
        break;
    }
    }
}

/*
 * One cluster of desks in front of the window wall, seen from the aisle: the
 * back screen, the worktop, whatever was left logged in, and the chairs. Sized
 * to the man and not to the room — a partition in a tall storey is exactly as
 * tall as one in a low one — which is what lets the same furniture stand in
 * every storey without looking like a model of itself.
 */
static void office_cubicles(const LevelArtScene *s, const LevelThemeArt *art,
                            float x, float floor, unsigned cluster)
{
    SDL_Renderer *r = s->renderer;
    const float panel = 26.0f;
    const float width = 144.0f;
    float top = floor - panel;
    float desk = floor - 13.0f;
    SDL_Color screen = art->near_shape;
    SDL_Color frame = fx_mix(art->near_shape, FX_INK, 0.4f);

    fx_rect(r, screen, x, top, width, panel);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.35f), x, top, width,
            1.0f);
    fx_vgrad(r, x, top + 1.0f, width, 14.0f, art->wall_light, 14,
             art->wall_light, 0);
    /* The weave of the fabric runs down it. */
    for (float wx = x + 3.0f; wx < x + width; wx += 4.0f)
        fx_rect_a(r, FX_INK, 24, wx, top + 2.0f, 1.0f, desk - top - 2.0f);
    for (int divider = 0; divider < 4; ++divider)
    {
        float dx = x + (float)divider * 48.0f - (divider == 3 ? 3.0f : 0.0f);
        fx_rect(r, frame, dx, top - 1.0f, 3.0f, panel + 1.0f);
        fx_rect(r, fx_mix(frame, art->wall_light, 0.4f), dx, top - 1.0f, 3.0f,
                1.0f);
    }
    /* The worktop, its lit edge, and the shadow it keeps under it. */
    fx_rect(r, fx_mix(art->near_shape, art->wall, 0.4f), x + 3.0f, desk,
            width - 6.0f, 2.0f);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.4f), x + 3.0f, desk,
            width - 6.0f, 1.0f);
    fx_rect_a(r, FX_INK, 90, x + 3.0f, desk + 2.0f, width - 6.0f, 11.0f);

    for (int d = 0; d < 3; ++d)
    {
        unsigned dh = fx_hash(cluster * 3u + (unsigned)d + 977u);
        float dx = x + (float)d * 48.0f;
        /* Paper, a mug, the in-tray: the worktop is somebody's. */
        if (dh & 64u)
            fx_rect(r, fx_mix(FX_CREAM, art->wall, 0.5f), dx + 32.0f,
                    desk - 2.0f, 7.0f, 2.0f);
        if ((dh % 5u) == 0u)
        {
            /* A desk lamp left on, in a pool of its own on the worktop. */
            fx_rect(r, frame, dx + 38.0f, desk - 9.0f, 1.0f, 9.0f);
            fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f),
                    dx + 35.0f, desk - 11.0f, 6.0f, 2.0f);
            fx_rect(r, fx_dim(FX_WARM, 0.85f), dx + 36.0f, desk - 9.0f, 4.0f,
                    1.0f);
            fx_light_cone(r, dx + 38.0f, desk - 9.0f, 2.0f, 9.0f, 9.0f, FX_WARM,
                          44);
            fx_glow(r, dx + 38.0f, desk - 1.0f, 12.0f, FX_WARM, 30);
        }
        if ((dh >> 3 & 3u) == 0u)
            continue;
        /* A screen left logged in: the only light source at this hour. A
         * screensaver pulse rather than a blink — alive, and not distracting
         * behind a fight. */
        float mx = dx + 12.0f;
        float my = desk - 12.0f;
        fx_rect(r, FX_INK, mx - 1.0f, my - 1.0f, 15.0f, 10.0f);
        fx_rect(r, frame, mx + 5.0f, my + 9.0f, 3.0f, 3.0f);
        float pulse = 0.55f + 0.45f * sinf(s->time * 1.3f + (float)d +
                                           (float)(cluster % 7u));
        pulse = art_pulse(s, pulse, 0.55f);
        SDL_Color glow = (dh & 128u) ? art->accent : fx_mix(art->accent, FX_LAMP, 0.5f);
        fx_rect(r, fx_dim(glow, 0.35f + pulse * 0.45f), mx, my, 13.0f, 8.0f);
        fx_rect_a(r, art->trim_hi, 40, mx, my, 13.0f, 1.0f);
        fx_rect_a(r, FX_INK, 60, mx + 2.0f, my + 3.0f, 6.0f, 1.0f);
        fx_glow(r, mx + 6.0f, my + 4.0f, 18.0f, glow, (Uint8)(22.0f * pulse));
        /* The chair, pushed back from the desk and turned a little, a
         * silhouette with the screen's light along the top of its back:
         * back, seat, the pedestal and its star base. */
        SDL_Color chair = fx_mix(art->near_shape, FX_INK, 0.6f);
        float kx = mx + 16.0f;
        fx_mass(r, chair, kx + 5.0f, floor - 19.0f, 5.0f, 10.0f, 2, 1);
        fx_rect_a(r, glow, (Uint8)(40.0f * pulse), kx + 5.0f, floor - 19.0f,
                  1.0f, 8.0f);
        fx_rect(r, chair, kx - 2.0f, floor - 10.0f, 12.0f, 2.0f);
        fx_rect(r, chair, kx + 3.0f, floor - 8.0f, 2.0f, 6.0f);
        fx_rect(r, chair, kx - 1.0f, floor - 2.0f, 10.0f, 1.0f);
    }
    fx_contact_shadow(r, x + width * 0.5f, floor - 2.0f, width * 0.5f, 0.0f, 60);
}

/* What stands in the gap between two clusters: a plant, the water cooler, a
 * pair of filing drawers, the printer — or nothing, which is also true of an
 * office. */
static void office_gap_prop(const LevelArtScene *s, const LevelThemeArt *art,
                            float cx, float floor, unsigned h)
{
    SDL_Renderer *r = s->renderer;
    SDL_Color body = fx_mix(art->near_shape, art->wall, 0.35f);
    switch (h % 5u)
    {
    case 0:
    {
        /* A potted palm: fronds fanning out of one stem, lit on top. */
        SDL_Color leaf = fx_mix(FX_GREEN_DK, art->near_shape, 0.6f);
        SDL_Color leaf_lit = fx_mix(leaf, art->wall_light, 0.3f);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), cx - 5.0f,
                floor - 9.0f, 10.0f, 9.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), cx - 5.0f,
                floor - 9.0f, 10.0f, 1.0f);
        fx_rect(r, fx_mix(leaf, FX_INK, 0.3f), cx, floor - 20.0f, 1.0f, 11.0f);
        for (int frond = 0; frond < 5; ++frond)
        {
            float fx0 = cx + (float)(frond - 2) * 4.0f;
            float fy = floor - 24.0f + (float)(frond == 2 ? -3 : SDL_abs(frond - 2)) * 2.0f;
            float fw = frond == 2 ? 2.0f : 5.0f;
            fx_rect(r, leaf, fx0 - fw * 0.5f, fy, fw, 2.0f);
            fx_rect(r, leaf, fx0 + (float)(frond - 2) * 1.5f - 1.0f, fy + 2.0f,
                    3.0f, 3.0f + (float)SDL_abs(frond - 2));
            fx_rect(r, leaf_lit, fx0 - fw * 0.5f, fy, fw, 1.0f);
        }
        break;
    }
    case 1:
    {
        /* The water cooler: a pale bottle upended on a white cabinet. */
        fx_rect(r, body, cx - 5.0f, floor - 17.0f, 10.0f, 17.0f);
        fx_rect(r, fx_mix(body, art->wall_light, 0.4f), cx - 5.0f,
                floor - 17.0f, 10.0f, 1.0f);
        fx_mass(r, fx_mix(FX_LAMP, art->near_shape, 0.55f), cx - 4.0f,
                floor - 28.0f, 8.0f, 11.0f, 2, 1);
        fx_rect_a(r, FX_LAMP, 40, cx - 3.0f, floor - 27.0f, 2.0f, 8.0f);
        fx_rect(r, fx_dim(FX_RED, 0.7f), cx - 3.0f, floor - 13.0f, 2.0f, 1.0f);
        fx_rect(r, fx_dim(art->accent, 0.8f), cx + 1.0f, floor - 13.0f, 2.0f,
                1.0f);
        break;
    }
    case 2:
    {
        /* Two filing drawers and the box of somebody else's files on top. */
        fx_rect(r, body, cx - 8.0f, floor - 20.0f, 16.0f, 20.0f);
        fx_rect(r, fx_mix(body, art->wall_light, 0.35f), cx - 8.0f,
                floor - 20.0f, 16.0f, 1.0f);
        fx_rect(r, fx_mix(body, FX_INK, 0.5f), cx - 8.0f, floor - 10.0f, 16.0f,
                1.0f);
        fx_rect(r, fx_mix(body, art->trim_hi, 0.4f), cx - 3.0f, floor - 16.0f,
                6.0f, 1.0f);
        fx_rect(r, fx_mix(body, art->trim_hi, 0.4f), cx - 3.0f, floor - 6.0f,
                6.0f, 1.0f);
        fx_rect(r, fx_mix(FX_WOOD, art->near_shape, 0.5f), cx - 6.0f,
                floor - 25.0f, 12.0f, 5.0f);
        break;
    }
    case 3:
    {
        /* The printer, asleep, one light telling you it is. */
        fx_rect(r, body, cx - 11.0f, floor - 15.0f, 22.0f, 15.0f);
        fx_rect(r, fx_mix(body, art->wall_light, 0.35f), cx - 11.0f,
                floor - 15.0f, 22.0f, 1.0f);
        fx_rect(r, fx_mix(body, FX_INK, 0.45f), cx - 9.0f, floor - 11.0f, 18.0f,
                2.0f);
        fx_rect(r, fx_mix(FX_CREAM, art->wall, 0.4f), cx - 7.0f, floor - 17.0f,
                12.0f, 2.0f);
        float blink = art_pulse(s, sinf(s->time * 0.9f + (float)(h % 9u)) > 0.0f
                                       ? 1.0f : 0.35f, 0.7f);
        fx_rect(r, fx_dim(FX_GREEN, 0.75f * blink), cx + 7.0f, floor - 14.0f,
                1.0f, 1.0f);
        break;
    }
    default:
        break;
    }
}

/*
 * OFFICE — open plan after hours: a window wall onto the tower across the
 * street, the blinds nobody set the same way, a suspended ceiling with one
 * light in three still on, and the cubicle farm in front of it all.
 *
 * Laid out for the storey rather than for the screen: the ceiling and the
 * window head hang from `top`, the sill and the furniture stand on `floor`,
 * and the glass takes whatever height is left between them, so a low storey
 * has a letterbox of city and a tall one a proper window. The city is on the
 * screen, not the wall: every storey looks out at one skyline, each at its
 * own slice of it.
 */
static void backdrop_office_room(const LevelArtScene *s,
                                 const LevelThemeArt *art, const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float wall = 0.45f; /* the window wall's own drift */
    const float cl = room->clip_left;
    const float cr = room->clip_right;
    const float cw = cr - cl;
    const float top = room->top;
    const float floor = room->floor;
    const float high = room->height;
    const float ceiling_h = fminf(12.0f, fmaxf(7.0f, high * 0.11f));
    const float head = top + ceiling_h;
    const float glass_top = head + 3.0f;
    const float sill = floor - fminf(28.0f, fmaxf(16.0f, high * 0.27f));
    const float glass_h = sill - glass_top;
    const float bay_w = 132.0f;

    /* The painted wall under the sill and wherever the glass stops. */
    SDL_Color paint = fx_mix(art->near_shape, art->wall, 0.32f);
    fx_rect(r, paint, cl, head, cw, floor - head);

    if (glass_h > 6.0f)
    {
        lobby_city_view(s, cl, glass_top, cw, glass_h,
                        lobby_horizon(s, 0.55f, 0.08f), 0.4f);
        office_facing_blocks(s, cl, glass_top, cw, glass_h);
        fx_rect_a(r, art->haze, 12, cl, glass_top, cw, glass_h);

        /* The blinds, bay by bay: six vertical louvres to a bay, and each bay
         * of each storey in its own state — drawn up, open edge-on, turned
         * shut, or let down partway. */
        SDL_Color louvre = art->trim_hi;
        LobbyRun bay = lobby_run(s->cam_x, wall, bay_w, 0.0f, cl - bay_w);
        for (; bay.x < cr; bay.x += bay_w, ++bay.index)
        {
            unsigned bh = fx_hash((unsigned)bay.index * 40503u + room->seed);
            if ((bh >> 20) % 5u == 0u)
            {
                office_wall_bay(s, art, bay.x + 5.0f, bay_w - 5.0f, glass_top,
                                sill, bh >> 3);
                continue;
            }
            /* Mostly up or open: the blinds are there to say "window", and
             * a floor of them turned shut is a floor with no city in it. */
            unsigned roll = bh % 20u;
            unsigned state = roll < 6u ? 0u : roll < 12u ? 1u : roll < 15u ? 2u : 3u;
            float drop = state == 0u   ? fminf(8.0f, glass_h * 0.2f)
                         : state == 3u ? glass_h * (0.35f + art_unit(bh, 8) * 0.3f)
                                       : glass_h - 3.0f;
            fx_rect_a(r, louvre, 16, bay.x + 5.0f, glass_top + 1.0f, bay_w - 5.0f,
                      2.0f);
            for (int slat = 0; slat < 6; ++slat)
            {
                float lx = bay.x + 6.0f + (float)slat * 21.0f;
                if (state == 0u)
                {
                    /* Drawn up: the stack bunched under the head. */
                    fx_rect_a(r, louvre, 30, lx, glass_top + 1.0f, 18.0f, drop);
                    fx_rect_a(r, FX_INK, 50, lx, glass_top + drop, 18.0f, 1.0f);
                    continue;
                }
                bool open = state == 1u;
                float lw = open ? 4.0f : 20.0f;
                float ox = open ? 8.0f : 0.0f;
                fx_vgrad(r, lx + ox, glass_top + 1.0f, lw, drop, louvre,
                         open ? 38 : 28, louvre, open ? 20 : 14);
                fx_rect_a(r, FX_INK, 60, lx + ox, glass_top + drop, lw, 1.0f);
                /* Each louvre is a curved strip: its lit edge toward the
                 * room's light, its far edge in its own shadow. */
                fx_rect_a(r, louvre, 40, lx + ox, glass_top + 1.0f, 1.0f, drop);
                if (!open)
                    fx_rect_a(r, FX_INK, 50, lx + 19.0f, glass_top + 1.0f, 1.0f,
                              drop);
            }
            /* The pull chain down the side of the bay. */
            if (state != 0u)
                fx_rect_a(r, louvre, 40, bay.x + 128.0f, glass_top + 3.0f, 1.0f,
                          drop * 0.8f);
            /* Glass catching the room's light, one raking sheen per bay. */
            fx_rect_a(r, art->trim_hi, 8, bay.x + 20.0f, glass_top, 24.0f,
                      glass_h);
        }

        /* Mullions, the head rail and the window board. */
        LobbyRun mull = lobby_run(s->cam_x, wall, bay_w, 0.0f, cl - bay_w);
        for (; mull.x < cr; mull.x += bay_w)
        {
            fx_rect(r, art->near_shape, mull.x, glass_top, 5.0f, glass_h);
            fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), mull.x,
                    glass_top, 1.0f, glass_h);
            fx_rect_a(r, FX_INK, 70, mull.x + 5.0f, glass_top, 1.0f, glass_h);
        }
        fx_rect(r, art->near_shape, cl, glass_top - 3.0f, cw, 4.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.45f), cl,
                glass_top - 3.0f, cw, 1.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.3f), cl, sill, cw, 3.0f);
        fx_rect_a(r, art->trim_hi, 50, cl, sill, cw, 1.0f);
        fx_rect_a(r, FX_INK, 80, cl, sill + 3.0f, cw, 3.0f);
    }

    /* Perimeter trunking under the sill, with a socket every few metres, and
     * the skirting. */
    if (floor - sill > 12.0f)
    {
        float ty = sill + 6.0f;
        fx_rect(r, fx_mix(paint, art->wall_light, 0.18f), cl, ty, cw, 4.0f);
        fx_rect_a(r, FX_INK, 60, cl, ty + 4.0f, cw, 1.0f);
        LobbyRun sock = lobby_run(s->cam_x, wall, 66.0f, 20.0f, cl - 66.0f);
        for (; sock.x < cr; sock.x += 66.0f)
            fx_rect(r, fx_mix(paint, art->trim_hi, 0.35f), sock.x, ty + 1.0f,
                    4.0f, 2.0f);
    }
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), cl, floor - 3.0f, cw,
            3.0f);

    /* The cubicle farm, nearer than the wall and drifting a little faster:
     * three desks to a cluster and something standing in the gap. */
    const float run_w = 224.0f;
    LobbyRun cube = lobby_run(s->cam_x, 0.6f, run_w,
                              lobby_phase(room, 0x51edu, run_w), cl - run_w);
    for (; cube.x < cr + 20.0f; cube.x += run_w, ++cube.index)
    {
        unsigned ch = fx_hash((unsigned)cube.index * 2246822519u + room->seed);
        office_cubicles(s, art, cube.x, floor, ch);
        office_gap_prop(s, art, cube.x + 144.0f + 40.0f, floor, ch >> 7);
    }

    /* The suspended ceiling: a grid of lay-in tiles, and one light in three
     * still on — small and dim, because the lamps that light the floor are
     * the playfield's own and these are the ones further back. */
    fx_rect(r, art->near_shape, cl, top, cw, ceiling_h);
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), cl, head - 1.0f, cw,
            1.0f);
    LobbyRun tile = lobby_run(s->cam_x, wall, 60.0f, 0.0f, cl - 60.0f);
    for (; tile.x < cr; tile.x += 60.0f, ++tile.index)
    {
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), tile.x, top, 2.0f,
                ceiling_h);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.16f),
                tile.x + 2.0f, head - 2.0f, 58.0f, 1.0f);
        unsigned th = fx_hash((unsigned)tile.index * 7919u + room->seed);
        if ((th % 3u) != 0u)
            continue;
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), tile.x + 9.0f,
                head - 3.0f, 42.0f, 3.0f);
        fx_rect(r, fx_dim(art->lamp, 0.62f), tile.x + 10.0f, head - 2.0f, 40.0f,
                2.0f);
        for (int cell = 1; cell < 4; ++cell)
            fx_rect_a(r, FX_INK, 70, tile.x + 10.0f + (float)cell * 10.0f,
                      head - 2.0f, 1.0f, 2.0f);
        fx_glow(r, tile.x + 30.0f, head, 20.0f, art->lamp, 16);
        fx_light_cone(r, tile.x + 30.0f, head, 20.0f, 34.0f,
                      fminf(40.0f, floor - head - 4.0f), art->lamp, 10);
    }
}

/*
 * SERVER — a cold aisle, one storey of it at a time.
 *
 * The room is an aisle between a cable tray and a raised floor, and that is
 * the whole of how it is laid out: the tray hangs a fixed band under the
 * room's own ceiling, the near row of racks stands on the room's own floor
 * and is exactly as tall as what is left between the two, and the next aisle
 * over shows through the gaps a step back and a step darker. A taller room
 * buys a taller rack with more in it, never a bigger picture, so nothing here
 * can run behind a slab.
 *
 * The VAULT borrows this backdrop in its own palette, and in the vault the
 * racks are the evidence: some have been stripped to the rails, the slots
 * standing empty and a patch lead hanging where somebody pulled the unit
 * behind it.
 */

/* What can sit in one slot of a rack, and how many pixels of it each takes. */
typedef enum
{
    SERVER_UNIT_BLANK = 0, /* a blanking plate: nothing fitted, nothing lit */
    SERVER_UNIT_SERVER,    /* four drive bays, a status light, activity */
    SERVER_UNIT_STORAGE,   /* a double-height shelf of drive carriers */
    SERVER_UNIT_SWITCH,    /* two thin switches, their ports chattering */
    SERVER_UNIT_PATCH,     /* a patch panel and the leads looping off it */
    SERVER_UNIT_EMPTY      /* pulled: the rails and nothing between them */
} ServerUnit;

static float server_unit_height(ServerUnit kind)
{
    switch (kind)
    {
    case SERVER_UNIT_STORAGE:
        return 18.0f;
    case SERVER_UNIT_SWITCH:
    case SERVER_UNIT_PATCH:
        return 9.0f;
    case SERVER_UNIT_BLANK:
    case SERVER_UNIT_SERVER:
    case SERVER_UNIT_EMPTY:
    default:
        return 8.0f;
    }
}

/*
 * One status light on a unit's face: its own rate, so a wall of them
 * shimmers rather than strobing in unison, and held at its average when the
 * player has asked for steady lights.
 */
static void server_status_led(const LevelArtScene *s, SDL_Color c, unsigned h,
                              float x, float y, float w, float hh)
{
    float rate = 1.1f + (float)(h % 9u) * 0.4f;
    bool lit = fmodf(s->time * rate + art_unit(h, 5) * 4.0f, 2.0f) < 1.2f;
    float level = art_pulse(s, lit ? 1.0f : 0.16f, 0.66f);
    fx_rect(s->renderer, fx_dim(c, level), x, y, w, hh);
    if (lit && !s->steady_lights && (h % 5u) == 0u)
        fx_glow(s->renderer, x + w * 0.5f, y + hh * 0.5f, 9.0f, c, 60);
}

/* One slot of a near rack, face and lights together. `x` is the rack's left
 * edge and the face runs from x + 6 to x + 98. */
static void server_unit(const LevelArtScene *s, const LevelThemeArt *art,
                        ServerUnit kind, float x, float uy, unsigned uh)
{
    SDL_Renderer *r = s->renderer;
    SDL_Color face = fx_mix(art->near_shape, art->wall, 0.5f);
    SDL_Color slot = fx_mix(art->near_shape, FX_INK, 0.3f);
    SDL_Color edge = fx_mix(face, art->wall_light, 0.35f);
    SDL_Color led = (uh & 4u) ? art->accent : FX_GREEN;
    float fh = server_unit_height(kind);

    switch (kind)
    {
    case SERVER_UNIT_EMPTY:
        /* Nothing between the rails but the cage nuts somebody left. */
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.12f), x + 7.0f,
                uy + 3.0f, 2.0f, 1.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.12f), x + 95.0f,
                uy + 3.0f, 2.0f, 1.0f);
        return;
    case SERVER_UNIT_BLANK:
        fx_rect(r, fx_mix(art->near_shape, art->wall, 0.3f), x + 6.0f, uy,
                92.0f, fh);
        fx_rect(r, slot, x + 8.0f, uy + 3.0f, 1.0f, 1.0f);
        fx_rect(r, slot, x + 95.0f, uy + 3.0f, 1.0f, 1.0f);
        return;
    case SERVER_UNIT_STORAGE:
    {
        /* Two rows of carriers, each with the pin of light a drive shows
         * while it is being read. */
        fx_rect(r, face, x + 6.0f, uy, 92.0f, fh);
        fx_rect(r, edge, x + 6.0f, uy, 92.0f, 1.0f);
        for (int row = 0; row < 2; ++row)
        {
            for (int c = 0; c < 6; ++c)
            {
                float cx = x + 9.0f + (float)c * 13.0f;
                float cy = uy + 2.0f + (float)row * 8.0f;
                fx_rect(r, slot, cx, cy, 11.0f, 6.0f);
                fx_rect(r, fx_mix(slot, art->wall_light, 0.18f), cx, cy, 11.0f,
                        1.0f);
                unsigned dh = fx_hash(uh + (unsigned)(row * 6 + c) * 977u);
                float busy = fmodf(s->time * (2.0f + (float)(dh % 7u)) +
                                       art_unit(dh, 8),
                                   1.0f);
                float on = art_pulse(s, busy < 0.35f ? 0.95f : 0.22f, 0.5f);
                if ((dh & 7u) == 0u)
                    on = 0.08f; /* a dead drive nobody has swapped */
                fx_rect(r, fx_dim((dh & 32u) ? FX_AMBER : FX_GREEN, on),
                        cx + 8.0f, cy + 3.0f, 2.0f, 1.0f);
            }
        }
        server_status_led(s, led, uh, x + 88.0f, uy + 3.0f, 3.0f, 4.0f);
        server_status_led(s, FX_GREEN, uh >> 3, x + 88.0f, uy + 10.0f, 3.0f,
                          2.0f);
        return;
    }
    case SERVER_UNIT_SWITCH:
        /* Two 1U switches: the port row is the face, and the ports chatter. */
        for (int half = 0; half < 2; ++half)
        {
            float sy = uy + (float)half * 5.0f;
            fx_rect(r, face, x + 6.0f, sy, 92.0f, 4.0f);
            fx_rect(r, edge, x + 6.0f, sy, 92.0f, 1.0f);
            fx_rect(r, slot, x + 9.0f, sy + 1.0f, 72.0f, 2.0f);
            for (int port = 0; port < 12; ++port)
            {
                unsigned ph = fx_hash(uh + (unsigned)(half * 16 + port) * 131u);
                float chatter = fmodf(s->time * (3.0f + (float)(ph % 7u)) +
                                          art_unit(ph, 4),
                                      1.0f);
                float on = art_pulse(s, chatter < 0.5f ? 0.8f : 0.2f, 0.5f);
                if ((ph & 3u) == 0u)
                    on = 0.1f;
                fx_rect(r, fx_dim(FX_GREEN, on), x + 10.0f + (float)port * 6.0f,
                        sy + 2.0f, 2.0f, 1.0f);
            }
            server_status_led(s, led, uh >> (unsigned)(half * 4), x + 88.0f,
                              sy + 1.0f, 3.0f, 2.0f);
        }
        return;
    case SERVER_UNIT_PATCH:
    {
        /* The panel, and under it the leads it feeds, looping across to the
         * cable manager at the side of the rack. */
        fx_rect(r, face, x + 6.0f, uy, 92.0f, 4.0f);
        fx_rect(r, edge, x + 6.0f, uy, 92.0f, 1.0f);
        for (int port = 0; port < 14; ++port)
            fx_rect(r, slot, x + 9.0f + (float)port * 6.0f, uy + 1.0f, 3.0f,
                    2.0f);
        SDL_Color leads[3] = {fx_mix(art->near_shape, art->accent, 0.45f),
                              fx_mix(art->near_shape, art->wall_light, 0.4f),
                              fx_mix(art->near_shape, FX_INK, 0.1f)};
        fx_rect(r, slot, x + 6.0f, uy + 5.0f, 92.0f, 4.0f);
        for (int lead = 0; lead < 7; ++lead)
        {
            unsigned lh = fx_hash(uh + (unsigned)lead * 53u);
            float lx = x + 10.0f + (float)(lh % 80u);
            float drop = 2.0f + (float)((lh >> 8) % 3u);
            SDL_Color c = leads[(lh >> 12) % 3u];
            fx_rect(r, c, lx, uy + 3.0f, 1.0f, drop + 1.0f);
            fx_rect(r, c, lx, uy + 4.0f + drop, x + 97.0f - lx, 1.0f);
        }
        return;
    }
    case SERVER_UNIT_SERVER:
    default:
        fx_rect(r, face, x + 6.0f, uy, 92.0f, fh);
        fx_rect(r, edge, x + 6.0f, uy, 92.0f, 1.0f);
        for (int bay = 0; bay < 4; ++bay)
            fx_rect(r, slot, x + 9.0f + (float)bay * 15.0f, uy + 2.0f, 13.0f,
                    4.0f);
        /* A pull handle at each ear, catching the aisle light. */
        fx_rect(r, fx_mix(face, art->wall_light, 0.25f), x + 7.0f, uy + 2.0f,
                1.0f, 4.0f);
        server_status_led(s, led, uh, x + 88.0f, uy + 2.0f, 3.0f, 4.0f);
        {
            float busy = fmodf(s->time * (5.0f + (float)(uh % 5u)) +
                                   art_unit(uh, 17),
                               1.0f);
            float on = art_pulse(s, busy < 0.3f ? 0.9f : 0.25f, 0.45f);
            fx_rect(r, fx_dim(FX_AMBER, on), x + 80.0f, uy + 3.0f, 2.0f, 2.0f);
        }
        return;
    }
}

/*
 * One rack of the near row, standing on `floor` with its top at `top`.
 *
 * What is fitted slot by slot belongs to the rack and the slot; how many
 * slots there are belongs to the room, because the rack is as tall as the
 * room lets it be and a slot is a fixed number of pixels. Whatever does not
 * divide into a unit is left as a blank at the foot, the way a real rack
 * fills from the top.
 */
static void server_rack(const LevelArtScene *s, const LevelThemeArt *art,
                        float x, float top, float floor, unsigned rack,
                        bool vault)
{
    SDL_Renderer *r = s->renderer;
    const float w = 104.0f;
    float h = floor - top;
    unsigned style = (rack >> 3) % 20u;
    bool stripped = vault && ((rack >> 9) % 3u) == 0u;
    bool door = !stripped && style >= 11u && style < 17u;
    bool network = style >= 17u;

    fx_rect(r, art->near_shape, x, top, w, h);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.4f), x, top, w,
            2.0f);
    SDL_Color recess = fx_mix(art->near_shape, FX_INK, 0.5f);
    fx_rect(r, recess, x + 4.0f, top + 6.0f, w - 8.0f, h - 12.0f);
    /* The number plate on the top panel, and the vent slots in the plinth. */
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), x + 8.0f,
            top + 3.0f, 6.0f + (float)(rack % 5u), 2.0f);
    for (int vent = 0; vent < 6; ++vent)
        fx_rect(r, recess, x + 30.0f + (float)vent * 8.0f, floor - 4.0f, 5.0f,
                1.0f);

    /* The mounting rails, with their square holes, which is what shows in
     * a slot with nothing fitted in it. */
    SDL_Color rail = fx_mix(art->near_shape, art->wall, 0.25f);
    fx_rect(r, rail, x + 5.0f, top + 6.0f, 1.0f, h - 12.0f);
    fx_rect(r, rail, x + 98.0f, top + 6.0f, 1.0f, h - 12.0f);

    float y = top + 8.0f;
    float y_end = floor - 7.0f;
    for (int i = 0; i < 48; ++i)
    {
        unsigned uh = fx_hash(rack + (unsigned)i * 2654435761u);
        unsigned roll = (uh >> 11) % 16u;
        ServerUnit kind;
        if (network)
            kind = roll < 6u   ? SERVER_UNIT_SWITCH
                   : roll < 10u ? SERVER_UNIT_PATCH
                   : roll < 13u ? SERVER_UNIT_SERVER
                                : SERVER_UNIT_BLANK;
        else
            kind = roll < 7u    ? SERVER_UNIT_SERVER
                   : roll < 10u ? SERVER_UNIT_STORAGE
                   : roll < 12u ? SERVER_UNIT_SWITCH
                   : roll < 13u ? SERVER_UNIT_PATCH
                                : SERVER_UNIT_BLANK;
        if (stripped && ((uh >> 20) % 3u) != 0u)
            kind = SERVER_UNIT_EMPTY;
        float uh_px = server_unit_height(kind);
        if (y + uh_px > y_end)
        {
            kind = stripped ? SERVER_UNIT_EMPTY : SERVER_UNIT_BLANK;
            uh_px = y_end - y;
            if (uh_px < 3.0f)
                break;
            if (kind == SERVER_UNIT_BLANK)
            {
                fx_rect(r, fx_mix(art->near_shape, art->wall, 0.3f), x + 6.0f,
                        y, 92.0f, uh_px);
            }
            break;
        }
        server_unit(s, art, kind, x, y, uh);
        y += uh_px + 2.0f;
    }

    if (stripped)
    {
        /* The lead that went to whatever was pulled, left hanging. */
        float lx = x + 20.0f + (float)((rack >> 13) % 60u);
        float drop = fminf(h * 0.5f, 18.0f + (float)((rack >> 7) % 14u));
        SDL_Color lead = fx_mix(art->near_shape, art->accent, 0.4f);
        fx_rect(r, lead, lx, top + 6.0f, 1.0f, drop);
        fx_rect(r, lead, lx - 1.0f, top + 6.0f + drop, 3.0f, 2.0f);
    }
    if (door)
    {
        /* A perforated door: the equipment behind it goes dark and its
         * lights come through the mesh a step down. */
        fx_rect_a(r, FX_INK, 120, x + 4.0f, top + 6.0f, w - 8.0f, h - 12.0f);
        SDL_Color mesh = fx_mix(art->near_shape, art->wall, 0.35f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, mesh.r, mesh.g, mesh.b, 150);
        for (float mx = x + 6.0f; mx < x + w - 5.0f; mx += 3.0f)
            fx_fill(r, mx, top + 7.0f, 1.0f, h - 14.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), x + 90.0f,
                top + h * 0.5f - 6.0f, 2.0f, 12.0f);
    }
    if (network)
    {
        /* Vertical cable managers down both sides, full of leads. */
        SDL_Color bundle = fx_mix(art->near_shape, art->accent, 0.3f);
        fx_rect(r, bundle, x + 1.0f, top + 6.0f, 3.0f, h - 12.0f);
        fx_rect(r, fx_mix(bundle, FX_INK, 0.4f), x + 3.0f, top + 6.0f, 1.0f,
                h - 12.0f);
    }

    /* The aisle strip catching the top of the rack and the dark gathering
     * toward the floor, which is what makes it a cabinet standing in a lit
     * aisle rather than a panel of lights. */
    fx_vgrad(r, x, top, w, fminf(16.0f, h * 0.25f), art->lamp, 26, art->lamp,
             0);
    fx_vgrad(r, x, top + h * 0.45f, w, h * 0.55f, FX_INK, 0, FX_INK, 60);
    /* The frame's uprights: lit on the side the aisle light reaches, and the
     * rack's own shadow on the floor it stands on. */
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.22f), x, top, 1.0f,
            h);
    fx_rect_a(r, FX_INK, 110, x + 101.0f, top, 3.0f, h);
    fx_rect_a(r, FX_INK, 90, x - 3.0f, floor - 2.0f, w + 8.0f, 2.0f);
}

/* One rack of the aisle behind: the same shape a step back and a step down,
 * its units a ladder of dark plates and a scatter of lights knocked right
 * back, because the next aisle over is not what the eye is meant to read. */
static void server_far_rack(const LevelArtScene *s, const LevelThemeArt *art,
                            ArtBatch *batch, float x, float top, float floor,
                            unsigned rack)
{
    SDL_Renderer *r = s->renderer;
    float h = floor - top;
    SDL_Color unit = fx_mix(art->far_shape, art->near_shape, 0.5f);
    fx_rect(r, art->far_shape, x, top, 78.0f, h);
    fx_rect(r, fx_mix(art->far_shape, art->accent, 0.12f), x, top, 78.0f, 2.0f);
    int units = (int)((h - 10.0f) / 9.0f);
    for (int u = 0; u < units; ++u)
        art_batch_add(r, batch, unit, 255, x + 4.0f,
                      top + 5.0f + (float)u * 9.0f, 70.0f, 6.0f);
    art_batch_flush(r, batch, unit, 255);
    fx_rect_a(r, FX_INK, 90, x + 74.0f, top, 4.0f, h);
    for (int led = 0; led < 5 && units > 0; ++led)
    {
        unsigned lh = fx_hash(rack * 7u + (unsigned)led * 887u);
        float ly = top + 5.0f + (float)(lh % (unsigned)units) * 9.0f;
        float rate = 0.6f + (float)(lh % 5u) * 0.3f;
        float live = fmodf(s->time * rate + art_unit(lh, 9) * 3.0f, 2.0f) < 1.3f
                         ? 0.55f
                         : 0.12f;
        SDL_Color c = (lh & 16u) ? art->accent : FX_GREEN;
        fx_rect(r, fx_dim(c, art_pulse(s, live, 0.40f)), x + 62.0f, ly + 2.0f,
                2.0f, 2.0f);
    }
}

static void backdrop_server_room(const LevelArtScene *s,
                                 const LevelThemeArt *art, const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    ArtBatch batch;
    batch.count = 0;
    const bool vault = s->level->map.theme == LEVEL_THEME_VAULT;
    const float clip_w = room->clip_right - room->clip_left;

    /* The band under the ceiling: the tray hangs a few pixels down and is
     * a little slimmer in a low room, and the racks take the rest. */
    const float tray_h = room->height >= 90.0f ? 9.0f : 6.0f;
    const float gap = fmaxf(4.0f, fminf(10.0f, room->height * 0.06f));
    float tray_y = room->top + fminf(6.0f, room->height * 0.05f);
    float rack_top = tray_y + tray_h + gap;
    /* A rack is built to a person and not to the room: past its own height
     * the tray comes down to meet it on longer hangers, and the height over
     * that is the room's, with a second ladder of cable up under the slab. */
    const float rack_max = 168.0f;
    const bool hall = room->floor - rack_top > rack_max;
    if (hall)
    {
        rack_top = room->floor - rack_max;
        tray_y = rack_top - gap - tray_h;
    }
    const float tray_foot = tray_y + tray_h;

    /* The aisle behind, standing on the same floor and reaching up under
     * the tray, seen only where the near row leaves a gap. */
    {
        const float period = 96.0f;
        /* Each storey's run starts at a place of its own, so the same
         * furniture is not stacked floor over floor like a stamp. */
        const float cam =
            s->cam_x + (float)(fx_hash(room->seed ^ 0x1111u) % 96u) / 0.09f;
        float x = art_scroll(cam, 0.09f, period) - 20.0f;
        int idx = art_repeat(cam, 0.09f, period);
        while (x + 78.0f < room->clip_left)
        {
            x += period;
            ++idx;
        }
        for (; x < room->clip_right; x += period, ++idx)
            server_far_rack(s, art, &batch, x, tray_foot + 1.0f,
                            room->floor - 1.0f,
                            fx_hash(room->seed ^ (unsigned)idx * 0x2c1b3c6du));
    }

    /* The tray: a wire basket on threaded hangers, the leads it carries
     * showing over its lip, and the aisle's one strip of white light run
     * along its underside. */
    SDL_Color tray = fx_mix(art->near_shape, FX_INK, 0.2f);
    SDL_Color tray_lit = fx_mix(art->near_shape, art->wall_light, 0.3f);
    {
        const float period = 26.0f;
        float x = art_scroll(s->cam_x, 0.3f, period);
        int hanger = art_repeat(s->cam_x, 0.3f, period);
        while (x + 2.0f < room->clip_left)
        {
            x += period;
            ++hanger;
        }
        for (; x < room->clip_right; x += period, ++hanger)
        {
            if ((hanger & 3) == 0)
                fx_rect(r, fx_mix(art->far_shape, art->wall_light, 0.12f),
                        x + 1.0f, room->top, 1.0f, tray_y - room->top);
        }
    }
    if (hall)
    {
        /* Up under the slab, the hall's own services: the supply duct the
         * cold aisle is fed from, flanged every few metres, and a second
         * ladder of cable under it. */
        float duct = room->top + 12.0f;
        SDL_Color sheet = fx_mix(art->near_shape, art->wall, 0.3f);
        fx_rect(r, sheet, room->clip_left, duct, clip_w, 22.0f);
        fx_rect(r, fx_mix(sheet, art->wall_light, 0.3f), room->clip_left,
                duct, clip_w, 2.0f);
        fx_vgrad(r, room->clip_left, duct + 8.0f, clip_w, 14.0f, FX_INK, 0,
                 FX_INK, 90);
        fx_rect_a(r, FX_INK, 90, room->clip_left, duct + 22.0f, clip_w, 3.0f);
        const float flange = 72.0f;
        float fxp = art_scroll(s->cam_x, 0.3f, flange);
        while (fxp + 3.0f < room->clip_left)
            fxp += flange;
        for (; fxp < room->clip_right; fxp += flange)
        {
            fx_rect(r, fx_mix(sheet, art->wall_light, 0.2f), fxp, duct - 1.0f,
                    3.0f, 24.0f);
            fx_rect(r, fx_mix(sheet, FX_INK, 0.4f), fxp + 3.0f, duct, 1.0f,
                    22.0f);
        }
        float ladder = duct + 34.0f;
        fx_rect(r, tray, room->clip_left, ladder, clip_w, 5.0f);
        fx_rect(r, tray_lit, room->clip_left, ladder, clip_w, 1.0f);
        fx_rect_a(r, FX_INK, 80, room->clip_left, ladder + 5.0f, clip_w, 3.0f);
    }
    fx_rect(r, fx_mix(art->near_shape, art->accent, 0.18f), room->clip_left,
            tray_y - 2.0f, clip_w, 2.0f);
    fx_rect(r, tray, room->clip_left, tray_y, clip_w, tray_h);
    fx_rect(r, tray_lit, room->clip_left, tray_y, clip_w, 1.0f);
    fx_rect(r, fx_mix(tray, FX_INK, 0.4f), room->clip_left, tray_foot - 1.0f,
            clip_w, 1.0f);
    {
        const float period = 4.0f;
        float x = art_scroll(s->cam_x, 0.3f, period);
        while (x < room->clip_left - 1.0f)
            x += period;
        for (; x < room->clip_right; x += period)
            art_batch_add(r, &batch, tray_lit, 255, x, tray_y + 1.0f, 1.0f,
                          tray_h - 2.0f);
        art_batch_flush(r, &batch, tray_lit, 90);
    }
    fx_rect(r, fx_dim(art->lamp, 0.55f), room->clip_left, tray_foot, clip_w,
            1.0f);
    fx_vgrad(r, room->clip_left, tray_foot + 1.0f, clip_w,
             fminf(44.0f, room->height * 0.4f), art->lamp, 22, art->lamp, 0);

    /* The near row, and the leads dropping out of the tray into each rack. */
    const float period = 132.0f;
    const float cam =
        s->cam_x + (float)(fx_hash(room->seed ^ 0x2222u) % 132u) / 0.24f;
    float x = art_scroll(cam, 0.24f, period) - 30.0f;
    int idx = art_repeat(cam, 0.24f, period);
    while (x + 104.0f < room->clip_left)
    {
        x += period;
        ++idx;
    }
    for (; x < room->clip_right; x += period, ++idx)
    {
        unsigned rack = fx_hash(room->seed + (unsigned)idx * 0x9e3779b9u);
        server_rack(s, art, x, rack_top, room->floor, rack, vault);
        SDL_Color lead = fx_mix(art->near_shape, FX_INK, 0.35f);
        for (int cable = 0; cable < 3; ++cable)
            fx_rect(r, cable == 1 ? fx_mix(art->near_shape, art->accent, 0.3f)
                                  : lead,
                    x + 20.0f + (float)cable * 5.0f + (float)(rack % 4u) * 9.0f,
                    tray_foot, 2.0f, rack_top - tray_foot);
        fx_glow(r, x + 52.0f, rack_top + (room->floor - rack_top) * 0.5f,
                fminf(90.0f, room->height * 0.7f), art->accent, 14);
        if (vault && ((unsigned)idx % 3u) == 0u)
        {
            /* The emergency bulkhead somebody left burning when they walked
             * out: the one warm light in the vault, in the gap between two
             * racks, throwing a little of itself down the aisle. */
            float ex = x + 118.0f;
            float ey = tray_foot + 2.0f;
            fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.2f), ex - 5.0f, ey,
                    10.0f, 5.0f);
            fx_rect(r, fx_dim(FX_WARM, 0.85f), ex - 3.0f, ey + 1.0f, 6.0f,
                    3.0f);
            fx_glow(r, ex, ey + 3.0f, 26.0f, FX_WARM, 46);
            fx_light_cone(r, ex, ey + 5.0f, 3.0f, 14.0f,
                          fminf(60.0f, room->floor - ey - 5.0f), FX_WARM, 30);
        }
    }
}

/*
 * CANTEEN — the servery, from the tables.
 *
 * It used to be three flat bands and a wall of faint horizontal lines, which at
 * the size this hall is drawn left most of the sector a flat brown field with
 * the ceiling cones hanging in it. A kitchen reads by its planes: the tiled
 * back wall, the lit hatches through into the kitchen behind it (the warmest
 * light in the building, and the one thing out here that is further away than
 * the wall), the tiled columns standing in front of that, and then the counter
 * under its canopy with the steam coming off the wells.
 *
 * Laid out per room, because the floor is three kinds of room: the dining
 * hall, twelve tiles tall with the servery along its foot, gets all of that
 * plus the extract duct along its head; a low storey is a short servery —
 * hatches, menu boxes, a low counter — with no room for a canopy; and a
 * two-tile storey is the back of house, a stainless bench under a shelf of
 * pans. Every layer stands on `floor` or hangs from `top`, and the
 * tile bond is counted up from the floor in world rows so that two rooms
 * sharing a floor meet at the doorway between them on the same grout line.
 */


/* The glazed tile every room on this floor is lined with, and its skirting. */
static void canteen_tiles(const LevelArtScene *s, const LevelThemeArt *art,
                          const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float w = room->clip_right - room->clip_left;
    ArtBatch batch;
    batch.count = 0;

    /* Large glazed tiles in running bond, the grout a shade over the air.
     * The columns are world-anchored to the layer and the courses to the
     * building, so the bond slides with the wall instead of shimmering. */
    SDL_Color grout = fx_mix(art->air_bottom, art->wall_light, 0.18f);
    int col0 = 0;
    float x0 = art_room_first(s, room, 0.06f, 32.0f, 48.0f, 0u, &col0);
    int course = room->floor_row * 2 - 1;
    for (float y = room->floor - 16.0f; y > room->top - 16.0f;
         y -= 16.0f, --course)
    {
        if (y > room->clip_bottom || y + 16.0f < room->clip_top)
            continue;
        art_batch_add(r, &batch, grout, 40, room->clip_left, y, w, 1.0f);
        float offset = (course & 1) ? 16.0f : 0.0f;
        for (float x = x0 + offset; x < room->clip_right; x += 32.0f)
            art_batch_add(r, &batch, grout, 40, x, y + 1.0f, 1.0f, 15.0f);
    }
    art_batch_flush(r, &batch, grout, 40);

    /* Glaze: one tile in a handful catches the ceiling along its top edge,
     * which is what separates a glazed tile from a painted grid. Keyed to
     * the tile's own course and column. */
    course = room->floor_row * 2 - 1;
    for (float y = room->floor - 16.0f; y > room->top - 16.0f;
         y -= 16.0f, --course)
    {
        if (y > room->clip_bottom || y + 16.0f < room->clip_top)
            continue;
        float offset = (course & 1) ? 16.0f : 0.0f;
        int col = col0;
        for (float x = x0 + offset; x < room->clip_right; x += 32.0f, ++col)
        {
            unsigned h = art_hash(col * 7 + 3, course * 5 + 811);
            if ((h % 9u) != 0u)
                continue;
            art_batch_add(r, &batch, art->wall_light, 22, x + 2.0f, y + 2.0f,
                          12.0f + (float)(h >> 8 & 15u), 1.0f);
        }
    }
    art_batch_flush(r, &batch, art->wall_light, 22);

    /* The wall is lit from the ceiling, and gives more of it back near the
     * top than down among the tables. */
    fx_vgrad(r, room->clip_left, room->top, w,
             fminf(room->height * 0.5f, 170.0f), art->wall_light, 14,
             art->wall_light, 0);
    /* A coved skirting in a darker glaze, the mop line every kitchen has. */
    fx_rect_a(r, art->wall_dark, 70, room->clip_left, room->floor - 16.0f, w,
              16.0f);
    fx_rect_a(r, art->trim_hi, 22, room->clip_left, room->floor - 16.0f, w,
              1.0f);
}

/*
 * One hatch through into the kitchen, `top` to `bottom`, with its menu box
 * over it. The kitchen is the warmest light in the building and the one thing
 * on this floor further away than the wall, so it carries its own tiling,
 * shelving and range; a short hatch keeps the shelf and the line and drops the
 * rest rather than squashing it.
 */
static void canteen_hatch(SDL_Renderer *r, const LevelThemeArt *art, float hx,
                          float top, float bottom, int hatch, unsigned seed)
{
    const float hw = 150.0f;
    const float hh = bottom - top;
    unsigned h = art_hash(hatch, 613 + (int)(seed & 1023u));
    SDL_Color kitchen_hi = fx_dim(art->lamp, 0.34f);
    SDL_Color kitchen_lo = fx_dim(art->lamp, 0.13f);
    SDL_Color steel_dark = fx_mix(art->far_shape, FX_INK, 0.4f);
    ArtBatch batch;
    batch.count = 0;

    /* The light box over the hatch: a lit menu with its lines of type, the
     * only sign in the room anybody still reads. */
    fx_rect(r, steel_dark, hx + 20.0f, top - 22.0f, hw - 40.0f, 16.0f);
    fx_rect(r, fx_dim(art->lamp, 0.38f), hx + 22.0f, top - 20.0f, hw - 44.0f,
            12.0f);
    fx_rect_a(r, art->lamp, 60, hx + 22.0f, top - 20.0f, hw - 44.0f, 1.0f);
    for (int word = 0; word < 4; ++word)
        fx_rect(r, fx_mix(art->far_shape, art->lamp, 0.12f),
                hx + 28.0f + (float)word * 24.0f, top - 17.0f,
                10.0f + (float)((h >> (unsigned)(word * 2)) % 8u), 2.0f);
    fx_rect(r, fx_mix(art->far_shape, art->lamp, 0.12f), hx + 28.0f,
            top - 13.0f, 60.0f + (float)(h % 30u), 2.0f);
    fx_rect_a(r, FX_INK, 70, hx + 20.0f, top - 6.0f, hw - 40.0f, 2.0f);

    /* The opening: its reveal first, then the kitchen through it. */
    fx_rect(r, FX_INK, hx - 3.0f, top - 3.0f, hw + 6.0f, hh + 6.0f);
    fx_vgrad(r, hx, top, hw, hh, kitchen_lo, 255, kitchen_hi, 255);
    /* The kitchen is tiled too, a step finer than the hall. */
    for (float ky = top + 10.0f; ky < bottom; ky += 10.0f)
        art_batch_add(r, &batch, FX_INK, 36, hx, ky, hw, 1.0f);
    for (float kx = hx + 12.0f; kx < hx + hw; kx += 20.0f)
        art_batch_add(r, &batch, FX_INK, 36, kx, top, 1.0f, hh);
    art_batch_flush(r, &batch, FX_INK, 36);

    /* Shelving along the back of the kitchen, and what is stacked on it:
     * pans and tubs as silhouettes against the lit wall. */
    SDL_Color sil = fx_mix(art->far_shape, kitchen_lo, 0.35f);
    int shelves = hh >= 96.0f ? 3 : (hh >= 60.0f ? 2 : 1);
    for (int shelf = 0; shelf < shelves; ++shelf)
    {
        float sy = top + 16.0f + (float)shelf * 26.0f;
        fx_rect(r, sil, hx + 6.0f, sy, hw - 12.0f, 2.0f);
        float px = hx + 10.0f;
        for (int item = 0; item < 6 && px < hx + hw - 20.0f; ++item)
        {
            unsigned ih = art_hash(hatch * 16 + shelf * 6 + item, 71);
            float iw = 8.0f + (float)(ih % 12u);
            float ihh = 5.0f + (float)((ih >> 4) % 9u);
            fx_rect(r, sil, px, sy - ihh, iw, ihh);
            /* The lamp behind catches the lip of a pan. */
            fx_rect_a(r, art->lamp, 26, px, sy - ihh, iw, 1.0f);
            px += iw + 3.0f + (float)((ih >> 9) % 8u);
        }
    }
    /* Utensils hanging from a rail near the front of the kitchen. */
    fx_rect(r, sil, hx + 4.0f, top + 4.0f, hw - 8.0f, 1.0f);
    for (int tool = 0; tool < 7; ++tool)
    {
        unsigned th = art_hash(hatch * 8 + tool, 29);
        float tx = hx + 14.0f + (float)tool * 19.0f;
        float tl = fminf(10.0f + (float)(th % 8u), hh * 0.3f);
        fx_rect(r, sil, tx, top + 5.0f, 1.0f, tl);
        fx_rect(r, sil, tx - 1.0f - (float)(th & 1u), top + 5.0f + tl,
                3.0f + (float)((th >> 3) & 1u), 3.0f);
    }
    /* The range along the back: its canopy and the lit strip under it, where
     * the hatch is tall enough to show one, and always the stainless top of
     * the line the cooks work at. */
    if (hh >= 60.0f)
    {
        fx_rect(r, fx_mix(art->far_shape, kitchen_lo, 0.2f), hx + 30.0f,
                bottom - 40.0f, hw - 60.0f, 8.0f);
        fx_rect_a(r, art->lamp, 30, hx + 30.0f, bottom - 32.0f, hw - 60.0f,
                  18.0f);
    }
    float line = fminf(14.0f, hh * 0.3f);
    fx_rect(r, fx_mix(art->far_shape, kitchen_lo, 0.45f), hx + 8.0f,
            bottom - line, hw - 16.0f, line);
    fx_rect(r, fx_mix(kitchen_hi, art->trim_hi, 0.3f), hx + 8.0f,
            bottom - line, hw - 16.0f, 1.0f);
    for (int ring = 0; ring < 3; ++ring)
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.3f),
                hx + 40.0f + (float)ring * 26.0f, bottom - line - 2.0f, 14.0f,
                2.0f);

    /* Shutter: down, half down or up. A roller shutter down is a closed
     * hatch; slats are its only texture. Which one belongs to the hatch. */
    unsigned shut = (h >> 3) % 7u;
    float shutter = shut == 0u ? hh : (shut == 1u ? hh * 0.45f : 7.0f);
    SDL_Color slat = fx_mix(art->far_shape, art->wall, 0.15f);
    fx_rect(r, slat, hx, top, hw, shutter);
    for (float sy = top + 3.0f; sy < top + shutter - 1.0f; sy += 4.0f)
        art_batch_add(r, &batch, FX_INK, 90, hx, sy, hw, 1.0f);
    art_batch_flush(r, &batch, FX_INK, 90);
    fx_rect(r, fx_mix(slat, art->wall_light, 0.3f), hx, top + shutter - 2.0f,
            hw, 2.0f);
    /* The shutter box over the opening. */
    fx_rect(r, fx_mix(slat, FX_INK, 0.25f), hx - 3.0f, top - 3.0f, hw + 6.0f,
            3.0f);

    /* The stainless sill the plates go out over. */
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), hx - 6.0f,
            bottom, hw + 12.0f, 4.0f);
    fx_rect(r, art->trim_hi, hx - 6.0f, bottom, hw + 12.0f, 1.0f);
    fx_rect_a(r, FX_INK, 90, hx - 6.0f, bottom + 4.0f, hw + 12.0f, 3.0f);
    /* Kitchen light spilling out over the sill onto the wall below. */
    if (shut != 0u)
        fx_vgrad(r, hx - 6.0f, bottom + 4.0f, hw + 12.0f, 30.0f, art->lamp, 18,
                 art->lamp, 0);
}

/* The counter a servery runs along its foot: stainless on a toe kick, the
 * sneeze guard over it and the gastronorm wells under their heat lamps, with
 * the steam coming off them. `rise` is how far the steam climbs. */
static void canteen_counter(const LevelArtScene *s, const LevelThemeArt *art,
                            float x, float counter, float floor, float guard,
                            float rise)
{
    SDL_Renderer *r = s->renderer;
    const float body = floor - counter;
    SDL_Color front = fx_mix(art->near_shape, art->wall_light, 0.35f);
    fx_rect(r, front, x, counter, 200.0f, body);
    fx_vgrad(r, x, counter + 3.0f, 200.0f, body - 3.0f, FX_INK, 0, FX_INK, 90);
    fx_rect(r, art->trim_hi, x, counter, 200.0f, 3.0f);
    fx_rect_a(r, FX_INK, 80, x, counter + 3.0f, 200.0f, 2.0f);
    if (body > 30.0f)
    {
        /* The tray rail along the front, and the panels under it. */
        fx_rect(r, fx_mix(front, art->trim_hi, 0.4f), x - 4.0f,
                counter + 14.0f, 208.0f, 2.0f);
        fx_rect_a(r, FX_INK, 90, x - 4.0f, counter + 16.0f, 208.0f, 2.0f);
        for (float px = x + 50.0f; px < x + 200.0f; px += 50.0f)
            fx_rect_a(r, FX_INK, 60, px, counter + 20.0f, 1.0f, body - 28.0f);
    }
    /* Toe kick: the counter stands on the floor rather than hovering. */
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.55f), x + 3.0f, floor - 6.0f,
            194.0f, 6.0f);

    /* The sneeze guard: one pane of glass on posts, caught by the heat lamps
     * along its top edge. */
    fx_rect_a(r, art->trim_hi, 14, x + 12.0f, counter - guard, 176.0f,
              guard - 8.0f);
    fx_rect_a(r, art->trim_hi, 90, x + 12.0f, counter - guard, 176.0f, 1.0f);
    fx_rect_a(r, art->trim_hi, 30, x + 14.0f, counter - guard + 2.0f, 30.0f,
              1.0f);
    for (int post = 0; post < 3; ++post)
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.4f),
                x + 12.0f + (float)post * 87.0f, counter - guard, 2.0f, guard);
    /* Gastronorm wells under heat lamps; the glow is the warmest thing in
     * the game and the reason this sector feels different. */
    for (int well = 0; well < 3; ++well)
    {
        float wx = x + 22.0f + (float)well * 60.0f;
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), wx, counter + 5.0f,
                44.0f, fminf(12.0f, body - 10.0f));
        fx_rect(r, art->accent, wx + 6.0f, counter - guard + 8.0f, 32.0f, 3.0f);
        fx_glow(r, wx + 22.0f, counter - 6.0f, 30.0f, art->lamp, 52);
        /* Steam off the well, turning over in the lamp light. Each puff
         * rises, spreads and thins out before the next takes its place, so
         * the loop never shows a seam. */
        for (int puff = 0; puff < 3; ++puff)
        {
            float life = fmodf(s->time * 0.34f + (float)puff / 3.0f +
                                   (float)well * 0.21f,
                               1.0f);
            float px = wx + 22.0f +
                       sinf(s->time * 0.9f + (float)puff * 2.1f + (float)well) *
                           (3.0f + life * 5.0f);
            float py = counter - 2.0f - life * rise;
            float fade = life < 0.2f ? life / 0.2f : 1.0f - life;
            fx_glow(r, px, py, 6.0f + life * 10.0f, art->haze,
                    (Uint8)(26.0f * fade));
        }
    }
}

/* A pendant on its flex, swaying just enough to be noticed. The shade is a
 * cone, not a slab: lit rim, dark crown, the lamp showing under it. These are
 * further back than the fixtures in the ceiling itself, so they stay small
 * and throw a thin beam. */
static void canteen_pendant(const LevelArtScene *s, const LevelThemeArt *art,
                            float x, float top, float drop, float beam,
                            int pendant)
{
    SDL_Renderer *r = s->renderer;
    float sway = sinf(s->time * 0.7f +
                      (float)(art_hash(pendant, 131) % 628u) * 0.01f) *
                 fminf(4.0f, drop * 0.06f);
    float lx = x + sway;
    float ly = top + drop;
    fx_set(r, fx_mix(art->near_shape, FX_INK, 0.4f));
    SDL_RenderLine(r, x, top, lx, ly);
    SDL_Color shade = fx_mix(art->near_shape, art->wall_light, 0.3f);
    for (int row = 0; row < 8; ++row)
    {
        float half = 3.0f + (float)row;
        fx_rect(r, fx_mix(shade, FX_INK, 0.35f - (float)row * 0.03f),
                lx - half, ly + (float)row, half * 2.0f, 1.0f);
    }
    fx_rect(r, fx_mix(shade, art->trim_hi, 0.5f), lx - 10.0f, ly + 7.0f, 20.0f,
            1.0f);
    fx_rect(r, art->lamp, lx - 5.0f, ly + 8.0f, 10.0f, 2.0f);
    fx_glow(r, lx, ly + 9.0f, 34.0f, art->lamp, 40);
    if (beam > 8.0f)
        fx_light_cone(r, lx, ly + 8.0f, 9.0f, 36.0f, beam, art->lamp, 16);
}

/* A tiled column standing in the hall, lit from above and throwing its right
 * face and a shadow on the wall into shade. */
static void canteen_column(SDL_Renderer *r, const LevelThemeArt *art, float cx,
                           const ArtRoom *room, float band)
{
    const float top = room->top;
    const float h = room->height;
    SDL_Color column = fx_mix(art->near_shape, art->wall, 0.12f);
    fx_hgrad(r, cx + 26.0f, top, 18.0f, h, FX_INK, 60, FX_INK, 0);
    fx_rect(r, column, cx, top, 26.0f, h);
    fx_vgrad(r, cx, top, 26.0f, h * 0.6f, art->wall_light, 30, art->wall_light,
             0);
    fx_rect(r, fx_mix(column, art->wall_light, 0.28f), cx, top, 2.0f, h);
    fx_rect(r, fx_mix(column, FX_INK, 0.45f), cx + 20.0f, top, 6.0f, h);
    /* Joints on the building's own courses, so they meet the wall's. */
    for (float y = room->floor - 16.0f; y > top; y -= 16.0f)
        fx_rect_a(r, FX_INK, 60, cx, y, 20.0f, 1.0f);
    if (band > top + 4.0f)
    {
        fx_rect(r, fx_mix(column, art->accent, 0.3f), cx, band, 26.0f, 6.0f);
        fx_rect_a(r, art->trim_hi, 30, cx, band, 26.0f, 1.0f);
    }
    /* The plinth, a step proud of the shaft. */
    fx_rect(r, fx_mix(column, FX_INK, 0.3f), cx - 2.0f, room->floor - 14.0f,
            30.0f, 14.0f);
    fx_rect(r, fx_mix(column, art->wall_light, 0.2f), cx - 2.0f,
            room->floor - 14.0f, 30.0f, 1.0f);
}

/*
 * The dining hall: every layer of the servery at full size, plus what only a
 * hall has room for — the extract duct along its head that the canopies hang
 * from, the supply grilles over the hatches, and pendants on long flexes.
 */
static void canteen_hall(const LevelArtScene *s, const LevelThemeArt *art,
                         const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float top = room->top;
    const float floor = room->floor;
    const float w = room->clip_right - room->clip_left;
    const float counter = floor - 58.0f;
    const float sill = counter - 40.0f;
    const float hatch_top =
        sill - fminf(150.0f, fmaxf(60.0f, room->height * 0.36f));
    const float border = hatch_top - 40.0f;
    const float hood = counter - 118.0f;
    /* The duct needs the wall over the border course to itself; a hall only
     * just tall enough for the servery hangs its canopies off the soffit. */
    const bool has_duct = border - top > 60.0f;
    const float duct = has_duct ? top + 12.0f : top - 16.0f;

    /* The border course the tile material itself carries, in the amber the
     * galley puts on everything. */
    fx_rect_a(r, fx_mix(art->wall_dark, art->accent, 0.35f), 120,
              room->clip_left, border, w, 6.0f);
    fx_rect_a(r, art->trim_hi, 26, room->clip_left, border, w, 1.0f);
    fx_rect_a(r, FX_INK, 50, room->clip_left, border + 6.0f, w, 2.0f);

    /* The pass, and over each hatch where the wall is tall enough, the
     * supply grille that feeds the hall its air, with the grime the draught
     * has streaked down the tiles under it. */
    const float grille = (duct + 28.0f + border) * 0.5f - 8.0f;
    const bool grilles = has_duct && border - duct > 76.0f;
    int hatch = 0;
    for (float x = art_room_first(s, room, 0.12f, 250.0f, 200.0f, 1u, &hatch);
         x < room->clip_right; x += 250.0f, ++hatch)
    {
        canteen_hatch(r, art, x + 40.0f, hatch_top, sill, hatch, room->seed);
        if (!grilles)
            continue;
        float gx = x + 85.0f;
        SDL_Color rim = fx_mix(art->near_shape, art->wall_light, 0.12f);
        fx_vgrad(r, gx + 4.0f, grille + 16.0f, 52.0f, 22.0f, FX_INK, 40,
                 FX_INK, 0);
        fx_rect(r, rim, gx, grille, 60.0f, 16.0f);
        fx_rect(r, fx_mix(rim, art->wall_light, 0.25f), gx, grille, 60.0f,
                1.0f);
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.5f), gx + 2.0f,
                grille + 2.0f, 56.0f, 12.0f);
        for (float by = grille + 3.0f; by < grille + 13.0f; by += 3.0f)
            fx_rect(r, fx_mix(rim, FX_INK, 0.2f), gx + 2.0f, by, 56.0f, 1.0f);
    }

    int column = 0;
    for (float x = art_room_first(s, room, 0.19f, 280.0f, 70.0f, 2u, &column);
         x < room->clip_right; x += 280.0f, ++column)
        canteen_column(r, art, x + 20.0f, room, border);

    /* The extract duct along the head of the hall: one round run of spiral
     * duct on hangers, with the canopies below hung off it. It is on the
     * counters' plane, so a canopy's rods meet it rather than sliding past
     * it. */
    SDL_Color steel = fx_mix(art->near_shape, art->wall_light, 0.28f);
    if (has_duct)
    {
        fx_rect_a(r, FX_INK, 60, room->clip_left, duct + 18.0f, w, 4.0f);
        for (int row = 0; row < 16; ++row)
        {
            float lit = row < 4 ? 0.3f - (float)row * 0.06f : 0.0f;
            SDL_Color c =
                lit > 0.0f ? fx_mix(steel, art->wall_light, lit)
                           : fx_mix(steel, FX_INK, (float)(row - 4) * 0.035f);
            fx_rect(r, c, room->clip_left, duct + (float)row, w, 1.0f);
        }
        int seam = 0;
        for (float x = art_room_first(s, room, 0.26f, 40.0f, 4.0f, 3u, &seam);
             x < room->clip_right; x += 40.0f, ++seam)
        {
            fx_rect_a(r, FX_INK, 60, x, duct + 1.0f, 1.0f, 15.0f);
            fx_rect_a(r, art->wall_light, 30, x + 1.0f, duct + 1.0f, 1.0f,
                      15.0f);
            if ((seam % 3) == 0)
                fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.35f), x + 2.0f,
                        top, 1.0f, 12.0f);
        }
    }

    int bay = 0;
    for (float x = art_room_first(s, room, 0.26f, 240.0f, 210.0f, 4u, &bay);
         x < room->clip_right; x += 240.0f, ++bay)
    {
        /* The canopy hangs off the duct, or the soffit, on a pair of rods. */
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), x + 40.0f,
                duct + 16.0f, 2.0f, hood - duct - 16.0f);
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), x + 158.0f,
                duct + 16.0f, 2.0f, hood - duct - 16.0f);

        /* The canopy over the counter: a stainless hood with its filter
         * slats and a lit strip underneath. */
        for (int row = 0; row < 14; ++row)
        {
            float inset = 14.0f - (float)row;
            SDL_Color c = row < 2 ? fx_mix(steel, art->wall_light, 0.35f)
                                  : fx_mix(steel, FX_INK, (float)row * 0.018f);
            fx_rect(r, c, x + 8.0f + inset, hood + (float)row,
                    184.0f - inset * 2.0f, 1.0f);
        }
        for (float slot = x + 22.0f; slot < x + 180.0f; slot += 8.0f)
            fx_rect_a(r, FX_INK, 70, slot, hood + 4.0f, 4.0f, 7.0f);
        fx_rect(r, fx_mix(steel, FX_INK, 0.4f), x + 8.0f, hood + 14.0f, 184.0f,
                2.0f);
        fx_rect(r, fx_dim(art->lamp, 0.8f), x + 20.0f, hood + 16.0f, 160.0f,
                1.0f);
        fx_light_cone(r, x + 100.0f, hood + 17.0f, 80.0f, 104.0f,
                      counter - hood - 17.0f, art->lamp, 14);

        canteen_counter(s, art, x, counter, floor, 34.0f, 46.0f);
    }

    /* Pendants over the tables, on flexes long enough to be a hall's. */
    float drop = fminf(90.0f, room->height * 0.22f);
    int pendant = 0;
    for (float x = art_room_first(s, room, 0.34f, 130.0f, 90.0f, 5u, &pendant);
         x < room->clip_right + 40.0f; x += 130.0f, ++pendant)
        canteen_pendant(s, art, x + 41.0f, top, drop,
                        (counter - top - drop) * 0.8f, pendant);
}

/*
 * A low storey — three tiles on this floor — is the servery with the top
 * taken off. The hatches keep their menus and the counter keeps its wells and
 * its steam; the canopy, the duct and the pendants go, because there is
 * nowhere to hang them.
 */
static void canteen_servery(const LevelArtScene *s, const LevelThemeArt *art,
                            const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float top = room->top;
    const float floor = room->floor;
    const float counter = floor - 26.0f;
    /* The hatch keeps to the counter and to a hatch's height, and a taller
     * storey gets more wall over its menus rather than a taller hatch. */
    const float sill = floor - 30.0f;
    const float hatch_top =
        fminf(sill - 24.0f, fmaxf(top + 28.0f, sill - 70.0f));

    int hatch = 0;
    for (float x = art_room_first(s, room, 0.12f, 250.0f, 200.0f, 6u, &hatch);
         x < room->clip_right; x += 250.0f, ++hatch)
        canteen_hatch(r, art, x + 40.0f, hatch_top, sill, hatch, room->seed);

    int column = 0;
    for (float x = art_room_first(s, room, 0.19f, 280.0f, 70.0f, 7u, &column);
         x < room->clip_right; x += 280.0f, ++column)
        canteen_column(r, art, x + 20.0f, room, top + 6.0f);

    /* The counters, with no canopy over them: a low room has nowhere to
     * hang one, and the heat lamps along the sneeze guard are the light. */
    int bay = 0;
    for (float x = art_room_first(s, room, 0.26f, 240.0f, 210.0f, 8u, &bay);
         x < room->clip_right; x += 240.0f, ++bay)
    {
        canteen_counter(s, art, x, counter, floor, 22.0f, 24.0f);
    }
}

/*
 * A two-tile storey: the back of house. A stainless bench runs the length of
 * the wall on its cupboards, with a shelf of pans over it and a heat strip
 * under the shelf lighting the splashback, so the ladles hanging there read
 * against the one warm surface in the room. What stands on the bench belongs
 * to the bay it stands in.
 */
static void canteen_back_of_house(const LevelArtScene *s,
                                  const LevelThemeArt *art, const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float top = room->top;
    const float floor = room->floor;
    const float w = room->clip_right - room->clip_left;
    const float bench = floor - 24.0f;
    const float shelf = bench - fminf(22.0f, room->height * 0.34f);
    SDL_Color steel = fx_mix(art->near_shape, art->wall_light, 0.32f);
    SDL_Color sil = fx_mix(art->far_shape, art->near_shape, 0.5f);

    /* A stainless splashback behind the bench, lit by the strip over it. */
    fx_rect_a(r, fx_mix(steel, art->air_bottom, 0.55f), 110, room->clip_left,
              shelf + 2.0f, w, bench - shelf - 2.0f);
    fx_vgrad(r, room->clip_left, shelf + 3.0f, w, bench - shelf - 3.0f,
             art->lamp, 46, art->lamp, 8);

    /* The shelf, and the heat strip under it: the warmest line on the
     * storey. */
    fx_rect(r, steel, room->clip_left, shelf, w, 3.0f);
    fx_rect(r, fx_mix(steel, art->wall_light, 0.45f), room->clip_left, shelf,
            w, 1.0f);
    fx_rect(r, fx_dim(art->lamp, 0.9f), room->clip_left, shelf + 3.0f, w, 1.0f);
    fx_rect_a(r, FX_INK, 60, room->clip_left, shelf - 1.0f, w, 1.0f);

    /* Ladles and whisks off hooks under the shelf, dark against the lit
     * splashback. Which hook carries what is the hook's. */
    int hook = 0;
    for (float x = art_room_first(s, room, 0.2f, 17.0f, 6.0f, 9u, &hook);
         x < room->clip_right; x += 17.0f, ++hook)
    {
        unsigned th = art_hash(hook, 29 + (int)(room->seed & 255u));
        if ((th % 5u) < 2u)
            continue;
        float tl = fminf(5.0f + (float)(th >> 3 & 7u), bench - shelf - 10.0f);
        float tx = x + (float)(th >> 7 & 7u);
        fx_rect(r, sil, tx, shelf + 4.0f, 1.0f, tl);
        if ((th >> 10) & 1u)
            fx_rect(r, sil, tx - 2.0f, shelf + 4.0f + tl, 5.0f, 3.0f);
        else
            fx_rect(r, sil, tx - 1.0f, shelf + 4.0f + tl, 3.0f, 4.0f);
    }

    int bay = 0;
    for (float x = art_room_first(s, room, 0.2f, 190.0f, 190.0f, 10u, &bay);
         x < room->clip_right; x += 190.0f, ++bay)
    {
        unsigned bh = art_hash(bay, 977 + (int)(room->seed & 1023u));
        /* Brackets under the shelf. */
        fx_rect(r, fx_mix(steel, FX_INK, 0.35f), x + 20.0f, shelf + 3.0f,
                2.0f, 5.0f);
        fx_rect(r, fx_mix(steel, FX_INK, 0.35f), x + 115.0f, shelf + 3.0f,
                2.0f, 5.0f);
        /* What is on the shelf: stockpots and pans in steel, lidded tubs in
         * pale plastic, each lit along its rim. */
        float px = x + 6.0f;
        float room_over = shelf - top - 3.0f;
        for (int item = 0; item < 10 && px < x + 180.0f; ++item)
        {
            unsigned ih = art_hash(bay * 16 + item, 173);
            float iw = 7.0f + (float)(ih % 10u);
            float ihh = fminf(4.0f + (float)((ih >> 4) % 9u), room_over);
            unsigned kind = (ih >> 12) % 5u;
            if (kind != 0u && ihh > 2.0f)
            {
                SDL_Color c = kind == 1u
                                  ? fx_mix(art->wall, art->air_bottom, 0.35f)
                                  : fx_mix(steel, FX_INK, 0.12f);
                fx_rect(r, c, px, shelf - ihh, iw, ihh);
                fx_rect(r, fx_mix(c, art->wall_light, 0.4f), px, shelf - ihh,
                        iw, 1.0f);
                fx_rect(r, fx_mix(c, FX_INK, 0.4f), px + iw - 2.0f,
                        shelf - ihh + 1.0f, 2.0f, ihh - 1.0f);
                if (kind == 1u)
                    fx_rect_a(r, FX_INK, 60, px, shelf - ihh + 2.0f, iw, 1.0f);
            }
            px += iw + 2.0f + (float)((ih >> 9) % 6u);
        }

        /* On the bench: a stockpot with its steam, a stack of trays, or a
         * clear run of steel — which one is the bay's. */
        unsigned what = bh % 3u;
        if (what == 0u)
        {
            float pot = x + 40.0f + (float)(bh >> 4 & 63u);
            SDL_Color body = fx_mix(steel, FX_INK, 0.05f);
            fx_rect(r, body, pot, bench - 12.0f, 18.0f, 12.0f);
            fx_rect(r, fx_mix(body, art->wall_light, 0.35f), pot, bench - 12.0f,
                    18.0f, 1.0f);
            fx_rect(r, fx_mix(body, art->lamp, 0.2f), pot + 1.0f, bench - 11.0f,
                    2.0f, 10.0f);
            fx_rect(r, fx_mix(body, FX_INK, 0.45f), pot + 13.0f, bench - 11.0f,
                    5.0f, 11.0f);
            fx_rect(r, fx_mix(body, FX_INK, 0.3f), pot - 2.0f, bench - 9.0f,
                    2.0f, 2.0f);
            fx_rect(r, fx_mix(body, FX_INK, 0.3f), pot + 18.0f, bench - 9.0f,
                    2.0f, 2.0f);
            for (int puff = 0; puff < 2; ++puff)
            {
                float life = fmodf(s->time * 0.4f + (float)puff * 0.5f +
                                       (float)(bay & 7) * 0.13f,
                                   1.0f);
                float fade = life < 0.2f ? life / 0.2f : 1.0f - life;
                fx_glow(r, pot + 9.0f + sinf(s->time + (float)puff) * 3.0f,
                        bench - 14.0f - life * (bench - shelf - 4.0f),
                        4.0f + life * 6.0f, art->haze, (Uint8)(30.0f * fade));
            }
        }
        else if (what == 1u)
        {
            float tray = x + 120.0f - (float)(bh >> 4 & 31u);
            for (int t = 0; t < 4; ++t)
            {
                float ty = bench - 3.0f - (float)t * 3.0f;
                fx_rect(r, fx_mix(steel, FX_INK, 0.1f + (float)t * 0.05f),
                        tray + (float)(t & 1), ty, 26.0f, 2.0f);
                fx_rect(r, fx_mix(steel, art->wall_light, 0.4f),
                        tray + (float)(t & 1), ty, 26.0f, 1.0f);
            }
        }
    }

    /* The bench: stainless top on a run of cupboards, and the toe kick it
     * stands on. */
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.22f), room->clip_left,
            bench, w, floor - bench);
    fx_vgrad(r, room->clip_left, bench + 3.0f, w, floor - bench - 3.0f, FX_INK,
             0, FX_INK, 80);
    fx_rect(r, art->trim_hi, room->clip_left, bench, w, 2.0f);
    fx_rect_a(r, FX_INK, 90, room->clip_left, bench + 2.0f, w, 2.0f);
    int door = 0;
    for (float x = art_room_first(s, room, 0.2f, 38.0f, 38.0f, 11u, &door);
         x < room->clip_right; x += 38.0f, ++door)
    {
        fx_rect_a(r, FX_INK, 80, x, bench + 5.0f, 1.0f, floor - bench - 10.0f);
        fx_rect_a(r, art->wall_light, 26, x + 1.0f, bench + 5.0f, 1.0f,
                  floor - bench - 10.0f);
        float handle = (door & 1) ? x + 4.0f : x + 30.0f;
        fx_rect(r, fx_mix(steel, art->wall_light, 0.3f), handle, bench + 7.0f,
                4.0f, 1.0f);
    }
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.6f), room->clip_left,
            floor - 4.0f, w, 4.0f);
}

/* One room of the CANTEEN theme: see `ArtRoom` for what a room layout owes. */
static void backdrop_canteen_room(const LevelArtScene *s,
                                  const LevelThemeArt *art, const ArtRoom *room)
{
    canteen_tiles(s, art, room);
    if (room->height >= 256.0f)
        canteen_hall(s, art, room);
    else if (room->height >= 80.0f)
        canteen_servery(s, art, room);
    else
        canteen_back_of_house(s, art, room);
}

/*
 * LAB — the clean rooms, one storey at a time.
 *
 * A clean-room floor is a corridor of glazed partitions with the bays behind
 * them, and each storey is laid out to its own ceiling: a transom panel under
 * the head of the partition, the glass down to a kick rail on the floor, the
 * frosted band every partition carries at eye height so nobody walks into it,
 * and through the glass the bay's back wall with its racking and the reagents
 * on it. In front of the partitions the benches stand on the floor; a room
 * with the height for it gets a fume cupboard on its bench, and a low one
 * gets a glass-fronted cabinet on the wall over the bench instead, because a
 * cupboard squashed under a two-tile ceiling is a box rather than a hood.
 */

/* A streak of the strip light on a pane: slanted, which is what says glass
 * where a flat tint only says lighter. `slant` is how far the foot of the
 * streak sits left of its head. */
static void lab_sheen(SDL_Renderer *r, SDL_Color c, Uint8 alpha, float x,
                      float y, float w, float h, float slant)
{
    SDL_FColor fc = fx_fcolor(c, (float)alpha / 255.0f);
    SDL_Vertex v[4] = {{{x + slant, y}, fc, {0.0f, 0.0f}},
                       {{x + slant + w, y}, fc, {0.0f, 0.0f}},
                       {{x + w, y + h}, fc, {0.0f, 0.0f}},
                       {{x, y + h}, fc, {0.0f, 0.0f}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * The bay behind one pane: its back wall, the racking on it and the bottles
 * on the racking, drawn under a clip so nothing of it leaves the glass.
 *
 * It is a room further back than the partition, so it moves a little less
 * than the partition does when the camera climbs: the racking slides a few
 * pixels toward the middle of the frame, which is all it takes for the glass
 * to read as glass with a room behind it rather than as a picture of one.
 */
static void lab_bay(const LevelArtScene *s, const LevelThemeArt *art,
                    float gx, float gy, float gw, float gh, unsigned salt)
{
    SDL_Renderer *r = s->renderer;
    SDL_Rect saved;
    SDL_GetRenderClipRect(r, &saved);
    SDL_Rect pane = {(int)floorf(gx), (int)floorf(gy), (int)ceilf(gw),
                     (int)ceilf(gh)};
    SDL_Rect clip;
    if (!SDL_GetRectIntersection(&saved, &pane, &clip))
        return;
    SDL_SetRenderClipRect(r, &clip);

    float view_mid = (float)HUD_HEIGHT + ((float)s->win_h - HUD_HEIGHT) * 0.5f;
    float shift = fmaxf(-7.0f, fminf(7.0f, (view_mid - (gy + gh * 0.5f)) *
                                                0.035f));

    fx_rect(r, art->far_shape, gx, gy, gw, gh);
    /* The bay's own ceiling light, seen from under the transom. */
    fx_vgrad(r, gx, gy, gw, fminf(26.0f, gh * 0.4f), art->lamp, 26, art->lamp,
             0);

    SDL_Color stock = fx_mix(art->far_shape, art->near_shape, 0.8f);
    SDL_Color glass = fx_mix(stock, art->haze, 0.25f);
    const SDL_Color brown = {88, 66, 46, 255}; /* reagent glass */
    int shelves = (int)(gh / 24.0f);
    if (shelves < 2)
        shelves = 2;
    if (shelves > 8)
        shelves = 8;
    float pitch = (gh - 6.0f) / (float)shelves;
    for (int shelf = 0; shelf < shelves; ++shelf)
    {
        float sy = gy + 6.0f + pitch * (float)(shelf + 1) - 3.0f + shift;
        fx_rect(r, stock, gx + 6.0f, sy, gw - 12.0f, 2.0f);
        fx_rect(r, fx_mix(stock, art->haze, 0.2f), gx + 6.0f, sy, gw - 12.0f,
                1.0f);
        float bx = gx + 10.0f;
        for (int item = 0; item < 14 && bx < gx + gw - 14.0f; ++item)
        {
            unsigned bh = fx_hash(salt + (unsigned)(shelf * 14 + item) *
                                             0x2545f491u);
            float bw = 3.0f + (float)(bh % 4u);
            float tall = fminf(pitch - 4.0f, 5.0f + (float)((bh >> 3) % 9u));
            unsigned kind = (bh >> 12) % 6u;
            SDL_Color body = kind == 0u ? brown : (kind < 3u ? glass : stock);
            fx_rect(r, body, bx, sy - tall, bw, tall);
            fx_rect(r, fx_mix(body, art->haze, 0.35f), bx, sy - tall, 1.0f,
                    tall);
            /* A neck and a cap on the taller ones. */
            if (tall >= 8.0f && bw >= 4.0f)
                fx_rect(r, fx_mix(body, FX_INK, 0.3f), bx + 1.0f,
                        sy - tall - 2.0f, bw - 2.0f, 2.0f);
            if ((bh & 64u) != 0u && kind != 0u)
                fx_rect_a(r, art->accent, 60, bx, sy - tall * 0.45f, bw,
                          tall * 0.45f);
            bx += bw + 2.0f + (float)((bh >> 8) % 6u);
        }
    }
    SDL_SetRenderClipRect(r, &saved);
}

/*
 * One glazed partition, standing between the room's ceiling and its floor:
 * head frame, transom panel and rail, the bay behind the glass, the frosted
 * band at eye height, a kick rail on the floor, and a mullion down the
 * middle. One in three carries a door, with the access lamp beside it.
 */
static void lab_partition(const LevelArtScene *s, const LevelThemeArt *art,
                          const ArtRoom *room, float x, int bay)
{
    SDL_Renderer *r = s->renderer;
    unsigned h = fx_hash((unsigned)bay * 0x9e3779b9u ^ room->seed);
    const float w = 170.0f;
    /* A partition is a storey of glass, not a room of it: under a high
     * ceiling it stops at its own head height and a bulkhead carries on
     * up to the slab. */
    const float floor = room->floor;
    const float top = fmaxf(room->top + 3.0f, floor - 150.0f);
    const float height = floor - top;
    if (top > room->top + 3.0f)
    {
        fx_rect(r, fx_mix(art->far_shape, art->wall, 0.14f), x, room->top, w,
                top - room->top);
        fx_rect_a(r, FX_INK, 80, x, top - 3.0f, w, 3.0f);
    }
    const float transom = fmaxf(8.0f, height * 0.13f);
    const float kick = 5.0f;
    SDL_Color frame = fx_mix(art->far_shape, art->wall_light, 0.3f);
    SDL_Color frame_lit = fx_mix(art->far_shape, art->wall_light, 0.45f);

    /* The transom panel: solid, with the bay's colour code on it. */
    fx_rect(r, fx_mix(art->far_shape, art->wall, 0.22f), x, top, w, transom);
    fx_rect(r, fx_mix(art->far_shape, art->accent, 0.35f),
            x + 10.0f + (float)(h % 90u), top + transom * 0.5f - 1.0f, 18.0f,
            2.0f);

    float gx = x + 4.0f;
    float gy = top + transom + 2.0f;
    float gw = w - 8.0f;
    float gh = floor - kick - gy;
    lab_bay(s, art, gx, gy, gw, gh, h);

    /* The pane over the bay: a faint lift, and the strip light caught in it
     * as two slanted streaks. */
    fx_rect_a(r, art->trim_hi, 14, gx, gy, gw, gh);
    float streak = 18.0f + (float)((h >> 6) % 40u);
    float slant = fminf(14.0f, gh * 0.25f);
    lab_sheen(r, art->trim_hi, 26, gx + streak, gy, 14.0f, gh, slant);
    lab_sheen(r, art->trim_hi, 18, gx + streak + 20.0f, gy, 4.0f, gh, slant);
    lab_sheen(r, art->trim_hi, 20, gx + streak + 96.0f, gy, 9.0f, gh, slant);
    fx_rect_a(r, art->trim_hi, 40, gx, gy, gw, 1.0f);

    /* The frosted manifestation band, at the eye height of whoever might
     * walk into it, which is a fixed height above the floor rather than a
     * share of the room. */
    float band = floor - 32.0f;
    if (band > gy + 4.0f)
    {
        fx_rect_a(r, art->trim_hi, 46, gx, band, gw, 7.0f);
        for (float dot = gx + 4.0f; dot < gx + gw - 3.0f; dot += 6.0f)
            fx_rect_a(r, art->trim_hi, 60, dot, band + 3.0f, 2.0f, 1.0f);
    }

    /* Head frame, transom rail, kick rail. */
    fx_rect(r, frame_lit, x, top, w, 2.0f);
    fx_rect(r, frame, x, top + transom, w, 2.0f);
    fx_rect(r, fx_mix(frame, art->wall_light, 0.3f), x, top + transom, w,
            1.0f);
    fx_rect(r, fx_mix(art->far_shape, art->wall, 0.3f), x, floor - kick, w,
            kick);
    fx_rect(r, frame_lit, x, floor - kick, w, 1.0f);
    /* The jambs either end and the mullion down the middle. */
    fx_rect(r, frame, x, top, 4.0f, height);
    fx_rect(r, fx_mix(frame, FX_INK, 0.3f), x + w - 4.0f, top, 4.0f, height);
    fx_rect(r, frame, x + 82.0f, top, 4.0f, height);
    fx_rect_a(r, FX_INK, 60, x + 86.0f, top, 2.0f, height);

    if ((h % 3u) == 0u)
    {
        /* A glazed door in the right-hand pane: its own frame, a push bar
         * across it and the card reader beside it, lit green — the
         * palette's word for access granted. */
        float dx = x + 104.0f;
        float dw = 44.0f;
        fx_rect(r, frame, dx, gy, 2.0f, gh);
        fx_rect(r, frame, dx + dw, gy, 2.0f, gh);
        fx_rect(r, frame_lit, dx + 6.0f, floor - 26.0f, dw - 10.0f, 2.0f);
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.2f), dx + dw + 6.0f,
                floor - 30.0f, 5.0f, 8.0f);
        fx_rect(r, fx_dim(FX_GREEN, 0.85f), dx + dw + 7.0f, floor - 29.0f,
                3.0f, 2.0f);
    }
}

/*
 * The casework in front of the partitions: base units standing on the
 * floor, a resin worktop, and what is on it. The cupboard or cabinet over
 * the bench is the room's one strong light, and in this palette that is the
 * green the whole floor is known for.
 */
static void lab_bench(const LevelArtScene *s, const LevelThemeArt *art,
                      const ArtRoom *room, float x, int cell)
{
    SDL_Renderer *r = s->renderer;
    unsigned bay = fx_hash((unsigned)cell * 0x85ebca6bu + room->seed);
    const float floor = room->floor;
    const float bench_h = room->height >= 84.0f ? 17.0f : 15.0f;
    const float bench = floor - bench_h;
    SDL_Color unit = fx_mix(art->near_shape, art->wall, 0.18f);

    if (room->height >= 84.0f && (bay % 3u) != 0u)
    {
        /* Fume cupboard on the bench, its extract duct up into the
         * ceiling. It is a fixed size — a cupboard is built to a person, not
         * to the room — and a taller room shows more of the duct. */
        float hood_h = fminf(bench - room->top - 10.0f, 52.0f);
        float hood_top = bench - hood_h;
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), x + 52.0f,
                room->top, 16.0f, hood_top - room->top);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.18f), x + 64.0f,
                room->top, 4.0f, hood_top - room->top);
        fx_rect(r, art->near_shape, x + 14.0f, hood_top, 96.0f, hood_h);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.35f), x + 14.0f,
                hood_top, 96.0f, 2.0f);
        float gy = hood_top + 6.0f;
        float gh = hood_h - 10.0f;
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.45f), x + 20.0f, gy,
                84.0f, gh);
        fx_rect_a(r, art->accent, 70, x + 20.0f, gy, 84.0f, gh);
        fx_vgrad(r, x + 20.0f, gy, 84.0f, gh * 0.5f, art->lamp, 40, art->lamp,
                 0);
        /* Glassware standing inside it, in silhouette against the light. */
        for (int item = 0; item < 3; ++item)
        {
            float ih = 6.0f + (float)((bay >> (unsigned)(item * 3)) % 7u);
            fx_rect(r, fx_mix(art->near_shape, art->accent, 0.25f),
                    x + 32.0f + (float)item * 22.0f, gy + gh - ih, 5.0f, ih);
        }
        fx_glow(r, x + 62.0f, gy + gh * 0.5f, fminf(62.0f, hood_h), art->accent,
                44);
        /* The airflow monitor on the fascia: a readout and the lamp that
         * says the extract is running. */
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), x + 84.0f,
                hood_top + 2.0f, 16.0f, 3.0f);
        fx_rect(r, fx_dim(art->accent, 0.7f), x + 86.0f, hood_top + 3.0f,
                7.0f, 1.0f);
        fx_rect(r, fx_dim(FX_GREEN, 0.9f), x + 96.0f,
                hood_top + 3.0f, 2.0f, 1.0f);
        /* The sash, half open: its bar, and the glass over the top half. */
        float sash = gy + gh * 0.45f;
        fx_rect(r, art->trim_hi, x + 18.0f, sash, 88.0f, 3.0f);
        fx_rect_a(r, art->trim_hi, 22, x + 20.0f, gy, 84.0f, sash - gy);
        lab_sheen(r, art->trim_hi, 20, x + 30.0f, gy, 8.0f, sash - gy, 6.0f);
    }
    else
    {
        /* A glass-fronted cabinet on the wall over the bench, lit from
         * inside: all a low ceiling has room for, and the odd bench under a
         * high one that has no cupboard. */
        float cy = fmaxf(room->top + 5.0f, bench - 46.0f);
        float ch = fminf(22.0f, bench - cy - 18.0f);
        if (ch >= 10.0f)
        {
            fx_rect(r, art->near_shape, x + 14.0f, cy, 96.0f, ch);
            fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.35f),
                    x + 14.0f, cy, 96.0f, 1.0f);
            fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.45f), x + 17.0f,
                    cy + 3.0f, 90.0f, ch - 6.0f);
            fx_rect_a(r, art->accent, 64, x + 17.0f, cy + 3.0f, 90.0f,
                      ch - 6.0f);
            for (int item = 0; item < 9; ++item)
            {
                float ih = 4.0f + (float)((bay >> (unsigned)item) % 5u);
                fx_rect(r, fx_mix(art->near_shape, art->accent, 0.3f),
                        x + 21.0f + (float)item * 9.0f, cy + ch - 3.0f - ih,
                        4.0f, ih);
            }
            fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f),
                    x + 61.0f, cy + 3.0f, 2.0f, ch - 6.0f);
            fx_glow(r, x + 62.0f, cy + ch * 0.5f, 40.0f, art->accent, 34);
            fx_rect_a(r, FX_INK, 70, x + 16.0f, cy + ch, 96.0f, 3.0f);
        }
    }

    /* The base units and the worktop over them: drawer and door fronts,
     * and one bay left open as a knee space, which is what makes a run of
     * casework read as a bench somebody sits at rather than a plinth. */
    int knee = (int)((bay >> 20) % 5u);
    fx_rect(r, unit, x, bench, 200.0f, bench_h);
    fx_rect(r, fx_mix(unit, FX_INK, 0.6f), x + (float)knee * 40.0f + 1.0f,
            bench + 4.0f, 39.0f, bench_h - 4.0f);
    fx_vgrad(r, x, bench, 200.0f, bench_h, FX_INK, 0, FX_INK, 70);
    for (int door = 1; door < 5; ++door)
        fx_rect(r, fx_mix(unit, FX_INK, 0.45f), x + (float)door * 40.0f,
                bench + 5.0f, 1.0f, bench_h - 8.0f);
    for (int door = 0; door < 5; ++door)
    {
        if (door == knee)
            continue;
        fx_rect(r, fx_mix(unit, FX_INK, 0.3f), x + (float)door * 40.0f + 3.0f,
                bench + 7.0f, 34.0f, 1.0f);
        fx_rect(r, fx_mix(unit, art->wall_light, 0.3f),
                x + 16.0f + (float)door * 40.0f, bench + 5.0f, 8.0f, 1.0f);
    }
    fx_rect_a(r, FX_INK, 110, x + 3.0f, floor - 3.0f, 194.0f, 3.0f);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.5f), x - 2.0f, bench,
            204.0f, 4.0f);
    fx_rect(r, art->trim_hi, x - 2.0f, bench, 204.0f, 1.0f);
    fx_rect_a(r, FX_INK, 90, x, bench + 4.0f, 200.0f, 2.0f);

    /* Glassware on the worktop, and a centrifuge that is still running. */
    float room_over = bench - room->top;
    float flask_max = fminf(21.0f, room_over * 0.42f);
    for (int flask = 0; flask < 4; ++flask)
    {
        float fx_pos = x + 124.0f + (float)flask * 17.0f;
        float height = fminf(flask_max,
                             10.0f + (float)((bay >> (unsigned)flask) % 12u));
        fx_rect_a(r, art->trim_hi, 120, fx_pos, bench - height, 9.0f, height);
        fx_rect_a(r, art->accent, 150, fx_pos + 1.0f, bench - height * 0.4f,
                  7.0f, height * 0.4f);
        fx_rect_a(r, art->trim_hi, 170, fx_pos + 1.0f, bench - height, 1.0f,
                  height);
    }
    float spin = s->steady_lights ? 0.5f : fmodf(s->time * 3.0f, 1.0f);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.25f), x + 118.0f,
            bench - 16.0f, 26.0f, 16.0f);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.45f), x + 118.0f,
            bench - 16.0f, 26.0f, 1.0f);
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), x + 121.0f,
            bench - 12.0f, 20.0f, 6.0f);
    fx_rect(r, art->accent, x + 122.0f + spin * 16.0f, bench - 11.0f, 3.0f,
            4.0f);
}

static void backdrop_lab_room(const LevelArtScene *s, const LevelThemeArt *art,
                              const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float clip_w = room->clip_right - room->clip_left;

    /* The partitions, and the bays behind them. */
    {
        const float period = 190.0f;
        /* Each storey's run starts at a place of its own, so the same
         * furniture is not stacked floor over floor like a stamp. */
        const float cam =
            s->cam_x + (float)(fx_hash(room->seed ^ 0x3333u) % 190u) / 0.1f;
        float x = art_scroll(cam, 0.1f, period) - 20.0f;
        int idx = art_repeat(cam, 0.1f, period);
        while (x + period < room->clip_left)
        {
            x += period;
            ++idx;
        }
        for (; x < room->clip_right; x += period, ++idx)
        {
            lab_partition(s, art, room, x, idx);
            /* Between two partitions, the wall they are fixed to. */
            fx_rect(r, fx_mix(art->far_shape, art->wall, 0.12f), x + 170.0f,
                    room->top, 20.0f, room->height);
            fx_hgrad(r, x + 170.0f, room->top, 8.0f, room->height, FX_INK, 90,
                     FX_INK, 0);
        }
    }

    /* The benches on the floor. */
    {
        const float period = 216.0f;
        const float cam =
            s->cam_x + (float)(fx_hash(room->seed ^ 0x4444u) % 216u) / 0.24f;
        float x = art_scroll(cam, 0.24f, period);
        int idx = art_repeat(cam, 0.24f, period);
        while (x + 204.0f < room->clip_left)
        {
            x += period;
            ++idx;
        }
        for (; x < room->clip_right; x += period, ++idx)
            lab_bench(s, art, room, x, idx);
    }

    /* Continuous ceiling strip: clean rooms are lit edge to edge, and this
     * one is the far edge — slimmer and dimmer than the fittings the room
     * is lit by, which hang in front of it. */
    fx_rect(r, art->near_shape, room->clip_left, room->top, clip_w, 3.0f);
    fx_rect(r, fx_dim(art->lamp, 0.72f), room->clip_left, room->top + 1.0f,
            clip_w, 1.0f);
    fx_vgrad(r, room->clip_left, room->top + 3.0f, clip_w,
             fminf(40.0f, room->height * 0.4f), art->lamp, 20, art->lamp, 0);
}

/*
 * ARCHIVE — stacks running back into the dark under the old brick shell.
 *
 * Two ranks of shelving at different parallax do the whole job: the aisle
 * depth comes from the offset, not from detail — and from the dust between
 * them, which is why the far rank goes behind a veil of the room's own air
 * before the near one is drawn: at the same contrast the two read as one wall
 * of paper.
 *
 * Laid out per room: both ranks stand on the storey's floor and stop short of
 * its ceiling, and a shelf is always the same pitch — a taller storey gets
 * another shelf rather than taller boxes, because a box file is a fixed size
 * and a stack of them that grows with the room is a stack of something else.
 * What is left over under the soffit is where the brick shell shows.
 */


enum
{
    ARCHIVE_SHELF_PITCH = 26 /* one box file and the board it stands on */
};

/* How many shelves fit a unit standing on `base` under `ceiling`, and so
 * where its top is. */
static int archive_shelves(float base, float ceiling, float *top)
{
    const float plinth = 6.0f;
    const float cap = 4.0f;
    int shelves = (int)((base - plinth - cap - ceiling) /
                        (float)ARCHIVE_SHELF_PITCH);
    if (shelves < 1)
        shelves = 1;
    *top = base - plinth - cap - (float)(shelves * ARCHIVE_SHELF_PITCH);
    return shelves;
}

/*
 * One face of shelving: its uprights, cap and plinth, and a box file in every
 * slot. The key for a spine is the bay and the slot, so a box does not change
 * colour when the shelf scrolls, and the storey's seed is in it so the floor
 * above is not the same run of files.
 */
static void archive_stack(SDL_Renderer *r, const LevelThemeArt *art, float x,
                          float face, float base, float ceiling, SDL_Color body,
                          unsigned bay, bool near)
{
    float top;
    int shelves = archive_shelves(base, ceiling, &top);
    const float height = base - top;

    fx_rect(r, body, x, top, face, height);
    /* The cap, lit by the bulbs, and the plinth in shade. */
    fx_rect(r, fx_mix(body, art->wall_light, 0.3f), x, top, face, 2.0f);
    fx_rect(r, fx_mix(body, FX_INK, 0.5f), x, base - 6.0f, face, 6.0f);

    for (int shelf = 0; shelf < shelves; ++shelf)
    {
        float board = base - 6.0f - (float)(shelf * ARCHIVE_SHELF_PITCH);
        fx_rect(r, fx_mix(body, FX_INK, 0.4f), x, board - 3.0f, face, 3.0f);
        if (near)
            fx_rect(r, fx_mix(body, art->wall_light, 0.16f), x, board - 3.0f,
                    face, 1.0f);
        /* Box spines: a row of narrow blocks in slightly different papers,
         * which is all a shelf of files ever looks like. A slot now and then
         * is empty, and now and then two boxes lie flat instead. */
        int slot = 0;
        for (float box = x + 4.0f; box < x + face - 10.0f; box += 9.0f, ++slot)
        {
            unsigned bh = fx_hash(bay * 2654435761u + (unsigned)slot * 40503u +
                                  (unsigned)shelf * 977u);
            if ((bh % 13u) == 0u)
                continue;
            SDL_Color paper = fx_mix(art->far_shape, art->trim,
                                     0.14f + art_unit(bh, 3) * 0.26f);
            /* Not every file is the same buff board: a run of the old red
             * boxes here, a pale newer one there. */
            unsigned stock = (bh >> 20) % 7u;
            if (stock == 0u)
                paper = fx_mix(paper, art->wall, 0.45f);
            else if (stock == 1u)
                paper = fx_mix(paper, art->trim_hi, 0.22f);
            float tall = 20.0f - (float)((bh >> 11) % 4u);
            float spine_top = board - 3.0f - tall;
            if ((bh % 17u) == 1u && box + 16.0f < x + face - 4.0f)
            {
                /* Two lying flat, spine out, the stack of them squatter. */
                fx_rect(r, paper, box, board - 11.0f, 16.0f, 8.0f);
                fx_rect(r, fx_mix(paper, FX_INK, 0.35f), box, board - 7.0f,
                        16.0f, 1.0f);
                fx_rect(r, fx_mix(paper, art->trim_hi, 0.15f), box,
                        board - 19.0f, 16.0f, 7.0f);
                box += 9.0f;
                ++slot;
                continue;
            }
            fx_rect(r, paper, box, spine_top, 7.0f, tall);
            if (near)
            {
                /* The bulb catches the top of a spine and the hand hole
                 * sits in shade. */
                fx_rect(r, fx_mix(paper, art->trim_hi, 0.22f), box, spine_top,
                        7.0f, 1.0f);
                fx_rect(r, fx_mix(paper, FX_INK, 0.45f), box + 2.0f,
                        spine_top + 3.0f, 3.0f, 1.0f);
            }
            if ((bh % 6u) == 0u)
                fx_rect_a(r, art->trim_hi, near ? 60 : 45, box + 1.0f,
                          spine_top + 6.0f, 5.0f, 3.0f);
        }
    }

    /* The uprights at either end of the face. */
    SDL_Color post = fx_mix(body, FX_INK, 0.35f);
    fx_rect(r, post, x - 2.0f, top - 1.0f, 3.0f, height + 1.0f);
    fx_rect(r, post, x + face - 1.0f, top - 1.0f, 3.0f, height + 1.0f);
    if (near)
    {
        fx_rect(r, fx_mix(body, art->wall_light, 0.22f), x - 2.0f, top - 1.0f,
                1.0f, height + 1.0f);
        /* The placard on the stack end, with its range of files. */
        float py = base - 6.0f - (float)ARCHIVE_SHELF_PITCH * 2.0f + 6.0f;
        if (py > top + 4.0f)
        {
            SDL_Color card = fx_mix(art->trim_hi, art->far_shape, 0.45f);
            fx_rect(r, card, x + 2.0f, py, 8.0f, 9.0f);
            fx_rect(r, fx_mix(card, FX_INK, 0.5f), x + 3.0f, py + 2.0f, 6.0f,
                    1.0f);
            fx_rect(r, fx_mix(card, FX_INK, 0.5f), x + 3.0f, py + 5.0f, 4.0f,
                    1.0f);
        }
    }
}

/* The rolling library ladder some stacks carry, leaning on its rail. */
static void archive_ladder(SDL_Renderer *r, const LevelThemeArt *art, float x,
                           float base, float top)
{
    SDL_Color wood = fx_mix(art->trim, art->near_shape, 0.45f);
    SDL_Color lit = fx_mix(wood, art->trim_hi, 0.3f);
    const float lean = 16.0f;
    const float height = base - top;
    /* The rail along the top of the stack it rolls on. */
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.3f), x - 30.0f,
            top, 110.0f, 1.0f);
    for (int rail = 0; rail < 2; ++rail)
    {
        float rx = x + (float)rail * 10.0f;
        fx_set(r, rail == 0 ? lit : wood);
        SDL_RenderLine(r, rx + lean, top, rx, base);
        fx_set(r, wood);
        SDL_RenderLine(r, rx + lean + 1.0f, top, rx + 1.0f, base);
    }
    for (float t = 7.0f; t < height - 4.0f; t += 9.0f)
    {
        float rx = x + lean * (1.0f - t / height);
        fx_rect(r, wood, rx + 1.0f, top + t, 10.0f, 1.0f);
    }
    /* Its wheels on the floor. */
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), x - 1.0f, base - 3.0f,
            4.0f, 3.0f);
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), x + 9.0f, base - 3.0f,
            4.0f, 3.0f);
}

/* One room of the ARCHIVE theme: see `ArtRoom` for what a room layout owes. */
static void backdrop_archive_room(const LevelArtScene *s,
                                  const LevelThemeArt *art, const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float w = room->clip_right - room->clip_left;
    const float far_base = room->floor - 8.0f;
    const float near_base = room->floor - 2.0f;
    ArtBatch batch;
    batch.count = 0;

    /* The brick shell over the stacks: stretcher bond counted up from the
     * floor in world courses, so it meets the masonry at the room's own
     * edges. Only the band above the far rank is ever seen, so only that
     * is laid. */
    /* The far rank stops a shelf or so under the near one: it is further
     * off, and a stack seen down an aisle stands shorter than the one beside
     * the player, with the brick showing over it. */
    float near_top;
    archive_shelves(near_base, room->top + 8.0f, &near_top);
    const float far_ceiling = near_top + 4.0f;
    float far_top;
    archive_shelves(far_base, far_ceiling, &far_top);
    SDL_Color joint = fx_mix(art->air_bottom, art->wall_light, 0.16f);
    int brick0 = 0;
    float bx0 = art_room_first(s, room, 0.05f, 24.0f, 36.0f, 0u, &brick0);
    int course = room->floor_row * 4 - 1;
    for (float y = room->floor - 8.0f; y > room->top - 8.0f;
         y -= 8.0f, --course)
    {
        if (y > far_top + 8.0f || y + 8.0f < room->clip_top)
            continue;
        art_batch_add(r, &batch, joint, 44, room->clip_left, y, w, 1.0f);
        float offset = (course & 1) ? 12.0f : 0.0f;
        for (float x = bx0 + offset; x < room->clip_right; x += 24.0f)
            art_batch_add(r, &batch, joint, 44, x, y + 1.0f, 1.0f, 7.0f);
    }
    art_batch_flush(r, &batch, joint, 44);
    /* The soffit's own shadow, a beam's depth of it. */
    fx_vgrad(r, room->clip_left, room->top, w, 14.0f, FX_INK, 90, FX_INK, 0);

    /* The far rank, then the air between the two. */
    int stack = 0;
    for (float x = art_room_first(s, room, 0.09f, 118.0f, 96.0f, 1u, &stack);
         x < room->clip_right + 4.0f; x += 118.0f, ++stack)
    {
        unsigned bay = (unsigned)stack + room->seed * 31u;
        archive_stack(r, art, x, 92.0f, far_base, far_ceiling, art->far_shape,
                      bay, false);
    }
    fx_rect_a(r, art->air_bottom, 100, room->clip_left, room->top, w,
              room->height);

    /* The near rank, each face throwing its upright's shadow on the aisle
     * beside it. */
    for (float x = art_room_first(s, room, 0.2f, 156.0f, 150.0f, 2u, &stack);
         x < room->clip_right + 4.0f; x += 156.0f, ++stack)
    {
        unsigned bay = (unsigned)stack + 977u + room->seed * 31u;
        fx_hgrad(r, x + 130.0f, near_top, 16.0f, near_base - near_top, FX_INK,
                 80, FX_INK, 0);
        archive_stack(r, art, x, 130.0f, near_base, room->top + 8.0f,
                      art->near_shape, bay, true);
        if ((fx_hash(bay * 0x9e3779b9u) % 4u) == 0u)
            archive_ladder(r, art, x + 40.0f + (float)(bay % 40u), near_base,
                           near_top);
    }

    /* Bare bulbs on cords with heavy dust in the beam — the archive's whole
     * mood is one warm cone in a lot of brown dark. Further back than the
     * ceiling's own fittings, so smaller and on a shorter throw. */
    float cord = fminf(34.0f, fmaxf(14.0f, room->height * 0.16f));
    int bulb = 0;
    for (float x = art_room_first(s, room, 0.3f, 208.0f, 120.0f, 3u, &bulb);
         x < room->clip_right + 60.0f; x += 208.0f, ++bulb)
    {
        /* The swing belongs to the bulb, not to where it is on screen. */
        float sway = sinf(s->time * 0.5f +
                          (float)(art_hash(bulb, 211) % 628u) * 0.01f) *
                     2.0f;
        float bx = x + 60.0f + sway;
        float by = room->top + cord;
        fx_set(r, fx_mix(art->far_shape, FX_INK, 0.4f));
        SDL_RenderLine(r, x + 60.0f, room->top, bx, by);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.2f), bx - 2.0f,
                by, 5.0f, 2.0f);
        fx_rect(r, art->lamp, bx - 2.0f, by + 2.0f, 5.0f, 5.0f);
        fx_rect(r, fx_mix(art->lamp, FX_PALE, 0.5f), bx - 1.0f, by + 3.0f,
                2.0f, 2.0f);
        fx_glow(r, bx, by + 4.0f, 34.0f, art->lamp, 58);
        fx_light_cone(r, bx, by + 5.0f, 6.0f, 54.0f,
                      room->floor - by - 5.0f, art->lamp, 24);
    }
}

/*
 * One camera feed on the monitor wall.
 *
 * A feed used to be a flat panel with a lighter band across its foot, which is
 * a screen showing nothing. At thirty-two pixels a picture only needs its
 * perspective to read as one: the floor of a corridor running away to a door,
 * the diagonal of a stair, a room with a desk in it, the street under a lamp.
 * Which view a screen carries belongs to the screen.
 */
static void security_feed(SDL_Renderer *r, const LevelThemeArt *art,
                          float sx, float sy, unsigned h, float time)
{
    SDL_Color feed = fx_mix(art->far_shape, art->near_shape, 0.6f);
    SDL_Color lit = fx_mix(feed, art->trim, 0.5f);
    SDL_Color dark = fx_mix(feed, FX_INK, 0.6f);
    float px = sx + 2.0f;
    float py = sy + 2.0f;
    fx_rect(r, feed, px, py, 32.0f, 32.0f);
    switch ((h >> 4) % 4u)
    {
    case 0u:
        /* A corridor: the floor narrowing to a door at the far end. */
        for (int row = 0; row < 12; ++row)
        {
            float inset = 12.0f - (float)row;
            fx_rect(r, fx_mix(dark, lit, (float)row / 12.0f), px + inset,
                    py + 20.0f + (float)row, 32.0f - inset * 2.0f, 1.0f);
        }
        fx_rect(r, dark, px + 13.0f, py + 10.0f, 6.0f, 10.0f);
        fx_rect(r, lit, px + 15.0f, py + 3.0f, 2.0f, 1.0f);
        break;
    case 1u:
        /* A stairwell, seen from the landing. */
        for (int step = 0; step < 6; ++step)
            fx_rect(r, fx_mix(dark, lit, 0.3f + (float)step * 0.1f),
                    px + (float)step * 5.0f, py + 28.0f - (float)step * 4.0f,
                    32.0f - (float)step * 5.0f, 4.0f);
        fx_rect(r, dark, px + 2.0f, py + 2.0f, 1.0f, 26.0f);
        break;
    case 2u:
        /* An office: a desk and a chair under a ceiling light. */
        fx_rect(r, fx_mix(feed, lit, 0.4f), px, py + 24.0f, 32.0f, 8.0f);
        fx_rect(r, dark, px + 6.0f, py + 16.0f, 16.0f, 3.0f);
        fx_rect(r, dark, px + 7.0f, py + 19.0f, 2.0f, 6.0f);
        fx_rect(r, dark, px + 19.0f, py + 19.0f, 2.0f, 6.0f);
        fx_rect(r, lit, px + 10.0f, py + 12.0f, 7.0f, 4.0f);
        fx_rect(r, dark, px + 24.0f, py + 14.0f, 5.0f, 11.0f);
        break;
    default:
        /* The street outside the loading dock, under one lamp. */
        fx_rect(r, dark, px, py, 32.0f, 20.0f);
        fx_rect(r, fx_mix(dark, lit, 0.5f), px, py + 20.0f, 32.0f, 12.0f);
        fx_rect(r, fx_mix(feed, FX_INK, 0.2f), px + 22.0f, py + 6.0f, 1.0f,
                14.0f);
        fx_rect(r, lit, px + 20.0f, py + 5.0f, 5.0f, 2.0f);
        fx_rect_a(r, art->trim, 60, px + 16.0f, py + 20.0f, 13.0f, 6.0f);
        break;
    }
    /* Something crosses one feed now and then. */
    float walker = fmodf(time * 0.4f + art_unit(h, 4) * 6.0f, 6.0f);
    if (walker < 1.0f)
        fx_rect(r, fx_mix(feed, FX_INK, 0.7f), px + 2.0f + walker * 26.0f,
                py + 16.0f, 4.0f, 10.0f);
    /* The caption burned into the corner of every feed. */
    fx_rect_a(r, art->haze, 90, px + 2.0f, py + 2.0f, 8.0f + (float)(h % 6u),
              1.0f);
}

/*
 * One screen of the monitor wall: its bezel, and either a feed or the snow
 * of a camera that has stopped answering — the building watches itself
 * badly, so a few of them are dead.
 */
static void security_screen(const LevelArtScene *s, const LevelThemeArt *art,
                            float sx, float sy, unsigned h, int index)
{
    SDL_Renderer *r = s->renderer;
    fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.6f), sx, sy, 36.0f, 36.0f);
    fx_rect(r, fx_mix(art->far_shape, art->wall_light, 0.2f), sx, sy, 36.0f,
            1.0f);
    if ((h % 9u) == 0u)
    {
        /* Static: a handful of scan rows at random brightness. Held on one
         * frame of it when the lights are held steady — snow at twelve
         * changes a second is a strobe. */
        unsigned tick = s->steady_lights ? 0u : fx_salt(s->time * 12.0f);
        for (int line = 0; line < 6; ++line)
        {
            unsigned lh = fx_hash(h + (unsigned)line + tick);
            fx_rect_a(r, art->haze, (Uint8)(30u + lh % 90u), sx + 2.0f,
                      sy + 3.0f + (float)line * 5.0f, 32.0f, 3.0f);
        }
        return;
    }
    security_feed(r, art, sx, sy, h, s->time);
    fx_rect_a(r, art->haze, 22, sx + 2.0f,
              sy + 2.0f +
                  fmodf(s->time * 26.0f + (float)index * 9.0f, 32.0f),
              32.0f, 2.0f);
    /* The glass over the tube, catching the room. */
    fx_rect_a(r, art->haze, 14, sx + 4.0f, sy + 3.0f, 5.0f, 30.0f);
}

/*
 * A status board: the building's plan drawn in its own lines, a scan bar
 * going over it and the one marker nobody on shift has looked at. It is
 * whatever size the wall gives it — a strip under a low ceiling, a proper
 * board in a tall room — and what is drawn on it scales with the plate.
 */
static void security_plan(const LevelArtScene *s, const LevelThemeArt *art,
                          float bx, float by, float bw, float bh, unsigned h)
{
    SDL_Renderer *r = s->renderer;
    SDL_Color line = fx_mix(art->far_shape, art->accent, 0.35f);
    fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), bx - 4.0f, by - 4.0f,
            bw + 8.0f, bh + 8.0f);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.25f), bx - 4.0f,
            by - 4.0f, bw + 8.0f, 1.0f);
    fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.5f), bx, by, bw, bh);
    float m = bh >= 60.0f ? 10.0f : 5.0f;
    float ix = bx + m;
    float iy = by + m;
    float iw = bw - m * 2.0f;
    float ih = bh - m * 2.0f;
    fx_rect_a(r, line, 180, ix, iy, iw, 1.0f);
    fx_rect_a(r, line, 180, ix, iy + ih, iw, 1.0f);
    fx_rect_a(r, line, 180, ix, iy, 1.0f, ih);
    fx_rect_a(r, line, 180, ix + iw - 1.0f, iy, 1.0f, ih);
    for (int wall_i = 0; wall_i < 5; ++wall_i)
    {
        unsigned wh = fx_hash(h + (unsigned)wall_i * 1559u);
        float wxp = ix + 8.0f + (float)fx_spread(wh, iw - 16.0f);
        fx_rect_a(r, line, 130, wxp, iy, 1.0f,
                  ih * (0.3f + art_unit(wh, 8) * 0.4f));
        fx_rect_a(r, line, 130, ix, iy + ih * (0.3f + art_unit(wh, 4) * 0.4f),
                  iw * (0.15f + art_unit(wh, 12) * 0.25f), 1.0f);
    }
    float scan = fmodf(s->time * 0.35f + art_unit(h, 3), 1.0f);
    fx_hgrad(r, bx + scan * (bw - 20.0f), by + 1.0f, 20.0f, bh - 2.0f,
             art->accent, 0, art->accent, 26);
    float mark = art_pulse(s, fmodf(s->time * 1.2f + art_unit(h, 9), 1.0f) <
                                      0.5f
                                  ? 1.0f
                                  : 0.3f,
                           0.65f);
    float mx = ix + 6.0f + (float)fx_spread(h, iw - 14.0f);
    float my = iy + 3.0f + (float)fx_spread(h >> 8, ih - 8.0f);
    fx_rect(r, fx_dim(art->accent, 0.9f * mark), mx, my, 3.0f, 3.0f);
    fx_glow(r, mx + 1.0f, my + 1.0f, 8.0f, art->accent, (Uint8)(50.0f * mark));
    fx_glow(r, bx + bw * 0.5f, by + bh * 0.5f, fminf(90.0f, bw * 0.6f),
            art->accent, 10);
    fx_rect_a(r, art->haze, 10, bx + 12.0f, by, 18.0f, bh);
}

/*
 * One bay of the monitor wall, `rows` screens high: a steel housing
 * standing behind the consoles, three screens across. Every fourth bay or
 * so is a status board instead, on the same housing, so the wall is one
 * piece of furniture rather than two layers sliding over each other.
 */
static void security_bay(const LevelArtScene *s, const LevelThemeArt *art,
                         float x, float top, int rows, unsigned bay,
                         bool board)
{
    SDL_Renderer *r = s->renderer;
    float h = 6.0f + 42.0f * (float)rows;
    fx_rect(r, art->far_shape, x, top, 132.0f, h);
    fx_rect(r, fx_mix(art->far_shape, art->wall_light, 0.25f), x, top, 132.0f,
            1.0f);
    fx_rect_a(r, FX_INK, 90, x + 2.0f, top + h, 132.0f, 4.0f);
    if (board)
    {
        security_plan(s, art, x + 8.0f, top + 7.0f, 116.0f, h - 14.0f,
                      fx_hash(bay ^ 1553u));
        return;
    }
    for (int screen = 0; screen < 3 * rows; ++screen)
    {
        float sx = x + 6.0f + (float)(screen % 3) * 42.0f;
        float sy = top + 6.0f + (float)(screen / 3) * 42.0f;
        security_screen(s, art, sx, sy, fx_hash(bay * 31u + (unsigned)screen),
                        screen);
    }
    fx_glow(r, x + 66.0f, top + h * 0.5f, fminf(96.0f, h * 0.9f),
            fx_mix(art->near_shape, art->haze, 0.5f), 26);
}

/* One console desk on the floor: its own two monitors turned to the chairs,
 * the key banks along its lip, and the dark under the worktop. */
static void security_desk(const LevelArtScene *s, const LevelThemeArt *art,
                          float x, float floor, float desk_h, unsigned h)
{
    SDL_Renderer *r = s->renderer;
    float desk = floor - desk_h;
    for (int mon = 0; mon < 2; ++mon)
    {
        float mx = x + 30.0f + (float)mon * 70.0f;
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), mx, desk - 17.0f,
                30.0f, 15.0f);
        fx_rect(r, fx_mix(art->far_shape, art->haze, 0.18f), mx + 2.0f,
                desk - 15.0f, 26.0f, 11.0f);
        /* What is on it: a few lines of the log, one of them highlighted. */
        for (int ln = 0; ln < 3; ++ln)
        {
            unsigned lh = fx_hash(h + (unsigned)(mon * 8 + ln) * 71u);
            fx_rect_a(r, art->haze, (ln == (int)(h % 3u)) ? 70 : 34,
                      mx + 4.0f, desk - 13.0f + (float)ln * 3.0f,
                      8.0f + (float)(lh % 14u), 1.0f);
        }
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), mx + 13.0f,
                desk - 2.0f, 4.0f, 2.0f);
        fx_glow(r, mx + 15.0f, desk - 9.0f, 22.0f, art->haze, 14);
    }
    /* The desk: a worktop lit along its lip by the screens over it, the
     * sloped key bank under that, and a fascia going dark to the floor. */
    SDL_Color body = fx_mix(art->near_shape, art->wall, 0.22f);
    fx_rect(r, body, x, desk, 160.0f, desk_h);
    fx_rect(r, fx_mix(art->near_shape, art->trim, 0.45f), x, desk, 160.0f,
            2.0f);
    fx_rect(r, fx_mix(art->near_shape, art->trim_hi, 0.45f), x, desk, 160.0f,
            1.0f);
    fx_rect(r, fx_mix(body, art->wall_light, 0.15f), x, desk + 2.0f, 160.0f,
            1.0f);
    /* The screens' own light, lying along the worktop under them. */
    for (int mon = 0; mon < 2; ++mon)
        fx_rect_a(r, art->haze, 40, x + 30.0f + (float)mon * 70.0f, desk,
                  30.0f, 1.0f);
    fx_rect(r, fx_mix(body, FX_INK, 0.35f), x + 4.0f, desk + 4.0f, 152.0f,
            6.0f);
    fx_vgrad(r, x, desk + 10.0f, 160.0f, desk_h - 10.0f, FX_INK, 20, FX_INK,
             110);
    fx_rect_a(r, FX_INK, 70, x + 80.0f, desk + 10.0f, 1.0f, desk_h - 10.0f);
    /* The kick recess under the worktop, where the chairs go. */
    fx_rect_a(r, FX_INK, 90, x + 8.0f, floor - 4.0f, 144.0f, 4.0f);
    for (int key = 0; key < 12; ++key)
    {
        unsigned kh = fx_hash(h + (unsigned)key * 37u);
        bool hot = (kh % 7u) == 0u;
        fx_rect(r, hot ? fx_dim(art->accent, 0.8f)
                       : fx_mix(body, art->haze, 0.25f),
                x + 10.0f + (float)key * 12.0f, desk + 6.0f, 7.0f, 2.0f);
    }
}

/*
 * SECURITY — the control wing, one room at a time.
 *
 * Every room here is the same kind of room, laid out for its own height:
 * console desks on the floor, the monitor wall standing behind them with as
 * many rows of screens as the ceiling allows — one short row under a low
 * ceiling, three in a double-height hall, a status board in place of every
 * fourth bay — and the standby beacon turning under the ceiling. A room
 * with height to spare over its wall carries the annunciator strip there;
 * a tall one takes a second tier instead: the status boards hung from the
 * ceiling on rods, over a run of trunking that says where one tier ends and
 * the other begins.
 */
static void backdrop_security_room(const LevelArtScene *s,
                                   const LevelThemeArt *art,
                                   const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float clip_w = room->clip_right - room->clip_left;
    const float desk_h = room->height >= 110.0f ? 26.0f : 22.0f;
    /* The desk monitors top out 17 px over the worktop; the wall may stand
     * a little lower than that, behind them. */
    const float wall_limit = room->floor - desk_h - 17.0f + 12.0f;
    const bool two_tier = room->height >= 260.0f;

    int rows;
    float wall_top;
    if (two_tier)
    {
        rows = 3;
        wall_top = wall_limit - (6.0f + 42.0f * 3.0f);
    }
    else
    {
        /* Fewer than one row's worth of wall and there is no wall: a
         * monitor squashed into a crawlspace is not a monitor wall. */
        float avail = wall_limit - (room->top + 6.0f);
        rows = (int)((avail - 6.0f) / 42.0f);
        if (rows > 3)
            rows = 3;
        if (rows < 0)
            rows = 0;
        float spare = avail - (6.0f + 42.0f * (float)rows);
        wall_top = room->top + 6.0f + fmaxf(0.0f, spare) *
                                          (spare >= 24.0f ? 1.0f : 0.5f);
    }
    if (two_tier)
    {
        /* The upper tier: status boards hung on rods from the ceiling, and
         * a run of trunking under them where the tier ends. */
        float board_top = room->top + 24.0f;
        float board_h = fminf(92.0f, wall_top - 30.0f - board_top);
        const float period = 230.0f;
        /* Each storey's run starts at a place of its own, so the same
         * furniture is not stacked floor over floor like a stamp. */
        const float cam =
            s->cam_x + (float)(fx_hash(room->seed ^ 0x5555u) % 230u) / 0.2f;
        float x = art_scroll(cam, 0.2f, period) - 40.0f;
        int idx = art_repeat(cam, 0.2f, period);
        while (x + 190.0f < room->clip_left)
        {
            x += period;
            ++idx;
        }
        for (; x < room->clip_right; x += period, ++idx)
        {
            unsigned h = fx_hash(room->seed ^ (unsigned)idx * 0x5bd1e995u);
            SDL_Color rod = fx_mix(art->far_shape, art->wall_light, 0.18f);
            fx_rect(r, rod, x + 50.0f, room->top, 1.0f, board_top - room->top);
            fx_rect(r, rod, x + 160.0f, room->top, 1.0f,
                    board_top - room->top);
            security_plan(s, art, x + 30.0f, board_top + 4.0f, 150.0f,
                          board_h - 8.0f, h);
            /* A lamp over each board on a bracket, washing its face — the
             * only light on this tier that is not a screen. */
            for (int lamp = 0; lamp < 2; ++lamp)
            {
                float lx = x + 70.0f + (float)lamp * 70.0f;
                fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.25f),
                        lx - 4.0f, board_top - 6.0f, 8.0f, 3.0f);
                fx_rect(r, fx_dim(art->lamp, 0.7f), lx - 3.0f,
                        board_top - 3.0f, 6.0f, 1.0f);
                fx_light_cone(r, lx, board_top - 2.0f, 3.0f, 22.0f,
                              fminf(60.0f, board_h), art->lamp, 18);
            }
        }
        float duct_y = wall_top - 22.0f;
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.2f), room->clip_left,
                duct_y, clip_w, 8.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.25f),
                room->clip_left, duct_y, clip_w, 1.0f);
        fx_rect_a(r, FX_INK, 70, room->clip_left, duct_y + 8.0f, clip_w, 3.0f);
        const float joint = 64.0f;
        for (float jx = art_scroll(s->cam_x, 0.18f, joint);
             jx < room->clip_right; jx += joint)
            if (jx + 2.0f >= room->clip_left)
                fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.5f), jx,
                        duct_y + 1.0f, 2.0f, 7.0f);
    }
    else if (wall_top - room->top >= 24.0f)
    {
        /* Room to spare over the wall: an annunciator strip along it, one
         * lamp per zone of the building, the tripped one never reset. */
        float ay = room->top + 6.0f;
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), room->clip_left, ay,
                clip_w, 10.0f);
        fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.2f),
                room->clip_left, ay, clip_w, 1.0f);
        const float period = 18.0f;
        float x = art_scroll(s->cam_x, 0.16f, period);
        int idx = art_repeat(s->cam_x, 0.16f, period);
        while (x + 12.0f < room->clip_left)
        {
            x += period;
            ++idx;
        }
        for (; x < room->clip_right; x += period, ++idx)
        {
            unsigned h = fx_hash(room->seed + (unsigned)idx * 0x27d4eb2du);
            bool tripped = (h % 11u) == 0u;
            float on = tripped ? art_pulse(s, fmodf(s->time * 1.5f +
                                                        art_unit(h, 5),
                                                    1.0f) < 0.5f
                                                  ? 1.0f
                                                  : 0.35f,
                                           0.7f)
                               : 0.3f;
            fx_rect(r, fx_dim(tripped ? art->accent : art->trim, on),
                    x + 2.0f, ay + 3.0f, 12.0f, 4.0f);
        }
    }

    /* The monitor wall. */
    {
        const float period = 150.0f;
        const float cam =
            s->cam_x + (float)(fx_hash(room->seed ^ 0x6666u) % 150u) / 0.16f;
        float x = art_scroll(cam, 0.16f, period) - 20.0f;
        int idx = art_repeat(cam, 0.16f, period);
        while (x + 150.0f < room->clip_left)
        {
            x += period;
            ++idx;
        }
        for (; x < room->clip_right && rows > 0; x += period, ++idx)
        {
            unsigned bay = fx_hash(room->seed + (unsigned)idx * 0x9e3779b9u);
            security_bay(s, art, x, wall_top, rows, bay,
                         ((unsigned)idx + room->seed) % 4u == 0u);
            /* In the gap after every few bays, the standby beacon that never
             * quite stops turning, hung from the ceiling on a short stalk. */
            if (((unsigned)idx & 3u) != 1u)
                continue;
            float bx = x + 141.0f;
            float by = room->top + 3.0f;
            float turn = s->time * 2.4f + art_unit(bay, 6) * 6.0f;
            float beam = art_pulse(s, 0.5f + 0.5f * cosf(turn), 0.5f);
            fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.2f),
                    bx - 0.5f, room->top, 1.0f, 3.0f);
            fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.2f),
                    bx - 4.0f, by, 8.0f, 2.0f);
            fx_rect(r, fx_dim(art->accent, 0.45f + 0.55f * beam), bx - 3.0f,
                    by + 2.0f, 6.0f, 3.0f);
            float sweep = s->steady_lights ? 0.0f : sinf(turn) * 46.0f;
            fx_glow(r, bx + sweep, by + 4.0f, 34.0f, art->accent,
                    (Uint8)(20.0f + 40.0f * beam));
        }
    }

    /* The consoles on the floor. */
    {
        const float period = 190.0f;
        const float cam =
            s->cam_x + (float)(fx_hash(room->seed ^ 0x7777u) % 190u) / 0.3f;
        float x = art_scroll(cam, 0.3f, period);
        int idx = art_repeat(cam, 0.3f, period);
        while (x + 160.0f < room->clip_left)
        {
            x += period;
            ++idx;
        }
        for (; x < room->clip_right; x += period, ++idx)
            security_desk(s, art, x, room->floor, desk_h,
                          fx_hash(room->seed ^ (unsigned)idx * 0x85ebca6bu));
    }
}

/*
 * DUCTS — the plenum between floors.
 *
 * The darkest sector in the game has to stay dark. What a void needs in order
 * to read as deep rather than empty is several planes a step apart in value —
 * the building's own steel far back in the murk, the trunking and the pipe
 * runs in front of it, the hangers closest of all — and a few small lights to
 * measure the distance by. None of them is bright; the separation is the whole
 * of the effect.
 *
 * And every storey of it is two tiles high, which is the other half of the
 * reading. This used to be a picture of a tall void — three runs of trunking
 * stacked up a screen, a fan seventy pixels across — and squashed into a
 * sixty-four pixel crawl space that is a stripe of everything at once. A
 * service void is *low*: the underside of the slab above is close overhead,
 * its beams coming end-on toward the viewer, one run of trunking hung just
 * under it on short rods, a pipe under that, another run on stools along the
 * floor, and an inline fan box squeezed in between slab and floor. Everything
 * here is sized off the room it is in, so the void reads as a void a man has
 * to stoop in rather than a hall seen through a letterbox.
 */


/*
 * An inline fan box squeezed into the void: a square casing between the two
 * trunking stubs it serves, the round shroud, blades turning behind a guard,
 * and the damper actuator with its status lamp. It is the sector's only moving
 * light — a lamp behind the blades throws them into silhouette — and the
 * reminder that the air in here is going somewhere.
 */
static void ducts_fan(const LevelArtScene *s, const LevelThemeArt *art,
                      float cx, float cy, float size, unsigned h)
{
    SDL_Renderer *r = s->renderer;
    float half = floorf(size * 0.5f);
    SDL_Color casing = fx_mix(art->near_shape, art->wall, 0.38f);
    SDL_Color inner = fx_mix(art->far_shape, FX_INK, 0.30f);
    fx_rect(r, casing, cx - half, cy - half, size, size);
    fx_rect(r, fx_mix(casing, art->wall_light, 0.40f), cx - half, cy - half,
            size, 1.0f);
    fx_rect(r, fx_mix(casing, art->wall_light, 0.18f), cx - half,
            cy - half + 1.0f, 1.0f, size - 1.0f);
    fx_rect(r, fx_mix(casing, FX_INK, 0.55f), cx - half, cy + half - 1.0f,
            size, 1.0f);
    fx_rect_a(r, FX_INK, 90, cx + half, cy - half + 2.0f, 3.0f, size - 2.0f);
    /* Corner bolts on the casing face. */
    for (int b = 0; b < 4; ++b)
        fx_rect(r, fx_mix(casing, art->trim_hi, 0.45f),
                cx - half + 2.0f + (float)(b & 1) * (size - 5.0f),
                cy - half + 2.0f + (float)(b >> 1) * (size - 5.0f), 1.0f, 1.0f);

    /* The shroud: a lit rim round the throat, and the lamp behind the fan
     * that throws its blades into silhouette. */
    float ring = half - 4.0f;
    fx_mass(r, fx_mix(casing, art->wall_light, 0.20f), cx - ring - 2.0f,
            cy - ring - 2.0f, ring * 2.0f + 4.0f, ring * 2.0f + 4.0f, 6, 6);
    fx_mass(r, inner, cx - ring, cy - ring, ring * 2.0f, ring * 2.0f, 5, 5);
    fx_glow(r, cx, cy, ring + 2.0f, art->lamp, 58);
    fx_glow(r, cx, cy, ring * 0.55f, art->lamp, 60);
    /* Four blades, each a backlit wedge with its leading edge catching the
     * light, turning on the render clock. */
    float spin = s->time * 1.1f + (float)(h % 628u) * 0.01f;
    SDL_Color blade = fx_mix(art->far_shape, art->near_shape, 0.5f);
    SDL_Color edge = fx_mix(art->wall, art->wall_light, 0.45f);
    for (int k = 0; k < 4; ++k)
    {
        float angle = spin + (float)k * 1.5707963f;
        float ux = cosf(angle);
        float uy = sinf(angle);
        float reach = ring - 2.0f;
        fx_set(r, blade);
        for (int w = -3; w <= 2; ++w)
        {
            float o = (float)w;
            SDL_RenderLine(r, cx - uy * o * 0.3f, cy + ux * o * 0.3f,
                           cx + ux * reach - uy * o, cy + uy * reach + ux * o);
        }
        fx_set(r, edge);
        SDL_RenderLine(r, cx - uy, cy + ux, cx + ux * reach - uy * 3.0f,
                       cy + uy * reach + ux * 3.0f);
    }
    fx_mass(r, fx_mix(art->wall, art->wall_light, 0.30f), cx - 3.0f,
            cy - 3.0f, 7.0f, 7.0f, 2, 2);
    fx_rect(r, fx_mix(art->wall, art->wall_light, 0.60f), cx - 1.0f,
            cy - 3.0f, 3.0f, 1.0f);
    /* The guard: two bars across the throat, faint enough that the blades
     * still read through it. */
    fx_rect_a(r, fx_mix(art->far_shape, FX_INK, 0.2f), 170, cx - ring, cy,
              ring * 2.0f, 1.0f);
    fx_rect_a(r, fx_mix(art->far_shape, FX_INK, 0.2f), 170, cx, cy - ring,
              1.0f, ring * 2.0f);

    /* The damper actuator bolted to the casing, and its status lamp. */
    fx_rect(r, fx_mix(art->near_shape, art->wall, 0.48f), cx + half,
            cy - half + 6.0f, 7.0f, 11.0f);
    fx_rect(r, fx_mix(art->near_shape, art->wall_light, 0.25f), cx + half,
            cy - half + 6.0f, 7.0f, 1.0f);
    float blink = art_pulse(s,
                            fmodf(s->time * 0.8f + (float)(h % 50u) * 0.02f,
                                  1.0f) < 0.5f
                                ? 1.0f
                                : 0.3f,
                            0.65f);
    SDL_Color status = (h & 1u) ? FX_GREEN : FX_AMBER;
    fx_rect(r, fx_dim(status, 0.7f * blink), cx + half + 2.0f,
            cy - half + 9.0f, 2.0f, 2.0f);
}

/*
 * A caged bulkhead lamp screwed to the steel: the bulb in its guard wires,
 * the small pool of light it makes and the patch it lays on the floor of the
 * void. A tired one fails for a beat now and then. These are the brightest
 * thing in the plenum and they are still dim, because this is the one floor
 * with no fixtures of its own.
 */
static void ducts_caged_lamp(const LevelArtScene *s, const LevelThemeArt *art,
                             float x, float y, float reach, unsigned h)
{
    SDL_Renderer *r = s->renderer;
    float live = ((h >> 5) % 3u) == 0u &&
                         fmodf(s->time * 0.9f + (float)(h % 97u) * 0.1f,
                               5.0f) < 0.12f
                     ? 0.25f
                     : 1.0f;
    float on = art_pulse(s, live, 1.0f);
    SDL_Color base = fx_mix(art->near_shape, art->wall, 0.45f);
    SDL_Color wire = fx_mix(art->near_shape, FX_INK, 0.25f);
    fx_rect(r, base, x - 4.0f, y, 9.0f, 2.0f);
    fx_rect(r, fx_mix(base, art->wall_light, 0.3f), x - 4.0f, y, 9.0f, 1.0f);
    fx_rect(r, fx_dim(FX_SODIUM, 0.92f * on), x - 2.0f, y + 2.0f, 5.0f, 3.0f);
    fx_rect(r, fx_dim(FX_SODIUM, 0.55f * on), x - 1.0f, y + 5.0f, 3.0f, 1.0f);
    /* The guard: a hoop round the glass and the wires over it. */
    fx_rect(r, wire, x - 3.0f, y + 2.0f, 1.0f, 4.0f);
    fx_rect(r, wire, x + 3.0f, y + 2.0f, 1.0f, 4.0f);
    fx_rect(r, wire, x, y + 2.0f, 1.0f, 5.0f);
    fx_rect(r, wire, x - 2.0f, y + 6.0f, 5.0f, 1.0f);
    fx_glow(r, x + 0.5f, y + 4.0f, 16.0f, FX_SODIUM, (Uint8)(38.0f * on));
    if (reach > 6.0f)
    {
        fx_light_cone(r, x + 0.5f, y + 6.0f, 3.0f, 20.0f, reach, FX_SODIUM,
                      (Uint8)(16.0f * on));
        fx_rect_a(r, FX_SODIUM, (Uint8)(14.0f * on), x - 14.0f,
                  y + 6.0f + reach - 2.0f, 29.0f, 2.0f);
    }
}

/* One room of the DUCTS theme; see `ArtRoom` for what a room layout owes. */
static void backdrop_ducts_room(const LevelArtScene *s,
                                const LevelThemeArt *art, const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float top = room->top;
    const float floor = room->floor;
    const float height = room->height;
    const float cl = room->clip_left;
    const float cw = room->clip_right - room->clip_left;

    /* The bands of the void, off its own height: the slab's soffit, one run
     * of trunking, a pipe and — where the trunking is not using it — a run on
     * stools along the floor. A taller void spreads them out; a two-tile one,
     * which is every storey this theme is drawn in, packs them the way a real
     * plenum is packed.
     *
     * How a storey's services are run belongs to the storey, so every piece
     * of it agrees and no two storeys need be alike: most hang the trunking
     * from the slab with the pipe under it, some stand it on stools along the
     * floor with the pipe up under the slab, and some carry the air in round
     * spiral-wound duct rather than rectangular. */
    const unsigned kind = (room->seed >> 3) % 3u;
    const bool low = kind == 1u;
    const bool round = kind == 2u;
    const float soffit = 6.0f;
    const float duct_h = fminf(28.0f, fmaxf(14.0f, floorf(height * 0.28f)));
    const float duct_y = low ? floor - duct_h - 6.0f : top + soffit + 9.0f;
    const float pipe_y = low ? top + soffit + 12.0f : duct_y + duct_h + 5.0f;
    const float floor_pipe = floor - 9.0f;
    const float pipe_limit = low ? duct_y : floor_pipe;
    const bool pipe_room = pipe_y + 6.0f < pipe_limit - 4.0f;

    /* Farthest: the frame of the building, far back in the murk — stanchions
     * floor to slab with a haze-lit left flank so each stands clear of the
     * dark, and cross-bracing between them as flat as the void makes it. */
    SDL_Color frame = fx_mix(art->near_shape, art->wall, 0.22f);
    SDL_Color frame_rim = fx_mix(frame, art->haze, 0.2f);
    SDL_Color brace = fx_mix(art->far_shape, frame, 0.7f);
    int post = 0;
    for (float x = art_room_first_lead(s, room, 0.05f, 170.0f, -40.0f, &post);
         x < cl + cw + 180.0f; x += 170.0f, ++post)
    {
        fx_set(r, brace);
        SDL_RenderLine(r, x + 74.0f, top + soffit + 2.0f, x + 230.0f,
                       floor - 3.0f);
        SDL_RenderLine(r, x + 74.0f, floor - 3.0f, x + 230.0f,
                       top + soffit + 2.0f);
        fx_rect(r, frame, x + 60.0f, top, 12.0f, height);
        fx_rect(r, fx_mix(frame, FX_INK, 0.35f), x + 64.0f, top, 5.0f,
                height);
        fx_rect(r, frame_rim, x + 60.0f, top, 1.0f, height);
        fx_rect(r, fx_mix(frame, art->haze, 0.12f), x + 57.0f, floor - 3.0f,
                18.0f, 3.0f);
    }
    /* The murk the frame stands in, thickest in the middle distance. */
    fx_vgrad(r, cl, top + height * 0.25f, cw, height * 0.25f, art->haze, 0,
             art->haze, 14);
    fx_vgrad(r, cl, top + height * 0.5f, cw, height * 0.3f, art->haze, 14,
             art->haze, 0);

    /* Far service lamps: pinpricks on the far steel at a slow parallax, which
     * is what tells the eye the dark goes a long way back. */
    int far_lamp = 0;
    for (float x = art_room_first_lead(s, room, 0.07f, 90.0f, 0.0f, &far_lamp);
         x < cl + cw + 90.0f; x += 90.0f, ++far_lamp)
    {
        unsigned h = fx_hash(room->seed + (unsigned)far_lamp * 0x9e3779b9u);
        if ((h & 7u) == 0u)
            continue;
        float ly = top + soffit + 3.0f + (float)fx_spread(h >> 4,
                                                          height * 0.55f);
        float lx = x + (float)(h % 60u);
        fx_rect(r, fx_dim(FX_SODIUM, 0.55f), lx, ly, 2.0f, 2.0f);
        fx_glow(r, lx + 1.0f, ly + 1.0f, 10.0f, FX_SODIUM, 30);
    }

    /* The underside of the slab above: the main beam along the ceiling, and
     * the secondary beams coming end-on toward the viewer under it — the one
     * thing that says the ceiling is close enough to touch. */
    SDL_Color slab = fx_mix(art->near_shape, art->wall, 0.20f);
    fx_rect(r, slab, cl, top, cw, soffit);
    fx_rect(r, fx_mix(slab, art->wall_light, 0.32f), cl, top + soffit - 1.0f,
            cw, 1.0f);
    fx_rect_a(r, FX_INK, 80, cl, top + soffit, cw, 3.0f);
    int joist = 0;
    for (float x = art_room_first_lead(s, room, 0.14f, 64.0f, 0.0f, &joist);
         x < cl + cw + 16.0f; x += 64.0f, ++joist)
    {
        fx_rect(r, fx_mix(slab, FX_INK, 0.25f), x + 4.0f, top + soffit, 3.0f,
                6.0f);
        fx_rect(r, slab, x, top + soffit + 5.0f, 11.0f, 3.0f);
        fx_rect(r, fx_mix(slab, art->wall_light, 0.22f), x, top + soffit + 5.0f,
                11.0f, 1.0f);
        fx_rect(r, fx_mix(slab, art->wall_light, 0.45f), x, top + soffit + 7.0f,
                11.0f, 1.0f);
        fx_rect_a(r, FX_INK, 70, x + 1.0f, top + soffit + 8.0f, 11.0f, 2.0f);
    }

    /* Galvanised trunking along the void, with the flange rings that make a
     * duct read as a duct and not as a pipe. It is lit along the top, rolls
     * into shade underneath and throws a shadow on the dark below it; round
     * duct carries its highlight a quarter of the way down instead, where a
     * cylinder under a ceiling light catches it. Every so often the run
     * passes through an inline fan box. */
    SDL_Color body = fx_mix(art->far_shape, art->wall, 0.50f);
    SDL_Color body_lit = fx_mix(body, art->wall_light, 0.40f);
    SDL_Color body_dk = fx_mix(body, FX_INK, 0.35f);
    fx_rect(r, body, cl, duct_y, cw, duct_h);
    if (round)
    {
        float crest = floorf(duct_h * 0.28f);
        fx_vgrad(r, cl, duct_y, cw, crest, body_dk, 200, body_lit, 255);
        fx_vgrad(r, cl, duct_y + crest, cw, duct_h - crest, body_lit, 255,
                 fx_mix(body, FX_INK, 0.50f), 255);
        fx_rect_a(r, art->wall_light, 44, cl, duct_y + crest, cw, 1.0f);
        fx_rect(r, fx_mix(body, FX_INK, 0.40f), cl, duct_y, cw, 1.0f);
    }
    else
    {
        fx_vgrad(r, cl, duct_y, cw, duct_h, body_lit, 255, body_dk, 255);
        fx_rect(r, fx_mix(body, art->wall_light, 0.70f), cl, duct_y, cw,
                1.0f);
        fx_rect_a(r, art->wall_light, 22, cl,
                  duct_y + floorf(duct_h * 0.45f), cw, 1.0f);
    }
    fx_rect(r, fx_mix(body, FX_INK, 0.60f), cl, duct_y + duct_h - 1.0f, cw,
            1.0f);
    fx_vgrad(r, cl, duct_y + duct_h, cw, low ? 6.0f : 8.0f, FX_INK, 110,
             FX_INK, 0);
    const float fan_size = fminf(44.0f, floorf(height - soffit - 14.0f));
    int section = 0;
    for (float x = art_room_first_lead(s, room, 0.18f, 96.0f, 0.0f, &section);
         x < cl + cw + 96.0f; x += 96.0f, ++section)
    {
        unsigned sh = fx_hash(room->seed + (unsigned)section * 0x85ebca6bu);
        bool fan = fan_size >= 26.0f &&
                   ((unsigned)section + room->seed) % 4u == 0u;
        if (round)
        {
            /* The spiral seam the strip was wound on, and the coupling where
             * two lengths are pushed together. */
            fx_set(r, fx_mix(body, FX_INK, 0.28f));
            for (float sx = x + 8.0f; sx < x + 96.0f; sx += 11.0f)
                SDL_RenderLine(r, sx, duct_y + 1.0f, sx + 5.0f,
                               duct_y + duct_h - 2.0f);
            fx_rect(r, fx_mix(body, art->wall_light, 0.30f), x, duct_y, 3.0f,
                    duct_h);
            fx_rect(r, fx_mix(body, FX_INK, 0.40f), x + 3.0f, duct_y, 1.0f,
                    duct_h);
        }
        else
        {
            /* The flange ring and its bolts. */
            fx_rect(r, fx_mix(body, art->wall_light, 0.55f), x, duct_y - 2.0f,
                    4.0f, duct_h + 4.0f);
            fx_rect(r, fx_mix(body, FX_INK, 0.40f), x + 4.0f, duct_y - 2.0f,
                    2.0f, duct_h + 4.0f);
            for (float by = duct_y + 3.0f; by < duct_y + duct_h - 2.0f;
                 by += 6.0f)
                fx_rect(r, fx_mix(body, art->trim_hi, 0.5f), x + 1.0f, by,
                        2.0f, 1.0f);
        }
        /* What holds the run up: a rod from the slab and the strap it hangs
         * the duct in, or the stools a run on the floor stands on. */
        if (low)
        {
            for (int leg = 0; leg < 2; ++leg)
                fx_rect(r, fx_mix(body, FX_INK, 0.45f),
                        x + 41.0f + (float)leg * 6.0f, duct_y + duct_h, 1.0f,
                        floor - duct_y - duct_h);
            fx_rect(r, fx_mix(body, art->wall_light, 0.20f), x + 38.0f,
                    floor - 2.0f, 13.0f, 2.0f);
        }
        else
            fx_rect(r, fx_mix(body, FX_INK, 0.50f), x + 44.0f, top + soffit,
                    1.0f, duct_y - top - soffit);
        fx_rect(r, fx_mix(body, FX_INK, 0.30f), x + 41.0f, duct_y - 1.0f, 7.0f,
                duct_h + 2.0f);
        fx_rect(r, fx_mix(body, art->wall_light, 0.45f), x + 41.0f,
                duct_y - 1.0f, 7.0f, 1.0f);
        fx_rect(r, fx_mix(body, art->trim_hi, 0.40f), x + 44.0f,
                duct_y + duct_h, 1.0f, 2.0f);
        if (fan)
        {
            float half = floorf(fan_size * 0.5f);
            float cy = floorf(duct_y + duct_h * 0.5f);
            cy = fmaxf(fminf(cy, floor - half - 1.0f), top + half);
            ducts_fan(s, art, x + 62.0f, cy, fan_size, sh);
            continue;
        }
        /* An access panel on some sections, screwed shut, and the odd section
         * wrapped in foil-faced lagging. Neither is fitted to spiral duct. */
        if (!round && (sh % 3u) == 0u)
        {
            float px = x + 56.0f;
            fx_rect(r, fx_mix(body, FX_INK, 0.45f), px, duct_y + 4.0f, 18.0f,
                    duct_h - 8.0f);
            fx_rect(r, fx_mix(body, art->wall_light, 0.30f), px + 1.0f,
                    duct_y + 5.0f, 16.0f, duct_h - 10.0f);
            fx_rect(r, fx_mix(body, art->wall_light, 0.60f), px + 1.0f,
                    duct_y + 5.0f, 16.0f, 1.0f);
            fx_rect(r, fx_mix(body, art->trim_hi, 0.35f), px + 2.0f,
                    duct_y + 6.0f, 1.0f, 1.0f);
            fx_rect(r, fx_mix(body, art->trim_hi, 0.35f), px + 15.0f,
                    duct_y + 6.0f, 1.0f, 1.0f);
        }
        else if (!round && (sh % 4u) == 1u)
        {
            for (float lx = x + 12.0f; lx < x + 38.0f; lx += 4.0f)
                fx_rect_a(r, art->trim_hi, 26, lx, duct_y, 2.0f, duct_h);
            fx_rect_a(r, art->trim_hi, 40, x + 12.0f, duct_y, 26.0f, 1.0f);
        }
        /* A branch taking off the top of the run and up through the slab to
         * the floor above, with its damper handle: the duct touching the
         * ceiling is most of what says how low the ceiling is. */
        if ((sh % 5u) == 2u)
        {
            float bx = x + 78.0f;
            float by = top + soffit;
            float bh = duct_y - by;
            fx_rect(r, body, bx, by, 12.0f, bh);
            fx_rect(r, fx_mix(body, art->wall_light, 0.40f), bx, by, 1.0f, bh);
            fx_rect(r, fx_mix(body, FX_INK, 0.45f), bx + 11.0f, by, 1.0f, bh);
            fx_rect(r, fx_mix(body, art->wall_light, 0.55f), bx - 1.0f,
                    duct_y - 2.0f, 14.0f, 1.0f);
            fx_rect(r, fx_mix(body, FX_INK, 0.40f), bx - 1.0f, duct_y - 1.0f,
                    14.0f, 1.0f);
            fx_rect(r, fx_dim(art->accent, 0.50f), bx + 3.0f,
                    by + floorf(bh * 0.4f), 6.0f, 1.0f);
        }
    }

    /* The pipe under the trunking and the run along the floor, flanged, on
     * clips and stools, with a valve on some spans and a caged lamp under the
     * upper run every so often. */
    SDL_Color pipe = fx_mix(art->near_shape, art->wall, 0.55f);
    for (int line = 0; line < 2; ++line)
    {
        if ((line == 0 && !pipe_room) || (line == 1 && low))
            continue;
        float factor = 0.22f + (float)line * 0.08f;
        float py = line == 0 ? pipe_y : floor_pipe;
        float thick = 4.0f + (float)line;
        fx_rect(r, pipe, cl, py, cw, thick);
        fx_rect(r, fx_mix(pipe, art->wall_light, 0.55f), cl, py, cw, 1.0f);
        fx_rect(r, fx_mix(pipe, FX_INK, 0.55f), cl, py + thick - 1.0f, cw,
                1.0f);
        fx_rect_a(r, FX_INK, 60, cl, py + thick, cw, 2.0f);
        int span = 0;
        for (float x = art_room_first_lead(s, room, factor, 150.0f, 0.0f, &span);
             x < cl + cw + 150.0f; x += 150.0f, ++span)
        {
            unsigned h = fx_hash(room->seed ^
                                 ((unsigned)span * 0x27d4eb2du +
                                  (unsigned)line * 0x165667b1u));
            fx_rect(r, fx_mix(pipe, art->wall_light, 0.4f), x + 20.0f,
                    py - 2.0f, 3.0f, thick + 4.0f);
            fx_rect(r, fx_mix(pipe, art->wall_light, 0.4f), x + 96.0f,
                    py - 2.0f, 3.0f, thick + 4.0f);
            if (line == 0)
            {
                /* The clip that hangs it off the trunking above it, or off
                 * the slab where the trunking is down on the floor. */
                float from = low ? top + soffit : duct_y + duct_h;
                fx_rect(r, fx_mix(pipe, FX_INK, 0.45f), x + 60.0f, from, 1.0f,
                        py - from);
                fx_rect(r, fx_mix(pipe, FX_INK, 0.30f), x + 58.0f,
                        py + thick, 5.0f, 1.0f);
            }
            else
            {
                /* The stools it rests on. */
                for (int st = 0; st < 2; ++st)
                {
                    float sx = x + 50.0f + (float)st * 75.0f;
                    fx_rect(r, fx_mix(pipe, FX_INK, 0.35f), sx, py + thick,
                            2.0f, floor - py - thick);
                    fx_rect(r, fx_mix(pipe, art->wall_light, 0.2f), sx - 3.0f,
                            floor - 2.0f, 8.0f, 2.0f);
                }
            }
            if ((h % 3u) == 0u)
            {
                /* Gate valve: bonnet and hand wheel, the wheel in the amber
                 * the plant paints its moving parts. */
                fx_rect(r, pipe, x + 118.0f, py - 5.0f, 6.0f, 5.0f);
                fx_rect(r, fx_mix(pipe, art->wall_light, 0.3f), x + 118.0f,
                        py - 5.0f, 6.0f, 1.0f);
                fx_rect(r, fx_dim(art->accent, 0.55f), x + 114.0f, py - 7.0f,
                        14.0f, 2.0f);
                fx_rect(r, fx_mix(art->accent, FX_INK, 0.6f), x + 120.0f,
                        py - 7.0f, 2.0f, 2.0f);
            }
            if (line == 0 && (h % 5u) == 1u)
                ducts_caged_lamp(s, art, x + 40.0f, py + thick,
                                 (low ? duct_y - 2.0f : floor - 8.0f) - py -
                                     thick,
                                 h);
        }
    }

    /* Cable bundles in a tray under the slab, sagging a little between the
     * brackets it hangs from: the closest thing to the camera. */
    int tray = 0;
    SDL_Color cable = fx_mix(art->far_shape, art->wall, 0.30f);
    for (float x = art_room_first_lead(s, room, 0.36f, 120.0f, 0.0f, &tray);
         x < cl + cw + 120.0f; x += 120.0f, ++tray)
    {
        float ty = top + soffit + 3.0f;
        for (int strand = 0; strand < 3; ++strand)
        {
            float sag = 2.0f + (float)strand * 1.5f;
            float y = ty + 1.0f + (float)strand * 1.0f;
            fx_set(r, strand == 0 ? fx_mix(cable, art->wall_light, 0.18f)
                                  : cable);
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
        /* The tray bracket the bundle hangs from, and its rod. */
        fx_rect(r, fx_mix(art->far_shape, art->wall, 0.45f), x - 1.0f,
                top + soffit, 1.0f, 4.0f);
        fx_rect(r, fx_mix(art->far_shape, art->wall, 0.45f), x - 3.0f,
                ty, 6.0f, 2.0f);
        fx_rect(r, fx_mix(art->wall, art->wall_light, 0.25f), x - 3.0f, ty,
                6.0f, 1.0f);
    }
}

/*
 * PENTHOUSE — a panelled hall hung with pictures and lit by sconces. Warm,
 * still, and expensive: the last place in the building that looks lived in.
 *
 * What panelling is made of is relief: raised fields inside mouldings that
 * catch the light along their tops and hold a shadow under them, a dado rail
 * at the height of a hand, a cornice under the ceiling. It is laid out for the
 * room it lines — the wainscot and the furniture are the man's height in every
 * room, and what a taller room gains is wall: taller fields, taller windows
 * down to the floor, and in a hall of two storeys a second tier of panelling
 * over a frieze and a chandelier hanging in the middle of the volume.
 *
 * The wall is laid out in modules of four bays — a mirror or a bare panel, a
 * painting over a console, a window or a lit cabinet, and a sconce — and each
 * storey shuffles which of those it gets, so no two floors of the penthouse
 * are hung the same.
 */
/*
 * One raised field of boiserie: the moulding lit along its top and left and in
 * shade along its bottom and right, which is the ceiling's light and nothing
 * else, and the field inside it catching a little of that light up top.
 */
static void penthouse_field(SDL_Renderer *r, const LevelThemeArt *art,
                            SDL_Color lit, SDL_Color dark, float px, float top,
                            float pw, float ph)
{
    if (ph < 8.0f)
        return;
    fx_rect(r, dark, px, top, pw, 1.0f);
    fx_rect(r, dark, px, top, 1.0f, ph);
    fx_rect(r, lit, px, top + ph - 1.0f, pw, 1.0f);
    fx_rect(r, lit, px + pw - 1.0f, top, 1.0f, ph);
    fx_rect(r, lit, px + 3.0f, top + 3.0f, pw - 6.0f, 1.0f);
    fx_rect(r, lit, px + 3.0f, top + 3.0f, 1.0f, ph - 6.0f);
    fx_rect(r, dark, px + 3.0f, top + ph - 4.0f, pw - 6.0f, 1.0f);
    fx_rect(r, dark, px + pw - 4.0f, top + 3.0f, 1.0f, ph - 6.0f);
    fx_vgrad(r, px + 4.0f, top + 4.0f, pw - 8.0f, fminf(ph - 8.0f, 60.0f),
             art->wall_light, 10, art->wall_light, 0);
}

/* A framed canvas with a subject — a varnished landscape or a portrait — and
 * the brass picture light that washes down it. */
static void penthouse_painting(const LevelArtScene *s, const LevelThemeArt *art,
                               float cx, float bottom, float pw, float ph,
                               unsigned h)
{
    SDL_Renderer *r = s->renderer;
    float px = cx - pw * 0.5f;
    float py = bottom - ph;
    fx_rect_a(r, FX_INK, 90, px + 3.0f, py + 4.0f, pw, ph);
    fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.35f), px, py, pw, ph);
    fx_rect(r, art->trim, px + 1.0f, py + 1.0f, pw - 2.0f, ph - 3.0f);
    fx_rect(r, art->trim_hi, px, py, pw, 1.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.35f), px + 4.0f, py + 4.0f, pw - 8.0f,
            ph - 8.0f);
    float ix = px + 6.0f;
    float iy = py + 6.0f;
    float iw = pw - 12.0f;
    float ih = ph - 12.0f;
    if ((h >> 11) & 1u)
    {
        /* A landscape: a dusk sky going down to a lit horizon, a range of
         * hills and water under them. Old varnish browns all of it. */
        SDL_Color sky = fx_mix(art->far_shape, art->lamp, 0.22f);
        SDL_Color dusk = fx_mix(art->far_shape, art->lamp, 0.42f);
        float horizon = iy + ih * (0.52f + art_unit(h, 13) * 0.14f);
        fx_vgrad(r, ix, iy, iw, horizon - iy, sky, 255, dusk, 255);
        SDL_Color hill = fx_mix(art->far_shape, art->wall_dark, 0.5f);
        for (int step = 0; step < 6; ++step)
        {
            float hw = iw / 6.0f;
            float rise = 3.0f + (float)((h >> (unsigned)(step * 2)) % 7u);
            fx_rect(r, hill, ix + (float)step * hw, horizon - rise, hw + 1.0f,
                    rise);
        }
        fx_rect(r, fx_mix(art->far_shape, art->wall, 0.3f), ix, horizon, iw,
                iy + ih - horizon);
        fx_rect_a(r, art->lamp, 50, ix + iw * 0.3f, horizon + 2.0f, iw * 0.3f,
                  1.0f);
    }
    else
    {
        /* A portrait: the sitter as a dark bust against a darker ground, face
         * and collar picking up the painter's light. */
        fx_rect(r, fx_mix(art->far_shape, art->wall_dark, 0.3f), ix, iy, iw, ih);
        float bx = ix + iw * 0.5f;
        fx_mass(r, fx_mix(art->far_shape, FX_INK, 0.35f), bx - iw * 0.3f,
                iy + ih * 0.55f, iw * 0.6f, ih * 0.45f, 5, 0);
        fx_mass(r, fx_mix(art->far_shape, art->lamp, 0.3f), bx - ih * 0.1f,
                iy + ih * 0.22f, ih * 0.2f, ih * 0.26f, 3, 3);
        fx_rect(r, fx_mix(art->far_shape, art->lamp, 0.2f), bx - 3.0f,
                iy + ih * 0.55f, 6.0f, 3.0f);
    }
    /* Craquelure and varnish: the one sheen across the canvas. */
    fx_rect_a(r, art->lamp, 16, ix, iy, iw, ih * 0.3f);
    /* The picture light: a brass bar over the frame, and what it pours down
     * the canvas. */
    fx_rect(r, art->trim, cx - 13.0f, py - 7.0f, 26.0f, 3.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.3f), cx - 1.0f, py - 4.0f, 2.0f,
            4.0f);
    fx_rect(r, fx_dim(art->lamp, 0.9f), cx - 11.0f, py - 4.0f, 22.0f, 1.0f);
    fx_light_cone(r, cx, py - 3.0f, 11.0f, pw * 0.55f, ph * 0.8f, art->lamp, 34);
    fx_glow(r, cx, py - 3.0f, 26.0f, art->lamp, 28);
}

/* A wall sconce: a brass bracket, a shade lit from inside, and the light it
 * throws both ways along the panelling with a slow, candle-like breath. */
static void penthouse_sconce(const LevelArtScene *s, const LevelThemeArt *art,
                             float cx, float y, float room_top, unsigned salt)
{
    SDL_Renderer *r = s->renderer;
    float breathe = art_pulse(
        s, 0.9f + 0.1f * sinf(s->time * 1.6f + (float)(salt % 628u) * 0.01f),
        0.95f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.3f), cx - 2.0f, y + 12.0f, 4.0f,
            5.0f);
    fx_rect(r, art->trim, cx - 5.0f, y, 10.0f, 13.0f);
    fx_rect(r, art->trim_hi, cx - 5.0f, y + 12.0f, 10.0f, 1.0f);
    fx_rect(r, fx_dim(art->lamp, breathe), cx - 3.0f, y + 2.0f, 6.0f, 8.0f);
    fx_glow(r, cx, y + 6.0f, 40.0f, art->lamp, (Uint8)(50.0f * breathe));
    fx_light_cone(r, cx, y + 13.0f, 5.0f, 22.0f, 60.0f, art->lamp,
                  (Uint8)(22.0f * breathe));
    float up = fminf(40.0f, y - room_top - 4.0f);
    if (up > 4.0f)
        fx_light_cone(r, cx, y + 1.0f, 5.0f, 16.0f, -up, art->lamp,
                      (Uint8)(20.0f * breathe));
}

/*
 * A tall window onto the city between heavy drapes, running down nearly to
 * the floor. From forty storeys up the city is mostly below the horizon: a
 * street grid of sodium, a few towers rising through it, and the sky.
 */
static void penthouse_window(const LevelArtScene *s, const LevelThemeArt *art,
                             float wx, float wy, float ww, float floor)
{
    SDL_Renderer *r = s->renderer;
    float wb = floor - 10.0f;
    float wh = wb - wy;
    if (wh < 20.0f)
        return;
    fx_rect(r, FX_INK, wx - 3.0f, wy - 3.0f, ww + 6.0f, wh + 6.0f);
    lobby_city_view(s, wx, wy, ww, wh, lobby_horizon(s, 0.46f, 0.06f), 1.0f);
    /* Glazing bars, the veil of glass, and the sill. */
    SDL_Color bar = fx_mix(art->wall_dark, FX_INK, 0.3f);
    fx_rect(r, bar, wx + ww * 0.5f - 1.0f, wy, 2.0f, wh);
    int panes = (int)(wh / 42.0f);
    if (panes < 2)
        panes = 2;
    for (int i = 1; i < panes; ++i)
        fx_rect(r, bar, wx, wy + wh * (float)i / (float)panes - 1.0f, ww, 2.0f);
    fx_rect_a(r, FX_LAMP, 10, wx, wy, ww, wh);
    fx_rect_a(r, art->trim_hi, 12, wx + 6.0f, wy, 9.0f, wh);
    fx_rect_a(r, art->trim_hi, 7, wx + ww * 0.5f + 5.0f, wy, 5.0f, wh);
    fx_rect(r, art->trim, wx - 5.0f, wb, ww + 10.0f, 4.0f);
    fx_rect(r, art->trim_hi, wx - 5.0f, wb, ww + 10.0f, 1.0f);

    /* The drapes: deep, gathered, lit down their folds, and pooled on the
     * floor at the foot. */
    SDL_Color drape = fx_mix(art->wall_dark, art->accent, 0.16f);
    for (int side = 0; side < 2; ++side)
    {
        float dx = side == 0 ? wx - 13.0f : wx + ww - 1.0f;
        fx_rect(r, drape, dx, wy - 8.0f, 14.0f, floor - wy + 8.0f);
        fx_vgrad(r, dx, wy - 8.0f, 14.0f, floor - wy + 8.0f, art->lamp, 18,
                 FX_INK, 60);
        for (int fold = 0; fold < 3; ++fold)
        {
            fx_rect(r, fx_mix(drape, art->lamp, 0.2f),
                    dx + 2.0f + (float)fold * 4.0f, wy - 8.0f, 1.0f,
                    floor - wy + 8.0f);
            fx_rect(r, fx_mix(drape, FX_INK, 0.4f),
                    dx + 4.0f + (float)fold * 4.0f, wy - 8.0f, 1.0f,
                    floor - wy + 8.0f);
        }
        fx_rect(r, fx_mix(drape, FX_INK, 0.2f), dx - 1.0f, floor - 3.0f, 16.0f,
                3.0f);
    }
    /* The pelmet the drapes hang from. */
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.2f), wx - 17.0f, wy - 12.0f,
            ww + 34.0f, 5.0f);
    fx_rect(r, art->trim_hi, wx - 17.0f, wy - 12.0f, ww + 34.0f, 1.0f);
    fx_rect_a(r, FX_INK, 80, wx - 16.0f, wy - 7.0f, ww + 32.0f, 2.0f);
}

/* A display cabinet standing against the panelling, its shelves lit from
 * inside. */
static void penthouse_cabinet(SDL_Renderer *r, const LevelThemeArt *art,
                              float x, float floor, float cab_h, unsigned h)
{
    float cy = floor - cab_h;
    fx_rect_a(r, FX_INK, 70, x + 3.0f, cy + 3.0f, 56.0f, cab_h - 3.0f);
    fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.3f), x, cy, 56.0f, cab_h);
    fx_rect(r, art->trim, x - 2.0f, cy, 60.0f, 3.0f);
    fx_rect(r, art->trim_hi, x - 2.0f, cy, 60.0f, 1.0f);
    int shelves = (int)((cab_h - 16.0f) / 24.0f);
    for (int shelf = 0; shelf < shelves; ++shelf)
    {
        unsigned sh = fx_hash(h + (unsigned)shelf * 131u);
        float sy = cy + 26.0f + (float)shelf * 24.0f;
        fx_rect_a(r, art->lamp, 44, x + 4.0f, sy - 20.0f, 48.0f, 20.0f);
        fx_rect(r, art->trim_hi, x + 4.0f, sy, 48.0f, 2.0f);
        /* A vase, a bowl, a small bronze: never the same row twice. */
        float ox = x + 10.0f + (float)(sh % 20u);
        fx_mass(r, fx_mix(art->far_shape, art->trim, 0.45f), ox, sy - 12.0f,
                8.0f, 12.0f, 2, 1);
        fx_rect(r, fx_mix(art->trim, art->trim_hi, 0.5f), ox + 1.0f, sy - 12.0f,
                2.0f, 8.0f);
        fx_mass(r, fx_mix(art->far_shape, art->lamp, 0.25f),
                x + 34.0f + (float)(sh >> 8 & 7u), sy - 6.0f, 10.0f, 6.0f, 2, 0);
    }
    fx_rect_a(r, art->trim_hi, 22, x + 6.0f, cy + 4.0f, 5.0f, cab_h - 8.0f);
    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.3f), x, floor - 8.0f, 56.0f,
            8.0f);
}

/* A console table under a picture, with a lamp left burning on it. */
static void penthouse_console(const LevelArtScene *s, const LevelThemeArt *art,
                              float cx, float floor)
{
    SDL_Renderer *r = s->renderer;
    float ty = floor - 15.0f;
    SDL_Color wood = fx_mix(art->wall_dark, art->trim, 0.35f);
    fx_contact_shadow(r, cx, floor - 2.0f, 24.0f, 0.0f, 70);
    fx_rect(r, wood, cx - 22.0f, ty, 44.0f, 3.0f);
    fx_rect(r, fx_mix(wood, art->trim_hi, 0.4f), cx - 22.0f, ty, 44.0f, 1.0f);
    fx_rect(r, fx_mix(wood, FX_INK, 0.3f), cx - 20.0f, ty + 3.0f, 40.0f, 3.0f);
    fx_rect(r, fx_mix(wood, FX_INK, 0.4f), cx - 19.0f, ty + 3.0f, 2.0f, 12.0f);
    fx_rect(r, fx_mix(wood, FX_INK, 0.4f), cx + 17.0f, ty + 3.0f, 2.0f, 12.0f);
    /* The lamp: a turned base, a pleated shade glowing through. */
    fx_rect(r, art->trim, cx - 9.0f, ty - 5.0f, 5.0f, 5.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.3f), cx - 7.0f, ty - 9.0f, 1.0f, 4.0f);
    fx_mass(r, fx_mix(art->lamp, art->trim, 0.35f), cx - 12.0f, ty - 16.0f,
            11.0f, 7.0f, 2, 0);
    fx_glow(r, cx - 6.5f, ty - 11.0f, 22.0f, art->lamp, 40);
    fx_light_cone(r, cx - 6.5f, ty - 9.0f, 5.0f, 14.0f, 9.0f, art->lamp, 30);
    /* A small bronze at the other end. */
    fx_mass(r, fx_mix(art->far_shape, art->trim, 0.4f), cx + 8.0f, ty - 7.0f,
            6.0f, 7.0f, 2, 0);
}

/* A chandelier in a hall tall enough to hang one: a chain out of the ceiling,
 * two rings of candle bulbs, and the crystal drops that catch them. */
static void penthouse_chandelier(const LevelArtScene *s,
                                 const LevelThemeArt *art, float cx, float top,
                                 float drop)
{
    SDL_Renderer *r = s->renderer;
    float cy = top + drop;
    SDL_Color brass = art->trim;
    SDL_Color brass_dk = fx_mix(art->trim, FX_INK, 0.35f);
    SDL_Color bulb = fx_mix(art->lamp, FX_CREAM, 0.5f);
    for (float link = top; link < cy - 14.0f; link += 4.0f)
        fx_rect(r, brass_dk, cx - 0.5f, link, 1.0f, 3.0f);
    /* The stem, a bobeche at its foot, and the two arms of rings. */
    fx_rect(r, brass, cx - 2.0f, cy - 14.0f, 4.0f, 26.0f);
    fx_rect(r, art->trim_hi, cx - 2.0f, cy - 14.0f, 1.0f, 26.0f);
    fx_mass(r, brass, cx - 5.0f, cy + 10.0f, 10.0f, 5.0f, 0, 2);
    const float ring_w[2] = {30.0f, 52.0f};
    const float ring_y[2] = {cy - 8.0f, cy + 4.0f};
    const int ring_n[2] = {5, 7};
    for (int ring = 0; ring < 2; ++ring)
    {
        float hw = ring_w[ring] * 0.5f;
        fx_rect(r, brass_dk, cx - hw, ring_y[ring], ring_w[ring], 2.0f);
        fx_rect(r, art->trim_hi, cx - hw, ring_y[ring], ring_w[ring], 1.0f);
        for (int i = 0; i < ring_n[ring]; ++i)
        {
            float ax = cx - hw + ring_w[ring] * (float)i / (float)(ring_n[ring] - 1);
            fx_rect(r, fx_mix(FX_CREAM, art->lamp, 0.4f), ax - 0.5f,
                    ring_y[ring] - 4.0f, 1.0f, 4.0f);
            fx_rect(r, bulb, ax - 1.0f, ring_y[ring] - 6.0f, 2.0f, 2.0f);
            /* A drop of crystal under every candle, and a longer one under
             * every other. */
            fx_rect_a(r, FX_PALE, 150, ax - 0.5f, ring_y[ring] + 2.0f, 1.0f,
                      (i & 1) ? 5.0f : 3.0f);
            fx_rect_a(r, FX_CREAM, 180, ax - 0.5f, ring_y[ring] + ((i & 1) ? 6.0f : 4.0f),
                      1.0f, 1.0f);
        }
    }
    fx_rect_a(r, FX_PALE, 160, cx - 1.0f, cy + 15.0f, 2.0f, 7.0f);
    fx_glow(r, cx, cy - 2.0f, 60.0f, art->lamp, 46);
    fx_glow(r, cx, cy - 2.0f, 22.0f, art->lamp, 60);
}

/* One room of the PENTHOUSE theme: see the top of this section. */
static void backdrop_penthouse_room(const LevelArtScene *s,
                                    const LevelThemeArt *art,
                                    const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float wall = 0.45f;
    const float bay = 78.0f;
    const float cl = room->clip_left;
    const float cr = room->clip_right;
    const float cw = cr - cl;
    const float top = room->top;
    const float floor = room->floor;
    const float high = room->height;
    const bool tall = high > 272.0f;
    const float cornice = top + (tall ? 18.0f : 14.0f);
    const float dado = floor - 30.0f;
    const float skirt = floor - 7.0f;
    /* A hall of two storeys takes a frieze across the middle of its upper
     * wall, so a field never runs the height of the room. */
    const float frieze = tall ? cornice + (dado - cornice) * 0.42f : 0.0f;
    SDL_Color field = fx_mix(art->far_shape, art->wall, 0.35f);
    SDL_Color field_lit = fx_mix(field, art->wall_light, 0.30f);
    SDL_Color field_dark = fx_mix(field, FX_INK, 0.45f);

    fx_rect(r, field, cl, top, cw, high);
    /* The cove light under the cornice washes the top of the wall, and the
     * room's light falls away toward the floor. */
    fx_vgrad(r, cl, cornice, cw, fminf(96.0f, high * 0.5f), art->lamp, 16,
             art->lamp, 0);
    fx_vgrad(r, cl, dado - 40.0f, cw, 40.0f, FX_INK, 0, FX_INK, 36);

    LobbyRun b = lobby_run(s->cam_x, wall, bay, 0.0f, cl - bay);
    for (; b.x < cr; b.x += bay)
    {
        fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.3f), b.x, cornice, 4.0f,
                floor - cornice);
        fx_rect(r, fx_mix(art->far_shape, art->trim, 0.25f), b.x + 4.0f,
                cornice, 1.0f, floor - cornice);
        float px = b.x + 12.0f;
        const float pw = 58.0f;
        if (tall)
        {
            penthouse_field(r, art, field_lit, field_dark, px, cornice + 12.0f,
                            pw, frieze - 8.0f - (cornice + 12.0f));
            penthouse_field(r, art, field_lit, field_dark, px, frieze + 10.0f,
                            pw, dado - 10.0f - (frieze + 10.0f));
        }
        else
        {
            penthouse_field(r, art, field_lit, field_dark, px, cornice + 12.0f,
                            pw, dado - 10.0f - (cornice + 12.0f));
        }
        penthouse_field(r, art, field_lit, field_dark, px, dado + 8.0f, pw,
                        skirt - 4.0f - (dado + 8.0f));
    }

    /* The cornice under the ceiling, stepped, and the cove behind it. */
    fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.2f), cl, top, cw, cornice - top);
    fx_rect(r, fx_mix(art->far_shape, art->trim, 0.3f), cl, cornice - 6.0f, cw,
            2.0f);
    fx_rect(r, art->trim, cl, cornice, cw, 4.0f);
    fx_rect(r, art->trim_hi, cl, cornice, cw, 1.0f);
    fx_rect(r, fx_mix(art->trim, FX_INK, 0.5f), cl, cornice + 4.0f, cw, 2.0f);
    fx_rect_a(r, FX_INK, 60, cl, cornice + 6.0f, cw, 4.0f);
    fx_rect_a(r, art->lamp, 60, cl, cornice - 4.0f, cw, 1.0f);
    if (tall)
    {
        fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.35f), cl, frieze, cw,
                4.0f);
        fx_rect(r, fx_mix(art->trim, art->trim_hi, 0.4f), cl, frieze, cw, 1.0f);
        fx_rect_a(r, FX_INK, 70, cl, frieze + 4.0f, cw, 3.0f);
    }
    /* The dado rail, and the shadow it throws on the panelling under it. */
    fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.35f), cl, dado, cw, 5.0f);
    fx_rect(r, fx_mix(art->trim, art->trim_hi, 0.4f), cl, dado, cw, 1.0f);
    fx_rect_a(r, FX_INK, 70, cl, dado + 5.0f, cw, 3.0f);
    /* The skirting. */
    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.35f), cl, skirt, cw, 7.0f);
    fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.45f), cl, skirt, cw, 1.0f);

    /* The hangings, one module of four bays at a time. */
    const float module = bay * 4.0f;
    LobbyRun m = lobby_run(s->cam_x, wall, module, 0.0f, cl - module);
    for (; m.x < cr + 40.0f; m.x += module, ++m.index)
    {
        unsigned h = fx_hash((unsigned)m.index * 2246822519u + room->seed);
        float centre[4];
        for (int i = 0; i < 4; ++i)
            centre[i] = m.x + bay * (float)i + 41.0f;

        /* Bay 0: a pier glass in a gilt frame, or bare panelling. */
        if ((h >> 3) & 1u)
        {
            float mh = fminf(tall ? 150.0f : 96.0f, dado - cornice - 34.0f);
            float mb = dado - 14.0f;
            if (mh > 30.0f)
            {
                fx_rect_a(r, FX_INK, 80, centre[0] - 14.0f, mb - mh + 3.0f, 32.0f,
                          mh);
                fx_rect(r, art->trim, centre[0] - 16.0f, mb - mh, 32.0f, mh);
                fx_rect(r, art->trim_hi, centre[0] - 16.0f, mb - mh, 32.0f, 1.0f);
                fx_vgrad(r, centre[0] - 13.0f, mb - mh + 3.0f, 26.0f, mh - 6.0f,
                         fx_mix(field, FX_PALE, 0.28f), 255,
                         fx_mix(field, art->lamp, 0.14f), 255);
                fx_rect_a(r, FX_CREAM, 22, centre[0] - 9.0f, mb - mh + 3.0f, 5.0f,
                          mh - 6.0f);
                fx_rect_a(r, FX_CREAM, 14, centre[0] + 2.0f, mb - mh + 3.0f, 2.0f,
                          mh - 6.0f);
            }
        }

        /* Bay 1: a painting, hung to the room, and a console under it. */
        {
            float pw = 50.0f + (float)(h % 14u);
            float ph;
            float bottom;
            if (tall)
            {
                ph = fminf(110.0f, frieze - cornice - 40.0f);
                bottom = frieze - 20.0f;
                pw = 58.0f + (float)(h % 10u);
            }
            else
            {
                ph = fminf(64.0f, dado - cornice - 50.0f) +
                     (float)((h >> 5) % 8u);
                bottom = dado - 20.0f;
            }
            if (ph > 24.0f)
                penthouse_painting(s, art, centre[1], bottom, pw, ph, h);
            if (tall && ((h >> 9) & 1u))
            {
                /* And a smaller one down at eye level. */
                float lph = fminf(56.0f, dado - frieze - 44.0f);
                if (lph > 24.0f)
                    penthouse_painting(s, art, centre[1], dado - 20.0f, 44.0f,
                                       lph, h >> 3);
            }
            if ((h >> 13) & 1u)
                penthouse_console(s, art, centre[1], floor);
        }

        /* Bay 2: a window onto the city, or a display cabinet. */
        if ((h >> 16) % 5u < 3u)
        {
            float wy = cornice + (tall ? 22.0f : 16.0f);
            penthouse_window(s, art, centre[2] - 25.0f, wy, 50.0f, floor);
        }
        else
        {
            float cab_h = fminf(tall ? 150.0f : 88.0f, high - (cornice - top) - 30.0f);
            if (cab_h > 40.0f)
                penthouse_cabinet(r, art, centre[2] - 28.0f, floor, cab_h, h);
        }

        /* Bay 3: a sconce on the panelling at the height of a raised hand. */
        penthouse_sconce(s, art, centre[3], dado - 36.0f, top,
                         (unsigned)m.index * 157u + room->seed);
    }

    /* A hall of two storeys has a volume to hang something in. The chandelier
     * hangs in front of the wall, so it drifts a little faster than it. */
    if (tall)
    {
        const float span = module * 2.0f;
        LobbyRun c = lobby_run(s->cam_x, 0.6f, span, span * 0.25f, cl - 80.0f);
        for (; c.x < cr + 80.0f; c.x += span)
            penthouse_chandelier(s, art, c.x + 40.0f, top, high * 0.3f);
    }
}

/*
 * ROOF — the plant level at the top of the tower, behind a curtain wall with
 * the whole city on the other side of the glass. After five climbs on the
 * outside of this building, the last sector puts the drop behind glass.
 *
 * Two distances, and the whole picture is keeping them apart. The glass, its
 * mullions and transoms, the truss it hangs from and the bracing in front of
 * it are the building: they belong to the room and move with it on both axes.
 * The sky, the moon, the overcast and the city are a long way off: they are
 * drawn against the frame and only sink a little as the camera climbs, so the
 * structure slides past them the way a real one would. The low service level
 * under the roof sees the same city, through a ribbon of glass and louvres.
 */

/* Where one roof layer's repeats begin for the piece being drawn. */
static float roof_first(float clip_left, float scroll, float period,
                        float reach, int *index)
{
    int skip = (int)floorf((clip_left - reach - scroll) / period);
    *index += skip;
    return scroll + (float)skip * period;
}

/* How far the view has sunk as the camera climbs: the city is at eye level
 * whatever storey Chuck is on, so height puts it further down the frame, by a
 * small share of what it moves the building. `share` lets the moon and the
 * overcast sink less than the towers under them. */
static float roof_sink(const LevelArtScene *s, float share)
{
    return level_backdrop_sink(&s->level->map, s->cam_y,
                               (float)s->win_h - HUD_HEIGHT, 0.12f * share);
}

/*
 * The view: sky, moon, overcast and the city, across `x0`..`x1` of the frame
 * and whatever of the frame's height the clip allows.
 *
 * The sky behind the towers has to stay darker than the room or the skyline
 * walks inside the building, so what separates one tower from the next cannot
 * be a brighter sky — it has to be the haze they stand in and a rim on their
 * own edges.
 */
static void roof_view(const LevelArtScene *s, const LevelThemeArt *art,
                      float x0, float x1, float y0, float y1)
{
    SDL_Renderer *r = s->renderer;
    const float view_top = HUD_HEIGHT;
    const float view_h = (float)s->win_h - HUD_HEIGHT;
    const float w = x1 - x0;
    const float sink = roof_sink(s, 1.0f);
    const float horizon = view_top + view_h * 0.76f + sink;

    /* The sky's own gradient, on the frame rather than on the room: over it
     * the room's air would carry the dark band up and down with the
     * building. */
    fx_rect(r, art->air_top, x0, y0, w, y1 - y0);
    fx_vgrad(r, x0, view_top - 40.0f + sink * 0.5f, w,
             horizon - view_top + 40.0f, art->air_top, 255, art->air_bottom,
             255);
    fx_rect(r, art->air_bottom, x0, horizon, w, fmaxf(0.0f, y1 - horizon));

    /* Stars in the clear gaps. They do not scroll: nothing a camera can do in
     * one sector moves them. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 44; ++i)
    {
        unsigned h = art_hash(i * 17 + 3, 401);
        float x = (float)fx_spread(h, (float)s->win_w);
        if (x < x0 - 1.0f || x > x1)
            continue;
        float y = view_top + sink * 0.4f +
                  (float)fx_spread(h >> 9, view_h * 0.42f);
        SDL_SetRenderDrawColor(r, art->haze.r, art->haze.g, art->haze.b,
                               (Uint8)(50 + h % 90u));
        fx_fill(r, x, y, 1.0f, 1.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* The city glow gathered along the horizon. */
    fx_vgrad(r, x0, horizon - 190.0f, w, 190.0f, art->lamp, 0, art->lamp, 34);

    /* The moon, up and to the left the way it hangs over every other roof in
     * the game: small, hard-edged, with a halo the veil of the curtain wall
     * softens. */
    float moon_x = (float)s->win_w * 0.26f;
    float moon_y = view_top + 100.0f + roof_sink(s, 0.3f);
    if (moon_x + 90.0f > x0 && moon_x - 90.0f < x1)
    {
        fx_glow(r, moon_x, moon_y, 90.0f, FX_STEEL_LT, 40);
        fx_glow(r, moon_x, moon_y, 34.0f, FX_PALE, 60);
        fx_mass(r, fx_mix(FX_PALE, FX_STEEL, 0.35f), moon_x - 9.0f,
                moon_y - 9.0f, 18.0f, 18.0f, 5, 5);
        fx_mass(r, fx_mix(FX_PALE, FX_CREAM, 0.4f), moon_x - 8.0f,
                moon_y - 8.0f, 16.0f, 16.0f, 5, 5);
        fx_rect_a(r, FX_STEEL_LT, 70, moon_x - 4.0f, moon_y - 3.0f, 5.0f,
                  4.0f);
        fx_rect_a(r, FX_STEEL_LT, 55, moon_x + 2.0f, moon_y + 2.0f, 3.0f,
                  3.0f);
    }

    /* A high, broken overcast drifting across it, lit from underneath by the
     * city: dark on top, the street's warmth along its belly. Each bank is
     * keyed to its place on its own drift. */
    SDL_Color overcast = fx_mix(art->far_shape, art->haze, 0.12f);
    SDL_Color belly = fx_mix(overcast, FX_SODIUM, 0.3f);
    for (int deck = 0; deck < 2; ++deck)
    {
        float drift = s->time * (3.0f + (float)deck * 2.0f) +
                      s->cam_x * (0.02f + (float)deck * 0.01f);
        const float seg = 110.0f;
        float cy = view_top + 30.0f + (float)deck * 70.0f +
                   roof_sink(s, 0.6f + (float)deck * 0.2f);
        int first = (int)floorf((drift + x0 - 180.0f) / seg);
        int last = (int)floorf((drift + x1 + 60.0f) / seg);
        for (int k = first; k <= last; ++k)
        {
            unsigned ch = art_hash(k, 1709 + deck);
            if ((ch % 3u) == 0u)
                continue;
            float cx = (float)k * seg - drift + (float)(ch % 30u) - 40.0f;
            float cw = seg + (float)((ch >> 5) % 70u);
            float ch_h = 14.0f + (float)((ch >> 11) % 14u);
            float cloud_top = cy + (float)((ch >> 15) % 14u);
            art_mass_a(r, overcast, 160, cx + cw * 0.18f, cloud_top - 6.0f,
                       cw * 0.5f, 8.0f, 4, 0);
            art_mass_a(r, overcast, 160, cx, cloud_top + 2.0f, cw,
                       ch_h * 0.5f - 1.0f, 6, 0);
            fx_vgrad(r, cx, cloud_top + 1.0f + ch_h * 0.5f, cw, ch_h * 0.5f,
                     overcast, 160, belly, 160);
            fx_vgrad(r, cx + 6.0f, cloud_top + 1.0f + ch_h, cw - 12.0f, 7.0f,
                     belly, 110, belly, 0);
        }
    }

    /* Two ranks. The far one is a low even band that gives the skyline a
     * floor; the near one breaks the horizon and carries the lit windows.
     * Both run on down past the horizon into the haze, because from up here
     * the city is below eye level and its towers have no foot to show. */
    int distant = art_repeat(s->cam_x, 0.03f, 88.0f);
    for (float x = roof_first(x0, art_scroll(s->cam_x, 0.03f, 88.0f) - 44.0f,
                              88.0f, 80.0f, &distant);
         x < x1; x += 88.0f, ++distant)
    {
        unsigned h = fx_hash((unsigned)distant * 2246822519u);
        float slab = 54.0f + (float)(h % 52u);
        fx_rect(r, fx_mix(art->far_shape, art->lamp, 0.1f), x, horizon - slab,
                80.0f, slab + 120.0f);
        fx_rect_a(r, art->haze, 40, x, horizon - slab, 80.0f, 1.0f);
    }

    int block = art_repeat(s->cam_x, 0.05f, 132.0f);
    for (float x = roof_first(x0, art_scroll(s->cam_x, 0.05f, 132.0f) - 40.0f,
                              132.0f, 84.0f, &block);
         x < x1; x += 132.0f, ++block)
    {
        unsigned h = fx_hash((unsigned)block * 71u);
        float tower_h = 120.0f + (float)(h % 150u);
        float top = horizon - tower_h;
        fx_rect(r, art->far_shape, x, top, 84.0f, tower_h + 130.0f);
        /* The rim: a lit parapet and the flank the moon reaches. */
        fx_rect_a(r, art->haze, 90, x, top, 84.0f, 1.0f);
        fx_rect_a(r, art->haze, 55, x, top, 1.0f, tower_h);
        fx_rect(r, fx_mix(art->far_shape, art->accent, 0.2f), x, top, 84.0f,
                2.0f);
        /* A setback crown on some, so the roofline is not a row of boxes. */
        if ((h % 4u) == 1u)
        {
            fx_rect(r, art->far_shape, x + 22.0f, top - 14.0f, 40.0f, 14.0f);
            fx_rect_a(r, art->haze, 70, x + 22.0f, top - 14.0f, 40.0f, 1.0f);
        }
        int floor_index = 0;
        for (float wy = top + 10.0f; wy < horizon + 116.0f;
             wy += 16.0f, ++floor_index)
        {
            if (wy + 5.0f < y0 || wy > y1)
                continue;
            int bay = 0;
            for (float wx = x + 8.0f; wx < x + 76.0f; wx += 14.0f, ++bay)
            {
                /* Keyed to the tower, floor and bay rather than to where the
                 * window currently is on screen: hashing the screen position
                 * made the whole skyline switch its lights while Chuck
                 * walked. */
                unsigned wh = art_hash(block * 13 + bay, floor_index + 71);
                if ((wh % 5u) < 2u)
                    continue;
                SDL_Color lit = (wh & 8u)
                                    ? art->lamp
                                    : fx_mix(art->lamp, art->accent, 0.6f);
                fx_rect_a(r, lit, (Uint8)(70u + (wh >> 6) % 80u), wx, wy, 4.0f,
                          5.0f);
            }
        }
        /* Aircraft warning light on the tallest neighbours. */
        if ((h % 3u) == 0u)
        {
            float lamp_top = (h % 4u) == 1u ? top - 14.0f : top;
            float pulse = art_pulse(
                s, sinf(s->time * 1.6f + (float)(h % 5u)) > 0.7f ? 1.0f : 0.15f,
                0.36f);
            fx_glow(r, x + 42.0f, lamp_top - 4.0f, 14.0f, FX_RED,
                    (Uint8)(90.0f * pulse));
        }
    }
    /* Ground haze thick enough to lose the foot of the towers in, deepening
     * the further below eye level the view looks, and the streets' own glow
     * coming up through it from far below. */
    fx_vgrad(r, x0, horizon - view_h * 0.14f, w, view_h * 0.14f + 120.0f,
             art->far_shape, 0, art->far_shape, 170);
    fx_rect_a(r, art->far_shape, 170, x0, horizon + 120.0f, w,
              fmaxf(0.0f, y1 - horizon - 120.0f));
    fx_vgrad(r, x0, horizon + 20.0f, w, 90.0f, FX_SODIUM, 0, FX_SODIUM, 26);

    /* The streets themselves, forty storeys down: rows of sodium points
     * under the haze, a few white ones among them, each keyed to the block
     * it belongs to. Nearer rows are lower in the frame and a little
     * brighter. */
    if (y1 > horizon + 30.0f)
    {
        int street = art_repeat(s->cam_x, 0.04f, 64.0f);
        for (float x = roof_first(x0, art_scroll(s->cam_x, 0.04f, 64.0f),
                                  64.0f, 64.0f, &street);
             x < x1; x += 64.0f, ++street)
        {
            for (int k = 0; k < 9; ++k)
            {
                unsigned h = art_hash(street * 11 + k, 523);
                int row = (int)((h >> 8) % 12u);
                float ly = horizon + 36.0f + (float)row * 8.0f +
                           (float)(row * row) * 0.35f;
                if (ly < y0 || ly > y1)
                    continue;
                SDL_Color c = (h % 5u) == 0u ? art->lamp : FX_SODIUM;
                fx_rect_a(r, c, (Uint8)(50u + (unsigned)row * 8u + h % 30u),
                          x + (float)(h % 64u), ly, row > 5 ? 2.0f : 1.0f,
                          1.0f);
            }
        }
    }
}

/* The view through a window on the wall: `roof_view` clipped to the pane and
 * to the piece being drawn, and the piece's clip put back afterwards. */
static void roof_view_in(const LevelArtScene *s, const LevelThemeArt *art,
                         const ArtRoom *room, float x0, float y0, float x1,
                         float y1)
{
    SDL_Renderer *r = s->renderer;
    x0 = fmaxf(x0, room->clip_left);
    x1 = fminf(x1, room->clip_right);
    y0 = fmaxf(y0, room->clip_top);
    y1 = fminf(y1, room->clip_bottom);
    if (x1 <= x0 || y1 <= y0)
        return;
    SDL_Rect piece;
    SDL_GetRenderClipRect(r, &piece);
    SDL_Rect pane = {(int)floorf(x0), (int)floorf(y0),
                     (int)ceilf(x1) - (int)floorf(x0),
                     (int)ceilf(y1) - (int)floorf(y0)};
    SDL_SetRenderClipRect(r, &pane);
    roof_view(s, art, x0, x1, y0, y1);
    SDL_SetRenderClipRect(r, &piece);
}

/* Rain on the outside of the glass: it travels with the glass, at the
 * mullions' own parallax and anchored to the room, or it runs down the lens
 * instead. */
static void roof_rain(const LevelArtScene *s, const LevelThemeArt *art,
                      const ArtRoom *room, float y0, float y1)
{
    SDL_Renderer *r = s->renderer;
    const float span = (float)s->win_w + 74.0f;
    const float fall = y1 - y0;
    if (fall < 8.0f)
        return;
    int count = (int)(34.0f * fall / 512.0f) + 3;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, art->haze.r, art->haze.g, art->haze.b, 60);
    for (int drop = 0; drop < count; ++drop)
    {
        unsigned h = art_hash(drop * 23 + (int)(room->seed & 511u), 907);
        float x = (float)fx_spread(h, span) - s->cam_x * 0.18f;
        x = x - span * floorf(x / span) - 37.0f;
        if (x < room->clip_left - 1.0f || x > room->clip_right)
            continue;
        float speed = 40.0f + (float)(h % 60u);
        float len = 7.0f + (float)(h % 6u);
        float y = y0 + fmodf(s->time * speed + (float)((h >> 7) % 400u), fall);
        fx_fill(r, x, y, 1.0f, fminf(len, y1 - y));
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* A two-pixel steel member between two points. */
static void roof_member(SDL_Renderer *r, SDL_Color c, float x0, float y0,
                        float x1, float y1)
{
    fx_set(r, c);
    SDL_RenderLine(r, x0, y0, x1, y1);
    SDL_RenderLine(r, x0 + 1.0f, y0, x1 + 1.0f, y1);
}

/*
 * The rooftop itself: a curtain wall of storey-high panes on a concrete
 * upstand, hung from a lattice truss under the roof slab, with bracing
 * standing in front of it. Every piece of that is on the room; only what is
 * through the glass is not.
 */
static void roof_rooftop(const LevelArtScene *s, const LevelThemeArt *art,
                         const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float top = room->top;
    const float floor = room->floor;
    const float w = room->clip_right - room->clip_left;
    const float upstand = floor - 22.0f;
    const float truss = top + 42.0f;

    roof_view(s, art, room->clip_left, room->clip_right, room->clip_top,
              room->clip_bottom);

    /* The glazing: mullions and the veil of the pane between them. */
    SDL_Color frame = art->near_shape;
    SDL_Color frame_lit = fx_mix(art->near_shape, art->trim_hi, 0.4f);
    int bay = art_repeat(s->cam_x, 0.18f, 74.0f);
    float first = roof_first(room->clip_left,
                             art_scroll(s->cam_x, 0.18f, 74.0f), 74.0f, 74.0f,
                             &bay);
    for (float x = first; x < room->clip_right; x += 74.0f, ++bay)
    {
        fx_rect_a(r, art->trim_hi, 12, x + 14.0f, truss, 30.0f,
                  upstand - truss);
        fx_rect_a(r, art->trim_hi, 7, x + 48.0f, truss, 8.0f, upstand - truss);
    }
    roof_rain(s, art, room, truss, upstand);
    /* Transoms a storey apart, counted up from the upstand. */
    for (float y = upstand - 96.0f; y > truss + 30.0f; y -= 96.0f)
    {
        fx_rect(r, frame, room->clip_left, y, w, 6.0f);
        fx_rect(r, fx_mix(frame, art->trim_hi, 0.3f), room->clip_left, y, w,
                2.0f);
        fx_rect_a(r, FX_INK, 60, room->clip_left, y + 6.0f, w, 2.0f);
    }
    for (float x = first; x < room->clip_right; x += 74.0f)
    {
        fx_rect(r, frame, x, truss, 8.0f, upstand - truss);
        fx_rect(r, frame_lit, x + 1.0f, truss, 2.0f, upstand - truss);
        fx_rect(r, fx_mix(frame, FX_INK, 0.35f), x + 6.0f, truss, 2.0f,
                upstand - truss);
    }

    /* The upstand the glass stands on: a concrete kerb with its coping. */
    SDL_Color kerb = fx_mix(art->wall_dark, art->air_bottom, 0.3f);
    fx_rect(r, kerb, room->clip_left, upstand, w, floor - upstand);
    fx_rect(r, fx_mix(art->trim, art->near_shape, 0.4f), room->clip_left,
            upstand, w, 4.0f);
    fx_rect(r, fx_mix(art->trim_hi, art->near_shape, 0.5f), room->clip_left,
            upstand, w, 1.0f);
    fx_rect_a(r, FX_INK, 70, room->clip_left, upstand + 4.0f, w, 3.0f);
    for (float x = first; x < room->clip_right; x += 74.0f)
    {
        /* Pour joints under each mullion, and a drainage slot between. */
        fx_rect_a(r, FX_INK, 60, x + 3.0f, upstand + 4.0f, 1.0f,
                  floor - upstand - 4.0f);
        fx_rect_a(r, FX_INK, 90, x + 30.0f, floor - 8.0f, 14.0f, 3.0f);
    }

    /* The truss the curtain wall hangs from: two chords and a Warren web,
     * on the mullions' plane so a node lands on every mullion. It is lit
     * from underneath by the glow coming through the glass. */
    SDL_Color steel = fx_mix(art->near_shape, FX_INK, 0.25f);
    SDL_Color under = fx_mix(art->near_shape, FX_SODIUM, 0.18f);
    fx_rect(r, fx_mix(art->air_top, FX_INK, 0.3f), room->clip_left, top, w,
            truss - top);
    for (float x = first; x < room->clip_right; x += 74.0f)
    {
        roof_member(r, steel, x + 3.0f, top + 7.0f, x + 40.0f, truss - 3.0f);
        roof_member(r, steel, x + 40.0f, truss - 3.0f, x + 77.0f, top + 7.0f);
        fx_rect(r, steel, x + 3.0f, top + 5.0f, 3.0f, truss - top - 6.0f);
    }
    fx_rect(r, steel, room->clip_left, top + 3.0f, w, 4.0f);
    fx_rect(r, fx_mix(steel, art->trim_hi, 0.2f), room->clip_left, top + 3.0f,
            w, 1.0f);
    fx_rect(r, steel, room->clip_left, truss - 4.0f, w, 5.0f);
    fx_rect(r, under, room->clip_left, truss, w, 1.0f);
    fx_rect_a(r, FX_INK, 60, room->clip_left, truss + 1.0f, w, 3.0f);

    /* Bracing in front of the glass, close and unlit: a tier of crosses a
     * storey or two high, so a tall room gets more tiers rather than one
     * cross stretched thin. */
    const float span = upstand - truss;
    int tiers = (int)floorf(span / 200.0f + 0.5f);
    if (tiers < 1)
        tiers = 1;
    const float tier_h = span / (float)tiers;
    SDL_Color brace = fx_mix(art->near_shape, FX_INK, 0.4f);
    int cross = art_repeat(s->cam_x, 0.32f, 220.0f);
    for (float x = roof_first(room->clip_left,
                              art_scroll(s->cam_x, 0.32f, 220.0f), 220.0f,
                              112.0f, &cross);
         x < room->clip_right; x += 220.0f, ++cross)
    {
        for (int tier = 0; tier < tiers; ++tier)
        {
            float y0 = truss + tier_h * (float)tier;
            float y1 = y0 + tier_h;
            roof_member(r, brace, x, y0, x + 108.0f, y1);
            roof_member(r, brace, x + 108.0f, y0, x, y1);
            fx_rect(r, brace, x, y1 - 2.0f, 110.0f, 3.0f);
            /* The gusset where the two diagonals cross. */
            fx_rect(r, fx_mix(brace, art->near_shape, 0.5f), x + 51.0f,
                    (y0 + y1) * 0.5f - 3.0f, 8.0f, 6.0f);
        }
        fx_rect(r, brace, x, truss, 3.0f, span);
        fx_rect(r, brace, x + 108.0f, truss, 3.0f, span);
    }
}

/*
 * The service level under the roof: a two-tile concrete void the plant is
 * run through. Pipework along its soffit, caged bulkheads, and the curtain
 * wall's rhythm carried down as a ribbon of glass and louvres with the same
 * city beyond it.
 */
static void roof_service(const LevelArtScene *s, const LevelThemeArt *art,
                         const ArtRoom *room)
{
    SDL_Renderer *r = s->renderer;
    const float top = room->top;
    const float floor = room->floor;
    const float w = room->clip_right - room->clip_left;
    const float louvre_top = top + fminf(24.0f, room->height * 0.36f);
    const float louvre_bottom = floor - fminf(12.0f, room->height * 0.18f);

    /* Board-marked concrete: the pour's courses counted up from the floor,
     * so the rooms either side of a partition share them. */
    SDL_Color concrete = fx_mix(art->air_bottom, art->wall_dark, 0.55f);
    fx_rect(r, concrete, room->clip_left, top, w, room->height);
    fx_vgrad(r, room->clip_left, top, w, room->height, FX_INK, 60, FX_INK, 0);
    for (float y = floor - 9.0f; y > top; y -= 9.0f)
        fx_rect_a(r, FX_INK, 34, room->clip_left, y, w, 1.0f);

    /* The curtain wall's rhythm carried down as a ribbon of glass, one bay
     * in three a louvred vent instead: the same city beyond both, and which
     * bay breathes belongs to the bay. */
    int bay0 = art_repeat(s->cam_x, 0.18f, 74.0f);
    float first = roof_first(room->clip_left,
                             art_scroll(s->cam_x, 0.18f, 74.0f), 74.0f, 74.0f,
                             &bay0);
    SDL_Color frame = fx_mix(art->near_shape, concrete, 0.3f);
    int bay = bay0;
    for (float x = first; x < room->clip_right; x += 74.0f, ++bay)
    {
        float px = x + 8.0f;
        roof_view_in(s, art, room, px, louvre_top, px + 66.0f, louvre_bottom);
        if (bay % 3 != 0)
        {
            fx_rect_a(r, art->trim_hi, 12, px + 6.0f, louvre_top, 26.0f,
                      louvre_bottom - louvre_top);
            fx_rect_a(r, art->trim_hi, 7, px + 40.0f, louvre_top, 8.0f,
                      louvre_bottom - louvre_top);
        }
    }
    roof_rain(s, art, room, louvre_top, louvre_bottom);
    bay = bay0;
    for (float x = first; x < room->clip_right; x += 74.0f, ++bay)
    {
        float px = x + 8.0f;
        if (bay % 3 == 0)
        {
            for (float y = louvre_top + 1.0f; y < louvre_bottom - 1.0f;
                 y += 5.0f)
            {
                fx_rect(r, fx_mix(frame, FX_INK, 0.3f), px, y, 66.0f, 2.0f);
                fx_rect(r, fx_mix(frame, art->trim_hi, 0.18f), px, y, 66.0f,
                        1.0f);
            }
        }
        fx_rect(r, frame, x, louvre_top - 2.0f, 74.0f, 2.0f);
        fx_rect(r, fx_mix(frame, art->trim_hi, 0.25f), x, louvre_top - 2.0f,
                74.0f, 1.0f);
        fx_rect(r, frame, x, louvre_bottom, 74.0f, 2.0f);
        fx_rect(r, frame, x, louvre_top - 2.0f, 8.0f,
                louvre_bottom - louvre_top + 4.0f);
        fx_rect(r, fx_mix(frame, art->trim_hi, 0.3f), x + 1.0f, louvre_top,
                1.0f, louvre_bottom - louvre_top);
        /* Form ties in the concrete under the band. */
        fx_rect_a(r, FX_INK, 90, x + 24.0f, floor - 6.0f, 2.0f, 2.0f);
        fx_rect_a(r, FX_INK, 90, x + 60.0f, floor - 6.0f, 2.0f, 2.0f);
    }

    /* The pipework along the soffit: two runs on brackets, lit along the top
     * by the bulkheads under them. */
    for (int run = 0; run < 2; ++run)
    {
        float py = top + 3.0f + (float)run * 7.0f;
        float d = run == 0 ? 5.0f : 4.0f;
        SDL_Color pipe = run == 0 ? fx_mix(art->near_shape, art->trim, 0.25f)
                                  : fx_mix(art->near_shape, art->wall, 0.3f);
        if (py + d > louvre_top - 3.0f)
            break;
        fx_rect(r, pipe, room->clip_left, py, w, d);
        fx_rect(r, fx_mix(pipe, art->trim_hi, 0.35f), room->clip_left, py, w,
                1.0f);
        fx_rect(r, fx_mix(pipe, FX_INK, 0.45f), room->clip_left, py + d - 1.0f,
                w, 1.0f);
    }
    int bracket = art_repeat(s->cam_x, 0.18f, 74.0f);
    for (float x = roof_first(room->clip_left,
                              art_scroll(s->cam_x, 0.18f, 74.0f), 74.0f, 74.0f,
                              &bracket);
         x < room->clip_right; x += 74.0f, ++bracket)
    {
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.3f), x + 36.0f, top, 2.0f,
                fminf(15.0f, louvre_top - top - 3.0f));
        if ((bracket & 1) == 0)
        {
            /* A flange on each run. */
            fx_rect(r, fx_mix(art->near_shape, art->trim_hi, 0.2f), x + 10.0f,
                    top + 2.0f, 3.0f, 7.0f);
            fx_rect(r, fx_mix(art->near_shape, art->trim_hi, 0.2f), x + 10.0f,
                    top + 9.0f, 3.0f, 6.0f);
        }
        else
        {
            /* A caged bulkhead on the frame between two panels, and the pool
             * it throws on the wall under it. */
            float lx = x + 1.0f;
            float ly = louvre_top + 4.0f;
            fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.2f), lx - 1.0f,
                    ly - 1.0f, 8.0f, 8.0f);
            fx_rect(r, art->lamp, lx, ly, 6.0f, 6.0f);
            fx_rect_a(r, FX_INK, 150, lx + 2.0f, ly, 1.0f, 6.0f);
            fx_rect_a(r, FX_INK, 150, lx, ly + 3.0f, 6.0f, 1.0f);
            fx_glow(r, lx + 3.0f, ly + 3.0f, 22.0f, art->lamp, 50);
            fx_light_cone(r, lx + 3.0f, ly + 6.0f, 4.0f, 30.0f,
                          floor - ly - 6.0f, art->lamp, 18);
        }
    }
}

/* One room of the ROOF theme: see `ArtRoom` for what a room layout owes. */
static void backdrop_roof_room(const LevelArtScene *s, const LevelThemeArt *art,
                               const ArtRoom *room)
{
    if (room->height >= 160.0f)
        roof_rooftop(s, art, room);
    else
        roof_service(s, art, room);
}

/* ---- Exterior backdrops ---------------------------------------------- */

/*
 * The five climbs share one building: the same masonry shell, floor bands and
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
            /* Broad weathering over the face: a wall that has been outside
             * for sixty years is cleaner where the rain scours it and
             * darker where it does not, in patches much bigger than any
             * one block. One smooth value per block keeps it a surface
             * rather than a speckle. */
            float drift = art_drift(block * 2, course / 2);
            if (drift > 0.56f)
                fx_rect_a(r, art->trim, (Uint8)((drift - 0.56f) * 150.0f),
                          left, y, right - left, 16.0f);
            else if (drift < 0.44f)
                fx_rect_a(r, FX_INK, (Uint8)((0.44f - drift) * 180.0f), left,
                          y, right - left, 16.0f);
            /* A block re-cut at some point, paler than its neighbours; and
             * the odd arris spalled away at a corner. */
            if ((bh % 41u) == 7u)
                fx_rect_a(r, art->trim, 44, left + 1.0f, y + 1.0f,
                          right - left - 2.0f, 15.0f);
            if ((bh % 17u) == 3u && x >= face_left)
            {
                fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.3f), x + 1.0f,
                        y + 1.0f, 3.0f, 2.0f);
                fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.3f), x + 1.0f,
                        y + 3.0f, 1.0f, 2.0f);
                fx_rect_a(r, art->wall_light, 70, x + 4.0f, y + 1.0f, 1.0f,
                          2.0f);
            }
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
    for (float px = art_scroll(s->cam_x, 1.0f, 512.0f) - 512.0f;
         px < (float)s->win_w; px += 512.0f)
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

/*
 * How far the backdrop has sunk by the time the camera has climbed this high.
 * The rule, and the two bugs that made it one, are on `level_backdrop_sink` in
 * [level.h](level.h): it is over there because a climb is the only place the
 * camera moves on this axis, and a function in here is a function no test can
 * reach.
 */
static float facade_sink(const LevelArtScene *s, float factor)
{
    return level_backdrop_sink(&s->level->map, s->cam_y,
                               (float)s->win_h - HUD_HEIGHT, factor);
}

/*
 * Distant towers, at a parallax slow enough to sell the height, in two ranks.
 *
 * One rank of flat slabs at the value of the sky is what this was, and at that
 * value a tower is not there at all: its windows float. Each tower keeps a rim
 * on its roofline and on the flank the moon reaches — the moon is up and to the
 * left on every wall out here — some are stepped at the crown and carry a mast
 * with its aviation light, and a second rank nearer and darker stands in front
 * of them, sinking faster as the climb rises. Depth out here is two planes a
 * step apart and nothing else, because the sky strips either side of the wall
 * are all of the city a climber gets to see.
 */
static void facade_skyline(const LevelArtScene *s, const LevelThemeArt *art,
                           float lit_alpha)
{
    SDL_Renderer *r = s->renderer;
    float skyline_shift = facade_sink(s, 0.14f);
    SDL_Color rim = fx_mix(art->far_shape, art->haze, 0.16f);
    int tower = art_repeat(s->cam_x, 0.08f, 104.0f);
    for (float x = art_scroll(s->cam_x, 0.08f, 104.0f);
         x < (float)s->win_w; x += 104.0f, ++tower)
    {
        unsigned h = art_hash(tower, 307);
        float tower_w = 74.0f + (float)(h % 58u);
        float tower_h = 100.0f + (float)((h >> 7) % 180u);
        float y = (float)s->win_h - tower_h + skyline_shift;
        fx_rect(r, art->far_shape, x, y, tower_w, tower_h);
        fx_rect(r, rim, x, y, tower_w, 1.0f);
        fx_rect(r, rim, x, y, 1.0f, tower_h);
        fx_hgrad(r, x + tower_w - 18.0f, y, 18.0f, tower_h, FX_INK, 0, FX_INK,
                 50);
        /* A setback crown on some, and a mast with its light on the tallest:
         * a skyline of flat tops is a bar chart. */
        if ((h >> 15) & 1u)
        {
            float cw = tower_w * 0.55f;
            float ch = 12.0f + (float)((h >> 17) % 18u);
            fx_rect(r, art->far_shape, x + 6.0f, y - ch, cw, ch);
            fx_rect(r, rim, x + 6.0f, y - ch, cw, 1.0f);
            fx_rect(r, rim, x + 6.0f, y - ch, 1.0f, ch);
            if (tower_h > 200.0f)
            {
                float mx = x + 6.0f + cw * 0.5f;
                fx_rect(r, fx_mix(art->far_shape, art->haze, 0.1f), mx,
                        y - ch - 22.0f, 1.0f, 22.0f);
                float pulse = art_pulse(
                    s,
                    fmodf(s->time * 0.7f + art_unit(h, 21) * 3.0f, 1.4f) < 0.2f
                        ? 1.0f
                        : 0.25f,
                    0.36f);
                fx_rect(r, fx_dim(FX_RED, 0.8f * pulse), mx - 1.0f,
                        y - ch - 24.0f, 3.0f, 2.0f);
                fx_glow(r, mx, y - ch - 23.0f, 8.0f, FX_RED,
                        (Uint8)(60.0f * pulse));
            }
        }
        if (lit_alpha <= 0.0f)
            continue;
        /* Windows stop at the tower's own foot rather than at the bottom of
         * the frame: the two are the same line only while the skyline is
         * sitting on the bottom edge, which is one moment of one climb. */
        float sill = y + tower_h - 6.0f;
        float limit = sill < (float)s->win_h ? sill : (float)s->win_h;
        int floor_index = 0;
        for (float wy = y + 18.0f; wy < limit; wy += 28.0f, ++floor_index)
        {
            /* The spandrel between one floor and the next. */
            fx_rect_a(r, FX_INK, 50, x + 1.0f, wy + 12.0f, tower_w - 1.0f,
                      2.0f);
            int bay = 0;
            for (float wx = x + 13.0f; wx < x + tower_w - 8.0f;
                 wx += 22.0f, ++bay)
            {
                /* Keyed to the tower, floor and bay rather than to where the
                 * window currently is on screen. `(int)(wx + wy)` is a screen
                 * position, and the skyline's own parallax moves it: measured
                 * over a climb it repainted every light on the city about
                 * thirty times a second, some two thousand windows changing
                 * state per second, and only while the camera moved. The three
                 * other backdrops that draw a city -- the lobby's, the
                 * office's and the roof's -- have keyed theirs this way for
                 * years, one of them saying in writing that hashing the screen
                 * position "made the whole skyline switch its lights while
                 * Chuck walked". This is the one that never got it, behind the
                 * only sector type with nothing else on the glass. */
                unsigned wh = art_hash(tower * 7 + bay, floor_index + 307);
                if ((wh % 3u) != 0u)
                    continue;
                fx_rect_a(r, art->near_shape, (Uint8)lit_alpha, wx, wy,
                          5.0f, 8.0f);
            }
        }
    }

    /* The nearer rank: lower, darker, and sinking faster, with only the odd
     * warm window lit in it. */
    float near_shift = facade_sink(s, 0.22f);
    SDL_Color near_body = fx_mix(art->far_shape, FX_INK, 0.45f);
    SDL_Color near_rim = fx_mix(near_body, art->haze, 0.12f);
    int block = art_repeat(s->cam_x, 0.12f, 136.0f);
    for (float x = art_scroll(s->cam_x, 0.12f, 136.0f) - 60.0f;
         x < (float)s->win_w; x += 136.0f, ++block)
    {
        unsigned h = art_hash(block, 1231);
        float bw = 90.0f + (float)(h % 50u);
        float bh = 60.0f + (float)((h >> 6) % 90u);
        float y = (float)s->win_h - bh + near_shift + 20.0f;
        if (y >= (float)s->win_h)
            continue;
        fx_rect(r, near_body, x, y, bw, bh);
        fx_rect(r, near_rim, x, y, bw, 1.0f);
        fx_rect(r, near_rim, x, y, 1.0f, bh);
        /* Rooftop plant on the near blocks: a water tank on its legs, or a
         * lift motor room. */
        if ((h >> 12) & 1u)
        {
            float tx = x + 12.0f + (float)((h >> 13) % 40u);
            fx_rect(r, near_body, tx, y - 14.0f, 14.0f, 10.0f);
            fx_rect(r, near_rim, tx, y - 14.0f, 14.0f, 1.0f);
            fx_rect(r, near_body, tx + 2.0f, y - 4.0f, 1.0f, 4.0f);
            fx_rect(r, near_body, tx + 11.0f, y - 4.0f, 1.0f, 4.0f);
        }
        if (lit_alpha <= 0.0f)
            continue;
        int floor_index = 0;
        for (float wy = y + 10.0f; wy < (float)s->win_h && wy < y + bh - 6.0f;
             wy += 18.0f, ++floor_index)
        {
            for (int bay = 0; bay < 6; ++bay)
            {
                unsigned wh = art_hash(block * 11 + bay, floor_index + 1237);
                if ((wh % 7u) != 0u)
                    continue;
                fx_rect_a(r, FX_WARM, (Uint8)(lit_alpha * 0.35f),
                          x + 8.0f + (float)bay * 14.0f, wy, 4.0f, 5.0f);
            }
        }
    }
}

/*
 * The cordon, seen from the wall.
 *
 * The demand broadcast at 00:04 was theatre, and this is what it bought: every
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
 * falls away with height, not with the hour — the whole night is thirty-eight
 * minutes long and the ring was already standing before any of it: the third
 * sector is the lowest wall and still in the thick of it, the storm climb is
 * higher and wetter, and by the moon climb it is a suggestion. Above the
 * weather there is nothing to see at all.
 */
static void facade_cordon(const LevelArtScene *s, float top, float height,
                          float strength)
{
    if (strength <= 0.0f)
        return;
    SDL_Renderer *r = s->renderer;
    /* The blue half of a light bar is FX_CORDON_BLUE — it lived here as a
     * file-local until the press cover's cordon needed the same light; see
     * the note beside it in fx.h. The red half is FX_RED, which is exactly
     * what the palette's danger red is for. */

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
        /* Held at the pulse's own mean when the player has asked for it: the
         * triangle is on for 0.34 of the period and averages half of that, so
         * a steady 0.17 puts the same amount of light on the same wall
         * without the bar ever being a strobe. */
        float flash = 0.17f;
        if (!s->steady_lights)
        {
            if (beat > 0.34f)
                continue;
            flash = 1.0f - beat / 0.34f;
        }
        SDL_Color c = red_half ? FX_RED : FX_CORDON_BLUE;
        fx_glow(r, x, floor_y + 30.0f, 150.0f + (float)(h % 60u), c,
                (Uint8)(52.0f * flash * strength));
    }
    /* And the light that actually lands on the wall: a cold rise off the
     * bottom edge, steady, because the sum of a dozen bars is steady even
     * where each one of them is not. */
    fx_vgrad(r, 0.0f, floor_y - 130.0f, (float)s->win_w, 130.0f,
             FX_CORDON_BLUE, 0, FX_CORDON_BLUE, (Uint8)(30.0f * strength));
}

/*
 * The one aircraft the cordon lets near the tower.
 *
 * A demand goes out on the wire at 00:04 and a news ship is over the block
 * within the hour; a police one would be a problem, because the helicopter on
 * this roof at the end of the night is the crew's ride out and nothing in the
 * sky can be allowed to contradict that. So it holds station well off the
 * building with its beacon going, drifts slowly, and passes behind the face
 * Chuck is climbing rather than over it — which is why it is drawn before the
 * shell and not after.
 *
 * The hull is laid out tail-to-nose along +x, which is the direction the
 * traverse below runs: the boom, the fin and the cabin door were all drawn
 * pointing the other way, so the one aircraft in the sky flew tail-first for
 * the whole of every climb. A nose and a tail boom are the only thing on this
 * layer that says which way an aircraft is going, so reversing the traverse
 * means mirroring these offsets with it.
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

    /* Cabin, then the boom and fin trailing behind it, then the skids, all in
     * the depth haze's own dark so it sits at the same distance as the towers
     * behind it. */
    SDL_Color hull = fx_mix(FX_SHADOW, FX_STEEL_DK, 0.45f);
    fx_rect(r, hull, x - 8.0f, y - 3.0f, 15.0f, 6.0f);
    fx_rect(r, hull, x - 20.0f, y - 1.0f, 13.0f, 2.0f);
    fx_rect(r, fx_mix(hull, FX_STEEL_LT, 0.3f), x - 20.0f, y - 5.0f, 2.0f,
            6.0f);
    fx_rect(r, hull, x - 6.0f, y + 3.0f, 12.0f, 1.0f);
    /* The rotor is a blur, not blades: two blades at this size would strobe. */
    fx_rect_a(r, fx_mix(hull, FX_PALE, 0.4f), 90, x - 15.0f, y - 6.0f,
              32.0f, 1.0f);
    /* Anti-collision beacon underneath, and the cabin light in the door where
     * somebody is leaning out of it with a camera. */
    float beacon = fmodf(s->time * 1.35f, 1.0f);
    float flash = art_pulse(s, beacon < 0.16f ? 1.0f - beacon / 0.16f : 0.0f,
                            0.08f);
    if (flash > 0.0f)
    {
        Uint8 a = (Uint8)(flash * 190.0f);
        fx_rect_a(r, FX_RED, a, x - 1.0f, y + 4.0f, 2.0f, 2.0f);
        fx_glow(r, x, y + 5.0f, 13.0f, FX_RED, (Uint8)(a / 2u));
    }
    fx_rect_a(r, FX_WARM, 130, x + 1.0f, y - 2.0f, 3.0f, 3.0f);
    fx_glow(r, x + 3.0f, y - 1.0f, 16.0f, FX_WARM, 34);
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
        /* A full-frame flash is exactly the strobe reduced motion exists to
         * take away, and it averages to next to nothing over its period, so
         * a steady sky simply has no lightning in it. */
        flash = art_pulse(s, flash, 0.0f);
        if (flash > 0.0f)
            fx_rect_a(r, STORM_FLASH,
                      (Uint8)(70.0f * flash), 0.0f, top, (float)s->win_w,
                      height);
        facade_skyline(s, art, 40.0f);
        /* Low cloud drifting across the towers. It was four ruled bands the
         * width of the frame, which read as stripes painted on the sky: a
         * cloud deck is torn into masses of its own, soft at both edges,
         * darker underneath, and lit along the top by the discharge when it
         * comes. Each mass belongs to its place on the band's own drift, so
         * the deck slides rather than reshuffling. */
        SDL_Color cloud = fx_mix(FX_SHADOW, FX_STEEL_DK, 0.8f);
        for (int band = 0; band < 4; ++band)
        {
            float y = top + 40.0f + (float)band * 74.0f +
                      facade_sink(s, 0.05f);
            float thick = 26.0f + (float)band * 7.0f;
            float drift = s->time * (7.0f + (float)band * 3.0f);
            const float seg = 70.0f;
            int first = (int)floorf(drift / seg) - 2;
            for (int k = first; k < first + (int)((float)s->win_w / seg) + 5;
                 ++k)
            {
                unsigned ch = art_hash(k, 1301 + band);
                if ((ch % 5u) == 0u)
                    continue; /* a tear in the deck */
                float cx = (float)k * seg - drift + (float)(ch % 20u) - 40.0f;
                float cw = seg + 20.0f + (float)((ch >> 5) % 50u);
                float lift = (float)((ch >> 11) % 12u) - 6.0f;
                float ch_h = thick + (float)((ch >> 15) % 12u);
                float cy = y + lift;
                art_mass_a(r, cloud, 90, cx + cw * 0.25f, cy - 7.0f,
                           cw * 0.45f, 9.0f, 4, 0);
                art_mass_a(r, cloud, 90, cx, cy + 2.0f, cw, ch_h - 10.0f, 7,
                           0);
                fx_vgrad(r, cx, cy + ch_h - 8.0f, cw, 10.0f, cloud, 90,
                         FX_INK, 0);
                if (flash > 0.0f)
                    fx_rect_a(r, STORM_FLASH, (Uint8)(60.0f * flash), cx + 4.0f,
                              cy - 6.0f, cw - 8.0f, 2.0f);
            }
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
    case BACKDROP_FACADE_MOON:
    {
        /*
         * A low moon just off the corner of the building, and the towers go
         * to silhouette against it.
         *
         * This was a sunrise for a long time, and it was the one thing in the
         * game that contradicted the clock outright: the night runs 00:22 to
         * 01:00, this is sector 11, and the wall clocks in the sectors either
         * side of it read 00:42 and 00:46. A player who
         * looked up in sector 10, climbed through a dawn and looked up again
         * in sector 12 was told the sun had risen and set inside five minutes.
         * (Both readings were written here as `00:47` while the night divided
         * fifteen ways; `check_docs.py` derives them from `NIGHT_CLOCK_*` now,
         * so the next campaign that grows moves this comment with it.)
         *
         * What the beat is actually for survives the change intact — one
         * climb lit from the side by a single round source off the corner,
         * against three that are not — so this keeps the composition and
         * swaps the light. A moon is cold where the sun was warm, and it has
         * a **hard edge**, which is most of what tells the two apart at this
         * size: the sun was drawn as haze alone and would still read as a sun
         * however blue it was painted.
         */
        float moon_x = (float)s->win_w * 0.78f;
        float moon_y = (float)s->win_h * 0.72f;
        /* Cold light, out of the palette's own cool end rather than out of
         * new literals: FX_STEEL_LT for the far halo, FX_PALE for the near
         * one. Not FX_LAMP — that is the fluorescent tube, and hanging one in
         * the sky is exactly what the palette's three light temperatures
         * exist to stop. */
        fx_glow(r, moon_x, moon_y, 300.0f, FX_STEEL_LT, 90);
        fx_glow(r, moon_x, moon_y, 120.0f, FX_PALE, 120);
        /* The disc. Cut hard at the corners the way the wall clock's face is,
         * so it comes out round in the game's own drawing vocabulary rather
         * than as a rasterised circle nothing else in the tree draws. */
        fx_mass(r, fx_mix(FX_PALE, FX_STEEL, 0.35f),
                moon_x - 21.0f, moon_y - 21.0f, 42.0f, 42.0f, 13, 13);
        fx_mass(r, fx_mix(FX_PALE, FX_CREAM, 0.55f),
                moon_x - 19.0f, moon_y - 19.0f, 38.0f, 38.0f, 12, 12);
        /* Two maria, so the disc is a body and not a hole in the sky. */
        fx_rect_a(r, FX_STEEL_LT, 70, moon_x - 9.0f, moon_y - 7.0f,
                  11.0f, 8.0f);
        fx_rect_a(r, FX_STEEL_LT, 55, moon_x + 3.0f, moon_y + 4.0f,
                  7.0f, 6.0f);
        facade_skyline(s, art, 0.0f);
        /* Haze lying on the air near the ground, lit from the side by the
         * moon. It is what keeps the lower sky from going flat now the light
         * is cold, and it is why there are no stars out here: they are up in
         * the HIGH climb, above all of this. */
        for (int band = 0; band < 3; ++band)
        {
            float y = (float)s->win_h - 150.0f + (float)band * 34.0f;
            fx_rect_a(r, fx_mix(FX_PALE, FX_STEEL_LT, 0.45f), 46, 0.0f, y,
                      (float)s->win_w, 12.0f);
        }
        facade_shell(s, art, top, height);
        /* Birds off the ledges, far enough out to be scenery rather than the
         * gameplay kind — the same population the climb throws at Chuck up
         * close, put out where it is only weather. Cold against a cold sky:
         * a warm dark out here would be the sunrise back again in one line. */
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
            fx_set(r, fx_mix(FX_SHADOW, FX_STEEL_DK, 0.45f));
            SDL_RenderLine(r, x - 5.0f, y - flap, x, y);
            SDL_RenderLine(r, x, y, x + 5.0f, y - flap);
        }
        break;
    }
    case BACKDROP_FACADE_HIGH:
    {
        /* The one purple in the game, and it is deliberate: seen from this
         * high the sodium-and-neon city sums to a violet haze that no single
         * fixture in the palette owns — anchoring it to FX_LAMP or FX_CYAN
         * would hang a lamp in the sky. Owned here as one named trio (the
         * glow, the cloud it soaks, the cloud's lit tops) so the hue has
         * exactly one home and cannot multiply. */
        static const SDL_Color CITY_GLOW = {150, 130, 220, 255};
        static const SDL_Color CLOUD_BAND = {58, 56, 104, 255};
        static const SDL_Color CLOUD_BAND_LIT = {96, 92, 156, 255};
        /* Thin air, hard stars, and the city reduced to a glow coming up
         * between torn cloud rather than through a floor of it — see the note
         * on the theme: the climb above this one is sleet. */
        for (int i = 0; i < 60; ++i)
        {
            unsigned h = art_hash(i * 11 + 5, 733);
            float x = (float)fx_spread(h, (float)s->win_w);
            float y = top + fmodf((float)((h >> 8) % 900u) -
                                      s->cam_y * 0.05f + 900.0f,
                                  900.0f);
            fx_rect_a(r, art->haze, (Uint8)(110 + h % 120u), x, y,
                      (h & 7u) == 0u ? 2.0f : 1.0f, 1.0f);
        }
        float deck = (float)s->win_h - 90.0f + facade_sink(s, 0.1f);
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
            float py = deck + (float)((h >> 4) % 40u);
            /* A puff is a flat-bottomed body with two heads standing up out
             * of it, each head catching the high light along its crown and
             * the city glow soaking its underside. Slabs of it read as
             * bars of colour laid across the sky. */
            float l1x = x + w * (0.12f + art_unit(h, 9) * 0.1f);
            float l1w = w * 0.34f;
            float l2x = x + w * (0.5f + art_unit(h, 17) * 0.1f);
            float l2w = w * 0.28f;
            art_mass_a(r, CLOUD_BAND, 150, l1x, py - 10.0f, l1w, 10.0f, 6, 0);
            art_mass_a(r, CLOUD_BAND, 150, l2x, py - 6.0f, l2w, 6.0f, 4, 0);
            art_mass_a(r, CLOUD_BAND, 150, x, py, w, 30.0f, 8, 2);
            art_mass_a(r, CLOUD_BAND_LIT, 90, l1x + 3.0f, py - 10.0f,
                       l1w - 6.0f, 3.0f, 3, 0);
            art_mass_a(r, CLOUD_BAND_LIT, 90, l2x + 2.0f, py - 6.0f,
                       l2w - 4.0f, 3.0f, 2, 0);
            art_mass_a(r, CLOUD_BAND_LIT, 70, x + 6.0f, py, w - 12.0f, 3.0f,
                       4, 0);
            fx_vgrad(r, x + 4.0f, py + 18.0f, w - 8.0f, 12.0f, CITY_GLOW, 0,
                     CITY_GLOW, 34);
        }
        facade_shell(s, art, top, height);
        /* The building's own signage, mounted on the face he is climbing. */
        float sign_y = 260.0f + HUD_HEIGHT - s->cam_y;
        if (sign_y > top - 90.0f && sign_y < (float)s->win_h)
        {
            float buzz = art_pulse(
                s, fmodf(s->time * 3.1f, 6.0f) < 0.12f ? 0.35f : 1.0f, 1.0f);
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
            float x = (float)fx_spread(h, (float)s->win_w);
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

    /* Every climb keeps the same aircraft beacon so the five walls read as
     * the same tower at different hours. */
    float beacon = art_pulse(s, 0.45f + 0.55f * sinf(s->time * 2.2f), 0.45f);
    fx_glow(r, (float)s->win_w - 42.0f, top + 25.0f, 20.0f, art->accent,
            (Uint8)(35.0f + beacon * 55.0f));

    /* The street the cordon is standing in, last of all, because it is light
     * falling on the wall rather than something behind it. */
    /*
     * Asked of the **theme**, which is one row per climb, and not of the
     * backdrop, which is not — and asked of `level.c`, which the suite can
     * reach, rather than answered by a switch in here, which it cannot.
     *
     * The wash fades with height and that is the whole of what it says, so it
     * needs an answer for each of the five walls. Keyed on `art->backdrop` it
     * had four, because `FACADE_SLEET` borrows the storm's: the highest climb
     * in the game, two floors under the roof, washed its face with the storm's
     * 0.60 and therefore showed *more* street than the two climbs below it.
     * Nothing failed — the picture simply stopped meaning what
     * [../docs/story.md](../docs/story.md) says it means, which is that the
     * climb is also a climb away from the cordon.
     */
    facade_cordon(s, top, height,
                  level_theme_cordon(s->level->map.theme));
}

/* ---- Entry point ----------------------------------------------------- */

/* The room of whichever theme this is. */
static void backdrop_room(const LevelArtScene *s, const LevelThemeArt *art,
                          const ArtRoom *room)
{
    switch (art->backdrop)
    {
    case BACKDROP_LOBBY:
        backdrop_lobby_room(s, art, room);
        break;
    case BACKDROP_OFFICE:
        backdrop_office_room(s, art, room);
        break;
    case BACKDROP_SERVER:
        backdrop_server_room(s, art, room);
        break;
    case BACKDROP_CANTEEN:
        backdrop_canteen_room(s, art, room);
        break;
    case BACKDROP_LAB:
        backdrop_lab_room(s, art, room);
        break;
    case BACKDROP_ARCHIVE:
        backdrop_archive_room(s, art, room);
        break;
    case BACKDROP_SECURITY:
        backdrop_security_room(s, art, room);
        break;
    case BACKDROP_DUCTS:
        backdrop_ducts_room(s, art, room);
        break;
    case BACKDROP_PENTHOUSE:
        backdrop_penthouse_room(s, art, room);
        break;
    case BACKDROP_ROOF:
        backdrop_roof_room(s, art, room);
        break;
    case BACKDROP_FACADE_NIGHT:
    case BACKDROP_FACADE_STORM:
    case BACKDROP_FACADE_MOON:
    case BACKDROP_FACADE_HIGH:
    case BACKDROP_RESTROOM:
        break; /* never reached: neither is drawn per room */
    case BACKDROP_PLANT:
    default:
        backdrop_plant_room(s, art, room);
        break;
    }
}

void level_art_backdrop(const LevelArtScene *s)
{
    const LevelThemeArt *art = level_art(s->level->map.theme);
    SDL_Renderer *r = s->renderer;
    const float oy = HUD_HEIGHT;
    const float fh = (float)s->win_h - oy;

    switch (art->backdrop)
    {
    case BACKDROP_FACADE_NIGHT:
    case BACKDROP_FACADE_STORM:
    case BACKDROP_FACADE_MOON:
    case BACKDROP_FACADE_HIGH:
        /* A climb is outside: one sky for the whole frame, sinking as the
         * wall rises (see `level_backdrop_sink`). */
        fx_rect(r, art->air_top, 0.0f, oy, (float)s->win_w, fh);
        fx_vgrad(r, 0.0f, oy, (float)s->win_w, fh, art->air_top, 255,
                 art->air_bottom, 255);
        backdrop_facade(s, art, oy, fh);
        return;
    case BACKDROP_RESTROOM:
        return; /* game_render.c derives the room from its own wall ring. */
    default:
        break;
    }

    /* Behind everything, the building's own structure: what a gap across a
     * slab — a ladder going through it, a pair of panels — shows, because a
     * hole in a floor is a hole and not another slice of somebody's wall. */
    fx_rect(r, fx_mix(art->air_top, FX_INK, 0.55f), 0.0f, oy, (float)s->win_w,
            fh);

    static LevelBackdropPlan plan;
    level_backdrop_plan(s->level, &plan);
    const float world_top = oy - s->cam_y;
    for (int i = 0; i < plan.piece_count; ++i)
    {
        const LevelBackdropPiece *piece = &plan.pieces[i];
        if (piece->room < 0)
            continue;
        float x0 = (float)piece->col0 * (float)TILE_SIZE - s->cam_x;
        float x1 = (float)(piece->col1 + 1) * (float)TILE_SIZE - s->cam_x;
        float y0 = (float)piece->top * (float)TILE_SIZE + world_top;
        float y1 = (float)(piece->bottom + 1) * (float)TILE_SIZE + world_top;
        float cx0 = fmaxf(x0, 0.0f);
        float cx1 = fminf(x1, (float)s->win_w);
        float cy0 = fmaxf(y0, oy);
        float cy1 = fminf(y1, (float)s->win_h);
        if (cx1 <= cx0 || cy1 <= cy0)
            continue;

        const LevelBackdropRoom *layout = &plan.rooms[piece->room];
        ArtRoom room;
        room.left = (float)layout->col0 * (float)TILE_SIZE - s->cam_x;
        room.right = (float)(layout->col1 + 1) * (float)TILE_SIZE - s->cam_x;
        room.top = (float)layout->top * (float)TILE_SIZE + world_top;
        room.floor = (float)layout->floor * (float)TILE_SIZE + world_top;
        room.height = room.floor - room.top;
        room.clip_left = cx0;
        room.clip_right = cx1;
        room.clip_top = cy0;
        room.clip_bottom = cy1;
        room.top_row = layout->top;
        room.floor_row = layout->floor;
        room.seed = art_hash(layout->floor * 131 + s->level_index,
                             layout->top);

        /* Whole pixels: a clip that lands between two is a one-pixel seam
         * down every partition. */
        SDL_Rect clip = {(int)floorf(cx0), (int)floorf(cy0),
                         (int)ceilf(cx1) - (int)floorf(cx0),
                         (int)ceilf(cy1) - (int)floorf(cy0)};
        SDL_SetRenderClipRect(r, &clip);
        room_air(s, art, &room);
        backdrop_room(s, art, &room);
        room_motes(s, art, &room);
        room_floor_haze(s, art, &room);
        SDL_SetRenderClipRect(r, NULL);
    }
}
