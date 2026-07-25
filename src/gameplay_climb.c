#include "gameplay_climb.h"

#include "gameplay_world.h"

#include <math.h>

static float random_between(Rng *rng, float minimum, float maximum)
{
    return minimum + (maximum - minimum) * rng_unit(rng);
}

static float spawn_delay(GameplayState *state, FacadeHazardType type)
{
    if (type == FACADE_HAZARD_BIRD)
    {
        return random_between(&state->rng, BIRD_SPAWN_MIN, BIRD_SPAWN_MAX);
    }
    return random_between(&state->rng, THROWN_OBJECT_SPAWN_MIN,
                          THROWN_OBJECT_SPAWN_MAX);
}

void gameplay_climb_init(GameplayState *state)
{
    if (state->level.map.mode != LEVEL_MODE_FACADE ||
        state->facade_hazards_initialized)
        return;

    for (int i = 0; i < state->level.map.facade_hazard_spawn_count; ++i)
    {
        FacadeHazardType type =
            state->level.map.facade_hazard_spawns[i].type;
        /* A short first delay makes a source legible before it attacks. */
        state->facade_hazard_spawn_timers[i] =
            spawn_delay(state, type) * random_between(&state->rng, 0.25f, 0.55f);
    }
    state->facade_hazards_initialized = true;
}

void gameplay_climb_update_player(GameplayState *state, const Input *input,
                                  float dt)
{
    Player *player = &state->player;
    float move_x = 0.0f;
    float move_y = 0.0f;
    if (input->left)
    {
        move_x -= 1.0f;
        player->facing = -1;
    }
    if (input->right)
    {
        move_x += 1.0f;
        player->facing = 1;
    }
    if (input->up)
        move_y -= 1.0f;
    if (input->down)
        move_y += 1.0f;
    if (move_x != 0.0f && move_y != 0.0f)
    {
        move_x *= 0.70710678f;
        move_y *= 0.70710678f;
    }

    player->vx = move_x * FACADE_CLIMB_SPEED;
    player->vy = move_y * FACADE_CLIMB_SPEED;
    player->x += player->vx * dt;
    player->y += player->vy * dt;
    player->on_ground = false;
    player->on_ladder = false;
    player->facade_climbing = true;
    player->crawling = false;

    float world_width = state->level.map.width * (float)TILE_SIZE;
    float world_height = state->level.map.height * (float)TILE_SIZE;
    float min_x = FACADE_CLIMB_SIDE_MARGIN;
    float max_x = world_width - PLAYER_W - FACADE_CLIMB_SIDE_MARGIN;
    float min_y = 0.0f;
    float max_y = world_height - PLAYER_H;
    if (max_x < min_x)
        max_x = min_x;
    if (max_y < min_y)
        max_y = min_y;
    if (player->x < min_x)
        player->x = min_x;
    if (player->x > max_x)
        player->x = max_x;
    if (player->y < min_y)
        player->y = min_y;
    if (player->y > max_y)
        player->y = max_y;

    if (move_x != 0.0f || move_y != 0.0f)
        player->anim_time += dt * 5.0f;
}

static bool spawn_thrown_object(GameplayState *state,
                                const FacadeHazardSpawn *spawn)
{
    ThrownObject *object = NULL;
    for (int i = 0; i < MAX_THROWN_OBJECTS; ++i)
    {
        if (!state->thrown_objects[i].active)
        {
            object = &state->thrown_objects[i];
            break;
        }
    }
    if (object == NULL)
        return false;

    float player_height = state->player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;
    float target_x = state->player.x + PLAYER_W * 0.5f;
    float target_y = state->player.y + player_height * 0.5f;
    float dx = target_x - spawn->x;
    if (fabsf(dx) < TILE_SIZE)
    {
        float world_mid = state->level.map.width * TILE_SIZE * 0.5f;
        dx = spawn->x < world_mid ? TILE_SIZE : -TILE_SIZE;
    }

    float speed = THROWN_OBJECT_SPEED *
                  random_between(&state->rng, 0.88f, 1.12f);
    object->vx = dx < 0.0f ? -speed : speed;
    float travel_time = fabsf(dx / object->vx);
    if (travel_time < 0.35f)
        travel_time = 0.35f;
    if (travel_time > 1.35f)
        travel_time = 1.35f;
    object->vy = (target_y - spawn->y -
                  0.5f * THROWN_OBJECT_GRAVITY * travel_time * travel_time) /
                 travel_time;
    if (object->vy < -330.0f)
        object->vy = -330.0f;
    if (object->vy > 150.0f)
        object->vy = 150.0f;
    object->x = spawn->x - THROWN_OBJECT_SIZE * 0.5f;
    object->y = spawn->y - THROWN_OBJECT_SIZE * 0.5f;
    object->angle = random_between(&state->rng, 0.0f, 6.28318531f);
    object->variant = rng_range(&state->rng, 3);
    object->active = true;
    return true;
}

static bool spawn_bird(GameplayState *state,
                       const FacadeHazardSpawn *spawn)
{
    Bird *bird = NULL;
    for (int i = 0; i < MAX_BIRDS; ++i)
    {
        if (!state->birds[i].active)
        {
            bird = &state->birds[i];
            break;
        }
    }
    if (bird == NULL)
        return false;

    float player_height = state->player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;
    float target_x = state->player.x + PLAYER_W * 0.5f;
    float target_y = state->player.y + player_height * 0.5f;
    float direction = target_x >= spawn->x ? 1.0f : -1.0f;
    if (fabsf(target_x - spawn->x) < TILE_SIZE)
    {
        float world_mid = state->level.map.width * TILE_SIZE * 0.5f;
        direction = spawn->x < world_mid ? 1.0f : -1.0f;
    }

    bird->x = spawn->x - BIRD_W * 0.5f;
    bird->y = spawn->y - BIRD_H * 0.5f;
    bird->vx = direction * BIRD_SPEED *
               random_between(&state->rng, 0.90f, 1.12f);
    bird->vy = (target_y - spawn->y) * 0.28f;
    if (bird->vy < -48.0f)
        bird->vy = -48.0f;
    if (bird->vy > 48.0f)
        bird->vy = 48.0f;
    bird->anim_time = random_between(&state->rng, 0.0f, 1.0f);
    bird->active = true;
    return true;
}

static bool point_hits_solid(const Level *level, float x, float y)
{
    int col = (int)floorf(x / TILE_SIZE);
    int row = (int)floorf(y / TILE_SIZE);
    return level_is_solid(level, col, row);
}

static void update_thrown_objects(GameplayState *state, float dt)
{
    float world_width = state->level.map.width * (float)TILE_SIZE;
    float world_height = state->level.map.height * (float)TILE_SIZE;
    float player_height = state->player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;

    for (int i = 0; i < MAX_THROWN_OBJECTS; ++i)
    {
        ThrownObject *object = &state->thrown_objects[i];
        if (!object->active)
            continue;

        object->vy += THROWN_OBJECT_GRAVITY * dt;
        object->x += object->vx * dt;
        object->y += object->vy * dt;
        object->angle += (object->vx < 0.0f ? -7.0f : 7.0f) * dt;

        float center_x = object->x + THROWN_OBJECT_SIZE * 0.5f;
        float center_y = object->y + THROWN_OBJECT_SIZE * 0.5f;
        if (point_hits_solid(&state->level, center_x, center_y))
        {
            gameplay_world_sound(state, SFX_BULLET_IMPACT,
                                 center_x, center_y);
            game_events_particles(&state->events, center_x, center_y, 6,
                                  object->vx < 0.0f ? -1 : 1);
            object->active = false;
            continue;
        }

        if (object->x + THROWN_OBJECT_SIZE < 0.0f ||
            object->x > world_width ||
            object->y > world_height + TILE_SIZE ||
            object->y + THROWN_OBJECT_SIZE < -TILE_SIZE)
        {
            object->active = false;
            continue;
        }

        if (!state->player.dying && state->invuln_timer <= 0.0f &&
            gameplay_boxes_overlap(object->x, object->y,
                                   THROWN_OBJECT_SIZE, THROWN_OBJECT_SIZE,
                                   state->player.x, state->player.y,
                                   PLAYER_W, player_height))
        {
            gameplay_world_sound(state, SFX_CRATE_BREAK,
                                 center_x, center_y);
            object->active = false;
            gameplay_hit_player(state);
        }
    }
}

static void update_birds(GameplayState *state, float dt)
{
    float world_width = state->level.map.width * (float)TILE_SIZE;
    float world_height = state->level.map.height * (float)TILE_SIZE;
    float player_height = state->player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;

    for (int i = 0; i < MAX_BIRDS; ++i)
    {
        Bird *bird = &state->birds[i];
        if (!bird->active)
            continue;
        bird->x += bird->vx * dt;
        bird->y += bird->vy * dt;
        bird->anim_time += dt;

        if (bird->x + BIRD_W < -TILE_SIZE ||
            bird->x > world_width + TILE_SIZE ||
            bird->y + BIRD_H < -TILE_SIZE ||
            bird->y > world_height + TILE_SIZE)
        {
            bird->active = false;
            continue;
        }

        if (!state->player.dying && state->invuln_timer <= 0.0f &&
            gameplay_boxes_overlap(bird->x, bird->y, BIRD_W, BIRD_H,
                                   state->player.x, state->player.y,
                                   PLAYER_W, player_height))
        {
            gameplay_world_sound(state, SFX_FAN_HIT,
                                 bird->x + BIRD_W * 0.5f,
                                 bird->y + BIRD_H * 0.5f);
            bird->active = false;
            gameplay_hit_player(state);
        }
    }
}

void gameplay_climb_update(GameplayState *state, float dt)
{
    if (state->level.map.mode != LEVEL_MODE_FACADE)
        return;

    gameplay_climb_init(state);
    float player_height = state->player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;
    float player_y = state->player.y + player_height * 0.5f;

    for (int i = 0; i < state->level.map.facade_hazard_spawn_count; ++i)
    {
        const FacadeHazardSpawn *spawn =
            &state->level.map.facade_hazard_spawns[i];
        state->facade_hazard_spawn_timers[i] -= dt;
        if (state->facade_hazard_spawn_timers[i] > 0.0f)
            continue;

        if (fabsf(spawn->y - player_y) > FACADE_HAZARD_WAKE_RANGE)
        {
            state->facade_hazard_spawn_timers[i] = 0.25f;
            continue;
        }

        bool spawned = spawn->type == FACADE_HAZARD_BIRD
                           ? spawn_bird(state, spawn)
                           : spawn_thrown_object(state, spawn);
        state->facade_hazard_spawn_timers[i] =
            spawned ? spawn_delay(state, spawn->type) : 0.25f;
    }

    update_thrown_objects(state, dt);
    update_birds(state, dt);
}
