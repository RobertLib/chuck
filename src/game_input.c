#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>

void game_read_input(Game *game)
{
  const bool *ks = SDL_GetKeyboardState(NULL);
  game->input.left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
  game->input.right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
  game->input.up = ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W];
  game->input.down = ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S];
}

void game_handle_event(Game *game, const SDL_Event *event)
{
  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
  {
    SDL_Keycode key = event->key.key;
    /* Toggle fullscreen on 'f' or Alt+Enter. Use scancode/modifier via
     * keyboard state to avoid dependency on deprecated keycode macros. */
    SDL_Scancode sc = event->key.scancode;
    const bool *kstate = SDL_GetKeyboardState(NULL);
    if (sc == SDL_SCANCODE_F ||
        (sc == SDL_SCANCODE_RETURN && (kstate[SDL_SCANCODE_LALT] || kstate[SDL_SCANCODE_RALT])))
    {
      if (!game->fullscreen)
      {
        SDL_SetWindowFullscreen(game->window, true);
        game->fullscreen = true;
      }
      else
      {
        SDL_SetWindowFullscreen(game->window, false);
        game->fullscreen = false;
      }
      return;
    }
    /* Shoot on Space only */
    if (key == SDLK_SPACE)
    {
      game->input.shoot = true;
    }
    /* Jump on Up arrow, but avoid interfering with ladders: only trigger
     * jump edge when player is on the ground and not overlapping a ladder.
     */
    if (key == SDLK_UP || event->key.scancode == SDL_SCANCODE_W)
    {
      /* Determine whether player box overlaps a ladder near center/feet */
      int col = (int)floorf((game->player.x + PLAYER_W * 0.5f) / TILE_SIZE);
      float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
      int row_center = (int)floorf((game->player.y + ph * 0.5f) / TILE_SIZE);
      int row_feet = (int)floorf((game->player.y + ph - 1.0f) / TILE_SIZE);
      bool over_ladder = level_is_ladder(&game->level, col, row_center) ||
                         level_is_ladder(&game->level, col, row_feet);
      if (!over_ladder && !game->player.on_ladder && game->player.on_ground)
      {
        game->input.jump = true;
      }
    }
    if (key == SDLK_DOWN || key == SDLK_S)
    {
      game->input.use_door = true;
    }
    if (key == SDLK_R &&
        (game->state == STATE_GAME_OVER || game->state == STATE_WIN))
    {
      /* Restart logic lives in game.c; set a flag by reusing input.jump
       * edge so caller can respond. Simpler: call restart directly is
       * intrusive here so we keep input semantics. */
      game->input.jump = true; /* consumer in game_update must handle this case */
    }
  }
}
