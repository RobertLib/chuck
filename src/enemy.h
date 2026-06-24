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
} Enemy;

void enemy_init(Enemy *enemy, float x, float y);
void enemy_update(Enemy *enemy, Level *level, float dt);

#endif /* CHUCK_ENEMY_H */
