/*
 * Every film in the game, rendered from the same hard-edged primitives as the
 * sector: the kerb where Ellen is taken, the pavement outside Kessler Tower
 * where she is walked in, the report between sectors, and the roof. They share
 * one cast, one set of vehicles, one street and one rain, which is the only
 * reason four cinematics cost no asset pipeline at all — and the reason a
 * change to a figure here changes all four of them at once.
 *
 * The order the player meets them is: title screen, then the kerb
 * (`abduction_*`), then the drive in [chase.c](chase.c), then the tower's
 * front door (`opening_*`), then the campaign.
 */
#include "cutscene.h"

#include <math.h>

#include "chuck_pose.h"
#include "fx.h"
#include "intel.h"
#include "manual_pages.h"
#include "render_chuck.h"

static const float TRANSITION_DOOR_TOP = 358.0f;
static const float TRANSITION_DOOR_INNER_TOP = 368.0f;
static const float TRANSITION_DOOR_DEPTH_TOP = 376.0f;
static const float OUTRO_REUNION_AGENT_OFFSET = 1.0f;
static const float OUTRO_REUNION_HOSTAGE_OFFSET = -31.0f;
static const float OUTRO_REUNION_HOSTAGE_SCALE = 1.18f;

static void set_color(SDL_Renderer *r, SDL_Color color)
{
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
}

static void set_rgba(SDL_Renderer *r, Uint8 red, Uint8 green,
                     Uint8 blue, Uint8 alpha)
{
    SDL_SetRenderDrawColor(r, red, green, blue, alpha);
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

static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      SDL_Color color, const char *text)
{
    SDL_SetRenderScale(r, scale, scale);
    set_color(r, color);
    SDL_RenderDebugText(r, x / scale, y / scale, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

/* The run this file's own `draw_text` lays down, in pixels — the same
 * arithmetic `render_sprite.c` and `chase_render.c` each keep locally, because
 * every renderer here carries its own `draw_text` rather than sharing one. */
static float text_width(const char *text, float scale)
{
    return (float)SDL_strlen(text) *
           SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
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

static float ease_out_cubic(float value)
{
    float t = 1.0f - clamp01(value);
    return 1.0f - t * t * t;
}

static float lerpf(float from, float to, float amount)
{
    return from + (to - from) * amount;
}

static unsigned scene_hash(unsigned value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

void opening_cutscene_init(OpeningCutscene *cutscene)
{
    SDL_zerop(cutscene);
}

static bool crossed_time(float previous, float current, float cue_time)
{
    return previous < cue_time && current >= cue_time;
}

static bool crossed_any_time(float previous, float current,
                             const float *cue_times, int cue_count)
{
    for (int i = 0; i < cue_count; ++i)
    {
        if (crossed_time(previous, current, cue_times[i]))
            return true;
    }
    return false;
}

/*
 * Which of the tower's windows are lit, asked in one place: the facade asks
 * it to draw them and the wet street asks it again to reflect them, so the
 * road can never show a light the building does not have.
 */
static bool tower_window_lit(int row, int col, float time, unsigned *variation)
{
    unsigned h = scene_hash((unsigned)(row * 29 + col * 7 + 19));
    *variation = h;
    /* One office light flickers subtly during the establishing shot. */
    if (row == 2 && col == 4)
        return fmodf(time, 2.8f) < 2.2f;
    return (h % 6u) == 0u;
}

static SDL_Color tower_window_colour(unsigned variation)
{
    return (variation & 1u) ? (SDL_Color){112, 106, 72, 255}
                            : (SDL_Color){48, 86, 94, 255};
}

/*
 * One office window, and it is glass in a frame rather than a swatch: a sill
 * under it, a mullion down the middle, and the glass either lit from inside —
 * brightest under the ceiling it is lit by, with the blinds half down in some
 * of them — or dark and holding a faint diagonal of the sky. Everything that
 * varies is keyed to the window's own hash, so nothing on the facade shifts
 * while the camera holds.
 */
static void draw_window(SDL_Renderer *r, float x, float y,
                        float w, float h, bool lit, unsigned variation)
{
    color_rect(r, (SDL_Color){5, 10, 17, 255}, x, y, w, h);
    color_rect(r, (SDL_Color){38, 47, 53, 255}, x - 1.0f, y + h, w + 2.0f,
               1.0f);
    if (lit)
    {
        SDL_Color light = tower_window_colour(variation);
        color_rect(r, light, x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f);
        fx_vgrad(r, x + 2.0f, y + 2.0f + (h - 4.0f) * 0.4f, w - 4.0f,
                 (h - 4.0f) * 0.6f, FX_INK, 0, FX_INK, 90);
        color_rect(r, (SDL_Color){154, 145, 94, 255},
                   x + 3.0f, y + 2.0f, w - 6.0f, 1.0f);
        if ((variation >> 5) % 3u == 0u)
        {
            /* Blinds half down: slats across the top of the pane. */
            for (float sy = y + 3.0f; sy < y + h * 0.55f; sy += 2.0f)
                color_rect(r, fx_dim(light, 0.62f), x + 2.0f, sy, w - 4.0f,
                           1.0f);
        }
        else if ((variation >> 5) % 5u == 1u)
        {
            /* Somebody still at a desk, seen from the street. */
            float px = x + 6.0f + (float)((variation >> 9) % 14u);
            color_rect(r, fx_dim(light, 0.35f), px, y + h - 9.0f, 5.0f,
                       7.0f);
            color_rect(r, fx_dim(light, 0.35f), px + 1.0f, y + h - 12.0f,
                       3.0f, 3.0f);
        }
    }
    else
    {
        color_rect(r, (SDL_Color){15, 25, 35, 255},
                   x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f);
        /* The moon is up and to the left: its reflection runs across the
         * dark glass on the diagonal. */
        float band = (float)((variation >> 7) % 9u);
        for (float i = 0.0f; i < h - 4.0f; i += 1.0f)
        {
            float bx = x + 2.0f + band + (h - 4.0f - i) * 0.8f;
            if (bx + 2.0f < x + w - 2.0f)
                color_rect(r, FX_BASE, bx, y + 2.0f + i,
                           2.0f, 1.0f);
        }
    }
    color_rect(r, (SDL_Color){5, 10, 17, 255}, x + w * 0.5f - 0.5f, y, 1.0f,
               h);
}

static void render_city(SDL_Renderer *r, float time, int win_w, int win_h)
{
    /* Smooth night gradient with a storm-lit teal horizon. */
    color_rect(r, FX_NIGHT, 0.0f, 0.0f, (float)win_w, (float)win_h);
    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, (float)win_h,
             (SDL_Color){6, 9, 16, 255}, 255,
             (SDL_Color){18, 28, 40, 255}, 255);

    /* Sparse stars remain visible above the high-rise through the rain. */
    for (unsigned i = 0; i < 58u; ++i)
    {
        unsigned h = scene_hash(i + 31u);
        float x = (float)fx_spread(h, (float)win_w);
        float y = 25.0f + (float)((h >> 8) % 205u);
        float blink = 0.45f + 0.55f *
                                  sinf(time * (0.5f + (float)(i % 4u) * 0.17f) +
                                       (float)(h & 31u));
        SDL_Color star = {(Uint8)(120.0f + blink * 55.0f),
                          (Uint8)(134.0f + blink * 52.0f),
                          (Uint8)(143.0f + blink * 55.0f), 255};
        color_rect(r, star, x, y, i % 9u == 0u ? 2.0f : 1.0f, 1.0f);
    }

    /*
     * The city in two planes, the way the title screen builds it. The far
     * row stands in the haze the city throws up and is a step *lighter* than
     * the sky behind it; the near blocks stand in front of that haze and are
     * a step darker. The old single row sat within a unit or two of the sky
     * it was drawn on, so what the eye found was scattered lit windows and
     * a set of thin pale poles, which were the blocks' edges.
     */
    SDL_Color haze = {30, 48, 60, 255};
    for (int i = 0; i < 8; ++i)
    {
        unsigned h = scene_hash((unsigned)i * 131u + 977u);
        float tw = 54.0f + (float)(h % 34u);
        float x = (float)(i * 112 - 30) + (float)((h >> 6) % 26u);
        float th = 150.0f + (float)((h >> 12) % 90u);
        float y = 432.0f - th;
        SDL_Color wall = fx_mix(FX_SHADOW, haze, 0.18f);
        color_rect(r, wall, x, y, tw, th);
        /* A setback crown on some of them, so the far row is not all boxes. */
        if ((h & 3u) == 0u)
            color_rect(r, wall, x + tw * 0.25f, y - 16.0f, tw * 0.5f, 16.0f);
        if ((h & 3u) == 1u)
            color_rect(r, wall, x + tw * 0.5f - 1.0f, y - 22.0f, 2.0f, 22.0f);
        color_rect(r, fx_mix(wall, haze, 0.45f), x, y, 1.0f, th);
        for (int row = 0; row * 9 < (int)th - 12; ++row)
        {
            for (int col = 0; col * 8 < (int)tw - 10; ++col)
            {
                unsigned wh = scene_hash(h + (unsigned)(row * 37 + col * 11));
                if ((wh % 11u) != 0u)
                    continue;
                color_rect(r, (wh & 16u) ? (SDL_Color){78, 74, 54, 255}
                                         : (SDL_Color){44, 70, 78, 255},
                           x + 5.0f + (float)col * 8.0f,
                           y + 8.0f + (float)row * 9.0f, 3.0f, 2.0f);
            }
        }
    }
    fx_vgrad(r, 0.0f, 250.0f, (float)win_w, 182.0f, haze, 0, haze, 70);

    /* The near blocks: darker than the haze, the moon on their left flanks
     * and along the roofline, and hardware on the roofs. */
    for (int i = 0; i < 9; ++i)
    {
        float x = (float)(i * 104 - 35);
        float h = 88.0f + (float)((i * 37) % 105);
        float y = 432.0f - h;
        SDL_Color wall = FX_NIGHT;
        color_rect(r, wall, x, y, 86.0f, h);
        color_rect(r, FX_BASE, x, y, 2.0f, h);
        color_rect(r, (SDL_Color){36, 46, 54, 255}, x, y, 86.0f, 1.0f);
        color_rect(r, FX_INK, x, y + 1.0f, 86.0f, 2.0f);
        /* Floor lines, faint, so a lit window sits in a building. */
        for (int row = 0; row < (int)h - 12; row += 21)
            color_rect(r, (SDL_Color){13, 19, 27, 255}, x + 2.0f,
                       y + 11.0f + (float)row, 84.0f, 1.0f);

        switch (i % 3)
        {
        case 0:
            /* A water tank on its legs. */
            color_rect(r, wall, x + 56.0f, y - 7.0f, 2.0f, 7.0f);
            color_rect(r, wall, x + 68.0f, y - 7.0f, 2.0f, 7.0f);
            color_rect(r, wall, x + 54.0f, y - 21.0f, 18.0f, 14.0f);
            color_rect(r, wall, x + 56.0f, y - 24.0f, 14.0f, 3.0f);
            color_rect(r, FX_BASE, x + 54.0f, y - 21.0f,
                       1.0f, 14.0f);
            break;
        case 1:
        {
            /* A mast with its aviation light. */
            color_rect(r, wall, x + 22.0f, y - 30.0f, 2.0f, 30.0f);
            color_rect(r, wall, x + 19.0f, y - 4.0f, 8.0f, 4.0f);
            bool on = fmodf(time + (float)i * 0.37f, 1.6f) < 0.8f;
            color_rect(r, on ? FX_RED : FX_RED_DK, x + 22.0f, y - 32.0f,
                       2.0f, 2.0f);
            if (on)
                fx_glow(r, x + 23.0f, y - 31.0f, 7.0f, FX_RED, 60);
            break;
        }
        default:
            /* A stair head and a plant housing. */
            color_rect(r, wall, x + 10.0f, y - 10.0f, 20.0f, 10.0f);
            color_rect(r, FX_BASE, x + 10.0f, y - 10.0f,
                       20.0f, 1.0f);
            color_rect(r, wall, x + 44.0f, y - 5.0f, 26.0f, 5.0f);
            break;
        }

        for (int row = 0; row < (int)h - 24; row += 21)
        {
            for (int col = 0; col < 3; ++col)
            {
                unsigned wh = scene_hash((unsigned)(i * 71 + row + col));
                float wx = x + 15.0f + col * 21.0f;
                float wy = y + 15.0f + row;
                if ((wh & 7u) == 0u)
                {
                    SDL_Color lit = (wh & 8u) ? (SDL_Color){73, 70, 48, 255}
                                              : (SDL_Color){40, 68, 76, 255};
                    color_rect(r, lit, wx, wy, 7.0f, 3.0f);
                    color_rect(r, fx_ramp(lit).lit, wx, wy, 7.0f, 1.0f);
                }
                else
                {
                    color_rect(r, FX_INK, wx, wy, 7.0f,
                               3.0f);
                }
            }
        }
    }

    /* Wet-night haze settles between the skyline and the street. */
    fx_vgrad(r, 0.0f, 352.0f, (float)win_w, 82.0f,
             (SDL_Color){30, 48, 60, 255}, 0,
             (SDL_Color){30, 48, 60, 255}, 60);
}

static void render_tower(SDL_Renderer *r, float time, int win_w)
{
    const float x = (float)win_w - 390.0f;
    const float y = 26.0f;
    const float w = 350.0f;
    const float ground = 437.0f;

    /* The tower's shadowed return. This was a colour with an alpha of 170
     * drawn with blending off, so the alpha went straight into the frame:
     * dark in the window, and a translucent strip down the right of every
     * still `--shot` took of the arrival. */
    fx_rect_a(r, (SDL_Color){3, 6, 10, 255}, 170,
              x + 12.0f, y + 9.0f, w, ground - y);
    color_rect(r, (SDL_Color){31, 40, 48, 255}, x, y, w, ground - y);
    color_rect(r, (SDL_Color){57, 68, 74, 255}, x, y, w, 5.0f);
    color_rect(r, (SDL_Color){84, 96, 100, 255}, x, y, w, 1.0f);
    color_rect(r, (SDL_Color){20, 28, 36, 255},
               x + 7.0f, y + 8.0f, w - 14.0f, ground - y - 8.0f);
    /* The moon, up and to the left, finds the tower's left arris. */
    color_rect(r, (SDL_Color){52, 63, 70, 255}, x, y + 5.0f, 2.0f,
               ground - y - 5.0f);
    fx_hgrad(r, x + 7.0f, y + 8.0f, 60.0f, ground - y - 8.0f,
             (SDL_Color){44, 56, 64, 255}, 40, (SDL_Color){44, 56, 64, 255},
             0);
    /* Piers between the window bays. */
    for (int col = 0; col < 6; ++col)
    {
        float px = x + 55.0f + (float)col * 45.0f;
        color_rect(r, (SDL_Color){25, 34, 42, 255}, px, y + 8.0f, 3.0f,
                   ground - y - 83.0f);
        color_rect(r, (SDL_Color){32, 42, 50, 255}, px, y + 8.0f, 1.0f,
                   ground - y - 83.0f);
    }
    /* Plant on the roof, and the parapet it stands behind. */
    color_rect(r, (SDL_Color){31, 40, 48, 255}, x + 30.0f, y - 12.0f, 42.0f,
               12.0f);
    color_rect(r, (SDL_Color){52, 63, 70, 255}, x + 30.0f, y - 12.0f, 42.0f,
               1.0f);
    for (float gx = x + 34.0f; gx < x + 70.0f; gx += 4.0f)
        color_rect(r, (SDL_Color){20, 28, 36, 255}, gx, y - 9.0f, 2.0f, 7.0f);
    color_rect(r, (SDL_Color){31, 40, 48, 255}, x + 118.0f, y - 6.0f, 22.0f,
               6.0f);
    color_rect(r, (SDL_Color){52, 63, 70, 255}, x + 118.0f, y - 6.0f, 22.0f,
               1.0f);

    /* Leave the stone-clad street level windowless; a tenth row here would
       be partly covered by the facade and look visibly cut off. */
    for (int row = 0; row < 9; ++row)
    {
        float wy = y + 20.0f + row * 34.0f;
        color_rect(r, (SDL_Color){51, 61, 66, 255},
                   x + 8.0f, wy + 21.0f, w - 16.0f, 3.0f);
        for (int col = 0; col < 7; ++col)
        {
            unsigned h;
            bool lit = tower_window_lit(row, col, time, &h);
            draw_window(r, x + 18.0f + col * 45.0f, wy,
                        30.0f, 19.0f, lit, h);
        }
    }

    /* Street-level stone, canopy, and a deep entrance opening. The stone is
     * laid in courses, darker toward the pavement where the rain splashes. */
    color_rect(r, (SDL_Color){48, 53, 53, 255},
               x - 10.0f, ground - 75.0f, w + 20.0f, 75.0f);
    for (float cy = ground - 62.0f; cy < ground; cy += 12.0f)
        color_rect(r, (SDL_Color){36, 40, 41, 255}, x - 10.0f, cy,
                   w + 20.0f, 1.0f);
    fx_vgrad(r, x - 10.0f, ground - 22.0f, w + 20.0f, 22.0f,
             (SDL_Color){18, 22, 24, 255}, 0, (SDL_Color){18, 22, 24, 255},
             120);
    color_rect(r, (SDL_Color){84, 87, 80, 255},
               x - 13.0f, ground - 78.0f, w + 26.0f, 5.0f);
    color_rect(r, (SDL_Color){112, 114, 104, 255},
               x - 13.0f, ground - 78.0f, w + 26.0f, 1.0f);
    color_rect(r, (SDL_Color){24, 28, 29, 255},
               x - 13.0f, ground - 73.0f, w + 26.0f, 1.0f);
    color_rect(r, (SDL_Color){14, 20, 25, 255},
               x + 205.0f, ground - 68.0f, 112.0f, 68.0f);
    color_rect(r, (SDL_Color){6, 11, 16, 255},
               x + 214.0f, ground - 59.0f, 94.0f, 59.0f);
    /* The lobby behind the glass: lit from its own ceiling, falling off to
     * the floor, with the reception desk a dark band across it. */
    fx_vgrad(r, x + 214.0f, ground - 59.0f, 94.0f, 59.0f,
             fx_dim(FX_WARM, 0.45f), 90, fx_dim(FX_WARM, 0.45f), 10);
    color_rect(r, FX_NIGHT, x + 226.0f, ground - 22.0f,
               70.0f, 10.0f);
    color_rect(r, fx_dim(FX_WARM, 0.55f), x + 226.0f, ground - 22.0f, 70.0f,
               1.0f);
    color_rect(r, (SDL_Color){69, 83, 85, 255},
               x + 259.0f, ground - 59.0f, 4.0f, 59.0f);
    /* Door frames and push bars. */
    color_rect(r, (SDL_Color){46, 56, 58, 255}, x + 214.0f, ground - 59.0f,
               2.0f, 59.0f);
    color_rect(r, (SDL_Color){46, 56, 58, 255}, x + 306.0f, ground - 59.0f,
               2.0f, 59.0f);
    color_rect(r, (SDL_Color){104, 114, 108, 255}, x + 247.0f,
               ground - 31.0f, 10.0f, 1.0f);
    color_rect(r, (SDL_Color){104, 114, 108, 255}, x + 265.0f,
               ground - 31.0f, 10.0f, 1.0f);
    color_rect(r, (SDL_Color){118, 128, 116, 255},
               x + 205.0f, ground - 68.0f, 112.0f, 4.0f);
    color_rect(r, FX_AMBER, x + 220.0f, ground - 76.0f, 82.0f, 3.0f);
    fx_glow(r, x + 261.0f, ground - 74.0f, 46.0f, FX_AMBER, 42);
    fx_light_cone(r, x + 261.0f, ground - 73.0f, 42.0f, 58.0f, 74.0f,
                  (SDL_Color){248, 205, 130, 255}, 26);

    /* The building's own name, at the font's own size and centred on the
     * entrance block it is fixed to — 104 pixels of lettering across 112 of
     * stone, which is what a parapet nameplate looks like. */
    draw_text(r, x + 209.0f, ground - 91.0f, 1.0f,
              (SDL_Color){159, 158, 140, 255}, "KESSLER TOWER");

    for (int i = 0; i < 3; ++i)
    {
        /* Three dark shopfronts under the offices, shuttered for the night. */
        float sx = x + 28.0f + i * 57.0f;
        color_rect(r, (SDL_Color){36, 42, 43, 255}, sx, ground - 50.0f, 38.0f,
                   50.0f);
        color_rect(r, (SDL_Color){74, 79, 75, 255},
                   x + 31.0f + i * 57.0f, ground - 46.0f, 32.0f, 4.0f);
        for (float sy = ground - 40.0f; sy < ground - 2.0f; sy += 4.0f)
        {
            color_rect(r, (SDL_Color){28, 33, 34, 255}, sx + 3.0f, sy, 32.0f,
                       1.0f);
            color_rect(r, (SDL_Color){44, 50, 50, 255}, sx + 3.0f, sy + 1.0f,
                       32.0f, 1.0f);
        }
    }

    /* Warning beacon above the tower. */
    float beacon = sinf(time * 4.6f) > 0.2f ? 1.0f : 0.24f;
    color_rect(r, (SDL_Color){42, 47, 48, 255},
               x + 278.0f, y - 9.0f, 31.0f, 9.0f);
    color_rect(r, (SDL_Color){(Uint8)(FX_RUST.r * beacon),
                              (Uint8)(FX_RUST.g * beacon),
                              (Uint8)(FX_RUST.b * beacon), 255},
               x + 287.0f, y - 14.0f, 13.0f, 5.0f);
}

static void draw_headlight_beam(SDL_Renderer *r, float x, float y,
                                float length, SDL_Color color)
{
    SDL_Vertex vertices[3] = {
        {{x, y}, {(float)color.r / 255.0f, (float)color.g / 255.0f,
                  (float)color.b / 255.0f, 0.30f}, {0.0f, 0.0f}},
        {{x + length, y - 8.0f}, {(float)color.r / 255.0f,
                                 (float)color.g / 255.0f,
                                 (float)color.b / 255.0f, 0.0f},
         {0.0f, 0.0f}},
        {{x + length, y + 18.0f}, {(float)color.r / 255.0f,
                                  (float)color.g / 255.0f,
                                  (float)color.b / 255.0f, 0.0f},
         {0.0f, 0.0f}}};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, vertices, 3, NULL, 0);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_wet_streak(SDL_Renderer *r, float cx, float top, float bottom,
                            float w, SDL_Color c, float strength,
                            unsigned seed, float time);

/* A filled disc, one row at a time, so a wheel is round at any size. */
static void fill_disc(SDL_Renderer *r, float cx, float cy, float radius,
                      SDL_Color c)
{
    int rows = (int)ceilf(radius);
    set_color(r, c);
    for (int dy = -rows; dy <= rows; ++dy)
    {
        float yy = (float)dy + 0.5f;
        float half = radius * radius - yy * yy;
        if (half <= 0.0f)
            continue;
        half = sqrtf(half);
        fill_rect(r, floorf(cx - half + 0.5f), floorf(cy) + (float)dy,
                  floorf(half * 2.0f + 0.5f), 1.0f);
    }
}

/*
 * A wheel: a round tyre, the sidewall inside it, a rim whose spokes turn
 * with the car, and the hub catching the moon on its upper left. The film's
 * wheels used to be squares with a cross in them, which at a car's size is
 * the one shape nobody reads as rolling.
 */
static void draw_wheel(SDL_Renderer *r, float cx, float cy,
                       float scale, float rotation)
{
    float radius = 12.0f * scale;
    SDL_Color rim = {92, 99, 96, 255};

    fill_disc(r, cx, cy, radius + 0.5f, FX_INK);
    fill_disc(r, cx, cy, radius - 1.5f, (SDL_Color){30, 34, 36, 255});
    fill_disc(r, cx, cy, radius * 0.62f, rim);
    fill_disc(r, cx, cy, radius * 0.62f - 1.0f, (SDL_Color){58, 64, 63, 255});

    set_color(r, rim);
    for (int i = 0; i < 5; ++i)
    {
        float a = rotation + (float)i * 1.2566371f;
        SDL_RenderLine(r, cx, cy, cx + cosf(a) * radius * 0.55f,
                       cy + sinf(a) * radius * 0.55f);
    }
    fill_disc(r, cx, cy, 2.2f * scale, (SDL_Color){140, 146, 138, 255});
    /* The tread and the rim both catch the moon high on the left. */
    color_rect(r, (SDL_Color){52, 58, 60, 255}, cx - radius * 0.62f,
               cy - radius + 1.0f, radius * 0.5f, 1.0f);
    color_rect(r, (SDL_Color){168, 174, 164, 255}, cx - radius * 0.40f,
               cy - radius * 0.50f, 2.0f, 1.0f);
}

/* The dark of the wheel well, cut into the body over a wheel. */
static void draw_wheel_arch(SDL_Renderer *r, float cx, float cy,
                            float radius)
{
    int rows = (int)ceilf(radius);
    set_color(r, (SDL_Color){8, 11, 14, 255});
    for (int dy = -rows; dy <= 0; ++dy)
    {
        float yy = (float)dy + 0.5f;
        float half = radius * radius - yy * yy;
        if (half <= 0.0f)
            continue;
        half = sqrtf(half);
        fill_rect(r, floorf(cx - half + 0.5f), floorf(cy) + (float)dy,
                  floorf(half * 2.0f + 0.5f), 1.0f);
    }
}

/*
 * A pane of glass at night: it holds the sky, so it is lightest along the
 * top, and one clean streak of reflection runs across it on the diagonal.
 */
static void draw_car_glass(SDL_Renderer *r, float x, float y, float w,
                           float h, SDL_Color base)
{
    color_rect(r, base, x, y, w, h);
    fx_vgrad(r, x, y, w, h * 0.6f, (SDL_Color){58, 82, 94, 255}, 110,
             (SDL_Color){58, 82, 94, 255}, 0);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_rgba(r, 150, 176, 186, 60);
    for (float i = 0.0f; i < h; i += 1.0f)
    {
        float sx = x + w * 0.30f + (h - i) * 0.9f;
        if (sx + 3.0f < x + w)
            fill_rect(r, floorf(sx), y + i, 3.0f, 1.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * What a car leaves in a wet road: its own dark mass, mirrored and fading
 * down the asphalt, and — once they are on — a long column for each lamp.
 */
static void draw_car_reflection(SDL_Renderer *r, float x, float w,
                                float road_y, float time, bool headlight,
                                float head_x, bool taillight, float tail_x,
                                unsigned seed)
{
    /* Stacked narrower as it deepens, so the mirrored body has no corners. */
    fx_vgrad(r, x + 10.0f, road_y, w - 20.0f, 16.0f, FX_INK, 60, FX_INK, 0);
    fx_vgrad(r, x + 22.0f, road_y, w - 44.0f, 24.0f, FX_INK, 50, FX_INK, 0);
    fx_vgrad(r, x + 40.0f, road_y, w - 80.0f, 30.0f, FX_INK, 40, FX_INK, 0);
    if (headlight)
    {
        draw_wet_streak(r, head_x, road_y, road_y + 70.0f, 14.0f, FX_CREAM,
                        0.9f, seed, time);
        /* And the beam itself, laid flat along the wet road ahead. */
        fx_hgrad(r, head_x, road_y + 1.0f, 90.0f, 3.0f, FX_CREAM, 70,
                 FX_CREAM, 0);
    }
    if (taillight)
        draw_wet_streak(r, tail_x, road_y, road_y + 50.0f, 8.0f, FX_RED,
                        0.8f, seed + 5u, time);
}

/*
 * `headlights` is separate from `moving` because the one thing anybody
 * remembers about the vehicle is that it came up the kerb lane dark. A rolling
 * SUV throwing a beam is a car; a rolling SUV throwing nothing is a decision.
 * The tail lamps follow the headlamps for the same reason: a dark car is dark
 * at both ends.
 */
static void draw_suv(SDL_Renderer *r, float x, float ground_y,
                     float time, bool moving, bool headlights, bool door_open)
{
    const float y = ground_y - 51.0f;
    float rotation = moving ? time * 15.0f : 0.35f;
    SDL_Color body = {30, 35, 38, 255};
    SDL_Color body_lt = {52, 58, 60, 255};

    draw_car_reflection(r, x, 151.0f, ground_y + 9.0f, time, headlights,
                        x + 146.0f, headlights, x + 5.0f, 71u);
    if (headlights)
        draw_headlight_beam(r, x + 146.0f, y + 33.0f, 106.0f, FX_CREAM);

    /* A real pool with real blending: the old slab passed alpha into a
       helper that never set a blend mode, so every shadow in the film
       rendered as solid black. It lies under the tyres, on the road. */
    fx_contact_shadow(r, x + 77.0f, ground_y + 6.0f, 83.0f, 0.0f, 200);

    /* The silhouette, then the body shaded as a solid: the flanks catch the
     * sky along the top and fall into shadow toward the sills. */
    color_rect(r, FX_INK, x, y + 15.0f, 151.0f, 36.0f);
    color_rect(r, FX_INK, x + 11.0f, y + 2.0f, 95.0f, 16.0f);
    color_rect(r, body, x + 4.0f, y + 16.0f, 142.0f, 31.0f);
    fx_vgrad(r, x + 4.0f, y + 22.0f, 142.0f, 25.0f, body, 0, FX_INK, 150);
    color_rect(r, (SDL_Color){47, 51, 49, 255}, x + 13.0f, y + 4.0f, 91.0f,
               20.0f);
    /* The roof and the bonnet are the surfaces the moon lands on. */
    color_rect(r, (SDL_Color){78, 84, 80, 255}, x + 14.0f, y + 4.0f, 89.0f,
               1.0f);
    color_rect(r, body_lt, x + 104.0f, y + 16.0f, 41.0f, 1.0f);
    color_rect(r, fx_dim(body_lt, 0.8f), x + 5.0f, y + 16.0f, 8.0f, 1.0f);

    draw_car_glass(r, x + 18.0f, y + 7.0f, 34.0f, 16.0f,
                   (SDL_Color){12, 20, 26, 255});
    draw_car_glass(r, x + 57.0f, y + 7.0f, 39.0f, 16.0f,
                   (SDL_Color){9, 17, 23, 255});
    /* The raked screen at the front of the cabin. */
    for (int i = 0; i < 16; ++i)
        color_rect(r, (SDL_Color){9, 17, 23, 255}, x + 96.0f, y + 7.0f + i,
                   (float)i * 0.45f, 1.0f);

    color_rect(r, (SDL_Color){82, 88, 82, 255}, x + 6.0f, y + 19.0f, 135.0f,
               3.0f);
    color_rect(r, (SDL_Color){112, 118, 110, 255}, x + 6.0f, y + 19.0f,
               135.0f, 1.0f);
    color_rect(r, (SDL_Color){18, 21, 22, 255}, x + 66.0f, y + 23.0f, 2.0f,
               21.0f);
    color_rect(r, (SDL_Color){18, 21, 22, 255}, x + 105.0f, y + 23.0f, 2.0f,
               21.0f);
    /* Handles and the mirror, the three small things that make it a car. */
    color_rect(r, body_lt, x + 57.0f, y + 26.0f, 5.0f, 1.0f);
    color_rect(r, body_lt, x + 96.0f, y + 26.0f, 5.0f, 1.0f);
    color_rect(r, FX_INK, x + 102.0f, y + 12.0f, 7.0f, 5.0f);
    color_rect(r, body, x + 103.0f, y + 13.0f, 5.0f, 3.0f);
    /* The grille and the sill. */
    color_rect(r, (SDL_Color){18, 21, 22, 255}, x + 138.0f, y + 23.0f, 8.0f,
               6.0f);
    for (int i = 0; i < 3; ++i)
        color_rect(r, (SDL_Color){58, 64, 63, 255}, x + 139.0f,
                   y + 24.0f + (float)i * 2.0f, 6.0f, 1.0f);
    color_rect(r, (SDL_Color){20, 24, 26, 255}, x + 4.0f, y + 44.0f, 142.0f,
               3.0f);

    draw_wheel_arch(r, x + 31.0f, ground_y - 3.0f, 15.0f);
    draw_wheel_arch(r, x + 121.0f, ground_y - 3.0f, 15.0f);

    color_rect(r, FX_RUST, x + 2.0f, y + 29.0f, 6.0f, 8.0f);
    color_rect(r, fx_ramp(FX_RUST).lit, x + 2.0f, y + 29.0f, 6.0f, 1.0f);
    if (headlights)
    {
        fx_glow(r, x + 4.0f, y + 33.0f, 14.0f, FX_RED, 90);
        fx_glow(r, x + 146.0f, y + 31.0f, 22.0f, FX_CREAM, 90);
    }
    color_rect(r, headlights ? FX_CREAM : (SDL_Color){63, 62, 50, 255},
               x + 142.0f, y + 28.0f, 7.0f, 7.0f);
    color_rect(r, (SDL_Color){93, 99, 94, 255},
               x + 119.0f, y + 41.0f, 19.0f, 4.0f);
    color_rect(r, (SDL_Color){132, 138, 128, 255},
               x + 119.0f, y + 41.0f, 19.0f, 1.0f);

    if (door_open)
    {
        color_rect(r, FX_INK, x + 67.0f, y + 23.0f, 38.0f, 26.0f);
        /* The cabin light spilling on the sill: the door is open onto a
           dark street, and the inside of the car is the one lit thing. */
        fx_vgrad(r, x + 69.0f, y + 25.0f, 34.0f, 22.0f,
                 fx_dim(FX_WARM, 0.35f), 120, FX_INK, 0);
        color_rect(r, (SDL_Color){55, 59, 56, 255},
                   x + 61.0f, y + 20.0f, 6.0f, 30.0f);
        color_rect(r, (SDL_Color){95, 99, 90, 255},
                   x + 60.0f, y + 21.0f, 2.0f, 28.0f);
    }

    draw_wheel(r, x + 31.0f, ground_y - 4.0f, 1.0f, rotation);
    draw_wheel(r, x + 121.0f, ground_y - 4.0f, 1.0f, rotation);
}

static void draw_agent_car(SDL_Renderer *r, float x, float ground_y,
                           float time, bool moving, bool occupied)
{
    const float y = ground_y - 43.0f;
    float rotation = moving ? time * 17.0f : 0.65f;
    SDL_Color body = {28, 70, 91, 255};
    SDL_Color body_lt = {71, 143, 153, 255};

    draw_car_reflection(r, x, 141.0f, ground_y + 9.0f, time, moving,
                        x + 137.0f, moving, x + 6.0f, 43u);
    if (moving)
        draw_headlight_beam(r, x + 137.0f, y + 29.0f, 92.0f, FX_CREAM);

    fx_contact_shadow(r, x + 70.0f, ground_y + 6.0f, 76.0f, 0.0f, 200);
    color_rect(r, FX_INK, x, y + 15.0f, 141.0f, 28.0f);
    color_rect(r, FX_INK, x + 29.0f, y + 1.0f, 73.0f, 16.0f);
    color_rect(r, body, x + 4.0f, y + 17.0f, 133.0f, 22.0f);
    fx_vgrad(r, x + 4.0f, y + 23.0f, 133.0f, 16.0f, body, 0, FX_INK, 140);
    color_rect(r, (SDL_Color){41, 101, 121, 255},
               x + 31.0f, y + 3.0f, 69.0f, 18.0f);
    color_rect(r, fx_ramp((SDL_Color){41, 101, 121, 255}).lit,
               x + 32.0f, y + 3.0f, 67.0f, 1.0f);
    /* Boot lid and bonnet catching the moon along their tops. */
    color_rect(r, fx_mix(body, body_lt, 0.55f), x + 5.0f, y + 17.0f, 26.0f,
               1.0f);
    color_rect(r, fx_mix(body, body_lt, 0.55f), x + 100.0f, y + 17.0f, 36.0f,
               1.0f);

    draw_car_glass(r, x + 36.0f, y + 6.0f, 28.0f, 14.0f,
                   (SDL_Color){9, 20, 28, 255});
    draw_car_glass(r, x + 68.0f, y + 6.0f, 28.0f, 14.0f,
                   (SDL_Color){11, 24, 31, 255});
    color_rect(r, body_lt, x + 6.0f, y + 18.0f, 126.0f, 3.0f);
    color_rect(r, fx_ramp(body_lt).lit, x + 6.0f, y + 18.0f, 126.0f, 1.0f);
    color_rect(r, (SDL_Color){15, 43, 59, 255},
               x + 67.0f, y + 21.0f, 2.0f, 17.0f);
    color_rect(r, (SDL_Color){15, 43, 59, 255},
               x + 99.0f, y + 21.0f, 1.0f, 15.0f);
    color_rect(r, fx_mix(body, body_lt, 0.6f), x + 58.0f, y + 24.0f, 5.0f,
               1.0f);
    color_rect(r, fx_mix(body, body_lt, 0.6f), x + 90.0f, y + 24.0f, 5.0f,
               1.0f);
    /* The mirror on the door post, and a chrome sill line. */
    color_rect(r, FX_INK, x + 96.0f, y + 12.0f, 6.0f, 5.0f);
    color_rect(r, body, x + 97.0f, y + 13.0f, 4.0f, 3.0f);
    color_rect(r, (SDL_Color){109, 133, 133, 255}, x + 30.0f, y + 36.0f,
               70.0f, 1.0f);

    if (occupied)
    {
        color_rect(r, (SDL_Color){191, 133, 91, 255},
                   x + 75.0f, y + 8.0f, 7.0f, 8.0f);
        color_rect(r, (SDL_Color){47, 27, 22, 255},
                   x + 74.0f, y + 6.0f, 9.0f, 4.0f);
        color_rect(r, FX_CREAM, x + 80.0f, y + 10.0f, 2.0f, 1.0f);
        /* His headband, even at the wheel. */
        color_rect(r, FX_RED, x + 74.0f, y + 9.0f, 9.0f, 1.0f);
    }

    draw_wheel_arch(r, x + 27.0f, ground_y - 2.0f, 14.0f);
    draw_wheel_arch(r, x + 113.0f, ground_y - 2.0f, 14.0f);

    color_rect(r, FX_RUST, x + 3.0f, y + 25.0f, 6.0f, 7.0f);
    color_rect(r, fx_ramp(FX_RUST).lit, x + 3.0f, y + 25.0f, 6.0f, 1.0f);
    if (moving)
    {
        fx_glow(r, x + 5.0f, y + 28.0f, 13.0f, FX_RED, 80);
        fx_glow(r, x + 137.0f, y + 27.0f, 20.0f, FX_CREAM, 90);
    }
    color_rect(r, moving ? FX_CREAM : fx_mix(FX_CREAM, FX_STEEL, 0.45f),
               x + 133.0f, y + 24.0f, 7.0f, 7.0f);
    color_rect(r, (SDL_Color){109, 133, 133, 255},
               x + 113.0f, y + 34.0f, 19.0f, 4.0f);
    color_rect(r, (SDL_Color){150, 172, 170, 255},
               x + 113.0f, y + 34.0f, 19.0f, 1.0f);
    color_rect(r, (SDL_Color){109, 133, 133, 255},
               x + 1.0f, y + 34.0f, 14.0f, 3.0f);

    draw_wheel(r, x + 27.0f, ground_y - 3.0f, 0.92f, rotation);
    draw_wheel(r, x + 113.0f, ground_y - 3.0f, 0.92f, rotation);
}

static void sprite_rect(SDL_Renderer *r, float x, float y, float sprite_w,
                        int dir, float scale, float lx, float ly,
                        float w, float h, SDL_Color color)
{
    float rx = dir >= 0 ? x + lx * scale
                        : x + (sprite_w - lx - w) * scale;
    color_rect(r, color, floorf(rx), floorf(y + ly * scale),
               ceilf(w * scale), ceilf(h * scale));
}

/*
 * Scale-aware versions of the three passes every figure in the sector gets:
 * a tapered ink silhouette, the garment as a ramp with a lit crown and a
 * shaded underside, one rim down the leading flank. The film's cast used to
 * be flat stacks of rects inside square outlines — a cheaper drawing exactly
 * where the game asks the player to care most.
 */
static void mass_scaled(SDL_Renderer *r, float x, float y, float sprite_w,
                        int dir, float scale, float lx, float ly,
                        float w, float h, SDL_Color c, int top, int bottom)
{
    int rows = (int)h;

    for (int i = 0; i < rows; ++i)
    {
        float inset = fx_taper(i, rows, top, bottom);
        if (inset * 2.0f >= w)
            continue;
        sprite_rect(r, x, y, sprite_w, dir, scale,
                    lx + inset, ly + (float)i, w - inset * 2.0f, 1.0f, c);
    }
}

static void body_scaled(SDL_Renderer *r, float x, float y, float sprite_w,
                        int dir, float scale, float lx, float ly,
                        float w, float h, SDL_Color base, int top, int bottom)
{
    FxRamp ramp = fx_ramp(base);
    SDL_Color rim = fx_mix(ramp.base, ramp.lit, 0.50f);
    int rows = (int)h;

    mass_scaled(r, x, y, sprite_w, dir, scale, lx - 1.0f, ly - 1.0f,
                w + 2.0f, h + 2.0f, FX_INK, top + 1, bottom + 1);
    for (int i = 0; i < rows; ++i)
    {
        float inset = fx_taper(i, rows, top, bottom);
        float rw = w - inset * 2.0f;
        if (rw < 1.0f)
            continue;
        SDL_Color c = i == 0 ? ramp.lit
                             : (i == rows - 1 ? ramp.dark : ramp.base);
        sprite_rect(r, x, y, sprite_w, dir, scale,
                    lx + inset, ly + (float)i, rw, 1.0f, c);
        if (i > 0 && i < rows - 1 && rw >= 6.0f)
            sprite_rect(r, x, y, sprite_w, dir, scale,
                        lx + inset + rw - 1.0f, ly + (float)i, 1.0f, 1.0f,
                        rim);
    }
}

/*
 * One leg of the same two-beat walk the sector cast uses: stance tracks the
 * foot back under the body at a constant rate, swing lifts and reaches on an
 * arc. A sine is slowest exactly where the foot carries the body fastest,
 * which is what made the film's cast skate. Callers offset the phase a
 * quarter turn so a figure handed a frozen clock stands planted.
 */
static void walk_leg(float cycle, float reach, float *out_dx, float *out_lift)
{
    cycle -= floorf(cycle);
    if (cycle < 0.5f)
    {
        *out_dx = reach * (1.0f - 4.0f * cycle);
        *out_lift = 0.0f;
    }
    else
    {
        float t = (cycle - 0.5f) * 2.0f;
        float ease = t * t * (3.0f - 2.0f * t);
        *out_dx = reach * (-1.0f + 2.0f * ease);
        *out_lift = sinf(t * 3.14159265f) * reach * 1.1f;
    }
}

/* ---- The cast's limbs ------------------------------------------------ */

/*
 * Where a figure stands on screen and which way it faces, so that a limb can
 * be given in the sprite's own coordinates and land exactly where
 * `sprite_rect` would put a rectangle at the same place.
 */
typedef struct
{
    SDL_Renderer *r;
    float x;     /* the sprite's left edge on screen */
    float y;     /* its top on screen */
    float w;     /* its width in sprite units, which the mirror turns about */
    int dir;
    float scale;
} CastFrame;

static float cast_sx(const CastFrame *f, float lx)
{
    return f->dir >= 0 ? f->x + lx * f->scale
                       : f->x + (f->w - lx) * f->scale;
}

static float cast_sy(const CastFrame *f, float ly)
{
    return f->y + ly * f->scale;
}

static void cast_rect(const CastFrame *f, float lx, float ly, float w,
                      float h, SDL_Color c)
{
    sprite_rect(f->r, f->x, f->y, f->w, f->dir, f->scale, lx, ly, w, h, c);
}

static void cast_mass(const CastFrame *f, float lx, float ly, float w,
                      float h, SDL_Color c, int top, int bottom)
{
    mass_scaled(f->r, f->x, f->y, f->w, f->dir, f->scale, lx, ly, w, h, c,
                top, bottom);
}

static void cast_body(const CastFrame *f, float lx, float ly, float w,
                      float h, SDL_Color c, int top, int bottom)
{
    body_scaled(f->r, f->x, f->y, f->w, f->dir, f->scale, lx, ly, w, h, c,
                top, bottom);
}

/*
 * A straight run of pixels `w` wide between two screen points, stepped along
 * whichever axis is the longer so that a steep limb and a level one come out
 * equally solid. `cap` carries the run past both ends, which is what lets an
 * outline close round the end of a limb instead of stopping flush with it.
 */
static void stroke_px(SDL_Renderer *r, float x1, float y1, float x2, float y2,
                      float w, float cap, SDL_Color c)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);

    if (cap > 0.0f && len > 0.001f)
    {
        x1 -= dx / len * cap;
        y1 -= dy / len * cap;
        x2 += dx / len * cap;
        y2 += dy / len * cap;
        dx = x2 - x1;
        dy = y2 - y1;
    }

    set_color(r, c);
    if (fabsf(dy) >= fabsf(dx))
    {
        float top = floorf(fminf(y1, y2));
        float bottom = floorf(fmaxf(y1, y2));
        for (float row = top; row <= bottom; row += 1.0f)
        {
            float t = fabsf(dy) > 0.001f
                          ? clamp01((row + 0.5f - y1) / dy)
                          : 0.0f;
            fill_rect(r, floorf(x1 + dx * t - w * 0.5f + 0.5f), row, w, 1.0f);
        }
    }
    else
    {
        float left = floorf(fminf(x1, x2));
        float right = floorf(fmaxf(x1, x2));
        for (float col = left; col <= right; col += 1.0f)
        {
            float t = clamp01((col + 0.5f - x1) / dx);
            fill_rect(r, col, floorf(y1 + dy * t - w * 0.5f + 0.5f), 1.0f, w);
        }
    }
}

static float cast_limb_px(const CastFrame *f, float width)
{
    return fmaxf(2.0f, roundf(width * f->scale));
}

/* A strap or a barrel: one flat band, no outline and no shading of its own. */
static void cast_band(const CastFrame *f, float lx1, float ly1, float lx2,
                      float ly2, float width, SDL_Color c)
{
    stroke_px(f->r, cast_sx(f, lx1), cast_sy(f, ly1), cast_sx(f, lx2),
              cast_sy(f, ly2), fmaxf(1.0f, roundf(width * f->scale)), 0.0f,
              c);
}

/* The outline of one bone. Every bone of a limb is inked before any of them
 * is painted, so the joint between two of them never carries a seam. */
static void cast_bone_ink(const CastFrame *f, float lx1, float ly1,
                          float lx2, float ly2, float width)
{
    stroke_px(f->r, cast_sx(f, lx1), cast_sy(f, ly1), cast_sx(f, lx2),
              cast_sy(f, ly2), cast_limb_px(f, width) + 2.0f, 1.0f, FX_INK);
}

/*
 * One bone as the cylinder the sector's cast is built from: the garment, a
 * lit pixel along one side and a shaded one along the other. A limb standing
 * up takes its rim on the flank the figure faces, the same flank a body block
 * carries it on; a limb held level is lit along the top and shaded under it.
 */
static void cast_bone_paint(const CastFrame *f, float lx1, float ly1,
                            float lx2, float ly2, float width, SDL_Color fill)
{
    FxRamp ramp = fx_ramp(fill);
    float w = cast_limb_px(f, width);
    float edge = (w - 1.0f) * 0.5f;
    float x1 = cast_sx(f, lx1);
    float y1 = cast_sy(f, ly1);
    float x2 = cast_sx(f, lx2);
    float y2 = cast_sy(f, ly2);

    stroke_px(f->r, x1, y1, x2, y2, w, 0.0f, fill);
    if (w < 3.0f)
        return;
    if (fabsf(y2 - y1) >= fabsf(x2 - x1))
    {
        float lead = f->dir >= 0 ? edge : -edge;
        stroke_px(f->r, x1 + lead, y1, x2 + lead, y2, 1.0f, 0.0f,
                  fx_mix(ramp.base, ramp.lit, 0.55f));
        stroke_px(f->r, x1 - lead, y1, x2 - lead, y2, 1.0f, 0.0f,
                  ramp.dark);
    }
    else
    {
        stroke_px(f->r, x1, y1 - edge, x2, y2 - edge, 1.0f, 0.0f, ramp.lit);
        stroke_px(f->r, x1, y1 + edge, x2, y2 + edge, 1.0f, 0.0f, ramp.dark);
    }
}

/*
 * The middle joint of a two-bone limb whose bones are both `bone` long.
 *
 * Solving for it rather than placing it is what holds a pair of arms to one
 * length however they are posed. The film used to draw every arm as whatever
 * rectangle the pose wanted, so Chuck walked with a stub, aimed with a plank
 * half again as long, and shot at the helicopter with a line three times the
 * length of either — the defect the owner found on the hostage, on every
 * other figure in the same file. `bend` picks the side the joint breaks to:
 * +1 behind the line for an elbow, -1 in front of it for a knee. `clamp`
 * pulls a target the limb cannot reach back onto the end of it; a leg is
 * allowed to stretch the fraction a full stride asks for instead, because a
 * foot lifted off the pavement to keep a knee honest reads far worse.
 */
static void cast_joint(float ax, float ay, float *bx, float *by, float bone,
                       float bend, bool clamp, float *jx, float *jy)
{
    float dx = *bx - ax;
    float dy = *by - ay;
    float d = sqrtf(dx * dx + dy * dy);
    float reach = bone * 2.0f;

    if (d < 0.001f)
    {
        *jx = ax;
        *jy = ay + bone;
        return;
    }
    if (d > reach && clamp)
    {
        *bx = ax + dx / d * reach;
        *by = ay + dy / d * reach;
        dx = *bx - ax;
        dy = *by - ay;
        d = reach;
    }
    float half = d * 0.5f;
    float h = sqrtf(fmaxf(0.0f, bone * bone - half * half));
    *jx = ax + dx * 0.5f - dy / d * h * bend;
    *jy = ay + dy * 0.5f + dx / d * h * bend;
}

/* One length for every arm and one for every leg in the film, so no pose can
 * hand a figure a longer limb than the one beside it. */
#define CAST_ARM_BONE 4.6f
#define CAST_LEG_BONE 4.2f
#define CAST_ARM_W 2.7f
#define CAST_LEG_W 3.4f
/* The ankle a foot stands on: the sole under it finishes on the pavement. */
#define CAST_ANKLE_Y 29.0f

static void cast_hand(const CastFrame *f, float hx, float hy, SDL_Color skin)
{
    cast_rect(f, hx - 1.5f, hy - 1.0f, 3.5f, 3.0f, FX_INK);
    cast_rect(f, hx - 1.0f, hy - 0.5f, 2.5f, 2.0f, skin);
    cast_rect(f, hx - 1.0f, hy - 0.5f, 2.5f, 0.5f, fx_ramp(skin).lit);
}

/* Shoulder to hand. Returns where the hand actually landed, which is the
 * target pulled back onto the end of the arm if it asked for too much. */
static void cast_arm(const CastFrame *f, float sx, float sy, float *hx,
                     float *hy, SDL_Color sleeve, SDL_Color forearm,
                     SDL_Color skin, bool hand)
{
    float ex, ey;
    cast_joint(sx, sy, hx, hy, CAST_ARM_BONE, 1.0f, true, &ex, &ey);
    cast_bone_ink(f, sx, sy, ex, ey, CAST_ARM_W);
    cast_bone_ink(f, ex, ey, *hx, *hy, CAST_ARM_W);
    cast_bone_paint(f, sx, sy, ex, ey, CAST_ARM_W, sleeve);
    cast_bone_paint(f, ex, ey, *hx, *hy, CAST_ARM_W, forearm);
    if (hand)
        cast_hand(f, *hx, *hy, skin);
}

/* The sector's shoe at the film's scale: heel, sole, and the toe cap that
 * points the figure somewhere. */
static void cast_shoe(const CastFrame *f, float ankle_x, float ankle_y,
                      SDL_Color boot)
{
    FxRamp ramp = fx_ramp(boot);
    cast_mass(f, ankle_x - 2.0f, ankle_y - 1.0f, 7.5f, 4.0f, FX_INK, 1, 0);
    cast_rect(f, ankle_x - 1.0f, ankle_y, 5.5f, 2.0f, boot);
    cast_rect(f, ankle_x - 1.0f, ankle_y, 2.0f, 1.0f, ramp.dark);
    cast_rect(f, ankle_x + 2.5f, ankle_y, 2.0f, 1.0f, ramp.lit);
}

static void cast_leg(const CastFrame *f, float hip_x, float hip_y,
                     float ankle_x, float ankle_y, SDL_Color trouser,
                     SDL_Color boot)
{
    float kx, ky;
    cast_joint(hip_x, hip_y, &ankle_x, &ankle_y, CAST_LEG_BONE, -1.0f, false,
               &kx, &ky);
    cast_bone_ink(f, hip_x, hip_y, kx, ky, CAST_LEG_W);
    cast_bone_ink(f, kx, ky, ankle_x, ankle_y, CAST_LEG_W * 0.9f);
    cast_bone_paint(f, hip_x, hip_y, kx, ky, CAST_LEG_W, trouser);
    cast_bone_paint(f, kx, ky, ankle_x, ankle_y, CAST_LEG_W * 0.9f, trouser);
    cast_shoe(f, ankle_x, ankle_y, boot);
}

/*
 * A pair of legs on one clock. A frozen clock stands the figure with its feet
 * a little apart; a running one hands each leg the two-beat stride the sector
 * cast walks on, half a turn apart, with the lift turned up for a figure that
 * is running rather than walking.
 */
static void cast_legs(const CastFrame *f, bool moving, float cycle,
                      float reach, float lift_gain, float rear_hip,
                      float near_hip, float hip_y, float stance,
                      SDL_Color rear_trouser, SDL_Color near_trouser,
                      SDL_Color rear_boot, SDL_Color near_boot)
{
    float rear_dx = -stance;
    float near_dx = stance;
    float rear_lift = 0.0f;
    float near_lift = 0.0f;

    if (moving)
    {
        walk_leg(cycle, reach, &near_dx, &near_lift);
        walk_leg(cycle + 0.5f, reach, &rear_dx, &rear_lift);
        near_lift *= lift_gain;
        rear_lift *= lift_gain;
    }
    cast_leg(f, rear_hip, hip_y, rear_hip + rear_dx, CAST_ANKLE_Y - rear_lift,
             rear_trouser, rear_boot);
    cast_leg(f, near_hip, hip_y, near_hip + near_dx, CAST_ANKLE_Y - near_lift,
             near_trouser, near_boot);
}

/* ---- Chuck ------------------------------------------------------------ */

/*
 * Chuck in the film is Chuck in the sector, and now by construction rather than
 * by agreement: both are the skeleton in [chuck_pose.h](chuck_pose.h) drawn by
 * [render_chuck.c](render_chuck.c), this one at the film's scale.
 *
 * He used to be drawn here a second time, by hand, and the corridor between
 * sectors is where that showed worst, because it is the one place the film
 * shows him *running*, at one and a half times the size, for four seconds at a
 * stretch. The run was a
 * pair of feet pedalling three and a half units either side of the hips on a
 * clock, while the man was eased across the screen on a smoothstep at up to two
 * hundred and sixty pixels a second: the feet were sliding under him the whole
 * way. The cycle now comes from where he is rather than from the time, so a foot
 * put down stays where it was put down while the ease carries him over it, and
 * the pace he is going at blends the stride out to standing at both ends of the
 * ease instead of freezing him mid-stride where it stops.
 *
 * **And he runs the plain run here, not the sector's.** Drawn with the sector's
 * run he was the one figure in the film acting: the heel kicked up behind him,
 * the spine pitched into it, the elbows driving and the headband streaming,
 * beside captors who walk on a flat two-beat step with their bodies still and a
 * woman who barely lifts her feet. The skeleton was right and the style was
 * not, and the owner said so the first time they watched the two together.
 * `CHUCK_GAIT_PLAIN_RUN` keeps the run's flight and stride, so a foot still
 * stays where it was put at every speed the ease reaches, and holds everything
 * a viewer sees under his own walk. The sector keeps the full run, because
 * there a player is steering it and nobody is standing beside him to compare.
 */
static const SDL_Color CAST_GUNMETAL = {52, 58, 62, 255};

/* A pistol pointed along (dx, dy) from the hand, for a figure that does not
 * hold it level. */
static void cast_pistol_angled(const CastFrame *f, float hx, float hy,
                               float dx, float dy, float length)
{
    float mx = hx + dx * length;
    float my = hy + dy * length;
    cast_band(f, hx - dx * 1.0f, hy - dy * 1.0f, mx + dx * 0.6f,
              my + dy * 0.6f, 3.2f, FX_INK);
    cast_band(f, hx, hy, mx, my, 1.8f, CAST_GUNMETAL);
}


/* The film lays its cast out in boxes of its own; his skeleton lives in the
 * sector's twenty-six-unit box and is centred in whichever one a scene places.
 * The armed poses have the wider box because the pistol reaches past the
 * figure, and the outro's tracers are placed through it. */
#define AGENT_W 28.0f
/*
 * His size in the film, and his legs.
 *
 * The crew are drawn at 1.32 to 1.35 and he used to be drawn at 1.48, on legs a
 * fifth longer than theirs under a jacket two rows shorter, so between the men
 * he was chasing he stood half a head taller and read as a different build. The
 * sector's man keeps the sector's legs, because a jump and a stride are
 * measured on them; this one is fitted to the crew's proportions instead, and
 * drawn a hair larger than they are, which is as much as a lead is owed.
 */
#define AGENT_SCALE 1.40f
#define AGENT_LEGS 0.82f
#define AGENT_HELD_W 30.0f
#define AGENT_SKY_ANGLE 1.15f

static ChuckView agent_view(SDL_Renderer *r, float x, float ground_y,
                            float scale, int dir, float box_w)
{
    return (ChuckView){r, x + (box_w - CHUCK_BOX_W) * 0.5f * scale,
                       ground_y - CHUCK_GROUND_Y * scale, dir, scale, FX_INK};
}

/* How fast something eased along a smoothstep is going `u` of the way through
   it, as a share of its top speed: nought at both ends and one in the middle. */
static float smoothstep_pace(float u)
{
    u = clamp01(u);
    return 4.0f * u * (1.0f - u);
}

/*
 * Chuck on his feet: standing, or covering ground at `pace` (0..1) of the ease
 * that is moving him. The place in the gait comes from `x` itself, turned into
 * distance forward by `dir`.
 */
static void draw_agent(SDL_Renderer *r, float x, float ground_y, float scale,
                       float time, int dir, ChuckGait gait, float pace)
{
    ChuckView view = agent_view(r, x, ground_y, scale, dir, AGENT_W);
    ChuckPose pose;

    chuck_pose_stand(&pose, sinf(time * 2.2f));
    if (pace > 0.0f)
    {
        ChuckPose moving;
        chuck_pose_gait(&moving, gait,
                        chuck_gait_cycle(gait, x / scale * (float)dir /
                                                   AGENT_LEGS));
        chuck_pose_blend(&pose, &moving, smoothstep01(pace / 0.35f));
    }
    chuck_pose_fit_legs(&pose, AGENT_LEGS);
    chuck_pose_solve(&pose);

    fx_contact_shadow(r, chuck_view_x(&view, CHUCK_ROOT_X), ground_y - 2.0f,
                      12.0f * scale, 0.0f, 190);
    chuck_draw(&view, &pose, fx_blinking(time, 0x1u));
}

/* At the end of a run with nothing left to run at: on his feet, not at ease. */
static void agent_pose_braced(ChuckPose *pose)
{
    chuck_pose_stand(pose, 0.0f);
    chuck_pose_sink(pose, 0.4f);
    pose->lean = 0.8f;
    pose->arm_swing[CHUCK_NEAR] = 0.10f;
    pose->arm_bend[CHUCK_NEAR] = 0.95f;
    pose->arm_swing[CHUCK_FAR] = -0.10f;
    pose->arm_bend[CHUCK_FAR] = 0.95f;
}

/*
 * The plain run with a reason in it: the legs are the film's, flat and low, but
 * he is pitched into it and his fists are carried and driving. Nothing in the
 * legs changes, so the foot stays where it was put; only the spine and the arms
 * do, which is what a viewer reads as urgency. He runs it twice: after the SUV
 * at the kerb, and after the men who walk her into the tower.
 */
static void agent_hurry(ChuckPose *pose)
{
    pose->lean += 1.1f;
    for (int side = 0; side < 2; ++side)
    {
        pose->arm_swing[side] *= 1.8f;
        pose->arm_bend[side] += 1.25f;
    }
}

/* How far into his stride he is at `x`, in the skeleton's units on the film's
   legs: what the gait is driven by. */
static float agent_stride_distance(float x, int dir)
{
    return x / AGENT_SCALE * (float)dir / AGENT_LEGS;
}

/* Running at `pace` (0..1) of the ease that moves him, out of `rest` and back
   into it at both ends. */
static void agent_pose_running(ChuckPose *pose, const ChuckPose *rest, float x,
                               int dir, float pace)
{
    ChuckPose moving;
    chuck_pose_gait(&moving, CHUCK_GAIT_PLAIN_RUN,
                    chuck_gait_cycle(CHUCK_GAIT_PLAIN_RUN,
                                     agent_stride_distance(x, dir)));
    agent_hurry(&moving);
    *pose = *rest;
    chuck_pose_blend(pose, &moving, smoothstep01(pace / 0.35f));
}

typedef enum
{
    AGENT_FOOT_NONE,
    AGENT_FOOT_NEAR,
    AGENT_FOOT_FAR
} AgentFoot;

/*
 * Which foot came down between two places on a run: the near heel strikes at
 * the top of each cycle and the far one half a cycle on. Both runs used to hear
 * two lists of times written beside them — eight steps on the kerb's way out
 * where his feet came down four times — and nothing kept them in step with the
 * legs once the legs changed. None is heard while the stride has all but
 * blended back to standing at either end.
 */
static AgentFoot agent_footfall(float x0, float x1, int dir, float pace)
{
    if (pace < 0.2f)
        return AGENT_FOOT_NONE;
    float half = chuck_gait_stride(CHUCK_GAIT_PLAIN_RUN) * 0.5f;
    int was = (int)floorf(agent_stride_distance(x0, dir) / half);
    int now = (int)floorf(agent_stride_distance(x1, dir) / half);
    if (now == was)
        return AGENT_FOOT_NONE;
    return (now & 1) ? AGENT_FOOT_FAR : AGENT_FOOT_NEAR;
}

/* Chuck in a pose a scene has built for him, with a face and his hands, on
   the film's legs. Hands back the view so a scene can place a mark over him. */
static ChuckView draw_agent_posed(SDL_Renderer *r, float x, float ground_y,
                                  int dir, ChuckPose *pose, float time,
                                  ChuckFace face, ChuckHand hands)
{
    chuck_pose_fit_legs(pose, AGENT_LEGS);
    chuck_pose_solve(pose);

    ChuckView view = agent_view(r, x, ground_y, AGENT_SCALE, dir, AGENT_W);
    fx_contact_shadow(r, chuck_view_x(&view, CHUCK_ROOT_X), ground_y - 2.0f,
                      12.0f * AGENT_SCALE, 0.0f, 190);
    chuck_draw_arm(&view, pose, CHUCK_FAR, hands);
    chuck_draw_legs(&view, pose);
    chuck_draw_torso(&view, pose);
    chuck_draw_head_as(&view, pose, fx_blinking(time, 0x1u), face);
    chuck_draw_arm(&view, pose, CHUCK_NEAR, hands);
    return view;
}

/*
 * Chuck with the pistol out: aiming along the line of his arm, lowered with
 * the muzzle at the ground ahead of him, or — once, on the roof — raised at
 * the sky. All three are the same body with the same two arms, which is the
 * point: the sky shot used to be the lowered pose with a second, three-times
 * longer arm ruled over the top of it, so he fired at the helicopter holding
 * a pistol in each hand. Solved hands cannot do that: an arm asked to reach
 * further than it is long stops at the end of itself.
 */
typedef enum
{
    AGENT_GUN_LOWERED,
    AGENT_GUN_LEVEL,
    AGENT_GUN_SKY
} AgentGunPose;

/* The braced stance, the hands on the gun, and where its muzzle is, in the
   skeleton's own units. The outro's tracers leave from that muzzle, so a round
   leaves the gun that is drawn rather than a point in the air beside it. */
static void agent_armed_pose(ChuckPose *pose, float time, AgentGunPose aim,
                             ChuckPoint *muzzle)
{
    chuck_pose_stand(pose, sinf(time * 4.5f));
    /* The far foot back and the near one forward, the weight a little down in
       the knees: a man set to take the kick of what he is holding. */
    pose->ankle[CHUCK_FAR].x = CHUCK_ROOT_X - 3.8f;
    pose->ankle[CHUCK_NEAR].x = CHUCK_ROOT_X + 3.3f;
    pose->lean = aim == AGENT_GUN_SKY ? -0.2f : 0.7f;
    chuck_pose_sink(pose, 0.6f);
    chuck_pose_fit_legs(pose, AGENT_LEGS);
    chuck_pose_solve(pose);

    ChuckPoint sh = pose->shoulder[CHUCK_NEAR];
    if (aim == AGENT_GUN_LEVEL)
    {
        ChuckPoint near = {sh.x + 8.6f, sh.y + 0.5f};
        chuck_pose_reach(pose, CHUCK_NEAR, near);
        chuck_pose_reach(pose, CHUCK_FAR,
                         (ChuckPoint){near.x - 1.1f, near.y + 1.3f});
        *muzzle = (ChuckPoint){near.x + 7.3f, near.y - 1.0f};
    }
    else if (aim == AGENT_GUN_SKY)
    {
        float dx = cosf(AGENT_SKY_ANGLE);
        float dy = -sinf(AGENT_SKY_ANGLE);
        ChuckPoint near = {sh.x + dx * 8.8f, sh.y + dy * 8.8f};
        chuck_pose_reach(pose, CHUCK_NEAR, near);
        chuck_pose_reach(pose, CHUCK_FAR,
                         (ChuckPoint){near.x - 0.9f, near.y + 1.3f});
        *muzzle = (ChuckPoint){near.x + dx * 7.0f, near.y + dy * 7.0f};
    }
    else
    {
        ChuckPoint near = {sh.x + 4.0f, sh.y + 7.9f};
        chuck_pose_reach(pose, CHUCK_NEAR, near);
        *muzzle = (ChuckPoint){near.x + 0.55f * 5.5f, near.y + 0.83f * 5.5f};
    }
}

/* A pistol laid from the hand toward the muzzle: the slide, the lit line along
   its top, and a grip dropped into the fist. */
static void agent_pistol(const ChuckView *view, ChuckPoint hand,
                         ChuckPoint muzzle)
{
    float dx = muzzle.x - hand.x;
    float dy = muzzle.y - hand.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f)
        return;
    float ux = dx / len;
    float uy = dy / len;
    /* The side of the slide the grip hangs from: down for a gun held level. */
    float nx = -uy;
    float ny = ux;
    ChuckPoint back = {hand.x - ux * 1.2f, hand.y - uy * 1.2f};
    ChuckPoint grip = {hand.x + nx * 1.8f + ux * 0.4f,
                       hand.y + ny * 1.8f + uy * 0.4f};

    chuck_view_band(view, back, muzzle, 3.4f, FX_INK);
    chuck_view_band(view, hand, grip, 3.0f, FX_INK);
    chuck_view_band(view, hand, grip, 1.6f, fx_dim(CAST_GUNMETAL, 0.8f));
    chuck_view_band(view, (ChuckPoint){back.x + ux * 0.5f, back.y + uy * 0.5f},
                    (ChuckPoint){muzzle.x - ux * 0.4f, muzzle.y - uy * 0.4f},
                    1.9f, CAST_GUNMETAL);
    chuck_view_band(view,
                    (ChuckPoint){back.x + ux * 0.5f - nx * 0.6f,
                                 back.y + uy * 0.5f - ny * 0.6f},
                    (ChuckPoint){muzzle.x - ux * 0.4f - nx * 0.6f,
                                 muzzle.y - uy * 0.4f - ny * 0.6f},
                    0.6f, fx_ramp(CAST_GUNMETAL).lit);
}

static void draw_agent_armed(SDL_Renderer *r, float x, float ground_y,
                             float scale, float time, AgentGunPose aim,
                             int dir, ChuckFace face)
{
    ChuckView view = agent_view(r, x, ground_y, scale, dir, AGENT_HELD_W);
    ChuckPose pose;
    ChuckPoint muzzle;

    agent_armed_pose(&pose, time, aim, &muzzle);
    fx_contact_shadow(r, chuck_view_x(&view, CHUCK_ROOT_X), ground_y - 2.0f,
                      14.0f * scale, 0.0f, 190);

    /* The support hand is on the far side of the gun: it comes up under it in
       both aimed poses and hangs behind the body in the lowered one — closed
       on nothing, in a man who is angry about it. */
    chuck_draw_arm(&view, &pose, CHUCK_FAR,
                   aim == AGENT_GUN_LOWERED && face != CHUCK_FACE_FURY
                       ? CHUCK_HAND_OPEN
                       : CHUCK_HAND_GRIP);
    chuck_draw_legs(&view, &pose);
    chuck_draw_torso(&view, &pose);
    chuck_draw_head_as(&view, &pose, fx_blinking(time, 0x1u), face);
    agent_pistol(&view, pose.hand[CHUCK_NEAR], muzzle);
    chuck_draw_arm(&view, &pose, CHUCK_NEAR, CHUCK_HAND_GRIP);
}

static void draw_agent_held_fire(SDL_Renderer *r, float x, float ground_y,
                                 float scale, float time, bool aiming, int dir)
{
    draw_agent_armed(r, x, ground_y, scale, time,
                     aiming ? AGENT_GUN_LEVEL : AGENT_GUN_LOWERED, dir,
                     CHUCK_FACE_EASY);
}

/* Where the muzzle of an armed pose is on screen, so a tracer can leave the
   pistol the figure is drawn holding. */
static void agent_muzzle_point(float x, float ground_y, float scale, int dir,
                               float time, AgentGunPose aim, float *sx,
                               float *sy)
{
    ChuckView view = agent_view(NULL, x, ground_y, scale, dir, AGENT_HELD_W);
    ChuckPose pose;
    ChuckPoint muzzle;

    agent_armed_pose(&pose, time, aim, &muzzle);
    *sx = chuck_view_x(&view, muzzle.x);
    *sy = chuck_view_y(&view, muzzle.y);
}

/* ---- The crew -------------------------------------------------------- */

/*
 * The crew, and the one man who is not part of it.
 *
 * Voss came in through the front door dressed like a client and gives orders
 * rather than carrying a rifle, so at twenty-six pixels across he is a long
 * pale coat, bare grey hair and a sidearm — three differences, all of them in
 * the silhouette, because that is the only channel a figure this size has.
 * Everything else is the same two-beat walk on the same clock.
 *
 * The crew's faces carry the red visor the sector's guards do, for the same
 * reason: it is the one mark that says "enemy" across a room. Their rifle is
 * held the way a rifle is, a hand on the grip and a hand on the handguard,
 * rather than balanced on one flat forearm.
 */
static const SDL_Color CREW_SKIN = {145, 103, 75, 255};
static const SDL_Color CREW_TROUSER = {33, 37, 36, 255};
static const SDL_Color CREW_BOOT = {18, 21, 22, 255};
static const SDL_Color VOSS_COAT = {102, 106, 108, 255};
static const SDL_Color VOSS_SKIN = {202, 166, 132, 255};

/* Voss above the legs and behind the arms: the long pale coat and the bare
 * grey head. Split out of `draw_terrorist` because the store's key art
 * poses him differently from the outro and he has to be the same man. */
static void draw_voss_coat_and_head(const CastFrame *f, float bob)
{
    /* The coat runs six rows further down than the crew's webbing rig, so
       the legs barely show: that alone reads as "not dressed for this".
       It is a man's width and not the rig's. The crew's seventeen units
       are a plate carrier, dark on the night and broken up by the rifle;
       the same block in a pale coat with nothing across it showed every
       unit and stood half again as wide as Chuck and Ellen beside him. */
    cast_body(f, 7.0f, 11.0f + bob, 13.0f, 15.0f, VOSS_COAT, 1, 0);
    cast_rect(f, 8.0f, 12.0f + bob, 3.0f, 13.0f,
              fx_mix(VOSS_COAT, FX_CREAM, 0.22f));
    cast_rect(f, 15.0f, 12.0f + bob, 1.0f, 13.0f,
              fx_mix(VOSS_COAT, FX_INK, 0.45f));
    cast_rect(f, 16.0f, 12.0f + bob, 3.0f, 3.0f,
              fx_mix(VOSS_COAT, FX_INK, 0.25f));
    cast_rect(f, 17.0f, 17.0f + bob, 1.0f, 1.0f, FX_INK);
    cast_rect(f, 17.0f, 21.0f + bob, 1.0f, 1.0f, FX_INK);
    /* A shirt collar under the coat: a client, not a soldier. */
    cast_rect(f, 13.0f, 11.0f + bob, 5.0f, 2.0f,
              (SDL_Color){206, 204, 188, 255});

    cast_body(f, 10.0f, 4.0f + bob, 8.0f, 7.0f, VOSS_SKIN, 0, 2);
    cast_mass(f, 9.0f, 1.0f + bob, 10.0f, 4.0f, FX_INK, 2, 0);
    cast_mass(f, 10.0f, 2.0f + bob, 8.0f, 3.0f,
              (SDL_Color){148, 150, 146, 255}, 1, 0);
    cast_rect(f, 10.0f, 5.0f + bob, 2.0f, 3.0f,
              (SDL_Color){148, 150, 146, 255});
    cast_mass(f, 10.0f, 9.0f + bob, 8.0f, 2.0f,
              fx_dim(VOSS_SKIN, 0.80f), 1, 2);
    cast_rect(f, 18.0f, 6.0f + bob, 1.5f, 4.0f, FX_INK);
    cast_rect(f, 17.0f, 7.0f + bob, 2.0f, 2.0f, VOSS_SKIN);
    cast_rect(f, 13.5f, 6.0f + bob, 4.0f, 0.8f,
              fx_dim(VOSS_SKIN, 0.62f));
    cast_rect(f, 14.4f, 7.2f + bob, 2.6f, 1.3f,
              (SDL_Color){176, 180, 168, 255});
    cast_rect(f, 15.7f, 7.2f + bob, 1.3f, 1.3f,
              (SDL_Color){38, 50, 60, 255});
    cast_rect(f, 13.0f, 10.0f + bob, 3.0f, 1.0f,
              fx_dim(VOSS_SKIN, 0.55f));
}

static void draw_terrorist(SDL_Renderer *r, float x, float ground_y,
                           float scale, float time, float phase, int dir,
                           bool leader)
{
    CastFrame f = {r, x, ground_y - 32.0f * scale, 28.0f, dir, scale};
    bool moving = time > 0.0f;
    float cycle = time * 1.43f + phase * 0.159f;
    float bob = moving ? fabsf(sinf(cycle * 6.2831853f)) * 0.6f : 0.0f;

    fx_contact_shadow(r, x + 14.0f * scale, ground_y - 2.0f,
                      12.0f * scale, 0.0f, 190);

    if (leader)
    {
        SDL_Color trouser = {46, 48, 52, 255};
        SDL_Color shoe = {28, 22, 20, 255};
        float swing = moving ? -cosf(cycle * 6.2831853f) : 0.0f;

        cast_legs(&f, moving, cycle, 2.4f, 0.7f, 11.5f, 15.0f, 21.5f + bob,
                  1.0f, fx_dim(trouser, 0.8f), trouser, shoe, shoe);
        /* The empty hand swings at his side behind the coat. */
        float rear_hx = 13.0f + swing * 1.6f;
        float rear_hy = 13.5f + bob + 8.8f;
        cast_arm(&f, 13.0f, 13.5f + bob, &rear_hx, &rear_hy,
                 fx_dim(VOSS_COAT, 0.72f), fx_dim(VOSS_COAT, 0.72f),
                 fx_dim(VOSS_SKIN, 0.75f), true);

        draw_voss_coat_and_head(&f, bob);

        /* Sidearm, held down at the thigh rather than shouldered. */
        float near_hx = 16.5f + swing * 0.4f;
        float near_hy = 13.5f + bob + 8.4f;
        cast_arm(&f, 14.5f, 13.5f + bob, &near_hx, &near_hy, VOSS_COAT,
                 VOSS_COAT, VOSS_SKIN, false);
        cast_pistol_angled(&f, near_hx, near_hy, 0.62f, 0.78f, 5.0f);
        cast_hand(&f, near_hx, near_hy, VOSS_SKIN);
        return;
    }

    cast_legs(&f, moving, cycle, 2.8f, 0.8f, 11.0f, 15.0f, 21.0f + bob, 1.0f,
              fx_dim(CREW_TROUSER, 0.8f), CREW_TROUSER, CREW_BOOT, CREW_BOOT);

    /* The support arm runs out along the far side of the rifle. */
    float rear_hx = 23.5f;
    float rear_hy = 15.4f + bob;
    cast_arm(&f, 14.5f, 13.5f + bob, &rear_hx, &rear_hy,
             (SDL_Color){20, 24, 23, 255}, (SDL_Color){20, 24, 23, 255},
             fx_dim(CREW_SKIN, 0.8f), false);

    cast_body(&f, 5.0f, 11.0f + bob, 17.0f, 12.0f,
              (SDL_Color){22, 27, 26, 255}, 1, 0);
    cast_body(&f, 7.0f, 12.0f + bob, 13.0f, 9.0f,
              (SDL_Color){49, 54, 49, 255}, 1, 0);
    /* Pouches on the plate carrier, above and below the band. */
    cast_rect(&f, 8.0f, 19.0f + bob, 3.0f, 2.0f, (SDL_Color){36, 41, 37, 255});
    cast_rect(&f, 12.0f, 19.0f + bob, 3.0f, 2.0f, (SDL_Color){36, 41, 37, 255});
    cast_rect(&f, 7.0f, 16.0f + bob, 13.0f, 3.0f, FX_RUST);
    cast_rect(&f, 7.0f, 16.0f + bob, 13.0f, 1.0f, fx_ramp(FX_RUST).lit);

    /* Face under a watch cap, the brim of it shading the brow, the red visor,
     * a nose that breaks the profile and a set mouth. */
    cast_body(&f, 10.0f, 4.0f + bob, 8.0f, 7.0f, CREW_SKIN, 0, 2);
    cast_mass(&f, 9.0f, 0.0f + bob, 10.0f, 6.0f, FX_INK, 3, 0);
    cast_mass(&f, 10.0f, 1.0f + bob, 8.0f, 4.5f, (SDL_Color){24, 28, 27, 255},
              2, 0);
    cast_rect(&f, 10.0f, 4.0f + bob, 8.0f, 1.0f, (SDL_Color){40, 45, 43, 255});
    cast_rect(&f, 11.0f, 1.0f + bob, 5.0f, 1.0f, (SDL_Color){46, 52, 50, 255});
    cast_rect(&f, 12.0f, 5.5f + bob, 6.0f, 1.0f, fx_dim(CREW_SKIN, 0.72f));
    cast_mass(&f, 10.0f, 9.0f + bob, 8.0f, 2.0f, fx_dim(CREW_SKIN, 0.78f), 1,
              2);
    cast_rect(&f, 18.0f, 6.0f + bob, 1.5f, 4.0f, FX_INK);
    cast_rect(&f, 17.0f, 7.0f + bob, 2.0f, 2.0f, CREW_SKIN);
    cast_rect(&f, 15.0f, 6.6f + bob, 2.8f, 1.4f, FX_RED);
    cast_rect(&f, 15.0f, 6.6f + bob, 2.8f, 0.6f,
              (SDL_Color){255, 138, 122, 255});
    cast_rect(&f, 13.5f, 9.5f + bob, 2.5f, 1.0f, (SDL_Color){70, 34, 27, 255});

    /* Low-ready rifle makes the captors unmistakable at pixel scale: stock
     * in the shoulder, magazine, handguard and a muzzle out in front. */
    cast_rect(&f, 13.5f, 13.0f + bob, 18.5f, 3.5f, FX_INK);
    cast_rect(&f, 20.0f, 15.5f + bob, 3.0f, 4.0f, FX_INK);
    cast_rect(&f, 30.5f, 13.5f + bob, 3.5f, 2.5f, FX_INK);
    cast_rect(&f, 14.5f, 13.5f + bob, 6.0f, 2.5f, (SDL_Color){38, 34, 30, 255});
    cast_rect(&f, 20.5f, 13.5f + bob, 10.5f, 2.0f,
              (SDL_Color){67, 73, 69, 255});
    cast_rect(&f, 20.5f, 13.5f + bob, 10.5f, 0.5f,
              fx_ramp((SDL_Color){67, 73, 69, 255}).lit);
    cast_rect(&f, 20.5f, 16.0f + bob, 2.0f, 3.0f, (SDL_Color){30, 33, 32, 255});
    cast_rect(&f, 31.0f, 14.0f + bob, 3.0f, 1.0f, (SDL_Color){56, 61, 58, 255});
    cast_hand(&f, rear_hx, rear_hy, fx_dim(CREW_SKIN, 0.8f));

    float near_hx = 19.0f;
    float near_hy = 16.8f + bob;
    cast_arm(&f, 13.5f, 13.5f + bob, &near_hx, &near_hy,
             (SDL_Color){30, 35, 33, 255}, (SDL_Color){30, 35, 33, 255},
             CREW_SKIN, true);
}

static void draw_hostage(SDL_Renderer *r, float x, float ground_y,
                         float scale, float time, int dir, bool tied)
{
    float step = sinf(time * 8.3f + 1.4f);
    float y = ground_y - 34.0f * scale;
    float dx_a, lift_a, dx_b, lift_b;
    walk_leg(time * 1.32f + 0.47f, 0.45f, &dx_a, &lift_a);
    walk_leg(time * 1.32f + 0.97f, 0.45f, &dx_b, &lift_b);
    float bob = (lift_a + lift_b) * 1.1f * scale;
    float hair_sway = step * 0.25f;
    SDL_Color hair_dark = {76, 51, 35, 255};
    SDL_Color hair = {137, 94, 55, 255};
    SDL_Color skin = {224, 171, 128, 255};
    SDL_Color coat_dark = {91, 28, 37, 255};
    SDL_Color coat = {148, 42, 49, 255};
    SDL_Color coat_light = {183, 57, 60, 255};
    SDL_Color trousers = {39, 48, 58, 255};
    SDL_Color boots = {15, 19, 25, 255};

    fx_contact_shadow(r, x + 13.0f * scale, ground_y - 2.0f,
                      10.0f * scale, 0.0f, 185);

    /* Practical trousers and flat boots, on the same restrained cycle. */
    sprite_rect(r, x, y - lift_a * scale, 26.0f, dir, scale,
                8 + dx_a, 23, 5, 11, FX_INK);
    sprite_rect(r, x, y - lift_a * scale, 26.0f, dir, scale,
                9 + dx_a, 24, 3, 8, trousers);
    sprite_rect(r, x, y - lift_a * scale, 26.0f, dir, scale,
                7 + dx_a, 31, 7, 3, boots);
    sprite_rect(r, x, y - lift_b * scale, 26.0f, dir, scale,
                14 + dx_b, 23, 5, 11, FX_INK);
    sprite_rect(r, x, y - lift_b * scale, 26.0f, dir, scale,
                15 + dx_b, 24, 3, 8, trousers);
    sprite_rect(r, x, y - lift_b * scale, 26.0f, dir, scale,
                13 + dx_b, 31, 7, 3, boots);

    /* Shoulder-length hair provides the main readable cue at pixel scale;
       it lies over a skull, so it keeps the skull's rounding. */
    mass_scaled(r, x, y + bob, 26.0f, dir, scale,
                7 + hair_sway, 1, 13, 15, FX_INK, 1, 1);
    mass_scaled(r, x, y + bob, 26.0f, dir, scale,
                8 + hair_sway, 2, 11, 13, hair_dark, 1, 1);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                7 - hair_sway, 7, 4, 9, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8 - hair_sway, 8, 2, 7, hair);

    /* Simple profile without makeup accents — but a profile, with a nose
     * that breaks the outline, an eye with a white behind the pupil, and a
     * jaw that turns into shade, the modelling every other face in the film
     * has. */
    body_scaled(r, x, y + bob, 26.0f, dir, scale,
                11, 3, 8, 9, skin, 2, 1);
    mass_scaled(r, x, y + bob, 26.0f, dir, scale,
                11, 10, 8, 2, fx_dim(skin, 0.84f), 1, 2);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                13, 4.2f, 6, 0.8f, fx_dim(skin, 0.86f));
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                18.8f, 5.8f, 1.4f, 2.2f, skin);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                20.1f, 5.4f, 1.0f, 2.8f, FX_INK);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8, 0, 12, 5, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                9, 1, 9, 2, hair);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                10, 1, 5, 0.8f, fx_ramp(hair).lit);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8, 3, 4, 8, hair);
    if (time <= 0.0f || !fx_blinking(time, 0x0eu))
    {
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    16.0f, 5.0f, 2.2f, 1.2f, (SDL_Color){196, 190, 178, 255});
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    17.3f, 5.0f, 1.1f, 1.2f, FX_INK);
    }
    else
    {
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    16.0f, 5.6f, 2.4f, 0.6f, (SDL_Color){110, 58, 40, 255});
    }
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                16.8f, 9.0f, 1.8f, 0.8f, (SDL_Color){150, 80, 70, 255});

    /* Straight-cut red coat keeps her silhouette distinct but grounded. */
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                12, 11, 5, 4, skin);
    body_scaled(r, x, y + bob, 26.0f, dir, scale,
                8, 13, 12, 13, coat, 1, 0);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                9, 14, 4, 10, coat_light);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                13, 14, 2, 12, coat_dark);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8, 24, 12, 3, coat_dark);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                11, 13, 6, 2, (SDL_Color){226, 222, 199, 255});

    if (tied)
    {
        /* Bent sleeves lead into small hands tied together in front of her. */
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    18, 14, 6, 5, FX_INK);
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    19, 15, 5, 3, coat);
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    22, 17, 5, 3, skin);
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    24, 16, 3, 4, skin);
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    24, 16, 2, 5, (SDL_Color){68, 75, 72, 255});
    }
    else
    {
        /*
         * Untied, both arms hang at her sides. The pair is the point rather
         * than the pose: the bound version above shows two hands taped
         * together in front, so a player who has seen her walked through a
         * lobby six times reads this drawing against that one, and a single
         * sleeve on the leading flank reads as a woman with one arm.
         *
         * **The two are one arm drawn twice**, mirrored about the middle of
         * the coat: same width, same length, hands on the same row. The first
         * version of the far arm was a pixel narrower and ended two rows
         * higher, on the argument that level hands merge into the hem — and at
         * 1.18 of a scale that difference is three screen pixels of sleeve,
         * which is not depth, it is one arm longer than the other. What keeps
         * the hands apart is the walk instead: each forearm swings with the
         * opposite leg, so they pass rather than sit level, and a figure handed
         * a frozen clock stands with both arms straight down. The far one stays
         * dimmed, because it is on the unlit side.
         */
        float swing = time > 0.0f
                          ? sinf((time * 1.32f + 0.47f) * 6.2831853f) * 1.4f
                          : 0.0f;
        for (int side = 0; side < 2; ++side)
        {
            bool front = side == 1;
            float shoulder_x = front ? 18.0f : 5.0f;
            float reach = front ? swing : -swing;
            SDL_Color sleeve = front ? coat : coat_dark;
            SDL_Color hand = front ? skin : fx_dim(skin, 0.72f);

            sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                        shoulder_x, 13, 5, 7, FX_INK);
            sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                        shoulder_x + reach, 19, 5, 8, FX_INK);
            sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                        shoulder_x + 1.0f, 14, 3, 6, sleeve);
            sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                        shoulder_x + 1.0f + reach, 19, 3, 5, sleeve);
            sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                        shoulder_x + 1.0f + reach, 24, 3, 2, hand);
        }
    }
}

static void draw_target_brackets(SDL_Renderer *r, float x, float y,
                                 float w, float h, float pulse)
{
    SDL_Color color = {(Uint8)(FX_RUST.r * (0.7f + pulse * 0.3f)),
                       (Uint8)(FX_RUST.g * (0.7f + pulse * 0.3f)),
                       (Uint8)(FX_RUST.b * (0.7f + pulse * 0.3f)), 255};
    set_color(r, color);

    SDL_RenderLine(r, x, y, x + 15.0f, y);
    SDL_RenderLine(r, x, y, x, y + 11.0f);
    SDL_RenderLine(r, x + w, y, x + w - 15.0f, y);
    SDL_RenderLine(r, x + w, y, x + w, y + 11.0f);
    SDL_RenderLine(r, x, y + h, x + 15.0f, y + h);
    SDL_RenderLine(r, x, y + h, x, y + h - 11.0f);
    SDL_RenderLine(r, x + w, y + h, x + w - 15.0f, y + h);
    SDL_RenderLine(r, x + w, y + h, x + w, y + h - 11.0f);
}

/*
 * A light the wet street owes a reflection to: where it is across the frame,
 * how wide the source is, and how much of it reaches the road.
 */
typedef struct
{
    float x;
    float w;
    SDL_Color c;
    float strength;
    float pool; /* half-width of the pool it lays on the pavement, or 0 */
    float top;  /* how high the light hangs: rain below it catches it */
} WetLight;

/* Where the pavement the cast stands on ends and the road begins. */
#define STREET_KERB_Y (437.0f + 5.0f)
#define STREET_ROAD_Y (437.0f + 11.0f)

/*
 * One light broken up in the wet road under it: a column of short dashes that
 * narrows and fades as it comes toward the camera, each dash wobbling a pixel
 * or so on its own slow clock, which is rain on standing water rather than a
 * strip of paint. The dash pattern is keyed to the light, never to where the
 * light is on screen, so a streak under a moving car travels with the car
 * instead of reshuffling itself every frame.
 */
static void draw_wet_streak(SDL_Renderer *r, float cx, float top, float bottom,
                            float w, SDL_Color c, float strength,
                            unsigned seed, float time)
{
    if (bottom <= top || strength <= 0.0f)
        return;

    /* The soft body of the reflection first: most of what the eye reads as
     * light in water is this glow, and the dashes are only its broken top. */
    fx_vgrad(r, cx - w * 0.30f, top, w * 0.60f, (bottom - top) * 0.80f,
             c, (Uint8)(44.0f * strength), c, 0);
    fx_vgrad(r, cx - w * 0.60f, top, w * 1.20f, (bottom - top) * 0.35f,
             c, (Uint8)(22.0f * strength), c, 0);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    float y = top + 1.0f;
    for (int seg = 0; y < bottom; ++seg)
    {
        unsigned h = scene_hash(seed * 131u + (unsigned)seg * 17u + 7u);
        float fall = (y - top) / (bottom - top);
        float width = w * (1.0f - fall * 0.5f) *
                      (0.25f + (float)(h % 16u) * 0.05f);
        float wobble = sinf(time * (1.1f + (float)(h % 7u) * 0.23f) +
                            (float)(h & 63u)) *
                       (1.0f + fall * 1.5f);
        float jitter = ((float)((h >> 4) % 9u) - 4.0f) * 0.5f * (0.4f + fall);
        float a = strength * (1.0f - fall * 0.8f) *
                  (float)(40u + (h >> 9) % 90u);
        set_rgba(r, c.r, c.g, c.b, (Uint8)fminf(a, 255.0f));
        fill_rect(r, floorf(cx - width * 0.5f + jitter + wobble), floorf(y),
                  fmaxf(1.0f, floorf(width)), 1.0f);
        y += 1.0f + (float)((h >> 12) % 3u) + fall * (float)((h >> 14) % 4u);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * A puddle lying on the asphalt: a flatter, darker sheet of water with the
 * city haze caught along its far lip, and the rings the rain keeps opening in
 * it. Each ring is born, spreads and fades on its own period, so the surface
 * is never still and never in step.
 */
static void draw_street_puddle(SDL_Renderer *r, float cx, float cy, float rx,
                               float ry, unsigned seed, float time)
{
    /* Standing water is smoother than the wet asphalt round it, so it holds
     * a truer image of the sky's haze: a step lighter, not a hole. */
    SDL_Color water = {30, 44, 54, 255};
    SDL_Color sheen = {70, 96, 108, 255};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int rows = (int)(ry * 2.0f) + 1;
    for (int i = 0; i < rows; ++i)
    {
        float t = ((float)i - ry) / ry;
        float half = rx * sqrtf(fmaxf(0.0f, 1.0f - t * t));
        if (half < 1.0f)
            continue;
        set_rgba(r, water.r, water.g, water.b, 110);
        fill_rect(r, floorf(cx - half), floorf(cy - ry) + (float)i,
                  floorf(half * 2.0f), 1.0f);
    }
    /* The far lip catches the sky: a thin bright row, strongest in the middle. */
    float lip = rx * 0.72f;
    set_rgba(r, sheen.r, sheen.g, sheen.b, 120);
    fill_rect(r, floorf(cx - lip), floorf(cy - ry), floorf(lip * 2.0f), 1.0f);
    set_rgba(r, sheen.r, sheen.g, sheen.b, 60);
    fill_rect(r, floorf(cx - lip * 0.6f), floorf(cy - ry) + 1.0f,
              floorf(lip * 1.2f), 1.0f);

    for (unsigned k = 0; k < 3u; ++k)
    {
        unsigned h = scene_hash(seed * 977u + k * 131u);
        float period = 0.85f + (float)(h % 50u) * 0.012f;
        float age = fmodf(time + (float)(h % 97u) * 0.01f * period, period) /
                    period;
        float px = cx + ((float)((h >> 8) % 100u) / 100.0f - 0.5f) * rx * 1.1f;
        float py = cy + ((float)((h >> 16) % 100u) / 100.0f - 0.5f) * ry * 0.8f;
        float rr = 1.0f + age * rx * 0.28f;
        float rv = fmaxf(1.0f, rr * ry / rx);
        Uint8 a = (Uint8)(110.0f * (1.0f - age));
        set_rgba(r, 132, 159, 170, a);
        fill_rect(r, floorf(px - rr * 0.7f), floorf(py - rv),
                  floorf(rr * 1.4f), 1.0f);
        fill_rect(r, floorf(px - rr * 0.7f), floorf(py + rv),
                  floorf(rr * 1.4f), 1.0f);
        fill_rect(r, floorf(px - rr), floorf(py), 1.0f, 1.0f);
        fill_rect(r, floorf(px + rr), floorf(py), 1.0f, 1.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * Rain landing: a pixel of spray thrown up where each drop hits, alive for a
 * tenth of a second and then somewhere else. Every splash keeps its own slot
 * and its own period, so there is never a frame where they all land at once.
 */
static void draw_rain_splashes(SDL_Renderer *r, float time, int win_w,
                               float top, float bottom, unsigned count)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (unsigned i = 0; i < count; ++i)
    {
        float period = 0.34f + (float)(i % 7u) * 0.05f;
        float local = time + (float)i * 0.173f;
        unsigned epoch = fx_salt(local / period);
        float age = fmodf(local, period) / period;
        if (age > 0.30f)
            continue;
        unsigned h = scene_hash(i * 7919u + epoch * 104729u);
        float x = (float)fx_spread(h, (float)win_w);
        float y = top + (float)fx_spread(h >> 10, bottom - top);
        Uint8 a = (Uint8)(150.0f * (1.0f - age / 0.30f));
        set_rgba(r, 150, 176, 186, a);
        fill_rect(r, floorf(x), floorf(y) - 1.0f, 1.0f, 1.0f);
        if (age > 0.08f)
        {
            fill_rect(r, floorf(x) - 2.0f, floorf(y), 1.0f, 1.0f);
            fill_rect(r, floorf(x) + 2.0f, floorf(y), 1.0f, 1.0f);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * The street both prologue scenes are played on, and it is wet: the story
 * says it is raining, so every light in the shot is asked for twice, once
 * where it hangs and once broken up in the road under it — the title screen's
 * rule for its own pavement. It used to be three flat bands and seven fixed
 * smears of colour that belonged to no light at all, which is what a road
 * looks like on a dry night in a picture that is raining on it.
 *
 * The pavement is paved and its front edge holds water; the kerb has a face
 * and a lit arris; the road runs from the sheen of the sky at the kerb to the
 * dark nearest the camera, with puddles lying in it and the rain landing on
 * all of it.
 */
static void render_street(SDL_Renderer *r, float time, int win_w, int win_h,
                          const WetLight *lights, int light_count)
{
    const float ground = 437.0f;
    const float w = (float)win_w;
    const float bottom = (float)win_h - 19.0f;

    /* Pavement: slabs, each a shade apart, lit along the edge the sky finds. */
    color_rect(r, (SDL_Color){52, 55, 54, 255}, 0.0f, ground - 3.0f, w, 8.0f);
    for (int i = 0; i * 44 < win_w + 44; ++i)
    {
        float jx = (float)(i * 44) - 13.0f;
        unsigned h = scene_hash((unsigned)i * 53u + 11u);
        float lift = (float)(h % 5u) * 0.012f;
        color_rect(r, fx_mix((SDL_Color){52, 55, 54, 255}, FX_PALE, lift),
                   jx + 1.0f, ground - 2.0f, 43.0f, 6.0f);
        color_rect(r, (SDL_Color){36, 40, 41, 255}, jx, ground - 3.0f, 1.0f,
                   8.0f);
    }
    color_rect(r, (SDL_Color){78, 84, 84, 255}, 0.0f, ground - 3.0f, w, 1.0f);
    /* The wet front edge of the pavement, where the water collects. */
    fx_vgrad(r, 0.0f, ground + 1.0f, w, 4.0f, (SDL_Color){24, 30, 34, 255}, 0,
             (SDL_Color){24, 30, 34, 255}, 170);

    /* The kerb: a lit arris, a face turned to the camera, the gutter below. */
    color_rect(r, (SDL_Color){72, 76, 73, 255}, 0.0f, STREET_KERB_Y, w, 1.0f);
    color_rect(r, (SDL_Color){34, 38, 39, 255}, 0.0f, STREET_KERB_Y + 1.0f, w,
               5.0f);
    for (int i = 0; i * 64 < win_w + 64; ++i)
        color_rect(r, (SDL_Color){22, 26, 28, 255}, (float)(i * 64) + 21.0f,
                   STREET_KERB_Y + 1.0f, 1.0f, 5.0f);

    /* The road: the sky's own sheen along the far side, falling away to the
     * dark in front of the lens. */
    color_rect(r, (SDL_Color){18, 24, 29, 255}, 0.0f, STREET_ROAD_Y, w,
               (float)win_h - STREET_ROAD_Y);
    fx_vgrad(r, 0.0f, STREET_ROAD_Y, w, bottom - STREET_ROAD_Y,
             (SDL_Color){34, 47, 57, 255}, 150, (SDL_Color){8, 11, 15, 255},
             140);
    color_rect(r, FX_NIGHT, 0.0f, STREET_ROAD_Y, w, 2.0f);
    /* Aggregate: a sparse scatter of lighter stones, keyed to the road. */
    for (unsigned i = 0; i < 140u; ++i)
    {
        unsigned h = scene_hash(i * 2654435761u + 3u);
        float y = STREET_ROAD_Y + 3.0f +
                  (float)fx_spread(h >> 12, bottom - STREET_ROAD_Y - 4.0f);
        float fall = (y - STREET_ROAD_Y) / (bottom - STREET_ROAD_Y);
        color_rect(r, fx_mix((SDL_Color){30, 40, 47, 255},
                             (SDL_Color){16, 21, 26, 255}, fall),
                   (float)fx_spread(h, w), y, 1.0f + (float)(h >> 30), 1.0f);
    }

    /* The painted edge line along the gutter, worn and holding the wet. */
    for (int i = 0; i * 24 < win_w + 24; ++i)
    {
        unsigned h = scene_hash((unsigned)i * 97u + 5u);
        float x = (float)(i * 24);
        color_rect(r, (h % 5u) == 0u ? (SDL_Color){58, 60, 56, 255}
                                     : (SDL_Color){80, 82, 76, 255},
                   x, ground + 13.0f, 24.0f - (float)(h % 3u), 2.0f);
    }
    color_rect(r, (SDL_Color){104, 108, 100, 255}, 0.0f, ground + 13.0f, w,
               1.0f);

    /* Puddles, where the camber lets the water stand. */
    draw_street_puddle(r, 146.0f, ground + 33.0f, 42.0f, 3.5f, 1u, time);
    draw_street_puddle(r, 452.0f, ground + 55.0f, 58.0f, 5.0f, 2u, time);
    draw_street_puddle(r, 716.0f, ground + 27.0f, 34.0f, 3.0f, 3u, time);
    draw_street_puddle(r, 318.0f, ground + 86.0f, 46.0f, 4.0f, 4u, time);

    /* Lane dashes, wet: a lit top row where they catch the sky. */
    for (int i = 0; i < 8; ++i)
    {
        float x = (float)(i * 128 - 42);
        color_rect(r, (SDL_Color){118, 107, 70, 255}, x, ground + 70.0f,
                   64.0f, 3.0f);
        color_rect(r, (SDL_Color){150, 139, 100, 255}, x + 2.0f,
                   ground + 70.0f, 60.0f, 1.0f);
        fx_vgrad(r, x + 6.0f, ground + 73.0f, 52.0f, 9.0f,
                 (SDL_Color){118, 107, 70, 255}, 30,
                 (SDL_Color){118, 107, 70, 255}, 0);
    }

    /* Every light in the scene, once more in the road. */
    for (int i = 0; i < light_count; ++i)
    {
        const WetLight *l = &lights[i];
        /* A pool on the wet pavement under a lamp or a lit window... */
        if (l->pool > 0.0f)
        {
            Uint8 a = (Uint8)(52.0f * l->strength);
            fx_hgrad(r, l->x - l->pool, ground - 3.0f, l->pool, 8.0f, l->c, 0,
                     l->c, a);
            fx_hgrad(r, l->x, ground - 3.0f, l->pool, 8.0f, l->c, a, l->c, 0);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, l->c.r, l->c.g, l->c.b, (Uint8)(46.0f * l->strength));
        fill_rect(r, floorf(l->x - l->w * 0.5f), ground - 3.0f,
                  floorf(l->w), 1.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        /* ...and the long broken column of it down the road. */
        draw_wet_streak(r, l->x, STREET_ROAD_Y, bottom - 4.0f,
                        fmaxf(6.0f, l->w * 0.7f), l->c, l->strength,
                        (unsigned)i + 17u, time);
    }

    draw_rain_splashes(r, time, win_w, ground - 2.0f, bottom - 6.0f, 22u);
}

/*
 * The rain in two planes — a fine far curtain, and fewer, longer, quicker
 * drops close to the lens — and it catches the light it falls through: a drop
 * inside a lamp's pool or in front of a lit window takes that light's colour
 * and brightens, which is most of what makes rain read as rain at night.
 */
static void render_rain(SDL_Renderer *r, float time, int win_w, int win_h,
                        const WetLight *lights, int light_count)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (unsigned i = 0; i < 96u; ++i)
    {
        bool near = i >= 72u;
        unsigned h = scene_hash(i * 17u + 5u);
        float speed = (near ? 260.0f : 128.0f) + (float)(h % 95u);
        float drift = near ? 52.0f : 20.0f + (float)(i % 4u) * 7.0f;
        float x = fmodf((float)fx_spread(h, (float)(win_w + 70)) -
                            time * drift,
                        (float)(win_w + 70));
        if (x < 0.0f)
            x += (float)(win_w + 70);
        x -= 25.0f;
        float y = fmodf((float)fx_spread(h >> 9, (float)(win_h + 40)) +
                            time * speed,
                        (float)(win_h + 40)) -
                  20.0f;

        SDL_Color c = {132, 159, 170, 255};
        float a = near ? (float)(46u + h % 40u) : (float)(30u + h % 35u);
        for (int l = 0; l < light_count; ++l)
        {
            const WetLight *lt = &lights[l];
            if (lt->pool <= 0.0f || y < lt->top)
                continue;
            float k = 1.0f - fabsf(x - lt->x) / lt->pool;
            if (k <= 0.0f)
                continue;
            k *= lt->strength;
            c = fx_mix(c, fx_ramp(lt->c).lit, k * 0.85f);
            a += k * 70.0f;
        }
        set_rgba(r, c.r, c.g, c.b, (Uint8)fminf(a, 200.0f));
        if (near)
            SDL_RenderLine(r, x, y, x - 6.0f, y + 15.0f);
        else
            SDL_RenderLine(r, x, y, x - 4.0f, y + 9.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* ---- The tower's front door ------------------------------------------- */

/*
 * Chuck outside the tower: out of his car the moment it stops, and running
 * while they walk her in.
 *
 * He used to sit at the wheel for three seconds after the car had parked,
 * his head in the window, while two men walked his wife across the pavement
 * in front of him and in through the doors; only once the doors had shut on
 * her did he get out and run for them. Everything that makes the kerb read as
 * a man who cannot get there in time is undone by a man who does not try. Now
 * his door goes as the car settles, he is at a run before they are halfway to
 * the glass, and the doors take her a second before he reaches them.
 *
 * Who is in front of what is the kerb's rule: the camera is across the road,
 * the parked cars are nearer to it than the pavement, and the doors the people
 * in a car use are the ones on this side. So everybody who arrives here is in
 * the road in front of the cars until they are round a bonnet: the two who
 * get out of the SUV walk her along its flank and round its nose to the doors,
 * and he gets out of his own driver's door and runs the same way after them.
 * Running along the pavement instead, behind the SUV, is just as possible and
 * was tried — it hid him for the second in which they reach the glass, which
 * is the second the shot is for.
 */
#define ARRIVAL_CHUCK_DOOR_X 155.0f  /* at his own driver's door */
#define ARRIVAL_CHUCK_LOBBY_X 658.0f /* at the lobby's glass */
#define ARRIVAL_CAR_DOOR_TIME 4.28f
#define ARRIVAL_CHUCK_OUT_TIME 4.40f
#define ARRIVAL_RUN_TIME 4.45f
#define ARRIVAL_AT_LOBBY_TIME 7.85f
#define ARRIVAL_CHUCK_IN_TIME 8.10f
/* The street held empty behind him under his name, then a fade of 1.15s that
   is black a tenth of a second before the beat hands over. */
#define ARRIVAL_FADE_TIME (OPENING_CUTSCENE_DURATION - 1.25f)
/* How long he takes to get up to his speed, and to pull up at the doors. The
   rest of the run is at the one speed, about the kerb's: an ease across the
   whole of it would spend its first and last second at a walk. */
#define ARRIVAL_RUN_RAMP 0.30f

/* Where he is on the run to the doors and how fast he is going (`pace`, 0..1
   of his top speed). False while he is not running. The drawing and the
   footsteps both read him off this. */
static bool arrival_chuck_run(float time, float *x, float *pace)
{
    if (time < ARRIVAL_RUN_TIME || time >= ARRIVAL_AT_LOBBY_TIME)
        return false;

    const float span = ARRIVAL_AT_LOBBY_TIME - ARRIVAL_RUN_TIME;
    const float ramp = ARRIVAL_RUN_RAMP;
    const float distance = ARRIVAL_CHUCK_LOBBY_X - ARRIVAL_CHUCK_DOOR_X;
    const float top = distance / (span - ramp);
    float t = time - ARRIVAL_RUN_TIME;
    float covered;
    if (t < ramp)
    {
        covered = 0.5f * top * t * t / ramp;
        *pace = t / ramp;
    }
    else if (t > span - ramp)
    {
        float left = span - t;
        covered = distance - 0.5f * top * left * left / ramp;
        *pace = left / ramp;
    }
    else
    {
        covered = top * (t - 0.5f * ramp);
        *pace = 1.0f;
    }
    *x = ARRIVAL_CHUCK_DOOR_X + covered;
    return true;
}

/* A footstep wherever one of his feet came down on the way to the doors, see
   `agent_footfall`. */
static Uint32 arrival_chuck_footfalls(float previous, float current)
{
    float x0, x1, pace0, pace1;
    if (!arrival_chuck_run(previous, &x0, &pace0) ||
        !arrival_chuck_run(current, &x1, &pace1))
        return 0;

    switch (agent_footfall(x0, x1, 1, pace1))
    {
    case AGENT_FOOT_NEAR:
        return OPENING_CUE_CHUCK_STEP_A;
    case AGENT_FOOT_FAR:
        return OPENING_CUE_CHUCK_STEP_B;
    case AGENT_FOOT_NONE:
        break;
    }
    return 0;
}

/* The kerb's run, with the kerb's face: it is the same man five minutes
   later, after the same people. */
static void draw_arrival_chuck(SDL_Renderer *r, float time, float ground)
{
    if (time < ARRIVAL_CHUCK_OUT_TIME || time >= ARRIVAL_CHUCK_IN_TIME)
        return;

    ChuckPose rest;
    ChuckPose pose;
    agent_pose_braced(&rest);
    float x = time < ARRIVAL_RUN_TIME ? ARRIVAL_CHUCK_DOOR_X
                                      : ARRIVAL_CHUCK_LOBBY_X;
    float pace = 0.0f;
    if (arrival_chuck_run(time, &x, &pace))
        agent_pose_running(&pose, &rest, x, 1, pace);
    else
        pose = rest;
    draw_agent_posed(r, x, ground, 1, &pose, time, CHUCK_FACE_FURY,
                     CHUCK_HAND_GRIP);
}

static void render_cinematic_ui(SDL_Renderer *r, float time,
                                int win_w, int win_h,
                                float target_x, const PadHints *pad)
{
    if (time > 0.55f && time < 4.5f)
    {
        float reveal = smoothstep01((time - 0.55f) / 0.5f);
        draw_text(r, 35.0f, 38.0f, 1.0f,
                  (SDL_Color){(Uint8)(FX_CYAN.r * reveal),
                              (Uint8)(FX_CYAN.g * reveal),
                              (Uint8)(FX_CYAN.b * reveal), 255},
                  "00:22 // FINANCIAL DISTRICT");
        color_rect(r, FX_RUST, 35.0f, 53.0f, 52.0f * reveal, 2.0f);
    }

    if (time > 3.0f && time < 6.5f)
    {
        /*
         * The caption is right-aligned to the bracket it labels, and that is a
         * fix rather than a taste.
         *
         * Drawn from `target_x + 3` it ran 200px to the right of a parked SUV
         * and straight into the building's own parapet nameplate, which
         * `render_tower` fixes at `win_w - 390 + 209` on very nearly the same
         * baseline — so the last three cells of this line and the first three
         * of `KESSLER TOWER` were printed one pixel apart, and the frame read
         * `SAME SUV // KESSLER TOWEKESSLER TOWER` for the whole 3.5s the
         * caption is up. Not a moment of it: the SUV has finished parking by
         * the time this beat starts, so the overlap was every run and every
         * seed, on the third screen of the game.
         *
         * Nothing in this tree could see it. Both strings are renderer
         * literals on the far side of the SDL boundary, the sweep drew the
         * frame every run so `make coverage` counted it, and `--shot` had
         * never been pointed at this beat — the press kit photographs the
         * kerb, not the arrival. It was read off a capture.
         *
         * Right-aligned rather than nudged, because a caption belongs to the
         * bracket under it: end the label where the bracket ends and the pair
         * travels together if the SUV's stop ever moves, which a hand-picked x
         * would not.
         */
        float pulse = 0.5f + 0.5f * sinf(time * 5.0f);
        const char *label = "SAME SUV // KESSLER TOWER";
        float bracket_x = target_x - 8.0f;
        float bracket_w = 167.0f;
        draw_target_brackets(r, bracket_x, 371.0f, bracket_w, 72.0f, pulse);
        draw_text(r, bracket_x + bracket_w - text_width(label, 1.0f),
                  351.0f, 1.0f, FX_RUST, label);
    }

    if (time > 7.1f && time < ARRIVAL_FADE_TIME - 0.15f)
    {
        float reveal = smoothstep01((time - 7.1f) / 0.35f);
        draw_text(r, 35.0f, 38.0f, 1.0f,
                  (SDL_Color){(Uint8)(FX_AMBER.r * reveal),
                              (Uint8)(FX_AMBER.g * reveal),
                              (Uint8)(FX_AMBER.b * reveal), 255},
                  "CHUCK ROSS // NOBODY ELSE IS COMING");
        color_rect(r, FX_RUST, 35.0f, 53.0f, 52.0f * reveal, 2.0f);
    }

    if (time > 0.9f && time < ARRIVAL_FADE_TIME)
    {
        float pulse = 0.45f + 0.55f * sinf(time * 2.0f);
        SDL_Color skip = {(Uint8)(100.0f + pulse * 42.0f),
                          (Uint8)(108.0f + pulse * 42.0f),
                          (Uint8)(106.0f + pulse * 38.0f), 255};
        char hint[32];
        draw_text(r, (float)win_w - 180.0f, (float)win_h - 31.0f,
                  1.0f, skip,
                  pad_hint(pad, hint, sizeof(hint),
                           PAD_CONFIRM_PAD " TO SKIP",
                           PAD_CONFIRM_KEYS " TO SKIP"));
    }
}

bool opening_cutscene_update(OpeningCutscene *cutscene, float dt,
                             Uint32 *out_cues)
{
    static const float escort_steps_a[] = {4.22f, 4.92f, 5.62f, 6.32f};
    static const float escort_steps_b[] = {4.57f, 5.27f, 5.97f, 6.67f};

    float previous = cutscene->time;
    float current = previous + dt;
    Uint32 cues = 0;

    if (crossed_time(previous, current, 0.05f))
        cues |= OPENING_CUE_RAIN;
    if (crossed_time(previous, current, 0.65f))
        cues |= OPENING_CUE_SUV_ENGINE;
    if (crossed_time(previous, current, 1.45f))
        cues |= OPENING_CUE_CAR_ENGINE;
    if (crossed_time(previous, current, 3.23f))
        cues |= OPENING_CUE_SUV_BRAKE;
    if (crossed_time(previous, current, 3.75f))
        cues |= OPENING_CUE_CAR_DOOR;
    if (crossed_time(previous, current, 3.92f))
        cues |= OPENING_CUE_CAR_BRAKE;
    if (crossed_any_time(previous, current, escort_steps_a,
                         (int)SDL_arraysize(escort_steps_a)))
        cues |= OPENING_CUE_ESCORT_STEP_A;
    if (crossed_any_time(previous, current, escort_steps_b,
                         (int)SDL_arraysize(escort_steps_b)))
        cues |= OPENING_CUE_ESCORT_STEP_B;
    /* His own door, open and then slammed behind him as he goes. */
    if (crossed_time(previous, current, ARRIVAL_CAR_DOOR_TIME) ||
        crossed_time(previous, current, ARRIVAL_RUN_TIME))
        cues |= OPENING_CUE_CAR_DOOR;
    cues |= arrival_chuck_footfalls(previous, current);
    if (crossed_time(previous, current, ARRIVAL_CHUCK_IN_TIME))
        cues |= OPENING_CUE_BUILDING_DOOR;

    cutscene->time = current;
    if (out_cues != NULL)
        *out_cues = cues;
    return cutscene->time >= OPENING_CUTSCENE_DURATION;
}

void opening_cutscene_render(SDL_Renderer *r,
                             const OpeningCutscene *cutscene,
                             int win_w, int win_h, const PadHints *pad)
{
    const float time = cutscene->time;
    const float ground = 437.0f;

    render_city(r, time, win_w, win_h);
    render_tower(r, time, win_w);

    /* The entrance canopy and the tower's lower lit windows, once more in
     * the road: the same question `render_tower` asked, asked again. */
    WetLight lights[24];
    int light_count = 0;
    const float tower_x = (float)win_w - 390.0f;
    lights[light_count++] = (WetLight){tower_x + 261.0f, 74.0f, FX_AMBER,
                                       0.95f, 90.0f, 362.0f};
    for (int row = 5; row < 9; ++row)
    {
        for (int col = 0; col < 7 && light_count < 24; ++col)
        {
            unsigned h;
            if (!tower_window_lit(row, col, time, &h))
                continue;
            lights[light_count++] = (WetLight){
                tower_x + 33.0f + (float)col * 45.0f, 20.0f,
                fx_ramp(tower_window_colour(h)).lit,
                0.22f + (float)(row - 5) * 0.08f, 0.0f, 0.0f};
        }
    }
    render_street(r, time, win_w, win_h, lights, light_count);

    float suv_move = ease_out_cubic((time - 0.65f) / 2.65f);
    float suv_x = lerpf(-175.0f, 438.0f, suv_move);
    bool suv_moving = time < 3.30f;
    bool suv_door_open = time >= 3.75f && time < 7.0f;

    float agent_car_move = ease_out_cubic((time - 1.45f) / 2.55f);
    float agent_car_x = lerpf(-170.0f, 92.0f, agent_car_move);
    bool agent_car_moving = time < 4.0f;
    bool agent_in_car = time < ARRIVAL_CHUCK_OUT_TIME;

    draw_suv(r, suv_x, ground, time, suv_moving, suv_moving,
             suv_door_open);
    draw_agent_car(r, agent_car_x, ground, time,
                   agent_car_moving, agent_in_car);

    if (time >= 4.05f && time < 7.18f)
    {
        float group_move = smoothstep01((time - 4.05f) / 2.95f);
        float escort_one_x = lerpf(500.0f, 643.0f, group_move);
        float hostage_x = lerpf(525.0f, 668.0f, group_move);
        float escort_two_x = lerpf(550.0f, 693.0f, group_move);
        draw_terrorist(r, escort_one_x, ground, 1.35f, time, 0.0f, 1, false);
        draw_hostage(r, hostage_x, ground, 1.18f, time, 1, true);
        draw_terrorist(r, escort_two_x, ground, 1.35f, time, 2.2f, 1, false);
    }

    /* In front of both cars, like the men he is after: see
       `draw_arrival_chuck`. */
    draw_arrival_chuck(r, time, ground);

    render_rain(r, time, win_w, win_h, lights, light_count);
    render_cinematic_ui(r, time, win_w, win_h, suv_x, pad);

    /* Film grain is the cutscene's own texture; the vignette and scanlines
       are laid on by game_render's one shared finishing pass. */
    fx_grain(r, win_w, win_h, time, FX_GRAIN_FILM);

    /* Narrow letterbox bars frame the scene without hiding the gameplay art. */
    color_rect(r, FX_INK, 0.0f, 0.0f, (float)win_w, 19.0f);
    color_rect(r, FX_INK, 0.0f, (float)win_h - 19.0f, (float)win_w, 19.0f);

    float fade_in = 1.0f - smoothstep01(time / 0.68f);
    float fade_out = smoothstep01((time - ARRIVAL_FADE_TIME) / 1.15f);
    float fade = fmaxf(fade_in, fade_out);
    if (fade > 0.0f)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, FX_INK.r, FX_INK.g, FX_INK.b, (Uint8)(fade * 255.0f));
        fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

/* ---- The kerb ---------------------------------------------------------- */

/*
 * The beat the campaign hangs off, and the only one the player is never asked
 * to do anything in.
 *
 * It is staged left to right so it hands straight over to the drive: Chuck's
 * car is parked at the far kerb, Ellen is twenty metres ahead of him on the
 * pavement, and the SUV comes up behind them both and leaves the way it was
 * already pointing — toward the tower on the skyline. When the scene ends the
 * world is in exactly the state `CHASE_PHASE_DEPARTURE` opens in, so the two
 * beats read as one continuous shot rather than as a cut.
 *
 * Every cue is a sound the game already synthesises. A beat this short does
 * not earn an entry in the effect table of its own.
 */

/* Where each actor stands, so the timings below and the staging cannot drift
 * apart. All of them are the sprite's left edge. */
#define KERB_GROUND 437.0f
#define KERB_CAR_X 10.0f
#define KERB_CHUCK_WAIT_X 190.0f
/* Chuck stops a clear seventy pixels short of the SUV's tail. Close enough to
   be in the same shot as the vehicle, far enough that the frame still reads as
   a distance he is not going to cover in time. */
#define KERB_CHUCK_CHASE_X 322.0f
#define KERB_CHUCK_RETURN_X 158.0f
#define KERB_ELLEN_FROM_X 372.0f
#define KERB_ELLEN_TAKEN_X 612.0f
#define KERB_SUV_STOP_X 404.0f
/* The SUV's near door, in the sprite's own coordinates: the crew get out of it
   and she goes back into it, so both journeys are measured from here. */
#define KERB_SUV_DOOR_X (KERB_SUV_STOP_X + 61.0f)

/* When things happen to him. The first three are the moments he reacts to —
   the brakes, the men getting out, her scream — and the scream is also the cue
   the sound and the dropped cup are timed to, so it is named once. */
#define KERB_NOTICE_TIME 3.66f
#define KERB_FRIGHT_TIME 4.10f
#define KERB_SCREAM_TIME 5.10f
#define KERB_RUN_OUT_TIME 5.60f
#define KERB_AIM_TIME 7.60f
#define KERB_LOWER_TIME 8.75f
/* He goes for the car the moment the SUV moves off, and he runs it: a hundred
   and sixty pixels in a second and a quarter. It used to take him two and
   three quarters from a standing start a third of a second later, which at the
   plain run's stride was two steps a second — a walk with the arms pumping. */
#define KERB_RUN_BACK_TIME 9.30f
#define KERB_AT_CAR_TIME 10.55f

static void draw_coffee_cup(SDL_Renderer *r, float time)
{
    /* Dropped at the moment of contact and left in frame for the rest of the
       scene. It is the whole of the struggle that survives the wide shot, so
       it gets a real arc: fall, one bounce, a short roll, and the spill
       spreading on the wet stone under it. */
    float age = time - KERB_SCREAM_TIME;
    if (age < 0.0f)
        return;

    const float start_x = KERB_ELLEN_TAKEN_X + 21.0f;
    const float start_y = KERB_GROUND - 46.0f;
    const float floor_y = KERB_GROUND - 5.0f;
    float x = start_x + age * 27.0f;
    float y = start_y + 470.0f * age * age;
    float bounce = age - 0.42f;

    if (y >= floor_y)
    {
        if (bounce > 0.0f && bounce < 0.30f)
            y = floor_y - sinf(bounce / 0.30f * 3.14159265f) * 9.0f;
        else
            y = floor_y;
    }
    if (age > 0.95f)
        x = start_x + 0.95f * 27.0f + (1.0f - expf(-(age - 0.95f) * 3.4f)) * 9.0f;

    if (age > 0.42f)
    {
        float spread = clamp01((age - 0.42f) / 1.6f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, 74, 56, 40, (Uint8)(150.0f * (1.0f - spread * 0.35f)));
        fill_rect(r, start_x + 9.0f - spread * 13.0f, floor_y + 3.0f,
                  4.0f + spread * 27.0f, 2.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    color_rect(r, FX_INK, x - 1.0f, y - 1.0f, 7.0f, 8.0f);
    color_rect(r, (SDL_Color){171, 166, 152, 255}, x, y, 5.0f, 6.0f);
    color_rect(r, (SDL_Color){118, 113, 102, 255}, x, y + 4.0f, 5.0f, 2.0f);
}

/*
 * The street this happens on, and Kessler Tower standing over the end of it.
 *
 * Two rules the shot is built to. The frame is a canyon, not a backdrop with
 * figures in front of it: the near terrace runs from a hundred and eighty
 * pixels down to the shopfronts, so there is no band of empty sky between the
 * skyline and the pavement for the eye to fall into. And the tower clears the
 * terrace by a hundred and thirty pixels, because it is the place the SUV
 * drives to and the player has to recognise it three minutes later from the
 * pavement outside its own doors — a landmark the same height as its
 * neighbours is not a landmark.
 */
static void render_kerb_backdrop(SDL_Renderer *r, float time, int win_w)
{
    static const float parapet[6] = {212.0f, 178.0f, 246.0f, 196.0f,
                                     264.0f, 224.0f};
    const float parade_top = 344.0f;

    /* Kessler Tower, five blocks up, behind everything else on the street.
     * It is recognisable from the pavement outside its own doors three
     * minutes later only if it reads as the same building here: the same
     * floors laid across it, the same grid of glass with most of it dark, the
     * same moonlit left arris and the beacon on the plant room. */
    color_rect(r, (SDL_Color){19, 27, 35, 255}, 594.0f, 52.0f, 136.0f, 294.0f);
    color_rect(r, (SDL_Color){36, 47, 54, 255}, 594.0f, 52.0f, 136.0f, 3.0f);
    color_rect(r, FX_NIGHT, 602.0f, 59.0f, 120.0f, 287.0f);
    color_rect(r, (SDL_Color){34, 45, 52, 255}, 594.0f, 55.0f, 2.0f, 291.0f);
    color_rect(r, (SDL_Color){19, 27, 35, 255}, 614.0f, 42.0f, 60.0f, 10.0f);
    color_rect(r, (SDL_Color){36, 47, 54, 255}, 614.0f, 42.0f, 60.0f, 1.0f);
    for (int row = 0; row < 13; ++row)
    {
        float fy = 68.0f + (float)row * 21.0f;
        color_rect(r, FX_SHADOW, 602.0f, fy + 11.0f, 120.0f,
                   2.0f);
        for (int col = 0; col < 5; ++col)
        {
            unsigned h = scene_hash((unsigned)(row * 53 + col * 11 + 401));
            float wx = 609.0f + (float)col * 22.0f;
            if ((h % 5u) != 0u)
            {
                color_rect(r, FX_INK, wx, fy, 12.0f,
                           6.0f);
                continue;
            }
            SDL_Color glass = (h & 8u) ? (SDL_Color){96, 89, 57, 255}
                                       : (SDL_Color){44, 78, 86, 255};
            color_rect(r, glass, wx, fy, 12.0f, 6.0f);
            color_rect(r, fx_ramp(glass).lit, wx, fy, 12.0f, 1.0f);
        }
    }
    color_rect(r, (SDL_Color){30, 38, 44, 255}, 654.0f, 40.0f, 5.0f, 12.0f);
    float beacon = sinf(time * 4.6f) > 0.2f ? 1.0f : 0.24f;
    color_rect(r, fx_dim(FX_RUST, beacon), 651.0f, 35.0f, 11.0f, 5.0f);
    fx_glow(r, 656.0f, 37.0f, 26.0f, FX_RUST, (Uint8)(40.0f * beacon));

    /* The near terrace: walk-ups over the shops, a step lighter than the sky
       and a step darker than the pavement they stand on. */
    for (int i = 0; i < 6; ++i)
    {
        float x = (float)i * 138.0f - 22.0f;
        float top = parapet[i];
        unsigned bh = scene_hash((unsigned)i * 7717u + 29u);
        /* What stands on the roof, keyed to the building: a chimney stack,
         * a water tank on its legs, or an aerial. */
        SDL_Color roof_kit = {16, 22, 30, 255};
        switch (bh % 3u)
        {
        case 0:
            color_rect(r, roof_kit, x + 24.0f + (float)(bh % 40u), top - 12.0f,
                       10.0f, 12.0f);
            color_rect(r, (SDL_Color){35, 43, 50, 255},
                       x + 23.0f + (float)(bh % 40u), top - 13.0f, 12.0f,
                       2.0f);
            color_rect(r, roof_kit, x + 90.0f, top - 7.0f, 6.0f, 7.0f);
            break;
        case 1:
            color_rect(r, roof_kit, x + 80.0f, top - 6.0f, 2.0f, 6.0f);
            color_rect(r, roof_kit, x + 94.0f, top - 6.0f, 2.0f, 6.0f);
            color_rect(r, roof_kit, x + 78.0f, top - 20.0f, 20.0f, 14.0f);
            color_rect(r, roof_kit, x + 80.0f, top - 23.0f, 16.0f, 3.0f);
            color_rect(r, (SDL_Color){30, 38, 45, 255}, x + 78.0f,
                       top - 20.0f, 1.0f, 14.0f);
            break;
        default:
            color_rect(r, roof_kit, x + 40.0f, top - 24.0f, 1.0f, 24.0f);
            color_rect(r, roof_kit, x + 34.0f, top - 18.0f, 13.0f, 1.0f);
            color_rect(r, roof_kit, x + 36.0f, top - 12.0f, 9.0f, 1.0f);
            break;
        }
        color_rect(r, (SDL_Color){16, 22, 30, 255}, x, top, 138.0f,
                   parade_top - top + 4.0f);
        color_rect(r, (SDL_Color){35, 43, 50, 255}, x, top, 138.0f, 3.0f);
        color_rect(r, (SDL_Color){10, 14, 20, 255}, x, top + 3.0f, 138.0f, 2.0f);
        /* The moon, up and to the left, on each building's left flank. */
        color_rect(r, FX_BASE, x, top + 3.0f, 1.0f,
                   parade_top - top - 3.0f);
        /* A cornice band halfway down: without one long horizontal the
           terrace is a row of blank slabs rather than masonry. */
        float band = top + (parade_top - top) * 0.52f;
        color_rect(r, (SDL_Color){26, 33, 40, 255}, x, band, 138.0f, 3.0f);
        for (int row = 0; row * 27 < (int)(parade_top - top) - 26; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                unsigned h = scene_hash((unsigned)(i * 311 + row * 29 + col * 7));
                float wx = x + 16.0f + (float)col * 30.0f;
                float wy = top + 14.0f + (float)row * 27.0f;
                if (wy > band - 4.0f && wy < band + 6.0f)
                    continue;
                /* A lintel over each window and a sill under it. */
                color_rect(r, (SDL_Color){24, 31, 38, 255}, wx - 1.0f,
                           wy - 2.0f, 17.0f, 2.0f);
                color_rect(r, (SDL_Color){30, 38, 45, 255}, wx - 1.0f,
                           wy + 18.0f, 17.0f, 1.0f);
                color_rect(r, (SDL_Color){8, 12, 18, 255}, wx, wy, 15.0f, 18.0f);
                if ((h % 7u) != 0u)
                {
                    /* Dark sash windows still show their glazing bar. */
                    color_rect(r, (SDL_Color){14, 19, 26, 255}, wx + 2.0f,
                               wy + 8.0f, 11.0f, 1.0f);
                    continue;
                }
                SDL_Color glass = (h & 8u) ? (SDL_Color){104, 91, 56, 255}
                                           : (SDL_Color){40, 72, 80, 255};
                color_rect(r, glass, wx + 2.0f, wy + 2.0f, 11.0f, 14.0f);
                color_rect(r, fx_mix(glass, FX_CREAM, 0.35f),
                           wx + 2.0f, wy + 2.0f, 11.0f, 1.0f);
                /* Curtains drawn to either side of some of them. */
                if ((h >> 4) % 2u == 0u)
                {
                    color_rect(r, fx_dim(glass, 0.6f), wx + 2.0f, wy + 2.0f,
                               3.0f, 14.0f);
                    color_rect(r, fx_dim(glass, 0.6f), wx + 10.0f, wy + 2.0f,
                               3.0f, 14.0f);
                }
                color_rect(r, fx_dim(glass, 0.45f), wx + 2.0f, wy + 8.0f,
                           11.0f, 1.0f);
            }
        }
    }

    /* Haze between the terrace and the street, so the near plane separates
       from the one behind it the way every other scene's does. */
    fx_vgrad(r, 0.0f, 250.0f, (float)win_w, 100.0f,
             (SDL_Color){30, 48, 60, 255}, 0,
             (SDL_Color){30, 48, 60, 255}, 44);

    /* The shopfronts under them. Their fascia is one unbroken line, which is
       what ties six separate buildings into one street. */
    color_rect(r, (SDL_Color){23, 29, 36, 255},
               0.0f, parade_top, (float)win_w, KERB_GROUND - parade_top);
    color_rect(r, (SDL_Color){52, 58, 60, 255},
               0.0f, parade_top, (float)win_w, 5.0f);
    color_rect(r, (SDL_Color){13, 18, 24, 255},
               0.0f, parade_top + 5.0f, (float)win_w, 3.0f);
    for (int i = 0; i < 9; ++i)
    {
        float x = (float)i * 92.0f + 6.0f;
        color_rect(r, (SDL_Color){15, 20, 26, 255},
                   x - 6.0f, parade_top + 8.0f, 6.0f, KERB_GROUND - parade_top);
        /* Shutters down on everything but the one place still open. */
        if (i == 2)
            continue;
        color_rect(r, (SDL_Color){29, 34, 38, 255},
                   x, 372.0f, 78.0f, 61.0f);
        for (float slat = 375.0f; slat < 430.0f; slat += 5.0f)
            color_rect(r, (SDL_Color){17, 21, 25, 255}, x, slat, 78.0f, 1.0f);
    }

    /* The window they stopped at: the one warm thing in the frame, and the
       reason she is carrying a cup at all. */
    const float shop_x = 190.0f;
    color_rect(r, (SDL_Color){14, 19, 24, 255}, shop_x, 358.0f, 86.0f, 12.0f);
    draw_text(r, shop_x + 11.0f, 360.0f, 1.0f,
              (SDL_Color){206, 158, 84, 255}, "COFFEE");
    color_rect(r, (SDL_Color){84, 68, 40, 255}, shop_x, 372.0f, 86.0f, 61.0f);
    color_rect(r, (SDL_Color){139, 111, 62, 255}, shop_x + 3.0f, 375.0f,
               80.0f, 44.0f);
    /* Inside: shelves on the back wall under three pendant lamps, and the
     * counter across the bottom of the glass with the machine on it. */
    fx_vgrad(r, shop_x + 3.0f, 375.0f, 80.0f, 44.0f,
             (SDL_Color){170, 138, 80, 255}, 110,
             (SDL_Color){96, 74, 42, 255}, 180);
    for (int i = 0; i < 2; ++i)
        color_rect(r, (SDL_Color){104, 82, 48, 255}, shop_x + 46.0f,
                   386.0f + (float)i * 8.0f, 34.0f, 1.0f);
    for (int i = 0; i < 3; ++i)
    {
        float px = shop_x + 16.0f + (float)i * 26.0f;
        color_rect(r, (SDL_Color){70, 56, 34, 255}, px, 375.0f, 1.0f, 5.0f);
        color_rect(r, FX_FLAME_HOT, px - 1.0f, 380.0f, 3.0f, 2.0f);
    }
    color_rect(r, (SDL_Color){66, 50, 30, 255}, shop_x + 3.0f, 408.0f, 80.0f,
               11.0f);
    color_rect(r, (SDL_Color){178, 144, 88, 255}, shop_x + 3.0f, 408.0f,
               80.0f, 1.0f);
    color_rect(r, (SDL_Color){58, 62, 60, 255}, shop_x + 58.0f, 400.0f, 12.0f,
               8.0f);
    color_rect(r, (SDL_Color){104, 110, 102, 255}, shop_x + 58.0f, 400.0f,
               12.0f, 1.0f);
    color_rect(r, (SDL_Color){38, 32, 24, 255}, shop_x + 40.0f, 375.0f,
               3.0f, 58.0f);
    /* The menu board stood in the left pane, chalk on black. */
    color_rect(r, (SDL_Color){28, 24, 20, 255}, shop_x + 9.0f, 396.0f,
               22.0f, 23.0f);
    for (int i = 0; i < 4; ++i)
        color_rect(r, (SDL_Color){92, 86, 74, 255}, shop_x + 12.0f,
                   400.0f + (float)i * 4.0f, 10.0f + (float)((i * 5) % 7),
                   1.0f);
    /* The glass: the street's own reflection across the pane on the
     * diagonal, and a frame round it. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_rgba(r, 236, 238, 224, 26);
    for (float i = 0.0f; i < 44.0f; i += 1.0f)
    {
        fill_rect(r, shop_x + 50.0f + (44.0f - i) * 0.6f, 375.0f + i, 5.0f,
                  1.0f);
        fill_rect(r, shop_x + 8.0f + (44.0f - i) * 0.6f, 375.0f + i, 3.0f,
                  1.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    color_rect(r, (SDL_Color){58, 46, 28, 255}, shop_x, 419.0f, 86.0f, 3.0f);
    fx_glow(r, shop_x + 43.0f, 400.0f, 96.0f,
            FX_WARM, 40);
    fx_light_cone(r, shop_x + 43.0f, 420.0f, 44.0f, 96.0f, 22.0f,
                  FX_WARM, 22);

    /* Two street lamps, so the pavement is lit in pools rather than evenly:
       the taking happens in the gap between them. */
    for (int i = 0; i < 2; ++i)
    {
        float lx = 118.0f + (float)i * 582.0f;
        color_rect(r, (SDL_Color){26, 32, 36, 255}, lx, 268.0f, 4.0f, 170.0f);
        color_rect(r, (SDL_Color){45, 52, 55, 255}, lx, 268.0f, 1.0f, 170.0f);
        color_rect(r, (SDL_Color){38, 44, 47, 255}, lx - 9.0f, 264.0f, 22.0f, 5.0f);
        color_rect(r, FX_SODIUM, lx - 6.0f, 269.0f, 16.0f, 3.0f);
        color_rect(r, fx_ramp(FX_SODIUM).lit, lx - 5.0f, 271.0f, 14.0f, 1.0f);
        fx_glow(r, lx + 2.0f, 271.0f, 46.0f, FX_SODIUM, 62);
        fx_light_cone(r, lx + 2.0f, 272.0f, 15.0f, 92.0f, 166.0f,
                      FX_SODIUM, 24);

    }
}

static void render_kerb_ui(SDL_Renderer *r, float time, int win_w, int win_h,
                           float suv_x, const PadHints *pad)
{
    if (time > 0.45f && time < 4.40f)
    {
        float reveal = smoothstep01((time - 0.45f) / 0.5f);
        draw_text(r, 35.0f, 38.0f, 1.0f, fx_dim(FX_CYAN, reveal),
                  "00:12 // THREE BLOCKS FROM KESSLER TOWER");
        color_rect(r, FX_RUST, 35.0f, 53.0f, 52.0f * reveal, 2.0f);
    }

    if (time > 5.40f && time < 8.05f)
    {
        float reveal = smoothstep01((time - 5.40f) / 0.28f);
        draw_text(r, 35.0f, 38.0f, 1.0f, fx_dim(FX_RUST, reveal),
                  "NO WORD // NO RANSOM // THEY CAME FOR HER");
        color_rect(r, FX_RUST, 35.0f, 53.0f, 142.0f * reveal, 2.0f);
    }

    if (time > 7.95f && time < 8.95f)
    {
        float pulse = 0.5f + 0.5f * sinf(time * 6.0f);
        draw_target_brackets(r, suv_x - 8.0f, 371.0f, 167.0f, 72.0f, pulse);
        draw_text(r, suv_x + 3.0f, 351.0f, 1.0f, FX_RUST,
                  "NO SHOT // SHE IS IN THE DOOR");
    }

    if (time > 9.60f && time < 12.90f)
    {
        float reveal = smoothstep01((time - 9.60f) / 0.3f);
        draw_text(r, 35.0f, 38.0f, 1.0f, fx_dim(FX_AMBER, reveal),
                  "CHUCK ROSS // FORTY SECONDS BEHIND");
        color_rect(r, FX_RUST, 35.0f, 53.0f, 52.0f * reveal, 2.0f);
    }

    if (time > 0.9f && time < 12.35f)
    {
        float pulse = 0.45f + 0.55f * sinf(time * 2.0f);
        SDL_Color skip = {(Uint8)(100.0f + pulse * 42.0f),
                          (Uint8)(108.0f + pulse * 42.0f),
                          (Uint8)(106.0f + pulse * 38.0f), 255};
        char hint[32];
        draw_text(r, (float)win_w - 180.0f, (float)win_h - 31.0f, 1.0f, skip,
                  pad_hint(pad, hint, sizeof(hint),
                           PAD_CONFIRM_PAD " TO SKIP",
                           PAD_CONFIRM_KEYS " TO SKIP"));
    }
}

/*
 * Chuck at the kerb, and what it does to him.
 *
 * He used to stand at the coffee window at ease through the whole of it — the
 * SUV braking level with her, two men with rifles getting out, her scream —
 * and then set off after them upright with his hands at his sides, which read
 * as a man who had seen nothing worth hurrying for. This is the one beat of the
 * campaign in which he is given nothing to do but watch, so what the moment
 * does to him has to be on him, in the order it would happen to anybody:
 *
 *   - the brakes: he comes up straight, the idle gone out of him;
 *   - the rifles: he starts back from them with his hands up and his mouth
 *     open, under the red mark the game warns with everywhere else;
 *   - her scream: the fright turns over into the other thing. He drops into
 *     his knees with his fists closed and the brow down, and goes.
 *
 * From there he runs like a man with a reason (`agent_hurry`) and the face
 * stays set, through the shot he cannot take and the run back to the car. Each
 * beat blends into the next over a fraction of a second, except the start at
 * the rifles, which is meant to be sudden.
 */

/* The brakes: stood up straight, the weight off the hip, the hands in. */
static void kerb_pose_alert(ChuckPose *pose)
{
    chuck_pose_stand(pose, 0.0f);
    pose->pelvis.y -= 0.3f;
    pose->lean = 0.0f;
    pose->arm_swing[CHUCK_FAR] = 0.08f;
    pose->arm_swing[CHUCK_NEAR] = 0.12f;
    pose->arm_bend[CHUCK_FAR] = 0.35f;
    pose->arm_bend[CHUCK_NEAR] = 0.35f;
}

/* The rifles: started back from them, the knees giving, both hands up in front
   of him. `settle` is how far the first jolt has eased into holding still. */
static void kerb_pose_fright(ChuckPose *pose, float settle)
{
    chuck_pose_stand(pose, 0.0f);
    chuck_pose_sink(pose, 0.5f);
    pose->pelvis.x -= 0.6f;
    pose->lean = -0.9f + 0.3f * settle;
    pose->arm_swing[CHUCK_NEAR] = 1.05f - 0.15f * settle;
    pose->arm_bend[CHUCK_NEAR] = 1.55f;
    pose->arm_swing[CHUCK_FAR] = 0.80f - 0.10f * settle;
    pose->arm_bend[CHUCK_FAR] = 1.65f;
}

/* Her scream: down into the knees to go, the fists closed, the weight thrown
   forward over the front foot. */
static void kerb_pose_wind_up(ChuckPose *pose)
{
    chuck_pose_stand(pose, 0.0f);
    chuck_pose_sink(pose, 0.9f);
    pose->pelvis.x += 0.4f;
    pose->lean = 1.5f;
    pose->arm_swing[CHUCK_NEAR] = -0.50f;
    pose->arm_bend[CHUCK_NEAR] = 1.60f;
    pose->arm_swing[CHUCK_FAR] = 0.55f;
    pose->arm_bend[CHUCK_FAR] = 1.50f;
}

/*
 * The two stretches he runs — out after them and back for the car — as where he
 * is, which way he faces and how fast he is going (`pace`, 0..1 of the ease).
 * False while he is on his feet in one place. The drawing and the footsteps
 * both read him off this, so a step is heard where a foot comes down.
 */
static bool kerb_chuck_run(float time, float *x, int *dir, float *pace)
{
    float from, to, start, end;
    if (time >= KERB_RUN_OUT_TIME && time < KERB_AIM_TIME)
    {
        from = KERB_CHUCK_WAIT_X;
        to = KERB_CHUCK_CHASE_X;
        start = KERB_RUN_OUT_TIME;
        end = KERB_AIM_TIME;
        *dir = 1;
    }
    else if (time >= KERB_RUN_BACK_TIME && time < KERB_AT_CAR_TIME)
    {
        from = KERB_CHUCK_CHASE_X;
        to = KERB_CHUCK_RETURN_X;
        start = KERB_RUN_BACK_TIME;
        end = KERB_AT_CAR_TIME;
        *dir = -1;
    }
    else
    {
        return false;
    }
    float u = (time - start) / (end - start);
    *x = lerpf(from, to, smoothstep01(u));
    *pace = smoothstep_pace(u);
    return true;
}

/* A footstep wherever one of his feet came down on the kerb, see
   `agent_footfall`. */
static Uint32 kerb_chuck_footfalls(float previous, float current)
{
    float x0, x1, pace0, pace1;
    int dir0, dir1;
    if (!kerb_chuck_run(previous, &x0, &dir0, &pace0) ||
        !kerb_chuck_run(current, &x1, &dir1, &pace1) || dir0 != dir1)
        return 0;

    switch (agent_footfall(x0, x1, dir1, pace1))
    {
    case AGENT_FOOT_NEAR:
        return ABDUCTION_CUE_STEP_A;
    case AGENT_FOOT_FAR:
        return ABDUCTION_CUE_STEP_B;
    case AGENT_FOOT_NONE:
        break;
    }
    return 0;
}

/* The game's own "!" over a head that has just seen something, in the red it
   warns with, popped up a few pixels and then held. `bottom` is where its dot
   ends. */
static void draw_alarm_mark(SDL_Renderer *r, float cx, float bottom,
                            float age)
{
    FxRamp ramp = fx_ramp(FX_RED);
    float rise = 3.0f * smoothstep01(age / 0.10f);
    float x = floorf(cx - 1.0f);
    float y = floorf(bottom - 11.0f - rise);

    color_rect(r, FX_INK, x - 1.0f, y - 1.0f, 5.0f, 8.0f);
    color_rect(r, FX_INK, x - 1.0f, y + 7.0f, 5.0f, 5.0f);
    color_rect(r, FX_RED, x, y, 3.0f, 6.0f);
    color_rect(r, ramp.lit, x, y, 3.0f, 1.0f);
    color_rect(r, FX_RED, x, y + 8.0f, 3.0f, 3.0f);
}

static void draw_kerb_chuck(SDL_Renderer *r, float time, float ground)
{
    if (time >= KERB_AIM_TIME && time < KERB_RUN_BACK_TIME)
    {
        draw_agent_armed(r, KERB_CHUCK_CHASE_X, ground, AGENT_SCALE, time,
                         time < KERB_LOWER_TIME ? AGENT_GUN_LEVEL
                                                : AGENT_GUN_LOWERED,
                         1, CHUCK_FACE_FURY);
        return;
    }

    ChuckPose pose;
    ChuckPose next;
    ChuckFace face = CHUCK_FACE_FURY;
    ChuckHand hands = CHUCK_HAND_GRIP;
    float x = KERB_CHUCK_WAIT_X;
    float pace = 0.0f;
    int dir = 1;

    if (kerb_chuck_run(time, &x, &dir, &pace))
    {
        if (dir > 0)
            kerb_pose_wind_up(&next);
        else
            agent_pose_braced(&next);
        agent_pose_running(&pose, &next, x, dir, pace);
    }
    else if (time < KERB_NOTICE_TIME)
    {
        chuck_pose_stand(&pose, sinf(time * 2.2f));
        face = CHUCK_FACE_EASY;
        hands = CHUCK_HAND_OPEN;
    }
    else if (time < KERB_FRIGHT_TIME)
    {
        chuck_pose_stand(&pose, sinf(time * 2.2f));
        kerb_pose_alert(&next);
        chuck_pose_blend(&pose, &next,
                         smoothstep01((time - KERB_NOTICE_TIME) / 0.15f));
        face = CHUCK_FACE_EASY;
        hands = CHUCK_HAND_OPEN;
    }
    else if (time < KERB_SCREAM_TIME)
    {
        kerb_pose_alert(&pose);
        kerb_pose_fright(
            &next, smoothstep01((time - KERB_FRIGHT_TIME - 0.25f) / 0.50f));
        chuck_pose_blend(&pose, &next,
                         smoothstep01((time - KERB_FRIGHT_TIME) / 0.08f));
        face = CHUCK_FACE_ALARM;
        hands = CHUCK_HAND_OPEN;
    }
    else if (time < KERB_RUN_OUT_TIME)
    {
        kerb_pose_fright(&pose, 1.0f);
        kerb_pose_wind_up(&next);
        chuck_pose_blend(&pose, &next,
                         smoothstep01((time - KERB_SCREAM_TIME) / 0.14f));
    }
    else
    {
        x = KERB_CHUCK_RETURN_X;
        dir = -1;
        agent_pose_braced(&pose);
    }
    ChuckView view = draw_agent_posed(r, x, ground, dir, &pose, time, face,
                                      hands);

    float mark_age = time - KERB_FRIGHT_TIME;
    if (mark_age >= 0.0f && mark_age < 0.75f)
    {
        ChuckPoint crown = chuck_head_crown(&pose);
        draw_alarm_mark(r, chuck_view_x(&view, crown.x),
                        chuck_view_y(&view, crown.y) - 3.0f, mark_age);
    }
}

void abduction_cutscene_init(AbductionCutscene *cutscene)
{
    SDL_zerop(cutscene);
}

bool abduction_cutscene_update(AbductionCutscene *cutscene, float dt,
                               Uint32 *out_cues)
{
    /* Ellen walking up; Chuck's two runs are read off his feet, see
       `kerb_chuck_footfalls`. */
    static const float ellen_steps_a[] = {0.95f, 1.63f, 2.31f, 2.99f, 3.67f};
    static const float ellen_steps_b[] = {1.29f, 1.97f, 2.65f, 3.33f, 4.01f};
    static const float door_times[] = {3.90f, 4.02f, 7.35f, 7.47f, 12.45f};

    float previous = cutscene->time;
    float current = previous + dt;
    Uint32 cues = 0;

    if (crossed_time(previous, current, 0.05f))
        cues |= ABDUCTION_CUE_RAIN;
    if (crossed_time(previous, current, 2.00f))
        cues |= ABDUCTION_CUE_SUV_ROLL;
    if (crossed_time(previous, current, 3.62f))
        cues |= ABDUCTION_CUE_SUV_BRAKE;
    if (crossed_any_time(previous, current, door_times,
                         (int)SDL_arraysize(door_times)))
        cues |= ABDUCTION_CUE_CAR_DOOR;
    if (crossed_time(previous, current, KERB_SCREAM_TIME))
        cues |= ABDUCTION_CUE_SCREAM;
    if (crossed_any_time(previous, current, ellen_steps_a,
                         (int)SDL_arraysize(ellen_steps_a)))
        cues |= ABDUCTION_CUE_STEP_A;
    if (crossed_any_time(previous, current, ellen_steps_b,
                         (int)SDL_arraysize(ellen_steps_b)))
        cues |= ABDUCTION_CUE_STEP_B;
    cues |= kerb_chuck_footfalls(previous, current);
    if (crossed_time(previous, current, 9.20f))
        cues |= ABDUCTION_CUE_SUV_AWAY;

    cutscene->time = current;
    if (out_cues != NULL)
        *out_cues = cues;
    return cutscene->time >= ABDUCTION_CUTSCENE_DURATION;
}

void abduction_cutscene_render(SDL_Renderer *r,
                               const AbductionCutscene *cutscene,
                               int win_w, int win_h, const PadHints *pad)
{
    const float time = cutscene->time;
    const float ground = KERB_GROUND;

    render_city(r, time, win_w, win_h);
    render_kerb_backdrop(r, time, win_w);

    /* The two sodium lamps and the one window still open, in the road. */
    const WetLight lights[] = {
        {120.0f, 20.0f, FX_SODIUM, 1.0f, 82.0f, 272.0f},
        {702.0f, 20.0f, FX_SODIUM, 1.0f, 82.0f, 272.0f},
        {233.0f, 76.0f, FX_WARM, 0.85f, 70.0f, 360.0f},
        {656.0f, 6.0f, FX_RUST, 0.35f, 0.0f, 0.0f},
    };
    render_street(r, time, win_w, win_h, lights,
                  (int)SDL_arraysize(lights));

    /* The SUV: dark up the kerb lane, hard on the brakes level with her, then
       away toward the tower with the lights finally on. */
    float suv_x;
    bool suv_moving;
    bool suv_lights;
    if (time < 2.00f)
    {
        suv_x = -230.0f;
        suv_moving = false;
        suv_lights = false;
    }
    else if (time < 3.70f)
    {
        suv_x = lerpf(-230.0f, KERB_SUV_STOP_X,
                      ease_out_cubic((time - 2.00f) / 1.70f));
        suv_moving = true;
        suv_lights = false;
    }
    else if (time < 9.20f)
    {
        suv_x = KERB_SUV_STOP_X;
        suv_moving = false;
        suv_lights = false;
    }
    else
    {
        float away = clamp01((time - 9.20f) / 1.85f);
        suv_x = lerpf(KERB_SUV_STOP_X, 980.0f, away * away);
        suv_moving = true;
        suv_lights = true;
    }
    bool suv_door_open = time >= 3.85f && time < 7.45f;

    /*
     * Ellen: up the pavement, a stop, and then walked back to the vehicle
     * between the two of them. She is never bound in this scene — the wrists
     * are taped in the SUV, which is why she arrives at the tower tied and
     * leaves this street not.
     */
    bool ellen_here = time < 7.35f;
    bool ellen_taken = time >= 5.20f;
    float ellen_x = KERB_ELLEN_TAKEN_X;
    int ellen_dir = 1;
    float ellen_clock = 0.0f;
    if (time < 4.50f)
    {
        float walk = clamp01((time - 0.70f) / 3.80f);
        ellen_x = lerpf(KERB_ELLEN_FROM_X, KERB_ELLEN_TAKEN_X, walk);
        ellen_clock = time;
    }
    else if (ellen_taken)
    {
        /* Walked back down the pavement to the door she came level with.
           She never reaches Chuck, and the two of them are never in the
           same frame facing each other — that is the scene. */
        float taken = smoothstep01((time - 5.20f) / 2.15f);
        ellen_x = lerpf(KERB_ELLEN_TAKEN_X, KERB_SUV_DOOR_X + 14.0f, taken);
        ellen_dir = -1;
        ellen_clock = time;
    }
    else
    {
        ellen_dir = -1;
    }

    /*
     * Who is in front of what. The camera is across the road, so the kerb lane
     * the two cars are in is nearer to it than the pavement, and everybody on
     * the pavement — Chuck at the coffee window, Ellen walking up it, the cup
     * she drops — is behind the cars. The SUV passes in front of him and stops
     * with her behind its bonnet. The two who get out use the door on this
     * side, the one that is drawn open, so they are in the road in front of it,
     * and so is she once they have her: she is changed over only while she is
     * standing clear of its nose, where the two layers look the same.
     *
     * Everything used to be drawn after both cars, which drove the SUV behind a
     * man standing at a shop window, stood her on its bonnet, and left the
     * dropped cup lying across its wing as it pulled away over it.
     */
    draw_coffee_cup(r, time);

    /*
     * Chuck: waiting at the coffee window, what he sees, out after them, the
     * shot he does not take, and the run back. Every one of those is a beat
     * the drive then inherits. See `draw_kerb_chuck`.
     */
    draw_kerb_chuck(r, time, ground);

    if (ellen_here && !ellen_taken)
        draw_hostage(r, ellen_x, ground, 1.18f, ellen_clock, ellen_dir, false);

    /* Chuck's car never moves in this scene. It is parked, locked and empty:
       he is out of it, which is the whole reason he cannot simply drive. */
    draw_agent_car(r, KERB_CAR_X, ground, time, false, false);
    draw_suv(r, suv_x, ground, time, suv_moving, suv_lights, suv_door_open);

    if (ellen_here && ellen_taken)
        draw_hostage(r, ellen_x, ground, 1.18f, ellen_clock, ellen_dir, false);

    /* The two who get out of it. They walk up to her, then walk her back. */
    if (time >= 4.05f && time < 7.35f)
    {
        float out = smoothstep01((time - 4.05f) / 1.25f);
        float back = smoothstep01((time - 5.20f) / 2.15f);
        float near_x = lerpf(KERB_SUV_DOOR_X + 4.0f,
                             KERB_ELLEN_TAKEN_X - 36.0f, out);
        float far_x = lerpf(KERB_SUV_DOOR_X + 42.0f,
                            KERB_ELLEN_TAKEN_X + 32.0f, out);
        if (time >= 5.20f)
        {
            near_x = lerpf(KERB_ELLEN_TAKEN_X - 36.0f,
                           KERB_SUV_DOOR_X - 22.0f, back);
            far_x = lerpf(KERB_ELLEN_TAKEN_X + 32.0f,
                          KERB_SUV_DOOR_X + 46.0f, back);
        }
        int crew_dir = time < 5.20f ? 1 : -1;
        draw_terrorist(r, near_x, ground, 1.32f, time, 0.0f, crew_dir, false);
        draw_terrorist(r, far_x, ground, 1.32f, time, 2.2f, crew_dir, false);
    }

    render_rain(r, time, win_w, win_h, lights, (int)SDL_arraysize(lights));
    render_kerb_ui(r, time, win_w, win_h, suv_x, pad);

    fx_grain(r, win_w, win_h, time, FX_GRAIN_FILM);

    color_rect(r, FX_INK, 0.0f, 0.0f, (float)win_w, 19.0f);
    color_rect(r, FX_INK, 0.0f, (float)win_h - 19.0f, (float)win_w, 19.0f);

    float fade_in = 1.0f - smoothstep01(time / 0.68f);
    float fade_out = smoothstep01((time - 12.55f) / 1.05f);
    float fade = fmaxf(fade_in, fade_out);
    if (fade > 0.0f)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, FX_INK.r, FX_INK.g, FX_INK.b, (Uint8)(fade * 255.0f));
        fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

void level_transition_init(LevelTransition *transition,
                           int completed_level, int next_level,
                           float elapsed_seconds, int level_score,
                           int hostiles_neutralized, int deaths,
                           int time_bonus, int clean_bonus,
                           float best_seconds, bool best_is_new,
                           int docket_sheets)
{
    SDL_zerop(transition);
    transition->completed_level = completed_level;
    transition->next_level = next_level;
    transition->elapsed_seconds = elapsed_seconds;
    transition->level_score = level_score;
    transition->hostiles_neutralized = hostiles_neutralized;
    transition->deaths = deaths;
    transition->best_seconds = best_seconds;
    transition->best_is_new = best_is_new;
    transition->time_bonus = time_bonus;
    transition->clean_bonus = clean_bonus;
    transition->docket_sheets = docket_sheets < 0 ? 0 : docket_sheets;
    /* Through the same function `sector_tally_set` and the RECORDS page read it
     * from, so the screens cannot come to disagree about how big the collection
     * is. One sheet to an interior, none on a climb.
     *
     * This used to spell the arithmetic out again, under a comment claiming
     * "exactly as `sector_tally_set` does it" — which is a sentence whose whole
     * content is that two copies agree, and this file has a page on what those
     * are worth. `campaign_docket_sheets` already existed; what was missing was
     * the call. */
    transition->docket_total = campaign_docket_sheets();
    if (transition->docket_sheets > transition->docket_total)
        transition->docket_sheets = transition->docket_total;
}

bool level_transition_update(LevelTransition *transition, float dt,
                             Uint32 *out_cues)
{
    static const float steps_a[] = {
        0.72f, 1.20f, 1.68f, 2.16f, 2.64f, 3.12f, 3.60f,
        4.08f, 4.56f, 4.04f, 4.52f, 5.00f, 5.48f, 5.96f,
        6.44f, 6.92f, 7.40f};
    static const float steps_b[] = {
        0.96f, 1.44f, 1.92f, 2.40f, 2.88f, 3.36f, 3.84f,
        4.32f, 4.28f, 4.76f, 5.24f, 5.72f, 6.20f, 6.68f,
        7.16f, 7.64f};

    float previous = transition->time;
    float current = previous + dt;
    Uint32 cues = 0;

    if (crossed_any_time(previous, current, steps_a,
                         (int)SDL_arraysize(steps_a)))
        cues |= LEVEL_TRANSITION_CUE_STEP_A;
    if (crossed_any_time(previous, current, steps_b,
                         (int)SDL_arraysize(steps_b)))
        cues |= LEVEL_TRANSITION_CUE_STEP_B;
    if (crossed_time(previous, current, 3.78f))
        cues |= LEVEL_TRANSITION_CUE_DOOR_OPEN;
    if (crossed_time(previous, current, 8.12f))
        cues |= LEVEL_TRANSITION_CUE_DOOR_CLOSE;

    transition->time = current;
    if (out_cues != NULL)
        *out_cues = cues;
    return transition->time >= LEVEL_TRANSITION_DURATION;
}

static void render_transition_report(SDL_Renderer *r,
                                     const LevelTransition *transition,
                                     int win_w)
{
    float time = transition->time;
    float reveal = smoothstep01((time - 0.12f) / 0.58f);
    float line_reveal = smoothstep01((time - 0.42f) / 0.55f);
    SDL_Color title = {
        (Uint8)(FX_CREAM.r * reveal),
        (Uint8)(FX_CREAM.g * reveal),
        (Uint8)(FX_CREAM.b * reveal), 255};
    SDL_Color muted = {
        (Uint8)(122.0f * line_reveal),
        (Uint8)(139.0f * line_reveal),
        (Uint8)(141.0f * line_reveal), 255};
    SDL_Color value = {
        (Uint8)(FX_AMBER.r * line_reveal),
        (Uint8)(FX_AMBER.g * line_reveal),
        (Uint8)(FX_AMBER.b * line_reveal), 255};
    char buffer[64];

    color_rect(r, (SDL_Color){7, 12, 18, 255},
               0.0f, 18.0f, (float)win_w, 133.0f);
    color_rect(r, (SDL_Color){25, 42, 49, 255},
               0.0f, 21.0f, (float)win_w, 3.0f);
    color_rect(r, (SDL_Color){34, 62, 65, 255},
               0.0f, 148.0f, (float)win_w, 3.0f);
    color_rect(r, (SDL_Color){
                      (Uint8)(FX_CYAN.r * reveal),
                      (Uint8)(FX_CYAN.g * reveal),
                      (Uint8)(FX_CYAN.b * reveal), 255},
               27.0f, 61.0f, 72.0f * reveal, 2.0f);

    draw_text(r, 27.0f, 34.0f, 2.0f, title, "ONE FLOOR BEHIND");

    /* Set from INTEL_TEXT_LEFT, and stopped by the divider at
     * INTEL_TEXT_RIGHT drawn below. Both come out of [intel.h](intel.h), which
     * is where the suite measures the table against them, so the line and the
     * column it has to live in cannot drift apart. */
    const char *intel = intel_line(transition->completed_level);
    if (intel != NULL)
    {
        color_rect(r, fx_dim(FX_RUST, line_reveal), 27.0f, 69.0f, 3.0f, 9.0f);
        draw_text(r, INTEL_TEXT_LEFT, 69.0f, 1.0f,
                  fx_dim((SDL_Color){158, 174, 178, 255}, line_reveal), intel);
    }

    /* What the two earned fields paid, printed under the field that earned it.
     * Dimmer than the value above, because it is the reason for a number
     * rather than the number: the eye should find the clock first and then the
     * credit beside it. See `campaign_award_sector_bonus`. */
    SDL_Color credit = fx_dim(FX_AMBER_DK, line_reveal);

    int elapsed = (int)transition->elapsed_seconds;
    int minutes = elapsed / 60;
    int seconds = elapsed % 60;
    draw_text(r, 27.0f, 88.0f, 1.0f, muted, "TIME");
    SDL_snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    draw_text(r, 27.0f, 106.0f, 1.0f, value, buffer);

    /*
     * And what there is to beat, in the same field rather than in one of its
     * own.
     *
     * The clock and the par bonus under it have been asking the player to go
     * fast since they existed, and giving them nothing to be fast against: 134
     * seconds is what the *night* allows, not what this player has
     * ever managed. A record next to the run is the smallest thing that turns
     * the stopwatch into something worth reading twice.
     *
     * A run that just set it shows its own time in the credit colour rather
     * than the record it beat, because printing the old number under the word
     * BEST on the very screen that replaced it is the field disagreeing with
     * itself. That also covers a first clear, which has nothing to compare
     * against and is a record by definition.
     */
    draw_text(r, 75.0f, 88.0f, 1.0f, muted, "BEST");
    float record = transition->best_is_new ? transition->elapsed_seconds
                                           : transition->best_seconds;
    if (record > 0.0f)
    {
        int best_total = (int)record;
        SDL_snprintf(buffer, sizeof(buffer), "%02d:%02d", best_total / 60,
                     best_total % 60);
        draw_text(r, 75.0f, 106.0f, 1.0f,
                  transition->best_is_new ? credit : muted, buffer);
    }
    else
    {
        /* Nothing banked: the authoring entry points bank nothing at all, and
         * a blank here would read as the field having failed rather than as
         * there being no record yet. */
        draw_text(r, 75.0f, 106.0f, 1.0f, muted, "--:--");
    }
    /* A floor that ran over its slot on the night clock is told so rather than
     * left with a blank line: nothing there reads as a field that sometimes
     * pays for no reason, and the words are what teach the par exists. */
    if (transition->time_bonus > 0)
    {
        SDL_snprintf(buffer, sizeof(buffer), "+%d", transition->time_bonus);
        draw_text(r, 27.0f, 122.0f, 1.0f, credit, buffer);
    }
    else
    {
        draw_text(r, 27.0f, 122.0f, 1.0f, muted, "OVER PAR");
    }

    color_rect(r, (SDL_Color){31, 47, 52, 255},
               129.0f, 85.0f, 1.0f, 43.0f);
    draw_text(r, 151.0f, 88.0f, 1.0f, muted, "SCORE");
    SDL_snprintf(buffer, sizeof(buffer), "+%06d", transition->level_score);
    draw_text(r, 151.0f, 106.0f, 1.0f, value, buffer);

    /* Under the score, in the credit colour, because a sheet is worth
     * `EVIDENCE_SCORE` and this is the row that says where points came from.
     * The whole cell is twelve cells of 8px from x=151, so it stops well short
     * of the divider at 282. */
    SDL_snprintf(buffer, sizeof(buffer), "DOCKET %02d/%02d",
                 transition->docket_sheets, transition->docket_total);
    draw_text(r, 151.0f, 122.0f, 1.0f, credit, buffer);

    color_rect(r, (SDL_Color){31, 47, 52, 255},
               282.0f, 85.0f, 1.0f, 43.0f);
    draw_text(r, 307.0f, 88.0f, 1.0f, muted, "HOSTILES");
    SDL_snprintf(buffer, sizeof(buffer), "%02d",
                 transition->hostiles_neutralized);
    draw_text(r, 307.0f, 106.0f, 1.0f, value, buffer);

    /* Deaths are reported neutrally — a number to beat next run, never a
     * grade. A clean sector earns the one word of praise. */
    color_rect(r, (SDL_Color){31, 47, 52, 255},
               412.0f, 85.0f, 1.0f, 43.0f);
    draw_text(r, 434.0f, 88.0f, 1.0f, muted, "DEATHS");
    if (transition->deaths == 0)
        SDL_snprintf(buffer, sizeof(buffer), "CLEAN");
    else
        SDL_snprintf(buffer, sizeof(buffer), "%02d", transition->deaths);
    draw_text(r, 434.0f, 106.0f, 1.0f, value, buffer);
    if (transition->clean_bonus > 0)
    {
        SDL_snprintf(buffer, sizeof(buffer), "+%d", transition->clean_bonus);
        draw_text(r, 434.0f, 122.0f, 1.0f, credit, buffer);
    }

    /* The divider the intel line above is measured against. It is the same
     * constant the test walks the table off, so moving this column moves what
     * a line is allowed to be. */
    color_rect(r, (SDL_Color){31, 47, 52, 255},
               INTEL_TEXT_RIGHT, 37.0f, 1.0f, 91.0f);
    draw_text(r, 558.0f, 45.0f, 1.0f, muted, "TRAIL LEADS TO");
    SDL_snprintf(buffer, sizeof(buffer), "SECTOR %02d",
                 transition->next_level + 1);
    draw_text(r, 558.0f, 69.0f, 2.0f, FX_CREAM, buffer);
}

static float transition_door_open(float time)
{
    float opening = smoothstep01((time - 3.72f) / 0.42f);
    float closing = smoothstep01((time - 8.10f) / 0.42f);
    return opening * (1.0f - closing);
}

/*
 * The service corridor the report plays over, and it is a place rather than a
 * gradient: exposed services along the ceiling, plaster between the concrete
 * pilasters, a darker painted wainscot under a dado rail, skirting, and a
 * sealed floor polished enough to hold every strip light twice. The lamps
 * wash the wall under them and lay a pool on the floor, so the light has
 * somewhere to land instead of fading out in mid-air.
 */
static void render_transition_corridor(SDL_Renderer *r, float time,
                                       int win_w, int win_h,
                                       float door_x, float ground_y,
                                       int next_sector)
{
    const float w = (float)win_w;
    const float dado = ground_y - 66.0f;
    const SDL_Color lamp_colour = {248, 205, 130, 255};

    color_rect(r, (SDL_Color){14, 22, 28, 255},
               0.0f, 151.0f, w, (float)win_h - 151.0f);
    fx_vgrad(r, 0.0f, 151.0f, w, (float)win_h - 151.0f,
             (SDL_Color){13, 20, 30, 255}, 255,
             (SDL_Color){24, 34, 44, 255}, 255);

    /* The ceiling void over the beam: a cable tray on its hangers and a
     * run of duct, both catching a line of light along their undersides
     * from the strip lamps below. */
    color_rect(r, FX_NIGHT, 0.0f, 151.0f, w, 28.0f);
    color_rect(r, (SDL_Color){24, 31, 35, 255}, 0.0f, 157.0f, w, 3.0f);
    color_rect(r, (SDL_Color){36, 44, 47, 255}, 0.0f, 157.0f, w, 1.0f);
    for (int i = 0; i * 51 < win_w + 51; ++i)
        color_rect(r, (SDL_Color){20, 26, 30, 255}, (float)(i * 51) + 9.0f,
                   151.0f, 1.0f, 9.0f);
    color_rect(r, (SDL_Color){21, 28, 33, 255}, 0.0f, 165.0f, w, 11.0f);
    color_rect(r, (SDL_Color){33, 41, 45, 255}, 0.0f, 165.0f, w, 1.0f);
    color_rect(r, FX_NIGHT, 0.0f, 175.0f, w, 1.0f);
    for (int i = 0; i * 67 < win_w + 67; ++i)
    {
        float sx = (float)(i * 67) + 30.0f;
        color_rect(r, (SDL_Color){14, 20, 24, 255}, sx, 165.0f, 1.0f, 11.0f);
        color_rect(r, (SDL_Color){30, 38, 42, 255}, sx + 1.0f, 165.0f, 1.0f,
                   11.0f);
    }

    /* Plaster between the pilasters, a painted wainscot under a dado rail,
     * and skirting along the floor. */
    for (int i = 0; i < 8; ++i)
    {
        float x = 16.0f + i * 102.0f;
        float mid = x + 58.0f;
        color_rect(r, FX_SHADOW, mid, 190.0f, 1.0f,
                   dado - 190.0f);
        color_rect(r, FX_BASE, mid + 1.0f, 190.0f, 1.0f,
                   dado - 190.0f);
    }
    color_rect(r, FX_SHADOW, 0.0f, dado, w,
               ground_y - 5.0f - dado);
    fx_vgrad(r, 0.0f, dado + 3.0f, w, 18.0f, FX_INK, 90, FX_INK, 0);
    color_rect(r, (SDL_Color){30, 39, 44, 255}, 0.0f, dado, w, 3.0f);
    color_rect(r, (SDL_Color){56, 66, 67, 255}, 0.0f, dado, w, 1.0f);
    color_rect(r, (SDL_Color){24, 31, 34, 255}, 0.0f, ground_y - 11.0f, w,
               6.0f);
    color_rect(r, (SDL_Color){44, 52, 53, 255}, 0.0f, ground_y - 11.0f, w,
               1.0f);

    /* Repeating concrete bays and pipes echo the tower interior in the intro. */
    for (int i = 0; i < 8; ++i)
    {
        float x = 16.0f + i * 102.0f;
        color_rect(r, (SDL_Color){32, 43, 47, 255},
                   x, 188.0f, 4.0f, ground_y - 188.0f);
        color_rect(r, (SDL_Color){52, 62, 64, 255},
                   x, 188.0f, 1.0f, ground_y - 188.0f);
        color_rect(r, (SDL_Color){10, 16, 21, 255},
                   x + 5.0f, 188.0f, 2.0f, ground_y - 188.0f);
        if ((i & 1) == 0)
        {
            /* A louvred return-air grille, set into every other bay. */
            color_rect(r, (SDL_Color){47, 55, 54, 255},
                       x + 17.0f, 212.0f, 55.0f, 5.0f);
            color_rect(r, (SDL_Color){66, 74, 71, 255},
                       x + 17.0f, 212.0f, 55.0f, 1.0f);
            color_rect(r, (SDL_Color){17, 26, 31, 255},
                       x + 21.0f, 218.0f, 47.0f, 31.0f);
            for (float slat = 220.0f; slat < 248.0f; slat += 4.0f)
            {
                color_rect(r, FX_NIGHT, x + 23.0f, slat,
                           43.0f, 2.0f);
                color_rect(r, (SDL_Color){30, 40, 45, 255}, x + 23.0f,
                           slat + 2.0f, 43.0f, 1.0f);
            }
        }
        else
        {
            /* A junction box and its conduit down to the rail. */
            color_rect(r, FX_INK, x + 34.0f, 262.0f, 16.0f, 20.0f);
            color_rect(r, (SDL_Color){38, 47, 50, 255}, x + 35.0f, 263.0f,
                       14.0f, 18.0f);
            color_rect(r, (SDL_Color){58, 68, 68, 255}, x + 35.0f, 263.0f,
                       14.0f, 1.0f);
            color_rect(r, (SDL_Color){26, 33, 36, 255}, x + 41.0f, 282.0f,
                       2.0f, dado - 282.0f);
            color_rect(r, (SDL_Color){46, 55, 57, 255}, x + 41.0f, 282.0f,
                       1.0f, dado - 282.0f);
        }
    }
    color_rect(r, (SDL_Color){53, 61, 59, 255},
               0.0f, 179.0f, w, 7.0f);
    color_rect(r, (SDL_Color){70, 78, 75, 255}, 0.0f, 179.0f, w, 1.0f);
    color_rect(r, (SDL_Color){20, 28, 31, 255},
               0.0f, 186.0f, w, 4.0f);

    /* The floor: its lit front edge and tile joints, then the polished
     * sheet running toward the lens. */
    color_rect(r, (SDL_Color){78, 83, 75, 255},
               0.0f, ground_y - 5.0f, w, 8.0f);
    color_rect(r, (SDL_Color){104, 108, 98, 255},
               0.0f, ground_y - 5.0f, w, 1.0f);
    for (int i = 0; i * 48 < win_w + 48; ++i)
        color_rect(r, (SDL_Color){58, 62, 56, 255}, (float)(i * 48) + 14.0f,
                   ground_y - 4.0f, 1.0f, 7.0f);
    color_rect(r, (SDL_Color){13, 18, 22, 255},
               0.0f, ground_y + 3.0f, w,
               (float)win_h - ground_y - 3.0f);
    fx_vgrad(r, 0.0f, ground_y + 3.0f, w, (float)win_h - ground_y - 21.0f,
             (SDL_Color){30, 38, 44, 255}, 140, FX_INK, 60);
    /* Joints across the floor, closing up with distance. */
    for (int i = 0; i < 5; ++i)
    {
        float jy = ground_y + 6.0f + (float)(i * i) * 2.4f + (float)i * 5.0f;
        color_rect(r, FX_NIGHT, 0.0f, floorf(jy), w,
                   1.0f);
    }

    for (int i = 0; i < 7; ++i)
    {
        float pulse = 0.55f + 0.45f *
                                  sinf(time * 2.1f + (float)i * 0.7f);
        color_rect(r, (SDL_Color){(Uint8)(120.0f * pulse),
                                  (Uint8)(104.0f * pulse),
                                  (Uint8)(58.0f * pulse), 255},
                   32.0f + i * 118.0f, ground_y + 21.0f, 57.0f, 2.0f);
        fx_glow(r, 60.0f + i * 118.0f, ground_y + 22.0f, 30.0f,
                (SDL_Color){222, 186, 104, 255}, (Uint8)(30.0f * pulse));
    }

    /* Overhead strip lights wash the corridor between the bays. */
    for (int i = 0; i < 4; ++i)
    {
        float lx = 96.0f + (float)i * 214.0f;
        float flicker = i == 2 && fmodf(time * 1.8f, 3.8f) < 0.07f ? 0.3f : 1.0f;
        SDL_Color lamp = fx_dim(lamp_colour, flicker);
        color_rect(r, (SDL_Color){16, 21, 31, 255}, lx - 14.0f, 186.0f, 28.0f, 4.0f);
        color_rect(r, lamp, lx - 11.0f, 188.0f, 22.0f, 2.0f);
        fx_glow(r, lx, 190.0f, 18.0f, lamp, (Uint8)(60.0f * flicker));
        fx_light_cone(r, lx, 189.0f, 13.0f, 52.0f, 120.0f,
                      lamp_colour,
                      (Uint8)(22.0f * flicker));
        /* Where the light lands: a pool on the wainscot and the floor... */
        fx_glow(r, lx, ground_y - 4.0f, 70.0f, lamp_colour,
                (Uint8)(26.0f * flicker));
        fx_vgrad(r, lx - 40.0f, ground_y - 5.0f, 80.0f, 2.0f, lamp_colour,
                 (Uint8)(60.0f * flicker), lamp_colour, 0);
        /* ...and the lamp itself, once more in the polished floor. */
        draw_wet_streak(r, lx, ground_y + 3.0f, (float)win_h - 22.0f, 26.0f,
                        lamp_colour, 0.55f * flicker, (unsigned)i + 91u,
                        time * 0.25f);
    }

    /* The dark aperture is drawn before the actors so they can walk into it. */
    color_rect(r, (SDL_Color){54, 61, 59, 255},
               door_x - 11.0f, TRANSITION_DOOR_TOP,
               112.0f, ground_y - TRANSITION_DOOR_TOP);
    color_rect(r, FX_INK,
               door_x, TRANSITION_DOOR_INNER_TOP,
               90.0f, ground_y - TRANSITION_DOOR_INNER_TOP);
    color_rect(r, (SDL_Color){12, 24, 29, 255},
               door_x + 8.0f, TRANSITION_DOOR_DEPTH_TOP,
               74.0f, ground_y - TRANSITION_DOOR_DEPTH_TOP);
    color_rect(r, (SDL_Color){42, 69, 70, 255},
               door_x + 43.0f, TRANSITION_DOOR_DEPTH_TOP,
               3.0f, ground_y - TRANSITION_DOOR_DEPTH_TOP);
    /* The car's own light, only seen with the doors apart. */
    fx_light_cone(r, door_x + 45.0f, TRANSITION_DOOR_DEPTH_TOP, 20.0f, 40.0f,
                  ground_y - TRANSITION_DOOR_DEPTH_TOP, FX_LAMP, 26);
    color_rect(r, fx_dim(FX_LAMP, 0.55f), door_x + 26.0f,
               TRANSITION_DOOR_DEPTH_TOP, 38.0f, 1.0f);
    color_rect(r, (SDL_Color){30, 44, 48, 255}, door_x + 8.0f, ground_y - 3.0f,
               74.0f, 3.0f);

    /*
     * Both sliding panels are part of the background layer. While boarding,
     * the actors therefore stay in front of every vertical part on the
     * elevator's left side.
     */
    float open = transition_door_open(time);
    float half_panel = 43.0f * (1.0f - open);
    if (half_panel > 0.0f)
    {
        float panel_h = ground_y - TRANSITION_DOOR_INNER_TOP;
        color_rect(r, (SDL_Color){43, 50, 49, 255},
                   door_x, TRANSITION_DOOR_INNER_TOP,
                   half_panel, panel_h);
        color_rect(r, (SDL_Color){62, 68, 63, 255},
                   door_x + 90.0f - half_panel, TRANSITION_DOOR_INNER_TOP,
                   half_panel, panel_h);
        /* Brushed steel: the strip lamps' sheen high on each leaf, the floor
         * dark reflected low, and a grain of fine vertical lines. */
        for (int leaf = 0; leaf < 2; ++leaf)
        {
            float lx = leaf == 0 ? door_x : door_x + 90.0f - half_panel;
            fx_vgrad(r, lx, TRANSITION_DOOR_INNER_TOP, half_panel,
                     panel_h * 0.5f, (SDL_Color){120, 124, 110, 255}, 40,
                     (SDL_Color){120, 124, 110, 255}, 0);
            fx_vgrad(r, lx, ground_y - panel_h * 0.4f, half_panel,
                     panel_h * 0.4f, FX_INK, 0, FX_INK, 90);
        }
        for (float gx = 5.0f; gx < half_panel - 3.0f; gx += 7.0f)
        {
            color_rect(r, (SDL_Color){37, 43, 42, 255}, door_x + gx,
                       TRANSITION_DOOR_INNER_TOP + 4.0f, 1.0f, panel_h - 8.0f);
            color_rect(r, (SDL_Color){56, 62, 58, 255},
                       door_x + 90.0f - gx - 1.0f,
                       TRANSITION_DOOR_INNER_TOP + 4.0f, 1.0f, panel_h - 8.0f);
        }
        color_rect(r, FX_RUST,
                   door_x + half_panel - 3.0f, TRANSITION_DOOR_INNER_TOP,
                   3.0f, ground_y - TRANSITION_DOOR_INNER_TOP);
        color_rect(r, FX_RUST,
                   door_x + 90.0f - half_panel, TRANSITION_DOOR_INNER_TOP,
                   3.0f, ground_y - TRANSITION_DOOR_INNER_TOP);
    }

    /*
     * The left jamb stays behind the actors so they remain visibly in front
     * of it while stepping into the elevator.
     */
    color_rect(r, (SDL_Color){91, 96, 87, 255},
               door_x - 11.0f, TRANSITION_DOOR_TOP,
               11.0f, ground_y - TRANSITION_DOOR_TOP);
    color_rect(r, (SDL_Color){27, 34, 35, 255},
               door_x - 7.0f, TRANSITION_DOOR_INNER_TOP,
               3.0f, ground_y - TRANSITION_DOOR_INNER_TOP);

    /*
     * The plate over the stair door, and it was the literal `"02"` for as long
     * as this screen existed.
     *
     * The report is shown after six sectors and the number in the top right of
     * the same frame is `next_level`, so on five of those six the door Chuck
     * walks through was labelled 02 under a caption reading SECTOR 05 — two
     * numbers about the same door, two hundred pixels apart, disagreeing. It
     * shipped, in an ordinary run, on every report but the first.
     *
     * Nothing in the tree could see it and the reason is the sweep: `--screen
     * report` staged the clear of sector one whatever `--level` said, and
     * sector one is the *one* sector this literal is right for. A frame drawn
     * by the gate on the only floor that agrees with it is a frame nobody has
     * a reason to read.
     */
    /* The plate is a floor indicator: a dark window in a steel bezel, the
     * number set in it, and the call panel on the wall beside the doors. */
    color_rect(r, (SDL_Color){58, 64, 60, 255}, door_x + 31.0f,
               TRANSITION_DOOR_TOP - 21.0f, 28.0f, 16.0f);
    color_rect(r, FX_INK, door_x + 33.0f,
               TRANSITION_DOOR_TOP - 19.0f, 24.0f, 12.0f);
    color_rect(r, (SDL_Color){84, 90, 84, 255}, door_x + 31.0f,
               TRANSITION_DOOR_TOP - 21.0f, 28.0f, 1.0f);
    color_rect(r, FX_INK, door_x - 27.0f, ground_y - 62.0f, 10.0f, 18.0f);
    color_rect(r, (SDL_Color){58, 64, 60, 255}, door_x - 26.0f,
               ground_y - 61.0f, 8.0f, 16.0f);
    color_rect(r, (SDL_Color){84, 90, 84, 255}, door_x - 26.0f,
               ground_y - 61.0f, 8.0f, 1.0f);
    color_rect(r, FX_INK, door_x - 24.0f, ground_y - 57.0f, 4.0f, 4.0f);
    color_rect(r, FX_INK, door_x - 24.0f, ground_y - 51.0f, 4.0f, 4.0f);
    color_rect(r, FX_AMBER, door_x - 23.0f, ground_y - 56.0f, 2.0f, 2.0f);
    color_rect(r, (SDL_Color){70, 76, 70, 255}, door_x - 23.0f,
               ground_y - 50.0f, 2.0f, 2.0f);
    fx_glow(r, door_x - 22.0f, ground_y - 55.0f, 7.0f, FX_AMBER, 70);

    char plate[8];
    SDL_snprintf(plate, sizeof(plate), "%02d", next_sector);
    draw_text(r, door_x + 37.0f, TRANSITION_DOOR_TOP - 17.0f, 1.0f,
              (SDL_Color){143, 151, 137, 255}, plate);
}

static void draw_transition_door_foreground(SDL_Renderer *r,
                                            float door_x, float ground_y)
{
    color_rect(r, (SDL_Color){91, 96, 87, 255},
               door_x + 90.0f, TRANSITION_DOOR_TOP,
               11.0f, ground_y - TRANSITION_DOOR_TOP);
    color_rect(r, (SDL_Color){27, 34, 35, 255},
               door_x + 94.0f, TRANSITION_DOOR_INNER_TOP,
               3.0f, ground_y - TRANSITION_DOOR_INNER_TOP);
    color_rect(r, (SDL_Color){104, 105, 92, 255},
               door_x - 11.0f, TRANSITION_DOOR_TOP,
               112.0f, 10.0f);

    color_rect(r, (SDL_Color){6, 10, 14, 255},
               door_x + 101.0f, TRANSITION_DOOR_TOP,
               25.0f, ground_y - TRANSITION_DOOR_TOP);
}

static void render_transition_action_ui(SDL_Renderer *r, float time,
                                        float hostage_x, float ground_y,
                                        int win_w, int win_h,
                                        const PadHints *pad)
{
    if (time >= 2.18f && time < 3.72f)
    {
        float reveal = smoothstep01((time - 2.18f) / 0.25f);
        float pulse = 0.5f + 0.5f * sinf(time * 6.0f);
        float target_x = hostage_x + 10.0f;
        draw_target_brackets(r, target_x - 10.0f,
                             ground_y - 55.0f, 52.0f, 61.0f, pulse);
        draw_text(r, 35.0f, 192.0f, 1.0f,
                  (SDL_Color){(Uint8)(FX_RUST.r * reveal),
                              (Uint8)(FX_RUST.g * reveal),
                              (Uint8)(FX_RUST.b * reveal), 255},
                  "SHE'S TOO CLOSE // KEEP MOVING");
        color_rect(r, FX_RUST, 35.0f, 207.0f, 142.0f * reveal, 2.0f);
    }

    if (time > 0.75f && time < 8.55f)
    {
        float pulse = 0.45f + 0.55f * sinf(time * 2.0f);
        char hint[32];
        draw_text(r, (float)win_w - 180.0f, (float)win_h - 31.0f,
                  1.0f,
                  (SDL_Color){(Uint8)(100.0f + pulse * 42.0f),
                              (Uint8)(108.0f + pulse * 42.0f),
                              (Uint8)(106.0f + pulse * 38.0f), 255},
                  pad_hint(pad, hint, sizeof(hint),
                           PAD_CONFIRM_PAD " TO SKIP",
                           PAD_CONFIRM_KEYS " TO SKIP"));
    }
}

void level_transition_render(SDL_Renderer *r,
                             const LevelTransition *transition,
                             int win_w, int win_h, const PadHints *pad)
{
    float time = transition->time;
    float ground_y = 482.0f;
    float door_x = (float)win_w - 126.0f;
    float group_progress = clamp01((time - 0.35f) / 4.60f);
    float group_x = lerpf(-80.0f, door_x + 112.0f, group_progress);
    float hostage_x = group_x + 35.0f;

    render_transition_report(r, transition, win_w);
    render_transition_corridor(r, time, win_w, win_h, door_x, ground_y,
                               transition->next_level + 1);

    if (time >= 0.35f && group_x < (float)win_w + 40.0f)
    {
        /*
         * The captors keep physical control of her as the group moves: the
         * man behind has his rifle on her back, the man in front leads her by
         * a cord from her taped wrists to his belt. These used to be two
         * skin-coloured lines drawn from the men's chins — a third arm each,
         * once their own two were drawn holding the rifle.
         */
        draw_terrorist(r, group_x, ground_y, 1.32f, time, 0.0f, 1, false);
        draw_hostage(r, hostage_x, ground_y, 1.18f, time, 1, true);
        float cord_x0 = hostage_x + 30.0f;
        float cord_y0 = ground_y - 20.0f;
        float cord_x1 = group_x + 76.0f;
        float cord_y1 = ground_y - 17.0f;
        float cord_mid = (cord_x0 + cord_x1) * 0.5f;
        set_color(r, (SDL_Color){72, 62, 48, 255});
        SDL_RenderLine(r, cord_x0, cord_y0, cord_mid, cord_y1 + 1.5f);
        SDL_RenderLine(r, cord_mid, cord_y1 + 1.5f, cord_x1, cord_y1);
        draw_terrorist(r, group_x + 68.0f, ground_y,
                       1.32f, time, 2.2f, 1, false);
    }

    if (time >= 0.95f && time < 2.20f)
    {
        float u = (time - 0.95f) / 1.25f;
        draw_agent(r, lerpf(-48.0f, 170.0f, smoothstep01(u)), ground_y,
                   AGENT_SCALE, time, 1, CHUCK_GAIT_PLAIN_RUN,
                   smoothstep_pace(u));
    }
    else if (time >= 2.20f && time < 3.48f)
    {
        draw_agent_held_fire(r, 170.0f, ground_y, AGENT_SCALE, time, true, 1);
    }
    else if (time >= 3.48f && time < 3.88f)
    {
        draw_agent_held_fire(r, 170.0f, ground_y, AGENT_SCALE, time, false, 1);
    }
    else if (time >= 3.88f && time < 8.10f)
    {
        float u = (time - 3.88f) / 4.10f;
        draw_agent(r, lerpf(170.0f, door_x + 112.0f, smoothstep01(u)),
                   ground_y, AGENT_SCALE, time, 1, CHUCK_GAIT_PLAIN_RUN,
                   smoothstep_pace(u));
    }

    draw_transition_door_foreground(r, door_x, ground_y);
    render_transition_action_ui(r, time, hostage_x, ground_y,
                                win_w, win_h, pad);

    fx_grain(r, win_w, win_h, time, FX_GRAIN_FILM);

    color_rect(r, FX_INK, 0.0f, 0.0f, (float)win_w, 18.0f);
    color_rect(r, FX_INK, 0.0f, (float)win_h - 18.0f, (float)win_w, 18.0f);

    float fade_in = 1.0f - smoothstep01(time / 0.48f);
    float fade_out = smoothstep01((time - 8.45f) / 0.85f);
    float fade = fmaxf(fade_in, fade_out);
    if (fade > 0.0f)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, FX_INK.r, FX_INK.g, FX_INK.b, (Uint8)(fade * 255.0f));
        fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

void outro_cutscene_init(OutroCutscene *cutscene)
{
    SDL_zerop(cutscene);
}

void outro_cutscene_update(OutroCutscene *cutscene, float dt,
                           Uint32 *out_cues)
{
    static const float step_a_times[] = {
        0.78f, 1.34f, 1.90f, 2.46f, 3.02f, 3.58f, 4.14f};
    static const float step_b_times[] = {
        1.06f, 1.62f, 2.18f, 2.74f, 3.30f, 3.86f, 4.42f};
    static const float shot_times[] = {9.95f, 14.55f, 15.45f};
    static const float down_times[] = {14.70f, 15.60f};

    float previous = cutscene->time;
    float current = previous + dt;
    Uint32 cues = 0;

    if (crossed_time(previous, current, 0.34f))
        cues |= OUTRO_CUE_DOOR;
    if (crossed_any_time(previous, current, step_a_times,
                         (int)SDL_arraysize(step_a_times)))
        cues |= OUTRO_CUE_STEP_A;
    if (crossed_any_time(previous, current, step_b_times,
                         (int)SDL_arraysize(step_b_times)))
        cues |= OUTRO_CUE_STEP_B;
    if (crossed_time(previous, current, 4.25f))
        cues |= OUTRO_CUE_HELICOPTER;
    if (crossed_any_time(previous, current, shot_times,
                         (int)SDL_arraysize(shot_times)))
        cues |= OUTRO_CUE_PLAYER_SHOT;
    if (crossed_any_time(previous, current, down_times,
                         (int)SDL_arraysize(down_times)))
        cues |= OUTRO_CUE_ENEMY_DOWN;
    if (crossed_time(previous, current, 13.35f))
        cues |= OUTRO_CUE_EXPLOSION;
    if (crossed_time(previous, current, 19.10f))
        cues |= OUTRO_CUE_WIN;

    if (current > OUTRO_CUTSCENE_DURATION)
        current = OUTRO_CUTSCENE_DURATION;
    cutscene->time = current;

    if (out_cues != NULL)
        *out_cues = cues;
}

static void draw_cutscene_text_centered(SDL_Renderer *r, float center_x,
                                        float y, float scale,
                                        SDL_Color color, const char *text)
{
    float width = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE *
                  scale * (float)SDL_strlen(text);
    draw_text(r, center_x - width * 0.5f, y, scale, color, text);
}

/*
 * The city behind the roof, in the two rows the title screen builds it from.
 *
 * It used to be one row of towers two or three units off the sky behind them,
 * so the frame read as an empty sky with lit windows scattered across it like
 * dirt — the failure docs/art-and-audio.md names for a view with nothing
 * separating it from the air, on the screen the whole campaign is played for.
 * What separates it is value: the haze the city throws up behind itself (the
 * title screen's own teal), a far row a step darker than that haze, and a near
 * row darker again, with the moon catching one flank and the roofline so every
 * tower has an edge that is not its windows. Floors are laid in as faint
 * spandrels, so a lit window sits in a building rather than in the sky.
 *
 * Everything is keyed to a tower's index and nothing to time, except the
 * aviation lights, which blink the way the title screen's do.
 */
static void render_outro_skyline(SDL_Renderer *r, float time, int win_w,
                                 float moon_x)
{
    const float base = 407.0f;
    SDL_Color haze = fx_mix(FX_STEEL_DK, FX_CYAN_DK, 0.20f);
    fx_vgrad(r, 0.0f, 206.0f, (float)win_w, base - 206.0f, haze, 0, haze, 76);

    /* Far row: hazy, a lit roofline, and a window or two at the size a
     * window is from this far off. */
    SDL_Color far_wall = fx_mix(FX_SHADOW, FX_BASE, 0.28f);
    SDL_Color far_edge = fx_mix(far_wall, FX_STEEL_DK, 0.55f);
    float cursor = -18.0f;
    for (unsigned i = 0; cursor < (float)win_w + 20.0f; ++i)
    {
        unsigned h = scene_hash(i * 7919u + 0x46415221u);
        float w = 28.0f + (float)(h % 34u);
        float height = 104.0f + (float)((h >> 8) % 92u);
        float top = base - height;
        color_rect(r, far_wall, cursor, top, w, height);
        color_rect(r, far_edge, cursor, top, w, 1.0f);
        for (unsigned k = 0; k < 6u; ++k)
        {
            unsigned wh = scene_hash(h + k * 131u);
            if ((wh & 3u) != 0u)
                continue;
            float wx = cursor + 3.0f + (float)fx_spread(wh >> 4, w - 6.0f);
            float floor_n = (float)fx_spread(wh >> 12, (height - 16.0f) / 6.0f);
            color_rect(r, (wh & 0x100u) ? fx_dim(FX_WARM, 0.34f)
                                        : fx_dim(FX_LAMP, 0.30f),
                       floorf(wx), top + 6.0f + floor_n * 6.0f, 2.0f, 1.0f);
        }
        if ((h >> 24) % 5u == 0u)
        {
            float ax = floorf(cursor + w * 0.5f);
            color_rect(r, far_edge, ax, top - 10.0f, 1.0f, 10.0f);
            float blink = sinf(time * 1.5f + (float)i * 2.3f) > 0.7f ? 1.0f : 0.2f;
            color_rect(r, fx_dim(FX_RED, 0.7f * blink), ax - 1.0f, top - 12.0f,
                       3.0f, 2.0f);
        }
        cursor += w + 1.0f + (float)((h >> 20) % 7u);
    }
    fx_vgrad(r, 0.0f, base - 110.0f, (float)win_w, 110.0f, haze, 0, haze, 60);

    /* Near row. */
    cursor = -30.0f;
    for (unsigned i = 0; cursor < (float)win_w + 30.0f; ++i)
    {
        unsigned h = scene_hash(i * 104729u + 0x4e454152u);
        float w = 52.0f + (float)(h % 34u);
        float height = 66.0f + (float)((h >> 7) % 116u);
        float top = base - height;
        SDL_Color wall = fx_mix(FX_NIGHT, FX_SHADOW, (i & 1u) ? 0.45f : 0.20f);
        SDL_Color spandrel = fx_mix(wall, FX_STEEL_DK, 0.20f);
        SDL_Color roofline = fx_mix(wall, FX_PALE, 0.26f);
        bool lit_left = cursor + w * 0.5f > moon_x;

        /* A setback crown on some, which is most of what makes a row of
         * rectangles read as a skyline. */
        if ((h & 0x10000u) != 0u)
        {
            float cw = floorf(w * 0.58f);
            float ch = 10.0f + (float)((h >> 18) % 14u);
            float cx = cursor + floorf((w - cw) * 0.5f);
            color_rect(r, wall, cx, top - ch, cw, ch);
            color_rect(r, roofline, cx, top - ch, cw, 1.0f);
            fx_rect_a(r, FX_PALE, 34, lit_left ? cx : cx + cw - 1.0f, top - ch,
                      1.0f, ch);
        }

        color_rect(r, wall, cursor, top, w, height);
        for (float fy = top + 7.0f; fy < base - 3.0f; fy += 9.0f)
            color_rect(r, spandrel, cursor + 2.0f, fy, w - 4.0f, 1.0f);
        int floors = (int)((base - top - 10.0f) / 9.0f);
        int bays = (int)((w - 6.0f) / 8.0f);
        for (int fl = 0; fl < floors; ++fl)
        {
            for (int bay = 0; bay < bays; ++bay)
            {
                unsigned wh = scene_hash(h + (unsigned)(fl * 61 + bay * 7));
                if ((wh % 9u) != 0u)
                    continue;
                SDL_Color light = (wh & 0x40u) ? fx_dim(FX_WARM, 0.50f)
                                               : fx_dim(FX_LAMP, 0.42f);
                float wx = cursor + 4.0f + (float)bay * 8.0f;
                float wy = top + 9.0f + (float)fl * 9.0f;
                color_rect(r, light, wx, wy, 5.0f, 4.0f);
                /* A blind half down in some, so they are not all one lamp. */
                if ((wh & 0x80u) != 0u)
                    color_rect(r, fx_dim(light, 0.55f), wx, wy, 5.0f, 2.0f);
            }
        }

        /* The flank the moon is on, brightest up where it clears the rest of
         * the city and fading into the haze, and the roofline over it. */
        fx_vgrad(r, lit_left ? cursor : cursor + w - 2.0f, top, 2.0f, height,
                 FX_PALE, 44, FX_PALE, 6);
        color_rect(r, roofline, cursor, top, w, 1.0f);
        color_rect(r, fx_dim(wall, 0.7f), cursor, top + 1.0f, w, 1.0f);

        if (h % 3u == 0u)
        {
            float ax = floorf(cursor + w * (0.3f + (float)((h >> 11) % 40u) * 0.01f));
            float mast = 12.0f + (float)((h >> 13) % 14u);
            color_rect(r, spandrel, ax, top - mast, 2.0f, mast);
            float blink = sinf(time * 1.8f + (float)i * 1.9f) > 0.75f ? 1.0f : 0.18f;
            SDL_Color beacon = fx_dim(FX_RED, 0.93f * blink);
            if (blink > 0.5f)
                fx_glow(r, ax + 1.0f, top - mast - 1.0f, 9.0f, beacon, 70);
            color_rect(r, beacon, ax - 1.0f, top - mast - 3.0f, 4.0f, 3.0f);
        }
        else if (h % 3u == 1u)
        {
            /* The water tank on its legs, in silhouette. */
            float tx = floorf(cursor + w * 0.62f);
            color_rect(r, wall, tx, top - 6.0f, 1.0f, 6.0f);
            color_rect(r, wall, tx + 11.0f, top - 6.0f, 1.0f, 6.0f);
            color_rect(r, wall, tx - 1.0f, top - 17.0f, 14.0f, 11.0f);
            color_rect(r, wall, tx + 1.0f, top - 20.0f, 10.0f, 3.0f);
            color_rect(r, roofline, tx + 1.0f, top - 20.0f, 10.0f, 1.0f);
            fx_rect_a(r, FX_PALE, 30, lit_left ? tx - 1.0f : tx + 12.0f,
                      top - 17.0f, 1.0f, 11.0f);
        }
        cursor += w + 6.0f + (float)((h >> 22) % 24u);
    }
}

static void render_outro_sky(SDL_Renderer *r, float time,
                             int win_w, int win_h)
{
    color_rect(r, FX_NIGHT, 0.0f, 0.0f, (float)win_w, (float)win_h);
    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, (float)win_h,
             (SDL_Color){7, 11, 20, 255}, 255,
             (SDL_Color){22, 32, 46, 255}, 255);

    /* The same quiet moon from the title screen, higher over the roof. */
    float moon_x = (float)win_w * 0.205f;
    float moon_y = 86.0f;
    fx_glow(r, moon_x, moon_y, 52.0f, (SDL_Color){196, 214, 224, 255}, 30);
    for (int row = -11; row <= 11; ++row)
    {
        float half = sqrtf(fmaxf(0.0f, 121.0f - (float)(row * row)));
        color_rect(r, (SDL_Color){204, 217, 224, 255},
                   moon_x - half, moon_y + (float)row, half * 2.0f, 1.0f);
    }
    color_rect(r, (SDL_Color){174, 190, 202, 255},
               moon_x - 4.0f, moon_y - 3.0f, 4.0f, 3.0f);
    color_rect(r, (SDL_Color){184, 200, 210, 255},
               moon_x + 2.0f, moon_y + 4.0f, 3.0f, 2.0f);

    for (unsigned i = 0; i < 92u; ++i)
    {
        unsigned h = scene_hash(i + 0x524f4f46u);
        float x = (float)fx_spread(h, (float)win_w);
        float y = 24.0f + (float)((h >> 9) % 245u);
        float glow = 0.45f + 0.55f *
                                 sinf(time * (0.42f + (float)(i % 5u) * 0.12f) +
                                      (float)(h & 63u));
        SDL_Color star = {(Uint8)(116.0f + glow * 67.0f),
                          (Uint8)(132.0f + glow * 61.0f),
                          (Uint8)(146.0f + glow * 62.0f), 255};
        color_rect(r, star, x, y, i % 13u == 0u ? 2.0f : 1.0f, 1.0f);
    }

    render_outro_skyline(r, time, win_w, moon_x);

    /* City haze between the skyline and the rooftop parapet. */
    fx_vgrad(r, 0.0f, 336.0f, (float)win_w, 72.0f,
             (SDL_Color){28, 44, 58, 255}, 0,
             (SDL_Color){28, 44, 58, 255}, 52);
}

/* The roof's own materials: a green-grey concrete for everything built, the
 * membrane it is all standing on, and the helipad's worn yellow — named once
 * here, the way level_art.c names a wall. */
static const SDL_Color COL_ROOF_CONCRETE = {74, 80, 78, 255};
static const SDL_Color COL_ROOF_BULKHEAD = {62, 67, 63, 255};
static const SDL_Color COL_ROOF_DECK = {30, 37, 40, 255};
static const SDL_Color COL_ROOF_PAINT = {105, 104, 81, 255};
static const SDL_Color COL_ROOF_DOORWAY = {7, 12, 16, 255};
static const SDL_Color COL_ROOF_SIGN = {169, 168, 145, 255};

/* A flat ellipse of standing water. It reflects the sky, so its far edge
 * carries the horizon's haze and its near edge the dark overhead, which is the
 * opposite of the deck around it — that inversion is what reads as wet. */
static void draw_roof_puddle(SDL_Renderer *r, float cx, float cy,
                             float rx, float ry, SDL_Color haze)
{
    int rows = (int)ry;
    for (int row = -rows; row <= rows; ++row)
    {
        float t = (float)row / ry;
        float half = floorf(rx * sqrtf(fmaxf(0.0f, 1.0f - t * t)) + 0.5f);
        if (half < 1.0f)
            continue;
        SDL_Color water = fx_mix(haze, fx_dim(COL_ROOF_DECK, 0.55f),
                                 (t + 1.0f) * 0.5f);
        color_rect(r, water, floorf(cx - half), floorf(cy + (float)row),
                   half * 2.0f, 1.0f);
    }
    fx_rect_a(r, FX_PALE, 70, floorf(cx - rx * 0.55f), floorf(cy - ry),
              floorf(rx * 0.7f), 1.0f);
}

static void render_rooftop(SDL_Renderer *r, float time, int win_w, int win_h)
{
    const float ground = 438.0f;
    const float moon_x = (float)win_w * 0.205f;
    const float deck_h = (float)win_h - ground;
    SDL_Color haze = fx_mix(FX_STEEL_DK, FX_CYAN_DK, 0.20f);

    /* The deck: lighter toward the parapet, where it catches the sky at a
     * grazing angle, darker underfoot. It rained on the drive, and a wet roof
     * holds the moon in a long smear straight down from it. */
    fx_vgrad(r, 0.0f, ground, (float)win_w, deck_h,
             fx_mix(COL_ROOF_DECK, FX_STEEL_DK, 0.40f), 255,
             fx_dim(COL_ROOF_DECK, 0.66f), 255);
    /* Broken into short horizontal strokes rather than laid as one column,
     * because a wet surface is not a mirror: every ripple and lap in the
     * membrane throws its own piece of the moon, and a smooth band reads as a
     * pane of something standing on the roof. */
    for (float sy = 2.0f; sy < deck_h * 0.8f; sy += 2.0f)
    {
        unsigned ripple = scene_hash(fx_salt(sy) * 2246822519u + 0x4d4f4f4eu);
        float fade = 1.0f - sy / (deck_h * 0.8f);
        float half = 3.0f + (float)(ripple % 12u) * (0.4f + fade * 0.6f);
        float wobble = (float)((int)((ripple >> 8) % 7u) - 3);
        fx_rect_a(r, FX_PALE, (Uint8)(10.0f + 34.0f * fade),
                  floorf(moon_x + wobble - half), ground + sy, floorf(half * 2.0f),
                  1.0f);
    }

    /* Membrane seams in perspective: lapped joints across the deck, closer
     * together the further off they are, and the strips between them running
     * back to one vanishing point over the middle of the city. */
    static const float seams[] = {6.0f, 18.0f, 36.0f, 61.0f, 95.0f};
    for (int i = 0; i < (int)SDL_arraysize(seams); ++i)
    {
        fx_rect_a(r, FX_INK, 64, 0.0f, ground + seams[i], (float)win_w, 1.0f);
        fx_rect_a(r, FX_PALE, 12, 0.0f, ground + seams[i] + 1.0f, (float)win_w,
                  1.0f);
    }
    const float vanish_x = (float)win_w * 0.5f;
    const float vanish_y = ground - 240.0f;
    float spread = ((float)win_h - vanish_y) / (ground - vanish_y);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, FX_INK.r, FX_INK.g, FX_INK.b, 46);
    for (int k = -12; k <= 12; ++k)
    {
        float at_parapet = vanish_x + (float)k * 54.0f;
        float underfoot = vanish_x + (float)k * 54.0f * spread;
        SDL_RenderLine(r, at_parapet, ground, underfoot, (float)win_h);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_roof_puddle(r, 486.0f, ground + 64.0f, 46.0f, 4.0f, haze);
    draw_roof_puddle(r, 292.0f, ground + 92.0f, 34.0f, 5.0f, haze);
    draw_roof_puddle(r, 742.0f, ground + 34.0f, 30.0f, 3.0f, haze);

    /* The helipad, painted on the deck and so foreshortened with it: an
     * ellipse and an H laid down flat, worn through in places. It used to be a
     * circle drawn face-on, which put the top of its ring above the parapet,
     * hanging in the air over the city. */
    const float pad_x = 165.0f;
    const float pad_y = ground + 44.0f;
    for (int step = 0; step < 120; ++step)
    {
        if (scene_hash((unsigned)step * 2246822519u + 0x50414400u) % 6u == 0u)
            continue;
        float angle = (float)step / 120.0f * 6.2831853f;
        float px = pad_x + cosf(angle) * 72.0f;
        float py = pad_y + sinf(angle) * 16.0f;
        fx_rect_a(r, COL_ROOF_PAINT, 170, floorf(px - 1.5f), floorf(py), 3.0f,
                  2.0f);
    }
    SDL_Color h_paint = fx_dim(COL_ROOF_PAINT, 0.87f);
    fx_rect_a(r, h_paint, 190, pad_x - 26.0f, pad_y - 11.0f, 8.0f, 22.0f);
    fx_rect_a(r, h_paint, 190, pad_x + 18.0f, pad_y - 11.0f, 8.0f, 22.0f);
    fx_rect_a(r, h_paint, 190, pad_x - 18.0f, pad_y - 2.0f, 36.0f, 4.0f);
    for (unsigned i = 0; i < 40u; ++i)
    {
        unsigned wear = scene_hash(i * 40503u + 0x48u);
        float wx = pad_x - 26.0f + (float)fx_spread(wear, 52.0f);
        float wy = pad_y - 11.0f + (float)fx_spread(wear >> 10, 22.0f);
        fx_rect_a(r, COL_ROOF_DECK, 150, wx, wy, 2.0f, 1.0f);
    }

    /* The far parapet: a cap that takes the moon along its top, a face in
     * shade jointed every few feet and streaked where the rain runs off, and
     * the shadow it lays on the deck at its foot. */
    SDL_Color face = fx_dim(COL_ROOF_CONCRETE, 0.62f);
    color_rect(r, face, 0.0f, ground - 10.0f, (float)win_w, 10.0f);
    color_rect(r, COL_ROOF_CONCRETE, 0.0f, ground - 10.0f, (float)win_w, 3.0f);
    color_rect(r, fx_mix(COL_ROOF_CONCRETE, FX_PALE, 0.30f), 0.0f,
               ground - 10.0f, (float)win_w, 1.0f);
    color_rect(r, fx_dim(COL_ROOF_CONCRETE, 0.45f), 0.0f, ground - 7.0f,
               (float)win_w, 1.0f);
    for (float jx = 17.0f; jx < (float)win_w; jx += 34.0f)
    {
        color_rect(r, fx_dim(COL_ROOF_CONCRETE, 0.45f), jx, ground - 7.0f, 1.0f,
                   7.0f);
        unsigned streak = scene_hash(fx_salt(jx) * 2654435761u);
        if ((streak & 1u) != 0u)
            fx_vgrad(r, jx + 4.0f + (float)(streak % 20u), ground - 6.0f, 2.0f,
                     6.0f, FX_INK, 70, FX_INK, 0);
    }
    fx_vgrad(r, 0.0f, ground, (float)win_w, 6.0f, FX_INK, 80, FX_INK, 0);

    /* A rooftop unit on the left: louvred casing, a fan guard standing proud
     * of its lid, lit on the side the moon is on, and the conduit that feeds
     * it running off along the foot of the parapet. */
    const float unit_x = 28.0f;
    const float unit_w = 84.0f;
    SDL_Color casing = fx_mix(FX_STEEL_DK, COL_ROOF_DECK, 0.35f);
    fx_rect_a(r, FX_INK, 110, unit_x - 4.0f, ground - 1.0f, unit_w + 10.0f, 4.0f);
    fx_rect_a(r, FX_INK, 50, unit_x - 8.0f, ground + 3.0f, unit_w + 18.0f, 2.0f);
    color_rect(r, fx_dim(casing, 0.8f), unit_x + unit_w, ground - 5.0f, 70.0f,
               4.0f);
    color_rect(r, fx_mix(casing, FX_PALE, 0.18f), unit_x + unit_w,
               ground - 5.0f, 70.0f, 1.0f);
    color_rect(r, FX_INK, unit_x - 1.0f, ground - 31.0f, unit_w + 2.0f, 31.0f);
    color_rect(r, casing, unit_x, ground - 30.0f, unit_w, 30.0f);
    for (float ly = ground - 25.0f; ly < ground - 3.0f; ly += 3.0f)
    {
        color_rect(r, fx_dim(casing, 0.62f), unit_x + 6.0f, ly, unit_w - 12.0f,
                   1.0f);
        color_rect(r, fx_mix(casing, FX_PALE, 0.10f), unit_x + 6.0f, ly + 1.0f,
                   unit_w - 12.0f, 1.0f);
    }
    color_rect(r, fx_dim(casing, 0.5f), unit_x + 6.0f, ground - 3.0f,
               unit_w - 12.0f, 3.0f);
    SDL_Color lid = fx_mix(COL_ROOF_CONCRETE, FX_STEEL_DK, 0.5f);
    color_rect(r, FX_INK, unit_x - 5.0f, ground - 36.0f, unit_w + 10.0f, 7.0f);
    color_rect(r, lid, unit_x - 4.0f, ground - 35.0f, unit_w + 8.0f, 5.0f);
    color_rect(r, fx_mix(lid, FX_PALE, 0.30f), unit_x - 4.0f, ground - 35.0f,
               unit_w + 8.0f, 1.0f);
    color_rect(r, FX_INK, unit_x + 23.0f, ground - 41.0f, 38.0f, 6.0f);
    color_rect(r, fx_dim(lid, 0.8f), unit_x + 24.0f, ground - 40.0f, 36.0f, 5.0f);
    color_rect(r, fx_mix(lid, FX_PALE, 0.36f), unit_x + 24.0f, ground - 40.0f,
               36.0f, 1.0f);
    for (float gx = unit_x + 27.0f; gx < unit_x + 58.0f; gx += 4.0f)
        color_rect(r, fx_dim(lid, 0.5f), gx, ground - 39.0f, 1.0f, 3.0f);
    fx_vgrad(r, unit_x + unit_w - 2.0f, ground - 35.0f, 2.0f, 35.0f, FX_PALE,
             46, FX_PALE, 8);

    /* The stair head the whole night has been climbing toward. Concrete,
     * rain-streaked down from its coping, its moon side lit, and one caged
     * lamp over the door laying a pool on the wet deck in front of it. */
    const float head_x = 616.0f;
    const float head_w = 171.0f;
    const float head_top = ground - 91.0f;
    fx_rect_a(r, FX_INK, 90, head_x - 6.0f, ground, head_w + 18.0f, 4.0f);
    color_rect(r, COL_ROOF_BULKHEAD, head_x, head_top, head_w, 91.0f);
    for (unsigned i = 0; i < 9u; ++i)
    {
        unsigned streak = scene_hash(i * 9176u + 0x53544149u);
        float sx = head_x + 4.0f + (float)fx_spread(streak, head_w - 8.0f);
        float sl = 20.0f + (float)((streak >> 12) % 50u);
        fx_vgrad(r, floorf(sx), head_top, 2.0f + (float)(streak & 1u), sl,
                 FX_INK, 56, FX_INK, 0);
    }
    fx_vgrad(r, head_x, head_top, 3.0f, 91.0f, FX_PALE, 40, FX_PALE, 10);
    fx_vgrad(r, head_x + head_w - 10.0f, head_top, 10.0f, 91.0f, FX_INK, 0,
             FX_INK, 60);
    color_rect(r, FX_INK, 607.0f, ground - 99.0f, 189.0f, 10.0f);
    color_rect(r, fx_mix(COL_ROOF_BULKHEAD, COL_ROOF_CONCRETE, 0.8f), 608.0f,
               ground - 98.0f, 187.0f, 8.0f);
    color_rect(r, fx_mix(COL_ROOF_CONCRETE, FX_PALE, 0.34f), 608.0f,
               ground - 98.0f, 187.0f, 1.0f);
    color_rect(r, fx_dim(COL_ROOF_CONCRETE, 0.55f), 608.0f, ground - 91.0f,
               187.0f, 1.0f);
    fx_vgrad(r, head_x, head_top, head_w, 8.0f, FX_INK, 70, FX_INK, 0);

    /* The doorway: a lit frame, the dark of the stair beyond with the first
     * few treads just catching the lamp, and the plate over it. */
    color_rect(r, fx_dim(COL_ROOF_BULKHEAD, 0.5f), 649.0f, ground - 76.0f,
               86.0f, 76.0f);
    color_rect(r, COL_ROOF_DOORWAY, 656.0f, ground - 69.0f, 72.0f, 69.0f);
    for (int tread = 0; tread < 4; ++tread)
        fx_rect_a(r, FX_WARM, (Uint8)(26 - tread * 6), 656.0f,
                  ground - 12.0f - (float)tread * 11.0f, 72.0f, 2.0f);
    color_rect(r, fx_mix(COL_ROOF_CONCRETE, FX_PALE, 0.22f), 649.0f,
               ground - 76.0f, 86.0f, 4.0f);
    color_rect(r, fx_mix(COL_ROOF_BULKHEAD, FX_PALE, 0.14f), 649.0f,
               ground - 72.0f, 3.0f, 72.0f);
    color_rect(r, fx_dim(COL_ROOF_BULKHEAD, 0.72f), 670.0f, ground - 93.0f,
               44.0f, 12.0f);
    draw_text(r, 676.0f, ground - 91.0f, 1.0f, COL_ROOF_SIGN, "ROOF");

    const float lamp_x = 692.0f;
    const float lamp_y = ground - 80.0f;
    fx_light_cone(r, lamp_x, lamp_y + 3.0f, 5.0f, 58.0f, 96.0f, FX_WARM, 34);
    for (int lobe = -1; lobe <= 1; ++lobe)
        fx_glow(r, lamp_x + (float)lobe * 30.0f, ground + 8.0f, 38.0f, FX_WARM,
                24);
    fx_glow(r, lamp_x, lamp_y + 1.0f, 26.0f, FX_WARM, 90);
    color_rect(r, FX_INK, lamp_x - 4.0f, lamp_y - 2.0f, 8.0f, 6.0f);
    color_rect(r, FX_WARM, lamp_x - 3.0f, lamp_y - 1.0f, 6.0f, 4.0f);
    color_rect(r, fx_dim(FX_WARM, 0.5f), lamp_x - 1.0f, lamp_y - 1.0f, 1.0f,
               4.0f);
    color_rect(r, fx_dim(FX_WARM, 0.5f), lamp_x + 1.0f, lamp_y - 1.0f, 1.0f,
               4.0f);

    float beacon = 0.38f + 0.62f * (sinf(time * 5.5f) > 0.25f);
    SDL_Color beacon_color = fx_dim(FX_RUST, beacon);
    if (beacon > 0.5f)
        fx_glow(r, 630.0f, ground - 103.0f, 16.0f, FX_RUST, 70);
    color_rect(r, FX_INK, 624.0f, ground - 106.0f, 12.0f, 7.0f);
    color_rect(r, beacon_color, 625.0f, ground - 105.0f, 10.0f, 5.0f);
}

static void rotate_local(float cx, float cy, float lx, float ly, float angle,
                         float *out_x, float *out_y)
{
    float c = cosf(angle);
    float s = sinf(angle);
    *out_x = cx + lx * c - ly * s;
    *out_y = cy + lx * s + ly * c;
}

static void draw_rotated_box(SDL_Renderer *r, float cx, float cy,
                             float x, float y, float w, float h,
                             float angle, SDL_Color color)
{
    SDL_FPoint points[4];
    rotate_local(cx, cy, x, y, angle, &points[0].x, &points[0].y);
    rotate_local(cx, cy, x + w, y, angle, &points[1].x, &points[1].y);
    rotate_local(cx, cy, x + w, y + h, angle, &points[2].x, &points[2].y);
    rotate_local(cx, cy, x, y + h, angle, &points[3].x, &points[3].y);

    SDL_FColor fc = {(float)color.r / 255.0f,
                     (float)color.g / 255.0f,
                     (float)color.b / 255.0f,
                     (float)color.a / 255.0f};
    SDL_Vertex vertices[4] = {
        {points[0], fc, {0.0f, 0.0f}},
        {points[1], fc, {0.0f, 0.0f}},
        {points[2], fc, {0.0f, 0.0f}},
        {points[3], fc, {0.0f, 0.0f}}};
    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, NULL, vertices, 4, indices, 6);
}

/* The airframe without its rotors, `scale` times the outro's size: the key
 * art draws the same ship holding a hover, where a spinning rotor is a blur
 * rather than a blade. `damage` above a quarter is the scorched paint of the
 * outro's last seconds. */
static void draw_helicopter_hull(SDL_Renderer *r, float x, float y,
                                 float angle, float damage, float scale)
{
    const float k = scale;
    SDL_Color body_dark = {10, 17, 20, 255};
    SDL_Color body = damage > 0.25f
                         ? (SDL_Color){50, 52, 47, 255}
                         : (SDL_Color){38, 69, 72, 255};
    SDL_Color body_light = damage > 0.25f
                               ? (SDL_Color){83, 74, 60, 255}
                               : (SDL_Color){70, 111, 111, 255};

    /* Shadow makes the craft readable against both sky and buildings. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    draw_rotated_box(r, x + 4.0f * k, y + 5.0f * k, -48.0f * k,
                     -17.0f * k, 82.0f * k, 37.0f * k, angle,
                     (SDL_Color){2, 4, 7, 120});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_rotated_box(r, x, y, -48.0f * k, -16.0f * k, 82.0f * k, 34.0f * k,
                     angle, body_dark);
    draw_rotated_box(r, x, y, -43.0f * k, -13.0f * k, 72.0f * k, 27.0f * k,
                     angle, body);
    draw_rotated_box(r, x, y, -38.0f * k, -10.0f * k, 25.0f * k, 19.0f * k,
                     angle, (SDL_Color){15, 35, 43, 255});
    draw_rotated_box(r, x, y, -35.0f * k, -8.0f * k, 18.0f * k, 14.0f * k,
                     angle, (SDL_Color){50, 108, 119, 255});
    draw_rotated_box(r, x, y, -9.0f * k, -10.0f * k, 32.0f * k, 4.0f * k,
                     angle, body_light);

    /* Tail boom and stabilizer. */
    draw_rotated_box(r, x, y, 28.0f * k, -6.0f * k, 77.0f * k, 12.0f * k,
                     angle, body_dark);
    draw_rotated_box(r, x, y, 31.0f * k, -3.0f * k, 67.0f * k, 7.0f * k,
                     angle, body);
    draw_rotated_box(r, x, y, 91.0f * k, -24.0f * k, 12.0f * k, 28.0f * k,
                     angle, body_dark);
    draw_rotated_box(r, x, y, 94.0f * k, -21.0f * k, 7.0f * k, 22.0f * k,
                     angle, body_light);

    /* Landing skids and the rotor mast. */
    draw_rotated_box(r, x, y, -31.0f * k, 24.0f * k, 62.0f * k, 3.0f * k,
                     angle, body_dark);
    draw_rotated_box(r, x, y, -24.0f * k, 15.0f * k, 3.0f * k, 11.0f * k,
                     angle, body_dark);
    draw_rotated_box(r, x, y, 19.0f * k, 14.0f * k, 3.0f * k, 12.0f * k,
                     angle, body_dark);
    draw_rotated_box(r, x, y, -2.0f * k, -31.0f * k, 4.0f * k, 17.0f * k,
                     angle, body_dark);
}

static void draw_helicopter(SDL_Renderer *r, float x, float y,
                            float angle, float rotor_angle, float damage)
{
    draw_helicopter_hull(r, x, y, angle, damage, 1.0f);

    float tail_x = 0.0f, tail_y = 0.0f;
    rotate_local(x, y, 100.0f, -8.0f, angle, &tail_x, &tail_y);
    draw_rotated_box(r, tail_x, tail_y, -18.0f, -1.5f, 36.0f, 3.0f,
                     angle + rotor_angle * 1.7f, FX_INK);
    draw_rotated_box(r, tail_x, tail_y, -14.0f, -1.5f, 28.0f, 3.0f,
                     angle + rotor_angle * 1.7f + 1.5708f, FX_INK);

    float rotor_x = 0.0f, rotor_y = 0.0f;
    rotate_local(x, y, 0.0f, -31.0f, angle, &rotor_x, &rotor_y);
    draw_rotated_box(r, rotor_x, rotor_y, -85.0f, -2.0f, 170.0f, 4.0f,
                     angle + rotor_angle, (SDL_Color){18, 25, 27, 255});
    draw_rotated_box(r, rotor_x, rotor_y, -56.0f, -1.0f, 112.0f, 2.0f,
                     angle + rotor_angle + 1.5708f,
                     (SDL_Color){87, 96, 91, 255});

    if (damage > 0.0f)
    {
        float flicker = 0.45f + 0.55f * sinf(damage * 47.0f);
        draw_rotated_box(r, x, y, -7.0f, -15.0f,
                         8.0f + flicker * 8.0f,
                         7.0f + flicker * 7.0f, angle,
                         (SDL_Color){245, 116, 35, 255});
        color_rect(r, (SDL_Color){69, 76, 70, 255},
                   x - 5.0f, y - 38.0f - damage * 18.0f,
                   11.0f + damage * 12.0f, 7.0f + damage * 8.0f);
    }
}

/* Angled searchlight cone from the hovering helicopter. */
static void draw_search_beam(SDL_Renderer *r, float apex_x, float apex_y,
                             float target_x, float target_y, float half_w,
                             SDL_Color c, Uint8 alpha)
{
    SDL_Vertex v[3] = {
        {{apex_x, apex_y}, fx_fcolor(c, (float)alpha / 255.0f), {0.0f, 0.0f}},
        {{target_x - half_w, target_y}, fx_fcolor(c, 0.0f), {0.0f, 0.0f}},
        {{target_x + half_w, target_y}, fx_fcolor(c, 0.0f), {0.0f, 0.0f}}};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 3, NULL, 0);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* The one shot at the sky: the armed pose with the arm raised along the line
 * to the helicopter, the same arm he aims level with. */
static void draw_outro_agent_sky_aim(SDL_Renderer *r, float x,
                                     float ground_y, float scale,
                                     float time)
{
    draw_agent_armed(r, x, ground_y, scale, time, AGENT_GUN_SKY, 1,
                     CHUCK_FACE_EASY);
}

/* An angled shot, which is the only kind this screen fires. The name says
   `angled` because the figures have a muzzle flash of their own and the two
   used to be called the same thing in two files while drawing different light;
   they share `fx_muzzle_glow` now and only the flame is still local. */
static void draw_angled_muzzle_flash(SDL_Renderer *r, float x, float y,
                                     float angle, float strength)
{
    float dx = cosf(angle);
    float dy = sinf(angle);
    /* The brightest thing in the frame illuminates the air around it, or it
       reads as a decal stuck on the gun. */
    fx_muzzle_glow(r, x, y, strength, FX_AMBER);
    set_color(r, FX_FLAME_HOT);
    SDL_RenderLine(r, x - dy * 5.0f, y + dx * 5.0f,
                   x + dy * 5.0f, y - dx * 5.0f);
    set_color(r, FX_FLAME);
    SDL_RenderLine(r, x, y, x + dx * (13.0f * strength),
                   y + dy * (13.0f * strength));
}

static void draw_shot_tracer(SDL_Renderer *r, float time, float shot_time,
                             float from_x, float from_y,
                             float to_x, float to_y)
{
    float age = time - shot_time;
    if (age < 0.0f || age > 0.16f)
        return;

    float head = clamp01(age / 0.08f);
    float tail = clamp01((age - 0.035f) / 0.11f);
    float hx = lerpf(from_x, to_x, head);
    float hy = lerpf(from_y, to_y, head);
    float tx = lerpf(from_x, to_x, tail);
    float ty = lerpf(from_y, to_y, tail);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_rgba(r, FX_FLAME.r, FX_FLAME.g, FX_FLAME.b, 105);
    SDL_RenderLine(r, tx, ty + 2.0f, hx, hy + 2.0f);
    set_rgba(r, FX_FLAME_HOT.r, FX_FLAME_HOT.g, FX_FLAME_HOT.b, 255);
    SDL_RenderLine(r, tx, ty, hx, hy);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    if (age < 0.07f)
        draw_angled_muzzle_flash(r, from_x, from_y,
                                 atan2f(to_y - from_y, to_x - from_x), 1.0f);
}

/*
 * A man down on the deck, on his back, in profile: boots, the legs with one
 * knee still half up, the torso with the arm fallen along it, and the head
 * with the face turned to the sky. The old drawing was a black slab with a
 * square of skin at one end and a block standing up off the middle of it,
 * which read as luggage rather than as the man who had been holding her.
 */
static void draw_terrorist_down(SDL_Renderer *r, float x, float ground_y,
                                bool faces_right, bool leader)
{
    /* The coat and the bare head are what said which of them was Voss while
       he was standing; down on the deck they have to keep saying it. */
    SDL_Color garment = leader ? (SDL_Color){86, 90, 92, 255}
                               : (SDL_Color){42, 47, 43, 255};
    SDL_Color trouser = leader ? (SDL_Color){46, 48, 52, 255} : CREW_TROUSER;
    SDL_Color boot = leader ? (SDL_Color){28, 22, 20, 255} : CREW_BOOT;
    SDL_Color skin = fx_dim(leader ? VOSS_SKIN : CREW_SKIN, 0.78f);
    SDL_Color hair = leader ? (SDL_Color){118, 120, 116, 255}
                            : (SDL_Color){24, 28, 27, 255};
    float gy = ground_y - 1.0f;
#define DOWN_X(lx, w) (faces_right ? x + (lx) : x + 46.0f - (lx) - (w))

    fx_contact_shadow(r, x + 23.0f, ground_y - 3.0f, 28.0f, 0.0f, 190);

    /* The silhouette, then the parts inside it. */
    color_rect(r, FX_INK, DOWN_X(0.0f, 19.0f), gy - 7.0f, 19.0f, 7.0f);
    color_rect(r, FX_INK, DOWN_X(6.0f, 8.0f), gy - 10.0f, 8.0f, 4.0f);
    color_rect(r, FX_INK, DOWN_X(17.0f, 19.0f), gy - 11.0f, 19.0f, 11.0f);
    color_rect(r, FX_INK, DOWN_X(35.0f, 10.0f), gy - 10.0f, 10.0f, 10.0f);

    color_rect(r, fx_dim(trouser, 0.75f), DOWN_X(3.0f, 15.0f), gy - 6.0f,
               15.0f, 2.0f);
    /* The near knee, still half raised. */
    color_rect(r, trouser, DOWN_X(7.0f, 6.0f), gy - 9.0f, 6.0f, 3.0f);
    color_rect(r, trouser, DOWN_X(3.0f, 15.0f), gy - 4.0f, 15.0f, 3.0f);
    color_rect(r, fx_ramp(trouser).lit, DOWN_X(7.0f, 6.0f), gy - 9.0f, 6.0f,
               1.0f);
    color_rect(r, boot, DOWN_X(1.0f, 3.0f), gy - 6.0f, 3.0f, 5.0f);
    color_rect(r, fx_ramp(boot).lit, DOWN_X(1.0f, 1.0f), gy - 6.0f, 1.0f,
               5.0f);

    color_rect(r, garment, DOWN_X(18.0f, 17.0f), gy - 10.0f, 17.0f, 9.0f);
    color_rect(r, fx_ramp(garment).lit, DOWN_X(18.0f, 17.0f), gy - 10.0f,
               17.0f, 1.0f);
    color_rect(r, fx_ramp(garment).dark, DOWN_X(18.0f, 17.0f), gy - 2.0f,
               17.0f, 1.0f);
    if (leader)
        color_rect(r, (SDL_Color){206, 204, 188, 255}, DOWN_X(32.0f, 3.0f),
                   gy - 9.0f, 3.0f, 5.0f);
    else
        color_rect(r, FX_RUST, DOWN_X(25.0f, 3.0f), gy - 10.0f, 3.0f, 9.0f);
    /* The arm, fallen along his side, and the hand at his hip. */
    color_rect(r, fx_dim(garment, 0.8f), DOWN_X(20.0f, 13.0f), gy - 4.0f,
               13.0f, 2.0f);
    color_rect(r, skin, DOWN_X(17.0f, 3.0f), gy - 4.0f, 3.0f, 2.0f);

    /* The head, face up: the back of the skull on the deck, the profile
     * turned to the sky, the nose breaking the top of the outline. */
    color_rect(r, skin, DOWN_X(36.0f, 8.0f), gy - 9.0f, 8.0f, 8.0f);
    color_rect(r, hair, DOWN_X(36.0f, 8.0f), gy - 3.0f, 8.0f, 2.0f);
    color_rect(r, hair, DOWN_X(42.0f, 2.0f), gy - 9.0f, 2.0f, 8.0f);
    color_rect(r, FX_INK, DOWN_X(38.0f, 2.0f), gy - 11.0f, 2.0f, 2.0f);
    color_rect(r, skin, DOWN_X(38.0f, 1.0f), gy - 10.0f, 1.0f, 1.0f);
    if (leader)
        color_rect(r, FX_INK, DOWN_X(39.0f, 2.0f), gy - 7.0f, 2.0f, 1.0f);
    else
        color_rect(r, FX_RED, DOWN_X(39.0f, 2.0f), gy - 7.0f, 2.0f, 1.0f);

    if (!leader)
    {
        /* His rifle, where it fell on the deck beside him. */
        color_rect(r, FX_INK, DOWN_X(26.0f, 28.0f), gy - 2.0f, 28.0f, 3.0f);
        color_rect(r, (SDL_Color){67, 73, 69, 255}, DOWN_X(34.0f, 19.0f),
                   gy - 1.0f, 19.0f, 1.0f);
    }
#undef DOWN_X
}

static void draw_shock_mark(SDL_Renderer *r, float x, float y, float time)
{
    float pulse = 0.65f + 0.35f * sinf(time * 13.0f);
    SDL_Color color = {(Uint8)(FX_AMBER.r * pulse),
                       (Uint8)(FX_AMBER.g * pulse),
                       (Uint8)(FX_AMBER.b * pulse), 255};
    color_rect(r, color, x, y, 4.0f, 14.0f);
    color_rect(r, color, x, y + 18.0f, 4.0f, 4.0f);
}

static void draw_explosion(SDL_Renderer *r, float x, float y, float age)
{
    if (age < 0.0f || age > 3.2f)
        return;

    float expand = smoothstep01(age / 0.42f);
    float fade = 1.0f - clamp01((age - 0.95f) / 2.25f);

    if (age < 1.4f)
        fx_glow(r, x, y, 150.0f, fx_mix(FX_FLAME, FX_AMBER, 0.45f),
                (Uint8)(120.0f * (1.0f - age / 1.4f)));

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    if (age < 0.85f)
    {
        float radius = 18.0f + expand * 78.0f;
        set_rgba(r, FX_FLAME.r, FX_FLAME.g, FX_FLAME.b, (Uint8)(205.0f * (1.0f - age / 0.85f)));
        fill_rect(r, x - radius, y - radius * 0.55f,
                  radius * 2.0f, radius * 1.10f);
        set_rgba(r, FX_FLAME_HOT.r, FX_FLAME_HOT.g, FX_FLAME_HOT.b,
                 (Uint8)(240.0f * (1.0f - age / 0.85f)));
        fill_rect(r, x - radius * 0.45f, y - radius * 0.38f,
                  radius * 0.90f, radius * 0.72f);
    }

    for (unsigned i = 0; i < 24u; ++i)
    {
        unsigned h = scene_hash(i + 0x4558504cu);
        float angle = (float)(h % 628u) * 0.01f;
        float speed = 24.0f + (float)((h >> 8) % 58u);
        float distance = speed * age;
        float px = x + cosf(angle) * distance;
        float py = y + sinf(angle) * distance * 0.55f -
                   age * (20.0f + (float)(i % 4u) * 5.0f);
        float size = 5.0f + (float)(i % 5u) * 2.0f + age * 4.0f;
        SDL_Color smoke = i % 4u == 0u
                              ? (SDL_Color){103, 76, 52, 255}
                              : (SDL_Color){49, 55, 54, 255};
        set_rgba(r, smoke.r, smoke.g, smoke.b,
                 (Uint8)(fade * (i % 3u == 0u ? 190.0f : 135.0f)));
        fill_rect(r, px - size * 0.5f, py - size * 0.5f, size, size);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_wreckage(SDL_Renderer *r, float x, float ground_y,
                          float time)
{
    fx_contact_shadow(r, x - 1.0f, ground_y - 5.0f, 71.0f, 0.0f, 200);
    color_rect(r, FX_INK, x - 48.0f, ground_y - 23.0f, 82.0f, 20.0f);
    color_rect(r, (SDL_Color){48, 50, 45, 255},
               x - 41.0f, ground_y - 19.0f, 67.0f, 12.0f);
    draw_rotated_box(r, x, ground_y, -91.0f, -5.0f, 150.0f, 4.0f,
                     -0.15f, (SDL_Color){20, 25, 26, 255});
    float fire = 0.5f + 0.5f * sinf(time * 17.0f);
    color_rect(r, FX_FLAME,
               x - 12.0f, ground_y - 28.0f - fire * 8.0f,
               18.0f, 16.0f + fire * 8.0f);
    color_rect(r, FX_FLAME_HOT,
               x - 7.0f, ground_y - 23.0f - fire * 5.0f,
               9.0f, 12.0f + fire * 5.0f);
}

static void draw_pixel_heart(SDL_Renderer *r, float center_x,
                             float y, float time)
{
    /* fx.h's one heart, held up large: the same glyph the HUD counts and
       the manual teaches, pulsing on the outro's own clock. */
    float pulse = 1.0f + (0.5f + 0.5f * sinf(time * 4.0f)) * 0.10f;
    float pixel = 3.0f * pulse;
    float bob = sinf(time * 2.2f) * 2.0f;

    fx_glow(r, center_x, y + bob + 3.0f * pixel, 7.0f * pixel + 14.0f,
            FX_RED, 66);
    fx_heart(r, center_x - 3.5f * pixel, y + bob, pixel, true);
}

static void draw_reunion_pair(SDL_Renderer *r, float center_x,
                              float ground_y, float time)
{
    /*
     * Keep the ending deliberately simple: the two original sprites stand
     * close and face one another, with no overlapping limbs to muddy them.
     */
    draw_hostage(r, center_x + OUTRO_REUNION_HOSTAGE_OFFSET, ground_y,
                 OUTRO_REUNION_HOSTAGE_SCALE, 0.0f, 1, false);
    draw_agent(r, center_x + OUTRO_REUNION_AGENT_OFFSET, ground_y,
               AGENT_SCALE, time, -1, CHUCK_GAIT_WALK, 0.0f);
    draw_pixel_heart(r, center_x, ground_y - 94.0f, time);
}

static void render_outro_ui(SDL_Renderer *r, float time,
                            float hostage_x, int win_w, int win_h,
                            const PadHints *pad)
{
    if (time >= 4.35f && time < 7.45f)
    {
        float reveal = smoothstep01((time - 4.35f) / 0.35f);
        draw_text(r, 34.0f, 39.0f, 1.0f,
                  (SDL_Color){(Uint8)(FX_AMBER.r * reveal),
                              (Uint8)(FX_AMBER.g * reveal),
                              (Uint8)(FX_AMBER.b * reveal), 255},
                  "ROOF // 01:00 // THEIR RIDE IS COMING");
        color_rect(r, FX_AMBER, 34.0f, 54.0f, 105.0f * reveal, 2.0f);
    }

    if (time >= 6.00f && time < 9.30f)
    {
        float pulse = 0.5f + 0.5f * sinf(time * 6.0f);
        draw_target_brackets(r, hostage_x - 10.0f, 352.0f,
                             94.0f, 73.0f, pulse);
        draw_text(r, 34.0f, 78.0f, 1.0f, FX_RUST,
                  "NO CLEAR SHOT // ELLEN IN LINE OF FIRE");
    }

    if (time >= 9.25f && time < 10.25f)
    {
        float reveal = smoothstep01((time - 9.25f) / 0.18f);
        draw_text(r, 34.0f, 78.0f, 1.0f,
                  (SDL_Color){(Uint8)(FX_CYAN.r * reveal),
                              (Uint8)(FX_CYAN.g * reveal),
                              (Uint8)(FX_CYAN.b * reveal), 255},
                  "NEW TARGET // THE RIDE");
    }

    if (time >= 13.35f && time < 16.50f)
    {
        float reveal = smoothstep01((time - 13.35f) / 0.22f);
        draw_text(r, 34.0f, 39.0f, 1.0f,
                  (SDL_Color){(Uint8)(FX_RUST.r * reveal),
                              (Uint8)(FX_RUST.g * reveal),
                              (Uint8)(FX_RUST.b * reveal), 255},
                  "NO RIDE // VOSS HAS NOWHERE LEFT TO GO");
    }

    /*
     * The last caption, and the only one that is not a readout.
     *
     * Voss's men spent seventeen sectors calling him a cowboy — off the net, and
     * shouted down at him off the wall — because a man alone in a building
     * doing this is the only thing they could file him as. It is the wrong
     * word, and this is the line that says so: he came up forty floors for one
     * person and not for the building, and the whole night pays off on that
     * being the difference between them. It works without the joke; it lands
     * twice with it.
     */
    if (time >= 16.60f && time < OUTRO_FINAL_REVEAL_TIME)
    {
        float reveal = smoothstep01((time - 16.60f) / 0.3f);
        draw_text(r, 34.0f, 39.0f, 1.0f, fx_dim(FX_CREAM, reveal),
                  "NO COWBOY // JUST THE HUSBAND");
        color_rect(r, fx_dim(FX_RUST, reveal), 34.0f, 54.0f,
                   88.0f * reveal, 2.0f);
    }

    if (time > 0.85f && time < OUTRO_FINAL_REVEAL_TIME)
    {
        float pulse = 0.45f + 0.55f * sinf(time * 2.0f);
        char hint[32];
        draw_text(r, (float)win_w - 181.0f, (float)win_h - 31.0f,
                  1.0f,
                  (SDL_Color){(Uint8)(101.0f + pulse * 40.0f),
                              (Uint8)(109.0f + pulse * 40.0f),
                              (Uint8)(108.0f + pulse * 38.0f), 255},
                  pad_hint(pad, hint, sizeof(hint),
                           PAD_CONFIRM_PAD " TO SKIP",
                           PAD_CONFIRM_KEYS " TO SKIP"));
    }
}

void outro_cutscene_render(SDL_Renderer *r,
                           const OutroCutscene *cutscene,
                           int win_w, int win_h, const PadHints *pad)
{
    const float time = cutscene->time;
    const float ground = 438.0f;
    const float captor_one_x = 298.0f;
    const float hostage_x = 344.0f;
    const float captor_two_x = 390.0f;
    const float agent_stop_x = 538.0f;
    const float reunion_center_x = (float)win_w * 0.5f;
    const float reunion_agent_x =
        reunion_center_x + OUTRO_REUNION_AGENT_OFFSET;
    const float reunion_hostage_x =
        reunion_center_x + OUTRO_REUNION_HOSTAGE_OFFSET;

    render_outro_sky(r, time, win_w, win_h);
    render_rooftop(r, time, win_w, win_h);

    float heli_x;
    float heli_y;
    float heli_angle = 0.0f;
    float rotor_angle = time * 22.0f;
    float heli_damage = 0.0f;
    bool helicopter_visible = time >= 4.20f && time < 13.35f;

    if (time < 10.10f)
    {
        float approach = smoothstep01((time - 4.20f) / 5.15f);
        heli_x = lerpf((float)win_w + 155.0f, 675.0f, approach);
        heli_y = lerpf(116.0f, 168.0f, approach) +
                 sinf(time * 3.2f) * 3.0f;
    }
    else
    {
        float crash = clamp01((time - 10.10f) / 3.25f);
        heli_x = lerpf(675.0f, 710.0f, crash);
        heli_y = lerpf(168.0f, 389.0f, crash * crash);
        heli_angle = crash * 12.9f;
        rotor_angle = time * lerpf(20.0f, 2.0f, crash);
        heli_damage = crash;
    }

    if (helicopter_visible)
    {
        /* While hovering, the helicopter sweeps a searchlight over the
         * hostage group; the beam dies once the craft is hit. */
        if (time >= 5.4f && time < 10.10f)
        {
            float sweep = sinf(time * 0.85f) * 30.0f;
            float beam_x = hostage_x + 22.0f + sweep;
            draw_search_beam(r, heli_x - 32.0f, heli_y + 12.0f,
                             beam_x, ground + 4.0f, 36.0f,
                             (SDL_Color){236, 244, 248, 255}, 36);
            fx_glow(r, beam_x, ground + 2.0f, 46.0f,
                    (SDL_Color){236, 244, 248, 255}, 26);
        }
        draw_helicopter(r, heli_x, heli_y, heli_angle,
                        rotor_angle, heli_damage);
    }

    if (time >= 13.35f)
    {
        draw_wreckage(r, 710.0f, ground, time);
        draw_explosion(r, 710.0f, 379.0f, time - 13.35f);
    }

    /* Captors and hostage cross the roof first and wait for extraction. */
    if (time >= 0.45f)
    {
        float group_move = smoothstep01((time - 0.45f) / 3.30f);
        float first_x = lerpf(690.0f, captor_one_x, group_move);
        float woman_x = lerpf(726.0f, hostage_x, group_move);
        float second_x = lerpf(765.0f, captor_two_x, group_move);
        int group_dir = time < 3.75f ? -1 : 1;

        if (time < 14.70f)
        {
            float shake = time >= 13.35f
                              ? sinf(time * 36.0f) * 2.5f
                              : 0.0f;
            draw_terrorist(r, first_x + shake, ground,
                           1.34f, time < 3.75f ? time : 0.0f,
                           0.0f, group_dir, true);
            if (time >= 13.35f)
                draw_shock_mark(r, first_x + 17.0f, ground - 78.0f, time);
        }
        else if (time < 15.60f)
        {
            draw_terrorist(r, first_x, ground,
                           1.34f, 0.0f, 0.0f, 1, true);
        }
        else
        {
            draw_terrorist_down(r, first_x, ground, false, true);
        }

        if (time < 14.70f)
        {
            float shake = time >= 13.35f
                              ? sinf(time * 33.0f + 1.2f) * 2.5f
                              : 0.0f;
            draw_terrorist(r, second_x + shake, ground,
                           1.34f, time < 3.75f ? time : 0.0f,
                           2.2f, group_dir, false);
            if (time >= 13.35f)
                draw_shock_mark(r, second_x + 17.0f,
                                ground - 78.0f, time + 0.3f);
        }
        else
        {
            draw_terrorist_down(r, second_x, ground, true, false);
        }

        if (time < 16.45f)
        {
            draw_hostage(r, woman_x, ground, 1.18f,
                         time < 3.75f ? time : 0.0f, group_dir, true);
        }
        else if (time < 18.35f)
        {
            float reunion = smoothstep01((time - 16.45f) / 1.90f);
            draw_hostage(r, lerpf(woman_x, reunion_hostage_x, reunion),
                         ground, OUTRO_REUNION_HOSTAGE_SCALE,
                         time * 1.4f, 1, false);
        }
    }

    /* Chuck follows, holds his fire, then pivots toward the helicopter. */
    if (time >= 2.35f && time < 5.05f)
    {
        float u = (time - 2.35f) / 2.55f;
        draw_agent(r, lerpf(704.0f, agent_stop_x, smoothstep01(u)), ground,
                   AGENT_SCALE, time, -1, CHUCK_GAIT_PLAIN_RUN,
                   smoothstep_pace(u));
    }
    else if (time >= 5.05f && time < 9.25f)
    {
        bool aiming = time < 8.55f;
        draw_agent_held_fire(r, agent_stop_x, ground,
                             AGENT_SCALE, time, aiming, -1);
    }
    else if (time >= 9.25f && time < 10.45f)
    {
        draw_outro_agent_sky_aim(r, agent_stop_x, ground, AGENT_SCALE, time);
    }
    else if (time >= 10.45f && time < 14.20f)
    {
        draw_agent_held_fire(r, agent_stop_x, ground,
                             AGENT_SCALE, time, false, 1);
    }
    else if (time >= 14.20f && time < 16.40f)
    {
        draw_agent_held_fire(r, agent_stop_x, ground,
                             AGENT_SCALE, time, true, -1);
    }
    else if (time >= 16.40f && time < 18.35f)
    {
        float u = (time - 16.40f) / 1.95f;
        draw_agent(r, lerpf(agent_stop_x, reunion_agent_x, smoothstep01(u)),
                   ground, AGENT_SCALE, time, -1,
                   CHUCK_GAIT_WALK, smoothstep_pace(u));
    }
    else if (time >= 18.35f && time < OUTRO_FINAL_REVEAL_TIME)
    {
        draw_reunion_pair(r, reunion_center_x, ground, time);
    }

    /* Three deliberate shots tell the turn-and-rescue beat without gameplay. */
    /* Each round leaves the muzzle of the pistol as it is drawn in that pose,
     * so the flash sits on the gun rather than in the air in front of it. */
    float sky_x, sky_y, level_x, level_y;
    agent_muzzle_point(agent_stop_x, ground, AGENT_SCALE, 1, time,
                       AGENT_GUN_SKY, &sky_x, &sky_y);
    agent_muzzle_point(agent_stop_x, ground, AGENT_SCALE, -1, time,
                       AGENT_GUN_LEVEL, &level_x, &level_y);
    draw_shot_tracer(r, time, 9.95f, sky_x, sky_y,
                     heli_x - 8.0f, heli_y + 2.0f);
    draw_shot_tracer(r, time, 14.55f, level_x, level_y,
                     captor_two_x + 17.0f, ground - 28.0f);
    draw_shot_tracer(r, time, 15.45f, level_x, level_y,
                     captor_one_x + 17.0f, ground - 28.0f);

    render_outro_ui(r, time, hostage_x, win_w, win_h, pad);

    fx_grain(r, win_w, win_h, time, FX_GRAIN_FILM);

    color_rect(r, FX_INK, 0.0f, 0.0f, (float)win_w, 19.0f);
    color_rect(r, FX_INK, 0.0f, (float)win_h - 19.0f, (float)win_w, 19.0f);

    if (time >= OUTRO_FINAL_REVEAL_TIME)
    {
        float reveal = smoothstep01(
            (time - OUTRO_FINAL_REVEAL_TIME) / 1.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, 3, 7, 11, (Uint8)(190.0f * reveal));
        fill_rect(r, 0.0f, 19.0f, (float)win_w,
                  (float)win_h - 38.0f);
        set_rgba(r, FX_CYAN.r, FX_CYAN.g, FX_CYAN.b,
                 (Uint8)(70.0f * reveal));
        fill_rect(r, 0.0f, 99.0f, (float)win_w, 2.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

        draw_reunion_pair(r, reunion_center_x, ground, time);
        draw_cutscene_text_centered(
            r, (float)win_w * 0.5f, 69.0f, 2.0f,
            (SDL_Color){(Uint8)((float)FX_GREEN.r * reveal),
                        (Uint8)((float)FX_GREEN.g * reveal),
                        (Uint8)((float)FX_GREEN.b * reveal), 255},
            "SHE'S SAFE");
        draw_cutscene_text_centered(
            r, (float)win_w * 0.5f, 122.0f, 2.0f,
            (SDL_Color){(Uint8)((float)FX_CREAM.r * reveal),
                        (Uint8)((float)FX_CREAM.g * reveal),
                        (Uint8)((float)FX_CREAM.b * reveal), 255},
            "THANK YOU FOR PLAYING");
        draw_cutscene_text_centered(
            r, (float)win_w * 0.5f, 151.0f, 1.0f,
            (SDL_Color){(Uint8)((float)FX_LABEL.r * reveal),
                        (Uint8)((float)FX_LABEL.g * reveal),
                        (Uint8)((float)FX_LABEL.b * reveal), 255},
            "THE BONDS BURNED. SHE DID NOT.");

        if (time >= OUTRO_REPLAY_PROMPT_TIME)
        {
            float pulse = 0.55f + 0.45f * sinf(time * 2.4f);
            char hint[40];
            draw_cutscene_text_centered(
                r, (float)win_w * 0.5f, 505.0f, 1.0f,
                (SDL_Color){(Uint8)(130.0f + pulse * 48.0f),
                            (Uint8)(139.0f + pulse * 48.0f),
                            (Uint8)(136.0f + pulse * 45.0f), 255},
                pad_hint(pad, hint, sizeof(hint),
                         "PRESS $A TO REPLAY THE RESCUE",
                         "PRESS R TO REPLAY THE RESCUE"));
        }
    }

    float fade_in = 1.0f - smoothstep01(time / 0.62f);
    if (fade_in > 0.0f)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, FX_INK.r, FX_INK.g, FX_INK.b, (Uint8)(fade_in * 255.0f));
        fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

/* ---- The store's key art ---------------------------------------------- */

/*
 * The one picture most people will ever see of this game, and the only one
 * whose job is to make somebody want to play it.
 *
 * It used to be the title screen's night recomposed — the tower from across
 * the street, the cordon at its foot, and Chuck a dozen pixels tall in the
 * searchlights three floors up. Every element was true and the picture asked
 * nothing of anybody: at the 315x250 a store lists a game at, it was a quiet
 * building with a dot on it. What the night is *about* was nowhere in it.
 *
 * So this is the moment the whole climb is for, drawn as close as the film
 * ever gets: the roof, forty floors over a city that has gone small, a
 * helicopter holding a hover with its light on the parapet, Voss at the edge
 * with his pistol down on the man hanging from it, and Ellen — the red coat,
 * the one warm colour on the roof — a stride behind him in his other hand. It
 * is not a scene the game plays; it is the stakes of every scene it does, in
 * one frame. Every figure in it is the film's own drawing, at the film's own
 * proportions, and the near planes are drawn at twice the pixel, so the cast
 * reads at a third of the size and the city they are standing over reads as
 * far away by being finer.
 *
 * **Twice, and not the two and a half that framed it best**, because the
 * press kit photographs through SDL's software renderer (`SDL_VIDEODRIVER`
 * is `dummy` there) and a render scale that is not a whole number leaves
 * seams between the one-unit runs every figure is built from: at 2.5 the
 * GPU drew clean limbs and the kit's own capture drew them striped. The cast
 * is scaled up inside the zoom instead, which costs nothing.
 */
#define KEY_ART_ZOOM 2.0f
/* The roof's edge, in the zoomed units the near planes are drawn in. */
#define KEY_ART_ROOF_Y 150.0f
#define KEY_ART_CORNER_X 215.0f
#define KEY_ART_CHUCK_SCALE 3.0f
#define KEY_ART_VOSS_SCALE 2.5f
#define KEY_ART_ELLEN_SCALE 2.3f
/* The ship, in the frame's own pixels and at a size of its own rather than
 * through the render scale, for the same reason. */
#define KEY_ART_HELI_SCALE 1.3f
#define KEY_ART_HELI_X 590.0f
#define KEY_ART_HELI_Y 96.0f

/*
 * One deck of storm cloud, seen from inside the weather: filled from `top`
 * down to a lumpy underside around `base`, the underside lit by the city
 * under it and every part of it lit by the strike when the strike is near.
 * The decks are darker than the sky between them, which is the one way a
 * cloud at night has of being seen.
 */
static void key_art_cloud_deck(SDL_Renderer *r, int win_w, float top,
                               float base, unsigned seed, SDL_Color body,
                               SDL_Color under, SDL_Color flash,
                               float flash_x, float strike)
{
    for (int x = 0; x < win_w; x += 2)
    {
        float fx = (float)x;
        float edge = base + sinf(fx * 0.009f + (float)(seed & 63u)) * 12.0f +
                     sinf(fx * 0.027f + (float)(seed >> 6 & 63u)) * 4.0f;
        /* Domes hanging off the underside, a few to a hundred pixels, and
         * not every cell has one: a deck is ragged, not scalloped. */
        for (int dome = -1; dome <= 1; ++dome)
        {
            int cell = (int)floorf(fx / 52.0f) + dome;
            unsigned h = scene_hash((unsigned)(cell + 1000) * 2654435761u + seed);
            if (h % 3u == 0u)
                continue;
            float cx = (float)cell * 52.0f + (float)(h % 40u);
            float rad = 12.0f + (float)((h >> 8) % 34u);
            float dx = fx - cx;
            if (fabsf(dx) < rad)
                edge = fmaxf(edge, base - 8.0f +
                                       sqrtf(rad * rad - dx * dx) * 0.6f);
        }
        edge = floorf(edge);
        float near = clamp01(1.0f - fabsf(fx - flash_x) / 220.0f);
        float lit = near * near * strike;
        SDL_Color c = fx_mix(body, flash, lit * 0.55f);
        SDL_Color u = fx_mix(fx_mix(body, under, 0.6f), flash, lit * 0.8f);
        color_rect(r, c, fx, top, 2.0f, edge - top - 6.0f);
        color_rect(r, fx_mix(c, u, 0.35f), fx, edge - 6.0f, 2.0f, 3.0f);
        color_rect(r, fx_mix(c, u, 0.75f), fx, edge - 3.0f, 2.0f, 3.0f);
    }
}

static void key_art_sky(SDL_Renderer *r, float time, int win_w, int win_h)
{
    const SDL_Color top = fx_mix(FX_INK, FX_NIGHT, 0.5f);
    const SDL_Color low = fx_mix(FX_MID, FX_CYAN_DK, 0.30f);
    color_rect(r, top, 0.0f, 0.0f, (float)win_w, (float)win_h);
    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, 300.0f, top, 255, low, 255);

    /* The strike, and the cloud it is inside: the ship holds its hover in
     * front of the brightest patch of sky in the frame, so it is a shape
     * against the light rather than a dark thing on a dark one. */
    float strike = 0.86f + 0.14f * sinf(time * 23.0f) * sinf(time * 7.0f);
    const float bolt_x = 694.0f;
    const SDL_Color flash = fx_mix(FX_LAMP, FX_CREAM, 0.45f);

    SDL_Color deck_far = fx_mix(low, FX_SHADOW, 0.45f);
    SDL_Color deck_mid = fx_mix(FX_SHADOW, FX_BASE, 0.35f);
    SDL_Color deck_near = fx_mix(FX_NIGHT, FX_SHADOW, 0.40f);
    SDL_Color under = fx_mix(low, FX_SODIUM, 0.18f);
    key_art_cloud_deck(r, win_w, 150.0f, 244.0f, 0x46415252u, deck_far,
                       fx_mix(under, FX_PALE, 0.10f), flash, bolt_x, strike);
    key_art_cloud_deck(r, win_w, 70.0f, 186.0f, 0x4d494444u, deck_mid,
                       fx_mix(deck_mid, under, 0.55f), flash, bolt_x - 60.0f,
                       strike);
    key_art_cloud_deck(r, win_w, 0.0f, 92.0f, 0x544f5020u, deck_near,
                       fx_mix(deck_near, under, 0.45f), flash, bolt_x - 90.0f,
                       strike * 0.8f);
    fx_glow(r, bolt_x - 60.0f, 150.0f, 230.0f, flash, (Uint8)(60.0f * strike));
    fx_glow(r, bolt_x - 40.0f, 170.0f, 110.0f, flash, (Uint8)(70.0f * strike));

    /* The bolt: a random walk down from the cloud base to the city, in a
     * wide dim stroke under a hot core, with one fork. Hashed rather than
     * drawn from the RNG, so the same night strikes in the same place. */
    float x = bolt_x;
    float y = 200.0f;
    float fork_x = 0.0f, fork_y = 0.0f;
    SDL_Color hot = fx_mix(FX_CREAM, FX_LAMP, 0.25f);
    /* It comes down behind the roof, and the roof is where it stops. */
    for (unsigned step = 0; y < KEY_ART_ROOF_Y * KEY_ART_ZOOM; ++step)
    {
        unsigned h = scene_hash(step * 40503u + 0x424f4c54u);
        float nx = x + (float)((int)(h % 19u) - 9) * 1.3f;
        float ny = y + 7.0f + (float)((h >> 8) % 7u);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, hot.r, hot.g, hot.b, (Uint8)(60.0f * strike));
        for (int w = -2; w <= 3; ++w)
            SDL_RenderLine(r, x + (float)w, y, nx + (float)w, ny);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        set_color(r, fx_mix(fx_mix(FX_NIGHT, hot, 0.35f), hot, strike));
        SDL_RenderLine(r, x, y, nx, ny);
        SDL_RenderLine(r, x + 1.0f, y, nx + 1.0f, ny);
        if (step == 4u)
        {
            fork_x = nx;
            fork_y = ny;
        }
        x = nx;
        y = ny;
    }
    x = fork_x;
    y = fork_y;
    for (unsigned step = 0; step < 6u; ++step)
    {
        unsigned h = scene_hash(step * 69069u + 0x464f524bu);
        float nx = x + 6.0f + (float)(h % 7u);
        float ny = y + 5.0f + (float)((h >> 8) % 6u);
        set_color(r, fx_mix(fx_mix(FX_NIGHT, hot, 0.3f), hot, strike * 0.7f));
        SDL_RenderLine(r, x, y, nx, ny);
        x = nx;
        y = ny;
    }
}

/*
 * The city, forty floors down.
 *
 * The camera is level with the roof, so the horizon is too, and every other
 * building in the city is under it: rows of roofs and lit windows running away
 * to the haze, each row smaller and paler than the one in front, with the
 * streets between them showing as sodium glow in the gaps. It is the title
 * screen's skyline looked down on instead of up at, and looking down on it is
 * the whole point — nothing a flat picture can do says "a long way down" as
 * plainly as a city with its roofs below you. The tower's own foot is under
 * the bottom of the frame, and the cordon at it is a red and blue light coming
 * up out of the dark.
 */
#define KEY_ART_HORIZON_Y 306.0f
#define KEY_ART_VANISH_X 250.0f
/* How far under the camera the street is, in the units a building's height
 * is measured in: every building in the city is shorter than this one. */
#define KEY_ART_EYE_HEIGHT 260.0f

static void key_art_city(SDL_Renderer *r, float time, int win_w, int win_h)
{
    static const float rows[] = {16.0f, 12.5f, 10.0f, 8.2f, 6.8f, 5.7f,
                                 4.8f,  4.05f, 3.4f,  2.9f, 2.45f, 2.08f,
                                 1.78f, 1.52f, 1.3f,  1.12f};
    const float hy = KEY_ART_HORIZON_Y;
    const float eye = KEY_ART_EYE_HEIGHT;
    SDL_Color haze = fx_mix(FX_BASE, FX_CYAN_DK, 0.34f);

    color_rect(r, fx_mix(haze, FX_NIGHT, 0.3f), 0.0f, hy, (float)win_w,
               (float)win_h - hy);

    for (int k = 0; k < (int)SDL_arraysize(rows); ++k)
    {
        float d = rows[k];
        float street_y = hy + eye / d;
        float mist = clamp01((d - 1.0f) / 13.0f);
        SDL_Color wall = fx_mix(fx_mix(FX_NIGHT, FX_SHADOW, 0.6f), haze,
                                0.25f + mist * 0.7f);
        SDL_Color roof = fx_mix(wall, FX_PALE, 0.16f + (1.0f - mist) * 0.12f);

        /* The street this row stands on, glowing in whatever gaps the row in
         * front of it leaves. */
        fx_rect_a(r, FX_SODIUM, (Uint8)(50.0f + 90.0f * (1.0f - mist)), 0.0f,
                  floorf(street_y) - 2.0f, (float)win_w, 2.0f);
        fx_vgrad(r, 0.0f, street_y - 14.0f / d - 4.0f, (float)win_w,
                 14.0f / d + 2.0f, FX_SODIUM, 0, FX_SODIUM,
                 (Uint8)(24.0f + 40.0f * (1.0f - mist)));

        float u = -1400.0f - (float)(k % 3) * 37.0f;
        for (unsigned i = 0; u < 1400.0f; ++i)
        {
            unsigned h = scene_hash((unsigned)k * 7919u + i * 104729u +
                                    0x43495459u);
            float w = 36.0f + (float)(h % 70u);
            float tall = 24.0f + (float)((h >> 8) % 150u);
            float x = KEY_ART_VANISH_X + u / d;
            float sw = w / d;
            u += w + 10.0f + (float)((h >> 16) % 22u);
            if (x + sw < 0.0f || x > (float)win_w)
                continue;
            float top = hy + (eye - tall) / d;
            float bottom = street_y;
            color_rect(r, wall, floorf(x), floorf(top), ceilf(sw),
                       ceilf(bottom - top) + 1.0f);
            /* Looked down on, a roof is a lit slab and its parapet. */
            float slab = fmaxf(1.0f, floorf(8.0f / d));
            color_rect(r, roof, floorf(x), floorf(top), ceilf(sw), slab);
            if ((h >> 24) % 4u == 0u && sw > 6.0f)
                color_rect(r, fx_dim(FX_RED, 0.55f + 0.45f * (1.0f - mist)),
                           floorf(x + sw * 0.5f), floorf(top) - 1.0f, 1.0f,
                           1.0f);

            /* Windows, a floor at a time, as many as the row's distance
             * leaves room for. */
            float pitch_y = 8.0f / d;
            float pitch_x = 7.0f / d;
            if (pitch_y < 1.6f)
            {
                pitch_y = 2.0f;
                pitch_x = 2.0f;
            }
            for (float wy = top + slab + pitch_y * 0.5f; wy < bottom - 1.0f;
                 wy += pitch_y)
            {
                for (float wx = x + pitch_x * 0.5f; wx < x + sw - 1.0f;
                     wx += pitch_x)
                {
                    unsigned wh = scene_hash(h + fx_salt(wx) * 31u +
                                             fx_salt(wy) * 977u);
                    if (wh % 4u != 0u)
                        continue;
                    SDL_Color c = (wh & 0x100u) ? FX_WARM : FX_LAMP;
                    float lit = 0.30f + 0.55f * (1.0f - mist);
                    float pw = fmaxf(1.0f, floorf(3.0f / d));
                    color_rect(r, fx_mix(wall, c, lit), floorf(wx), floorf(wy),
                               pw, fmaxf(1.0f, floorf(3.0f / d)));
                }
            }
        }
        /* Each row is one step further into the haze than the row in front
         * of it: the far city is mostly light and very little building. */
        if (k + 1 < (int)SDL_arraysize(rows))
        {
            float next = hy + eye / rows[k + 1];
            fx_vgrad(r, 0.0f, hy, (float)win_w, next - hy, haze,
                     (Uint8)(30.0f * mist), haze, 0);
        }
    }

    /* Haze at the horizon, where the city dissolves into its own glow. */
    fx_vgrad(r, 0.0f, hy - 34.0f, (float)win_w, 34.0f, haze, 0, haze, 190);
    fx_vgrad(r, 0.0f, hy, (float)win_w, 26.0f, haze, 190, haze, 0);

    /* The cordon at the foot of the tower, far under the frame. */
    float pulse = 0.5f + 0.5f * sinf(time * 6.0f);
    fx_glow(r, 470.0f, (float)win_h + 30.0f, 170.0f, FX_RED,
            (Uint8)(30.0f + 40.0f * pulse));
    fx_glow(r, 390.0f, (float)win_h + 34.0f, 180.0f, FX_CORDON_BLUE,
            (Uint8)(30.0f + 40.0f * (1.0f - pulse)));
}

/*
 * The top of the tower, in zoomed units: the coping every figure in the shot
 * is on or hanging from, the curtain wall dropping away under it with the
 * night's lit offices, and the corner the searchlight is on.
 */
static void key_art_tower(SDL_Renderer *r, float right, float bottom)
{
    const float x0 = KEY_ART_CORNER_X;
    const float roof = KEY_ART_ROOF_Y;
    const SDL_Color concrete = fx_mix(FX_STEEL_DK, FX_STEEL, 0.35f);
    const SDL_Color wall = fx_mix(FX_SHADOW, FX_BASE, 0.55f);

    /* The wall, and the offices behind it: warm and cool tubes, one blind
     * half down, and a man at one of them looking at the same thing. */
    color_rect(r, wall, x0, roof, right - x0, bottom - roof);
    const float floor_h = 31.0f;
    const float bay = 28.0f;
    const float pane_w = 19.0f;
    const float pane_h = 20.0f;
    for (int fl = 0; roof + 21.0f + (float)fl * floor_h < bottom; ++fl)
    {
        float wy = roof + 21.0f + (float)fl * floor_h;
        color_rect(r, fx_mix(wall, FX_STEEL_DK, 0.35f), x0, wy - 6.0f,
                   right - x0, 2.0f);
        for (int b = 0; x0 + 9.0f + (float)b * bay < right; ++b)
        {
            float wx = x0 + 9.0f + (float)b * bay;
            unsigned h = scene_hash((unsigned)(fl * 31 + b * 7) + 0x4b455941u);
            color_rect(r, FX_INK, wx - 1.0f, wy - 1.0f, pane_w + 2.0f,
                       pane_h + 2.0f);
            bool lit = h % 5u < 2u;
            SDL_Color glass = lit ? ((h & 8u) ? fx_dim(FX_WARM, 0.72f)
                                              : fx_dim(FX_LAMP, 0.58f))
                                  : fx_mix(FX_NIGHT, FX_SHADOW, 0.6f);
            float fade = clamp01((wy - roof) / (bottom - roof));
            glass = fx_mix(glass, wall, 0.25f + fade * 0.6f);
            color_rect(r, glass, wx, wy, pane_w, pane_h);
            color_rect(r, fx_mix(glass, FX_INK, 0.35f), wx + 9.0f, wy, 1.0f,
                       pane_h);
            if (lit && (h & 0x30u) == 0x30u)
                color_rect(r, fx_dim(glass, 0.6f), wx, wy, pane_w, 8.0f);
            if (lit && (h & 0x1c0u) == 0x40u)
            {
                /* Somebody at the glass. */
                color_rect(r, FX_INK, wx + 4.0f, wy + 6.0f, 5.0f, 14.0f);
                color_rect(r, FX_INK, wx + 5.0f, wy + 3.0f, 3.0f, 4.0f);
            }
            if (lit)
                fx_rect_a(r, FX_CREAM, 34, wx, wy, pane_w, 1.0f);
        }
    }
    /* The wet face darkens as it drops away, and forty floors down the
     * cordon's lights are coming up it. */
    fx_vgrad(r, x0, roof + 12.0f, right - x0, bottom - roof, FX_NIGHT, 30,
             FX_NIGHT, 200);
    fx_vgrad(r, x0, bottom - 75.0f, right - x0, 75.0f, FX_CORDON_BLUE, 0,
             FX_CORDON_BLUE, 44);
    fx_glow(r, x0 + (right - x0) * 0.7f, bottom + 12.0f, 88.0f, FX_RED, 46);

    /* The coping and the parapet under it. */
    color_rect(r, FX_INK, x0 - 1.0f, roof - 1.0f, right - x0 + 1.0f, 15.0f);
    color_rect(r, fx_dim(concrete, 0.7f), x0, roof + 3.0f, right - x0, 10.0f);
    color_rect(r, concrete, x0, roof, right - x0, 4.0f);
    color_rect(r, fx_mix(concrete, FX_PALE, 0.55f), x0, roof, right - x0, 1.0f);
    for (float jx = x0 + 32.0f; jx < right; jx += 50.0f)
        color_rect(r, fx_dim(concrete, 0.45f), jx, roof + 4.0f, 1.0f, 9.0f);
    fx_vgrad(r, x0, roof + 14.0f, right - x0, 12.0f, FX_INK, 150, FX_INK, 0);

    /* The corner: the arris the searchlight grazes, bright at the top. */
    fx_vgrad(r, x0, roof, 2.0f, bottom - roof, FX_PALE, 120, FX_PALE, 0);
}

/* The ship holding its hover, `k` times the outro's size and drawn at the
 * frame's own pixel: the airframe the outro flies, with the rotor as what a
 * spinning rotor is seen as edge-on — a thin blur of disc. */
static void key_art_helicopter(SDL_Renderer *r, float x, float y, float k,
                               float time)
{
    const float angle = -0.06f;
    const SDL_Color blur = {30, 38, 40, 255};
    draw_helicopter_hull(r, x, y, angle, 0.0f, k);

    float rotor_x = 0.0f, rotor_y = 0.0f;
    rotate_local(x, y, 0.0f, -31.0f * k, angle, &rotor_x, &rotor_y);
    const float span = floorf(96.0f * k);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (float d = -span; d <= span; d += 1.0f)
    {
        float t = fabsf(d) / span;
        set_rgba(r, blur.r, blur.g, blur.b, (Uint8)(150.0f * (1.0f - t * t)));
        fill_rect(r, floorf(rotor_x + d), floorf(rotor_y - 1.0f - d * 0.06f),
                  1.0f, 2.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    color_rect(r, FX_INK, floorf(rotor_x - 5.0f), floorf(rotor_y - 2.0f),
               10.0f, 4.0f);

    float tail_x = 0.0f, tail_y = 0.0f;
    rotate_local(x, y, 100.0f * k, -8.0f * k, angle, &tail_x, &tail_y);
    fx_glow(r, tail_x, tail_y, 14.0f * k, blur, 150);

    /* Nav light on the belly, and the searchlight under the nose: the
     * brightest point in the sky, because everything lit on the roof is lit
     * from here. */
    float beacon = fmodf(time * 1.35f, 1.0f) < 0.5f ? 1.0f : 0.35f;
    float nav_x = floorf(x - 2.0f * k);
    float nav_y = floorf(y + 12.0f * k);
    color_rect(r, fx_dim(FX_RED, beacon), nav_x, nav_y, 4.0f, 3.0f);
    fx_glow(r, nav_x + 2.0f, nav_y + 1.0f, 12.0f * k, FX_RED,
            (Uint8)(90.0f * beacon));
    float lamp_x = floorf(x - 36.0f * k);
    float lamp_y = floorf(y + 12.0f * k);
    color_rect(r, FX_INK, lamp_x - 5.0f, lamp_y - 4.0f, 10.0f, 8.0f);
    color_rect(r, FX_CREAM, lamp_x - 4.0f, lamp_y - 3.0f, 8.0f, 6.0f);
    fx_glow(r, lamp_x, lamp_y, 26.0f * k, FX_CREAM, 150);
    fx_glow(r, lamp_x, lamp_y, 10.0f * k, FX_CREAM, 220);
}

/* Voss at the edge: his pistol down along `aim` at the man under it, his
 * other hand closed on `grab`, both given in his own sprite units. */
static void key_art_voss(SDL_Renderer *r, float x, float ground_y,
                         float scale, int dir, float aim_dx, float aim_dy,
                         float grab_x, float grab_y)
{
    CastFrame f = {r, x, ground_y - 32.0f * scale, 28.0f, dir, scale};
    SDL_Color trouser = {46, 48, 52, 255};
    SDL_Color shoe = {28, 22, 20, 255};

    fx_contact_shadow(r, x + 14.0f * scale, ground_y - 2.0f, 12.0f * scale,
                      0.0f, 190);
    /* Braced wide at the lip: the front foot forward, the back one set. */
    cast_leg(&f, 11.5f, 21.5f, 8.2f, CAST_ANKLE_Y, fx_dim(trouser, 0.8f),
             shoe);
    cast_leg(&f, 15.0f, 21.5f, 18.4f, CAST_ANKLE_Y, trouser, shoe);

    float rear_hx = grab_x;
    float rear_hy = grab_y;
    cast_arm(&f, 13.0f, 13.5f, &rear_hx, &rear_hy, fx_dim(VOSS_COAT, 0.72f),
             fx_dim(VOSS_COAT, 0.72f), fx_dim(VOSS_SKIN, 0.75f), true);

    draw_voss_coat_and_head(&f, 0.0f);

    float len = sqrtf(aim_dx * aim_dx + aim_dy * aim_dy);
    float ux = aim_dx / len;
    float uy = aim_dy / len;
    float near_hx = 14.5f + ux * 8.6f;
    float near_hy = 13.5f + uy * 8.6f;
    cast_arm(&f, 14.5f, 13.5f, &near_hx, &near_hy, VOSS_COAT, VOSS_COAT,
             VOSS_SKIN, false);
    cast_pistol_angled(&f, near_hx, near_hy, ux, uy, 5.0f);
    cast_hand(&f, near_hx, near_hy, VOSS_SKIN);
}

/*
 * Chuck coming over the lip: forearms flat on the coping, head and shoulders
 * up over the roof, the rest of him hanging out over forty floors of air with
 * a knee against the wall for purchase — and his face level with the muzzle.
 * It is the sector's skeleton, posed, not a second drawing of him, so the man
 * on the store page is the man in the game. `lip_x`/`lip_y` is the corner of
 * the coping he is holding.
 */
static void key_art_chuck(SDL_Renderer *r, float lip_x, float lip_y,
                          float scale, float time)
{
    /* The corner of the coping, in his own sprite units: level with his
     * armpits, a unit ahead of his chest. */
    const ChuckPoint lip = {19.5f, 14.0f};
    ChuckPose pose;

    chuck_pose_stand(&pose, 0.0f);
    pose.lean = 0.5f;
    pose.ankle[CHUCK_FAR] = (ChuckPoint){CHUCK_ROOT_X - 1.2f, 30.9f};
    pose.pitch[CHUCK_FAR] = 0.8f;
    pose.ankle[CHUCK_NEAR] = (ChuckPoint){CHUCK_ROOT_X + 5.8f, 27.2f};
    pose.pitch[CHUCK_NEAR] = 0.2f;
    pose.tail = 1.0f;
    chuck_pose_solve(&pose);
    chuck_pose_reach(&pose, CHUCK_NEAR, (ChuckPoint){lip.x + 4.8f, lip.y - 0.6f});
    chuck_pose_reach(&pose, CHUCK_FAR, (ChuckPoint){lip.x + 3.0f, lip.y - 0.5f});

    ChuckView view = {r, lip_x - lip.x * scale, lip_y - lip.y * scale, 1,
                      scale, FX_INK};
    chuck_draw_arm(&view, &pose, CHUCK_FAR, CHUCK_HAND_GRIP);
    chuck_draw_legs(&view, &pose);
    chuck_draw_torso(&view, &pose);
    chuck_draw_head_as(&view, &pose, fx_blinking(time, 0x1u),
                       CHUCK_FACE_FURY);
    chuck_draw_arm(&view, &pose, CHUCK_NEAR, CHUCK_HAND_GRIP);
}

/* A searchlight from the ship: brightest where it leaves the lamp, spread
 * and paler by the time it lands, and laid over the wall it lands on. */
static void key_art_beam(SDL_Renderer *r, float ax, float ay, float tx,
                         float ty, float half, SDL_Color c)
{
    float dx = tx - ax;
    float dy = ty - ay;
    float len = sqrtf(dx * dx + dy * dy);
    float nx = -dy / len;
    float ny = dx / len;
    SDL_Vertex v[4] = {
        {{ax - nx * 3.0f, ay - ny * 3.0f}, fx_fcolor(c, 0.46f), {0.0f, 0.0f}},
        {{ax + nx * 3.0f, ay + ny * 3.0f}, fx_fcolor(c, 0.46f), {0.0f, 0.0f}},
        {{tx + nx * half, ty + ny * half}, fx_fcolor(c, 0.16f), {0.0f, 0.0f}},
        {{tx - nx * half, ty - ny * half}, fx_fcolor(c, 0.16f), {0.0f, 0.0f}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void key_art_render(SDL_Renderer *r, float time, int win_w, int win_h)
{
    const float z = KEY_ART_ZOOM;
    const float roof = KEY_ART_ROOF_Y;
    const float corner = KEY_ART_CORNER_X;
    const SDL_Color beam = {236, 244, 248, 255};

    key_art_sky(r, time, win_w, win_h);
    key_art_city(r, time, win_w, win_h);

    /* The ship is further off than the roof, so it keeps a finer pixel. */
    const float hz = KEY_ART_HELI_SCALE;
    key_art_helicopter(r, KEY_ART_HELI_X, KEY_ART_HELI_Y, hz, time);

    /* Its light, laid on the roof before the roof is drawn, so the parapet
     * is what stops it. */
    float spot_x = (corner + 22.5f) * z;
    float spot_y = (roof - 7.5f) * z;
    key_art_beam(r, KEY_ART_HELI_X - 36.0f * hz, KEY_ART_HELI_Y + 12.0f * hz,
                 spot_x, spot_y + 40.0f, 130.0f, beam);

    /* The near planes, at the coarser pixel. */
    SDL_SetRenderScale(r, z, z);
    key_art_tower(r, (float)win_w / z, (float)win_h / z);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
    fx_glow(r, spot_x, spot_y - 20.0f, 160.0f, beam, 34);
    fx_glow(r, spot_x + 10.0f, roof * z, 90.0f, beam, 60);

    SDL_SetRenderScale(r, z, z);
    const float voss_x = corner + 3.0f;
    const float ellen_x = corner + 47.0f;
    draw_hostage(r, ellen_x, roof, KEY_ART_ELLEN_SCALE, 0.0f, -1, true);
    /* Her mouth open on his name: the drawing's set mouth is the film's, for
     * a woman being walked, and this one is watching him hang. */
    sprite_rect(r, ellen_x, roof - 34.0f * KEY_ART_ELLEN_SCALE, 26.0f, -1,
                KEY_ART_ELLEN_SCALE, 16.6f, 8.6f, 2.0f, 1.6f,
                (SDL_Color){58, 24, 22, 255});
    /* Chuck before Voss: the muzzle is over his face, not behind it. */
    key_art_chuck(r, corner, roof, KEY_ART_CHUCK_SCALE, time);
    key_art_voss(r, voss_x, roof, KEY_ART_VOSS_SCALE, -1, 0.88f, 0.48f, 2.5f,
                 16.5f);
    SDL_SetRenderScale(r, 1.0f, 1.0f);

    const WetLight lights[] = {{spot_x, 160.0f, beam, 1.0f, 170.0f, 0.0f}};
    render_rain(r, time, win_w, win_h, lights, (int)SDL_arraysize(lights));
}
