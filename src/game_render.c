#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>

#include "fx.h"

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
  if (SDL_GetRenderLogicalPresentation(game->renderer, &lw, &lh, &mode) && lw > 0 && lh > 0)
  {
    *out_w = lw;
    *out_h = lh;
    return;
  }
  SDL_GetWindowSize(game->window, out_w, out_h);
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

static void draw_text_centered(Game *game, float center_y, float scale,
                               Uint8 cr, Uint8 cg, Uint8 cb, const char *text)
{
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  float text_w = (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  float text_h = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  draw_text(game->renderer, ((float)win_w - text_w) * 0.5f,
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
  SDL_Renderer *r = game->renderer;
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
  float far_shift = fmodf(-game->cam_x * 0.10f, 224.0f);
  if (far_shift > 0.0f)
    far_shift -= 224.0f;
  for (float x = far_shift - 48.0f; x < (float)win_w + 224.0f; x += 224.0f)
  {
    unsigned seed = (unsigned)((int)(x + game->cam_x * 0.10f) / 224);
    unsigned h = fx_hash(seed * 31u + (unsigned)game->current_level * 7u);
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
  float mid_shift = fmodf(-game->cam_x * 0.18f, 192.0f);
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
      unsigned h = tile_hash((int)(x + game->cam_x * 0.18f) / 8 + lamp,
                             game->current_level + 7);
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
  float shaft_shift = fmodf(-game->cam_x * 0.22f, 384.0f);
  if (shaft_shift > 0.0f)
    shaft_shift -= 384.0f;
  for (float x = shaft_shift - 96.0f; x < (float)win_w + 384.0f; x += 384.0f)
  {
    float sway = sinf(t * 0.21f + x * 0.01f) * 14.0f;
    fx_light_cone(r, x + 150.0f + sway, oy - 6.0f, 14.0f, 52.0f,
                  fh * 0.9f, (SDL_Color){120, 190, 196, 255}, 13);
  }

  /* Near layer: wall seams and conduit shadows at a faster parallax. */
  float near_shift = fmodf(-game->cam_x * 0.30f, 96.0f);
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
    unsigned h = tile_hash(i + game->current_level * 29, 91);
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
  bool unlocked = game->level.exit_unlocked;
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

/* Draw a local sprite rectangle, mirrored inside the entity collision width. */
static void sprite_rect(SDL_Renderer *r, float bx, float by, float sprite_w, int dir,
                        float lx, float ly, float w, float h, SDL_Color c)
{
  float x = (dir >= 0) ? bx + lx : bx + sprite_w - lx - w;
  color_rect(r, c, floorf(x), floorf(by + ly), w, h);
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
                              float rail_x, float hand_y,
                              SDL_Color sleeve, SDL_Color skin)
{
  float elbow_x = shoulder_x + (rail_x - shoulder_x) * 0.72f;
  float elbow_y = shoulder_y + (hand_y - shoulder_y) * 0.48f;

  sprite_limb_segment(r, x, y, sprite_w, dir,
                      shoulder_x, shoulder_y, elbow_x, elbow_y, sleeve);
  sprite_limb_segment(r, x, y, sprite_w, dir,
                      elbow_x, elbow_y, rail_x, hand_y, skin);

  /* Broad, outlined fists stay readable against the amber ladder rungs. */
  sprite_rect(r, x, y, sprite_w, dir,
              rail_x - 2.5f, hand_y - 2.0f, 5.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir,
              rail_x - 1.5f, hand_y - 1.0f, 3.0f, 3.0f, skin);
}

static void draw_player_crawling(SDL_Renderer *r, const Player *p, float x, float y)
{
  int dir = p->facing;
  float phase = p->anim_time * 3.2f;
  float shove = (fabsf(p->vx) > 1.0f) ? sinf(phase) * 2.0f : 0.0f;
  bool firing = p->action_timer > 0.0f;

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

  /* Braced front arm and compact sidearm. */
  sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 11.0f, 7.0f, 4.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, 11.0f, 6.0f, 2.0f, (SDL_Color){209, 154, 106, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 22.0f, 10.0f, firing ? 7.0f : 5.0f, 3.0f, (SDL_Color){36, 43, 48, 255});
  if (firing && p->action_timer > 0.055f)
  {
    sprite_rect(r, x, y, PLAYER_W, dir, 29.0f, 9.0f, 3.0f, 5.0f, COL_AMBER);
    sprite_rect(r, x, y, PLAYER_W, dir, 32.0f, 10.0f, 2.0f, 3.0f, (SDL_Color){255, 239, 165, 255});
  }
}

static void draw_player(SDL_Renderer *r, const Player *p, float cam_x, float oy)
{
  float x = p->x - cam_x;
  float y = p->y + oy;
  int dir = p->facing;

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
  bool firing = p->action_timer > 0.0f;
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
  if (!climbing && !firing)
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
    /* Keep one hand on the ladder while the other operates the sidearm. */
    draw_climbing_arm(r, x, y, PLAYER_W, dir,
                      8.0f, 14.0f + bob, 4.5f, 5.0f - climb,
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
                        18.0f, 14.0f + bob, 21.5f, 5.0f + climb,
                        (SDL_Color){42, 118, 153, 255},
                        (SDL_Color){209, 154, 105, 255});
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
    if (firing)
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

static void draw_enemy(SDL_Renderer *r, const Enemy *e, float cam_x, float oy)
{
  float x = e->x - cam_x;
  float y = e->y + oy;
  int dir = e->dir;
  /* The ladder pose faces away from the camera, so left/right mirroring is invalid. */
  if (e->climbing)
    dir = 1;
  bool aiming = e->aim_timer > 0.0f || e->recoil_timer > 0.0f;
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
                      8.0f, 14.0f + bob, 4.5f, 5.0f - climb,
                      uniform, (SDL_Color){183, 132, 91, 255});
    draw_climbing_arm(r, x, y, ENEMY_W, dir,
                      18.0f, 14.0f + bob, 21.5f, 5.0f + climb,
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

static void render_world(Game *game)
{
  SDL_Renderer *r = game->renderer;
  const Level *lvl = &game->level;
  const float oy = HUD_HEIGHT;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  const float cam_x = game->cam_x;
  float world_t = (float)SDL_GetTicksNS() * 1.0e-9f;

  render_background(game, win_w, win_h);

  /* Structural tile layer. */
  for (int row = 0; row < lvl->height; ++row)
  {
    for (int col = 0; col < lvl->width; ++col)
    {
      float x = col * (float)TILE_SIZE - cam_x;
      float y = row * (float)TILE_SIZE + oy;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w || !lvl->tiles_visible[row][col])
        continue;
      TileType tile = lvl->tiles[row][col];
      if (tile == TILE_WALL)
        draw_wall_tile(r, lvl, col, row, x, y);
      else if (tile == TILE_LADDER)
        draw_ladder_tile(r, x, y, row);
      else if (tile == TILE_ELEVATOR_SHAFT)
        draw_shaft_tile(r, x, y, col, row);
    }
  }

  if (!lvl->reveal_done)
    return;

  /* Lighting pass derived from the tile grid: soft contact shadows under
   * every overhead surface, plus recessed warm fixtures on long ceilings.
   * Because it reads the level itself, light always matches architecture. */
  {
    int first_col = (int)(cam_x / (float)TILE_SIZE) - 1;
    if (first_col < 0)
      first_col = 0;
    int last_col = first_col + win_w / TILE_SIZE + 3;
    if (last_col > lvl->width)
      last_col = lvl->width;
    for (int row = 0; row < lvl->height; ++row)
    {
      for (int col = first_col; col < last_col; ++col)
      {
        if (lvl->tiles[row][col] != TILE_EMPTY &&
            lvl->tiles[row][col] != TILE_LADDER)
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
            lvl->tiles[row][col] == TILE_EMPTY)
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
  if (!game->player.dying)
    fx_glow(r, game->player.x + PLAYER_W * 0.5f - cam_x,
            game->player.y + PLAYER_H * 0.5f + oy,
            120.0f, (SDL_Color){134, 196, 214, 255}, 26);

  for (int i = 0; i < lvl->elevator_count; ++i)
  {
    const Elevator *el = &lvl->elevators[i];
    draw_platform(r, el->col * (float)TILE_SIZE - cam_x, el->y + oy,
                  (SDL_Color){78, 218, 208, 255}, false);
  }
  for (int i = 0; i < lvl->fall_platform_count; ++i)
  {
    const FallPlatform *fp = &lvl->fall_platforms[i];
    if (!fp->removed)
      draw_platform(r, fp->col * (float)TILE_SIZE - cam_x, fp->y + oy,
                    fp->triggered ? COL_RED : COL_AMBER, true);
  }
  for (int i = 0; i < lvl->moving_platform_count; ++i)
  {
    const MovingPlatform *mp = &lvl->moving_platforms[i];
    draw_platform(r, mp->x - cam_x, mp->row * (float)TILE_SIZE + oy,
                  (SDL_Color){84, 187, 216, 255}, false);
  }

  for (int d = 0; d < lvl->door_count; ++d)
  {
    float x = lvl->doors[d].col * (float)TILE_SIZE - cam_x;
    float y = lvl->doors[d].row * (float)TILE_SIZE + oy;
    draw_door(r, x, y, d);
  }
  if (lvl->has_exit)
  {
    float x = lvl->exit_col * (float)TILE_SIZE - cam_x;
    float y = lvl->exit_row * (float)TILE_SIZE + oy;
    draw_exit(r, game, x, y);
  }
  for (int i = 0; i < lvl->terminal_count; ++i)
  {
    float x = lvl->terminals[i].col * (float)TILE_SIZE - cam_x;
    float y = lvl->terminals[i].row * (float)TILE_SIZE + oy;
    bool active = i == lvl->active_terminal_index;
    draw_terminal(r, x, y, active,
                  active && lvl->terminal_hacked, world_t);
  }

  /* Pickups bob independently and cast restrained color-coded glows. */
  int card_pos = 0;
  for (int i = 0; i < lvl->item_count; ++i)
  {
    const Item *it = &lvl->items[i];
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
        active = card_pos == game->card_anim_current;
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
  }

  for (int i = 0; i < lvl->spike_count; ++i)
    draw_spike_strip(r, lvl->spike_spawns[i].x - cam_x,
                     lvl->spike_spawns[i].y + oy);

  for (int i = 0; i < lvl->crate_count; ++i)
    if (lvl->crates[i].active)
      draw_crate(r, &lvl->crates[i], cam_x, oy);

  for (int i = 0; i < game->mine_count; ++i)
    if (game->mines[i].active)
      draw_mine(r, &game->mines[i], cam_x, oy);

  for (int i = 0; i < game->dog_count; ++i)
    if (!game->dogs[i].dead)
      draw_dog(r, &game->dogs[i], cam_x, oy);

  for (int i = 0; i < game->enemy_count; ++i)
    if (!game->enemies[i].dead)
      draw_enemy(r, &game->enemies[i], cam_x, oy);

  for (int i = 0; i < game->grenade_count; ++i)
  {
    const Grenade *g = &game->grenades[i];
    if (g->active)
      draw_grenade(r, g->x - cam_x, g->y + oy, g->timer);
  }

  /* Fast projectiles get a one-pixel trail and hot core for impact/readability. */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < MAX_BULLETS; ++i)
  {
    const Bullet *b = &game->bullets[i];
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
    const Bullet *b = &game->enemy_bullets[i];
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

  particle_system_render(&game->particles, r, oy, cam_x);

  if (!game->player.dying)
  {
    bool blink = game->invuln_timer > 0.0f &&
                 ((int)(game->invuln_timer * 12.0f) % 2 == 0);
    if (!blink)
      draw_player(r, &game->player, cam_x, oy);
  }
  else
  {
    const Particle *spark = &game->particles.particles[0];
    if (spark->active && spark->life > 0.0f)
    {
      color_rect(r, (SDL_Color){68, 17, 19, 255},
                 spark->x - cam_x - 7.0f, spark->y + oy - 3.0f, 14.0f, 5.0f);
      color_rect(r, COL_RED, spark->x - cam_x - 4.0f, spark->y + oy - 4.0f, 8.0f, 2.0f);
    }
  }
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
  SDL_Renderer *r = game->renderer;
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
    draw_hud_heart(r, 99.0f + i * 14.0f, 19.0f, i < game->lives);
  if (game->lives > 5)
  {
    char life_buf[8];
    SDL_snprintf(life_buf, sizeof(life_buf), "+%d", game->lives - 5);
    draw_text(r, 168.0f, 20.0f, 0.9f, 246, 110, 96, life_buf);
  }
  draw_hud_separator(r, 190.0f);

  /* Ammunition as brass rounds standing in a rack. */
  draw_text(r, 201.0f, 8.0f, 0.7f, label_r, label_g, label_b, "AMMO");
  for (int i = 0; i < MAX_AMMO; ++i)
  {
    float ammo_x = 202.0f + i * 7.0f;
    if (i < game->player.bullets)
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
  if (game->player.grenades > 0)
    draw_grenade(r, 252.0f, 19.0f, 0.0f);
  draw_hud_separator(r, 276.0f);

  /* Access status chip with a live LED. */
  draw_text(r, 287.0f, 8.0f, 0.7f, label_r, label_g, label_b, "ACCESS");
  bool unlocked = game->level.exit_unlocked;
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
  SDL_snprintf(level_buf, sizeof(level_buf), "%02d", game->current_level + 1);
  draw_text(r, 382.0f, 8.0f, 0.7f, label_r, label_g, label_b, "SECTOR");
  draw_text(r, 382.0f, 19.0f, 1.5f, 226, 232, 220, level_buf);
  draw_hud_separator(r, 440.0f);

  /* Score keeps its leading zeros, but only the live digits glow. */
  char score_buf[16];
  SDL_snprintf(score_buf, sizeof(score_buf), "%07d", game->score);
  draw_text(r, 451.0f, 8.0f, 0.7f, label_r, label_g, label_b, "SCORE");
  int first_digit = 0;
  while (first_digit < 6 && score_buf[first_digit] == '0')
    ++first_digit;
  draw_text(r, 451.0f, 19.0f, 1.5f, 74, 88, 102, score_buf);
  draw_text(r, 451.0f + first_digit * 12.0f, 19.0f, 1.5f, 248, 196, 92,
            score_buf + first_digit);

  /* Live signal meter fills the right edge without stealing attention. */
  float t = (float)SDL_GetTicksNS() * 1.0e-9f;
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

static void render_terminal_interaction(Game *game, int win_w, int win_h)
{
  if (game->state != STATE_PLAYING ||
      !game->terminal_in_range ||
      game->level.exit_unlocked)
  {
    return;
  }

  SDL_Renderer *r = game->renderer;
  float panel_w = 330.0f;
  float panel_h = 45.0f;
  float x = ((float)win_w - panel_w) * 0.5f;
  float y = (float)win_h - panel_h - 12.0f;
  float progress = game->terminal_hack_progress / TERMINAL_HACK_TIME;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;

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

  char label[64];
  if (game->terminal_hacking)
  {
    int percent = (int)(progress * 100.0f);
    SDL_snprintf(label, sizeof(label), "BREACHING SECURITY... %d%%", percent);
  }
  else
  {
    SDL_snprintf(label, sizeof(label), "HOLD E TO HACK ACTIVE TERMINAL");
  }
  draw_text(r, x + 12.0f, y + 9.0f, 1.25f, 174, 255, 213, label);

  color_rect(r, (SDL_Color){16, 34, 32, 255},
             x + 12.0f, y + 31.0f, panel_w - 24.0f, 7.0f);
  if (progress > 0.0f)
  {
    float bar_w = (panel_w - 24.0f) * progress;
    color_rect(r, (SDL_Color){44, 168, 118, 255},
               x + 12.0f, y + 31.0f, bar_w, 7.0f);
    color_rect(r, (SDL_Color){120, 255, 190, 255},
               x + 12.0f, y + 31.0f, bar_w, 2.0f);
    color_rect(r, (SDL_Color){190, 255, 220, 255},
               x + 12.0f + bar_w - 2.0f, y + 31.0f, 2.0f, 7.0f);
    fx_glow(r, x + 12.0f + bar_w, y + 34.0f, 14.0f,
            (SDL_Color){86, 240, 170, 255}, 80);
  }
}

static void draw_overlay_panel(Game *game, float y, SDL_Color accent,
                               const char *title, const char *subtitle)
{
  SDL_Renderer *r = game->renderer;
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
  SDL_Renderer *r = game->renderer;
  SDL_SetRenderDrawColor(r, 8, 11, 17, 255);
  SDL_RenderClear(r);

  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  if (game->state == STATE_OPENING_CUTSCENE)
  {
    opening_cutscene_render(r, &game->opening_cutscene, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  if (game->state == STATE_INTRO)
  {
    intro_render(r, &game->intro, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  if (game->state == STATE_LEVEL_TRANSITION)
  {
    level_transition_render(r, &game->level_transition, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  if (game->state == STATE_OUTRO)
  {
    outro_cutscene_render(r, &game->outro_cutscene, win_w, win_h);
    SDL_RenderPresent(r);
    return;
  }

  render_world(game);
  render_hud(game);
  render_terminal_interaction(game, win_w, win_h);

  if (game->exit_unlocked_timer > 0.0f)
  {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_rgba(r, 7, 20, 20, 215);
    fill_rect(r, 226.0f, 47.0f, 348.0f, 31.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    draw_text_centered(game, 63.0f, 2.2f, 94, 255, 201, "PURSUIT ROUTE OPEN");
    float exit_screen = game->level.exit_col * (float)TILE_SIZE - game->cam_x;
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
  else if (game->state == STATE_WIN)
    draw_overlay_panel(game, 225.0f, (SDL_Color){91, 237, 169, 255},
                       "SHE'S SAFE", "PRESS R TO REPLAY THE RESCUE");

  /* One shared finishing pass keeps every frame looking like the same film:
   * gentle scanlines for texture and a vignette that focuses the action. */
  fx_vignette(r, win_w, win_h, 58);
  fx_scanlines(r, win_w, win_h, 11);

  SDL_RenderPresent(r);
}
