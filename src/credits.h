#ifndef CHUCK_CREDITS_H
#define CHUCK_CREDITS_H

#include <stdbool.h>

/*
 * The roll that follows the ending.
 *
 * The outro holds its thank-you frame for a few seconds and then hands over to
 * this: the names rise through the same letterboxed, grainy frame the rescue
 * ended in, because the game ends the way the films it came out of end. When
 * the roll runs out the shell goes back to the title screen, so a finished
 * campaign lands where a new one starts rather than parking on a card the
 * player has to dismiss.
 *
 * The module links no SDL, for the same reason [crew.c](crew.c) and
 * [settings.c](settings.c) do not: it is a table of strings plus the geometry
 * of how tall each row stands, so the test suite can hold every line to the
 * width of the frame. `game_render.c` draws whatever it finds in the table,
 * exactly the way it draws the options sheet — a line added here needs no
 * layout, and a line too long to fit fails the build instead of running off
 * the edge of a screen nobody would ever check.
 */

/* The kinds are the whole layout language: what a row is decides its size,
 * its colour and how much air it stands in. */
typedef enum
{
    CREDIT_TITLE, /* the game's own name, at the head of the roll and again at
                     the foot of it */
    CREDIT_ROLE,  /* what the job was — small, quiet, and above the name */
    CREDIT_NAME,  /* who did it */
    CREDIT_NOTE,  /* a line of prose about the build or the night */
    CREDIT_RULE,  /* a short rule between blocks */
    CREDIT_GAP
} CreditLineKind;

typedef struct
{
    CreditLineKind kind;
    const char *text; /* NULL for a rule and a gap, which carry no words */
} CreditLine;

/* The frame the roll is measured against and the inset every line keeps from
 * its edges. The 8x8 debug font is drawn at scale 1 or 2 and nothing between,
 * so a line's width is exactly its length in cells — which is what makes the
 * fit a thing a test can prove rather than something to eyeball. */
#define CREDITS_FRAME_W 800.0f
#define CREDITS_SIDE_MARGIN 24.0f

/* Pixels a second the roll climbs. Slow enough to read a name twice, quick
 * enough that the whole roll is over in about half a minute. */
#define CREDITS_SCROLL_SPEED 46.0f

/* Where the last line comes to rest: the closing card sits a little above the
 * middle of the frame, with the roll stopped under it. */
#define CREDITS_REST_Y 300.0f

/* How long the closing card is held before the title screen takes over. */
#define CREDITS_HOLD_TIME 5.0f

/*
 * The longest the whole roll may take, closing hold included.
 *
 * It is a ceiling rather than a measurement because something outside this file
 * has to wait for it: `make smoke` dwells on each screen for a fixed few
 * seconds, and a screen that is a *clock* rather than a still is only covered
 * for the beats that fit inside the dwell. The roll opens on a skyline and ends
 * on a card thirty-odd seconds later, so a three-second dwell executed the
 * skyline — which is where the one undefined-behaviour bug this whole target
 * was written to catch was sitting — and never once reached the card.
 *
 * [../tools/smoke.sh](../tools/smoke.sh) reads this number out of this header
 * rather than keeping a copy of it, the way CI reads the pinned SDL version out
 * of the script that owns it, and `test_credits_fit_the_frame` holds the table
 * under it. A roll that outgrows the dwell therefore fails the build instead of
 * quietly outliving the only thing that runs it.
 */
#define CREDITS_MAX_DURATION 45.0f

typedef struct
{
    float time;     /* seconds since the roll began */
    float travel;   /* pixels it climbs before it comes to rest */
    float duration; /* the whole thing, the closing hold included */
} CreditsRoll;

/* The roll, in order. */
const CreditLine *credits_lines(int *out_count);

/* How tall a row stands and what scale its text is set at. Both are asked per
 * kind rather than assumed by the renderer, so a new kind is a case here and
 * nothing at the draw site. */
float credits_line_height(CreditLineKind kind);
float credits_line_scale(CreditLineKind kind);

/* `view_h` is the height of the frame the roll is drawn in: it decides how far
 * the first line has to climb before the last one is standing on its mark. */
void credits_init(CreditsRoll *roll, float view_h);

/* Advances the roll and returns true once it has run out, which is the shell's
 * cue to go back to the title screen. */
bool credits_update(CreditsRoll *roll, float dt);

/* Confirm, while the names are still moving: straight to the closing card,
 * which is then held as if it had been reached the slow way. */
void credits_skip_to_rest(CreditsRoll *roll);

/* True once the roll has stopped — the beat where confirm means "done" rather
 * than "get on with it". */
bool credits_at_rest(const CreditsRoll *roll);

/* Pixels the roll has climbed so far. The first line starts one frame below
 * the bottom edge, so a line's y on screen is `view_h + its offset - this`. */
float credits_scroll(const CreditsRoll *roll);

#endif /* CHUCK_CREDITS_H */
