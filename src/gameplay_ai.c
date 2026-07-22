#include "gameplay_ai.h"

#include "gameplay_physics.h"
#include "gameplay_world.h"

#include <math.h>

#define ENEMY_SIDE_PROBE 4.0f

static int find_enemy_slot(GameplayState *state)
{
    for (int i = 0; i < state->enemy_count; ++i)
    {
        if (!state->enemies[i].dead)
            continue;
        for (int dog = 0; dog < state->dog_count; ++dog)
            if (!state->dogs[dog].dead && state->dogs[dog].owner == i)
                state->dogs[dog].owner = -1;
        return i;
    }
    return state->enemy_count < MAX_ENEMIES ? state->enemy_count++ : -1;
}

static int find_dog_slot(GameplayState *state)
{
    for (int i = 0; i < state->dog_count; ++i)
        if (state->dogs[i].dead)
            return i;
    return state->dog_count < MAX_DOGS ? state->dog_count++ : -1;
}

static bool crate_or_enemy_blocks_side(const GameplayState *state,
                                       int enemy_index, int direction)
{
    const Enemy *enemy = &state->enemies[enemy_index];
    float x = direction < 0
                  ? enemy->x - ENEMY_SIDE_PROBE
                  : enemy->x + ENEMY_W;
    float y = enemy->y + 1.0f;
    float height = ENEMY_H - 2.0f;
    if (!gameplay_box_tiles_clear(state, x, y, ENEMY_SIDE_PROBE, height))
        return true;
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        const Crate *crate = &state->level.runtime.crates[i];
        if (crate->active &&
            gameplay_boxes_overlap(x, y, ENEMY_SIDE_PROBE, height,
                                   crate->x, crate->y, CRATE_W, CRATE_H))
            return true;
    }
    for (int i = 0; i < state->dog_count; ++i)
    {
        const Dog *dog = &state->dogs[i];
        if (!dog->dead &&
            gameplay_boxes_overlap(x, y, ENEMY_SIDE_PROBE, height,
                                   dog->x, dog->y, DOG_W, DOG_H))
            return true;
    }
    for (int i = 0; i < state->enemy_count; ++i)
    {
        const Enemy *other = &state->enemies[i];
        if (i != enemy_index && !other->dead &&
            gameplay_boxes_overlap(x, y, ENEMY_SIDE_PROBE, height,
                                   other->x, other->y, ENEMY_W, ENEMY_H))
            return true;
    }
    return false;
}

static bool horizontal_los_clear(const GameplayState *state,
                                 float ax, float bx, float y)
{
    int row = (int)floorf(y / TILE_SIZE);
    int first = (int)floorf(fminf(ax, bx) / TILE_SIZE);
    int last = (int)floorf(fmaxf(ax, bx) / TILE_SIZE);
    for (int col = first; col <= last; ++col)
        if (level_is_solid(&state->level, col, row))
            return false;
    return !gameplay_crate_blocks_row(state, ax, bx, row);
}

static bool enemy_has_los(const GameplayState *state, const Enemy *enemy)
{
    float enemy_x = enemy->x + ENEMY_W * 0.5f;
    float enemy_y = enemy->y + ENEMY_H * 0.5f;
    float player_x = state->player.x + PLAYER_W * 0.5f;
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    float player_y = state->player.y + player_h * 0.5f;
    if (enemy->talking &&
        (fabsf(player_x - enemy_x) > ENEMY_TALK_NOTICE_RADIUS ||
         fabsf(player_y - enemy_y) > ENEMY_TALK_NOTICE_RADIUS))
        return false;
    if (fabsf(player_x - enemy_x) > ENEMY_SHOOT_RANGE ||
        fabsf(player_y - enemy_y) > TILE_SIZE * 1.2f)
        return false;
    float dx = player_x - enemy_x;
    if ((dx > 0.0f && enemy->dir < 0) ||
        (dx < 0.0f && enemy->dir > 0))
        return false;
    return horizontal_los_clear(state, enemy_x, player_x, enemy_y);
}

static bool terminal_target(const GameplayState *state,
                            float *target_x, float *target_y)
{
    int index = state->level.runtime.active_terminal_index;
    if (index < 0 || index >= state->level.map.terminal_count)
        return false;
    const Terminal *terminal = &state->level.map.terminals[index];
    *target_x = (terminal->col + 0.5f) * TILE_SIZE;
    *target_y = (terminal->row + 0.5f) * TILE_SIZE;
    return true;
}

static void spawn_dog_for_enemy(GameplayState *state, int enemy_index)
{
    int slot = find_dog_slot(state);
    if (slot < 0 || enemy_index < 0 || enemy_index >= state->enemy_count)
        return;
    const Enemy *handler = &state->enemies[enemy_index];
    float base_x = handler->x + ENEMY_W * 0.5f - DOG_W * 0.5f;
    float y = handler->y + ENEMY_H - DOG_H;
    float x = base_x - handler->dir * DOG_HANDLER_DISTANCE;
    if (!gameplay_box_tiles_clear(state, x, y, DOG_W, DOG_H))
        x = base_x + handler->dir * DOG_HANDLER_DISTANCE;
    if (!gameplay_box_tiles_clear(state, x, y, DOG_W, DOG_H))
        x = base_x;
    dog_init(&state->dogs[slot], x, y, enemy_index, &state->rng);
}

static void janitor_set_activity(Janitor *janitor,
                                  JanitorActivity activity, Rng *rng)
{
    janitor->activity = activity;
    janitor->vx = 0.0f;
    switch (activity)
    {
    case JANITOR_WALK:
        janitor->activity_timer =
            3.5f + (float)rng_range(rng, 351) * 0.01f;
        break;
    case JANITOR_MOP:
        janitor->activity_timer =
            2.8f + (float)rng_range(rng, 241) * 0.01f;
        janitor->wet_timer = 0.0f;
        break;
    case JANITOR_PAUSE:
        janitor->activity_timer =
            0.6f + (float)rng_range(rng, 91) * 0.01f;
        break;
    }
}

static void janitor_init(Janitor *janitor, float x, float y,
                         bool starts_mopping, Rng *rng)
{
    *janitor = (Janitor){0};
    janitor->x = x;
    janitor->y = y;
    janitor->dir = rng_range(rng, 2) == 0 ? -1 : 1;
    janitor->anim_time = (float)rng_range(rng, 628) * 0.01f;
    janitor_set_activity(janitor,
                          starts_mopping ? JANITOR_MOP : JANITOR_WALK, rng);
}

static bool janitor_has_floor_ahead(const GameplayState *state,
                                    const Janitor *janitor)
{
    float probe_x = janitor->dir > 0
                        ? janitor->x + JANITOR_W + 7.0f
                        : janitor->x - 7.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int row = (int)floorf((janitor->y + JANITOR_H + 3.0f) / TILE_SIZE);
    return level_is_solid(&state->level, col, row) ||
           level_is_ladder(&state->level, col, row);
}

static bool janitor_side_blocked(const GameplayState *state,
                                 const Janitor *janitor)
{
    float probe_x = janitor->dir > 0
                        ? janitor->x + JANITOR_W + 2.0f
                        : janitor->x - 2.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int top = (int)floorf((janitor->y + 1.0f) / TILE_SIZE);
    int bottom = (int)floorf((janitor->y + JANITOR_H - 1.0f) / TILE_SIZE);
    for (int row = top; row <= bottom; ++row)
    {
        TileType tile = level_tile(&state->level, col, row);
        if (tile == TILE_WALL || tile == TILE_DOOR)
            return true;
    }
    return false;
}

static void janitor_leave_wet_spot(Janitor *janitor)
{
    JanitorWetSpot *spot =
        &janitor->wet_spots[janitor->next_wet_spot];
    float sweep = sinf(janitor->anim_time * 4.5f) * 9.0f;
    spot->x = janitor->x + JANITOR_W * 0.5f +
              janitor->dir * (16.0f + sweep);
    spot->y = janitor->y + JANITOR_H - 1.0f;
    spot->life = JANITOR_WET_LIFETIME;
    spot->active = true;
    janitor->next_wet_spot =
        (janitor->next_wet_spot + 1) % JANITOR_WET_SPOTS;
}

static void update_janitor(GameplayState *state, Janitor *janitor, float dt)
{
    for (int i = 0; i < JANITOR_WET_SPOTS; ++i)
    {
        JanitorWetSpot *spot = &janitor->wet_spots[i];
        if (!spot->active)
            continue;
        spot->life -= dt;
        if (spot->life <= 0.0f)
        {
            spot->life = 0.0f;
            spot->active = false;
        }
    }

    janitor->activity_timer -= dt;
    if (janitor->activity_timer <= 0.0f)
    {
        if (janitor->activity == JANITOR_WALK)
            janitor_set_activity(janitor, JANITOR_MOP, &state->rng);
        else if (janitor->activity == JANITOR_MOP)
            janitor_set_activity(janitor, JANITOR_PAUSE, &state->rng);
        else
        {
            if (rng_range(&state->rng, 100) < 35)
                janitor->dir = -janitor->dir;
            janitor_set_activity(janitor, JANITOR_WALK, &state->rng);
        }
    }

    if (janitor->activity == JANITOR_WALK)
    {
        if (janitor->on_ground &&
            (!janitor_has_floor_ahead(state, janitor) ||
             janitor_side_blocked(state, janitor)))
        {
            janitor->dir = -janitor->dir;
            janitor_set_activity(janitor, JANITOR_PAUSE, &state->rng);
        }
        else
            janitor->vx = janitor->dir * JANITOR_WALK_SPEED;
        janitor->anim_time += dt * 2.2f;
    }
    else if (janitor->activity == JANITOR_MOP)
    {
        janitor->vx = 0.0f;
        janitor->anim_time += dt * 2.8f;
        janitor->wet_timer -= dt;
        if (janitor->wet_timer <= 0.0f)
        {
            janitor_leave_wet_spot(janitor);
            janitor->wet_timer = 0.34f;
        }
    }
    else
    {
        janitor->vx = 0.0f;
        janitor->anim_time += dt * 0.8f;
    }

    janitor->vy += GRAVITY * dt;
    if (janitor->vy > MAX_FALL_SPEED)
        janitor->vy = MAX_FALL_SPEED;
    float intended_vx = janitor->vx;
    level_move(&state->level, &janitor->x, &janitor->y,
               &janitor->vx, &janitor->vy, JANITOR_W, JANITOR_H,
               dt, false, &janitor->on_ground, false);
    if (janitor->activity == JANITOR_WALK && intended_vx != 0.0f &&
        janitor->vx == 0.0f)
    {
        janitor->dir = -janitor->dir;
        janitor_set_activity(janitor, JANITOR_PAUSE, &state->rng);
    }
}

static bool spawn_enemy_from_door(GameplayState *state, int door_index)
{
    if (door_index < 0 || door_index >= state->level.map.door_count)
        return false;
    const Door *door = &state->level.map.doors[door_index];
    float door_x = door->col * (float)TILE_SIZE;
    float door_y = door->row * (float)TILE_SIZE;
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    if (gameplay_boxes_overlap(state->player.x, state->player.y,
                               PLAYER_W, player_h,
                               door_x, door_y, TILE_SIZE, TILE_SIZE))
        return false;
    float x = door_x + (TILE_SIZE - ENEMY_W) * 0.5f;
    float y = (door->row + 1) * (float)TILE_SIZE - ENEMY_H;
    for (int i = 0; i < state->enemy_count; ++i)
        if (!state->enemies[i].dead &&
            gameplay_boxes_overlap(x, y, ENEMY_W, ENEMY_H,
                                   state->enemies[i].x, state->enemies[i].y,
                                   ENEMY_W, ENEMY_H))
            return false;
    int slot = find_enemy_slot(state);
    if (slot < 0)
        return false;
    enemy_init(&state->enemies[slot], x, y, &state->rng);
    if (rng_range(&state->rng, 100) < DOG_DOOR_HANDLER_CHANCE)
        spawn_dog_for_enemy(state, slot);
    gameplay_world_sound(state, SFX_DOOR,
                         x + ENEMY_W * 0.5f, y + ENEMY_H * 0.5f);
    return true;
}

void gameplay_ai_spawn_level_entities(GameplayState *state)
{
    state->enemy_count = state->level.map.enemy_count;
    state->dog_count = 0;
    for (int i = 0; i < state->enemy_count; ++i)
    {
        enemy_init(&state->enemies[i],
                   state->level.map.enemy_spawns[i].x,
                   state->level.map.enemy_spawns[i].y, &state->rng);
        if (state->level.map.enemy_spawns[i].has_dog)
            spawn_dog_for_enemy(state, i);
    }
    state->janitor_count = state->level.map.janitor_count;
    for (int i = 0; i < state->janitor_count; ++i)
        janitor_init(&state->janitors[i],
                     state->level.map.janitor_spawns[i].x,
                     state->level.map.janitor_spawns[i].y,
                     (i & 1) == 0, &state->rng);
    state->mine_count = state->level.map.mine_count;
    for (int i = 0; i < state->mine_count; ++i)
    {
        state->mines[i].x = state->level.map.mine_spawns[i].x;
        state->mines[i].y = state->level.map.mine_spawns[i].y;
        state->mines[i].active = true;
        state->mines[i].triggered = false;
        state->mines[i].timer = 0.0f;
    }
}

void gameplay_ai_update_spawns(GameplayState *state, float dt)
{
    if (state->terminal_alarm_timer > 0.0f &&
        state->terminal_reinforcements_pending > 0 &&
        state->level.map.door_count > 0)
    {
        state->terminal_reinforcement_timer -= dt;
        if (state->terminal_reinforcement_timer <= 0.0f)
        {
            int first = rng_range(&state->rng, state->level.map.door_count);
            bool spawned = false;
            for (int offset = 0; offset < state->level.map.door_count; ++offset)
                if (spawn_enemy_from_door(
                        state, (first + offset) % state->level.map.door_count))
                {
                    spawned = true;
                    break;
                }
            if (!spawned)
                state->terminal_reinforcement_timer =
                    0.35f + rng_range(&state->rng, 46) * 0.01f;
            else if (--state->terminal_reinforcements_pending > 0)
                state->terminal_reinforcement_timer =
                    TERMINAL_REINFORCEMENT_GAP_MIN +
                    (TERMINAL_REINFORCEMENT_GAP_MAX -
                     TERMINAL_REINFORCEMENT_GAP_MIN) * rng_unit(&state->rng);
            else
                state->terminal_reinforcement_timer = 0.0f;
        }
    }

    for (int door = 0; door < state->level.map.door_count; ++door)
    {
        if (state->door_spawns[door] <= 0)
            continue;
        state->door_timers[door] -= dt;
        if (state->door_timers[door] > 0.0f)
            continue;
        if (spawn_enemy_from_door(state, door))
        {
            state->door_spawns[door]--;
            state->door_timers[door] =
                DOOR_SPAWN_INTERVAL *
                (0.8f + rng_range(&state->rng, 40) * 0.01f);
        }
        else
            state->door_timers[door] = 0.5f;
    }
}

static bool dog_has_floor_ahead(const GameplayState *state,
                                const Dog *dog, int direction)
{
    float probe_x = direction > 0 ? dog->x + DOG_W + 3.0f : dog->x - 3.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int row = (int)floorf((dog->y + DOG_H + 2.0f) / TILE_SIZE);
    if (level_is_solid(&state->level, col, row) ||
        level_is_ladder(&state->level, col, row))
        return true;
    for (int i = 0; i < state->level.runtime.fall_platform_count; ++i)
    {
        const FallPlatform *platform = &state->level.runtime.fall_platforms[i];
        if (!platform->removed && platform->col == col &&
            fabsf(platform->y - row * (float)TILE_SIZE) < 3.0f)
            return true;
    }
    for (int i = 0; i < state->level.runtime.moving_platform_count; ++i)
    {
        const MovingPlatform *platform = &state->level.runtime.moving_platforms[i];
        if (platform->row == row &&
            (int)floorf(platform->x / TILE_SIZE) == col)
            return true;
    }
    return false;
}

static bool dog_can_jump_gap(const GameplayState *state,
                             const Dog *dog, int direction)
{
    if (!dog->on_ground)
        return false;
    int row = (int)floorf((dog->y + DOG_H + 2.0f) / TILE_SIZE);
    int front = direction > 0
                    ? (int)floorf((dog->x + DOG_W + 3.0f) / TILE_SIZE)
                    : (int)floorf((dog->x - 3.0f) / TILE_SIZE);
    for (int gap = 1; gap <= DOG_JUMP_MAX_GAP_TILES; ++gap)
    {
        int col = front + direction * gap;
        if (!level_is_solid(&state->level, col, row) &&
            !level_is_ladder(&state->level, col, row))
            continue;
        float x = col * (float)TILE_SIZE + (TILE_SIZE - DOG_W) * 0.5f;
        if (gameplay_box_tiles_clear(state, x, dog->y, DOG_W, DOG_H) &&
            gameplay_box_tiles_clear(state, x,
                                     dog->y - TILE_SIZE * 0.8f,
                                     DOG_W, DOG_H))
            return true;
    }
    return false;
}

static bool dog_sees_player(const GameplayState *state, const Dog *dog)
{
    float dog_x = dog->x + DOG_W * 0.5f;
    float dog_y = dog->y + DOG_H * 0.5f;
    float player_x = state->player.x + PLAYER_W * 0.5f;
    float player_y = state->player.y +
                     (state->player.crawling
                          ? PLAYER_CRAWL_H * 0.5f
                          : PLAYER_H * 0.5f);
    float dx = player_x - dog_x;
    if (fabsf(player_y - dog_y) > TILE_SIZE * 0.9f ||
        fabsf(dx) > DOG_VIEW_RANGE)
        return false;
    if (((dx > 0.0f && dog->dir < 0) ||
         (dx < 0.0f && dog->dir > 0)) &&
        fabsf(dx) > DOG_BACK_SENSE_RANGE)
        return false;
    return horizontal_los_clear(state, dog_x, player_x, dog_y);
}

static float dog_anchor_x(const Dog *dog, const Enemy *handler)
{
    if (handler == NULL)
        return dog->guard_x;
    return handler->x + ENEMY_W * 0.5f - DOG_W * 0.5f -
           handler->dir * DOG_HANDLER_DISTANCE;
}

static void dog_pick_roam_target(GameplayState *state, Dog *dog,
                                 float anchor_x)
{
    float offset = rng_range(&state->rng, (int)(DOG_ROAM_RADIUS * 2.0f)) -
                   DOG_ROAM_RADIUS;
    if (fabsf(offset) < DOG_HANDLER_DISTANCE)
        offset = offset < 0.0f ? -DOG_HANDLER_DISTANCE : DOG_HANDLER_DISTANCE;
    dog->roam_target_x = anchor_x + offset;
    dog->state_timer = 0.8f + rng_range(&state->rng, 120) * 0.01f;
}

static void update_dog(GameplayState *state, Dog *dog, float dt)
{
    dog->vocal_timer -= dt;
    if (dog->attack_timer > 0.0f)
    {
        dog->attack_timer -= dt;
        if (dog->attack_timer < 0.0f)
            dog->attack_timer = 0.0f;
    }
    dog->anim_time += dt * (2.0f + fabsf(dog->vx) * 0.032f);

    Enemy *handler = NULL;
    if (dog->owner >= 0 && dog->owner < state->enemy_count)
    {
        handler = &state->enemies[dog->owner];
        if (handler->dead)
        {
            dog->owner = -1;
            handler = NULL;
        }
    }
    if (handler != NULL)
    {
        dog->guard_x = dog_anchor_x(dog, handler);
        dog->guard_y = handler->y + ENEMY_H - DOG_H;
    }
    if (dog->bite_cooldown > 0.0f)
    {
        dog->bite_cooldown -= dt;
        if (dog->bite_cooldown < 0.0f)
            dog->bite_cooldown = 0.0f;
    }

    if (dog_sees_player(state, dog))
    {
        dog->chase_target_x = state->player.x + PLAYER_W * 0.5f;
        dog->has_chase_target = true;
        dog->state = DOG_CHASE;
        dog->lost_timer = DOG_LOST_TIME;
    }
    else if (state->terminal_alarm_timer > 0.0f)
    {
        if (!dog->has_chase_target)
        {
            float ignored_y;
            dog->has_chase_target =
                terminal_target(state, &dog->chase_target_x, &ignored_y);
        }
        if (dog->has_chase_target)
        {
            dog->state = DOG_CHASE;
            dog->lost_timer = DOG_LOST_TIME;
        }
    }
    else if (dog->state == DOG_CHASE)
    {
        dog->lost_timer -= dt;
        if (dog->lost_timer <= 0.0f)
        {
            dog->state = DOG_RETURN;
            dog->has_chase_target = false;
        }
    }
    else
        dog->has_chase_target = false;

    dog->vy += GRAVITY * dt;
    if (dog->vy > MAX_FALL_SPEED)
        dog->vy = MAX_FALL_SPEED;

    float speed = DOG_PATROL_SPEED;
    float target_x = dog->guard_x;
    bool wants_move = false;
    if (dog->state == DOG_CHASE)
    {
        target_x = dog->chase_target_x - DOG_W * 0.5f;
        speed = DOG_CHASE_SPEED;
        wants_move = fabsf(target_x - dog->x) > DOG_BITE_RANGE * 0.6f;
    }
    else
    {
        float anchor = dog->guard_x;
        if (fabsf(dog->x - anchor) > DOG_RETURN_RADIUS)
            dog->state = DOG_RETURN;
        if (dog->state == DOG_RETURN)
        {
            target_x = anchor;
            speed = DOG_RETURN_SPEED;
            wants_move = fabsf(target_x - dog->x) > 10.0f;
            if (!wants_move)
            {
                dog->state = DOG_GUARD;
                dog->state_timer =
                    0.6f + rng_range(&state->rng, 120) * 0.01f;
            }
        }
        else if (dog->state == DOG_ROAM)
        {
            target_x = dog->roam_target_x;
            wants_move = fabsf(target_x - dog->x) > 8.0f;
            dog->state_timer -= dt;
            if (!wants_move || dog->state_timer <= 0.0f)
                dog->state = DOG_RETURN;
        }
        else
        {
            dog->state_timer -= dt;
            wants_move = fabsf(anchor - dog->x) > 18.0f;
            if (wants_move)
            {
                target_x = anchor;
                speed = DOG_RETURN_SPEED;
            }
            else if (dog->state_timer <= 0.0f &&
                     rng_range(&state->rng, 100) < 45)
            {
                dog->state = DOG_ROAM;
                dog_pick_roam_target(state, dog, anchor);
                target_x = dog->roam_target_x;
                wants_move = true;
            }
            else if (dog->state_timer <= 0.0f)
                dog->state_timer =
                    0.5f + rng_range(&state->rng, 120) * 0.01f;
        }
    }

    dog->vx = 0.0f;
    if (wants_move)
    {
        int direction = target_x > dog->x ? 1 : -1;
        dog->dir = direction;
        if (!dog->on_ground || dog_has_floor_ahead(state, dog, direction))
            dog->vx = direction * speed;
        else if (dog_can_jump_gap(state, dog, direction))
        {
            dog->vx = direction * fmaxf(speed, DOG_JUMP_MIN_SPEED);
            dog->vy = -DOG_JUMP_SPEED;
            dog->on_ground = false;
        }
        else
        {
            dog->dir = -direction;
            dog->state = DOG_ROAM;
            dog->roam_target_x =
                dog->x - direction * (DOG_HANDLER_DISTANCE + 24.0f);
            dog->state_timer =
                0.7f + rng_range(&state->rng, 90) * 0.01f;
        }
    }
    float previous_vx = dog->vx;
    level_move(&state->level, &dog->x, &dog->y, &dog->vx, &dog->vy,
               DOG_W, DOG_H, dt, false, &dog->on_ground, true);
    if (fabsf(previous_vx) > 0.0f && fabsf(dog->vx) < 0.1f &&
        dog->state != DOG_CHASE)
        dog->state = DOG_RETURN;
}

static void update_conversations(GameplayState *state)
{
    if (state->terminal_alarm_timer <= 0.0f)
    {
        for (int i = 0; i < state->enemy_count; ++i)
        {
            Enemy *first = &state->enemies[i];
            if (first->dead || first->provoked || first->talking ||
                first->climbing || !first->on_ground ||
                first->talk_partner != -1 || first->talk_cooldown > 0.0f)
                continue;
            for (int j = i + 1; j < state->enemy_count; ++j)
            {
                Enemy *second = &state->enemies[j];
                if (second->dead || second->provoked || second->talking ||
                    second->climbing || !second->on_ground ||
                    second->talk_partner != -1 ||
                    second->talk_cooldown > 0.0f ||
                    fabsf(first->y - second->y) > TILE_SIZE * 0.5f)
                    continue;
                float first_x = first->x + ENEMY_W * 0.5f;
                float second_x = second->x + ENEMY_W * 0.5f;
                if (fabsf(first_x - second_x) > ENEMY_W + 16.0f ||
                    rng_range(&state->rng, 100) >= ENEMY_TALK_CHANCE)
                    continue;

                Enemy *left = first->x < second->x ? first : second;
                Enemy *right = first->x < second->x ? second : first;
                float overlap = left->x + ENEMY_W - right->x;
                float shift = overlap > 0.0f ? (overlap + 6.0f) * 0.5f : 0.0f;
                float left_x = left->x - shift;
                float right_x = right->x + shift;
                if (!gameplay_box_tiles_clear(state, left_x, left->y,
                                              ENEMY_W, ENEMY_H) ||
                    !gameplay_box_tiles_clear(state, right_x, right->y,
                                              ENEMY_W, ENEMY_H))
                    continue;

                left->x = left_x;
                right->x = right_x;
                left->vx = right->vx = 0.0f;
                left->aim_timer = right->aim_timer = 0.0f;
                left->talking = right->talking = true;
                left->talk_timer = right->talk_timer = ENEMY_TALK_DURATION;
                left->talk_partner = (int)(right - state->enemies);
                right->talk_partner = (int)(left - state->enemies);
                left->dir = 1;
                right->dir = -1;
                gameplay_world_sound(state, SFX_GUARD_TALK,
                                     (left->x + right->x) * 0.5f +
                                         ENEMY_W * 0.5f,
                                     (left->y + right->y) * 0.5f +
                                         ENEMY_H * 0.5f);
                break;
            }
        }
    }

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        int partner_index = enemy->talk_partner;
        if (partner_index < 0)
            continue;
        if (partner_index >= state->enemy_count)
        {
            enemy->talk_partner = -1;
            continue;
        }
        Enemy *partner = &state->enemies[partner_index];
        if (enemy->talking && partner->talking)
            continue;
        if (partner->talk_partner == i)
        {
            partner->talk_partner = -1;
            partner->talking = false;
            partner->talk_timer = 0.0f;
            partner->talk_cooldown = ENEMY_TALK_COOLDOWN;
        }
        enemy->talk_partner = -1;
        enemy->talking = false;
        enemy->talk_timer = 0.0f;
        enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
    }
}

void gameplay_ai_update_movement(GameplayState *state, float dt)
{
    for (int i = 0; i < state->janitor_count; ++i)
        update_janitor(state, &state->janitors[i], dt);

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead)
            continue;
        float previous_x = enemy->x;
        float previous_y = enemy->y;
        bool terminal_pursuit = state->terminal_alarm_timer > 0.0f;
        bool pursuing = terminal_pursuit || enemy->provoked;
        if (pursuing && enemy_has_los(state, enemy))
        {
            float height = state->player.crawling
                               ? (float)PLAYER_CRAWL_H
                               : (float)PLAYER_H;
            enemy->pursuit_target_x = state->player.x + PLAYER_W * 0.5f;
            enemy->pursuit_target_y = state->player.y + height * 0.5f;
            enemy->has_pursuit_target = true;
        }
        else if (terminal_pursuit && !enemy->has_pursuit_target)
            enemy->has_pursuit_target =
                terminal_target(state, &enemy->pursuit_target_x,
                                &enemy->pursuit_target_y);
        else if (!pursuing)
            enemy->has_pursuit_target = false;

        pursuing = pursuing && enemy->has_pursuit_target;
        bool hemmed_in =
            crate_or_enemy_blocks_side(state, i, -1) &&
            crate_or_enemy_blocks_side(state, i, 1);
        enemy_update(enemy, &state->level, dt, pursuing,
                     enemy->pursuit_target_x, enemy->pursuit_target_y,
                     hemmed_in, &state->rng);
        gameplay_resolve_enemy_crates(state, enemy, previous_x, previous_y);
    }

    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (dog->dead)
            continue;
        float previous_x = dog->x;
        float previous_y = dog->y;
        DogState previous_state = dog->state;
        update_dog(state, dog, dt);
        gameplay_resolve_dog_crates(state, dog, previous_x, previous_y);
        if (previous_state != DOG_CHASE && dog->state == DOG_CHASE)
        {
            SoundEffect bark = rng_range(&state->rng, 2) == 0
                                   ? SFX_DOG_BARK
                                   : SFX_DOG_BARK_ALT;
            gameplay_world_sound(state, bark,
                                 dog->x + DOG_W * 0.5f,
                                 dog->y + DOG_H * 0.5f);
            dog->vocal_timer = 1.0f + rng_range(&state->rng, 100) * 0.01f;
        }
        else if (dog->state == DOG_CHASE && dog->vocal_timer <= 0.0f)
        {
            float x = dog->x + DOG_W * 0.5f;
            float y = dog->y + DOG_H * 0.5f;
            float distance = fabsf(state->player.x + PLAYER_W * 0.5f - x);
            SoundEffect sound =
                distance < 3.0f * TILE_SIZE &&
                        rng_range(&state->rng, 3) == 0
                    ? SFX_DOG_GROWL
                    : (rng_range(&state->rng, 2) == 0
                           ? SFX_DOG_BARK
                           : SFX_DOG_BARK_ALT);
            gameplay_world_sound(state, sound, x, y);
            dog->vocal_timer = 1.7f + rng_range(&state->rng, 180) * 0.01f;
        }
    }
    update_conversations(state);
}

static void update_enemy_reactions(GameplayState *state)
{
    if (state->player.crawling)
        return;
    float player_x = state->player.x + PLAYER_W * 0.5f;
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead || enemy->climbing)
            continue;
        float enemy_x = enemy->x + ENEMY_W * 0.5f;
        float dx = player_x - enemy_x;
        if (enemy->talking)
        {
            if (fabsf(dx) > ENEMY_TALK_NOTICE_RADIUS)
                continue;
            enemy->talking = false;
            enemy->talk_timer = 0.0f;
            if (enemy->talk_cooldown <= 0.0f)
                enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
        }
        if (dx * enemy->dir >= 0.0f ||
            fabsf(dx) > ENEMY_RETALIATE_RADIUS)
            continue;
        float enemy_y = enemy->y + ENEMY_H * 0.5f;
        float player_y = state->player.y + PLAYER_H * 0.5f;
        if (fabsf(player_y - enemy_y) > TILE_SIZE * 1.2f ||
            !horizontal_los_clear(state, enemy_x, player_x, enemy_y))
            continue;
        if (rng_range(&state->rng, 100) < ENEMY_RETALIATE_CHANCE)
        {
            enemy->dir = dx > 0.0f ? 1 : -1;
            enemy->aim_target_x = player_x;
            enemy->aim_target_y = state->player.y + PLAYER_H * 0.15f;
            enemy->aim_timer = ENEMY_AIM_TIME;
            gameplay_world_sound(state, SFX_ENEMY_ALERT,
                                 enemy_x, enemy_y);
        }
    }
}

static void fire_enemy_bullet(GameplayState *state, Enemy *enemy)
{
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        Bullet *bullet = &state->enemy_bullets[i];
        if (bullet->active)
            continue;
        float enemy_x = enemy->x + ENEMY_W * 0.5f;
        int direction = fabsf(enemy->aim_target_x - enemy_x) < 0.001f
                            ? (enemy->dir >= 0 ? 1 : -1)
                            : (enemy->aim_target_x > enemy_x ? 1 : -1);
        float shot_y = enemy->aim_target_y;
        float minimum = enemy->y + ENEMY_H * ENEMY_MUZZLE_MIN_Y_FACTOR;
        float maximum = enemy->y + ENEMY_H * ENEMY_MUZZLE_MAX_Y_FACTOR;
        if (shot_y < minimum)
            shot_y = minimum;
        if (shot_y > maximum)
            shot_y = maximum;
        bullet->vx = direction * ENEMY_BULLET_SPEED;
        bullet->vy = 0.0f;
        bullet->x = direction > 0
                        ? enemy->x + ENEMY_W
                        : enemy->x - BULLET_W;
        bullet->y = shot_y - BULLET_H * 0.5f;
        bullet->active = true;
        enemy->recoil_timer = 0.14f;
        gameplay_world_sound(state, SFX_ENEMY_SHOT,
                             enemy_x, enemy->y + ENEMY_H * 0.5f);
        return;
    }
}

void gameplay_ai_update_combat(GameplayState *state, float dt)
{
    update_enemy_reactions(state);
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead || enemy->climbing)
            continue;
        if (enemy->aim_timer > 0.0f)
        {
            enemy->aim_timer -= dt;
            if (enemy->aim_timer <= 0.0f)
            {
                enemy->aim_timer = 0.0f;
                fire_enemy_bullet(state, enemy);
                enemy->shoot_cooldown =
                    ENEMY_SHOOT_COOLDOWN *
                    (0.7f + rng_range(&state->rng, 60) * 0.01f);
            }
            continue;
        }
        enemy->shoot_cooldown -= dt;
        if (enemy->shoot_cooldown > 0.0f || !enemy_has_los(state, enemy))
            continue;
        enemy->aim_target_x = state->player.x + PLAYER_W * 0.5f;
        enemy->aim_target_y = state->player.y +
                              (state->player.crawling
                                   ? PLAYER_CRAWL_H * 0.45f
                                   : PLAYER_H * 0.15f);
        enemy->aim_timer = ENEMY_AIM_TIME;
        gameplay_world_sound(state, SFX_ENEMY_ALERT,
                             enemy->x + ENEMY_W * 0.5f,
                             enemy->y + ENEMY_H * 0.5f);
    }
}
