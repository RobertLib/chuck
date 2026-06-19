#include "gameplay_state.h"

#include "game_config.h"

#include <string.h>

/*
 * The docket is a bitmask over the campaign, so the campaign has to fit one.
 *
 * `MAX_ALARM_SWITCHES` is held to the same width one file over for the same
 * reason: past thirty-two the bit is silently never set, which here would mean
 * a sheet that can be taken again on every retry — the exact defect the mask
 * exists to close, returning on the sectors nobody could see it on.
 */
_Static_assert(CAMPAIGN_SECTORS <= 32,
               "CampaignState.docket_sheets_held is a uint32_t; widen it "
               "before the campaign passes thirty-two sectors");

void campaign_reset(CampaignState *campaign, bool veteran)
{
    memset(campaign, 0, sizeof(*campaign));
    campaign->veteran = veteran;
    campaign->lives = veteran ? VETERAN_LIVES : PLAYER_LIVES;
    campaign->continues_remaining =
        veteran ? VETERAN_CONTINUES : PLAYER_CONTINUES;
    campaign->next_extra_life_score = EXTRA_LIFE_SCORE_STEP;
}

bool campaign_holds_docket_sheet(const CampaignState *campaign, int sector)
{
    if (campaign == NULL || sector < 0 || sector >= CAMPAIGN_SECTORS)
        return false;
    return (campaign->docket_sheets_held & (1u << sector)) != 0u;
}

void campaign_take_docket_sheet(CampaignState *campaign, int sector)
{
    if (campaign == NULL || sector < 0 || sector >= CAMPAIGN_SECTORS)
        return;
    /* Idempotent, so a caller that has not asked first cannot double-count.
     * The pickup does ask — `item_would_be_wasted` leaves the sheet on the
     * floor — but the count and the set must not be able to disagree even
     * from a call site that forgets. */
    if (campaign_holds_docket_sheet(campaign, sector))
        return;
    campaign->docket_sheets_held |= 1u << sector;
    campaign->evidence_collected++;
}

int campaign_time_bonus_for(float elapsed_seconds)
{
    /* Whole seconds, truncated, and the same arithmetic the report prints: the
     * field shows `elapsed / 60` and `elapsed % 60` off an int, and a bonus
     * computed off the float would pay for a second the player was never
     * shown. */
    int elapsed = (int)elapsed_seconds;
    int par = (int)SECTOR_PAR_SECONDS;
    int spare = par - elapsed;
    if (spare < 0)
        spare = 0;
    return spare * SECTOR_TIME_BONUS_PER_SECOND;
}

void campaign_note_veteran(CampaignState *campaign, bool veteran)
{
    if (campaign == NULL)
        return;
    campaign->veteran = veteran;
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
    /* The run's own number rather than the default one. A veteran run has no
     * continues to spend, so this is the branch its every death takes, and
     * handing it `PLAYER_LIVES` made the mode expire on first contact. */
    campaign->lives = campaign->veteran ? VETERAN_LIVES : PLAYER_LIVES;
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
    {
        /*
         * And the milestones the score has *already* passed go with it, which
         * the paragraph above claimed and the caller undid.
         *
         * Every call site is `while (campaign_check_extra_life(...))`, so the
         * drain stops on the first `false` — and returning here left the
         * threshold one step behind a score that may be several ahead. Those
         * steps were banked after all: at nine lives with a score five
         * milestones past the mark, the next death dropped the count to eight
         * and the next point scored handed a life straight back, once per
         * banked step. Measured before this loop existed, a run at the cap
         * that had passed four spare milestones got two of them back over its
         * next two deaths.
         *
         * The cap is meant to *cost* the milestones it swallows, which is what
         * makes nine lives a ceiling rather than a buffer. Burning them here
         * rather than at the call site keeps that a property of the rule and
         * not of how a caller happens to loop.
         */
        while (campaign->score >= campaign->next_extra_life_score)
            campaign->next_extra_life_score += EXTRA_LIFE_SCORE_STEP;
        return false;
    }
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

    /* Through `campaign_time_bonus_for`, which is the only place the clock is
     * priced. It was inline here, and the three staged screens in game.c each
     * carried a figure somebody had worked out by hand — one of which was the
     * bonus for a different clock than the one printed beside it. */
    int time_bonus = campaign_time_bonus_for(campaign->level_elapsed_time);
    /* A death is what costs this, not a hit: the hearts are the sector's own
     * currency and spending all three is already the walk back. */
    int clean_bonus = campaign->level_deaths == 0 ? SECTOR_CLEAN_BONUS : 0;

    campaign->score += time_bonus + clean_bonus;

    if (out_time_bonus != NULL)
        *out_time_bonus = time_bonus;
    if (out_clean_bonus != NULL)
        *out_clean_bonus = clean_bonus;
}

void gameplay_carry_through_doorway(GameplayState *arriving,
                                    const Player *travelling,
                                    float travelling_invuln)
{
    /* No NULL guard, to match `player_carry_loadout` — which this function's
     * first line is — rather than the campaign accessors above it: both callers
     * are `&game->gameplay`, and a guard nothing can reach is a line `make
     * coverage` reports for ever. */
    player_carry_loadout(&arriving->player, travelling);
    /* The door is not a heal. */
    arriving->player.hp = travelling->hp;
    arriving->invuln_timer = travelling_invuln;
    /* And the door he has just come through must not read as under his feet on
     * the frame he arrives, in either direction. */
    arriving->teleport_cooldown = TELEPORT_COOLDOWN;
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
