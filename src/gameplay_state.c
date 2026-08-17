#include "gameplay_state.h"

#include <string.h>

void campaign_reset(CampaignState *campaign, bool veteran)
{
    memset(campaign, 0, sizeof(*campaign));
    campaign->lives = veteran ? VETERAN_LIVES : PLAYER_LIVES;
    campaign->continues_remaining =
        veteran ? VETERAN_CONTINUES : PLAYER_CONTINUES;
    campaign->next_extra_life_score = EXTRA_LIFE_SCORE_STEP;
}

void campaign_note_assist(CampaignState *campaign, bool assist_on)
{
    if (campaign == NULL || !assist_on)
        return;
    campaign->assisted = true;
}

bool campaign_records_count(const CampaignState *campaign)
{
    return campaign != NULL && !campaign->assisted;
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

    /* The threshold moves whether or not there was a life to give, so a run
     * sitting on the cap does not bank every milestone it passes and cash them
     * all in on the first death. */
    campaign->next_extra_life_score += EXTRA_LIFE_SCORE_STEP;
    /* But the caller is told only about a life it actually got: the return
     * value is what flashes 1UP on the strip and plays the jingle, and a
     * counter already at MAX_LIVES announcing a life it did not gain is the
     * HUD miscounting out loud. */
    if (campaign->lives >= MAX_LIVES)
        return false;
    campaign->lives++;
    return true;
}

void campaign_begin_sector(CampaignState *campaign)
{
    campaign->level_elapsed_time = 0.0f;
    campaign->level_start_score = campaign->score;
    campaign->level_deaths = 0;
    campaign->sector_bonus_paid = false;
}

void campaign_award_sector_bonus(CampaignState *campaign,
                                 int *out_time_bonus, int *out_clean_bonus)
{
    if (out_time_bonus != NULL)
        *out_time_bonus = 0;
    if (out_clean_bonus != NULL)
        *out_clean_bonus = 0;
    /* Once a sector, however many times the way out is asked about — see the
     * note on the latch. */
    if (campaign->sector_bonus_paid)
        return;
    campaign->sector_bonus_paid = true;

    /* Whole seconds under par, floored, so the number on the report and the
     * number in the score are arrived at the same way: the field prints
     * `elapsed / 60` and `elapsed % 60` off an int, and a bonus computed off
     * the float would pay for a second the player was never shown. */
    int elapsed = (int)campaign->level_elapsed_time;
    int par = (int)SECTOR_PAR_SECONDS;
    int spare = par - elapsed;
    if (spare < 0)
        spare = 0;

    int time_bonus = spare * SECTOR_TIME_BONUS_PER_SECOND;
    /* A death is what costs this, not a hit: the hearts are the sector's own
     * currency and spending all three is already the walk back. */
    int clean_bonus = campaign->level_deaths == 0 ? SECTOR_CLEAN_BONUS : 0;

    campaign->score += time_bonus + clean_bonus;

    if (out_time_bonus != NULL)
        *out_time_bonus = time_bonus;
    if (out_clean_bonus != NULL)
        *out_clean_bonus = clean_bonus;
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
    /* Assist wins where both are set, and that is the right way round: one of
     * the two switches is somebody asking the game to be easier and the other
     * is somebody asking for a harder run they have not had to turn off. A
     * player who has both on is a player who wants the help. */
    if (state->assist_slow_enemies)
        return ASSIST_ENEMY_SPEED;
    return state->veteran ? VETERAN_ENEMY_SPEED : 1.0f;
}
