/*
 * Every film in the game, drawn the way a BBC Micro in MODE 2 drew a story
 * screen: the kerb where Ellen is taken, the pavement outside Kessler Tower
 * where she is walked in, the report between sectors, the roof, and the store's
 * key art. They share one cast (the sprites in [bbc_cast.c](bbc_cast.c), the
 * same people the player meets on the floor), one pair of vehicles, one street
 * and one rain, which is the only reason five pictures cost no asset pipeline
 * at all — and the reason a change to a figure here changes all of them.
 *
 * Every pixel goes through [bbc.h](bbc.h): eight colours, fat pixels four wide
 * and two tall, nothing blended. What the old films did with light this file
 * does with the three things the machine had instead (see
 * [docs/art-and-audio.md](../docs/art-and-audio.md)): a lamp is a flat yellow
 * shape and a dithered pool, a fade is the palette stepping down its ladder, a
 * flash is a white frame, an alarm is a colour that blinks.
 *
 * The order the player meets them is: title screen, then the kerb
 * (`abduction_*`), then the drive in [chase.c](chase.c), then the tower's
 * front door (`opening_*`), then the campaign.
 */
#include "cutscene.h"

#include <math.h>

#include "bbc.h"
#include "bbc_cast.h"
#include "intel.h"
#include "manual_pages.h"

/* The lift the report's captors take: the top of its frame and of its car. */
static const float TRANSITION_DOOR_TOP = 358.0f;
static const float TRANSITION_DOOR_INNER_TOP = 368.0f;

/* ---- Arithmetic ------------------------------------------------------- */

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

/* A hash spread over `range`, for placing things that must not move. */
static float spread(unsigned h, float range)
{
    return (float)(h % 10007u) / 10007.0f * range;
}

/* How fast something eased along a smoothstep is going `u` of the way through
   it, as a share of its top speed: nought at both ends and one in the middle. */
static float smoothstep_pace(float u)
{
    u = clamp01(u);
    return 4.0f * u * (1.0f - u);
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

/* ---- Fades ------------------------------------------------------------ */

/*
 * A fade is the palette walking down its ladder, three rungs from the picture
 * to black, because that is the only fade the machine had: VDU 19 four times,
 * never a blend. Each scene works out how far down the ladder it is at the top
 * of its frame and everything it draws lands one, two or three rungs dark.
 *
 * A line of type that "fades in" over a scene is the same thing done locally:
 * the palette is stepped down for the few calls that draw it and put back
 * after, so a caption arrives in three hard steps the way a BBC title did.
 * Nothing here leaves the palette changed when a render function returns,
 * because the shell draws its own lines over two of these screens.
 */
static int scene_dim_steps;

static int fade_steps(float fade)
{
    if (!(fade > 0.0f))
        return 0;
    if (fade >= 1.0f)
        return 3;
    int steps = 1 + (int)(fade * 3.0f);
    return steps > 3 ? 3 : steps;
}

static void scene_begin(int steps)
{
    scene_dim_steps = steps;
    bbc_palette_reset();
    bbc_palette_dim(steps);
}

/* The scene's own dim, or `steps`, whichever is darker. */
static void dim_local(int steps)
{
    bbc_palette_reset();
    bbc_palette_dim(steps > scene_dim_steps ? steps : scene_dim_steps);
}

static void dim_restore(void)
{
    dim_local(0);
}

static void scene_end(void)
{
    scene_dim_steps = 0;
    bbc_palette_reset();
}

/* ---- Type ------------------------------------------------------------- */

static void text(SDL_Renderer *r, float x, float y, int scale, BbcColour c,
                 const char *s)
{
    bbc_text(r, x, y, (float)scale, c, s);
}

static float text_w(const char *s, int scale)
{
    return bbc_text_width(s, (float)scale);
}

static void text_centered(SDL_Renderer *r, float cx, float y, int scale,
                          BbcColour c, const char *s)
{
    text(r, cx - text_w(s, scale) * 0.5f, y, scale, c, s);
}

/*
 * The film's captions come in the way a teletype prints them: a character at
 * a time behind a block cursor, which is how every BBC game of the period put
 * a line of plot on the screen and the one reveal type can have without a
 * fade. Forty-eight characters a second puts the longest caption here up in
 * under a second, about what the old fade-in took.
 */
#define CAPTION_RATE 48.0f

static void typed_text(SDL_Renderer *r, float x, float y, int scale,
                       BbcColour c, const char *s, float age)
{
    if (age < 0.0f || s == NULL)
        return;
    size_t len = SDL_strlen(s);
    size_t shown = (size_t)(age * CAPTION_RATE);
    if (shown >= len)
    {
        text(r, x, y, scale, c, s);
        return;
    }
    char buffer[96];
    if (shown >= sizeof(buffer))
        shown = sizeof(buffer) - 1;
    SDL_memcpy(buffer, s, shown);
    buffer[shown] = '\0';
    text(r, x, y, scale, c, buffer);
    float cell = (float)(BBC_FONT_CELL * scale);
    bbc_fill(r, c, x + cell * (float)shown, y, cell - 2.0f, cell - 2.0f);
}

/* The same, on a black bar the width of what has been typed so far: text
 * printed in a background colour, the way the machine printed every line it
 * put over a picture. For a caption laid over scenery rather than sky. */
static void typed_text_backed(SDL_Renderer *r, float x, float y, BbcColour c,
                              const char *s, float age)
{
    if (age < 0.0f || s == NULL)
        return;
    size_t len = SDL_strlen(s);
    size_t shown = (size_t)(age * CAPTION_RATE) + 1;
    if (shown > len)
        shown = len;
    bbc_fill(r, BBC_BLACK, x - 4.0f, y - 4.0f,
             (float)(shown * BBC_FONT_CELL) + 8.0f, 16.0f);
    typed_text(r, x, y, 1, c, s, age);
}

/* A caption with the short rule under it that every readout in the film
 * carries, drawn out from the left while the line types. */
static void draw_caption(SDL_Renderer *r, float x, float y, BbcColour c,
                         const char *s, float age, float rule_w,
                         BbcColour rule)
{
    if (age < 0.0f)
        return;
    typed_text(r, x, y, 1, c, s, age);
    float grown = rule_w * smoothstep01(age / 0.40f);
    if (grown >= (float)BBC_PX_W)
        bbc_fill(r, rule, x, y + 15.0f, grown, 2.0f);
}

/* The skip offer, right-aligned off the line it draws so a longer spelling of
   the buttons cannot run off the frame. */
static void draw_skip_hint(SDL_Renderer *r, int win_w, int win_h,
                           const PadHints *pad)
{
    char hint[32];
    const char *line = pad_hint(pad, hint, sizeof(hint),
                                PAD_CONFIRM_PAD " TO SKIP",
                                PAD_CONFIRM_KEYS " TO SKIP");
    text(r, (float)win_w - 12.0f - text_w(line, 1), (float)win_h - 31.0f, 1,
         BBC_CYAN, line);
}

/* Four corner marks round what the film is pointing at, in the colour that
 * blinks: red and its complement at the machine's own flash rate. */
static void draw_target_brackets(SDL_Renderer *r, float x, float y, float w,
                                 float h)
{
    BbcColour c = bbc_flash(BBC_RED);
    const float arm_w = 16.0f;
    const float arm_h = 12.0f;
    bbc_fill(r, c, x, y, arm_w, 2.0f);
    bbc_fill(r, c, x, y, 4.0f, arm_h);
    bbc_fill(r, c, x + w - arm_w, y, arm_w, 2.0f);
    bbc_fill(r, c, x + w - 4.0f, y, 4.0f, arm_h);
    bbc_fill(r, c, x, y + h - 2.0f, arm_w, 2.0f);
    bbc_fill(r, c, x, y + h - arm_h, 4.0f, arm_h);
    bbc_fill(r, c, x + w - arm_w, y + h - 2.0f, arm_w, 2.0f);
    bbc_fill(r, c, x + w - 4.0f, y + h - arm_h, 4.0f, arm_h);
}

/* The film's frame: black bars top and bottom, on the grid. */
static void draw_letterbox(SDL_Renderer *r, int win_w, int win_h, float bar)
{
    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, (float)win_w, bar);
    bbc_fill(r, BBC_BLACK, 0.0f, (float)win_h - bar, (float)win_w, bar);
}

/* ---- Shapes the bbc layer does not have -------------------------------- */

/* An ellipse filled row by row, `rx` across and `ry` down in logical pixels:
 * a puddle, a pool of lamplight, the helipad's ring. */
static void fill_ellipse(SDL_Renderer *r, BbcColour c, float cx, float cy,
                         float rx, float ry)
{
    if (!(rx > 0.0f) || !(ry > 0.0f))
        return;
    for (float y = bbc_snap_y(cy - ry); y <= cy + ry; y += (float)BBC_PX_H)
    {
        float t = (y + 1.0f - cy) / ry;
        if (t < -1.0f || t > 1.0f)
            continue;
        float half = rx * sqrtf(1.0f - t * t);
        if (half < 2.0f)
            continue;
        bbc_fill(r, c, cx - half, y, half * 2.0f, (float)BBC_PX_H);
    }
}

/* Its outline: the two ends of each row's span. */
static void ring_ellipse(SDL_Renderer *r, BbcColour c, float cx, float cy,
                         float rx, float ry)
{
    if (!(rx > 0.0f) || !(ry > 0.0f))
        return;
    float last_half = -1.0f;
    for (float y = bbc_snap_y(cy - ry); y <= cy + ry; y += (float)BBC_PX_H)
    {
        float t = (y + 1.0f - cy) / ry;
        if (t < -1.0f || t > 1.0f)
            continue;
        float half = rx * sqrtf(1.0f - t * t);
        float run = fabsf(half - last_half);
        if (last_half < 0.0f || t <= -0.9f || t >= 0.9f)
            run = half;
        if (run < (float)BBC_PX_W)
            run = (float)BBC_PX_W;
        if (t < -0.85f || t > 0.85f)
        {
            bbc_fill(r, c, cx - half, y, half * 2.0f, (float)BBC_PX_H);
        }
        else
        {
            bbc_fill(r, c, cx - half, y, run, (float)BBC_PX_H);
            bbc_fill(r, c, cx + half - run, y, run, (float)BBC_PX_H);
        }
        last_half = half;
    }
}

/*
 * A wedge of light as the machine drew one: not a translucent cone but a
 * triangle of dither laid over whatever it crosses, from a point to a span.
 * `pattern` decides how bright it reads — a searchlight is `SPARSE`, a lamp's
 * spill `DOTS`.
 */
static void dither_wedge(SDL_Renderer *r, BbcColour c, BbcDither pattern,
                         float ax, float ay, float bx0, float bx1, float by)
{
    if (by <= ay)
        return;
    for (float y = bbc_snap_y(ay); y < by; y += (float)BBC_PX_H)
    {
        float t = (y - ay) / (by - ay);
        float x0 = lerpf(ax, bx0, t);
        float x1 = lerpf(ax, bx1, t);
        if (x1 < x0)
        {
            float swap = x0;
            x0 = x1;
            x1 = swap;
        }
        bbc_stipple(r, c, pattern, x0, y, fmaxf(x1 - x0, (float)BBC_PX_W),
                    (float)BBC_PX_H);
    }
}

/* The same wedge lying on its side, from a point out to a vertical span at
 * `bx`: a headlamp's beam. It goes either way. */
static void dither_wedge_h(SDL_Renderer *r, BbcColour c, BbcDither pattern,
                           float ax, float ay, float bx, float by0, float by1)
{
    float dir = bx >= ax ? 1.0f : -1.0f;
    float len = fabsf(bx - ax);
    if (len < (float)BBC_PX_W)
        return;
    for (float d = 0.0f; d < len; d += (float)BBC_PX_W)
    {
        float t = d / len;
        float y0 = lerpf(ay, by0, t);
        float y1 = lerpf(ay, by1, t);
        float x = dir > 0.0f ? ax + d : ax - d - (float)BBC_PX_W;
        bbc_stipple(r, c, pattern, x, y0, (float)BBC_PX_W,
                    fmaxf(y1 - y0, (float)BBC_PX_H));
    }
}

/* ---- The night -------------------------------------------------------- */

/* Stars in the black: mostly the dim blue ones, a few white, and a handful
 * that drop to blue for a moment on clocks of their own, which is the only
 * twinkle a two-level star has. */
static void draw_stars(SDL_Renderer *r, float time, float w, float top,
                       float bottom, unsigned seed, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
    {
        unsigned h = scene_hash(i * 31u + seed);
        float x = spread(h, w);
        float y = top + spread(h >> 9, bottom - top);
        BbcColour c = (h & 3u) == 0u ? BBC_WHITE : BBC_BLUE;
        if ((h & 3u) == 0u && ((h >> 4) & 3u) == 0u)
        {
            float period = 1.7f + (float)((h >> 6) % 9u) * 0.31f;
            if (fmodf(time + (float)(h % 97u) * 0.05f, period) < 0.18f)
                c = BBC_BLUE;
        }
        if ((h & 15u) == 7u)
            c = BBC_CYAN;
        bbc_plot(r, c, x, y);
    }
}

/* A light the rain owes a colour to: a drop falling through the band under it
 * is drawn in the lamp's colour instead of the night's. */
typedef struct
{
    float x0;
    float x1;
    float top;
    BbcColour c;
} RainLight;

/*
 * The rain, as the machine drew rain: a few hundred slanted pixel streaks,
 * two cells long in the far layer and three in the near one, each moving down
 * a whole BBC pixel at a time on its own speed. Nothing blends; a drop that
 * crosses a lamp's band simply is the lamp's colour for those rows, which is
 * most of what makes rain read as rain at night.
 */
static void draw_rain(SDL_Renderer *r, float time, float w, float top,
                      float bottom, const RainLight *lights, int light_count,
                      unsigned count, BbcColour far_c, BbcColour near_c)
{
    float span = bottom - top + 40.0f;
    for (unsigned i = 0; i < count; ++i)
    {
        bool near = i * 4u >= count * 3u;
        unsigned h = scene_hash(i * 17u + 5u);
        float speed = (near ? 300.0f : 180.0f) + (float)(h % 80u);
        float drift = near ? 64.0f : 34.0f;
        float x = fmodf(spread(h, w + 80.0f) - time * drift, w + 80.0f);
        if (x < 0.0f)
            x += w + 80.0f;
        x -= 40.0f;
        float y = fmodf(spread(h >> 9, span) + time * speed, span) + top - 20.0f;
        if (y < top || y > bottom - 4.0f)
            continue;

        BbcColour c = near ? near_c : far_c;
        for (int l = 0; l < light_count; ++l)
        {
            if (y >= lights[l].top && x >= lights[l].x0 && x <= lights[l].x1)
                c = lights[l].c;
        }
        bbc_block(r, c, x, y, 1, 2);
        if (near && y + 8.0f < bottom)
            bbc_block(r, c, x - (float)BBC_PX_W, y + 4.0f, 1, 2);
    }
}

/* A drop landing: one cell of spray where it hits, alive for a tenth of a
 * second and then somewhere else. */
static void draw_splashes(SDL_Renderer *r, float time, float w, float top,
                          float bottom, unsigned count, BbcColour c)
{
    for (unsigned i = 0; i < count; ++i)
    {
        float period = 0.34f + (float)(i % 7u) * 0.05f;
        float local = time + (float)i * 0.173f;
        unsigned epoch = (unsigned)(int)floorf(local / period);
        float age = fmodf(local, period) / period;
        if (age > 0.30f)
            continue;
        unsigned h = scene_hash(i * 7919u + epoch * 104729u);
        float x = spread(h, w);
        float y = top + spread(h >> 10, bottom - top);
        bbc_plot(r, c, x, y);
        if (age > 0.12f)
        {
            bbc_plot(r, c, x - 4.0f, y + 2.0f);
            bbc_plot(r, c, x + 4.0f, y + 2.0f);
        }
    }
}

/*
 * The city as a BBC drew a city: blocks of blue on the black with a grid of
 * windows in them, a few lit yellow and a few cyan, and the hardware on the
 * roofs in silhouette. The far plane is blue dithered into the black, a step
 * further away than the solid blue near plane in front of it — the only depth
 * eight colours have.
 */
static void draw_far_city(SDL_Renderer *r, float time, float w, float base,
                          unsigned seed, int count, float min_h, float max_h)
{
    float pitch = w / (float)count;
    for (int i = 0; i < count; ++i)
    {
        unsigned h = scene_hash((unsigned)i * 131u + seed);
        float tw = pitch * (0.55f + spread(h, 0.4f));
        float x = (float)i * pitch + spread(h >> 5, pitch - tw) - 10.0f;
        float th = min_h + spread(h >> 11, max_h - min_h);
        float y = base - th;
        bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, x, y, tw, th);
        if ((h & 3u) == 0u)
            bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK,
                       x + tw * 0.25f, y - 14.0f, tw * 0.5f, 14.0f);
        if ((h & 3u) == 1u)
        {
            bbc_fill(r, BBC_BLUE, x + tw * 0.5f, y - 22.0f, 4.0f, 22.0f);
            if (fmodf(time + (float)i * 0.37f, 1.6f) < 0.8f)
                bbc_plot(r, BBC_RED, x + tw * 0.5f, y - 24.0f);
        }
        for (float wy = y + 8.0f; wy < base - 8.0f; wy += 10.0f)
        {
            for (float wx = x + 4.0f; wx < x + tw - 6.0f; wx += 8.0f)
            {
                unsigned wh = scene_hash(h + (unsigned)(wx * 7.0f + wy * 13.0f));
                if ((wh % 9u) != 0u)
                    continue;
                bbc_plot(r, (wh & 16u) ? BBC_YELLOW : BBC_CYAN, wx, wy);
            }
        }
    }
}

/* A near block: solid blue, black windows in a grid, some of them lit, the
 * moon along its left flank and its roofline. */
static void draw_near_block(SDL_Renderer *r, float time, float x, float top,
                            float w, float base, unsigned seed)
{
    bbc_fill(r, BBC_BLUE, x, top, w, base - top);
    bbc_fill(r, BBC_CYAN, x, top, 4.0f, base - top);
    bbc_fill(r, BBC_CYAN, x, top, w, 2.0f);

    switch (seed % 3u)
    {
    case 0:
        /* A water tank on its legs. */
        bbc_fill(r, BBC_BLUE, x + w * 0.6f, top - 8.0f, 4.0f, 8.0f);
        bbc_fill(r, BBC_BLUE, x + w * 0.6f + 12.0f, top - 8.0f, 4.0f, 8.0f);
        bbc_fill(r, BBC_BLUE, x + w * 0.6f - 2.0f, top - 22.0f, 20.0f, 14.0f);
        bbc_fill(r, BBC_CYAN, x + w * 0.6f - 2.0f, top - 22.0f, 4.0f, 14.0f);
        break;
    case 1:
        /* A mast with its aviation light. */
        bbc_fill(r, BBC_BLUE, x + 20.0f, top - 30.0f, 4.0f, 30.0f);
        bbc_fill(r, BBC_BLUE, x + 16.0f, top - 4.0f, 12.0f, 4.0f);
        if (fmodf(time + (float)(seed % 7u) * 0.37f, 1.6f) < 0.8f)
            bbc_block(r, BBC_RED, x + 20.0f, top - 34.0f, 1, 2);
        break;
    default:
        /* A stair head and a plant housing. */
        bbc_fill(r, BBC_BLUE, x + 8.0f, top - 10.0f, 20.0f, 10.0f);
        bbc_fill(r, BBC_CYAN, x + 8.0f, top - 10.0f, 20.0f, 2.0f);
        bbc_fill(r, BBC_BLUE, x + 40.0f, top - 6.0f, 28.0f, 6.0f);
        break;
    }

    for (float wy = top + 12.0f; wy < base - 14.0f; wy += 20.0f)
    {
        for (float wx = x + 12.0f; wx < x + w - 12.0f; wx += 20.0f)
        {
            unsigned wh = scene_hash(seed * 71u + (unsigned)(wy * 3.0f + wx));
            BbcColour c = BBC_BLACK;
            if ((wh & 7u) == 0u)
                c = (wh & 8u) ? BBC_YELLOW : BBC_CYAN;
            bbc_fill(r, c, wx, wy, 8.0f, 6.0f);
        }
    }
}

/* ---- The street ------------------------------------------------------- */

/* Where the cast stands, the kerb's lit arris, and the road. */
#define STREET_GROUND 437.0f
#define STREET_KERB_Y 442.0f
#define STREET_ROAD_Y 450.0f

/* A light the wet road owes a reflection to. */
typedef struct
{
    float x;
    float w;
    BbcColour c;
    int rows; /* how far down the road the streak reaches, in pairs of rows */
} WetLight;

/*
 * The street both prologue scenes are played on, and it is wet: the story says
 * it is raining, so every light in the shot is drawn twice, once where it
 * hangs and once broken up in the road under it — a column of short dashes,
 * every other row, wobbling a cell on its own slow clock, which is how a BBC
 * game drew light on water. The dash pattern is keyed to the light, never to
 * where it is on screen, so a streak under a moving car travels with the car.
 */
static void render_street(SDL_Renderer *r, float time, int win_w, int win_h,
                          const WetLight *lights, int light_count)
{
    const float w = (float)win_w;

    /* The pavement: wet slabs, black with the blue of the sky lying on
     * them, their joints black. */
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_SPARSE, 0.0f, 432.0f, w,
               STREET_KERB_Y - 432.0f);
    for (float jx = 20.0f; jx < w; jx += 64.0f)
        bbc_fill(r, BBC_BLACK, jx, 432.0f, 4.0f, STREET_KERB_Y - 432.0f);
    /* The kerb: a lit arris and its face. */
    bbc_fill(r, BBC_WHITE, 0.0f, STREET_KERB_Y, w, 2.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, STREET_KERB_Y + 2.0f, w, 4.0f);
    bbc_fill(r, BBC_BLACK, 0.0f, STREET_KERB_Y + 6.0f, w,
             (float)win_h - STREET_KERB_Y - 6.0f);

    /* The centre line, in dashes. */
    for (float dx = 4.0f; dx < w; dx += 128.0f)
        bbc_fill(r, BBC_WHITE, dx, 500.0f, 48.0f, 2.0f);

    for (int l = 0; l < light_count; ++l)
    {
        const WetLight *lt = &lights[l];
        unsigned seed = scene_hash((unsigned)(int)(lt->w * 13.0f) + (unsigned)l * 977u +
                                   (unsigned)lt->c * 31u);
        for (int row = 0; row < lt->rows; ++row)
        {
            unsigned h = scene_hash(seed + (unsigned)row * 131u);
            if ((h & 3u) == 0u)
                continue;
            float fall = (float)row / (float)lt->rows;
            float width = lt->w * (1.0f - fall * 0.6f) *
                          (0.35f + (float)(h % 8u) * 0.09f);
            float wobble = sinf(time * (1.3f + (float)(h % 5u) * 0.3f) +
                                (float)(h & 31u)) *
                           (2.0f + fall * 6.0f);
            float y = STREET_ROAD_Y + 2.0f + (float)row * 4.0f;
            BbcColour c = lt->c;
            /* The far half of a reflection holds the light's own colour and
             * the near half one rung down its ladder. */
            if (fall > 0.55f)
                c = c == BBC_YELLOW ? BBC_RED
                    : c == BBC_WHITE ? BBC_CYAN
                                     : BBC_BLUE;
            bbc_fill(r, c, lt->x - width * 0.5f + wobble, y,
                     fmaxf(width, 4.0f), 2.0f);
        }
    }

    /* Puddles lying in the road: a darker sheet with the sky caught along
     * its far lip, and the rings the rain keeps opening in it. */
    static const float puddles[][3] = {
        {140.0f, 470.0f, 40.0f}, {420.0f, 492.0f, 56.0f},
        {650.0f, 476.0f, 36.0f}, {300.0f, 518.0f, 48.0f}};
    for (int p = 0; p < 4; ++p)
    {
        float cx = puddles[p][0];
        float cy = puddles[p][1];
        float rx = puddles[p][2];
        ring_ellipse(r, BBC_BLUE, cx, cy, rx, 6.0f);
        bbc_fill(r, BBC_CYAN, cx - rx * 0.5f, cy - 6.0f, rx, 2.0f);
        for (unsigned k = 0; k < 2u; ++k)
        {
            unsigned h = scene_hash((unsigned)p * 977u + k * 131u);
            float period = 0.9f + (float)(h % 50u) * 0.012f;
            float age = fmodf(time + (float)(h % 97u) * 0.01f, period) / period;
            float px = cx + (spread(h >> 8, 1.0f) - 0.5f) * rx;
            float rr = 4.0f + age * rx * 0.3f;
            BbcColour c = age < 0.5f ? BBC_CYAN : BBC_BLUE;
            bbc_plot(r, c, px - rr, cy);
            bbc_plot(r, c, px + rr, cy);
        }
    }

    draw_splashes(r, time, w, STREET_ROAD_Y + 4.0f, (float)win_h - 22.0f, 26u,
                  BBC_CYAN);
    draw_splashes(r, time + 3.1f, w, 432.0f, STREET_KERB_Y, 8u, BBC_WHITE);
}

/* ---- The cast on the film's floor -------------------------------------- */

/*
 * The cast is the floor's cast — the same drawings the sector stands up, out
 * of [bbc_cast.c](bbc_cast.c) — at twice the floor's size: two BBC pixels to a
 * sprite pixel, so a figure is sixty-four pixels tall. The film is a wide shot
 * of a street and a roof, and at the floor's own size its people were a
 * thumbnail of the scene they are the subject of; doubled, they are the chunky
 * figures a loading screen of the period put in front of its scenery, and the
 * scenery behind them keeps the machine's finest pixel, which is what makes it
 * read as further off.
 *
 * Every position in the film is the *centre* a figure is stood on.
 */
#define CAST_SCALE 2
#define CAST_CELL_W ((float)(BBC_PX_W * CAST_SCALE))
#define CAST_CELL_H ((float)(BBC_PX_H * CAST_SCALE))
/* A standing figure, doubled: the height his head is at. */
#define CAST_H 64.0f

/* A walk read off the distance covered rather than the clock, so a foot stays
 * where it was put: a quarter of a stride per frame. */
#define CHUCK_RUN_QUARTER 14.0f
#define CAST_WALK_QUARTER 10.0f

static int stride_quarter(float distance, float quarter)
{
    return (int)floorf(distance / quarter) & 3;
}

static const BbcSprite *chuck_run_frame(float distance)
{
    static const BbcSprite *const frames[4] = {
        &SPR_CHUCK_WALK1, &SPR_CHUCK_WALK2, &SPR_CHUCK_WALK3,
        &SPR_CHUCK_WALK2};
    return frames[stride_quarter(distance, CHUCK_RUN_QUARTER)];
}

/* Ellen and the crew walk the way the guards patrol: a stride, a pass, the
 * other stride, a pass. */
static const BbcSprite *ellen_walk_frame(float distance, bool bound)
{
    switch (stride_quarter(distance, CAST_WALK_QUARTER))
    {
    case 0:
        return bound ? &SPR_ELLEN_BOUND1 : &SPR_ELLEN_WALK1;
    case 2:
        return bound ? &SPR_ELLEN_BOUND2 : &SPR_ELLEN_WALK2;
    default:
        return bound ? &SPR_ELLEN_BOUND2 : &SPR_ELLEN_STAND;
    }
}

static const BbcSprite *crew_walk_frame(float distance)
{
    return (stride_quarter(distance, CAST_WALK_QUARTER) & 1)
               ? &SPR_AGENT_STAND
               : &SPR_AGENT_WALK;
}

typedef enum
{
    AGENT_FOOT_NONE,
    AGENT_FOOT_NEAR,
    AGENT_FOOT_FAR
} AgentFoot;

/*
 * Which foot came down between two places on a run: one at each stride frame
 * of the drawing, so a step is heard on the frame a heel is drawn striking.
 * None is heard while the run has all but stopped at either end.
 */
static AgentFoot agent_footfall(float x0, float x1, int dir, float pace)
{
    if (pace < 0.2f)
        return AGENT_FOOT_NONE;
    float stride = CHUCK_RUN_QUARTER * 2.0f;
    int was = (int)floorf(x0 * (float)dir / stride);
    int now = (int)floorf(x1 * (float)dir / stride);
    if (now == was)
        return AGENT_FOOT_NONE;
    return (now & 1) ? AGENT_FOOT_FAR : AGENT_FOOT_NEAR;
}

/* Where a sprite stood on a point lands at `scale`: centred on `cx`, its
 * bottom row on `foot_y`, both rounded to the grid. */
static float sprite_left(const BbcSprite *s, float cx, int scale)
{
    float x = cx - bbc_sprite_px_w(s) * (float)scale * 0.5f;
    return floorf(x / (float)BBC_PX_W + 0.5f) * (float)BBC_PX_W;
}

static float sprite_top(const BbcSprite *s, float foot_y, int scale)
{
    float y = foot_y - bbc_sprite_px_h(s) * (float)scale;
    return floorf(y / (float)BBC_PX_H + 0.5f) * (float)BBC_PX_H;
}

/* The top-left of one sprite pixel of a figure stood on a point, honouring
 * the mirror, so a muzzle or a cup is put on the drawing rather than beside
 * it. */
static void sprite_cell(const BbcSprite *s, float cx, float foot_y, int dir,
                        int scale, int col, int row, float *out_x,
                        float *out_y)
{
    int w = bbc_sprite_w(s);
    int c = dir >= 0 ? col : w - 1 - col;
    *out_x = sprite_left(s, cx, scale) + (float)(c * BBC_PX_W * scale);
    *out_y = sprite_top(s, foot_y, scale) + (float)(row * BBC_PX_H * scale);
}

static void stand_at(SDL_Renderer *r, const BbcSprite *s, float cx,
                     float foot_y, int dir, const BbcColour *paint, int scale)
{
    bbc_sprite_scaled(r, s, sprite_left(s, cx, scale),
                      sprite_top(s, foot_y, scale), dir, paint, scale);
}

static void stand(SDL_Renderer *r, const BbcSprite *s, float cx, float foot_y,
                  int dir, const BbcColour *paint)
{
    stand_at(r, s, cx, foot_y, dir, paint, CAST_SCALE);
}

/*
 * The poses the shared cast does not have, because they happen only here.
 * Each is in the cast's own style — eight wide, feet on the bottom row,
 * facing right — and built from the same parts as the drawing it is a variant
 * of, so the man who starts back from the rifles is the man at the window.
 */

/* The rifles: started back from them, both hands up at his face, mouth open. */
BBC_SPRITE(CHUCK_FRIGHT,
    "...RRR..",
    "..RRRRRR",
    "...WKW.W",
    "...WKWCW",
    "....WC..",
    "..CCCC..",
    ".WCCCCC.",
    ".CCCCC..",
    "..CCCC..",
    "..CCCC..",
    "..BBBB..",
    "..BB.BB.",
    ".BB..BB.",
    ".BB..BB.",
    "BB...BB.",
    "RR...RR.");

/* Her scream: down into his knees to go, the weight thrown forward, fists
   closed. */
BBC_SPRITE(CHUCK_LUNGE,
    "........",
    "........",
    "....RRR.",
    "...RRRRR",
    "....WKW.",
    "....WWW.",
    "...CCW..",
    "..CCCCCW",
    ".WCCCCC.",
    "..CCCC..",
    "..BBBB..",
    ".BBBBBB.",
    "BB....BB",
    "BB....BB",
    "B.....BB",
    "RR...RR.");

/* At the lip with his forearms flat on the coping, head and shoulders over the
 * roof and the rest of him hanging: the key art's Chuck. Ten wide, because his
 * arms reach past the box; the coping is under row four. */
BBC_SPRITE(CHUCK_LEDGE,
    "...RRR....",
    "..RRRRRR..",
    "...WKW....",
    "...WWW....",
    "..CCCCCWWW",
    "..CCCCCCCC",
    "..CBCCC...",
    "..CBCCC...",
    "..CCCC....",
    "..BBBB....",
    "..BB.BB...",
    "..BB.BB...",
    "..BB.BB...",
    "..BB.BB...",
    "..RR.RR...",
    "..........");

/* Taped at the wrists and standing: the bound drawing's arms on the standing
 * drawing's legs. */
BBC_SPRITE(ELLEN_BOUND_STAND,
    "..YYY..",
    ".YYYYY.",
    ".YWKWY.",
    ".YWWWY.",
    "...W...",
    "..RRR..",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRWW",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    "..W.W..",
    "..W.W..",
    "..W.W..",
    ".KK.KK.");

/* Down on her knees with her taped hands over her head: the blast. */
BBC_SPRITE(ELLEN_DUCK,
    ".......",
    ".......",
    ".......",
    ".......",
    ".......",
    ".......",
    "....WW.",
    "..YYYW.",
    ".YYYYY.",
    ".YWKWY.",
    ".YWWWY.",
    "..RRR..",
    ".RRRRR.",
    "RRRRRRR",
    ".RW.WR.",
    ".KK.KK.");

/* Her mouth open on his name, the taped hands up. */
BBC_SPRITE(ELLEN_CRY,
    "..YYY..",
    ".YYYYY.",
    ".YWKWY.",
    ".YWKWY.",
    "...W.WW",
    "..RRRR.",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    "..W.W..",
    "..W.W..",
    "..W.W..",
    ".KK.KK.");

/* The crew's rear man on the move with his rifle up at her back: the aimed
 * drawing's top on the walk's legs. */
BBC_SPRITE(AGENT_AIM_WALK,
    "...222..",
    "..2222..",
    "..2RR2..",
    "..2222..",
    "...22...",
    "..1111..",
    "..11WWWW",
    "..1111..",
    "..1111..",
    "..1111..",
    "..1111..",
    "..11111.",
    ".11..11.",
    ".11...11",
    "11....11",
    "KK.....K");

/* Voss with the pistol down along the line to the man under him. */
BBC_SPRITE(VOSS_AIM_DOWN,
    "...WWW..",
    "..WWWWW.",
    "..WWKW..",
    "...WWW..",
    "....W...",
    "..MWWM..",
    ".MMWWMM.",
    ".MMMWMMM",
    ".MMMMMMW",
    ".MMMMMMY",
    ".MMMMMM.",
    ".MMMMMM.",
    "..MM.MM.",
    "..BB.BB.",
    "..BB.BB.",
    "..KK.KKK");

/* Voss down on the roof, on his back, the coat spread. */
BBC_SPRITE(VOSS_DOWN,
    "........W",
    "KBBMMMMWW",
    "KBBMMMMWW",
    "...MWM...");

/* ---- Vehicles ---------------------------------------------------------- */

/*
 * The two cars, drawn on squared paper the way the cast is and at the cast's
 * own pixel: in whole cells from a corner snapped to the grid, so a car that
 * moves takes every part of itself along a cell at a time instead of
 * shimmering. A pen is that corner and the size of its cells.
 */
typedef struct
{
    SDL_Renderer *r;
    float ox;
    float oy;
    int k; /* BBC pixels to a cell */
} Pen;

static Pen pen_at(SDL_Renderer *r, float x, float y, int k)
{
    return (Pen){r, bbc_snap_x(x), bbc_snap_y(y), k};
}

static float pen_x(const Pen *p, int col)
{
    return p->ox + (float)(col * BBC_PX_W * p->k);
}

static float pen_y(const Pen *p, int row)
{
    return p->oy + (float)(row * BBC_PX_H * p->k);
}

static void pen_block(const Pen *p, BbcColour c, int col, int row, int w,
                      int h)
{
    bbc_block(p->r, c, pen_x(p, col), pen_y(p, row), w * p->k, h * p->k);
}

/* A run of a pen's cells in a dither anchored to the pen, so a car's paint
 * travels with the car instead of crawling over it as it moves. The dither is
 * at the machine's own pixel, which is what a material is. */
static void pen_dither(const Pen *p, BbcColour base, BbcColour ink,
                       BbcDither pattern, int col, int row, int w, int h)
{
    bbc_set_pattern_origin(p->ox, p->oy);
    bbc_dither(p->r, base, ink, pattern, pen_x(p, col), pen_y(p, row),
               (float)(w * BBC_PX_W * p->k), (float)(h * BBC_PX_H * p->k));
    bbc_set_pattern_origin(0.0f, 0.0f);
}

/* One wheel, four cells by eight with its hub cap, and the black mark on the
 * cap in one of four places round it — which is all a turning wheel is at
 * this size. */
static void pen_wheel(const Pen *p, int col, int row, int turn)
{
    pen_block(p, BBC_BLACK, col + 1, row, 2, 8);
    pen_block(p, BBC_BLACK, col, row + 1, 4, 6);
    pen_block(p, BBC_WHITE, col + 1, row + 2, 2, 4);
    static const int marks[4][2] = {{0, 0}, {1, 1}, {1, 3}, {0, 2}};
    const int *m = marks[turn & 3];
    pen_block(p, BBC_BLUE, col + 1 + m[0], row + 2 + m[1], 1, 1);
}

#define SUV_W 176.0f /* 22 cells */
#define SUV_H 64.0f  /* 16 rows */

/*
 * The crew's SUV, side on and facing right: the one blue car on the road, as
 * it is on the drive, with its roof rack and the spare on its tailgate in
 * white — the one car the player must never mistake for another, so it is
 * painted the same on every screen it is on. Every pane is smoked black,
 * because it is the one car nobody can see into.
 *
 * `headlights` is separate from `moving` because the one thing anybody
 * remembers about the vehicle is that it came up the kerb lane dark. A rolling
 * SUV throwing a beam is a car; a rolling SUV throwing nothing is a decision.
 * The tail lamp follows the headlamp for the same reason.
 */
static void draw_suv(SDL_Renderer *r, float x, float ground_y, bool moving,
                     bool headlights, bool door_open)
{
    Pen p = pen_at(r, x, ground_y - SUV_H, CAST_SCALE);
    int turn = moving ? (int)floorf(x / 10.0f) : 1;

    if (headlights)
        dither_wedge_h(r, BBC_YELLOW, BBC_DITHER_SPARSE, p.ox + SUV_W,
                       pen_y(&p, 7), p.ox + SUV_W + 150.0f, pen_y(&p, 4),
                       ground_y);

    /* The roof rack. */
    pen_block(&p, BBC_WHITE, 4, 0, 12, 1);
    pen_block(&p, BBC_WHITE, 5, 1, 1, 1);
    pen_block(&p, BBC_WHITE, 10, 1, 1, 1);
    pen_block(&p, BBC_WHITE, 15, 1, 1, 1);

    /* The cabin, the moon along its roof and down the raked screen. */
    pen_block(&p, BBC_BLUE, 2, 2, 14, 4);
    pen_block(&p, BBC_BLUE, 16, 3, 1, 3);
    pen_block(&p, BBC_BLUE, 17, 4, 1, 2);
    pen_block(&p, BBC_BLUE, 18, 5, 1, 1);
    pen_block(&p, BBC_CYAN, 2, 2, 14, 1);
    pen_block(&p, BBC_CYAN, 16, 3, 1, 1);
    pen_block(&p, BBC_CYAN, 17, 4, 1, 1);
    pen_block(&p, BBC_CYAN, 18, 5, 1, 1);
    /* Three smoked panes: black with a quarter of it white. */
    pen_dither(&p, BBC_BLACK, BBC_WHITE, BBC_DITHER_SPARSE, 3, 3, 3, 2);
    pen_dither(&p, BBC_BLACK, BBC_WHITE, BBC_DITHER_SPARSE, 7, 3, 4, 2);
    pen_dither(&p, BBC_BLACK, BBC_WHITE, BBC_DITHER_SPARSE, 12, 3, 4, 2);
    pen_dither(&p, BBC_BLACK, BBC_WHITE, BBC_DITHER_SPARSE, 16, 4, 1, 1);

    /* The flank under the waist chrome, the bonnet, the white spare. */
    pen_block(&p, BBC_BLUE, 0, 6, 22, 6);
    pen_block(&p, BBC_WHITE, 1, 6, 21, 1);
    pen_block(&p, BBC_WHITE, -1, 3, 1, 6);
    pen_block(&p, BBC_BLACK, -1, 5, 1, 2);
    /* Door shuts and handles. */
    pen_block(&p, BBC_BLACK, 6, 7, 1, 4);
    pen_block(&p, BBC_BLACK, 11, 7, 1, 4);
    pen_block(&p, BBC_BLACK, 16, 7, 1, 3);
    pen_block(&p, BBC_WHITE, 9, 7, 1, 1);
    pen_block(&p, BBC_WHITE, 14, 7, 1, 1);
    /* The grille, the bumpers and the sill. */
    pen_block(&p, BBC_BLACK, 21, 8, 1, 3);
    pen_block(&p, BBC_WHITE, 19, 11, 3, 1);
    pen_block(&p, BBC_WHITE, 0, 11, 2, 1);
    pen_block(&p, BBC_BLACK, 8, 11, 6, 1);
    /* Arches over the wheels. */
    pen_block(&p, BBC_BLACK, 3, 8, 4, 1);
    pen_block(&p, BBC_BLACK, 2, 9, 6, 3);
    pen_block(&p, BBC_BLACK, 15, 8, 4, 1);
    pen_block(&p, BBC_BLACK, 14, 9, 6, 3);

    /* Lamps: off is a reflector, on is the lamp. */
    pen_block(&p, headlights ? BBC_WHITE : BBC_BLACK, 21, 7, 1, 1);
    pen_block(&p, headlights ? BBC_RED : BBC_BLACK, 0, 7, 1, 2);

    if (door_open)
    {
        /* The near rear door stands open on a lit cabin: the one warm thing
         * in the car, and the hole she goes into. */
        pen_block(&p, BBC_YELLOW, 7, 3, 4, 2);
        pen_block(&p, BBC_YELLOW, 7, 7, 4, 4);
        pen_block(&p, BBC_RED, 7, 9, 4, 2);
        pen_block(&p, BBC_BLUE, 4, 2, 2, 9);
        pen_block(&p, BBC_WHITE, 4, 2, 1, 9);
        pen_dither(&p, BBC_BLACK, BBC_WHITE, BBC_DITHER_SPARSE, 5, 3, 1, 2);
    }

    pen_wheel(&p, 3, 8, turn);
    pen_wheel(&p, 15, 8, turn);
}

#define SEDAN_W 160.0f /* 20 cells */
#define SEDAN_H 56.0f  /* 14 rows */

/* Chuck's car, in his colour: the cyan saloon the drive is driven in, the only
 * cyan car on the road, with the white double stripe it carries there. The
 * man at the wheel is a cell of cap and a cell of face in the window. */
static void draw_sedan(SDL_Renderer *r, float x, float ground_y, bool moving,
                       bool occupied)
{
    Pen p = pen_at(r, x, ground_y - SEDAN_H, CAST_SCALE);
    int turn = moving ? (int)floorf(x / 10.0f) : 2;

    if (moving)
        dither_wedge_h(r, BBC_YELLOW, BBC_DITHER_SPARSE, p.ox + SEDAN_W,
                       pen_y(&p, 4), p.ox + SEDAN_W + 130.0f, pen_y(&p, 2),
                       ground_y);

    /* The cabin: a roof, the rear screen and the windscreen raked. */
    pen_block(&p, BBC_WHITE, 6, 0, 7, 1);
    pen_block(&p, BBC_CYAN, 5, 1, 9, 1);
    pen_block(&p, BBC_CYAN, 4, 2, 11, 1);
    pen_block(&p, BBC_CYAN, 3, 3, 13, 1);
    pen_block(&p, BBC_BLUE, 6, 1, 3, 2);
    pen_block(&p, BBC_BLUE, 5, 2, 1, 1);
    pen_block(&p, BBC_BLUE, 10, 1, 3, 2);
    pen_block(&p, BBC_BLUE, 13, 2, 1, 1);
    if (occupied)
    {
        /* His cap and his face at the wheel. */
        pen_block(&p, BBC_RED, 11, 1, 1, 1);
        pen_block(&p, BBC_WHITE, 11, 2, 1, 1);
    }

    /* The body: boot and bonnet, the white moulding down the side. */
    pen_block(&p, BBC_CYAN, 0, 4, 20, 5);
    pen_block(&p, BBC_WHITE, 0, 4, 3, 1);
    pen_block(&p, BBC_WHITE, 16, 4, 4, 1);
    /* The double stripe down his flank, as the drive draws it. */
    bbc_fill(r, BBC_WHITE, pen_x(&p, 1), pen_y(&p, 5) + 2.0f,
             pen_x(&p, 19) - pen_x(&p, 1), 2.0f);
    bbc_fill(r, BBC_WHITE, pen_x(&p, 1), pen_y(&p, 6) + 2.0f,
             pen_x(&p, 19) - pen_x(&p, 1), 2.0f);
    pen_block(&p, BBC_BLUE, 9, 4, 1, 4);
    pen_block(&p, BBC_BLUE, 14, 4, 1, 3);
    pen_block(&p, BBC_WHITE, 7, 4, 1, 1);
    pen_block(&p, BBC_WHITE, 12, 4, 1, 1);
    /* Arches. */
    pen_block(&p, BBC_BLACK, 1, 6, 6, 3);
    pen_block(&p, BBC_BLACK, 13, 6, 6, 3);

    pen_block(&p, moving ? BBC_WHITE : BBC_BLUE, 19, 5, 1, 1);
    pen_block(&p, moving ? BBC_RED : BBC_BLUE, 0, 5, 1, 1);

    pen_wheel(&p, 2, 6, turn);
    pen_wheel(&p, 14, 6, turn);
}

/* ---- The tower from the street ---------------------------------------- */

/*
 * Which of the tower's windows are lit, asked in one place: the facade asks
 * it to draw them and the wet street asks it again to reflect them, so the
 * road can never show a light the building does not have.
 */
static bool tower_window_lit(int row, int col, float time, unsigned *variation)
{
    unsigned h = scene_hash((unsigned)(row * 29 + col * 7 + 19));
    *variation = h;
    /* One office light flickers during the establishing shot. */
    if (row == 2 && col == 4)
        return fmodf(time, 2.8f) < 2.2f;
    return (h % 6u) == 0u;
}

/* The beacon on the plant room: red on, black off, at its own rate. */
static bool beacon_on(float time)
{
    return sinf(time * 4.6f) > 0.2f;
}

/*
 * One office window: a black pane in the blue, or a lit one — yellow or cyan
 * from a ceiling lamp along its top, the blinds half down in some, somebody
 * still at a desk in others. Everything that varies is keyed to the window's
 * own hash, so nothing on the facade changes while the camera holds.
 */
static void draw_tower_window(SDL_Renderer *r, float x, float y, float w,
                              float h, bool lit, unsigned variation)
{
    bbc_fill(r, BBC_BLACK, x, y, w, h);
    if (!lit)
    {
        /* The moon in the dark glass, on the diagonal. */
        float band = (float)((variation >> 7) % 3u) * 4.0f;
        bbc_plot(r, BBC_BLUE, x + 4.0f + band, y + h - 6.0f);
        bbc_plot(r, BBC_BLUE, x + 8.0f + band, y + h - 10.0f);
        return;
    }
    BbcColour glass = (variation & 2u) ? BBC_YELLOW : BBC_CYAN;
    bbc_fill(r, glass, x + 4.0f, y + 2.0f, w - 8.0f, h - 4.0f);
    bbc_fill(r, BBC_WHITE, x + 4.0f, y + 2.0f, w - 8.0f, 2.0f);
    if ((variation >> 5) % 3u == 0u)
    {
        /* Blinds half down. */
        for (float sy = y + 6.0f; sy < y + h * 0.55f; sy += 4.0f)
            bbc_fill(r, glass == BBC_YELLOW ? BBC_RED : BBC_BLUE, x + 4.0f,
                     sy, w - 8.0f, 2.0f);
    }
    else if ((variation >> 5) % 5u == 1u)
    {
        /* Somebody still at a desk. */
        float px = x + 8.0f + (float)((variation >> 9) % 3u) * 4.0f;
        bbc_fill(r, BBC_BLACK, px, y + h - 10.0f, 4.0f, 8.0f);
        bbc_fill(r, BBC_BLACK, px - 4.0f, y + h - 6.0f, 12.0f, 4.0f);
    }
    bbc_fill(r, BBC_BLACK, x + w * 0.5f - 2.0f, y, 4.0f, h);
}

/* ---- The tower's front door -------------------------------------------- */

/*
 * Kessler Tower close to: blue to the roof, the grid of glass the kerb showed
 * from five blocks off, the cyan arris the moon finds, the red beacon on the
 * plant room — and at its foot the one lit thing on the street, the lobby,
 * yellow behind its glass under a white canopy.
 */
#define ARRIVAL_TOWER_X(win_w) ((float)(win_w) - 390.0f)
/* The lobby's opening, from the tower's left edge, and how tall it is: tall
 * enough for the doubled cast to walk into. */
#define ARRIVAL_LOBBY_DX 205.0f
#define ARRIVAL_LOBBY_W 112.0f
#define ARRIVAL_LOBBY_H 92.0f

static void render_tower(SDL_Renderer *r, float time, int win_w,
                         float door_open)
{
    const float x = ARRIVAL_TOWER_X(win_w);
    const float y = 26.0f;
    const float w = 350.0f;
    const float ground = STREET_GROUND;
    const float base = ground - 124.0f;

    /* The shadowed return down the right, then the face. */
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, x + w, y + 8.0f,
               12.0f, ground - y - 8.0f);
    bbc_fill(r, BBC_BLUE, x, y, w, base - y);
    bbc_fill(r, BBC_WHITE, x, y, w, 2.0f);
    bbc_fill(r, BBC_CYAN, x, y + 2.0f, w, 2.0f);
    bbc_fill(r, BBC_CYAN, x, y + 2.0f, 4.0f, base - y - 2.0f);

    /* Plant on the roof and the beacon on it. */
    bbc_fill(r, BBC_BLUE, x + 30.0f, y - 12.0f, 44.0f, 12.0f);
    bbc_fill(r, BBC_CYAN, x + 30.0f, y - 12.0f, 44.0f, 2.0f);
    for (float gx = x + 34.0f; gx < x + 70.0f; gx += 8.0f)
        bbc_fill(r, BBC_BLACK, gx, y - 8.0f, 4.0f, 6.0f);
    bbc_fill(r, BBC_BLUE, x + 118.0f, y - 6.0f, 24.0f, 6.0f);
    bbc_fill(r, BBC_BLUE, x + 278.0f, y - 10.0f, 32.0f, 10.0f);
    bbc_fill(r, beacon_on(time) ? BBC_RED : BBC_BLACK, x + 286.0f,
             y - 16.0f, 12.0f, 6.0f);

    /* Eight floors of offices, the slab between each. */
    for (int row = 0; row < 8; ++row)
    {
        float wy = y + 20.0f + (float)row * 34.0f;
        bbc_fill(r, BBC_BLACK, x + 8.0f, wy + 24.0f, w - 16.0f, 2.0f);
        for (int col = 0; col < 7; ++col)
        {
            unsigned h;
            bool lit = tower_window_lit(row, col, time, &h);
            draw_tower_window(r, x + 18.0f + (float)col * 45.0f, wy, 28.0f,
                              18.0f, lit, h);
        }
    }

    /* Street-level stone, dark, in courses ruled in blue, with a white string
     * course along its top: dark so the blue SUV parked against it and the
     * men in blue walking past it are shapes on it rather than in it. */
    bbc_fill(r, BBC_BLACK, x - 12.0f, base, w + 24.0f, ground - base);
    for (float cy = base + 14.0f; cy < ground; cy += 14.0f)
    {
        bbc_fill(r, BBC_BLUE, x - 12.0f, cy, w + 24.0f, 2.0f);
        float off = ((int)((cy - base) / 14.0f) & 1) ? 0.0f : 24.0f;
        for (float jx = x - 12.0f + off; jx < x + w + 12.0f; jx += 48.0f)
            bbc_fill(r, BBC_BLUE, jx, cy - 12.0f, 4.0f, 12.0f);
    }
    bbc_fill(r, BBC_WHITE, x - 12.0f, base - 2.0f, w + 24.0f, 4.0f);

    /* Three shopfronts under the offices, shuttered for the night. */
    for (int i = 0; i < 3; ++i)
    {
        float sx = x + 24.0f + (float)i * 58.0f;
        bbc_fill(r, BBC_CYAN, sx - 4.0f, ground - 84.0f, 52.0f, 84.0f);
        bbc_fill(r, BBC_BLACK, sx, ground - 80.0f, 44.0f, 80.0f);
        for (float sy = ground - 72.0f; sy < ground - 2.0f; sy += 8.0f)
            bbc_fill(r, BBC_BLUE, sx + 4.0f, sy, 36.0f, 2.0f);
    }

    /* The lobby: a deep opening, lit from its own ceiling, the reception
     * desk a band across it, and the doors. */
    const float lx = x + ARRIVAL_LOBBY_DX;
    const float lw = ARRIVAL_LOBBY_W;
    const float lh = ARRIVAL_LOBBY_H;
    bbc_fill(r, BBC_BLACK, lx, ground - lh, lw, lh);
    bbc_fill(r, BBC_YELLOW, lx + 8.0f, ground - lh + 8.0f, lw - 16.0f,
             lh - 8.0f);
    bbc_fill(r, BBC_WHITE, lx + 8.0f, ground - lh + 8.0f, lw - 16.0f, 2.0f);
    for (float px = lx + 20.0f; px < lx + lw - 16.0f; px += 24.0f)
        bbc_fill(r, BBC_WHITE, px, ground - lh + 12.0f, 8.0f, 2.0f);
    bbc_fill(r, BBC_RED, lx + 20.0f, ground - 30.0f, 72.0f, 2.0f);
    bbc_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, lx + 20.0f,
               ground - 28.0f, 72.0f, 14.0f);
    /* The doors slide apart as people go through them. */
    float leaf = 44.0f - door_open * 28.0f;
    float top = ground - lh + 8.0f;
    bbc_fill(r, BBC_WHITE, lx + 8.0f, top, 4.0f, lh - 8.0f);
    bbc_fill(r, BBC_WHITE, lx + lw - 12.0f, top, 4.0f, lh - 8.0f);
    bbc_fill(r, BBC_WHITE, lx + 12.0f + leaf, top, 4.0f, lh - 8.0f);
    bbc_fill(r, BBC_WHITE, lx + lw - 16.0f - leaf, top, 4.0f, lh - 8.0f);
    bbc_fill(r, BBC_WHITE, lx + 4.0f + leaf, ground - 44.0f, 10.0f, 2.0f);
    bbc_fill(r, BBC_WHITE, lx + lw - 14.0f - leaf, ground - 44.0f, 10.0f,
             2.0f);

    /* The canopy and its strip light, and the light it throws on the
     * pavement — a pool of dither rather than a glow. */
    bbc_fill(r, BBC_WHITE, lx - 12.0f, ground - lh - 8.0f, lw + 24.0f, 6.0f);
    bbc_fill(r, BBC_CYAN, lx - 12.0f, ground - lh - 2.0f, lw + 24.0f, 2.0f);
    bbc_fill(r, BBC_YELLOW, lx + 16.0f, ground - lh, lw - 32.0f, 2.0f);
    for (float py = 432.0f; py < STREET_KERB_Y; py += 2.0f)
        bbc_stipple(r, BBC_YELLOW, BBC_DITHER_CHECK, lx - 20.0f, py,
                    lw + 40.0f, 2.0f);

    /* The building's own name, at the font's own size and centred on the
     * entrance block it is fixed to — 104 pixels of lettering across 112 of
     * stone, which is what a parapet nameplate looks like. */
    bbc_fill(r, BBC_BLACK, x + 205.0f, ground - lh - 24.0f, 112.0f, 14.0f);
    text(r, x + 209.0f, ground - lh - 21.0f, 1, BBC_WHITE, "KESSLER TOWER");
}

/*
 * The city to the left of the tower, in the two planes: the far row dithered
 * into the night, the near row solid.
 */
static void render_arrival_city(SDL_Renderer *r, float time, int win_w)
{
    draw_stars(r, time, (float)win_w, 22.0f, 250.0f, 31u, 70u);
    draw_far_city(r, time, (float)win_w * 0.6f, 432.0f, 977u, 7, 150.0f,
                  240.0f);
    for (int i = 0; i < 4; ++i)
    {
        float x = (float)(i * 104 - 35);
        float h = 88.0f + (float)((i * 37) % 105);
        draw_near_block(r, time, x, 432.0f - h, 86.0f, 432.0f,
                        (unsigned)i * 7u + 3u);
    }
}

/* ---- The tower's front door: the scene ---------------------------------- */

/*
 * Chuck outside the tower: out of his car the moment it stops, and running
 * while they walk her in.
 *
 * He used to sit at the wheel for three seconds after the car had parked,
 * while two men walked his wife across the pavement in front of him and in
 * through the doors. Everything that makes the kerb read as a man who cannot
 * get there in time is undone by a man who does not try. Now his door goes as
 * the car settles, he is at a run before they are halfway to the glass, and
 * the doors take her a second before he reaches them.
 *
 * Who is in front of what is the kerb's rule: the camera is across the road,
 * the parked cars are nearer to it than the pavement, and everybody who gets
 * out of a car is in the road in front of it until they are round a bonnet.
 */
#define ARRIVAL_SUV_STOP_X 420.0f
#define ARRIVAL_CAR_STOP_X 40.0f
#define ARRIVAL_CHUCK_DOOR_X 152.0f  /* at his own driver's door */
#define ARRIVAL_CHUCK_LOBBY_X 671.0f /* at the lobby's glass */
#define ARRIVAL_CAR_DOOR_TIME 4.28f
#define ARRIVAL_CHUCK_OUT_TIME 4.40f
#define ARRIVAL_RUN_TIME 4.45f
#define ARRIVAL_AT_LOBBY_TIME 7.85f
#define ARRIVAL_CHUCK_IN_TIME 8.10f
/* The street held empty behind him under his name, then a fade of 1.15s that
   is black a tenth of a second before the beat hands over. */
#define ARRIVAL_FADE_TIME (OPENING_CUTSCENE_DURATION - 1.25f)
/* How long he takes to get up to his speed, and to pull up at the doors. */
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

static void draw_arrival_chuck(SDL_Renderer *r, float time, float ground)
{
    if (time < ARRIVAL_CHUCK_OUT_TIME || time >= ARRIVAL_CHUCK_IN_TIME)
        return;

    float x = time < ARRIVAL_RUN_TIME ? ARRIVAL_CHUCK_DOOR_X
                                      : ARRIVAL_CHUCK_LOBBY_X;
    float pace = 0.0f;
    const BbcSprite *s = &SPR_CHUCK_STAND;
    if (arrival_chuck_run(time, &x, &pace) && pace > 0.15f)
        s = chuck_run_frame(x);
    stand(r, s, x, ground, 1, NULL);
}

static void render_cinematic_ui(SDL_Renderer *r, float time, int win_w,
                                int win_h, float target_x,
                                const PadHints *pad)
{
    if (time > 0.55f && time < 4.5f)
        draw_caption(r, 35.0f, 38.0f, BBC_CYAN, "00:22 // FINANCIAL DISTRICT",
                     time - 0.55f, 52.0f, BBC_RED);

    if (time > 3.0f && time < 6.5f)
    {
        /*
         * The caption is right-aligned to the bracket it labels, and that is a
         * fix rather than a taste.
         *
         * Drawn from `target_x + 3` it ran 200px to the right of a parked SUV
         * and straight into the building's own parapet nameplate, which
         * `render_tower` fixes over the entrance block on very nearly the
         * same baseline — so the frame read `SAME SUV // KESSLER
         * TOWEKESSLER TOWER` for the whole 3.5s the caption is up.
         * Right-aligned rather than nudged, because a caption belongs to the
         * bracket under it: end the label where the bracket ends and the pair
         * travels together if the SUV's stop ever moves, which a hand-picked x
         * would not.
         */
        const char *label = "SAME SUV // KESSLER TOWER";
        float bracket_x = target_x - 8.0f;
        float bracket_w = SUV_W + 16.0f;
        draw_target_brackets(r, bracket_x, STREET_GROUND - SUV_H - 8.0f,
                             bracket_w, SUV_H + 14.0f);
        typed_text_backed(r, bracket_x + bracket_w - text_w(label, 1),
                          STREET_GROUND - SUV_H - 22.0f, BBC_RED, label,
                          time - 3.0f);
    }

    if (time > 7.1f && time < ARRIVAL_FADE_TIME - 0.15f)
        draw_caption(r, 35.0f, 38.0f, BBC_YELLOW,
                     "CHUCK ROSS // NOBODY ELSE IS COMING", time - 7.1f, 52.0f,
                     BBC_RED);

    if (time > 0.9f && time < ARRIVAL_FADE_TIME)
        draw_skip_hint(r, win_w, win_h, pad);
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

void opening_cutscene_init(OpeningCutscene *cutscene)
{
    SDL_zerop(cutscene);
}

/* How far apart the lobby doors stand: open for her, shut, and open again for
   him. */
static float arrival_door_open(float time)
{
    float her = smoothstep01((time - 6.70f) / 0.30f) *
                (1.0f - smoothstep01((time - 7.35f) / 0.30f));
    float his = smoothstep01((time - 7.70f) / 0.25f) *
                (1.0f - smoothstep01((time - 8.25f) / 0.30f));
    return fmaxf(her, his);
}

void opening_cutscene_render(SDL_Renderer *r, const OpeningCutscene *cutscene,
                             int win_w, int win_h, const PadHints *pad)
{
    const float time = cutscene->time;
    const float ground = STREET_GROUND;

    float fade_in = 1.0f - smoothstep01(time / 0.68f);
    float fade_out = smoothstep01((time - ARRIVAL_FADE_TIME) / 1.15f);
    scene_begin(fade_steps(fmaxf(fade_in, fade_out)));

    render_arrival_city(r, time, win_w);
    render_tower(r, time, win_w, arrival_door_open(time));

    /* The canopy and the tower's lower lit windows, once more in the road:
     * the same question `render_tower` asked, asked again. */
    WetLight lights[24];
    int light_count = 0;
    const float tower_x = ARRIVAL_TOWER_X(win_w);
    lights[light_count++] = (WetLight){
        tower_x + ARRIVAL_LOBBY_DX + ARRIVAL_LOBBY_W * 0.5f, 72.0f,
        BBC_YELLOW, 18};
    for (int row = 5; row < 8; ++row)
    {
        for (int col = 0; col < 7 && light_count < 24; ++col)
        {
            unsigned h;
            if (!tower_window_lit(row, col, time, &h))
                continue;
            lights[light_count++] = (WetLight){
                tower_x + 32.0f + (float)col * 45.0f, 16.0f,
                (h & 2u) ? BBC_YELLOW : BBC_CYAN, 6 + (row - 5) * 2};
        }
    }
    render_street(r, time, win_w, win_h, lights, light_count);

    float suv_move = ease_out_cubic((time - 0.65f) / 2.65f);
    float suv_x = lerpf(-200.0f, ARRIVAL_SUV_STOP_X, suv_move);
    bool suv_moving = time < 3.30f;
    bool suv_door_open = time >= 3.75f && time < 7.0f;

    float car_move = ease_out_cubic((time - 1.45f) / 2.55f);
    float car_x = lerpf(-180.0f, ARRIVAL_CAR_STOP_X, car_move);
    bool car_moving = time < 4.0f;
    bool chuck_in_car = time < ARRIVAL_CHUCK_OUT_TIME;

    draw_suv(r, suv_x, ground, suv_moving, suv_moving, suv_door_open);
    draw_sedan(r, car_x, ground, car_moving, chuck_in_car);

    /* The two who get out of it walk her along its flank and round its nose
     * to the doors: one behind her with his rifle at her back, one leading. */
    if (time >= 4.05f && time < 7.18f)
    {
        float group_move = smoothstep01((time - 4.05f) / 2.95f);
        bool walking = group_move < 1.0f;
        float escort_one_x = lerpf(496.0f, 600.0f, group_move);
        float hostage_x = lerpf(552.0f, 656.0f, group_move);
        float escort_two_x = lerpf(608.0f, 712.0f, group_move);
        const BbcSprite *rear = &AGENT_AIM_WALK;
        if (!walking || (stride_quarter(escort_one_x, CAST_WALK_QUARTER) & 1))
            rear = &SPR_AGENT_AIM;
        stand(r, rear, escort_one_x, ground, 1, BBC_AGENT_PAINT);
        stand(r, walking ? ellen_walk_frame(hostage_x, true)
                         : &ELLEN_BOUND_STAND,
              hostage_x, ground, 1, NULL);
        stand(r, walking ? crew_walk_frame(escort_two_x) : &SPR_AGENT_STAND,
              escort_two_x, ground, 1, BBC_AGENT_PAINT);
    }

    /* In front of both cars, like the men he is after. */
    draw_arrival_chuck(r, time, ground);

    const RainLight rain_lights[] = {
        {tower_x + 180.0f, tower_x + 340.0f, 340.0f, BBC_YELLOW}};
    draw_rain(r, time, (float)win_w, 20.0f, (float)win_h - 20.0f, rain_lights,
              1, 120u, BBC_BLUE, BBC_CYAN);
    render_cinematic_ui(r, time, win_w, win_h, suv_x, pad);
    draw_letterbox(r, win_w, win_h, 20.0f);
    scene_end();
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
 */

/* Where each actor stands — the centre a figure is stood on, or a car's left
 * end — so the timings below and the staging cannot drift apart. */
#define KERB_GROUND 437.0f
#define KERB_CAR_X 10.0f
#define KERB_CHUCK_WAIT_X 228.0f
/* Chuck stops a clear distance short of the SUV's tail. Close enough to be in
   the same shot as the vehicle, far enough that the frame still reads as a
   distance he is not going to cover in time. */
#define KERB_CHUCK_CHASE_X 336.0f
#define KERB_CHUCK_RETURN_X 188.0f
#define KERB_ELLEN_FROM_X 392.0f
#define KERB_ELLEN_TAKEN_X 652.0f
#define KERB_SUV_STOP_X 420.0f
/* The SUV's near door: the crew get out of it and she goes back into it, so
   both journeys are measured from here. */
#define KERB_SUV_DOOR_X (KERB_SUV_STOP_X + 72.0f)
/* The coffee window they stopped at. */
#define KERB_SHOP_X 168.0f
#define KERB_SHOP_W 124.0f
#define KERB_PARADE_TOP 316.0f

/* When things happen to him. The first three are the moments he reacts to —
   the brakes, the men getting out, her scream — and the scream is also the cue
   the sound and the dropped cup are timed to, so it is named once. */
#define KERB_NOTICE_TIME 3.66f
#define KERB_FRIGHT_TIME 4.10f
#define KERB_SCREAM_TIME 5.10f
#define KERB_RUN_OUT_TIME 5.60f
#define KERB_AIM_TIME 7.60f
#define KERB_LOWER_TIME 8.75f
/* He goes for the car the moment the SUV moves off, and he runs it. */
#define KERB_RUN_BACK_TIME 9.30f
#define KERB_AT_CAR_TIME 10.55f

/* The coffee she was carrying, dropped at the moment of contact and left in
   frame for the rest of the scene: a fall, one bounce, a short roll, and the
   spill spreading on the wet stone as a puddle of brown — red and yellow on a
   checkerboard, which is the only brown there is. */
static void draw_coffee_cup(SDL_Renderer *r, float time)
{
    float age = time - KERB_SCREAM_TIME;
    if (age < 0.0f)
        return;

    const float start_x = KERB_ELLEN_TAKEN_X + 32.0f;
    const float start_y = KERB_GROUND - 36.0f;
    const float floor_y = KERB_GROUND - 12.0f;
    float x = start_x + age * 27.0f;
    float y = start_y + 470.0f * age * age;
    float bounce = age - 0.22f;
    if (y >= floor_y)
    {
        if (bounce > 0.0f && bounce < 0.30f)
            y = floor_y - sinf(bounce / 0.30f * 3.14159265f) * 10.0f;
        else
            y = floor_y;
    }
    if (age > 0.95f)
        x = start_x + 0.95f * 27.0f +
            (1.0f - expf(-(age - 0.95f) * 3.4f)) * 9.0f;

    if (age > 0.30f)
    {
        float spill = 8.0f + clamp01((age - 0.30f) / 1.6f) * 36.0f;
        bbc_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK,
                   start_x + 12.0f - spill * 0.5f, KERB_GROUND - 2.0f, spill,
                   4.0f);
    }
    bbc_block(r, BBC_WHITE, x, y, 2, 6);
    bbc_block(r, BBC_RED, x, y, 2, 2);
}

/*
 * The street this happens on, and Kessler Tower standing over the end of it.
 *
 * Two rules the shot is built to. The frame is a canyon, not a backdrop with
 * figures in front of it: the near terrace runs from well up the frame down to
 * the shopfronts, so there is no band of empty sky between the skyline and the
 * pavement for the eye to fall into. And the tower clears the terrace by a
 * long way, because it is the place the SUV drives to and the player has to
 * recognise it three minutes later from the pavement outside its own doors:
 * the same blue, the same grid of glass, the same cyan arris and red beacon.
 */
static void render_kerb_backdrop(SDL_Renderer *r, float time, int win_w)
{
    static const float parapet[6] = {196.0f, 164.0f, 228.0f, 180.0f,
                                     244.0f, 208.0f};
    const float parade_top = KERB_PARADE_TOP;
    const float w = (float)win_w;

    draw_stars(r, time, w, 22.0f, 200.0f, 31u, 60u);
    draw_far_city(r, time, w, parade_top, 977u, 9, 110.0f, 190.0f);

    /* Kessler Tower, five blocks up, behind everything else on the street:
     * further off than the terrace, so a step of dither into the night, but
     * the same building. */
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_DENSE, 594.0f, 52.0f, 136.0f,
               parade_top - 52.0f);
    bbc_fill(r, BBC_CYAN, 594.0f, 52.0f, 4.0f, parade_top - 52.0f);
    bbc_fill(r, BBC_CYAN, 594.0f, 52.0f, 136.0f, 2.0f);
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_DENSE, 614.0f, 42.0f, 60.0f,
               10.0f);
    bbc_fill(r, BBC_BLUE, 654.0f, 36.0f, 4.0f, 6.0f);
    bbc_fill(r, beacon_on(time) ? BBC_RED : BBC_BLACK, 650.0f, 32.0f, 12.0f,
             4.0f);
    for (int row = 0; row < 12; ++row)
    {
        float fy = 66.0f + (float)row * 20.0f;
        for (int col = 0; col < 5; ++col)
        {
            unsigned h = scene_hash((unsigned)(row * 53 + col * 11 + 401));
            float wx = 606.0f + (float)col * 24.0f;
            BbcColour glass = BBC_BLACK;
            if ((h % 5u) == 0u)
                glass = (h & 8u) ? BBC_YELLOW : BBC_CYAN;
            bbc_fill(r, glass, wx, fy, 12.0f, 6.0f);
        }
    }

    /* The near terrace: walk-ups over the shops, solid blue, windows lit and
     * dark, the hardware on their roofs. */
    for (int i = 0; i < 6; ++i)
    {
        float x = (float)i * 138.0f - 22.0f;
        float top = parapet[i];
        unsigned bh = scene_hash((unsigned)i * 7717u + 29u);
        bbc_fill(r, BBC_BLUE, x, top, 132.0f, parade_top - top);
        bbc_fill(r, BBC_CYAN, x, top, 132.0f, 2.0f);
        bbc_fill(r, BBC_CYAN, x, top, 4.0f, parade_top - top);
        /* What stands on the roof: a chimney stack, a tank, or a mast. */
        switch (bh % 3u)
        {
        case 0:
            bbc_fill(r, BBC_BLUE, x + 24.0f + (float)(bh % 40u), top - 12.0f,
                     12.0f, 12.0f);
            bbc_fill(r, BBC_CYAN, x + 24.0f + (float)(bh % 40u), top - 12.0f,
                     12.0f, 2.0f);
            break;
        case 1:
            bbc_fill(r, BBC_BLUE, x + 80.0f, top - 6.0f, 4.0f, 6.0f);
            bbc_fill(r, BBC_BLUE, x + 92.0f, top - 6.0f, 4.0f, 6.0f);
            bbc_fill(r, BBC_BLUE, x + 78.0f, top - 20.0f, 20.0f, 14.0f);
            bbc_fill(r, BBC_CYAN, x + 78.0f, top - 20.0f, 4.0f, 14.0f);
            break;
        default:
            bbc_fill(r, BBC_BLUE, x + 40.0f, top - 24.0f, 4.0f, 24.0f);
            bbc_fill(r, BBC_BLUE, x + 32.0f, top - 18.0f, 20.0f, 2.0f);
            bbc_fill(r, BBC_BLUE, x + 36.0f, top - 12.0f, 12.0f, 2.0f);
            break;
        }
        for (int row = 0; row * 28 < (int)(parade_top - top) - 30; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                unsigned wh = scene_hash(bh + (unsigned)(row * 37 + col * 11));
                float wx = x + 16.0f + (float)col * 30.0f;
                float wy = top + 12.0f + (float)row * 28.0f;
                bbc_fill(r, BBC_BLACK, wx, wy, 16.0f, 18.0f);
                if ((wh % 5u) == 0u)
                {
                    BbcColour glass = (wh & 8u) ? BBC_YELLOW : BBC_CYAN;
                    bbc_fill(r, glass, wx + 4.0f, wy + 2.0f, 8.0f, 14.0f);
                    /* A curtain drawn to one side of some of them. */
                    if ((wh & 16u) != 0u)
                        bbc_fill(r, BBC_RED, wx + 4.0f, wy + 2.0f, 4.0f, 14.0f);
                }
                else
                {
                    /* Dark sash windows still show their glazing bar. */
                    bbc_fill(r, BBC_BLUE, wx + 4.0f, wy + 8.0f, 8.0f, 2.0f);
                }
            }
        }
    }

    /*
     * The shopfronts under them. Their fascia is one unbroken line, which is
     * what makes a parade of shops; under it, shutters down on everything but
     * the one place still open. The bays are laid out against the staging and
     * not the other way round: a blue shutter behind his cyan car, and dark
     * glass behind the blue SUV and wherever somebody in blue has to be seen
     * standing.
     */
    static const struct
    {
        float x;
        float w;
        bool shutter; /* or dark glass */
    } bays[] = {
        {-8.0f, 168.0f, true},   {300.0f, 112.0f, false},
        {420.0f, 176.0f, false}, {604.0f, 160.0f, false},
        {772.0f, 40.0f, true},
    };
    const float bay_top = parade_top + 10.0f;
    const float bay_h = KERB_GROUND - 5.0f - bay_top;
    bbc_fill(r, BBC_BLACK, 0.0f, parade_top, w, KERB_GROUND - parade_top);
    bbc_fill(r, BBC_CYAN, 0.0f, parade_top, w, 2.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, parade_top + 2.0f, w, 8.0f);
    for (int i = 0; i < (int)SDL_arraysize(bays); ++i)
    {
        float x = bays[i].x;
        float bw = bays[i].w;
        bbc_fill(r, BBC_BLUE, x - 8.0f, bay_top, 8.0f, KERB_GROUND - bay_top);
        bbc_fill(r, BBC_BLUE, x + bw, bay_top, 8.0f, KERB_GROUND - bay_top);
        if (bays[i].shutter)
        {
            bbc_fill(r, BBC_BLUE, x, bay_top + 4.0f, bw, bay_h - 4.0f);
            for (float slat = bay_top + 10.0f; slat < KERB_GROUND - 6.0f;
                 slat += 8.0f)
                bbc_fill(r, BBC_BLACK, x, slat, bw, 2.0f);
            bbc_fill(r, BBC_CYAN, x, bay_top + 4.0f, bw, 2.0f);
        }
        else
        {
            bbc_fill(r, BBC_BLUE, x, bay_top + 4.0f, bw, 4.0f);
            bbc_fill(r, BBC_WHITE, x, KERB_GROUND - 8.0f, bw, 2.0f);
            for (int g = 0; g < 4; ++g)
                bbc_plot(r, BBC_WHITE, x + bw - 16.0f - (float)g * 4.0f,
                         bay_top + 14.0f + (float)g * 4.0f);
            bbc_fill(r, BBC_BLUE, x + bw * 0.5f - 2.0f, bay_top + 8.0f, 4.0f,
                     bay_h - 10.0f);
        }
    }

    /* The window they stopped at: the one warm thing in the frame, and the
       reason she is carrying a cup at all. */
    const float sx = KERB_SHOP_X;
    const float sw = KERB_SHOP_W;
    const float glass_top = parade_top + 26.0f;
    const float glass_h = KERB_GROUND - glass_top;
    bbc_fill(r, BBC_BLACK, sx, parade_top + 10.0f, sw, 16.0f);
    text_centered(r, sx + sw * 0.5f, parade_top + 13.0f, 1, BBC_YELLOW,
                  "COFFEE");
    bbc_fill(r, BBC_WHITE, sx, glass_top - 2.0f, sw, 2.0f);
    bbc_fill(r, BBC_YELLOW, sx + 4.0f, glass_top, sw - 8.0f, glass_h);
    /* Inside: shelves on the back wall, three pendant lamps, the counter in
     * wood, the menu board chalk on black in the left pane. */
    for (float shy = glass_top + 14.0f; shy < glass_top + 44.0f; shy += 10.0f)
        bbc_fill(r, BBC_RED, sx + 64.0f, shy, 52.0f, 2.0f);
    for (int i = 0; i < 3; ++i)
    {
        float px = sx + 20.0f + (float)i * 36.0f;
        bbc_fill(r, BBC_RED, px + 2.0f, glass_top, 4.0f, 6.0f);
        bbc_fill(r, BBC_WHITE, px - 2.0f, glass_top + 6.0f, 12.0f, 2.0f);
    }
    bbc_fill(r, BBC_WHITE, sx + 4.0f, KERB_GROUND - 38.0f, sw - 8.0f, 2.0f);
    bbc_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, sx + 4.0f,
               KERB_GROUND - 36.0f, sw - 8.0f, 36.0f);
    bbc_fill(r, BBC_BLACK, sx + 12.0f, glass_top + 14.0f, 32.0f, 22.0f);
    for (float cy = glass_top + 18.0f; cy < glass_top + 34.0f; cy += 4.0f)
        bbc_fill(r, BBC_WHITE, sx + 16.0f, cy, 16.0f + (float)((int)cy % 3) * 4.0f,
                 2.0f);
    /* The glass: a mullion and the street's reflection across the pane. */
    bbc_fill(r, BBC_BLACK, sx + 56.0f, glass_top, 4.0f, glass_h);
    for (int i = 0; i < 7; ++i)
        bbc_plot(r, BBC_WHITE, sx + 100.0f - (float)i * 4.0f,
                 glass_top + 6.0f + (float)i * 4.0f);
    /* Its light on the pavement in front. */
    for (float py = 432.0f; py < STREET_KERB_Y; py += 2.0f)
        bbc_stipple(r, BBC_YELLOW, BBC_DITHER_CHECK, sx - 12.0f, py, sw + 24.0f,
                    2.0f);

    /* Two street lamps, so the pavement is lit in pools rather than evenly. */
    for (int i = 0; i < 2; ++i)
    {
        float lx = 108.0f + (float)i * 600.0f;
        bbc_fill(r, BBC_BLUE, lx, 252.0f, 4.0f, KERB_GROUND - 252.0f);
        bbc_fill(r, BBC_CYAN, lx, 252.0f, 4.0f, 2.0f);
        bbc_fill(r, BBC_BLUE, lx - 8.0f, 248.0f, 24.0f, 4.0f);
        bbc_fill(r, BBC_YELLOW, lx - 6.0f, 252.0f, 16.0f, 4.0f);
        bbc_fill(r, BBC_WHITE, lx - 2.0f, 254.0f, 8.0f, 2.0f);
        for (float py = 432.0f; py < STREET_KERB_Y; py += 2.0f)
            bbc_stipple(r, BBC_YELLOW, BBC_DITHER_CHECK, lx - 38.0f, py, 80.0f,
                        2.0f);
    }
}

static void render_kerb_ui(SDL_Renderer *r, float time, int win_w, int win_h,
                           float suv_x, const PadHints *pad)
{
    if (time > 0.45f && time < 4.40f)
        draw_caption(r, 35.0f, 38.0f, BBC_CYAN,
                     "00:12 // THREE BLOCKS FROM KESSLER TOWER", time - 0.45f,
                     52.0f, BBC_RED);

    if (time > 5.40f && time < 8.05f)
        draw_caption(r, 35.0f, 38.0f, BBC_RED,
                     "NO WORD // NO RANSOM // THEY CAME FOR HER", time - 5.40f,
                     142.0f, BBC_RED);

    if (time > 7.95f && time < 8.95f)
    {
        draw_target_brackets(r, suv_x - 8.0f, KERB_GROUND - SUV_H - 8.0f,
                             SUV_W + 16.0f, SUV_H + 14.0f);
        typed_text_backed(r, suv_x - 4.0f, KERB_GROUND - SUV_H - 22.0f,
                          BBC_RED, "NO SHOT // SHE IS IN THE DOOR",
                          time - 7.95f);
    }

    if (time > 9.60f && time < 12.90f)
        draw_caption(r, 35.0f, 38.0f, BBC_YELLOW,
                     "CHUCK ROSS // FORTY SECONDS BEHIND", time - 9.60f, 52.0f,
                     BBC_RED);

    if (time > 0.9f && time < 12.35f)
        draw_skip_hint(r, win_w, win_h, pad);
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

/* The game's own "!" over a head that has just seen something, in the colour
   it warns with, popped up a row and then held. `bottom` is where its dot
   ends. */
static void draw_alarm_mark(SDL_Renderer *r, float cx, float bottom,
                            float age, BbcColour c)
{
    float rise = age < 0.10f ? 0.0f : 4.0f;
    float x = cx - 4.0f;
    float y = bottom - 20.0f - rise;
    bbc_fill(r, BBC_BLACK, x - 4.0f, y - 2.0f, 16.0f, 24.0f);
    bbc_fill(r, c, x, y, 8.0f, 12.0f);
    bbc_fill(r, c, x, y + 16.0f, 8.0f, 4.0f);
}

/*
 * Chuck at the kerb, and what it does to him — the one beat of the campaign in
 * which he is given nothing to do but watch, so what the moment does to him
 * has to be on him, in the order it would happen to anybody: the brakes and he
 * comes up straight; the rifles and he starts back from them with his hands up
 * under the red mark the game warns with everywhere else; her scream and the
 * fright turns over into the other thing, down into his knees, and he goes.
 */
static void draw_kerb_chuck(SDL_Renderer *r, float time, float ground)
{
    const BbcSprite *s = &SPR_CHUCK_STAND;
    float x = KERB_CHUCK_WAIT_X;
    float pace = 0.0f;
    int dir = 1;

    if (time >= KERB_AIM_TIME && time < KERB_RUN_BACK_TIME)
    {
        x = KERB_CHUCK_CHASE_X;
        s = time < KERB_LOWER_TIME ? &SPR_CHUCK_AIM : &SPR_CHUCK_AIM_DOWN;
    }
    else if (kerb_chuck_run(time, &x, &dir, &pace))
    {
        if (pace > 0.15f)
            s = chuck_run_frame(x * (float)dir);
        else
            s = dir > 0 ? &CHUCK_LUNGE : &SPR_CHUCK_STAND;
    }
    else if (time < KERB_FRIGHT_TIME)
    {
        s = &SPR_CHUCK_STAND;
    }
    else if (time < KERB_SCREAM_TIME)
    {
        s = &CHUCK_FRIGHT;
    }
    else if (time < KERB_RUN_OUT_TIME)
    {
        s = &CHUCK_LUNGE;
    }
    else
    {
        x = KERB_CHUCK_RETURN_X;
        dir = -1;
    }
    stand(r, s, x, ground, dir, NULL);

    float mark_age = time - KERB_FRIGHT_TIME;
    if (mark_age >= 0.0f && mark_age < 0.75f)
        draw_alarm_mark(r, x, ground - CAST_H - 4.0f, mark_age, BBC_RED);
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
                               const AbductionCutscene *cutscene, int win_w,
                               int win_h, const PadHints *pad)
{
    const float time = cutscene->time;
    const float ground = KERB_GROUND;

    float fade_in = 1.0f - smoothstep01(time / 0.68f);
    float fade_out = smoothstep01((time - 12.55f) / 1.05f);
    scene_begin(fade_steps(fmaxf(fade_in, fade_out)));

    render_kerb_backdrop(r, time, win_w);

    /* The two sodium lamps, the one window still open, the beacon. */
    const WetLight lights[] = {
        {110.0f, 28.0f, BBC_YELLOW, 18},
        {710.0f, 28.0f, BBC_YELLOW, 18},
        {KERB_SHOP_X + KERB_SHOP_W * 0.5f, 72.0f, BBC_YELLOW, 14},
        {656.0f, 8.0f, BBC_RED, 5},
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
        suv_x = -260.0f;
        suv_moving = false;
        suv_lights = false;
    }
    else if (time < 3.70f)
    {
        suv_x = lerpf(-260.0f, KERB_SUV_STOP_X,
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
        suv_x = lerpf(KERB_SUV_STOP_X, 1000.0f, away * away);
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
    const BbcSprite *ellen = &SPR_ELLEN_STAND;
    if (time < 4.50f)
    {
        float walk = clamp01((time - 0.70f) / 3.80f);
        ellen_x = lerpf(KERB_ELLEN_FROM_X, KERB_ELLEN_TAKEN_X, walk);
        if (walk > 0.0f && walk < 1.0f)
            ellen = ellen_walk_frame(ellen_x, false);
    }
    else if (ellen_taken)
    {
        /* Walked back down the pavement to the door she came level with.
           She never reaches Chuck, and the two of them are never in the
           same frame facing each other — that is the scene. */
        float taken = smoothstep01((time - 5.20f) / 2.15f);
        ellen_x = lerpf(KERB_ELLEN_TAKEN_X, KERB_SUV_DOOR_X + 8.0f, taken);
        ellen_dir = -1;
        ellen = taken < 1.0f ? ellen_walk_frame(-ellen_x, false)
                             : &SPR_ELLEN_STAND;
    }
    else
    {
        /* Turned on them, and the scream. */
        ellen_dir = -1;
    }

    /*
     * Who is in front of what. The camera is across the road, so the kerb lane
     * the two cars are in is nearer to it than the pavement, and everybody on
     * the pavement — Chuck at the coffee window, Ellen walking up it, the cup
     * she drops — is behind the cars. The two who get out use the door on this
     * side, the one that is drawn open, so they are in the road in front of it,
     * and so is she once they have her.
     */
    draw_coffee_cup(r, time);
    draw_kerb_chuck(r, time, ground);

    if (ellen_here && !ellen_taken)
    {
        stand(r, ellen, ellen_x, ground, ellen_dir, NULL);
        if (time < KERB_SCREAM_TIME)
        {
            /* The cup in her hand. */
            float hx, hy;
            sprite_cell(ellen, ellen_x, ground, ellen_dir, CAST_SCALE, 6, 7,
                        &hx, &hy);
            bbc_block(r, BBC_WHITE, hx + 4.0f, hy - 2.0f, 2, 6);
            bbc_block(r, BBC_RED, hx + 4.0f, hy - 2.0f, 2, 2);
        }
    }

    /* Chuck's car never moves in this scene. It is parked, locked and empty:
       he is out of it, which is the whole reason he cannot simply drive. */
    draw_sedan(r, KERB_CAR_X, ground, false, false);
    draw_suv(r, suv_x, ground, suv_moving, suv_lights, suv_door_open);

    if (ellen_here && ellen_taken)
        stand(r, ellen, ellen_x, ground, ellen_dir, NULL);

    /* The two who get out of it. They walk up to her, then walk her back. */
    if (time >= 4.05f && time < 7.35f)
    {
        float out = smoothstep01((time - 4.05f) / 1.25f);
        float back = smoothstep01((time - 5.20f) / 2.15f);
        float near_x = lerpf(KERB_SUV_DOOR_X, KERB_ELLEN_TAKEN_X - 52.0f, out);
        float far_x = lerpf(KERB_SUV_DOOR_X + 40.0f,
                            KERB_ELLEN_TAKEN_X + 52.0f, out);
        bool walking = out > 0.0f && out < 1.0f;
        if (time >= 5.20f)
        {
            /* They end either side of the car rather than in front of its
             * blue flank, so each of them is seen against the dark glass. */
            near_x = lerpf(KERB_ELLEN_TAKEN_X - 52.0f,
                           KERB_SUV_STOP_X - 20.0f, back);
            far_x = lerpf(KERB_ELLEN_TAKEN_X + 52.0f,
                          KERB_SUV_STOP_X + SUV_W + 20.0f, back);
            walking = back < 1.0f;
        }
        int crew_dir = time < 5.20f ? 1 : -1;
        const BbcSprite *near_s =
            walking ? crew_walk_frame(near_x * (float)crew_dir)
                    : &SPR_AGENT_STAND;
        const BbcSprite *far_s = &SPR_AGENT_AIM;
        if (walking &&
            !(stride_quarter(far_x * (float)crew_dir, CAST_WALK_QUARTER) & 1))
            far_s = &AGENT_AIM_WALK;
        stand(r, near_s, near_x, ground, crew_dir, BBC_AGENT_PAINT);
        stand(r, far_s, far_x, ground, crew_dir, BBC_AGENT_PAINT);
    }

    const RainLight rain_lights[] = {
        {72.0f, 148.0f, 252.0f, BBC_YELLOW},
        {672.0f, 748.0f, 252.0f, BBC_YELLOW},
        {KERB_SHOP_X, KERB_SHOP_X + KERB_SHOP_W, KERB_PARADE_TOP + 26.0f,
         BBC_YELLOW},
    };
    draw_rain(r, time, (float)win_w, 20.0f, (float)win_h - 20.0f, rain_lights,
              (int)SDL_arraysize(rain_lights), 120u, BBC_BLUE, BBC_CYAN);
    render_kerb_ui(r, time, win_w, win_h, suv_x, pad);
    draw_letterbox(r, win_w, win_h, 20.0f);
    scene_end();
}

/* ---- The report between sectors --------------------------------------- */

void level_transition_init(LevelTransition *transition, int completed_level,
                           int next_level, float elapsed_seconds,
                           int level_score, int hostiles_neutralized,
                           int deaths, int time_bonus, int clean_bonus,
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
     * is. One sheet to an interior, none on a climb. */
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

/*
 * The report itself: a black panel across the top of the frame ruled off in
 * cyan, the heading typed in, the intel line under it, and the numbers filling
 * in a field at a time the way an ops screen fills its cells. The layout is the
 * one the suite measures — the intel line between `INTEL_TEXT_LEFT` and the
 * divider at `INTEL_TEXT_RIGHT`, every cell eight pixels a character — and
 * only the drawing of it has changed.
 *
 * The colours are the rationed ones: cyan for a label, yellow for a value,
 * green for what a field paid, magenta for the docket, which is the story's.
 */
static void render_transition_report(SDL_Renderer *r,
                                     const LevelTransition *transition,
                                     int win_w)
{
    const float time = transition->time;
    char buffer[64];

    bbc_fill(r, BBC_BLACK, 0.0f, 18.0f, (float)win_w, 134.0f);
    bbc_fill(r, BBC_CYAN, 0.0f, 22.0f, (float)win_w, 2.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, 26.0f, (float)win_w, 2.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, 144.0f, (float)win_w, 2.0f);
    bbc_fill(r, BBC_CYAN, 0.0f, 148.0f, (float)win_w, 2.0f);

    typed_text(r, 27.0f, 34.0f, 2, BBC_WHITE, "ONE FLOOR BEHIND",
               (time - 0.12f) * 0.5f);
    float rule = 72.0f * smoothstep01((time - 0.12f) / 0.58f);
    if (rule >= 4.0f)
        bbc_fill(r, BBC_CYAN, 27.0f, 60.0f, rule, 2.0f);

    /* Set from INTEL_TEXT_LEFT, and stopped by the divider at
     * INTEL_TEXT_RIGHT drawn below. Both come out of [intel.h](intel.h), which
     * is where the suite measures the table against them, so the line and the
     * column it has to live in cannot drift apart. */
    const char *intel = intel_line(transition->completed_level);
    if (intel != NULL && time >= 0.42f)
    {
        bbc_fill(r, BBC_RED, 28.0f, 68.0f, 4.0f, 10.0f);
        typed_text(r, INTEL_TEXT_LEFT, 69.0f, 1, BBC_WHITE, intel,
                   time - 0.42f);
    }

    /* What the two earned fields paid, printed under the field that earned it,
     * in green rather than the value's yellow: the eye should find the clock
     * first and then the credit beside it. See `campaign_award_sector_bonus`. */
    const BbcColour label = BBC_CYAN;
    const BbcColour value = BBC_YELLOW;
    const BbcColour credit = BBC_GREEN;

    if (time >= 0.60f)
    {
        int elapsed = (int)transition->elapsed_seconds;
        text(r, 27.0f, 88.0f, 1, label, "TIME");
        SDL_snprintf(buffer, sizeof(buffer), "%02d:%02d", elapsed / 60,
                     elapsed % 60);
        text(r, 27.0f, 106.0f, 1, value, buffer);
    }

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
    if (time >= 0.70f)
    {
        text(r, 75.0f, 88.0f, 1, label, "BEST");
        float record = transition->best_is_new ? transition->elapsed_seconds
                                               : transition->best_seconds;
        if (record > 0.0f)
        {
            int best_total = (int)record;
            SDL_snprintf(buffer, sizeof(buffer), "%02d:%02d", best_total / 60,
                         best_total % 60);
            text(r, 75.0f, 106.0f, 1,
                 transition->best_is_new ? credit : label, buffer);
        }
        else
        {
            /* Nothing banked: a blank here would read as the field having
             * failed rather than as there being no record yet. */
            text(r, 75.0f, 106.0f, 1, label, "--:--");
        }
    }
    /* A floor that ran over its slot on the night clock is told so rather than
     * left with a blank line: the words are what teach the par exists. */
    if (time >= 0.80f)
    {
        if (transition->time_bonus > 0)
        {
            SDL_snprintf(buffer, sizeof(buffer), "+%d", transition->time_bonus);
            text(r, 27.0f, 122.0f, 1, credit, buffer);
        }
        else
        {
            text(r, 27.0f, 122.0f, 1, label, "OVER PAR");
        }
    }

    bbc_fill(r, BBC_BLUE, 128.0f, 86.0f, 4.0f, 42.0f);
    if (time >= 0.90f)
    {
        text(r, 151.0f, 88.0f, 1, label, "SCORE");
        SDL_snprintf(buffer, sizeof(buffer), "+%06d", transition->level_score);
        text(r, 151.0f, 106.0f, 1, value, buffer);
        /* Under the score, because a sheet is worth `EVIDENCE_SCORE` and this
         * is the row that says where points came from. The whole cell is
         * twelve cells of 8px from x=151, so it stops well short of the
         * divider at 282. */
        SDL_snprintf(buffer, sizeof(buffer), "DOCKET %02d/%02d",
                     transition->docket_sheets, transition->docket_total);
        text(r, 151.0f, 122.0f, 1, BBC_MAGENTA, buffer);
    }

    bbc_fill(r, BBC_BLUE, 280.0f, 86.0f, 4.0f, 42.0f);
    if (time >= 1.00f)
    {
        text(r, 307.0f, 88.0f, 1, label, "HOSTILES");
        SDL_snprintf(buffer, sizeof(buffer), "%02d",
                     transition->hostiles_neutralized);
        text(r, 307.0f, 106.0f, 1, value, buffer);
    }

    /* Deaths are reported neutrally — a number to beat next run, never a
     * grade. A clean sector earns the one word of praise. */
    bbc_fill(r, BBC_BLUE, 412.0f, 86.0f, 4.0f, 42.0f);
    if (time >= 1.10f)
    {
        text(r, 434.0f, 88.0f, 1, label, "DEATHS");
        if (transition->deaths == 0)
            SDL_snprintf(buffer, sizeof(buffer), "CLEAN");
        else
            SDL_snprintf(buffer, sizeof(buffer), "%02d", transition->deaths);
        text(r, 434.0f, 106.0f, 1, value, buffer);
        if (transition->clean_bonus > 0)
        {
            SDL_snprintf(buffer, sizeof(buffer), "+%d",
                         transition->clean_bonus);
            text(r, 434.0f, 122.0f, 1, credit, buffer);
        }
    }

    /* The divider the intel line above is measured against. It is the same
     * constant the test walks the table off, so moving this column moves what
     * a line is allowed to be. */
    bbc_fill(r, BBC_BLUE, INTEL_TEXT_RIGHT, 36.0f, 4.0f, 92.0f);
    if (time >= 1.20f)
    {
        text(r, 558.0f, 45.0f, 1, label, "TRAIL LEADS TO");
        SDL_snprintf(buffer, sizeof(buffer), "SECTOR %02d",
                     transition->next_level + 1);
        text(r, 558.0f, 69.0f, 2, BBC_WHITE, buffer);
    }
}

static float transition_door_open(float time)
{
    float opening = smoothstep01((time - 3.72f) / 0.42f);
    float closing = smoothstep01((time - 8.10f) / 0.42f);
    return opening * (1.0f - closing);
}

/*
 * The service corridor the report plays over, and it is a room the way every
 * room in the game is a room: black, with its own lines drawn on it in blue —
 * the cable tray and the duct along the ceiling, the pilasters, a dado rail —
 * the strip lamps a flat white bar each with their light a pool of yellow on
 * the floor, and the floor itself a lit lip and polished boards that hold
 * every lamp once more.
 */
static void render_transition_corridor(SDL_Renderer *r, float time, int win_w,
                                       int win_h, float door_x, float ground_y,
                                       int next_sector)
{
    const float w = (float)win_w;
    const float dado = ground_y - 76.0f;
    float open = transition_door_open(time);

    bbc_fill(r, BBC_BLACK, 0.0f, 152.0f, w, (float)win_h - 152.0f);

    /* The ceiling void: a cable tray on its hangers and a run of duct. */
    bbc_fill(r, BBC_BLUE, 0.0f, 158.0f, w, 2.0f);
    for (float hx = 8.0f; hx < w; hx += 52.0f)
        bbc_fill(r, BBC_BLUE, hx, 152.0f, 4.0f, 6.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, 166.0f, w, 2.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, 176.0f, w, 2.0f);
    for (float sx = 28.0f; sx < w; sx += 68.0f)
        bbc_fill(r, BBC_BLUE, sx, 166.0f, 4.0f, 12.0f);

    /* Pilasters, a grille or a junction box in each bay, the dado rail. */
    for (int i = 0; i < 8; ++i)
    {
        float x = 16.0f + (float)i * 102.0f;
        bbc_fill(r, BBC_BLUE, x, 186.0f, 4.0f, ground_y - 186.0f);
        if ((i & 1) == 0)
        {
            for (float gy = 214.0f; gy < 240.0f; gy += 4.0f)
                bbc_fill(r, BBC_BLUE, x + 20.0f, gy, 56.0f, 2.0f);
        }
        else
        {
            bbc_frame(r, BBC_BLUE, x + 44.0f, 262.0f, 16.0f, 20.0f);
            bbc_fill(r, BBC_BLUE, x + 50.0f, 282.0f, 4.0f, dado - 282.0f);
        }
    }
    bbc_fill(r, BBC_CYAN, 0.0f, dado, w, 2.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, dado + 4.0f, w, 2.0f);
    bbc_fill(r, BBC_BLUE, 0.0f, ground_y - 10.0f, w, 8.0f);

    /* The floor: the lit lip a foot lands on, then the polished boards. */
    bbc_fill(r, BBC_WHITE, 0.0f, ground_y - 2.0f, w, 2.0f);
    bbc_fill(r, BBC_CYAN, 0.0f, ground_y, w, 2.0f);
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_ROWS, 0.0f, ground_y + 2.0f,
               w, (float)win_h - ground_y - 2.0f);
    for (float jy = ground_y + 12.0f; jy < (float)win_h; jy += 14.0f)
        bbc_fill(r, BBC_BLACK, 0.0f, jy, w, 2.0f);

    /* The strip lamps, their pools on the boards, and the lamp once more in
     * the polish. */
    for (int i = 0; i < 4; ++i)
    {
        float lx = 96.0f + (float)i * 205.0f;
        bbc_fill(r, BBC_WHITE, lx - 16.0f, 184.0f, 32.0f, 2.0f);
        bbc_fill(r, BBC_YELLOW, lx - 12.0f, 186.0f, 24.0f, 2.0f);
        for (float py = ground_y + 2.0f; py < ground_y + 10.0f; py += 2.0f)
            bbc_stipple(r, BBC_YELLOW, BBC_DITHER_CHECK, lx - 44.0f, py, 88.0f,
                        2.0f);
        for (int row = 0; row < 7; ++row)
        {
            unsigned h = scene_hash((unsigned)(i * 31 + row * 7));
            if ((h & 3u) == 0u)
                continue;
            float width = 24.0f - (float)row * 2.0f + (float)(h % 3u) * 4.0f;
            bbc_fill(r, row < 4 ? BBC_YELLOW : BBC_RED,
                     lx - width * 0.5f + (float)((int)(h >> 4) % 3 - 1) * 4.0f,
                     ground_y + 12.0f + (float)row * 6.0f, width, 2.0f);
        }
    }

    /* The lift: a white frame, its car behind lit along the ceiling, and the
     * two steel leaves — grey, which is black and white on a checkerboard. */
    const float top = TRANSITION_DOOR_TOP;
    const float inner = TRANSITION_DOOR_INNER_TOP;
    bbc_fill(r, BBC_BLACK, door_x, inner, 90.0f, ground_y - inner);
    if (open > 0.0f)
    {
        bbc_fill(r, BBC_YELLOW, door_x + 4.0f, inner + 2.0f, 82.0f, 2.0f);
        bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_SPARSE, door_x + 4.0f,
                   inner + 4.0f, 82.0f, ground_y - inner - 4.0f);
        bbc_fill(r, BBC_BLUE, door_x + 8.0f, ground_y - 40.0f, 74.0f, 2.0f);
    }
    float leaf = 45.0f * (1.0f - open);
    if (leaf >= 4.0f)
    {
        bbc_dither(r, BBC_BLACK, BBC_WHITE, BBC_DITHER_CHECK, door_x, inner,
                   leaf, ground_y - inner);
        bbc_dither(r, BBC_BLACK, BBC_WHITE, BBC_DITHER_CHECK,
                   door_x + 90.0f - leaf, inner, leaf, ground_y - inner);
        bbc_fill(r, BBC_WHITE, door_x + leaf - 4.0f, inner, 4.0f,
                 ground_y - inner);
        bbc_fill(r, BBC_WHITE, door_x + 90.0f - leaf, inner, 4.0f,
                 ground_y - inner);
    }
    bbc_fill(r, BBC_WHITE, door_x - 12.0f, top, 12.0f, ground_y - top);
    bbc_fill(r, BBC_CYAN, door_x - 8.0f, inner, 4.0f, ground_y - inner);

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
    bbc_fill(r, BBC_BLUE, door_x + 28.0f, top - 22.0f, 32.0f, 18.0f);
    bbc_fill(r, BBC_BLACK, door_x + 32.0f, top - 20.0f, 24.0f, 14.0f);
    char plate[8];
    SDL_snprintf(plate, sizeof(plate), "%02d", next_sector);
    text(r, door_x + 37.0f, top - 17.0f, 1, BBC_YELLOW, plate);

    /* The fire point and the exit sign on the corridor wall: the two things
     * every service corridor has, in the two colours that mean stop and go. */
    bbc_fill(r, BBC_RED, 340.0f, ground_y - 112.0f, 16.0f, 20.0f);
    bbc_fill(r, BBC_WHITE, 344.0f, ground_y - 106.0f, 8.0f, 8.0f);
    bbc_fill(r, BBC_RED, 346.0f, ground_y - 104.0f, 4.0f, 4.0f);
    bbc_fill(r, BBC_GREEN, door_x - 4.0f, 196.0f, 96.0f, 16.0f);
    text(r, door_x + 28.0f, 200.0f, 1, BBC_WHITE, "EXIT");
    bbc_fill(r, BBC_WHITE, door_x + 8.0f, 202.0f, 12.0f, 4.0f);
    bbc_fill(r, BBC_WHITE, door_x + 4.0f, 200.0f, 4.0f, 8.0f);

    /* The call panel on the wall beside the doors. */
    bbc_fill(r, BBC_BLUE, door_x - 32.0f, ground_y - 62.0f, 12.0f, 18.0f);
    bbc_fill(r, BBC_BLACK, door_x - 28.0f, ground_y - 58.0f, 4.0f, 4.0f);
    bbc_fill(r, BBC_BLACK, door_x - 28.0f, ground_y - 52.0f, 4.0f, 4.0f);
    bbc_fill(r, open > 0.0f ? BBC_YELLOW : BBC_RED, door_x - 28.0f,
             ground_y - 58.0f, 4.0f, 2.0f);
}

/* The right-hand jamb and the lintel, drawn over the cast so they walk into
 * the car rather than past it, and the dark of the shaft beyond. */
static void draw_transition_door_foreground(SDL_Renderer *r, int win_w,
                                            float door_x, float ground_y)
{
    const float top = TRANSITION_DOOR_TOP;
    bbc_fill(r, BBC_WHITE, door_x + 90.0f, top, 12.0f, ground_y - top);
    bbc_fill(r, BBC_CYAN, door_x + 94.0f, TRANSITION_DOOR_INNER_TOP, 4.0f,
             ground_y - TRANSITION_DOOR_INNER_TOP);
    bbc_fill(r, BBC_WHITE, door_x - 12.0f, top, 114.0f, 10.0f);
    bbc_fill(r, BBC_CYAN, door_x - 12.0f, top + 8.0f, 114.0f, 2.0f);
    bbc_fill(r, BBC_BLACK, door_x + 102.0f, top, (float)win_w - door_x - 102.0f,
             ground_y - top);
}

static void render_transition_action_ui(SDL_Renderer *r, float time,
                                        float hostage_x, float ground_y,
                                        int win_w, int win_h,
                                        const PadHints *pad)
{
    if (time >= 2.18f && time < 3.72f)
    {
        draw_target_brackets(r, hostage_x - 40.0f, ground_y - CAST_H - 10.0f,
                             80.0f, CAST_H + 16.0f);
        draw_caption(r, 35.0f, 192.0f, BBC_RED,
                     "SHE'S TOO CLOSE // KEEP MOVING", time - 2.18f, 142.0f,
                     BBC_RED);
    }

    if (time > 0.75f && time < 8.55f)
        draw_skip_hint(r, win_w, win_h, pad);
}

void level_transition_render(SDL_Renderer *r,
                             const LevelTransition *transition, int win_w,
                             int win_h, const PadHints *pad)
{
    float time = transition->time;
    float ground_y = 482.0f;
    float door_x = (float)win_w - 126.0f;
    float group_progress = clamp01((time - 0.35f) / 4.60f);
    /* The rear man's place; she is a body ahead of him and the leader a body
     * ahead of her. */
    float group_x = lerpf(-60.0f, door_x + 150.0f, group_progress);
    float hostage_x = group_x + 60.0f;
    float lead_x = group_x + 120.0f;

    float fade_in = 1.0f - smoothstep01(time / 0.48f);
    float fade_out = smoothstep01((time - 8.45f) / 0.85f);
    scene_begin(fade_steps(fmaxf(fade_in, fade_out)));

    render_transition_report(r, transition, win_w);
    render_transition_corridor(r, time, win_w, win_h, door_x, ground_y,
                               transition->next_level + 1);

    if (time >= 0.35f)
    {
        /*
         * The captors keep physical control of her as the group moves: the
         * man behind has his rifle on her back, the man in front leads her by
         * a cord from her taped wrists to his belt.
         */
        bool walking = group_progress < 1.0f;
        const BbcSprite *rear = &AGENT_AIM_WALK;
        if (!walking || (stride_quarter(group_x, CAST_WALK_QUARTER) & 1))
            rear = &SPR_AGENT_AIM;
        stand(r, rear, group_x, ground_y, 1, BBC_AGENT_PAINT);
        const BbcSprite *ellen =
            walking ? ellen_walk_frame(hostage_x, true) : &ELLEN_BOUND_STAND;
        stand(r, ellen, hostage_x, ground_y, 1, NULL);
        stand(r, walking ? crew_walk_frame(lead_x) : &SPR_AGENT_STAND, lead_x,
              ground_y, 1, BBC_AGENT_PAINT);
        float wx, wy, bx, by;
        sprite_cell(ellen, hostage_x, ground_y, 1, CAST_SCALE, 6, 8, &wx, &wy);
        sprite_cell(&SPR_AGENT_STAND, lead_x, ground_y, 1, CAST_SCALE, 2, 10,
                    &bx, &by);
        wx += CAST_CELL_W;
        if (bx > wx + 4.0f)
        {
            float mid = (wx + bx) * 0.5f;
            bbc_line(r, BBC_YELLOW, wx, wy + 2.0f, mid, by + 6.0f);
            bbc_line(r, BBC_YELLOW, mid, by + 6.0f, bx, by + 2.0f);
        }
    }

    if (time >= 0.95f && time < 2.20f)
    {
        float u = (time - 0.95f) / 1.25f;
        float x = lerpf(-40.0f, 190.0f, smoothstep01(u));
        stand(r,
              smoothstep_pace(u) > 0.15f ? chuck_run_frame(x)
                                         : &SPR_CHUCK_STAND,
              x, ground_y, 1, NULL);
    }
    else if (time >= 2.20f && time < 3.48f)
    {
        stand(r, &SPR_CHUCK_AIM, 190.0f, ground_y, 1, NULL);
    }
    else if (time >= 3.48f && time < 3.88f)
    {
        stand(r, &SPR_CHUCK_AIM_DOWN, 190.0f, ground_y, 1, NULL);
    }
    else if (time >= 3.88f && time < 8.10f)
    {
        float u = (time - 3.88f) / 4.10f;
        float x = lerpf(190.0f, door_x + 150.0f, smoothstep01(u));
        stand(r,
              smoothstep_pace(u) > 0.15f ? chuck_run_frame(x)
                                         : &SPR_CHUCK_STAND,
              x, ground_y, 1, NULL);
    }

    draw_transition_door_foreground(r, win_w, door_x, ground_y);
    render_transition_action_ui(r, time, hostage_x, ground_y, win_w, win_h,
                                pad);
    draw_letterbox(r, win_w, win_h, 18.0f);
    scene_end();
}

/* ---- The roof ----------------------------------------------------------- */

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

#define OUTRO_GROUND 438.0f
/* Low enough that the second caption line never crosses it. */
#define OUTRO_MOON_X 164.0f
#define OUTRO_MOON_Y 126.0f

/* The moon: a white disc with its seas in cyan, far enough off that nothing
 * the night does moves it. */
static void draw_moon(SDL_Renderer *r, float cx, float cy, float radius)
{
    bbc_disc(r, BBC_WHITE, cx, cy, radius);
    bbc_block(r, BBC_CYAN, cx - radius * 0.45f, cy - radius * 0.35f, 2, 3);
    bbc_block(r, BBC_CYAN, cx + radius * 0.15f, cy + radius * 0.25f, 2, 2);
    bbc_block(r, BBC_CYAN, cx - radius * 0.1f, cy + radius * 0.5f, 1, 2);
}

/*
 * The city behind the roof, forty floors down: the far towers dithered into
 * the night, the near ones solid blue with their windows lit yellow and cyan,
 * and the aviation lights on the masts blinking the way the title screen's
 * do.
 */
static void render_outro_skyline(SDL_Renderer *r, float time, int win_w)
{
    const float base = 408.0f;
    draw_far_city(r, time, (float)win_w, base, 0x524f4fu, 11, 140.0f, 220.0f);
    for (int i = 0; i < 9; ++i)
    {
        unsigned h = scene_hash((unsigned)i * 811u + 7u);
        float x = (float)i * 96.0f - 20.0f + spread(h, 20.0f);
        float th = 60.0f + spread(h >> 8, 80.0f);
        if (i == 6 || i == 7)
            th *= 0.6f;
        draw_near_block(r, time, x, base - th, 64.0f + spread(h >> 12, 18.0f),
                        base, h);
    }
}

/* A flat ellipse of standing water on the roof, its far lip catching the sky
 * and the rain opening a ring in it. */
static void draw_roof_puddle(SDL_Renderer *r, float cx, float cy, float rx,
                             float time, unsigned seed)
{
    fill_ellipse(r, BBC_BLACK, cx, cy, rx, 5.0f);
    bbc_fill(r, BBC_CYAN, cx - rx * 0.6f, cy - 4.0f, rx * 1.2f, 2.0f);
    unsigned h = scene_hash(seed);
    float period = 1.1f + (float)(h % 40u) * 0.02f;
    float age = fmodf(time + (float)(h % 50u) * 0.03f, period) / period;
    float rr = 4.0f + age * rx * 0.6f;
    bbc_plot(r, age < 0.5f ? BBC_WHITE : BBC_BLUE, cx - rr, cy);
    bbc_plot(r, age < 0.5f ? BBC_WHITE : BBC_BLUE, cx + rr, cy);
}

/*
 * The roof the whole night has been climbing toward: the far parapet with the
 * city over it, the membrane the deck is laid in, wet from the drive and
 * holding the moon in a broken column straight down from it, the helipad's
 * ring in yellow, a plant unit on the left, and on the right the stair head
 * with its door lit — the door Chuck comes out of.
 */
static void render_rooftop(SDL_Renderer *r, float time, int win_w, int win_h,
                           float door_open)
{
    const float ground = OUTRO_GROUND;
    const float w = (float)win_w;
    const float moon_x = OUTRO_MOON_X;

    /* The far parapet. */
    bbc_fill(r, BBC_BLUE, 0.0f, ground - 12.0f, w, 12.0f);
    bbc_fill(r, BBC_CYAN, 0.0f, ground - 12.0f, w, 2.0f);
    for (float px = 0.0f; px < w; px += 48.0f)
        bbc_fill(r, BBC_BLACK, px, ground - 8.0f, 4.0f, 8.0f);

    /* The deck: black membrane laid in strips, its laps ruled in blue, so a
     * man down on it is a shape on it. */
    bbc_fill(r, BBC_WHITE, 0.0f, ground, w, 2.0f);
    bbc_fill(r, BBC_BLACK, 0.0f, ground + 2.0f, w, (float)win_h - ground - 2.0f);
    for (float jy = ground + 14.0f; jy < (float)win_h; jy += 20.0f)
        bbc_fill(r, BBC_BLUE, 0.0f, jy, w, 2.0f);
    for (float jx = 40.0f; jx < w; jx += 96.0f)
        bbc_fill(r, BBC_BLUE, jx, ground + 2.0f, 4.0f, 12.0f);
    /* The moon on the wet membrane: short strokes, every other row, because a
     * wet surface is not a mirror. */
    for (float sy = ground + 4.0f; sy < (float)win_h - 30.0f; sy += 4.0f)
    {
        unsigned ripple = scene_hash((unsigned)(int)sy * 2246822519u +
                                     0x4d4f4fu);
        if ((ripple & 3u) == 0u)
            continue;
        float fade = 1.0f - (sy - ground) / ((float)win_h - ground);
        float half = 4.0f + (float)(ripple % 4u) * 4.0f * fade;
        float wobble = (float)((int)((ripple >> 8) % 3u) - 1) * 4.0f;
        bbc_fill(r, fade > 0.5f ? BBC_WHITE : BBC_CYAN,
                 moon_x + wobble - half, sy, half * 2.0f, 2.0f);
    }

    /* The helipad: a yellow ring and the H in it. */
    ring_ellipse(r, BBC_YELLOW, 164.0f, 490.0f, 76.0f, 16.0f);
    bbc_fill(r, BBC_YELLOW, 140.0f, 482.0f, 4.0f, 16.0f);
    bbc_fill(r, BBC_YELLOW, 184.0f, 482.0f, 4.0f, 16.0f);
    bbc_fill(r, BBC_YELLOW, 144.0f, 488.0f, 40.0f, 4.0f);

    draw_roof_puddle(r, 470.0f, 476.0f, 30.0f, time, 3u);
    draw_roof_puddle(r, 300.0f, 514.0f, 44.0f, time, 7u);
    draw_roof_puddle(r, 720.0f, 500.0f, 36.0f, time, 11u);

    /* The plant unit on the left: a louvred casing and its fan guard. */
    bbc_fill(r, BBC_BLUE, 24.0f, ground - 38.0f, 92.0f, 38.0f);
    bbc_fill(r, BBC_CYAN, 24.0f, ground - 38.0f, 92.0f, 2.0f);
    bbc_fill(r, BBC_CYAN, 24.0f, ground - 38.0f, 4.0f, 38.0f);
    for (float gy = ground - 30.0f; gy < ground - 4.0f; gy += 4.0f)
        bbc_fill(r, BBC_BLACK, 32.0f, gy, 76.0f, 2.0f);
    bbc_fill(r, BBC_BLUE, 52.0f, ground - 44.0f, 36.0f, 6.0f);
    bbc_fill(r, BBC_CYAN, 52.0f, ground - 44.0f, 36.0f, 2.0f);

    /* The stair head: concrete, a white coping, its door lit from the stair
     * below, ROOF over it and a lamp. */
    const float sx = 608.0f;
    const float sw = 188.0f;
    const float top = ground - 112.0f;
    bbc_fill(r, BBC_BLUE, sx, top, sw, ground - top);
    bbc_fill(r, BBC_WHITE, sx - 4.0f, top - 4.0f, sw + 8.0f, 4.0f);
    bbc_fill(r, BBC_CYAN, sx, top, 4.0f, ground - top);
    for (float cy = top + 12.0f; cy < ground; cy += 14.0f)
        bbc_fill(r, BBC_BLACK, sx + 4.0f, cy, sw - 4.0f, 2.0f);
    const float dx = 648.0f;
    const float dw = 88.0f;
    const float dh = 80.0f;
    bbc_fill(r, BBC_BLACK, dx, ground - dh, dw, dh);
    bbc_fill(r, BBC_YELLOW, dx + 4.0f, ground - dh + 4.0f, dw - 8.0f, dh - 4.0f);
    for (int step = 0; step < 4; ++step)
        bbc_fill(r, BBC_RED, dx + 4.0f, ground - 18.0f - (float)step * 16.0f,
                 dw - 8.0f - (float)step * 16.0f, 2.0f);
    float leaf = (dw - 8.0f) * (1.0f - door_open);
    if (leaf >= 4.0f)
        bbc_dither(r, BBC_BLACK, BBC_WHITE, BBC_DITHER_CHECK, dx + 4.0f,
                   ground - dh + 4.0f, leaf, dh - 4.0f);
    bbc_fill(r, BBC_WHITE, dx, ground - dh, dw, 4.0f);
    bbc_fill(r, BBC_BLACK, dx + 20.0f, ground - dh - 20.0f, 48.0f, 14.0f);
    text(r, dx + 28.0f, ground - dh - 17.0f, 1, BBC_WHITE, "ROOF");
    bbc_fill(r, BBC_YELLOW, dx + 40.0f, ground - dh - 6.0f, 8.0f, 4.0f);
    for (float py = ground + 2.0f; py < ground + 14.0f; py += 2.0f)
        bbc_stipple(r, BBC_YELLOW, BBC_DITHER_CHECK, dx - 12.0f, py, dw + 24.0f,
                    2.0f);
}

/*
 * Their ride, side on, the nose to the left the way it flies in: a green
 * airframe with its glass in cyan, the tail boom and fin trailing, the skids
 * under it in white. Drawn at the cast's own doubled pixel. The rotor is drawn
 * by the scene, because a rotor at this size is a bar that changes length,
 * not a drawing.
 */
BBC_SPRITE(HELI_SIDE,
    ".......................W",
    "......GGGGGG..........GG",
    "...CCCGGGGGGGG........GG",
    "..CCCCGGGGGGGGGGGGGGGGGG",
    ".CCWCCGGGGWGGGGGGGGGG.GG",
    ".GGGGGGGGGGGGGGG.....W..",
    "..GGGGGGGGGGGG..........",
    "....GY....G.............",
    "..WWWWWWWWWWWW..........");

/* The same ship end-on, which is the frame a ship spinning on its mast shows
 * between one side and the other. */
BBC_SPRITE(HELI_END,
    "......",
    ".GGGG.",
    "GCCCCG",
    "GCWCCG",
    "GCCCCG",
    "GGGGGG",
    ".GGGG.",
    ".GYYG.",
    "W....W");

/* Where on the side drawing the mast and the lamp are, in its own pixels. */
#define HELI_MAST_COL 9
#define HELI_LAMP_COL 4
#define HELI_LAMP_ROW 7

/*
 * The ship with `(x, y)` at the head of its mast, facing the way it flies
 * (`dir`), at `scale`, and its rotor: a long white bar and a short one taking
 * turns frame to frame, which is what a rotor seen from the side looks like
 * on a machine that cannot draw a blur. `spin` picks the yaw frame of a ship
 * that has lost its tail: side, end on, the other side, end on.
 */
static void draw_helicopter(SDL_Renderer *r, float x, float y, int dir,
                            float rotor_time, int spin, int scale)
{
    const float cw = (float)(BBC_PX_W * scale);
    const float ch = (float)(BBC_PX_H * scale);
    const BbcSprite *s = &HELI_SIDE;
    int face = dir >= 0 ? 1 : -1;
    if (spin == 2)
        face = -face;
    /* The drawing faces left, so a ship facing right is the mirror. */
    int mirror = face >= 0 ? -1 : 1;
    int mast = face >= 0 ? bbc_sprite_w(s) - 1 - HELI_MAST_COL : HELI_MAST_COL;
    float left = x - ((float)mast + 0.5f) * cw;
    if (spin == 1 || spin == 3)
    {
        s = &HELI_END;
        left = x - bbc_sprite_px_w(s) * (float)scale * 0.5f;
    }
    bbc_sprite_scaled(r, s, left, y + ch, mirror, NULL, scale);

    int phase = (int)floorf(rotor_time * 20.0f) & 1;
    float half = (phase ? 26.0f : 14.0f) * cw * 0.5f;
    bbc_fill(r, BBC_WHITE, x - cw * 0.5f, y, cw, ch);
    bbc_fill(r, phase ? BBC_WHITE : BBC_CYAN, x - half, y - 2.0f, half * 2.0f,
             2.0f);
    if (s == &HELI_SIDE)
    {
        int tail = face >= 0 ? 0 : bbc_sprite_w(s) - 1;
        float tx = left + (float)tail * cw;
        bbc_fill(r, phase ? BBC_WHITE : BBC_BLUE, tx, y + ch,
                 (float)BBC_PX_W, phase ? ch * 4.0f : ch * 2.0f);
    }
}

/* Where the ship's searchlight hangs, for the beam and the shot at it. */
static void heli_lamp(float x, float y, int dir, int scale, float *lx,
                      float *ly)
{
    const float cw = (float)(BBC_PX_W * scale);
    const float ch = (float)(BBC_PX_H * scale);
    int lamp = dir >= 0 ? bbc_sprite_w(&HELI_SIDE) - 1 - HELI_LAMP_COL
                        : HELI_LAMP_COL;
    int mast = dir >= 0 ? bbc_sprite_w(&HELI_SIDE) - 1 - HELI_MAST_COL
                        : HELI_MAST_COL;
    *lx = x + (float)(lamp - mast) * cw;
    *ly = y + ch + (float)HELI_LAMP_ROW * ch + ch * 0.5f;
}

/*
 * A round in flight: a yellow line whose head runs from the muzzle to the
 * target in eight hundredths of a second and whose tail follows it, and the
 * white star at the muzzle on the frames it fires.
 */
static void draw_shot_tracer(SDL_Renderer *r, float time, float shot_time,
                             float from_x, float from_y, float to_x,
                             float to_y)
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
    bbc_line(r, BBC_YELLOW, tx, ty, hx, hy);
    bbc_line(r, BBC_YELLOW, tx, ty + 2.0f, hx, hy + 2.0f);
    bbc_block(r, BBC_WHITE, hx, hy, 1, 2);

    if (age < 0.07f)
        bbc_sprite_scaled(r, &SPR_MUZZLE_FLASH, from_x - 12.0f, from_y - 6.0f,
                          1, NULL, CAST_SCALE);
}

/* The muzzle of one of Chuck's aimed drawings, the far side of its yellow
 * pixel. */
static void chuck_muzzle(const BbcSprite *s, float cx, float foot_y, int dir,
                         float *mx, float *my)
{
    int row = s == &SPR_CHUCK_AIM_UP ? 0 : 7;
    sprite_cell(s, cx, foot_y, dir, CAST_SCALE, 7, row, mx, my);
    *mx += dir >= 0 ? CAST_CELL_W : 0.0f;
    *my += CAST_CELL_H * 0.5f;
}

/*
 * A blast as the machine drew one: rings of white, yellow and red going out
 * from the heart of it and taking turns frame to frame, so the whole thing
 * flashes; burning pieces thrown out as single cells; and the smoke after it
 * as blue checkerboard rising off the wreck.
 */
static void draw_explosion(SDL_Renderer *r, float x, float y, float age)
{
    if (age < 0.0f || age > 3.2f)
        return;

    static const BbcColour ring[3] = {BBC_WHITE, BBC_YELLOW, BBC_RED};
    int tick = (int)floorf(age * 20.0f);

    /* Smoke, rising and spreading behind the fire. */
    if (age > 0.4f)
    {
        float smoke = clamp01((age - 0.4f) / 2.8f);
        for (int k = 0; k < 5; ++k)
        {
            float sx = x - 32.0f + (float)k * 16.0f +
                       smoke * (float)(k - 2) * 12.0f;
            float sy = y - 10.0f - smoke * (50.0f + (float)k * 14.0f);
            float rad = 12.0f + smoke * 20.0f;
            BbcDither pat = smoke < 0.6f ? BBC_DITHER_CHECK : BBC_DITHER_SPARSE;
            for (float py = sy - rad * 0.6f; py < sy + rad * 0.6f; py += 2.0f)
            {
                float t = (py - sy) / (rad * 0.6f);
                float half = rad * sqrtf(fmaxf(0.0f, 1.0f - t * t));
                bbc_stipple(r, BBC_BLUE, pat, sx - half, py, half * 2.0f,
                            2.0f);
            }
        }
    }

    if (age < 0.9f)
    {
        float grow = smoothstep01(age / 0.42f);
        float radius = 20.0f + grow * 80.0f;
        for (int k = 0; k < 3; ++k)
        {
            float rr = radius * (1.0f - (float)k * 0.28f);
            fill_ellipse(r, ring[(k + tick) % 3], x, y, rr, rr * 0.6f);
        }
        if (age < 0.25f)
            fill_ellipse(r, BBC_WHITE, x, y, radius * 0.3f, radius * 0.2f);
    }
    else if (age < 1.6f)
    {
        float rr = 56.0f * (1.0f - (age - 0.9f) / 0.7f) + 10.0f;
        ring_ellipse(r, ring[tick % 3], x, y, rr, rr * 0.6f);
    }

    for (unsigned i = 0; i < 28u; ++i)
    {
        unsigned h = scene_hash(i + 0x4558504cu);
        float angle = (float)(h % 628u) * 0.01f;
        float speed = 50.0f + (float)((h >> 8) % 110u);
        float distance = speed * age;
        float px = x + cosf(angle) * distance;
        float py = y + sinf(angle) * distance * 0.6f + 70.0f * age * age -
                   age * 30.0f;
        if (age > 1.8f && (h & 1u))
            continue;
        if (py > OUTRO_GROUND)
            continue;
        bbc_block(r, ring[(i + (unsigned)tick) % 3u], px, py, 1, 2);
    }
}

/* What is left of the ship on the roof: a heap of green and black, a skid
 * standing out of it, and the fire in it flickering in three flat frames. */
static void draw_wreckage(SDL_Renderer *r, float x, float ground_y,
                          float time)
{
    bbc_fill(r, BBC_BLACK, x - 56.0f, ground_y - 28.0f, 100.0f, 28.0f);
    bbc_dither(r, BBC_BLACK, BBC_GREEN, BBC_DITHER_CHECK, x - 48.0f,
               ground_y - 24.0f, 84.0f, 20.0f);
    bbc_fill(r, BBC_CYAN, x - 40.0f, ground_y - 20.0f, 16.0f, 8.0f);
    bbc_line(r, BBC_WHITE, x - 96.0f, ground_y - 2.0f, x + 60.0f,
             ground_y - 30.0f);
    int frame = (int)floorf(time * 9.0f) % 3;
    static const float flame_h[3][4] = {
        {18.0f, 30.0f, 14.0f, 22.0f},
        {26.0f, 20.0f, 24.0f, 14.0f},
        {14.0f, 34.0f, 18.0f, 26.0f}};
    for (int k = 0; k < 4; ++k)
    {
        float fx = x - 24.0f + (float)k * 12.0f;
        float fh = flame_h[frame][k];
        bbc_fill(r, BBC_RED, fx, ground_y - 24.0f - fh, 12.0f, fh);
        bbc_fill(r, BBC_YELLOW, fx + 4.0f, ground_y - 24.0f - fh * 0.6f, 4.0f,
                 fh * 0.6f);
    }
}

/* The heart, held up large and beating: the same glyph the HUD counts and the
 * manual teaches, a size bigger on every beat. */
static void draw_pixel_heart(SDL_Renderer *r, float center_x, float y,
                             float time, int base_scale)
{
    bool beat = fmodf(time, 0.8f) < 0.14f;
    int scale = base_scale + (beat ? 1 : 0);
    float bob = fmodf(time, 1.6f) < 0.8f ? 0.0f : -4.0f;
    float w = bbc_sprite_px_w(&SPR_ICON_HEART) * (float)scale;
    float h = bbc_sprite_px_h(&SPR_ICON_HEART) * (float)scale;
    bbc_sprite_scaled(r, &SPR_ICON_HEART, bbc_snap_x(center_x - w * 0.5f),
                      bbc_snap_y(y + bob - h * 0.5f), 1, NULL, scale);
}

/* Where the two of them stand at the end, either side of the centre. */
#define REUNION_ELLEN_DX (-34.0f)
#define REUNION_CHUCK_DX 34.0f

static void draw_reunion_pair(SDL_Renderer *r, float center_x, float ground_y,
                              float time)
{
    /*
     * Keep the ending deliberately simple: the two of them stand close and
     * face one another, nothing crossing between them, and the heart over
     * them.
     */
    stand(r, &SPR_ELLEN_STAND, center_x + REUNION_ELLEN_DX, ground_y, 1, NULL);
    stand(r, &SPR_CHUCK_STAND, center_x + REUNION_CHUCK_DX, ground_y, -1,
          NULL);
    draw_pixel_heart(r, center_x, ground_y - CAST_H - 30.0f, time, 4);
}

static void render_outro_ui(SDL_Renderer *r, float time, float hostage_x,
                            int win_w, int win_h, const PadHints *pad)
{
    if (time >= 4.35f && time < 7.45f)
        draw_caption(r, 34.0f, 39.0f, BBC_YELLOW,
                     "ROOF // 01:00 // THEIR RIDE IS COMING", time - 4.35f,
                     104.0f, BBC_YELLOW);

    if (time >= 6.00f && time < 9.30f)
    {
        draw_target_brackets(r, hostage_x - 44.0f, OUTRO_GROUND - CAST_H - 10.0f,
                             88.0f, CAST_H + 16.0f);
        typed_text(r, 34.0f, 78.0f, 1, BBC_RED,
                   "NO CLEAR SHOT // ELLEN IN LINE OF FIRE", time - 6.00f);
    }

    if (time >= 9.25f && time < 10.25f)
        typed_text(r, 34.0f, 78.0f, 1, BBC_CYAN, "NEW TARGET // THE RIDE",
                   time - 9.25f);

    if (time >= 13.35f && time < 16.50f)
        draw_caption(r, 34.0f, 39.0f, BBC_RED,
                     "NO RIDE // VOSS HAS NOWHERE LEFT TO GO", time - 13.35f,
                     0.0f, BBC_RED);

    /*
     * The last caption, and the only one that is not a readout.
     *
     * Voss's men spent seventeen sectors calling him a cowboy — off the net, and
     * shouted down at him off the wall — because a man alone in a building
     * doing this is the only thing they could file him as. It is the wrong
     * word, and this is the line that says so: he came up forty floors for one
     * person and not for the building, and the whole night pays off on that
     * being the difference between them.
     */
    if (time >= 16.60f && time < OUTRO_FINAL_REVEAL_TIME)
        draw_caption(r, 34.0f, 39.0f, BBC_WHITE,
                     "NO COWBOY // JUST THE HUSBAND", time - 16.60f, 88.0f,
                     BBC_RED);

    if (time > 0.85f && time < OUTRO_FINAL_REVEAL_TIME)
        draw_skip_hint(r, win_w, win_h, pad);
}

/* How far the stair head's door stands open: out of it, and shut behind. */
static float outro_door_open(float time)
{
    return smoothstep01((time - 0.20f) / 0.25f) *
           (1.0f - smoothstep01((time - 2.90f) / 0.40f));
}

void outro_cutscene_render(SDL_Renderer *r, const OutroCutscene *cutscene,
                           int win_w, int win_h, const PadHints *pad)
{
    const float time = cutscene->time;
    const float ground = OUTRO_GROUND;
    /* Where Voss, Ellen and his man stand to wait, and where Chuck stops. */
    const float captor_one_x = 228.0f;
    const float hostage_x = 296.0f;
    const float captor_two_x = 364.0f;
    const float agent_stop_x = 572.0f;
    /* Clear of where his man falls, so the two of them do not stand on him. */
    const float reunion_center_x = 444.0f;
    const float reunion_chuck_x = reunion_center_x + REUNION_CHUCK_DX;
    const float reunion_ellen_x = reunion_center_x + REUNION_ELLEN_DX;

    float fade_in = 1.0f - smoothstep01(time / 0.62f);
    scene_begin(fade_steps(fade_in));

    /* The blast is a white frame and then a beat of the night gone red: one
     * palette change, everywhere at once. */
    float blast_age = time - 13.35f;
    if (blast_age >= 0.06f && blast_age < 0.22f)
        bbc_palette_set(BBC_BLACK, BBC_RED);

    draw_stars(r, time, (float)win_w, 24.0f, 270.0f, 0x524f4f46u, 92u);
    draw_moon(r, OUTRO_MOON_X, OUTRO_MOON_Y, 22.0f);
    render_outro_skyline(r, time, win_w);
    render_rooftop(r, time, win_w, win_h, outro_door_open(time));

    float heli_x;
    float heli_y;
    int heli_spin = 0;
    float rotor_time = time;
    bool helicopter_visible = time >= 4.20f && time < 13.35f;

    if (time < 10.10f)
    {
        float approach = smoothstep01((time - 4.20f) / 5.15f);
        heli_x = lerpf((float)win_w + 155.0f, 675.0f, approach);
        heli_y = lerpf(108.0f, 160.0f, approach) +
                 (fmodf(time, 1.0f) < 0.5f ? 0.0f : 2.0f);
    }
    else
    {
        /* Hit, it loses its tail and spins on its mast as it falls: side,
         * end on, the other side, end on, faster as it goes. */
        float crash = clamp01((time - 10.10f) / 3.25f);
        heli_x = lerpf(675.0f, 710.0f, crash);
        heli_y = lerpf(160.0f, 372.0f, crash * crash);
        heli_spin = (int)floorf(crash * crash * 18.0f + crash * 4.0f) & 3;
        rotor_time = time * (1.0f - crash * 0.8f);
    }

    if (helicopter_visible)
    {
        /* While hovering, the ship sweeps its searchlight over the group: a
         * wedge of white dither from the lamp to a pool on the deck. The beam
         * dies once the craft is hit. */
        if (time >= 5.4f && time < 10.10f)
        {
            float lx, ly;
            heli_lamp(heli_x, heli_y, -1, CAST_SCALE, &lx, &ly);
            float sweep = sinf(time * 0.85f) * 30.0f;
            float beam_x = hostage_x + sweep;
            dither_wedge(r, BBC_WHITE, BBC_DITHER_CHECK, lx, ly,
                         beam_x - 44.0f, beam_x + 44.0f, ground);
            for (float py = ground + 2.0f; py < ground + 12.0f; py += 2.0f)
                bbc_stipple(r, BBC_WHITE, BBC_DITHER_CHECK, beam_x - 56.0f, py,
                            112.0f, 2.0f);
        }
        draw_helicopter(r, heli_x, heli_y, -1, rotor_time, heli_spin,
                        CAST_SCALE);
        if (time >= 10.10f)
        {
            /* Burning as it goes: flame at the engine and smoke behind. */
            float crash = clamp01((time - 10.10f) / 3.25f);
            bbc_fill(r, bbc_flash_pair(BBC_RED, BBC_YELLOW), heli_x - 8.0f,
                     heli_y + 4.0f, 24.0f, 12.0f);
            for (int k = 1; k < 6; ++k)
                bbc_stipple(r, BBC_BLUE, BBC_DITHER_CHECK,
                            heli_x - 16.0f + (float)k * 2.0f,
                            heli_y - 6.0f - (float)k * 12.0f * (0.5f + crash),
                            32.0f + (float)k * 6.0f, 10.0f);
        }
    }

    if (time >= 13.35f)
    {
        draw_wreckage(r, 710.0f, ground, time);
        draw_explosion(r, 710.0f, 379.0f, time - 13.35f);
    }

    /* Voss, Ellen and his man cross the roof first and wait for their ride. */
    if (time >= 0.45f)
    {
        float group_move = smoothstep01((time - 0.45f) / 3.30f);
        float first_x = lerpf(700.0f, captor_one_x, group_move);
        float woman_x = lerpf(764.0f, hostage_x, group_move);
        float second_x = lerpf(828.0f, captor_two_x, group_move);
        bool walking = time < 3.75f;
        int group_dir = walking ? -1 : 1;
        float shake = 0.0f;
        if (time >= 13.35f && time < 14.70f)
            shake = (fmodf(time, 0.1f) < 0.05f) ? 4.0f : -4.0f;

        /* Voss. */
        if (time < 15.60f)
        {
            const BbcSprite *voss = &SPR_VOSS_STAND;
            if (walking)
                voss = (stride_quarter(first_x, CAST_WALK_QUARTER) & 1)
                           ? &SPR_VOSS_STAND
                           : &SPR_VOSS_WALK;
            else if (time >= 5.05f && time < 13.35f)
                voss = &SPR_VOSS_AIM;
            stand(r, voss, first_x + shake, ground, group_dir, NULL);
            if (time >= 13.35f && time < 14.70f)
                draw_alarm_mark(r, first_x, ground - CAST_H - 4.0f, 1.0f,
                                bbc_flash(BBC_YELLOW));
        }
        else
        {
            stand(r, &VOSS_DOWN, first_x, ground, -1, NULL);
        }

        /* His man. */
        if (time < 14.70f)
        {
            const BbcSprite *man =
                walking ? crew_walk_frame(-second_x) : &SPR_AGENT_AIM;
            stand(r, man, second_x - shake, ground, group_dir,
                  BBC_AGENT_PAINT);
            if (time >= 13.35f)
                draw_alarm_mark(r, second_x, ground - CAST_H - 4.0f, 1.0f,
                                bbc_flash(BBC_YELLOW));
        }
        else
        {
            /* Hit from the right, he goes down backwards. */
            stand(r, &SPR_GUARD_DOWN, second_x - 16.0f, ground, 1,
                  BBC_AGENT_PAINT);
        }

        /* Ellen. The blast puts her down on her knees, which is what gives
         * him the shot over her head. */
        if (time < 16.45f)
        {
            const BbcSprite *ellen =
                walking ? ellen_walk_frame(-woman_x, true) : &ELLEN_BOUND_STAND;
            if (time >= 13.45f)
                ellen = &ELLEN_DUCK;
            stand(r, ellen, woman_x, ground, group_dir, NULL);
        }
        else if (time < 18.35f)
        {
            float reunion = smoothstep01((time - 16.45f) / 1.90f);
            float x = lerpf(woman_x, reunion_ellen_x, reunion);
            stand(r, reunion < 1.0f ? ellen_walk_frame(x, false)
                                    : &SPR_ELLEN_STAND,
                  x, ground, 1, NULL);
        }
    }

    /* Chuck follows, holds his fire, then pivots toward the ship. */
    const BbcSprite *chuck = NULL;
    float chuck_x = agent_stop_x;
    int chuck_dir = -1;
    if (time >= 2.35f && time < 5.05f)
    {
        float u = (time - 2.35f) / 2.55f;
        chuck_x = lerpf(724.0f, agent_stop_x, smoothstep01(u));
        chuck = smoothstep_pace(u) > 0.15f ? chuck_run_frame(-chuck_x)
                                           : &SPR_CHUCK_STAND;
    }
    else if (time >= 5.05f && time < 9.25f)
    {
        chuck = time < 8.55f ? &SPR_CHUCK_AIM : &SPR_CHUCK_AIM_DOWN;
    }
    else if (time >= 9.25f && time < 10.45f)
    {
        chuck = &SPR_CHUCK_AIM_UP;
        chuck_dir = 1;
    }
    else if (time >= 10.45f && time < 14.20f)
    {
        chuck = &SPR_CHUCK_AIM_DOWN;
        chuck_dir = 1;
    }
    else if (time >= 14.20f && time < 16.40f)
    {
        chuck = &SPR_CHUCK_AIM;
    }
    else if (time >= 16.40f && time < 18.35f)
    {
        float u = (time - 16.40f) / 1.95f;
        chuck_x = lerpf(agent_stop_x, reunion_chuck_x, smoothstep01(u));
        chuck = smoothstep_pace(u) > 0.15f ? chuck_run_frame(-chuck_x)
                                           : &SPR_CHUCK_STAND;
    }
    else if (time >= 18.35f && time < OUTRO_FINAL_REVEAL_TIME)
    {
        draw_reunion_pair(r, reunion_center_x, ground, time);
    }
    if (chuck != NULL)
        stand(r, chuck, chuck_x, ground, chuck_dir, NULL);

    /* Three deliberate shots tell the turn-and-rescue beat without gameplay.
     * Each round leaves the muzzle of the pistol as it is drawn in that pose,
     * so the flash sits on the gun rather than in the air in front of it. */
    float sky_x, sky_y, level_x, level_y;
    chuck_muzzle(&SPR_CHUCK_AIM_UP, agent_stop_x, ground, 1, &sky_x, &sky_y);
    chuck_muzzle(&SPR_CHUCK_AIM, agent_stop_x, ground, -1, &level_x, &level_y);
    draw_shot_tracer(r, time, 9.95f, sky_x, sky_y, heli_x - 8.0f,
                     heli_y + 16.0f);
    draw_shot_tracer(r, time, 14.55f, level_x, level_y, captor_two_x + 8.0f,
                     level_y);
    draw_shot_tracer(r, time, 15.45f, level_x, level_y, captor_one_x + 8.0f,
                     level_y);

    render_outro_ui(r, time, hostage_x, win_w, win_h, pad);

    /* The flash itself: one white frame. */
    if (blast_age >= 0.0f && blast_age < 0.06f)
        bbc_fill(r, BBC_WHITE, 0.0f, 20.0f, (float)win_w, (float)win_h - 40.0f);

    draw_letterbox(r, win_w, win_h, 20.0f);

    if (time >= OUTRO_FINAL_REVEAL_TIME)
    {
        /* The card: the roof shaded behind it the way a BBC shaded what sat
         * behind a menu — a checkerboard of black over all of it — and the two
         * of them, and the words, coming up the palette in three steps. */
        float reveal = smoothstep01((time - OUTRO_FINAL_REVEAL_TIME) / 1.0f);
        int steps = fade_steps(1.0f - reveal);
        bbc_stipple(r, BBC_BLACK, BBC_DITHER_CHECK, 0.0f, 20.0f, (float)win_w,
                    (float)win_h - 40.0f);
        bbc_fill(r, BBC_BLACK, 0.0f, 56.0f, (float)win_w, 112.0f);
        draw_reunion_pair(r, reunion_center_x, ground, time);

        dim_local(steps);
        bbc_fill(r, BBC_CYAN, 0.0f, 98.0f, (float)win_w, 2.0f);
        text_centered(r, (float)win_w * 0.5f, 69.0f, 2, BBC_GREEN,
                      "SHE'S SAFE");
        text_centered(r, (float)win_w * 0.5f, 122.0f, 2, BBC_WHITE,
                      "THANK YOU FOR PLAYING");
        text_centered(r, (float)win_w * 0.5f, 151.0f, 1, BBC_CYAN,
                      "THE BONDS BURNED. SHE DID NOT.");
        dim_restore();

        if (time >= OUTRO_REPLAY_PROMPT_TIME)
        {
            char hint[40];
            text_centered(r, (float)win_w * 0.5f, 505.0f, 1,
                          fmodf(time, 1.2f) < 0.8f ? BBC_WHITE : BBC_CYAN,
                          pad_hint(pad, hint, sizeof(hint),
                                   "PRESS $A TO REPLAY THE RESCUE",
                                   "PRESS R TO REPLAY THE RESCUE"));
        }
    }
    scene_end();
}

/* ---- The store's key art ---------------------------------------------- */

/*
 * The one picture most people will ever see of this game, and the only one
 * whose job is to make somebody want to play it.
 *
 * It is the moment the whole climb is for, drawn as close as the film ever
 * gets — a loading screen rather than a frame of play: the roof, forty floors
 * over a city that has gone to a scatter of lit cells, a helicopter holding a
 * hover with its light on the parapet, Voss at the edge with his pistol down
 * on the man hanging from it, and Ellen — the red coat, the one warm colour on
 * the roof — a stride behind him. It is not a scene the game plays; it is the
 * stakes of every scene it does, in one frame.
 *
 * Every figure in it is the cast's own drawing, four BBC pixels to a sprite
 * pixel, so the people read at the size a store lists a game at and the city
 * they are standing over reads as far away by being finer. The wordmark goes
 * over the empty sky top left (intro.c).
 */
#define KEY_ART_ROOF_Y 300.0f
#define KEY_ART_CORNER_X 430.0f
#define KEY_ART_SCALE 4
#define KEY_ART_HELI_X 640.0f
#define KEY_ART_HELI_Y 70.0f

/* One deck of storm cloud: blue dithered into the black from `top` down to a
 * lumpy underside around `base`, and lit white where the strike is near. */
static void key_art_cloud_deck(SDL_Renderer *r, int win_w, float top,
                               float base, unsigned seed, BbcDither body,
                               float flash_x, bool strike)
{
    for (int x = 0; x < win_w; x += BBC_PX_W)
    {
        float fx = (float)x;
        float edge = base + sinf(fx * 0.009f + (float)(seed & 63u)) * 12.0f +
                     sinf(fx * 0.027f + (float)(seed >> 6 & 63u)) * 4.0f;
        int cell = (int)floorf(fx / 52.0f);
        unsigned h = scene_hash((unsigned)(cell + 1000) * 2654435761u + seed);
        if (h % 3u != 0u)
        {
            float cx = (float)cell * 52.0f + (float)(h % 40u);
            float rad = 12.0f + (float)((h >> 8) % 30u);
            float dx = fx - cx;
            if (fabsf(dx) < rad)
                edge = fmaxf(edge, base - 8.0f +
                                       sqrtf(rad * rad - dx * dx) * 0.6f);
        }
        /* Lit full near the strike and on alternate columns a little further
         * out, so the light has an edge of dither rather than a cut. */
        float near = fabsf(fx - flash_x);
        bool lit = strike && (near < 150.0f ||
                              (near < 230.0f && ((x / BBC_PX_W) & 1) == 0));
        bbc_dither(r, BBC_BLACK, lit ? BBC_WHITE : BBC_BLUE, body, fx, top,
                   (float)BBC_PX_W, edge - top - 4.0f);
        bbc_fill(r, lit ? BBC_WHITE : BBC_BLUE, fx, edge - 4.0f,
                 (float)BBC_PX_W, 4.0f);
    }
}

/* The strike: a jagged white line from the cloud to the horizon, on for a
 * frame or two and then gone, twice a cycle. The poster's own still frame is
 * taken in the dark between strikes. */
static bool key_art_strike(float time)
{
    float t = fmodf(time, 4.6f);
    return (t > 3.10f && t < 3.22f) || (t > 3.32f && t < 3.40f);
}

static void key_art_sky(SDL_Renderer *r, float time, int win_w, int win_h)
{
    const float w = (float)win_w;
    bool strike = key_art_strike(time);
    const float flash_x = 120.0f;

    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, w, (float)win_h);
    key_art_cloud_deck(r, win_w, 0.0f, 22.0f, 0x51u, BBC_DITHER_CHECK, flash_x,
                       strike);
    draw_stars(r, time, w, 30.0f, 150.0f, 77u, 40u);
    key_art_cloud_deck(r, win_w, 214.0f, 250.0f, 0x7f1u, BBC_DITHER_CHECK,
                       flash_x, strike);

    if (strike)
    {
        float x = flash_x;
        float y = 250.0f;
        for (int k = 0; k < 6; ++k)
        {
            unsigned h = scene_hash((unsigned)k * 97u + 13u);
            float nx = x + ((float)(h % 7u) - 3.0f) * 6.0f;
            float ny = y + 8.0f;
            bbc_line(r, BBC_WHITE, x, y, nx, ny);
            x = nx;
            y = ny;
        }
    }
}

/* The city forty floors down: the horizon a band of blue, and under it the
 * streets as lines of lit cells running toward the foot of the frame, sparse
 * near the horizon and closer together nearer the tower, with the blocks
 * between them black. */
static void key_art_city(SDL_Renderer *r, int win_w, int win_h)
{
    const float horizon = 296.0f;
    bbc_fill(r, BBC_BLUE, 0.0f, horizon, (float)win_w, 2.0f);
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_SPARSE, 0.0f, horizon + 2.0f,
               (float)win_w, 8.0f);
    int row = 0;
    for (float y = horizon + 14.0f; y < (float)win_h; ++row)
    {
        float depth = (y - horizon) / ((float)win_h - horizon);
        /* A street running across: a broken line of lamps. */
        unsigned rh = scene_hash((unsigned)row * 131u + 5u);
        BbcColour street = (rh & 3u) == 0u ? BBC_WHITE
                           : (rh & 3u) == 1u ? BBC_CYAN
                                             : BBC_YELLOW;
        float gap = depth < 0.4f ? 20.0f : 12.0f;
        for (float x = spread(rh, 16.0f); x < (float)win_w; x += gap)
        {
            unsigned h = scene_hash((unsigned)(int)x * 7919u + rh);
            if ((h % 3u) == 0u)
                continue;
            bbc_plot(r, street, x, y);
        }
        /* And a few lit windows in the blocks between this street and the
         * next. */
        float next = y + 8.0f + depth * 26.0f;
        for (float by = y + 4.0f; by < next - 2.0f && by < (float)win_h;
             by += 4.0f)
        {
            for (float x = 0.0f; x < (float)win_w; x += 8.0f)
            {
                unsigned h = scene_hash((unsigned)(int)x * 104729u +
                                        (unsigned)(int)by * 31u + rh);
                if ((h % 13u) != 0u)
                    continue;
                bbc_plot(r, (h & 16u) ? BBC_YELLOW : BBC_BLUE, x, by);
            }
        }
        y = next;
    }
}

/* The tower's top, close to: blue face, the white coping and the cyan arris,
 * and the office glass under them at the poster's coarse size. */
static void key_art_tower(SDL_Renderer *r, int win_w, int win_h)
{
    const float x = KEY_ART_CORNER_X;
    const float y = KEY_ART_ROOF_Y;
    const float w = (float)win_w - x;
    bbc_fill(r, BBC_BLUE, x, y, w, (float)win_h - y);
    bbc_fill(r, BBC_WHITE, x, y, w, 4.0f);
    bbc_fill(r, BBC_CYAN, x, y + 4.0f, w, 4.0f);
    bbc_fill(r, BBC_BLACK, x, y + 8.0f, w, 4.0f);
    bbc_fill(r, BBC_CYAN, x, y, 8.0f, (float)win_h - y);
    for (int row = 0; row < 4; ++row)
    {
        float wy = y + 34.0f + (float)row * 60.0f;
        for (int col = 0; col < 6; ++col)
        {
            float wx = x + 28.0f + (float)col * 60.0f;
            unsigned h = scene_hash((unsigned)(row * 17 + col * 5 + 3));
            bbc_fill(r, BBC_BLACK, wx, wy, 44.0f, 40.0f);
            if ((h % 4u) == 0u)
            {
                bbc_fill(r, (h & 8u) ? BBC_YELLOW : BBC_CYAN, wx + 4.0f,
                         wy + 4.0f, 36.0f, 32.0f);
                bbc_fill(r, BBC_WHITE, wx + 4.0f, wy + 4.0f, 36.0f, 4.0f);
            }
            else
            {
                bbc_fill(r, BBC_BLUE, wx + 8.0f, wy + 28.0f, 4.0f, 4.0f);
                bbc_fill(r, BBC_BLUE, wx + 16.0f, wy + 20.0f, 4.0f, 4.0f);
            }
            bbc_fill(r, BBC_BLACK, wx + 20.0f, wy, 4.0f, 40.0f);
        }
    }
}

void key_art_render(SDL_Renderer *r, float time, int win_w, int win_h)
{
    const float roof = KEY_ART_ROOF_Y;
    const float corner = KEY_ART_CORNER_X;
    const int k = KEY_ART_SCALE;
    const float cw = (float)(BBC_PX_W * k);
    const float ch = (float)(BBC_PX_H * k);

    key_art_sky(r, time, win_w, win_h);
    key_art_city(r, win_w, win_h);

    /* The ship further off than the roof, at the film's doubled pixel. */
    draw_helicopter(r, KEY_ART_HELI_X, KEY_ART_HELI_Y, -1, time, 0, 2);
    float lx, ly;
    heli_lamp(KEY_ART_HELI_X, KEY_ART_HELI_Y, -1, 2, &lx, &ly);

    key_art_tower(r, win_w, win_h);

    /* Its light on the parapet: a wedge of white dither from the lamp to the
     * corner of the roof, over everything it crosses. */
    float spot_x = corner + 90.0f;
    dither_wedge(r, BBC_WHITE, BBC_DITHER_CHECK, lx, ly, spot_x - 110.0f,
                 spot_x + 110.0f, roof);
    for (float py = roof; py < roof + 8.0f; py += 2.0f)
        bbc_stipple(r, BBC_WHITE, BBC_DITHER_CHECK, spot_x - 130.0f, py,
                    260.0f, 2.0f);

    /* Ellen behind him, bound, her mouth open on his name. */
    bbc_sprite_scaled(r, &ELLEN_CRY,
                      bbc_snap_x(corner + 236.0f - 3.5f * cw),
                      roof - 16.0f * ch, -1, NULL, k);

    /* Voss at the edge, the pistol down at the man under it. */
    bbc_sprite_scaled(r, &VOSS_AIM_DOWN, bbc_snap_x(corner + 92.0f - 4.0f * cw),
                      roof - 16.0f * ch, -1, NULL, k);

    /* Chuck at the lip: forearms on the coping, his face level with the
     * muzzle, the rest of him over forty floors of air. */
    bbc_sprite_scaled(r, &CHUCK_LEDGE, bbc_snap_x(corner - 7.0f * cw),
                      roof - 5.0f * ch, 1, NULL, k);

    const RainLight lights[] = {{spot_x - 150.0f, spot_x + 150.0f, ly,
                                 BBC_WHITE}};
    draw_rain(r, time, (float)win_w, 0.0f, (float)win_h, lights, 1, 220u,
              BBC_BLUE, BBC_CYAN);
}
