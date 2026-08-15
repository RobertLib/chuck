#include "gameplay_state.h"

#include <string.h>

void campaign_reset(CampaignState *campaign)
{
    memset(campaign, 0, sizeof(*campaign));
    campaign->lives = PLAYER_LIVES;
    campaign->continues_remaining = PLAYER_CONTINUES;
    campaign->next_extra_life_score = EXTRA_LIFE_SCORE_STEP;
}

bool campaign_lose_life(CampaignState *campaign)
{
    if (campaign->lives > 0)
        campaign->lives--;
    return campaign->lives <= 0;
}

bool campaign_begin_continue(CampaignState *campaign)
{
    campaign->continue_timer = 0.0f;
    if (campaign->lives > 0)
        return false;

    /* Retrying the sector is always on offer. Continues are no longer the
     * countdown to losing the campaign — they are the score insurance: while
     * one is left the score survives the retry, after that the retry costs
     * it. Nobody is ever sent back to level one against their will. */
    campaign->continue_timer = CONTINUE_COUNTDOWN_TIME;
    return true;
}

bool campaign_update_continue(CampaignState *campaign, float dt)
{
    if (campaign->continue_timer <= 0.0f)
        return true;

    campaign->continue_timer -= dt;
    if (campaign->continue_timer <= 0.0f)
    {
        campaign->continue_timer = 0.0f;
        return true;
    }
    return false;
}

bool campaign_accept_continue(CampaignState *campaign)
{
    if (campaign->lives > 0 || campaign->continue_timer <= 0.0f)
        return false;

    if (campaign->continues_remaining > 0)
    {
        campaign->continues_remaining--;
    }
    else
    {
        campaign->score = 0;
        campaign->level_start_score = 0;
        campaign->next_extra_life_score = EXTRA_LIFE_SCORE_STEP;
    }
    campaign->lives = PLAYER_LIVES;
    campaign->continue_timer = 0.0f;
    return true;
}

bool campaign_check_extra_life(CampaignState *campaign)
{
    if (campaign->next_extra_life_score <= 0 ||
        campaign->score < campaign->next_extra_life_score)
        return false;

    campaign->next_extra_life_score += EXTRA_LIFE_SCORE_STEP;
    if (campaign->lives < MAX_LIVES)
        campaign->lives++;
    return true;
}

void gameplay_state_begin_level(GameplayState *state)
{
    Rng rng = state->rng;
    memset(state, 0, sizeof(*state));
    state->rng = rng;
    state->player_on_elevator = -1;
    state->player_on_moving_platform = -1;
    state->active_alarm_switch = -1;
}

int gameplay_player_max_hp(const GameplayState *state)
{
    return state->assist_more_hearts ? PLAYER_ASSIST_MAX_HP : PLAYER_MAX_HP;
}

float gameplay_enemy_speed_scale(const GameplayState *state)
{
    return state->assist_slow_enemies ? ASSIST_ENEMY_SPEED : 1.0f;
}
