/*
 * Chuck's opening is rendered entirely from the same hard-edged primitives
 * as the rest of the game. It establishes the hostage situation before the
 * title screen without adding an external asset pipeline.
 */
#include "cutscene.h"

#include <math.h>

static const SDL_Color COL_INK = {6, 9, 14, 255};
static const SDL_Color COL_NIGHT = {9, 15, 25, 255};
static const SDL_Color COL_CREAM = {226, 222, 199, 255};
static const SDL_Color COL_RUST = {183, 49, 43, 255};
static const SDL_Color COL_AMBER = {229, 158, 50, 255};
static const SDL_Color COL_CYAN = {55, 188, 189, 255};
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
    for (int y = 0; y < win_h; y += 16)
    {
        float p = (float)y / (float)win_h;
        set_rgba(r, (Uint8)(COL_NIGHT.r + p * 5.0f),
                 (Uint8)(COL_NIGHT.g + p * 7.0f),
                 (Uint8)(COL_NIGHT.b + p * 9.0f), 255);
        fill_rect(r, 0.0f, (float)y, (float)win_w, 16.0f);
    }

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
                       float scale, float time)
{
    float stride = sinf(time * 12.0f);
    float y = ground_y - 32.0f * scale;
    float bob = fabsf(stride) * scale;

    color_rect(r, (SDL_Color){2, 4, 7, 150},
               x + 2.0f * scale, ground_y - 2.0f,
               24.0f * scale, 4.0f);

    sprite_rect(r, x, y + fabsf(stride) * scale, 28.0f, 1, scale,
                5 + stride * 1.6f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * scale, 28.0f, 1, scale,
                15 - stride * 1.6f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                5, 10, 17, 14, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                7, 11, 13, 12, (SDL_Color){35, 102, 142, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                8, 12, 4, 9, (SDL_Color){60, 148, 171, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                8, 20, 11, 2, COL_AMBER);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                9, 1, 11, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                10, 3, 9, 8, (SDL_Color){210, 154, 105, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                9, 1, 11, 4, (SDL_Color){70, 38, 28, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                18, 13, 11, 5, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                19, 14, 10, 3, (SDL_Color){209, 154, 105, 255});
}

static void draw_terrorist(SDL_Renderer *r, float x, float ground_y,
                           float scale, float time, float phase)
{
    float stride = sinf(time * 9.0f + phase);
    float y = ground_y - 32.0f * scale;
    float bob = fabsf(stride) * 0.7f * scale;

    color_rect(r, (SDL_Color){2, 4, 7, 155},
               x + 2.0f * scale, ground_y - 2.0f,
               24.0f * scale, 4.0f);
    sprite_rect(r, x, y + fabsf(stride) * scale, 28.0f, 1, scale,
                4 + stride, 21, 8, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * scale, 28.0f, 1, scale,
                15 - stride, 21, 8, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                4, 10, 19, 14, (SDL_Color){17, 21, 22, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                7, 12, 13, 9, (SDL_Color){49, 54, 49, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                7, 16, 13, 3, COL_RUST);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                8, 1, 12, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                10, 4, 9, 7, (SDL_Color){145, 103, 75, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                8, 1, 13, 5, (SDL_Color){24, 28, 27, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                17, 6, 2, 2, COL_RUST);

    /* Low-ready rifle makes the captors unmistakable at pixel scale. */
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                17, 13, 13, 4, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                21, 14, 13, 2, (SDL_Color){67, 73, 69, 255});
    sprite_rect(r, x, y + bob, 28.0f, 1, scale,
                18, 14, 6, 3, (SDL_Color){145, 103, 75, 255});
}

static void draw_hostage(SDL_Renderer *r, float x, float ground_y,
                         float scale, float time)
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
    sprite_rect(r, x, y + fabsf(step) * scale, 26.0f, 1, scale,
                7 + step * 0.7f, 23, 5, 11, COL_INK);
    sprite_rect(r, x, y + fabsf(step) * scale, 26.0f, 1, scale,
                8 + step * 0.7f, 24, 3, 8, trousers);
    sprite_rect(r, x, y + fabsf(step) * scale, 26.0f, 1, scale,
                6 + step * 0.7f, 31, 7, 3, boots);
    sprite_rect(r, x, y + (1.0f - fabsf(step)) * scale, 26.0f, 1, scale,
                15 - step * 0.7f, 23, 5, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(step)) * scale, 26.0f, 1, scale,
                16 - step * 0.7f, 24, 3, 8, trousers);
    sprite_rect(r, x, y + (1.0f - fabsf(step)) * scale, 26.0f, 1, scale,
                14 - step * 0.7f, 31, 7, 3, boots);

    /* Shoulder-length hair provides the main readable cue at pixel scale. */
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                7 + hair_sway, 1, 13, 15, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                8 + hair_sway, 2, 11, 13, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                7 - hair_sway, 7, 4, 9, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                8 - hair_sway, 8, 2, 7, hair);

    /* Simple profile without makeup accents. */
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                10, 2, 10, 11, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                11, 3, 8, 8, skin);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                12, 10, 7, 2, skin);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                8, 0, 12, 5, hair_dark);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                9, 1, 9, 2, hair);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                8, 3, 4, 8, hair);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                17, 5, 2, 1, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                18, 9, 1, 1, hair_dark);

    /* Straight-cut red coat keeps her silhouette distinct but grounded. */
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                12, 11, 5, 4, skin);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                7, 12, 14, 15, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                8, 13, 12, 13, coat);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                9, 14, 4, 10, coat_light);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                13, 14, 2, 12, coat_dark);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                8, 24, 12, 3, coat_dark);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                11, 13, 6, 2, (SDL_Color){226, 222, 199, 255});

    /* Bent sleeves lead into small hands tied together in front of her. */
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                18, 14, 6, 5, COL_INK);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                19, 15, 5, 3, coat);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                22, 17, 5, 3, skin);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                24, 16, 3, 4, skin);
    sprite_rect(r, x, y + bob, 26.0f, 1, scale,
                24, 16, 2, 5, (SDL_Color){68, 75, 72, 255});
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
        draw_terrorist(r, escort_one_x, ground, 1.35f, time, 0.0f);
        draw_hostage(r, hostage_x, ground, 1.18f, time);
        draw_terrorist(r, escort_two_x, ground, 1.35f, time, 2.2f);
    }

    if (time >= 7.0f && time < 11.05f)
    {
        float run = smoothstep01((time - 7.0f) / 3.75f);
        float agent_x = lerpf(199.0f, 658.0f, run);
        draw_agent(r, agent_x, ground, 1.48f, time);
    }

    render_rain(r, time, win_w, win_h);
    render_cinematic_ui(r, time, win_w, win_h, suv_x);

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
    for (int y = 151; y < win_h; y += 14)
    {
        float p = (float)(y - 151) / (float)(win_h - 151);
        set_rgba(r, (Uint8)(14.0f + p * 9.0f),
                 (Uint8)(22.0f + p * 11.0f),
                 (Uint8)(28.0f + p * 10.0f), 255);
        fill_rect(r, 0.0f, (float)y, (float)win_w, 14.0f);
    }

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
        color_rect(r, (SDL_Color){(Uint8)(83.0f * pulse),
                                  (Uint8)(79.0f * pulse),
                                  (Uint8)(50.0f * pulse), 255},
                   32.0f + i * 118.0f, ground_y + 21.0f, 57.0f, 2.0f);
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
                                 float scale, float time, bool aiming)
{
    float y = ground_y - 32.0f * scale;
    float breath = sinf(time * 4.5f) * 0.35f * scale;

    color_rect(r, (SDL_Color){2, 4, 7, 150},
               x + 2.0f * scale, ground_y - 2.0f,
               28.0f * scale, 4.0f);
    sprite_rect(r, x, y, 30.0f, 1, scale,
                6, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y, 30.0f, 1, scale,
                16, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                5, 10, 18, 14, COL_INK);
    sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                7, 11, 14, 12, (SDL_Color){35, 102, 142, 255});
    sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                8, 12, 4, 9, (SDL_Color){60, 148, 171, 255});
    sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                8, 20, 12, 2, COL_AMBER);
    sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                9, 1, 11, 11, COL_INK);
    sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                10, 3, 9, 8, (SDL_Color){210, 154, 105, 255});
    sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                9, 1, 11, 4, (SDL_Color){70, 38, 28, 255});

    if (aiming)
    {
        sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                    18, 12, 13, 4, (SDL_Color){209, 154, 105, 255});
        sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                    27, 11, 10, 4, COL_INK);
        sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                    35, 12, 6, 2, (SDL_Color){77, 84, 81, 255});
    }
    else
    {
        /* He lowers the pistol rather than taking the obstructed shot. */
        sprite_rect(r, x, y + breath, 30.0f, 1, scale,
                    18, 13, 8, 8, (SDL_Color){209, 154, 105, 255});
        sprite_rect(r, x, y + breath, 30.0f, 1, scale,
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
        draw_terrorist(r, group_x, ground_y, 1.32f, time, 0.0f);
        draw_hostage(r, hostage_x, ground_y, 1.18f, time);
        draw_terrorist(r, group_x + 68.0f, ground_y,
                       1.32f, time, 2.2f);
    }

    if (time >= 0.95f && time < 2.20f)
    {
        float entry = smoothstep01((time - 0.95f) / 1.25f);
        draw_agent(r, lerpf(-48.0f, 170.0f, entry),
                   ground_y, 1.48f, time * 1.2f);
    }
    else if (time >= 2.20f && time < 3.48f)
    {
        draw_agent_held_fire(r, 170.0f, ground_y, 1.48f, time, true);
    }
    else if (time >= 3.48f && time < 3.88f)
    {
        draw_agent_held_fire(r, 170.0f, ground_y, 1.48f, time, false);
    }
    else if (time >= 3.88f && time < 8.10f)
    {
        float pursuit = smoothstep01((time - 3.88f) / 4.10f);
        draw_agent(r, lerpf(170.0f, door_x + 112.0f, pursuit),
                   ground_y, 1.48f, time * 1.42f);
    }

    draw_transition_door_foreground(r, door_x, ground_y);
    render_transition_action_ui(r, time, hostage_x, ground_y,
                                win_w, win_h);

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
