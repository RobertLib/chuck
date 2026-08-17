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
    case PLAYER_WEAPON_FLASH:
        return player->flashbangs > 0;
    case PLAYER_WEAPON_DECOY:
        /* Never carried, so never out. The cooldown is checked where the throw
         * happens rather than here, because a weapon that vanishes off the ring
         * for a second after being used would step the bumpers onto something
         * else under the player's thumb. */
        return true;
    case PLAYER_WEAPON_COUNT:
        return false;
    }
    return false;
}

/* This order makes one press after a temporary pickup return to the ordinary
 * sidearm, while still keeping the always-available knife in the cycle. The
 * bolts sit after the sidearm — the two things that never run out are adjacent,
 * so the pair the player falls back on is one step apart, and the step out of a
 * spent explosive still lands on the pistol. */
static const PlayerWeapon WEAPON_CYCLE[] = {
    PLAYER_WEAPON_KNIFE,
    PLAYER_WEAPON_BAZOOKA,
    PLAYER_WEAPON_GRENADE,
    /* Beside the grenade, because they are thrown the same way and the player
     * choosing between them is choosing what the next few seconds are for. */
    PLAYER_WEAPON_FLASH,
    PLAYER_WEAPON_PISTOL,
    PLAYER_WEAPON_DECOY};

/*
 * The ring and the enum are two lists of the same weapons, and
 * `select_weapon_step` walks the ring modulo the *enum's* count — so a weapon
 * added to the enum and not to the ring is an index past the end of this
 * array, on the bumpers, in every sector. The same guard `PAGE_ILLUSTRATIONS`
 * keeps against the manual's page count — and **it only keeps it because both
 * arrays are written `[]`**, which is worth saying here because for a long time
 * only this one was. `PAGE_ILLUSTRATIONS` was declared `[MANUAL_PAGE_COUNT]`, so
 * its `sizeof` was the count by construction and its assertion could not fail:
 * the sentence above was true of the ring and false of the thing it pointed at.
 * A length assertion measures an initializer against a count, so the count must
 * not also be the size.
 *
 * **A length is all this can ask, and it used to claim more than that.** The
 * message read "every weapon exactly once", which is the rule; what an
 * assertion over `sizeof` can see is the count, and one weapon written twice
 * with another left out satisfies it exactly. That costs a weapon the bumpers
 * can never reach, mid-fight, with nothing anywhere to say so — and a reader
 * who took the message at its word had no reason to go looking.
 * `test_the_weapon_ring_names_every_weapon_exactly_once` is the half that
 * needs a running player to ask, so it lives in the suite and this says only
 * what it actually holds.
 */
_Static_assert(sizeof(WEAPON_CYCLE) / sizeof(WEAPON_CYCLE[0]) ==
                   (size_t)PLAYER_WEAPON_COUNT,
               "the weapon ring has to be as long as the weapon enum");

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

/* A weapon that has just been spent cannot stay in the hand, but what replaces
 * it is never chosen by the player, so it must never be a one-shot explosive.
 * Walking the cycle put the grenade in Chuck's hand the instant the last rocket
 * left the tube, and the next press of attack — the one aimed at whatever the
 * rocket did not kill — threw it. The sidearm is the weapon nothing is wasted
 * by holding, so the fall-back is the sidearm, or the knife when the clip is
 * dry. An explosive is only ever selected on purpose. */
void player_fall_back_to_sidearm(Player *player)
{
    player->active_weapon = player_weapon_available(player,
                                                    PLAYER_WEAPON_PISTOL)
                                ? PLAYER_WEAPON_PISTOL
                                : PLAYER_WEAPON_KNIFE;
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
    player->flashbangs = 0;
    player->decoy_cooldown = 0.0f;
    player->dragging = false;
    player->dragging_body = 0;
    player->dragging_is_dog = false;
    player->drag_side = 0;
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

/*
 * Opening a sector, and the one thing the sector below is allowed to send with
 * him.
 *
 * **A grenade and a rocket cross the threshold; nothing else does.** The
 * facade is what settles it: nothing on a climb can be thrown or fired at all
 * — the shell clears `shoot` for the whole of `update_facade_playing` and
 * [gameplay_climb.c](gameplay_climb.c) has no notion of a weapon — so the `N`
 * standing mid-wall on every one of the five climbs is a pickup whose entire
 * value is in the sector above it. Wiped at the doorway, it was a detour paid
 * for in wind and thrown bricks that bought nothing whatever, and the
 * campaign's own count of the explosive it lays out was counting four grenades
 * that could never be spent.
 *
 * The sidearm does not travel because it does not need to: `player_reset` hands
 * over a full clip either way. The **weapon in the hand** deliberately does not
 * travel either — "a pickup never arms itself" is a rule about a doorway as
 * much as about a floor tile, and a sector that opened with the last grenade
 * already raised would throw it at the first thing the player pulled the
 * trigger on. It arrives on the pistol, like everything else.
 */
void player_begin_sector(Player *player, const Level *level,
                         const Player *previous)
{
    player_reset(player, level);
    if (previous == NULL)
        return;
    player->grenades = previous->grenades;
    player->bazooka_rockets = previous->bazooka_rockets;
    /* And the flash travels with them, for the same reason: it cannot be
     * thrown on a climb either, so one picked up mid-wall would be a detour
     * that bought nothing. */
    player->flashbangs = previous->flashbangs;
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

    /* Hauling a body is slower than crawling, deliberately: crawling is the
     * other way to be hard to see, and if dragging were the quicker of the two
     * it would be the fastest careful way across a floor. Crawling still wins
     * where both are true, because a man on his elbows has let go of whatever
     * he was pulling — `gameplay_update_body_drag` drops it. */
    if (player->crawling)
        player->vx = move * PLAYER_CRAWL_SPEED;
    else if (player->dragging)
        player->vx = move * PLAYER_DRAG_SPEED;
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
            /*
             * Stepping on from the top also snaps the box *down*, far enough
             * that it actually overlaps the rung, and without it the move is
             * not merely awkward — it deadlocks.
             *
             * A player standing on the top edge overlaps no rung at all, which
             * is the whole reason `descend_from_top` exists: his feet are on
             * the tile and the ladder is under them. But `descend_from_top`
             * also requires `on_ground`, and the first thing climbing does is
             * take `on_ground` away. So the grab has one frame to travel far
             * enough for `player_over_ladder` to take over, and at
             * SIM_STEP_DT it cannot: the climb covers 0.42px of the 1px it
             * needs. The frame after, neither predicate holds, the grip is
             * dropped at the release below, and `level_move` — now called with
             * `climbing` false, so the rung is a one-way platform again —
             * catches the fall and puts him back exactly where he started.
             * Then it happens again. The ladder reads as one-way: up works,
             * down is a man juddering on the spot forever.
             *
             * It is a distance rather than a rule, which is why it hid: at
             * 1/60 the same climb covers 1.67px and latches on the first
             * frame, so every hand-written test that picked its own timestep
             * saw a working ladder.
             * `test_every_ladder_in_the_campaign_can_be_climbed_down` runs at
             * the rate the game actually steps at, and at three others either
             * side of it.
             *
             * Snapping is the fix the line above already uses for the same
             * kind of problem, and it is invisible for the same reason.
             */
            if (descend_from_top && !over_ladder)
            {
                float height = player->crawling ? (float)PLAYER_CRAWL_H
                                                : (float)PLAYER_H;
                int rung = (int)floorf((player->y + height) / TILE_SIZE);
                float onto = (float)rung * TILE_SIZE - height +
                             LADDER_TOP_GRAB_OVERLAP;
                if (onto > player->y)
                    player->y = onto;
            }
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
            /* And the rung stays let go of for a beat, exactly as it does after
             * a stomp bounce and for the same reason: the grab above only asks
             * that the box is over a rung with up or down held, so a jump taken
             * mid-climb was caught again on the very next frame and the climb
             * speed wrote the jump straight back out. Holding up and pressing
             * jump therefore did nothing at all — which is precisely the case
             * the keyboard's separate jump key exists for, since over a ladder
             * `UP` is the climb and the player pressing jump is nearly always
             * already holding it. */
            player->ladder_lockout_timer = PLAYER_LADDER_JUMP_LOCKOUT;
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
