#ifndef CHUCK_GAMEPLAY_STATE_H
#define CHUCK_GAMEPLAY_STATE_H

#include "enemy.h"
#include "game_event.h"
#include "level.h"
#include "player.h"
#include "rng.h"

typedef struct
{
    float x, y;
    float vx, vy;
    bool active;
} Bullet;

typedef struct
{
    float x, y;
    float vx, vy;
    bool active;
    float timer;
    float fuse_sound_timer;
    bool grounded;
} Grenade;

typedef struct
{
    float x, y;
    float vx;
    bool active;
} Rocket;

typedef struct
{
    float x, y;
    bool active;
    bool triggered;
    float timer;
} Mine;

typedef enum
{
    JANITOR_WALK,
    JANITOR_MOP,
    JANITOR_PAUSE
} JanitorActivity;

typedef struct
{
    float x, y;
    float life;
    bool active;
} JanitorWetSpot;

typedef struct
{
    float x, y;
    float vx, vy;
    int dir;
    bool on_ground;
    JanitorActivity activity;
    float activity_timer;
    float anim_time;
    float wet_timer;
    int next_wet_spot;
    JanitorWetSpot wet_spots[JANITOR_WET_SPOTS];
} Janitor;

typedef struct
{
    int current_level;
    int lives;
    int score;
    float level_elapsed_time;
    int level_start_score;
} CampaignState;

typedef struct
{
    Rng rng;
    GameEventBuffer events;

    Level level;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int enemy_count;
    Dog dogs[MAX_DOGS];
    int dog_count;
    Janitor janitors[MAX_JANITORS];
    int janitor_count;
    Mine mines[MAX_MINES];
    int mine_count;
    Grenade grenades[MAX_GRENADES];
    int grenade_count;
    Rocket rockets[MAX_ROCKETS];
    Bullet bullets[MAX_BULLETS];
    Bullet enemy_bullets[MAX_ENEMY_BULLETS];

    float invuln_timer;
    int door_spawns[MAX_DOORS];
    float door_timers[MAX_DOORS];
    float teleport_cooldown;
    int player_on_elevator;
    int player_on_moving_platform;

    float terminal_hack_progress;
    float terminal_hack_tick_timer;
    bool terminal_in_range;
    bool terminal_hacking;
    /* Historical name: terminal breaches and guard-operated switches now
     * share this building-wide quiet-time countdown. */
    float terminal_alarm_timer;
    float alarm_target_x;
    float alarm_target_y;
    float alarm_siren_timer;
    int active_alarm_switch;
    float terminal_reinforcement_timer;
    int terminal_reinforcements_pending;
} GameplayState;

/* Clear all per-level simulation state while preserving the RNG stream. */
void gameplay_state_begin_level(GameplayState *state);

#endif /* CHUCK_GAMEPLAY_STATE_H */
