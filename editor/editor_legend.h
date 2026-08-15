#ifndef CHUCK_EDITOR_LEGEND_H
#define CHUCK_EDITOR_LEGEND_H

/*
 * Every character a map file may contain, in one table.
 *
 * levels/LEGEND.md is the document an author reads; this is the same list in a
 * form the editor can paint a palette from, colour a tile with and check an
 * unknown character against. The parser silently turns anything it does not
 * recognise into air, so a typo is invisible in the game and has to be caught
 * here.
 */

#include <stdbool.h>

typedef enum
{
    ED_GROUP_TERRAIN = 0,
    ED_GROUP_ROUTE,
    ED_GROUP_ITEMS,
    ED_GROUP_ENEMIES,
    ED_GROUP_PEOPLE,
    ED_GROUP_HAZARDS,
    ED_GROUP_FITTINGS,
    ED_GROUP_OFFICE,
    ED_GROUP_LOBBY,
    ED_GROUP_RESTROOM,
    /* Props that belong to this night rather than to the building: the crew's
     * flight case and the clock the job is running to. They are dressing like
     * any other prop, but they are the dressing that carries the story, so
     * they are painted from their own bin rather than buried in the office
     * set where an author would place them for the wrong reason. */
    ED_GROUP_NIGHT,
    ED_GROUP_FACADE,
    ED_GROUP_COUNT
} EdGroup;

typedef struct
{
    char symbol;
    const char *name;   /* short label for the palette */
    const char *detail; /* the sentence LEGEND.md gives it */
    EdGroup group;
    unsigned char r, g, b; /* the colour the editor draws it in */
    bool solid;            /* collides with the player */
    bool interior_only;    /* meaningless on a climbed wall */
    bool facade_only;
} EdSymbol;

extern const EdSymbol ED_SYMBOLS[];
extern const int ED_SYMBOL_COUNT;

extern const char *const ED_GROUP_NAMES[ED_GROUP_COUNT];

/* NULL when the character is not in the legend at all. */
const EdSymbol *editor_symbol(char symbol);

/*
 * True for a prop the loader hangs from the tile above rather than stands on
 * the tile below (`decoration_hangs` in level.h is the same rule on the
 * parsed side). One character has it, and it is named in one place here so
 * the report and the loader cannot end up disagreeing about which way up a
 * prop needs its wall.
 */
bool editor_symbol_hangs(char symbol);

#endif /* CHUCK_EDITOR_LEGEND_H */
