/*
 * Chuck's field manual: the page that says what the building is and what the
 * buttons do.
 *
 * Three decisions carry it.
 *
 * The text is a table, not a routine.  Every sheet is a `ManualPage` — a
 * title, a strap, a list of typed lines and one illustration — so a rule that
 * changes in [game_config.h](game_config.h) is a string edited in one place
 * rather than a paragraph hunted through a draw function.  The line kinds are
 * the whole layout language: a head, a body line, a bullet, a control row and
 * a gap.
 *
 * The sheet is an instruction page of the period.  A BBC game's manual on the
 * screen was black, with a border in a colour, headings in yellow, the body in
 * white and the keys in inverse video, and this is that: a blue frame with red
 * corner marks, a double-height title, and the illustration in a panel with a
 * doubled border of its own because it is the thing on the page that matters.
 *
 * The illustrations are drawn in the game's own pixels rather than as
 * diagrams of it. Every figure on them is the cast's own sprite in the cast's
 * own paint ([bbc_cast.c](bbc_cast.c)), every pickup is the icon the HUD and
 * the floor show, and the floors, ladders, crates and spikes are built from the
 * same eight-colour vocabulary the sectors are, out of the theme paint the
 * sectors read ([level_paint.c](level_paint.c)) — because a manual that teaches
 * a guard in one drawing and puts a different one on the floor is a manual for
 * a different game.
 */
#include "manual.h"

#include <math.h>

#include "bbc.h"
#include "bbc_cast.h"
#include "crew.h"
#include "level_paint.h"
/* For `PROGRESS_NO_TIME`, which is the difference between a sector nobody has
 * finished and one finished in no time at all. The manual reads the constant and
 * never the file. */
#include "progress.h"
#include "run_tally.h"
#include "manual_pages.h"

/* ---- Layout ----------------------------------------------------------- */

#define SHEET_INSET 20.0f
#define SHEET_TOP 18.0f
#define SHEET_BOTTOM 18.0f

/* The text column's own geometry belongs to [manual_pages.h](manual_pages.h),
 * where the suite can measure the sheets against it: two copies of a pitch is
 * how a page comes to be certified at a height the frame then clips. Named
 * locally so the draw code below still reads as layout. */
#define TEXT_X MANUAL_TEXT_X
#define TEXT_RIGHT MANUAL_TEXT_RIGHT
#define BODY_Y MANUAL_BODY_Y
#define BODY_BOTTOM MANUAL_BODY_BOTTOM
#define BULLET_INDENT MANUAL_BULLET_INDENT
#define LINE_PITCH MANUAL_LINE_PITCH
#define KEY_PITCH MANUAL_KEY_PITCH
#define HEAD_PITCH MANUAL_HEAD_PITCH
#define HEAD_LEAD MANUAL_HEAD_LEAD
#define GAP_PITCH MANUAL_GAP_PITCH
#define CAPTION_MAX MANUAL_CAPTION_MAX

/*
 * The illustration's own plate, in BBC pixels: 76 columns by 159 rows from
 * (448, 106). Every coordinate inside an illustration is a column and a row of
 * it, so nothing on a picture lands on half a BBC pixel and gets rounded to
 * whichever side the arithmetic happened to fall.
 *
 * It is the width the old plate's 314-pixel frame left inside its bezel give
 * or take a column, and it has to stay at least that: `RUN_TALLY_RECORD_LINE_W`
 * is the record card's line, which is this width less the card's inset and a
 * text margin either side, and the suite measures every record line against
 * it. The doubled border is drawn outside the plate — blue, then cyan — and
 * the caption hangs under the inner one.
 */
#define ART_X 448.0f
#define ART_Y 106.0f
#define ART_W 304.0f
#define ART_H 318.0f
#define CAPTION_X 444.0f
#define CAPTION_Y 438.0f

_Static_assert((int)ART_W - 16 - 24 >= (int)RUN_TALLY_RECORD_LINE_W,
               "the record card has to leave a record line its full width");
_Static_assert(CAPTION_MAX * BBC_FONT_CELL <= (int)ART_W + 8,
               "a caption is as wide as the plate it hangs under");

#define CH ((float)BBC_FONT_CELL)
/* A chip's height belongs to [manual_pages.h](manual_pages.h) with the rest of
 * the geometry, because the fit check has to know it: a keycap is the deepest
 * ink a control row lays down, and a copy of the number here is how a sheet
 * comes to be certified with its last row in the footer. */
#define CHIP_H MANUAL_CHIP_H
#define KEY_CHIP_RISE MANUAL_KEY_CHIP_RISE
#define HEAD_RULE_Y MANUAL_HEAD_RULE_Y

/* The page module has no SDL under it and so cannot ask the renderer how wide
 * a glyph is. It is the same 8x8 cell either way, and this is what says so. */
_Static_assert(BBC_FONT_CELL == (int)MANUAL_CH,
               "the manual's fit checks measure a different glyph than the "
               "renderer draws");

/* A BBC column or row, as the logical coordinate its top-left corner sits on. */
#define COL(c) ((float)(c) * (float)BBC_PX_W)
#define ROW(r) ((float)(r) * (float)BBC_PX_H)

/* ---- Tiny drawing helpers -------------------------------------------- */

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static float smoothstep01(float value)
{
    float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

static unsigned hash_u(unsigned value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

static float text_width(const char *text)
{
    return CH * (float)SDL_strlen(text);
}

/* Letterspaced type, for the few lines that have to read as a label rather
 * than as prose. */
static void draw_tracked(SDL_Renderer *r, float x, float y, float track,
                         BbcColour colour, const char *text)
{
    char glyph[2] = {0, 0};
    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        glyph[0] = text[i];
        bbc_text(r, x + (CH + track) * (float)i, y, 1.0f, colour, glyph);
    }
}

/*
 * An illustration's plate as a grid of BBC pixels. Every drawing below is
 * written in its columns and rows, and every helper here takes them, so a
 * picture is designed on squared paper the way the cast was.
 */
typedef struct
{
    int col, row;   /* top-left, in frame columns and rows */
    int cols, rows; /* how many of each the plate has */
} Plate;

static Plate plate_of(SDL_FRect p)
{
    Plate pl = {(int)floorf(p.x / (float)BBC_PX_W),
                (int)floorf(p.y / (float)BBC_PX_H),
                (int)(p.w / (float)BBC_PX_W), (int)(p.h / (float)BBC_PX_H)};
    return pl;
}

/* Logical coordinates of a plate column and row, for text and sprites. */
static float px_of(const Plate *pl, float col)
{
    return COL(pl->col) + col * (float)BBC_PX_W;
}

static float py_of(const Plate *pl, float row)
{
    return ROW(pl->row) + row * (float)BBC_PX_H;
}

static void blk(SDL_Renderer *r, const Plate *pl, BbcColour c, int col,
                int row, int w, int h)
{
    bbc_block(r, c, COL(pl->col + col), ROW(pl->row + row), w, h);
}

static void dith(SDL_Renderer *r, const Plate *pl, BbcColour base,
                 BbcColour ink, BbcDither pattern, int col, int row, int w,
                 int h)
{
    if (w < 1 || h < 1)
        return;
    bbc_dither(r, base, ink, pattern, COL(pl->col + col), ROW(pl->row + row),
               COL(w), ROW(h));
}

static void stip(SDL_Renderer *r, const Plate *pl, BbcColour ink,
                 BbcDither pattern, int col, int row, int w, int h)
{
    if (w < 1 || h < 1)
        return;
    bbc_stipple(r, ink, pattern, COL(pl->col + col), ROW(pl->row + row),
                COL(w), ROW(h));
}

/* Text set from a plate's top-left, `dx` and `dy` in logical pixels: type is
 * the one thing on the plate that is not on the BBC grid. */
static void ptext(SDL_Renderer *r, const Plate *pl, float dx, float dy,
                  BbcColour c, const char *text)
{
    bbc_text(r, COL(pl->col) + dx, ROW(pl->row) + dy, 1.0f, c, text);
}

/* A sprite stood with its feet on plate row `foot` and centred on column
 * `cx`, as a figure is stood on a floor. */
static void figure(SDL_Renderer *r, const Plate *pl, const BbcSprite *sprite,
                   int cx, int foot, int dir, const BbcColour *paint)
{
    int w = bbc_sprite_w(sprite);
    bbc_sprite(r, sprite, COL(pl->col + cx - w / 2),
               ROW(pl->row + foot - sprite->h), dir, paint);
}

static void dash_row(SDL_Renderer *r, const Plate *pl, BbcColour c, int col,
                     int row, int len, int dash, int gap)
{
    for (int at = 0; at < len; at += dash + gap)
        blk(r, pl, c, col + at, row, len - at < dash ? len - at : dash, 1);
}

static void dash_col(SDL_Renderer *r, const Plate *pl, BbcColour c, int col,
                     int row, int len, int dash, int gap)
{
    for (int at = 0; at < len; at += dash + gap)
        blk(r, pl, c, col, row + at, 1, len - at < dash ? len - at : dash);
}

/*
 * A trajectory: BBC pixels along a quadratic arc, which is how the game's own
 * grenades and thrown bricks actually travel.
 *
 * `lift` is how high the drawn curve goes, not where the control point sits: a
 * quadratic only rises halfway to its handle, so passing the handle straight
 * through leaves the arc a comfortable distance below whatever it was drawn to
 * meet — a figure hanging over its own jump. All four ends are logical pixels.
 */
static void dash_arc(SDL_Renderer *r, BbcColour c, float x0, float y0,
                     float x1, float y1, float lift, int steps)
{
    float mx = (x0 + x1) * 0.5f;
    float my = (y0 + y1) * 0.5f - lift * 2.0f;
    for (int i = 0; i <= steps; ++i)
    {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        bbc_plot(r, c, u * u * x0 + 2.0f * u * t * mx + t * t * x1,
                 u * u * y0 + 2.0f * u * t * my + t * t * y1);
    }
}

/* The arrows: one drawing each way, mirrored for the other. Used for the nav
 * chips' sense of direction on the plates and for the way a stomp goes. */
BBC_SPRITE(SPR_ARROW_RIGHT,
    "1..",
    "11.",
    "111",
    "11.",
    "1..");

BBC_SPRITE(SPR_ARROW_DOWN,
    "11111",
    ".111.",
    "..1..");

static void arrow(SDL_Renderer *r, const Plate *pl, BbcColour c, int col,
                  int row, int dx, int dy)
{
    const BbcColour paint[BBC_SPRITE_SLOTS] = {c, c, c, c};
    if (dy != 0)
        bbc_sprite(r, &SPR_ARROW_DOWN, COL(pl->col + col), ROW(pl->row + row),
                   1, paint);
    else
        bbc_sprite(r, &SPR_ARROW_RIGHT, COL(pl->col + col), ROW(pl->row + row),
                   dx > 0 ? 1 : -1, paint);
}

/*
 * A key cap, the two kinds the sheet is made of. A key on the keyboard is a
 * bar of blue with a lit top edge and the letter in white on it — a moulded
 * thing, solid — and a pad button is the same cap cut as an outline in cyan
 * with the letter in cyan: the two columns of the control table are meant to
 * read as two materials, and in eight colours the difference between a solid
 * and an outline is the whole of what a material can be.
 */
typedef enum
{
    CAP_KEY,
    CAP_PAD
} CapKind;

static void draw_keycap(SDL_Renderer *r, float x, float y, float w, float h,
                        const char *label, CapKind kind)
{
    /* The cap lands on the BBC grid and the letter does not, so the letter is
     * centred in the cap as drawn rather than in the rectangle asked for:
     * otherwise a cap whose left edge rounded down by three pixels carries its
     * label three pixels right of centre, into the border. */
    float x0 = bbc_snap_x(x);
    float x1 = bbc_snap_x(x + w);
    x = x0;
    w = x1 - x0;
    if (kind == CAP_KEY)
    {
        bbc_fill(r, BBC_BLUE, x, y, w, h);
        bbc_fill(r, BBC_CYAN, x, y, w, 2.0f);
    }
    else
    {
        bbc_fill(r, BBC_BLACK, x, y, w, h);
        bbc_frame(r, BBC_CYAN, x, y, w, h);
    }
    if (label != NULL && label[0] != '\0')
        bbc_text(r, x + (w - text_width(label)) * 0.5f, y + (h - CH) * 0.5f + 1.0f,
                 1.0f, kind == CAP_KEY ? BBC_WHITE : BBC_CYAN, label);
}

/* Corner marks: the brackets the cutscenes frame things with, a BBC pixel
 * thick, three columns along and six rows down. */
static void draw_brackets(SDL_Renderer *r, BbcColour c, SDL_FRect box)
{
    const float arm_x = 12.0f;
    const float arm_y = 12.0f;
    float x0 = box.x;
    float y0 = box.y;
    float x1 = box.x + box.w - 4.0f;
    float y1 = box.y + box.h - 2.0f;
    bbc_fill(r, c, x0, y0, arm_x, 2.0f);
    bbc_fill(r, c, x0, y0, 4.0f, arm_y);
    bbc_fill(r, c, x1 - arm_x + 4.0f, y0, arm_x, 2.0f);
    bbc_fill(r, c, x1, y0, 4.0f, arm_y);
    bbc_fill(r, c, x0, y1, arm_x, 2.0f);
    bbc_fill(r, c, x0, y1 - arm_y + 2.0f, 4.0f, arm_y);
    bbc_fill(r, c, x1 - arm_x + 4.0f, y1, arm_x, 2.0f);
    bbc_fill(r, c, x1, y1 - arm_y + 2.0f, 4.0f, arm_y);
}

/* ---- The building, in the sectors' own vocabulary --------------------- */

/*
 * The floor the plates are built from is the plant room's, out of the same
 * row of paint the sector renderer reads: red plate with yellow joints and a
 * yellow-and-black hazard nosing on the lip a foot lands on. Red is danger
 * elsewhere, and on a floor it is the machine floor — the one theme a player
 * meets on the first floors that has nothing else on it to explain.
 */
static const LevelThemePaint *plate_paint(void)
{
    return level_theme_paint(LEVEL_THEME_PLANT);
}

/* A slab: the material, the lip, the joints — counted from the slab's own
 * left end, so a slab cut by a hole starts its rhythm afresh — and a black
 * seam under it so it stands off the room below. */
static void slab(SDL_Renderer *r, const Plate *pl, int col, int row, int w,
                 int h)
{
    if (w <= 0 || h <= 0)
        return;
    const LevelThemePaint *paint = plate_paint();
    blk(r, pl, paint->wall, col, row, w, h);
    blk(r, pl, paint->trim, col, row, w, 1);
    for (int c = 0; c < w; c += 2)
        blk(r, pl, ((c / 2) & 1) ? paint->trim_ink : paint->trim, col + c,
            row + 1, w - c < 2 ? w - c : 2, 1);
    for (int c = col + 10; c < col + w - 1; c += 11)
        blk(r, pl, paint->wall_ink, c, row + 2, 1, h - 3);
    for (int c = col + 3; c < col + w - 1; c += 6)
        blk(r, pl, paint->wall_ink, c, row + 3, 1, 1);
    blk(r, pl, BBC_BLACK, col, row + h - 1, w, 1);
}

/*
 * The wall behind a vignette. Rooms are black, and what puts a slab in one is
 * the room's own lines in its backdrop colour — a panel seam every so often,
 * dotted, and a skirting where the wall meets the floor — kept to a whisper,
 * because the plate is a diagram and the figures are what it is about.
 */
static void back_wall(SDL_Renderer *r, const Plate *pl, int top, int floor_row)
{
    const LevelThemePaint *paint = plate_paint();
    if (floor_row - top <= 4)
        return;
    for (int c = 9; c < pl->cols - 1; c += 19)
        stip(r, pl, paint->backdrop, BBC_DITHER_DOTS, c, top, 1,
             floor_row - top - 3);
    blk(r, pl, paint->backdrop, 0, floor_row - 3, pl->cols, 1);
}

/* A window in the back wall, drawn the way the sectors draw the room behind:
 * its frame, a mullion and a transom in the backdrop's blue on the black, and
 * nothing behind the glass but the dark. */
static void room_window(SDL_Renderer *r, const Plate *pl, int col, int row,
                        int w, int h)
{
    const LevelThemePaint *paint = plate_paint();
    blk(r, pl, BBC_BLACK, col, row, w, h);
    bbc_frame(r, paint->backdrop, px_of(pl, (float)col), py_of(pl, (float)row),
              COL(w), ROW(h));
    blk(r, pl, paint->backdrop, col + w / 2, row + 1, 1, h - 2);
    blk(r, pl, paint->backdrop, col + 1, row + h / 3, w - 2, 1);
    blk(r, pl, paint->backdrop, col - 1, row + h, w + 2, 1);
}

/*
 * A ceiling fitting and the light it throws, as the machine had light: a
 * tube in the lamp colour and a cone of its dots falling to the floor. Drawn
 * under the figures so they stand in it.
 */
static void ceiling_lamp(SDL_Renderer *r, const Plate *pl, int col,
                         int ceiling, int floor_row)
{
    const LevelThemePaint *paint = plate_paint();
    /* The light is a short spray of the lamp's dots under the tube and the
     * pool it lands in on the skirting: a cone drawn the whole height of the
     * room is a tree of dots that out-shouts every figure on the plate. */
    for (int i = 0; i < 6 && ceiling + 2 + i < floor_row - 3; ++i)
        stip(r, pl, paint->lamp, BBC_DITHER_SPARSE, col - 1 - i / 2,
             ceiling + 2 + i, 3 + (i / 2) * 2, 1);
    blk(r, pl, paint->lamp, col - 4, floor_row - 3, 9, 1);
    blk(r, pl, BBC_BLUE, col - 2, ceiling, 5, 1);
    blk(r, pl, paint->lamp, col - 1, ceiling + 1, 3, 1);
}

/* A ladder, the yellow every ladder in the building is: two rails and a rung
 * every four rows, six columns across. */
static void ladder(SDL_Renderer *r, const Plate *pl, int col, int row, int h)
{
    blk(r, pl, BBC_YELLOW, col, row, 1, h);
    blk(r, pl, BBC_YELLOW, col + 5, row, 1, h);
    for (int y = row + 2; y < row + h - 1; y += 4)
        blk(r, pl, BBC_YELLOW, col + 1, y, 4, 1);
}

/* The crate the sectors push: timber, which in eight colours is red and
 * yellow on a checkerboard, inside a yellow frame with a brace across it. */
static void crate(SDL_Renderer *r, const Plate *pl, int col, int foot)
{
    int top = foot - 12;
    dith(r, pl, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, col, top, 6, 12);
    bbc_frame(r, BBC_YELLOW, px_of(pl, (float)col), py_of(pl, (float)top),
              COL(6), ROW(12));
    bbc_line(r, BBC_YELLOW, px_of(pl, (float)col + 1.0f),
             py_of(pl, (float)top + 1.0f), px_of(pl, (float)col + 4.0f),
             py_of(pl, (float)top + 10.0f));
}

/* Steel teeth on a strip bolted to the floor: each tooth a white column with
 * its shoulders in cyan, on a red strip because a spike is a hazard before it
 * is anything else. */
static void spikes(SDL_Renderer *r, const Plate *pl, int col, int foot,
                   int count)
{
    for (int i = 0; i < count; ++i)
    {
        int c = col + i * 3;
        blk(r, pl, BBC_WHITE, c + 1, foot - 6, 1, 5);
        blk(r, pl, BBC_CYAN, c, foot - 3, 1, 2);
        blk(r, pl, BBC_CYAN, c + 2, foot - 3, 1, 2);
    }
    blk(r, pl, BBC_RED, col - 1, foot - 1, count * 3 + 1, 1);
}

/*
 * A guard's sight cone, as a diagram of one: a stipple of the cone's colour
 * with its two edges stepped in solid, out to the range and no further. The
 * range is a falloff the player feels rather than a fence on the floor, so
 * the end of it is left open.
 */
static void sight_cone(SDL_Renderer *r, const Plate *pl, int eye_col,
                       int eye_row, int dir, int len, int half, BbcColour c)
{
    for (int k = 1; k <= len; ++k)
    {
        int h = 1 + (half - 1) * k / len;
        int col = eye_col + dir * k;
        stip(r, pl, c, BBC_DITHER_DOTS, col, eye_row - h, 1, h * 2 + 1);
        if (k < len - 2 || (k & 1) == 0)
        {
            blk(r, pl, c, col, eye_row - h, 1, 1);
            blk(r, pl, c, col, eye_row + h, 1, 1);
        }
    }
}

/* ---- Small props ------------------------------------------------------ */

/* A sprite scaled up and laid in place: the pickups on the plates are the
 * HUD's and the floor's own icons, so the thing the sheet tells the player to
 * look for is the thing they will find. */
static void icon(SDL_Renderer *r, const Plate *pl, const BbcSprite *sprite,
                 int col, int row, int scale)
{
    bbc_sprite_scaled(r, sprite, COL(pl->col + col), ROW(pl->row + row), 1,
                      NULL, scale);
}

/* One of the three states the ACCESS chip shows: a black chip framed in its
 * colour, a lamp at its head and the word beside it. */
static void draw_access_chip(SDL_Renderer *r, float x, float y, float w,
                             BbcColour edge, BbcColour led, BbcColour text,
                             const char *label)
{
    bbc_fill(r, BBC_BLACK, x, y, w, 14.0f);
    bbc_frame(r, edge, x, y, w, 14.0f);
    bbc_fill(r, led, x + 4.0f, y + 4.0f, 4.0f, 6.0f);
    bbc_text(r, x + 12.0f, y + 3.0f, 1.0f, text, label);
}

/* ---- Illustrations --------------------------------------------------- */

/* A cutout in the case's foam: black, with its far lip catching the lamp. */
static void recess(SDL_Renderer *r, const Plate *pl, int col, int row, int w,
                   int h)
{
    blk(r, pl, BBC_BLACK, col, row, w, h);
    blk(r, pl, BBC_BLUE, col, row + h - 1, w, 1);
}

/* The rifle in its foam, a drawing of its own at two BBC pixels to the
 * square: timber stock and grip, blued receiver lit along the top in cyan,
 * the vented handguard, and the barrel and sights in white. */
BBC_SPRITE(SPR_CASE_RIFLE,
    ".....WW.....W.",
    "YYYYCCCCCBBBBW",
    "RRRRBBBBBKBKBW",
    "RR..RRBB......",
    "......BB......");

static void case_rifle(SDL_Renderer *r, const Plate *pl, int col, int row)
{
    bbc_sprite_scaled(r, &SPR_CASE_RIFLE, COL(pl->col + col), ROW(pl->row + row),
                      1, NULL, 2);
}

/*
 * What Meridian Facility Services wheeled through the goods entrance, opened
 * on the floor of a sector.
 *
 * The story page could have been illustrated with the three faces in it, and
 * three heads at twelve pixels across would have said nothing the text does
 * not already say. This says the part the text cannot: the rifles, the tube
 * and the frags were signed in weeks ago on a maintenance docket, which is
 * both the twist and the reason the player keeps finding a bazooka on a
 * carpet tile.
 *
 * It is drawn as a case standing open and facing the reader — lid on the
 * left, foam bed on the right — because a case lying open would have to be
 * drawn from above, and this game has exactly one camera. The launcher, the
 * frags and the rockets in it are the HUD's own icons blown up, which is what
 * the player is about to be picking up.
 */
static void illus_night(SDL_Renderer *r, SDL_FRect p, float time,
                        const PadHints *pad,
                        const ManualRecords *records)
{
    (void)records;
    (void)pad;
    (void)time;
    Plate pl = plate_of(p);
    int cx = pl.cols / 2;
    int floor_row = pl.rows - 20;
    int top = floor_row - 122;
    int lid_col = cx - 37;
    int bed_col = cx + 1;
    const int lid_w = 35;
    const int bed_w = 36;
    const int case_h = 122;

    /* The floor it is standing on. */
    blk(r, &pl, BBC_BLUE, 0, floor_row, pl.cols, 1);
    dith(r, &pl, BBC_BLACK, BBC_BLUE, BBC_DITHER_SPARSE, 0, floor_row + 1,
         pl.cols, pl.rows - floor_row - 1);

    for (int half = 0; half < 2; ++half)
    {
        int col = half == 0 ? lid_col : bed_col;
        int w = half == 0 ? lid_w : bed_w;
        blk(r, &pl, BBC_BLUE, col, top, w, case_h);
        blk(r, &pl, BBC_CYAN, col, top, w, 1);
        blk(r, &pl, BBC_BLACK, col, top + case_h - 1, w, 1);
        /* Corner protectors and two latches down the outer edge. */
        int edge = half == 0 ? col : col + w - 2;
        blk(r, &pl, BBC_WHITE, edge, top, 2, 8);
        blk(r, &pl, BBC_WHITE, edge, top + case_h - 9, 2, 8);
        for (int i = 0; i < 2; ++i)
        {
            int ly = top + 33 + i * 56;
            blk(r, &pl, BBC_CYAN, edge, ly, 2, 7);
            blk(r, &pl, BBC_BLACK, edge, ly + 3, 2, 1);
        }
        blk(r, &pl, BBC_RED, half == 0 ? col + 2 : col + w - 5, top + case_h - 22,
            3, 1);
    }

    /* The piano hinge the lid swings on, down the seam between the halves. */
    blk(r, &pl, BBC_BLACK, cx - 2, top, 3, case_h);
    for (int hy = top + 6; hy < top + case_h - 6; hy += 5)
    {
        blk(r, &pl, BBC_CYAN, cx - 2, hy, 3, 3);
        blk(r, &pl, BBC_WHITE, cx - 2, hy, 3, 1);
    }

    /* The lid liner, so the docket is stencilled on something. */
    blk(r, &pl, BBC_BLACK, lid_col + 2, top + 5, lid_w - 4, case_h - 12);

    float lx = px_of(&pl, (float)lid_col);
    float ly = py_of(&pl, (float)top);
    draw_tracked(r, lx + 18.0f, ly + 26.0f, 2.0f, BBC_WHITE, "MERIDIAN");
    bbc_text(r, lx + 18.0f, ly + 44.0f, 1.0f, BBC_CYAN, "FACILITY");
    bbc_text(r, lx + 18.0f, ly + 56.0f, 1.0f, BBC_CYAN, "SERVICES");
    blk(r, &pl, BBC_RED, lid_col + 4, top + 36, 16, 1);
    bbc_text(r, lx + 18.0f, ly + 88.0f, 1.0f, BBC_WHITE, "NIGHT SHIFT");
    bbc_text(r, lx + 18.0f, ly + 102.0f, 1.0f, BBC_WHITE, "TOOL SET 07");
    bbc_text(r, lx + 18.0f, ly + 128.0f, 1.0f, BBC_BLUE, "SIGNED IN");
    bbc_text(r, lx + 18.0f, ly + 142.0f, 1.0f, BBC_YELLOW, "14 MARCH");

    /* The docket bar, and the line on it that is the whole plot. */
    for (int i = 0; i < 24;)
    {
        unsigned h = hash_u((unsigned)i * 2654435761u + 91u);
        int bar = (h & 4u) ? 2 : 1;
        blk(r, &pl, BBC_WHITE, lid_col + 4 + i, top + 86, bar, 7);
        i += bar + 1 + (int)(h & 1u);
    }
    bbc_text(r, lx + 18.0f, ly + 196.0f, 1.0f, BBC_RED, "NOT INSPECTED");

    /* The foam bed, and the five cutouts that are not tools. */
    dith(r, &pl, BBC_BLACK, BBC_BLUE, BBC_DITHER_SPARSE, bed_col + 2, top + 4,
         bed_w - 4, case_h - 11);
    int inner = bed_w - 6;

    recess(r, &pl, bed_col + 3, top + 8, inner, 15);
    case_rifle(r, &pl, bed_col + 4, top + 10);

    recess(r, &pl, bed_col + 3, top + 26, inner, 17);
    icon(r, &pl, &SPR_ICON_BAZOOKA, bed_col + 6, top + 28, 3);

    recess(r, &pl, bed_col + 3, top + 46, inner, 21);
    for (int i = 0; i < 4; ++i)
        icon(r, &pl, &SPR_ICON_GRENADE, bed_col + 5 + i * 7, top + 51, 2);

    recess(r, &pl, bed_col + 3, top + 70, inner, 22);
    for (int i = 0; i < 3; ++i)
        icon(r, &pl, &SPR_ICON_ROCKET, bed_col + 12, top + 72 + i * 7, 2);

    recess(r, &pl, bed_col + 3, top + 95, inner, 20);
    for (int i = 0; i < 5; ++i)
    {
        int my = top + 97 + i * 4;
        blk(r, &pl, BBC_BLUE, bed_col + 5, my, inner - 4, 3);
        blk(r, &pl, BBC_CYAN, bed_col + 5, my, inner - 4, 1);
    }
}

/*
 * The night access log, on the clipboard it was signed on.
 *
 * The sheet beside it names the crew in prose; this is the same twelve names
 * as a document, which is the difference between being told there are twelve
 * of them and counting twelve of them. It is drawn off `crew_callsign`
 * (see [crew.h](crew.h)) rather than out of a list of its own, so the man who
 * answers to LENZ on the strip while a sector is being played is the man on
 * line one of the log — a manual that named a thirteenth would be a manual for
 * a different building.
 *
 * The bottom of the sheet is the joke and the threat at once: twelve badged
 * in, twelve accounted for, and a signature block nobody ever checked. The
 * stamp is in magenta, which is the story's colour and Voss's.
 */
static void illus_crew(SDL_Renderer *r, SDL_FRect p, float time,
                       const PadHints *pad,
                       const ManualRecords *records)
{
    (void)records;
    (void)pad;
    (void)time;
    Plate pl = plate_of(p);

    /* Hardboard behind, docket clipped over it: the board is what makes this
       a thing on a desk rather than a page of the manual with a border. */
    const int bx = 9;
    const int by = 7;
    const int bw = 58;
    const int bh = 148;
    dith(r, &pl, BBC_RED, BBC_YELLOW, BBC_DITHER_SPARSE, bx, by, bw, bh);
    blk(r, &pl, BBC_YELLOW, bx, by, bw, 1);
    blk(r, &pl, BBC_BLACK, bx, by + bh - 1, bw, 1);

    const int sx = bx + 2;
    const int sy = by + 9;
    const int sw = bw - 4;
    const int sh = bh - 13;
    blk(r, &pl, BBC_WHITE, sx, sy, sw, sh);
    /* The corner that has been thumbed, lifting off the board. */
    blk(r, &pl, BBC_CYAN, sx + sw - 2, sy + sh - 2, 2, 2);
    blk(r, &pl, BBC_BLACK, sx + sw - 1, sy + sh - 1, 1, 1);

    /* The spring clip, biting the head of the sheet. */
    blk(r, &pl, BBC_CYAN, bx + 19, by + 1, 20, 11);
    blk(r, &pl, BBC_WHITE, bx + 19, by + 1, 20, 1);
    blk(r, &pl, BBC_BLUE, bx + 21, by + 5, 16, 2);
    blk(r, &pl, BBC_WHITE, bx + 20, by + 9, 1, 1);
    blk(r, &pl, BBC_WHITE, bx + 37, by + 9, 1, 1);
    blk(r, &pl, BBC_BLUE, bx + 19, by + 11, 20, 1);

    float tx = px_of(&pl, (float)sx) + 8.0f;
    float ty = py_of(&pl, (float)sy);
    draw_tracked(r, tx, ty + 30.0f, 1.0f, BBC_BLACK, "MERIDIAN FACILITY");
    bbc_text(r, tx, ty + 42.0f, 1.0f, BBC_BLUE, "NIGHT ACCESS LOG");
    bbc_text(r, tx, ty + 54.0f, 1.0f, BBC_BLUE, "14 MARCH  22:00-06:00");
    blk(r, &pl, BBC_RED, sx + 2, sy + 33, 19, 1);

    /* Twelve ruled lines, twelve names, twelve marks in the box. The
       signatures are hash-driven scribble rather than glyphs: a signature made
       of letters is a printed name, and a printed name is what the left
       column already is. */
    const int first = sy + 37;
    const int pitch = 6;
    for (int i = 0; i < CREW_SIZE; ++i)
    {
        int row = first + i * pitch;
        dash_row(r, &pl, BBC_CYAN, sx + 2, row + 5, sw - 4, 1, 1);
        blk(r, &pl, BBC_RED, sx + 2, row + 1, 1, 2);
        bbc_text(r, tx + 10.0f, py_of(&pl, (float)row), 1.0f, BBC_BLACK,
                 crew_callsign(i));

        for (int stroke = 0; stroke < 7; ++stroke)
        {
            unsigned h = hash_u((unsigned)(i * 13 + stroke) * 2654435761u);
            int s_h = 1 + (int)(h % 3u);
            blk(r, &pl, BBC_BLUE, sx + 32 + stroke * 2, row + 3 - s_h / 2 - (int)((h >> 4) & 1u), 1, s_h);
        }
    }

    /* The stamp along the foot: the count nobody ever went back and checked
       against a case. */
    int foot = first + CREW_SIZE * pitch + 2;
    blk(r, &pl, BBC_BLACK, sx + 2, foot, sw - 4, 1);
    bbc_text(r, tx, py_of(&pl, (float)foot) + 8.0f, 1.0f, BBC_BLACK,
             "12 SIGNED IN");
    blk(r, &pl, BBC_MAGENTA, sx + sw - 19, foot + 2, 17, 8);
    bbc_text(r, px_of(&pl, (float)(sx + sw - 19)) + 6.0f,
             py_of(&pl, (float)foot) + 8.0f, 1.0f, BBC_WHITE, "A. VOSS");
}

/* Sector one to the roof, and where the climbs fall in it. */
static void illus_mission(SDL_Renderer *r, SDL_FRect p, float time,
                          const PadHints *pad,
                          const ManualRecords *records)
{
    (void)records;
    (void)pad;
    Plate pl = plate_of(p);
    const int street = pl.rows - 11;
    const int roof = 13;
    const int cx = 46;
    const int base_half = 12;
    const int top_half = 10;

    /* Stars, a BBC pixel each, keyed to the index so the same sky is drawn
     * every frame; a twinkle is a step up the ladder for a beat. */
    for (unsigned i = 0; i < 34u; ++i)
    {
        unsigned h = hash_u(i * 2654435761u + 17u);
        int col = (int)(h % (unsigned)pl.cols);
        int row = (int)((h >> 9) % (unsigned)(pl.rows * 6 / 10));
        bool flare = sinf(time * 1.6f + (float)(h % 61u)) > 0.7f;
        BbcColour c = (h & 3u) == 0u ? BBC_CYAN : BBC_BLUE;
        blk(r, &pl, flare ? BBC_WHITE : c, col, row, 1, 1);
    }

    /* The moon, up and to the left where every lit thing in the game has its
     * light from, with its own shadow down the lower limb. */
    float moon_x = px_of(&pl, 7.0f);
    float moon_y = py_of(&pl, 16.0f);
    bbc_disc(r, BBC_CYAN, moon_x, moon_y, 11.0f);
    bbc_disc(r, BBC_WHITE, moon_x - 2.0f, moon_y - 2.0f, 9.0f);
    bbc_plot(r, BBC_CYAN, moon_x - 4.0f, moon_y - 4.0f);
    bbc_plot(r, BBC_CYAN, moon_x + 2.0f, moon_y + 2.0f);

    /* The city behind it: a far row of blue on a checkerboard, so the tower
     * stands in front of something rather than against an empty sky. */
    for (unsigned i = 0; i < 9u; ++i)
    {
        unsigned h = hash_u(i * 2246822519u + 5u);
        int bw = 5 + (int)(h % 6u);
        int bh = 25 + (int)((h >> 5) % 45u);
        int bx = (int)i * 9 - 3 + (int)((h >> 12) % 3u);
        dith(r, &pl, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, bx, street - bh, bw,
             bh);
        blk(r, &pl, BBC_BLUE, bx, street - bh, bw, 1);
        for (unsigned k = 0; k < 6u; ++k)
        {
            unsigned w = hash_u(h + k * 97u);
            if ((w % 100u) >= 32u)
                continue;
            blk(r, &pl, (w & 1u) ? BBC_YELLOW : BBC_CYAN,
                bx + 1 + (int)((w >> 8) % (unsigned)(bw - 1 > 1 ? bw - 1 : 1)),
                street - bh + 3 + (int)((w >> 16) % (unsigned)(bh - 6)), 1, 1);
        }
    }

    /* The tower, stepping wider toward the street because the shot looks up
     * at it from the kerb, lit down its moon side and in shade down the other:
     * a mass standing in the light, not a shape cut out of the sky. */
    const int course = 8;
    const int courses = (street - roof - 12) / course;
    for (int k = 0; k <= courses; ++k)
    {
        int y0 = roof + 2 + k * course;
        int h = k == courses ? street - y0 : course;
        int half = top_half + (base_half - top_half) * k / courses;
        blk(r, &pl, BBC_BLUE, cx - half, y0, half * 2, h);
        blk(r, &pl, BBC_CYAN, cx - half, y0, 1, h);
        dith(r, &pl, BBC_BLUE, BBC_BLACK, BBC_DITHER_CHECK, cx + half - 1, y0, 1,
             h);
    }
    blk(r, &pl, BBC_BLUE, cx - top_half - 1, roof, top_half * 2 + 2, 2);
    blk(r, &pl, BBC_CYAN, cx - top_half - 1, roof, top_half * 2 + 2, 1);

    /* The plant room on the roof and a mast over it, with the one red light a
     * tower this tall has to carry for the helicopters, on the flash. */
    blk(r, &pl, BBC_BLUE, cx - 6, roof - 4, 12, 4);
    blk(r, &pl, BBC_CYAN, cx - 6, roof - 4, 12, 1);
    blk(r, &pl, BBC_BLUE, cx - 4, roof - 10, 1, 6);
    blk(r, &pl, bbc_flash_pair(BBC_RED, BBC_BLACK), cx - 4, roof - 11, 1, 1);

    /* The windows, four a course. A lit office is a dither — warm is red and
     * yellow, cool is blue and cyan — and a dark one is glass. */
    for (int k = 0; k < courses; ++k)
    {
        int wy = roof + 4 + k * course;
        int half = top_half + (base_half - top_half) * k / courses;
        int pane = (half * 2 - 3 - 3) / 4;
        int x0 = cx - half + 2 + (half * 2 - 3 - pane * 4 - 3) / 2;
        for (int c = 0; c < 4; ++c)
        {
            int wx = x0 + c * (pane + 1);
            unsigned h = hash_u((unsigned)(k * 41 + c * 7 + 3) * 2654435761u);
            bool lit = (h % 100u) < 26u &&
                       time > 0.25f + (float)(h % 12u) * 0.05f;
            if (k == 1 && c == 2)
            {
                /* The window the whole picture points at: second course,
                 * third pane, solid where every other light is a dither, with
                 * the corner marks on it flashing. */
                blk(r, &pl, BBC_YELLOW, wx, wy, pane, 4);
                blk(r, &pl, BBC_BLACK, wx + pane / 2, wy + 1, 1, 3);
                BbcColour mark = bbc_flash_pair(BBC_RED, BBC_BLUE);
                blk(r, &pl, mark, wx - 2, wy - 2, 2, 1);
                blk(r, &pl, mark, wx - 2, wy - 2, 1, 2);
                blk(r, &pl, mark, wx + pane, wy - 2, 2, 1);
                blk(r, &pl, mark, wx + pane + 1, wy - 2, 1, 2);
                blk(r, &pl, mark, wx - 2, wy + 5, 2, 1);
                blk(r, &pl, mark, wx - 2, wy + 4, 1, 2);
                blk(r, &pl, mark, wx + pane, wy + 5, 2, 1);
                blk(r, &pl, mark, wx + pane + 1, wy + 4, 1, 2);
                continue;
            }
            if (lit)
                dith(r, &pl, (h & 8u) ? BBC_RED : BBC_BLUE,
                     (h & 8u) ? BBC_YELLOW : BBC_CYAN, BBC_DITHER_CHECK, wx, wy,
                     pane, 4);
            else
                blk(r, &pl, BBC_BLACK, wx, wy, pane, 4);
        }
    }

    /*
     * The route: a tick a sector bottom to top, the climbs called out in
     * yellow. It is the only place the campaign's shape is drawn rather than
     * described.
     *
     * **Both numbers are read rather than written here**, and that is the whole
     * of what this loop got wrong. It counted to a literal 15 and tested against
     * four literal climb numbers, so the day the vault and the fifth climb
     * arrived it went on drawing the old campaign — two ticks short, one of them
     * the wrong colour — on the one sheet whose job is to show the player the
     * shape of the night. Nothing could have caught it: a strap is measured
     * against its column, and a picture is measured against nothing.
     * `CAMPAIGN_SECTORS` is the count the night clock is already checked
     * against, and `CAMPAIGN_CLIMB_SECTORS` is the list the strap on `THE CLIMB`
     * spells out, both held against the embedded maps by the suite.
     */
    const int route = cx - base_half - 4;
    dash_col(r, &pl, BBC_BLUE, route, roof + 2, street - roof - 4, 2, 2);
    for (int sector = 1; sector <= CAMPAIGN_SECTORS; ++sector)
    {
        float t = (float)(sector - 1) / (float)(CAMPAIGN_SECTORS - 1);
        int row = street - 4 - (int)(t * (float)(street - roof - 8) + 0.5f);
        bool climb = false;
        for (int i = 0; i < CAMPAIGN_CLIMB_SECTOR_COUNT; ++i)
            if (CAMPAIGN_CLIMB_SECTORS[i] == sector)
                climb = true;
        blk(r, &pl, climb ? BBC_YELLOW : BBC_CYAN, route - 1, row, 3, 1);
    }
    bbc_text(r, px_of(&pl, (float)route) - 34.0f, py_of(&pl, (float)roof) - 2.0f,
             1.0f, BBC_CYAN, "ROOF");

    /* Blocks cropping both edges. Without them one slab stands in the middle
     * of an empty plate; the point of the shot is a building in a city. */
    blk(r, &pl, BBC_BLACK, 0, street - 31, 10, 31);
    blk(r, &pl, BBC_BLUE, 0, street - 31, 10, 1);
    blk(r, &pl, BBC_BLUE, 9, street - 31, 1, 31);
    blk(r, &pl, BBC_BLUE, 2, street - 26, 2, 2);
    blk(r, &pl, BBC_CYAN, 5, street - 16, 2, 2);
    blk(r, &pl, BBC_BLACK, pl.cols - 13, street - 44, 13, 44);
    blk(r, &pl, BBC_BLUE, pl.cols - 13, street - 44, 13, 1);
    blk(r, &pl, BBC_BLUE, pl.cols - 13, street - 44, 1, 44);
    for (unsigned i = 0; i < 9u; ++i)
    {
        unsigned h = hash_u(i * 40503u + 7u);
        if ((h % 100u) >= 45u)
            continue;
        dith(r, &pl, BBC_RED, BBC_YELLOW, BBC_DITHER_SPARSE,
             pl.cols - 11 + (int)(i % 3u) * 4, street - 39 + (int)(i / 3u) * 11,
             2, 3);
    }

    /* The front door he walked her through, lit at the foot of the tower,
     * and the wet street under it giving the light back in rows. */
    blk(r, &pl, BBC_CYAN, cx - 4, street - 8, 8, 1);
    blk(r, &pl, BBC_YELLOW, cx - 3, street - 7, 6, 7);
    blk(r, &pl, BBC_BLACK, cx, street - 7, 1, 7);

    blk(r, &pl, BBC_BLUE, 0, street, pl.cols, 1);
    stip(r, &pl, BBC_YELLOW, BBC_DITHER_ROWS, cx - 3, street + 1, 6, 6);

    /* Street, lamp, and the man looking up at it. */
    for (int row = street - 18; row < street; ++row)
    {
        int half = 1 + (row - street + 18) / 4;
        stip(r, &pl, BBC_CYAN, BBC_DITHER_DOTS, 7 - half, row, half * 2 + 1, 1);
    }
    blk(r, &pl, BBC_BLUE, 7, street - 20, 1, 20);
    blk(r, &pl, BBC_WHITE, 6, street - 21, 3, 1);
    stip(r, &pl, BBC_YELLOW, BBC_DITHER_ROWS, 2, street + 1, 11, 4);
    figure(r, &pl, &SPR_CHUCK_STAND, 14, street, 1, NULL);
}

/* Where a face button physically sits, from the position `pad_hint` filed its
 * letter under. It is the drawing's half of the module's rule: the sheet spells
 * no letter itself, and it draws none in a place the pad does not put it. */
static void face_offset(int at, float *ox, float *oy)
{
    *ox = 0.0f;
    *oy = 0.0f;
    switch (at)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        *oy = 16.0f;
        break;
    case SDL_GAMEPAD_BUTTON_EAST:
        *ox = 16.0f;
        break;
    case SDL_GAMEPAD_BUTTON_WEST:
        *ox = -16.0f;
        break;
    case SDL_GAMEPAD_BUTTON_NORTH:
        *oy = -16.0f;
        break;
    default:
        break;
    }
}

/*
 * Every control, on the two things you might be holding.
 *
 * Both halves are laid out from one centre line with the labels given their
 * own rows, because a label tucked beside a keycap is a label that collides
 * with the next one the moment either string changes length.
 *
 * The pad is drawn for the pad in the player's hands rather than for an Xbox
 * one. A Nintendo pad prints A where an Xbox pad prints B, so a diagram with
 * the letters nailed to fixed corners tells a Switch player the wrong thing
 * about every button on it — and a PlayStation player four letters their pad
 * does not carry at all. Both the position and the spelling therefore come off
 * `PadHints`, which is the same place the control table beside it reads from;
 * the colours stay with the *action*, so the sheet still looks exactly as it
 * did on the pad it was drawn for.
 */
static void illus_controls(SDL_Renderer *r, SDL_FRect p, float time,
                          const PadHints *pad,
                          const ManualRecords *records)
{
    (void)records;
    (void)time;
    Plate pl = plate_of(p);
    float ox = px_of(&pl, 0.0f);
    float oy = py_of(&pl, 0.0f);
    float cx = px_of(&pl, (float)(pl.cols / 2));
    const float cap = 24.0f;
    const float pitch = 28.0f;

    bbc_text(r, ox + 10.0f, oy + 8.0f, 1.0f, BBC_CYAN, "KEYBOARD");

    /* The movement cluster, drawn as the cross it is under the hand. */
    float wy = oy + 28.0f;
    draw_keycap(r, cx - cap * 0.5f, wy, cap, CHIP_H, "W", CAP_KEY);
    draw_keycap(r, cx - cap * 0.5f - pitch, wy + pitch - 6.0f, cap, CHIP_H, "A",
                CAP_KEY);
    draw_keycap(r, cx - cap * 0.5f, wy + pitch - 6.0f, cap, CHIP_H, "S", CAP_KEY);
    draw_keycap(r, cx - cap * 0.5f + pitch, wy + pitch - 6.0f, cap, CHIP_H, "D",
                CAP_KEY);
    bbc_text(r, cx + 20.0f, wy + 5.0f, 1.0f, BBC_YELLOW, "JUMP");
    bbc_text(r, cx - 116.0f, wy + pitch - 1.0f, 1.0f, BBC_YELLOW, "MOVE");
    dash_row(r, &pl, BBC_BLUE, (int)((cx - 76.0f - ox) / 4.0f),
             (int)((wy + pitch + 3.0f - oy) / 2.0f), 6, 1, 1);
    /* Under S, not out past D. Set to the right of the row it read as the
     * label of the last key in it, which is the one key in the cluster that
     * does not crawl. */
    bbc_text(r, cx - 20.0f, wy + pitch + 18.0f, 1.0f, BBC_YELLOW, "CRAWL");

    /* Everything the hands do that is not walking, on its own row so the
     * labels underneath never meet. */
    float row = wy + pitch * 2.0f + 8.0f;
    draw_keycap(r, cx - 96.0f, row, 64.0f, CHIP_H, "SPACE", CAP_KEY);
    draw_keycap(r, cx - 24.0f, row, 36.0f, CHIP_H, "TAB", CAP_KEY);
    draw_keycap(r, cx + 32.0f, row, cap, CHIP_H, "E", CAP_KEY);
    bbc_text(r, cx - 96.0f, row + CHIP_H + 6.0f, 1.0f, BBC_YELLOW, "ATTACK");
    bbc_text(r, cx - 30.0f, row + CHIP_H + 6.0f, 1.0f, BBC_YELLOW, "WEAPON");
    bbc_text(r, cx + 32.0f, row + CHIP_H + 6.0f, 1.0f, BBC_YELLOW, "USE");

    /* The pad. Same actions, one thumb. Laid out in BBC columns from its own
     * left edge, so the shell's rounded corners step a whole pixel at a time. */
    const int gw = 44;
    const int gh = 34;
    int gx = pl.cols / 2 - gw / 2;
    int gy = pl.rows - 54;
    bbc_text(r, ox + 10.0f, py_of(&pl, (float)gy) - 30.0f, 1.0f, BBC_CYAN,
             "GAMEPAD");

    const PadHints *hints = pad != NULL ? pad : &PAD_HINTS_XBOX;

    /* The bumpers, lit along the top edge. */
    for (int side = 0; side < 2; ++side)
    {
        int bx = side == 0 ? gx + 5 : gx + gw - 13;
        blk(r, &pl, BBC_BLUE, bx, gy - 3, 8, 3);
        blk(r, &pl, BBC_CYAN, bx + 1, gy - 3, 6, 1);
    }
    /* A shoulder is hardware exactly as much as a face button is: L and R on a
     * Switch pad, L1 and R1 on a PlayStation. The control table beside this
     * drawing already spells them out of `$LB $RB`, so a drawing that says LB
     * whatever is plugged in contradicts its own page. Centred on the bumper
     * rather than set at a fixed inset, because the names are one, two or
     * three cells wide. */
    bbc_text(r, px_of(&pl, (float)gx + 9.0f) - text_width(hints->shoulder_l) * 0.5f,
             py_of(&pl, (float)gy) - 18.0f, 1.0f, BBC_CYAN, hints->shoulder_l);
    bbc_text(r,
             px_of(&pl, (float)(gx + gw) - 9.0f) -
                 text_width(hints->shoulder_r) * 0.5f,
             py_of(&pl, (float)gy) - 18.0f, 1.0f, BBC_CYAN, hints->shoulder_r);

    /* The shell, its corners rounded off in steps and its grips hanging under
     * it: blue, with the top face lit in cyan and the belly in a checkerboard
     * of shade. */
    for (int y = 0; y < gh; ++y)
    {
        int inset = y < 3 ? 3 - y : 0;
        if (y >= gh - 10)
        {
            /* The two grips. */
            int g = (y - (gh - 10)) / 2;
            blk(r, &pl, BBC_BLUE, gx + g, gy + y, 13 - g, 1);
            blk(r, &pl, BBC_BLUE, gx + gw - 13, gy + y, 13 - g, 1);
            continue;
        }
        blk(r, &pl, BBC_BLUE, gx + inset, gy + y, gw - inset * 2, 1);
    }
    blk(r, &pl, BBC_CYAN, gx + 3, gy, gw - 6, 1);
    stip(r, &pl, BBC_BLACK, BBC_DITHER_SPARSE, gx + 13, gy + gh - 12, gw - 26,
         3);

    /* Two sticks in the middle of the shell, and the two small buttons between
     * the halves: without them the drawing is a d-pad glued to four buttons
     * rather than the thing in the player's hands. */
    for (int side = 0; side < 2; ++side)
    {
        float sx = px_of(&pl, (float)(side == 0 ? gx + 17 : gx + 27));
        float sy = py_of(&pl, (float)gy + 23.0f);
        bbc_disc(r, BBC_BLACK, sx, sy, 8.0f);
        bbc_disc(r, BBC_CYAN, sx, sy - 1.0f, 4.0f);
    }
    blk(r, &pl, BBC_CYAN, gx + 19, gy + 9, 2, 1);
    blk(r, &pl, BBC_CYAN, gx + 23, gy + 9, 2, 1);

    /* D-pad left, face buttons right, both far enough apart to be read. */
    int dx = gx + 9;
    int dy = gy + 15;
    blk(r, &pl, BBC_BLACK, dx - 2, dy - 7, 5, 15);
    blk(r, &pl, BBC_BLACK, dx - 5, dy - 2, 11, 5);
    blk(r, &pl, BBC_WHITE, dx - 1, dy - 6, 3, 13);
    blk(r, &pl, BBC_WHITE, dx - 4, dy - 1, 9, 3);
    blk(r, &pl, BBC_CYAN, dx, dy, 1, 1);

    float bx = px_of(&pl, (float)(gx + gw - 10));
    float by = py_of(&pl, (float)(gy + 15)) + 1.0f;
    /* Filed by what the button does, so the colours stay with the action while
     * the letter and the corner follow the hardware. */
    static const BbcColour FACE_TINT[] = {
        BBC_GREEN,  /* confirm */
        BBC_RED,    /* cancel  */
        BBC_BLUE,   /* attack  */
        BBC_YELLOW, /* door    */
    };
    _Static_assert(sizeof(FACE_TINT) / sizeof(FACE_TINT[0]) ==
                       (size_t)PAD_FACE_COUNT,
                   "every face needs a tint");
    for (int i = 0; i < PAD_FACE_COUNT; ++i)
    {
        float fx = 0.0f;
        float fy = 0.0f;
        face_offset(pad_hints_button(hints, (PadFace)i), &fx, &fy);
        float px = bx + fx;
        float py = by + fy;
        bbc_disc(r, BBC_BLACK, px, py, 9.0f);
        bbc_disc(r, FACE_TINT[i], px, py, 7.0f);
        /* Centred rather than set at a fixed inset: a PlayStation pad spells
         * two of its faces with two characters, and a cap the label hangs out
         * of is worse than no cap at all. */
        const char *label = hints->face[i];
        float label_w = (float)SDL_strlen(label) * CH;
        bbc_text(r, px - label_w * 0.5f, py - 3.0f, 1.0f,
                 FACE_TINT[i] == BBC_YELLOW || FACE_TINT[i] == BBC_GREEN
                     ? BBC_BLACK
                     : BBC_WHITE,
                 label);
    }

    /* B attacks beside X inside a sector, and the legend says so: a live
     * button left off the sheet is a button nobody finds. */
    char legend[24];
    float legend_y = py_of(&pl, (float)(gy + gh)) + 10.0f;
    bbc_text(r, ox + 14.0f, legend_y, 1.0f, BBC_YELLOW,
             pad_hint(hints, legend, sizeof(legend), "$A JUMP", "$A JUMP"));
    bbc_text(r, ox + 104.0f, legend_y, 1.0f, BBC_YELLOW,
             pad_hint(hints, legend, sizeof(legend), "$X $B ATTACK",
                      "$X $B ATTACK"));
    bbc_text(r, ox + 232.0f, legend_y, 1.0f, BBC_YELLOW,
             pad_hint(hints, legend, sizeof(legend), "$Y USE", "$Y USE"));
}

/* What the floor plan will and will not let a pair of boots do. */
static void illus_movement(SDL_Renderer *r, SDL_FRect p, float time,
                          const PadHints *pad,
                          const ManualRecords *records)
{
    (void)records;
    (void)pad;
    (void)time;
    Plate pl = plate_of(p);
    const int floor_row = pl.rows - 17;
    const int upper = 48;
    const int gap_left = 37;
    /* Two tiles at the vignette's own scale, which is about four fifths of the
     * world's — the crate beside it is drawn 24px against a `CRATE_W` of 28.
     * It was one tile wide, under a caption that said one tile was the jump,
     * and both were a tile short of what the body does: see
     * `test_a_jump_clears_a_wider_hole_than_the_model_will_route` for the
     * widths and `test_the_on_foot_sheet_spells_the_jump_it_draws` for the
     * caption. The caption is the claim and is held; this is the picture
     * agreeing with it, because a drawing that shows a notch under a line saying two tiles is
     * the reader's first reason to doubt the line. */
    const int gap_right = gap_left + 13;

    /* Two storeys and the hole between the upper slabs. */
    back_wall(r, &pl, 0, upper);
    back_wall(r, &pl, upper + 7, floor_row);
    room_window(r, &pl, 55, 12, 12, 22);
    room_window(r, &pl, 21, 72, 12, 26);
    ceiling_lamp(r, &pl, 19, 0, upper);
    ceiling_lamp(r, &pl, 55, upper + 7, floor_row);
    slab(r, &pl, 0, upper, gap_left, 7);
    slab(r, &pl, gap_right, upper, pl.cols - gap_right, 7);
    slab(r, &pl, 0, floor_row, pl.cols, 10);

    /* The jump: two tiles of hole, and the arc over it. */
    dash_arc(r, BBC_CYAN, px_of(&pl, (float)gap_left - 3.0f),
             py_of(&pl, (float)upper - 3.0f), px_of(&pl, (float)gap_right + 3.0f),
             py_of(&pl, (float)upper - 3.0f), 24.0f, 11);
    figure(r, &pl, &SPR_CHUCK_JUMP, gap_left + 6, upper - 14, 1, NULL);

    /* The ladder between the storeys, with someone on the rungs. */
    int ladder_col = 10;
    ladder(r, &pl, ladder_col, upper + 7, floor_row - upper - 7);
    figure(r, &pl, &SPR_CHUCK_CLIMB, ladder_col + 3, floor_row - 9, 1, NULL);

    /* The floor's furniture: something to shove, something not to touch. */
    crate(r, &pl, pl.cols - 12, floor_row);
    arrow(r, &pl, BBC_CYAN, pl.cols - 16, floor_row - 8, -1, 0);
    spikes(r, &pl, 29, floor_row, 4);

    ptext(r, &pl, 8.0f, ROW(floor_row) + 24.0f, BBC_CYAN, "LADDER");
    ptext(r, &pl, 112.0f, ROW(floor_row) + 24.0f, BBC_RED, "SPIKES");
    ptext(r, &pl, COL(pl.cols) - 50.0f, ROW(floor_row) + 24.0f, BBC_CYAN,
          "CRATE");
}

/* The two things worth knowing about a guard: where he is looking, and what
 * happens if you arrive from above. */
static void illus_combat(SDL_Renderer *r, SDL_FRect p, float time,
                         const PadHints *pad,
                         const ManualRecords *records)
{
    (void)records;
    (void)pad;
    Plate pl = plate_of(p);

    /* Upper vignette: the stomp. The two figures split the top of the plate,
     * with the arrow in the clear gap between the boots and the helmet — put
     * on the figure itself it disappears into him. */
    const int ledge = (int)((float)pl.rows * 0.42f);
    const int floor_row = pl.rows - 17;
    back_wall(r, &pl, 0, ledge);
    back_wall(r, &pl, ledge + 7, floor_row);
    room_window(r, &pl, 22, 14, 11, 24);
    room_window(r, &pl, 44, 92, 11, 22);
    ceiling_lamp(r, &pl, 63, 0, ledge);
    slab(r, &pl, 0, ledge, pl.cols, 7);
    ptext(r, &pl, 8.0f, 8.0f, BBC_CYAN, "FROM ABOVE");

    int stomp = (int)((float)pl.cols * 0.56f);
    figure(r, &pl, &SPR_GUARD_STAND, stomp, ledge, -1, BBC_GUARD_PAINT);
    int bob = sinf(time * 2.4f) > 0.0f ? 1 : 0;
    figure(r, &pl, &SPR_CHUCK_FALL, stomp, ledge - 23 + bob, -1, NULL);
    arrow(r, &pl, BBC_YELLOW, stomp - 2, ledge - 21 + bob, 0, 1);
    ptext(r, &pl, 8.0f, ROW(ledge) - 40.0f, BBC_YELLOW, "LAND ON");
    ptext(r, &pl, 8.0f, ROW(ledge) - 28.0f, BBC_YELLOW, "HIS HEAD");

    /* Lower vignette: the cone, and the only side of it worth being on —
     * which is also the side the lamp is on. */
    ceiling_lamp(r, &pl, pl.cols - 12, ledge + 7, floor_row);
    slab(r, &pl, 0, floor_row, pl.cols, 10);
    /* The reach of the cone below, spelled in [manual_pages.h](manual_pages.h)
     * beside the sentence on this same sheet that states it, because it is a
     * number out of `game_config.h` rather than a caption. */
    ptext(r, &pl, 8.0f, ROW(ledge) + 34.0f, BBC_CYAN, MANUAL_SIGHT_CONE_LABEL);

    int guard = (int)((float)pl.cols * 0.46f);
    int eye = floor_row - 12;
    sight_cone(r, &pl, guard - 3, eye, -1, 29, 16, BBC_RED);
    figure(r, &pl, &SPR_GUARD_STAND, guard, floor_row, -1, BBC_GUARD_PAINT);

    /* Chuck behind the cone, answering it. */
    int chuck = pl.cols - 12;
    figure(r, &pl, &SPR_CHUCK_AIM, chuck, floor_row, -1, NULL);
    dash_row(r, &pl, BBC_YELLOW, guard + 4, floor_row - 8, chuck - guard - 10, 2,
             2);
    if (fmodf(time, 1.6f) < 0.12f)
        bbc_sprite(r, &SPR_MUZZLE_FLASH, px_of(&pl, (float)(chuck - 7)),
                   py_of(&pl, (float)(floor_row - 10)), -1, NULL);
}

/*
 * The sheet for the three quiet answers, and it is two vignettes because the
 * three of them are two ideas: put the noise somewhere else, and put the body
 * somewhere else. The blade shares the top with the bolt, since "behind him" is
 * the same sentence the bolt is thrown to create.
 */
static void illus_quiet(SDL_Renderer *r, SDL_FRect p, float time,
                        const PadHints *pad,
                        const ManualRecords *records)
{
    (void)records;
    (void)pad;
    Plate pl = plate_of(p);

    /* Upper: the bolt in the air, and the man turning to where it will land. */
    const int upper = (int)((float)pl.rows * 0.44f);
    const int floor_row = pl.rows - 15;
    back_wall(r, &pl, 0, upper);
    back_wall(r, &pl, upper + 7, floor_row);
    room_window(r, &pl, 30, 100, 12, 24);
    /* The noise where it lands, rings going out from the floor: drawn before
     * the slab, so the slab cuts them off and they read as a sound rather
     * than as a hoop. */
    float ring = 6.0f + 5.0f * (float)((int)(time * 6.0f) % 3);
    bbc_ring(r, BBC_YELLOW, px_of(&pl, (float)(pl.cols - 8)),
             py_of(&pl, (float)upper), ring);
    slab(r, &pl, 0, upper, pl.cols, 7);
    ptext(r, &pl, 8.0f, 8.0f, BBC_CYAN, "THROW IT PAST HIM");

    int thrower = 8;
    int landing = pl.cols - 8;
    figure(r, &pl, &SPR_CHUCK_THROW, thrower, upper, 1, NULL);

    /* The arc, and the bolt riding it. `dash_arc` takes the height the curve
       actually reaches rather than the handle's, which is the revision the
       climb sheet already paid for once. */
    float x0 = px_of(&pl, (float)thrower + 3.0f);
    float y0 = py_of(&pl, (float)upper - 13.0f);
    float x1 = px_of(&pl, (float)landing);
    float y1 = py_of(&pl, (float)upper - 2.0f);
    dash_arc(r, BBC_WHITE, x0, y0, x1, y1, 22.0f, 16);
    float flight = fmodf(time * 0.55f, 1.0f);
    float bolt_x = x0 + (x1 - x0) * flight;
    float bolt_y = y0 + (y1 - y0) * flight - 4.0f * 22.0f * flight * (1.0f - flight);
    bbc_sprite(r, &SPR_ICON_DECOY, bolt_x - 4.0f, bolt_y - 3.0f, 1, NULL);

    /* The guard between them, looking the way the noise went rather than at the
       man who made it — which is the entire mechanic in one figure. */
    int guard = (int)((float)pl.cols * 0.56f);
    sight_cone(r, &pl, guard + 3, upper - 12, 1, 15, 10, BBC_YELLOW);
    figure(r, &pl, &SPR_GUARD_STAND, guard, upper, 1, BBC_GUARD_PAINT);

    /* Lower: the body going out of the room it fell in, toward the dark end
     * of it. */
    slab(r, &pl, 0, floor_row, pl.cols, 9);
    ceiling_lamp(r, &pl, pl.cols - 18, upper + 7, floor_row);
    ptext(r, &pl, 8.0f, ROW(upper) + 34.0f, BBC_CYAN, "AND MOVE WHAT IS LEFT");

    int haul = sinf(time * 1.1f) > 0.0f ? 1 : 0;
    int chuck = 10 + haul;
    figure(r, &pl, &SPR_CHUCK_DRAG, chuck, floor_row, 1, NULL);
    figure(r, &pl, &SPR_GUARD_DOWN, chuck + 9, floor_row, 1, BBC_GUARD_PAINT);

    /* The trail it left, from where it fell to where it is now. */
    dash_row(r, &pl, BBC_CYAN, chuck + 15, floor_row - 2,
             pl.cols - chuck - 19, 2, 2);
    arrow(r, &pl, BBC_CYAN, chuck - 1, floor_row - 22, -1, 0);
}

/*
 * The wall: no gravity, no ladders, and everything trying to take you off it.
 * Laid in the night facade's own paint — a black face in blue courses, white
 * stone ledges iced in cyan — because that is the first wall a player is put
 * on.
 */
static void illus_climb(SDL_Renderer *r, SDL_FRect p, float time,
                        const PadHints *pad,
                        const ManualRecords *records)
{
    (void)records;
    (void)pad;
    Plate pl = plate_of(p);
    const LevelThemePaint *wall = level_theme_paint(LEVEL_THEME_FACADE_NIGHT);

    /* Brick, in courses that break joint. */
    for (int course = 0; course * 5 < pl.rows; ++course)
    {
        int y = course * 5;
        blk(r, &pl, wall->backdrop, 0, y + 4, pl.cols, 1);
        for (int x = (course & 1) ? 3 : 0; x < pl.cols; x += 6)
            blk(r, &pl, wall->backdrop, x, y, 1, 4);
    }

    /* The cornices: the ledge the climber routes around, and the only cover
     * out there. A lit top, a moulding, a row of dentils, and its shadow cut
     * under it. */
    const int lower = pl.rows - 38;
    const int upper_row = 54;
    for (int c = 0; c < 2; ++c)
    {
        int col = c == 0 ? 0 : (int)((float)pl.cols * 0.30f);
        int w = c == 0 ? (int)((float)pl.cols * 0.62f) : pl.cols - col;
        int y = c == 0 ? lower : upper_row;
        blk(r, &pl, wall->wall, col, y, w, 6);
        blk(r, &pl, wall->wall_ink, col, y + 1, w, 2);
        for (int d = col + 1; d < col + w - 1; d += 2)
            blk(r, &pl, BBC_BLACK, d, y + 4, 1, 1);
        blk(r, &pl, BBC_BLACK, col, y + 6, w, 1);
    }

    /* The thrower: a shout and a lean, and only then the brick. The room
     * behind him is lit, and the opening is set in the same stone as the
     * cornices. */
    const int win_col = 3;
    const int win_row = 12;
    blk(r, &pl, wall->wall, win_col - 1, win_row - 2, 12, 2);
    blk(r, &pl, BBC_BLACK, win_col, win_row, 10, 19);
    dith(r, &pl, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, win_col, win_row, 10, 7);
    blk(r, &pl, BBC_YELLOW, win_col + 2, win_row, 6, 1);
    figure(r, &pl, &SPR_GUARD_REACH, win_col + 5, win_row + 19, 1,
           BBC_GUARD_PAINT);
    blk(r, &pl, wall->wall, win_col - 1, win_row + 19, 12, 2);
    blk(r, &pl, BBC_BLACK, win_col - 1, win_row + 21, 12, 1);

    bool wind_up = sinf(time * 2.0f) > 0.0f;
    dash_arc(r, wind_up ? BBC_WHITE : BBC_YELLOW,
             px_of(&pl, (float)win_col + 10.0f), py_of(&pl, (float)win_row + 8.0f),
             px_of(&pl, (float)pl.cols * 0.40f), py_of(&pl, (float)lower - 4.0f),
             26.0f, 18);
    blk(r, &pl, BBC_RED, win_col + 10, win_row + 7, 2, 2);

    /* The climber in the lee of a stub of masonry, with the gust arriving on
     * the far side of it: the shelter has to be between him and the wind or
     * the picture says nothing the text does not have to say twice. */
    int climber = (int)((float)pl.cols * 0.34f) + 3;
    figure(r, &pl, &SPR_CHUCK_CLIMB, climber, lower, 1, NULL);

    int shelter = climber + 6;
    blk(r, &pl, wall->wall, shelter, lower - 21, 5, 15);
    blk(r, &pl, wall->wall_ink, shelter + 3, lower - 21, 2, 15);
    blk(r, &pl, BBC_BLACK, shelter + 5, lower - 20, 1, 14);
    blk(r, &pl, BBC_WHITE, shelter, lower - 21, 5, 1);
    ptext(r, &pl, COL(climber) - 26.0f, ROW(lower) + 20.0f, BBC_YELLOW,
          "SHELTER");

    /* The gust: streaks that stop dead on the masonry. */
    bool gust = sinf(time * 1.3f) > 0.0f;
    int wind_from = pl.cols - 2;
    int wind_to = shelter + 7;
    for (int i = 0; i < 4; ++i)
    {
        int len = (wind_from - wind_to) * (gust ? 10 : 6) / 10;
        dash_row(r, &pl, BBC_CYAN, wind_from - len, lower - 19 + i * 4, len, 2,
                 1);
    }
    arrow(r, &pl, BBC_CYAN, wind_to, lower - 13, -1, 0);
    ptext(r, &pl, COL(pl.cols) - 44.0f, ROW(lower) - 60.0f, BBC_CYAN, "WIND");

    /* A bird, and the height already banked. */
    int bird = (int)((float)pl.cols * 0.72f) + (int)(sinf(time * 0.9f) * 4.0f);
    int heading = cosf(time * 0.9f) >= 0.0f ? 1 : -1;
    bool up = sinf(time * 9.0f) > 0.0f;
    bbc_sprite(r, up ? &SPR_BIRD_UP : &SPR_BIRD_DOWN, px_of(&pl, (float)bird),
               py_of(&pl, 28.0f), heading, NULL);
    ptext(r, &pl, COL(bird) - 4.0f, 40.0f, BBC_WHITE, "BIRD");

    dash_row(r, &pl, BBC_GREEN, 1, lower - 1, 9, 1, 1);
    ptext(r, &pl, 4.0f, ROW(lower) - 16.0f, BBC_GREEN, "BANK");
}

/* Three rounds stood side by side, for the AMMO row of the legend. */
static void draw_icon_ammo(SDL_Renderer *r, float x, float y)
{
    for (int i = 0; i < 3; ++i)
        bbc_sprite(r, &SPR_ICON_ROUND, x + 4.0f + (float)i * 8.0f, y, 1, NULL);
}

static void draw_icon_card(SDL_Renderer *r, float x, float y)
{
    bbc_sprite(r, &SPR_ICON_CARD, x + 4.0f, y - 1.0f, 1, NULL);
}

static void draw_icon_grenade(SDL_Renderer *r, float x, float y)
{
    bbc_sprite(r, &SPR_ICON_GRENADE, x + 8.0f, y, 1, NULL);
}

/* The other half of the grenade, exactly as the floor draws it: a can banded in
   yellow where the frag is a green ball. Told apart at a glance is the whole
   rule — one of the two is about to kill whoever is standing next to it. */
static void draw_icon_flash(SDL_Renderer *r, float x, float y)
{
    bbc_sprite(r, &SPR_ICON_FLASH, x + 8.0f, y, 1, NULL);
}

static void draw_icon_medkit(SDL_Renderer *r, float x, float y)
{
    bbc_sprite(r, &SPR_ICON_MEDKIT, x + 4.0f, y, 1, NULL);
}

static void draw_icon_rocket(SDL_Renderer *r, float x, float y)
{
    bbc_sprite(r, &SPR_ICON_ROCKET, x, y + 2.0f, 1, NULL);
}

/* The strip along the top of the screen, and what is worth picking up. */
static void illus_console(SDL_Renderer *r, SDL_FRect p, float time,
                          const PadHints *pad,
                          const ManualRecords *records)
{
    (void)records;
    (void)pad;
    (void)time;
    Plate pl = plate_of(p);
    float ox = px_of(&pl, 0.0f);

    /* A cutting of the real console: black, framed in blue, the red tab down
     * its left end and a cyan rule along its foot. */
    const int strip = 5;
    blk(r, &pl, BBC_BLUE, 1, strip, pl.cols - 2, 1);
    blk(r, &pl, BBC_CYAN, 1, strip + 20, pl.cols - 2, 1);
    blk(r, &pl, BBC_RED, 1, strip, 1, 21);
    float sy = py_of(&pl, (float)strip);

    /*
     * A cutting of the strip has to be the strip. Three hearts, because three
     * is what a life is and what the sheet before this one says it is — the
     * five sockets this used to draw are the assist option, and a player
     * meeting the diagram before the game read them as two hearts already
     * spent. The lives counter beside them is here for the same reason: the
     * first bullet on the page points at it, so leaving it out made the
     * sentence describe a console the player would not find.
     */
    bbc_text(r, ox + 12.0f, sy + 6.0f, 1.0f, BBC_CYAN, "VITAL");
    for (int i = 0; i < 3; ++i)
        icon(r, &pl, i < 2 ? &SPR_ICON_HEART : &SPR_ICON_HEART_EMPTY, 3 + i * 6,
             strip + 11, 1);
    bbc_text(r, ox + 84.0f, sy + 24.0f, 1.0f, BBC_RED, "x3");
    blk(r, &pl, BBC_BLUE, 26, strip + 3, 1, 15);
    /* The strip labels this field with the weapon's own name rather than with
     * a heading, so the cutting has to as well: a diagram captioned WPN is a
     * diagram of a console the player will never find. */
    bbc_text(r, ox + 112.0f, sy + 6.0f, 1.0f, BBC_CYAN, "PISTOL");
    for (int i = 0; i < 6; ++i)
        icon(r, &pl, i < 4 ? &SPR_ICON_ROUND : &SPR_ICON_ROUND_SPENT,
             28 + i * 2, strip + 11, 1);
    blk(r, &pl, BBC_BLUE, 41, strip + 3, 1, 15);
    bbc_text(r, ox + 172.0f, sy + 6.0f, 1.0f, BBC_CYAN, "ACCESS");
    draw_access_chip(r, ox + 172.0f, sy + 20.0f, 64.0f, BBC_RED,
                     bbc_flash_pair(BBC_RED, BBC_BLACK), BBC_RED, "LOCKED");
    bbc_text(r, ox + 244.0f, sy + 6.0f, 1.0f, BBC_CYAN, "SECTOR");
    /* An interior. Seven is a climb, and a climb draws a different strip
     * altogether — no ACCESS chip on it at all — so a cutting labelled 07 was a
     * console of a sector that never shows one, on the sheet that explains what
     * the chip means. */
    bbc_text(r, ox + 248.0f, sy + 19.0f, 2.0f, BBC_WHITE, "09");

    /* The other two things the chip can say. */
    float alt_y = sy + 54.0f;
    draw_access_chip(r, ox + 8.0f, alt_y, 76.0f, BBC_GREEN, BBC_GREEN,
                     BBC_GREEN, "GRANTED");
    bbc_text(r, ox + 92.0f, alt_y + 3.0f, 1.0f, BBC_WHITE, "DOOR IS OPEN");
    draw_access_chip(r, ox + 8.0f, alt_y + 20.0f, 76.0f, BBC_YELLOW,
                     BBC_YELLOW, BBC_WHITE, "BLOCKED");
    bbc_text(r, ox + 92.0f, alt_y + 23.0f, 1.0f, BBC_WHITE, "USE THE WINDOW");

    /*
     * The legend. One icon, one name, and the two written together so they
     * cannot come apart.
     *
     * They had, and in the way this repository already has a name for: the
     * names were a `[5]` beside a `for (i < 5)` and a `switch` on the index,
     * and the flash charge — added to the game after the other five — was in
     * none of the three. The words on the left of this sheet named the card,
     * the ammo, the medkit, the grenade and the rocket; so did the picture; and
     * the one item in the game that answers *having already been seen* was on
     * neither list, on the one sheet whose subject is what is worth picking up.
     * A pair table written `[]` is the same guard `WEAPON_CYCLE` keeps, and it
     * is the only one available here, because no fit check can read a picture.
     * The drawings are the cast's own icons, so the row is the thing on the
     * floor as well as the thing in the HUD.
     */
    static const struct
    {
        const char *name;
        void (*draw)(SDL_Renderer *, float, float);
    } legend[] = {
        {"KEY CARD", draw_icon_card},
        {"AMMO", draw_icon_ammo},
        {"GRENADE", draw_icon_grenade},
        /* Beside the grenade, where the weapon ring puts it and for the same
           reason: the player choosing between them is choosing what the next
           few seconds are for. */
        {"FLASH", draw_icon_flash},
        {"MEDKIT", draw_icon_medkit},
        {"ROCKET", draw_icon_rocket},
    };
    const int legend_rows = (int)SDL_arraysize(legend);
    float list_y = alt_y + 52.0f;
    for (int i = 0; i < legend_rows; ++i)
    {
        float y = list_y + (float)i * 22.0f;
        bbc_fill(r, BBC_BLUE, ox + 8.0f, y - 5.0f, COL(pl.cols) - 16.0f, 2.0f);
        legend[i].draw(r, ox + 16.0f, y);
        bbc_text(r, ox + 52.0f, y + 2.0f, 1.0f, BBC_WHITE, legend[i].name);
    }

    /* And what the idle trail meter turns into once the building is looking
     * for you, which is the one readout worth recognising in a hurry: red, on
     * the machine's own flash. */
    float alert_y = list_y + (float)legend_rows * 22.0f + 8.0f;
    BbcColour alert = bbc_flash_pair(BBC_RED, BBC_YELLOW);
    bbc_text(r, ox + 8.0f, alert_y, 1.0f, BBC_RED, "SECURITY");
    bbc_fill(r, BBC_RED, ox + 8.0f, alert_y + 12.0f, 124.0f, 16.0f);
    bbc_fill(r, alert, ox + 12.0f, alert_y + 17.0f, 4.0f, 6.0f);
    bbc_text(r, ox + 24.0f, alert_y + 16.0f, 1.0f, BBC_BLACK, "ALERT 07");
    bbc_text(r, ox + 140.0f, alert_y + 16.0f, 1.0f, BBC_WHITE,
             "LOOKING FOR YOU");
}

/*
 * The record sheet: a printed sector-time card with the run figures over it.
 *
 * It is the only illustration in the book that draws a number the player earned
 * rather than a thing in the building, and it is why `ManualRecords` is handed
 * through `manual_render`: the manual links no `Progress` and must not gain one.
 *
 * The grid steps by `RUN_TALLY_SECTOR_CELL_W` and the cells are spelled by
 * `run_tally_format_sector_time`, both of which the suite measures — so a
 * seventeenth sector, or an eighteenth, lands in the column rather than over the
 * one beside it. A NULL `records` draws the same card with every cell reading
 * `--:--`, which is what a fresh install actually has and what `--screen manual`
 * shows on a machine that has never played.
 */
static void illus_record(SDL_Renderer *r, SDL_FRect p, float time,
                         const PadHints *pad,
                         const ManualRecords *records)
{
    (void)pad;
    (void)time;
    /* The card, inset two BBC pixels from the plate all round and framed in
     * blue: the record line under it is the card less twelve pixels of margin
     * either side, which is what `RUN_TALLY_RECORD_LINE_W` is measured as. */
    SDL_FRect card = {p.x + 8.0f, p.y + 12.0f, p.w - 16.0f, p.h - 24.0f};
    bbc_frame(r, BBC_BLUE, card.x, card.y, card.w, card.h);
    /* A printed card: the punched hole it hangs by. */
    bbc_ring(r, BBC_CYAN, card.x + card.w - 28.0f, card.y + 16.0f, 9.0f);

    bbc_text(r, card.x + 12.0f, card.y + 10.0f, 1.0f, BBC_YELLOW,
             "SECTOR TIMES");
    for (float dx = 0.0f; dx < card.w - 24.0f; dx += 8.0f)
        bbc_fill(r, BBC_RED, card.x + 12.0f + dx, card.y + 22.0f, 4.0f, 2.0f);

    /*
     * Two columns, filled down the first before the second, because that is how
     * a list of seventeen is read — and the campaign's own order is the thing
     * being looked up.
     */
    int count = records != NULL ? records->sector_count : CAMPAIGN_SECTORS;
    if (count < 1)
        count = CAMPAIGN_SECTORS;
    if (count > CAMPAIGN_SECTORS)
        count = CAMPAIGN_SECTORS;
    int per_column = (count + 1) / 2;
    float row_pitch = 14.0f;
    float grid_y = card.y + 32.0f;
    for (int row = 0; row < per_column; ++row)
        for (float dx = 0.0f; dx < card.w - 24.0f; dx += 8.0f)
            bbc_fill(r, BBC_BLUE, card.x + 12.0f + dx,
                     grid_y + (float)row * row_pitch + 10.0f, 4.0f, 2.0f);

    for (int i = 0; i < count; ++i)
    {
        int column = i / per_column;
        int row = i % per_column;
        float x = card.x + 14.0f + (float)column * RUN_TALLY_SECTOR_CELL_W;
        float y = grid_y + (float)row * row_pitch;

        float seconds = PROGRESS_NO_TIME;
        if (records != NULL && records->sector_time != NULL &&
            i < records->sector_count)
        {
            seconds = records->sector_time[i];
        }

        char cell[RUN_TALLY_SECTOR_MAX];
        if (run_tally_format_sector_time(i, seconds, cell, sizeof(cell)) <= 0)
            continue;
        /* A sector nobody has finished is set in blue rather than the white
         * the finished ones get: the card reads at a glance as how much of the
         * night has been measured. */
        bool have = seconds > PROGRESS_NO_TIME;
        bbc_text(r, x, y, 1.0f, have ? BBC_WHITE : BBC_BLUE, cell);
    }

    /* The two run figures, under a rule at the foot of the card. */
    float foot = card.y + card.h - 30.0f;
    for (float dx = 0.0f; dx < card.w - 24.0f; dx += 8.0f)
        bbc_fill(r, BBC_CYAN, card.x + 12.0f + dx, foot - 8.0f, 4.0f, 2.0f);

    char line[RUN_TALLY_RECORD_LINE_MAX];
    int best_score = records != NULL ? records->best_score : 0;
    int best_sheets = records != NULL ? records->best_evidence : 0;
    /*
     * Spelled by [run_tally.c](run_tally.c) rather than here, which is the whole
     * reason that file exists — and it took a sweep to notice that these two
     * lines were the one place it had been bypassed. They were an
     * `SDL_snprintf("BEST SCORE %d")` and an `SDL_snprintf("DOCKET %d")`, so a
     * fresh install opened this sheet and read `BEST SCORE 0` and `DOCKET 0`
     * while the options page, reading the same file, said `--`, and while the
     * seventeen cells directly above — which do go through that file — said
     * `--:--`. The card contradicted the page beside it and its own grid, and
     * `DOCKET 0` in particular reads as "your best night carried no sheets"
     * rather than "no night has finished": the very misreading the game-over
     * card's `SCORE 0 - BEST 0` was fixed to stop.
     *
     * Three screens read a record; the two that came through that file agreed,
     * and this one was a renderer literal on the far side of the SDL boundary
     * where nothing could compare it with anything. A record has one spelling
     * wherever it is read.
     *
     * There is no `%d` left on this card, which is also why nothing here reaches
     * for `snprintf`: this file includes no <stdio.h> and compiled for a year
     * only because the platform's SDL headers happened to drag one in — under
     * mingw-w64 they do not, and that is what the Windows cross-build first
     * stopped on.
     */
    if (run_tally_format_record_line(RUN_TALLY_RECORD_SCORE, best_score, line,
                                     sizeof(line)) > 0)
        bbc_text(r, card.x + 12.0f, foot, 1.0f, BBC_CYAN, line);
    if (run_tally_format_record_line(RUN_TALLY_RECORD_DOCKET, best_sheets, line,
                                     sizeof(line)) > 0)
        bbc_text(r, card.x + 12.0f, foot + 13.0f, 1.0f, BBC_MAGENTA, line);
}

/* ---- The sheets ------------------------------------------------------- */

/*
 * What is drawn beside each sheet, in the sheaf's own order.
 *
 * This is the one thing that stayed behind when the text moved to
 * [manual_pages.c](manual_pages.c): an illustration is a function that takes an
 * `SDL_Renderer`, and a table carrying one could not be linked by a suite that
 * links no SDL. Splitting the page in two is what lets `make test` hold every
 * sheet to the column instead of a `CHUCK_DEBUG` assert nobody runs.
 *
 * Indexed the same way `MANUAL_PAGES` is, and the assertion below is what keeps
 * the two arrays the same length — a sheet added to one and not the other would
 * otherwise be a page with somebody else's picture on it.
 *
 * **The array is deliberately unsized, and that is the whole of what makes the
 * assertion below real.** Written `[MANUAL_PAGE_COUNT]`, as it was, the size is
 * the count by declaration and `SDL_arraysize(PAGE_ILLUSTRATIONS) ==
 * MANUAL_PAGE_COUNT` is a tautology no missing row can fail: a short initializer
 * zero-fills the tail, so a sheet added to `MANUAL_PAGES` and not to this table
 * is a **null function pointer** called at `PAGE_ILLUSTRATIONS[index](...)`
 * below. It was exactly that for as long as the assertion existed, and three
 * places said otherwise — this paragraph, the note on `WEAPON_CYCLE` in
 * [player.c](player.c), and [docs/screens.md](../docs/screens.md). Removing
 * `illus_record` from the list built clean, passed `make lint` and all of
 * `make test`, and killed `--screen manual --page 10` with a segmentation
 * fault; only the soak sweep, which walks every sheet, had anything to say. An
 * array whose length is a claim has to be measured against its initializer, not
 * against itself, which is why `WEAPON_CYCLE` next door is written `[]` too.
 */
static void (*const PAGE_ILLUSTRATIONS[])(
    SDL_Renderer *r, SDL_FRect panel, float time, const PadHints *pad,
    const ManualRecords *records) = {
    illus_night,
    illus_crew,
    illus_mission,
    illus_controls,
    illus_movement,
    illus_combat,
    illus_quiet,
    illus_climb,
    illus_console,
    illus_record,
};

_Static_assert(SDL_arraysize(PAGE_ILLUSTRATIONS) == (size_t)MANUAL_PAGE_COUNT,
               "every sheet needs exactly one illustration");


/* ---- Control rows ---------------------------------------------------- */

/* Field `index` of a bar-separated control row. */
static void key_field(const char *text, int index, char *out, size_t size)
{
    const char *start = text;
    for (int field = 0; field < index; ++field)
    {
        const char *bar = SDL_strchr(start, '|');
        if (bar == NULL)
        {
            out[0] = '\0';
            return;
        }
        start = bar + 1;
    }
    const char *end = SDL_strchr(start, '|');
    size_t len = end != NULL ? (size_t)(end - start) : SDL_strlen(start);
    if (len >= size)
        len = size - 1;
    SDL_memcpy(out, start, len);
    out[len] = '\0';
}

/* A pad column entry, spelled for whatever is plugged in. With nothing
 * plugged in the tokens still expand, into the Xbox letters the sheet is
 * written in — an unspelled `$A` on the page would be worse than either. */
static const char *pad_column(const PadHints *pad, const char *field,
                              char *buf, size_t size)
{
    return pad_hint(pad != NULL ? pad : &PAD_HINTS_XBOX, buf, size, field,
                    field);
}

/*
 * The two chip columns are as wide as the widest label on the sheet, not as
 * wide as each row needs: a table whose columns move from row to row is a list
 * of pairs, not a table. The pad column is measured after spelling, because
 * `$START` is six characters and OPTIONS is seven.
 */
static void key_columns(const ManualPageText *page, const PadHints *pad,
                        float *out_key, float *out_pad)
{
    size_t key_max = 0;
    size_t pad_max = 0;
    char buf[32];
    char spelled[32];

    for (int i = 0; i < page->line_count; ++i)
    {
        if (page->lines[i].kind != LINE_KEY)
            continue;
        key_field(page->lines[i].text, 0, buf, sizeof(buf));
        if (SDL_strlen(buf) > key_max)
            key_max = SDL_strlen(buf);
        key_field(page->lines[i].text, 1, buf, sizeof(buf));
        const char *label = pad_column(pad, buf, spelled, sizeof(spelled));
        if (SDL_strlen(label) > pad_max)
            pad_max = SDL_strlen(label);
    }
    *out_key = CH * (float)key_max + 12.0f;
    *out_pad = CH * (float)pad_max + 12.0f;
}

/* ---- Chrome ---------------------------------------------------------- */

static SDL_FRect sheet_rect(float w, float h)
{
    return (SDL_FRect){SHEET_INSET, SHEET_TOP, w - SHEET_INSET * 2.0f,
                       h - SHEET_TOP - SHEET_BOTTOM};
}

static float chip_width(const char *label)
{
    return text_width(label) + 16.0f;
}

static void layout_chips(Manual *manual, float w, float h,
                         const PadHints *pad)
{
    /* The way out is named for the thing in the player's hands, and the chip
     * is sized from that name — a label decided at draw time and a width
     * decided at layout time is how a chip ends up with its text hanging out
     * of it. Kept rather than used in place, so it is copied into the manual's
     * own storage: `label` is a frame of this function and does not outlive
     * it. */
    char label[sizeof(manual->back_label)];
    SDL_strlcpy(manual->back_label,
                pad_hint(pad, label, sizeof(label), "$B BACK", "ESC BACK"),
                sizeof(manual->back_label));

    float dots = (float)MANUAL_PAGE_COUNT * 10.0f;
    float prev_w = chip_width("< PREV");
    float next_w = chip_width("NEXT >");
    float back_w = chip_width(manual->back_label);
    float total = prev_w + 18.0f + dots + 18.0f + next_w + 28.0f + back_w;
    float x = (w - total) * 0.5f;
    float y = h - 46.0f;

    manual->prev_chip = (SDL_FRect){x, y, prev_w, CHIP_H};
    x += prev_w + 18.0f + dots + 18.0f;
    manual->next_chip = (SDL_FRect){x, y, next_w, CHIP_H};
    x += next_w + 28.0f;
    manual->back_chip = (SDL_FRect){x, y, back_w, CHIP_H};
}

static bool in_rect(SDL_FRect box, float x, float y)
{
    return x >= box.x && x <= box.x + box.w &&
           y >= box.y && y <= box.y + box.h;
}

/* ---- Public interface ------------------------------------------------ */

void manual_init(Manual *manual, int win_w, int win_h, const PadHints *pad)
{
    SDL_zerop(manual);
    /* Whether every sheet reaches the frame used to be asked here, under
     * CHUCK_DEBUG, which meant it was only ever answered for whoever opened the
     * book in a debug build. `test_manual_sheets_fit_the_column` asks it of
     * every sheet on every `make test` instead, so a line that would fall off
     * the bottom or run off the side fails the build rather than the reader. */
    layout_chips(manual, win_w > 0 ? (float)win_w : 800.0f,
                 win_h > 0 ? (float)win_h : 552.0f, pad);
}

void manual_update(Manual *manual, float dt, int win_w, int win_h,
                   float mouse_x, float mouse_y, const PadHints *pad)
{
    manual->time += dt;
    if (manual->settle > 0.0f)
    {
        manual->settle -= dt * 4.5f;
        if (manual->settle < 0.0f)
            manual->settle = 0.0f;
    }

    layout_chips(manual, win_w > 0 ? (float)win_w : 800.0f,
                 win_h > 0 ? (float)win_h : 552.0f, pad);
    manual->hovered = manual_hit_test(manual, mouse_x, mouse_y);
}

bool manual_turn_page(Manual *manual, int delta)
{
    int next = manual->page + delta;
    if (next < 0)
        next = 0;
    if (next > MANUAL_PAGE_COUNT - 1)
        next = MANUAL_PAGE_COUNT - 1;
    if (next == manual->page)
        return false;

    manual->settle_dir = next > manual->page ? 1 : -1;
    manual->settle = 1.0f;
    manual->page = next;
    return true;
}

ManualHot manual_hit_test(const Manual *manual, float x, float y)
{
    if (in_rect(manual->prev_chip, x, y))
        return MANUAL_HOT_PREV;
    if (in_rect(manual->next_chip, x, y))
        return MANUAL_HOT_NEXT;
    if (in_rect(manual->back_chip, x, y))
        return MANUAL_HOT_BACK;
    return MANUAL_HOT_NONE;
}

/* ---- Rendering ------------------------------------------------------- */

/*
 * A fade the way the machine did one: the palette walked down its ladder a
 * rung at a time. `level` is how much of something is up, 0 to 1, and the
 * answer is how many rungs short of it the palette has to sit — set before a
 * part of the frame is drawn and put back after, which is how the sheet's
 * words and pictures can come up out of black on a page turn while the frame
 * round them stays where it is.
 */
static int dim_steps(float level)
{
    return (int)ceilf((1.0f - clamp01(level)) * 3.0f - 0.01f);
}

typedef struct
{
    BbcColour entry[BBC_COLOUR_COUNT];
} SavedPalette;

static SavedPalette palette_save(void)
{
    SavedPalette saved;
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        saved.entry[i] = bbc_map((BbcColour)i);
    return saved;
}

static void palette_restore(const SavedPalette *saved)
{
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        bbc_palette_set((BbcColour)i, saved->entry[i]);
}

static void palette_dim_from(const SavedPalette *saved, int steps)
{
    palette_restore(saved);
    bbc_palette_dim(steps);
}

/* The sheet: the whole of the frame, black, inside a blue border with red
 * marks at its corners and a clip across its head. */
static void render_sheet(SDL_Renderer *r, SDL_FRect sheet)
{
    bbc_frame(r, BBC_BLUE, sheet.x, sheet.y, sheet.w, sheet.h);
    draw_brackets(r, BBC_RED, sheet);
    float clip_x = sheet.x + sheet.w * 0.5f - 44.0f;
    bbc_fill(r, BBC_BLUE, clip_x, sheet.y - 4.0f, 88.0f, 8.0f);
    bbc_fill(r, BBC_CYAN, clip_x, sheet.y - 4.0f, 88.0f, 2.0f);
    bbc_fill(r, BBC_BLACK, clip_x + 12.0f, sheet.y, 64.0f, 2.0f);
}

static void render_header(SDL_Renderer *r, const ManualPageText *page,
                          float w)
{
    draw_tracked(r, TEXT_X, 32.0f, 3.0f, BBC_MAGENTA, "FIELD MANUAL");
    bbc_text(r, TEXT_X, 46.0f, 2.0f, BBC_YELLOW, page->title);
    bbc_text(r, TEXT_X, 74.0f, 1.0f, BBC_CYAN, page->strap);

    float rule_y = 88.0f;
    bbc_fill(r, BBC_RED, TEXT_X - 2.0f, rule_y, 64.0f, 2.0f);
    bbc_fill(r, BBC_BLUE, TEXT_X + 70.0f, rule_y, w - TEXT_X * 2.0f - 70.0f,
             2.0f);
}

static void render_text_column(SDL_Renderer *r, const ManualPageText *page,
                               const PadHints *hints, float slide)
{
    float key_w = 0.0f;
    float pad_w = 0.0f;
    key_columns(page, hints, &key_w, &pad_w);

    float x = TEXT_X + slide;
    float y = BODY_Y;
    bool first_head = true;
    char key[32];
    char pad[32];
    char spelled[32];
    char action[48];
    char prose[64];
    char pad_form[64];
    char key_form[64];

    for (int i = 0; i < page->line_count && y < BODY_BOTTOM; ++i)
    {
        const ManualLine *line = &page->lines[i];
        /* A prose line that names a button carries both wordings and is spelled
         * for what is in the player's hands, exactly as the control rows are. */
        const char *body = line->text;
        if (line->text != NULL && SDL_strchr(line->text, '|') != NULL)
        {
            key_field(line->text, 0, pad_form, sizeof(pad_form));
            key_field(line->text, 1, key_form, sizeof(key_form));
            body = pad_hint(hints, prose, sizeof(prose), pad_form, key_form);
        }
        switch (line->kind)
        {
        case LINE_HEAD:
            if (!first_head)
                y += HEAD_LEAD;
            first_head = false;
            draw_tracked(r, x, y, MANUAL_HEAD_TRACK, BBC_YELLOW, line->text);
            bbc_fill(r, BBC_BLUE, x, y + HEAD_RULE_Y, TEXT_RIGHT - TEXT_X, 1.0f);
            y += HEAD_PITCH;
            break;
        case LINE_BULLET:
            bbc_block(r, BBC_CYAN, x + 1.0f, y + 2.0f, 1, 2);
            bbc_text(r, x + BULLET_INDENT, y, 1.0f, BBC_WHITE, body);
            y += LINE_PITCH;
            break;
        case LINE_BODY:
            bbc_text(r, x + BULLET_INDENT, y, 1.0f, BBC_WHITE, body);
            y += LINE_PITCH;
            break;
        case LINE_KEY:
            key_field(line->text, 0, key, sizeof(key));
            key_field(line->text, 1, pad, sizeof(pad));
            key_field(line->text, 2, action, sizeof(action));
            draw_keycap(r, x, y - KEY_CHIP_RISE, key_w, CHIP_H, key, CAP_KEY);
            draw_keycap(r, x + key_w + MANUAL_KEY_CHIP_GAP, y - KEY_CHIP_RISE,
                        pad_w, CHIP_H,
                        pad_column(hints, pad, spelled, sizeof(spelled)),
                        CAP_PAD);
            bbc_text(r, x + key_w + pad_w + MANUAL_KEY_ACTION_GAP, y + 2.0f, 1.0f,
                     BBC_WHITE, action);
            y += KEY_PITCH;
            break;
        case LINE_GAP:
            y += GAP_PITCH;
            break;
        }
    }
}

static void render_panel(SDL_Renderer *r, const ManualPageText *page,
                         int index, float time, float slide,
                         const PadHints *pad, const ManualRecords *records)
{
    SDL_FRect art = {ART_X + slide, ART_Y, ART_W, ART_H};
    SDL_FRect inner = {art.x - 4.0f, art.y - 2.0f, art.w + 8.0f, art.h + 4.0f};
    SDL_FRect outer = {art.x - 8.0f, art.y - 4.0f, art.w + 16.0f,
                       art.h + 8.0f};

    /* A panel that matters gets its border doubled: blue, then cyan. */
    bbc_fill(r, BBC_BLACK, art.x, art.y, art.w, art.h);
    bbc_frame(r, BBC_BLUE, outer.x, outer.y, outer.w, outer.h);
    bbc_frame(r, BBC_CYAN, inner.x, inner.y, inner.w, inner.h);

    PAGE_ILLUSTRATIONS[index](r, art, time, pad, records);

    draw_brackets(r, BBC_RED, outer);

    /* Clipped to the plate's own width rather than trusted to be short: a
     * caption that outgrows the panel runs off the sheet entirely. */
    char caption[CAPTION_MAX + 1];
    SDL_strlcpy(caption, page->caption, sizeof(caption));
    bbc_text(r, CAPTION_X + slide, CAPTION_Y, 1.0f, BBC_CYAN, caption);
}

/*
 * A footer chip: a blue frame round the rectangle the mouse can hit and the
 * label in white; over the pointer, inverse video, which is the machine's own
 * highlight; and a chip with nowhere to go set in blue, the colour of a thing
 * behind glass.
 */
static void render_chip(SDL_Renderer *r, SDL_FRect box, const char *label,
                        bool hot, bool live)
{
    /* The label is centred in the chip as it lands on the grid, for the reason
     * the key caps are. */
    float x0 = bbc_snap_x(box.x);
    box.w = bbc_snap_x(box.x + box.w) - x0;
    box.x = x0;
    if (hot && live)
    {
        bbc_fill(r, BBC_YELLOW, box.x, box.y, box.w, box.h);
        bbc_text(r, box.x + (box.w - text_width(label)) * 0.5f, box.y + 5.0f,
                 1.0f, BBC_BLACK, label);
        return;
    }
    bbc_fill(r, BBC_BLACK, box.x, box.y, box.w, box.h);
    bbc_frame(r, BBC_BLUE, box.x, box.y, box.w, box.h);
    bbc_text(r, box.x + (box.w - text_width(label)) * 0.5f, box.y + 5.0f, 1.0f,
             live ? BBC_WHITE : BBC_BLUE, label);
}

static void render_footer(SDL_Renderer *r, const Manual *manual)
{
    render_chip(r, manual->prev_chip, "< PREV",
                manual->hovered == MANUAL_HOT_PREV, manual->page > 0);
    render_chip(r, manual->next_chip, "NEXT >",
                manual->hovered == MANUAL_HOT_NEXT,
                manual->page < MANUAL_PAGE_COUNT - 1);
    render_chip(r, manual->back_chip, manual->back_label,
                manual->hovered == MANUAL_HOT_BACK, true);

    /* Where in the sheaf this sheet is: a BBC pixel a sheet, every other
     * column, centred in the gap the layout leaves for them — this one tall
     * and yellow, the rest blue. */
    float gap_x = manual->prev_chip.x + manual->prev_chip.w + 18.0f;
    float gap_w = (float)MANUAL_PAGE_COUNT * 10.0f;
    float run = (float)(MANUAL_PAGE_COUNT * 2 - 1) * (float)BBC_PX_W;
    float x = bbc_snap_x(gap_x + (gap_w - run) * 0.5f);
    float y = bbc_snap_y(manual->prev_chip.y + CHIP_H * 0.5f - 2.0f);
    for (int i = 0; i < MANUAL_PAGE_COUNT; ++i)
    {
        bool live = i == manual->page;
        float dx = x + (float)(i * 2 * BBC_PX_W);
        if (live)
            bbc_block(r, BBC_YELLOW, dx, y - 2.0f, 1, 4);
        else
            bbc_block(r, BBC_BLUE, dx, y, 1, 2);
    }

    /* A divider before the way out, so BACK does not read as a third page
     * button. */
    bbc_fill(r, BBC_BLUE, manual->back_chip.x - 14.0f, manual->prev_chip.y + 2.0f,
             4.0f, CHIP_H - 4.0f);
}

void manual_render(SDL_Renderer *r, const Manual *manual, int win_w, int win_h,
                   const PadHints *pad, const ManualRecords *records)
{
    float w = win_w > 0 ? (float)win_w : 800.0f;
    float h = win_h > 0 ? (float)win_h : 552.0f;
    const ManualPageText *page = &MANUAL_PAGES[manual->page];

    float appear = smoothstep01(manual->time / 0.30f);
    /* A turned sheet arrives from the side it came from and settles, a whole
     * BBC pixel at a time. */
    float settle = smoothstep01(manual->settle);
    float slide = bbc_snap_x((float)manual->settle_dir * settle * 16.0f + 2.0f);
    float content = appear * (1.0f - settle * 0.85f);

    SavedPalette saved = palette_save();

    /* Rooms are black, and so is the desk the sheet is on. */
    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, w, h);

    palette_dim_from(&saved, dim_steps(appear));
    render_sheet(r, sheet_rect(w, h));

    palette_dim_from(&saved, dim_steps(content));
    render_header(r, page, w);
    render_text_column(r, page, pad, slide);
    render_panel(r, page, manual->page, manual->time, slide, pad, records);

    palette_dim_from(&saved, dim_steps(appear));
    render_footer(r, manual);

    palette_restore(&saved);
}
