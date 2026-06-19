#ifndef CHUCK_PROGRESS_H
#define CHUCK_PROGRESS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * What survives the game being closed.
 *
 * The campaign is seventeen sectors and a prologue, which is more than one
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
/*
 * How many sectors this file will keep a time for.
 *
 * Deliberately not `EMBEDDED_LEVEL_COUNT`: this module links no level data at
 * all, which is the whole reason it can be held to a round trip through text by
 * a suite that links no SDL either. It is generous rather than exact for the
 * same reason `PROGRESS_MAX_SECTOR` is — a sixteenth sector must not need an
 * edit here, and a hand-edited file naming sector 900 must not be able to write
 * past the end of an array.
 */
#define PROGRESS_MAX_TRACKED_SECTORS 32

/* No run has finished this sector yet. Nought would be a perfect time rather
 * than an absent one, which is the shape of bug that makes a leaderboard
 * useless the first time anybody looks at it. */
#define PROGRESS_NO_TIME 0.0f

/*
 * The widest each stored figure may be, and why they are up here rather than
 * inside [progress.c](progress.c) where the clamps use them.
 *
 * A hand-edited file cannot hand the title screen a score that is not a number,
 * which is what the clamps are for — but the same ceiling is also what bounds
 * the width of every line that ever prints one of these, and a fit check that
 * invented its own idea of "the widest score" would be measuring a line the
 * game cannot produce while missing the one it can. [run_tally.c](run_tally.c)
 * lays out its line against these and the suite measures it against the same
 * ones, so the words, the clamp and the check are one number apiece.
 */
#define PROGRESS_MAX_SCORE 99999999
/* Generous rather than the campaign's actual sheet count, for the reason the
 * sector cap below is: this module links no level data and must not have to be
 * edited when the maps lay out one more. */
#define PROGRESS_MAX_EVIDENCE 99

typedef struct
{
    /* The best score any run has finished on. */
    int best_score;
    /* The highest campaign sector index a run has ever reached, 0-based. Zero
     * means the lobby and nothing past it, which is also what a fresh install
     * says, so the title screen offers no resume until a sector has actually
     * been earned. */
    int furthest_sector;
    /*
     * The quickest any run has cleared each sector, in seconds, or
     * `PROGRESS_NO_TIME` for one nobody has finished.
     *
     * It is here rather than in the score for a reason the report between
     * floors already argues: the night clock gives every sector two and a half
     * minutes and pays for the seconds handed back, so the game has been asking
     * players to go fast since `campaign_award_sector_bonus` existed — and then
     * threw away the only number that would tell them whether they had. A par
     * you cannot measure yourself against is a par nobody plays for twice.
     */
    float best_sector_time[PROGRESS_MAX_TRACKED_SECTORS];
    /*
     * The most sheets of the docket any single run has come away with.
     *
     * A high-water mark rather than a set of flags, and the difference is a
     * decision about what the collectable *is*. Flags would let a player
     * assemble the case across a dozen abandoned runs, which is a checklist;
     * this asks for them in one night, which is what the fiction is about —
     * Chuck is not coming back tomorrow. It is also the third ratchet in this
     * file and behaves exactly like the other two.
     */
    int best_evidence;
} Progress;

void progress_defaults(Progress *progress);

/*
 * Both of these answer "did this change anything", so the shell writes the
 * file on the frames that moved a number and on no others.
 */
bool progress_note_sector(Progress *progress, int sector_index);
bool progress_note_score(Progress *progress, int score);
/* How much of the docket a finished run came away with. True only when it beats
 * what any previous run managed. */
bool progress_note_evidence(Progress *progress, int found);
/* A sector cleared in `seconds`. True only when that is quicker than anything
 * before it, which — unlike the two above — includes the first time it is
 * cleared at all. An index outside the tracked range is dropped rather than
 * clamped: writing sector 900's time into sector 31's slot would be a record
 * for a floor nobody played. */
bool progress_note_sector_time(Progress *progress, int sector_index,
                               float seconds);
/* The stored time, or `PROGRESS_NO_TIME`. Safe for any index. */
float progress_sector_time(const Progress *progress, int sector_index);

/*
 * Throw the three ratchets away and keep the resume.
 *
 * The options sheet's RESET RECORDS row, and the split is the point of it:
 * `best_score`, `best_sector_time` and `best_evidence` are records and go, while
 * `furthest_sector` is the title screen's resume chip and stays. Clearing that
 * too would answer "my times are polluted, I want them back" by also throwing
 * away the campaign the player is in the middle of.
 */
void progress_clear_records(Progress *progress);

/* Text in, text out; the same damaged-file rule the settings keep — anything
 * unrecognised is left at whatever it already was rather than reset. */
size_t progress_serialize(const Progress *progress, char *out, size_t cap);
void progress_parse(Progress *progress, const char *text);

#endif /* CHUCK_PROGRESS_H */
