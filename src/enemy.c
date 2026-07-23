#include "enemy.h"

#include <math.h>

void enemy_init(Enemy *enemy, float x, float y, Rng *rng)
{
    enemy->x = x;
    enemy->y = y;
    enemy->vx = 0.0f;
    enemy->vy = 0.0f;
    enemy->dir = rng_range(rng, 2) == 0 ? -1 : 1;
    enemy->on_ground = false;
    enemy->climbing = false;
    enemy->climb_dir = -1;
    enemy->ladder_col = 0;
    enemy->climb_cooldown = ENEMY_CLIMB_COOLDOWN;
    enemy->obstacle_avoid_timer = 0.0f;
    enemy->hp = ENEMY_HP;
    enemy->dead = false;
    /* Stagger initial shoot time so enemies don't all fire at once */
    enemy->shoot_cooldown = 1.0f + rng_range(rng, 250) * 0.01f;
    enemy->aim_timer = 0.0f;
    enemy->aim_target_x = 0.0f;
    enemy->aim_target_y = 0.0f;
    enemy->pursuit_target_x = x + ENEMY_W * 0.5f;
    enemy->pursuit_target_y = y + ENEMY_H * 0.5f;
    enemy->has_pursuit_target = false;
    enemy->provoked = false;
    enemy->encounter_decided = false;
    enemy->encounter_lost_timer = 0.0f;
    enemy->raising_alarm = false;
    enemy->alarm_switch_index = -1;
    enemy->alarm_use_timer = 0.0f;
    enemy->investigate_timer = 0.0f;
    enemy->investigate_x = x + ENEMY_W * 0.5f;
    enemy->investigate_y = y + ENEMY_H * 0.5f;
    enemy->investigate_scan_timer = 0.0f;
    enemy->alerted_by_body = false;
    enemy->aim_vdir = 0;
    enemy->talking = false;
    enemy->talk_timer = 0.0f;
    enemy->talk_partner = -1;
    enemy->talk_cooldown = 0.0f;
    enemy->anim_time = (float)rng_range(rng, 628) * 0.01f;
    enemy->recoil_timer = 0.0f;
}

void dog_init(Dog *dog, float x, float y, int owner, Rng *rng)
{
    dog->x = x;
    dog->y = y;
    dog->vx = 0.0f;
    dog->vy = 0.0f;
    dog->dir = rng_range(rng, 2) == 0 ? -1 : 1;
    dog->on_ground = false;
    dog->hp = DOG_HP;
    dog->dead = false;
    dog->owner = owner;
    dog->state = DOG_GUARD;
    dog->state_timer = 0.4f + rng_range(rng, 120) * 0.01f;
    dog->bite_cooldown = 0.0f;
    dog->lost_timer = 0.0f;
    dog->chase_target_x = x + DOG_W * 0.5f;
    dog->has_chase_target = false;
    dog->guard_x = x;
    dog->guard_y = y;
    dog->roam_target_x = x;
    dog->vocal_timer = 0.8f + rng_range(rng, 150) * 0.01f;
    dog->anim_time = (float)rng_range(rng, 628) * 0.01f;
    dog->attack_timer = 0.0f;
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
}

static void enemy_update_climbing(Enemy *enemy, Level *level, float dt,
                                  bool pursuing, float target_x,
                                  float target_y, Rng *rng)
{
    bool following_target =
        pursuing && enemy->obstacle_avoid_timer <= 0.0f;

    /* Finish the chosen climb direction before making another routing
       decision. Re-evaluating it from target_y on every frame can reverse the
       enemy just after it passes the target height, trapping it in a short
       up/down loop when a side exit is not yet available. */

    /* Smoothly slide toward the ladder column centre instead of hard-snapping,
       to avoid a one-frame position jump when climbing starts. */
    float ladder_x =
        enemy->ladder_col * (float)TILE_SIZE +
        (TILE_SIZE - ENEMY_W) * 0.5f;
    float align_dx = ladder_x - enemy->x;

    /* Centre the whole collision box before moving vertically. Starting the
     * climb while part of the guard is still over the neighbouring floor tile
     * lets vertical collision pin it to that floor indefinitely. */
    if (fabsf(align_dx) > 0.5f)
    {
        enemy->vx = align_dx * 10.0f;
        enemy->vy = 0.0f;
        enemy->on_ground = false;
        level_move(level, &enemy->x, &enemy->y, &enemy->vx, &enemy->vy,
                   ENEMY_W, ENEMY_H, dt, true, &enemy->on_ground,
                   false);
        return;
    }
    enemy->x = ladder_x;
    enemy->vx = 0.0f;
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

    int target_row = (int)floorf(target_y / TILE_SIZE);
    float exit_y = (bot_row + 1) * (float)TILE_SIZE - ENEMY_H;
    bool reached_target_floor =
        following_target && target_row == bot_row &&
        fabsf(enemy->y - exit_y) <= ENEMY_CLIMB_SPEED * dt + 0.5f;
    bool leave_ladder =
        !ladder_ahead ||
        (floor_beside &&
         (following_target ? reached_target_floor
                           : rng_range(rng, 100) < 4));
    if (leave_ladder)
    {
        if (reached_target_floor)
            enemy->y = exit_y;
        enemy->climbing = false;
        enemy->vy = 0.0f;
        enemy->climb_cooldown = ENEMY_CLIMB_COOLDOWN;
        if (can_left && can_right)
        {
            if (following_target)
            {
                float enemy_center_x = enemy->x + ENEMY_W * 0.5f;
                enemy->dir = target_x < enemy_center_x ? -1 : 1;
            }
            else
            {
                enemy->dir = rng_range(rng, 2) == 0 ? -1 : 1;
            }
        }
        else if (can_left)
            enemy->dir = -1;
        else if (can_right)
            enemy->dir = 1;
        else
            enemy->dir = rng_range(rng, 2) == 0 ? -1 : 1;
    }

    /* Do not carry the grounded state from before the climb. In particular,
       an enemy leaving a ladder must land before the walking AI is allowed to
       activate that same ladder again. */
    enemy->on_ground = false;
    level_move(level, &enemy->x, &enemy->y, &enemy->vx, &enemy->vy,
               ENEMY_W, ENEMY_H, dt, enemy->climbing, &enemy->on_ground,
               false);
}

static bool enemy_box_tiles_clear(const Level *level, float x, float y,
                                  float w, float h)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + w - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + h - 1.0f) / TILE_SIZE);
    for (int r = top; r <= bottom; ++r)
        for (int c = left; c <= right; ++c)
            if (level_is_solid(level, c, r))
                return false;
    return true;
}

static bool enemy_floor_ahead(const Level *level, const Enemy *enemy, int dir)
{
    float probe_x = dir > 0 ? enemy->x + ENEMY_W + 3.0f : enemy->x - 3.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int row = (int)floorf((enemy->y + ENEMY_H + 2.0f) / TILE_SIZE);
    return level_is_solid(level, col, row) || level_is_ladder(level, col, row);
}

/* Is there a landing within jump range across a gap in the given direction? */
static bool enemy_can_jump_gap(const Level *level, const Enemy *enemy, int dir)
{
    int row = (int)floorf((enemy->y + ENEMY_H + 2.0f) / TILE_SIZE);
    int front = dir > 0
                    ? (int)floorf((enemy->x + ENEMY_W + 3.0f) / TILE_SIZE)
                    : (int)floorf((enemy->x - 3.0f) / TILE_SIZE);
    for (int gap = 1; gap <= ENEMY_JUMP_MAX_GAP_TILES; ++gap)
    {
        int col = front + dir * gap;
        if (!level_is_solid(level, col, row) &&
            !level_is_ladder(level, col, row))
            continue;
        float x = col * (float)TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
        if (enemy_box_tiles_clear(level, x, enemy->y, ENEMY_W, ENEMY_H) &&
            enemy_box_tiles_clear(level, x, enemy->y - TILE_SIZE * 0.8f,
                                  ENEMY_W, ENEMY_H))
            return true;
    }
    return false;
}

static void enemy_update_walking(Enemy *enemy, Level *level, float dt,
                                 bool pursuing, bool alarmed, float target_x,
                                 float target_y, bool hemmed_in, Rng *rng)
{
    bool following_target =
        pursuing && enemy->obstacle_avoid_timer <= 0.0f;

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
    if (alarmed)
        speed *= ENEMY_ALARM_SPEED_MULTIPLIER;

    float enemy_center_x = enemy->x + ENEMY_W * 0.5f;
    float enemy_center_y = enemy->y + ENEMY_H * 0.5f;
    float target_dx = target_x - enemy_center_x;
    bool target_on_same_floor =
        fabsf(target_y - enemy_center_y) <= TILE_SIZE * 0.75f;
    bool reached_target_x = false;

    /* Only follow the target's X position once the enemy is on its floor.
     * Otherwise, crossing that X position makes the direction flip every
     * frame and prevents the enemy from continuing along the platform to a
     * ladder. Keeping the current patrol direction lets the ladder logic
     * below find a route between floors. */
    if (following_target && !hemmed_in && target_on_same_floor)
    {
        if (fabsf(target_dx) > 2.0f)
            enemy->dir = target_dx < 0.0f ? -1 : 1;
        else
            reached_target_x = true;
    }

    enemy->vx = (hemmed_in || reached_target_x)
                    ? 0.0f
                    : (float)enemy->dir * speed;

    if (enemy->on_ground)
    {
        int center_col = (int)floorf((enemy->x + ENEMY_W * 0.5f) / TILE_SIZE);
        int center_row = (int)floorf((enemy->y + ENEMY_H * 0.5f) / TILE_SIZE);
        int foot_row = (int)floorf((enemy->y + ENEMY_H * 0.5f) / TILE_SIZE);

        /* Reverse at walls. */
        int ahead_col = (enemy->dir > 0)
                            ? (int)floorf((enemy->x + ENEMY_W + 1.0f) / TILE_SIZE)
                            : (int)floorf((enemy->x - 1.0f) / TILE_SIZE);
        if (!hemmed_in && enemy->vx != 0.0f &&
            level_is_solid(level, ahead_col, foot_row))
        {
            enemy->dir = -enemy->dir;
            enemy->vx = (float)enemy->dir * speed;
        }

        /* While chasing, hop a short gap rather than stalling at its edge,
         * but only when the target lies ahead and not below it (a lower target
         * is still reached by walking off the ledge, as before). */
        if (following_target && !hemmed_in && enemy->vx != 0.0f &&
            !enemy_floor_ahead(level, enemy, enemy->dir) &&
            target_dx * (float)enemy->dir > 0.0f &&
            target_y < enemy_center_y + TILE_SIZE * 0.75f &&
            enemy_can_jump_gap(level, enemy, enemy->dir))
        {
            enemy->vy = -ENEMY_JUMP_SPEED;
            if (fabsf(enemy->vx) < ENEMY_JUMP_MIN_SPEED)
                enemy->vx = (float)enemy->dir * ENEMY_JUMP_MIN_SPEED;
            enemy->on_ground = false;
        }

        /* Patrols use ladders occasionally. During an alarm, take a usable
         * ladder immediately when it leads toward the player's floor. */
        enemy->climb_cooldown -= dt;
        bool ladder_here = level_is_ladder(level, center_col, center_row);
        if (ladder_here &&
            (pursuing || enemy->climb_cooldown <= 0.0f))
        {
            bool up_ok = level_is_ladder(level, center_col, center_row - 1);
            bool down_ok = level_is_ladder(level, center_col, center_row + 1);
            float enemy_center_y = enemy->y + ENEMY_H * 0.5f;
            int desired_climb_dir =
                target_y < enemy_center_y ? -1 : 1;
            bool ladder_toward_target =
                desired_climb_dir < 0 ? up_ok : down_ok;
            bool start_climbing =
                following_target
                    ? (fabsf(target_y - enemy_center_y) >
                           TILE_SIZE * 0.75f &&
                       ladder_toward_target)
                    : ((up_ok || down_ok) &&
                       rng_range(rng, 100) < ENEMY_CLIMB_CHANCE);
            if (start_climbing)
            {
                enemy->climbing = true;
                enemy->on_ground = false;
                enemy->ladder_col = center_col;
                if (following_target)
                {
                    enemy->climb_dir = desired_climb_dir;
                }
                else if (up_ok && down_ok)
                {
                    enemy->climb_dir = rng_range(rng, 2) == 0 ? -1 : 1;
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

void enemy_update(Enemy *enemy, Level *level, float dt,
                  bool pursuing, bool alarmed,
                  float target_x, float target_y,
                  bool hemmed_in, Rng *rng)
{
    /* Conversation cooldown must advance in every AI state, including walking
     * and climbing, so enemies eventually become eligible to talk again. */
    if (enemy->talk_cooldown > 0.0f)
    {
        enemy->talk_cooldown -= dt;
        if (enemy->talk_cooldown < 0.0f)
            enemy->talk_cooldown = 0.0f;
    }

    if (enemy->obstacle_avoid_timer > 0.0f)
    {
        enemy->obstacle_avoid_timer -= dt;
        if (enemy->obstacle_avoid_timer < 0.0f)
            enemy->obstacle_avoid_timer = 0.0f;
    }

    if (enemy->recoil_timer > 0.0f)
    {
        enemy->recoil_timer -= dt;
        if (enemy->recoil_timer < 0.0f)
            enemy->recoil_timer = 0.0f;
    }

    if (enemy->climbing)
        enemy->anim_time += dt * (2.4f + fabsf(enemy->vy) * 0.022f);
    else if (fabsf(enemy->vx) > 1.0f && enemy->aim_timer <= 0.0f && !enemy->talking)
        enemy->anim_time += dt * (2.3f + fabsf(enemy->vx) * 0.030f);
    else
        enemy->anim_time += dt;

    if (enemy->talking && !pursuing)
    {
        enemy_update_talking(enemy, level, dt);
        return;
    }
    if (pursuing && enemy->talking)
    {
        enemy->talking = false;
        enemy->talk_timer = 0.0f;
        enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
    }
    if (enemy->climbing)
    {
        enemy_update_climbing(enemy, level, dt,
                              pursuing, target_x, target_y, rng);
    }
    else
    {
        enemy_update_walking(enemy, level, dt,
                             pursuing, alarmed, target_x, target_y,
                             hemmed_in, rng);
    }
}
