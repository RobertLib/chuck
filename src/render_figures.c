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

#include "fx.h"
#include "game_config.h"
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

void draw_grenade(SDL_Renderer *r, float x, float y, float fuse)
{
  color_rect(r, COL_OUTLINE, x - 1.0f, y + 1.0f, 12.0f, 10.0f);
  color_rect(r, (SDL_Color){68, 92, 61, 255}, x, y + 2.0f, 10.0f, 8.0f);
  color_rect(r, (SDL_Color){101, 124, 78, 255}, x + 2.0f, y + 2.0f, 3.0f, 7.0f);
  color_rect(r, (SDL_Color){169, 144, 85, 255}, x + 3.0f, y, 5.0f, 3.0f);
  color_rect(r, FX_INK, x + 7.0f, y - 1.0f, 4.0f, 2.0f);
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
 * palette: a steel cylinder with a white band and a cyan tell-tale rather than
 * an olive body with a brass spoon, and the tell-tale is what strobes as the
 * fuse runs down. Same size, opposite colours.
 */
void draw_flashbang(SDL_Renderer *r, float x, float y, float fuse)
{
  color_rect(r, COL_OUTLINE, x - 1.0f, y + 1.0f, 12.0f, 10.0f);
  color_rect(r, FX_STEEL, x, y + 2.0f, 10.0f, 8.0f);
  color_rect(r, FX_CREAM, x, y + 4.0f, 10.0f, 2.0f);
  color_rect(r, FX_STEEL_LT, x + 2.0f, y + 2.0f, 2.0f, 7.0f);
  color_rect(r, FX_INK, x + 7.0f, y - 1.0f, 4.0f, 2.0f);
  if (fuse > 0.0f && ((int)(fuse * 18.0f) & 1) == 0)
  {
    color_rect(r, FX_CREAM, x + 10.0f, y - 2.0f, 2.0f, 2.0f);
    color_rect(r, FX_CYAN, x + 11.0f, y - 1.0f, 2.0f, 2.0f);
  }
}

/*
 * A sheet off the docket, lying where it fell out of a case.
 *
 * Paper rather than kit, so it is drawn as paper: a pale leaf with a corner
 * turned, two ruled lines and the red stamp that makes it Meridian's rather
 * than the building's. It is the one pickup that is not a weapon, a heart or a
 * door, and it has to read that way from across a room or a player will walk
 * past it assuming they are full up on whatever it is.
 */
void draw_evidence_pickup(SDL_Renderer *r, float x, float y)
{
  color_rect(r, COL_OUTLINE, x + 2.0f, y + 1.0f, 12.0f, 15.0f);
  color_rect(r, FX_CREAM, x + 3.0f, y + 2.0f, 10.0f, 13.0f);
  color_rect(r, FX_PALE, x + 3.0f, y + 12.0f, 10.0f, 3.0f);
  /* The turned corner, which is what stops it reading as a plain white box. */
  color_rect(r, COL_OUTLINE, x + 10.0f, y + 2.0f, 4.0f, 4.0f);
  color_rect(r, FX_PALE, x + 10.0f, y + 2.0f, 3.0f, 3.0f);
  /* Ruled lines and the contractor's stamp. */
  color_rect(r, FX_LABEL, x + 5.0f, y + 6.0f, 6.0f, 1.0f);
  color_rect(r, FX_LABEL, x + 5.0f, y + 8.0f, 6.0f, 1.0f);
  color_rect(r, FX_RED_DK, x + 5.0f, y + 10.0f, 4.0f, 2.0f);
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

static void draw_player_crawling(SDL_Renderer *r, const Player *p, float x, float y)
{
  int dir = p->facing;
  float phase = p->anim_time * 3.2f;
  float shove = (fabsf(p->vx) > 1.0f) ? sinf(phase) * 2.0f : 0.0f;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  bool firing = p->action_timer > 0.0f && !knife;
  bool bazooka = (p->active_weapon == PLAYER_WEAPON_BAZOOKA &&
                   p->bazooka_rockets > 0) ||
                 (firing && p->bazooka_firing);

  /* Rear boot; the ground shadow is laid by the caller, anchored to the
     floor rather than to the belly. */
  sprite_rect(r, x, y, PLAYER_W, dir, 1.0f - shove, 12.0f, 8.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 2.0f - shove, 12.0f, 7.0f, 3.0f, fx_dim(FX_HERO_DK, 0.80f));

  /* Horizontal torso, shoulder plate and head at the leading edge. */
  sprite_body(r, x, y, PLAYER_W, dir, 7.0f, 7.0f, 12.0f, 8.0f,
              FX_HERO, COL_OUTLINE, 1, 1);
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 7.0f, 10.0f, 2.0f, FX_HERO_LT);
  sprite_body(r, x, y, PLAYER_W, dir, 18.0f, 4.0f, 6.0f, 7.0f, FX_SKIN,
              COL_OUTLINE, 1, 2);
  sprite_mass(r, x, y, PLAYER_W, dir, 18.0f, 4.0f, 6.0f, 3.0f,
              FX_HAIR, 1, 0);
  sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 6.0f, 2.0f, 2.0f, (SDL_Color){220, 239, 219, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 23.0f, 6.0f, 1.0f, 2.0f, (SDL_Color){40, 54, 64, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 18.0f, 9.0f, 5.0f, 1.0f, FX_SKIN_DK);
  sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 5.0f, 4.0f, 2.0f, FX_RED);

  /* Braced front arm with either the sidearm or an empty-ammo knife thrust. */
  sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 11.0f, 7.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, 11.0f, 6.0f, 2.0f, FX_SKIN);
  if (knife)
  {
    float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f ? 2.0f : 0.0f;
    sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 10.0f, 4.0f + thrust, 4.0f,
                FX_SKIN);
    sprite_rect(r, x, y, PLAYER_W, dir, 25.0f + thrust, 9.0f, 3.0f, 5.0f,
                (SDL_Color){55, 43, 31, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 28.0f + thrust, 10.0f, 6.0f, 2.0f,
                (SDL_Color){205, 221, 225, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 34.0f + thrust, 10.5f, 1.0f, 1.0f,
                (SDL_Color){241, 247, 239, 255});
  }
  else if (bazooka)
  {
    draw_bazooka_weapon(r, x, y, PLAYER_W, dir,
                        15.0f, 5.0f, p->bazooka_firing);
  }
  else if ((p->active_weapon == PLAYER_WEAPON_PISTOL && p->bullets > 0) ||
           firing)
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 10.0f,
                firing ? 7.0f : 5.0f, 3.0f,
                (SDL_Color){36, 43, 48, 255});
    if (firing && p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
    {
      /* Prone or standing, a shot lights the floor it is fired across. */
      draw_muzzle_flash(r, x, y, PLAYER_W, dir, 31.0f, 11.5f, FX_AMBER);
      sprite_rect(r, x, y, PLAYER_W, dir, 29.0f, 9.0f, 3.0f, 5.0f, FX_AMBER);
      sprite_rect(r, x, y, PLAYER_W, dir, 32.0f, 10.0f, 2.0f, 3.0f, FX_FLAME_HOT);
    }
  }
}

static void draw_player_hacking(SDL_Renderer *r, float x, float y,
                                float hack_time)
{
  const SDL_Color shirt = FX_HERO;
  const SDL_Color shirt_light = FX_HERO_LT;
  const SDL_Color trousers = FX_HERO_DK;
  const SDL_Color boots = FX_SHADOW;
  const SDL_Color skin = FX_SKIN;
  float type_phase = hack_time * 15.0f;
  float tap_a = sinf(type_phase) * 1.2f;
  float tap_b = sinf(type_phase + 3.14159265f) * 1.2f;
  float bob = sinf(hack_time * 5.0f) * 0.25f;

  /* Rear view: Chuck faces the wall-mounted terminal, so the camera sees
     the back of his head, shoulders and torso. */
  sprite_rect(r, x, y, PLAYER_W, 1,
              7.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
  sprite_form(r, x, y, PLAYER_W, 1,
              8.0f, 23.0f, 4.0f, 7.0f, trousers);
  sprite_rect(r, x, y, PLAYER_W, 1,
              6.0f, 29.0f, 8.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1,
              7.0f, 30.0f, 7.0f, 2.0f, boots);
  sprite_rect(r, x, y, PLAYER_W, 1,
              13.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
  sprite_form(r, x, y, PLAYER_W, 1,
              14.0f, 23.0f, 4.0f, 7.0f, fx_mix(FX_HERO_DK, FX_HERO, 0.30f));
  sprite_rect(r, x, y, PLAYER_W, 1,
              13.0f, 29.0f, 8.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1,
              14.0f, 30.0f, 7.0f, 2.0f, boots);

  /* Seen from behind, elbows flare outward and both forearms reach forward
     again to the terminal's lower keypad. */
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      8.0f, 14.0f + bob, 3.5f, 17.0f + bob,
                      shirt);
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      3.5f, 17.0f + bob, 8.5f, 21.0f + tap_a,
                      skin);
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      18.0f, 14.0f + bob, 22.5f, 17.0f + bob,
                      shirt_light);
  sprite_limb_segment(r, x, y, PLAYER_W, 1,
                      22.5f, 17.0f + bob, 17.5f, 21.0f + tap_b,
                      skin);

  /* Broad, symmetrical back with shoulder panels and central webbing. */
  sprite_body(r, x, y, PLAYER_W, 1,
              8.0f, 11.0f + bob, 13.0f, 12.0f, shirt, COL_OUTLINE, 2, 1);
  sprite_rect(r, x, y, PLAYER_W, 1,
              8.0f, 12.0f + bob, 4.0f, 8.0f, shirt_light);
  sprite_rect(r, x, y, PLAYER_W, 1,
              17.0f, 12.0f + bob, 4.0f, 8.0f, shirt_light);
  sprite_rect(r, x, y, PLAYER_W, 1,
              13.0f, 12.0f + bob, 3.0f, 11.0f,
              (SDL_Color){21, 54, 76, 255});
  sprite_rect(r, x, y, PLAYER_W, 1,
              9.0f, 20.0f + bob, 11.0f, 2.0f, FX_AMBER);

  /* Back of the head: no face or eye is visible from this angle. */
  sprite_body(r, x, y, PLAYER_W, 1,
              10.0f, 2.0f + bob, 8.0f, 9.0f, FX_HAIR, COL_OUTLINE, 2, 2);
  sprite_rect(r, x, y, PLAYER_W, 1,
              12.0f, 2.0f + bob, 4.0f, 1.0f, PLAYER_HAIR_LT);
  sprite_rect(r, x, y, PLAYER_W, 1,
              8.0f, 4.0f + bob, 12.0f, 2.0f, FX_RED);
  sprite_rect(r, x, y, PLAYER_W, 1,
              9.0f, 7.0f + bob, 2.0f, 2.0f,
              (SDL_Color){91, 48, 31, 255});
  sprite_rect(r, x, y, PLAYER_W, 1,
              17.0f, 7.0f + bob, 2.0f, 2.0f,
              (SDL_Color){91, 48, 31, 255});

  /* His hands are between his body and the terminal, so they are hidden
     from this rear angle; only the alternating elbow motion is visible. */
}

void draw_player(SDL_Renderer *r, const Player *p, const Level *level,
                        float cam_x, float oy, bool hacking, float hacking_time,
                        float land_squash)
{
  float x = p->x - cam_x;
  float y = p->y + oy;
  int dir = p->facing;

  if (hacking)
  {
    /* Both special poses get the same floor-anchored pool as the standing
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

  float phase = p->anim_time * 3.0f;
  bool moving = fabsf(p->vx) > 2.0f;
  bool climbing = p->on_ladder || p->facade_climbing;
  /* A back-facing ladder pose must not inherit or mirror the last walk direction. */
  if (climbing)
    dir = 1;
  bool airborne = !p->on_ground && !climbing;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  bool grenade = p->action_timer > 0.0f && p->grenade_throwing;
  bool firing = p->action_timer > 0.0f && !knife;
  bool bazooka = (p->active_weapon == PLAYER_WEAPON_BAZOOKA &&
                   p->bazooka_rockets > 0) ||
                 (firing && p->bazooka_firing);
  bool walking = moving && p->on_ground && !climbing;
  /* Across the rungs rather than up them. Vertical travel wins when both are
     held: that is the part of the move the player is watching, and a pose
     trying to say both at once says neither. */
  bool shuffling = climbing && moving && fabsf(p->vy) <= 1.0f;
  float shuffle_side = shuffling ? (p->vx > 0.0f ? 1.0f : -1.0f) : 0.0f;
  float cycle = phase * (1.0f / 6.28318531f);
  float step = walking ? sinf(phase) : 0.0f;
  float bob = walking ? fabsf(step) * 0.55f
                      : sinf(p->anim_time * 2.0f) * 0.35f;
  /*
   * Squash and stretch. Two or three pixels is all a thirty-two pixel figure
   * can take before it turns into rubber, and it is the difference between a
   * jump with weight and one that teleports: the body draws out while it is in
   * the air and compresses into the frames just after the boots land.
   */
  float air_stretch = airborne ? fminf(1.0f, fabsf(p->vy) / 340.0f) : 0.0f;
  float crouch = land_squash * 2.8f - air_stretch * 1.3f;
  /* Walking is a body carried forward by its legs, so the torso leads them. */
  float lean = walking ? 1.0f : 0.0f;
  float arm_swing = -step;
  float climb = 0.0f;
  /* How far each side has travelled across the ladder this beat, signed in
     screen space. Shared with the arms below, which grip on the same beat as
     the boot on their own side steps. */
  float left_step = 0.0f;
  float right_step = 0.0f;
  float left_grip = 6.5f;
  float right_grip = 19.5f;

  bob += crouch;

  float shadow_y;
  float shadow_lift;
  if (level != NULL &&
      character_ground(level, p->x + PLAYER_W * 0.5f, p->y + PLAYER_H,
                       &shadow_y, &shadow_lift))
  {
    fx_contact_shadow(r, x + PLAYER_W * 0.5f, shadow_y + oy - 1.0f,
                      11.0f, shadow_lift, 205);
  }

  /* The weight goes across as well as the limbs: the body hangs back off the
     hand that is reaching and rides forward over the pair that gather. A pixel
     and a half of it is the difference between someone shifting across a ladder
     and two limbs waving on a figure travelling on rails. The shadow is already
     down, so it stays with the floor. */
  if (shuffling)
    x -= shuffle_side * sinf(phase) * 0.7f;

  if (climbing)
  {
    float beat = sinf(phase);
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
    left_step = shuffle_side > 0.0f ? gather : reach;
    right_step = shuffle_side > 0.0f ? reach : gather;
    climb = shuffling ? 0.0f : beat * 4.0f;
    /* A boot that slides along a rung is a boot with no weight on it, so the
       one that is moving clears it first. */
    float left_lift = fabsf(left_step) * 0.5f;
    float right_lift = fabsf(right_step) * 0.5f;
    /* Each hand travels with the boot below it, but stays on its own side of
       the shoulders: a grip that crossed the body would swap the elbow flare
       mid-beat and pop. Reaching outward is free, gathering inward is damped. */
    left_grip += left_step > 0.0f ? left_step * 0.4f : left_step;
    right_grip += right_step < 0.0f ? right_step * 0.4f : right_step;

    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + left_step, 13.0f - climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 13.0f + right_step, 13.0f + climb, 5.0f, 10.0f, COL_OUTLINE);
    /* Seen from behind the legs are still trousers, and still darker than the
       jacket above them — drawn at the torso's own value they turned the whole
       climb into one blue column. */
    sprite_form(r, x, y, PLAYER_W, dir, 9.0f + left_step, 14.0f - climb, 3.0f, 8.0f, (SDL_Color){30, 58, 84, 255});
    sprite_form(r, x, y, PLAYER_W, dir, 14.0f + right_step, 14.0f + climb, 3.0f, 8.0f, (SDL_Color){30, 58, 84, 255});
    /* Boots seen from behind, one per leg, so the climb has feet on the rungs
       rather than two blank shanks ending in the dark. */
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + left_step, 23.0f + climb - left_lift, 5.0f, 8.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 13.0f + right_step, 23.0f - climb - right_lift, 5.0f, 8.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 9.0f + left_step, 24.0f + climb - left_lift, 3.0f, 5.0f,
                (SDL_Color){44, 51, 63, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 14.0f + right_step, 24.0f - climb - right_lift, 3.0f, 5.0f,
                (SDL_Color){44, 51, 63, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 9.0f + left_step, 24.0f + climb - left_lift, 3.0f, 1.0f,
                (SDL_Color){63, 72, 86, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 14.0f + right_step, 24.0f - climb - right_lift, 3.0f, 1.0f,
                (SDL_Color){63, 72, 86, 255});
    bob = fabsf(beat) * 0.7f;
  }
  else
  {
    /*
     * Trousers are a long way below the jacket in value, and that gap is the
     * figure's whole read at this size. A figure whose legs sit a few steps
     * under its torso in the same hue is one blue smear with a belt drawn
     * across it; drop the legs into the dark and the jacket becomes the mass
     * the eye lands on — which is exactly how Chuck is built in the cutscenes
     * and in the rear-facing terminal pose.
     */
    const SDL_Color trouser_rear = {21, 40, 59, 255};
    const SDL_Color trouser_front = {29, 55, 80, 255};
    /* Boots, not holes. A near-black shoe under a near-black outline fuses the
       two feet and the contact shadow into one slab, and a figure standing on a
       slab has no feet at all. They stay a step under the trousers so the ankle
       still breaks; the lit toe cap is what carries them back out of the dark. */
    const SDL_Color boot_rear = {26, 31, 40, 255};
    const SDL_Color boot_front = {34, 39, 49, 255};

    if (walking)
    {
      draw_walking_leg(r, x, y, PLAYER_W, dir, 12.0f, 21.0f + bob,
                       cycle + 0.5f, 3.4f, trouser_rear, boot_rear);
      draw_walking_leg(r, x, y, PLAYER_W, dir, 14.0f, 21.0f + bob,
                       cycle, 3.4f, trouser_front, boot_front);
    }
    else if (airborne)
    {
      /* Two short rects of equal length read as a figure standing on nothing.
         In the air the trailing leg tucks under and the leading one reaches,
         and the reach opens out as he starts to come down for the landing. */
      float tuck = p->vy < 0.0f ? 2.0f : 0.0f;
      float reach = p->vy > 40.0f ? 1.5f : 0.0f;

      sprite_limb_segment(r, x, y, PLAYER_W, dir, 12.0f, 21.0f + bob,
                          9.0f, 25.0f - tuck, trouser_rear);
      sprite_limb_segment(r, x, y, PLAYER_W, dir, 9.0f, 25.0f - tuck,
                          12.5f, 28.0f - tuck * 1.5f, trouser_rear);
      sprite_shoe(r, x, y, PLAYER_W, dir, 12.5f, 28.0f - tuck * 1.5f,
                  boot_rear);
      sprite_limb_segment(r, x, y, PLAYER_W, dir, 14.0f, 21.0f + bob,
                          17.0f, 25.5f, trouser_front);
      sprite_limb_segment(r, x, y, PLAYER_W, dir, 17.0f, 25.5f,
                          17.5f + reach, 30.0f - tuck * 0.5f, trouser_front);
      sprite_shoe(r, x, y, PLAYER_W, dir, 17.5f + reach,
                  30.0f - tuck * 0.5f, boot_front);
    }
    else
    {
      /* Standing. The legs carry the squash: their tops travel down with the
         body while the soles stay on the floor. */
      draw_standing_legs(r, x, y, PLAYER_W, dir, 9.0f, 14.0f,
                         22.0f + fmaxf(0.0f, crouch),
                         trouser_rear, trouser_front, boot_front);
    }
  }

  /* Rear arm passes behind the torso and counter-swings against the legs. */
  if (!climbing && !firing && !knife)
  {
    draw_walking_arm(r, x, y, PLAYER_W, dir, 14.0f, 13.0f + bob,
                     -arm_swing, FX_HERO,
                     (SDL_Color){189, 132, 91, 255});
  }

  /* Torso, webbing and shoulder. The jacket is shaded as a solid first; only
     then does the tailoring go on it. Two lines are the whole of it — the
     lapel down the leading edge and the hem where the jacket ends — because a
     thirteen-pixel chest that already carries a crown, a shoulder plate, the
     webbing and a belt has no room left for a third. */
  sprite_body(r, x, y, PLAYER_W, dir, 7.0f + lean, 11.0f + bob, 13.0f, 12.0f,
              FX_HERO, COL_OUTLINE, 2, 1);
  if (climbing)
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 13.0f + bob, 4.0f, 8.0f, (SDL_Color){48, 125, 157, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 13.0f + bob, 4.0f, 8.0f, (SDL_Color){48, 125, 157, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 13.0f + bob, 3.0f, 10.0f, (SDL_Color){21, 54, 76, 255});
  }
  else
  {
    /* A shoulder is the top of a torso, not a stripe down the length of it. */
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + lean, 13.0f + bob, 10.0f, 2.0f,
                FX_HERO_LT);
    /* The webbing runs across the chest from the near shoulder to the far hip.
       Two pixels of it standing vertically is a stripe; on the diagonal it is
       a strap, and it is the one line that says this jacket is rigged for a
       job rather than worn to one. */
    sprite_segment(r, x, y, PLAYER_W, dir, 18.0f + lean, 13.0f + bob,
                   9.0f + lean, 21.0f + bob, 3, (SDL_Color){21, 54, 76, 255});
    sprite_segment_shifted(r, x, y, PLAYER_W, dir, 18.0f + lean, 13.0f + bob,
                           9.0f + lean, 21.0f + bob, 1, 1.0f,
                           (SDL_Color){46, 96, 126, 255});
    /* Lapel notch, so the jacket has a front to it. */
    sprite_rect(r, x, y, PLAYER_W, dir, 17.0f + lean, 14.0f + bob, 2.0f, 4.0f,
                (SDL_Color){30, 76, 106, 255});
  }
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + lean, 20.0f + bob, 11.0f, 2.0f,
              FX_AMBER);
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + lean, 20.0f + bob, 11.0f, 1.0f,
              (SDL_Color){255, 214, 128, 255});
  /* The hem of the jacket, one dark line so the two garments part company. It
     follows the taper of the last row rather than running the full width. */
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + lean, 22.0f + bob, 11.0f, 1.0f,
              (SDL_Color){18, 46, 66, 255});

  if (climbing)
  {
    if (knife)
    {
      /* One hand stays on the ladder while the other follows the selected
         attack direction. */
      if (p->shot_vertical != 0)
      {
        float hand_y = p->shot_vertical < 0 ? 6.0f : 22.0f;
        float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f
                           ? 2.0f
                           : 0.0f;
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          8.0f, 14.0f + bob, left_grip, 5.0f - climb,
                          (SDL_Color){42, 118, 153, 255},
                          (SDL_Color){209, 154, 105, 255});
        sprite_limb_segment(r, x, y, PLAYER_W, dir,
                            18.0f, 14.0f + bob, 21.0f, hand_y,
                            (SDL_Color){42, 118, 153, 255});
        sprite_rect(r, x, y, PLAYER_W, dir,
                    19.0f, hand_y - 2.0f, 5.0f, 5.0f, COL_OUTLINE);
        sprite_rect(r, x, y, PLAYER_W, dir,
                    20.0f, hand_y - 1.0f, 3.0f, 3.0f,
                    (SDL_Color){209, 154, 105, 255});
        float handle_y = p->shot_vertical < 0
                             ? hand_y - 6.0f - thrust
                             : hand_y + 2.0f + thrust;
        float blade_y = p->shot_vertical < 0
                            ? handle_y - 8.0f
                            : handle_y + 5.0f;
        sprite_rect(r, x, y, PLAYER_W, dir,
                    19.0f, handle_y, 5.0f, 6.0f,
                    (SDL_Color){55, 43, 31, 255});
        sprite_rect(r, x, y, PLAYER_W, dir,
                    20.0f, blade_y, 3.0f, 8.0f,
                    (SDL_Color){205, 221, 225, 255});
      }
      else
      {
        /* The rear-facing ladder pose is fixed, so only the attacking arm is
           mirrored for a sideways stab. */
        if (p->facing > 0)
        {
          draw_climbing_arm(r, x, y, PLAYER_W, dir,
                            8.0f, 14.0f + bob, left_grip, 5.0f - climb,
                            (SDL_Color){42, 118, 153, 255},
                            (SDL_Color){209, 154, 105, 255});
        }
        else
        {
          draw_climbing_arm(r, x, y, PLAYER_W, dir,
                            18.0f, 14.0f + bob, right_grip, 5.0f + climb,
                            (SDL_Color){42, 118, 153, 255},
                            (SDL_Color){209, 154, 105, 255});
        }

        int knife_dir = p->facing;
        float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f
                           ? 2.0f
                           : 0.0f;
        sprite_limb_segment(r, x, y, PLAYER_W, knife_dir,
                            17.0f, 14.0f + bob,
                            21.0f + thrust, 15.0f + bob,
                            (SDL_Color){42, 118, 153, 255});
        sprite_rect(r, x, y, PLAYER_W, knife_dir,
                    20.0f + thrust, 13.0f + bob,
                    6.0f, 5.0f, COL_OUTLINE);
        sprite_rect(r, x, y, PLAYER_W, knife_dir,
                    21.0f + thrust, 14.0f + bob, 5.0f, 3.0f,
                    (SDL_Color){209, 154, 105, 255});
        sprite_rect(r, x, y, PLAYER_W, knife_dir,
                    25.0f + thrust, 13.0f + bob, 3.0f, 5.0f,
                    (SDL_Color){55, 43, 31, 255});
        sprite_rect(r, x, y, PLAYER_W, knife_dir,
                    28.0f + thrust, 14.0f + bob, 6.0f, 2.0f,
                    (SDL_Color){205, 221, 225, 255});
        sprite_rect(r, x, y, PLAYER_W, knife_dir,
                    34.0f + thrust, 14.5f + bob, 1.0f, 1.0f,
                    (SDL_Color){241, 247, 239, 255});
      }
    }
    else if (grenade)
    {
      draw_climbing_arm(r, x, y, PLAYER_W, dir,
                        8.0f, 14.0f + bob, left_grip, 5.0f - climb,
                        (SDL_Color){42, 118, 153, 255},
                        (SDL_Color){209, 154, 105, 255});
      if (p->shot_vertical != 0)
      {
        float hand_y = p->shot_vertical < 0 ? 5.0f : 22.0f;
        sprite_limb_segment(r, x, y, PLAYER_W, dir,
                            18.0f, 14.0f + bob, 21.0f, hand_y,
                            (SDL_Color){42, 118, 153, 255});
        draw_grenade(r, x + 17.0f,
                     p->shot_vertical < 0 ? y - 5.0f : y + 23.0f, 0.0f);
      }
      else
      {
        int throw_dir = p->facing;
        sprite_limb_segment(r, x, y, PLAYER_W, throw_dir,
                            17.0f, 14.0f + bob, 23.0f, 10.0f + bob,
                            (SDL_Color){42, 118, 153, 255});
        draw_grenade(r,
                     throw_dir > 0 ? x + PLAYER_W + 2.0f
                                   : x - GRENADE_W - 2.0f,
                     y + 5.0f + bob, 0.0f);
      }
    }
    else if (firing && p->shot_vertical == 0)
    {
      /* Horizontal ladder fire uses the stored facing direction while the
         body remains turned toward the ladder. */
      if (p->facing > 0)
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          8.0f, 14.0f + bob, left_grip, 5.0f - climb,
                          (SDL_Color){42, 118, 153, 255},
                          (SDL_Color){209, 154, 105, 255});
      }
      else
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          18.0f, 14.0f + bob, right_grip, 5.0f + climb,
                          (SDL_Color){42, 118, 153, 255},
                          (SDL_Color){209, 154, 105, 255});
      }

      int gun_dir = p->facing;
      float recoil = p->action_timer > 0.075f ? -1.0f : 0.0f;
      sprite_limb_segment(r, x, y, PLAYER_W, gun_dir,
                          17.0f, 14.0f + bob,
                          22.0f + recoil, 15.0f + bob,
                          (SDL_Color){42, 118, 153, 255});
      if (bazooka)
      {
        draw_bazooka_weapon(r, x, y, PLAYER_W, gun_dir,
                            14.0f, 8.0f + bob, true);
      }
      else
      {
        sprite_rect(r, x, y, PLAYER_W, gun_dir,
                    21.0f + recoil, 13.0f + bob, 7.0f, 5.0f, COL_OUTLINE);
        sprite_rect(r, x, y, PLAYER_W, gun_dir,
                    22.0f + recoil, 14.0f + bob, 6.0f, 3.0f,
                    (SDL_Color){209, 154, 105, 255});
        sprite_rect(r, x, y, PLAYER_W, gun_dir,
                    26.0f + recoil, 12.0f + bob, 8.0f, 4.0f,
                    (SDL_Color){31, 38, 43, 255});
        sprite_rect(r, x, y, PLAYER_W, gun_dir,
                    28.0f + recoil, 16.0f + bob, 3.0f, 5.0f,
                    (SDL_Color){44, 49, 49, 255});
        if (p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
        {
          draw_muzzle_flash(r, x, y, PLAYER_W, gun_dir,
                            36.0f + recoil, 14.0f + bob, FX_AMBER);
          sprite_rect(r, x, y, PLAYER_W, gun_dir,
                      34.0f + recoil, 11.0f + bob, 4.0f, 6.0f, FX_AMBER);
          sprite_rect(r, x, y, PLAYER_W, gun_dir,
                      38.0f + recoil, 13.0f + bob, 3.0f, 3.0f,
                      (SDL_Color){255, 242, 184, 255});
        }
      }
    }
    else
    {
      /* Keep one hand on the ladder while the other operates the sidearm. */
      draw_climbing_arm(r, x, y, PLAYER_W, dir,
                        8.0f, 14.0f + bob, left_grip, 5.0f - climb,
                        (SDL_Color){42, 118, 153, 255},
                        (SDL_Color){209, 154, 105, 255});
      if (firing && p->shot_vertical != 0)
      {
        float hand_y = p->shot_vertical < 0 ? 8.0f : 20.0f;
        sprite_limb_segment(r, x, y, PLAYER_W, dir,
                            18.0f, 14.0f + bob, 21.0f, hand_y,
                            (SDL_Color){42, 118, 153, 255});
        sprite_rect(r, x, y, PLAYER_W, dir,
                    19.0f, hand_y - 2.0f, 5.0f, 5.0f, COL_OUTLINE);
        sprite_rect(r, x, y, PLAYER_W, dir,
                    20.0f, hand_y - 1.0f, 3.0f, 3.0f,
                    (SDL_Color){209, 154, 105, 255});
        if (bazooka)
        {
          draw_vertical_bazooka_weapon(r, x, y, p->shot_vertical, true);
        }
        else
        {
          float gun_y = p->shot_vertical < 0 ? 1.0f : 18.0f;
          sprite_rect(r, x, y, PLAYER_W, dir,
                      19.0f, gun_y, 5.0f, 8.0f,
                      (SDL_Color){31, 38, 43, 255});
          if (p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
          {
            float flash_y = p->shot_vertical < 0 ? -5.0f : 26.0f;
            draw_muzzle_flash(r, x, y, PLAYER_W, dir,
                              21.0f, flash_y + 2.0f, FX_AMBER);
            sprite_rect(r, x, y, PLAYER_W, dir,
                        18.0f, flash_y, 7.0f, 5.0f, FX_AMBER);
            sprite_rect(r, x, y, PLAYER_W, dir,
                        20.0f,
                        p->shot_vertical < 0
                            ? flash_y - 3.0f
                            : flash_y + 5.0f,
                        3.0f, 3.0f, (SDL_Color){255, 242, 184, 255});
          }
        }
      }
      else
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          18.0f, 14.0f + bob, right_grip, 5.0f + climb,
                          (SDL_Color){42, 118, 153, 255},
                          (SDL_Color){209, 154, 105, 255});
      }
    }
  }

  /* Face the ladder while climbing; otherwise keep the normal side profile. */
  if (climbing)
  {
    /* Seen from behind: the nape of the neck below the hair, and no face. */
    sprite_body(r, x, y, PLAYER_W, dir, 10.0f, 2.0f + bob, 8.0f, 9.0f,
                FX_SKIN, COL_OUTLINE, 2, 2);
    sprite_mass(r, x, y, PLAYER_W, dir, 10.0f, 2.0f + bob, 8.0f, 7.0f,
                FX_HAIR, 2, 0);
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 2.0f + bob, 4.0f, 1.0f,
                PLAYER_HAIR_LT);
    sprite_rect(r, x, y, PLAYER_W, dir, 11.0f, 9.0f + bob, 6.0f, 1.0f,
                FX_SKIN_DK);
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 4.0f + bob, 12.0f, 2.0f, FX_RED);
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 4.0f + bob, 12.0f, 1.0f,
                (SDL_Color){246, 104, 88, 255});
  }
  else
  {
    /*
     * The head, in profile. The headband is the identity and stays exactly
     * where it always was; the rest is the small amount of modelling a face
     * needs to stop being a swatch with an eye on it — a lit cheek, the shadow
     * the fringe throws over the brow, a nose that breaks the leading edge, a
     * jaw that steps back into shade, and an eye that closes now and then.
     */
    /* The face first, coming to a chin, and the hair over the top of it. The
       hair is its own outlined dome rather than a rectangle laid across the
       skull — a square block of hair puts the corners of the head straight back
       however round the head under it is — and it goes on second so its fill
       covers the face's own top outline row instead of being cut by it. */
    sprite_body(r, x, y, PLAYER_W, dir, 10.0f + lean, 4.0f + bob, 8.0f, 7.0f,
                FX_SKIN, COL_OUTLINE, 0, 2);
    sprite_mass(r, x, y, PLAYER_W, dir, 9.0f + lean, 0.0f + bob, 10.0f, 5.0f,
                COL_OUTLINE, 3, 0);
    sprite_mass(r, x, y, PLAYER_W, dir, 10.0f + lean, 1.0f + bob, 8.0f, 4.0f,
                FX_HAIR, 2, 0);
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f + lean, 1.0f + bob, 4.0f, 1.0f,
                PLAYER_HAIR_LT);
    /* The back of the skull stays hair the whole way down to the nape. */
    sprite_rect(r, x, y, PLAYER_W, dir, 10.0f + lean, 6.0f + bob, 2.0f, 3.0f,
                FX_HAIR);
    /* The brow the fringe shades, and the jaw stepping back under the cheek —
       drawn with the face's own taper so the shading cannot square it off. */
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f + lean, 6.0f + bob, 6.0f, 1.0f,
                (SDL_Color){181, 127, 87, 255});
    sprite_mass(r, x, y, PLAYER_W, dir, 10.0f + lean, 9.0f + bob, 8.0f, 2.0f,
                FX_SKIN_DK, 1, 2);
    /* The nose. A profile without one is a rectangle with an eye in it, and to
       be a profile it has to break the head's outline rather than sit inside
       it — so the skin steps one pixel out and the outline moves out in front
       of it, leaving a bridge above and the shadow of the tip below. */
    sprite_rect(r, x, y, PLAYER_W, dir, 18.0f + lean, 6.0f + bob, 2.0f, 4.0f,
                COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 17.0f + lean, 7.0f + bob, 2.0f, 2.0f,
                FX_SKIN);
    if (fx_blinking(p->anim_time, 0x1u))
    {
      sprite_rect(r, x, y, PLAYER_W, dir, 14.0f + lean, 8.0f + bob, 3.0f, 1.0f,
                  (SDL_Color){110, 58, 40, 255});
    }
    else
    {
      /* Mostly pupil, with the white behind it: the dark is what the eye is,
         and the pupil sits at the front of it rather than in the middle, since
         a profile with the dark centred reads as two eyes seen head-on. The
         white is kept under the value of the lit cheek beside it — three pixels
         of near-cream on an eight pixel face is the brightest thing on the
         figure, and the eye ends up reading as the whole head. */
      sprite_rect(r, x, y, PLAYER_W, dir, 14.0f + lean, 7.0f + bob, 3.0f, 2.0f,
                  (SDL_Color){166, 176, 164, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 15.0f + lean, 7.0f + bob, 2.0f, 2.0f,
                  (SDL_Color){38, 50, 60, 255});
    }
    /* A closed mouth, one pixel deep and inside the jaw rather than on the
       outline below it. Any more of it reads as a grimace. */
    sprite_rect(r, x, y, PLAYER_W, dir, 13.0f + lean, 10.0f + bob, 3.0f, 1.0f,
                (SDL_Color){126, 66, 50, 255});
    /* The headband crosses both the hairline and the brow, so it goes on last
       over the two of them. */
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + lean, 4.0f + bob, 12.0f, 2.0f,
                FX_RED);
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f + lean, 4.0f + bob, 12.0f, 1.0f,
                (SDL_Color){246, 104, 88, 255});
    /* The loose tail of it, trailing behind the run. */
    sprite_rect(r, x, y, PLAYER_W, dir, 5.0f + lean - fabsf(step) * 0.8f,
                5.0f + bob, 4.0f, 2.0f, (SDL_Color){166, 38, 42, 255});
  }

  if (!climbing)
  {
    if (knife)
    {
      float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f
                         ? 2.0f
                         : 0.0f;
      sprite_limb_segment(r, x, y, PLAYER_W, dir,
                          17.0f, 14.0f + bob,
                          21.0f + thrust, 15.0f + bob,
                          (SDL_Color){42, 118, 153, 255});
      sprite_rect(r, x, y, PLAYER_W, dir,
                  20.0f + thrust, 13.0f + bob, 6.0f, 5.0f, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir,
                  21.0f + thrust, 14.0f + bob, 5.0f, 3.0f,
                  (SDL_Color){209, 154, 105, 255});
      sprite_rect(r, x, y, PLAYER_W, dir,
                  25.0f + thrust, 13.0f + bob, 3.0f, 5.0f,
                  (SDL_Color){55, 43, 31, 255});
      sprite_rect(r, x, y, PLAYER_W, dir,
                  28.0f + thrust, 14.0f + bob, 6.0f, 2.0f,
                  (SDL_Color){205, 221, 225, 255});
      sprite_rect(r, x, y, PLAYER_W, dir,
                  34.0f + thrust, 14.5f + bob, 1.0f, 1.0f,
                  (SDL_Color){241, 247, 239, 255});
    }
    else if (bazooka)
    {
      sprite_limb_segment(r, x, y, PLAYER_W, dir,
                          17.0f, 14.0f + bob,
                          22.0f, 15.0f + bob,
                          (SDL_Color){42, 118, 153, 255});
      draw_bazooka_weapon(r, x, y, PLAYER_W, dir,
                          13.0f, 8.0f + bob, p->bazooka_firing);
    }
    else if (firing)
    {
      float recoil = p->action_timer > 0.075f ? -1.0f : 0.0f;
      sprite_rect(r, x, y, PLAYER_W, dir, 17.0f + recoil, 13.0f + bob, 8.0f, 5.0f, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir, 18.0f + recoil, 14.0f + bob, 7.0f, 3.0f, (SDL_Color){209, 154, 105, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 23.0f + recoil, 12.0f + bob, 8.0f, 4.0f, (SDL_Color){31, 38, 43, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 25.0f + recoil, 16.0f + bob, 3.0f, 5.0f, (SDL_Color){44, 49, 49, 255});
      if (p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
      {
        draw_muzzle_flash(r, x, y, PLAYER_W, dir, 33.0f + recoil, 14.0f + bob,
                          FX_AMBER);
        sprite_rect(r, x, y, PLAYER_W, dir, 31.0f + recoil, 11.0f + bob, 4.0f, 6.0f, FX_AMBER);
        sprite_rect(r, x, y, PLAYER_W, dir, 35.0f + recoil, 13.0f + bob, 3.0f, 3.0f, (SDL_Color){255, 242, 184, 255});
      }
    }
    else
    {
      draw_walking_arm(r, x, y, PLAYER_W, dir, 14.0f, 13.0f + bob,
                       arm_swing, (SDL_Color){42, 118, 153, 255},
                       (SDL_Color){209, 154, 105, 255});
      if (p->active_weapon == PLAYER_WEAPON_PISTOL && p->bullets > 0)
      {
        float hand_x = 14.0f + arm_swing * 3.0f;
        sprite_rect(r, x, y, PLAYER_W, dir, hand_x, 20.0f + bob,
                    6.0f, 3.0f, (SDL_Color){31, 38, 43, 255});
      }
    }
  }
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
  float step = walking ? sinf(phase) : 0.0f;
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
  /* Short contact shadows keep the two silhouettes grounded without joining
   * them into one long, high-contrast stripe. */
  npc_contact_shadow(r, level, janitor->x + JANITOR_W * 0.5f,
                     janitor->y + 31.0f, 9.0f, 200, cam_x, oy);
  npc_contact_shadow(r, level, cart_x + cam_x + 11.0f,
                     janitor->y + 31.0f, 11.0f, 200, cam_x, oy);
  color_rect(r, COL_OUTLINE, cart_x, cart_y + 4.0f, 23.0f, 18.0f);
  color_rect(r, (SDL_Color){52, 59, 62, 255},
             cart_x + 2.0f, cart_y + 6.0f, 19.0f, 14.0f);
  color_rect(r, (SDL_Color){142, 112, 54, 255},
             cart_x + 3.0f, cart_y + 7.0f, 17.0f, 4.0f);
  color_rect(r, (SDL_Color){43, 79, 91, 255},
             cart_x + 4.0f, cart_y + 12.0f, 15.0f, 7.0f);
  color_rect(r, (SDL_Color){60, 108, 116, 255},
             cart_x + 6.0f, cart_y + 12.0f, 11.0f, 2.0f);
  color_rect(r, (SDL_Color){23, 29, 33, 255},
             cart_x + 2.0f, cart_y + 21.0f, 6.0f, 4.0f);
  color_rect(r, (SDL_Color){23, 29, 33, 255},
             cart_x + 16.0f, cart_y + 21.0f, 6.0f, 4.0f);
  color_rect(r, (SDL_Color){88, 96, 96, 255},
             cart_x + 4.0f, cart_y + 22.0f, 2.0f, 2.0f);
  color_rect(r, (SDL_Color){88, 96, 96, 255},
             cart_x + 18.0f, cart_y + 22.0f, 2.0f, 2.0f);
  set_color(r, (SDL_Color){82, 91, 92, 255});
  SDL_RenderLine(r, cart_x + (cart_dir > 0 ? 20.0f : 3.0f), cart_y + 5.0f,
                 cart_x + (cart_dir > 0 ? 24.0f : -1.0f), cart_y - 1.0f);

  /* The mop is clipped to the cart during a patrol and swept in a broad arc
   * while the janitor is working. */
  if (mopping)
  {
    sprite_segment(r, x, y, JANITOR_W, dir,
                   16.0f, 15.0f + bob, 28.0f + sweep, 30.0f,
                   4, COL_OUTLINE);
    sprite_segment(r, x, y, JANITOR_W, dir,
                   16.0f, 15.0f + bob, 28.0f + sweep, 30.0f,
                   2, (SDL_Color){130, 112, 82, 255});
    sprite_rect(r, x, y, JANITOR_W, dir,
                22.0f + sweep, 29.0f, 13.0f, 3.0f, COL_OUTLINE);
    sprite_rect(r, x, y, JANITOR_W, dir,
                23.0f + sweep, 30.0f, 11.0f, 2.0f,
                (SDL_Color){97, 132, 130, 255});
  }
  else
  {
    set_color(r, COL_OUTLINE);
    SDL_RenderLine(r, cart_x + 5.0f, cart_y + 5.0f,
                   cart_x + 9.0f, cart_y - 13.0f);
    set_color(r, (SDL_Color){130, 112, 82, 255});
    SDL_RenderLine(r, cart_x + 6.0f, cart_y + 5.0f,
                   cart_x + 10.0f, cart_y - 13.0f);
    color_rect(r, (SDL_Color){97, 132, 130, 255},
               cart_x + 5.0f, cart_y + 3.0f, 9.0f, 3.0f);
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
    sprite_rect(r, x, y, JANITOR_W, dir,
                14.0f, 7.0f + bob, 3.0f, 2.0f,
                (SDL_Color){186, 196, 190, 255});
    sprite_rect(r, x, y, JANITOR_W, dir,
                16.0f, 7.0f + bob, 1.0f, 2.0f,
                (SDL_Color){17, 28, 29, 255});
  }

  if (mopping)
  {
    sprite_limb_segment(r, x, y, JANITOR_W, dir,
                        8.0f, 14.0f + bob, 16.0f, 17.0f + bob, uniform_hi);
    sprite_limb_segment(r, x, y, JANITOR_W, dir,
                        18.0f, 14.0f + bob, 18.0f, 20.0f + bob, uniform);
    sprite_rect(r, x, y, JANITOR_W, dir,
                14.0f, 16.0f + bob, 4.0f, 4.0f, skin);
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
  float step = running ? sinf(phase) : 0.0f;
  /* How far into the sprawl this frame is: 1 while down, easing to 0 as the
     last of the beat is spent scrambling up. */
  float down = fallen ? fminf(1.0f, civilian->activity_timer /
                                        (CIVILIAN_STUMBLE_TIME * 0.35f))
                      : 0.0f;
  float drop = down * 9.0f;
  float bob = running ? fabsf(step) * 1.2f - 0.6f
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
  /* The open mouth is the one cue that reads as fear at this scale. */
  sprite_rect(r, x, y, CIVILIAN_W, dir, 11.0f + lean, 8.0f + body,
              4.0f, 1.0f, civilian_fade((SDL_Color){28, 12, 14, 255}, fade));
  sprite_rect(r, x, y, CIVILIAN_W, dir, 12.0f + lean, 9.0f + body,
              2.0f, 1.0f, civilian_fade((SDL_Color){48, 22, 24, 255}, fade));

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
  float step = walking ? sinf(phase) : 0.0f;
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
  if (fx_blinking(rec->anim_time, 0x5bu))
  {
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 13.0f, 7.0f + bob, 3.0f, 1.0f,
                (SDL_Color){20, 24, 30, 255});
  }
  else
  {
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 13.0f, 6.0f + bob, 3.0f, 2.0f,
                (SDL_Color){214, 220, 218, 255});
    sprite_rect(r, x, y, RECEPTIONIST_W, dir, 15.0f, 6.0f + bob, 1.0f, 2.0f,
                (SDL_Color){20, 24, 30, 255});
  }
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 13.0f, 9.0f + bob, 2.0f, 1.0f,
              (SDL_Color){146, 76, 76, 255});
  /* Headset: band, earpiece and a boom down to the mouth. It is what makes a
     figure standing still at a counter read as answering the switchboard. */
  sprite_mass(r, x, y, RECEPTIONIST_W, dir, 9.0f, 0.0f + bob, 8.0f, 2.0f,
              (SDL_Color){28, 32, 38, 255}, 2, 0);
  sprite_rect(r, x, y, RECEPTIONIST_W, dir, 8.0f, 5.0f + bob, 3.0f, 3.0f,
              (SDL_Color){28, 32, 38, 255});
  sprite_segment(r, x, y, RECEPTIONIST_W, dir, 11.0f, 7.0f + bob,
                 15.0f, 9.0f + bob, 1, (SDL_Color){28, 32, 38, 255});

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
  float step = moving ? sinf(phase) : 0.0f;
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
      sprite_rect(r, x, y, ENEMY_W, dir, 9.0f, 13.0f + bob, 12.0f, 2.0f,
                  FX_STEEL_LT);
      sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 15.0f + bob, 10.0f, 6.0f,
                  FX_STEEL_DK);
      sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 15.0f + bob, 10.0f, 1.0f,
                  FX_STEEL);
      sprite_rect(r, x, y, ENEMY_W, dir, 7.0f, 12.0f + bob, 3.0f, 3.0f,
                  FX_STEEL);
      sprite_rect(r, x, y, ENEMY_W, dir, 19.0f, 12.0f + bob, 3.0f, 3.0f,
                  FX_STEEL);
    }
  }
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 20.0f + bob, 11.0f, 2.0f, (SDL_Color){31, 37, 31, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 20.0f + bob, 11.0f, 1.0f,
              (SDL_Color){58, 66, 52, 255});

  if (e->climbing)
  {
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
    sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 6.0f + bob, 3.0f, 2.0f, FX_RED);
    sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 6.0f + bob, 3.0f, 1.0f,
                (SDL_Color){255, 138, 122, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 9.0f + bob, 2.0f, 1.0f, (SDL_Color){70, 34, 27, 255});
  }

  if (aiming && !e->climbing)
  {
    float recoil = e->recoil_timer > 0.07f ? -2.0f : 0.0f;
    sprite_rect(r, x, y, ENEMY_W, dir, 17.0f + recoil, 13.0f + bob, 8.0f, 5.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 18.0f + recoil, 14.0f + bob, 7.0f, 3.0f, fx_dim(FX_SKIN, 0.85f));
    sprite_rect(r, x, y, ENEMY_W, dir, 23.0f + recoil, 12.0f + bob, 8.0f, 4.0f, (SDL_Color){24, 29, 31, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 25.0f + recoil, 16.0f + bob, 3.0f, 5.0f, (SDL_Color){40, 44, 42, 255});
    if (e->recoil_timer > PLAYER_MUZZLE_FLASH_TIME)
    {
      draw_muzzle_flash(r, x, y, ENEMY_W, dir, 33.0f + recoil, 14.0f + bob,
                        (SDL_Color){255, 128, 74, 255});
      sprite_rect(r, x, y, ENEMY_W, dir, 31.0f + recoil, 11.0f + bob, 4.0f, 6.0f, FX_RED);
      sprite_rect(r, x, y, ENEMY_W, dir, 35.0f + recoil, 13.0f + bob, 3.0f, 3.0f, FX_AMBER);
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
  else if (!e->climbing)
  {
    float front_swing = moving ? -step : gesture_swing;
    draw_walking_arm(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                     front_swing, uniform,
                     fx_dim(FX_SKIN, 0.85f));
  }

  /* Compact health pips sit in-world without turning into a large UI bar.
     Granted-green and a red-shadow socket, because the semantic colours are
     rationed: green is the palette's "still standing" everywhere else too. */
  for (int hp = 0; hp < ENEMY_HP; ++hp)
  {
    SDL_Color hc = hp < e->hp ? FX_GREEN
                              : fx_mix(FX_SHADOW, FX_RED_DK, 0.35f);
    color_rect(r, FX_INK, x + 3.0f + hp * 7.0f, y - 6.0f, 6.0f, 4.0f);
    color_rect(r, hc, x + 4.0f + hp * 7.0f, y - 5.0f, 4.0f, 2.0f);
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
  SDL_Color uniform = fx_dim(FX_GUARD, 0.82f);
  SDL_Color trouser = (SDL_Color){34, 39, 31, 255};
  SDL_Color boot = (SDL_Color){28, 32, 26, 255};
  SDL_Color skin = fx_dim(FX_SKIN, 0.72f);

  /* Wider and fainter than the standing pool: the mass is spread along the
     floor rather than balanced on two boots. */
  npc_contact_shadow(r, level, e->x + ENEMY_W * 0.5f, e->y + 31.0f,
                     14.0f, 165, cam_x, oy);

  /*
   * The silhouette is the whole job. Lying down he has a tenth of the height
   * he had standing, so the parts have to be spread along the floor and read
   * separately or the figure collapses into one dark lump the eye files as
   * scenery: boots at one end, helmet at the other, and a knee drawn up
   * between them so the legs are two things rather than one.
   */
  sprite_limb_segment(r, x, y, ENEMY_W, dir, 11.0f, 26.0f, 5.0f, 24.0f,
                      trouser);
  sprite_shoe(r, x, y, ENEMY_W, dir, 4.0f, 24.0f, boot);

  sprite_body(r, x, y, ENEMY_W, dir, 8.0f, 23.0f, 11.0f, 8.0f, uniform,
              COL_OUTLINE, 1, 1);
  /* The plate carrier is what separates a guard from a man in a green shirt,
     lying down as much as standing up. */
  sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 25.0f, 7.0f, 4.0f,
              (SDL_Color){36, 43, 37, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 25.0f, 7.0f, 1.0f,
              (SDL_Color){58, 68, 52, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 29.0f, 11.0f, 1.0f,
              (SDL_Color){26, 31, 27, 255});

  /* Near leg along the floor, and the arm thrown out past the head. */
  sprite_limb_segment(r, x, y, ENEMY_W, dir, 11.0f, 29.0f, 4.0f, 30.0f,
                      trouser);
  sprite_shoe(r, x, y, ENEMY_W, dir, 3.0f, 30.0f, boot);
  sprite_limb_segment(r, x, y, ENEMY_W, dir, 16.0f, 27.0f, 22.0f, 30.0f,
                      uniform);
  sprite_rect(r, x, y, ENEMY_W, dir, 21.0f, 29.0f, 3.0f, 2.0f, skin);

  /* The head, and the helmet still on it but tipped back off the brow. */
  sprite_body(r, x, y, ENEMY_W, dir, 17.0f, 24.0f, 7.0f, 6.0f, skin,
              COL_OUTLINE, 1, 2);
  sprite_mass(r, x, y, ENEMY_W, dir, 15.0f, 21.0f, 10.0f, 5.0f, COL_OUTLINE,
              3, 1);
  sprite_mass(r, x, y, ENEMY_W, dir, 16.0f, 22.0f, 8.0f, 3.0f,
              (SDL_Color){44, 53, 40, 255}, 2, 0);
  sprite_mass(r, x, y, ENEMY_W, dir, 17.0f, 22.0f, 6.0f, 1.0f,
              (SDL_Color){86, 97, 66, 255}, 1, 0);
  /* The visor is dead. Lit, it is the pixel that says "enemy" across a room,
     and a body must not say it. */
  sprite_rect(r, x, y, ENEMY_W, dir, 20.0f, 26.0f, 3.0f, 1.0f,
              fx_dim(FX_RED_DK, 0.55f));
}

void draw_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
                     float cam_x, float oy)
{
  float x = dog->x - cam_x;
  float y = dog->y + oy;
  int dir = dog->dir;
  bool moving = fabsf(dog->vx) > 4.0f;
  bool chase = dog->state == DOG_CHASE;
  float phase = dog->anim_time * (chase ? 3.5f : 2.7f);
  float gait = moving ? sinf(phase) * 3.0f : 0.0f;
  float bob = moving ? fabsf(sinf(phase)) : sinf(dog->anim_time * 1.7f) * 0.35f;
  float lunge = dog->attack_timer > 0.0f ? 3.0f : 0.0f;
  SDL_Color fur = chase ? (SDL_Color){91, 59, 39, 255}
                        : (SDL_Color){70, 54, 42, 255};
  SDL_Color fur_hi = chase ? (SDL_Color){143, 82, 44, 255}
                           : (SDL_Color){109, 76, 51, 255};

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

  sprite_body(r, x, y, DOG_W, dir, 4.0f + lunge, 6.0f + bob, 14.0f, 7.0f, fur,
              COL_OUTLINE, 1, 1);
  sprite_rect(r, x, y, DOG_W, dir, 7.0f + lunge, 6.0f + bob, 8.0f, 2.0f, fur_hi);

  /* Hindquarters and animated tail. */
  sprite_form(r, x, y, DOG_W, dir, 1.0f + lunge, 6.0f + bob, 6.0f, 7.0f, fur);
  sprite_rect(r, x, y, DOG_W, dir, 0.0f + lunge, 3.0f + bob - gait * 0.35f, 4.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 0.0f + lunge, 4.0f + bob - gait * 0.35f, 3.0f, 1.0f, fur_hi);

  /* Long working-dog muzzle, ears and alert eye. */
  sprite_body(r, x, y, DOG_W, dir, 17.0f + lunge, 3.0f + bob, 6.0f, 7.0f,
              fur_hi, COL_OUTLINE, 1, 1);
  sprite_rect(r, x, y, DOG_W, dir, 17.0f + lunge, 0.0f + bob, 4.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 18.0f + lunge, 1.0f + bob, 2.0f, 3.0f, fur);
  sprite_rect(r, x, y, DOG_W, dir, 21.0f + lunge, 6.0f + bob, 4.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 23.0f + lunge, 7.0f + bob, 2.0f, 2.0f, FX_INK);
  /* The alert eye blinks like every other eye in the cast — a dog whose eye
     never closes is a glass one — but never mid-charge. */
  if (chase || !fx_blinking(dog->anim_time, 0x0d06u))
    sprite_rect(r, x, y, DOG_W, dir, 21.0f + lunge, 4.0f + bob, 2.0f, 2.0f,
                chase ? FX_RED : fx_mix(FX_AMBER, FX_CREAM, 0.45f));
  color_rect(r, chase ? FX_RED : FX_AMBER, x + 13.0f, y + 7.0f + bob, 5.0f, 2.0f);

  if (dog->attack_timer > 0.0f)
  {
    sprite_rect(r, x, y, DOG_W, dir, 21.0f + lunge, 10.0f + bob, 5.0f, 3.0f,
                fx_dim(FX_RED_DK, 0.70f));
    sprite_rect(r, x, y, DOG_W, dir, 23.0f + lunge, 10.0f + bob, 2.0f, 1.0f,
                FX_CREAM);
  }

  /* Four-beat run condensed into two readable leg pairs, each on the same
     stance-and-swing cycle the rest of the cast walks: through stance the paw
     holds its ground and tracks back under the body, through swing it lifts
     and reaches, half a turn apart. The old sine bobbed both pairs in place —
     paws that pump vertically while the body slides is the quadruped version
     of skating. Legs are fur over an outline rather than bare outline; a dog
     standing on two strokes of ink has no legs, only supports. */
  float run = phase * 0.5f;
  for (int pair = 0; pair < 2; ++pair)
  {
    /* A parked dog holds mid-stance on all fours instead of freezing one
       pair mid-swing. */
    float cycle = moving ? run + (float)pair * 0.5f : 0.25f;
    cycle -= floorf(cycle);
    float reach = moving ? 2.5f : 0.0f;
    float leg_x = pair == 0 ? 15.0f : 5.0f;
    float leg_lift = 0.0f;
    if (cycle < 0.5f)
      leg_x += reach * (1.0f - 4.0f * cycle);
    else
    {
      float t = (cycle - 0.5f) * 2.0f;
      float ease = t * t * (3.0f - 2.0f * t);
      leg_x += reach * (-1.0f + 2.0f * ease);
      leg_lift = sinf(t * 3.14159265f) * 1.5f;
    }
    sprite_rect(r, x, y, DOG_W, dir, leg_x + lunge, 12.0f - leg_lift,
                4.0f, 4.0f + leg_lift * 0.5f, COL_OUTLINE);
    sprite_rect(r, x, y, DOG_W, dir, leg_x + 1.0f + lunge, 12.0f - leg_lift,
                2.0f, 3.0f + leg_lift * 0.5f, fur);
  }
}

/* The dog, down, on its side. Same reason as the guard: a handler who finds it
   investigates and may raise the alarm, and the animal has to be on the floor
   for that to be a thing the player saw happen. */
void draw_downed_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
                            float cam_x, float oy)
{
  float x = dog->x - cam_x;
  float y = dog->y + oy;
  int dir = dog->dir;
  SDL_Color fur = (SDL_Color){56, 43, 34, 255};
  SDL_Color fur_hi = (SDL_Color){84, 60, 41, 255};

  npc_contact_shadow(r, level, dog->x + DOG_W * 0.5f, dog->y + 15.0f,
                     12.0f, 150, cam_x, oy);

  /* Legs out sideways off the lying body: the one line that says this animal
     is not standing. */
  sprite_rect(r, x, y, DOG_W, dir, 6.0f, 7.0f, 2.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 11.0f, 7.0f, 2.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 6.0f, 8.0f, 1.0f, 3.0f, fur);
  sprite_rect(r, x, y, DOG_W, dir, 11.0f, 8.0f, 1.0f, 3.0f, fur);

  sprite_rect(r, x, y, DOG_W, dir, 0.0f, 11.0f, 4.0f, 2.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 0.0f, 11.0f, 3.0f, 1.0f, fur);
  sprite_body(r, x, y, DOG_W, dir, 3.0f, 10.0f, 13.0f, 5.0f, fur,
              COL_OUTLINE, 1, 1);
  sprite_body(r, x, y, DOG_W, dir, 15.0f, 11.0f, 7.0f, 4.0f, fur_hi,
              COL_OUTLINE, 1, 1);
  sprite_rect(r, x, y, DOG_W, dir, 15.0f, 9.0f, 3.0f, 2.0f, COL_OUTLINE);
  /* The eye is shut. An open one on a body is the animal watching the room. */
  sprite_rect(r, x, y, DOG_W, dir, 18.0f, 12.0f, 2.0f, 1.0f, FX_INK);
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

void draw_bird(SDL_Renderer *r, const Bird *bird,
                      float cam_x, float oy)
{
  float x = bird->x - cam_x;
  float y = bird->y + oy;
  float flap = sinf(bird->anim_time * 15.0f);
  int dir = bird->vx >= 0.0f ? 1 : -1;
  float head_x = dir > 0 ? x + 19.0f : x + 2.0f;
  /* City-pigeon slate out of the room's own darks, not a fourth grey. */
  SDL_Color body = fx_mix(FX_INK, FX_STEEL_DK, 0.60f);
  SDL_Color wing = fx_mix(FX_INK, FX_STEEL_DK, 0.95f);
  FxRamp body_ramp = fx_ramp(body);
  FxRamp wing_ramp = fx_ramp(wing);

  /* The one animate thing that was still a stack of flat swatches: the body
     and head are masses with the ceiling light on their crowns, and the wing
     is a form so the downbeat reads as a surface turning, not a bar moving. */
  fx_mass(r, COL_OUTLINE, x + 6.0f, y + 4.0f, 15.0f, 8.0f, 1, 1);
  fx_form_mass(r, x + 7.0f, y + 5.0f, 13.0f, 6.0f, body_ramp, dir, 1, 1);
  fx_mass(r, COL_OUTLINE, head_x - 1.0f, y + 2.0f, 8.0f, 8.0f, 1, 1);
  fx_form_mass(r, head_x, y + 3.0f, 6.0f, 6.0f, body_ramp, dir, 1, 1);
  color_rect(r, fx_mix(FX_AMBER_DK, FX_AMBER, 0.30f),
             dir > 0 ? head_x + 5.0f : head_x - 3.0f, y + 5.0f, 4.0f, 2.0f);
  color_rect(r, FX_INK, dir > 0 ? head_x + 3.0f : head_x + 1.0f,
             y + 4.0f, 1.0f, 1.0f);
  if (flap > 0.0f)
  {
    fx_form_block(r, x + 8.0f, y, 10.0f, 5.0f, wing_ramp, dir);
    fx_form_block(r, x + 10.0f, y + 9.0f, 8.0f, 3.0f, wing_ramp, dir);
  }
  else
  {
    fx_form_block(r, x + 8.0f, y + 7.0f, 10.0f, 5.0f, wing_ramp, dir);
    fx_form_block(r, x + 10.0f, y + 2.0f, 8.0f, 3.0f, wing_ramp, dir);
  }
  color_rect(r, body_ramp.dark, dir > 0 ? x + 3.0f : x + 19.0f,
             y + 6.0f, 5.0f, 3.0f);
}
