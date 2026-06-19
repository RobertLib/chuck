#include "gameplay_physics.h"

#include "gameplay_world.h"

#include <math.h>

/*
 * Where a crate may be.
 *
 * **It used to ask about masonry and other crates and nothing else, which meant
 * a crate ignored every moving surface in the building.** Measured on the
 * shipped campaign, two of those were reachable by pushing: on sector 6 a crate
 * shoved off the walkway drops down the goods-lift shaft and the deck drives
 * straight through it, and on sector 12 a crate goes through a cracked panel as
 * though it were not there. A third, a moving platform, is the same code and no
 * crate on any shipped map can reach one. None of it costs a run — the route
 * model has never counted a crate as floor, so nothing is certified on one — but
 * a solid object falling through the two things in this game that are drawn as
 * floor is the game contradicting its own picture, and no gate could see it: the
 * soak drew both frames every run and a counter cannot tell a frame that was
 * drawn from a frame anybody could believe.
 *
 * The three answers are deliberately not the same answer, because the three
 * surfaces are not the same thing:
 *
 * - **A lift shaft is refused outright.** Not the deck — the *shaft*. A deck
 *   that stopped a crate would still rise into one resting on the shaft floor,
 *   because `level_update_elevators` asks nothing about crates and must not
 *   start: it is the one moving surface here that travels *upward into* things.
 *   Refusing the column is the whole fix rather than half of it, and it says
 *   something true — a goods lift is machinery, not somewhere a box is parked.
 * - **A falling panel holds one.** A panel only ever falls *away*, so support is
 *   the entire interaction and there is no direction left over to get wrong. It
 *   does not arm: `triggered` is Chuck's weight alone, the same rule a mine
 *   keeps, so a crate stood on a panel is a crate stood on a panel until he
 *   steps up beside it. Once `removed` it stops blocking here and whatever was
 *   on it falls, which is what a panel giving way looks like.
 * - **A moving platform holds one too, with a limit worth writing down.** The
 *   same box test gives it vertical support for free, and its own pass carries
 *   nobody — so a platform slides out from under a crate and drops it, and one
 *   travelling into a crate passes through it sideways. That is better than the
 *   crate falling through it and it is not *right*; what makes it acceptable is
 *   that nothing can reach it. [levels/LEGEND.md](../levels/LEGEND.md) says to
 *   keep `B` out of a `P`'s row, and `test_a_crate_rests_on_what_the_player_does`
 *   holds the campaign to it.
 */
static bool crate_position_clear(const GameplayState *state, int crate_index,
                                 float x, float y)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + CRATE_W - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + CRATE_H - 1.0f) / TILE_SIZE);

    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col)
        {
            if (level_is_solid(&state->level, col, row))
                return false;
            /* Asked by tile rather than by box: a shaft is refused for its whole
             * height, so there is no position inside one for a deck to meet. */
            if (level_tile(&state->level, col, row) == TILE_ELEVATOR_SHAFT)
                return false;
        }

    for (int i = 0; i < state->level.runtime.fall_platform_count; ++i)
    {
        const FallPlatform *panel = &state->level.runtime.fall_platforms[i];
        if (!panel->removed &&
            gameplay_boxes_overlap(x, y, CRATE_W, CRATE_H,
                                   panel->col * (float)TILE_SIZE, panel->y,
                                   (float)TILE_SIZE,
                                   (float)FALL_PLATFORM_H))
            return false;
    }

    for (int i = 0; i < state->level.runtime.moving_platform_count; ++i)
    {
        const MovingPlatform *platform =
            &state->level.runtime.moving_platforms[i];
        if (gameplay_boxes_overlap(x, y, CRATE_W, CRATE_H, platform->x,
                                   platform->row * (float)TILE_SIZE,
                                   (float)TILE_SIZE,
                                   (float)MOVING_PLATFORM_H))
            return false;
    }

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

/*
 * Whether a box at this position is standing in a ladder's way.
 *
 * A ladder is not solid — nothing else in the building asks it to be — so
 * `crate_position_clear` above never had a reason to mention one, and a crate is
 * 28 wide against a 32 tile. Between them that let a box come to rest beside a
 * mouth with most of its width over it, and a climber is stopped by the
 * underside of a crate exactly as he is by a ceiling. Measured on sector 9: 22px
 * of the column covered leaves a 10px gap for a 26px body, and the same climb
 * that makes two tiles with the box where the map puts it makes 31px with the
 * box shoved eight tiles along that floor.
 *
 * Which is a route the model certified and the box then took away.
 * `route_never_strands` is what says every shipped map can be finished from
 * anywhere the player can get to, it is run on the map **as authored**, and a
 * crate is neither floor nor wall to it — the first half of that is written down
 * and the second was not. A pushed crate also outlives a death (`LevelRuntime`,
 * and its own comment says so), so dying does not undo it. Blocking the cell
 * over the mouth makes the way out unreachable from the spawn on sectors 4, 9,
 * 10 and 16.
 *
 * So the rungs are refused the way a lift shaft is refused above, and for the
 * same reason stated the same way: a column something else has to move through
 * is not somewhere a box is parked. What it costs is that a box can no longer be
 * shoved *across* a ladder, which is what a fixture on a wall does to a box.
 *
 * **A row either side, and that is the whole of what the first version got
 * wrong.** A run ends at a slab and the mouth is the tile *above* the top rung,
 * so a box parked over one is standing on the slab at row R with the rung at row
 * R+1 and overlaps no ladder tile at all — an overlap test sees nothing to
 * refuse. The foot of a run is the same thing upside down, where the box is what
 * a climber has to step off onto. The padding is free: no crate on any of the
 * twenty-one shipped maps, the four washrooms included, is authored within a row
 * of a ladder tile, measured at every combination of the two paddings. So this
 * forbids nothing anybody has drawn and only narrows where a box may be shoved.
 *
 * **Asked of the horizontal move alone, and that is not tidiness.** `crate->x`
 * is written in one place, the loop in `move_crate_x`, so refusing it there is
 * what makes "no box is shoved into a ladder's way" true of every position a
 * push can reach. Put in `crate_position_clear` instead it would refuse a
 * *downward* step as well, and `move_crate_y` answers a refused step by stopping
 * there and calling it ground — which would leave a box hanging in mid-air over
 * a ladder, one bug swapped for a worse-looking one. What a fall can still do is
 * `crate_topple_off_the_rungs`'s business, further down.
 */
static bool crate_in_the_rungs(const GameplayState *state, float x, float y)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + CRATE_W - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE) - 1;
    int bottom = (int)floorf((y + CRATE_H - 1.0f) / TILE_SIZE) + 1;

    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col)
            if (level_tile(&state->level, col, row) == TILE_LADDER)
                return true;
    return false;
}

/* The run of ladder columns a box at this position is standing in, or false. */
static bool crate_rung_span(const GameplayState *state, float x, float y,
                            int *first_col, int *last_col)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + CRATE_W - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE) - 1;
    int bottom = (int)floorf((y + CRATE_H - 1.0f) / TILE_SIZE) + 1;
    bool any = false;

    for (int col = left; col <= right; ++col)
        for (int row = top; row <= bottom; ++row)
        {
            if (level_tile(&state->level, col, row) != TILE_LADDER)
                continue;
            if (!any)
                *first_col = col;
            *last_col = col;
            any = true;
            break;
        }
    return any;
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
    /*
     * Whether it is in there already, asked once before the walk rather than
     * per step, and it is what keeps this rule from being worse than the thing
     * it fixes. Refusing every overlapping position outright — the first
     * version of this — refuses the steps that *leave* one too, because every
     * step out is still overlapping until the last of them. Measured, that
     * turned nine fall-ins across the campaign into nine boxes nothing could
     * ever shove out of the rungs again: a soft-lock in place of a blocked
     * climb the player could at least undo. So a push may not put a box in
     * there, and a box that is in there may be pushed either way, which is the
     * only shape of this rule that cannot wedge anything.
     */
    bool in_the_rungs = crate_in_the_rungs(state, crate->x, crate->y);
    while (remaining > 0.0f)
    {
        float step = fminf(remaining, 1.0f) * sign;
        int enemy_index =
            crate_blocking_enemy(state, crate->x + step, crate->y);
        if (!crate_position_clear(state, crate_index,
                                  crate->x + step, crate->y) ||
            (!in_the_rungs &&
             crate_in_the_rungs(state, crate->x + step, crate->y)) ||
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

/*
 * A box does not come to rest in the rungs: it topples off.
 *
 * `move_crate_x` refuses to *push* one in, and measured over every crate on
 * every shipped map that closes every way a player has of putting one there.
 * What it cannot see is a **fall**: a box shoved off a ledge keeps the x it left
 * with, and a run that starts a storey lower than the floor it was shoved along
 * is a column the refusal had no reason to object to. Before the padding above,
 * nine of the campaign's twenty-six boxes could be left sitting in one.
 *
 * So the landing is corrected rather than the fall: the box is moved to the
 * nearer side of the run it is in, which is the shorter of the two and the one
 * that reads as it settling rather than sliding across the floor. In one move
 * rather than as a drift, because a drift is a velocity, a velocity is subject to
 * friction, and a box that ran out of momentum halfway out of the rungs is the
 * state this exists to prevent.
 *
 * If neither side will take it the box stays where it is and this is tried again
 * next frame. It is written that way rather than asserted because the
 * alternative to "it stays" is "it goes into masonry" — and no shipped map can
 * reach it, since a run walled on both sides has no position a crate could have
 * arrived from.
 */
static void crate_topple_off_the_rungs(GameplayState *state, int index)
{
    Crate *crate = &state->level.runtime.crates[index];
    int first = 0;
    int last = 0;

    if (!crate->on_ground || crate->vx != 0.0f)
        return;
    if (!crate_rung_span(state, crate->x, crate->y, &first, &last))
        return;

    float to_left = crate->x - ((float)first * TILE_SIZE - (float)CRATE_W);
    float to_right = (float)(last + 1) * TILE_SIZE - crate->x;
    int nearer = to_left <= to_right ? -1 : 1;

    /*
     * Both sides in the same frame, and that second attempt is not belt and
     * braces: a mouth with masonry one column out has a near side the box
     * cannot move along **at all**, and this function only reruns next frame
     * from a position that has not changed — so one attempt is one attempt
     * for ever. The distances are the ones measured above rather than measured
     * again between the two, because a partial move is healed by the next frame
     * finding a nearer side: only "it did not move" needs answering here.
     */
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        move_crate_x(state, index, nearer < 0 ? -to_left : to_right, NULL);
        if (!crate_in_the_rungs(state, crate->x, crate->y))
            return;
        nearer = -nearer;
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

        /* After the friction rather than before it, because what this asks is
         * whether the box has *stopped* in there. */
        crate_topple_off_the_rungs(state, i);
    }
}

/*
 * Put a body out from under a crate, on the side it was leaving by if there is
 * room and the other side if there is not.
 *
 * **Neither branch used to ask the building anything, and that shipped a live
 * dog inside masonry.** A crate is 28 wide against a 32 tile and
 * `crate_position_clear` reads its right edge as `x + w - 1`, so one shoved
 * hard against a wall settles about a pixel inside the wall column; the
 * ejection then set the dog's left edge to `crate->x + CRATE_W`, and a body 24
 * wide starting a pixel inside a 32 tile is a body *wholly* inside it. What
 * followed is worse than the overlap: the next step of the animal's own walk
 * back to its post ran `level_move`'s left clamp, which answers a solid
 * left-edge column with `x = (col + 1) * TILE_SIZE` — one tile further out —
 * and on a wall at the edge of the map that is off the map altogether, for the
 * rest of the visit. Measured: reproducible by holding one direction, on
 * sectors 4, 6, 8, 9, 10, 12, 16 and 17 and in the penthouse washroom, in as
 * little as 0.02s; and found unaided by a monkey on the washroom, which is the
 * one map with a crate, a dog and a hard wall on one row.
 *
 * The other side rather than nowhere, because a crate shoved at an animal is a
 * *mechanic* — `gameplay_kill_dog_with_crate` is what a box dropped on one
 * does — so the horizontal shove has to keep meaning something. Popping out
 * behind the box reads as the dog scrambling past it and leaves it in the
 * world, patrolling. When neither side fits, the body stays where it is and
 * overlaps: a picture nobody can act on beats a state nothing can recover
 * from.
 *
 * `move_crate_x` asks `crate_blocking_enemy` about the men and nothing about
 * the animals, which is why a crate passes through a dog at all. That stays as
 * it is on purpose — a dog that stopped a crate would stop the shove the
 * mechanic is played for — and it is exactly why the *landing* has to be
 * checked here instead.
 */
static bool eject_from_crate(const GameplayState *state, const Crate *crate,
                             float *x, float y, float w, float h,
                             Stance stance, int side, int *out_side)
{
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        int try_side = attempt == 0 ? side : -side;
        float candidate =
            try_side > 0 ? crate->x + CRATE_W : crate->x - w;
        if (!gameplay_box_tiles_clear(state, candidate, y, w, h, stance))
            continue;
        *x = candidate;
        if (out_side != NULL)
            *out_side = try_side;
        return true;
    }
    return false;
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
            /* Pushing right puts him on the crate's left, so the side is the
             * opposite of the shove. See `eject_from_crate`: the guard is inert
             * on every ordinary push, because the side he is put back on is the
             * side he walked in from. */
            if (eject_from_crate(state, crate, &player->x, player->y, PLAYER_W,
                                 height, player_stance(player), -direction,
                                 NULL))
            {
                player->vx = 0.0f;
            }
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
        int side = previous_x + DOG_W * 0.5f < crate->x + CRATE_W * 0.5f
                       ? -1
                       : 1;
        int landed = side;
        if (eject_from_crate(state, crate, &dog->x, dog->y, DOG_W, DOG_H,
                             STANCE_UPRIGHT, side, &landed))
        {
            dog->dir = landed;
            dog->vx = 0.0f;
        }
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
                              float x, float y, float w, float h,
                              Stance stance)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + w - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + h - 1.0f) / TILE_SIZE);
    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col)
            if (level_blocks_stance(&state->level, col, row, stance))
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

    /*
     * Carried no further than the deck itself can go.
     *
     * This runs *before* `level_update_elevators`, on purpose: the rider is
     * moved to where the lift is about to be so that
     * `gameplay_resolve_player_crush` gets a look at it, which is the whole
     * reason a rider who boarded off-centre is squeezed clear instead of killed.
     * But it integrated the lift's velocity without the clamp that
     * `level_update_elevators` applies a few lines later, so on the step the
     * lift reached the top of its run the player was carried a fraction of a
     * pixel *past* it — and a fraction was enough, because the crush check reads
     * the row of his top edge and one pixel is a different row.
     *
     * On sector 6 that row is the ceiling the shaft is drilled up to, so the
     * lift killed anybody who rode it to the top. `gameplay_ride_platforms` puts
     * him back on the deck at the end of the same frame, so the position was
     * always corrected and only the crush check ever saw the overshoot: a bug
     * that existed for exactly one pass of one frame and cost the whole run.
     *
     * Giving the shaft its rider's headroom (see `top_limit` in level.c) is the
     * other half and neither half is sufficient alone — with the headroom and
     * without this clamp, the overshoot simply lands one row lower and crushes
     * him against the same slab.
     */
    float height = state->player.crawling ? (float)PLAYER_CRAWL_H
                                          : (float)PLAYER_H;
    float highest_reach = elevator->top_limit - height;
    state->player.y += elevator->vy * dt;
    if (state->player.y < highest_reach)
        state->player.y = highest_reach;
    state->player.vy = 0.0f;
}

bool gameplay_resolve_player_crush(GameplayState *state)
{
    Player *player = &state->player;
    float height = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    Stance stance = player_stance(player);
    int col_left = (int)floorf(player->x / TILE_SIZE);
    int col_right = (int)floorf((player->x + PLAYER_W - 1.0f) / TILE_SIZE);
    int row_top = (int)floorf(player->y / TILE_SIZE);

    /* Asked in the posture he is actually in, which is the whole of what makes
     * a duct survivable. The tile a crawler is *inside* is the tile this loop
     * reads, so an upright answer would find masonry over the head of every man
     * in every shaft, push him at both walls, find those blocked too, and take
     * a heart — a hazard that reads on screen as nothing at all happening. */
    bool slab_overhead = false;
    for (int col = col_left; col <= col_right && !slab_overhead; ++col)
        slab_overhead = level_blocks_stance(&state->level, col, row_top, stance);
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
                                     PLAYER_W, height, stance))
        {
            player->x = into_right;
            return false;
        }
        if (gameplay_box_tiles_clear(state, into_left, player->y,
                                     PLAYER_W, height, stance))
        {
            player->x = into_left;
            return false;
        }
    }

    gameplay_hit_player(state);
    return true;
}

/*
 * Move a box sideways with the plate it is standing on, but only into space it
 * fits in.
 *
 * One rule shared by everything that can be standing on a platform, because it
 * was written for the player and the player is not the only one who gets there.
 * `level_move` already holds *every* body up on a plate — it treats one as the
 * one-way platform it is, beside the falling panel it already knew — so a guard
 * who walks onto a platform stands on it correctly. Nothing carried him, so the
 * plate slid out from under him and he dropped off the trailing edge 0.44s
 * later, measured. That is not a corner: `enemy_floor_in_col` counts a platform
 * as floor *on purpose*, under a comment about a pursuing guard not mistaking
 * one for a gap, so the AI steers bodies onto plates — and it lands them there
 * for 716 frames on sector 14 and 1512 on sector 17 across twenty-four minutes
 * of play a sector. Sector 17's plate is eleven tiles long and it is the roof.
 *
 * So it is the crate's limit with the object swapped, and the crate's own note
 * is what made it easy to leave: that one says "its own pass carries nobody, so
 * it slides out from under a crate and drops it, and nothing on any shipped map
 * can reach one". The second clause is what does not carry over.
 *
 * The clearance test is the reason this is a function rather than a line. A
 * platform's limits are the ends of its own row's clear run, so the *platform*
 * never reaches a wall; the rider does, because a tile is 32 and a body is 26,
 * and being over the plate is not the same as fitting where the plate is going.
 */
static void carry_with_plate(const GameplayState *state,
                             const MovingPlatform *plate, float *x, float y,
                             float w, float h, Stance stance, float dt)
{
    float carried = *x + plate->vx * dt;
    if (gameplay_box_tiles_clear(state, carried, y, w, h, stance))
        *x = carried;
}

/*
 * The plate a box is resting on, or NULL.
 *
 * Resting rather than overlapping, and the difference is which question the
 * caller is asking: the player's own block below is looking for a plate to
 * *land* on and wants a forgiving window for it, while this is asked of a body
 * `level_move` has already settled, so the feet are on the top surface to
 * within a pixel. A window here would pick up somebody falling past.
 */
static const MovingPlatform *plate_under(const GameplayState *state,
                                         float x, float y, float w, float h)
{
    for (int i = 0; i < state->level.runtime.moving_platform_count; ++i)
    {
        const MovingPlatform *plate =
            &state->level.runtime.moving_platforms[i];
        int plate_col = (int)floorf(plate->x / TILE_SIZE);
        int left = (int)floorf(x / TILE_SIZE);
        int right = (int)floorf((x + w - 1.0f) / TILE_SIZE);
        if (plate_col < left || plate_col > right)
            continue;
        if (fabsf((y + h) - plate->row * (float)TILE_SIZE) < 2.0f)
            return plate;
    }
    return NULL;
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
            /*
             * Carry the rider sideways with the platform — but only into air.
             *
             * The platform's limits are the ends of its own row's clear run, so
             * the *platform* never reaches a wall; the rider does. A tile is 32
             * and the box is 26, and the ride only asks that the player's
             * centre is over the platform, so somebody standing on its edge
             * hangs 13px past it and meets the masonry the platform stops
             * short of. Pushed in regardless, they stayed there: `level_move`
             * resolves the horizontal axis only when `vx` is non-zero, and a
             * rider holding no direction has none, so the frame drew Chuck a
             * third of the way inside the wall until he happened to touch a
             * key.
             */
            carry_with_plate(state, platform, &player->x, player->y,
                             PLAYER_W, height, player_stance(player), dt);
            break;
        }
    }

    /*
     * And everybody else the plate is holding up, which for a long time was
     * nobody.
     *
     * `level_move` settles a guard, a dog or a janitor onto a plate the same way
     * it settles them onto a falling panel, so the support has always been
     * right; only the carry was the player's alone. See `carry_with_plate` for
     * what that cost and where it was measured.
     *
     * Ordering: this pass runs after `level_update_moving_platforms` and before
     * `gameplay_ai_update_movement`, so a body is carried on the strength of
     * where last frame's `level_move` left it, against a plate that has already
     * taken this frame's step. The displacement per frame is the same either
     * way — it is one `vx * dt` — and a body the plate has just left behind
     * simply is not carried, which is the outcome wanted. Doing it here rather
     * than after each of the three movement calls is what keeps "which plate is
     * under this box" in one place.
     *
     * The janitor is deliberately not in this loop. He is the one body whose own
     * floor test (`janitor_has_floor_ahead`) refuses every moving surface in the
     * building — masonry and ladders only — so he never steps onto a plate, and
     * an arm nothing can reach is an arm nobody has checked.
     */
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead || enemy->climbing || enemy->on_elevator >= 0)
            continue;
        const MovingPlatform *plate =
            plate_under(state, enemy->x, enemy->y, ENEMY_W, ENEMY_H);
        if (plate != NULL)
        {
            carry_with_plate(state, plate, &enemy->x, enemy->y,
                             ENEMY_W, ENEMY_H, STANCE_UPRIGHT, dt);
        }
    }
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (dog->dead)
            continue;
        const MovingPlatform *plate =
            plate_under(state, dog->x, dog->y, DOG_W, DOG_H);
        if (plate != NULL)
        {
            carry_with_plate(state, plate, &dog->x, dog->y,
                             DOG_W, DOG_H, STANCE_UPRIGHT, dt);
        }
    }
}
