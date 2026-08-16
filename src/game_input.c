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
  /* Everything downstream — which button jumps, which letter the title screen
   * asks for — is decided here, once, from what this pad says it is. */
  pad_hints_read(&game->platform.pad, game->platform.gamepad);
  const char *name = SDL_GetGamepadName(game->platform.gamepad);
  SDL_Log("Gamepad connected: %s (%s = confirm)", name != NULL ? name : "unknown",
          game->platform.pad.face[PAD_FACE_CONFIRM]);
}

void game_input_init(Game *game)
{
  pad_hints_read(&game->platform.pad, NULL);

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
    pad_hints_read(&game->platform.pad, NULL);
  }
}

const PadHints *game_pad_hints(const Game *game)
{
  return game->platform.gamepad_active ? &game->platform.pad : NULL;
}

static bool gamepad_button(const Game *game, SDL_GamepadButton button)
{
  return game->platform.gamepad != NULL &&
         SDL_GetGamepadButton(game->platform.gamepad, button);
}

/* The button carrying a letter on this pad, for the two inputs read every
 * frame rather than delivered as presses. */
static bool gamepad_face(const Game *game, PadFace face)
{
  return gamepad_button(game, pad_hints_button(&game->platform.pad, face));
}

void game_read_input(Game *game)
{
  const bool *ks = SDL_GetKeyboardState(NULL);
  bool key_left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
  bool key_right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
  bool key_up = ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W];
  bool key_down = ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S];
  bool key_interact = ks[SDL_SCANCODE_E];
  /* The dedicated jump key. UP is the keyboard's jump everywhere except over a
   * ladder, where the same key has to mean climb — which left the keyboard as
   * the one input that could not jump off a ladder at all, a move the pad has
   * had all along under A. */
  bool key_jump = ks[SDL_SCANCODE_LSHIFT];

  bool pad_left = false;
  bool pad_right = false;
  bool pad_up = false;
  bool pad_down = false;
  bool pad_interact = false;
  bool pad_jump_held = false;
  bool pad_brake = false;
  bool pad_gas_trigger = false;
  bool pad_brake_trigger = false;
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
    pad_interact = gamepad_face(game, PAD_FACE_DOOR);
    pad_jump_held = gamepad_face(game, PAD_FACE_CONFIRM);
    pad_brake = gamepad_face(game, PAD_FACE_CANCEL);
    /* The triggers are what a driver's fingers go to, so the drive answers
     * them as well as the letters it prompts for: RT accelerates, LT brakes.
     * They are analogue, but this car has one throttle position, so anything
     * past the dead zone counts as down. */
    pad_gas_trigger = SDL_GetGamepadAxis(game->platform.gamepad,
                                         SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >
                      GAMEPAD_AXIS_DEAD_ZONE;
    pad_brake_trigger = SDL_GetGamepadAxis(game->platform.gamepad,
                                           SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >
                        GAMEPAD_AXIS_DEAD_ZONE;
  }

  game->input.left = key_left || pad_left;
  game->input.right = key_right || pad_right;
  game->input.up = key_up || pad_up;
  game->input.down = key_down || pad_down;
  game->input.interact = key_interact || pad_interact;
  /* Held state feeds the variable jump height: release mid-rise cuts it. Both
   * keyboard jump keys are read, or a jump started on one of them would be cut
   * back to a hop on the very next frame. */
  game->input.jump_held = key_up || key_jump || pad_jump_held;
  /* The pedals of the prologue drive: A goes, B stops, the triggers do the
   * same for the fingers that expect a car to be driven with them, and the
   * stick, the d-pad and the arrows keep working for anyone who reaches for
   * those first. All of it is read every frame rather than delivered as
   * presses, because a throttle is held. */
  game->input.gas = game->input.up || pad_jump_held || pad_gas_trigger;
  game->input.brake = game->input.down || pad_brake || pad_brake_trigger;

  if (key_left || key_right || key_up || key_down || key_interact || key_jump)
    game->platform.gamepad_active = false;
  else if (pad_left || pad_right || pad_up || pad_down || pad_interact ||
           pad_jump_held || pad_brake || pad_gas_trigger || pad_brake_trigger)
    game->platform.gamepad_active = true;
}

static bool state_accepts_confirm(GameState state)
{
  return state == STATE_INTRO ||
         state == STATE_ABDUCTION ||
         state == STATE_CHASE ||
         state == STATE_OPENING_CUTSCENE ||
         state == STATE_LEVEL_START ||
         state == STATE_SHOW_KEYCARD ||
         state == STATE_LEVEL_TRANSITION ||
         state == STATE_OUTRO ||
         state == STATE_CONTINUE ||
         state == STATE_GAME_OVER ||
         state == STATE_PAUSED;
}

/* The states ESC (or START) pauses rather than aborts. */
static bool state_accepts_pause(GameState state)
{
  return state == STATE_PLAYING ||
         state == STATE_LEVEL_START ||
         state == STATE_SHOW_KEYCARD ||
         state == STATE_CHASE;
}

/* F, from anywhere. It goes through the same function the options sheet's own
 * row does, so the window and the saved setting can never disagree about which
 * one the player asked for. */
static void toggle_fullscreen(Game *game)
{
  game_set_fullscreen(game, !game->platform.fullscreen);
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
 * The manual's own bindings, on the pad. The sheaf is walked with left and
 * right — on the d-pad and on the **bumpers**, which is what a bumper is for:
 * paging through a list is the one job every platform's own guidance gives
 * them. Everything that means "done" puts the sheet away, B included, which is
 * the whole reason a player reaches for B; A, Y and START are here because
 * whichever button opened the manual is the one the thumb will press again.
 */
static bool handle_manual_gamepad(Game *game, SDL_GamepadButton button,
                                  PadFace face)
{
  if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT ||
      button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
  {
    turn_manual_page(game, -1);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ||
      button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
  {
    turn_manual_page(game, 1);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_START ||
      button == SDL_GAMEPAD_BUTTON_BACK || face == PAD_FACE_CONFIRM ||
      face == PAD_FACE_CANCEL || face == PAD_FACE_DOOR)
  {
    game_return_to_intro(game);
    return true;
  }
  return false;
}

/*
 * The options sheet's pad bindings: up and down walk the rows, left and right
 * change the one under the cursor, and everything that means "done" puts the
 * sheet away.
 *
 * A is a change input as well as the two directions, and that is deliberate
 * rather than a shortcut: a two-state row has no "more" and no "less", so on a
 * switch all three do the same thing, and on a level A steps it up. What
 * matters is that the footer names exactly these and nothing else — a sheet
 * that answers a button it never mentions is the same bug as a prompt naming a
 * button that does nothing.
 *
 * It closes through game_close_settings and never through the route out below,
 * because the sheet opens from the pause screen as well and has to hand that
 * run back rather than drop it on the title screen.
 */
static bool handle_settings_gamepad(Game *game, SDL_GamepadButton button,
                                    PadFace face)
{
  if (button == SDL_GAMEPAD_BUTTON_DPAD_UP)
  {
    game_settings_move_cursor(game, -1);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
  {
    game_settings_move_cursor(game, 1);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT)
  {
    game_settings_adjust(game, -1);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT || face == PAD_FACE_CONFIRM)
  {
    game_settings_adjust(game, 1);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_START ||
      button == SDL_GAMEPAD_BUTTON_BACK || face == PAD_FACE_CANCEL ||
      face == PAD_FACE_ATTACK || face == PAD_FACE_DOOR)
  {
    game_close_settings(game);
    return true;
  }
  return false;
}

/* The pause menu's own bindings. Up and down walk the three items and A
 * answers the one under the cursor; B and START still resume outright, because
 * the button that opened the sheet and the button that backs out of one are
 * both faster than walking to RESUME and pressing it. */
static bool handle_pause_gamepad(Game *game, SDL_GamepadButton button)
{
  if (button == SDL_GAMEPAD_BUTTON_DPAD_UP)
  {
    game_pause_move_cursor(game, -1);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
  {
    game_pause_move_cursor(game, 1);
    return true;
  }
  return false;
}

static void confirm_with_gamepad(Game *game, bool allow_jump)
{
  /* During the drive A is the accelerator and nothing else: a letter that is
   * being held down to move cannot also be the button that skips the scene it
   * is moving through. The drive is skipped with Y instead. */
  if (game->state == STATE_CHASE)
    return;

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

/*
 * Backing out of whatever is on screen.
 *
 * B is the back button on every pad ever made, and back is all it does here:
 * it closes what was opened, and where there is nothing open it does nothing
 * at all. Three things it used to do and must never do again, because every
 * one of them cost a player something they did not ask to spend:
 *
 * - **It does not pause.** Pause is START's job. B carrying it as well meant a
 *   stray thumb froze a sector, and once the drive put the brake pedal on B it
 *   meant braking opened the pause sheet.
 * - **It does not abandon a run.** During a sector, a cutscene, a transition
 *   or the drive it does nothing at all. Dropping a run on one press of the
 *   button players use to say "not that" is the same bug wearing a hat; the
 *   way out of a run is deliberate, from the pause screen, and `abandons_run`
 *   (SELECT) is only honoured there.
 * - **It does not close the game.** Quitting is not "backing out" of anything
 *   the player opened — they opened the game — and a first press of B on the
 *   first screen ending the session is the worst version of that mistake.
 *   ESC and the window's own close box are both asked for on purpose.
 */
static void back_out_with_gamepad(Game *game, bool abandons_run)
{
  if (game->state == STATE_PAUSED)
  {
    if (abandons_run)
      game_return_to_intro(game);
    else
      game_toggle_pause(game);
    return;
  }
  /* Everywhere else — the title screen included — there is nothing open to
   * close, so B is deliberately inert here. See the third rule above. */
}

static void handle_gamepad_button(Game *game, SDL_GamepadButton button)
{
  game->platform.gamepad_active = true;

  /* The letter this pad prints on the button that was pressed. Everything the
   * player is told to press is a letter, so everything they press is read as
   * one; the buttons that carry no letter are handled as themselves below. */
  PadFace face = pad_hints_face(&game->platform.pad, button);

  if (game->state == STATE_MANUAL && handle_manual_gamepad(game, button, face))
    return;
  if (game->state == STATE_SETTINGS &&
      handle_settings_gamepad(game, button, face))
    return;
  if (game->state == STATE_PAUSED && handle_pause_gamepad(game, button))
    return;

  switch (face)
  {
  case PAD_FACE_CONFIRM:
    confirm_with_gamepad(game, true);
    return;
  case PAD_FACE_CANCEL:
    /* In a sector B is the second trigger finger. A and B are the two buttons
     * every thumb finds first, so leaving B inert while the game is being
     * played reads as a dead button on the pad; there is nothing to back out
     * of mid-sector anyway. Attack sits on both B and X, and B keeps meaning
     * "back" everywhere something is actually open. */
    if (game->state == STATE_PLAYING)
      game->input.shoot = true;
    else
      back_out_with_gamepad(game, false);
    return;
  case PAD_FACE_ATTACK:
    if (game->state == STATE_PLAYING)
      game->input.shoot = true;
    else
      /* The sheet opens from the title screen and from pause and returns to
       * whichever opened it; game_open_settings ignores every other state. */
      game_open_settings(game);
    return;
  case PAD_FACE_DOOR:
    /* The drive reads use_door as its skip: A and B are the pedals there, so
     * the way past the prologue moved to the one letter still free. */
    if (game->state == STATE_PLAYING || game->state == STATE_CHASE)
      game->input.use_door = true;
    else if (game->state == STATE_INTRO)
      game_open_manual(game);
    /* Y used to mute from the pause screen, which was the only sound control a
     * pad had. It is gone because the options sheet is two presses away and
     * carries two real levels: a pad muting to silence while the sheet beside
     * it still read 100 was two answers to the same question. */
    return;
  case PAD_FACE_NONE:
  case PAD_FACE_COUNT:
    break;
  }

  switch (button)
  {
  case SDL_GAMEPAD_BUTTON_START:
    /* START is the pause button while anything is running; everywhere else
     * it keeps meaning "confirm". */
    if (state_accepts_pause(game->state) || game->state == STATE_PAUSED)
      game_toggle_pause(game);
    else
      confirm_with_gamepad(game, false);
    break;
  case SDL_GAMEPAD_BUTTON_BACK:
    back_out_with_gamepad(game, true);
    break;
  /* The bumpers cycle, which is the one job every platform's own guidance
   * gives them: RB takes the next weapon, LB the one before it. */
  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
    if (game->state == STATE_PLAYING)
      game->input.switch_weapon_back = true;
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
    else if (intro_hit_options_button(&game->presentation.intro, mx, my))
    {
      game_open_settings(game);
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

    /* The options sheet owns the keyboard while it is open, like the manual.
     * Up and down walk the rows and left and right change the one under the
     * cursor; ENTER is a change input too, so a switch can be flipped without
     * the hand leaving the row it is on. */
    if (game->state == STATE_SETTINGS)
    {
      if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
        game_settings_move_cursor(game, -1);
      else if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
        game_settings_move_cursor(game, 1);
      else if (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_A)
        game_settings_adjust(game, -1);
      else if (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_D ||
               key == SDLK_SPACE || key == SDLK_RETURN ||
               key == SDLK_KP_ENTER)
        game_settings_adjust(game, 1);
      else if (sc == SDL_SCANCODE_J || sc == SDL_SCANCODE_H ||
               sc == SDL_SCANCODE_BACKSPACE)
        game_close_settings(game);
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

    if (sc == SDL_SCANCODE_J &&
        (game->state == STATE_INTRO || game->state == STATE_PAUSED))
    {
      game_open_settings(game);
      return;
    }

    /* The pause menu is walked, not just read. Its three items are answered by
     * the confirm below, which every state on that list already reports. */
    if (game->state == STATE_PAUSED &&
        (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W))
    {
      game_pause_move_cursor(game, -1);
      return;
    }
    if (game->state == STATE_PAUSED &&
        (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S))
    {
      game_pause_move_cursor(game, 1);
      return;
    }

    if (game->state == STATE_PAUSED && sc == SDL_SCANCODE_Q)
    {
      game_return_to_intro(game);
      return;
    }

    /* BACKSPACE backs out of whatever is open, and the pause sheet is one of
     * the three things that can be. The manual and the assist sheet already
     * answered it and the control table promises all three, so the pause sheet
     * ignoring it was the keyboard disagreeing with its own manual — and with
     * the pad, where B closes all three. */
    if (game->state == STATE_PAUSED && sc == SDL_SCANCODE_BACKSPACE)
    {
      game_toggle_pause(game);
      return;
    }

    if (state_accepts_confirm(game->state) && game->state != STATE_PLAYING &&
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
    /* The ring is walked both ways on the pad, so it is walked both ways here
     * too: a keyboard that could only ever go forward is a keyboard three
     * presses from the weapon a bumper reaches in one. */
    if (sc == SDL_SCANCODE_Z && game->state == STATE_PLAYING)
    {
      game->input.switch_weapon_back = true;
    }
    /* A jump key that is not also the climb key, so it needs no ladder test:
     * it reports the press and player_update decides whether to honour it,
     * now or from the buffer — exactly as the pad's A does. */
    if (sc == SDL_SCANCODE_LSHIFT && game->state == STATE_PLAYING)
    {
      game->input.jump = true;
    }
    /* Jump on Up arrow. Over a ladder the same key means "climb", so the
     * press is not reported as a jump there; everywhere else it always is —
     * whether it lands now, in the coyote window, or from the jump buffer is
     * player_update's decision, not the input layer's.
     *
     * Only in a sector, like LSHIFT above: the ladder test below reads the
     * player out of the live simulation, and outside a sector that simulation
     * is whatever the last one left behind. */
    if ((key == SDLK_UP || event->key.scancode == SDL_SCANCODE_W) &&
        game->state == STATE_PLAYING)
    {
      /* Determine whether player box overlaps a ladder near center/feet */
      int col = (int)floorf((game->gameplay.player.x + PLAYER_W * 0.5f) / TILE_SIZE);
      float ph = game->gameplay.player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
      int row_center = (int)floorf((game->gameplay.player.y + ph * 0.5f) / TILE_SIZE);
      int row_feet = (int)floorf((game->gameplay.player.y + ph - 1.0f) / TILE_SIZE);
      bool over_ladder = level_is_ladder(&game->gameplay.level, col, row_center) ||
                         level_is_ladder(&game->gameplay.level, col, row_feet);
      if (!over_ladder && !game->gameplay.player.on_ladder)
      {
        game->input.jump = true;
      }
    }
    /* The door key belongs to the sector and is only reported there. The drive
     * also reads `use_door`, as the skip the pad puts on Y — so an ungated E
     * was a second, unadvertised way past the prologue, on the one input that
     * is never told about it: the drive prompts the keyboard for ENTER/SPACE,
     * and a button that works without ever being named is the mirror image of
     * a prompt naming a button that does not. */
    if (sc == SDL_SCANCODE_E && game->state == STATE_PLAYING)
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
