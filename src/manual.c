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
 * The sheet is a thing in the frame.  A wall of type on a flat fill would be
 * the one screen in the game that is not lit, so the manual is a steel-clipped
 * sheet on a dark desk: the plate carries a lit top edge and a dark base, rust
 * corner brackets like the cutscenes use, and the same vignette and scanlines
 * every other frame is finished with.
 *
 * The illustrations are drawn with the game's own vocabulary (fx.h) rather
 * than being diagrams — the figures go through `fx_form_mass`/`fx_form_block`,
 * the legs drop away from the garment, the cornices get an arris.  A manual
 * drawn in a second style would be a manual for a different game.
 */
#include "manual.h"

#include <math.h>

#include "crew.h"
#include "fx.h"
/* For `PROGRESS_NO_TIME`, which is the difference between a sector nobody has
 * finished and one finished in no time at all. The manual reads the constant and
 * never the file. */
#include "progress.h"
#include "run_tally.h"
#include "manual_pages.h"
/* For `draw_decoy`: the bolt on the GOING QUIET sheet is the game's own object
 * rather than a sketch of one, which is the rule every other illustration on
 * this screen already keeps. */
#include "render_figures.h"

/* ---- Palette ---------------------------------------------------------- */

/* The paper stock this screen owns: a steel-blue sheet a step lighter than
 * the room behind it, so the manual reads as a thing under a lamp rather
 * than a mode. Everything else on the desk comes from fx.h. */
static const SDL_Color COL_SHEET = {22, 29, 42, 255};
static const SDL_Color COL_SHEET_LIT = {56, 70, 90, 255};
static const SDL_Color COL_TEXT = {176, 190, 200, 255}; /* body            */
static const SDL_Color COL_COLD = {104, 188, 196, 255}; /* strip lighting  */
/* The keyboard column's caps, and the one light thing on the sheet that is not
 * type: a moulded key is paler and greener than FX_CREAM's paper white and
 * flatter than FX_LABEL's interface grey, because it is a plastic object rather
 * than a printed one. The pad column beside it keeps FX_LABEL on purpose — the
 * two columns are meant to read as different materials. */
static const SDL_Color COL_KEYCAP = {198, 208, 200, 255};

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

#define PANEL_X 444.0f
#define PANEL_Y 100.0f
#define PANEL_W 314.0f
#define PANEL_H 330.0f
#define PANEL_FRAME 6.0f

#define CH ((float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)
/* A chip's height belongs to [manual_pages.h](manual_pages.h) with the rest of
 * the geometry, because the fit check has to know it: a keycap is the deepest
 * ink a control row lays down, and a copy of the number here is how a sheet
 * comes to be certified with its last row in the footer. */
#define CHIP_H MANUAL_CHIP_H
#define KEY_CHIP_RISE MANUAL_KEY_CHIP_RISE
#define HEAD_RULE_Y MANUAL_HEAD_RULE_Y

/* The page module has no SDL under it and so cannot ask SDL how wide a glyph
 * is. It is the same 8x8 cell either way, and this is what says so. */
_Static_assert(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE == (int)MANUAL_CH,
               "the manual's fit checks measure a different glyph than the "
               "renderer draws");

/* ---- Tiny drawing helpers -------------------------------------------- */

static void set_color(SDL_Renderer *r, SDL_Color color)
{
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer *r, float x, float y, float w, float h)
{
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void color_rect(SDL_Renderer *r, SDL_Color color,
                       float x, float y, float w, float h)
{
    set_color(r, color);
    fill_rect(r, x, y, w, h);
}

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

/*
 * The debug font is an 8x8 bitmap, so it is set at scale 1.0 or at a whole
 * multiple of it and never in between: any other scale resamples the glyphs,
 * and a manual is the one screen in the game that is nothing but type.
 */
static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      SDL_Color color, const char *text)
{
    SDL_SetRenderScale(r, scale, scale);
    set_color(r, color);
    SDL_RenderDebugText(r, x / scale, y / scale, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static float text_width(const char *text)
{
    return CH * (float)SDL_strlen(text);
}

/* Letterspaced type, for the few lines that have to read as a label rather
 * than as prose. */
static void draw_tracked(SDL_Renderer *r, float x, float y, float track,
                         SDL_Color color, const char *text)
{
    char glyph[2] = {0, 0};
    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        glyph[0] = text[i];
        draw_text(r, x + (CH + track) * (float)i, y, 1.0f, color, glyph);
    }
}

static void dash_h(SDL_Renderer *r, SDL_Color c, float x, float y, float len,
                   float dash, float gap)
{
    for (float at = 0.0f; at < len; at += dash + gap)
    {
        float run = len - at < dash ? len - at : dash;
        color_rect(r, c, x + at, y, run, 1.0f);
    }
}

static void dash_v(SDL_Renderer *r, SDL_Color c, float x, float y, float len,
                   float dash, float gap)
{
    for (float at = 0.0f; at < len; at += dash + gap)
    {
        float run = len - at < dash ? len - at : dash;
        color_rect(r, c, x, y + at, 1.0f, run);
    }
}

/*
 * A trajectory: pips along a quadratic arc, which is how the game's own
 * grenades and thrown bricks actually travel.
 *
 * `lift` is how high the drawn curve goes, not where the control point sits: a
 * quadratic only rises halfway to its handle, so passing the handle straight
 * through leaves the arc a comfortable distance below whatever it was drawn to
 * meet — a figure hanging over its own jump.
 */
static void dash_arc(SDL_Renderer *r, SDL_Color c, float x0, float y0,
                     float x1, float y1, float lift, int steps, float size)
{
    float mx = (x0 + x1) * 0.5f;
    float my = (y0 + y1) * 0.5f - lift * 2.0f;
    for (int i = 0; i <= steps; ++i)
    {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        float x = u * u * x0 + 2.0f * u * t * mx + t * t * x1;
        float y = u * u * y0 + 2.0f * u * t * my + t * t * y1;
        color_rect(r, c, floorf(x), floorf(y), size, size);
    }
}

/* A chevron, pointing four ways. Used for the nav chips and for the arrow
 * that says which way a stomp goes. */
static void draw_chevron(SDL_Renderer *r, SDL_Color c, float x, float y,
                         int dx, int dy)
{
    for (int i = 0; i < 3; ++i)
    {
        float span = (float)(6 - i * 2);
        if (dy != 0)
            color_rect(r, c, x - span * 0.5f, y + (float)(dy > 0 ? i : 2 - i) * 2.0f,
                       span, 2.0f);
        else
            color_rect(r, c, x + (float)(dx > 0 ? i : 2 - i) * 2.0f,
                       y - span * 0.5f, 2.0f, span);
    }
}

/* A keycap: dark bezel, lit top edge, sunken base — the same cap the title
 * screen puts its control hints on. */
static void draw_keycap(SDL_Renderer *r, float x, float y, float w, float h,
                        const char *label, SDL_Color face)
{
    color_rect(r, FX_INK, x, y, w, h);
    color_rect(r, (SDL_Color){32, 42, 56, 255}, x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f);
    color_rect(r, (SDL_Color){68, 84, 102, 255}, x + 1.0f, y + 1.0f, w - 2.0f, 1.0f);
    color_rect(r, FX_NIGHT, x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f);
    if (label != NULL && label[0] != '\0')
        draw_text(r, x + (w - text_width(label)) * 0.5f,
                  y + (h - CH) * 0.5f + 1.0f, 1.0f, face, label);
}

/* Rust corner brackets: the marker the cutscenes frame things with. */
static void draw_brackets(SDL_Renderer *r, SDL_Color c, SDL_FRect box,
                          float arm, float weight)
{
    color_rect(r, c, box.x, box.y, arm, weight);
    color_rect(r, c, box.x, box.y, weight, arm);
    color_rect(r, c, box.x + box.w - arm, box.y, arm, weight);
    color_rect(r, c, box.x + box.w - weight, box.y, weight, arm);
    color_rect(r, c, box.x, box.y + box.h - weight, arm, weight);
    color_rect(r, c, box.x, box.y + box.h - arm, weight, arm);
    color_rect(r, c, box.x + box.w - arm, box.y + box.h - weight, arm, weight);
    color_rect(r, c, box.x + box.w - weight, box.y + box.h - arm, weight, arm);
}

/* ---- Small figures ---------------------------------------------------- */

/*
 * The cast, at twelve pixels across.
 *
 * Same three passes as the game: a tapered mass for the head, form-shaded
 * blocks for the body, and the legs a long way under the garment so the torso
 * is the mass the eye lands on. The ink pass around each block is the one
 * concession to the illustration panels — a figure this small on a dark plate
 * needs the silhouette that the world's ambient occlusion gives it in a level.
 */
#define FIG_W 12.0f
#define FIG_H 26.0f

typedef enum
{
    POSE_STAND,
    POSE_LOOK_UP,
    POSE_AIM,
    POSE_JUMP,
    POSE_CLIMB, /* rear view, on the rungs */
    POSE_CLING  /* rear view, flat against a wall */
} FigurePose;

typedef struct
{
    SDL_Color garment;
    SDL_Color legs;
    SDL_Color cap;
    bool helmet; /* a helmet covers the crown; a headband is one row */
} FigureLook;

static FigureLook look_chuck(void)
{
    return (FigureLook){FX_HERO, {26, 32, 46, 255}, FX_RUST, false};
}

static FigureLook look_guard(void)
{
    return (FigureLook){FX_GUARD, {30, 34, 30, 255}, FX_GUARD_DK, true};
}

static void ink_block(SDL_Renderer *r, float x, float y, float w, float h)
{
    color_rect(r, FX_INK, x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f);
}

/* The outline under a tapered mass follows the same taper one pixel further
 * out. A rounded fill inside a square outline is still a box with something
 * drawn in it — the silhouette itself has to lose its corners. */
static void ink_mass(SDL_Renderer *r, float x, float y, float w, float h,
                     int top, int bottom)
{
    fx_mass(r, FX_INK, x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f,
            top + 1, bottom + 1);
}

static void draw_figure(SDL_Renderer *r, float x, float feet_y, int dir,
                        FigurePose pose, FigureLook look, float time)
{
    FxRamp coat = fx_ramp(look.garment);
    FxRamp trouser = fx_ramp(look.legs);
    FxRamp skin = fx_ramp(FX_SKIN);
    bool rear = pose == POSE_CLIMB || pose == POSE_CLING;

    float head_y = feet_y - FIG_H;
    float torso_y = head_y + 8.0f;
    float leg_y = torso_y + 10.0f;
    float leg_h = 8.0f;
    float lead_leg = 0.0f;
    float rear_leg = 0.0f;

    switch (pose)
    {
    case POSE_CLIMB:
        lead_leg = 2.0f;
        break;
    case POSE_CLING:
        lead_leg = 3.0f;
        rear_leg = -1.0f;
        break;
    case POSE_JUMP:
        lead_leg = (float)dir * 3.0f;
        rear_leg = -(float)dir * 2.0f;
        leg_h = 6.0f;
        break;
    default:
        break;
    }

    ink_block(r, x + 1.0f + rear_leg, leg_y, 4.0f, leg_h);
    ink_block(r, x + 7.0f + lead_leg, leg_y, 4.0f, leg_h);
    fx_form_block(r, x + 1.0f + rear_leg, leg_y, 4.0f, leg_h, trouser, dir);
    fx_form_block(r, x + 7.0f + lead_leg, leg_y, 4.0f, leg_h, trouser, dir);

    /* Arms before the torso where they hang behind it, after it where they
     * reach out in front. */
    if (rear)
    {
        ink_block(r, x - 2.0f, torso_y - 3.0f, 3.0f, 8.0f);
        ink_block(r, x + FIG_W - 1.0f, torso_y - 1.0f, 3.0f, 8.0f);
        fx_form_block(r, x - 2.0f, torso_y - 3.0f, 3.0f, 8.0f, coat, dir);
        fx_form_block(r, x + FIG_W - 1.0f, torso_y - 1.0f, 3.0f, 8.0f, coat, dir);
    }

    ink_mass(r, x, torso_y, FIG_W, 10.0f, 2, 0);
    fx_form_mass(r, x, torso_y, FIG_W, 10.0f, coat, dir, 2, 0);

    if (pose == POSE_AIM)
    {
        float arm_x = dir > 0 ? x + FIG_W : x - 6.0f;
        ink_block(r, arm_x, torso_y + 3.0f, 6.0f, 3.0f);
        fx_form_block(r, arm_x, torso_y + 3.0f, 6.0f, 3.0f, coat, dir);
        color_rect(r, FX_STEEL_LT, dir > 0 ? arm_x + 6.0f : arm_x - 3.0f,
                   torso_y + 3.0f, 3.0f, 2.0f);
    }
    else if (pose == POSE_JUMP)
    {
        float arm_x = dir > 0 ? x + FIG_W - 1.0f : x - 2.0f;
        ink_block(r, arm_x, torso_y - 4.0f, 3.0f, 8.0f);
        fx_form_block(r, arm_x, torso_y - 4.0f, 3.0f, 8.0f, coat, dir);
    }
    else if (!rear)
    {
        float arm_x = dir > 0 ? x + FIG_W - 1.0f : x - 2.0f;
        ink_block(r, arm_x, torso_y + 2.0f, 3.0f, 8.0f);
        fx_form_block(r, arm_x, torso_y + 2.0f, 3.0f, 8.0f, coat, dir);
    }

    /* The head last, and whatever is worn on it after the face, so the fill
     * covers the face's own top outline row instead of being cut by it. */
    ink_mass(r, x + 2.0f, head_y, 8.0f, 7.0f, 2, 2);
    fx_form_mass(r, x + 2.0f, head_y, 8.0f, 7.0f, skin, dir, 2, 2);

    if (!rear)
    {
        float eye_x = dir > 0 ? x + 6.0f : x + 4.0f;
        float eye_y = head_y + (pose == POSE_LOOK_UP ? 2.0f : 3.0f);
        /* The diagrams are drawings of the people in the game, and the
           people in the game blink; salted by post so the sheet's figures
           never blink in unison. */
        if (fx_blinking(time, fx_salt(x * 7.0f + feet_y)))
            color_rect(r, skin.dark, eye_x, eye_y, 2.0f, 1.0f);
        else
        {
            color_rect(r, FX_CREAM, eye_x, eye_y, 2.0f, 1.0f);
            color_rect(r, FX_INK, dir > 0 ? eye_x + 1.0f : eye_x, eye_y,
                       1.0f, 1.0f);
        }
        /* The nose has to break the outline or the head is not a profile. */
        color_rect(r, skin.base, dir > 0 ? x + 10.0f : x + 1.0f,
                   eye_y + 1.0f, 1.0f, 1.0f);
    }

    if (look.helmet)
        fx_mass(r, look.cap, x + 1.0f, head_y - 1.0f, 10.0f, 4.0f, 2, 0);
    else
    {
        fx_mass(r, FX_HAIR, x + 2.0f, head_y, 8.0f, 2.0f, 2, 0);
        fx_mass(r, look.cap, x + 2.0f, head_y + 1.0f, 8.0f, 2.0f, 1, 0);
    }
}

/* ---- Props ------------------------------------------------------------ */

/* A slab: the material, then the shading, then the arris on top — the order a
 * wall tile is built in, because a highlight dimmed by the shading pass stops
 * being one. */
static void draw_slab(SDL_Renderer *r, float x, float y, float w, float h)
{
    fx_vgrad(r, x, y, w, h, FX_STEEL_DK, 255,
             (SDL_Color){24, 31, 43, 255}, 255);
    color_rect(r, (SDL_Color){126, 142, 156, 255}, x, y, w, 1.0f);
    color_rect(r, (SDL_Color){14, 19, 28, 255}, x, y + h - 1.0f, w, 1.0f);
    for (float rivet = x + 8.0f; rivet < x + w - 4.0f; rivet += 22.0f)
        color_rect(r, FX_STEEL, rivet, y + 5.0f, 2.0f, 2.0f);
}

static void draw_ladder(SDL_Renderer *r, float x, float y, float h)
{
    color_rect(r, FX_STEEL_DK, x, y, 3.0f, h);
    color_rect(r, FX_STEEL_DK, x + 15.0f, y, 3.0f, h);
    color_rect(r, FX_STEEL_LT, x, y, 1.0f, h);
    color_rect(r, FX_STEEL_LT, x + 15.0f, y, 1.0f, h);
    for (float rung = y + 5.0f; rung < y + h - 2.0f; rung += 9.0f)
    {
        color_rect(r, FX_STEEL, x + 3.0f, rung, 12.0f, 2.0f);
        color_rect(r, FX_STEEL_LT, x + 3.0f, rung, 12.0f, 1.0f);
    }
}

static void draw_crate(SDL_Renderer *r, float x, float y, float size)
{
    FxRamp wood = fx_ramp(FX_WOOD);
    ink_block(r, x, y, size, size);
    fx_form_block(r, x, y, size, size, wood, 1);
    color_rect(r, FX_WOOD_DK, x + 2.0f, y + size * 0.5f - 1.0f, size - 4.0f, 2.0f);
    color_rect(r, FX_WOOD_LT, x + 2.0f, y + size * 0.5f - 1.0f, size - 4.0f, 1.0f);
}

static void draw_spikes(SDL_Renderer *r, float x, float y, int count)
{
    for (int i = 0; i < count; ++i)
    {
        float sx = x + (float)i * 11.0f;
        for (int row = 0; row < 7; ++row)
        {
            float w = 7.0f - (float)row;
            SDL_Color tip = fx_mix(FX_PALE, FX_STEEL_DK, (float)row / 7.0f);
            color_rect(r, tip, sx + (7.0f - w) * 0.5f, y + 7.0f - (float)row - 1.0f,
                       w, 1.0f);
        }
        color_rect(r, FX_STEEL_DK, sx - 1.0f, y + 6.0f, 9.0f, 2.0f);
    }
}

/*
 * A guard's sight cone. It fades out along its length rather than ending on a
 * line, because the range is a falloff the player feels rather than a fence
 * drawn on the floor.
 */
static void draw_sight_cone(SDL_Renderer *r, float ex, float ey, int dir,
                            float len, float half, SDL_Color c, Uint8 alpha)
{
    SDL_FColor near_c = fx_fcolor(c, (float)alpha / 255.0f);
    SDL_FColor far_c = fx_fcolor(c, 0.0f);
    float fx_end = ex + (float)dir * len;
    SDL_Vertex v[4] = {
        {{ex, ey - 3.0f}, near_c, {0.0f, 0.0f}},
        {{fx_end, ey - half}, far_c, {0.0f, 0.0f}},
        {{fx_end, ey + half}, far_c, {0.0f, 0.0f}},
        {{ex, ey + 3.0f}, near_c, {0.0f, 0.0f}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* Brick, in courses that break joint — the archive's material, and the one
 * every facade is climbed on. */
static void draw_brickwork(SDL_Renderer *r, SDL_FRect p)
{
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){46, 38, 38, 255}, 255,
             (SDL_Color){28, 24, 28, 255}, 255);
    int course = 0;
    for (float y = p.y; y < p.y + p.h; y += 9.0f, ++course)
    {
        float offset = (course % 2) ? -11.0f : 0.0f;
        /* A header course every fifth run is what gives the wall a scale; the
         * panel grid alone only says how big a brick is. */
        bool header = (course % 5) == 0;
        for (float x = p.x + offset; x < p.x + p.w; x += 22.0f)
        {
            float w = header ? 10.0f : 21.0f;
            float step = header ? 11.0f : 22.0f;
            for (float bx = x; bx < x + 22.0f && bx < p.x + p.w; bx += step)
            {
                unsigned h = fx_hash(fx_salt(bx * 3.0f + y * 71.0f));
                SDL_Color face = fx_dim((SDL_Color){86, 62, 54, 255},
                                        0.62f + (float)(h % 30u) * 0.011f);
                float left = bx < p.x ? p.x : bx;
                float span = bx + w > p.x + p.w ? p.x + p.w - left : w - (left - bx);
                if (span > 0.0f)
                    color_rect(r, face, left, y, span, 8.0f);
            }
        }
    }
}

/* A cornice: the ledge the climber routes around, and the only cover out
 * there. */
static void draw_cornice(SDL_Renderer *r, float x, float y, float w)
{
    fx_vgrad(r, x, y, w, 13.0f, (SDL_Color){112, 108, 98, 255}, 255,
             (SDL_Color){46, 44, 42, 255}, 255);
    color_rect(r, (SDL_Color){186, 180, 164, 255}, x, y, w, 1.0f);
    color_rect(r, (SDL_Color){22, 20, 20, 255}, x, y + 12.0f, w, 1.0f);
    color_rect(r, (SDL_Color){138, 132, 120, 255}, x, y + 3.0f, w, 1.0f);
}

/* ---- Pickup icons ---------------------------------------------------- */

static void draw_icon_card(SDL_Renderer *r, float x, float y)
{
    color_rect(r, FX_INK, x - 1.0f, y - 1.0f, 18.0f, 13.0f);
    color_rect(r, FX_CYAN_DK, x, y, 16.0f, 11.0f);
    color_rect(r, FX_CYAN, x, y, 16.0f, 1.0f);
    color_rect(r, (SDL_Color){12, 40, 44, 255}, x + 2.0f, y + 3.0f, 12.0f, 3.0f);
    color_rect(r, FX_CREAM, x + 2.0f, y + 7.0f, 5.0f, 2.0f);
}

static void draw_icon_ammo(SDL_Renderer *r, float x, float y)
{
    color_rect(r, FX_INK, x - 1.0f, y - 1.0f, 18.0f, 13.0f);
    color_rect(r, FX_STEEL_DK, x, y, 16.0f, 11.0f);
    color_rect(r, FX_STEEL_LT, x, y, 16.0f, 1.0f);
    for (int i = 0; i < 3; ++i)
    {
        float bx = x + 3.0f + (float)i * 4.0f;
        color_rect(r, fx_ramp(FX_AMBER).lit, bx, y + 3.0f, 3.0f, 2.0f);
        color_rect(r, fx_dim(FX_AMBER, 0.90f), bx, y + 5.0f, 3.0f, 4.0f);
    }
}

static void draw_icon_grenade(SDL_Renderer *r, float x, float y)
{
    color_rect(r, FX_INK, x + 3.0f, y - 1.0f, 10.0f, 13.0f);
    fx_mass(r, FX_GREEN_DK, x + 4.0f, y + 1.0f, 8.0f, 9.0f, 2, 2);
    color_rect(r, fx_dim(FX_GREEN, 0.75f), x + 5.0f, y + 2.0f, 4.0f, 1.0f);
    color_rect(r, FX_STEEL, x + 6.0f, y - 1.0f, 4.0f, 2.0f);
    color_rect(r, FX_AMBER_DK, x + 10.0f, y - 1.0f, 3.0f, 1.0f);
}

/* The other half of the grenade's palette, exactly as `draw_flashbang` is: same
   silhouette, steel and a white band instead of olive and brass, and a cyan
   tell-tale where the spoon is. Told apart at a glance is the whole rule — one
   of the two is about to kill whoever is standing next to it. */
static void draw_icon_flash(SDL_Renderer *r, float x, float y)
{
    color_rect(r, FX_INK, x + 3.0f, y - 1.0f, 10.0f, 13.0f);
    fx_mass(r, FX_STEEL, x + 4.0f, y + 1.0f, 8.0f, 9.0f, 2, 2);
    color_rect(r, FX_CREAM, x + 4.0f, y + 4.0f, 8.0f, 2.0f);
    color_rect(r, FX_STEEL_LT, x + 5.0f, y + 2.0f, 2.0f, 1.0f);
    color_rect(r, FX_STEEL, x + 6.0f, y - 1.0f, 4.0f, 2.0f);
    color_rect(r, FX_CYAN, x + 10.0f, y - 1.0f, 3.0f, 1.0f);
}

static void draw_icon_medkit(SDL_Renderer *r, float x, float y)
{
    color_rect(r, FX_INK, x - 1.0f, y - 1.0f, 18.0f, 13.0f);
    color_rect(r, (SDL_Color){214, 220, 214, 255}, x, y, 16.0f, 11.0f);
    color_rect(r, FX_CREAM, x, y, 16.0f, 1.0f);
    color_rect(r, FX_RED, x + 6.0f, y + 2.0f, 4.0f, 7.0f);
    color_rect(r, FX_RED, x + 3.0f, y + 4.0f, 10.0f, 3.0f);
}

static void draw_icon_rocket(SDL_Renderer *r, float x, float y)
{
    color_rect(r, FX_INK, x - 1.0f, y + 1.0f, 18.0f, 9.0f);
    color_rect(r, FX_STEEL, x, y + 2.0f, 12.0f, 6.0f);
    color_rect(r, FX_STEEL_LT, x, y + 2.0f, 12.0f, 1.0f);
    color_rect(r, FX_RED, x + 12.0f, y + 3.0f, 4.0f, 4.0f);
    color_rect(r, FX_AMBER, x, y + 3.0f, 2.0f, 4.0f);
}

/* ---- Console parts --------------------------------------------------- */

/* The heart and the cartridge are the HUD's own glyphs out of fx.h — the
   manual is a diagram of that console, and a diagram of a different heart
   teaches a different game. */
static void draw_heart(SDL_Renderer *r, float x, float y, bool full)
{
    fx_heart(r, x, y, 1.0f, full);
}

static void draw_ammo_pip(SDL_Renderer *r, float x, float y, bool full)
{
    fx_ammo_pip(r, x, y, full);
}

/* One of the three states the ACCESS chip shows, drawn the way the HUD draws
 * it so the manual and the strip cannot disagree. */
static void draw_access_chip(SDL_Renderer *r, float x, float y, float w,
                             SDL_Color led, SDL_Color body, SDL_Color edge,
                             SDL_Color text, const char *label)
{
    color_rect(r, body, x, y, w, 13.0f);
    color_rect(r, edge, x, y, w, 1.0f);
    color_rect(r, led, x + 4.0f, y + 5.0f, 3.0f, 3.0f);
    draw_text(r, x + 11.0f, y + 3.0f, 1.0f, text, label);
}

/* ---- Illustrations --------------------------------------------------- */

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
 * drawn from above, and this game has exactly one camera.
 */
static void illus_night(SDL_Renderer *r, SDL_FRect p, float time,
                        const PadHints *pad,
                        const ManualRecords *records)
{
    (void)records;
    (void)pad;
    (void)time;
    float cx = p.x + p.w * 0.5f;
    float floor_y = p.y + p.h - 40.0f;
    float top_y = floor_y - 244.0f;
    SDL_FRect lid = {cx - 146.0f, top_y, 138.0f, 244.0f};
    SDL_FRect bed = {cx + 2.0f, top_y, 148.0f, 244.0f};
    SDL_Color shell = {49, 55, 58, 255};
    SDL_Color foam = {35, 40, 39, 255};
    SDL_Color cut = {14, 18, 19, 255};

    fx_vgrad(r, p.x, p.y, p.w, p.h, FX_NIGHT, 255,
             (SDL_Color){21, 27, 37, 255}, 255);
    /* One lamp above and to the left, the same direction every lit thing in
       the game is lit from. */
    fx_glow(r, p.x + p.w * 0.30f, p.y - 30.0f, 260.0f,
            (SDL_Color){146, 168, 190, 255}, 32);
    color_rect(r, (SDL_Color){26, 32, 40, 255}, p.x, floor_y, p.w,
               p.y + p.h - floor_y);
    color_rect(r, (SDL_Color){58, 68, 76, 255}, p.x, floor_y, p.w, 1.0f);
    fx_contact_shadow(r, cx, floor_y + 1.0f, 158.0f, 0.0f, 200);

    for (int half = 0; half < 2; ++half)
    {
        SDL_FRect box = half == 0 ? lid : bed;
        ink_block(r, box.x, box.y, box.w, box.h);
        fx_vgrad(r, box.x, box.y, box.w, box.h, shell, 255,
                 fx_mix(shell, FX_INK, 0.55f), 255);
        color_rect(r, (SDL_Color){118, 130, 136, 255}, box.x, box.y, box.w, 1.0f);
        color_rect(r, FX_INK, box.x, box.y + box.h - 2.0f, box.w, 2.0f);
        /* Corner protectors and two latches down the outer edge. */
        float edge = half == 0 ? box.x : box.x + box.w - 5.0f;
        color_rect(r, fx_mix(shell, FX_STEEL_LT, 0.4f), edge, box.y, 5.0f, 20.0f);
        color_rect(r, fx_mix(shell, FX_STEEL_LT, 0.4f), edge,
                   box.y + box.h - 22.0f, 5.0f, 20.0f);
        for (int i = 0; i < 2; ++i)
        {
            float ly = box.y + 66.0f + (float)i * 112.0f;
            color_rect(r, FX_STEEL_DK, edge - 2.0f, ly, 9.0f, 16.0f);
            color_rect(r, FX_STEEL_LT, edge - 2.0f, ly, 9.0f, 1.0f);
        }
        color_rect(r, FX_RUST, box.x + (half == 0 ? 4.0f : box.w - 13.0f),
                   box.y + box.h - 44.0f, 9.0f, 3.0f);
    }

    /* The lid liner, so the docket is stencilled on something. */
    color_rect(r, (SDL_Color){31, 36, 39, 255}, lid.x + 9.0f, lid.y + 10.0f,
               lid.w - 18.0f, lid.h - 24.0f);
    color_rect(r, (SDL_Color){17, 21, 24, 255}, lid.x + 9.0f, lid.y + 10.0f,
               lid.w - 18.0f, 1.0f);

    draw_tracked(r, lid.x + 18.0f, lid.y + 26.0f, 2.0f,
                 (SDL_Color){182, 176, 150, 255}, "MERIDIAN");
    draw_text(r, lid.x + 18.0f, lid.y + 44.0f, 1.0f,
              (SDL_Color){128, 130, 118, 255}, "FACILITY");
    draw_text(r, lid.x + 18.0f, lid.y + 56.0f, 1.0f,
              (SDL_Color){128, 130, 118, 255}, "SERVICES");
    color_rect(r, fx_dim(FX_RUST, 0.85f), lid.x + 18.0f, lid.y + 72.0f,
               64.0f, 2.0f);
    draw_text(r, lid.x + 18.0f, lid.y + 88.0f, 1.0f, FX_LABEL, "NIGHT SHIFT");
    draw_text(r, lid.x + 18.0f, lid.y + 102.0f, 1.0f, FX_LABEL, "TOOL SET 07");
    draw_text(r, lid.x + 18.0f, lid.y + 128.0f, 1.0f,
              (SDL_Color){92, 100, 104, 255}, "SIGNED IN");
    draw_text(r, lid.x + 18.0f, lid.y + 142.0f, 1.0f,
              (SDL_Color){148, 156, 154, 255}, "14 MARCH");

    /* The docket bar, and the line on it that is the whole plot. */
    for (int i = 0; i < 22; ++i)
    {
        unsigned h = fx_hash((unsigned)i * 2654435761u + 91u);
        color_rect(r, (SDL_Color){158, 162, 156, 255},
                   lid.x + 18.0f + (float)i * 4.0f, lid.y + 170.0f,
                   (h & 1u) ? 2.0f : 1.0f, 18.0f);
    }
    draw_text(r, lid.x + 18.0f, lid.y + 196.0f, 1.0f, fx_dim(FX_RUST, 0.95f),
              "NOT INSPECTED");

    /* The foam bed, and the five cutouts that are not tools. */
    color_rect(r, foam, bed.x + 7.0f, bed.y + 8.0f, bed.w - 14.0f,
               bed.h - 22.0f);
    color_rect(r, fx_mix(foam, FX_INK, 0.5f), bed.x + 7.0f, bed.y + 8.0f,
               bed.w - 14.0f, 1.0f);

    /* Rifle. */
    color_rect(r, cut, bed.x + 12.0f, bed.y + 16.0f, bed.w - 24.0f, 30.0f);
    color_rect(r, FX_INK, bed.x + 20.0f, bed.y + 28.0f, 104.0f, 5.0f);
    color_rect(r, (SDL_Color){76, 83, 80, 255}, bed.x + 20.0f, bed.y + 28.0f,
               104.0f, 1.0f);
    color_rect(r, FX_INK, bed.x + 48.0f, bed.y + 25.0f, 28.0f, 10.0f);
    color_rect(r, (SDL_Color){66, 60, 45, 255}, bed.x + 52.0f, bed.y + 34.0f,
               10.0f, 9.0f);
    color_rect(r, FX_INK, bed.x + 16.0f, bed.y + 24.0f, 14.0f, 9.0f);

    /* Launcher tube. */
    color_rect(r, cut, bed.x + 12.0f, bed.y + 52.0f, bed.w - 24.0f, 34.0f);
    fx_vgrad(r, bed.x + 18.0f, bed.y + 60.0f, 112.0f, 17.0f,
             (SDL_Color){80, 76, 53, 255}, 255,
             (SDL_Color){36, 34, 26, 255}, 255);
    color_rect(r, (SDL_Color){122, 114, 76, 255}, bed.x + 18.0f, bed.y + 60.0f,
               112.0f, 1.0f);
    color_rect(r, FX_INK, bed.x + 18.0f, bed.y + 57.0f, 6.0f, 23.0f);
    color_rect(r, FX_INK, bed.x + 124.0f, bed.y + 57.0f, 6.0f, 23.0f);
    color_rect(r, FX_RUST, bed.x + 64.0f, bed.y + 65.0f, 16.0f, 3.0f);

    /* Four frags in their own cups. */
    color_rect(r, cut, bed.x + 12.0f, bed.y + 92.0f, bed.w - 24.0f, 42.0f);
    for (int i = 0; i < 4; ++i)
    {
        float gx = bed.x + 24.0f + (float)i * 28.0f;
        color_rect(r, (SDL_Color){9, 12, 12, 255}, gx - 3.0f, bed.y + 98.0f,
                   20.0f, 30.0f);
        fx_mass(r, (SDL_Color){57, 64, 47, 255}, gx, bed.y + 104.0f, 14.0f,
                20.0f, 2, 2);
        color_rect(r, (SDL_Color){86, 94, 70, 255}, gx + 2.0f, bed.y + 106.0f,
                   6.0f, 1.0f);
        color_rect(r, FX_STEEL_DK, gx + 4.0f, bed.y + 100.0f, 6.0f, 5.0f);
        color_rect(r, FX_STEEL_LT, gx + 10.0f, bed.y + 101.0f, 2.0f, 11.0f);
    }

    /* Rockets, nose down. */
    color_rect(r, cut, bed.x + 12.0f, bed.y + 140.0f, bed.w - 24.0f, 44.0f);
    for (int i = 0; i < 3; ++i)
    {
        float rx = bed.x + 28.0f + (float)i * 36.0f;
        color_rect(r, FX_INK, rx - 1.0f, bed.y + 147.0f, 16.0f, 30.0f);
        fx_mass(r, (SDL_Color){70, 64, 46, 255}, rx, bed.y + 148.0f, 14.0f,
                12.0f, 3, 0);
        color_rect(r, (SDL_Color){44, 48, 46, 255}, rx + 2.0f, bed.y + 160.0f,
                   10.0f, 16.0f);
        color_rect(r, FX_RUST, rx + 2.0f, bed.y + 163.0f, 10.0f, 2.0f);
    }

    /* Magazines, stacked flat. */
    color_rect(r, cut, bed.x + 12.0f, bed.y + 190.0f, bed.w - 24.0f, 40.0f);
    for (int i = 0; i < 5; ++i)
    {
        float my = bed.y + 196.0f + (float)i * 7.0f;
        color_rect(r, FX_INK, bed.x + 20.0f, my, 112.0f, 6.0f);
        color_rect(r, (SDL_Color){58, 64, 62, 255}, bed.x + 21.0f, my + 1.0f,
                   110.0f, 4.0f);
        color_rect(r, (SDL_Color){90, 98, 94, 255}, bed.x + 21.0f, my + 1.0f,
                   110.0f, 1.0f);
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
 * in, twelve accounted for, and a signature block nobody ever checked.
 */
static void illus_crew(SDL_Renderer *r, SDL_FRect p, float time,
                       const PadHints *pad,
                       const ManualRecords *records)
{
    (void)records;
    (void)pad;
    (void)time;
    float cx = p.x + p.w * 0.5f;
    SDL_Color paper = {168, 170, 158, 255};
    SDL_Color ink = {42, 46, 46, 255};
    SDL_Color faint = {116, 120, 112, 255};

    fx_vgrad(r, p.x, p.y, p.w, p.h, FX_NIGHT, 255,
             (SDL_Color){21, 27, 37, 255}, 255);
    fx_glow(r, p.x + p.w * 0.30f, p.y - 30.0f, 260.0f,
            (SDL_Color){146, 168, 190, 255}, 32);

    const float bw = 232.0f;
    const float bh = 296.0f;
    float bx = cx - bw * 0.5f;
    float by = p.y + 14.0f;
    fx_contact_shadow(r, cx, by + bh + 3.0f, 150.0f, 0.0f, 195);

    /* Hardboard behind, docket clipped over it: the board is what makes this
       a thing on a desk rather than a page of the manual with a border. */
    ink_block(r, bx, by, bw, bh);
    fx_vgrad(r, bx, by, bw, bh, (SDL_Color){64, 53, 40, 255}, 255,
             (SDL_Color){33, 28, 23, 255}, 255);
    color_rect(r, (SDL_Color){99, 84, 62, 255}, bx, by, bw, 1.0f);

    float px = bx + 10.0f;
    float py = by + 18.0f;
    float pw = bw - 20.0f;
    float ph = bh - 30.0f;
    color_rect(r, FX_INK, px + 2.0f, py + 3.0f, pw, ph);
    color_rect(r, paper, px, py, pw, ph);
    color_rect(r, fx_mix(paper, FX_CREAM, 0.55f), px, py, pw, 1.0f);
    color_rect(r, fx_mix(paper, FX_INK, 0.35f), px, py + ph - 1.0f, pw, 1.0f);

    /* The spring clip, biting the head of the sheet. */
    color_rect(r, FX_INK, cx - 41.0f, by + 2.0f, 82.0f, 24.0f);
    fx_vgrad(r, cx - 40.0f, by + 3.0f, 80.0f, 22.0f, FX_STEEL_LT, 255,
             FX_STEEL_DK, 255);
    color_rect(r, fx_mix(FX_STEEL_LT, FX_CREAM, 0.4f), cx - 40.0f, by + 3.0f,
               80.0f, 1.0f);
    color_rect(r, FX_INK, cx - 31.0f, by + 11.0f, 62.0f, 4.0f);

    draw_tracked(r, px + 9.0f, py + 30.0f, 1.0f, ink, "MERIDIAN FACILITY");
    draw_text(r, px + 9.0f, py + 42.0f, 1.0f, faint, "NIGHT ACCESS LOG");
    draw_text(r, px + 9.0f, py + 54.0f, 1.0f, faint, "14 MARCH  22:00-06:00");
    color_rect(r, fx_dim(FX_RUST, 0.9f), px + 9.0f, py + 66.0f, 76.0f, 2.0f);

    /* Twelve ruled lines, twelve names, twelve marks in the box. The
       signatures are hash-driven scribble rather than glyphs: a signature made
       of letters is a printed name, and a printed name is what the left
       column already is. */
    const float first = py + 76.0f;
    const float pitch = 13.0f;
    for (int i = 0; i < CREW_SIZE; ++i)
    {
        float ly = first + (float)i * pitch;
        dash_h(r, fx_mix(paper, ink, 0.30f), px + 9.0f, ly + 10.0f,
               pw - 18.0f, 3.0f, 2.0f);
        color_rect(r, fx_dim(FX_RUST, 0.85f), px + 9.0f, ly + 2.0f, 4.0f, 4.0f);
        draw_text(r, px + 18.0f, ly, 1.0f, ink, crew_callsign(i));

        for (int stroke = 0; stroke < 7; ++stroke)
        {
            unsigned h = fx_hash((unsigned)(i * 13 + stroke) * 2654435761u);
            float sx = px + 124.0f + (float)stroke * 8.0f;
            float sh = 3.0f + (float)(h % 5u);
            color_rect(r, fx_mix(paper, ink, 0.62f), sx,
                       ly + 7.0f - sh * 0.5f, 2.0f, sh);
        }
    }

    /* The stamp along the foot: the count nobody ever went back and checked
       against a case. */
    float foot = first + (float)CREW_SIZE * pitch + 5.0f;
    color_rect(r, fx_mix(paper, ink, 0.25f), px + 9.0f, foot, pw - 18.0f, 1.0f);
    draw_text(r, px + 9.0f, foot + 8.0f, 1.0f, ink, "12 SIGNED IN");
    color_rect(r, FX_INK, px + pw - 74.0f, foot + 4.0f, 64.0f, 17.0f);
    color_rect(r, fx_dim(FX_RUST, 0.95f), px + pw - 73.0f, foot + 5.0f, 62.0f,
               15.0f);
    draw_text(r, px + pw - 68.0f, foot + 9.0f, 1.0f, FX_INK, "A. VOSS");
}

/* Sector one to the roof, and where the climbs fall in it. */
static void illus_mission(SDL_Renderer *r, SDL_FRect p, float time,
                          const PadHints *pad,
                          const ManualRecords *records)
{
    (void)records;
    (void)pad;
    float street = p.y + p.h - 22.0f;
    float roof = p.y + 26.0f;
    float cx = p.x + p.w * 0.60f;
    float base_half = 50.0f;
    float top_half = 42.0f;

    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){8, 12, 22, 255}, 255,
             (SDL_Color){28, 36, 50, 255}, 255);
    for (unsigned i = 0; i < 34u; ++i)
    {
        unsigned h = fx_hash(i * 2654435761u + 17u);
        float sx = p.x + (float)fx_spread(h, p.w);
        float sy = p.y + (float)fx_spread(h >> 9, p.h * 0.6f);
        float twinkle = 0.5f + 0.5f * sinf(time * 1.6f + (float)(h % 61u));
        color_rect(r, fx_dim(FX_PALE, 0.30f + twinkle * 0.45f), sx, sy, 1.0f, 1.0f);
    }
    fx_glow(r, p.x + p.w * 0.20f, p.y + 34.0f, 30.0f,
            (SDL_Color){170, 196, 214, 255}, 26);
    color_rect(r, (SDL_Color){206, 222, 230, 255}, p.x + p.w * 0.20f - 5.0f,
               p.y + 30.0f, 10.0f, 9.0f);

    /* The tower, tapering because the shot looks up at it from the kerb. */
    for (float y = roof; y < street; y += 1.0f)
    {
        float t = (y - roof) / (street - roof);
        float half = top_half + (base_half - top_half) * t;
        color_rect(r, fx_mix((SDL_Color){20, 26, 38, 255},
                             (SDL_Color){34, 43, 58, 255}, t),
                   cx - half, y, half * 2.0f, 1.0f);
    }
    color_rect(r, (SDL_Color){52, 64, 80, 255}, cx - top_half - 3.0f, roof - 3.0f,
               top_half * 2.0f + 6.0f, 3.0f);

    /* The courses fill whatever height the plate has rather than a fixed
     * count, so the building never ends short of its own street. */
    int rows = (int)((street - roof - 24.0f) / 17.0f);
    for (int row = 0; row < rows; ++row)
    {
        float wy = roof + 10.0f + (float)row * 17.0f;
        float t = (wy - roof) / (street - roof);
        float half = top_half + (base_half - top_half) * t - 9.0f;
        float pane = (half * 2.0f - 3.0f * 4.0f) / 4.0f;
        for (int col = 0; col < 4; ++col)
        {
            float wx = cx - half + (pane + 4.0f) * (float)col;
            unsigned h = fx_hash((unsigned)(row * 41 + col * 7 + 3) * 2654435761u);
            bool lit = (h % 100u) < 38u &&
                       time > 0.25f + (float)(h % 12u) * 0.05f;
            SDL_Color glass = lit
                                  ? fx_dim((h & 8u) ? FX_WARM : COL_COLD,
                                           0.55f + (float)(h % 6u) * 0.06f)
                                  : (SDL_Color){13, 18, 27, 255};
            color_rect(r, glass, wx, wy, pane, 9.0f);
            color_rect(r, FX_NIGHT, wx, wy + 9.0f, pane, 1.0f);
        }
    }

    /* The window the whole picture points at: second course, third pane. */
    {
        float wy = roof + 10.0f + 17.0f;
        float t = (wy - roof) / (street - roof);
        float half = top_half + (base_half - top_half) * t - 9.0f;
        float pane = (half * 2.0f - 3.0f * 4.0f) / 4.0f;
        float wx = cx - half + (pane + 4.0f) * 2.0f;
        color_rect(r, fx_dim(FX_WARM, 0.85f), wx, wy, pane, 9.0f);
        color_rect(r, FX_INK, wx + pane * 0.5f - 2.0f, wy + 2.0f, 4.0f, 7.0f);
        float pulse = 0.55f + 0.45f * sinf(time * 3.1f);
        SDL_FRect mark = {wx - 4.0f, wy - 4.0f, pane + 8.0f, 17.0f};
        draw_brackets(r, fx_dim(FX_RUST, pulse), mark, 5.0f, 1.0f);
    }

    /*
     * The route: a tick a sector bottom to top, the climbs called out in amber.
     * It is the only place the campaign's shape is drawn rather than described.
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
    float route_x = cx - base_half - 14.0f;
    dash_v(r, fx_dim(FX_STEEL, 0.7f), route_x, roof + 4.0f, street - roof - 8.0f,
           3.0f, 4.0f);
    for (int sector = 1; sector <= CAMPAIGN_SECTORS; ++sector)
    {
        float t = (float)(sector - 1) / (float)(CAMPAIGN_SECTORS - 1);
        float y = street - 8.0f - t * (street - roof - 16.0f);
        bool climb = false;
        for (int i = 0; i < CAMPAIGN_CLIMB_SECTOR_COUNT; ++i)
            if (CAMPAIGN_CLIMB_SECTORS[i] == sector)
                climb = true;
        color_rect(r, climb ? FX_AMBER : FX_CYAN, route_x - 3.0f, y, 8.0f, 2.0f);
    }
    draw_text(r, route_x - 34.0f, roof - 2.0f, 1.0f, FX_LABEL, "ROOF");

    /* Blocks cropping both edges, a step darker than the tower. Without them
     * one slab stands in the middle of an empty plate; the point of the shot is
     * a building in a city. */
    color_rect(r, (SDL_Color){14, 18, 27, 255}, p.x, street - 62.0f, 40.0f, 62.0f);
    color_rect(r, (SDL_Color){24, 30, 42, 255}, p.x, street - 62.0f, 40.0f, 1.0f);
    color_rect(r, (SDL_Color){14, 18, 27, 255}, p.x + p.w - 52.0f, street - 88.0f,
               52.0f, 88.0f);
    color_rect(r, (SDL_Color){24, 30, 42, 255}, p.x + p.w - 52.0f, street - 88.0f,
               52.0f, 1.0f);
    for (unsigned i = 0; i < 9u; ++i)
    {
        unsigned h = fx_hash(i * 40503u + 7u);
        if ((h % 100u) >= 45u)
            continue;
        float lx = p.x + p.w - 46.0f + (float)(i % 3u) * 14.0f;
        float ly = street - 78.0f + (float)(i / 3u) * 22.0f;
        color_rect(r, fx_dim(FX_WARM, 0.42f), lx, ly, 6.0f, 5.0f);
    }

    /* Street, lamp, and the man looking up at it. */
    color_rect(r, (SDL_Color){18, 22, 30, 255}, p.x, street, p.w,
               p.y + p.h - street);
    color_rect(r, (SDL_Color){44, 52, 62, 255}, p.x, street, p.w, 1.0f);
    fx_glow(r, p.x + 30.0f, street - 6.0f, 40.0f, FX_LAMP, 40);
    color_rect(r, FX_STEEL_DK, p.x + 29.0f, street - 40.0f, 2.0f, 40.0f);
    color_rect(r, (SDL_Color){206, 226, 232, 255}, p.x + 26.0f, street - 42.0f,
               8.0f, 3.0f);
    fx_contact_shadow(r, p.x + 52.0f, street, 9.0f, 0.0f, 150);
    draw_figure(r, p.x + 46.0f, street, 1, POSE_LOOK_UP, look_chuck(), time);
}

/* Where a face button physically sits, from the position `pad_hint` filed its
 * letter under. It is the drawing's half of the module's rule: the sheet spells
 * no letter itself, and it draws none in a place the pad does not put it. */
static void face_offset(SDL_GamepadButton at, float *ox, float *oy)
{
    *ox = 0.0f;
    *oy = 0.0f;
    switch (at)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        *oy = 17.0f;
        break;
    case SDL_GAMEPAD_BUTTON_EAST:
        *ox = 17.0f;
        break;
    case SDL_GAMEPAD_BUTTON_WEST:
        *ox = -17.0f;
        break;
    case SDL_GAMEPAD_BUTTON_NORTH:
        *oy = -17.0f;
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
 * the tints stay with the *action*, so the sheet still looks exactly as it did
 * on the pad it was drawn for.
 */
static void illus_controls(SDL_Renderer *r, SDL_FRect p, float time,
                          const PadHints *pad,
                          const ManualRecords *records)
{
    (void)records;
    (void)time;
    fx_vgrad(r, p.x, p.y, p.w, p.h, FX_SHADOW, 255,
             (SDL_Color){11, 15, 24, 255}, 255);

    float cx = p.x + p.w * 0.5f;
    const float cap = 22.0f;
    const float pitch = 25.0f;

    draw_text(r, p.x + 10.0f, p.y + 8.0f, 1.0f, FX_LABEL, "KEYBOARD");

    /* The movement cluster, drawn as the cross it is under the hand. */
    float wy = p.y + 28.0f;
    draw_keycap(r, cx - cap * 0.5f, wy, cap, CHIP_H, "W", FX_CREAM);
    draw_keycap(r, cx - cap * 0.5f - pitch, wy + pitch, cap, CHIP_H, "A", FX_CREAM);
    draw_keycap(r, cx - cap * 0.5f, wy + pitch, cap, CHIP_H, "S", FX_CREAM);
    draw_keycap(r, cx - cap * 0.5f + pitch, wy + pitch, cap, CHIP_H, "D", FX_CREAM);
    draw_text(r, cx + 20.0f, wy + 5.0f, 1.0f, FX_AMBER, "JUMP");
    draw_text(r, cx - 116.0f, wy + pitch + 5.0f, 1.0f, FX_AMBER, "MOVE");
    dash_h(r, fx_dim(FX_STEEL, 0.8f), cx - 74.0f, wy + pitch + 9.0f, 24.0f,
           3.0f, 3.0f);
    /* Under S, not out past D. Set to the right of the row it read as the
     * label of the last key in it, which is the one key in the cluster that
     * does not crawl. */
    draw_text(r, cx - 20.0f, wy + pitch + 25.0f, 1.0f, FX_AMBER, "CRAWL");

    /* Everything the hands do that is not walking, on its own row so the
     * labels underneath never meet. */
    float row = wy + pitch * 2.0f + 14.0f;
    draw_keycap(r, cx - 96.0f, row, 62.0f, CHIP_H, "SPACE", FX_CREAM);
    draw_keycap(r, cx - 22.0f, row, 34.0f, CHIP_H, "TAB", FX_CREAM);
    draw_keycap(r, cx + 34.0f, row, cap, CHIP_H, "E", FX_CREAM);
    draw_text(r, cx - 96.0f, row + CHIP_H + 6.0f, 1.0f, FX_AMBER, "ATTACK");
    draw_text(r, cx - 30.0f, row + CHIP_H + 6.0f, 1.0f, FX_AMBER, "WEAPON");
    draw_text(r, cx + 36.0f, row + CHIP_H + 6.0f, 1.0f, FX_AMBER, "USE");

    /* The pad. Same actions, one thumb. */
    float gw = 176.0f;
    float gx = cx - gw * 0.5f;
    float gy = p.y + p.h - 108.0f;
    draw_text(r, p.x + 10.0f, gy - 30.0f, 1.0f, FX_LABEL, "GAMEPAD");

    const PadHints *hints = pad != NULL ? pad : &PAD_HINTS_XBOX;

    color_rect(r, FX_STEEL_DK, gx + 22.0f, gy - 6.0f, 30.0f, 6.0f);
    color_rect(r, FX_STEEL_DK, gx + gw - 52.0f, gy - 6.0f, 30.0f, 6.0f);
    /* A shoulder is hardware exactly as much as a face button is: L and R on a
     * Switch pad, L1 and R1 on a PlayStation. The control table beside this
     * drawing already spells them out of `$LB $RB`, so a drawing that says LB
     * whatever is plugged in contradicts its own page — the same bug the face
     * caps below were fixed for, left standing one row higher. Centred on the
     * bumper rather than set at a fixed inset, because the names are one, two
     * or three cells wide. */
    draw_text(r, gx + 37.0f - text_width(hints->shoulder_l) * 0.5f,
              gy - 18.0f, 1.0f, FX_LABEL, hints->shoulder_l);
    draw_text(r, gx + gw - 37.0f - text_width(hints->shoulder_r) * 0.5f,
              gy - 18.0f, 1.0f, FX_LABEL, hints->shoulder_r);
    fx_mass(r, FX_INK, gx - 1.0f, gy - 1.0f, gw + 2.0f, 72.0f, 7, 10);
    fx_mass(r, (SDL_Color){40, 50, 64, 255}, gx, gy, gw, 70.0f, 7, 10);
    fx_mass(r, (SDL_Color){62, 76, 94, 255}, gx, gy, gw, 3.0f, 7, 0);

    /* D-pad left, face buttons right, both far enough apart to be read. */
    float dx = gx + 40.0f;
    float dy = gy + 32.0f;
    color_rect(r, FX_STEEL_DK, dx - 5.0f, dy - 15.0f, 10.0f, 30.0f);
    color_rect(r, FX_STEEL_DK, dx - 15.0f, dy - 5.0f, 30.0f, 10.0f);
    color_rect(r, FX_STEEL_LT, dx - 5.0f, dy - 15.0f, 10.0f, 1.0f);
    color_rect(r, FX_STEEL_LT, dx - 15.0f, dy - 5.0f, 1.0f, 10.0f);

    float bx = gx + gw - 44.0f;
    float by = gy + 32.0f;
    /* Filed by what the button does, so the colours stay with the action while
     * the letter and the corner follow the hardware. */
    static const SDL_Color FACE_TINT[] = {
        {110, 214, 130, 255}, /* confirm */
        {228, 96, 86, 255},   /* cancel  */
        {104, 158, 226, 255}, /* attack  */
        {236, 200, 96, 255}}; /* door    */
    _Static_assert(SDL_arraysize(FACE_TINT) == (size_t)PAD_FACE_COUNT,
                   "every face needs a tint");
    for (int i = 0; i < PAD_FACE_COUNT; ++i)
    {
        float ox = 0.0f;
        float oy = 0.0f;
        face_offset(pad_hints_button(hints, (PadFace)i), &ox, &oy);
        float px = bx + ox - 7.0f;
        float py = by + oy - 7.0f;
        fx_mass(r, FX_INK, px - 1.0f, py - 1.0f, 16.0f, 16.0f, 4, 4);
        fx_mass(r, fx_dim(FACE_TINT[i], 0.50f), px, py, 14.0f, 14.0f, 4, 4);
        /* Centred rather than set at a fixed inset: a PlayStation pad spells
         * two of its faces with two characters, and a cap the label hangs out
         * of is worse than no cap at all. */
        const char *label = hints->face[i];
        float label_w = (float)SDL_strlen(label) * CH;
        draw_text(r, px + (14.0f - label_w) * 0.5f, py + 3.0f, 1.0f,
                  FX_CREAM, label);
    }

    /* B attacks beside X inside a sector, and the legend says so: a live
     * button left off the sheet is a button nobody finds. */
    char legend[24];
    float legend_y = gy + 80.0f;
    draw_text(r, p.x + 14.0f, legend_y, 1.0f, FX_AMBER,
              pad_hint(hints, legend, sizeof(legend), "$A JUMP", "$A JUMP"));
    draw_text(r, p.x + 104.0f, legend_y, 1.0f, FX_AMBER,
              pad_hint(hints, legend, sizeof(legend), "$X $B ATTACK",
                       "$X $B ATTACK"));
    draw_text(r, p.x + 232.0f, legend_y, 1.0f, FX_AMBER,
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
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){20, 26, 38, 255}, 255,
             (SDL_Color){12, 16, 25, 255}, 255);

    float floor_y = p.y + p.h - 34.0f;
    float upper_y = p.y + 96.0f;
    float gap_left = p.x + 150.0f;
    /* Two tiles at the vignette's own scale, which is about four fifths of the
     * world's — the crate beside it is drawn 22px against a `CRATE_W` of 28.
     * It was one tile wide, under a caption that said one tile was the jump,
     * and both were a tile short of what the body does: see
     * `test_a_jump_clears_a_wider_hole_than_the_model_will_route` for the
     * widths and `test_the_on_foot_sheet_spells_the_jump_it_draws` for the
     * caption. The caption is the claim and is held; this is the picture
     * agreeing with it, because a drawing that shows a notch under a line saying two tiles is
     * the reader's first reason to doubt the line. */
    float gap_right = gap_left + 50.0f;

    /* Two storeys and the hole between the upper slabs. */
    draw_slab(r, p.x, upper_y, gap_left - p.x, 14.0f);
    draw_slab(r, gap_right, upper_y, p.x + p.w - gap_right, 14.0f);
    draw_slab(r, p.x, floor_y, p.w, 20.0f);

    /* Ambient occlusion under the slab, the same pass the world gets: the air
     * beside a wall is lit too, and without it the slab floats. */
    fx_vgrad(r, p.x, upper_y + 14.0f, p.w, 16.0f, FX_INK, 130, FX_INK, 0);
    fx_vgrad(r, p.x, floor_y - 14.0f, p.w, 14.0f, FX_INK, 0, FX_INK, 90);

    /* The jump: one tile of hole, and a shadow left behind on the slab. */
    dash_arc(r, fx_dim(FX_CYAN, 0.8f), gap_left - 14.0f, upper_y - 6.0f,
             gap_right + 14.0f, upper_y - 6.0f, 24.0f, 11, 2.0f);
    fx_contact_shadow(r, gap_left + 13.0f, upper_y, 10.0f, 0.9f, 170);
    draw_figure(r, gap_left + 7.0f, upper_y - 30.0f, 1, POSE_JUMP,
                look_chuck(), time);

    /* The ladder between the storeys, with someone on the rungs. */
    float ladder_x = p.x + 46.0f;
    draw_ladder(r, ladder_x, upper_y + 14.0f, floor_y - upper_y - 14.0f);
    draw_figure(r, ladder_x - 3.0f, floor_y - 18.0f, 1, POSE_CLIMB,
                look_chuck(), time);

    /* The floor's furniture: something to shove, something not to touch. */
    draw_crate(r, p.x + p.w - 46.0f, floor_y - 22.0f, 22.0f);
    draw_chevron(r, fx_dim(FX_CYAN, 0.85f), p.x + p.w - 56.0f, floor_y - 12.0f,
                 -1, 0);
    draw_spikes(r, p.x + 116.0f, floor_y - 7.0f, 4);

    draw_text(r, p.x + 8.0f, floor_y + 24.0f, 1.0f, FX_LABEL, "LADDER");
    draw_text(r, p.x + 112.0f, floor_y + 24.0f, 1.0f, fx_dim(FX_RED, 0.9f),
              "SPIKES");
    draw_text(r, p.x + p.w - 50.0f, floor_y + 24.0f, 1.0f, FX_LABEL, "CRATE");
}

/* The two things worth knowing about a guard: where he is looking, and what
 * happens if you arrive from above. */
static void illus_combat(SDL_Renderer *r, SDL_FRect p, float time,
                         const PadHints *pad,
                         const ManualRecords *records)
{
    (void)records;
    (void)pad;
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){20, 26, 38, 255}, 255,
             (SDL_Color){12, 16, 25, 255}, 255);

    /* Upper vignette: the stomp. The two figures split the top of the plate,
     * with the arrow in the clear gap between the boots and the helmet — put
     * on the figure itself it disappears into him. */
    float ledge_y = p.y + p.h * 0.42f;
    draw_slab(r, p.x, ledge_y, p.w, 14.0f);
    fx_vgrad(r, p.x, ledge_y + 14.0f, p.w, 14.0f, FX_INK, 120, FX_INK, 0);
    draw_text(r, p.x + 8.0f, p.y + 8.0f, 1.0f, FX_LABEL, "FROM ABOVE");

    float stomp_x = p.x + p.w * 0.56f;
    draw_figure(r, stomp_x, ledge_y, -1, POSE_STAND, look_guard(), time);
    float bob = sinf(time * 2.4f) * 2.0f;
    draw_figure(r, stomp_x - 2.0f, ledge_y - 46.0f + bob, -1, POSE_JUMP,
                look_chuck(), time);
    draw_chevron(r, fx_dim(FX_AMBER, 0.9f), stomp_x + 5.0f,
                 ledge_y - 40.0f + bob, 0, 1);
    draw_text(r, p.x + 8.0f, ledge_y - 40.0f, 1.0f, FX_AMBER, "LAND ON");
    draw_text(r, p.x + 8.0f, ledge_y - 28.0f, 1.0f, FX_AMBER, "HIS HEAD");

    /* Lower vignette: the cone, and the only side of it worth being on. */
    float floor_y = p.y + p.h - 34.0f;
    draw_slab(r, p.x, floor_y, p.w, 20.0f);
    fx_vgrad(r, p.x, floor_y - 14.0f, p.w, 14.0f, FX_INK, 0, FX_INK, 90);
    /* The reach of the cone below, spelled in [manual_pages.h](manual_pages.h)
   * beside the sentence on this same sheet that states it, because it is a
   * number out of `game_config.h` rather than a caption. */
  draw_text(r, p.x + 8.0f, ledge_y + 34.0f, 1.0f, FX_LABEL,
            MANUAL_SIGHT_CONE_LABEL);

    float guard_x = p.x + p.w * 0.46f;
    float eye_y = floor_y - 22.0f;
    draw_sight_cone(r, guard_x + 1.0f, eye_y, -1, 118.0f, 34.0f, FX_RED, 52);
    dash_h(r, fx_dim(FX_RED, 0.7f), guard_x - 118.0f, eye_y - 34.0f, 6.0f, 3.0f, 3.0f);
    draw_figure(r, guard_x, floor_y, -1, POSE_STAND, look_guard(), time);

    /* Chuck behind the cone, answering it. */
    float chuck_x = p.x + p.w - 52.0f;
    fx_contact_shadow(r, chuck_x + 6.0f, floor_y, 9.0f, 0.0f, 150);
    draw_figure(r, chuck_x, floor_y, -1, POSE_AIM, look_chuck(), time);
    float flash = fmodf(time, 1.6f) < 0.12f ? 1.0f : 0.0f;
    if (flash > 0.0f)
    {
        fx_glow(r, chuck_x - 10.0f, floor_y - 15.0f, 22.0f, FX_WARM, 120);
        color_rect(r, FX_CREAM, chuck_x - 13.0f, floor_y - 16.0f, 5.0f, 2.0f);
    }
    dash_h(r, fx_dim(FX_AMBER, 0.9f), guard_x + 16.0f, floor_y - 15.0f,
           chuck_x - guard_x - 30.0f, 5.0f, 4.0f);
}

/*
 * A body on the floor, drawn the way the sector draws one: the same garment
 * mass laid on its side, no visor lit and no pips over it. Local to this file
 * because `draw_downed_enemy` takes an `Enemy` and a `Level`, neither of which
 * a sheet has, and because the plate's figures are twelve pixels across rather
 * than twenty-six.
 */
static void draw_downed_figure(SDL_Renderer *r, float x, float floor_y,
                               int dir, FigureLook look)
{
    FxRamp coat = fx_ramp(look.garment);
    float body_w = FIG_H * 0.72f;
    float body_h = 7.0f;
    float y = floor_y - body_h;
    ink_block(r, x, y, body_w, body_h);
    color_rect(r, coat.base, x, y, body_w, body_h);
    color_rect(r, coat.lit, x, y, body_w, 2.0f);

    /* The head at the trailing end, so the figure reads as having been dragged
       feet first — which is how it is actually being carried. */
    float head = dir > 0 ? x + body_w - 1.0f : x - 5.0f;
    ink_block(r, head, y - 3.0f, 6.0f, 6.0f);
    color_rect(r, fx_ramp(FX_SKIN).base, head, y - 3.0f, 6.0f, 6.0f);
    if (look.helmet)
        color_rect(r, fx_ramp(look.cap).base, head, y - 3.0f, 6.0f, 3.0f);
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
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){19, 25, 36, 255}, 255,
             (SDL_Color){11, 15, 24, 255}, 255);

    /* Upper: the bolt in the air, and the man turning to where it will land. */
    float upper_floor = p.y + p.h * 0.44f;
    draw_slab(r, p.x, upper_floor, p.w, 14.0f);
    fx_vgrad(r, p.x, upper_floor + 14.0f, p.w, 14.0f, FX_INK, 120, FX_INK, 0);
    draw_text(r, p.x + 8.0f, p.y + 8.0f, 1.0f, FX_LABEL, "THROW IT PAST HIM");

    float thrower_x = p.x + 26.0f;
    float landing_x = p.x + p.w - 30.0f;
    draw_figure(r, thrower_x, upper_floor, 1, POSE_AIM, look_chuck(), time);

    /* The arc, and the bolt riding it. `dash_arc` takes the height the curve
       actually reaches rather than the handle's, which is the revision the
       climb sheet already paid for once. */
    float flight = fmodf(time * 0.55f, 1.0f);
    dash_arc(r, fx_dim(FX_STEEL_LT, 0.75f), thrower_x + 10.0f,
             upper_floor - 20.0f, landing_x, upper_floor - 4.0f, 22.0f,
             5.0f, 4.0f);
    float bolt_x = thrower_x + 10.0f + (landing_x - thrower_x - 10.0f) * flight;
    float bolt_y = upper_floor - 20.0f - 4.0f * 22.0f * flight * (1.0f - flight) +
                   16.0f * flight;
    draw_decoy(r, bolt_x, bolt_y);

    /* The guard between them, looking the way the noise went rather than at the
       man who made it — which is the entire mechanic in one figure. */
    float guard_x = p.x + p.w * 0.56f;
    draw_figure(r, guard_x, upper_floor, 1, POSE_STAND, look_guard(), time);
    draw_sight_cone(r, guard_x + 11.0f, upper_floor - 22.0f, 1, 62.0f, 22.0f,
                    FX_AMBER, 46);
    float ring = 0.4f + 0.6f * fabsf(sinf(time * 3.0f));
    fx_glow(r, landing_x + 3.0f, upper_floor - 2.0f, 13.0f * ring, FX_AMBER,
            (Uint8)(90.0f * ring));

    /* Lower: the body going out of the room it fell in. */
    float floor_y = p.y + p.h - 30.0f;
    draw_slab(r, p.x, floor_y, p.w, 18.0f);
    fx_vgrad(r, p.x, floor_y - 14.0f, p.w, 14.0f, FX_INK, 0, FX_INK, 90);
    draw_text(r, p.x + 8.0f, upper_floor + 34.0f, 1.0f, FX_LABEL,
              "AND MOVE WHAT IS LEFT");

    float haul = sinf(time * 1.1f) * 5.0f;
    float chuck_x = p.x + 40.0f + haul;
    fx_contact_shadow(r, chuck_x + 6.0f, floor_y, 9.0f, 0.0f, 150);
    draw_figure(r, chuck_x, floor_y, -1, POSE_STAND, look_chuck(), time);
    draw_downed_figure(r, chuck_x + 16.0f, floor_y, 1, look_guard());

    /* The trail it left, from where it fell to where it is now. */
    dash_h(r, fx_dim(FX_CYAN, 0.55f), chuck_x + 38.0f, floor_y - 3.0f,
           p.w - (chuck_x - p.x) - 52.0f, 5.0f, 5.0f);
    draw_chevron(r, fx_dim(FX_CYAN, 0.85f), chuck_x + 2.0f, floor_y - 30.0f,
                 -1, 0);
}

/* The wall: no gravity, no ladders, and everything trying to take you off it. */
static void illus_climb(SDL_Renderer *r, SDL_FRect p, float time,
                        const PadHints *pad,
                        const ManualRecords *records)
{
    (void)records;
    (void)pad;
    draw_brickwork(r, p);

    float lower = p.y + p.h - 76.0f;
    float upper = p.y + 108.0f;
    draw_cornice(r, p.x, lower, p.w * 0.62f);
    draw_cornice(r, p.x + p.w * 0.30f, upper, p.w * 0.70f);
    fx_vgrad(r, p.x, lower + 13.0f, p.w * 0.62f, 12.0f, FX_INK, 120, FX_INK, 0);
    fx_vgrad(r, p.x + p.w * 0.30f, upper + 13.0f, p.w * 0.70f, 12.0f,
             FX_INK, 120, FX_INK, 0);

    /* The thrower: a shout and a lean, and only then the brick. */
    float win_x = p.x + 14.0f;
    float win_y = p.y + 30.0f;
    color_rect(r, FX_INK, win_x - 2.0f, win_y - 2.0f, 34.0f, 30.0f);
    color_rect(r, (SDL_Color){14, 16, 22, 255}, win_x, win_y, 30.0f, 26.0f);
    color_rect(r, fx_dim(FX_WARM, 0.35f), win_x + 2.0f, win_y + 2.0f, 26.0f, 8.0f);
    draw_figure(r, win_x + 9.0f, win_y + 26.0f, 1, POSE_STAND, look_guard(),
                time);
    float wind_up = 0.5f + 0.5f * sinf(time * 2.0f);
    /* Pale, not rust: a dark trajectory drawn on dark brick is a trajectory
     * nobody can follow. */
    dash_arc(r, fx_dim(fx_mix(FX_WARM, FX_CREAM, 0.5f), 0.6f + wind_up * 0.4f),
             win_x + 30.0f, win_y + 14.0f, p.x + p.w * 0.40f, lower - 8.0f,
             26.0f, 12, 3.0f);
    color_rect(r, FX_WOOD_DK, win_x + 32.0f, win_y + 12.0f, 6.0f, 4.0f);

    /* The climber in the lee of a stub of masonry, with the gust arriving on
     * the far side of it: the shelter has to be between him and the wind or
     * the picture says nothing the text does not have to say twice. */
    float climber_x = p.x + p.w * 0.34f;
    draw_figure(r, climber_x, lower, 1, POSE_CLING, look_chuck(), time);

    float shelter_x = climber_x + 24.0f;
    color_rect(r, FX_INK, shelter_x - 1.0f, lower - 43.0f, 20.0f, 30.0f);
    fx_vgrad(r, shelter_x, lower - 42.0f, 18.0f, 29.0f,
             (SDL_Color){112, 108, 98, 255}, 255, (SDL_Color){52, 50, 46, 255}, 255);
    color_rect(r, (SDL_Color){186, 180, 164, 255}, shelter_x, lower - 42.0f,
               18.0f, 1.0f);
    draw_text(r, climber_x - 10.0f, lower + 20.0f, 1.0f, FX_AMBER, "SHELTER");

    /* The gust: streaks that stop dead on the masonry. */
    float gust = 0.5f + 0.5f * sinf(time * 1.3f);
    float wind_from = p.x + p.w - 8.0f;
    float wind_to = shelter_x + 20.0f;
    for (int i = 0; i < 4; ++i)
    {
        float y = lower - 38.0f + (float)i * 8.0f;
        float len = (wind_from - wind_to) * (0.55f + gust * 0.45f);
        dash_h(r, fx_dim(COL_COLD, 0.35f + gust * 0.45f), wind_from - len, y,
               len, 7.0f, 4.0f);
    }
    draw_chevron(r, fx_dim(COL_COLD, 0.4f + gust * 0.6f), wind_to + 10.0f,
                 lower - 22.0f, -1, 0);
    draw_text(r, p.x + p.w - 44.0f, lower - 60.0f, 1.0f, COL_COLD, "WIND");

    /* A bird, and the height already banked. */
    float bird_x = p.x + p.w * 0.72f + sinf(time * 0.9f) * 18.0f;
    float bird_y = p.y + 62.0f;
    float wing = sinf(time * 9.0f) * 3.0f;
    SDL_Color feather = {162, 158, 150, 255};
    color_rect(r, feather, bird_x, bird_y, 3.0f, 2.0f);
    color_rect(r, feather, bird_x - 5.0f, bird_y - wing, 5.0f, 2.0f);
    color_rect(r, feather, bird_x + 3.0f, bird_y + wing, 5.0f, 2.0f);
    draw_text(r, bird_x - 14.0f, bird_y - 16.0f, 1.0f, fx_dim(feather, 0.8f),
              "BIRD");

    dash_h(r, fx_dim(FX_GREEN, 0.8f), p.x + 4.0f, lower - 2.0f, 34.0f, 4.0f, 3.0f);
    draw_text(r, p.x + 4.0f, lower - 16.0f, 1.0f, fx_dim(FX_GREEN, 0.9f), "BANK");
}

/* The strip along the top of the screen, and what is worth picking up. */
static void illus_console(SDL_Renderer *r, SDL_FRect p, float time,
                          const PadHints *pad,
                          const ManualRecords *records)
{
    (void)records;
    (void)pad;
    fx_vgrad(r, p.x, p.y, p.w, p.h, FX_SHADOW, 255,
             (SDL_Color){11, 15, 24, 255}, 255);

    /* A cutting of the real console, brushed and lit from above. */
    float strip_y = p.y + 10.0f;
    fx_vgrad(r, p.x + 6.0f, strip_y, p.w - 12.0f, 40.0f,
             (SDL_Color){30, 40, 56, 255}, 255, (SDL_Color){13, 19, 30, 255}, 255);
    color_rect(r, (SDL_Color){60, 76, 98, 255}, p.x + 6.0f, strip_y, p.w - 12.0f, 1.0f);
    color_rect(r, (SDL_Color){38, 112, 110, 255}, p.x + 6.0f, strip_y + 41.0f,
               p.w - 12.0f, 1.0f);
    color_rect(r, FX_RUST, p.x + 6.0f, strip_y, 3.0f, 40.0f);

    /*
     * A cutting of the strip has to be the strip. Three hearts, because three
     * is what a life is and what the sheet before this one says it is — the
     * five sockets this used to draw are the assist option, and a player
     * meeting the diagram before the game read them as two hearts already
     * spent. The lives counter beside them is here for the same reason: the
     * first bullet on the page points at it, so leaving it out made the
     * sentence describe a console the player would not find.
     */
    draw_text(r, p.x + 16.0f, strip_y + 6.0f, 1.0f, FX_LABEL, "VITAL");
    for (int i = 0; i < 3; ++i)
        draw_heart(r, p.x + 16.0f + (float)i * 12.0f, strip_y + 20.0f, i < 2);
    draw_text(r, p.x + 54.0f, strip_y + 21.0f, 1.0f,
              (SDL_Color){246, 110, 96, 255}, "x3");
    color_rect(r, (SDL_Color){34, 44, 58, 255}, p.x + 86.0f, strip_y + 6.0f,
               1.0f, 28.0f);
    /* The strip labels this field with the weapon's own name rather than with
     * a heading, so the cutting has to as well: a diagram captioned WPN is a
     * diagram of a console the player will never find. */
    draw_text(r, p.x + 96.0f, strip_y + 6.0f, 1.0f, FX_LABEL, "PISTOL");
    for (int i = 0; i < 6; ++i)
        draw_ammo_pip(r, p.x + 97.0f + (float)i * 7.0f, strip_y + 19.0f, i < 4);
    color_rect(r, (SDL_Color){34, 44, 58, 255}, p.x + 148.0f, strip_y + 6.0f,
               1.0f, 28.0f);
    draw_text(r, p.x + 158.0f, strip_y + 6.0f, 1.0f, FX_LABEL, "ACCESS");
    float blink = 0.45f + 0.55f * sinf(time * 4.0f);
    draw_access_chip(r, p.x + 158.0f, strip_y + 19.0f, 66.0f,
                     fx_dim((SDL_Color){246, 90, 70, 255}, blink),
                     (SDL_Color){54, 24, 24, 255}, (SDL_Color){124, 52, 46, 255},
                     FX_RED, "LOCKED");
    draw_text(r, p.x + 234.0f, strip_y + 6.0f, 1.0f, FX_LABEL, "SECTOR");
    /* An interior. Seven is a climb, and a climb draws a different strip
     * altogether — no ACCESS chip on it at all — so a cutting labelled 07 was a
     * console of a sector that never shows one, on the sheet that explains what
     * the chip means. */
    draw_text(r, p.x + 236.0f, strip_y + 19.0f, 2.0f, (SDL_Color){226, 232, 220, 255},
              "09");

    /* The other two things the chip can say. */
    float alt_y = strip_y + 54.0f;
    draw_access_chip(r, p.x + 10.0f, alt_y, 74.0f, FX_GREEN,
                     (SDL_Color){16, 52, 40, 255}, (SDL_Color){40, 132, 96, 255},
                     FX_GREEN, "GRANTED");
    draw_text(r, p.x + 92.0f, alt_y + 3.0f, 1.0f, COL_TEXT, "DOOR IS OPEN");
    draw_access_chip(r, p.x + 10.0f, alt_y + 20.0f, 74.0f,
                     (SDL_Color){166, 142, 91, 255}, (SDL_Color){36, 38, 42, 255},
                     (SDL_Color){96, 102, 108, 255},
                     (SDL_Color){190, 190, 184, 255}, "BLOCKED");
    draw_text(r, p.x + 92.0f, alt_y + 23.0f, 1.0f, COL_TEXT, "USE THE WINDOW");

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
        color_rect(r, (SDL_Color){15, 20, 30, 255}, p.x + 10.0f, y - 3.0f,
                   p.w - 20.0f, 18.0f);
        color_rect(r, (SDL_Color){26, 34, 48, 255}, p.x + 10.0f, y - 3.0f,
                   p.w - 20.0f, 1.0f);
        legend[i].draw(r, p.x + 18.0f, y);
        draw_text(r, p.x + 46.0f, y + 2.0f, 1.0f, COL_TEXT, legend[i].name);
    }

    /* And what the idle trail meter turns into once the building is looking
     * for you, which is the one readout worth recognising in a hurry. */
    float alert_y = list_y + (float)legend_rows * 22.0f + 12.0f;
    float pulse = 0.5f + 0.5f * sinf(time * 7.0f);
    SDL_Color alert = fx_dim((SDL_Color){255, 76, 54, 255}, 0.55f + pulse * 0.45f);
    draw_text(r, p.x + 10.0f, alert_y, 1.0f, alert, "SECURITY");
    color_rect(r, (SDL_Color){58, 16, 18, 255}, p.x + 10.0f, alert_y + 12.0f,
               124.0f, 16.0f);
    color_rect(r, alert, p.x + 10.0f, alert_y + 12.0f, 124.0f, 2.0f);
    color_rect(r, alert, p.x + 15.0f, alert_y + 17.0f, 5.0f, 5.0f);
    draw_text(r, p.x + 27.0f, alert_y + 16.0f, 1.0f, alert, "ALERT 07");
    draw_text(r, p.x + 142.0f, alert_y + 16.0f, 1.0f, COL_TEXT, "LOOKING FOR YOU");
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
    fx_vgrad(r, p.x, p.y, p.w, p.h, FX_SHADOW, 255,
             (SDL_Color){11, 15, 24, 255}, 255);

    /* The card itself, lit from the lamp above the desk like every other prop
     * on this sheet. */
    SDL_FRect card = {p.x + 10.0f, p.y + 12.0f, p.w - 20.0f, p.h - 24.0f};
    color_rect(r, COL_SHEET, card.x, card.y, card.w, card.h);
    color_rect(r, fx_dim(COL_SHEET_LIT, 0.8f), card.x, card.y, card.w, 1.0f);
    color_rect(r, FX_INK, card.x, card.y + card.h - 1.0f, card.w, 1.0f);

    draw_text(r, card.x + 12.0f, card.y + 10.0f, 1.0f, FX_CREAM,
              "SECTOR TIMES");
    dash_h(r, fx_dim(FX_RUST, 0.8f), card.x + 12.0f, card.y + 22.0f,
           card.w - 24.0f, 3.0f, 3.0f);

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
        /* A sector nobody has finished is set in the label grey rather than the
         * paper white the finished ones get: the card reads at a glance as how
         * much of the night has been measured. */
        bool have = seconds > PROGRESS_NO_TIME;
        draw_text(r, x, y, 1.0f, have ? COL_TEXT : fx_dim(FX_LABEL, 0.7f),
                  cell);
    }

    /* The two run figures, under a rule at the foot of the card. */
    float foot = card.y + card.h - 30.0f;
    dash_h(r, fx_dim(FX_STEEL_DK, 0.9f), card.x + 12.0f, foot - 8.0f,
           card.w - 24.0f, 3.0f, 3.0f);

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
        draw_text(r, card.x + 12.0f, foot, 1.0f, COL_COLD, line);
    if (run_tally_format_record_line(RUN_TALLY_RECORD_DOCKET, best_sheets, line,
                                     sizeof(line)) > 0)
        draw_text(r, card.x + 12.0f, foot + 13.0f, 1.0f,
                  fx_dim(FX_AMBER, 0.85f), line);

    /* The lamp's own flicker, so the card sits under the same light as the rest
     * of the desk rather than reading as a screen. */
    float flicker = 0.86f + 0.14f * sinf(time * 1.3f);
    fx_glow(r, card.x + card.w * 0.5f, card.y - 6.0f, 120.0f,
            fx_dim(FX_WARM, flicker), 18);
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

static void render_desk(SDL_Renderer *r, float w, float h)
{
    fx_vgrad(r, 0.0f, 0.0f, w, h, (SDL_Color){12, 16, 24, 255}, 255,
             (SDL_Color){6, 9, 15, 255}, 255);
    /* One lamp off to the left, so the sheet is lit by something rather than
     * being evenly bright. */
    fx_glow(r, w * 0.22f, -40.0f, 340.0f, (SDL_Color){120, 148, 176, 255}, 26);
}

static void render_sheet(SDL_Renderer *r, SDL_FRect sheet, float appear)
{
    fx_rect_a(r, FX_INK, (Uint8)(200.0f * appear), sheet.x + 3.0f,
              sheet.y + 4.0f, sheet.w, sheet.h);
    fx_rect_a(r, COL_SHEET, (Uint8)(248.0f * appear), sheet.x, sheet.y,
              sheet.w, sheet.h);
    fx_rect_a(r, COL_SHEET_LIT, (Uint8)(220.0f * appear), sheet.x, sheet.y,
              sheet.w, 1.0f);
    fx_rect_a(r, FX_INK, (Uint8)(210.0f * appear), sheet.x,
              sheet.y + sheet.h - 1.0f, sheet.w, 1.0f);
    /* A steel clip across the head of the sheet. */
    fx_rect_a(r, FX_STEEL_DK, (Uint8)(235.0f * appear),
              sheet.x + sheet.w * 0.5f - 46.0f, sheet.y - 4.0f, 92.0f, 8.0f);
    fx_rect_a(r, FX_STEEL_LT, (Uint8)(235.0f * appear),
              sheet.x + sheet.w * 0.5f - 46.0f, sheet.y - 4.0f, 92.0f, 1.0f);
    draw_brackets(r, fx_dim(FX_RUST, 0.85f * appear), sheet, 12.0f, 2.0f);
}

static void render_header(SDL_Renderer *r, const ManualPageText *page,
                          float w, float appear)
{
    draw_tracked(r, TEXT_X, 32.0f, 3.0f, fx_dim(FX_LABEL, appear),
                 "FIELD MANUAL");
    draw_text(r, TEXT_X, 46.0f, 2.0f, fx_dim(FX_CREAM, appear), page->title);
    draw_text(r, TEXT_X, 74.0f, 1.0f, fx_dim(FX_LABEL, appear), page->strap);

    float rule_y = 88.0f;
    color_rect(r, fx_dim(FX_RUST, 0.9f * appear), TEXT_X, rule_y, 64.0f, 2.0f);
    color_rect(r, fx_dim(FX_STEEL_DK, appear), TEXT_X + 70.0f, rule_y,
               w - TEXT_X * 2.0f - 70.0f, 1.0f);
}

static void render_text_column(SDL_Renderer *r, const ManualPageText *page,
                               const PadHints *hints, float slide, float appear)
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
            draw_tracked(r, x, y, 1.0f, fx_dim(FX_AMBER, appear), line->text);
            color_rect(r, fx_dim(FX_STEEL_DK, appear), x, y + HEAD_RULE_Y,
                       TEXT_RIGHT - TEXT_X, 1.0f);
            y += HEAD_PITCH;
            break;
        case LINE_BULLET:
            color_rect(r, fx_dim(FX_CYAN, appear), x + 1.0f, y + 3.0f, 3.0f, 3.0f);
            draw_text(r, x + BULLET_INDENT, y, 1.0f, fx_dim(COL_TEXT, appear),
                      body);
            y += LINE_PITCH;
            break;
        case LINE_BODY:
            draw_text(r, x + BULLET_INDENT, y, 1.0f, fx_dim(COL_TEXT, appear),
                      body);
            y += LINE_PITCH;
            break;
        case LINE_KEY:
            key_field(line->text, 0, key, sizeof(key));
            key_field(line->text, 1, pad, sizeof(pad));
            key_field(line->text, 2, action, sizeof(action));
            draw_keycap(r, x, y - KEY_CHIP_RISE, key_w, CHIP_H, key,
                        fx_dim(COL_KEYCAP, appear));
            draw_keycap(r, x + key_w + 8.0f, y - KEY_CHIP_RISE, pad_w, CHIP_H,
                        pad_column(hints, pad, spelled, sizeof(spelled)),
                        fx_dim(FX_LABEL, appear));
            draw_text(r, x + key_w + pad_w + 20.0f, y + 2.0f, 1.0f,
                      fx_dim(COL_TEXT, appear), action);
            y += KEY_PITCH;
            break;
        case LINE_GAP:
            y += GAP_PITCH;
            break;
        }
    }
}

static void render_panel(SDL_Renderer *r, const ManualPageText *page,
                         int index, float time, float slide, float appear,
                         const PadHints *pad, const ManualRecords *records)
{
    SDL_FRect frame = {PANEL_X + slide, PANEL_Y, PANEL_W, PANEL_H};
    SDL_FRect inner = {frame.x + PANEL_FRAME, frame.y + PANEL_FRAME,
                       frame.w - PANEL_FRAME * 2.0f,
                       frame.h - PANEL_FRAME * 2.0f};

    color_rect(r, fx_dim((SDL_Color){30, 39, 54, 255}, appear), frame.x, frame.y,
               frame.w, frame.h);
    color_rect(r, fx_dim(FX_INK, appear), inner.x - 1.0f, inner.y - 1.0f,
               inner.w + 2.0f, inner.h + 2.0f);

    PAGE_ILLUSTRATIONS[index](r, inner, time, pad, records);

    /* The wash a turning sheet passes under. The illustrations do not know
     * about the page turn, so the veil is what carries them through it. */
    fx_rect_a(r, FX_INK, (Uint8)(215.0f * (1.0f - appear)), inner.x, inner.y,
              inner.w, inner.h);
    draw_brackets(r, fx_dim(FX_RUST, 0.7f * appear), frame, 10.0f, 2.0f);

    /* Clipped to the plate's own width rather than trusted to be short: a
     * caption that outgrows the panel runs off the sheet entirely. */
    char caption[CAPTION_MAX + 1];
    SDL_strlcpy(caption, page->caption, sizeof(caption));
    draw_text(r, frame.x, frame.y + frame.h + 8.0f, 1.0f,
              fx_dim(FX_LABEL, appear), caption);
}

static void render_chip(SDL_Renderer *r, SDL_FRect box, const char *label,
                        bool hot, bool live, float appear)
{
    float level = (live ? 1.0f : 0.45f) * appear;
    if (hot && live)
        fx_glow(r, box.x + box.w * 0.5f, box.y + box.h * 0.5f, 60.0f,
                (SDL_Color){226, 104, 78, 255}, 30);
    fx_rect_a(r, FX_INK, (Uint8)(190.0f * level), box.x, box.y, box.w, box.h);
    fx_rect_a(r, (SDL_Color){32, 42, 56, 255}, (Uint8)(215.0f * level),
              box.x + 1.0f, box.y + 1.0f, box.w - 2.0f, box.h - 2.0f);
    fx_rect_a(r, hot && live ? FX_RUST : (SDL_Color){68, 84, 102, 255},
              (Uint8)(215.0f * level), box.x + 1.0f, box.y + 1.0f,
              box.w - 2.0f, 1.0f);
    draw_text(r, box.x + 8.0f, box.y + 5.0f, 1.0f,
              fx_dim(hot && live ? FX_CREAM : (SDL_Color){186, 196, 190, 255},
                     level),
              label);
}

static void render_footer(SDL_Renderer *r, const Manual *manual, float appear)
{
    render_chip(r, manual->prev_chip, "< PREV",
                manual->hovered == MANUAL_HOT_PREV, manual->page > 0, appear);
    render_chip(r, manual->next_chip, "NEXT >",
                manual->hovered == MANUAL_HOT_NEXT,
                manual->page < MANUAL_PAGE_COUNT - 1, appear);
    render_chip(r, manual->back_chip, manual->back_label,
                manual->hovered == MANUAL_HOT_BACK, true, appear);

    /* Where in the sheaf this sheet is. */
    float x = manual->prev_chip.x + manual->prev_chip.w + 18.0f;
    float y = manual->prev_chip.y + CHIP_H * 0.5f - 2.0f;
    for (int i = 0; i < MANUAL_PAGE_COUNT; ++i)
    {
        bool live = i == manual->page;
        color_rect(r, fx_dim(live ? FX_AMBER : FX_STEEL_DK, appear),
                   x + (float)i * 10.0f, live ? y - 1.0f : y, 6.0f,
                   live ? 4.0f : 3.0f);
    }

    /* A divider before the way out, so BACK does not read as a third page
     * button. */
    color_rect(r, fx_dim(FX_STEEL_DK, appear),
               manual->back_chip.x - 14.0f, manual->prev_chip.y + 2.0f, 1.0f,
               CHIP_H - 4.0f);
}

void manual_render(SDL_Renderer *r, const Manual *manual, int win_w, int win_h,
                   const PadHints *pad, const ManualRecords *records)
{
    float w = win_w > 0 ? (float)win_w : 800.0f;
    float h = win_h > 0 ? (float)win_h : 552.0f;
    const ManualPageText *page = &MANUAL_PAGES[manual->page];

    float appear = smoothstep01(manual->time / 0.30f);
    /* A turned sheet arrives from the side it came from and settles. */
    float settle = smoothstep01(manual->settle);
    float slide = (float)manual->settle_dir * settle * 16.0f;
    float content = appear * (1.0f - settle * 0.85f);

    render_desk(r, w, h);
    render_sheet(r, sheet_rect(w, h), appear);
    render_header(r, page, w, content);
    render_text_column(r, page, pad, slide, content);
    render_panel(r, page, manual->page, manual->time, slide, content, pad,
                 records);
    render_footer(r, manual, appear);

    /* The frame is finished (vignette, scanlines) by game_render's one
       shared pass, so the sheet cannot drift from the film. */
}
