#ifndef CHUCK_GAME_H
#define CHUCK_GAME_H

#include "common.h"
#include "level.h"
#include "player.h"
#include "enemy.h"
#include "particle.h"
#include "intro.h"
#include "cutscene.h"
#include "audio.h"

typedef struct
{
    float x, y; /* world-space position (top-left) */
    float vx;   /* horizontal velocity, no gravity */
    float vy;   /* vertical velocity for aimed bullets */
    bool active;
} Bullet;

typedef struct
{
    float x, y;    /* top-left of grenade box */
    float vx, vy;  /* full physics (gravity) */
    bool active;   /* true until exploded */
    float timer;   /* countdown until explosion */
    bool grounded; /* true when resting on floor */
} Grenade;

typedef enum
{
    STATE_OPENING_CUTSCENE,
    STATE_INTRO,
    STATE_LEVEL_START,
    STATE_SHOW_KEYCARD,
    STATE_PLAYING,
    STATE_LEVEL_TRANSITION,
    STATE_LEVEL_CLEARED,
    STATE_OUTRO,
    STATE_GAME_OVER,
    STATE_WIN
} GameState;

typedef struct
{
    float x, y;     /* top-left of mine box */
    bool active;    /* not yet exploded */
    bool triggered; /* player stepped on it */
    float timer;    /* countdown until explosion */
} Mine;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;

    Level level;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int enemy_count;
    Dog dogs[MAX_DOGS];
    int dog_count;

    Mine mines[MAX_MINES];
    int mine_count;

    Grenade grenades[MAX_GRENADES];
    int grenade_count;

    Bullet bullets[MAX_BULLETS];             /* fired by player */
    Bullet enemy_bullets[MAX_ENEMY_BULLETS]; /* fired by enemies */

    Input input;

    int current_level; /* 0-based index */
    int lives;
    int score;
    float invuln_timer;
    float message_timer; /* pause before advancing on level cleared */
    float level_elapsed_time;
    int level_start_score;

    /* Door state (per door, indexed same as level.doors[]) */
    int door_spawns[MAX_DOORS];   /* enemies still to spawn from each door */
    float door_timers[MAX_DOORS]; /* seconds until next spawn from each door */
    float teleport_cooldown;      /* seconds before player can teleport again */

    int player_on_elevator;        /* index into level.elevators[], or -1 */
    int player_on_moving_platform; /* index into level.moving_platforms[], or -1 */

    /* Particle system for effects */
    ParticleSystem particles;

    /* Procedurally generated, memory-cached sound effects and voice pool. */
    AudioSystem audio;
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

    bool fullscreen; /* true when window is fullscreen */

    GameState state;
    Uint64 last_tick;
    float cam_x; /* world x of left edge of the viewport */
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

    /* Active-terminal interaction. Progress requires holding interact while
     * stationary and resets if the player lets go or leaves its range. */
    float terminal_hack_progress;
    float terminal_hack_tick_timer;
    bool terminal_in_range;
    bool terminal_hacking;
    float terminal_alarm_timer;
    float terminal_reinforcement_timer;
    int terminal_reinforcements_pending;
} Game;

bool game_init(Game *game);
void game_handle_event(Game *game, const SDL_Event *event);
void game_update(Game *game, float dt);
void game_render(Game *game);
void game_shutdown(Game *game);

/* Abort the current game (if any) and go back to the title screen. */
void game_return_to_intro(Game *game);

/* Helper: obtain current view size (logical or window). Exposed to render
 * module so rendering and camera code can share the same behavior. */
void game_get_view_size(Game *game, int *out_w, int *out_h);

/* Read current keyboard state into `game->input`. */
void game_read_input(Game *game);

#endif /* CHUCK_GAME_H */
