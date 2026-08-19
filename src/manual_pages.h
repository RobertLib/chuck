#ifndef CHUCK_MANUAL_PAGES_H
#define CHUCK_MANUAL_PAGES_H

/*
 * What the field manual says, apart from how it is drawn.
 *
 * The sheets are a table — a title, a strap, a caption and a list of typed
 * lines — and the line kinds are the whole layout language. That much needs no
 * SDL, so it lives here rather than in [manual.c](manual.c), for the same
 * reason [credits.c](credits.c) and [crew.c](crew.c) are split out: the test
 * suite links no SDL, and a page that outgrows its column has to fail a build
 * rather than lose its last line in silence.
 *
 * It used to be one file, and the cost was exactly that. `render_text_column`
 * stops at `MANUAL_BODY_BOTTOM` instead of drawing past it, which is right for
 * a frame and silent for whoever is writing the sheet: `CONTROLS` spent a long
 * time over the line, and what fell off the bottom was the only line in the
 * game that named the key which closes it. The check for it was a
 * `CHUCK_DEBUG` assert, so it only ever ran for whoever happened to open the
 * book in a debug build — and the only reason it could not be a `make test`
 * check the way the credits' own fit test is was that the page struct
 * carried a pointer to an SDL draw function. That pointer is the one thing
 * that stayed behind: [manual.c](manual.c) keeps a parallel array of
 * illustrations, indexed the same way and length-checked against this one.
 *
 * Everything in `MANUAL_PAGES` is presentation and none of it is read by the
 * simulation.
 */

#include <stdbool.h>

typedef enum
{
    LINE_HEAD,
    LINE_BODY,
    LINE_BULLET,
    LINE_KEY, /* "keyboard|gamepad|action" */
    LINE_GAP
} ManualLineKind;

/*
 * A prose line may be written as `pad wording|keyboard wording`, the same
 * bar-separated idiom the control rows already use, and that is how the sheet
 * says "hold $Y" to a pad and "hold E" to a keyboard out of one entry. The
 * control table has two columns and never needed it; the paragraphs around it
 * had no second column at all, so they spelled `E` at everybody — the rule the
 * `$` tokens exist to prevent, told to the one reader with no E to press. A
 * line with no bar in it is printed exactly as written, which is every line
 * that names no button.
 */
typedef struct
{
    ManualLineKind kind;
    const char *text;
} ManualLine;

typedef struct
{
    const char *title;
    const char *strap;
    const char *caption;
    const ManualLine *lines;
    int line_count;
} ManualPageText;

#define MANUAL_PAGE_COUNT 10

/*
 * The one label inside an illustration that states a tuning number.
 *
 * `FIGHTING`'s lower vignette draws a guard's sight cone and prints how far it
 * reaches, and that figure is `ENEMY_VIEW_RANGE` — the number the whole quiet
 * route is played against. It was a literal inside
 * [manual.c](manual.c), on the side of the SDL boundary no test can reach, while
 * the bullet saying the same thing an inch to its left sat in
 * [manual_pages.c](manual_pages.c), where a test could have reached it and none
 * did. So the most important number in the game was written out in words twice
 * and derived from the constant neither time.
 *
 * It is here for the reason every table of words in this tree is here: so the
 * suite can read it. `test_the_sheets_spell_the_tuning_they_quote` requires both
 * copies to spell `ENEMY_VIEW_RANGE / TILE_SIZE`, so retuning the cone fails the
 * build instead of leaving the manual quietly lying about it.
 *
 * The rest of that vignette's labels stay in the renderer, and the difference is
 * the rule rather than an inconsistency: `FROM ABOVE` and `LAND ON HIS HEAD` are
 * captions on a picture, and this one is a claim about the simulation.
 */
#define MANUAL_SIGHT_CONE_LABEL "SEVEN TILES"

/*
 * Declared without its length, so the definition's `_Static_assert` measures the
 * sheaf rather than itself — see the same note on `ED_GROUP_NAMES` in
 * editor/editor_legend.h. A sheet in the enum with no words here would otherwise
 * be a row of null strings, and the count callers want is `MANUAL_PAGE_COUNT`.
 */
extern const ManualPageText MANUAL_PAGES[];

/*
 * Which sectors are climbed rather than walked, 1-based, and how many there are.
 *
 * The `THE MISSION` sheet is the one place in the game the campaign's shape is
 * *drawn* — a tick a sector up the side of the tower, the climbs in amber —
 * and the loop that draws it had the count and the climb numbers written into
 * the renderer. So it kept drawing a fifteen-sector campaign with four climbs
 * after the campaign was seventeen with five, and no check anywhere could have
 * caught it: the fit tests measure words, and this is a picture.
 *
 * It lives here, beside the words that say the same thing on the same sheet, so
 * the strap and the illustration read one list.
 * `test_the_manual_draws_the_campaign_it_ships_with` holds that list against the
 * `MODE FACADE` lines of the maps actually embedded, and the sector count with
 * it. **A new climb owes this array an edit**, and the suite says so by name.
 */
extern const int CAMPAIGN_CLIMB_SECTORS[];
extern const int CAMPAIGN_CLIMB_SECTOR_COUNT;

/*
 * How many sheets the docket has in it: one per interior, and none on a climb.
 *
 * A function beside the array above rather than a `#define`, because the climb
 * count is a table's length and not a macro. It is here because it was about to
 * be arithmetic in two files — [sector_tally.c](sector_tally.c) already derived
 * it for the line the report prints, and [run_tally.c](run_tally.c) needed the
 * same number for the RECORDS page — and "a number written down twice is checked
 * or it is two numbers" is the rule this whole file exists under. Derived rather
 * than written down for the reason the climb list is: a campaign that gains a
 * floor gains a sheet with it.
 */
int campaign_docket_sheets(void);

/*
 * And how much of it exists at or below `sector` (1-based), which is the most a
 * run standing on that floor can be holding.
 *
 * The whole-campaign answer above is this one asked about the last sector, so
 * the two are one function rather than two spellings of `CAMPAIGN_SECTORS`
 * minus the climbs. What wanted the general form is the staged clear the
 * `--screen` cards show: its docket was a flat `SOAK_TALLY_DOCKET` of seven,
 * printed on a report after sector one, where a run can be holding at most a
 * single sheet. See the note on the fixture in
 * [sector_tally.h](sector_tally.h).
 */
int campaign_docket_sheets_by(int sector);

/*
 * The geometry the renderer lays out against and the fit checks measure. Both
 * walk the kinds in the same order and off the same numbers, so they cannot
 * disagree about where a line lands or how wide it is.
 *
 * `MANUAL_CH` is the 8x8 debug font's cell, which every prompt in the game is
 * drawn in; manual.c asserts it against SDL's own constant so the two cannot
 * drift. Type is only ever drawn at scale 1 or a multiple of it, so a line's
 * width is exactly its length in cells — which is what makes the fit a thing a
 * test can prove rather than something to eyeball.
 */
#define MANUAL_CH 8.0f
#define MANUAL_TEXT_X 42.0f
#define MANUAL_TEXT_RIGHT 424.0f
#define MANUAL_BODY_Y 100.0f
#define MANUAL_BODY_BOTTOM 496.0f
#define MANUAL_BULLET_INDENT 12.0f
#define MANUAL_LINE_PITCH 13.0f
#define MANUAL_KEY_PITCH 21.0f
#define MANUAL_HEAD_PITCH 20.0f
#define MANUAL_HEAD_LEAD 9.0f
#define MANUAL_GAP_PITCH 7.0f
/* A heading is letterspaced by a pixel a cell, so it is wider than its length
 * says. */
#define MANUAL_HEAD_TRACK 1.0f

/*
 * How far below its own `y` each kind actually puts ink, which is the question
 * the vertical fit check has to ask.
 *
 * `render_text_column` clips on `y < MANUAL_BODY_BOTTOM`, so what it decides is
 * whether a line *starts* inside the column — and a line that starts a pixel
 * above the bottom is drawn in full, several pixels below it. That is right for
 * the renderer, which needs a cheap backstop, and wrong for a check whose whole
 * job is to prove the sheet lands in the frame: measured by its top, a control
 * row could pass with its keycap sitting in the footer chips, and the sheet
 * that would do it is the one this whole file exists because of.
 *
 * A keycap is the deep one — it is drawn from `y - MANUAL_KEY_CHIP_RISE` and is
 * `MANUAL_CHIP_H` tall — which is why the three are written out separately
 * rather than folded into one number: they come off the draw calls in
 * [manual.c](manual.c), and that file names them from here so the two cannot
 * drift.
 */
#define MANUAL_CHIP_H 18.0f
#define MANUAL_KEY_CHIP_RISE 3.0f
#define MANUAL_HEAD_RULE_Y 11.0f
#define MANUAL_INK_TEXT MANUAL_CH
#define MANUAL_INK_HEAD (MANUAL_HEAD_RULE_Y + 1.0f)
#define MANUAL_INK_KEY (MANUAL_CHIP_H - MANUAL_KEY_CHIP_RISE)
/* The gap between the two chip columns of a control row and the action text
 * that follows them. */
#define MANUAL_KEY_CHIP_PAD 12.0f
#define MANUAL_KEY_CHIP_GAP 8.0f
#define MANUAL_KEY_ACTION_GAP 20.0f
/* A caption is set from the illustration panel's left edge, so it has the
 * panel's width and no more: a line that runs past the plate runs off the
 * sheet. */
#define MANUAL_CAPTION_MAX 38

/* True when every line of `page` reaches the frame at all — the vertical half
 * of the rule, and the one the clipped `CONTROLS` sheet broke. Measured by the
 * ink each kind lays down rather than by where the line starts; see the
 * `MANUAL_INK_*` note above for why those are not the same question. */
bool manual_page_lines_fit(const ManualPageText *page);

/*
 * And the horizontal half, which nothing used to check at all: a line wider
 * than the text column runs off the sheet exactly as silently as one below the
 * bottom of it.
 *
 * A control row's two chip columns are as wide as the widest label on the
 * sheet, and the pad column is spelled for whatever is plugged in — so the
 * widths are handed in rather than computed here, because how a pad spells its
 * buttons is the one part of this that needs a platform. Pass the widest
 * spelling to measure the worst case; pass the sheet's own Xbox lettering to
 * measure what it is written in.
 */
bool manual_page_lines_fit_width(const ManualPageText *page,
                                 float key_column_w, float pad_column_w);

/* The longest of a bar-separated line's wordings, in cells. A line with no bar
 * in it is its own length. */
int manual_line_widest_form(const char *text);

#endif /* CHUCK_MANUAL_PAGES_H */
