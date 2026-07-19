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
  game->input.interact = ks[SDL_SCANCODE_E];
}

void game_handle_event(Game *game, const SDL_Event *event)
{
  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
      event->button.button == SDL_BUTTON_LEFT &&
      game->state == STATE_INTRO)
  {
    float mx = 0.0f, my = 0.0f;
    SDL_RenderCoordinatesFromWindow(game->renderer, event->button.x, event->button.y, &mx, &my);
    if (intro_hit_start_button(&game->intro, mx, my))
    {
      game->input.confirm = true;
    }
    return;
  }

  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
  {
    SDL_Keycode key = event->key.key;
    SDL_Scancode sc = event->key.scancode;

    /* Handle fullscreen before title-screen confirmation so Alt+Enter does
     * not accidentally start the game. */
    if (sc == SDL_SCANCODE_F ||
        (sc == SDL_SCANCODE_RETURN && (event->key.mod & SDL_KMOD_ALT) != 0))
    {
      bool target = !game->fullscreen;
      if (SDL_SetWindowFullscreen(game->window, target))
      {
        game->fullscreen = target;
      }
      else
      {
        SDL_Log("Could not toggle fullscreen: %s", SDL_GetError());
      }
      return;
    }

    if ((game->state == STATE_INTRO ||
         game->state == STATE_OPENING_CUTSCENE ||
         game->state == STATE_LEVEL_TRANSITION) &&
        (key == SDLK_SPACE || key == SDLK_RETURN || key == SDLK_KP_ENTER))
    {
      game->input.confirm = true;
      return;
    }
    if (sc == SDL_SCANCODE_M)
    {
      audio_toggle_mute(&game->audio);
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
