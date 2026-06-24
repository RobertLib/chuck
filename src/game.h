#ifndef CHUCK_GAME_H
#define CHUCK_GAME_H

#include "common.h"
#include "level.h"
#include "player.h"
#include "enemy.h"
#include "particle.h"

#define LEVEL_COUNT 2

typedef struct
{
    float x, y; /* world-space position (top-left) */
    float vx;   /* horizontal velocity, no gravity */
    bool active;
} Bullet;

typedef enum
{
    STATE_PLAYING,
    STATE_LEVEL_CLEARED,
    STATE_GAME_OVER,
    STATE_WIN
} GameState;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;

    Level level;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int enemy_count;

    Bullet bullets[MAX_BULLETS];             /* fired by player */
    Bullet enemy_bullets[MAX_ENEMY_BULLETS]; /* fired by enemies */

    Input input;

    int current_level; /* 0-based index */
    int lives;
    int score;
    float invuln_timer;
    float message_timer; /* pause before advancing on level cleared */

    /* Door state (per door, indexed same as level.doors[]) */
    int door_spawns[MAX_DOORS];   /* enemies still to spawn from each door */
    float door_timers[MAX_DOORS]; /* seconds until next spawn from each door */
    float teleport_cooldown;      /* seconds before player can teleport again */

    int player_on_elevator; /* index into level.elevators[], or -1 */

    /* Particle system for effects */
    ParticleSystem particles;

    GameState state;
    Uint64 last_tick;
} Game;

bool game_init(Game *game);
void game_handle_event(Game *game, const SDL_Event *event);
void game_update(Game *game, float dt);
void game_render(Game *game);
void game_shutdown(Game *game);

#endif /* CHUCK_GAME_H */
