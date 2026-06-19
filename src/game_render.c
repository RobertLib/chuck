#include "game.h"
#include "bbc.h"
#include "bbc_cast.h"

#include <SDL3/SDL.h>
#include <math.h>

#include "chase_render.h"
#include "crew.h"
#include "gameplay_ai.h"
#include "gameplay_interaction.h"
#include "gameplay_world.h"
#include "run_tally.h"
#include "screenshot.h"
#include "world_render.h"

#ifdef CHUCK_DEBUG
#include "embedded_levels.h"
#endif

/*
 * Everything drawn over the world, and the screens that have no world.
 *
 * The sector itself is [world_render.c](world_render.c)'s; this file is the
 * status strip along the top of it, the crew's net, the prompts, the cards a
 * sector or a run ends on, the pause and options sheets, and the roll of names.
 * All of it is drawn the way a BBC Micro game of the period drew its panels,
 * through [bbc.h](bbc.h) and nothing else: black plates, borders made of BBC
 * pixels and doubled where a surface matters, labels in a colour, values in
 * another, the highlight in inverse video and anything urgent flashing. There
 * is no alpha anywhere in it, because the machine had none — a scene behind a
 * menu is stippled rather than dimmed, and a line fading out steps down the
 * palette the way a BBC fade did. See the style guide at the top of
 * [docs/art-and-audio.md](../docs/art-and-audio.md).
 *
 * Every layout number the suite holds — the options sheet's rows and caps, the
 * pause sheet's plate, the tallies' columns, the HUD's carried row — is read
 * from the header that owns it, and the panels are laid out from those exactly
 * as they were before the screens were redrawn.
 */

/* The interface's own two colours: a label names a field and a value is what
 * is in it. Cyan is technology on this screen and everywhere else, which is
 * what a status strip is; yellow is the thing worth reading. */
#define INK_LABEL BBC_CYAN
#define INK_VALUE BBC_YELLOW

void game_get_view_size(Game *game, int *out_w, int *out_h)
{
  int lw = 0, lh = 0;
  SDL_RendererLogicalPresentation mode;
  if (SDL_GetRenderLogicalPresentation(game->platform.renderer, &lw, &lh, &mode) && lw > 0 && lh > 0)
  {
    *out_w = lw;
    *out_h = lh;
    return;
  }
  SDL_GetWindowSize(game->platform.window, out_w, out_h);
}

/*
 * Called before anything in this file draws over a frame another renderer has
 * started. The world is free to re-point the palette — the lights going red is
 * one VDU 19 — and to anchor its dither to the camera, and neither must reach
 * the panels: a status strip whose hearts turn with the room's lighting is a
 * status strip that has stopped reporting.
 */
static void begin_overlay(void)
{
  bbc_palette_reset();
  bbc_set_pattern_origin(0.0f, 0.0f);
}

static void draw_text_centered(Game *game, float center_y, float scale,
                               BbcColour c, const char *text)
{
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  float text_w = bbc_text_width(text, scale);
  float text_h = (float)BBC_FONT_CELL * scale;
  bbc_text(game->platform.renderer, ((float)win_w - text_w) * 0.5f,
           center_y - text_h * 0.5f, scale, c, text);
}

/*
 * Inverse video on the grid: a bar half a character cell wider than the text on
 * each side and one BBC row taller above and below, and the text in `ink` on
 * it. `bbc_text_inverse` floors both of the bar's edges, which leaves a label
 * that does not start on a BBC pixel with six pixels of bar on its left and two
 * on its right; this one rounds the right edge out, so the bar sits round the
 * word the same distance on both sides. And it is only a row deeper than the
 * type, so the sentence a row carries under its label is not touched by it.
 */
static void draw_inverse(SDL_Renderer *r, float x, float y, float scale,
                         BbcColour bar, BbcColour ink, const char *text)
{
  float s = scale < 1.5f ? 1.0f : floorf(scale + 0.5f);
  float x0 = bbc_snap_x(x - 4.0f * s);
  float x1 = bbc_snap_x(x + bbc_text_width(text, scale) + 4.0f * s +
                        (float)(BBC_PX_W - 1));
  float y0 = bbc_snap_y(y - 2.0f);
  float y1 = bbc_snap_y(y + (float)BBC_FONT_CELL * s + 2.0f + 1.0f);
  bbc_fill(r, bar, x0, y0, x1 - x0, y1 - y0);
  bbc_text(r, x, y, scale, ink, text);
}

/*
 * Text built out of BBC pixels rather than drawn in the system font: each pixel
 * of the glyph is one BBC pixel across and `rows` BBC rows down. This is how a
 * BBC game put a number on the screen bigger than the font goes — the face read
 * out of the character set and replotted in blocks — and it is the only way
 * past scale four that stays on the grid. Three rows a pixel makes a digit
 * 32 wide and 48 tall, the height the continue countdown has always had.
 */
static float block_text_width(const char *text)
{
  return (float)SDL_strlen(text) * (float)(BBC_FONT_CELL * BBC_PX_W);
}

static void draw_block_text(SDL_Renderer *r, float x, float y, int rows,
                            BbcColour c, const char *text)
{
  float px_h = (float)(rows * BBC_PX_H);
  int i = 0;
  for (const unsigned char *ch = (const unsigned char *)text; *ch != '\0';
       ++ch, ++i)
  {
    float cx = x + (float)(i * BBC_FONT_CELL * BBC_PX_W);
    for (int row = 0; row < BBC_FONT_CELL; ++row)
    {
      /* Runs rather than single blocks: a stroke is mostly runs. */
      int col = 0;
      while (col < BBC_FONT_CELL)
      {
        if (!bbc_font_pixel(*ch, col, row))
        {
          ++col;
          continue;
        }
        int start = col;
        while (col < BBC_FONT_CELL && bbc_font_pixel(*ch, col, row))
          ++col;
        bbc_block(r, c, cx + (float)(start * BBC_PX_W), y + (float)row * px_h,
                  col - start, rows);
      }
    }
  }
}

/* ---- The status strip ------------------------------------------------- */

/*
 * The strip's own grid. Forty logical pixels is twenty BBC rows, and every
 * field is laid out on the same three lines of it: the labels along the top,
 * the icons — hearts, cartridges, the carried row — on `HUD_CARRY_ROW_Y`, and a
 * double-height value (SECTOR, SCORE) or a chip between them and the rule. The
 * bottom three rows are the double rule — a line, a row of black, a line — that
 * closes the strip off from the frame below, which is how a BBC game told its
 * panel from its playfield.
 */
#define HUD_LABEL_Y 4.0f
#define HUD_VALUE_Y 16.0f
#define HUD_SMALL_Y 20.0f
#define HUD_CHIP_Y 16.0f
#define HUD_CHIP_H 16.0f
#define HUD_RULE_INNER_Y 34.0f
#define HUD_RULE_OUTER_Y 38.0f
#define HUD_HEART_PITCH 24.0f

static void draw_hud_strip(SDL_Renderer *r, float w, BbcColour inner,
                           BbcColour outer, BbcColour spine)
{
  bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, w, (float)HUD_HEIGHT);
  bbc_fill(r, inner, 0.0f, HUD_RULE_INNER_Y, w, 2.0f);
  bbc_fill(r, outer, 0.0f, HUD_RULE_OUTER_Y, w, 2.0f);
  /* The spine down the left edge: one BBC pixel of the strip's own colour. */
  bbc_fill(r, spine, 0.0f, 0.0f, 4.0f, HUD_RULE_INNER_Y);
}

/* An upright between two fields, dotted, in the blue of things that are not
 * the point: a divider is there to be read past. */
static void draw_hud_separator(SDL_Renderer *r, float x)
{
  bbc_stipple(r, BBC_BLUE, BBC_DITHER_ROWS, x, 2.0f, 4.0f, 30.0f);
}

/*
 * What Chuck is carrying, in one place, because both strips ask it.
 *
 * They used to answer it separately and both answers were wrong. The wall drew
 * the flash charge at 706, inside a cartridge run that ends at 717, so a
 * climber carrying one was shown a four-round clip on the one readout whose own
 * comment promises "every pip is always lit". The floor did not draw the charge
 * at all — on the twelve sectors where it is the only one of the three that can
 * actually be thrown, and where [gameplay.md](../docs/gameplay.md) leans on this
 * row by name to argue that a pickup which does not arm itself is still
 * visible. Two strips, one question, two answers: this codebase's most reliable
 * smell, and here it was pointing at both of them.
 *
 * The slots and the ink widths are in [game_config.h](game_config.h) where the
 * suite can reach them; see the note there for why a renderer's own literals
 * are a layout nothing can measure. The glyphs are the same three icons the
 * floor and the manual draw them with.
 */
static void draw_hud_carried(SDL_Renderer *r, const Player *p, float x,
                             float y)
{
  if (p->flashbangs > 0)
    bbc_sprite(r, &SPR_ICON_FLASH, x, y, 1, NULL);
  if (p->grenades > 0)
    bbc_sprite(r, &SPR_ICON_GRENADE, x + (float)HUD_CARRY_SLOT_W, y, 1, NULL);
  if (p->bazooka_rockets > 0)
    bbc_sprite(r, &SPR_ICON_ROCKET, x + 2.0f * (float)HUD_CARRY_SLOT_W,
               y + (float)HUD_CARRY_ROCKET_DROP, 1, NULL);

  /* And a rule under the one the label is talking about.
   *
   * The label names the active weapon and the cartridges beside it are always
   * the pistol's, so `BAZOOKA` over a full clip read as six rockets. This is
   * what says which number the word means. Drawn only when the slot has
   * something in it, and only for the three that live in this row: the pistol
   * and the knife are the clip and the hand, and BOLTS is a pocketful with no
   * count anywhere — see `player_weapon_label`.
   *
   * The slot's own ink rather than the whole slot, so it sits under the glyph
   * rather than under the gap after it. See `HUD_CARRY_MARK_DROP`. The ink
   * widths there are measured from `x - 1`, so the glyph itself is one less. */
  int slot = -1;
  int ink = 0;
  if (p->active_weapon == PLAYER_WEAPON_FLASH && p->flashbangs > 0)
  {
    slot = 0;
    ink = HUD_FLASH_INK_W;
  }
  else if (p->active_weapon == PLAYER_WEAPON_GRENADE && p->grenades > 0)
  {
    slot = 1;
    ink = HUD_GRENADE_INK_W;
  }
  else if (p->active_weapon == PLAYER_WEAPON_BAZOOKA && p->bazooka_rockets > 0)
  {
    slot = 2;
    ink = HUD_ROCKET_INK_W;
  }
  if (slot >= 0)
    bbc_fill(r, INK_VALUE, x + (float)slot * (float)HUD_CARRY_SLOT_W,
             y + (float)HUD_CARRY_MARK_DROP, (float)(ink - 1),
             (float)HUD_CARRY_MARK_H);
}

/* The clip, a cartridge a slot, spent ones kept in place as empty sockets so
 * the run never closes up under the player's eye. The same two sprites the
 * manual teaches them with. */
static void draw_hud_clip(SDL_Renderer *r, float x, int bullets)
{
  for (int i = 0; i < MAX_AMMO; ++i)
    bbc_sprite(r, i < bullets ? &SPR_ICON_ROUND : &SPR_ICON_ROUND_SPENT,
               x + (float)i * (float)HUD_AMMO_PIP_PITCH,
               (float)HUD_CARRY_ROW_Y, 1, NULL);
}

/*
 * Hearts are the hits left in this life; the counter beside them is the lives
 * left in the run. Empty sockets keep the maximum readable. The block is sized
 * for the assist row of five plus the widest counter, which is the three-cell
 * "1UP" and not the "x9" it replaces: five hearts at `HUD_HEART_PITCH` and
 * three cells is 144, which is what both strips leave for it.
 *
 * The console counts the same heart the manual teaches and the outro hands
 * over — one sprite, `SPR_ICON_HEART`, drawn from bbc_cast.c on every screen.
 */
static void draw_hud_vital(Game *game, float x)
{
  SDL_Renderer *r = game->platform.renderer;
  bbc_text(r, x, HUD_LABEL_Y, 1.0f, INK_LABEL, "VITAL");
  int max_hp = gameplay_player_max_hp(&game->gameplay);
  for (int i = 0; i < max_hp; ++i)
    bbc_sprite(r,
               i < game->gameplay.player.hp ? &SPR_ICON_HEART
                                            : &SPR_ICON_HEART_EMPTY,
               x + (float)i * HUD_HEART_PITCH, (float)HUD_CARRY_ROW_Y, 1,
               NULL);
  char life_buf[8];
  SDL_snprintf(life_buf, sizeof(life_buf), "x%d", game->campaign.lives);
  /* Both strips earn extra lives the same way, so both announce them the same
   * way: a score that pays out silently reads as the counter having
   * miscounted. The beat is the timer's own rather than the flash rate's,
   * because it is a short burst that has to land while the player is still
   * looking, not a state that holds. */
  float counter_x = x + (float)max_hp * HUD_HEART_PITCH;
  if (game->presentation.extra_life_timer > 0.0f &&
      fmodf(game->presentation.extra_life_timer, 0.3f) > 0.15f)
    bbc_text(r, counter_x, HUD_SMALL_Y, 1.0f, BBC_GREEN, "1UP");
  else
    bbc_text(r, counter_x, HUD_SMALL_Y, 1.0f, BBC_WHITE, life_buf);
}

static const char *player_weapon_label(PlayerWeapon weapon)
{
  switch (weapon)
  {
  case PLAYER_WEAPON_PISTOL:
    return "PISTOL";
  case PLAYER_WEAPON_KNIFE:
    return "KNIFE";
  case PLAYER_WEAPON_GRENADE:
    return "GRENADE";
  case PLAYER_WEAPON_BAZOOKA:
    return "BAZOOKA";
  case PLAYER_WEAPON_FLASH:
    return "FLASH";
  case PLAYER_WEAPON_DECOY:
    /* Plural, because there is no count beside it and never will be. The other
       five labels name a thing in the hand; this one names a pocketful. */
    return "BOLTS";
  case PLAYER_WEAPON_COUNT:
    return "NONE";
  }
  return "NONE";
}

/* A wind chevron: one arrowhead of the gauge, facing the way the gust will
 * push. Slot '1' is the gauge's state colour. */
BBC_SPRITE(SPR_HUD_CHEVRON,
    "1..",
    "11.",
    "111",
    "11.",
    "1..");

/*
 * The climb's strip. The same grid as the sector's, the same hearts, the same
 * clip, closed off in the brass of the wall rather than the cyan of the
 * building: yellow over red, the one strip in the game that is outdoors.
 *
 *   FACADE / MOVE ON WALL   8..104
 *   VITAL                 116..260
 *   WIND                  272..372
 *   ALTITUDE + WINDOW nM  384..672
 *   the weapon            684..796
 */
static void render_facade_hud(Game *game, int win_w)
{
  SDL_Renderer *r = game->platform.renderer;
  draw_hud_strip(r, (float)win_w, BBC_RED, BBC_YELLOW, BBC_YELLOW);

  bbc_text(r, 8.0f, HUD_LABEL_Y, 2.0f, BBC_WHITE, "FACADE");
  bbc_text(r, 8.0f, 24.0f, 1.0f, INK_LABEL, "MOVE ON WALL");
  draw_hud_separator(r, 108.0f);

  draw_hud_vital(game, 116.0f);
  draw_hud_separator(r, 264.0f);

  /* Wind gauge. During the warning it shows which way the gust will push;
   * during the gust it also says whether Chuck is currently in its lee.
   *
   * The four states are four colours with the meanings they have everywhere
   * else: blue is nothing happening, yellow is a warning and flashes because it
   * is one, red is the gust that is pushing him and green is the shelter that
   * means it is not. */
  const GameplayState *state = &game->gameplay;
  bool gusting = state->facade_wind_phase == FACADE_WIND_GUSTING;
  bool warning = state->facade_wind_phase == FACADE_WIND_WARNING;
  bbc_text(r, 272.0f, HUD_LABEL_Y, 1.0f, INK_LABEL, "WIND");
  BbcColour wind_colour = gusting ? (state->facade_wind_sheltered ? BBC_GREEN
                                                                  : BBC_RED)
                          : warning ? bbc_flash(BBC_YELLOW)
                                    : BBC_BLUE;
  const char *wind_text = gusting
                              ? (state->facade_wind_sheltered ? "LEE" : "GUST")
                              : warning ? "RISING" : "CALM";
  BbcColour chevron_paint[BBC_SPRITE_SLOTS] = {wind_colour, wind_colour,
                                               wind_colour, wind_colour};
  int dir = state->facade_wind_dir >= 0 ? 1 : -1;
  for (int arrow = 0; arrow < 4; ++arrow)
    bbc_sprite(r, &SPR_HUD_CHEVRON, 272.0f + (float)arrow * 12.0f,
               (float)HUD_CARRY_ROW_Y, dir, chevron_paint);
  /* CALM in the gauge's own blue would be a word nobody can read on black, so
   * the quiet state is spelled in white and only the chevrons go dark. */
  bbc_text(r, 324.0f, HUD_SMALL_Y, 1.0f,
           (gusting || warning) ? wind_colour : BBC_WHITE, wind_text);
  draw_hud_separator(r, 376.0f);

  float start_y = game->gameplay.level.map.start_y;
  float target_y = game->gameplay.level.map.window_row * (float)TILE_SIZE;
  float distance = start_y - target_y;
  float climbed = start_y - game->gameplay.player.y;
  float progress = distance > 0.0f ? climbed / distance : 1.0f;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;
  bbc_text(r, 384.0f, HUD_LABEL_Y, 1.0f, INK_LABEL, "ALTITUDE");

  /* The gauge: a well with a blue rim, filled in cyan from the left. Its inside
   * is 280 wide so a tenth of the climb is exactly seven BBC pixels, and the
   * graduations land on the grid rather than wandering a pixel either way of
   * it. */
  const float well_x = 384.0f, well_y = 16.0f, well_w = 288.0f, well_h = 16.0f;
  const float bar_x = well_x + 4.0f, bar_y = well_y + 2.0f;
  const float bar_w = well_w - 8.0f, bar_h = well_h - 4.0f;
  bbc_frame(r, BBC_BLUE, well_x, well_y, well_w, well_h);
  float filled = bbc_snap_x(bar_w * progress);
  if (filled > 0.0f)
    bbc_fill(r, BBC_CYAN, bar_x, bar_y, filled, bar_h);
  /* A graduation every tenth of the climb, over the fill and the empty well
     alike, so the bar reads as a gauge and a climber can count what is left. */
  for (int tick = 1; tick < 10; ++tick)
    bbc_block(r, BBC_BLUE, bar_x + bar_w * (float)tick / 10.0f,
              bar_y + bar_h - 4.0f, 1, 2);
  /* And the climber himself: the leading edge of the fill, in white. */
  if (filled > 0.0f)
    bbc_fill(r, BBC_WHITE, bar_x + filled - 4.0f, bar_y, 4.0f, bar_h);
  /* Where a lost life puts him back on the wall. The bar is the only thing
   * that can say it, and a climber deciding whether to take the next gust
   * head-on is asking exactly this question. Taller than the well, so it reads
   * over the fill as well as over the empty part. */
  if (game->gameplay.facade_has_checkpoint && distance > 0.0f)
  {
    float banked = (start_y - game->gameplay.facade_checkpoint_y) / distance;
    if (banked < 0.0f)
      banked = 0.0f;
    if (banked > 1.0f)
      banked = 1.0f;
    bbc_fill(r, BBC_YELLOW, bar_x + bbc_snap_x(bar_w * banked) - 4.0f,
             well_y - 2.0f, 4.0f, well_h + 4.0f);
  }

  /* How far there is still to go, on the gauge's own top line and flush with
   * its right end, so the number and the bar read as one instrument. */
  int tiles_remaining = (int)ceilf(fabsf(game->gameplay.player.y - target_y) /
                                   TILE_SIZE);
  char remaining[24];
  SDL_snprintf(remaining, sizeof(remaining), "WINDOW %02dM", tiles_remaining);
  bbc_text(r, well_x + well_w - bbc_text_width(remaining, 1.0f), HUD_LABEL_Y,
           1.0f, INK_VALUE, remaining);
  draw_hud_separator(r, 676.0f);

  /* What Chuck is carrying up the wall is what he will have inside — drawn in
   * the same glyphs the sector draws it in, because the player is asked to
   * recognise a cartridge and a grenade everywhere else in the game and a
   * climb spelling them `A6 G0 R0` is the one screen teaching a private
   * shorthand. Nothing here is usable on the wall, so the name is set as a
   * label rather than as a live readout: it is a note about the next sector,
   * not a thing to reach for.
   *
   * Two of the three fields are constants and that is by construction rather
   * than by accident, which is worth saying so nobody reads them as tracking
   * anything: the label is always PISTOL and every pip is always lit, because
   * `player_begin_sector` resets the weapon in hand and hands over a full clip
   * at the doorway, `update_facade_playing` clears `switch_weapon` for the
   * whole climb, and nothing up here can fire. They stay because the note is
   * still true — that *is* what the next sector opens with, and a new player
   * has no way to know it — but the carried row below is the only part of this
   * that a climb can change. It is also why a climb lays out no `G`: see
   * `test_no_climb_lays_out_a_pickup_it_cannot_use`.
   *
   * "Every pip is always lit" is a promise this block used to break by itself.
   * The flash charge was drawn at 706, on top of the last two of the six, so a
   * climber carrying one read a four-round clip off the readout that says
   * otherwise — and no gate could see it, because nothing headless has ever
   * carried a charge onto a wall. It is `draw_hud_carried` now, clear of the
   * cartridges and shared with the strip inside. */
  bbc_text(r, (float)HUD_FACADE_AMMO_X, HUD_LABEL_Y, 1.0f, INK_LABEL,
           player_weapon_label(game->gameplay.player.active_weapon));
  draw_hud_clip(r, (float)HUD_FACADE_AMMO_X, game->gameplay.player.bullets);
  draw_hud_carried(r, &game->gameplay.player, (float)HUD_FACADE_CARRY_X,
                   (float)HUD_CARRY_ROW_Y);
}

/*
 * The sector's strip.
 *
 *   CHUCK / NO BACKUP   12..92
 *   VITAL              104..248
 *   the weapon         260..368   (HUD_SECTOR_*)
 *   ACCESS             376..456
 *   SECTOR             468..516
 *   SCORE              528..640
 *   TRAIL / SECURITY   652..796
 *
 * Every field starts on a BBC pixel and every divider is one wide; the hearts
 * are five-pixel sprites now rather than fourteen-pixel glyphs, which is where
 * the room for the wider VITAL block came from.
 */
static void render_hud(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  begin_overlay();
  if (game->gameplay.level.map.mode == LEVEL_MODE_FACADE)
  {
    render_facade_hud(game, win_w);
    return;
  }

  /* A black console closed off by a double rule, blue over cyan, with the red
   * spine down its left edge that has always marked it as Chuck's. */
  draw_hud_strip(r, (float)win_w, BBC_BLUE, BBC_CYAN, BBC_RED);

  /* Identity block. The wordmark at double height, underlined in the spine's
   * red at its own width — five cells at scale 2 is 80, so the rule stops
   * where the K does rather than running on to the divider. */
  bbc_text(r, 12.0f, HUD_LABEL_Y, 2.0f, BBC_WHITE, "CHUCK");
  bbc_fill(r, BBC_RED, 12.0f, 20.0f, 80.0f, 2.0f);
  bbc_text(r, 12.0f, 24.0f, 1.0f, INK_LABEL, "NO BACKUP");
  draw_hud_separator(r, 96.0f);

  draw_hud_vital(game, 104.0f);
  draw_hud_separator(r, 252.0f);

  /* All carried ammunition remains visible; the label names the weapon that
   * the next attack will actually use, which makes it a value rather than a
   * label, and it is set in the values' yellow. */
  bbc_text(r, (float)HUD_SECTOR_AMMO_X, HUD_LABEL_Y, 1.0f, INK_VALUE,
           player_weapon_label(game->gameplay.player.active_weapon));
  draw_hud_clip(r, (float)HUD_SECTOR_AMMO_X, game->gameplay.player.bullets);
  /* Three slots rather than the two this had: the flash charge is carried on
     twelve of the seventeen sectors and was reported on none of them, which is
     the one place in the game where "a pickup never arms itself" stopped being
     a rule the player could see the consequence of. */
  draw_hud_carried(r, &game->gameplay.player, (float)HUD_SECTOR_CARRY_X,
                   (float)HUD_CARRY_ROW_Y);
  draw_hud_separator(r, (float)HUD_SECTOR_BLOCK_END);

  /* Access status chip with a live LED.
   *
   * It reports the *sector*, not the room the boots are standing in. Entering
   * the restroom swaps the whole simulation, so reading the active one put the
   * WC's own (permanently locked, because it has none) door on the strip: a
   * player who had already found the card watched ACCESS fall back to a
   * blinking red LOCKED for the length of a detour, and go green again on the
   * way out. SECTOR beside it already names the sector rather than the room,
   * and the two have to agree.
   *
   * GRANTED is the one state drawn in inverse video, because it is the one
   * the player has done something to reach and the one they are glancing up
   * to find; LOCKED is the ordinary state of a floor and only its LED moves. */
  const GameplayState *sector =
      game->in_sublevel ? &game->inactive_gameplay : &game->gameplay;
  const float chip_x = 376.0f, chip_w = 80.0f;
  bbc_text(r, chip_x, HUD_LABEL_Y, 1.0f, INK_LABEL, "ACCESS");
  bool blocked = sector->level.map.has_window;
  bool unlocked = sector->level.runtime.exit_unlocked;
  const float led_x = chip_x + 8.0f, led_y = HUD_CHIP_Y + 6.0f;
  const float word_x = chip_x + 16.0f;
  if (blocked)
  {
    bbc_frame(r, BBC_BLUE, chip_x, HUD_CHIP_Y, chip_w, HUD_CHIP_H);
    bbc_block(r, BBC_BLUE, led_x, led_y, 1, 2);
    bbc_text(r, word_x, HUD_SMALL_Y, 1.0f, BBC_WHITE, "BLOCKED");
  }
  else if (unlocked)
  {
    bbc_fill(r, BBC_GREEN, chip_x, HUD_CHIP_Y, chip_w, HUD_CHIP_H);
    bbc_block(r, BBC_WHITE, led_x, led_y, 1, 2);
    bbc_text(r, word_x, HUD_SMALL_Y, 1.0f, BBC_BLACK, "GRANTED");
  }
  else
  {
    bbc_frame(r, BBC_RED, chip_x, HUD_CHIP_Y, chip_w, HUD_CHIP_H);
    bbc_block(r, bbc_flash_pair(BBC_RED, BBC_BLACK), led_x, led_y, 1, 2);
    bbc_text(r, word_x, HUD_SMALL_Y, 1.0f, BBC_RED, "LOCKED");
  }
  draw_hud_separator(r, 460.0f);

  char level_buf[32];
  SDL_snprintf(level_buf, sizeof(level_buf), "%02d", game->campaign.current_level + 1);
  bbc_text(r, 468.0f, HUD_LABEL_Y, 1.0f, INK_LABEL, "SECTOR");
  bbc_text(r, 468.0f, HUD_VALUE_Y, 2.0f, BBC_WHITE, level_buf);
  draw_hud_separator(r, 520.0f);

  /* Score keeps its leading zeros, but only the live digits are lit: the
   * zeros are in the blue of things that are not the point. */
  char score_buf[16];
  SDL_snprintf(score_buf, sizeof(score_buf), "%07d", game->campaign.score);
  bbc_text(r, 528.0f, HUD_LABEL_Y, 1.0f, INK_LABEL, "SCORE");
  int first_digit = 0;
  while (first_digit < 6 && score_buf[first_digit] == '0')
    ++first_digit;
  bbc_text(r, 528.0f, HUD_VALUE_Y, 2.0f, BBC_BLUE, score_buf);
  bbc_text(r, 528.0f + (float)first_digit * 16.0f, HUD_VALUE_Y, 2.0f,
           INK_VALUE, score_buf + first_digit);
  draw_hud_separator(r, 644.0f);

  /* The passive trail meter becomes an unmistakable security readout while
   * the building alarm is active.
   *
   * Off the *sector*, for the reason ACCESS is: the restroom is a room of the
   * building, not a different building, and its own simulation has no alarm in
   * it. Reading the active one meant a sector left ringing went quiet the
   * moment Chuck stepped through the WC door and started ringing again when he
   * came back out — a countdown that pauses when the player hides is the HUD
   * offering a safe room the sector never granted. The timer itself is frozen
   * with the rest of the sector while he is away, so the number he returns to
   * is the number he left. */
  const float meter_x = 652.0f, meter_w = 144.0f;
  float t = game->presentation.render_clock;
  if (gameplay_alarm_active(sector))
  {
    /* The machine's own alarm: logical colour 9, red and cyan alternating at
     * the flash rate, with the count in black across it. It holds red under
     * reduced motion, which is what that switch promises. */
    char alarm_buf[24];
    SDL_snprintf(alarm_buf, sizeof(alarm_buf), "ALERT %02d",
                 (int)ceilf(sector->terminal_alarm_timer));
    bbc_text(r, meter_x, HUD_LABEL_Y, 1.0f, BBC_RED, "SECURITY");
    bbc_fill(r, bbc_flash(BBC_RED), meter_x, HUD_CHIP_Y, meter_w, HUD_CHIP_H);
    bbc_block(r, BBC_BLACK, meter_x + 8.0f, HUD_CHIP_Y + 6.0f, 1, 2);
    bbc_text(r, meter_x + 16.0f, HUD_SMALL_Y, 1.0f, BBC_BLACK, alarm_buf);
  }
  else
  {
    /* A level meter: twelve columns two BBC pixels wide, each a stack of up
     * to four lit segments a row apart with the top one white, the way a real
     * meter is built rather than as a solid bar. The last three are the
     * meter's headroom and stay in the blue of a scale nobody is reaching. */
    bbc_text(r, meter_x, HUD_LABEL_Y, 1.0f, INK_LABEL, "TRAIL");
    const float base = 32.0f;
    for (int i = 0; i < 12; ++i)
    {
      float wave = 0.5f + 0.5f * sinf(t * 2.6f + (float)i * 0.9f);
      int segments = 1 + (int)floorf(wave * 3.0f + 0.5f);
      float bx = meter_x + 4.0f + (float)i * 12.0f;
      bool live = i < 9;
      for (int seg = 0; seg < segments; ++seg)
      {
        BbcColour ink = !live ? BBC_BLUE
                              : seg == segments - 1 ? BBC_WHITE : BBC_CYAN;
        bbc_fill(r, ink, bx, base - 2.0f - 4.0f * (float)seg, 8.0f, 2.0f);
      }
    }
  }
}

/*
 * What the crew just said, printed under the strip.
 *
 * The building has always had a night shift talking to itself — a pose, a
 * bubble of dots and a handset sound — and never once a word of it. This is
 * the words, and it is the only place in the game the plot is told while the
 * player is actually playing rather than watching. It is deliberately a strip
 * and not a speech bubble: at twenty-six pixels across, a guard is not wide
 * enough to hang a sentence off, and a bubble would have to track a man who is
 * about to walk out of frame. The plate stays put and names him instead.
 *
 * The accent is the palette's own semantics rather than five decorative
 * colours: cyan is the handset (technology), red is the alarm (danger), yellow
 * is a voice shouting out of a window on the wall (warning), white is somebody
 * who does not work for Meridian, and two men talking in a room carry no
 * accent at all, because nothing is happening.
 */
static void render_crew_chatter(Game *game, int win_w)
{
  if (game->presentation.chatter_timer <= 0.0f)
    return;
  /* The unlocked-exit banner lands in exactly this band and owns the frame
   * for its two seconds. Two plates arguing over the same rows is worse than
   * losing one overheard line. */
  if (game->presentation.exit_unlocked_timer > 0.0f)
    return;

  SDL_Renderer *r = game->platform.renderer;
  ChatterKind kind = game->presentation.chatter_kind;
  /* Everybody on the crew is named; the people running out of the lobby are
   * not on anybody's docket and are not given one. */
  const char *who = kind == CHATTER_PANIC
                        ? NULL
                        : crew_callsign(game->presentation.chatter_speaker);
  /* The name is settled before the words are, because it decides which words
   * this man is allowed to be the one saying — and so does the hour. The
   * sector is the campaign's, never the restroom's, for the same reason every
   * other field on this strip reads through the sector: a man in the WC two
   * floors under the roof is still two floors under the roof. */
  CrewSituation situation = {game->campaign.current_level + 1,
                             game->campaign.hostiles_down};
  const char *line =
      crew_line_in(kind, game->presentation.chatter_roll, who, &situation);
  if (line == NULL)
    return;

  /*
   * Two men talking in a room is the one kind of line where *nothing is
   * happening*, so it must not carry an accent that means something — and every
   * colour on this machine does. What is wanted is the absence of one: the
   * tick goes the blue of the room behind, the name is spelled in the same
   * white as the words, and the colour says nothing at all. Blue is not used
   * for the name itself because blue type on black is the one combination a
   * BBC screen could not make readable.
   */
  BbcColour accent = BBC_CYAN;
  BbcColour name_ink = BBC_CYAN;
  if (kind == CHATTER_ALARM)
    accent = name_ink = BBC_RED;
  else if (kind == CHATTER_WALL)
    accent = name_ink = BBC_YELLOW;
  else if (kind == CHATTER_PANIC)
    accent = name_ink = BBC_WHITE;
  else if (kind == CHATTER_TALK)
  {
    accent = BBC_BLUE;
    name_ink = BBC_WHITE;
  }

  /* One curve for the whole life of the line: up fast, hold, and out over the
   * last half second. Read off the timer rather than off a wall clock, so it
   * freezes with the sector when the game is paused.
   *
   * The curve is spent as palette steps rather than as an alpha, which is how
   * a BBC faded anything: four rungs, white to cyan to blue to black, and at
   * the bottom rung the plate is not drawn at all. */
  float left = game->presentation.chatter_timer;
  float rising = (CHATTER_HOLD_TIME - left) / 0.16f;
  float falling = left / 0.5f;
  float fade = rising < falling ? rising : falling;
  if (fade > 1.0f)
    fade = 1.0f;
  if (fade < 0.0f)
    fade = 0.0f;
  int steps = (int)((1.0f - fade) * 4.0f);
  if (steps >= 3)
    return;

  char buffer[96];
  if (who != NULL)
    SDL_snprintf(buffer, sizeof(buffer), "%s: %s", who, line);
  else
    SDL_strlcpy(buffer, line, sizeof(buffer));

  const float x = 16.0f;
  const float y = (float)HUD_HEIGHT + 8.0f;
  const float h = 18.0f;
  float w = bbc_text_width(buffer, 1.0f) + 24.0f;
  if (w > (float)win_w - x * 2.0f)
    w = (float)win_w - x * 2.0f;

  bbc_palette_dim(steps);
  bbc_fill(r, BBC_BLACK, x, y, w, h);
  bbc_frame(r, BBC_BLUE, x, y, w, h);
  /* The tick down the left edge is the same mark the report between sectors
   * puts against its intel line: this is the same kind of thing, overheard
   * instead of read afterwards. */
  bbc_fill(r, accent, x, y, 4.0f, h);

  const float text_x = x + 12.0f, text_y = y + 5.0f;
  if (who != NULL)
  {
    bbc_text(r, text_x, text_y, 1.0f, name_ink, who);
    bbc_text(r, text_x + bbc_text_width(who, 1.0f), text_y, 1.0f, BBC_WHITE,
             buffer + SDL_strlen(who));
  }
  else
  {
    bbc_text(r, text_x, text_y, 1.0f, BBC_WHITE, buffer);
  }
  bbc_palette_reset();
}

static void render_interaction_prompt(Game *game, int win_w, int win_h)
{
  bool terminal_available = game->gameplay.terminal_in_range &&
                            !game->gameplay.level.runtime.exit_unlocked;
  bool door_available = gameplay_player_door_index(&game->gameplay) >= 0;
  SublevelDoorAction sublevel_action =
      gameplay_player_sublevel_door_action(&game->gameplay);
  bool sublevel_door_available = sublevel_action != SUBLEVEL_DOOR_NONE;
  /*
   * A body at Chuck's feet, which is the one line here that teaches a mechanic
   * rather than naming a door. Nothing else on screen says a corpse can be
   * moved: there is no icon on it, the HUD has no field for it, and a player
   * who never happens to hold USE while standing on one would finish the
   * campaign without discovering that the rule the guards have been punishing
   * them with all night has an answer.
   */
  bool dragging = game->gameplay.player.dragging;
  bool body_available = gameplay_body_within_reach(&game->gameplay);
  if (game->state != STATE_PLAYING ||
      (!terminal_available && !door_available && !sublevel_door_available &&
       !body_available && !dragging))
  {
    return;
  }

  SDL_Renderer *r = game->platform.renderer;
  float progress = terminal_available
                       ? game->gameplay.terminal_hack_progress /
                             TERMINAL_HACK_TIME
                       : 0.0f;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;

  const PadHints *pad = game_pad_hints(game);
  char buf[64];
  const char *label;
  /*
   * The key this prompt names is whatever the player has USE bound to, not the
   * E it shipped with. These four are the only lines in the game that name a
   * rebindable key while it is being played, and a prompt naming a button the
   * state does not accept is the one thing this file's own rules refuse — the
   * mirror of the ungated E the door key used to answer without being named.
   *
   * An action with no key at all prints "-", which reads as "nothing does
   * this" rather than as a missing word. The sheet says the same thing about
   * the same row, so the two agree.
   */
  const char *use_key =
      keybind_key_name(game->settings.bindings.keys[BIND_USE][0]);
  if (use_key[0] == '\0')
    use_key = keybind_key_name(game->settings.bindings.keys[BIND_USE][1]);
  if (use_key[0] == '\0')
    use_key = "-";
  char key_form[64];
  if (terminal_available && game->gameplay.terminal_hacking)
  {
    int percent = (int)(progress * 100.0f);
    SDL_snprintf(buf, sizeof(buf), "BREACHING SECURITY... %d%%", percent);
    label = buf;
  }
  else if (terminal_available)
  {
    SDL_snprintf(key_form, sizeof(key_form),
                 "HOLD %s TO HACK ACTIVE TERMINAL", use_key);
    label = pad_hint(pad, buf, sizeof(buf), "HOLD $Y TO HACK ACTIVE TERMINAL",
                     key_form);
  }
  else if (dragging)
  {
    /* Named while it is happening as well as before it starts, because letting
       go is a decision too: the body stays exactly where the button was
       released, and that is the whole of how a corpse gets hidden. */
    SDL_snprintf(key_form, sizeof(key_form), "RELEASE %s TO DROP BODY",
                 use_key);
    label = pad_hint(pad, buf, sizeof(buf), "RELEASE $Y TO DROP BODY",
                     key_form);
  }
  else if (body_available)
  {
    SDL_snprintf(key_form, sizeof(key_form), "HOLD %s TO DRAG BODY", use_key);
    label = pad_hint(pad, buf, sizeof(buf), "HOLD $Y TO DRAG BODY", key_form);
  }
  else if (sublevel_action == SUBLEVEL_DOOR_ENTER)
  {
    SDL_snprintf(key_form, sizeof(key_form), "PRESS %s TO ENTER WC", use_key);
    label = pad_hint(pad, buf, sizeof(buf), "PRESS $Y TO ENTER WC", key_form);
  }
  else if (sublevel_action == SUBLEVEL_DOOR_RETURN)
  {
    SDL_snprintf(key_form, sizeof(key_form), "PRESS %s TO LEAVE WC", use_key);
    label = pad_hint(pad, buf, sizeof(buf), "PRESS $Y TO LEAVE WC", key_form);
  }
  else
  {
    SDL_snprintf(key_form, sizeof(key_form), "PRESS %s TO ENTER DOOR",
                 use_key);
    label = pad_hint(pad, buf, sizeof(buf), "PRESS $Y TO ENTER DOOR",
                     key_form);
  }

  /* A black plate with a blue rim and green corner brackets, the targeting
   * frame the prompt has always had, and the words in the green of access.
   * Every edge is on the grid: the plate is the text plus three cells, and the
   * text is a whole number of cells, so a centred plate starts on a BBC
   * pixel. */
  const float text_scale = 1.0f;
  const float panel_padding = 12.0f;
  float panel_w = bbc_text_width(label, text_scale) + panel_padding * 2.0f;
  float panel_h = terminal_available ? 46.0f : 32.0f;
  float x = bbc_snap_x(((float)win_w - panel_w) * 0.5f);
  float y = bbc_snap_y((float)win_h - panel_h - 12.0f);

  begin_overlay();
  bbc_fill(r, BBC_BLACK, x, y, panel_w, panel_h);
  bbc_frame(r, BBC_BLUE, x, y, panel_w, panel_h);

  const float tick_w = 12.0f, tick_h = 8.0f;
  float right = x + panel_w, bottom = y + panel_h;
  bbc_fill(r, BBC_GREEN, x, y, tick_w, 2.0f);
  bbc_fill(r, BBC_GREEN, x, y, 4.0f, tick_h);
  bbc_fill(r, BBC_GREEN, right - tick_w, y, tick_w, 2.0f);
  bbc_fill(r, BBC_GREEN, right - 4.0f, y, 4.0f, tick_h);
  bbc_fill(r, BBC_GREEN, x, bottom - 2.0f, tick_w, 2.0f);
  bbc_fill(r, BBC_GREEN, x, bottom - tick_h, 4.0f, tick_h);
  bbc_fill(r, BBC_GREEN, right - tick_w, bottom - 2.0f, tick_w, 2.0f);
  bbc_fill(r, BBC_GREEN, right - 4.0f, bottom - tick_h, 4.0f, tick_h);

  bbc_text(r, x + panel_padding, y + (terminal_available ? 10.0f : 12.0f),
           text_scale, BBC_GREEN, label);

  if (terminal_available)
  {
    /* The breach as a bar of solid green over a dotted well, with a white
     * leading edge where the bytes are going in. */
    float well_x = x + panel_padding;
    float well_y = y + 30.0f;
    float well_w = panel_w - panel_padding * 2.0f;
    bbc_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_SPARSE, well_x, well_y,
               well_w, 8.0f);
    float bar_w = bbc_snap_x(well_w * progress);
    if (bar_w > 0.0f)
    {
      bbc_fill(r, BBC_GREEN, well_x, well_y, bar_w, 8.0f);
      bbc_fill(r, BBC_WHITE, well_x + bar_w - 4.0f, well_y, 4.0f, 8.0f);
    }
  }
}

/* ---- Cards and sheets ------------------------------------------------- */

/* The BBC's shade: a black checkerboard over everything already drawn, which
 * is the only way the machine had of putting a scene behind a menu. */
static void dim_whole_frame(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  bbc_stipple(r, BBC_BLACK, BBC_DITHER_CHECK, 0.0f, 0.0f, (float)win_w,
              (float)win_h);
}

/*
 * A verdict across the whole frame: the scene stippled out behind it, a black
 * band, a double rule of the accent's colour along each edge of the band, and
 * the title in quadruple-size type with the line under it.
 *
 * `extra_h` grows the band downward to take whatever the card prints under its
 * own subtitle — the countdown, the run's tally, the sector's — so a card is
 * one plate rather than a plate with lines hanging off it into the stipple,
 * where type over a checkerboard of the world is type nobody can read.
 */
static void draw_overlay_panel(Game *game, float y, BbcColour accent,
                               const char *title, const char *subtitle,
                               float extra_h)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  float w = (float)win_w;

  begin_overlay();
  dim_whole_frame(game);
  float top = bbc_snap_y(y - 36.0f);
  float bottom = bbc_snap_y(y + (subtitle ? 60.0f : 36.0f) + extra_h + 1.0f);
  bbc_fill(r, BBC_BLACK, 0.0f, top, w, bottom - top);
  bbc_fill(r, accent, 0.0f, top, w, 2.0f);
  bbc_fill(r, accent, 0.0f, top + 4.0f, w, 2.0f);
  bbc_fill(r, accent, 0.0f, bottom - 6.0f, w, 2.0f);
  bbc_fill(r, accent, 0.0f, bottom - 2.0f, w, 2.0f);

  /* A drop shadow one BBC pixel down and right in blue, which is how a BBC
   * title stood off its background without a glow to stand on. */
  float title_w = bbc_text_width(title, 4.0f);
  float title_x = bbc_snap_x((w - title_w) * 0.5f);
  float title_y = bbc_snap_y(y - 16.0f);
  bbc_text(r, title_x + 4.0f, title_y + 4.0f, 4.0f, BBC_BLUE, title);
  bbc_text(r, title_x, title_y, 4.0f, accent, title);
  if (subtitle)
    draw_text_centered(game, y + 37.0f, 2.0f, BBC_WHITE, subtitle);
}

/*
 * What a finished run made, on both of the screens a run can finish on.
 *
 * The words are [run_tally.c](run_tally.c) because they are read on two screens
 * now rather than one, and the reason there are two is the defect this pair of
 * functions was written for: the card below is reached by *losing*, and until
 * this existed it was the only place either figure appeared. A won campaign
 * banked its score and its sheets on the way into `STATE_OUTRO` and then said
 * nothing about either, so the ending the whole game is played for was the
 * ending that reported least — and a player who found all twelve sheets was told
 * less than one who died on sector three.
 */
static void draw_run_tally_lines(Game *game, float score_y, float score_scale,
                                 float docket_y, BbcColour score_colour,
                                 BbcColour docket_colour, float reveal)
{
  /* Faded in the way the BBC faded anything — the palette stepped up from
   * black a rung at a time — which is also how the outro's own cards fade, so
   * the two lines arrive as part of the card they sit on. */
  int steps = (int)((1.0f - reveal) * 4.0f);
  if (steps >= 3)
    return;
  bbc_palette_dim(steps);
  char line[RUN_TALLY_MAX];
  bool assisted = !campaign_records_count(&game->campaign);
  if (run_tally_format_score(game->campaign.score, game_best_score(game),
                             assisted, line, sizeof(line)) > 0)
  {
    draw_text_centered(game, score_y, score_scale, score_colour, line);
  }
  if (run_tally_format_docket(game->campaign.evidence_collected,
                              game_best_evidence(game), assisted, line,
                              sizeof(line)) > 0)
  {
    draw_text_centered(game, docket_y, 1.0f, docket_colour, line);
  }
  bbc_palette_reset();
}

/*
 * The same two lines over the outro's thank-you card.
 *
 * Drawn here rather than inside `outro_cutscene_render` because the cutscene
 * takes a time and a frame and knows nothing about a campaign — the same split
 * that keeps the sector tally in this file. It waits for the final reveal the
 * card itself waits for and fades in with it, so it arrives as part of that card
 * rather than over the shooting.
 */
static void draw_run_tally(Game *game, int win_w)
{
  (void)win_w;
  float time = game->presentation.outro_cutscene.time;
  if (time < OUTRO_FINAL_REVEAL_TIME)
    return;
  float reveal = (time - OUTRO_FINAL_REVEAL_TIME) / 1.6f;
  if (reveal > 1.0f)
    reveal = 1.0f;
  begin_overlay();
  /* Under THE BONDS BURNED. SHE DID NOT. at 151, and well above the replay
   * prompt at 505: the two lines read as the last paragraph of that card. */
  draw_run_tally_lines(game, 182.0f, 1.0f, 200.0f, BBC_WHITE, INK_LABEL,
                       reveal);
}

/*
 * The end of a run by losing it. Until the score outlived the process there was
 * nothing to compare this run to; now there is, so the card says both — what
 * this run made, and the best any run ever has.
 *
 * The docket line is a line here rather than a chip on the title screen, and
 * that is a composition decision the manual already argues: the shot up there
 * holds a wordmark, START and one quiet line of four chips that already come to
 * about 650 of the 800 the frame is laid out in. A fifth would be the band that
 * has to earn itself. Here there is room, and the number means something in the
 * same breath the score does.
 */
static void draw_game_over_panel(Game *game)
{
  /* The band runs on under the subtitle to take the two tally lines, which
   * end at 327. */
  draw_overlay_panel(game, 225.0f, BBC_RED, "GAME OVER",
                     "RETURNING TO MAIN MENU", 56.0f);
  draw_run_tally_lines(game, 305.0f, 2.0f, 323.0f, BBC_WHITE, INK_LABEL,
                       1.0f);
}

static void draw_continue_overlay(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  char hint[40];
  /* The band holds the offer, the countdown and the line under it, which ends
   * at 373. */
  draw_overlay_panel(game, 190.0f, BBC_YELLOW,
                     "CONTINUE?",
                     /* The same pair the skip prompts name, spelled the same
                      * way: this offer is a different one, the two buttons are
                      * not. `ENTER OR SPACE` was the third spelling of them in
                      * the tree — see `PAD_CONFIRM_KEYS`. */
                     pad_hint(game_pad_hints(game), hint, sizeof(hint),
                              "PRESS " PAD_CONFIRM_PAD,
                              "PRESS " PAD_CONFIRM_KEYS),
                     134.0f);

  int seconds = (int)ceilf(game->campaign.continue_timer);
  char countdown[16];
  char remaining[48];
  SDL_snprintf(countdown, sizeof(countdown), "%d", seconds);
  /* The retry is always on the table; what runs out is the score insurance. */
  if (game->campaign.continues_remaining > 0)
    SDL_snprintf(remaining, sizeof(remaining), "SCORE KEPT - %d LEFT",
                 game->campaign.continues_remaining);
  else
    SDL_snprintf(remaining, sizeof(remaining), "SCORE RESETS ON CONTINUE");
  /* The count in blocks, three rows to a font pixel, centred on 310 where it
   * has always stood: red behind, yellow in front, a BBC pixel apart. */
  float digits_w = block_text_width(countdown);
  float digits_x = bbc_snap_x(((float)win_w - digits_w) * 0.5f);
  float digits_y = bbc_snap_y(310.0f - 24.0f);
  draw_block_text(r, digits_x + 4.0f, digits_y + 2.0f, 3, BBC_RED, countdown);
  draw_block_text(r, digits_x, digits_y, 3, BBC_YELLOW, countdown);
  draw_text_centered(game, 365.0f, 2.0f, BBC_WHITE, remaining);
}

/*
 * The one plate both sheets are drawn on: the pause menu and the options.
 * They are the same object seen twice, so they are drawn the same way — the
 * plate cleared to black, a double rule of BBC pixels round it, cyan outside
 * and blue inside, which is the edge every interface surface in the game has.
 */
static void draw_sheet_plate(SDL_Renderer *r, float x, float y, float w,
                             float h)
{
  bbc_fill(r, BBC_BLACK, x, y, w, h);
  bbc_frame(r, BBC_CYAN, x, y, w, h);
  bbc_frame(r, BBC_BLUE, x + 8.0f, y + 4.0f, w - 16.0f, h - 8.0f);
}

/* The cursor's caret: the machine's own flashing cursor, beside the label it
 * points at. The label itself is drawn in inverse video by its row. The caret's
 * height is given rather than derived from the row, because a row is a label
 * with a sentence under it and the caret belongs beside the label — centring it
 * in the band puts it in the gap between the two, pointing at neither. */
static void draw_sheet_cursor(SDL_Renderer *r, float x, float caret_y)
{
  if (bbc_flash_on())
    bbc_text(r, x + 16.0f, caret_y, 1.0f, BBC_YELLOW, ">");
}

/*
 * The pause menu. Three items, because a paused run has exactly three things
 * the player can want from it, and the one that cannot be taken back is last
 * and set in the danger red — a list where ABANDON RUN looks like RESUME is a
 * list somebody eventually loses a run to.
 *
 * The words and the plate's own numbers are in [pause_sheet.h](pause_sheet.h)
 * rather than here, which is the rule every other sheet in the game already
 * keeps: this one held its three labels, its three details and its width as
 * literals inside this function, so `make test` could not measure the one
 * against the other. See the note in that header.
 */
/* The pause sheet has no SDL under it and so cannot ask the renderer how wide a
 * glyph is; it is the same 8x8 cell either way, and this is what says so. Same
 * assertion the manual's page module carries, for the same reason: the fit check
 * must measure the cell the renderer draws. */
_Static_assert(BBC_FONT_CELL == (int)PAUSE_GLYPH_W,
               "the pause sheet's fit check measures a different glyph than "
               "the renderer draws");
/* And the options sheet's, which had gone without one — the asymmetry this tree
 * keeps finding: half a rule kept, and nobody looking at the other half. It
 * needs both axes now, because that sheet's fit check has grown a second one. */
_Static_assert(BBC_FONT_CELL == (int)SETTINGS_GLYPH_W,
               "the options sheet's fit check measures a different glyph than "
               "the renderer draws");
_Static_assert(BBC_FONT_CELL == (int)SETTINGS_GLYPH_H,
               "the options sheet's height check measures a different glyph "
               "than the renderer draws");

static void draw_pause_menu(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  begin_overlay();
  dim_whole_frame(game);

  float panel_h = PAUSE_ROWS_TOP +
                  PAUSE_ROW_H * (float)PAUSE_ITEM_COUNT + PAUSE_ROW_FOOT;
  float panel_x = bbc_snap_x(((float)win_w - PAUSE_PANEL_W) * 0.5f);
  float panel_y = bbc_snap_y(((float)win_h - panel_h) * 0.5f - 10.0f);

  draw_sheet_plate(r, panel_x, panel_y, PAUSE_PANEL_W, panel_h);

  bbc_text(r, panel_x + PAUSE_LABEL_X, panel_y + 20.0f, PAUSE_LABEL_SCALE,
           BBC_YELLOW, PAUSE_TITLE);
  bbc_text(r, panel_x + PAUSE_LABEL_X, panel_y + 46.0f, PAUSE_DETAIL_SCALE,
           INK_LABEL, PAUSE_STRAP);

  for (int i = 0; i < PAUSE_ITEM_COUNT; ++i)
  {
    const PauseRow *row = &PAUSE_SHEET_ROWS[i];
    float row_y = panel_y + PAUSE_ROWS_TOP + (float)i * PAUSE_ROW_H;
    bool selected = game->pause_cursor == i;
    BbcColour tint = i == PAUSE_ITEM_ABANDON ? BBC_RED : BBC_WHITE;
    /* The armed row says what a second press will do, in the same red the label
     * is drawn in: a warning printed in the ordinary label colour is a warning
     * that reads as a description. `settings.c`'s RESET RECORDS row does the
     * same thing for the same reason — and while it waits for that press the
     * highlight flashes, because a run is one press from being thrown away. */
    bool armed = i == PAUSE_ITEM_ABANDON && game->pause_abandon_armed;
    float label_x = panel_x + PAUSE_ROW_TEXT_X;
    if (selected)
    {
      draw_sheet_cursor(r, panel_x, row_y + 4.0f);
      draw_inverse(r, label_x, row_y, PAUSE_LABEL_SCALE,
                   armed ? bbc_flash(tint) : tint, BBC_BLACK, row->label);
    }
    else
    {
      bbc_text(r, label_x, row_y, PAUSE_LABEL_SCALE, tint, row->label);
    }
    const char *detail = armed ? PAUSE_ABANDON_ARMED : row->detail;
    bbc_text(r, label_x, row_y + 20.0f, PAUSE_DETAIL_SCALE,
             armed ? BBC_RED : INK_LABEL, detail);
  }

  char hint[80];
  bbc_text(r, panel_x + PAUSE_LABEL_X, panel_y + panel_h - 26.0f,
           PAUSE_DETAIL_SCALE, INK_LABEL,
           pad_hint(game_pad_hints(game), hint, sizeof(hint),
                    PAUSE_HINT_PAD, PAUSE_HINT_KEYS));
}

/* From the end of the bar to the sheet's right margin: the widest reading the
 * row can print ("100%", four cells of the 8x8 font) plus a gap that keeps it
 * off the bar. */
#define SETTING_VALUE_GUTTER 46.0f

/*
 * The level a slider is set to, drawn as a bar rather than only as a number:
 * a number says what it is, a bar says how far along it is, and moving one is
 * a thing the player does by eye.
 *
 * The bar is the ten notches the row actually moves in, one block each, so it
 * reads as a control with stops rather than as a continuous fill the player is
 * nudging — lit in cyan up to the level and blue past it, which is how a BBC
 * game drew a gauge it had no room to label.
 */
static void draw_setting_slider(SDL_Renderer *r, float control_right, float y,
                                int percent, bool selected)
{
  const int notches = 100 / SETTING_VOLUME_STEP;
  const float pitch = 16.0f, block_w = 12.0f, block_h = 8.0f;
  float right = bbc_snap_x(control_right - SETTING_VALUE_GUTTER);
  float x = right - pitch * (float)notches + (pitch - block_w);
  int lit = (percent + SETTING_VOLUME_STEP / 2) / SETTING_VOLUME_STEP;
  for (int notch = 0; notch < notches; ++notch)
    bbc_fill(r, notch < lit ? BBC_CYAN : BBC_BLUE,
             x + (float)notch * pitch, y, block_w, block_h);

  char value[8];
  SDL_snprintf(value, sizeof(value), "%d%%", percent);
  /* Right-aligned against the sheet's own margin, so the digits do not shuffle
   * sideways as the level walks between 90 and 100 — and clear of the bar by
   * more than the one pixel that "wide enough for 100%" turns out to leave. */
  float value_w = bbc_text_width(value, 1.0f);
  bbc_text(r, control_right - value_w, y, 1.0f,
           selected ? INK_VALUE : BBC_WHITE, value);
}

/* A switch: a lit chip when on, an unlit one when off. Inverse video both
 * ways — black on green for on, white on blue for off — because the switch is
 * a state and a state is read as a colour before it is read as a word. */
static void draw_setting_toggle(SDL_Renderer *r, float x, float y, bool on)
{
  const float w = 56.0f, h = 16.0f;
  const char *word = on ? "ON" : "OFF";
  bbc_fill(r, on ? BBC_GREEN : BBC_BLUE, x, y - 4.0f, w, h);
  bbc_text(r, x + (w - bbc_text_width(word, 1.0f)) * 0.5f, y, 1.0f,
           on ? BBC_BLACK : BBC_WHITE, word);
}

/*
 * A binding row's caps.
 *
 * Every one is drawn whether or not it holds anything, because an action with
 * one key and an action with an empty second slot are different states and a
 * row that drew only what was filled would show them identically. An empty cap
 * is an outline with a dash in it — visibly a place a key goes, rather than a
 * gap in the sheet.
 *
 * The caret is the slot the next capture will land in, and it is only drawn on
 * the row under the cursor: a caret on every row would be nine claims about
 * where a press is about to go. It is the cap in inverse video, and a cap
 * waiting for a press flashes yellow, the way the machine's cursor did while it
 * waited for a key.
 */
static void draw_setting_keys(Game *game, float right, float y,
                              const SettingRow *row, bool selected)
{
  SDL_Renderer *r = game->platform.renderer;
  BindAction action = settings_row_action(row->id);
  if (action == BIND_COUNT)
    return;

  /*
   * Where the caps go is `settings_bind_caps` in
   * [settings.c](settings.c) rather than four lines of arithmetic here, and the
   * move is the whole of a shipped bug. The order they are drawn in is a claim
   * the sheet makes in words — the heading over the page says `TWO KEYS, THEN
   * TWO PAD BUTTONS` — and this function was contradicting it: the *keyboard's*
   * pair was pinned to the right margin with the pad's put to its left, under a
   * comment saying that made a row read keys-then-buttons "in the order the caret
   * walks them". Slot 0 is a key, so the page opened with the caret on the third
   * cap from the left and LEFT moved nothing at all. See `SettingsCap`.
   *
   * What is worth keeping from the old comment is why the two halves are still
   * sized separately over there: they are different alphabets. A key spells
   * itself in up to `KEYBIND_NAME_MAX` and a pad button in `PADBIND_NAME_MAX`,
   * most of them in two characters, so giving the pad the keyboard's width would
   * put four wide caps on a row with nine label characters to spare. Both are
   * sized off the longest name their half can spell rather than off what is in
   * them, because a column that moved with its contents would make the sheet
   * jitter as it is edited.
   */
  const float cap_h = 16.0f;

  const PadHints *spelling = game_pad_spelling(game);
  SettingsCap caps[BIND_TOTAL_SLOTS];
  int cap_count = settings_bind_caps(right, caps);

  for (int slot = 0; slot < cap_count; ++slot)
  {
    bool is_pad = caps[slot].pad;
    int within = is_pad ? slot - BIND_PAD_SLOT : slot;
    float w = caps[slot].w;
    float x = caps[slot].x;

    /* A pad name is a `pad_hint` template — `$A` and the rest — because what a
     * face button is called depends on what is plugged in. Expanded here, once,
     * through the same call every other prompt in the game makes.
     *
     * `game_pad_spelling` rather than `game_pad_hints`, and that is the whole of
     * a shipped bug: the second answers NULL with no pad plugged in, `pad_hint`
     * then copies its key form through verbatim, and the only key form a `$A`
     * template has is itself. So this cap printed the literal text `$A` — and
     * `$B`, `$X`, `$Y`, `$LB`, `$RB` — to every player without a controller,
     * which is most of them. See the note in [game_input.c](game_input.c). */
    char spelled[16];
    const char *name;
    if (is_pad)
    {
      const char *form =
          keybind_pad_name(game->settings.bindings.pad[action][within]);
      name = form[0] == '\0'
                 ? ""
                 : pad_hint(spelling, spelled, sizeof(spelled), form, form);
    }
    else
    {
      name = keybind_key_name(game->settings.bindings.keys[action][within]);
    }
    bool empty = name[0] == '\0';
    bool caret = selected && game->settings_bind_slot == slot;
    bool arming = caret && game->settings_capturing;

    const char *shown = arming ? "?" : (empty ? "-" : name);
    BbcColour ink;
    if (arming)
    {
      bbc_fill(r, bbc_flash_pair(BBC_YELLOW, BBC_RED), x, y - 4.0f, w, cap_h);
      ink = BBC_BLACK;
    }
    else if (caret)
    {
      bbc_fill(r, BBC_CYAN, x, y - 4.0f, w, cap_h);
      ink = BBC_BLACK;
    }
    else
    {
      bbc_frame(r, BBC_BLUE, x, y - 4.0f, w, cap_h);
      /* An empty cap's dash in the label colour rather than the rim's blue:
       * a dash nobody can see is a cap that reads as broken, not as empty. */
      ink = empty ? INK_LABEL : BBC_WHITE;
    }
    /* Centred in the cap rather than left-aligned: a one-character name and a
     * six-character one both have to read as the same kind of thing. */
    float text_w = bbc_text_width(shown, 1.0f);
    bbc_text(r, x + (w - text_w) * 0.5f, y, 1.0f, ink, shown);
  }
}

/*
 * The options sheet. Its rows come straight out of the table in
 * [settings.c](settings.c) and it draws whatever it finds there: adding a
 * setting is a line in that table and nothing here. The two kinds of control
 * are the whole layout language, which is the same bargain the manual's
 * `ManualLine` makes.
 */

static void draw_settings_sheet(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  /* Part of the layout, not a decoration on top of it: the warning stands in
   * the audio heading's detail slot, so the plate is sized with it. */
  bool muted = audio_is_muted(&game->platform.audio);

  begin_overlay();
  dim_whole_frame(game);

  int row_count = 0;
  const SettingRow *rows = settings_rows(game->settings_page, &row_count);

  /*
   * The whole of how tall this sheet is comes from `settings_page_layout`, on
   * the SDL-free side where `make test` can hold it — see the note on the
   * vertical geometry in [settings.h](settings.h).
   *
   * It used to be worked out here, and the arithmetic was wrong in the one
   * direction nothing could see: the squeeze scaled the value rows and then
   * took the scale off the whole total, headings included, so the plate came
   * out about 50px shorter than what this loop draws on it. The last section of
   * the sheet was printed over the footer and off the bottom of the frame, on
   * every machine, at every window size — the presentation is logical and
   * fixed — and in the still the store page is cut from.
   */
  SettingsLayout layout =
      settings_page_layout(game->settings_page, muted, (float)win_h);
  /* A heading is a rule with a name in it; one that carries a sentence as well
   * needs the room for it, or the sentence lands on the first row of its own
   * section. Neither height is squeezed, which is why they are read straight
   * from the header while the two below come off the layout. */
  const float heading_h = SETTINGS_HEADING_H;
  const float heading_detail_h = SETTINGS_HEADING_DETAIL_H;
  const float value_h = layout.value_h;
  const float bind_h = layout.bind_h;
  const float rows_top = SETTINGS_ROWS_TOP;
  const float panel_w = SETTINGS_PANEL_W;
  const float panel_h = layout.plate_h;
  float panel_x = bbc_snap_x(((float)win_w - panel_w) * 0.5f);
  float panel_y = bbc_snap_y(((float)win_h - panel_h) * 0.5f);

  draw_sheet_plate(r, panel_x, panel_y, panel_w, panel_h);

  bbc_text(r, panel_x + SETTINGS_LABEL_X, panel_y + 18.0f, 2.0f, BBC_YELLOW,
           settings_page_title(game->settings_page));
  bbc_text(r, panel_x + SETTINGS_LABEL_X, panel_y + 42.0f, 1.0f, INK_LABEL,
           settings_page_strap(game->settings_page));

  float y = panel_y + rows_top;
  for (int i = 0; i < row_count; ++i)
  {
    const SettingRow *row = &rows[i];

    if (row->kind == SETTING_ROW_HEADING)
    {
      /* A section rule: the name, and a blue rule of BBC pixels that carries
       * it across the plate so the groups read as groups. */
      const char *heading = settings_row_label(row);
      float label_w = bbc_text_width(heading, 1.0f);
      bbc_text(r, panel_x + SETTINGS_LABEL_X, y + 11.0f, 1.0f, BBC_YELLOW,
               heading);
      float rule_x = bbc_snap_x(panel_x + SETTINGS_LABEL_X + label_w + 11.0f);
      float rule_end = panel_x + panel_w - SETTINGS_LABEL_X;
      bbc_fill(r, BBC_BLUE, rule_x, y + 14.0f, rule_end - rule_x, 2.0f);
      /*
       * The mute is the one thing on this sheet that is not a stored setting,
       * and the only thing that can make the two levels below it lie.
       *
       * Keeping M off this screen stopped the sheet *creating* that
       * contradiction and did nothing at all about a player who muted first
       * and opened the sheet second — one keystroke, and the more likely
       * order of the two. The ban is still right and still in force, which is
       * why the line says outright that the key is not answered here rather
       * than naming a binding this screen does not have.
       *
       * Set in the danger red rather than the label colour: it is not a
       * description of the section, it is a correction to it.
       */
      bool levels_are_silenced =
          muted && settings_heading_governs_levels(rows, row_count, i);
      if (levels_are_silenced)
      {
        bbc_text(r, panel_x + SETTINGS_LABEL_X, y + 24.0f, 1.0f, BBC_RED,
                 SETTINGS_MUTED_LINE);
        y += heading_detail_h;
      }
      else if (row->detail != NULL)
      {
        bbc_text(r, panel_x + SETTINGS_LABEL_X, y + 24.0f, 1.0f, INK_LABEL,
                 row->detail);
        y += heading_detail_h;
      }
      else
      {
        y += heading_h;
      }
      continue;
    }

    bool selected = game->settings_cursor == i;
    /* The row's own pitch, rather than the fixed 36px the highlight used to
     * be: that number was chosen against an unsqueezed 40px row, so on a
     * squeezed page — and on the controls page, whose rows are 28 — the wash
     * reached over the label under it and claimed a row the cursor was not on.
     * The highlight is the label alone now, in inverse video, so it cannot
     * reach anything. */
    float row_pitch = row->kind == SETTING_ROW_BINDING ? bind_h : value_h;
    float label_x = panel_x + SETTINGS_ROW_TEXT_X;
    if (selected)
    {
      draw_sheet_cursor(r, panel_x, y);
      draw_inverse(r, label_x, y, 1.0f, BBC_WHITE, BBC_BLACK,
                   settings_row_label(row));
    }
    else
    {
      bbc_text(r, label_x, y, 1.0f, BBC_WHITE, settings_row_label(row));
    }
    /*
     * An armed row swaps its own detail line for the confirmation, in the danger
     * red the muted-levels correction uses, and for the same reason it is red
     * there: this is not a description of the row any more, it is what the next
     * press will do. Every line is a string in [settings.h](settings.h) reached
     * through [settings.c](settings.c) so the fit check measures the one the
     * player is actually reading. It flashes, because it is the one line on the
     * sheet that is waiting for something.
     *
     * Which rows can be armed is not asked here, and that is the point: this
     * used to name `SETTING_RECORDS_RESET` outright, so the row next door could
     * be — and was — destructive with nothing to draw.
     */
    const char *armed_detail = row->id == game->settings_armed_row
                                   ? settings_row_armed_detail(row->id)
                                   : NULL;
    if (armed_detail != NULL)
      bbc_text(r, label_x, y + SETTINGS_ROW_DETAIL_DY, 1.0f,
               bbc_flash_pair(BBC_RED, BBC_YELLOW), armed_detail);
    else if (row->detail != NULL)
      bbc_text(r, label_x, y + SETTINGS_ROW_DETAIL_DY, 1.0f, INK_LABEL,
               row->detail);

    /* Both controls end on the same right margin, so the column of switches
     * and the ends of the bars line up down the sheet. */
    float control_right = panel_x + panel_w - SETTINGS_CONTROL_INSET;
    if (row->kind == SETTING_ROW_SLIDER)
    {
      draw_setting_slider(r, control_right, y,
                          settings_value_percent(&game->settings, row->id),
                          selected);
    }
    else if (row->kind == SETTING_ROW_TOGGLE)
    {
      draw_setting_toggle(r, control_right - 56.0f, y,
                          settings_value_bool(&game->settings, row->id));
    }
    else if (row->kind == SETTING_ROW_BINDING)
    {
      draw_setting_keys(game, control_right, y, row, selected);
    }
    else if (row->kind == SETTING_ROW_READOUT)
    {
      /*
       * A figure the game is reporting, right-aligned on the same margin the
       * switches and the ends of the bars sit on, so the column reads as one
       * column whether it holds a state or a number.
       *
       * Set in the values' yellow the section rules use rather than the
       * switches' green and blue: a toggle's chip is a thing the player just
       * changed, and this is a thing the game is telling them.
       * `run_tally_format_record` answers `--` for a record nothing has been
       * written to, which is why there is no empty case here — see the note on
       * it.
       */
      char value[RUN_TALLY_RECORD_MAX];
      run_tally_format_record(settings_row_readout(row->id), &game->progress,
                              value, sizeof(value));
      bbc_text(r, control_right - bbc_text_width(value, 1.0f), y, 1.0f,
               INK_VALUE, value);
    }
    /* SETTING_ROW_ACTION draws no control of its own: it is a label that
     * happens when it is pressed, and a switch beside it would say it holds a
     * state it does not. */

    y += row_pitch;
  }

  /* Named exactly as bound: up and down walk, left and right change, and the
   * way out is whatever the player already reached for to open this. */
  char hint[96];
  /* An armed cap says what it is waiting for, and the two caps wait for
   * different things: a keyboard press into a key cap, a button into a pad
   * one. One line for both would name the wrong escape half the time — and
   * naming a way out the state does not accept is the thing this codebase
   * refuses everywhere else. All four of these lines are measured by
   * `test_every_word_on_the_options_sheet_fits_the_plate`, the footer included:
   * it lived here as a literal while every other word on the sheet had been
   * moved out to be measured, and it is the widest line the plate carries. */
  const char *footer;
  BbcColour footer_ink = INK_LABEL;
  if (game->settings_capturing)
  {
    footer = game_settings_slot_is_pad(game)
                 ? SETTINGS_CAPTURE_PAD_LINE
                 : SETTINGS_CAPTURE_KEY_LINE;
    footer_ink = BBC_YELLOW;
  }
  else
  {
    footer = pad_hint(game_pad_hints(game), hint, sizeof(hint),
                      SETTINGS_FOOTER_PAD_LINE, SETTINGS_FOOTER_KEY_LINE);
  }
  /* `SETTINGS_LABEL_X` rather than the 22 it used to repeat, so the indent the
   * test measures from is the indent the line is drawn at. */
  bbc_text(r, panel_x + SETTINGS_LABEL_X, panel_y + panel_h - 24.0f, 1.0f,
           footer_ink, footer);
}

#ifdef CHUCK_DEBUG
static void draw_debug_level_select(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  char label[80];
  SDL_snprintf(label, sizeof(label),
               "DEBUG  </>: LEVEL %02d/%02d  F5: LOAD",
               game->debug_selected_level + 1, (int)EMBEDDED_LEVEL_COUNT);
  float text_w = bbc_text_width(label, 1.0f);
  float panel_w = text_w + 24.0f;
  float panel_x = bbc_snap_x(((float)win_w - panel_w) * 0.5f);
  float panel_y = bbc_snap_y((float)win_h - 126.0f);

  /* A tool's own strip, in magenta so it is never mistaken for a line of the
   * game: a black plate with a bar each end, the level in inverse. */
  begin_overlay();
  bbc_fill(r, BBC_BLACK, panel_x, panel_y, panel_w, 24.0f);
  bbc_fill(r, BBC_MAGENTA, panel_x, panel_y, 4.0f, 24.0f);
  bbc_fill(r, BBC_MAGENTA, panel_x + panel_w - 4.0f, panel_y, 4.0f, 24.0f);
  bbc_text(r, panel_x + 12.0f, panel_y + 8.0f, 1.0f, BBC_MAGENTA, label);
}
#endif

/*
 * The city the names roll over.
 *
 * A roll on flat black would be the one screen in the game with nothing on it
 * but type, so the frame keeps a horizon: the skyline Chuck spent the night
 * above, seen from nowhere in particular, with Kessler Tower still the tallest
 * thing on it and its roof beacon still turning. It is drawn the way a BBC drew
 * a city at night — black blocks edged in the blue of distance against a black
 * sky with a few stars in it, a handful of lit windows a pixel each — because
 * the type is what this screen is for, and a backdrop that competes with it has
 * stopped being a backdrop. The blocks are cleared to black, and every line of
 * the roll clears its own cells as it goes, so every name is read on black.
 */
static unsigned credits_hash(unsigned x)
{
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

static void draw_credits_skyline(SDL_Renderer *r, float view_w, float view_h,
                                 float time)
{
  const float base = view_h - 20.0f; /* the lower letterbox bar is the ground */

  /* Stars first, so the towers stand in front of them. Asked for once and
   * never twinkling: a star that moved would be the one thing on the screen
   * the eye went to. */
  for (unsigned s = 0; s < 70u; ++s)
  {
    unsigned h = credits_hash(s * 2246822519u + 101u);
    float sx = (float)(h % (unsigned)view_w);
    float sy = 24.0f + (float)((h >> 12) % (unsigned)(base - 120.0f));
    bbc_plot(r, (h & 0x60000000u) == 0x60000000u ? BBC_WHITE : BBC_BLUE, sx,
             sy);
  }

  float x = -24.0f;
  for (int i = 0; x < view_w; ++i)
  {
    unsigned h = credits_hash((unsigned)i * 2654435761u + 17u);
    float w = 32.0f + (float)(h % 5u) * 12.0f;
    float height = 40.0f + (float)((h >> 5) % 7u) * 16.0f;
    bool tower = (i == 9);
    if (tower)
      height = 226.0f;

    float top = bbc_snap_y(base - height);
    bbc_fill(r, BBC_BLACK, x, top, w, base - top);
    bbc_fill(r, BBC_BLUE, x, top, w, 2.0f);
    bbc_fill(r, BBC_BLUE, x, top, 4.0f, base - top);
    bbc_fill(r, BBC_BLUE, x + w - 4.0f, top, 4.0f, base - top);

    /* A handful of floors still working, the way the tower's own facade is
     * drawn on the title screen: asked for once, never twinkling. The cell is
     * keyed on the window's place in its block, never on the screen, and the
     * arithmetic is done in `int` because the row starts off the left edge of
     * the frame — converting a negative float straight to `unsigned` is
     * undefined, and undefined *differently* on the two slices of the shipped
     * universal binary, which would light different windows on Intel and on
     * Apple Silicon. */
    int row = 0;
    for (float wy = top + 8.0f; wy < base - 6.0f; wy += 10.0f, ++row)
    {
      int col = 0;
      for (float wx = x + 8.0f; wx < x + w - 4.0f; wx += 8.0f, ++col)
      {
        unsigned cell = credits_hash((unsigned)(i * 977 + row * 131 + col));
        if ((cell % 9u) > 1u)
          continue;
        bbc_plot(r, (cell & 8u) ? BBC_YELLOW : BBC_BLUE, wx, wy);
      }
    }

    if (tower)
    {
      /* The roof they were never going to leave from: a red lamp on a mast,
       * on for a beat and off for one, at the rate it has always turned. */
      float beacon_x = bbc_snap_x(x + w * 0.5f);
      bbc_block(r, BBC_BLUE, beacon_x, top - 6.0f, 1, 3);
      if (sinf(time * 2.6f) > -0.2f)
        bbc_block(r, BBC_RED, beacon_x - 4.0f, top - 10.0f, 3, 2);
    }

    x += w + 4.0f;
  }
}

/* A row's colour, which is the one thing about it the table does not carry: the
 * game's own name is the brightest thing on the roll, the names are what the
 * screen is for, the job above a name is an interface label, and a note is
 * prose about the night, which on this screen is the story's magenta. */
static BbcColour credit_ink(CreditLineKind kind)
{
  switch (kind)
  {
  case CREDIT_TITLE:
    return BBC_YELLOW;
  case CREDIT_NAME:
    return BBC_WHITE;
  case CREDIT_ROLE:
    return INK_LABEL;
  case CREDIT_NOTE:
    return BBC_MAGENTA;
  case CREDIT_RULE:
  case CREDIT_GAP:
    break;
  }
  return BBC_WHITE;
}

/*
 * The roll itself: the table in [credits.c](credits.c), walked from the top,
 * every row placed by the height its own kind asks for. Nothing about the
 * layout is written here, which is the point — a line added to the table needs
 * no change to this function, and a line too long for the frame fails the test
 * suite rather than this screen.
 */
static void draw_credits_roll(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  const CreditsRoll *roll = &game->presentation.credits;
  float view_w = (float)win_w;
  float view_h = (float)win_h;

  begin_overlay();
  bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, view_w, view_h);
  draw_credits_skyline(r, view_w, view_h, roll->time);

  int count = 0;
  const CreditLine *lines = credits_lines(&count);
  float scroll = credits_scroll(roll);
  /* The roll climbs a whole BBC row at a time, the way anything on the machine
   * scrolled, so every line is on the grid and moves with the city behind it
   * rather than sliding over it a pixel at a time. */
  float y = bbc_snap_y(view_h - scroll);

  for (int i = 0; i < count; ++i)
  {
    float row_h = credits_line_height(lines[i].kind);
    /* Rows above and below the frame are stepped over rather than drawn: a
     * roll is a long strip and only a screen of it is ever on. */
    if (y > -row_h && y < view_h)
    {
      float center_x = view_w * 0.5f;
      if (lines[i].kind == CREDIT_RULE)
      {
        bbc_fill(r, BBC_RED, center_x - 56.0f, y + 8.0f, 112.0f, 2.0f);
      }
      else if (lines[i].kind != CREDIT_GAP)
      {
        /* Printed the way the machine printed text, which is cell and all: the
         * glyph's own eight-by-eight square goes down in the text background
         * colour first, so a line that rolls across a rooftop or a star is
         * still a line on black rather than a line through a city. */
        float scale = credits_line_scale(lines[i].kind);
        float text_w = bbc_text_width(lines[i].text, scale);
        float text_x = center_x - text_w * 0.5f;
        float cell_x = bbc_snap_x(text_x);
        bbc_fill(r, BBC_BLACK, cell_x,
                 y, bbc_snap_x(text_x + text_w + (float)(BBC_PX_W - 1)) - cell_x,
                 (float)BBC_FONT_CELL * scale);
        bbc_text(r, text_x, y, scale, credit_ink(lines[i].kind),
                 lines[i].text);
      }
    }
    y += row_h;
  }

  /* The same two bars the outro finishes on, drawn last so the roll runs under
   * them instead of stopping short of them. */
  bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, view_w, 20.0f);
  bbc_fill(r, BBC_BLACK, 0.0f, view_h - 20.0f, view_w, 20.0f);

  char hint[48];
  const PadHints *pad = game_pad_hints(game);
  bool resting = credits_at_rest(roll);
  /* The same words the outro one screen back spells, because the outro and the
   * roll are the two halves of one ending — see `PAD_CONFIRM_KEYS` in
   * [pad_hint.h](pad_hint.h) for what these two lines used to say instead. */
  const char *prompt =
      resting ? pad_hint(pad, hint, sizeof(hint),
                         PAD_CONFIRM_PAD " FOR THE MAIN MENU",
                         PAD_CONFIRM_KEYS " FOR THE MAIN MENU")
              : pad_hint(pad, hint, sizeof(hint),
                         PAD_CONFIRM_PAD " TO SKIP",
                         PAD_CONFIRM_KEYS " TO SKIP");
  /* It breathes at the flash rate between cyan and blue rather than blinking
   * out: a prompt that vanishes half the time is a prompt read half the time. */
  bbc_text(r, view_w - bbc_text_width(prompt, 1.0f) - 24.0f, view_h - 32.0f,
           1.0f, bbc_flash_pair(BBC_CYAN, BBC_BLUE), prompt);
}

/* ---- The exit banner -------------------------------------------------- */

/* Arrowheads for the banner, pointing where the way out is. '1' is filled with
 * the flashing yellow at draw time. Left is mirrored for right; up and down are
 * two drawings, because a sprite mirrors only across. */
BBC_SPRITE(SPR_ARROW_LEFT,
    "...1",
    "..11",
    ".111",
    "1111",
    ".111",
    "..11",
    "...1");
BBC_SPRITE(SPR_ARROW_UP,
    "..1..",
    ".111.",
    "11111",
    ".111.",
    ".111.",
    ".111.");
BBC_SPRITE(SPR_ARROW_DOWN,
    ".111.",
    ".111.",
    ".111.",
    "11111",
    ".111.",
    "..1..");

/*
 * PURSUIT ROUTE OPEN, for the two seconds after the card, with an arrow at each
 * end of the plate when the way out is off the screen — pointing at it, in the
 * yellow of something worth going to, flashing because it is new.
 */
static void draw_exit_banner(Game *game, int win_w, int win_h)
{
  SDL_Renderer *r = game->platform.renderer;
  const float plate_x = 200.0f, plate_y = 46.0f;
  const float plate_w = 400.0f, plate_h = 32.0f;
  begin_overlay();
  bbc_fill(r, BBC_BLACK, plate_x, plate_y, plate_w, plate_h);
  bbc_frame(r, BBC_GREEN, plate_x, plate_y, plate_w, plate_h);
  bbc_frame(r, BBC_GREEN, plate_x + 8.0f, plate_y + 4.0f, plate_w - 16.0f,
            plate_h - 8.0f);
  draw_text_centered(game, plate_y + plate_h * 0.5f, 2.0f, BBC_GREEN,
                     "PURSUIT ROUTE OPEN");

  float exit_screen = game->gameplay.level.map.exit_col * (float)TILE_SIZE - game->presentation.cam_x;
  float exit_screen_y = game->gameplay.level.map.exit_row * (float)TILE_SIZE +
                        HUD_HEIGHT - game->presentation.cam_y;
  const BbcSprite *arrow = NULL;
  int dir = 1;
  bool both_ends = false;
  if (exit_screen + TILE_SIZE < 0.0f)
    arrow = &SPR_ARROW_LEFT;
  else if (exit_screen > (float)win_w)
  {
    arrow = &SPR_ARROW_LEFT;
    dir = -1;
  }
  else if (exit_screen_y + TILE_SIZE < HUD_HEIGHT)
  {
    arrow = &SPR_ARROW_UP;
    both_ends = true;
  }
  else if (exit_screen_y > (float)win_h)
  {
    arrow = &SPR_ARROW_DOWN;
    both_ends = true;
  }
  if (arrow == NULL)
    return;

  BbcColour paint[BBC_SPRITE_SLOTS] = {bbc_flash(BBC_YELLOW), BBC_YELLOW,
                                       BBC_YELLOW, BBC_YELLOW};
  float aw = bbc_sprite_px_w(arrow);
  float ay = bbc_snap_y(plate_y + (plate_h - bbc_sprite_px_h(arrow)) * 0.5f);
  float left_x = plate_x + 16.0f;
  float right_x = plate_x + plate_w - 16.0f - aw;
  if (both_ends || dir > 0)
    bbc_sprite(r, arrow, left_x, ay, dir, paint);
  if (both_ends || dir < 0)
    bbc_sprite(r, arrow, right_x, ay, dir, paint);
}

/* ---- The frame -------------------------------------------------------- */

/*
 * The frame is drawn into a texture the size of the logical view and copied to
 * the window last, through the window's own letterboxed presentation. That
 * copy is the only place a BBC pixel is ever resized, so it is the only place
 * that has to keep them square-edged: the texture is scaled as pixel art.
 *
 * There used to be a bloom here, a vignette and scanlines. A BBC Micro had
 * none of them — what it put on the glass was eight colours and the edges of
 * its pixels — so the frame leaves this file as it was drawn.
 */
static bool frame_prepare(PlatformState *platform)
{
  if (platform->frame_state != 0)
    return platform->frame_state > 0;
  platform->frame_state = -1;

  SDL_Renderer *r = platform->renderer;
  platform->frame = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_TARGET, VIEW_W, VIEW_H);
  bool ok = platform->frame != NULL;
  if (ok && !SDL_SetTextureScaleMode(platform->frame, SDL_SCALEMODE_PIXELART))
    ok = SDL_SetTextureScaleMode(platform->frame, SDL_SCALEMODE_NEAREST);
  if (ok)
  {
    /* The frame answers the same logical size the window does, so every
     * renderer that asks `game_get_view_size` gets the frame it drew for. */
    ok = SDL_SetRenderTarget(r, platform->frame) &&
         SDL_SetRenderLogicalPresentation(r, VIEW_W, VIEW_H,
                                          SDL_LOGICAL_PRESENTATION_STRETCH);
  }
  SDL_SetRenderTarget(r, NULL);
  if (!ok)
  {
    SDL_Log("Offscreen frame unavailable on this renderer (%s); drawing to "
            "the window", SDL_GetError());
    if (platform->frame)
      SDL_DestroyTexture(platform->frame);
    platform->frame = NULL;
    return false;
  }
  platform->frame_state = 1;
  return true;
}

/*
 * What the sector just cleared paid, on the two screens that reach a player
 * who never sees a report — the reveal of the sector above a window, and the
 * card that ends the campaign. See [sector_tally.h](sector_tally.h) for why
 * eleven of the seventeen clears needed one.
 *
 * Drawn at scale 1.0 and centred, which is what the constants in that header
 * measure. It sits low in the frame during a reveal, under the map coming up
 * rather than over it, and inside the card's own band on the cleared card.
 *
 * `card_extra` is how far the cleared card's band has to run on under its
 * subtitle to take these lines, so the card can be drawn first and the lines
 * inside it rather than on a plate of their own over its bottom edge.
 */
static bool sector_tally_lines(Game *game, char *tally_line, size_t size,
                               const char **intel)
{
  if (!game->presentation.sector_tally.pending ||
      (game->state != STATE_LEVEL_START && game->state != STATE_LEVEL_CLEARED))
    return false;
  if (sector_tally_format(&game->presentation.sector_tally, tally_line,
                          size) <= 0)
    return false;
  *intel = sector_tally_intel(&game->presentation.sector_tally);
  return true;
}

static void draw_sector_tally(Game *game, int win_w, const char *tally_line,
                              const char *intel)
{
  SDL_Renderer *r = game->platform.renderer;
  begin_overlay();
  /*
   * And the plot line beside the numbers, for the ten clears whose row of
   * `TRANSITION_INTEL` no screen in the game used to reach. Two voices, so two
   * lines and two colours: the scoreboard is yellow and Chuck is the report's
   * own white, which is the pairing [cutscene.c](cutscene.c) draws one screen
   * over.
   *
   * The band grows only when there is a line to put in it. The last sector of
   * the campaign has no row, so the cleared card keeps the single-line band it
   * was laid out with and nothing shifts under the verdict panel above it.
   */
  bool on_card = game->state == STATE_LEVEL_CLEARED;
  /* On the card the line is four pixels lower than the 292 it used to be set
   * at, which is the room the card's subtitle needs under it now that the two
   * share one black band rather than the tally drawing a plate of its own over
   * the card's bottom edge. */
  float tally_y = on_card ? 296.0f : 522.0f;
  /*
   * The second line grows *away* from whatever the band must not touch, and
   * the two placements have opposite neighbours: the reveal sits on the bottom
   * edge of the frame with the map coming up above it, and the card sits
   * directly under `draw_overlay_panel`'s verdict. So the story line goes above
   * the numbers on a reveal and below them on the card.
   *
   * Grown upward on both, as the first draft did, the card printed the
   * sentence straight through `SHE IS TWENTY FEET AWAY`. No test could see it —
   * every assertion about this band is a width — and no run reaches it either,
   * because the card is only ever the last sector and the last sector has no
   * row. What reaches it is `--screen cleared --level 6`, which is a thing a
   * person types when they are cutting a still, and a frame that prints text
   * over text is the one thing `--shot` exists to catch.
   */
  float intel_y = on_card ? tally_y + SECTOR_TALLY_INTEL_RISE
                          : tally_y - SECTOR_TALLY_INTEL_RISE;
  if (!on_card)
  {
    /* A plate of its own along the foot of the reveal: black, ruled in yellow
     * top and bottom. On the card the card's own band is the plate. */
    float band_top = tally_y - 7.0f;
    if (intel != NULL)
      band_top -= SECTOR_TALLY_INTEL_RISE;
    band_top = bbc_snap_y(band_top);
    float band_bottom = tally_y + 16.0f;
    bbc_fill(r, BBC_BLACK, 0.0f, band_top, (float)win_w,
             band_bottom - band_top);
    bbc_fill(r, BBC_YELLOW, 0.0f, band_top, (float)win_w, 2.0f);
    bbc_fill(r, BBC_RED, 0.0f, band_bottom - 2.0f, (float)win_w, 2.0f);
  }
  if (intel != NULL)
    draw_text_centered(game, intel_y, 1.0f, BBC_WHITE, intel);
  draw_text_centered(game, tally_y, 1.0f, BBC_YELLOW, tally_line);
}

void game_render(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  bool offscreen = frame_prepare(&game->platform);
  if (offscreen)
    SDL_SetRenderTarget(r, game->platform.frame);
  bbc_begin_frame(game->presentation.render_clock,
                  game->settings.reduced_motion);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
  SDL_RenderClear(r);

  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);


  if (game->state == STATE_CHASE ||
      (game->state == STATE_PAUSED &&
       game->pause_return_state == STATE_CHASE))
  {
    chase_render(r, &game->chase, win_w, win_h,
                 game->presentation.camera_shake_x,
                 game->presentation.camera_shake_y,
                 game->settings.reduced_motion,
                 game_pad_hints(game));
    if (game->state == STATE_PAUSED && !game->platform.staged_frame)
      draw_pause_menu(game);
  }
  else if (game->state == STATE_SETTINGS)
  {
    /* Over the title the sheet floats on the night; over the pause screen it
     * floats on the held frame — and "the held frame" has to mean whatever was
     * actually running, which during the prologue is the drive. Asking only
     * whether the sheet came from the title screen put the frozen first sector
     * behind a sheet opened from a paused drive, which is a road the player
     * has not reached yet. */
    if (game->settings_return_state == STATE_INTRO)
    {
      intro_render(r, &game->presentation.intro, win_w, win_h,
                   game_pad_hints(game));
    }
    else if (game->pause_return_state == STATE_CHASE)
    {
      chase_render(r, &game->chase, win_w, win_h,
                   game->presentation.camera_shake_x,
                   game->presentation.camera_shake_y,
                   game->settings.reduced_motion,
                   game_pad_hints(game));
    }
    else
    {
      render_world(game);
      render_hud(game);
    }
    draw_settings_sheet(game);
  }
  else if (game->state == STATE_ABDUCTION)
  {
    abduction_cutscene_render(r, &game->presentation.abduction_cutscene,
                              win_w, win_h, game_pad_hints(game));
  }
  else if (game->state == STATE_OPENING_CUTSCENE)
  {
    opening_cutscene_render(r, &game->presentation.opening_cutscene,
                            win_w, win_h, game_pad_hints(game));
  }
  else if (game->state == STATE_INTRO)
  {
    intro_render(r, &game->presentation.intro, win_w, win_h,
                 game_pad_hints(game));
#ifdef CHUCK_DEBUG
    draw_debug_level_select(game);
#endif
  }
  else if (game->state == STATE_MANUAL)
  {
    /* The four figures the last sheet is about, read out of the file the shell
     * owns: the manual links no `Progress` and must not gain one. */
    ManualRecords records = {
        .best_score = game_best_score(game),
        .best_evidence = game_best_evidence(game),
        .sector_count = CAMPAIGN_SECTORS,
        .sector_time = game_sector_records(game),
    };
    manual_render(r, &game->presentation.manual, win_w, win_h,
                  game_pad_hints(game), &records);
  }
  else if (game->state == STATE_LEVEL_TRANSITION)
  {
    level_transition_render(r, &game->presentation.level_transition,
                            win_w, win_h, game_pad_hints(game));
  }
  else if (game->state == STATE_OUTRO)
  {
    outro_cutscene_render(r, &game->presentation.outro_cutscene,
                          win_w, win_h, game_pad_hints(game));
    draw_run_tally(game, win_w);
  }
  else if (game->state == STATE_CREDITS)
  {
    draw_credits_roll(game);
  }
  else
  {
    render_world(game);
    render_hud(game);
    render_crew_chatter(game, win_w);
    render_interaction_prompt(game, win_w, win_h);

    if (game->presentation.exit_unlocked_timer > 0.0f)
      draw_exit_banner(game, win_w, win_h);

    /*
     * What the sector just cleared paid — settled before the cards are drawn,
     * because on the cleared card the band the lines sit in is the card's own:
     * the card is drawn tall enough to hold them.
     */
    char tally_line[SECTOR_TALLY_MAX];
    const char *intel = NULL;
    bool tally = sector_tally_lines(game, tally_line, sizeof(tally_line),
                                    &intel);

    /*
     * The card that ends the campaign, and it is the only thing this state is.
     *
     * `try_finish_current_level` enters `STATE_LEVEL_CLEARED` in exactly one
     * branch: the one where there is no sector above. A window hands straight
     * over and a stair door draws the report, so every other clear in the game
     * goes somewhere else — which made "THE TRAIL LEADS UP" a line the player
     * reads once, on the roof, at the top of the building, with nothing above it
     * and Ellen twenty feet away. It was right when every clear drew this card
     * and became wrong the day the two other routes were added, which is the
     * shape of drift no fit check can see: the words fitted perfectly.
     */
    if (game->state == STATE_LEVEL_CLEARED)
    {
      /* The subtitle ends at 285 and the band would close at 300; the tally
       * at 296 and the story line under it at 307 need it to run on to 310
       * and 322 with the double rule below them. */
      float extra = 0.0f;
      if (tally)
        extra = intel != NULL ? 22.0f : 10.0f;
      draw_overlay_panel(game, 240.0f, BBC_GREEN,
                         "THE ROOF IS HIS", "SHE IS TWENTY FEET AWAY", extra);
    }
    else if (game->state == STATE_CONTINUE)
      draw_continue_overlay(game);
    else if (game->state == STATE_GAME_OVER)
      draw_game_over_panel(game);
    else if (game->state == STATE_PAUSED && !game->platform.staged_frame)
      draw_pause_menu(game);

    /*
     * **After the card rather than before it**, because `draw_overlay_panel`
     * opens by stippling the whole frame so the verdict owns it. Drawn first,
     * the line would have been the one thing on the cleared card printed
     * through the shade — and on the card the lines are inside its band now,
     * which only holds if the band is down before they are.
     */
    if (tally)
      draw_sector_tally(game, win_w, tally_line, intel);
  }

  /* The frame goes out to the window last, through the window's own
   * letterboxed presentation — the bars keep the colour they always had. */
  if (offscreen)
  {
    SDL_SetRenderTarget(r, NULL);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_SetTextureBlendMode(game->platform.frame, SDL_BLENDMODE_NONE);
    SDL_RenderTexture(r, game->platform.frame, NULL, NULL);
  }

  /* A pending `--shot` is written here rather than back in `SDL_AppIterate`,
   * because the line below is the last moment this frame exists: a presented
   * back buffer holds whatever the driver decided to leave in it, so a capture
   * taken after the swap is a capture of nothing in particular. */
  shot_plan_capture(&game->platform.shot, r);

  SDL_RenderPresent(r);
}
