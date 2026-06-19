#include "level_route.h"

#include <string.h>

void route_map_init(RouteMap *route, const Level *level)
{
    memset(route, 0, sizeof(*route));
    route->level = level;
    for (int i = 0; i < level->map.spike_count; ++i)
    {
        int col = (int)(level->map.spike_spawns[i].x / TILE_SIZE);
        int row = (int)(level->map.spike_spawns[i].y / TILE_SIZE);
        if (col >= 0 && col < level->map.width &&
            row >= 0 && row < level->map.height)
        {
            route->spike[row][col] = true;
        }
    }
}

bool route_inside(const RouteMap *route, int col, int row)
{
    return col >= 0 && col < route->level->map.width &&
           row >= 0 && row < route->level->map.height;
}

/*
 * Masonry as the map was authored, which is why this reads the map rather than
 * asking `level_is_solid`. A weak wall counts as wall in both directions: the
 * model will not walk through one, because opening it costs an explosive it
 * knows nothing about, and it will stand on one, because a patch set into a
 * floor slab is floor until the player chooses to blow his own floor away.
 * Both halves matter — passable would make a blocked-up opening a route the
 * sector could depend on, and unsupported would cut a floor in two wherever a
 * patch was set into it.
 */
static bool route_masonry(const RouteMap *route, int col, int row)
{
    TileType tile = route->level->map.tiles[row][col];
    /* A duct counts as masonry here, and that is deliberate rather than
     * unfinished: everything this predicate feeds is a question about an
     * *upright* player — is there headroom, can a jump clear this, does a fall
     * stop here, will a slab hold somebody up — and trunking answers wall to
     * every one of them. The crawl through it is not a special case of walking;
     * it is its own move, and it is the one edge `route_neighbours` adds for a
     * duct. Keeping it out of here is what stops a shaft from quietly becoming
     * a place to jump out of, step up out of, or hop a hole through. */
    return tile == TILE_WALL || tile == TILE_WEAK_WALL || tile == TILE_VENT;
}

bool route_passable(const RouteMap *route, int col, int row)
{
    if (!route_inside(route, col, row))
        return false;
    if (route_masonry(route, col, row))
        return false;
    return !route->spike[row][col];
}

bool route_support(const RouteMap *route, int col, int row)
{
    if (route_inside(route, col, row + 1) && route_masonry(route, col, row + 1))
    {
        return true;
    }
    for (int i = 0; i < route->level->runtime.moving_platform_count; ++i)
    {
        const MovingPlatform *platform =
            &route->level->runtime.moving_platforms[i];
        if (platform->row != row + 1)
            continue;
        int left = (int)(platform->left_limit / TILE_SIZE);
        int right = (int)(platform->right_limit / TILE_SIZE);
        if (col >= left && col <= right)
            return true;
    }
    return false;
}

bool route_standing(const RouteMap *route, int col, int row)
{
    if (!route_passable(route, col, row))
        return false;
    return route_support(route, col, row) ||
           route->level->map.tiles[row][col] == TILE_LADDER;
}

bool route_landing(const RouteMap *route, int col, int row, RouteCell *landing)
{
    for (int r = row; r < route->level->map.height; ++r)
    {
        if (!route_passable(route, col, r))
            return false;
        if (route_standing(route, col, r))
        {
            landing->col = col;
            landing->row = r;
            return true;
        }
    }
    return false;
}

bool route_survivable_fall(int from_row, int landing_row)
{
    float drop = (float)(landing_row - from_row) * (float)TILE_SIZE;
    return drop < PLAYER_FATAL_FALL_HEIGHT;
}

/*
 * Stepping off a ledge, which is the one move in this model that is a fall the
 * player chooses to make. It has to be one they can also get up from: a drop
 * past PLAYER_FATAL_FALL_HEIGHT kills outright, whatever the hearts say, so a
 * route that depends on one is not a route.
 *
 * Only this move is capped. `route_landing` itself stays unbounded because its
 * other two callers are not falls at all — the player's own start tile settling
 * onto the floor the map put it above, and a card hanging in mid-air resolving
 * to the floor it is collected from.
 */
static bool route_step_off(const RouteMap *route, int col, int row,
                           RouteCell *landing)
{
    return route_landing(route, col, row, landing) &&
           route_survivable_fall(row, landing->row);
}

/*
 * Trunking, which the player crosses on their elbows.
 *
 * It is its own move for the same reason the lift shaft is: what the player can
 * do from inside one is not what they can do standing on a floor. A crawl runs
 * along its own row and does nothing else — no jump starts in a duct, no step up
 * leaves one, no hole is cleared through one — because a man flat on his face
 * makes none of those moves. `route_masonry` keeps saying wall so that none of
 * them can be routed through a duct by accident.
 */
static bool route_in_duct(const RouteMap *route, int col, int row)
{
    return route_inside(route, col, row) &&
           route->level->map.tiles[row][col] == TILE_VENT;
}

/* A shaft only carries anybody if its run is long enough to hold a lift. */
bool route_in_shaft(const RouteMap *route, int col, int row)
{
    const LevelMap *map = &route->level->map;
    if (!route_inside(route, col, row) ||
        map->tiles[row][col] != TILE_ELEVATOR_SHAFT)
    {
        return false;
    }
    return (row > 0 && map->tiles[row - 1][col] == TILE_ELEVATOR_SHAFT) ||
           (row + 1 < map->height &&
            map->tiles[row + 1][col] == TILE_ELEVATOR_SHAFT);
}

int route_neighbours(const RouteMap *route, int col, int row, RouteCell *out)
{
    const LevelMap *map = &route->level->map;
    int count = 0;
    RouteCell landing;

    /*
     * Inside trunking, the crawl is the whole of what the player can do: along
     * the row, or out onto a floor at either mouth. Every other move below this
     * point belongs to somebody on their feet, and this early return is what
     * keeps them out — without it the model walked *out* of a duct into open
     * air and took the fall, so a shaft over a hole in the floor became a route
     * down through it. That is the one shape a duct must never certify, because
     * the game will not do it: a crawler is stopped by the same air an upright
     * player is stopped by, and being flat on the floor is what makes a duct a
     * duct rather than a doorway.
     */
    if (route_in_duct(route, col, row))
    {
        for (int step = -1; step <= 1; step += 2)
        {
            int next = col + step;
            if (route_in_duct(route, next, row) ||
                route_standing(route, next, row))
            {
                out[count++] = (RouteCell){next, row};
            }
        }
        return count;
    }

    if (map->tiles[row][col] == TILE_LADDER)
    {
        for (int step = -1; step <= 1; step += 2)
        {
            if (route_inside(route, col, row + step) &&
                map->tiles[row + step][col] == TILE_LADDER)
            {
                out[count++] = (RouteCell){col, row + step};
            }
        }
        if (route_standing(route, col, row - 1))
            out[count++] = (RouteCell){col, row - 1};
    }

    if (route_in_shaft(route, col, row))
    {
        for (int step = -1; step <= 1; step += 2)
        {
            if (route_in_shaft(route, col, row + step))
                out[count++] = (RouteCell){col, row + step};
        }
    }

    /* Into a duct from a floor beside its mouth. Leaving one is handled at the
     * top of this function, because inside trunking the crawl is the only move
     * there is. */
    for (int step = -1; step <= 1; step += 2)
    {
        if (route_in_duct(route, col + step, row) &&
            route_standing(route, col, row))
        {
            out[count++] = (RouteCell){col + step, row};
        }
    }

    for (int step = -1; step <= 1; step += 2)
    {
        int next = col + step;
        if (route_passable(route, next, row))
        {
            if (route_standing(route, next, row))
                out[count++] = (RouteCell){next, row};
            else if (route_step_off(route, next, row, &landing))
                out[count++] = landing;
        }
        if (route_in_shaft(route, next, row))
            out[count++] = (RouteCell){next, row};
        if (route_passable(route, col, row - 1) &&
            route_standing(route, next, row - 1))
        {
            out[count++] = (RouteCell){next, row - 1};
        }
    }

    /* A hole is cleared one tile wide under a low ceiling, two with a second
     * open row for the jump to use. */
    for (int step = -1; step <= 1; step += 2)
    {
        for (int hole = 1; hole <= 2; ++hole)
        {
            int destination = col + step * (hole + 1);
            if (!route_standing(route, destination, row))
                continue;
            bool clear = true;
            for (int i = 1; i <= hole && clear; ++i)
            {
                int over = col + step * i;
                clear = route_passable(route, over, row) &&
                        !route_standing(route, over, row);
            }
            for (int i = 0; i <= hole + 1 && clear; ++i)
            {
                for (int head = 1; head <= hole && clear; ++head)
                    clear = route_passable(route, col + step * i, row - head);
            }
            if (clear)
                out[count++] = (RouteCell){destination, row};
        }
    }

    for (int step = -1; step <= 1; step += 2)
    {
        int over = col + step;
        int destination = col + 2 * step;
        if (!route_inside(route, over, row) || !route->spike[row][over])
            continue;
        if (!route_standing(route, destination, row))
            continue;
        bool clear = true;
        for (int i = 0; i <= 2 && clear; ++i)
        {
            for (int head = 1; head <= 2 && clear; ++head)
                clear = route_passable(route, col + step * i, row - head);
        }
        if (clear)
            out[count++] = (RouteCell){destination, row};
    }

    if (map->tiles[row][col] == TILE_DOOR)
    {
        for (int i = 0; i < map->door_count; ++i)
        {
            if (map->doors[i].col != col || map->doors[i].row != row)
                continue;
            int pair = i ^ 1;
            if (pair < map->door_count)
            {
                out[count++] =
                    (RouteCell){map->doors[pair].col, map->doors[pair].row};
            }
        }
    }

    return count;
}

RouteCell route_player_start(const RouteMap *route)
{
    const LevelMap *map = &route->level->map;
    RouteCell start = {
        (int)((map->start_x + PLAYER_W * 0.5f) / TILE_SIZE),
        (int)((map->start_y + PLAYER_H * 0.5f) / TILE_SIZE)};
    RouteCell landing;
    if (!route_standing(route, start.col, start.row) &&
        route_landing(route, start.col, start.row, &landing))
    {
        start = landing;
    }
    return start;
}

void route_flood(RouteMap *route, RouteCell start)
{
    static RouteCell queue[MAX_LEVEL_HEIGHT * MAX_LEVEL_WIDTH];
    int head = 0;
    int tail = 0;

    memset(route->seen, 0, sizeof(route->seen));
    if (!route_inside(route, start.col, start.row))
        return;
    route->seen[start.row][start.col] = true;
    queue[tail++] = start;
    while (head < tail)
    {
        RouteCell current = queue[head++];
        RouteCell next[ROUTE_MAX_NEIGHBOURS];
        int count = route_neighbours(route, current.col, current.row, next);
        for (int i = 0; i < count; ++i)
        {
            if (route->seen[next[i].row][next[i].col])
                continue;
            route->seen[next[i].row][next[i].col] = true;
            queue[tail++] = next[i];
        }
    }
}

bool route_reaches(const RouteMap *route, int col, int row)
{
    RouteCell landing;
    if (route_inside(route, col, row) && route->seen[row][col])
        return true;
    if (!route_landing(route, col, row, &landing))
        return false;
    return route->seen[landing.row][landing.col];
}

bool route_never_strands(RouteMap *route, RouteCell goal)
{
    memset(route->escapes, 0, sizeof(route->escapes));
    if (!route_inside(route, goal.col, goal.row))
        return false;
    route->escapes[goal.row][goal.col] = true;

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (int row = 0; row < route->level->map.height; ++row)
        {
            for (int col = 0; col < route->level->map.width; ++col)
            {
                if (!route->seen[row][col] || route->escapes[row][col])
                    continue;
                RouteCell next[ROUTE_MAX_NEIGHBOURS];
                int count = route_neighbours(route, col, row, next);
                for (int i = 0; i < count; ++i)
                {
                    if (!route->escapes[next[i].row][next[i].col])
                        continue;
                    route->escapes[row][col] = true;
                    changed = true;
                    break;
                }
            }
        }
    }

    for (int row = 0; row < route->level->map.height; ++row)
    {
        for (int col = 0; col < route->level->map.width; ++col)
        {
            if (route->seen[row][col] && !route->escapes[row][col])
                return false;
        }
    }
    return true;
}

int level_storey_rhythm(const LevelMap *map, int *bands, int max_bands)
{
    int count = 0;
    int previous = -1;
    for (int row = 0; row < map->height; ++row)
    {
        int walls = 0;
        for (int col = 0; col < map->width; ++col)
        {
            /* A patch is part of the architecture: a slab with one blocked-up
             * opening in it is still the storey the sector is built out of. */
            walls += map->tiles[row][col] == TILE_WALL ||
                     map->tiles[row][col] == TILE_WEAK_WALL;
        }
        if (walls * 100 < map->width * 85)
            continue;
        if (previous >= 0 && count < max_bands)
            bands[count++] = row - previous - 1;
        previous = row;
    }
    return count;
}

int level_hazard_budget(const Level *level)
{
    if (level->map.mode == LEVEL_MODE_FACADE)
    {
        int budget = 0;
        for (int i = 0; i < level->map.facade_hazard_spawn_count; ++i)
        {
            budget += level->map.facade_hazard_spawns[i].type ==
                              FACADE_HAZARD_THROWN_OBJECT
                          ? 3
                          : 2;
        }
        return budget;
    }

    int dogs = 0;
    /* A heavy is counted at his own weight rather than as another guard: he
     * denies the free kill outright, which is worth more of a floor's pressure
     * than one more man with the same three answers to him. */
    int men = 0;
    for (int i = 0; i < level->map.enemy_count; ++i)
    {
        dogs += level->map.enemy_spawns[i].has_dog;
        men += level->map.enemy_spawns[i].kind == ENEMY_KIND_HEAVY
                   ? ENEMY_HEAVY_HAZARD_WEIGHT
                   : 3;
    }
    return men + 2 * dogs +
           2 * level->map.mine_count + level->map.spike_count +
           CAMERA_HAZARD_WEIGHT * level->map.camera_count +
           level->map.ceiling_fan_count;
}
