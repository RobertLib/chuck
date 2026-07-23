#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>

#include "fx.h"
#include "gameplay_interaction.h"
#include "gameplay_world.h"

/*
 * Chuck intentionally ships without art assets.  Everything in this file is
 * assembled at runtime from small, hard-edged shapes.  The palette and the
 * lighting primitives live in fx.h and are shared with the intro and the
 * cutscenes, so the whole game reads as one production.  This file adds the
 * things only gameplay needs: the tile materials, ambient occlusion under
 * floors, warm ceiling lights, and a finishing scanline/vignette pass.
 */

static const SDL_Color COL_INK = {5, 7, 12, 255};
static const SDL_Color COL_OUTLINE = {13, 18, 27, 255};
static const SDL_Color COL_CYAN = {74, 222, 212, 255};
static const SDL_Color COL_AMBER = {248, 188, 74, 255};
static const SDL_Color COL_RED = {232, 74, 62, 255};

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

static void set_color(SDL_Renderer *r, SDL_Color c)
{
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
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

static void color_rect(SDL_Renderer *r, SDL_Color c, float x, float y, float w, float h)
{
  set_color(r, c);
  fill_rect(r, x, y, w, h);
}

static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      Uint8 cr, Uint8 cg, Uint8 cb, const char *text)
{
  SDL_SetRenderScale(r, scale, scale);
  SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
  SDL_RenderDebugText(r, x / scale, y / scale, text);
  SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static float draw_text_width(const char *text, float scale)
{
  return (float)SDL_strlen(text) *
         SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

static void draw_text_centered(Game *game, float center_y, float scale,
                               Uint8 cr, Uint8 cg, Uint8 cb, const char *text)
{
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  float text_w = draw_text_width(text, scale);
  float text_h = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  draw_text(game->platform.renderer, ((float)win_w - text_w) * 0.5f,
            center_y - text_h * 0.5f, scale, cr, cg, cb, text);
}

static unsigned tile_hash(int x, int y)
{
  unsigned h = (unsigned)x * 0x8da6b343u;
  h ^= (unsigned)y * 0xd8163841u;
  h ^= h >> 13;
  h *= 0xcb1ab31fu;
  return h ^ (h >> 16);
}

static void draw_soft_glow(SDL_Renderer *r, float x, float y, float w, float h,
                           SDL_Color color)
{
  /* True radial falloff sells emissive surfaces far better than rings. */
  float radius = (w > h ? w : h) * 0.5f + 12.0f;
  fx_glow(r, x + w * 0.5f, y + h * 0.5f, radius, color, 72);
}

static void render_background(Game *game, int win_w, int win_h)
{
  SDL_Renderer *r = game->platform.renderer;
  const float oy = HUD_HEIGHT;
  const float fh = (float)win_h - oy;
  float t = (float)SDL_GetTicksNS() * 1.0e-9f;

  /* Smooth atmospheric gradient: cool and dark up top, a faint teal cast
   * toward the floor so the space feels lit from below by machinery. */
  color_rect(r, FX_NIGHT, 0.0f, oy, (float)win_w, fh);
  fx_vgrad(r, 0.0f, oy, (float)win_w, fh,
           (SDL_Color){7, 10, 18, 255}, 255,
           (SDL_Color){20, 30, 42, 255}, 255);

  /* Farthest layer: hulking silhouettes of plant machinery, barely lit. */
  float far_shift = fmodf(-game->presentation.cam_x * 0.10f, 224.0f);
  if (far_shift > 0.0f)
    far_shift -= 224.0f;
  for (float x = far_shift - 48.0f; x < (float)win_w + 224.0f; x += 224.0f)
  {
    unsigned seed = (unsigned)((int)(x + game->presentation.cam_x * 0.10f) / 224);
    unsigned h = fx_hash(seed * 31u + (unsigned)game->campaign.current_level * 7u);
    float tank_h = 96.0f + (float)(h % 60u);
    color_rect(r, (SDL_Color){13, 18, 29, 255},
               x + 24.0f, oy + fh - tank_h, 74.0f, tank_h);
    color_rect(r, (SDL_Color){16, 22, 34, 255},
               x + 30.0f, oy + fh - tank_h - 14.0f, 62.0f, 14.0f);
    color_rect(r, (SDL_Color){13, 18, 29, 255},
               x + 128.0f, oy + fh - 64.0f, 70.0f, 64.0f);
    /* Slim exhaust stack with a slow-blinking service light. */
    color_rect(r, (SDL_Color){15, 21, 32, 255},
               x + 150.0f, oy + 34.0f, 10.0f, fh - 98.0f);
    float blink = sinf(t * 1.4f + (float)(h % 7u)) > 0.86f ? 1.0f : 0.28f;
    color_rect(r, fx_dim((SDL_Color){194, 84, 66, 255}, blink),
               x + 153.0f, oy + 30.0f, 4.0f, 3.0f);
  }

  /* Mid layer: service bays with pilasters, pipe runs and dim windows. */
  float mid_shift = fmodf(-game->presentation.cam_x * 0.18f, 192.0f);
  if (mid_shift > 0.0f)
    mid_shift -= 192.0f;
  for (float x = mid_shift - 32.0f; x < (float)win_w + 192.0f; x += 192.0f)
  {
    color_rect(r, (SDL_Color){18, 25, 38, 255},
               x + 16.0f, oy + 26.0f, 144.0f, fh - 44.0f);
    color_rect(r, (SDL_Color){26, 36, 50, 255},
               x + 16.0f, oy + 26.0f, 144.0f, 3.0f);
    color_rect(r, (SDL_Color){11, 16, 26, 255},
               x + 28.0f, oy + 52.0f, 120.0f, 78.0f);
    color_rect(r, (SDL_Color){31, 42, 56, 255},
               x + 28.0f, oy + 52.0f, 120.0f, 2.0f);
    /* Pipe run along the bay with hanging brackets. */
    color_rect(r, (SDL_Color){24, 33, 46, 255},
               x + 16.0f, oy + 146.0f, 144.0f, 5.0f);
    color_rect(r, (SDL_Color){33, 45, 60, 255},
               x + 16.0f, oy + 146.0f, 144.0f, 1.0f);
    for (int b = 0; b < 3; ++b)
      color_rect(r, (SDL_Color){15, 21, 32, 255},
                 x + 34.0f + (float)b * 48.0f, oy + 130.0f, 3.0f, 16.0f);

    /* Dim equipment lights; the warm ones flicker very rarely. */
    for (int lamp = 0; lamp < 4; ++lamp)
    {
      unsigned h = tile_hash((int)(x + game->presentation.cam_x * 0.18f) / 8 + lamp,
                             game->campaign.current_level + 7);
      bool warm = (h & 1u) != 0u;
      float flicker = warm && ((h >> 3) & 7u) == 0u &&
                              fmodf(t * 1.7f + (float)lamp, 4.0f) < 0.09f
                          ? 0.3f
                          : 1.0f;
      SDL_Color lc = warm ? fx_dim((SDL_Color){126, 88, 44, 255}, flicker)
                          : (SDL_Color){40, 96, 104, 255};
      color_rect(r, lc, x + 36.0f + lamp * 26.0f, oy + 70.0f, 12.0f, 4.0f);
      fx_rect_a(r, lc, 26, x + 34.0f + lamp * 26.0f, oy + 68.0f, 16.0f, 8.0f);
    }
  }

  /* Slow volumetric light shafts falling between the far bays. */
  float shaft_shift = fmodf(-game->presentation.cam_x * 0.22f, 384.0f);
  if (shaft_shift > 0.0f)
    shaft_shift -= 384.0f;
  for (float x = shaft_shift - 96.0f; x < (float)win_w + 384.0f; x += 384.0f)
  {
    float sway = sinf(t * 0.21f + x * 0.01f) * 14.0f;
    fx_light_cone(r, x + 150.0f + sway, oy - 6.0f, 14.0f, 52.0f,
                  fh * 0.9f, (SDL_Color){120, 190, 196, 255}, 13);
  }

  /* Near layer: wall seams and conduit shadows at a faster parallax. */
  float near_shift = fmodf(-game->presentation.cam_x * 0.30f, 96.0f);
  if (near_shift > 0.0f)
    near_shift -= 96.0f;
  for (float x = near_shift; x < (float)win_w + 96.0f; x += 96.0f)
  {
    fx_rect_a(r, (SDL_Color){30, 42, 56, 255}, 130,
              x, oy, 2.0f, fh);
    color_rect(r, (SDL_Color){12, 17, 27, 255}, x + 66.0f, oy + 8.0f, 4.0f, 86.0f);
    color_rect(r, (SDL_Color){45, 58, 72, 255}, x + 67.0f, oy + 8.0f, 1.0f, 86.0f);
  }

  /* Tiny, low-contrast dust motes make empty rooms feel alive. */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < 26; ++i)
  {
    unsigned h = tile_hash(i + game->campaign.current_level * 29, 91);
    float x = fmodf((float)(h % (unsigned)(win_w + 80)) + t * (4.0f + (float)(i % 5)), (float)(win_w + 80)) - 40.0f;
    float y = oy + 18.0f + (float)((h >> 8) % (unsigned)(win_h - HUD_HEIGHT - 36)) +
              sinf(t * 0.8f + (float)i) * 2.0f;
    Uint8 a = (Uint8)(22 + (h % 20u));
    set_rgba(r, 130, 162, 170, a);
    fill_rect(r, x, y, (i % 3 == 0) ? 2.0f : 1.0f, 1.0f);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  /* Depth haze pooling at the bottom of the frame. */
  fx_vgrad(r, 0.0f, oy + fh - 90.0f, (float)win_w, 90.0f,
           (SDL_Color){8, 12, 20, 255}, 0,
           (SDL_Color){8, 12, 20, 255}, 120);
}

static void draw_wall_tile(SDL_Renderer *r, const Level *lvl,
                           int col, int row, float x, float y)
{
  /* Plates span 2x2 tiles: seams and bevels only appear on panel borders,
   * so the wall reads as riveted plating rather than a 32px checkerboard. */
  unsigned ph = tile_hash(col >> 1, row >> 1);
  unsigned h = tile_hash(col, row);
  Uint8 v = (Uint8)(ph % 7u);
  SDL_Color base = {(Uint8)(41 + v), (Uint8)(51 + v), (Uint8)(64 + v), 255};
  bool left_edge = (col & 1) == 0;
  bool top_edge = (row & 1) == 0;
  bool right_edge = !left_edge;
  bool bottom_edge = !top_edge;

  color_rect(r, base, x, y, TILE_SIZE, TILE_SIZE);

  /* Quiet per-tile wear keeps large walls from feeling machine-stamped. */
  color_rect(r, (SDL_Color){(Uint8)(35 + v), (Uint8)(45 + v), (Uint8)(57 + v), 255},
             x + (float)(h % 21u) + 4.0f, y + (float)((h >> 6) % 22u) + 4.0f,
             4.0f, 2.0f);
  if ((h & 3u) == 0u)
    color_rect(r, (SDL_Color){(Uint8)(48 + v), (Uint8)(59 + v), (Uint8)(73 + v), 255},
               x + (float)((h >> 4) % 18u) + 6.0f, y + (float)((h >> 9) % 18u) + 8.0f,
               6.0f, 1.0f);

  /* Panel bevel: lit top/left edge, shaded bottom/right edge, dark seam. */
  if (top_edge)
  {
    color_rect(r, (SDL_Color){20, 27, 39, 255}, x, y, TILE_SIZE, 1.0f);
    color_rect(r, (SDL_Color){(Uint8)(58 + v), (Uint8)(71 + v), (Uint8)(86 + v), 255},
               x, y + 1.0f, TILE_SIZE, 1.0f);
  }
  if (left_edge)
  {
    color_rect(r, (SDL_Color){20, 27, 39, 255}, x, y, 1.0f, TILE_SIZE);
    color_rect(r, (SDL_Color){(Uint8)(53 + v), (Uint8)(66 + v), (Uint8)(80 + v), 255},
               x + 1.0f, y + 1.0f, 1.0f, TILE_SIZE - 1.0f);
  }
  if (bottom_edge)
    color_rect(r, (SDL_Color){(Uint8)(30 + v), (Uint8)(39 + v), (Uint8)(51 + v), 255},
               x, y + TILE_SIZE - 2.0f, TILE_SIZE, 2.0f);
  if (right_edge)
    color_rect(r, (SDL_Color){(Uint8)(33 + v), (Uint8)(42 + v), (Uint8)(54 + v), 255},
               x + TILE_SIZE - 2.0f, y, 2.0f, TILE_SIZE);

  /* One rivet per panel corner. */
  if (top_edge && left_edge)
  {
    color_rect(r, (SDL_Color){118, 134, 148, 255}, x + 3.0f, y + 3.0f, 2.0f, 2.0f);
    color_rect(r, (SDL_Color){22, 30, 42, 255}, x + 4.0f, y + 4.0f, 1.0f, 1.0f);
  }
  if (bottom_edge && right_edge)
    color_rect(r, (SDL_Color){96, 110, 124, 255},
               x + TILE_SIZE - 6.0f, y + TILE_SIZE - 6.0f, 2.0f, 2.0f);

  /* Rare full-tile variants: a vent grille or a hairline crack. */
  if ((h % 23u) == 0u)
  {
    for (int slit = 0; slit < 4; ++slit)
    {
      color_rect(r, (SDL_Color){17, 23, 35, 255},
                 x + 8.0f, y + 9.0f + slit * 4.0f, 16.0f, 2.0f);
      color_rect(r, (SDL_Color){(Uint8)(56 + v), (Uint8)(69 + v), (Uint8)(84 + v), 255},
                 x + 8.0f, y + 11.0f + slit * 4.0f, 16.0f, 1.0f);
    }
  }
  else if ((h % 11u) == 0u)
  {
    set_rgba(r, 26, 34, 46, 255);
    SDL_RenderLine(r, x + 12.0f, y + 9.0f, x + 16.0f, y + 14.0f);
    SDL_RenderLine(r, x + 16.0f, y + 14.0f, x + 14.0f, y + 20.0f);
    set_rgba(r, 62, 76, 92, 255);
    SDL_RenderLine(r, x + 13.0f, y + 9.0f, x + 17.0f, y + 14.0f);
  }

  /* Exposed floor tops get a bright machined lip with worn paint dashes. */
  if (!level_is_solid(lvl, col, row - 1))
  {
    color_rect(r, (SDL_Color){104, 122, 136, 255}, x, y, TILE_SIZE, 3.0f);
    color_rect(r, (SDL_Color){168, 184, 192, 255}, x + 1.0f, y, TILE_SIZE - 2.0f, 1.0f);
    color_rect(r, (SDL_Color){58, 72, 86, 255}, x, y + 3.0f, TILE_SIZE, 1.0f);
    if ((h & 3u) == 0u)
      color_rect(r, FX_AMBER_DK, x + (float)(h % 14u) + 4.0f, y + 1.0f, 9.0f, 2.0f);
  }

  /* Exposed undersides fall into shadow. */
  if (!level_is_solid(lvl, col, row + 1))
    color_rect(r, (SDL_Color){16, 22, 33, 255},
               x, y + TILE_SIZE - 2.0f, TILE_SIZE, 2.0f);

  /* Side faces against open air get a slim machined trim. */
  if (!level_is_solid(lvl, col - 1, row))
    color_rect(r, (SDL_Color){74, 90, 105, 255}, x, y, 2.0f, TILE_SIZE);
  if (!level_is_solid(lvl, col + 1, row))
    color_rect(r, (SDL_Color){26, 34, 46, 255},
               x + TILE_SIZE - 2.0f, y, 2.0f, TILE_SIZE);
}

static void draw_ladder_tile(SDL_Renderer *r, float x, float y, int row)
{
  (void)row;
  /* Cast shadow, steel side rails, and worn amber-painted rungs. */
  color_rect(r, (SDL_Color){6, 9, 15, 170}, x + 5.0f, y, 5.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){6, 9, 15, 170}, x + 24.0f, y, 5.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){47, 46, 38, 255}, x + 7.0f, y, 4.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){47, 46, 38, 255}, x + 22.0f, y, 4.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){196, 148, 62, 255}, x + 8.0f, y, 2.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){196, 148, 62, 255}, x + 23.0f, y, 2.0f, TILE_SIZE);
  for (int rung = -4; rung < TILE_SIZE; rung += 8)
  {
    color_rect(r, (SDL_Color){52, 42, 28, 255}, x + 8.0f, y + (float)rung + 2.0f, 17.0f, 3.0f);
    color_rect(r, (SDL_Color){226, 180, 84, 255}, x + 9.0f, y + (float)rung + 1.0f, 15.0f, 2.0f);
    color_rect(r, (SDL_Color){255, 224, 150, 255}, x + 9.0f, y + (float)rung + 1.0f, 15.0f, 1.0f);
  }
}

static void draw_shaft_tile(SDL_Renderer *r, float x, float y, int col, int row)
{
  unsigned h = tile_hash(col, row);
  color_rect(r, (SDL_Color){8, 13, 20, 255}, x + 4.0f, y, TILE_SIZE - 8.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){23, 31, 42, 255}, x + 5.0f, y, 3.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){23, 31, 42, 255}, x + 24.0f, y, 3.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){82, 94, 102, 255}, x + 8.0f, y, 2.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){82, 94, 102, 255}, x + 22.0f, y, 2.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){5, 8, 13, 255}, x + 15.0f, y, 2.0f, TILE_SIZE);
  if ((h & 3u) == 0u)
    color_rect(r, (SDL_Color){42, 125, 126, 255}, x + 12.0f, y + 13.0f, 8.0f, 3.0f);
}

static void draw_platform(SDL_Renderer *r, float x, float y, SDL_Color accent, bool unstable)
{
  color_rect(r, COL_INK, x - 1.0f, y - 1.0f, TILE_SIZE + 2.0f, 9.0f);
  color_rect(r, (SDL_Color){50, 61, 66, 255}, x, y, TILE_SIZE, 7.0f);
  color_rect(r, accent, x + 1.0f, y, TILE_SIZE - 2.0f, 2.0f);
  color_rect(r, (SDL_Color){22, 29, 35, 255}, x + 4.0f, y + 4.0f, TILE_SIZE - 8.0f, 2.0f);
  color_rect(r, (SDL_Color){132, 143, 139, 255}, x + 3.0f, y + 3.0f, 2.0f, 2.0f);
  color_rect(r, (SDL_Color){132, 143, 139, 255}, x + TILE_SIZE - 5.0f, y + 3.0f, 2.0f, 2.0f);
  if (unstable)
  {
    color_rect(r, COL_AMBER, x + 9.0f, y + 1.0f, 4.0f, 2.0f);
    color_rect(r, COL_RED, x + 15.0f, y + 1.0f, 4.0f, 2.0f);
  }
}

static void draw_door(SDL_Renderer *r, float x, float y, int index)
{
  /* A small lamp above the frame spills warm light onto the doorway. */
  fx_glow(r, x + 16.0f, y + 1.0f, 16.0f, (SDL_Color){248, 202, 118, 255}, 46);
  color_rect(r, (SDL_Color){15, 20, 30, 255}, x + 10.0f, y - 3.0f, 12.0f, 3.0f);
  color_rect(r, (SDL_Color){248, 202, 118, 255}, x + 12.0f, y - 1.0f, 8.0f, 1.0f);

  /* Sliding steel service door in a machined frame. */
  color_rect(r, COL_INK, x + 1.0f, y, 30.0f, 32.0f);
  color_rect(r, (SDL_Color){62, 75, 90, 255}, x + 2.0f, y + 1.0f, 28.0f, 31.0f);
  color_rect(r, (SDL_Color){88, 104, 120, 255}, x + 2.0f, y + 1.0f, 28.0f, 1.0f);
  color_rect(r, (SDL_Color){34, 43, 56, 255}, x + 5.0f, y + 4.0f, 22.0f, 28.0f);
  color_rect(r, (SDL_Color){44, 55, 70, 255}, x + 6.0f, y + 5.0f, 20.0f, 26.0f);

  /* Recessed viewing slit and brushed panel lines. */
  color_rect(r, (SDL_Color){12, 18, 26, 255}, x + 9.0f, y + 7.0f, 14.0f, 5.0f);
  color_rect(r, (SDL_Color){30, 66, 76, 255}, x + 10.0f, y + 8.0f, 12.0f, 3.0f);
  for (int line = 0; line < 3; ++line)
    color_rect(r, (SDL_Color){33, 42, 54, 255},
               x + 8.0f, y + 18.0f + line * 4.0f, 16.0f, 1.0f);

  /* Access reader with a live status LED. */
  color_rect(r, (SDL_Color){14, 20, 28, 255}, x + 22.0f, y + 16.0f, 5.0f, 9.0f);
  color_rect(r, (SDL_Color){86, 226, 186, 255}, x + 23.0f, y + 17.0f, 3.0f, 2.0f);

  char label[3];
  SDL_snprintf(label, sizeof(label), "%d", index / 2 + 1);
  draw_text(r, x + 13.0f, y + 14.0f, 0.65f, 148, 176, 188, label);
}

static void draw_exit(SDL_Renderer *r, const Game *game, float x, float y)
{
  bool unlocked = game->gameplay.level.runtime.exit_unlocked;
  SDL_Color signal = unlocked ? (SDL_Color){64, 238, 145, 255}
                              : (SDL_Color){230, 75, 61, 255};
  if (unlocked)
    draw_soft_glow(r, x + 5.0f, y + 3.0f, 22.0f, 28.0f, signal);

  color_rect(r, COL_INK, x + 1.0f, y, 30.0f, 32.0f);
  color_rect(r, (SDL_Color){41, 54, 62, 255}, x + 3.0f, y + 2.0f, 26.0f, 30.0f);
  color_rect(r, (SDL_Color){66, 82, 88, 255}, x + 5.0f, y + 4.0f, 22.0f, 26.0f);
  color_rect(r, (SDL_Color){19, 29, 34, 255}, x + 8.0f, y + 12.0f, 16.0f, 16.0f);
  color_rect(r, signal, x + 6.0f, y + 6.0f, 20.0f, 3.0f);
  color_rect(r, (SDL_Color){11, 18, 22, 255}, x + 20.0f, y + 18.0f, 5.0f, 7.0f);
  color_rect(r, signal, x + 21.0f, y + 19.0f, 3.0f, 2.0f);
  draw_text(r, x + 9.0f, y + 12.0f, 0.65f, signal.r, signal.g, signal.b,
            unlocked ? "GO" : "LOCK");
}

static void draw_terminal(SDL_Renderer *r, float x, float y,
                          bool active, bool hacked, float world_t)
{
  SDL_Color screen = active ? (SDL_Color){68, 245, 159, 255}
                            : (SDL_Color){62, 75, 79, 255};
  bool pulse = ((int)(world_t * 4.0f) & 1) == 0;

  if (active)
  {
    draw_soft_glow(r, x + 5.0f, y + 3.0f, 22.0f, 25.0f, screen);
    color_rect(r, pulse ? (SDL_Color){126, 255, 196, 255} : screen,
               x + 13.0f, y - 4.0f, 6.0f, 3.0f);
    color_rect(r, screen, x + 15.0f, y - 7.0f, 2.0f, 4.0f);
  }

  color_rect(r, COL_INK, x + 2.0f, y + 1.0f, 28.0f, 30.0f);
  color_rect(r, active ? (SDL_Color){38, 72, 69, 255}
                       : (SDL_Color){42, 48, 51, 255},
             x + 4.0f, y + 3.0f, 24.0f, 27.0f);
  color_rect(r, (SDL_Color){11, 19, 23, 255},
             x + 6.0f, y + 5.0f, 20.0f, 13.0f);
  color_rect(r, active ? screen : (SDL_Color){50, 61, 64, 255},
             x + 8.0f, y + 7.0f, 16.0f, 9.0f);

  if (active)
  {
    color_rect(r, (SDL_Color){10, 76, 60, 255},
               x + 10.0f, y + 9.0f, 12.0f, 1.0f);
    color_rect(r, (SDL_Color){193, 255, 218, 255},
               x + 10.0f, y + 12.0f, hacked ? 12.0f : 7.0f, 2.0f);
  }
  else
  {
    color_rect(r, (SDL_Color){29, 35, 38, 255},
               x + 10.0f, y + 9.0f, 12.0f, 1.0f);
    color_rect(r, (SDL_Color){88, 47, 44, 255},
               x + 15.0f, y + 12.0f, 2.0f, 2.0f);
  }

  color_rect(r, (SDL_Color){20, 27, 30, 255},
             x + 7.0f, y + 20.0f, 18.0f, 8.0f);
  color_rect(r, active ? screen : (SDL_Color){73, 78, 77, 255},
             x + 9.0f, y + 22.0f, 3.0f, 2.0f);
  color_rect(r, active ? screen : (SDL_Color){73, 78, 77, 255},
             x + 14.0f, y + 22.0f, 3.0f, 2.0f);
  color_rect(r, active ? screen : (SDL_Color){73, 78, 77, 255},
             x + 19.0f, y + 22.0f, 3.0f, 2.0f);
  draw_text(r, x + (active ? 7.0f : 9.0f), y + 25.0f, 0.55f,
            screen.r, screen.g, screen.b,
            active ? (hacked ? "OPEN" : "LIVE") : "OFF");
}

static void draw_alarm_switch(SDL_Renderer *r, float x, float y,
                              bool alarm_active, bool source,
                              bool being_used, float world_t)
{
  bool flash = ((int)(world_t * 5.0f) & 1) == 0;
  SDL_Color signal = alarm_active
                         ? (flash ? (SDL_Color){255, 76, 58, 255}
                                  : (SDL_Color){150, 29, 27, 255})
                         : (being_used ? COL_AMBER
                                       : (SDL_Color){108, 44, 40, 255});

  if (alarm_active || being_used)
    draw_soft_glow(r, x + 10.0f, y + 8.0f, 12.0f, 15.0f, signal);
  if (alarm_active && flash)
  {
    fx_glow(r, x + 16.0f, y + 7.0f, source ? 30.0f : 22.0f,
            (SDL_Color){255, 48, 38, 255}, source ? 105 : 65);
    fx_light_cone(r, x + 16.0f, y + 7.0f, 3.0f, 24.0f, 54.0f,
                  (SDL_Color){255, 54, 42, 255}, source ? 54 : 34);
  }

  /* Compact wall-mounted call point: a shallow metal housing, one status
   * lamp and a thumb-sized recessed button. */
  color_rect(r, (SDL_Color){16, 20, 23, 170},
             x + 11.0f, y + 9.0f, 13.0f, 16.0f);
  color_rect(r, COL_INK, x + 10.0f, y + 7.0f, 12.0f, 16.0f);
  color_rect(r, (SDL_Color){68, 76, 77, 255},
             x + 11.0f, y + 8.0f, 10.0f, 14.0f);
  color_rect(r, (SDL_Color){211, 162, 45, 255},
             x + 11.0f, y + 8.0f, 10.0f, 2.0f);
  color_rect(r, signal, x + 15.0f, y + 10.0f, 3.0f, 2.0f);
  color_rect(r, (SDL_Color){23, 26, 29, 255},
             x + 13.0f, y + 13.0f, 6.0f, 7.0f);
  color_rect(r, signal, x + 14.0f, y + 14.0f, 4.0f, 5.0f);
  color_rect(r, (SDL_Color){255, 151, 102, 255},
             x + 15.0f, y + 15.0f, 2.0f, 1.0f);
  color_rect(r, (SDL_Color){31, 36, 38, 255},
             x + 15.0f, y + 23.0f, 2.0f, 7.0f);
}

static void draw_office_chair(SDL_Renderer *r, float x, float y)
{
  /* Low-backed swivel chair, kept cool and subdued so actors read over it. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 4.0f, y + 29.0f, 25.0f, 2.0f);

  color_rect(r, COL_INK, x + 5.0f, y + 4.0f, 17.0f, 15.0f);
  color_rect(r, (SDL_Color){43, 56, 68, 255}, x + 7.0f, y + 6.0f, 13.0f, 11.0f);
  color_rect(r, (SDL_Color){67, 84, 96, 255}, x + 8.0f, y + 7.0f, 11.0f, 2.0f);
  color_rect(r, (SDL_Color){28, 37, 48, 255}, x + 8.0f, y + 15.0f, 11.0f, 2.0f);

  color_rect(r, COL_INK, x + 7.0f, y + 17.0f, 20.0f, 6.0f);
  color_rect(r, (SDL_Color){61, 77, 88, 255}, x + 9.0f, y + 18.0f, 16.0f, 3.0f);
  color_rect(r, (SDL_Color){93, 108, 116, 255}, x + 10.0f, y + 18.0f, 14.0f, 1.0f);

  color_rect(r, (SDL_Color){18, 25, 34, 255}, x + 15.0f, y + 22.0f, 4.0f, 7.0f);
  color_rect(r, (SDL_Color){87, 101, 108, 255}, x + 16.0f, y + 22.0f, 2.0f, 6.0f);
  color_rect(r, COL_INK, x + 8.0f, y + 27.0f, 18.0f, 3.0f);
  color_rect(r, (SDL_Color){76, 88, 94, 255}, x + 10.0f, y + 27.0f, 14.0f, 1.0f);
  color_rect(r, COL_INK, x + 7.0f, y + 29.0f, 5.0f, 3.0f);
  color_rect(r, COL_INK, x + 23.0f, y + 29.0f, 5.0f, 3.0f);
}

static void draw_office_desk(SDL_Renderer *r, float x, float y)
{
  /* Compact workstation: desk, drawer, monitor and keyboard in one tile. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 1.0f, y + 30.0f, 31.0f, 2.0f);

  color_rect(r, COL_INK, x + 8.0f, y + 1.0f, 19.0f, 14.0f);
  color_rect(r, (SDL_Color){37, 51, 61, 255}, x + 10.0f, y + 3.0f, 15.0f, 10.0f);
  color_rect(r, (SDL_Color){41, 119, 124, 255}, x + 11.0f, y + 4.0f, 13.0f, 8.0f);
  color_rect(r, (SDL_Color){92, 196, 190, 255}, x + 12.0f, y + 5.0f, 7.0f, 1.0f);
  color_rect(r, (SDL_Color){21, 74, 80, 255}, x + 12.0f, y + 8.0f, 10.0f, 1.0f);
  color_rect(r, (SDL_Color){21, 74, 80, 255}, x + 12.0f, y + 10.0f, 6.0f, 1.0f);
  color_rect(r, (SDL_Color){91, 104, 108, 255}, x + 16.0f, y + 14.0f, 4.0f, 3.0f);

  color_rect(r, COL_INK, x, y + 16.0f, 32.0f, 6.0f);
  color_rect(r, (SDL_Color){91, 69, 49, 255}, x + 1.0f, y + 17.0f, 30.0f, 4.0f);
  color_rect(r, (SDL_Color){150, 111, 66, 255}, x + 2.0f, y + 17.0f, 28.0f, 1.0f);
  color_rect(r, (SDL_Color){25, 32, 38, 255}, x + 20.0f, y + 14.0f, 9.0f, 2.0f);
  color_rect(r, (SDL_Color){99, 109, 108, 255}, x + 21.0f, y + 14.0f, 7.0f, 1.0f);

  color_rect(r, COL_INK, x + 3.0f, y + 21.0f, 5.0f, 11.0f);
  color_rect(r, (SDL_Color){70, 54, 43, 255}, x + 4.0f, y + 21.0f, 3.0f, 10.0f);
  color_rect(r, COL_INK, x + 25.0f, y + 21.0f, 5.0f, 11.0f);
  color_rect(r, (SDL_Color){70, 54, 43, 255}, x + 26.0f, y + 21.0f, 3.0f, 10.0f);
  color_rect(r, (SDL_Color){45, 49, 49, 255}, x + 21.0f, y + 22.0f, 7.0f, 6.0f);
  color_rect(r, (SDL_Color){93, 101, 98, 255}, x + 23.0f, y + 24.0f, 3.0f, 1.0f);
}

static void draw_office_equipment(SDL_Renderer *r, float x, float y,
                                  unsigned variant, float world_t)
{
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 3.0f, y + 30.0f, 27.0f, 2.0f);

  if (variant == 0u)
  {
    /* Filing cabinet with alternating label holders and recessed handles. */
    color_rect(r, COL_INK, x + 5.0f, y + 2.0f, 23.0f, 30.0f);
    color_rect(r, (SDL_Color){61, 72, 78, 255}, x + 7.0f, y + 4.0f, 19.0f, 27.0f);
    color_rect(r, (SDL_Color){100, 112, 113, 255}, x + 8.0f, y + 5.0f, 17.0f, 2.0f);
    for (int drawer = 0; drawer < 3; ++drawer)
    {
      float dy = y + 7.0f + drawer * 8.0f;
      color_rect(r, (SDL_Color){34, 43, 49, 255}, x + 8.0f, dy, 17.0f, 7.0f);
      color_rect(r, (SDL_Color){76, 88, 91, 255}, x + 9.0f, dy + 1.0f, 15.0f, 5.0f);
      color_rect(r, (SDL_Color){20, 27, 31, 255}, x + 13.0f, dy + 2.0f, 7.0f, 2.0f);
      color_rect(r, (SDL_Color){145, 151, 137, 255}, x + 15.0f, dy + 2.0f, 3.0f, 1.0f);
    }
  }
  else if (variant == 1u)
  {
    /* Floor-standing copier/printer with a sheet left in the output tray. */
    color_rect(r, COL_INK, x + 3.0f, y + 12.0f, 27.0f, 20.0f);
    color_rect(r, (SDL_Color){75, 83, 83, 255}, x + 5.0f, y + 14.0f, 23.0f, 17.0f);
    color_rect(r, COL_INK, x + 7.0f, y + 7.0f, 20.0f, 9.0f);
    color_rect(r, (SDL_Color){113, 119, 114, 255}, x + 9.0f, y + 8.0f, 16.0f, 6.0f);
    color_rect(r, (SDL_Color){37, 48, 52, 255}, x + 6.0f, y + 17.0f, 21.0f, 5.0f);
    color_rect(r, (SDL_Color){204, 207, 187, 255}, x + 10.0f, y + 19.0f, 13.0f, 5.0f);
    color_rect(r, (SDL_Color){150, 157, 145, 255}, x + 11.0f, y + 20.0f, 11.0f, 1.0f);
    color_rect(r, (SDL_Color){50, 189, 155, 255}, x + 24.0f, y + 15.0f, 2.0f, 2.0f);
    color_rect(r, (SDL_Color){35, 42, 43, 255}, x + 8.0f, y + 27.0f, 17.0f, 2.0f);
  }
  else
  {
    /* Small office server/network rack with asynchronous status lights. */
    color_rect(r, COL_INK, x + 5.0f, y + 1.0f, 23.0f, 31.0f);
    color_rect(r, (SDL_Color){39, 49, 58, 255}, x + 7.0f, y + 3.0f, 19.0f, 28.0f);
    color_rect(r, (SDL_Color){83, 96, 103, 255}, x + 8.0f, y + 4.0f, 17.0f, 2.0f);
    for (int unit = 0; unit < 4; ++unit)
    {
      float uy = y + 7.0f + unit * 6.0f;
      color_rect(r, (SDL_Color){15, 22, 29, 255}, x + 9.0f, uy, 15.0f, 4.0f);
      color_rect(r, (SDL_Color){49, 66, 73, 255}, x + 10.0f, uy + 1.0f, 8.0f, 1.0f);
      bool blink = ((int)(world_t * (2.0f + unit * 0.31f)) + unit) % 3 != 0;
      SDL_Color led = blink ? (SDL_Color){61, 226, 161, 255}
                            : (SDL_Color){35, 76, 65, 255};
      color_rect(r, led, x + 20.0f, uy + 1.0f, 2.0f, 2.0f);
    }
  }
}

static void draw_decoration(SDL_Renderer *r, const Decoration *decoration,
                            float cam_x, float oy, float world_t)
{
  float x = decoration->col * (float)TILE_SIZE - cam_x;
  float y = decoration->row * (float)TILE_SIZE + oy;
  if (decoration->type == DECOR_OFFICE_CHAIR)
    draw_office_chair(r, x, y);
  else if (decoration->type == DECOR_OFFICE_DESK)
    draw_office_desk(r, x, y);
  else
    draw_office_equipment(r, x, y,
                          tile_hash(decoration->col, decoration->row) % 3u,
                          world_t);
}

static void draw_card(SDL_Renderer *r, float x, float y, Uint8 alpha, bool active)
{
  SDL_Color glow = active ? (SDL_Color){71, 255, 225, 255}
                          : (SDL_Color){46, 181, 190, 255};
  SDL_BlendMode old = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &old);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, glow.r, glow.g, glow.b, (Uint8)(alpha / 5));
  fill_rect(r, x - 4.0f, y - 4.0f, 22.0f, 26.0f);
  set_rgba(r, 8, 24, 30, alpha);
  fill_rect(r, x - 1.0f, y - 1.0f, 16.0f, 20.0f);
  set_rgba(r, glow.r, glow.g, glow.b, alpha);
  fill_rect(r, x, y, 14.0f, 18.0f);
  set_rgba(r, 10, 81, 92, alpha);
  fill_rect(r, x + 2.0f, y + 3.0f, 10.0f, 12.0f);
  set_rgba(r, 211, 255, 245, alpha);
  fill_rect(r, x + 3.0f, y + 4.0f, 5.0f, 4.0f);
  fill_rect(r, x + 3.0f, y + 11.0f, 8.0f, 2.0f);
  set_rgba(r, 255, 225, 90, alpha);
  fill_rect(r, x + 9.0f, y + 4.0f, 2.0f, 4.0f);
  SDL_SetRenderDrawBlendMode(r, old);
}

static void draw_gun_pickup(SDL_Renderer *r, float x, float y)
{
  draw_soft_glow(r, x + 2.0f, y + 1.0f, 15.0f, 13.0f, COL_AMBER);
  color_rect(r, COL_OUTLINE, x + 2.0f, y, 17.0f, 7.0f);
  color_rect(r, (SDL_Color){142, 157, 158, 255}, x + 3.0f, y + 1.0f, 15.0f, 4.0f);
  color_rect(r, (SDL_Color){218, 227, 214, 255}, x + 5.0f, y + 1.0f, 12.0f, 1.0f);
  color_rect(r, COL_OUTLINE, x + 6.0f, y + 5.0f, 7.0f, 10.0f);
  color_rect(r, (SDL_Color){91, 76, 57, 255}, x + 8.0f, y + 6.0f, 4.0f, 8.0f);
}

static void draw_bazooka_pickup(SDL_Renderer *r, float x, float y)
{
  draw_soft_glow(r, x + 3.0f, y + 4.0f, 21.0f, 13.0f,
                 (SDL_Color){112, 205, 93, 255});
  color_rect(r, COL_OUTLINE, x - 1.0f, y + 5.0f, 23.0f, 8.0f);
  color_rect(r, (SDL_Color){55, 77, 48, 255}, x, y + 6.0f, 20.0f, 6.0f);
  color_rect(r, (SDL_Color){100, 129, 72, 255}, x + 2.0f, y + 7.0f, 15.0f, 2.0f);
  color_rect(r, (SDL_Color){155, 167, 120, 255}, x + 19.0f, y + 4.0f, 4.0f, 10.0f);
  color_rect(r, COL_OUTLINE, x + 6.0f, y + 12.0f, 6.0f, 5.0f);
  color_rect(r, (SDL_Color){83, 68, 47, 255}, x + 8.0f, y + 12.0f, 3.0f, 4.0f);
}

static void draw_grenade(SDL_Renderer *r, float x, float y, float fuse)
{
  color_rect(r, COL_OUTLINE, x - 1.0f, y + 1.0f, 12.0f, 10.0f);
  color_rect(r, (SDL_Color){68, 92, 61, 255}, x, y + 2.0f, 10.0f, 8.0f);
  color_rect(r, (SDL_Color){101, 124, 78, 255}, x + 2.0f, y + 2.0f, 3.0f, 7.0f);
  color_rect(r, (SDL_Color){169, 144, 85, 255}, x + 3.0f, y, 5.0f, 3.0f);
  color_rect(r, COL_INK, x + 7.0f, y - 1.0f, 4.0f, 2.0f);
  if (fuse > 0.0f && ((int)(fuse * 14.0f) & 1) == 0)
  {
    color_rect(r, (SDL_Color){255, 235, 128, 255}, x + 10.0f, y - 2.0f, 2.0f, 2.0f);
    color_rect(r, COL_RED, x + 11.0f, y - 1.0f, 2.0f, 2.0f);
  }
}

static void draw_medkit(SDL_Renderer *r, float x, float y)
{
  draw_soft_glow(r, x + 2.0f, y + 2.0f, 16.0f, 16.0f, COL_RED);
  color_rect(r, COL_OUTLINE, x + 1.0f, y + 1.0f, 18.0f, 18.0f);
  color_rect(r, (SDL_Color){197, 48, 48, 255}, x + 2.0f, y + 2.0f, 16.0f, 16.0f);
  color_rect(r, (SDL_Color){244, 82, 65, 255}, x + 4.0f, y + 3.0f, 12.0f, 2.0f);
  color_rect(r, (SDL_Color){244, 239, 210, 255}, x + 8.0f, y + 5.0f, 4.0f, 10.0f);
  color_rect(r, (SDL_Color){244, 239, 210, 255}, x + 5.0f, y + 8.0f, 10.0f, 4.0f);
}

static void draw_spike_strip(SDL_Renderer *r, float x, float y)
{
  color_rect(r, (SDL_Color){25, 28, 31, 255}, x, y + 27.0f, TILE_SIZE, 5.0f);
  for (int k = 0; k < TILE_SIZE; k += 8)
  {
    color_rect(r, COL_OUTLINE, x + k, y + 19.0f, 8.0f, 10.0f);
    color_rect(r, (SDL_Color){134, 147, 147, 255}, x + k + 1.0f, y + 23.0f, 6.0f, 5.0f);
    color_rect(r, (SDL_Color){172, 187, 181, 255}, x + k + 2.0f, y + 19.0f, 4.0f, 9.0f);
    color_rect(r, (SDL_Color){222, 224, 204, 255}, x + k + 3.0f, y + 17.0f, 2.0f, 10.0f);
  }
}

static void draw_fan_segment(SDL_Renderer *r, float x1, float y1,
                             float x2, float y2, int thickness,
                             SDL_Color color)
{
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length = sqrtf(dx * dx + dy * dy);
  float nx = length > 0.001f ? -dy / length : 0.0f;
  float ny = length > 0.001f ? dx / length : 0.0f;
  float half = (float)(thickness - 1) * 0.5f;

  set_color(r, color);
  for (int i = 0; i < thickness; ++i)
  {
    float offset = (float)i - half;
    SDL_RenderLine(r, x1 + nx * offset, y1 + ny * offset,
                   x2 + nx * offset, y2 + ny * offset);
  }
}

static void draw_ceiling_fan_blades(SDL_Renderer *r, float cx, float cy,
                                    float angle, bool front)
{
  for (int blade = 0; blade < 4; ++blade)
  {
    float a = angle + (float)blade * 1.57079633f;
    float projection = cosf(a);
    float depth = sinf(a);
    if ((depth >= 0.0f) != front)
      continue;

    /* A horizontal rotor seen edge-on: depth changes the apparent blade
       length and only nudges it vertically for a hint of perspective. */
    float inner_x = cx + projection * 4.0f;
    float inner_y = cy + depth * 0.4f;
    float outer_x = cx + projection * CEILING_FAN_BLADE_LENGTH;
    float outer_y = cy + depth * 2.0f;
    SDL_Color blade_color = front
                                ? (SDL_Color){177, 191, 187, 255}
                                : (SDL_Color){82, 94, 95, 255};
    draw_fan_segment(r, inner_x, inner_y, outer_x, outer_y, 7, COL_OUTLINE);
    draw_fan_segment(r, inner_x, inner_y, outer_x, outer_y, 3, blade_color);
  }
}

static void draw_ceiling_fan(SDL_Renderer *r, const CeilingFan *fan,
                             float cam_x, float oy, float world_t,
                             int index)
{
  float cx = fan->x - cam_x;
  float cy = fan->y + oy;
  float angle = world_t * 9.0f + (float)index * 0.83f;

  /* Side-view mount: ceiling plate, vertical shaft, and motor housing. */
  color_rect(r, COL_OUTLINE, cx - 8.0f, cy - 11.0f, 16.0f, 5.0f);
  color_rect(r, (SDL_Color){107, 120, 121, 255},
             cx - 6.0f, cy - 10.0f, 12.0f, 3.0f);
  color_rect(r, COL_OUTLINE, cx - 3.0f, cy - 8.0f, 6.0f, 8.0f);
  color_rect(r, (SDL_Color){139, 151, 148, 255},
             cx - 1.0f, cy - 7.0f, 2.0f, 7.0f);

  draw_ceiling_fan_blades(r, cx, cy + 2.0f, angle, false);

  color_rect(r, COL_OUTLINE, cx - 7.0f, cy - 5.0f, 14.0f, 9.0f);
  color_rect(r, (SDL_Color){72, 84, 85, 255},
             cx - 5.0f, cy - 3.0f, 10.0f, 5.0f);
  color_rect(r, (SDL_Color){139, 151, 148, 255},
             cx - 4.0f, cy - 2.0f, 8.0f, 2.0f);

  draw_ceiling_fan_blades(r, cx, cy + 2.0f, angle, true);

  color_rect(r, COL_OUTLINE, cx - 3.0f, cy, 6.0f, 6.0f);
  color_rect(r, (SDL_Color){235, 86, 65, 255},
             cx - 1.0f, cy + 2.0f, 2.0f, 2.0f);
}

static void draw_crate(SDL_Renderer *r, const Crate *crate, float cam_x, float oy)
{
  float x = crate->x - cam_x;
  float y = crate->y + oy;
  color_rect(r, COL_INK, x - 1.0f, y - 1.0f, CRATE_W + 2.0f, CRATE_H + 2.0f);
  color_rect(r, (SDL_Color){105, 67, 38, 255}, x, y, CRATE_W, CRATE_H);
  color_rect(r, (SDL_Color){161, 103, 53, 255}, x + 2.0f, y + 2.0f, CRATE_W - 4.0f, 4.0f);
  color_rect(r, (SDL_Color){73, 48, 32, 255}, x + 2.0f, y + CRATE_H - 6.0f, CRATE_W - 4.0f, 4.0f);
  color_rect(r, (SDL_Color){143, 90, 47, 255}, x + 2.0f, y + 2.0f, 4.0f, CRATE_H - 4.0f);
  color_rect(r, (SDL_Color){70, 47, 33, 255}, x + CRATE_W - 6.0f, y + 2.0f, 4.0f, CRATE_H - 4.0f);
  set_rgba(r, 68, 44, 30, 255);
  SDL_RenderLine(r, x + 6.0f, y + 6.0f, x + CRATE_W - 7.0f, y + CRATE_H - 7.0f);
  SDL_RenderLine(r, x + CRATE_W - 7.0f, y + 6.0f, x + 6.0f, y + CRATE_H - 7.0f);
  set_rgba(r, 188, 125, 67, 255);
  SDL_RenderLine(r, x + 7.0f, y + 6.0f, x + CRATE_W - 7.0f, y + CRATE_H - 8.0f);
  color_rect(r, (SDL_Color){178, 171, 133, 255}, x + 11.0f, y + 10.0f, 8.0f, 7.0f);
  draw_text(r, x + 12.0f, y + 11.0f, 0.55f, 74, 62, 45, "X");
}

static void draw_mine(SDL_Renderer *r, const Mine *mine, float cam_x, float oy)
{
  float x = mine->x - cam_x;
  float y = mine->y + oy;
  bool flash = mine->triggered && (((int)(mine->timer * 24.0f) & 1) == 0);
  if (flash)
    draw_soft_glow(r, x + 5.0f, y, 6.0f, 5.0f, COL_RED);
  color_rect(r, COL_INK, x - 2.0f, y + 4.0f, MINE_W + 4.0f, 6.0f);
  color_rect(r, (SDL_Color){45, 51, 48, 255}, x, y + 2.0f, MINE_W, 7.0f);
  color_rect(r, (SDL_Color){91, 100, 89, 255}, x + 2.0f, y + 1.0f, MINE_W - 4.0f, 3.0f);
  color_rect(r, flash ? (SDL_Color){255, 230, 167, 255} : COL_RED,
             x + 6.0f, y + 1.0f, 4.0f, 3.0f);
  color_rect(r, (SDL_Color){157, 146, 90, 255}, x + 1.0f, y + 7.0f, 3.0f, 2.0f);
  color_rect(r, (SDL_Color){157, 146, 90, 255}, x + 12.0f, y + 7.0f, 3.0f, 2.0f);
}

static void draw_gas_canister(SDL_Renderer *r,
                              const GasCanister *canister,
                              float cam_x, float oy)
{
  float x = canister->x - cam_x;
  float y = canister->y + oy;

  /* A narrow pressure cylinder with stepped shoulders and a brass valve.
     The silhouette stays below a standing shot while remaining readable
     against the dark industrial background. */
  color_rect(r, (SDL_Color){3, 6, 9, 120},
             x - 2.0f, y + 14.0f, GAS_CANISTER_W + 4.0f, 3.0f);

  /* Valve, collar and short neck. */
  color_rect(r, COL_INK, x + 4.0f, y, 5.0f, 3.0f);
  color_rect(r, (SDL_Color){201, 174, 93, 255},
             x + 5.0f, y, 3.0f, 2.0f);
  color_rect(r, COL_INK, x + 3.0f, y + 2.0f, 6.0f, 4.0f);
  color_rect(r, (SDL_Color){118, 126, 116, 255},
             x + 4.0f, y + 3.0f, 4.0f, 3.0f);

  /* Rounded shoulders and long cylindrical body. */
  color_rect(r, COL_INK, x + 2.0f, y + 5.0f, 8.0f, 2.0f);
  color_rect(r, COL_INK, x + 1.0f, y + 6.0f, 10.0f, 9.0f);
  color_rect(r, COL_INK, x + 2.0f, y + 15.0f, 8.0f, 1.0f);
  color_rect(r, (SDL_Color){224, 70, 53, 255},
             x + 3.0f, y + 6.0f, 6.0f, 1.0f);
  color_rect(r, (SDL_Color){92, 103, 103, 255},
             x + 2.0f, y + 7.0f, 8.0f, 7.0f);
  color_rect(r, (SDL_Color){51, 61, 63, 255},
             x + 2.0f, y + 14.0f, 8.0f, 1.0f);
  color_rect(r, (SDL_Color){153, 163, 158, 255},
             x + 3.0f, y + 9.0f, 2.0f, 5.0f);

  /* A red shoulder identifies the flammable contents without making the
     whole cylinder resemble a fire extinguisher. */
  color_rect(r, (SDL_Color){174, 48, 41, 255},
             x + 2.0f, y + 7.0f, 8.0f, 2.0f);
  color_rect(r, (SDL_Color){237, 80, 58, 255},
             x + 3.0f, y + 7.0f, 2.0f, 2.0f);

  /* Small warning label with a dark gas-flame mark. */
  color_rect(r, (SDL_Color){239, 188, 48, 255},
             x + 5.0f, y + 10.0f, 4.0f, 3.0f);
  color_rect(r, (SDL_Color){70, 39, 27, 255},
             x + 6.0f, y + 10.0f, 2.0f, 2.0f);
  color_rect(r, (SDL_Color){255, 225, 103, 255},
             x + 5.0f, y + 10.0f, 1.0f, 2.0f);
}

/* Draw a local sprite rectangle, mirrored inside the entity collision width. */
static void sprite_rect(SDL_Renderer *r, float bx, float by, float sprite_w, int dir,
                        float lx, float ly, float w, float h, SDL_Color c)
{
  float x = (dir >= 0) ? bx + lx : bx + sprite_w - lx - w;
  color_rect(r, c, floorf(x), floorf(by + ly), w, h);
}

static void draw_rocket_sprite(SDL_Renderer *r, float x, float y, int dir,
                               bool flame)
{
  sprite_rect(r, x, y, ROCKET_W, dir, 0.0f, 1.0f,
              12.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ROCKET_W, dir, 2.0f, 2.0f,
              10.0f, 3.0f, (SDL_Color){112, 132, 91, 255});
  sprite_rect(r, x, y, ROCKET_W, dir, 12.0f, 2.0f,
              4.0f, 3.0f, (SDL_Color){213, 187, 111, 255});
  sprite_rect(r, x, y, ROCKET_W, dir, 1.0f, 0.0f,
              4.0f, 2.0f, (SDL_Color){73, 91, 65, 255});
  sprite_rect(r, x, y, ROCKET_W, dir, 1.0f, 5.0f,
              4.0f, 1.0f, (SDL_Color){73, 91, 65, 255});
  if (flame)
  {
    sprite_rect(r, x, y, ROCKET_W, dir, -5.0f, 1.0f,
                5.0f, 5.0f, (SDL_Color){226, 70, 38, 210});
    sprite_rect(r, x, y, ROCKET_W, dir, -3.0f, 2.0f,
                4.0f, 3.0f, (SDL_Color){255, 218, 91, 255});
  }
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
    sprite_rect(r, x, y, sprite_w, dir, lx - 8.0f + recoil, ly + 1.0f,
                6.0f, 7.0f, (SDL_Color){223, 65, 38, 190});
    sprite_rect(r, x, y, sprite_w, dir, lx - 5.0f + recoil, ly + 3.0f,
                4.0f, 3.0f, (SDL_Color){255, 218, 94, 255});
  }
}

/* Thick local-space line used for jointed pixel-art limbs. */
static void sprite_segment(SDL_Renderer *r, float bx, float by, float sprite_w, int dir,
                           float lx1, float ly1, float lx2, float ly2,
                           int thickness, SDL_Color c)
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

  set_color(r, c);
  for (int i = 0; i < thickness; ++i)
  {
    float offset = (float)i - half;
    SDL_RenderLine(r,
                   floorf(x1 + nx * offset), floorf(y1 + ny * offset),
                   floorf(x2 + nx * offset), floorf(y2 + ny * offset));
  }
}

static void sprite_limb_segment(SDL_Renderer *r, float bx, float by,
                                float sprite_w, int dir,
                                float lx1, float ly1, float lx2, float ly2,
                                SDL_Color fill)
{
  sprite_segment(r, bx, by, sprite_w, dir, lx1, ly1, lx2, ly2, 6, COL_OUTLINE);
  sprite_segment(r, bx, by, sprite_w, dir, lx1, ly1, lx2, ly2, 3, fill);
}

static void draw_walking_leg(SDL_Renderer *r, float x, float y, float sprite_w,
                             int dir, float hip_x, float hip_y, float stride,
                             SDL_Color trouser, SDL_Color boot)
{
  /* Keep the gait mostly under the torso.  The lift and knee bend carry the
     animation; a restrained horizontal reach avoids a wide cowboy stance. */
  float lift = fmaxf(0.0f, stride) * 0.48f;
  float knee_x = hip_x + stride * 0.30f;
  float knee_y = 26.0f - lift * 0.25f;
  float ankle_x = hip_x + stride * 0.62f;
  float ankle_y = 30.0f - lift;

  sprite_limb_segment(r, x, y, sprite_w, dir,
                      hip_x, hip_y, knee_x, knee_y, trouser);
  sprite_limb_segment(r, x, y, sprite_w, dir,
                      knee_x, knee_y, ankle_x, ankle_y, trouser);
  sprite_rect(r, x, y, sprite_w, dir,
              ankle_x - 1.5f, ankle_y - 1.0f, 7.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir,
              ankle_x - 0.5f, ankle_y, 5.0f, 2.0f, boot);
}

static void draw_walking_arm(SDL_Renderer *r, float x, float y, float sprite_w,
                             int dir, float shoulder_x, float shoulder_y,
                             float swing, SDL_Color upper, SDL_Color lower)
{
  /* In side view both arms share the same visible shoulder pivot near the
     centre of the upper torso.  Only the elbow and hand counter-swing. */
  float elbow_x = shoulder_x + swing * 1.25f;
  float hand_x = shoulder_x + swing * 3.0f;
  float elbow_y = shoulder_y + 4.5f;
  float hand_y = shoulder_y + 9.0f;

  sprite_limb_segment(r, x, y, sprite_w, dir,
                      shoulder_x, shoulder_y, elbow_x, elbow_y, upper);
  sprite_limb_segment(r, x, y, sprite_w, dir,
                      elbow_x, elbow_y, hand_x, hand_y, lower);
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

static void draw_player_crawling(SDL_Renderer *r, const Player *p, float x, float y)
{
  int dir = p->facing;
  float phase = p->anim_time * 3.2f;
  float shove = (fabsf(p->vx) > 1.0f) ? sinf(phase) * 2.0f : 0.0f;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  bool firing = p->action_timer > 0.0f && !knife;
  bool bazooka = p->bazooka_rockets > 0 ||
                 (firing && p->bazooka_firing);

  /* Ground shadow and rear boot. */
  color_rect(r, (SDL_Color){3, 6, 10, 130}, x + 2.0f, y + 16.0f, 24.0f, 2.0f);
  sprite_rect(r, x, y, PLAYER_W, dir, 1.0f - shove, 12.0f, 8.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 2.0f - shove, 12.0f, 7.0f, 3.0f, (SDL_Color){26, 48, 72, 255});

  /* Horizontal torso, shoulder plate and head at the leading edge. */
  sprite_rect(r, x, y, PLAYER_W, dir, 6.0f, 6.0f, 13.0f, 10.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 7.0f, 7.0f, 12.0f, 8.0f, (SDL_Color){38, 100, 139, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 7.0f, 4.0f, 8.0f, (SDL_Color){62, 143, 169, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, 3.0f, 8.0f, 9.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 18.0f, 4.0f, 6.0f, 7.0f, (SDL_Color){209, 154, 106, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 18.0f, 3.0f, 7.0f, 3.0f, (SDL_Color){74, 39, 28, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 6.0f, 2.0f, 2.0f, (SDL_Color){220, 239, 219, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 5.0f, 4.0f, 2.0f, COL_RED);

  /* Braced front arm with either the sidearm or an empty-ammo knife thrust. */
  sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 11.0f, 7.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, 11.0f, 6.0f, 2.0f, (SDL_Color){209, 154, 106, 255});
  if (knife)
  {
    float thrust = p->action_timer > PLAYER_KNIFE_ACTION_TIME * 0.5f ? 2.0f : 0.0f;
    sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 10.0f, 4.0f + thrust, 4.0f,
                (SDL_Color){209, 154, 106, 255});
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
  else if (p->bullets > 0 || firing)
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 10.0f,
                firing ? 7.0f : 5.0f, 3.0f,
                (SDL_Color){36, 43, 48, 255});
    if (firing && p->action_timer > 0.055f)
    {
      sprite_rect(r, x, y, PLAYER_W, dir, 29.0f, 9.0f, 3.0f, 5.0f, COL_AMBER);
      sprite_rect(r, x, y, PLAYER_W, dir, 32.0f, 10.0f, 2.0f, 3.0f, (SDL_Color){255, 239, 165, 255});
    }
  }
}

static void draw_player_hacking(SDL_Renderer *r, float x, float y,
                                float hack_time)
{
  const SDL_Color shirt = {35, 102, 142, 255};
  const SDL_Color shirt_light = {60, 148, 171, 255};
  const SDL_Color trousers = {28, 63, 92, 255};
  const SDL_Color boots = {15, 24, 35, 255};
  const SDL_Color skin = {209, 154, 106, 255};
  float type_phase = hack_time * 15.0f;
  float tap_a = sinf(type_phase) * 1.2f;
  float tap_b = sinf(type_phase + 3.14159265f) * 1.2f;
  float bob = sinf(hack_time * 5.0f) * 0.25f;

  /* Rear view: Chuck faces the wall-mounted terminal, so the camera sees
     the back of his head, shoulders and torso. */
  color_rect(r, (SDL_Color){3, 6, 10, 125},
             x + 3.0f, y + 30.0f, 22.0f, 3.0f);
  sprite_rect(r, x, y, PLAYER_W, 1,
              7.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1,
              8.0f, 23.0f, 4.0f, 7.0f, trousers);
  sprite_rect(r, x, y, PLAYER_W, 1,
              6.0f, 29.0f, 8.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1,
              7.0f, 30.0f, 7.0f, 2.0f, boots);
  sprite_rect(r, x, y, PLAYER_W, 1,
              13.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1,
              14.0f, 23.0f, 4.0f, 7.0f, (SDL_Color){35, 78, 105, 255});
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
  sprite_rect(r, x, y, PLAYER_W, 1,
              7.0f, 10.0f + bob, 15.0f, 14.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1,
              8.0f, 11.0f + bob, 13.0f, 12.0f, shirt);
  sprite_rect(r, x, y, PLAYER_W, 1,
              8.0f, 12.0f + bob, 4.0f, 8.0f, shirt_light);
  sprite_rect(r, x, y, PLAYER_W, 1,
              17.0f, 12.0f + bob, 4.0f, 8.0f, shirt_light);
  sprite_rect(r, x, y, PLAYER_W, 1,
              13.0f, 12.0f + bob, 3.0f, 11.0f,
              (SDL_Color){21, 54, 76, 255});
  sprite_rect(r, x, y, PLAYER_W, 1,
              9.0f, 20.0f + bob, 11.0f, 2.0f, COL_AMBER);

  /* Back of the head: no face or eye is visible from this angle. */
  sprite_rect(r, x, y, PLAYER_W, 1,
              9.0f, 1.0f + bob, 10.0f, 11.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, 1,
              10.0f, 2.0f + bob, 8.0f, 9.0f,
              (SDL_Color){70, 38, 28, 255});
  sprite_rect(r, x, y, PLAYER_W, 1,
              12.0f, 0.0f + bob, 6.0f, 3.0f,
              (SDL_Color){91, 48, 31, 255});
  sprite_rect(r, x, y, PLAYER_W, 1,
              8.0f, 4.0f + bob, 12.0f, 2.0f, COL_RED);
  sprite_rect(r, x, y, PLAYER_W, 1,
              9.0f, 7.0f + bob, 2.0f, 2.0f,
              (SDL_Color){91, 48, 31, 255});
  sprite_rect(r, x, y, PLAYER_W, 1,
              17.0f, 7.0f + bob, 2.0f, 2.0f,
              (SDL_Color){91, 48, 31, 255});

  /* His hands are between his body and the terminal, so they are hidden
     from this rear angle; only the alternating elbow motion is visible. */
}

static void draw_player(SDL_Renderer *r, const Player *p, float cam_x, float oy,
                        bool hacking, float hacking_time)
{
  float x = p->x - cam_x;
  float y = p->y + oy;
  int dir = p->facing;

  if (hacking)
  {
    draw_player_hacking(r, x, y, hacking_time);
    return;
  }

  if (p->crawling)
  {
    draw_player_crawling(r, p, x, y);
    return;
  }

  float phase = p->anim_time * 3.0f;
  bool moving = fabsf(p->vx) > 2.0f;
  bool climbing = p->on_ladder;
  /* A back-facing ladder pose must not inherit or mirror the last walk direction. */
  if (climbing)
    dir = 1;
  bool airborne = !p->on_ground && !climbing;
  bool knife = p->action_timer > 0.0f && p->knife_attacking;
  bool firing = p->action_timer > 0.0f && !knife;
  bool bazooka = p->bazooka_rockets > 0 ||
                 (firing && p->bazooka_firing);
  float step = moving && p->on_ground ? sinf(phase) : 0.0f;
  float bob = moving && p->on_ground ? fabsf(step) * 0.55f
                                     : sinf(p->anim_time * 2.0f) * 0.35f;
  float arm_swing = -step;
  float climb = 0.0f;

  color_rect(r, (SDL_Color){3, 6, 10, 125}, x + 3.0f, y + 30.0f, 20.0f, 3.0f);

  if (climbing)
  {
    climb = sinf(phase) * 4.0f;
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 13.0f - climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 13.0f, 13.0f + climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 14.0f - climb, 3.0f, 8.0f, (SDL_Color){51, 130, 159, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 14.0f, 14.0f + climb, 3.0f, 8.0f, (SDL_Color){51, 130, 159, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 23.0f + climb, 5.0f, 8.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 13.0f, 23.0f - climb, 5.0f, 8.0f, COL_OUTLINE);
    bob = fabsf(sinf(phase)) * 0.7f;
  }
  else
  {
    if (moving && p->on_ground)
    {
      draw_walking_leg(r, x, y, PLAYER_W, dir, 12.0f, 21.0f + bob,
                       -step * 3.4f, (SDL_Color){25, 57, 82, 255},
                       (SDL_Color){12, 20, 30, 255});
      draw_walking_leg(r, x, y, PLAYER_W, dir, 14.0f, 21.0f + bob,
                       step * 3.4f, (SDL_Color){38, 82, 111, 255},
                       (SDL_Color){18, 28, 39, 255});
    }
    else
    {
      float leg_a_y = airborne ? 24.0f : 22.0f;
      float leg_b_y = airborne ? 21.0f : 22.0f;
      float leg_a_h = airborne ? 7.0f : 10.0f;
      float leg_b_h = airborne ? 7.0f : 10.0f;
      sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, leg_a_y, 6.0f, leg_a_h, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir, 13.0f, leg_b_y, 6.0f, leg_b_h, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, leg_a_y, 4.0f, fmaxf(3.0f, leg_a_h - 2.0f), (SDL_Color){28, 63, 92, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 14.0f, leg_b_y, 4.0f, fmaxf(3.0f, leg_b_h - 2.0f), (SDL_Color){35, 78, 105, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 7.0f, leg_a_y + leg_a_h - 3.0f, 7.0f, 3.0f, (SDL_Color){15, 24, 35, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 13.0f, leg_b_y + leg_b_h - 3.0f, 7.0f, 3.0f, (SDL_Color){15, 24, 35, 255});
    }
  }

  /* Rear arm passes behind the torso and counter-swings against the legs. */
  if (!climbing && !firing && !knife)
  {
    draw_walking_arm(r, x, y, PLAYER_W, dir, 14.0f, 13.0f + bob,
                     -arm_swing, (SDL_Color){35, 102, 142, 255},
                     (SDL_Color){189, 132, 91, 255});
  }

  /* Torso, webbing and shoulder. */
  sprite_rect(r, x, y, PLAYER_W, dir, 6.0f, 10.0f + bob, 15.0f, 14.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 7.0f, 11.0f + bob, 13.0f, 12.0f, (SDL_Color){35, 102, 142, 255});
  if (climbing)
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 7.0f, 12.0f + bob, 4.0f, 8.0f, (SDL_Color){48, 125, 157, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 12.0f + bob, 4.0f, 8.0f, (SDL_Color){48, 125, 157, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 12.0f + bob, 3.0f, 11.0f, (SDL_Color){21, 54, 76, 255});
  }
  else
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 12.0f + bob, 4.0f, 9.0f, (SDL_Color){60, 148, 171, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 12.0f + bob, 2.0f, 11.0f, (SDL_Color){21, 54, 76, 255});
  }
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 20.0f + bob, 11.0f, 2.0f, COL_AMBER);

  if (climbing)
  {
    if (knife)
    {
      /* One hand keeps its grip while the other stabs sideways.  The ladder
         pose remains rear-facing, so only the attacking arm is mirrored. */
      if (p->facing > 0)
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          8.0f, 14.0f + bob, 6.5f, 5.0f - climb,
                          (SDL_Color){42, 118, 153, 255},
                          (SDL_Color){209, 154, 105, 255});
      }
      else
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          18.0f, 14.0f + bob, 19.5f, 5.0f + climb,
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
                  20.0f + thrust, 13.0f + bob, 6.0f, 5.0f, COL_OUTLINE);
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
    else if (firing && p->shot_vertical == 0)
    {
      /* Horizontal ladder fire uses the stored facing direction while the
         body remains turned toward the ladder. */
      if (p->facing > 0)
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          8.0f, 14.0f + bob, 6.5f, 5.0f - climb,
                          (SDL_Color){42, 118, 153, 255},
                          (SDL_Color){209, 154, 105, 255});
      }
      else
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          18.0f, 14.0f + bob, 19.5f, 5.0f + climb,
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
        if (p->action_timer > 0.055f)
        {
          sprite_rect(r, x, y, PLAYER_W, gun_dir,
                      34.0f + recoil, 11.0f + bob, 4.0f, 6.0f, COL_AMBER);
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
                        8.0f, 14.0f + bob, 6.5f, 5.0f - climb,
                        (SDL_Color){42, 118, 153, 255},
                        (SDL_Color){209, 154, 105, 255});
      if (firing && p->shot_vertical != 0)
      {
        float hand_y = p->shot_vertical < 0 ? 8.0f : 20.0f;
        float gun_y = p->shot_vertical < 0 ? 1.0f : 18.0f;
        sprite_limb_segment(r, x, y, PLAYER_W, dir,
                            18.0f, 14.0f + bob, 21.0f, hand_y,
                            (SDL_Color){42, 118, 153, 255});
        sprite_rect(r, x, y, PLAYER_W, dir,
                    19.0f, hand_y - 2.0f, 5.0f, 5.0f, COL_OUTLINE);
        sprite_rect(r, x, y, PLAYER_W, dir,
                    20.0f, hand_y - 1.0f, 3.0f, 3.0f,
                    (SDL_Color){209, 154, 105, 255});
        sprite_rect(r, x, y, PLAYER_W, dir,
                    19.0f, gun_y, 5.0f, 8.0f, (SDL_Color){31, 38, 43, 255});
        if (p->action_timer > 0.055f)
        {
          float flash_y = p->shot_vertical < 0 ? -5.0f : 26.0f;
          sprite_rect(r, x, y, PLAYER_W, dir,
                      18.0f, flash_y, 7.0f, 5.0f, COL_AMBER);
          sprite_rect(r, x, y, PLAYER_W, dir,
                      20.0f,
                      p->shot_vertical < 0 ? flash_y - 3.0f : flash_y + 5.0f,
                      3.0f, 3.0f, (SDL_Color){255, 242, 184, 255});
        }
      }
      else
      {
        draw_climbing_arm(r, x, y, PLAYER_W, dir,
                          18.0f, 14.0f + bob, 19.5f, 5.0f + climb,
                          (SDL_Color){42, 118, 153, 255},
                          (SDL_Color){209, 154, 105, 255});
      }
    }
  }

  /* Face the ladder while climbing; otherwise keep the normal side profile. */
  if (climbing)
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 1.0f + bob, 10.0f, 11.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 10.0f, 2.0f + bob, 8.0f, 9.0f, (SDL_Color){210, 154, 105, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 1.0f + bob, 10.0f, 8.0f, (SDL_Color){70, 38, 28, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 0.0f + bob, 6.0f, 3.0f, (SDL_Color){91, 48, 31, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 10.0f, 7.0f + bob, 2.0f, 2.0f, (SDL_Color){91, 48, 31, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 7.0f + bob, 2.0f, 2.0f, (SDL_Color){91, 48, 31, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 4.0f + bob, 12.0f, 2.0f, COL_RED);
  }
  else
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 1.0f + bob, 10.0f, 11.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 10.0f, 2.0f + bob, 8.0f, 9.0f, (SDL_Color){210, 154, 105, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 1.0f + bob, 10.0f, 4.0f, (SDL_Color){70, 38, 28, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 0.0f + bob, 7.0f, 2.0f, (SDL_Color){91, 48, 31, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 5.0f + bob, 2.0f, 2.0f, (SDL_Color){230, 242, 216, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, 7.0f + bob, 2.0f, 1.0f, (SDL_Color){83, 40, 27, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 4.0f + bob, 12.0f, 2.0f, COL_RED);
    sprite_rect(r, x, y, PLAYER_W, dir, 5.0f, 5.0f + bob, 4.0f, 2.0f, (SDL_Color){166, 38, 42, 255});
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
      if (p->action_timer > 0.055f)
      {
        sprite_rect(r, x, y, PLAYER_W, dir, 31.0f + recoil, 11.0f + bob, 4.0f, 6.0f, COL_AMBER);
        sprite_rect(r, x, y, PLAYER_W, dir, 35.0f + recoil, 13.0f + bob, 3.0f, 3.0f, (SDL_Color){255, 242, 184, 255});
      }
    }
    else
    {
      draw_walking_arm(r, x, y, PLAYER_W, dir, 14.0f, 13.0f + bob,
                       arm_swing, (SDL_Color){42, 118, 153, 255},
                       (SDL_Color){209, 154, 105, 255});
      if (p->bullets > 0)
      {
        float hand_x = 14.0f + arm_swing * 3.0f;
        sprite_rect(r, x, y, PLAYER_W, dir, hand_x, 20.0f + bob,
                    6.0f, 3.0f, (SDL_Color){31, 38, 43, 255});
      }
    }
  }
}

static void draw_janitor(SDL_Renderer *r, const Janitor *janitor,
                         float cam_x, float oy)
{
  float x = janitor->x - cam_x;
  float y = janitor->y + oy;
  int dir = janitor->dir;
  bool walking = janitor->activity == JANITOR_WALK &&
                 fabsf(janitor->vx) > 2.0f;
  bool mopping = janitor->activity == JANITOR_MOP;
  float phase = janitor->anim_time * 2.2f;
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

  /* Compact housekeeping cart, trailing behind the walking direction. */
  float cart_x = dir > 0 ? x - 25.0f : x + JANITOR_W + 3.0f;
  float cart_y = y + 7.0f;
  /* Short contact shadows keep the two silhouettes grounded without joining
   * them into one long, high-contrast stripe. */
  color_rect(r, (SDL_Color){6, 9, 13, 255},
             x + 4.0f, y + 30.0f, 18.0f, 2.0f);
  color_rect(r, (SDL_Color){6, 9, 13, 255},
             cart_x + 2.0f, y + 30.0f, 19.0f, 2.0f);
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
  SDL_RenderLine(r, cart_x + (dir > 0 ? 20.0f : 3.0f), cart_y + 5.0f,
                 cart_x + (dir > 0 ? 24.0f : -1.0f), cart_y - 1.0f);

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

  if (walking)
  {
    draw_walking_leg(r, x, y, JANITOR_W, dir, 12.0f, 21.0f + bob,
                     -step * 2.8f, (SDL_Color){30, 57, 60, 255}, COL_INK);
    draw_walking_leg(r, x, y, JANITOR_W, dir, 14.0f, 21.0f + bob,
                     step * 2.8f, (SDL_Color){36, 68, 70, 255}, COL_INK);
  }
  else
  {
    sprite_rect(r, x, y, JANITOR_W, dir,
                8.0f, 22.0f, 6.0f, 9.0f, (SDL_Color){30, 57, 60, 255});
    sprite_rect(r, x, y, JANITOR_W, dir,
                14.0f, 22.0f, 6.0f, 9.0f, (SDL_Color){36, 68, 70, 255});
    sprite_rect(r, x, y, JANITOR_W, dir,
                7.0f, 29.0f, 7.0f, 3.0f, COL_INK);
    sprite_rect(r, x, y, JANITOR_W, dir,
                14.0f, 29.0f, 7.0f, 3.0f, COL_INK);
  }

  sprite_rect(r, x, y, JANITOR_W, dir,
              6.0f, 10.0f + bob, 15.0f, 14.0f, COL_OUTLINE);
  sprite_rect(r, x, y, JANITOR_W, dir,
              7.0f, 11.0f + bob, 13.0f, 12.0f, uniform);
  sprite_rect(r, x, y, JANITOR_W, dir,
              8.0f, 12.0f + bob, 4.0f, 9.0f, uniform_hi);
  /* A muted service vest keeps the role legible without competing with
   * pickups, enemies, or the player's brighter silhouette. */
  sprite_rect(r, x, y, JANITOR_W, dir,
              11.0f, 11.0f + bob, 3.0f, 12.0f,
              (SDL_Color){139, 118, 63, 255});
  sprite_rect(r, x, y, JANITOR_W, dir,
              8.0f, 20.0f + bob, 12.0f, 2.0f,
              (SDL_Color){139, 118, 63, 255});

  sprite_rect(r, x, y, JANITOR_W, dir,
              9.0f, 2.0f + bob, 10.0f, 10.0f, COL_OUTLINE);
  sprite_rect(r, x, y, JANITOR_W, dir,
              10.0f, 4.0f + bob, 8.0f, 7.0f, skin);
  sprite_rect(r, x, y, JANITOR_W, dir,
              8.0f, 1.0f + bob, 12.0f, 5.0f,
              (SDL_Color){42, 87, 91, 255});
  sprite_rect(r, x, y, JANITOR_W, dir,
              15.0f, 6.0f + bob, 2.0f, 2.0f,
              (SDL_Color){17, 28, 29, 255});

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

static void draw_enemy(SDL_Renderer *r, const Enemy *e, float cam_x, float oy)
{
  float x = e->x - cam_x;
  float y = e->y + oy;
  int dir = e->dir;
  /* The ladder pose faces away from the camera, so left/right mirroring is invalid. */
  if (e->climbing)
    dir = 1;
  bool aiming = e->aim_timer > 0.0f || e->recoil_timer > 0.0f;
  bool using_alarm = e->raising_alarm && e->alarm_use_timer > 0.0f;
  bool moving = fabsf(e->vx) > 2.0f && !aiming && !e->talking;
  float phase = e->anim_time * 3.0f;
  float step = moving ? sinf(phase) : 0.0f;
  float bob = moving ? fabsf(step) * 0.5f : sinf(e->anim_time * 1.8f) * 0.3f;
  float climb = e->climbing ? sinf(phase) * 4.0f : 0.0f;
  SDL_Color uniform = e->hp >= ENEMY_HP ? (SDL_Color){76, 91, 69, 255}
                                           : e->hp == 2 ? (SDL_Color){103, 83, 54, 255}
                                                        : (SDL_Color){101, 65, 49, 255};
  SDL_Color light = e->hp >= ENEMY_HP ? (SDL_Color){116, 129, 86, 255}
                                        : (SDL_Color){135, 98, 58, 255};

  color_rect(r, (SDL_Color){3, 6, 9, 125}, x + 3.0f, y + 30.0f, 20.0f, 3.0f);

  if (e->climbing)
  {
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 13.0f - climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 13.0f, 13.0f + climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 9.0f, 14.0f - climb, 3.0f, 8.0f, uniform);
    sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 14.0f + climb, 3.0f, 8.0f, uniform);
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 23.0f + climb, 5.0f, 8.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 13.0f, 23.0f - climb, 5.0f, 8.0f, COL_OUTLINE);
    bob = fabsf(sinf(phase)) * 0.7f;
  }
  else
  {
    if (moving)
    {
      draw_walking_leg(r, x, y, ENEMY_W, dir, 12.0f, 21.0f + bob,
                       -step * 3.2f, (SDL_Color){43, 50, 40, 255}, COL_INK);
      draw_walking_leg(r, x, y, ENEMY_W, dir, 14.0f, 21.0f + bob,
                       step * 3.2f, (SDL_Color){67, 77, 57, 255},
                       (SDL_Color){12, 15, 14, 255});
    }
    else
    {
      sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
      sprite_rect(r, x, y, ENEMY_W, dir, 13.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
      sprite_rect(r, x, y, ENEMY_W, dir, 9.0f, 22.0f, 4.0f, 7.0f, (SDL_Color){42, 49, 39, 255});
      sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 22.0f, 4.0f, 7.0f, (SDL_Color){42, 49, 39, 255});
      sprite_rect(r, x, y, ENEMY_W, dir, 7.0f, 29.0f, 7.0f, 3.0f, COL_INK);
      sprite_rect(r, x, y, ENEMY_W, dir, 13.0f, 29.0f, 7.0f, 3.0f, COL_INK);
    }
  }

  /* Arm behind torso while patrolling / gesturing. */
  float gesture_swing = e->talking ? sinf(e->anim_time * 5.0f) * 0.65f : 0.0f;
  if (!aiming && !e->climbing)
  {
    float rear_swing = moving ? step : -gesture_swing;
    draw_walking_arm(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                     rear_swing, uniform,
                     (SDL_Color){164, 113, 77, 255});
  }

  sprite_rect(r, x, y, ENEMY_W, dir, 6.0f, 10.0f + bob, 15.0f, 14.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ENEMY_W, dir, 7.0f, 11.0f + bob, 13.0f, 12.0f, uniform);
  if (e->climbing)
  {
    sprite_rect(r, x, y, ENEMY_W, dir, 7.0f, 12.0f + bob, 4.0f, 8.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 12.0f + bob, 4.0f, 8.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 12.0f, 12.0f + bob, 3.0f, 11.0f, (SDL_Color){38, 45, 39, 255});
  }
  else
  {
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 12.0f + bob, 4.0f, 8.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 11.0f, 12.0f + bob, 3.0f, 11.0f, (SDL_Color){38, 45, 39, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob, 4.0f, 5.0f, (SDL_Color){30, 35, 31, 255});
  }
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 20.0f + bob, 11.0f, 2.0f, (SDL_Color){31, 37, 31, 255});

  if (e->climbing)
  {
    draw_climbing_arm(r, x, y, ENEMY_W, dir,
                      8.0f, 14.0f + bob, 6.5f, 5.0f - climb,
                      uniform, (SDL_Color){183, 132, 91, 255});
    draw_climbing_arm(r, x, y, ENEMY_W, dir,
                      18.0f, 14.0f + bob, 19.5f, 5.0f + climb,
                      uniform, (SDL_Color){183, 132, 91, 255});

    /* Back of the helmet: no side-facing face or visor while on a ladder. */
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 1.0f + bob, 12.0f, 11.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 9.0f, 2.0f + bob, 10.0f, 9.0f, (SDL_Color){47, 57, 43, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 2.0f + bob, 8.0f, 2.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 10.0f + bob, 8.0f, 2.0f, (SDL_Color){30, 35, 31, 255});
  }
  else
  {
    /* Helmeted head, red visor/insignia for immediate enemy readability. */
    sprite_rect(r, x, y, ENEMY_W, dir, 9.0f, 2.0f + bob, 10.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 4.0f + bob, 8.0f, 7.0f, (SDL_Color){183, 132, 91, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 1.0f + bob, 12.0f, 5.0f, (SDL_Color){47, 57, 43, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 2.0f + bob, 8.0f, 2.0f, light);
    sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 6.0f + bob, 3.0f, 2.0f, COL_RED);
    sprite_rect(r, x, y, ENEMY_W, dir, 17.0f, 9.0f + bob, 2.0f, 1.0f, (SDL_Color){70, 34, 27, 255});
  }

  if (aiming && !e->climbing)
  {
    float recoil = e->recoil_timer > 0.07f ? -2.0f : 0.0f;
    sprite_rect(r, x, y, ENEMY_W, dir, 17.0f + recoil, 13.0f + bob, 8.0f, 5.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 18.0f + recoil, 14.0f + bob, 7.0f, 3.0f, (SDL_Color){183, 132, 91, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 23.0f + recoil, 12.0f + bob, 8.0f, 4.0f, (SDL_Color){24, 29, 31, 255});
    sprite_rect(r, x, y, ENEMY_W, dir, 25.0f + recoil, 16.0f + bob, 3.0f, 5.0f, (SDL_Color){40, 44, 42, 255});
    if (e->recoil_timer > 0.055f)
    {
      sprite_rect(r, x, y, ENEMY_W, dir, 31.0f + recoil, 11.0f + bob, 4.0f, 6.0f, COL_RED);
      sprite_rect(r, x, y, ENEMY_W, dir, 35.0f + recoil, 13.0f + bob, 3.0f, 3.0f, COL_AMBER);
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
                        (SDL_Color){183, 132, 91, 255});
    sprite_rect(r, x, y, ENEMY_W, dir,
                21.0f, 6.0f + bob, 5.0f, 5.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir,
                22.0f, 7.0f + bob, 3.0f, 3.0f,
                (SDL_Color){201, 148, 101, 255});
  }
  else if (!e->climbing)
  {
    float front_swing = moving ? -step : gesture_swing;
    draw_walking_arm(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob,
                     front_swing, uniform,
                     (SDL_Color){183, 132, 91, 255});
  }

  /* Compact health pips sit in-world without turning into a large UI bar. */
  for (int hp = 0; hp < ENEMY_HP; ++hp)
  {
    SDL_Color hc = hp < e->hp ? (SDL_Color){100, 223, 111, 255}
                              : (SDL_Color){52, 31, 31, 255};
    color_rect(r, COL_INK, x + 3.0f + hp * 7.0f, y - 6.0f, 6.0f, 4.0f);
    color_rect(r, hc, x + 4.0f + hp * 7.0f, y - 5.0f, 4.0f, 2.0f);
  }

  if (e->talking)
  {
    float bubble_y = fmaxf(oy + 2.0f, y - 25.0f);
    color_rect(r, (SDL_Color){8, 12, 17, 220}, x + 2.0f, bubble_y, 22.0f, 11.0f);
    color_rect(r, (SDL_Color){214, 224, 211, 255}, x + 3.0f, bubble_y + 1.0f, 20.0f, 8.0f);
    color_rect(r, (SDL_Color){214, 224, 211, 255}, x + 12.0f, bubble_y + 9.0f, 4.0f, 3.0f);
    for (int dot = 0; dot < 3; ++dot)
    {
      float bounce = (dot == ((int)(e->anim_time * 3.0f) % 3)) ? -1.0f : 0.0f;
      color_rect(r, (SDL_Color){34, 46, 48, 255}, x + 7.0f + dot * 5.0f,
                 bubble_y + 4.0f + bounce, 2.0f, 2.0f);
    }
  }
}

static void draw_dog(SDL_Renderer *r, const Dog *dog, float cam_x, float oy)
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

  color_rect(r, (SDL_Color){3, 5, 8, 115}, x + 2.0f, y + 14.0f, 21.0f, 2.0f);
  sprite_rect(r, x, y, DOG_W, dir, 3.0f + lunge, 5.0f + bob, 16.0f, 9.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 4.0f + lunge, 6.0f + bob, 14.0f, 7.0f, fur);
  sprite_rect(r, x, y, DOG_W, dir, 7.0f + lunge, 6.0f + bob, 8.0f, 3.0f, fur_hi);

  /* Hindquarters and animated tail. */
  sprite_rect(r, x, y, DOG_W, dir, 1.0f + lunge, 6.0f + bob, 6.0f, 7.0f, fur);
  sprite_rect(r, x, y, DOG_W, dir, 0.0f + lunge, 3.0f + bob - gait * 0.35f, 4.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 0.0f + lunge, 4.0f + bob - gait * 0.35f, 3.0f, 1.0f, fur_hi);

  /* Long working-dog muzzle, ears and alert eye. */
  sprite_rect(r, x, y, DOG_W, dir, 16.0f + lunge, 2.0f + bob, 7.0f, 9.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 17.0f + lunge, 3.0f + bob, 6.0f, 7.0f, fur_hi);
  sprite_rect(r, x, y, DOG_W, dir, 17.0f + lunge, 0.0f + bob, 4.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 18.0f + lunge, 1.0f + bob, 2.0f, 3.0f, fur);
  sprite_rect(r, x, y, DOG_W, dir, 21.0f + lunge, 6.0f + bob, 4.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 23.0f + lunge, 7.0f + bob, 2.0f, 2.0f, (SDL_Color){4, 5, 6, 255});
  sprite_rect(r, x, y, DOG_W, dir, 21.0f + lunge, 4.0f + bob, 2.0f, 2.0f,
              chase ? COL_RED : (SDL_Color){228, 205, 142, 255});
  color_rect(r, chase ? COL_RED : COL_AMBER, x + 13.0f, y + 7.0f + bob, 5.0f, 2.0f);

  if (dog->attack_timer > 0.0f)
  {
    sprite_rect(r, x, y, DOG_W, dir, 21.0f + lunge, 10.0f + bob, 5.0f, 3.0f, (SDL_Color){93, 24, 22, 255});
    sprite_rect(r, x, y, DOG_W, dir, 23.0f + lunge, 10.0f + bob, 2.0f, 1.0f, (SDL_Color){239, 231, 195, 255});
  }

  /* Four-beat run condensed into two readable leg pairs. */
  float front_y = 12.0f + fmaxf(0.0f, gait);
  float rear_y = 12.0f + fmaxf(0.0f, -gait);
  sprite_rect(r, x, y, DOG_W, dir, 5.0f + lunge, front_y, 4.0f, 4.0f - fmaxf(0.0f, gait * 0.5f), COL_OUTLINE);
  sprite_rect(r, x, y, DOG_W, dir, 15.0f + lunge, rear_y, 4.0f, 4.0f - fmaxf(0.0f, -gait * 0.5f), COL_OUTLINE);
}

static void render_alarm_lighting(SDL_Renderer *r, int win_w, int win_h,
                                  float world_t)
{
  float wave = 0.5f + 0.5f * sinf(world_t * 7.0f);
  Uint8 wash_alpha = (Uint8)(10.0f + wave * 17.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 210, 20, 16, wash_alpha);
  fill_rect(r, 0.0f, HUD_HEIGHT, (float)win_w,
            (float)win_h - HUD_HEIGHT);
  set_rgba(r, 255, 47, 35, (Uint8)(105.0f + wave * 95.0f));
  fill_rect(r, 0.0f, HUD_HEIGHT, (float)win_w, 3.0f);
  fill_rect(r, 0.0f, (float)win_h - 3.0f, (float)win_w, 3.0f);

  /* Alternating edge blocks suggest rotating emergency beacons without
   * covering the action in the middle of the viewport. */
  for (int x = -12; x < win_w + 24; x += 48)
  {
    float offset = wave > 0.5f ? 0.0f : 24.0f;
    set_rgba(r, 255, 92, 54, 125);
    fill_rect(r, (float)x + offset, HUD_HEIGHT + 4.0f, 18.0f, 2.0f);
    fill_rect(r, (float)x + 24.0f - offset,
              (float)win_h - 7.0f, 18.0f, 2.0f);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void render_world(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  const Level *lvl = &game->gameplay.level;
  const float oy = HUD_HEIGHT + game->presentation.camera_shake_y;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  const float cam_x = game->presentation.cam_x - game->presentation.camera_shake_x;
  float world_t = (float)SDL_GetTicksNS() * 1.0e-9f;

  render_background(game, win_w, win_h);

  /* Structural tile layer. */
  for (int row = 0; row < lvl->map.height; ++row)
  {
    for (int col = 0; col < lvl->map.width; ++col)
    {
      float x = col * (float)TILE_SIZE - cam_x;
      float y = row * (float)TILE_SIZE + oy;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w || !lvl->reveal.tiles_visible[row][col])
        continue;
      TileType tile = lvl->map.tiles[row][col];
      if (tile == TILE_WALL)
        draw_wall_tile(r, lvl, col, row, x, y);
      else if (tile == TILE_LADDER)
        draw_ladder_tile(r, x, y, row);
      else if (tile == TILE_ELEVATOR_SHAFT)
        draw_shaft_tile(r, x, y, col, row);
    }
  }

  if (!lvl->reveal.done)
    return;

  /* Lighting pass derived from the tile grid: soft contact shadows under
   * every overhead surface, plus recessed warm fixtures on long ceilings.
   * Because it reads the level itself, light always matches architecture. */
  {
    int first_col = (int)(cam_x / (float)TILE_SIZE) - 1;
    if (first_col < 0)
      first_col = 0;
    int last_col = first_col + win_w / TILE_SIZE + 3;
    if (last_col > lvl->map.width)
      last_col = lvl->map.width;
    for (int row = 0; row < lvl->map.height; ++row)
    {
      for (int col = first_col; col < last_col; ++col)
      {
        if (lvl->map.tiles[row][col] != TILE_EMPTY &&
            lvl->map.tiles[row][col] != TILE_LADDER)
          continue;
        if (!level_is_solid(lvl, col, row - 1))
          continue;
        float x = col * (float)TILE_SIZE - cam_x;
        float y = row * (float)TILE_SIZE + oy;
        fx_vgrad(r, x, y, TILE_SIZE, 11.0f, FX_INK, 80, FX_INK, 0);

        unsigned h = tile_hash(col, row);
        bool long_ceiling = level_is_solid(lvl, col - 1, row - 1) &&
                            level_is_solid(lvl, col + 1, row - 1);
        if ((h % 7u) == 0u && long_ceiling &&
            lvl->map.tiles[row][col] == TILE_EMPTY)
        {
          float cx = x + TILE_SIZE * 0.5f;
          float flicker = ((h >> 5) % 9u) == 0u &&
                                  fmodf(world_t * 1.9f + (float)col, 5.0f) < 0.08f
                              ? 0.35f
                              : 1.0f;
          SDL_Color warm = fx_dim((SDL_Color){248, 202, 118, 255}, flicker);
          color_rect(r, (SDL_Color){15, 20, 30, 255}, cx - 7.0f, y - 1.0f, 14.0f, 4.0f);
          color_rect(r, warm, cx - 5.0f, y + 1.0f, 10.0f, 2.0f);
          fx_glow(r, cx, y + 3.0f, 12.0f, warm, (Uint8)(70.0f * flicker));
          fx_light_cone(r, cx, y + 2.0f, 7.0f, 30.0f, 86.0f,
                        (SDL_Color){248, 205, 130, 255},
                        (Uint8)(30.0f * flicker));
        }
      }
    }
  }

  /* A soft cool pool of light keeps the hero readable in dark rooms. */
  if (!game->gameplay.player.dying)
    fx_glow(r, game->gameplay.player.x + PLAYER_W * 0.5f - cam_x,
            game->gameplay.player.y + PLAYER_H * 0.5f + oy,
            120.0f, (SDL_Color){134, 196, 214, 255}, 26);

  for (int i = 0; i < lvl->runtime.elevator_count; ++i)
  {
    const Elevator *el = &lvl->runtime.elevators[i];
    draw_platform(r, el->col * (float)TILE_SIZE - cam_x, el->y + oy,
                  (SDL_Color){78, 218, 208, 255}, false);
  }
  for (int i = 0; i < lvl->runtime.fall_platform_count; ++i)
  {
    const FallPlatform *fp = &lvl->runtime.fall_platforms[i];
    if (!fp->removed)
      draw_platform(r, fp->col * (float)TILE_SIZE - cam_x, fp->y + oy,
                    fp->triggered ? COL_RED : COL_AMBER, true);
  }
  for (int i = 0; i < lvl->runtime.moving_platform_count; ++i)
  {
    const MovingPlatform *mp = &lvl->runtime.moving_platforms[i];
    draw_platform(r, mp->x - cam_x, mp->row * (float)TILE_SIZE + oy,
                  (SDL_Color){84, 187, 216, 255}, false);
  }

  /* Furniture stays behind every interactive object and actor. */
  for (int i = 0; i < lvl->map.decoration_count; ++i)
  {
    const Decoration *decoration = &lvl->map.decorations[i];
    float x = decoration->col * (float)TILE_SIZE - cam_x;
    if (x + TILE_SIZE < 0.0f || x > (float)win_w)
      continue;
    draw_decoration(r, decoration, cam_x, oy, world_t);
  }

  /* Doors and other wall-mounted fixtures belong behind anyone walking along
   * the corridor floor. */
  for (int d = 0; d < lvl->map.door_count; ++d)
  {
    float x = lvl->map.doors[d].col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.doors[d].row * (float)TILE_SIZE + oy;
    draw_door(r, x, y, d);
  }
  if (lvl->map.has_exit)
  {
    float x = lvl->map.exit_col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.exit_row * (float)TILE_SIZE + oy;
    draw_exit(r, game, x, y);
  }
  for (int i = 0; i < lvl->map.terminal_count; ++i)
  {
    float x = lvl->map.terminals[i].col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.terminals[i].row * (float)TILE_SIZE + oy;
    bool active = i == lvl->runtime.active_terminal_index;
    draw_terminal(r, x, y, active,
                  active && lvl->runtime.terminal_hacked, world_t);
  }
  for (int i = 0; i < lvl->map.alarm_switch_count; ++i)
  {
    float x = lvl->map.alarm_switches[i].col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.alarm_switches[i].row * (float)TILE_SIZE + oy;
    bool being_used = false;
    for (int guard = 0; guard < game->gameplay.enemy_count; ++guard)
    {
      const Enemy *enemy = &game->gameplay.enemies[guard];
      if (!enemy->dead && enemy->raising_alarm &&
          enemy->alarm_switch_index == i && enemy->alarm_use_timer > 0.0f)
      {
        being_used = true;
        break;
      }
    }
    draw_alarm_switch(r, x, y, gameplay_alarm_active(&game->gameplay),
                      game->gameplay.active_alarm_switch == i,
                      being_used, world_t);
  }

  /* Ambient staff remain subdued, but correctly stand in front of the back
   * wall and its fixtures. Floor props and gameplay actors render later. */
  for (int i = 0; i < game->gameplay.janitor_count; ++i)
    draw_janitor(r, &game->gameplay.janitors[i], cam_x, oy);

  /* Redraw ladders as a middle layer so ambient janitors pass behind their
   * rails and rungs. Interactive actors are rendered later and remain in
   * front, preserving the usual climbing readability. */
  for (int row = 0; row < lvl->map.height; ++row)
  {
    for (int col = 0; col < lvl->map.width; ++col)
    {
      if (lvl->map.tiles[row][col] != TILE_LADDER)
        continue;
      float x = col * (float)TILE_SIZE - cam_x;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w)
        continue;
      float y = row * (float)TILE_SIZE + oy;
      draw_ladder_tile(r, x, y, row);
    }
  }

  /* Pickups bob independently and cast restrained color-coded glows. */
  int card_pos = 0;
  for (int i = 0; i < lvl->runtime.item_count; ++i)
  {
    const Item *it = &lvl->runtime.items[i];
    if (it->collected)
      continue;
    float bob = sinf(world_t * 3.14159265f + (float)i * 0.7f) * 3.0f;
    float x = it->x - 7.0f - cam_x;
    float y = it->y - 9.0f + oy + bob;
    if (it->type == ITEM_CARD)
    {
      Uint8 alpha = 255;
      bool active = true;
      if (game->state == STATE_SHOW_KEYCARD)
      {
        active = card_pos == game->presentation.card_anim_current;
        alpha = active ? 255 : 80;
      }
      draw_card(r, x, y, alpha, active);
      card_pos++;
    }
    else if (it->type == ITEM_GUN)
      draw_gun_pickup(r, x, y + 3.0f);
    else if (it->type == ITEM_GRENADE)
      draw_grenade(r, x + 3.0f, y + 4.0f, 0.0f);
    else if (it->type == ITEM_MEDKIT)
      draw_medkit(r, x, y);
    else if (it->type == ITEM_BAZOOKA)
      draw_bazooka_pickup(r, x - 4.0f, y + 1.0f);
  }

  for (int i = 0; i < lvl->map.spike_count; ++i)
    draw_spike_strip(r, lvl->map.spike_spawns[i].x - cam_x,
                     lvl->map.spike_spawns[i].y + oy);

  for (int i = 0; i < lvl->map.ceiling_fan_count; ++i)
    draw_ceiling_fan(r, &lvl->map.ceiling_fans[i], cam_x, oy, world_t, i);

  for (int i = 0; i < lvl->runtime.crate_count; ++i)
    if (lvl->runtime.crates[i].active)
      draw_crate(r, &lvl->runtime.crates[i], cam_x, oy);

  for (int i = 0; i < lvl->runtime.gas_canister_count; ++i)
    if (lvl->runtime.gas_canisters[i].active)
      draw_gas_canister(r, &lvl->runtime.gas_canisters[i], cam_x, oy);

  for (int i = 0; i < game->gameplay.mine_count; ++i)
    if (game->gameplay.mines[i].active)
      draw_mine(r, &game->gameplay.mines[i], cam_x, oy);

  for (int i = 0; i < game->gameplay.dog_count; ++i)
    if (!game->gameplay.dogs[i].dead)
      draw_dog(r, &game->gameplay.dogs[i], cam_x, oy);

  for (int i = 0; i < game->gameplay.enemy_count; ++i)
    if (!game->gameplay.enemies[i].dead)
      draw_enemy(r, &game->gameplay.enemies[i], cam_x, oy);

  for (int i = 0; i < game->gameplay.grenade_count; ++i)
  {
    const Grenade *g = &game->gameplay.grenades[i];
    if (g->active)
      draw_grenade(r, g->x - cam_x, g->y + oy, g->timer);
  }

  /* Fast projectiles get a one-pixel trail and hot core for impact/readability. */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < MAX_ROCKETS; ++i)
  {
    const Rocket *rocket = &game->gameplay.rockets[i];
    if (!rocket->active)
      continue;
    float x = rocket->x - cam_x;
    float y = rocket->y + oy;
    int dir = rocket->vx >= 0.0f ? 1 : -1;
    fx_glow(r, x + ROCKET_W * 0.5f, y + ROCKET_H * 0.5f, 18.0f,
            (SDL_Color){255, 132, 45, 255}, 125);
    set_rgba(r, 122, 132, 124, 70);
    fill_rect(r, x - (dir > 0 ? 15.0f : -15.0f), y + 1.0f,
              16.0f, 4.0f);
    draw_rocket_sprite(r, x, y, dir, true);
  }
  for (int i = 0; i < MAX_BULLETS; ++i)
  {
    const Bullet *b = &game->gameplay.bullets[i];
    if (!b->active)
      continue;
    float x = b->x - cam_x;
    float y = b->y + oy;
    bool vertical = fabsf(b->vy) > fabsf(b->vx);
    float bullet_w = vertical ? (float)BULLET_H : (float)BULLET_W;
    float bullet_h = vertical ? (float)BULLET_W : (float)BULLET_H;
    fx_glow(r, x + bullet_w * 0.5f, y + bullet_h * 0.5f, 11.0f,
            (SDL_Color){255, 200, 90, 255}, 110);
    set_rgba(r, 255, 183, 38, 75);
    if (vertical)
      fill_rect(r, x + 1.0f,
                y - (b->vy > 0.0f ? 8.0f : -8.0f), 2.0f, 12.0f);
    else
      fill_rect(r, x - (b->vx > 0.0f ? 8.0f : -8.0f),
                y + 1.0f, 12.0f, 2.0f);
    color_rect(r, (SDL_Color){255, 243, 170, 255},
               x, y, bullet_w, bullet_h);
  }
  for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
  {
    const Bullet *b = &game->gameplay.enemy_bullets[i];
    if (!b->active)
      continue;
    float x = b->x - cam_x;
    float y = b->y + oy;
    fx_glow(r, x + BULLET_W * 0.5f, y + BULLET_H * 0.5f, 11.0f,
            (SDL_Color){255, 92, 62, 255}, 110);
    set_rgba(r, 255, 52, 39, 80);
    fill_rect(r, x - (b->vx > 0.0f ? 7.0f : -7.0f), y, 11.0f, 4.0f);
    color_rect(r, (SDL_Color){255, 103, 54, 255}, x, y, BULLET_W, BULLET_H);
    color_rect(r, (SDL_Color){255, 225, 128, 255}, x + 2.0f, y + 1.0f, 4.0f, 2.0f);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  particle_system_render(&game->presentation.particles, r, oy, cam_x);

  if (!game->gameplay.player.dying)
  {
    bool blink = game->gameplay.invuln_timer > 0.0f &&
                 ((int)(game->gameplay.invuln_timer * 12.0f) % 2 == 0);
    if (!blink)
      draw_player(r, &game->gameplay.player, cam_x, oy,
                  game->gameplay.terminal_hacking,
                  game->gameplay.terminal_hack_progress);
  }
  else
  {
    const Particle *spark = &game->presentation.particles.particles[0];
    if (spark->active && spark->life > 0.0f)
    {
      color_rect(r, (SDL_Color){68, 17, 19, 255},
                 spark->x - cam_x - 7.0f, spark->y + oy - 3.0f, 14.0f, 5.0f);
      color_rect(r, COL_RED, spark->x - cam_x - 4.0f, spark->y + oy - 4.0f, 8.0f, 2.0f);
    }
  }
  if (gameplay_alarm_active(&game->gameplay))
    render_alarm_lighting(r, win_w, win_h, world_t);
}

static void draw_hud_separator(SDL_Renderer *r, float x)
{
  color_rect(r, (SDL_Color){6, 9, 15, 255}, x, 8.0f, 1.0f, 24.0f);
  color_rect(r, (SDL_Color){56, 72, 92, 255}, x + 1.0f, 8.0f, 1.0f, 24.0f);
}

static void draw_hud_heart(SDL_Renderer *r, float x, float y, bool filled)
{
  static const char *rows[5] = {"01010", "11111", "11111", "01110", "00100"};
  for (int row = 0; row < 5; ++row)
  {
    for (int col = 0; col < 5; ++col)
    {
      if (rows[row][col] != '1')
        continue;
      SDL_Color c;
      if (filled)
        c = row < 2 ? (SDL_Color){246, 92, 92, 255}
                    : (SDL_Color){198, 44, 52, 255};
      else
        c = (SDL_Color){46, 34, 40, 255};
      color_rect(r, c, x + col * 2.0f, y + row * 2.0f, 2.0f, 2.0f);
    }
  }
}

static void render_hud(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  const Uint8 label_r = 108, label_g = 128, label_b = 148;

  /* Brushed console body, lit from above, closed by a cyan status line. */
  fx_vgrad(r, 0.0f, 0.0f, (float)win_w, 37.0f,
           (SDL_Color){30, 40, 56, 255}, 255,
           (SDL_Color){13, 19, 30, 255}, 255);
  color_rect(r, (SDL_Color){60, 76, 98, 255}, 0.0f, 0.0f, (float)win_w, 1.0f);
  color_rect(r, (SDL_Color){5, 7, 12, 255}, 0.0f, 37.0f, (float)win_w, 1.0f);
  color_rect(r, (SDL_Color){38, 112, 110, 255}, 0.0f, 38.0f, (float)win_w, 1.0f);
  color_rect(r, (SDL_Color){5, 7, 12, 255}, 0.0f, 39.0f, (float)win_w, 1.0f);

  /* Identity block. */
  color_rect(r, COL_RED, 0.0f, 0.0f, 3.0f, 37.0f);
  draw_text(r, 12.0f, 9.0f, 1.5f, 236, 238, 224, "CHUCK");
  color_rect(r, (SDL_Color){170, 52, 46, 255}, 12.0f, 23.0f, 58.0f, 2.0f);
  draw_text(r, 12.0f, 27.0f, 0.7f, 120, 138, 152, "RESCUE OPS");
  draw_hud_separator(r, 88.0f);

  /* Lives as pixel hearts, with empty slots so the max is always visible. */
  draw_text(r, 99.0f, 8.0f, 0.7f, label_r, label_g, label_b, "VITAL");
  for (int i = 0; i < 5; ++i)
    draw_hud_heart(r, 99.0f + i * 14.0f, 19.0f, i < game->campaign.lives);
  if (game->campaign.lives > 5)
  {
    char life_buf[8];
    SDL_snprintf(life_buf, sizeof(life_buf), "+%d", game->campaign.lives - 5);
    draw_text(r, 168.0f, 20.0f, 0.9f, 246, 110, 96, life_buf);
  }
  draw_hud_separator(r, 190.0f);

  /* Ammunition as brass rounds standing in a rack. */
  draw_text(r, 201.0f, 8.0f, 0.7f, label_r, label_g, label_b, "AMMO");
  for (int i = 0; i < MAX_AMMO; ++i)
  {
    float ammo_x = 202.0f + i * 7.0f;
    if (i < game->gameplay.player.bullets)
    {
      color_rect(r, (SDL_Color){255, 236, 170, 255}, ammo_x, 19.0f, 3.0f, 3.0f);
      color_rect(r, (SDL_Color){222, 172, 84, 255}, ammo_x, 22.0f, 3.0f, 8.0f);
      color_rect(r, (SDL_Color){164, 118, 52, 255}, ammo_x + 2.0f, 22.0f, 1.0f, 8.0f);
    }
    else
    {
      color_rect(r, (SDL_Color){40, 48, 58, 255}, ammo_x, 19.0f, 3.0f, 11.0f);
    }
    color_rect(r, (SDL_Color){70, 84, 99, 255}, ammo_x - 1.0f, 30.0f, 5.0f, 2.0f);
  }
  if (game->gameplay.player.grenades > 0)
    draw_grenade(r, 247.0f, 19.0f, 0.0f);
  if (game->gameplay.player.bazooka_rockets > 0)
    draw_rocket_sprite(r, 260.0f, 22.0f, 1, false);
  draw_hud_separator(r, 276.0f);

  /* Access status chip with a live LED. */
  draw_text(r, 287.0f, 8.0f, 0.7f, label_r, label_g, label_b, "ACCESS");
  bool unlocked = game->gameplay.level.runtime.exit_unlocked;
  float blink = 0.5f + 0.5f * sinf((float)SDL_GetTicksNS() * 1.0e-9f * 4.0f);
  if (unlocked)
  {
    color_rect(r, (SDL_Color){16, 52, 40, 255}, 287.0f, 19.0f, 70.0f, 13.0f);
    color_rect(r, (SDL_Color){40, 132, 96, 255}, 287.0f, 19.0f, 70.0f, 1.0f);
    color_rect(r, (SDL_Color){96, 230, 140, 255}, 291.0f, 24.0f, 3.0f, 3.0f);
    draw_text(r, 298.0f, 22.0f, 0.9f, 168, 255, 206, "GRANTED");
  }
  else
  {
    color_rect(r, (SDL_Color){54, 24, 24, 255}, 287.0f, 19.0f, 70.0f, 13.0f);
    color_rect(r, (SDL_Color){124, 52, 46, 255}, 287.0f, 19.0f, 70.0f, 1.0f);
    color_rect(r, fx_dim((SDL_Color){246, 90, 70, 255}, 0.45f + blink * 0.55f),
               291.0f, 24.0f, 3.0f, 3.0f);
    draw_text(r, 298.0f, 22.0f, 0.9f, 250, 158, 128, "LOCKED");
  }
  draw_hud_separator(r, 371.0f);

  char level_buf[32];
  SDL_snprintf(level_buf, sizeof(level_buf), "%02d", game->campaign.current_level + 1);
  draw_text(r, 382.0f, 8.0f, 0.7f, label_r, label_g, label_b, "SECTOR");
  draw_text(r, 382.0f, 19.0f, 1.5f, 226, 232, 220, level_buf);
  draw_hud_separator(r, 440.0f);

  /* Score keeps its leading zeros, but only the live digits glow. */
  char score_buf[16];
  SDL_snprintf(score_buf, sizeof(score_buf), "%07d", game->campaign.score);
  draw_text(r, 451.0f, 8.0f, 0.7f, label_r, label_g, label_b, "SCORE");
  int first_digit = 0;
  while (first_digit < 6 && score_buf[first_digit] == '0')
    ++first_digit;
  draw_text(r, 451.0f, 19.0f, 1.5f, 74, 88, 102, score_buf);
  draw_text(r, 451.0f + first_digit * 12.0f, 19.0f, 1.5f, 248, 196, 92,
            score_buf + first_digit);

  /* The passive trail meter becomes an unmistakable security readout while
   * the building alarm is active. */
  float t = (float)SDL_GetTicksNS() * 1.0e-9f;
  if (gameplay_alarm_active(&game->gameplay))
  {
    float pulse = 0.5f + 0.5f * sinf(t * 7.0f);
    SDL_Color alert = fx_dim((SDL_Color){255, 76, 54, 255},
                             0.55f + pulse * 0.45f);
    char alarm_buf[24];
    SDL_snprintf(alarm_buf, sizeof(alarm_buf), "ALERT %02d",
                 (int)ceilf(game->gameplay.terminal_alarm_timer));
    draw_text(r, 650.0f, 8.0f, 0.7f,
              alert.r, alert.g, alert.b, "SECURITY");
    color_rect(r, (SDL_Color){58, 16, 18, 255},
               648.0f, 17.0f, 134.0f, 16.0f);
    color_rect(r, alert, 648.0f, 17.0f, 134.0f, 2.0f);
    color_rect(r, alert, 653.0f, 22.0f, 5.0f, 5.0f);
    draw_text(r, 665.0f, 20.0f, 1.1f,
              alert.r, alert.g, alert.b, alarm_buf);
  }
  else
  {
    draw_text(r, 650.0f, 8.0f, 0.7f, label_r, label_g, label_b, "TRAIL");
    color_rect(r, (SDL_Color){10, 15, 24, 255}, 648.0f, 17.0f, 134.0f, 16.0f);
    color_rect(r, (SDL_Color){30, 42, 58, 255}, 648.0f, 17.0f, 134.0f, 1.0f);
    for (int i = 0; i < 12; ++i)
    {
      float wave = 0.5f + 0.5f * sinf(t * 2.6f + (float)i * 0.9f);
      float height = 3.0f + wave * 9.0f;
      SDL_Color bar = i < 9 ? fx_mix((SDL_Color){24, 96, 96, 255}, COL_CYAN, wave)
                            : (SDL_Color){52, 68, 82, 255};
      color_rect(r, bar, 653.0f + i * 10.0f, 30.0f - height, 6.0f, height);
    }
  }
}

static void render_interaction_prompt(Game *game, int win_w, int win_h)
{
  bool terminal_available = game->gameplay.terminal_in_range &&
                            !game->gameplay.level.runtime.exit_unlocked;
  bool door_available = gameplay_player_door_index(&game->gameplay) >= 0;
  if (game->state != STATE_PLAYING ||
      (!terminal_available && !door_available))
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

  char label[64];
  if (terminal_available && game->gameplay.terminal_hacking)
  {
    int percent = (int)(progress * 100.0f);
    SDL_snprintf(label, sizeof(label), "BREACHING SECURITY... %d%%", percent);
  }
  else if (terminal_available)
  {
    SDL_snprintf(label, sizeof(label), "HOLD E TO HACK ACTIVE TERMINAL");
  }
  else
  {
    SDL_snprintf(label, sizeof(label), "PRESS E TO ENTER DOOR");
  }

  const float text_scale = 1.25f;
  const float panel_padding = 12.0f;
  float panel_w = draw_text_width(label, text_scale) + panel_padding * 2.0f;
  float panel_h = terminal_available ? 45.0f : 31.0f;
  float x = ((float)win_w - panel_w) * 0.5f;
  float y = (float)win_h - panel_h - 12.0f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 5, 10, 15, 235);
  fill_rect(r, x, y, panel_w, panel_h);
  set_rgba(r, 24, 46, 44, 255);
  fill_rect(r, x, y, panel_w, 1.0f);
  fill_rect(r, x, y + panel_h - 1.0f, panel_w, 1.0f);
  fill_rect(r, x, y, 1.0f, panel_h);
  fill_rect(r, x + panel_w - 1.0f, y, 1.0f, panel_h);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  /* Terminal-green corner ticks frame the prompt like a targeting HUD. */
  set_rgba(r, 86, 240, 170, 255);
  const float tick = 7.0f;
  fill_rect(r, x, y, tick, 2.0f);
  fill_rect(r, x, y, 2.0f, tick);
  fill_rect(r, x + panel_w - tick, y, tick, 2.0f);
  fill_rect(r, x + panel_w - 2.0f, y, 2.0f, tick);
  fill_rect(r, x, y + panel_h - 2.0f, tick, 2.0f);
  fill_rect(r, x, y + panel_h - tick, 2.0f, tick);
  fill_rect(r, x + panel_w - tick, y + panel_h - 2.0f, tick, 2.0f);
  fill_rect(r, x + panel_w - 2.0f, y + panel_h - tick, 2.0f, tick);

  draw_text(r, x + panel_padding, y + 9.0f, text_scale,
            174, 255, 213, label);

  if (terminal_available)
  {
    color_rect(r, (SDL_Color){16, 34, 32, 255},
               x + panel_padding, y + 31.0f,
               panel_w - panel_padding * 2.0f, 7.0f);
    if (progress > 0.0f)
    {
      float bar_w = (panel_w - panel_padding * 2.0f) * progress;
      color_rect(r, (SDL_Color){44, 168, 118, 255},
                 x + panel_padding, y + 31.0f, bar_w, 7.0f);
      color_rect(r, (SDL_Color){120, 255, 190, 255},
                 x + panel_padding, y + 31.0f, bar_w, 2.0f);
      color_rect(r, (SDL_Color){190, 255, 220, 255},
                 x + panel_padding + bar_w - 2.0f,
                 y + 31.0f, 2.0f, 7.0f);
      fx_glow(r, x + panel_padding + bar_w, y + 34.0f, 14.0f,
              (SDL_Color){86, 240, 170, 255}, 80);
    }
  }
}

static void draw_overlay_panel(Game *game, float y, SDL_Color accent,
                               const char *title, const char *subtitle)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  /* Dim the whole scene so the verdict owns the frame. */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 4, 6, 11, 150);
  fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
  set_rgba(r, 5, 9, 15, 235);
  fill_rect(r, 0.0f, y - 35.0f, (float)win_w, subtitle ? 93.0f : 70.0f);
  set_rgba(r, accent.r, accent.g, accent.b, 220);
  fill_rect(r, 0.0f, y - 35.0f, (float)win_w, 3.0f);
  fill_rect(r, 0.0f, y + (subtitle ? 55.0f : 32.0f), (float)win_w, 2.0f);
  set_rgba(r, accent.r, accent.g, accent.b, 60);
  fill_rect(r, 0.0f, y - 32.0f, (float)win_w, 1.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  fx_glow(r, (float)win_w * 0.5f, y + 8.0f, 190.0f, accent, 34);
  draw_text_centered(game, y + 3.0f, 4.0f, 6, 9, 14, title);
  draw_text_centered(game, y, 4.0f, accent.r, accent.g, accent.b, title);
  if (subtitle)
    draw_text_centered(game, y + 37.0f, 1.5f, 210, 220, 215, subtitle);
}

void game_render(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  SDL_SetRenderDrawColor(r, 8, 11, 17, 255);
  SDL_RenderClear(r);

  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  if (game->state == STATE_OPENING_CUTSCENE)
  {
    opening_cutscene_render(r, &game->presentation.opening_cutscene, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  if (game->state == STATE_INTRO)
  {
    intro_render(r, &game->presentation.intro, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  if (game->state == STATE_LEVEL_TRANSITION)
  {
    level_transition_render(r, &game->presentation.level_transition, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  if (game->state == STATE_OUTRO)
  {
    outro_cutscene_render(r, &game->presentation.outro_cutscene, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  render_world(game);
  render_hud(game);
  render_interaction_prompt(game, win_w, win_h);

  if (game->presentation.exit_unlocked_timer > 0.0f)
  {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_rgba(r, 7, 20, 20, 215);
    fill_rect(r, 226.0f, 47.0f, 348.0f, 31.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    draw_text_centered(game, 63.0f, 2.2f, 94, 255, 201, "PURSUIT ROUTE OPEN");
    float exit_screen = game->gameplay.level.map.exit_col * (float)TILE_SIZE - game->presentation.cam_x;
    if (exit_screen + TILE_SIZE < 0.0f)
      draw_text(r, 199.0f, 52.0f, 2.2f, 226, 216, 94, "<");
    else if (exit_screen > (float)win_w)
      draw_text(r, 584.0f, 52.0f, 2.2f, 226, 216, 94, ">");
  }

  if (game->state == STATE_LEVEL_CLEARED)
    draw_overlay_panel(game, 240.0f, (SDL_Color){86, 233, 151, 255},
                       "THE TRAIL LEADS UP", NULL);
  else if (game->state == STATE_GAME_OVER)
    draw_overlay_panel(game, 225.0f, (SDL_Color){235, 72, 65, 255},
                       "THE TRAIL WENT COLD", "PRESS R TO RESTART THE PURSUIT");
  /* One shared finishing pass keeps every frame looking like the same film:
   * gentle scanlines for texture and a vignette that focuses the action. */
  fx_vignette(r, win_w, win_h, 58);
  fx_scanlines(r, win_w, win_h, 11);

  SDL_RenderPresent(r);
}
