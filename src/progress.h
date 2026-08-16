#ifndef CHUCK_PROGRESS_H
#define CHUCK_PROGRESS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * What survives the game being closed.
 *
 * The campaign is fifteen sectors and a prologue, which is more than one
 * sitting for most people, and until now none of it outlived the process: a
 * run abandoned on sector nine started again in the lobby, and a score was
 * forgotten the moment the title screen came back. Two numbers fix both, and
 * they are deliberately only two — this is a record of what happened, not a
 * save state. Nothing about the sector itself is stored, so resuming is a
 * fresh run of that sector rather than a restored one, and no file on disk can
 * ever put the simulation into a state the game could not reach by playing.
 *
 * Like [settings.c](settings.c) this module links no SDL: the shell owns the
 * file, because `SDL_GetPrefPath` is the only part of it that needs a
 * platform, and the tests hold the rest to a round trip through text.
 */
typedef struct
{
    /* The best score any run has finished on. */
    int best_score;
    /* The highest campaign sector index a run has ever reached, 0-based. Zero
     * means the lobby and nothing past it, which is also what a fresh install
     * says, so the title screen offers no resume until a sector has actually
     * been earned. */
    int furthest_sector;
} Progress;

void progress_defaults(Progress *progress);

/*
 * Both of these answer "did this change anything", so the shell writes the
 * file on the frames that moved a number and on no others.
 */
bool progress_note_sector(Progress *progress, int sector_index);
bool progress_note_score(Progress *progress, int score);

/* Text in, text out; the same damaged-file rule the settings keep — anything
 * unrecognised is left at whatever it already was rather than reset. */
size_t progress_serialize(const Progress *progress, char *out, size_t cap);
void progress_parse(Progress *progress, const char *text);

#endif /* CHUCK_PROGRESS_H */
