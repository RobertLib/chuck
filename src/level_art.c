#include "level_art.h"

#include <math.h>

#include "bbc.h"
#include "level_paint.h"

/*
 * Themed wall materials and backdrops, drawn as a BBC Micro in MODE 2 would.
 *
 * Everything in this file is presentation. It reads the immutable LevelMap to
 * know where solid tiles are and nothing else, so a theme can never change how
 * a level plays — only how the same floor plan feels to walk through.
 *
 * Every pixel goes through [bbc.h](bbc.h), so it lands on the 4x2 grid in one
 * of the eight colours, and every colour comes from the theme's row of
 * [level_paint.c](level_paint.c). What this file owns is the pattern: which
 * wall a theme is built of, what its walkway is finished in, and what stands
 * in the room behind it. A 32-pixel tile is eight BBC pixels by sixteen, which
 * is the squared paper every pattern below was laid out on, and every one of
 * them is keyed to the tile's world column and row so it slides with the
 * building rather than crawling under it.
 *
 * The rules the drawing keeps are the style guide's (docs/art-and-audio.md):
 * masonry is saturated and the lip a foot lands on is the brightest line on a
 * floor, because those are what a player reads a floor plan from; rooms are
 * black, with their own few lines drawn on them in the theme's backdrop colour;
 * and anything lit is lit sparingly, in flat colour, flashing where it has to
 * shout. Nothing glows, because nothing can.
 */

/* ---- Patterns ------------------------------------------------------- */

static const LevelThemeArt THEME_ART[LEVEL_THEME_COUNT] = {
    /* PLANT — the mechanical floor the game shipped with: cold steel
     * plating over a hall of half-lit machinery. */
    [LEVEL_THEME_PLANT] = {
        WALL_STYLE_PLATE, FLOOR_SCREED, BACKDROP_PLANT},

    /* LOBBY — the ground floor Chuck walks in through: stone, brass and a
     * glazed street front, the only sector still lit for the public. */
    [LEVEL_THEME_LOBBY] = {
        WALL_STYLE_MARBLE, FLOOR_STONE, BACKDROP_LOBBY},

    /* OFFICE — open plan after hours: painted partitions, blinds, and the
     * one bank of lights nobody switched off. */
    [LEVEL_THEME_OFFICE] = {
        WALL_STYLE_DRYWALL, FLOOR_CARPET, BACKDROP_OFFICE},

    /* SERVER — a cold aisle. Almost no white light; the room is lit by the
     * equipment it exists to hold. */
    [LEVEL_THEME_SERVER] = {
        WALL_STYLE_PLATE, FLOOR_PANEL, BACKDROP_SERVER},

    /* CANTEEN — glazed tile and stainless steel under heat lamps; the
     * warmest, brightest sector of the climb. */
    [LEVEL_THEME_CANTEEN] = {
        WALL_STYLE_TILE, FLOOR_CHEQUER, BACKDROP_CANTEEN},

    /* LAB — clean room: pale tile, green-lit cabinets, everything sterile
     * and slightly too bright. */
    [LEVEL_THEME_LAB] = {
        WALL_STYLE_TILE, FLOOR_CERAMIC, BACKDROP_LAB},

    /* ARCHIVE — old brick shell kept from the original building, filled
     * floor to ceiling with paper and dust. */
    [LEVEL_THEME_ARCHIVE] = {
        WALL_STYLE_BRICK, FLOOR_BOARDS, BACKDROP_ARCHIVE},

    /* SECURITY — the control wing: dark steel, a wall of monitors and a
     * standby beacon that never quite stops turning. */
    [LEVEL_THEME_SECURITY] = {
        WALL_STYLE_PLATE, FLOOR_CARPET, BACKDROP_SECURITY},

    /* DUCTS — the plenum between floors: galvanised trunking, no fixtures,
     * the darkest sector in the game. */
    [LEVEL_THEME_DUCTS] = {
        WALL_STYLE_PLATE, FLOOR_CHEQUER, BACKDROP_DUCTS},

    /* PENTHOUSE — the executive floor: hardwood panelling, sconces and
     * money, one storey below the roof. */
    [LEVEL_THEME_PENTHOUSE] = {
        WALL_STYLE_WOOD, FLOOR_PARQUET, BACKDROP_PENTHOUSE},

    /* ROOF — the last sector: raw concrete plant room behind a curtain wall
     * with the whole city on the other side of the glass. */
    [LEVEL_THEME_ROOF] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_ROOF},

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
        WALL_STYLE_PLATE, FLOOR_PANEL, BACKDROP_SERVER},

    /* RESTROOM — the sublevel. Its interior is derived from the room's own
     * wall ring in game_render.c; only the tile material comes from here. */
    [LEVEL_THEME_RESTROOM] = {
        WALL_STYLE_TILE, FLOOR_CERAMIC, BACKDROP_RESTROOM},

    /* ---- The five exterior climbs.  Same fields, exterior meanings:
     * air_* is sky, far_shape the distant towers, wall* the building face,
     * trim* the cornice stone, lamp a lit window, accent the signage.
     * Five entries and four backdrops: SLEET borrows the storm's. */

    /* NIGHT — the first climb: clear, cold, city lights below. */
    [LEVEL_THEME_FACADE_NIGHT] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_NIGHT},

    /* STORM — the second: rain, wet stone and lightning off the skyline. */
    [LEVEL_THEME_FACADE_STORM] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_STORM},

    /* MOON — the third: the storm has blown through, the sky has cleared, and
     * a low moon stands off the corner of the building. It is the one climb
     * lit from the side rather than from underneath, which is what makes it
     * read differently from the other three without moving the clock: the
     * whole night is thirty-eight minutes long (`NIGHT_CLOCK_*`), so the light
     * out here cannot come from the sun, and it does not. */
    [LEVEL_THEME_FACADE_MOON] = {
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_MOON},

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
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_HIGH},

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
        WALL_STYLE_CONCRETE, FLOOR_SCREED, BACKDROP_FACADE_STORM},
};

const LevelThemeArt *level_art(LevelTheme theme)
{
    if (theme < 0 || theme >= LEVEL_THEME_COUNT)
        return &THEME_ART[LEVEL_THEME_PLANT];
    return &THEME_ART[theme];
}

/* ---- Shared helpers -------------------------------------------------- */

/* One BBC pixel, across and down, in logical pixels. Every offset in this file
 * is a whole number of these, which is what keeps a shape in one piece when
 * the grid snaps it. */
#define PXW ((float)BBC_PX_W)
#define PXH ((float)BBC_PX_H)

static unsigned art_mix(unsigned value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

static unsigned art_hash(int x, int y)
{
    return art_mix((unsigned)x * 0x8da6b343u ^ (unsigned)y * 0xd8163841u);
}

/* Deterministic 0..1 from a hash slice, for per-tile and per-prop variation. */
static float art_unit(unsigned h, unsigned shift)
{
    return (float)((h >> shift) & 255u) / 255.0f;
}

/*
 * Which repeat of a run a position along it falls in.
 *
 * Everything a layer varies per repeat - which blind is shut, which window is
 * lit, what colour a file spine is - is keyed to this index, so the index has
 * to belong to the repeat rather than to where the repeat currently sits on
 * screen. Recovering it as `(int)(x + cam_x * factor) / period` looks like it
 * does: that sum is the repeat's world position, an exact multiple of the
 * period. In floats it lands a hair either side of the multiple instead, and
 * truncation then hands one repeat two different indices as the camera moves -
 * which is why the backdrop used to boil while the level scrolled rather than
 * sliding with it. Every run below takes its index from here and its position
 * from its index, never the other way round.
 */
static int art_repeat(float position, float period)
{
    return (int)floorf(position / period);
}

static float art_sx(float v)
{
    return bbc_snap_x(v);
}

static float art_sy(float v)
{
    return bbc_snap_y(v);
}

/* ---- The frame ------------------------------------------------------- */

/*
 * Everything a layer needs, with the camera already on the grid.
 *
 * The scene's camera is snapped before anything is laid out and the dither
 * patterns are anchored to world (0, 0) through it, so a wall, the room behind
 * it and the checkerboard on its carpet all move a whole BBC pixel at a time
 * and together. Without that the room slides a fraction of a pixel against the
 * slabs and every dither in it boils.
 */
typedef struct
{
    SDL_Renderer *r;
    const LevelArtScene *s;
    const Level *level;
    const LevelThemePaint *p;
    LevelTheme theme;
    float cam_x, cam_y;
    float ox, oy; /* where world (0, 0) lands on screen */
    float top;    /* the frame under the HUD */
    float bottom;
    float w;
    float t;
    bool steady;
} ArtCtx;

static ArtCtx art_ctx(const LevelArtScene *s)
{
    ArtCtx c;
    c.r = s->renderer;
    c.s = s;
    c.level = s->level;
    c.theme = s->level->map.theme;
    c.p = level_theme_paint(c.theme);
    c.cam_x = bbc_snap_x(s->cam_x);
    c.cam_y = bbc_snap_y(s->cam_y);
    c.ox = -c.cam_x;
    c.oy = HUD_HEIGHT - c.cam_y;
    c.top = HUD_HEIGHT;
    c.bottom = (float)s->win_h;
    c.w = (float)s->win_w;
    c.t = s->time;
    c.steady = s->steady_lights;
    return c;
}

static void art_world_pattern(const ArtCtx *c)
{
    bbc_set_pattern_origin(c->ox, c->oy);
}

/* A line one BBC pixel thick. */
static void art_hline(const ArtCtx *c, BbcColour col, float x0, float x1,
                      float y)
{
    if (x1 > x0)
        bbc_fill(c->r, col, x0, y, x1 - x0, PXH);
}

static void art_vline(const ArtCtx *c, BbcColour col, float x, float y0,
                      float y1)
{
    if (y1 > y0)
        bbc_fill(c->r, col, x, y0, PXW, y1 - y0);
}

/*
 * A lamp on the machine's own flash. `phase` picks which half of the beat it
 * is lit in, so a rack of them twinkles rather than pulsing as one lamp, and
 * under reduced motion `bbc_flash_pair` holds every one of them on its first
 * half: half the lamps lit and half dark, still, which is the same amount of
 * light on the wall and none of the blinking.
 */
static BbcColour art_blink(BbcColour on, unsigned phase)
{
    return (phase & 1u) != 0u ? bbc_flash_pair(on, BBC_BLACK)
                              : bbc_flash_pair(BBC_BLACK, on);
}

/* A beat of a lamp's own, for anything slower or rarer than the flash rate:
 * lit for `duty` of every period, and simply lit when the player has asked
 * for the lights to hold still. */
static bool art_beat(const ArtCtx *c, float rate, float phase, float duty)
{
    if (c->steady)
        return true;
    float b = c->t * rate + phase;
    b -= floorf(b);
    return b < duty;
}

static void art_clip(const ArtCtx *c, float x0, float y0, float x1, float y1)
{
    SDL_Rect rect = {(int)floorf(x0), (int)floorf(y0),
                     (int)ceilf(x1) - (int)floorf(x0),
                     (int)ceilf(y1) - (int)floorf(y0)};
    SDL_SetRenderClipRect(c->r, &rect);
}

/* ---- Walls ----------------------------------------------------------- */

/*
 * One tile on squared paper: eight BBC pixels by sixteen, its top-left on the
 * grid, and the world column and row every pattern on it is keyed to.
 */
typedef struct
{
    SDL_Renderer *r;
    const LevelThemePaint *p;
    float x, y;
    int col, row;
    unsigned h;
} ArtTile;

static void tile_px(const ArtTile *t, BbcColour c, int bx, int by, int bw,
                    int bh)
{
    bbc_block(t->r, c, t->x + (float)bx * PXW, t->y + (float)by * PXH, bw,
              bh);
}

/* Which of a tile's neighbours are open air: everything that decides where
 * the lip goes and which edges a mass shows. */
enum
{
    OPEN_UP = 1u << 0,
    OPEN_DOWN = 1u << 1,
    OPEN_LEFT = 1u << 2,
    OPEN_RIGHT = 1u << 3
};

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

/* Rows of the tile the lip takes when the tile is a floor: the arris, two of
 * finish and the black nosing line under them. */
enum
{
    LIP_ROWS = 4
};

/*
 * Riveted plate, hung in panels two tiles by two.
 *
 * A panel is a raised plate: lit along the top and the left in the ink, in
 * shadow along the bottom and the right, which is the one direction of light
 * every surface in this file agrees on — the ceiling's, from the upper left.
 * At sixteen by thirty-two BBC pixels a panel is the module a player reads the
 * scale of the room off, which is why it is two tiles and not one: a seam
 * every tile is graph paper. The weathered floors (the plant, the plenum) run
 * rust down from the odd rivet.
 */
static void wall_plate(const ArtTile *t, bool weathered)
{
    BbcColour wall = t->p->wall;
    BbcColour ink = t->p->wall_ink;
    bool left = (t->col & 1) == 0;
    bool upper = (t->row & 1) == 0;

    tile_px(t, wall, 0, 0, 8, 16);
    if (upper)
        tile_px(t, ink, 0, 0, 8, 1);
    else
        tile_px(t, BBC_BLACK, 0, 15, 8, 1);
    if (left)
        tile_px(t, ink, 0, 0, 1, 16);
    else
        tile_px(t, BBC_BLACK, 7, 0, 1, 16);

    /* Rivets two pixels in from the panel's corners and one halfway down each
     * flank, each with the shadow it throws on the plate under it. */
    int rx = left ? 2 : 5;
    int ry = upper ? 3 : 11;
    tile_px(t, ink, rx, ry, 1, 1);
    tile_px(t, BBC_BLACK, rx, ry + 1, 1, 1);
    if (!upper)
    {
        tile_px(t, ink, rx, 0, 1, 1);
        tile_px(t, BBC_BLACK, rx, 1, 1, 1);
    }

    /* A stiffener across every second panel: a rib in the plate at its
     * mid-height, which is what makes a wall of plate read as a structure
     * rather than as a pattern. */
    if (upper && ((t->row >> 1) & 1) == 0)
    {
        tile_px(t, ink, left ? 1 : 0, 13, 7, 1);
        tile_px(t, BBC_BLACK, left ? 1 : 0, 14, 7, 1);
    }

    if (weathered && (t->h % 5u) == 0u)
    {
        int len = 3 + (int)((t->h >> 4) % 4u);
        tile_px(t, BBC_BLACK, rx, ry + 2, 1, len);
    }
}

/*
 * Poured concrete, struck off shuttering boards two tiles wide and one tall.
 *
 * The boards' joints are black, because a joint in concrete is a shadow and
 * not a seam; the form-tie holes are what say it is concrete at all, four to a
 * board on the same grid as the boards; and the aggregate is a few loose
 * pixels of the ink, never enough to make a texture of it.
 */
static void wall_concrete(const ArtTile *t)
{
    BbcColour wall = t->p->wall;
    BbcColour ink = t->p->wall_ink;
    bool left = (t->col & 1) == 0;

    tile_px(t, wall, 0, 0, 8, 16);
    tile_px(t, BBC_BLACK, 0, 0, 8, 1);
    if (left)
        tile_px(t, BBC_BLACK, 0, 0, 1, 16);
    int tx = left ? 3 : 4;
    tile_px(t, ink, tx, 4, 1, 1);
    tile_px(t, ink, tx, 11, 1, 1);
    tile_px(t, BBC_BLACK, tx, 5, 1, 1);
    tile_px(t, BBC_BLACK, tx, 12, 1, 1);
    for (unsigned fleck = 0; fleck < 2u; ++fleck)
    {
        unsigned fh = t->h >> (fleck * 9u);
        if ((fh & 3u) != 0u)
            continue;
        tile_px(t, ink, 1 + (int)((fh >> 2) % 6u), 2 + (int)((fh >> 5) % 12u),
                1, 1);
    }
}

/*
 * Glazed tile, in its grout.
 *
 * A square tile is four BBC pixels by eight, which is sixteen logical pixels
 * each way and square on the glass; the galley is set in metro instead — a
 * brick of tile, four by four, laid half over the course below. Both are keyed
 * to the world row of the course, so a run of wall stays one bond.
 */
static void wall_tile_material(const ArtTile *t, bool metro)
{
    BbcColour wall = t->p->wall;
    BbcColour ink = t->p->wall_ink;

    tile_px(t, wall, 0, 0, 8, 16);
    if (!metro)
    {
        tile_px(t, ink, 0, 0, 8, 1);
        tile_px(t, ink, 0, 8, 8, 1);
        tile_px(t, ink, 0, 0, 1, 16);
        tile_px(t, ink, 4, 0, 1, 16);
        return;
    }
    for (int course = 0; course < 4; ++course)
    {
        int world = t->row * 4 + course;
        int offset = (world & 1) != 0 ? 2 : 0;
        int y = course * 4;
        tile_px(t, ink, 0, y + 3, 8, 1);
        for (int px = 0; px < 8; ++px)
            if (((px + offset) & 3) == 0)
                tile_px(t, ink, px, y, 1, 3);
    }
}

/*
 * Polished stone in slabs two tiles square, veined, with a brass inlay run
 * along the wall every third course.
 *
 * A vein is a walk across the slab, one column at a time, falling a row or two
 * a column with the odd kink: drawn per column it stays one connected line
 * across the two tiles of a slab that are drawn by separate calls, because
 * both work it out from the slab's own hash.
 */
static void marble_vein(const ArtTile *t, unsigned vh, int slab_x,
                        int slab_y)
{
    int y = (int)((vh >> 4) % 32u);
    int dir = ((vh >> 9) & 1u) != 0u ? 1 : -1;
    int steep = 1 + (int)((vh >> 10) & 1u);
    for (int k = 0; k < 16; ++k)
    {
        int step = steep;
        if ((art_mix(vh + (unsigned)k * 0x9e3779b9u) & 3u) == 0u)
            step = steep == 1 ? 2 : 0;
        int next = y + dir * step;
        int lo = y < next ? y : next;
        int hi = y < next ? next : y;
        if (hi == lo)
            ++hi;
        int px = k - slab_x;
        if (px >= 0 && px < 8)
        {
            int a = lo - slab_y;
            int b = hi - slab_y;
            if (a < 0)
                a = 0;
            if (b > 16)
                b = 16;
            if (b > a)
                tile_px(t, t->p->wall_ink, px, a, 1, b - a);
        }
        y = next;
        if (y < -2 || y > 34)
            break;
    }
}

static void wall_marble(const ArtTile *t)
{
    int slab_x = (t->col & 1) * 8;
    int slab_y = (t->row & 1) * 16;
    unsigned sh = art_hash(t->col >> 1, t->row >> 1);

    tile_px(t, t->p->wall, 0, 0, 8, 16);
    if ((sh & 3u) != 0u)
        marble_vein(t, sh, slab_x, slab_y);
    if ((sh & 0x3000u) == 0u)
        marble_vein(t, art_mix(sh ^ 0x51u), slab_x, slab_y);
    /* The joints in the vein colour rather than in black: polished stone is
     * laid tight, and a black joint every two tiles turned a marble hall into
     * a sheet of graph paper. */
    if (slab_x == 0)
        tile_px(t, t->p->wall_ink, 0, 0, 1, 16);
    if (slab_y == 0)
        tile_px(t, t->p->wall_ink, 0, 0, 8, 1);
    if (((t->row % 3) + 3) % 3 == 1)
        tile_px(t, t->p->accent, 0, 8, 8, 1);
}

/*
 * Painted partition boarding: sheets two tiles wide and three tall, jointed in
 * the ink, with the screw line down each sheet's edge. Plain between them,
 * because a partition is.
 */
static void wall_drywall(const ArtTile *t)
{
    BbcColour ink = t->p->wall_ink;
    bool left = (t->col & 1) == 0;

    tile_px(t, t->p->wall, 0, 0, 8, 16);
    if (left)
        tile_px(t, ink, 0, 0, 1, 16);
    if (((t->row % 3) + 3) % 3 == 0)
        tile_px(t, ink, 0, 0, 8, 1);
    int sx = left ? 2 : 5;
    tile_px(t, ink, sx, 4, 1, 1);
    tile_px(t, ink, sx, 12, 1, 1);
}

/*
 * Stretcher-bond brick: a brick is four BBC pixels by four rows, sixteen by
 * eight logical, which is the proportion of a real one, four courses to the
 * tile and each laid half over the one below. The odd brick out of a
 * different firing is a checkerboard of the brick and its mortar — the BBC's
 * brown, which is exactly what an old brick shell is full of.
 */
static void wall_brick(const ArtTile *t)
{
    BbcColour wall = t->p->wall;
    BbcColour ink = t->p->wall_ink;

    tile_px(t, wall, 0, 0, 8, 16);
    for (int course = 0; course < 4; ++course)
    {
        int world = t->row * 4 + course;
        int offset = (world & 1) != 0 ? 2 : 0;
        int y = course * 4;
        for (int px = 0; px < 8; ++px)
        {
            int wx = t->col * 8 + px + offset;
            if ((wx & 3) == 0)
            {
                tile_px(t, ink, px, y, 1, 3);
                unsigned bh = art_hash(wx >> 2, world);
                if ((bh % 9u) == 0u)
                {
                    int w = px + 4 <= 8 ? 3 : 8 - px - 1;
                    if (w > 0)
                        bbc_dither(t->r, wall, ink, BBC_DITHER_CHECK,
                                   t->x + (float)(px + 1) * PXW,
                                   t->y + (float)y * PXH, (float)w * PXW,
                                   3.0f * PXH);
                }
            }
        }
        tile_px(t, ink, 0, y + 3, 8, 1);
    }
}

/*
 * Stile-and-rail panelling, a panel two tiles by two: the frame plain, the
 * raised field inside it moulded — the ink along its top and left where the
 * light catches it, black along its bottom and right where it does not.
 */
static void wall_wood(const ArtTile *t)
{
    BbcColour ink = t->p->wall_ink;
    bool left = (t->col & 1) == 0;
    bool upper = (t->row & 1) == 0;

    tile_px(t, t->p->wall, 0, 0, 8, 16);
    /* The field runs from two pixels in to two from the far side, and from
     * three rows down to three from the bottom of the panel. */
    int x0 = left ? 2 : 0;
    int x1 = left ? 8 : 6;
    int y0 = upper ? 3 : 0;
    int y1 = upper ? 16 : 13;
    if (upper)
        tile_px(t, ink, x0, y0, x1 - x0, 1);
    else
        tile_px(t, BBC_BLACK, x0, y1 - 1, x1 - x0, 1);
    if (left)
        tile_px(t, ink, x0, y0, 1, y1 - y0);
    else
        tile_px(t, BBC_BLACK, x1 - 1, y0, 1, y1 - y0);
    /* The joint between one panel and the next. */
    if (left)
        tile_px(t, BBC_BLACK, 0, 0, 1, 16);
}

/*
 * A blocked-up opening: the patch a blast will open.
 *
 * It has to say so at a glance, in every material, which is why it is drawn
 * the same way on all of them and coarser than any of them: two courses of
 * big blocks set in a black surround, and cracks running across the joints
 * rather than along them, because a fracture that stops at every block edge is
 * a block edge. The material is drawn under it, so the patch still belongs to
 * this floor of the building.
 */
static void wall_weak_patch(const ArtTile *t)
{
    BbcColour wall = t->p->wall;
    BbcColour ink = t->p->wall_ink;

    tile_px(t, BBC_BLACK, 0, 1, 8, 15);
    /* Two courses, offset half a block, keyed to the world so a run of
     * patches is one piece of work. */
    static const int COURSE_A[2][2] = {{1, 3}, {5, 2}};
    static const int COURSE_B[2][2] = {{1, 2}, {4, 3}};
    bool odd = (t->col & 1) != 0;
    /* The blocks are rough where the wall is finished: the material and its
     * ink on a checkerboard, which no wall in the building is built of, lit
     * along the top course it was laid to. */
    for (int course = 0; course < 2; ++course)
    {
        const int(*blocks)[2] = (course == 0) != odd ? COURSE_A : COURSE_B;
        int by = course == 0 ? 2 : 9;
        for (int b = 0; b < 2; ++b)
        {
            bbc_dither(t->r, wall, ink, BBC_DITHER_CHECK,
                       t->x + (float)blocks[b][0] * PXW,
                       t->y + (float)(by + 1) * PXH,
                       (float)blocks[b][1] * PXW, 5.0f * PXH);
            tile_px(t, wall, blocks[b][0], by, blocks[b][1], 1);
        }
    }
    /* Two cracks, each a jag of black stepping down across both courses with
     * the spalled edge beside it in the ink. */
    for (int i = 0; i < 2; ++i)
    {
        unsigned ch = art_mix(t->h + (unsigned)i * 0x2545f491u);
        int cx = 2 + (int)(ch % 4u);
        int cy = 3 + (int)((ch >> 3) % 3u);
        int lean = ((ch >> 6) & 1u) != 0u ? 1 : -1;
        for (int step = 0; step < 5 && cy < 15; ++step)
        {
            tile_px(t, BBC_BLACK, cx, cy, 1, 2);
            if (step == 1)
                tile_px(t, ink, cx - lean, cy, 1, 1);
            cy += 2;
            if (((ch >> (unsigned)(8 + step)) & 1u) != 0u)
                cx += lean;
            if (cx < 1)
                cx = 1;
            if (cx > 6)
                cx = 6;
        }
    }
}

/*
 * The walkway lip: the one surface in the game a player looks at all the
 * time, and so the brightest line on any floor.
 *
 * Row 0 is the arris in the trim, rows 1 and 2 are the floor finish in the
 * trim and its ink, and row 3 is the black of the nosing's own shadow, which
 * is what separates the surface you stand on from the face of the slab holding
 * it up. The finish is the theme's FloorStyle, and every one of them is keyed
 * to the world column so a floor is one floor and not a row of stamps.
 */
static void floor_lip(const ArtTile *t, FloorStyle style)
{
    BbcColour trim = t->p->trim;
    BbcColour ink = t->p->trim_ink;
    unsigned h = t->h;

    tile_px(t, trim, 0, 0, 8, 1);
    switch (style)
    {
    case FLOOR_STONE:
        /* Polished stone under a brass arris; jointed every two tiles, with
         * the odd streak of the brass given back by the polish. */
        tile_px(t, ink, 0, 1, 8, 2);
        if ((t->col & 1) == 0)
            tile_px(t, BBC_BLACK, 0, 1, 1, 2);
        if ((h & 3u) == 0u)
            tile_px(t, trim, 2 + (int)((h >> 2) % 4u), 1, 2, 1);
        break;
    case FLOOR_CARPET:
        /* Matte, so it is the one finish drawn as a material rather than as a
         * colour: its ink and black on a checkerboard, which is how a BBC drew
         * anything with a nap. */
        bbc_dither(t->r, ink, BBC_BLACK, BBC_DITHER_CHECK, t->x, t->y + PXH,
                   8.0f * PXW, 2.0f * PXH);
        break;
    case FLOOR_PANEL:
        /* Raised access floor: a liftable panel every tile, jointed at both
         * ends, perforated so the cold air comes up through it. */
        tile_px(t, trim, 0, 1, 8, 2);
        tile_px(t, ink, 0, 1, 1, 2);
        tile_px(t, ink, 7, 1, 1, 2);
        tile_px(t, BBC_BLACK, 2, 1, 1, 1);
        tile_px(t, BBC_BLACK, 4, 1, 1, 1);
        tile_px(t, BBC_BLACK, 3, 2, 1, 1);
        tile_px(t, BBC_BLACK, 5, 2, 1, 1);
        break;
    case FLOOR_CHEQUER:
        /* Treadplate: raised studs in two staggered rows. */
        tile_px(t, trim, 0, 1, 8, 2);
        tile_px(t, ink, 1, 1, 1, 1);
        tile_px(t, ink, 5, 1, 1, 1);
        tile_px(t, ink, 3, 2, 1, 1);
        tile_px(t, ink, 7, 2, 1, 1);
        break;
    case FLOOR_CERAMIC:
        /* Grouted on the floor tile's own module, half a tile. */
        tile_px(t, ink, 0, 1, 8, 2);
        tile_px(t, trim, 0, 1, 1, 2);
        tile_px(t, trim, 4, 1, 1, 2);
        break;
    case FLOOR_BOARDS:
        /* Timber: butt joints land where they land. */
        tile_px(t, ink, 0, 1, 8, 2);
        tile_px(t, trim, 1 + (int)(h % 7u), 1, 1, 2);
        if ((h & 0x100u) != 0u)
            tile_px(t, trim, 0, 2, 2, 1);
        break;
    case FLOOR_PARQUET:
        /* Blocks laid in alternating pairs, two pixels a block. */
        for (int block = 0; block < 4; ++block)
        {
            bool across = ((t->col * 4 + block + t->row) & 1) != 0;
            tile_px(t, across ? ink : trim, block * 2, 1, 2, 2);
        }
        break;
    case FLOOR_SCREED:
    default:
        /* Power-floated deck: nothing on it but a saw cut every four tiles
         * and the odd fleck. */
        tile_px(t, trim, 0, 1, 8, 2);
        if ((t->col & 3) == 0)
            tile_px(t, ink, 0, 1, 1, 2);
        if ((h & 1u) != 0u)
            tile_px(t, ink, 2 + (int)((h >> 3) % 5u), 1 + (int)((h >> 6) & 1u),
                    1, 1);
        break;
    }
    tile_px(t, BBC_BLACK, 0, 3, 8, 1);
}

/* The faces of a mass the player walks past and jumps under: an arris in the
 * ink down an exposed flank and along an exposed underside, so a slab is
 * drawn with an edge rather than simply stopping. A slab that ends in mid-air
 * carries its lip a few rows down the flank, which is the difference between a
 * ledge and a rectangle that happens to be brighter along the top. */
static void wall_edges(const ArtTile *t, unsigned open)
{
    BbcColour ink = t->p->wall_ink;
    if (open & OPEN_LEFT)
        tile_px(t, ink, 0, 0, 1, 16);
    if (open & OPEN_RIGHT)
        tile_px(t, ink, 7, 0, 1, 16);
    if (open & OPEN_DOWN)
        tile_px(t, ink, 0, 15, 8, 1);
}

static void wall_lip_returns(const ArtTile *t, unsigned open)
{
    if ((open & OPEN_LEFT) != 0u)
        tile_px(t, t->p->trim, 0, 0, 1, LIP_ROWS + 2);
    if ((open & OPEN_RIGHT) != 0u)
        tile_px(t, t->p->trim, 7, 0, 1, LIP_ROWS + 2);
}

static ArtTile art_tile(SDL_Renderer *r, const Level *level, int col, int row,
                        float x, float y)
{
    ArtTile t;
    t.r = r;
    t.p = level_theme_paint(level->map.theme);
    t.x = bbc_snap_x(x);
    t.y = bbc_snap_y(y);
    t.col = col;
    t.row = row;
    t.h = art_hash(col, row);
    /* Anchored here as well as by the caller: the editor draws walls with a
     * camera of its own, and a carpet dither anchored to anything but the
     * tile's own world position crawls along the floor as it scrolls. */
    bbc_set_pattern_origin(t.x - (float)col * (float)TILE_SIZE,
                           t.y - (float)row * (float)TILE_SIZE);
    return t;
}

void level_art_broken_wall_tile(SDL_Renderer *r, const Level *level,
                                int col, int row, float x, float y)
{
    ArtTile t = art_tile(r, level, col, row, x, y);
    BbcColour wall = t.p->wall;
    BbcColour ink = t.p->wall_ink;

    /* The ragged edge of the hole where it was torn out of the masonry beside
     * it: a few teeth of the material standing proud of the cut, only on the
     * sides that were wall. Open air on a side means the hole was the end of
     * the wall, and there is nothing to have torn. */
    for (int side = 0; side < 2; ++side)
    {
        if (!level_is_solid(level, col + (side == 0 ? -1 : 1), row))
            continue;
        for (int tooth = 0; tooth < 4; ++tooth)
        {
            unsigned th = art_mix(t.h + (unsigned)(side * 8 + tooth));
            int depth = 1 + (int)(th % 2u);
            int ty = 1 + tooth * 3 + (int)((th >> 2) % 2u);
            tile_px(&t, wall, side == 0 ? 0 : 8 - depth, ty, depth, 2);
        }
    }
    if (level_is_solid(level, col, row - 1))
    {
        for (int tooth = 0; tooth < 3; ++tooth)
        {
            unsigned th = art_mix(t.h + 31u + (unsigned)tooth);
            tile_px(&t, wall, 1 + tooth * 2 + (int)(th % 2u), 0, 1,
                    1 + (int)((th >> 1) % 3u));
        }
    }

    /* Rubble only rests on something. A hole blown through a wall with air
     * under it keeps nothing, which is right: it all went to the floor below.
     * Where there is a floor, a heap highest in the middle, lumps of the
     * material lit along their tops, black between them. */
    if (!level_is_solid(level, col, row + 1))
        return;
    static const int HEAP[8] = {1, 2, 3, 4, 4, 3, 2, 1};
    for (int px = 0; px < 8; ++px)
    {
        unsigned rh = art_mix(t.h + 97u + (unsigned)px);
        int tall = HEAP[px] - 1 + (int)(rh % 2u);
        if (tall < 1)
            continue;
        tile_px(&t, (rh & 4u) != 0u ? wall : ink, px, 16 - tall, 1, tall);
        tile_px(&t, ink, px, 16 - tall, 1, 1);
        if ((rh & 24u) == 0u && tall > 1)
            tile_px(&t, BBC_BLACK, px, 17 - tall, 1, 1);
    }
}

/*
 * Whether a tile is buried: masonry on all eight sides, so nothing anybody can
 * stand on, walk past or jump under touches it.
 *
 * A BBC game drew solid rock darker than the face of it, because the face is
 * what the player reads a floor plan from and the rock behind it is only there
 * to be not-a-room. Drawn at full strength, the slab over the lobby was the
 * brightest thing on the first screen of the game — a field of white marble
 * nobody can reach.
 */
static bool tile_buried(const Level *level, int col, int row)
{
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx != 0 || dy != 0) &&
                !level_is_solid(level, col + dx, row + dy))
                return false;
    return true;
}

void level_art_wall_tile(SDL_Renderer *r, const Level *level,
                         int col, int row, float x, float y)
{
    const LevelThemeArt *art = level_art(level->map.theme);
    ArtTile t = art_tile(r, level, col, row, x, y);

    /* The darker rock is the material one rung down the palette ladder — white
     * marble to cyan, yellow brick to red, red plate to black — done the way the
     * machine did it, by re-pointing the palette for the tile and putting it
     * back, so every material gets its own darker self without a second
     * drawing of any of them. A patch is never buried: it is the one wall in
     * the building that is meant to be seen. */
    BbcColour saved[BBC_COLOUR_COUNT];
    bool buried = tile_buried(level, col, row) &&
                  level_tile(level, col, row) != TILE_WEAK_WALL;
    if (buried)
    {
        for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
            saved[i] = bbc_map((BbcColour)i);
        bbc_palette_dim(1);
    }

    switch (art->wall_style)
    {
    case WALL_STYLE_CONCRETE:
        wall_concrete(&t);
        break;
    case WALL_STYLE_TILE:
        /* The galley is the one tiled floor that is set in metro. */
        wall_tile_material(&t, level->map.theme == LEVEL_THEME_CANTEEN);
        break;
    case WALL_STYLE_MARBLE:
        wall_marble(&t);
        break;
    case WALL_STYLE_DRYWALL:
        wall_drywall(&t);
        break;
    case WALL_STYLE_BRICK:
        wall_brick(&t);
        break;
    case WALL_STYLE_WOOD:
        wall_wood(&t);
        break;
    case WALL_STYLE_PLATE:
    default:
        wall_plate(&t, level->map.theme == LEVEL_THEME_PLANT ||
                           level->map.theme == LEVEL_THEME_DUCTS);
        break;
    }

    /* A patched opening is drawn over its sector's own material, so the wall it
     * was let into still belongs to this floor of the building. */
    if (level_tile(level, col, row) == TILE_WEAK_WALL)
        wall_weak_patch(&t);

    /* Edges are shared by every material: the surfaces the player stands on,
     * walks past and jumps under have to read the same way in all seventeen
     * sectors or the level stops being legible. */
    unsigned open = tile_open_mask(level, col, row);
    wall_edges(&t, open);
    if (open & OPEN_UP)
    {
        floor_lip(&t, art->floor_style);
        wall_lip_returns(&t, open);
    }

    if (buried)
        for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
            bbc_palette_set((BbcColour)i, saved[i]);
}

/* ---- Interior backdrops ---------------------------------------------- */

/*
 * One room's back wall, on screen: what an interior theme lays a wall out
 * against.
 *
 * `level_backdrop_plan` reads the map into rooms (see level.h), and a theme is
 * handed one room at a time with the renderer clipped to the piece of it being
 * drawn. The room is anchored to the building: it moves with the camera on both
 * axes, a whole BBC pixel at a time, exactly as the slab above it and the floor
 * under it do — which is also what lets a dither on the back wall sit still.
 *
 * The rules for a theme drawing into one:
 *
 * - **The room is black.** What a theme draws is the room's own lines, in the
 *   paint's backdrop colour, sparsely: a mullion, a shelf edge, a panel seam.
 *   Bright on black reads as near and blue on black as far, and that is all
 *   the depth the machine had. Anything lit — a lamp, a status light, a window
 *   across the street — is a few pixels, never a pool.
 * - **Stand things on `floor` and hang them from `top`**, and size anything
 *   tall to `height`: `height` runs from two tiles to a dozen, so a layout has
 *   to scale.
 * - **Horizontal repeats are world-keyed** and a storey's variation comes off
 *   `seed` — never off `left`/`right`. Two pieces of one storey on either side
 *   of a partition get the same room and must draw the same wall, or the
 *   partition becomes a seam.
 * - **What is far away is not on the wall.** A city through a window is a long
 *   way off and should barely move when the room does, so it runs at a small
 *   share of the camera, clipped to the glass; the window frame, which is on
 *   the wall, does not.
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

/* A storey's own place along a run of period `period`, off its seed, on the
 * grid. A layer that has to stay on the building's own grid passes no salt. */
static float room_phase(const ArtRoom *m, unsigned salt, float period)
{
    unsigned steps = (unsigned)(period / PXW);
    if (steps == 0u)
        return 0.0f;
    return (float)(art_mix(m->seed ^ (salt * 0x9e3779b9u)) % steps) * PXW;
}

/* The first bay of a run that repeats every `period` world pixels from
 * `phase`, among those whose right-hand `reach` is inside the piece. A theme is
 * called once per piece, so a run looped across the whole window draws most of
 * itself into a clip that throws it away. */
static int room_bay(const ArtCtx *c, const ArtRoom *m, float period,
                    float phase, float reach)
{
    return art_repeat(m->clip_left - reach - c->ox - phase, period);
}

static float room_bay_x(const ArtCtx *c, float period, float phase, int i)
{
    return c->ox + phase + (float)i * period;
}

/* Clip to one pane inside the piece being drawn; false when it is not in the
 * piece. `room_unpane` puts the piece's own clip back. */
static bool room_pane(const ArtCtx *c, const ArtRoom *m, float x0, float y0,
                      float x1, float y1)
{
    x0 = fmaxf(x0, m->clip_left);
    y0 = fmaxf(y0, m->clip_top);
    x1 = fminf(x1, m->clip_right);
    y1 = fminf(y1, m->clip_bottom);
    if (x1 <= x0 || y1 <= y0)
        return false;
    art_clip(c, x0, y0, x1, y1);
    return true;
}

static void room_unpane(const ArtCtx *c, const ArtRoom *m)
{
    art_clip(c, m->clip_left, m->clip_top, m->clip_right, m->clip_bottom);
}

/*
 * A city through glass, the towers standing on `horizon`.
 *
 * Two kinds, because a window looks at two distances. The **near** city is the
 * block across the street: towers in outline, close enough that their floors
 * are rows you could count, with somebody else's late night lit in a few of
 * them. The **far** city is the skyline: solid blue against the black, its
 * floors ruled in black and its lit windows yellow. Either way it slides at a
 * small share of the camera — it is not on the wall — and which windows are
 * lit is keyed to the tower, the floor and the bay, never to where the window
 * is on screen, or the city repaints its lights every time the camera moves.
 */
static void art_city(const ArtCtx *c, float x0, float x1, float sky_top,
                     float horizon, float factor, unsigned salt, bool far)
{
    const float period = far ? 40.0f : 72.0f;
    const BbcColour blue = c->p->backdrop;
    float shift = c->cam_x * factor;
    for (int i = art_repeat(x0 - 72.0f + shift, period);; ++i)
    {
        float x = art_sx((float)i * period - shift);
        if (x > x1)
            break;
        unsigned h = art_hash(i, (int)salt);
        float tw = far ? 24.0f + (float)(h % 4u) * 4.0f
                       : 40.0f + (float)(h % 5u) * 4.0f;
        float th = art_sy((far ? 24.0f : 40.0f) +
                          (float)((h >> 4) % 13u) * (far ? 8.0f : 12.0f));
        float ty = horizon - th;
        if (ty < sky_top - 400.0f)
            ty = sky_top - 400.0f;
        /* The near block is its roofline and the flank the street light
         * catches, and nothing else: the edges of every tower in outline
         * were a cage of verticals standing behind the glass. */
        if (far)
            bbc_fill(c->r, blue, x, ty, tw, horizon - ty);
        else
        {
            art_hline(c, blue, x, x + tw, ty);
            art_vline(c, blue, x, ty, horizon);
        }
        /* A setback crown on some, and a mast with its aviation light on the
         * tallest: a skyline of flat tops is a bar chart. */
        if (((h >> 12) & 3u) == 0u)
        {
            float cw = art_sx(tw * 0.5f);
            float cx = x + art_sx((tw - cw) * 0.5f);
            if (far)
                bbc_fill(c->r, blue, cx, ty - 8.0f, cw, 8.0f);
            else
            {
                art_hline(c, blue, cx, cx + cw, ty - 8.0f);
                art_vline(c, blue, cx, ty - 8.0f, ty);
            }
            art_vline(c, blue, cx + art_sx(cw * 0.5f), ty - 20.0f, ty - 8.0f);
            bbc_plot(c->r, art_blink(BBC_RED, h >> 20),
                     cx + art_sx(cw * 0.5f), ty - 22.0f);
        }
        for (int fl = 0;; ++fl)
        {
            float wy = ty + 6.0f + (float)fl * 8.0f;
            if (wy > horizon - 6.0f)
                break;
            if (far)
                art_hline(c, BBC_BLACK, x, x + tw, wy + 4.0f);
            for (int bay = 0; 4.0f + (float)bay * 8.0f + PXW < tw; ++bay)
            {
                unsigned wh = art_hash(i * 13 + bay, fl * 7 + (int)salt);
                if ((wh % (far ? 4u : 7u)) != 0u)
                    continue;
                bbc_block(c->r, BBC_YELLOW, x + 4.0f + (float)bay * 8.0f, wy, 1,
                          2);
            }
        }
    }
}

/* A girder along a ceiling: two chords and the zigzag between them, which is
 * how a lattice beam was drawn on a machine with lines and nothing else. */
static void art_lattice(const ArtCtx *c, BbcColour col, float x0, float x1,
                        float y0, float y1, float pitch)
{
    art_hline(c, col, x0, x1, y0);
    art_hline(c, col, x0, x1, y1);
    float half = pitch * 0.5f;
    for (int i = art_repeat(x0 - c->ox - pitch, pitch);; ++i)
    {
        float x = c->ox + (float)i * pitch;
        if (x > x1)
            break;
        bbc_line(c->r, col, x, y0, x + half, y1);
        bbc_line(c->r, col, x + half, y1, x + pitch, y0);
    }
}

/* ---- PLANT ------------------------------------------------------------ */

/*
 * PLANT — the mechanical floor: a hall of vessels and pump sets under a lattice
 * roof beam, with a service gallery halfway up the taller storeys.
 *
 * Laid out per room it is a hall *of that storey*: the vessels stand on its
 * floor and rise to a height the room can hold, the beam hangs from its
 * ceiling, and a storey tall enough for two levels of plant (eight tiles or
 * more) gets a gallery across its middle. Nothing is ever taller than the room
 * it stands in. Each vessel's flue carries a red warning light at the ceiling,
 * which is the floor's only colour that is not structure.
 */
static void plant_vessel(const ArtCtx *c, float x, float w, float top,
                         float floor, unsigned h)
{
    const BbcColour blue = c->p->backdrop;
    /* The body, its head stepped round to a dome in two rows. */
    art_hline(c, blue, x + 8.0f, x + w - 8.0f, top);
    art_hline(c, blue, x + 4.0f, x + 8.0f, top + 2.0f);
    art_hline(c, blue, x + w - 8.0f, x + w - 4.0f, top + 2.0f);
    art_vline(c, blue, x, top + 4.0f, floor - 6.0f);
    art_vline(c, blue, x + w - PXW, top + 4.0f, floor - 6.0f);
    bbc_plot(c->r, blue, x + 4.0f, top + 4.0f);
    bbc_plot(c->r, blue, x + w - 8.0f, top + 4.0f);
    /* Hoops, and the skirt it stands on. */
    for (float y = top + 24.0f; y < floor - 20.0f; y += 32.0f)
        art_hline(c, blue, x, x + w, y);
    art_hline(c, blue, x - 4.0f, x + w + 4.0f, floor - 6.0f);
    art_vline(c, blue, x + 4.0f, floor - 6.0f, floor);
    art_vline(c, blue, x + w - 8.0f, floor - 6.0f, floor);
    /* A manway near the foot and a gauge by the head, the one lit thing on
     * it: a dial needle in the lamp colour. */
    if (floor - top > 72.0f)
        bbc_ring(c->r, blue, x + art_sx(w * 0.5f), floor - 26.0f, 9.0f);
    if ((h & 2u) != 0u)
    {
        bbc_frame(c->r, blue, x + 8.0f, top + 10.0f, 12.0f, 8.0f);
        bbc_plot(c->r, c->p->lamp, x + 12.0f, top + 12.0f);
    }
}

static void room_plant(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float top = m->top;
    const float floor = m->floor;
    const float height = m->height;
    const bool tall = height >= 240.0f;
    const float cl = m->clip_left;
    const float cr = m->clip_right;

    /* Vessels and pump houses, a bay every 224 pixels, shuffled along the
     * storey by its seed so the tanks do not stack in a column. */
    const float period = 224.0f;
    float phase = room_phase(m, 0x51u, period);
    for (int i = room_bay(c, m, period, phase, 200.0f);; ++i)
    {
        float x = room_bay_x(c, period, phase, i);
        if (x > cr)
            break;
        unsigned h = art_hash(i, (int)m->seed);
        float share = (tall ? 0.58f : 0.55f) + art_unit(h, 0) * 0.16f;
        float tank_top = art_sy(fmaxf(floor - height * share, top + 40.0f));
        float tw = 64.0f + (float)(h % 3u) * 8.0f;
        float tx = x + 24.0f;
        plant_vessel(c, tx, tw, tank_top, floor, h);

        /* The flue off the head, up through the roof. */
        float fx = tx + art_sx(tw * 0.4f);
        art_vline(c, blue, fx, top, tank_top);
        art_vline(c, blue, fx + 8.0f, top, tank_top);
        for (float fy = top + 24.0f; fy < tank_top - 8.0f; fy += 40.0f)
            art_hline(c, blue, fx - 4.0f, fx + 16.0f, fy);
        bbc_block(c->r, art_blink(BBC_RED, h >> 7), fx + 4.0f, top + 10.0f, 1,
                  2);

        /* The pump house, louvred, fed by a pipe from the vessel. */
        float house = art_sy(fminf(64.0f, height * 0.30f));
        float hx = tx + tw + 28.0f;
        bbc_frame(c->r, blue, hx, floor - house, 64.0f, house);
        for (float ly = floor - house + 8.0f; ly < floor - 6.0f; ly += 6.0f)
            art_hline(c, blue, hx + 8.0f, hx + 40.0f, ly);
        float feed = art_sy(floor - house * 0.6f);
        art_hline(c, blue, tx + tw, hx, feed);
        art_hline(c, blue, tx + tw, hx, feed + 6.0f);
        bbc_fill(c->r, blue, tx + tw + 12.0f, feed - 2.0f, 4.0f, 10.0f);
    }

    /* The roof: the lattice beam the hall is built under. */
    art_lattice(c, blue, cl, cr, top + 6.0f, top + 18.0f, 32.0f);

    /* The service pipe run under it, flanged every 96 pixels. */
    float pipe = top + (tall ? 40.0f : 24.0f);
    if (pipe + 8.0f < floor - 24.0f)
    {
        art_hline(c, blue, cl, cr, pipe);
        art_hline(c, blue, cl, cr, pipe + 6.0f);
        for (int i = room_bay(c, m, 96.0f, 0.0f, 8.0f);; ++i)
        {
            float x = room_bay_x(c, 96.0f, 0.0f, i);
            if (x > cr)
                break;
            bbc_fill(c->r, blue, x, pipe - 2.0f, 8.0f, 12.0f);
        }
    }

    /* The gallery across a tall hall, on its own posts, with the cabinets
     * that stand on it. */
    if (tall)
    {
        float deck = art_sy(floor - height * 0.5f);
        art_hline(c, blue, cl, cr, deck);
        art_hline(c, blue, cl, cr, deck - 24.0f);
        art_hline(c, blue, cl, cr, deck - 12.0f);
        for (int i = room_bay(c, m, 32.0f, 0.0f, 4.0f);; ++i)
        {
            float x = room_bay_x(c, 32.0f, 0.0f, i);
            if (x > cr)
                break;
            art_vline(c, blue, x, deck - 24.0f, deck);
        }
        float cab_phase = room_phase(m, 0x77u, 160.0f);
        for (int i = room_bay(c, m, 160.0f, cab_phase, 40.0f);; ++i)
        {
            float x = room_bay_x(c, 160.0f, cab_phase, i);
            if (x > cr)
                break;
            unsigned h = art_hash(i, 991 + (int)m->seed);
            bbc_frame(c->r, blue, x, deck - 40.0f, 28.0f, 40.0f);
            bbc_plot(c->r, art_blink(BBC_GREEN, h), x + 8.0f, deck - 34.0f);
            bbc_plot(c->r, BBC_RED, x + 16.0f, deck - 34.0f);
        }
    }

    /* Caged lamps hung under the beam, one bay in two lit. */
    for (int i = room_bay(c, m, 192.0f, 96.0f, 16.0f);; ++i)
    {
        float x = room_bay_x(c, 192.0f, 96.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x + 4.0f, top + 18.0f, top + 22.0f);
        bbc_frame(c->r, blue, x, top + 22.0f, 12.0f, 6.0f);
        if ((art_hash(i, 17) & 1u) == 0u)
            bbc_block(c->r, c->p->lamp, x + 4.0f, top + 24.0f, 1, 1);
    }
}

/* ---- LOBBY ------------------------------------------------------------ */

/*
 * LOBBY — the ground floor Chuck walks in through: a glazed street front onto
 * the block across the road, a stone fascia it is hung from, brass pendants in
 * the hall, and the way in off the street drawn once, because a lobby has one.
 * The floors above it are the same stone with windows looking out over the
 * same street.
 */
static void lobby_car(const ArtCtx *c, float x, float kerb, bool leftward)
{
    const BbcColour blue = c->p->backdrop;
    /* A body in outline and its lights: white at the front, red at the back,
     * which is the only way a car at night reads at this size. */
    art_hline(c, blue, x + 8.0f, x + 24.0f, kerb - 10.0f);
    art_hline(c, blue, x, x + 32.0f, kerb - 6.0f);
    bbc_plot(c->r, blue, x + 4.0f, kerb - 8.0f);
    bbc_plot(c->r, blue, x + 24.0f, kerb - 8.0f);
    bbc_plot(c->r, blue, x + 4.0f, kerb - 2.0f);
    bbc_plot(c->r, blue, x + 24.0f, kerb - 2.0f);
    bbc_plot(c->r, leftward ? BBC_WHITE : BBC_RED, x, kerb - 6.0f);
    bbc_plot(c->r, leftward ? BBC_RED : BBC_WHITE, x + 28.0f, kerb - 6.0f);
}

static void lobby_street_front(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float floor = m->floor;
    const bool hall = m->height >= 160.0f;
    const float soffit = m->top + (hall ? 24.0f : 8.0f);
    const float glass_bottom = floor - 4.0f;
    const float kerb = glass_bottom - 8.0f;

    /* Through the glass: the block opposite, its pavement and its street
     * lamps, and the odd car going by. */
    if (room_pane(c, m, cl, soffit, cr, glass_bottom))
    {
        art_city(c, cl, cr, soffit, kerb - 28.0f, 0.2f, 97u, false);
        art_hline(c, blue, cl, cr, kerb - 28.0f);
        /* The shopfronts along the foot of the block opposite, which are
         * what put a street at the bottom of the towers rather than a gap:
         * two lit in three, warm or cold, keyed to the shop. */
        float shops = c->cam_x * 0.2f;
        for (int i = art_repeat(cl - 40.0f + shops, 40.0f);; ++i)
        {
            float x = art_sx((float)i * 40.0f - shops);
            if (x > cr)
                break;
            unsigned sh = art_hash(i, 97 + c->s->level_index);
            if ((sh % 3u) == 0u)
                continue;
            bbc_fill(c->r, (sh & 4u) != 0u ? BBC_YELLOW : BBC_CYAN, x + 4.0f,
                     kerb - 26.0f, 24.0f, 6.0f);
            art_vline(c, BBC_BLACK, x + 16.0f, kerb - 26.0f, kerb - 20.0f);
        }
        art_hline(c, blue, cl, cr, kerb - 18.0f);
        float shift = c->cam_x * 0.4f;
        for (int i = art_repeat(cl - 16.0f + shift, 192.0f);; ++i)
        {
            float x = art_sx((float)i * 192.0f - shift);
            if (x > cr)
                break;
            art_vline(c, blue, x, kerb - 48.0f, kerb);
            art_hline(c, blue, x, x + 12.0f, kerb - 48.0f);
            bbc_block(c->r, BBC_YELLOW, x + 8.0f, kerb - 46.0f, 1, 1);
        }
        for (int car = 0; car < 2; ++car)
        {
            float lane = 640.0f + (float)car * 330.0f;
            float run = c->t * (car == 0 ? 52.0f : 37.0f) +
                        (float)car * 211.0f;
            run -= lane * floorf(run / lane);
            bool leftward = car == 1;
            float x = leftward ? cr + 40.0f - run : cl - 40.0f + run;
            lobby_car(c, art_sx(x), kerb + (float)car * 4.0f, leftward);
        }
        room_unpane(c, m);
    }

    /* The curtain wall: mullions on the building's own grid, a transom in a
     * hall, the base channel the glass sits in and the fascia it hangs from. */
    for (int i = room_bay(c, m, 96.0f, 0.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 96.0f, 0.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, soffit, glass_bottom);
    }
    art_hline(c, blue, cl, cr, soffit);
    art_hline(c, blue, cl, cr, glass_bottom);
    float transom = floor - 96.0f;
    if (hall && transom > soffit + 24.0f)
        art_hline(c, blue, cl, cr, transom);

    if (!hall)
        return;

    /* The fascia: jointed stone on the mullion grid, with a brass nosing. */
    art_hline(c, blue, cl, cr, m->top + 8.0f);
    art_hline(c, c->p->accent, cl, cr, soffit - 4.0f);

    /* The way in, drawn once, its jambs on the mullion grid: a canopy, two
     * pairs of doors and the revolving drum between them. */
    float ex = c->ox + 96.0f;
    if (ex < cr && ex + 192.0f > cl && transom > soffit + 24.0f)
    {
        bbc_fill(c->r, BBC_BLACK, ex, transom, 192.0f, glass_bottom - transom);
        art_hline(c, c->p->accent, ex - 8.0f, ex + 200.0f, transom);
        art_hline(c, blue, ex - 8.0f, ex + 200.0f, transom + 4.0f);
        for (int leaf = 0; leaf <= 4; ++leaf)
        {
            if (leaf == 2)
                continue;
            art_vline(c, blue, ex + (float)leaf * 48.0f, transom + 4.0f,
                      glass_bottom);
        }
        /* The revolving door between them: a drum under a canopy of its
         * own, and its four wings turning inside it, drawn where each one is
         * in the turn and only while it is on the near side. */
        float dx = ex + 96.0f;
        float drum_top = glass_bottom - 64.0f;
        art_hline(c, c->p->accent, dx - 32.0f, dx + 32.0f, drum_top);
        art_hline(c, blue, dx - 28.0f, dx + 28.0f, drum_top + 4.0f);
        art_vline(c, blue, dx - 28.0f, drum_top + 4.0f, glass_bottom);
        art_vline(c, blue, dx + 24.0f, drum_top + 4.0f, glass_bottom);
        float turn = c->steady ? 0.4f : c->t * 0.6f;
        for (int wing = 0; wing < 4; ++wing)
        {
            float a = turn + (float)wing * 1.5708f;
            if (cosf(a) <= 0.0f)
                continue;
            art_vline(c, blue, art_sx(dx + sinf(a) * 22.0f), drum_top + 6.0f,
                      glass_bottom);
        }
        bbc_plot(c->r, BBC_YELLOW, dx, transom + 8.0f);
    }

    /* Brass pendants hung through the hall. */
    for (int i = room_bay(c, m, 192.0f, 48.0f, 16.0f);; ++i)
    {
        float x = room_bay_x(c, 192.0f, 48.0f, i);
        if (x > cr)
            break;
        float drop = art_sy(soffit + fminf(64.0f, m->height * 0.25f));
        art_vline(c, blue, x + 4.0f, m->top, drop);
        art_hline(c, c->p->accent, x - 4.0f, x + 16.0f, drop);
        art_hline(c, c->p->accent, x, x + 12.0f, drop + 2.0f);
        bbc_block(c->r, c->p->lamp, x + 4.0f, drop + 4.0f, 1, 1);
    }
}

static void lobby_upper_floor(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float high = m->height;

    /* The cladding, hung in panels: a joint every 64 pixels and one across
     * the middle of the storey. */
    for (int i = room_bay(c, m, 64.0f, 32.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 64.0f, 32.0f, i);
        if (x > cr)
            break;
        if ((i & 1) != 0)
            art_vline(c, blue, x, top, floor);
    }
    art_hline(c, blue, cl, cr, art_sy(top + high * 0.5f));

    /* Windows, one to every two panels, the street a storey down behind
     * them, and a brass wall-washer on each pier. */
    const float wy = art_sy(top + fminf(16.0f, high * 0.16f));
    const float wb = art_sy(floor - fminf(22.0f, high * 0.22f));
    for (int i = room_bay(c, m, 128.0f, 12.0f, 120.0f);; ++i)
    {
        float x = room_bay_x(c, 128.0f, 12.0f, i);
        if (x > cr)
            break;
        float wx = x + 20.0f;
        const float ww = 44.0f;
        bbc_fill(c->r, BBC_BLACK, wx, wy, ww, wb - wy);
        if (room_pane(c, m, wx, wy, wx + ww, wb))
        {
            art_city(c, wx, wx + ww, wy, wb + 40.0f, 0.15f, 131u, true);
            room_unpane(c, m);
        }
        bbc_frame(c->r, blue, wx - 4.0f, wy - 2.0f, ww + 8.0f, wb - wy + 4.0f);
        art_vline(c, blue, wx + 20.0f, wy, wb);
        art_hline(c, c->p->accent, wx - 8.0f, wx + ww + 8.0f, wb + 2.0f);
        float lx = x + 96.0f;
        float ly = art_sy(top + high * 0.42f);
        art_hline(c, c->p->accent, lx - 4.0f, lx + 8.0f, ly);
        bbc_plot(c->r, c->p->lamp, lx, ly - 2.0f);
    }
}

static void room_lobby(const ArtCtx *c, const ArtRoom *m)
{
    if (m->floor_row >= c->level->map.height - 2)
        lobby_street_front(c, m);
    else
        lobby_upper_floor(c, m);
}

/* ---- OFFICE ----------------------------------------------------------- */

/*
 * OFFICE — open plan after hours: a window wall onto the tower across the
 * street, the blinds nobody set the same way, a suspended ceiling, and the
 * cubicle farm in front of it all with the odd screen left on.
 *
 * The ceiling and the window head hang from `top`, the sill and the furniture
 * stand on `floor`, and the glass takes whatever height is left between them,
 * so a low storey has a letterbox of city and a tall one a proper window.
 */
static void room_office(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float high = m->height;
    const float head = art_sy(top + fminf(12.0f, fmaxf(8.0f, high * 0.11f)));
    const float glass_top = head + 2.0f;
    const float sill = art_sy(floor - fminf(28.0f, fmaxf(16.0f, high * 0.27f)));
    const float bay_w = 128.0f;

    /* The suspended ceiling: its grid line and the T-bars across it. */
    art_hline(c, blue, cl, cr, head);
    for (int i = room_bay(c, m, 32.0f, 0.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 32.0f, 0.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, top, head);
    }

    /* Through the glass, the block opposite. */
    if (sill > glass_top + 8.0f && room_pane(c, m, cl, glass_top, cr, sill))
    {
        art_city(c, cl, cr, glass_top, sill + 60.0f, 0.25f,
                 211u + (unsigned)m->floor_row, false);
        room_unpane(c, m);
    }

    /* The window wall, bay by bay, and the blinds: open, half down or shut,
     * keyed to the bay and the storey. A blind is a ruled field, which is
     * exactly what one looks like from across a room. */
    for (int i = room_bay(c, m, bay_w, 0.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, bay_w, 0.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, glass_top, sill);
        unsigned bh = art_hash(i, m->floor_row * 31 + 7);
        unsigned state = bh % 4u;
        if (state == 0u || sill <= glass_top + 8.0f)
            continue;
        float down = state == 1u ? art_sy((sill - glass_top) * 0.5f)
                                 : sill - glass_top;
        if (state == 3u)
            down = art_sy((sill - glass_top) * 0.25f);
        bbc_fill(c->r, BBC_BLACK, x + 4.0f, glass_top, bay_w - 4.0f, down);
        bbc_stipple(c->r, blue, BBC_DITHER_ROWS, x + 4.0f, glass_top,
                    bay_w - 4.0f, down);
        art_hline(c, blue, x + 4.0f, x + bay_w, glass_top + down);
    }
    art_hline(c, blue, cl, cr, sill);

    /* The cubicle farm: partition rails on posts, and a desk with a screen
     * in every bay, a few of them still on. */
    float rail = art_sy(floor - fminf(28.0f, high * 0.4f));
    art_hline(c, blue, cl, cr, rail);
    for (int i = room_bay(c, m, 64.0f, 32.0f, 24.0f);; ++i)
    {
        float x = room_bay_x(c, 64.0f, 32.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, rail, floor);
        unsigned h = art_hash(i, m->floor_row * 13 + 5);
        if ((h & 1u) == 0u || rail - 14.0f < glass_top)
            continue;
        float mx = x + 20.0f;
        bbc_frame(c->r, blue, mx, rail - 12.0f, 20.0f, 10.0f);
        art_vline(c, blue, mx + 8.0f, rail - 2.0f, rail);
        if ((h & 6u) == 0u)
            bbc_fill(c->r, BBC_CYAN, mx + 4.0f, rail - 10.0f, 12.0f, 6.0f);
    }
}

/* ---- SERVER ----------------------------------------------------------- */

/*
 * SERVER — a cold aisle, one storey of it at a time.
 *
 * The room is an aisle between a cable tray and a raised floor: the tray hangs
 * a fixed band under the room's own ceiling, the near row of racks stands on
 * the room's own floor and is exactly as tall as what is left between the two,
 * and the posts of the next aisle over show in the gaps. A taller room buys a
 * taller rack with more in it, never a bigger picture.
 *
 * The room is lit by the equipment it exists to hold: a status light on every
 * unit, half of them blinking on the machine's flash and in opposite phase to
 * their neighbours, so a rack twinkles rather than pulsing. The VAULT borrows
 * this backdrop, and in the vault the racks are the evidence: some have been
 * stripped to the rails, the lights left are the emergency circuit's red, and a
 * patch lead hangs where somebody pulled the unit behind it.
 */
typedef enum
{
    SERVER_UNIT_BLANK = 0, /* a blanking plate: nothing fitted, nothing lit */
    SERVER_UNIT_SERVER,    /* drive bays and a status light */
    SERVER_UNIT_STORAGE,   /* a double-height shelf of drive carriers */
    SERVER_UNIT_SWITCH,    /* a row of ports */
    SERVER_UNIT_EMPTY      /* pulled: the rails and nothing between them */
} ServerUnit;

static void server_rack(const ArtCtx *c, float x, float y0, float y1,
                        unsigned h, bool vault)
{
    const BbcColour blue = c->p->backdrop;
    const BbcColour led = vault ? c->p->accent : c->p->lamp;
    const float w = 48.0f;
    bbc_frame(c->r, blue, x, y0, w, y1 - y0);
    art_hline(c, blue, x, x + w, y0 + 4.0f);
    bool stripped = vault && (h % 3u) == 0u;
    int slot = 0;
    for (float y = y0 + 8.0f; y < y1 - 8.0f; ++slot)
    {
        unsigned uh = art_mix(h + (unsigned)slot * 0x9e3779b9u);
        ServerUnit kind = (ServerUnit)(uh % 4u);
        if (stripped || (vault && (uh & 0x30u) == 0u))
            kind = SERVER_UNIT_EMPTY;
        float uh_px = kind == SERVER_UNIT_STORAGE ? 12.0f : 6.0f;
        if (y + uh_px > y1 - 6.0f)
            break;
        switch (kind)
        {
        case SERVER_UNIT_SERVER:
            art_hline(c, blue, x + 4.0f, x + w - 4.0f, y + uh_px - 2.0f);
            bbc_plot(c->r, art_blink(led, uh >> 3), x + 8.0f, y + 2.0f);
            if ((uh & 0x100u) != 0u)
                bbc_plot(c->r, led, x + 16.0f, y + 2.0f);
            break;
        case SERVER_UNIT_STORAGE:
            for (float dx = x + 8.0f; dx < x + w - 8.0f; dx += 8.0f)
                art_vline(c, blue, dx, y + 2.0f, y + uh_px - 2.0f);
            bbc_plot(c->r, art_blink(led, uh >> 5), x + w - 8.0f, y + 2.0f);
            break;
        case SERVER_UNIT_SWITCH:
            bbc_stipple(c->r, blue, BBC_DITHER_COLS, x + 8.0f, y + 2.0f,
                        w - 16.0f, 2.0f);
            bbc_plot(c->r, art_blink(BBC_YELLOW, uh >> 9), x + w - 8.0f,
                     y + 2.0f);
            break;
        case SERVER_UNIT_EMPTY:
            break;
        case SERVER_UNIT_BLANK:
        default:
            art_hline(c, blue, x + 4.0f, x + w - 4.0f, y + uh_px - 2.0f);
            break;
        }
        y += uh_px;
    }
    if (stripped)
    {
        /* The patch lead left hanging where the unit behind it was pulled. */
        float lx = x + 20.0f;
        float ly = y0 + art_sy((y1 - y0) * 0.3f);
        bbc_line(c->r, BBC_RED, lx, ly, lx + 8.0f, ly + 16.0f);
        bbc_line(c->r, BBC_RED, lx + 8.0f, ly + 16.0f, lx + 4.0f, ly + 30.0f);
    }
}

static void room_server(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const bool vault = c->theme == LEVEL_THEME_VAULT;

    /* The cable tray: a ladder of rungs between two rails, hung on rods. */
    float tray = top + fminf(10.0f, m->height * 0.1f);
    tray = art_sy(tray);
    art_hline(c, blue, cl, cr, tray);
    art_hline(c, blue, cl, cr, tray + 6.0f);
    bbc_stipple(c->r, blue, BBC_DITHER_COLS, cl, tray + 2.0f, cr - cl, 4.0f);
    for (int i = room_bay(c, m, 64.0f, 0.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 64.0f, 0.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, top, tray);
    }

    /* The racks, a pitch of 64 with a 16-pixel gap that shows the posts of
     * the aisle behind. */
    float rack_top = tray + 10.0f;
    if (floor - rack_top < 24.0f)
        return;
    for (int i = room_bay(c, m, 64.0f, 8.0f, 60.0f);; ++i)
    {
        float x = room_bay_x(c, 64.0f, 8.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x + 52.0f, rack_top + 8.0f, floor);
        server_rack(c, x, rack_top, floor, art_hash(i, (int)m->seed), vault);
    }
}

/* ---- CANTEEN ---------------------------------------------------------- */

/*
 * CANTEEN — the servery, from the tables.
 *
 * A kitchen reads by its planes: the tiled back wall, the hatches through into
 * the kitchen behind it with its heat lamps on, the columns standing in front
 * of that, and then the counter under its canopy with the steam coming off the
 * wells. The dining hall, twelve tiles tall, gets all of that and the extract
 * duct along its head; a low storey is a short servery — hatches and menu
 * boards over a low counter — and a two-tile storey is the back of house, a
 * stainless bench under a shelf of pans. The dado is counted up from the floor
 * in world rows, so two rooms sharing a floor meet on the same grout line.
 */
static void canteen_hatch(const ArtCtx *c, float x, float y, float w,
                          float h, unsigned seed)
{
    const BbcColour blue = c->p->backdrop;
    bbc_frame(c->r, blue, x, y, w, h);
    /* The heat lamps along the head of the opening: the warmest light in the
     * building and the one thing out here further away than the wall. */
    for (float lx = x + 8.0f; lx < x + w - 8.0f; lx += 16.0f)
        bbc_block(c->r, c->p->lamp, lx, y + 4.0f, 2, 1);
    /* The kitchen through it: a shelf of pans and a hood. */
    float shelf = y + art_sy(h * 0.55f);
    art_hline(c, blue, x + 4.0f, x + w - 4.0f, shelf);
    for (int pan = 0; pan < 3; ++pan)
    {
        unsigned ph = art_mix(seed + (unsigned)pan);
        float px = x + 8.0f + (float)(ph % 3u) * 4.0f + (float)pan * 20.0f;
        if (px + 12.0f > x + w - 4.0f)
            break;
        bbc_frame(c->r, blue, px, shelf - 6.0f, 12.0f, 6.0f);
    }
    /* A roller shutter part way down on some of them. */
    if ((seed & 3u) == 0u)
    {
        float down = art_sy(h * 0.4f);
        bbc_fill(c->r, BBC_BLACK, x + 4.0f, y + 2.0f, w - 8.0f, down);
        bbc_stipple(c->r, blue, BBC_DITHER_ROWS, x + 4.0f, y + 2.0f, w - 8.0f,
                    down);
    }
}

static void canteen_steam(const ArtCtx *c, float x, float well, unsigned seed)
{
    if (c->steady)
        return;
    for (int puff = 0; puff < 3; ++puff)
    {
        unsigned h = art_mix(seed + (unsigned)puff * 0x632be5abu);
        float rise = c->t * 14.0f + (float)(h % 40u);
        rise -= 40.0f * floorf(rise / 40.0f);
        float px = x + (float)((h >> 8) % 5u) * PXW +
                   art_sx(sinf(c->t * 1.7f + (float)puff) * 4.0f);
        bbc_plot(c->r, rise < 20.0f ? BBC_WHITE : c->p->backdrop, px,
                 well - 4.0f - rise);
    }
}

static void room_canteen(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float high = m->height;

    /* The dado of glazed tile along the foot of every room on this floor. */
    float dado = art_sy(floor - fminf(48.0f, high * 0.5f));
    art_hline(c, blue, cl, cr, dado);
    for (int i = room_bay(c, m, 32.0f, 0.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 32.0f, 0.0f, i);
        if (x > cr)
            break;
        bbc_stipple(c->r, blue, BBC_DITHER_ROWS, x, dado + 2.0f, PXW,
                    floor - dado - 2.0f);
    }

    if (high <= 64.0f)
    {
        /* Back of house: a stainless bench under a shelf of pans. */
        float shelf = art_sy(top + high * 0.3f);
        art_hline(c, blue, cl, cr, shelf);
        float bench = floor - 20.0f;
        art_hline(c, blue, cl, cr, bench);
        for (int i = room_bay(c, m, 48.0f, 8.0f, 16.0f);; ++i)
        {
            float x = room_bay_x(c, 48.0f, 8.0f, i);
            if (x > cr)
                break;
            unsigned h = art_hash(i, 61 + m->floor_row);
            art_vline(c, blue, x, bench, floor);
            bbc_frame(c->r, blue, x + 8.0f, shelf - 8.0f,
                      12.0f + (float)(h % 3u) * 4.0f, 8.0f);
            if ((h & 8u) != 0u)
                bbc_ring(c->r, blue, x + 28.0f, shelf - 8.0f, 6.0f);
        }
        return;
    }

    /* Hatches into the kitchen, and the menu boards between them. */
    float hatch_top = art_sy(fmaxf(top + 24.0f, dado - 72.0f));
    float hatch_h = art_sy(dado - 24.0f - hatch_top);
    for (int i = room_bay(c, m, 160.0f, 16.0f, 140.0f);; ++i)
    {
        float x = room_bay_x(c, 160.0f, 16.0f, i);
        if (x > cr)
            break;
        unsigned h = art_hash(i, 401 + m->floor_row);
        if (hatch_h >= 24.0f)
            canteen_hatch(c, x, hatch_top, 96.0f, hatch_h, h);
        float mx = x + 112.0f;
        if (hatch_top - 4.0f > top + 8.0f)
        {
            bbc_frame(c->r, blue, mx, hatch_top, 32.0f, 24.0f);
            for (int line = 0; line < 3; ++line)
                art_hline(c, c->p->accent, mx + 4.0f,
                          mx + 12.0f + (float)((h >> line) % 3u) * 4.0f,
                          hatch_top + 6.0f + (float)line * 6.0f);
        }
    }

    /* The counter under its canopy, with the steam off the wells. */
    float counter = floor - 24.0f;
    art_hline(c, blue, cl, cr, counter);
    art_hline(c, blue, cl, cr, counter + 4.0f);
    for (int i = room_bay(c, m, 48.0f, 24.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 48.0f, 24.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, counter + 4.0f, floor);
        canteen_steam(c, x + 16.0f, counter, art_hash(i, 77));
    }
    if (high >= 160.0f)
    {
        float canopy = counter - 56.0f;
        art_hline(c, blue, cl, cr, canopy);
        bbc_stipple(c->r, blue, BBC_DITHER_COLS, cl, canopy + 2.0f, cr - cl,
                    2.0f);
        for (int i = room_bay(c, m, 64.0f, 40.0f, 8.0f);; ++i)
        {
            float x = room_bay_x(c, 64.0f, 40.0f, i);
            if (x > cr)
                break;
            bbc_plot(c->r, BBC_RED, x, canopy + 4.0f);
        }

        /* The tiled columns of the hall, and the extract duct along its
         * head. */
        for (int i = room_bay(c, m, 256.0f, 136.0f, 24.0f);; ++i)
        {
            float x = room_bay_x(c, 256.0f, 136.0f, i);
            if (x > cr)
                break;
            art_vline(c, blue, x, top + 24.0f, counter);
            art_vline(c, blue, x + 16.0f, top + 24.0f, counter);
            art_hline(c, blue, x - 4.0f, x + 24.0f, top + 24.0f);
        }
        art_hline(c, blue, cl, cr, top + 6.0f);
        art_hline(c, blue, cl, cr, top + 18.0f);
        for (int i = room_bay(c, m, 64.0f, 0.0f, 4.0f);; ++i)
        {
            float x = room_bay_x(c, 64.0f, 0.0f, i);
            if (x > cr)
                break;
            art_vline(c, blue, x, top + 6.0f, top + 18.0f);
        }

        /* The tiled frieze high on the hall's back wall, which is what stops
         * a twelve-tile hall reading as a void over a counter: a band of the
         * same glazed tile the dado is, a third of the way down. */
        float frieze = art_sy(top + high * 0.3f);
        art_hline(c, blue, cl, cr, frieze);
        art_hline(c, blue, cl, cr, frieze + 16.0f);
        bbc_stipple(c->r, blue, BBC_DITHER_COLS, cl, frieze + 8.0f, cr - cl,
                    PXH);

        /* The pendants over the tables: a long flex, a shade and the bulb
         * under it, the hall's own lamps rather than the building's. */
        for (int i = room_bay(c, m, 128.0f, 72.0f, 16.0f);; ++i)
        {
            float x = room_bay_x(c, 128.0f, 72.0f, i);
            if (x > cr)
                break;
            unsigned h = art_hash(i, 4447 + m->floor_row);
            float drop = art_sy(frieze + 32.0f + (float)(h % 4u) * 8.0f);
            if (drop > canopy - 24.0f)
                drop = canopy - 24.0f;
            art_vline(c, blue, x + 4.0f, top + 18.0f, drop);
            art_hline(c, blue, x, x + 12.0f, drop);
            art_hline(c, blue, x - 4.0f, x + 16.0f, drop + 2.0f);
            bbc_block(c->r, c->p->lamp, x + 4.0f, drop + 4.0f, 1, 1);
        }
    }
}

/* ---- LAB -------------------------------------------------------------- */

/*
 * LAB — the clean rooms, one storey at a time.
 *
 * A corridor of glazed partitions with the bays behind them, each storey laid
 * out to its own ceiling: a transom panel under the head, the glass down to a
 * kick rail, the frosted band every partition carries at eye height so nobody
 * walks into it, and through the glass the bay's back wall with its shelving
 * and the reagents on it. In front of the partitions the benches stand on the
 * floor; a room with the height for it gets a fume cupboard on its bench.
 */
static void room_lab(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float high = m->height;
    const float transom = top + art_sy(fminf(16.0f, high * 0.15f));
    const float kick = floor - 12.0f;
    const float frost = art_sy(floor - fminf(48.0f, high * 0.6f));

    art_hline(c, blue, cl, cr, transom);
    art_hline(c, blue, cl, cr, kick);
    if (frost > transom + 8.0f && frost + 6.0f < kick)
    {
        bbc_stipple(c->r, blue, BBC_DITHER_CHECK, cl, frost, cr - cl, 6.0f);
    }

    /* The partitions and, through them, the bays. */
    for (int i = room_bay(c, m, 128.0f, 0.0f, 124.0f);; ++i)
    {
        float x = room_bay_x(c, 128.0f, 0.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, top, floor);
        art_vline(c, blue, x + 8.0f, transom, kick);
        unsigned h = art_hash(i, 503 + m->floor_row);
        /* The bay's shelving, a step back behind the glass, and the reagents
         * on it: a few bottles in green and white. */
        float shelf_top = transom + 12.0f;
        for (float sy = shelf_top; sy < frost - 4.0f; sy += 18.0f)
        {
            art_hline(c, blue, x + 24.0f, x + 104.0f, sy);
            for (int b = 0; b < 5; ++b)
            {
                unsigned bh = art_mix(h + (unsigned)b * 7u +
                                      (unsigned)(sy - top));
                if ((bh & 3u) != 0u)
                    continue;
                bbc_block(c->r, (bh & 4u) != 0u ? c->p->accent : BBC_WHITE,
                          x + 28.0f + (float)b * 16.0f, sy - 6.0f, 1, 3);
            }
        }
    }

    /* The benches, and a fume cupboard on every other one in a room tall
     * enough to stand one in. */
    float bench = floor - 24.0f;
    for (int i = room_bay(c, m, 192.0f, 32.0f, 96.0f);; ++i)
    {
        float x = room_bay_x(c, 192.0f, 32.0f, i);
        if (x > cr)
            break;
        art_hline(c, blue, x, x + 96.0f, bench);
        art_vline(c, blue, x + 4.0f, bench, floor);
        art_vline(c, blue, x + 88.0f, bench, floor);
        unsigned h = art_hash(i, 77 + m->floor_row);
        if (high >= 128.0f && (h & 1u) == 0u)
        {
            float hood = bench - 64.0f;
            bbc_frame(c->r, blue, x + 16.0f, hood, 64.0f, 64.0f);
            art_hline(c, blue, x + 16.0f, x + 80.0f, hood + 24.0f);
            art_hline(c, c->p->lamp, x + 24.0f, x + 72.0f, hood + 4.0f);
        }
        else
        {
            bbc_block(c->r, c->p->accent, x + 24.0f, bench - 6.0f, 1, 3);
            bbc_block(c->r, BBC_WHITE, x + 36.0f, bench - 4.0f, 2, 2);
        }
    }
}

/* ---- ARCHIVE ---------------------------------------------------------- */

/*
 * ARCHIVE — stacks running back into the dark under the old brick shell.
 *
 * Two ranks of shelving, and the aisle depth comes from the difference between
 * them rather than from detail: the near rank in solid lines with its box files
 * on it, the far rank in its gaps as broken lines, a step back. A shelf is
 * always the same pitch — a taller storey gets another shelf rather than taller
 * boxes, because a box file is a fixed size — and what is left over under the
 * soffit is where the brick shell shows. The odd file has a label on its spine,
 * which is all the colour paper needs.
 */
enum
{
    ARCHIVE_SHELF_PITCH = 26 /* one box file and the board it stands on */
};

static void room_archive(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float pitch = (float)ARCHIVE_SHELF_PITCH;

    int shelves = (int)((floor - top - 16.0f) / pitch);
    if (shelves < 1)
        shelves = 1;
    if (shelves > 9)
        shelves = 9;
    float stack_top = floor - (float)shelves * pitch;

    /* The brick shell over the stacks. */
    for (float y = top + 4.0f; y < stack_top - 4.0f; y += 16.0f)
    {
        bbc_stipple(c->r, blue, BBC_DITHER_COLS, cl, y, cr - cl, PXH);
        int course = art_repeat(y - c->oy, 16.0f);
        float off = (course & 1) != 0 ? 32.0f : 0.0f;
        for (int i = room_bay(c, m, 64.0f, off, 4.0f);; ++i)
        {
            float x = room_bay_x(c, 64.0f, off, i);
            if (x > cr)
                break;
            art_vline(c, blue, x, y, fminf(y + 16.0f, stack_top - 4.0f));
        }
    }

    /* The far rank, through the aisles. */
    for (int i = room_bay(c, m, 96.0f, 48.0f, 8.0f);; ++i)
    {
        float x = room_bay_x(c, 96.0f, 48.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, stack_top + 4.0f, floor);
        for (int s = 0; s < shelves; ++s)
            bbc_stipple(c->r, blue, BBC_DITHER_COLS, x - 8.0f,
                        floor - (float)s * pitch - 2.0f, 16.0f, 2.0f);
    }

    /* The near rank: units of four bays, a box file to every eight pixels
     * with gaps where something has been taken out. */
    for (int i = room_bay(c, m, 96.0f, 0.0f, 80.0f);; ++i)
    {
        float x = room_bay_x(c, 96.0f, 0.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, stack_top, floor);
        art_vline(c, blue, x + 72.0f, stack_top, floor);
        for (int s = 0; s <= shelves; ++s)
        {
            float board = floor - (float)s * pitch;
            art_hline(c, blue, x, x + 76.0f, board);
            if (s == shelves)
                break;
            for (int f = 0; f < 6; ++f)
            {
                unsigned fh = art_hash(i * 8 + f, s * 17 + m->floor_row);
                if ((fh % 4u) == 0u)
                    continue;
                /* A box file is its spine and nothing else: the near rank
                 * drawn as boxes in outline was a wall of blue, and a row of
                 * spines is what a shelf of files looks like end on. */
                float fx = x + 8.0f + (float)f * 12.0f;
                float fh_px = 14.0f + (float)(fh % 3u) * 2.0f;
                art_vline(c, blue, fx, board - fh_px, board);
                if ((fh & 0x60u) == 0u)
                    bbc_plot(c->r, (fh & 0x80u) != 0u ? c->p->accent
                                                      : c->p->lamp,
                             fx + 4.0f, board - fh_px + 4.0f);
            }
        }
    }

    /* A bulb on a flex in every other bay of the shell. */
    for (int i = room_bay(c, m, 192.0f, 136.0f, 8.0f);; ++i)
    {
        float x = room_bay_x(c, 192.0f, 136.0f, i);
        if (x > cr)
            break;
        float drop = fminf(stack_top - 8.0f, top + 20.0f);
        if (drop < top + 6.0f)
            continue;
        art_vline(c, blue, x, top, drop);
        bbc_plot(c->r, c->p->lamp, x, drop);
    }
}

/* ---- SECURITY --------------------------------------------------------- */

/*
 * SECURITY — the control wing, one room at a time.
 *
 * Console desks on the floor, the monitor wall standing behind them with as
 * many rows of screens as the ceiling allows, a status board in place of every
 * fourth bay, and the standby beacon turning under the ceiling. The screens
 * are the room's light, in cyan, because cyan is the building watching: most
 * show a corridor, the odd one is snow, and one in a while is dark.
 */
static void security_screen(const ArtCtx *c, float x, float y, float w,
                            float h, unsigned seed)
{
    const BbcColour blue = c->p->backdrop;
    bbc_frame(c->r, blue, x, y, w, h);
    unsigned kind = seed % 6u;
    if (kind == 0u)
        return; /* off */
    float ix = x + 4.0f;
    float iy = y + 2.0f;
    float iw = w - 8.0f;
    float ih = h - 4.0f;
    if (kind == 1u)
    {
        /* Snow: a dither whose origin moves, which is the one thing on this
         * floor that is meant to crawl. */
        int frame = c->steady ? 0 : (int)(c->t * 12.0f);
        bbc_set_pattern_origin((float)(frame & 1) * PXW,
                               (float)((frame >> 1) & 1) * PXH);
        bbc_stipple(c->r, BBC_CYAN, BBC_DITHER_SPARSE, ix, iy, iw, ih);
        art_world_pattern(c);
        return;
    }
    if (kind == 2u)
    {
        /* A log scrolling: lines of cyan text, a cursor on the last. */
        for (int line = 0; line < 4; ++line)
        {
            float ly = iy + 2.0f + (float)line * 6.0f;
            if (ly + PXH > iy + ih)
                break;
            float len = 8.0f + (float)((seed >> (line * 3)) % 5u) * 4.0f;
            art_hline(c, BBC_CYAN, ix + 2.0f, fminf(ix + 2.0f + len, ix + iw),
                      ly);
        }
        return;
    }
    /* A corridor seen from a camera at its end: the far wall a small frame
     * in the middle, the four corners of the near end running into it, and
     * somebody standing in it on some of them. */
    float fw = 12.0f;
    float fh = 8.0f;
    float fx = ix + art_sx((iw - fw) * 0.5f);
    float fy = iy + art_sy((ih - fh) * 0.5f);
    bbc_frame(c->r, BBC_CYAN, fx, fy, fw, fh);
    bbc_line(c->r, BBC_CYAN, ix, iy, fx, fy);
    bbc_line(c->r, BBC_CYAN, ix + iw - PXW, iy, fx + fw - PXW, fy);
    bbc_line(c->r, BBC_CYAN, ix, iy + ih - PXH, fx, fy + fh - PXH);
    bbc_line(c->r, BBC_CYAN, ix + iw - PXW, iy + ih - PXH, fx + fw - PXW,
             fy + fh - PXH);
    if ((seed & 8u) != 0u)
        bbc_block(c->r, BBC_WHITE, fx + 4.0f, fy + 2.0f, 1, 3);
}

static void room_security(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float desk = floor - 24.0f;

    /* The monitor wall, as many rows as fit over the desks. */
    const float screen_w = 40.0f;
    const float screen_h = 26.0f;
    float wall_top = top + 16.0f;
    int rows = (int)((desk - 8.0f - wall_top) / (screen_h + 4.0f));
    if (rows > 3)
        rows = 3;
    float wall_y = desk - 8.0f - (float)rows * (screen_h + 4.0f);
    for (int i = room_bay(c, m, 96.0f, 0.0f, 92.0f);; ++i)
    {
        float x = room_bay_x(c, 96.0f, 0.0f, i);
        if (x > cr)
            break;
        unsigned h = art_hash(i, 211 + m->floor_row);
        if ((i & 3) == 3 && rows > 0)
        {
            /* A status board: a grid of lamps, one of them in alarm. */
            float bh = (float)rows * (screen_h + 4.0f) - 4.0f;
            bbc_frame(c->r, blue, x + 4.0f, wall_y, 88.0f, bh);
            int lamp = 0;
            for (float ly = wall_y + 6.0f; ly < wall_y + bh - 4.0f; ly += 8.0f)
                for (float lx = x + 12.0f; lx < x + 84.0f; lx += 12.0f, ++lamp)
                {
                    unsigned lh = art_mix(h + (unsigned)lamp);
                    BbcColour col = (lh % 11u) == 0u ? art_blink(BBC_RED, lh)
                                    : (lh & 1u) != 0u ? BBC_GREEN
                                                      : blue;
                    bbc_plot(c->r, col, lx, ly);
                }
            continue;
        }
        for (int row = 0; row < rows; ++row)
            for (int k = 0; k < 2; ++k)
                security_screen(c, x + 4.0f + (float)k * (screen_w + 4.0f),
                                wall_y + (float)row * (screen_h + 4.0f),
                                screen_w, screen_h,
                                art_mix(h + (unsigned)(row * 2 + k)));
    }

    /* The console desks, with their buttons. */
    art_hline(c, blue, cl, cr, desk);
    art_hline(c, blue, cl, cr, desk + 4.0f);
    for (int i = room_bay(c, m, 64.0f, 16.0f, 60.0f);; ++i)
    {
        float x = room_bay_x(c, 64.0f, 16.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, desk + 4.0f, floor);
        unsigned h = art_hash(i, 313);
        static const BbcColour BUTTONS[3] = {BBC_RED, BBC_GREEN, BBC_YELLOW};
        for (int b = 0; b < 3; ++b)
            if (((h >> b) & 1u) != 0u)
                bbc_plot(c->r, BUTTONS[b], x + 12.0f + (float)b * 8.0f,
                         desk + 8.0f);
    }

    /* The standby beacon under the ceiling: it never quite stops turning. */
    for (int i = room_bay(c, m, 320.0f, 160.0f, 8.0f);; ++i)
    {
        float x = room_bay_x(c, 320.0f, 160.0f, i);
        if (x > cr)
            break;
        art_hline(c, blue, x - 4.0f, x + 8.0f, top + 6.0f);
        bbc_block(c->r,
                  art_beat(c, 0.8f, (float)(i & 3) * 0.25f, 0.5f)
                      ? BBC_RED
                      : BBC_BLACK,
                  x, top + 2.0f, 1, 2);
    }

    /* The annunciator strip in a room with height to spare over its wall. */
    if (wall_y - top > 40.0f)
    {
        float strip = top + 18.0f;
        for (int i = room_bay(c, m, 32.0f, 0.0f, 28.0f);; ++i)
        {
            float x = room_bay_x(c, 32.0f, 0.0f, i);
            if (x > cr)
                break;
            bbc_frame(c->r, blue, x, strip, 24.0f, 8.0f);
            if ((art_hash(i, 707) % 7u) == 0u)
                art_hline(c, c->p->accent, x + 4.0f, x + 20.0f, strip + 4.0f);
        }
    }
}

/* ---- DUCTS ------------------------------------------------------------ */

/*
 * DUCTS — the plenum between floors.
 *
 * The darkest sector in the game has to stay dark, and every storey of it is
 * two tiles high: the underside of the slab above is close overhead with its
 * beams coming end-on toward the viewer, one run of trunking hung just under it
 * on short rods, a pipe under that, another run on stools along the floor, and
 * an inline fan box squeezed in between with its blades turning. A few small
 * lamps measure the distance; nothing else in it is lit.
 */
static void ducts_fan(const ArtCtx *c, float x, float y, float size,
                      unsigned seed)
{
    const BbcColour blue = c->p->backdrop;
    bbc_frame(c->r, blue, x, y, size, size);
    float cx = x + art_sx(size * 0.5f);
    float cy = y + art_sy(size * 0.5f);
    float radius = size * 0.38f;
    bbc_ring(c->r, blue, cx, cy, radius);
    /* Three frames of blade, which is how a fan turned on the machine: a
     * drawing and its successor, never an angle. */
    static const float ANGLES[3] = {0.0f, 1.0472f, 2.0944f};
    int frame = c->steady ? 0 : (int)(c->t * 10.0f) % 3;
    float a = ANGLES[frame];
    for (int blade = 0; blade < 3; ++blade)
    {
        float b = a + (float)blade * 2.0944f;
        bbc_line(c->r, blue, cx, cy, cx + cosf(b) * radius * 0.85f,
                 cy + sinf(b) * radius * 0.85f);
    }
    bbc_plot(c->r, BBC_WHITE, cx, cy);
    bbc_plot(c->r, art_blink(BBC_GREEN, seed), x + size - 8.0f, y + 4.0f);
}

static void room_ducts(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float high = m->height;

    /* The slab's beams, end-on. */
    for (int i = room_bay(c, m, 128.0f, 0.0f, 20.0f);; ++i)
    {
        float x = room_bay_x(c, 128.0f, 0.0f, i);
        if (x > cr)
            break;
        art_hline(c, blue, x, x + 20.0f, top + 8.0f);
        art_vline(c, blue, x + 8.0f, top, top + 8.0f);
    }

    /* The trunking under the slab, flanged every 96 pixels: a flange stands
     * a pixel proud of the duct above and below, which is the whole of how a
     * run of sheet metal reads as sections rather than as a stripe. */
    float duct_top = top + 12.0f;
    float duct_bottom = duct_top + art_sy(fminf(16.0f, high * 0.22f));
    art_hline(c, blue, cl, cr, duct_top);
    art_hline(c, blue, cl, cr, duct_bottom);
    for (int i = room_bay(c, m, 96.0f, 16.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 96.0f, 16.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, duct_top - PXH, duct_bottom + 2.0f * PXH);
    }

    /* The run on stools along the floor, broken where the fan boxes stand. */
    float low = floor - 12.0f;
    bbc_stipple(c->r, blue, BBC_DITHER_COLS, cl, low, cr - cl, PXH);
    for (int i = room_bay(c, m, 128.0f, 24.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 128.0f, 24.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, low, floor);
    }

    /* The inline fan box, every 384 pixels, and a caged lamp every 256. */
    float fan = art_sy(fminf(32.0f, floor - duct_bottom - 6.0f));
    if (fan >= 16.0f)
    {
        for (int i = room_bay(c, m, 384.0f, 192.0f, fan);; ++i)
        {
            float x = room_bay_x(c, 384.0f, 192.0f, i);
            if (x > cr)
                break;
            bbc_fill(c->r, BBC_BLACK, x, floor - fan, fan, fan);
            ducts_fan(c, x, floor - fan, fan, art_hash(i, 5));
        }
    }
    for (int i = room_bay(c, m, 256.0f, 64.0f, 12.0f);; ++i)
    {
        float x = room_bay_x(c, 256.0f, 64.0f, i);
        if (x > cr)
            break;
        bbc_frame(c->r, blue, x, duct_bottom + 2.0f, 12.0f, 6.0f);
        bbc_plot(c->r, c->p->lamp, x + 4.0f, duct_bottom + 4.0f);
    }
}

/* ---- PENTHOUSE -------------------------------------------------------- */

/*
 * PENTHOUSE — a panelled hall hung with pictures and lit by sconces: the last
 * place in the building that looks lived in, and the one floor whose pictures
 * are in the story's own magenta.
 *
 * Panelling is mouldings: a cornice under the ceiling, a dado rail at the
 * height of a hand, raised fields above and below it. The wall is laid out in
 * modules of four bays — a bare field, a painting over a console, a window
 * onto the city, a sconce — and each storey shuffles which of those it gets,
 * so no two floors of the penthouse are hung the same. A hall of two storeys
 * gets a chandelier in the middle of the volume.
 */
static void penthouse_painting(const ArtCtx *c, float x, float y, float w,
                               float h, unsigned seed)
{
    /* Gilt frame, and a picture in the story's colours: a figure against a
     * ground, or a horizon. */
    bbc_frame(c->r, BBC_YELLOW, x, y, w, h);
    float ix = x + 4.0f;
    float iy = y + 2.0f;
    float iw = w - 8.0f;
    float ih = h - 4.0f;
    if ((seed & 1u) != 0u)
    {
        float cx = ix + art_sx(iw * 0.5f) - 4.0f;
        bbc_fill(c->r, BBC_MAGENTA, cx, iy + 4.0f, 8.0f, 6.0f);
        bbc_fill(c->r, BBC_MAGENTA, cx - 4.0f, iy + 10.0f, 16.0f, ih - 10.0f);
        bbc_plot(c->r, BBC_WHITE, cx + 4.0f, iy + 6.0f);
    }
    else
    {
        float horizon = iy + art_sy(ih * 0.6f);
        bbc_stipple(c->r, c->p->backdrop, BBC_DITHER_ROWS, ix, iy, iw,
                    horizon - iy);
        bbc_fill(c->r, BBC_MAGENTA, ix, horizon, iw, iy + ih - horizon);
        bbc_plot(c->r, BBC_YELLOW, ix + art_sx(iw * 0.7f), iy + 4.0f);
    }
}

static void room_penthouse(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;
    const float high = m->height;
    const float dado = floor - 36.0f;
    const float cornice = top + 8.0f;

    art_hline(c, blue, cl, cr, top + 4.0f);
    art_hline(c, blue, cl, cr, cornice);
    art_hline(c, blue, cl, cr, dado);
    art_hline(c, blue, cl, cr, dado + 4.0f);

    /* The wainscot: raised fields under the dado, on the building's grid. */
    for (int i = room_bay(c, m, 48.0f, 0.0f, 44.0f);; ++i)
    {
        float x = room_bay_x(c, 48.0f, 0.0f, i);
        if (x > cr)
            break;
        bbc_frame(c->r, blue, x + 4.0f, dado + 10.0f, 40.0f, floor - dado - 16.0f);
    }

    /* The wall over it, a module of four 96-pixel bays per storey. */
    const float bay = 96.0f;
    float phase = room_phase(m, 0x9du, bay * 4.0f);
    for (int i = room_bay(c, m, bay, phase, bay);; ++i)
    {
        float x = room_bay_x(c, bay, phase, i);
        if (x > cr)
            break;
        float field_top = cornice + 8.0f;
        float field_bottom = dado - 8.0f;
        if (field_bottom - field_top < 16.0f)
            continue;
        unsigned h = art_hash(i, (int)m->seed);
        /* Which of the four a bay is comes off its place in the module and
         * the storey's seed, so the module is the same on every piece of one
         * storey and different on the next. */
        int kind = (int)(((unsigned)((i % 4) + 4) + (m->seed >> 5)) % 4u);
        switch (kind)
        {
        case 0:
            bbc_frame(c->r, blue, x + 8.0f, field_top, 80.0f,
                      field_bottom - field_top);
            break;
        case 1:
        {
            float ph = art_sy(fminf(64.0f, (field_bottom - field_top) * 0.7f));
            penthouse_painting(c, x + 20.0f, field_top + 4.0f, 56.0f, ph, h);
            /* The console under it. */
            art_hline(c, blue, x + 16.0f, x + 80.0f, dado - 12.0f);
            art_vline(c, blue, x + 20.0f, dado - 12.0f, dado);
            art_vline(c, blue, x + 72.0f, dado - 12.0f, dado);
            break;
        }
        case 2:
        {
            /* A window onto the city, down to the floor in a tall room. */
            float wt = field_top;
            float wb = high >= 160.0f ? floor - 4.0f : field_bottom;
            bbc_fill(c->r, BBC_BLACK, x + 16.0f, wt, 64.0f, wb - wt);
            if (room_pane(c, m, x + 16.0f, wt, x + 80.0f, wb))
            {
                art_city(c, x + 16.0f, x + 80.0f, wt, wb + 48.0f, 0.12f, 557u,
                         false);
                room_unpane(c, m);
            }
            bbc_frame(c->r, blue, x + 12.0f, wt - 2.0f, 72.0f, wb - wt + 4.0f);
            art_vline(c, blue, x + 44.0f, wt, wb);
            break;
        }
        default:
            /* A sconce on the pier, gilt bracket and a lit shade. */
            art_hline(c, BBC_YELLOW, x + 40.0f, x + 56.0f,
                      art_sy(field_top + (field_bottom - field_top) * 0.4f));
            bbc_block(c->r, c->p->lamp, x + 44.0f,
                      art_sy(field_top + (field_bottom - field_top) * 0.4f) -
                          4.0f,
                      2, 2);
            break;
        }
    }

    /* A chandelier hanging in the middle of a hall of two storeys. */
    if (high >= 192.0f)
    {
        for (int i = room_bay(c, m, 384.0f, 192.0f, 40.0f);; ++i)
        {
            float x = room_bay_x(c, 384.0f, 192.0f, i);
            if (x > cr)
                break;
            float drop = art_sy(top + high * 0.3f);
            art_vline(c, blue, x, top, drop);
            art_hline(c, BBC_YELLOW, x - 24.0f, x + 28.0f, drop);
            for (int k = -2; k <= 2; ++k)
            {
                float kx = x + (float)k * 12.0f;
                bbc_plot(c->r, BBC_YELLOW, kx, drop + 2.0f);
                bbc_plot(c->r, (k & 1) != 0 ? BBC_WHITE : c->p->lamp, kx,
                         drop - 2.0f);
            }
            art_hline(c, BBC_YELLOW, x - 8.0f, x + 12.0f, drop + 8.0f);
        }
    }
}

/* ---- ROOF ------------------------------------------------------------- */

/*
 * ROOF — the plant level at the top of the tower, behind a curtain wall with
 * the whole city on the other side of the glass. After five climbs on the
 * outside of this building, the last sector puts the drop behind glass.
 *
 * Two distances, and the picture is keeping them apart. The glass, its mullions
 * and transoms and the truss it hangs from are the building: they belong to the
 * room and move with it. The sky and the skyline are a long way off: drawn
 * against the frame, they only sink a little as the camera climbs, so the
 * structure slides past them the way a real one would. The low service level
 * under the roof sees the same city through a ribbon of glass and louvres.
 */
static void roof_sky(const ArtCtx *c, const ArtRoom *m, float glass_top,
                     float glass_bottom)
{
    if (!room_pane(c, m, m->clip_left, glass_top, m->clip_right, glass_bottom))
        return;
    /* Stars, on a field wider than the frame so they turn over off screen,
     * barely moving: they are the one thing out there further off than the
     * city. */
    for (int i = 0; i < 110; ++i)
    {
        unsigned h = art_hash(i * 11 + 5, 883);
        float x = (float)(h % 1040u) - c->cam_x * 0.05f;
        x -= 1040.0f * floorf(x / 1040.0f);
        float y = (float)((h >> 11) % 600u) - c->cam_y * 0.03f;
        y -= 600.0f * floorf(y / 600.0f);
        bbc_plot(c->r, (h & 3u) == 0u ? BBC_WHITE : c->p->backdrop,
                 art_sx(x - 120.0f), art_sy(c->top + y));
    }
    /* The city from the top of the tower is below the eye: its roofs stand
     * along the foot of the glass, sinking a little further as the camera
     * climbs the room. */
    float horizon = art_sy(glass_bottom + 24.0f);
    art_city(c, m->clip_left, m->clip_right, glass_top, horizon, 0.06f, 739u,
             true);
    room_unpane(c, m);
}

static void room_roof(const ArtCtx *c, const ArtRoom *m)
{
    const BbcColour blue = c->p->backdrop;
    const float cl = m->clip_left;
    const float cr = m->clip_right;
    const float top = m->top;
    const float floor = m->floor;

    if (m->height >= 160.0f)
    {
        float glass_top = top + 28.0f;
        float glass_bottom = floor - 8.0f;
        roof_sky(c, m, glass_top, glass_bottom);
        /* The truss the curtain wall hangs from. */
        art_lattice(c, blue, cl, cr, top + 6.0f, top + 22.0f, 32.0f);
        /* Mullions every 128 pixels with a transom across, and the cross
         * bracing of the wind frame in every third bay. */
        float transom = art_sy(glass_top + (glass_bottom - glass_top) * 0.45f);
        art_hline(c, blue, cl, cr, glass_top);
        art_hline(c, blue, cl, cr, transom);
        art_hline(c, blue, cl, cr, glass_bottom);
        for (int i = room_bay(c, m, 128.0f, 0.0f, 128.0f);; ++i)
        {
            float x = room_bay_x(c, 128.0f, 0.0f, i);
            if (x > cr)
                break;
            art_vline(c, blue, x, top + 22.0f, floor);
            art_vline(c, blue, x + 64.0f, glass_top, glass_bottom);
            if (((i % 3) + 3) % 3 == 0)
            {
                bbc_line(c->r, blue, x, glass_top, x + 64.0f, transom);
                bbc_line(c->r, blue, x + 64.0f, glass_top, x, transom);
            }
        }
        return;
    }

    /* The service level: a ribbon of glass over a run of louvres, conduit
     * along the ceiling, and the city in the ribbon. */
    float ribbon_top = top + 8.0f;
    float ribbon_bottom = art_sy(top + m->height * 0.5f);
    if (ribbon_bottom > ribbon_top + 8.0f)
        roof_sky(c, m, ribbon_top, ribbon_bottom);
    art_hline(c, blue, cl, cr, ribbon_top);
    art_hline(c, blue, cl, cr, ribbon_bottom);
    float louvre_bottom = floor - 8.0f;
    if (louvre_bottom > ribbon_bottom + 8.0f)
        bbc_stipple(c->r, blue, BBC_DITHER_ROWS, cl, ribbon_bottom + 4.0f,
                    cr - cl, louvre_bottom - ribbon_bottom - 4.0f);
    for (int i = room_bay(c, m, 96.0f, 0.0f, 4.0f);; ++i)
    {
        float x = room_bay_x(c, 96.0f, 0.0f, i);
        if (x > cr)
            break;
        art_vline(c, blue, x, ribbon_top, floor);
    }
}

/* ---- Exterior backdrops ---------------------------------------------- */

/*
 * How far the backdrop has sunk by the time the camera has climbed this high.
 * The rule, and the two bugs that made it one, are on `level_backdrop_sink` in
 * [level.h](level.h): it is over there because a climb is the only place the
 * camera moves on this axis, and a function in here is a function no test can
 * reach. Snapped on the way out, so a layer sinks a whole BBC row at a time.
 */
static float facade_sink(const ArtCtx *c, float factor)
{
    return art_sy(level_backdrop_sink(&c->level->map, c->s->cam_y,
                                      c->bottom - HUD_HEIGHT, factor));
}

/*
 * The five climbs share one building: the same stone face, floor bands and
 * structural bays. What changes is the hour and the weather, which is what a
 * climber would actually notice between one wall and the next.
 *
 * The face is black and its masonry is the backdrop's blue in lines — the
 * courses and their joints, the pilasters between the window bays, a band
 * under every row of windows, a downpipe — because the climber, the windows,
 * the ledges and everything thrown at him are drawn over it and all of them
 * have to read first. Every line is keyed to the wall's own world position, so
 * the face slides with the ledges rather than under them.
 */
static void facade_shell(const ArtCtx *c, float top, float height)
{
    const BbcColour blue = c->p->backdrop;
    const float face_left = FACADE_BUILDING_SIDE_INSET + c->ox;
    const float face_width = (float)c->level->map.width * (float)TILE_SIZE -
                             FACADE_BUILDING_SIDE_INSET * 2.0f;
    const float face_right = face_left + face_width;
    const float bottom = top + height;

    bbc_fill(c->r, BBC_BLACK, face_left, top, face_width, height);

    /* Ashlar, drawn the way a period game would have drawn it quietly: a
     * dotted bed joint every course of a tile's height, and the perpends as
     * a tick at the joint, staggered half a block on alternate courses. A
     * solid line every course was a blue grid the climber had to be found
     * in, and the climber is what this face exists to be read behind. */
    const float course_h = 32.0f;
    const float block_w = 64.0f;
    for (int k = art_repeat(top - c->oy, course_h) - 1;; ++k)
    {
        float y = c->oy + (float)k * course_h + 16.0f;
        if (y > bottom)
            break;
        bbc_stipple(c->r, blue, BBC_DITHER_COLS, face_left, y, face_width,
                    PXH);
        float off = (k & 1) != 0 ? block_w * 0.5f : 0.0f;
        for (float x = face_left + off; x < face_right; x += block_w)
            art_vline(c, blue, x, y + PXH, y + 3.0f * PXH);
    }

    /* Pilasters between the window bays: a pier of plain stone standing off
     * the coursing, the arris the light finds drawn solid and the one in its
     * own shadow dotted — which is the only relief a black face can have. */
    for (int col = 6; col < c->level->map.width - 3; col += 4)
    {
        float x = ((float)col + 0.5f) * (float)TILE_SIZE + c->ox;
        bbc_fill(c->r, BBC_BLACK, x - 8.0f, top, 16.0f, height);
        art_vline(c, blue, x - 8.0f, top, bottom);
        bbc_stipple(c->r, blue, BBC_DITHER_ROWS, x + 4.0f, top, PXW, height);
    }

    /* A band in the wall gap between two rows of windows, lit along its top
     * edge and dotted along the shadow under it. */
    for (int row = 3; row < c->level->map.height; row += 3)
    {
        float y = (float)(row + 2) * (float)TILE_SIZE + c->oy;
        if (y < top - 8.0f || y > bottom)
            continue;
        bbc_fill(c->r, BBC_BLACK, face_left, y, face_width, 8.0f);
        art_hline(c, blue, face_left, face_right, y);
        bbc_stipple(c->r, blue, BBC_DITHER_COLS, face_left, y + 6.0f,
                    face_width, PXH);
    }

    /* A rainwater downpipe every four bays, bracketed to the stone. A face
     * with no service on it is a drawing of a facade. */
    for (int i = 0;; ++i)
    {
        float x = face_left - FACADE_BUILDING_SIDE_INSET + 40.0f +
                  (float)i * 512.0f;
        if (x > face_right)
            break;
        if (x < face_left + 8.0f || x + 8.0f > face_right - 8.0f)
            continue;
        bbc_fill(c->r, BBC_BLACK, x, top, 8.0f, height);
        art_vline(c, blue, x, top, bottom);
        bbc_stipple(c->r, blue, BBC_DITHER_ROWS, x + 4.0f, top, PXW, height);
        for (int k = art_repeat(top - c->oy, 96.0f);; ++k)
        {
            float by = c->oy + (float)k * 96.0f;
            if (by > bottom)
                break;
            art_hline(c, blue, x - 4.0f, x + 12.0f, by);
        }
    }

    /* The two returns: the face's own corners, which are the only thing that
     * says where a black wall stops against a black sky. */
    art_vline(c, blue, face_left, top, bottom);
    art_vline(c, blue, face_right - PXW, top, bottom);

    /* The parapet along the top of the tower. */
    float roof_y = c->oy;
    if (roof_y >= top - 10.0f && roof_y <= bottom)
    {
        bbc_fill(c->r, BBC_BLACK, face_left - 4.0f, roof_y, face_width + 8.0f,
                 10.0f);
        art_hline(c, c->p->trim, face_left - 4.0f, face_right + 4.0f, roof_y);
        art_hline(c, blue, face_left - 4.0f, face_right + 4.0f, roof_y + 8.0f);
    }
}

/*
 * Distant towers either side of the wall, in two ranks.
 *
 * The far rank is solid blue against the black, its floors ruled in black and
 * the lit windows in yellow — keyed to the tower, the floor and the bay rather
 * than to where the window currently is on screen. `(int)(wx + wy)` is a screen
 * position, and the skyline's own parallax moves it: measured over a climb it
 * repainted every light on the city about thirty times a second, some two
 * thousand windows changing state per second, and only while the camera moved.
 * The nearer rank is lower and black, outlined in the blue along its roofline
 * and its lit flank, with only the odd window lit, and it sinks faster as the
 * climb rises. Depth out here is two planes a step apart and nothing else,
 * because the sky strips either side of the wall are all of the city a climber
 * gets to see.
 */
static void facade_skyline(const ArtCtx *c, int lit_one_in, bool windows)
{
    const BbcColour blue = c->p->backdrop;
    float skyline_shift = facade_sink(c, 0.14f);
    for (int tower = art_repeat(c->cam_x * 0.08f, 104.0f);; ++tower)
    {
        float x = art_sx((float)tower * 104.0f - c->cam_x * 0.08f);
        if (x > c->w)
            break;
        unsigned h = art_hash(tower, 307);
        float tower_w = 72.0f + (float)(h % 15u) * 4.0f;
        float tower_h = art_sy(100.0f + (float)((h >> 7) % 180u));
        float y = c->bottom - tower_h + skyline_shift;
        bbc_fill(c->r, blue, x, y, tower_w, tower_h);
        if (((h >> 15) & 1u) != 0u)
        {
            float cw = art_sx(tower_w * 0.55f);
            float ch = art_sy(12.0f + (float)((h >> 17) % 18u));
            bbc_fill(c->r, blue, x + 8.0f, y - ch, cw, ch);
            if (tower_h > 200.0f)
            {
                float mx = x + 8.0f + art_sx(cw * 0.5f);
                art_vline(c, blue, mx, y - ch - 22.0f, y - ch);
                bbc_block(c->r, art_blink(BBC_RED, h >> 21), mx,
                          y - ch - 24.0f, 1, 1);
            }
        }
        if (!windows)
            continue;
        float sill = y + tower_h - 6.0f;
        float limit = sill < c->bottom ? sill : c->bottom;
        int floor_index = 0;
        for (float wy = y + 16.0f; wy < limit; wy += 20.0f, ++floor_index)
        {
            art_hline(c, BBC_BLACK, x, x + tower_w, wy + 12.0f);
            int bay = 0;
            for (float wx = x + 12.0f; wx < x + tower_w - 8.0f;
                 wx += 16.0f, ++bay)
            {
                unsigned wh = art_hash(tower * 7 + bay, floor_index + 307);
                bbc_block(c->r,
                          (wh % (unsigned)lit_one_in) == 0u ? BBC_YELLOW
                                                            : BBC_BLACK,
                          wx, wy, 1, 3);
            }
        }
    }

    float near_shift = facade_sink(c, 0.22f);
    for (int block = art_repeat(c->cam_x * 0.12f + 60.0f, 136.0f);; ++block)
    {
        float x = art_sx((float)block * 136.0f - c->cam_x * 0.12f - 60.0f);
        if (x > c->w)
            break;
        unsigned h = art_hash(block, 1231);
        float bw = 88.0f + (float)(h % 13u) * 4.0f;
        float bh = art_sy(60.0f + (float)((h >> 6) % 90u));
        float y = c->bottom - bh + near_shift + 20.0f;
        if (y >= c->bottom)
            continue;
        bbc_fill(c->r, BBC_BLACK, x, y, bw, c->bottom - y);
        art_hline(c, blue, x, x + bw, y);
        art_vline(c, blue, x, y, c->bottom);
        /* Rooftop plant on the near blocks: a water tank on its legs. */
        if (((h >> 12) & 1u) != 0u)
        {
            float tx = x + 12.0f + (float)((h >> 13) % 10u) * 4.0f;
            bbc_frame(c->r, blue, tx, y - 14.0f, 16.0f, 10.0f);
            art_vline(c, blue, tx, y - 4.0f, y);
            art_vline(c, blue, tx + 12.0f, y - 4.0f, y);
        }
        if (!windows)
            continue;
        int floor_index = 0;
        for (float wy = y + 10.0f; wy < c->bottom && wy < y + bh - 6.0f;
             wy += 18.0f, ++floor_index)
        {
            for (int bay = 0; bay < 6; ++bay)
            {
                unsigned wh = art_hash(block * 11 + bay, floor_index + 1237);
                if ((wh % 7u) != 0u)
                    continue;
                bbc_block(c->r, BBC_YELLOW, x + 8.0f + (float)bay * 12.0f, wy,
                          1, 2);
            }
        }
    }
}

/*
 * A field of stars. It may wrap, and does — it is a field, and its period is
 * wider than the frame, so a star turns over off screen — and it sinks more
 * slowly than the climb, which sells the height without letting the sky
 * interfere with the route.
 */
static void facade_stars(const ArtCtx *c, int count, float factor,
                         float period, unsigned salt, int white_one_in)
{
    for (int i = 0; i < count; ++i)
    {
        unsigned h = art_hash(i * 13 + 7, (int)salt);
        float x = (float)(h % (unsigned)c->w);
        float y = (float)((h >> 8) % (unsigned)period) - c->s->cam_y * factor;
        y -= period * floorf(y / period);
        BbcColour col =
            (h >> 20) % (unsigned)white_one_in == 0u ? BBC_WHITE
                                                     : c->p->backdrop;
        bbc_plot(c->r, col, art_sx(x), art_sy(c->top + y));
    }
}

/*
 * A mass of cloud: flat underneath, stepped up into a head or two on top, in a
 * half-tone of the blue. The pattern origin travels with the layer it belongs
 * to (`ox`, `oy`), so the checkerboard drifts with the cloud instead of the
 * cloud drifting through a checkerboard pinned to the glass — and every mass on
 * one layer shares it, so two that overlap stay one half-tone rather than
 * closing up solid where their checkerboards disagree.
 */
static void facade_cloud(const ArtCtx *c, BbcColour ink, float ox, float oy,
                         float x, float y, float w, float h, unsigned seed)
{
    x = art_sx(x);
    y = art_sy(y);
    w = art_sx(w);
    bbc_set_pattern_origin(art_sx(ox), art_sy(oy));
    bbc_stipple(c->r, ink, BBC_DITHER_CHECK, x, y, w, h);
    float hx = x + art_sx(w * (0.15f + art_unit(seed, 3) * 0.2f));
    float hw = art_sx(w * 0.4f);
    bbc_stipple(c->r, ink, BBC_DITHER_CHECK, hx, y - 8.0f, hw, 8.0f);
    bbc_stipple(c->r, ink, BBC_DITHER_CHECK, hx + 8.0f, y - 12.0f, hw - 16.0f,
                4.0f);
    if ((seed & 16u) != 0u)
    {
        float kx = x + art_sx(w * 0.6f);
        bbc_stipple(c->r, ink, BBC_DITHER_CHECK, kx, y - 6.0f,
                    art_sx(w * 0.25f), 6.0f);
    }
    art_world_pattern(c);
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
 * It is drawn as the light it puts on the foot of the wall rather than as
 * vehicles, because there is no street in frame: the climb starts several
 * storeys up. What a cordon does to a tower on a BBC is what a light bar does
 * on one — it flashes, red and blue in turn — so the wall's lowest storeys
 * carry patches of that light, scattered pixels of it, out of step with each
 * other because a dozen bars are never in phase, thinning as they rise. They
 * belong to the street, which is at the foot of the map, so the climb leaves
 * them behind. The strength falls away with height across the five climbs,
 * not with the hour — the whole night is thirty-eight minutes long and the
 * ring was already standing before any of it: the third sector is the lowest
 * wall and still in the thick of it, and above the weather there is nothing to
 * see at all.
 */
static void facade_cordon(const ArtCtx *c, float strength)
{
    if (strength <= 0.0f)
        return;
    float street = c->oy + (float)c->level->map.height * (float)TILE_SIZE;
    float reach = art_sy(32.0f + 64.0f * strength);
    if (street - reach > c->bottom)
        return;
    int bars = (int)(6.0f * strength + 0.5f);
    if (bars < 1)
        bars = 1;
    for (int bar = 0; bar < bars; ++bar)
    {
        unsigned h = art_hash(bar * 23, 401);
        float x = art_sx(c->w * (0.04f + (float)(h % 86u) * 0.01f));
        float w = 48.0f + (float)(h >> 8 & 7u) * 8.0f;
        BbcColour col = (bar & 1) != 0 ? bbc_flash_pair(BBC_RED, BBC_BLUE)
                                       : bbc_flash_pair(BBC_BLUE, BBC_RED);
        /* Three tiers, each narrower and thinner than the one under it, so
         * the light falls away up the wall rather than stopping at a box. */
        float low = art_sy(reach * 0.35f);
        float mid = art_sy(reach * 0.7f);
        bbc_stipple(c->r, col, BBC_DITHER_SPARSE, x, street - low, w, low);
        bbc_stipple(c->r, col, BBC_DITHER_DOTS, x + 8.0f, street - mid,
                    w - 16.0f, mid - low);
        bbc_stipple(c->r, col, BBC_DITHER_DOTS, x + 20.0f, street - reach,
                    w - 40.0f, reach - mid);
    }
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
 * The sprite is drawn nose-right, because the traverse below runs left to
 * right: the boom, the fin and the cabin door were once all drawn pointing the
 * other way, so the one aircraft in the sky flew tail-first for the whole of
 * every climb. A nose and a tail boom are the only thing on this layer that
 * says which way an aircraft is going, so reversing the traverse means
 * mirroring the sprite with it.
 */
BBC_SPRITE(SPR_NEWS_HELICOPTER,
           "..2222222222",
           ".......1....",
           "1.....11111.",
           "11111111Y111",
           "1.....111111",
           "......1...1.",
           ".....111111.");

static void facade_news_helicopter(const ArtCtx *c, float top)
{
    /* A long, slow traverse: it is holding a shot, not going anywhere. The
     * wrap happens well off the side of the frame, so it is never seen. */
    float cycle = c->t * 0.045f;
    cycle -= floorf(cycle);
    float x = -90.0f + cycle * (c->w + 180.0f);
    /* It holds one altitude over the city rather than scrolling with the sky,
     * so the climb goes past it: high in the frame from the bottom of the
     * wall, level with Chuck somewhere in the middle, and below him by the
     * top. Wrapping the height the way the stars wrap would put a single
     * recognisable object through a visible jump every few hundred pixels. */
    float y = top + 240.0f - (c->s->cam_y - 500.0f) * 0.45f +
              (c->steady ? 0.0f : sinf(c->t * 0.5f) * 6.0f);
    if (y < top - 20.0f || y > top + 470.0f)
        return;
    const BbcColour slots[BBC_SPRITE_SLOTS] = {c->p->backdrop, BBC_CYAN,
                                               BBC_BLACK, BBC_BLACK};
    float sx = art_sx(x - bbc_sprite_px_w(&SPR_NEWS_HELICOPTER) * 0.5f);
    float sy = art_sy(y);
    bbc_sprite(c->r, &SPR_NEWS_HELICOPTER, sx, sy, 1, slots);
    /* The anti-collision beacon under the hull, on its own beat. */
    if (art_beat(c, 1.35f, 0.0f, 0.18f))
        bbc_plot(c->r, BBC_RED, sx + 7.0f * PXW, sy + 7.0f * PXH);
}

/* Rain in two sheets, a fast near one in cyan and a slow far one in blue;
 * or, on the last climb, sleet — white flecks blown slantwise. */
static void facade_weather(const ArtCtx *c, float top, float height,
                           bool sleet)
{
    for (int drop = 0; drop < 120; ++drop)
    {
        unsigned h = art_hash(drop * 31, 613);
        bool near_sheet = (h & 1u) != 0u;
        if (sleet)
        {
            float speed = near_sheet ? 150.0f : 90.0f;
            float drift = near_sheet ? 70.0f : 40.0f;
            float x = (float)(h % 900u) - c->t * drift +
                      sinf(c->t * 1.3f + (float)drop) * 8.0f;
            x -= 900.0f * floorf(x / 900.0f);
            float y = c->t * speed + (float)((h >> 6) % 800u);
            y -= height * floorf(y / height);
            bbc_block(c->r, near_sheet ? BBC_WHITE : BBC_CYAN,
                      art_sx(x - 50.0f), art_sy(top + y), 1, near_sheet ? 2 : 1);
            continue;
        }
        float speed = near_sheet ? 900.0f : 480.0f;
        float x = (float)(h % 900u) - c->t * 130.0f;
        x -= 900.0f * floorf(x / 900.0f);
        float y = c->t * speed + (float)((h >> 6) % 800u);
        y -= height * floorf(y / height);
        bbc_block(c->r, near_sheet ? BBC_CYAN : c->p->backdrop,
                  art_sx(x - 100.0f), art_sy(top + y), 1, near_sheet ? 6 : 3);
    }
}

static void backdrop_facade(const ArtCtx *c, const LevelThemeArt *art)
{
    const float top = c->top;
    const float height = c->bottom - top;
    const BbcColour blue = c->p->backdrop;

    bbc_fill(c->r, BBC_BLACK, 0.0f, top, c->w, height);
    art_world_pattern(c);

    switch (art->backdrop)
    {
    case BACKDROP_FACADE_STORM:
    {
        /* Lightning is scheduled off a coarse slice of the clock so the whole
         * sky goes at once: white for the strike and cyan for the flicker
         * after it, the towers and the cloud standing black and blue against
         * it. A full-frame flash is exactly the strobe reduced motion exists
         * to take away, so a steady sky simply has no lightning in it. */
        float beat = c->t - 7.3f * floorf(c->t / 7.3f);
        BbcColour sky = BBC_BLACK;
        if (!c->steady)
        {
            if (beat < 0.09f)
                sky = BBC_WHITE;
            else if (beat > 0.16f && beat < 0.24f)
                sky = BBC_CYAN;
        }
        if (sky != BBC_BLACK)
            bbc_fill(c->r, sky, 0.0f, top, c->w, height);
        facade_skyline(c, 6, true);
        /* Low cloud drifting across the towers, torn into masses each of
         * which belongs to its place on the band's own drift, so the deck
         * slides rather than reshuffling. */
        for (int band = 0; band < 4; ++band)
        {
            float y = top + 40.0f + (float)band * 74.0f + facade_sink(c, 0.05f);
            float thick = 18.0f + (float)band * 6.0f;
            float drift = c->steady ? 0.0f : c->t * (7.0f + (float)band * 3.0f);
            const float seg = 72.0f;
            int first = (int)floorf(drift / seg) - 2;
            for (int k = first; k < first + (int)(c->w / seg) + 5; ++k)
            {
                unsigned ch = art_hash(k, 1301 + band);
                if ((ch % 5u) == 0u)
                    continue; /* a tear in the deck */
                float cx = (float)k * seg - drift + (float)(ch % 5u) * 4.0f -
                           40.0f;
                float cw = seg + 16.0f + (float)((ch >> 5) % 12u) * 4.0f;
                float lift = (float)((ch >> 11) % 6u) * 2.0f - 6.0f;
                facade_cloud(c, sky == BBC_BLACK ? blue : BBC_BLUE, -drift, y,
                             cx, y + lift, cw, thick, ch);
            }
        }
        facade_news_helicopter(c, top);
        facade_shell(c, top, height);
        facade_weather(c, top, height,
                       c->theme == LEVEL_THEME_FACADE_SLEET);
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
         * size. On the machine that is all it can have: a white disc with a
         * blue ring for its halo and two seas on it, standing in the strip of
         * sky to the right of the wall where a climber can actually see it.
         */
        float moon_x = c->w - 38.0f;
        float moon_y = art_sy(c->bottom * 0.62f + facade_sink(c, 0.02f));
        bbc_ring(c->r, blue, moon_x, moon_y, 30.0f);
        bbc_disc(c->r, BBC_WHITE, moon_x, moon_y, 20.0f);
        bbc_block(c->r, BBC_CYAN, moon_x - 8.0f, moon_y - 8.0f, 2, 3);
        bbc_block(c->r, BBC_CYAN, moon_x + 4.0f, moon_y + 4.0f, 1, 3);
        facade_skyline(c, 1, false);
        /* Haze lying on the air near the ground, lit from the side by the
         * moon: three rows of it, thinning upward. It is why there are no
         * stars out here: they are up in the HIGH climb, above all of this. */
        bbc_set_pattern_origin(0.0f, top);
        for (int band = 0; band < 3; ++band)
        {
            float y = c->bottom - 150.0f + (float)band * 34.0f;
            bbc_stipple(c->r, blue, band == 2 ? BBC_DITHER_SPARSE
                                              : BBC_DITHER_DOTS,
                        0.0f, y, c->w, 12.0f);
        }
        art_world_pattern(c);
        facade_shell(c, top, height);
        /* Birds off the ledges, far enough out to be scenery rather than the
         * gameplay kind, cold against a cold sky: two drawings of a wingbeat
         * and nothing in between. */
        for (int bird = 0; bird < 5; ++bird)
        {
            unsigned h = art_hash(bird * 7, 55);
            float x = c->t * (13.0f + (float)(h % 9u)) + (float)(h % 640u);
            x -= (c->w + 90.0f) * floorf(x / (c->w + 90.0f));
            x -= 45.0f;
            float y = top + 40.0f + (float)((h >> 5) % 130u);
            bool up = !c->steady && ((int)(c->t * 6.0f + (float)bird) & 1) != 0;
            float bx = art_sx(x);
            float by = art_sy(y);
            bbc_plot(c->r, blue, bx, by);
            bbc_plot(c->r, blue, bx - PXW, by - (up ? 2.0f : 0.0f) * PXH);
            bbc_plot(c->r, blue, bx + PXW, by - (up ? 2.0f : 0.0f) * PXH);
            if (up)
            {
                bbc_plot(c->r, blue, bx - PXW, by - PXH);
                bbc_plot(c->r, blue, bx + PXW, by - PXH);
            }
        }
        break;
    }
    case BACKDROP_FACADE_HIGH:
    {
        /* The one purple in the game, and it is deliberate: seen from this
         * high the sodium-and-neon city sums to a violet haze that no fixture
         * owns, which on this machine is magenta — so the city is a glow of
         * it coming up between torn cloud a long way below rather than a
         * floor of it, under hard stars. */
        facade_stars(c, 120, 0.05f, 900.0f, 733u, 2);
        float deck = art_sy(c->bottom - 90.0f + facade_sink(c, 0.1f));
        /* The glow is drawn as an eight-bit sky is drawn: lines of it, closer
         * together the nearer the city, until they close up solid — a fade
         * the machine could do, where a dither of it reads as a screen. */
        static const float GLOW_GAPS[] = {14.0f, 10.0f, 8.0f, 6.0f, 4.0f,
                                          4.0f,  2.0f,  2.0f};
        float gy = deck - 24.0f;
        for (unsigned g = 0; g < sizeof GLOW_GAPS / sizeof GLOW_GAPS[0]; ++g)
        {
            art_hline(c, BBC_MAGENTA, 0.0f, c->w, gy);
            gy += GLOW_GAPS[g];
        }
        if (gy < c->bottom)
            bbc_fill(c->r, BBC_MAGENTA, 0.0f, gy, c->w, c->bottom - gy);
        for (int puff = 0; puff < 7; ++puff)
        {
            unsigned h = art_hash(puff * 19, 811);
            float x = (c->steady ? 0.0f : c->t * (5.0f + (float)(h % 5u))) +
                      (float)(h % 700u);
            x -= (c->w + 320.0f) * floorf(x / (c->w + 320.0f));
            x -= 160.0f;
            float w = 150.0f + (float)(h % 40u) * 4.0f;
            float py = deck + (float)((h >> 4) % 20u) * 2.0f;
            facade_cloud(c, blue, x, py, x, py, w, 24.0f, h);
            art_hline(c, BBC_MAGENTA, art_sx(x + 8.0f), art_sx(x + w - 8.0f),
                      art_sy(py + 24.0f));
        }
        facade_shell(c, top, height);
        /* The building's own signage, mounted on the face he is climbing in
         * the gap between two rows of windows: the tower's name in neon, in
         * the climb's accent, buzzing out for a beat now and then. Set at
         * the font's largest scale, which is four logical pixels to a font
         * pixel and so lands on the grid like everything else out here. */
        float sign_y = art_sy(250.0f + c->oy);
        if (sign_y > top - 40.0f && sign_y < c->bottom)
        {
            float buzz = c->t * 3.1f;
            buzz -= 6.0f * floorf(buzz / 6.0f);
            BbcColour neon = (!c->steady && buzz < 0.12f) ? BBC_BLACK
                                                          : c->p->accent;
            float sign_x = FACADE_BUILDING_SIDE_INSET + 40.0f + c->ox;
            bbc_text(c->r, sign_x, sign_y, 4.0f, neon, "KESSLER");
        }
        break;
    }
    case BACKDROP_FACADE_NIGHT:
    default:
        facade_stars(c, 60, 0.08f, 700.0f, 211u, 3);
        facade_skyline(c, 3, true);
        facade_news_helicopter(c, top);
        facade_shell(c, top, height);
        break;
    }

    /* Every climb keeps the same aircraft beacon so the five walls read as
     * the same tower at different hours. */
    if (art_beat(c, 0.35f, 0.0f, 0.5f))
        bbc_block(c->r, c->p->accent, c->w - 44.0f, top + 24.0f, 1, 2);

    /*
     * The street the cordon is standing in, last of all, because it is light
     * falling on the wall rather than something behind it.
     *
     * Asked of the **theme**, which is one row per climb, and not of the
     * backdrop, which is not — and asked of `level.c`, which the suite can
     * reach, rather than answered by a switch in here, which it cannot.
     * Keyed on `art->backdrop` it had four answers for five walls, because
     * `FACADE_SLEET` borrows the storm's: the highest climb in the game, two
     * floors under the roof, showed *more* street than the two climbs below
     * it.
     */
    facade_cordon(c, level_theme_cordon(c->theme));
}

/* ---- Entry point ----------------------------------------------------- */

/* The room of whichever theme this is. */
static void backdrop_room(const ArtCtx *c, const LevelThemeArt *art,
                          const ArtRoom *room)
{
    switch (art->backdrop)
    {
    case BACKDROP_LOBBY:
        room_lobby(c, room);
        break;
    case BACKDROP_OFFICE:
        room_office(c, room);
        break;
    case BACKDROP_SERVER:
        room_server(c, room);
        break;
    case BACKDROP_CANTEEN:
        room_canteen(c, room);
        break;
    case BACKDROP_LAB:
        room_lab(c, room);
        break;
    case BACKDROP_ARCHIVE:
        room_archive(c, room);
        break;
    case BACKDROP_SECURITY:
        room_security(c, room);
        break;
    case BACKDROP_DUCTS:
        room_ducts(c, room);
        break;
    case BACKDROP_PENTHOUSE:
        room_penthouse(c, room);
        break;
    case BACKDROP_ROOF:
        room_roof(c, room);
        break;
    case BACKDROP_FACADE_NIGHT:
    case BACKDROP_FACADE_STORM:
    case BACKDROP_FACADE_MOON:
    case BACKDROP_FACADE_HIGH:
    case BACKDROP_RESTROOM:
        break; /* never reached: neither is drawn per room */
    case BACKDROP_PLANT:
    default:
        room_plant(c, room);
        break;
    }
}

void level_art_backdrop(const LevelArtScene *s)
{
    const LevelThemeArt *art = level_art(s->level->map.theme);
    /* A window that reports nothing while it is minimised has no frame to lay
     * a backdrop out in, and several layers below take a hash modulo its
     * width: `% 0` is a trap rather than a wrong pixel. */
    if (s->win_w < 8 || (float)s->win_h <= HUD_HEIGHT)
        return;
    ArtCtx c = art_ctx(s);

    switch (art->backdrop)
    {
    case BACKDROP_FACADE_NIGHT:
    case BACKDROP_FACADE_STORM:
    case BACKDROP_FACADE_MOON:
    case BACKDROP_FACADE_HIGH:
        /* A climb is outside: one sky for the whole frame, sinking as the
         * wall rises (see `level_backdrop_sink`). */
        backdrop_facade(&c, art);
        art_world_pattern(&c);
        return;
    case BACKDROP_RESTROOM:
        return; /* world_render.c derives the room from its own wall ring. */
    default:
        break;
    }

    /* Behind everything, the building's own structure: what a gap across a
     * slab — a ladder going through it, a pair of panels — shows, because a
     * hole in a floor is a hole and not another slice of somebody's wall. On
     * this machine that is black, which is what the rooms are too. */
    bbc_fill(c.r, BBC_BLACK, 0.0f, c.top, c.w, c.bottom - c.top);
    art_world_pattern(&c);

    static LevelBackdropPlan plan;
    level_backdrop_plan(s->level, &plan);
    for (int i = 0; i < plan.piece_count; ++i)
    {
        const LevelBackdropPiece *piece = &plan.pieces[i];
        if (piece->room < 0)
            continue;
        float x0 = (float)piece->col0 * (float)TILE_SIZE + c.ox;
        float x1 = (float)(piece->col1 + 1) * (float)TILE_SIZE + c.ox;
        float y0 = (float)piece->top * (float)TILE_SIZE + c.oy;
        float y1 = (float)(piece->bottom + 1) * (float)TILE_SIZE + c.oy;
        float cx0 = fmaxf(x0, 0.0f);
        float cx1 = fminf(x1, c.w);
        float cy0 = fmaxf(y0, c.top);
        float cy1 = fminf(y1, c.bottom);
        if (cx1 <= cx0 || cy1 <= cy0)
            continue;

        const LevelBackdropRoom *layout = &plan.rooms[piece->room];
        ArtRoom room;
        room.left = (float)layout->col0 * (float)TILE_SIZE + c.ox;
        room.right = (float)(layout->col1 + 1) * (float)TILE_SIZE + c.ox;
        room.top = (float)layout->top * (float)TILE_SIZE + c.oy;
        room.floor = (float)layout->floor * (float)TILE_SIZE + c.oy;
        room.height = room.floor - room.top;
        room.clip_left = cx0;
        room.clip_right = cx1;
        room.clip_top = cy0;
        room.clip_bottom = cy1;
        room.top_row = layout->top;
        room.floor_row = layout->floor;
        room.seed = art_hash(layout->floor * 131 + s->level_index,
                             layout->top);

        art_clip(&c, cx0, cy0, cx1, cy1);
        backdrop_room(&c, art, &room);
        SDL_SetRenderClipRect(c.r, NULL);
    }
    art_world_pattern(&c);
}
