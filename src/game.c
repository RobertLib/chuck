#include "game.h"
#include "embedded_levels.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

static bool boxes_overlap(float ax, float ay, float aw, float ah,
                          float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

static bool box_tiles_clear(const Game *game, float x, float y,
                            float w, float h);

/*
 * Probe far enough to cover an enemy's largest possible horizontal step.
 * The main loop caps dt at 0.05 s, so the undamaged patrol speed can move a
 * little over three pixels in one update.
 */
#define ENEMY_SIDE_PROBE 4.0f

static bool enemy_side_blocked(const Game *game, int enemy_index, int dir)
{
    const Enemy *enemy = &game->enemies[enemy_index];
    float probe_x =
        dir < 0 ? enemy->x - ENEMY_SIDE_PROBE : enemy->x + ENEMY_W;
    float probe_y = enemy->y + 1.0f;
    float probe_h = ENEMY_H - 2.0f;

    if (!box_tiles_clear(game, probe_x, probe_y,
                         ENEMY_SIDE_PROBE, probe_h))
    {
        return true;
    }

    for (int i = 0; i < game->level.crate_count; ++i)
    {
        const Crate *crate = &game->level.crates[i];
        if (crate->active &&
            boxes_overlap(probe_x, probe_y, ENEMY_SIDE_PROBE, probe_h,
                          crate->x, crate->y, CRATE_W, CRATE_H))
        {
            return true;
        }
    }

    for (int i = 0; i < game->dog_count; ++i)
    {
        const Dog *dog = &game->dogs[i];
        if (!dog->dead &&
            boxes_overlap(probe_x, probe_y, ENEMY_SIDE_PROBE, probe_h,
                          dog->x, dog->y, DOG_W, DOG_H))
        {
            return true;
        }
    }

    for (int i = 0; i < game->enemy_count; ++i)
    {
        const Enemy *other = &game->enemies[i];
        if (i != enemy_index && !other->dead &&
            boxes_overlap(probe_x, probe_y, ENEMY_SIDE_PROBE, probe_h,
                          other->x, other->y, ENEMY_W, ENEMY_H))
        {
            return true;
        }
    }

    return false;
}

static void play_world_sound(Game *game, SoundEffect effect,
                             float source_x, float source_y)
{
    float player_height =
        game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    audio_play_at(&game->audio, effect, source_x, source_y,
                  game->player.x + PLAYER_W * 0.5f,
                  game->player.y + player_height * 0.5f);
}

/*
 * Being shot provokes a surviving guard regardless of distance. The guard
 * immediately tries a return shot, then keeps pursuing the position from
 * which the player attacked until normal line of sight lets it track them.
 * If the guard was talking, its partner is alerted as well.
 */
static void provoke_enemy_after_hit(Game *game, int enemy_index)
{
    if (enemy_index < 0 || enemy_index >= game->enemy_count)
        return;

    Enemy *attacked = &game->enemies[enemy_index];
    int participant_count = attacked->talking ? 2 : 1;
    int participants[2] = {enemy_index, attacked->talk_partner};
    float target_x = game->player.x + PLAYER_W * 0.5f;
    float target_y =
        game->player.y +
        (game->player.crawling
             ? (float)PLAYER_CRAWL_H * 0.45f
             : (float)PLAYER_H * 0.15f);
    Enemy *alert_source = NULL;

    for (int i = 0; i < participant_count; ++i)
    {
        int index = participants[i];
        if (index < 0 || index >= game->enemy_count ||
            (i > 0 && index == participants[0]))
        {
            continue;
        }

        Enemy *enemy = &game->enemies[index];
        enemy->talking = false;
        enemy->talk_timer = 0.0f;
        enemy->talk_partner = -1;
        enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;

        if (enemy->dead)
            continue;

        float enemy_x = enemy->x + ENEMY_W * 0.5f;
        enemy->provoked = true;
        enemy->pursuit_target_x = target_x;
        enemy->pursuit_target_y = target_y;
        enemy->has_pursuit_target = true;
        enemy->dir = target_x < enemy_x ? -1 : 1;
        enemy->aim_target_x = target_x;
        enemy->aim_target_y = target_y;
        enemy->aim_timer = ENEMY_AIM_TIME;
        if (alert_source == NULL)
            alert_source = enemy;
    }

    if (alert_source != NULL)
    {
        play_world_sound(game, SFX_ENEMY_ALERT,
                         alert_source->x + ENEMY_W * 0.5f,
                         alert_source->y + ENEMY_H * 0.5f);
    }
}

static bool crate_position_clear(const Game *game, int crate_index, float x, float y)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + CRATE_W - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + CRATE_H - 1.0f) / TILE_SIZE);

    for (int row = top; row <= bottom; ++row)
    {
        for (int col = left; col <= right; ++col)
        {
            if (level_is_solid(&game->level, col, row))
                return false;
        }
    }

    for (int i = 0; i < game->level.crate_count; ++i)
    {
        if (i == crate_index)
            continue;
        const Crate *other = &game->level.crates[i];
        if (!other->active)
            continue;
        if (boxes_overlap(x, y, CRATE_W, CRATE_H,
                          other->x, other->y, CRATE_W, CRATE_H))
            return false;
    }

    return true;
}

static float move_crate_x(Game *game, int crate_index, float dx)
{
    Crate *crate = &game->level.crates[crate_index];
    if (!crate->active || dx == 0.0f)
        return 0.0f;

    float moved = 0.0f;
    float remaining = fabsf(dx);
    float sign = (dx > 0.0f) ? 1.0f : -1.0f;
    while (remaining > 0.0f)
    {
        float step = fminf(remaining, 1.0f) * sign;
        if (!crate_position_clear(game, crate_index, crate->x + step, crate->y))
            break;
        crate->x += step;
        moved += step;
        remaining -= fabsf(step);
    }
    return moved;
}

static float move_crate_y(Game *game, int crate_index, float dy)
{
    Crate *crate = &game->level.crates[crate_index];
    if (!crate->active || dy == 0.0f)
        return 0.0f;

    float moved = 0.0f;
    float remaining = fabsf(dy);
    float sign = (dy > 0.0f) ? 1.0f : -1.0f;
    while (remaining > 0.0f)
    {
        float step = fminf(remaining, 1.0f) * sign;
        if (!crate_position_clear(game, crate_index, crate->x, crate->y + step))
            break;
        crate->y += step;
        moved += step;
        remaining -= fabsf(step);
    }
    return moved;
}

static void destroy_crate(Game *game, Crate *crate)
{
    if (!crate->active)
        return;

    float cx = crate->x + CRATE_W * 0.5f;
    float cy = crate->y + CRATE_H * 0.5f;
    crate->active = false;
    particle_system_explosion(&game->particles, cx, cy, 18);
    play_world_sound(game, SFX_CRATE_BREAK, cx, cy);
    game->score += 20;
}

static void kill_enemy_with_crate(Game *game, Enemy *enemy)
{
    float cx = enemy->x + ENEMY_W * 0.5f;
    float cy = enemy->y + ENEMY_H * 0.5f;

    enemy->hp = 0;
    enemy->dead = true;
    particle_system_emit(&game->particles, cx, cy, 24, enemy->dir);
    play_world_sound(game, SFX_ENEMY_DOWN, cx, cy);
    game->score += 150;
}

static void kill_dog_with_crate(Game *game, Dog *dog)
{
    float cx = dog->x + DOG_W * 0.5f;
    float cy = dog->y + DOG_H * 0.5f;

    dog->hp = 0;
    dog->dead = true;
    particle_system_emit(&game->particles, cx, cy, 14, dog->dir);
    play_world_sound(game, SFX_DOG_YELP, cx, cy);
    game->score += 75;
}

/*
 * Kill enemies crossed by the crate's downward-facing edge. Checking the
 * whole swept distance prevents a fast crate from skipping an enemy between
 * frames, while the previous-bottom test keeps sideways pushes harmless.
 */
static void resolve_falling_crate_hits(Game *game, const Crate *crate,
                                       float previous_y)
{
    float previous_bottom = previous_y + CRATE_H;
    float current_bottom = crate->y + CRATE_H;

    if (current_bottom <= previous_bottom)
        return;

    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *enemy = &game->enemies[i];
        if (enemy->dead)
            continue;
        if (crate->x >= enemy->x + ENEMY_W ||
            crate->x + CRATE_W <= enemy->x)
            continue;
        if (previous_bottom <= enemy->y &&
            current_bottom >= enemy->y)
        {
            kill_enemy_with_crate(game, enemy);
        }
    }

    for (int i = 0; i < game->dog_count; ++i)
    {
        Dog *dog = &game->dogs[i];
        if (dog->dead)
            continue;
        if (crate->x >= dog->x + DOG_W ||
            crate->x + CRATE_W <= dog->x)
            continue;
        if (previous_bottom <= dog->y &&
            current_bottom >= dog->y)
        {
            kill_dog_with_crate(game, dog);
        }
    }
}

static void update_crates(Game *game, float dt)
{
    for (int i = 0; i < game->level.crate_count; ++i)
    {
        Crate *crate = &game->level.crates[i];
        if (!crate->active)
            continue;

        crate->vy += GRAVITY * dt;
        if (crate->vy > MAX_FALL_SPEED)
            crate->vy = MAX_FALL_SPEED;

        float desired_x = crate->vx * dt;
        float moved_x = move_crate_x(game, i, desired_x);
        if (fabsf(moved_x - desired_x) > 0.01f)
            crate->vx = 0.0f;

        crate->on_ground = false;
        float previous_y = crate->y;
        float desired_y = crate->vy * dt;
        float moved_y = move_crate_y(game, i, desired_y);
        resolve_falling_crate_hits(game, crate, previous_y);
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

static void resolve_player_crates(Game *game, float prev_x, float prev_y, float prev_h)
{
    Player *player = &game->player;
    float ph = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;

    for (int i = 0; i < game->level.crate_count; ++i)
    {
        Crate *crate = &game->level.crates[i];
        if (!crate->active)
            continue;
        if (!boxes_overlap(player->x, player->y, PLAYER_W, ph,
                           crate->x, crate->y, CRATE_W, CRATE_H))
            continue;

        if (prev_y + prev_h <= crate->y + 2.0f && player->vy >= 0.0f)
        {
            player->y = crate->y - ph;
            player->vy = 0.0f;
            player->on_ground = true;
            continue;
        }

        if (prev_y >= crate->y + CRATE_H - 2.0f && player->vy < 0.0f)
        {
            player->y = crate->y + CRATE_H;
            player->vy = 0.0f;
            continue;
        }

        int dir = 0;
        if (prev_x + PLAYER_W <= crate->x + 2.0f)
            dir = 1;
        else if (prev_x >= crate->x + CRATE_W - 2.0f)
            dir = -1;
        else
            dir = (player->x + PLAYER_W * 0.5f < crate->x + CRATE_W * 0.5f) ? 1 : -1;

        if ((dir > 0 && player->vx > 0.0f) || (dir < 0 && player->vx < 0.0f))
        {
            float penetration = (dir > 0)
                                    ? (player->x + PLAYER_W) - crate->x
                                    : (crate->x + CRATE_W) - player->x;
            float moved = move_crate_x(game, i, dir * (penetration + 0.5f));
            if (fabsf(moved) > 0.0f)
                crate->vx = (float)dir * CRATE_PUSH_SPEED;
        }

        if (boxes_overlap(player->x, player->y, PLAYER_W, ph,
                          crate->x, crate->y, CRATE_W, CRATE_H))
        {
            if (dir > 0)
                player->x = crate->x - PLAYER_W;
            else
                player->x = crate->x + CRATE_W;
            player->vx = 0.0f;
        }
    }
}

static void resolve_enemy_crates(Game *game, Enemy *enemy, float prev_x, float prev_y)
{
    if (enemy->dead)
        return;

    for (int i = 0; i < game->level.crate_count; ++i)
    {
        Crate *crate = &game->level.crates[i];
        if (!crate->active)
            continue;
        if (!boxes_overlap(enemy->x, enemy->y, ENEMY_W, ENEMY_H,
                           crate->x, crate->y, CRATE_W, CRATE_H))
            continue;

        if (prev_y + ENEMY_H <= crate->y + 2.0f && enemy->vy >= 0.0f)
        {
            enemy->y = crate->y - ENEMY_H;
            enemy->vy = 0.0f;
            enemy->on_ground = true;
            continue;
        }

        if (prev_x + ENEMY_W * 0.5f < crate->x + CRATE_W * 0.5f)
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

static void resolve_dog_crates(Game *game, Dog *dog, float prev_x, float prev_y)
{
    if (dog->dead)
        return;

    for (int i = 0; i < game->level.crate_count; ++i)
    {
        Crate *crate = &game->level.crates[i];
        if (!crate->active)
            continue;
        if (!boxes_overlap(dog->x, dog->y, DOG_W, DOG_H,
                           crate->x, crate->y, CRATE_W, CRATE_H))
            continue;

        if (prev_y + DOG_H <= crate->y + 2.0f && dog->vy >= 0.0f)
        {
            dog->y = crate->y - DOG_H;
            dog->vy = 0.0f;
            dog->on_ground = true;
            continue;
        }

        if (prev_x + DOG_W * 0.5f < crate->x + CRATE_W * 0.5f)
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

static bool crate_blocks_row(const Game *game, float ax, float bx, int row)
{
    float left = fminf(ax, bx);
    float width = fabsf(bx - ax);
    float y = row * (float)TILE_SIZE + TILE_SIZE * 0.5f;

    for (int i = 0; i < game->level.crate_count; ++i)
    {
        const Crate *crate = &game->level.crates[i];
        if (!crate->active)
            continue;
        if (boxes_overlap(left, y, width, 1.0f,
                          crate->x, crate->y, CRATE_W, CRATE_H))
            return true;
    }
    return false;
}

static bool box_tiles_clear(const Game *game, float x, float y, float w, float h)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + w - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + h - 1.0f) / TILE_SIZE);
    for (int r = top; r <= bottom; ++r)
    {
        for (int c = left; c <= right; ++c)
        {
            if (level_is_solid(&game->level, c, r))
                return false;
        }
    }
    return true;
}

/* Find a free slot in the enemies array: reuse a dead slot or extend the array. */
static int find_enemy_slot(Game *game)
{
    for (int i = 0; i < game->enemy_count; ++i)
    {
        if (game->enemies[i].dead)
        {
            for (int d = 0; d < game->dog_count; ++d)
            {
                if (!game->dogs[d].dead && game->dogs[d].owner == i)
                    game->dogs[d].owner = -1;
            }
            return i;
        }
    }
    if (game->enemy_count < MAX_ENEMIES)
        return game->enemy_count++;
    return -1;
}

static int find_dog_slot(Game *game)
{
    for (int i = 0; i < game->dog_count; ++i)
        if (game->dogs[i].dead)
            return i;
    if (game->dog_count < MAX_DOGS)
        return game->dog_count++;
    return -1;
}

static int find_grenade_slot(Game *game)
{
    for (int i = 0; i < game->grenade_count; ++i)
        if (!game->grenades[i].active)
            return i;
    if (game->grenade_count < MAX_GRENADES)
        return game->grenade_count++;
    return -1;
}

/*
 * Returns true when the enemy has an unobstructed horizontal view of the
 * player: same floor level (within 1 tile of Y), within ENEMY_SHOOT_RANGE,
 * and no solid tile along the row between them.
 */
static bool enemy_has_los(const Game *game, const Enemy *e)
{
    const Player *p = &game->player;
    float ex = e->x + ENEMY_W * 0.5f;
    float ey = e->y + ENEMY_H * 0.5f;
    float px = p->x + PLAYER_W * 0.5f;
    float ph = p->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    float py = p->y + ph * 0.5f;

    /* If the enemy is currently talking to another enemy, they will ignore
     * the player unless the player comes very close. */
    if (e->talking)
    {
        float dx = px - ex;
        float dy = py - ey;
        if (fabsf(dx) > (float)ENEMY_TALK_NOTICE_RADIUS || fabsf(dy) > (float)ENEMY_TALK_NOTICE_RADIUS)
            return false;
    }

    if (fabsf(px - ex) > (float)ENEMY_SHOOT_RANGE)
        return false;
    if (fabsf(py - ey) > (float)TILE_SIZE * 1.2f)
        return false;

    /* Enemy must be facing toward the player */
    float dx = px - ex;
    if ((dx > 0.0f && e->dir < 0) || (dx < 0.0f && e->dir > 0))
        return false;

    /* Scan all tile columns between enemy and player at enemy mid-row */
    int row = (int)floorf(ey / TILE_SIZE);
    int col0 = (int)floorf(fminf(ex, px) / TILE_SIZE);
    int col1 = (int)floorf(fmaxf(ex, px) / TILE_SIZE);
    for (int c = col0; c <= col1; ++c)
    {
        if (level_is_solid(&game->level, c, row))
            return false;
    }
    if (crate_blocks_row(game, ex, px, row))
        return false;
    return true;
}

static bool horizontal_los_clear(const Game *game, float ax, float bx, float y)
{
    int row = (int)floorf(y / TILE_SIZE);
    int col0 = (int)floorf(fminf(ax, bx) / TILE_SIZE);
    int col1 = (int)floorf(fmaxf(ax, bx) / TILE_SIZE);
    for (int c = col0; c <= col1; ++c)
    {
        if (level_is_solid(&game->level, c, row))
            return false;
    }
    return !crate_blocks_row(game, ax, bx, row);
}

static bool terminal_alarm_target(const Game *game,
                                  float *target_x, float *target_y)
{
    int index = game->level.active_terminal_index;
    if (index < 0 || index >= game->level.terminal_count)
        return false;

    const Terminal *terminal = &game->level.terminals[index];
    *target_x = (terminal->col + 0.5f) * (float)TILE_SIZE;
    *target_y = (terminal->row + 0.5f) * (float)TILE_SIZE;
    return true;
}

static bool dog_has_floor_ahead(const Game *game, const Dog *dog, int dir)
{
    float probe_x = (dir > 0) ? dog->x + DOG_W + 3.0f : dog->x - 3.0f;
    int col = (int)floorf(probe_x / TILE_SIZE);
    int row = (int)floorf((dog->y + DOG_H + 2.0f) / TILE_SIZE);
    if (level_is_solid(&game->level, col, row) || level_is_ladder(&game->level, col, row))
        return true;
    for (int i = 0; i < game->level.fall_platform_count; ++i)
    {
        const FallPlatform *fp = &game->level.fall_platforms[i];
        if (!fp->removed && fp->col == col && fabsf(fp->y - (float)row * TILE_SIZE) < 3.0f)
            return true;
    }
    for (int i = 0; i < game->level.moving_platform_count; ++i)
    {
        const MovingPlatform *mp = &game->level.moving_platforms[i];
        int plat_col = (int)floorf(mp->x / TILE_SIZE);
        if (mp->row == row && plat_col == col)
            return true;
    }
    return false;
}

static bool dog_can_jump_gap(const Game *game, const Dog *dog, int dir)
{
    if (!dog->on_ground)
        return false;

    int feet_row = (int)floorf((dog->y + DOG_H + 2.0f) / TILE_SIZE);
    int front_col = (dir > 0)
                        ? (int)floorf((dog->x + DOG_W + 3.0f) / TILE_SIZE)
                        : (int)floorf((dog->x - 3.0f) / TILE_SIZE);

    for (int gap = 1; gap <= DOG_JUMP_MAX_GAP_TILES; ++gap)
    {
        int landing_col = front_col + dir * gap;
        if (!level_is_solid(&game->level, landing_col, feet_row) &&
            !level_is_ladder(&game->level, landing_col, feet_row))
        {
            continue;
        }

        float landing_x = landing_col * (float)TILE_SIZE + (TILE_SIZE - DOG_W) * 0.5f;
        float top_y = dog->y - (float)TILE_SIZE * 0.8f;
        if (box_tiles_clear(game, landing_x, dog->y, DOG_W, DOG_H) &&
            box_tiles_clear(game, landing_x, top_y, DOG_W, DOG_H))
        {
            return true;
        }
    }

    return false;
}

static void dog_avoid_edge(Dog *dog, int dir)
{
    dog->dir = -dir;
    dog->state = DOG_ROAM;
    dog->roam_target_x = dog->x - (float)dir * (DOG_HANDLER_DISTANCE + 24.0f);
    dog->state_timer = 0.7f + SDL_rand(90) * 0.01f;
}

static bool dog_sees_player(const Game *game, const Dog *dog)
{
    const Player *p = &game->player;
    float dcx = dog->x + DOG_W * 0.5f;
    float dcy = dog->y + DOG_H * 0.5f;
    float ph = p->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    float pcx = p->x + PLAYER_W * 0.5f;
    float pcy = p->y + ph * 0.5f;
    float dx = pcx - dcx;

    if (fabsf(pcy - dcy) > (float)TILE_SIZE * 0.9f)
        return false;
    if (fabsf(dx) > (float)DOG_VIEW_RANGE)
        return false;
    if ((dx > 0.0f && dog->dir < 0) || (dx < 0.0f && dog->dir > 0))
    {
        if (fabsf(dx) > (float)DOG_BACK_SENSE_RANGE)
            return false;
    }
    return horizontal_los_clear(game, dcx, pcx, dcy);
}

static void spawn_dog_for_enemy(Game *game, int enemy_index)
{
    if (enemy_index < 0 || enemy_index >= game->enemy_count)
        return;

    int slot = find_dog_slot(game);
    if (slot < 0)
        return;

    const Enemy *handler = &game->enemies[enemy_index];
    float base_x = handler->x + ENEMY_W * 0.5f - DOG_W * 0.5f;
    float sy = handler->y + ENEMY_H - DOG_H;
    float sx = base_x - (float)handler->dir * DOG_HANDLER_DISTANCE;
    if (!box_tiles_clear(game, sx, sy, DOG_W, DOG_H))
        sx = base_x + (float)handler->dir * DOG_HANDLER_DISTANCE;
    if (!box_tiles_clear(game, sx, sy, DOG_W, DOG_H))
        sx = base_x;
    dog_init(&game->dogs[slot], sx, sy, enemy_index);
}

static bool spawn_enemy_from_door(Game *game, int door_index)
{
    if (door_index < 0 || door_index >= game->level.door_count)
        return false;

    const Door *door = &game->level.doors[door_index];
    float door_x = door->col * (float)TILE_SIZE;
    float door_y = door->row * (float)TILE_SIZE;
    float player_h =
        game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;

    /* A delayed spawn is retried instead of placing a guard on the player. */
    if (boxes_overlap(game->player.x, game->player.y,
                      PLAYER_W, player_h,
                      door_x, door_y,
                      (float)TILE_SIZE, (float)TILE_SIZE))
    {
        return false;
    }

    float spawn_x =
        door_x + (TILE_SIZE - ENEMY_W) * 0.5f;
    float spawn_y =
        (door->row + 1) * (float)TILE_SIZE - ENEMY_H;
    for (int i = 0; i < game->enemy_count; ++i)
    {
        const Enemy *enemy = &game->enemies[i];
        if (!enemy->dead &&
            boxes_overlap(spawn_x, spawn_y, ENEMY_W, ENEMY_H,
                          enemy->x, enemy->y, ENEMY_W, ENEMY_H))
        {
            return false;
        }
    }

    int slot = find_enemy_slot(game);
    if (slot < 0)
        return false;

    enemy_init(&game->enemies[slot], spawn_x, spawn_y);
    if (SDL_rand(100) < DOG_DOOR_HANDLER_CHANCE)
        spawn_dog_for_enemy(game, slot);
    play_world_sound(game, SFX_DOOR,
                     spawn_x + ENEMY_W * 0.5f,
                     spawn_y + ENEMY_H * 0.5f);
    return true;
}

static float random_reinforcement_delay(float minimum, float maximum)
{
    float unit = (float)SDL_rand(10001) / 10000.0f;
    return minimum + (maximum - minimum) * unit;
}

static void update_terminal_reinforcements(Game *game, float dt)
{
    if (game->terminal_alarm_timer <= 0.0f ||
        game->terminal_reinforcements_pending <= 0 ||
        game->level.door_count <= 0)
    {
        return;
    }

    game->terminal_reinforcement_timer -= dt;
    if (game->terminal_reinforcement_timer > 0.0f)
        return;

    /* Start at a random door, then try the others if that one is occupied.
     * This keeps both arrival time and entry point unpredictable. */
    int first_door = SDL_rand(game->level.door_count);
    bool spawned = false;
    for (int offset = 0; offset < game->level.door_count; ++offset)
    {
        int door_index =
            (first_door + offset) % game->level.door_count;
        if (spawn_enemy_from_door(game, door_index))
        {
            spawned = true;
            break;
        }
    }

    if (!spawned)
    {
        /* All doors may be occupied or the enemy array may be full. */
        game->terminal_reinforcement_timer =
            0.35f + SDL_rand(46) * 0.01f;
        return;
    }

    game->terminal_reinforcements_pending--;
    if (game->terminal_reinforcements_pending > 0)
    {
        game->terminal_reinforcement_timer =
            random_reinforcement_delay(
                TERMINAL_REINFORCEMENT_GAP_MIN,
                TERMINAL_REINFORCEMENT_GAP_MAX);
    }
    else
    {
        game->terminal_reinforcement_timer = 0.0f;
    }
}

static float dog_anchor_x(const Dog *dog, const Enemy *handler)
{
    if (!handler)
        return dog->guard_x;
    return handler->x + ENEMY_W * 0.5f - DOG_W * 0.5f - (float)handler->dir * DOG_HANDLER_DISTANCE;
}

static void dog_pick_roam_target(Dog *dog, float anchor_x)
{
    float offset = (float)(SDL_rand((int)(DOG_ROAM_RADIUS * 2.0f)) - (int)DOG_ROAM_RADIUS);
    if (fabsf(offset) < DOG_HANDLER_DISTANCE)
        offset = (offset < 0.0f) ? -DOG_HANDLER_DISTANCE : DOG_HANDLER_DISTANCE;
    dog->roam_target_x = anchor_x + offset;
    dog->state_timer = 0.8f + SDL_rand(120) * 0.01f;
}

static void dog_update_ai(Game *game, Dog *dog, float dt)
{
    if (dog->dead)
        return;

    dog->vocal_timer -= dt;

    if (dog->attack_timer > 0.0f)
    {
        dog->attack_timer -= dt;
        if (dog->attack_timer < 0.0f)
            dog->attack_timer = 0.0f;
    }
    dog->anim_time += dt * (2.0f + fabsf(dog->vx) * 0.032f);

    Enemy *handler = NULL;
    if (dog->owner >= 0 && dog->owner < game->enemy_count)
    {
        handler = &game->enemies[dog->owner];
        if (handler->dead)
        {
            dog->owner = -1;
            handler = NULL;
        }
    }

    if (handler)
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

    bool sees_player = dog_sees_player(game, dog);
    if (sees_player)
    {
        dog->chase_target_x =
            game->player.x + PLAYER_W * 0.5f;
        dog->has_chase_target = true;
        dog->state = DOG_CHASE;
        dog->lost_timer = DOG_LOST_TIME;
    }
    else if (game->terminal_alarm_timer > 0.0f)
    {
        if (!dog->has_chase_target)
        {
            float ignored_y;
            if (terminal_alarm_target(
                    game, &dog->chase_target_x, &ignored_y))
            {
                dog->has_chase_target = true;
            }
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
    {
        dog->has_chase_target = false;
    }

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
        float dist_from_anchor = fabsf((dog->x + DOG_W * 0.5f) - (anchor + DOG_W * 0.5f));
        if (dist_from_anchor > DOG_RETURN_RADIUS)
            dog->state = DOG_RETURN;

        if (dog->state == DOG_RETURN)
        {
            target_x = anchor;
            speed = DOG_RETURN_SPEED;
            wants_move = fabsf(target_x - dog->x) > 10.0f;
            if (!wants_move)
            {
                dog->state = DOG_GUARD;
                dog->state_timer = 0.6f + SDL_rand(120) * 0.01f;
            }
        }
        else if (dog->state == DOG_ROAM)
        {
            target_x = dog->roam_target_x;
            wants_move = fabsf(target_x - dog->x) > 8.0f;
            dog->state_timer -= dt;
            if (!wants_move || dog->state_timer <= 0.0f)
            {
                dog->state = DOG_RETURN;
            }
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
            else if (dog->state_timer <= 0.0f && SDL_rand(100) < 45)
            {
                dog->state = DOG_ROAM;
                dog_pick_roam_target(dog, anchor);
                target_x = dog->roam_target_x;
                wants_move = true;
            }
            else if (dog->state_timer <= 0.0f)
            {
                dog->state_timer = 0.5f + SDL_rand(120) * 0.01f;
            }
        }
    }

    dog->vx = 0.0f;
    if (wants_move)
    {
        int dir = (target_x > dog->x) ? 1 : -1;
        dog->dir = dir;
        if (!dog->on_ground || dog_has_floor_ahead(game, dog, dir))
        {
            dog->vx = (float)dir * speed;
        }
        else if (dog_can_jump_gap(game, dog, dir))
        {
            dog->vx = (float)dir * fmaxf(speed, DOG_JUMP_MIN_SPEED);
            dog->vy = -DOG_JUMP_SPEED;
            dog->on_ground = false;
        }
        else
        {
            dog_avoid_edge(dog, dir);
        }
    }

    float prev_vx = dog->vx;
    level_move(&game->level, &dog->x, &dog->y, &dog->vx, &dog->vy,
               DOG_W, DOG_H, dt, false, &dog->on_ground, true);
    if (fabsf(prev_vx) > 0.0f && fabsf(dog->vx) < 0.1f && dog->state != DOG_CHASE)
        dog->state = DOG_RETURN;
}

static bool load_level(Game *game, int index)
{
    if (index < 0 || (size_t)index >= EMBEDDED_LEVEL_COUNT)
    {
        SDL_Log("Level index %d is out of range", index);
        return false;
    }

    const EmbeddedLevelData *source = &EMBEDDED_LEVELS[index];
    if (!level_load_data(&game->level, source->name,
                         source->data, source->size))
    {
        return false;
    }
    game->current_level = index;
    audio_play_music(&game->audio,
                     (MusicTrack)(MUSIC_LEVEL_ONE + index));

    player_reset(&game->player, &game->level);

    /* Start with no active enemies/mines: we'll reveal the level first and
     * spawn entities after the tile-reveal animation completes. */
    game->enemy_count = 0;
    game->dog_count = 0;
    game->mine_count = 0;

    game->invuln_timer = 0.0f;
    game->message_timer = 0.0f;
    game->exit_unlocked_timer = 0.0f;
    game->level_elapsed_time = 0.0f;
    game->level_start_score = game->score;
    game->state = STATE_LEVEL_START;
    level_reveal_init(&game->level);

    /* Initialise grenades state */
    game->grenade_count = 0;
    for (int i = 0; i < MAX_GRENADES; ++i)
    {
        game->grenades[i].active = false;
        game->grenades[i].timer = 0.0f;
        game->grenades[i].grounded = false;
        game->grenades[i].vx = 0.0f;
        game->grenades[i].vy = 0.0f;
    }

    /* Initialise per-door state */
    for (int i = 0; i < game->level.door_count; ++i)
    {
        game->door_spawns[i] = game->level.door_spawn_counts[i];
        /* Stagger initial spawn times so doors don't all fire at once */
        game->door_timers[i] = DOOR_SPAWN_INTERVAL * (0.4f + SDL_rand(60) * 0.01f);
    }
    game->teleport_cooldown = 0.0f;

    game->player_on_elevator = -1;
    game->player_on_moving_platform = -1;
    game->footstep_audio_timer = 0.0f;
    game->ladder_audio_timer = 0.0f;
    game->footstep_alternate = false;
    game->terminal_hack_progress = 0.0f;
    game->terminal_hack_tick_timer = 0.0f;
    game->terminal_in_range = false;
    game->terminal_hacking = false;
    game->terminal_alarm_timer = 0.0f;
    game->terminal_reinforcement_timer = 0.0f;
    game->terminal_reinforcements_pending = 0;
    SDL_memset(game->fall_platform_sounded, 0,
               sizeof(game->fall_platform_sounded));

    SDL_memset(game->bullets, 0, sizeof(game->bullets));
    SDL_memset(game->enemy_bullets, 0, sizeof(game->enemy_bullets));

    /* Keep the existing window size; initialise camera to centre on player. */
    int win_w = 0, win_h = 0;
    game_get_view_size(game, &win_w, &win_h);
    float max_cam = game->level.width * (float)TILE_SIZE - (float)win_w;
    if (max_cam < 0.0f)
        max_cam = 0.0f;
    float desired = game->player.x + PLAYER_W * 0.5f - (float)win_w * 0.5f;
    if (desired < 0.0f)
        desired = 0.0f;
    if (desired > max_cam)
        desired = max_cam;
    game->cam_x = desired;

    return true;
}

/* Spawn enemies/mines/etc after the level reveal animation completes. */
static void spawn_level_entities_after_reveal(Game *game)
{
    /* Initialise enemies from level spawn points */
    game->enemy_count = game->level.enemy_count;
    for (int i = 0; i < game->enemy_count; ++i)
    {
        enemy_init(&game->enemies[i],
                   game->level.enemy_spawns[i].x,
                   game->level.enemy_spawns[i].y);
        if (game->level.enemy_spawns[i].has_dog)
            spawn_dog_for_enemy(game, i);
    }

    /* Initialise mines from level spawns */
    game->mine_count = game->level.mine_count;
    for (int i = 0; i < game->mine_count; ++i)
    {
        game->mines[i].x = game->level.mine_spawns[i].x;
        game->mines[i].y = game->level.mine_spawns[i].y;
        game->mines[i].active = true;
        game->mines[i].triggered = false;
        game->mines[i].timer = 0.0f;
    }
}

static void restart_game(Game *game)
{
    game->lives = PLAYER_LIVES;
    game->score = 0;
    load_level(game, 0);
    audio_play(&game->audio, SFX_MENU_START);
}

bool game_init(Game *game)
{
    SDL_zerop(game);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_CreateWindowAndRenderer("Chuck", 800, 552, 0,
                                     &game->window, &game->renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    /* Use a fixed logical presentation so the game's coordinate system stays
     * consistent when the window is resized or when toggling fullscreen.
     * This makes the game look identical but scaled when entering fullscreen. */
    SDL_SetRenderLogicalPresentation(game->renderer, 800, 552, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_SetRenderVSync(game->renderer, 1);
    srand((unsigned int)time(NULL));
    SDL_srand(SDL_GetTicksNS());

    game->lives = PLAYER_LIVES;
    game->score = 0;
    if (!load_level(game, 0))
    {
        SDL_DestroyRenderer(game->renderer);
        game->renderer = NULL;
        SDL_DestroyWindow(game->window);
        game->window = NULL;
        SDL_Quit();
        return false;
    }

    /* Sound is optional. If no playback device exists, gameplay stays intact. */
    audio_init(&game->audio);
    audio_play_music(&game->audio, MUSIC_INTRO);

    /* Initialise particle system */
    particle_system_init(&game->particles);

    /* Establish the hostage situation once, then reveal the title screen. */
    int win_w = 0, win_h = 0;
    game_get_view_size(game, &win_w, &win_h);
    intro_init(&game->intro, win_w, win_h);
    opening_cutscene_init(&game->opening_cutscene);
    game->state = STATE_OPENING_CUTSCENE;

    game->last_tick = SDL_GetTicksNS();
    return true;
}

void game_return_to_intro(Game *game)
{
    particle_system_clear(&game->particles);
    audio_stop_effects(&game->audio);
    audio_play_music(&game->audio, MUSIC_INTRO);
    audio_play(&game->audio, SFX_MENU_BACK);

    int win_w = 0, win_h = 0;
    game_get_view_size(game, &win_w, &win_h);
    intro_init(&game->intro, win_w, win_h);
    game->state = STATE_INTRO;
}

static void hit_player(Game *game)
{
    /* Start death animation using the particle system. Use a player-local
     * dying flag so other game state remains unchanged. */
    if (game->player.dying)
        return;

    game->terminal_hack_progress = 0.0f;
    game->terminal_hack_tick_timer = 0.0f;
    game->terminal_in_range = false;
    game->terminal_hacking = false;
    game->terminal_alarm_timer = 0.0f;
    game->terminal_reinforcement_timer = 0.0f;
    game->terminal_reinforcements_pending = 0;

    float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    float cx = game->player.x + PLAYER_W * 0.5f;
    float cy = game->player.y + ph * 0.5f;
    particle_system_emit(&game->particles, cx, cy, 32, game->player.facing);
    audio_play(&game->audio, SFX_PLAYER_HIT);
    /* mark player as dying; actual life loss / respawn happens after timer */
    game->player.dying = true;
    game->player.death_timer = 0.75f;
}

static void advance_level(Game *game)
{
    if ((size_t)(game->current_level + 1) < EMBEDDED_LEVEL_COUNT)
    {
        load_level(game, game->current_level + 1);
    }
    else
    {
        audio_stop_music(&game->audio);
        audio_stop_effects(&game->audio);
        outro_cutscene_init(&game->outro_cutscene);
        audio_play_music(&game->audio, MUSIC_INTRO);
        game->state = STATE_OUTRO;
    }
}

static void finish_player_death(Game *game)
{
    /* Apply the actual hit effects after the death animation */
    game->player.dying = false;
    game->player.death_timer = 0.0f;
    particle_system_clear(&game->particles);

    game->lives -= 1;
    game->invuln_timer = INVULN_TIME;

    if (game->lives <= 0)
    {
        audio_stop_music(&game->audio);
        game->state = STATE_GAME_OVER;
        audio_play(&game->audio, SFX_GAME_OVER);
    }
    else
    {
        player_reset(&game->player, &game->level);
        audio_play(&game->audio, SFX_RESPAWN);
    }
}

static void unlock_exit(Game *game)
{
    if (game->level.exit_unlocked)
        return;

    game->level.exit_unlocked = true;
    game->exit_unlocked_timer = 2.5f;
    game->terminal_in_range = false;
    game->terminal_hacking = false;
    audio_play(&game->audio, SFX_EXIT_UNLOCKED);
}

static bool player_near_active_terminal(const Game *game)
{
    int index = game->level.active_terminal_index;
    if (game->level.exit_unlocked ||
        index < 0 || index >= game->level.terminal_count)
    {
        return false;
    }

    const Terminal *terminal = &game->level.terminals[index];
    float terminal_x = terminal->col * (float)TILE_SIZE + TILE_SIZE * 0.5f;
    float terminal_y = terminal->row * (float)TILE_SIZE + TILE_SIZE * 0.5f;
    float player_h =
        game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    float player_x = game->player.x + PLAYER_W * 0.5f;
    float player_y = game->player.y + player_h * 0.5f;

    return fabsf(player_x - terminal_x) <= TERMINAL_INTERACT_RANGE &&
           fabsf(player_y - terminal_y) <= TILE_SIZE * 0.65f;
}

static void clear_edge_input(Game *game)
{
    game->input.jump = false;
    game->input.shoot = false;
    game->input.use_door = false;
    game->input.confirm = false;
    game->input.restart = false;
}

void game_update(Game *game, float dt)
{
    game_read_input(game);
    audio_update_music(&game->audio);

    if (game->state == STATE_OPENING_CUTSCENE)
    {
        Uint32 cues = 0;
        bool finished =
            opening_cutscene_update(&game->opening_cutscene, dt, &cues);

        if (cues & OPENING_CUE_RAIN)
            audio_play(&game->audio, SFX_OPENING_RAIN);
        if (cues & OPENING_CUE_SUV_ENGINE)
            audio_play(&game->audio, SFX_OPENING_SUV_ENGINE);
        if (cues & OPENING_CUE_CAR_ENGINE)
            audio_play(&game->audio, SFX_OPENING_CAR_ENGINE);
        if (cues & (OPENING_CUE_SUV_BRAKE | OPENING_CUE_CAR_BRAKE))
            audio_play(&game->audio, SFX_OPENING_BRAKE);
        if (cues & OPENING_CUE_CAR_DOOR)
            audio_play(&game->audio, SFX_OPENING_CAR_DOOR);
        if (cues & (OPENING_CUE_ESCORT_STEP_A | OPENING_CUE_CHUCK_STEP_A))
            audio_play(&game->audio, SFX_STEP_A);
        if (cues & (OPENING_CUE_ESCORT_STEP_B | OPENING_CUE_CHUCK_STEP_B))
            audio_play(&game->audio, SFX_STEP_B);
        if (cues & OPENING_CUE_BUILDING_DOOR)
            audio_play(&game->audio, SFX_DOOR);

        if (finished || game->input.confirm)
        {
            audio_stop_effects(&game->audio);
            int win_w = 0, win_h = 0;
            game_get_view_size(game, &win_w, &win_h);
            intro_init(&game->intro, win_w, win_h);
            game->state = STATE_INTRO;
        }
        clear_edge_input(game);
        return;
    }

    if (game->state == STATE_INTRO)
    {
        float win_x = 0.0f, win_y = 0.0f, mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&win_x, &win_y);
        SDL_RenderCoordinatesFromWindow(game->renderer, win_x, win_y, &mx, &my);

        int win_w = 0, win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        intro_update(&game->intro, dt, win_w, win_h, mx, my);

        if (game->input.confirm)
        {
            game->input.confirm = false;
            restart_game(game);
        }
        clear_edge_input(game);
        return;
    }

    if (game->state == STATE_LEVEL_TRANSITION)
    {
        Uint32 cues = 0;
        bool finished =
            level_transition_update(&game->level_transition, dt, &cues);

        if (cues & LEVEL_TRANSITION_CUE_STEP_A)
            audio_play(&game->audio, SFX_STEP_A);
        if (cues & LEVEL_TRANSITION_CUE_STEP_B)
            audio_play(&game->audio, SFX_STEP_B);
        if (cues & (LEVEL_TRANSITION_CUE_DOOR_OPEN |
                    LEVEL_TRANSITION_CUE_DOOR_CLOSE))
            audio_play(&game->audio, SFX_DOOR);

        if (finished || game->input.confirm)
        {
            audio_stop_effects(&game->audio);
            advance_level(game);
        }
        clear_edge_input(game);
        return;
    }

    if (game->state == STATE_OUTRO)
    {
        if (game->input.restart &&
            game->outro_cutscene.time >= OUTRO_FINAL_REVEAL_TIME)
        {
            restart_game(game);
            clear_edge_input(game);
            return;
        }

        /*
         * Skipping preserves the happy ending instead of dropping the player
         * onto a separate results screen.
         */
        if (game->input.confirm &&
            game->outro_cutscene.time < OUTRO_FINAL_REVEAL_TIME)
        {
            audio_stop_effects(&game->audio);
            game->outro_cutscene.time = OUTRO_FINAL_REVEAL_TIME;
            audio_play(&game->audio, SFX_WIN);
            clear_edge_input(game);
            return;
        }

        Uint32 cues = 0;
        outro_cutscene_update(&game->outro_cutscene, dt, &cues);
        if (cues & OUTRO_CUE_DOOR)
            audio_play(&game->audio, SFX_DOOR);
        if (cues & OUTRO_CUE_STEP_A)
            audio_play(&game->audio, SFX_STEP_A);
        if (cues & OUTRO_CUE_STEP_B)
            audio_play(&game->audio, SFX_STEP_B);
        if (cues & OUTRO_CUE_HELICOPTER)
            audio_play(&game->audio, SFX_OUTRO_HELICOPTER);
        if (cues & OUTRO_CUE_PLAYER_SHOT)
            audio_play(&game->audio, SFX_PLAYER_SHOT);
        if (cues & OUTRO_CUE_ENEMY_DOWN)
            audio_play(&game->audio, SFX_ENEMY_DOWN);
        if (cues & OUTRO_CUE_EXPLOSION)
            audio_play(&game->audio, SFX_EXPLOSION);
        if (cues & OUTRO_CUE_WIN)
            audio_play(&game->audio, SFX_WIN);

        clear_edge_input(game);
        return;
    }

    /* If restart requested via input handler while in end state, restart. */
    if (game->input.restart &&
        (game->state == STATE_GAME_OVER || game->state == STATE_WIN))
    {
        restart_game(game);
        clear_edge_input(game);
    }

    /* Level start reveal animation: show tiles progressively, then spawn entities. */
    if (game->state == STATE_LEVEL_START)
    {
        /* Do not defer actions pressed during a non-interactive transition. */
        clear_edge_input(game);

        /* Advance reveal; if not finished yet, skip the rest of update. */
        int reveal_row = game->level.reveal_next_row;
        int reveal_col = game->level.reveal_next_col;
        level_reveal_step(&game->level, (float)dt);
        if (reveal_row != game->level.reveal_next_row ||
            reveal_col != game->level.reveal_next_col)
        {
            audio_play(&game->audio, SFX_REVEAL_TICK);
        }
        if (!game->level.reveal_done)
        {
            return;
        }
        /* Reveal finished: either start key-card intro animation or spawn entities. */
        if (game->level.card_count > 0 && game->level.active_card_index >= 0)
        {
            /* Determine active card position among card items. */
            int active_pos = 0;
            for (int i = 0; i < game->level.item_count; ++i)
            {
                if (game->level.items[i].type != ITEM_CARD)
                    continue;
                if (i == game->level.active_card_index)
                    break;
                active_pos++;
            }
            game->card_anim_count = game->level.card_count;
            game->card_anim_current = 0;
            game->card_anim_step = 0;
            game->card_anim_timer = 0.0f;
            /* Run a few cycles before landing on the chosen card. Compute
             * interval so the total animation duration is roughly constant
             * regardless of number of cards. */
            int cycles = 3;
            game->card_anim_total_steps = cycles * game->card_anim_count + active_pos;
            if (game->card_anim_total_steps <= 0)
            {
                game->card_anim_interval = 0.075f;
            }
            else
            {
                const float target_total = 0.75f; /* total seconds for keycard intro */
                float interval = target_total / (float)game->card_anim_total_steps;
                const float min_interval = 0.02f; /* don't go too fast per step */
                const float max_interval = 0.15f; /* don't go too slow per step */
                if (interval < min_interval)
                    interval = min_interval;
                if (interval > max_interval)
                    interval = max_interval;
                game->card_anim_interval = interval;
            }
            game->state = STATE_SHOW_KEYCARD;
        }
        else
        {
            spawn_level_entities_after_reveal(game);
            game->state = STATE_PLAYING;
        }
    }

    /* Key-card intro animation: cycle highlight until target reached, then begin play */
    if (game->state == STATE_SHOW_KEYCARD)
    {
        game->card_anim_timer += dt;
        if (game->card_anim_timer >= game->card_anim_interval)
        {
            game->card_anim_timer -= game->card_anim_interval;
            game->card_anim_step++;
            if (game->card_anim_count > 0)
                game->card_anim_current = (game->card_anim_current + 1) % game->card_anim_count;
            if (game->card_anim_step >= game->card_anim_total_steps)
            {
                /* Animation finished: spawn entities and start playing. */
                audio_play(&game->audio, SFX_CARD_TARGET);
                spawn_level_entities_after_reveal(game);
                game->state = STATE_PLAYING;
            }
            else
            {
                audio_play(&game->audio, SFX_CARD_SCAN);
            }
        }
        clear_edge_input(game);
        return;
    }

    if (game->state == STATE_LEVEL_CLEARED)
    {
        game->message_timer -= dt;
        if (game->message_timer <= 0.0f)
        {
            advance_level(game);
        }
        clear_edge_input(game);
        return;
    }

    /* If player is in dying animation, update particles and wait */
    if (game->player.dying)
    {
        particle_system_update(&game->particles, dt);
        game->player.death_timer -= dt;
        if (game->player.death_timer <= 0.0f)
            finish_player_death(game);
        clear_edge_input(game);
        return;
    }

    if (game->state != STATE_PLAYING)
    {
        clear_edge_input(game);
        return;
    }

    game->level_elapsed_time += dt;

    if (game->invuln_timer > 0.0f)
    {
        game->invuln_timer -= dt;
    }
    if (game->exit_unlocked_timer > 0.0f)
    {
        game->exit_unlocked_timer -= dt;
        if (game->exit_unlocked_timer < 0.0f)
            game->exit_unlocked_timer = 0.0f;
    }

    game->terminal_in_range = player_near_active_terminal(game);
    game->terminal_hacking =
        game->terminal_in_range &&
        game->input.interact &&
        game->player.on_ground &&
        !game->player.on_ladder;
    if (game->terminal_hacking)
    {
        /* The interaction is shown head-on: centre Chuck in front of the
         * wall terminal so the camera sees his back and both hands reach the
         * keypad.  Terminal tiles are non-solid, so this remains a valid
         * player position after the interaction ends. */
        int terminal_index = game->level.active_terminal_index;
        const Terminal *terminal = &game->level.terminals[terminal_index];
        game->player.x =
            terminal->col * (float)TILE_SIZE +
            ((float)TILE_SIZE - PLAYER_W) * 0.5f;
    }
    if (!game->terminal_hacking)
    {
        game->terminal_hack_progress = 0.0f;
        game->terminal_hack_tick_timer = 0.0f;
    }
    bool terminal_alarm_started =
        game->terminal_hacking &&
        game->terminal_alarm_timer <= 0.0f;
    if (game->terminal_hacking)
    {
        game->terminal_alarm_timer = TERMINAL_ALARM_TIME;
        if (terminal_alarm_started && game->level.door_count > 0)
        {
            int count_range =
                TERMINAL_REINFORCEMENT_MAX_COUNT -
                TERMINAL_REINFORCEMENT_MIN_COUNT + 1;
            game->terminal_reinforcements_pending =
                TERMINAL_REINFORCEMENT_MIN_COUNT +
                SDL_rand(count_range);
            game->terminal_reinforcement_timer =
                random_reinforcement_delay(
                    TERMINAL_REINFORCEMENT_FIRST_MIN,
                    TERMINAL_REINFORCEMENT_FIRST_MAX);
        }
    }
    else if (game->terminal_alarm_timer > 0.0f)
    {
        game->terminal_alarm_timer -= dt;
        if (game->terminal_alarm_timer <= 0.0f)
        {
            game->terminal_alarm_timer = 0.0f;
            game->terminal_reinforcement_timer = 0.0f;
            game->terminal_reinforcements_pending = 0;
        }
    }

    bool player_was_grounded = game->player.on_ground;
    bool jump_requested =
        !game->terminal_hacking &&
        game->input.jump &&
        game->player.on_ground;
    float player_fall_speed = game->player.vy;
    int previous_elevator = game->player_on_elevator;

    /* --- Elevator: pre-carry (upward), player physics, update platforms, snap --- */
    {
        /* If riding an upward-moving elevator, nudge the player up before physics
         * so gravity doesn't immediately drop them off. */
        if (previous_elevator >= 0 &&
            previous_elevator < game->level.elevator_count)
        {
            const Elevator *el = &game->level.elevators[previous_elevator];
            if (el->vy < 0.0f)
            {
                game->player.y += el->vy * dt;
                game->player.vy = 0.0f;
            }
        }
    }

    /* Crush detection: if anything pushed the player into a solid tile overhead, they die. */
    {
        int col_left = (int)floorf(game->player.x / (float)TILE_SIZE);
        int col_right = (int)floorf((game->player.x + PLAYER_W - 1.0f) / (float)TILE_SIZE);
        int row_top = (int)floorf(game->player.y / (float)TILE_SIZE);
        for (int c = col_left; c <= col_right; ++c)
        {
            if (level_is_solid(&game->level, c, row_top))
            {
                hit_player(game);
                break;
            }
        }
    }

    float prev_player_x = game->player.x;
    float prev_player_y = game->player.y;
    float prev_player_h = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;

    Input player_input = game->input;
    if (game->terminal_hacking)
    {
        player_input.left = false;
        player_input.right = false;
        player_input.up = false;
        player_input.down = false;
        player_input.jump = false;
        player_input.shoot = false;
        player_input.use_door = false;
        game->input.shoot = false;
        game->input.use_door = false;
        game->player.vx = 0.0f;
    }
    player_update(&game->player, &game->level, &player_input, dt);
    game->input.jump = false;

    if (game->terminal_hacking)
    {
        game->terminal_hack_progress += dt;
        game->terminal_hack_tick_timer -= dt;
        if (game->terminal_hack_tick_timer <= 0.0f)
        {
            audio_play(&game->audio, SFX_CARD_SCAN);
            game->terminal_hack_tick_timer = 0.45f;
        }
        if (game->terminal_hack_progress >= TERMINAL_HACK_TIME)
        {
            game->terminal_hack_progress = TERMINAL_HACK_TIME;
            game->level.terminal_hacked = true;
            game->score += 250;
            unlock_exit(game);
        }
    }

    level_update_elevators(&game->level, dt);
    level_update_falling_platforms(&game->level, dt);
    level_update_moving_platforms(&game->level, dt);
    update_crates(game, dt);
    resolve_player_crates(game, prev_player_x, prev_player_y, prev_player_h);
    game->player_on_elevator = -1;
    for (int i = 0; i < game->level.elevator_count; ++i)
    {
        const Elevator *el = &game->level.elevators[i];
        float plat_x = el->col * (float)TILE_SIZE;
        float player_cx = game->player.x + PLAYER_W * 0.5f;
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_feet = game->player.y + ph;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            game->player.vy >= 0.0f &&
            player_feet >= el->y - 2.0f &&
            player_feet <= el->y + ELEVATOR_PLAT_H + 8.0f)
        {
            game->player.y = el->y - ph;
            game->player.vy = 0.0f;
            game->player.on_ground = true;
            game->player_on_elevator = i;
        }
    }

    /* Moving platforms: detect if player stands on one and snap/carry them. */
    game->player_on_moving_platform = -1;
    for (int i = 0; i < game->level.moving_platform_count; ++i)
    {
        const MovingPlatform *mp = &game->level.moving_platforms[i];
        float plat_x = mp->x;
        float player_cx = game->player.x + PLAYER_W * 0.5f;
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_feet = game->player.y + ph;
        float plat_top = mp->row * (float)TILE_SIZE;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            game->player.vy >= 0.0f &&
            player_feet >= plat_top - 2.0f &&
            player_feet <= plat_top + MOVING_PLATFORM_H + 8.0f)
        {
            game->player.y = plat_top - ph;
            game->player.vy = 0.0f;
            game->player.on_ground = true;
            game->player_on_moving_platform = i;
            break;
        }
    }

    /* If riding a moving platform, carry the player horizontally. */
    if (game->player_on_moving_platform >= 0 && game->player_on_moving_platform < game->level.moving_platform_count)
    {
        const MovingPlatform *mp = &game->level.moving_platforms[game->player_on_moving_platform];
        game->player.x += mp->vx * dt;
    }

    if (jump_requested && game->player.vy < -1.0f)
        audio_play(&game->audio, SFX_JUMP);
    if (!player_was_grounded && game->player.on_ground &&
        player_fall_speed > 150.0f)
    {
        audio_play(&game->audio, SFX_LAND);
    }
    if (game->player_on_elevator >= 0 && previous_elevator < 0)
        audio_play(&game->audio, SFX_ELEVATOR);

    if (game->player.on_ladder && fabsf(game->player.vy) > 1.0f)
    {
        game->ladder_audio_timer -= dt;
        if (game->ladder_audio_timer <= 0.0f)
        {
            audio_play(&game->audio, SFX_LADDER);
            game->ladder_audio_timer = 0.27f;
        }
    }
    else
    {
        game->ladder_audio_timer = 0.0f;
    }

    if (game->player.on_ground && !game->player.on_ladder &&
        fabsf(game->player.vx) > 8.0f)
    {
        game->footstep_audio_timer -= dt;
        if (game->footstep_audio_timer <= 0.0f)
        {
            audio_play(&game->audio,
                       game->footstep_alternate ? SFX_STEP_A : SFX_STEP_B);
            game->footstep_alternate = !game->footstep_alternate;
            game->footstep_audio_timer = game->player.crawling ? 0.40f : 0.27f;
        }
    }
    else
    {
        game->footstep_audio_timer = 0.0f;
    }

    if (game->teleport_cooldown > 0.0f)
        game->teleport_cooldown -= dt;

    /* Mines: stepping triggers a short countdown, then explosion */
    for (int i = 0; i < game->mine_count; ++i)
    {
        /* Mine stored in Game as an inline struct */
        Mine *m = &game->mines[i];
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        if (!m->active)
            continue;
        if (!m->triggered)
        {
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              m->x, m->y, (float)MINE_W, (float)MINE_H))
            {
                m->triggered = true;
                m->timer = MINE_TRIGGER_DELAY;
                play_world_sound(game, SFX_MINE_ARM,
                                 m->x + MINE_W * 0.5f,
                                 m->y + MINE_H * 0.5f);
            }
        }
        else
        {
            m->timer -= dt;
            if (m->timer <= 0.0f)
            {
                m->active = false;
                float cx = m->x + MINE_W * 0.5f;
                float cy = m->y + MINE_H * 0.5f;
                particle_system_explosion(&game->particles, cx, cy, 48);
                play_world_sound(game, SFX_EXPLOSION, cx, cy);
                for (int j = 0; j < game->level.crate_count; ++j)
                {
                    Crate *crate = &game->level.crates[j];
                    if (!crate->active)
                        continue;
                    float bx = crate->x + CRATE_W * 0.5f;
                    float by = crate->y + CRATE_H * 0.5f;
                    float dx = bx - cx;
                    float dy = by - cy;
                    float dist2 = dx * dx + dy * dy;
                    if (dist2 <= MINE_RADIUS * MINE_RADIUS)
                        destroy_crate(game, crate);
                }
                /* Damage player if within radius and not invulnerable */
                float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
                float px = game->player.x + PLAYER_W * 0.5f;
                float py = game->player.y + ph * 0.5f;
                float dx = px - cx;
                float dy = py - cy;
                float dist2 = dx * dx + dy * dy;
                if (dist2 <= MINE_RADIUS * MINE_RADIUS && game->invuln_timer <= 0.0f)
                {
                    hit_player(game);
                }
            }
        }

    }

    /* Grenades: update thrown grenades (physics + fuse) */
    for (int i = 0; i < game->grenade_count; ++i)
    {
        Grenade *g = &game->grenades[i];
        if (!g->active)
            continue;

        /* Apply gravity to grenade velocity, then integrate with collisions
         * so grenades arc and come to rest on floors. */
        g->vy += GRAVITY * dt;
        if (g->vy > MAX_FALL_SPEED)
            g->vy = MAX_FALL_SPEED;

        bool on_ground = false;
        bool was_grounded = g->grounded;
        level_move(&game->level, &g->x, &g->y, &g->vx, &g->vy,
                   GRENADE_W, GRENADE_H, dt, false, &on_ground, false);
        if (on_ground)
        {
            g->grounded = true;
            g->vx = 0.0f;
            g->vy = 0.0f;
            if (!was_grounded)
            {
                play_world_sound(game, SFX_GRENADE_BOUNCE,
                                 g->x + GRENADE_W * 0.5f,
                                 g->y + GRENADE_H * 0.5f);
            }
        }

        g->timer -= dt;
        if (g->timer <= 0.0f)
        {
            /* Explode */
            g->active = false;
            float cx = g->x + GRENADE_W * 0.5f;
            float cy = g->y + GRENADE_H * 0.5f;
            particle_system_explosion(&game->particles, cx, cy, 64);
            play_world_sound(game, SFX_EXPLOSION, cx, cy);
            /* Kill enemies in radius */
            for (int j = 0; j < game->enemy_count; ++j)
            {
                Enemy *e = &game->enemies[j];
                if (e->dead)
                    continue;
                float ex = e->x + ENEMY_W * 0.5f;
                float ey = e->y + ENEMY_H * 0.5f;
                float dx = ex - cx;
                float dy = ey - cy;
                float dist2 = dx * dx + dy * dy;
                if (dist2 <= GRENADE_RADIUS * GRENADE_RADIUS)
                {
                    /* instant kill */
                    e->hp = 0;
                    float px = e->x + ENEMY_W * 0.5f;
                    float py = e->y + ENEMY_H * 0.5f;
                    particle_system_emit(&game->particles, px, py, 24, e->dir);
                    e->dead = true;
                    game->score += 150;
                }
            }
            for (int j = 0; j < game->dog_count; ++j)
            {
                Dog *dog = &game->dogs[j];
                if (dog->dead)
                    continue;
                float dx = (dog->x + DOG_W * 0.5f) - cx;
                float dy = (dog->y + DOG_H * 0.5f) - cy;
                float dist2 = dx * dx + dy * dy;
                if (dist2 <= GRENADE_RADIUS * GRENADE_RADIUS)
                {
                    particle_system_emit(&game->particles,
                                         dog->x + DOG_W * 0.5f,
                                         dog->y + DOG_H * 0.5f,
                                         14,
                                         dog->dir);
                    dog->hp = 0;
                    dog->dead = true;
                    game->score += 75;
                    play_world_sound(game, SFX_DOG_YELP,
                                     dog->x + DOG_W * 0.5f,
                                     dog->y + DOG_H * 0.5f);
                }
            }
            for (int j = 0; j < game->level.crate_count; ++j)
            {
                Crate *crate = &game->level.crates[j];
                if (!crate->active)
                    continue;
                float bx = crate->x + CRATE_W * 0.5f;
                float by = crate->y + CRATE_H * 0.5f;
                float dx = bx - cx;
                float dy = by - cy;
                float dist2 = dx * dx + dy * dy;
                if (dist2 <= GRENADE_RADIUS * GRENADE_RADIUS)
                    destroy_crate(game, crate);
            }
            /* Damage player if within radius */
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            float px = game->player.x + PLAYER_W * 0.5f;
            float py = game->player.y + ph * 0.5f;
            float dx = px - cx;
            float dy = py - cy;
            float dist2 = dx * dx + dy * dy;
            if (dist2 <= GRENADE_RADIUS * GRENADE_RADIUS && game->invuln_timer <= 0.0f)
            {
                hit_player(game);
            }
        }
    }

    if (game->input.use_door && game->player.on_ground && game->teleport_cooldown <= 0.0f)
    {
        int center_col = (int)floorf((game->player.x + PLAYER_W * 0.5f) / TILE_SIZE);
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        int center_row = (int)floorf((game->player.y + ph * 0.5f) / TILE_SIZE);
        const Level *lvl = &game->level;
        for (int d = 0; d < lvl->door_count; ++d)
        {
            if (lvl->doors[d].col == center_col && lvl->doors[d].row == center_row)
            {
                int pair = d ^ 1; /* 0<->1, 2<->3, ... */
                if (pair < lvl->door_count)
                {
                    const Door *dest = &lvl->doors[pair];
                    game->player.x = dest->col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
                    game->player.y = (dest->row + 1) * TILE_SIZE - ph;
                    game->player.vx = 0.0f;
                    game->player.vy = 0.0f;
                    game->teleport_cooldown = TELEPORT_COOLDOWN;
                    audio_play(&game->audio, SFX_DOOR);
                }
                break;
            }
        }
    }
    game->input.use_door = false;

    /* A terminal alarm adds its own small, irregular reinforcement wave.
     * It is independent of the level's normal scripted door spawns. */
    update_terminal_reinforcements(game, dt);

    /* --- Door enemy spawning --- */
    for (int d = 0; d < game->level.door_count; ++d)
    {
        if (game->door_spawns[d] <= 0)
            continue;
        game->door_timers[d] -= dt;
        if (game->door_timers[d] > 0.0f)
            continue;

        if (spawn_enemy_from_door(game, d))
        {
            game->door_spawns[d]--;
            game->door_timers[d] = DOOR_SPAWN_INTERVAL * (0.8f + SDL_rand(40) * 0.01f);
        }
        else
        {
            game->door_timers[d] = 0.5f; /* occupied door/full array: retry */
        }
    }

    /* Shoot or throw grenade */
    if (game->input.shoot)
    {
        /* If player has a grenade, throw it */
        if (game->player.grenades > 0)
        {
            int slot = find_grenade_slot(game);
            if (slot >= 0)
            {
                Grenade *g = &game->grenades[slot];
                g->active = true;
                g->timer = GRENADE_FUSE_TIME;
                g->grounded = false;
                /* Spawn near player's hand */
                {
                    float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
                    g->y = game->player.y + ph * 0.45f;
                }
                /* Spawn slightly ahead of player and choose velocities so the
                 * grenade follows a parabolic arc landing some distance ahead. */
                {
                    const float desired_range = 160.0f;              /* px: target horizontal distance */
                    const float base_v = GRENADE_THROW_SPEED * 0.9f; /* calmer forward throw */
                    float vx = (game->player.facing > 0) ? base_v : -base_v;
                    float hand_offset = (game->player.facing > 0) ? (PLAYER_W + 6.0f) : -(GRENADE_W + 6.0f);
                    g->x = game->player.x + hand_offset;
                    g->vx = vx;
                    /* Compute required initial upward speed magnitude to reach approx range
                     * R = |vx| * (2*vy / g)  =>  vy = R * g / (2 * |vx|) */
                    float vy_mag = (desired_range * GRAVITY) / (2.0f * fabsf(vx));
                    /* clamp vy_mag to reasonable bounds */
                    if (vy_mag < 30.0f)
                        vy_mag = 30.0f;
                    if (vy_mag > 220.0f)
                        vy_mag = 220.0f;
                    g->vy = -vy_mag;
                }
                game->player.grenades = 0;
                game->player.shot_vertical = 0;
                game->player.action_timer = 0.18f;
                audio_play(&game->audio, SFX_GRENADE_THROW);
            }
        }
        else if (game->player.bullets > 0)
        {
            for (int i = 0; i < MAX_BULLETS; ++i)
            {
                if (!game->bullets[i].active)
                {
                    Bullet *b = &game->bullets[i];
                    b->active = true;
                    int vertical_dir = 0;
                    if (game->player.on_ladder)
                    {
                        if (game->input.up && !game->input.down)
                            vertical_dir = -1;
                        else if (game->input.down && !game->input.up)
                            vertical_dir = 1;
                    }

                    if (vertical_dir != 0)
                    {
                        float vertical_bullet_w = (float)BULLET_H;
                        float vertical_bullet_h = (float)BULLET_W;
                        b->x = game->player.x +
                               (PLAYER_W - vertical_bullet_w) * 0.5f;
                        b->y = vertical_dir < 0
                                   ? game->player.y - vertical_bullet_h
                                   : game->player.y + PLAYER_H;
                        b->vx = 0.0f;
                        b->vy = vertical_dir * BULLET_SPEED;
                    }
                    else
                    {
                        float ph = game->player.crawling
                                       ? (float)PLAYER_CRAWL_H
                                       : (float)PLAYER_H;
                        b->y = game->player.y + ph * 0.35f;
                        b->vy = 0.0f;
                        if (game->player.facing > 0)
                        {
                            b->x = game->player.x + PLAYER_W;
                            b->vx = BULLET_SPEED;
                        }
                        else
                        {
                            b->x = game->player.x - BULLET_W;
                            b->vx = -BULLET_SPEED;
                        }
                    }
                    game->player.bullets--;
                    game->player.shot_vertical = vertical_dir;
                    game->player.action_timer = 0.12f;
                    audio_play(&game->audio, SFX_PLAYER_SHOT);
                    break;
                }
            }
        }
        else
        {
            audio_play(&game->audio, SFX_EMPTY_CLICK);
        }
    }
    game->input.shoot = false;

    for (int i = 0; i < game->enemy_count; ++i)
    {
        if (!game->enemies[i].dead)
        {
            Enemy *enemy = &game->enemies[i];
            float prev_enemy_x = enemy->x;
            float prev_enemy_y = enemy->y;
            bool terminal_pursuit = game->terminal_alarm_timer > 0.0f;
            bool pursuing = terminal_pursuit || enemy->provoked;
            if (pursuing && enemy_has_los(game, enemy))
            {
                float player_h =
                    game->player.crawling
                        ? (float)PLAYER_CRAWL_H
                        : (float)PLAYER_H;
                enemy->pursuit_target_x =
                    game->player.x + PLAYER_W * 0.5f;
                enemy->pursuit_target_y =
                    game->player.y + player_h * 0.5f;
                enemy->has_pursuit_target = true;
            }
            else if (terminal_pursuit && !enemy->has_pursuit_target)
            {
                enemy->has_pursuit_target =
                    terminal_alarm_target(
                        game,
                        &enemy->pursuit_target_x,
                        &enemy->pursuit_target_y);
            }
            else if (!pursuing)
            {
                enemy->has_pursuit_target = false;
            }
            pursuing = pursuing && enemy->has_pursuit_target;
            bool hemmed_in =
                enemy_side_blocked(game, i, -1) &&
                enemy_side_blocked(game, i, 1);
            enemy_update(enemy, &game->level, dt,
                         pursuing,
                         enemy->pursuit_target_x,
                         enemy->pursuit_target_y,
                         hemmed_in);
            resolve_enemy_crates(game, enemy, prev_enemy_x, prev_enemy_y);
        }
    }

    for (int i = 0; i < game->dog_count; ++i)
    {
        if (!game->dogs[i].dead)
        {
            float prev_dog_x = game->dogs[i].x;
            float prev_dog_y = game->dogs[i].y;
            DogState previous_state = game->dogs[i].state;
            dog_update_ai(game, &game->dogs[i], dt);
            resolve_dog_crates(game, &game->dogs[i], prev_dog_x, prev_dog_y);
            if (previous_state != DOG_CHASE &&
                game->dogs[i].state == DOG_CHASE)
            {
                SoundEffect bark = SDL_rand(2) == 0
                                       ? SFX_DOG_BARK
                                       : SFX_DOG_BARK_ALT;
                play_world_sound(game, bark,
                                 game->dogs[i].x + DOG_W * 0.5f,
                                 game->dogs[i].y + DOG_H * 0.5f);
                game->dogs[i].vocal_timer =
                    1.0f + SDL_rand(100) * 0.01f;
            }
            else if (game->dogs[i].state == DOG_CHASE &&
                     game->dogs[i].vocal_timer <= 0.0f)
            {
                float dog_cx = game->dogs[i].x + DOG_W * 0.5f;
                float dog_cy = game->dogs[i].y + DOG_H * 0.5f;
                float dx = (game->player.x + PLAYER_W * 0.5f) - dog_cx;
                SoundEffect voice =
                    fabsf(dx) < 3.0f * TILE_SIZE && SDL_rand(3) == 0
                        ? SFX_DOG_GROWL
                        : (SDL_rand(2) == 0
                               ? SFX_DOG_BARK
                               : SFX_DOG_BARK_ALT);
                play_world_sound(game, voice, dog_cx, dog_cy);
                /* Dogs stay audible in a chase without barking every second. */
                game->dogs[i].vocal_timer =
                    1.7f + SDL_rand(180) * 0.01f;
            }
        }
    }

    for (int i = 0; i < game->level.fall_platform_count; ++i)
    {
        FallPlatform *platform = &game->level.fall_platforms[i];
        if (platform->triggered && !game->fall_platform_sounded[i])
        {
            game->fall_platform_sounded[i] = true;
            play_world_sound(
                game, SFX_PLATFORM_CRACK,
                platform->col * (float)TILE_SIZE + TILE_SIZE * 0.5f,
                platform->y + FALL_PLATFORM_H * 0.5f);
        }
    }

    /* Enemy conversations: pair-only logic. Two nearby, grounded enemies may
     * stop and chat with some chance. They are nudged apart slightly to stand
     * facing each other; a partner link prevents third parties joining. */
    for (int i = 0;
         game->terminal_alarm_timer <= 0.0f &&
         i < game->enemy_count;
         ++i)
    {
        Enemy *a = &game->enemies[i];
        if (a->dead || a->provoked || a->talking || a->climbing || !a->on_ground || a->talk_partner != -1 || a->talk_cooldown > 0.0f)
            continue;
        for (int j = i + 1; j < game->enemy_count; ++j)
        {
            Enemy *b = &game->enemies[j];
            if (b->dead || b->provoked || b->talking || b->climbing || !b->on_ground || b->talk_partner != -1 || b->talk_cooldown > 0.0f)
                continue;

            /* Must be approximately same vertical level */
            if (fabsf(a->y - b->y) > (float)TILE_SIZE * 0.5f)
                continue;

            /* Horizontal proximity check */
            float axc = a->x + ENEMY_W * 0.5f;
            float bxc = b->x + ENEMY_W * 0.5f;
            float absdx = fabsf(axc - bxc);
            if (absdx > (float)ENEMY_W + 16.0f)
                continue;

            /* Chance to start a conversation */
            if (SDL_rand(100) >= ENEMY_TALK_CHANCE)
                continue;

            /* Try to nudge them apart horizontally if overlapping so they stand
             * in front of each other rather than stacked. Compute desired shifts
             * and verify target tiles are free. */
            Enemy *left = (a->x < b->x) ? a : b;
            Enemy *right = (a->x < b->x) ? b : a;
            float overlap = (left->x + ENEMY_W) - right->x;
            float gap = 6.0f;
            float shift = 0.0f;
            if (overlap > 0.0f)
                shift = (overlap + gap) * 0.5f;
            float new_left_x = left->x - shift;
            float new_right_x = right->x + shift;

            /* Check that new positions don't collide with solid tiles */
            int top_row = (int)floorf(left->y / TILE_SIZE);
            int bot_row = (int)floorf((left->y + ENEMY_H - 1.0f) / TILE_SIZE);
            bool can_place = true;
            for (int r = top_row; r <= bot_row && can_place; ++r)
            {
                int lc = (int)floorf(new_left_x / TILE_SIZE);
                int rc = (int)floorf((new_right_x + ENEMY_W - 1.0f) / TILE_SIZE);
                int lc_end = (int)floorf((new_left_x + ENEMY_W - 1.0f) / TILE_SIZE);
                int rc_start = (int)floorf(new_right_x / TILE_SIZE);
                /* left enemy area */
                for (int c = lc; c <= lc_end; ++c)
                {
                    if (level_is_solid(&game->level, c, r))
                    {
                        can_place = false;
                        break;
                    }
                }
                /* right enemy area */
                for (int c = rc_start; c <= rc; ++c)
                {
                    if (level_is_solid(&game->level, c, r))
                    {
                        can_place = false;
                        break;
                    }
                }
            }
            if (!can_place)
                continue; /* cannot safely separate, skip conversation */

            /* Start conversation pair */
            left->x = new_left_x;
            right->x = new_right_x;
            left->vx = right->vx = 0.0f;
            left->aim_timer = right->aim_timer = 0.0f;
            left->talking = right->talking = true;
            left->talk_timer = right->talk_timer = ENEMY_TALK_DURATION;
            left->talk_partner = (int)(right - game->enemies);
            right->talk_partner = (int)(left - game->enemies);
            left->dir = 1;   /* face right */
            right->dir = -1; /* face left */
            play_world_sound(game, SFX_GUARD_TALK,
                             (left->x + right->x) * 0.5f + ENEMY_W * 0.5f,
                             (left->y + right->y) * 0.5f + ENEMY_H * 0.5f);
            break;           /* only pair with one partner */
        }
    }

    /* Cleanup partner links: if either partner finished talking, clear both
     * sides and start a cooldown so they won't immediately talk again. */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *e = &game->enemies[i];
        if (e->talk_partner < 0)
            continue;
        int p = e->talk_partner;
        if (p < 0 || p >= game->enemy_count)
        {
            e->talk_partner = -1;
            continue;
        }
        Enemy *pe = &game->enemies[p];
        if (!e->talking || !pe->talking)
        {
            /* Clear partner links on both sides */
            if (pe->talk_partner == i)
            {
                pe->talk_partner = -1;
                pe->talking = false;
                pe->talk_timer = 0.0f;
                pe->talk_cooldown = ENEMY_TALK_COOLDOWN;
            }
            e->talk_partner = -1;
            e->talking = false;
            e->talk_timer = 0.0f;
            e->talk_cooldown = ENEMY_TALK_COOLDOWN;
        }
    }

    /* Collect items. */
    for (int i = 0; i < game->level.item_count; ++i)
    {
        Item *it = &game->level.items[i];
        if (it->collected)
        {
            continue;
        }
        {
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              it->x - 8.0f, it->y - 8.0f, 16.0f, 16.0f))
            {
                it->collected = true;
                /* Start respawn timer only for pickups that should respawn (ammo, grenades). */
                if (it->type == ITEM_GUN || it->type == ITEM_GRENADE)
                {
                    it->respawn_timer = ITEM_RESPAWN_TIME;
                }
                if (it->type == ITEM_CARD)
                {
                    game->score += 100;
                    if (i == game->level.active_card_index)
                    {
                        game->level.items_remaining = 0;
                        unlock_exit(game);
                    }
                    else
                        audio_play(&game->audio, SFX_CARD_WRONG);
                }
                else if (it->type == ITEM_GUN)
                {
                    game->player.bullets = MAX_AMMO;
                    audio_play(&game->audio, SFX_PICKUP_AMMO);
                }
                else if (it->type == ITEM_GRENADE)
                {
                    game->player.grenades = 1; /* pickup gives one grenade */
                    audio_play(&game->audio, SFX_PICKUP_GRENADE);
                }
                else if (it->type == ITEM_MEDKIT)
                {
                    /* Give the player an extra life when picking up a medkit, capped. */
                    if (game->lives < MAX_LIVES)
                        game->lives += 1;
                    audio_play(&game->audio, SFX_PICKUP_HEALTH);
                }
            }
        }
    }

    /* Respawn timers: only decremented for items that should respawn (ammo, grenades). */
    for (int i = 0; i < game->level.item_count; ++i)
    {
        Item *it = &game->level.items[i];
        if (!it->collected)
            continue;
        if (it->type != ITEM_GUN && it->type != ITEM_GRENADE)
            continue; /* only ammo and grenades respawn */
        it->respawn_timer -= dt;
        if (it->respawn_timer <= 0.0f)
        {
            it->collected = false;
            it->respawn_timer = 0.0f;
        }
    }

    /* Ceiling fans and spikes are lethal environmental hazards. */
    if (game->invuln_timer <= 0.0f)
    {
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        for (int i = 0; i < game->level.ceiling_fan_count; ++i)
        {
            const CeilingFan *fan = &game->level.ceiling_fans[i];
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              fan->x - CEILING_FAN_BLADE_LENGTH,
                              fan->y - CEILING_FAN_HIT_HEIGHT * 0.25f,
                              CEILING_FAN_BLADE_LENGTH * 2.0f,
                              CEILING_FAN_HIT_HEIGHT))
            {
                hit_player(game);
                break;
            }
        }

        for (int i = 0; i < game->level.spike_count; ++i)
        {
            const SpikeSpawn *s = &game->level.spike_spawns[i];
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              s->x, s->y, (float)SPIKE_W, (float)SPIKE_H))
            {
                hit_player(game);
                break;
            }
        }
    }

    /* Update bullets. */
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        Bullet *b = &game->bullets[i];
        if (!b->active)
            continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        bool vertical = fabsf(b->vy) > fabsf(b->vx);
        float bullet_w = vertical ? (float)BULLET_H : (float)BULLET_W;
        float bullet_h = vertical ? (float)BULLET_W : (float)BULLET_H;

        /* Off-screen */
        if (b->x + bullet_w < 0.0f ||
            b->x > game->level.width * (float)TILE_SIZE ||
            b->y + bullet_h < 0.0f ||
            b->y > game->level.height * (float)TILE_SIZE)
        {
            b->active = false;
            continue;
        }

        /* Wall collision */
        float leading_x = b->x + bullet_w * 0.5f;
        float leading_y = b->y + bullet_h * 0.5f;
        if (b->vx > 0.0f)
            leading_x = b->x + bullet_w - 1.0f;
        else if (b->vx < 0.0f)
            leading_x = b->x;
        if (b->vy > 0.0f)
            leading_y = b->y + bullet_h - 1.0f;
        else if (b->vy < 0.0f)
            leading_y = b->y;
        int col = (int)floorf(leading_x / TILE_SIZE);
        int row = (int)floorf(leading_y / TILE_SIZE);
        if (level_is_solid(&game->level, col, row))
        {
            b->active = false;
            play_world_sound(game, SFX_BULLET_IMPACT,
                             b->x + bullet_w * 0.5f,
                             b->y + bullet_h * 0.5f);
            continue;
        }

        /* Crate collision */
        for (int j = 0; j < game->level.crate_count; ++j)
        {
            Crate *crate = &game->level.crates[j];
            if (!crate->active)
                continue;
            if (boxes_overlap(b->x, b->y, bullet_w, bullet_h,
                              crate->x, crate->y, CRATE_W, CRATE_H))
            {
                b->active = false;
                destroy_crate(game, crate);
                break;
            }
        }
        if (!b->active)
            continue;

        /* Dog collision */
        for (int j = 0; j < game->dog_count; ++j)
        {
            Dog *dog = &game->dogs[j];
            if (dog->dead)
                continue;
            if (boxes_overlap(b->x, b->y, bullet_w, bullet_h,
                              dog->x, dog->y, DOG_W, DOG_H))
            {
                b->active = false;
                dog->hp--;
                if (dog->hp <= 0)
                {
                    particle_system_emit(&game->particles,
                                         dog->x + DOG_W * 0.5f,
                                         dog->y + DOG_H * 0.5f,
                                         14,
                                         dog->dir);
                    dog->dead = true;
                    game->score += 75;
                    play_world_sound(game, SFX_DOG_YELP,
                                     dog->x + DOG_W * 0.5f,
                                     dog->y + DOG_H * 0.5f);
                }
                else
                    play_world_sound(game, SFX_DOG_YELP,
                                     dog->x + DOG_W * 0.5f,
                                     dog->y + DOG_H * 0.5f);
                break;
            }
        }
        if (!b->active)
            continue;

        /* Enemy collision */
        for (int j = 0; j < game->enemy_count; ++j)
        {
            Enemy *e = &game->enemies[j];
            if (e->dead)
                continue;
            if (boxes_overlap(b->x, b->y, bullet_w, bullet_h,
                              e->x, e->y, ENEMY_W, ENEMY_H))
            {
                b->active = false;
                e->hp--;
                if (e->hp <= 0)
                {
                    /* Emit blood particles at enemy death, then mark dead */
                    float cx = e->x + ENEMY_W * 0.5f;
                    float cy = e->y + ENEMY_H * 0.5f;
                    particle_system_emit(&game->particles, cx, cy, 24, e->dir);
                    e->dead = true;
                    game->score += 150;
                    play_world_sound(game, SFX_ENEMY_DOWN,
                                     e->x + ENEMY_W * 0.5f,
                                     e->y + ENEMY_H * 0.5f);
                }
                else
                    play_world_sound(game, SFX_ENEMY_HIT,
                                     e->x + ENEMY_W * 0.5f,
                                     e->y + ENEMY_H * 0.5f);
                provoke_enemy_after_hit(game, j);
                break;
            }
        }
    }

    /* Enemy reaction: if the player is close and the enemy is moving away
     * while the player is not crawling, there's a small chance the enemy
     * will turn to face the player and start aiming to attack. */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *e = &game->enemies[i];
        if (e->dead || e->climbing)
            continue;
        if (game->player.crawling)
            continue; /* don't retaliate at lowered (crawling) player */
        float px = game->player.x + PLAYER_W * 0.5f;
        float ex = e->x + ENEMY_W * 0.5f;
        float dx = px - ex;

        /* If the enemy is currently talking, only notice the player when
         * the player is very close (ENEMY_TALK_NOTICE_RADIUS). In that case
         * interrupt the conversation so the enemy can retaliate. */
        if (e->talking)
        {
            if (fabsf(dx) > (float)ENEMY_TALK_NOTICE_RADIUS)
                continue;
            /* Interrupt talk and start per-enemy cooldown so they won't
             * immediately start another conversation. */
            e->talking = false;
            e->talk_timer = 0.0f;
            if (e->talk_cooldown <= 0.0f)
                e->talk_cooldown = ENEMY_TALK_COOLDOWN;
        }
        /* Only consider when enemy is moving away from player */
        if (dx * (float)e->dir >= 0.0f)
            continue;
        if (fabsf(dx) > (float)ENEMY_RETALIATE_RADIUS)
            continue;
        /* Check unobstructed horizontal line of sight (ignore current facing)
         * and reasonable vertical alignment. This mirrors `enemy_has_los`
         * but does not require the enemy to already face the player. */
        {
            const Player *p = &game->player;
            float ey = e->y + ENEMY_H * 0.5f;
            float ph = p->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            float py = p->y + ph * 0.5f;
            if (fabsf(py - ey) > (float)TILE_SIZE * 1.2f)
                continue;
            int row = (int)floorf(ey / TILE_SIZE);
            float ex = e->x + ENEMY_W * 0.5f;
            float px2 = p->x + PLAYER_W * 0.5f;
            int col0 = (int)floorf(fminf(ex, px2) / TILE_SIZE);
            int col1 = (int)floorf(fmaxf(ex, px2) / TILE_SIZE);
            bool blocked = false;
            for (int c = col0; c <= col1; ++c)
            {
                if (level_is_solid(&game->level, c, row))
                {
                    blocked = true;
                    break;
                }
            }
            if (!blocked)
                blocked = crate_blocks_row(game, ex, px2, row);
            if (blocked)
                continue;
        }
        if (SDL_rand(100) < ENEMY_RETALIATE_CHANCE)
        {
            e->dir = (dx > 0.0f) ? 1 : -1;
            e->aim_target_x = px;
            if (game->player.crawling)
                e->aim_target_y = game->player.y + (float)PLAYER_CRAWL_H * 0.45f;
            else
                e->aim_target_y = game->player.y + (float)PLAYER_H * 0.15f;
            e->aim_timer = ENEMY_AIM_TIME;
            play_world_sound(game, SFX_ENEMY_ALERT,
                             e->x + ENEMY_W * 0.5f,
                             e->y + ENEMY_H * 0.5f);
        }
    }

    /* Enemy shooting: tick cooldowns, fire when LOS to player is clear. */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *e = &game->enemies[i];
        if (e->dead || e->climbing)
            continue;

        /* Tick aim timer; fire bullet when it expires. */
        if (e->aim_timer > 0.0f)
        {
            e->aim_timer -= dt;
            if (e->aim_timer <= 0.0f)
            {
                e->aim_timer = 0.0f;
                for (int j = 0; j < MAX_ENEMY_BULLETS; ++j)
                {
                    Bullet *b = &game->enemy_bullets[j];
                    if (b->active)
                        continue;
                    /* Use the stored target snapshot captured when aiming started. */
                    /* Fire horizontally toward the captured target X while
                     * keeping vertical speed zero so the bullet travels straight.
                     * Follow the captured aim Y, but keep the muzzle within the
                     * enemy's torso so shots never originate at its head or feet. */
                    float px = e->aim_target_x;
                    float sx = e->x + ENEMY_W * 0.5f;
                    int dir = (fabsf(px - sx) < 0.001f) ? (e->dir >= 0 ? 1 : -1) : ((px > sx) ? 1 : -1);
                    float shot_y = e->aim_target_y;
                    float min_shot_y = e->y + ENEMY_H * ENEMY_MUZZLE_MIN_Y_FACTOR;
                    float max_shot_y = e->y + ENEMY_H * ENEMY_MUZZLE_MAX_Y_FACTOR;
                    if (shot_y < min_shot_y)
                        shot_y = min_shot_y;
                    if (shot_y > max_shot_y)
                        shot_y = max_shot_y;
                    b->vx = (float)dir * ENEMY_BULLET_SPEED;
                    b->vy = 0.0f;
                    b->x = (dir > 0) ? e->x + ENEMY_W : e->x - BULLET_W;
                    b->y = shot_y - BULLET_H * 0.5f;
                    b->active = true;
                    e->recoil_timer = 0.14f;
                    play_world_sound(game, SFX_ENEMY_SHOT,
                                     e->x + ENEMY_W * 0.5f,
                                     e->y + ENEMY_H * 0.5f);
                    break;
                }
                e->shoot_cooldown = ENEMY_SHOOT_COOLDOWN * (0.7f + SDL_rand(60) * 0.01f);
            }
            continue;
        }

        e->shoot_cooldown -= dt;
        if (e->shoot_cooldown > 0.0f || !enemy_has_los(game, e))
            continue;

        /* Start aiming – capture the player's position now and freeze for ENEMY_AIM_TIME before firing. */
        {
            /* Capture player's horizontal centre; aim near the head when
             * standing, but if the player is crawling aim lower (knee level)
             * so crouching changes the vertical aim point. */
            e->aim_target_x = game->player.x + PLAYER_W * 0.5f;
            if (game->player.crawling)
            {
                /* Aim a bit above the feet of the crawling box to approximate knees. */
                e->aim_target_y = game->player.y + (float)PLAYER_CRAWL_H * 0.45f;
            }
            else
            {
                /* Aim near the top of the standing player (head). */
                e->aim_target_y = game->player.y + (float)PLAYER_H * 0.15f;
            }
            e->aim_timer = ENEMY_AIM_TIME;
            play_world_sound(game, SFX_ENEMY_ALERT,
                             e->x + ENEMY_W * 0.5f,
                             e->y + ENEMY_H * 0.5f);
        }
    }

    /* Update enemy bullets: move, wall check, player hit. */
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        Bullet *b = &game->enemy_bullets[i];
        if (!b->active)
            continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        if (b->x + BULLET_W < 0.0f || b->x > game->level.width * (float)TILE_SIZE || b->y + BULLET_H < 0.0f)
        {
            b->active = false;
            continue;
        }

        int col = (int)floorf((b->x + (b->vx > 0 ? BULLET_W - 1 : 0)) / TILE_SIZE);
        int row = (int)floorf((b->y + BULLET_H * 0.5f) / TILE_SIZE);
        if (level_is_solid(&game->level, col, row))
        {
            b->active = false;
            play_world_sound(game, SFX_BULLET_IMPACT,
                             b->x + BULLET_W * 0.5f,
                             b->y + BULLET_H * 0.5f);
            continue;
        }

        for (int j = 0; j < game->level.crate_count; ++j)
        {
            const Crate *crate = &game->level.crates[j];
            if (!crate->active)
                continue;
            if (boxes_overlap(b->x, b->y, BULLET_W, BULLET_H,
                              crate->x, crate->y, CRATE_W, CRATE_H))
            {
                b->active = false;
                play_world_sound(game, SFX_BULLET_IMPACT,
                                 b->x + BULLET_W * 0.5f,
                                 b->y + BULLET_H * 0.5f);
                break;
            }
        }
        if (!b->active)
            continue;

        if (game->invuln_timer <= 0.0f)
        {
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(b->x, b->y, BULLET_W, BULLET_H,
                              game->player.x, game->player.y, PLAYER_W, ph))
            {
                b->active = false;
                hit_player(game);
            }
        }
    }

    /* Enemy contact. */
    if (game->invuln_timer <= 0.0f)
    {
        for (int i = 0; i < game->enemy_count; ++i)
        {
            Enemy *e = &game->enemies[i];
            if (e->dead)
                continue;
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              e->x, e->y, ENEMY_W, ENEMY_H))
            {
                hit_player(game);
                break;
            }
        }
    }

    /* Guard dog bite: fast contact damage with its own cooldown. */
    if (game->invuln_timer <= 0.0f)
    {
        for (int i = 0; i < game->dog_count; ++i)
        {
            Dog *dog = &game->dogs[i];
            if (dog->dead || dog->bite_cooldown > 0.0f)
                continue;
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              dog->x, dog->y, DOG_W, DOG_H))
            {
                dog->bite_cooldown = DOG_BITE_COOLDOWN;
                dog->attack_timer = 0.18f;
                dog->state = DOG_CHASE;
                dog->lost_timer = DOG_LOST_TIME;
                dog->chase_target_x =
                    game->player.x + PLAYER_W * 0.5f;
                dog->has_chase_target = true;
                play_world_sound(game, SFX_DOG_BITE,
                                 dog->x + DOG_W * 0.5f,
                                 dog->y + DOG_H * 0.5f);
                hit_player(game);
                break;
            }
        }
    }

    /* Reach the exit after either access route has granted clearance. */
    if (game->state == STATE_PLAYING && game->level.has_exit &&
        game->level.exit_unlocked)
    {
        float ex = game->level.exit_col * (float)TILE_SIZE;
        float ey = game->level.exit_row * (float)TILE_SIZE;
        {
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              ex, ey, TILE_SIZE, TILE_SIZE))
            {
                if ((size_t)(game->current_level + 1) < EMBEDDED_LEVEL_COUNT)
                {
                    int hostiles_neutralized = 0;
                    for (int i = 0; i < game->enemy_count; ++i)
                    {
                        if (game->enemies[i].dead)
                            hostiles_neutralized++;
                    }
                    for (int i = 0; i < game->dog_count; ++i)
                    {
                        if (game->dogs[i].dead)
                            hostiles_neutralized++;
                    }

                    level_transition_init(
                        &game->level_transition,
                        game->current_level,
                        game->current_level + 1,
                        game->level_elapsed_time,
                        game->score - game->level_start_score,
                        hostiles_neutralized);
                    audio_stop_music(&game->audio);
                    game->state = STATE_LEVEL_TRANSITION;
                }
                else
                {
                    game->state = STATE_LEVEL_CLEARED;
                    game->message_timer = 1.2f;
                }
                audio_play(&game->audio, SFX_LEVEL_CLEAR);
            }
        }
    }

    /* Update particle system every frame so non-player-triggered effects animate. */
    particle_system_update(&game->particles, dt);

    /* Camera: follow player horizontally, clamped to level bounds */
    {
        int win_w = 0, win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        float desired = game->player.x + PLAYER_W * 0.5f - (float)win_w * 0.5f;
        float max_cam = game->level.width * (float)TILE_SIZE - (float)win_w;
        if (max_cam < 0.0f)
            max_cam = 0.0f;
        if (desired < 0.0f)
            desired = 0.0f;
        if (desired > max_cam)
            desired = max_cam;
        /* Smoothly interpolate camera towards desired position (lerp). */
        float alpha = 8.0f * dt; /* responsiveness factor */
        if (alpha > 1.0f)
            alpha = 1.0f;
        game->cam_x += (desired - game->cam_x) * alpha;
    }
}

/* Shutdown and free game resources. */

void game_shutdown(Game *game)
{
    audio_shutdown(&game->audio);
    if (game->renderer)
    {
        SDL_DestroyRenderer(game->renderer);
    }
    if (game->window)
    {
        SDL_DestroyWindow(game->window);
    }
    SDL_Quit();
}
