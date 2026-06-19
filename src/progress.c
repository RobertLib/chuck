#include "progress.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The campaign is seventeen sectors and the score is a plain int; both are
 * clamped on the way in so a hand-edited file cannot hand the title screen a
 * sector that does not exist or a score that is not a number. The sector cap
 * is deliberately generous rather than EMBEDDED_LEVEL_COUNT: this module links
 * no level data, and the shell re-checks the index before loading anything.
 *
 * `PROGRESS_MAX_SCORE` and `PROGRESS_MAX_EVIDENCE` are in the header instead,
 * because the line that prints them is laid out against the same ceilings —
 * see the note beside them there. */
#define PROGRESS_MAX_SECTOR 999
/* A sector cleared faster than this is a hand-edited file rather than a run:
 * the walk to the first door alone takes longer. Slower than this is a session
 * somebody left running, and either would sit at the top of the sheet for
 * ever. */
#define PROGRESS_MIN_TIME 1.0f
#define PROGRESS_MAX_TIME 5999.0f

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
    progress->best_evidence = 0;
    for (int i = 0; i < PROGRESS_MAX_TRACKED_SECTORS; ++i)
        progress->best_sector_time[i] = PROGRESS_NO_TIME;
}

void progress_clear_records(Progress *progress)
{
    if (progress == NULL)
        return;
    progress->best_score = 0;
    progress->best_evidence = 0;
    for (int i = 0; i < PROGRESS_MAX_TRACKED_SECTORS; ++i)
        progress->best_sector_time[i] = PROGRESS_NO_TIME;
}

bool progress_note_evidence(Progress *progress, int found)
{
    if (progress == NULL || found <= progress->best_evidence)
        return false;
    progress->best_evidence = clamp_int(found, 0, PROGRESS_MAX_EVIDENCE);
    return true;
}

bool progress_note_sector_time(Progress *progress, int sector_index,
                               float seconds)
{
    if (progress == NULL || sector_index < 0 ||
        sector_index >= PROGRESS_MAX_TRACKED_SECTORS)
        return false;
    if (seconds < PROGRESS_MIN_TIME || seconds > PROGRESS_MAX_TIME)
        return false;
    float best = progress->best_sector_time[sector_index];
    /* The first clear always counts, which is the one way this ratchet differs
     * from the other two: they start at a real value (nought points, the lobby)
     * and this one starts at "nobody has". */
    if (best != PROGRESS_NO_TIME && seconds >= best)
        return false;
    progress->best_sector_time[sector_index] = seconds;
    return true;
}

float progress_sector_time(const Progress *progress, int sector_index)
{
    if (progress == NULL || sector_index < 0 ||
        sector_index >= PROGRESS_MAX_TRACKED_SECTORS)
        return PROGRESS_NO_TIME;
    return progress->best_sector_time[sector_index];
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
#define KEY_EVIDENCE "best_evidence"
/* One line per sector that has a time, written as `sector_time N SECONDS` — a
 * key and an index rather than thirty-two numbered keys, so the file stays as
 * short as the player's progress actually is and a build that tracks more
 * sectors reads an older file unchanged. */
#define KEY_SECTOR_TIME "sector_time"

size_t progress_serialize(const Progress *progress, char *out, size_t cap)
{
    if (progress == NULL || out == NULL || cap == 0)
        return 0;

    int written = snprintf(
        out, cap,
        "# Chuck progress. Delete this file to start the tower again.\n"
        KEY_BEST " %d\n"
        KEY_SECTOR " %d\n"
        KEY_EVIDENCE " %d\n",
        clamp_int(progress->best_score, 0, PROGRESS_MAX_SCORE),
        clamp_int(progress->furthest_sector, 0, PROGRESS_MAX_SECTOR),
        clamp_int(progress->best_evidence, 0, PROGRESS_MAX_EVIDENCE));

    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return cap - 1;

    size_t used = (size_t)written;
    for (int i = 0; i < PROGRESS_MAX_TRACKED_SECTORS; ++i)
    {
        float best = progress->best_sector_time[i];
        if (best == PROGRESS_NO_TIME)
            continue;
        int line = snprintf(out + used, cap - used,
                            KEY_SECTOR_TIME " %d %.2f\n", i, (double)best);
        /* Out of room stops the file here rather than truncating a line into
         * something the reader would have to guess at. What is already written
         * is a complete, valid file — which is the same promise `read_int`
         * makes from the other end. */
        if (line < 0 || (size_t)line >= cap - used)
        {
            out[used] = '\0';
            break;
        }
        used += (size_t)line;
    }
    return used;
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
    else if (line_key_is(line, KEY_SECTOR_TIME, &value))
    {
        /* Two fields, and both have to be there: an index with no time after it
         * is exactly the half-written line `read_int` exists to survive, and
         * taking it would file a nought — a perfect time — against a sector
         * nobody had cleared. */
        char *end = NULL;
        long index = strtol(value, &end, 10);
        if (end != value && index >= 0 &&
            index < PROGRESS_MAX_TRACKED_SECTORS)
        {
            const char *rest = end;
            char *time_end = NULL;
            double seconds = strtod(rest, &time_end);
            if (time_end != rest && seconds >= PROGRESS_MIN_TIME &&
                seconds <= PROGRESS_MAX_TIME)
                progress->best_sector_time[index] = (float)seconds;
        }
    }
    else if (line_key_is(line, KEY_EVIDENCE, &value))
    {
        if (read_int(value, &number))
            progress->best_evidence =
                clamp_int(number, 0, PROGRESS_MAX_EVIDENCE);
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
