#ifndef CHUCK_PAUSE_SHEET_H
#define CHUCK_PAUSE_SHEET_H

/*
 * What the pause menu says, apart from how it is drawn.
 *
 * Four rows, a title, a strap, an armed warning and two footer prompts — a table
 * of words the
 * player reads, which is the one kind of thing this tree insists lives in its
 * own SDL-free file with the geometry it has to fit beside it. It did not: the
 * labels and the details were string literals inside `draw_pause_menu` and the
 * plate's width was a local `const float`, so nothing in `make test` could
 * measure one against the other.
 *
 * **That is exactly the shape `settings.c` was found in**, and the note in
 * AGENTS.md about it is worth restating here because this file is the second
 * time the same conclusion was reached: *"a table of words that stays inside its
 * renderer is a table nothing measures"*, and when the check was finally written
 * for the options sheet it immediately found a line already running thirty
 * pixels off the plate. This one was measured by hand at the point it was split
 * out and did fit — the widest detail is `GIVE UP THIS RUN AND RETURN TO THE
 * TITLE` at 320px inside a 420px plate — so the check here is not paying off a
 * bug. It is the only thing that will notice the fourth row somebody adds, or
 * the day `ABANDON RUN` becomes `ABANDON THIS RUN AND LOSE THE SCORE`.
 *
 * **The fourth row has since arrived, and the check is what cleared it.**
 * `FIELD MANUAL` is here because the manual could not be opened from inside a
 * run at all — see `game_open_manual` — and the row it needed is the row this
 * paragraph was written waiting for: the words were measured against the plate
 * and the plate against the rows it now has to be tall enough for, before either
 * was drawn once.
 *
 * The pause menu and the options sheet are drawn on the same plate by the same
 * `draw_sheet_plate`, so their two widths are deliberately separate constants
 * rather than one shared one: they are the same *object* seen twice, not the
 * same size, and the options sheet is wider because it carries key caps.
 */

/*
 * The rows, in the order they are drawn and in the order `pause_cursor` walks.
 * Kept as an X-macro so the enum, the table and the count cannot come to
 * disagree about how many rows there are — the failure the manual's mission
 * sheet had when its loop counted to a literal.
 */
/*
 * The OPTIONS row's detail read `SOUND, DISPLAY AND ASSIST` for a release after
 * the sheet it describes stopped being that sheet. The options sheet is four
 * pages now — AUDIO and DISPLAY on the first, with a row down to CONTROLS,
 * DIFFICULTY and RECORDS — so ASSIST is two levels away and is not a heading
 * anybody arrives at. Not a false sentence, which is why nothing caught it: the
 * assist switches are still reachable through this row. Just a description of a
 * layout that no longer exists, on the one line whose job is telling a player
 * what is behind a door. `check_docs.py` derives figures out of `docs/` and
 * `README.md` and has never read a header.
 *
 * It names no count on purpose. "AND THREE MORE SHEETS" would be a fifth copy of
 * `SETTINGS_PAGE_COUNT` in a player-facing string, which is the drift this tree
 * spends most of its history on. It avoids the word SHEETS for a sharper reason:
 * `test_the_sheets_spell_the_tuning_they_quote` reads every pause detail that
 * says SHEETS as a claim about `MANUAL_PAGE_COUNT` and requires exactly one row
 * to make it. The first rewrite of this line said "THE SHEETS BEHIND IT" and
 * failed that check — which is the check being right: two rows saying SHEETS on
 * one sheet, meaning two different things, is how a count comes to be read off
 * the wrong one.
 */
#define PAUSE_ROWS(ROW)                                                       \
    ROW(RESUME, "RESUME", "BACK TO THE SECTOR")                               \
    ROW(SETTINGS, "OPTIONS", "SOUND, DISPLAY AND EVERYTHING BEHIND THEM")     \
    ROW(MANUAL, "FIELD MANUAL", "THE TEN SHEETS, WITHOUT LEAVING THE RUN")    \
    ROW(ABANDON, "ABANDON RUN", "GIVE UP THIS RUN AND RETURN TO THE TITLE")

/*
 * What the pause menu offers, in the order it lists them, generated from the
 * same list the words are — so the enum, the table and the count cannot come to
 * disagree about how many rows there are. That is the failure the manual's
 * mission sheet had when its loop counted to a literal.
 *
 * It sits beside the rows rather than in game.h, where it used to be, for the
 * reason `SettingRow`'s does: the suite links no SDL, so a check that measures
 * the table has to be able to reach the count as well.
 */
#define PAUSE_ROW_ENUM(name, label, detail) PAUSE_ITEM_##name,
typedef enum
{
    PAUSE_ROWS(PAUSE_ROW_ENUM)
    PAUSE_ITEM_COUNT
} PauseItem;
#undef PAUSE_ROW_ENUM

typedef struct
{
    /* Two glyph sizes, because a row is a name with a sentence under it. */
    const char *label;
    const char *detail;
} PauseRow;

/* Indexed by `PauseItem` above. */
extern const PauseRow PAUSE_SHEET_ROWS[];
int pause_sheet_row_count(void);

/*
 * The plate, and everything laid out on it. The renderer reads these and
 * `test_every_word_on_the_pause_sheet_fits_the_plate` measures every string in
 * the table against them, so the two cannot disagree about where a line ends.
 *
 * `PAUSE_LABEL_X` is the title's and the strap's own indent; a row's text is
 * pushed past the cursor caret to `PAUSE_ROW_TEXT_X`. The footer prompt starts
 * back at the title's indent because it belongs to the plate rather than to a
 * row.
 */
#define PAUSE_PANEL_W 420.0f
#define PAUSE_LABEL_X 22.0f
#define PAUSE_ROW_TEXT_X 40.0f
#define PAUSE_ROWS_TOP 78.0f
#define PAUSE_ROW_H 44.0f
#define PAUSE_ROW_FOOT 40.0f
#define PAUSE_GLYPH_W 8.0f
/* A label is drawn at scale 2 and its detail at scale 1, which is what makes
 * the row read as a name with a sentence under it rather than as two lines. */
#define PAUSE_LABEL_SCALE 2.0f
#define PAUSE_DETAIL_SCALE 1.0f

/* The title above the rows and the line under it. Here rather than inline for
 * the reason the rows are: they are drawn on the same plate and are just as
 * able to run off it. */
#define PAUSE_TITLE "PAUSED"
#define PAUSE_STRAP "THE BUILDING WAITS."

/*
 * What the ABANDON row's detail says once it is armed, and why it is armed at
 * all.
 *
 * **The row used to act on one press, and two shortcuts reached it without
 * touching the row.** `Q` on the keyboard and `SELECT` on the pad both called
 * `game_return_to_intro` outright from the pause sheet — and `Q` is the default
 * `BIND_WEAPON_NEXT`, the key a hand cycling weapons is already trained to hit.
 * Pause, reach for Q out of habit, and the night was over: no confirmation, and
 * nothing on this sheet or its footer naming the key.
 *
 * The sheet already argued against exactly this. `game_toggle_pause` opens the
 * cursor on RESUME because "the one item on this list that cannot be undone must
 * never be the one sitting under the thumb", and `back_out_with_gamepad` says
 * dropping a run on one press of the button players use to say "not that" is a
 * bug "wearing a hat". Both were true and neither covered the shortcut beside
 * them. Meanwhile `SETTING_RECORDS_RESET` — which throws away records a player
 * can rebuild, rather than the run they are in — has been arm-then-confirm since
 * it existed.
 *
 * So this row is armed the same way, and it is the *row* that arms: `Q` and
 * `SELECT` now put the cursor on it and arm it rather than acting, which makes
 * the shortcut a way of *reaching* the decision instead of a second way of
 * making it. Moving the cursor off disarms, because moving away is what changing
 * your mind looks like.
 *
 * Measured with the other three by
 * `test_every_word_on_the_pause_sheet_fits_the_plate`, because a warning that
 * runs off the plate is a warning the player does not read.
 */
#define PAUSE_ABANDON_ARMED "PRESS AGAIN TO GIVE UP THIS RUN"

/*
 * The footer, in both alphabets.
 *
 * Two forms of one line, spelled for whatever is in the player's hands — see
 * `pad_hint`. Both are measured, because the pad form expands: `$A` and `$B`
 * become up to two glyphs each on a PlayStation pad, so the longer of the two
 * is not always the one that looks longer here.
 */
#define PAUSE_HINT_PAD "LS/DPAD: SELECT   $A: CHOOSE   $B: RESUME"
#define PAUSE_HINT_KEYS "ARROWS: SELECT   ENTER: CHOOSE   ESC: RESUME"

#endif /* CHUCK_PAUSE_SHEET_H */
