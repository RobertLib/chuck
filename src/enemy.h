#ifndef CHUCK_ENEMY_H
#define CHUCK_ENEMY_H

#include "common.h"
#include "level.h"

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
    float guard_x;
    float guard_y;
    float roam_target_x;
    float vocal_timer; /* seconds until another chase bark or growl */
    float anim_time;    /* local procedural animation clock */
    float attack_timer; /* bite/lunge follow-through */
} Dog;

void enemy_init(Enemy *enemy, float x, float y);
void enemy_update(Enemy *enemy, Level *level, float dt);
void dog_init(Dog *dog, float x, float y, int owner);

#endif /* CHUCK_ENEMY_H */
