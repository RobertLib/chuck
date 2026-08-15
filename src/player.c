#include "player.h"

#include <math.h>

bool player_weapon_available(const Player *player, PlayerWeapon weapon)
{
    switch (weapon)
    {
    case PLAYER_WEAPON_PISTOL:
        return player->bullets > 0;
    case PLAYER_WEAPON_KNIFE:
        return true;
    case PLAYER_WEAPON_GRENADE:
        return player->grenades > 0;
    case PLAYER_WEAPON_BAZOOKA:
        return player->bazooka_rockets > 0;
    case PLAYER_WEAPON_COUNT:
        return false;
    }
    return false;
}

/* This order makes one press after a temporary pickup return to the ordinary
 * sidearm, while still keeping the always-available knife in the cycle. */
static const PlayerWeapon WEAPON_CYCLE[] = {
    PLAYER_WEAPON_KNIFE,
    PLAYER_WEAPON_BAZOOKA,
    PLAYER_WEAPON_GRENADE,
    PLAYER_WEAPON_PISTOL};

/* One step through the cycle in either direction, skipping whatever is out of
 * ammo. `step` is +1 for the next weapon and PLAYER_WEAPON_COUNT - 1 for the
 * one before it, so both bumpers walk the same ring rather than two lists that
 * could disagree about what follows what. */
static void select_weapon_step(Player *player, int step)
{
    int current = -1;
    for (int i = 0; i < PLAYER_WEAPON_COUNT; ++i)
    {
        if (WEAPON_CYCLE[i] == player->active_weapon)
        {
            current = i;
            break;
        }
    }
    if (current < 0)
        current = 0; /* never, but a negative index would not stay never */
    for (int offset = 1; offset <= PLAYER_WEAPON_COUNT; ++offset)
    {
        PlayerWeapon candidate =
            WEAPON_CYCLE[(current + offset * step) % PLAYER_WEAPON_COUNT];
        if (player_weapon_available(player, candidate))
        {
            player->active_weapon = candidate;
            return;
        }
    }
}

void player_select_next_weapon(Player *player)
{
    select_weapon_step(player, 1);
}

void player_select_prev_weapon(Player *player)
{
    select_weapon_step(player, PLAYER_WEAPON_COUNT - 1);
}

void player_reset(Player *player, const Level *level)
{
    player->x = level->map.start_x;
    player->y = level->map.start_y;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->on_ground = false;
    player->on_ladder = false;
    player->ladder_direction = 0;
    player->ladder_lockout_timer = 0.0f;
    player->facade_climbing = false;
    player->facing = 1;
    player->hp = PLAYER_MAX_HP;
    player->coyote_timer = 0.0f;
    player->jump_buffer_timer = 0.0f;
    player->jump_cut_ok = false;
    player->jumped = false;
    player->bullets = MAX_AMMO;
    player->grenades = 0;
    player->bazooka_rockets = 0;
    player->active_weapon = PLAYER_WEAPON_PISTOL;
    player->dying = false;
    player->death_timer = 0.0f;
    player->crawling = false;
    player->anim_time = 0.0f;
    player->action_timer = 0.0f;
    player->knife_attacking = false;
    player->grenade_throwing = false;
    player->bazooka_firing = false;
    player->shot_vertical = 0;
}

/* True when the player box overlaps a ladder near its center or feet. */
static bool player_over_ladder(const Player *player, const Level *level)
{
    int col = (int)floorf((player->x + PLAYER_W * 0.5f) / TILE_SIZE);
    float ph = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    int row_center = (int)floorf((player->y + ph * 0.5f) / TILE_SIZE);
    int row_feet = (int)floorf((player->y + ph - 1.0f) / TILE_SIZE);
    return level_is_ladder(level, col, row_center) ||
           level_is_ladder(level, col, row_feet);
}

/* A player standing on the top edge of a ladder does not overlap its tile.
 * Sample immediately below the collision box so Down can still grab it. */
static bool player_has_ladder_below(const Player *player, const Level *level)
{
    int col = (int)floorf((player->x + PLAYER_W * 0.5f) / TILE_SIZE);
    float ph = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    int row = (int)floorf((player->y + ph) / TILE_SIZE);
    return level_is_ladder(level, col, row);
}

float player_update(Player *player, Level *level, const Input *input, float dt)
{
    player->jumped = false;
    /* Remember the press; whether it can be honoured is decided below, and
     * possibly a few frames later than the press itself. */
    if (input->jump)
        player->jump_buffer_timer = PLAYER_JUMP_BUFFER;

    if (player->action_timer > 0.0f)
    {
        player->action_timer -= dt;
        if (player->action_timer <= 0.0f)
        {
            player->action_timer = 0.0f;
            player->knife_attacking = false;
            player->grenade_throwing = false;
            player->bazooka_firing = false;
        }
    }

    /* Horizontal intent */
    float move = 0.0f;
    if (input->left)
    {
        move -= 1.0f;
        player->facing = -1;
    }
    if (input->right)
    {
        move += 1.0f;
        player->facing = 1;
    }
    /* Horizontal speed depends on crawling state */
    /* Determine crawling intent: holding down while on ground and not on ladder */
    bool descend_from_top = input->down && player->on_ground &&
                            player_has_ladder_below(player, level);
    bool want_crawl = input->down && player->on_ground && !player->on_ladder &&
                      !descend_from_top;
    if (want_crawl && !player->crawling)
    {
        /* Enter crawling: lower the collision box while keeping feet in place */
        player->y += (float)(PLAYER_H - PLAYER_CRAWL_H);
        player->crawling = true;
    }
    else if (!want_crawl && player->crawling)
    {
        /* Try to stand up: check there's room above before rising */
        float new_y = player->y - (float)(PLAYER_H - PLAYER_CRAWL_H);
        int left_col = (int)floorf(player->x / TILE_SIZE);
        int right_col = (int)floorf((player->x + PLAYER_W - 1.0f) / TILE_SIZE);
        int top_row = (int)floorf(new_y / TILE_SIZE);
        int bottom_row = (int)floorf((new_y + PLAYER_H - 1.0f) / TILE_SIZE);
        bool blocked = false;
        for (int r = top_row; r <= bottom_row && !blocked; ++r)
        {
            for (int c = left_col; c <= right_col; ++c)
            {
                if (level_is_solid(level, c, r))
                {
                    blocked = true;
                    break;
                }
            }
        }
        if (!blocked)
        {
            player->y = new_y;
            player->crawling = false;
        }
        else
        {
            /* remain crawling if blocked */
            player->crawling = true;
        }
    }

    if (player->crawling)
        player->vx = move * PLAYER_CRAWL_SPEED;
    else
        player->vx = move * PLAYER_WALK_SPEED;

    bool over_ladder = player_over_ladder(player, level);

    if (player->ladder_lockout_timer > 0.0f)
        player->ladder_lockout_timer -= dt;

    /* Mount the ladder by pressing up/down; stay on it until we leave it.
     *
     * Grabbing it also centres the box in the rung column. The box is 26 of a
     * 32px tile, so a player who stops anywhere in the outer 6px of the column
     * still overlaps the neighbouring one; a slab beside the ladder then blocks
     * the climb on the vertical axis and pressing up appears to do nothing at
     * all. Snapping on mount is invisible at 3px and makes every ladder in the
     * campaign catch from wherever the player happened to stop walking. */
    if (((over_ladder && (input->up || input->down)) || descend_from_top) &&
        player->ladder_lockout_timer <= 0.0f)
    {
        if (!player->on_ladder)
        {
            int col = (int)floorf((player->x + PLAYER_W * 0.5f) / TILE_SIZE);
            player->x = (float)col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
        }
        player->on_ladder = true;
    }
    if (!over_ladder && !descend_from_top)
    {
        player->on_ladder = false;
        player->ladder_direction = 0;
    }

    if (player->on_ladder)
    {
        float climb = 0.0f;
        if (input->up)
        {
            climb -= 1.0f;
        }
        if (input->down)
        {
            climb += 1.0f;
        }
        if (climb < 0.0f)
            player->ladder_direction = -1;
        else if (climb > 0.0f)
            player->ladder_direction = 1;
        else if (move != 0.0f)
            /* Facing already remembers which side was pressed. Clear the
             * vertical aim so the next idle attack follows that side. */
            player->ladder_direction = 0;
        player->vy = climb * PLAYER_CLIMB_SPEED;

        /* Jump off the ladder */
        if (input->jump)
        {
            player->vy = -PLAYER_JUMP_SPEED;
            player->on_ladder = false;
            player->ladder_direction = 0;
            player->jump_buffer_timer = 0.0f;
            player->coyote_timer = 0.0f;
            player->jump_cut_ok = true;
            player->jumped = true;
        }
    }
    else
    {
        player->vy += GRAVITY * dt;
        if (player->vy > MAX_FALL_SPEED)
        {
            player->vy = MAX_FALL_SPEED;
        }
        if (player->on_ground)
            player->coyote_timer = PLAYER_COYOTE_TIME;
        else if (player->coyote_timer > 0.0f)
            player->coyote_timer -= dt;
        if (player->jump_buffer_timer > 0.0f &&
            (player->on_ground || player->coyote_timer > 0.0f))
        {
            player->vy = -PLAYER_JUMP_SPEED;
            player->jump_buffer_timer = 0.0f;
            player->coyote_timer = 0.0f;
            player->jump_cut_ok = true;
            player->jumped = true;
        }
        else if (!input->jump_held && player->jump_cut_ok &&
                 player->vy < -PLAYER_JUMP_SPEED * PLAYER_JUMP_CUT_FACTOR)
        {
            /* Released mid-rise: cap the climb so a tap hops. Bounces are not
             * cut — jump_cut_ok only marks rises the player started. */
            player->vy = -PLAYER_JUMP_SPEED * PLAYER_JUMP_CUT_FACTOR;
        }
    }

    if (player->jump_buffer_timer > 0.0f)
        player->jump_buffer_timer -= dt;

    float fall_speed = player->vy > 0.0f ? player->vy : 0.0f;
    float ph = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    level_move(level, &player->x, &player->y, &player->vx, &player->vy,
               PLAYER_W, ph, dt, player->on_ladder, &player->on_ground,
               true);
    if (player->on_ground)
        player->jump_cut_ok = false;

    /* Advance a local clock from the actual locomotion state.  Rendering can
     * use this for unsynchronised, state-driven procedural animation. */
    if (player->on_ladder)
    {
        /* Hold the current grip and pose when the player stops on a ladder.
         * Moving across the rungs is motion too, and it has to drive the same
         * clock: on a frozen one the figure slides sideways off the ladder in
         * a fixed grip, which reads as the pose being dragged rather than as
         * anyone shifting their weight across. Slower than the climb, because
         * a shuffle covers a rung in two beats where a climb covers it in one. */
        if (fabsf(player->vy) > 1.0f)
            player->anim_time += dt * (2.6f + fabsf(player->vy) * 0.018f);
        else if (fabsf(player->vx) > 1.0f)
            player->anim_time += dt * (2.0f + fabsf(player->vx) * 0.016f);
    }
    else if (player->crawling && fabsf(player->vx) > 1.0f)
        player->anim_time += dt * (2.2f + fabsf(player->vx) * 0.020f);
    else if (player->on_ground && fabsf(player->vx) > 1.0f)
        player->anim_time += dt * (3.0f + fabsf(player->vx) * 0.025f);
    else
        player->anim_time += dt;

    return fall_speed;
}
