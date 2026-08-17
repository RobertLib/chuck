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
} SectorTally;

void sector_tally_clear(SectorTally *tally);
/* `docket_sheets` is the run's own count (`CampaignState.evidence_collected`);
 * the total it is shown against is worked out here, off the campaign's shape,
 * so no caller can pass a number that disagrees with the maps. */
void sector_tally_set(SectorTally *tally, int sector, float elapsed_seconds,
                      float best_seconds, bool best_is_new,
                      int time_bonus, int clean_bonus, int docket_sheets);

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
