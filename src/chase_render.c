#include "chase_render.h"

#include "fx.h"

#include <math.h>

/*
 * The pursuit is drawn from above, so this file is a small top-down companion
 * to game_render.c: same procedural, no-assets approach, same fx.h palette and
 * lighting vocabulary, just a different camera. Nothing here reads or writes
 * simulation state; every animation is derived from `Chase`.
 */

/* Road space is drawn 1:1, so the visible strip of road is one view height. */
typedef struct
{
    float road_left;
    float view_top;
    float view_h;
    float camera_y;
    float shake_x;
    float shake_y;
    /* The player's reduced-motion switch, carried down to the one thing out
     * here that strobes: the light bars on the cordon. It rides in this struct
     * because this struct is already threaded through every part of the road,
     * which is the same reason the shake offsets do. */
    bool steady_lights;
} ChaseView;

/* The chase's material table: the road surfaces, markings and glass this
 * scene alone is made of, named once here the way level_art.c names walls. */
static const SDL_Color COL_ASPHALT = {26, 30, 38, 255};
static const SDL_Color COL_ASPHALT_LT = {35, 40, 49, 255};
static const SDL_Color COL_PAVEMENT = {46, 52, 62, 255};
static const SDL_Color COL_KERB = {96, 104, 114, 255};
static const SDL_Color COL_PAINT = {198, 204, 194, 255};
static const SDL_Color COL_PAINT_MID = {150, 146, 96, 255};
static const SDL_Color COL_GLASS = {11, 20, 28, 255};

/* Wreck soot: warm neutral chars for burnt-out panels, deliberately off the
 * blue-slate ramp so a burnt car reads as burnt rather than repainted. */
static const SDL_Color SOOT = {44, 40, 40, 255};
static const SDL_Color SOOT_LT = {62, 56, 54, 255};
static const SDL_Color SOOT_DK = {30, 28, 28, 255};

typedef struct
{
    SDL_Color body;
    SDL_Color body_lt;
    SDL_Color roof;
} CarPaint;

static const CarPaint TRAFFIC_PAINT[4] = {
    {{118, 60, 54, 255}, {152, 86, 74, 255}, {78, 40, 38, 255}},
    {{56, 74, 92, 255}, {86, 108, 126, 255}, {36, 50, 64, 255}},
    {{94, 96, 86, 255}, {128, 130, 116, 255}, {60, 64, 58, 255}},
    {{138, 130, 96, 255}, {174, 166, 124, 255}, {94, 88, 66, 255}}};

/* Chuck's car wears his jacket: the FX_HERO ramp, not a fourth blue. A
 * function because the fx.h colours are const objects, which C17 will not
 * accept inside a static initializer. */
static CarPaint player_paint(void)
{
    return (CarPaint){FX_HERO, FX_HERO_LT, FX_HERO_DK};
}

static const CarPaint TARGET_PAINT = {
    {38, 44, 46, 255}, {62, 68, 66, 255}, {24, 28, 30, 255}};

/* Cordon livery: near-black with a white flank stripe implied by the light
 * roof. Kept well away from TARGET_PAINT's charcoal — the one car in this
 * scene the player must never mistake for another is the SUV. */
static const CarPaint CORDON_PAINT = {
    {30, 38, 56, 255}, {188, 194, 198, 255}, {22, 28, 42, 255}};

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static float screen_x(const ChaseView *view, float x)
{
    return view->road_left + x + view->shake_x;
}

static float screen_y(const ChaseView *view, float y)
{
    return view->view_top + view->view_h - (y - view->camera_y) + view->shake_y;
}

static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      SDL_Color color, const char *text)
{
    SDL_SetRenderScale(r, scale, scale);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 255);
    SDL_RenderDebugText(r, x / scale, y / scale, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static float text_width(const char *text, float scale)
{
    return (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

static void draw_text_centered(SDL_Renderer *r, float center_x, float y,
                               float scale, SDL_Color color, const char *text)
{
    draw_text(r, center_x - text_width(text, scale) * 0.5f, y, scale, color, text);
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
 */
#define CHASE_PROMPT_MARGIN 12.0f

static void draw_skip_prompt(SDL_Renderer *r, int win_w, int win_h, float blink,
                             const PadHints *pad, const char *offer)
{
    char pad_form[64];
    char key_form[64];
    char spelled[64];
    SDL_snprintf(pad_form, sizeof(pad_form), "%s%s", PAD_CONFIRM_DRIVE, offer);
    SDL_snprintf(key_form, sizeof(key_form), "%s%s", PAD_CONFIRM_KEYS, offer);
    const char *line =
        pad_hint(pad, spelled, sizeof(spelled), pad_form, key_form);
    draw_text(r, (float)win_w - text_width(line, 1.0f) - CHASE_PROMPT_MARGIN,
              (float)win_h - 31.0f, 1.0f, fx_dim(FX_STEEL_LT, blink), line);
}

/* ---- Cars ------------------------------------------------------------ */

/*
 * Every car is drawn in its own frame: `along` runs from tail (-0.5) to nose
 * (+0.5) and `across` from the left flank (-0.5) to the right (+0.5). The
 * frame's nose vector is axis-aligned, so one set of artwork serves the lanes
 * and the cross streets without any rotation maths.
 */
typedef struct
{
    float cx, cy;
    float ax, ay;
    float length;
    float width;
} CarFrame;

/*
 * A rectangle in the car's own frame, measured in pixels from its centre: `a`
 * along the nose, `b` across toward the frame's right flank. Every part of a
 * car goes through here, so each car is drawn once whichever way it points.
 * The frame's centre is snapped to a whole pixel before any part is laid, so
 * the parts of one car can never round apart from each other as it moves.
 */
static void car_px(SDL_Renderer *r, const CarFrame *f, float a0, float a1,
                   float b0, float b1, SDL_Color c, Uint8 alpha)
{
    float rx = -f->ay;
    float ry = f->ax;
    float x0 = f->cx + f->ax * a0 + rx * b0;
    float y0 = f->cy + f->ay * a0 + ry * b0;
    float x1 = f->cx + f->ax * a1 + rx * b1;
    float y1 = f->cy + f->ay * a1 + ry * b1;
    float left = roundf(fminf(x0, x1));
    float top = roundf(fminf(y0, y1));
    float w = roundf(fmaxf(x0, x1)) - left;
    float h = roundf(fmaxf(y0, y1)) - top;
    if (w < 1.0f || h < 1.0f)
        return;
    if (alpha == 255)
        fx_rect(r, c, left, top, w, h);
    else
        fx_rect_a(r, c, alpha, left, top, w, h);
}

/* A span on one flank: `inner` to `outer` pixels off the centre line, on the
 * right flank for `side` > 0 and the left one otherwise. */
static void car_side(SDL_Renderer *r, const CarFrame *f, float side,
                     float a0, float a1, float inner, float outer,
                     SDL_Color c, Uint8 alpha)
{
    if (side > 0.0f)
        car_px(r, f, a0, a1, inner, outer, c, alpha);
    else
        car_px(r, f, a0, a1, -outer, -inner, c, alpha);
}

/*
 * A panel with its corners rounded, one strip per row of the curve so the
 * rows never overlap — which matters for the translucent ones, where an
 * overlap would print as a darker seam. A car built out of square boxes reads
 * as a box with wheels however well it is lit; the corner is the tell.
 */
static void car_mass(SDL_Renderer *r, const CarFrame *f, float a0, float a1,
                     float b0, float b1, float radius, SDL_Color c, Uint8 alpha)
{
    if (radius < 0.0f)
        radius = 0.0f;
    int rows = (int)radius;
    for (int k = 0; k < rows; ++k)
    {
        float d = radius - (float)k - 0.5f;
        float inset = floorf(radius - sqrtf(fmaxf(0.0f, radius * radius - d * d)) +
                             0.5f);
        car_px(r, f, a0 + inset, a1 - inset, b0 + (float)k,
               b0 + (float)k + 1.0f, c, alpha);
        car_px(r, f, a0 + inset, a1 - inset, b1 - (float)k - 1.0f,
               b1 - (float)k, c, alpha);
    }
    car_px(r, f, a0, a1, b0 + (float)rows, b1 - (float)rows, c, alpha);
}

/* A rectangle that fades along the car's own axis, from `alpha0` at `a0` to
 * `alpha1` at `a1`: a headlamp's throw, a tail lamp's smear on wet asphalt. */
static void car_fade(SDL_Renderer *r, const CarFrame *f, float a0, float a1,
                     float b0, float b1, SDL_Color c, float alpha0,
                     float alpha1)
{
    float rx = -f->ay;
    float ry = f->ax;
    SDL_FColor c0 = fx_fcolor(c, alpha0);
    SDL_FColor c1 = fx_fcolor(c, alpha1);
    SDL_Vertex v[4] = {
        {{f->cx + f->ax * a0 + rx * b0, f->cy + f->ay * a0 + ry * b0}, c0,
         {0.0f, 0.0f}},
        {{f->cx + f->ax * a0 + rx * b1, f->cy + f->ay * a0 + ry * b1}, c0,
         {0.0f, 0.0f}},
        {{f->cx + f->ax * a1 + rx * b1, f->cy + f->ay * a1 + ry * b1}, c1,
         {0.0f, 0.0f}},
        {{f->cx + f->ax * a1 + rx * b0, f->cy + f->ay * a1 + ry * b0}, c1,
         {0.0f, 0.0f}}};
    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, indices, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* A point in the car's frame, in screen space. */
static void car_point(const CarFrame *f, float a, float b, float *x, float *y)
{
    *x = f->cx + f->ax * a - f->ay * b;
    *y = f->cy + f->ay * a + f->ax * b;
}

/*
 * Which end and which flank of a car face the moon.
 *
 * The moon hangs up and to the left of the frame, as it does over every roof
 * either side of this road, so the side of a car that catches it is a fact
 * about the screen and not about the car: a car driving down the far lanes
 * shows the moon its right flank and its tail. The frame is rotated, so the
 * answer has to be asked of it — the old artwork put its lit flank on the
 * car's own left and lit every oncoming car from the wrong side.
 * +1 is the nose / the right flank.
 */
static float car_moon_end(const CarFrame *f)
{
    return -(f->ax + f->ay) > 0.0f ? 1.0f : -1.0f;
}

static float car_moon_flank(const CarFrame *f)
{
    return f->ay - f->ax > 0.0f ? 1.0f : -1.0f;
}

static void draw_head_beam(SDL_Renderer *r, const CarFrame *f, float length,
                           float spread, SDL_Color color, float alpha)
{
    float rx = -f->ay;
    float ry = f->ax;
    float nose_x = f->cx + f->ax * f->length * 0.5f;
    float nose_y = f->cy + f->ay * f->length * 0.5f;
    float half = f->width * 0.34f;
    SDL_FColor near_color = fx_fcolor(color, alpha);
    SDL_FColor far_color = fx_fcolor(color, 0.0f);
    SDL_Vertex v[4] = {
        {{nose_x - rx * half, nose_y - ry * half}, near_color, {0.0f, 0.0f}},
        {{nose_x + rx * half, nose_y + ry * half}, near_color, {0.0f, 0.0f}},
        {{nose_x + f->ax * length + rx * spread,
          nose_y + f->ay * length + ry * spread},
         far_color,
         {0.0f, 0.0f}},
        {{nose_x + f->ax * length - rx * spread,
          nose_y + f->ay * length - ry * spread},
         far_color,
         {0.0f, 0.0f}}};
    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, indices, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* Where the throw lands hardest, the wet road gives it back: a hot patch
     * a car's length out, which is what a beam on rain actually looks like
     * from above and what stops it reading as a pane of frosted glass. */
    float hot_x = nose_x + f->ax * length * 0.30f;
    float hot_y = nose_y + f->ay * length * 0.30f;
    fx_glow(r, hot_x, hot_y, length * 0.26f, color, (Uint8)(alpha * 190.0f));
}

/*
 * What stands on a car's footprint, as fractions of its half length from the
 * centre: where the windscreen starts, where the roof starts and ends, and
 * where the rear glass ends. The outline and the wheels are the same for every
 * car because those are what the simulation collides; only the cabin changes,
 * which is enough to make a road of traffic read as several kinds of car.
 */
typedef struct
{
    float screen;     /* front edge of the windscreen */
    float roof;       /* windscreen meets roof */
    float roof_back;  /* roof meets rear glass */
    float tail_glass; /* back edge of the rear glass */
    float cabin;      /* how far in from the flank the roof sits, px */
    float radius;     /* corner rounding, px */
} CarShape;

static const CarShape SHAPE_SALOON = {0.40f, 0.12f, -0.46f, -0.68f, 7.0f, 5.0f};
static const CarShape SHAPE_HATCH = {0.42f, 0.16f, -0.62f, -0.82f, 7.0f, 5.0f};
static const CarShape SHAPE_ESTATE = {0.40f, 0.12f, -0.72f, -0.84f, 7.0f, 4.0f};
/* Chuck's car is the one fastback on the road: a long bonnet and the rear
 * glass laid nearly flat, so it is his from the first frame. */
static const CarShape SHAPE_COUPE = {0.32f, 0.04f, -0.46f, -0.70f, 8.0f, 6.0f};
/* The SUV is a box on purpose — short bonnet, a roof that runs almost to
 * the tail and a near-vertical tailgate — so it cannot be mistaken for any of
 * the rounded cars around it. */
static const CarShape SHAPE_SUV = {0.54f, 0.32f, -0.80f, -0.88f, 6.0f, 3.0f};

typedef struct
{
    CarPaint paint;
    const CarShape *shape;
    bool headlamps;  /* lit lenses at the nose */
    bool tail_lamps; /* running lights at the tail */
    bool braking;
    float wreck;
    unsigned seed; /* per-car salt, for what a wreck has lost */
    bool tinted;   /* privacy glass: darker panes, a dimmer sky in them */
} CarLook;

/* The car's centre snapped to a whole pixel, which every part is laid from. */
static CarFrame car_snapped(const CarFrame *f)
{
    CarFrame s = *f;
    s.cx = floorf(s.cx + 0.5f);
    s.cy = floorf(s.cy + 0.5f);
    return s;
}

static void draw_car_body(SDL_Renderer *r, const CarFrame *frame,
                          const CarLook *look)
{
    CarFrame f = car_snapped(frame);
    const CarShape *shape = look->shape;
    const float hl = floorf(f.length * 0.5f);
    const float hw = floorf(f.width * 0.5f);
    const float me = car_moon_end(&f);
    const float mf = car_moon_flank(&f);
    const float rad = shape->radius;
    const float wreck = look->wreck;
    const bool intact = wreck <= 0.0f;

    SDL_Color body = look->paint.body;
    SDL_Color lit = look->paint.body_lt;
    SDL_Color shade = look->paint.roof;
    /* Glass at night gives back the sky, which is darker than paint and
     * lighter than a hole: the pane is the deep glass lifted a step toward
     * the slate of the clouds over it. */
    SDL_Color glass = fx_mix(COL_GLASS, FX_STEEL_DK, look->tinted ? 0.08f : 0.34f);
    if (!intact)
    {
        /* Scorched panels, and less of the crisp highlight. */
        float burn = clamp01(wreck * 0.9f);
        body = fx_mix(body, SOOT, burn);
        lit = fx_mix(lit, SOOT_LT, burn);
        shade = fx_mix(shade, SOOT_DK, burn);
        glass = fx_mix(glass, SOOT_DK, burn * 0.6f);
    }

    /* The shadow the moon throws down and to the right: a soft skirt and a
     * denser core, so it reads as light the car blocks rather than as a
     * second, black car under it. */
    CarFrame drop = f;
    drop.cx += 5.0f;
    drop.cy += 6.0f;
    car_mass(r, &drop, -hl, hl, -hw, hw, rad + 1.0f, FX_INK, 46);
    drop = f;
    drop.cx += 3.0f;
    drop.cy += 4.0f;
    car_mass(r, &drop, -hl + 1.0f, hl - 1.0f, -hw + 1.0f, hw - 1.0f, rad,
             FX_INK, 98);

    /* Tyres first, so the body sits over them and only the shoulder of each
     * one shows past the flank — which is all of a wheel a roof-top camera
     * gets, and is what says "car" before anything else does. */
    SDL_Color tyre = {14, 16, 20, 255};
    SDL_Color sidewall = fx_mix(tyre, FX_STEEL, 0.55f);
    float axle = floorf(hl * 0.60f);
    float tread = floorf(hl * 0.17f);
    for (int end = -1; end <= 1; end += 2)
    {
        float a = (float)end * axle;
        for (int flank = -1; flank <= 1; flank += 2)
        {
            float side = (float)flank;
            car_side(r, &f, side, a - tread - 1.0f, a + tread + 1.0f,
                     hw - 6.0f, hw + 3.0f, FX_INK, 255);
            car_side(r, &f, side, a - tread, a + tread, hw - 6.0f, hw + 2.0f,
                     tyre, 255);
            if (side == mf)
                car_side(r, &f, side, a - tread + 1.0f, a + tread - 1.0f,
                         hw + 1.0f, hw + 2.0f, sidewall, 255);
        }
    }

    /* The shell, built as three nested masses: the shade the curve of the
     * body turns away from the moon, the lit shoulder that turns toward it,
     * and the paint between. The insets are what put the moon's side on the
     * moon's side whichever way the car is pointed. */
    float moon_b0 = mf < 0.0f ? 1.0f : 0.0f;
    float moon_b1 = mf > 0.0f ? 1.0f : 0.0f;
    float moon_a0 = me < 0.0f ? 1.0f : 0.0f;
    float moon_a1 = me > 0.0f ? 1.0f : 0.0f;
    car_mass(r, &f, -hl, hl, -hw, hw, rad + 1.0f, FX_INK, 255);
    car_mass(r, &f, -hl + 1.0f, hl - 1.0f, -hw + 1.0f, hw - 1.0f, rad, shade,
             255);
    car_mass(r, &f, -hl + 2.0f - moon_a0, hl - 2.0f + moon_a1,
             -hw + 4.0f - 3.0f * moon_b0, hw - 4.0f + 3.0f * moon_b1,
             rad - 1.0f, lit, 255);
    car_mass(r, &f, -hl + 2.0f, hl - 2.0f, -hw + 3.0f, hw - 3.0f, rad - 1.0f,
             body, 255);

    const float screen_a = floorf(shape->screen * hl + 0.5f);
    const float roof_a = floorf(shape->roof * hl + 0.5f);
    const float roof_back_a = floorf(shape->roof_back * hl + 0.5f);
    const float tail_a = floorf(shape->tail_glass * hl + 0.5f);
    const float roof_hw = hw - shape->cabin;

    /* Two creases down the bonnet: the one toward the moon catches it, the
     * other falls into shade, and between them the panel reads as raised. */
    SDL_Color crease_lit = fx_mix(body, lit, 0.55f);
    SDL_Color crease_dark = fx_mix(body, shade, 0.55f);
    car_side(r, &f, mf, screen_a + 2.0f, hl - 6.0f, hw - 10.0f, hw - 9.0f,
             crease_lit, 255);
    car_side(r, &f, -mf, screen_a + 2.0f, hl - 6.0f, hw - 10.0f, hw - 9.0f,
             crease_dark, 255);
    /* The boot lid's shut line. */
    car_px(r, &f, -hl + 6.0f, -hl + 7.0f, -hw + 6.0f, hw - 6.0f, crease_dark,
           255);

    /* Door shut lines across both shoulders, at the pillar and mid-cabin. */
    float door_mid = floorf((roof_a + roof_back_a) * 0.5f);
    for (int flank = -1; flank <= 1; flank += 2)
    {
        car_side(r, &f, (float)flank, roof_a, roof_a + 1.0f, roof_hw + 3.0f,
                 hw - 1.0f, crease_dark, 255);
        car_side(r, &f, (float)flank, door_mid, door_mid + 1.0f,
                 roof_hw + 3.0f, hw - 1.0f, crease_dark, 255);
    }

    /* Glass: side windows as a strip each side of the roof, the windscreen
     * widening from the roof down to the bonnet, the rear glass narrowing.
     * Each pane carries a reflection of the sky on the side toward the moon —
     * glass is the one material on a car that shows what is above it. */
    SDL_Color sheen = fx_mix(glass, FX_PALE, intact ? 0.34f : 0.18f);
    SDL_Color sheen_soft = fx_mix(glass, FX_STEEL, 0.42f);
    SDL_Color glass_far = fx_mix(glass, COL_GLASS, 0.55f);
    car_side(r, &f, mf, roof_back_a, roof_a, roof_hw - 1.0f, roof_hw + 3.0f,
             glass, 255);
    car_side(r, &f, -mf, roof_back_a, roof_a, roof_hw - 1.0f, roof_hw + 3.0f,
             glass_far, 255);
    car_side(r, &f, mf, roof_back_a + 2.0f, roof_a - 1.0f, roof_hw + 2.0f,
             roof_hw + 3.0f, sheen_soft, 255);
    /* The B pillar breaks the side glass in two. */
    for (int flank = -1; flank <= 1; flank += 2)
        car_side(r, &f, (float)flank, door_mid - 1.0f, door_mid + 1.0f,
                 roof_hw - 1.0f, roof_hw + 3.0f, flank == (int)mf ? body : shade,
                 255);

    float rows = screen_a - roof_a;
    for (float a = roof_a; a < screen_a; a += 1.0f)
    {
        float t = rows > 1.0f ? (a - roof_a) / (rows - 1.0f) : 0.0f;
        float half = roof_hw + floorf(t * 2.0f + 0.5f);
        car_px(r, &f, a, a + 1.0f, -half, half, glass, 255);
        /* A streak laid diagonally across the pane, and a thinner echo of
         * it: the sky caught in curved glass. */
        float u = 2.0f + floorf(t * 6.0f);
        car_side(r, &f, mf, a, a + 1.0f, half - u - 3.0f, half - u, sheen,
                 255);
        car_side(r, &f, mf, a, a + 1.0f, half - u - 7.0f, half - u - 6.0f,
                 sheen_soft, 255);
    }
    /* The top rail of the windscreen, in the roof's own shadow. */
    car_px(r, &f, roof_a, roof_a + 1.0f, -roof_hw, roof_hw, FX_INK, 150);

    float back_rows = roof_back_a - tail_a;
    for (float a = tail_a; a < roof_back_a; a += 1.0f)
    {
        float t = back_rows > 1.0f ? (roof_back_a - 1.0f - a) / (back_rows - 1.0f)
                                   : 0.0f;
        float half = roof_hw - floorf(t * 1.5f + 0.5f);
        car_px(r, &f, a, a + 1.0f, -half, half, glass_far, 255);
        car_side(r, &f, mf, a, a + 1.0f, half - 5.0f, half - 3.0f,
                 sheen_soft, 255);
    }

    /* The roof: the highest, flattest panel on the car, so the one the moon
     * reaches most squarely. A lit edge toward it, a shaded edge away, and a
     * soft highlight near the corner nearest the sky. */
    SDL_Color roof = fx_mix(body, lit, look->tinted ? 0.48f : 0.30f);
    car_mass(r, &f, roof_back_a, roof_a, -roof_hw, roof_hw, 2.0f, roof, 255);
    car_side(r, &f, mf, roof_back_a + 1.0f, roof_a - 1.0f, roof_hw - 1.0f,
             roof_hw, lit, 255);
    car_side(r, &f, -mf, roof_back_a + 1.0f, roof_a - 1.0f, roof_hw - 1.0f,
             roof_hw, fx_mix(roof, shade, 0.6f), 255);
    {
        float spot_a = me > 0.0f ? roof_a - 6.0f : roof_back_a + 2.0f;
        car_side(r, &f, mf, spot_a, spot_a + 4.0f, roof_hw - 7.0f,
                 roof_hw - 2.0f, fx_mix(roof, FX_PALE, intact ? 0.20f : 0.06f),
                 255);
    }

    /* Mirrors, at the foot of the A pillars. */
    for (int flank = -1; flank <= 1; flank += 2)
    {
        float side = (float)flank;
        car_side(r, &f, side, screen_a - 5.0f, screen_a, hw - 1.0f, hw + 3.0f,
                 FX_INK, 255);
        car_side(r, &f, side, screen_a - 4.0f, screen_a - 1.0f, hw - 1.0f,
                 hw + 2.0f, side == mf ? lit : shade, 255);
    }

    /* Nose: a grille between two lamps, and the lamps themselves. A lit lens
     * is bright enough for the frame's bloom to take it, which is what makes
     * it a light rather than a white sticker. */
    car_px(r, &f, hl - 3.0f, hl - 1.0f, -hw * 0.30f, hw * 0.30f,
           fx_dim(shade, 0.55f), 255);
    SDL_Color lens_off = fx_mix(glass, FX_STEEL_LT, 0.30f);
    for (int flank = -1; flank <= 1; flank += 2)
    {
        float side = (float)flank;
        float inner = floorf(hw * 0.36f);
        float outer = floorf(hw * 0.80f);
        car_side(r, &f, side, hl - 5.0f, hl - 4.0f, inner, outer, FX_INK, 200);
        if (look->headlamps && intact)
        {
            car_side(r, &f, side, hl - 4.0f, hl - 1.0f, inner, outer,
                     fx_mix(FX_PALE, FX_CREAM, 0.55f), 255);
            car_side(r, &f, side, hl - 3.0f, hl - 1.0f, outer - 4.0f,
                     outer - 1.0f, FX_CREAM, 255);
        }
        else
        {
            car_side(r, &f, side, hl - 4.0f, hl - 1.0f, inner, outer, lens_off,
                     255);
        }
    }

    /* Tail: running lamps, lit harder on the brake. A brake lamp is FX_RED
     * lit, not a new red: the ramp's own bright step. */
    for (int flank = -1; flank <= 1; flank += 2)
    {
        float side = (float)flank;
        float inner = floorf(hw * 0.42f);
        float outer = floorf(hw * 0.84f);
        if (!intact)
        {
            car_side(r, &f, side, -hl + 1.0f, -hl + 4.0f, inner, outer,
                     fx_mix(FX_RED_DK, SOOT_DK, 0.6f), 255);
            continue;
        }
        SDL_Color lamp = FX_RED_DK;
        SDL_Color core = fx_mix(FX_RED_DK, FX_RED, 0.40f);
        if (look->braking)
        {
            lamp = fx_ramp(FX_RED).lit;
            core = fx_mix(FX_RED, FX_CREAM, 0.45f);
        }
        else if (look->tail_lamps)
        {
            lamp = fx_mix(FX_RED_DK, FX_RED, 0.55f);
            core = FX_RED;
        }
        car_side(r, &f, side, -hl + 1.0f, -hl + 4.0f, inner, outer, lamp, 255);
        car_side(r, &f, side, -hl + 1.0f, -hl + 3.0f, outer - 4.0f,
                 outer - 1.0f, core, 255);
        if (look->tail_lamps || look->braking)
        {
            /* The lamp's smear on the wet road behind the car. */
            float mid = (inner + outer) * 0.5f * side;
            car_fade(r, &f, -hl - 1.0f, -hl - (look->braking ? 26.0f : 16.0f),
                     mid - 3.0f, mid + 3.0f, FX_RED,
                     look->braking ? 0.34f : 0.20f, 0.0f);
            if (look->braking)
            {
                float gx = 0.0f;
                float gy = 0.0f;
                car_point(&f, -hl + 2.0f, mid, &gx, &gy);
                fx_glow(r, gx, gy, 16.0f, FX_RED, 96);
            }
        }
    }

    if (!intact)
    {
        /* Crazed glass and a scorch across the bonnet, keyed to the car rather
         * than to where it has drifted to, so the damage holds still. */
        for (unsigned i = 0; i < 9u; ++i)
        {
            unsigned h = fx_hash(look->seed * 2654435761u + i * 0x9e3779b9u);
            float a = roof_a + (float)fx_spread(h, screen_a - roof_a);
            if (i >= 6u)
                a = tail_a + (float)fx_spread(h, roof_back_a - tail_a);
            float b = -roof_hw + 1.0f + (float)fx_spread(h >> 10,
                                                         roof_hw * 2.0f - 2.0f);
            car_px(r, &f, a, a + 1.0f, b, b + ((h >> 20) & 1u ? 2.0f : 1.0f),
                   fx_mix(glass, FX_PALE, 0.45f), 255);
        }
        car_mass(r, &f, screen_a + 4.0f, hl - 6.0f, -hw + 8.0f, hw - 8.0f,
                 3.0f, FX_INK, (Uint8)(clamp01(wreck) * 110.0f));
    }
}

/*
 * The smoke off a wreck: a few puffs, each born at the engine bay, drifting
 * away from the moon on the wind and thinning as it goes. Everything comes off
 * how long the car has been a wreck, so it is the same smoke every time the
 * moment is drawn.
 */
static void draw_wreck_smoke(SDL_Renderer *r, const CarFrame *f, float wreck,
                             unsigned seed)
{
    float strength = clamp01(1.3f - wreck * 0.10f);
    if (strength <= 0.0f)
        return;
    float ox = 0.0f;
    float oy = 0.0f;
    car_point(f, f->length * 0.30f, 0.0f, &ox, &oy);
    for (unsigned k = 0; k < 5u; ++k)
    {
        unsigned h = fx_hash(seed + k * 0x85ebca6bu);
        float life = fmodf(wreck * 0.55f + (float)k * 0.2f, 1.0f);
        float sway = (float)(h % 7u) - 3.0f;
        float px = ox + life * 26.0f + sway * life;
        float py = oy + life * 18.0f - (float)((h >> 8) % 5u);
        float radius = 5.0f + life * 13.0f;
        Uint8 alpha = (Uint8)((1.0f - life) * 120.0f * strength);
        fx_glow(r, px, py, radius, fx_mix(FX_STEEL_DK, FX_STEEL, 0.4f), alpha);
        /* The moon on the top of each puff. */
        fx_glow(r, px - radius * 0.25f, py - radius * 0.25f, radius * 0.55f,
                fx_mix(FX_STEEL, FX_PALE, 0.25f), (Uint8)(alpha * 0.7f));
    }
}

static void draw_wreck_debris(SDL_Renderer *r, const CarFrame *f, float wreck,
                              unsigned seed)
{
    /* A short burst of glass and sparks, then just a dead car in the road.
     * Keyed to the car and not to its screen x: a wreck drifts, and a burst
     * hashed off where it has drifted to re-rolled itself every pixel. */
    if (wreck > 0.7f)
        return;
    float fade = 1.0f - wreck / 0.7f;
    for (unsigned i = 0; i < 10u; ++i)
    {
        unsigned h = fx_hash(i * 2246822519u + seed);
        float angle = (float)(h % 628u) * 0.01f;
        float distance = 8.0f + (float)((h >> 9) % 34u) * wreck * 3.0f;
        SDL_Color spark = (i & 1u) ? FX_AMBER : fx_mix(FX_PALE, FX_CREAM, 0.7f);
        fx_rect_a(r, spark, (Uint8)(fade * 210.0f),
                  f->cx + cosf(angle) * distance,
                  f->cy + sinf(angle) * distance, 2.0f, 2.0f);
    }
    fx_glow(r, f->cx, f->cy, 30.0f + wreck * 26.0f, FX_AMBER,
            (Uint8)(fade * 120.0f));
}

static void draw_traffic_car(SDL_Renderer *r, const ChaseView *view,
                             const ChaseCar *car, int slot)
{
    CarFrame frame;
    frame.cx = screen_x(view, car->x);
    frame.cy = screen_y(view, car->y);
    frame.length = CHASE_CAR_LENGTH;
    frame.width = CHASE_CAR_WIDTH;
    if (car->kind == CHASE_CAR_CROSSING)
    {
        /* Which way it faces, not which way it is moving: a car waiting at a
         * standstill has no velocity to read, and a wreck is sliding wherever
         * the crash knocked it. */
        frame.ax = car->heading < 0.0f ? -1.0f : 1.0f;
        frame.ay = 0.0f;
    }
    else
    {
        frame.ax = 0.0f;
        frame.ay = car->kind == CHASE_CAR_ONCOMING ? 1.0f : -1.0f;
    }

    /* The slot a car occupies is its identity for as long as it is on the
     * road, so its body style and what a crash breaks on it hold still. */
    unsigned seed = fx_hash((unsigned)slot * 2654435761u +
                            (unsigned)car->variant * 40503u + 0x51u);
    static const CarShape *const TRAFFIC_SHAPES[3] = {
        &SHAPE_SALOON, &SHAPE_HATCH, &SHAPE_ESTATE};
    bool wrecked = car->wreck_time > 0.0f;
    CarLook look = {TRAFFIC_PAINT[car->variant & 3], TRAFFIC_SHAPES[seed % 3u],
                    true, true, false, car->wreck_time, seed, false};

    if (!wrecked)
    {
        /* Everyone out tonight has their lights on. The cars coming at the
         * player throw the long beam; the ones ahead of him throw a shorter,
         * fainter one away up the road. */
        if (car->kind == CHASE_CAR_TRAFFIC)
            draw_head_beam(r, &frame, 92.0f, CHASE_CAR_WIDTH * 0.70f, FX_CREAM,
                           0.10f);
        else
            draw_head_beam(r, &frame, 132.0f, CHASE_CAR_WIDTH * 0.85f,
                           FX_CREAM, 0.20f);
    }
    draw_car_body(r, &frame, &look);

    /* One in three of the pale cars is a cab, and says so with a lamp on the
     * roof — amber, the palette's word for a light. */
    if (!wrecked && (car->variant & 3) == 3 && (seed >> 8) % 3u == 0u)
    {
        CarFrame f = car_snapped(&frame);
        float a = floorf((look.shape->roof + look.shape->roof_back) * 0.5f *
                         CHASE_CAR_LENGTH * 0.5f);
        car_px(r, &f, a - 3.0f, a + 3.0f, -7.0f, 7.0f, FX_INK, 255);
        car_px(r, &f, a - 2.0f, a + 2.0f, -6.0f, 6.0f, FX_AMBER_DK, 255);
        car_px(r, &f, a - 1.0f, a + 1.0f, -5.0f, 5.0f, FX_AMBER, 255);
    }

    if (wrecked)
    {
        draw_wreck_smoke(r, &frame, car->wreck_time, seed);
        draw_wreck_debris(r, &frame, car->wreck_time, seed);
    }
}

static void draw_target_car(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase)
{
    CarFrame frame;
    frame.cx = screen_x(view, chase->target.x);
    frame.cy = screen_y(view, chase->target.y);
    frame.ax = 0.0f;
    frame.ay = -1.0f;
    frame.length = CHASE_SUV_LENGTH;
    frame.width = CHASE_SUV_WIDTH;

    bool braking = chase->phase == CHASE_PHASE_ARRIVAL;
    draw_head_beam(r, &frame, 150.0f, CHASE_SUV_WIDTH, FX_CREAM, 0.16f);
    /* Every pane of it dark: the one car on the road nobody can see into. */
    CarPaint paint = TARGET_PAINT;
    CarLook look = {paint, &SHAPE_SUV, true, true, braking, 0.0f, 0u, true};
    draw_car_body(r, &frame, &look);

    /*
     * Roof rack and a spare on the back: the SUV has to be unmistakable. All
     * the fittings come out of the SUV's own paint so a fourth grey never
     * joins the table. The rack is two rails and three bars standing proud of
     * the roof, each throwing its own sliver of shadow down and to the right.
     */
    CarFrame f = car_snapped(&frame);
    const float hl = floorf(CHASE_SUV_LENGTH * 0.5f);
    const float hw = floorf(CHASE_SUV_WIDTH * 0.5f);
    float rail_hw = hw - SHAPE_SUV.cabin - 2.0f;
    float front = floorf(SHAPE_SUV.roof * hl) - 3.0f;
    float back = floorf(SHAPE_SUV.roof_back * hl) + 4.0f;
    SDL_Color rail = fx_mix(TARGET_PAINT.body, TARGET_PAINT.body_lt, 0.6f);
    SDL_Color rail_lit = fx_mix(TARGET_PAINT.body_lt, FX_PALE, 0.25f);
    for (int flank = -1; flank <= 1; flank += 2)
    {
        float side = (float)flank;
        car_side(r, &f, side, back, front, rail_hw - 3.0f, rail_hw - 1.0f,
                 FX_INK, 150);
        car_side(r, &f, side, back, front, rail_hw - 2.0f, rail_hw,
                 FX_INK, 255);
        car_side(r, &f, side, back, front, rail_hw - 1.0f, rail_hw,
                 flank < 0 ? rail_lit : rail, 255);
    }
    for (int bar = 0; bar < 3; ++bar)
    {
        float a = floorf(back + (front - back) * ((float)bar + 0.5f) / 3.0f);
        car_px(r, &f, a - 2.0f, a, -rail_hw + 1.0f, rail_hw - 1.0f, FX_INK,
               110);
        car_px(r, &f, a, a + 2.0f, -rail_hw + 1.0f, rail_hw - 1.0f, FX_INK,
               255);
        car_px(r, &f, a + 1.0f, a + 2.0f, -rail_hw + 1.0f, rail_hw - 1.0f,
               rail, 255);
    }

    /* The spare, hung on the tailgate: a tyre seen edge-on from above, with
     * its tread blocks and the moon along its top. */
    car_mass(r, &f, -hl - 6.0f, -hl + 1.0f, -12.0f, 12.0f, 3.0f, FX_INK, 255);
    car_mass(r, &f, -hl - 5.0f, -hl, -11.0f, 11.0f, 2.0f, TARGET_PAINT.roof,
             255);
    for (float b = -9.0f; b < 10.0f; b += 3.0f)
        car_px(r, &f, -hl - 4.0f, -hl - 1.0f, b, b + 1.0f,
               fx_dim(TARGET_PAINT.roof, 0.6f), 255);
    car_px(r, &f, -hl - 2.0f, -hl - 1.0f, -5.0f, 5.0f,
           fx_mix(TARGET_PAINT.body, TARGET_PAINT.body_lt, 0.5f), 255);
    car_px(r, &f, -hl - 5.0f, -hl - 4.0f, -9.0f, 9.0f, TARGET_PAINT.body, 255);

    /* Pursuit bracket, borrowed from the cutscene's target framing. */
    float pulse = 0.5f + 0.5f * sinf(chase->time * 5.0f);
    SDL_Color mark = fx_dim(FX_CYAN, 0.55f + pulse * 0.45f);
    float half_w = CHASE_SUV_WIDTH * 0.5f + 9.0f;
    float half_h = CHASE_SUV_LENGTH * 0.5f + 9.0f;
    float arm = 12.0f;
    float left = frame.cx - half_w;
    float right = frame.cx + half_w;
    float top = frame.cy - half_h;
    float bottom = frame.cy + half_h;
    fx_rect(r, mark, left, top, arm, 2.0f);
    fx_rect(r, mark, left, top, 2.0f, arm);
    fx_rect(r, mark, right - arm, top, arm, 2.0f);
    fx_rect(r, mark, right - 2.0f, top, 2.0f, arm);
    fx_rect(r, mark, left, bottom - 2.0f, arm, 2.0f);
    fx_rect(r, mark, left, bottom - arm, 2.0f, arm);
    fx_rect(r, mark, right - arm, bottom - 2.0f, arm, 2.0f);
    fx_rect(r, mark, right - 2.0f, bottom - arm, 2.0f, arm);
}

static void draw_player_car(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase)
{
    const ChasePlayerCar *car = &chase->player;
    CarFrame frame;
    frame.cx = screen_x(view, car->x);
    frame.cy = screen_y(view, car->y);
    frame.ax = 0.0f;
    frame.ay = -1.0f;
    frame.length = CHASE_CAR_LENGTH;
    frame.width = CHASE_CAR_WIDTH;

    if (car->engine_running)
        draw_head_beam(r, &frame, 190.0f, CHASE_CAR_WIDTH * 1.1f, FX_CREAM, 0.22f);

    CarPaint paint = player_paint();
    /* Damage is legible on the car itself, not only in the HUD: worn paint
     * dulls toward the same soot the wrecks burn to. */
    int lost = CHASE_INTEGRITY - car->integrity;
    float wear = (float)lost / (float)CHASE_INTEGRITY;
    if (wear > 0.0f)
    {
        paint.body = fx_mix(paint.body, SOOT_LT, wear * 0.55f);
        paint.body_lt = fx_mix(paint.body_lt, fx_mix(SOOT_LT, FX_PALE, 0.25f),
                               wear * 0.55f);
    }
    bool braking = car->engine_running &&
                   (chase->phase == CHASE_PHASE_ARRIVAL ||
                    chase->phase == CHASE_PHASE_FAILED);
    CarLook look = {paint, &SHAPE_COUPE, car->engine_running,
                    car->engine_running, braking, 0.0f, 0u, false};
    draw_car_body(r, &frame, &look);

    /* And each knock leaves its mark where it landed: bare metal scraped
     * along a flank, and a corner stove in. Fixed places, so the car keeps
     * its scars for the rest of the attempt instead of re-rolling them. */
    CarFrame f = car_snapped(&frame);
    const float hl = floorf(CHASE_CAR_LENGTH * 0.5f);
    const float hw = floorf(CHASE_CAR_WIDTH * 0.5f);
    SDL_Color bare = fx_mix(paint.body, FX_PALE, 0.45f);
    for (int k = 0; k < lost && k < 3; ++k)
    {
        float side = (k & 1) ? 1.0f : -1.0f;
        float a = k == 2 ? -hl * 0.50f : hl * (0.35f - 0.55f * (float)k);
        car_side(r, &f, side, a - 7.0f, a + 7.0f, hw - 3.0f, hw - 2.0f, bare,
                 255);
        car_side(r, &f, side, a - 3.0f, a + 5.0f, hw - 4.0f, hw - 3.0f,
                 fx_dim(paint.roof, 0.7f), 255);
        car_side(r, &f, side, (k == 1 ? -hl + 1.0f : hl - 5.0f),
                 (k == 1 ? -hl + 5.0f : hl - 1.0f), hw - 5.0f, hw - 1.0f,
                 fx_dim(paint.roof, 0.6f), 255);
    }

    if (car->invuln_timer > 0.0f && fmodf(car->invuln_timer, 0.24f) > 0.12f)
    {
        car_mass(r, &f, -hl, hl, -hw, hw, SHAPE_COUPE.radius + 1.0f, FX_CREAM,
                 90);
    }
    if (car->scrape_timer > 0.0f)
    {
        float side = car->x < CHASE_ROAD_WIDTH * 0.5f ? -1.0f : 1.0f;
        for (int i = 0; i < 4; ++i)
        {
            fx_rect_a(r, FX_AMBER, (Uint8)(150 - i * 30),
                      frame.cx + side * CHASE_CAR_WIDTH * 0.5f,
                      frame.cy + (float)i * 7.0f, 3.0f, 5.0f);
        }
    }
}

/* ---- Chuck on foot, during the opening beat -------------------------- */

/*
 * One limb's place in a two-beat stride, -1 (fully back) to +1 (fully
 * forward). The first half of the cycle is stance: the limb tracks straight
 * back under the body at a constant rate, carrying it. The second half is
 * the swing, eased so it is quick through the middle and slow where the limb
 * takes or gives up the load. A sine is slowest exactly where the stride
 * should be fastest, which reads as skating even seen from above; the other
 * limb runs the same cycle half a turn along.
 */
static float stride_offset(float cycle)
{
    cycle -= floorf(cycle);
    if (cycle < 0.5f)
        return 1.0f - 4.0f * cycle;
    float t = (cycle - 0.5f) * 2.0f;
    float ease = t * t * (3.0f - 2.0f * t);
    return -1.0f + 2.0f * ease;
}

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
     * He runs up the pavement past his parked car, then cuts around its nose to
     * the driver's door. Two straight legs are enough to read as "he gets in"
     * at this scale, and they keep him clear of his own bodywork.
     */
    const float pavement_x = CHASE_ROAD_WIDTH + CHASE_PAVEMENT_WIDTH * 0.5f;
    float door_x = chase->player.x - CHASE_CAR_WIDTH * 0.5f - 11.0f;
    float road_x = pavement_x;
    float road_y;
    if (progress < 0.62f)
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
    float x = screen_x(view, road_x);
    float y = screen_y(view, road_y);
    bool running = progress > 0.0f && progress < 1.0f;
    /* Two limbs half a cycle apart, ~2.4 strides a second at a flat run. */
    float cycle = chase->phase_time * 2.4f;
    float near_limb = running ? stride_offset(cycle) : 0.0f;
    float far_limb = running ? stride_offset(cycle + 0.5f) : 0.0f;

    /* A pool of light under him so a 16-pixel figure still reads at night. */
    fx_glow(r, x, y, 34.0f, FX_AMBER, 44);

    /*
     * Seen from above, in his own frame — up the pavement first, then across
     * the front of the car to the door — so the figure turns the way he runs
     * instead of sliding sideways past his own bonnet. Built the way a car
     * is: the shadow the moon throws, the legs and the swinging arms under
     * the shoulders, the jacket lit on the moon's side and shaded on the
     * other, and the head on top with his hair catching the light and the
     * bridge of his face showing past it the way he is going.
     */
    CarFrame body;
    body.cx = floorf(x + 0.5f);
    body.cy = floorf(y + 0.5f);
    body.ax = 0.0f;
    body.ay = -1.0f;
    if (progress >= 0.62f)
    {
        body.ax = door_x < pavement_x ? -1.0f : 1.0f;
        body.ay = 0.0f;
    }
    body.length = 16.0f;
    body.width = 16.0f;
    const float mf = car_moon_flank(&body);
    const float me = car_moon_end(&body);

    CarFrame drop = body;
    drop.cx += 3.0f;
    drop.cy += 4.0f;
    car_mass(r, &drop, -7.0f, 7.0f, -9.0f, 9.0f, 4.0f, FX_INK, 44);
    car_mass(r, &drop, -5.0f, 6.0f, -7.0f, 7.0f, 3.0f, FX_INK, 70);

    /* Legs, opposite the arms: the stride shows as a boot out in front and
     * a boot trailing behind. */
    SDL_Color trousers = fx_mix(FX_SHADOW, FX_HERO_DK, 0.60f);
    SDL_Color boot = fx_mix(FX_INK, FX_WOOD_DK, 0.55f);
    for (int leg = -1; leg <= 1; leg += 2)
    {
        float swing = (leg < 0 ? far_limb : near_limb) * 6.0f;
        float b = (float)leg * 3.0f;
        float foot = floorf(swing + 0.5f);
        car_px(r, &body, fminf(0.0f, foot) - 1.0f, fmaxf(0.0f, foot) + 1.0f,
               b - 2.0f, b + 2.0f, trousers, 255);
        car_px(r, &body, foot - 1.0f, foot + 2.0f, b - 2.0f, b + 2.0f, boot,
               255);
        car_px(r, &body, foot + 1.0f, foot + 2.0f, b - 1.0f, b + 1.0f,
               fx_mix(boot, FX_PALE, 0.30f), 255);
    }

    /* Arms at his sides, swinging, the hand at whichever end leads. */
    for (int arm = -1; arm <= 1; arm += 2)
    {
        float swing = floorf((arm < 0 ? near_limb : far_limb) * 3.0f + 0.5f);
        float side = (float)arm;
        SDL_Color sleeve = side == mf ? FX_HERO : FX_HERO_DK;
        car_side(r, &body, side, swing - 4.0f, swing + 4.0f, 6.0f, 10.0f,
                 FX_INK, 255);
        car_side(r, &body, side, swing - 3.0f, swing + 3.0f, 7.0f, 9.0f,
                 sleeve, 255);
        float hand = swing >= 0.0f ? swing + 3.0f : swing - 5.0f;
        car_side(r, &body, side, hand, hand + 2.0f, 7.0f, 9.0f,
                 side == mf ? FX_SKIN : FX_SKIN_DK, 255);
    }

    /* The jacket across his shoulders, as three nested masses like a car's
     * shell: shade, the lit shoulder toward the moon, and the cloth between. */
    car_mass(r, &body, -5.0f, 5.0f, -8.0f, 8.0f, 3.0f, FX_INK, 255);
    car_mass(r, &body, -4.0f, 4.0f, -7.0f, 7.0f, 2.0f, FX_HERO_DK, 255);
    car_mass(r, &body, -4.0f + (me < 0.0f ? 0.0f : 1.0f),
             4.0f - (me > 0.0f ? 0.0f : 1.0f), -7.0f + (mf < 0.0f ? 0.0f : 2.0f),
             7.0f - (mf > 0.0f ? 0.0f : 2.0f), 2.0f, FX_HERO_LT, 255);
    car_mass(r, &body, -3.0f, 3.0f, -6.0f, 6.0f, 2.0f, FX_HERO, 255);

    /* The head: hair over the crown, lit toward the moon, and the brow and
     * nose showing at the front. */
    car_mass(r, &body, -3.0f, 5.0f, -5.0f, 5.0f, 2.0f, FX_INK, 255);
    car_px(r, &body, 3.0f, 5.0f, -2.0f, 2.0f, FX_SKIN, 255);
    car_px(r, &body, 4.0f, 5.0f, -1.0f, 1.0f, FX_SKIN_DK, 255);
    car_mass(r, &body, -2.0f, 3.0f, -4.0f, 4.0f, 2.0f, FX_HAIR, 255);
    car_side(r, &body, -1.0f, 0.0f, 2.0f, 4.0f, 5.0f, FX_SKIN_DK, 255);
    car_side(r, &body, 1.0f, 0.0f, 2.0f, 4.0f, 5.0f, FX_SKIN_DK, 255);
    car_side(r, &body, mf, -1.0f, 2.0f, 1.0f, 3.0f, fx_ramp(FX_HAIR).lit, 255);
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
 * They are the one surface in this scene seen from straight above, and for a
 * long time they were flat slabs with a box on some of them — the only thing in
 * the game with no light on it, filling a third of the screen for the whole
 * drive. A roof reads as a roof from three things: the parapet standing up
 * round it, lit along the lip and throwing a shadow in across the membrane; the
 * membrane itself, rolled in strips; and the hardware standing on it, every
 * piece throwing the same shadow. One light for all of it — the moon, up and to
 * the left, which is where the title screen and the outro both hang it — so
 * forty roofs scrolling past agree about where the sky is. The one other light
 * is the street's: the lip over the road catches the sodium from below.
 *
 * Everything is keyed to the block's seed, which comes off its world index and
 * never off where the block happens to be on screen: hardware that moved as a
 * roof scrolled would boil, which is the rule `art_repeat` carries for the
 * backdrops.
 */
static void roof_disk(SDL_Renderer *r, SDL_Color c, Uint8 alpha,
                      float cx, float cy, float radius)
{
    int rows = (int)ceilf(radius);
    for (int row = -rows; row <= rows; ++row)
    {
        float dy = (float)row;
        float half = floorf(sqrtf(fmaxf(0.0f, radius * radius - dy * dy)) + 0.5f);
        if (half < 1.0f)
            continue;
        if (alpha == 255)
            fx_rect(r, c, floorf(cx - half), floorf(cy + dy), half * 2.0f, 1.0f);
        else
            fx_rect_a(r, c, alpha, floorf(cx - half), floorf(cy + dy),
                      half * 2.0f, 1.0f);
    }
}

/* What something `lift` pixels tall throws down and to the right. The soft
 * pass a pixel wider is what keeps it from being a second, darker box. */
static void roof_shadow_box(SDL_Renderer *r, float x, float y,
                            float w, float h, float lift)
{
    fx_rect_a(r, FX_INK, 34, x + lift - 1.0f, y + lift - 1.0f, w + 2.0f,
              h + 2.0f);
    fx_rect_a(r, FX_INK, 80, x + lift, y + lift, w, h);
}

/* A box standing on the roof, from above: its top, a lit arris on the two
 * sides that face the moon and a dark one on the two that face away. */
static void roof_box(SDL_Renderer *r, SDL_Color top,
                     float x, float y, float w, float h)
{
    SDL_Color lit = fx_mix(top, FX_PALE, 0.28f);
    SDL_Color shade = fx_dim(top, 0.68f);
    fx_rect(r, FX_INK, x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f);
    fx_rect(r, top, x, y, w, h);
    fx_rect(r, lit, x, y, w, 1.0f);
    fx_rect(r, lit, x, y, 1.0f, h);
    fx_rect(r, shade, x, y + h - 1.0f, w, 1.0f);
    fx_rect(r, shade, x + w - 1.0f, y, 1.0f, h);
}

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
        *w = 26.0f;
        *h = 20.0f;
        return;
    case ROOF_VENTS:
        *w = 26.0f;
        *h = 14.0f;
        return;
    case ROOF_TANK:
        *w = 26.0f;
        *h = 26.0f;
        return;
    case ROOF_SKYLIGHT:
        *w = 30.0f;
        *h = 18.0f;
        return;
    case ROOF_BULKHEAD:
        *w = 32.0f;
        *h = 24.0f;
        return;
    case ROOF_DISH:
        *w = 14.0f;
        *h = 14.0f;
        return;
    case ROOF_BARE:
        break;
    }
    *w = 0.0f;
    *h = 0.0f;
}

/* One fitting at (x, y), its footprint's top-left. `shadow` draws only what it
 * throws, so every shadow on a roof can go down before any fitting does and
 * none of them lands on top of the thing beside it. */
static void draw_roof_fitting(SDL_Renderer *r, RoofFitting fitting,
                              float x, float y, unsigned seed, bool shadow)
{
    switch (fitting)
    {
    case ROOF_CONDENSER:
    {
        const float w = 26.0f;
        const float h = 20.0f;
        if (shadow)
        {
            roof_shadow_box(r, x, y, w, h, 4.0f);
            return;
        }
        roof_box(r, fx_mix(FX_STEEL_DK, FX_STEEL, 0.35f), x, y, w, h);
        float cx = x + 10.0f;
        float cy = y + h * 0.5f;
        roof_disk(r, FX_INK, 255, cx, cy, 7.5f);
        roof_disk(r, FX_SHADOW, 255, cx, cy, 6.0f);
        /* The blades as an X rather than a cross: a cross in a circle reads
         * as a crosshair, and four diagonals read as a fan. */
        for (int k = 1; k <= 4; ++k)
        {
            float d = (float)k;
            fx_rect(r, FX_STEEL, cx - d, cy - d, 1.0f, 1.0f);
            fx_rect(r, FX_STEEL, cx + d - 1.0f, cy - d, 1.0f, 1.0f);
            fx_rect(r, FX_STEEL_DK, cx - d, cy + d - 1.0f, 1.0f, 1.0f);
            fx_rect(r, FX_STEEL_DK, cx + d - 1.0f, cy + d - 1.0f, 1.0f, 1.0f);
        }
        fx_rect(r, FX_STEEL_LT, cx - 1.0f, cy - 1.0f, 2.0f, 2.0f);
        for (int louvre = 0; louvre < 3; ++louvre)
            fx_rect(r, fx_dim(FX_STEEL_DK, 0.75f), x + 20.0f,
                    y + 5.0f + (float)louvre * 4.0f, 4.0f, 1.0f);
        return;
    }
    case ROOF_VENTS:
    {
        int count = 2 + (int)(seed % 2u);
        for (int i = 0; i < count; ++i)
        {
            float vx = x + 4.0f + (float)i * 9.0f;
            float vy = y + 4.0f + (float)((seed >> (i * 3 + 4)) % 6u);
            if (shadow)
            {
                roof_disk(r, FX_INK, 80, vx + 3.0f, vy + 3.0f, 3.0f);
                continue;
            }
            roof_disk(r, FX_INK, 255, vx, vy, 3.5f);
            roof_disk(r, FX_STEEL, 255, vx, vy, 2.5f);
            fx_rect(r, fx_dim(FX_STEEL, 0.7f), vx, vy, 2.0f, 2.0f);
            fx_rect(r, FX_STEEL_LT, vx - 2.0f, vy - 2.0f, 1.0f, 1.0f);
        }
        return;
    }
    case ROOF_TANK:
    {
        /* The wooden water tank every roof in this part of town has one of,
         * seen from above: a conical cap, lit on the moon's side. */
        float cx = x + 12.0f;
        float cy = y + 12.0f;
        if (shadow)
        {
            roof_disk(r, FX_INK, 40, cx + 8.0f, cy + 8.0f, 13.5f);
            roof_disk(r, FX_INK, 76, cx + 8.0f, cy + 8.0f, 12.0f);
            return;
        }
        roof_disk(r, FX_INK, 255, cx, cy, 12.5f);
        roof_disk(r, fx_dim(FX_WOOD_DK, 0.72f), 255, cx, cy, 11.5f);
        roof_disk(r, fx_dim(FX_WOOD, 0.56f), 255, cx - 1.0f, cy - 1.0f, 9.5f);
        roof_disk(r, fx_dim(FX_WOOD_LT, 0.58f), 255, cx - 3.0f, cy - 3.0f,
                  5.0f);
        fx_rect(r, fx_dim(FX_WOOD_DK, 0.6f), cx - 1.0f, cy - 11.0f, 1.0f,
                22.0f);
        fx_rect(r, FX_STEEL, cx - 1.0f, cy - 1.0f, 2.0f, 2.0f);
        return;
    }
    case ROOF_SKYLIGHT:
    {
        const float w = 30.0f;
        const float h = 18.0f;
        bool lit = (seed & 0x100u) != 0u;
        if (shadow)
        {
            roof_shadow_box(r, x, y, w, h, 2.0f);
            return;
        }
        roof_box(r, FX_STEEL_DK, x, y, w, h);
        for (int pane = 0; pane < 3; ++pane)
        {
            float px = x + 2.0f + (float)pane * 9.0f;
            SDL_Color glass = lit ? fx_mix(FX_SHADOW, FX_WARM, 0.30f) : COL_GLASS;
            SDL_Color sheen = lit ? fx_mix(FX_SHADOW, FX_WARM, 0.48f)
                                  : fx_mix(COL_GLASS, FX_PALE, 0.16f);
            fx_rect(r, glass, px, y + 2.0f, 8.0f, h - 4.0f);
            fx_rect(r, sheen, px + 1.0f, y + 3.0f, 2.0f, h - 6.0f);
        }
        if (lit)
            fx_glow(r, x + w * 0.5f, y + h * 0.5f, 26.0f, FX_WARM, 30);
        return;
    }
    case ROOF_BULKHEAD:
    {
        /* The stair head: the tallest thing on the roof, so the longest
         * shadow, and a lamp over the door on its lee side. */
        const float w = 32.0f;
        const float h = 24.0f;
        if (shadow)
        {
            roof_shadow_box(r, x, y, w, h, 8.0f);
            return;
        }
        SDL_Color slab = fx_mix(FX_MID, FX_STEEL_DK, 0.6f);
        roof_box(r, slab, x, y, w, h);
        fx_rect(r, fx_dim(slab, 0.82f), x + 2.0f, y + h * 0.5f, w - 4.0f, 1.0f);
        fx_rect(r, FX_WARM, x + w * 0.5f - 1.0f, y + h, 2.0f, 1.0f);
        fx_glow(r, x + w * 0.5f, y + h + 3.0f, 14.0f, FX_WARM, 44);
        return;
    }
    case ROOF_DISH:
    {
        float cx = x + 6.0f;
        float cy = y + 6.0f;
        if (shadow)
        {
            roof_disk(r, FX_INK, 80, cx + 4.0f, cy + 4.0f, 5.5f);
            return;
        }
        roof_disk(r, FX_INK, 255, cx, cy, 6.0f);
        roof_disk(r, fx_dim(FX_PALE, 0.62f), 255, cx, cy, 5.0f);
        roof_disk(r, fx_dim(FX_PALE, 0.44f), 255, cx + 1.0f, cy + 1.0f, 3.5f);
        fx_rect(r, FX_STEEL_DK, cx, cy, 5.0f, 1.0f);
        fx_rect(r, FX_STEEL_LT, cx + 4.0f, cy - 1.0f, 2.0f, 2.0f);
        return;
    }
    case ROOF_BARE:
        return;
    }
}

/* `street_side` is which way the road is from this block: +1 to its right,
 * -1 to its left. The lip over it takes the street's light and the sign faces
 * it. */
static void draw_rooftop_block(SDL_Renderer *r, float x, float y,
                               float w, float h, unsigned seed,
                               int street_side)
{
    const float lip = 4.0f;
    SDL_Color roof = fx_mix(FX_BASE, FX_STEEL_DK, (float)(seed % 100u) * 0.01f);
    SDL_Color parapet = fx_mix(roof, FX_STEEL, 0.55f);

    /* The drop to the alley between neighbours, then the parapet ring. */
    fx_rect(r, FX_INK, x, y, w, h);
    fx_rect(r, parapet, x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f);

    float ix = x + 1.0f + lip;
    float iy = y + 1.0f + lip;
    float iw = w - 2.0f - lip * 2.0f;
    float ih = h - 2.0f - lip * 2.0f;

    /* The membrane, rolled across the block in strips a hair apart in tone,
     * with a lapped seam at every join. */
    SDL_Color strip_dark = fx_dim(roof, 0.90f);
    SDL_Color strip_light = fx_mix(roof, FX_STEEL, 0.10f);
    for (float sy = 0.0f; sy < ih; sy += 22.0f)
    {
        unsigned strip = fx_hash(seed + fx_salt(sy) * 131u);
        float band = fminf(22.0f, ih - sy);
        fx_rect(r, fx_mix(strip_dark, strip_light, (float)(strip % 9u) / 8.0f),
                ix, iy + sy, iw, band);
        fx_rect(r, fx_dim(roof, 0.76f), ix, iy + sy, iw, 1.0f);
    }

    /* Ballast, and the dark ring a roof drain leaves round itself. */
    for (unsigned i = 0; i < 30u; ++i)
    {
        unsigned grit = fx_hash(seed * 31u + i * 0x9e3779b9u);
        float gx = ix + (float)fx_spread(grit, iw);
        float gy = iy + (float)fx_spread(grit >> 11, ih);
        fx_rect(r, (grit & 1u) ? fx_dim(roof, 0.70f)
                               : fx_mix(roof, FX_STEEL_LT, 0.28f),
                gx, gy, 1.0f, 1.0f);
    }
    for (unsigned d = 0; d < 2u; ++d)
    {
        unsigned drain = fx_hash(seed + 0xd1u + d * 977u);
        float dx = ix + 12.0f + (float)fx_spread(drain, iw - 24.0f);
        float dy = iy + 12.0f + (float)fx_spread(drain >> 9, ih - 24.0f);
        fx_glow(r, dx, dy, 11.0f, FX_INK, 64);
        fx_rect(r, FX_INK, dx - 2.0f, dy - 2.0f, 4.0f, 4.0f);
        fx_rect(r, FX_STEEL_DK, dx - 1.0f, dy - 1.0f, 2.0f, 1.0f);
    }

    /* The parapet's own shadow falls in across the membrane off the two walls
     * between the roof and the moon; the inner faces of the other two are the
     * ones it lights. */
    fx_vgrad(r, ix, iy, iw, 7.0f, FX_INK, 118, FX_INK, 0);
    fx_hgrad(r, ix, iy, 7.0f, ih, FX_INK, 96, FX_INK, 0);
    SDL_Color face = fx_mix(parapet, FX_PALE, 0.12f);
    fx_rect(r, face, ix, iy + ih - 1.0f, iw, 1.0f);
    fx_rect(r, face, ix + iw - 1.0f, iy, 1.0f, ih);

    /* Coping along the lip, jointed along the long runs. */
    SDL_Color coping = fx_mix(parapet, FX_PALE, 0.30f);
    SDL_Color lee = fx_dim(parapet, 0.70f);
    fx_rect(r, coping, x + 1.0f, y + 1.0f, w - 2.0f, 1.0f);
    fx_rect(r, coping, x + 1.0f, y + 1.0f, 1.0f, h - 2.0f);
    fx_rect(r, lee, x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f);
    fx_rect(r, lee, x + w - 2.0f, y + 1.0f, 1.0f, h - 2.0f);
    for (float jy = 18.0f; jy < h - 4.0f; jy += 18.0f)
    {
        fx_rect(r, lee, x + 1.0f, y + jy, lip, 1.0f);
        fx_rect(r, lee, x + w - 1.0f - lip, y + jy, lip, 1.0f);
    }
    float street_lip = street_side > 0 ? x + w - 2.0f : x + 1.0f;
    fx_rect(r, fx_mix(parapet, FX_SODIUM, 0.45f), street_lip, y + 2.0f, 1.0f,
            h - 4.0f);

    /* Hardware in a grid of bays, so nothing lands on anything else, and a
     * good third of the bays left bare, so a roof reads as a roof rather than
     * as a yard. At most one tank and one stair head to a roof. */
    enum
    {
        ROOF_BAY_COLS = 2,
        ROOF_BAY_ROWS = 4
    };
    float bay_w = (iw - 16.0f) / (float)ROOF_BAY_COLS;
    float bay_h = (ih - 16.0f) / (float)ROOF_BAY_ROWS;
    for (int pass = 0; pass < 2; ++pass)
    {
        bool tank = false;
        bool bulkhead = false;
        for (int bay = 0; bay < ROOF_BAY_COLS * ROOF_BAY_ROWS; ++bay)
        {
            unsigned roll = fx_hash(seed ^ ((unsigned)bay + 1u) * 0x9e3779b9u);
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
            float fit_x = bx + (float)fx_spread(roll >> 8, bay_w - fw);
            float fit_y = by + (float)fx_spread(roll >> 16, bay_h - fh);
            draw_roof_fitting(r, fitting, floorf(fit_x), floorf(fit_y), roll,
                              pass == 0);
        }
    }

    /* A sign board standing on the street lip, lit face to the road. From
     * above it is a catwalk with a line of light along one edge, and the
     * light it throws is what says which way it faces. */
    if ((seed & 5u) == 1u)
    {
        SDL_Color lit = (seed & 8u) ? FX_AMBER_DK : FX_CYAN_DK;
        SDL_Color hot = (seed & 8u) ? FX_AMBER : FX_CYAN;
        float len = 44.0f + (float)((seed >> 5) % 30u);
        float sy = y + h - 20.0f - len;
        float board_x = street_side > 0 ? x + w - 12.0f : x + 6.0f;
        float face_x = street_side > 0 ? board_x + 5.0f : board_x;
        roof_shadow_box(r, board_x, sy, 6.0f, len, 3.0f);
        fx_rect(r, FX_INK, board_x, sy, 6.0f, len);
        fx_rect(r, FX_STEEL_DK, board_x + 1.0f, sy + 1.0f, 4.0f, len - 2.0f);
        for (float rung = sy + 4.0f; rung < sy + len - 2.0f; rung += 5.0f)
            fx_rect(r, FX_MID, board_x + 1.0f, rung, 4.0f, 1.0f);
        fx_rect(r, lit, face_x, sy, 1.0f, len);
        fx_rect(r, hot, face_x, sy + len * 0.2f, 1.0f, len * 0.6f);
        fx_glow(r, face_x + (float)street_side * 10.0f, sy + len * 0.5f,
                len * 0.75f, lit, 62);
    }
}

static void render_blocks(SDL_Renderer *r, const ChaseView *view, int win_w)
{
    const float block = 220.0f;
    float road_right = view->road_left + CHASE_ROAD_WIDTH;
    float first = floorf((view->camera_y - block) / block);
    float last = ceilf((view->camera_y + view->view_h + block) / block);

    /* Pavements run continuously between the blocks and the kerb, laid in
     * flags that scroll with the street, with the foot of each building
     * darkening the flags beside it. Drawn before the roofs so a sign's light
     * can spill out over them. */
    const float flag = 24.0f;
    float flag_first = floorf(view->camera_y / flag) - 1.0f;
    float flag_last = ceilf((view->camera_y + view->view_h) / flag) + 1.0f;
    for (int side = 0; side < 2; ++side)
    {
        float px = side == 0 ? view->road_left - CHASE_PAVEMENT_WIDTH : road_right;
        fx_rect(r, COL_PAVEMENT, px, view->view_top, CHASE_PAVEMENT_WIDTH,
                view->view_h);
        for (float index = flag_first; index <= flag_last; index += 1.0f)
        {
            float top = screen_y(view, (index + 1.0f) * flag);
            unsigned laid = fx_hash(fx_salt(index) * 2246822519u + (unsigned)side);
            if ((laid & 3u) == 0u)
                fx_rect(r, fx_mix(COL_PAVEMENT, COL_KERB, 0.10f), px + 1.0f,
                        top + 1.0f, CHASE_PAVEMENT_WIDTH - 2.0f, flag - 1.0f);
            fx_rect(r, fx_dim(COL_PAVEMENT, 0.80f), px, top,
                    CHASE_PAVEMENT_WIDTH, 1.0f);
        }
        fx_rect(r, fx_dim(COL_PAVEMENT, 0.86f),
                px + floorf(CHASE_PAVEMENT_WIDTH * 0.5f), view->view_top, 1.0f,
                view->view_h);
        if (side == 0)
            fx_hgrad(r, px, view->view_top, 8.0f, view->view_h, FX_INK, 96,
                     FX_INK, 0);
        else
            fx_hgrad(r, px + CHASE_PAVEMENT_WIDTH - 8.0f, view->view_top, 8.0f,
                     view->view_h, FX_INK, 0, FX_INK, 96);
    }

    for (float index = first; index <= last; index += 1.0f)
    {
        float world_y = index * block;
        float top = screen_y(view, world_y + block);
        float height = block - 8.0f;
        unsigned seed = fx_hash((unsigned)(int)index * 2654435761u);
        draw_rooftop_block(r, -20.0f, top,
                           view->road_left - CHASE_PAVEMENT_WIDTH + 20.0f,
                           height, seed, 1);
        draw_rooftop_block(r, road_right + CHASE_PAVEMENT_WIDTH, top,
                           (float)win_w - road_right - CHASE_PAVEMENT_WIDTH +
                               20.0f,
                           height, fx_hash(seed + 77u), -1);
    }
}

/*
 * The road surface, and why it is more than one grey.
 *
 * It has rained all night, and a flat fill says nothing about that. What does:
 * the two polished ruts each lane's wheels have worn into it, water standing in
 * the gutters and the dips with the rain still landing in it, and the lamps
 * coming back off it (`render_streetlights`). Scale comes from what is set into
 * it — covers, gratings, sealed cracks, chipped paint — at the size a man would
 * see them, because a road with nothing on it could be any width at all. Every
 * one of them is keyed to a world index along the road, so they travel with it
 * and never re-roll as it scrolls.
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
#define CHASE_LAMP_REACH 34.0f

/* The world y of the mast nearest `y` along the road. */
static float nearest_lamp_y(float y)
{
    return (floorf(y / CHASE_LAMP_SPAN + 0.5f - CHASE_LAMP_PHASE) +
            CHASE_LAMP_PHASE) *
           CHASE_LAMP_SPAN;
}

/* A rounded rectangle in screen space: the car frame's own helper, pointed
 * along x, so a puddle and a bumper are rounded by the same rule. */
static void road_blob(SDL_Renderer *r, float x0, float x1, float y0, float y1,
                      float radius, SDL_Color c, Uint8 alpha)
{
    CarFrame flat = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    car_mass(r, &flat, x0, x1, y0, y1, radius, c, alpha);
}

/* Loose aggregate: pale stones and dark pits a pixel across, scattered over a
 * patch of road `w` by `h` from its top-left corner. */
static void road_grain(SDL_Renderer *r, float x, float y, float w, float h,
                       unsigned seed, unsigned count)
{
    SDL_Color stone = fx_mix(COL_ASPHALT, COL_KERB, 0.16f);
    SDL_Color pit = fx_dim(COL_ASPHALT, 0.70f);
    for (unsigned k = 0; k < count; ++k)
    {
        unsigned g = fx_hash(seed + k * 0x9e3779b9u);
        float gx = floorf(x + (float)fx_spread(g, w));
        float gy = floorf(y + (float)fx_spread(g >> 12, h));
        fx_rect(r, (g >> 28) & 1u ? stone : pit, gx, gy,
                (g >> 29) & 1u ? 2.0f : 1.0f, 1.0f);
    }
}

/* A ring of rain landing in standing water: a dot, then a widening ring that
 * fades. `life` runs 0 to 1 over the ring's short existence. */
static void draw_ripple(SDL_Renderer *r, float x, float y, float life,
                        Uint8 strength)
{
    Uint8 alpha = (Uint8)((1.0f - life) * (float)strength);
    if (alpha < 4)
        return;
    float radius = floorf(1.0f + life * 3.5f);
    if (radius < 2.0f)
    {
        fx_rect_a(r, FX_PALE, alpha, x, y, 1.0f, 1.0f);
        return;
    }
    float diag = floorf(radius * 0.7f + 0.5f);
    fx_rect_a(r, FX_PALE, alpha, x - diag, y - radius, diag * 2.0f + 1.0f, 1.0f);
    fx_rect_a(r, FX_PALE, alpha, x - diag, y + radius, diag * 2.0f + 1.0f, 1.0f);
    fx_rect_a(r, FX_PALE, alpha, x - radius, y - diag + 1.0f, 1.0f,
              diag * 2.0f - 1.0f);
    fx_rect_a(r, FX_PALE, alpha, x + radius, y - diag + 1.0f, 1.0f,
              diag * 2.0f - 1.0f);
}

/* Standing water: darker than the asphalt round it, because it gives back
 * the black sky instead of the road, inside a margin of road darkened by the
 * wet, with a streak of sky across it and the rain still landing in it. A
 * puddle under a street lamp gives the lamp back too — `lamp` is how near
 * one is, 0 to 1, and `lamp_dx` which side it is on. */
static void draw_puddle(SDL_Renderer *r, float cx, float cy, float w, float h,
                        unsigned seed, float time, float lamp, float lamp_dx)
{
    SDL_Color water = fx_mix(COL_ASPHALT, FX_SHADOW, 0.62f);
    SDL_Color deep = fx_mix(COL_ASPHALT, FX_INK, 0.50f);
    float x0 = floorf(cx - w * 0.5f);
    /* Three overlapping lobes, their widths and offsets off the puddle's own
     * seed, so no two are the same shape and none of them is a rectangle. */
    float lobe_x[3];
    float lobe_w[3];
    float lobe_y0[3];
    float lobe_y1[3];
    lobe_x[0] = x0 + (float)((seed >> 5) % 4u);
    lobe_w[0] = w - (float)((seed >> 5) % 4u) - (float)((seed >> 7) % 3u);
    lobe_y0[0] = cy;
    lobe_y1[0] = floorf(cy + h * 0.45f);
    lobe_x[1] = x0;
    lobe_w[1] = w;
    lobe_y0[1] = floorf(cy + h * 0.25f);
    lobe_y1[1] = floorf(cy + h * 0.80f);
    lobe_x[2] = x0 + (float)((seed >> 9) % 5u);
    lobe_w[2] = floorf(w * 0.68f);
    lobe_y0[2] = floorf(cy + h * 0.62f);
    lobe_y1[2] = cy + h;
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int k = 0; k < 3; ++k)
        {
            float radius = fminf(4.0f, floorf(lobe_w[k] * 0.5f) - 1.0f);
            float grow = pass == 0 ? 2.0f : 0.0f;
            road_blob(r, lobe_x[k] - grow, lobe_x[k] + lobe_w[k] + grow,
                      lobe_y0[k] - grow, lobe_y1[k] + grow, radius + grow,
                      pass == 0 ? FX_INK : water, pass == 0 ? 30 : 255);
        }
    }
    /* Deepest where the tyres have pressed the road down, along the middle. */
    road_blob(r, x0 + 3.0f, x0 + w - 3.0f, floorf(cy + h * 0.36f),
              floorf(cy + h * 0.70f), fminf(3.0f, floorf(w * 0.5f) - 4.0f),
              deep, 255);
    fx_rect_a(r, FX_STEEL_LT, 34, x0 + 2.0f, floorf(cy + h * 0.30f),
              w - 5.0f, 1.0f);
    fx_rect_a(r, FX_STEEL_LT, 20, lobe_x[2] + 2.0f, floorf(cy + h * 0.80f),
              lobe_w[2] - 4.0f, 1.0f);
    if (lamp > 0.0f)
    {
        SDL_Color glint = fx_mix(FX_AMBER, FX_CREAM, 0.30f);
        float side = lamp_dx < 0.0f ? 0.0f : 1.0f;
        for (int bar = 0; bar < 3; ++bar)
        {
            float len = floorf(w * (0.55f - (float)bar * 0.12f));
            float drift = sinf(time * 2.3f + (float)bar * 2.1f +
                               (float)(seed % 11u)) * 1.2f;
            float bx = floorf(x0 + 2.0f + side * (w - 4.0f - len) + drift);
            fx_rect_a(r, glint, (Uint8)(lamp * (150.0f - (float)bar * 34.0f)),
                      bx, floorf(cy + 3.0f + (float)bar * 3.0f), len, 1.0f);
        }
    }
    for (unsigned k = 0; k < 2u; ++k)
    {
        float phase = time * 0.85f + (float)((seed >> (k * 6u)) % 64u) / 64.0f;
        float cycle = floorf(phase);
        unsigned ring = fx_hash(seed + fx_salt(cycle) * 0x9e3779b9u + k * 77u);
        float rx = floorf(x0 + 2.0f + (float)fx_spread(ring, w - 4.0f));
        float ry = floorf(cy + 2.0f + (float)fx_spread(ring >> 12, h - 4.0f));
        draw_ripple(r, rx, ry, phase - cycle, 70);
    }
}

/* A cast-iron cover set flush into the road: a rim lit toward the moon, a
 * chequered plate, and the wet shine along its top. */
static void draw_manhole(SDL_Renderer *r, float cx, float cy)
{
    SDL_Color iron = fx_mix(COL_ASPHALT, FX_STEEL_DK, 0.55f);
    roof_disk(r, FX_INK, 90, cx + 1.0f, cy + 1.0f, 9.0f);
    roof_disk(r, FX_INK, 255, cx, cy, 8.5f);
    roof_disk(r, fx_mix(iron, FX_STEEL_LT, 0.35f), 255, cx - 1.0f, cy - 1.0f,
              7.0f);
    roof_disk(r, iron, 255, cx, cy, 7.0f);
    SDL_Color boss = fx_dim(iron, 0.66f);
    for (int gy = -4; gy <= 4; gy += 2)
        for (int gx = -4; gx <= 4; gx += 2)
        {
            if (gx * gx + gy * gy > 20 || ((gx + gy) / 2) % 2 != 0)
                continue;
            fx_rect(r, boss, cx + (float)gx, cy + (float)gy, 1.0f, 1.0f);
        }
    fx_rect(r, FX_INK, cx - 3.0f, cy, 2.0f, 1.0f);
    fx_rect(r, FX_INK, cx + 2.0f, cy, 2.0f, 1.0f);
    fx_rect_a(r, FX_PALE, 46, cx - 4.0f, cy - 6.0f, 5.0f, 1.0f);
}

/* A kerb grating: bars across a dark slot, the bar ends lit on the moon side.
 * `w` is how far it reaches into the road from the kerb. */
static void draw_drain(SDL_Renderer *r, float x, float y, float w)
{
    fx_rect(r, FX_INK, x, y, w, 14.0f);
    fx_rect(r, fx_mix(FX_INK, FX_SHADOW, 0.5f), x + 1.0f, y + 1.0f, w - 2.0f,
            12.0f);
    SDL_Color bar = fx_mix(COL_ASPHALT, FX_STEEL_DK, 0.70f);
    for (float by = y + 2.0f; by < y + 13.0f; by += 2.0f)
    {
        fx_rect(r, bar, x + 1.0f, by, w - 2.0f, 1.0f);
        fx_rect(r, fx_mix(bar, FX_STEEL_LT, 0.30f), x + 1.0f, by, 1.0f, 1.0f);
    }
    fx_rect(r, fx_mix(bar, FX_STEEL_LT, 0.25f), x, y, w, 1.0f);
}

static void render_road(SDL_Renderer *r, const ChaseView *view,
                        const Chase *chase)
{
    const float road_left = view->road_left;
    const float road_right = view->road_left + CHASE_ROAD_WIDTH;
    const float top = view->view_top;
    const float height = view->view_h;
    fx_rect(r, COL_ASPHALT, road_left, top, CHASE_ROAD_WIDTH, height);

    /* The aggregate, a band at a time. */
    const float grain = 32.0f;
    float g_first = floorf(view->camera_y / grain) - 1.0f;
    float g_last = ceilf((view->camera_y + height) / grain) + 1.0f;
    for (float index = g_first; index <= g_last; index += 1.0f)
    {
        float band_top = screen_y(view, (index + 1.0f) * grain);
        road_grain(r, road_left, band_top, CHASE_ROAD_WIDTH, grain,
                   fx_hash(fx_salt(index) * 0x27d4eb2du + 0x9au), 14u);
    }

    /* Repair patches: some old and pale, some fresh and dark, all of them
     * cut square and sealed round the edge with a bead of tar. */
    float patch_span = 160.0f;
    float first = floorf(view->camera_y / patch_span);
    float last = ceilf((view->camera_y + height) / patch_span);
    for (float index = first; index <= last; index += 1.0f)
    {
        unsigned seed = fx_hash((unsigned)(int)index * 40503u + 17u);
        if ((seed & 3u) != 0u)
            continue;
        float x = floorf(road_left + (float)fx_spread(seed, CHASE_ROAD_WIDTH));
        float y = floorf(screen_y(view, index * patch_span));
        float w = 40.0f + (float)((seed >> 7) % 70u);
        float h = 20.0f + (float)((seed >> 3) % 26u);
        if (x + w > road_right)
            w = road_right - x;
        if (w < 4.0f)
            continue;
        bool fresh = ((seed >> 13) & 1u) != 0u;
        SDL_Color fill = fresh ? fx_mix(COL_ASPHALT, FX_INK, 0.26f)
                               : COL_ASPHALT_LT;
        SDL_Color bead = fx_dim(COL_ASPHALT, 0.60f);
        fx_rect(r, fill, x, y, w, h);
        road_grain(r, x, y, w, h, seed * 7u, (unsigned)(w * h / 160.0f));
        fx_rect(r, bead, x, y, w, 1.0f);
        fx_rect(r, bead, x, y + h - 1.0f, w, 1.0f);
        fx_rect(r, bead, x, y, 1.0f, h);
        fx_rect(r, bead, x + w - 1.0f, y, 1.0f, h);
        if (fresh)
            fx_rect_a(r, FX_STEEL_LT, 16, x + 2.0f, y + 2.0f, w - 4.0f, 1.0f);
    }

    /* Cracks, and the older ones sealed: a dry crack is a thread darker than
     * the road, a tar snake is a glossy black bead that catches the light. */
    const float crack_span = 124.0f;
    float c_first = floorf(view->camera_y / crack_span) - 1.0f;
    float c_last = ceilf((view->camera_y + height) / crack_span) + 1.0f;
    SDL_Color crack = fx_dim(COL_ASPHALT, 0.55f);
    for (float index = c_first; index <= c_last; index += 1.0f)
    {
        unsigned seed = fx_hash(fx_salt(index) * 0x165667b1u + 0x2du);
        if (seed % 3u == 0u)
            continue;
        bool tar = ((seed >> 3) & 1u) != 0u;
        float x = floorf(road_left + 14.0f +
                         (float)fx_spread(seed >> 5, CHASE_ROAD_WIDTH - 28.0f));
        float wy = index * crack_span + (float)fx_spread(seed >> 14, crack_span);
        int steps = 4 + (int)((seed >> 20) % 5u);
        for (int s = 0; s < steps; ++s)
        {
            unsigned h = fx_hash(seed + (unsigned)s * 0x9e3779b9u);
            float len = 4.0f + (float)(h % 6u);
            float y = floorf(screen_y(view, wy));
            float dx = (float)((h >> 8) % 5u) - 2.0f;
            if (tar)
            {
                fx_rect_a(r, FX_INK, 120, x, y, 2.0f, len);
                if (dx != 0.0f)
                    fx_rect_a(r, FX_INK, 120, fminf(x, x + dx), y + len - 2.0f,
                              fabsf(dx) + 2.0f, 2.0f);
                if (s & 1)
                    fx_rect_a(r, FX_PALE, 40, x, y + 1.0f, 1.0f, 1.0f);
            }
            else
            {
                fx_rect(r, crack, x, y, 1.0f, len);
                if (dx != 0.0f)
                    fx_rect(r, crack, fminf(x, x + dx), y + len - 1.0f,
                            fabsf(dx) + 1.0f, 1.0f);
            }
            x += dx;
            wy -= len;
        }
    }

    /* The ruts: two polished bands down every lane, darker for the water the
     * tyres have pressed into them, with a thread of sheen along the middle. */
    for (int lane = 0; lane < CHASE_LANE_COUNT; ++lane)
    {
        float centre = road_left + ((float)lane + 0.5f) * CHASE_LANE_WIDTH;
        for (int side = -1; side <= 1; side += 2)
        {
            float x = floorf(centre + (float)side * 22.0f - 5.0f);
            fx_rect_a(r, FX_INK, 26, x, top, 10.0f, height);
            fx_rect_a(r, FX_INK, 22, x + 2.0f, top, 6.0f, height);
            fx_rect_a(r, FX_STEEL_LT, 13, x + 4.0f, top, 1.0f, height);
        }
    }

    /* Covers down the middle of the lanes, where the wheels miss them. */
    const float cover_span = 250.0f;
    float m_first = floorf(view->camera_y / cover_span) - 1.0f;
    float m_last = ceilf((view->camera_y + height) / cover_span) + 1.0f;
    for (float index = m_first; index <= m_last; index += 1.0f)
    {
        unsigned seed = fx_hash(fx_salt(index) * 0x7feb352du + 0x6du);
        if (seed & 1u)
            continue;
        int lane = (int)((seed >> 1) % (unsigned)CHASE_LANE_COUNT);
        float wy = index * cover_span + 20.0f +
                   (float)fx_spread(seed >> 9, cover_span - 40.0f);
        if (inside_junction(chase, wy, 30.0f))
            continue;
        float cx = floorf(road_left + ((float)lane + 0.5f) * CHASE_LANE_WIDTH +
                          (float)((seed >> 4) % 9u) - 4.0f);
        draw_manhole(r, cx, floorf(screen_y(view, wy)));
    }

    /* Standing water: along the gutters, and in the ruts where the road has
     * sagged under the traffic. */
    const float puddle_span = 170.0f;
    float p_first = floorf(view->camera_y / puddle_span) - 1.0f;
    float p_last = ceilf((view->camera_y + height) / puddle_span) + 1.0f;
    for (float index = p_first; index <= p_last; index += 1.0f)
    {
        unsigned seed = fx_hash(fx_salt(index) * 0x2c1b3c6du + 0x1fu);
        if (seed % 5u >= 2u)
            continue;
        float wy = index * puddle_span + (float)fx_spread(seed >> 4, puddle_span);
        if (inside_junction(chase, wy, 40.0f))
            continue;
        unsigned where = (seed >> 12) % 4u;
        float cx;
        float w;
        float h;
        if (where < 2u)
        {
            w = 10.0f + (float)((seed >> 24) % 5u);
            h = 30.0f + (float)((seed >> 19) % 40u);
            cx = where == 0u ? road_left + 2.0f + w * 0.5f
                             : road_right - 2.0f - w * 0.5f;
        }
        else
        {
            int lane = (int)((seed >> 15) % (unsigned)CHASE_LANE_COUNT);
            w = 14.0f + (float)((seed >> 24) % 12u);
            h = 18.0f + (float)((seed >> 19) % 30u);
            cx = road_left + ((float)lane + 0.5f) * CHASE_LANE_WIDTH +
                 (((seed >> 18) & 1u) ? 22.0f : -22.0f);
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
                float hx = side == 0 ? road_left + CHASE_LAMP_REACH - 7.0f
                                     : road_right - CHASE_LAMP_REACH + 7.0f;
                float dx = hx - cx;
                float dy = lamp_wy - (wy - h * 0.5f);
                float near = 1.0f - sqrtf(dx * dx + dy * dy) / 70.0f;
                if (near > lamp)
                {
                    lamp = near;
                    lamp_dx = dx;
                }
            }
        }
        draw_puddle(r, cx, floorf(screen_y(view, wy)), w, h, seed, chase->time,
                    lamp, lamp_dx);
    }

    /* The rain on the open road: a bead of light where a drop lands and a
     * ring that opens and is gone. A handful at a time, never in step. */
    const float splash_band = 40.0f;
    float s_first = floorf(view->camera_y / splash_band) - 1.0f;
    float s_last = ceilf((view->camera_y + height) / splash_band) + 1.0f;
    for (float index = s_first; index <= s_last; index += 1.0f)
    {
        unsigned band = fx_hash(fx_salt(index) * 0x9e3779b1u + 0x3bu);
        for (unsigned k = 0; k < 3u; ++k)
        {
            float phase = chase->time * 1.3f +
                          (float)((band >> (k * 7u)) % 97u) / 97.0f;
            float cycle = floorf(phase);
            float life = (phase - cycle) * 3.0f;
            if (life >= 1.0f)
                continue;
            unsigned drop = fx_hash(band + fx_salt(cycle) * 0x85ebca6bu + k * 131u);
            float x = floorf(road_left + 8.0f +
                             (float)fx_spread(drop, CHASE_ROAD_WIDTH - 16.0f));
            float y = floorf(screen_y(view, index * splash_band +
                                                 (float)fx_spread(drop >> 12,
                                                                  splash_band)));
            draw_ripple(r, x, y, life, 54);
        }
    }

    /* Gutters: the kerb's foot, where the water runs. */
    fx_rect_a(r, FX_INK, 34, road_left, top, 6.0f, height);
    fx_rect_a(r, FX_INK, 34, road_right - 6.0f, top, 6.0f, height);
    fx_rect_a(r, FX_STEEL_LT, 14, road_left + 3.0f, top, 1.0f, height);
    fx_rect_a(r, FX_STEEL_LT, 14, road_right - 4.0f, top, 1.0f, height);
    const float drain_span = 180.0f;
    float d_first = floorf(view->camera_y / drain_span) - 1.0f;
    float d_last = ceilf((view->camera_y + height) / drain_span) + 1.0f;
    for (int side = 0; side < 2; ++side)
    {
        for (float index = d_first; index <= d_last; index += 1.0f)
        {
            unsigned seed = fx_hash(fx_salt(index) * 0x68e31da4u +
                                    (unsigned)side * 911u);
            if (seed % 3u == 0u)
                continue;
            float wy = index * drain_span + (float)fx_spread(seed >> 3, drain_span);
            if (inside_junction(chase, wy, 24.0f))
                continue;
            float y = floorf(screen_y(view, wy));
            draw_drain(r, side == 0 ? road_left : road_right - 9.0f, y, 9.0f);
        }
    }

    /* Lane paint: dashes between same-direction lanes, a double centre line.
     * Worn the way road paint wears — chipped, thinner at one edge, scuffed
     * at the ends where tyres cross it — but never so far that a lane stops
     * reading as a lane. */
    const float dash_span = 96.0f;
    float dash_first = floorf(view->camera_y / dash_span) - 1.0f;
    float dash_last = ceilf((view->camera_y + height) / dash_span);
    SDL_Color paint_thin = fx_mix(COL_PAINT, COL_ASPHALT, 0.35f);
    SDL_Color chip = fx_mix(COL_ASPHALT, COL_PAINT, 0.18f);
    for (int lane = 1; lane < CHASE_LANE_COUNT; ++lane)
    {
        float x = floorf(road_left + (float)lane * CHASE_LANE_WIDTH - 1.0f);
        if (lane == CHASE_FIRST_FORWARD_LANE)
            continue;
        for (float index = dash_first; index <= dash_last; index += 1.0f)
        {
            float world_y = index * dash_span;
            if (inside_junction(chase, world_y, 0.0f))
                continue;
            float y = floorf(screen_y(view, world_y + 52.0f));
            fx_rect(r, COL_PAINT, x, y, 3.0f, 52.0f);
            fx_rect(r, paint_thin, x + 2.0f, y, 1.0f, 52.0f);
            fx_rect_a(r, COL_ASPHALT, 110, x, y, 3.0f, 2.0f);
            fx_rect_a(r, COL_ASPHALT, 110, x, y + 50.0f, 3.0f, 2.0f);
            unsigned wear = fx_hash(fx_salt(index) * 0x9e3779b9u +
                                    (unsigned)lane * 977u);
            for (unsigned c = 0; c < 4u; ++c)
            {
                unsigned h = fx_hash(wear + c * 0x85ebca6bu);
                fx_rect(r, chip, x + (float)(h % 3u),
                        y + 3.0f + (float)fx_spread(h >> 4, 46.0f), 1.0f,
                        1.0f + (float)((h >> 12) % 2u));
            }
        }
    }
    float center = view->road_left +
                   (float)CHASE_FIRST_FORWARD_LANE * CHASE_LANE_WIDTH;
    fx_rect(r, COL_PAINT_MID, center - 4.0f, top, 3.0f, height);
    fx_rect(r, COL_PAINT_MID, center + 1.0f, top, 3.0f, height);
    fx_rect(r, fx_mix(COL_PAINT_MID, COL_ASPHALT, 0.35f), center - 2.0f, top,
            1.0f, height);
    fx_rect(r, fx_mix(COL_PAINT_MID, COL_ASPHALT, 0.35f), center + 3.0f, top,
            1.0f, height);
    const float chip_band = 18.0f;
    float k_first = floorf(view->camera_y / chip_band) - 1.0f;
    float k_last = ceilf((view->camera_y + height) / chip_band) + 1.0f;
    SDL_Color chip_mid = fx_mix(COL_ASPHALT, COL_PAINT_MID, 0.25f);
    for (float index = k_first; index <= k_last; index += 1.0f)
    {
        unsigned h = fx_hash(fx_salt(index) * 0x4f1bbcddu + 0x77u);
        if (h % 3u == 0u)
            continue;
        float x = ((h >> 2) & 1u ? center + 1.0f : center - 4.0f) +
                  (float)((h >> 3) % 3u);
        float y = floorf(screen_y(view, index * chip_band +
                                            (float)fx_spread(h >> 6, chip_band)));
        fx_rect(r, chip_mid, x, y, 1.0f, 1.0f + (float)((h >> 12) % 3u));
    }

    /* Edge lines, the ones drivers are supposed to respect. */
    SDL_Color edge = fx_dim(COL_PAINT, 0.72f);
    fx_rect(r, edge, road_left + 6.0f, top, 2.0f, height);
    fx_rect(r, edge, road_right - 8.0f, top, 2.0f, height);
    for (float index = k_first; index <= k_last; index += 1.0f)
    {
        unsigned h = fx_hash(fx_salt(index) * 0x2545f491u + 0x5u);
        if (h % 4u != 0u)
            continue;
        float x = (h >> 2) & 1u ? road_left + 6.0f : road_right - 8.0f;
        float y = floorf(screen_y(view, index * chip_band +
                                            (float)fx_spread(h >> 6, chip_band)));
        fx_rect(r, chip, x + (float)((h >> 3) & 1u), y, 1.0f,
                1.0f + (float)((h >> 12) % 3u));
    }

    /*
     * Kerbs, as stones rather than as a line: a top face, the arris toward
     * the moon catching it, the face toward the road on the near side in
     * shade and on the far side lit, and each kerb's own shadow thrown down
     * and to the right — onto the road from the near kerb, onto the pavement
     * from the far one. Jointed every stone.
     */
    SDL_Color kerb_lit = fx_mix(COL_KERB, FX_PALE, 0.34f);
    SDL_Color kerb_dark = fx_dim(COL_KERB, 0.66f);
    fx_rect(r, COL_KERB, road_left - 3.0f, top, 3.0f, height);
    fx_rect(r, kerb_lit, road_left - 3.0f, top, 1.0f, height);
    fx_rect(r, kerb_dark, road_left - 1.0f, top, 1.0f, height);
    fx_rect_a(r, FX_INK, 96, road_left, top, 2.0f, height);
    fx_rect(r, COL_KERB, road_right, top, 3.0f, height);
    fx_rect(r, kerb_lit, road_right, top, 1.0f, height);
    fx_rect(r, kerb_dark, road_right + 2.0f, top, 1.0f, height);
    fx_rect_a(r, FX_INK, 84, road_right + 3.0f, top, 2.0f, height);
    const float stone = 30.0f;
    float st_first = floorf(view->camera_y / stone) - 1.0f;
    float st_last = ceilf((view->camera_y + height) / stone) + 1.0f;
    for (float index = st_first; index <= st_last; index += 1.0f)
    {
        float y = floorf(screen_y(view, index * stone));
        fx_rect_a(r, FX_INK, 150, road_left - 3.0f, y, 3.0f, 1.0f);
        fx_rect_a(r, FX_INK, 150, road_right, y, 3.0f, 1.0f);
        unsigned h = fx_hash(fx_salt(index) * 0x61c88647u + 0x13u);
        if ((h & 3u) == 0u)
            fx_rect_a(r, (h & 4u) ? FX_PALE : FX_INK, 22,
                      (h & 8u) ? road_right : road_left - 3.0f, y + 1.0f, 3.0f,
                      stone - 1.0f);
    }
}

/*
 * A signal head on its mast, seen from above and a little to the front, which
 * is the only way a player can read which lamp is lit: a backplate, three
 * lenses under their visors, and the lit one bright enough for the frame's
 * bloom to take it. The amber sits dark between them — it is the cycle's own
 * lamp and the drive never shows it — because three lenses read as a traffic
 * light where two read as a status chip. What the lit lamp throws on the wet
 * ground under it is part of the signal too: a colour on the road is visible
 * out of the corner of the eye when the head itself is not.
 */
static void draw_traffic_signal(SDL_Renderer *r, float x, float y,
                                bool cross_green)
{
    SDL_Color on = cross_green ? FX_RED : FX_GREEN;
    float lit_y = cross_green ? y - 7.0f : y + 3.0f;
    fx_glow(r, x + 2.0f, y + 16.0f, 22.0f, on, 38);
    fx_vgrad(r, x - 2.0f, y + 12.0f, 5.0f, 18.0f, on, 52, on, 0);

    fx_rect_a(r, FX_INK, 90, x - 2.0f, y - 7.0f, 10.0f, 21.0f);
    fx_rect(r, FX_INK, x - 5.0f, y - 10.0f, 10.0f, 21.0f);
    fx_rect(r, FX_STEEL_DK, x - 4.0f, y - 9.0f, 8.0f, 19.0f);
    fx_rect(r, fx_mix(FX_STEEL_DK, FX_STEEL_LT, 0.40f), x - 4.0f, y - 9.0f,
            1.0f, 19.0f);
    fx_rect(r, fx_dim(FX_STEEL_DK, 0.70f), x + 3.0f, y - 9.0f, 1.0f, 19.0f);

    SDL_Color stop = cross_green ? FX_RED : fx_dim(FX_RED_DK, 0.80f);
    SDL_Color caution = fx_dim(FX_AMBER_DK, 0.45f);
    SDL_Color go = cross_green ? fx_dim(FX_GREEN_DK, 0.80f) : FX_GREEN;
    const SDL_Color lamps[3] = {stop, caution, go};
    for (int i = 0; i < 3; ++i)
    {
        float ly = y - 7.0f + (float)i * 5.0f;
        fx_rect(r, FX_INK, x - 3.0f, ly - 1.0f, 6.0f, 1.0f);
        fx_rect(r, lamps[i], x - 2.0f, ly, 4.0f, 4.0f);
    }
    fx_rect(r, fx_mix(on, FX_CREAM, 0.55f), x - 1.0f, lit_y + 1.0f, 2.0f, 2.0f);
    fx_glow(r, x, lit_y + 2.0f, 16.0f, on, 96);
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
 */
static void draw_cordon_car(SDL_Renderer *r, float cx, float cy, int facing,
                            float time, unsigned seed, bool steady)
{
    CarFrame frame;
    frame.cx = cx;
    frame.cy = cy;
    frame.ax = (float)facing;
    frame.ay = 0.0f;
    frame.length = CHASE_CAR_LENGTH;
    frame.width = CHASE_CAR_WIDTH;

    /* The livery is black and white, which from above means a dark body with
     * a white roof and white door tops: the white is CORDON_PAINT's own light
     * step, laid on after the body so the shell keeps its dark shading. */
    CarPaint shell = {CORDON_PAINT.body,
                      fx_mix(CORDON_PAINT.body, FX_STEEL_LT, 0.35f),
                      CORDON_PAINT.roof};
    CarLook look = {shell, &SHAPE_SALOON, false, false, false, 0.0f, seed, false};
    draw_car_body(r, &frame, &look);

    CarFrame f = car_snapped(&frame);
    const float hl = floorf(CHASE_CAR_LENGTH * 0.5f);
    const float hw = floorf(CHASE_CAR_WIDTH * 0.5f);
    const float mf = car_moon_flank(&f);
    const float roof_a = floorf(SHAPE_SALOON.roof * hl + 0.5f);
    const float roof_back_a = floorf(SHAPE_SALOON.roof_back * hl + 0.5f);
    const float roof_hw = hw - SHAPE_SALOON.cabin;
    SDL_Color white = CORDON_PAINT.body_lt;
    SDL_Color white_lit = fx_mix(white, FX_CREAM, 0.35f);
    SDL_Color white_shade = fx_dim(white, 0.68f);
    car_mass(r, &f, roof_back_a, roof_a, -roof_hw, roof_hw, 2.0f, white, 255);
    car_side(r, &f, mf, roof_back_a + 1.0f, roof_a - 1.0f, roof_hw - 1.0f,
             roof_hw, white_lit, 255);
    car_side(r, &f, -mf, roof_back_a + 1.0f, roof_a - 1.0f, roof_hw - 1.0f,
             roof_hw, white_shade, 255);
    for (int flank = -1; flank <= 1; flank += 2)
    {
        float side = (float)flank;
        car_side(r, &f, side, roof_back_a + 1.0f, roof_a + 1.0f, roof_hw + 3.0f,
                 hw - 2.0f, side == mf ? white : white_shade, 255);
        car_side(r, &f, side, floorf((roof_a + roof_back_a) * 0.5f),
                 floorf((roof_a + roof_back_a) * 0.5f) + 1.0f, roof_hw + 3.0f,
                 hw - 2.0f, fx_dim(white_shade, 0.7f), 255);
    }
    /* The push bar across the nose. */
    car_px(r, &f, hl, hl + 3.0f, -hw * 0.55f, hw * 0.55f, FX_INK, 255);
    car_px(r, &f, hl + 1.0f, hl + 2.0f, -hw * 0.55f + 1.0f, hw * 0.55f - 1.0f,
           FX_STEEL, 255);

    /* The bar itself: two halves alternating on their own beat, salted per car
     * so a street of them never flashes in unison. The bar sits across the
     * roof, so it runs along the car's width — which from above is the axis
     * the body is not pointing down. */
    float rx = -frame.ay;
    float ry = frame.ax;
    float phase = fmodf(time * CHASE_CORDON_STROBE_HZ +
                            (float)(seed % 100u) * 0.01f,
                        1.0f);
    bool blue_half = phase < 0.5f;
    float flash = blue_half ? 1.0f - phase * 2.0f : 1.0f - (phase - 0.5f) * 2.0f;
    flash = 0.35f + 0.65f * flash;
    /* Held at the ramp's own mean when the player has asked for it. The two
     * halves still alternate, because that is the bar being a bar rather than
     * a strobe — what goes is the brightness sweeping up and down 3.4 times a
     * second on top of it, for the whole minute the drive lasts. */
    if (steady)
        flash = 0.675f;

    float bar_a = floorf((roof_a + roof_back_a) * 0.5f) + 4.0f;
    float bar_hw = roof_hw + 2.0f;
    CarFrame drop = f;
    drop.cx += 1.0f;
    drop.cy += 2.0f;
    car_px(r, &drop, bar_a - 3.0f, bar_a + 3.0f, -bar_hw, bar_hw, FX_INK, 120);
    car_px(r, &f, bar_a - 3.0f, bar_a + 3.0f, -bar_hw, bar_hw, FX_INK, 255);
    for (int half = 0; half < 2; ++half)
    {
        bool lit = (half == 0) == blue_half;
        SDL_Color c = half == 0 ? FX_CORDON_BLUE : FX_RED;
        float sign = half == 0 ? -1.0f : 1.0f;
        SDL_Color lens = lit ? fx_mix(c, FX_CREAM, 0.35f) : fx_dim(c, 0.32f);
        SDL_Color core = lit ? fx_mix(c, FX_CREAM, 0.70f) : fx_dim(c, 0.44f);
        for (int cell = 0; cell < 3; ++cell)
        {
            float inner = 2.0f + (float)cell * ((bar_hw - 2.0f) / 3.0f);
            float outer =
                2.0f + (float)(cell + 1) * ((bar_hw - 2.0f) / 3.0f) - 1.0f;
            car_side(r, &f, sign, bar_a - 2.0f, bar_a + 2.0f, floorf(inner),
                     floorf(outer), lens, 255);
            car_side(r, &f, sign, bar_a - 1.0f, bar_a + 1.0f, floorf(inner) + 1.0f,
                     floorf(outer) - 1.0f, core, 255);
        }
        float offset = sign * bar_hw * 0.5f;
        float bx = f.cx + rx * offset + frame.ax * bar_a;
        float by = f.cy + ry * offset;
        if (lit)
            fx_glow(r, bx, by, 46.0f, c, (Uint8)(150.0f * flash));
    }
    /* And what the bar throws on the road it is standing on. A beacon that
     * lights nothing is a sticker on a roof. */
    fx_glow(r, cx, cy, 96.0f, blue_half ? FX_CORDON_BLUE : FX_RED,
            (Uint8)(46.0f * flash));
}

/* A chevron pointing the way the pursuit drives, laid in steps the way road
 * paint is sprayed through a stencil. */
static void draw_chevron(SDL_Renderer *r, float cx, float y, Uint8 alpha)
{
    for (int step = 0; step < 6; ++step)
    {
        float out = 3.0f + (float)step * 6.0f;
        float sy = y + (float)step * 3.0f;
        fx_rect_a(r, FX_AMBER, alpha, cx - out - 7.0f, sy, 7.0f, 6.0f);
        fx_rect_a(r, FX_AMBER, alpha, cx + out, sy, 7.0f, 6.0f);
    }
}

static void render_junctions(SDL_Renderer *r, const ChaseView *view,
                             const Chase *chase, int win_w)
{
    float road_right = view->road_left + CHASE_ROAD_WIDTH;
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        float top = floorf(screen_y(view, junction->y + CHASE_JUNCTION_HALF));
        float height = CHASE_JUNCTION_HALF * 2.0f;
        if (top > view->view_top + view->view_h || top + height < view->view_top)
            continue;
        /* One identity per junction for everything that wears on it. */
        unsigned id = fx_hash(fx_salt(junction->y * 0.5f) * 2246822519u + 0x4au);

        /* The cross street runs edge to edge, over blocks and lane paint. */
        fx_rect(r, COL_ASPHALT, -20.0f, top, (float)win_w + 40.0f, height);
        road_grain(r, -20.0f, top, (float)win_w + 40.0f, height, id, 90u);
        float middle = floorf(screen_y(view, junction->y));
        /* Its own ruts, one pair to each cross lane. */
        for (int lane = -1; lane <= 1; lane += 2)
        {
            float lc = middle + (float)lane * CHASE_CROSS_LANE_OFFSET;
            for (int side = -1; side <= 1; side += 2)
            {
                float y = floorf(lc + (float)side * 22.0f - 5.0f);
                fx_rect_a(r, FX_INK, 22, -20.0f, y, (float)win_w + 40.0f, 10.0f);
                fx_rect_a(r, FX_INK, 16, -20.0f, y + 2.0f, (float)win_w + 40.0f,
                          6.0f);
            }
        }

        /* Kerbs along both edges of the cross street, off the main road: the
         * near edge's face turned from the moon and shading the street under
         * it, the far edge's face lit and its shadow on the pavement. */
        for (int side = 0; side < 2; ++side)
        {
            float x0 = side == 0 ? -20.0f : road_right;
            float w = side == 0 ? view->road_left + 20.0f
                                : (float)win_w + 20.0f - road_right;
            fx_rect(r, COL_KERB, x0, top, w, 3.0f);
            fx_rect(r, fx_mix(COL_KERB, FX_PALE, 0.34f), x0, top, w, 1.0f);
            fx_rect(r, fx_dim(COL_KERB, 0.66f), x0, top + 2.0f, w, 1.0f);
            fx_rect_a(r, FX_INK, 90, x0, top + 3.0f, w, 2.0f);
            fx_rect(r, COL_KERB, x0, top + height - 3.0f, w, 3.0f);
            fx_rect(r, fx_mix(COL_KERB, FX_PALE, 0.34f), x0, top + height - 3.0f,
                    w, 1.0f);
            fx_rect(r, fx_dim(COL_KERB, 0.66f), x0, top + height - 1.0f, w, 1.0f);
            fx_rect_a(r, FX_INK, 80, x0, top + height, w, 2.0f);
        }

        /* Cross-street lane paint, interrupted where our road passes through. */
        for (float x = -20.0f; x < (float)win_w + 20.0f; x += 62.0f)
        {
            if (x + 34.0f > view->road_left - 6.0f &&
                x < view->road_left + CHASE_ROAD_WIDTH + 6.0f)
            {
                continue;
            }
            fx_rect(r, COL_PAINT_MID, x, middle - 1.0f, 34.0f, 3.0f);
            fx_rect_a(r, COL_ASPHALT, 110, x, middle - 1.0f, 2.0f, 3.0f);
            fx_rect_a(r, COL_ASPHALT, 110, x + 32.0f, middle - 1.0f, 2.0f, 3.0f);
        }

        /* Crosswalks mark where the pursuit is crossing traffic. Worn through
         * in the wheel paths of every lane, and chipped, the way a crossing
         * on a busy road always is. */
        SDL_Color stripe_worn = COL_ASPHALT;
        for (int side = 0; side < 2; ++side)
        {
            float stripe_y = side == 0
                                 ? screen_y(view, junction->y + CHASE_JUNCTION_HALF - 4.0f)
                                 : screen_y(view, junction->y - CHASE_JUNCTION_HALF + 20.0f);
            stripe_y = floorf(stripe_y);
            for (int stripe = 0; stripe < 9; ++stripe)
            {
                float sx = view->road_left + 10.0f + (float)stripe * 52.0f;
                fx_rect_a(r, COL_PAINT, 150, sx, stripe_y, 26.0f, 16.0f);
                for (int lane = 0; lane < CHASE_LANE_COUNT; ++lane)
                {
                    float centre = view->road_left +
                                   ((float)lane + 0.5f) * CHASE_LANE_WIDTH;
                    for (int wheel = -1; wheel <= 1; wheel += 2)
                    {
                        float rut = centre + (float)wheel * 22.0f - 4.0f;
                        float lo = fmaxf(rut, sx);
                        float hi = fminf(rut + 8.0f, sx + 26.0f);
                        if (hi > lo)
                            fx_rect_a(r, stripe_worn, 70, lo, stripe_y,
                                      hi - lo, 16.0f);
                    }
                }
                unsigned wear =
                    fx_hash(id + (unsigned)(stripe + side * 9) * 0x9e3779b9u);
                for (unsigned c = 0; c < 5u; ++c)
                {
                    unsigned h = fx_hash(wear + c * 0x85ebca6bu);
                    fx_rect_a(r, stripe_worn, 150, sx + (float)fx_spread(h, 24.0f),
                              stripe_y + (float)fx_spread(h >> 10, 15.0f),
                              1.0f + (float)((h >> 20) % 3u), 1.0f);
                }
            }
        }

        /* Stop line and hazard chevrons on the approach side: the junction has
         * to announce itself before it scrolls into view, or crossing it is a
         * coin toss. */
        float stop_line =
            floorf(screen_y(view, junction->y - CHASE_JUNCTION_HALF - 8.0f));
        fx_rect_a(r, COL_PAINT, 190, view->road_left + 6.0f, stop_line,
                  CHASE_ROAD_WIDTH - 12.0f, 5.0f);
        for (int lane = 0; lane < CHASE_LANE_COUNT; ++lane)
        {
            float centre =
                view->road_left + ((float)lane + 0.5f) * CHASE_LANE_WIDTH;
            for (int wheel = -1; wheel <= 1; wheel += 2)
                fx_rect_a(r, stripe_worn, 60, centre + (float)wheel * 22.0f - 4.0f,
                          stop_line, 8.0f, 5.0f);
        }
        for (int chevron = 0; chevron < 3; ++chevron)
        {
            float y = floorf(screen_y(view, junction->y - CHASE_JUNCTION_HALF -
                                                70.0f - (float)chevron * 46.0f));
            draw_chevron(r, view->road_left + CHASE_ROAD_WIDTH * 0.5f, y - 8.0f,
                         120);
        }

        bool cross_green = chase_cross_has_green(junction, chase->time);
        draw_traffic_signal(r, view->road_left - 8.0f, top - 12.0f, cross_green);
        draw_traffic_signal(r, road_right + 8.0f, top - 12.0f, cross_green);
        draw_traffic_signal(r, view->road_left - 8.0f, top + height + 12.0f,
                            cross_green);
        draw_traffic_signal(r, road_right + 8.0f, top + height + 12.0f,
                            cross_green);

        if (junction->cordon_side != 0)
        {
            float edge = junction->cordon_side < 0
                             ? view->road_left - CHASE_PAVEMENT_WIDTH
                             : road_right + CHASE_PAVEMENT_WIDTH;
            float cx = edge + (float)junction->cordon_side *
                                  (CHASE_CORDON_KERB_INSET +
                                   CHASE_CAR_LENGTH * 0.5f);
            draw_cordon_car(r, cx, middle, -junction->cordon_side, chase->time,
                            fx_hash((unsigned)i * 2654435761u +
                                    fx_salt(junction->y * 0.5f)),
                            view->steady_lights);
        }
    }
}

/*
 * The street lamps: a mast on the pavement, an arm out over the kerb lane and
 * a lantern at its end. Seen from above a lantern is a housing with its light
 * showing round the lip, so that is what is drawn, bright enough to bloom.
 * The ground under it gets two things: the pool the lamp lights, and — because
 * the road is wet — the lamp itself coming back off the surface as a scatter
 * of glints, stirred a little by the rain.
 *
 * Those are two layers with the traffic between them, and the function draws
 * one of them per call. The pool, the glints and the shadows are on the road,
 * so a car drives over them; the mast, the arm and the lantern are the top of
 * a pole several metres up, so every car drives *under* them. Drawn in one
 * pass before the cars, as they were, the arm reached across the kerb lane
 * under whatever was in it and every car in that lane drove over the lamp
 * heads — the one thing in this scene that tells you how high anything is,
 * saying the lamps were painted on the tarmac.
 */
typedef enum
{
    STREETLIGHT_GROUND,
    STREETLIGHT_OVERHEAD
} StreetlightLayer;

static void render_streetlights(SDL_Renderer *r, const ChaseView *view,
                                const Chase *chase, StreetlightLayer layer)
{
    const float span = CHASE_LAMP_SPAN;
    float first = floorf(view->camera_y / span - CHASE_LAMP_PHASE);
    float last = ceilf((view->camera_y + view->view_h) / span);
    float road_right = view->road_left + CHASE_ROAD_WIDTH;
    SDL_Color lens = fx_mix(FX_AMBER, FX_CREAM, 0.45f);
    SDL_Color glint = fx_mix(FX_AMBER, FX_CREAM, 0.30f);

    for (float index = first; index <= last; index += 1.0f)
    {
        float world_y = (index + CHASE_LAMP_PHASE) * span;
        if (inside_junction(chase, world_y, 20.0f))
            continue;
        float y = floorf(screen_y(view, world_y));
        for (int side = 0; side < 2; ++side)
        {
            float x = side == 0 ? view->road_left - 7.0f : road_right + 7.0f;
            float reach = side == 0 ? CHASE_LAMP_REACH : -CHASE_LAMP_REACH;
            float hx = x + reach;
            float arm_x = fminf(x, hx);
            if (layer == STREETLIGHT_OVERHEAD)
            {
                /* The mast and its arm, the arm lit along the side the moon
                 * is on. */
                fx_rect(r, FX_INK, arm_x, y - 2.0f, fabsf(reach), 3.0f);
                fx_rect(r, FX_STEEL, arm_x, y - 1.0f, fabsf(reach), 1.0f);
                fx_rect(r, fx_mix(FX_STEEL, FX_PALE, 0.25f), arm_x, y - 2.0f,
                        fabsf(reach), 1.0f);
                roof_disk(r, FX_INK, 255, x, y, 3.5f);
                roof_disk(r, FX_STEEL_DK, 255, x, y, 2.5f);
                fx_rect(r, FX_STEEL_LT, x - 2.0f, y - 2.0f, 1.0f, 1.0f);

                /* The lantern: a cobra head, its cap over the arm end and the
                 * lit bowl showing at the tip, sodium-amber and bright enough
                 * to bloom. */
                float tip = side == 0 ? 1.0f : -1.0f;
                float cap_x = side == 0 ? hx - 7.0f : hx + 1.0f;
                fx_glow(r, hx + tip * 2.0f, y, 16.0f, FX_AMBER, 110);
                fx_rect(r, FX_INK, hx - 7.0f, y - 4.0f, 14.0f, 8.0f);
                fx_rect(r, FX_AMBER, hx - 6.0f, y - 3.0f, 12.0f, 6.0f);
                fx_rect(r, lens, side == 0 ? hx + 1.0f : hx - 5.0f, y - 2.0f,
                        4.0f, 4.0f);
                fx_rect(r, FX_STEEL_DK, cap_x, y - 3.0f, 6.0f, 6.0f);
                fx_rect(r, fx_mix(FX_STEEL_DK, FX_STEEL_LT, 0.45f), cap_x,
                        y - 3.0f, 6.0f, 1.0f);
                fx_rect(r, fx_dim(FX_STEEL_DK, 0.7f), cap_x, y + 2.0f, 6.0f,
                        1.0f);
                continue;
            }

            fx_glow(r, hx, y, 96.0f, FX_AMBER, 46);
            fx_glow(r, hx, y + 4.0f, 34.0f, FX_AMBER, 30);
            /* The hot spot where the wet surface mirrors the lantern, and
             * the reflection broken into bars by the rain landing on it. */
            fx_glow(r, hx, y + 8.0f, 20.0f, glint, 58);
            unsigned lamp = fx_hash(fx_salt(index) * 0x9e3779b9u +
                                    (unsigned)side * 613u);
            for (int bar = 0; bar < 5; ++bar)
            {
                float drift = sinf(chase->time * 2.1f + (float)bar * 1.9f +
                                   (float)(lamp % 13u)) * 1.5f;
                float half = 6.0f - (float)bar;
                fx_rect_a(r, glint, (Uint8)(118 - bar * 18),
                          floorf(hx - half + drift), y + 6.0f + (float)bar * 3.0f,
                          half * 2.0f, 1.0f);
            }
            for (unsigned g = 0; g < 14u; ++g)
            {
                unsigned h = fx_hash(lamp + g * 0x85ebca6bu);
                float angle = (float)(h % 628u) * 0.01f;
                float dist = 7.0f + (float)((h >> 10) % 26u);
                float gx = floorf(hx + cosf(angle) * dist * 1.1f);
                float gy = floorf(y + 5.0f + sinf(angle) * dist * 0.8f);
                float shimmer = 0.55f + 0.45f * sinf(chase->time * 2.6f +
                                                     (float)(h % 97u));
                float near = 1.0f - dist / 34.0f;
                fx_rect_a(r, glint, (Uint8)(shimmer * near * 130.0f), gx, gy,
                          2.0f + (float)((h >> 16) % 4u), 1.0f);
            }

            /* The sliver of shadow the mast and the arm throw away from the
             * moon lies on the ground with the pool, under the traffic. */
            fx_rect_a(r, FX_INK, 70, arm_x + 2.0f, y + 2.0f, fabsf(reach), 2.0f);
            roof_disk(r, FX_INK, 90, x + 2.0f, y + 2.0f, 3.5f);
        }
    }
}

/*
 * The destination, seen from above as the drive arrives at it: a forecourt,
 * and the roof of Kessler Tower filling the top of the frame.
 *
 * It is built out of the same vocabulary as every other roof in this scene —
 * a parapet lit along the moon's side and shading the membrane inside it,
 * hardware throwing its shadow down and to the right — because a destination
 * drawn in flat panels next to forty roofs drawn as solids reads as a
 * placeholder rather than as the place the night is going. Two lights of its
 * own: the lit entrance canopy on the kerb side the SUV pulls up to, and the
 * helipad the rescue ends on, ringed with lamps. And one shadow: forty storeys
 * throw a long one across the forecourt, away from the moon.
 */
static void render_destination(SDL_Renderer *r, const ChaseView *view,
                               const Chase *chase, int win_w)
{
    if (chase->building_y <= 0.0f)
        return;
    float face = floorf(screen_y(view, chase->building_y));
    if (face > view->view_top + view->view_h)
        return;
    const float W = (float)win_w;
    float road_right = view->road_left + CHASE_ROAD_WIDTH;

    /* Forecourt paving, laid in flags, with parking bays. */
    fx_rect(r, COL_PAVEMENT, -20.0f, face, W + 40.0f, 30.0f);
    for (float fx0 = 0.0f; fx0 < W; fx0 += 24.0f)
    {
        unsigned h = fx_hash(fx_salt(fx0) * 2246822519u + 0x7du);
        if ((h & 3u) == 0u)
            fx_rect(r, fx_mix(COL_PAVEMENT, COL_KERB, 0.10f), fx0 + 1.0f,
                    face + 1.0f, 23.0f, 13.0f);
        if ((h & 12u) == 0u)
            fx_rect(r, fx_mix(COL_PAVEMENT, COL_KERB, 0.08f), fx0 + 1.0f,
                    face + 15.0f, 23.0f, 12.0f);
        fx_rect(r, fx_dim(COL_PAVEMENT, 0.82f), fx0, face, 1.0f, 28.0f);
    }
    fx_rect(r, fx_dim(COL_PAVEMENT, 0.82f), -20.0f, face + 14.0f, W + 40.0f,
            1.0f);
    for (int bay = 0; bay < 5; ++bay)
    {
        float bx = 60.0f + (float)bay * 74.0f;
        fx_rect_a(r, COL_PAINT, 90, bx, face + 4.0f, 2.0f, 22.0f);
        fx_rect_a(r, COL_PAVEMENT, 120, bx, face + 12.0f, 2.0f, 3.0f);
        /* A wheel stop at the head of the bay. */
        if (bay < 4)
        {
            fx_rect(r, FX_INK, bx + 26.0f, face + 4.0f, 22.0f, 4.0f);
            fx_rect(r, fx_mix(COL_KERB, COL_PAVEMENT, 0.35f), bx + 27.0f,
                    face + 4.0f, 20.0f, 3.0f);
            fx_rect(r, fx_mix(COL_KERB, FX_PALE, 0.25f), bx + 27.0f,
                    face + 4.0f, 20.0f, 1.0f);
        }
    }
    /* The forecourt kerb: lit on top, its face to the road in shade, and its
     * shadow on the road. */
    fx_rect(r, COL_KERB, -20.0f, face + 28.0f, W + 40.0f, 3.0f);
    fx_rect(r, fx_mix(COL_KERB, FX_PALE, 0.34f), -20.0f, face + 28.0f,
            W + 40.0f, 1.0f);
    fx_rect(r, fx_dim(COL_KERB, 0.66f), -20.0f, face + 30.0f, W + 40.0f, 1.0f);
    fx_rect_a(r, FX_INK, 90, -20.0f, face + 31.0f, W + 40.0f, 2.0f);

    /* The tower's shadow, laid over the forecourt and out onto the road:
     * offset away from the moon, so a strip of the forecourt at the left
     * still has moonlight on it. */
    fx_vgrad(r, 70.0f, face, W - 50.0f, 64.0f, FX_INK, 120, FX_INK, 0);
    fx_hgrad(r, 26.0f, face, 44.0f, 64.0f, FX_INK, 0, FX_INK, 60);

    float top = view->view_top - 20.0f;
    float depth = face - top;
    fx_rect(r, FX_NIGHT, -20.0f, top, W + 40.0f, depth);

    /* The roof: a membrane in strips, keyed to their distance from the face so
     * they travel with the building rather than with the screen. */
    const float rx0 = 26.0f;
    const float rx1 = W - 26.0f;
    const float lip = 6.0f;
    SDL_Color roof = FX_BASE;
    SDL_Color parapet = fx_mix(roof, FX_STEEL, 0.55f);
    fx_rect(r, parapet, rx0, top, rx1 - rx0, depth - 5.0f);
    float mx0 = rx0 + lip;
    float mx1 = rx1 - lip;
    float my1 = face - 5.0f - lip;
    fx_rect(r, roof, mx0, top, mx1 - mx0, my1 - top);
    for (float k = 0.0f; face - 5.0f - lip - k * 24.0f > top - 24.0f; k += 1.0f)
    {
        float sy = my1 - (k + 1.0f) * 24.0f;
        unsigned strip = fx_hash(fx_salt(k) * 131u + 0x5au);
        fx_rect(r, fx_mix(fx_dim(roof, 0.92f), fx_mix(roof, FX_STEEL, 0.10f),
                          (float)(strip % 9u) / 8.0f),
                mx0, sy, mx1 - mx0, 24.0f);
        fx_rect(r, fx_dim(roof, 0.76f), mx0, sy, mx1 - mx0, 1.0f);
    }
    for (unsigned i = 0; i < 90u; ++i)
    {
        unsigned grit = fx_hash(i * 0x9e3779b9u + 0x33u);
        float gx = mx0 + (float)fx_spread(grit, mx1 - mx0);
        float gy = my1 - (float)fx_spread(grit >> 11, 220.0f);
        if (gy < top)
            continue;
        fx_rect(r, (grit & 1u) ? fx_dim(roof, 0.70f)
                               : fx_mix(roof, FX_STEEL_LT, 0.28f),
                floorf(gx), floorf(gy), 1.0f, 1.0f);
    }
    /* The parapet's shadow falls in off the moon's side; the inner faces on
     * the other sides are the ones it lights. */
    fx_hgrad(r, mx0, top, 12.0f, my1 - top, FX_INK, 104, FX_INK, 0);
    SDL_Color face_lit = fx_mix(parapet, FX_PALE, 0.14f);
    fx_rect(r, face_lit, mx0, my1 - 1.0f, mx1 - mx0, 1.0f);
    fx_rect(r, face_lit, mx1 - 1.0f, top, 1.0f, my1 - top);
    SDL_Color coping = fx_mix(parapet, FX_PALE, 0.30f);
    SDL_Color lee = fx_dim(parapet, 0.70f);
    fx_rect(r, coping, rx0, top, 1.0f, depth - 5.0f);
    fx_rect(r, lee, rx1 - 1.0f, top, 1.0f, depth - 5.0f);
    fx_rect(r, lee, rx0, face - 6.0f, rx1 - rx0, 1.0f);
    for (float jx = rx0 + 22.0f; jx < rx1 - 4.0f; jx += 22.0f)
        fx_rect(r, lee, jx, my1, 1.0f, lip);
    /* The street lip takes the sodium from below, like every roof beside the
     * road. */
    fx_rect(r, fx_mix(parapet, FX_SODIUM, 0.45f), rx0 + 1.0f, face - 7.0f,
            rx1 - rx0 - 2.0f, 1.0f);

    /* Plant: a row of condensers, a stair head and a vent cluster, out of
     * the same fittings every roof on the drive is built from. */
    struct
    {
        RoofFitting fitting;
        float x;
        float y;
    } plant[] = {
        {ROOF_CONDENSER, 58.0f, 62.0f},  {ROOF_CONDENSER, 90.0f, 62.0f},
        {ROOF_CONDENSER, 122.0f, 62.0f}, {ROOF_VENTS, 66.0f, 104.0f},
        {ROOF_BULKHEAD, W - 170.0f, 64.0f}, {ROOF_TANK, W - 104.0f, 120.0f},
        {ROOF_SKYLIGHT, W - 122.0f, 52.0f}, {ROOF_DISH, 170.0f, 40.0f}};
    const int plant_count = (int)(sizeof(plant) / sizeof(plant[0]));
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < plant_count; ++i)
            draw_roof_fitting(r, plant[i].fitting, plant[i].x,
                              face - plant[i].y, 0x2d1u * (unsigned)(i + 1),
                              pass == 0);

    /* The helipad: a raised deck, the ring and the H painted on it, and the
     * perimeter lamps a pilot finds it by. */
    float pad_cx = W * 0.5f;
    float pad_cy = face - 64.0f;
    float pad_hw = 50.0f;
    float pad_hh = 38.0f;
    roof_shadow_box(r, pad_cx - pad_hw, pad_cy - pad_hh, pad_hw * 2.0f,
                    pad_hh * 2.0f, 3.0f);
    SDL_Color deck = fx_mix(FX_MID, FX_STEEL_DK, 0.35f);
    roof_box(r, deck, pad_cx - pad_hw, pad_cy - pad_hh, pad_hw * 2.0f,
             pad_hh * 2.0f);
    for (float gy = pad_cy - pad_hh + 8.0f; gy < pad_cy + pad_hh - 2.0f; gy += 8.0f)
        fx_rect(r, fx_dim(deck, 0.86f), pad_cx - pad_hw + 2.0f, gy,
                pad_hw * 2.0f - 4.0f, 1.0f);
    SDL_Color pad_paint = fx_dim(COL_PAINT, 0.78f);
    roof_disk(r, pad_paint, 255, pad_cx, pad_cy, 31.0f);
    roof_disk(r, deck, 255, pad_cx, pad_cy, 28.0f);
    fx_rect(r, pad_paint, pad_cx - 16.0f, pad_cy - 17.0f, 6.0f, 34.0f);
    fx_rect(r, pad_paint, pad_cx + 10.0f, pad_cy - 17.0f, 6.0f, 34.0f);
    fx_rect(r, pad_paint, pad_cx - 10.0f, pad_cy - 3.0f, 20.0f, 6.0f);
    for (unsigned i = 0; i < 14u; ++i)
    {
        unsigned h = fx_hash(i * 0x85ebca6bu + 0x11u);
        fx_rect(r, deck, pad_cx - 16.0f + (float)fx_spread(h, 32.0f),
                pad_cy - 17.0f + (float)fx_spread(h >> 10, 34.0f), 1.0f, 1.0f);
    }
    for (int i = 0; i < 12; ++i)
    {
        float t = (float)(i % 4) / 3.0f;
        float lx;
        float ly;
        if (i < 4)
        {
            lx = pad_cx - pad_hw + 3.0f + t * (pad_hw * 2.0f - 8.0f);
            ly = pad_cy - pad_hh + 2.0f;
        }
        else if (i < 8)
        {
            lx = pad_cx - pad_hw + 3.0f + t * (pad_hw * 2.0f - 8.0f);
            ly = pad_cy + pad_hh - 4.0f;
        }
        else
        {
            lx = i < 10 ? pad_cx - pad_hw + 2.0f : pad_cx + pad_hw - 4.0f;
            ly = pad_cy + (i % 2 == 0 ? -12.0f : 10.0f);
        }
        fx_glow(r, lx + 1.0f, ly + 1.0f, 8.0f, FX_AMBER, 70);
        fx_rect(r, fx_mix(FX_AMBER, FX_CREAM, 0.40f), floorf(lx), floorf(ly), 2.0f,
                2.0f);
    }

    /* The aviation mast at the corner, its red lamp on the slow beat the
     * title screen's use — held lit when the player has asked for steady
     * lights. */
    float mast_x = W - 54.0f;
    float mast_y = face - 34.0f;
    fx_rect_a(r, FX_INK, 80, mast_x + 2.0f, mast_y + 3.0f, 14.0f, 2.0f);
    roof_disk(r, FX_INK, 255, mast_x, mast_y, 3.5f);
    roof_disk(r, FX_STEEL, 255, mast_x, mast_y, 2.5f);
    float beacon = view->steady_lights
                       ? 0.8f
                       : (sinf(chase->time * 1.5f) > 0.7f ? 1.0f : 0.2f);
    fx_rect(r, fx_dim(FX_RED, 0.3f + 0.7f * beacon), mast_x - 1.0f,
            mast_y - 1.0f, 2.0f, 2.0f);
    fx_glow(r, mast_x, mast_y, 18.0f, FX_RED, (Uint8)(110.0f * beacon));

    /*
     * The entrance canopy, on the kerb side the SUV pulled up to: a glazed
     * slab standing out from the face over the forecourt, lit from under,
     * so its light shows along the front edge and pools on the paving round
     * it. The lobby behind the glass spills a warm band down the face.
     */
    float door_x = road_right - 118.0f;
    float canopy_x = door_x - 9.0f;
    float canopy_w = 118.0f;
    fx_glow(r, door_x + 50.0f, face + 14.0f, 140.0f, FX_AMBER, 96);
    fx_glow(r, canopy_x - 18.0f, face + 4.0f, 34.0f, FX_WARM, 56);
    fx_glow(r, canopy_x + canopy_w + 18.0f, face + 4.0f, 34.0f, FX_WARM, 56);
    fx_rect_a(r, FX_INK, 110, canopy_x + 4.0f, face - 6.0f + 5.0f, canopy_w,
              26.0f);
    fx_rect(r, FX_INK, canopy_x, face - 8.0f, canopy_w, 28.0f);
    fx_rect(r, FX_STEEL_DK, canopy_x + 1.0f, face - 7.0f, canopy_w - 2.0f, 26.0f);
    for (int pane = 0; pane < 5; ++pane)
    {
        float px = canopy_x + 3.0f + (float)pane * 23.0f;
        fx_rect(r, fx_mix(COL_GLASS, FX_WARM, 0.22f), px, face - 5.0f, 21.0f,
                20.0f);
        fx_rect(r, fx_mix(COL_GLASS, FX_WARM, 0.40f), px, face + 11.0f, 21.0f,
                4.0f);
        fx_rect(r, fx_mix(COL_GLASS, FX_PALE, 0.24f), px + 2.0f, face - 4.0f,
                2.0f, 10.0f);
    }
    fx_rect(r, fx_mix(FX_STEEL_DK, FX_PALE, 0.30f), canopy_x + 1.0f,
            face - 7.0f, canopy_w - 2.0f, 1.0f);
    fx_rect(r, FX_AMBER_DK, canopy_x + 1.0f, face + 18.0f, canopy_w - 2.0f,
            1.0f);
    fx_rect(r, fx_mix(FX_AMBER, FX_CREAM, 0.45f), canopy_x + 3.0f, face + 19.0f,
            canopy_w - 6.0f, 1.0f);
    /* Bollards along the front of it, each with the canopy's light on its cap
     * and its own shadow away from the moon. */
    for (float bx = canopy_x - 4.0f; bx < canopy_x + canopy_w + 8.0f; bx += 20.0f)
    {
        roof_disk(r, FX_INK, 90, bx + 2.0f, face + 25.0f, 2.5f);
        roof_disk(r, FX_INK, 255, bx, face + 23.0f, 2.5f);
        roof_disk(r, FX_STEEL, 255, bx, face + 23.0f, 1.5f);
        fx_rect(r, fx_mix(FX_STEEL_LT, FX_AMBER, 0.35f), bx - 1.0f, face + 22.0f,
                1.0f, 1.0f);
    }
}

static void render_speed_streaks(SDL_Renderer *r, const ChaseView *view,
                                 const Chase *chase)
{
    float fast = clamp01((chase->player.speed - CHASE_CRUISE_SPEED * 0.6f) /
                         (CHASE_MAX_SPEED - CHASE_CRUISE_SPEED * 0.6f));
    if (fast <= 0.02f)
        return;

    /* Tapered: bright at the leading end, fading up the tail, which is what
     * separates a streak of rain racing past from a scratch on the frame. */
    Uint8 alpha = (Uint8)(fast * 76.0f);
    for (unsigned i = 0; i < 26u; ++i)
    {
        unsigned h = fx_hash(i * 374761393u);
        float x = floorf(view->road_left + (float)fx_spread(h, CHASE_ROAD_WIDTH));
        float phase = fmodf(chase->time * chase->player.speed * 1.6f +
                                (float)((h >> 8) % 700u),
                            view->view_h + 120.0f);
        float y = view->view_top + phase - 60.0f;
        float len = 26.0f + fast * 34.0f;
        float w = (h >> 20) & 1u ? 2.0f : 1.0f;
        fx_vgrad(r, x, y, w, len, FX_PALE, 0, FX_PALE, alpha);
    }
}

/* ---- Overlays and HUD ------------------------------------------------ */

static void draw_car_pip(SDL_Renderer *r, float x, float y, bool intact)
{
    CarPaint hero = player_paint();
    /* A lost pip is the car gone grey: slate steps off the shared ramp, so
     * it recedes instead of reading as a second paint option. The pip is the
     * car in miniature — corners off, lit down the moon's flank, a pane of
     * glass either side of the roof — so the row reads as three cars rather
     * than three blue tiles. */
    SDL_Color body = intact ? hero.body_lt : fx_mix(FX_SHADOW, FX_STEEL, 0.4f);
    SDL_Color roof = intact ? hero.roof : fx_mix(FX_SHADOW, FX_STEEL, 0.2f);
    SDL_Color lit = intact ? fx_mix(hero.body_lt, FX_PALE, 0.35f)
                           : fx_mix(FX_SHADOW, FX_STEEL, 0.55f);
    SDL_Color glass = intact ? fx_mix(COL_GLASS, FX_STEEL, 0.30f) : FX_SHADOW;
    fx_rect(r, FX_INK, x + 1.0f, y, 7.0f, 13.0f);
    fx_rect(r, FX_INK, x, y + 1.0f, 9.0f, 11.0f);
    fx_rect(r, body, x + 1.0f, y + 1.0f, 7.0f, 11.0f);
    fx_rect(r, lit, x + 1.0f, y + 2.0f, 1.0f, 9.0f);
    fx_rect(r, fx_dim(body, 0.72f), x + 7.0f, y + 2.0f, 1.0f, 9.0f);
    fx_rect(r, glass, x + 2.0f, y + 3.0f, 5.0f, 2.0f);
    fx_rect(r, roof, x + 2.0f, y + 5.0f, 5.0f, 4.0f);
    fx_rect(r, glass, x + 2.0f, y + 9.0f, 5.0f, 1.0f);
    if (intact)
    {
        fx_rect(r, FX_CREAM, x + 2.0f, y + 1.0f, 1.0f, 1.0f);
        fx_rect(r, FX_CREAM, x + 6.0f, y + 1.0f, 1.0f, 1.0f);
    }
}

/* A readout's well, pressed into the strip: its top edge in the shadow of
 * the lip above it, its bottom edge catching the strip's own light. */
static void draw_hud_well(SDL_Renderer *r, float x, float y, float w, float h)
{
    fx_rect(r, FX_NIGHT, x, y, w, h);
    fx_rect(r, FX_INK, x, y, w, 1.0f);
    fx_rect(r, FX_INK, x, y, 1.0f, h);
    fx_rect(r, fx_mix(FX_NIGHT, FX_STEEL_DK, 0.55f), x + 1.0f, y + h - 1.0f,
            w - 1.0f, 1.0f);
}

/* A groove between two groups of readouts. */
static void draw_hud_groove(SDL_Renderer *r, float x)
{
    fx_rect(r, fx_mix(FX_INK, FX_NIGHT, 0.5f), x, 5.0f, 1.0f, 28.0f);
    fx_rect(r, fx_mix(FX_MID, FX_STEEL, 0.30f), x + 1.0f, 5.0f, 1.0f, 28.0f);
}

static void render_hud(SDL_Renderer *r, const Chase *chase, int win_w,
                       const PadHints *pad)
{
    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, 37.0f,
             fx_mix(FX_BASE, FX_MID, 0.30f), 255,
             fx_mix(FX_NIGHT, FX_SHADOW, 0.40f), 255);
    fx_rect(r, fx_mix(FX_MID, FX_STEEL_LT, 0.35f), 0.0f, 0.0f, (float)win_w,
            1.0f);
    fx_rect(r, FX_INK, 0.0f, 37.0f, (float)win_w, 1.0f);
    fx_rect(r, fx_dim(FX_AMBER, 0.73f), 0.0f, 38.0f, (float)win_w, 2.0f);

    fx_rect(r, FX_RED, 0.0f, 0.0f, 3.0f, 37.0f);
    fx_rect(r, fx_ramp(FX_RED).lit, 0.0f, 0.0f, 1.0f, 37.0f);
    draw_hud_groove(r, 184.0f);
    draw_hud_groove(r, 241.0f);
    draw_hud_groove(r, 502.0f);
    draw_hud_groove(r, 696.0f);
    draw_text(r, 12.0f, 4.0f, 2.0f, FX_CREAM, "PURSUIT");
    fx_rect(r, fx_dim(FX_RED, 0.73f), 12.0f, 22.0f, 112.0f, 2.0f);
    /* The readout under the title names the pedals rather than the hardware:
     * "STICK / DPAD DRIVE" told a player holding a pad everything except the
     * one thing they needed, which is which button makes the car go. */
    char pedals[40];
    draw_text(r, 12.0f, 27.0f, 1.0f, FX_LABEL,
              pad_hint(pad, pedals, sizeof(pedals), "$A GAS   $B BRAKE",
                       "UP GAS   DOWN BRAKE"));

    draw_text(r, 196.0f, 8.0f, 1.0f, FX_LABEL, "CAR");
    for (int i = 0; i < CHASE_INTEGRITY; ++i)
        draw_car_pip(r, 196.0f + (float)i * 13.0f, 19.0f,
                     i < chase->player.integrity);

    draw_text(r, 250.0f, 8.0f, 1.0f, FX_LABEL, "ROUTE");
    draw_hud_well(r, 250.0f, 20.0f, 244.0f, 11.0f);
    float route = chase_route_progress(chase) * 240.0f;
    fx_rect(r, fx_mix(FX_CYAN_DK, FX_CYAN, 0.2f), 252.0f, 22.0f, route, 7.0f);
    fx_rect(r, FX_CYAN, 252.0f, 22.0f, route, 2.0f);

    /* The gap meter is the whole game: it fills as the SUV pulls away. */
    float gap = fmaxf(chase_gap(chase), 0.0f);
    float pressure = clamp01(gap / CHASE_LOSE_GAP);
    SDL_Color gap_color = fx_mix(FX_CYAN, FX_RED, pressure);
    char gap_text[24];
    SDL_snprintf(gap_text, sizeof(gap_text), "%03dM", (int)(gap * 0.1f));
    draw_text(r, 512.0f, 8.0f, 1.0f, FX_LABEL, "GAP TO SUV");
    draw_text(r, 600.0f, 8.0f, 1.0f, gap_color, gap_text);
    draw_hud_well(r, 512.0f, 20.0f, 176.0f, 11.0f);
    fx_rect(r, fx_dim(gap_color, 0.55f), 514.0f, 22.0f, 172.0f * pressure, 7.0f);
    fx_rect(r, gap_color, 514.0f, 22.0f, 172.0f * pressure, 2.0f);

    char speed_text[24];
    SDL_snprintf(speed_text, sizeof(speed_text), "%03d",
                 (int)(chase->player.speed * 0.5f));
    draw_text(r, 706.0f, 8.0f, 1.0f, FX_LABEL, "SPEED");
    draw_text(r, 706.0f, 19.0f, 2.0f, FX_CREAM, speed_text);
    draw_text(r, 758.0f, 25.0f, 1.0f, FX_LABEL, "KMH");
}

/*
 * Cross traffic enters from off-screen, so the junction ahead gets an early
 * readout: how far it is and whether the cars crossing it have the green.
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
    SDL_Color accent = cross_green ? FX_RED : FX_GREEN;
    char text[40];
    SDL_snprintf(text, sizeof(text), "JUNCTION %03dM  %s",
                 (int)(nearest_distance * 0.1f),
                 cross_green ? "CROSS TRAFFIC" : "CLEAR");

    float width = text_width(text, 1.0f) + 26.0f;
    float x = ((float)win_w - width) * 0.5f;
    float y = view->view_top + 60.0f;
    Uint8 alpha = (Uint8)(170.0f * clamp01(2.0f - nearest_distance / 450.0f));
    fx_rect_a(r, FX_INK, alpha, x, y, width, 20.0f);
    fx_rect_a(r, accent, alpha, x, y, 3.0f, 20.0f);
    float pulse = cross_green ? 0.55f + 0.45f * sinf(chase->time * 8.0f) : 1.0f;
    draw_text(r, x + 14.0f, y + 6.0f, 1.0f, fx_dim(accent, pulse), text);
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
    if (fade <= 0.0f)
        return;

    char buf[64];
    const char *pedals =
        pad_hint(pad, buf, sizeof(buf), "$A ACCELERATE    $B BRAKE",
                 "UP ACCELERATE    DOWN BRAKE");
    const char *steer = pad != NULL ? "STICK OR DPAD STEERS"
                                    : "LEFT AND RIGHT STEER";

    float width = fmaxf(text_width(pedals, 2.0f), text_width(steer, 1.0f)) +
                  40.0f;
    float x = ((float)win_w - width) * 0.5f;
    /* Under the car, not over it: the camera keeps Chuck's bonnet a fixed
     * CHASE_CAMERA_LEAD off the bottom edge, so this band is the one strip of
     * road the player never drives through. */
    float y = (float)win_h - 88.0f;
    Uint8 alpha = (Uint8)(fade * 200.0f);
    fx_rect_a(r, FX_INK, alpha, x, y, width, 44.0f);
    fx_rect_a(r, FX_AMBER, alpha, x, y, 3.0f, 44.0f);
    float center_x = (float)win_w * 0.5f;
    draw_text_centered(r, center_x, y + 7.0f, 2.0f, fx_dim(FX_CREAM, fade),
                       pedals);
    draw_text_centered(r, center_x, y + 29.0f, 1.0f, fx_dim(FX_LABEL, fade),
                       steer);
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
    if (fade <= 0.0f)
        return;

    draw_text(r, 35.0f, 48.0f, 1.0f, fx_dim(FX_CYAN, fade),
              "00:12 // THEIR CORDON, NOT YOURS");
    fx_rect_a(r, FX_RUST, (Uint8)(fade * 255.0f), 35.0f, 63.0f,
              52.0f * fade, 2.0f);
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
        float pulse = 0.5f + 0.5f * sinf(chase->time * 6.0f);
        SDL_Color mark = fx_dim(FX_AMBER, 0.5f + pulse * 0.5f);
        draw_text_centered(r, center_x, view->view_top + 10.0f, 2.0f, mark, "^");
        draw_text_centered(r, center_x, view->view_top + 30.0f, 1.0f, mark,
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
        fx_rect_a(r, FX_INK, (Uint8)(fade * 200.0f), 0.0f,
                  view->view_top + 6.0f, (float)win_w, 30.0f);
        draw_text_centered(r, center_x, view->view_top + 14.0f, 2.0f,
                           fx_dim(FX_CREAM, fade), caption);
        float blink = 0.45f + 0.55f * sinf(chase->time * 2.0f);
        draw_skip_prompt(r, win_w, win_h, blink, pad, " TO SKIP");
    }

    if (chase->phase == CHASE_PHASE_FAILED)
    {
        const char *headline = chase->failure == CHASE_FAILURE_WRECKED
                                   ? "CAR WRECKED"
                                   : "TRAIL LOST";
        fx_rect_a(r, FX_INK, 210, 0.0f, 232.0f, (float)win_w, 76.0f);
        fx_rect(r, FX_RED, 0.0f, 232.0f, (float)win_w, 2.0f);
        draw_text_centered(r, center_x, 248.0f, 3.0f, FX_RED, headline);
        draw_text_centered(r, center_x, 286.0f, 1.0f, FX_STEEL_LT,
                           "CUTTING THROUGH THE BLOCKS TO GET BACK ON THEM");
    }

    /* After enough failed attempts the drive stops insisting on itself. */
    if (chase->phase == CHASE_PHASE_PURSUIT &&
        chase->attempts >= CHASE_SKIP_AFTER_ATTEMPTS)
    {
        float blink = 0.45f + 0.55f * sinf(chase->time * 2.0f);
        draw_skip_prompt(r, win_w, win_h, blink, pad, " TO SKIP THE DRIVE");
    }

    if (chase->phase == CHASE_PHASE_ARRIVAL && chase->phase_time > 1.4f)
    {
        float fade = clamp01((chase->phase_time - 1.4f) / 0.5f);
        fx_rect_a(r, FX_INK, (Uint8)(fade * 205.0f), 0.0f, 340.0f,
                  (float)win_w, 74.0f);
        fx_rect(r, fx_dim(FX_CYAN, fade), 0.0f, 340.0f, (float)win_w, 2.0f);
        draw_text_centered(r, center_x, 356.0f, 2.0f, fx_dim(FX_CYAN, fade),
                           "THEY STOPPED HERE");
        draw_text_centered(r, center_x, 392.0f, 1.0f,
                           fx_dim(FX_STEEL_LT, fade),
                           "THEY WALKED HER INTO KESSLER TOWER");
    }
}

void chase_render(SDL_Renderer *r, const Chase *chase, int win_w, int win_h,
                  float shake_x, float shake_y, bool steady_lights,
                  const PadHints *pad)
{
    ChaseView view;
    view.road_left = ((float)win_w - CHASE_ROAD_WIDTH) * 0.5f;
    view.view_top = (float)HUD_HEIGHT;
    view.view_h = (float)win_h - (float)HUD_HEIGHT;
    view.camera_y = chase->camera_y;
    view.shake_x = shake_x;
    view.shake_y = shake_y;
    view.steady_lights = steady_lights;

    fx_rect(r, FX_NIGHT, 0.0f, 0.0f, (float)win_w, (float)win_h);
    render_blocks(r, &view, win_w);
    render_road(r, &view, chase);
    render_junctions(r, &view, chase, win_w);
    render_streetlights(r, &view, chase, STREETLIGHT_GROUND);

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
    render_streetlights(r, &view, chase, STREETLIGHT_OVERHEAD);
    /* Drawn after the traffic so the building hides whatever has driven past
     * its front and turned off the road — and after the lamps, because the
     * roof of a forty-storey tower is above everything else in the street. */
    render_destination(r, &view, chase, win_w);
    render_speed_streaks(r, &view, chase);

    render_overlays(r, &view, chase, win_w, win_h, pad);
    render_hud(r, chase, win_w, pad);

    /* Finishing (vignette, scanlines) belongs to game_render's one shared
       pass — the pause overlay has to sit under it, and it is drawn by the
       shell after this function returns. */
}
