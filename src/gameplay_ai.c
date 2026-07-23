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

static bool point_in_active_crate(const GameplayState *state,
                                  float px, float py)
{
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        const Crate *crate = &state->level.runtime.crates[i];
        if (crate->active &&
            px >= crate->x && px < crate->x + CRATE_W &&
            py >= crate->y && py < crate->y + CRATE_H)
            return true;
    }
    return false;
}

/* Sample the segment between two world points and report whether solid tiles
 * or crates block the sight line. Ladders and elevator shafts are transparent
 * (level_is_solid already treats them as non-solid), so a guard can see through
 * a ladder but not through a floor. Endpoints are skipped: both entity centres
 * sit inside empty tiles and must not count as self-occlusion. */
static bool los_clear(const GameplayState *state,
                      float ax, float ay, float bx, float by)
{
    float dx = bx - ax;
    float dy = by - ay;
    float dist = sqrtf(dx * dx + dy * dy);
    int steps = (int)(dist / ENEMY_LOS_STEP) + 1;
    for (int s = 1; s < steps; ++s)
    {
        float t = (float)s / (float)steps;
        float px = ax + dx * t;
        float py = ay + dy * t;
        if (level_is_solid(&state->level, (int)floorf(px / TILE_SIZE),
                           (int)floorf(py / TILE_SIZE)))
            return false;
        if (point_in_active_crate(state, px, py))
            return false;
    }
    return true;
}

/* Can the guard see the given world point? Combines a range limit, a forward
 * vision cone, a close-range peripheral radius, and a ray-cast line of sight. */
static bool enemy_sees_point(const GameplayState *state, const Enemy *enemy,
                             float tx, float ty, float range, float peripheral)
{
    float ex = enemy->x + ENEMY_W * 0.5f;
    float ey = enemy->y + ENEMY_H * 0.5f;
    float dx = tx - ex;
    float dy = ty - ey;
    float dist2 = dx * dx + dy * dy;
    if (dist2 <= peripheral * peripheral)
        return los_clear(state, ex, ey, tx, ty);
    if (dist2 > range * range)
        return false;
    float dist = sqrtf(dist2);
    /* Facing is horizontal (enemy->dir, 0). The dot product with the unit
     * direction to the target is the cosine of the angle between them. */
    if (dx * (float)enemy->dir / dist < ENEMY_VIEW_CONE_COS)
        return false;
    return los_clear(state, ex, ey, tx, ty);
}

static bool enemy_has_los(const GameplayState *state, const Enemy *enemy)
{
    float player_x = state->player.x + PLAYER_W * 0.5f;
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    float player_y = state->player.y + player_h * 0.5f;
    float range = ENEMY_VIEW_RANGE;
    float peripheral = ENEMY_PERIPHERAL_RANGE;
    /* Crawling shrinks both cone reach and the behind-the-back radius, so a
     * low, slow Chuck can slip closer and past a guard before being noticed. */
    if (state->player.crawling)
    {
        range *= ENEMY_CRAWL_VIEW_FACTOR;
        peripheral *= ENEMY_CRAWL_VIEW_FACTOR;
    }
    /* A guard mid-conversation is distracted and only notices Chuck up close. */
    if (enemy->talking && range > ENEMY_TALK_NOTICE_RADIUS)
        range = ENEMY_TALK_NOTICE_RADIUS;
    if (enemy_sees_point(state, enemy, player_x, player_y, range, peripheral))
        return true;
    /* Vertical awareness lane: a guard covers its own column, so it notices
     * Chuck climbing a ladder directly above or dropping in below within a
     * limited range even though that lies outside the forward cone. */
    float ex = enemy->x + ENEMY_W * 0.5f;
    float ey = enemy->y + ENEMY_H * 0.5f;
    return fabsf(player_x - ex) <= ENEMY_VERTICAL_SHOOT_HALF_W &&
           fabsf(player_y - ey) <= ENEMY_VERTICAL_SHOOT_RANGE &&
           los_clear(state, ex, ey, player_x, player_y);
}

/* Decide whether a guard that can see Chuck also has a clean firing solution,
 * and along which axis. Returns 0 for no shot; otherwise fills *vdir with
 * 0 (horizontal), -1 (up), or +1 (down). Horizontal fire is preferred. */
static bool enemy_shot_solution(const GameplayState *state, const Enemy *enemy,
                                int *vdir)
{
    if (!enemy_has_los(state, enemy))
        return false;
    float ex = enemy->x + ENEMY_W * 0.5f;
    float ey = enemy->y + ENEMY_H * 0.5f;
    float player_x = state->player.x + PLAYER_W * 0.5f;
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    float player_y = state->player.y + player_h * 0.5f;
    float dx = player_x - ex;
    float dy = player_y - ey;

    bool facing_player = !((dx > 0.0f && enemy->dir < 0) ||
                           (dx < 0.0f && enemy->dir > 0));
    if (facing_player && fabsf(dx) <= ENEMY_SHOOT_RANGE &&
        fabsf(dy) <= TILE_SIZE * 1.2f &&
        horizontal_los_clear(state, ex, player_x, ey))
    {
        *vdir = 0;
        return true;
    }
    if (fabsf(dx) <= ENEMY_VERTICAL_SHOOT_HALF_W &&
        fabsf(dy) <= ENEMY_VERTICAL_SHOOT_RANGE)
    {
        *vdir = dy < 0.0f ? -1 : 1;
        return true;
    }
    return false;
}

static bool alarm_target(const GameplayState *state,
                         float *target_x, float *target_y)
{
    if (!gameplay_alarm_active(state))
        return false;
    *target_x = state->alarm_target_x;
    *target_y = state->alarm_target_y;
    return true;
}

static bool another_guard_is_raising_alarm(const GameplayState *state,
                                           int enemy_index)
{
    for (int i = 0; i < state->enemy_count; ++i)
        if (i != enemy_index && !state->enemies[i].dead &&
            state->enemies[i].raising_alarm)
            return true;
    return false;
}

static int nearest_alarm_switch(const GameplayState *state,
                                const Enemy *enemy)
{
    int nearest = -1;
    float best_distance = 0.0f;
    float enemy_x = enemy->x + ENEMY_W * 0.5f;
    float enemy_y = enemy->y + ENEMY_H * 0.5f;
    for (int i = 0; i < state->level.map.alarm_switch_count; ++i)
    {
        const AlarmSwitch *alarm_switch =
            &state->level.map.alarm_switches[i];
        float switch_x = (alarm_switch->col + 0.5f) * TILE_SIZE;
        float switch_y = (alarm_switch->row + 0.5f) * TILE_SIZE;
        /* Vertical travel is deliberately weighted: a switch on the current
         * corridor is usually faster than a slightly nearer one by a ladder. */
        float distance = fabsf(switch_x - enemy_x) +
                         fabsf(switch_y - enemy_y) * 1.35f;
        if (nearest < 0 || distance < best_distance)
        {
            nearest = i;
            best_distance = distance;
        }
    }
    return nearest;
}

static void guard_run_to_alarm(GameplayState *state, Enemy *enemy,
                               int switch_index)
{
    enemy->raising_alarm = true;
    enemy->alarm_switch_index = switch_index;
    enemy->alarm_use_timer = 0.0f;
    enemy->aim_timer = 0.0f;
    enemy->talking = false;
    enemy->talk_timer = 0.0f;
    enemy->talk_partner = -1;
    enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
    gameplay_world_sound(state, SFX_ENEMY_ALERT,
                         enemy->x + ENEMY_W * 0.5f,
                         enemy->y + ENEMY_H * 0.5f);
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

/* True when there is no floor at the feet row ahead but solid ground (or a
 * ladder top) lies within a short drop below it. This lets a dog that ended
 * up on a ladder rung a tile above the floor walk off the ledge and drop back
 * down instead of treating both sides as a cliff and spinning in place. */
static bool dog_can_step_down(const GameplayState *state,
                              const Dog *dog, int direction)
{
    if (!dog->on_ground)
        return false;
    float probe_x = direction > 0 ? dog->x + DOG_W + 3.0f : dog->x - 3.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int feet_row = (int)floorf((dog->y + DOG_H + 2.0f) / TILE_SIZE);
    /* Do not step into a wall at body height. */
    if (level_is_solid(&state->level, col, feet_row - 1))
        return false;
    for (int drop = 1; drop <= DOG_STEP_DOWN_MAX_TILES; ++drop)
    {
        int row = feet_row + drop;
        if (level_is_solid(&state->level, col, row) ||
            level_is_ladder(&state->level, col, row))
            return true;
    }
    return false;
}

/* Whether the dog can make progress in a direction by any means. */
static bool dog_can_advance(const GameplayState *state, const Dog *dog,
                            int direction)
{
    return dog_has_floor_ahead(state, dog, direction) ||
           dog_can_step_down(state, dog, direction) ||
           dog_can_jump_gap(state, dog, direction);
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
    if (dog->turn_cooldown > 0.0f)
    {
        dog->turn_cooldown -= dt;
        if (dog->turn_cooldown < 0.0f)
            dog->turn_cooldown = 0.0f;
    }

    if (dog_sees_player(state, dog))
    {
        if (gameplay_alarm_active(state))
            gameplay_refresh_alarm_from_player(state);
        dog->chase_target_x = state->player.x + PLAYER_W * 0.5f;
        dog->has_chase_target = true;
        dog->state = DOG_CHASE;
        dog->lost_timer = DOG_LOST_TIME;
    }
    else if (gameplay_alarm_active(state))
    {
        if (!dog->has_chase_target)
        {
            float ignored_y;
            dog->has_chase_target =
                alarm_target(state, &dog->chase_target_x, &ignored_y);
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
        if (!dog->on_ground || dog_has_floor_ahead(state, dog, direction) ||
            dog_can_step_down(state, dog, direction))
        {
            dog->dir = direction;
            dog->vx = direction * speed;
        }
        else if (dog_can_jump_gap(state, dog, direction))
        {
            dog->dir = direction;
            dog->vx = direction * fmaxf(speed, DOG_JUMP_MIN_SPEED);
            dog->vy = -DOG_JUMP_SPEED;
            dog->on_ground = false;
        }
        else if (dog->turn_cooldown <= 0.0f &&
                 dog_can_advance(state, dog, -direction))
        {
            /* Ledge ahead that cannot be crossed: turn around and roam back.
             * Only flip when the other way is actually passable so a boxed-in
             * dog stands still instead of spinning in place on a ladder, and
             * the cooldown keeps it from flipping every frame. */
            dog->dir = -direction;
            dog->state = DOG_ROAM;
            dog->roam_target_x =
                dog->x - direction * (DOG_HANDLER_DISTANCE + 24.0f);
            dog->state_timer =
                0.7f + rng_range(&state->rng, 90) * 0.01f;
            dog->turn_cooldown = DOG_TURN_COOLDOWN;
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
    if (!gameplay_alarm_active(state))
    {
        for (int i = 0; i < state->enemy_count; ++i)
        {
            Enemy *first = &state->enemies[i];
            if (first->dead || first->provoked || first->raising_alarm ||
                first->talking ||
                first->climbing || !first->on_ground ||
                first->talk_partner != -1 || first->talk_cooldown > 0.0f)
                continue;
            for (int j = i + 1; j < state->enemy_count; ++j)
            {
                Enemy *second = &state->enemies[j];
                if (second->dead || second->provoked ||
                    second->raising_alarm || second->talking ||
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
        bool alarm_pursuit = gameplay_alarm_active(state);
        if (alarm_pursuit && enemy->raising_alarm)
        {
            enemy->raising_alarm = false;
            enemy->alarm_switch_index = -1;
            enemy->alarm_use_timer = 0.0f;
        }

        bool switch_pursuit = enemy->raising_alarm;
        if (switch_pursuit)
        {
            int switch_index = enemy->alarm_switch_index;
            if (switch_index < 0 ||
                switch_index >= state->level.map.alarm_switch_count)
            {
                enemy->raising_alarm = false;
                switch_pursuit = false;
            }
            else
            {
                const AlarmSwitch *alarm_switch =
                    &state->level.map.alarm_switches[switch_index];
                float switch_x = (alarm_switch->col + 0.5f) * TILE_SIZE;
                float switch_y = (alarm_switch->row + 0.5f) * TILE_SIZE;
                float enemy_x = enemy->x + ENEMY_W * 0.5f;
                enemy->pursuit_target_x = switch_x +
                    (enemy_x < switch_x ? -ALARM_SWITCH_STAND_DISTANCE
                                        : ALARM_SWITCH_STAND_DISTANCE);
                enemy->pursuit_target_y = switch_y;
                enemy->has_pursuit_target = true;
                if (fabsf(enemy_x - switch_x) <=
                        ALARM_SWITCH_USE_RANGE &&
                    fabsf(enemy->y + ENEMY_H * 0.5f - switch_y) <=
                        TILE_SIZE * 0.65f &&
                    enemy->on_ground && !enemy->climbing)
                {
                    enemy->alarm_use_timer += dt;
                    enemy->dir = switch_x < enemy_x
                                     ? -1
                                     : 1;
                    if (enemy->alarm_use_timer >= ALARM_SWITCH_USE_TIME)
                    {
                        gameplay_trigger_alarm(state, switch_x, switch_y,
                                               switch_index);
                        enemy->raising_alarm = false;
                        enemy->alarm_switch_index = -1;
                        enemy->alarm_use_timer = 0.0f;
                        alarm_pursuit = true;
                        switch_pursuit = false;
                    }
                }
                else
                    enemy->alarm_use_timer = 0.0f;
            }
        }

        /* Suspicion. When not already alarmed or committed to a switch, a guard
         * that heard a noise or found a body walks to the disturbance, scans,
         * then drops back to patrol. Seeing Chuck escalates it to real pursuit
         * via the combat/encounter passes. */
        bool investigating = false;
        if (!alarm_pursuit && !switch_pursuit && !enemy->provoked &&
            enemy->investigate_timer > 0.0f)
        {
            enemy->investigate_timer -= dt;
            if (enemy->investigate_timer < 0.0f)
                enemy->investigate_timer = 0.0f;
            if (enemy->investigate_timer > 0.0f)
            {
                investigating = true;
                enemy->pursuit_target_x = enemy->investigate_x;
                enemy->pursuit_target_y = enemy->investigate_y;
                enemy->has_pursuit_target = true;
                float ex = enemy->x + ENEMY_W * 0.5f;
                float ey = enemy->y + ENEMY_H * 0.5f;
                if (enemy->on_ground &&
                    fabsf(enemy->investigate_x - ex) <=
                        ENEMY_INVESTIGATE_REACH &&
                    fabsf(enemy->investigate_y - ey) <= TILE_SIZE * 0.75f)
                {
                    /* Arrived: shorten to a brief scan and turn on the spot. */
                    if (enemy->investigate_timer > ENEMY_INVESTIGATE_LOOK_TIME)
                        enemy->investigate_timer = ENEMY_INVESTIGATE_LOOK_TIME;
                    enemy->investigate_scan_timer -= dt;
                    if (enemy->investigate_scan_timer <= 0.0f)
                    {
                        enemy->dir = -enemy->dir;
                        enemy->investigate_scan_timer =
                            ENEMY_INVESTIGATE_SCAN_FLIP;
                    }
                }
            }
        }

        bool pursuing = alarm_pursuit || enemy->provoked || switch_pursuit ||
                        investigating;
        bool sees_player = pursuing && enemy_has_los(state, enemy);
        if (sees_player)
        {
            float height = state->player.crawling
                               ? (float)PLAYER_CRAWL_H
                               : (float)PLAYER_H;
            float player_cx = state->player.x + PLAYER_W * 0.5f;
            float player_cy = state->player.y + height * 0.5f;
            if (!switch_pursuit)
            {
                float target_x = player_cx;
                /* Posted-up tactics: with a clean horizontal shot already in
                 * range, hold position and fire instead of crowding into
                 * melee. Out of range, keep closing the distance. */
                int shot_vdir = 0;
                float enemy_cx = enemy->x + ENEMY_W * 0.5f;
                if (enemy_shot_solution(state, enemy, &shot_vdir) &&
                    shot_vdir == 0 &&
                    fabsf(player_cx - enemy_cx) <= ENEMY_KEEP_DISTANCE)
                    target_x = enemy_cx;
                enemy->pursuit_target_x = target_x;
                enemy->pursuit_target_y = player_cy;
                enemy->has_pursuit_target = true;
            }
            if (alarm_pursuit)
                gameplay_refresh_alarm_from_player(state);
            /* A suspicious guard that actually sees Chuck keeps chasing the
             * live position and stays alert for a while after losing sight. */
            if (investigating)
            {
                enemy->investigate_x = player_cx;
                enemy->investigate_y = player_cy;
                enemy->investigate_timer = ENEMY_INVESTIGATE_TIME;
            }
        }
        else if (alarm_pursuit)
        {
            float target_x;
            float target_y;
            if (alarm_target(state, &target_x, &target_y))
            {
                float enemy_x = enemy->x + ENEMY_W * 0.5f;
                float enemy_y = enemy->y + ENEMY_H * 0.5f;
                /* Fan the search party out around the last sighting so guards
                 * cover different ground instead of stacking on one pixel. */
                float search_x = target_x +
                                 (float)((i % 3) - 1) *
                                     ENEMY_ALARM_SEARCH_RADIUS *
                                     ENEMY_SEARCH_FAN;
                bool near_search_point =
                    fabsf(search_x - enemy_x) <=
                        ENEMY_ALARM_SEARCH_NEAR_RADIUS &&
                    fabsf(target_y - enemy_y) <= TILE_SIZE * 0.75f;
                if (near_search_point)
                {
                    /* Once at its search point, sweep both sides of the
                     * corridor instead of standing on one exact pixel. */
                    float phase = enemy->anim_time * 0.45f + (float)i * 1.7f;
                    search_x += sinf(phase) * ENEMY_ALARM_SEARCH_RADIUS;
                }
                enemy->pursuit_target_x = search_x;
                enemy->pursuit_target_y = target_y;
                enemy->has_pursuit_target = true;
            }
        }
        else if (!pursuing)
            enemy->has_pursuit_target = false;

        pursuing = pursuing && enemy->has_pursuit_target;
        bool hemmed_in =
            crate_or_enemy_blocks_side(state, i, -1) &&
            crate_or_enemy_blocks_side(state, i, 1);
        enemy_update(enemy, &state->level, dt, pursuing, alarm_pursuit,
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
        if (enemy->dead || enemy->climbing || enemy->raising_alarm)
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
            enemy->aim_vdir = 0;
            enemy->aim_target_x = player_x;
            enemy->aim_target_y = state->player.y + PLAYER_H * 0.15f;
            enemy->aim_timer = ENEMY_AIM_TIME *
                               (gameplay_alarm_active(state)
                                    ? ENEMY_ALARM_AIM_MULTIPLIER
                                    : 1.0f);
            gameplay_world_sound(state, SFX_ENEMY_ALERT,
                                 enemy_x, enemy_y);
        }
    }
}

static void update_guard_encounters(GameplayState *state, float dt)
{
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead)
            continue;

        bool sees_player = !enemy->climbing && enemy_has_los(state, enemy);
        if (!sees_player)
        {
            if (enemy->encounter_decided && !enemy->raising_alarm)
            {
                enemy->encounter_lost_timer -= dt;
                if (enemy->encounter_lost_timer <= 0.0f)
                {
                    enemy->encounter_decided = false;
                    enemy->encounter_lost_timer = 0.0f;
                }
            }
            continue;
        }

        enemy->encounter_lost_timer = GUARD_ENCOUNTER_RESET_TIME;
        if (gameplay_alarm_active(state))
            gameplay_refresh_alarm_from_player(state);
        if (enemy->encounter_decided || enemy->provoked ||
            enemy->raising_alarm || gameplay_alarm_active(state))
            continue;

        enemy->encounter_decided = true;
        int switch_index = nearest_alarm_switch(state, enemy);
        if (switch_index >= 0 &&
            !another_guard_is_raising_alarm(state, i) &&
            rng_range(&state->rng, 100) < GUARD_ALARM_CHANCE)
            guard_run_to_alarm(state, enemy, switch_index);
    }
}

/* A calm guard that sees a fallen comrade nearby becomes suspicious, walks over
 * to investigate, and often sprints to raise the building alarm. Latched per
 * guard so it reacts once, not every frame it stands beside the body. */
static void update_body_discovery(GameplayState *state)
{
    if (gameplay_alarm_active(state))
        return;
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead || enemy->climbing || enemy->alerted_by_body ||
            enemy->raising_alarm || enemy->provoked ||
            enemy->investigate_timer > 0.0f)
            continue;

        float bx = 0.0f;
        float by = 0.0f;
        bool found = false;
        for (int j = 0; j < state->enemy_count && !found; ++j)
        {
            const Enemy *body = &state->enemies[j];
            if (j == i || !body->dead)
                continue;
            float cx = body->x + ENEMY_W * 0.5f;
            float cy = body->y + ENEMY_H * 0.5f;
            if (enemy_sees_point(state, enemy, cx, cy, ENEMY_BODY_NOTICE_RANGE,
                                 ENEMY_PERIPHERAL_RANGE))
            {
                bx = cx;
                by = cy;
                found = true;
            }
        }
        for (int j = 0; j < state->dog_count && !found; ++j)
        {
            const Dog *body = &state->dogs[j];
            if (!body->dead)
                continue;
            float cx = body->x + DOG_W * 0.5f;
            float cy = body->y + DOG_H * 0.5f;
            if (enemy_sees_point(state, enemy, cx, cy, ENEMY_BODY_NOTICE_RANGE,
                                 ENEMY_PERIPHERAL_RANGE))
            {
                bx = cx;
                by = cy;
                found = true;
            }
        }
        if (!found)
            continue;

        enemy->alerted_by_body = true;
        enemy->investigate_x = bx;
        enemy->investigate_y = by;
        enemy->investigate_timer = ENEMY_INVESTIGATE_TIME;
        enemy->investigate_scan_timer = ENEMY_INVESTIGATE_SCAN_FLIP;
        enemy->dir = bx < enemy->x + ENEMY_W * 0.5f ? -1 : 1;

        int switch_index = nearest_alarm_switch(state, enemy);
        if (switch_index >= 0 &&
            !another_guard_is_raising_alarm(state, i) &&
            rng_range(&state->rng, 100) < GUARD_BODY_ALARM_CHANCE)
            guard_run_to_alarm(state, enemy, switch_index);
        else
            gameplay_world_sound(state, SFX_ENEMY_ALERT,
                                 enemy->x + ENEMY_W * 0.5f,
                                 enemy->y + ENEMY_H * 0.5f);
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
        if (enemy->aim_vdir != 0)
        {
            /* Straight vertical shot: fire up or down the guard's own column,
             * e.g. at Chuck climbing a ladder above or dropping in below. */
            bullet->vx = 0.0f;
            bullet->vy = (float)enemy->aim_vdir * ENEMY_BULLET_SPEED;
            bullet->x = enemy_x - BULLET_W * 0.5f;
            bullet->y = enemy->aim_vdir < 0
                            ? enemy->y - BULLET_H
                            : enemy->y + ENEMY_H;
        }
        else
        {
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
        }
        bullet->active = true;
        enemy->recoil_timer = 0.14f;
        gameplay_world_sound(state, SFX_ENEMY_SHOT,
                             enemy_x, enemy->y + ENEMY_H * 0.5f);
        return;
    }
}

void gameplay_ai_update_combat(GameplayState *state, float dt)
{
    update_guard_encounters(state, dt);
    update_body_discovery(state);
    update_enemy_reactions(state);
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead || enemy->climbing || enemy->raising_alarm)
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
                    (0.7f + rng_range(&state->rng, 60) * 0.01f) *
                    (gameplay_alarm_active(state)
                         ? ENEMY_ALARM_COOLDOWN_MULTIPLIER
                         : 1.0f);
            }
            continue;
        }
        enemy->shoot_cooldown -= dt;
        int vdir = 0;
        if (enemy->shoot_cooldown > 0.0f ||
            !enemy_shot_solution(state, enemy, &vdir))
            continue;
        enemy->aim_vdir = vdir;
        if (vdir == 0)
        {
            /* Lead a moving target so guards stop firing behind a running
             * Chuck. The lead is capped implicitly by the clamp in the fire. */
            enemy->aim_target_x = state->player.x + PLAYER_W * 0.5f +
                                  state->player.vx * ENEMY_AIM_LEAD;
            enemy->aim_target_y = state->player.y +
                                  (state->player.crawling
                                       ? PLAYER_CRAWL_H * 0.45f
                                       : PLAYER_H * 0.15f);
        }
        else
        {
            enemy->aim_target_x = enemy->x + ENEMY_W * 0.5f;
            enemy->aim_target_y = state->player.y +
                                  (state->player.crawling
                                       ? PLAYER_CRAWL_H * 0.5f
                                       : PLAYER_H * 0.5f);
        }
        enemy->aim_timer = ENEMY_AIM_TIME *
                           (gameplay_alarm_active(state)
                                ? ENEMY_ALARM_AIM_MULTIPLIER
                                : 1.0f);
        gameplay_world_sound(state, SFX_ENEMY_ALERT,
                             enemy->x + ENEMY_W * 0.5f,
                             enemy->y + ENEMY_H * 0.5f);
    }
}
