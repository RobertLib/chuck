/*
 * The MODE 2 drawing layer. See [bbc.h](bbc.h) for what it is and why every
 * pixel in the game comes through it.
 */

#include "bbc.h"

#include <math.h>
#include <stdlib.h>

static float frame_time;
static bool frame_steady;
static BbcColour palette[BBC_COLOUR_COUNT] = {
    BBC_BLACK, BBC_RED, BBC_GREEN, BBC_YELLOW,
    BBC_BLUE, BBC_MAGENTA, BBC_CYAN, BBC_WHITE};
/* The pattern origin in whole BBC pixels: the grid cell world (0, 0) is in. */
static int pattern_ox;
static int pattern_oy;

void bbc_begin_frame(float time, bool steady)
{
    frame_time = time;
    frame_steady = steady;
    bbc_palette_reset();
    pattern_ox = 0;
    pattern_oy = 0;
}

/* ---- Grid ------------------------------------------------------------- */

static int cell_x(float x)
{
    return (int)floorf(x / (float)BBC_PX_W);
}

static int cell_y(float y)
{
    return (int)floorf(y / (float)BBC_PX_H);
}

float bbc_snap_x(float x)
{
    return (float)(cell_x(x) * BBC_PX_W);
}

float bbc_snap_y(float y)
{
    return (float)(cell_y(y) * BBC_PX_H);
}

void bbc_set_pattern_origin(float screen_x_of_world_0, float screen_y_of_world_0)
{
    pattern_ox = cell_x(screen_x_of_world_0);
    pattern_oy = cell_y(screen_y_of_world_0);
}

/* ---- Colour ----------------------------------------------------------- */

BbcColour bbc_map(BbcColour logical)
{
    return palette[(unsigned)logical & 7u];
}

void bbc_palette_set(BbcColour logical, BbcColour physical)
{
    palette[(unsigned)logical & 7u] = (BbcColour)((unsigned)physical & 7u);
}

void bbc_palette_reset(void)
{
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        palette[i] = (BbcColour)i;
}

/*
 * One rung down the ladder each colour fades by. Luminance decides the rungs —
 * white through cyan and blue, yellow through red, green and magenta through
 * blue — so a fading scene goes cold and dark the way a picture does when the
 * set is turned down, and nothing ever brightens on its way out.
 */
static BbcColour dim_step(BbcColour c)
{
    switch (c)
    {
    case BBC_WHITE:
        return BBC_CYAN;
    case BBC_CYAN:
    case BBC_GREEN:
    case BBC_MAGENTA:
        return BBC_BLUE;
    case BBC_YELLOW:
        return BBC_RED;
    case BBC_RED:
    case BBC_BLUE:
    case BBC_BLACK:
    default:
        return BBC_BLACK;
    }
}

void bbc_palette_dim(int steps)
{
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        for (int s = 0; s < steps && s < 3; ++s)
            palette[i] = dim_step(palette[i]);
}

bool bbc_flash_on(void)
{
    if (frame_steady)
        return true;
    float period = BBC_FLASH_HALF_PERIOD * 2.0f;
    float phase = frame_time - period * floorf(frame_time / period);
    return phase < BBC_FLASH_HALF_PERIOD;
}

BbcColour bbc_flash(BbcColour c)
{
    return bbc_flash_on() ? c : bbc_complement(c);
}

BbcColour bbc_flash_pair(BbcColour a, BbcColour b)
{
    return bbc_flash_on() ? a : b;
}

static void use(SDL_Renderer *r, BbcColour c)
{
    BbcColour p = bbc_map(c);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, BBC_RGB[p][0], BBC_RGB[p][1], BBC_RGB[p][2],
                           255);
}

/* ---- Filled shapes ---------------------------------------------------- */

/* Grid cells [x0, x1) x [y0, y1), filled in the colour last `use`d. */
static void fill_cells(SDL_Renderer *r, int x0, int y0, int x1, int y1)
{
    if (x1 <= x0 || y1 <= y0)
        return;
    SDL_FRect rect = {(float)(x0 * BBC_PX_W), (float)(y0 * BBC_PX_H),
                      (float)((x1 - x0) * BBC_PX_W),
                      (float)((y1 - y0) * BBC_PX_H)};
    SDL_RenderFillRect(r, &rect);
}

/* The cells a logical rectangle covers: both edges floored, and at least one
 * cell in each direction for anything that has a size at all. */
static bool rect_cells(float x, float y, float w, float h, int *x0, int *y0,
                       int *x1, int *y1)
{
    if (!(w > 0.0f) || !(h > 0.0f))
        return false;
    *x0 = cell_x(x);
    *y0 = cell_y(y);
    *x1 = cell_x(x + w);
    *y1 = cell_y(y + h);
    if (*x1 <= *x0)
        *x1 = *x0 + 1;
    if (*y1 <= *y0)
        *y1 = *y0 + 1;
    return true;
}

void bbc_fill(SDL_Renderer *r, BbcColour c, float x, float y, float w, float h)
{
    int x0, y0, x1, y1;
    if (!rect_cells(x, y, w, h, &x0, &y0, &x1, &y1))
        return;
    use(r, c);
    fill_cells(r, x0, y0, x1, y1);
}

void bbc_block(SDL_Renderer *r, BbcColour c, float x, float y, int bw, int bh)
{
    if (bw < 1 || bh < 1)
        return;
    int x0 = cell_x(x);
    int y0 = cell_y(y);
    use(r, c);
    fill_cells(r, x0, y0, x0 + bw, y0 + bh);
}

void bbc_plot(SDL_Renderer *r, BbcColour c, float x, float y)
{
    bbc_block(r, c, x, y, 1, 1);
}

void bbc_frame(SDL_Renderer *r, BbcColour c, float x, float y, float w,
               float h)
{
    int x0, y0, x1, y1;
    if (!rect_cells(x, y, w, h, &x0, &y0, &x1, &y1))
        return;
    use(r, c);
    fill_cells(r, x0, y0, x1, y0 + 1);
    fill_cells(r, x0, y1 - 1, x1, y1);
    fill_cells(r, x0, y0 + 1, x0 + 1, y1 - 1);
    fill_cells(r, x1 - 1, y0 + 1, x1, y1 - 1);
}

void bbc_line(SDL_Renderer *r, BbcColour c, float x0, float y0, float x1,
              float y1)
{
    int ax = cell_x(x0), ay = cell_y(y0);
    int bx = cell_x(x1), by = cell_y(y1);
    int dx = abs(bx - ax), sx = ax < bx ? 1 : -1;
    int dy = -abs(by - ay), sy = ay < by ? 1 : -1;
    int err = dx + dy;
    /* A line across the whole frame is four hundred cells, which is a bound
     * worth writing down: a NaN endpoint must not become a loop that never
     * ends. */
    int guard = 4096;

    use(r, c);
    int run_start = ax;
    int run_y = ay;
    for (;;)
    {
        int px = ax, py = ay;
        bool last = ax == bx && ay == by;
        if (!last)
        {
            int e2 = 2 * err;
            if (e2 >= dy)
            {
                err += dy;
                ax += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                ay += sy;
            }
        }
        /* Merge horizontal runs into one rect: a shallow line is mostly runs. */
        if (last || ay != run_y)
        {
            int lo = run_start < px ? run_start : px;
            int hi = run_start < px ? px : run_start;
            fill_cells(r, lo, py, hi + 1, py + 1);
            run_start = ax;
            run_y = ay;
        }
        if (last || --guard <= 0)
            break;
    }
}

void bbc_disc(SDL_Renderer *r, BbcColour c, float cx, float cy, float radius)
{
    if (!(radius > 0.0f) || !(radius < 4096.0f))
        return;
    use(r, c);
    int top = cell_y(cy - radius);
    int bottom = cell_y(cy + radius);
    bool drew = false;
    for (int gy = top; gy <= bottom; ++gy)
    {
        float dy = (float)(gy * BBC_PX_H) + (float)BBC_PX_H * 0.5f - cy;
        if (fabsf(dy) > radius)
            continue;
        float half = sqrtf(radius * radius - dy * dy);
        /* The cells whose centres lie within the span. */
        int gx0 = (int)ceilf((cx - half - (float)BBC_PX_W * 0.5f) /
                             (float)BBC_PX_W);
        int gx1 = (int)floorf((cx + half - (float)BBC_PX_W * 0.5f) /
                              (float)BBC_PX_W);
        if (gx1 < gx0)
            continue;
        fill_cells(r, gx0, gy, gx1 + 1, gy + 1);
        drew = true;
    }
    if (!drew)
        fill_cells(r, cell_x(cx), cell_y(cy), cell_x(cx) + 1, cell_y(cy) + 1);
}

void bbc_ring(SDL_Renderer *r, BbcColour c, float cx, float cy, float radius)
{
    if (!(radius > 0.0f) || !(radius < 4096.0f))
        return;
    float rx_in = radius - (float)BBC_PX_W;
    float ry_in = radius - (float)BBC_PX_H;
    use(r, c);
    for (int gy = cell_y(cy - radius); gy <= cell_y(cy + radius); ++gy)
        for (int gx = cell_x(cx - radius); gx <= cell_x(cx + radius); ++gx)
        {
            float dx = (float)(gx * BBC_PX_W) + (float)BBC_PX_W * 0.5f - cx;
            float dy = (float)(gy * BBC_PX_H) + (float)BBC_PX_H * 0.5f - cy;
            bool outer = dx * dx + dy * dy <= radius * radius;
            bool inner = rx_in > 0.0f && ry_in > 0.0f &&
                         (dx * dx) / (rx_in * rx_in) +
                                 (dy * dy) / (ry_in * ry_in) <=
                             1.0f;
            if (outer && !inner)
                fill_cells(r, gx, gy, gx + 1, gy + 1);
        }
}

/* ---- Patterns --------------------------------------------------------- */

bool bbc_pattern_ink(BbcDither pattern, int bx, int by)
{
    /* The 2x2 ordered-dither matrix every pattern below is cut from, so the
     * sparse, half and dense fills nest inside one another and a fade through
     * them never moves a pixel that was already lit. */
    int bayer = ((bx & 1) == (by & 1)) ? ((bx & 1) ? 1 : 0)
                                       : ((bx & 1) ? 2 : 3);
    switch (pattern)
    {
    case BBC_DITHER_CHECK:
        return bayer < 2;
    case BBC_DITHER_SPARSE:
        return bayer == 0;
    case BBC_DITHER_DENSE:
        return bayer < 3;
    case BBC_DITHER_ROWS:
        return (by & 1) == 0;
    case BBC_DITHER_COLS:
        return (bx & 1) == 0;
    case BBC_DITHER_DOTS:
        return (by & 1) == 0 && ((bx + ((by >> 1) & 1) * 2) & 3) == 0;
    case BBC_DITHER_COUNT:
    default:
        return false;
    }
}

void bbc_stipple(SDL_Renderer *r, BbcColour ink, BbcDither pattern, float x,
                 float y, float w, float h)
{
    int x0, y0, x1, y1;
    if (!rect_cells(x, y, w, h, &x0, &y0, &x1, &y1))
        return;
    use(r, ink);

    enum
    {
        BATCH = 256
    };
    SDL_FRect rects[BATCH];
    int count = 0;
    for (int gy = y0; gy < y1; ++gy)
    {
        int gx = x0;
        while (gx < x1)
        {
            if (!bbc_pattern_ink(pattern, gx - pattern_ox, gy - pattern_oy))
            {
                ++gx;
                continue;
            }
            int start = gx;
            while (gx < x1 &&
                   bbc_pattern_ink(pattern, gx - pattern_ox, gy - pattern_oy))
                ++gx;
            rects[count++] = (SDL_FRect){(float)(start * BBC_PX_W),
                                         (float)(gy * BBC_PX_H),
                                         (float)((gx - start) * BBC_PX_W),
                                         (float)BBC_PX_H};
            if (count == BATCH)
            {
                SDL_RenderFillRects(r, rects, count);
                count = 0;
            }
        }
    }
    if (count > 0)
        SDL_RenderFillRects(r, rects, count);
}

void bbc_dither(SDL_Renderer *r, BbcColour base, BbcColour ink,
                BbcDither pattern, float x, float y, float w, float h)
{
    bbc_fill(r, base, x, y, w, h);
    if (bbc_map(base) != bbc_map(ink))
        bbc_stipple(r, ink, pattern, x, y, w, h);
}

/* ---- Sprites ---------------------------------------------------------- */

static BbcColour code_colour(int code, const BbcColour *slots)
{
    if (code >= BBC_CODE_SLOT0)
        return slots != NULL ? slots[code - BBC_CODE_SLOT0] : BBC_WHITE;
    return (BbcColour)code;
}

/* The one sprite loop: `solid` paints every opaque pixel in `solid_colour`. */
static void blit(SDL_Renderer *r, const BbcSprite *sprite, float x, float y,
                 int dir, const BbcColour *slots, int scale, bool solid,
                 BbcColour solid_colour)
{
    if (sprite == NULL || sprite->rows == NULL)
        return;
    if (scale < 1)
        scale = 1;
    int w = bbc_sprite_w(sprite);
    int ox = cell_x(x);
    int oy = cell_y(y);

    for (int row = 0; row < sprite->h; ++row)
    {
        const char *line = sprite->rows[row];
        if (line == NULL)
            continue;
        int col = 0;
        while (col < w && line[col] != '\0')
        {
            int code = bbc_sprite_code(line[col]);
            int start = col;
            while (col < w && line[col] != '\0' &&
                   bbc_sprite_code(line[col]) == code)
                ++col;
            if (code < 0)
                continue;
            use(r, solid ? solid_colour : code_colour(code, slots));
            int a = dir >= 0 ? start : w - col;
            int b = dir >= 0 ? col : w - start;
            fill_cells(r, ox + a * scale, oy + row * scale, ox + b * scale,
                       oy + (row + 1) * scale);
        }
    }
}

void bbc_sprite(SDL_Renderer *r, const BbcSprite *sprite, float x, float y,
                int dir, const BbcColour *slots)
{
    blit(r, sprite, x, y, dir, slots, 1, false, BBC_BLACK);
}

void bbc_sprite_scaled(SDL_Renderer *r, const BbcSprite *sprite, float x,
                       float y, int dir, const BbcColour *slots, int scale)
{
    blit(r, sprite, x, y, dir, slots, scale, false, BBC_BLACK);
}

void bbc_sprite_solid(SDL_Renderer *r, const BbcSprite *sprite, float x,
                      float y, int dir, BbcColour c)
{
    blit(r, sprite, x, y, dir, NULL, 1, true, c);
}

float bbc_sprite_px_w(const BbcSprite *sprite)
{
    return (float)(bbc_sprite_w(sprite) * BBC_PX_W);
}

float bbc_sprite_px_h(const BbcSprite *sprite)
{
    return sprite != NULL ? (float)(sprite->h * BBC_PX_H) : 0.0f;
}

void bbc_sprite_at_feet(SDL_Renderer *r, const BbcSprite *sprite, float cx,
                        float foot_y, int dir, const BbcColour *slots)
{
    float x = cx - bbc_sprite_px_w(sprite) * 0.5f;
    float y = foot_y - bbc_sprite_px_h(sprite);
    /* Round the centre rather than floor the corner, so a figure whose box is
     * not a whole number of cells wide stands over the middle of it. */
    x = (float)((int)floorf(x / (float)BBC_PX_W + 0.5f) * BBC_PX_W);
    y = (float)((int)floorf(y / (float)BBC_PX_H + 0.5f) * BBC_PX_H);
    blit(r, sprite, x, y, dir, slots, 1, false, BBC_BLACK);
}

/* ---- Text ------------------------------------------------------------- */

#define FONT_KEY "chuck.bbc.font"
#define FONT_COLS 16
#define FONT_ROWS 6

/*
 * The face as a texture, built once per renderer and hung on the renderer's
 * own properties so it dies with it. Ink is white at full alpha and everything
 * else is fully transparent, so colour-modulated it lands as exactly one of
 * the eight colours and never as an edge between two.
 */
static SDL_Texture *font_texture(SDL_Renderer *r)
{
    SDL_PropertiesID props = SDL_GetRendererProperties(r);
    SDL_Texture *texture =
        props != 0 ? (SDL_Texture *)SDL_GetPointerProperty(props, FONT_KEY, NULL)
                   : NULL;
    if (texture != NULL)
        return texture;

    SDL_Surface *surface = SDL_CreateSurface(FONT_COLS * BBC_FONT_CELL,
                                             FONT_ROWS * BBC_FONT_CELL,
                                             SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL)
        return NULL;
    for (int i = 0; i < FONT_COLS * FONT_ROWS; ++i)
    {
        unsigned char ch = (unsigned char)(BBC_FONT_FIRST + i);
        int gx = (i % FONT_COLS) * BBC_FONT_CELL;
        int gy = (i / FONT_COLS) * BBC_FONT_CELL;
        for (int row = 0; row < BBC_FONT_CELL; ++row)
        {
            Uint32 *line = (Uint32 *)((Uint8 *)surface->pixels +
                                      (gy + row) * surface->pitch) +
                           gx;
            for (int col = 0; col < BBC_FONT_CELL; ++col)
                line[col] = bbc_font_pixel(ch, col, row) ? 0xFFFFFFFFu : 0u;
        }
    }
    texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_DestroySurface(surface);
    if (texture == NULL)
        return NULL;
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    if (props != 0)
        SDL_SetPointerProperty(props, FONT_KEY, texture);
    return texture;
}

static int whole_scale(float scale)
{
    int s = (int)floorf(scale + 0.5f);
    return s < 1 ? 1 : (s > 8 ? 8 : s);
}

float bbc_text_width(const char *text, float scale)
{
    if (text == NULL)
        return 0.0f;
    return (float)SDL_strlen(text) * (float)BBC_FONT_CELL *
           (float)whole_scale(scale);
}

void bbc_text(SDL_Renderer *r, float x, float y, float scale, BbcColour c,
              const char *text)
{
    if (text == NULL || text[0] == '\0')
        return;
    SDL_Texture *texture = font_texture(r);
    if (texture == NULL)
        return;
    int s = whole_scale(scale);
    BbcColour p = bbc_map(c);
    SDL_SetTextureColorMod(texture, BBC_RGB[p][0], BBC_RGB[p][1],
                           BBC_RGB[p][2]);
    float cell = (float)(BBC_FONT_CELL * s);
    float px = floorf(x);
    float py = floorf(y);
    for (const unsigned char *ch = (const unsigned char *)text; *ch != '\0';
         ++ch, px += cell)
    {
        if (*ch == ' ')
            continue;
        int index = (*ch >= BBC_FONT_FIRST && *ch <= BBC_FONT_LAST)
                        ? *ch - BBC_FONT_FIRST
                        : FONT_COLS * FONT_ROWS - 1;
        SDL_FRect src = {(float)((index % FONT_COLS) * BBC_FONT_CELL),
                         (float)((index / FONT_COLS) * BBC_FONT_CELL),
                         (float)BBC_FONT_CELL, (float)BBC_FONT_CELL};
        SDL_FRect dst = {px, py, cell, cell};
        SDL_RenderTexture(r, texture, &src, &dst);
    }
}

void bbc_text_centered(SDL_Renderer *r, float cx, float y, float scale,
                       BbcColour c, const char *text)
{
    bbc_text(r, cx - bbc_text_width(text, scale) * 0.5f, y, scale, c, text);
}

void bbc_text_inverse(SDL_Renderer *r, float x, float y, float scale,
                      BbcColour bar, BbcColour ink, const char *text)
{
    float s = (float)whole_scale(scale);
    bbc_fill(r, bar, x - 4.0f * s, y - 2.0f * s,
             bbc_text_width(text, scale) + 8.0f * s, 12.0f * s);
    bbc_text(r, x, y, scale, ink, text);
}
