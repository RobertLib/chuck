/*
 * One line of what the sector just told him, indexed by the sector he just
 * finished.
 *
 * The plot lives here rather than in a cutscene nobody replays or on a manual
 * page most players never open: this is a screen the run passes through
 * repeatedly, and a thriller told a sentence at a time between floors is the
 * version of it the game can be reasonably sure was read.
 *
 * Two constraints shape the table, and both are easy to break by accident.
 *
 * The width is the first of them. The report's first divider stands at
 * `INTEL_TEXT_RIGHT` and the line is set from `INTEL_TEXT_LEFT` in 8px cells,
 * so a line that runs under the divider stops being a line. That used to be a
 * sentence in a comment and nothing else, which is exactly the state the
 * manual's control sheet was in when its last bullet quietly fell off the
 * bottom of the column; `test_the_report_between_sectors_fits_its_column`
 * measures every row now, off the same two constants the renderer lays it out
 * from.
 *
 * And **a sector that leaves by the window has no report at all**: the window
 * is a continuous physical route out onto the facade, so
 * `try_finish_current_level` loads the next sector directly rather than
 * cutting to a screen that would contradict what is on the display. In the
 * campaign as shipped that is sectors 2, 3, 6, 7, 10, 11, 12, 13, 14 and 15,
 * which leaves exactly six reports — after 1, 4, 5, 8, 9 and 16 — and those six
 * carry the whole arc: she walked in, they have been here since March, the
 * police response is the plan, they took her to open a door, the bonds leave at
 * 01:00, and she is the second key. The other ten lines are written anyway,
 * because the table is indexed by sector and a sector that later gains a stair
 * door must not gain a blank line with it.
 *
 * **Those other ten used to be written and read by nobody**, which is a
 * different thing from being off the arc and was not noticed for as long as the
 * two were spelled the same way. A row that no screen draws is not a spare row,
 * it is a deleted one — `MONITOR WALL: VOSS. HER HAND ON THE SEVENTH LOCK.`
 * deleted, with every gate green, because every check here asks whether a line
 * fits its column and none asked whether anything puts it on the glass. They
 * ride the tally over the next sector's reveal now (see
 * [sector_tally.h](sector_tally.h)), which is the vehicle the suppressed
 * *numbers* were already given and the same argument: the fiction objects to
 * the cut, and a line over a reveal cuts away from nothing.
 *
 * **Which six those are is a property of the maps, not of this file**, and that
 * is the whole reason `INTEL_ARC_SECTORS` below exists rather than the row
 * comments being the only record. The paragraph above was true of a
 * fifteen-sector campaign and stopped being true the day sector 15 became a
 * climb: sector 14 had to gain a `Y` to put Chuck on the wall, a `Y` is a
 * window, and a window silently took the report after it away. The line lost
 * that way was `TWO-KEY DOOR. SHE IS THE SECOND.` — the answer to sector 8's
 * own turn, and the only place in the game that says why she is still alive.
 * A beat the ending depends on sat in a row nothing reads, in the exact shape
 * the note on the table below warns about, for as long as nothing held the two
 * halves together. `test_the_arc_lands_on_the_sectors_that_show_a_report` is
 * what holds them now: a map that gains or loses a window fails the build
 * rather than quietly rewriting the plot.
 *
 * Sector 8 is where the turn goes, and the beat it displaced says why. The
 * load-out — a contractor's flight cases nobody weighed — is the single most
 * over-told thing in the game already: an `m` case stands in every interior
 * sector, the manual's `THE NIGHT` sheet is illustrated with one, and the crew
 * say it themselves on the net. "Not for ransom" is told nowhere else while a
 * sector is being played, so it needs a slot early enough to recontextualise
 * the rest rather than arriving beside its own payoff. It sets up sector 16,
 * which now answers it with the mechanism: the door takes two keys and she is
 * the second.
 *
 * **What sector 8 adds is the door, not the headline**, and this paragraph
 * said otherwise for a release: "the only line that overturns what the player
 * has assumed since the kerb". The kerb is where they are *told*. The
 * prologue's second beat holds `NO WORD // NO RANSOM // THEY CAME FOR HER` on
 * the glass for two and a half seconds before the drive has even started, so
 * there is no assumption left for a report seven sectors later to overturn —
 * only a mechanism to supply, which is the clause that actually earns the
 * screen. A rationale that argues from a state the player was never allowed
 * into is this file's own recurring defect wearing the plot's clothes.
 *
 * **And it was being told a third time, one sector early, by a row that was
 * written on the understanding that nobody would read it.** Sector 7's line
 * was `THEIR DEMAND WENT OUT AT 00:04. IT ASKED FOR NO MONEY.` — the first
 * half a near-verbatim restatement of sector 5's report, the second half
 * sector 8's turn, delivered on the reveal *into* sector 8. It was harmless
 * for exactly as long as a window suppressed it, and the change that gave the
 * ten unmarked rows a vehicle did not go back and re-read the arc as a whole.
 * That is the shape this tree keeps finding from the other end: a fix that
 * makes something visible, and nothing asking what it is now standing next to.
 * The row says what a climb can see instead, and the turn is told once.
 *
 * **Sector 16 is the right room for that answer and not merely the nearest
 * shown slot.** The line it displaced said the vault was open and empty, which
 * is the one thing on the whole table the player does not need telling — they
 * have just walked the length of the emptied strongroom to get to this screen.
 * That is the same trade the old sector 13 line already made when the vault
 * became a room instead of an inference, and the same rule both times: a report
 * is for what the sector could not show.
 */

#include "intel.h"

#include <stddef.h>

/*
 * The six rows marked SHOWN get a whole screen to themselves — the report — and
 * they have to carry the arc between them. Every other row is read too, on the
 * one line the tally puts over the next sector's reveal, so the difference
 * between a marked row and an unmarked one is how much room the beat gets and
 * how long the player has with it, not whether it exists.
 *
 * **It used to be whether it exists**, and the sentence here said so: "put a
 * beat the ending depends on in an unmarked row and nobody will ever see it."
 * That was true, and it is the kind of true that reads as a note about
 * emphasis. A beat that needs the player to stop and take it in still belongs
 * on the arc; one that only needs saying can go anywhere on this table.
 */
static const char *const TRANSITION_INTEL[] = {
    /*  1 LOBBY    SHOWN */ "FRONT DESK LOG: SHE BADGED IN AT 00:22. CALMLY.",
    /*  2 OFFICE         */ "EVERY STAIR CORE IS WELDED. SEALED FROM THE INSIDE.",
    /*  3 CLIMB          */ "NO SIRENS UP HERE. THE WHOLE RING IS FACING OUT.",
    /*  4 SERVER   SHOWN */ "MERIDIAN. NIGHT MAINTENANCE CONTRACTOR SINCE MARCH.",
    /*  5 PLANT    SHOWN */ "THEIR DEMAND AT 00:04 BOUGHT THE CORDON. NOBODY IS COMING.",
    /*  6 CANTEEN        */ "TWELVE PLACES LAID IN THE GALLEY. TWELVE MEN.",
    /*  7 CLIMB          */ "STORM COMING IN. THE LAB WINDOWS ARE BLACKED OUT.",
    /*  8 LAB      SHOWN */ "NOT FOR RANSOM. THEY TOOK HER TO OPEN A DOOR.",
    /*  9 ARCHIVE  SHOWN */ "01:00: SIX HUNDRED AND FORTY MILLION LEAVES.",
    /* 10 SECURITY       */ "MONITOR WALL: VOSS. HER HAND ON THE SEVENTH LOCK.",
    /*
     * The one row on the table that states a *duration*, which makes it
     * arithmetic on `NIGHT_CLOCK_*` rather than a fact about the building —
     * and it went stale the way every other reading of that clock did when the
     * night was divided seventeen ways instead of fifteen. This row is read
     * entering sector 12, whose dial says 00:46, so what is left before the
     * helicopter is fourteen minutes; it said ten.
     *
     * Nothing caught it because the row was one of the ten a window suppressed
     * outright. The suite measures every row's *width* and `INTEL_ARC_SECTORS`
     * pins which of them reach a *report*, so this line was checked for the one
     * property that did not matter while nothing drew it. `check_docs.py`
     * derives the figure off the dial now, the same way it derives the dials
     * the pages quote — and the row itself reaches the player over the reveal,
     * so a wrong number here is now a wrong number somebody reads.
     */
    /* 11 CLIMB          */ "THE SETTLEMENT CLOCK IS RUNNING. FOURTEEN MINUTES.",
    /* 12 DUCTS          */ "FLIGHT CASES CAME UP THIS SHAFT. NOBODY WEIGHED THEM.",
    /*
     * 13 was "THE VAULT IS OPEN AND EMPTY. THEY ARE GOING UP." for as long as
     * the vault was a thing Chuck worked out rather than a room he walked
     * through. He walks through it now, three sectors later, so the line moved
     * to where he can actually see it and this climb says what the climb is
     * for instead.
     */
    /* 13 CLIMB          */ "PENTHOUSE LIGHTS ARE ON. SOMEBODY IS STILL WORKING.",
    /*
     * 14 leaves by the window — sector 15 is a climb, and a climb is entered
     * from one — so this row never reaches a report. It says a penthouse thing
     * rather than an arc thing for exactly that reason: the beat it used to
     * carry is at 16 now, where a report will actually carry it.
     *
     * It is read, though, which it was not when that sentence was written: the
     * tally over the next sector's reveal carries every row a report does not.
     * The arc still belongs on the six, because a report is where a beat can be
     * given a whole screen — what changed is that a row off the arc is now a
     * quieter line rather than a dead one.
     */
    /* 14 PENTHSE        */ "THE DUTY OFFICE. HER COAT IS STILL ON THE CHAIR.",
    /* 15 CLIMB          */ "SLEET AGAIN. THE PAD LIGHTS ARE LIT TWO FLOORS UP.",
    /* 16 VAULT    SHOWN */ "TWO-KEY DOOR. SHE IS THE SECOND. VOSS IS ON THE ROOF.",
};

/*
 * The sectors whose lines the plot rests on, 1-based.
 *
 * This is the same claim the SHOWN comments above make, written where a test
 * can read it — and it is written down twice on purpose, because the two
 * halves are checked against each other rather than against a reader's memory.
 * `test_the_arc_lands_on_the_sectors_that_show_a_report` walks the embedded
 * maps, works out which sectors actually reach a report (every one that is not
 * the last and does not leave by a window, which is `try_finish_current_level`'s
 * own rule), and requires that set to be exactly this one.
 *
 * So a map edit that turns a stair door into a window — the edit that silently
 * dropped `TWO-KEY DOOR. SHE IS THE SECOND.` out of the shipped game — now
 * fails `make test` with the sector number in the message. **A new sector, or a
 * `Y` added to an existing one, owes this array an edit and the arc a re-read**:
 * the list is not a description of the maps, it is a decision about where the
 * story is told, and the suite exists to stop the maps making that decision by
 * accident.
 */
const int INTEL_ARC_SECTORS[] = {1, 4, 5, 8, 9, 16};
const int INTEL_ARC_SECTOR_COUNT =
    (int)(sizeof(INTEL_ARC_SECTORS) / sizeof(INTEL_ARC_SECTORS[0]));

#define INTEL_COUNT ((int)(sizeof(TRANSITION_INTEL) / sizeof(TRANSITION_INTEL[0])))

int intel_line_count(void)
{
    return INTEL_COUNT;
}

const char *intel_line(int completed_level)
{
    if (completed_level < 0 || completed_level >= INTEL_COUNT)
        return NULL;
    return TRANSITION_INTEL[completed_level];
}
