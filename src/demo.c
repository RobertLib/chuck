#include "demo.h"

#include <math.h>
#include <string.h>

/*
 * How long one lap of the script runs before it starts again. A sector is
 * smoked for a few seconds and a climb for the same, so the lap is short
 * enough that a short dwell still sees most of it and long enough that each
 * beat gets frames rather than a single tick.
 */
#define DEMO_LAP 8.0f

/* How long the hand walks one way before trying the other, when it has nothing
 * in the sector to walk towards. */
#define DEMO_WANDER_TIME 1.4f

/*
 * Whether this frame is the one that crossed `mark`, which is the whole reason
 * the hand keeps the previous beat.
 *
 * Half the fields on `Input` are edges the shell consumes and clears once a
 * frame, and the first draft of this script set them from a window — `beat <
 * 2.4f` and the like — which at sixty steps a second is not one press but
 * twelve. Twelve presses of `switch_weapon` walk a four-weapon cycle three
 * times round and land back where they started, so the tube was selected and
 * deselected inside a beat and the two vertical bazooka drawings stayed at
 * zero. A press is a crossing, not an interval.
 */
static bool demo_edge(float previous, float current, float mark)
{
    /* The lap wraps, and a mark just after the wrap has to still be crossed
     * exactly once: measure the previous beat as negative rather than letting
     * it sit a whole lap ahead of the current one. */
    if (current < previous)
        previous -= DEMO_LAP;
    return previous < mark && current >= mark;
}

void demo_hand_init(DemoHand *hand)
{
    memset(hand, 0, sizeof(*hand));
    hand->wander_dir = 1;
    hand->wander_timer = DEMO_WANDER_TIME;
}

void demo_grant_loadout(GameplayState *state)
{
    /*
     * Two of each of the one-shots, and the reason is the lap below rather
     * than generosity: every weapon is fired once on the floor and once from a
     * ladder, because the vertical shot is a different drawing from the
     * horizontal one and not the same drawing rotated. One rocket would cover
     * whichever of the two came first and leave the other at zero, which is
     * exactly the state this file was written to get out of.
     *
     * The sidearm is filled rather than topped up so the count is the same in
     * every sector and the script cannot run dry halfway up the campaign.
     */
    state->player.bullets = MAX_AMMO;
    state->player.grenades = 2;
    state->player.bazooka_rockets = 2;
}

/*
 * Where the hand is trying to get to, if the sector gives it anywhere.
 *
 * The active terminal is the one landmark worth steering at, and it is worth
 * three of the twelve uncovered functions on its own: standing at it draws the
 * hacking pose, holding the button raises the alarm — which is the alarm
 * lighting pass — and the alarm walks the floor's guards to the console, which
 * is how the hand gets something to shoot without having to hunt for it. It is
 * also the only steering in here; everything else is a clock.
 *
 * Returns false on a climb, and on any interior whose terminal the seed did not
 * light, in which case the hand paces instead.
 */
static bool demo_target_x(const GameplayState *state, float *out_x)
{
    if (state->level.map.mode == LEVEL_MODE_FACADE)
        return false;
    int index = state->level.runtime.active_terminal_index;
    if (index < 0 || index >= state->level.map.terminal_count)
        return false;
    *out_x = ((float)state->level.map.terminals[index].col + 0.5f) *
             (float)TILE_SIZE;
    return true;
}

/*
 * The nearest rung, and why half the lap is spent looking for one.
 *
 * **A vertical shot is only ever fired from a ladder.**
 * `player_ladder_attack_direction` returns nought unless `on_ladder`, so up and
 * down do not aim a standing figure — they are the climb. The first draft of
 * this script held up and pulled the trigger on the floor and got a horizontal
 * shot every time, which is why `draw_vertical_bazooka_weapon`,
 * `draw_vertical_rocket_sprite` and `vertical_rocket_rect` stayed at nought
 * while everything else in the list came up: the hand was firing the wrong
 * thing from the wrong place and the coverage said so.
 *
 * Nearest by column, and the row is not consulted at all: the hand cannot
 * pathfind and does not need to. Walking at the column is enough on a floor
 * that has a ladder anywhere along it, and a sector where it is not is a
 * sector this lap covers a little less of — which the run reports as coverage
 * rather than hiding as a pass.
 */
static bool demo_ladder_x(const GameplayState *state, float from_x,
                          float *out_x)
{
    const LevelMap *map = &state->level.map;
    if (map->mode == LEVEL_MODE_FACADE)
        return false;

    bool found = false;
    float best = 0.0f;
    for (int col = 0; col < map->width; ++col)
    {
        bool has_rung = false;
        for (int row = 0; row < map->height && !has_rung; ++row)
            has_rung = map->tiles[row][col] == TILE_LADDER;
        if (!has_rung)
            continue;
        float x = ((float)col + 0.5f) * (float)TILE_SIZE;
        if (!found || fabsf(x - from_x) < fabsf(best - from_x))
        {
            best = x;
            found = true;
        }
    }
    if (found)
        *out_x = best;
    return found;
}

void demo_hand_drive(DemoHand *hand, const GameplayState *state,
                     Input *input, float dt)
{
    hand->time += dt;

    /* Cleared rather than added to: this stands in for the keyboard read, so
     * anything left set from the previous frame would be a press the script
     * never made — and half of these are edges. */
    memset(input, 0, sizeof(*input));

    float previous_beat = fmodf(hand->time - dt, DEMO_LAP);
    float beat = fmodf(hand->time, DEMO_LAP);
    float player_x = state->player.x + PLAYER_W * 0.5f;

    /*
     * A climb is four-way and has nothing to shoot, so it is the whole of the
     * script up there: hold up, and lean sideways past the cornices.
     */
    if (state->level.map.mode == LEVEL_MODE_FACADE)
    {
        hand->wander_timer -= dt;
        if (hand->wander_timer <= 0.0f)
        {
            hand->wander_dir = -hand->wander_dir;
            hand->wander_timer = DEMO_WANDER_TIME;
        }
        input->left = hand->wander_dir < 0;
        input->right = hand->wander_dir > 0;
        input->up = true;
        return;
    }

    /*
     * Inside, a lap is in two halves and they want different ground.
     *
     * The floor half walks at the sector's live terminal, which is worth three
     * drawings on its own: standing on it is the hacking pose, holding the
     * button raises the alarm — the alarm lighting pass — and the alarm walks
     * the floor's guards to the console, which is how the hand gets something
     * to shoot without having to hunt for it.
     *
     * The ladder half walks at the nearest rung, because a vertical shot is
     * only ever fired from one; see `demo_ladder_x`.
     */
    bool ladder_half = beat >= DEMO_LAP * 0.5f;

    /* ---- Where it walks ------------------------------------------------ */

    float target_x = 0.0f;
    bool steering = ladder_half ? demo_ladder_x(state, player_x, &target_x)
                                : demo_target_x(state, &target_x);
    bool at_target =
        steering && fabsf(target_x - player_x) <= (float)TILE_SIZE * 0.4f;

    if (steering && !at_target)
    {
        input->left = target_x < player_x;
        input->right = target_x > player_x;
    }
    else if (!steering)
    {
        hand->wander_timer -= dt;
        if (hand->wander_timer <= 0.0f)
        {
            hand->wander_dir = -hand->wander_dir;
            hand->wander_timer = DEMO_WANDER_TIME;
        }
        input->left = hand->wander_dir < 0;
        input->right = hand->wander_dir > 0;
    }

    /* ---- What it presses ----------------------------------------------- */

    /* Held throughout: standing on the terminal is what draws the hacking pose
     * and starts the alarm, and holding it costs nothing anywhere else. */
    input->interact = true;

    /* Ladders, doorways and the stair out all answer this, so the hand covers
     * a sector rather than pacing one room of it. */
    input->use_door = demo_edge(previous_beat, beat, 0.05f);

    /*
     * Every weapon fired, and fired from wherever the hand is standing.
     *
     * The cycle is walked one press at a time rather than aimed at a slot,
     * because which slot holds what depends on the loadout and a script that
     * knew the order would be a script that broke the day a weapon was added.
     * Four presses spread over the half means each of the four gets a turn
     * whichever one the cycle opened on.
     */
    float half_start = ladder_half ? DEMO_LAP * 0.5f : 0.0f;
    for (int slot = 0; slot < 4; ++slot)
    {
        float base = half_start + 0.6f + (float)slot * 0.85f;
        if (demo_edge(previous_beat, beat, base))
            input->switch_weapon = true;
        if (demo_edge(previous_beat, beat, base + 0.45f))
            input->shoot = true;
    }

    if (ladder_half)
    {
        /*
         * On the rung, up is the climb and it is also the aim: with the ladder
         * under him, up-and-shoot is the vertical attack and down-and-shoot is
         * the one below. Fired without a rung the very same press is an
         * ordinary shot straight ahead, which is what the first draft of this
         * script did for every one of its laps.
         */
        if (state->player.on_ladder)
        {
            input->left = false;
            input->right = false;
            /* Down for the back half of the half, so both aims are taken and
             * neither cancels the other — `player_ladder_attack_direction`
             * reads up-with-down as no aim at all. */
            bool aim_down = beat > DEMO_LAP * 0.78f;
            input->up = !aim_down;
            input->down = aim_down;
        }
        else
        {
            /* Still walking at it: up is what mounts a rung on arrival. */
            input->up = at_target;
        }
        return;
    }

    if (beat < 0.55f)
    {
        /* Opens on its elbows, which is a pose of its own and the only way a
         * gas canister is ever shot. Firing from down here draws the prone
         * flash rather than the standing one. */
        input->down = true;
        if (demo_edge(previous_beat, beat, 0.25f))
            input->shoot = true;
    }
    else if (beat > DEMO_LAP * 0.5f - 0.6f)
    {
        /* The last of the floor half: a jump, and the one press of
         * `switch_weapon_back` the lap owes the cycle's other direction. */
        if (demo_edge(previous_beat, beat, DEMO_LAP * 0.5f - 0.5f))
            input->jump = true;
        input->jump_held = beat > DEMO_LAP * 0.5f - 0.5f &&
                           beat < DEMO_LAP * 0.5f - 0.25f;
        if (demo_edge(previous_beat, beat, DEMO_LAP * 0.5f - 0.2f))
            input->switch_weapon_back = true;
    }
}
