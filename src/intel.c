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
 * campaign as shipped that is sectors 2, 3, 6, 7, 10, 11, 12 and 13, which
 * leaves exactly six reports — after 1, 4, 5, 8, 9 and 14 — and those six
 * carry the whole arc: she walked in, they have been here since March, the
 * police response is the plan, they took her to open a door, it is the vault,
 * and she is the second key. The other eight lines are written anyway, because
 * the table is indexed by sector and a sector that later gains a stair door
 * must not gain a blank line with it.
 *
 * Sector 8 is where the turn goes, and the beat it displaced says why. The
 * load-out — a contractor's flight cases nobody weighed — is the single most
 * over-told thing in the game already: an `m` case stands in every interior
 * sector, the manual's `THE NIGHT` sheet is illustrated with one, and the crew
 * say it themselves on the net. "Not for ransom" is told nowhere else while a
 * sector is being played, and it is the only line that overturns what the
 * player has assumed since the kerb, so it needs a slot early enough to
 * recontextualise the rest rather than arriving beside its own payoff. It sets
 * up sector 14, which now answers it with the mechanism: the door takes two
 * keys and she is the second.
 */

#include "intel.h"

#include <stddef.h>

/*
 * The six rows marked SHOWN are the whole of the plot the player meets while
 * actually playing, and they have to carry the arc between them; the rest are
 * written for the sectors they belong to and are only read if the campaign's
 * layout changes. Put a beat the ending depends on in an unmarked row and
 * nobody will ever see it.
 */
static const char *const TRANSITION_INTEL[] = {
    /*  1 LOBBY    SHOWN */ "FRONT DESK LOG: SHE BADGED IN AT 00:22. CALMLY.",
    /*  2 OFFICE         */ "EVERY STAIR CORE IS WELDED. SEALED FROM THE INSIDE.",
    /*  3 CLIMB          */ "NO SIRENS UP HERE. THE WHOLE RING IS FACING OUT.",
    /*  4 SERVER   SHOWN */ "MERIDIAN. NIGHT MAINTENANCE CONTRACTOR SINCE MARCH.",
    /*  5 PLANT    SHOWN */ "THEIR DEMAND AT 00:04 BOUGHT THE CORDON. NOBODY IS COMING.",
    /*  6 CANTEEN        */ "TWELVE PLACES LAID IN THE GALLEY. TWELVE MEN.",
    /*  7 CLIMB          */ "THEIR DEMAND WENT OUT AT 00:04. IT ASKED FOR NO MONEY.",
    /*  8 LAB      SHOWN */ "NOT FOR RANSOM. THEY TOOK HER TO OPEN A DOOR.",
    /*  9 ARCHIVE  SHOWN */ "01:00: SIX HUNDRED AND FORTY MILLION LEAVES.",
    /* 10 SECURITY       */ "MONITOR WALL: VOSS. HER HAND ON THE SEVENTH LOCK.",
    /* 11 CLIMB          */ "THE SETTLEMENT CLOCK IS RUNNING. TEN MINUTES.",
    /* 12 DUCTS          */ "FLIGHT CASES CAME UP THIS SHAFT. NOBODY WEIGHED THEM.",
    /* 13 CLIMB          */ "THE VAULT IS OPEN AND EMPTY. THEY ARE GOING UP.",
    /* 14 PENTHSE  SHOWN */ "TWO-KEY DOOR. SHE IS THE SECOND. VOSS IS ON THE ROOF.",
};

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
