#ifndef CHUCK_GAME_H
#define CHUCK_GAME_H

#include "audio.h"
#include "chase.h"
#include "common.h"
#include "cutscene.h"
#include "gameplay_state.h"
#include "intro.h"
#include "particle.h"

typedef enum
{
    STATE_CHASE,
    STATE_OPENING_CUTSCENE,
    STATE_INTRO,
    STATE_LEVEL_START,
    STATE_SHOW_KEYCARD,
    STATE_PLAYING,
    STATE_LEVEL_TRANSITION,
    STATE_LEVEL_CLEARED,
    STATE_OUTRO,
    STATE_CONTINUE,
    STATE_GAME_OVER
} GameState;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Gamepad *gamepad;
    SDL_JoystickID gamepad_id;
    AudioSystem audio;
    bool fullscreen;
    bool gamepad_active;
    bool quit_requested;
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

    /* One-shot story setup shown only when the application first launches. */
    OpeningCutscene opening_cutscene;

    /* Results report and continuing hostage pursuit shown between levels. */
    LevelTransition level_transition;

    /* Final rooftop rescue and happy ending after the last level. */
    OutroCutscene outro_cutscene;

    /* Title-screen state (field-operations briefing shown before STATE_LEVEL_START) */
    Intro intro;

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
    PresentationState presentation;
    Input input;
    GameState state;
} Game;

bool game_init(Game *game);
bool game_init_seeded(Game *game, uint64_t seed);
void game_handle_event(Game *game, const SDL_Event *event);
void game_update(Game *game, float dt);
void game_render(Game *game);
void game_shutdown(Game *game);

/* Abort the current game (if any) and go back to the title screen. */
void game_return_to_intro(Game *game);

/* Helper: obtain current view size (logical or window). Exposed to render
 * module so rendering and camera code can share the same behavior. */
void game_get_view_size(Game *game, int *out_w, int *out_h);

/* Open/close the first available gamepad and track hot-plug events. */
void game_input_init(Game *game);
void game_input_shutdown(Game *game);

/* Read current keyboard and gamepad state into `game->input`. */
void game_read_input(Game *game);

#endif /* CHUCK_GAME_H */
