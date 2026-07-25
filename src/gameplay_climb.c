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

static float calm_delay(GameplayState *state)
{
    return random_between(&state->rng, FACADE_WIND_CALM_MIN,
                          FACADE_WIND_CALM_MAX);
}

/*
 * Facade collision is deliberately its own thing. `level_is_solid` reports
 * out-of-bounds tiles as wall, which is right for a floor-based level but
 * would pin a climber against the top of the shaft; here the world edges are
 * handled by the clamp instead, and only real masonry blocks movement.
 */
static bool facade_tile_solid(const Level *level, int col, int row)
{
    if (col < 0 || col >= level->map.width ||
        row < 0 || row >= level->map.height)
    {
        return false;
    }
    return level->map.tiles[row][col] == TILE_WALL;
}

static bool facade_point_solid(const Level *level, float x, float y)
{
    return facade_tile_solid(level, (int)floorf(x / TILE_SIZE),
                             (int)floorf(y / TILE_SIZE));
}

static bool facade_box_blocked(const Level *level, float x, float y,
                               float w, float h)
{
    int first_col = (int)floorf(x / TILE_SIZE);
    int last_col = (int)floorf((x + w - 1.0f) / TILE_SIZE);
    int first_row = (int)floorf(y / TILE_SIZE);
    int last_row = (int)floorf((y + h - 1.0f) / TILE_SIZE);
    for (int row = first_row; row <= last_row; ++row)
    {
        for (int col = first_col; col <= last_col; ++col)
        {
            if (facade_tile_solid(level, col, row))
                return true;
        }
    }
    return false;
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
        state->facade_hazard_windup_timers[i] = 0.0f;
    }
    state->facade_wind_phase = FACADE_WIND_CALM;
    state->facade_wind_timer = calm_delay(state);
    state->facade_wind_dir = rng_range(&state->rng, 2) == 0 ? -1 : 1;
    state->facade_wind_sheltered = false;
    state->facade_hazards_initialized = true;
}

float gameplay_climb_wind_push(const GameplayState *state)
{
    if (state->facade_wind_phase != FACADE_WIND_GUSTING)
        return 0.0f;
    return (float)state->facade_wind_dir * FACADE_WIND_PUSH;
}

/* A solid tile a short way upwind of the climber's chest takes the gust. */
static bool facade_wind_sheltered(const GameplayState *state)
{
    if (state->facade_wind_dir == 0)
        return false;

    const Player *player = &state->player;
    float height = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    float edge = state->facade_wind_dir > 0
                     ? player->x - FACADE_WIND_SHELTER_REACH
                     : player->x + PLAYER_W + FACADE_WIND_SHELTER_REACH;
    for (int sample = 0; sample < 3; ++sample)
    {
        float y = player->y + height * (0.2f + 0.3f * (float)sample);
        if (facade_point_solid(&state->level, edge, y))
            return true;
    }
    return false;
}

static void update_wind(GameplayState *state, float dt)
{
    state->facade_wind_timer -= dt;
    if (state->facade_wind_timer > 0.0f)
        return;

    switch (state->facade_wind_phase)
    {
    case FACADE_WIND_CALM:
        state->facade_wind_dir = rng_range(&state->rng, 2) == 0 ? -1 : 1;
        state->facade_wind_phase = FACADE_WIND_WARNING;
        state->facade_wind_timer = FACADE_WIND_WARN_TIME;
        /* The cue is the whole point of the warning: it is what lets the
         * player pick a shelter before the wall starts pushing back. */
        game_events_sound(&state->events, SFX_WIND_GUST);
        break;
    case FACADE_WIND_WARNING:
        state->facade_wind_phase = FACADE_WIND_GUSTING;
        state->facade_wind_timer = FACADE_WIND_GUST_TIME;
        break;
    case FACADE_WIND_GUSTING:
    default:
        state->facade_wind_phase = FACADE_WIND_CALM;
        state->facade_wind_timer = calm_delay(state);
        break;
    }
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

    /* Evaluated here, once per simulated frame, so the HUD and the renderer
     * report the same shelter the movement below is using. */
    float wind = gameplay_climb_wind_push(state);
    state->facade_wind_sheltered = wind != 0.0f && facade_wind_sheltered(state);
    if (wind != 0.0f && !state->facade_wind_sheltered)
        player->vx += wind;

    player->on_ground = false;
    player->on_ladder = false;
    player->facade_climbing = true;
    player->crawling = false;

    /* Axis-separated so a climber pressed diagonally into a ledge slides
     * along it instead of sticking to the corner. */
    float height = (float)PLAYER_H;
    float step_x = player->vx * dt;
    if (step_x != 0.0f &&
        !facade_box_blocked(&state->level, player->x + step_x, player->y,
                            (float)PLAYER_W, height))
    {
        player->x += step_x;
    }
    float step_y = player->vy * dt;
    if (step_y != 0.0f &&
        !facade_box_blocked(&state->level, player->x, player->y + step_y,
                            (float)PLAYER_W, height))
    {
        player->y += step_y;
    }

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

    /* Bank height every few floors. Only positions the climber has actually
     * held are stored, which is what keeps a respawn out of the masonry. */
    if (!state->facade_has_checkpoint ||
        player->y < state->facade_checkpoint_y - FACADE_CHECKPOINT_STEP)
    {
        state->facade_checkpoint_x = player->x;
        state->facade_checkpoint_y = player->y;
        state->facade_has_checkpoint = true;
    }
}

void gameplay_climb_restore_checkpoint(GameplayState *state)
{
    if (state->level.map.mode != LEVEL_MODE_FACADE ||
        !state->facade_has_checkpoint)
    {
        return;
    }
    state->player.x = state->facade_checkpoint_x;
    state->player.y = state->facade_checkpoint_y;
    state->player.vx = 0.0f;
    state->player.vy = 0.0f;
    state->player.facade_climbing = true;

    /* Nothing already in the air should be allowed to land on a climber who
     * has just been put back on the wall. */
    for (int i = 0; i < MAX_THROWN_OBJECTS; ++i)
        state->thrown_objects[i].active = false;
    for (int i = 0; i < MAX_BIRDS; ++i)
        state->birds[i].active = false;
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
    gameplay_world_sound(state, SFX_GRENADE_THROW, spawn->x, spawn->y);
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
    gameplay_world_sound(state, SFX_BIRD_CALL, spawn->x, spawn->y);
    return true;
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
        /* Shattering on ledges is what turns them into usable cover. */
        if (facade_point_solid(&state->level, center_x, center_y))
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
        bird->anim_time += dt;
        /* The swoop is a function of the bird's own clock, so a replay of the
         * same seed reproduces the same flight path. */
        float swoop = sinf(bird->anim_time * BIRD_WAVE_RATE) * BIRD_WAVE_SPEED;
        bird->x += bird->vx * dt;
        bird->y += (bird->vy + swoop) * dt;

        if (bird->x + BIRD_W < -TILE_SIZE ||
            bird->x > world_width + TILE_SIZE ||
            bird->y + BIRD_H < -TILE_SIZE ||
            bird->y > world_height + TILE_SIZE)
        {
            bird->active = false;
            continue;
        }

        /* Birds break off against masonry, so a ledge is cover from them too. */
        float bird_center_x = bird->x + BIRD_W * 0.5f;
        float bird_center_y = bird->y + BIRD_H * 0.5f;
        if (facade_point_solid(&state->level, bird_center_x, bird_center_y))
        {
            gameplay_world_sound(state, SFX_BIRD_CALL,
                                 bird_center_x, bird_center_y);
            game_events_particles(&state->events, bird_center_x, bird_center_y,
                                  5, bird->vx < 0.0f ? -1 : 1);
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
    update_wind(state, dt);
    float player_height = state->player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;
    float player_y = state->player.y + player_height * 0.5f;

    for (int i = 0; i < state->level.map.facade_hazard_spawn_count; ++i)
    {
        const FacadeHazardSpawn *spawn =
            &state->level.map.facade_hazard_spawns[i];

        if (state->facade_hazard_windup_timers[i] > 0.0f)
        {
            state->facade_hazard_windup_timers[i] -= dt;
            if (state->facade_hazard_windup_timers[i] > 0.0f)
                continue;
            state->facade_hazard_windup_timers[i] = 0.0f;
            state->facade_hazard_spawn_timers[i] =
                spawn_thrown_object(state, spawn)
                    ? spawn_delay(state, spawn->type)
                    : 0.25f;
            continue;
        }

        state->facade_hazard_spawn_timers[i] -= dt;
        if (state->facade_hazard_spawn_timers[i] > 0.0f)
            continue;

        if (fabsf(spawn->y - player_y) > FACADE_HAZARD_WAKE_RANGE)
        {
            state->facade_hazard_spawn_timers[i] = 0.25f;
            continue;
        }

        if (spawn->type == FACADE_HAZARD_BIRD)
        {
            state->facade_hazard_spawn_timers[i] =
                spawn_bird(state, spawn) ? spawn_delay(state, spawn->type)
                                         : 0.25f;
            continue;
        }

        /* Throwers announce themselves, then let go a beat later. */
        state->facade_hazard_windup_timers[i] = THROWN_OBJECT_WINDUP;
        gameplay_world_sound(state, SFX_GUARD_TALK, spawn->x, spawn->y);
    }

    update_thrown_objects(state, dt);
    update_birds(state, dt);
}
