#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>

#ifdef CHUCK_DEBUG
#include "embedded_levels.h"
#endif

/*
 * The numbers in [keybind.h](keybind.h) are SDL scancodes, and that file links
 * no SDL — so this is where the two are made to agree.
 *
 * One assertion per row, generated from the same list the table is, so the
 * check cannot fall behind the thing it is checking: adding a key to
 * `CHUCK_KEY_LIST` adds its assertion with it, and a scancode that is ever
 * wrong is a build failure rather than a key that silently does nothing. It is
 * the same reason `packaging/fetch_sdl3.sh`'s pin is read by CI rather than
 * copied into it — two hand-kept copies of a number are two numbers.
 */
#define CHUCK_KEY_ASSERT(ident, code, name)                                   \
    _Static_assert((code) == SDL_SCANCODE_##ident,                            \
                   "keybind.h disagrees with SDL about " name);
CHUCK_KEY_LIST(CHUCK_KEY_ASSERT)
#undef CHUCK_KEY_ASSERT

/* The pad's numbers are a copy of somebody else's constants for exactly the
 * same reason and are checked exactly the same way. */
#define CHUCK_PAD_ASSERT(ident, button, file_name, shown)                     \
    _Static_assert((button) == SDL_GAMEPAD_BUTTON_##ident,                    \
                   "keybind.h disagrees with SDL about " file_name);
CHUCK_PAD_LIST(CHUCK_PAD_ASSERT)
#undef CHUCK_PAD_ASSERT
_Static_assert(PADBIND_NONE == SDL_GAMEPAD_BUTTON_INVALID,
               "keybind.h disagrees with SDL about the absent button");

/*
 * And [pad_hint.h](pad_hint.h)'s two tables, for the third time and the same
 * reason.
 *
 * That file used to link SDL and use these enumerators directly, which needed
 * no assertion and cost something worse: it put the four functions that decide
 * which letter goes on which button on the side of the SDL boundary no test can
 * reach, and `make coverage` duly reported them as never executed. They are
 * plain numbers over there now, so they are checked here — where every other
 * copy of somebody else's constants in this tree is checked.
 */
#define CHUCK_PAD_LABEL_ASSERT(ident, value)                                  \
    _Static_assert((value) == SDL_GAMEPAD_BUTTON_LABEL_##ident,               \
                   "pad_hint.h disagrees with SDL about label " #ident);
CHUCK_PAD_LABEL_LIST(CHUCK_PAD_LABEL_ASSERT)
#undef CHUCK_PAD_LABEL_ASSERT

#define CHUCK_PAD_TYPE_ASSERT(ident, value)                                   \
    _Static_assert((value) == SDL_GAMEPAD_TYPE_##ident,                       \
                   "pad_hint.h disagrees with SDL about pad type " #ident);
CHUCK_PAD_TYPE_LIST(CHUCK_PAD_TYPE_ASSERT)
#undef CHUCK_PAD_TYPE_ASSERT

/*
 * Ask an open pad the two questions `pad_hints_apply` needs answered, and let
 * it decide the rest.
 *
 * This is the whole of what needed a gamepad, and it is why the decision is no
 * longer in here with it: a label per face position and one type number is all
 * that crosses the boundary. A NULL pad asks nothing and gets the Xbox set,
 * which is what an unplugged pad and a fresh launch both want.
 */
static void read_pad_hints(PadHints *hints, SDL_Gamepad *gamepad)
{
  if (gamepad == NULL)
  {
    pad_hints_apply(hints, PAD_TYPE_UNKNOWN, NULL, 0);
    return;
  }

  PadButtonLabel labels[PAD_FACE_COUNT];
  for (int i = 0; i < PAD_FACE_COUNT; ++i)
    labels[i] = (PadButtonLabel)SDL_GetGamepadButtonLabel(
        gamepad, (SDL_GamepadButton)PAD_FACE_POSITIONS[i]);

  pad_hints_apply(hints, (PadType)SDL_GetGamepadType(gamepad), labels,
                  PAD_FACE_COUNT);
}

/* Whether a bound key is down, for the controls read every frame rather than
 * delivered as presses. */
static bool key_bound_down(const Game *game, const bool *ks, BindAction action)
{
  for (int slot = 0; slot < BIND_SLOTS; ++slot)
  {
    int code = game->settings.bindings.keys[action][slot];
    if (code != KEYBIND_NONE && ks[code])
      return true;
  }
  return false;
}

/* And whether a press is one of the keys an action answers to. */
static bool key_press_is(const Game *game, SDL_Scancode sc, BindAction action)
{
  return keybind_action_has(&game->settings.bindings, action, (int)sc);
}

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
  /* Centred until it says otherwise, so the first push off centre is an edge
   * rather than a value that happens to differ from a zeroed struct. */
  game->platform.pad_menu_direction = SDL_GAMEPAD_BUTTON_INVALID;
  /* Everything downstream — which button jumps, which letter the title screen
   * asks for — is decided here, once, from what this pad says it is. */
  read_pad_hints(&game->platform.pad, game->platform.gamepad);
  const char *name = SDL_GetGamepadName(game->platform.gamepad);
  SDL_Log("Gamepad connected: %s (%s = confirm)", name != NULL ? name : "unknown",
          game->platform.pad.face[PAD_FACE_CONFIRM]);
}

void game_input_init(Game *game)
{
  read_pad_hints(&game->platform.pad, NULL);

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
    read_pad_hints(&game->platform.pad, NULL);
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

/*
 * A stored binding, turned into the button on the pad that is actually in the
 * player's hands.
 *
 * The four faces are kept as letters and everything else as itself — see the
 * note on `keybind_pad_face_index`. This is the one place that translation
 * happens, in both directions: `pad_bound_down` reads through it and
 * `pad_capture_button` writes through its inverse, so a Switch player's chosen
 * button and an Xbox player's chosen button are the same entry in the file.
 */
static SDL_GamepadButton pad_resolve(const Game *game, int stored)
{
  if (stored == PADBIND_NONE)
    return SDL_GAMEPAD_BUTTON_INVALID;
  int face = keybind_pad_face_index(stored);
  if (face < 0)
    return (SDL_GamepadButton)stored;
  return pad_hints_button(&game->platform.pad, (PadFace)face);
}

/* And the way back, for a capture: the physical button the player pressed,
 * said as the thing the file will keep. */
static int pad_capture_button(const Game *game, SDL_GamepadButton pressed)
{
  PadFace face = pad_hints_face(&game->platform.pad, pressed);
  if (face != PAD_FACE_NONE)
    return keybind_pad_face_button((int)face);
  return (int)pressed;
}

/* Whether a bound pad button is down, for the controls read every frame. */
static bool pad_bound_down(const Game *game, BindAction action)
{
  if (game->platform.gamepad == NULL)
    return false;
  for (int slot = 0; slot < BIND_SLOTS; ++slot)
  {
    SDL_GamepadButton at =
        pad_resolve(game, game->settings.bindings.pad[action][slot]);
    if (at != SDL_GAMEPAD_BUTTON_INVALID && gamepad_button(game, at))
      return true;
  }
  return false;
}

/* And whether a press is one of the buttons an action answers to. */
static bool pad_press_is(const Game *game, SDL_GamepadButton pressed,
                         BindAction action)
{
  return keybind_action_has_pad(&game->settings.bindings, action,
                                pad_capture_button(game, pressed));
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
  bool key_left = key_bound_down(game, ks, BIND_LEFT);
  bool key_right = key_bound_down(game, ks, BIND_RIGHT);
  bool key_up = key_bound_down(game, ks, BIND_UP);
  bool key_down = key_bound_down(game, ks, BIND_DOWN);
  bool key_interact = key_bound_down(game, ks, BIND_USE);
  /* The dedicated jump key. UP is the keyboard's jump everywhere except over a
   * ladder, where the same key has to mean climb — which left the keyboard as
   * the one input that could not jump off a ladder at all, a move the pad has
   * had all along under A. */
  bool key_jump = key_bound_down(game, ks, BIND_JUMP);

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
    /* The stick is not a binding and cannot be — it is an axis, and there is
     * nothing about "push left" a player could usefully move elsewhere. The
     * buttons beside it are, and they are the d-pad only until somebody says
     * otherwise on the controls sheet. */
    pad_left = x < -GAMEPAD_AXIS_DEAD_ZONE || pad_bound_down(game, BIND_LEFT);
    pad_right = x > GAMEPAD_AXIS_DEAD_ZONE || pad_bound_down(game, BIND_RIGHT);
    pad_up = y < -GAMEPAD_AXIS_DEAD_ZONE || pad_bound_down(game, BIND_UP);
    pad_down = y > GAMEPAD_AXIS_DEAD_ZONE || pad_bound_down(game, BIND_DOWN);
    pad_interact = pad_bound_down(game, BIND_USE);
    pad_jump_held = pad_bound_down(game, BIND_JUMP);
    /* The pedal and the way out of a sheet stay on the letter rather than on
     * the binding: B is `cancel_held` on the title screen and the brake on the
     * drive, and neither of those is a sector control the sheet offers. */
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
  /* Pad only, and deliberately not the keyboard's ESC: ESC is a key that means
   * nothing but "quit" on the title screen and can answer on the press, where
   * B has four other jobs and has to be held to mean this one. */
  game->input.cancel_held = pad_brake;

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
         state == STATE_CREDITS ||
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
 *
 * X is the exception, and it keeps its own meaning rather than being folded
 * into "done": everywhere else X opens the options sheet, and the manual and
 * the options are the two things the title screen's one quiet line offers. So
 * it crosses straight from one to the other, and the sheet hands back to the
 * title screen the way it would have from there. Left out, X was the only
 * letter on the pad that did nothing at all on this screen.
 */
static bool handle_manual_gamepad(Game *game, SDL_GamepadButton button,
                                  PadFace face)
{
  if (face == PAD_FACE_ATTACK)
  {
    game_open_settings(game);
    return true;
  }
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
    game_close_manual(game);
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
  /*
   * An armed pad cap takes the very next button, and it has to come first: the
   * d-pad and A below are how the sheet is *navigated*, so testing them before
   * this would make the four directions and the confirm the five buttons a
   * player could never bind — which on a pad is most of the ones worth having.
   *
   * The same rule the keyboard's capture keeps, arrived at from the other
   * side: once armed, the next press means itself rather than what it does.
   * START and BACK are unbindable and so cancel, which is the pad's ESC.
   */
  if (game->settings_capturing && game_settings_slot_is_pad(game))
  {
    game_settings_capture_pad(game, pad_capture_button(game, button));
    return true;
  }
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
  if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
  {
    game_settings_adjust(game, 1);
    return true;
  }
  /* A is the sheet's yes, and what yes means is the row's business: it changes
   * a slider or a switch, opens the controls page, or arms a capture. It used
   * to be a straight `adjust(+1)`, which is the same thing for the only two
   * kinds of row that existed then. */
  if (face == PAD_FACE_CONFIRM)
  {
    game_settings_confirm(game);
    return true;
  }
  if (button == SDL_GAMEPAD_BUTTON_START ||
      button == SDL_GAMEPAD_BUTTON_BACK || face == PAD_FACE_CANCEL ||
      face == PAD_FACE_ATTACK || face == PAD_FACE_DOOR)
  {
    /* Innermost first, as on the keyboard: a capture, then the controls page,
     * then the sheet. A pad cannot take a key, so arming a capture and then
     * reaching for the keyboard is exactly what a player does — and B has to
     * put it down again for anyone who changes their mind. */
    if (!game_settings_leave_page(game))
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
      game->presentation.outro_cutscene.time >= OUTRO_REPLAY_PROMPT_TIME)
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
    /* SELECT arms the row; it does not spend it. The rule above — "dropping a
     * run on one press of the button players use to say 'not that' is the same
     * bug wearing a hat" — was written about B and was just as true of the
     * button beside START, which is the one this branch is. */
    if (abandons_run)
      game_pause_arm_abandon(game);
    else
      game_toggle_pause(game);
    return;
  }
  /* Everywhere else — the title screen included — there is nothing open to
   * close, so B is deliberately inert here. See the third rule above. */
}

static void handle_gamepad_button(Game *game, SDL_GamepadButton button);

/*
 * Which way the left stick is pushed, said as the d-pad button that means the
 * same thing — or INVALID when it is inside the dead zone.
 *
 * Read live off both axes rather than off the one the event happened to carry:
 * a diagonal arrives as two separate events, and which of the two a menu
 * should answer is only decidable with both in hand. The dominant axis wins,
 * with a tie going to the vertical, because every cursor in the game runs down
 * a column.
 */
static SDL_GamepadButton menu_stick_direction(const Game *game)
{
  if (game->platform.gamepad == NULL)
    return SDL_GAMEPAD_BUTTON_INVALID;

  Sint16 x = SDL_GetGamepadAxis(game->platform.gamepad,
                                SDL_GAMEPAD_AXIS_LEFTX);
  Sint16 y = SDL_GetGamepadAxis(game->platform.gamepad,
                                SDL_GAMEPAD_AXIS_LEFTY);
  int px = x < 0 ? -(int)x : (int)x;
  int py = y < 0 ? -(int)y : (int)y;
  if (px < GAMEPAD_AXIS_DEAD_ZONE && py < GAMEPAD_AXIS_DEAD_ZONE)
    return SDL_GAMEPAD_BUTTON_INVALID;
  if (py >= px)
    return y < 0 ? SDL_GAMEPAD_BUTTON_DPAD_UP : SDL_GAMEPAD_BUTTON_DPAD_DOWN;
  return x < 0 ? SDL_GAMEPAD_BUTTON_DPAD_LEFT : SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
}

/*
 * The left stick, on the three screens that have a cursor.
 *
 * Every menu in the game answered the d-pad and nothing else, while the same
 * stick steered Chuck, steered the car and is the first thing most thumbs
 * reach for — so the pause sheet, the options sheet and the manual were three
 * screens where half the pad quietly stopped working. The footers were honest
 * about it (`D-PAD: SELECT`), which is why this was a gap rather than a lie,
 * but naming the limitation is not the same as having a reason for it.
 *
 * The push is translated into the d-pad button that means the same thing and
 * handed to the very same three handlers, so there is exactly one description
 * anywhere of what up does on each sheet. A stick is an axis and a menu wants
 * presses, so the edge is made here: a step is taken when the push *changes*,
 * and holding a direction is one step rather than a row a frame.
 *
 * Nothing without a cursor is touched. A synthesised d-pad press has no
 * business reaching a sector, where the stick is already read every frame as
 * movement, and none reaching the title screen, whose chips are not a list.
 */
static void menu_stick_step(Game *game)
{
  SDL_GamepadButton pushed = menu_stick_direction(game);
  if (pushed == game->platform.pad_menu_direction)
    return;
  game->platform.pad_menu_direction = pushed;
  if (pushed == SDL_GAMEPAD_BUTTON_INVALID)
    return;
  /*
   * Not into an armed capture, and this is the one place the translation has
   * to stop being transparent.
   *
   * A push is handed on as *the d-pad button that means the same thing*, which
   * is exactly right for walking a cursor and exactly wrong for the one moment
   * the sheet is waiting to be told which button an action answers to: a thumb
   * resting on the stick would bind DPAD UP to whatever row was armed, and the
   * player never touched the d-pad. The stick is not on the bindable list for
   * the same reason it is not a control the sheet offers — it is an axis, and
   * it always moves you.
   */
  if (game->state == STATE_SETTINGS && game->settings_capturing &&
      game_settings_slot_is_pad(game))
  {
    return;
  }
  if (game->state == STATE_MANUAL || game->state == STATE_SETTINGS ||
      game->state == STATE_PAUSED)
    handle_gamepad_button(game, pushed);
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

  /*
   * Inside a sector the bindings answer first, and that is the whole of what
   * made the pad rebindable: the three edges a sector reads — attack, use and
   * the two weapon steps — used to be a `switch` over letters and bumpers
   * here, which is where "the pad layout is a property of the source rather
   * than of the player" was still true long after the keyboard stopped being.
   *
   * Only in a sector. Everywhere else a letter keeps its job, because the
   * letters are what every prompt in the game asks for by name and a player
   * who has moved attack onto B has said nothing whatever about which button
   * closes the pause sheet.
   */
  if (game->state == STATE_PLAYING)
  {
    bool handled = false;
    if (pad_press_is(game, button, BIND_SHOOT))
    {
      game->input.shoot = true;
      handled = true;
    }
    if (pad_press_is(game, button, BIND_USE))
    {
      game->input.use_door = true;
      handled = true;
    }
    if (pad_press_is(game, button, BIND_WEAPON_NEXT))
    {
      game->input.switch_weapon = true;
      handled = true;
    }
    if (pad_press_is(game, button, BIND_WEAPON_PREV))
    {
      game->input.switch_weapon_back = true;
      handled = true;
    }
    if (handled)
      return;
  }

  switch (face)
  {
  case PAD_FACE_CONFIRM:
    confirm_with_gamepad(game, true);
    return;
  case PAD_FACE_CANCEL:
    /* Not in a sector: attack is bound to B by default and the branch above
     * has already answered it there. Everywhere something is open, B backs
     * out of it. */
    back_out_with_gamepad(game, false);
    return;
  case PAD_FACE_ATTACK:
    if (game->state != STATE_PLAYING)
      /* The sheet opens from the title screen and from pause and returns to
       * whichever opened it; game_open_settings ignores every other state. */
      game_open_settings(game);
    return;
  case PAD_FACE_DOOR:
    /* The drive reads use_door as its skip: A and B are the pedals there, so
     * the way past the prologue moved to the one letter still free. */
    if (game->state == STATE_CHASE)
      game->input.use_door = true;
    else if (game->state == STATE_INTRO || game->state == STATE_PAUSED)
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
    /* On the title screen SELECT takes the resume the third chip is offering,
     * and does nothing at all when there is none. It is the one button left
     * that is free there: A starts, X and Y open the two sheets, and B is
     * deliberately inert. Everywhere else SELECT keeps its one other job,
     * which is the deliberate second step out of a paused run. */
    if (game->state == STATE_INTRO)
      game_resume_campaign(game);
    else
      back_out_with_gamepad(game, true);
    break;
  /* The bumpers cycle the weapons by default — the one job every platform's
   * own guidance gives them — and that is now a row on the controls sheet
   * rather than a case here; the sector branch above is what answers it. */
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
      game->platform.gamepad_id == event->gaxis.which)
  {
    /* Only a real push counts as the player picking the pad back up; a stick
     * settling back to centre is not somebody reaching for it. */
    if (event->gaxis.value < -GAMEPAD_AXIS_DEAD_ZONE ||
        event->gaxis.value > GAMEPAD_AXIS_DEAD_ZONE)
      game->platform.gamepad_active = true;
    /* The release matters as much as the push here, because it is what re-arms
     * the next step — so this runs on every axis event rather than only on the
     * ones past the dead zone. */
    menu_stick_step(game);
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
        game_close_manual(game);
        break;
      case MANUAL_HOT_NONE:
        break;
      }
    }
    else if (intro_hit_start_button(&game->presentation.intro, mx, my))
    {
      game->input.confirm = true;
    }
    else if (intro_hit_resume_button(&game->presentation.intro, mx, my))
    {
      game_resume_campaign(game);
    }
    else if (intro_hit_manual_button(&game->presentation.intro, mx, my))
    {
      game_open_manual(game);
    }
    else if (intro_hit_options_button(&game->presentation.intro, mx, my))
    {
      game_open_settings(game);
    }
    /* A click on the quit chip needs no hold: the mouse that reached it is the
     * same mouse that reaches the window's own close box, and neither is a
     * button anybody presses by reflex. The hold belongs to the pad's B, which
     * has four other jobs and no ESC beside it in fullscreen. */
    else if (intro_hit_quit_button(&game->presentation.intro, mx, my))
    {
      game->quit_requested = true;
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
        game_close_manual(game);
      /* J crosses to the options sheet, as X does on the pad: the two sheets
       * are siblings hanging off the title screen, not a hierarchy. */
      else if (sc == SDL_SCANCODE_J)
        game_open_settings(game);
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
      /*
       * A capture swallows the whole keyboard, and it has to: the next press
       * means *itself* rather than what it is bound to or what this screen
       * would otherwise do with it, which is the only way a player can put
       * their jump on W without the sheet reading it as "cursor up".
       */
      if (game_settings_capture_key(game, (int)sc))
        return;
      /*
       * The sheet's own navigation is not rebindable and is deliberately the
       * keys it always was. They are how a player who has just bound something
       * unreachable walks back to the row and undoes it — a menu steered by
       * the bindings it edits is a menu that can be locked shut.
       */
      if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
        game_settings_move_cursor(game, -1);
      else if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
        game_settings_move_cursor(game, 1);
      else if (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_A)
        game_settings_adjust(game, -1);
      else if (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_D)
        game_settings_adjust(game, 1);
      /* ENTER and SPACE are the sheet's "yes", and what yes means is the row's
       * business: a slider and a switch are changed by it, the controls row
       * opens its page, a binding row arms the capture. */
      else if (key == SDLK_SPACE || key == SDLK_RETURN || key == SDLK_KP_ENTER)
        game_settings_confirm(game);
      else if (sc == SDL_SCANCODE_J || sc == SDL_SCANCODE_H ||
               sc == SDL_SCANCODE_BACKSPACE)
      {
        /* Back out of the innermost thing first: the capture, then the
         * controls page, then the sheet. */
        if (!game_settings_leave_page(game))
          game_close_settings(game);
      }
      /* M is deliberately not answered here. It is the kill switch on top of
       * the mix, and this is the one screen showing the mix: a sheet reading
       * MUSIC 100 over a silent game is the same "two answers to one question"
       * that took mute off the pad's Y, said to the player who is looking
       * straight at the two levels that actually decide it. */
      return;
    }

    /* From a paused run as well as from the title screen: the manual is most
     * wanted on the floor it describes. `game_open_manual` remembers which of
     * the two it was opened over. */
    if ((game->state == STATE_INTRO || game->state == STATE_PAUSED) &&
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

    /* Q lands on ABANDON RUN and arms it rather than taking it. It used to
     * abandon outright, and Q is the default `BIND_WEAPON_NEXT` — the key a hand
     * cycling weapons already knows — so pausing and reaching for it out of habit
     * ended the night with no confirmation and nothing on the sheet naming the
     * key. See `PAUSE_ABANDON_ARMED` in pause_sheet.h. */
    if (game->state == STATE_PAUSED && sc == SDL_SCANCODE_Q)
    {
      game_pause_arm_abandon(game);
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

    if (state_accepts_confirm(game->state) &&
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
    /* Attack on Space only, and only in a sector — the same gate E, LSHIFT and
     * the UP jump keep. Every other state clears the edge inputs it did not
     * ask for, so an ungated press was harmless; a press the sector does not
     * own being reported anyway is still the rule this file is built on being
     * kept by accident rather than on purpose. */
    if (key_press_is(game, sc, BIND_SHOOT) && game->state == STATE_PLAYING)
    {
      game->input.shoot = true;
    }
    if (key_press_is(game, sc, BIND_WEAPON_NEXT) &&
        game->state == STATE_PLAYING)
    {
      game->input.switch_weapon = true;
    }
    /* The ring is walked both ways on the pad, so it is walked both ways here
     * too: a keyboard that could only ever go forward is a keyboard three
     * presses from the weapon a bumper reaches in one. */
    if (key_press_is(game, sc, BIND_WEAPON_PREV) &&
        game->state == STATE_PLAYING)
    {
      game->input.switch_weapon_back = true;
    }
    /* A jump key that is not also the climb key, so it needs no ladder test:
     * it reports the press and player_update decides whether to honour it,
     * now or from the buffer — exactly as the pad's A does. */
    if (key_press_is(game, sc, BIND_JUMP) && game->state == STATE_PLAYING)
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
    if (key_press_is(game, sc, BIND_UP) && game->state == STATE_PLAYING)
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
    if (key_press_is(game, sc, BIND_USE) && game->state == STATE_PLAYING)
    {
      game->input.use_door = true;
    }
    if (key == SDLK_R && game->state == STATE_OUTRO &&
        game->presentation.outro_cutscene.time >= OUTRO_REPLAY_PROMPT_TIME)
    {
      game->input.restart = true;
    }
    /* R on the title screen takes the resume the third chip is offering. The
     * two readings of R never overlap: one is a card at the end of a finished
     * campaign, the other is the screen a new session opens on. */
    if (key == SDLK_R && game->state == STATE_INTRO)
    {
      game_resume_campaign(game);
    }
  }
}
