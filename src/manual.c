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

#include "fx.h"

/* ---- Palette ---------------------------------------------------------- */

static const SDL_Color COL_SHEET = {22, 29, 42, 255};
static const SDL_Color COL_SHEET_LIT = {56, 70, 90, 255};
static const SDL_Color COL_RUST = {198, 62, 50, 255};
static const SDL_Color COL_HEAD = {248, 188, 74, 255};  /* section heads   */
static const SDL_Color COL_TEXT = {176, 190, 200, 255}; /* body            */
static const SDL_Color COL_LABEL = {108, 128, 148, 255};
static const SDL_Color COL_WARM = {240, 190, 112, 255}; /* interior light  */
static const SDL_Color COL_COLD = {104, 188, 196, 255}; /* strip lighting  */

/* ---- Layout ----------------------------------------------------------- */

#define SHEET_INSET 20.0f
#define SHEET_TOP 18.0f
#define SHEET_BOTTOM 18.0f

#define TEXT_X 42.0f
#define TEXT_RIGHT 424.0f
#define BODY_Y 100.0f
#define BODY_BOTTOM 496.0f
#define BULLET_INDENT 12.0f
#define LINE_PITCH 13.0f
#define KEY_PITCH 21.0f
#define HEAD_PITCH 20.0f
#define HEAD_LEAD 9.0f
#define GAP_PITCH 7.0f

#define PANEL_X 444.0f
#define PANEL_Y 100.0f
#define PANEL_W 314.0f
#define PANEL_H 330.0f
#define PANEL_FRAME 6.0f
/* A caption is set from the panel's left edge, so it has the panel's width and
 * no more: a line that runs past the plate runs off the sheet. */
#define CAPTION_MAX 38

#define CH ((float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)
#define CHIP_H 18.0f

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
    color_rect(r, (SDL_Color){10, 14, 22, 255}, x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f);
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
    return (FigureLook){FX_HERO, {26, 32, 46, 255}, COL_RUST, false};
}

static FigureLook look_guard(void)
{
    return (FigureLook){FX_GUARD, {30, 34, 30, 255}, FX_GUARD_DK, true};
}

static void ink_block(SDL_Renderer *r, float x, float y, float w, float h)
{
    color_rect(r, FX_INK, x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f);
}

static void draw_figure(SDL_Renderer *r, float x, float feet_y, int dir,
                        FigurePose pose, FigureLook look)
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

    ink_block(r, x, torso_y, FIG_W, 10.0f);
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
    ink_block(r, x + 2.0f, head_y, 8.0f, 7.0f);
    fx_form_mass(r, x + 2.0f, head_y, 8.0f, 7.0f, skin, dir, 2, 2);

    if (!rear)
    {
        float eye_x = dir > 0 ? x + 6.0f : x + 4.0f;
        float eye_y = head_y + (pose == POSE_LOOK_UP ? 2.0f : 3.0f);
        color_rect(r, FX_CREAM, eye_x, eye_y, 2.0f, 1.0f);
        color_rect(r, FX_INK, dir > 0 ? eye_x + 1.0f : eye_x, eye_y, 1.0f, 1.0f);
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
    fx_vgrad(r, x, y, w, h, (SDL_Color){50, 60, 74, 255}, 255,
             (SDL_Color){24, 31, 43, 255}, 255);
    color_rect(r, (SDL_Color){126, 142, 156, 255}, x, y, w, 1.0f);
    color_rect(r, (SDL_Color){14, 19, 28, 255}, x, y + h - 1.0f, w, 1.0f);
    for (float rivet = x + 8.0f; rivet < x + w - 4.0f; rivet += 22.0f)
        color_rect(r, (SDL_Color){70, 84, 99, 255}, rivet, y + 5.0f, 2.0f, 2.0f);
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
                unsigned h = fx_hash((unsigned)(bx * 3.0f + y * 71.0f));
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
        color_rect(r, (SDL_Color){255, 236, 170, 255}, bx, y + 3.0f, 3.0f, 2.0f);
        color_rect(r, (SDL_Color){222, 172, 84, 255}, bx, y + 5.0f, 3.0f, 4.0f);
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

static void draw_heart(SDL_Renderer *r, float x, float y, bool full)
{
    SDL_Color c = full ? (SDL_Color){236, 84, 78, 255} : (SDL_Color){42, 50, 60, 255};
    SDL_Color lit = full ? (SDL_Color){255, 152, 132, 255} : c;
    color_rect(r, c, x + 1.0f, y, 2.0f, 2.0f);
    color_rect(r, c, x + 5.0f, y, 2.0f, 2.0f);
    color_rect(r, c, x, y + 2.0f, 8.0f, 3.0f);
    color_rect(r, c, x + 1.0f, y + 5.0f, 6.0f, 1.0f);
    color_rect(r, c, x + 2.0f, y + 6.0f, 4.0f, 1.0f);
    color_rect(r, c, x + 3.0f, y + 7.0f, 2.0f, 1.0f);
    color_rect(r, lit, x + 1.0f, y + 2.0f, 2.0f, 1.0f);
}

static void draw_ammo_pip(SDL_Renderer *r, float x, float y, bool full)
{
    if (full)
    {
        color_rect(r, (SDL_Color){255, 236, 170, 255}, x, y, 3.0f, 3.0f);
        color_rect(r, (SDL_Color){222, 172, 84, 255}, x, y + 3.0f, 3.0f, 8.0f);
        color_rect(r, (SDL_Color){164, 118, 52, 255}, x + 2.0f, y + 3.0f, 1.0f, 8.0f);
    }
    else
        color_rect(r, (SDL_Color){40, 48, 58, 255}, x, y, 3.0f, 11.0f);
    color_rect(r, FX_STEEL, x - 1.0f, y + 11.0f, 5.0f, 2.0f);
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

/* Sector one to the roof, and where the four climbs fall in it. */
static void illus_mission(SDL_Renderer *r, SDL_FRect p, float time)
{
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
        float sx = p.x + (float)(h % (unsigned)p.w);
        float sy = p.y + (float)((h >> 9) % (unsigned)(p.h * 0.6f));
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
                                  ? fx_dim((h & 8u) ? COL_WARM : COL_COLD,
                                           0.55f + (float)(h % 6u) * 0.06f)
                                  : (SDL_Color){13, 18, 27, 255};
            color_rect(r, glass, wx, wy, pane, 9.0f);
            color_rect(r, (SDL_Color){10, 14, 21, 255}, wx, wy + 9.0f, pane, 1.0f);
        }
    }

    /* The window the whole picture points at: second course, third pane. */
    {
        float wy = roof + 10.0f + 17.0f;
        float t = (wy - roof) / (street - roof);
        float half = top_half + (base_half - top_half) * t - 9.0f;
        float pane = (half * 2.0f - 3.0f * 4.0f) / 4.0f;
        float wx = cx - half + (pane + 4.0f) * 2.0f;
        color_rect(r, fx_dim(COL_WARM, 0.85f), wx, wy, pane, 9.0f);
        color_rect(r, FX_INK, wx + pane * 0.5f - 2.0f, wy + 2.0f, 4.0f, 7.0f);
        float pulse = 0.55f + 0.45f * sinf(time * 3.1f);
        SDL_FRect mark = {wx - 4.0f, wy - 4.0f, pane + 8.0f, 17.0f};
        draw_brackets(r, fx_dim(COL_RUST, pulse), mark, 5.0f, 1.0f);
    }

    /* The route: fifteen sectors bottom to top, the four climbs called out in
     * amber. It is the only place the campaign's shape is drawn rather than
     * described. */
    float route_x = cx - base_half - 14.0f;
    dash_v(r, fx_dim(FX_STEEL, 0.7f), route_x, roof + 4.0f, street - roof - 8.0f,
           3.0f, 4.0f);
    for (int sector = 1; sector <= 15; ++sector)
    {
        float t = (float)(sector - 1) / 14.0f;
        float y = street - 8.0f - t * (street - roof - 16.0f);
        bool climb = sector == 3 || sector == 7 || sector == 11 || sector == 13;
        color_rect(r, climb ? COL_HEAD : FX_CYAN, route_x - 3.0f, y, 8.0f, 2.0f);
    }
    draw_text(r, route_x - 34.0f, roof - 2.0f, 1.0f, COL_LABEL, "ROOF");

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
        color_rect(r, fx_dim(COL_WARM, 0.42f), lx, ly, 6.0f, 5.0f);
    }

    /* Street, lamp, and the man looking up at it. */
    color_rect(r, (SDL_Color){18, 22, 30, 255}, p.x, street, p.w,
               p.y + p.h - street);
    color_rect(r, (SDL_Color){44, 52, 62, 255}, p.x, street, p.w, 1.0f);
    fx_glow(r, p.x + 30.0f, street - 6.0f, 40.0f, (SDL_Color){150, 206, 214, 255}, 40);
    color_rect(r, FX_STEEL_DK, p.x + 29.0f, street - 40.0f, 2.0f, 40.0f);
    color_rect(r, (SDL_Color){206, 226, 232, 255}, p.x + 26.0f, street - 42.0f,
               8.0f, 3.0f);
    fx_contact_shadow(r, p.x + 52.0f, street, 9.0f, 0.0f, 150);
    draw_figure(r, p.x + 46.0f, street, 1, POSE_LOOK_UP, look_chuck());
}

/*
 * Every control, on the two things you might be holding.
 *
 * Both halves are laid out from one centre line with the labels given their
 * own rows, because a label tucked beside a keycap is a label that collides
 * with the next one the moment either string changes length.
 */
static void illus_controls(SDL_Renderer *r, SDL_FRect p, float time)
{
    (void)time;
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){17, 23, 34, 255}, 255,
             (SDL_Color){11, 15, 24, 255}, 255);

    float cx = p.x + p.w * 0.5f;
    const float cap = 22.0f;
    const float pitch = 25.0f;

    draw_text(r, p.x + 10.0f, p.y + 8.0f, 1.0f, COL_LABEL, "KEYBOARD");

    /* The movement cluster, drawn as the cross it is under the hand. */
    float wy = p.y + 28.0f;
    draw_keycap(r, cx - cap * 0.5f, wy, cap, CHIP_H, "W", FX_CREAM);
    draw_keycap(r, cx - cap * 0.5f - pitch, wy + pitch, cap, CHIP_H, "A", FX_CREAM);
    draw_keycap(r, cx - cap * 0.5f, wy + pitch, cap, CHIP_H, "S", FX_CREAM);
    draw_keycap(r, cx - cap * 0.5f + pitch, wy + pitch, cap, CHIP_H, "D", FX_CREAM);
    draw_text(r, cx + 20.0f, wy + 5.0f, 1.0f, COL_HEAD, "JUMP");
    draw_text(r, cx + 44.0f, wy + pitch + 5.0f, 1.0f, COL_HEAD, "CRAWL");
    draw_text(r, cx - 116.0f, wy + pitch + 5.0f, 1.0f, COL_HEAD, "MOVE");
    dash_h(r, fx_dim(FX_STEEL, 0.8f), cx - 74.0f, wy + pitch + 9.0f, 24.0f,
           3.0f, 3.0f);

    /* Everything the hands do that is not walking, on its own row so the
     * labels underneath never meet. */
    float row = wy + pitch * 2.0f + 8.0f;
    draw_keycap(r, cx - 96.0f, row, 62.0f, CHIP_H, "SPACE", FX_CREAM);
    draw_keycap(r, cx - 22.0f, row, 34.0f, CHIP_H, "TAB", FX_CREAM);
    draw_keycap(r, cx + 34.0f, row, cap, CHIP_H, "E", FX_CREAM);
    draw_text(r, cx - 96.0f, row + CHIP_H + 6.0f, 1.0f, COL_HEAD, "ATTACK");
    draw_text(r, cx - 30.0f, row + CHIP_H + 6.0f, 1.0f, COL_HEAD, "WEAPON");
    draw_text(r, cx + 36.0f, row + CHIP_H + 6.0f, 1.0f, COL_HEAD, "USE");

    /* The pad. Same actions, one thumb. */
    float gw = 176.0f;
    float gx = cx - gw * 0.5f;
    float gy = p.y + p.h - 108.0f;
    draw_text(r, p.x + 10.0f, gy - 30.0f, 1.0f, COL_LABEL, "GAMEPAD");

    color_rect(r, FX_STEEL_DK, gx + 22.0f, gy - 6.0f, 30.0f, 6.0f);
    color_rect(r, FX_STEEL_DK, gx + gw - 52.0f, gy - 6.0f, 30.0f, 6.0f);
    draw_text(r, gx + 30.0f, gy - 18.0f, 1.0f, COL_LABEL, "LB");
    draw_text(r, gx + gw - 44.0f, gy - 18.0f, 1.0f, COL_LABEL, "RB");
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
    struct
    {
        float ox, oy;
        const char *label;
        SDL_Color tint;
    } face[4] = {
        {0.0f, 17.0f, "A", {110, 214, 130, 255}},
        {17.0f, 0.0f, "B", {228, 96, 86, 255}},
        {-17.0f, 0.0f, "X", {104, 158, 226, 255}},
        {0.0f, -17.0f, "Y", {236, 200, 96, 255}}};
    for (int i = 0; i < 4; ++i)
    {
        float px = bx + face[i].ox - 7.0f;
        float py = by + face[i].oy - 7.0f;
        fx_mass(r, FX_INK, px - 1.0f, py - 1.0f, 16.0f, 16.0f, 4, 4);
        fx_mass(r, fx_dim(face[i].tint, 0.50f), px, py, 14.0f, 14.0f, 4, 4);
        draw_text(r, px + 3.0f, py + 3.0f, 1.0f, FX_CREAM, face[i].label);
    }

    float legend_y = gy + 80.0f;
    draw_text(r, p.x + 14.0f, legend_y, 1.0f, COL_HEAD, "A JUMP");
    draw_text(r, p.x + 110.0f, legend_y, 1.0f, COL_HEAD, "X ATTACK");
    draw_text(r, p.x + 216.0f, legend_y, 1.0f, COL_HEAD, "Y USE");
}

/* What the floor plan will and will not let a pair of boots do. */
static void illus_movement(SDL_Renderer *r, SDL_FRect p, float time)
{
    (void)time;
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){20, 26, 38, 255}, 255,
             (SDL_Color){12, 16, 25, 255}, 255);

    float floor_y = p.y + p.h - 34.0f;
    float upper_y = p.y + 96.0f;
    float gap_left = p.x + 150.0f;
    float gap_right = gap_left + 26.0f;

    /* Two storeys and the one-tile hole between the upper slabs. */
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
    draw_figure(r, gap_left + 7.0f, upper_y - 30.0f, 1, POSE_JUMP, look_chuck());

    /* The ladder between the storeys, with someone on the rungs. */
    float ladder_x = p.x + 46.0f;
    draw_ladder(r, ladder_x, upper_y + 14.0f, floor_y - upper_y - 14.0f);
    draw_figure(r, ladder_x - 3.0f, floor_y - 18.0f, 1, POSE_CLIMB, look_chuck());

    /* The floor's furniture: something to shove, something not to touch. */
    draw_crate(r, p.x + p.w - 46.0f, floor_y - 22.0f, 22.0f);
    draw_chevron(r, fx_dim(FX_CYAN, 0.85f), p.x + p.w - 56.0f, floor_y - 12.0f,
                 -1, 0);
    draw_spikes(r, p.x + 116.0f, floor_y - 7.0f, 4);

    draw_text(r, p.x + 8.0f, floor_y + 24.0f, 1.0f, COL_LABEL, "LADDER");
    draw_text(r, p.x + 112.0f, floor_y + 24.0f, 1.0f, fx_dim(FX_RED, 0.9f),
              "SPIKES");
    draw_text(r, p.x + p.w - 50.0f, floor_y + 24.0f, 1.0f, COL_LABEL, "CRATE");
}

/* The two things worth knowing about a guard: where he is looking, and what
 * happens if you arrive from above. */
static void illus_combat(SDL_Renderer *r, SDL_FRect p, float time)
{
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){20, 26, 38, 255}, 255,
             (SDL_Color){12, 16, 25, 255}, 255);

    /* Upper vignette: the stomp. The two figures split the top of the plate,
     * with the arrow in the clear gap between the boots and the helmet — put
     * on the figure itself it disappears into him. */
    float ledge_y = p.y + p.h * 0.42f;
    draw_slab(r, p.x, ledge_y, p.w, 14.0f);
    fx_vgrad(r, p.x, ledge_y + 14.0f, p.w, 14.0f, FX_INK, 120, FX_INK, 0);
    draw_text(r, p.x + 8.0f, p.y + 8.0f, 1.0f, COL_LABEL, "FROM ABOVE");

    float stomp_x = p.x + p.w * 0.56f;
    draw_figure(r, stomp_x, ledge_y, -1, POSE_STAND, look_guard());
    float bob = sinf(time * 2.4f) * 2.0f;
    draw_figure(r, stomp_x - 2.0f, ledge_y - 46.0f + bob, -1, POSE_JUMP,
                look_chuck());
    draw_chevron(r, fx_dim(COL_HEAD, 0.9f), stomp_x + 5.0f,
                 ledge_y - 40.0f + bob, 0, 1);
    draw_text(r, p.x + 8.0f, ledge_y - 40.0f, 1.0f, COL_HEAD, "LAND ON");
    draw_text(r, p.x + 8.0f, ledge_y - 28.0f, 1.0f, COL_HEAD, "HIS HEAD");

    /* Lower vignette: the cone, and the only side of it worth being on. */
    float floor_y = p.y + p.h - 34.0f;
    draw_slab(r, p.x, floor_y, p.w, 20.0f);
    fx_vgrad(r, p.x, floor_y - 14.0f, p.w, 14.0f, FX_INK, 0, FX_INK, 90);
    draw_text(r, p.x + 8.0f, ledge_y + 34.0f, 1.0f, COL_LABEL, "SEVEN TILES");

    float guard_x = p.x + p.w * 0.46f;
    float eye_y = floor_y - 22.0f;
    draw_sight_cone(r, guard_x + 1.0f, eye_y, -1, 118.0f, 34.0f, FX_RED, 52);
    dash_h(r, fx_dim(FX_RED, 0.7f), guard_x - 118.0f, eye_y - 34.0f, 6.0f, 3.0f, 3.0f);
    draw_figure(r, guard_x, floor_y, -1, POSE_STAND, look_guard());

    /* Chuck behind the cone, answering it. */
    float chuck_x = p.x + p.w - 52.0f;
    fx_contact_shadow(r, chuck_x + 6.0f, floor_y, 9.0f, 0.0f, 150);
    draw_figure(r, chuck_x, floor_y, -1, POSE_AIM, look_chuck());
    float flash = fmodf(time, 1.6f) < 0.12f ? 1.0f : 0.0f;
    if (flash > 0.0f)
    {
        fx_glow(r, chuck_x - 10.0f, floor_y - 15.0f, 22.0f, COL_WARM, 120);
        color_rect(r, FX_CREAM, chuck_x - 13.0f, floor_y - 16.0f, 5.0f, 2.0f);
    }
    dash_h(r, fx_dim(COL_HEAD, 0.9f), guard_x + 16.0f, floor_y - 15.0f,
           chuck_x - guard_x - 30.0f, 5.0f, 4.0f);
}

/* The wall: no gravity, no ladders, and everything trying to take you off it. */
static void illus_climb(SDL_Renderer *r, SDL_FRect p, float time)
{
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
    color_rect(r, fx_dim(COL_WARM, 0.35f), win_x + 2.0f, win_y + 2.0f, 26.0f, 8.0f);
    draw_figure(r, win_x + 9.0f, win_y + 26.0f, 1, POSE_STAND, look_guard());
    float wind_up = 0.5f + 0.5f * sinf(time * 2.0f);
    /* Pale, not rust: a dark trajectory drawn on dark brick is a trajectory
     * nobody can follow. */
    dash_arc(r, fx_dim(fx_mix(COL_WARM, FX_CREAM, 0.5f), 0.6f + wind_up * 0.4f),
             win_x + 30.0f, win_y + 14.0f, p.x + p.w * 0.40f, lower - 8.0f,
             26.0f, 12, 3.0f);
    color_rect(r, FX_WOOD_DK, win_x + 32.0f, win_y + 12.0f, 6.0f, 4.0f);

    /* The climber in the lee of a stub of masonry, with the gust arriving on
     * the far side of it: the shelter has to be between him and the wind or
     * the picture says nothing the text does not have to say twice. */
    float climber_x = p.x + p.w * 0.34f;
    draw_figure(r, climber_x, lower, 1, POSE_CLING, look_chuck());

    float shelter_x = climber_x + 24.0f;
    color_rect(r, FX_INK, shelter_x - 1.0f, lower - 43.0f, 20.0f, 30.0f);
    fx_vgrad(r, shelter_x, lower - 42.0f, 18.0f, 29.0f,
             (SDL_Color){112, 108, 98, 255}, 255, (SDL_Color){52, 50, 46, 255}, 255);
    color_rect(r, (SDL_Color){186, 180, 164, 255}, shelter_x, lower - 42.0f,
               18.0f, 1.0f);
    draw_text(r, climber_x - 10.0f, lower + 20.0f, 1.0f, COL_HEAD, "SHELTER");

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
static void illus_console(SDL_Renderer *r, SDL_FRect p, float time)
{
    fx_vgrad(r, p.x, p.y, p.w, p.h, (SDL_Color){17, 23, 34, 255}, 255,
             (SDL_Color){11, 15, 24, 255}, 255);

    /* A cutting of the real console, brushed and lit from above. */
    float strip_y = p.y + 10.0f;
    fx_vgrad(r, p.x + 6.0f, strip_y, p.w - 12.0f, 40.0f,
             (SDL_Color){30, 40, 56, 255}, 255, (SDL_Color){13, 19, 30, 255}, 255);
    color_rect(r, (SDL_Color){60, 76, 98, 255}, p.x + 6.0f, strip_y, p.w - 12.0f, 1.0f);
    color_rect(r, (SDL_Color){38, 112, 110, 255}, p.x + 6.0f, strip_y + 41.0f,
               p.w - 12.0f, 1.0f);
    color_rect(r, COL_RUST, p.x + 6.0f, strip_y, 3.0f, 40.0f);

    draw_text(r, p.x + 16.0f, strip_y + 6.0f, 1.0f, COL_LABEL, "VITAL");
    for (int i = 0; i < 5; ++i)
        draw_heart(r, p.x + 16.0f + (float)i * 12.0f, strip_y + 20.0f, i < 3);
    color_rect(r, (SDL_Color){34, 44, 58, 255}, p.x + 86.0f, strip_y + 6.0f,
               1.0f, 28.0f);
    draw_text(r, p.x + 96.0f, strip_y + 6.0f, 1.0f, COL_LABEL, "WPN GUN");
    for (int i = 0; i < 6; ++i)
        draw_ammo_pip(r, p.x + 97.0f + (float)i * 7.0f, strip_y + 19.0f, i < 4);
    color_rect(r, (SDL_Color){34, 44, 58, 255}, p.x + 148.0f, strip_y + 6.0f,
               1.0f, 28.0f);
    draw_text(r, p.x + 158.0f, strip_y + 6.0f, 1.0f, COL_LABEL, "ACCESS");
    float blink = 0.45f + 0.55f * sinf(time * 4.0f);
    draw_access_chip(r, p.x + 158.0f, strip_y + 19.0f, 66.0f,
                     fx_dim((SDL_Color){246, 90, 70, 255}, blink),
                     (SDL_Color){54, 24, 24, 255}, (SDL_Color){124, 52, 46, 255},
                     (SDL_Color){250, 158, 128, 255}, "LOCKED");
    draw_text(r, p.x + 234.0f, strip_y + 6.0f, 1.0f, COL_LABEL, "SECTOR");
    draw_text(r, p.x + 236.0f, strip_y + 19.0f, 2.0f, (SDL_Color){226, 232, 220, 255},
              "07");

    /* The other two things the chip can say. */
    float alt_y = strip_y + 54.0f;
    draw_access_chip(r, p.x + 10.0f, alt_y, 74.0f, FX_GREEN,
                     (SDL_Color){16, 52, 40, 255}, (SDL_Color){40, 132, 96, 255},
                     (SDL_Color){168, 255, 206, 255}, "GRANTED");
    draw_text(r, p.x + 92.0f, alt_y + 3.0f, 1.0f, COL_TEXT, "DOOR IS OPEN");
    draw_access_chip(r, p.x + 10.0f, alt_y + 20.0f, 74.0f,
                     (SDL_Color){166, 142, 91, 255}, (SDL_Color){36, 38, 42, 255},
                     (SDL_Color){96, 102, 108, 255},
                     (SDL_Color){190, 190, 184, 255}, "BLOCKED");
    draw_text(r, p.x + 92.0f, alt_y + 23.0f, 1.0f, COL_TEXT, "USE THE WINDOW");

    /* The legend. One icon, one name, one line about it. */
    static const char *const names[5] = {"KEY CARD", "AMMO", "GRENADE",
                                         "MEDKIT", "ROCKET"};
    float list_y = alt_y + 52.0f;
    for (int i = 0; i < 5; ++i)
    {
        float y = list_y + (float)i * 22.0f;
        color_rect(r, (SDL_Color){15, 20, 30, 255}, p.x + 10.0f, y - 3.0f,
                   p.w - 20.0f, 18.0f);
        color_rect(r, (SDL_Color){26, 34, 48, 255}, p.x + 10.0f, y - 3.0f,
                   p.w - 20.0f, 1.0f);
        switch (i)
        {
        case 0:
            draw_icon_card(r, p.x + 18.0f, y);
            break;
        case 1:
            draw_icon_ammo(r, p.x + 18.0f, y);
            break;
        case 2:
            draw_icon_grenade(r, p.x + 18.0f, y);
            break;
        case 3:
            draw_icon_medkit(r, p.x + 18.0f, y);
            break;
        default:
            draw_icon_rocket(r, p.x + 18.0f, y);
            break;
        }
        draw_text(r, p.x + 46.0f, y + 2.0f, 1.0f, COL_TEXT, names[i]);
    }

    /* And what the idle trail meter turns into once the building is looking
     * for you, which is the one readout worth recognising in a hurry. */
    float alert_y = list_y + 5.0f * 22.0f + 12.0f;
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

/* ---- The sheets ------------------------------------------------------- */

typedef enum
{
    LINE_HEAD,
    LINE_BODY,
    LINE_BULLET,
    LINE_KEY, /* "keyboard|gamepad|action" */
    LINE_GAP
} ManualLineKind;

typedef struct
{
    ManualLineKind kind;
    const char *text;
} ManualLine;

typedef struct
{
    const char *title;
    const char *strap;
    const char *caption;
    const ManualLine *lines;
    int line_count;
    void (*illustration)(SDL_Renderer *r, SDL_FRect panel, float time);
} ManualPage;

static const ManualLine PAGE_MISSION[] = {
    {LINE_HEAD, "THE JOB"},
    {LINE_BODY, "They took her off the street and carried"},
    {LINE_BODY, "her inside. Nobody else is coming up"},
    {LINE_BODY, "after her."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "EVERY SECTOR IS THE SAME SHAPE"},
    {LINE_BULLET, "One security door leads up, and it"},
    {LINE_BODY, "starts LOCKED."},
    {LINE_BULLET, "Two ways to open it: pick up the one"},
    {LINE_BODY, "real KEY CARD, or hold E at the live"},
    {LINE_BODY, "TERMINAL for four seconds."},
    {LINE_BULLET, "Cards lie. A wrong one buzzes and"},
    {LINE_BODY, "changes nothing. Keep looking."},
    {LINE_BULLET, "Hacking wakes the building: guards are"},
    {LINE_BODY, "sent to the terminal you used."},
    {LINE_BULLET, "Four sectors have no door at all. The"},
    {LINE_BODY, "way on is the open WINDOW, out onto the"},
    {LINE_BODY, "wall itself."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "STAYING ALIVE"},
    {LINE_BULLET, "Three hearts a life. A hit costs one,"},
    {LINE_BODY, "a blast two; falls and crushes cost"},
    {LINE_BODY, "everything. Medkits refill the hearts."},
    {LINE_BULLET, "Cards, terminals and doors bank your"},
    {LINE_BODY, "progress. A lost life resumes there."},
    {LINE_BULLET, "Out of lives, a CONTINUE retries the"},
    {LINE_BODY, "same sector. The score survives three"},
    {LINE_BODY, "of those, then resets - the run never"},
    {LINE_BODY, "goes back to the lobby."},
};

static const ManualLine PAGE_CONTROLS[] = {
    {LINE_HEAD, "IN THE SECTORS"},
    {LINE_KEY, "WASD/ARROWS|LS/DPAD|MOVE - CLIMB - AIM"},
    {LINE_KEY, "W or UP|A|JUMP"},
    {LINE_KEY, "S or DOWN|DPAD|CRAWL"},
    {LINE_KEY, "SPACE|X|ATTACK"},
    {LINE_KEY, "TAB or Q|RB|NEXT WEAPON"},
    {LINE_KEY, "E|Y|USE DOOR / HOLD TO HACK"},
    {LINE_GAP, NULL},
    {LINE_HEAD, "ANYWHERE"},
    {LINE_KEY, "ENTER|START|CONFIRM - SKIP"},
    {LINE_KEY, "ESC|START|PAUSE - RESUME"},
    {LINE_KEY, "Q|BACK|QUIT TO TITLE, FROM PAUSE"},
    {LINE_KEY, "J|X|ASSIST, FROM TITLE OR PAUSE"},
    {LINE_KEY, "M|LB|MUTE"},
    {LINE_KEY, "F|-|FULLSCREEN"},
    {LINE_GAP, NULL},
    {LINE_HEAD, "TWO THINGS WORTH KNOWING"},
    {LINE_BULLET, "SPACE attacks and UP jumps. They are"},
    {LINE_BODY, "not the same key."},
    {LINE_BULLET, "On a ladder, UP or DOWN aims the shot"},
    {LINE_BODY, "straight up or straight down."},
};

static const ManualLine PAGE_MOVEMENT[] = {
    {LINE_HEAD, "GROUND"},
    {LINE_BULLET, "A jump clears a one-tile hole in the"},
    {LINE_BODY, "floor. Two tiles needs a ladder, a lift"},
    {LINE_BODY, "shaft or a moving platform."},
    {LINE_BULLET, "Hold DOWN to crawl. It is the only way"},
    {LINE_BODY, "under a one-tile gap, and the only way"},
    {LINE_BODY, "to hit something sitting on the floor."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "GOING UP"},
    {LINE_BULLET, "LADDERS: press UP or DOWN to grab one."},
    {LINE_BODY, "You can shuffle sideways on the rungs."},
    {LINE_BULLET, "LIFT SHAFTS carry you up and down, and"},
    {LINE_BODY, "so do moving platforms."},
    {LINE_BULLET, "CRACKED PANELS hold for a moment, then"},
    {LINE_BODY, "drop. They stay gone for the run."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "THINGS IN THE WAY"},
    {LINE_BULLET, "CRATES shove along the floor and make a"},
    {LINE_BODY, "step. Shots and blasts break them."},
    {LINE_BULLET, "SPIKES and CEILING FANS kill on touch."},
    {LINE_BULLET, "PAIRED DOORS link up: stand in one and"},
    {LINE_BODY, "press E to come out of the other."},
    {LINE_BULLET, "A restroom door is a room of its own,"},
    {LINE_BODY, "with a medkit in it. What you carry in"},
    {LINE_BODY, "comes back out with you."},
};

static const ManualLine PAGE_COMBAT[] = {
    {LINE_HEAD, "WHAT YOU CARRY"},
    {LINE_BULLET, "KNIFE: always with you, one tile of"},
    {LINE_BODY, "reach, and it makes no noise."},
    {LINE_BULLET, "PISTOL: six rounds. Ammo lies around"},
    {LINE_BODY, "and comes back seconds after it is"},
    {LINE_BODY, "taken. A shot carries seven tiles."},
    {LINE_BULLET, "GRENADE: one at a time. Short fuse, it"},
    {LINE_BODY, "bounces, and it does not pick sides."},
    {LINE_BULLET, "BAZOOKA: one rocket, odd sectors only."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT THEY DO"},
    {LINE_BULLET, "A GUARD sees a cone seven tiles long in"},
    {LINE_BODY, "front of him, and nothing behind it."},
    {LINE_BULLET, "Land on a guard's head to knock him"},
    {LINE_BODY, "down and bounce clear. Touch him any"},
    {LINE_BODY, "other way and it is over."},
    {LINE_BULLET, "DOGS are faster, lower, and cannot be"},
    {LINE_BODY, "stomped."},
    {LINE_BULLET, "A guard who has seen you may run for a"},
    {LINE_BODY, "wall ALARM and wake the whole floor."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "BLASTS"},
    {LINE_BULLET, "Crawl and shoot a GAS CANISTER. A"},
    {LINE_BODY, "standing shot goes straight over it."},
    {LINE_BULLET, "A blast opens a blocked-up patch of"},
    {LINE_BODY, "wall for good, and that can be a route."},
};

static const ManualLine PAGE_CLIMB[] = {
    {LINE_HEAD, "OUT THERE"},
    {LINE_BULLET, "No gravity and no ladders: you move"},
    {LINE_BODY, "four ways across the brickwork."},
    {LINE_BULLET, "Stone CORNICES are in the way, and they"},
    {LINE_BODY, "are also the only cover on the wall."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT THE WALL THROWS"},
    {LINE_BULLET, "WIND: a howl warns you, then the gust"},
    {LINE_BODY, "shoves. Get masonry upwind of you and"},
    {LINE_BODY, "it passes over."},
    {LINE_BULLET, "THROWERS lean out of a window and shout"},
    {LINE_BODY, "before they let go. A cornice between"},
    {LINE_BODY, "you shatters the brick."},
    {LINE_BULLET, "BIRDS cross at you. Masonry breaks them"},
    {LINE_BODY, "off as well."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "HEIGHT IS KEPT"},
    {LINE_BULLET, "Every three tiles of climb is banked."},
    {LINE_BULLET, "A lost life resumes at the last bank,"},
    {LINE_BODY, "not down on the pavement."},
    {LINE_BULLET, "Pickups out here are real detours: the"},
    {LINE_BODY, "loadout carries into the next sector."},
    {LINE_BULLET, "The way back inside is the WINDOW."},
};

static const ManualLine PAGE_CONSOLE[] = {
    {LINE_HEAD, "THE STRIP"},
    {LINE_BULLET, "VITAL: lives in hand. Five slots show;"},
    {LINE_BODY, "anything over that counts up beside."},
    {LINE_BULLET, "WPN: the weapon the next attack will"},
    {LINE_BODY, "use, with the rounds left beside it."},
    {LINE_BULLET, "ACCESS: LOCKED until a card or a"},
    {LINE_BODY, "terminal says otherwise. BLOCKED means"},
    {LINE_BODY, "this sector has no door -- the way on"},
    {LINE_BODY, "is the window."},
    {LINE_BULLET, "TRAIL is idle chatter. It becomes a red"},
    {LINE_BODY, "ALERT countdown while the building is"},
    {LINE_BODY, "actively looking for you."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT TO PICK UP"},
    {LINE_BULLET, "KEY CARD: one per sector is real."},
    {LINE_BULLET, "AMMO: fills the pistol back to six."},
    {LINE_BULLET, "GRENADE: one, and it selects itself."},
    {LINE_BULLET, "MEDKIT: one more life."},
    {LINE_BULLET, "ROCKET: the bazooka, one shot."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "ON THE WALL"},
    {LINE_BULLET, "The climb has a strip of its own:"},
    {LINE_BODY, "height made, the wind, and the bank."},
};

#define PAGE(lines) lines, (int)(sizeof(lines) / sizeof((lines)[0]))

static const ManualPage PAGES[] = {
    {"THE MISSION", "FIFTEEN SECTORS BETWEEN THE LOBBY AND THE ROOF",
     "FOUR OF THEM ARE ON THE OUTSIDE", PAGE(PAGE_MISSION), illus_mission},
    {"CONTROLS", "KEYBOARD AND GAMEPAD ARE BOTH ALWAYS LIVE",
     "PICK ONE UP AND THE HINTS FOLLOW", PAGE(PAGE_CONTROLS), illus_controls},
    {"ON FOOT", "WHAT THE FLOOR PLAN WILL AND WILL NOT ALLOW",
     "ONE TILE IS A JUMP. TWO IS A LADDER", PAGE(PAGE_MOVEMENT),
     illus_movement},
    {"FIGHTING", "NOTHING IN THIS BUILDING IS FRIGHTENED OF YOU",
     "BEHIND HIM IS THE SAFEST PLACE", PAGE(PAGE_COMBAT), illus_combat},
    {"THE CLIMB", "SECTORS 3, 7, 11 AND 13 ARE CLIMBED, NOT WALKED",
     "THE WIND ANNOUNCES ITSELF FIRST", PAGE(PAGE_CLIMB), illus_climb},
    {"THE CONSOLE", "READING THE STRIP ALONG THE TOP OF THE SCREEN",
     "LOCKED, GRANTED, BLOCKED", PAGE(PAGE_CONSOLE), illus_console},
};

#define MANUAL_PAGE_COUNT ((int)(sizeof(PAGES) / sizeof(PAGES[0])))

int manual_page_count(void)
{
    return MANUAL_PAGE_COUNT;
}

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

/*
 * The two chip columns are as wide as the widest label on the sheet, not as
 * wide as each row needs: a table whose columns move from row to row is a list
 * of pairs, not a table.
 */
static void key_columns(const ManualPage *page, float *out_key, float *out_pad)
{
    size_t key_max = 0;
    size_t pad_max = 0;
    char buf[32];

    for (int i = 0; i < page->line_count; ++i)
    {
        if (page->lines[i].kind != LINE_KEY)
            continue;
        key_field(page->lines[i].text, 0, buf, sizeof(buf));
        if (SDL_strlen(buf) > key_max)
            key_max = SDL_strlen(buf);
        key_field(page->lines[i].text, 1, buf, sizeof(buf));
        if (SDL_strlen(buf) > pad_max)
            pad_max = SDL_strlen(buf);
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

static void layout_chips(Manual *manual, float w, float h)
{
    float dots = (float)MANUAL_PAGE_COUNT * 10.0f;
    float prev_w = chip_width("< PREV");
    float next_w = chip_width("NEXT >");
    float back_w = chip_width("ESC BACK");
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

void manual_init(Manual *manual, int win_w, int win_h)
{
    SDL_zerop(manual);
    layout_chips(manual, win_w > 0 ? (float)win_w : 800.0f,
                 win_h > 0 ? (float)win_h : 552.0f);
}

void manual_update(Manual *manual, float dt, int win_w, int win_h,
                   float mouse_x, float mouse_y)
{
    manual->time += dt;
    if (manual->settle > 0.0f)
    {
        manual->settle -= dt * 4.5f;
        if (manual->settle < 0.0f)
            manual->settle = 0.0f;
    }

    layout_chips(manual, win_w > 0 ? (float)win_w : 800.0f,
                 win_h > 0 ? (float)win_h : 552.0f);
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
    draw_brackets(r, fx_dim(COL_RUST, 0.85f * appear), sheet, 12.0f, 2.0f);
}

static void render_header(SDL_Renderer *r, const ManualPage *page,
                          float w, float appear)
{
    draw_tracked(r, TEXT_X, 32.0f, 3.0f, fx_dim(COL_LABEL, appear),
                 "FIELD MANUAL");
    draw_text(r, TEXT_X, 46.0f, 2.0f, fx_dim(FX_CREAM, appear), page->title);
    draw_text(r, TEXT_X, 74.0f, 1.0f, fx_dim(COL_LABEL, appear), page->strap);

    float rule_y = 88.0f;
    color_rect(r, fx_dim(COL_RUST, 0.9f * appear), TEXT_X, rule_y, 64.0f, 2.0f);
    color_rect(r, fx_dim(FX_STEEL_DK, appear), TEXT_X + 70.0f, rule_y,
               w - TEXT_X * 2.0f - 70.0f, 1.0f);
}

static void render_text_column(SDL_Renderer *r, const ManualPage *page,
                               float slide, float appear)
{
    float key_w = 0.0f;
    float pad_w = 0.0f;
    key_columns(page, &key_w, &pad_w);

    float x = TEXT_X + slide;
    float y = BODY_Y;
    bool first_head = true;
    char key[32];
    char pad[32];
    char action[48];

    for (int i = 0; i < page->line_count && y < BODY_BOTTOM; ++i)
    {
        const ManualLine *line = &page->lines[i];
        switch (line->kind)
        {
        case LINE_HEAD:
            if (!first_head)
                y += HEAD_LEAD;
            first_head = false;
            draw_tracked(r, x, y, 1.0f, fx_dim(COL_HEAD, appear), line->text);
            color_rect(r, fx_dim(FX_STEEL_DK, appear), x, y + 11.0f,
                       TEXT_RIGHT - TEXT_X, 1.0f);
            y += HEAD_PITCH;
            break;
        case LINE_BULLET:
            color_rect(r, fx_dim(FX_CYAN, appear), x + 1.0f, y + 3.0f, 3.0f, 3.0f);
            draw_text(r, x + BULLET_INDENT, y, 1.0f, fx_dim(COL_TEXT, appear),
                      line->text);
            y += LINE_PITCH;
            break;
        case LINE_BODY:
            draw_text(r, x + BULLET_INDENT, y, 1.0f, fx_dim(COL_TEXT, appear),
                      line->text);
            y += LINE_PITCH;
            break;
        case LINE_KEY:
            key_field(line->text, 0, key, sizeof(key));
            key_field(line->text, 1, pad, sizeof(pad));
            key_field(line->text, 2, action, sizeof(action));
            draw_keycap(r, x, y - 3.0f, key_w, CHIP_H, key,
                        fx_dim((SDL_Color){198, 208, 200, 255}, appear));
            draw_keycap(r, x + key_w + 8.0f, y - 3.0f, pad_w, CHIP_H, pad,
                        fx_dim(COL_LABEL, appear));
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

static void render_panel(SDL_Renderer *r, const ManualPage *page,
                         float time, float slide, float appear)
{
    SDL_FRect frame = {PANEL_X + slide, PANEL_Y, PANEL_W, PANEL_H};
    SDL_FRect inner = {frame.x + PANEL_FRAME, frame.y + PANEL_FRAME,
                       frame.w - PANEL_FRAME * 2.0f,
                       frame.h - PANEL_FRAME * 2.0f};

    color_rect(r, fx_dim((SDL_Color){30, 39, 54, 255}, appear), frame.x, frame.y,
               frame.w, frame.h);
    color_rect(r, fx_dim(FX_INK, appear), inner.x - 1.0f, inner.y - 1.0f,
               inner.w + 2.0f, inner.h + 2.0f);

    page->illustration(r, inner, time);

    /* The wash a turning sheet passes under. The illustrations do not know
     * about the page turn, so the veil is what carries them through it. */
    fx_rect_a(r, FX_INK, (Uint8)(215.0f * (1.0f - appear)), inner.x, inner.y,
              inner.w, inner.h);
    draw_brackets(r, fx_dim(COL_RUST, 0.7f * appear), frame, 10.0f, 2.0f);

    /* Clipped to the plate's own width rather than trusted to be short: a
     * caption that outgrows the panel runs off the sheet entirely. */
    char caption[CAPTION_MAX + 1];
    SDL_strlcpy(caption, page->caption, sizeof(caption));
    draw_text(r, frame.x, frame.y + frame.h + 8.0f, 1.0f,
              fx_dim(COL_LABEL, appear), caption);
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
    fx_rect_a(r, hot && live ? COL_RUST : (SDL_Color){68, 84, 102, 255},
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
    render_chip(r, manual->back_chip, "ESC BACK",
                manual->hovered == MANUAL_HOT_BACK, true, appear);

    /* Where in the sheaf this sheet is. */
    float x = manual->prev_chip.x + manual->prev_chip.w + 18.0f;
    float y = manual->prev_chip.y + CHIP_H * 0.5f - 2.0f;
    for (int i = 0; i < MANUAL_PAGE_COUNT; ++i)
    {
        bool live = i == manual->page;
        color_rect(r, fx_dim(live ? COL_HEAD : FX_STEEL_DK, appear),
                   x + (float)i * 10.0f, live ? y - 1.0f : y, 6.0f,
                   live ? 4.0f : 3.0f);
    }

    /* A divider before the way out, so BACK does not read as a third page
     * button. */
    color_rect(r, fx_dim(FX_STEEL_DK, appear),
               manual->back_chip.x - 14.0f, manual->prev_chip.y + 2.0f, 1.0f,
               CHIP_H - 4.0f);
}

void manual_render(SDL_Renderer *r, const Manual *manual, int win_w, int win_h)
{
    float w = win_w > 0 ? (float)win_w : 800.0f;
    float h = win_h > 0 ? (float)win_h : 552.0f;
    const ManualPage *page = &PAGES[manual->page];

    float appear = smoothstep01(manual->time / 0.30f);
    /* A turned sheet arrives from the side it came from and settles. */
    float settle = smoothstep01(manual->settle);
    float slide = (float)manual->settle_dir * settle * 16.0f;
    float content = appear * (1.0f - settle * 0.85f);

    render_desk(r, w, h);
    render_sheet(r, sheet_rect(w, h), appear);
    render_header(r, page, w, content);
    render_text_column(r, page, slide, content);
    render_panel(r, page, manual->time, slide, content);
    render_footer(r, manual, appear);

    fx_vignette(r, win_w, win_h, 64);
    fx_scanlines(r, win_w, win_h, 11);
}
