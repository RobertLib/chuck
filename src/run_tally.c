#include "run_tally.h"

#include "game_config.h"
#include "manual_pages.h"
#include "progress.h"

#include <stdio.h>

/* The RECORDS page walks the campaign's floors through an array sized for more
 * of them. That is the arrangement `PROGRESS_MAX_TRACKED_SECTORS` exists for,
 * and this is what keeps the walk inside the array if the two ever cross. */
_Static_assert(CAMPAIGN_SECTORS <= PROGRESS_MAX_TRACKED_SECTORS,
               "the campaign has more sectors than Progress keeps times for");

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
    else if (best_score <= 0)
    {
        /*
         * No record to stand the run beside, so none is quoted — the same rule
         * the docket line below keeps, and the same rule `run_tally_format_record`
         * keeps when it spells a nought `--`: `progress_note_score` only ever
         * raises the figure, so nought is "no run has finished" rather than "a
         * run finished on nothing".
         *
         * This used to fall through to the clause below and print
         * `SCORE 0 - BEST 0` at the first player to die before scoring — three
         * words of comparison against a record that does not exist, on the card
         * that is their first sight of the scoreboard, while the RECORDS page
         * two screens away described the identical state as `--`. Two answers
         * to one question, in one file, and the file is the one that exists so
         * a record reads the same wherever it is read.
         *
         * `best_score` is banked before this card is drawn
         * (`game_record_run_score` at `STATE_GAME_OVER`), so on an unassisted
         * run it can only be nought when the score is too — this is the
         * nothing-yet case and no other.
         */
        written = snprintf(out, cap, "SCORE %d", score);
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

/*
 * The headline figures, and the labels over them.
 *
 * One list rather than a switch per question, for the reason
 * `CHUCK_BIND_LIST` is one list: the enum, the words and the formatting are
 * three things that have to agree per row, and three switches over the same
 * enum is three places to forget a row. The `_Static_assert` measures the
 * initializer, so it is written `[]` — see the note in
 * [../AGENTS.md](../AGENTS.md) on why a sized array makes that check
 * tautological.
 */
/*
 * **And the third of these said FLOOR over a figure that reads SECTOR.**
 *
 * `run_tally_format_record` prints `SECTOR %02d` and the row's own detail line
 * on the RECORDS page says "THE HIGHEST SECTOR ANY RUN HAS REACHED", so the
 * label was the one word out of three that disagreed. It is not a synonym in
 * this game: `BUILDING_FLOORS` is forty and `CAMPAIGN_SECTORS` is seventeen, and
 * AGENTS.md keeps those two constants deliberately underived from each other
 * precisely because a sector is a stretch of the climb rather than a storey.
 * A row reading "FURTHEST FLOOR — SECTOR 09" quotes the wrong unit at the
 * player on the one screen whose whole subject is what the game remembers.
 */
static const char *const RECORD_LABELS[] = {
    "BEST SCORE",
    "DOCKET",
    "FURTHEST SECTOR",
    "SECTORS TIMED",
};
_Static_assert(sizeof(RECORD_LABELS) / sizeof(RECORD_LABELS[0]) ==
                   (size_t)RUN_TALLY_RECORD_COUNT,
               "one label per record figure");

const char *run_tally_record_label(RunTallyRecord which)
{
    if (which < 0 || which >= RUN_TALLY_RECORD_COUNT)
        return "";
    return RECORD_LABELS[which];
}

int run_tally_format_record_value(RunTallyRecord which, int value, char *out,
                                  size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;
    out[0] = '\0';
    if (which < 0 || which >= RUN_TALLY_RECORD_COUNT)
        return 0;

    int written = 0;
    switch (which)
    {
    case RUN_TALLY_RECORD_SCORE:
        /* Nought is "no run has finished" rather than "a run finished on
         * nothing": `progress_note_score` only ever raises this, and a fresh
         * file is nought, so the two are the same state and `--` is the honest
         * spelling of it. The same argument `PROGRESS_NO_TIME` is built on. */
        written = value > 0 ? snprintf(out, cap, "%d",
                                       clamp_int(value, 0, PROGRESS_MAX_SCORE))
                            : snprintf(out, cap, "--");
        break;
    case RUN_TALLY_RECORD_DOCKET:
        /*
         * Clamped to the campaign's own docket rather than to
         * `PROGRESS_MAX_EVIDENCE`, and that is the same correction
         * `RUN_TALLY_RECORD_SECTORS_TIMED` three cases down already carries:
         * the numerator and the denominator have to be asked the same
         * question. The file's ceiling is ninety-nine on purpose — the slack
         * is what lets a longer campaign ship without a new format — so this
         * row printed `13 / 12`, a fraction over its own whole, on the two
         * screens whose entire subject is what the game remembers.
         *
         * It was reachable by playing rather than only by a hand-edited file.
         * The retry after a continue reloads the map, so the `*` was laid out
         * again while `CampaignState.evidence_collected` carried on: five
         * retries of sector one banked five sheets off a floor that has one.
         * `campaign_take_docket_sheet` is what stops the count now; this stops
         * the *sentence*, which a file written by another build or by anybody
         * with a text editor can still reach.
         */
        written = value > 0
                      ? snprintf(out, cap, "%d / %d",
                                 clamp_int(value, 0, campaign_docket_sheets()),
                                 campaign_docket_sheets())
                      : snprintf(out, cap, "--");
        break;
    case RUN_TALLY_RECORD_FURTHEST:
        /* 1-based, because that is how a sector is named everywhere a human
         * reads one. Nought is the lobby and nothing past it, which is also a
         * fresh file — so it is the same `--` as the two above rather than
         * `SECTOR 01`, which would claim a floor nobody had reached. */
        written = value > 0 ? snprintf(out, cap, "SECTOR %02d", value + 1)
                            : snprintf(out, cap, "--");
        break;
    case RUN_TALLY_RECORD_SECTORS_TIMED:
        written = value > 0
                      ? snprintf(out, cap, "%d / %d", value, CAMPAIGN_SECTORS)
                      : snprintf(out, cap, "--");
        break;
    case RUN_TALLY_RECORD_COUNT:
        break;
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

/*
 * The stored figure behind one row, which is the only part of this that needs
 * the player's file at all.
 */
static int record_figure(RunTallyRecord which, const Progress *progress)
{
    switch (which)
    {
    case RUN_TALLY_RECORD_SCORE:
        return progress->best_score;
    case RUN_TALLY_RECORD_DOCKET:
        return progress->best_evidence;
    case RUN_TALLY_RECORD_FURTHEST:
        return progress->furthest_sector;
    case RUN_TALLY_RECORD_SECTORS_TIMED:
    {
        /*
         * How many sectors have a time on them at all, which is the one figure
         * here that is not a stored number: the file keeps a time per sector and
         * what a player wants off this page is how much of the ladder they are
         * on.
         *
         * Counted over the campaign rather than over the array, and the
         * difference is the whole of the bug this replaces. `Progress`
         * deliberately keeps `PROGRESS_MAX_TRACKED_SECTORS` slots against
         * `CAMPAIGN_SECTORS` floors — see the note on that constant: the slack is
         * there so a longer campaign does not need a new file format, and
         * `progress_parse` accepts every index inside it. So a file written by a
         * build with more floors, or by anybody with a text editor, put times
         * above the sixteenth index and this loop counted them: the RECORDS page
         * printed `20 / 17`, a fraction over its own whole. The numerator and the
         * denominator have to be asked the same question, and the question is
         * the campaign that is running.
         */
        int timed = 0;
        for (int i = 0; i < CAMPAIGN_SECTORS; ++i)
            if (progress->best_sector_time[i] > PROGRESS_NO_TIME)
                ++timed;
        return timed;
    }
    case RUN_TALLY_RECORD_COUNT:
        break;
    }
    return 0;
}

int run_tally_format_record(RunTallyRecord which, const Progress *progress,
                            char *out, size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;
    out[0] = '\0';
    if (progress == NULL || which < 0 || which >= RUN_TALLY_RECORD_COUNT)
        return 0;
    return run_tally_format_record_value(which, record_figure(which, progress),
                                         out, cap);
}

int run_tally_format_record_line(RunTallyRecord which, int value, char *out,
                                 size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;
    out[0] = '\0';
    if (which < 0 || which >= RUN_TALLY_RECORD_COUNT)
        return 0;

    char cell[RUN_TALLY_RECORD_MAX];
    if (run_tally_format_record_value(which, value, cell, sizeof(cell)) <= 0)
        return 0;

    int written =
        snprintf(out, cap, "%s %s", run_tally_record_label(which), cell);
    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return (int)cap - 1;
    return written;
}
