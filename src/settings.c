#include "settings.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The sheet. Three sections, in the order a player looks for them: what they
 * can hear, what they are looking at, and what the game will do for them if
 * they ask. Assist is last and keeps its own sentence, because the three
 * switches under it are the only ones that change what the game is rather than
 * how it is delivered.
 */
static const SettingRow ROWS[] = {
    {SETTING_ROW_HEADING, SETTING_NONE, "AUDIO", NULL},
    {SETTING_ROW_SLIDER, SETTING_MUSIC_VOLUME, "MUSIC",
     "ONE SCORE PER SECTOR"},
    {SETTING_ROW_SLIDER, SETTING_SFX_VOLUME, "SOUND EFFECTS",
     "SHOTS, STEPS, DOORS AND THE CREW'S NET"},

    {SETTING_ROW_HEADING, SETTING_NONE, "DISPLAY", NULL},
    {SETTING_ROW_TOGGLE, SETTING_FULLSCREEN, "FULLSCREEN",
     "ALSO ON F, ANYWHERE IN THE GAME"},
    {SETTING_ROW_TOGGLE, SETTING_CRT_FILTER, "CRT FILTER",
     "SCANLINES AND VIGNETTE OVER THE PICTURE"},
    {SETTING_ROW_TOGGLE, SETTING_REDUCED_MOTION, "REDUCED MOTION",
     "NO SCREEN SHAKE; WARNING LIGHTS GLOW INSTEAD OF STROBING"},
    {SETTING_ROW_ACTION, SETTING_OPEN_CONTROLS, "CONTROLS",
     "WHICH KEY AND BUTTON DOES WHAT, ON ITS OWN SHEET"},

    {SETTING_ROW_HEADING, SETTING_NONE, "ASSIST",
     "THE GAME IS TUNED WITHOUT THESE. TAKE WHAT HELPS."},
    {SETTING_ROW_TOGGLE, SETTING_MORE_HEARTS, "MORE HEARTS",
     "FIVE HEARTS PER LIFE INSTEAD OF THREE"},
    {SETTING_ROW_TOGGLE, SETTING_SLOWER_GUARDS, "SLOWER GUARDS",
     "GUARDS AND DOGS MOVE AT 80% SPEED"},
    {SETTING_ROW_TOGGLE, SETTING_INFINITE_LIVES, "INFINITE LIVES",
     "A DEATH NEVER COSTS A LIFE"},
};

/*
 * The second page: the nine sector controls and the row that puts them back.
 *
 * A binding row carries no detail line of its own, and that is the one place
 * this table departs from "every row explains itself". The label *is* the
 * explanation — MOVE LEFT says the whole of what the row does — and nine
 * sentences repeating it would push the page past the frame for no reader's
 * benefit. What the section needs saying is said once, on the heading.
 */
static const SettingRow CONTROL_ROWS[] = {
    {SETTING_ROW_HEADING, SETTING_NONE, "ON FOOT",
     "LEFT/RIGHT PICKS A CAP: TWO KEYS, THEN TWO PAD BUTTONS."},
#define CHUCK_BIND_SHEET_ROW(ident, label, key)                              \
    {SETTING_ROW_BINDING, (SettingId)(SETTING_BIND_FIRST + BIND_##ident),    \
     label, NULL},
    CHUCK_BIND_LIST(CHUCK_BIND_SHEET_ROW)
#undef CHUCK_BIND_SHEET_ROW
    {SETTING_ROW_ACTION, SETTING_BINDINGS_RESET, "RESET CONTROLS",
     "PUT THE KEYBOARD AND THE PAD BACK TO WHAT THEY WERE"},
};

#define ROW_COUNT ((int)(sizeof(ROWS) / sizeof(ROWS[0])))
#define CONTROL_ROW_COUNT                                                     \
    ((int)(sizeof(CONTROL_ROWS) / sizeof(CONTROL_ROWS[0])))

const SettingRow *settings_rows(SettingsPage page, int *out_count)
{
    if (page == SETTINGS_PAGE_CONTROLS)
    {
        if (out_count != NULL)
            *out_count = CONTROL_ROW_COUNT;
        return CONTROL_ROWS;
    }
    if (out_count != NULL)
        *out_count = ROW_COUNT;
    return ROWS;
}

const char *settings_page_title(SettingsPage page)
{
    return page == SETTINGS_PAGE_CONTROLS ? "CONTROLS" : "OPTIONS";
}

const char *settings_page_strap(SettingsPage page)
{
    return page == SETTINGS_PAGE_CONTROLS
               ? "ENTER, THEN THE NEW KEY OR BUTTON. ESC AND START CANCEL."
               : "EVERY CHANGE IS KEPT WHEN THIS SHEET IS CLOSED.";
}

bool settings_heading_governs_levels(const SettingRow *rows, int row_count,
                                     int index)
{
    if (rows == NULL || index < 0 || index >= row_count ||
        rows[index].kind != SETTING_ROW_HEADING)
    {
        return false;
    }
    for (int i = index + 1; i < row_count; ++i)
    {
        if (rows[i].kind == SETTING_ROW_HEADING)
            return false; /* the section ended without one */
        if (rows[i].kind == SETTING_ROW_SLIDER)
            return true;
    }
    return false;
}

BindAction settings_row_action(SettingId id)
{
    if (id < SETTING_BIND_FIRST)
        return BIND_COUNT;
    int action = (int)id - (int)SETTING_BIND_FIRST;
    return action < BIND_COUNT ? (BindAction)action : BIND_COUNT;
}

void settings_defaults(Settings *settings)
{
    /* Both levels start at the top, so a fresh install sounds exactly like the
     * mix the effects and the scores were written against: the sliders scale
     * that mix down and never up. */
    settings->music_volume = 100;
    settings->sfx_volume = 100;
    settings->fullscreen = false;
    settings->crt_filter = true;
    /* Off, because the shake and the strobes are how the game was written and
     * a comfort switch that is on by default is not a choice anybody made. */
    settings->reduced_motion = false;
    keybind_defaults(&settings->bindings);
    settings->assist.more_hearts = false;
    settings->assist.slower_guards = false;
    settings->assist.infinite_lives = false;
}

int settings_first_row(SettingsPage page)
{
    int count = 0;
    const SettingRow *rows = settings_rows(page, &count);
    for (int i = 0; i < count; ++i)
    {
        if (rows[i].kind != SETTING_ROW_HEADING)
            return i;
    }
    return 0;
}

int settings_move_cursor(SettingsPage page, int cursor, int delta)
{
    if (delta == 0)
        return cursor;

    int count = 0;
    const SettingRow *rows = settings_rows(page, &count);
    if (count <= 0)
        return cursor;

    int step = delta > 0 ? 1 : -1;
    int at = cursor;
    /* Walk at most one full lap. A sheet of nothing but headings would
     * otherwise spin here forever, and a table is an easy thing to edit into
     * that state by accident. */
    for (int guard = 0; guard < count; ++guard)
    {
        at = (at + step + count) % count;
        if (rows[at].kind != SETTING_ROW_HEADING)
            return at;
    }
    return cursor;
}

static int clamp_percent(int value)
{
    if (value < 0)
        return 0;
    if (value > 100)
        return 100;
    return value;
}

static bool step_volume(int *value, int delta)
{
    int next = clamp_percent(*value + delta * SETTING_VOLUME_STEP);
    if (next == *value)
        return false;
    *value = next;
    return true;
}

static bool flip(bool *value)
{
    *value = !*value;
    return true;
}

bool settings_adjust(Settings *settings, SettingId id, int delta)
{
    switch (id)
    {
    case SETTING_MUSIC_VOLUME:
        return step_volume(&settings->music_volume, delta);
    case SETTING_SFX_VOLUME:
        return step_volume(&settings->sfx_volume, delta);
    case SETTING_FULLSCREEN:
        return flip(&settings->fullscreen);
    case SETTING_CRT_FILTER:
        return flip(&settings->crt_filter);
    case SETTING_REDUCED_MOTION:
        return flip(&settings->reduced_motion);
    case SETTING_MORE_HEARTS:
        return flip(&settings->assist.more_hearts);
    case SETTING_SLOWER_GUARDS:
        return flip(&settings->assist.slower_guards);
    case SETTING_INFINITE_LIVES:
        return flip(&settings->assist.infinite_lives);
    /* None of these hold a value that "less" and "more" mean anything to. A
     * binding is taken rather than adjusted — left and right walk its two
     * slots, which is the shell's cursor and not a value at all — and the
     * other
     * two are pressed. Returning false is what keeps the change inputs from
     * clicking at a row nothing happened to. */
    case SETTING_OPEN_CONTROLS:
    case SETTING_BINDINGS_RESET:
    case SETTING_BIND_FIRST:
    case SETTING_NONE:
        break;
    }
    return false;
}

int settings_value_percent(const Settings *settings, SettingId id)
{
    switch (id)
    {
    case SETTING_MUSIC_VOLUME:
        return settings->music_volume;
    case SETTING_SFX_VOLUME:
        return settings->sfx_volume;
    default:
        return 0;
    }
}

bool settings_value_bool(const Settings *settings, SettingId id)
{
    switch (id)
    {
    case SETTING_FULLSCREEN:
        return settings->fullscreen;
    case SETTING_CRT_FILTER:
        return settings->crt_filter;
    case SETTING_REDUCED_MOTION:
        return settings->reduced_motion;
    case SETTING_MORE_HEARTS:
        return settings->assist.more_hearts;
    case SETTING_SLOWER_GUARDS:
        return settings->assist.slower_guards;
    case SETTING_INFINITE_LIVES:
        return settings->assist.infinite_lives;
    default:
        return false;
    }
}

/* ---- The file ------------------------------------------------------- */

/*
 * `key value` a line, because the one thing worth more than a compact format
 * here is that a player who opens the file can see what is in it and fix it.
 * The keys are spelled out once, in the writer and the reader together, so a
 * renamed field cannot be written under one name and looked for under another.
 */
#define KEY_MUSIC "music"
#define KEY_SFX "sfx"
#define KEY_FULLSCREEN "fullscreen"
#define KEY_CRT "crt"
#define KEY_MOTION "reduced_motion"
#define KEY_HEARTS "assist_hearts"
#define KEY_GUARDS "assist_guards"
#define KEY_LIVES "assist_lives"

size_t settings_serialize(const Settings *settings, char *out, size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;

    int written = snprintf(
        out, cap,
        "# Chuck settings. Delete this file to go back to the defaults.\n"
        KEY_MUSIC " %d\n"
        KEY_SFX " %d\n"
        KEY_FULLSCREEN " %d\n"
        KEY_CRT " %d\n"
        KEY_MOTION " %d\n"
        KEY_HEARTS " %d\n"
        KEY_GUARDS " %d\n"
        KEY_LIVES " %d\n",
        clamp_percent(settings->music_volume),
        clamp_percent(settings->sfx_volume),
        settings->fullscreen ? 1 : 0,
        settings->crt_filter ? 1 : 0,
        settings->reduced_motion ? 1 : 0,
        settings->assist.more_hearts ? 1 : 0,
        settings->assist.slower_guards ? 1 : 0,
        settings->assist.infinite_lives ? 1 : 0);

    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return cap - 1;

    /*
     * The bindings, by name rather than by number, one line an action: a
     * scancode in a text file is a number nobody can check and a key nobody can
     * fix by hand, and the whole reason this file is `key value` lines is that
     * a player who opens it can see what is in it. An empty slot is written as
     * `-`, so a line always has both fields and a missing one is damage rather
     * than a shorthand.
     */
    size_t at = (size_t)written;
    for (int action = 0; action < BIND_COUNT; ++action)
    {
        const int *slots = settings->bindings.keys[action];
        const char *first = keybind_key_name(slots[0]);
        const char *second = keybind_key_name(slots[1]);
        int line = snprintf(out + at, cap - at, "%s %s %s\n",
                            keybind_action_file_key((BindAction)action),
                            first[0] != '\0' ? first : "-",
                            second[0] != '\0' ? second : "-");
        if (line < 0 || (size_t)line >= cap - at)
        {
            /* Out of room: what is already written is a valid file, so it is
             * kept and terminated rather than thrown away. A truncated
             * settings file must still load. */
            out[at] = '\0';
            return at;
        }
        at += (size_t)line;
    }

    /*
     * The pad, on lines of its own rather than as two more fields on the
     * keyboard's.
     *
     * Separate lines mean a settings file written by the build before pads
     * were bindable still loads exactly as it did — its nine `bind_*` rows
     * apply and the pad keeps its defaults — and a file written by this build
     * loads on that one, which ignores keys it does not know. Sharing a line
     * would have made the pad half a change to the *format* of a line that
     * already exists, and an older build reading four names where it expects
     * two treats the whole row as damage and drops a binding the player set.
     *
     * Positional names, never letters: see the note on `keybind_pad_face_index`
     * for why a file that said `A` would be a file that means a different
     * button on a Switch pad than on an Xbox one.
     */
    for (int action = 0; action < BIND_COUNT; ++action)
    {
        const int *slots = settings->bindings.pad[action];
        const char *first = keybind_pad_file_name(slots[0]);
        const char *second = keybind_pad_file_name(slots[1]);
        int line = snprintf(out + at, cap - at, "%s %s %s\n",
                            keybind_action_pad_file_key((BindAction)action),
                            first[0] != '\0' ? first : "-",
                            second[0] != '\0' ? second : "-");
        if (line < 0 || (size_t)line >= cap - at)
        {
            out[at] = '\0';
            return at;
        }
        at += (size_t)line;
    }
    return at;
}

/*
 * One binding line's value: `NAME NAME`, either of which may be `-`.
 *
 * Both slots are read before either is written, for the reason every other
 * reader in this file leaves a field alone when it cannot parse it: a line with
 * one good name and one this build has never heard of is damage, and half
 * applying it would leave the action holding a key the player never chose
 * beside one they did.
 */
static bool read_binding(const char *value, int *first, int *second)
{
    int slot[BIND_SLOTS] = {KEYBIND_NONE, KEYBIND_NONE};
    for (int i = 0; i < BIND_SLOTS; ++i)
    {
        while (*value == ' ' || *value == '\t')
            ++value;
        const char *start = value;
        while (*value != '\0' && *value != ' ' && *value != '\t')
            ++value;
        size_t len = (size_t)(value - start);
        if (len == 0)
            return false;
        if (len == 1 && start[0] == '-')
            continue;
        slot[i] = keybind_key_from_name(start, len);
        if (slot[i] == KEYBIND_NONE)
            return false;
    }
    *first = slot[0];
    *second = slot[1];
    return true;
}

/* The pad's line, read exactly as the keyboard's is above and refused whole
 * for the same reason: half a row applied is an action holding a button the
 * player never chose beside one they did. */
static bool read_pad_binding(const char *value, int *first, int *second)
{
    int slot[BIND_SLOTS] = {PADBIND_NONE, PADBIND_NONE};
    for (int i = 0; i < BIND_SLOTS; ++i)
    {
        while (*value == ' ' || *value == '\t')
            ++value;
        const char *start = value;
        while (*value != '\0' && *value != ' ' && *value != '\t')
            ++value;
        size_t len = (size_t)(value - start);
        if (len == 0)
            return false;
        if (len == 1 && start[0] == '-')
            continue;
        slot[i] = keybind_pad_from_file_name(start, len);
        if (slot[i] == PADBIND_NONE)
            return false;
    }
    *first = slot[0];
    *second = slot[1];
    return true;
}

/* True when `line` opens with `key` followed by a separator, and hands back
 * whatever follows it. Requiring the separator is what stops `music` from
 * matching a future `music_device`. */
static bool line_key_is(const char *line, const char *key, const char **value)
{
    size_t n = strlen(key);
    if (strncmp(line, key, n) != 0)
        return false;
    const char *rest = line + n;
    if (*rest != ' ' && *rest != '\t' && *rest != '=')
        return false;
    while (*rest == ' ' || *rest == '\t' || *rest == '=')
        ++rest;
    *value = rest;
    return true;
}

/*
 * A number, and whether there was one at all.
 *
 * `strtol` reads "no digits" as zero, which would make a half-written line the
 * one kind of damage that does change a setting: `crt ` left behind by an
 * interrupted write reads as a nought and turns the filter off. A key this
 * build recognises with a value it cannot read is treated exactly as a key it
 * does not recognise — the setting keeps whatever it already had.
 */
static bool read_int(const char *value, int *out)
{
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value)
        return false;
    /* Clamped as a long before the cast, because a hand-edited 99999999999 is
     * out of range for an int and the conversion itself would be undefined. */
    if (parsed > INT_MAX)
        parsed = INT_MAX;
    if (parsed < INT_MIN)
        parsed = INT_MIN;
    *out = (int)parsed;
    return true;
}

static void apply_line(Settings *settings, const char *line)
{
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0' || *line == '#')
        return;

    const char *value = NULL;
    int number = 0;

    /* The bindings first and on their own, because they are the one kind of
     * line whose value is not a number. Matching one ends the line: the chain
     * below is a single `else if` ladder precisely so that no line can ever be
     * applied twice, and a `for` cannot be a rung of it. */
    for (int action = 0; action < BIND_COUNT; ++action)
    {
        /* The pad row is tried first, and it has to be: `line_key_is` matches
         * on a prefix, and while `pad_bind_left` cannot be mistaken for
         * `bind_left` in this direction, testing the shorter key first is the
         * habit that makes the next pair of keys a silent bug. */
        if (line_key_is(line, keybind_action_pad_file_key((BindAction)action),
                        &value))
        {
            int first = PADBIND_NONE;
            int second = PADBIND_NONE;
            if (read_pad_binding(value, &first, &second))
            {
                settings->bindings.pad[action][0] = first;
                settings->bindings.pad[action][1] = second;
            }
            return;
        }
        if (!line_key_is(line, keybind_action_file_key((BindAction)action),
                         &value))
        {
            continue;
        }
        int first = KEYBIND_NONE;
        int second = KEYBIND_NONE;
        if (read_binding(value, &first, &second))
        {
            settings->bindings.keys[action][0] = first;
            settings->bindings.keys[action][1] = second;
        }
        return;
    }

    if (line_key_is(line, KEY_MUSIC, &value))
    {
        if (read_int(value, &number))
            settings->music_volume = clamp_percent(number);
    }
    else if (line_key_is(line, KEY_SFX, &value))
    {
        if (read_int(value, &number))
            settings->sfx_volume = clamp_percent(number);
    }
    else if (line_key_is(line, KEY_FULLSCREEN, &value))
    {
        if (read_int(value, &number))
            settings->fullscreen = number != 0;
    }
    else if (line_key_is(line, KEY_CRT, &value))
    {
        if (read_int(value, &number))
            settings->crt_filter = number != 0;
    }
    else if (line_key_is(line, KEY_MOTION, &value))
    {
        if (read_int(value, &number))
            settings->reduced_motion = number != 0;
    }
    else if (line_key_is(line, KEY_HEARTS, &value))
    {
        if (read_int(value, &number))
            settings->assist.more_hearts = number != 0;
    }
    else if (line_key_is(line, KEY_GUARDS, &value))
    {
        if (read_int(value, &number))
            settings->assist.slower_guards = number != 0;
    }
    else if (line_key_is(line, KEY_LIVES, &value))
    {
        if (read_int(value, &number))
            settings->assist.infinite_lives = number != 0;
    }
    /* Anything else is a line from a build that knew something this one does
     * not. Ignoring it is deliberate: a settings file must never be the reason
     * a game refuses to start. */
}

void settings_parse(Settings *settings, const char *text)
{
    if (text == NULL)
        return;

    /* Zeroed on the way in only to keep the static analyser from reading the
     * scan below as working on an uninitialised buffer; every line is
     * terminated before it is handed on regardless. */
    char line[128] = {0};
    size_t len = 0;
    for (const char *at = text;; ++at)
    {
        if (*at == '\n' || *at == '\r' || *at == '\0')
        {
            line[len] = '\0';
            if (len > 0)
                apply_line(settings, line);
            len = 0;
            if (*at == '\0')
                return;
            continue;
        }
        /* A line longer than the buffer is truncated rather than run off the
         * end of it; what survives either parses or is ignored. */
        if (len + 1 < sizeof(line))
            line[len++] = *at;
    }
}
