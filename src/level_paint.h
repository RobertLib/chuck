#ifndef CHUCK_LEVEL_PAINT_H
#define CHUCK_LEVEL_PAINT_H

/*
 * The eight-colour paint each theme is drawn in.
 *
 * A theme is a floor plan's look: which of the eight colours its masonry is,
 * which its walkway lip, which the faint lines of the room behind everything.
 * Two renderers read it — [level_art.c](level_art.c) for the walls and the
 * room behind them, [game_render.c](game_render.c) for the ladders, the doors
 * and the lamps in it — and they have to agree, so it is one table and it is
 * here.
 *
 * Here rather than in level_art.c for the reason `THEME_MUSIC` is in level.c:
 * level_art.c links SDL and no test can reach it, and a theme added without a
 * row would zero-fill to black masonry in a black room — a sector drawn as
 * nothing at all. `test_every_theme_has_a_paint_it_can_be_seen_in` holds every
 * row to being visible.
 *
 * The rules the rows keep (see docs/art-and-audio.md):
 *  - **Rooms are black.** The backdrop colour is for the room's own lines,
 *    never a fill, and it is the quietest colour on the floor — blue, almost
 *    everywhere — because the whole cast is drawn over it.
 *  - **Structure is saturated.** Masonry and the lip a foot lands on are what
 *    a player reads a floor plan from, so they are never black and never the
 *    backdrop's colour.
 *  - **No two sectors in a row share masonry**, which is what tells a player
 *    the climb has moved on.
 */

#include <stdbool.h>

#include "bbc_art.h"
#include "level.h"

typedef struct
{
    BbcColour wall;     /* the body of a solid tile */
    BbcColour wall_ink; /* its joints, mortar, seams and rivets */
    BbcColour trim;     /* the lip a foot lands on */
    BbcColour trim_ink; /* the lip's second colour: a stripe, a nosing */
    BbcColour accent;   /* signage, paint marks, small fittings */
    BbcColour backdrop; /* the room's own lines, behind everything */
    BbcColour lamp;     /* anything that gives light: a lamp, a lit window */
    bool lit;           /* whether the ceilings carry lamps at all */
} LevelThemePaint;

const LevelThemePaint *level_theme_paint(LevelTheme theme);

#endif /* CHUCK_LEVEL_PAINT_H */
