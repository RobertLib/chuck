#ifndef CHUCK_BBC_H
#define CHUCK_BBC_H

/*
 * Every pixel this game draws, drawn the way a BBC Micro in MODE 2 would.
 *
 * The frame is 800x552 logical pixels and a BBC pixel is four of them across
 * and two down, so the screen is a grid of 200 by 276 fat pixels and a 32-pixel
 * tile is eight of them wide and sixteen tall — the proportions of a MODE 2
 * sprite. Everything drawn through this header lands on that grid in one of
 * eight colours, opaque, and nothing else can: there is no alpha here, no
 * gradient, no glow, because the machine had none of them.
 *
 * What the machine did have is in here instead, and it is what every renderer
 * reaches for where it used to reach for light:
 *
 *  - **Dither.** Two colours alternated pixel by pixel. A BBC game's brown is
 *    red and yellow on a checkerboard and its grey is black and white, and the
 *    eye mixes them at arm's length. `bbc_dither` and `bbc_stipple`.
 *  - **Flashing colours.** Logical colours 8 to 15 alternated with their
 *    complements at the flash rate, for free, which is why every alarm on the
 *    machine blinked red and cyan. `bbc_flash`.
 *  - **The palette.** VDU 19 re-pointed a logical colour at a different
 *    physical one, instantly, everywhere on the screen. A fade to black was
 *    four palette changes, and an alarm turning the lights red was one.
 *    `bbc_palette_set` and `bbc_palette_dim`.
 *
 * The one exception to the grid is text: the system font is drawn at one
 * logical pixel to one font pixel, which keeps every line of the game the
 * width it was measured at. It still draws in the eight colours.
 *
 * Coordinates are logical pixels everywhere, and snapped on the way out — so a
 * layout written in logical pixels draws on the grid without knowing it is
 * there. A renderer that scrolls snaps its camera with `bbc_snap_x` /
 * `bbc_snap_y` first and hands the same offset to `bbc_set_pattern_origin`, or
 * the grid and the dither both crawl as the view moves.
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "bbc_art.h"

#define BBC_PX_W 4
#define BBC_PX_H 2

/* The machine's flash rate: 25 fiftieths of a second each way, which is what
 * *FX 9 and *FX 10 default to. */
#define BBC_FLASH_HALF_PERIOD 0.5f

/* ---- The frame -------------------------------------------------------- */

/*
 * Called once at the top of every frame. `time` drives the flashing colours;
 * `steady` is the player's reduced-motion switch, under which a flashing
 * colour holds its first half rather than blinking. Resets the palette and the
 * pattern origin, so nothing one screen set leaks into the next.
 */
void bbc_begin_frame(float time, bool steady);

/* ---- Grid ------------------------------------------------------------- */

float bbc_snap_x(float x);
float bbc_snap_y(float y);

/* The world point the dither patterns are anchored to, as the screen offset of
 * world (0, 0) — that is, minus the camera. Without it a checkerboard is
 * anchored to the glass and the floor appears to boil as the camera scrolls. */
void bbc_set_pattern_origin(float screen_x_of_world_0, float screen_y_of_world_0);

/* ---- Colour ----------------------------------------------------------- */

/* A logical colour through the current palette: what will actually land. */
BbcColour bbc_map(BbcColour logical);

void bbc_palette_set(BbcColour logical, BbcColour physical);
void bbc_palette_reset(void);

/*
 * The screen one, two or three steps toward black, done the way a BBC fade was
 * done: every logical colour re-pointed one step down a fixed ladder (white to
 * cyan to blue to black, yellow to red to black, and so on), never a blend. 0 is
 * the palette as it was; 3 or more is black.
 */
void bbc_palette_dim(int steps);

/* `c` and its complement, alternating at the flash rate. */
BbcColour bbc_flash(BbcColour c);

/* `a` and `b`, alternating at the flash rate. */
BbcColour bbc_flash_pair(BbcColour a, BbcColour b);

/* True in the first half of the flash cycle (and always, when steady). */
bool bbc_flash_on(void);

/* ---- Filled shapes ---------------------------------------------------- */

/* A rectangle with both edges snapped to the grid, never less than one BBC
 * pixel in either direction when it has any size at all. */
void bbc_fill(SDL_Renderer *r, BbcColour c, float x, float y, float w, float h);

/* `bw` by `bh` BBC pixels with their top-left at the grid point under (x, y). */
void bbc_block(SDL_Renderer *r, BbcColour c, float x, float y, int bw, int bh);

/* One BBC pixel. */
void bbc_plot(SDL_Renderer *r, BbcColour c, float x, float y);

/* A one-pixel outline around the rectangle `bbc_fill` would fill. */
void bbc_frame(SDL_Renderer *r, BbcColour c, float x, float y, float w,
               float h);

/* A line of BBC pixels from one point to the other, Bresenham on the grid. */
void bbc_line(SDL_Renderer *r, BbcColour c, float x0, float y0, float x1,
              float y1);

/* A filled circle of `radius` logical pixels. Round on the glass rather than on
 * the grid, which is to say twice as many rows as columns. */
void bbc_disc(SDL_Renderer *r, BbcColour c, float cx, float cy, float radius);

/* Its outline. */
void bbc_ring(SDL_Renderer *r, BbcColour c, float cx, float cy, float radius);

/* ---- Patterns --------------------------------------------------------- */

typedef enum
{
    BBC_DITHER_CHECK = 0, /* half and half, alternate pixels */
    BBC_DITHER_SPARSE,    /* one pixel in four of the ink */
    BBC_DITHER_DENSE,     /* three in four */
    BBC_DITHER_ROWS,      /* every other row */
    BBC_DITHER_COLS,      /* every other column */
    BBC_DITHER_DOTS,      /* one pixel in eight: distant lights, a starfield */
    BBC_DITHER_COUNT
} BbcDither;

/* Whether the grid cell (bx, by) — in BBC pixels, measured from the pattern
 * origin — carries the ink in `pattern`. */
bool bbc_pattern_ink(BbcDither pattern, int bx, int by);

/* `base` everywhere in the rectangle, `ink` where the pattern says. */
void bbc_dither(SDL_Renderer *r, BbcColour base, BbcColour ink,
                BbcDither pattern, float x, float y, float w, float h);

/* `ink` where the pattern says and nothing anywhere else, over whatever is
 * already there. A stipple of black over a scene is how a BBC game shaded
 * what sat behind a menu, and it is the only kind of "dimming" in this tree. */
void bbc_stipple(SDL_Renderer *r, BbcColour ink, BbcDither pattern, float x,
                 float y, float w, float h);

/* ---- Sprites ---------------------------------------------------------- */

/*
 * A sprite with its top-left at (x, y), facing right for `dir >= 0` and
 * mirrored for `dir < 0`. `slots` fills '1'..'4' and may be NULL if the sprite
 * uses none.
 */
void bbc_sprite(SDL_Renderer *r, const BbcSprite *sprite, float x, float y,
                int dir, const BbcColour *slots);

/* The same at `scale` BBC pixels to a sprite pixel. */
void bbc_sprite_scaled(SDL_Renderer *r, const BbcSprite *sprite, float x,
                       float y, int dir, const BbcColour *slots, int scale);

/* Every opaque pixel in `c`: a silhouette, a hit flash, a figure seen against
 * a lit window. */
void bbc_sprite_solid(SDL_Renderer *r, const BbcSprite *sprite, float x,
                      float y, int dir, BbcColour c);

/* A sprite stood on a point: centred on `cx`, its bottom row on `foot_y`. This
 * is how a figure is placed from its collision box, which knows where its feet
 * are and not where its hat is. */
void bbc_sprite_at_feet(SDL_Renderer *r, const BbcSprite *sprite, float cx,
                        float foot_y, int dir, const BbcColour *slots);

/* Width and height of a sprite in logical pixels. */
float bbc_sprite_px_w(const BbcSprite *sprite);
float bbc_sprite_px_h(const BbcSprite *sprite);

/* ---- Text ------------------------------------------------------------- */

/*
 * A line of the system font with its top-left at (x, y), at a whole `scale`
 * (1, 2, 3 or 4; anything else is rounded). Eight logical pixels to a
 * character at scale 1.
 */
void bbc_text(SDL_Renderer *r, float x, float y, float scale, BbcColour c,
              const char *text);

float bbc_text_width(const char *text, float scale);

/* The same line centred on `cx`. */
void bbc_text_centered(SDL_Renderer *r, float cx, float y, float scale,
                       BbcColour c, const char *text);

/* Inverse video: a bar of `bar` one character cell taller and wider than the
 * text, and the text in `ink` on it. The machine's highlight. */
void bbc_text_inverse(SDL_Renderer *r, float x, float y, float scale,
                      BbcColour bar, BbcColour ink, const char *text);

#endif /* CHUCK_BBC_H */
