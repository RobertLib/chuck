#ifndef CHUCK_RENDER_SPRITE_H
#define CHUCK_RENDER_SPRITE_H

/*
 * The drawing vocabulary the sector's renderers share.
 *
 * Two things live here. The first is the handful of wrappers every draw call
 * goes through — a rect, a coloured rect, a line of the 8x8 debug font — so no
 * renderer sets a draw colour by hand and forgets to put it back.
 *
 * The second is the reason this is a file rather than a convenience: the
 * `sprite_*` family is how a figure is built. A body drawn out of plain
 * rectangles reads as assembled however well each rectangle is shaded, and the
 * corners are the tell — four of them on every part. These take a pixel or two
 * off, run the outline along the same taper a pixel further out, and light the
 * result from the one direction the whole game is lit from. That is why the
 * cast in [render_figures.c](render_figures.c) gained the treatment all at
 * once rather than being hand-shaded figure by figure, and why anything new
 * joining the cast has to come through here to look like it belongs.
 *
 * All of it was `static` inside game_render.c, which is what kept the cast,
 * the props and the HUD in one six-thousand-line file: nothing could move out
 * without taking a copy of the vocabulary with it, and two copies of a
 * lighting rule is two answers to how a figure is lit.
 */

#include <SDL3/SDL.h>

#include "fx.h"

/* The sprite outline: a step warmer than FX_INK's void so a figure's edge
 * reads as drawn rather than as a hole in the room. */
extern const SDL_Color COL_OUTLINE;

void set_color(SDL_Renderer *r, SDL_Color c);

void set_rgba(SDL_Renderer *r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);

void fill_rect(SDL_Renderer *r, float x, float y, float w, float h);

void color_rect(SDL_Renderer *r, SDL_Color c, float x, float y, float w, float h);

/*
 * `SDL_RenderDebugText` is an 8x8 bitmap, so it is drawn at scale 1.0 or a
 * whole multiple of it and never in between: any other scale resamples the
 * glyphs, and a line of mushy type cheapens a screen faster than anything
 * else on it. If a row does not fit at 1.0, cut words, not scale.
 */
void draw_text(SDL_Renderer *r, float x, float y, float scale,
               Uint8 cr, Uint8 cg, Uint8 cb, const char *text);

float draw_text_width(const char *text, float scale);

/* ---- The figure vocabulary ------------------------------------------ */

/* A plain rectangle in sprite space, mirrored for whichever way the figure is
 * facing, so a pose is authored once and drawn both ways. */
void sprite_rect(SDL_Renderer *r, float bx, float by, float sprite_w, int dir,
                 float lx, float ly, float w, float h, SDL_Color c);

/* The same mirroring, for a single point. */
float sprite_point_x(float bx, float sprite_w, int dir, float lx);

/* A lit solid: the garment, the crown the ceiling reaches, the underside
 * dropped into shade, and one rim pixel down the leading flank. The trailing
 * flank is deliberately left alone — it sits against the sprite's own outline,
 * where a second dark column reads as a thicker outline rather than as a
 * surface turning away. */
void sprite_form(SDL_Renderer *r, float bx, float by, float sprite_w,
                 int dir, float lx, float ly, float w, float h,
                 SDL_Color base);

/* The same, for anything laid over a form — hair, a helmet, a cap, the shade
 * along a jaw. A rectangle of hair puts the corners of the head straight back,
 * so it follows the form's own taper; the top and bottom are given separately
 * because a body is not symmetrical about its waist. */
void sprite_mass(SDL_Renderer *r, float bx, float by, float sprite_w,
                 int dir, float lx, float ly, float w, float h,
                 SDL_Color c, int top, int bottom);

/* A form with the outline run along the same taper a pixel further out, which
 * is the part that matters: a rounded fill inside a square outline is still a
 * box with something drawn in it. */
void sprite_body(SDL_Renderer *r, float bx, float by, float sprite_w,
                 int dir, float lx, float ly, float w, float h,
                 SDL_Color base, SDL_Color outline, int top, int bottom);

void sprite_segment_shifted(SDL_Renderer *r, float bx, float by,
                            float sprite_w, int dir,
                            float lx1, float ly1, float lx2, float ly2,
                            int thickness, float shift, SDL_Color c);

void sprite_segment(SDL_Renderer *r, float bx, float by, float sprite_w, int dir,
                    float lx1, float ly1, float lx2, float ly2,
                    int thickness, SDL_Color c);

/* A limb as a lit cylinder — outline, shaded underside, garment, one lit pixel
 * along the top. Parts narrow enough that a chamfer would eat them whole stay
 * rectangular and come through here. */
void sprite_limb_segment(SDL_Renderer *r, float bx, float by,
                         float sprite_w, int dir,
                         float lx1, float ly1, float lx2, float ly2,
                         SDL_Color fill);

void sprite_shoe(SDL_Renderer *r, float bx, float by, float sprite_w,
                 int dir, float ankle_x, float ankle_y, SDL_Color boot);

#endif /* CHUCK_RENDER_SPRITE_H */
