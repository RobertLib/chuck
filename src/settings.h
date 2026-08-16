#ifndef CHUCK_SETTINGS_H
#define CHUCK_SETTINGS_H

#include "keybind.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Everything the player is allowed to decide about the game, in one struct and
 * one table.
 *
 * The struct is the whole of it, and the table below is the whole of how it is
 * presented: a row names a value, says one sentence about it, and says whether
 * it is a level or a switch. A setting that exists in the struct and not in the
 * table is a setting nobody can reach; a row naming a value the struct does not
 * hold will not compile. That is the point of writing it this way — the sheet
 * the player reads and the state the game keeps cannot drift apart.
 *
 * This module links no SDL, for the same reason [crew.c](crew.c) does not: it
 * is a model and a table of strings, so the test suite can hold it to the round
 * trip through a file and to the rules the cursor obeys. The file itself is
 * read and written by the shell, which is the only part that knows where a
 * preferences directory is.
 */

/* Optional help, chosen by the player and free to leave off. The options are
 * shell state: they survive campaign resets and are handed to the gameplay
 * core as plain numbers at level load, so the simulation stays deterministic
 * and knows nothing about menus. */
typedef struct
{
    bool more_hearts;    /* 5 hearts per life instead of 3 */
    bool slower_guards;  /* guards and dogs move at 80% speed */
    bool infinite_lives; /* a death never costs a life */
} AssistOptions;

typedef struct
{
    /* Percent, 0 to 100, because that is what the sheet prints and a level the
     * player reads as 70 must be the number that was saved. The audio system
     * takes them as a 0..1 scale on top of its own mix. */
    int music_volume;
    int sfx_volume;
    bool fullscreen;
    /* Scanlines and the vignette: the finishing pass at the bottom of
     * `game_render`. It is the game's look and it is on by default, but it is
     * also a filter over everything, so it is the player's to switch off. */
    bool crt_filter;
    /*
     * Screen shake off, and every warning light held steady.
     *
     * The CRT filter was already the player's to switch off and these two were
     * not, which left the sheet answering the smaller half of the question: a
     * blast shakes the frame, an alarm switch blinks at 5Hz, a mine's fuse at
     * 24, and the cordon the drive is threaded through strobes red and blue
     * for a minute at a time. None of that is decoration the game can be
     * played without seeing — the lights are how a sector reports its own
     * state — so this dims the strobe to a steady glow rather than removing
     * it, and takes the shake out entirely. What is on screen still says
     * exactly what it said before; it stops moving while it says it.
     */
    bool reduced_motion;
    /* Which key does what. See [keybind.h](keybind.h): the nine sector
     * controls, two slots each, and the reason ESC, ENTER and BACKSPACE are
     * not among the keys that may be put in them. */
    KeyBindings bindings;
    AssistOptions assist;
} Settings;

/* What a row does when the player pushes at it. */
typedef enum
{
    SETTING_ROW_HEADING, /* a section rule; the cursor steps over it */
    SETTING_ROW_SLIDER,  /* a level, moved in SETTING_VOLUME_STEP notches */
    SETTING_ROW_TOGGLE,  /* two states, so every change input flips it */
    /*
     * One control, and the two keys it answers to.
     *
     * It is its own kind because it is the one row the change inputs do not
     * change: left and right walk between the row's two slots and confirm
     * captures the next key pressed into whichever one is under the caret. A
     * slider is adjusted, a toggle is flipped, a binding is *taken*, and
     * folding the third into either of the first two is how a sheet ends up
     * cycling a player's jump key alphabetically.
     */
    SETTING_ROW_BINDING,
    /* A row that just does something when confirmed; the controls reset. */
    SETTING_ROW_ACTION
} SettingRowKind;

/* Which field of `Settings` a row drives. */
typedef enum
{
    SETTING_NONE = 0,
    SETTING_MUSIC_VOLUME,
    SETTING_SFX_VOLUME,
    SETTING_FULLSCREEN,
    SETTING_CRT_FILTER,
    SETTING_REDUCED_MOTION,
    SETTING_MORE_HEARTS,
    SETTING_SLOWER_GUARDS,
    SETTING_INFINITE_LIVES,
    /* The row on the main page that opens the controls page. It changes no
     * value of its own, which is why it has an id at all: the cursor test
     * refuses a reachable row whose id is SETTING_NONE. */
    SETTING_OPEN_CONTROLS,
    /* The bindings, one row per action, and the row that puts them all back.
     * `SETTING_BIND_FIRST + action` is the id of an action's row, which is
     * what keeps nine near-identical enum values out of this list. */
    SETTING_BINDINGS_RESET,
    SETTING_BIND_FIRST
} SettingId;

/* The action a binding row drives, or `BIND_COUNT` for a row that is not
 * one. */
BindAction settings_row_action(SettingId id);

/*
 * The sheet has two pages, and that is a layout fact rather than a taste.
 *
 * The plate is sized from its rows, and the main page already stands 516px tall
 * inside a 552px frame. Nine controls, each with two keys on it, do not go in
 * the 36px that are left — so they are a page of their own, reached from a row
 * on the first, the way the manual and the options sheet are siblings hanging
 * off the title screen rather than one scrolling list.
 */
typedef enum
{
    SETTINGS_PAGE_MAIN,
    SETTINGS_PAGE_CONTROLS,
    SETTINGS_PAGE_COUNT
} SettingsPage;

typedef struct
{
    SettingRowKind kind;
    SettingId id;
    const char *label;
    /* One line under the label saying what the switch actually costs or buys.
     * A heading may carry one too, and the ASSIST heading does, because the
     * sentence belongs to the section rather than to any one of its rows. */
    const char *detail;
} SettingRow;

/* One notch of a volume slider, in percent. Ten notches across the bar is
 * coarse enough to walk end to end in a second and fine enough to sit a score
 * under a conversation. */
#define SETTING_VOLUME_STEP 10

/*
 * The sheet's plate, and the run of caps a binding row draws on it.
 *
 * Here rather than inside the renderer for the reason every table of words in
 * this tree keeps its geometry beside it: a row that outgrows its column is
 * silent, and the only sheet in the game that has already lost a line to that
 * is the manual's control page. The renderer lays the caps out from these and
 * `test_a_binding_row_fits_the_plate` measures the widest row against them, so
 * a fifth cap or a longer key name fails the build instead of quietly drawing
 * over the label beside it.
 *
 * A cap is `NAME_MAX` glyphs of the 8px font plus `SETTINGS_CAP_PAD` of air,
 * and the two halves of a row are sized separately because they spell
 * themselves in different alphabets — see the note in `draw_setting_keys`.
 */
#define SETTINGS_PANEL_W 512.0f
#define SETTINGS_LABEL_X 22.0f
/* A row's own label and detail are indented past the cursor gutter; a
 * heading's and the strap's are not. */
#define SETTINGS_ROW_TEXT_X 34.0f
#define SETTINGS_CONTROL_INSET 28.0f
#define SETTINGS_CAP_PAD 14.0f
#define SETTINGS_CAP_GAP 8.0f
#define SETTINGS_CAP_GROUP_GAP 14.0f
#define SETTINGS_GLYPH_W 8.0f

/*
 * The three lines the sheet draws that are not rows of the table.
 *
 * Two of them tell an armed cap what it is waiting for, and they have to be
 * two: a key cap takes a keyboard press and cancels on ESC, a pad cap takes a
 * button and cancels on START, and one line covering both would name the wrong
 * escape half the time. The third is the mute warning, which is a correction
 * to the audio heading rather than a description of it.
 *
 * Here rather than inline in the renderer so the fit test can reach them: they
 * are drawn on the same plate as everything else and are just as capable of
 * running off it.
 */
#define SETTINGS_CAPTURE_KEY_LINE \
    "PRESS THE KEY TO BIND, OR ESC TO LEAVE IT ALONE"
#define SETTINGS_CAPTURE_PAD_LINE \
    "PRESS THE BUTTON TO BIND, OR START TO LEAVE IT ALONE"
#define SETTINGS_MUTED_LINE \
    "MUTED BY M - CLOSE THIS SHEET AND PRESS M TO HEAR IT."

/* One page of the sheet, in order. */
const SettingRow *settings_rows(SettingsPage page, int *out_count);

/* What the page is titled and what its strap line says. */
const char *settings_page_title(SettingsPage page);
const char *settings_page_strap(SettingsPage page);

/*
 * True for the heading that governs the sheet's audio levels: the first one
 * whose own rows are sliders.
 *
 * Asked of the table rather than of the label, so a rename cannot silently
 * break it. It exists for one line — while the kill switch is down that
 * heading has to say so, because this is the one screen the mix is read off
 * and two levels reading 100 over a silent game is the contradiction the sheet
 * is there to prevent.
 *
 * **It lives here rather than in the renderer that draws the line**, and that
 * is the same rule the word tables keep: it is a question about this table, so
 * it belongs beside this table where `make test` can hold it. Inside
 * [game_render.c](game_render.c) it was reachable only with the mute on, which
 * no run in the tree ever is — so a rule about the one screen the mix is read
 * off was checked by nothing at all. See
 * `test_the_audio_heading_is_found_by_what_it_holds`.
 */
bool settings_heading_governs_levels(const SettingRow *rows, int row_count,
                                     int index);

void settings_defaults(Settings *settings);

/* The first row the cursor may stand on, and the walk between them. Both skip
 * headings, and the walk wraps: a list this short is quicker to reach the
 * bottom of by going up. */
int settings_first_row(SettingsPage page);
int settings_move_cursor(SettingsPage page, int cursor, int delta);

/* Push at the selected row. `delta` is -1 or +1 for a slider; a toggle has two
 * states, so "less" and "more" are the same move and it flips either way.
 * Returns true when the value actually changed, so the shell knows whether
 * anything happened — a slider already at either end is a no-op, and a no-op
 * must not click. */
bool settings_adjust(Settings *settings, SettingId id, int delta);

int settings_value_percent(const Settings *settings, SettingId id);
bool settings_value_bool(const Settings *settings, SettingId id);

/* The file, as text. `settings_serialize` writes at most `cap` bytes including
 * the terminator and returns the length written. `settings_parse` reads what it
 * recognises and leaves every other field alone, so a file from an older build
 * — or one with a line hand-edited into nonsense — loads as the defaults it
 * does not override rather than as a reset. */
size_t settings_serialize(const Settings *settings, char *out, size_t cap);
void settings_parse(Settings *settings, const char *text);

#endif /* CHUCK_SETTINGS_H */
