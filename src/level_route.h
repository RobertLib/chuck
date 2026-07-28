#ifndef CHUCK_LEVEL_ROUTE_H
#define CHUCK_LEVEL_ROUTE_H

/*
 * A deliberately conservative account of what the player can do on foot: walk,
 * fall, step up one tile, jump a one-tile hole (two tiles with a second open
 * row overhead), hop a single floor spike with that same clearance, ride
 * ladders, lift shafts and moving platforms, and step through a paired door.
 * Falling panels are left out on purpose, because a map has to still work once
 * every one of them has gone. Everything it reaches really is reachable, so a
 * map it calls finishable is finishable.
 *
 * It lives here rather than in the test suite because the level editor asks
 * exactly the same question while a map is being drawn. Two models would drift,
 * and a sector the editor calls solvable that `make test` then rejects is worse
 * than no check at all.
 */

#include "level.h"

#define ROUTE_MAX_NEIGHBOURS 24

typedef struct
{
    int col;
    int row;
} RouteCell;

typedef struct
{
    const Level *level;
    bool spike[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    /* Filled by route_flood: every cell the player can stand in. */
    bool seen[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    /* Filled by route_never_strands: every seen cell the goal is still
     * reachable from. */
    bool escapes[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
} RouteMap;

void route_map_init(RouteMap *route, const Level *level);

bool route_inside(const RouteMap *route, int col, int row);
bool route_passable(const RouteMap *route, int col, int row);
bool route_support(const RouteMap *route, int col, int row);
bool route_standing(const RouteMap *route, int col, int row);
/* Where a body dropped at (col,row) comes to rest. False if it never does. */
bool route_landing(const RouteMap *route, int col, int row, RouteCell *landing);
bool route_in_shaft(const RouteMap *route, int col, int row);

/* Every cell one step of the model leads to. `out` needs ROUTE_MAX_NEIGHBOURS. */
int route_neighbours(const RouteMap *route, int col, int row, RouteCell *out);

/* The tile the player actually starts standing on, having fallen if the map
 * put `S` in mid-air. */
RouteCell route_player_start(const RouteMap *route);

/* Flood `route->seen` from `start`. */
void route_flood(RouteMap *route, RouteCell start);

/* True if the flood reached (col,row), or the tile a body dropped there lands
 * on — a card floating over a floor is picked up by walking under it. */
bool route_reaches(const RouteMap *route, int col, int row);

/* One-way drops are fine; being dropped somewhere the goal can no longer be
 * reached from is not. Fills `route->escapes`. */
bool route_never_strands(RouteMap *route, RouteCell goal);

/*
 * The storey rhythm of a map: the run of open rows between each pair of
 * full-width structural slabs. It ignores furniture and ladder columns, so two
 * sectors built on the same stack of floors produce the same rhythm however
 * differently they are dressed - which is exactly the repetition worth
 * forbidding.
 */
int level_storey_rhythm(const LevelMap *map, int *bands, int max_bands);

/* Pressure the sector puts the player under, in the campaign's own currency. */
int level_hazard_budget(const Level *level);

#endif /* CHUCK_LEVEL_ROUTE_H */
