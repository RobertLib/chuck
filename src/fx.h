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
