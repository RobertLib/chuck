#include "gameplay_interaction.h"

#include "gameplay_world.h"

#include <math.h>

static float reinforcement_delay(Rng *rng, float minimum, float maximum)
{
    return minimum + (maximum - minimum) * rng_unit(rng);
}

void gameplay_prepare_terminal(GameplayState *state, const Input *input,
                               float dt)
{
    state->terminal_in_range = gameplay_player_near_active_terminal(state);
    state->terminal_hacking = state->terminal_in_range &&
                              input->interact &&
                              state->player.on_ground &&
                              !state->player.on_ladder;
    bool alarm_started = state->terminal_hacking &&
                         !gameplay_alarm_active(state);
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
    }
    else
    {
        state->terminal_hack_progress = 0.0f;
        state->terminal_hack_tick_timer = 0.0f;
    }

    if (state->terminal_hacking)
    {
        const Terminal *terminal =
            &state->level.map.terminals[state->level.runtime.active_terminal_index];
        state->terminal_alarm_timer = ALARM_CALM_TIME;
        state->alarm_target_x = (terminal->col + 0.5f) * TILE_SIZE;
        state->alarm_target_y = (terminal->row + 0.5f) * TILE_SIZE;
        if (alarm_started && state->level.map.door_count > 0)
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
        game_events_sound(&state->events, SFX_DOOR);
    }
    input->use_door = false;
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
            gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, player_h,
                                   item->x - 8.0f, item->y - 8.0f,
                                   16.0f, 16.0f))
        {
            item->collected = true;
            item->respawn_timer =
                item->type == ITEM_GUN || item->type == ITEM_GRENADE
                    ? ITEM_RESPAWN_TIME
                    : 0.0f;
            switch (item->type)
            {
            case ITEM_CARD:
                campaign->score += 100;
                if (i == state->level.runtime.active_card_index)
                {
                    state->level.runtime.items_remaining = 0;
                    gameplay_unlock_exit(state);
                }
                else
                    game_events_sound(&state->events, SFX_CARD_WRONG);
                break;
            case ITEM_GUN:
                state->player.bullets = MAX_AMMO;
                game_events_sound(&state->events, SFX_PICKUP_AMMO);
                break;
            case ITEM_GRENADE:
                state->player.grenades = 1;
                game_events_sound(&state->events, SFX_PICKUP_GRENADE);
                break;
            case ITEM_MEDKIT:
                if (campaign->lives < MAX_LIVES)
                    campaign->lives++;
                game_events_sound(&state->events, SFX_PICKUP_HEALTH);
                break;
            case ITEM_BAZOOKA:
                state->player.bazooka_rockets = BAZOOKA_AMMO;
                game_events_sound(&state->events, SFX_PICKUP_BAZOOKA);
                break;
            }
        }

        if (item->collected &&
            (item->type == ITEM_GUN || item->type == ITEM_GRENADE))
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
    if (!state->level.map.has_exit || !state->level.runtime.exit_unlocked)
        return false;
    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    return gameplay_boxes_overlap(
        state->player.x, state->player.y, PLAYER_W, height,
        state->level.map.exit_col * (float)TILE_SIZE,
        state->level.map.exit_row * (float)TILE_SIZE,
        TILE_SIZE, TILE_SIZE);
}

int gameplay_neutralized_hostiles(const GameplayState *state)
{
    int count = 0;
    for (int i = 0; i < state->enemy_count; ++i)
        if (state->enemies[i].dead)
            count++;
    for (int i = 0; i < state->dog_count; ++i)
        if (state->dogs[i].dead)
            count++;
    return count;
}
