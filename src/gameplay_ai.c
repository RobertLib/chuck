#include "gameplay_ai.h"

#include "gameplay_physics.h"
#include "gameplay_world.h"

#include <math.h>

#define ENEMY_SIDE_PROBE 4.0f

/*
 * Somewhere to put a guard the doors have just sent.
 *
 * A fresh slot first, and a downed guard's only if the array is full. The
 * bodies are drawn now, so taking a slot back deletes one off the floor in
 * front of the player — and that body is the whole reason the guard beside it
 * is walking over to look. When the cap does force it, take the one furthest
 * from Chuck: it is the body he is least likely to be looking at.
 */
static int find_enemy_slot(GameplayState *state)
{
    if (state->enemy_count < MAX_ENEMIES)
        return state->enemy_count++;

    int furthest = -1;
    float best_distance = -1.0f;
    float player_x = state->player.x + PLAYER_W * 0.5f;
    for (int i = 0; i < state->enemy_count; ++i)
    {
        if (!state->enemies[i].dead)
            continue;
        float distance = fabsf(state->enemies[i].x + ENEMY_W * 0.5f - player_x);
        if (distance > best_distance)
        {
            best_distance = distance;
            furthest = i;
        }
    }
    if (furthest < 0)
        return -1;

    for (int dog = 0; dog < state->dog_count; ++dog)
        if (!state->dogs[dog].dead && state->dogs[dog].owner == furthest)
            state->dogs[dog].owner = -1;
    return furthest;
}

static int find_dog_slot(GameplayState *state)
{
    for (int i = 0; i < state->dog_count; ++i)
        if (state->dogs[i].dead)
            return i;
    return state->dog_count < MAX_DOGS ? state->dog_count++ : -1;
}

/*
 * A body falls.
 *
 * It used to be able to hang wherever it was shot, and that cost nothing while
 * nothing drew it. It is on the floor of the frame now, and it is also a place
 * on the map: `update_body_discovery` sends the next guard who sees it over to
 * look. A guard shot off a ladder or at the top of a jump would otherwise leave
 * a corpse floating in the air with a comrade walking to the empty tile beneath
 * it. Nothing else about a body is simulated — it does not collide with anyone
 * and does not trigger a cracked panel on the way down.
 */
static void settle_body(GameplayState *state, float *x, float *y, float *vy,
                        float w, float h, float dt)
{
    bool on_ground = false;
    float vx = 0.0f;
    *vy += GRAVITY * dt;
    if (*vy > MAX_FALL_SPEED)
        *vy = MAX_FALL_SPEED;
    level_move(&state->level, x, y, &vx, vy, w, h, dt, false, &on_ground,
               false);
    if (on_ground)
        *vy = 0.0f;
}

static bool actor_or_tile_blocks_side(const GameplayState *state,
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
    /* A guard mid-conversation is distracted and only notices Chuck up close.
     * A radio check is not that: he is standing on his own post facing his own
     * corridor with a handset at his shoulder, and the whole point of the beat
     * is that it costs the player nothing either way. Blinding him for it
     * would quietly turn a piece of colour into a stealth window. */
    if (enemy->talking && !enemy_on_radio(enemy) &&
        range > ENEMY_TALK_NOTICE_RADIUS)
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
    janitor->cart_dir = janitor->dir;
    janitor->anim_time = (float)rng_range(rng, 628) * 0.01f;
    janitor_set_activity(janitor,
                          starts_mopping ? JANITOR_MOP : JANITOR_WALK, rng);
}

static void janitor_collision_bounds(const Janitor *janitor, int cart_dir,
                                     float *x, float *width)
{
    *x = janitor->x;
    *width = JANITOR_W + JANITOR_CART_SIDE_EXTENT;
    if (cart_dir > 0)
        *x -= JANITOR_CART_SIDE_EXTENT;
}

static bool janitor_box_clear(const GameplayState *state,
                              float x, float y, float width, float height)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + width - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + height - 1.0f) / TILE_SIZE);
    for (int row = top; row <= bottom; ++row)
    {
        for (int col = left; col <= right; ++col)
        {
            if (level_is_solid(&state->level, col, row) ||
                level_tile(&state->level, col, row) == TILE_DOOR)
                return false;
        }
    }
    return true;
}

static bool janitor_cart_side_clear(const GameplayState *state,
                                    const Janitor *janitor, int cart_dir)
{
    float x;
    float width;
    janitor_collision_bounds(janitor, cart_dir, &x, &width);
    return janitor_box_clear(state, x, janitor->y, width, JANITOR_H);
}

static bool janitor_has_floor_ahead(const GameplayState *state,
                                    const Janitor *janitor)
{
    float x;
    float width;
    janitor_collision_bounds(janitor, janitor->cart_dir, &x, &width);
    float probe_x = janitor->dir > 0
                        ? x + width + 7.0f
                        : x - 7.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int row = (int)floorf((janitor->y + JANITOR_H + 3.0f) / TILE_SIZE);
    return level_is_solid(&state->level, col, row) ||
           level_is_ladder(&state->level, col, row);
}

static bool janitor_side_blocked(const GameplayState *state,
                                 const Janitor *janitor)
{
    float x;
    float width;
    janitor_collision_bounds(janitor, janitor->cart_dir, &x, &width);
    float probe_x = janitor->dir > 0
                        ? x + width + 2.0f
                        : x - 2.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int top = (int)floorf((janitor->y + 1.0f) / TILE_SIZE);
    int bottom = (int)floorf((janitor->y + JANITOR_H - 1.0f) / TILE_SIZE);
    for (int row = top; row <= bottom; ++row)
    {
        if (level_is_solid(&state->level, col, row) ||
            level_tile(&state->level, col, row) == TILE_DOOR)
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

    /* Keep the cart on its old side after a turn at a wall. Once the janitor
     * has walked far enough away, it can move to the new trailing side without
     * appearing inside static geometry. */
    if (janitor->cart_dir != janitor->dir &&
        janitor_cart_side_clear(state, janitor, janitor->dir))
        janitor->cart_dir = janitor->dir;

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
    float collision_x;
    float collision_width;
    janitor_collision_bounds(janitor, janitor->cart_dir,
                             &collision_x, &collision_width);
    float left_extension = janitor->x - collision_x;
    level_move(&state->level, &collision_x, &janitor->y,
               &janitor->vx, &janitor->vy, collision_width, JANITOR_H,
               dt, false, &janitor->on_ground, false);
    janitor->x = collision_x + left_extension;
    if (janitor->activity == JANITOR_WALK && intended_vx != 0.0f &&
        janitor->vx == 0.0f)
    {
        janitor->dir = -janitor->dir;
        janitor_set_activity(janitor, JANITOR_PAUSE, &state->rng);
    }
}

/*
 * The evacuation.
 *
 * Everyone in the room runs for the way the player just came in — in the lobby
 * that is the street entrance behind him, which is the only door in a level
 * that leads out of the building rather than further into it. Nothing here
 * touches the simulation: civilians are not seen by guards, cannot be hit, and
 * carry no collision of their own, so the scene can only ever be staging.
 */
static float civilian_exit_x(const GameplayState *state)
{
    return state->level.map.start_x + PLAYER_W * 0.5f;
}

static void civilian_init(Civilian *civilian, float x, float y,
                          float exit_x, int order, Rng *rng)
{
    *civilian = (Civilian){0};
    civilian->x = x;
    civilian->y = y;
    civilian->flee_dir = x + CIVILIAN_W * 0.5f < exit_x ? 1 : -1;
    /* Caught mid-turn: still facing what came through the door, not yet
     * running from it. */
    civilian->dir = -civilian->flee_dir;
    civilian->activity = CIVILIAN_STARTLED;
    civilian->activity_timer =
        CIVILIAN_STARTLE_MIN +
        (float)rng_range(rng, 1000) * 0.001f * CIVILIAN_STARTLE_SPREAD;
    civilian->speed =
        CIVILIAN_RUN_SPEED +
        (float)rng_range(rng, 1000) * 0.001f * CIVILIAN_RUN_SPEED_SPREAD;
    civilian->anim_time = (float)rng_range(rng, 628) * 0.01f;
    civilian->fade = 1.0f;
    civilian->variant = (order + rng_range(rng, CIVILIAN_VARIANTS)) %
                        CIVILIAN_VARIANTS;
    if (rng_range(rng, 100) < CIVILIAN_STUMBLE_CHANCE)
    {
        civilian->stumble_timer =
            CIVILIAN_STUMBLE_DELAY_MIN +
            (float)rng_range(rng, 1000) * 0.001f *
                CIVILIAN_STUMBLE_DELAY_SPREAD;
    }
}

static void civilian_begin_run(GameplayState *state, Civilian *civilian)
{
    /*
     * Only the first of them gets words. The five break over a 2.4 second
     * spread (`CIVILIAN_STARTLE_SPREAD`) and the strip holds a line for nearly
     * four, so letting each one speak would replace the caption four times
     * before anybody could finish reading the first — five half-second flashes
     * that read as a bug rather than as a room emptying. One voice carries it
     * and the other four stay what they always were: separate shouts.
     */
    bool first = true;
    for (int i = 0; i < state->civilian_count; ++i)
    {
        if (&state->civilians[i] != civilian &&
            state->civilians[i].activity != CIVILIAN_STARTLED)
        {
            first = false;
            break;
        }
    }

    civilian->activity = CIVILIAN_FLEEING;
    civilian->dir = civilian->flee_dir;
    /* One voice per person as they break, so the room empties as a handful of
     * separate shouts instead of a single crowd noise. */
    gameplay_world_sound(state,
                         civilian->variant == 1 ? SFX_CIVILIAN_SCREAM
                                                : SFX_CIVILIAN_SHOUT,
                         civilian->x + CIVILIAN_W * 0.5f,
                         civilian->y + CIVILIAN_H * 0.5f);
    /* And what the voice is saying. This is the first thing anybody sees in
     * the campaign, and until now it was five people running out of a room
     * for no stated reason: the crew walked Ellen through this lobby ninety
     * seconds ago and these are the only witnesses in the game. A shout with
     * no words in it makes them scenery. */
    if (first)
        gameplay_crew_chatter(state, CHATTER_PANIC, -1,
                              civilian->x + CIVILIAN_W * 0.5f,
                              civilian->y + CIVILIAN_H * 0.5f);
}

static void update_civilian(GameplayState *state, Civilian *civilian, float dt)
{
    if (civilian->activity == CIVILIAN_GONE)
        return;

    switch (civilian->activity)
    {
    case CIVILIAN_STARTLED:
        civilian->vx = 0.0f;
        civilian->anim_time += dt * 1.4f;
        civilian->activity_timer -= dt;
        if (civilian->activity_timer <= 0.0f)
            civilian_begin_run(state, civilian);
        break;
    case CIVILIAN_FLEEING:
        civilian->vx = (float)civilian->flee_dir * civilian->speed;
        civilian->anim_time += dt * 3.4f;
        if (civilian->stumble_timer > 0.0f)
        {
            civilian->stumble_timer -= dt;
            if (civilian->stumble_timer <= 0.0f)
            {
                civilian->stumble_timer = 0.0f;
                civilian->activity = CIVILIAN_STUMBLING;
                civilian->activity_timer = CIVILIAN_STUMBLE_TIME;
            }
        }
        break;
    case CIVILIAN_STUMBLING:
        /* Sprawled, then scrambling up: the last of the beat already carries
         * some speed so getting up flows back into the run. */
        civilian->anim_time += dt * 2.0f;
        civilian->activity_timer -= dt;
        civilian->vx = civilian->activity_timer < CIVILIAN_STUMBLE_TIME * 0.3f
                           ? (float)civilian->flee_dir * civilian->speed * 0.35f
                           : 0.0f;
        if (civilian->activity_timer <= 0.0f)
        {
            civilian->activity = CIVILIAN_FLEEING;
            civilian->activity_timer = 0.0f;
        }
        break;
    case CIVILIAN_GONE:
        return;
    }

    civilian->vy += GRAVITY * dt;
    if (civilian->vy > MAX_FALL_SPEED)
        civilian->vy = MAX_FALL_SPEED;
    float intended_vx = civilian->vx;
    level_move(&state->level, &civilian->x, &civilian->y,
               &civilian->vx, &civilian->vy, CIVILIAN_W, CIVILIAN_H,
               dt, false, &civilian->on_ground, false);

    /* Panic does not turn around at an obstacle the way a patrol does: hop
     * what can be hopped. Should the way really be shut, the person leaves the
     * shot after a moment rather than running on the spot for the whole
     * level. */
    if (intended_vx != 0.0f && civilian->vx == 0.0f)
    {
        if (civilian->on_ground && civilian->stuck_timer <= 0.0f)
            civilian->vy = -CIVILIAN_HOP_SPEED;
        civilian->stuck_timer += dt;
        if (civilian->stuck_timer >= CIVILIAN_STUCK_TIME)
            civilian->fade -= dt / CIVILIAN_STUCK_FADE_TIME;
    }
    else
    {
        civilian->stuck_timer = 0.0f;
    }

    if (civilian->activity != CIVILIAN_STARTLED)
    {
        /* Fade with the remaining distance rather than with a timer, so the
         * dissolve always happens in the doorway however fast this one runs. */
        float distance = fabsf(civilian->x + CIVILIAN_W * 0.5f -
                               civilian_exit_x(state));
        float reach = (distance - CIVILIAN_EXIT_REACH) /
                      (CIVILIAN_FADE_DISTANCE - CIVILIAN_EXIT_REACH);
        if (reach < civilian->fade)
            civilian->fade = reach;
        if (distance <= CIVILIAN_EXIT_REACH)
            civilian->fade = 0.0f;
    }
    if (civilian->fade <= 0.0f)
    {
        civilian->fade = 0.0f;
        civilian->activity = CIVILIAN_GONE;
    }
    else if (civilian->fade > 1.0f)
    {
        civilian->fade = 1.0f;
    }
}

/*
 * The front desk.
 *
 * The janitor's patrol is a walk that happens to be interrupted; this one is a
 * post that is left and returned to. Every errand target is measured from
 * post_x rather than from wherever the walk happened to stop, so a level the
 * player spends ten minutes in still has someone standing at the counter
 * afterwards instead of a receptionist who has wandered into the next room.
 *
 * Like the janitor and the civilians this touches nothing: no perception, no
 * player collision, no damage, no scoring, no events.
 */
static int receptionist_floor_row(const Receptionist *receptionist)
{
    return (int)floorf((receptionist->y + RECEPTIONIST_H + 3.0f) / TILE_SIZE);
}

static bool receptionist_column_walkable(const GameplayState *state,
                                         const Receptionist *receptionist,
                                         int col)
{
    int floor_row = receptionist_floor_row(receptionist);
    if (!level_is_solid(&state->level, col, floor_row) &&
        !level_is_ladder(&state->level, col, floor_row))
        return false;
    int top = (int)floorf((receptionist->y + 1.0f) / TILE_SIZE);
    int bottom =
        (int)floorf((receptionist->y + RECEPTIONIST_H - 1.0f) / TILE_SIZE);
    for (int row = top; row <= bottom; ++row)
    {
        if (level_is_solid(&state->level, col, row) ||
            level_tile(&state->level, col, row) == TILE_DOOR)
            return false;
    }
    return true;
}

/* How many whole tiles of standing room there are on one side of the post. */
static int receptionist_open_run(const GameplayState *state,
                                 const Receptionist *receptionist, int dir)
{
    int col = (int)floorf((receptionist->x + RECEPTIONIST_W * 0.5f) /
                          TILE_SIZE);
    int run = 0;
    for (int step = 1; step <= RECEPTIONIST_OPEN_RUN_PROBE; ++step)
    {
        if (!receptionist_column_walkable(state, receptionist,
                                          col + dir * step))
            break;
        run++;
    }
    return run;
}

static bool receptionist_can_step(const GameplayState *state,
                                  const Receptionist *receptionist, int dir)
{
    float edge = dir > 0 ? receptionist->x + RECEPTIONIST_W : receptionist->x;
    int col = (int)floorf((edge + (float)dir * 3.0f) / TILE_SIZE);
    return receptionist_column_walkable(state, receptionist, col);
}

static float receptionist_roll(Rng *rng, float minimum, float spread)
{
    return minimum + (float)rng_range(rng, 1000) * 0.001f * spread;
}

static void receptionist_set_activity(Receptionist *receptionist,
                                      ReceptionistActivity activity, Rng *rng)
{
    receptionist->activity = activity;
    receptionist->vx = 0.0f;
    switch (activity)
    {
    case RECEPTIONIST_DESK:
        receptionist->activity_timer =
            receptionist_roll(rng, RECEPTIONIST_DESK_TIME_MIN,
                              RECEPTIONIST_DESK_TIME_SPREAD);
        receptionist->glance_timer =
            receptionist_roll(rng, RECEPTIONIST_GLANCE_MIN,
                              RECEPTIONIST_GLANCE_SPREAD);
        receptionist->glancing = false;
        break;
    case RECEPTIONIST_WALK:
        /* A walk ends where it arrives, not when a clock runs out. */
        receptionist->activity_timer = 0.0f;
        receptionist->walk_dir =
            receptionist->target_x >= receptionist->x ? 1 : -1;
        break;
    case RECEPTIONIST_ERRAND:
        receptionist->activity_timer =
            receptionist_roll(rng, RECEPTIONIST_ERRAND_TIME_MIN,
                              RECEPTIONIST_ERRAND_TIME_SPREAD);
        break;
    }
}

static void receptionist_begin_errand(GameplayState *state,
                                      Receptionist *receptionist)
{
    int dir = rng_range(&state->rng, 2) == 0 ? -1 : 1;
    /* A counter tucked into a corner still gets its errand: when the rolled
     * side is a wall, take the side the room is on. */
    if (receptionist_open_run(state, receptionist, dir) < 2 &&
        receptionist_open_run(state, receptionist, -dir) >= 2)
        dir = -dir;
    float reach = receptionist_roll(&state->rng, RECEPTIONIST_ERRAND_MIN_REACH,
                                    RECEPTIONIST_ERRAND_REACH_SPREAD);
    receptionist->target_x = receptionist->post_x + (float)dir * reach;
    receptionist->heading_home = false;
    receptionist_set_activity(receptionist, RECEPTIONIST_WALK, &state->rng);
}

static void receptionist_arrive(GameplayState *state,
                                Receptionist *receptionist, bool on_target)
{
    /* Landing exactly on the target is what keeps the post from drifting off
     * the counter over a long level. It is only ever a fraction of a frame's
     * travel back into ground already walked, so it cannot enter geometry. */
    if (on_target)
        receptionist->x = receptionist->target_x;
    receptionist_set_activity(receptionist,
                              receptionist->heading_home
                                  ? RECEPTIONIST_DESK
                                  : RECEPTIONIST_ERRAND,
                              &state->rng);
}

static void receptionist_head_home(GameplayState *state,
                                   Receptionist *receptionist)
{
    receptionist->target_x = receptionist->post_x;
    receptionist->heading_home = true;
    receptionist_set_activity(receptionist, RECEPTIONIST_WALK, &state->rng);
}

static void receptionist_init(GameplayState *state, Receptionist *receptionist,
                              float x, float y)
{
    *receptionist = (Receptionist){0};
    receptionist->x = x;
    receptionist->y = y;
    receptionist->post_x = x;
    receptionist->target_x = x;
    /* The counter is looked over, not turned away from: face whichever side
     * has room to stand in, so a desk backed against a wall still faces the
     * room rather than the masonry. */
    int left = receptionist_open_run(state, receptionist, -1);
    int right = receptionist_open_run(state, receptionist, 1);
    receptionist->desk_dir = right > left ? 1 : -1;
    receptionist->dir = receptionist->desk_dir;
    receptionist->anim_time = (float)rng_range(&state->rng, 628) * 0.01f;
    receptionist_set_activity(receptionist, RECEPTIONIST_DESK, &state->rng);
}

static void update_receptionist(GameplayState *state,
                                Receptionist *receptionist, float dt)
{
    switch (receptionist->activity)
    {
    case RECEPTIONIST_DESK:
        receptionist->dir = receptionist->glancing
                                ? -receptionist->desk_dir
                                : receptionist->desk_dir;
        receptionist->anim_time += dt * 1.1f;
        receptionist->glance_timer -= dt;
        if (receptionist->glance_timer <= 0.0f)
        {
            receptionist->glancing = !receptionist->glancing;
            receptionist->glance_timer =
                receptionist->glancing
                    ? RECEPTIONIST_GLANCE_TIME
                    : receptionist_roll(&state->rng, RECEPTIONIST_GLANCE_MIN,
                                        RECEPTIONIST_GLANCE_SPREAD);
        }
        receptionist->activity_timer -= dt;
        if (receptionist->activity_timer <= 0.0f)
            receptionist_begin_errand(state, receptionist);
        break;
    case RECEPTIONIST_WALK:
        receptionist->dir = receptionist->walk_dir;
        receptionist->anim_time += dt * 2.4f;
        if ((receptionist->target_x - receptionist->x) *
                (float)receptionist->walk_dir <= 0.0f)
            receptionist_arrive(state, receptionist, true);
        else if (receptionist->on_ground &&
                 !receptionist_can_step(state, receptionist,
                                        receptionist->walk_dir))
            receptionist_arrive(state, receptionist, false);
        else
            receptionist->vx =
                (float)receptionist->walk_dir * RECEPTIONIST_WALK_SPEED;
        break;
    case RECEPTIONIST_ERRAND:
        receptionist->anim_time += dt * 1.6f;
        receptionist->activity_timer -= dt;
        if (receptionist->activity_timer <= 0.0f)
            receptionist_head_home(state, receptionist);
        break;
    }

    receptionist->vy += GRAVITY * dt;
    if (receptionist->vy > MAX_FALL_SPEED)
        receptionist->vy = MAX_FALL_SPEED;
    float intended_vx = receptionist->vx;
    level_move(&state->level, &receptionist->x, &receptionist->y,
               &receptionist->vx, &receptionist->vy,
               RECEPTIONIST_W, RECEPTIONIST_H, dt, false,
               &receptionist->on_ground, false);
    /* Whatever the probes missed, being stopped dead by the map is the end of
     * the walk; standing there pushing into a wall is not a pose. */
    if (receptionist->activity == RECEPTIONIST_WALK && intended_vx != 0.0f &&
        receptionist->vx == 0.0f)
        receptionist_arrive(state, receptionist, false);
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
    {
        janitor_init(&state->janitors[i],
                     state->level.map.janitor_spawns[i].x,
                     state->level.map.janitor_spawns[i].y,
                     (i & 1) == 0, &state->rng);
        Janitor *janitor = &state->janitors[i];
        if (!janitor_cart_side_clear(state, janitor, janitor->cart_dir) &&
            janitor_cart_side_clear(state, janitor, -janitor->cart_dir))
        {
            janitor->dir = -janitor->dir;
            janitor->cart_dir = janitor->dir;
        }
    }
    state->civilian_count = state->level.map.civilian_count;
    for (int i = 0; i < state->civilian_count; ++i)
    {
        civilian_init(&state->civilians[i],
                      state->level.map.civilian_spawns[i].x,
                      state->level.map.civilian_spawns[i].y,
                      civilian_exit_x(state), i, &state->rng);
    }
    state->receptionist_count = state->level.map.receptionist_count;
    for (int i = 0; i < state->receptionist_count; ++i)
    {
        receptionist_init(state, &state->receptionists[i],
                          state->level.map.receptionist_spawns[i].x,
                          state->level.map.receptionist_spawns[i].y);
    }
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
    /* The announced bite: the crouch counts down only while the contact
     * holds. Stepping or jumping clear cancels the lunge entirely — that is
     * what makes the growl an answerable telegraph rather than a death rattle
     * played over a decided outcome. */
    if (dog->bite_windup > 0.0f || dog->bite_ready)
    {
        float player_h = state->player.crawling
                             ? (float)PLAYER_CRAWL_H
                             : (float)PLAYER_H;
        bool contact = !state->player.dying &&
                       gameplay_boxes_overlap(state->player.x,
                                              state->player.y,
                                              PLAYER_W, player_h,
                                              dog->x, dog->y,
                                              DOG_W, DOG_H);
        if (!contact)
        {
            dog->bite_windup = 0.0f;
            dog->bite_ready = false;
        }
        else if (dog->bite_windup > 0.0f)
        {
            dog->bite_windup -= dt;
            if (dog->bite_windup <= 0.0f)
            {
                dog->bite_windup = 0.0f;
                dog->bite_ready = true;
            }
        }
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
        speed = DOG_CHASE_SPEED * gameplay_enemy_speed_scale(state);
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

/*
 * The solo half of a conversation.
 *
 * A pair of guards standing together get the chat above; one on his own gets
 * the handset, because twelve men badged into a building under one contractor
 * name are a crew running a schedule, not twelve strangers. It reuses the chat
 * wholesale — the same standing beat, the same cooldown, the same cancellation
 * the moment anything happens — and is a solo talk purely by having no
 * partner, so every path that ends a chat already ends this too.
 */
static void update_radio_checks(GameplayState *state, float dt)
{
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead)
            continue;
        if (enemy->radio_timer > 0.0f)
        {
            enemy->radio_timer -= dt;
            continue;
        }
        /* Due, but the building has to be quiet and the man has to be free:
         * a guard hunting Chuck is not filing a routine report. */
        if (gameplay_alarm_active(state) || enemy->provoked ||
            enemy->raising_alarm || enemy->talking || enemy->climbing ||
            !enemy->on_ground || enemy->talk_partner != -1 ||
            enemy->talk_cooldown > 0.0f || enemy->investigate_timer > 0.0f ||
            enemy->aim_timer > 0.0f)
        {
            /* Try again shortly rather than firing the instant the guard is
             * free, which would put the call right on the heels of whatever
             * interrupted it. */
            enemy->radio_timer = 2.5f + (float)rng_range(&state->rng, 300) *
                                            0.01f;
            continue;
        }

        enemy->talking = true;
        enemy->talk_timer = ENEMY_RADIO_DURATION;
        enemy->vx = 0.0f;
        enemy->radio_timer =
            ENEMY_RADIO_GAP_MIN +
            (ENEMY_RADIO_GAP_MAX - ENEMY_RADIO_GAP_MIN) * rng_unit(&state->rng);
        gameplay_world_sound(state, SFX_GUARD_RADIO,
                             enemy->x + ENEMY_W * 0.5f,
                             enemy->y + ENEMY_H * 0.5f);
        gameplay_crew_chatter(state, CHATTER_RADIO, i,
                              enemy->x + ENEMY_W * 0.5f,
                              enemy->y + ENEMY_H * 0.5f);
    }
}

static void update_conversations(GameplayState *state, float dt)
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
                /* Credited to the man on the left, who is the one facing the
                 * way the sector is read. Whichever it is, the pair are stood
                 * a body's width apart and the caption names one of them. */
                gameplay_crew_chatter(state, CHATTER_TALK,
                                      (int)(left - state->enemies),
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

    /* After the pairing pass, so whoever is left over is genuinely alone. */
    update_radio_checks(state, dt);
}

void gameplay_ai_update_movement(GameplayState *state, float dt)
{
    for (int i = 0; i < state->janitor_count; ++i)
        update_janitor(state, &state->janitors[i], dt);
    for (int i = 0; i < state->civilian_count; ++i)
        update_civilian(state, &state->civilians[i], dt);
    for (int i = 0; i < state->receptionist_count; ++i)
        update_receptionist(state, &state->receptionists[i], dt);

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead)
        {
            settle_body(state, &enemy->x, &enemy->y, &enemy->vy,
                        ENEMY_W, ENEMY_H, dt);
            continue;
        }
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
                        /* The one line the player is guaranteed to read,
                         * because they caused it. It goes off the switch
                         * rather than off the man: he is at arm's length from
                         * it and the switch is what the eye is on. */
                        gameplay_crew_chatter(state, CHATTER_ALARM, i,
                                              switch_x, switch_y);
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
            actor_or_tile_blocks_side(state, i, -1) &&
            actor_or_tile_blocks_side(state, i, 1);
        enemy_update(enemy, &state->level, dt, pursuing, alarm_pursuit,
                     enemy->pursuit_target_x, enemy->pursuit_target_y,
                     hemmed_in, gameplay_enemy_speed_scale(state),
                     &state->rng);
        gameplay_resolve_enemy_crates(state, enemy, previous_y);
    }

    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (dog->dead)
        {
            settle_body(state, &dog->x, &dog->y, &dog->vy, DOG_W, DOG_H, dt);
            continue;
        }
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
    update_conversations(state, dt);
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
        /* A chat is a distraction: the pair only notice Chuck up close, and
         * they stop talking when he gets there whether or not either of them
         * then turns. A radio check is not a distraction — the man is on his
         * own post facing his own corridor with a handset up — so it is read
         * here exactly as an empty-handed guard is, the same rule perception
         * already follows. Blinding him for it would quietly turn a piece of
         * colour into a stealth window. */
        if (enemy->talking && !enemy_on_radio(enemy))
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
            /* The handset comes down before he turns: a guard shooting while
             * still posed mid-call is the one way the beat could contradict
             * itself. */
            if (enemy->talking)
            {
                enemy->talking = false;
                enemy->talk_timer = 0.0f;
                if (enemy->talk_cooldown <= 0.0f)
                    enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
            }
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
            bullet->x = enemy_x - BULLET_H * 0.5f;
            bullet->y = enemy->aim_vdir < 0
                            ? enemy->y - BULLET_W
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
        /* The suspicion ramp: an unbroken sight line has to be held for a
         * beat before the aim telegraph may start, so a fresh sighting never
         * fires below reaction time. Guards already provoked, and every guard
         * under an active alarm, are past noticing. */
        if (enemy_has_los(state, enemy))
            enemy->sight_timer += dt;
        else
            enemy->sight_timer = 0.0f;
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
        if (!enemy->provoked && !gameplay_alarm_active(state) &&
            enemy->sight_timer < ENEMY_NOTICE_TIME)
            continue;
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
