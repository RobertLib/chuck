#ifndef CHUCK_ENEMY_H
#define CHUCK_ENEMY_H

#include "game_config.h"
#include "level.h"
#include "rng.h"

#include <stdbool.h>

typedef struct
{
    float x, y; /* top-left of the enemy box */
    float vx, vy;
    int dir; /* horizontal patrol direction: -1 or +1 */
    bool on_ground;
    bool climbing;
    int climb_dir;  /* -1 = up, +1 = down */
    int ladder_col; /* column the enemy is climbing */
    float climb_cooldown;
    int hp; /* current hit points, 1..ENEMY_HP */
    bool dead;
    float shoot_cooldown; /* seconds until next shot */
    float aim_timer;      /* > 0 while enemy is standing still to aim */
    /* Stored target coordinates captured when aiming starts. */
    float aim_target_x;
    float aim_target_y;
    /* Last position the guard could actually see (or the alarm source).
     * Keeping this snapshot prevents tracking the live player through floors. */
    float pursuit_target_x;
    float pursuit_target_y;
    bool has_pursuit_target;
    /* A guard that survives a player bullet keeps hunting the shooter even
     * when the terminal alarm is inactive or the player is initially outside
     * the guard's normal sight and shooting range. */
    bool provoked;
    bool talking;        /* true while chatting with another enemy */
    float talk_timer;    /* seconds remaining while talking */
    float talk_cooldown; /* seconds remaining before eligible to talk again */
    int talk_partner;    /* index of partner enemy, -1 if none */
    float anim_time;     /* local procedural animation clock */
    float recoil_timer;  /* brief muzzle flash / firing follow-through */
} Enemy;

typedef enum
{
    DOG_GUARD,
    DOG_ROAM,
    DOG_RETURN,
    DOG_CHASE
} DogState;

typedef struct
{
    float x, y; /* top-left of the dog box */
    float vx, vy;
    int dir;
    bool on_ground;
    int hp;
    bool dead;
    int owner; /* index into Game.enemies[], -1 after the handler is gone */
    DogState state;
    float state_timer;
    float bite_cooldown;
    float lost_timer;
    /* Dogs keep running to the last visible/alarm position, not to the
     * player's live position after line of sight has been lost. */
    float chase_target_x;
    bool has_chase_target;
    float guard_x;
    float guard_y;
    float roam_target_x;
    float vocal_timer; /* seconds until another chase bark or growl */
    float anim_time;    /* local procedural animation clock */
    float attack_timer; /* bite/lunge follow-through */
} Dog;

void enemy_init(Enemy *enemy, float x, float y, Rng *rng);
void enemy_update(Enemy *enemy, Level *level, float dt,
                  bool pursuing, float target_x, float target_y,
                  bool hemmed_in, Rng *rng);
void dog_init(Dog *dog, float x, float y, int owner, Rng *rng);

#endif /* CHUCK_ENEMY_H */
