#include "progress.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The campaign is fifteen sectors and the score is a plain int; both are
 * clamped on the way in so a hand-edited file cannot hand the title screen a
 * sector that does not exist or a score that is not a number. The sector cap
 * is deliberately generous rather than EMBEDDED_LEVEL_COUNT: this module links
 * no level data, and the shell re-checks the index before loading anything. */
#define PROGRESS_MAX_SECTOR 999
#define PROGRESS_MAX_SCORE 99999999

static int clamp_int(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

void progress_defaults(Progress *progress)
{
    if (progress == NULL)
        return;
    progress->best_score = 0;
    progress->furthest_sector = 0;
}

bool progress_note_sector(Progress *progress, int sector_index)
{
    if (progress == NULL || sector_index <= progress->furthest_sector)
        return false;
    progress->furthest_sector =
        clamp_int(sector_index, 0, PROGRESS_MAX_SECTOR);
    return true;
}

bool progress_note_score(Progress *progress, int score)
{
    if (progress == NULL || score <= progress->best_score)
        return false;
    progress->best_score = clamp_int(score, 0, PROGRESS_MAX_SCORE);
    return true;
}

/* ---- The file ------------------------------------------------------- */

/* Same shape as the settings file, and for the same reason: a player who opens
 * it can read what is in it. The keys are spelled once, shared by the writer
 * and the reader, so neither can drift from the other. */
#define KEY_BEST "best_score"
#define KEY_SECTOR "furthest_sector"

size_t progress_serialize(const Progress *progress, char *out, size_t cap)
{
    if (progress == NULL || out == NULL || cap == 0)
        return 0;

    int written = snprintf(
        out, cap,
        "# Chuck progress. Delete this file to start the tower again.\n"
        KEY_BEST " %d\n"
        KEY_SECTOR " %d\n",
        clamp_int(progress->best_score, 0, PROGRESS_MAX_SCORE),
        clamp_int(progress->furthest_sector, 0, PROGRESS_MAX_SECTOR));

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
 * whatever follows it. */
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
 * `strtol` reads "no digits" as zero, and taking that would turn every kind of
 * damage into a wipe: a write cut off mid-number leaves `best_score ` behind,
 * and a run somebody spent an evening on would be gone because their disk
 * filled up. A key this build recognises with a value it cannot read is
 * treated exactly as a key it does not recognise — the number keeps whatever
 * it already had.
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

static void apply_line(Progress *progress, const char *line)
{
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0' || *line == '#')
        return;

    const char *value = NULL;
    int number = 0;
    if (line_key_is(line, KEY_BEST, &value))
    {
        if (read_int(value, &number))
            progress->best_score = clamp_int(number, 0, PROGRESS_MAX_SCORE);
    }
    else if (line_key_is(line, KEY_SECTOR, &value))
    {
        if (read_int(value, &number))
            progress->furthest_sector =
                clamp_int(number, 0, PROGRESS_MAX_SECTOR);
    }
    /* Anything else is a line from a build that knew something this one does
     * not. Ignoring it is deliberate, exactly as it is for the settings: a
     * file on disk must never be the reason a game refuses to start. */
}

void progress_parse(Progress *progress, const char *text)
{
    if (progress == NULL || text == NULL)
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
                apply_line(progress, line);
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
