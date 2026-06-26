#include "enemy.h"

#include <math.h>

void enemy_init(Enemy *enemy, float x, float y)
{
    enemy->x = x;
    enemy->y = y;
    enemy->vx = 0.0f;
    enemy->vy = 0.0f;
    enemy->dir = (SDL_rand(2) == 0) ? -1 : 1;
    enemy->on_ground = false;
    enemy->climbing = false;
    enemy->climb_dir = -1;
    enemy->ladder_col = 0;
    enemy->climb_cooldown = ENEMY_CLIMB_COOLDOWN;
    enemy->hp = ENEMY_HP;
    enemy->dead = false;
    /* Stagger initial shoot time so enemies don't all fire at once */
    enemy->shoot_cooldown = 1.0f + SDL_rand(250) * 0.01f;
    enemy->aim_timer = 0.0f;
    enemy->aim_target_x = 0.0f;
    enemy->aim_target_y = 0.0f;
    enemy->talking = false;
    enemy->talk_timer = 0.0f;
    enemy->talk_partner = -1;
    enemy->talk_cooldown = 0.0f;
}

static void enemy_update_talking(Enemy *enemy, Level *level, float dt)
{
    /* While talking, enemies stand still on the ground (or fall if unsupported).
     * They do not aim or patrol. The talk timer counts down until they resume. */
    enemy->vy += GRAVITY * dt;
    if (enemy->vy > MAX_FALL_SPEED)
        enemy->vy = MAX_FALL_SPEED;

    enemy->vx = 0.0f;

    enemy->talk_timer -= dt;
    if (enemy->talk_timer <= 0.0f)
    {
        enemy->talk_timer = 0.0f;
        enemy->talking = false;
        /* Leave partner cleanup to game logic; start per-enemy cooldown */
        if (enemy->talk_cooldown <= 0.0f)
            enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
    }

    bool ignored_ground = false;
    level_move(level, &enemy->x, &enemy->y, &enemy->vx, &enemy->vy,
               ENEMY_W, ENEMY_H, dt, false, &ignored_ground,
               false);

    /* Tick per-enemy cooldown if any */
    if (enemy->talk_cooldown > 0.0f)
    {
        enemy->talk_cooldown -= dt;
        if (enemy->talk_cooldown < 0.0f)
            enemy->talk_cooldown = 0.0f;
    }
}

static void enemy_update_climbing(Enemy *enemy, Level *level, float dt)
{
    /* Smoothly slide toward the ladder column centre instead of hard-snapping,
       to avoid a one-frame position jump when climbing starts. */
    float target_x = enemy->ladder_col * (float)TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
    enemy->vx = (target_x - enemy->x) * 10.0f;
    enemy->vy = (float)enemy->climb_dir * ENEMY_CLIMB_SPEED;

    bool ladder_ahead;
    if (enemy->climb_dir < 0)
    {
        int row_above = (int)floorf((enemy->y - 1.0f) / TILE_SIZE);
        ladder_ahead = level_is_ladder(level, enemy->ladder_col, row_above);
    }
    else
    {
        int row_below = (int)floorf((enemy->y + ENEMY_H + 1.0f) / TILE_SIZE);
        ladder_ahead = level_is_ladder(level, enemy->ladder_col, row_below);
    }

    /* A side exit is only usable when the adjacent column is free of solid
       tiles across the enemy's full body height AND there is floor to land on.
       This prevents the enemy from trying to step off into a blocked column
       (e.g. while the body still overlaps the floor row on both sides),
       which would cause instant wall-bounce oscillation. */
    int top_row = (int)floorf(enemy->y / TILE_SIZE);
    int bot_row = (int)floorf((enemy->y + ENEMY_H - 1.0f) / TILE_SIZE);

    bool left_clear = true, right_clear = true;
    for (int r = top_row; r <= bot_row; ++r)
    {
        if (level_is_solid(level, enemy->ladder_col - 1, r))
            left_clear = false;
        if (level_is_solid(level, enemy->ladder_col + 1, r))
            right_clear = false;
    }
    bool can_left = left_clear && level_is_solid(level, enemy->ladder_col - 1, bot_row + 1);
    bool can_right = right_clear && level_is_solid(level, enemy->ladder_col + 1, bot_row + 1);
    bool floor_beside = can_left || can_right;

    if (!ladder_ahead || (floor_beside && SDL_rand(100) < 4))
    {
        enemy->climbing = false;
        enemy->vy = 0.0f;
        enemy->climb_cooldown = ENEMY_CLIMB_COOLDOWN;
        if (can_left && can_right)
            enemy->dir = (SDL_rand(2) == 0) ? -1 : 1;
        else if (can_left)
            enemy->dir = -1;
        else if (can_right)
            enemy->dir = 1;
        else
            enemy->dir = (SDL_rand(2) == 0) ? -1 : 1;
    }

    bool ignored_ground = false;
    level_move(level, &enemy->x, &enemy->y, &enemy->vx, &enemy->vy,
               ENEMY_W, ENEMY_H, dt, true, &ignored_ground,
               false);
}

static void enemy_update_walking(Enemy *enemy, Level *level, float dt)
{
    enemy->vy += GRAVITY * dt;
    if (enemy->vy > MAX_FALL_SPEED)
    {
        enemy->vy = MAX_FALL_SPEED;
    }

    /* Stand still while aiming */
    if (enemy->aim_timer > 0.0f)
    {
        enemy->vx = 0.0f;
        level_move(level, &enemy->x, &enemy->y, &enemy->vx, &enemy->vy,
                   ENEMY_W, ENEMY_H, dt, false, &enemy->on_ground,
                   false);
        return;
    }

    /* Speed decreases with each hit taken */
    float speed = ENEMY_WALK_SPEED;
    if (enemy->hp == ENEMY_HP - 1)
        speed = ENEMY_WALK_SPEED * ENEMY_SPEED_HP2;
    else if (enemy->hp < ENEMY_HP - 1)
        speed = ENEMY_WALK_SPEED * ENEMY_SPEED_HP1;

    enemy->vx = (float)enemy->dir * speed;

    if (enemy->on_ground)
    {
        int center_col = (int)floorf((enemy->x + ENEMY_W * 0.5f) / TILE_SIZE);
        int center_row = (int)floorf((enemy->y + ENEMY_H * 0.5f) / TILE_SIZE);
        int foot_row = (int)floorf((enemy->y + ENEMY_H * 0.5f) / TILE_SIZE);

        /* Reverse at walls. */
        int ahead_col = (enemy->dir > 0)
                            ? (int)floorf((enemy->x + ENEMY_W + 1.0f) / TILE_SIZE)
                            : (int)floorf((enemy->x - 1.0f) / TILE_SIZE);
        if (level_is_solid(level, ahead_col, foot_row))
        {
            enemy->dir = -enemy->dir;
            enemy->vx = (float)enemy->dir * ENEMY_WALK_SPEED;
        }

        /* Maybe use a ladder we are standing over. */
        enemy->climb_cooldown -= dt;
        bool ladder_here = level_is_ladder(level, center_col, center_row);
        if (ladder_here && enemy->climb_cooldown <= 0.0f)
        {
            bool up_ok = level_is_ladder(level, center_col, center_row - 1);
            bool down_ok = level_is_ladder(level, center_col, center_row + 1);
            if ((up_ok || down_ok) && SDL_rand(100) < ENEMY_CLIMB_CHANCE)
            {
                enemy->climbing = true;
                enemy->ladder_col = center_col;
                if (up_ok && down_ok)
                {
                    enemy->climb_dir = (SDL_rand(2) == 0) ? -1 : 1;
                }
                else
                {
                    enemy->climb_dir = up_ok ? -1 : 1;
                }
                enemy->climb_cooldown = ENEMY_CLIMB_COOLDOWN;
                return;
            }
        }
    }

    level_move(level, &enemy->x, &enemy->y, &enemy->vx, &enemy->vy,
               ENEMY_W, ENEMY_H, dt, false, &enemy->on_ground,
               false);
}

void enemy_update(Enemy *enemy, Level *level, float dt)
{
    if (enemy->talking)
    {
        enemy_update_talking(enemy, level, dt);
        return;
    }
    if (enemy->climbing)
    {
        enemy_update_climbing(enemy, level, dt);
    }
    else
    {
        enemy_update_walking(enemy, level, dt);
    }
}
