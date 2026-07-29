/*
 * Chuck's title screen is the shot the story starts from: the tower his
 * fiancee was carried into, seen from the pavement he parked on.  It is not a
 * menu laid over a diagram of the game — it is one deep image built from
 * planes that each sit a step darker or lighter than the plane behind them
 * (sky, far skyline, near skyline, the blocks that crop the frame, the tower,
 * the wet street), so the eye is led from the wordmark down the building to
 * the one lit window that matters and out to the man looking up at it.
 *
 * Only three things on the screen are interface: the wordmark, the start
 * prompt and the control hints.  Everything else is the world, drawn from the
 * same palette and lighting vocabulary as the game (fx.h), so the title is
 * recognisably the same production as the first level.
 */
#include "intro.h"

#include <math.h>

#include "fx.h"

static const SDL_Color COL_INK = {5, 7, 12, 255};
static const SDL_Color COL_CREAM = {236, 238, 224, 255};
static const SDL_Color COL_MUTED = {112, 128, 142, 255};
static const SDL_Color COL_RUST = {198, 62, 50, 255};
static const SDL_Color COL_AMBER = {248, 188, 74, 255};
static const SDL_Color COL_LAMP = {150, 206, 214, 255}; /* cold street lamp */
static const SDL_Color COL_WARM = {240, 190, 112, 255}; /* interior light   */
static const SDL_Color COL_SODIUM = {182, 116, 62, 255}; /* what the road
                                                          * throws back up  */

/* The tower: eight office floors over a lit lobby, tapering slightly toward
 * the roof because the shot looks up at it from street level. */
#define TOWER_FLOORS 8
#define TOWER_PANES 4
#define TOWER_FLOOR_H 30.0f

/* The window the whole composition points at. */
#define TARGET_FLOOR 1
#define TARGET_PANE 2

/*
 * The start plate is sized from its own contents rather than by eye: the
 * chevron, a gap, and the longest label the screen can show, plus equal
 * padding on both sides.  Guessing the width is how the label ends up jammed
 * against one edge with the chevron hanging off the other.
 */
#define START_LABEL "PRESS ENTER TO START"
#define START_TRACK 2.0f
#define START_CHEVRON_W 6.0f
#define START_CHEVRON_GAP 9.0f
#define START_PAD 18.0f
#define START_LABEL_W ((float)(sizeof(START_LABEL) - 1) * \
                       ((float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + START_TRACK) - \
                       START_TRACK)
#define START_PLATE_W (START_CHEVRON_W + START_CHEVRON_GAP + START_LABEL_W + \
                       START_PAD * 2.0f)

typedef struct
{
    float w, h;
    float street_y; /* where the pavement starts; the tower stands on it */
    float roof_y;
    float base_left, base_right;
    float top_left, top_right;
} IntroScene;

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

static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      SDL_Color color, const char *text)
{
    SDL_SetRenderScale(r, scale, scale);
    set_color(r, color);
    SDL_RenderDebugText(r, x / scale, y / scale, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

/*
 * Letterspaced text.  The debug font is set solid, which is fine for the
 * control hints but too tight for the two lines that have to read as type;
 * drawing a character at a time is the only tracking control available.
 */
static float tracked_width(const char *text, float scale, float track)
{
    size_t len = SDL_strlen(text);
    if (len == 0)
        return 0.0f;
    float step = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale + track;
    return (float)len * step - track;
}

static void draw_tracked(SDL_Renderer *r, float x, float y, float scale,
                         float track, SDL_Color color, const char *text)
{
    float step = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale + track;
    char glyph[2] = {0, 0};
    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        glyph[0] = text[i];
        draw_text(r, x + step * (float)i, y, scale, color, glyph);
    }
}

static void draw_tracked_centered(SDL_Renderer *r, float cx, float y,
                                  float scale, float track, SDL_Color color,
                                  const char *text)
{
    draw_tracked(r, cx - tracked_width(text, scale, track) * 0.5f, y,
                 scale, track, color, text);
}

static IntroScene scene_layout(int win_w, int win_h)
{
    IntroScene s;
    s.w = win_w > 0 ? (float)win_w : 800.0f;
    s.h = win_h > 0 ? (float)win_h : 552.0f;
    s.street_y = s.h - 92.0f;
    s.roof_y = 166.0f;

    /*
     * Dead centre.  The wordmark, the start plate and the control hints all
     * sit on the screen's centre line, so a subject nudged seventy pixels off
     * it does not read as composition — it reads as something that failed to
     * line up.  The asymmetry the shot needs is carried on the street instead:
     * the lamp and the man on the left, the car at the kerb on the right.
     *
     * Narrow, though: a slab as wide as it is tall would read as a warehouse,
     * and the point of the shot is the climb.
     */
    float cx = s.w * 0.5f;
    s.base_left = cx - 100.0f;
    s.base_right = cx + 100.0f;
    s.top_left = cx - 90.0f;
    s.top_right = cx + 90.0f;
    return s;
}

static float tower_edge(const IntroScene *s, float y, bool right)
{
    float t = clamp01((y - s->roof_y) / (s->street_y - s->roof_y));
    return right ? s->top_right + (s->base_right - s->top_right) * t
                 : s->top_left + (s->base_left - s->top_left) * t;
}

static float floor_top(const IntroScene *s, int index)
{
    return s->roof_y + 10.0f + TOWER_FLOOR_H * (float)index;
}

static SDL_FRect pane_rect(const IntroScene *s, int fl, int pane)
{
    float top = floor_top(s, fl) + 8.0f;
    float left = tower_edge(s, top, false) + 11.0f;
    float right = tower_edge(s, top, true) - 11.0f;
    float mullion = 5.0f;
    float pane_w = (right - left - mullion * (float)(TOWER_PANES - 1)) /
                   (float)TOWER_PANES;
    return (SDL_FRect){left + (pane_w + mullion) * (float)pane, top,
                       pane_w, 20.0f};
}

/*
 * One deterministic decision per pane.  The wet pavement reflects exactly the
 * windows that are lit, and it can work that out by asking the same question
 * again rather than by the renderer keeping a list.
 */
static bool pane_light(int fl, int pane, float time, SDL_Color *out)
{
    unsigned h = fx_hash((unsigned)(fl * 37 + pane * 7 + 11) * 2654435761u);
    if ((h % 100u) >= 30u)
        return false;
    /* The building wakes up window by window while the title assembles. */
    if (time < 0.55f + (float)(h % 13u) * 0.06f)
        return false;

    float bright = 0.6f + (float)(h % 7u) * 0.055f;
    if ((h & 64u) != 0u &&
        fmodf(time * 1.7f + (float)(h % 97u) * 0.06f, 5.4f) < 0.08f)
        bright *= 0.32f; /* a tube stutters */

    *out = fx_dim((h & 8u) ? (SDL_Color){236, 182, 104, 255}
                           : (SDL_Color){104, 188, 196, 255},
                  bright);
    return true;
}

void intro_init(Intro *intro, int win_w, int win_h)
{
    SDL_zerop(intro);
    int w = win_w > 0 ? win_w : 800;
    int h = win_h > 0 ? win_h : 552;

    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        IntroStar *star = &intro->stars[i];
        star->x = (float)SDL_rand(w);
        /* Everything below the horizon is masonry, so the field is packed
         * into the band of sky that is actually visible. */
        star->y = (float)SDL_rand(i < 118 ? 210 : h / 2);
        star->speed = 2.0f + (float)(i % 5) * 1.7f + (float)SDL_rand(3);
        star->phase = (float)SDL_rand(628) * 0.01f;
    }

    intro->start_button.w = START_PLATE_W;
    intro->start_button.h = 34.0f;
    intro->start_button.x = ((float)w - intro->start_button.w) * 0.5f;
    intro->start_button.y = (float)h - 78.0f;
}

void intro_update(Intro *intro, float dt, int win_w, int win_h,
                  float mouse_x, float mouse_y)
{
    intro->time += dt;

    int w = win_w > 0 ? win_w : 800;
    int h = win_h > 0 ? win_h : 552;
    intro->start_button.x = ((float)w - intro->start_button.w) * 0.5f;
    intro->start_button.y = (float)h - 78.0f;

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

/* ---- Sky ------------------------------------------------------------ */

static void draw_moon(SDL_Renderer *r, float cx, float cy)
{
    const float radius = 14.0f;
    fx_glow(r, cx, cy, 78.0f, (SDL_Color){160, 190, 210, 255}, 30);
    fx_glow(r, cx, cy, 34.0f, (SDL_Color){206, 224, 232, 255}, 34);

    for (int row = -14; row <= 14; ++row)
    {
        float half = sqrtf(fmaxf(0.0f, radius * radius - (float)(row * row)));
        /* Cooler along the lower limb so the disc reads as a sphere. */
        SDL_Color tint = fx_mix((SDL_Color){222, 232, 236, 255},
                                (SDL_Color){168, 186, 202, 255},
                                ((float)row + radius) / (2.0f * radius));
        color_rect(r, tint, cx - half, cy + (float)row, half * 2.0f, 1.0f);
    }
    color_rect(r, (SDL_Color){182, 198, 210, 255}, cx - 6.0f, cy - 5.0f, 5.0f, 4.0f);
    color_rect(r, (SDL_Color){182, 198, 210, 255}, cx + 3.0f, cy + 6.0f, 4.0f, 3.0f);
    color_rect(r, (SDL_Color){192, 208, 218, 255}, cx - 2.0f, cy + 2.0f, 3.0f, 2.0f);
    color_rect(r, (SDL_Color){188, 204, 216, 255}, cx + 6.0f, cy - 2.0f, 2.0f, 2.0f);
}

static void draw_cloud(SDL_Renderer *r, float x, float y, float width,
                       Uint8 alpha)
{
    /* Stepped bands rather than a soft blob, so it belongs to the pixel grid.
     * The rows never overlap, so the alpha does not double up anywhere. */
    static const float rows[6][2] = {
        {0.30f, 0.40f}, {0.16f, 0.68f}, {0.04f, 0.92f},
        {0.00f, 1.00f}, {0.10f, 0.78f}, {0.26f, 0.44f}};
    const float band = 5.0f;
    for (int i = 0; i < 6; ++i)
    {
        float bx = x + rows[i][0] * width;
        float bw = rows[i][1] * width;
        fx_rect_a(r, (SDL_Color){26, 34, 48, 255}, alpha,
                  bx, y + (float)i * band, bw, band);
        if (i == 0)
            fx_rect_a(r, (SDL_Color){64, 78, 96, 255}, (Uint8)(alpha + 18),
                      bx, y, bw, 1.0f);
    }
}

static void render_sky(SDL_Renderer *r, const Intro *intro, const IntroScene *s)
{
    fx_vgrad(r, 0.0f, 0.0f, s->w, s->h,
             (SDL_Color){5, 8, 15, 255}, 255,
             (SDL_Color){18, 27, 39, 255}, 255);

    /* Sodium haze the city throws up behind its own skyline. */
    fx_vgrad(r, 0.0f, 236.0f, s->w, s->street_y - 236.0f,
             (SDL_Color){44, 70, 84, 255}, 0,
             (SDL_Color){58, 84, 92, 255}, 64);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        const IntroStar *star = &intro->stars[i];
        float depth = 0.35f + (float)(i % 5) * 0.15f;
        float twinkle = 0.5f + 0.5f * sinf(intro->time *
                                              (0.65f + (float)(i % 7) * 0.13f) +
                                          star->phase);
        Uint8 alpha = (Uint8)(26.0f + depth * 64.0f +
                              twinkle * (28.0f + depth * 44.0f));
        Uint8 red = i % 7 == 0 ? 214 : 182;
        Uint8 green = i % 7 == 0 ? 196 : 206;
        Uint8 blue = i % 7 == 0 ? 164 : 219;
        float sx = floorf(star->x);
        float sy = floorf(star->y);

        set_rgba(r, red, green, blue, alpha);
        if (depth > 0.85f && i % 3 == 0)
        {
            fill_rect(r, sx, sy, 2.0f, 2.0f);
            if (twinkle > 0.88f)
            {
                set_rgba(r, red, green, blue, (Uint8)((float)alpha * 0.5f));
                fill_rect(r, sx - 1.0f, sy, 4.0f, 1.0f);
                fill_rect(r, sx, sy - 1.0f, 1.0f, 4.0f);
            }
        }
        else
        {
            fill_rect(r, sx, sy, 1.0f, 1.0f);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_moon(r, s->w * 0.135f, 88.0f);

    /* Three sheets of high cloud drift across, one of them past the moon. */
    static const float clouds[3][4] = {
        {40.0f, 62.0f, 168.0f, 34.0f},
        {430.0f, 122.0f, 232.0f, 26.0f},
        {620.0f, 40.0f, 128.0f, 22.0f}};
    for (int i = 0; i < 3; ++i)
    {
        float span = s->w + 260.0f;
        float x = fmodf(clouds[i][0] + intro->time * (3.4f + (float)i * 2.1f),
                        span) -
                  200.0f;
        draw_cloud(r, x, clouds[i][1], clouds[i][2], (Uint8)clouds[i][3]);
    }

    /* One restrained shooting star every few seconds, stepped so it stays
     * inside the same pixel grid as everything else. */
    float meteor = fmodf(intro->time, 11.5f);
    if (meteor >= 3.1f && meteor < 4.3f)
    {
        float p = (meteor - 3.1f) / 1.2f;
        float head_x = -12.0f + p * (s->w + 36.0f);
        float head_y = 26.0f + p * 58.0f;
        float fade = 1.0f - smoothstep01((p - 0.72f) / 0.28f);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 8; i >= 0; --i)
        {
            Uint8 alpha = (Uint8)((28.0f + (float)(8 - i) * 18.0f) * fade);
            set_rgba(r, 202, 224, 228, alpha);
            fill_rect(r, floorf(head_x - (float)i * 7.0f),
                      floorf(head_y - (float)i * 3.0f),
                      i < 2 ? 3.0f : 2.0f, 1.0f);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

/* ---- The city behind the tower -------------------------------------- */

static void render_skyline(SDL_Renderer *r, const Intro *intro,
                           const IntroScene *s)
{
    /* Far row: hazy, no detail, only a ragged edge. */
    static const int far_heights[] = {58, 84, 66, 96, 74, 88, 62, 104, 70};
    const float far_base = 406.0f;
    int cursor = -24;
    for (int i = 0; (float)cursor < s->w + 30.0f; ++i)
    {
        int width = 62 + (i * 17) % 34;
        int height = far_heights[i % (int)SDL_arraysize(far_heights)];
        color_rect(r, (SDL_Color){16, 23, 34, 255}, (float)cursor,
                   far_base - (float)height, (float)width, (float)height);
        cursor += width + 5;
    }
    fx_vgrad(r, 0.0f, far_base - 96.0f, s->w, 96.0f,
             (SDL_Color){50, 74, 88, 255}, 0,
             (SDL_Color){52, 76, 88, 255}, 54);

    /* Near row: a value darker, with the last windows still burning. */
    static const int heights[] = {44, 68, 52, 82, 58, 74, 48, 90, 62, 78, 54};
    const float base = 442.0f;
    cursor = -10;
    for (int i = 0; (float)cursor < s->w + 30.0f; ++i)
    {
        int width = 48 + (i * 13) % 30;
        int height = heights[i % (int)SDL_arraysize(heights)];
        float top = base - (float)height;
        color_rect(r, (SDL_Color){10, 15, 24, 255}, (float)cursor, top,
                   (float)width, (float)height);
        color_rect(r, (SDL_Color){19, 27, 39, 255}, (float)cursor, top,
                   (float)width, 1.0f);

        for (int w = 0; w < width / 12; ++w)
        {
            unsigned h = fx_hash((unsigned)(i * 53 + w * 17));
            if ((h % 6u) != 0u)
                continue;
            SDL_Color light = (h & 8u) ? (SDL_Color){106, 82, 44, 255}
                                       : (SDL_Color){40, 80, 88, 255};
            color_rect(r, light, (float)(cursor + 6 + w * 12),
                       top + 10.0f + (float)(h % 4u) * 12.0f, 5.0f, 3.0f);
        }
        if (i % 4 == 1)
        {
            float ax = (float)cursor + (float)width * 0.5f;
            color_rect(r, (SDL_Color){14, 20, 30, 255}, ax, top - 13.0f, 2.0f, 13.0f);
            float blink = sinf(intro->time * 1.8f + (float)i) > 0.75f ? 1.0f : 0.18f;
            color_rect(r, fx_dim((SDL_Color){212, 74, 58, 255}, blink),
                       ax - 1.0f, top - 16.0f, 4.0f, 3.0f);
        }
        cursor += width + 8;
    }
}

/*
 * The two blocks that crop the frame.  They carry almost no light: their job
 * is to be the darkest thing on screen so the tower reads as further away,
 * and to close the composition at both edges.
 */
/*
 * The blocks the tower shares its street with.  They are what the tower gets
 * to overlap, which is the only way a flat drawing says one thing is in front
 * of another.
 */
static void draw_neighbour_block(SDL_Renderer *r, const IntroScene *s,
                                 float x, float top, float width, unsigned seed)
{
    color_rect(r, (SDL_Color){9, 13, 21, 255}, x, top, width,
               s->street_y - top);
    color_rect(r, (SDL_Color){18, 24, 34, 255}, x, top, width, 2.0f);
    color_rect(r, (SDL_Color){13, 18, 27, 255}, x + width - 3.0f, top, 3.0f,
               s->street_y - top);

    /* Roof plant, then whatever is left burning inside. */
    color_rect(r, (SDL_Color){9, 13, 21, 255}, x + width * 0.5f - 11.0f,
               top - 9.0f, 22.0f, 9.0f);
    color_rect(r, (SDL_Color){16, 22, 32, 255}, x + width * 0.5f - 11.0f,
               top - 9.0f, 22.0f, 2.0f);

    int cols = (int)(width / 17.0f);
    int rows = (int)((s->street_y - top) / 22.0f);
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            unsigned h = fx_hash(seed + (unsigned)(row * 29 + col * 7));
            if ((h % 8u) != 0u)
                continue;
            /* Kept dim: these are background, and a bright dot back here
             * competes with the window the shot is about. */
            SDL_Color light = (h & 8u) ? (SDL_Color){58, 44, 24, 255}
                                       : (SDL_Color){24, 50, 54, 255};
            color_rect(r, light, x + 8.0f + (float)col * 17.0f,
                       top + 12.0f + (float)row * 22.0f, 6.0f, 4.0f);
        }
    }
}

static void render_flanking_blocks(SDL_Renderer *r, const IntroScene *s)
{
    const SDL_Color mass = {7, 10, 17, 255};
    const SDL_Color edge = {15, 20, 30, 255};

    /*
     * Mid-ground slabs at four different heights, so the rooflines step
     * unevenly up to the tower on both sides instead of mirroring it.  They
     * fill what was a dead quarter of the frame and give the tower something
     * to stand in front of.
     */
    draw_neighbour_block(r, s, 116.0f, 342.0f, 118.0f, 41u);
    draw_neighbour_block(r, s, s->base_left - 76.0f, 262.0f, 118.0f, 17u);
    draw_neighbour_block(r, s, s->base_right - 22.0f, 312.0f, 130.0f, 91u);
    draw_neighbour_block(r, s, s->base_right + 96.0f, 358.0f, 104.0f, 63u);

    /* Left: a low block with a water tank on its roof, cut by the frame. */
    color_rect(r, mass, -12.0f, 302.0f, 152.0f, s->street_y - 302.0f + 40.0f);
    color_rect(r, edge, -12.0f, 302.0f, 152.0f, 2.0f);
    color_rect(r, mass, 40.0f, 274.0f, 34.0f, 30.0f);
    color_rect(r, edge, 40.0f, 274.0f, 34.0f, 2.0f);
    color_rect(r, mass, 48.0f, 262.0f, 4.0f, 12.0f);
    color_rect(r, mass, 62.0f, 262.0f, 4.0f, 12.0f);
    color_rect(r, (SDL_Color){64, 52, 30, 255}, 16.0f, 348.0f, 6.0f, 4.0f);
    color_rect(r, (SDL_Color){30, 58, 62, 255}, 104.0f, 384.0f, 6.0f, 4.0f);

    /* Right: nearer, taller, also cut by the frame, so the composition is
     * closed on both sides instead of draining out of them. */
    color_rect(r, mass, s->w - 138.0f, 262.0f, 150.0f,
               s->street_y - 262.0f + 40.0f);
    color_rect(r, edge, s->w - 138.0f, 262.0f, 150.0f, 2.0f);
    color_rect(r, mass, s->w - 92.0f, 226.0f, 3.0f, 36.0f);
    color_rect(r, mass, s->w - 100.0f, 236.0f, 19.0f, 2.0f);
    color_rect(r, (SDL_Color){58, 46, 28, 255}, s->w - 122.0f, 314.0f, 6.0f, 4.0f);
    color_rect(r, (SDL_Color){58, 46, 28, 255}, s->w - 102.0f, 360.0f, 6.0f, 4.0f);
    color_rect(r, (SDL_Color){28, 54, 58, 255}, s->w - 56.0f, 336.0f, 6.0f, 4.0f);
}

/* ---- The tower ------------------------------------------------------- */

static void draw_roof_hardware(SDL_Renderer *r, const IntroScene *s, float time)
{
    const SDL_Color mass = {13, 18, 28, 255};
    const SDL_Color lit = {26, 34, 47, 255};
    float left = s->top_left;
    float right = s->top_right;

    /*
     * Everything on the roof stays inside about twenty-five pixels: the
     * wordmark's tagline sits just above, and a mast tall enough to look
     * right would grow straight into it.
     */
    float mast_x = left + 30.0f;
    color_rect(r, mass, mast_x, s->roof_y - 22.0f, 3.0f, 22.0f);
    color_rect(r, mass, mast_x - 5.0f, s->roof_y - 15.0f, 13.0f, 2.0f);
    float blink = sinf(time * 2.1f) > 0.62f ? 1.0f : 0.16f;
    SDL_Color beacon = fx_dim((SDL_Color){226, 78, 60, 255}, blink);
    if (blink > 0.5f)
        fx_glow(r, mast_x + 1.5f, s->roof_y - 25.0f, 15.0f, beacon, 96);
    color_rect(r, beacon, mast_x - 1.0f, s->roof_y - 26.0f, 5.0f, 4.0f);

    /* Water tank on its legs, and the plant room over the lift shaft. */
    color_rect(r, mass, left + 58.0f, s->roof_y - 24.0f, 26.0f, 24.0f);
    color_rect(r, lit, left + 58.0f, s->roof_y - 24.0f, 26.0f, 2.0f);
    color_rect(r, mass, left + 63.0f, s->roof_y - 30.0f, 4.0f, 6.0f);
    color_rect(r, mass, left + 75.0f, s->roof_y - 30.0f, 4.0f, 6.0f);

    color_rect(r, mass, right - 62.0f, s->roof_y - 16.0f, 40.0f, 16.0f);
    color_rect(r, lit, right - 62.0f, s->roof_y - 16.0f, 40.0f, 2.0f);
    color_rect(r, mass, right - 54.0f, s->roof_y - 20.0f, 7.0f, 4.0f);
    color_rect(r, mass, right - 38.0f, s->roof_y - 20.0f, 7.0f, 4.0f);

    /* Satellite dish on the parapet. */
    color_rect(r, mass, right - 18.0f, s->roof_y - 10.0f, 3.0f, 10.0f);
    color_rect(r, mass, right - 23.0f, s->roof_y - 19.0f, 12.0f, 3.0f);
    color_rect(r, mass, right - 23.0f, s->roof_y - 16.0f, 3.0f, 4.0f);

    /* Cornice: overhangs both faces, lit along its top arris. */
    color_rect(r, (SDL_Color){10, 14, 22, 255}, left - 7.0f, s->roof_y - 1.0f,
               right - left + 14.0f, 11.0f);
    color_rect(r, (SDL_Color){46, 57, 70, 255}, left - 7.0f, s->roof_y - 1.0f,
               right - left + 14.0f, 2.0f);
    color_rect(r, (SDL_Color){96, 112, 126, 255}, left - 5.0f, s->roof_y - 1.0f,
               right - left + 10.0f, 1.0f);
}

static void draw_fire_escape(SDL_Renderer *r, const IntroScene *s)
{
    /* Bolted to the left face: it breaks the box silhouette, and it is the
     * one thing on the wall that says this building gets climbed. */
    const SDL_Color steel = {20, 26, 36, 255};
    const SDL_Color rail = {34, 43, 55, 255};
    for (int i = 0; i < TOWER_FLOORS; ++i)
    {
        float y = floor_top(s, i) + 30.0f;
        float x = tower_edge(s, y, false);
        color_rect(r, steel, x - 22.0f, y, 24.0f, 3.0f);
        color_rect(r, rail, x - 22.0f, y - 9.0f, 2.0f, 9.0f);
        color_rect(r, rail, x - 22.0f, y - 9.0f, 24.0f, 1.0f);

        set_color(r, steel);
        if (i + 1 < TOWER_FLOORS)
        {
            float ny = floor_top(s, i + 1) + 30.0f;
            float nx = tower_edge(s, ny, false);
            SDL_RenderLine(r, x - 20.0f, y + 3.0f, nx - 4.0f, ny);
            SDL_RenderLine(r, x - 20.0f, y + 6.0f, nx - 4.0f, ny + 3.0f);
        }
    }
}

static void draw_lobby(SDL_Renderer *r, const IntroScene *s, float time,
                       float wake)
{
    float top = floor_top(s, TOWER_FLOORS);
    float left = tower_edge(s, top, false);
    float right = tower_edge(s, top, true);
    float floor_line = s->street_y;

    /*
     * Double-height glazing with the room behind it.  This is the brightest
     * large area in the frame, which is what makes the way in obvious — but
     * it is lit from its own ceiling and falls off downward, because a flat
     * wash of amber would read as a painted panel and not as a room.
     */
    color_rect(r, (SDL_Color){8, 11, 18, 255}, left, top, right - left,
               floor_line - top);
    fx_vgrad(r, left + 4.0f, top + 5.0f, right - left - 8.0f,
             floor_line - top - 7.0f,
             fx_dim((SDL_Color){118, 88, 52, 255}, wake), 255,
             fx_dim((SDL_Color){40, 33, 27, 255}, wake), 255);

    /* Ceiling coves, seen through the glass. */
    for (int i = 0; i < 3; ++i)
    {
        float cx = left + 30.0f + (float)i * (right - left - 60.0f) * 0.5f;
        color_rect(r, fx_dim(COL_WARM, wake), cx - 10.0f, top + 6.0f, 20.0f, 2.0f);
        fx_glow(r, cx, top + 9.0f, 26.0f, COL_WARM, (Uint8)(66.0f * wake));
    }

    /* Reception counter, with whoever is left behind it, and the lift core
     * glowing cold at the back of the room. */
    float counter_x = left + 96.0f;
    color_rect(r, (SDL_Color){15, 19, 27, 255}, counter_x, floor_line - 17.0f,
               54.0f, 17.0f);
    color_rect(r, fx_dim((SDL_Color){150, 114, 66, 255}, wake), counter_x,
               floor_line - 17.0f, 54.0f, 2.0f);
    float lean = sinf(time * 0.8f) * 1.5f;
    color_rect(r, (SDL_Color){12, 16, 23, 255}, counter_x + 22.0f + lean,
               floor_line - 29.0f, 8.0f, 12.0f);
    color_rect(r, (SDL_Color){10, 14, 20, 255}, counter_x + 23.0f + lean,
               floor_line - 33.0f, 6.0f, 5.0f);

    color_rect(r, (SDL_Color){12, 16, 24, 255}, right - 40.0f,
               floor_line - 38.0f, 28.0f, 38.0f);
    color_rect(r, fx_dim((SDL_Color){64, 140, 146, 255}, wake), right - 35.0f,
               floor_line - 34.0f, 18.0f, 2.0f);

    /* Glazing bars, so the light is read through a wall of glass. */
    for (float mx = left + 16.0f; mx < right - 8.0f; mx += 22.0f)
        color_rect(r, (SDL_Color){10, 13, 20, 255}, mx, top + 4.0f, 2.0f,
                   floor_line - top - 5.0f);
    color_rect(r, (SDL_Color){10, 13, 20, 255}, left + 4.0f, top + 4.0f,
               right - left - 8.0f, 2.0f);

    /* The entrance itself: two leaves, a mullion, a lit canopy over them. */
    float door_x = left + 26.0f;
    color_rect(r, (SDL_Color){7, 10, 16, 255}, door_x - 3.0f,
               floor_line - 34.0f, 58.0f, 34.0f);
    fx_vgrad(r, door_x, floor_line - 31.0f, 52.0f, 31.0f,
             fx_dim((SDL_Color){206, 158, 98, 255}, wake), 255,
             fx_dim((SDL_Color){132, 100, 62, 255}, wake), 255);
    color_rect(r, (SDL_Color){9, 12, 19, 255}, door_x + 24.0f,
               floor_line - 31.0f, 4.0f, 31.0f);
    color_rect(r, (SDL_Color){7, 10, 16, 255}, door_x - 6.0f,
               floor_line - 40.0f, 64.0f, 5.0f);
    color_rect(r, fx_dim(COL_AMBER, wake * 0.85f), door_x - 6.0f,
               floor_line - 36.0f, 64.0f, 2.0f);

    /* Sill line: the building meets the pavement on a lit concrete edge. */
    color_rect(r, (SDL_Color){36, 44, 54, 255}, left - 4.0f, floor_line - 4.0f,
               right - left + 8.0f, 4.0f);
    color_rect(r, (SDL_Color){64, 76, 88, 255}, left - 4.0f, floor_line - 4.0f,
               right - left + 8.0f, 1.0f);
}

static void draw_target_window(SDL_Renderer *r, const IntroScene *s, float time)
{
    SDL_FRect p = pane_rect(s, TARGET_FLOOR, TARGET_PANE);
    float on = smoothstep01((time - 1.35f) / 0.5f);
    if (on <= 0.0f)
        return;
    /* Struck once as it comes on, then held. */
    float strike = time < 1.55f ? 1.0f : (time < 1.62f ? 0.4f : 1.0f);
    float level = on * strike;

    fx_glow(r, p.x + p.w * 0.5f, p.y + p.h * 0.5f, 74.0f,
            (SDL_Color){250, 206, 130, 255}, (Uint8)(58.0f * level));
    fx_vgrad(r, p.x, p.y, p.w, p.h,
             fx_dim((SDL_Color){255, 226, 158, 255}, level), 255,
             fx_dim((SDL_Color){236, 176, 92, 255}, level), 255);

    /* Two figures: one held in a chair, one standing over her. */
    float base = p.y + p.h;
    SDL_Color ink = {10, 10, 14, 255};
    color_rect(r, ink, p.x + 7.0f, base - 11.0f, 6.0f, 11.0f);
    color_rect(r, ink, p.x + 6.0f, base - 14.0f, 5.0f, 4.0f);
    color_rect(r, ink, p.x + 12.0f, base - 12.0f, 2.0f, 12.0f);

    float pace = sinf(time * 0.55f) * 3.0f;
    color_rect(r, ink, p.x + p.w - 13.0f + pace, base - 17.0f, 6.0f, 17.0f);
    color_rect(r, ink, p.x + p.w - 13.0f + pace, base - 21.0f, 5.0f, 5.0f);

    /* Frame last, so the figures sit behind the glazing bars. */
    color_rect(r, (SDL_Color){28, 22, 18, 255}, p.x, p.y, p.w, 1.0f);
    color_rect(r, fx_dim((SDL_Color){255, 236, 190, 255}, level),
               p.x, base - 1.0f, p.w, 1.0f);
}

static void render_tower(SDL_Renderer *r, const Intro *intro,
                         const IntroScene *s)
{
    float time = intro->time;
    float wake = smoothstep01((time - 0.35f) / 0.8f);

    draw_roof_hardware(r, s, time);

    /* Body: per-floor bands, so the taper steps rather than slopes — a slope
     * cannot be drawn on this grid. */
    for (int i = 0; i < TOWER_FLOORS; ++i)
    {
        float top = floor_top(s, i);
        float bottom = top + TOWER_FLOOR_H;
        float left = fminf(tower_edge(s, top, false),
                           tower_edge(s, bottom, false));
        float right = fmaxf(tower_edge(s, top, true),
                            tower_edge(s, bottom, true));

        /* Spandrel: one lit arris on top, dark concrete, deep soffit.  The
         * concrete stays low in value on purpose — every bit of brightness on
         * this wall is meant to be a window. */
        color_rect(r, (SDL_Color){9, 12, 19, 255}, left, top, right - left,
                   TOWER_FLOOR_H);
        color_rect(r, (SDL_Color){21, 27, 36, 255}, left, top, right - left, 7.0f);
        color_rect(r, (SDL_Color){44, 54, 66, 255}, left, top, right - left, 1.0f);
        color_rect(r, (SDL_Color){6, 8, 13, 255}, left, top + 7.0f,
                   right - left, 1.0f);

        for (int pane = 0; pane < TOWER_PANES; ++pane)
        {
            SDL_FRect p = pane_rect(s, i, pane);
            SDL_Color light;

            /* Sky reflected in dark glass; without it the unlit panes read as
             * holes in the wall rather than as windows. */
            fx_vgrad(r, p.x, p.y, p.w, p.h,
                     (SDL_Color){38, 54, 72, 255}, 104,
                     (SDL_Color){11, 17, 27, 255}, 40);

            if (i == TARGET_FLOOR && pane == TARGET_PANE)
                continue;
            if (pane_light(i, pane, time, &light))
            {
                /* A lit office is a ceiling, a room and a floor, not a slab
                 * of colour: the ramp plus a blind and a desk line keep the
                 * pane from reading as a painted rectangle. */
                unsigned h = fx_hash((unsigned)(i * 91 + pane * 13));
                fx_vgrad(r, p.x, p.y, p.w, p.h,
                         fx_mix(light, COL_CREAM, 0.18f), 255,
                         fx_dim(light, 0.45f), 255);
                color_rect(r, fx_dim(light, 0.24f), p.x, p.y,
                           p.w, 2.0f + (float)(h % 3u));
                if ((h & 32u) != 0u) /* half-drawn blind */
                    color_rect(r, fx_dim(light, 0.5f), p.x, p.y,
                               p.w, p.h * 0.42f);
                color_rect(r, fx_dim(light, 0.38f), p.x, p.y + p.h - 6.0f,
                           p.w, 1.0f);
                color_rect(r, fx_mix(light, COL_CREAM, 0.4f), p.x,
                           p.y + p.h - 1.0f, p.w, 1.0f);

                /* Someone is still working on a couple of the floors. */
                if ((h % 3u) == 0u)
                {
                    float walk = (h & 16u) ? sinf(time * 0.5f +
                                                  (float)(h % 31u) * 0.2f) *
                                                 (p.w * 0.24f)
                                           : 0.0f;
                    float px = p.x + p.w * 0.5f + walk;
                    color_rect(r, (SDL_Color){13, 13, 17, 255}, px - 2.0f,
                               p.y + p.h - 12.0f, 4.5f, 12.0f);
                    color_rect(r, (SDL_Color){13, 13, 17, 255}, px - 1.5f,
                               p.y + p.h - 15.0f, 3.5f, 3.5f);
                }
                fx_glow(r, p.x + p.w * 0.5f, p.y + p.h * 0.5f, 26.0f, light, 22);
            }

            /* Two leaves per opening: the transom is what gives the wall its
             * scale, and it crosses lit and dark panes alike. */
            color_rect(r, (SDL_Color){10, 13, 20, 255},
                       p.x + floorf(p.w * 0.5f), p.y, 1.0f, p.h);
        }

        /* Piers between the openings, run full height so the facade is a
         * grid of structure rather than a stack of shelves. */
        for (int pane = 0; pane <= TOWER_PANES; ++pane)
        {
            SDL_FRect p = pane_rect(s, i, pane < TOWER_PANES ? pane : pane - 1);
            float px = pane < TOWER_PANES ? p.x - 5.0f : p.x + p.w;
            float pw = pane < TOWER_PANES ? 5.0f : 5.0f;
            if (pane == 0)
            {
                px = left;
                pw = p.x - left;
            }
            else if (pane == TOWER_PANES)
            {
                px = p.x + p.w;
                pw = right - px;
            }
            color_rect(r, (SDL_Color){17, 22, 31, 255}, px, top + 8.0f,
                       pw, TOWER_FLOOR_H - 8.0f);
            color_rect(r, (SDL_Color){28, 36, 47, 255}, px, top + 8.0f,
                       1.0f, TOWER_FLOOR_H - 8.0f);
        }
    }

    draw_fire_escape(r, s);
    draw_lobby(r, s, time, wake);
    draw_target_window(r, s, time);
}

/* ---- Street ---------------------------------------------------------- */

static void draw_chuck(SDL_Renderer *r, float x, float feet_y, float scale,
                       float time)
{
    /*
     * In profile, facing the tower with his head tipped back at the lit
     * window.  Two things had to be true before he read as a man at all: he
     * stands in the lamp's pool, because a dark figure on the darkest plane
     * in the shot is nothing, and he keeps the colours the game gives him,
     * dimmed to night, rather than being flattened to black — a silhouette
     * needs a bright ground behind it, and this street has none.  Rim light
     * is then an accent on the edges that face a lamp, not a strip down his
     * whole body.
     */
    const SDL_Color coat = {32, 62, 86, 255};      /* front, toward the lobby */
    const SDL_Color coat_dk = {17, 32, 48, 255};   /* back, toward the lamp   */
    const SDL_Color trouser = {24, 30, 42, 255};
    const SDL_Color boot = {10, 12, 18, 255};
    const SDL_Color skin = {154, 108, 74, 255};
    const SDL_Color cap = {104, 40, 34, 255};
    const SDL_Color cap_dk = {58, 24, 22, 255};
    const SDL_Color cold = {112, 166, 180, 255};
    const SDL_Color warm = {214, 152, 82, 255};

    float breath = sinf(time * 1.5f) * 0.35f;
#define U(v) ((v) * scale)
    float top = feet_y - U(34.0f);

    /* Cast shadow, thrown forward and away from the lamp behind him. */
    fx_rect_a(r, COL_INK, 120, x + U(6.0f), feet_y - U(2.0f), U(32.0f), U(2.5f));
    fx_rect_a(r, COL_INK, 80, x + U(18.0f), feet_y - U(2.0f), U(26.0f), U(2.0f));

    /* Legs: back leg planted, near leg forward, boots pointing at the door. */
    color_rect(r, fx_dim(trouser, 0.6f), x + U(6.0f), top + U(24.0f), U(4.5f), U(8.5f));
    color_rect(r, boot, x + U(4.5f), top + U(31.5f), U(7.5f), U(2.5f));
    color_rect(r, trouser, x + U(11.0f), top + U(24.0f), U(4.5f), U(8.5f));
    color_rect(r, boot, x + U(11.0f), top + U(31.5f), U(9.0f), U(2.5f));

    /* Coat: back panel, lit front panel, and a hem over the trousers. */
    float body = top + U(10.0f) + U(breath);
    color_rect(r, coat_dk, x + U(5.0f), body, U(7.0f), U(15.0f));
    color_rect(r, coat, x + U(12.0f), body, U(5.5f), U(15.0f));
    color_rect(r, coat_dk, x + U(4.0f), body + U(11.0f), U(8.0f), U(4.0f));
    color_rect(r, fx_dim(coat, 0.8f), x + U(12.0f), body + U(11.0f), U(6.0f), U(4.0f));
    color_rect(r, fx_mix(coat, COL_CREAM, 0.12f), x + U(9.0f), body,
               U(8.5f), U(2.0f)); /* shoulder catching the lamp */

    /* Near arm hanging at his side, hand loose. */
    color_rect(r, fx_dim(coat, 0.62f), x + U(12.5f), body + U(3.0f), U(3.5f), U(8.5f));
    color_rect(r, fx_dim(skin, 0.72f), x + U(12.5f), body + U(11.0f), U(3.0f), U(2.5f));

    /* Head in profile, tipped back: neck, skull, face, nose, cap, brim. */
    float head = top + U(1.5f) + U(breath);
    color_rect(r, fx_dim(skin, 0.5f), x + U(9.5f), head + U(7.5f), U(4.0f), U(2.5f));
    color_rect(r, (SDL_Color){38, 24, 18, 255}, x + U(7.0f), head + U(2.5f),
               U(4.0f), U(6.0f));
    color_rect(r, skin, x + U(11.0f), head + U(2.5f), U(5.0f), U(6.0f));
    color_rect(r, skin, x + U(16.0f), head + U(4.5f), U(1.5f), U(2.0f)); /* nose */
    color_rect(r, fx_dim(skin, 0.55f), x + U(11.0f), head + U(7.0f), U(5.0f), U(1.5f));
    color_rect(r, (SDL_Color){28, 18, 14, 255}, x + U(13.5f), head + U(4.0f),
               U(1.5f), U(1.0f)); /* eye, aimed up the wall */
    color_rect(r, cap, x + U(6.5f), head, U(10.0f), U(3.0f));
    color_rect(r, cap_dk, x + U(6.5f), head + U(2.0f), U(10.0f), U(1.5f));
    color_rect(r, cap_dk, x + U(16.0f), head - U(0.5f), U(4.5f), U(2.0f));

    /* Rim accents only: cap, shoulder blade, calf, and the coat's front edge
     * where the lobby light catches it. */
    color_rect(r, fx_mix(cap, COL_CREAM, 0.45f), x + U(6.5f), head, U(10.0f), U(1.0f));
    color_rect(r, cold, x + U(4.5f), body + U(1.0f), U(1.0f), U(9.0f));
    color_rect(r, fx_dim(cold, 0.7f), x + U(6.0f), head + U(2.5f), U(1.0f), U(5.0f));
    color_rect(r, fx_dim(cold, 0.55f), x + U(6.0f), top + U(24.0f), U(1.0f), U(7.5f));
    color_rect(r, fx_dim(warm, 0.85f), x + U(17.0f), body + U(1.0f), U(1.0f), U(5.0f));
    color_rect(r, fx_dim(warm, 0.6f), x + U(17.0f), body + U(8.0f), U(1.0f), U(3.0f));
    color_rect(r, fx_dim(warm, 0.8f), x + U(19.0f), head - U(0.5f), U(1.5f), U(1.0f));
#undef U
}

static void draw_street_lamp(SDL_Renderer *r, const IntroScene *s, float x)
{
    float base = s->street_y + 46.0f;
    float head = 318.0f;

    /* The cone reaches wide enough at the bottom to put the man standing in
     * front of it inside the light rather than beside it. */
    fx_light_cone(r, x + 2.0f, head + 12.0f, 9.0f, 104.0f, base - head - 4.0f,
                  COL_LAMP, 30);
    fx_glow(r, x + 24.0f, base - 8.0f, 108.0f, COL_LAMP, 30);

    color_rect(r, (SDL_Color){12, 16, 24, 255}, x, head + 10.0f, 5.0f,
               base - head - 10.0f);
    color_rect(r, (SDL_Color){24, 31, 42, 255}, x + 1.0f, head + 10.0f, 1.0f,
               base - head - 10.0f);
    color_rect(r, (SDL_Color){12, 16, 24, 255}, x - 1.0f, head, 18.0f, 4.0f);
    color_rect(r, (SDL_Color){12, 16, 24, 255}, x + 13.0f, head + 4.0f, 4.0f, 3.0f);
    color_rect(r, fx_mix(COL_LAMP, COL_CREAM, 0.5f), x + 2.0f, head + 7.0f,
               13.0f, 4.0f);
    fx_glow(r, x + 8.0f, head + 9.0f, 26.0f, COL_LAMP, 88);
}

static void draw_suv(SDL_Renderer *r, float x, float base_y, float time)
{
    /* The car they arrived in, still at the kerb with the brake lights
     * cooling off — the shot picks up exactly where the drive ends.  It is a
     * dark shape lit from the lobby side, so the top surfaces and the near
     * flank carry a warm edge and the rest stays in the road. */
    const SDL_Color body = {16, 20, 28, 255};
    const SDL_Color trim = {40, 47, 58, 255};
    const SDL_Color glass = {22, 30, 42, 255};
    float brake = 1.0f - smoothstep01((time - 2.2f) / 2.6f);

    fx_rect_a(r, COL_INK, 170, x - 3.0f, base_y - 2.0f, 94.0f, 4.0f);
    color_rect(r, body, x, base_y - 24.0f, 88.0f, 22.0f);
    color_rect(r, body, x + 16.0f, base_y - 37.0f, 52.0f, 14.0f);
    color_rect(r, trim, x + 16.0f, base_y - 37.0f, 52.0f, 1.0f);
    color_rect(r, glass, x + 20.0f, base_y - 34.0f, 20.0f, 9.0f);
    color_rect(r, fx_dim(glass, 0.8f), x + 44.0f, base_y - 34.0f, 20.0f, 9.0f);
    color_rect(r, (SDL_Color){52, 44, 36, 255}, x + 20.0f, base_y - 34.0f,
               20.0f, 2.0f);
    color_rect(r, trim, x, base_y - 24.0f, 88.0f, 1.0f);
    color_rect(r, (SDL_Color){64, 54, 40, 255}, x + 2.0f, base_y - 15.0f,
               84.0f, 1.0f); /* beltline catching the entrance light */
    color_rect(r, (SDL_Color){9, 11, 16, 255}, x, base_y - 8.0f, 88.0f, 6.0f);

    color_rect(r, COL_INK, x + 10.0f, base_y - 9.0f, 17.0f, 9.0f);
    color_rect(r, COL_INK, x + 59.0f, base_y - 9.0f, 17.0f, 9.0f);
    color_rect(r, (SDL_Color){34, 40, 50, 255}, x + 13.0f, base_y - 6.0f, 10.0f, 4.0f);
    color_rect(r, (SDL_Color){34, 40, 50, 255}, x + 62.0f, base_y - 6.0f, 10.0f, 4.0f);

    if (brake > 0.01f)
    {
        SDL_Color tail = fx_dim((SDL_Color){236, 62, 48, 255},
                                0.35f + brake * 0.65f);
        fx_glow(r, x + 2.0f, base_y - 18.0f, 26.0f, tail,
                (Uint8)(70.0f * brake + 24.0f));
        color_rect(r, tail, x, base_y - 21.0f, 4.0f, 5.0f);
        color_rect(r, fx_dim(tail, 0.7f), x, base_y - 13.0f, 3.0f, 3.0f);
    }
}

static void render_street(SDL_Renderer *r, const Intro *intro,
                          const IntroScene *s)
{
    float time = intro->time;
    float wake = smoothstep01((time - 0.35f) / 0.8f);
    float y = s->street_y;

    /*
     * The ground is the darkest plane in the shot.  It has to be: everything
     * it shows — the pool at the entrance, the smears under the lit windows,
     * the lamp — is reflected light, and reflected light on a pale road is
     * invisible.
     */
    color_rect(r, (SDL_Color){14, 18, 26, 255}, 0.0f, y, s->w, s->h - y);
    color_rect(r, (SDL_Color){40, 49, 60, 255}, 0.0f, y, s->w, 1.0f);
    color_rect(r, (SDL_Color){8, 11, 17, 255}, 0.0f, y + 7.0f, s->w, s->h - y);
    fx_vgrad(r, 0.0f, y + 7.0f, s->w, 26.0f,
             (SDL_Color){30, 40, 54, 255}, 40,
             (SDL_Color){30, 40, 54, 255}, 0);
    fx_vgrad(r, 0.0f, s->h - 44.0f, s->w, 44.0f,
             (SDL_Color){4, 6, 10, 255}, 0,
             (SDL_Color){4, 6, 10, 255}, 150);

    /* Kerb joints, and two damp patches placed clear of the figure so no seam
     * cuts him off at the knee. */
    for (float jx = 24.0f; jx < s->w; jx += 96.0f)
        color_rect(r, (SDL_Color){17, 22, 30, 255}, jx, y + 1.0f, 1.0f, 6.0f);
    fx_rect_a(r, (SDL_Color){40, 52, 66, 255}, 20, 14.0f, y + 30.0f, 116.0f, 7.0f);
    fx_rect_a(r, (SDL_Color){40, 52, 66, 255}, 16, 622.0f, y + 14.0f, 150.0f, 6.0f);

    /* Wet asphalt: every lit pane smears straight down the frame.  Asking
     * pane_light() again is what keeps the reflection honest. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < TOWER_FLOORS; ++i)
    {
        for (int pane = 0; pane < TOWER_PANES; ++pane)
        {
            SDL_Color light;
            bool target = i == TARGET_FLOOR && pane == TARGET_PANE;
            if (!target && !pane_light(i, pane, time, &light))
                continue;
            if (target)
                light = (SDL_Color){250, 206, 130, 255};

            SDL_FRect p = pane_rect(s, i, pane);
            float cx = p.x + p.w * 0.5f;
            float sy = y + 4.0f;
            for (int seg = 0; seg < 6 && sy < s->h - 30.0f; ++seg)
            {
                unsigned h = fx_hash((unsigned)(i * 71 + pane * 9 + seg * 3));
                float jitter = (float)(h % 9u) - 4.0f;
                float width = p.w * (0.62f - (float)seg * 0.07f);
                Uint8 alpha = (Uint8)(44 - seg * 6 + (target ? 22 : 0));
                set_rgba(r, light.r, light.g, light.b, alpha);
                fill_rect(r, cx - width * 0.5f + jitter, sy, width,
                          2.0f + (float)(h % 3u));
                sy += 5.0f + (float)((h >> 3) % 7u);
            }
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /*
     * A fully glazed lobby spills along its whole width, not just out of the
     * doors.  That band of light is what the parked car and the kerb are read
     * against — without it both are dark shapes on a dark ground.
     */
    float lobby_top = floor_top(s, TOWER_FLOORS);
    float lobby_left = tower_edge(s, lobby_top, false);
    float lobby_right = tower_edge(s, lobby_top, true);
    float door_x = lobby_left + 52.0f;
    fx_vgrad(r, lobby_left - 24.0f, y, lobby_right - lobby_left + 130.0f, 38.0f,
             COL_WARM, (Uint8)(46.0f * wake), COL_WARM, 0);
    fx_glow(r, door_x, y + 12.0f, 132.0f, COL_WARM, (Uint8)(72.0f * wake));
    fx_vgrad(r, door_x - 40.0f, y, 80.0f, 52.0f,
             COL_WARM, (Uint8)(76.0f * wake), COL_WARM, 0);
    fx_vgrad(r, door_x - 18.0f, y, 36.0f, 34.0f,
             (SDL_Color){255, 226, 168, 255}, (Uint8)(58.0f * wake),
             COL_WARM, 0);

    /* Parked past the right edge of the start plate, so no part of it ends up
     * showing faintly through the translucent plate. */
    draw_suv(r, lobby_right + 30.0f, y + 20.0f, time);

    /* Sidewalk furniture on the near side, in near silhouette. */
    color_rect(r, (SDL_Color){12, 16, 23, 255}, 262.0f, y + 26.0f, 7.0f, 16.0f);
    color_rect(r, (SDL_Color){12, 16, 23, 255}, 259.0f, y + 30.0f, 13.0f, 4.0f);
    color_rect(r, (SDL_Color){12, 16, 23, 255}, 263.0f, y + 22.0f, 5.0f, 4.0f);
    color_rect(r, (SDL_Color){38, 30, 26, 255}, 262.0f, y + 26.0f, 1.0f, 16.0f);

    draw_street_lamp(r, s, 112.0f);
    draw_chuck(r, 162.0f, y + 52.0f, 2.1f, time);

    /* Ground fog: a thin seam along the kerb line only, drifting both ways.
     * Any deeper and it turns the darkest plane in the shot into a grey wash
     * and takes the wet look with it. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 5; ++i)
    {
        float speed = 6.0f + (float)(i % 3) * 4.0f;
        float dir = (i & 1) ? -1.0f : 1.0f;
        float span = s->w + 320.0f;
        float x = fmodf((float)i * 197.0f + time * speed * dir + span * 4.0f,
                        span) -
                  200.0f;
        set_rgba(r, 58, 78, 96, (Uint8)(9 + (i % 3) * 2));
        fill_rect(r, x, y - 5.0f + (float)i * 5.0f,
                  200.0f + (float)(i % 4) * 60.0f, 6.0f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/* ---- Wordmark -------------------------------------------------------- */

/*
 * The title is signage, not text.
 *
 * A wordmark filled with a vertical rainbow is the one thing in this shot that
 * could not be in it: every other surface in the frame is lit from somewhere,
 * and the somewhere is the moon at the top left.  So the five letters are
 * plates cut from the same slate the game builds its walls out of, bolted up
 * over the city and lit by that moon — drawn in the three passes every other
 * solid in Chuck is drawn in (material, then form, then edges), weathered by
 * the same rust that stains the building, and legible because the brightest
 * edge in the frame runs along their top faces rather than because the fill
 * shouts.
 *
 * The letterforms are condensed, because a plate half again as tall as it is
 * wide reads as a title where a square grid of cells reads as an arcade
 * cabinet whatever is done to its colour.  They are held as convex polygons
 * rather than as rows of characters for two reasons: the K keeps one straight
 * diagonal of even weight instead of a staircase, and every outer corner can
 * carry the same cut.  Then they are rasterised at one screen pixel — the
 * resolution the rest of the frame is drawn at, and the other half of why the
 * old cell grid read as a different, coarser picture pasted over this one.
 */
#define MARK_W 48       /* one plate, in pixels                            */
#define MARK_H 76
#define MARK_STROKE 12  /* stroke weight: bold enough to hold a lit edge    */
#define MARK_GAP 11     /* tight, so five plates read as one sign — but not
                         * so tight that one plate's edge fouls the next    */
#define MARK_CUT 7      /* the chamfer taken off every outer corner         */
#define MARK_TERM 10    /* how far a terminal is cut back on the diagonal   */
#define MARK_DEPTH 3    /* how far a plate stands off what is behind it     */

/* Where a plate is fixed, and it is the same two places on all five of them:
 * a fixing pattern that wanders from letter to letter reads as dirt on the
 * screen rather than as ironmongery holding a sign up. */
#define MARK_BOLT_TOP 11
#define MARK_BOLT_BOTTOM 57

/* One pixel of margin all round: the outline and the edge classifier both ask
 * about neighbours, and clamping at the border would report a letter's own
 * edge as solid and swallow it. */
#define MARK_BUF_W (MARK_W + 2)
#define MARK_BUF_H (MARK_H + 2)

typedef struct
{
    float x[6], y[6];
    int n;
} MarkPoly;

typedef struct
{
    MarkPoly part[3];
    int parts;
} MarkPlate;

static MarkPoly mark_poly(const float *xy, int n)
{
    MarkPoly poly;
    SDL_zero(poly);
    poly.n = n;
    for (int i = 0; i < n; ++i)
    {
        poly.x[i] = xy[i * 2];
        poly.y[i] = xy[i * 2 + 1];
    }
    return poly;
}

/*
 * Every letter is at most three convex pieces in a 48x76 box, and the pieces
 * are allowed to overlap — a union is what keeps a joint from opening up at
 * the chamfers.
 */
static MarkPlate mark_plate(char letter)
{
    const float w = (float)MARK_W;
    const float h = (float)MARK_H;
    const float s = (float)MARK_STROKE;
    const float c = (float)MARK_CUT;
    const float t = (float)MARK_TERM;

    MarkPlate p;
    SDL_zero(p);
    p.parts = 3;

    switch (letter)
    {
    case 'C':
    {
        /* Both terminals cut back on the diagonal.  A squared C at this weight
         * with blunt terminals closes its own aperture up; the cut opens the
         * mouth without thinning the stroke. */
        const float top[] = {c, 0.0f, w, 0.0f, w - t, s, 0.0f, s, 0.0f, c};
        const float bottom[] = {0.0f, h - c, 0.0f, h - s, w - t, h - s,
                                w, h, c, h};
        const float stem[] = {0.0f, c, s, c, s, h - c, 0.0f, h - c};
        p.part[0] = mark_poly(top, 5);
        p.part[1] = mark_poly(bottom, 5);
        p.part[2] = mark_poly(stem, 4);
        break;
    }
    case 'H':
    {
        const float left[] = {c, 0.0f, s, 0.0f, s, h, c, h, 0.0f, h - c,
                              0.0f, c};
        const float right[] = {w - s, 0.0f, w - c, 0.0f, w, c, w, h - c,
                               w - c, h, w - s, h};
        /* Bar centre a shade above the middle: set at the true half it reads
         * as having dropped, because the eye weights the lower counter more. */
        const float bar_y = h * 0.455f - s * 0.5f;
        const float bar[] = {s, bar_y, w - s, bar_y, w - s, bar_y + s,
                             s, bar_y + s};
        p.part[0] = mark_poly(left, 6);
        p.part[1] = mark_poly(right, 6);
        p.part[2] = mark_poly(bar, 4);
        break;
    }
    case 'U':
    {
        const float left[] = {c, 0.0f, s, 0.0f, s, h - s, 0.0f, h - s,
                              0.0f, c};
        const float right[] = {w - s, 0.0f, w - c, 0.0f, w, c, w, h - s,
                               w - s, h - s};
        const float bottom[] = {0.0f, h - s, w, h - s, w - c, h, c, h};
        p.part[0] = mark_poly(left, 5);
        p.part[1] = mark_poly(right, 5);
        p.part[2] = mark_poly(bottom, 4);
        break;
    }
    default:
    {
        /*
         * The arm and the leg are parallelograms, and that is the whole reason
         * the letters are polygons: both long edges of each share one slope,
         * so the stroke holds its weight from the stem out to the tip.  The
         * horizontal span a bar needs at its ends to measure `s` across is
         * s * sqrt(1 + m^2) / m — get that wrong and the K goes thin at the
         * corners while the other four letters stay heavy.
         */
        const float stem[] = {c, 0.0f, s, 0.0f, s, h, c, h, 0.0f, h - c,
                              0.0f, c};
        const float arm_y = h * 0.45f;
        const float arm_m = arm_y / (w - s);
        const float arm_dx = s * sqrtf(1.0f + arm_m * arm_m) / arm_m;
        const float arm[] = {w - arm_dx, 0.0f, w - c, 0.0f,
                             w - c * 0.7f, arm_m * c * 0.7f,
                             s, arm_y, s, arm_m * (w - arm_dx - s)};
        const float leg_y = h * 0.42f;
        const float leg_m = (h - leg_y) / (w - s);
        const float leg_dx = s * sqrtf(1.0f + leg_m * leg_m) / leg_m;
        const float leg[] = {s, leg_y, s, h - leg_m * (w - leg_dx - s),
                             w - leg_dx, h, w - c, h,
                             w - c * 0.7f, h - leg_m * c * 0.7f};
        p.part[0] = mark_poly(stem, 6);
        p.part[1] = mark_poly(arm, 5);
        p.part[2] = mark_poly(leg, 5);
        break;
    }
    }
    return p;
}

/* Convex, either winding: the sign of the cross product has to agree for every
 * edge, and which sign it is depends on how the piece happens to be wound. */
static bool mark_in_poly(const MarkPoly *poly, float x, float y)
{
    int above = 0, below = 0;

    for (int i = 0; i < poly->n; ++i)
    {
        int j = (i + 1) % poly->n;
        float ex = poly->x[j] - poly->x[i];
        float ey = poly->y[j] - poly->y[i];
        float cross = ex * (y - poly->y[i]) - ey * (x - poly->x[i]);
        if (cross > 0.0f)
            ++above;
        else if (cross < 0.0f)
            ++below;
    }
    return above == 0 || below == 0;
}

static void mark_rasterise(char letter, Uint8 *mask)
{
    MarkPlate plate = mark_plate(letter);

    SDL_memset(mask, 0, (size_t)(MARK_BUF_W * MARK_BUF_H));
    for (int y = 0; y < MARK_H; ++y)
    {
        for (int x = 0; x < MARK_W; ++x)
        {
            for (int i = 0; i < plate.parts; ++i)
            {
                if (mark_in_poly(&plate.part[i], (float)x + 0.5f,
                                 (float)y + 0.5f))
                {
                    mask[(y + 1) * MARK_BUF_W + (x + 1)] = 1;
                    break;
                }
            }
        }
    }
}

static bool mark_solid(const Uint8 *mask, int x, int y)
{
    if (x < -1 || x > MARK_W || y < -1 || y > MARK_H)
        return false;
    return mask[(y + 1) * MARK_BUF_W + (x + 1)] != 0;
}

static bool mark_grown(const Uint8 *mask, int x, int y)
{
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if (mark_solid(mask, x + dx, y + dy))
                return true;
    return false;
}

/* One flat pass over the letter, emitted as horizontal runs rather than as one
 * rect per pixel — a plate is three or four spans a row, not four hundred. */
static void mark_fill(SDL_Renderer *r, const Uint8 *mask, float ox, float oy,
                      SDL_Color color, bool grown)
{
    set_color(r, color);
    for (int y = -1; y <= MARK_H; ++y)
    {
        int run = -1;
        for (int x = -1; x <= MARK_W + 1; ++x)
        {
            bool on = x <= MARK_W && (grown ? mark_grown(mask, x, y)
                                            : mark_solid(mask, x, y));
            if (on && run < 0)
                run = x;
            else if (!on && run >= 0)
            {
                fill_rect(r, ox + (float)run, oy + (float)y,
                          (float)(x - run), 1.0f);
                run = -1;
            }
        }
    }
}

/*
 * Which face of the plate a pixel belongs to.  Light comes from the upper left
 * — the moon is drawn there and the tower is shaded for it — so the top and
 * the left flank catch it and the underside and the right flank turn away.
 */
enum
{
    MARK_BODY,
    MARK_CROWN,  /* the top face proper */
    MARK_ARRIS,  /* the row under it, where the chamfer rolls over */
    MARK_BASE,
    MARK_RIM_L,
    MARK_RIM_R
};

/*
 * Broad patches of light and shade across the plate, on a lattice four cells to
 * the stroke — the same drift the game lays over its wall tiles, and there for
 * the same reason.  A face in one flat value is a swatch, and a row of bevelled
 * swatches is what a die-cut sticker looks like however well the bevel is done.
 * Smooth, so it never competes with the edge it sits under, and salted per
 * plate so no two letters weather the same way.
 */
static float mark_drift(unsigned salt, int x, int y)
{
    const float cell = 15.0f;
    float fx = (float)x / cell;
    float fy = (float)y / cell;
    int x0 = (int)floorf(fx);
    int y0 = (int)floorf(fy);
    float tx = smoothstep01(fx - (float)x0);
    float ty = smoothstep01(fy - (float)y0);
    float value = 0.0f;

    for (int j = 0; j <= 1; ++j)
    {
        for (int i = 0; i <= 1; ++i)
        {
            unsigned hash = fx_hash((unsigned)(x0 + i) * 73856093u ^
                                    (unsigned)(y0 + j) * 19349663u ^ salt);
            value += (float)(hash % 1024u) * (1.0f / 1024.0f) *
                     (i ? tx : 1.0f - tx) * (j ? ty : 1.0f - ty);
        }
    }
    return value * 2.0f - 1.0f;
}

static int mark_facing(const Uint8 *mask, int x, int y)
{
    if (!mark_solid(mask, x, y - 1))
        return MARK_CROWN;
    if (!mark_solid(mask, x, y - 2))
        return MARK_ARRIS;
    if (!mark_solid(mask, x, y + 1))
        return MARK_BASE;
    if (!mark_solid(mask, x - 1, y))
        return MARK_RIM_L;
    if (!mark_solid(mask, x + 1, y))
        return MARK_RIM_R;
    return MARK_BODY;
}

static SDL_Color mark_face_color(int facing, SDL_Color body, float glint)
{
    /*
     * How much of a passing beam each face takes.  The crown is nearly cream
     * already, so putting the beam mostly there makes it a whiter white that
     * nobody sees; it is the body and the flanks — the faces a light coming
     * across the sign actually turns toward — that carry the sweep, and the
     * underside barely at all.
     */
    static const float take[6] = {0.62f, 0.26f, 0.52f, 0.16f, 0.70f, 0.34f};
    SDL_Color color = body;

    switch (facing)
    {
    case MARK_CROWN:
        color = fx_mix(body, FX_CREAM, 0.78f);
        break;
    case MARK_ARRIS:
        color = fx_mix(body, FX_CREAM, 0.30f);
        break;
    case MARK_BASE:
        /* Cooled by turning away from the moon, then warmed again by the
         * street: the sign is hung over a lit road, and a night exterior with
         * nothing but cold in its shadows is a night exterior in a vacuum.
         * The same bounce the game lays off its own floors, aimed upward. */
        color = fx_mix(fx_mix(body, COL_INK, 0.60f), COL_SODIUM, 0.34f);
        break;
    case MARK_RIM_L:
        color = fx_mix(body, FX_CREAM, 0.26f);
        break;
    case MARK_RIM_R:
        color = fx_mix(fx_mix(body, COL_INK, 0.40f), COL_SODIUM, 0.16f);
        break;
    default:
        break;
    }
    if (glint > 0.0f)
        color = fx_mix(color, (SDL_Color){255, 246, 226, 255},
                       glint * take[facing]);
    return color;
}

/* A rivet, four pixels of it: the same fixing the game's walls carry on their
 * stiffeners every fourth course, so the sign is made of the building's steel
 * rather than of nothing in particular.  Lit on the same corner as everything
 * else on the plate, or it reads as a hole instead of as a head. */
static void draw_mark_bolt(SDL_Renderer *r, float x, float y, float level)
{
    color_rect(r, fx_dim(COL_INK, level), x, y, 4.0f, 4.0f);
    color_rect(r, fx_dim(FX_STEEL, level), x, y, 3.0f, 3.0f);
    color_rect(r, fx_dim(FX_PALE, level), x, y, 2.0f, 1.0f);
    color_rect(r, fx_dim(fx_mix(FX_STEEL_DK, COL_INK, 0.5f), level),
               x + 2.0f, y + 2.0f, 1.0f, 1.0f);
}

/*
 * Rust bleeds out of a fixing and runs down the plate under it, and it stops
 * where the plate stops — which is why the streak walks the mask instead of
 * being drawn as a rectangle: on the C it runs down the stem and dies at the
 * chamfer, on the K it dies at the leg.
 */
static void draw_mark_stain(SDL_Renderer *r, const Uint8 *mask, float ox,
                            float oy, int bx, int by, float level)
{
    static const SDL_Color rust = {146, 68, 40, 255};
    const int reach = 34;

    for (int col = 0; col < 4; ++col)
    {
        int x = bx + col - 1;
        for (int step = 0; step < reach; step += 2)
        {
            int y = by + 4 + step;
            if (!mark_solid(mask, x, y) || !mark_solid(mask, x, y + 1))
                break;
            float fade = 1.0f - (float)step / (float)reach;
            Uint8 alpha = (Uint8)(96.0f * fade * fade * level *
                                  (col == 0 || col == 3 ? 0.45f : 1.0f));
            if (alpha == 0)
                break;
            fx_rect_a(r, rust, alpha, ox + (float)x, oy + (float)y, 1.0f, 2.0f);
        }
    }
}

static void draw_mark_letter(SDL_Renderer *r, char letter, unsigned salt,
                             float ox, float oy, float level, float glint_x)
{
    Uint8 mask[MARK_BUF_W * MARK_BUF_H];
    const float depth = (float)MARK_DEPTH;

    mark_rasterise(letter, mask);

    /* The plate stands off the sky: its own dark first, then the thickness of
     * its edge, then the outline of the face over that.  The offset goes down
     * and to the right because the light is up and to the left. */
    mark_fill(r, mask, ox + depth, oy + depth, COL_INK, true);
    mark_fill(r, mask, ox + depth, oy + depth,
              fx_dim(fx_mix(FX_STEEL_DK, COL_INK, 0.45f), level), false);
    mark_fill(r, mask, ox, oy, COL_INK, true);

    for (int y = 0; y < MARK_H; ++y)
    {
        /* Steel, lighter at the top of the plate than at its foot — one ramp
         * across the whole letter, so the five plates read as cut from one
         * sheet — with the weather in the bottom of it, where water sits. */
        float t = (float)y / (float)(MARK_H - 1);
        SDL_Color body = fx_mix(fx_mix(FX_STEEL_LT, FX_STEEL_DK, t),
                                COL_SODIUM, t * t * 0.16f);
        int run = -1;
        int run_key = -1;
        SDL_Color run_color = COL_INK;

        for (int x = 0; x <= MARK_W; ++x)
        {
            int key = -1;
            SDL_Color color = COL_INK;

            if (x < MARK_W && mark_solid(mask, x, y))
            {
                int facing = mark_facing(mask, x, y);
                int glint = 0;

                /* A beam off the street crossing the sign.  The drift and the
                 * beam are both quantised, because a run of pixels is only one
                 * rect while its colour is one colour. */
                float reach = 1.0f - fabsf((float)x + ox - glint_x) / 34.0f;
                if (reach > 0.0f)
                    glint = (int)(reach * reach * 8.0f + 0.5f);

                int shade = (int)(mark_drift(salt, x, y) * 2.5f + 3.0f);
                float drift = (float)shade / 2.5f - 1.2f;
                SDL_Color plate = drift > 0.0f
                                      ? fx_mix(body, FX_CREAM, drift * 0.11f)
                                      : fx_mix(body, COL_INK, -drift * 0.15f);

                key = (facing * 16 + glint) * 8 + shade;
                color = fx_dim(mark_face_color(facing, plate,
                                               (float)glint / 8.0f), level);
            }

            if (key != run_key)
            {
                if (run >= 0)
                    color_rect(r, run_color, ox + (float)run, oy + (float)y,
                               (float)(x - run), 1.0f);
                run = key >= 0 ? x : -1;
                run_key = key;
                run_color = color;
            }
        }
    }

    /* Both fixings go down the left stem, which every one of the five letters
     * has, at the same two heights — so the pattern reads across the word. */
    static const int bolt_y[2] = {MARK_BOLT_TOP, MARK_BOLT_BOTTOM};
    const int bolt_x = MARK_STROKE / 2 - 2;
    for (int i = 0; i < 2; ++i)
    {
        draw_mark_stain(r, mask, ox, oy, bolt_x, bolt_y[i], level);
        draw_mark_bolt(r, ox + (float)bolt_x, oy + (float)bolt_y[i], level);
    }
}

static void render_logo(SDL_Renderer *r, const Intro *intro,
                        const IntroScene *s)
{
    static const char *word = "CHUCK";
    const float advance = (float)(MARK_W + MARK_GAP);
    const float mark_w = advance * 4.0f + (float)MARK_W;
    const float x = (s->w - mark_w) * 0.5f;
    const float y = 30.0f;

    /* Haze rather than bloom.  A steel sign does not glow; what it needs is
     * the city's light lifting the sky just behind it, so the plates have
     * something to be dark against at the top of the frame. */
    fx_glow(r, s->w * 0.5f, y + 40.0f, 330.0f, (SDL_Color){52, 76, 96, 255}, 30);
    fx_glow(r, s->w * 0.5f, y + 62.0f, 170.0f, (SDL_Color){70, 92, 108, 255}, 22);

    float sweep = fmodf(intro->time, 9.0f);
    float glint_x = sweep < 1.8f
                        ? x - 40.0f + (sweep / 1.8f) * (mark_w + 80.0f)
                        : -4000.0f;

    for (int i = 0; i < 5; ++i)
    {
        /* Each plate drops the last few pixels onto its fixings. */
        float reveal = smoothstep01((intro->time - 0.05f - (float)i * 0.08f) /
                                    0.36f);
        if (reveal <= 0.0f)
            continue;
        draw_mark_letter(r, word[i], (unsigned)i * 977u + 41u,
                         x + (float)i * advance, y + (1.0f - reveal) * 8.0f,
                         0.34f + reveal * 0.66f, glint_x);
    }

    /* Tagline, set between two rules so it reads as part of the wordmark. */
    float tag = smoothstep01((intro->time - 0.72f) / 0.5f);
    if (tag <= 0.0f)
        return;
    const char *line = "THEY TOOK HER. BRING HER HOME.";
    const float track = 3.0f;
    float width = tracked_width(line, 1.0f, track);
    float ty = y + (float)MARK_H + 24.0f;
    draw_tracked_centered(r, s->w * 0.5f, ty, 1.0f, track,
                          fx_dim((SDL_Color){196, 202, 196, 255}, tag), line);

    float rule = 40.0f * tag;
    color_rect(r, fx_dim(COL_RUST, 0.55f + tag * 0.45f),
               s->w * 0.5f - width * 0.5f - 16.0f - rule, ty + 3.0f, rule, 2.0f);
    color_rect(r, fx_dim(COL_RUST, 0.55f + tag * 0.45f),
               s->w * 0.5f + width * 0.5f + 16.0f, ty + 3.0f, rule, 2.0f);
}

/* ---- Interface ------------------------------------------------------- */

static void draw_chevron(SDL_Renderer *r, float x, float y, SDL_Color color)
{
    color_rect(r, color, x, y, 2.0f, 8.0f);
    color_rect(r, color, x + 2.0f, y + 2.0f, 2.0f, 4.0f);
    color_rect(r, color, x + 4.0f, y + 3.0f, 2.0f, 2.0f);
}

static void render_start_prompt(SDL_Renderer *r, const Intro *intro,
                                const IntroScene *s, bool gamepad_active)
{
    const SDL_FRect *button = &intro->start_button;
    float appear = smoothstep01((intro->time - 1.15f) / 0.55f);
    if (appear <= 0.0f)
        return;

    float cx = s->w * 0.5f;
    float pulse = 0.5f + 0.5f * sinf(intro->time * 2.3f);
    bool hot = intro->start_hovered;

    if (hot)
        fx_glow(r, cx, button->y + button->h * 0.5f, 140.0f,
                (SDL_Color){226, 104, 78, 255}, 34);

    /* A translucent plate, so the prompt sits on the wet street rather than
     * covering it. */
    fx_rect_a(r, (SDL_Color){6, 9, 15, 255}, (Uint8)((hot ? 210.0f : 176.0f) * appear),
              button->x, button->y, button->w, button->h);
    fx_rect_a(r, (SDL_Color){86, 104, 124, 255}, (Uint8)(180.0f * appear),
              button->x, button->y, button->w, 1.0f);
    fx_rect_a(r, (SDL_Color){4, 6, 10, 255}, (Uint8)(190.0f * appear),
              button->x, button->y + button->h - 1.0f, button->w, 1.0f);

    /* Rust corner brackets: the same marker the cutscenes frame things with. */
    SDL_Color bracket = fx_dim(COL_RUST, (0.62f + pulse * 0.38f) * appear);
    const float arm = 10.0f;
    set_color(r, bracket);
    fill_rect(r, button->x - 3.0f, button->y - 3.0f, arm, 2.0f);
    fill_rect(r, button->x - 3.0f, button->y - 3.0f, 2.0f, arm);
    fill_rect(r, button->x + button->w - arm + 3.0f, button->y - 3.0f, arm, 2.0f);
    fill_rect(r, button->x + button->w + 1.0f, button->y - 3.0f, 2.0f, arm);
    fill_rect(r, button->x - 3.0f, button->y + button->h + 1.0f, arm, 2.0f);
    fill_rect(r, button->x - 3.0f, button->y + button->h - arm + 3.0f, 2.0f, arm);
    fill_rect(r, button->x + button->w - arm + 3.0f, button->y + button->h + 1.0f,
              arm, 2.0f);
    fill_rect(r, button->x + button->w + 1.0f, button->y + button->h - arm + 3.0f,
              2.0f, arm);

    /* Chevron and label are one group, centred as a group — so whichever
     * label is showing, the padding is equal on both sides. */
    const char *label = gamepad_active ? "PRESS A TO START" : START_LABEL;
    float width = tracked_width(label, 1.0f, START_TRACK);
    float group = START_CHEVRON_W + START_CHEVRON_GAP + width;
    float group_x = cx - group * 0.5f;
    SDL_Color text = fx_dim(hot ? COL_CREAM : (SDL_Color){206, 212, 202, 255},
                            appear);
    draw_chevron(r, group_x, button->y + 13.0f,
                 fx_dim(hot ? COL_AMBER : COL_RUST,
                        (0.6f + pulse * 0.4f) * appear));
    draw_tracked(r, group_x + START_CHEVRON_W + START_CHEVRON_GAP,
                 button->y + 13.0f, 1.0f, START_TRACK, text, label);

    float accent = hot ? 74.0f : 26.0f + pulse * 22.0f;
    fx_rect_a(r, COL_RUST, (Uint8)(235.0f * appear), cx - accent * 0.5f,
              button->y + button->h - 2.0f, accent, 2.0f);
}

/*
 * The hints are set at scale 1.0 and no smaller.  The debug font is an 8x8
 * bitmap: any other scale resamples it, and a row of blurred type along the
 * bottom of the frame undoes the rest of the screen.  At 1.0 the whole row
 * still fits inside 800 logical pixels.
 */
#define CONTROL_CH ((float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)

static float control_hint_width(const char *key, const char *action)
{
    return CONTROL_CH * (float)SDL_strlen(key) + 12.0f + 7.0f +
           CONTROL_CH * (float)SDL_strlen(action) + 20.0f;
}

static float draw_control_hint(SDL_Renderer *r, float x, float y,
                               const char *key, const char *action,
                               float level)
{
    float key_w = CONTROL_CH * (float)SDL_strlen(key) + 12.0f;
    const float key_h = 18.0f;

    /* Keycap: dark bezel, lit top edge, sunken base. */
    fx_rect_a(r, COL_INK, (Uint8)(190.0f * level), x, y, key_w, key_h);
    fx_rect_a(r, (SDL_Color){32, 42, 56, 255}, (Uint8)(215.0f * level),
              x + 1.0f, y + 1.0f, key_w - 2.0f, key_h - 2.0f);
    fx_rect_a(r, (SDL_Color){68, 84, 102, 255}, (Uint8)(215.0f * level),
              x + 1.0f, y + 1.0f, key_w - 2.0f, 1.0f);
    fx_rect_a(r, (SDL_Color){10, 14, 22, 255}, (Uint8)(215.0f * level),
              x + 1.0f, y + key_h - 2.0f, key_w - 2.0f, 1.0f);
    draw_text(r, x + 6.0f, y + 5.0f, 1.0f,
              fx_dim((SDL_Color){186, 196, 190, 255}, level), key);
    draw_text(r, x + key_w + 7.0f, y + 5.0f, 1.0f, fx_dim(COL_MUTED, level),
              action);
    return x + key_w + 7.0f + CONTROL_CH * (float)SDL_strlen(action) + 20.0f;
}

static void render_controls(SDL_Renderer *r, const Intro *intro,
                            const IntroScene *s, bool gamepad_active)
{
    static const char *keyboard_keys[] = {
        "WASD", "W", "SPACE", "TAB", "S", "E"};
    static const char *keyboard_actions[] = {
        "MOVE", "JUMP", "ATTACK", "WEAPON", "CRAWL", "USE/HACK"};
    static const char *gamepad_keys[] = {"LS/DPAD", "A", "X", "RB", "Y"};
    static const char *gamepad_actions[] = {
        "MOVE", "JUMP", "ATTACK", "WEAPON", "USE/HACK"};
    const char *const *keys = gamepad_active ? gamepad_keys : keyboard_keys;
    const char *const *actions = gamepad_active ? gamepad_actions
                                                : keyboard_actions;
    int count = gamepad_active ? 5 : 6;

    float level = smoothstep01((intro->time - 1.45f) / 0.6f);
    if (level <= 0.0f)
        return;

    float total = 0.0f;
    for (int i = 0; i < count; ++i)
        total += control_hint_width(keys[i], actions[i]);
    total -= 20.0f;

    float x = (s->w - total) * 0.5f;
    float y = s->h - 30.0f;
    for (int i = 0; i < count; ++i)
        x = draw_control_hint(r, x, y, keys[i], actions[i], level);
}

void intro_render(SDL_Renderer *r, const Intro *intro, int win_w, int win_h,
                  bool gamepad_active)
{
    IntroScene scene = scene_layout(win_w, win_h);

    render_sky(r, intro, &scene);
    render_skyline(r, intro, &scene);
    render_flanking_blocks(r, &scene);
    render_tower(r, intro, &scene);
    render_street(r, intro, &scene);
    render_logo(r, intro, &scene);
    render_start_prompt(r, intro, &scene, gamepad_active);
    render_controls(r, intro, &scene, gamepad_active);

    /* The shot fades up from black, like the cutscenes it hands over to. */
    float fade = 1.0f - smoothstep01(intro->time / 0.65f);
    if (fade > 0.0f)
        fx_rect_a(r, COL_INK, (Uint8)(255.0f * fade), 0.0f, 0.0f,
                  scene.w, scene.h);

    /* Same finishing pass as gameplay so the menu belongs to the film. */
    fx_vignette(r, win_w, win_h, 72);
    fx_scanlines(r, win_w, win_h, 11);
}
