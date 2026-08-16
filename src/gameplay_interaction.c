#include "gameplay_interaction.h"

#include "gameplay_world.h"

#include <math.h>

static float reinforcement_delay(Rng *rng, float minimum, float maximum)
{
    return minimum + (maximum - minimum) * rng_unit(rng);
}

/*
 * Standing at the terminal with the button held, and what that costs.
 *
 * **The reinforcements belong to the terminal, not to the alarm.** They used to
 * be queued off `alarm_started` — the frame the hack put the alarm up — which
 * quietly made them conditional on the building having been quiet beforehand: a
 * player who had already been seen, whose alarm was already ringing, hacked the
 * same terminal and nobody came out of the doors at all. That is the incentive
 * exactly backwards. The manual sells the console as the loud way through
 * ("Hacking wakes the building: guards are sent to the terminal you used"), and
 * a rule that waives the cost for the player who was *caught first* rewards
 * getting spotted on the three sectors that have both a terminal and a door
 * pair to send anyone out of — 4, 8 and 14. Nothing on screen explained it,
 * because there was nothing to explain: it was the gate reading the alarm when
 * what it meant to read was the hack.
 *
 * So it is the *start of a hack* that calls them, and `was_hacking` is the only
 * thing needed to see one: `terminal_hacking` is recomputed every frame, so the
 * value it is about to lose is the previous frame's answer. Two guards keep it
 * honest. A batch already on its way is not doubled, so tapping the button does
 * not stack them up — and since `gameplay_update_alarm` clears the pending
 * count when the alarm lapses, that guard can never wedge permanently. And the
 * alarm itself is still raised only when it was not already up, because
 * `gameplay_trigger_alarm` is what makes the whole floor turn round and doing
 * that twice is not a second event.
 */
void gameplay_prepare_terminal(GameplayState *state, const Input *input,
                               float dt)
{
    bool was_hacking = state->terminal_hacking;
    state->terminal_in_range = gameplay_player_near_active_terminal(state);
    state->terminal_hacking = state->terminal_in_range &&
                              input->interact &&
                              state->player.on_ground &&
                              !state->player.on_ladder;
    bool alarm_started = state->terminal_hacking &&
                         !gameplay_alarm_active(state);
    bool hack_began = state->terminal_hacking && !was_hacking;
    if (state->terminal_hacking)
    {
        const Terminal *terminal =
            &state->level.map.terminals[state->level.runtime.active_terminal_index];
        state->player.x = terminal->col * (float)TILE_SIZE +
                          ((float)TILE_SIZE - PLAYER_W) * 0.5f;
        if (alarm_started)
        {
            gameplay_trigger_alarm(state,
                                   (terminal->col + 0.5f) * TILE_SIZE,
                                   (terminal->row + 0.5f) * TILE_SIZE, -1);
        }
        /* Held after the trigger above, which sets the same two values: while
         * the hack runs, the floor converges on the console rather than on
         * wherever Chuck was last seen, however the alarm came to be up. */
        state->terminal_alarm_timer = ALARM_CALM_TIME;
        state->alarm_target_x = (terminal->col + 0.5f) * TILE_SIZE;
        state->alarm_target_y = (terminal->row + 0.5f) * TILE_SIZE;
        if (hack_began && state->level.map.door_count > 0 &&
            state->terminal_reinforcements_pending <= 0)
        {
            int count_range = TERMINAL_REINFORCEMENT_MAX_COUNT -
                              TERMINAL_REINFORCEMENT_MIN_COUNT + 1;
            state->terminal_reinforcements_pending =
                TERMINAL_REINFORCEMENT_MIN_COUNT +
                rng_range(&state->rng, count_range);
            state->terminal_reinforcement_timer =
                reinforcement_delay(&state->rng,
                                    TERMINAL_REINFORCEMENT_FIRST_MIN,
                                    TERMINAL_REINFORCEMENT_FIRST_MAX);
        }
    }
    else
    {
        state->terminal_hack_progress = 0.0f;
        state->terminal_hack_tick_timer = 0.0f;
    }
    (void)dt;
}

bool gameplay_advance_terminal(GameplayState *state,
                               CampaignState *campaign, float dt)
{
    if (!state->terminal_hacking)
        return false;

    state->terminal_hack_progress += dt;
    state->terminal_hack_tick_timer -= dt;
    if (state->terminal_hack_tick_timer <= 0.0f)
    {
        game_events_sound(&state->events, SFX_CARD_SCAN);
        state->terminal_hack_tick_timer = 0.45f;
    }
    if (state->terminal_hack_progress < TERMINAL_HACK_TIME)
        return false;

    state->terminal_hack_progress = TERMINAL_HACK_TIME;
    state->level.runtime.terminal_hacked = true;
    campaign->score += 250;
    /* A finished hack is real progress: a later death resumes here. */
    gameplay_bank_checkpoint(state);
    bool was_unlocked = state->level.runtime.exit_unlocked;
    gameplay_unlock_exit(state);
    return !was_unlocked && state->level.runtime.exit_unlocked;
}

int gameplay_player_door_index(const GameplayState *state)
{
    if (!state->player.on_ground || state->teleport_cooldown > 0.0f)
        return -1;

    int center_col = (int)floorf((state->player.x + PLAYER_W * 0.5f) /
                                 TILE_SIZE);
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    int center_row = (int)floorf((state->player.y + player_h * 0.5f) /
                                 TILE_SIZE);
    for (int index = 0; index < state->level.map.door_count; ++index)
    {
        const Door *door = &state->level.map.doors[index];
        if (door->col == center_col && door->row == center_row)
            return index;
    }

    return -1;
}

SublevelDoorAction gameplay_player_sublevel_door_action(
    const GameplayState *state)
{
    if (!state->player.on_ground || state->teleport_cooldown > 0.0f)
        return SUBLEVEL_DOOR_NONE;

    int center_col = (int)floorf((state->player.x + PLAYER_W * 0.5f) /
                                 TILE_SIZE);
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    int center_row = (int)floorf((state->player.y + player_h * 0.5f) /
                                 TILE_SIZE);

    if (state->level.map.has_sublevel_entrance &&
        center_col == state->level.map.sublevel_entrance_col &&
        center_row == state->level.map.sublevel_entrance_row)
    {
        return SUBLEVEL_DOOR_ENTER;
    }
    if (state->level.map.has_sublevel_return &&
        center_col == state->level.map.sublevel_return_col &&
        center_row == state->level.map.sublevel_return_row)
    {
        return SUBLEVEL_DOOR_RETURN;
    }
    return SUBLEVEL_DOOR_NONE;
}

SublevelDoorAction gameplay_use_sublevel_door(GameplayState *state,
                                              Input *input)
{
    SublevelDoorAction action =
        gameplay_player_sublevel_door_action(state);
    if (!input->use_door || action == SUBLEVEL_DOOR_NONE)
        return SUBLEVEL_DOOR_NONE;

    input->use_door = false;
    return action;
}

void gameplay_use_door(GameplayState *state, Input *input)
{
    int index = gameplay_player_door_index(state);
    if (!input->use_door || index < 0)
    {
        input->use_door = false;
        return;
    }

    int pair = index ^ 1;
    if (pair < state->level.map.door_count)
    {
        const Door *destination = &state->level.map.doors[pair];
        float player_h = state->player.crawling
                             ? (float)PLAYER_CRAWL_H
                             : (float)PLAYER_H;
        state->player.x = destination->col * TILE_SIZE +
                          (TILE_SIZE - PLAYER_W) * 0.5f;
        state->player.y = (destination->row + 1) * TILE_SIZE - player_h;
        state->player.vx = 0.0f;
        state->player.vy = 0.0f;
        state->teleport_cooldown = TELEPORT_COOLDOWN;
        /* Passing a door banks the arrival side, so a death on the new
         * storey does not replay the trip to the door. */
        gameplay_bank_checkpoint(state);
        game_events_sound(&state->events, SFX_DOOR);
    }
    else
    {
        /* Doors are matched 0<->1, 2<->3, so an odd number of them leaves the
         * last one with nowhere to go. The editor calls that an error and no
         * shipped sector has it, but a press that does nothing at all still has
         * to say so: it is the same rule the dry clip and the busy launcher
         * keep, and a silent door reads as the key having missed rather than as
         * the door being dead. */
        game_events_sound(&state->events, SFX_EMPTY_CLICK);
    }
    input->use_door = false;
}

/*
 * A pickup that would change nothing is left where it is.
 *
 * This is the rule `gameplay_update_ammo_drops` already keeps — *"Left lying
 * until it is actually useful, so a full magazine does not eat the pickup"* —
 * said for the boxes on the floor, and it was missing from all three of the
 * ones that cannot come back. Walking over a second `N` while already carrying
 * a grenade set `collected` with a nought respawn timer and played
 * `SFX_PICKUP_GRENADE`: the scarcest thing in the sector destroyed by crossing
 * a tile, announced with the sound of a successful pickup. Sector 12 carries
 * two grenades, sectors 10, 12 and 15 two medkits apiece, and every restroom
 * hands out the grenade the campaign's own budget is balanced on — so the case
 * is not a corner, it is the middle of four maps.
 *
 * The boxed magazine is deliberately *not* on this list, and the difference is
 * the respawn: `ITEM_GUN` comes back on `ITEM_RESPAWN_TIME`, so taking one with
 * a full clip costs the player nothing and the box is there again before it is
 * wanted. Nothing else in the game gets a second chance, which is exactly why
 * nothing else may be spent on a counter that is already full.
 */
static bool item_would_be_wasted(const GameplayState *state,
                                 const CampaignState *campaign,
                                 ItemType type)
{
    switch (type)
    {
    case ITEM_GRENADE:
        return state->player.grenades > 0;
    case ITEM_BAZOOKA:
        return state->player.bazooka_rockets >= BAZOOKA_AMMO;
    case ITEM_MEDKIT:
        /* The kit answers the hearts first and the spare lives second, so it
         * is only wasted when both are already at their cap. */
        return state->player.hp >= gameplay_player_max_hp(state) &&
               campaign->lives >= MAX_LIVES;
    case ITEM_CARD:
    case ITEM_GUN:
        break;
    }
    return false;
}

void gameplay_collect_items(GameplayState *state, CampaignState *campaign,
                            float dt)
{
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    for (int i = 0; i < state->level.runtime.item_count; ++i)
    {
        Item *item = &state->level.runtime.items[i];
        if (!item->collected &&
            !item_would_be_wasted(state, campaign, item->type) &&
            gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, player_h,
                                   item->x - 8.0f, item->y - 8.0f,
                                   16.0f, 16.0f))
        {
            item->collected = true;
            /* The magazine is the only thing that comes back.
             *
             * It has to: the sidearm is what the sector is played with, and a
             * player who has spent it and cannot find another box is playing
             * the rest of the floor with a knife. Nothing else is in that
             * position. The grenade used to come back with it, which made one
             * `N` an unlimited supply at ten seconds apiece — enough to clear
             * a floor a blast at a time, and enough to open every blocked-up
             * patch in the campaign without ever needing the bazooka the
             * patches were placed for. A one-shot explosive that regrows is
             * not a decision about when to spend it. */
            item->respawn_timer =
                item->type == ITEM_GUN ? ITEM_RESPAWN_TIME : 0.0f;
            switch (item->type)
            {
            case ITEM_CARD:
                campaign->score += 100;
                /* Any card found is progress worth resuming at. */
                gameplay_bank_checkpoint(state);
                if (i == state->level.runtime.active_card_index)
                {
                    state->level.runtime.items_remaining = 0;
                    /*
                     * The live card always answers, and only the door's own
                     * fanfare is conditional.
                     *
                     * `gameplay_unlock_exit` returns without a sound when there
                     * is no door to open — an interior whose stair core is
                     * welded and whose route out is the window, or a sector
                     * where a finished hack already opened it. Left at that,
                     * the *right* card was the one pickup in the game that made
                     * no sound at all, while a decoy buzzed: the feedback
                     * exactly backwards, in a sector where the strip reads
                     * BLOCKED and cannot report it either. No shipped map has a
                     * card in a window sector today, which is the only reason
                     * nobody has heard it.
                     */
                    bool was_locked = !state->level.runtime.exit_unlocked;
                    gameplay_unlock_exit(state);
                    if (!(was_locked && state->level.runtime.exit_unlocked))
                        game_events_sound(&state->events, SFX_CARD_SCAN);
                }
                else
                    game_events_sound(&state->events, SFX_CARD_WRONG);
                break;
            case ITEM_GUN:
                /* The one pickup allowed to change what is in the hand, and
                 * only out of the knife: a dry clip is the whole reason Chuck
                 * is holding a blade, so the magazine that ends that has to
                 * end it without a button press. Nothing is spent by holding
                 * the sidearm, so this can never cost the player anything —
                 * unlike the explosives below. A player who deliberately
                 * picked the knife while carrying a loaded gun is not in this
                 * case and keeps it. */
                if (state->player.bullets == 0 &&
                    state->player.active_weapon == PLAYER_WEAPON_KNIFE)
                    state->player.active_weapon = PLAYER_WEAPON_PISTOL;
                state->player.bullets = MAX_AMMO;
                game_events_sound(&state->events, SFX_PICKUP_AMMO);
                break;
            case ITEM_GRENADE:
                /* Picking a one-shot explosive up is not deciding to spend it.
                 * Arming it here meant walking over an `N` mid-firefight and
                 * throwing the grenade with the next press of the trigger that
                 * was meant for the pistol — a single pickup silently spending
                 * the scarcest thing in the sector. The HUD shows the grenade
                 * is carried; the bumpers are how it reaches the hand. */
                state->player.grenades = 1;
                game_events_sound(&state->events, SFX_PICKUP_GRENADE);
                break;
            case ITEM_MEDKIT:
                /* Hearts first; a spare life only once the hearts are full,
                 * so the kit is never wasted on either counter. */
                if (state->player.hp < gameplay_player_max_hp(state))
                    state->player.hp = gameplay_player_max_hp(state);
                else if (campaign->lives < MAX_LIVES)
                    campaign->lives++;
                gameplay_bank_checkpoint(state);
                game_events_sound(&state->events, SFX_PICKUP_HEALTH);
                break;
            case ITEM_BAZOOKA:
                /* Same rule as the grenade, and it matters more here: one `Z`
                 * a sector, and the patched walls it was placed for are only
                 * opened by a rocket that was not fired at the first guard
                 * along. */
                state->player.bazooka_rockets = BAZOOKA_AMMO;
                game_events_sound(&state->events, SFX_PICKUP_BAZOOKA);
                break;
            }
        }

        if (item->collected && item->type == ITEM_GUN)
        {
            item->respawn_timer -= dt;
            if (item->respawn_timer <= 0.0f)
            {
                item->collected = false;
                item->respawn_timer = 0.0f;
            }
        }
    }
}

bool gameplay_player_reached_exit(const GameplayState *state)
{
    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    if (state->level.map.has_window)
    {
        return gameplay_boxes_overlap(
            state->player.x, state->player.y, PLAYER_W, height,
            state->level.map.window_col * (float)TILE_SIZE,
            state->level.map.window_row * (float)TILE_SIZE,
            TILE_SIZE, TILE_SIZE);
    }
    if (!state->level.map.has_exit || !state->level.runtime.exit_unlocked)
        return false;
    return gameplay_boxes_overlap(
        state->player.x, state->player.y, PLAYER_W, height,
        state->level.map.exit_col * (float)TILE_SIZE,
        state->level.map.exit_row * (float)TILE_SIZE,
        TILE_SIZE, TILE_SIZE);
}

int gameplay_neutralized_hostiles(const GameplayState *state)
{
    /* The running tally, not a sweep of the `dead` flags. Those flags say who
     * is still standing: a reinforcement takes over a downed guard's slot, so
     * counting them reported one kill fewer for every guard the doors sent
     * after the first — the report between sectors credited the player with
     * less than the floor they had actually cleared. */
    return state->hostiles_neutralized;
}
