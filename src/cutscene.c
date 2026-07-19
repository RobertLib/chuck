/*
 * Chuck's opening is rendered entirely from the same hard-edged primitives
 * as the rest of the game. It establishes the hostage situation before the
 * title screen without adding an external asset pipeline.
 */
#include "cutscene.h"

#include <math.h>

#include "fx.h"

static const SDL_Color COL_INK = {5, 7, 12, 255};
static const SDL_Color COL_NIGHT = {10, 14, 23, 255};
static const SDL_Color COL_CREAM = {236, 238, 224, 255};
static const SDL_Color COL_RUST = {186, 54, 46, 255};
static const SDL_Color COL_AMBER = {248, 188, 74, 255};
static const SDL_Color COL_CYAN = {74, 222, 212, 255};
static const float TRANSITION_DOOR_TOP = 358.0f;
static const float TRANSITION_DOOR_INNER_TOP = 368.0f;
static const float TRANSITION_DOOR_DEPTH_TOP = 376.0f;

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

bool opening_cutscene_update(OpeningCutscene *cutscene, float dt,
                             Uint32 *out_cues)
{
    static const float escort_steps_a[] = {4.22f, 4.92f, 5.62f, 6.32f};
    static const float escort_steps_b[] = {4.57f, 5.27f, 5.97f, 6.67f};
    static const float chuck_steps_a[] = {
        7.15f, 7.67f, 8.19f, 8.71f, 9.23f, 9.75f, 10.27f, 10.79f};
    static const float chuck_steps_b[] = {
        7.41f, 7.93f, 8.45f, 8.97f, 9.49f, 10.01f, 10.53f};

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
    if (crossed_time(previous, current, 6.82f) ||
        crossed_time(previous, current, 6.98f))
        cues |= OPENING_CUE_CAR_DOOR;
    if (crossed_any_time(previous, current, chuck_steps_a,
                         (int)SDL_arraysize(chuck_steps_a)))
        cues |= OPENING_CUE_CHUCK_STEP_A;
    if (crossed_any_time(previous, current, chuck_steps_b,
                         (int)SDL_arraysize(chuck_steps_b)))
        cues |= OPENING_CUE_CHUCK_STEP_B;
    if (crossed_time(previous, current, 10.63f))
        cues |= OPENING_CUE_BUILDING_DOOR;

    cutscene->time = current;
    if (out_cues != NULL)
        *out_cues = cues;
    return cutscene->time >= OPENING_CUTSCENE_DURATION;
}

static void draw_window(SDL_Renderer *r, float x, float y,
                        float w, float h, bool lit, unsigned variation)
{
    color_rect(r, (SDL_Color){5, 10, 17, 255}, x, y, w, h);
    if (lit)
    {
        SDL_Color light = (variation & 1u)
                              ? (SDL_Color){112, 106, 72, 255}
                              : (SDL_Color){48, 86, 94, 255};
        color_rect(r, light, x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f);
        color_rect(r, (SDL_Color){154, 145, 94, 255},
                   x + 3.0f, y + 2.0f, w - 6.0f, 1.0f);
    }
    else
    {
        color_rect(r, (SDL_Color){15, 25, 35, 255},
                   x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f);
    }
}

static void render_city(SDL_Renderer *r, float time, int win_w, int win_h)
{
    /* Smooth night gradient with a storm-lit teal horizon. */
    color_rect(r, COL_NIGHT, 0.0f, 0.0f, (float)win_w, (float)win_h);
    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, (float)win_h,
             (SDL_Color){6, 9, 16, 255}, 255,
             (SDL_Color){18, 28, 40, 255}, 255);

    /* Sparse stars remain visible above the high-rise through the rain. */
    for (unsigned i = 0; i < 58u; ++i)
    {
        unsigned h = scene_hash(i + 31u);
        float x = (float)(h % (unsigned)win_w);
        float y = 25.0f + (float)((h >> 8) % 205u);
        float blink = 0.45f + 0.55f *
                                  sinf(time * (0.5f + (float)(i % 4u) * 0.17f) +
                                       (float)(h & 31u));
        SDL_Color star = {(Uint8)(120.0f + blink * 55.0f),
                          (Uint8)(134.0f + blink * 52.0f),
                          (Uint8)(143.0f + blink * 55.0f), 255};
        color_rect(r, star, x, y, i % 9u == 0u ? 2.0f : 1.0f, 1.0f);
    }

    /* Distant blocks make the tower feel embedded in a city. */
    for (int i = 0; i < 9; ++i)
    {
        float x = (float)(i * 104 - 35);
        float h = 88.0f + (float)((i * 37) % 105);
        float y = 432.0f - h;
        color_rect(r, (SDL_Color){12, 20, 29, 255}, x, y, 86.0f, h);
        color_rect(r, (SDL_Color){25, 35, 43, 255}, x + 3.0f, y, 3.0f, h);
        for (int row = 0; row < (int)h - 24; row += 21)
        {
            for (int col = 0; col < 3; ++col)
            {
                unsigned wh = scene_hash((unsigned)(i * 71 + row + col));
                if ((wh & 7u) == 0u)
                    color_rect(r, (SDL_Color){73, 70, 48, 255},
                               x + 15.0f + col * 21.0f, y + 15.0f + row,
                               7.0f, 3.0f);
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

    color_rect(r, (SDL_Color){3, 6, 10, 170},
               x + 12.0f, y + 9.0f, w, ground - y);
    color_rect(r, (SDL_Color){31, 40, 48, 255}, x, y, w, ground - y);
    color_rect(r, (SDL_Color){57, 68, 74, 255}, x, y, w, 5.0f);
    color_rect(r, (SDL_Color){20, 28, 36, 255},
               x + 7.0f, y + 8.0f, w - 14.0f, ground - y - 8.0f);

    for (int row = 0; row < 10; ++row)
    {
        float wy = y + 20.0f + row * 34.0f;
        color_rect(r, (SDL_Color){51, 61, 66, 255},
                   x + 8.0f, wy + 21.0f, w - 16.0f, 3.0f);
        for (int col = 0; col < 7; ++col)
        {
            unsigned h = scene_hash((unsigned)(row * 29 + col * 7 + 19));
            bool lit = (h % 6u) == 0u;
            /* One office light flickers subtly during the establishing shot. */
            if (row == 2 && col == 4)
                lit = fmodf(time, 2.8f) < 2.2f;
            draw_window(r, x + 18.0f + col * 45.0f, wy,
                        30.0f, 19.0f, lit, h);
        }
    }

    /* Street-level stone, canopy, and a deep entrance opening. */
    color_rect(r, (SDL_Color){48, 53, 53, 255},
               x - 10.0f, ground - 75.0f, w + 20.0f, 75.0f);
    color_rect(r, (SDL_Color){84, 87, 80, 255},
               x - 13.0f, ground - 78.0f, w + 26.0f, 5.0f);
    color_rect(r, (SDL_Color){14, 20, 25, 255},
               x + 205.0f, ground - 68.0f, 112.0f, 68.0f);
    color_rect(r, (SDL_Color){6, 11, 16, 255},
               x + 214.0f, ground - 59.0f, 94.0f, 59.0f);
    color_rect(r, (SDL_Color){69, 83, 85, 255},
               x + 259.0f, ground - 59.0f, 4.0f, 59.0f);
    color_rect(r, (SDL_Color){118, 128, 116, 255},
               x + 205.0f, ground - 68.0f, 112.0f, 4.0f);
    color_rect(r, COL_AMBER, x + 220.0f, ground - 76.0f, 82.0f, 3.0f);
    fx_glow(r, x + 261.0f, ground - 74.0f, 46.0f, COL_AMBER, 42);
    fx_light_cone(r, x + 261.0f, ground - 73.0f, 42.0f, 58.0f, 74.0f,
                  (SDL_Color){248, 205, 130, 255}, 26);

    draw_text(r, x + 226.0f, ground - 91.0f, 0.75f,
              (SDL_Color){159, 158, 140, 255}, "KESSLER TOWER");

    for (int i = 0; i < 3; ++i)
    {
        color_rect(r, (SDL_Color){36, 42, 43, 255},
                   x + 28.0f + i * 57.0f, ground - 50.0f, 38.0f, 50.0f);
        color_rect(r, (SDL_Color){74, 79, 75, 255},
                   x + 31.0f + i * 57.0f, ground - 46.0f, 32.0f, 4.0f);
    }

    /* Warning beacon above the tower. */
    float beacon = sinf(time * 4.6f) > 0.2f ? 1.0f : 0.24f;
    color_rect(r, (SDL_Color){42, 47, 48, 255},
               x + 278.0f, y - 9.0f, 31.0f, 9.0f);
    color_rect(r, (SDL_Color){(Uint8)(COL_RUST.r * beacon),
                              (Uint8)(COL_RUST.g * beacon),
                              (Uint8)(COL_RUST.b * beacon), 255},
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

static void draw_wheel(SDL_Renderer *r, float cx, float cy,
                       float scale, float rotation)
{
    float radius = 12.0f * scale;
    color_rect(r, COL_INK, cx - radius, cy - radius,
               radius * 2.0f, radius * 2.0f);
    color_rect(r, (SDL_Color){38, 44, 47, 255},
               cx - radius + 3.0f * scale, cy - radius + 3.0f * scale,
               radius * 2.0f - 6.0f * scale,
               radius * 2.0f - 6.0f * scale);
    color_rect(r, (SDL_Color){119, 124, 116, 255},
               cx - 3.0f * scale, cy - 3.0f * scale,
               6.0f * scale, 6.0f * scale);

    set_color(r, (SDL_Color){92, 99, 96, 255});
    float dx = cosf(rotation) * radius * 0.58f;
    float dy = sinf(rotation) * radius * 0.58f;
    SDL_RenderLine(r, cx - dx, cy - dy, cx + dx, cy + dy);
    SDL_RenderLine(r, cx + dy, cy - dx, cx - dy, cy + dx);
}

static void draw_suv(SDL_Renderer *r, float x, float ground_y,
                     float time, bool moving, bool door_open)
{
    const float y = ground_y - 51.0f;
    float rotation = moving ? time * 15.0f : 0.35f;

    if (moving)
        draw_headlight_beam(r, x + 146.0f, y + 33.0f, 106.0f, COL_CREAM);

    color_rect(r, (SDL_Color){3, 5, 8, 150},
               x - 6.0f, ground_y - 4.0f, 166.0f, 7.0f);
    color_rect(r, COL_INK, x, y + 20.0f, 151.0f, 31.0f);
    color_rect(r, (SDL_Color){30, 35, 38, 255},
               x + 4.0f, y + 16.0f, 142.0f, 31.0f);
    color_rect(r, (SDL_Color){47, 51, 49, 255},
               x + 13.0f, y + 4.0f, 91.0f, 20.0f);
    color_rect(r, (SDL_Color){12, 20, 26, 255},
               x + 18.0f, y + 7.0f, 34.0f, 16.0f);
    color_rect(r, (SDL_Color){9, 17, 23, 255},
               x + 57.0f, y + 7.0f, 39.0f, 16.0f);
    color_rect(r, (SDL_Color){82, 88, 82, 255},
               x + 6.0f, y + 19.0f, 135.0f, 3.0f);
    color_rect(r, (SDL_Color){18, 21, 22, 255},
               x + 66.0f, y + 23.0f, 2.0f, 21.0f);
    color_rect(r, (SDL_Color){18, 21, 22, 255},
               x + 105.0f, y + 23.0f, 2.0f, 21.0f);

    color_rect(r, COL_RUST, x + 2.0f, y + 29.0f, 6.0f, 8.0f);
    color_rect(r, moving ? COL_CREAM : (SDL_Color){149, 145, 113, 255},
               x + 142.0f, y + 28.0f, 7.0f, 7.0f);
    color_rect(r, (SDL_Color){93, 99, 94, 255},
               x + 119.0f, y + 41.0f, 19.0f, 4.0f);

    if (door_open)
    {
        color_rect(r, COL_INK, x + 67.0f, y + 23.0f, 38.0f, 26.0f);
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

    if (moving)
        draw_headlight_beam(r, x + 137.0f, y + 29.0f, 92.0f, COL_CREAM);

    color_rect(r, (SDL_Color){3, 5, 8, 150},
               x - 5.0f, ground_y - 4.0f, 151.0f, 7.0f);
    color_rect(r, COL_INK, x, y + 15.0f, 141.0f, 28.0f);
    color_rect(r, (SDL_Color){28, 70, 91, 255},
               x + 4.0f, y + 17.0f, 133.0f, 22.0f);
    color_rect(r, (SDL_Color){41, 101, 121, 255},
               x + 31.0f, y + 3.0f, 69.0f, 18.0f);
    color_rect(r, (SDL_Color){9, 20, 28, 255},
               x + 36.0f, y + 6.0f, 28.0f, 14.0f);
    color_rect(r, (SDL_Color){11, 24, 31, 255},
               x + 68.0f, y + 6.0f, 28.0f, 14.0f);
    color_rect(r, (SDL_Color){71, 143, 153, 255},
               x + 6.0f, y + 18.0f, 126.0f, 3.0f);
    color_rect(r, (SDL_Color){15, 43, 59, 255},
               x + 67.0f, y + 21.0f, 2.0f, 17.0f);

    if (occupied)
    {
        color_rect(r, (SDL_Color){191, 133, 91, 255},
                   x + 75.0f, y + 8.0f, 7.0f, 8.0f);
        color_rect(r, (SDL_Color){47, 27, 22, 255},
                   x + 74.0f, y + 6.0f, 9.0f, 4.0f);
        color_rect(r, COL_CREAM, x + 80.0f, y + 10.0f, 2.0f, 1.0f);
    }

    color_rect(r, COL_RUST, x + 3.0f, y + 25.0f, 6.0f, 7.0f);
    color_rect(r, COL_CREAM, x + 133.0f, y + 24.0f, 7.0f, 7.0f);
    color_rect(r, (SDL_Color){109, 133, 133, 255},
               x + 113.0f, y + 34.0f, 19.0f, 4.0f);

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

static void draw_agent(SDL_Renderer *r, float x, float ground_y,
                       float scale, float time, int dir)
{
    float stride = sinf(time * 12.0f);
    float y = ground_y - 32.0f * scale;
    float bob = fabsf(stride) * scale;

    color_rect(r, (SDL_Color){2, 4, 7, 150},
               x + 2.0f * scale, ground_y - 2.0f,
               24.0f * scale, 4.0f);

    sprite_rect(r, x, y + fabsf(stride) * scale, 28.0f, dir, scale,
                5 + stride * 1.6f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * scale, 28.0f, dir, scale,
                15 - stride * 1.6f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                5, 10, 17, 14, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                7, 11, 13, 12, (SDL_Color){35, 102, 142, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                8, 12, 4, 9, (SDL_Color){60, 148, 171, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                8, 20, 11, 2, COL_AMBER);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                9, 1, 11, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                10, 3, 9, 8, (SDL_Color){210, 154, 105, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                9, 1, 11, 4, (SDL_Color){70, 38, 28, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                18, 13, 11, 5, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                19, 14, 10, 3, (SDL_Color){209, 154, 105, 255});
}

static void draw_terrorist(SDL_Renderer *r, float x, float ground_y,
                           float scale, float time, float phase, int dir)
{
    float stride = sinf(time * 9.0f + phase);
    float y = ground_y - 32.0f * scale;
    float bob = fabsf(stride) * 0.7f * scale;

    color_rect(r, (SDL_Color){2, 4, 7, 155},
               x + 2.0f * scale, ground_y - 2.0f,
               24.0f * scale, 4.0f);
    sprite_rect(r, x, y + fabsf(stride) * scale, 28.0f, dir, scale,
                4 + stride, 21, 8, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * scale, 28.0f, dir, scale,
                15 - stride, 21, 8, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                4, 10, 19, 14, (SDL_Color){17, 21, 22, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                7, 12, 13, 9, (SDL_Color){49, 54, 49, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                7, 16, 13, 3, COL_RUST);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                8, 1, 12, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                10, 4, 9, 7, (SDL_Color){145, 103, 75, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                8, 1, 13, 5, (SDL_Color){24, 28, 27, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                17, 6, 2, 2, COL_RUST);

    /* Low-ready rifle makes the captors unmistakable at pixel scale. */
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                17, 13, 13, 4, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                21, 14, 13, 2, (SDL_Color){67, 73, 69, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale,
                18, 14, 6, 3, (SDL_Color){145, 103, 75, 255});
}

static void draw_hostage(SDL_Renderer *r, float x, float ground_y,
                         float scale, float time, int dir, bool tied)
{
    float step = sinf(time * 8.3f + 1.4f);
    float y = ground_y - 34.0f * scale;
    float bob = fabsf(step) * 0.55f * scale;
    float hair_sway = step * 0.25f;
    SDL_Color hair_dark = {76, 51, 35, 255};
    SDL_Color hair = {137, 94, 55, 255};
    SDL_Color skin = {224, 171, 128, 255};
    SDL_Color coat_dark = {91, 28, 37, 255};
    SDL_Color coat = {148, 42, 49, 255};
    SDL_Color coat_light = {183, 57, 60, 255};
    SDL_Color trousers = {39, 48, 58, 255};
    SDL_Color boots = {15, 19, 25, 255};

    color_rect(r, (SDL_Color){2, 4, 7, 145},
               x + 3.0f * scale, ground_y - 2.0f,
               20.0f * scale, 4.0f);

    /* Practical trousers and flat boots, animated with the same restrained step. */
    sprite_rect(r, x, y + fabsf(step) * scale, 26.0f, dir, scale,
                7 + step * 0.7f, 23, 5, 11, COL_INK);
    sprite_rect(r, x, y + fabsf(step) * scale, 26.0f, dir, scale,
                8 + step * 0.7f, 24, 3, 8, trousers);
    sprite_rect(r, x, y + fabsf(step) * scale, 26.0f, dir, scale,
                6 + step * 0.7f, 31, 7, 3, boots);
    sprite_rect(r, x, y + (1.0f - fabsf(step)) * scale, 26.0f, dir, scale,
                15 - step * 0.7f, 23, 5, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(step)) * scale, 26.0f, dir, scale,
                16 - step * 0.7f, 24, 3, 8, trousers);
    sprite_rect(r, x, y + (1.0f - fabsf(step)) * scale, 26.0f, dir, scale,
                14 - step * 0.7f, 31, 7, 3, boots);

    /* Shoulder-length hair provides the main readable cue at pixel scale. */
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                7 + hair_sway, 1, 13, 15, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8 + hair_sway, 2, 11, 13, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                7 - hair_sway, 7, 4, 9, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8 - hair_sway, 8, 2, 7, hair);

    /* Simple profile without makeup accents. */
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                10, 2, 10, 11, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                11, 3, 8, 8, skin);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                12, 10, 7, 2, skin);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8, 0, 12, 5, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                9, 1, 9, 2, hair);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8, 3, 4, 8, hair);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                17, 5, 2, 1, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                18, 9, 1, 1, hair_dark);

    /* Straight-cut red coat keeps her silhouette distinct but grounded. */
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                12, 11, 5, 4, skin);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                7, 12, 14, 15, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                8, 13, 12, 13, coat);
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
                    18, 14, 6, 5, COL_INK);
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
        /* Once safe, her arm rests naturally at her side. */
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    18, 14, 6, 12, COL_INK);
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    19, 15, 4, 10, coat);
        sprite_rect(r, x, y + bob, 26.0f, dir, scale,
                    19, 24, 4, 3, skin);
    }
}

static void draw_target_brackets(SDL_Renderer *r, float x, float y,
                                 float w, float h, float pulse)
{
    SDL_Color color = {(Uint8)(COL_RUST.r * (0.7f + pulse * 0.3f)),
                       (Uint8)(COL_RUST.g * (0.7f + pulse * 0.3f)),
                       (Uint8)(COL_RUST.b * (0.7f + pulse * 0.3f)), 255};
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

static void render_street(SDL_Renderer *r, float time, int win_w, int win_h)
{
    const float ground = 437.0f;
    color_rect(r, (SDL_Color){52, 55, 54, 255},
               0.0f, ground - 3.0f, (float)win_w, 8.0f);
    color_rect(r, (SDL_Color){18, 24, 29, 255},
               0.0f, ground + 5.0f, (float)win_w,
               (float)win_h - ground - 5.0f);
    color_rect(r, (SDL_Color){80, 82, 76, 255},
               0.0f, ground + 13.0f, (float)win_w, 2.0f);
    for (int i = 0; i < 8; ++i)
    {
        float x = (float)(i * 128 - 42);
        color_rect(r, (SDL_Color){118, 107, 70, 255},
                   x, ground + 70.0f, 64.0f, 3.0f);
    }

    /* Puddle reflections stretch the tower lights across wet asphalt. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 7; ++i)
    {
        float ripple = sinf(time * 1.6f + i) * 4.0f;
        set_rgba(r, i & 1 ? 67 : 166, i & 1 ? 113 : 116,
                 i & 1 ? 117 : 56, 24);
        fill_rect(r, 420.0f + i * 43.0f + ripple,
                  ground + 23.0f + (float)(i % 3) * 13.0f,
                  31.0f + (float)(i % 2) * 19.0f, 2.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void render_rain(SDL_Renderer *r, float time, int win_w, int win_h)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (unsigned i = 0; i < 72u; ++i)
    {
        unsigned h = scene_hash(i * 17u + 5u);
        float speed = 128.0f + (float)(h % 95u);
        float x = fmodf((float)(h % (unsigned)(win_w + 70)) -
                            time * (20.0f + (float)(i % 4u) * 7.0f),
                        (float)(win_w + 70));
        if (x < 0.0f)
            x += (float)(win_w + 70);
        x -= 25.0f;
        float y = fmodf((float)((h >> 9) % (unsigned)(win_h + 40)) +
                            time * speed,
                        (float)(win_h + 40)) -
                  20.0f;
        set_rgba(r, 132, 159, 170, (Uint8)(30 + (h % 35u)));
        SDL_RenderLine(r, x, y, x - 4.0f, y + 9.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void render_cinematic_ui(SDL_Renderer *r, float time,
                                int win_w, int win_h,
                                float target_x)
{
    if (time > 0.55f && time < 4.5f)
    {
        float reveal = smoothstep01((time - 0.55f) / 0.5f);
        draw_text(r, 35.0f, 38.0f, 1.0f,
                  (SDL_Color){(Uint8)(COL_CYAN.r * reveal),
                              (Uint8)(COL_CYAN.g * reveal),
                              (Uint8)(COL_CYAN.b * reveal), 255},
                  "23:47 // FINANCIAL DISTRICT");
        color_rect(r, COL_RUST, 35.0f, 53.0f, 52.0f * reveal, 2.0f);
    }

    if (time > 3.0f && time < 6.5f)
    {
        float pulse = 0.5f + 0.5f * sinf(time * 5.0f);
        draw_target_brackets(r, target_x - 8.0f, 371.0f,
                             167.0f, 72.0f, pulse);
        draw_text(r, target_x + 3.0f, 351.0f, 0.75f,
                  COL_RUST, "FIANCEE ABDUCTED // VEHICLE CONFIRMED");
    }

    if (time > 7.1f && time < 10.8f)
    {
        float reveal = smoothstep01((time - 7.1f) / 0.35f);
        draw_text(r, 35.0f, 38.0f, 1.0f,
                  (SDL_Color){(Uint8)(COL_AMBER.r * reveal),
                              (Uint8)(COL_AMBER.g * reveal),
                              (Uint8)(COL_AMBER.b * reveal), 255},
                  "CHUCK // RESCUE IN PROGRESS");
        color_rect(r, COL_RUST, 35.0f, 53.0f, 52.0f * reveal, 2.0f);
    }

    if (time > 0.9f && time < 11.15f)
    {
        float pulse = 0.45f + 0.55f * sinf(time * 2.0f);
        SDL_Color skip = {(Uint8)(100.0f + pulse * 42.0f),
                          (Uint8)(108.0f + pulse * 42.0f),
                          (Uint8)(106.0f + pulse * 38.0f), 255};
        draw_text(r, (float)win_w - 180.0f, (float)win_h - 31.0f,
                  0.75f, skip, "ENTER / SPACE TO SKIP");
    }
}

void opening_cutscene_render(SDL_Renderer *r,
                             const OpeningCutscene *cutscene,
                             int win_w, int win_h)
{
    const float time = cutscene->time;
    const float ground = 437.0f;

    render_city(r, time, win_w, win_h);
    render_tower(r, time, win_w);
    render_street(r, time, win_w, win_h);

    float suv_move = ease_out_cubic((time - 0.65f) / 2.65f);
    float suv_x = lerpf(-175.0f, 438.0f, suv_move);
    bool suv_moving = time < 3.30f;
    bool suv_door_open = time >= 3.75f && time < 7.0f;
    draw_suv(r, suv_x, ground, time, suv_moving, suv_door_open);

    float agent_car_move = ease_out_cubic((time - 1.45f) / 2.55f);
    float agent_car_x = lerpf(-170.0f, 92.0f, agent_car_move);
    bool agent_car_moving = time < 4.0f;
    bool agent_in_car = time < 7.0f;
    draw_agent_car(r, agent_car_x, ground, time,
                   agent_car_moving, agent_in_car);

    if (time >= 4.05f && time < 7.18f)
    {
        float group_move = smoothstep01((time - 4.05f) / 2.95f);
        float escort_one_x = lerpf(500.0f, 643.0f, group_move);
        float hostage_x = lerpf(525.0f, 668.0f, group_move);
        float escort_two_x = lerpf(550.0f, 693.0f, group_move);
        draw_terrorist(r, escort_one_x, ground, 1.35f, time, 0.0f, 1);
        draw_hostage(r, hostage_x, ground, 1.18f, time, 1, true);
        draw_terrorist(r, escort_two_x, ground, 1.35f, time, 2.2f, 1);
    }

    if (time >= 7.0f && time < 11.05f)
    {
        float run = smoothstep01((time - 7.0f) / 3.75f);
        float agent_x = lerpf(199.0f, 658.0f, run);
        draw_agent(r, agent_x, ground, 1.48f, time, 1);
    }

    render_rain(r, time, win_w, win_h);
    render_cinematic_ui(r, time, win_w, win_h, suv_x);

    /* Film finish: vignette and animated grain under the letterbox bars. */
    fx_vignette(r, win_w, win_h, 74);
    fx_grain(r, win_w, win_h, time, 26);

    /* Narrow letterbox bars frame the scene without hiding the gameplay art. */
    color_rect(r, (SDL_Color){3, 5, 8, 255},
               0.0f, 0.0f, (float)win_w, 19.0f);
    color_rect(r, (SDL_Color){3, 5, 8, 255},
               0.0f, (float)win_h - 19.0f, (float)win_w, 19.0f);

    float fade_in = 1.0f - smoothstep01(time / 0.68f);
    float fade_out = smoothstep01((time - 11.15f) / 1.15f);
    float fade = fmaxf(fade_in, fade_out);
    if (fade > 0.0f)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, 2, 4, 7, (Uint8)(fade * 255.0f));
        fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

void level_transition_init(LevelTransition *transition,
                           int completed_level, int next_level,
                           float elapsed_seconds, int level_score,
                           int hostiles_neutralized)
{
    SDL_zerop(transition);
    transition->completed_level = completed_level;
    transition->next_level = next_level;
    transition->elapsed_seconds = elapsed_seconds;
    transition->level_score = level_score;
    transition->hostiles_neutralized = hostiles_neutralized;
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
        (Uint8)(COL_CREAM.r * reveal),
        (Uint8)(COL_CREAM.g * reveal),
        (Uint8)(COL_CREAM.b * reveal), 255};
    SDL_Color muted = {
        (Uint8)(122.0f * line_reveal),
        (Uint8)(139.0f * line_reveal),
        (Uint8)(141.0f * line_reveal), 255};
    SDL_Color value = {
        (Uint8)(COL_AMBER.r * line_reveal),
        (Uint8)(COL_AMBER.g * line_reveal),
        (Uint8)(COL_AMBER.b * line_reveal), 255};
    char buffer[64];

    color_rect(r, (SDL_Color){7, 12, 18, 255},
               0.0f, 18.0f, (float)win_w, 133.0f);
    color_rect(r, (SDL_Color){25, 42, 49, 255},
               0.0f, 21.0f, (float)win_w, 3.0f);
    color_rect(r, (SDL_Color){34, 62, 65, 255},
               0.0f, 148.0f, (float)win_w, 3.0f);
    color_rect(r, (SDL_Color){
                      (Uint8)(COL_CYAN.r * reveal),
                      (Uint8)(COL_CYAN.g * reveal),
                      (Uint8)(COL_CYAN.b * reveal), 255},
               27.0f, 61.0f, 72.0f * reveal, 2.0f);

    draw_text(r, 27.0f, 34.0f, 1.75f, title, "PURSUIT CONTINUES");

    int elapsed = (int)transition->elapsed_seconds;
    int minutes = elapsed / 60;
    int seconds = elapsed % 60;
    draw_text(r, 27.0f, 88.0f, 0.75f, muted, "TIME");
    SDL_snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    draw_text(r, 27.0f, 106.0f, 1.25f, value, buffer);

    color_rect(r, (SDL_Color){31, 47, 52, 255},
               129.0f, 85.0f, 1.0f, 43.0f);
    draw_text(r, 151.0f, 88.0f, 0.75f, muted, "SCORE");
    SDL_snprintf(buffer, sizeof(buffer), "+%06d", transition->level_score);
    draw_text(r, 151.0f, 106.0f, 1.25f, value, buffer);

    color_rect(r, (SDL_Color){31, 47, 52, 255},
               282.0f, 85.0f, 1.0f, 43.0f);
    draw_text(r, 307.0f, 88.0f, 0.75f, muted, "HOSTILES");
    SDL_snprintf(buffer, sizeof(buffer), "%02d",
                 transition->hostiles_neutralized);
    draw_text(r, 307.0f, 106.0f, 1.25f, value, buffer);

    color_rect(r, (SDL_Color){31, 47, 52, 255},
               526.0f, 37.0f, 1.0f, 91.0f);
    draw_text(r, 558.0f, 45.0f, 0.75f, muted, "TRAIL LEADS TO");
    SDL_snprintf(buffer, sizeof(buffer), "SECTOR %02d",
                 transition->next_level + 1);
    draw_text(r, 558.0f, 69.0f, 1.75f, COL_CREAM, buffer);
}

static float transition_door_open(float time)
{
    float opening = smoothstep01((time - 3.72f) / 0.42f);
    float closing = smoothstep01((time - 8.10f) / 0.42f);
    return opening * (1.0f - closing);
}

static void render_transition_corridor(SDL_Renderer *r, float time,
                                       int win_w, int win_h,
                                       float door_x, float ground_y)
{
    color_rect(r, (SDL_Color){14, 22, 28, 255},
               0.0f, 151.0f, (float)win_w, (float)win_h - 151.0f);
    fx_vgrad(r, 0.0f, 151.0f, (float)win_w, (float)win_h - 151.0f,
             (SDL_Color){13, 20, 30, 255}, 255,
             (SDL_Color){24, 34, 44, 255}, 255);

    /* Repeating concrete bays and pipes echo the tower interior in the intro. */
    for (int i = 0; i < 8; ++i)
    {
        float x = 16.0f + i * 102.0f;
        color_rect(r, (SDL_Color){32, 43, 47, 255},
                   x, 188.0f, 4.0f, ground_y - 188.0f);
        color_rect(r, (SDL_Color){10, 16, 21, 255},
                   x + 5.0f, 188.0f, 2.0f, ground_y - 188.0f);
        if ((i & 1) == 0)
        {
            color_rect(r, (SDL_Color){47, 55, 54, 255},
                       x + 17.0f, 212.0f, 55.0f, 5.0f);
            color_rect(r, (SDL_Color){17, 26, 31, 255},
                       x + 21.0f, 218.0f, 47.0f, 31.0f);
        }
    }
    color_rect(r, (SDL_Color){53, 61, 59, 255},
               0.0f, 179.0f, (float)win_w, 7.0f);
    color_rect(r, (SDL_Color){20, 28, 31, 255},
               0.0f, 186.0f, (float)win_w, 4.0f);
    color_rect(r, (SDL_Color){78, 83, 75, 255},
               0.0f, ground_y - 5.0f, (float)win_w, 8.0f);
    color_rect(r, (SDL_Color){13, 18, 22, 255},
               0.0f, ground_y + 3.0f, (float)win_w,
               (float)win_h - ground_y - 3.0f);

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
        SDL_Color lamp = fx_dim((SDL_Color){248, 205, 130, 255}, flicker);
        color_rect(r, (SDL_Color){16, 21, 31, 255}, lx - 14.0f, 186.0f, 28.0f, 4.0f);
        color_rect(r, lamp, lx - 11.0f, 188.0f, 22.0f, 2.0f);
        fx_glow(r, lx, 190.0f, 18.0f, lamp, (Uint8)(60.0f * flicker));
        fx_light_cone(r, lx, 189.0f, 13.0f, 52.0f, 120.0f,
                      (SDL_Color){248, 205, 130, 255},
                      (Uint8)(22.0f * flicker));
    }

    /* The dark aperture is drawn before the actors so they can walk into it. */
    color_rect(r, (SDL_Color){54, 61, 59, 255},
               door_x - 11.0f, TRANSITION_DOOR_TOP,
               112.0f, ground_y - TRANSITION_DOOR_TOP);
    color_rect(r, (SDL_Color){4, 8, 12, 255},
               door_x, TRANSITION_DOOR_INNER_TOP,
               90.0f, ground_y - TRANSITION_DOOR_INNER_TOP);
    color_rect(r, (SDL_Color){12, 24, 29, 255},
               door_x + 8.0f, TRANSITION_DOOR_DEPTH_TOP,
               74.0f, ground_y - TRANSITION_DOOR_DEPTH_TOP);
    color_rect(r, (SDL_Color){42, 69, 70, 255},
               door_x + 43.0f, TRANSITION_DOOR_DEPTH_TOP,
               3.0f, ground_y - TRANSITION_DOOR_DEPTH_TOP);

    /*
     * Both sliding panels are part of the background layer. While boarding,
     * the actors therefore stay in front of every vertical part on the
     * elevator's left side.
     */
    float open = transition_door_open(time);
    float half_panel = 43.0f * (1.0f - open);
    if (half_panel > 0.0f)
    {
        color_rect(r, (SDL_Color){43, 50, 49, 255},
                   door_x, TRANSITION_DOOR_INNER_TOP,
                   half_panel, ground_y - TRANSITION_DOOR_INNER_TOP);
        color_rect(r, (SDL_Color){62, 68, 63, 255},
                   door_x + 90.0f - half_panel, TRANSITION_DOOR_INNER_TOP,
                   half_panel, ground_y - TRANSITION_DOOR_INNER_TOP);
        color_rect(r, COL_RUST,
                   door_x + half_panel - 3.0f, TRANSITION_DOOR_INNER_TOP,
                   3.0f, ground_y - TRANSITION_DOOR_INNER_TOP);
        color_rect(r, COL_RUST,
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

    draw_text(r, door_x + 36.0f, TRANSITION_DOOR_TOP - 17.0f, 0.75f,
              (SDL_Color){143, 151, 137, 255}, "02");
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

static void draw_agent_held_fire(SDL_Renderer *r, float x, float ground_y,
                                 float scale, float time, bool aiming, int dir)
{
    float y = ground_y - 32.0f * scale;
    float breath = sinf(time * 4.5f) * 0.35f * scale;

    color_rect(r, (SDL_Color){2, 4, 7, 150},
               x + 2.0f * scale, ground_y - 2.0f,
               28.0f * scale, 4.0f);
    sprite_rect(r, x, y, 30.0f, dir, scale,
                6, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y, 30.0f, dir, scale,
                16, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                5, 10, 18, 14, COL_INK);
    sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                7, 11, 14, 12, (SDL_Color){35, 102, 142, 255});
    sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                8, 12, 4, 9, (SDL_Color){60, 148, 171, 255});
    sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                8, 20, 12, 2, COL_AMBER);
    sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                9, 1, 11, 11, COL_INK);
    sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                10, 3, 9, 8, (SDL_Color){210, 154, 105, 255});
    sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                9, 1, 11, 4, (SDL_Color){70, 38, 28, 255});

    if (aiming)
    {
        sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                    18, 12, 13, 4, (SDL_Color){209, 154, 105, 255});
        sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                    27, 11, 10, 4, COL_INK);
        sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                    35, 12, 6, 2, (SDL_Color){77, 84, 81, 255});
    }
    else
    {
        /* He lowers the pistol rather than taking the obstructed shot. */
        sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                    18, 13, 8, 8, (SDL_Color){209, 154, 105, 255});
        sprite_rect(r, x, y + breath, 30.0f, dir, scale,
                    23, 19, 5, 9, COL_INK);
    }
}

static void render_transition_action_ui(SDL_Renderer *r, float time,
                                        float hostage_x, float ground_y,
                                        int win_w, int win_h)
{
    if (time >= 2.18f && time < 3.72f)
    {
        float reveal = smoothstep01((time - 2.18f) / 0.25f);
        float pulse = 0.5f + 0.5f * sinf(time * 6.0f);
        float target_x = hostage_x + 10.0f;
        draw_target_brackets(r, target_x - 10.0f,
                             ground_y - 55.0f, 52.0f, 61.0f, pulse);
        draw_text(r, 35.0f, 192.0f, 0.75f,
                  (SDL_Color){(Uint8)(COL_RUST.r * reveal),
                              (Uint8)(COL_RUST.g * reveal),
                              (Uint8)(COL_RUST.b * reveal), 255},
                  "SHE'S TOO CLOSE // KEEP MOVING");
        color_rect(r, COL_RUST, 35.0f, 207.0f, 142.0f * reveal, 2.0f);
    }

    if (time > 0.75f && time < 8.55f)
    {
        float pulse = 0.45f + 0.55f * sinf(time * 2.0f);
        draw_text(r, (float)win_w - 180.0f, (float)win_h - 31.0f,
                  0.75f,
                  (SDL_Color){(Uint8)(100.0f + pulse * 42.0f),
                              (Uint8)(108.0f + pulse * 42.0f),
                              (Uint8)(106.0f + pulse * 38.0f), 255},
                  "SPACE / ENTER: SKIP");
    }
}

void level_transition_render(SDL_Renderer *r,
                             const LevelTransition *transition,
                             int win_w, int win_h)
{
    float time = transition->time;
    float ground_y = 482.0f;
    float door_x = (float)win_w - 126.0f;
    float group_progress = clamp01((time - 0.35f) / 4.60f);
    float group_x = lerpf(-80.0f, door_x + 112.0f, group_progress);
    float hostage_x = group_x + 35.0f;

    render_transition_report(r, transition, win_w);
    render_transition_corridor(r, time, win_w, win_h, door_x, ground_y);

    if (time >= 0.35f && group_x < (float)win_w + 40.0f)
    {
        /* The captors keep physical control of her as the group moves. */
        set_color(r, (SDL_Color){128, 91, 67, 255});
        SDL_RenderLine(r, group_x + 31.0f, ground_y - 29.0f,
                       hostage_x + 9.0f, ground_y - 25.0f);
        SDL_RenderLine(r, hostage_x + 25.0f, ground_y - 25.0f,
                       group_x + 73.0f, ground_y - 29.0f);
        draw_terrorist(r, group_x, ground_y, 1.32f, time, 0.0f, 1);
        draw_hostage(r, hostage_x, ground_y, 1.18f, time, 1, true);
        draw_terrorist(r, group_x + 68.0f, ground_y,
                       1.32f, time, 2.2f, 1);
    }

    if (time >= 0.95f && time < 2.20f)
    {
        float entry = smoothstep01((time - 0.95f) / 1.25f);
        draw_agent(r, lerpf(-48.0f, 170.0f, entry),
                   ground_y, 1.48f, time * 1.2f, 1);
    }
    else if (time >= 2.20f && time < 3.48f)
    {
        draw_agent_held_fire(r, 170.0f, ground_y, 1.48f, time, true, 1);
    }
    else if (time >= 3.48f && time < 3.88f)
    {
        draw_agent_held_fire(r, 170.0f, ground_y, 1.48f, time, false, 1);
    }
    else if (time >= 3.88f && time < 8.10f)
    {
        float pursuit = smoothstep01((time - 3.88f) / 4.10f);
        draw_agent(r, lerpf(170.0f, door_x + 112.0f, pursuit),
                   ground_y, 1.48f, time * 1.42f, 1);
    }

    draw_transition_door_foreground(r, door_x, ground_y);
    render_transition_action_ui(r, time, hostage_x, ground_y,
                                win_w, win_h);

    fx_vignette(r, win_w, win_h, 70);
    fx_grain(r, win_w, win_h, time, 24);

    color_rect(r, (SDL_Color){3, 5, 8, 255},
               0.0f, 0.0f, (float)win_w, 18.0f);
    color_rect(r, (SDL_Color){3, 5, 8, 255},
               0.0f, (float)win_h - 18.0f, (float)win_w, 18.0f);

    float fade_in = 1.0f - smoothstep01(time / 0.48f);
    float fade_out = smoothstep01((time - 8.45f) / 0.85f);
    float fade = fmaxf(fade_in, fade_out);
    if (fade > 0.0f)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, 2, 4, 7, (Uint8)(fade * 255.0f));
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

static void render_outro_sky(SDL_Renderer *r, float time,
                             int win_w, int win_h)
{
    color_rect(r, COL_NIGHT, 0.0f, 0.0f, (float)win_w, (float)win_h);
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
        float x = (float)(h % (unsigned)win_w);
        float y = 24.0f + (float)((h >> 9) % 245u);
        float glow = 0.45f + 0.55f *
                                 sinf(time * (0.42f + (float)(i % 5u) * 0.12f) +
                                      (float)(h & 63u));
        SDL_Color star = {(Uint8)(116.0f + glow * 67.0f),
                          (Uint8)(132.0f + glow * 61.0f),
                          (Uint8)(146.0f + glow * 62.0f), 255};
        color_rect(r, star, x, y, i % 13u == 0u ? 2.0f : 1.0f, 1.0f);
    }

    /* Layered high-rises keep the roof visually tied to Kessler Tower. */
    for (int i = 0; i < 11; ++i)
    {
        float x = (float)(i * 83 - 27);
        float height = 70.0f + (float)((i * 47) % 112);
        float top = 407.0f - height;
        SDL_Color wall = i & 1 ? (SDL_Color){14, 23, 33, 255}
                               : (SDL_Color){11, 19, 29, 255};
        color_rect(r, wall, x, top, 67.0f, height);
        color_rect(r, (SDL_Color){28, 39, 48, 255},
                   x + 4.0f, top + 4.0f, 3.0f, height - 4.0f);
        for (int row = 0; row < (int)height - 18; row += 19)
        {
            for (int col = 0; col < 3; ++col)
            {
                unsigned wh = scene_hash((unsigned)(i * 113 + row * 7 + col));
                if ((wh & 7u) == 0u)
                {
                    SDL_Color light = wh & 8u
                                          ? (SDL_Color){104, 92, 57, 255}
                                          : (SDL_Color){42, 79, 88, 255};
                    color_rect(r, light, x + 13.0f + col * 17.0f,
                               top + 12.0f + row, 6.0f, 3.0f);
                }
            }
        }
    }

    /* City haze between the skyline and the rooftop parapet. */
    fx_vgrad(r, 0.0f, 336.0f, (float)win_w, 72.0f,
             (SDL_Color){28, 44, 58, 255}, 0,
             (SDL_Color){28, 44, 58, 255}, 52);
}

static void render_rooftop(SDL_Renderer *r, float time, int win_w, int win_h)
{
    const float ground = 438.0f;

    color_rect(r, (SDL_Color){74, 80, 78, 255},
               0.0f, ground - 8.0f, (float)win_w, 8.0f);
    color_rect(r, (SDL_Color){30, 37, 40, 255},
               0.0f, ground, (float)win_w, (float)win_h - ground);
    color_rect(r, (SDL_Color){45, 52, 52, 255},
               0.0f, ground + 7.0f, (float)win_w, 3.0f);
    color_rect(r, (SDL_Color){18, 24, 27, 255},
               0.0f, ground + 50.0f, (float)win_w, 4.0f);

    /* Broken helipad ring and H marking. */
    for (int row = -54; row <= 54; row += 4)
    {
        float half = sqrtf(fmaxf(0.0f, 54.0f * 54.0f -
                                          (float)(row * row)));
        if (row < -46 || row > 46)
            color_rect(r, (SDL_Color){105, 104, 81, 255},
                       165.0f - half, ground + 35.0f + (float)row,
                       half * 2.0f, 2.0f);
    }
    color_rect(r, (SDL_Color){91, 91, 73, 255},
               137.0f, ground + 16.0f, 8.0f, 55.0f);
    color_rect(r, (SDL_Color){91, 91, 73, 255},
               185.0f, ground + 16.0f, 8.0f, 55.0f);
    color_rect(r, (SDL_Color){91, 91, 73, 255},
               137.0f, ground + 39.0f, 56.0f, 8.0f);

    /* Pipes, vents, and warning lights sell the rooftop scale. */
    color_rect(r, (SDL_Color){23, 29, 31, 255},
               28.0f, ground - 30.0f, 84.0f, 30.0f);
    color_rect(r, (SDL_Color){69, 76, 73, 255},
               24.0f, ground - 35.0f, 92.0f, 7.0f);
    for (int x = 35; x < 105; x += 13)
        color_rect(r, (SDL_Color){42, 49, 49, 255},
                   (float)x, ground - 27.0f, 4.0f, 22.0f);

    color_rect(r, (SDL_Color){62, 67, 63, 255},
               616.0f, ground - 91.0f, 171.0f, 91.0f);
    color_rect(r, (SDL_Color){91, 94, 84, 255},
               608.0f, ground - 98.0f, 187.0f, 8.0f);
    color_rect(r, (SDL_Color){31, 38, 39, 255},
               649.0f, ground - 76.0f, 86.0f, 76.0f);
    color_rect(r, (SDL_Color){7, 12, 16, 255},
               656.0f, ground - 69.0f, 72.0f, 69.0f);
    color_rect(r, (SDL_Color){100, 103, 93, 255},
               649.0f, ground - 76.0f, 86.0f, 5.0f);
    draw_text(r, 669.0f, ground - 91.0f, 0.75f,
              (SDL_Color){169, 168, 145, 255}, "ROOF");

    float beacon = 0.38f + 0.62f * (sinf(time * 5.5f) > 0.25f);
    color_rect(r, (SDL_Color){(Uint8)(COL_RUST.r * beacon),
                              (Uint8)(COL_RUST.g * beacon),
                              (Uint8)(COL_RUST.b * beacon), 255},
               625.0f, ground - 105.0f, 10.0f, 5.0f);
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

static void draw_helicopter(SDL_Renderer *r, float x, float y,
                            float angle, float rotor_angle, float damage)
{
    SDL_Color body_dark = {10, 17, 20, 255};
    SDL_Color body = damage > 0.25f
                         ? (SDL_Color){50, 52, 47, 255}
                         : (SDL_Color){38, 69, 72, 255};
    SDL_Color body_light = damage > 0.25f
                               ? (SDL_Color){83, 74, 60, 255}
                               : (SDL_Color){70, 111, 111, 255};

    /* Shadow makes the craft readable against both sky and buildings. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    draw_rotated_box(r, x + 4.0f, y + 5.0f, -48.0f, -17.0f,
                     82.0f, 37.0f, angle, (SDL_Color){2, 4, 7, 120});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_rotated_box(r, x, y, -48.0f, -16.0f, 82.0f, 34.0f,
                     angle, body_dark);
    draw_rotated_box(r, x, y, -43.0f, -13.0f, 72.0f, 27.0f,
                     angle, body);
    draw_rotated_box(r, x, y, -38.0f, -10.0f, 25.0f, 19.0f,
                     angle, (SDL_Color){15, 35, 43, 255});
    draw_rotated_box(r, x, y, -35.0f, -8.0f, 18.0f, 14.0f,
                     angle, (SDL_Color){50, 108, 119, 255});
    draw_rotated_box(r, x, y, -9.0f, -10.0f, 32.0f, 4.0f,
                     angle, body_light);

    /* Tail boom, stabilizer, and rear rotor. */
    draw_rotated_box(r, x, y, 28.0f, -6.0f, 77.0f, 12.0f,
                     angle, body_dark);
    draw_rotated_box(r, x, y, 31.0f, -3.0f, 67.0f, 7.0f,
                     angle, body);
    draw_rotated_box(r, x, y, 91.0f, -24.0f, 12.0f, 28.0f,
                     angle, body_dark);
    draw_rotated_box(r, x, y, 94.0f, -21.0f, 7.0f, 22.0f,
                     angle, body_light);

    float tail_x = 0.0f, tail_y = 0.0f;
    rotate_local(x, y, 100.0f, -8.0f, angle, &tail_x, &tail_y);
    draw_rotated_box(r, tail_x, tail_y, -18.0f, -1.5f, 36.0f, 3.0f,
                     angle + rotor_angle * 1.7f, COL_INK);
    draw_rotated_box(r, tail_x, tail_y, -14.0f, -1.5f, 28.0f, 3.0f,
                     angle + rotor_angle * 1.7f + 1.5708f, COL_INK);

    /* Landing skids and main rotor. */
    draw_rotated_box(r, x, y, -31.0f, 24.0f, 62.0f, 3.0f,
                     angle, body_dark);
    draw_rotated_box(r, x, y, -24.0f, 15.0f, 3.0f, 11.0f,
                     angle, body_dark);
    draw_rotated_box(r, x, y, 19.0f, 14.0f, 3.0f, 12.0f,
                     angle, body_dark);
    draw_rotated_box(r, x, y, -2.0f, -31.0f, 4.0f, 17.0f,
                     angle, body_dark);

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

static void draw_outro_agent_sky_aim(SDL_Renderer *r, float x,
                                     float ground_y, float scale,
                                     float time)
{
    draw_agent_held_fire(r, x, ground_y, scale, time, false, 1);

    float shoulder_x = x + 20.0f * scale;
    float shoulder_y = ground_y - 20.0f * scale;
    SDL_Color skin = {209, 154, 105, 255};
    set_color(r, COL_INK);
    for (int i = -2; i <= 2; ++i)
        SDL_RenderLine(r, shoulder_x, shoulder_y + (float)i,
                       shoulder_x + 20.0f * scale,
                       shoulder_y - 24.0f * scale + (float)i);
    set_color(r, skin);
    SDL_RenderLine(r, shoulder_x + 2.0f, shoulder_y,
                   shoulder_x + 17.0f * scale,
                   shoulder_y - 20.0f * scale);
    set_color(r, (SDL_Color){72, 80, 78, 255});
    for (int i = -1; i <= 1; ++i)
        SDL_RenderLine(r, shoulder_x + 17.0f * scale,
                       shoulder_y - 22.0f * scale + (float)i,
                       shoulder_x + 28.0f * scale,
                       shoulder_y - 37.0f * scale + (float)i);
}

static void draw_muzzle_flash(SDL_Renderer *r, float x, float y,
                              float angle, float strength)
{
    float dx = cosf(angle);
    float dy = sinf(angle);
    set_color(r, (SDL_Color){255, 236, 153, 255});
    SDL_RenderLine(r, x - dy * 5.0f, y + dx * 5.0f,
                   x + dy * 5.0f, y - dx * 5.0f);
    set_color(r, (SDL_Color){255, 151, 37, 255});
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
    set_rgba(r, 255, 135, 28, 105);
    SDL_RenderLine(r, tx, ty + 2.0f, hx, hy + 2.0f);
    set_rgba(r, 255, 239, 160, 255);
    SDL_RenderLine(r, tx, ty, hx, hy);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    if (age < 0.07f)
        draw_muzzle_flash(r, from_x, from_y,
                          atan2f(to_y - from_y, to_x - from_x), 1.0f);
}

static void draw_terrorist_down(SDL_Renderer *r, float x, float ground_y,
                                bool faces_right)
{
    float dir = faces_right ? 1.0f : -1.0f;
    color_rect(r, (SDL_Color){2, 4, 7, 155},
               x - 5.0f, ground_y - 3.0f, 57.0f, 5.0f);
    color_rect(r, COL_INK, x, ground_y - 13.0f, 43.0f, 13.0f);
    color_rect(r, (SDL_Color){42, 47, 43, 255},
               x + 5.0f, ground_y - 11.0f, 27.0f, 8.0f);
    color_rect(r, COL_RUST, x + 13.0f, ground_y - 9.0f, 12.0f, 2.0f);
    color_rect(r, (SDL_Color){143, 101, 73, 255},
               x + (faces_right ? 34.0f : -2.0f),
               ground_y - 12.0f, 10.0f, 9.0f);
    color_rect(r, (SDL_Color){20, 24, 24, 255},
               x + 8.0f + dir * 3.0f, ground_y - 20.0f, 9.0f, 10.0f);
    color_rect(r, COL_INK, x + 30.0f, ground_y - 5.0f, 26.0f, 4.0f);
}

static void draw_shock_mark(SDL_Renderer *r, float x, float y, float time)
{
    float pulse = 0.65f + 0.35f * sinf(time * 13.0f);
    SDL_Color color = {(Uint8)(COL_AMBER.r * pulse),
                       (Uint8)(COL_AMBER.g * pulse),
                       (Uint8)(COL_AMBER.b * pulse), 255};
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
        fx_glow(r, x, y, 150.0f, (SDL_Color){255, 150, 60, 255},
                (Uint8)(120.0f * (1.0f - age / 1.4f)));

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    if (age < 0.85f)
    {
        float radius = 18.0f + expand * 78.0f;
        set_rgba(r, 255, 99, 28, (Uint8)(205.0f * (1.0f - age / 0.85f)));
        fill_rect(r, x - radius, y - radius * 0.55f,
                  radius * 2.0f, radius * 1.10f);
        set_rgba(r, 255, 229, 126,
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
    color_rect(r, (SDL_Color){4, 7, 9, 160},
               x - 72.0f, ground_y - 5.0f, 142.0f, 7.0f);
    color_rect(r, COL_INK, x - 48.0f, ground_y - 23.0f, 82.0f, 20.0f);
    color_rect(r, (SDL_Color){48, 50, 45, 255},
               x - 41.0f, ground_y - 19.0f, 67.0f, 12.0f);
    draw_rotated_box(r, x, ground_y, -91.0f, -5.0f, 150.0f, 4.0f,
                     -0.15f, (SDL_Color){20, 25, 26, 255});
    float fire = 0.5f + 0.5f * sinf(time * 17.0f);
    color_rect(r, (SDL_Color){238, 83, 24, 255},
               x - 12.0f, ground_y - 28.0f - fire * 8.0f,
               18.0f, 16.0f + fire * 8.0f);
    color_rect(r, (SDL_Color){255, 198, 72, 255},
               x - 7.0f, ground_y - 23.0f - fire * 5.0f,
               9.0f, 12.0f + fire * 5.0f);
}

static void draw_pixel_heart(SDL_Renderer *r, float center_x,
                             float y, float time)
{
    static const char *rows[] = {
        "0110110",
        "1111111",
        "1111111",
        "0111110",
        "0011100",
        "0001000"};
    float pulse = 1.0f + (0.5f + 0.5f * sinf(time * 4.0f)) * 0.10f;
    float pixel = 3.0f * pulse;
    float width = 7.0f * pixel;
    float bob = sinf(time * 2.2f) * 2.0f;

    fx_glow(r, center_x, y + bob + 3.0f * pixel, width + 14.0f,
            (SDL_Color){236, 70, 84, 255}, 66);

    for (int row = 0; row < 6; ++row)
    {
        for (int col = 0; col < 7; ++col)
        {
            if (rows[row][col] != '1')
                continue;
            SDL_Color color = row < 2
                                  ? (SDL_Color){247, 83, 91, 255}
                                  : (SDL_Color){207, 43, 55, 255};
            color_rect(r, color,
                       center_x - width * 0.5f + (float)col * pixel,
                       y + bob + (float)row * pixel,
                       ceilf(pixel), ceilf(pixel));
        }
    }
}

static void draw_reunion_pair(SDL_Renderer *r, float center_x,
                              float ground_y, float time)
{
    /*
     * Keep the ending deliberately simple: the two original sprites stand
     * close and face one another, with no overlapping limbs to muddy them.
     */
    draw_agent(r, center_x - 38.0f, ground_y, 1.35f, 0.0f, 1);
    draw_hostage(r, center_x + 1.0f, ground_y,
                 1.18f, 0.0f, -1, false);
    draw_pixel_heart(r, center_x, ground_y - 94.0f, time);
}

static void render_outro_ui(SDL_Renderer *r, float time,
                            float hostage_x, int win_w, int win_h)
{
    if (time >= 4.35f && time < 7.45f)
    {
        float reveal = smoothstep01((time - 4.35f) / 0.35f);
        draw_text(r, 34.0f, 39.0f, 0.9f,
                  (SDL_Color){(Uint8)(COL_AMBER.r * reveal),
                              (Uint8)(COL_AMBER.g * reveal),
                              (Uint8)(COL_AMBER.b * reveal), 255},
                  "ROOFTOP // EXTRACTION INBOUND");
        color_rect(r, COL_AMBER, 34.0f, 54.0f, 105.0f * reveal, 2.0f);
    }

    if (time >= 6.00f && time < 9.30f)
    {
        float pulse = 0.5f + 0.5f * sinf(time * 6.0f);
        draw_target_brackets(r, hostage_x - 10.0f, 352.0f,
                             94.0f, 73.0f, pulse);
        draw_text(r, 34.0f, 78.0f, 0.8f, COL_RUST,
                  "NO CLEAR SHOT // HOSTAGE IN LINE OF FIRE");
    }

    if (time >= 9.25f && time < 10.25f)
    {
        float reveal = smoothstep01((time - 9.25f) / 0.18f);
        draw_text(r, 34.0f, 78.0f, 0.9f,
                  (SDL_Color){(Uint8)(COL_CYAN.r * reveal),
                              (Uint8)(COL_CYAN.g * reveal),
                              (Uint8)(COL_CYAN.b * reveal), 255},
                  "NEW TARGET // ROTORCRAFT");
    }

    if (time >= 13.35f && time < 16.50f)
    {
        float reveal = smoothstep01((time - 13.35f) / 0.22f);
        draw_text(r, 34.0f, 39.0f, 0.9f,
                  (SDL_Color){(Uint8)(COL_RUST.r * reveal),
                              (Uint8)(COL_RUST.g * reveal),
                              (Uint8)(COL_RUST.b * reveal), 255},
                  "EXTRACTION DENIED // RESCUE WINDOW OPEN");
    }

    if (time > 0.85f && time < OUTRO_FINAL_REVEAL_TIME)
    {
        float pulse = 0.45f + 0.55f * sinf(time * 2.0f);
        draw_text(r, (float)win_w - 181.0f, (float)win_h - 31.0f,
                  0.75f,
                  (SDL_Color){(Uint8)(101.0f + pulse * 40.0f),
                              (Uint8)(109.0f + pulse * 40.0f),
                              (Uint8)(108.0f + pulse * 38.0f), 255},
                  "SPACE / ENTER: SKIP");
    }
}

void outro_cutscene_render(SDL_Renderer *r,
                           const OutroCutscene *cutscene,
                           int win_w, int win_h)
{
    const float time = cutscene->time;
    const float ground = 438.0f;
    const float captor_one_x = 298.0f;
    const float hostage_x = 344.0f;
    const float captor_two_x = 390.0f;
    const float agent_stop_x = 538.0f;

    render_outro_sky(r, time, win_w, win_h);
    render_rooftop(r, time, win_w, win_h);

    float heli_x = (float)win_w + 155.0f;
    float heli_y = 126.0f;
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
                           0.0f, group_dir);
            if (time >= 13.35f)
                draw_shock_mark(r, first_x + 17.0f, ground - 78.0f, time);
        }
        else if (time < 15.60f)
        {
            draw_terrorist(r, first_x, ground,
                           1.34f, 0.0f, 0.0f, 1);
        }
        else
        {
            draw_terrorist_down(r, first_x, ground, false);
        }

        if (time < 14.70f)
        {
            float shake = time >= 13.35f
                              ? sinf(time * 33.0f + 1.2f) * 2.5f
                              : 0.0f;
            draw_terrorist(r, second_x + shake, ground,
                           1.34f, time < 3.75f ? time : 0.0f,
                           2.2f, group_dir);
            if (time >= 13.35f)
                draw_shock_mark(r, second_x + 17.0f,
                                ground - 78.0f, time + 0.3f);
        }
        else
        {
            draw_terrorist_down(r, second_x, ground, true);
        }

        if (time < 16.45f)
        {
            draw_hostage(r, woman_x, ground, 1.18f,
                         time < 3.75f ? time : 0.0f, group_dir, true);
        }
        else if (time < 18.35f)
        {
            float reunion = smoothstep01((time - 16.45f) / 1.75f);
            draw_hostage(r, lerpf(woman_x, 452.0f, reunion),
                         ground, 1.18f, time * 1.4f, 1, false);
        }
    }

    /* Chuck follows, holds his fire, then pivots toward the helicopter. */
    if (time >= 2.35f && time < 5.05f)
    {
        float entry = smoothstep01((time - 2.35f) / 2.55f);
        draw_agent(r, lerpf(704.0f, agent_stop_x, entry),
                   ground, 1.48f, time * 1.25f, -1);
    }
    else if (time >= 5.05f && time < 9.25f)
    {
        bool aiming = time < 8.55f;
        draw_agent_held_fire(r, agent_stop_x, ground,
                             1.48f, time, aiming, -1);
    }
    else if (time >= 9.25f && time < 10.45f)
    {
        draw_outro_agent_sky_aim(r, agent_stop_x, ground, 1.48f, time);
    }
    else if (time >= 10.45f && time < 14.20f)
    {
        draw_agent_held_fire(r, agent_stop_x, ground,
                             1.48f, time, false, 1);
    }
    else if (time >= 14.20f && time < 16.40f)
    {
        draw_agent_held_fire(r, agent_stop_x, ground,
                             1.48f, time, true, -1);
    }
    else if (time >= 16.40f && time < 18.35f)
    {
        float reunion = smoothstep01((time - 16.40f) / 1.75f);
        draw_agent(r, lerpf(agent_stop_x, 430.0f, reunion),
                   ground, 1.48f, time * 1.35f, -1);
    }
    else if (time >= 18.35f && time < OUTRO_FINAL_REVEAL_TIME)
    {
        draw_reunion_pair(r, 451.0f, ground, time);
    }

    /* Three deliberate shots tell the turn-and-rescue beat without gameplay. */
    draw_shot_tracer(r, time, 9.95f,
                     agent_stop_x + 58.0f, ground - 91.0f,
                     heli_x - 8.0f, heli_y + 2.0f);
    draw_shot_tracer(r, time, 14.55f,
                     agent_stop_x - 16.0f, ground - 30.0f,
                     captor_two_x + 17.0f, ground - 28.0f);
    draw_shot_tracer(r, time, 15.45f,
                     agent_stop_x - 16.0f, ground - 30.0f,
                     captor_one_x + 17.0f, ground - 28.0f);

    render_outro_ui(r, time, hostage_x, win_w, win_h);

    fx_vignette(r, win_w, win_h, 74);
    fx_grain(r, win_w, win_h, time, 24);

    color_rect(r, (SDL_Color){3, 5, 8, 255},
               0.0f, 0.0f, (float)win_w, 19.0f);
    color_rect(r, (SDL_Color){3, 5, 8, 255},
               0.0f, (float)win_h - 19.0f, (float)win_w, 19.0f);

    if (time >= OUTRO_FINAL_REVEAL_TIME)
    {
        float reveal = smoothstep01(
            (time - OUTRO_FINAL_REVEAL_TIME) / 1.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, 3, 7, 11, (Uint8)(190.0f * reveal));
        fill_rect(r, 0.0f, 19.0f, (float)win_w,
                  (float)win_h - 38.0f);
        set_rgba(r, COL_CYAN.r, COL_CYAN.g, COL_CYAN.b,
                 (Uint8)(70.0f * reveal));
        fill_rect(r, 0.0f, 99.0f, (float)win_w, 2.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

        draw_reunion_pair(r, 400.0f, 438.0f, time);
        draw_cutscene_text_centered(
            r, (float)win_w * 0.5f, 69.0f, 2.2f,
            (SDL_Color){(Uint8)(91.0f * reveal),
                        (Uint8)(237.0f * reveal),
                        (Uint8)(169.0f * reveal), 255},
            "SHE'S SAFE");
        draw_cutscene_text_centered(
            r, (float)win_w * 0.5f, 122.0f, 1.55f,
            (SDL_Color){(Uint8)(226.0f * reveal),
                        (Uint8)(222.0f * reveal),
                        (Uint8)(199.0f * reveal), 255},
            "THANK YOU FOR PLAYING");
        draw_cutscene_text_centered(
            r, (float)win_w * 0.5f, 151.0f, 0.85f,
            (SDL_Color){(Uint8)(128.0f * reveal),
                        (Uint8)(145.0f * reveal),
                        (Uint8)(143.0f * reveal), 255},
            "CHUCK BROUGHT HER HOME");

        if (time >= 21.0f)
        {
            float pulse = 0.55f + 0.45f * sinf(time * 2.4f);
            draw_cutscene_text_centered(
                r, (float)win_w * 0.5f, 505.0f, 0.85f,
                (SDL_Color){(Uint8)(130.0f + pulse * 48.0f),
                            (Uint8)(139.0f + pulse * 48.0f),
                            (Uint8)(136.0f + pulse * 45.0f), 255},
                "PRESS R TO REPLAY THE RESCUE");
        }
    }

    float fade_in = 1.0f - smoothstep01(time / 0.62f);
    if (fade_in > 0.0f)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_rgba(r, 2, 4, 7, (Uint8)(fade_in * 255.0f));
        fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}
