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
        WALL_STYLE_PLATE, BACKDROP_PLANT,
        {41, 51, 64, 255}, {20, 27, 39, 255}, {88, 104, 120, 255},
        {104, 122, 136, 255}, {168, 184, 192, 255}, {168, 112, 40, 255},
        {7, 10, 18, 255}, {20, 30, 42, 255},
        {13, 18, 29, 255}, {18, 25, 38, 255},
        {248, 202, 118, 255}, {130, 162, 170, 255}, 70},

    /* LOBBY — the ground floor Chuck walks in through: stone, brass and a
     * glazed street front, the only sector still lit for the public. */
    [LEVEL_THEME_LOBBY] = {
        WALL_STYLE_MARBLE, BACKDROP_LOBBY,
        {88, 84, 80, 255}, {38, 36, 36, 255}, {162, 154, 140, 255},
        {158, 132, 86, 255}, {228, 206, 158, 255}, {228, 186, 104, 255},
        {14, 17, 26, 255}, {32, 34, 42, 255},
        {18, 22, 33, 255}, {30, 33, 44, 255},
        {250, 222, 164, 255}, {186, 176, 150, 255}, 96},

    /* OFFICE — open plan after hours: painted partitions, blinds, and the
     * one bank of lights nobody switched off. */
    [LEVEL_THEME_OFFICE] = {
        WALL_STYLE_DRYWALL, BACKDROP_OFFICE,
        {58, 66, 78, 255}, {26, 31, 40, 255}, {112, 124, 140, 255},
        {150, 160, 172, 255}, {198, 208, 216, 255}, {84, 150, 168, 255},
        {10, 16, 26, 255}, {26, 36, 50, 255},
        {15, 21, 33, 255}, {24, 32, 46, 255},
        {206, 226, 236, 255}, {140, 166, 182, 255}, 84},

    /* SERVER — a cold aisle. Almost no white light; the room is lit by the
     * equipment it exists to hold. */
    [LEVEL_THEME_SERVER] = {
        WALL_STYLE_PLATE, BACKDROP_SERVER,
        {26, 34, 42, 255}, {10, 15, 20, 255}, {62, 84, 96, 255},
        {66, 116, 122, 255}, {148, 214, 208, 255}, {74, 222, 212, 255},
        {4, 10, 16, 255}, {9, 23, 31, 255},
        {8, 16, 22, 255}, {12, 24, 32, 255},
        {96, 226, 220, 255}, {90, 180, 190, 255}, 58},

    /* CANTEEN — glazed tile and stainless steel under heat lamps; the
     * warmest, brightest sector of the climb. */
    [LEVEL_THEME_CANTEEN] = {
        WALL_STYLE_TILE, BACKDROP_CANTEEN,
        {126, 132, 128, 255}, {52, 58, 57, 255}, {200, 204, 192, 255},
        {126, 120, 104, 255}, {212, 202, 172, 255}, {236, 178, 84, 255},
        {18, 20, 22, 255}, {36, 38, 35, 255},
        {24, 26, 26, 255}, {42, 44, 41, 255},
        {252, 214, 146, 255}, {170, 168, 150, 255}, 104},

    /* LAB — clean room: pale tile, green-lit cabinets, everything sterile
     * and slightly too bright. */
    [LEVEL_THEME_LAB] = {
        WALL_STYLE_TILE, BACKDROP_LAB,
        {150, 164, 162, 255}, {58, 72, 74, 255}, {218, 228, 222, 255},
        {112, 150, 148, 255}, {212, 238, 230, 255}, {110, 230, 182, 255},
        {14, 24, 28, 255}, {32, 50, 52, 255},
        {20, 32, 36, 255}, {32, 50, 52, 255},
        {216, 246, 238, 255}, {160, 200, 196, 255}, 110},

    /* ARCHIVE — old brick shell kept from the original building, filled
     * floor to ceiling with paper and dust. */
    [LEVEL_THEME_ARCHIVE] = {
        WALL_STYLE_BRICK, BACKDROP_ARCHIVE,
        {92, 62, 48, 255}, {40, 27, 22, 255}, {146, 102, 74, 255},
        {126, 96, 58, 255}, {192, 152, 96, 255}, {224, 164, 72, 255},
        {16, 12, 12, 255}, {36, 27, 23, 255},
        {22, 17, 15, 255}, {36, 28, 23, 255},
        {244, 196, 116, 255}, {180, 152, 110, 255}, 78},

    /* SECURITY — the control wing: dark steel, a wall of monitors and a
     * standby beacon that never quite stops turning. */
    [LEVEL_THEME_SECURITY] = {
        WALL_STYLE_PLATE, BACKDROP_SECURITY,
        {46, 40, 44, 255}, {18, 14, 17, 255}, {92, 76, 80, 255},
        {112, 68, 62, 255}, {202, 130, 114, 255}, {232, 74, 62, 255},
        {10, 6, 10, 255}, {28, 15, 17, 255},
        {16, 10, 13, 255}, {26, 16, 19, 255},
        {236, 120, 96, 255}, {170, 120, 120, 255}, 64},

    /* DUCTS — the plenum between floors: galvanised trunking, no fixtures,
     * the darkest sector in the game. */
    [LEVEL_THEME_DUCTS] = {
        WALL_STYLE_PLATE, BACKDROP_DUCTS,
        {44, 48, 54, 255}, {18, 20, 24, 255}, {104, 112, 120, 255},
        {120, 130, 138, 255}, {186, 196, 202, 255}, {206, 166, 92, 255},
        {6, 8, 11, 255}, {17, 21, 26, 255},
        {12, 15, 19, 255}, {20, 24, 30, 255},
        {198, 214, 222, 255}, {120, 134, 146, 255}, 40},

    /* PENTHOUSE — the executive floor: hardwood panelling, sconces and
     * money, one storey below the roof. */
    [LEVEL_THEME_PENTHOUSE] = {
        WALL_STYLE_WOOD, BACKDROP_PENTHOUSE,
        {74, 48, 32, 255}, {32, 20, 14, 255}, {134, 90, 56, 255},
        {172, 138, 80, 255}, {230, 196, 132, 255}, {236, 196, 120, 255},
        {18, 13, 14, 255}, {40, 29, 26, 255},
        {26, 18, 18, 255}, {42, 30, 26, 255},
        {250, 208, 140, 255}, {190, 164, 128, 255}, 92},

    /* ROOF — the last sector: raw concrete plant room behind a curtain wall
     * with the whole city on the other side of the glass. */
    [LEVEL_THEME_ROOF] = {
        WALL_STYLE_CONCRETE, BACKDROP_ROOF,
        {68, 72, 78, 255}, {28, 31, 36, 255}, {124, 130, 136, 255},
        {96, 104, 112, 255}, {170, 180, 188, 255}, {110, 200, 220, 255},
        {6, 10, 20, 255}, {18, 26, 42, 255},
        {12, 17, 28, 255}, {20, 28, 42, 255},
        {190, 220, 240, 255}, {140, 170, 196, 255}, 50},

    /* RESTROOM — the sublevel. Its interior is derived from the room's own
     * wall ring in game_render.c; only the tile material comes from here. */
    [LEVEL_THEME_RESTROOM] = {
        WALL_STYLE_TILE, BACKDROP_RESTROOM,
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
        WALL_STYLE_CONCRETE, BACKDROP_FACADE_NIGHT,
        {43, 43, 47, 255}, {30, 31, 35, 255}, {59, 58, 60, 255},
        {88, 84, 78, 255}, {126, 120, 108, 255}, {228, 54, 48, 255},
        {7, 12, 29, 255}, {25, 32, 48, 255},
        {10, 16, 28, 255}, {70, 69, 55, 255},
        {220, 158, 76, 255}, {155, 194, 218, 255}, 0},

    /* STORM — the second: rain, wet stone and lightning off the skyline. */
    [LEVEL_THEME_FACADE_STORM] = {
        WALL_STYLE_CONCRETE, BACKDROP_FACADE_STORM,
        {36, 38, 42, 255}, {24, 26, 30, 255}, {58, 62, 68, 255},
        {70, 72, 74, 255}, {114, 118, 120, 255}, {236, 84, 64, 255},
        {10, 13, 20, 255}, {32, 36, 44, 255},
        {13, 16, 23, 255}, {58, 62, 66, 255},
        {198, 178, 120, 255}, {170, 196, 214, 255}, 0},

    /* DAWN — the third: the sun comes up mid-climb and the stone goes warm. */
    [LEVEL_THEME_FACADE_DAWN] = {
        WALL_STYLE_CONCRETE, BACKDROP_FACADE_DAWN,
        {86, 72, 64, 255}, {62, 52, 48, 255}, {118, 100, 86, 255},
        {158, 132, 104, 255}, {214, 186, 148, 255}, {232, 96, 72, 255},
        {44, 38, 74, 255}, {236, 150, 96, 255},
        {52, 44, 62, 255}, {120, 96, 74, 255},
        {252, 214, 150, 255}, {252, 206, 158, 255}, 0},

    /* HIGH — the last: above the weather, a sea of cloud below and the
     * building's own neon signage for company. */
    [LEVEL_THEME_FACADE_HIGH] = {
        WALL_STYLE_CONCRETE, BACKDROP_FACADE_HIGH,
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
    fx_rect(r, fx_mix(base, art->wall_dark, 0.25f), x, y + 16.0f,
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
                               float x, float y, unsigned h)
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
    if ((h % 19u) == 0u)
    {
        /* A cracked tile, drawn across the grout so it reads as damage. */
        fx_set(r, fx_mix(art->wall, art->wall_dark, 0.8f));
        SDL_RenderLine(r, x + 6.0f, y + 7.0f, x + 14.0f, y + 17.0f);
        SDL_RenderLine(r, x + 14.0f, y + 17.0f, x + 11.0f, y + 26.0f);
    }
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

    /* Veining: two slack diagonals per slab, keyed off the slab hash so the
     * pattern continues across the two tiles that share a slab. */
    SDL_Color vein = fx_mix(base, art->wall_light, 0.75f);
    fx_set(r, vein);
    float vx = x + art_unit(sh, 5) * 20.0f;
    SDL_RenderLine(r, vx, y, vx + 11.0f, y + 15.0f);
    SDL_RenderLine(r, vx + 11.0f, y + 15.0f, vx + 5.0f, y + TILE_SIZE);
    fx_set(r, fx_mix(base, art->wall_dark, 0.35f));
    SDL_RenderLine(r, vx + 14.0f, y, vx + 6.0f, y + TILE_SIZE);

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
        wall_tile_material(r, art, x, y, h);
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

    /* Edge treatment is shared by every material: the surfaces the player
     * actually stands on, walks past and jumps under have to read the same
     * way in all fifteen sectors or the level stops being legible. */
    if (!level_is_solid(level, col, row - 1))
    {
        fx_rect(r, art->trim, x, y, TILE_SIZE, 3.0f);
        fx_rect(r, art->trim_hi, x + 1.0f, y, TILE_SIZE - 2.0f, 1.0f);
        fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.6f), x, y + 3.0f,
                TILE_SIZE, 1.0f);
        if ((h & 3u) == 0u)
            fx_rect(r, art->accent, x + (float)(h % 14u) + 4.0f, y + 1.0f,
                    9.0f, 2.0f);
    }
    if (!level_is_solid(level, col, row + 1))
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.4f), x,
                y + TILE_SIZE - 2.0f, TILE_SIZE, 2.0f);
    if (!level_is_solid(level, col - 1, row))
        fx_rect(r, fx_mix(art->wall_light, art->trim, 0.4f), x, y,
                2.0f, TILE_SIZE);
    if (!level_is_solid(level, col + 1, row))
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.25f),
                x + TILE_SIZE - 2.0f, y, 2.0f, TILE_SIZE);
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
    for (float x = art_scroll(s->cam_x, 0.10f, 224.0f) - 48.0f;
         x < (float)s->win_w + 224.0f; x += 224.0f)
    {
        unsigned seed = (unsigned)((int)(x + s->cam_x * 0.10f) / 224);
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
        fx_rect(r, fx_dim((SDL_Color){194, 84, 66, 255}, blink),
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
            unsigned h = art_hash((int)(x + s->cam_x * 0.18f) / 8 + lamp,
                                  s->level_index + 7);
            bool warm = (h & 1u) != 0u;
            float flicker = warm && ((h >> 3) & 7u) == 0u &&
                                    fmodf(s->time * 1.7f + (float)lamp, 4.0f) <
                                        0.09f
                                ? 0.3f
                                : 1.0f;
            SDL_Color lc = warm
                               ? fx_dim((SDL_Color){126, 88, 44, 255}, flicker)
                               : (SDL_Color){40, 96, 104, 255};
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
                      (SDL_Color){120, 190, 196, 255}, 13);
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

static void backdrop_lobby(const LevelArtScene *s, const LevelThemeArt *art,
                           float oy, float fh)
{
    SDL_Renderer *r = s->renderer;
    float street = oy + fh * 0.62f;

    /* Behind the glass: the street Chuck just came off, and the city he is
     * about to climb. Traffic keeps moving whether he is watching or not. */
    for (float x = art_scroll(s->cam_x, 0.06f, 168.0f) - 40.0f;
         x < (float)s->win_w + 168.0f; x += 168.0f)
    {
        unsigned h = fx_hash((unsigned)((int)(x + s->cam_x * 0.06f) / 168) *
                             2654435761u);
        float tower_h = 90.0f + (float)(h % 120u);
        fx_rect(r, art->far_shape, x + 12.0f, street - tower_h, 92.0f, tower_h);
        for (float wy = street - tower_h + 12.0f; wy < street - 8.0f;
             wy += 20.0f)
        {
            for (float wx = x + 20.0f; wx < x + 96.0f; wx += 18.0f)
            {
                if ((((int)wx * 7 + (int)wy) % 5) != 0)
                    continue;
                fx_rect_a(r, art->lamp, 110, wx, wy, 5.0f, 7.0f);
            }
        }
    }
    fx_rect(r, fx_mix(art->far_shape, FX_INK, 0.4f), 0.0f, street,
            (float)s->win_w, oy + fh - street);
    for (int car = 0; car < 3; ++car)
    {
        float speed = 46.0f + (float)car * 21.0f;
        float x = fmodf(s->time * speed + (float)car * 190.0f,
                        (float)s->win_w + 150.0f) -
                  75.0f;
        fx_glow(r, x, street + 18.0f + (float)car * 7.0f, 26.0f,
                (SDL_Color){248, 226, 176, 255}, 58);
        fx_glow(r, x + 44.0f, street + 18.0f + (float)car * 7.0f, 18.0f,
                (SDL_Color){228, 82, 62, 255}, 44);
    }

    /* The glazed front itself: full-height mullions, a transom, and the
     * reflection band that tells you it is glass and not an opening. */
    for (float x = art_scroll(s->cam_x, 0.16f, 84.0f);
         x < (float)s->win_w + 84.0f; x += 84.0f)
    {
        fx_rect(r, art->near_shape, x, oy, 7.0f, fh);
        fx_rect(r, fx_mix(art->near_shape, art->trim, 0.45f), x + 1.0f, oy,
                2.0f, fh);
        fx_rect_a(r, art->trim_hi, 16, x + 12.0f, oy, 26.0f, fh);
    }
    fx_rect(r, art->near_shape, 0.0f, oy + 74.0f, (float)s->win_w, 8.0f);
    fx_rect(r, fx_mix(art->near_shape, art->trim, 0.5f), 0.0f, oy + 74.0f,
            (float)s->win_w, 2.0f);

    /* Reception: a stone counter, a lit backing panel and planting. Read at
     * a glance as "the building's front of house", then stays out of the way. */
    for (float x = art_scroll(s->cam_x, 0.28f, 340.0f);
         x < (float)s->win_w + 340.0f; x += 340.0f)
    {
        float desk_y = oy + fh - 118.0f;
        fx_rect_a(r, art->trim_hi, 26, x + 30.0f, oy + 40.0f, 150.0f, 90.0f);
        fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.4f), x + 30.0f,
                oy + 40.0f, 150.0f, 90.0f);
        fx_rect(r, art->trim, x + 30.0f, oy + 40.0f, 150.0f, 3.0f);
        for (int letter = 0; letter < 5; ++letter)
            fx_rect_a(r, art->trim_hi, 150, x + 58.0f + (float)letter * 20.0f,
                      oy + 74.0f, 12.0f, 18.0f);

        fx_rect(r, fx_mix(art->wall, art->wall_light, 0.25f), x + 46.0f, desk_y,
                120.0f, 46.0f);
        fx_rect(r, art->trim, x + 42.0f, desk_y, 128.0f, 5.0f);
        fx_rect(r, art->trim_hi, x + 42.0f, desk_y, 128.0f, 2.0f);
        fx_glow(r, x + 106.0f, desk_y - 4.0f, 60.0f, art->lamp, 40);

        /* Two palms in stone planters, the universal lobby plant. */
        for (int plant = 0; plant < 2; ++plant)
        {
            float px = x + 210.0f + (float)plant * 58.0f;
            float py = oy + fh - 74.0f;
            fx_rect(r, fx_mix(art->wall, art->wall_dark, 0.3f), px, py,
                    26.0f, 30.0f);
            fx_rect(r, art->trim, px, py, 26.0f, 3.0f);
            for (int frond = 0; frond < 5; ++frond)
            {
                float lean = ((float)frond - 2.0f) * 9.0f +
                             sinf(s->time * 0.6f + (float)frond) * 2.0f;
                fx_set(r, (SDL_Color){44, 74, 52, 255});
                SDL_RenderLine(r, px + 13.0f, py, px + 13.0f + lean, py - 34.0f);
            }
        }
    }
}

static void backdrop_office(const LevelArtScene *s, const LevelThemeArt *art,
                            float oy, float fh)
{
    SDL_Renderer *r = s->renderer;

    /* A window band with the neighbouring tower behind it: enough city to
     * place the floor in the building, dim enough to stay background. */
    for (float x = art_scroll(s->cam_x, 0.07f, 150.0f);
         x < (float)s->win_w + 150.0f; x += 150.0f)
    {
        fx_rect(r, art->far_shape, x, oy + 30.0f, 128.0f, fh - 60.0f);
        for (float wy = oy + 42.0f; wy < oy + fh - 60.0f; wy += 22.0f)
            for (float wx = x + 10.0f; wx < x + 118.0f; wx += 20.0f)
                if ((((int)wx + (int)wy) % 7) == 0)
                    fx_rect_a(r, art->lamp, 70, wx, wy, 6.0f, 8.0f);
    }
    /* Vertical blinds, half of them closed. */
    for (float x = art_scroll(s->cam_x, 0.12f, 14.0f);
         x < (float)s->win_w + 14.0f; x += 14.0f)
    {
        unsigned h = fx_hash((unsigned)((int)(x + s->cam_x * 0.12f) / 14));
        fx_rect_a(r, art->trim_hi, (h & 1u) ? 34 : 16, x, oy + 24.0f, 9.0f,
                  fh * 0.55f);
    }

    /* The cubicle farm. Screens left logged in are the only light source at
     * this hour, so they carry the layer. */
    for (float x = art_scroll(s->cam_x, 0.22f, 176.0f) - 20.0f;
         x < (float)s->win_w + 176.0f; x += 176.0f)
    {
        float top = oy + fh - 150.0f;
        unsigned bay = fx_hash((unsigned)((int)(x + s->cam_x * 0.22f) / 176) +
                               (unsigned)s->level_index);
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
    for (float x = art_scroll(s->cam_x, 0.3f, 60.0f);
         x < (float)s->win_w + 60.0f; x += 60.0f)
    {
        fx_rect(r, fx_mix(art->near_shape, FX_INK, 0.4f), x, oy, 2.0f, 20.0f);
        unsigned h = fx_hash((unsigned)((int)(x + s->cam_x * 0.3f) / 60));
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

    for (float x = art_scroll(s->cam_x, 0.24f, 132.0f) - 30.0f;
         x < (float)s->win_w + 132.0f; x += 132.0f)
    {
        float top = oy + 46.0f;
        float height = fh - 108.0f;
        unsigned rack = fx_hash((unsigned)((int)(x + s->cam_x * 0.24f) / 132) +
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
            SDL_Color led = (uh & 4u) ? art->accent
                                      : (SDL_Color){120, 240, 150, 255};
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

    for (float x = art_scroll(s->cam_x, 0.24f, 216.0f);
         x < (float)s->win_w + 216.0f; x += 216.0f)
    {
        float bench = oy + fh - 104.0f;
        unsigned bay = fx_hash((unsigned)((int)(x + s->cam_x * 0.24f) / 216));

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
        for (float x = art_scroll(s->cam_x, factor, period) - 30.0f;
             x < (float)s->win_w + period; x += period)
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
                unsigned bay = (unsigned)((int)(x + s->cam_x * factor) /
                                          (int)period) +
                               (unsigned)rank * 977u;
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
    for (float x = art_scroll(s->cam_x, 0.16f, 150.0f) - 20.0f;
         x < (float)s->win_w + 150.0f; x += 150.0f)
    {
        float top = oy + 34.0f;
        fx_rect(r, art->far_shape, x, top, 132.0f, 132.0f);
        for (int screen = 0; screen < 9; ++screen)
        {
            float sx = x + 6.0f + (float)(screen % 3) * 42.0f;
            float sy = top + 6.0f + (float)(screen / 3) * 42.0f;
            unsigned h = fx_hash((unsigned)((int)(x + s->cam_x * 0.16f) / 150) *
                                     31u +
                                 (unsigned)screen);
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

    for (float x = art_scroll(s->cam_x, 0.2f, 260.0f);
         x < (float)s->win_w + 260.0f; x += 260.0f)
    {
        unsigned h = fx_hash((unsigned)((int)(x + s->cam_x * 0.2f) / 260) +
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
    for (float x = art_scroll(s->cam_x, 0.05f, 132.0f) - 40.0f;
         x < (float)s->win_w + 132.0f; x += 132.0f)
    {
        unsigned h = fx_hash((unsigned)((int)(x + s->cam_x * 0.05f) / 132) * 71u);
        float tower_h = 120.0f + (float)(h % 150u);
        float base = oy + fh * 0.78f;
        fx_rect(r, art->far_shape, x, base - tower_h, 84.0f, tower_h);
        fx_rect(r, fx_mix(art->far_shape, art->accent, 0.2f), x,
                base - tower_h, 84.0f, 2.0f);
        for (float wy = base - tower_h + 10.0f; wy < base - 6.0f; wy += 16.0f)
            for (float wx = x + 8.0f; wx < x + 76.0f; wx += 14.0f)
                if ((((int)wx * 3 + (int)wy) % 6) < 2)
                    fx_rect_a(r, art->lamp, 90, wx, wy, 4.0f, 5.0f);
        /* Aircraft warning light on the tallest neighbours. */
        if ((h % 3u) == 0u)
        {
            float pulse = sinf(s->time * 1.6f + (float)(h % 5u)) > 0.7f ? 1.0f
                                                                       : 0.15f;
            fx_glow(r, x + 42.0f, base - tower_h - 4.0f, 14.0f,
                    (SDL_Color){236, 78, 62, 255}, (Uint8)(90.0f * pulse));
        }
    }
    fx_vgrad(r, 0.0f, oy + fh * 0.62f, (float)s->win_w, fh * 0.38f,
             art->far_shape, 0, art->far_shape, 190);

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
        fx_rect(r, art->wall_dark, face_left, y, face_width, 1.0f);
        float joint_offset = (course & 1) != 0 ? 32.0f : 0.0f;
        for (float x = face_left + joint_offset; x < face_right; x += 64.0f)
            fx_rect(r, fx_mix(art->wall_dark, art->wall, 0.5f), x, y,
                    1.0f, 16.0f);
    }

    /* Window bays separated by shallow stone pilasters. */
    for (int col = 6; col < s->level->map.width - 3; col += 4)
    {
        float x = ((float)col + 0.5f) * (float)TILE_SIZE - s->cam_x;
        fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.25f), x - 4.0f, top,
                9.0f, height);
        fx_rect(r, art->wall_light, x - 3.0f, top, 2.0f, height);
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
    }

    fx_rect(r, fx_mix(art->wall_dark, FX_INK, 0.4f), face_left, top,
            7.0f, height);
    fx_rect(r, fx_mix(art->trim, art->wall_dark, 0.4f), face_right - 7.0f, top,
            7.0f, height);

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

static void backdrop_facade(const LevelArtScene *s, const LevelThemeArt *art,
                            float top, float height)
{
    SDL_Renderer *r = s->renderer;

    switch (art->backdrop)
    {
    case BACKDROP_FACADE_STORM:
    {
        /* Lightning is scheduled off a coarse slice of the clock so the whole
         * sky flashes at once; the wall is repainted wet underneath it. */
        float beat = fmodf(s->time, 7.3f);
        float flash = beat < 0.09f ? 1.0f
                                   : (beat < 0.24f && beat > 0.16f ? 0.6f
                                                                   : 0.0f);
        if (flash > 0.0f)
            fx_rect_a(r, (SDL_Color){186, 206, 236, 255},
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
            fx_rect_a(r, (SDL_Color){44, 48, 58, 255}, 90, x, y,
                      (float)s->win_w + 260.0f, 26.0f + (float)band * 7.0f);
        }
        facade_shell(s, art, top, height);
        /* Rain in two sheets: a fast near one and a slow far one. */
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
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
            SDL_SetRenderDrawColor(r, 176, 198, 222, near_sheet ? 90 : 45);
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
        fx_glow(r, sun_x, sun_y, 300.0f, (SDL_Color){255, 196, 120, 255}, 110);
        fx_glow(r, sun_x, sun_y, 120.0f, (SDL_Color){255, 236, 190, 255}, 150);
        facade_skyline(s, art, 0.0f);
        for (int band = 0; band < 3; ++band)
        {
            float y = (float)s->win_h - 150.0f + (float)band * 34.0f;
            fx_rect_a(r, (SDL_Color){250, 186, 132, 255}, 60, 0.0f, y,
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
            fx_set(r, (SDL_Color){52, 40, 44, 255});
            SDL_RenderLine(r, x - 5.0f, y - flap, x, y);
            SDL_RenderLine(r, x, y, x + 5.0f, y - flap);
        }
        break;
    }
    case BACKDROP_FACADE_HIGH:
    {
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
                (SDL_Color){150, 130, 220, 255}, 70);
        for (int puff = 0; puff < 7; ++puff)
        {
            unsigned h = art_hash(puff * 19, 811);
            float x = fmodf(s->time * (5.0f + (float)(h % 5u)) +
                                (float)(h % 700u),
                            (float)s->win_w + 320.0f) -
                      160.0f;
            float w = 150.0f + (float)(h % 160u);
            fx_rect_a(r, (SDL_Color){58, 56, 104, 255}, 150, x,
                      deck + (float)((h >> 4) % 40u), w, 30.0f);
            fx_rect_a(r, (SDL_Color){96, 92, 156, 255}, 90, x,
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
        facade_shell(s, art, top, height);
        break;
    }
    }

    /* Every climb keeps the same aircraft beacon so the four walls read as
     * the same tower at different hours. */
    float beacon = 0.45f + 0.55f * sinf(s->time * 2.2f);
    fx_glow(r, (float)s->win_w - 42.0f, top + 25.0f, 20.0f, art->accent,
            (Uint8)(35.0f + beacon * 55.0f));
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
