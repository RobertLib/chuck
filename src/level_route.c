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

bool route_passable(const RouteMap *route, int col, int row)
{
    if (!route_inside(route, col, row))
        return false;
    if (route->level->map.tiles[row][col] == TILE_WALL)
        return false;
    return !route->spike[row][col];
}

bool route_support(const RouteMap *route, int col, int row)
{
    if (route_inside(route, col, row + 1) &&
        route->level->map.tiles[row + 1][col] == TILE_WALL)
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

    for (int step = -1; step <= 1; step += 2)
    {
        int next = col + step;
        if (route_passable(route, next, row))
        {
            if (route_standing(route, next, row))
                out[count++] = (RouteCell){next, row};
            else if (route_landing(route, next, row, &landing))
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
            walls += map->tiles[row][col] == TILE_WALL;
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
    for (int i = 0; i < level->map.enemy_count; ++i)
        dogs += level->map.enemy_spawns[i].has_dog;
    return 3 * level->map.enemy_count + 2 * dogs +
           2 * level->map.mine_count + level->map.spike_count +
           level->map.ceiling_fan_count;
}
