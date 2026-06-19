#include "chase_render.h"

#include "bbc.h"

#include <math.h>

/*
 * The pursuit, drawn as a BBC Micro top-down driving game would have drawn it
 * in MODE 2: a black road between blue blocks, white and yellow paint, cars
 * that are clean sprites in saturated paint, and a dashboard across the top.
 * Nothing here reads or writes simulation state; every animation is derived
 * from `Chase`.
 *
 * Everything goes through [bbc.h](bbc.h), so everything lands on the 4x2 grid
 * in one of the eight colours. What the old renderer did with light this one
 * does with the machine's three substitutes for it: a headlamp is a wedge of
 * dots on the asphalt, a street lamp is a pool of yellow dither, the cordon's
 * bars are the hardware's flashing colours, and smoke is grey — which is black
 * and white on a checkerboard, because there is no grey.
 *
 * **The road is world-locked, the cars are not.** The camera is snapped to the
 * grid before a pixel is drawn and every pattern is anchored to the same
 * snapped origin (`chase_view_make`), so the paint, the puddles and the dither
 * under a lamp all travel down the screen together, a whole BBC pixel at a
 * time, while each car — which has a float position of its own — snaps on its
 * own. A car creeping a pixel against the paint is what a BBC game looked
 * like; paint creeping against paint is the road boiling.
 */

/* ---- The view -------------------------------------------------------- */

/* Road space is drawn 1:1, so the visible strip of road is one view height. */
typedef struct
{
    float view_top;
    float view_h;
    float camera_y;
    /* Where road (0, 0) lands on the glass, snapped to the grid. Every world
     * position goes through these two numbers and nothing else, which is what
     * keeps the street rigid while it scrolls. The shake is folded in here
     * rather than added later, so a hit moves the whole street by whole
     * pixels together. */
    float origin_x;
    float origin_y;
    /* The player's reduced-motion switch. The flashing colours already hold
     * still under it (`bbc_begin_frame` is told); this is for the few things
     * here that move on a clock of their own — the glints in the puddles and
     * under the lamps — which hold still with them. */
    bool steady_lights;
} ChaseView;

/*
 * The street has two layers with the traffic between them. What lies on the
 * road — a lamp's pool, a signal's colour on the wet — goes down before the
 * cars and is driven over; what stands on a mast — the lamp heads, the signal
 * heads — goes down after them, because every car drives *under* it. Drawn in
 * one pass before the cars, the lamp arms reached across the kerb lane under
 * whatever was in it, which said the lamps were painted on the tarmac.
 */
typedef enum
{
    STREET_GROUND,
    STREET_OVERHEAD
} StreetLayer;

static ChaseView chase_view_make(int win_w, int win_h, float camera_y,
                                 float shake_x, float shake_y, bool steady)
{
    ChaseView view;
    float road_left = ((float)win_w - CHASE_ROAD_WIDTH) * 0.5f;
    view.view_top = (float)HUD_HEIGHT;
    view.view_h = (float)win_h - (float)HUD_HEIGHT;
    view.camera_y = camera_y;
    view.origin_x = bbc_snap_x(road_left + shake_x);
    view.origin_y = bbc_snap_y(view.view_top + view.view_h + camera_y + shake_y);
    view.steady_lights = steady;
    return view;
}

static float screen_x(const ChaseView *view, float x)
{
    return view->origin_x + x;
}

static float screen_y(const ChaseView *view, float y)
{
    return view->origin_y - y;
}

/* The road's left and right edges on the glass, shake included. */
static float road_left_x(const ChaseView *view)
{
    return view->origin_x;
}

static float road_right_x(const ChaseView *view)
{
    return view->origin_x + CHASE_ROAD_WIDTH;
}

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

/* ---- Hashing --------------------------------------------------------- */

/*
 * Every scattered thing on this road — a crack, a puddle, a fitting on a roof
 * — is keyed to a world index and hashed, so it travels with the street and
 * never re-rolls as the street scrolls.
 */
static unsigned hash_u(unsigned value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

/* A float index turned into a hash salt. A negative float cast straight to
 * `unsigned` is undefined, so it goes through `int`, and anything out of an
 * int's range answers nought rather than trapping. */
static unsigned salt(float value)
{
    if (!(value > -2.14e9f && value < 2.14e9f))
        return 0u;
    return (unsigned)(int)value;
}

/* `h` scattered over `span` whole units; nought for a span under one. */
static float spread(unsigned h, float span)
{
    if (!(span >= 1.0f) || !(span < 2.14e9f))
        return 0.0f;
    return (float)(h % (unsigned)span);
}

/* ---- Palette fades ----------------------------------------------------- */

/*
 * A caption fading in, done the way a BBC fade was done: every colour stepped
 * down the palette ladder (`bbc_palette_dim`), never blended. The palette is
 * resolved as each thing is drawn, so dimming it around one plate fades that
 * plate and nothing drawn before or after it.
 */
typedef struct
{
    BbcColour map[BBC_COLOUR_COUNT];
} PaletteHold;

/* How many rungs down the ladder `amount` (0 to 1) of a fade is. Three is
 * black, and a plate faded all the way is a plate not drawn: a black box
 * standing over the road with nothing on it would be a hole in the street. */
static int fade_steps(float amount)
{
    if (amount >= 0.95f)
        return 0;
    if (amount >= 0.6f)
        return 1;
    if (amount >= 0.25f)
        return 2;
    return 3;
}

static PaletteHold fade_begin(float amount)
{
    PaletteHold hold;
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        hold.map[i] = bbc_map((BbcColour)i);
    bbc_palette_dim(fade_steps(amount));
    return hold;
}

static void fade_end(const PaletteHold *hold)
{
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        bbc_palette_set((BbcColour)i, hold->map[i]);
}

/* ---- Shapes the grid does not have ------------------------------------- */

/*
 * An ellipse of pattern, a row of grid at a time: `rx` across and `ry` down in
 * logical pixels, so a circle on the glass is `rx == ry`. With `opaque` the
 * rows are `bbc_dither`ed — the base laid down and the ink over it, a material
 * such as smoke or a wooden tank lid — and without it they are only stippled,
 * which is light falling on what is already there.
 */
static void pattern_ellipse_between(SDL_Renderer *r, BbcColour base,
                                    BbcColour ink, BbcDither pattern, float cx,
                                    float cy, float rx, float ry, bool opaque,
                                    float x_lo, float x_hi)
{
    if (!(rx > 0.0f) || !(ry > 0.0f) || rx > 4096.0f || ry > 4096.0f)
        return;
    float top = bbc_snap_y(cy - ry);
    for (float y = top; y <= cy + ry; y += (float)BBC_PX_H)
    {
        float dy = (y + (float)BBC_PX_H * 0.5f - cy) / ry;
        if (dy * dy > 1.0f)
            continue;
        float half = rx * sqrtf(1.0f - dy * dy);
        float x0 = bbc_snap_x(fmaxf(cx - half + (float)BBC_PX_W * 0.5f, x_lo));
        float x1 = bbc_snap_x(fminf(cx + half + (float)BBC_PX_W * 0.5f, x_hi));
        if (x1 <= x0)
            continue;
        if (opaque)
            bbc_dither(r, base, ink, pattern, x0, y, x1 - x0, (float)BBC_PX_H);
        else
            bbc_stipple(r, ink, pattern, x0, y, x1 - x0, (float)BBC_PX_H);
    }
}

static void pattern_ellipse(SDL_Renderer *r, BbcColour base, BbcColour ink,
                            BbcDither pattern, float cx, float cy, float rx,
                            float ry, bool opaque)
{
    pattern_ellipse_between(r, base, ink, pattern, cx, cy, rx, ry, opaque,
                            -4096.0f, 4096.0f);
}

/* ---- Text ------------------------------------------------------------ */

static float text_width(const char *text, float scale)
{
    return bbc_text_width(text, scale);
}

/*
 * The way out of the drive, offered twice on the same screen.
 *
 * Both prompts are one function because they had drifted into two spellings —
 * see `PAD_CONFIRM_KEYS` in [pad_hint.h](pad_hint.h) — and `offer` is the only
 * thing that differs between them, which is what is being skipped rather than
 * how.
 *
 * Right-aligned rather than at a fixed x, and that is not tidying: the two used
 * to sit at `win_w - 180` and `win_w - 196`, two magic numbers each measured by
 * hand against the string that happened to be there. Naming both keys makes the
 * longer of the two 31 characters, which is 248px and straight off the edge of
 * the 196. Measured off the drawn line there is nothing left to keep in step —
 * and with the shorter of the two the arithmetic lands on exactly the old
 * position, `168 + 12 == 180`, which is what says the margin below is the one
 * that was always there.
 *
 * It blinks between white and cyan at the flash rate: a prompt that says it is
 * waiting, which is what the machine's own cursor did.
 */
#define CHASE_PROMPT_MARGIN 12.0f

static void draw_skip_prompt(SDL_Renderer *r, int win_w, int win_h,
                             const PadHints *pad, const char *offer)
{
    char pad_form[64];
    char key_form[64];
    char spelled[64];
    SDL_snprintf(pad_form, sizeof(pad_form), "%s%s", PAD_CONFIRM_DRIVE, offer);
    SDL_snprintf(key_form, sizeof(key_form), "%s%s", PAD_CONFIRM_KEYS, offer);
    const char *line =
        pad_hint(pad, spelled, sizeof(spelled), pad_form, key_form);
    float x = (float)win_w - text_width(line, 1.0f) - CHASE_PROMPT_MARGIN;
    float y = (float)win_h - 31.0f;
    /* A black bar under the line, so it reads over whatever paint is under
     * it: the road scrolls behind this corner for the whole drive. */
    bbc_fill(r, BBC_BLACK, x - 4.0f, y - 2.0f, text_width(line, 1.0f) + 8.0f,
             12.0f);
    bbc_text(r, x, y, 1.0f, bbc_flash_pair(BBC_WHITE, BBC_CYAN), line);
}

/* A plate a caption sits on: black, with a one-pixel border in `edge`. */
static void draw_plate(SDL_Renderer *r, float x, float y, float w, float h,
                       BbcColour edge)
{
    bbc_fill(r, BBC_BLACK, x, y, w, h);
    bbc_frame(r, edge, x, y, w, h);
}

/* ---- Sprites ----------------------------------------------------------- */

/*
 * Every car on the road, drawn on squared paper.
 *
 * A car is twelve BBC pixels across and forty-two down, nose up — 48 by 84
 * logical, over a 46 by 84 box, the extra pixel each side being the tyres —
 * and a crossing car is the same car turned on its side, twenty-one across
 * and twenty-three down. The two cannot be one drawing rotated: a BBC pixel
 * is twice as wide as it is tall, so a car turned through a right angle on
 * the grid is a car twice as long.
 *
 * The slots are the paint: '1' the body, '2' the roof (a vinyl roof, a cab's
 * sign, a squad car's white), '3' the headlamps (white when lit) and '4' the
 * tail lamps. Glass is black and the tyres are blue — on a black road a black
 * tyre is no tyre at all, and blue is the colour this machine said "dark" in.
 */
BBC_SPRITE(SPR_CAR_SALOON,
    "...111111...",
    "..31111113..",
    ".3311111133.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    ".1111111111.",
    ".1111111111.",
    ".1KKKKKKKK1.",
    ".1KWKKKKKK1.",
    ".11KWKKKK11.",
    ".11KKKKKK11.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".11KKKKKK11.",
    ".11KKKKKK11.",
    ".1KKKKKKKK1.",
    ".1111111111.",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".4411111144.",
    "..41111114..",
    "...111111...");

/* Short in the tail: the roof runs back to a near-vertical hatch. */
BBC_SPRITE(SPR_CAR_HATCH,
    "...111111...",
    "..31111113..",
    ".3311111133.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".1KKKKKKKK1.",
    ".1KWKKKKKK1.",
    ".11KWKKKK11.",
    ".11KKKKKK11.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    "B1K222222K1B",
    "B1K222222K1B",
    "B11KKKKKK11B",
    "B1KKKKKKKK1B",
    "B1111111111B",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".4411111144.",
    "..41111114..",
    "...111111...");

/* An estate: the roof all the way back, and a ribbed load floor under it. */
BBC_SPRITE(SPR_CAR_ESTATE,
    "...111111...",
    "..31111113..",
    ".3311111133.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    "B1111111111B",
    ".1111111111.",
    ".1111111111.",
    ".1KKKKKKKK1.",
    ".1KWKKKKKK1.",
    ".11KWKKKK11.",
    ".11KKKKKK11.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K2K22K2K1.",
    "B1K222222K1B",
    "B1K2K22K2K1B",
    "B1K222222K1B",
    "B1K2K22K2K1B",
    "B1K222222K1B",
    ".1KKKKKKKK1.",
    ".11KKKKKK11.",
    ".1111111111.",
    ".1111111111.",
    ".1111111111.",
    ".4411111144.",
    "..41111114..",
    "...111111...");

/*
 * Chuck's car is the one fastback on the road — a long bonnet, the rear glass
 * laid nearly flat — and the one car in Chuck's own cyan, with a white double
 * stripe nose to tail, so it is his from the first frame and from the corner of
 * an eye. The stripe is part of the drawing rather than a slot: it is what the
 * car is, not a paint it could be in.
 */
BBC_SPRITE(SPR_CAR_COUPE,
    "...11WW11...",
    "..311WW113..",
    ".3311WW1133.",
    ".1111WW1111.",
    ".1111WW1111.",
    ".1111WW1111.",
    "B1111WW1111B",
    "B1111WW1111B",
    "B1111WW1111B",
    "B1111WW1111B",
    "B1111WW1111B",
    ".1111WW1111.",
    ".1111WW1111.",
    ".1111WW1111.",
    ".1111WW1111.",
    ".1111111111.",
    ".1KKKKKKKK1.",
    ".1KWKKKKKK1.",
    ".11KWKKKK11.",
    ".11KKKKKK11.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".1K222222K1.",
    ".11KKKKKK11.",
    ".11KKKKKK11.",
    ".111KKKK111.",
    ".111KKKK111.",
    "B111KKKK111B",
    "B1111KK1111B",
    "B1111111111B",
    "B1111WW1111B",
    "B1111WW1111B",
    ".1111WW1111.",
    ".1111WW1111.",
    ".1111WW1111.",
    ".1111WW1111.",
    ".1111WW1111.",
    ".4411WW1144.",
    "..411WW114..",
    "...11WW11...");

/*
 * The SUV is a box on purpose — a short bonnet, a roof that runs almost to the
 * tail, a near-vertical tailgate with a spare hung on it, and a white roof rack
 * standing proud of the lot — so it cannot be mistaken for any of the rounded
 * cars around it. Fourteen across and forty-nine down over its 54 by 98 box.
 */
BBC_SPRITE(SPR_CAR_SUV,
    "...11111111...",
    "..3111111113..",
    ".331111111133.",
    ".111111111111.",
    ".111111111111.",
    "B111111111111B",
    "B111111111111B",
    "B111111111111B",
    "B111111111111B",
    "B111111111111B",
    "B111111111111B",
    ".111111111111.",
    ".1KKKKKKKKKK1.",
    ".1KWKKKKKKKK1.",
    ".1KKWKKKKKKK1.",
    ".11KKKKKKKK11.",
    ".1KWWWWWWWWK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KWWWWWWWWK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KWWWWWWWWK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    ".1KW222222WK1.",
    "B1KWWWWWWWWK1B",
    "B1KW222222WK1B",
    "B1KW222222WK1B",
    "B1KW222222WK1B",
    "B1KW222222WK1B",
    "B1KWWWWWWWWK1B",
    "B1K22222222K1B",
    ".11KKKKKKKK11.",
    ".1KKKKKKKKKK1.",
    ".111111111111.",
    ".441111111144.",
    ".441111111144.",
    "..1111111111..",
    "....KWWWWK....",
    "....WKKKKW....",
    ".....WWWW.....");

/* The saloon on its side, nose to the right, for the cross streets. */
BBC_SPRITE(SPR_CAR_SIDE,
    "...BBB........BBB....",
    "..11111111111111111..",
    ".1111111111111111111.",
    "411111111111111111113",
    "4111111KKKKKKK1111113",
    "4111111K2222WK1111113",
    "1111111K2222KW1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "1111111K2222KK1111111",
    "4111111K2222KK1111113",
    "4111111KKKKKKK1111113",
    "411111111111111111113",
    ".1111111111111111111.",
    "..11111111111111111..",
    "...BBB........BBB....");

/*
 * A squad car on its side, holding a cross street: white with the red flank
 * stripe the night shift's cars carried, and the light bar across the roof.
 * '3' is the bar's near half and '4' its far half — the two halves the flash
 * alternates between, so a bar reads as a bar rather than as a lamp.
 */
BBC_SPRITE(SPR_CAR_SQUAD,
    "...BBB........BBB....",
    "..11111111111111111..",
    ".RRRRRRRRRRRRRRRRRRR.",
    "R11111111111111111111",
    "K111111KKKKKKK111111W",
    "K111111K2333WK111111W",
    "1111111K2333KW1111111",
    "1111111K2333KK1111111",
    "1111111K2333KK1111111",
    "1111111K2333KK1111111",
    "1111111K2KKKKK1111111",
    "1111111K2KKKKK1111111",
    "1111111K2KKKKK1111111",
    "1111111K2444KK1111111",
    "1111111K2444KK1111111",
    "1111111K2444KK1111111",
    "1111111K2444KK1111111",
    "K111111K2444KK111111W",
    "K111111KKKKKKK111111W",
    "R11111111111111111111",
    ".RRRRRRRRRRRRRRRRRRR.",
    "..11111111111111111..",
    "...BBB........BBB....");

/* The dashboard's car, one pip per hit it can take, seen from the side the way
 * every lives counter of the period drew its car: a roof, the glass, the body
 * and two wheels. '1' is the paint and '2' the glass and the wheels. */
BBC_SPRITE(SPR_PIP_CAR,
    ".111..",
    "121211",
    "111111",
    ".2..2.");

/* A condenser from above: a white housing with the fan in it, the blades an X
 * rather than a cross — a cross in a circle reads as a crosshair — and the
 * louvred side panel in blue. */
BBC_SPRITE(SPR_ROOF_CONDENSER,
    "WWWWWB",
    "WKKKWB",
    "KWKWKB",
    "KKWKKB",
    "KKWKKB",
    "KWKWKB",
    "WKKKWB",
    "WWWWWB");

/*
 * Chuck on foot, seen from straight above — which is the one way the drive
 * sees anybody. The platformer's figures are side views, and a side view
 * running up a pavement reads as a man moonwalking; so the drive has its own
 * small drawing of him in the cast's colours, red cap, white hands,
 * cyan jacket, blue trousers and red shoes, facing up the screen. A stride is
 * the drawing and its mirror, which swaps which foot is out in front.
 */
BBC_SPRITE(SPR_TOP_CHUCK_RUN,
    "..R....",
    "..B..W.",
    "..B..C.",
    ".CRRRC.",
    "CCRRRCC",
    "CCRRRCC",
    ".CCCCC.",
    "W.CCC..",
    "...B...",
    "...B...",
    "...R...");

BBC_SPRITE(SPR_TOP_CHUCK_STAND,
    ".......",
    ".......",
    ".......",
    ".CRRRC.",
    "CCRRRCC",
    "CCRRRCC",
    "CCCCCCC",
    "W.BBB.W",
    "..B.B..",
    "..R.R..",
    ".......");

/* Facing right: the shoulders run down the screen and the stride across it.
 * Mirrored for the run to the left, and turned top to bottom to swap the
 * feet. */
BBC_SPRITE(SPR_TOP_CHUCK_SIDE,
    "....CW..",
    "...CC...",
    "..CCC...",
    "RBCRRR..",
    "..CRRRR.",
    "..CRRRBR",
    "..CCC...",
    "..CC....",
    ".WC.....");

/* ---- Drawing a sprite any way up ----------------------------------------- */

static BbcColour slot_colour(int code, const BbcColour *slots)
{
    if (code >= BBC_CODE_SLOT0)
        return slots != NULL ? slots[code - BBC_CODE_SLOT0] : BBC_WHITE;
    return (BbcColour)code;
}

/*
 * A sprite with its top-left at (x, y), mirrored across for `flip_x` and top
 * to bottom for `flip_y`. `bbc_sprite` only mirrors across, which is all a
 * figure walking a floor ever needs; a car driving at the player is a car
 * turned end over end, so this is the one blitter that can do both. With
 * `solid` every opaque pixel goes down in `solid_colour` — the flash a car
 * gives when it is hit.
 */
static void blit_sprite(SDL_Renderer *r, const BbcSprite *sprite, float x,
                        float y, bool flip_x, bool flip_y,
                        const BbcColour *slots, bool solid,
                        BbcColour solid_colour)
{
    int w = bbc_sprite_w(sprite);
    float left = bbc_snap_x(x);
    float top = bbc_snap_y(y);
    for (int row = 0; row < sprite->h; ++row)
    {
        const char *line = sprite->rows[flip_y ? sprite->h - 1 - row : row];
        int col = 0;
        while (col < w && line[col] != '\0')
        {
            int code = bbc_sprite_code(line[col]);
            int start = col;
            while (col < w && line[col] != '\0' &&
                   bbc_sprite_code(line[col]) == code)
                ++col;
            if (code < 0)
                continue;
            int a = flip_x ? w - col : start;
            BbcColour c = solid ? solid_colour : slot_colour(code, slots);
            bbc_block(r, c, left + (float)(a * BBC_PX_W),
                      top + (float)(row * BBC_PX_H), col - start, 1);
        }
    }
}

/* The top-left that puts a sprite's middle on (cx, cy), rounded to the grid
 * rather than floored, so a car whose box is not a whole number of cells wide
 * sits over the middle of it. */
static void corner_for_centre(const BbcSprite *sprite, float cx, float cy,
                          float *x, float *y)
{
    float w = (float)(bbc_sprite_w(sprite) * BBC_PX_W);
    float h = (float)(sprite->h * BBC_PX_H);
    *x = floorf((cx - w * 0.5f) / (float)BBC_PX_W + 0.5f) * (float)BBC_PX_W;
    *y = floorf((cy - h * 0.5f) / (float)BBC_PX_H + 0.5f) * (float)BBC_PX_H;
}

/* ---- Cars ---------------------------------------------------------------- */

/* Which way a car's nose points on the glass. */
typedef enum
{
    NOSE_UP,
    NOSE_DOWN,
    NOSE_LEFT,
    NOSE_RIGHT
} NoseDir;

/* Where a car was drawn: its rectangle on the glass and the way it faces, so
 * the beam, the lamps' smear and the damage can all be laid from the drawing
 * rather than re-derived from the simulation. */
typedef struct
{
    float x, y, w, h;
    NoseDir nose;
} CarPlace;

static CarPlace place_car(const BbcSprite *sprite, float cx, float cy,
                          NoseDir nose)
{
    CarPlace place;
    corner_for_centre(sprite, cx, cy, &place.x, &place.y);
    place.w = (float)(bbc_sprite_w(sprite) * BBC_PX_W);
    place.h = (float)(sprite->h * BBC_PX_H);
    place.nose = nose;
    return place;
}

static void draw_car_at(SDL_Renderer *r, const BbcSprite *sprite,
                        const CarPlace *place, const BbcColour *slots,
                        bool solid, BbcColour solid_colour)
{
    bool flip_x = place->nose == NOSE_LEFT;
    bool flip_y = place->nose == NOSE_DOWN;
    blit_sprite(r, sprite, place->x, place->y, flip_x, flip_y, slots, solid,
                solid_colour);
}

/*
 * A headlamp's throw: a wedge of dots on the asphalt ahead of the nose,
 * widening as it goes. The dots are anchored to the road, not to the car, so as
 * a car drives the wedge sweeps over a texture that stays where it is — which
 * is exactly what a beam on a road does, and what a transparent cone could
 * never show on a machine with no transparency. The near half is denser than
 * the far: the light is stronger close to the lamp, said in dots per row.
 */
static void draw_head_beam(SDL_Renderer *r, const CarPlace *car, float length,
                           float spread_px, BbcColour ink)
{
    const bool vertical = car->nose == NOSE_UP || car->nose == NOSE_DOWN;
    const float across = vertical ? car->w : car->h;
    const float half0 = across * 0.34f;
    const float step = vertical ? (float)BBC_PX_H : (float)BBC_PX_W;
    int steps = (int)(length / step);
    for (int k = 0; k < steps; ++k)
    {
        float t = (float)k / (float)(steps > 1 ? steps - 1 : 1);
        float half = half0 + (spread_px - half0) * t;
        BbcDither pattern = t < 0.35f ? BBC_DITHER_SPARSE : BBC_DITHER_DOTS;
        float d = (float)(k + 1) * step;
        if (vertical)
        {
            float cx = car->x + car->w * 0.5f;
            float y = car->nose == NOSE_UP ? car->y - d
                                           : car->y + car->h + d - step;
            bbc_stipple(r, ink, pattern, cx - half, y, half * 2.0f, step);
        }
        else
        {
            float cy = car->y + car->h * 0.5f;
            float x = car->nose == NOSE_RIGHT ? car->x + car->w + d - step
                                              : car->x - d;
            bbc_stipple(r, ink, pattern, x, cy - half, step, half * 2.0f);
        }
    }
}

/* The tail lamps' smear on the wet road behind a braking car: a few rows of
 * red dots, longer the harder the brake. */
static void draw_brake_smear(SDL_Renderer *r, const CarPlace *car, float length)
{
    if (car->nose != NOSE_UP && car->nose != NOSE_DOWN)
        return;
    float y = car->nose == NOSE_UP ? car->y + car->h : car->y - length;
    for (int side = 0; side < 2; ++side)
    {
        float x = side == 0 ? car->x + 4.0f : car->x + car->w - 12.0f;
        bbc_stipple(r, BBC_RED, BBC_DITHER_SPARSE, x, y, 8.0f, length);
    }
}

/*
 * Burnt out: the car's own drawing in scorched red, with black laid over it on
 * a checkerboard — red and black are what this machine called soot — and the
 * glass crazed white. The stipple covers the car's rectangle, which on a black
 * road blackens nothing but the car.
 */
static void draw_wreck_body(SDL_Renderer *r, const BbcSprite *sprite,
                            const CarPlace *place, unsigned seed)
{
    static const BbcColour SCORCHED[BBC_SPRITE_SLOTS] = {
        BBC_RED, BBC_RED, BBC_BLUE, BBC_BLACK};
    draw_car_at(r, sprite, place, SCORCHED, false, BBC_BLACK);
    bbc_stipple(r, BBC_BLACK, BBC_DITHER_CHECK, place->x, place->y, place->w,
                place->h);
    /* Crazed glass, keyed to the car rather than to where it has drifted to,
     * so the damage holds still. */
    for (unsigned i = 0; i < 7u; ++i)
    {
        unsigned h = hash_u(seed * 2654435761u + i * 0x9e3779b9u);
        float gx = place->x + 8.0f + spread(h, place->w - 16.0f);
        float gy = place->y + 8.0f + spread(h >> 10, place->h - 16.0f);
        bbc_plot(r, BBC_WHITE, gx, gy);
    }
}

/*
 * The smoke off a wreck: a few puffs, each born at the engine bay and drifting
 * down and to the right on the wind, growing as it goes. Grey is black and
 * white on a checkerboard, so each puff is a disc of exactly that, with a
 * lighter cap where the street light catches its top. Everything comes off how
 * long the car has been a wreck, so it is the same smoke every time the moment
 * is drawn.
 */
static void draw_wreck_smoke(SDL_Renderer *r, const CarPlace *car, float wreck,
                             unsigned seed)
{
    /* The fire under it goes out after a while, and the smoke with it. */
    if (wreck > 13.0f)
        return;
    float ox = car->x + car->w * 0.5f;
    float oy = car->nose == NOSE_DOWN ? car->y + car->h * 0.8f
                                      : car->y + car->h * 0.2f;
    if (car->nose == NOSE_LEFT || car->nose == NOSE_RIGHT)
    {
        ox = car->nose == NOSE_RIGHT ? car->x + car->w * 0.8f
                                     : car->x + car->w * 0.2f;
        oy = car->y + car->h * 0.5f;
    }
    int puffs = wreck > 9.0f ? 2 : 4;
    for (int k = 0; k < puffs; ++k)
    {
        unsigned h = hash_u(seed + (unsigned)k * 0x85ebca6bu);
        float life = fmodf(wreck * 0.55f + (float)k * 0.25f, 1.0f);
        float sway = (float)(h % 7u) - 3.0f;
        float px = ox + life * 30.0f + sway * life;
        float py = oy + life * 22.0f - (float)((h >> 8) % 5u);
        float radius = 6.0f + life * 12.0f;
        pattern_ellipse(r, BBC_BLACK, BBC_WHITE, BBC_DITHER_CHECK, px, py,
                        radius, radius, true);
        pattern_ellipse(r, BBC_BLACK, BBC_WHITE, BBC_DITHER_DENSE,
                        px - radius * 0.3f, py - radius * 0.3f, radius * 0.5f,
                        radius * 0.5f, true);
    }
}

/* A short burst of glass and sparks, then just a dead car in the road. Keyed
 * to the car and not to its screen x: a wreck drifts, and a burst hashed off
 * where it has drifted to re-rolled itself every pixel. */
static void draw_wreck_debris(SDL_Renderer *r, const CarPlace *car,
                              float wreck, unsigned seed)
{
    if (wreck > 0.7f)
        return;
    float cx = car->x + car->w * 0.5f;
    float cy = car->y + car->h * 0.5f;
    for (unsigned i = 0; i < 12u; ++i)
    {
        unsigned h = hash_u(i * 2246822519u + seed);
        float angle = (float)(h % 628u) * 0.01f;
        float distance = 10.0f + (float)((h >> 9) % 34u) * wreck * 3.0f;
        bbc_plot(r, (i & 1u) ? BBC_YELLOW : BBC_WHITE,
                 cx + cosf(angle) * distance, cy + sinf(angle) * distance);
    }
    /* The flash of the impact itself, for the first instant. */
    if (wreck < 0.15f)
        bbc_disc(r, BBC_YELLOW, cx, cy, 12.0f);
}

/* ---- The traffic ----------------------------------------------------------- */

/* Four paints, none of them cyan — that is Chuck's — and none of them blue,
 * which is the SUV's and the road's. */
static const BbcColour TRAFFIC_BODY[4] = {BBC_RED, BBC_GREEN, BBC_WHITE,
                                          BBC_YELLOW};

static NoseDir traffic_nose(const ChaseCar *car)
{
    if (car->kind == CHASE_CAR_CROSSING)
    {
        /* Which way it faces, not which way it is moving: a car waiting at a
         * standstill has no velocity to read, and a wreck is sliding wherever
         * the crash knocked it. */
        return car->heading < 0.0f ? NOSE_LEFT : NOSE_RIGHT;
    }
    return car->kind == CHASE_CAR_ONCOMING ? NOSE_DOWN : NOSE_UP;
}

/* The slot a car occupies is its identity for as long as it is on the road,
 * so its body style, its roof and what a crash breaks on it hold still. */
static unsigned traffic_seed(const ChaseCar *car, int slot)
{
    return hash_u((unsigned)slot * 2654435761u + (unsigned)car->variant * 40503u +
                  0x51u);
}

static const BbcSprite *traffic_sprite(const ChaseCar *car, unsigned seed)
{
    if (car->kind == CHASE_CAR_CROSSING)
        return &SPR_CAR_SIDE;
    static const BbcSprite *const SHAPES[3] = {&SPR_CAR_SALOON, &SPR_CAR_HATCH,
                                               &SPR_CAR_ESTATE};
    return SHAPES[seed % 3u];
}

static CarPlace traffic_place(const ChaseView *view, const ChaseCar *car,
                              int slot)
{
    unsigned seed = traffic_seed(car, slot);
    return place_car(traffic_sprite(car, seed), screen_x(view, car->x),
                     screen_y(view, car->y), traffic_nose(car));
}

/* One in three of the yellow cars is a cab. */
static bool traffic_is_cab(const ChaseCar *car, unsigned seed)
{
    return (car->variant & 3) == 3 && (seed >> 8) % 3u == 0u;
}

/* Everyone out tonight has their lights on. The cars coming at the player throw
 * the long beam; the ones ahead of him throw a shorter one away up the road. */
static void draw_traffic_beam(SDL_Renderer *r, const ChaseView *view,
                              const ChaseCar *car, int slot)
{
    if (car->wreck_time > 0.0f)
        return;
    CarPlace place = traffic_place(view, car, slot);
    if (car->kind == CHASE_CAR_TRAFFIC)
        draw_head_beam(r, &place, 64.0f, CHASE_CAR_WIDTH * 0.62f, BBC_WHITE);
    else
        draw_head_beam(r, &place, 112.0f, CHASE_CAR_WIDTH * 0.80f, BBC_WHITE);
}

static void draw_traffic_car(SDL_Renderer *r, const ChaseView *view,
                             const ChaseCar *car, int slot)
{
    unsigned seed = traffic_seed(car, slot);
    const BbcSprite *sprite = traffic_sprite(car, seed);
    CarPlace place = traffic_place(view, car, slot);
    if (car->wreck_time > 0.0f)
    {
        draw_wreck_body(r, sprite, &place, seed);
        draw_wreck_smoke(r, &place, car->wreck_time, seed);
        draw_wreck_debris(r, &place, car->wreck_time, seed);
        return;
    }

    BbcColour body = TRAFFIC_BODY[car->variant & 3];
    /* A vinyl roof on one car in three of the two darker paints: the cheapest
     * way a road of four paints becomes a road of more than four cars. */
    BbcColour roof = body;
    if ((body == BBC_RED || body == BBC_GREEN) && (seed >> 4) % 3u == 0u)
        roof = BBC_WHITE;
    bool cab = traffic_is_cab(car, seed);
    BbcColour paint[BBC_SPRITE_SLOTS] = {body, roof, BBC_WHITE, BBC_RED};
    draw_car_at(r, sprite, &place, paint, false, BBC_BLACK);

    /* A cab says so with a lamp on the roof: white, ringed in black so it
     * reads on yellow. */
    if (cab && car->kind != CHASE_CAR_CROSSING)
    {
        float cx = place.x + place.w * 0.5f;
        float cy = place.y + place.h * 0.5f;
        bbc_fill(r, BBC_BLACK, cx - 8.0f, cy - 4.0f, 16.0f, 8.0f);
        bbc_fill(r, BBC_WHITE, cx - 4.0f, cy - 2.0f, 8.0f, 4.0f);
    }
}

/* ---- The SUV --------------------------------------------------------------- */

/* Blue all over, with the rack in white: the darkest car on the road and the
 * one nobody can see into. Blue is the colour this machine said "dark" in, and
 * no other car out here wears it. */
static const BbcColour SUV_PAINT[BBC_SPRITE_SLOTS] = {BBC_BLUE, BBC_BLUE,
                                                      BBC_WHITE, BBC_RED};

static CarPlace target_place(const ChaseView *view, const Chase *chase)
{
    return place_car(&SPR_CAR_SUV, screen_x(view, chase->target.x),
                     screen_y(view, chase->target.y), NOSE_UP);
}

static void draw_target_car(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase)
{
    CarPlace place = target_place(view, chase);
    bool braking = chase->phase == CHASE_PHASE_ARRIVAL;
    if (braking)
        draw_brake_smear(r, &place, 16.0f);
    draw_car_at(r, &SPR_CAR_SUV, &place, SUV_PAINT, false, BBC_BLACK);

    /* The pursuit bracket, borrowed from the cutscene's target framing: four
     * corners in cyan, the colour of everything that is Chuck's, flashing to
     * white the way the machine's cursor did. */
    BbcColour mark = bbc_flash_pair(BBC_CYAN, BBC_WHITE);
    float left = place.x - 8.0f;
    float right = place.x + place.w + 8.0f;
    float top = place.y - 10.0f;
    float bottom = place.y + place.h + 10.0f;
    const float arm_w = 12.0f;
    const float arm_h = 12.0f;
    bbc_fill(r, mark, left, top, arm_w, 2.0f);
    bbc_fill(r, mark, left, top, 4.0f, arm_h);
    bbc_fill(r, mark, right - arm_w, top, arm_w, 2.0f);
    bbc_fill(r, mark, right - 4.0f, top, 4.0f, arm_h);
    bbc_fill(r, mark, left, bottom - 2.0f, arm_w, 2.0f);
    bbc_fill(r, mark, left, bottom - arm_h, 4.0f, arm_h);
    bbc_fill(r, mark, right - arm_w, bottom - 2.0f, arm_w, 2.0f);
    bbc_fill(r, mark, right - 4.0f, bottom - arm_h, 4.0f, arm_h);
}

/* ---- Chuck's car ----------------------------------------------------------- */

static CarPlace player_place(const ChaseView *view, const Chase *chase)
{
    return place_car(&SPR_CAR_COUPE, screen_x(view, chase->player.x),
                     screen_y(view, chase->player.y), NOSE_UP);
}

static void draw_player_car(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase)
{
    const ChasePlayerCar *car = &chase->player;
    CarPlace place = player_place(view, chase);
    bool braking = car->engine_running &&
                   (chase->phase == CHASE_PHASE_ARRIVAL ||
                    chase->phase == CHASE_PHASE_FAILED);
    if (braking)
        draw_brake_smear(r, &place, 20.0f);

    /* Unlit lenses until he turns the key. */
    BbcColour paint[BBC_SPRITE_SLOTS] = {
        BBC_CYAN, BBC_CYAN, car->engine_running ? BBC_WHITE : BBC_BLUE,
        BBC_RED};
    /* The hit flash: the whole car in white, every other beat of the mercy
     * window — the flash a sprite gave on this machine when it was struck. */
    bool flash = car->invuln_timer > 0.0f &&
                 fmodf(car->invuln_timer, 0.24f) > 0.12f;
    draw_car_at(r, &SPR_CAR_COUPE, &place, paint, flash, BBC_WHITE);

    /*
     * Damage is legible on the car itself, not only on the dashboard: each knock
     * leaves its mark where it landed — bare metal scraped white along a flank
     * and a corner stove in to black. Fixed places, so the car keeps its scars
     * for the rest of the attempt instead of re-rolling them.
     */
    int lost = CHASE_INTEGRITY - car->integrity;
    if (!flash)
    {
        for (int k = 0; k < lost && k < 3; ++k)
        {
            bool right = (k & 1) != 0;
            float x = right ? place.x + place.w - 8.0f : place.x + 4.0f;
            float y = place.y + (k == 2 ? 60.0f : (k == 1 ? 40.0f : 20.0f));
            bbc_fill(r, BBC_WHITE, x, y, 4.0f, 10.0f);
            bbc_fill(r, BBC_BLACK, right ? x - 4.0f : x + 4.0f, y + 2.0f, 4.0f,
                     4.0f);
            float corner_y = k == 1 ? place.y + place.h - 6.0f : place.y + 2.0f;
            bbc_fill(r, BBC_BLACK, right ? place.x + place.w - 12.0f
                                         : place.x + 8.0f,
                     corner_y, 4.0f, 4.0f);
        }
    }

    /* Grinding along a kerb: sparks off the flank, trailing behind. */
    if (car->scrape_timer > 0.0f)
    {
        bool right = car->x >= CHASE_ROAD_WIDTH * 0.5f;
        float x = right ? place.x + place.w : place.x - 4.0f;
        unsigned beat = (unsigned)(chase->time * 20.0f);
        for (int i = 0; i < 5; ++i)
        {
            unsigned h = hash_u(beat * 977u + (unsigned)i * 131u);
            float sx = x + (right ? 1.0f : -1.0f) * spread(h, 8.0f);
            float sy = place.y + 30.0f + (float)i * 10.0f + spread(h >> 8, 6.0f);
            bbc_plot(r, (h & 1u) ? BBC_YELLOW : BBC_WHITE, sx, sy);
        }
    }
}

/* ---- Chuck on foot, during the opening beat ----------------------------- */

static void draw_chuck_on_foot(SDL_Renderer *r, const ChaseView *view,
                               const Chase *chase)
{
    const float run_start = CHASE_DEPARTURE_CHUCK_RUN;
    const float run_end = CHASE_DEPARTURE_CAR_DOOR;
    if (chase->phase != CHASE_PHASE_DEPARTURE ||
        chase->phase_time > run_end + 0.05f)
    {
        return;
    }

    float progress = clamp01((chase->phase_time - run_start) /
                             (run_end - run_start));
    /*
     * He runs up the pavement past his parked car, then cuts round its nose to
     * the driver's door. Two straight legs are enough to read as "he gets in"
     * at this scale, and they keep him clear of his own bodywork.
     */
    const float pavement_x = CHASE_ROAD_WIDTH + CHASE_PAVEMENT_WIDTH * 0.5f;
    float door_x = chase->player.x - CHASE_CAR_WIDTH * 0.5f - 11.0f;
    float road_x = pavement_x;
    float road_y;
    bool across = progress >= 0.62f;
    if (!across)
    {
        float leg = progress / 0.62f;
        road_y = chase->player.y - 86.0f + 138.0f * leg;
    }
    else
    {
        /* Straight across the front of the car, never over its bodywork. */
        float leg = (progress - 0.62f) / 0.38f;
        road_x = pavement_x + (door_x - pavement_x) * leg;
        road_y = chase->player.y + 52.0f;
    }
    bool running = progress > 0.0f && progress < 1.0f;
    /* ~2.4 strides a second at a flat run: the drawing and its mirror, so a
     * stride is two frames and the feet swap on each. */
    bool stride = ((int)floorf(chase->phase_time * 4.8f) & 1) != 0;

    const BbcSprite *sprite = &SPR_TOP_CHUCK_STAND;
    bool flip_x = false;
    bool flip_y = false;
    if (running && !across)
    {
        sprite = &SPR_TOP_CHUCK_RUN;
        flip_x = stride;
    }
    else if (running)
    {
        /* Across the bonnet, to the left: the side drawing, mirrored to face
         * the way he goes, with its stride swapped top to bottom. */
        sprite = &SPR_TOP_CHUCK_SIDE;
        flip_x = door_x < pavement_x;
        flip_y = stride;
    }
    float x = 0.0f;
    float y = 0.0f;
    corner_for_centre(sprite, screen_x(view, road_x), screen_y(view, road_y), &x,
                      &y);
    /* His shadow first, a pixel down and to the right, opaque — the only
     * shadow the machine had, and what lifts a figure this small off a
     * pavement laid in the same blue as his trousers. */
    blit_sprite(r, sprite, x + 4.0f, y + 2.0f, flip_x, flip_y, NULL, true,
                BBC_BLACK);
    blit_sprite(r, sprite, x, y, flip_x, flip_y, NULL, false, BBC_BLACK);
}

/* ---- Street ---------------------------------------------------------- */

static bool inside_junction(const Chase *chase, float y, float margin)
{
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        if (fabsf(junction->y - y) < CHASE_JUNCTION_HALF + margin)
            return true;
    }
    return false;
}

/*
 * The roofs either side of the drive.
 *
 * They are the one surface in this scene seen from straight above, and a roof
 * reads as a roof from three things: the parapet standing up round it, the
 * membrane inside it, and the hardware standing on it — every piece throwing
 * the same shadow, down and to the right, away from the moon. A shadow here is
 * black laid down a pixel or two off the thing, opaque, which is the only
 * shadow the machine had and all the depth a roof needs.
 *
 * The roofs are blue and the road is black: the city is the far thing and the
 * road the near one, which is the depth every BBC screen was built on. The
 * fittings are where the other colours go — a white condenser, a red-and-yellow
 * water tank, a lit skylight — rationed, so forty roofs scrolling past read as
 * a city rather than as confetti.
 *
 * Everything is keyed to the block's seed, which comes off its world index and
 * never off where the block happens to be on screen: hardware that moved as a
 * roof scrolled would boil, which is the rule `art_repeat` carries for the
 * backdrops.
 */
typedef enum
{
    ROOF_BARE,
    ROOF_CONDENSER,
    ROOF_VENTS,
    ROOF_TANK,
    ROOF_SKYLIGHT,
    ROOF_BULKHEAD,
    ROOF_DISH
} RoofFitting;

/* Each fitting's footprint, so a bay can place it wholly inside itself. */
static void roof_fitting_size(RoofFitting fitting, float *w, float *h)
{
    switch (fitting)
    {
    case ROOF_CONDENSER:
        *w = 28.0f;
        *h = 20.0f;
        return;
    case ROOF_VENTS:
        *w = 28.0f;
        *h = 14.0f;
        return;
    case ROOF_TANK:
        *w = 28.0f;
        *h = 28.0f;
        return;
    case ROOF_SKYLIGHT:
        *w = 32.0f;
        *h = 18.0f;
        return;
    case ROOF_BULKHEAD:
        *w = 32.0f;
        *h = 24.0f;
        return;
    case ROOF_DISH:
        *w = 16.0f;
        *h = 14.0f;
        return;
    case ROOF_BARE:
        break;
    }
    *w = 0.0f;
    *h = 0.0f;
}

/* A box standing on the roof: its top in `top`, a white arris along the two
 * sides that face the moon, and its shadow `lift` pixels down and right. */
static void roof_box(SDL_Renderer *r, BbcColour top, float x, float y, float w,
                     float h, float lift)
{
    bbc_fill(r, BBC_BLACK, x + 4.0f, y + lift, w, h);
    bbc_fill(r, top, x, y, w, h);
    bbc_fill(r, BBC_WHITE, x, y, w, 2.0f);
    bbc_fill(r, BBC_WHITE, x, y, 4.0f, h);
}

/* One fitting at (x, y), its footprint's top-left. */
static void draw_roof_fitting(SDL_Renderer *r, RoofFitting fitting, float x,
                              float y, unsigned seed)
{
    switch (fitting)
    {
    case ROOF_CONDENSER:
        bbc_fill(r, BBC_BLACK, x + 4.0f, y + 4.0f, 24.0f, 16.0f);
        bbc_sprite(r, &SPR_ROOF_CONDENSER, x, y, 1, NULL);
        return;
    case ROOF_VENTS:
    {
        int count = 2 + (int)(seed % 2u);
        for (int i = 0; i < count; ++i)
        {
            float vx = x + (float)i * 10.0f;
            float vy = y + 2.0f + (float)((seed >> (i * 3 + 4)) % 3u) * 2.0f;
            bbc_fill(r, BBC_BLACK, vx + 4.0f, vy + 2.0f, 4.0f, 6.0f);
            bbc_fill(r, BBC_WHITE, vx, vy, 4.0f, 6.0f);
            bbc_plot(r, BBC_BLUE, vx, vy + 4.0f);
        }
        return;
    }
    case ROOF_TANK:
    {
        /* The wooden water tank every roof in this part of town has one of:
         * brown is red and yellow on a checkerboard, ringed in red, with the
         * hatch in the middle of its lid. */
        float cx = x + 12.0f;
        float cy = y + 12.0f;
        bbc_disc(r, BBC_BLACK, cx + 6.0f, cy + 6.0f, 12.0f);
        bbc_disc(r, BBC_RED, cx, cy, 12.0f);
        pattern_ellipse(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, cx, cy, 9.0f,
                        9.0f, true);
        bbc_fill(r, BBC_RED, cx - 2.0f, cy - 10.0f, 4.0f, 20.0f);
        bbc_fill(r, BBC_WHITE, cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
        return;
    }
    case ROOF_SKYLIGHT:
    {
        /* Three panes, lit from the floor under them or dark. */
        bool lit = (seed & 0x100u) != 0u;
        roof_box(r, BBC_BLUE, x, y, 32.0f, 18.0f, 2.0f);
        for (int pane = 0; pane < 3; ++pane)
        {
            float px = x + 4.0f + (float)pane * 8.0f;
            if (lit)
                bbc_dither(r, BBC_YELLOW, BBC_WHITE, BBC_DITHER_SPARSE, px,
                           y + 4.0f, 4.0f, 12.0f);
            else
                bbc_fill(r, BBC_BLACK, px, y + 4.0f, 4.0f, 12.0f);
        }
        return;
    }
    case ROOF_BULKHEAD:
    {
        /* The stair head: the tallest thing on the roof, so the longest
         * shadow, and a lamp over the door on its lee side. */
        roof_box(r, BBC_BLUE, x, y, 32.0f, 24.0f, 8.0f);
        bbc_fill(r, BBC_BLACK, x + 4.0f, y + 12.0f, 28.0f, 2.0f);
        bbc_fill(r, BBC_YELLOW, x + 12.0f, y + 24.0f, 8.0f, 2.0f);
        bbc_stipple(r, BBC_YELLOW, BBC_DITHER_SPARSE, x + 4.0f, y + 26.0f,
                    24.0f, 6.0f);
        return;
    }
    case ROOF_DISH:
    {
        float cx = x + 8.0f;
        float cy = y + 7.0f;
        bbc_disc(r, BBC_BLACK, cx + 4.0f, cy + 4.0f, 7.0f);
        bbc_disc(r, BBC_WHITE, cx, cy, 7.0f);
        bbc_fill(r, BBC_BLUE, cx, cy, 4.0f, 2.0f);
        return;
    }
    case ROOF_BARE:
        return;
    }
}

/* `street_side` is which way the road is from this block: +1 to its right,
 * -1 to its left. The lip over it takes the street's light and the sign faces
 * it. */
static void draw_rooftop_block(SDL_Renderer *r, float x, float y, float w,
                               float h, unsigned seed, int street_side)
{
    /* The parapet ring, then the membrane inside it with the seams of the
     * rolled felt across it in black. Most roofs on the drive are felted in
     * blue; one in four is the older red tile of the blocks the ring road was
     * cut through, and one in six is tarred dark — which is all the variety a
     * row of flat roofs has, and enough to stop forty of them reading as one
     * roof stamped forty times. */
    const float lip_x = 8.0f;
    const float lip_y = 4.0f;
    bbc_fill(r, BBC_BLUE, x, y, w, h);
    float ix = x + lip_x;
    float iy = y + lip_y;
    float iw = w - lip_x * 2.0f;
    float ih = h - lip_y * 2.0f;
    unsigned felt = (seed >> 11) % 12u;
    if (felt < 3u)
        bbc_fill(r, BBC_RED, ix, iy, iw, ih);
    else if (felt < 5u)
        bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, ix, iy, iw, ih);
    bbc_frame(r, BBC_BLACK, ix - 4.0f, iy - 2.0f, iw + 8.0f, ih + 4.0f);
    for (float seam = 26.0f; seam < ih - 4.0f; seam += 26.0f)
        bbc_stipple(r, BBC_BLACK, BBC_DITHER_COLS, ix, iy + seam, iw, 2.0f);

    /* The coping along the lip catches the moon on the two sides that face
     * it; the street lip catches a little of the sodium from below. */
    bbc_fill(r, BBC_WHITE, x, y, w, 2.0f);
    bbc_fill(r, BBC_WHITE, x, y, 4.0f, h);
    float street_lip = street_side > 0 ? x + w - 4.0f : x;
    bbc_stipple(r, BBC_YELLOW, BBC_DITHER_ROWS, street_lip, y + 2.0f, 4.0f,
                h - 2.0f);
    /* The parapet's own shadow, in along the walls between the roof and the
     * moon. */
    bbc_fill(r, BBC_BLACK, ix, iy, iw, 2.0f);
    bbc_fill(r, BBC_BLACK, ix, iy, 4.0f, ih);

    /* Hardware in a grid of bays, so nothing lands on anything else, and a good
     * third of the bays left bare, so a roof reads as a roof rather than as a
     * yard. At most one tank and one stair head to a roof. */
    enum
    {
        ROOF_BAY_COLS = 2,
        ROOF_BAY_ROWS = 4
    };
    float bay_w = (iw - 16.0f) / (float)ROOF_BAY_COLS;
    float bay_h = (ih - 16.0f) / (float)ROOF_BAY_ROWS;
    bool tank = false;
    bool bulkhead = false;
    for (int bay = 0; bay < ROOF_BAY_COLS * ROOF_BAY_ROWS; ++bay)
    {
        unsigned roll = hash_u(seed ^ ((unsigned)bay + 1u) * 0x9e3779b9u);
        RoofFitting fitting = ROOF_BARE;
        switch (roll % 16u)
        {
        case 0u:
        case 1u:
        case 2u:
            fitting = ROOF_CONDENSER;
            break;
        case 3u:
        case 4u:
            fitting = ROOF_VENTS;
            break;
        case 5u:
            fitting = tank ? ROOF_BARE : ROOF_TANK;
            tank = true;
            break;
        case 6u:
        case 7u:
            fitting = ROOF_SKYLIGHT;
            break;
        case 8u:
            fitting = bulkhead ? ROOF_BARE : ROOF_BULKHEAD;
            bulkhead = true;
            break;
        case 9u:
            fitting = ROOF_DISH;
            break;
        default:
            break;
        }
        if (fitting == ROOF_BARE)
            continue;
        float fw = 0.0f;
        float fh = 0.0f;
        roof_fitting_size(fitting, &fw, &fh);
        float bx = ix + 8.0f + (float)(bay % ROOF_BAY_COLS) * bay_w;
        float by = iy + 8.0f + (float)(bay / ROOF_BAY_COLS) * bay_h;
        float fit_x = bbc_snap_x(bx + spread(roll >> 8, bay_w - fw - 8.0f));
        float fit_y = bbc_snap_y(by + spread(roll >> 16, bay_h - fh - 8.0f));
        draw_roof_fitting(r, fitting, fit_x, fit_y, roll);
    }

    /* A sign board standing on the street lip, its lit face to the road: from
     * above, a catwalk with a line of bulbs along one edge. The bulbs chase,
     * which is what a sign on this street does all night. */
    if ((seed & 5u) == 1u)
    {
        BbcColour lit = (seed & 8u) ? BBC_YELLOW : BBC_RED;
        float len = bbc_snap_y(44.0f + (float)((seed >> 5) % 30u));
        float sy = bbc_snap_y(y + h - 20.0f - len);
        float board_x = street_side > 0 ? x + w - 16.0f : x + 8.0f;
        float face_x = street_side > 0 ? board_x + 4.0f : board_x;
        bbc_fill(r, BBC_BLACK, board_x + 4.0f, sy + 4.0f, 8.0f, len);
        bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_ROWS, board_x, sy, 8.0f,
                   len);
        bbc_fill(r, lit, face_x, sy, 4.0f, len);
        for (float bulb = 0.0f; bulb < len; bulb += 6.0f)
            bbc_plot(r, BBC_WHITE, face_x, sy + bulb);
    }
}

/* The pavement each side of the road, laid in flags with the joints in black,
 * and the kerb along its foot: a white stone edge, jointed every stone. */
#define CHASE_PAVE_W 28.0f

static void render_blocks(SDL_Renderer *r, const ChaseView *view, int win_w)
{
    const float block = 220.0f;
    float road_left = road_left_x(view);
    float road_right = road_right_x(view);
    float first = floorf((view->camera_y - block) / block);
    float last = ceilf((view->camera_y + view->view_h + block) / block);

    const float flag = 24.0f;
    float flag_first = floorf(view->camera_y / flag) - 1.0f;
    float flag_last = ceilf((view->camera_y + view->view_h) / flag) + 1.0f;
    for (int side = 0; side < 2; ++side)
    {
        float px = side == 0 ? road_left - CHASE_PAVE_W : road_right;
        /* Concrete flags, blue and black on a checkerboard — wet paving under
         * a night sky, in a machine with no grey — with the joints between
         * them left black. */
        bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, px, view->view_top,
                   CHASE_PAVE_W, view->view_h);
        for (float index = flag_first; index <= flag_last; index += 1.0f)
        {
            float top = screen_y(view, (index + 1.0f) * flag);
            bbc_fill(r, BBC_BLACK, px, top, CHASE_PAVE_W, 2.0f);
        }
    }

    for (float index = first; index <= last; index += 1.0f)
    {
        float world_y = index * block;
        float top = screen_y(view, world_y + block);
        float height = block - 8.0f;
        unsigned seed = hash_u((unsigned)(int)index * 2654435761u);
        float left_edge = road_left - CHASE_PAVE_W - 4.0f;
        draw_rooftop_block(r, -24.0f, top, left_edge + 24.0f, height, seed, 1);
        float right_edge = road_right + CHASE_PAVE_W + 4.0f;
        draw_rooftop_block(r, right_edge, top,
                           (float)win_w + 24.0f - right_edge, height,
                           hash_u(seed + 77u), -1);
    }
}

/*
 * The road surface, and why it is more than black.
 *
 * It has rained all night, and a flat fill says nothing about that. What does:
 * water standing in the gutters and the dips with the rain still landing in it,
 * and the lamps coming back off it (`render_streetlights`). Scale comes from
 * what is set into it — covers, gratings, cracks, the joints of a repair — at
 * the size a man would see them, because a road with nothing on it could be any
 * width at all. Every one of them is keyed to a world index along the road, so
 * they travel with it and never re-roll as it scrolls. All of them are blue,
 * the machine's dark, so the paint and the cars are the only bright things on
 * it.
 */

/* The street lamps' spacing along the road and how far each arm reaches out
 * from its mast, named because the road's puddles have to know where the
 * lamps are to give them back.
 *
 * The masts stand half a span off the round numbers, which puts the kerb at
 * y = 0 — where Chuck's car is parked for the departure, and where he runs up
 * the pavement past it to the door — midway between two of them. On the round
 * numbers one stood level with his car, on the kerb edge of a pavement 26px
 * wide, and he ran straight through it: a figure 20px across with his arms at
 * his sides has no line past a mast on that pavement, and once the lamp heads
 * were drawn over everything on the ground the pole was drawn over him too. */
#define CHASE_LAMP_SPAN 260.0f
#define CHASE_LAMP_PHASE 0.5f
#define CHASE_LAMP_REACH 32.0f

/* The world y of the mast nearest `y` along the road. */
static float nearest_lamp_y(float y)
{
    return (floorf(y / CHASE_LAMP_SPAN + 0.5f - CHASE_LAMP_PHASE) +
            CHASE_LAMP_PHASE) *
           CHASE_LAMP_SPAN;
}

/* A ring of rain landing in standing water: a drop, then a ring that opens and
 * goes cold — white, then cyan, then blue, one rung down the ladder at a time,
 * which is how this machine let a thing go. `life` runs 0 to 1. */
static void draw_ripple(SDL_Renderer *r, float x, float y, float life)
{
    if (life < 0.0f || life >= 1.0f)
        return;
    if (life < 0.3f)
    {
        bbc_plot(r, BBC_WHITE, x, y);
        return;
    }
    BbcColour c = life < 0.65f ? BBC_CYAN : BBC_BLUE;
    float rx = life < 0.65f ? 4.0f : 8.0f;
    float ry = life < 0.65f ? 2.0f : 4.0f;
    bbc_plot(r, c, x - rx, y);
    bbc_plot(r, c, x + rx, y);
    bbc_plot(r, c, x, y - ry);
    bbc_plot(r, c, x, y + ry);
}

/* Standing water: blue on black, three lobes so no two puddles are the same
 * shape and none of them is a rectangle, with a streak of sky across it and the
 * rain still landing in it. A puddle under a street lamp gives the lamp back
 * too, as a few short bars of yellow — `lamp` is how near one is, 0 to 1, and
 * `lamp_dx` which side it is on. */
static void draw_puddle(SDL_Renderer *r, const ChaseView *view, float cx,
                        float cy, float w, float h, unsigned seed, float time,
                        float lamp, float lamp_dx)
{
    float lobe_dx[3] = {(float)((seed >> 5) % 3u) * 4.0f - 4.0f, 0.0f,
                        (float)((seed >> 9) % 3u) * 4.0f - 4.0f};
    float lobe_w[3] = {w * 0.40f, w * 0.50f, w * 0.34f};
    float lobe_y[3] = {cy + h * 0.22f, cy + h * 0.50f, cy + h * 0.80f};
    float lobe_h[3] = {h * 0.26f, h * 0.32f, h * 0.22f};
    for (int k = 0; k < 3; ++k)
        pattern_ellipse(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK,
                        cx + lobe_dx[k], lobe_y[k], lobe_w[k], lobe_h[k], true);
    /* The sky caught in it, along the middle. */
    bbc_fill(r, BBC_BLUE, cx - w * 0.30f, cy + h * 0.46f, w * 0.50f, 2.0f);
    if (lamp > 0.0f)
    {
        float side = lamp_dx < 0.0f ? -1.0f : 1.0f;
        for (int bar = 0; bar < 3; ++bar)
        {
            float len = bbc_snap_x(w * (0.60f - (float)bar * 0.14f));
            float drift = view->steady_lights
                              ? 0.0f
                              : (sinf(time * 2.3f + (float)bar * 2.1f +
                                      (float)(seed % 11u)) > 0.3f
                                     ? 4.0f
                                     : 0.0f);
            float bx = cx + side * w * 0.12f - len * 0.5f + drift;
            bbc_fill(r, lamp > 0.45f ? BBC_YELLOW : BBC_RED, bx,
                     cy + h * 0.30f + (float)bar * 4.0f, len, 2.0f);
        }
    }
    for (unsigned k = 0; k < 2u; ++k)
    {
        float phase = time * 0.85f + (float)((seed >> (k * 6u)) % 64u) / 64.0f;
        float cycle = floorf(phase);
        unsigned ring = hash_u(seed + salt(cycle) * 0x9e3779b9u + k * 77u);
        float rx = cx - w * 0.3f + spread(ring, w * 0.6f);
        float ry = cy + h * 0.2f + spread(ring >> 12, h * 0.6f);
        draw_ripple(r, rx, ry, phase - cycle);
    }
}

/* A cast-iron cover set flush into the road: a blue rim, and the chequered
 * plate inside it in the same blue on black. */
static void draw_manhole(SDL_Renderer *r, float cx, float cy)
{
    bbc_disc(r, BBC_BLUE, cx, cy, 9.0f);
    pattern_ellipse(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, cx, cy, 6.0f,
                    6.0f, true);
    bbc_plot(r, BBC_WHITE, cx - 4.0f, cy - 6.0f);
}

/* A kerb grating: blue bars across a black slot, `w` into the road. */
static void draw_drain(SDL_Renderer *r, float x, float y, float w)
{
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_ROWS, x, y, w, 14.0f);
    bbc_frame(r, BBC_BLUE, x, y, w, 14.0f);
}

static void render_road(SDL_Renderer *r, const ChaseView *view,
                        const Chase *chase)
{
    const float road_left = road_left_x(view);
    const float road_right = road_right_x(view);
    const float top = view->view_top;
    const float height = view->view_h;
    bbc_fill(r, BBC_BLACK, road_left, top, CHASE_ROAD_WIDTH, height);

    /* The aggregate, a band at a time: a few blue stones in every stretch,
     * which at speed is most of what says the road is moving. */
    const float grain = 32.0f;
    float g_first = floorf(view->camera_y / grain) - 1.0f;
    float g_last = ceilf((view->camera_y + height) / grain) + 1.0f;
    for (float index = g_first; index <= g_last; index += 1.0f)
    {
        float band_top = screen_y(view, (index + 1.0f) * grain);
        unsigned band = hash_u(salt(index) * 0x27d4eb2du + 0x9au);
        for (unsigned k = 0; k < 5u; ++k)
        {
            unsigned g = hash_u(band + k * 0x9e3779b9u);
            bbc_plot(r, BBC_BLUE, road_left + spread(g, CHASE_ROAD_WIDTH),
                     band_top + spread(g >> 12, grain));
        }
    }

    /* Repair patches: a square of fresher aggregate, more stones to the
     * stretch than the road round it. */
    const float patch_span = 160.0f;
    float first = floorf(view->camera_y / patch_span);
    float last = ceilf((view->camera_y + height) / patch_span);
    for (float index = first; index <= last; index += 1.0f)
    {
        unsigned seed = hash_u((unsigned)(int)index * 40503u + 17u);
        if ((seed & 3u) != 0u)
            continue;
        float x = road_left + bbc_snap_x(spread(seed, CHASE_ROAD_WIDTH));
        float y = screen_y(view, index * patch_span);
        float w = bbc_snap_x(40.0f + spread(seed >> 7, 70.0f));
        float h = bbc_snap_y(20.0f + spread(seed >> 3, 26.0f));
        if (x + w > road_right - 8.0f)
            w = road_right - 8.0f - x;
        if (w < 8.0f)
            continue;
        bbc_stipple(r, BBC_BLUE, BBC_DITHER_DOTS, x, y, w, h);
    }

    /* Cracks: a thread of blue wandering down the lane. */
    const float crack_span = 124.0f;
    float c_first = floorf(view->camera_y / crack_span) - 1.0f;
    float c_last = ceilf((view->camera_y + height) / crack_span) + 1.0f;
    for (float index = c_first; index <= c_last; index += 1.0f)
    {
        unsigned seed = hash_u(salt(index) * 0x165667b1u + 0x2du);
        if (seed % 3u == 0u)
            continue;
        float x = road_left + 16.0f +
                  bbc_snap_x(spread(seed >> 5, CHASE_ROAD_WIDTH - 32.0f));
        float wy = index * crack_span +
                   2.0f * floorf(spread(seed >> 14, crack_span) * 0.5f);
        int steps = 3 + (int)((seed >> 20) % 4u);
        for (int s = 0; s < steps; ++s)
        {
            unsigned h = hash_u(seed + (unsigned)s * 0x9e3779b9u);
            float len = 6.0f + 2.0f * (float)(h % 4u);
            float dx = ((float)((h >> 8) % 3u) - 1.0f) * 4.0f;
            float y = screen_y(view, wy);
            bbc_line(r, BBC_BLUE, x, y, x + dx, y + len);
            x += dx;
            wy -= len;
        }
    }

    /* Covers down the middle of the lanes, where the wheels miss them. */
    const float cover_span = 250.0f;
    float m_first = floorf(view->camera_y / cover_span) - 1.0f;
    float m_last = ceilf((view->camera_y + height) / cover_span) + 1.0f;
    for (float index = m_first; index <= m_last; index += 1.0f)
    {
        unsigned seed = hash_u(salt(index) * 0x7feb352du + 0x6du);
        if (seed & 1u)
            continue;
        int lane = (int)((seed >> 1) % (unsigned)CHASE_LANE_COUNT);
        float wy = index * cover_span + 20.0f +
                   2.0f * floorf(spread(seed >> 9, cover_span - 40.0f) * 0.5f);
        if (inside_junction(chase, wy, 30.0f))
            continue;
        float cx = road_left + ((float)lane + 0.5f) * CHASE_LANE_WIDTH;
        draw_manhole(r, cx, screen_y(view, wy));
    }

    /* Standing water: along the gutters, and in the lanes where the road has
     * sagged under the traffic. */
    const float puddle_span = 170.0f;
    float p_first = floorf(view->camera_y / puddle_span) - 1.0f;
    float p_last = ceilf((view->camera_y + height) / puddle_span) + 1.0f;
    for (float index = p_first; index <= p_last; index += 1.0f)
    {
        unsigned seed = hash_u(salt(index) * 0x2c1b3c6du + 0x1fu);
        if (seed % 5u >= 2u)
            continue;
        float wy = index * puddle_span +
                   2.0f * floorf(spread(seed >> 4, puddle_span) * 0.5f);
        if (inside_junction(chase, wy, 40.0f))
            continue;
        unsigned where = (seed >> 12) % 4u;
        float cx;
        float w;
        float h;
        if (where < 2u)
        {
            w = 12.0f;
            h = 30.0f + 2.0f * spread(seed >> 19, 20.0f);
            cx = where == 0u ? road_left + 12.0f : road_right - 12.0f;
        }
        else
        {
            int lane = (int)((seed >> 15) % (unsigned)CHASE_LANE_COUNT);
            w = 16.0f + 4.0f * spread(seed >> 24, 3.0f);
            h = 18.0f + 2.0f * spread(seed >> 19, 15.0f);
            cx = road_left + ((float)lane + 0.5f) * CHASE_LANE_WIDTH +
                 (((seed >> 18) & 1u) ? 24.0f : -24.0f);
        }
        /* How near the nearest lamp is, for the reflection. The lamps stand
         * every `CHASE_LAMP_SPAN` along both kerbs, their heads out over the
         * kerb lane, and none inside a junction. */
        float lamp = 0.0f;
        float lamp_dx = 0.0f;
        float lamp_wy = nearest_lamp_y(wy - h * 0.5f);
        if (!inside_junction(chase, lamp_wy, 20.0f))
        {
            for (int side = 0; side < 2; ++side)
            {
                float hx = side == 0 ? CHASE_LAMP_REACH - 7.0f
                                     : CHASE_ROAD_WIDTH - CHASE_LAMP_REACH + 7.0f;
                float dx = hx - (cx - road_left);
                float dy = lamp_wy - (wy - h * 0.5f);
                float near = 1.0f - sqrtf(dx * dx + dy * dy) / 70.0f;
                if (near > lamp)
                {
                    lamp = near;
                    lamp_dx = dx;
                }
            }
        }
        draw_puddle(r, view, cx, screen_y(view, wy), w, h, seed, chase->time,
                    lamp, lamp_dx);
    }

    /* The rain on the open road: a drop landing and a ring that opens and is
     * gone. A handful at a time, never in step. */
    const float splash_band = 40.0f;
    float s_first = floorf(view->camera_y / splash_band) - 1.0f;
    float s_last = ceilf((view->camera_y + height) / splash_band) + 1.0f;
    for (float index = s_first; index <= s_last; index += 1.0f)
    {
        unsigned band = hash_u(salt(index) * 0x9e3779b1u + 0x3bu);
        for (unsigned k = 0; k < 2u; ++k)
        {
            float phase = chase->time * 1.3f +
                          (float)((band >> (k * 7u)) % 97u) / 97.0f;
            float cycle = floorf(phase);
            float life = (phase - cycle) * 3.0f;
            if (life >= 1.0f)
                continue;
            unsigned drop = hash_u(band + salt(cycle) * 0x85ebca6bu + k * 131u);
            float x = road_left + 8.0f + spread(drop, CHASE_ROAD_WIDTH - 16.0f);
            float y = screen_y(view, index * splash_band +
                                         spread(drop >> 12, splash_band));
            draw_ripple(r, x, y, life);
        }
    }

    /* Gratings in the gutter, at the kerb's foot where the water runs. */
    const float drain_span = 180.0f;
    float d_first = floorf(view->camera_y / drain_span) - 1.0f;
    float d_last = ceilf((view->camera_y + height) / drain_span) + 1.0f;
    for (int side = 0; side < 2; ++side)
    {
        for (float index = d_first; index <= d_last; index += 1.0f)
        {
            unsigned seed = hash_u(salt(index) * 0x68e31da4u +
                                   (unsigned)side * 911u);
            if (seed % 3u == 0u)
                continue;
            float wy = index * drain_span +
                       2.0f * floorf(spread(seed >> 3, drain_span) * 0.5f);
            if (inside_junction(chase, wy, 24.0f))
                continue;
            draw_drain(r, side == 0 ? road_left : road_right - 8.0f,
                       screen_y(view, wy), 8.0f);
        }
    }

    /* Lane paint: white dashes between same-direction lanes and a double
     * yellow line down the middle. */
    const float dash_span = 96.0f;
    float dash_first = floorf(view->camera_y / dash_span) - 1.0f;
    float dash_last = ceilf((view->camera_y + height) / dash_span);
    for (int lane = 1; lane < CHASE_LANE_COUNT; ++lane)
    {
        if (lane == CHASE_FIRST_FORWARD_LANE)
            continue;
        float boundary = (float)lane * CHASE_LANE_WIDTH;
        /* One pixel wide, set on whichever side of the boundary keeps the two
         * dashed lines mirror images of each other about the centre. */
        float x = road_left + (boundary < CHASE_ROAD_WIDTH * 0.5f
                                   ? boundary - 4.0f
                                   : boundary);
        for (float index = dash_first; index <= dash_last; index += 1.0f)
        {
            float world_y = index * dash_span;
            if (inside_junction(chase, world_y, 0.0f))
                continue;
            bbc_fill(r, BBC_WHITE, x, screen_y(view, world_y + 52.0f), 4.0f,
                     52.0f);
        }
    }
    float center = road_left + (float)CHASE_FIRST_FORWARD_LANE * CHASE_LANE_WIDTH;
    bbc_fill(r, BBC_YELLOW, center - 8.0f, top, 4.0f, height);
    bbc_fill(r, BBC_YELLOW, center + 4.0f, top, 4.0f, height);

    /*
     * Kerbs, as stones rather than as a line: a white edge with a joint every
     * stone, and the gutter at their foot left black.
     */
    bbc_fill(r, BBC_WHITE, road_left - 4.0f, top, 4.0f, height);
    bbc_fill(r, BBC_WHITE, road_right, top, 4.0f, height);
    const float stone = 30.0f;
    float st_first = floorf(view->camera_y / stone) - 1.0f;
    float st_last = ceilf((view->camera_y + height) / stone) + 1.0f;
    for (float index = st_first; index <= st_last; index += 1.0f)
    {
        float y = screen_y(view, index * stone);
        bbc_fill(r, BBC_BLACK, road_left - 4.0f, y, 4.0f, 2.0f);
        bbc_fill(r, BBC_BLACK, road_right, y, 4.0f, 2.0f);
    }
}

/*
 * A signal head on its mast, seen from above and a little to the front, which
 * is the only way a player can read which lamp is lit: a blue backplate and
 * three lenses, red over amber over green, the lit one in its colour and the
 * other two dark. The amber is the cycle's own lamp and the drive never shows
 * it — because three lenses read as a traffic light where two read as a status
 * chip. What the lit lamp throws on the wet ground under it is part of the
 * signal too, as a scatter of its colour: a colour on the road is visible out
 * of the corner of the eye when the head itself is not.
 */
static void draw_traffic_signal(SDL_Renderer *r, float x, float y,
                                bool cross_green, StreetLayer layer)
{
    BbcColour on = cross_green ? BBC_RED : BBC_GREEN;
    float left = bbc_snap_x(x - 8.0f);
    float top = bbc_snap_y(y - 14.0f);
    if (layer == STREET_GROUND)
    {
        bbc_stipple(r, on, BBC_DITHER_SPARSE, left - 4.0f, top + 30.0f, 24.0f,
                    14.0f);
        return;
    }
    /* A black head lit along its top and its moon side, like everything that
     * stands up out of this street; the dark lenses are blue, the machine's
     * dark. */
    bbc_fill(r, BBC_BLACK, left, top, 16.0f, 28.0f);
    bbc_fill(r, BBC_WHITE, left, top, 16.0f, 2.0f);
    bbc_fill(r, BBC_WHITE, left, top, 4.0f, 28.0f);
    const BbcColour lamps[3] = {cross_green ? BBC_RED : BBC_BLUE, BBC_BLUE,
                                cross_green ? BBC_BLUE : BBC_GREEN};
    for (int i = 0; i < 3; ++i)
        bbc_fill(r, lamps[i], left + 8.0f, top + 4.0f + (float)i * 8.0f, 4.0f,
                 6.0f);
}

/*
 * A squad car holding the side street at a junction.
 *
 * This is the cover story made visible. The demand went out at 00:04 and put
 * the whole city's night shift on a ring around one building; the drive in
 * runs through that ring from the outside, so the junctions fill up with cars
 * facing the wrong way while Chuck goes past them in the one direction nobody
 * is watching. He cannot stop and he cannot be helped by them, which is the
 * point of drawing them at all.
 *
 * It stands in the cross street beyond the pavement, never in a lane: nothing
 * here is part of the simulation, and a car in a lane that the player's own
 * car drives straight through would be a bug rather than a detail. Parked
 * nose-in to the main road, because that is how a road gets closed.
 *
 * The bar is the hardware's flashing colours — one half blue while the other is
 * red, swapping at the flash rate — so every bar in the city flashes in step,
 * which is what a BBC's bars did, and every one of them holds still when the
 * player has asked for steady lights. What it throws on the road it stands on
 * flashes with it, as a scatter of the same two colours: a beacon that lights
 * nothing is a sticker on a roof.
 */
static void draw_cordon_car(SDL_Renderer *r, float cx, float cy, int facing)
{
    BbcColour near_half = bbc_flash_pair(BBC_BLUE, BBC_RED);
    BbcColour far_half = bbc_flash_pair(BBC_RED, BBC_BLUE);
    pattern_ellipse(r, BBC_BLACK, near_half, BBC_DITHER_DOTS, cx, cy, 76.0f,
                    46.0f, false);
    pattern_ellipse(r, BBC_BLACK, near_half, BBC_DITHER_SPARSE, cx, cy, 48.0f,
                    28.0f, false);
    BbcColour paint[BBC_SPRITE_SLOTS] = {BBC_WHITE, BBC_WHITE, near_half,
                                         far_half};
    CarPlace place = place_car(&SPR_CAR_SQUAD, cx, cy,
                               facing < 0 ? NOSE_LEFT : NOSE_RIGHT);
    draw_car_at(r, &SPR_CAR_SQUAD, &place, paint, false, BBC_BLACK);
    /* The push bar across the nose. */
    float bar_x = facing < 0 ? place.x - 4.0f : place.x + place.w;
    bbc_fill(r, BBC_BLUE, bar_x, place.y + 8.0f, 4.0f, place.h - 16.0f);
}

/* A chevron pointing the way the pursuit drives, laid in steps the way road
 * paint is sprayed through a stencil. */
static void draw_chevron(SDL_Renderer *r, float cx, float y)
{
    for (int step = 0; step < 6; ++step)
    {
        float out = 4.0f + (float)step * 8.0f;
        float sy = y + (float)step * 4.0f;
        bbc_fill(r, BBC_YELLOW, cx - out - 8.0f, sy, 8.0f, 4.0f);
        bbc_fill(r, BBC_YELLOW, cx + out, sy, 8.0f, 4.0f);
    }
}

static void render_junctions(SDL_Renderer *r, const ChaseView *view,
                             const Chase *chase, int win_w)
{
    float road_left = road_left_x(view);
    float road_right = road_right_x(view);
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        float top = bbc_snap_y(screen_y(view, junction->y + CHASE_JUNCTION_HALF));
        float height = CHASE_JUNCTION_HALF * 2.0f;
        if (top > view->view_top + view->view_h || top + height < view->view_top)
            continue;
        unsigned id = hash_u(salt(junction->y * 0.5f) * 2246822519u + 0x4au);

        /* The cross street runs edge to edge, over blocks and lane paint. */
        bbc_fill(r, BBC_BLACK, -24.0f, top, (float)win_w + 48.0f, height);
        for (unsigned k = 0; k < 40u; ++k)
        {
            unsigned g = hash_u(id + k * 0x9e3779b9u);
            bbc_plot(r, BBC_BLUE, spread(g, (float)win_w),
                     top + 8.0f + spread(g >> 12, height - 16.0f));
        }
        float middle = bbc_snap_y(screen_y(view, junction->y));

        /* Kerbs along both edges of the cross street, off the main road. */
        for (int side = 0; side < 2; ++side)
        {
            float x0 = side == 0 ? -24.0f : road_right;
            float w = side == 0 ? road_left + 24.0f
                                : (float)win_w + 24.0f - road_right;
            bbc_fill(r, BBC_WHITE, x0, top, w, 2.0f);
            bbc_fill(r, BBC_WHITE, x0, top + height - 2.0f, w, 2.0f);
        }

        /* Cross-street lane paint, interrupted where our road passes through,
         * and laid from the road's own edge so it shakes with the road. */
        float dash_x0 = road_left - 64.0f * ceilf((road_left + 24.0f) / 64.0f);
        for (float x = dash_x0; x < (float)win_w + 16.0f; x += 64.0f)
        {
            if (x + 32.0f > road_left - 8.0f && x < road_right + 8.0f)
                continue;
            bbc_fill(r, BBC_YELLOW, x + 16.0f, middle - 2.0f, 32.0f, 2.0f);
        }

        /* Zebra crossings either side of the junction, where the pursuit is
         * crossing traffic: white bars on the black, the way they are painted. */
        for (int side = 0; side < 2; ++side)
        {
            float stripe_y =
                side == 0 ? top + 8.0f : top + height - 24.0f;
            for (int stripe = 0; stripe < 9; ++stripe)
            {
                float sx = road_left + 8.0f + (float)stripe * 52.0f;
                bbc_fill(r, BBC_WHITE, sx, stripe_y, 24.0f, 16.0f);
                unsigned wear =
                    hash_u(id + (unsigned)(stripe + side * 9) * 0x9e3779b9u);
                for (unsigned c = 0; c < 3u; ++c)
                {
                    unsigned h = hash_u(wear + c * 0x85ebca6bu);
                    bbc_plot(r, BBC_BLACK, sx + spread(h, 24.0f),
                             stripe_y + spread(h >> 10, 16.0f));
                }
            }
        }

        /* Stop line and hazard chevrons on the approach side: the junction has
         * to announce itself before it scrolls into view, or crossing it is a
         * coin toss. */
        float stop_line =
            bbc_snap_y(screen_y(view, junction->y - CHASE_JUNCTION_HALF - 8.0f));
        bbc_fill(r, BBC_WHITE, road_left + 8.0f, stop_line,
                 CHASE_ROAD_WIDTH - 16.0f, 4.0f);
        for (int chevron = 0; chevron < 3; ++chevron)
        {
            float ahead = CHASE_JUNCTION_HALF + 70.0f + (float)chevron * 46.0f;
            float y = bbc_snap_y(screen_y(view, junction->y - ahead));
            draw_chevron(r, road_left + CHASE_ROAD_WIDTH * 0.5f, y - 8.0f);
        }


        if (junction->cordon_side != 0)
        {
            float edge = junction->cordon_side < 0
                             ? road_left - CHASE_PAVEMENT_WIDTH
                             : road_right + CHASE_PAVEMENT_WIDTH;
            float cx = edge + (float)junction->cordon_side *
                                  (CHASE_CORDON_KERB_INSET +
                                   CHASE_CAR_LENGTH * 0.5f);
            draw_cordon_car(r, cx, middle, -junction->cordon_side);
        }
    }
}

/* The four signal heads at every junction, on the corners of its pavements. */
static void render_signals(SDL_Renderer *r, const ChaseView *view,
                           const Chase *chase, StreetLayer layer)
{
    float road_left = road_left_x(view);
    float road_right = road_right_x(view);
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        float top = bbc_snap_y(screen_y(view, junction->y + CHASE_JUNCTION_HALF));
        float height = CHASE_JUNCTION_HALF * 2.0f;
        if (top - 40.0f > view->view_top + view->view_h ||
            top + height + 40.0f < view->view_top)
            continue;
        bool cross_green = chase_cross_has_green(junction, chase->time);
        draw_traffic_signal(r, road_left - 14.0f, top - 16.0f, cross_green,
                            layer);
        draw_traffic_signal(r, road_right + 14.0f, top - 16.0f, cross_green,
                            layer);
        draw_traffic_signal(r, road_left - 14.0f, top + height + 16.0f,
                            cross_green, layer);
        draw_traffic_signal(r, road_right + 14.0f, top + height + 16.0f,
                            cross_green, layer);
    }
}

/*
 * The street lamps: a mast on the pavement, an arm out over the kerb lane and
 * a lantern at its end. The ground under it gets two things: the pool the lamp
 * lights, in yellow dither — densest under the head and thinning out, said in
 * dots per row — and, because the road is wet, the lamp itself coming back off
 * the surface as a few bars of yellow, broken up by the rain.
 *
 * The pool and the glints are the `STREET_GROUND` layer and the mast, the arm
 * and the lantern the `STREET_OVERHEAD` one (see `StreetLayer`): the lamp heads
 * are the one thing in this scene that tells you how high anything is.
 */
static void render_streetlights(SDL_Renderer *r, const ChaseView *view,
                                const Chase *chase, StreetLayer layer)
{
    const float span = CHASE_LAMP_SPAN;
    float first = floorf(view->camera_y / span - CHASE_LAMP_PHASE);
    float last = ceilf((view->camera_y + view->view_h) / span);
    float road_left = road_left_x(view);
    float road_right = road_right_x(view);

    for (float index = first; index <= last; index += 1.0f)
    {
        float world_y = (index + CHASE_LAMP_PHASE) * span;
        if (inside_junction(chase, world_y, 20.0f))
            continue;
        float y = screen_y(view, world_y);
        for (int side = 0; side < 2; ++side)
        {
            float x = side == 0 ? road_left - 8.0f : road_right + 8.0f;
            float reach = side == 0 ? CHASE_LAMP_REACH : -CHASE_LAMP_REACH;
            float hx = x + reach;
            float arm_x = fminf(x, hx);
            if (layer == STREET_OVERHEAD)
            {
                /* The mast and its arm, lit along the moon's side. */
                bbc_fill(r, BBC_BLUE, arm_x, y, fabsf(reach), 2.0f);
                bbc_fill(r, BBC_WHITE, arm_x, y - 2.0f, fabsf(reach), 2.0f);
                bbc_disc(r, BBC_WHITE, x, y, 4.0f);
                bbc_plot(r, BBC_BLUE, x, y);

                /* The lantern: a cobra head, its cap over the arm end and the
                 * lit bowl showing at the tip. */
                float tip = side == 0 ? 4.0f : -8.0f;
                bbc_fill(r, BBC_YELLOW, hx - 8.0f, y - 4.0f, 16.0f, 8.0f);
                bbc_fill(r, BBC_WHITE, hx + tip, y - 2.0f, 4.0f, 4.0f);
                bbc_fill(r, BBC_BLUE, side == 0 ? hx - 8.0f : hx + 4.0f,
                         y - 4.0f, 4.0f, 8.0f);
                continue;
            }

            /* The pool stops at the building line: the roofs beside the road
             * are forty storeys above the lamp, and light from a lamp-post
             * lands on the ground and nowhere else. */
            float street_lo = road_left - CHASE_PAVE_W;
            float street_hi = road_right + CHASE_PAVE_W;
            pattern_ellipse_between(r, BBC_BLACK, BBC_YELLOW, BBC_DITHER_DOTS,
                                    hx, y + 4.0f, 64.0f, 44.0f, false,
                                    street_lo, street_hi);
            pattern_ellipse_between(r, BBC_BLACK, BBC_YELLOW,
                                    BBC_DITHER_SPARSE, hx, y + 4.0f, 30.0f,
                                    20.0f, false, street_lo, street_hi);
            /* The reflection broken into bars by the rain landing on it. */
            unsigned lamp = hash_u(salt(index) * 0x9e3779b9u +
                                   (unsigned)side * 613u);
            for (int bar = 0; bar < 4; ++bar)
            {
                float drift = 0.0f;
                if (!view->steady_lights &&
                    sinf(chase->time * 2.1f + (float)bar * 1.9f +
                         (float)(lamp % 13u)) > 0.4f)
                    drift = 4.0f;
                float half = 12.0f - (float)bar * 2.0f;
                bbc_fill(r, bar == 0 ? BBC_WHITE : BBC_YELLOW,
                         bbc_snap_x(hx - half) + drift,
                         y + 14.0f + (float)bar * 4.0f, bbc_snap_x(half * 2.0f),
                         2.0f);
            }
        }
    }
}

/*
 * The destination, seen from above as the drive arrives at it: a forecourt,
 * and the roof of Kessler Tower filling the top of the frame.
 *
 * It is built out of the same vocabulary as every other roof in this scene —
 * a parapet, a membrane, hardware throwing its shadow down and to the right —
 * because a destination drawn in a different hand from forty roofs drawn in
 * this one reads as a placeholder rather than as the place the night is going.
 * What makes it the place is the colour the game keeps for the story: the
 * tower's coping is magenta, the one roof on the drive that is. Two lights of
 * its own: the lit entrance canopy on the kerb side the SUV pulls up to, and
 * the helipad the rescue ends on, ringed with lamps.
 */
static void render_destination(SDL_Renderer *r, const ChaseView *view,
                               const Chase *chase, int win_w)
{
    if (chase->building_y <= 0.0f)
        return;
    float face = bbc_snap_y(screen_y(view, chase->building_y));
    if (face > view->view_top + view->view_h)
        return;
    const float W = (float)win_w;
    float road_right = road_right_x(view);

    /* Forecourt paving, the pavement's own flags carried across, with parking
     * bays. */
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, -24.0f, face,
               W + 48.0f, 28.0f);
    for (float fx0 = 0.0f; fx0 < W; fx0 += 48.0f)
        bbc_fill(r, BBC_BLACK, fx0, face, 4.0f, 28.0f);
    bbc_fill(r, BBC_BLACK, -24.0f, face + 14.0f, W + 48.0f, 2.0f);
    for (int bay = 0; bay < 5; ++bay)
    {
        float bx = 60.0f + (float)bay * 72.0f;
        bbc_fill(r, BBC_WHITE, bx, face + 4.0f, 4.0f, 20.0f);
        /* A wheel stop at the head of the bay. */
        if (bay < 4)
        {
            bbc_fill(r, BBC_BLACK, bx + 24.0f, face + 4.0f, 24.0f, 4.0f);
            bbc_fill(r, BBC_WHITE, bx + 28.0f, face + 4.0f, 16.0f, 2.0f);
        }
    }
    /* The forecourt kerb, white like every kerb on the drive. */
    bbc_fill(r, BBC_WHITE, -24.0f, face + 28.0f, W + 48.0f, 2.0f);
    bbc_fill(r, BBC_BLACK, -24.0f, face + 30.0f, W + 48.0f, 2.0f);

    float top = view->view_top - 20.0f;
    float depth = face - top;
    bbc_fill(r, BBC_BLACK, -24.0f, top, W + 48.0f, depth);

    /* The roof: a parapet ring, the membrane in it, the coping in magenta. */
    const float rx0 = 24.0f;
    const float rx1 = W - 24.0f;
    const float lip = 8.0f;
    float my1 = face - 4.0f - lip;
    bbc_fill(r, BBC_BLUE, rx0, top, rx1 - rx0, depth - 4.0f);
    bbc_frame(r, BBC_BLACK, rx0 + lip - 4.0f, top - 2.0f,
              rx1 - rx0 - lip * 2.0f + 8.0f, my1 - top + 4.0f);
    for (float seam = my1 - 26.0f; seam > top; seam -= 26.0f)
        bbc_stipple(r, BBC_BLACK, BBC_DITHER_COLS, rx0 + lip, seam,
                    rx1 - rx0 - lip * 2.0f, 2.0f);
    bbc_fill(r, BBC_MAGENTA, rx0, top, 4.0f, depth - 4.0f);
    bbc_fill(r, BBC_MAGENTA, rx1 - 4.0f, top, 4.0f, depth - 4.0f);
    bbc_fill(r, BBC_MAGENTA, rx0, face - 6.0f, rx1 - rx0, 2.0f);
    /* The parapet's shadow falls in off the moon's side. */
    bbc_fill(r, BBC_BLACK, rx0 + lip, top, 4.0f, my1 - top);

    /* Plant: a row of condensers, a stair head and a vent cluster, out of
     * the same fittings every roof on the drive is built from. */
    struct
    {
        RoofFitting fitting;
        float x;
        float y;
    } plant[] = {
        {ROOF_CONDENSER, 56.0f, 62.0f},  {ROOF_CONDENSER, 88.0f, 62.0f},
        {ROOF_CONDENSER, 120.0f, 62.0f}, {ROOF_VENTS, 64.0f, 104.0f},
        {ROOF_BULKHEAD, W - 168.0f, 64.0f}, {ROOF_TANK, W - 104.0f, 120.0f},
        {ROOF_SKYLIGHT, W - 120.0f, 52.0f}, {ROOF_DISH, 168.0f, 40.0f}};
    const int plant_count = (int)(sizeof(plant) / sizeof(plant[0]));
    for (int i = 0; i < plant_count; ++i)
        draw_roof_fitting(r, plant[i].fitting, plant[i].x,
                          face - plant[i].y, 0x2d1u * (unsigned)(i + 1));

    /* The helipad: a raised deck, the ring and the H painted on it in white,
     * and the perimeter lamps a pilot finds it by. */
    float pad_cx = W * 0.5f;
    float pad_cy = face - 64.0f;
    float pad_hw = 52.0f;
    float pad_hh = 38.0f;
    bbc_fill(r, BBC_BLACK, pad_cx - pad_hw + 4.0f, pad_cy - pad_hh + 4.0f,
             pad_hw * 2.0f, pad_hh * 2.0f);
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_SPARSE, pad_cx - pad_hw,
               pad_cy - pad_hh, pad_hw * 2.0f, pad_hh * 2.0f);
    bbc_frame(r, BBC_WHITE, pad_cx - pad_hw, pad_cy - pad_hh, pad_hw * 2.0f,
              pad_hh * 2.0f);
    bbc_ring(r, BBC_WHITE, pad_cx, pad_cy, 31.0f);
    bbc_fill(r, BBC_WHITE, pad_cx - 16.0f, pad_cy - 16.0f, 8.0f, 32.0f);
    bbc_fill(r, BBC_WHITE, pad_cx + 8.0f, pad_cy - 16.0f, 8.0f, 32.0f);
    bbc_fill(r, BBC_WHITE, pad_cx - 8.0f, pad_cy - 2.0f, 16.0f, 4.0f);
    for (int i = 0; i < 12; ++i)
    {
        float t = (float)(i % 4) / 3.0f;
        float lx;
        float ly;
        if (i < 4)
        {
            lx = pad_cx - pad_hw + 4.0f + t * (pad_hw * 2.0f - 12.0f);
            ly = pad_cy - pad_hh + 4.0f;
        }
        else if (i < 8)
        {
            lx = pad_cx - pad_hw + 4.0f + t * (pad_hw * 2.0f - 12.0f);
            ly = pad_cy + pad_hh - 6.0f;
        }
        else
        {
            lx = i < 10 ? pad_cx - pad_hw + 4.0f : pad_cx + pad_hw - 8.0f;
            ly = pad_cy + (i % 2 == 0 ? -12.0f : 10.0f);
        }
        bbc_plot(r, BBC_YELLOW, lx, ly);
    }

    /* The aviation mast at the corner, its red lamp on the flash. */
    float mast_x = W - 56.0f;
    float mast_y = face - 34.0f;
    bbc_fill(r, BBC_BLACK, mast_x + 4.0f, mast_y + 2.0f, 16.0f, 2.0f);
    bbc_disc(r, BBC_WHITE, mast_x, mast_y, 4.0f);
    bbc_plot(r, bbc_flash_pair(BBC_RED, BBC_BLUE), mast_x, mast_y);

    /*
     * The entrance canopy, on the kerb side the SUV pulled up to: a glazed
     * slab standing out from the face over the forecourt, lit from under, so
     * the lobby's light shows through its panes and pools on the paving round
     * it.
     */
    float door_x = road_right - 116.0f;
    float canopy_x = door_x - 8.0f;
    float canopy_w = 116.0f;
    pattern_ellipse(r, BBC_BLACK, BBC_YELLOW, BBC_DITHER_DOTS,
                    canopy_x + canopy_w * 0.5f, face + 18.0f, 110.0f, 30.0f,
                    false);
    pattern_ellipse(r, BBC_BLACK, BBC_YELLOW, BBC_DITHER_SPARSE,
                    canopy_x + canopy_w * 0.5f, face + 18.0f, 64.0f, 16.0f,
                    false);
    bbc_fill(r, BBC_BLACK, canopy_x + 4.0f, face - 2.0f, canopy_w, 26.0f);
    bbc_fill(r, BBC_BLUE, canopy_x, face - 8.0f, canopy_w, 28.0f);
    bbc_fill(r, BBC_WHITE, canopy_x, face - 8.0f, canopy_w, 2.0f);
    for (int pane = 0; pane < 5; ++pane)
    {
        float px = canopy_x + 4.0f + (float)pane * 24.0f;
        bbc_dither(r, BBC_YELLOW, BBC_WHITE, BBC_DITHER_SPARSE, px, face - 4.0f,
                   16.0f, 18.0f);
        bbc_fill(r, BBC_WHITE, px, face - 4.0f, 4.0f, 8.0f);
    }
    bbc_fill(r, BBC_YELLOW, canopy_x, face + 18.0f, canopy_w, 2.0f);
    /* Bollards along the front of it, each with the canopy's light on its cap
     * and its own shadow away from the moon. */
    for (float bx = canopy_x - 4.0f; bx < canopy_x + canopy_w + 8.0f; bx += 20.0f)
    {
        bbc_plot(r, BBC_BLACK, bx + 4.0f, face + 24.0f);
        bbc_plot(r, BBC_WHITE, bx, face + 22.0f);
    }
}

/* At speed the rain races past: short white lines with a blue tail, falling
 * down the road at the car's own pace. */
static void render_speed_streaks(SDL_Renderer *r, const ChaseView *view,
                                 const Chase *chase)
{
    float fast = clamp01((chase->player.speed - CHASE_CRUISE_SPEED * 0.6f) /
                         (CHASE_MAX_SPEED - CHASE_CRUISE_SPEED * 0.6f));
    if (fast <= 0.02f)
        return;

    int count = 6 + (int)(fast * 14.0f);
    for (int i = 0; i < count; ++i)
    {
        unsigned h = hash_u((unsigned)i * 374761393u);
        float x = road_left_x(view) + bbc_snap_x(spread(h, CHASE_ROAD_WIDTH));
        float phase = fmodf(chase->time * chase->player.speed * 1.6f +
                                (float)((h >> 8) % 700u),
                            view->view_h + 120.0f);
        float y = view->view_top + phase - 60.0f;
        float len = 20.0f + fast * 30.0f;
        bbc_fill(r, BBC_BLUE, x, y, 4.0f, len);
        bbc_fill(r, BBC_WHITE, x, y + len - 8.0f, 4.0f, 8.0f);
    }
}

/* ---- Overlays and HUD ------------------------------------------------ */

/*
 * The dashboard is a BBC game's status strip: black, ruled off under a yellow
 * line, with the readouts in wells framed in blue. Labels are cyan, values
 * white or yellow, and the one figure that matters — the gap — goes from cyan
 * to yellow to red as the SUV pulls away, and flashes when it is nearly gone.
 */
static void draw_hud_well(SDL_Renderer *r, float x, float y, float w, float h)
{
    bbc_fill(r, BBC_BLACK, x, y, w, h);
    bbc_frame(r, BBC_BLUE, x, y, w, h);
}

/* A groove between two groups of readouts. */
static void draw_hud_groove(SDL_Renderer *r, float x)
{
    bbc_fill(r, BBC_BLUE, x, 6.0f, 4.0f, 26.0f);
}

static void render_hud(SDL_Renderer *r, const Chase *chase, int win_w,
                       const PadHints *pad)
{
    bbc_set_pattern_origin(0.0f, 0.0f);
    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, (float)win_w, (float)HUD_HEIGHT);
    bbc_fill(r, BBC_YELLOW, 0.0f, (float)HUD_HEIGHT - 2.0f, (float)win_w, 2.0f);
    bbc_fill(r, BBC_RED, 0.0f, 0.0f, 4.0f, (float)HUD_HEIGHT - 2.0f);

    draw_hud_groove(r, 184.0f);
    draw_hud_groove(r, 280.0f);
    draw_hud_groove(r, 504.0f);
    draw_hud_groove(r, 696.0f);
    bbc_text(r, 12.0f, 4.0f, 2.0f, BBC_YELLOW, "PURSUIT");
    bbc_fill(r, BBC_RED, 12.0f, 22.0f, 112.0f, 2.0f);
    /* The readout under the title names the pedals rather than the hardware:
     * "STICK / DPAD DRIVE" told a player holding a pad everything except the
     * one thing they needed, which is which button makes the car go. */
    char pedals[40];
    bbc_text(r, 12.0f, 27.0f, 1.0f, BBC_WHITE,
             pad_hint(pad, pedals, sizeof(pedals), "$A GAS   $B BRAKE",
                      "UP GAS   DOWN BRAKE"));

    bbc_text(r, 196.0f, 8.0f, 1.0f, BBC_CYAN, "CAR");
    for (int i = 0; i < CHASE_INTEGRITY; ++i)
    {
        bool intact = i < chase->player.integrity;
        /* A lost pip is the car gone to blue: still there, so the row reads as
         * three cars, but in the colour of things that are not lit. */
        BbcColour paint[BBC_SPRITE_SLOTS] = {BBC_CYAN, BBC_WHITE, BBC_BLACK,
                                             BBC_BLACK};
        bbc_sprite(r, &SPR_PIP_CAR, 196.0f + (float)i * 28.0f, 22.0f, 1, paint);
        if (!intact)
            bbc_sprite_solid(r, &SPR_PIP_CAR, 196.0f + (float)i * 28.0f, 22.0f,
                             1, BBC_BLUE);
    }

    bbc_text(r, 292.0f, 8.0f, 1.0f, BBC_CYAN, "ROUTE");
    draw_hud_well(r, 292.0f, 20.0f, 204.0f, 12.0f);
    float route = bbc_snap_x(chase_route_progress(chase) * 196.0f);
    if (route > 0.0f)
        bbc_fill(r, BBC_CYAN, 296.0f, 22.0f, route, 8.0f);

    /* The gap meter is the whole game: it fills as the SUV pulls away. */
    float gap = fmaxf(chase_gap(chase), 0.0f);
    float pressure = clamp01(gap / CHASE_LOSE_GAP);
    BbcColour gap_colour = BBC_CYAN;
    if (pressure > 0.85f)
        gap_colour = bbc_flash_pair(BBC_RED, BBC_YELLOW);
    else if (pressure > 0.65f)
        gap_colour = BBC_RED;
    else if (pressure > 0.40f)
        gap_colour = BBC_YELLOW;
    char gap_text[24];
    SDL_snprintf(gap_text, sizeof(gap_text), "%03dM", (int)(gap * 0.1f));
    bbc_text(r, 512.0f, 8.0f, 1.0f, BBC_CYAN, "GAP TO SUV");
    bbc_text(r, 600.0f, 8.0f, 1.0f,
             gap_colour == BBC_CYAN ? BBC_WHITE : gap_colour, gap_text);
    draw_hud_well(r, 512.0f, 20.0f, 176.0f, 12.0f);
    float fill = bbc_snap_x(168.0f * pressure);
    if (fill > 0.0f)
        bbc_fill(r, gap_colour, 516.0f, 22.0f, fill, 8.0f);

    char speed_text[24];
    SDL_snprintf(speed_text, sizeof(speed_text), "%03d",
                 (int)(chase->player.speed * 0.5f));
    bbc_text(r, 708.0f, 8.0f, 1.0f, BBC_CYAN, "SPEED");
    bbc_text(r, 708.0f, 19.0f, 2.0f, BBC_WHITE, speed_text);
    bbc_text(r, 760.0f, 25.0f, 1.0f, BBC_CYAN, "KMH");
}

/*
 * Cross traffic enters from off-screen, so the junction ahead gets an early
 * readout: how far it is and whether the cars crossing it have the green. Red
 * for cross traffic, flashing red and cyan — every alarm on this machine
 * blinked in exactly that pair — and green for a clear road.
 */
static void render_junction_warning(SDL_Renderer *r, const ChaseView *view,
                                    const Chase *chase, int win_w)
{
    if (chase->phase != CHASE_PHASE_PURSUIT)
        return;

    const ChaseIntersection *nearest = NULL;
    float nearest_distance = CHASE_CROSS_ALERT_RANGE;
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        float distance = junction->y - CHASE_JUNCTION_HALF - chase->player.y;
        if (distance < 0.0f || distance > nearest_distance)
            continue;
        nearest_distance = distance;
        nearest = junction;
    }
    if (nearest == NULL)
        return;

    bool cross_green = chase_cross_has_green(nearest, chase->time);
    BbcColour accent = cross_green ? BBC_RED : BBC_GREEN;
    char text[40];
    SDL_snprintf(text, sizeof(text), "JUNCTION %03dM  %s",
                 (int)(nearest_distance * 0.1f),
                 cross_green ? "CROSS TRAFFIC" : "CLEAR");

    float width = bbc_snap_x(text_width(text, 1.0f) + 28.0f);
    float x = bbc_snap_x(((float)win_w - width) * 0.5f);
    float y = view->view_top + 60.0f;
    /* It comes up as the junction closes rather than at the edge of its range,
     * and it comes up whole: red and green are a rung or two from black on the
     * palette ladder, so a plate faded in on it would spend most of its fade as
     * a black box over the road. */
    if (nearest_distance > CHASE_CROSS_ALERT_RANGE * 0.75f)
        return;
    draw_plate(r, x, y, width, 20.0f, accent);
    bbc_fill(r, accent, x, y, 8.0f, 20.0f);
    bbc_text(r, x + 16.0f, y + 6.0f, 1.0f,
             cross_green ? bbc_flash(BBC_RED) : BBC_GREEN, text);
}

/*
 * The pedals, spelled out on the road.
 *
 * The drive is the one beat of the game that is not a platformer, and the
 * platformer never asks for a throttle: told nothing, a player holds a
 * direction and watches the SUV pull away without ever learning that the car
 * had to be driven. So the two pedals are named at the head of every attempt —
 * a crash is exactly when someone needs to read them again — and they fade out
 * once the drive is under way, because a prompt that never leaves is a prompt
 * nobody reads. What stays is the HUD line under PURSUIT, which names the same
 * two buttons for anyone who arrives late.
 */
static void render_control_hint(SDL_Renderer *r, const Chase *chase, int win_w,
                                int win_h, const PadHints *pad)
{
    /* Only over the drive itself: the departure is watched rather than driven,
     * and its own beat already carries a caption and a skip prompt. */
    if (chase->phase != CHASE_PHASE_PURSUIT)
        return;
    float fade = fminf(clamp01(chase->phase_time / 0.4f),
                       clamp01(CHASE_CONTROL_HINT_TIME - chase->phase_time));
    if (fade_steps(fade) >= 3)
        return;

    char buf[64];
    const char *pedals =
        pad_hint(pad, buf, sizeof(buf), "$A ACCELERATE    $B BRAKE",
                 "UP ACCELERATE    DOWN BRAKE");
    const char *steer = pad != NULL ? "STICK OR DPAD STEERS"
                                    : "LEFT AND RIGHT STEER";

    float width = bbc_snap_x(fmaxf(text_width(pedals, 2.0f),
                                   text_width(steer, 1.0f)) +
                             40.0f);
    float x = bbc_snap_x(((float)win_w - width) * 0.5f);
    /* Under the car, not over it: the camera keeps Chuck's bonnet a fixed
     * CHASE_CAMERA_LEAD off the bottom edge, so this band is the one strip of
     * road the player never drives through. */
    float y = (float)win_h - 88.0f;
    float center_x = (float)win_w * 0.5f;
    PaletteHold hold = fade_begin(fade);
    draw_plate(r, x, y, width, 44.0f, BBC_YELLOW);
    bbc_fill(r, BBC_YELLOW, x, y, 8.0f, 44.0f);
    bbc_text_centered(r, center_x, y + 7.0f, 2.0f, BBC_WHITE, pedals);
    bbc_text_centered(r, center_x, y + 29.0f, 1.0f, BBC_CYAN, steer);
    fade_end(&hold);
}

/*
 * One line naming the thing the player is driving through.
 *
 * The squad cars sealing every junction are the most legible object on the
 * road and the only one the drive never explains: without a caption they read
 * as traffic the player is being asked to dodge, which is the opposite of what
 * they are. They are the cordon the 00:04 broadcast bought, standing since
 * before Ellen was taken, and it is the reason nobody answered when she was.
 *
 * It waits for the pedal prompt to clear rather than sharing the frame with
 * it — the controls are what the first seconds are for — and it is set where
 * the cutscenes set theirs, so the drive is captioned in the same hand as the
 * beats either side of it.
 */
static void render_cordon_caption(SDL_Renderer *r, const Chase *chase)
{
    if (chase->phase != CHASE_PHASE_PURSUIT)
        return;
    float since = chase->pursuit_time - (CHASE_CONTROL_HINT_TIME + 1.5f);
    float fade = fminf(clamp01(since / 0.45f), clamp01(4.5f - since));
    if (fade_steps(fade) >= 3)
        return;

    const char *line = "00:12 // THEIR CORDON, NOT YOURS";
    PaletteHold hold = fade_begin(fade);
    bbc_fill(r, BBC_BLACK, 32.0f, 44.0f, text_width(line, 1.0f) + 8.0f, 24.0f);
    bbc_text(r, 36.0f, 48.0f, 1.0f, BBC_CYAN, line);
    bbc_fill(r, BBC_RED, 36.0f, 62.0f, bbc_snap_x(52.0f * fade), 2.0f);
    fade_end(&hold);
}

static void render_overlays(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase, int win_w, int win_h,
                            const PadHints *pad)
{
    float center_x = (float)win_w * 0.5f;
    render_junction_warning(r, view, chase, win_w);
    render_control_hint(r, chase, win_w, win_h, pad);
    render_cordon_caption(r, chase);

    /* Off-screen target: the player still needs to know where the SUV went.
     *
     * Only while the drive is actually being driven. The departure sets its own
     * caption in the same band — the SUV is off the top of the screen for most
     * of it, so both were drawn, and the caption's plate cut the marker in half
     * rather than replacing it. Neither beat needs it: nobody is steering
     * during the departure, and a failed attempt is already telling the player
     * what went wrong. */
    float target_screen_y = screen_y(view, chase->target.y);
    if (target_screen_y < view->view_top - CHASE_SUV_LENGTH * 0.5f &&
        chase->phase != CHASE_PHASE_FAILED &&
        chase->phase != CHASE_PHASE_DEPARTURE)
    {
        BbcColour mark = bbc_flash_pair(BBC_YELLOW, BBC_RED);
        bbc_fill(r, BBC_BLACK, center_x - 40.0f, view->view_top + 6.0f, 80.0f,
                 36.0f);
        bbc_text_centered(r, center_x, view->view_top + 10.0f, 2.0f, mark, "^");
        bbc_text_centered(r, center_x, view->view_top + 30.0f, 1.0f, mark,
                          "SUV AHEAD");
    }

    if (chase->phase == CHASE_PHASE_DEPARTURE)
    {
        const char *caption = "THEY HAVE HER";
        if (chase->phase_time >= CHASE_DEPARTURE_IGNITION)
            caption = "STAY ON THAT SUV";
        else if (chase->phase_time >= CHASE_DEPARTURE_CHUCK_RUN - 0.05f)
            caption = "GET TO THE CAR";
        float fade = clamp01(chase->phase_time / 0.6f);
        if (fade_steps(fade) < 3)
        {
            PaletteHold hold = fade_begin(fade);
            bbc_fill(r, BBC_BLACK, 0.0f, view->view_top + 6.0f, (float)win_w,
                     30.0f);
            bbc_fill(r, BBC_WHITE, 0.0f, view->view_top + 6.0f, (float)win_w,
                     2.0f);
            bbc_fill(r, BBC_WHITE, 0.0f, view->view_top + 34.0f, (float)win_w,
                     2.0f);
            bbc_text_centered(r, center_x, view->view_top + 14.0f, 2.0f,
                              BBC_WHITE, caption);
            fade_end(&hold);
        }
        draw_skip_prompt(r, win_w, win_h, pad, " TO SKIP");
    }

    if (chase->phase == CHASE_PHASE_FAILED)
    {
        const char *headline = chase->failure == CHASE_FAILURE_WRECKED
                                   ? "CAR WRECKED"
                                   : "TRAIL LOST";
        bbc_fill(r, BBC_BLACK, 0.0f, 232.0f, (float)win_w, 76.0f);
        bbc_fill(r, BBC_RED, 0.0f, 232.0f, (float)win_w, 2.0f);
        bbc_fill(r, BBC_RED, 0.0f, 306.0f, (float)win_w, 2.0f);
        bbc_text_centered(r, center_x, 248.0f, 3.0f, BBC_RED, headline);
        bbc_text_centered(r, center_x, 286.0f, 1.0f, BBC_WHITE,
                          "CUTTING THROUGH THE BLOCKS TO GET BACK ON THEM");
    }

    /* After enough failed attempts the drive stops insisting on itself. */
    if (chase->phase == CHASE_PHASE_PURSUIT &&
        chase->attempts >= CHASE_SKIP_AFTER_ATTEMPTS)
    {
        draw_skip_prompt(r, win_w, win_h, pad, " TO SKIP THE DRIVE");
    }

    if (chase->phase == CHASE_PHASE_ARRIVAL &&
        fade_steps(clamp01((chase->phase_time - 1.4f) / 0.5f)) < 3)
    {
        float fade = clamp01((chase->phase_time - 1.4f) / 0.5f);
        PaletteHold hold = fade_begin(fade);
        bbc_fill(r, BBC_BLACK, 0.0f, 340.0f, (float)win_w, 74.0f);
        bbc_fill(r, BBC_CYAN, 0.0f, 340.0f, (float)win_w, 2.0f);
        bbc_fill(r, BBC_CYAN, 0.0f, 412.0f, (float)win_w, 2.0f);
        bbc_text_centered(r, center_x, 356.0f, 2.0f, BBC_CYAN,
                          "THEY STOPPED HERE");
        bbc_text_centered(r, center_x, 392.0f, 1.0f, BBC_WHITE,
                          "THEY WALKED HER INTO KESSLER TOWER");
        fade_end(&hold);
    }
}

void chase_render(SDL_Renderer *r, const Chase *chase, int win_w, int win_h,
                  float shake_x, float shake_y, bool steady_lights,
                  const PadHints *pad)
{
    ChaseView view = chase_view_make(win_w, win_h, chase->camera_y, shake_x,
                                     shake_y, steady_lights);
    /* Every dither on the road is anchored to the road, so it scrolls with
     * the road instead of boiling under it. */
    bbc_set_pattern_origin(view.origin_x, view.origin_y);

    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, (float)win_w, (float)win_h);
    render_blocks(r, &view, win_w);
    render_road(r, &view, chase);
    render_junctions(r, &view, chase, win_w);
    render_streetlights(r, &view, chase, STREET_GROUND);
    render_signals(r, &view, chase, STREET_GROUND);

    /* Every beam goes down before any car, so a car's lamps light the road in
     * front of it and never the car in front of it. */
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        const ChaseCar *car = &chase->cars[i];
        if (car->active)
            draw_traffic_beam(r, &view, car, i);
    }
    {
        CarPlace suv = target_place(&view, chase);
        draw_head_beam(r, &suv, 120.0f, CHASE_SUV_WIDTH * 0.9f, BBC_WHITE);
        if (chase->player.engine_running)
        {
            CarPlace own = player_place(&view, chase);
            draw_head_beam(r, &own, 150.0f, CHASE_CAR_WIDTH * 1.05f,
                           BBC_WHITE);
        }
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < CHASE_MAX_CARS; ++i)
        {
            const ChaseCar *car = &chase->cars[i];
            if (!car->active)
                continue;
            if ((car->wreck_time > 0.0f) != (pass == 0))
                continue;
            draw_traffic_car(r, &view, car, i);
        }
    }
    draw_target_car(r, &view, chase);
    draw_player_car(r, &view, chase);
    /* Chuck is on the pavement with everybody else who is on the ground, so
     * under the lamp arms he runs past. The departure is the only phase he is
     * drawn in, and the building is never in frame then. */
    draw_chuck_on_foot(r, &view, chase);
    render_streetlights(r, &view, chase, STREET_OVERHEAD);
    render_signals(r, &view, chase, STREET_OVERHEAD);
    /* Drawn after the traffic so the building hides whatever has driven past
     * its front and turned off the road — and after the lamps, because the
     * roof of a forty-storey tower is above everything else in the street. */
    render_destination(r, &view, chase, win_w);
    render_speed_streaks(r, &view, chase);

    bbc_set_pattern_origin(0.0f, 0.0f);
    render_overlays(r, &view, chase, win_w, win_h, pad);
    render_hud(r, chase, win_w, pad);
}
