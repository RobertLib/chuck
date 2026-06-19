#ifndef CHUCK_EDITOR_VALIDATE_H
#define CHUCK_EDITOR_VALIDATE_H

/*
 * Everything `make test` will say about a map, said while it is being drawn.
 *
 * The campaign's rules are pinned by tests that run over the embedded levels,
 * which means an author only finds out a sector is unfinishable, or repeats the
 * storey rhythm of another one, after building and running the suite. The same
 * questions are asked here against the map in the editor, using the same route
 * model (level_route.h) and the same parser, so the answer cannot disagree.
 *
 * No SDL: the report is data, and the editor's chrome is what draws it.
 */

#include "editor_doc.h"
#include "level_route.h"

#define ED_MAX_FINDINGS 96
#define ED_FINDING_LEN 168
#define ED_MAX_CAMPAIGN 32

typedef enum
{
    ED_SEV_ERROR = 0, /* the game or the test suite rejects this */
    ED_SEV_WARN,      /* it loads, but it will not play the way it reads */
    ED_SEV_NOTE       /* worth knowing before it becomes one of the above */
} EdSeverity;

typedef struct
{
    EdSeverity severity;
    int col, row; /* -1 when the finding is about the map as a whole */
    char text[ED_FINDING_LEN];
} EdFinding;

/* One sector as it sits on disk, which is all the cross-sector rules need. */
typedef struct
{
    bool loaded;
    bool facade;
    bool has_window;
    bool has_exit;
    bool has_sublevel_entrance;
    LevelTheme theme;
    int width, height;
    int budget;
    int bazookas;
    int rhythm_len;
    int rhythm[MAX_LEVEL_HEIGHT];
} EdCampaignLevel;

typedef struct
{
    /* Indexed by sector number - 1. */
    EdCampaignLevel levels[ED_MAX_CAMPAIGN];
    int count;
} EdCampaign;

typedef struct
{
    bool parsed; /* level_load_data accepted the text */

    EdFinding findings[ED_MAX_FINDINGS];
    int count;
    int dropped; /* findings past ED_MAX_FINDINGS */
    int errors;
    int warnings;
    int notes;

    /* Kept so the canvas can draw what the route model saw. */
    bool route_valid;
    RouteMap route;
    RouteCell start;
    RouteCell goal;
    bool goal_reached;
    bool no_stranding;

    int budget;
    int counts[128]; /* how many of each legend character the map holds */
} EdReport;

/* Fill `campaign` slot `number` from a level that has already been parsed. */
void editor_campaign_record(EdCampaign *campaign, int number,
                            const Level *level);

/*
 * `level` is the document parsed by `level_load_data`, and `parsed` says
 * whether that succeeded — a map with two exits fails to load, and the report
 * still has to explain why.
 */
void editor_validate(const EditorDoc *doc, const Level *level, bool parsed,
                     const EdCampaign *campaign, EdReport *report);

#endif /* CHUCK_EDITOR_VALIDATE_H */
