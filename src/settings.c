#include "settings.h"

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

    {SETTING_ROW_HEADING, SETTING_NONE, "ASSIST",
     "THE GAME IS TUNED WITHOUT THESE. TAKE WHAT HELPS."},
    {SETTING_ROW_TOGGLE, SETTING_MORE_HEARTS, "MORE HEARTS",
     "FIVE HEARTS PER LIFE INSTEAD OF THREE"},
    {SETTING_ROW_TOGGLE, SETTING_SLOWER_GUARDS, "SLOWER GUARDS",
     "GUARDS AND DOGS MOVE AT 80% SPEED"},
    {SETTING_ROW_TOGGLE, SETTING_INFINITE_LIVES, "INFINITE LIVES",
     "A DEATH NEVER COSTS A LIFE"},
};

#define ROW_COUNT ((int)(sizeof(ROWS) / sizeof(ROWS[0])))

const SettingRow *settings_rows(int *out_count)
{
    if (out_count != NULL)
        *out_count = ROW_COUNT;
    return ROWS;
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
    settings->assist.more_hearts = false;
    settings->assist.slower_guards = false;
    settings->assist.infinite_lives = false;
}

int settings_first_row(void)
{
    for (int i = 0; i < ROW_COUNT; ++i)
    {
        if (ROWS[i].kind != SETTING_ROW_HEADING)
            return i;
    }
    return 0;
}

int settings_move_cursor(int cursor, int delta)
{
    if (delta == 0)
        return cursor;

    int step = delta > 0 ? 1 : -1;
    int at = cursor;
    /* Walk at most one full lap. A sheet of nothing but headings would
     * otherwise spin here forever, and a table is an easy thing to edit into
     * that state by accident. */
    for (int guard = 0; guard < ROW_COUNT; ++guard)
    {
        at = (at + step + ROW_COUNT) % ROW_COUNT;
        if (ROWS[at].kind != SETTING_ROW_HEADING)
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
    case SETTING_MORE_HEARTS:
        return flip(&settings->assist.more_hearts);
    case SETTING_SLOWER_GUARDS:
        return flip(&settings->assist.slower_guards);
    case SETTING_INFINITE_LIVES:
        return flip(&settings->assist.infinite_lives);
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
        KEY_HEARTS " %d\n"
        KEY_GUARDS " %d\n"
        KEY_LIVES " %d\n",
        clamp_percent(settings->music_volume),
        clamp_percent(settings->sfx_volume),
        settings->fullscreen ? 1 : 0,
        settings->crt_filter ? 1 : 0,
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
    return (size_t)written;
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

static void apply_line(Settings *settings, const char *line)
{
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0' || *line == '#')
        return;

    const char *value = NULL;
    if (line_key_is(line, KEY_MUSIC, &value))
        settings->music_volume = clamp_percent((int)strtol(value, NULL, 10));
    else if (line_key_is(line, KEY_SFX, &value))
        settings->sfx_volume = clamp_percent((int)strtol(value, NULL, 10));
    else if (line_key_is(line, KEY_FULLSCREEN, &value))
        settings->fullscreen = strtol(value, NULL, 10) != 0;
    else if (line_key_is(line, KEY_CRT, &value))
        settings->crt_filter = strtol(value, NULL, 10) != 0;
    else if (line_key_is(line, KEY_HEARTS, &value))
        settings->assist.more_hearts = strtol(value, NULL, 10) != 0;
    else if (line_key_is(line, KEY_GUARDS, &value))
        settings->assist.slower_guards = strtol(value, NULL, 10) != 0;
    else if (line_key_is(line, KEY_LIVES, &value))
        settings->assist.infinite_lives = strtol(value, NULL, 10) != 0;
    /* Anything else is a line from a build that knew something this one does
     * not. Ignoring it is deliberate: a settings file must never be the reason
     * a game refuses to start. */
}

void settings_parse(Settings *settings, const char *text)
{
    if (text == NULL)
        return;

    char line[128];
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
