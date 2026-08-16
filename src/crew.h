#ifndef CHUCK_CREW_H
#define CHUCK_CREW_H

#include "game_event.h"

/*
 * Meridian's night shift, written down once.
 *
 * The story page says twelve men badged into this tower under one contractor
 * name, and until now that number was only ever *stated* — in a cutscene, in a
 * report between sectors, on a sheet of the manual. All three are places the
 * player is not playing. This file is the same twelve men in the place they
 * actually are: on their own net, in the room, while Chuck is standing in it.
 *
 * Two halves, and the split is the reason the file exists at all:
 *
 * - The **roster**. A guard's callsign is filed by the slot he stands in, not
 *   drawn from the RNG. Nothing in the simulation may depend on which name a
 *   man wears, and a draw would both shift the seeded stream and make the same
 *   guard answer to a different name on a retry of the same sector.
 * - The **traffic**. The gameplay core knows a man spoke, where, and one
 *   opaque number it drew while doing it. It does not know a single word of
 *   what he said: the strings are presentation, they live here, and the shell
 *   spells them. That is the same boundary a sound effect crosses — an index
 *   out of the simulation, a waveform on the other side.
 *
 * `crew_line` takes the raw roll and folds it itself, so no caller can get the
 * modulus wrong or reach past the end of a table.
 */

#define CREW_SIZE 12

/*
 * The widest a line may be, in cells of the 8x8 font. A callsign, a colon and
 * this much text is what the intercept plate along the top of the screen is
 * sized for; `test_crew_traffic_fits_the_plate` fails the build rather than
 * letting a line run off the edge of the frame where nobody would see it.
 */
#define CREW_LINE_MAX 56

/*
 * How far the night has got when somebody speaks.
 *
 * A good half of the writing on this net is *about* where the player is — the
 * vault being empty, the roof going in two minutes, nothing coming back from
 * MARCO — and the tables are rolled from with no idea of any of that, so those
 * lines used to be sayable by a man standing in the lobby at 00:22 before
 * Chuck had climbed a floor or touched anybody. The gate is per line and lives
 * beside the line, in [crew.c](crew.c).
 *
 * `sector` is the 1-based campaign sector, the same number the strip prints;
 * `hostiles_down` is the run's tally rather than the floor's, because a crew
 * that has lost two men does not get them back when the player opens a stair
 * door.
 *
 * The tally reads in **both** directions. A handful of lines name an exact
 * number of men still standing, and those are true over a window rather than
 * from a moment onward — early they have not happened yet, late the player has
 * personally disproved them. See `CrewLine` in [crew.c](crew.c).
 */
typedef struct
{
    int sector;
    int hostiles_down;
} CrewSituation;

/* The name a guard answers to, by enemy slot. Wraps: a sector may hold more
 * bodies than the crew has men, and the twelve names repeating is the point —
 * it is the same twelve all night. */
const char *crew_callsign(int speaker);

/* How the crew talk, by what prompted it. `roll` is the number the simulation
 * drew when the line was spoken and may be anything. No speaker and no
 * situation, so it answers about the table itself: every line is reachable,
 * which is what lets the suite measure all of them. */
const char *crew_line(ChatterKind kind, int roll);

/* The same, with the speaker's own callsign ruled out of the answer: several
 * lines name a man, and a guard reporting that nothing has been heard from
 * himself reads as a table being shuffled rather than as a crew talking. Pass
 * NULL for a speaker who is not named on the strip at all — the people leaving
 * the lobby are on nobody's docket — which is exactly what `crew_line` does. */
const char *crew_line_said_by(ChatterKind kind, int roll,
                              const char *callsign);

/* What the shell actually calls: the speaker ruled out as above, and the night
 * ruled out as well. A NULL situation is every situation. */
const char *crew_line_in(ChatterKind kind, int roll, const char *callsign,
                         const CrewSituation *situation);

/* How many lines a kind has. Only the tests and the manual ask. */
int crew_line_count(ChatterKind kind);

/* Whether the line at `index` may be spoken in this situation. The gate is a
 * property of the table rather than of what came back, so the suite asks the
 * table about it directly instead of trying to infer it from a returned
 * string — see `test_the_net_always_has_something_to_say`. */
bool crew_line_allowed(ChatterKind kind, int index,
                       const CrewSituation *situation);

#endif /* CHUCK_CREW_H */
