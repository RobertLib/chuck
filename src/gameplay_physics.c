#include "gameplay_physics.h"

#include "gameplay_world.h"

#include <math.h>

static bool crate_position_clear(const GameplayState *state, int crate_index,
                                 float x, float y)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + CRATE_W - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + CRATE_H - 1.0f) / TILE_SIZE);

    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col)
            if (level_is_solid(&state->level, col, row))
                return false;

    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        if (i == crate_index)
            continue;
        const Crate *other = &state->level.runtime.crates[i];
        if (other->active &&
            gameplay_boxes_overlap(x, y, CRATE_W, CRATE_H,
                                   other->x, other->y, CRATE_W, CRATE_H))
            return false;
    }
    return true;
}

static int crate_blocking_enemy(const GameplayState *state, float x, float y)
{
    for (int i = 0; i < state->enemy_count; ++i)
    {
        const Enemy *enemy = &state->enemies[i];
        if (!enemy->dead &&
            gameplay_boxes_overlap(x, y, CRATE_W, CRATE_H,
                                   enemy->x, enemy->y, ENEMY_W, ENEMY_H))
        {
            return i;
        }
    }
    return -1;
}

static float move_crate_x(GameplayState *state, int crate_index, float dx,
                          int *blocking_enemy)
{
    Crate *crate = &state->level.runtime.crates[crate_index];
    if (blocking_enemy != NULL)
        *blocking_enemy = -1;
    if (!crate->active || dx == 0.0f)
        return 0.0f;
    float moved = 0.0f;
    float remaining = fabsf(dx);
    float sign = dx > 0.0f ? 1.0f : -1.0f;
    while (remaining > 0.0f)
    {
        float step = fminf(remaining, 1.0f) * sign;
        int enemy_index =
            crate_blocking_enemy(state, crate->x + step, crate->y);
        if (!crate_position_clear(state, crate_index,
                                  crate->x + step, crate->y) ||
            enemy_index >= 0)
        {
            if (blocking_enemy != NULL)
                *blocking_enemy = enemy_index;
            break;
        }
        crate->x += step;
        moved += step;
        remaining -= fabsf(step);
    }
    return moved;
}

static float move_crate_y(GameplayState *state, int crate_index, float dy)
{
    Crate *crate = &state->level.runtime.crates[crate_index];
    if (!crate->active || dy == 0.0f)
        return 0.0f;
    float moved = 0.0f;
    float remaining = fabsf(dy);
    float sign = dy > 0.0f ? 1.0f : -1.0f;
    while (remaining > 0.0f)
    {
        float step = fminf(remaining, 1.0f) * sign;
        if (!crate_position_clear(state, crate_index,
                                  crate->x, crate->y + step))
            break;
        crate->y += step;
        moved += step;
        remaining -= fabsf(step);
    }
    return moved;
}

static bool resolve_falling_crate_hits(GameplayState *state,
                                       CampaignState *campaign,
                                       const Crate *crate, float previous_y)
{
    float previous_bottom = previous_y + CRATE_H;
    float current_bottom = crate->y + CRATE_H;
    if (current_bottom <= previous_bottom)
        return false;

    bool hit_something = false;

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (!enemy->dead &&
            crate->x < enemy->x + ENEMY_W &&
            crate->x + CRATE_W > enemy->x &&
            previous_bottom <= enemy->y && current_bottom >= enemy->y)
        {
            gameplay_kill_enemy_with_crate(state, campaign, enemy);
            hit_something = true;
        }
    }
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (!dog->dead &&
            crate->x < dog->x + DOG_W &&
            crate->x + CRATE_W > dog->x &&
            previous_bottom <= dog->y && current_bottom >= dog->y)
        {
            gameplay_kill_dog_with_crate(state, campaign, dog);
            hit_something = true;
        }
    }
    return hit_something;
}

void gameplay_update_crates(GameplayState *state, CampaignState *campaign,
                            float dt)
{
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        Crate *crate = &state->level.runtime.crates[i];
        if (!crate->active)
            continue;
        crate->vy += GRAVITY * dt;
        if (crate->vy > MAX_FALL_SPEED)
            crate->vy = MAX_FALL_SPEED;

        float desired_x = crate->vx * dt;
        int blocking_enemy = -1;
        float moved_x = move_crate_x(state, i, desired_x,
                                     &blocking_enemy);
        if (blocking_enemy >= 0 &&
            !state->enemies[blocking_enemy].provoked)
        {
            gameplay_provoke_enemy(state, blocking_enemy);
        }
        if (fabsf(moved_x - desired_x) > 0.01f)
            crate->vx = 0.0f;

        bool was_grounded = crate->on_ground;
        float impact_speed = crate->vy;
        crate->on_ground = false;
        float previous_y = crate->y;
        float desired_y = crate->vy * dt;
        float moved_y = move_crate_y(state, i, desired_y);
        bool crushed_hostile =
            resolve_falling_crate_hits(state, campaign, crate, previous_y);
        if (fabsf(moved_y - desired_y) > 0.01f)
        {
            if (desired_y > 0.0f)
                crate->on_ground = true;
            crate->vy = 0.0f;
        }

        if (crushed_hostile ||
            (!was_grounded && crate->on_ground &&
             impact_speed >= CRATE_LAND_SOUND_SPEED))
        {
            gameplay_world_sound(state, SFX_CRATE_LAND,
                                 crate->x + CRATE_W * 0.5f,
                                 crate->y + CRATE_H * 0.5f);
        }

        if (crate->on_ground && crate->vx != 0.0f)
        {
            float factor = 1.0f - CRATE_FRICTION * dt;
            if (factor < 0.0f)
                factor = 0.0f;
            crate->vx *= factor;
            if (fabsf(crate->vx) < 1.0f)
                crate->vx = 0.0f;
        }
    }
}

void gameplay_resolve_player_crates(GameplayState *state,
                                    float previous_x, float previous_y,
                                    float previous_height)
{
    Player *player = &state->player;
    float height = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        Crate *crate = &state->level.runtime.crates[i];
        if (!crate->active ||
            !gameplay_boxes_overlap(player->x, player->y, PLAYER_W, height,
                                   crate->x, crate->y, CRATE_W, CRATE_H))
            continue;

        if (previous_y + previous_height <= crate->y + 2.0f &&
            player->vy >= 0.0f)
        {
            player->y = crate->y - height;
            player->vy = 0.0f;
            player->on_ground = true;
            continue;
        }
        if (previous_y >= crate->y + CRATE_H - 2.0f && player->vy < 0.0f)
        {
            player->y = crate->y + CRATE_H;
            player->vy = 0.0f;
            continue;
        }

        int direction;
        if (previous_x + PLAYER_W <= crate->x + 2.0f)
            direction = 1;
        else if (previous_x >= crate->x + CRATE_W - 2.0f)
            direction = -1;
        else
            direction = player->x + PLAYER_W * 0.5f <
                                crate->x + CRATE_W * 0.5f
                            ? 1
                            : -1;

        if ((direction > 0 && player->vx > 0.0f) ||
            (direction < 0 && player->vx < 0.0f))
        {
            bool started_push = fabsf(crate->vx) < 1.0f;
            float penetration = direction > 0
                                    ? player->x + PLAYER_W - crate->x
                                    : crate->x + CRATE_W - player->x;
            int blocking_enemy = -1;
            float moved = move_crate_x(state, i,
                                       direction * (penetration + 0.5f),
                                       &blocking_enemy);
            if (blocking_enemy >= 0)
            {
                crate->vx = 0.0f;
                if (!state->enemies[blocking_enemy].provoked)
                    gameplay_provoke_enemy(state, blocking_enemy);
            }
            else if (fabsf(moved) > 0.0f)
            {
                crate->vx = (float)direction * CRATE_PUSH_SPEED;
                if (started_push)
                {
                    gameplay_world_sound(state, SFX_CRATE_PUSH,
                                         crate->x + CRATE_W * 0.5f,
                                         crate->y + CRATE_H * 0.5f);
                }
            }
        }

        if (gameplay_boxes_overlap(player->x, player->y, PLAYER_W, height,
                                   crate->x, crate->y, CRATE_W, CRATE_H))
        {
            player->x = direction > 0
                            ? crate->x - PLAYER_W
                            : crate->x + CRATE_W;
            player->vx = 0.0f;
        }
    }
}

void gameplay_resolve_enemy_crates(GameplayState *state, Enemy *enemy,
                                   float previous_y)
{
    if (enemy->dead)
        return;
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        Crate *crate = &state->level.runtime.crates[i];
        if (!crate->active ||
            !gameplay_boxes_overlap(enemy->x, enemy->y, ENEMY_W, ENEMY_H,
                                   crate->x, crate->y, CRATE_W, CRATE_H))
            continue;
        if (previous_y + ENEMY_H <= crate->y + 2.0f && enemy->vy >= 0.0f)
        {
            enemy->y = crate->y - ENEMY_H;
            enemy->vy = 0.0f;
            enemy->on_ground = true;
            enemy->mounting_crate = false;
            if (enemy->climbing)
            {
                enemy->climbing = false;
                enemy->climb_cooldown = ENEMY_CLIMB_COOLDOWN;
                enemy->obstacle_avoid_timer = ENEMY_OBSTACLE_AVOID_TIME;
            }
            continue;
        }
        /* Side overlap is intentional: guards are rendered after crates and
         * may take the foreground route instead of mounting every box. */
    }
}

void gameplay_resolve_dog_crates(GameplayState *state, Dog *dog,
                                 float previous_x, float previous_y)
{
    if (dog->dead)
        return;
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        Crate *crate = &state->level.runtime.crates[i];
        if (!crate->active ||
            !gameplay_boxes_overlap(dog->x, dog->y, DOG_W, DOG_H,
                                   crate->x, crate->y, CRATE_W, CRATE_H))
            continue;
        if (previous_y + DOG_H <= crate->y + 2.0f && dog->vy >= 0.0f)
        {
            dog->y = crate->y - DOG_H;
            dog->vy = 0.0f;
            dog->on_ground = true;
            continue;
        }
        if (previous_x + DOG_W * 0.5f < crate->x + CRATE_W * 0.5f)
        {
            dog->x = crate->x - DOG_W;
            dog->dir = -1;
        }
        else
        {
            dog->x = crate->x + CRATE_W;
            dog->dir = 1;
        }
        dog->vx = 0.0f;
    }
}

bool gameplay_crate_blocks_row(const GameplayState *state,
                               float ax, float bx, int row)
{
    float left = fminf(ax, bx);
    float width = fabsf(bx - ax);
    float y = row * (float)TILE_SIZE + TILE_SIZE * 0.5f;
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        const Crate *crate = &state->level.runtime.crates[i];
        if (crate->active &&
            gameplay_boxes_overlap(left, y, width, 1.0f,
                                   crate->x, crate->y, CRATE_W, CRATE_H))
            return true;
    }
    return false;
}

bool gameplay_box_tiles_clear(const GameplayState *state,
                              float x, float y, float w, float h)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + w - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + h - 1.0f) / TILE_SIZE);
    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col)
            if (level_is_solid(&state->level, col, row))
                return false;
    return true;
}

void gameplay_carry_player_on_elevator(GameplayState *state, float dt)
{
    if (state->player_on_elevator < 0 ||
        state->player_on_elevator >= state->level.runtime.elevator_count)
        return;

    const Elevator *elevator =
        &state->level.runtime.elevators[state->player_on_elevator];
    if (elevator->vy >= 0.0f)
        return;

    state->player.y += elevator->vy * dt;
    state->player.vy = 0.0f;
}

bool gameplay_resolve_player_crush(GameplayState *state)
{
    Player *player = &state->player;
    float height = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    int col_left = (int)floorf(player->x / TILE_SIZE);
    int col_right = (int)floorf((player->x + PLAYER_W - 1.0f) / TILE_SIZE);
    int row_top = (int)floorf(player->y / TILE_SIZE);

    bool slab_overhead = false;
    for (int col = col_left; col <= col_right && !slab_overhead; ++col)
        slab_overhead = level_is_solid(&state->level, col, row_top);
    if (!slab_overhead)
        return false;

    /* An elevator shaft is one tile wide and the box is 26 of those 32 pixels,
     * so a rider who stepped aboard off-centre keeps overlapping the
     * neighbouring column. The overlap costs nothing in the open storeys and
     * killed him the moment the lift carried his head into the slab the shaft
     * runs through: the wall arrived above him rather than beside him, so
     * horizontal collision never had the chance to push him clear. Do it here
     * instead. The box is narrower than a tile, so it straddles at most two
     * columns and there are at most two ways out — wholly into the left one or
     * wholly into the right one. */
    if (col_left != col_right)
    {
        float into_right = (float)col_right * TILE_SIZE;
        float into_left = (float)(col_left + 1) * TILE_SIZE - PLAYER_W;
        if (gameplay_box_tiles_clear(state, into_right, player->y,
                                     PLAYER_W, height))
        {
            player->x = into_right;
            return false;
        }
        if (gameplay_box_tiles_clear(state, into_left, player->y,
                                     PLAYER_W, height))
        {
            player->x = into_left;
            return false;
        }
    }

    gameplay_hit_player(state);
    return true;
}

void gameplay_ride_platforms(GameplayState *state, float dt)
{
    Player *player = &state->player;

    state->player_on_elevator = -1;
    for (int i = 0; i < state->level.runtime.elevator_count; ++i)
    {
        const Elevator *elevator = &state->level.runtime.elevators[i];
        float plat_x = elevator->col * (float)TILE_SIZE;
        float height =
            player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_cx = player->x + PLAYER_W * 0.5f;
        float player_feet = player->y + height;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            player->vy >= 0.0f &&
            player_feet >= elevator->y - 2.0f &&
            player_feet <= elevator->y + ELEVATOR_PLAT_H + 8.0f)
        {
            player->y = elevator->y - height;
            player->vy = 0.0f;
            player->on_ground = true;
            state->player_on_elevator = i;
        }
    }

    state->player_on_moving_platform = -1;
    for (int i = 0; i < state->level.runtime.moving_platform_count; ++i)
    {
        const MovingPlatform *platform =
            &state->level.runtime.moving_platforms[i];
        float plat_top = platform->row * (float)TILE_SIZE;
        float height =
            player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_cx = player->x + PLAYER_W * 0.5f;
        float player_feet = player->y + height;
        if (player_cx > platform->x && player_cx < platform->x + TILE_SIZE &&
            player->vy >= 0.0f &&
            player_feet >= plat_top - 2.0f &&
            player_feet <= plat_top + MOVING_PLATFORM_H + 8.0f)
        {
            player->y = plat_top - height;
            player->vy = 0.0f;
            player->on_ground = true;
            state->player_on_moving_platform = i;
            /* Carry the rider sideways with the platform. */
            player->x += platform->vx * dt;
            break;
        }
    }
}
