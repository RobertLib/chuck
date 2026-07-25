#include "gameplay_state.h"

#include <string.h>

void campaign_reset(CampaignState *campaign)
{
    memset(campaign, 0, sizeof(*campaign));
    campaign->lives = PLAYER_LIVES;
    campaign->continues_remaining = PLAYER_CONTINUES;
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
    if (campaign->lives > 0 || campaign->continues_remaining <= 0)
        return false;

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
    if (campaign->lives > 0 || campaign->continues_remaining <= 0 ||
        campaign->continue_timer <= 0.0f)
    {
        return false;
    }

    campaign->continues_remaining--;
    campaign->lives = PLAYER_LIVES;
    campaign->continue_timer = 0.0f;
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
