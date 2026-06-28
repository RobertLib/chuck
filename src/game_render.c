#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

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

static void fill_rect(SDL_Renderer *r, float x, float y, float w, float h)
{
  SDL_FRect rect = {x, y, w, h};
  SDL_RenderFillRect(r, &rect);
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
  float text_w = (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  float text_h = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  float x = (win_w - text_w) * 0.5f;
  float y = center_y - text_h * 0.5f;
  draw_text(game->renderer, x, y, scale, cr, cg, cb, text);
}

static void render_world(Game *game)
{
  SDL_Renderer *r = game->renderer;
  const Level *lvl = &game->level;
  const Sprites *spr = &game->sprites;
  const float oy = HUD_HEIGHT;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  const float cam_x = game->cam_x;

  /* Tiles */
  for (int row = 0; row < lvl->height; ++row)
  {
    for (int col = 0; col < lvl->width; ++col)
    {
      float x = col * (float)TILE_SIZE - cam_x;
      float y = row * (float)TILE_SIZE + oy;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w)
        continue;
      /* Only draw tiles that have been revealed yet. */
      if (!lvl->tiles_visible[row][col])
        continue;
      TileType t = lvl->tiles[row][col];
      if (t == TILE_WALL)
      {
        if (spr->tileset)
        {
          SDL_FRect src = {(float)(SPRITE_TILE_WALL * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
          SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
          SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
          SDL_SetRenderDrawColor(r, 110, 70, 40, 255);
          fill_rect(r, x, y, TILE_SIZE, TILE_SIZE);
          SDL_SetRenderDrawColor(r, 140, 95, 60, 255);
          fill_rect(r, x, y, TILE_SIZE, 4);
        }
      }
      else if (t == TILE_LADDER)
      {
        if (spr->tileset)
        {
          SDL_FRect src = {(float)(SPRITE_TILE_LADDER * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
          SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
          SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
          SDL_SetRenderDrawColor(r, 210, 200, 90, 255);
          SDL_RenderLine(r, x + 6, y, x + 6, y + TILE_SIZE);
          SDL_RenderLine(r, x + TILE_SIZE - 6, y, x + TILE_SIZE - 6, y + TILE_SIZE);
          for (int rung = 4; rung < TILE_SIZE; rung += 10)
          {
            SDL_RenderLine(r, x + 6, y + rung, x + TILE_SIZE - 6, y + rung);
          }
        }
      }
      else if (t == TILE_ELEVATOR_SHAFT)
      {
        if (spr->tileset)
        {
          SDL_FRect src = {(float)(SPRITE_TILE_ELEVATOR_SHAFT * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
          SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
          SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
          /* Draw two vertical guide rails */
          SDL_SetRenderDrawColor(r, 100, 105, 115, 255);
          SDL_RenderLine(r, x + 9, y, x + 9, y + TILE_SIZE);
          SDL_RenderLine(r, x + TILE_SIZE - 9, y, x + TILE_SIZE - 9, y + TILE_SIZE);
        }
      }
    }
  }

  /* If reveal animation still in progress, don't draw other entities yet. */
  if (!lvl->reveal_done)
    return;

  /* Elevator moving platforms */
  for (int i = 0; i < lvl->elevator_count; ++i)
  {
    const Elevator *el = &lvl->elevators[i];
    float px = el->col * (float)TILE_SIZE - cam_x;
    float py = el->y + oy;
    if (spr->tileset)
    {
      SDL_FRect src = {(float)(SPRITE_PLAT_ELEV * SPRITE_CELL), (float)SPRITE_PLAT_ROW_Y, SPRITE_CELL, ELEVATOR_PLAT_H};
      SDL_FRect dst = {px, py, TILE_SIZE, ELEVATOR_PLAT_H};
      SDL_RenderTexture(r, spr->tileset, &src, &dst);
    }
    else
    {
      SDL_SetRenderDrawColor(r, 160, 165, 175, 255);
      fill_rect(r, px, py, TILE_SIZE, ELEVATOR_PLAT_H);
      SDL_SetRenderDrawColor(r, 220, 225, 235, 255);
      fill_rect(r, px, py, TILE_SIZE, 2);
    }
  }

  /* Falling platforms */
  for (int i = 0; i < lvl->fall_platform_count; ++i)
  {
    const FallPlatform *fp = &lvl->fall_platforms[i];
    if (fp->removed)
      continue;
    float px = fp->col * (float)TILE_SIZE - cam_x;
    float py = fp->y + oy;
    if (spr->tileset)
    {
      SDL_FRect src = {(float)(SPRITE_PLAT_FALL * SPRITE_CELL), (float)SPRITE_PLAT_ROW_Y, SPRITE_CELL, FALL_PLATFORM_H};
      SDL_FRect dst = {px, py, TILE_SIZE, FALL_PLATFORM_H};
      SDL_RenderTexture(r, spr->tileset, &src, &dst);
    }
    else
    {
      SDL_SetRenderDrawColor(r, 150, 90, 50, 255);
      fill_rect(r, px, py, TILE_SIZE, FALL_PLATFORM_H);
      SDL_SetRenderDrawColor(r, 210, 160, 120, 255);
      fill_rect(r, px, py, TILE_SIZE, 2);
    }
  }

  /* Moving horizontal platforms */
  for (int i = 0; i < lvl->moving_platform_count; ++i)
  {
    const MovingPlatform *mp = &lvl->moving_platforms[i];
    float px = mp->x - cam_x;
    float py = mp->row * (float)TILE_SIZE + oy;
    if (spr->tileset)
    {
      SDL_FRect src = {(float)(SPRITE_PLAT_MOVE * SPRITE_CELL), (float)SPRITE_PLAT_ROW_Y, SPRITE_CELL, MOVING_PLATFORM_H};
      SDL_FRect dst = {px, py, TILE_SIZE, MOVING_PLATFORM_H};
      SDL_RenderTexture(r, spr->tileset, &src, &dst);
    }
    else
    {
      SDL_SetRenderDrawColor(r, 140, 95, 60, 255);
      fill_rect(r, px, py, TILE_SIZE, MOVING_PLATFORM_H);
      SDL_SetRenderDrawColor(r, 210, 160, 120, 255);
      fill_rect(r, px, py, TILE_SIZE, 2);
    }
  }

  /* Doors (all identical — part of the puzzle) */
  for (int d = 0; d < lvl->door_count; ++d)
  {
    float x = lvl->doors[d].col * (float)TILE_SIZE - cam_x;
    float y = lvl->doors[d].row * (float)TILE_SIZE + oy;
    if (spr->tileset)
    {
      SDL_FRect src = {(float)(SPRITE_TILE_DOOR * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
      SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
      SDL_RenderTexture(r, spr->tileset, &src, &dst);
    }
    else
    {
      SDL_SetRenderDrawColor(r, 75, 45, 15, 255);
      fill_rect(r, x, y, TILE_SIZE, TILE_SIZE);
      SDL_SetRenderDrawColor(r, 115, 65, 25, 255);
      fill_rect(r, x + 3, y + 3, TILE_SIZE - 6, TILE_SIZE - 3);
      SDL_SetRenderDrawColor(r, 195, 165, 55, 255);
      fill_rect(r, x + TILE_SIZE - 10, y + TILE_SIZE / 2 - 2, 5, 5);
    }
  }

  /* Exit */
  if (lvl->has_exit)
  {
    float x = lvl->exit_col * (float)TILE_SIZE - cam_x;
    float y = lvl->exit_row * (float)TILE_SIZE + oy;
    if (spr->tileset)
    {
      int tile_idx = (lvl->items_remaining == 0) ? SPRITE_TILE_EXIT_ON : SPRITE_TILE_EXIT_OFF;
      SDL_FRect src = {(float)(tile_idx * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
      SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
      SDL_RenderTexture(r, spr->tileset, &src, &dst);
    }
    else
    {
      if (lvl->items_remaining == 0)
      {
        SDL_SetRenderDrawColor(r, 60, 220, 90, 255);
      }
      else
      {
        SDL_SetRenderDrawColor(r, 80, 80, 90, 255);
      }
      fill_rect(r, x + 4, y + 2, TILE_SIZE - 8, TILE_SIZE - 2);
      SDL_SetRenderDrawColor(r, 30, 30, 35, 255);
      fill_rect(r, x + TILE_SIZE - 12, y + TILE_SIZE / 2 - 2, 4, 4);
    }
  }

  /* Items */
  SDL_BlendMode _prev_blend = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &_prev_blend);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  int card_pos = 0;
  for (int i = 0; i < lvl->item_count; ++i)
  {
    const Item *it = &lvl->items[i];
    if (it->collected)
      continue;
    float x = it->x - 7.0f - cam_x;
    float y = it->y - 9.0f + oy;
    {
      float t = (float)SDL_GetTicksNS() * 1e-9f;
      const float TWO_PI = 6.283185307179586f;
      const float FREQ = 0.5f;
      const float AMP = 3.0f;
      float phase = (float)i * 0.7f;
      float bob = sinf(t * TWO_PI * FREQ + phase) * AMP;
      y += bob;
    }

    if (it->type == ITEM_CARD)
    {
      Uint8 alpha = 255;
      if (game->state == STATE_SHOW_KEYCARD)
        alpha = (card_pos == game->card_anim_current) ? 255 : 90;
      if (spr->items)
      {
        SDL_SetTextureAlphaMod(spr->items, alpha);
        SDL_FRect src = {(float)(SPRITE_ITEM_CARD * SPRITE_CELL), 0, 14, 18};
        SDL_FRect dst = {x, y, 14, 18};
        SDL_RenderTexture(r, spr->items, &src, &dst);
        SDL_SetTextureAlphaMod(spr->items, 255);
      }
      else
      {
        SDL_SetRenderDrawColor(r, 30, 190, 200, alpha);
        fill_rect(r, x, y, 14, 18);
        SDL_SetRenderDrawColor(r, 200, 255, 255, alpha);
        fill_rect(r, x, y, 14, 2);
        fill_rect(r, x, y + 16, 14, 2);
        fill_rect(r, x, y, 2, 18);
        fill_rect(r, x + 12, y, 2, 18);
        SDL_SetRenderDrawColor(r, 255, 255, 255, alpha);
        fill_rect(r, x + 5, y + 7, 4, 4);
      }
      card_pos++;
    }
    else if (it->type == ITEM_GUN)
    {
      if (spr->items)
      {
        SDL_FRect src = {(float)(SPRITE_ITEM_GUN * SPRITE_CELL), 0, 15, 12};
        SDL_FRect dst = {x + 4, y + 2, 15, 12};
        SDL_RenderTexture(r, spr->items, &src, &dst);
      }
      else
      {
        SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
        fill_rect(r, x + 4, y + 2, 11, 4);
        fill_rect(r, x + 4, y + 5, 6, 7);
        SDL_SetRenderDrawColor(r, 140, 140, 140, 255);
        fill_rect(r, x + 4, y + 2, 11, 2);
      }
    }
    else if (it->type == ITEM_GRENADE)
    {
      if (spr->items)
      {
        SDL_FRect src = {(float)(SPRITE_ITEM_GRENADE * SPRITE_CELL), 0, 13, 14};
        SDL_FRect dst = {x + 3.0f, y + 3.0f, 13, 14};
        SDL_RenderTexture(r, spr->items, &src, &dst);
      }
      else
      {
        SDL_SetRenderDrawColor(r, 140, 90, 40, 255);
        fill_rect(r, x + 3.0f, y + 4.0f, GRENADE_W, GRENADE_H);
        SDL_SetRenderDrawColor(r, 220, 180, 110, 255);
        fill_rect(r, x + 5.0f, y + 3.0f, 2.0f, 2.0f);
      }
    }
    else if (it->type == ITEM_MEDKIT)
    {
      if (spr->items)
      {
        SDL_FRect src = {(float)(SPRITE_ITEM_MEDKIT * SPRITE_CELL), 0, 14, 14};
        SDL_FRect dst = {x + 3.0f, y + 3.0f, 14, 14};
        SDL_RenderTexture(r, spr->items, &src, &dst);
      }
      else
      {
        SDL_SetRenderDrawColor(r, 200, 40, 40, 255);
        fill_rect(r, x + 3.0f, y + 3.0f, 14, 14);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        fill_rect(r, x + 8.0f - 2.0f, y + 3.0f + 3.0f, 4.0f, 8.0f);
        fill_rect(r, x + 3.0f + 3.0f, y + 8.0f - 2.0f, 8.0f, 4.0f);
      }
    }
  }
  SDL_SetRenderDrawBlendMode(r, _prev_blend);

  /* Spikes */
  for (int i = 0; i < lvl->spike_count; ++i)
  {
    const SpikeSpawn *s = &lvl->spike_spawns[i];
    float px = s->x - cam_x;
    float py = s->y + oy;
    for (int k = 0; k < TILE_SIZE; k += 8)
    {
      SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
      fill_rect(r, px + (float)k, py + (float)TILE_SIZE - 12.0f, 6.0f, 12.0f);
      SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
      fill_rect(r, px + (float)k + 1.0f, py + (float)TILE_SIZE - 10.0f, 4.0f, 10.0f);
    }
  }

  /* Mines */
  for (int i = 0; i < game->mine_count; ++i)
  {
    const Mine *m = &game->mines[i];
    if (!m->active)
      continue;
    float x = m->x - cam_x;
    float y = m->y + oy;
    if (spr->items)
    {
      SDL_FRect src = {(float)(SPRITE_ITEM_MINE * SPRITE_CELL), 0, MINE_W, MINE_H};
      SDL_FRect dst = {x, y, MINE_W, MINE_H};
      SDL_RenderTexture(r, spr->items, &src, &dst);
    }
    else
    {
      SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
      fill_rect(r, x, y, MINE_W, MINE_H);
      SDL_SetRenderDrawColor(r, 220, 50, 50, 255);
      fill_rect(r, x + MINE_W * 0.5f - 2.0f, y + 2.0f, 4.0f, 4.0f);
    }
  }

  /* Enemies */
  for (int i = 0; i < game->enemy_count; ++i)
  {
    const Enemy *e = &game->enemies[i];
    if (e->dead)
      continue;
    float x = e->x - cam_x;
    float y = e->y + oy;
    if (spr->enemy)
    {
      SDL_FRect src = {0, 0, ENEMY_W, ENEMY_H};
      SDL_FRect dst = {x, y, ENEMY_W, ENEMY_H};
      if (e->dir < 0)
        SDL_RenderTextureRotated(r, spr->enemy, &src, &dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
      else
        SDL_RenderTexture(r, spr->enemy, &src, &dst);
    }
    else
    {
      if (e->hp >= ENEMY_HP)
        SDL_SetRenderDrawColor(r, 210, 50, 50, 255);
      else if (e->hp == 2)
        SDL_SetRenderDrawColor(r, 210, 130, 50, 255);
      else
        SDL_SetRenderDrawColor(r, 200, 200, 50, 255);
      fill_rect(r, x, y, ENEMY_W, ENEMY_H);
      SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
      fill_rect(r, x + 4, y + 6, 4, 4);
      fill_rect(r, x + ENEMY_W - 8, y + 6, 4, 4);
    }
    for (int h = 0; h < ENEMY_HP; ++h)
    {
      if (h < e->hp)
        SDL_SetRenderDrawColor(r, 60, 220, 60, 255);
      else
        SDL_SetRenderDrawColor(r, 70, 30, 30, 255);
      fill_rect(r, x + 2 + h * 7, y - 6, 5, 4);
    }
    if (e->talking)
    {
      float bw = 20.0f;
      float bh = 10.0f;
      float bx = x + (ENEMY_W - bw) * 0.5f;
      float by = y - 18.0f - bh;
      if (by < oy)
        by = oy;
      SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
      fill_rect(r, bx, by, bw, bh);
      SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
      fill_rect(r, x + ENEMY_W * 0.5f - 2.0f, by + bh, 4.0f, 3.0f);
      SDL_SetRenderDrawColor(r, 30, 30, 35, 255);
      fill_rect(r, bx + 4.0f, by + 3.0f, 3.0f, 3.0f);
      fill_rect(r, bx + 9.0f, by + 3.0f, 3.0f, 3.0f);
      fill_rect(r, bx + 14.0f, by + 3.0f, 3.0f, 3.0f);
    }
  }

  /* Thrown grenades */
  for (int i = 0; i < game->grenade_count; ++i)
  {
    const Grenade *g = &game->grenades[i];
    if (!g->active)
      continue;
    if (spr->items)
    {
      SDL_FRect src = {(float)(SPRITE_ITEM_THROWN * SPRITE_CELL), 0, GRENADE_W, GRENADE_H};
      SDL_FRect dst = {g->x - cam_x, g->y + oy, GRENADE_W, GRENADE_H};
      SDL_RenderTexture(r, spr->items, &src, &dst);
    }
    else
    {
      SDL_SetRenderDrawColor(r, 140, 90, 40, 255);
      fill_rect(r, g->x - cam_x, g->y + oy, GRENADE_W, GRENADE_H);
      SDL_SetRenderDrawColor(r, 220, 180, 110, 255);
      fill_rect(r, g->x - cam_x + 2.0f, g->y + oy + 1.0f, 2.0f, 2.0f);
    }
  }

  /* Bullets */
  SDL_SetRenderDrawColor(r, 255, 220, 0, 255);
  for (int i = 0; i < MAX_BULLETS; ++i)
  {
    const Bullet *b = &game->bullets[i];
    if (!b->active)
      continue;
    fill_rect(r, b->x - cam_x, b->y + oy, BULLET_W, BULLET_H);
  }
  SDL_SetRenderDrawColor(r, 255, 90, 30, 255);
  for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
  {
    const Bullet *b = &game->enemy_bullets[i];
    if (!b->active)
      continue;
    fill_rect(r, b->x - cam_x, b->y + oy, BULLET_W, BULLET_H);
  }

  /* Particles */
  particle_system_render(&game->particles, r, oy, cam_x);

  /* Player */
  if (!game->player.dying)
  {
    bool blink = game->invuln_timer > 0.0f &&
                 ((int)(game->invuln_timer * 12.0f) % 2 == 0);
    if (!blink)
    {
      float x = game->player.x - cam_x;
      float y = game->player.y + oy;
      float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
      int frame = SPRITE_PLAYER_STAND;
      if (game->player.crawling)
      {
        frame = SPRITE_PLAYER_CRAWL;
      }
      else
      {
        bool is_walking = game->player.on_ground &&
                          (game->input.left || game->input.right || fabsf(game->player.vx) > 0.1f);
        if (is_walking && spr->player)
        {
          const float FRAME_TIME = 1.0f / 10.0f;
          int idx = (int)(game->player.anim_timer / FRAME_TIME) % SPRITE_PLAYER_WALK_COUNT;
          frame = SPRITE_PLAYER_WALK_START + idx;
        }
        else
        {
          frame = SPRITE_PLAYER_STAND;
        }
      }
      if (spr->player)
      {
        SDL_FRect src = {(float)(frame * SPRITE_CELL), 0, PLAYER_W, ph};
        SDL_FRect dst = {x, y, PLAYER_W, ph};
        if (game->player.facing < 0)
          SDL_RenderTextureRotated(r, spr->player, &src, &dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
        else
          SDL_RenderTexture(r, spr->player, &src, &dst);
      }
      else
      {
        SDL_SetRenderDrawColor(r, 60, 120, 230, 255);
        fill_rect(r, x, y, PLAYER_W, ph);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        float eye = (game->player.facing > 0) ? x + PLAYER_W - 8 : x + 4;
        fill_rect(r, eye, y + 6, 4, 4);
      }
    }
  }
  else
  {
    const Particle *s = &game->particles.particles[0];
    if (s->active && s->life > 0.0f)
    {
      SDL_SetRenderDrawColor(r, 90, 10, 10, 255);
      fill_rect(r, s->x - 6.0f, s->y + oy - 4.0f, 12.0f, 6.0f);
    }
  }
}

static void render_hud(Game *game)
{
  SDL_Renderer *r = game->renderer;

  int win_w = 0, win_h = 0;
  SDL_GetWindowSize(game->window, &win_w, &win_h);
  SDL_SetRenderDrawColor(r, 20, 20, 28, 255);
  fill_rect(r, 0, 0, (float)win_w, HUD_HEIGHT);

  char buf[128];
  if (game->player.grenades > 0)
  {
    SDL_snprintf(buf, sizeof(buf), "LIVES %d  AMMO %d  KEY: %s  LEVEL %d  SCORE %d  G",
                 game->lives,
                 game->player.bullets,
                 game->level.items_remaining == 0 ? "YES" : "NO",
                 game->current_level + 1,
                 game->score);
  }
  else
  {
    SDL_snprintf(buf, sizeof(buf), "LIVES %d  AMMO %d  KEY: %s  LEVEL %d  SCORE %d",
                 game->lives,
                 game->player.bullets,
                 game->level.items_remaining == 0 ? "YES" : "NO",
                 game->current_level + 1,
                 game->score);
  }
  draw_text(game->renderer, 8, 12, 2.0f, 240, 240, 240, buf);
}

void game_render(Game *game)
{
  SDL_Renderer *r = game->renderer;

  SDL_SetRenderDrawColor(r, 18, 18, 24, 255);
  SDL_RenderClear(r);

  render_world(game);
  render_hud(game);

  if (game->exit_unlocked_timer > 0.0f)
  {
    draw_text_centered(game, 56.0f, 3.0f, 90, 255, 200, "EXIT UNLOCKED");
    int win_w = 0, win_h = 0;
    game_get_view_size(game, &win_w, &win_h);
    float ex_screen = game->level.exit_col * (float)TILE_SIZE - game->cam_x;
    if (ex_screen + TILE_SIZE < 0.0f)
    {
      draw_text(game->renderer, 8.0f, 56.0f, 3.0f, 200, 200, 80, "<-");
    }
    else if (ex_screen > (float)win_w)
    {
      draw_text(game->renderer, (float)win_w - 28.0f, 56.0f, 3.0f, 200, 200, 80, "->");
    }
  }

  if (game->state == STATE_LEVEL_CLEARED)
  {
    draw_text_centered(game, 240.0f, 4.0f, 80, 230, 120, "LEVEL CLEARED");
  }
  else if (game->state == STATE_GAME_OVER)
  {
    draw_text_centered(game, 220.0f, 4.0f, 230, 70, 70, "GAME OVER");
    draw_text_centered(game, 280.0f, 2.0f, 230, 230, 230, "PRESS R TO RESTART");
  }
  else if (game->state == STATE_WIN)
  {
    draw_text_centered(game, 220.0f, 4.0f, 90, 230, 120, "YOU WIN!");
    draw_text_centered(game, 280.0f, 2.0f, 230, 230, 230, "PRESS R TO RESTART");
  }

  SDL_RenderPresent(r);
}
