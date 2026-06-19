/*
 * The shared drawing vocabulary. See [render_sprite.h](render_sprite.h) for
 * why a figure is built out of tapered, lit forms rather than out of
 * rectangles, and why that has to be one implementation rather than one per
 * renderer.
 */

#include "render_sprite.h"

#include <math.h>

/* The sprite outline: a step warmer than FX_INK's void so a figure's edge
 * reads as drawn rather than as a hole in the room. The one colour this file
 * owns; everything else comes from fx.h or the level's theme. */
const SDL_Color COL_OUTLINE = {13, 18, 27, 255};

void set_color(SDL_Renderer *r, SDL_Color c)
{
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void set_rgba(SDL_Renderer *r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha)
{
  SDL_SetRenderDrawColor(r, red, green, blue, alpha);
}

void fill_rect(SDL_Renderer *r, float x, float y, float w, float h)
{
  SDL_FRect rect = {x, y, w, h};
  SDL_RenderFillRect(r, &rect);
}

void color_rect(SDL_Renderer *r, SDL_Color c, float x, float y, float w, float h)
{
  set_color(r, c);
  fill_rect(r, x, y, w, h);
}

void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      Uint8 cr, Uint8 cg, Uint8 cb, const char *text)
{
  SDL_SetRenderScale(r, scale, scale);
  SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
  SDL_RenderDebugText(r, x / scale, y / scale, text);
  SDL_SetRenderScale(r, 1.0f, 1.0f);
}

float draw_text_width(const char *text, float scale)
{
  return (float)SDL_strlen(text) *
         SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

/* Draw a local sprite rectangle, mirrored inside the entity collision width. */
void sprite_rect(SDL_Renderer *r, float bx, float by, float sprite_w, int dir,
                        float lx, float ly, float w, float h, SDL_Color c)
{
  float x = (dir >= 0) ? bx + lx : bx + sprite_w - lx - w;
  color_rect(r, c, floorf(x), floorf(by + ly), w, h);
}

/* Where a point in sprite-local space lands on screen once the sprite has been
   mirrored. Anything that is not a rect — a glow, a spark — needs this, because
   it cannot be flipped by swapping its own left and right edges. */
float sprite_point_x(float bx, float sprite_w, int dir, float lx)
{
  return (dir >= 0) ? bx + lx : bx + sprite_w - lx;
}

/* A body block in sprite-local space, shaded as a solid rather than filled
   flat. Everything a figure is built out of goes through here, so the whole
   cast is lit by the same ceiling. */
void sprite_form(SDL_Renderer *r, float bx, float by, float sprite_w,
                        int dir, float lx, float ly, float w, float h,
                        SDL_Color base)
{
  float x = (dir >= 0) ? bx + lx : bx + sprite_w - lx - w;
  fx_form_block(r, floorf(x), floorf(by + ly), w, h, fx_ramp(base), dir);
}

/* A flat-coloured tapered mass in sprite-local space. */
void sprite_mass(SDL_Renderer *r, float bx, float by, float sprite_w,
                        int dir, float lx, float ly, float w, float h,
                        SDL_Color c, int top, int bottom)
{
  float x = (dir >= 0) ? bx + lx : bx + sprite_w - lx - w;
  fx_mass(r, c, floorf(x), floorf(by + ly), w, h, top, bottom);
}

/*
 * A body part, outline and all, with its corners taken off.
 *
 * The outline follows the same taper one pixel further out, so the silhouette
 * itself loses its corners instead of gaining a rounded fill inside a square
 * outline — which would read as a box with something drawn in it. Every part
 * big enough to have corners goes through here; the narrow ones a chamfer would
 * eat whole (a forearm, a trouser leg) stay on `sprite_form`.
 */
void sprite_body(SDL_Renderer *r, float bx, float by, float sprite_w,
                        int dir, float lx, float ly, float w, float h,
                        SDL_Color base, SDL_Color outline, int top, int bottom)
{
  float x = floorf((dir >= 0) ? bx + lx : bx + sprite_w - lx - w);
  float y = floorf(by + ly);

  fx_mass(r, outline, x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f,
          top + 1, bottom + 1);
  fx_form_mass(r, x, y, w, h, fx_ramp(base), dir, top, bottom);
}

/* Thick local-space line used for jointed pixel-art limbs. `shift` slides the
   run of lines along the segment's normal, sign-corrected so a positive value
   always lands toward the top of the screen — a limb is a cylinder, and the
   highlight belongs on the side the ceiling light reaches whichever way the
   limb happens to point. */
void sprite_segment_shifted(SDL_Renderer *r, float bx, float by,
                                   float sprite_w, int dir,
                                   float lx1, float ly1, float lx2, float ly2,
                                   int thickness, float shift, SDL_Color c)
{
  float x1 = (dir >= 0) ? bx + lx1 : bx + sprite_w - lx1;
  float x2 = (dir >= 0) ? bx + lx2 : bx + sprite_w - lx2;
  float y1 = by + ly1;
  float y2 = by + ly2;
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length = sqrtf(dx * dx + dy * dy);
  float nx = length > 0.001f ? -dy / length : 1.0f;
  float ny = length > 0.001f ? dx / length : 0.0f;
  float half = (float)(thickness - 1) * 0.5f;

  if (ny > 0.0f)
    shift = -shift;

  set_color(r, c);
  for (int i = 0; i < thickness; ++i)
  {
    float offset = (float)i - half + shift;
    SDL_RenderLine(r,
                   floorf(x1 + nx * offset), floorf(y1 + ny * offset),
                   floorf(x2 + nx * offset), floorf(y2 + ny * offset));
  }
}

void sprite_segment(SDL_Renderer *r, float bx, float by, float sprite_w, int dir,
                           float lx1, float ly1, float lx2, float ly2,
                           int thickness, SDL_Color c)
{
  sprite_segment_shifted(r, bx, by, sprite_w, dir, lx1, ly1, lx2, ly2,
                         thickness, 0.0f, c);
}

/* An arm or a leg, as a rounded limb: outline, the shaded underside, the
   garment, and one lit pixel along the top. Four passes over the same line,
   and the difference between a limb and a stroke. */
void sprite_limb_segment(SDL_Renderer *r, float bx, float by,
                                float sprite_w, int dir,
                                float lx1, float ly1, float lx2, float ly2,
                                SDL_Color fill)
{
  FxRamp ramp = fx_ramp(fill);

  sprite_segment(r, bx, by, sprite_w, dir, lx1, ly1, lx2, ly2, 5, COL_OUTLINE);
  sprite_segment(r, bx, by, sprite_w, dir, lx1, ly1, lx2, ly2, 4, ramp.dark);
  sprite_segment_shifted(r, bx, by, sprite_w, dir, lx1, ly1, lx2, ly2,
                         3, 0.5f, fill);
  sprite_segment_shifted(r, bx, by, sprite_w, dir, lx1, ly1, lx2, ly2,
                         1, 1.5f, ramp.lit);
}

/*
 * The end of a leg, as a shoe.
 *
 * Three pixels of heel, sole and a lit toe cap. It is a small thing to spend
 * geometry on, but a limb that ends in a flat two-pixel bar reads as a stick,
 * and the toe is what points the figure in a direction.
 */
void sprite_shoe(SDL_Renderer *r, float bx, float by, float sprite_w,
                        int dir, float ankle_x, float ankle_y, SDL_Color boot)
{
  FxRamp ramp = fx_ramp(boot);

  /* The ankle is narrower than the sole under it, so the top row steps in. */
  sprite_mass(r, bx, by, sprite_w, dir,
              ankle_x - 1.5f, ankle_y - 1.0f, 7.0f, 4.0f, COL_OUTLINE, 1, 0);
  sprite_rect(r, bx, by, sprite_w, dir,
              ankle_x - 0.5f, ankle_y, 5.0f, 2.0f, boot);
  /* Heel behind the ankle, toe cap catching the light in front of it. Boots
     drawn in a flat near-black instead merge into one mass at the bottom of
     the figure, which is what makes a pair of legs read as a plinth. */
  sprite_rect(r, bx, by, sprite_w, dir,
              ankle_x - 0.5f, ankle_y - 1.0f, 2.0f, 2.0f, ramp.dark);
  sprite_rect(r, bx, by, sprite_w, dir,
              ankle_x + 2.5f, ankle_y, 2.0f, 1.0f, ramp.lit);
}
