/*
 * The cast. See [render_figures.h](render_figures.h) for what is in here and
 * why it is one module.
 *
 * Everything below is built out of [render_sprite.h](render_sprite.h)'s
 * tapered, lit forms rather than out of rectangles, and reads nothing but the
 * simulation state it is handed — a renderer that could change what a figure
 * does would be a renderer the tests cannot vouch for.
 */

#include "render_figures.h"

#include <math.h>

#include "chuck_pose.h"
#include "fx.h"
#include "game_config.h"
#include "render_chuck.h"
#include "render_sprite.h"

/*
 * What the cast is made of: fx.h's material constants, so the man in the
 * sector, the man in the manual, the man on the title screen and the man in
 * the cutscene are the same man. This block used to carry near-miss copies of
 * those colours — a jacket a few units off FX_HERO, a fifth skin tone — under
 * a comment claiming they were identical, which is exactly how one cast
 * drifts into five. Only the hair's lit step stays local: it is the one step
 * fx_ramp cannot derive (a warm brown lifted toward the skin rather than
 * toward the lamp), and it has one name here.
 */
static const SDL_Color PLAYER_HAIR_LT = {102, 62, 42, 255};

/*
 * A frag grenade, in a hand mid-throw, lying on the floor as a pickup, and in
 * the HUD's carry row.
 *
 * Every other pickup in the building is a lit object — the medkit a case with a
 * crown and an underside, the pistol and the launcher shaded down their length
 * — and this was a flat olive box with a lighter stripe on it. It is a lit egg
 * now, inside exactly the box it always filled, because the HUD's carry row
 * measures that box (`HUD_GRENADE_INK_W`): chamfered to an oval, the crown lit
 * and the underside dropped into shade, the segmented body a frag has always
 * been drawn with, a glint where the lamp catches the upper curve, the brass
 * spoon running from the fuse head down the leading flank, and the ring of the
 * pin at the other side of the head. Olive, brass and a ring is the whole of
 * what tells it from a flash charge at a glance, and the flash charge is drawn
 * as the other half of the palette on purpose.
 */
void draw_grenade(SDL_Renderer *r, float x, float y, float fuse)
{
  FxRamp olive = fx_ramp((SDL_Color){68, 92, 61, 255});
  FxRamp brass = fx_ramp((SDL_Color){169, 144, 85, 255});

  /* The body: an oval, lit from the ceiling. */
  fx_mass(r, COL_OUTLINE, x - 1.0f, y + 1.0f, 12.0f, 10.0f, 2, 2);
  fx_form_mass(r, x, y + 2.0f, 10.0f, 8.0f, olive, 1, 2, 2);
  /* The segments, cut into it as grooves in the shade colour. */
  color_rect(r, olive.dark, x + 3.0f, y + 3.0f, 1.0f, 6.0f);
  color_rect(r, olive.dark, x + 6.0f, y + 3.0f, 1.0f, 6.0f);
  color_rect(r, olive.dark, x + 1.0f, y + 6.0f, 8.0f, 1.0f);
  /* The lamp on the upper curve of the near segment. */
  color_rect(r, olive.lit, x + 1.0f, y + 4.0f, 2.0f, 1.0f);
  color_rect(r, fx_mix(olive.lit, FX_CREAM, 0.45f), x + 1.0f, y + 3.0f, 1.0f,
             1.0f);

  /* The fuse head on top, in steel. */
  color_rect(r, COL_OUTLINE, x + 2.0f, y - 1.0f, 6.0f, 4.0f);
  color_rect(r, FX_STEEL, x + 3.0f, y, 4.0f, 2.0f);
  color_rect(r, FX_STEEL_LT, x + 3.0f, y, 4.0f, 1.0f);
  /* The spoon, out of the head and down the leading flank. */
  color_rect(r, COL_OUTLINE, x + 7.0f, y - 1.0f, 4.0f, 3.0f);
  color_rect(r, brass.base, x + 7.0f, y, 2.0f, 1.0f);
  color_rect(r, brass.lit, x + 7.0f, y, 1.0f, 1.0f);
  color_rect(r, brass.base, x + 8.0f, y + 1.0f, 2.0f, 1.0f);
  color_rect(r, brass.base, x + 9.0f, y + 2.0f, 1.0f, 5.0f);
  color_rect(r, brass.dark, x + 9.0f, y + 6.0f, 1.0f, 1.0f);
  /* The pin's ring, at the other side of the head. */
  color_rect(r, COL_OUTLINE, x, y - 1.0f, 3.0f, 3.0f);
  color_rect(r, FX_STEEL_LT, x + 1.0f, y - 1.0f, 1.0f, 1.0f);
  color_rect(r, FX_STEEL_LT, x, y, 1.0f, 1.0f);
  color_rect(r, FX_STEEL, x + 1.0f, y + 1.0f, 1.0f, 1.0f);

  if (fuse > 0.0f && ((int)(fuse * 14.0f) & 1) == 0)
  {
    color_rect(r, (SDL_Color){255, 235, 128, 255}, x + 10.0f, y - 2.0f, 2.0f, 2.0f);
    color_rect(r, FX_RED, x + 11.0f, y - 1.0f, 2.0f, 2.0f);
  }
}

/*
 * A bolt in the air.
 *
 * Six pixels of plated steel and an outline, and it is drawn small on purpose:
 * the player has to be able to tell it from a grenade at a glance, because one
 * of the two is about to go off. Nothing about it is animated — it is in the
 * air for well under a second and the noise it makes is the event, not the
 * flight.
 */
void draw_decoy(SDL_Renderer *r, float x, float y)
{
  color_rect(r, COL_OUTLINE, x - 1.0f, y - 1.0f,
             (float)DECOY_W + 2.0f, (float)DECOY_H + 2.0f);
  color_rect(r, FX_STEEL, x, y, (float)DECOY_W, (float)DECOY_H);
  color_rect(r, FX_STEEL_LT, x, y, (float)DECOY_W, 2.0f);
}

/*
 * A flash charge — in the air, and lying on the floor as a pickup.
 *
 * It has to be told from a grenade at a glance, because one of the two is about
 * to kill whoever is standing next to it. So it is the other half of the
 * palette and the other half of the geometry: a steel *cylinder* rather than an
 * olive egg, shaded across its width the way a can is — a specular stripe near
 * the lit side, the far side falling into shade — with the white band round
 * its middle, a straight rolled seam round the base where a frag has grooves, a
 * pull-ring on the right of its cap where a frag has its ring on the left, and
 * a cyan tell-tale in the cap that strobes as the fuse runs down. Same box as
 * the grenade (`HUD_FLASH_INK_W` measures it), opposite everything else.
 */
void draw_flashbang(SDL_Renderer *r, float x, float y, float fuse)
{
  SDL_Color edge = fx_mix(FX_STEEL, FX_STEEL_DK, 0.55f);
  SDL_Color shade = fx_mix(FX_STEEL_DK, FX_INK, 0.30f);
  SDL_Color band_shade = fx_mix(FX_CREAM, FX_STEEL, 0.50f);
  /* Across the can, left to right: the rim turning away, the lit side, the
     specular stripe, the face, and the far side in shade. */
  static const float COLUMN_MIX[10] = {0.0f, 0.6f, 1.0f, 0.6f, 0.3f,
                                       0.3f, 0.3f, -0.4f, -1.0f, -1.0f};

  fx_mass(r, COL_OUTLINE, x - 1.0f, y + 1.0f, 12.0f, 10.0f, 1, 1);
  for (int col = 0; col < 10; ++col)
  {
    float m = COLUMN_MIX[col];
    SDL_Color body = col == 0 ? edge
                     : m >= 0.0f ? fx_mix(FX_STEEL, FX_PALE, m * 0.75f)
                                 : fx_mix(FX_STEEL, shade, -m);
    SDL_Color band = m >= 0.0f ? fx_mix(band_shade, FX_CREAM, 0.5f + m * 0.5f)
                               : fx_mix(band_shade, FX_STEEL, -m * 0.5f);
    float top = (col == 0 || col == 9) ? 3.0f : 2.0f;
    float bottom = (col == 0 || col == 9) ? 9.0f : 10.0f;
    color_rect(r, body, x + (float)col, y + top, 1.0f, bottom - top);
    color_rect(r, band, x + (float)col, y + 5.0f, 1.0f, 2.0f);
  }
  /* The cap's lit rim, and its seam, and the base in shade. */
  color_rect(r, FX_STEEL_LT, x + 1.0f, y + 2.0f, 7.0f, 1.0f);
  color_rect(r, shade, x + 1.0f, y + 3.0f, 8.0f, 1.0f);
  color_rect(r, shade, x + 1.0f, y + 9.0f, 8.0f, 1.0f);
  /* A rolled seam round the base, where a frag has its grooves: straight
     across a can, where the grooves cross an egg. */
  color_rect(r, fx_mix(FX_STEEL, shade, 0.6f), x + 1.0f, y + 8.0f, 8.0f, 1.0f);

  /* The fuse head on the cap, with the tell-tale in it. */
  color_rect(r, COL_OUTLINE, x + 2.0f, y - 1.0f, 6.0f, 4.0f);
  color_rect(r, FX_STEEL_DK, x + 3.0f, y, 4.0f, 2.0f);
  color_rect(r, FX_STEEL, x + 3.0f, y, 4.0f, 1.0f);
  color_rect(r, FX_CYAN_DK, x + 4.0f, y + 1.0f, 1.0f, 1.0f);
  /* The pull-ring, on the right of the cap. */
  color_rect(r, COL_OUTLINE, x + 7.0f, y - 1.0f, 4.0f, 3.0f);
  color_rect(r, FX_STEEL_LT, x + 8.0f, y - 1.0f, 1.0f, 1.0f);
  color_rect(r, FX_STEEL_LT, x + 9.0f, y, 1.0f, 1.0f);
  color_rect(r, FX_STEEL, x + 8.0f, y + 1.0f, 1.0f, 1.0f);

  if (fuse > 0.0f && ((int)(fuse * 18.0f) & 1) == 0)
  {
    color_rect(r, FX_CYAN, x + 4.0f, y + 1.0f, 1.0f, 1.0f);
    color_rect(r, FX_CREAM, x + 10.0f, y - 2.0f, 2.0f, 2.0f);
    color_rect(r, FX_CYAN, x + 11.0f, y - 1.0f, 2.0f, 2.0f);
  }
}

/*
 * A sheet off the docket, lying where it fell out of a case.
 *
 * Paper rather than kit, so it is drawn as paper: a pale leaf with a corner
 * turned, a clip at the head of it, ruled lines and the red stamp that makes it
 * Meridian's rather than the building's. It is the one pickup that is not a
 * weapon, a heart or a door, and it has to read that way from across a room or
 * a player will walk past it assuming they are full up on whatever it is.
 *
 * It is lit like everything else in the room, which a flat cream rectangle was
 * not: bright where the lamp reaches its top edge and cooling down the sheet,
 * the turned corner showing the back of the leaf with its shadow under it, and
 * the clip catching the light. Same footprint as it always had.
 */
void draw_evidence_pickup(SDL_Renderer *r, float x, float y)
{
  SDL_Color paper_mid = fx_mix(FX_CREAM, FX_PALE, 0.45f);
  SDL_Color back = fx_mix(FX_PALE, FX_STEEL_LT, 0.35f);

  color_rect(r, COL_OUTLINE, x + 2.0f, y + 1.0f, 12.0f, 15.0f);
  /* The sheet, lit from its top edge down. */
  color_rect(r, FX_CREAM, x + 3.0f, y + 2.0f, 10.0f, 13.0f);
  color_rect(r, paper_mid, x + 3.0f, y + 9.0f, 10.0f, 3.0f);
  color_rect(r, FX_PALE, x + 3.0f, y + 12.0f, 10.0f, 3.0f);
  /* The right edge curling a little off the floor, into its own shade. */
  color_rect(r, paper_mid, x + 12.0f, y + 6.0f, 1.0f, 3.0f);
  color_rect(r, FX_PALE, x + 12.0f, y + 9.0f, 1.0f, 3.0f);
  color_rect(r, back, x + 12.0f, y + 12.0f, 1.0f, 3.0f);
  /* The turned corner: the back of the leaf, the fold's lit edge, and the
     shadow it throws on the sheet under it. */
  color_rect(r, COL_OUTLINE, x + 11.0f, y + 2.0f, 2.0f, 1.0f);
  color_rect(r, COL_OUTLINE, x + 12.0f, y + 3.0f, 1.0f, 1.0f);
  color_rect(r, back, x + 10.0f, y + 2.0f, 1.0f, 3.0f);
  color_rect(r, back, x + 11.0f, y + 3.0f, 1.0f, 2.0f);
  color_rect(r, FX_CREAM, x + 9.0f, y + 2.0f, 1.0f, 1.0f);
  color_rect(r, paper_mid, x + 10.0f, y + 5.0f, 3.0f, 1.0f);
  color_rect(r, paper_mid, x + 12.0f, y + 4.0f, 1.0f, 1.0f);
  /* Ruled lines and the contractor's stamp. */
  color_rect(r, FX_LABEL, x + 5.0f, y + 6.0f, 5.0f, 1.0f);
  color_rect(r, FX_LABEL, x + 5.0f, y + 8.0f, 6.0f, 1.0f);
  color_rect(r, fx_mix(FX_LABEL, FX_PALE, 0.4f), x + 5.0f, y + 10.0f, 3.0f,
             1.0f);
  color_rect(r, FX_RED_DK, x + 8.0f, y + 10.0f, 4.0f, 2.0f);
  color_rect(r, fx_mix(FX_RED_DK, FX_CREAM, 0.30f), x + 9.0f, y + 10.0f, 2.0f,
             1.0f);
  /* The clip at the head of the sheet, catching the lamp. */
  color_rect(r, COL_OUTLINE, x + 4.0f, y + 1.0f, 3.0f, 5.0f);
  color_rect(r, FX_STEEL_LT, x + 5.0f, y + 2.0f, 1.0f, 3.0f);
  color_rect(r, FX_PALE, x + 5.0f, y + 2.0f, 1.0f, 1.0f);
}

/*
 * The light a shot throws.
 *
 * A muzzle flash drawn as two bright rects is a decal: the brightest thing in
 * the frame lights nothing around it, and the eye reads it as a sticker on the
 * gun. One glow at the muzzle puts the shot back in the room — and because it
 * lasts two frames it costs nothing anyone will notice.
 */
static void draw_muzzle_flash(SDL_Renderer *r, float bx, float by,
                              float sprite_w, int dir, float lx, float ly,
                              SDL_Color tint)
{
  /* Sprite space resolved here, the light itself in `fx_muzzle_glow`, which is
     what the cutscene's shots draw with as well — see the note beside it. */
  fx_muzzle_glow(r, sprite_point_x(bx, sprite_w, dir, lx), by + ly, 1.0f, tint);
}

/*
 * A body built out of several masses, outlined as one.
 *
 * `sprite_body` outlines each part as it draws it, which is right for a head on
 * a torso — the chin is supposed to be a line — and wrong for an animal, whose
 * chest, barrel and haunch are one hide: drawn part by part, each outline cuts
 * through the fill of the part before it and the dog comes out as three boxes
 * glued together. So the outlines of every part go down first and the fills
 * over all of them afterwards, and the only ink left is the silhouette.
 */
static void sprite_mass_outline(SDL_Renderer *r, float bx, float by,
                                float sprite_w, int dir, float lx, float ly,
                                float w, float h, int top, int bottom)
{
  sprite_mass(r, bx, by, sprite_w, dir, lx - 1.0f, ly - 1.0f, w + 2.0f,
              h + 2.0f, COL_OUTLINE, top + 1, bottom + 1);
}

static void sprite_mass_form(SDL_Renderer *r, float bx, float by,
                             float sprite_w, int dir, float lx, float ly,
                             float w, float h, SDL_Color base, int top,
                             int bottom)
{
  float x = floorf((dir >= 0) ? bx + lx : bx + sprite_w - lx - w);
  fx_form_mass(r, x, floorf(by + ly), w, h, fx_ramp(base), dir, top, bottom);
}

/*
 * The crew's carbine.
 *
 * Every rifle in the building came out of Meridian's flight cases, and the
 * cutscenes arm the twelve with one for exactly the reason given there: a
 * low-ready rifle makes a captor unmistakable at pixel scale. In play the same
 * men used to walk their floors empty-handed and produce a pistol only for the
 * aim — so a patrolling guard read as a man in a green shirt, and the one
 * silhouette that says "armed" arrived at the same moment as the shot. It is
 * carried now, and the pose it is carried in is the telegraph: slung while he
 * talks, low and pointing at the floor while he walks, and brought up along
 * the line the round will take while he aims.
 *
 * Laid along a line in sprite space from the butt to the muzzle, so one drawing
 * serves every angle it is held at. The magazine and the grip hang from the
 * side of the gun that is *under* it — down for a rifle pointed forward,
 * forward for one pointed at the ceiling — which is what keeps it a rifle
 * rather than a stick at every angle. Material rather than semantic colour:
 * blued steel a step off the dark, polymer furniture a step under that, and one
 * lit pixel along the top of the receiver, because the ceiling lights a gun the
 * same way it lights the man holding it.
 */
static const SDL_Color CARBINE_STEEL = {43, 48, 49, 255};
static const SDL_Color CARBINE_STEEL_LT = {88, 97, 96, 255};
static const SDL_Color CARBINE_FURNITURE = {31, 35, 30, 255};

static void draw_carbine(SDL_Renderer *r, float x, float y, float sprite_w,
                         int dir, float butt_x, float butt_y,
                         float muzzle_x, float muzzle_y)
{
  float dx = muzzle_x - butt_x;
  float dy = muzzle_y - butt_y;
  float length = sqrtf(dx * dx + dy * dy);
  if (length < 4.0f)
    return;
  float ux = dx / length;
  float uy = dy / length;
  /* Under the gun, in sprite space: (0, 1) for a rifle pointed forward. */
  float nx = -uy;
  float ny = ux;
  float rx = butt_x + dx * 0.62f;
  float ry = butt_y + dy * 0.62f;
  float gx = butt_x + dx * 0.32f;
  float gy = butt_y + dy * 0.32f;
  float mx = butt_x + dx * 0.48f;
  float my = butt_y + dy * 0.48f;

  /* Every outline first, so the parts join without ink between them. */
  sprite_segment(r, x, y, sprite_w, dir, rx, ry, muzzle_x, muzzle_y, 3,
                 COL_OUTLINE);
  sprite_segment(r, x, y, sprite_w, dir, butt_x, butt_y, rx, ry, 4,
                 COL_OUTLINE);
  sprite_segment(r, x, y, sprite_w, dir, mx, my, mx + nx * 4.0f - ux,
                 my + ny * 4.0f - uy, 4, COL_OUTLINE);
  sprite_segment(r, x, y, sprite_w, dir, gx, gy, gx + nx * 3.0f - ux,
                 gy + ny * 3.0f - uy, 3, COL_OUTLINE);

  sprite_segment(r, x, y, sprite_w, dir, butt_x, butt_y,
                 butt_x + dx * 0.30f, butt_y + dy * 0.30f, 2,
                 CARBINE_FURNITURE);
  sprite_segment(r, x, y, sprite_w, dir, butt_x + dx * 0.28f,
                 butt_y + dy * 0.28f, rx, ry, 2, CARBINE_STEEL);
  sprite_segment(r, x, y, sprite_w, dir, rx, ry, muzzle_x, muzzle_y, 1,
                 CARBINE_STEEL);
  sprite_segment(r, x, y, sprite_w, dir, mx + nx, my + ny,
                 mx + nx * 3.5f - ux, my + ny * 3.5f - uy, 2,
                 CARBINE_FURNITURE);
  sprite_segment(r, x, y, sprite_w, dir, gx + nx, gy + ny,
                 gx + nx * 2.5f - ux, gy + ny * 2.5f - uy, 1,
                 CARBINE_FURNITURE);
  sprite_segment_shifted(r, x, y, sprite_w, dir, butt_x + dx * 0.30f,
                         butt_y + dy * 0.30f, rx, ry, 1, 1.0f,
                         CARBINE_STEEL_LT);
}

/* A hand closed on something, which is two pixels of skin inside an outline —
   and without it every arm that holds a weapon ends in the weapon. */
static void draw_closed_hand(SDL_Renderer *r, float x, float y,
                             float sprite_w, int dir, float hx, float hy,
                             SDL_Color skin)
{
  sprite_rect(r, x, y, sprite_w, dir, hx - 1.5f, hy - 1.5f, 4.0f, 4.0f,
              COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir, hx - 0.5f, hy - 0.5f, 2.0f, 2.0f, skin);
}

static void draw_bazooka_weapon(SDL_Renderer *r, float x, float y,
                                float sprite_w, int dir,
                                float lx, float ly, bool firing)
{
  float recoil = firing ? -2.0f : 0.0f;
  sprite_rect(r, x, y, sprite_w, dir, lx - 3.0f + recoil, ly + 1.0f,
              24.0f, 8.0f, COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir, lx - 2.0f + recoil, ly + 2.0f,
              21.0f, 6.0f, (SDL_Color){51, 75, 48, 255});
  sprite_rect(r, x, y, sprite_w, dir, lx + recoil, ly + 3.0f,
              17.0f, 2.0f, (SDL_Color){106, 135, 79, 255});
  sprite_rect(r, x, y, sprite_w, dir, lx + 18.0f + recoil, ly,
              5.0f, 10.0f, (SDL_Color){139, 151, 111, 255});
  sprite_rect(r, x, y, sprite_w, dir, lx + 4.0f + recoil, ly + 8.0f,
              5.0f, 6.0f, COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir, lx + 6.0f + recoil, ly + 8.0f,
              3.0f, 5.0f, (SDL_Color){73, 60, 43, 255});
  if (firing)
  {
    fx_glow(r, sprite_point_x(x, sprite_w, dir, lx - 5.0f + recoil),
            y + ly + 4.5f, 16.0f, FX_AMBER, 100);
    sprite_rect(r, x, y, sprite_w, dir, lx - 8.0f + recoil, ly + 1.0f,
                6.0f, 7.0f, FX_FLAME);
    sprite_rect(r, x, y, sprite_w, dir, lx - 5.0f + recoil, ly + 3.0f,
                4.0f, 3.0f, FX_FLAME_HOT);
  }
}

static void draw_vertical_bazooka_weapon(SDL_Renderer *r, float x, float y,
                                         int dir, bool firing)
{
  float bx = x + 16.0f;
  float by = dir < 0 ? y - 7.0f : y + 15.0f;
  color_rect(r, COL_OUTLINE, bx, by, 8.0f, 24.0f);
  color_rect(r, (SDL_Color){51, 75, 48, 255},
             bx + 1.0f, by + 1.0f, 6.0f, 21.0f);
  color_rect(r, (SDL_Color){106, 135, 79, 255},
             bx + 3.0f, by + 3.0f, 2.0f, 17.0f);
  color_rect(r, (SDL_Color){139, 151, 111, 255},
             bx - 1.0f, dir < 0 ? by : by + 19.0f, 10.0f, 5.0f);
  color_rect(r, COL_OUTLINE, bx - 5.0f, by + 9.0f, 6.0f, 5.0f);
  color_rect(r, (SDL_Color){73, 60, 43, 255},
             bx - 4.0f, by + 10.0f, 5.0f, 3.0f);
  if (firing)
  {
    float flame_y = dir < 0 ? by + 24.0f : by - 6.0f;
    fx_glow(r, bx + 4.0f, flame_y + 3.0f, 16.0f, FX_AMBER, 100);
    color_rect(r, FX_FLAME, bx, flame_y, 8.0f, 6.0f);
    color_rect(r, FX_FLAME_HOT, bx + 2.0f, flame_y, 4.0f, 4.0f);
  }
}

/*
 * Both legs of a figure that is standing still.
 *
 * `top` is where the trousers begin; the soles stay on the sprite's own floor
 * line, so a body that sinks into a squash shortens its legs instead of
 * lifting off the ground.
 */
static void draw_standing_legs(SDL_Renderer *r, float x, float y,
                               float sprite_w, int dir,
                               float rear_x, float front_x, float top,
                               SDL_Color rear, SDL_Color front, SDL_Color boot)
{
  float height = 30.0f - top;

  sprite_rect(r, x, y, sprite_w, dir, rear_x - 1.0f, top, 6.0f, height,
              COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir, front_x - 1.0f, top, 6.0f, height,
              COL_OUTLINE);
  sprite_form(r, x, y, sprite_w, dir, rear_x, top, 4.0f, height - 1.0f, rear);
  sprite_form(r, x, y, sprite_w, dir, front_x, top, 4.0f, height - 1.0f, front);
  sprite_shoe(r, x, y, sprite_w, dir, rear_x + 0.5f, 30.0f, boot);
  sprite_shoe(r, x, y, sprite_w, dir, front_x + 0.5f, 30.0f, boot);
}

/*
 * One leg of a two-beat walk.
 *
 * `cycle` is this leg's own place in the stride, 0..1, with the first half
 * stance and the second swing; the other leg is handed the same value half a
 * turn along. Driving the ankle from a cycle rather than from a sine is what
 * stops the foot skating: through stance it tracks straight back under the
 * body at a constant rate, and only the swing half lifts and reaches forward.
 * A sine does the opposite — it is slowest exactly where the foot should be
 * carrying the figure fastest.
 *
 * It also decides what the rest of a walking figure has to be timed from. A
 * foot is furthest forward at the start of its cycle, heel strike, so how far
 * forward the near foot is — the number the arms swing against and the body
 * bobs on — is a cosine of the phase and not a sine. Driven by the sine, the
 * arms and the bob ran a quarter of a stride out of step with the feet: the
 * near arm reached its furthest forward just as the legs crossed, and the body
 * rose at heel strike and sank as the legs passed, which is a walk played
 * backwards.
 *
 * Chuck does not walk on this any more — see [chuck_pose.h](chuck_pose.h),
 * whose gait is driven by distance rather than by a clock — but the crew, the
 * janitor, the civilians and the receptionist still do.
 */
static void draw_walking_leg(SDL_Renderer *r, float x, float y, float sprite_w,
                             int dir, float hip_x, float hip_y, float cycle,
                             float reach, SDL_Color trouser, SDL_Color boot)
{
  float ankle_x;
  float ankle_y;
  float lift = 0.0f;

  cycle -= floorf(cycle);
  if (cycle < 0.5f)
  {
    /* Stance: heel strike ahead of the hip through to toe off behind it. */
    float t = cycle * 2.0f;
    ankle_x = hip_x + reach * (1.0f - 2.0f * t);
    ankle_y = 30.0f;
  }
  else
  {
    /* Swing: quick through the middle, slow at both ends where the foot is
       about to take or give up the load. */
    float t = (cycle - 0.5f) * 2.0f;
    float ease = t * t * (3.0f - 2.0f * t);
    ankle_x = hip_x + reach * (-1.0f + 2.0f * ease);
    lift = sinf(t * 3.14159265f) * reach * 0.80f;
    ankle_y = 30.0f - lift;
  }

  /* The knee leads the ankle and bends hardest at the top of the swing. */
  float knee_x = hip_x + (ankle_x - hip_x) * 0.45f + lift * 0.30f + 0.6f;
  float knee_y = hip_y + (ankle_y - hip_y) * 0.52f;

  sprite_limb_segment(r, x, y, sprite_w, dir,
                      hip_x, hip_y, knee_x, knee_y, trouser);
  sprite_limb_segment(r, x, y, sprite_w, dir,
                      knee_x, knee_y, ankle_x, ankle_y, trouser);
  sprite_shoe(r, x, y, sprite_w, dir, ankle_x, ankle_y, boot);
}

static void draw_walking_arm(SDL_Renderer *r, float x, float y, float sprite_w,
                             int dir, float shoulder_x, float shoulder_y,
                             float swing, SDL_Color upper, SDL_Color lower)
{
  /* In side view both arms share the same visible shoulder pivot near the
     centre of the upper torso.  Only the elbow and hand counter-swing, and the
     hand trails the elbow by a fraction of the stride so the arm reads as
     being dragged along rather than as one rigid piece. */
  float elbow_x = shoulder_x + swing * 1.25f;
  float hand_x = shoulder_x + swing * 3.0f;
  float elbow_y = shoulder_y + 4.5f;
  float hand_y = shoulder_y + 9.0f - fabsf(swing) * 0.5f;

  sprite_limb_segment(r, x, y, sprite_w, dir,
                      shoulder_x, shoulder_y, elbow_x, elbow_y, upper);
  sprite_limb_segment(r, x, y, sprite_w, dir,
                      elbow_x, elbow_y, hand_x, hand_y, lower);
  /* A hand, so the arm ends in something rather than stopping. */
  sprite_rect(r, x, y, sprite_w, dir,
              hand_x - 1.5f, hand_y - 0.5f, 4.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir,
              hand_x - 0.5f, hand_y, 2.0f, 2.0f, fx_ramp(lower).lit);
}

static void draw_climbing_arm(SDL_Renderer *r, float x, float y, float sprite_w,
                              int dir, float shoulder_x, float shoulder_y,
                              float grip_x, float hand_y,
                              SDL_Color sleeve, SDL_Color skin)
{
  /* Seen from behind, a climber's elbows flare outside the shoulders while
     the forearms turn back in toward the rung.  Bending the arm this way is
     what distinguishes the ladder pose from two straight raised arms. */
  float side = grip_x < shoulder_x ? -1.0f : 1.0f;
  float elbow_x = shoulder_x + side * 5.0f;
  float elbow_y = shoulder_y + (hand_y - shoulder_y) * 0.52f;

  sprite_limb_segment(r, x, y, sprite_w, dir,
                      shoulder_x, shoulder_y, elbow_x, elbow_y, sleeve);
  sprite_limb_segment(r, x, y, sprite_w, dir,
                      elbow_x, elbow_y, grip_x, hand_y, skin);

  /* Compact palms sit just inside the rails, wrapped around a rung. */
  sprite_rect(r, x, y, sprite_w, dir,
              grip_x - 2.5f, hand_y - 1.5f, 5.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir,
              grip_x - 1.5f, hand_y - 0.5f, 3.0f, 2.0f, skin);
}

/*
 * The surface a figure's shadow falls on, and how far above it he is.
 *
 * A pool of shade drawn at the boots is not a cast shadow: it climbs with the
 * figure and so says the floor came along on the jump. Finding the first solid
 * tile under him instead costs one column scan and buys the entire read of a
 * jump — the shadow stays where the floor is and thins out as he leaves it.
 * Returns false when there is nothing close enough below to catch one.
 */
static bool character_ground(const Level *level, float cx, float feet_y,
                             float *out_y, float *out_lift)
{
  int col = (int)floorf(cx / (float)TILE_SIZE);
  int start = (int)floorf(feet_y / (float)TILE_SIZE);
  /* Four tiles is about as far as a shadow can fall and still belong to the
     figure casting it; past that the pool would read as someone else's. */
  int limit = start + 4;

  for (int row = start; row <= limit; ++row)
  {
    if (!level_is_solid(level, col, row))
      continue;
    float surface = (float)row * (float)TILE_SIZE;
    float height = surface - feet_y;
    if (height < 0.0f)
      height = 0.0f;
    *out_y = surface;
    *out_lift = fminf(1.0f, height / (2.6f * (float)TILE_SIZE));
    return true;
  }
  return false;
}

/*
 * The rest of the cast gets the same anchoring as the player. A pool pinned
 * to the boots travels up with a stomped guard or a dropping dog at full
 * size, which states the floor jumped with them; found below and thinned
 * with height, it is most of what sells how far off the ground they are.
 */
static void npc_contact_shadow(SDL_Renderer *r, const Level *level,
                               float world_cx, float world_feet_y,
                               float half_w, Uint8 alpha,
                               float cam_x, float oy)
{
  float ground_y;
  float lift;

  if (level != NULL &&
      character_ground(level, world_cx, world_feet_y, &ground_y, &lift))
    fx_contact_shadow(r, world_cx - cam_x, ground_y + oy - 1.0f,
                      half_w, lift, alpha);
}

/*
 * The thing in the hand of a throw, which is not always a grenade.
 *
 * All three underarm throwables share one pose and one flag — see
 * `Player.throwing_weapon` — and for as long as that was true the ladder pose
 * drew a grenade for every one of them, including the bolt, which is six
 * pixels of plated steel, and the flash charge, whose own comment up this file
 * says it "has to be told from a grenade at a glance, because one of the two is
 * about to kill whoever is standing next to it". A shared animation is not a
 * shared prop.
 *
 * The grenade is the default arm rather than an error, because it is what this
 * pose drew before there was anything else to draw, and a figure with an empty
 * hand mid-throw is worse than a figure holding the wrong thing.
 */
static void draw_thrown_in_hand(SDL_Renderer *r, const Player *p,
                                float x, float y)
{
  switch (p->throwing_weapon)
  {
  case PLAYER_WEAPON_FLASH:
    draw_flashbang(r, x, y, 0.0f);
    return;
  case PLAYER_WEAPON_DECOY:
    /* Centred in the space the fatter two fill, so a bolt does not read as a
       grenade that has slipped out of the fingers. */
    draw_decoy(r, x + (float)(GRENADE_W - DECOY_W) * 0.5f,
               y + (float)(GRENADE_W - DECOY_W) * 0.5f);
    return;
  case PLAYER_WEAPON_PISTOL:
  case PLAYER_WEAPON_KNIFE:
  case PLAYER_WEAPON_GRENADE:
  case PLAYER_WEAPON_BAZOOKA:
  case PLAYER_WEAPON_COUNT:
    break;
  }
  draw_grenade(r, x, y, 0.0f);
}

/* ---- Chuck ---------------------------------------------------------- */

/*
 * Chuck, in the four views the sector has of him: side on (standing, running,
 * in the air and every weapon), flat on his elbows, and from behind on a ladder
 * and at a console.
 *
 * Side on he is drawn from his skeleton — [chuck_pose.h](chuck_pose.h) for the
 * joints, [render_chuck.h](render_chuck.h) for the drawing — which is the same
 * code the film draws him with, at one pixel to the unit instead of one and a
 * half. The other three views keep hand-placed forms, because a skeleton seen
 * from behind or lying down is a different drawing rather than a different
 * pose, but they are held to the same proportions: the cast's eleven-row head,
 * a square-shouldered jacket ten deep over the hips, and the legs from 20.7 down —
 * a little longer than the guards' beside him, and otherwise their template. A
 * climb seen from behind in one set of proportions beside a run in another would
 * be two men.
 */

static const SDL_Color PLAYER_TROUSER = {29, 55, 80, 255};
static const SDL_Color PLAYER_TROUSER_FAR = {21, 40, 59, 255};
static const SDL_Color PLAYER_BOOT = {34, 39, 49, 255};
static const SDL_Color PLAYER_BOOT_FAR = {26, 31, 40, 255};
static const SDL_Color PLAYER_BOOT_LT = {63, 72, 86, 255};
static const SDL_Color PLAYER_SLEEVE = {42, 118, 153, 255};
static const SDL_Color PLAYER_FOREARM = {209, 154, 105, 255};
static const SDL_Color PLAYER_STRAP = {21, 54, 76, 255};
static const SDL_Color PLAYER_GUNMETAL = {31, 38, 43, 255};
static const SDL_Color PLAYER_GRIP = {44, 49, 49, 255};
static const SDL_Color KNIFE_HANDLE = {55, 43, 31, 255};
static const SDL_Color KNIFE_STEEL = {205, 221, 225, 255};
static const SDL_Color KNIFE_TIP = {241, 247, 239, 255};
static const SDL_Color MUZZLE_HOT = {255, 242, 184, 255};

static ChuckView player_view(SDL_Renderer *r, float x, float y, int dir)
{
  return (ChuckView){r, x, y, dir, 1.0f, COL_OUTLINE};
}

/* ---- From behind ---------------------------------------------------- */

/*
 * One leg seen from behind: the trouser leg from the seat to the ankle and the
 * back of the boot under it. `raise` lifts the boot up the leg, which is what a
 * knee drawn up toward a rung looks like from behind — the thigh goes into the
 * picture and the leg foreshortens.
 */
static void draw_back_leg(SDL_Renderer *r, float x, float y, float lx,
                          float raise, SDL_Color trouser)
{
  float top = 20.0f;
  float ankle = 29.0f - raise;

  sprite_rect(r, x, y, PLAYER_W, 1, lx - 1.0f, top, 5.0f, ankle - top + 1.0f,
              COL_OUTLINE);
  sprite_form(r, x, y, PLAYER_W, 1, lx, top, 3.0f, ankle - top, trouser);
  sprite_rect(r, x, y, PLAYER_W, 1, lx - 1.0f, ankle, 5.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1, lx, ankle + 1.0f, 3.0f, 2.0f,
              PLAYER_BOOT);
  sprite_rect(r, x, y, PLAYER_W, 1, lx, ankle + 1.0f, 3.0f, 1.0f,
              PLAYER_BOOT_LT);
}

/* The jacket from behind: square across the shoulders, in to the belt, the
   shoulder blades catching the lamp either side of the webbing's back strap. */
static void draw_back_torso(SDL_Renderer *r, float x, float y, float bob)
{
  sprite_body(r, x, y, PLAYER_W, 1, 9.0f, 19.0f + bob, 8.0f, 3.0f,
              PLAYER_TROUSER, COL_OUTLINE, 1, 1);
  /* Square across the shoulders: two rows off the top corners is shoulders
     sloping away from the neck, and with a full-width belt under them the back
     view came out as a bell. */
  sprite_body(r, x, y, PLAYER_W, 1, 7.0f, 11.0f + bob, 12.0f, 10.0f, FX_HERO,
              COL_OUTLINE, 1, 1);
  sprite_rect(r, x, y, PLAYER_W, 1, 8.0f, 13.0f + bob, 3.0f, 4.0f,
              FX_HERO_LT);
  sprite_rect(r, x, y, PLAYER_W, 1, 15.0f, 13.0f + bob, 3.0f, 4.0f,
              FX_HERO_LT);
  sprite_rect(r, x, y, PLAYER_W, 1, 12.0f, 12.0f + bob, 2.0f, 6.0f,
              PLAYER_STRAP);
  sprite_rect(r, x, y, PLAYER_W, 1, 8.0f, 18.0f + bob, 10.0f, 2.0f, FX_AMBER);
  sprite_rect(r, x, y, PLAYER_W, 1, 8.0f, 20.0f + bob, 10.0f, 1.0f,
              fx_ramp(FX_HERO).dark);
}

/* The back of his head: hair down to the nape, the ears either side of it, and
   the headband's knot at the back with its two ends hanging off it — the one
   view where the knot the side view trails is actually in front of the eye. */
static void draw_back_head(SDL_Renderer *r, float x, float y, float bob,
                           float sway)
{
  sprite_body(r, x, y, PLAYER_W, 1, 9.0f, 2.0f + bob, 8.0f, 9.0f, FX_SKIN,
              COL_OUTLINE, 2, 2);
  sprite_mass(r, x, y, PLAYER_W, 1, 9.0f, 2.0f + bob, 8.0f, 7.0f, FX_HAIR, 2,
              0);
  sprite_rect(r, x, y, PLAYER_W, 1, 11.0f, 2.0f + bob, 4.0f, 1.0f,
              PLAYER_HAIR_LT);
  sprite_rect(r, x, y, PLAYER_W, 1, 10.0f, 9.0f + bob, 6.0f, 1.0f,
              FX_SKIN_DK);
  sprite_rect(r, x, y, PLAYER_W, 1, 8.0f, 5.0f + bob, 1.0f, 2.0f,
              FX_SKIN_DK);
  sprite_rect(r, x, y, PLAYER_W, 1, 17.0f, 5.0f + bob, 1.0f, 2.0f,
              FX_SKIN_DK);
  sprite_rect(r, x, y, PLAYER_W, 1, 8.0f, 4.0f + bob, 10.0f, 2.0f, FX_RED);
  sprite_rect(r, x, y, PLAYER_W, 1, 8.0f, 4.0f + bob, 10.0f, 1.0f,
              (SDL_Color){246, 104, 88, 255});
  /* Tied off to one side of the middle: a knot dead centre with its ends
     hanging straight down it draws a red cross on the back of his head. */
  sprite_rect(r, x, y, PLAYER_W, 1, 14.0f, 4.0f + bob, 2.0f, 2.0f,
              (SDL_Color){166, 38, 42, 255});
  sprite_rect(r, x, y, PLAYER_W, 1, 14.0f + sway, 6.0f + bob, 1.0f, 2.0f,
              (SDL_Color){166, 38, 42, 255});
  sprite_rect(r, x, y, PLAYER_W, 1, 16.0f + sway * 1.5f, 5.0f + bob, 1.0f,
              3.0f, (SDL_Color){166, 38, 42, 255});
}

static void draw_player_crawling(SDL_Renderer *r, const Player *p, float x, float y)
{
  int dir = p->facing;
  float phase = p->anim_time * 3.2f;
  float shove = (fabsf(p->vx) > 1.0f) ? sinf(phase) * 2.0f : 0.0f;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  /* A throw is not a shot, prone any more than standing: see the note on the
     standing throw in `draw_player_side`. */
  bool throwing = p->action_timer > 0.0f && p->grenade_throwing && !knife;
  bool firing = p->action_timer > 0.0f && !knife && !throwing;
  bool bazooka = (p->active_weapon == PLAYER_WEAPON_BAZOOKA &&
                   p->bazooka_rockets > 0) ||
                 (firing && p->bazooka_firing);

  /* The legs. The ground shadow is laid by the caller, anchored to the floor
     rather than to the belly.
     This pose used to have one boot and no legs — a torso with a shoe stuck
     on the back of it — so a man on his elbows read as a blue lump, which is
     the one thing a crawl through a room full of guards must not do. Two
     legs, as long as they are standing: the far one straight out behind, the
     near one with its knee drawn up under him, the two trading places on the
     shove so the crawl travels. */
  float knee_far = 1.5f - shove * 0.8f;
  float knee_near = 3.5f + shove * 0.8f;
  sprite_limb_segment(r, x, y, PLAYER_W, dir, 8.0f, 12.0f, knee_far, 13.0f,
                      PLAYER_TROUSER_FAR);
  sprite_limb_segment(r, x, y, PLAYER_W, dir, knee_far, 13.0f, -3.5f, 14.0f,
                      PLAYER_TROUSER_FAR);
  sprite_rect(r, x, y, PLAYER_W, dir, -6.0f, 12.0f, 4.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, -5.0f, 13.0f, 2.0f, 3.0f,
              PLAYER_BOOT_FAR);
  sprite_limb_segment(r, x, y, PLAYER_W, dir, 9.0f, 13.0f, knee_near, 15.5f,
                      PLAYER_TROUSER);
  sprite_limb_segment(r, x, y, PLAYER_W, dir, knee_near, 15.5f,
                      -1.5f + shove * 0.4f, 14.5f, PLAYER_TROUSER);
  /* Toe dug into the floor, heel to the ceiling. */
  sprite_rect(r, x, y, PLAYER_W, dir, -3.5f + shove * 0.4f, 12.5f, 4.0f, 5.0f,
              COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, -2.5f + shove * 0.4f, 13.5f, 2.0f, 3.0f,
              PLAYER_BOOT);
  sprite_rect(r, x, y, PLAYER_W, dir, -2.5f + shove * 0.4f, 13.5f, 2.0f, 1.0f,
              PLAYER_BOOT_LT);

  /* The seat of the trousers, then the jacket laid along the floor with the
     belt across its tail and the webbing on the diagonal. */
  sprite_body(r, x, y, PLAYER_W, dir, 5.0f, 9.0f, 4.0f, 6.0f, PLAYER_TROUSER,
              COL_OUTLINE, 1, 1);
  sprite_body(r, x, y, PLAYER_W, dir, 8.0f, 7.0f, 11.0f, 8.0f, FX_HERO,
              COL_OUTLINE, 1, 1);
  sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 7.0f, 9.0f, 2.0f, FX_HERO_LT);
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 8.0f, 2.0f, 6.0f, FX_AMBER);
  sprite_segment(r, x, y, PLAYER_W, dir, 11.0f, 9.0f, 17.0f, 13.0f, 2,
                 PLAYER_STRAP);

  /* The head, raised to look ahead along the floor: the same head as standing,
     from the same code, so the face does not change when he gets down. */
  ChuckView view = player_view(r, x, y, dir);
  ChuckPose head_pose = {0};
  head_pose.neck = (ChuckPoint){20.5f, 11.0f};
  head_pose.pelvis = (ChuckPoint){20.5f, 11.0f + CHUCK_SPINE};
  head_pose.tail = 0.4f + fabsf(shove) * 0.3f;
  chuck_draw_head(&view, &head_pose, fx_blinking(p->anim_time, 0x1u));

  /* Braced front arm: the elbow planted under the shoulder, the forearm along
     the floor, and whatever it is holding out past the head. */
  sprite_limb_segment(r, x, y, PLAYER_W, dir, 16.0f, 10.5f, 17.5f, 15.0f,
                      PLAYER_SLEEVE);
  sprite_limb_segment(r, x, y, PLAYER_W, dir, 17.5f, 15.0f, 21.5f, 14.5f,
                      PLAYER_FOREARM);
  sprite_rect(r, x, y, PLAYER_W, dir, 21.0f, 13.0f, 4.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 14.0f, 2.0f, 2.0f, FX_SKIN);
  if (knife)
  {
    float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f ? 2.0f : 0.0f;
    sprite_rect(r, x, y, PLAYER_W, dir, 24.0f + thrust, 13.0f, 2.0f, 4.0f,
                KNIFE_HANDLE);
    sprite_rect(r, x, y, PLAYER_W, dir, 26.0f + thrust, 14.0f, 6.0f, 2.0f,
                KNIFE_STEEL);
    sprite_rect(r, x, y, PLAYER_W, dir, 32.0f + thrust, 14.5f, 1.0f, 1.0f,
                KNIFE_TIP);
  }
  else if (throwing)
  {
    /* Flat on the floor the throw is a flick along it: the fingers open out
       past the head, the thing already skidding away. */
    sprite_rect(r, x, y, PLAYER_W, dir, 24.0f, 13.0f, 2.0f, 1.0f, FX_SKIN);
    sprite_rect(r, x, y, PLAYER_W, dir, 24.0f, 15.0f, 2.0f, 1.0f, FX_SKIN);
  }
  else if (bazooka)
  {
    draw_bazooka_weapon(r, x, y, PLAYER_W, dir,
                        14.0f, 3.0f, p->bazooka_firing);
  }
  else if ((p->active_weapon == PLAYER_WEAPON_PISTOL && p->bullets > 0) ||
           firing)
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 23.0f, 13.0f,
                firing ? 7.0f : 5.0f, 3.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 23.5f, 13.5f,
                firing ? 6.0f : 4.0f, 2.0f, PLAYER_GUNMETAL);
    if (firing && p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
    {
      /* Prone or standing, a shot lights the floor it is fired across. */
      draw_muzzle_flash(r, x, y, PLAYER_W, dir, 31.0f, 14.5f, FX_AMBER);
      sprite_rect(r, x, y, PLAYER_W, dir, 29.0f, 12.0f, 3.0f, 5.0f, FX_AMBER);
      sprite_rect(r, x, y, PLAYER_W, dir, 32.0f, 13.0f, 2.0f, 3.0f, FX_FLAME_HOT);
    }
  }
}

static void draw_player_hacking(SDL_Renderer *r, float x, float y,
                                float hack_time)
{
  float type_phase = hack_time * 15.0f;
  float tap_a = sinf(type_phase) * 1.2f;
  float tap_b = sinf(type_phase + 3.14159265f) * 1.2f;
  float bob = sinf(hack_time * 5.0f) * 0.25f;

  /* Rear view: Chuck faces the wall-mounted terminal, so the camera sees
     the back of his head, shoulders and torso, planted on both feet. */
  draw_back_leg(r, x, y, 9.0f, 0.0f, fx_mix(PLAYER_TROUSER_FAR, PLAYER_TROUSER,
                                            0.5f));
  draw_back_leg(r, x, y, 14.0f, 0.0f, PLAYER_TROUSER);

  /* Seen from behind, elbows flare outward and both forearms reach forward
     again to the terminal's lower keypad. */
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      9.0f, 13.0f + bob, 4.5f, 16.0f + bob, FX_HERO);
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      4.5f, 16.0f + bob, 9.5f, 19.5f + tap_a, PLAYER_FOREARM);
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      17.0f, 13.0f + bob, 21.5f, 16.0f + bob, FX_HERO_LT);
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      21.5f, 16.0f + bob, 16.5f, 19.5f + tap_b, PLAYER_FOREARM);

  draw_back_torso(r, x, y, bob);
  draw_back_head(r, x, y, bob, 0.0f);

  /* His hands are between his body and the terminal, so they are hidden
     from this rear angle; only the alternating elbow motion is visible. */
}

/*
 * On a ladder, from behind. The beat is spent vertically on a climb — a hand
 * and the opposite boot rise while the other pair hold — and across the rungs on
 * a traverse; see `docs/art-and-audio.md`, "a traverse is not a climb".
 */
static void draw_player_climbing(SDL_Renderer *r, const Player *p, float x,
                                 float y)
{
  const int dir = 1;
  float phase = p->anim_time * 3.0f;
  bool moving = fabsf(p->vx) > 2.0f;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  bool grenade = p->action_timer > 0.0f && p->grenade_throwing;
  bool firing = p->action_timer > 0.0f && !knife;
  bool bazooka = (p->active_weapon == PLAYER_WEAPON_BAZOOKA &&
                   p->bazooka_rockets > 0) ||
                 (firing && p->bazooka_firing);
  /* Across the rungs rather than up them. Vertical travel wins when both are
     held: that is the part of the move the player is watching, and a pose
     trying to say both at once says neither. */
  bool shuffling = moving && fabsf(p->vy) <= 1.0f;
  float shuffle_side = shuffling ? (p->vx > 0.0f ? 1.0f : -1.0f) : 0.0f;
  float beat = sinf(phase);

  /* The weight goes across as well as the limbs: the body hangs back off the
     hand that is reaching and rides forward over the pair that gather. A pixel
     and a half of it is the difference between someone shifting across a ladder
     and two limbs waving on a figure travelling on rails. */
  if (shuffling)
    x -= shuffle_side * sinf(phase) * 0.7f;

  /*
   * A traverse is not a climb, and one beat is all that separates the two.
   * Climbing spends it vertically: one hand and the opposite boot rise while
   * the other pair hold. Going sideways spends the same beat across the
   * rungs — the leading hand and boot reach out on the first half, the
   * trailing pair gather across on the second — so the alternation stops and
   * nothing is pumping up and down while the figure travels level.
   */
  float reach = shuffle_side * fmaxf(0.0f, beat) * 3.0f;
  float gather = shuffle_side * fmaxf(0.0f, -beat) * 3.0f;
  float left_step = shuffle_side > 0.0f ? gather : reach;
  float right_step = shuffle_side > 0.0f ? reach : gather;
  float climb = shuffling ? 0.0f : beat * 4.0f;
  /* A boot that slides along a rung is a boot with no weight on it, so the
     one that is moving clears it first. */
  float left_raise = fmaxf(0.0f, -climb) + fabsf(left_step) * 0.5f;
  float right_raise = fmaxf(0.0f, climb) + fabsf(right_step) * 0.5f;
  /* Each hand travels with the boot below it, but stays on its own side of
     the shoulders: a grip that crossed the body would swap the elbow flare
     mid-beat and pop. Reaching outward is free, gathering inward is damped. */
  float left_grip = 7.0f + (left_step > 0.0f ? left_step * 0.4f : left_step);
  float right_grip = 19.0f + (right_step < 0.0f ? right_step * 0.4f
                                                : right_step);
  float bob = fabsf(beat) * 0.7f;
  float left_hand_y = 4.5f - climb;
  float right_hand_y = 4.5f + climb;

  /* Seen from behind the legs are still trousers, and still darker than the
     jacket above them — drawn at the torso's own value they turned the whole
     climb into one blue column. */
  draw_back_leg(r, x, y, 9.0f + left_step, left_raise,
                fx_mix(PLAYER_TROUSER_FAR, PLAYER_TROUSER, 0.5f));
  draw_back_leg(r, x, y, 14.0f + right_step, right_raise, PLAYER_TROUSER);
  draw_back_torso(r, x, y, bob);

  if (knife)
  {
    /* One hand stays on the ladder while the other follows the selected
       attack direction. */
    if (p->shot_vertical != 0)
    {
      float hand_y = p->shot_vertical < 0 ? 5.0f : 21.0f;
      float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f
                         ? 2.0f
                         : 0.0f;
      draw_climbing_arm(r, x, y, PLAYER_W, dir, 9.0f, 13.0f + bob,
                        left_grip, left_hand_y, PLAYER_SLEEVE,
                        PLAYER_FOREARM);
      sprite_limb_segment(r, x, y, PLAYER_W, dir, 17.0f, 13.0f + bob,
                          20.0f, hand_y, PLAYER_SLEEVE);
      sprite_rect(r, x, y, PLAYER_W, dir, 18.0f, hand_y - 2.0f, 5.0f, 5.0f,
                  COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir, 19.0f, hand_y - 1.0f, 3.0f, 3.0f,
                  PLAYER_FOREARM);
      float handle_y = p->shot_vertical < 0 ? hand_y - 6.0f - thrust
                                            : hand_y + 2.0f + thrust;
      float blade_y = p->shot_vertical < 0 ? handle_y - 8.0f
                                           : handle_y + 5.0f;
      sprite_rect(r, x, y, PLAYER_W, dir, 18.0f, handle_y, 5.0f, 6.0f,
                  KNIFE_HANDLE);
      sprite_rect(r, x, y, PLAYER_W, dir, 19.0f, blade_y, 3.0f, 8.0f,
                  KNIFE_STEEL);
    }
    else
    {
      /* The rear-facing ladder pose is fixed, so only the attacking arm is
         mirrored for a sideways stab. */
      if (p->facing > 0)
        draw_climbing_arm(r, x, y, PLAYER_W, dir, 9.0f, 13.0f + bob,
                          left_grip, left_hand_y, PLAYER_SLEEVE,
                          PLAYER_FOREARM);
      else
        draw_climbing_arm(r, x, y, PLAYER_W, dir, 17.0f, 13.0f + bob,
                          right_grip, right_hand_y, PLAYER_SLEEVE,
                          PLAYER_FOREARM);

      int knife_dir = p->facing;
      float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f
                         ? 2.0f
                         : 0.0f;
      sprite_limb_segment(r, x, y, PLAYER_W, knife_dir, 16.0f, 13.0f + bob,
                          20.0f + thrust, 14.0f + bob, PLAYER_SLEEVE);
      sprite_rect(r, x, y, PLAYER_W, knife_dir, 19.0f + thrust, 12.0f + bob,
                  6.0f, 5.0f, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, knife_dir, 20.0f + thrust, 13.0f + bob,
                  5.0f, 3.0f, PLAYER_FOREARM);
      sprite_rect(r, x, y, PLAYER_W, knife_dir, 24.0f + thrust, 12.0f + bob,
                  3.0f, 5.0f, KNIFE_HANDLE);
      sprite_rect(r, x, y, PLAYER_W, knife_dir, 27.0f + thrust, 13.0f + bob,
                  6.0f, 2.0f, KNIFE_STEEL);
      sprite_rect(r, x, y, PLAYER_W, knife_dir, 33.0f + thrust, 13.5f + bob,
                  1.0f, 1.0f, KNIFE_TIP);
    }
  }
  else if (grenade)
  {
    draw_climbing_arm(r, x, y, PLAYER_W, dir, 9.0f, 13.0f + bob, left_grip,
                      left_hand_y, PLAYER_SLEEVE, PLAYER_FOREARM);
    if (p->shot_vertical != 0)
    {
      float hand_y = p->shot_vertical < 0 ? 4.0f : 21.0f;
      sprite_limb_segment(r, x, y, PLAYER_W, dir, 17.0f, 13.0f + bob, 20.0f,
                          hand_y, PLAYER_SLEEVE);
      draw_thrown_in_hand(r, p, x + 16.0f,
                          p->shot_vertical < 0 ? y - 6.0f : y + 22.0f);
    }
    else
    {
      int throw_dir = p->facing;
      sprite_limb_segment(r, x, y, PLAYER_W, throw_dir, 16.0f, 13.0f + bob,
                          22.0f, 9.0f + bob, PLAYER_SLEEVE);
      draw_thrown_in_hand(r, p,
                          throw_dir > 0 ? x + PLAYER_W + 2.0f
                                        : x - GRENADE_W - 2.0f,
                          y + 4.0f + bob);
    }
  }
  else if (firing && p->shot_vertical == 0)
  {
    /* Horizontal ladder fire uses the stored facing direction while the
       body remains turned toward the ladder. */
    if (p->facing > 0)
      draw_climbing_arm(r, x, y, PLAYER_W, dir, 9.0f, 13.0f + bob,
                        left_grip, left_hand_y, PLAYER_SLEEVE,
                        PLAYER_FOREARM);
    else
      draw_climbing_arm(r, x, y, PLAYER_W, dir, 17.0f, 13.0f + bob,
                        right_grip, right_hand_y, PLAYER_SLEEVE,
                        PLAYER_FOREARM);

    int gun_dir = p->facing;
    float recoil = p->action_timer > 0.075f ? -1.0f : 0.0f;
    sprite_limb_segment(r, x, y, PLAYER_W, gun_dir, 16.0f, 13.0f + bob,
                        21.0f + recoil, 14.0f + bob, PLAYER_SLEEVE);
    if (bazooka)
    {
      draw_bazooka_weapon(r, x, y, PLAYER_W, gun_dir, 13.0f, 7.0f + bob,
                          true);
    }
    else
    {
      sprite_rect(r, x, y, PLAYER_W, gun_dir, 20.0f + recoil, 12.0f + bob,
                  7.0f, 5.0f, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, gun_dir, 21.0f + recoil, 13.0f + bob,
                  6.0f, 3.0f, PLAYER_FOREARM);
      sprite_rect(r, x, y, PLAYER_W, gun_dir, 25.0f + recoil, 11.0f + bob,
                  8.0f, 4.0f, PLAYER_GUNMETAL);
      sprite_rect(r, x, y, PLAYER_W, gun_dir, 27.0f + recoil, 15.0f + bob,
                  3.0f, 5.0f, PLAYER_GRIP);
      if (p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
      {
        draw_muzzle_flash(r, x, y, PLAYER_W, gun_dir, 35.0f + recoil,
                          13.0f + bob, FX_AMBER);
        sprite_rect(r, x, y, PLAYER_W, gun_dir, 33.0f + recoil, 10.0f + bob,
                    4.0f, 6.0f, FX_AMBER);
        sprite_rect(r, x, y, PLAYER_W, gun_dir, 37.0f + recoil, 12.0f + bob,
                    3.0f, 3.0f, MUZZLE_HOT);
      }
    }
  }
  else
  {
    /* Keep one hand on the ladder while the other operates the sidearm. */
    draw_climbing_arm(r, x, y, PLAYER_W, dir, 9.0f, 13.0f + bob, left_grip,
                      left_hand_y, PLAYER_SLEEVE, PLAYER_FOREARM);
    if (firing && p->shot_vertical != 0)
    {
      float hand_y = p->shot_vertical < 0 ? 7.0f : 20.0f;
      sprite_limb_segment(r, x, y, PLAYER_W, dir, 17.0f, 13.0f + bob, 20.0f,
                          hand_y, PLAYER_SLEEVE);
      sprite_rect(r, x, y, PLAYER_W, dir, 18.0f, hand_y - 2.0f, 5.0f, 5.0f,
                  COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir, 19.0f, hand_y - 1.0f, 3.0f, 3.0f,
                  PLAYER_FOREARM);
      if (bazooka)
      {
        draw_vertical_bazooka_weapon(r, x - 1.0f, y - 1.0f, p->shot_vertical,
                                     true);
      }
      else
      {
        float gun_y = p->shot_vertical < 0 ? 0.0f : 18.0f;
        sprite_rect(r, x, y, PLAYER_W, dir, 18.0f, gun_y, 5.0f, 8.0f,
                    PLAYER_GUNMETAL);
        if (p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
        {
          float flash_y = p->shot_vertical < 0 ? -6.0f : 26.0f;
          draw_muzzle_flash(r, x, y, PLAYER_W, dir, 20.0f, flash_y + 2.0f,
                            FX_AMBER);
          sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, flash_y, 7.0f, 5.0f,
                      FX_AMBER);
          sprite_rect(r, x, y, PLAYER_W, dir, 19.0f,
                      p->shot_vertical < 0 ? flash_y - 3.0f
                                           : flash_y + 5.0f,
                      3.0f, 3.0f, MUZZLE_HOT);
        }
      }
    }
    else
    {
      draw_climbing_arm(r, x, y, PLAYER_W, dir, 17.0f, 13.0f + bob,
                        right_grip, right_hand_y, PLAYER_SLEEVE,
                        PLAYER_FOREARM);
    }
  }

  /* Seen from behind: the nape of the neck below the hair, and no face. */
  draw_back_head(r, x, y, bob, climb * 0.15f);
}

/* ---- Side on -------------------------------------------------------- */

/*
 * What is in his near hand side on, which decides what the arm does as well as
 * what is drawn in it.
 */
typedef enum
{
  PLAYER_PROP_NONE,
  PLAYER_PROP_PISTOL_CARRIED,
  PLAYER_PROP_KNIFE_CARRIED,
  PLAYER_PROP_PISTOL_AIMED,
  PLAYER_PROP_KNIFE_THRUST,
  PLAYER_PROP_THROW,
  PLAYER_PROP_BAZOOKA
} PlayerProp;

/* The pistol held out along the line of the arm, over the fist that holds it:
   slide, the lit line along its top, and the grip under the hand. */
static void draw_pistol_aimed(const ChuckView *v, ChuckPoint hand)
{
  chuck_view_rect(v, hand.x - 1.0f, hand.y - 2.8f, 8.6f, 3.4f, COL_OUTLINE);
  chuck_view_rect(v, hand.x - 0.2f, hand.y - 0.2f, 2.4f, 3.2f, COL_OUTLINE);
  chuck_view_rect(v, hand.x + 0.2f, hand.y - 0.2f, 1.6f, 2.4f, PLAYER_GRIP);
  chuck_view_rect(v, hand.x, hand.y - 1.8f, 6.6f, 1.8f, PLAYER_GUNMETAL);
  chuck_view_rect(v, hand.x, hand.y - 1.8f, 6.6f, 0.8f,
                  fx_ramp(PLAYER_GUNMETAL).lit);
}

/*
 * Something carried in the near hand while the arm is doing something else —
 * swinging with the run, hanging at his side — pointed on along the forearm and
 * tipped a little forward of it, the way a man carries a pistol or a blade low
 * without thinking about it. It is the one thing that says what the next press
 * of the trigger will do: shoot, or stab.
 */
static void draw_carried(const ChuckView *v, const ChuckPose *pose,
                         PlayerProp prop)
{
  ChuckPoint e = pose->elbow[CHUCK_NEAR];
  ChuckPoint h = pose->hand[CHUCK_NEAR];
  float dx = h.x - e.x;
  float dy = h.y - e.y;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.001f)
    return;
  dx = dx / len + 0.55f;
  dy = dy / len;
  len = sqrtf(dx * dx + dy * dy);
  dx /= len;
  dy /= len;

  if (prop == PLAYER_PROP_PISTOL_CARRIED)
  {
    ChuckPoint a = {h.x + dx * 0.4f, h.y + dy * 0.4f};
    ChuckPoint b = {h.x + dx * 4.4f, h.y + dy * 4.4f};
    chuck_view_band(v, a, b, 3.0f, COL_OUTLINE);
    chuck_view_band(v, a, b, 1.2f, PLAYER_GUNMETAL);
  }
  else
  {
    ChuckPoint a = {h.x + dx * 1.2f, h.y + dy * 1.2f};
    ChuckPoint b = {h.x + dx * 4.6f, h.y + dy * 4.6f};
    chuck_view_band(v, a, b, 2.2f, COL_OUTLINE);
    chuck_view_band(v, a, b, 1.0f, KNIFE_STEEL);
  }
}

static void draw_player_side(SDL_Renderer *r, const Player *p, float x,
                             float y, float land_squash)
{
  int dir = p->facing;
  bool moving = fabsf(p->vx) > 2.0f;
  bool airborne = !p->on_ground;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  /*
   * The underarm throw, on the ground. Every throw sets `grenade_throwing`
   * and `action_timer` exactly as a shot does, and for a long time the
   * standing pose had no branch for it — so a grenade, a flash charge or a
   * bolt thrown on the floor fell through to the sidearm, and was drawn as a
   * pistol going off, muzzle flash and all, beside the thing he had just
   * lobbed.
   */
  bool throwing = p->action_timer > 0.0f && p->grenade_throwing && !knife;
  bool firing = p->action_timer > 0.0f && !knife && !throwing;
  bool bazooka = (p->active_weapon == PLAYER_WEAPON_BAZOOKA &&
                   p->bazooka_rockets > 0) ||
                 (firing && p->bazooka_firing);
  PlayerProp prop = PLAYER_PROP_NONE;
  ChuckPose pose;

  if (knife)
    prop = PLAYER_PROP_KNIFE_THRUST;
  else if (throwing)
    prop = PLAYER_PROP_THROW;
  else if (bazooka)
    prop = PLAYER_PROP_BAZOOKA;
  else if (firing)
    prop = PLAYER_PROP_PISTOL_AIMED;
  else if (p->active_weapon == PLAYER_WEAPON_PISTOL && p->bullets > 0)
    prop = PLAYER_PROP_PISTOL_CARRIED;
  else if (p->active_weapon == PLAYER_WEAPON_KNIFE ||
           p->active_weapon == PLAYER_WEAPON_PISTOL)
    /* With a dry clip or the knife picked the trigger stabs rather than
       shoots, and the hand that used to carry the pistol and then carried
       nothing was the only thing that could have said which. */
    prop = PLAYER_PROP_KNIFE_CARRIED;

  /*
   * The legs and the body. The gait's place in its cycle comes from how far he
   * has travelled rather than from a clock: a planted foot then stays exactly
   * where it was put on the floor while the hips pass over it, which is the
   * whole of the difference between running and skating. `facing` turns the
   * distance into distance forward, and a turn is a pop either way because the
   * sprite mirrors on it.
   */
  if (airborne)
  {
    chuck_pose_air(&pose, -p->vy / PLAYER_JUMP_SPEED);
  }
  else if (moving)
  {
    /* Hauling a body is a walk: nobody runs with a man's collar in his fist. */
    ChuckGait gait = p->dragging ? CHUCK_GAIT_WALK : CHUCK_GAIT_RUN;
    chuck_pose_gait(&pose, gait,
                    chuck_gait_cycle(gait, p->x * (float)p->facing));
  }
  else
  {
    chuck_pose_stand(&pose, sinf(p->anim_time * 2.0f));
  }
  /*
   * Squash. Two or three pixels is all a thirty-two pixel figure can take
   * before it turns into rubber, and it is the difference between a jump with
   * weight and one that teleports. The hips drop with the feet left where they
   * are, so the knees take it rather than the whole figure shrinking.
   */
  if (land_squash > 0.0f)
    chuck_pose_sink(&pose, land_squash * 2.8f);

  /* An arm doing something swings the other one back against it — which is
     half of what makes a throw read as a throw. */
  if (prop == PLAYER_PROP_KNIFE_THRUST)
  {
    pose.arm_swing[CHUCK_FAR] = -0.75f;
    pose.arm_bend[CHUCK_FAR] = 0.9f;
  }
  else if (prop == PLAYER_PROP_THROW)
  {
    pose.arm_swing[CHUCK_FAR] = -1.15f;
    pose.arm_bend[CHUCK_FAR] = 0.45f;
  }
  chuck_pose_solve(&pose);

  ChuckPoint sh = pose.shoulder[CHUCK_NEAR];
  ChuckHand near_hand = CHUCK_HAND_OPEN;
  ChuckHand far_hand = CHUCK_HAND_OPEN;
  float recoil = p->action_timer > 0.075f ? -1.0f : 0.0f;
  float bazooka_lx = 12.5f;
  float bazooka_ly = sh.y - 6.5f;

  switch (prop)
  {
  case PLAYER_PROP_KNIFE_THRUST:
  {
    float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f ? 2.0f
                                                                      : 0.0f;
    chuck_pose_reach(&pose, CHUCK_NEAR,
                     (ChuckPoint){sh.x + 6.2f + thrust, sh.y + 1.2f});
    near_hand = CHUCK_HAND_GRIP;
    break;
  }
  case PLAYER_PROP_THROW:
  {
    /* The throw leaves the hand the instant it is pressed (the projectile is
       its own sprite from that frame on), so what the figure shows is the
       release and the follow-through: the arm coming up through the front of
       the swing, and an open hand. */
    float follow = 1.0f - p->action_timer / PLAYER_THROW_ACTION_TIME;
    follow = follow < 0.0f ? 0.0f : (follow > 1.0f ? 1.0f : follow);
    ChuckPoint hand = {sh.x + 4.8f + follow * 2.4f, sh.y + 7.0f - follow * 8.0f};
    if (p->shot_vertical < 0)
      hand = (ChuckPoint){sh.x + 2.0f, sh.y - 8.4f - follow * 0.5f};
    else if (p->shot_vertical > 0)
      hand = (ChuckPoint){sh.x + 6.4f, sh.y + 6.2f};
    chuck_pose_reach(&pose, CHUCK_NEAR, hand);
    break;
  }
  case PLAYER_PROP_BAZOOKA:
    /* The tube on the shoulder, the near hand on its grip and the far one
       steadying it underneath further forward. */
    chuck_pose_reach(&pose, CHUCK_NEAR,
                     (ChuckPoint){bazooka_lx + 7.2f, bazooka_ly + 9.8f});
    chuck_pose_reach(&pose, CHUCK_FAR,
                     (ChuckPoint){bazooka_lx + 14.0f, bazooka_ly + 8.8f});
    near_hand = CHUCK_HAND_GRIP;
    far_hand = CHUCK_HAND_GRIP;
    break;
  case PLAYER_PROP_PISTOL_AIMED:
    /* Out along the line the round will take, the far hand coming up under
       the near one: two hands on a pistol is what a man trained to use one
       does with it. */
    chuck_pose_reach(&pose, CHUCK_NEAR,
                     (ChuckPoint){sh.x + 8.2f + recoil, sh.y + 0.8f});
    chuck_pose_reach(&pose, CHUCK_FAR,
                     (ChuckPoint){sh.x + 7.2f + recoil, sh.y + 2.0f});
    near_hand = CHUCK_HAND_GRIP;
    far_hand = CHUCK_HAND_GRIP;
    break;
  case PLAYER_PROP_PISTOL_CARRIED:
  case PLAYER_PROP_KNIFE_CARRIED:
    near_hand = CHUCK_HAND_GRIP;
    break;
  case PLAYER_PROP_NONE:
    break;
  }

  ChuckView view = player_view(r, x, y, dir);
  chuck_draw_arm(&view, &pose, CHUCK_FAR, far_hand);
  chuck_draw_legs(&view, &pose);
  /* The launcher rides on the far shoulder, so the body and the head go on in
     front of it: laid on the near one it sat across his face, and the one
     thing a player aiming a rocket needs to see is which way the man is
     looking. */
  if (prop == PLAYER_PROP_BAZOOKA)
    draw_bazooka_weapon(r, x, y, PLAYER_W, dir, bazooka_lx, bazooka_ly,
                        p->bazooka_firing);
  chuck_draw_torso(&view, &pose);
  chuck_draw_head(&view, &pose, fx_blinking(p->anim_time, 0x1u));

  switch (prop)
  {
  case PLAYER_PROP_BAZOOKA:
    chuck_draw_arm(&view, &pose, CHUCK_NEAR, near_hand);
    break;
  case PLAYER_PROP_PISTOL_AIMED:
  {
    ChuckPoint hand = pose.hand[CHUCK_NEAR];
    draw_pistol_aimed(&view, hand);
    chuck_draw_arm(&view, &pose, CHUCK_NEAR, near_hand);
    if (p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
    {
      float mx = hand.x + 7.0f;
      float my = hand.y - 1.0f;
      draw_muzzle_flash(r, x, y, PLAYER_W, dir, mx + 1.5f, my, FX_AMBER);
      chuck_view_rect(&view, mx, my - 3.0f, 3.5f, 5.0f, FX_AMBER);
      chuck_view_rect(&view, mx + 2.5f, my - 1.5f, 2.5f, 3.0f, MUZZLE_HOT);
    }
    break;
  }
  case PLAYER_PROP_KNIFE_THRUST:
  {
    ChuckPoint hand = pose.hand[CHUCK_NEAR];
    chuck_draw_arm(&view, &pose, CHUCK_NEAR, near_hand);
    chuck_view_rect(&view, hand.x + 1.6f, hand.y - 2.0f, 7.4f, 3.2f,
                    COL_OUTLINE);
    chuck_view_rect(&view, hand.x + 1.6f, hand.y - 1.4f, 1.2f, 2.2f,
                    KNIFE_HANDLE);
    chuck_view_rect(&view, hand.x + 2.8f, hand.y - 1.0f, 5.2f, 1.4f,
                    KNIFE_STEEL);
    chuck_view_rect(&view, hand.x + 8.0f, hand.y - 0.6f, 1.0f, 0.8f,
                    KNIFE_TIP);
    break;
  }
  case PLAYER_PROP_PISTOL_CARRIED:
  case PLAYER_PROP_KNIFE_CARRIED:
    draw_carried(&view, &pose, prop);
    chuck_draw_arm(&view, &pose, CHUCK_NEAR, near_hand);
    break;
  case PLAYER_PROP_THROW:
  case PLAYER_PROP_NONE:
    chuck_draw_arm(&view, &pose, CHUCK_NEAR, near_hand);
    break;
  }
}

void draw_player(SDL_Renderer *r, const Player *p, const Level *level,
                        float cam_x, float oy, bool hacking, float hacking_time,
                        float land_squash)
{
  /* Whole pixels, so every part of him moves as one piece: a sprite whose
     parts each round their own fraction of a pixel shimmers as it travels. */
  float x = floorf(p->x - cam_x);
  float y = floorf(p->y + oy);
  bool climbing = p->on_ladder || p->facade_climbing;

  if (hacking)
  {
    /* Every special pose gets the same floor-anchored pool as the standing
       figure; a pose is not a reason for the shadow to jump to the boots. */
    npc_contact_shadow(r, level, p->x + PLAYER_W * 0.5f, p->y + PLAYER_H,
                       11.0f, 195, cam_x, oy);
    draw_player_hacking(r, x, y, hacking_time);
    return;
  }

  if (p->crawling)
  {
    npc_contact_shadow(r, level, p->x + PLAYER_W * 0.5f,
                       p->y + PLAYER_CRAWL_H, 13.0f, 190, cam_x, oy);
    draw_player_crawling(r, p, x, y);
    return;
  }

  float shadow_y;
  float shadow_lift;
  if (level != NULL &&
      character_ground(level, p->x + PLAYER_W * 0.5f, p->y + PLAYER_H,
                       &shadow_y, &shadow_lift))
  {
    fx_contact_shadow(r, x + PLAYER_W * 0.5f, shadow_y + oy - 1.0f,
                      11.0f, shadow_lift, 205);
  }

  if (climbing)
    draw_player_climbing(r, p, x, y);
  else
    draw_player_side(r, p, x, y, land_squash);
}

void draw_janitor(SDL_Renderer *r, const Janitor *janitor,
                         const Level *level,
                         float cam_x, float oy)
{
  float x = janitor->x - cam_x;
  float y = janitor->y + oy;
  int dir = janitor->dir;
  int cart_dir = janitor->cart_dir;
  bool walking = janitor->activity == JANITOR_WALK &&
                 fabsf(janitor->vx) > 2.0f;
  bool mopping = janitor->activity == JANITOR_MOP;
  float phase = janitor->anim_time * 2.2f;
  float cycle = phase * (1.0f / 6.28318531f);
  /* The near foot's reach, on the legs' own clock — see `draw_walking_leg`. */
  float step = walking ? cosf(phase) : 0.0f;
  float bob = walking ? fabsf(step) * 0.45f
                      : sinf(janitor->anim_time * 1.6f) * 0.25f;
  float sweep = mopping ? sinf(janitor->anim_time * 4.5f) * 8.0f : 0.0f;
  SDL_Color uniform = {38, 78, 82, 255};
  SDL_Color uniform_hi = {50, 102, 105, 255};
  SDL_Color skin = {136, 101, 79, 255};

  /* The translucent streaks are presentation-only state owned by this NPC.
   * They fade out without changing friction or any other gameplay rule. */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < JANITOR_WET_SPOTS; ++i)
  {
    const JanitorWetSpot *spot = &janitor->wet_spots[i];
    if (!spot->active)
      continue;
    float fade = spot->life / JANITOR_WET_LIFETIME;
    Uint8 alpha = (Uint8)(10.0f + fade * 38.0f);
    set_rgba(r, 63, 135, 142, alpha);
    fill_rect(r, spot->x - cam_x - 10.0f, spot->y + oy - 1.0f,
              20.0f, 3.0f);
    set_rgba(r, 118, 164, 164, (Uint8)(alpha * 0.55f));
    fill_rect(r, spot->x - cam_x - 5.0f, spot->y + oy - 1.0f,
              7.0f, 1.0f);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  /* During a turn the cart stays on its clear side until there is enough room
   * to place it behind the janitor again. */
  float cart_x = cart_dir > 0 ? x - 25.0f : x + JANITOR_W + 3.0f;
  float cart_y = y + 7.0f;
  int cd = cart_dir > 0 ? 1 : -1;
  /* Short contact shadows keep the two silhouettes grounded without joining
   * them into one long, high-contrast stripe. */
  npc_contact_shadow(r, level, janitor->x + JANITOR_W * 0.5f,
                     janitor->y + 31.0f, 9.0f, 200, cam_x, oy);
  npc_contact_shadow(r, level, cart_x + cam_x + 11.0f,
                     janitor->y + 31.0f, 11.0f, 200, cam_x, oy);

  /*
   * The cart. It was one dark box with a stripe across it, which at this
   * size is a crate on castors, and the one prop that says what this man is
   * for. So it is the three things a janitor's cart is: a mop bucket with the
   * wringer clamped on it, a bin with the bag's lip folded over its rim, and a
   * steel deck on four wheels with a push bar at his end. Each is its own lit
   * form, and all of it is laid out along the cart's own facing so the push
   * bar is always the end nearest the man.
   */
  {
    SDL_Color bucket = {142, 112, 54, 255};
    SDL_Color steel = {88, 96, 96, 255};
    SDL_Color bin = {43, 79, 91, 255};
    SDL_Color bag = {27, 31, 34, 255};
    SDL_Color rubber = {23, 29, 33, 255};
    FxRamp bucket_ramp = fx_ramp(bucket);
    FxRamp steel_ramp = fx_ramp(steel);

    /* Push bar, behind everything, rising from the deck at his end. */
    sprite_segment(r, cart_x, cart_y, 23.0f, cd, 21.0f, 17.0f, 23.5f, -1.0f,
                   3, COL_OUTLINE);
    sprite_segment(r, cart_x, cart_y, 23.0f, cd, 21.0f, 17.0f, 23.5f, -1.0f,
                   1, steel);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 21.5f, -2.0f, 4.0f, 3.0f,
                COL_OUTLINE);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 22.5f, -1.0f, 2.0f, 1.0f,
                FX_INK);

    /* The bin and its bag, the tall half of the silhouette. */
    sprite_body(r, cart_x, cart_y, 23.0f, cd, 12.0f, 5.0f, 9.0f, 13.0f, bin,
                COL_OUTLINE, 0, 0);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 14.0f, 8.0f, 1.0f, 9.0f,
                fx_ramp(bin).dark);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 18.0f, 8.0f, 1.0f, 9.0f,
                fx_ramp(bin).dark);
    sprite_mass(r, cart_x, cart_y, 23.0f, cd, 11.0f, 2.0f, 11.0f, 5.0f,
                COL_OUTLINE, 2, 0);
    sprite_mass(r, cart_x, cart_y, 23.0f, cd, 12.0f, 3.0f, 9.0f, 3.0f, bag,
                1, 0);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 14.0f, 3.0f, 3.0f, 1.0f,
                fx_ramp(bag).lit);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 11.5f, 5.0f, 10.0f, 1.0f,
                fx_mix(bag, bin, 0.5f));

    /* The bucket, its water, and the wringer on top of it. */
    sprite_body(r, cart_x, cart_y, 23.0f, cd, 1.0f, 10.0f, 10.0f, 8.0f,
                bucket, COL_OUTLINE, 0, 1);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 1.0f, 10.0f, 10.0f, 1.0f,
                bucket_ramp.lit);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 2.0f, 15.0f, 8.0f, 1.0f,
                bucket_ramp.dark);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 2.0f, 11.0f, 3.0f, 1.0f,
                (SDL_Color){63, 118, 124, 255});
    sprite_body(r, cart_x, cart_y, 23.0f, cd, 5.0f, 6.0f, 5.0f, 4.0f, steel,
                COL_OUTLINE, 1, 0);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 6.0f, 8.0f, 3.0f, 1.0f,
                steel_ramp.dark);

    /* The deck, and the castors under it. */
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 0.0f, 17.0f, 23.0f, 4.0f,
                COL_OUTLINE);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 1.0f, 18.0f, 21.0f, 2.0f,
                steel_ramp.dark);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 1.0f, 18.0f, 21.0f, 1.0f,
                steel);
    for (int wheel = 0; wheel < 2; ++wheel)
    {
      float wx = wheel == 0 ? 2.0f : 16.0f;
      sprite_mass(r, cart_x, cart_y, 23.0f, cd, wx, 20.0f, 6.0f, 5.0f,
                  COL_OUTLINE, 1, 1);
      sprite_mass(r, cart_x, cart_y, 23.0f, cd, wx + 1.0f, 21.0f, 4.0f, 3.0f,
                  rubber, 1, 1);
      sprite_rect(r, cart_x, cart_y, 23.0f, cd, wx + 2.0f, 22.0f, 2.0f, 1.0f,
                  steel_ramp.lit);
    }
  }

  /* The mop is clipped to the cart during a patrol, standing in the bucket
   * behind the wringer; while he works it is in his hands (below). */
  if (!mopping)
  {
    sprite_segment(r, cart_x, cart_y, 23.0f, cd, 6.0f, 9.0f, 10.0f, -13.0f,
                   3, COL_OUTLINE);
    sprite_segment(r, cart_x, cart_y, 23.0f, cd, 6.0f, 9.0f, 10.0f, -13.0f,
                   1, (SDL_Color){130, 112, 82, 255});
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 3.0f, 8.0f, 7.0f, 3.0f,
                COL_OUTLINE);
    sprite_rect(r, cart_x, cart_y, 23.0f, cd, 4.0f, 8.0f, 5.0f, 2.0f,
                (SDL_Color){97, 132, 130, 255});
  }

  /* Work trousers, a clear step under the tunic. His teal is the darkest
     garment in the cast to begin with, so legs drawn at the tunic's own value
     left nothing on him for the eye to catch but the reflective band. */
  if (walking)
  {
    draw_walking_leg(r, x, y, JANITOR_W, dir, 12.0f, 21.0f + bob,
                     cycle + 0.5f, 2.8f, (SDL_Color){22, 33, 36, 255},
                     (SDL_Color){24, 27, 32, 255});
    draw_walking_leg(r, x, y, JANITOR_W, dir, 14.0f, 21.0f + bob,
                     cycle, 2.8f, (SDL_Color){28, 42, 45, 255},
                     (SDL_Color){31, 35, 41, 255});
  }
  else
  {
    draw_standing_legs(r, x, y, JANITOR_W, dir, 9.0f, 14.0f, 22.0f,
                       (SDL_Color){22, 33, 36, 255},
                       (SDL_Color){28, 42, 45, 255},
                       (SDL_Color){29, 33, 39, 255});
  }

  sprite_body(r, x, y, JANITOR_W, dir,
              7.0f, 11.0f + bob, 13.0f, 12.0f, uniform, COL_OUTLINE, 2, 1);
  sprite_rect(r, x, y, JANITOR_W, dir,
              8.0f, 12.0f + bob, 10.0f, 2.0f, uniform_hi);
  /* A muted service vest keeps the role legible without competing with
   * pickups, enemies, or the player's brighter silhouette. The reflective
   * band across it is what makes it read as workwear rather than as a shirt. */
  sprite_rect(r, x, y, JANITOR_W, dir,
              11.0f, 11.0f + bob, 4.0f, 12.0f,
              (SDL_Color){139, 118, 63, 255});
  sprite_rect(r, x, y, JANITOR_W, dir,
              11.0f, 11.0f + bob, 4.0f, 1.0f,
              (SDL_Color){186, 162, 96, 255});
  sprite_rect(r, x, y, JANITOR_W, dir,
              8.0f, 20.0f + bob, 12.0f, 2.0f,
              (SDL_Color){139, 118, 63, 255});

  sprite_body(r, x, y, JANITOR_W, dir,
              10.0f, 4.0f + bob, 8.0f, 7.0f, skin, COL_OUTLINE, 0, 2);
  /* Cap with a peak, and the shade the peak drops on the brow. */
  sprite_mass(r, x, y, JANITOR_W, dir,
              8.0f, 0.0f + bob, 12.0f, 6.0f, COL_OUTLINE, 3, 0);
  sprite_mass(r, x, y, JANITOR_W, dir,
              9.0f, 1.0f + bob, 10.0f, 5.0f,
              (SDL_Color){42, 87, 91, 255}, 2, 0);
  sprite_mass(r, x, y, JANITOR_W, dir,
              10.0f, 1.0f + bob, 8.0f, 2.0f,
              (SDL_Color){58, 112, 116, 255}, 1, 0);
  sprite_rect(r, x, y, JANITOR_W, dir,
              16.0f, 5.0f + bob, 5.0f, 1.0f,
              (SDL_Color){28, 62, 66, 255});
  sprite_rect(r, x, y, JANITOR_W, dir,
              10.0f, 6.0f + bob, 8.0f, 1.0f,
              (SDL_Color){112, 82, 64, 255});
  /* Jaw, and grey stubble along it — he has been on since before the shift. */
  sprite_mass(r, x, y, JANITOR_W, dir,
              10.0f, 9.0f + bob, 8.0f, 2.0f,
              (SDL_Color){104, 78, 62, 255}, 1, 2);
  if (fx_blinking(janitor->anim_time, 0x27u))
  {
    sprite_rect(r, x, y, JANITOR_W, dir,
                14.0f, 8.0f + bob, 3.0f, 1.0f,
                (SDL_Color){17, 28, 29, 255});
  }
  else
  {
    /* Mostly iris, at the front of a dimmed white — see the receptionist's
       note: the old bright white with a single dark pixel read as a monocle
       under the cap's peak. */
    sprite_rect(r, x, y, JANITOR_W, dir,
                14.0f, 7.0f + bob, 3.0f, 2.0f,
                (SDL_Color){150, 158, 152, 255});
    sprite_rect(r, x, y, JANITOR_W, dir,
                15.0f, 7.0f + bob, 2.0f, 2.0f,
                (SDL_Color){17, 28, 29, 255});
  }

  if (mopping)
  {
    /*
     * The mop in his hands and in front of him. It used to be drawn before the
     * legs, so the one thing he was doing was hidden behind the man doing it:
     * the handle vanished into the trousers and what was left was a flat bar
     * sliding about beside his boots. Now the pole runs from over his shoulder
     * to the floor ahead, both hands on it, and it pivots between them as the
     * head swings — the top of the pole goes back as the head goes out.
     */
    float top_x = 10.0f - sweep * 0.30f;
    float top_y = 11.0f + bob;
    float foot_x = 26.0f + sweep;
    float foot_y = 29.0f;
    float grip_hi_t = 0.22f;
    float grip_lo_t = 0.46f;
    float hi_x = top_x + (foot_x - top_x) * grip_hi_t;
    float hi_y = top_y + (foot_y - top_y) * grip_hi_t;
    float lo_x = top_x + (foot_x - top_x) * grip_lo_t;
    float lo_y = top_y + (foot_y - top_y) * grip_lo_t;

    sprite_segment(r, x, y, JANITOR_W, dir, top_x, top_y, foot_x, foot_y, 3,
                   COL_OUTLINE);
    sprite_segment(r, x, y, JANITOR_W, dir, top_x, top_y, foot_x, foot_y, 1,
                   (SDL_Color){130, 112, 82, 255});
    /* A string mop: a socket on the pole and a skirt of strands splayed on
       the floor, not a flat board. */
    sprite_rect(r, x, y, JANITOR_W, dir, foot_x - 2.0f, 27.0f, 4.0f, 3.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, JANITOR_W, dir, foot_x - 6.0f, 29.0f, 12.0f, 3.0f,
                COL_OUTLINE);
    for (int strand = 0; strand < 5; ++strand)
    {
      float sx = foot_x - 5.0f + (float)strand * 2.0f;
      sprite_rect(r, x, y, JANITOR_W, dir, sx, 29.0f, 1.0f, 3.0f,
                  (SDL_Color){97, 132, 130, 255});
    }
    sprite_rect(r, x, y, JANITOR_W, dir, foot_x - 4.0f, 29.0f, 8.0f, 1.0f,
                (SDL_Color){132, 166, 162, 255});

    sprite_limb_segment(r, x, y, JANITOR_W, dir, 16.0f, 13.0f + bob,
                        hi_x, hi_y, uniform_hi);
    sprite_limb_segment(r, x, y, JANITOR_W, dir, 14.0f, 14.0f + bob,
                        lo_x - 1.0f, lo_y + 1.5f, uniform);
    sprite_limb_segment(r, x, y, JANITOR_W, dir, lo_x - 1.0f, lo_y + 1.5f,
                        lo_x, lo_y, uniform);
    draw_closed_hand(r, x, y, JANITOR_W, dir, hi_x, hi_y, skin);
    draw_closed_hand(r, x, y, JANITOR_W, dir, lo_x, lo_y, skin);
  }
  else
  {
    float arm_swing = walking ? -step : 0.0f;
    draw_walking_arm(r, x, y, JANITOR_W, dir,
                     17.0f, 13.0f + bob, arm_swing, uniform, skin);
  }
}

/* Three people, not three palettes of the same person: the office worker, the
 * receptionist off the front desk, and a visitor still holding his case. */
typedef struct
{
  SDL_Color cloth;
  SDL_Color cloth_hi;
  SDL_Color legs;
  SDL_Color legs_hi;
  SDL_Color shoe;
  SDL_Color hair;
  SDL_Color skin;
  SDL_Color accent;
  bool carries_case;
} CivilianLook;

static const CivilianLook CIVILIAN_LOOKS[CIVILIAN_VARIANTS] = {
    {{212, 218, 226, 255},
     {236, 240, 245, 255},
     {46, 54, 74, 255},
     {58, 68, 90, 255},
     {42, 45, 54, 255},
     {62, 44, 33, 255},
     {214, 166, 124, 255},
     {150, 52, 54, 255},
     false},
    {{128, 58, 72, 255},
     {160, 78, 94, 255},
     {58, 34, 44, 255},
     {74, 46, 58, 255},
     {50, 40, 46, 255},
     {172, 122, 66, 255},
     {224, 178, 138, 255},
     {226, 214, 198, 255},
     false},
    {{88, 94, 92, 255},
     {112, 120, 116, 255},
     {52, 52, 50, 255},
     {66, 66, 62, 255},
     {48, 45, 42, 255},
     {40, 34, 30, 255},
     {162, 118, 88, 255},
     {126, 132, 126, 255},
     true}};

static SDL_Color civilian_fade(SDL_Color c, float fade)
{
  float alpha = (float)c.a * fade;
  c.a = (Uint8)(alpha < 0.0f ? 0.0f : (alpha > 255.0f ? 255.0f : alpha));
  return c;
}

/*
 * A civilian getting out of the building. The pose carries the whole read at
 * this size: a panicked run is forward pitch, a long stride and raised hands,
 * where the walk the guards and the janitor share is deliberately level. The
 * dissolve at the doors is drawn as plain alpha rather than as a shrink or a
 * step out of frame, because the doorway itself is on a parallax layer and
 * anything else would have to agree with it.
 */
void draw_civilian(SDL_Renderer *r, const Civilian *civilian,
                          const Level *level,
                          float cam_x, float oy)
{
  if (civilian->activity == CIVILIAN_GONE || civilian->fade <= 0.0f)
    return;

  const CivilianLook *look = &CIVILIAN_LOOKS[civilian->variant %
                                             CIVILIAN_VARIANTS];
  float fade = civilian->fade;
  float x = civilian->x - cam_x;
  float y = civilian->y + oy;
  int dir = civilian->dir;
  bool running = civilian->activity == CIVILIAN_FLEEING;
  bool fallen = civilian->activity == CIVILIAN_STUMBLING;
  bool startled = civilian->activity == CIVILIAN_STARTLED;
  float phase = civilian->anim_time * 2.6f;
  float cycle = phase * (1.0f / 6.28318531f);
  /* The near foot's reach, on the legs' own clock — see `draw_walking_leg` — which
     is what the arms swing against. The bob stays on the sine: this is a run
     rather than a walk, and a runner is lowest at mid-stance, where the knee
     takes the landing, and highest in the air between strides. */
  float step = running ? cosf(phase) : 0.0f;
  /* How far into the sprawl this frame is: 1 while down, easing to 0 as the
     last of the beat is spent scrambling up. */
  float down = fallen ? fminf(1.0f, civilian->activity_timer /
                                        (CIVILIAN_STUMBLE_TIME * 0.35f))
                      : 0.0f;
  float drop = down * 9.0f;
  float bob = running ? fabsf(sinf(phase)) * 1.2f - 0.6f
                      : sinf(civilian->anim_time * 2.2f) * 0.3f;
  float lean = running ? 2.5f : (startled ? -1.5f : 3.0f * down);
  float body = bob + drop;
  SDL_Color cloth = civilian_fade(look->cloth, fade);
  SDL_Color cloth_hi = civilian_fade(look->cloth_hi, fade);
  SDL_Color legs = civilian_fade(look->legs, fade);
  SDL_Color legs_hi = civilian_fade(look->legs_hi, fade);
  SDL_Color shoe = civilian_fade(look->shoe, fade);
  SDL_Color skin = civilian_fade(look->skin, fade);
  SDL_Color outline = civilian_fade(COL_OUTLINE, fade);

  if (fade < 1.0f)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  /* Anchored to the floor below rather than gated on contact: a shadow that
     popped out of existence mid-drop said the figure vanished, where one that
     stays on the stair and thins with height says he is over it. */
  npc_contact_shadow(r, level, civilian->x + CIVILIAN_W * 0.5f,
                     civilian->y + 31.0f, 9.0f, (Uint8)(fade * 200.0f),
                     cam_x, oy);

  if (fallen)
  {
    /* Down on one knee with the trailing leg stretched out behind. */
    sprite_limb_segment(r, x, y, CIVILIAN_W, dir, 11.0f, 21.0f + body,
                        6.0f - down * 3.0f, 29.0f, legs);
    sprite_limb_segment(r, x, y, CIVILIAN_W, dir, 12.0f, 21.0f + body,
                        16.0f, 30.0f - down * 2.0f, legs);
    sprite_rect(r, x, y, CIVILIAN_W, dir, 3.0f - down * 3.0f, 28.0f,
                6.0f, 3.0f, shoe);
    sprite_rect(r, x, y, CIVILIAN_W, dir, 15.0f, 29.0f - down * 2.0f,
                6.0f, 3.0f, shoe);
  }
  else if (running)
  {
    draw_walking_leg(r, x, y, CIVILIAN_W, dir, 10.0f, 21.0f + body,
                     cycle + 0.5f, 5.2f, legs, shoe);
    draw_walking_leg(r, x, y, CIVILIAN_W, dir, 13.0f, 21.0f + body,
                     cycle, 5.2f, legs_hi, shoe);
  }
  else
  {
    draw_standing_legs(r, x, y, CIVILIAN_W, dir, 9.0f, 13.0f, 22.0f + body,
                       legs, legs_hi, shoe);
  }

  sprite_body(r, x, y, CIVILIAN_W, dir, 6.0f + lean, 11.0f + body,
              11.0f, 11.0f, cloth, outline, 2, 1);
  /* Shoulder line and a lapel: office clothes, on someone whose day has just
     stopped being about the office. */
  sprite_rect(r, x, y, CIVILIAN_W, dir, 7.0f + lean, 12.0f + body,
              9.0f, 2.0f, cloth_hi);
  sprite_rect(r, x, y, CIVILIAN_W, dir, 14.0f + lean, 13.0f + body,
              2.0f, 4.0f, civilian_fade(look->legs, fade));
  sprite_rect(r, x, y, CIVILIAN_W, dir, 11.0f + lean, 11.0f + body,
              2.0f, 8.0f, civilian_fade(look->accent, fade));

  sprite_body(r, x, y, CIVILIAN_W, dir, 9.0f + lean, 3.0f + body,
              7.0f, 7.0f, skin, outline, 0, 2);
  sprite_mass(r, x, y, CIVILIAN_W, dir, 8.0f + lean, 0.0f + body,
              9.0f, 4.0f, outline, 3, 0);
  sprite_mass(r, x, y, CIVILIAN_W, dir, 9.0f + lean, 1.0f + body,
              7.0f, 4.0f, civilian_fade(look->hair, fade), 2, 0);
  sprite_rect(r, x, y, CIVILIAN_W, dir, 9.0f + lean, 5.0f + body,
              7.0f, 1.0f, civilian_fade(fx_ramp(look->skin).dark, fade));
  /* Eyes wide: whites all round the pupil, which is the difference between
     alarmed and asleep, and the one thing that must not blink here. */
  sprite_rect(r, x, y, CIVILIAN_W, dir, 12.0f + lean, 6.0f + body,
              3.0f, 2.0f, civilian_fade((SDL_Color){228, 236, 226, 255}, fade));
  sprite_rect(r, x, y, CIVILIAN_W, dir, 13.0f + lean, 6.0f + body,
              2.0f, 2.0f, civilian_fade(FX_INK, fade));
  /* The open mouth is the one cue that reads as fear at this scale — as a
     small O under the eye. It was a four-pixel bar across the lower face with
     a wedge under it, and on a seven-pixel face that is a black beard: from
     across the lobby the people running *from* the gunmen read as masked. */
  sprite_rect(r, x, y, CIVILIAN_W, dir, 13.0f + lean, 8.0f + body,
              2.0f, 2.0f, civilian_fade((SDL_Color){48, 22, 24, 255}, fade));
  sprite_rect(r, x, y, CIVILIAN_W, dir, 13.0f + lean, 8.0f + body,
              2.0f, 1.0f, civilian_fade((SDL_Color){28, 12, 14, 255}, fade));
  /* And the brow lifted over the wide eye, a pixel of shade on the skin
     rather than on the hairline. */
  sprite_rect(r, x, y, CIVILIAN_W, dir, 12.0f + lean, 5.0f + body,
              3.0f, 1.0f, civilian_fade(fx_mix(look->skin, look->hair, 0.5f),
                                        fade));

  if (fallen)
  {
    /* One hand braced on the floor, the other still thrown out ahead. */
    sprite_limb_segment(r, x, y, CIVILIAN_W, dir, 15.0f + lean, 14.0f + body,
                        21.0f, 27.0f - down * 3.0f, cloth);
    sprite_limb_segment(r, x, y, CIVILIAN_W, dir, 9.0f + lean, 14.0f + body,
                        14.0f, 24.0f, cloth_hi);
    sprite_rect(r, x, y, CIVILIAN_W, dir, 19.0f, 25.0f - down * 3.0f,
                4.0f, 3.0f, skin);
  }
  else if (look->carries_case)
  {
    /* The case is the joke and the tell: he has not thought to drop it. */
    float swing = running ? step * 2.5f : 0.0f;
    draw_walking_arm(r, x, y, CIVILIAN_W, dir, 13.0f + lean, 13.0f + body,
                     -swing, cloth, skin);
    float case_x = 4.0f + swing;
    sprite_limb_segment(r, x, y, CIVILIAN_W, dir, 9.0f + lean, 13.0f + body,
                        case_x + 2.0f, 20.0f + body, cloth_hi);
    sprite_rect(r, x, y, CIVILIAN_W, dir, case_x - 3.0f, 20.0f + body,
                10.0f, 8.0f, outline);
    sprite_rect(r, x, y, CIVILIAN_W, dir, case_x - 2.0f, 21.0f + body,
                8.0f, 6.0f, civilian_fade((SDL_Color){84, 56, 38, 255}, fade));
    sprite_rect(r, x, y, CIVILIAN_W, dir, case_x - 2.0f, 23.0f + body,
                8.0f, 1.0f, civilian_fade((SDL_Color){132, 100, 62, 255},
                                          fade));
  }
  else
  {
    /* Hands up: to the face while startled, flung overhead once running. */
    float flail = running ? sinf(phase * 1.35f) * 2.2f : 0.0f;
    float reach = startled ? 9.0f : 5.0f;
    sprite_limb_segment(r, x, y, CIVILIAN_W, dir, 9.0f + lean, 13.0f + body,
                        6.0f + lean - flail, reach + body, cloth_hi);
    sprite_limb_segment(r, x, y, CIVILIAN_W, dir, 15.0f + lean, 13.0f + body,
                        18.0f + lean + flail, reach - 1.0f + body, cloth);
    sprite_rect(r, x, y, CIVILIAN_W, dir, 4.0f + lean - flail,
                reach - 2.0f + body, 4.0f, 4.0f, skin);
    sprite_rect(r, x, y, CIVILIAN_W, dir, 16.0f + lean + flail,
                reach - 3.0f + body, 4.0f, 4.0f, skin);
  }

  if (fade < 1.0f)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * The desk, staffed.
 *
 * This one renders on the ambient-staff layer with the janitor and the
 * civilians, so the counter passes in front of it and the post reads as being
 * behind the desk rather than standing on the visitor side of it. Four poses
 * carry the whole part at this size: hands on the keyboard while on post,
 * turned around over a folder during a glance, walking with the folder
 * tucked, and reading it out on the floor. The suit is navy and brass so the
 * figure belongs to the same room as the stone-and-brass counter instead of
 * reading as another guard.
 */
void draw_receptionist(SDL_Renderer *r, const Receptionist *rec,
                              const Level *level,
                              float cam_x, float oy)
{
  float x = rec->x - cam_x;
  float y = rec->y + oy;
  int dir = rec->dir;
  bool walking = rec->activity == RECEPTIONIST_WALK &&
                 fabsf(rec->vx) > 2.0f;
  bool on_post = rec->activity == RECEPTIONIST_DESK && !rec->glancing;
  bool reading = rec->activity == RECEPTIONIST_ERRAND || rec->glancing;
  float phase = rec->anim_time * 2.4f;
  float cycle = phase * (1.0f / 6.28318531f);
  /* The near foot's reach, on the legs' own clock — see `draw_walking_leg`. */
  float step = walking ? cosf(phase) : 0.0f;
  float bob = walking ? fabsf(step) * 0.5f
                      : sinf(rec->anim_time * 1.7f) * 0.3f;
  /* One pixel of shift in the clasped hands is all the movement a 32-pixel
     figure needs to read as waiting on someone rather than as parked. */
  float shift = on_post && sinf(rec->anim_time * 2.9f) > 0.6f ? 1.0f : 0.0f;
  /* The one figure in the lobby wearing the lobby's own colour. Standing at a
     navy counter against a navy curtain wall, a navy suit at the value of the
     trousers under it left her a silhouette; the jacket carries a step up and
     the trousers a step down so there is a person at the desk. */
  SDL_Color suit = {52, 66, 102, 255};
  SDL_Color suit_hi = {74, 92, 138, 255};
  SDL_Color trouser = {26, 30, 44, 255};
  SDL_Color blouse = {202, 208, 218, 255};
  SDL_Color skin = {201, 154, 116, 255};
  SDL_Color hair = {58, 41, 33, 255};
  SDL_Color brass = {158, 132, 86, 255};

  npc_contact_shadow(r, level, rec->x + RECEPTIONIST_W * 0.5f,
                     rec->y + 31.0f, 8.0f, 200, cam_x, oy);

  if (walking)
  {
    draw_walking_leg(r, x, y, RECEPTIONIST_W, dir, 11.0f, 21.0f + bob,
                     cycle + 0.5f, 3.0f, trouser, (SDL_Color){30, 33, 40, 255});
    draw_walking_leg(r, x, y, RECEPTIONIST_W, dir, 13.0f, 21.0f + bob,
                     cycle, 3.0f, (SDL_Color){32, 37, 54, 255},
                     (SDL_Color){35, 39, 47, 255});
  }
  else
  {
    draw_standing_legs(r, x, y, RECEPTIONIST_W, dir, 8.0f, 12.0f, 22.0f,
                       trouser, (SDL_Color){32, 37, 54, 255},
                       (SDL_Color){32, 35, 43, 255});
  }

  sprite_body(r, x, y, RECEPTIONIST_W, dir, 6.0f, 11.0f + bob, 12.0f, 12.0f,
              suit, COL_OUTLINE, 2, 1);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 7.0f, 12.0f + bob, 9.0f, 2.0f,
              suit_hi);
  /* Open collar and the lanyard every visitor is handed one of: the two cues
     that separate front-of-house staff from a guard in a dark jacket. */
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 12.0f, 11.0f + bob, 4.0f, 7.0f,
              blouse);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 11.0f, 11.0f + bob, 1.0f, 8.0f,
              brass);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 10.0f, 18.0f + bob, 4.0f, 3.0f,
              COL_OUTLINE);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 10.0f, 19.0f + bob, 4.0f, 2.0f,
              (SDL_Color){222, 218, 204, 255});

  sprite_body(r, x, y, RECEPTIONIST_W, dir, 9.0f, 4.0f + bob, 8.0f, 7.0f,
              skin, COL_OUTLINE, 0, 2);
  sprite_mass(r, x, y, RECEPTIONIST_W, dir, 8.0f, 0.0f + bob, 10.0f, 5.0f,
              COL_OUTLINE, 3, 0);
  sprite_mass(r, x, y, RECEPTIONIST_W, dir, 9.0f, 1.0f + bob, 8.0f, 4.0f,
              hair, 2, 0);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 11.0f, 1.0f + bob, 4.0f, 1.0f,
              fx_ramp(hair).lit);
  /* The bob, gathered down the back of the neck. */
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 7.0f, 4.0f + bob, 3.0f, 5.0f,
              hair);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 9.0f, 5.0f + bob, 7.0f, 1.0f,
              fx_ramp(skin).dark);
  sprite_mass(r, x, y, RECEPTIONIST_W, dir, 9.0f, 9.0f + bob, 8.0f, 2.0f,
              fx_ramp(skin).dark, 1, 2);
  /* The eye as the rest of the cast draws it: mostly iris at the front of a
     dimmed white. A bright three-pixel white with a one-pixel pupil at its
     tip is the brightest thing on an eight-pixel face, and reads as a monocle
     rather than as an eye. */
  if (fx_blinking(rec->anim_time, 0x5bu))
  {
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 13.0f, 7.0f + bob, 3.0f, 1.0f,
                (SDL_Color){20, 24, 30, 255});
  }
  else
  {
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 13.0f, 6.0f + bob, 3.0f, 2.0f,
                (SDL_Color){172, 178, 176, 255});
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 14.0f, 6.0f + bob, 2.0f, 2.0f,
                (SDL_Color){34, 30, 30, 255});
  }
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 13.0f, 9.0f + bob, 2.0f, 1.0f,
              (SDL_Color){146, 76, 76, 255});
  /* Headset: band, earpiece and a boom down to the mouth. It is what makes a
     figure standing still at a counter read as answering the switchboard.
     In moulded grey rather than in black: a black earpiece against dark hair,
     joined to a black boom across the cheek, was one dark patch over half the
     face, and at this size a patch there is a mask. The earpiece sits on the
     ear, and the boom is one pixel ending in a mic at the mouth. */
  SDL_Color headset = fx_mix(FX_STEEL_DK, FX_STEEL, 0.5f);
  sprite_mass(r, x, y, RECEPTIONIST_W, dir, 9.0f, 0.0f + bob, 8.0f, 1.0f,
              headset, 1, 0);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 9.0f, 1.0f + bob, 1.0f, 4.0f,
              headset);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 9.0f, 5.0f + bob, 2.0f, 2.0f,
              COL_OUTLINE);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 9.0f, 5.0f + bob, 2.0f, 1.0f,
              fx_ramp(headset).lit);
  sprite_segment(r, x, y, RECEPTIONIST_W, dir, 11.0f, 7.0f + bob,
                 13.0f, 8.0f + bob, 1, headset);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 13.0f, 8.0f + bob, 1.0f, 1.0f,
              COL_OUTLINE);

  if (on_post)
  {
    /* Hands clasped in front at the waist. The counter is chin high on this
       figure, so there is nothing to rest an arm on and nothing above it to
       reach for: standing to it is the pose, and the headset and the lanyard
       are what say which side of it this is. */
    sprite_limb_segment(r, x, y, RECEPTIONIST_W, dir, 11.0f, 14.0f + bob,
                        16.0f, 20.0f + shift, suit_hi);
    sprite_limb_segment(r, x, y, RECEPTIONIST_W, dir, 14.0f, 14.0f + bob,
                        17.0f, 20.0f + shift, suit);
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 15.0f, 19.0f + shift,
                5.0f, 3.0f, skin);
  }
  else if (reading)
  {
    /* The folder, held up and read. Whatever the errand is, this is the only
       part of it the player ever sees. */
    float leaf = sinf(rec->anim_time * 2.6f) * 1.0f;
    sprite_limb_segment(r, x, y, RECEPTIONIST_W, dir, 11.0f, 14.0f + bob,
                        16.0f, 18.0f + bob, suit_hi);
    sprite_limb_segment(r, x, y, RECEPTIONIST_W, dir, 14.0f, 14.0f + bob,
                        18.0f, 17.0f + bob, suit);
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 15.0f, 14.0f + bob, 9.0f, 9.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 16.0f, 15.0f + bob, 7.0f, 7.0f,
                (SDL_Color){186, 178, 158, 255});
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 17.0f, 16.0f + leaf + bob,
                5.0f, 1.0f, (SDL_Color){232, 228, 214, 255});
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 17.0f, 19.0f + bob, 4.0f, 1.0f,
                (SDL_Color){232, 228, 214, 255});
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 14.0f, 17.0f + bob, 3.0f, 3.0f,
                skin);
  }
  else
  {
    /* Walking the errand with the folder tucked under the far arm. */
    float swing = walking ? -step : 0.0f;
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 4.0f, 16.0f + bob, 7.0f, 8.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 5.0f, 17.0f + bob, 5.0f, 6.0f,
                (SDL_Color){186, 178, 158, 255});
    sprite_limb_segment(r, x, y, RECEPTIONIST_W, dir, 11.0f, 14.0f + bob,
                        8.0f, 20.0f + bob, suit);
    draw_walking_arm(r, x, y, RECEPTIONIST_W, dir, 14.0f, 13.0f + bob,
                     swing, suit_hi, skin);
  }
}

/*
 * The glare over anything a flash charge has just taken the eyes off.
 *
 * It is one function because the two callers were the same five lines with
 * different numbers, and because a dog is very rarely the thing inside
 * `FLASH_RADIUS` when the charge goes off — a guard is blinded in six of the
 * campaign's interiors, and the nearest a dog has been measured is 141px
 * against a radius of 160. A second copy of this would therefore be a drawing
 * almost nothing reaches.
 *
 * It earns its place at all because a blinded figure is a figure standing
 * still, which is also what one that has merely lost you looks like — and those
 * two mean opposite things about whether it is safe to walk past.
 */
static void figure_flash_dazzle(SDL_Renderer *r, float cx, float cy,
                                float radius, float spread, float timer)
{
  if (timer <= 0.0f)
    return;
  float dazzle = timer / FLASH_BLIND_TIME;
  if (dazzle > 1.0f)
    dazzle = 1.0f;
  fx_glow(r, cx, cy, radius + spread * dazzle, FX_CREAM,
          (Uint8)(70.0f + 90.0f * dazzle));
}

void draw_enemy(SDL_Renderer *r, const Enemy *e, const Level *level,
                       float cam_x, float oy)
{
  float x = e->x - cam_x;
  float y = e->y + oy;
  int dir = e->dir;
  /* The ladder pose faces away from the camera, so left/right mirroring is invalid. */
  if (e->climbing)
    dir = 1;
  bool aiming = e->aim_timer > 0.0f || e->recoil_timer > 0.0f;
  bool using_alarm = e->raising_alarm && e->alarm_use_timer > 0.0f;
  /* Standing still and speaking to nobody: a call in on the crew's own net.
     It has to read differently from a chat, because the chat is two men who
     have stopped watching the corridor and this is one man who has not. */
  bool on_radio = enemy_on_radio(e) && !e->climbing;
  bool moving = fabsf(e->vx) > 2.0f && !aiming && !e->talking;
  float phase = e->anim_time * 3.0f;
  float cycle = phase * (1.0f / 6.28318531f);
  /* The near foot's reach, on the legs' own clock — see `draw_walking_leg`. */
  float step = moving ? cosf(phase) : 0.0f;
  float bob = moving ? fabsf(step) * 0.5f : sinf(e->anim_time * 1.8f) * 0.3f;
  float climb = e->climbing ? sinf(phase) * 4.0f : 0.0f;
  /* Wounded reads off how much of *his own* health is left rather than off a
     fixed three, which is the number that stopped being right the day a man
     with six of them walked into the campaign: read against ENEMY_HP a heavy
     went on looking untouched for the first three rounds and then jumped
     straight to the last colour. */
  int full = enemy_kind_hp(e->kind);
  bool heavy = e->kind == ENEMY_KIND_HEAVY;
  SDL_Color uniform = e->hp >= full ? (heavy ? FX_STEEL_DK : FX_GUARD)
                      : e->hp * 3 > full ? (SDL_Color){103, 83, 54, 255}
                                         : (SDL_Color){101, 65, 49, 255};
  SDL_Color light = e->hp >= full
                        ? (heavy ? FX_STEEL : (SDL_Color){116, 129, 86, 255})
                        : (SDL_Color){135, 98, 58, 255};

  npc_contact_shadow(r, level, e->x + ENEMY_W * 0.5f, e->y + 31.0f,
                     10.0f, 200, cam_x, oy);

  /*
   * Flashed, and the frame has to say so for the whole of it.
   *
   * The player threw the charge for the seconds it buys, so the seconds have to
   * be legible from across a room and while running: a cold wash over the
   * figure and a ring of glare at the head, both fading with the timer. Without
   * it a blinded man is an ordinary guard standing still, which is also what a
   * guard who has simply lost you looks like — and those two mean opposite
   * things about whether it is safe to walk past him.
   */
  figure_flash_dazzle(r, x + ENEMY_W * 0.5f, y + 8.0f, 16.0f, 8.0f,
                      e->blind_timer);

  if (e->climbing)
  {
    /* The rear-facing climb in the same limb vocabulary as the player's: a
       hand and the opposite boot rise while the other pair hold, and every
       limb is a rounded segment ending in a boot rather than a bare bar of
       uniform — the bars read as the ladder, not the man on it. */
    sprite_limb_segment(r, x, y, ENEMY_W, dir,
                        10.5f, 14.0f - climb, 10.5f, 22.0f - climb, uniform);
    sprite_limb_segment(r, x, y, ENEMY_W, dir,
                        15.5f, 14.0f + climb, 15.5f, 22.0f + climb, uniform);
    sprite_limb_segment(r, x, y, ENEMY_W, dir, 10.5f, 23.0f + climb * 0.5f,
                        10.5f, 29.0f + climb, (SDL_Color){30, 35, 28, 255});
    sprite_limb_segment(r, x, y, ENEMY_W, dir, 15.5f, 23.0f - climb * 0.5f,
                        15.5f, 29.0f - climb, (SDL_Color){30, 35, 28, 255});
    sprite_shoe(r, x, y, ENEMY_W, dir, 10.5f, 29.0f + climb,
                (SDL_Color){30, 34, 28, 255});
    sprite_shoe(r, x, y, ENEMY_W, dir, 15.5f, 29.0f - climb,
                (SDL_Color){30, 34, 28, 255});
    bob = fabsf(sinf(phase)) * 0.7f;
  }
  else
  {
    /* Fatigue trousers well under the tunic, for the same reason Chuck's are
       under his jacket: a uniform drawn in one value from the collar to the
       boots is a green column, and the tunic is the shape that has to carry
       the guard across a room. */
    if (moving)
    {
      draw_walking_leg(r, x, y, ENEMY_W, dir, 12.0f, 21.0f + bob,
                       cycle + 0.5f, 3.2f, (SDL_Color){30, 35, 28, 255},
                       (SDL_Color){24, 27, 23, 255});
      draw_walking_leg(r, x, y, ENEMY_W, dir, 14.0f, 21.0f + bob,
                       cycle, 3.2f, (SDL_Color){42, 49, 38, 255},
                       (SDL_Color){32, 36, 30, 255});
    }
    else
    {
      draw_standing_legs(r, x, y, ENEMY_W, dir, 9.0f, 14.0f, 22.0f,
                         (SDL_Color){30, 35, 28, 255},
                         (SDL_Color){40, 46, 36, 255},
                         (SDL_Color){30, 34, 28, 255});
    }
  }

  /* The rifle goes on his back whenever his hands are busy with something
     else — a chat, the handset, a wall switch — and that is a read the player
     can use: a slung rifle is a man who is not watching the corridor. Drawn
     before the body, so all that shows is the stock under his back and the
     muzzle past his shoulder. */
  bool slung = e->talking || on_radio || using_alarm;
  if (slung && !aiming && !e->climbing)
    draw_carbine(r, x, y + bob, ENEMY_W, dir, 3.0f, 24.0f, 7.5f, 3.0f);

  /* Arm behind torso while patrolling / gesturing. A man on a handset does
     not gesture with it, so the chat's arm swing is the chat's alone. */
  float gesture_swing =
      (e->talking && !on_radio) ? sinf(e->anim_time * 5.0f) * 0.65f : 0.0f;
  if (!aiming && !e->climbing)
  {
    float rear_swing = moving ? step : -gesture_swing;
    draw_walking_arm(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                     rear_swing, uniform,
                     (SDL_Color){164, 113, 77, 255});
  }

  sprite_body(r, x, y, ENEMY_W, dir, 7.0f, 11.0f + bob, 13.0f, 12.0f, uniform,
              COL_OUTLINE, 2, 1);
  if (e->climbing)
  {
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 13.0f + bob, 4.0f, 8.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 13.0f + bob, 4.0f, 8.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 12.0f, 13.0f + bob, 3.0f, 10.0f, (SDL_Color){38, 45, 39, 255});
  }
  else
  {
    /* Plate carrier over the uniform: a shoulder cap, the front plate with a
       seam down it, and a pouch on the belt line. What separates a guard from
       a man in a green shirt is the kit, and the kit is three shapes. */
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 12.0f + bob, 10.0f, 2.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 11.0f, 14.0f + bob, 8.0f, 6.0f,
                (SDL_Color){38, 45, 39, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 11.0f, 14.0f + bob, 8.0f, 1.0f,
                (SDL_Color){60, 70, 54, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 15.0f + bob, 1.0f, 5.0f,
                (SDL_Color){26, 31, 27, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 16.0f + bob, 3.0f, 4.0f,
                (SDL_Color){30, 35, 31, 255});
    /* And the heavy's own kit on top of it. A man who cannot be stomped has to
       be recognisable from across the room *before* the player jumps at him,
       so the difference is silhouette rather than tint: a second plate over the
       chest and a pad on each shoulder, both wide enough to break the outline.
       Reading the tint alone would be a rule the player only learns by losing
       a heart to it. */
    if (heavy)
    {
      /* The plate used to be FX_STEEL_DK laid on a uniform that is itself
         FX_STEEL_DK, so all that survived of it was its top edge, and the
         shoulder pads were unoutlined squares inside the torso's own outline
         — which is to say the silhouette the paragraph above asks for was not
         there. Now each piece is its own outlined mass: a plate a step darker
         than the jacket with two rows of webbing across it, and pads that
         stand proud of the shoulder line on both sides, so the outline itself
         is wider than a guard's before a colour has been read. */
      SDL_Color plate = fx_mix(FX_STEEL_DK, FX_INK, 0.40f);
      sprite_body(r, x, y, ENEMY_W, dir, 10.0f, 14.0f + bob, 10.0f, 7.0f,
                  plate, COL_OUTLINE, 1, 1);
      sprite_rect(r, x, y, ENEMY_W, dir, 11.0f, 16.0f + bob, 8.0f, 1.0f,
                  fx_mix(plate, FX_STEEL, 0.55f));
      sprite_rect(r, x, y, ENEMY_W, dir, 11.0f, 18.0f + bob, 8.0f, 1.0f,
                  fx_mix(plate, FX_STEEL, 0.55f));
      sprite_body(r, x, y, ENEMY_W, dir, 5.0f, 11.0f + bob, 5.0f, 4.0f,
                  FX_STEEL, COL_OUTLINE, 1, 1);
      sprite_body(r, x, y, ENEMY_W, dir, 18.0f, 11.0f + bob, 5.0f, 4.0f,
                  FX_STEEL, COL_OUTLINE, 1, 1);
    }
  }
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 20.0f + bob, 11.0f, 2.0f, (SDL_Color){31, 37, 31, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 20.0f + bob, 11.0f, 1.0f,
              (SDL_Color){58, 66, 52, 255});

  if (e->climbing)
  {
    /* On a ladder the rifle rides on his back, across it from the hip to past
       the far shoulder, under the arms that are doing the climbing. */
    draw_carbine(r, x, y + bob, ENEMY_W, dir, 7.0f, 22.0f, 21.5f, 5.0f);
    draw_climbing_arm(r, x, y, ENEMY_W, dir,
                      8.0f, 14.0f + bob, 6.5f, 5.0f - climb,
                      uniform, fx_dim(FX_SKIN, 0.85f));
    draw_climbing_arm(r, x, y, ENEMY_W, dir,
                      18.0f, 14.0f + bob, 19.5f, 5.0f + climb,
                      uniform, fx_dim(FX_SKIN, 0.85f));

    /* Back of the helmet: no side-facing face or visor while on a ladder. */
    sprite_body(r, x, y, ENEMY_W, dir, 9.0f, 2.0f + bob, 10.0f, 9.0f,
                (SDL_Color){47, 57, 43, 255}, COL_OUTLINE, 2, 1);
    sprite_mass(r, x, y, ENEMY_W, dir, 10.0f, 2.0f + bob, 8.0f, 2.0f, light, 1, 0);
    sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 10.0f + bob, 8.0f, 1.0f, (SDL_Color){30, 35, 31, 255});
  }
  else
  {
    /*
     * Helmeted head. The red visor stays where it was — it is the one pixel
     * that says "enemy" across a room — but the helmet is now a helmet: a
     * shell that catches the ceiling, a brim that throws a line of shade over
     * the brow, and a strap down past the ear to the jaw. Without the brim and
     * the strap it is a green rectangle resting on a face.
     */
    /* The face, then the helmet as its own outlined shell over it. A helmet
       drawn as a rectangle sitting on a rectangle is two boxes; domed, with the
       brim overhanging the brow, it is a helmet on a head. */
    sprite_body(r, x, y, ENEMY_W, dir, 10.0f, 4.0f + bob, 8.0f, 7.0f,
                fx_dim(FX_SKIN, 0.85f), COL_OUTLINE, 0, 2);
    sprite_mass(r, x, y, ENEMY_W, dir, 7.0f, 0.0f + bob, 14.0f, 7.0f,
                COL_OUTLINE, 3, 1);
    sprite_mass(r, x, y, ENEMY_W, dir, 8.0f, 1.0f + bob, 12.0f, 5.0f,
                (SDL_Color){47, 57, 43, 255}, 2, 0);
    sprite_mass(r, x, y, ENEMY_W, dir, 9.0f, 1.0f + bob, 10.0f, 2.0f, light, 1, 0);
    /* The shade the brim drops across the brow. */
    sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 6.0f + bob, 8.0f, 1.0f,
                (SDL_Color){138, 98, 68, 255});
    /* Chin strap, down past the ear and along the jaw. */
    sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 6.0f + bob, 1.0f, 3.0f,
                (SDL_Color){36, 42, 34, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 11.0f, 9.0f + bob, 3.0f, 1.0f,
                (SDL_Color){36, 42, 34, 255});
    /* Jaw shading on the face's own taper, then the visor and the set mouth. */
    sprite_mass(r, x, y, ENEMY_W, dir, 10.0f, 9.0f + bob, 8.0f, 2.0f,
                (SDL_Color){150, 106, 73, 255}, 1, 2);
    if (heavy)
    {
      /* The full helmet the docs give him: the shell carried down over the
         ear and the nape, and a guard across the jaw. A guard's head is a
         face under a brim; his is a helmet with a slit in it, which is a
         difference the silhouette carries from across a room. */
      SDL_Color shell = fx_mix(FX_STEEL_DK, FX_GUARD_DK, 0.35f);
      sprite_mass(r, x, y, ENEMY_W, dir, 7.0f, 5.0f + bob, 6.0f, 6.0f,
                  COL_OUTLINE, 0, 2);
      sprite_mass(r, x, y, ENEMY_W, dir, 8.0f, 5.0f + bob, 4.0f, 5.0f,
                  shell, 0, 1);
      sprite_rect(r, x, y, ENEMY_W, dir, 13.0f, 9.0f + bob, 7.0f, 2.0f,
                  COL_OUTLINE);
      sprite_rect(r, x, y, ENEMY_W, dir, 13.0f, 9.0f + bob, 6.0f, 1.0f,
                  fx_mix(shell, FX_STEEL_LT, 0.30f));
      /* The slit runs the width of the face rather than sitting in front
         of the eye, and it is still the red pixel that says "enemy". */
      sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 6.0f + bob, 6.0f, 2.0f,
                  COL_OUTLINE);
      sprite_rect(r, x, y, ENEMY_W, dir, 15.0f, 6.0f + bob, 4.0f, 1.0f,
                  FX_RED);
    }
    else
    {
      sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 6.0f + bob, 3.0f, 2.0f,
                  FX_RED);
      sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 6.0f + bob, 3.0f, 1.0f,
                  (SDL_Color){255, 138, 122, 255});
      sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 9.0f + bob, 2.0f, 1.0f,
                  (SDL_Color){70, 34, 27, 255});
    }
  }

  if (aiming && !e->climbing)
  {
    /*
     * The aim is the telegraph, so it points where the round is going to go.
     * The shot has had a vertical lane for as long as a guard could fire up a
     * ladder at a climber or down at a man dropping in on him — and since the
     * stomp was wired to it, up at a boot on his helmet — but the drawing only
     * ever knew one direction, so a man about to fire straight up his own
     * column was drawn aiming down the corridor at nobody. The rifle comes up
     * along `aim_vdir` now, and the flash is at the end of it.
     */
    float kick = e->recoil_timer > 0.07f ? 2.0f : 0.0f;
    SDL_Color hand = fx_dim(FX_SKIN, 0.85f);
    SDL_Color flash_light = (SDL_Color){255, 128, 74, 255};
    bool flash = e->recoil_timer > PLAYER_MUZZLE_FLASH_TIME;
    if (e->aim_vdir < 0)
    {
      /* Up the column, held in front of the face with the muzzle clear of
         the helmet brim, so the red visor stays in sight beside it. */
      draw_carbine(r, x, y + bob, ENEMY_W, dir, 20.5f, 17.0f + kick,
                   20.5f, -5.0f + kick);
      sprite_limb_segment(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                          17.5f, 16.0f + bob, uniform);
      sprite_limb_segment(r, x, y, ENEMY_W, dir, 17.5f, 16.0f + bob,
                          20.0f, 10.5f + bob + kick, uniform);
      draw_closed_hand(r, x, y, ENEMY_W, dir, 20.0f, 10.0f + bob + kick,
                       hand);
      if (flash)
      {
        draw_muzzle_flash(r, x, y, ENEMY_W, dir, 21.0f, -8.0f + bob,
                          flash_light);
        sprite_rect(r, x, y, ENEMY_W, dir, 18.0f, -10.0f + bob, 6.0f, 4.0f,
                    FX_RED);
        sprite_rect(r, x, y, ENEMY_W, dir, 19.5f, -13.0f + bob, 3.0f, 3.0f,
                    FX_AMBER);
      }
    }
    else if (e->aim_vdir > 0)
    {
      /* Down at the floor he is standing on, the muzzle past the boots. */
      draw_carbine(r, x, y + bob, ENEMY_W, dir, 20.5f, 11.0f - kick,
                   20.5f, 31.0f - kick);
      sprite_limb_segment(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                          16.5f, 17.5f + bob, uniform);
      sprite_limb_segment(r, x, y, ENEMY_W, dir, 16.5f, 17.5f + bob,
                          20.0f, 18.0f + bob - kick, uniform);
      draw_closed_hand(r, x, y, ENEMY_W, dir, 20.0f, 18.0f + bob - kick,
                       hand);
      if (flash)
      {
        draw_muzzle_flash(r, x, y, ENEMY_W, dir, 21.0f, 34.0f + bob,
                          flash_light);
        sprite_rect(r, x, y, ENEMY_W, dir, 18.0f, 32.0f + bob, 6.0f, 4.0f,
                    FX_RED);
        sprite_rect(r, x, y, ENEMY_W, dir, 19.5f, 36.0f + bob, 3.0f, 3.0f,
                    FX_AMBER);
      }
    }
    else
    {
      /* Shouldered, along the corridor: the stock in against the chest, the
         line of the barrel under the chin, the leading hand out on the
         handguard. Brought up from the low carry to here is the whole of the
         warning the player gets, so the two poses are as far apart as a
         twenty-six pixel man allows. */
      draw_carbine(r, x, y + bob, ENEMY_W, dir, 10.0f - kick, 13.5f,
                   31.0f - kick, 13.5f);
      sprite_limb_segment(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                          17.5f, 16.5f + bob, uniform);
      sprite_limb_segment(r, x, y, ENEMY_W, dir, 17.5f, 16.5f + bob,
                          22.0f - kick, 15.0f + bob, uniform);
      draw_closed_hand(r, x, y, ENEMY_W, dir, 22.5f - kick, 15.0f + bob,
                       hand);
      if (flash)
      {
        draw_muzzle_flash(r, x, y, ENEMY_W, dir, 34.0f - kick, 13.5f + bob,
                          flash_light);
        sprite_rect(r, x, y, ENEMY_W, dir, 32.0f - kick, 10.5f + bob,
                    4.0f, 6.0f, FX_RED);
        sprite_rect(r, x, y, ENEMY_W, dir, 36.0f - kick, 12.5f + bob,
                    3.0f, 3.0f, FX_AMBER);
      }
    }
  }
  else if (using_alarm && !e->climbing)
  {
    /* A raised forearm makes the switch interaction readable even when the
     * guard partly overlaps the wall fixture. */
    sprite_limb_segment(r, x, y, ENEMY_W, dir,
                        14.0f, 13.0f + bob, 19.0f, 10.0f + bob, uniform);
    sprite_limb_segment(r, x, y, ENEMY_W, dir,
                        19.0f, 10.0f + bob, 23.0f, 8.0f + bob,
                        fx_dim(FX_SKIN, 0.85f));
    sprite_rect(r, x, y, ENEMY_W, dir,
                21.0f, 6.0f + bob, 5.0f, 5.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir,
                22.0f, 7.0f + bob, 3.0f, 3.0f,
                fx_dim(FX_SKIN, 0.93f));
  }
  else if (on_radio)
  {
    /* The forearm comes up across the chest to the shoulder, which is where a
       shoulder-mounted handset is worked from — an arm raised to the side of
       the head would be a telephone call. The set itself is a stub of dark
       body with a short whip, and the whip is what has to clear the helmet or
       the whole thing disappears into the silhouette. */
    sprite_limb_segment(r, x, y, ENEMY_W, dir,
                        14.0f, 13.0f + bob, 17.0f, 12.0f + bob, uniform);
    sprite_limb_segment(r, x, y, ENEMY_W, dir,
                        17.0f, 12.0f + bob, 19.0f, 10.0f + bob,
                        fx_dim(FX_SKIN, 0.85f));
    /* The set is held just clear of the helmet, not against it: at this size a
       handset drawn over the head is a dark patch on a green shape. */
    sprite_rect(r, x, y, ENEMY_W, dir, 19.0f, 8.0f + bob, 4.0f, 6.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 20.0f, 9.0f + bob, 2.0f, 4.0f,
                (SDL_Color){44, 50, 46, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 21.0f, 2.0f + bob, 1.0f, 7.0f,
                (SDL_Color){30, 35, 31, 255});
    /* One transmit lamp, and it is the technology cyan rather than a second
       red: a red pip on a guard already means the visor. */
    bool keyed = fmodf(e->anim_time * 3.0f, 1.0f) < 0.62f;
    sprite_rect(r, x, y, ENEMY_W, dir, 20.0f, 8.0f + bob, 1.0f, 1.0f,
                keyed ? FX_CYAN : FX_CYAN_DK);
  }
  else if (!e->climbing && slung)
  {
    draw_walking_arm(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                     gesture_swing, uniform, fx_dim(FX_SKIN, 0.85f));
  }
  else if (!e->climbing && e->blind_timer > 0.0f)
  {
    /* Flashed: the forearm thrown up across the eyes and the rifle let go to
       hang on its sling, muzzle at his boots. The glare says a charge went
       off; this says what it did to him, which is the half the player is
       deciding whether to walk past. */
    draw_carbine(r, x, y + bob, ENEMY_W, dir, 12.0f, 14.0f, 17.0f, 29.0f);
    sprite_limb_segment(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                        18.5f, 11.0f + bob, uniform);
    sprite_limb_segment(r, x, y, ENEMY_W, dir, 18.5f, 11.0f + bob,
                        16.5f, 6.5f + bob, uniform);
    draw_closed_hand(r, x, y, ENEMY_W, dir, 17.0f, 6.0f + bob,
                     fx_dim(FX_SKIN, 0.85f));
  }
  else if (!e->climbing)
  {
    /* The low carry: stock under the arm, muzzle at the floor a stride ahead
       of him, the near hand out on the handguard. A man walking a rifle does
       not swing that arm, so only the far one keeps the counter-swing; the
       muzzle nods on the step instead. */
    float nod = moving ? step * 0.6f : 0.0f;
    draw_carbine(r, x, y + bob, ENEMY_W, dir, 9.5f, 14.5f, 27.0f,
                 21.5f + nod);
    sprite_limb_segment(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                        14.5f, 17.5f + bob, uniform);
    sprite_limb_segment(r, x, y, ENEMY_W, dir, 14.5f, 17.5f + bob,
                        18.5f, 18.0f + bob + nod * 0.4f, uniform);
    draw_closed_hand(r, x, y, ENEMY_W, dir, 19.0f, 18.0f + bob + nod * 0.4f,
                     fx_dim(FX_SKIN, 0.85f));
  }

  /* Compact health pips sit in-world without turning into a large UI bar.
     Granted-green and a red-shadow socket, because the semantic colours are
     rationed: green is the palette's "still standing" everywhere else too. */
  /* One pip for each round *he* can take, which is the same correction the
     wounded colours above had to have: counted against ENEMY_HP, a heavy
     showed three full pips and kept showing them through the first three hits,
     so the only readout over his head said the rounds were doing nothing. His
     six are narrower so the row still fits over the figure. */
  float pip_step = full > ENEMY_HP ? 4.0f : 7.0f;
  float pip_w = full > ENEMY_HP ? 4.0f : 6.0f;
  float pip_x = x + ((float)ENEMY_W - (pip_step * (float)(full - 1) + pip_w)) *
                        0.5f;
  for (int hp = 0; hp < full; ++hp)
  {
    SDL_Color hc = hp < e->hp ? FX_GREEN
                              : fx_mix(FX_SHADOW, FX_RED_DK, 0.35f);
    color_rect(r, FX_INK, floorf(pip_x + (float)hp * pip_step), y - 6.0f,
               pip_w, 4.0f);
    color_rect(r, hc, floorf(pip_x + (float)hp * pip_step) + 1.0f, y - 5.0f,
               pip_w - 2.0f, 2.0f);
  }

  if (on_radio)
  {
    /* Not a speech bubble: nobody is being spoken to in the room. Two arcs
       coming off the whip say the words are leaving the building instead. */
    float ax = dir >= 0 ? x + 21.0f : x + 5.0f;
    float ay = fmaxf(oy + 2.0f, y + 1.0f + bob);
    for (int arc = 0; arc < 2; ++arc)
    {
      float phase_out = fmodf(e->anim_time * 1.6f + (float)arc * 0.5f, 1.0f);
      Uint8 alpha = (Uint8)((1.0f - phase_out) * 150.0f);
      float spread = 2.0f + phase_out * 5.0f;
      fx_rect_a(r, FX_CYAN, alpha, ax + (dir >= 0 ? spread : -spread),
                ay - spread * 0.6f, 1.0f, 1.0f + spread * 0.8f);
    }
  }
  else if (e->talking)
  {
    float bubble_y = fmaxf(oy + 2.0f, y - 25.0f);
    color_rect(r, FX_NIGHT, x + 2.0f, bubble_y, 22.0f, 11.0f);
    color_rect(r, fx_dim(FX_CREAM, 0.92f), x + 3.0f, bubble_y + 1.0f, 20.0f, 8.0f);
    color_rect(r, fx_dim(FX_CREAM, 0.92f), x + 12.0f, bubble_y + 9.0f, 4.0f, 3.0f);
    for (int dot = 0; dot < 3; ++dot)
    {
      float bounce = (dot == ((int)(e->anim_time * 3.0f) % 3)) ? -1.0f : 0.0f;
      color_rect(r, FX_SHADOW, x + 7.0f + dot * 5.0f,
                 bubble_y + 4.0f + bounce, 2.0f, 2.0f);
    }
  }
}

/*
 * A guard who is down, lying where he fell.
 *
 * He has to be drawn, and the reason is a rule rather than a flourish: a calm
 * guard who sees a fallen comrade walks over to look and often sprints for the
 * nearest alarm (`update_body_discovery`). With nothing on the floor the player
 * watched a man cross the room to an empty patch of carpet and wake the
 * building, which reads as guards raising the alarm at random — a punishment
 * whose cause was simulated and never shown.
 *
 * So: the same figure, laid along the floor. Head toward the way he was facing,
 * the uniform a step darker than a standing guard's because nothing is lighting
 * him from the front any more, the visor dead rather than red — that pixel is
 * what says "enemy" across a room and a body must not say it — and no health
 * pips, because there is no fight left to report.
 */
void draw_downed_enemy(SDL_Renderer *r, const Enemy *e,
                              const Level *level, float cam_x, float oy)
{
  float x = e->x - cam_x;
  float y = e->y + oy;
  int dir = e->dir;
  bool heavy = e->kind == ENEMY_KIND_HEAVY;
  SDL_Color uniform = fx_dim(heavy ? FX_STEEL_DK : FX_GUARD, 0.82f);
  /* The trousers are lifted off the near-black a standing guard wears. Up on
     his feet the legs recede so the tunic carries him; flat on the floor
     there is nothing to recede *from*, and legs at that value vanished into
     the floor and left a torso and a helmet — which is to say a lump. */
  SDL_Color trouser = (SDL_Color){48, 55, 42, 255};
  SDL_Color boot = (SDL_Color){30, 34, 28, 255};
  SDL_Color skin = fx_dim(FX_SKIN, 0.72f);
  SDL_Color plate = heavy ? fx_mix(FX_STEEL_DK, FX_INK, 0.45f)
                          : (SDL_Color){36, 43, 37, 255};
  SDL_Color helmet = heavy ? fx_mix(FX_STEEL_DK, FX_GUARD_DK, 0.35f)
                           : (SDL_Color){44, 53, 40, 255};

  /* Wider and fainter than the standing pool: the mass is spread along the
     floor rather than balanced on two boots. */
  npc_contact_shadow(r, level, e->x + ENEMY_W * 0.5f, e->y + 31.0f,
                     15.0f, 165, cam_x, oy);

  /*
   * The silhouette is the whole job. Lying down he has a tenth of the height
   * he had standing, so the parts have to be spread along the floor and read
   * separately or the figure collapses into one dark lump the eye files as
   * scenery: on his back, boots up at one end, helmet at the other, one knee
   * drawn up so the legs are two things rather than one, and his rifle gone
   * from his hands — nothing that says "armed" may stay on a body.
   */
  /* The far leg, knee up. */
  sprite_limb_segment(r, x, y, ENEMY_W, dir, 10.0f, 27.0f, 6.0f, 23.5f,
                      fx_mix(trouser, FX_INK, 0.25f));
  sprite_limb_segment(r, x, y, ENEMY_W, dir, 6.0f, 23.5f, 2.5f, 27.5f,
                      fx_mix(trouser, FX_INK, 0.25f));
  /* Boots on end, soles to the room: the toe up is what says "on his back"
     from across a floor. */
  sprite_rect(r, x, y, ENEMY_W, dir, -0.5f, 24.0f, 4.0f, 6.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ENEMY_W, dir, 0.5f, 25.0f, 2.0f, 4.0f, boot);
  sprite_rect(r, x, y, ENEMY_W, dir, 0.5f, 25.0f, 2.0f, 1.0f,
              fx_ramp(boot).lit);

  /* The torso on its back, chest to the ceiling, the plate carrier on top. */
  sprite_body(r, x, y, ENEMY_W, dir, 8.0f, 25.0f, 11.0f, 6.0f, uniform,
              COL_OUTLINE, 1, 1);
  sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 25.0f, 7.0f, 3.0f, plate);
  sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 25.0f, 7.0f, 1.0f,
              fx_mix(plate, heavy ? FX_STEEL_LT : FX_GUARD_LT, 0.45f));
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 29.0f, 11.0f, 1.0f,
              (SDL_Color){26, 31, 27, 255});

  /* The near leg flat along the floor, and its boot up on end. */
  sprite_limb_segment(r, x, y, ENEMY_W, dir, 10.0f, 29.0f, 4.0f, 29.5f,
                      trouser);
  sprite_rect(r, x, y, ENEMY_W, dir, 1.5f, 25.0f, 4.0f, 7.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ENEMY_W, dir, 2.5f, 26.0f, 2.0f, 5.0f, boot);
  sprite_rect(r, x, y, ENEMY_W, dir, 2.5f, 26.0f, 2.0f, 1.0f,
              fx_ramp(boot).lit);

  /* The arm thrown out past the head, under it, so all that shows beyond the
     helmet is a forearm and an open hand on the floor — a feature at the end
     of the silhouette that no lump has. */
  sprite_limb_segment(r, x, y, ENEMY_W, dir, 17.0f, 29.5f, 27.5f, 30.0f,
                      uniform);
  sprite_rect(r, x, y, ENEMY_W, dir, 27.0f, 28.5f, 4.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ENEMY_W, dir, 28.0f, 29.5f, 2.0f, 1.0f, skin);

  /* The head, face to the ceiling, and the helmet still on it but tipped
     back off the brow onto the floor, so the face is clear of it: the one
     patch of skin on the body is what says which end is the head. */
  sprite_body(r, x, y, ENEMY_W, dir, 18.0f, 24.0f, 6.0f, 6.0f, skin,
              COL_OUTLINE, 1, 2);
  sprite_rect(r, x, y, ENEMY_W, dir, 18.0f, 24.0f, 5.0f, 1.0f,
              fx_ramp(skin).lit);
  /* The nose, breaking the top of the profile the way a standing man's
     breaks its front. */
  sprite_rect(r, x, y, ENEMY_W, dir, 19.5f, 22.0f, 3.0f, 2.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ENEMY_W, dir, 20.5f, 23.0f, 1.0f, 1.0f, skin);
  sprite_mass(r, x, y, ENEMY_W, dir, 22.0f, 25.0f, 7.0f, 7.0f, COL_OUTLINE,
              2, 1);
  sprite_mass(r, x, y, ENEMY_W, dir, 23.0f, 26.0f, 5.0f, 5.0f, helmet, 1, 0);
  sprite_rect(r, x, y, ENEMY_W, dir, 24.0f, 26.0f, 3.0f, 1.0f,
              fx_ramp(helmet).lit);
  /* The visor is dead. Lit, it is the pixel that says "enemy" across a room,
     and a body must not say it. */
  sprite_rect(r, x, y, ENEMY_W, dir, 23.0f, 27.0f, 1.0f, 2.0f,
              fx_dim(FX_RED_DK, 0.55f));
  /* The closed eye and the slack mouth either side of the nose. */
  sprite_rect(r, x, y, ENEMY_W, dir, 21.0f, 25.0f, 2.0f, 1.0f,
              fx_mix(skin, FX_INK, 0.55f));
  sprite_rect(r, x, y, ENEMY_W, dir, 18.0f, 25.0f, 1.0f, 1.0f,
              fx_mix(skin, FX_INK, 0.45f));
}

/*
 * One leg of a dog, standing on the floor line at the bottom of its box.
 *
 * Two pixels of leg inside the outline, a paw that turns forward, and the
 * front pixel of the leg lit — the same cylinder the cast's limbs are, at the
 * width a dog's leg is. A hind leg carries the hock: the thigh runs forward of
 * the paw and the shank steps back under it, which is the one kink in the
 * outline that says hind leg rather than fore, and that a dog standing on four
 * straight posts does not have.
 */
static void draw_dog_leg(SDL_Renderer *r, float x, float y, int dir,
                         float lx, float top, float lift, bool hind,
                         SDL_Color fill)
{
  float foot = 15.0f - lift;
  float h = foot - top;
  SDL_Color lit = fx_ramp(fill).lit;

  if (h < 2.0f)
    h = 2.0f;
  if (hind)
  {
    float knee = top + floorf(h * 0.45f);
    sprite_rect(r, x, y, DOG_W, dir, lx, top, 4.0f, knee - top + 1.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, DOG_W, dir, lx - 1.0f, knee, 4.0f, foot - knee + 1.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, DOG_W, dir, lx + 1.0f, top, 2.0f, knee - top, fill);
    sprite_rect(r, x, y, DOG_W, dir, lx, knee, 2.0f, foot - knee, fill);
  }
  else
  {
    sprite_rect(r, x, y, DOG_W, dir, lx - 1.0f, top, 4.0f, h + 1.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, DOG_W, dir, lx, top, 2.0f, h, fill);
    sprite_rect(r, x, y, DOG_W, dir, lx + 1.0f, top + 1.0f, 1.0f, h - 2.0f,
                lit);
  }
  /* The paw, a pixel longer than the leg and pointing the way he faces. */
  float paw_x = hind ? lx - 1.0f : lx;
  sprite_rect(r, x, y, DOG_W, dir, paw_x - 1.0f, foot - 1.0f, 5.0f, 2.0f,
              COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, paw_x, foot - 1.0f, 3.0f, 1.0f, lit);
}

/*
 * A working dog, and it has to read as one at twenty-four pixels.
 *
 * It was a brown box on two posts: a rectangle of body, a rectangle of head, a
 * stub of tail held up like a handle, and one leg for each end — with the
 * collar painted at a fixed screen offset, so a dog facing left wore it half
 * way down its back. What says "dog" at this size is the line of the back and
 * the belly: a deep chest, a waist tucked up under the loin, a haunch, and a
 * tail that hangs off the end of it. So the body is three masses under one
 * outline (see `sprite_mass_outline`), the head is a skull with a muzzle
 * narrowing out of it and a pricked ear, and there are four legs rather than
 * two — the far pair a value under the near, which is what puts the body
 * between them. The coat is the black-and-tan every handler's dog in a
 * building like this is: a dark saddle over the back and the tan on the legs,
 * the chest and the face, so the pattern alone separates the animal from the
 * brown of a floor or a crate.
 */
void draw_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
                     float cam_x, float oy)
{
  float x = dog->x - cam_x;
  float y = dog->y + oy;
  int dir = dog->dir;
  bool moving = fabsf(dog->vx) > 4.0f;
  bool chase = dog->state == DOG_CHASE;
  bool biting = dog->attack_timer > 0.0f;
  float phase = dog->anim_time * (chase ? 3.5f : 2.7f);
  float gait = moving ? sinf(phase) * 3.0f : 0.0f;
  float bob = moving ? fabsf(sinf(phase)) : sinf(dog->anim_time * 1.7f) * 0.35f;
  float lunge = biting ? 3.0f : 0.0f;
  /* Tan warms and brightens on the chase, which is the colour cue the
     animal has always given; the saddle stays black. */
  SDL_Color tan = chase ? (SDL_Color){143, 82, 44, 255}
                        : (SDL_Color){109, 76, 51, 255};
  SDL_Color coat = chase ? (SDL_Color){91, 59, 39, 255}
                         : (SDL_Color){70, 54, 42, 255};
  SDL_Color saddle = fx_mix(coat, FX_INK, 0.35f);
  SDL_Color far_leg = fx_mix(tan, FX_INK, 0.38f);
  SDL_Color mask = fx_mix(coat, FX_INK, 0.65f);
  float bx = lunge;
  float by = bob;

  npc_contact_shadow(r, level, dog->x + DOG_W * 0.5f, dog->y + 15.0f,
                     11.0f, 185, cam_x, oy);

  /*
   * The same glare the flashed guard gets, at the animal's head and scaled to
   * it, and it earns its place by the same argument: a blinded dog is a dog
   * standing still, which is also what a dog that has lost you looks like, and
   * those two mean opposite things about whether it is safe to walk past. Drawn
   * under the body rather than over it so the figure stays readable inside its
   * own halo.
   */
  figure_flash_dazzle(r, x + DOG_W * 0.68f, y + 6.0f, 11.0f, 6.0f,
                      dog->blind_timer);

  /* Each leg on the same stance-and-swing cycle the rest of the cast walks,
     in a trot: the near fore moves with the far hind, the far fore with the
     near hind. Through stance the paw holds its ground and tracks back under
     the body, through swing it lifts and reaches. A parked dog stands square
     on all four instead of freezing a pair mid-stride. */
  float run = phase * 0.5f;
  float leg_x[4] = {15.0f, 5.0f, 16.0f, 6.0f}; /* near fore, near hind, far */
  float leg_lift[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int leg = 0; leg < 4; ++leg)
  {
    if (!moving)
    {
      if (leg >= 2)
        leg_x[leg] -= 2.0f;
      continue;
    }
    float cycle = run + ((leg == 0 || leg == 3) ? 0.0f : 0.5f);
    cycle -= floorf(cycle);
    float reach = chase ? 3.0f : 2.5f;
    if (cycle < 0.5f)
      leg_x[leg] += reach * (1.0f - 4.0f * cycle);
    else
    {
      float t = (cycle - 0.5f) * 2.0f;
      float ease = t * t * (3.0f - 2.0f * t);
      leg_x[leg] += reach * (-1.0f + 2.0f * ease);
      leg_lift[leg] = sinf(t * 3.14159265f) * 1.5f;
    }
  }
  draw_dog_leg(r, x, y, dir, leg_x[2] + bx, 10.0f + by, leg_lift[2], false,
               far_leg);
  draw_dog_leg(r, x, y, dir, leg_x[3] + bx, 9.0f + by, leg_lift[3], true,
               far_leg);

  /* The tail comes off the top of the rump and hangs in a curve, carried out
     straight behind on the chase and swinging while he trots. */
  float wag = moving ? gait * 0.35f : sinf(dog->anim_time * 4.0f) * 0.8f;
  float tail_mid_x = chase ? -1.0f : 0.0f;
  float tail_mid_y = chase ? 4.5f : 7.0f;
  float tail_tip_x = chase ? -4.0f : -2.0f + wag * 0.5f;
  float tail_tip_y = chase ? 4.0f + wag * 0.4f : 11.0f;
  sprite_segment(r, x, y, DOG_W, dir, 3.0f + bx, 5.0f + by,
                 tail_mid_x + bx, tail_mid_y + by, 4, COL_OUTLINE);
  sprite_segment(r, x, y, DOG_W, dir, tail_mid_x + bx, tail_mid_y + by,
                 tail_tip_x + bx, tail_tip_y + by, 3, COL_OUTLINE);
  sprite_segment(r, x, y, DOG_W, dir, 3.0f + bx, 5.0f + by,
                 tail_mid_x + bx, tail_mid_y + by, 2, saddle);
  sprite_segment(r, x, y, DOG_W, dir, tail_mid_x + bx, tail_mid_y + by,
                 tail_tip_x + bx, tail_tip_y + by, 1, coat);

  /* The hide, outlined as one piece: haunch, loin tucked up under the back,
     a chest a row deeper than either, the neck rising out of it, then the
     skull, the muzzle and the ear. */
  float ear_top = chase ? -3.0f : -2.0f;
  sprite_mass_outline(r, x, y, DOG_W, dir, 2.0f + bx, 4.0f + by, 7.0f, 7.0f,
                      2, 2);
  sprite_mass_outline(r, x, y, DOG_W, dir, 7.0f + bx, 4.0f + by, 8.0f, 5.0f,
                      0, 1);
  sprite_mass_outline(r, x, y, DOG_W, dir, 13.0f + bx, 4.0f + by, 6.0f, 8.0f,
                      1, 2);
  sprite_mass_outline(r, x, y, DOG_W, dir, 16.0f + bx, 1.0f + by, 4.0f, 6.0f,
                      0, 0);
  sprite_mass_outline(r, x, y, DOG_W, dir, 17.0f + bx, 0.0f + by, 5.0f, 5.0f,
                      2, 1);
  sprite_mass_outline(r, x, y, DOG_W, dir, 21.0f + bx, 2.0f + by, 4.0f, 3.0f,
                      0, 1);
  sprite_mass_outline(r, x, y, DOG_W, dir, 17.0f + bx, ear_top + by, 3.0f,
                      4.0f - ear_top - 1.0f, 1, 0);

  sprite_mass_form(r, x, y, DOG_W, dir, 2.0f + bx, 4.0f + by, 7.0f, 7.0f,
                   tan, 2, 2);
  sprite_mass_form(r, x, y, DOG_W, dir, 7.0f + bx, 4.0f + by, 8.0f, 5.0f,
                   tan, 0, 1);
  sprite_mass_form(r, x, y, DOG_W, dir, 13.0f + bx, 4.0f + by, 6.0f, 8.0f,
                   tan, 1, 2);
  sprite_mass_form(r, x, y, DOG_W, dir, 16.0f + bx, 1.0f + by, 4.0f, 6.0f,
                   tan, 0, 0);
  sprite_mass_form(r, x, y, DOG_W, dir, 17.0f + bx, 0.0f + by, 5.0f, 5.0f,
                   tan, 2, 1);
  sprite_mass_form(r, x, y, DOG_W, dir, 21.0f + bx, 2.0f + by, 4.0f, 3.0f,
                   tan, 0, 1);
  sprite_mass(r, x, y, DOG_W, dir, 17.0f + bx, ear_top + by, 3.0f,
              4.0f - ear_top - 1.0f, tan, 1, 0);
  sprite_rect(r, x, y, DOG_W, dir, 18.0f + bx, ear_top + 1.0f + by, 1.0f,
              1.0f - ear_top, mask);

  /* The saddle: black across the back from the haunch to the withers, lit
     along its crown like everything else under the ceiling. */
  sprite_mass(r, x, y, DOG_W, dir, 3.0f + bx, 4.0f + by, 13.0f, 3.0f, saddle,
              1, 0);
  sprite_mass(r, x, y, DOG_W, dir, 5.0f + bx, 7.0f + by, 9.0f, 1.0f, saddle,
              0, 0);
  sprite_rect(r, x, y, DOG_W, dir, 4.0f + bx, 4.0f + by, 11.0f, 1.0f,
              fx_ramp(saddle).lit);
  /* The black mask down the muzzle and the nose at the end of it. */
  sprite_rect(r, x, y, DOG_W, dir, 21.0f + bx, 3.0f + by, 4.0f, 2.0f, mask);
  sprite_rect(r, x, y, DOG_W, dir, 24.0f + bx, 2.0f + by, 1.0f, 2.0f,
              FX_INK);
  /* Collar on the neck, in the colour that has always said which state the
     animal is in — and on the neck whichever way he faces. */
  sprite_rect(r, x, y, DOG_W, dir, 16.0f + bx, 3.0f + by, 2.0f, 4.0f,
              chase ? FX_RED : FX_AMBER);

  /* The alert eye blinks like every other eye in the cast — a dog whose eye
     never closes is a glass one — but never mid-charge. */
  if (chase || !fx_blinking(dog->anim_time, 0x0d06u))
    sprite_rect(r, x, y, DOG_W, dir, 20.0f + bx, 1.0f + by, 2.0f, 1.0f,
                chase ? FX_RED : fx_mix(FX_AMBER, FX_CREAM, 0.45f));
  else
    sprite_rect(r, x, y, DOG_W, dir, 20.0f + bx, 1.0f + by, 2.0f, 1.0f, mask);

  if (biting)
  {
    /* The jaw drops open under the muzzle: dark mouth, a row of teeth. */
    sprite_rect(r, x, y, DOG_W, dir, 20.0f + bx, 5.0f + by, 6.0f, 3.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, DOG_W, dir, 21.0f + bx, 5.0f + by, 4.0f, 2.0f,
                fx_dim(FX_RED_DK, 0.70f));
    sprite_rect(r, x, y, DOG_W, dir, 22.0f + bx, 5.0f + by, 3.0f, 1.0f,
                FX_CREAM);
  }
  else if (chase)
  {
    /* Mouth line, pulled back: the animal is working. */
    sprite_rect(r, x, y, DOG_W, dir, 21.0f + bx, 5.0f + by, 3.0f, 1.0f,
                fx_mix(mask, FX_INK, 0.5f));
  }

  /* The near pair last, in tan, in front of the body. */
  draw_dog_leg(r, x, y, dir, leg_x[0] + bx, 10.0f + by, leg_lift[0], false,
               tan);
  draw_dog_leg(r, x, y, dir, leg_x[1] + bx, 9.0f + by, leg_lift[1], true,
               tan);
}

/* One leg of a dog lying down, stiff from the joint to the paw, with the paw
   lit on top. The outline and the fill are separate calls so every leg can be
   inked before any fill goes down: where a leg crosses the hide, the hide (or
   the near leg's own fill) covers the ink, and what is left is a leg lying
   against a flank rather than a dark stroke cut across it. */
static void lying_dog_leg_outline(SDL_Renderer *r, float x, float y, int dir,
                                  float jx, float jy, float px, float py)
{
  sprite_segment(r, x, y, DOG_W, dir, jx, jy, px, py, 4, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, px - 1.0f, py - 1.5f, 4.0f, 3.0f,
              COL_OUTLINE);
}

static void lying_dog_leg_fill(SDL_Renderer *r, float x, float y, int dir,
                               float jx, float jy, float px, float py,
                               SDL_Color fill)
{
  sprite_segment(r, x, y, DOG_W, dir, jx, jy, px, py, 2, fill);
  sprite_rect(r, x, y, DOG_W, dir, px, py - 0.5f, 2.0f, 1.0f,
              fx_ramp(fill).lit);
}

/*
 * The dog, down. Same reason as the guard: a handler who finds it investigates
 * and may raise the alarm, and the animal has to be on the floor for that to
 * be a thing the player saw happen.
 *
 * It was a brown bar with two one-pixel stubs on top, which read as a log, and
 * then a dog on its back with four legs in the air, which read at 1x as a pile
 * of roots — four thin uprights is a silhouette the eye files as debris long
 * before it finds the head. So it lies on its side, the way an animal that has
 * gone down actually lies: one low black-and-tan hide along the floor with the
 * saddle on the spine, the neck dropping to a head that rests flat with the
 * ear fallen back and the eye shut, the tail limp behind, and the legs
 * stretched stiff and forward along the floor from hip and chest — the far
 * pair a value darker and partly behind the body, the near pair a step lighter
 * and in front, the near foreleg lying across under the jaw. The head sits
 * lower than the back and keeps its stop and its muzzle, because a hide with
 * no head is a sack; the legs reach out past the outline, because legs tucked
 * under it are a tray. Every piece is horizontal, which is the whole of "not
 * standing" at this size.
 */
void draw_downed_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
                            float cam_x, float oy)
{
  float x = dog->x - cam_x;
  float y = dog->y + oy;
  int dir = dog->dir;
  /* The live dog's coat a step down, for the same reason the downed guard's
     uniform is: nothing is holding it up into the light any more. Only a
     step, though — dimmed further the tan sank into every brown floor in the
     building and the pattern that says "dog" went with it. */
  SDL_Color tan = fx_dim((SDL_Color){109, 76, 51, 255}, 0.94f);
  SDL_Color saddle = fx_mix((SDL_Color){70, 54, 42, 255}, FX_INK, 0.45f);
  SDL_Color far_leg = fx_mix(tan, FX_INK, 0.40f);
  SDL_Color near_leg = fx_mix(tan, fx_ramp(tan).lit, 0.60f);
  SDL_Color mask = fx_mix(tan, FX_INK, 0.62f);

  npc_contact_shadow(r, level, dog->x + DOG_W * 0.5f, dog->y + 15.0f,
                     14.0f, 150, cam_x, oy);

  /* The tail, limp: off the rump and down onto the floor behind. */
  sprite_segment(r, x, y, DOG_W, dir, 3.0f, 9.0f, 0.0f, 12.5f, 4,
                 COL_OUTLINE);
  sprite_segment(r, x, y, DOG_W, dir, 0.0f, 12.5f, -4.0f, 14.5f, 3,
                 COL_OUTLINE);
  sprite_segment(r, x, y, DOG_W, dir, 3.0f, 9.0f, 0.0f, 12.5f, 2, saddle);
  sprite_segment(r, x, y, DOG_W, dir, 0.0f, 12.5f, -4.0f, 14.5f, 1, saddle);

  /* Every outline first — the hide, the neck, the skull, the muzzle and the
     four legs — so that the only ink left once the fills are down is the
     silhouette. */
  sprite_mass_outline(r, x, y, DOG_W, dir, 2.0f, 7.0f, 14.0f, 6.0f, 2, 1);
  sprite_mass_outline(r, x, y, DOG_W, dir, 14.0f, 8.0f, 4.0f, 4.0f, 1, 0);
  sprite_mass_outline(r, x, y, DOG_W, dir, 16.0f, 9.0f, 5.0f, 5.0f, 2, 1);
  sprite_mass_outline(r, x, y, DOG_W, dir, 20.0f, 11.0f, 5.0f, 3.0f, 0, 1);
  lying_dog_leg_outline(r, x, y, dir, 8.0f, 11.5f, 14.5f, 13.0f);
  lying_dog_leg_outline(r, x, y, dir, 16.0f, 11.5f, 26.5f, 13.0f);
  lying_dog_leg_outline(r, x, y, dir, 5.0f, 12.0f, 12.0f, 14.5f);
  lying_dog_leg_outline(r, x, y, dir, 13.0f, 12.5f, 24.5f, 14.5f);

  /* The far pair, a value down; the hide and the head cover their roots. */
  lying_dog_leg_fill(r, x, y, dir, 8.0f, 11.5f, 14.5f, 13.0f, far_leg);
  lying_dog_leg_fill(r, x, y, dir, 16.0f, 11.5f, 26.5f, 13.0f, far_leg);

  sprite_mass_form(r, x, y, DOG_W, dir, 2.0f, 7.0f, 14.0f, 6.0f, tan, 2, 1);
  sprite_mass_form(r, x, y, DOG_W, dir, 14.0f, 8.0f, 4.0f, 4.0f, tan, 1, 0);
  sprite_mass_form(r, x, y, DOG_W, dir, 16.0f, 9.0f, 5.0f, 5.0f, tan, 2, 1);
  sprite_mass_form(r, x, y, DOG_W, dir, 20.0f, 11.0f, 5.0f, 3.0f, tan, 0, 1);

  /* The saddle along the spine, from the rump to the withers, with the
     ceiling's one lit row on it as on the live dog. */
  sprite_mass(r, x, y, DOG_W, dir, 3.0f, 7.0f, 13.0f, 3.0f, saddle, 2, 0);
  sprite_rect(r, x, y, DOG_W, dir, 5.0f, 7.0f, 9.0f, 1.0f,
              fx_ramp(saddle).lit);
  /* The ear, fallen back from the skull onto the neck. */
  sprite_rect(r, x, y, DOG_W, dir, 15.0f, 9.0f, 4.0f, 1.0f, saddle);
  sprite_rect(r, x, y, DOG_W, dir, 15.0f, 10.0f, 2.0f, 1.0f, saddle);
  /* The black mask down the muzzle and the nose at the end of it. */
  sprite_rect(r, x, y, DOG_W, dir, 21.0f, 11.0f, 3.0f, 2.0f, mask);
  sprite_rect(r, x, y, DOG_W, dir, 24.0f, 11.0f, 1.0f, 2.0f, FX_INK);
  /* The eye is shut. An open one on a body is the animal watching the room. */
  sprite_rect(r, x, y, DOG_W, dir, 18.0f, 11.0f, 2.0f, 1.0f, FX_INK);

  /* The near pair last, in front of everything and a step lighter. */
  lying_dog_leg_fill(r, x, y, dir, 5.0f, 12.0f, 12.0f, 14.5f, near_leg);
  lying_dog_leg_fill(r, x, y, dir, 13.0f, 12.5f, 24.5f, 14.5f, near_leg);
}

void draw_thrown_object(SDL_Renderer *r, const ThrownObject *object,
                               float cam_x, float oy)
{
  float x = object->x - cam_x;
  float y = object->y + oy;
  float wobble_x = cosf(object->angle) * 2.0f;
  float wobble_y = sinf(object->angle) * 2.0f;
  int dir = object->vx >= 0.0f ? 1 : -1;

  /* Small as they are, these tumble through the lit air of the facade, so
     each one is a form — lit crown, shaded underside — rather than a flat
     swatch inside an outline. The materials anchor on the palette: terracotta
     out of the rust, glass out of the cyan, masonry out of the wood ramp. */
  if (object->variant == 0)
  {
    /* Flower pot. */
    SDL_Color clay = fx_mix(FX_RUST, FX_WOOD, 0.35f);
    color_rect(r, COL_OUTLINE, x + 2.0f + wobble_x, y + 2.0f, 10.0f, 11.0f);
    fx_form_block(r, x + 3.0f + wobble_x, y + 3.0f, 8.0f, 9.0f,
                  fx_ramp(clay), dir);
    fx_form_block(r, x + 5.0f, y + wobble_y, 4.0f, 5.0f,
                  fx_ramp(FX_GREEN_DK), dir);
  }
  else if (object->variant == 1)
  {
    /* Bottle. */
    SDL_Color glass = fx_mix(FX_CYAN_DK, FX_STEEL, 0.45f);
    color_rect(r, COL_OUTLINE, x + 4.0f + wobble_x, y + 1.0f, 7.0f, 13.0f);
    fx_form_block(r, x + 5.0f + wobble_x, y + 2.0f, 5.0f, 11.0f,
                  fx_ramp(glass), dir);
    color_rect(r, fx_ramp(glass).lit,
               x + 7.0f + wobble_x, y + 3.0f, 1.0f, 7.0f);
  }
  else
  {
    /* Brick-sized chunk of facade. */
    SDL_Color masonry = fx_mix(FX_WOOD_DK, FX_STEEL, 0.30f);
    color_rect(r, COL_OUTLINE, x + 1.0f, y + 2.0f + wobble_y, 13.0f, 10.0f);
    fx_form_block(r, x + 2.0f, y + 3.0f + wobble_y, 11.0f, 8.0f,
                  fx_ramp(masonry), dir);
  }
}

/* One wing as a blade from the shoulder to the tip: thick at the root, thin at
   the end, and lit along its leading edge. */
static void draw_bird_wing(SDL_Renderer *r, float x, float y, int dir,
                           float sx, float sy, float tx, float ty,
                           SDL_Color fill)
{
  float mx = (sx + tx) * 0.5f;
  float my = (sy + ty) * 0.5f;
  sprite_segment(r, x, y, BIRD_W, dir, sx, sy, mx, my, 5, COL_OUTLINE);
  sprite_segment(r, x, y, BIRD_W, dir, mx, my, tx, ty, 4, COL_OUTLINE);
  sprite_segment(r, x, y, BIRD_W, dir, sx, sy, mx, my, 3, fill);
  sprite_segment(r, x, y, BIRD_W, dir, mx, my, tx, ty, 2, fill);
  sprite_segment_shifted(r, x, y, BIRD_W, dir, sx, sy, tx, ty, 1, 1.0f,
                         fx_ramp(fill).lit);
}

void draw_bird(SDL_Renderer *r, const Bird *bird,
                      float cam_x, float oy)
{
  float x = bird->x - cam_x;
  float y = bird->y + oy;
  float flap = sinf(bird->anim_time * 15.0f);
  int dir = bird->vx >= 0.0f ? 1 : -1;
  bool up = flap > 0.0f;
  /* City-pigeon slate out of the room's own darks, not a fourth grey: a body,
     a darker hood, and wings a step lighter than either. */
  SDL_Color body = fx_mix(FX_INK, FX_STEEL_DK, 0.60f);
  SDL_Color hood = fx_mix(FX_INK, FX_STEEL_DK, 0.35f);
  SDL_Color wing = fx_mix(FX_STEEL_DK, FX_STEEL, 0.55f);
  FxRamp body_ramp = fx_ramp(body);

  /*
   * Both wings beat together. The old pose drew one wing above the body and
   * the other below it, which is no bird's stroke — it read as a cross, or as
   * a box with a tab on each side. On the upstroke the pair rise off the back,
   * on the downstroke they sweep down and back under the body, and each is a
   * blade rather than a block. The far wing is a value down and a pixel
   * behind, which is what puts the body between them.
   */
  if (up)
    draw_bird_wing(r, x, y, dir, 12.0f, 6.0f, 7.0f, -2.0f,
                   fx_ramp(wing).dark);
  else
    draw_bird_wing(r, x, y, dir, 12.0f, 8.0f, 6.0f, 13.0f,
                   fx_ramp(wing).dark);

  /* Tail fan at the back, then the body and the head as masses with the
     light on their crowns. */
  sprite_mass(r, x, y, BIRD_W, dir, 1.0f, 5.0f, 8.0f, 5.0f, COL_OUTLINE, 1, 1);
  sprite_mass(r, x, y, BIRD_W, dir, 2.0f, 6.0f, 6.0f, 3.0f, body_ramp.dark,
              1, 1);
  sprite_mass(r, x, y, BIRD_W, dir, 6.0f, 4.0f, 15.0f, 8.0f, COL_OUTLINE, 1, 1);
  sprite_mass_form(r, x, y, BIRD_W, dir, 7.0f, 5.0f, 13.0f, 6.0f, body, 1, 1);
  sprite_mass(r, x, y, BIRD_W, dir, 18.0f, 2.0f, 8.0f, 8.0f, COL_OUTLINE, 1, 1);
  sprite_mass_form(r, x, y, BIRD_W, dir, 19.0f, 3.0f, 6.0f, 6.0f, hood, 1, 1);
  /* The sheen on the neck, one pixel of the city's teal. */
  sprite_rect(r, x, y, BIRD_W, dir, 19.0f, 7.0f, 2.0f, 1.0f,
              fx_mix(hood, FX_CYAN_DK, 0.55f));
  sprite_rect(r, x, y, BIRD_W, dir, 24.0f, 5.0f, 3.0f, 1.0f,
              fx_mix(FX_AMBER_DK, FX_AMBER, 0.30f));
  sprite_rect(r, x, y, BIRD_W, dir, 22.0f, 4.0f, 1.0f, 1.0f, FX_INK);

  /* The near wing over the body. */
  if (up)
    draw_bird_wing(r, x, y, dir, 14.0f, 6.0f, 9.0f, -3.0f, wing);
  else
    draw_bird_wing(r, x, y, dir, 14.0f, 7.0f, 8.0f, 14.0f, wing);
}
