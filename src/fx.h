#ifndef CHUCK_FX_H
#define CHUCK_FX_H

/*
 * Shared art direction for every screen in Chuck.
 *
 * The game draws all of its art at runtime, so the look is defined by two
 * things: one disciplined palette, and a small set of lighting primitives.
 * Every renderer (world, HUD, intro, cutscenes) pulls from this header so
 * the whole game reads as a single hand-tuned production instead of three
 * screens that each invented their own colors.
 *
 * Palette rules:
 *  - Darks are blue-slate, never pure black, and always share one hue.
 *  - Cyan is the "security/technology" accent, amber is "light and warning",
 *    red is "danger and the enemy", green is "granted access".
 *  - Materials (steel, concrete, wood, skin) each get a fixed 3-step ramp.
 */

#include <SDL3/SDL.h>
#include <math.h>

/* ---- Core darks (blue-slate ramp) ---------------------------------- */
static const SDL_Color FX_INK = {5, 7, 12, 255};        /* outlines, void  */
static const SDL_Color FX_NIGHT = {10, 14, 23, 255};    /* darkest fill    */
static const SDL_Color FX_SHADOW = {17, 23, 35, 255};   /* deep interior   */
static const SDL_Color FX_BASE = {27, 35, 49, 255};     /* room walls      */
static const SDL_Color FX_MID = {41, 52, 68, 255};      /* mid structure   */

/* ---- Steel / concrete ramp ----------------------------------------- */
static const SDL_Color FX_STEEL_DK = {49, 60, 74, 255};
static const SDL_Color FX_STEEL = {70, 84, 99, 255};
static const SDL_Color FX_STEEL_LT = {104, 121, 137, 255};
static const SDL_Color FX_PALE = {156, 173, 186, 255};
static const SDL_Color FX_CREAM = {236, 238, 224, 255};

/* ---- Accents -------------------------------------------------------- */
static const SDL_Color FX_CYAN = {74, 222, 212, 255};
static const SDL_Color FX_CYAN_DK = {26, 112, 112, 255};
static const SDL_Color FX_AMBER = {248, 188, 74, 255};
static const SDL_Color FX_AMBER_DK = {168, 112, 40, 255};
static const SDL_Color FX_RED = {232, 74, 62, 255};
static const SDL_Color FX_RED_DK = {130, 36, 34, 255};
static const SDL_Color FX_GREEN = {96, 230, 140, 255};
static const SDL_Color FX_GREEN_DK = {32, 120, 76, 255};

/* ---- Character materials ------------------------------------------- */
static const SDL_Color FX_SKIN = {216, 160, 110, 255};
static const SDL_Color FX_SKIN_DK = {178, 124, 86, 255};
static const SDL_Color FX_HAIR = {82, 44, 30, 255};
static const SDL_Color FX_HERO = {40, 108, 148, 255};   /* Chuck's jacket  */
static const SDL_Color FX_HERO_LT = {70, 156, 180, 255};
static const SDL_Color FX_HERO_DK = {24, 66, 96, 255};
static const SDL_Color FX_GUARD = {84, 94, 66, 255};    /* enemy olive     */
static const SDL_Color FX_GUARD_LT = {122, 132, 88, 255};
static const SDL_Color FX_GUARD_DK = {52, 60, 42, 255};
static const SDL_Color FX_WOOD = {124, 82, 46, 255};
static const SDL_Color FX_WOOD_LT = {168, 116, 62, 255};
static const SDL_Color FX_WOOD_DK = {82, 55, 34, 255};

/* ---- Tiny drawing helpers ------------------------------------------ */

static inline void fx_set(SDL_Renderer *r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static inline void fx_fill(SDL_Renderer *r, float x, float y, float w, float h)
{
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static inline void fx_rect(SDL_Renderer *r, SDL_Color c,
                           float x, float y, float w, float h)
{
    fx_set(r, c);
    fx_fill(r, x, y, w, h);
}

static inline void fx_rect_a(SDL_Renderer *r, SDL_Color c, Uint8 alpha,
                             float x, float y, float w, float h)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, alpha);
    fx_fill(r, x, y, w, h);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static inline SDL_Color fx_dim(SDL_Color c, float factor)
{
    if (factor < 0.0f)
        factor = 0.0f;
    if (factor > 1.0f)
        factor = 1.0f;
    return (SDL_Color){(Uint8)((float)c.r * factor),
                       (Uint8)((float)c.g * factor),
                       (Uint8)((float)c.b * factor), c.a};
}

static inline SDL_Color fx_mix(SDL_Color a, SDL_Color b, float t)
{
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    return (SDL_Color){(Uint8)((float)a.r + ((float)b.r - (float)a.r) * t),
                       (Uint8)((float)a.g + ((float)b.g - (float)a.g) * t),
                       (Uint8)((float)a.b + ((float)b.b - (float)a.b) * t),
                       255};
}

static inline SDL_FColor fx_fcolor(SDL_Color c, float alpha)
{
    return (SDL_FColor){(float)c.r / 255.0f, (float)c.g / 255.0f,
                        (float)c.b / 255.0f, alpha};
}

static inline unsigned fx_hash(unsigned value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

/* ---- Character form and contact ------------------------------------- */

/*
 * A figure is a lit solid too.
 *
 * A wall is drawn material, then form shading, then edges, and that middle
 * pass is what makes it read as mass rather than as a swatch. A character
 * drawn as flat fills inside a dark outline skips it, and no amount of
 * silhouette work stops the result reading as a sticker laid over a lit room.
 * One three-step ramp derived from a single garment colour gives every body
 * part the same treatment, so the whole cast gains it at once instead of
 * thirty hand-authored literals per figure.
 */
typedef struct
{
    SDL_Color dark; /* the underside, turned away from the ceiling */
    SDL_Color base; /* the garment as authored */
    SDL_Color lit;  /* the crown and the leading edge */
} FxRamp;

static inline FxRamp fx_ramp(SDL_Color base)
{
    /* Interior light comes from the ceiling and is slightly warm, so the lit
     * step gains a little of that warmth and the shaded step cools as it
     * drops rather than only losing value. A ramp built by scaling one colour
     * up and down instead reads as plastic. */
    SDL_Color warm = {255, 236, 206, 255};
    SDL_Color cool = {13, 19, 33, 255};
    FxRamp ramp = {fx_mix(base, cool, 0.30f), base, fx_mix(base, warm, 0.24f)};
    ramp.dark.a = base.a;
    ramp.lit.a = base.a;
    return ramp;
}

/*
 * One body block, lit from above.
 *
 * Three passes and no more: the crown the ceiling reaches, the underside it
 * does not, and a rim down the leading flank. `dir` is the figure's facing, so
 * that rim always lands on the side turned toward the way he is going, and at
 * twenty-six pixels across it is a large part of what tells the player which
 * way someone is pointed.
 *
 * The trailing flank is deliberately left alone. It sits directly against the
 * figure's outline, where another dark column reads as a thicker outline rather
 * than as a surface turning away — and a block with something happening on all
 * four sides at this scale stops being a form and becomes a frame.
 */
static inline void fx_form_block(SDL_Renderer *r, float x, float y,
                                 float w, float h, FxRamp ramp, int dir)
{
    fx_rect(r, ramp.base, x, y, w, h);
    if (h < 3.0f)
    {
        if (h >= 2.0f)
            fx_rect(r, ramp.lit, x, y, w, 1.0f);
        return;
    }

    float crown = h >= 10.0f ? 2.0f : 1.0f;

    fx_rect(r, ramp.lit, x, y, w, crown);
    fx_rect(r, ramp.dark, x, y + h - 1.0f, w, 1.0f);
    /* Narrow blocks — a forearm, a trouser leg — are already almost all edge.
     * A rim on those would leave one column of the garment itself. */
    if (w >= 6.0f)
        fx_rect(r, fx_mix(ramp.base, ramp.lit, 0.50f),
                dir >= 0 ? x + w - 1.0f : x, y + crown, 1.0f,
                h - crown - 1.0f);
}

/*
 * How far one row of a mass is set in from its own edges.
 *
 * A body drawn out of rectangles reads as built rather than grown, however well
 * each rectangle is shaded — the corners are the tell, and there are four of
 * them on every part. This is a chamfer rather than an arc: at twenty-six
 * pixels across, two pixels off a corner is the whole difference between a
 * skull and a box, and anything more elaborate only muddies the silhouette.
 *
 * Top and bottom are given separately because a body is not symmetrical about
 * its waist. Shoulders slope where a hem runs straight, a skull is domed where
 * a jaw comes to a chin, and an ankle is narrower than the sole under it.
 */
static inline float fx_taper(int row, int rows, int top, int bottom)
{
    if (row < top)
        return (float)(top - row);
    if (row >= rows - bottom)
        return (float)(bottom - (rows - 1 - row));
    return 0.0f;
}

/*
 * A mass in one flat colour, corners taken off.
 *
 * For the things that sit on a form and would put its corners straight back if
 * they were rectangles: hair, a helmet, a cap, the shade along a jaw.
 */
static inline void fx_mass(SDL_Renderer *r, SDL_Color c, float x, float y,
                           float w, float h, int top, int bottom)
{
    int rows = (int)h;

    fx_set(r, c);
    for (int i = 0; i < rows; ++i)
    {
        float inset = fx_taper(i, rows, top, bottom);
        if (inset * 2.0f >= w)
            continue;
        fx_fill(r, x + inset, y + (float)i, w - inset * 2.0f, 1.0f);
    }
}

/*
 * fx_form_block's shaped counterpart: the same crown, underside and leading rim
 * laid over a tapered mass instead of a rectangle.
 *
 * The rim follows the edge in and out rather than running straight down the
 * side, which is what makes the chamfer read as a surface turning away rather
 * than as a step cut out of a box.
 */
static inline void fx_form_mass(SDL_Renderer *r, float x, float y,
                                float w, float h, FxRamp ramp, int dir,
                                int top, int bottom)
{
    int rows = (int)h;
    float crown = h >= 10.0f ? 2.0f : 1.0f;
    SDL_Color rim = fx_mix(ramp.base, ramp.lit, 0.50f);

    for (int i = 0; i < rows; ++i)
    {
        float inset = fx_taper(i, rows, top, bottom);
        float rw = w - inset * 2.0f;
        float rx = x + inset;
        if (rw < 1.0f)
            continue;

        if ((float)i < crown)
            fx_rect(r, ramp.lit, rx, y + (float)i, rw, 1.0f);
        else if (i == rows - 1)
            fx_rect(r, ramp.dark, rx, y + (float)i, rw, 1.0f);
        else
        {
            fx_rect(r, ramp.base, rx, y + (float)i, rw, 1.0f);
            if (rw >= 6.0f)
                fx_rect(r, rim, dir >= 0 ? rx + rw - 1.0f : rx,
                        y + (float)i, 1.0f, 1.0f);
        }
    }
}

/*
 * The shadow a figure casts on the surface it stands on.
 *
 * A single opaque slab under the boots is the cheapest way to ground a sprite
 * and the easiest to spot: it has a hard edge the rest of the frame does not,
 * and it stays the same size whether the figure is standing on the floor or
 * eight pixels above it at the top of a jump. `lift` is how far off that
 * surface he is, 0 to 1, and it both shrinks the pool and lets the floor back
 * through — which is most of what sells the height of a jump.
 */
static inline void fx_contact_shadow(SDL_Renderer *r, float cx, float y,
                                     float half_w, float lift, Uint8 alpha)
{
    if (lift < 0.0f)
        lift = 0.0f;
    if (lift > 1.0f)
        lift = 1.0f;

    float hw = half_w * (1.0f - 0.40f * lift);
    float fade = 1.0f - 0.55f * lift;
    float core = (float)alpha * fade;

    if (hw < 1.0f || core < 4.0f)
        return;

    /* Three overlapping passes: a wide faint skirt, the body of the pool, and
     * a small dark core directly under the weight. Alpha accumulates where
     * they overlap, which is what gives the edge its falloff. */
    fx_rect_a(r, FX_INK, (Uint8)(core * 0.34f),
              floorf(cx - hw), floorf(y), floorf(hw * 2.0f), 2.0f);
    fx_rect_a(r, FX_INK, (Uint8)(core * 0.52f),
              floorf(cx - hw * 0.70f), floorf(y), floorf(hw * 1.40f), 3.0f);
    fx_rect_a(r, FX_INK, (Uint8)(core * 0.62f),
              floorf(cx - hw * 0.36f), floorf(y + 1.0f), floorf(hw * 0.72f),
              2.0f);
}

/*
 * A blink, derived rather than stored.
 *
 * Eyes that never close are the tell that a figure is a drawing, and a blink
 * is two frames of work. `salt` scatters the interval per figure so a room
 * full of people never blinks in unison.
 */
static inline bool fx_blinking(float anim_time, unsigned salt)
{
    float period = 2.6f + (float)(fx_hash(salt) % 240u) * 0.01f;
    float phase = anim_time - period * floorf(anim_time / period);
    return phase < 0.11f;
}

/* ---- Lighting primitives ------------------------------------------- */

/* Smooth vertical gradient; alpha interpolates too, so it doubles as a
 * depth-haze / contact-shadow primitive when the ends are transparent. */
static inline void fx_vgrad(SDL_Renderer *r, float x, float y,
                            float w, float h,
                            SDL_Color top, Uint8 a_top,
                            SDL_Color bottom, Uint8 a_bottom)
{
    SDL_FColor tc = fx_fcolor(top, (float)a_top / 255.0f);
    SDL_FColor bc = fx_fcolor(bottom, (float)a_bottom / 255.0f);
    SDL_Vertex v[4] = {
        {{x, y}, tc, {0.0f, 0.0f}},
        {{x + w, y}, tc, {0.0f, 0.0f}},
        {{x + w, y + h}, bc, {0.0f, 0.0f}},
        {{x, y + h}, bc, {0.0f, 0.0f}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* The horizontal companion to fx_vgrad. Wall faces are lit from the side, so
 * the ambient occlusion beside a wall and the sheen on its exposed flank both
 * need a gradient that runs across rather than down. */
static inline void fx_hgrad(SDL_Renderer *r, float x, float y,
                            float w, float h,
                            SDL_Color left, Uint8 a_left,
                            SDL_Color right, Uint8 a_right)
{
    SDL_FColor lc = fx_fcolor(left, (float)a_left / 255.0f);
    SDL_FColor rc = fx_fcolor(right, (float)a_right / 255.0f);
    SDL_Vertex v[4] = {
        {{x, y}, lc, {0.0f, 0.0f}},
        {{x + w, y}, rc, {0.0f, 0.0f}},
        {{x + w, y + h}, rc, {0.0f, 0.0f}},
        {{x, y + h}, lc, {0.0f, 0.0f}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* Soft radial pool of light (or glow around an emissive object). */
static inline void fx_glow(SDL_Renderer *r, float cx, float cy,
                           float radius, SDL_Color c, Uint8 alpha)
{
    enum
    {
        FX_GLOW_SEGMENTS = 18
    };
    SDL_Vertex v[FX_GLOW_SEGMENTS + 2];
    int idx[FX_GLOW_SEGMENTS * 3];

    v[0].position = (SDL_FPoint){cx, cy};
    v[0].color = fx_fcolor(c, (float)alpha / 255.0f);
    v[0].tex_coord = (SDL_FPoint){0.0f, 0.0f};
    for (int i = 0; i <= FX_GLOW_SEGMENTS; ++i)
    {
        float angle = (float)i / (float)FX_GLOW_SEGMENTS * 6.2831853f;
        v[i + 1].position = (SDL_FPoint){cx + cosf(angle) * radius,
                                         cy + sinf(angle) * radius};
        v[i + 1].color = fx_fcolor(c, 0.0f);
        v[i + 1].tex_coord = (SDL_FPoint){0.0f, 0.0f};
    }
    for (int i = 0; i < FX_GLOW_SEGMENTS; ++i)
    {
        idx[i * 3 + 0] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = i + 2;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, FX_GLOW_SEGMENTS + 2, idx,
                       FX_GLOW_SEGMENTS * 3);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* Downward light cone from a fixture; widens as it falls and fades out. */
static inline void fx_light_cone(SDL_Renderer *r, float apex_x, float apex_y,
                                 float half_top, float half_bottom,
                                 float height, SDL_Color c, Uint8 alpha)
{
    SDL_FColor tc = fx_fcolor(c, (float)alpha / 255.0f);
    SDL_FColor bc = fx_fcolor(c, 0.0f);
    SDL_Vertex v[4] = {
        {{apex_x - half_top, apex_y}, tc, {0.0f, 0.0f}},
        {{apex_x + half_top, apex_y}, tc, {0.0f, 0.0f}},
        {{apex_x + half_bottom, apex_y + height}, bc, {0.0f, 0.0f}},
        {{apex_x - half_bottom, apex_y + height}, bc, {0.0f, 0.0f}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* ---- Full-frame finishing pass ------------------------------------- */

/* Subtle CRT scanlines; one dark row every third pixel keeps the pixel-art
 * grid alive without dimming the scene. */
static inline void fx_scanlines(SDL_Renderer *r, int w, int h, Uint8 alpha)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 4, 6, 10, alpha);
    for (int y = 0; y < h; y += 3)
        fx_fill(r, 0.0f, (float)y, (float)w, 1.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* Soft darkening toward the frame edges pulls focus to the action. */
static inline void fx_vignette(SDL_Renderer *r, int w, int h, Uint8 alpha)
{
    float fw = (float)w;
    float fh = (float)h;
    float edge_v = fh * 0.22f;
    float edge_h = fw * 0.16f;
    SDL_Color c = FX_INK;

    fx_vgrad(r, 0.0f, 0.0f, fw, edge_v, c, alpha, c, 0);
    fx_vgrad(r, 0.0f, fh - edge_v, fw, edge_v, c, 0, c, alpha);

    SDL_FColor solid = fx_fcolor(c, (float)alpha / 255.0f);
    SDL_FColor clear = fx_fcolor(c, 0.0f);
    SDL_Vertex left[4] = {
        {{0.0f, 0.0f}, solid, {0.0f, 0.0f}},
        {{edge_h, 0.0f}, clear, {0.0f, 0.0f}},
        {{edge_h, fh}, clear, {0.0f, 0.0f}},
        {{0.0f, fh}, solid, {0.0f, 0.0f}}};
    SDL_Vertex right[4] = {
        {{fw - edge_h, 0.0f}, clear, {0.0f, 0.0f}},
        {{fw, 0.0f}, solid, {0.0f, 0.0f}},
        {{fw, fh}, solid, {0.0f, 0.0f}},
        {{fw - edge_h, fh}, clear, {0.0f, 0.0f}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, left, 4, idx, 6);
    SDL_RenderGeometry(r, NULL, right, 4, idx, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* Animated film grain for the cinematic screens. */
static inline void fx_grain(SDL_Renderer *r, int w, int h, float time,
                            Uint8 alpha)
{
    unsigned frame = (unsigned)(time * 24.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (unsigned i = 0; i < 90u; ++i)
    {
        unsigned hsh = fx_hash(i * 2654435761u + frame * 40503u);
        float x = (float)(hsh % (unsigned)w);
        float y = (float)((hsh >> 11) % (unsigned)h);
        Uint8 a = (Uint8)(alpha / 2u + (hsh >> 22) % alpha);
        if ((hsh & 1u) != 0u)
            SDL_SetRenderDrawColor(r, 210, 220, 226, a);
        else
            SDL_SetRenderDrawColor(r, 5, 8, 12, a);
        fx_fill(r, x, y, ((hsh >> 5) & 1u) ? 2.0f : 1.0f, 1.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

#endif /* CHUCK_FX_H */
