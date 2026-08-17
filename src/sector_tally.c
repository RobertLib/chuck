#include "sector_tally.h"

#include "game_config.h"
#include "manual_pages.h"
#include "progress.h"

#include <stdio.h>
#include <string.h>

void sector_tally_clear(SectorTally *tally)
{
    if (tally == NULL)
        return;
    memset(tally, 0, sizeof(*tally));
}

void sector_tally_set(SectorTally *tally, int sector, float elapsed_seconds,
                      float best_seconds, bool best_is_new,
                      int time_bonus, int clean_bonus, int docket_sheets)
{
    if (tally == NULL)
        return;
    tally->pending = true;
    tally->sector = sector;
    tally->elapsed_seconds = elapsed_seconds;
    tally->best_seconds = best_seconds;
    tally->best_is_new = best_is_new;
    tally->time_bonus = time_bonus;
    tally->clean_bonus = clean_bonus;
    tally->docket_sheets = docket_sheets < 0 ? 0 : docket_sheets;
    /* One sheet to an interior and none on a climb, which is
     * `test_every_interior_lays_out_exactly_one_docket_sheet`'s rule read the
     * other way round. Derived rather than written down, because a campaign
     * that gains a floor gains a sheet with it and a literal twelve here would
     * be the third copy of a number the maps already answer. */
    tally->docket_total = CAMPAIGN_SECTORS - CAMPAIGN_CLIMB_SECTOR_COUNT;
    if (tally->docket_sheets > tally->docket_total)
        tally->docket_sheets = tally->docket_total;
}

/*
 * Whole seconds, truncated, and the same clamp the report's own field uses.
 *
 * A stopwatch is printed off an int in both places because
 * `campaign_award_sector_bonus` computes the bonus off an int too — the number
 * on the line and the number in the score have to be arrived at the same way,
 * or the field pays for a second the player was never shown. The ceiling is
 * `PROGRESS_MAX_TIME`'s own, so a session somebody left running overnight
 * prints 99:59 rather than widening the line past what the frame holds.
 */
static void spell_clock(float seconds, char *out, size_t cap)
{
    int total = (int)seconds;
    if (total < 0)
        total = 0;
    if (total > 5999)
        total = 5999;
    snprintf(out, cap, "%02d:%02d", total / 60, total % 60);
}

int sector_tally_format(const SectorTally *tally, char *out, size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;
    out[0] = '\0';
    if (tally == NULL || !tally->pending)
        return 0;

    char run[8];
    spell_clock(tally->elapsed_seconds, run, sizeof(run));

    /*
     * A run that just set the record shows the words rather than the number it
     * beat, which is the rule the report already keeps: printing the old time
     * under BEST on the screen that replaced it is the field disagreeing with
     * itself. A first clear is a record by definition and lands here too.
     *
     * `--:--` is the remaining case and it is a real one rather than a
     * defensive branch: `progress_note_sector_time` refuses anything under
     * `PROGRESS_MIN_TIME` or over `PROGRESS_MAX_TIME`, so a sector cleared in
     * under a second — which is a hand-edited map or an authoring shortcut,
     * not a run — has a time that was not banked and no record to show.
     */
    char best[16];
    if (tally->best_is_new)
    {
        snprintf(best, sizeof(best), "NEW BEST");
    }
    else if (tally->best_seconds > PROGRESS_NO_TIME)
    {
        char clock[8];
        spell_clock(tally->best_seconds, clock, sizeof(clock));
        snprintf(best, sizeof(best), "BEST %s", clock);
    }
    else
    {
        snprintf(best, sizeof(best), "BEST --:--");
    }

    /*
     * A floor that ran over its slot on the night clock is told so rather than
     * left with a gap, for the reason the report says it: nothing there reads
     * as a field that sometimes pays for no reason, and the words are what
     * teach the player that a par exists at all. The clean bonus is the other
     * way round — it is omitted when it was not earned, because a death is
     * already the walk back and `+0 CLEAN` would be the line rubbing it in.
     */
    char time_part[16];
    if (tally->time_bonus > 0)
        snprintf(time_part, sizeof(time_part), "+%d TIME", tally->time_bonus);
    else
        snprintf(time_part, sizeof(time_part), "OVER PAR");

    char clean_part[16];
    if (tally->clean_bonus > 0)
        snprintf(clean_part, sizeof(clean_part), "   +%d CLEAN",
                 tally->clean_bonus);
    else
        clean_part[0] = '\0';

    /*
     * And the run's paper, which is the only cell on the line that is not about
     * the sector that just ended.
     *
     * Printed from the first clear onward rather than once the player has found
     * one, and that is the decision: a cell that appears only after a sheet has
     * been picked up teaches nothing to the player who has walked past all of
     * them, which is precisely the player it is for. `00/12` after sector one
     * is the game saying there is something on these floors to look for, on the
     * screen where there is a moment to read it.
     */
    char docket_part[24];
    snprintf(docket_part, sizeof(docket_part), "   DOCKET %02d/%02d",
             tally->docket_sheets, tally->docket_total);

    /* 1-based, because that is how a sector is named everywhere a human reads
     * one — the strip's SECTOR field, `--level N`, the editor's playtest
     * button and the debug picker. */
    int written = snprintf(out, cap, "SECTOR %02d CLEAR   %s   %s   %s%s%s",
                           tally->sector + 1, run, best, time_part,
                           clean_part, docket_part);
    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return (int)cap - 1;
    return written;
}
