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

/* The name a guard answers to, by enemy slot. Wraps: a sector may hold more
 * bodies than the crew has men, and the twelve names repeating is the point —
 * it is the same twelve all night. */
const char *crew_callsign(int speaker);

/* How the crew talk, by what prompted it. `roll` is the number the simulation
 * drew when the line was spoken and may be anything. */
const char *crew_line(ChatterKind kind, int roll);

/* How many lines a kind has. Only the tests and the manual ask. */
int crew_line_count(ChatterKind kind);

#endif /* CHUCK_CREW_H */
