#ifndef CHUCK_SETTINGS_H
#define CHUCK_SETTINGS_H

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
    AssistOptions assist;
} Settings;

/* What a row does when the player pushes at it. */
typedef enum
{
    SETTING_ROW_HEADING, /* a section rule; the cursor steps over it */
    SETTING_ROW_SLIDER,  /* a level, moved in SETTING_VOLUME_STEP notches */
    SETTING_ROW_TOGGLE   /* two states, so every change input flips it */
} SettingRowKind;

/* Which field of `Settings` a row drives. */
typedef enum
{
    SETTING_NONE = 0,
    SETTING_MUSIC_VOLUME,
    SETTING_SFX_VOLUME,
    SETTING_FULLSCREEN,
    SETTING_CRT_FILTER,
    SETTING_MORE_HEARTS,
    SETTING_SLOWER_GUARDS,
    SETTING_INFINITE_LIVES
} SettingId;

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

/* The sheet, in order. */
const SettingRow *settings_rows(int *out_count);

void settings_defaults(Settings *settings);

/* The first row the cursor may stand on, and the walk between them. Both skip
 * headings, and the walk wraps: a list this short is quicker to reach the
 * bottom of by going up. */
int settings_first_row(void);
int settings_move_cursor(int cursor, int delta);

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
