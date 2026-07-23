#include "gameplay_world.h"

#include <math.h>

bool gameplay_boxes_overlap(float ax, float ay, float aw, float ah,
                            float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

void gameplay_world_sound(GameplayState *state, SoundEffect effect,
                          float x, float y)
{
    game_events_world_sound(&state->events, effect, x, y);
}

bool gameplay_alarm_active(const GameplayState *state)
{
    return state->terminal_alarm_timer > 0.0f;
}

void gameplay_trigger_alarm(GameplayState *state, float source_x,
                            float source_y, int switch_index)
{
    bool was_active = gameplay_alarm_active(state);
    state->terminal_alarm_timer = ALARM_CALM_TIME;
    state->alarm_target_x = source_x;
    state->alarm_target_y = source_y;
    state->active_alarm_switch = switch_index;
    state->alarm_siren_timer = ALARM_SIREN_INTERVAL;
    if (!was_active)
    {
        if (switch_index >= 0)
            gameplay_world_sound(state, SFX_CARD_SCAN, source_x, source_y);
        /* A building alarm is ambient infrastructure, not a sound emitted
         * only at the call point. Keep the siren audible everywhere. */
        game_events_sound(&state->events, SFX_TERMINAL_ALARM);
        for (int i = 0; i < state->enemy_count; ++i)
        {
            Enemy *enemy = &state->enemies[i];
            if (enemy->dead)
                continue;
            if (enemy->shoot_cooldown > ENEMY_ALARM_INITIAL_SHOT_DELAY)
                enemy->shoot_cooldown = ENEMY_ALARM_INITIAL_SHOT_DELAY;
            float alarm_aim_time =
                ENEMY_AIM_TIME * ENEMY_ALARM_AIM_MULTIPLIER;
            if (enemy->aim_timer > alarm_aim_time)
                enemy->aim_timer = alarm_aim_time;
        }
    }
}

void gameplay_refresh_alarm_from_player(GameplayState *state)
{
    if (!gameplay_alarm_active(state))
        return;
    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    state->terminal_alarm_timer = ALARM_CALM_TIME;
    state->alarm_target_x = state->player.x + PLAYER_W * 0.5f;
    state->alarm_target_y = state->player.y + height * 0.5f;
}

void gameplay_update_alarm(GameplayState *state, float dt)
{
    if (!gameplay_alarm_active(state))
        return;

    state->terminal_alarm_timer -= dt;
    if (state->terminal_alarm_timer <= 0.0f)
    {
        state->terminal_alarm_timer = 0.0f;
        state->alarm_siren_timer = 0.0f;
        state->active_alarm_switch = -1;
        state->terminal_reinforcement_timer = 0.0f;
        state->terminal_reinforcements_pending = 0;
        return;
    }

    state->alarm_siren_timer -= dt;
    if (state->alarm_siren_timer <= 0.0f)
    {
        game_events_sound(&state->events, SFX_TERMINAL_ALARM);
        state->alarm_siren_timer += ALARM_SIREN_INTERVAL;
    }
}

void gameplay_hit_player(GameplayState *state)
{
    if (state->player.dying)
        return;

    state->terminal_hack_progress = 0.0f;
    state->terminal_hack_tick_timer = 0.0f;
    state->terminal_in_range = false;
    state->terminal_hacking = false;
    state->terminal_alarm_timer = 0.0f;
    state->alarm_siren_timer = 0.0f;
    state->active_alarm_switch = -1;
    state->terminal_reinforcement_timer = 0.0f;
    state->terminal_reinforcements_pending = 0;

    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    float x = state->player.x + PLAYER_W * 0.5f;
    float y = state->player.y + height * 0.5f;
    game_events_particles(&state->events, x, y, 32, state->player.facing);
    game_events_sound(&state->events, SFX_PLAYER_HIT);
    state->player.dying = true;
    state->player.death_timer = 0.75f;
}

void gameplay_unlock_exit(GameplayState *state)
{
    if (state->level.runtime.exit_unlocked)
        return;
    state->level.runtime.exit_unlocked = true;
    state->terminal_in_range = false;
    state->terminal_hacking = false;
    game_events_sound(&state->events, SFX_EXIT_UNLOCKED);
}

bool gameplay_player_near_active_terminal(const GameplayState *state)
{
    int index = state->level.runtime.active_terminal_index;
    if (state->level.runtime.exit_unlocked ||
        index < 0 || index >= state->level.map.terminal_count)
    {
        return false;
    }

    const Terminal *terminal = &state->level.map.terminals[index];
    float terminal_x = terminal->col * (float)TILE_SIZE + TILE_SIZE * 0.5f;
    float terminal_y = terminal->row * (float)TILE_SIZE + TILE_SIZE * 0.5f;
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    float player_x = state->player.x + PLAYER_W * 0.5f;
    float player_y = state->player.y + player_h * 0.5f;
    return fabsf(player_x - terminal_x) <= TERMINAL_INTERACT_RANGE &&
           fabsf(player_y - terminal_y) <= TILE_SIZE * 0.65f;
}

void gameplay_provoke_enemy(GameplayState *state, int enemy_index)
{
    if (enemy_index < 0 || enemy_index >= state->enemy_count)
        return;

    Enemy *attacked = &state->enemies[enemy_index];
    int participant_count = attacked->talking ? 2 : 1;
    int participants[2] = {enemy_index, attacked->talk_partner};
    float target_x = state->player.x + PLAYER_W * 0.5f;
    float target_y = state->player.y +
                     (state->player.crawling
                          ? (float)PLAYER_CRAWL_H * 0.45f
                          : (float)PLAYER_H * 0.15f);
    Enemy *alert_source = NULL;

    for (int i = 0; i < participant_count; ++i)
    {
        int index = participants[i];
        if (index < 0 || index >= state->enemy_count ||
            (i > 0 && index == participants[0]))
            continue;

        Enemy *enemy = &state->enemies[index];
        enemy->raising_alarm = false;
        enemy->alarm_switch_index = -1;
        enemy->alarm_use_timer = 0.0f;
        enemy->encounter_decided = true;
        enemy->encounter_lost_timer = GUARD_ENCOUNTER_RESET_TIME;
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
        enemy->aim_timer = ENEMY_AIM_TIME *
                           (gameplay_alarm_active(state)
                                ? ENEMY_ALARM_AIM_MULTIPLIER
                                : 1.0f);
        if (alert_source == NULL)
            alert_source = enemy;
    }

    if (alert_source != NULL)
    {
        gameplay_world_sound(state, SFX_ENEMY_ALERT,
                             alert_source->x + ENEMY_W * 0.5f,
                             alert_source->y + ENEMY_H * 0.5f);
    }
}

void gameplay_destroy_crate(GameplayState *state, CampaignState *campaign,
                            Crate *crate)
{
    if (!crate->active)
        return;
    crate->active = false;
    crate->vx = 0.0f;
    crate->vy = 0.0f;
    float x = crate->x + CRATE_W * 0.5f;
    float y = crate->y + CRATE_H * 0.5f;
    game_events_explosion(&state->events, x, y, 18);
    gameplay_world_sound(state, SFX_CRATE_BREAK, x, y);
    campaign->score += 20;
}

void gameplay_kill_enemy_with_crate(GameplayState *state,
                                    CampaignState *campaign, Enemy *enemy)
{
    if (enemy->dead)
        return;
    enemy->hp = 0;
    enemy->dead = true;
    float x = enemy->x + ENEMY_W * 0.5f;
    float y = enemy->y + ENEMY_H * 0.5f;
    game_events_particles(&state->events, x, y, 24, enemy->dir);
    gameplay_world_sound(state, SFX_ENEMY_DOWN, x, y);
    campaign->score += 150;
}

void gameplay_kill_dog_with_crate(GameplayState *state,
                                  CampaignState *campaign, Dog *dog)
{
    if (dog->dead)
        return;
    dog->hp = 0;
    dog->dead = true;
    float x = dog->x + DOG_W * 0.5f;
    float y = dog->y + DOG_H * 0.5f;
    game_events_particles(&state->events, x, y, 14, dog->dir);
    gameplay_world_sound(state, SFX_DOG_YELP, x, y);
    campaign->score += 75;
}
