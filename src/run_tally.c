#include "run_tally.h"

#include "progress.h"

#include <stdio.h>

/*
 * Both figures are clamped to what `Progress` will actually store.
 *
 * Not because either is reachable — a full campaign at par pays tens of
 * thousands, four orders under the ceiling — but because the line's width is
 * measured against these two numbers and nothing else guarantees the run's own
 * counters stay inside them. `CampaignState.score` is a plain int the whole
 * campaign adds to; a clamp here is what makes the fit check a proof rather than
 * an estimate, and it can never fire in a game anybody plays.
 */
static int clamp_int(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

int run_tally_format_score(int score, int best_score, bool assisted,
                           char *out, size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;
    out[0] = '\0';

    score = clamp_int(score, 0, PROGRESS_MAX_SCORE);
    best_score = clamp_int(best_score, 0, PROGRESS_MAX_SCORE);

    int written;
    if (assisted)
    {
        /* No record was banked, so no record is quoted. The words are the
         * player being told why the number beside them did not move. */
        written = snprintf(out, cap, "SCORE %d - ASSIST, NOT RECORDED", score);
    }
    else if (score >= best_score && score > 0)
    {
        /* When the two are the same number the run that just ended is the best
         * one, and the line says that outright instead of printing the same
         * figure twice. */
        written = snprintf(out, cap, "SCORE %d - YOUR BEST YET", score);
    }
    else
    {
        written = snprintf(out, cap, "SCORE %d - BEST %d", score, best_score);
    }

    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return (int)cap - 1;
    return written;
}

int run_tally_format_docket(int sheets, int best_sheets, bool assisted,
                            char *out, size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;
    out[0] = '\0';

    sheets = clamp_int(sheets, 0, PROGRESS_MAX_EVIDENCE);
    best_sheets = clamp_int(best_sheets, 0, PROGRESS_MAX_EVIDENCE);

    /* Nothing tonight and nothing ever: the docket has not entered this
     * player's game yet and a line about it would be a line about nothing. An
     * assisted run is asked the same question about its own sheets only —
     * `best_sheets` is a ladder it is not on. */
    if (sheets <= 0 && (assisted || best_sheets <= 0))
        return 0;

    int written;
    if (assisted)
    {
        /* The assist is stated once, on the score line above this one. Repeating
         * it here would be the card explaining itself twice on two lines the
         * player reads as one block. */
        written = snprintf(out, cap, "DOCKET %d SHEETS", sheets);
    }
    else if (sheets >= best_sheets && sheets > 0)
    {
        written = snprintf(out, cap, "DOCKET %d SHEETS - YOUR BEST YET",
                           sheets);
    }
    else
    {
        written = snprintf(out, cap, "DOCKET %d SHEETS - BEST %d", sheets,
                           best_sheets);
    }

    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return (int)cap - 1;
    return written;
}

int run_tally_format_sector_time(int sector_index, float seconds, char *out,
                                 size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;
    out[0] = '\0';
    if (sector_index < 0)
        return 0;

    /* 1-based, because that is how a sector is named everywhere a human reads
     * one — the strip's SECTOR field, the report's own heading, `--level N` and
     * the editor's playtest button. */
    char clock[8];
    if (seconds > PROGRESS_NO_TIME)
    {
        int total = (int)seconds;
        if (total < 0)
            total = 0;
        if (total > 5999)
            total = 5999;
        snprintf(clock, sizeof(clock), "%02d:%02d", total / 60, total % 60);
    }
    else
    {
        /* Nobody has finished it. Not a blank, for the reason
         * `PROGRESS_NO_TIME` is not nought: an absent record and a perfect one
         * must not look the same on the sheet. */
        snprintf(clock, sizeof(clock), "--:--");
    }

    int written = snprintf(out, cap, "%02d  %s", sector_index + 1, clock);
    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return (int)cap - 1;
    return written;
}
