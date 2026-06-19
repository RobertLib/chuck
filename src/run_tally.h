#ifndef CHUCK_RUN_TALLY_H
#define CHUCK_RUN_TALLY_H

#include "progress.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * What a finished run made, on both of the screens a run can finish on.
 *
 * The game-over card has printed the score and the docket since either of them
 * outlived the process, and its own comment calls itself "the only screen a
 * score is being looked at rather than played for". That was true of the card
 * and false of the game: a run also finishes by **being won**, and the outro
 * and the roll of names behind it printed neither number. `game_record_run_score`
 * runs on the way into `STATE_OUTRO`, so a winning run banked its score, banked
 * its sheets, and then showed the player nothing — the one ending the whole
 * campaign is played for was the one ending that said least about it.
 *
 * The docket makes it worse than a missing number. Twelve sheets is a
 * collection the fiction spends a page on ("worth something to everybody else,
 * afterwards", [docs/story.md](../docs/story.md)); a player who finds all
 * twelve and walks onto the helicopter with them got no acknowledgement of any
 * kind, while a player who died on sector three was told how many they had.
 *
 * So the words live here, once, and both screens draw them: a table of words
 * the player reads, in its own SDL-free file, with the frame it has to fit
 * written as constants the renderer lays it out from and
 * `test_the_run_tally_fits_the_frame_it_is_drawn_in` measuring the widest line
 * either screen can produce. That is the rule in [AGENTS.md](../AGENTS.md), and
 * the two lines below were literals inside `draw_game_over_panel` when it was
 * written — which is exactly the state the options sheet and the pause sheet
 * were in when measuring them found a line already lost.
 */

/*
 * Where the lines are set, and what stops them.
 *
 * The frame is 800x552 (`SDL_SetRenderLogicalPresentation` in game.c) and the
 * type is `SDL_RenderDebugText`'s 8x8 bitmap. The game-over card draws these at
 * scale 2.0 and the outro at 1.0, so the bound is the coarser of the two: a
 * line that fits the card fits the outro with room to spare, and writing the
 * card's own margins down here is what keeps the check honest about the screen
 * that is actually tight.
 */
#define RUN_TALLY_TEXT_LEFT 24.0f
#define RUN_TALLY_TEXT_RIGHT 776.0f
#define RUN_TALLY_GLYPH_W 8.0f
#define RUN_TALLY_CARD_SCALE 2.0f

/* Room for the widest line plus its terminator. The suite holds the two
 * together, so this cannot quietly become the thing that truncates. */
#define RUN_TALLY_MAX 64

/*
 * The score line, written into `out`, and how many characters it came to.
 *
 * `assisted` is the run having had any assist switch on at any point in it (see
 * `campaign_note_assist`), and it changes what the line can honestly say rather
 * than only how it reads: an assisted run banks no records, so there is no
 * "best" for it to be measured against and printing one would compare this run
 * to a ladder it was never on. Saying so is the whole point — a player who
 * turned infinite lives on and was never told their times had stopped counting
 * would find out by never beating them again.
 */
int run_tally_format_score(int score, int best_score, bool assisted,
                           char *out, size_t cap);

/*
 * The docket line, or nought when there is nothing to say about it.
 *
 * Nought is a real answer rather than a defensive branch: a run that took no
 * sheets, on a profile that never has, has no line here and both screens simply
 * do not draw one. `out` is left as an empty string in that case, which is the
 * same promise `sector_tally_format` and `pad_hint` make so that a caller which
 * forgets to check the return value still draws nothing rather than the stack.
 */
int run_tally_format_docket(int sheets, int best_sheets, bool assisted,
                            char *out, size_t cap);

/*
 * One sector's record, for the sheet that shows all seventeen of them.
 *
 * `Progress` has kept a per-sector best since the report between floors started
 * printing one, and the only place a player could ever see one was the sector
 * they had just finished — so sixteen of the seventeen numbers on their own disk
 * were unreadable at any given moment, and the pages argue at length that "a par
 * you cannot measure yourself against is a par nobody plays for twice". THE
 * RECORD sheet in the field manual is where they are all readable, and this is
 * the cell it lays out.
 *
 * `PROGRESS_NO_TIME` prints as `--:--`, which is a real answer rather than a
 * blank: a sector nobody has finished is a different thing from one finished in
 * no time at all, and that distinction is the whole reason `PROGRESS_NO_TIME`
 * exists rather than nought meaning "perfect".
 *
 * The column is `RUN_TALLY_SECTOR_CELL_W` wide because the sheet sets these in
 * two columns and the renderer steps by that number; the suite measures the
 * widest cell any of the seventeen can produce against it, so the words and the
 * grid they are set in cannot drift.
 */
#define RUN_TALLY_SECTOR_MAX 16
#define RUN_TALLY_SECTOR_CELL_W 88.0f

int run_tally_format_sector_time(int sector_index, float seconds, char *out,
                                 size_t cap);

/*
 * The same three figures as a headline, for the sheet that offers to delete
 * them.
 *
 * The options sheet grew a RECORDS page when the main page ran out of height,
 * and for a release it was a page named RECORDS whose strap listed what the game
 * keeps — the best score, the docket and every sector's best time — over a single
 * row that clears all of it. The numbers themselves were readable in the field
 * manual and nowhere else, so the one screen in the game whose subject is the
 * records was the one screen that would not show them, and the destructive thing
 * on it asked for a confirmation without saying what was about to be lost.
 *
 * Formatted here rather than in [settings.c](settings.c) because this is the file
 * that already owns what a record reads as — `run_tally_format_sector_time` is
 * the manual's own cell — and because the answer has to come out of `Progress`,
 * which the options table has no business knowing about. `RUN_TALLY_RECORD_MAX`
 * is the ceiling every one of them fits, held by
 * `test_the_records_page_shows_what_it_offers_to_delete`.
 */
typedef enum
{
    RUN_TALLY_RECORD_SCORE,
    RUN_TALLY_RECORD_DOCKET,
    RUN_TALLY_RECORD_FURTHEST,
    RUN_TALLY_RECORD_SECTORS_TIMED,
    RUN_TALLY_RECORD_COUNT
} RunTallyRecord;

/* Room for the widest value plus its terminator. */
#define RUN_TALLY_RECORD_MAX 16

/*
 * `progress` is the player's own file. A figure nothing has ever been written to
 * prints as the same `--` the manual's sheet uses for a sector nobody has
 * finished, because nought and "never" are different answers and a page offering
 * to clear a record must not show one where there is none.
 */
int run_tally_format_record(RunTallyRecord which, const Progress *progress,
                            char *out, size_t cap);

/* What the row above the figure is called. */
const char *run_tally_record_label(RunTallyRecord which);

/*
 * The same figure, spelled from the number rather than from the player's file.
 *
 * `run_tally_format_record` above is the options sheet's call and reads
 * `Progress` directly. THE RECORD sheet in the field manual cannot: the manual
 * is handed plain integers precisely so that it links no `Progress` (see
 * `ManualRecords`), and for a release that is why it spelled its two run figures
 * with an `SDL_snprintf` of its own — `BEST SCORE 0` and `DOCKET 0` on a fresh
 * install, where the options page reading the same file said `--` and the
 * seventeen cells on that very card said `--:--`. Three screens read a record
 * and the two that went through this file agreed; the third was a renderer
 * literal on the far side of the SDL boundary, so nothing could compare it with
 * anything.
 *
 * This is the way in for a caller holding the number instead of the file, which
 * is what was actually missing: the rule that nought and never are the same
 * state, and both are `--`, now has one home for every screen that asks.
 *
 * `value` is the stored figure — the score, the sheets, the **0-based** furthest
 * sector, or how many sectors are timed.
 */
int run_tally_format_record_value(RunTallyRecord which, int value, char *out,
                                  size_t cap);

/*
 * Label and figure as one line, for the card that sets the two as prose rather
 * than in a table with the names in a column of their own.
 *
 * `RUN_TALLY_RECORD_LINE_W` is what THE RECORD sheet's card leaves for one:
 * `PANEL_W` less its frame, less the card's own inset, less a text margin
 * either side. The suite measures the widest line every figure can produce
 * against it, because otherwise this is a renderer's own literal in a renderer's
 * own layout — which is the state the options sheet's footer and the pause
 * sheet's rows were both found in.
 */
#define RUN_TALLY_RECORD_LINE_MAX 40
#define RUN_TALLY_RECORD_LINE_W 258.0f

int run_tally_format_record_line(RunTallyRecord which, int value, char *out,
                                 size_t cap);

#endif /* CHUCK_RUN_TALLY_H */
