#ifndef CHUCK_INTEL_H
#define CHUCK_INTEL_H

/*
 * The report between sectors: one line of what the sector just told him.
 *
 * This is the sixth of the places the plot reaches the player, and the only
 * one of them that used to sit inside an SDL renderer. It is a table of
 * strings measured against a divider, exactly like [credits.c](credits.c) and
 * [manual_pages.c](manual_pages.c), so it belongs on the same side of the line
 * they do: the suite links no SDL, and a line that runs under the divider is
 * as invisible as a manual sheet that runs off the bottom of its column.
 *
 * It spent a long time in [cutscene.c](cutscene.c) with its ceiling written
 * down as a sentence in a comment — "sixty characters, the first divider
 * stands at x=526" — and nothing to hold it there. Both numbers now live here,
 * the renderer lays the line out from them, and
 * `test_the_report_between_sectors_fits_its_column` measures every row off the
 * same two constants, so the table and the frame it is drawn in cannot
 * disagree about where a line ends.
 *
 * Everything here is presentation. The simulation never reads a word of it.
 */

/*
 * Where the line is set, and what stops it.
 *
 * The left edge is past the rust tick that marks the line; the right edge is
 * the report's first column divider, and the type is
 * `SDL_RenderDebugText`'s 8x8 bitmap at scale 1.0 — the only size it is sharp
 * at, so a line that does not fit is cut in words rather than in scale.
 */
#define INTEL_TEXT_LEFT 37.0f
#define INTEL_TEXT_RIGHT 526.0f
#define INTEL_GLYPH_W 8.0f

/*
 * The line for a finished sector, or NULL when there is none.
 *
 * Indexed by finished sector, but only a sector that leaves by a **stair
 * door** shows a report at all — a window is a continuous physical route onto
 * the facade and cuts straight to the next sector. In the campaign as shipped
 * that is six reports, after sectors 1, 4, 5, 8, 9 and 16, and those six carry
 * the arc on their own. The other rows are written anyway, because the table
 * is indexed by sector and a sector that later gains a stair door must not
 * gain a blank line with it.
 *
 * **What that rule was never an argument for is the scoreboard going with it.**
 * The report carries the stopwatch, the record, the two bonuses and the tally
 * as well as the line above, and for as long as the window rule existed the
 * other eleven clears paid a bonus and banked a record in silence. The
 * objection is to the *cut*, not to the numbers, so the numbers now travel on
 * their own — see [sector_tally.h](sector_tally.h).
 */
const char *intel_line(int completed_level);

/* How many rows the table has, so the suite can walk all of them rather than
 * only the six the shipped layout happens to show. */
int intel_line_count(void);

/*
 * Which sectors the arc is told on, 1-based, and how many there are.
 *
 * Exported so the suite can hold this decision against the maps that have to
 * honour it: a sector leaves by a window or by a stair door, only the second
 * kind reaches a report, and a `Y` added to a map has therefore already moved
 * the plot once without anybody noticing. See the note on the array in
 * [intel.c](intel.c) for which line that cost and how.
 */
extern const int INTEL_ARC_SECTORS[];
extern const int INTEL_ARC_SECTOR_COUNT;

#endif /* CHUCK_INTEL_H */
