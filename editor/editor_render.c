#include "editor.h"

#include "bbc.h"
#include "bbc_cast.h"
#include "level_art.h"
#include "level_paint.h"

#include <math.h>

/*
 * The canvas, and the marker every legend character is drawn as.
 *
 * Everything here goes through bbc.h, the same layer the game draws with, so
 * the canvas is a MODE 2 picture in the eight colours before it is scaled onto
 * the window: what the zoom does to it afterwards is a nearest-neighbour
 * enlargement of BBC pixels, never a blend of them.
 */

/* ---- The markers -------------------------------------------------------- */

/*
 * One drawing per legend character, eight BBC pixels by sixteen: exactly one
 * tile, and exactly the box every standing figure in bbc_cast.c is drawn in.
 * Where the game has a sprite for the thing — Chuck, a guard, the dog, the man
 * with the mop, every pickup — the marker *is* that sprite, so the floor plan
 * and the floor show the same man. The rest are drawn here on squared paper,
 * in the colours the style guide gives each kind of thing: cyan for
 * technology, yellow for what is worth picking up or watching, red for what
 * costs a heart, green for access, magenta for the story.
 *
 * Masonry is the exception that is not fixed: it is painted from the sector's
 * own theme (`level_theme_paint`), slot 1 the body and slot 2 the joints, so a
 * schematic of the lobby is white stone and a schematic of the plant is red
 * plate, as the art view would be.
 */

/* Stretcher bond, four rows to a course, two courses offset by half a brick:
 * the tile repeats on both axes without a seam. */
BBC_SPRITE(MARK_WALL,
    "11121112",
    "11121112",
    "11121112",
    "22222222",
    "12111211",
    "12111211",
    "12111211",
    "22222222",
    "11121112",
    "11121112",
    "11121112",
    "22222222",
    "12111211",
    "12111211",
    "12111211",
    "22222222");

/* Trunking: a white casing round louvres, black slit and cyan slat in turn.
 * The louvres are the point of a duct — opaque both ways — so they are what
 * the marker is made of. */
BBC_SPRITE(MARK_DUCT,
    "WWWWWWWW",
    "WKKKKKKW",
    "WCCCCCCW",
    "WKKKKKKW",
    "WCCCCCCW",
    "WKKKKKKW",
    "WCCCCCCW",
    "WKKKKKKW",
    "WCCCCCCW",
    "WKKKKKKW",
    "WCCCCCCW",
    "WKKKKKKW",
    "WCCCCCCW",
    "WKKKKKKW",
    "WCCCCCCW",
    "WWWWWWWW");

/* Rungs every four rows, so a run of ladder tiles keeps one rhythm. */
BBC_SPRITE(MARK_LADDER,
    ".Y....Y.",
    ".YYYYYY.",
    ".Y....Y.",
    ".Y....Y.",
    ".Y....Y.",
    ".YYYYYY.",
    ".Y....Y.",
    ".Y....Y.",
    ".Y....Y.",
    ".YYYYYY.",
    ".Y....Y.",
    ".Y....Y.",
    ".Y....Y.",
    ".YYYYYY.",
    ".Y....Y.",
    ".Y....Y.");

/* Two guide rails and the cable twisting between them. */
BBC_SPRITE(MARK_SHAFT,
    "C..W...C",
    "C...W..C",
    "C..W...C",
    "C...W..C",
    "C..W...C",
    "C...W..C",
    "C..W...C",
    "C...W..C",
    "C..W...C",
    "C...W..C",
    "C..W...C",
    "C...W..C",
    "C..W...C",
    "C...W..C",
    "C..W...C",
    "C...W..C");

/* A cracked panel where the foot lands, and which way it goes. */
BBC_SPRITE(MARK_FALL_PANEL,
    "YYYYYYYY",
    "RRRKRRRR",
    "RRKRRRRR",
    "YYYKYYYY",
    "........",
    "........",
    "...RR...",
    "...RR...",
    "...RR...",
    ".RRRRRR.",
    "..RRRR..",
    "...RR...",
    "........",
    "........",
    "........",
    "........");

/* A green deck with an arrowhead at each end: it patrols its row. */
BBC_SPRITE(MARK_PLATFORM,
    "........",
    "........",
    "........",
    "........",
    "........",
    ".W....W.",
    "WGGGGGGW",
    "WGGGGGGW",
    ".W....W.",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........");

/* Every door is this one in a different paint: '1' the frame, '2' the leaf,
 * '3' its mouldings and '4' the handle. The top two rows are left for a sign. */
BBC_SPRITE(MARK_DOOR,
    "........",
    "........",
    "11111111",
    "12222221",
    "12333321",
    "12322321",
    "12322321",
    "12333321",
    "12222221",
    "12222241",
    "12222241",
    "12333321",
    "12322321",
    "12322321",
    "12333321",
    "12222221");

/* The lamp over a door: green for the way out, cyan for the washroom. */
BBC_SPRITE(MARK_DOOR_SIGN,
    "..1111..",
    "..1111..");

/* The return door's arrow: back the way you came. */
BBC_SPRITE(MARK_DOOR_ARROW,
    "..Y.....",
    ".YYYYY..",
    "..Y.....");

static const BbcColour PAINT_EXIT_DOOR[BBC_SPRITE_SLOTS] = {
    BBC_WHITE, BBC_GREEN, BBC_BLACK, BBC_YELLOW};
static const BbcColour PAINT_PAIRED_DOOR[BBC_SPRITE_SLOTS] = {
    BBC_YELLOW, BBC_RED, BBC_BLACK, BBC_WHITE};
static const BbcColour PAINT_WASHROOM_DOOR[BBC_SPRITE_SLOTS] = {
    BBC_WHITE, BBC_BLUE, BBC_CYAN, BBC_YELLOW};
static const BbcColour PAINT_EXIT_SIGN[BBC_SPRITE_SLOTS] = {
    BBC_GREEN, BBC_GREEN, BBC_GREEN, BBC_GREEN};
static const BbcColour PAINT_WASHROOM_SIGN[BBC_SPRITE_SLOTS] = {
    BBC_CYAN, BBC_CYAN, BBC_CYAN, BBC_CYAN};

/* A traversable window: cyan frame and mullions, blue glass, one glint. */
BBC_SPRITE(MARK_WINDOW,
    "CCCCCCCC",
    "CBBCCBBC",
    "CWBCCWBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CCCCCCCC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CBBCCBBC",
    "CCCCCCCC");

/* A dome on the floor with its lamp, slot 1, which flashes. */
BBC_SPRITE(MARK_MINE,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "...11...",
    "..RRRR..",
    ".RRRRRR.",
    ".RRRRRR.",
    "WWWWWWWW");

/* The bed is the bottom half-tile and nothing above it, which is SPIKE_H: a
 * marker taller than the hazard would tell an author the hop is harder than
 * it is. White tips, red teeth. */
BBC_SPRITE(MARK_SPIKES,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "W.W.W.W.",
    "W.W.W.W.",
    "R.R.R.R.",
    "R.R.R.R.",
    "R.R.R.R.",
    "RRRRRRRR",
    "RRRRRRRR",
    "RRRRRRRR");

/* Green, with a valve and the yellow-and-black band that says shoot it. */
BBC_SPRITE(MARK_CANISTER,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "...WW...",
    "..WWWW..",
    ".GGGGGG.",
    ".GGGGGG.",
    ".YKYKYK.",
    ".KYKYKY.",
    ".GGGGGG.",
    ".GGGGGG.",
    ".GGGGGG.",
    "..GGGG..");

/* A braced crate, standing on the floor. */
BBC_SPRITE(MARK_CRATE,
    "........",
    "........",
    "YYYYYYYY",
    "YRRRRRYY",
    "YRRRRYRY",
    "YRRRYRRY",
    "YRRYRRRY",
    "YRYRRRRY",
    "YYYYYYYY",
    "YRRRRRYY",
    "YRRRRYRY",
    "YRRRYRRY",
    "YRRYRRRY",
    "YRYRRRRY",
    "YRRRRRRY",
    "YYYYYYYY");

/* A console on a desk: green type on a black screen, and slot 1 is the cursor,
 * which flashes the way every cursor on the machine did. */
BBC_SPRITE(MARK_TERMINAL,
    "........",
    "........",
    "CCCCCCCC",
    "CKKKKKKC",
    "CKGGGKKC",
    "CKKKKKKC",
    "CKGGKGKC",
    "CKKKKKKC",
    "CK1KKKKC",
    "CCCCCCCC",
    "...CC...",
    "..CCCC..",
    "WWWWWWWW",
    "BBBBBBBB",
    "B......B",
    "B......B");

/* A red box on the wall with a white push. */
BBC_SPRITE(MARK_ALARM,
    "........",
    "........",
    "........",
    "........",
    ".YYYYYY.",
    ".YRRRRY.",
    ".YRWWRY.",
    ".YRWWRY.",
    ".YRRRRY.",
    ".YYYYYY.",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........");

/* Hung from the slab above: the mount at the top row, a cyan body, a white
 * lens, and its lamp (slot 1) flashing red. */
BBC_SPRITE(MARK_CAMERA,
    "..WWWW..",
    "...WW...",
    "...WW...",
    ".C1CCCC.",
    "CCCCCCWW",
    "CCCCCCWW",
    ".CCCCCC.",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........");

/* ---- Props: blue and white in the office, brass and wood out front, white
 * porcelain and cyan partitions in the washroom, yellow and red plant, and
 * magenta for what belongs to tonight. */

BBC_SPRITE(MARK_CHAIR,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "......B.",
    "......B.",
    "......B.",
    ".BBBBBB.",
    "...W....",
    "...W....",
    "...W....",
    ".WWWWW..",
    ".W...W..");

BBC_SPRITE(MARK_DESK,
    "........",
    "........",
    "........",
    "........",
    "........",
    "..WWWW..",
    "..WCCW..",
    "..WCCW..",
    "..WWWW..",
    "....W...",
    "WWWWWWWW",
    "BBBBBBBB",
    "B....BBB",
    "B....BYB",
    "B....BBB",
    "B......B");

BBC_SPRITE(MARK_EQUIPMENT,
    "........",
    "........",
    ".WWWWWW.",
    ".WBBBBW.",
    ".WBYYBW.",
    ".WBBBBW.",
    ".WWWWWW.",
    ".WBBBBW.",
    ".WBYYBW.",
    ".WBBBBW.",
    ".WWWWWW.",
    ".WBBBBW.",
    ".WBYYBW.",
    ".WBBBBW.",
    ".WWWWWW.",
    ".W....W.");

BBC_SPRITE(MARK_COUNTER,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "YYYYYYYY",
    "RRRRRRRR",
    "RRRRRRRR",
    "YYYYYYYY",
    "RRRYRRRY",
    "RRRYRRRY",
    "RRRYRRRY",
    "RRRYRRRY",
    "RRRYRRRY",
    "YYYYYYYY");

BBC_SPRITE(MARK_BENCH,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    ".RRRRRR.",
    ".R....R.",
    ".R....R.",
    "YYYYYYYY",
    "RRRRRRRR",
    ".W....W.",
    ".W....W.",
    ".W....W.",
    "WW....WW");

BBC_SPRITE(MARK_PLANTER,
    "........",
    ".G....G.",
    "GGG..GGG",
    "..GGGG..",
    ".GG.GGG.",
    "G..YY..G",
    "...YY...",
    "...YY...",
    "...YY...",
    "...YY...",
    ".WWWWWW.",
    ".WWWWWW.",
    "..WWWW..",
    "..WWWW..",
    "..WWWW..",
    "..WWWW..");

/* Two posts with a green lamp each and the glass flaps between them. */
BBC_SPRITE(MARK_GATE,
    "........",
    "........",
    "........",
    "GG....GG",
    "WW....WW",
    "WW....WW",
    "WWCCCCWW",
    "WWCCCCWW",
    "WWCCCCWW",
    "WW....WW",
    "WW....WW",
    "WW....WW",
    "WW....WW",
    "WW....WW",
    "WW....WW",
    "WW....WW");

BBC_SPRITE(MARK_TOILET,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    ".WWW....",
    ".WWW....",
    ".WWW....",
    ".WWWWWW.",
    ".WWWWWW.",
    "..WWWW..",
    "...WW...",
    "...WW...",
    "..WWWW..",
    "..WWWW..");

/* The basin and the mirror the game hangs over it. */
BBC_SPRITE(MARK_BASIN,
    "........",
    ".CCCCCC.",
    ".CBBBBC.",
    ".CBWBBC.",
    ".CBBBBC.",
    ".CCCCCC.",
    "........",
    "........",
    "...W....",
    "WWWWWWWW",
    ".WWWWWW.",
    "..WWWW..",
    "...WW...",
    "...WW...",
    "...WW...",
    "..WWWW..");

BBC_SPRITE(MARK_URINAL,
    "...WW...",
    "...WW...",
    "..WWWW..",
    ".WWWWWW.",
    ".WBBBBW.",
    ".WBBBBW.",
    ".WWWWWW.",
    "..WWWW..",
    "...WW...",
    "...WW...",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........");

BBC_SPRITE(MARK_PARTITION,
    "........",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...CC...",
    "...WW...",
    "..WWWW..");

BBC_SPRITE(MARK_STALL_OPEN,
    "CCCCCCCC",
    "C......C",
    "C......C",
    "C......C",
    "C......C",
    "C......C",
    "C......C",
    "C......C",
    "C.WW...C",
    "C.WW...C",
    "C.WWWW.C",
    "C.WWWW.C",
    "C..WW..C",
    "C..WW..C",
    "C.WWWW.C",
    "C......C");

/* Shut, with the red tab that says somebody is in there, and the gap under the
 * door every stall has. */
BBC_SPRITE(MARK_STALL_SHUT,
    "CCCCCCCC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBRBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "C......C",
    "C......C",
    "C......C");

/* Two drums on a pallet. */
BBC_SPRITE(MARK_PALLET,
    "........",
    "........",
    "........",
    "........",
    ".YY..YY.",
    "YYYYYYYY",
    "RRRRRRRR",
    "YYYYYYYY",
    "YYYYYYYY",
    "YYYYYYYY",
    "YYYYYYYY",
    "RRRRRRRR",
    "YYYYYYYY",
    "YYYYYYYY",
    "RRRRRRRR",
    "R..RR..R");

/* Two risers, the run across them and the hand wheel on the valve. */
BBC_SPRITE(MARK_PIPE_RAIL,
    "........",
    "..RRRR..",
    "..R..R..",
    "..RRRR..",
    "...CC...",
    "CCCCCCCC",
    "CCCCCCCC",
    "CC....CC",
    "CC....CC",
    "CC....CC",
    "CC....CC",
    "CC....CC",
    "CC....CC",
    "CC....CC",
    "CC....CC",
    "WW....WW");

BBC_SPRITE(MARK_BOLLARD,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "...YY...",
    "..YYYY..",
    "..WWWW..",
    "..WWWW..",
    "..YYYY..",
    "..YYYY..",
    "..YYYY..",
    "..YYYY..",
    "..YYYY..",
    ".YYYYYY.");

BBC_SPRITE(MARK_FLIGHT_CASE,
    "........",
    "........",
    "........",
    "........",
    "...WW...",
    "WWWWWWWW",
    "WMMMMMMW",
    "WMMYYMMW",
    "WWWWWWWW",
    "WMMMMMMW",
    "WMMMMMMW",
    "WMMMMMMW",
    "WMMMMMMW",
    "WMMMMMMW",
    "WMMMMMMW",
    "WWWWWWWW");

/* A window with somebody hostile leaning out of it, and what he is about to
 * throw in his hand. */
BBC_SPRITE(MARK_THROWER,
    "WWWWWWWW",
    "WKKKKKKW",
    "WKKKKKKW",
    "WKRRKKKW",
    "WKRRKKYW",
    "WKRRRRRW",
    "WRRRRKKW",
    "WRRRRKKW",
    "WRRRRKKW",
    "WRRRRKKW",
    "WKRRKKKW",
    "WKRRKKKW",
    "WKKKKKKW",
    "WKKKKKKW",
    "WKKKKKKW",
    "WWWWWWWW");

/* Integer BBC pixels to a marker pixel: one at a tile's size, more only when
 * somebody asks for a marker bigger than a tile. */
static int marker_scale(float size)
{
    int k = (int)floorf(size / (float)TILE_SIZE);
    return k < 1 ? 1 : k;
}

static void stamp(SDL_Renderer *r, const BbcSprite *sprite, float x, float y,
                  int dir, const BbcColour *slots, int k)
{
    bbc_sprite_scaled(r, sprite, x, y, dir, slots, k);
}

/* A small sprite (a pickup) in the middle of the tile, on whole BBC pixels. */
static void stamp_centred(SDL_Renderer *r, const BbcSprite *sprite, float x,
                          float y, int k, const BbcColour *slots)
{
    int cols = (8 - bbc_sprite_w(sprite)) / 2;
    int rows = (16 - sprite->h) / 2;
    stamp(r, sprite, x + (float)(cols * BBC_PX_W * k),
          y + (float)(rows * BBC_PX_H * k), 1, slots, k);
}

/* A sprite stood on the bottom of the tile, centred across it. */
static void stamp_on_floor(SDL_Renderer *r, const BbcSprite *sprite, float x,
                           float y, int k, int dir, const BbcColour *slots)
{
    int cols = (8 - bbc_sprite_w(sprite)) / 2;
    int rows = 16 - sprite->h;
    stamp(r, sprite, x + (float)(cols * BBC_PX_W * k),
          y + (float)(rows * BBC_PX_H * k), dir, slots, k);
}

static void draw_door(SDL_Renderer *r, float x, float y, int k,
                      const BbcColour *paint, const BbcColour *sign)
{
    stamp(r, &MARK_DOOR, x, y, 1, paint, k);
    if (sign != NULL)
        stamp(r, &MARK_DOOR_SIGN, x, y, 1, sign, k);
}

/* The fan from the side: the rod, the hub, and the blades as a bar whose length
 * is how much of the sweep is broadside to us this frame. Four frames, the way
 * a sprite would have done it, rather than a rotation. */
static void draw_fan(SDL_Renderer *r, float x, float y, int k, float time)
{
    static const int HALF_SPAN[4] = {4, 3, 1, 3};
    float cw = (float)(BBC_PX_W * k);
    float ch = (float)(BBC_PX_H * k);
    int frame = (int)floorf(time * 10.0f) & 3;
    int half = HALF_SPAN[frame];

    bbc_fill(r, BBC_WHITE, x + 3.0f * cw, y, 2.0f * cw, 5.0f * ch);
    bbc_fill(r, BBC_RED, x + (float)(4 - half) * cw, y + 6.0f * ch,
             (float)(half * 2) * cw, ch);
    bbc_fill(r, BBC_WHITE, x + 3.0f * cw, y + 5.0f * ch, 2.0f * cw, 3.0f * ch);
}

/* A reel stood on its edge, with the tail run off along the floor. Round on
 * the glass, which on this grid is twice as many rows as columns. */
static void draw_reel(SDL_Renderer *r, float x, float y, int k)
{
    float u = (float)(BBC_PX_W * k);
    float cx = x + 4.0f * u;
    float cy = y + 18.0f * (float)k;
    bbc_fill(r, BBC_RED, x + 4.0f * u, y + 30.0f * (float)k, 4.0f * u,
             2.0f * (float)k);
    bbc_disc(r, BBC_YELLOW, cx, cy, 12.0f * (float)k);
    bbc_disc(r, BBC_RED, cx, cy, 7.0f * (float)k);
    bbc_disc(r, BBC_BLACK, cx, cy, 2.5f * (float)k);
}

/* White face, red rim, black hands at ten to one — the hour the job is run to
 * — hung from the slab on a short white stem. */
static void draw_clock(SDL_Renderer *r, float x, float y, int k)
{
    float u = (float)k;
    float cx = x + 16.0f * u;
    float cy = y + 14.0f * u;
    bbc_fill(r, BBC_WHITE, cx - 4.0f * u, y, 8.0f * u, 4.0f * u);
    bbc_disc(r, BBC_RED, cx, cy, 11.0f * u);
    bbc_disc(r, BBC_WHITE, cx, cy, 8.0f * u);
    bbc_line(r, BBC_BLACK, cx, cy, cx + 1.0f * u, cy - 7.0f * u);
    bbc_line(r, BBC_BLACK, cx, cy, cx - 6.0f * u, cy - 2.0f * u);
}

/* A character the legend does not have: a red box that will not stop shouting
 * until somebody fixes the map. */
static void draw_unknown(SDL_Renderer *r, char symbol, float x, float y, int k)
{
    float u = (float)k;
    bbc_fill(r, BBC_BLACK, x + 4.0f * u, y + 2.0f * u, 24.0f * u, 28.0f * u);
    bbc_frame(r, bbc_flash(BBC_RED), x + 4.0f * u, y + 2.0f * u, 24.0f * u,
              28.0f * u);
    char text[2] = {symbol >= 32 && symbol < 127 ? symbol : '?', '\0'};
    bbc_text(r, x + 12.0f * u, y + 12.0f * u, 1.0f, BBC_WHITE, text);
}

LevelTheme ed_theme(const EditorApp *app)
{
    /* The parsed map knows the theme a missing THEME line defaults to; the
     * document only knows what the file says. */
    if (app->level_ok)
        return app->level.map.theme;
    if (app->doc.grid.has_theme)
        return app->doc.grid.theme;
    return app->doc.grid.facade ? LEVEL_THEME_FACADE_NIGHT
                                : LEVEL_THEME_RESTROOM;
}

void ed_draw_symbol(SDL_Renderer *r, LevelTheme theme, char symbol, float x,
                    float y, float size, float time)
{
    int k = marker_scale(size);
    const LevelThemePaint *paint = level_theme_paint(theme);
    BbcColour wall[BBC_SPRITE_SLOTS] = {paint->wall, paint->wall_ink,
                                        paint->trim, paint->trim_ink};
    BbcColour lamp[BBC_SPRITE_SLOTS] = {BBC_YELLOW, BBC_YELLOW, BBC_YELLOW,
                                        BBC_YELLOW};

    switch (symbol)
    {
    case ' ':
    case '.':
        break;

    case '#':
        stamp(r, &MARK_WALL, x, y, 1, wall, k);
        break;

    case '%':
    {
        /* The same masonry with the crack run through it in black, which is
         * the one colour that shows on every theme's stone. At palette size
         * the crack is all that tells it from '#'. */
        static const float CRACK[][2] = {
            {16.0f, 0.0f}, {12.0f, 8.0f}, {20.0f, 16.0f}, {12.0f, 24.0f},
            {16.0f, 30.0f}};
        float u = (float)k;
        /* Lit along its edge in the lip's colour, so it is the brightest
         * thing in the tile as well as the darkest. */
        BbcColour lit = paint->trim != paint->wall ? paint->trim
                                                   : paint->wall_ink;
        stamp(r, &MARK_WALL, x, y, 1, wall, k);
        for (int i = 0; i + 1 < (int)(sizeof(CRACK) / sizeof(CRACK[0])); ++i)
        {
            bbc_line(r, lit, x + (CRACK[i][0] + 4.0f) * u, y + CRACK[i][1] * u,
                     x + (CRACK[i + 1][0] + 4.0f) * u, y + CRACK[i + 1][1] * u);
        }
        for (int i = 0; i + 1 < (int)(sizeof(CRACK) / sizeof(CRACK[0])); ++i)
        {
            bbc_line(r, BBC_BLACK, x + CRACK[i][0] * u, y + CRACK[i][1] * u,
                     x + CRACK[i + 1][0] * u, y + CRACK[i + 1][1] * u);
        }
        bbc_line(r, BBC_BLACK, x + 20.0f * u, y + 16.0f * u, x + 28.0f * u,
                 y + 20.0f * u);
        break;
    }

    case '=':
        stamp(r, &MARK_DUCT, x, y, 1, NULL, k);
        break;
    case 'H':
        stamp(r, &MARK_LADDER, x, y, 1, NULL, k);
        break;
    case 'V':
        stamp(r, &MARK_SHAFT, x, y, 1, NULL, k);
        break;
    case 'F':
        stamp(r, &MARK_FALL_PANEL, x, y, 1, NULL, k);
        break;
    case 'P':
        stamp(r, &MARK_PLATFORM, x, y, 1, NULL, k);
        break;

    case 'S':
        stamp(r, &SPR_CHUCK_STAND, x, y, 1, NULL, k);
        break;

    case 'E':
        draw_door(r, x, y, k, PAINT_EXIT_DOOR, PAINT_EXIT_SIGN);
        break;
    case 'D':
        draw_door(r, x, y, k, PAINT_PAIRED_DOOR, NULL);
        break;
    case 'U':
        draw_door(r, x, y, k, PAINT_WASHROOM_DOOR, PAINT_WASHROOM_SIGN);
        break;
    case 'R':
        draw_door(r, x, y, k, PAINT_WASHROOM_DOOR, NULL);
        stamp(r, &MARK_DOOR_ARROW, x + (float)(BBC_PX_W * k),
              y + (float)(16 * k), 1, NULL, k);
        break;

    case 'Y':
        stamp(r, &MARK_WINDOW, x, y, 1, NULL, k);
        break;

    case 'C':
        stamp_centred(r, &SPR_ICON_CARD, x, y, k, NULL);
        break;
    case 'G':
        stamp_centred(r, &SPR_ICON_PISTOL, x, y, k, NULL);
        break;
    case 'N':
        stamp_centred(r, &SPR_ICON_GRENADE, x, y, k, NULL);
        break;
    case 'K':
        stamp_centred(r, &SPR_ICON_MEDKIT, x, y, k, NULL);
        break;
    case 'Z':
        stamp_centred(r, &SPR_ICON_BAZOOKA, x, y, k, NULL);
        break;
    case '!':
        stamp_centred(r, &SPR_ICON_FLASH, x, y, k, NULL);
        break;
    case '*':
        stamp_centred(r, &SPR_ICON_EVIDENCE, x, y, k, NULL);
        break;

    /* The cast faces left on the plan, toward a player who mostly arrives
     * from the left, so a guard and Chuck never read as the same way round. */
    case 'M':
        stamp(r, &SPR_GUARD_STAND, x, y, -1, BBC_GUARD_PAINT, k);
        break;
    case 'W':
        /* The handler a pixel back and his dog in front of him: one tile, two
         * figures, and the animal is the one that arrives first. */
        stamp(r, &SPR_GUARD_STAND, x + (float)(BBC_PX_W * k), y, -1,
              BBC_GUARD_PAINT, k);
        stamp_on_floor(r, &SPR_DOG_STAND, x, y, k, -1, BBC_DOG_PAINT);
        break;
    case 'Q':
        stamp(r, &SPR_GUARD_STAND, x, y, -1, BBC_HEAVY_PAINT, k);
        stamp(r, &SPR_HEAVY_PLATE, x, y + (float)(HEAVY_PLATE_ROW * BBC_PX_H * k),
              -1, BBC_HEAVY_PAINT, k);
        break;

    case 'J':
        stamp(r, &SPR_JANITOR_WALK1, x, y, 1, NULL, k);
        break;
    case 'f':
        stamp_on_floor(r, &SPR_CIVILIAN_STAND, x, y, k, 1,
                       BBC_CIVILIAN_PAINT[0]);
        break;
    case 'k':
    {
        /* Waist up, as the game draws her, with a desk under her so the
         * marker does not read as half a woman. */
        float u = (float)k;
        stamp(r, &SPR_RECEPTIONIST_IDLE, x, y + 6.0f * u, 1,
              BBC_CIVILIAN_PAINT[1], k);
        bbc_fill(r, BBC_YELLOW, x, y + 24.0f * u, 32.0f * u, 2.0f * u);
        bbc_fill(r, BBC_RED, x, y + 26.0f * u, 32.0f * u, 6.0f * u);
        break;
    }

    case 'X':
        lamp[0] = bbc_flash(BBC_YELLOW);
        stamp(r, &MARK_MINE, x, y, 1, lamp, k);
        break;
    case '^':
        stamp(r, &MARK_SPIKES, x, y, 1, NULL, k);
        break;
    case 'O':
        draw_fan(r, x, y, k, time);
        break;
    case 'L':
        stamp(r, &MARK_CANISTER, x, y, 1, NULL, k);
        break;
    case 'B':
        stamp(r, &MARK_CRATE, x, y, 1, NULL, k);
        break;

    case 'T':
        lamp[0] = bbc_flash_pair(BBC_GREEN, BBC_BLACK);
        stamp(r, &MARK_TERMINAL, x, y, 1, lamp, k);
        break;
    case 'A':
        stamp(r, &MARK_ALARM, x, y, 1, NULL, k);
        break;
    case 'I':
        lamp[0] = bbc_flash_pair(BBC_RED, BBC_CYAN);
        stamp(r, &MARK_CAMERA, x, y, 1, lamp, k);
        break;

    case 'c':
        stamp(r, &MARK_CHAIR, x, y, 1, NULL, k);
        break;
    case 'd':
        stamp(r, &MARK_DESK, x, y, 1, NULL, k);
        break;
    case 'i':
        stamp(r, &MARK_EQUIPMENT, x, y, 1, NULL, k);
        break;

    case 'n':
        stamp(r, &MARK_COUNTER, x, y, 1, NULL, k);
        break;
    case 's':
        stamp(r, &MARK_BENCH, x, y, 1, NULL, k);
        break;
    case 't':
        stamp(r, &MARK_PLANTER, x, y, 1, NULL, k);
        break;
    case 'g':
        stamp(r, &MARK_GATE, x, y, 1, NULL, k);
        break;

    case 'q':
        stamp(r, &MARK_TOILET, x, y, 1, NULL, k);
        break;
    case 'b':
        stamp(r, &MARK_BASIN, x, y, 1, NULL, k);
        break;
    case 'u':
        stamp(r, &MARK_URINAL, x, y, 1, NULL, k);
        break;
    case 'p':
        stamp(r, &MARK_PARTITION, x, y, 1, NULL, k);
        break;
    case 'o':
        stamp(r, &MARK_STALL_OPEN, x, y, 1, NULL, k);
        break;
    case 'z':
        stamp(r, &MARK_STALL_SHUT, x, y, 1, NULL, k);
        break;

    case 'a':
        stamp(r, &MARK_PALLET, x, y, 1, NULL, k);
        break;
    case 'e':
        draw_reel(r, x, y, k);
        break;
    case 'j':
        stamp(r, &MARK_PIPE_RAIL, x, y, 1, NULL, k);
        break;
    case 'l':
        stamp(r, &MARK_BOLLARD, x, y, 1, NULL, k);
        break;

    case 'm':
        stamp(r, &MARK_FLIGHT_CASE, x, y, 1, NULL, k);
        break;
    case 'w':
        draw_clock(r, x, y, k);
        break;

    case 'r':
        stamp(r, &MARK_THROWER, x, y, 1, NULL, k);
        break;
    case 'v':
        stamp_centred(r, &SPR_BIRD_UP, x, y, k, NULL);
        break;

    default:
        draw_unknown(r, symbol, x, y, k);
        break;
    }
}

/* ---- The canvas -------------------------------------------------------- */

static bool ensure_texture(EditorApp *app, int w, int h)
{
    if (app->canvas_texture != NULL && app->canvas_texture_w == w &&
        app->canvas_texture_h == h)
    {
        return true;
    }
    if (app->canvas_texture != NULL)
        SDL_DestroyTexture(app->canvas_texture);
    app->canvas_texture =
        SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_TARGET, w, h);
    if (app->canvas_texture == NULL)
        return false;
    SDL_SetTextureScaleMode(app->canvas_texture, SDL_SCALEMODE_NEAREST);
    app->canvas_texture_w = w;
    app->canvas_texture_h = h;
    return true;
}

static int round_up(int value, int step)
{
    return (value + step - 1) / step * step;
}

/* A line of every other BBC pixel: a pairing, not a wall, so it must not hide
 * what it crosses. Stepped along whichever axis has more cells, so a steep
 * line and a shallow one are dotted at the same rhythm. */
static void dotted_line(SDL_Renderer *r, BbcColour c, float x0, float y0,
                        float x1, float y1)
{
    float dx = (x1 - x0) / (float)BBC_PX_W;
    float dy = (y1 - y0) / (float)BBC_PX_H;
    int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (steps < 1)
        steps = 1;
    if (steps > 4096)
        steps = 4096;
    for (int i = 0; i <= steps; i += 2)
    {
        float t = (float)i / (float)steps;
        bbc_plot(r, c, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
    }
}

static void draw_route_overlay(EditorApp *app, float ox, float oy)
{
    const EdReport *report = &app->report;
    if (!report->route_valid)
        return;

    /* Green dotted over where the route model can stand, red checkered where
     * it can stand and never leave: a wash done the only way the machine
     * could, by laying ink on some pixels and not others. */
    for (int row = 0; row < app->doc.grid.height; ++row)
    {
        for (int col = 0; col < app->doc.grid.width; ++col)
        {
            if (!report->route.seen[row][col])
                continue;
            float x = ox + (float)col * TILE_SIZE;
            float y = oy + (float)row * TILE_SIZE;
            bool stranded = !report->route.escapes[row][col];
            bbc_stipple(app->renderer, stranded ? BBC_RED : BBC_GREEN,
                        stranded ? BBC_DITHER_CHECK : BBC_DITHER_SPARSE, x, y,
                        TILE_SIZE, TILE_SIZE);
        }
    }

    float sx = ox + (float)report->start.col * TILE_SIZE;
    float sy = oy + (float)report->start.row * TILE_SIZE;
    bbc_frame(app->renderer, BBC_CYAN, sx, sy, TILE_SIZE, TILE_SIZE);
    float gx = ox + (float)report->goal.col * TILE_SIZE;
    float gy = oy + (float)report->goal.row * TILE_SIZE;
    /* A goal the model cannot reach is the one thing on this overlay that has
     * to shout, so it flashes. */
    bbc_frame(app->renderer,
              report->goal_reached ? BBC_GREEN : bbc_flash(BBC_RED), gx, gy,
              TILE_SIZE, TILE_SIZE);
}

static void draw_door_pairs(EditorApp *app, float ox, float oy)
{
    const LevelMap *map = &app->level.map;
    for (int i = 0; i + 1 < map->door_count; i += 2)
    {
        float ax = ox + (map->doors[i].col + 0.5f) * TILE_SIZE;
        float ay = oy + (map->doors[i].row + 0.5f) * TILE_SIZE;
        float bx = ox + (map->doors[i + 1].col + 0.5f) * TILE_SIZE;
        float by = oy + (map->doors[i + 1].row + 0.5f) * TILE_SIZE;
        dotted_line(app->renderer, BBC_YELLOW, ax, ay, bx, by);
    }
}

static void draw_preview_shape(EditorApp *app, float ox, float oy)
{
    if (!app->dragging)
        return;

    int c0 = SDL_min(app->drag_col, app->cursor_col);
    int c1 = SDL_max(app->drag_col, app->cursor_col);
    int r0 = SDL_min(app->drag_row, app->cursor_row);
    int r1 = SDL_max(app->drag_row, app->cursor_row);
    float x = ox + (float)c0 * TILE_SIZE;
    float y = oy + (float)r0 * TILE_SIZE;
    float w = (float)(c1 - c0 + 1) * TILE_SIZE;
    float h = (float)(r1 - r0 + 1) * TILE_SIZE;
    /* Provisional until the button comes up, so it flashes. */
    bbc_frame(app->renderer, bbc_flash_pair(BBC_CYAN, BBC_WHITE), x, y, w, h);
}

/* Dots at every tile corner and a dashed line every fourth tile: a sheet of
 * squared paper, light enough to read the floor plan through. */
static void draw_grid(EditorApp *app, float ox, float oy, int first_col,
                      int last_col, int first_row, int last_row)
{
    SDL_Renderer *r = app->renderer;
    float map_w = (float)app->doc.grid.width * TILE_SIZE;
    float map_h = (float)app->doc.grid.height * TILE_SIZE;
    int col_end = SDL_min(last_col + 1, app->doc.grid.width + 1);
    int row_end = SDL_min(last_row + 1, app->doc.grid.height + 1);
    float top = oy + (float)first_row * TILE_SIZE;
    float bottom = SDL_min(oy + (float)row_end * TILE_SIZE, oy + map_h);
    float left = ox + (float)first_col * TILE_SIZE;
    float right = SDL_min(ox + (float)col_end * TILE_SIZE, ox + map_w);

    for (int col = first_col; col < col_end; ++col)
    {
        if (col % 4 != 0)
            continue;
        float x = ox + (float)col * TILE_SIZE;
        for (float y = top; y < bottom; y += 2.0f * BBC_PX_H)
            bbc_plot(r, BBC_BLUE, x, y);
    }
    for (int row = first_row; row < row_end; ++row)
    {
        if (row % 4 != 0)
            continue;
        float y = oy + (float)row * TILE_SIZE;
        for (float x = left; x < right; x += 2.0f * BBC_PX_W)
            bbc_plot(r, BBC_BLUE, x, y);
    }
    for (int row = first_row; row < row_end; ++row)
    {
        for (int col = first_col; col < col_end; ++col)
        {
            bbc_plot(r, BBC_CYAN, ox + (float)col * TILE_SIZE,
                     oy + (float)row * TILE_SIZE);
        }
    }
}

/* Black over every part of the texture that is not the map. The backdrop is
 * drawn across the whole view, and what is beyond the map's edge is no part
 * of the floor plan. Clipped to the texture, because a rectangle four
 * thousand pixels wide is a rectangle a stipple would walk cell by cell. */
static void blank_outside(SDL_Renderer *r, float ox, float oy, float map_w,
                          float map_h, float tex_w, float tex_h)
{
    float left = SDL_max(0.0f, SDL_min(ox, tex_w));
    float right = SDL_max(0.0f, SDL_min(ox + map_w, tex_w));
    float top = SDL_max(0.0f, SDL_min(oy, tex_h));
    float bottom = SDL_max(0.0f, SDL_min(oy + map_h, tex_h));
    if (left > 0.0f)
        bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, left, tex_h);
    if (right < tex_w)
        bbc_fill(r, BBC_BLACK, right, 0.0f, tex_w - right, tex_h);
    if (top > 0.0f)
        bbc_fill(r, BBC_BLACK, left, 0.0f, right - left, top);
    if (bottom < tex_h)
        bbc_fill(r, BBC_BLACK, left, bottom, right - left, tex_h - bottom);
}

/*
 * The void round the map, drawn on the window rather than in the texture: it
 * is the editor's desk rather than part of the picture, so its dots stay one
 * BBC pixel on the glass at every zoom instead of swelling and aliasing with
 * the map — and it costs the same at 20% as at 400%. The pattern is anchored to
 * the map's corner, so the desk moves with the sheet when it is panned rather
 * than boiling under it.
 */
static void draw_desk(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    const SDL_FRect *c = &app->canvas;
    float x0 = c->x + (0.0f - app->cam_x) * app->zoom;
    float y0 = c->y + (0.0f - app->cam_y) * app->zoom;
    float x1 = c->x +
               ((float)app->doc.grid.width * TILE_SIZE - app->cam_x) * app->zoom;
    float y1 = c->y +
               ((float)app->doc.grid.height * TILE_SIZE - app->cam_y) *
                   app->zoom;

    SDL_Rect clip = {(int)c->x, (int)c->y, (int)c->w, (int)c->h};
    SDL_SetRenderClipRect(r, &clip);
    bbc_set_pattern_origin(x0, y0);

    float cl = c->x, cr = c->x + c->w, ct = c->y, cb = c->y + c->h;
    float il = SDL_max(cl, SDL_min(x0, cr));
    float ir = SDL_max(cl, SDL_min(x1, cr));
    float it = SDL_max(ct, SDL_min(y0, cb));
    float ib = SDL_max(ct, SDL_min(y1, cb));
    if (il > cl)
        bbc_stipple(r, BBC_BLUE, BBC_DITHER_DOTS, cl, ct, il - cl, cb - ct);
    if (ir < cr)
        bbc_stipple(r, BBC_BLUE, BBC_DITHER_DOTS, ir, ct, cr - ir, cb - ct);
    if (it > ct)
        bbc_stipple(r, BBC_BLUE, BBC_DITHER_DOTS, il, ct, ir - il, it - ct);
    if (ib < cb)
        bbc_stipple(r, BBC_BLUE, BBC_DITHER_DOTS, il, ib, ir - il, cb - ib);

    /* The sheet's edge, one BBC pixel outside it, so the last row and column
     * of tiles are never under it. */
    float fl = bbc_snap_x(x0) - (float)BBC_PX_W;
    float ft = bbc_snap_y(y0) - (float)BBC_PX_H;
    float fr = bbc_snap_x(x1 + (float)BBC_PX_W - 0.01f) + (float)BBC_PX_W;
    float fb = bbc_snap_y(y1 + (float)BBC_PX_H - 0.01f) + (float)BBC_PX_H;
    bbc_frame(r, BBC_WHITE, fl, ft, fr - fl, fb - ft);

    bbc_set_pattern_origin(0.0f, 0.0f);
    SDL_SetRenderClipRect(r, NULL);
}

void ed_draw_canvas(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    int view_w = (int)ceilf(app->canvas.w / app->zoom);
    int view_h = (int)ceilf(app->canvas.h / app->zoom);
    if (view_w < 16)
        view_w = 16;
    if (view_h < 16)
        view_h = 16;

    /*
     * The camera is snapped to the BBC grid before anything is drawn, so a
     * tile, the marker on it and the dither over it all land on whole cells;
     * the part of a cell the camera is really past is taken off when the
     * texture is copied out, so panning is still as smooth as the mouse. The
     * texture carries one spare cell each way for that, and is a whole number
     * of cells in both directions so a fill can cover all of it.
     */
    float cam_x = bbc_snap_x(app->cam_x);
    float cam_y = bbc_snap_y(app->cam_y);
    float frac_x = app->cam_x - cam_x;
    float frac_y = app->cam_y - cam_y;
    int tex_w = round_up(view_w + BBC_PX_W, BBC_PX_W);
    int tex_h = round_up(view_h + HUD_HEIGHT + BBC_PX_H, BBC_PX_H);
    if (!ensure_texture(app, tex_w, tex_h))
        return;

    SDL_SetRenderTarget(r, app->canvas_texture);
    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, (float)tex_w, (float)tex_h);

    /* The game draws the world under a 40px HUD, and the backdrop's parallax
     * is composed around that. Reproducing the offset and cropping it off
     * again is what keeps the editor's picture the game's picture. */
    float ox = -cam_x;
    float oy = HUD_HEIGHT - cam_y;
    float map_w = (float)app->doc.grid.width * TILE_SIZE;
    float map_h = (float)app->doc.grid.height * TILE_SIZE;
    LevelTheme theme = ed_theme(app);
    bool art = app->art_mode && app->level_ok;

    if (art)
    {
        LevelArtScene scene = {
            .renderer = r,
            .level = &app->level,
            .cam_x = cam_x,
            .cam_y = cam_y,
            .win_w = tex_w,
            .win_h = tex_h,
            .time = app->time,
            .level_index = app->current_file >= 0
                               ? app->files[app->current_file].number - 1
                               : 0,
            /* The editor draws the backdrop as the game ships it, so the
             * strobes stay strobing here: reduced motion is a player's setting
             * and this canvas is an author's. */
            .steady_lights = false,
        };
        level_art_backdrop(&scene);
        /* Whatever the backdrop did to the palette or the pattern origin is
         * its own business and must not reach the markers. */
        bbc_palette_reset();
        blank_outside(r, ox, oy, map_w, map_h, (float)tex_w, (float)tex_h);
    }
    bbc_set_pattern_origin(ox, oy);

    int first_col = (int)floorf(cam_x / TILE_SIZE) - 1;
    int last_col = (int)ceilf((cam_x + (float)tex_w) / TILE_SIZE) + 1;
    int first_row = (int)floorf((cam_y - HUD_HEIGHT) / TILE_SIZE) - 1;
    int last_row = (int)ceilf((cam_y + (float)tex_h) / TILE_SIZE) + 1;
    if (first_col < 0)
        first_col = 0;
    if (first_row < 0)
        first_row = 0;
    if (last_col > app->doc.grid.width)
        last_col = app->doc.grid.width;
    if (last_row > app->doc.grid.height)
        last_row = app->doc.grid.height;

    for (int row = first_row; row < last_row; ++row)
    {
        for (int col = first_col; col < last_col; ++col)
        {
            char cell = editor_doc_get(&app->doc, col, row);
            float x = ox + (float)col * TILE_SIZE;
            float y = oy + (float)row * TILE_SIZE;
            if ((cell == '#' || cell == '%') && art)
            {
                level_art_wall_tile(r, &app->level, col, row, x, y);
                continue;
            }
            /* Padding is air written as a dot, and the schematic says so
             * literally: one blue BBC pixel in the middle of the tile, which
             * is quieter than the desk's scatter round the sheet and cannot be
             * mistaken for it. */
            if (cell == '.' && !art)
            {
                bbc_plot(r, BBC_BLUE, x + 3.0f * BBC_PX_W, y + 7.0f * BBC_PX_H);
                continue;
            }
            ed_draw_symbol(r, theme, cell, x, y, TILE_SIZE, app->time);
        }
    }

    if (app->doc.grid.facade)
    {
        /* The outer 80px on each side is behind the inset: hatched in red over
         * whatever is drawn there, because nothing there can be reached. */
        bbc_stipple(r, BBC_RED, BBC_DITHER_SPARSE, ox, oy,
                    FACADE_BUILDING_SIDE_INSET, map_h);
        bbc_stipple(r, BBC_RED, BBC_DITHER_SPARSE,
                    ox + map_w - FACADE_BUILDING_SIDE_INSET, oy,
                    FACADE_BUILDING_SIDE_INSET, map_h);
    }

    if (app->show_grid)
        draw_grid(app, ox, oy, first_col, last_col, first_row, last_row);

    if (app->show_route)
        draw_route_overlay(app, ox, oy);
    if (app->level_ok)
        draw_door_pairs(app, ox, oy);

    if (app->has_selection)
    {
        float x = ox + (float)SDL_min(app->sel_col0, app->sel_col1) * TILE_SIZE;
        float y = oy + (float)SDL_min(app->sel_row0, app->sel_row1) * TILE_SIZE;
        float w = (float)(SDL_abs(app->sel_col1 - app->sel_col0) + 1) * TILE_SIZE;
        float h = (float)(SDL_abs(app->sel_row1 - app->sel_row0) + 1) * TILE_SIZE;
        /* A sparse cyan over what is selected and a doubled frame round it:
         * the selection is held, not provisional, so it does not flash. */
        bbc_stipple(r, BBC_CYAN, BBC_DITHER_SPARSE, x, y, w, h);
        bbc_frame(r, BBC_CYAN, x, y, w, h);
        bbc_frame(r, BBC_BLUE, x + (float)BBC_PX_W, y + (float)BBC_PX_H,
                  w - 2.0f * BBC_PX_W, h - 2.0f * BBC_PX_H);
    }

    draw_preview_shape(app, ox, oy);

    /*
     * The tile under the pointer. The brush blinks into it in the flash's
     * first half and the tile as it is shows in the second, which is the
     * nearest thing to a ghost a machine with no transparency has — and is
     * better than one, since both are seen whole. The frame round it is the
     * cursor, and flashes white and black as the machine's did.
     */
    if (app->cursor_col >= 0 && app->cursor_row >= 0)
    {
        float x = ox + (float)app->cursor_col * TILE_SIZE;
        float y = oy + (float)app->cursor_row * TILE_SIZE;
        if (app->tool != ED_TOOL_SELECT && app->tool != ED_TOOL_PICK &&
            bbc_flash_on())
        {
            bbc_fill(r, BBC_BLACK, x, y, TILE_SIZE, TILE_SIZE);
            ed_draw_symbol(r, theme, app->brush, x, y, TILE_SIZE, app->time);
        }
        bbc_frame(r, bbc_flash(BBC_WHITE), x, y, TILE_SIZE, TILE_SIZE);
    }

    bbc_set_pattern_origin(0.0f, 0.0f);
    SDL_SetRenderTarget(r, NULL);
    SDL_FRect src = {frac_x, (float)HUD_HEIGHT + frac_y, (float)view_w,
                     (float)view_h};
    SDL_RenderTexture(r, app->canvas_texture, &src, &app->canvas);

    draw_desk(app);
}
