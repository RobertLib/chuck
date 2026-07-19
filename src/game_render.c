#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>

/*
 * Chuck intentionally ships without art assets.  Everything in this file is
 * assembled at runtime from small, hard-edged shapes.  The limited palette,
 * one-pixel highlights and chunky silhouettes keep the result readable as
 * pixel art while still giving the infiltration levels a coherent industrial
 * setting.
 */

static const SDL_Color COL_INK = {8, 11, 17, 255};
static const SDL_Color COL_OUTLINE = {14, 19, 28, 255};
static const SDL_Color COL_CYAN = {48, 211, 210, 255};
static const SDL_Color COL_AMBER = {247, 174, 55, 255};
static const SDL_Color COL_RED = {224, 58, 54, 255};

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
  SDL_BlendMode old = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &old);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, color.r, color.g, color.b, 22);
  fill_rect(r, x - 5.0f, y - 5.0f, w + 10.0f, h + 10.0f);
  set_rgba(r, color.r, color.g, color.b, 45);
  fill_rect(r, x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f);
  SDL_SetRenderDrawBlendMode(r, old);
}

static void render_background(Game *game, int win_w, int win_h)
{
  SDL_Renderer *r = game->renderer;
  const float oy = HUD_HEIGHT;

  /* A banded near-black gradient reads as intentionally pixelated. */
  for (int y = HUD_HEIGHT; y < win_h; y += 16)
  {
    float p = (float)(y - HUD_HEIGHT) / (float)(win_h - HUD_HEIGHT);
    Uint8 rr = (Uint8)(12.0f + p * 5.0f);
    Uint8 gg = (Uint8)(18.0f + p * 7.0f);
    Uint8 bb = (Uint8)(29.0f + p * 10.0f);
    set_rgba(r, rr, gg, bb, 255);
    fill_rect(r, 0.0f, (float)y, (float)win_w, 16.0f);
  }

  /* Far service bays move slowly to establish depth during side-scrolling. */
  float far_shift = fmodf(-game->cam_x * 0.12f, 192.0f);
  if (far_shift > 0.0f)
    far_shift -= 192.0f;
  for (float x = far_shift - 32.0f; x < (float)win_w + 192.0f; x += 192.0f)
  {
    color_rect(r, (SDL_Color){17, 25, 37, 255}, x + 16.0f, oy + 28.0f, 144.0f, (float)win_h - oy - 50.0f);
    color_rect(r, (SDL_Color){22, 33, 46, 255}, x + 20.0f, oy + 32.0f, 136.0f, 4.0f);
    color_rect(r, (SDL_Color){9, 15, 24, 255}, x + 28.0f, oy + 55.0f, 120.0f, 76.0f);
    color_rect(r, (SDL_Color){35, 48, 60, 255}, x + 32.0f, oy + 59.0f, 112.0f, 3.0f);

    /* Dim windows / equipment screens in the distant bays. */
    for (int lamp = 0; lamp < 4; ++lamp)
    {
      unsigned h = tile_hash((int)x / 8 + lamp, game->current_level + 7);
      SDL_Color lc = (h & 1u) ? (SDL_Color){39, 102, 111, 255}
                              : (SDL_Color){105, 72, 38, 255};
      color_rect(r, lc, x + 36.0f + lamp * 26.0f, oy + 72.0f, 12.0f, 4.0f);
    }
  }

  /* Foreground wall seams and conduit shadows use a second parallax rate. */
  float near_shift = fmodf(-game->cam_x * 0.28f, 96.0f);
  if (near_shift > 0.0f)
    near_shift -= 96.0f;
  set_rgba(r, 28, 40, 53, 150);
  for (float x = near_shift; x < (float)win_w + 96.0f; x += 96.0f)
  {
    fill_rect(r, x, oy, 2.0f, (float)win_h - oy);
    fill_rect(r, x + 2.0f, oy, 1.0f, (float)win_h - oy);
    color_rect(r, (SDL_Color){11, 17, 26, 255}, x + 66.0f, oy + 8.0f, 4.0f, 86.0f);
    color_rect(r, (SDL_Color){43, 55, 64, 255}, x + 67.0f, oy + 8.0f, 1.0f, 86.0f);
  }

  /* Tiny, low-contrast dust motes make empty rooms feel alive. */
  float t = (float)SDL_GetTicksNS() * 1.0e-9f;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < 20; ++i)
  {
    unsigned h = tile_hash(i + game->current_level * 29, 91);
    float x = fmodf((float)(h % (unsigned)(win_w + 80)) + t * (4.0f + (float)(i % 5)), (float)(win_w + 80)) - 40.0f;
    float y = oy + 18.0f + (float)((h >> 8) % (unsigned)(win_h - HUD_HEIGHT - 36));
    Uint8 a = (Uint8)(24 + (h % 22u));
    set_rgba(r, 120, 150, 158, a);
    fill_rect(r, x, y, (i % 3 == 0) ? 2.0f : 1.0f, 1.0f);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_wall_tile(SDL_Renderer *r, const Level *lvl,
                           int col, int row, float x, float y)
{
  unsigned h = tile_hash(col, row);
  Uint8 variation = (Uint8)(h % 9u);
  SDL_Color base = {(Uint8)(47 + variation), (Uint8)(57 + variation),
                    (Uint8)(64 + variation), 255};

  color_rect(r, COL_INK, x, y, TILE_SIZE, TILE_SIZE);
  color_rect(r, base, x + 1.0f, y + 1.0f, TILE_SIZE - 2.0f, TILE_SIZE - 2.0f);
  color_rect(r, (SDL_Color){65, 76, 82, 255}, x + 2.0f, y + 2.0f, TILE_SIZE - 4.0f, 2.0f);
  color_rect(r, (SDL_Color){31, 39, 47, 255}, x + 2.0f, y + TILE_SIZE - 4.0f, TILE_SIZE - 4.0f, 2.0f);
  color_rect(r, (SDL_Color){38, 47, 54, 255}, x + TILE_SIZE - 4.0f, y + 3.0f, 2.0f, TILE_SIZE - 7.0f);

  /* Exposed floor tops get a bright metal lip and a warning inset. */
  if (!level_is_solid(lvl, col, row - 1))
  {
    color_rect(r, (SDL_Color){104, 119, 120, 255}, x, y, TILE_SIZE, 3.0f);
    color_rect(r, (SDL_Color){154, 158, 139, 255}, x + 2.0f, y, TILE_SIZE - 4.0f, 1.0f);
    if ((h & 3u) == 0u)
      color_rect(r, COL_AMBER, x + 8.0f, y + 1.0f, 9.0f, 2.0f);
  }

  /* Rivets and rare little cracks stop the repeating grid feeling flat. */
  color_rect(r, (SDL_Color){112, 120, 116, 255}, x + 4.0f, y + 6.0f, 2.0f, 2.0f);
  color_rect(r, (SDL_Color){21, 28, 34, 255}, x + 5.0f, y + 7.0f, 1.0f, 1.0f);
  color_rect(r, (SDL_Color){112, 120, 116, 255}, x + 26.0f, y + 24.0f, 2.0f, 2.0f);
  if ((h % 5u) == 0u)
  {
    set_rgba(r, 29, 36, 42, 255);
    SDL_RenderLine(r, x + 12.0f, y + 9.0f, x + 16.0f, y + 14.0f);
    SDL_RenderLine(r, x + 16.0f, y + 14.0f, x + 14.0f, y + 20.0f);
    set_rgba(r, 73, 81, 82, 255);
    SDL_RenderLine(r, x + 13.0f, y + 9.0f, x + 17.0f, y + 14.0f);
  }
}

static void draw_ladder_tile(SDL_Renderer *r, float x, float y, int row)
{
  /* Shadow first, then steel rails and amber rung faces. */
  color_rect(r, (SDL_Color){7, 12, 18, 180}, x + 5.0f, y, 5.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){7, 12, 18, 180}, x + 24.0f, y, 5.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){54, 49, 34, 255}, x + 7.0f, y, 4.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){54, 49, 34, 255}, x + 22.0f, y, 4.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){207, 157, 62, 255}, x + 8.0f, y, 2.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){207, 157, 62, 255}, x + 23.0f, y, 2.0f, TILE_SIZE);
  for (int rung = -4; rung < TILE_SIZE; rung += 8)
  {
    int ry = rung + (row & 1) * 0;
    color_rect(r, (SDL_Color){55, 44, 28, 255}, x + 8.0f, y + (float)ry + 2.0f, 17.0f, 3.0f);
    color_rect(r, (SDL_Color){224, 176, 72, 255}, x + 9.0f, y + (float)ry + 1.0f, 15.0f, 2.0f);
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
  color_rect(r, (SDL_Color){8, 11, 16, 255}, x + 1.0f, y, 30.0f, 32.0f);
  color_rect(r, (SDL_Color){82, 65, 48, 255}, x + 3.0f, y + 1.0f, 26.0f, 31.0f);
  color_rect(r, (SDL_Color){125, 94, 58, 255}, x + 5.0f, y + 3.0f, 22.0f, 27.0f);
  color_rect(r, (SDL_Color){59, 50, 44, 255}, x + 7.0f, y + 5.0f, 18.0f, 9.0f);
  color_rect(r, (SDL_Color){25, 30, 34, 255}, x + 9.0f, y + 7.0f, 14.0f, 5.0f);
  for (int line = 0; line < 3; ++line)
    color_rect(r, (SDL_Color){135, 111, 76, 255}, x + 8.0f, y + 18.0f + line * 3.0f, 12.0f, 1.0f);

  color_rect(r, (SDL_Color){16, 23, 28, 255}, x + 22.0f, y + 17.0f, 5.0f, 8.0f);
  color_rect(r, (SDL_Color){70, 213, 180, 255}, x + 23.0f, y + 18.0f, 3.0f, 2.0f);
  char label[3];
  SDL_snprintf(label, sizeof(label), "%d", index / 2 + 1);
  draw_text(r, x + 12.0f, y + 6.0f, 0.65f, 111, 153, 151, label);
}

static void draw_exit(SDL_Renderer *r, const Game *game, float x, float y)
{
  bool unlocked = game->level.items_remaining == 0;
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
  /* The forward leg lifts while the rear leg remains planted.  Moving the
     knee and ankle horizontally makes the step readable at 26x32 pixels. */
  float lift = fmaxf(0.0f, stride) * 0.48f;
  float knee_x = hip_x + stride * 0.42f;
  float knee_y = 26.0f - lift * 0.25f;
  float ankle_x = hip_x + stride * 0.90f;
  float ankle_y = 30.0f - lift;

  sprite_limb_segment(r, x, y, sprite_w, dir,
                      hip_x, hip_y, knee_x, knee_y, trouser);
  sprite_limb_segment(r, x, y, sprite_w, dir,
                      knee_x, knee_y, ankle_x, ankle_y, trouser);
  sprite_rect(r, x, y, sprite_w, dir,
              ankle_x - 2.0f, ankle_y - 1.0f, 8.0f, 3.0f, COL_OUTLINE);
  sprite_rect(r, x, y, sprite_w, dir,
              ankle_x - 1.0f, ankle_y, 6.0f, 2.0f, boot);
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
  bool airborne = !p->on_ground && !climbing;
  bool firing = p->action_timer > 0.0f;
  float step = moving && p->on_ground ? sinf(phase) : 0.0f;
  float bob = moving && p->on_ground ? fabsf(step) * 0.55f
                                     : sinf(p->anim_time * 2.0f) * 0.35f;
  float arm_swing = -step;

  color_rect(r, (SDL_Color){3, 6, 10, 125}, x + 3.0f, y + 30.0f, 20.0f, 3.0f);

  if (climbing)
  {
    float climb = sinf(phase) * 4.0f;
    sprite_rect(r, x, y, PLAYER_W, dir, 5.0f, 13.0f - climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 13.0f + climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 6.0f, 14.0f - climb, 3.0f, 8.0f, (SDL_Color){51, 130, 159, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, 14.0f + climb, 3.0f, 8.0f, (SDL_Color){51, 130, 159, 255});
    sprite_rect(r, x, y, PLAYER_W, dir, 7.0f, 23.0f + climb, 5.0f, 8.0f, COL_OUTLINE);
    sprite_rect(r, x, y, PLAYER_W, dir, 15.0f, 23.0f - climb, 5.0f, 8.0f, COL_OUTLINE);
    bob = fabsf(sinf(phase)) * 0.7f;
  }
  else
  {
    if (moving && p->on_ground)
    {
      draw_walking_leg(r, x, y, PLAYER_W, dir, 12.0f, 21.0f + bob,
                       -step * 5.0f, (SDL_Color){25, 57, 82, 255},
                       (SDL_Color){12, 20, 30, 255});
      draw_walking_leg(r, x, y, PLAYER_W, dir, 14.0f, 21.0f + bob,
                       step * 5.0f, (SDL_Color){38, 82, 111, 255},
                       (SDL_Color){18, 28, 39, 255});
    }
    else
    {
      float leg_a_y = airborne ? 24.0f : 22.0f;
      float leg_b_y = airborne ? 21.0f : 22.0f;
      float leg_a_h = airborne ? 7.0f : 10.0f;
      float leg_b_h = airborne ? 7.0f : 10.0f;
      sprite_rect(r, x, y, PLAYER_W, dir, 6.0f, leg_a_y, 6.0f, leg_a_h, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir, 15.0f, leg_b_y, 6.0f, leg_b_h, COL_OUTLINE);
      sprite_rect(r, x, y, PLAYER_W, dir, 7.0f, leg_a_y, 4.0f, fmaxf(3.0f, leg_a_h - 2.0f), (SDL_Color){28, 63, 92, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, leg_b_y, 4.0f, fmaxf(3.0f, leg_b_h - 2.0f), (SDL_Color){35, 78, 105, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 5.0f, leg_a_y + leg_a_h - 3.0f, 8.0f, 3.0f, (SDL_Color){15, 24, 35, 255});
      sprite_rect(r, x, y, PLAYER_W, dir, 14.0f, leg_b_y + leg_b_h - 3.0f, 8.0f, 3.0f, (SDL_Color){15, 24, 35, 255});
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
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 12.0f + bob, 4.0f, 9.0f, (SDL_Color){60, 148, 171, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 12.0f + bob, 2.0f, 11.0f, (SDL_Color){21, 54, 76, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 20.0f + bob, 11.0f, 2.0f, COL_AMBER);

  /* Head, hair and signature red headband. */
  sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 1.0f + bob, 10.0f, 11.0f, COL_OUTLINE);
  sprite_rect(r, x, y, PLAYER_W, dir, 10.0f, 2.0f + bob, 8.0f, 9.0f, (SDL_Color){210, 154, 105, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 9.0f, 1.0f + bob, 10.0f, 4.0f, (SDL_Color){70, 38, 28, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 12.0f, 0.0f + bob, 7.0f, 2.0f, (SDL_Color){91, 48, 31, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 16.0f, 5.0f + bob, 2.0f, 2.0f, (SDL_Color){230, 242, 216, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 17.0f, 7.0f + bob, 2.0f, 1.0f, (SDL_Color){83, 40, 27, 255});
  sprite_rect(r, x, y, PLAYER_W, dir, 8.0f, 4.0f + bob, 12.0f, 2.0f, COL_RED);
  sprite_rect(r, x, y, PLAYER_W, dir, 5.0f, 5.0f + bob, 4.0f, 2.0f, (SDL_Color){166, 38, 42, 255});

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
  bool aiming = e->aim_timer > 0.0f || e->recoil_timer > 0.0f;
  bool moving = fabsf(e->vx) > 2.0f && !aiming && !e->talking;
  float phase = e->anim_time * 3.0f;
  float step = moving ? sinf(phase) : 0.0f;
  float bob = moving ? fabsf(step) * 0.5f : sinf(e->anim_time * 1.8f) * 0.3f;
  SDL_Color uniform = e->hp >= ENEMY_HP ? (SDL_Color){76, 91, 69, 255}
                                           : e->hp == 2 ? (SDL_Color){103, 83, 54, 255}
                                                        : (SDL_Color){101, 65, 49, 255};
  SDL_Color light = e->hp >= ENEMY_HP ? (SDL_Color){116, 129, 86, 255}
                                        : (SDL_Color){135, 98, 58, 255};

  color_rect(r, (SDL_Color){3, 6, 9, 125}, x + 3.0f, y + 30.0f, 20.0f, 3.0f);

  if (e->climbing)
  {
    float climb = sinf(phase) * 4.0f;
    sprite_rect(r, x, y, ENEMY_W, dir, 4.0f, 13.0f - climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 17.0f, 13.0f + climb, 5.0f, 10.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 5.0f, 14.0f - climb, 3.0f, 8.0f, uniform);
    sprite_rect(r, x, y, ENEMY_W, dir, 18.0f, 14.0f + climb, 3.0f, 8.0f, uniform);
    sprite_rect(r, x, y, ENEMY_W, dir, 6.0f, 23.0f + climb, 6.0f, 8.0f, COL_OUTLINE);
    sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 23.0f - climb, 6.0f, 8.0f, COL_OUTLINE);
  }
  else
  {
    if (moving)
    {
      draw_walking_leg(r, x, y, ENEMY_W, dir, 12.0f, 21.0f + bob,
                       -step * 4.5f, (SDL_Color){43, 50, 40, 255}, COL_INK);
      draw_walking_leg(r, x, y, ENEMY_W, dir, 14.0f, 21.0f + bob,
                       step * 4.5f, (SDL_Color){67, 77, 57, 255},
                       (SDL_Color){12, 15, 14, 255});
    }
    else
    {
      sprite_rect(r, x, y, ENEMY_W, dir, 6.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
      sprite_rect(r, x, y, ENEMY_W, dir, 15.0f, 22.0f, 6.0f, 10.0f, COL_OUTLINE);
      sprite_rect(r, x, y, ENEMY_W, dir, 7.0f, 22.0f, 4.0f, 7.0f, (SDL_Color){42, 49, 39, 255});
      sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 22.0f, 4.0f, 7.0f, (SDL_Color){42, 49, 39, 255});
      sprite_rect(r, x, y, ENEMY_W, dir, 4.0f, 29.0f, 9.0f, 3.0f, COL_INK);
      sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 29.0f, 9.0f, 3.0f, COL_INK);
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
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 12.0f + bob, 4.0f, 8.0f, light);
  sprite_rect(r, x, y, ENEMY_W, dir, 11.0f, 12.0f + bob, 3.0f, 11.0f, (SDL_Color){38, 45, 39, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 14.0f, 13.0f + bob, 4.0f, 5.0f, (SDL_Color){30, 35, 31, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 20.0f + bob, 11.0f, 2.0f, (SDL_Color){31, 37, 31, 255});

  /* Helmeted head, red visor/insignia for immediate enemy readability. */
  sprite_rect(r, x, y, ENEMY_W, dir, 9.0f, 2.0f + bob, 10.0f, 10.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 4.0f + bob, 8.0f, 7.0f, (SDL_Color){183, 132, 91, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 8.0f, 1.0f + bob, 12.0f, 5.0f, (SDL_Color){47, 57, 43, 255});
  sprite_rect(r, x, y, ENEMY_W, dir, 10.0f, 2.0f + bob, 8.0f, 2.0f, light);
  sprite_rect(r, x, y, ENEMY_W, dir, 16.0f, 6.0f + bob, 3.0f, 2.0f, COL_RED);
  sprite_rect(r, x, y, ENEMY_W, dir, 17.0f, 9.0f + bob, 2.0f, 1.0f, (SDL_Color){70, 34, 27, 255});

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
    set_rgba(r, 255, 183, 38, 75);
    fill_rect(r, x - (b->vx > 0.0f ? 8.0f : -8.0f), y + 1.0f, 12.0f, 2.0f);
    color_rect(r, (SDL_Color){255, 243, 170, 255}, x, y, BULLET_W, BULLET_H);
  }
  for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
  {
    const Bullet *b = &game->enemy_bullets[i];
    if (!b->active)
      continue;
    float x = b->x - cam_x;
    float y = b->y + oy;
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
  color_rect(r, (SDL_Color){52, 73, 84, 255}, x, 7.0f, 1.0f, 26.0f);
  color_rect(r, (SDL_Color){8, 13, 20, 255}, x + 1.0f, 7.0f, 1.0f, 26.0f);
}

static void render_hud(Game *game)
{
  SDL_Renderer *r = game->renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;

  color_rect(r, (SDL_Color){7, 12, 19, 255}, 0.0f, 0.0f, (float)win_w, HUD_HEIGHT);
  color_rect(r, (SDL_Color){24, 39, 50, 255}, 0.0f, 2.0f, (float)win_w, 34.0f);
  color_rect(r, (SDL_Color){40, 67, 74, 255}, 0.0f, 36.0f, (float)win_w, 2.0f);
  color_rect(r, COL_CYAN, 0.0f, 38.0f, (float)win_w, 1.0f);

  draw_text(r, 10.0f, 10.0f, 1.5f, 97, 230, 218, "CHUCK");
  draw_text(r, 10.0f, 25.0f, 0.75f, 135, 157, 157, "RESCUE OPS");
  draw_hud_separator(r, 88.0f);

  /* Lives: small red field crosses instead of an abstract number alone. */
  draw_text(r, 99.0f, 8.0f, 0.75f, 145, 164, 165, "VITAL");
  for (int i = 0; i < game->lives && i < 5; ++i)
  {
    color_rect(r, (SDL_Color){121, 32, 35, 255}, 99.0f + i * 12.0f, 20.0f, 10.0f, 10.0f);
    color_rect(r, (SDL_Color){239, 78, 69, 255}, 103.0f + i * 12.0f, 22.0f, 2.0f, 6.0f);
    color_rect(r, (SDL_Color){239, 78, 69, 255}, 101.0f + i * 12.0f, 24.0f, 6.0f, 2.0f);
  }
  if (game->lives > 5)
  {
    char life_buf[8];
    SDL_snprintf(life_buf, sizeof(life_buf), "+%d", game->lives - 5);
    draw_text(r, 161.0f, 20.0f, 1.0f, 239, 102, 82, life_buf);
  }
  draw_hud_separator(r, 190.0f);

  draw_text(r, 201.0f, 8.0f, 0.75f, 145, 164, 165, "AMMO");
  const float ammo_slot_spacing = 7.0f;
  for (int i = 0; i < MAX_AMMO; ++i)
  {
    SDL_Color bullet = i < game->player.bullets ? COL_AMBER
                                                : (SDL_Color){63, 64, 58, 255};
    float ammo_x = 202.0f + i * ammo_slot_spacing;
    color_rect(r, bullet, ammo_x, 20.0f, 3.0f, 10.0f);
    color_rect(r, (SDL_Color){99, 72, 34, 255}, ammo_x, 28.0f, 5.0f, 2.0f);
  }
  if (game->player.grenades > 0)
    draw_grenade(r, 252.0f, 19.0f, 0.0f);
  draw_hud_separator(r, 276.0f);

  draw_text(r, 287.0f, 8.0f, 0.75f, 145, 164, 165, "ACCESS");
  if (game->level.items_remaining == 0)
  {
    color_rect(r, (SDL_Color){35, 139, 116, 255}, 287.0f, 20.0f, 61.0f, 11.0f);
    draw_text(r, 291.0f, 21.0f, 1.0f, 191, 255, 224, "GRANTED");
  }
  else
  {
    color_rect(r, (SDL_Color){94, 43, 39, 255}, 287.0f, 20.0f, 61.0f, 11.0f);
    draw_text(r, 295.0f, 21.0f, 1.0f, 247, 151, 112, "LOCKED");
  }
  draw_hud_separator(r, 363.0f);

  char level_buf[32];
  SDL_snprintf(level_buf, sizeof(level_buf), "SECTOR %02d", game->current_level + 1);
  draw_text(r, 374.0f, 8.0f, 0.75f, 145, 164, 165, "RESCUE");
  draw_text(r, 374.0f, 21.0f, 1.0f, 219, 226, 211, level_buf);
  draw_hud_separator(r, 478.0f);

  char score_buf[40];
  SDL_snprintf(score_buf, sizeof(score_buf), "%07d", game->score);
  draw_text(r, 489.0f, 8.0f, 0.75f, 145, 164, 165, "SCORE");
  draw_text(r, 489.0f, 20.0f, 1.25f, 247, 196, 90, score_buf);

  /* Decorative live telemetry fills the otherwise empty right edge. */
  float pulse = 0.5f + 0.5f * sinf((float)SDL_GetTicksNS() * 1.0e-9f * 3.0f);
  draw_text(r, 650.0f, 8.0f, 0.75f, 119, 151, 153, "TRAIL");
  color_rect(r, (SDL_Color){38, 66, 71, 255}, 650.0f, 21.0f, 130.0f, 8.0f);
  for (int i = 0; i < 12; ++i)
  {
    float height = 2.0f + fmodf((float)(i * 7) + pulse * 8.0f, 6.0f);
    color_rect(r, i < 9 ? COL_CYAN : (SDL_Color){63, 87, 91, 255},
               654.0f + i * 10.0f, 27.0f - height, 6.0f, height);
  }
}

static void draw_overlay_panel(Game *game, float y, SDL_Color accent,
                               const char *title, const char *subtitle)
{
  SDL_Renderer *r = game->renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 5, 9, 15, 225);
  fill_rect(r, 0.0f, y - 35.0f, (float)win_w, subtitle ? 93.0f : 70.0f);
  set_rgba(r, accent.r, accent.g, accent.b, 220);
  fill_rect(r, 0.0f, y - 35.0f, (float)win_w, 3.0f);
  fill_rect(r, 0.0f, y + (subtitle ? 55.0f : 32.0f), (float)win_w, 2.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
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

  render_world(game);
  render_hud(game);

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
                       "YOU FOUND HER", NULL);
  else if (game->state == STATE_GAME_OVER)
    draw_overlay_panel(game, 225.0f, (SDL_Color){235, 72, 65, 255},
                       "THE TRAIL WENT COLD", "PRESS R TO RESTART THE PURSUIT");
  else if (game->state == STATE_WIN)
    draw_overlay_panel(game, 225.0f, (SDL_Color){91, 237, 169, 255},
                       "SHE'S SAFE", "PRESS R TO REPLAY THE RESCUE");

  SDL_RenderPresent(r);
}
