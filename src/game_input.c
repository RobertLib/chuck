#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>

#ifdef CHUCK_DEBUG
#include "embedded_levels.h"
#endif

static void open_gamepad(Game *game, SDL_JoystickID id)
{
  if (game->platform.gamepad != NULL)
    return;

  game->platform.gamepad = SDL_OpenGamepad(id);
  if (game->platform.gamepad == NULL)
  {
    SDL_Log("Could not open gamepad: %s", SDL_GetError());
    return;
  }

  game->platform.gamepad_id = id;
  game->platform.gamepad_active = true;
  const char *name = SDL_GetGamepadName(game->platform.gamepad);
  SDL_Log("Gamepad connected: %s", name != NULL ? name : "unknown");
}

void game_input_init(Game *game)
{
  int count = 0;
  SDL_JoystickID *ids = SDL_GetGamepads(&count);
  for (int i = 0; i < count && game->platform.gamepad == NULL; ++i)
    open_gamepad(game, ids[i]);
  SDL_free(ids);
}

void game_input_shutdown(Game *game)
{
  if (game->platform.gamepad != NULL)
  {
    SDL_CloseGamepad(game->platform.gamepad);
    game->platform.gamepad = NULL;
    game->platform.gamepad_id = 0;
  }
}

static bool gamepad_button(const Game *game, SDL_GamepadButton button)
{
  return game->platform.gamepad != NULL &&
         SDL_GetGamepadButton(game->platform.gamepad, button);
}

void game_read_input(Game *game)
{
  const bool *ks = SDL_GetKeyboardState(NULL);
  bool key_left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
  bool key_right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
  bool key_up = ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W];
  bool key_down = ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S];
  bool key_interact = ks[SDL_SCANCODE_E];

  bool pad_left = false;
  bool pad_right = false;
  bool pad_up = false;
  bool pad_down = false;
  bool pad_interact = false;
  if (game->platform.gamepad != NULL)
  {
    Sint16 x = SDL_GetGamepadAxis(game->platform.gamepad,
                                  SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 y = SDL_GetGamepadAxis(game->platform.gamepad,
                                  SDL_GAMEPAD_AXIS_LEFTY);
    pad_left = x < -GAMEPAD_AXIS_DEAD_ZONE ||
               gamepad_button(game, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    pad_right = x > GAMEPAD_AXIS_DEAD_ZONE ||
                gamepad_button(game, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    pad_up = y < -GAMEPAD_AXIS_DEAD_ZONE ||
             gamepad_button(game, SDL_GAMEPAD_BUTTON_DPAD_UP);
    pad_down = y > GAMEPAD_AXIS_DEAD_ZONE ||
               gamepad_button(game, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    pad_interact = gamepad_button(game, SDL_GAMEPAD_BUTTON_NORTH);
  }

  game->input.left = key_left || pad_left;
  game->input.right = key_right || pad_right;
  game->input.up = key_up || pad_up;
  game->input.down = key_down || pad_down;
  game->input.interact = key_interact || pad_interact;

  if (key_left || key_right || key_up || key_down || key_interact)
    game->platform.gamepad_active = false;
  else if (pad_left || pad_right || pad_up || pad_down || pad_interact)
    game->platform.gamepad_active = true;
}

static bool state_accepts_confirm(GameState state)
{
  return state == STATE_INTRO ||
         state == STATE_CHASE ||
         state == STATE_OPENING_CUTSCENE ||
         state == STATE_LEVEL_TRANSITION ||
         state == STATE_OUTRO ||
         state == STATE_CONTINUE;
}

static void toggle_fullscreen(Game *game)
{
  bool target = !game->platform.fullscreen;
  if (SDL_SetWindowFullscreen(game->platform.window, target))
    game->platform.fullscreen = target;
  else
    SDL_Log("Could not toggle fullscreen: %s", SDL_GetError());
}

#ifdef CHUCK_DEBUG
static bool handle_debug_level_select(Game *game, SDL_Scancode scancode)
{
  if (game->state != STATE_INTRO || EMBEDDED_LEVEL_COUNT == 0)
    return false;

  int level_count = (int)EMBEDDED_LEVEL_COUNT;
  if (scancode == SDL_SCANCODE_LEFT ||
      scancode == SDL_SCANCODE_LEFTBRACKET)
  {
    game->debug_selected_level =
        (game->debug_selected_level + level_count - 1) % level_count;
    return true;
  }
  if (scancode == SDL_SCANCODE_RIGHT ||
      scancode == SDL_SCANCODE_RIGHTBRACKET)
  {
    game->debug_selected_level =
        (game->debug_selected_level + 1) % level_count;
    return true;
  }
  if (scancode == SDL_SCANCODE_F5)
  {
    game_start_at_level(game, game->debug_selected_level);
    return true;
  }
  return false;
}
#endif

/* Turn a sheet and sound it. The sound only plays when the page actually
 * changed, so holding against either end of the manual stays silent. */
static void turn_manual_page(Game *game, int delta)
{
  if (manual_turn_page(&game->presentation.manual, delta))
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
}

/*
 * The manual's own bindings, on the pad. Left and right walk the sheaf; A,
 * START and Y all put it away, because whichever button opened it is the one
 * the player will press again. B and BACK fall through to the ordinary route
 * out, which is the same route.
 */
static bool handle_manual_gamepad(Game *game, SDL_GamepadButton button)
{
  switch (button)
  {
  case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
    turn_manual_page(game, -1);
    return true;
  case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
    turn_manual_page(game, 1);
    return true;
  case SDL_GAMEPAD_BUTTON_SOUTH:
  case SDL_GAMEPAD_BUTTON_START:
  case SDL_GAMEPAD_BUTTON_NORTH:
    game_return_to_intro(game);
    return true;
  default:
    return false;
  }
}

static void confirm_with_gamepad(Game *game, bool allow_jump)
{
  if (game->state == STATE_OUTRO &&
      game->presentation.outro_cutscene.time >= OUTRO_FINAL_REVEAL_TIME)
  {
    game->input.restart = true;
  }
  else if (state_accepts_confirm(game->state))
  {
    game->input.confirm = true;
  }
  else if (allow_jump && game->state == STATE_PLAYING)
  {
    game->input.jump = true;
  }
}

static void handle_gamepad_button(Game *game, SDL_GamepadButton button)
{
  game->platform.gamepad_active = true;
  if (game->state == STATE_MANUAL && handle_manual_gamepad(game, button))
    return;
  switch (button)
  {
  case SDL_GAMEPAD_BUTTON_SOUTH:
    confirm_with_gamepad(game, true);
    break;
  case SDL_GAMEPAD_BUTTON_START:
    confirm_with_gamepad(game, false);
    break;
  case SDL_GAMEPAD_BUTTON_EAST:
  case SDL_GAMEPAD_BUTTON_BACK:
    if (game->state == STATE_INTRO)
      game->platform.quit_requested = true;
    else
      game_return_to_intro(game);
    break;
  case SDL_GAMEPAD_BUTTON_WEST:
    if (game->state == STATE_PLAYING)
      game->input.shoot = true;
    break;
  case SDL_GAMEPAD_BUTTON_NORTH:
    if (game->state == STATE_PLAYING)
      game->input.use_door = true;
    else if (game->state == STATE_INTRO)
      game_open_manual(game);
    break;
  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
    audio_toggle_mute(&game->platform.audio);
    break;
  case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
    if (game->state == STATE_PLAYING)
      game->input.switch_weapon = true;
    break;
  default:
    break;
  }
}

void game_handle_event(Game *game, const SDL_Event *event)
{
  if (event->type == SDL_EVENT_GAMEPAD_ADDED)
  {
    open_gamepad(game, event->gdevice.which);
    return;
  }

  if (event->type == SDL_EVENT_GAMEPAD_REMOVED &&
      game->platform.gamepad != NULL &&
      game->platform.gamepad_id == event->gdevice.which)
  {
    SDL_Log("Gamepad disconnected");
    game_input_shutdown(game);
    game->platform.gamepad_active = false;
    game_input_init(game);
    return;
  }

  if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION &&
      game->platform.gamepad != NULL &&
      game->platform.gamepad_id == event->gaxis.which &&
      (event->gaxis.value < -GAMEPAD_AXIS_DEAD_ZONE ||
       event->gaxis.value > GAMEPAD_AXIS_DEAD_ZONE))
  {
    game->platform.gamepad_active = true;
    return;
  }

  if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
      game->platform.gamepad != NULL &&
      game->platform.gamepad_id == event->gbutton.which)
  {
    handle_gamepad_button(game, (SDL_GamepadButton)event->gbutton.button);
    return;
  }

  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
      event->button.button == SDL_BUTTON_LEFT &&
      (game->state == STATE_INTRO || game->state == STATE_MANUAL))
  {
    game->platform.gamepad_active = false;
    float mx = 0.0f, my = 0.0f;
    SDL_RenderCoordinatesFromWindow(game->platform.renderer, event->button.x, event->button.y, &mx, &my);
    if (game->state == STATE_MANUAL)
    {
      switch (manual_hit_test(&game->presentation.manual, mx, my))
      {
      case MANUAL_HOT_PREV:
        turn_manual_page(game, -1);
        break;
      case MANUAL_HOT_NEXT:
        turn_manual_page(game, 1);
        break;
      case MANUAL_HOT_BACK:
        game_return_to_intro(game);
        break;
      case MANUAL_HOT_NONE:
        break;
      }
    }
    else if (intro_hit_start_button(&game->presentation.intro, mx, my))
    {
      game->input.confirm = true;
    }
    else if (intro_hit_manual_button(&game->presentation.intro, mx, my))
    {
      game_open_manual(game);
    }
    return;
  }

  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
  {
    game->platform.gamepad_active = false;
    SDL_Keycode key = event->key.key;
    SDL_Scancode sc = event->key.scancode;

#ifdef CHUCK_DEBUG
    if (handle_debug_level_select(game, sc))
      return;
#endif

    /* Handle fullscreen before title-screen confirmation so Alt+Enter does
     * not accidentally start the game. */
    if (sc == SDL_SCANCODE_F ||
        (sc == SDL_SCANCODE_RETURN && (event->key.mod & SDL_KMOD_ALT) != 0))
    {
      toggle_fullscreen(game);
      return;
    }

    /* The manual owns the whole keyboard while it is open: left and right walk
     * the sheaf, and every key that means "done" puts it away. */
    if (game->state == STATE_MANUAL)
    {
      if (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_A ||
          sc == SDL_SCANCODE_PAGEUP)
        turn_manual_page(game, -1);
      else if (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_D ||
               sc == SDL_SCANCODE_PAGEDOWN)
        turn_manual_page(game, 1);
      else if (key == SDLK_RETURN || key == SDLK_KP_ENTER ||
               key == SDLK_SPACE || sc == SDL_SCANCODE_H ||
               sc == SDL_SCANCODE_F1 || sc == SDL_SCANCODE_BACKSPACE)
        game_return_to_intro(game);
      else if (sc == SDL_SCANCODE_M)
        audio_toggle_mute(&game->platform.audio);
      return;
    }

    if (game->state == STATE_INTRO &&
        (sc == SDL_SCANCODE_H || sc == SDL_SCANCODE_F1))
    {
      game_open_manual(game);
      return;
    }

    if ((game->state == STATE_INTRO ||
         game->state == STATE_CHASE ||
         game->state == STATE_OPENING_CUTSCENE ||
         game->state == STATE_LEVEL_TRANSITION ||
         game->state == STATE_OUTRO ||
         game->state == STATE_CONTINUE) &&
        (key == SDLK_SPACE || key == SDLK_RETURN || key == SDLK_KP_ENTER))
    {
      game->input.confirm = true;
      return;
    }
    if (sc == SDL_SCANCODE_M)
    {
      audio_toggle_mute(&game->platform.audio);
      return;
    }
    /* Shoot on Space only */
    if (key == SDLK_SPACE)
    {
      game->input.shoot = true;
    }
    if ((sc == SDL_SCANCODE_Q || sc == SDL_SCANCODE_TAB) &&
        game->state == STATE_PLAYING)
    {
      game->input.switch_weapon = true;
    }
    /* Jump on Up arrow, but avoid interfering with ladders: only trigger
     * jump edge when player is on the ground and not overlapping a ladder.
     */
    if (key == SDLK_UP || event->key.scancode == SDL_SCANCODE_W)
    {
      /* Determine whether player box overlaps a ladder near center/feet */
      int col = (int)floorf((game->gameplay.player.x + PLAYER_W * 0.5f) / TILE_SIZE);
      float ph = game->gameplay.player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
      int row_center = (int)floorf((game->gameplay.player.y + ph * 0.5f) / TILE_SIZE);
      int row_feet = (int)floorf((game->gameplay.player.y + ph - 1.0f) / TILE_SIZE);
      bool over_ladder = level_is_ladder(&game->gameplay.level, col, row_center) ||
                         level_is_ladder(&game->gameplay.level, col, row_feet);
      if (!over_ladder && !game->gameplay.player.on_ladder && game->gameplay.player.on_ground)
      {
        game->input.jump = true;
      }
    }
    if (sc == SDL_SCANCODE_E)
    {
      game->input.use_door = true;
    }
    if (key == SDLK_R && game->state == STATE_OUTRO &&
        game->presentation.outro_cutscene.time >= OUTRO_FINAL_REVEAL_TIME)
    {
      game->input.restart = true;
    }
  }
}
