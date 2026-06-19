#ifndef CHUCK_SECTOR_TALLY_H
#define CHUCK_SECTOR_TALLY_H

#include <stdbool.h>
#include <stddef.h>

/*
 * What a sector paid, for the eleven clears that never reach a report.
 *
 * The report between sectors ([cutscene.c](cutscene.c)) carries the stopwatch,
 * the record beside it, the two bonuses and the tally of what went down — and
 * it is shown only after a sector that leaves by its **stair door**, because a
 * window is a continuous physical route onto the facade and cutting to an ops
 * screen would contradict what is on the display. That reasoning is sound and
 * it is written up in [intel.h](intel.h). What went with it was never argued
 * for at all: on the ten sectors that leave by a window, and on the last
 * sector of the campaign, the game went on **awarding** the time bonus and the
 * clean bonus and went on **banking** a per-sector record, and said nothing
 * about any of it.
 *
 * That is not a smaller version of the same screen, it is a different defect.
 * `progress_note_sector_time` writes a record for all seventeen sectors and
 * `progress_sector_time` had exactly one caller — the line that fills the
 * report in — so eleven of those records were written to the player's disk,
 * kept across sessions, and unreachable by any screen in the game. The score
 * meanwhile jumped by up to `SECTOR_PAR_SECONDS * SECTOR_TIME_BONUS_PER_SECOND`
 * plus `SECTOR_CLEAN_BONUS` with nothing to connect it to, which is the exact
 * thing the report's own comment says a bonus must never do: "a score that
 * sometimes pays for no visible reason teaches nothing."
 *
 * So this is the one line the report would have said, carried to the next
 * screen the player is already looking at — the reveal of the sector above, or
 * the card that ends the campaign. It is a line and not a panel on purpose:
 * the objection to a report after a window is to the *cut*, and a line over a
 * reveal cuts away from nothing.
 *
 * It is a table of words the player reads, so it lives here rather than inside
 * a renderer, with the frame it has to fit written as constants the renderer
 * lays it out from and `test_the_sector_tally_fits_the_frame_it_is_drawn_in`
 * measuring the widest line any run can produce. Every time that rule has been
 * skipped the result has been the same: a line nobody can see, in the part of
 * the game that exists to be read.
 */

/*
 * Where the line is set, and what stops it.
 *
 * The frame is 800x552 (`SDL_SetRenderLogicalPresentation` in game.c) and the
 * type is `SDL_RenderDebugText`'s 8x8 bitmap at scale 1.0, the only size it is
 * sharp at. The line is centred, so what bounds it is the narrower of the two
 * margins doubled; both are written out rather than one and a width, because
 * the renderer positions from the left edge and a reader checking the drawing
 * against this header should not have to do the arithmetic twice.
 */
#define SECTOR_TALLY_TEXT_LEFT 40.0f
#define SECTOR_TALLY_TEXT_RIGHT 760.0f
#define SECTOR_TALLY_GLYPH_W 8.0f

/*
 * How far above the numbers the story line sits, and it is a second line rather
 * than more words on the first because the two are different voices: the
 * numbers are the scoreboard, and the line above them is Chuck. The report
 * between sectors already stacks them that way round, which is the layout this
 * is carrying over rather than inventing.
 */
#define SECTOR_TALLY_INTEL_RISE 11.0f

/* Room for the widest line plus its terminator. The suite holds the two
 * together, so this cannot quietly become the thing that truncates. */
#define SECTOR_TALLY_MAX 96

typedef struct
{
    /* False except in the window between a sector being cleared without a
     * report and the next screen having drawn the line. Nothing else in the
     * struct is meaningful while this is false. */
    bool pending;
    /* The sector just cleared, 0-based, the way the campaign counts them. */
    int sector;
    float elapsed_seconds;
    /* The record this run was measured against — `PROGRESS_NO_TIME` when
     * there was none — and whether this run is the one that replaced it. */
    float best_seconds;
    bool best_is_new;
    int time_bonus;
    int clean_bonus;
    /*
     * How many sheets of the docket the *run* is carrying, and out of how many.
     *
     * The one number on this line that is not about the sector, and it is here
     * because it was nowhere: `campaign.evidence_collected` was raised by the
     * pickup and then read by exactly two screens, both of which are the end of
     * the run. Twelve sheets is a collection the fiction spends a page on, laid
     * out one to an interior so that a missed one is always a floor the player
     * can name — and for the whole of a run they could not name it, because
     * nothing said how many they had until there was nothing to be done about
     * it.
     *
     * It goes on the scoreboard's line rather than on the strip for the reason
     * this whole file exists: the objection to telling the player is an
     * objection about *Chuck*, who is carrying paper that is worth nothing to
     * him, and this line is not Chuck. It is the same voice that already prints
     * what the seconds paid.
     */
    int docket_sheets;
    int docket_total;
    /*
     * The one line of the plot this clear was carrying, or NULL.
     *
     * The paragraph at the top of this file rescued the *numbers* off the
     * suppressed report and stopped there, and it stopped one item short of its
     * own argument. `TRANSITION_INTEL` is sixteen lines, one per completed
     * sector, and the report shows six of them — so ten were written, measured
     * by `test_manual_sheets_fit_the_column`'s neighbour, held against the maps
     * by `test_the_arc_lands_on_the_sectors_that_show_a_report`, and **read by
     * nobody**. Among them `MONITOR WALL: VOSS. HER HAND ON THE SEVENTH LOCK.`
     * and `TWELVE PLACES LAID IN THE GALLEY. TWELVE MEN.`
     *
     * The fiction's objection is to the *cut*, exactly as it was for the
     * bonuses: an ops screen over a window would contradict a continuous route
     * onto the facade. A line over the reveal cuts away from nothing, which is
     * the whole argument this file was written on — so the sentence rides the
     * same vehicle the seconds do.
     *
     * A pointer rather than a copy: every value that reaches it comes from
     * `intel_line`, whose table is static storage, and the header stays free of
     * a second buffer to keep in step with `INTEL_MAX`. NULL on the last sector
     * of the campaign, which has no row and does not want one — the outro is
     * the payoff and a summary over it would be the report's mistake again.
     */
    const char *intel;
} SectorTally;

/*
 * The one clear the staged screens show, and it is here rather than in game.c
 * because it was in game.c three times.
 *
 * `--screen report`, `--screen cleared` and `--screen reveal` each spelled out
 * the same five figures, and one of the five was a number no clear can
 * produce: `+1200 TIME` printed beside a stopwatch of 01:31. The clock pays
 * twenty a second under a par of 134, so 91 seconds pays 860, and 1200 is
 * exactly what 74 seconds pays — which is the *best* time sitting next to it
 * in the same call. Somebody swapped the two clocks to fix an earlier
 * impossible pair and left the bonus behind, which is one half of a symmetric
 * defect, again.
 *
 * Nothing failed and nothing could: every assertion this band has is about how
 * *wide* a line is, and a wrong number is exactly as wide as a right one. What
 * it cost is that `tools/press_kit.sh` cuts `12-report` out of the first of
 * those three screens, so the contradiction shipped in the press kit and on
 * the store page rather than in the game.
 *
 * The bonus is therefore not a figure here at all — it is
 * `campaign_time_bonus_for(SOAK_TALLY_ELAPSED)`, the same function
 * `campaign_award_sector_bonus` prices a real clear with. The clocks are, and
 * `test_the_sector_tally_fits_the_frame_it_is_drawn_in` requires them to be a
 * pair a clear can reach: over par is refused because the bonus would be
 * nought and the row would stop being the ordinary one, and a run quicker than
 * the record it is printed under is refused because `sector_tally_format`
 * spells `NEW BEST` instead.
 *
 * **And the rule the two clocks were given was never applied to the third
 * figure.** The docket was a flat seven, and seven is a number a clear can
 * reach on exactly one of the three screens that print it. One sheet is laid
 * out per interior, so a report after sector 1 can be holding at most one and a
 * reveal at the campaign's first window boundary at most two — and both of
 * those printed `DOCKET 07/12`, on the band whose whole subject is a collection
 * the player is meant to be counting. Same defect as the bonus above it, in the
 * paragraph written to record the bonus: the ceiling is still `SOAK_TALLY_DOCKET`
 * and what a screen actually stages is `sector_tally_soak_docket`, which asks
 * the campaign how much of the docket exists below the floor being cleared.
 *
 * **And the body count was the same figure one column over.** The report's
 * HOSTILES field was a flat six and sector one has two men on it, no dog, and
 * no door for a console to call anybody out of — so the frame the press kit
 * cuts `12-report` from reported six bodies on a floor that holds two. It is
 * clamped to `level_authored_hostiles` for the same reason the docket is
 * clamped to the sheets below the floor: a fixture whose own paragraph says
 * "numbers a real clear could produce" owes that of every number in it, not of
 * the two somebody has already been caught on. The score is deliberately left
 * a lump — it is a sum of kills, hacks, cards, sheets and both bonuses with no
 * denominator anywhere on the screen, so there is nothing for it to contradict.
 */
#define SOAK_TALLY_ELAPSED 91.0f
#define SOAK_TALLY_BEST 74.0f
#define SOAK_TALLY_DOCKET 7
#define SOAK_TALLY_HOSTILES 6

/* The staged clear's docket for a screen staging the clear of `sector`
 * (1-based): the fixture's figure, or the whole of what has been laid out below
 * that floor when the campaign has not laid out that much yet. */
int sector_tally_soak_docket(int sector);

/* And its body count, against what the floor's plan actually holds
 * (`level_authored_hostiles`). It takes the number rather than the map because
 * the docket's whole is a property of the campaign and a floor's population is
 * a property of one map — this side of the SDL boundary has the first and not
 * the second, and the ceiling is what both of them are here to apply. */
/*
 * The run's own score to stage beside the band, which is the field the two
 * screens that draw a tally over the sector strip were leaving at nought.
 *
 * `SECTOR 17 CLEAR ... +860 TIME +500 CLEAN ... DOCKET 07/12` over `SCORE
 * 0000000` is a pair the game cannot produce: the sheets alone are worth
 * `7 * EVIDENCE_SCORE` and the bonuses beside them have just been paid. This is
 * a *lower* bound rather than a guess at a real run's total — a run that reached
 * the last sector has sixteen floors of bonuses behind it as well — and a lower
 * bound is the whole of what is needed, because the defect is that nought is not
 * one. Derived from the same fixture the band is, for the reason the docket and
 * the hostile count are: two readings of one state must not be able to disagree.
 */
int sector_tally_soak_score(int sector);

int sector_tally_soak_hostiles(int authored_hostiles);

void sector_tally_clear(SectorTally *tally);
/* `docket_sheets` is the run's own count (`CampaignState.evidence_collected`);
 * the total it is shown against is worked out here, off the campaign's shape,
 * so no caller can pass a number that disagrees with the maps. `intel` is the
 * plot line this clear was carrying (`intel_line`), or NULL where there is
 * none. */
void sector_tally_set(SectorTally *tally, int sector, float elapsed_seconds,
                      float best_seconds, bool best_is_new,
                      int time_bonus, int clean_bonus, int docket_sheets,
                      const char *intel);

/* The story line the tally is carrying, or NULL — nothing pending, or a clear
 * with no row of its own. Read through a function so the renderer and the fit
 * check ask the same question of the same field. */
const char *sector_tally_intel(const SectorTally *tally);

/*
 * The line, written into `out`, and how many characters it came to.
 *
 * Nought when there is nothing pending, and `out` is left as an empty string
 * so a caller that draws unconditionally draws nothing rather than the stack.
 * That is the same promise `pad_hint` makes and for the same reason: the one
 * caller that forgets to check the return value must still be correct.
 */
int sector_tally_format(const SectorTally *tally, char *out, size_t cap);

#endif /* CHUCK_SECTOR_TALLY_H */
