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

static float move_crate_x(GameplayState *state, int crate_index, float dx)
{
    Crate *crate = &state->level.runtime.crates[crate_index];
    if (!crate->active || dx == 0.0f)
        return 0.0f;
    float moved = 0.0f;
    float remaining = fabsf(dx);
    float sign = dx > 0.0f ? 1.0f : -1.0f;
    while (remaining > 0.0f)
    {
        float step = fminf(remaining, 1.0f) * sign;
        if (!crate_position_clear(state, crate_index,
                                  crate->x + step, crate->y))
            break;
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

static void resolve_falling_crate_hits(GameplayState *state,
                                       CampaignState *campaign,
                                       const Crate *crate, float previous_y)
{
    float previous_bottom = previous_y + CRATE_H;
    float current_bottom = crate->y + CRATE_H;
    if (current_bottom <= previous_bottom)
        return;

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (!enemy->dead &&
            crate->x < enemy->x + ENEMY_W &&
            crate->x + CRATE_W > enemy->x &&
            previous_bottom <= enemy->y && current_bottom >= enemy->y)
        {
            gameplay_kill_enemy_with_crate(state, campaign, enemy);
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
        }
    }
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
        float moved_x = move_crate_x(state, i, desired_x);
        if (fabsf(moved_x - desired_x) > 0.01f)
            crate->vx = 0.0f;

        crate->on_ground = false;
        float previous_y = crate->y;
        float desired_y = crate->vy * dt;
        float moved_y = move_crate_y(state, i, desired_y);
        resolve_falling_crate_hits(state, campaign, crate, previous_y);
        if (fabsf(moved_y - desired_y) > 0.01f)
        {
            if (desired_y > 0.0f)
                crate->on_ground = true;
            crate->vy = 0.0f;
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
            float penetration = direction > 0
                                    ? player->x + PLAYER_W - crate->x
                                    : crate->x + CRATE_W - player->x;
            float moved = move_crate_x(state, i,
                                       direction * (penetration + 0.5f));
            if (fabsf(moved) > 0.0f)
                crate->vx = (float)direction * CRATE_PUSH_SPEED;
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
                                   float previous_x, float previous_y)
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
            continue;
        }
        if (previous_x + ENEMY_W * 0.5f < crate->x + CRATE_W * 0.5f)
        {
            enemy->x = crate->x - ENEMY_W;
            enemy->dir = -1;
        }
        else
        {
            enemy->x = crate->x + CRATE_W;
            enemy->dir = 1;
        }
        enemy->vx = 0.0f;
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
