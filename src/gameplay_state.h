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
    float vx, vy;
    float angle;
    int variant;
    bool active;
} ThrownObject;

typedef struct
{
    float x, y;
    float vx, vy;
    float anim_time;
    bool active;
} Bird;

/* Facade wind runs as one building-wide cycle: quiet, an announced warning,
 * then the gust itself. Keeping it a single phase machine means the renderer,
 * the HUD and the simulation all read the same state. */
typedef enum
{
    FACADE_WIND_CALM = 0,
    FACADE_WIND_WARNING,
    FACADE_WIND_GUSTING
} FacadeWindPhase;

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
    int continues_remaining;
    int score;
    float level_elapsed_time;
    int level_start_score;
    float continue_timer;
} CampaignState;

void campaign_reset(CampaignState *campaign);
bool campaign_lose_life(CampaignState *campaign);
bool campaign_begin_continue(CampaignState *campaign);
bool campaign_update_continue(CampaignState *campaign, float dt);
bool campaign_accept_continue(CampaignState *campaign);

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
    ThrownObject thrown_objects[MAX_THROWN_OBJECTS];
    Bird birds[MAX_BIRDS];
    float facade_hazard_spawn_timers[MAX_FACADE_HAZARD_SPAWNS];
    float facade_hazard_windup_timers[MAX_FACADE_HAZARD_SPAWNS];
    bool facade_hazards_initialized;
    FacadeWindPhase facade_wind_phase;
    float facade_wind_timer;
    int facade_wind_dir;
    bool facade_wind_sheltered;
    /* Highest banked position on the wall; always somewhere the climber has
     * actually stood, so respawning there can never place him inside stone. */
    float facade_checkpoint_x;
    float facade_checkpoint_y;
    bool facade_has_checkpoint;

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
