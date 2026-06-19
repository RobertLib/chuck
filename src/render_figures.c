/*
 * The cast. See [render_figures.h](render_figures.h) for what is in here and
 * why it is one module.
 *
 * Every figure is a sprite out of [bbc_cast.c](bbc_cast.c), stood on the
 * floor of its own collision box and chosen by the state it is in — a frame,
 * never an interpolated limb — and everything laid on top of one (the pistol
 * in a hand, a muzzle flash, a fuse, a health pip) is a whole BBC pixel at a
 * whole cell of that sprite. Nothing here reads anything but the simulation
 * state it is handed: a renderer that could change what a figure does would
 * be a renderer the tests cannot vouch for.
 */

#include "render_figures.h"

#include <math.h>

#include "bbc.h"
#include "bbc_cast.h"
#include "game_config.h"

/* ---- Where a sprite lands ------------------------------------------- */

/*
 * A sprite stood on a point, and the grid cell it ended up on.
 *
 * `bbc_sprite_at_feet` rounds a figure's centre onto the grid and draws it;
 * everything that has to be laid *on* the figure — the round leaving the
 * muzzle, the pin of a grenade in a fist — has to land on the cell the figure
 * landed on, so the rounding is done once here and every overlay is placed by
 * column and row of the figure rather than by logical pixel. A column counts
 * from the figure's back for a figure facing right and from its front for one
 * facing left, which is to say overlays mirror with the man.
 */
typedef struct
{
  float x, y; /* top-left, on the grid */
  int w;      /* columns */
  int dir;
} Spot;

static Spot spot_at_feet(const BbcSprite *s, float cx, float foot_y, int dir)
{
  Spot spot;
  float x = cx - bbc_sprite_px_w(s) * 0.5f;
  float y = foot_y - bbc_sprite_px_h(s);

  spot.x = floorf(x / (float)BBC_PX_W + 0.5f) * (float)BBC_PX_W;
  spot.y = floorf(y / (float)BBC_PX_H + 0.5f) * (float)BBC_PX_H;
  spot.w = bbc_sprite_w(s);
  spot.dir = dir >= 0 ? 1 : -1;
  return spot;
}

static float spot_x(const Spot *s, int col)
{
  int c = s->dir >= 0 ? col : s->w - 1 - col;
  return s->x + (float)(c * BBC_PX_W);
}

static float spot_y(const Spot *s, int row)
{
  return s->y + (float)(row * BBC_PX_H);
}

static void spot_plot(SDL_Renderer *r, const Spot *s, int col, int row,
                      BbcColour c)
{
  bbc_plot(r, c, spot_x(s, col), spot_y(s, row));
}

static void spot_draw(SDL_Renderer *r, const Spot *s, const BbcSprite *sprite,
                      const BbcColour *slots)
{
  bbc_sprite(r, sprite, s->x, s->y, s->dir, slots);
}

/* Another sprite laid on the figure with its own top-left at the figure's
   (col, row), mirrored about the figure rather than about itself. */
static void spot_overlay(SDL_Renderer *r, const Spot *s, const BbcSprite *o,
                         int col, int row, const BbcColour *slots)
{
  int ow = bbc_sprite_w(o);
  int c = s->dir >= 0 ? col : s->w - col - ow;
  bbc_sprite(r, o, s->x + (float)(c * BBC_PX_W), s->y + (float)(row * BBC_PX_H),
             s->dir, slots);
}

/* The logical left edge of something `cells` wide whose near edge is at the
   figure's column `col` and which runs on away from him — a grenade past the
   hand that threw it. */
static float spot_item_x(const Spot *s, int col, int cells)
{
  float x = spot_x(s, col);
  return s->dir >= 0 ? x : x - (float)((cells - 1) * BBC_PX_W);
}

static BbcColour cast_colour(int code, const BbcColour *slots)
{
  if (code >= BBC_CODE_SLOT0)
    return slots != NULL ? slots[code - BBC_CODE_SLOT0] : BBC_WHITE;
  return (BbcColour)code;
}

/*
 * Rows `row0` to `row1` of a sprite, the rest left alone.
 *
 * A BBC game split a figure at the belt for the same reason this does: the
 * legs know whether he is running and the arms know what he is holding, and
 * drawing every combination of the two would be a sheet of paper the size of
 * the room. The upper rows of an aim over the lower rows of a stride is a man
 * shooting on the move.
 */
static void spot_rows(SDL_Renderer *r, const Spot *s, const BbcSprite *sprite,
                      const BbcColour *slots, int row0, int row1)
{
  int w = bbc_sprite_w(sprite);
  for (int row = row0; row < row1 && row < sprite->h; ++row)
  {
    const char *line = sprite->rows[row];
    for (int col = 0; col < w && line[col] != '\0'; ++col)
    {
      int code = bbc_sprite_code(line[col]);
      if (code >= 0)
        spot_plot(r, s, col, row, cast_colour(code, slots));
    }
  }
}

/*
 * A figure leaving the shot without alpha, which the machine did not have:
 * its pixels go out one by one in a fixed order until none is left. The
 * order is a 4x4 ordered dither anchored to the sprite's own cells, so a
 * figure dissolving as it walks keeps the same pixels lit as it goes rather
 * than boiling.
 */
static void spot_dissolve(SDL_Renderer *r, const Spot *s,
                          const BbcSprite *sprite, const BbcColour *slots,
                          float keep)
{
  static const unsigned char BAYER[4][4] = {
      {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  int w = bbc_sprite_w(sprite);
  for (int row = 0; row < sprite->h; ++row)
  {
    const char *line = sprite->rows[row];
    for (int col = 0; col < w && line[col] != '\0'; ++col)
    {
      int code = bbc_sprite_code(line[col]);
      if (code < 0)
        continue;
      float threshold = ((float)BAYER[row & 3][col & 3] + 0.5f) / 16.0f;
      if (threshold < keep)
        spot_plot(r, s, col, row, cast_colour(code, slots));
    }
  }
}

/* A short blink on a figure's own clock: on for the first half of every
   `period` seconds. The hardware's flash is half a second each way, which is
   right for an alarm and far too slow for a fuse or a muzzle telegraph that
   is over in less than that. */
static bool blink(float t, float period)
{
  if (!(period > 0.0f))
    return true;
  float phase = t - period * floorf(t / period);
  return phase < period * 0.5f;
}

/* Which of `count` frames a figure that has walked to `x` is on: travel, not a
   clock, so a planted foot stays planted while the body passes over it, and a
   figure standing still holds whatever frame it stopped on. */
static int stride_frame(float x, int dir, float stride, int count)
{
  float travelled = x * (float)(dir >= 0 ? 1 : -1);
  int f = (int)floorf(travelled / stride);
  f %= count;
  return f < 0 ? f + count : f;
}

/* ---- What is carried -------------------------------------------------- */

/*
 * A frag grenade, in a hand mid-throw, lying on the floor as a pickup, and in
 * the HUD's carry row. Green and round with the pin on top, and the same
 * `SPR_ICON_GRENADE` the HUD lays in its carry row, whose ink it measures from
 * `x - 1` (`HUD_GRENADE_INK_W`, the icon's three columns and the pixel before
 * them). While the fuse burns a spark at the pin blinks, faster than any
 * alarm, because it is about to go off.
 */
void draw_grenade(SDL_Renderer *r, float x, float y, float fuse)
{
  float left = x - 1.0f;
  bbc_sprite(r, &SPR_ICON_GRENADE, left, y, 1, NULL);
  if (fuse > 0.0f)
  {
    bool lit = blink(fuse, 1.0f / 7.0f);
    bbc_plot(r, lit ? BBC_YELLOW : BBC_RED, left + (float)(2 * BBC_PX_W),
             y - (float)BBC_PX_H);
    if (lit)
      bbc_plot(r, BBC_WHITE, left + (float)(3 * BBC_PX_W),
               y - (float)(2 * BBC_PX_H));
  }
}

/*
 * A bolt in the air: two columns of bright steel, drawn small on purpose. The
 * player has to tell it from a grenade at a glance, because one of the two is
 * about to go off. Nothing about it is animated — it is in the air for well
 * under a second and the noise it makes is the event, not the flight.
 */
void draw_decoy(SDL_Renderer *r, float x, float y)
{
  bbc_sprite(r, &SPR_ICON_DECOY, x, y, 1, NULL);
}

/*
 * A flash charge — in the air, and lying on the floor as a pickup.
 *
 * It has to be told from a grenade at a glance, because one of the two is
 * about to kill whoever is standing next to it. So it is the other half of
 * the palette and the other half of the geometry: a white can with a yellow
 * band where a frag is a green egg, the cap cyan where the frag's pin is
 * white, in exactly the grenade's three columns — which is why the HUD's carry
 * row measures both the same (`HUD_FLASH_INK_W`). As the fuse runs down the
 * cap strobes white and a white spark stands off it.
 */
void draw_flashbang(SDL_Renderer *r, float x, float y, float fuse)
{
  float left = x - 1.0f;
  bbc_sprite(r, &SPR_ICON_FLASH, left, y, 1, NULL);
  if (fuse > 0.0f && blink(fuse, 1.0f / 9.0f))
  {
    bbc_plot(r, BBC_WHITE, left + (float)BBC_PX_W, y);
    bbc_plot(r, BBC_CYAN, left + (float)(2 * BBC_PX_W), y - (float)BBC_PX_H);
  }
}

/*
 * A sheet off the docket, lying where it fell out of a case: white paper,
 * lines of type and the magenta stamp. It is the one pickup that is not a
 * weapon, a heart or a door, and it has to read that way from across a room
 * or a player will walk past it assuming they are full up on whatever it is —
 * which is why it is the whitest thing on the floor.
 */
void draw_evidence_pickup(SDL_Renderer *r, float x, float y)
{
  bbc_sprite(r, &SPR_ICON_EVIDENCE, x + 2.0f, y + 2.0f, 1, NULL);
}

/* The thing in the hand of a throw, which is not always a grenade.
 *
 * All three underarm throwables share one pose and one flag — see
 * `Player.throwing_weapon` — and for as long as that was true the ladder pose
 * drew a grenade for every one of them, including the flash charge, which
 * "has to be told from a grenade at a glance, because one of the two is about
 * to kill whoever is standing next to it". A shared animation is not a shared
 * prop. The grenade is the default arm rather than an error, because a figure
 * with an empty hand mid-throw is worse than one holding the wrong thing. */
static void draw_thrown_in_hand(SDL_Renderer *r, const Player *p, float x,
                                float y)
{
  switch (p->throwing_weapon)
  {
  case PLAYER_WEAPON_FLASH:
    draw_flashbang(r, x + 1.0f, y, 0.0f);
    return;
  case PLAYER_WEAPON_DECOY:
    /* A row down, in the middle of the space the fatter two fill, so a bolt
       does not read as a grenade that has slipped out of the fingers. */
    draw_decoy(r, x + (float)BBC_PX_W * 0.5f, y + (float)(2 * BBC_PX_H));
    return;
  case PLAYER_WEAPON_PISTOL:
  case PLAYER_WEAPON_KNIFE:
  case PLAYER_WEAPON_GRENADE:
  case PLAYER_WEAPON_BAZOOKA:
  case PLAYER_WEAPON_COUNT:
    break;
  }
  draw_grenade(r, x + 1.0f, y, 0.0f);
}

/* A shot going off sideways: the burst laid with its back edge on the
   figure's cell `col`, the one just past the muzzle, and its long arm along
   `row`, the line the round leaves on. It mirrors with the figure, so a man
   facing left fires to the left. */
static void muzzle_burst(SDL_Renderer *r, const Spot *s, int col, int row,
                         const BbcSprite *burst)
{
  spot_overlay(r, s, burst, col, row - burst->h / 2, NULL);
}

/* And straight up a shaft or down it: the star centred on (col, row). */
static void muzzle_star(SDL_Renderer *r, const Spot *s, int col, int row)
{
  spot_overlay(r, s, &SPR_MUZZLE_STAR,
               col - bbc_sprite_w(&SPR_MUZZLE_STAR) / 2,
               row - SPR_MUZZLE_STAR.h / 2, NULL);
}

/* ---- Chuck ---------------------------------------------------------- */

/*
 * What his near hand is doing side on, which decides both the arms and what
 * is drawn past the fist.
 */
typedef enum
{
  CHUCK_PROP_NONE,
  CHUCK_PROP_PISTOL_CARRIED,
  CHUCK_PROP_KNIFE_CARRIED,
  CHUCK_PROP_PISTOL_AIMED,
  CHUCK_PROP_KNIFE_THRUST,
  CHUCK_PROP_THROW,
  CHUCK_PROP_BAZOOKA
} ChuckProp;

/* The rows above the belt; everything from here down is the legs. */
#define CHUCK_BELT_ROW 10

/* Where the near hand is in each frame he can carry something in, and so
   where the carried pistol or blade goes: the cell just under the fist. */
typedef struct
{
  const BbcSprite *frame;
  signed char col, row;
} ChuckHand;

static const ChuckHand CHUCK_HANDS[] = {
    {&SPR_CHUCK_STAND, 3, 11}, {&SPR_CHUCK_WALK1, 0, 9},
    {&SPR_CHUCK_WALK2, 4, 10}, {&SPR_CHUCK_WALK3, 7, 9},
    {&SPR_CHUCK_JUMP, 0, 8},   {&SPR_CHUCK_FALL, 0, 0},
    {&SPR_CHUCK_LAND, 0, 10},
};

static void draw_carried(SDL_Renderer *r, const Spot *s,
                         const BbcSprite *frame, ChuckProp prop)
{
  for (size_t i = 0; i < SDL_arraysize(CHUCK_HANDS); ++i)
  {
    if (CHUCK_HANDS[i].frame != frame)
      continue;
    /* Brass for the pistol, bright steel for the blade: the one thing on him
       that says what the next press of the trigger does — shoot, or stab. */
    spot_plot(r, s, CHUCK_HANDS[i].col, CHUCK_HANDS[i].row,
              prop == CHUCK_PROP_PISTOL_CARRIED ? BBC_YELLOW : BBC_WHITE);
    return;
  }
}

/* The launcher on the shoulder, laid down before the man so it passes behind
   his head; kicked a column back for the first half of the shot, with the
   backblast out of its tail and the flash out of its mouth. */
static void draw_shouldered_tube(SDL_Renderer *r, const Spot *s, bool kick)
{
  int col = CHUCK_BAZOOKA_COL - (kick ? 1 : 0);
  spot_overlay(r, s, &SPR_BAZOOKA, col, CHUCK_BAZOOKA_ROW, NULL);
}

static void draw_tube_blast(SDL_Renderer *r, const Spot *s, bool kick)
{
  int tail = CHUCK_BAZOOKA_COL - (kick ? 1 : 0) - 1;
  int mouth = CHUCK_BAZOOKA_COL - (kick ? 1 : 0) + bbc_sprite_w(&SPR_BAZOOKA);
  int row = CHUCK_BAZOOKA_ROW + 2;
  /* The backblast: white at the tail, burning out to red behind it. */
  spot_plot(r, s, tail, row, BBC_WHITE);
  spot_plot(r, s, tail, row - 1, BBC_YELLOW);
  spot_plot(r, s, tail, row + 1, BBC_YELLOW);
  spot_plot(r, s, tail - 1, row, BBC_YELLOW);
  spot_plot(r, s, tail - 2, row, BBC_RED);
  spot_plot(r, s, tail - 1, row - 2, BBC_RED);
  spot_plot(r, s, tail - 1, row + 2, BBC_RED);
  muzzle_burst(r, s, mouth, row, &SPR_MUZZLE_BURST);
}

static void draw_player_side(SDL_Renderer *r, const Player *p, float cx,
                             float feet, float land_squash)
{
  int dir = p->facing;
  bool moving = fabsf(p->vx) > 2.0f;
  bool airborne = !p->on_ground;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  /* A throw is not a shot: every throw sets `grenade_throwing` and
     `action_timer` exactly as a shot does, so without its own branch a grenade
     lobbed on the floor was drawn as a pistol going off beside it. */
  bool throwing = p->action_timer > 0.0f && p->grenade_throwing && !knife;
  bool firing = p->action_timer > 0.0f && !knife && !throwing;
  bool bazooka = (p->active_weapon == PLAYER_WEAPON_BAZOOKA &&
                  p->bazooka_rockets > 0) ||
                 (firing && p->bazooka_firing);
  ChuckProp prop = CHUCK_PROP_NONE;

  if (knife)
    prop = CHUCK_PROP_KNIFE_THRUST;
  else if (throwing)
    prop = CHUCK_PROP_THROW;
  else if (bazooka)
    prop = CHUCK_PROP_BAZOOKA;
  else if (firing)
    prop = CHUCK_PROP_PISTOL_AIMED;
  else if (p->active_weapon == PLAYER_WEAPON_PISTOL && p->bullets > 0)
    prop = CHUCK_PROP_PISTOL_CARRIED;
  else if (p->active_weapon == PLAYER_WEAPON_KNIFE ||
           p->active_weapon == PLAYER_WEAPON_PISTOL)
    /* With a dry clip or the knife picked the trigger stabs rather than
       shoots, and the hand is the only thing that can say which. */
    prop = CHUCK_PROP_KNIFE_CARRIED;

  /* The legs: in the air, mid-stride, landing, or planted. A stride is chosen
     by how far he has travelled — WALK1, WALK2, WALK3, WALK2 — so a foot on
     the floor stays where it was put while he passes over it. Hauling a body
     is a walk of its own: nobody runs with a man's collar in his fist. */
  static const BbcSprite *const RUN[4] = {&SPR_CHUCK_WALK1, &SPR_CHUCK_WALK2,
                                          &SPR_CHUCK_WALK3, &SPR_CHUCK_WALK2};
  const BbcSprite *legs;
  if (airborne)
    legs = p->vy < 0.0f ? &SPR_CHUCK_JUMP : &SPR_CHUCK_FALL;
  else if (p->dragging)
    legs = moving && stride_frame(p->x, dir, 12.0f, 2) == 1 ? &SPR_CHUCK_DRAG2
                                                            : &SPR_CHUCK_DRAG;
  else if (moving)
    legs = RUN[stride_frame(p->x, dir, 11.0f, 4)];
  else if (land_squash > 0.2f && prop != CHUCK_PROP_BAZOOKA)
    legs = &SPR_CHUCK_LAND;
  else
    legs = &SPR_CHUCK_STAND;

  /* The arms: what the hand is doing, over whatever the legs are doing. */
  const BbcSprite *arms = NULL;
  switch (prop)
  {
  case CHUCK_PROP_KNIFE_THRUST:
    arms = &SPR_CHUCK_KNIFE;
    break;
  case CHUCK_PROP_THROW:
    arms = &SPR_CHUCK_THROW;
    break;
  case CHUCK_PROP_BAZOOKA:
    arms = &SPR_CHUCK_SHOULDER;
    break;
  case CHUCK_PROP_PISTOL_AIMED:
    arms = p->shot_vertical < 0   ? &SPR_CHUCK_AIM_UP
           : p->shot_vertical > 0 ? &SPR_CHUCK_AIM_DOWN
                                  : &SPR_CHUCK_AIM;
    break;
  case CHUCK_PROP_PISTOL_CARRIED:
  case CHUCK_PROP_KNIFE_CARRIED:
  case CHUCK_PROP_NONE:
    break;
  }
  /* Hauling takes both hands whatever is in them. */
  if (p->dragging)
    arms = NULL;
  bool tube = arms == &SPR_CHUCK_SHOULDER;

  Spot s = spot_at_feet(legs, cx, feet, dir);
  bool kick = tube && firing && p->bazooka_firing &&
              p->action_timer > PLAYER_SHOT_ACTION_TIME * 0.5f;
  if (tube)
    draw_shouldered_tube(r, &s, kick);

  if (arms == NULL)
  {
    spot_draw(r, &s, legs, NULL);
    if (prop == CHUCK_PROP_PISTOL_CARRIED || prop == CHUCK_PROP_KNIFE_CARRIED)
      draw_carried(r, &s, legs, prop);
  }
  else if (!airborne && !moving)
  {
    /* Planted: the action's own stance, feet braced under it. */
    spot_draw(r, &s, arms, NULL);
  }
  else
  {
    /* On the move or in the air: the action's arms over the legs' stride. */
    spot_rows(r, &s, arms, NULL, 0, CHUCK_BELT_ROW);
    spot_rows(r, &s, legs, NULL, CHUCK_BELT_ROW, legs->h);
  }

  bool flash = p->action_timer > PLAYER_MUZZLE_FLASH_TIME;
  if (prop == CHUCK_PROP_PISTOL_AIMED && flash)
  {
    if (p->shot_vertical < 0)
      muzzle_star(r, &s, 7, -3);
    else if (p->shot_vertical > 0)
      muzzle_star(r, &s, 7, 13);
    else
      muzzle_burst(r, &s, 8, 7, &SPR_MUZZLE_BURST);
  }
  else if (prop == CHUCK_PROP_KNIFE_THRUST &&
           p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f)
  {
    /* The thrust: the blade's point out past the fist for the first half of
       the stroke, then drawn back. */
    spot_plot(r, &s, 8, 6, BBC_WHITE);
  }
  else if (tube && p->bazooka_firing && flash)
  {
    draw_tube_blast(r, &s, kick);
  }
}

/* ---- On a ladder, from behind ------------------------------------------ */

static void draw_player_climbing(SDL_Renderer *r, const Player *p, float cx,
                                 float feet)
{
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  bool throwing = p->action_timer > 0.0f && p->grenade_throwing && !knife;
  bool firing = p->action_timer > 0.0f && !knife && !throwing;
  bool bazooka = firing && p->bazooka_firing;
  bool acting = knife || throwing || firing;

  if (!acting)
  {
    /* The stroke: one hand and the opposite knee up, then the mirror. It
       changes with the distance climbed — or traversed — so a man holding
       still on the rungs holds still. */
    int stroke = stride_frame(p->y + p->x, 1, 12.0f, 2);
    Spot s = spot_at_feet(&SPR_CHUCK_CLIMB, cx, feet, stroke == 0 ? 1 : -1);
    spot_draw(r, &s, &SPR_CHUCK_CLIMB, NULL);
    return;
  }

  /* One hand keeps the rail and the other does the work, up the shaft, down
     it, or out to the side he is facing. */
  int v = p->shot_vertical;
  const BbcSprite *pose = v < 0   ? &SPR_CHUCK_CLIMB_AIM_UP
                          : v > 0 ? &SPR_CHUCK_CLIMB_AIM_DOWN
                                  : &SPR_CHUCK_CLIMB_AIM;
  int dir = v != 0 ? 1 : p->facing;
  Spot s = spot_at_feet(pose, cx, feet, dir);
  /* Where the working hand is, and which way away from him "past it" is. */
  int hand_col = v < 0 ? 6 : 7;
  int hand_row = v < 0 ? 0 : (v > 0 ? 11 : 6);
  int step_col = v == 0 ? 1 : 0;
  int step_row = v < 0 ? -1 : (v > 0 ? 1 : 0);
  bool flash = p->action_timer > PLAYER_MUZZLE_FLASH_TIME;

  if (bazooka)
  {
    /* The tube goes on first and the man over it, as it does on the floor. */
    if (v < 0)
      spot_overlay(r, &s, &SPR_BAZOOKA_UP, 5, -6, NULL);
    else if (v > 0)
      spot_overlay(r, &s, &SPR_BAZOOKA_DOWN, 5, 6, NULL);
    else
      spot_overlay(r, &s, &SPR_BAZOOKA, 1, 4, NULL);
  }
  spot_draw(r, &s, pose, NULL);

  if (knife)
  {
    /* Two pixels of blade out past the fist, three on the thrust. */
    int reach = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f ? 3 : 2;
    for (int i = 1; i <= reach; ++i)
      spot_plot(r, &s, hand_col + step_col * i, hand_row + step_row * i,
                BBC_WHITE);
  }
  else if (throwing)
  {
    /* What left the hand, just past it: the pose is one animation for all
       three throwables, and the prop is not. */
    float ix;
    float iy;
    if (v < 0)
    {
      ix = spot_x(&s, 5);
      iy = spot_y(&s, -5);
    }
    else if (v > 0)
    {
      ix = spot_x(&s, 5);
      iy = spot_y(&s, 12);
    }
    else
    {
      ix = spot_item_x(&s, 8, 3);
      iy = spot_y(&s, 4);
    }
    draw_thrown_in_hand(r, p, ix, iy);
  }
  else if (bazooka)
  {
    if (flash)
    {
      if (v < 0)
        muzzle_star(r, &s, 6, -9);
      else if (v > 0)
        muzzle_star(r, &s, 6, 19);
      else
        muzzle_burst(r, &s, 11, 6, &SPR_MUZZLE_BURST);
    }
  }
  else
  {
    /* The pistol past the hand, and the flash past the pistol. */
    spot_plot(r, &s, hand_col + step_col, hand_row + step_row, BBC_YELLOW);
    if (flash)
    {
      if (v == 0)
        muzzle_burst(r, &s, hand_col + 2, hand_row, &SPR_MUZZLE_BURST);
      else
        muzzle_star(r, &s, hand_col, hand_row + step_row * 4);
    }
  }
}

/* ---- Flat on his elbows ------------------------------------------------- */

static void draw_player_crawling(SDL_Renderer *r, const Player *p, float cx,
                                 float feet)
{
  int dir = p->facing;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  bool throwing = p->action_timer > 0.0f && p->grenade_throwing && !knife;
  bool firing = p->action_timer > 0.0f && !knife && !throwing;
  bool bazooka = (p->active_weapon == PLAYER_WEAPON_BAZOOKA &&
                  p->bazooka_rockets > 0) ||
                 (firing && p->bazooka_firing);
  bool moving = fabsf(p->vx) > 1.0f;
  const BbcSprite *frame =
      moving && stride_frame(p->x, dir, 9.0f, 2) == 1 ? &SPR_CHUCK_CRAWL2
                                                      : &SPR_CHUCK_CRAWL1;
  Spot s = spot_at_feet(frame, cx, feet, dir);
  int last = frame->h - 1;

  /* The launcher rides along his back, under him, so the head stays in
     front of it. */
  if (bazooka)
    spot_overlay(r, &s, &SPR_ICON_BAZOOKA, 1, last - 4, NULL);
  spot_draw(r, &s, frame, NULL);

  if (knife)
  {
    int reach = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f ? 2 : 1;
    for (int i = 1; i <= reach; ++i)
      spot_plot(r, &s, 6 + i, last, BBC_WHITE);
  }
  else if (throwing)
  {
    /* Flat on the floor a throw is a flick along it: the fingers open out
       past the head. */
    spot_plot(r, &s, 7, last, BBC_WHITE);
  }
  else if (bazooka)
  {
    if (firing && p->bazooka_firing &&
        p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
      muzzle_burst(r, &s, 9, last - 3, &SPR_MUZZLE_BURST);
  }
  else if ((p->active_weapon == PLAYER_WEAPON_PISTOL && p->bullets > 0) ||
           firing)
  {
    spot_plot(r, &s, 7, last, BBC_YELLOW);
    /* Prone or standing, a shot goes off at the muzzle. */
    if (firing && p->action_timer > PLAYER_MUZZLE_FLASH_TIME)
      muzzle_burst(r, &s, 8, last - 2, &SPR_MUZZLE_BURST);
  }
}

static void draw_player_hacking(SDL_Renderer *r, float cx, float feet,
                                float hack_time)
{
  /* From behind at the keys, the elbows trading as he types. */
  const BbcSprite *frame = blink(hack_time, 0.30f) ? &SPR_CHUCK_HACK1
                                                    : &SPR_CHUCK_HACK2;
  Spot s = spot_at_feet(frame, cx, feet, 1);
  spot_draw(r, &s, frame, NULL);
}

void draw_player(SDL_Renderer *r, const Player *p, const Level *level,
                 float cam_x, float oy, bool hacking, float hacking_time,
                 float land_squash)
{
  (void)level;
  float cx = p->x - cam_x + (float)PLAYER_W * 0.5f;
  float top = p->y + oy;

  if (p->dying)
  {
    Spot s = spot_at_feet(&SPR_CHUCK_DEAD, cx, top + (float)PLAYER_H,
                          p->facing);
    spot_draw(r, &s, &SPR_CHUCK_DEAD, NULL);
    return;
  }
  if (hacking)
  {
    draw_player_hacking(r, cx, top + (float)PLAYER_H, hacking_time);
    return;
  }
  if (p->crawling)
  {
    draw_player_crawling(r, p, cx, top + (float)PLAYER_CRAWL_H);
    return;
  }
  if (p->on_ladder || p->facade_climbing)
    draw_player_climbing(r, p, cx, top + (float)PLAYER_H);
  else
    draw_player_side(r, p, cx, top + (float)PLAYER_H, land_squash);
}

/* ---- The man with the mop ---------------------------------------------- */

void draw_janitor(SDL_Renderer *r, const Janitor *janitor, const Level *level,
                  float cam_x, float oy)
{
  (void)level;
  float x = janitor->x - cam_x;
  float feet = janitor->y + oy + (float)JANITOR_H;
  int dir = janitor->dir;
  /* Between jobs the mop rides in the bucket; at a job it is in his hands. */
  bool on_the_move = janitor->activity == JANITOR_WALK;
  bool walking = on_the_move && fabsf(janitor->vx) > 2.0f;
  bool mopping = janitor->activity == JANITOR_MOP;

  /* The wet floor he leaves: a dash of cyan on the boards, going over to blue
     as it dries and then out a piece at a time. Presentation only — nothing
     about friction reads it. */
  for (int i = 0; i < JANITOR_WET_SPOTS; ++i)
  {
    const JanitorWetSpot *spot = &janitor->wet_spots[i];
    if (!spot->active || spot->life <= 0.0f)
      continue;
    float wet = spot->life / JANITOR_WET_LIFETIME;
    BbcColour c = wet > 0.5f ? BBC_CYAN : BBC_BLUE;
    int cells = wet > 0.75f ? 5 : (wet > 0.4f ? 4 : (wet > 0.15f ? 3 : 2));
    float sx = spot->x - cam_x - 10.0f;
    float sy = spot->y + oy - (float)BBC_PX_H;
    for (int k = 0; k < 5; ++k)
      if ((k * 2 + 1) % 5 < cells)
        bbc_plot(r, c, sx + (float)(k * BBC_PX_W), sy);
  }

  /* The cart trails him on its clear side, the push bar at his end. During a
     turn it stays where it was until there is room behind him again. */
  int cd = janitor->cart_dir > 0 ? 1 : -1;
  float cart_left = cd > 0 ? x - 25.0f : x + (float)JANITOR_W + 3.0f;
  const BbcSprite *cart_frame =
      on_the_move ? &SPR_JANITOR_CART : &SPR_JANITOR_CART_EMPTY;
  Spot cart = spot_at_feet(cart_frame, cart_left + 11.5f, feet, cd);
  spot_draw(r, &cart, cart_frame, NULL);

  /* Mopping swings the head out and draws it back; a pause is a man leaning
     on the pole; a walk is a walk, and a turn in the middle of one stands him
     on the passing frame rather than handing him the mop. */
  const BbcSprite *frame;
  if (mopping)
    frame = blink(janitor->anim_time, 0.9f) ? &SPR_JANITOR_MOP1
                                             : &SPR_JANITOR_MOP2;
  else if (walking)
    frame = stride_frame(janitor->x, dir, 9.0f, 2) == 0 ? &SPR_JANITOR_WALK1
                                                        : &SPR_JANITOR_WALK2;
  else if (on_the_move)
    frame = &SPR_JANITOR_WALK2;
  else
    frame = &SPR_JANITOR_PAUSE;
  Spot s = spot_at_feet(frame, x + (float)JANITOR_W * 0.5f, feet, dir);
  spot_draw(r, &s, frame, NULL);
}

/* ---- Staff and visitors ------------------------------------------------- */

/*
 * A civilian getting out of the building. The pose carries the whole read at
 * this size: a panicked run is both hands flung up, where a guard's walk is
 * level; the startle is the hands at the face; a trip is a knee on the floor.
 * The visitor runs with his case. At the doors they dissolve — the doorway is
 * the world's, so a figure leaving through it goes out pixel by pixel rather
 * than shrinking or stepping out of a frame it would have to agree with.
 */
void draw_civilian(SDL_Renderer *r, const Civilian *civilian,
                   const Level *level, float cam_x, float oy)
{
  (void)level;
  if (civilian->activity == CIVILIAN_GONE || civilian->fade <= 0.0f)
    return;

  int variant = civilian->variant % CIVILIAN_VARIANTS;
  if (variant < 0)
    variant += CIVILIAN_VARIANTS;
  const BbcColour *paint = BBC_CIVILIAN_PAINT[variant % BBC_CIVILIAN_PAINTS];
  bool carries_case = variant == 2;
  const BbcSprite *frame;

  switch (civilian->activity)
  {
  case CIVILIAN_STUMBLING:
    frame = &SPR_CIVILIAN_STUMBLE;
    break;
  case CIVILIAN_FLEEING:
  {
    int f = stride_frame(civilian->x, civilian->dir, 10.0f, 2);
    if (carries_case)
      frame = f == 0 ? &SPR_CIVILIAN_CASE1 : &SPR_CIVILIAN_CASE2;
    else
      frame = f == 0 ? &SPR_CIVILIAN_RUN1 : &SPR_CIVILIAN_RUN2;
    break;
  }
  case CIVILIAN_STARTLED:
  case CIVILIAN_GONE:
  default:
    frame = &SPR_CIVILIAN_STAND;
    break;
  }

  Spot s = spot_at_feet(frame,
                        civilian->x - cam_x + (float)CIVILIAN_W * 0.5f,
                        civilian->y + oy + (float)CIVILIAN_H, civilian->dir);
  if (civilian->fade < 1.0f)
    spot_dissolve(r, &s, frame, paint, civilian->fade);
  else
    spot_draw(r, &s, frame, paint);
}

/*
 * The desk, staffed. On the ambient-staff layer with the janitor and the
 * civilians, so the counter passes in front of her and the post reads as
 * being behind the desk. Standing to the post with the headset on, walking an
 * errand with the folder tucked, and reading it out on the floor — or over
 * her shoulder at the desk, glancing. Blue and brass, so she belongs to the
 * lobby rather than reading as another guard.
 */
void draw_receptionist(SDL_Renderer *r, const Receptionist *rec,
                       const Level *level, float cam_x, float oy)
{
  (void)level;
  bool walking = rec->activity == RECEPTIONIST_WALK && fabsf(rec->vx) > 2.0f;
  bool reading = rec->activity == RECEPTIONIST_ERRAND || rec->glancing;
  const BbcSprite *frame;

  if (walking)
    frame = stride_frame(rec->x, rec->dir, 9.0f, 2) == 0
                ? &SPR_RECEPTIONIST_WALK1
                : &SPR_RECEPTIONIST_WALK2;
  else if (reading)
    frame = &SPR_RECEPTIONIST_READ;
  else
    frame = &SPR_RECEPTIONIST_STAND;

  Spot s = spot_at_feet(frame, rec->x - cam_x + (float)RECEPTIONIST_W * 0.5f,
                        rec->y + oy + (float)RECEPTIONIST_H, rec->dir);
  spot_draw(r, &s, frame, NULL);
}

/* ---- Guards ------------------------------------------------------------ */

/*
 * The glare over anything a flash charge has just taken the eyes off: stars
 * wheeling round the head, more of them the fresher the flash, and for the
 * first beat the whole figure burnt white.
 *
 * It earns its place because a blinded figure is a figure standing still,
 * which is also what one that has merely lost you looks like — and those two
 * mean opposite things about whether it is safe to walk past.
 */
static void dazzle_stars(SDL_Renderer *r, float cx, float cy, float radius,
                         float timer, float t)
{
  if (timer <= 0.0f)
    return;
  float dazzle = fminf(1.0f, timer / FLASH_BLIND_TIME);
  int stars = 2 + (int)(dazzle * 3.0f);
  for (int i = 0; i < stars; ++i)
  {
    float a = t * 5.0f + (float)i * 6.2831853f / (float)stars;
    float sx = cx + cosf(a) * radius;
    float sy = cy + sinf(a) * radius * 0.5f;
    bbc_plot(r, (i & 1) ? BBC_YELLOW : BBC_WHITE, sx, sy);
  }
}

/* True for the first beat of a fresh flash: the figure is drawn solid white,
   which is what a flash in the face does to a picture on a BBC. */
static bool just_flashed(float timer)
{
  return timer > FLASH_BLIND_TIME * 0.88f;
}

/* One pip for each round *he* can take, green for those left and red for
   those already spent: a heavy has twice a guard's, so his are stacked in two
   rows of three over his helmet, which says "twice the man" before a single
   round is fired. */
static void draw_health_pips(SDL_Renderer *r, float cx, float top, int hp,
                             int full)
{
  int per_row = full > ENEMY_HP ? ENEMY_HP : full;
  int rows = (full + per_row - 1) / per_row;
  float row_w = (float)((per_row * 2 - 1) * BBC_PX_W);
  float left = bbc_snap_x(cx - row_w * 0.5f + (float)BBC_PX_W * 0.5f);
  for (int i = 0; i < full; ++i)
  {
    int row = i / per_row;
    int col = i % per_row;
    float py = top - (float)((rows - row) * 2 * BBC_PX_H);
    bbc_plot(r, i < hp ? BBC_GREEN : BBC_RED,
             left + (float)(col * 2 * BBC_PX_W), py);
  }
}

void draw_enemy(SDL_Renderer *r, const Enemy *e, const Level *level,
                float cam_x, float oy)
{
  (void)level;
  float cx = e->x - cam_x + (float)ENEMY_W * 0.5f;
  float feet = e->y + oy + (float)ENEMY_H;
  bool heavy = e->kind == ENEMY_KIND_HEAVY;
  /* Wounded reads off how much of *his own* health is left rather than off a
     fixed three: read against ENEMY_HP a heavy went on looking untouched for
     the first three rounds. */
  int full = enemy_kind_hp(e->kind);
  bool hurt = e->hp < full;
  const BbcColour *paint = heavy ? (hurt ? BBC_HEAVY_HURT_PAINT : BBC_HEAVY_PAINT)
                                 : (hurt ? BBC_GUARD_HURT_PAINT : BBC_GUARD_PAINT);
  bool aiming = e->aim_timer > 0.0f || e->recoil_timer > 0.0f;
  bool using_alarm = e->raising_alarm && e->alarm_use_timer > 0.0f;
  /* Standing still and speaking to nobody: a call in on the crew's own net,
     which has to read differently from a chat — the chat is two men who have
     stopped watching the corridor, and this is one man who has not. */
  bool on_radio = enemy_on_radio(e) && !e->climbing;
  bool moving = fabsf(e->vx) > 2.0f && !aiming && !e->talking;
  int dir = e->dir;
  const BbcSprite *frame;

  if (e->climbing)
  {
    /* From behind on the rungs, the stroke mirrored by the height climbed. */
    frame = &SPR_GUARD_CLIMB;
    dir = stride_frame(e->y, 1, 12.0f, 2) == 0 ? 1 : -1;
  }
  else if (aiming)
    frame = e->aim_vdir < 0   ? &SPR_GUARD_AIM_UP
            : e->aim_vdir > 0 ? &SPR_GUARD_AIM_DOWN
                              : &SPR_GUARD_AIM;
  else if (using_alarm)
    frame = &SPR_GUARD_REACH;
  else if (on_radio)
    frame = &SPR_GUARD_RADIO;
  else if (e->talking)
    frame = blink(e->anim_time, 0.7f) ? &SPR_GUARD_TALK : &SPR_GUARD_TALK2;
  else if (e->blind_timer > 0.0f)
    frame = &SPR_GUARD_DAZZLED;
  else if (moving)
  {
    static const BbcSprite *const PATROL[4] = {
        &SPR_GUARD_WALK1, &SPR_GUARD_STAND, &SPR_GUARD_WALK2,
        &SPR_GUARD_STAND};
    frame = PATROL[stride_frame(e->x, dir, 8.0f, 4)];
  }
  else
    frame = &SPR_GUARD_STAND;

  Spot s = spot_at_feet(frame, cx, feet, dir);

  if (just_flashed(e->blind_timer) && !e->climbing)
  {
    bbc_sprite_solid(r, frame, s.x, s.y, s.dir, BBC_WHITE);
  }
  else
  {
    /* The heavy's helmet and pauldrons go down first, so his own frame lands
       on top of them and only the extra width shows: the outline of a man
       wider at the head and the shoulders than any guard. */
    if (heavy)
    {
      if (!e->climbing)
        spot_overlay(r, &s, &SPR_HEAVY_HELM, 0, 0, paint);
      spot_overlay(r, &s, &SPR_HEAVY_PLATE, 0, HEAVY_PLATE_ROW, paint);
    }
    spot_draw(r, &s, frame, paint);
  }

  if (aiming && !e->climbing)
  {
    /* The aim is the telegraph, so it points where the round is going to go
       — along the corridor, up the column or down it — and the muzzle blinks
       red while he holds it there. Then the flash, at the end of the rifle. */
    int mc = e->aim_vdir == 0 ? 8 : 7;
    int mr = e->aim_vdir < 0 ? -1 : (e->aim_vdir > 0 ? 16 : 6);
    if (e->recoil_timer > PLAYER_MUZZLE_FLASH_TIME)
    {
      if (e->aim_vdir == 0)
        muzzle_burst(r, &s, 8, 6, &SPR_MUZZLE_FLASH_RIFLE);
      else
        muzzle_star(r, &s, 7, e->aim_vdir < 0 ? -3 : 18);
    }
    else if (e->aim_timer > 0.0f && blink(e->aim_timer, 0.125f))
    {
      spot_plot(r, &s, mc, mr, BBC_RED);
    }
  }

  if (e->blind_timer > 0.0f)
    dazzle_stars(r, cx, s.y + 6.0f, 16.0f, e->blind_timer, e->anim_time);

  draw_health_pips(r, cx, s.y - (float)BBC_PX_H, e->hp, full);

  if (on_radio)
  {
    /* Not a speech bubble: nobody in the room is being spoken to. The lamp
       on the handset keys cyan, and the words leave off the whip as two
       pixels walking outward. */
    if (blink(e->anim_time, 0.33f))
      spot_plot(r, &s, 6, 5, BBC_CYAN);
    int step = stride_frame(e->anim_time * 6.0f, 1, 1.0f, 3);
    spot_plot(r, &s, 8 + step, -1 - step / 2, BBC_CYAN);
    spot_plot(r, &s, 8 + step, 1 + step / 2, BBC_CYAN);
  }
  else if (e->talking)
  {
    /* A bubble over the pair of them with the dots of a conversation in it,
       one dot hopping: two men who are not looking down the corridor. */
    float bx = bbc_snap_x(cx - 14.0f);
    float by = s.y - (float)(9 * BBC_PX_H);
    bbc_fill(r, BBC_WHITE, bx, by, 7.0f * BBC_PX_W, 3.0f * BBC_PX_H);
    bbc_plot(r, BBC_WHITE, bx + 3.0f * BBC_PX_W, by + 3.0f * BBC_PX_H);
    int hop = stride_frame(e->anim_time * 3.0f, 1, 1.0f, 3);
    for (int dot = 0; dot < 3; ++dot)
      bbc_plot(r, BBC_BLACK, bx + (float)((1 + dot * 2) * BBC_PX_W),
               by + (float)((dot == hop ? 0 : 1) * BBC_PX_H));
  }
}

/*
 * A guard who is down, lying where he fell.
 *
 * He has to be drawn, and the reason is a rule rather than a flourish: a calm
 * guard who sees a fallen comrade walks over to look and often sprints for
 * the nearest alarm (`update_body_discovery`). With nothing on the floor the
 * player would watch a man cross the room to an empty patch of carpet and
 * wake the building. So: the same man, on his back, helmet at the far end and
 * the rifle gone from his hands, and the visor dark — the red pixel is what
 * says "enemy" across a room, and a body must not say it.
 */
void draw_downed_enemy(SDL_Renderer *r, const Enemy *e, const Level *level,
                       float cam_x, float oy)
{
  (void)level;
  bool heavy = e->kind == ENEMY_KIND_HEAVY;
  Spot s = spot_at_feet(&SPR_GUARD_DOWN, e->x - cam_x + (float)ENEMY_W * 0.5f,
                        e->y + oy + (float)ENEMY_H, e->dir);
  spot_draw(r, &s, &SPR_GUARD_DOWN, heavy ? BBC_HEAVY_PAINT : BBC_GUARD_PAINT);
}

/* ---- The dog ----------------------------------------------------------- */

/* Where the eye is in each frame, so the chase can light it red. */
typedef struct
{
  const BbcSprite *frame;
  signed char col, row;
} DogEye;

static const DogEye DOG_EYES[] = {
    {&SPR_DOG_STAND, 6, 2}, {&SPR_DOG_RUN1, 6, 3}, {&SPR_DOG_RUN2, 6, 3},
    {&SPR_DOG_BITE, 6, 3},  {&SPR_DOG_GROWL, 6, 5},
};

/*
 * A working dog, and it has to read as one at eight pixels by eight: the line
 * of the back, a head and a muzzle forward, the tail up behind. It gallops in
 * two frames, stretched and gathered, chosen by the ground it covers. The
 * windup before a bite is its own drawing — down on the haunches, head low,
 * teeth out — because the crouch is the whole of the player's warning, and the
 * bite is the lunge. On the chase, and through the windup and the bite, its
 * eye lights yellow in the red head: the dark eye of a dog at its handler's
 * heel is the calm one.
 */
void draw_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
              float cam_x, float oy)
{
  (void)level;
  bool moving = fabsf(dog->vx) > 4.0f;
  bool chase = dog->state == DOG_CHASE;
  bool biting = dog->attack_timer > 0.0f;
  bool winding = dog->bite_windup > 0.0f || dog->bite_ready;
  const BbcSprite *frame;

  if (biting)
    frame = &SPR_DOG_BITE;
  else if (winding)
    frame = &SPR_DOG_GROWL;
  else if (moving && dog->blind_timer <= 0.0f)
    frame = stride_frame(dog->x, dog->dir, chase ? 10.0f : 7.0f, 2) == 0
                ? &SPR_DOG_RUN1
                : &SPR_DOG_RUN2;
  else
    frame = &SPR_DOG_STAND;

  float cx = dog->x - cam_x + (float)DOG_W * 0.5f;
  Spot s = spot_at_feet(frame, cx, dog->y + oy + (float)DOG_H, dog->dir);
  if (just_flashed(dog->blind_timer))
    bbc_sprite_solid(r, frame, s.x, s.y, s.dir, BBC_WHITE);
  else
    spot_draw(r, &s, frame, BBC_DOG_PAINT);

  if (chase || winding || biting)
    for (size_t i = 0; i < SDL_arraysize(DOG_EYES); ++i)
      if (DOG_EYES[i].frame == frame)
        spot_plot(r, &s, DOG_EYES[i].col, DOG_EYES[i].row, BBC_YELLOW);

  if (dog->blind_timer > 0.0f)
    dazzle_stars(r, spot_x(&s, 6) + 2.0f, s.y + 2.0f, 11.0f, dog->blind_timer,
                 dog->anim_time);
}

/* The dog, down, on its side along the floor — the same reason as the guard:
   a handler who finds it investigates and may raise the alarm, and the animal
   has to be on the floor for that to be a thing the player saw happen. */
void draw_downed_dog(SDL_Renderer *r, const Dog *dog, const Level *level,
                     float cam_x, float oy)
{
  (void)level;
  Spot s = spot_at_feet(&SPR_DOG_DOWN, dog->x - cam_x + (float)DOG_W * 0.5f,
                        dog->y + oy + (float)DOG_H, dog->dir);
  spot_draw(r, &s, &SPR_DOG_DOWN, BBC_DOG_PAINT);
}

/* ---- What comes off the wall ------------------------------------------- */

void draw_thrown_object(SDL_Renderer *r, const ThrownObject *object,
                        float cam_x, float oy)
{
  /* A quarter turn at a time, which is all the rotation a BBC sprite ever
     had: upright, on its side, upright the other way, and on the other
     side. */
  static const BbcSprite *const UPRIGHT[3] = {
      &SPR_THROWN_POT, &SPR_THROWN_BOTTLE, &SPR_THROWN_BRICK};
  static const BbcSprite *const SIDE[3] = {
      &SPR_THROWN_POT_SIDE, &SPR_THROWN_BOTTLE_SIDE, &SPR_THROWN_BRICK_SIDE};
  int variant = object->variant % 3;
  if (variant < 0)
    variant += 3;
  int quarter = (int)floorf(object->angle / 1.5707963f) & 3;
  const BbcSprite *frame = (quarter & 1) ? SIDE[variant] : UPRIGHT[variant];
  int dir = (object->vx >= 0.0f ? 1 : -1) * (quarter >= 2 ? -1 : 1);
  Spot s = spot_at_feet(frame, object->x - cam_x + THROWN_OBJECT_SIZE * 0.5f,
                        object->y + oy + THROWN_OBJECT_SIZE, dir);
  spot_draw(r, &s, frame, NULL);
}

void draw_bird(SDL_Renderer *r, const Bird *bird, float cam_x, float oy)
{
  /* Both wings beat together: up off the back, then down under the body. */
  bool up = sinf(bird->anim_time * 15.0f) > 0.0f;
  const BbcSprite *frame = up ? &SPR_BIRD_UP : &SPR_BIRD_DOWN;
  Spot s = spot_at_feet(frame, bird->x - cam_x + BIRD_W * 0.5f,
                        bird->y + oy + BIRD_H, bird->vx >= 0.0f ? 1 : -1);
  spot_draw(r, &s, frame, NULL);
}
