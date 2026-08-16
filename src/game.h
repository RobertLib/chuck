#ifndef CHUCK_GAME_H
#define CHUCK_GAME_H

#include "audio.h"
#include "chase.h"
#include "common.h"
#include "credits.h"
#include "cutscene.h"
#include "gameplay_state.h"
#include "intro.h"
#include "manual.h"
#include "pad_hint.h"
#include "particle.h"
#include "settings.h"

typedef enum
{
    STATE_ABDUCTION,
    STATE_CHASE,
    STATE_OPENING_CUTSCENE,
    STATE_INTRO,
    STATE_MANUAL,
    STATE_SETTINGS,
    STATE_LEVEL_START,
    STATE_SHOW_KEYCARD,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_LEVEL_TRANSITION,
    STATE_LEVEL_CLEARED,
    STATE_OUTRO,
    STATE_CREDITS,
    STATE_CONTINUE,
    STATE_GAME_OVER
} GameState;

/* What the pause menu offers, in the order it lists them. */
typedef enum
{
    PAUSE_ITEM_RESUME,
    PAUSE_ITEM_SETTINGS,
    PAUSE_ITEM_ABANDON,
    PAUSE_ITEM_COUNT
} PauseItem;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Gamepad *gamepad;
    SDL_JoystickID gamepad_id;
    /* What the open pad prints on its face buttons, and where. Read once when
     * it is plugged in, because every prompt on every screen is spelled from
     * it and none of them should be asking a driver mid-frame. */
    PadHints pad;
    AudioSystem audio;
    bool fullscreen;
    bool gamepad_active;
    Uint64 last_tick;
} PlatformState;

typedef struct
{
    float message_timer;

    /* Particle system for effects */
    ParticleSystem particles;

    float footstep_audio_timer;
    float ladder_audio_timer;
    bool footstep_alternate;
    bool fall_platform_sounded[MAX_FALL_PLATFORMS];

    /* How much of the landing squash the player figure has left to play, 1 down
     * to 0. It is a weight cue and nothing else — the shell derives it from the
     * fall speed the physics step already reports, so no gameplay module has to
     * know the figure compresses when it lands. */
    float player_land_squash;

    /* The three beats of the prologue, in the order they play: the kerb where
     * she is taken, the drive across town (in `Chase`), and the pavement
     * outside the tower she is walked into. */
    AbductionCutscene abduction_cutscene;
    OpeningCutscene opening_cutscene;

    /* Results report and the captors' next flight of stairs, between levels. */
    LevelTransition level_transition;

    /* Final rooftop rescue and happy ending after the last level, and the roll
     * of names it hands to on its way back to the title screen. */
    OutroCutscene outro_cutscene;
    CreditsRoll credits;

    /* Title-screen state (field-operations briefing shown before STATE_LEVEL_START) */
    Intro intro;

    /* The field manual, opened from the title screen. */
    Manual manual;

    float cam_x; /* world x of left edge of the viewport */
    float cam_y; /* world y above the visible gameplay area */
    /* Short, decaying render offset used by gameplay explosions. The HUD is
     * rendered separately and therefore stays readable while the world shakes. */
    float camera_shake_timer;
    float camera_shake_duration;
    float camera_shake_strength;
    float camera_shake_x;
    float camera_shake_y;
    /* Key-card intro animation state (used between reveal and playing) */
    int card_anim_current;     /* current highlighted position */
    int card_anim_step;        /* steps advanced in animation */
    int card_anim_total_steps; /* total steps to run before stopping */
    int card_anim_count;       /* number of card positions */
    float card_anim_interval;  /* seconds between highlight steps */
    float card_anim_timer;     /* accumulator for highlight timing */
    float exit_unlocked_timer; /* seconds to show "EXIT UNLOCKED" overlay */
    float extra_life_timer;    /* seconds to flash the score-earned 1UP */

    /* The last thing anybody on the crew said within earshot, and how long is
     * left to read it. Only the shell holds it: the simulation reported that
     * somebody spoke and drew a number, and the words are looked up out of
     * [crew.c](crew.c) at draw time. A second line inside the window replaces
     * the first outright rather than queueing — the plate is one line high,
     * and a backlog of overheard traffic would still be printing the lobby's
     * jokes two rooms later. */
    ChatterKind chatter_kind;
    int chatter_speaker;
    int chatter_roll;
    float chatter_timer;
} PresentationState;

typedef struct
{
    PlatformState platform;
    CampaignState campaign;
    GameplayState gameplay;
    /* The prologue pursuit. It runs once, between the title screen and the
     * opening cutscene, and is a self-contained simulation of its own rather
     * than a variation on the platformer. */
    Chase chase;
    /* The inactive half of a sublevel visit. It holds the paused main level
     * while Chuck is inside, and the persistent restroom state while outside. */
    GameplayState inactive_gameplay;
    bool sublevel_initialized;
    bool in_sublevel;
    float main_level_cam_x;
    float main_level_cam_y;
    /* The parked half of PresentationState.fall_platform_sounded. It is keyed
     * by a panel of one particular level, so it changes hands with that level
     * rather than staying behind with the shell. */
    bool inactive_fall_platform_sounded[MAX_FALL_PLATFORMS];
    PresentationState presentation;
    Input input;
    GameState state;
    /* Where the pause and the assist sheet return to. Pausing must resume the
     * exact state it interrupted (re-entering STATE_LEVEL_START would replay
     * the reveal), so the return path is a stored state, not a transition. */
    GameState pause_return_state;
    GameState settings_return_state;
    /* Everything the player has decided, and where each sheet's cursor is
     * standing. The settings survive a campaign reset and are written back to
     * disk when the sheet is closed, so a run is never what a preference
     * belongs to. */
    Settings settings;
    int settings_cursor;
    int pause_cursor;
#ifdef CHUCK_DEBUG
    int debug_selected_level;
#endif
} Game;

bool game_init(Game *game);
bool game_init_seeded(Game *game, uint64_t seed);
void game_handle_event(Game *game, const SDL_Event *event);
void game_update(Game *game, float dt);
void game_render(Game *game);
void game_shutdown(Game *game);

/* Abort the current game (if any) and go back to the title screen. */
void game_return_to_intro(Game *game);

/* Open the field manual. It is read from the title screen, and closed with the
 * same route back as anything else: game_return_to_intro. */
void game_open_manual(Game *game);

/* The pause menu: three items, walked with the cursor and answered with
 * confirm. Pausing resumes the exact state it interrupted. */
void game_toggle_pause(Game *game);
void game_pause_move_cursor(Game *game, int delta);
void game_pause_activate(Game *game);

/* The options sheet. It opens from the title screen or from pause and returns
 * to whichever opened it, and every change it makes is felt at once: a volume
 * reaches the mixer, fullscreen reaches the window, an assist switch reaches
 * whatever is running. Closing it writes the file. */
void game_open_settings(Game *game);
void game_close_settings(Game *game);
void game_settings_move_cursor(Game *game, int delta);
void game_settings_adjust(Game *game, int delta);

/* Fullscreen, from the sheet or from F. One function, because the window and
 * the saved setting must never disagree about which the player asked for. */
void game_set_fullscreen(Game *game, bool on);

/* Write the settings file. The sheet does it on the way out; so does shutdown,
 * which is what catches a fullscreen toggled with F mid-run. */
void game_save_settings(const Game *game);

/* Start a clean campaign directly in one embedded level, skipping the title
 * screen and the prologue. The debug level picker uses it, and so does the
 * `--level N` switch the editor's playtest button launches the game with. */
bool game_start_at_level(Game *game, int level_index);

/* Helper: obtain current view size (logical or window). Exposed to render
 * module so rendering and camera code can share the same behavior. */
void game_get_view_size(Game *game, int *out_w, int *out_h);

/* Open/close the first available gamepad and track hot-plug events. */
void game_input_init(Game *game);
void game_input_shutdown(Game *game);

/* The pad every prompt is spelled for, or NULL when the keyboard is the thing
 * in the player's hands. One answer, so no two screens can disagree about
 * which set of hints the frame is wearing. */
const PadHints *game_pad_hints(const Game *game);

/* Read current keyboard and gamepad state into `game->input`. */
void game_read_input(Game *game);

#endif /* CHUCK_GAME_H */
