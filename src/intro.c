/*
 * Chuck's title screen is a small playable-world tableau rather than a
 * separate sci-fi interface.  It uses the same hard-edged shapes, materials
 * and character silhouettes as the game, with a simple arcade-style wordmark
 * and only the information needed to start.
 */
#include "intro.h"

#include <math.h>

#include "fx.h"

static const SDL_Color COL_INK = {5, 7, 12, 255};
static const SDL_Color COL_NIGHT = {10, 14, 23, 255};
static const SDL_Color COL_WALL = {27, 35, 49, 255};
static const SDL_Color COL_STEEL = {70, 84, 99, 255};
static const SDL_Color COL_CREAM = {236, 238, 224, 255};
static const SDL_Color COL_MUTED = {124, 140, 152, 255};
static const SDL_Color COL_RUST = {186, 54, 46, 255};
static const SDL_Color COL_AMBER = {248, 188, 74, 255};
static const SDL_Color COL_CYAN = {74, 222, 212, 255};

static void set_color(SDL_Renderer *r, SDL_Color color)
{
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
}

static void set_rgba(SDL_Renderer *r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha)
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

static SDL_Color dim_color(SDL_Color color, float amount)
{
    float factor = clamp01(amount);
    return (SDL_Color){
        (Uint8)((float)color.r * factor),
        (Uint8)((float)color.g * factor),
        (Uint8)((float)color.b * factor),
        color.a};
}

static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      SDL_Color color, const char *text)
{
    SDL_SetRenderScale(r, scale, scale);
    set_color(r, color);
    SDL_RenderDebugText(r, x / scale, y / scale, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static void draw_text_centered(SDL_Renderer *r, float cx, float y, float scale,
                               SDL_Color color, const char *text)
{
    float width = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE *
                  scale * (float)SDL_strlen(text);
    draw_text(r, cx - width * 0.5f, y, scale, color, text);
}

void intro_init(Intro *intro, int win_w, int win_h)
{
    SDL_zerop(intro);
    int w = win_w > 0 ? win_w : 800;
    int h = win_h > 0 ? win_h : 552;
    int skyline = h < 184 ? h : 184;

    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        IntroStar *star = &intro->stars[i];
        star->x = (float)SDL_rand(w);
        /*
         * Most stars live above the skyline, with a few continuing behind
         * the building so they remain visible in the side margins on wider
         * windows.
         */
        star->y = (float)SDL_rand(i < 110 ? skyline : h);
        star->speed = 3.0f + (float)(i % 5) * 2.2f +
                      (float)SDL_rand(4);
        star->phase = (float)SDL_rand(628) * 0.01f;
    }

    intro->start_button.w = 204.0f;
    intro->start_button.h = 34.0f;
    intro->start_button.x = ((float)w - intro->start_button.w) * 0.5f;
    intro->start_button.y = (float)h - 82.0f;
}

void intro_update(Intro *intro, float dt, int win_w, int win_h,
                  float mouse_x, float mouse_y)
{
    intro->time += dt;

    int w = win_w > 0 ? win_w : 800;
    int h = win_h > 0 ? win_h : 552;
    intro->start_button.x = ((float)w - intro->start_button.w) * 0.5f;
    intro->start_button.y = (float)h - 82.0f;

    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        IntroStar *star = &intro->stars[i];
        star->x += star->speed * dt;
        if (star->x > (float)w)
            star->x = 0.0f;
    }

    const SDL_FRect *button = &intro->start_button;
    intro->start_hovered =
        mouse_x >= button->x && mouse_x <= button->x + button->w &&
        mouse_y >= button->y && mouse_y <= button->y + button->h;
}

bool intro_hit_start_button(const Intro *intro, float x, float y)
{
    const SDL_FRect *button = &intro->start_button;
    return x >= button->x && x <= button->x + button->w &&
           y >= button->y && y <= button->y + button->h;
}

static void render_sky(SDL_Renderer *r, const Intro *intro, int win_w, int win_h)
{
    /* Deep night falling toward a faintly glowing teal horizon. */
    color_rect(r, COL_NIGHT, 0.0f, 0.0f, (float)win_w, (float)win_h);
    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, (float)win_h,
             (SDL_Color){6, 9, 17, 255}, 255,
             (SDL_Color){19, 30, 42, 255}, 255);

    /* A quiet moon anchors the composition's upper left. */
    float moon_x = (float)win_w * 0.155f;
    float moon_y = 74.0f;
    fx_glow(r, moon_x + 1.0f, moon_y + 1.0f, 58.0f,
            (SDL_Color){196, 214, 224, 255}, 34);
    for (int row = -12; row <= 12; ++row)
    {
        float half = sqrtf(fmaxf(0.0f, 144.0f - (float)(row * row)));
        color_rect(r, (SDL_Color){206, 219, 226, 255},
                   moon_x - half, moon_y + (float)row, half * 2.0f, 1.0f);
    }
    color_rect(r, (SDL_Color){176, 192, 204, 255},
               moon_x - 5.0f, moon_y - 4.0f, 5.0f, 4.0f);
    color_rect(r, (SDL_Color){176, 192, 204, 255},
               moon_x + 3.0f, moon_y + 5.0f, 4.0f, 3.0f);
    color_rect(r, (SDL_Color){186, 202, 212, 255},
               moon_x - 2.0f, moon_y + 2.0f, 3.0f, 2.0f);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        const IntroStar *star = &intro->stars[i];
        float depth = 0.35f + (float)(i % 5) * 0.15f;
        float twinkle_speed = 0.65f + (float)(i % 7) * 0.13f;
        float twinkle = 0.5f + 0.5f *
                                   sinf(intro->time * twinkle_speed +
                                        star->phase);
        Uint8 alpha = (Uint8)(30.0f + depth * 70.0f +
                              twinkle * (30.0f + depth * 45.0f));

        Uint8 red = i % 7 == 0 ? 209 : 180;
        Uint8 green = i % 7 == 0 ? 194 : 204;
        Uint8 blue = i % 7 == 0 ? 163 : 217;
        float sx = floorf(star->x);
        float sy = floorf(star->y);

        set_rgba(r, red, green, blue, alpha);
        if (depth > 0.85f && i % 3 == 0)
        {
            fill_rect(r, sx, sy, 2.0f, 2.0f);

            /* The brightest near stars briefly flare into a pixel cross. */
            if (twinkle > 0.88f)
            {
                set_rgba(r, red, green, blue, (Uint8)(alpha * 0.55f));
                fill_rect(r, sx - 1.0f, sy, 4.0f, 1.0f);
                fill_rect(r, sx, sy - 1.0f, 1.0f, 4.0f);
            }
        }
        else
        {
            fill_rect(r, sx, sy, 1.0f, 1.0f);
        }
    }

    /*
     * One restrained shooting star crosses the sky every few seconds.  Its
     * short stepped trail keeps it consistent with the pixel-art rendering.
     */
    float meteor_time = fmodf(intro->time, 9.5f);
    if (meteor_time >= 2.4f && meteor_time < 3.6f)
    {
        float p = (meteor_time - 2.4f) / 1.2f;
        float head_x = -12.0f + p * ((float)win_w + 36.0f);
        float head_y = 24.0f + p * 66.0f;

        for (int i = 7; i >= 0; --i)
        {
            Uint8 alpha = (Uint8)(32 + (7 - i) * 19);
            set_rgba(r, 198, 220, 225, alpha);
            fill_rect(r, floorf(head_x - (float)i * 7.0f),
                      floorf(head_y - (float)i * 3.0f),
                      i < 2 ? 3.0f : 2.0f, 1.0f);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* Two skyline depths keep the title grounded in a real city.  The far
     * row is hazy; the near row carries sparse lit windows and beacons. */
    int city_y = 184;
    fx_vgrad(r, 0.0f, (float)city_y - 66.0f, (float)win_w, 66.0f,
             (SDL_Color){36, 58, 70, 255}, 0,
             (SDL_Color){36, 58, 70, 255}, 56);

    static const int far_heights[] = {52, 78, 62, 90, 70, 84, 58};
    int fx_pos = -20;
    for (int i = 0; fx_pos < win_w + 30; ++i)
    {
        int width = 58 + (i * 17) % 30;
        int height = far_heights[i % (int)SDL_arraysize(far_heights)];
        color_rect(r, (SDL_Color){15, 22, 33, 255},
                   (float)fx_pos, (float)(city_y - height),
                   (float)width, (float)height);
        fx_pos += width + 4;
    }

    static const int heights[] = {36, 58, 44, 70, 49, 62, 40, 76, 51, 66, 45};
    int x = -8;
    for (int i = 0; x < win_w + 30; ++i)
    {
        int width = 46 + (i * 13) % 28;
        int height = heights[i % (int)SDL_arraysize(heights)];
        float top = (float)(city_y - height);
        color_rect(r, (SDL_Color){11, 16, 25, 255},
                   (float)x, top, (float)width, (float)height);
        color_rect(r, (SDL_Color){20, 28, 40, 255},
                   (float)x, top, (float)width, 1.0f);

        /* A few windows still lit this late; some rooftop hardware. */
        for (int wdw = 0; wdw < width / 12; ++wdw)
        {
            unsigned h = (unsigned)(i * 47 + wdw * 13);
            h ^= h << 7;
            if ((h % 5u) == 0u)
            {
                SDL_Color light = (h & 8u) ? (SDL_Color){118, 92, 48, 255}
                                           : (SDL_Color){44, 88, 96, 255};
                color_rect(r, light, (float)(x + 6 + wdw * 12),
                           top + 9.0f + (float)(h % 3u) * 11.0f, 5.0f, 3.0f);
            }
        }
        if (i % 4 == 1)
        {
            float ax = (float)x + (float)width * 0.5f;
            color_rect(r, (SDL_Color){16, 22, 33, 255},
                       ax, top - 12.0f, 2.0f, 12.0f);
            float blink = sinf(intro->time * 1.8f + (float)i) > 0.75f ? 1.0f : 0.2f;
            color_rect(r, fx_dim((SDL_Color){212, 74, 58, 255}, blink),
                       ax - 1.0f, top - 15.0f, 4.0f, 3.0f);
        }
        x += width + 7;
    }
}

static const char *logo_glyph(char letter, int row)
{
    static const char *c[7] = {
        "11111", "10000", "10000", "10000", "10000", "10000", "11111"};
    static const char *h[7] = {
        "10001", "10001", "10001", "11111", "10001", "10001", "10001"};
    static const char *u[7] = {
        "10001", "10001", "10001", "10001", "10001", "10001", "11111"};
    static const char *k[7] = {
        "10001", "10010", "10100", "11000", "10100", "10010", "10001"};

    switch (letter)
    {
    case 'C':
        return c[row];
    case 'H':
        return h[row];
    case 'U':
        return u[row];
    default:
        return k[row];
    }
}

static void draw_logo_letter(SDL_Renderer *r, float x, float y,
                             char letter, SDL_Color color,
                             bool bevel, float sweep_x)
{
    const float pitch = 11.0f;
    const float block = 10.0f;

    for (int row = 0; row < 7; ++row)
    {
        const char *bits = logo_glyph(letter, row);
        for (int col = 0; col < 5; ++col)
        {
            if (bits[col] != '1')
                continue;
            float bx = x + col * pitch;
            float by = y + row * pitch;
            SDL_Color c = color;
            if (bevel)
            {
                /* A specular band sweeps across the face now and then. */
                float boost = 1.0f - fabsf(bx - sweep_x) / 26.0f;
                if (boost > 0.0f)
                    c = fx_mix(c, (SDL_Color){255, 255, 248, 255},
                               boost * 0.75f);
            }
            color_rect(r, c, bx, by, block, block);
            if (bevel)
            {
                color_rect(r, fx_mix(c, (SDL_Color){255, 255, 250, 255}, 0.4f),
                           bx, by, block, 2.0f);
                color_rect(r, fx_dim(c, 0.68f),
                           bx, by + block - 2.0f, block, 2.0f);
                color_rect(r, fx_dim(c, 0.82f),
                           bx + block - 2.0f, by + 1.0f, 2.0f, block - 2.0f);
            }
        }
    }
}

static void render_logo(SDL_Renderer *r, const Intro *intro, int win_w)
{
    static const char *word = "CHUCK";
    const float logo_w = 304.0f;
    float x = ((float)win_w - logo_w) * 0.5f;
    const float y = 30.0f;
    const float letter_advance = 65.0f;

    /* A restrained warm under-glow lifts the wordmark off the night sky. */
    fx_glow(r, (float)win_w * 0.5f, y + 40.0f, 240.0f,
            (SDL_Color){222, 120, 92, 255}, 26);

    /*
     * The wordmark assembles when the screen opens, then keeps a restrained
     * travelling wave and an occasional specular sweep so the title never
     * becomes completely static.
     */
    float sweep_period = fmodf(intro->time, 7.0f);
    float sweep_x = sweep_period < 1.4f
                        ? x - 40.0f + (sweep_period / 1.4f) * (logo_w + 80.0f)
                        : -1000.0f;
    for (int i = 0; i < 5; ++i)
    {
        float reveal = smoothstep01((intro->time - 0.06f - i * 0.075f) / 0.34f);
        if (reveal <= 0.0f)
            continue;

        float wave = 0.5f + 0.5f * sinf(intro->time * 2.0f - i * 0.72f);
        float lx = x + i * letter_advance;
        float ly = y + (1.0f - reveal) * 11.0f - wave * reveal * 1.5f;
        float brightness = (0.35f + reveal * 0.65f) *
                           (0.92f + wave * 0.08f);
        draw_logo_letter(r, lx + 6.0f, ly + 7.0f, word[i],
                         dim_color((SDL_Color){30, 14, 18, 255}, brightness),
                         false, -1000.0f);
        draw_logo_letter(r, lx + 3.0f, ly + 4.0f, word[i],
                         dim_color(COL_RUST, brightness), false, -1000.0f);
        draw_logo_letter(r, lx, ly, word[i],
                         dim_color(COL_CREAM, brightness), true, sweep_x);
    }

    float rule_reveal = smoothstep01((intro->time - 0.48f) / 0.42f);
    color_rect(r, COL_RUST, x, y + 84.0f, 82.0f * rule_reveal, 4.0f);
    color_rect(r, (SDL_Color){236, 108, 88, 255},
               x, y + 84.0f, 82.0f * rule_reveal, 1.0f);

    float tagline_reveal = smoothstep01((intro->time - 0.68f) / 0.45f);
    draw_text_centered(r, (float)win_w * 0.5f, y + 101.0f, 1.0f,
                       dim_color((SDL_Color){182, 190, 184, 255},
                                 tagline_reveal),
                       "THEY TOOK HER. BRING HER HOME.");
}

static void draw_floor(SDL_Renderer *r, float x, float y, float width)
{
    color_rect(r, COL_INK, x, y, width, 13.0f);
    color_rect(r, (SDL_Color){55, 63, 65, 255}, x, y, width, 8.0f);
    color_rect(r, (SDL_Color){132, 139, 128, 255}, x, y, width, 2.0f);
    color_rect(r, (SDL_Color){29, 36, 41, 255}, x, y + 8.0f, width, 5.0f);

    for (float bx = x + 20.0f; bx < x + width - 10.0f; bx += 48.0f)
    {
        color_rect(r, (SDL_Color){123, 128, 119, 255}, bx, y + 4.0f, 2.0f, 2.0f);
    }
}

static void draw_ladder(SDL_Renderer *r, float x, float y, float height)
{
    color_rect(r, (SDL_Color){36, 31, 24, 255}, x, y, 5.0f, height);
    color_rect(r, (SDL_Color){199, 145, 49, 255}, x + 1.0f, y, 2.0f, height);
    color_rect(r, (SDL_Color){36, 31, 24, 255}, x + 25.0f, y, 5.0f, height);
    color_rect(r, (SDL_Color){199, 145, 49, 255}, x + 26.0f, y, 2.0f, height);
    for (float ry = y + 5.0f; ry < y + height; ry += 11.0f)
    {
        color_rect(r, (SDL_Color){67, 49, 26, 255}, x + 3.0f, ry + 1.0f, 24.0f, 3.0f);
        color_rect(r, COL_AMBER, x + 4.0f, ry, 22.0f, 2.0f);
    }
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

static void draw_agent(SDL_Renderer *r, float x, float y, float scale,
                       int dir, bool walking, float time)
{
    float stride = walking ? sinf(time * 8.0f) : 0.0f;
    float bob = walking ? fabsf(sinf(time * 8.0f)) * 0.65f
                        : sinf(time * 2.0f) * 0.5f;
    bool blinking = !walking && fmodf(time, 4.2f) < 0.13f;

    color_rect(r, (SDL_Color){2, 4, 7, 150},
               x + 2.0f * scale, y + 31.0f * scale,
               24.0f * scale, 3.0f * scale);

    sprite_rect(r, x, y + fabsf(stride) * 0.8f, 28.0f, dir, scale,
                5 + stride * 0.7f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + fabsf(stride) * 0.8f, 28.0f, dir, scale,
                6 + stride * 0.7f, 22, 5, 8,
                (SDL_Color){28, 63, 92, 255});
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * 0.8f, 28.0f, dir, scale,
                15 - stride * 0.7f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * 0.8f, 28.0f, dir, scale,
                16 - stride * 0.7f, 22, 5, 8,
                (SDL_Color){35, 78, 105, 255});

    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 5, 10, 17, 14, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 7, 11, 13, 12,
                (SDL_Color){35, 102, 142, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 8, 12, 4, 9,
                (SDL_Color){60, 148, 171, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 8, 20, 11, 2, COL_AMBER);

    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 9, 1, 11, 11, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 10, 3, 9, 8,
                (SDL_Color){210, 154, 105, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 9, 1, 11, 4,
                (SDL_Color){70, 38, 28, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 8, 4, 14, 2, COL_RUST);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 17, 6, 2, 2,
                blinking ? (SDL_Color){70, 38, 28, 255} : COL_CREAM);

    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 18, 13, 8, 5, COL_INK);
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 19, 14, 7, 3,
                (SDL_Color){209, 154, 105, 255});
    sprite_rect(r, x, y + bob, 28.0f, dir, scale, 24, 12, 9, 4,
                (SDL_Color){31, 38, 43, 255});
}

static void draw_guard(SDL_Renderer *r, float x, float y, float scale,
                       int dir, bool walking, float time)
{
    float stride = walking ? sinf(time * 7.0f) : 0.0f;
    float bob = walking ? fabsf(stride) * 0.5f
                        : sinf(time * 1.7f) * 0.35f;

    color_rect(r, (SDL_Color){2, 4, 7, 150},
               x + 2.0f * scale, y + 31.0f * scale,
               23.0f * scale, 3.0f * scale);

    sprite_rect(r, x, y + fabsf(stride) * 0.5f, 27.0f, dir, scale,
                5 + stride * 0.6f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + fabsf(stride) * 0.5f, 27.0f, dir, scale,
                6 + stride * 0.6f, 22, 5, 8,
                (SDL_Color){42, 49, 39, 255});
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * 0.5f, 27.0f, dir, scale,
                15 - stride * 0.6f, 21, 7, 11, COL_INK);
    sprite_rect(r, x, y + (1.0f - fabsf(stride)) * 0.5f, 27.0f, dir, scale,
                16 - stride * 0.6f, 22, 5, 8,
                (SDL_Color){42, 49, 39, 255});

    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 5, 10, 17, 14, COL_INK);
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 7, 11, 13, 12,
                (SDL_Color){76, 91, 69, 255});
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 8, 12, 4, 8,
                (SDL_Color){116, 129, 86, 255});

    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 9, 2, 11, 10, COL_INK);
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 10, 4, 9, 7,
                (SDL_Color){183, 132, 91, 255});
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 8, 1, 13, 5,
                (SDL_Color){47, 57, 43, 255});
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 10, 2, 9, 2,
                (SDL_Color){116, 129, 86, 255});
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 17, 6, 2, 2, COL_RUST);

    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 18, 13, 8, 5, COL_INK);
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 19, 14, 7, 3,
                (SDL_Color){183, 132, 91, 255});
    sprite_rect(r, x, y + bob, 27.0f, dir, scale, 24, 12, 8, 4,
                (SDL_Color){24, 29, 31, 255});
}

static void draw_crate(SDL_Renderer *r, float x, float y)
{
    color_rect(r, COL_INK, x - 2.0f, y - 2.0f, 36.0f, 36.0f);
    color_rect(r, (SDL_Color){111, 70, 39, 255}, x, y, 32.0f, 32.0f);
    color_rect(r, (SDL_Color){166, 105, 53, 255}, x + 3.0f, y + 3.0f, 26.0f, 4.0f);
    color_rect(r, (SDL_Color){72, 48, 32, 255}, x + 3.0f, y + 25.0f, 26.0f, 4.0f);
    color_rect(r, (SDL_Color){144, 91, 47, 255}, x + 3.0f, y + 3.0f, 4.0f, 26.0f);
    color_rect(r, (SDL_Color){72, 48, 32, 255}, x + 25.0f, y + 3.0f, 4.0f, 26.0f);
    set_color(r, (SDL_Color){70, 45, 30, 255});
    SDL_RenderLine(r, x + 7.0f, y + 7.0f, x + 25.0f, y + 25.0f);
    SDL_RenderLine(r, x + 25.0f, y + 7.0f, x + 7.0f, y + 25.0f);
}

static void draw_keycard(SDL_Renderer *r, float x, float y, float time)
{
    float glow = 0.5f + 0.5f * sinf(time * 3.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_rgba(r, COL_CYAN.r, COL_CYAN.g, COL_CYAN.b,
             (Uint8)(25.0f + glow * 32.0f));
    fill_rect(r, x - 6.0f, y - 6.0f, 30.0f, 34.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    color_rect(r, COL_INK, x - 1.0f, y - 1.0f, 20.0f, 24.0f);
    color_rect(r, COL_CYAN, x, y, 18.0f, 22.0f);
    color_rect(r, (SDL_Color){16, 77, 83, 255}, x + 3.0f, y + 4.0f, 12.0f, 14.0f);
    color_rect(r, COL_CREAM, x + 4.0f, y + 5.0f, 6.0f, 5.0f);
    color_rect(r, COL_AMBER, x + 12.0f, y + 5.0f, 2.0f, 5.0f);
    color_rect(r, (SDL_Color){157, 220, 207, 255}, x + 4.0f, y + 14.0f, 9.0f, 2.0f);
}

static void draw_exit(SDL_Renderer *r, float x, float y, float time)
{
    float pulse = 0.5f + 0.5f * sinf(time * 2.4f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_rgba(r, COL_CYAN.r, COL_CYAN.g, COL_CYAN.b,
             (Uint8)(16.0f + pulse * 18.0f));
    fill_rect(r, x - 5.0f, y - 5.0f, 49.0f, 66.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    color_rect(r, COL_INK, x, y, 39.0f, 61.0f);
    color_rect(r, (SDL_Color){47, 58, 62, 255}, x + 3.0f, y + 3.0f, 33.0f, 58.0f);
    color_rect(r, (SDL_Color){72, 82, 81, 255}, x + 6.0f, y + 6.0f, 27.0f, 51.0f);
    color_rect(r, (SDL_Color){17, 24, 28, 255}, x + 10.0f, y + 21.0f, 19.0f, 31.0f);
    color_rect(r, COL_CYAN, x + 8.0f, y + 10.0f, 23.0f, 4.0f);
    draw_text(r, x + 12.0f, y + 27.0f, 0.75f, COL_CYAN, "GO");

    float scan_y = y + 20.0f + fmodf(time * 12.0f, 30.0f);
    color_rect(r, dim_color(COL_CYAN, 0.58f), x + 11.0f, scan_y, 17.0f, 1.0f);
}

static void draw_guard_light(SDL_Renderer *r, float x, float y,
                             int dir, float time)
{
    float scan = sinf(time * 0.9f) * 20.0f;
    float far_x = x + (float)dir * 158.0f;
    SDL_Vertex vertices[3] = {
        {{x, y}, {0.91f, 0.64f, 0.21f, 0.16f}, {0.0f, 0.0f}},
        {{far_x, y + scan - 33.0f}, {0.91f, 0.64f, 0.21f, 0.0f}, {0.0f, 0.0f}},
        {{far_x, y + scan + 35.0f}, {0.91f, 0.64f, 0.21f, 0.0f}, {0.0f, 0.0f}}};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, vertices, 3, NULL, 0);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void render_building(SDL_Renderer *r, const Intro *intro, int win_w)
{
    const float x = ((float)win_w - 724.0f) * 0.5f;
    float scene_reveal = smoothstep01((intro->time - 0.28f) / 0.72f);
    const float y = 181.0f + (1.0f - scene_reveal) * 8.0f;
    const float w = 724.0f;
    const float h = 266.0f;
    const float upper_floor = y + 121.0f;
    const float lower_floor = y + 247.0f;

    color_rect(r, (SDL_Color){4, 6, 10, 180}, x + 9.0f, y + 10.0f, w, h);
    color_rect(r, (SDL_Color){52, 63, 76, 255}, x, y, w, h);
    color_rect(r, COL_WALL, x + 5.0f, y + 7.0f, w - 10.0f, h - 12.0f);
    color_rect(r, (SDL_Color){13, 19, 29, 255},
               x + 12.0f, y + 15.0f, w - 24.0f, h - 27.0f);

    /* Warm ceiling light settles into each floor; two fixtures per room. */
    fx_vgrad(r, x + 12.0f, y + 15.0f, w - 24.0f, 62.0f,
             (SDL_Color){240, 196, 120, 255}, 22,
             (SDL_Color){240, 196, 120, 255}, 0);
    fx_vgrad(r, x + 12.0f, upper_floor + 13.0f, w - 24.0f, 58.0f,
             (SDL_Color){240, 196, 120, 255}, 18,
             (SDL_Color){240, 196, 120, 255}, 0);
    for (int fixture = 0; fixture < 4; ++fixture)
    {
        float fxp = x + 150.0f + (float)(fixture % 2) * 400.0f;
        float fy = fixture < 2 ? y + 15.0f : upper_floor + 13.0f;
        float flicker = fixture == 1 &&
                                fmodf(intro->time * 1.6f, 4.4f) < 0.07f
                            ? 0.35f
                            : 1.0f;
        SDL_Color lamp = fx_dim((SDL_Color){248, 205, 130, 255}, flicker);
        color_rect(r, (SDL_Color){16, 21, 31, 255},
                   fxp - 12.0f, fy, 24.0f, 4.0f);
        color_rect(r, lamp, fxp - 9.0f, fy + 2.0f, 18.0f, 2.0f);
        fx_glow(r, fxp, fy + 4.0f, 16.0f, lamp, (Uint8)(64.0f * flicker));
        fx_light_cone(r, fxp, fy + 3.0f, 11.0f, 42.0f, 82.0f,
                      (SDL_Color){248, 205, 130, 255},
                      (Uint8)(26.0f * flicker));
    }

    /* Roof, columns and room seams form one continuous environment. */
    color_rect(r, COL_INK, x - 4.0f, y - 5.0f, w + 8.0f, 12.0f);
    color_rect(r, COL_STEEL, x, y - 3.0f, w, 5.0f);
    color_rect(r, (SDL_Color){158, 172, 182, 255}, x + 3.0f, y - 3.0f, w - 6.0f, 1.0f);

    /* Rooftop hardware: radio mast, AC boxes and a water tank silhouette. */
    color_rect(r, (SDL_Color){15, 21, 32, 255}, x + 94.0f, y - 29.0f, 3.0f, 24.0f);
    color_rect(r, (SDL_Color){15, 21, 32, 255}, x + 88.0f, y - 22.0f, 15.0f, 2.0f);
    color_rect(r, (SDL_Color){15, 21, 32, 255}, x + 90.0f, y - 15.0f, 11.0f, 2.0f);
    float mast_blink = sinf(intro->time * 2.2f) > 0.7f ? 1.0f : 0.25f;
    color_rect(r, fx_dim((SDL_Color){212, 74, 58, 255}, mast_blink),
               x + 93.0f, y - 33.0f, 5.0f, 4.0f);
    color_rect(r, (SDL_Color){17, 24, 35, 255}, x + 262.0f, y - 13.0f, 34.0f, 8.0f);
    color_rect(r, (SDL_Color){24, 32, 45, 255}, x + 262.0f, y - 13.0f, 34.0f, 2.0f);
    color_rect(r, (SDL_Color){17, 24, 35, 255}, x + 588.0f, y - 27.0f, 46.0f, 20.0f);
    color_rect(r, (SDL_Color){26, 35, 49, 255}, x + 588.0f, y - 27.0f, 46.0f, 3.0f);
    color_rect(r, (SDL_Color){13, 18, 28, 255}, x + 592.0f, y - 7.0f, 4.0f, 4.0f);
    color_rect(r, (SDL_Color){13, 18, 28, 255}, x + 626.0f, y - 7.0f, 4.0f, 4.0f);

    for (float cx = x + 12.0f; cx < x + w - 8.0f; cx += 118.0f)
    {
        color_rect(r, (SDL_Color){23, 31, 39, 255}, cx, y + 15.0f, 3.0f, h - 30.0f);
        color_rect(r, (SDL_Color){39, 48, 54, 255}, cx + 3.0f, y + 15.0f, 1.0f, h - 30.0f);
    }

    draw_floor(r, x + 6.0f, upper_floor, w - 12.0f);
    draw_floor(r, x + 6.0f, lower_floor, w - 12.0f);

    /* Sparse, practical room details replace decorative dashboard chrome. */
    color_rect(r, (SDL_Color){55, 60, 57, 255},
               x + 38.0f, y + 27.0f, 205.0f, 5.0f);
    color_rect(r, (SDL_Color){113, 114, 99, 255},
               x + 38.0f, y + 27.0f, 205.0f, 1.0f);
    color_rect(r, (SDL_Color){17, 23, 29, 255},
               x + 70.0f, y + 33.0f, 5.0f, 51.0f);
    color_rect(r, (SDL_Color){42, 49, 52, 255},
               x + 71.0f, y + 33.0f, 1.0f, 51.0f);

    color_rect(r, (SDL_Color){53, 57, 52, 255},
               x + 470.0f, y + 20.0f, 184.0f, 8.0f);
    color_rect(r, COL_AMBER, x + 500.0f, y + 22.0f, 68.0f, 3.0f);

    color_rect(r, (SDL_Color){19, 26, 33, 255},
               x + 488.0f, upper_floor + 31.0f, 144.0f, 52.0f);
    color_rect(r, (SDL_Color){49, 58, 60, 255},
               x + 496.0f, upper_floor + 39.0f, 128.0f, 4.0f);
    color_rect(r, (SDL_Color){49, 58, 60, 255},
               x + 496.0f, upper_floor + 50.0f, 128.0f, 4.0f);
    for (int i = 0; i < 5; ++i)
    {
        color_rect(r, i == 1 ? COL_AMBER : (SDL_Color){64, 82, 82, 255},
                   x + 503.0f + i * 22.0f, upper_floor + 67.0f, 12.0f, 4.0f);
    }

    draw_text(r, x + 19.0f, upper_floor - 19.0f, 0.75f,
              (SDL_Color){92, 105, 104, 255}, "02");
    draw_text(r, x + 19.0f, lower_floor - 19.0f, 0.75f,
              (SDL_Color){92, 105, 104, 255}, "01");

    /* The route through the facility is readable without explaining itself. */
    draw_ladder(r, x + 354.0f, upper_floor + 10.0f,
                lower_floor - upper_floor - 10.0f);
    float card_hover = sinf(intro->time * 2.2f) * 2.0f;
    draw_keycard(r, x + 230.0f, upper_floor - 35.0f + card_hover,
                 intro->time);
    draw_exit(r, x + 654.0f, upper_floor - 61.0f, intro->time);

    /*
     * A compact, repeating patrol makes the tableau feel alive.  Long pauses
     * at the ends keep the movement readable and leave the title as the focus.
     */
    float guard_phase = fmodf(intro->time + 1.1f, 9.0f);
    float guard_travel;
    int guard_dir;
    bool guard_walking;
    if (guard_phase < 3.2f)
    {
        guard_travel = smoothstep01(guard_phase / 3.2f);
        guard_dir = -1;
        guard_walking = true;
    }
    else if (guard_phase < 4.6f)
    {
        guard_travel = 1.0f;
        guard_dir = -1;
        guard_walking = false;
    }
    else if (guard_phase < 7.8f)
    {
        guard_travel = 1.0f - smoothstep01((guard_phase - 4.6f) / 3.2f);
        guard_dir = 1;
        guard_walking = true;
    }
    else
    {
        guard_travel = 0.0f;
        guard_dir = 1;
        guard_walking = false;
    }
    float guard_x = x + 520.0f - guard_travel * 82.0f;
    float light_x = guard_x + (guard_dir < 0 ? 5.0f : 31.0f);
    draw_guard_light(r, light_x, upper_floor - 28.0f,
                     guard_dir, intro->time);
    draw_guard(r, guard_x, upper_floor - 43.0f, 1.33f,
               guard_dir, guard_walking, intro->time);

    /* Chuck holds position; breathing and blinking keep the pose alive. */
    draw_agent(r, x + 55.0f, lower_floor - 50.0f, 1.55f,
               1, false, intro->time);
    draw_crate(r, x + 145.0f, lower_floor - 34.0f);

    /* A restrained warning stripe belongs to the architecture, not the UI. */
    for (int i = 0; i < 6; ++i)
    {
        color_rect(r, i % 2 == 0 ? COL_AMBER : (SDL_Color){38, 35, 30, 255},
                   x + 282.0f + i * 13.0f, lower_floor + 2.0f, 13.0f, 3.0f);
    }

    float beacon = sinf(intro->time * 4.0f) > 0.0f ? 1.0f : 0.35f;
    color_rect(r, (SDL_Color){43, 45, 43, 255},
               x + w - 83.0f, y - 14.0f, 29.0f, 9.0f);
    color_rect(r, (SDL_Color){(Uint8)(COL_RUST.r * beacon),
                              (Uint8)(COL_RUST.g * beacon),
                              (Uint8)(COL_RUST.b * beacon), 255},
               x + w - 75.0f, y - 19.0f, 13.0f, 5.0f);
}

static void render_start_prompt(SDL_Renderer *r, const Intro *intro, int win_w)
{
    const SDL_FRect *button = &intro->start_button;
    float cx = (float)win_w * 0.5f;
    float pulse = 0.5f + 0.5f * sinf(intro->time * 2.4f);
    SDL_Color text_color = intro->start_hovered ? COL_CREAM
                                                : (SDL_Color){198, 202, 190, 255};

    if (intro->start_hovered)
        fx_glow(r, cx, button->y + button->h * 0.5f, 120.0f,
                (SDL_Color){222, 96, 76, 255}, 30);

    color_rect(r, COL_INK, button->x - 1.0f, button->y - 1.0f,
               button->w + 2.0f, button->h + 2.0f);
    fx_vgrad(r, button->x, button->y, button->w, button->h,
             intro->start_hovered ? (SDL_Color){36, 46, 60, 255}
                                  : (SDL_Color){24, 32, 44, 255},
             255,
             intro->start_hovered ? (SDL_Color){20, 27, 38, 255}
                                  : (SDL_Color){13, 19, 29, 255},
             255);
    color_rect(r, (SDL_Color){64, 80, 100, 255},
               button->x, button->y, button->w, 1.0f);

    /* Rust corner brackets tie the button to the cinematic UI language. */
    set_color(r, dim_color(COL_RUST, 0.7f + pulse * 0.3f));
    fill_rect(r, button->x - 3.0f, button->y - 3.0f, 9.0f, 2.0f);
    fill_rect(r, button->x - 3.0f, button->y - 3.0f, 2.0f, 9.0f);
    fill_rect(r, button->x + button->w - 6.0f, button->y - 3.0f, 9.0f, 2.0f);
    fill_rect(r, button->x + button->w + 1.0f, button->y - 3.0f, 2.0f, 9.0f);
    fill_rect(r, button->x - 3.0f, button->y + button->h + 1.0f, 9.0f, 2.0f);
    fill_rect(r, button->x - 3.0f, button->y + button->h - 6.0f, 2.0f, 9.0f);
    fill_rect(r, button->x + button->w - 6.0f, button->y + button->h + 1.0f, 9.0f, 2.0f);
    fill_rect(r, button->x + button->w + 1.0f, button->y + button->h - 6.0f, 2.0f, 9.0f);

    color_rect(r, dim_color(COL_RUST, 0.55f + pulse * 0.45f),
               button->x + 12.0f, button->y + 13.0f, 7.0f, 7.0f);
    color_rect(r, dim_color((SDL_Color){255, 170, 150, 255}, 0.55f + pulse * 0.45f),
               button->x + 12.0f, button->y + 13.0f, 7.0f, 2.0f);
    draw_text_centered(r, cx + 9.0f, button->y + 13.0f, 1.0f,
                       text_color, "PRESS ENTER TO START");

    float accent_w = intro->start_hovered ? 62.0f : 24.0f + pulse * 18.0f;
    color_rect(r, COL_RUST, cx - accent_w * 0.5f,
               button->y + button->h - 2.0f, accent_w, 2.0f);
}

static float control_hint_width(const char *key, const char *action)
{
    float ch = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 0.75f;
    return ch * (float)SDL_strlen(key) + 10.0f + 6.0f +
           ch * (float)SDL_strlen(action) + 24.0f;
}

static float draw_control_hint(SDL_Renderer *r, float x, float y,
                               const char *key, const char *action)
{
    const float scale = 0.75f;
    float ch = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
    float key_w = ch * (float)SDL_strlen(key) + 10.0f;
    const float key_h = 14.0f;

    /* Keycap: dark bezel, lit top edge, sunken base. */
    color_rect(r, COL_INK, x, y, key_w, key_h);
    color_rect(r, (SDL_Color){40, 51, 66, 255},
               x + 1.0f, y + 1.0f, key_w - 2.0f, key_h - 2.0f);
    color_rect(r, (SDL_Color){78, 94, 112, 255},
               x + 1.0f, y + 1.0f, key_w - 2.0f, 1.0f);
    color_rect(r, (SDL_Color){14, 19, 29, 255},
               x + 1.0f, y + key_h - 2.0f, key_w - 2.0f, 1.0f);
    draw_text(r, x + 5.0f, y + 4.0f, scale,
              (SDL_Color){214, 222, 214, 255}, key);
    draw_text(r, x + key_w + 6.0f, y + 4.0f, scale, COL_MUTED, action);
    return x + key_w + 6.0f + ch * (float)SDL_strlen(action) + 24.0f;
}

static void render_controls(SDL_Renderer *r, int win_w, int win_h)
{
    static const char *keys[] = {"WASD", "W", "SPACE", "S", "E"};
    static const char *actions[] = {"MOVE", "JUMP", "FIRE", "USE", "HACK"};
    float total = 0.0f;
    for (int i = 0; i < 5; ++i)
        total += control_hint_width(keys[i], actions[i]);
    total -= 24.0f;

    float x = ((float)win_w - total) * 0.5f;
    float y = (float)win_h - 30.0f;
    for (int i = 0; i < 5; ++i)
        x = draw_control_hint(r, x, y, keys[i], actions[i]);
}

void intro_render(SDL_Renderer *r, const Intro *intro, int win_w, int win_h)
{
    render_sky(r, intro, win_w, win_h);
    render_logo(r, intro, win_w);
    render_building(r, intro, win_w);
    render_start_prompt(r, intro, win_w);
    render_controls(r, win_w, win_h);

    /* Same finishing pass as gameplay so the menu belongs to the film. */
    fx_vignette(r, win_w, win_h, 64);
    fx_scanlines(r, win_w, win_h, 11);
}
