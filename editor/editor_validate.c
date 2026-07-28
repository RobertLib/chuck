#include "editor_validate.h"

#include "editor_legend.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The campaign shape `test_all_embedded_levels_parse` pins. */
#define ED_CAMPAIGN_LENGTH 15
#define ED_CAMPAIGN_FACADES 4
#define ED_CAMPAIGN_RESTROOM_DOORS 4
#define ED_FACADE_MIN_HAZARDS 8
#define ED_FACADE_MIN_WALLS 40
#define ED_FACADE_MAX_ITEMS 4
#define ED_INTERIOR_MIN_ALARMS 2
/* The outer 80px of a climb is behind the inset, so masonry starts here. */
#define ED_FACADE_FIRST_COLUMN 2

static void report_add(EdReport *report, EdSeverity severity, int col, int row,
                       const char *format, ...)
{
    if (report->count >= ED_MAX_FINDINGS)
    {
        report->dropped++;
        return;
    }
    EdFinding *finding = &report->findings[report->count++];
    finding->severity = severity;
    finding->col = col;
    finding->row = row;

    va_list args;
    va_start(args, format);
    vsnprintf(finding->text, sizeof(finding->text), format, args);
    va_end(args);

    switch (severity)
    {
    case ED_SEV_ERROR:
        report->errors++;
        break;
    case ED_SEV_WARN:
        report->warnings++;
        break;
    case ED_SEV_NOTE:
        report->notes++;
        break;
    }
}

static char doc_at(const EditorDoc *doc, int col, int row)
{
    return editor_doc_get(doc, col, row);
}

static bool doc_solid(const EditorDoc *doc, int col, int row)
{
    return doc_at(doc, col, row) == '#';
}

static int count_of(const EdReport *report, char symbol)
{
    unsigned char index = (unsigned char)symbol;
    return index < 128 ? report->counts[index] : 0;
}

void editor_campaign_record(EdCampaign *campaign, int number,
                            const Level *level)
{
    if (number < 1 || number > ED_MAX_CAMPAIGN)
        return;
    EdCampaignLevel *slot = &campaign->levels[number - 1];
    memset(slot, 0, sizeof(*slot));
    slot->loaded = true;
    slot->facade = level->map.mode == LEVEL_MODE_FACADE;
    slot->has_window = level->map.has_window;
    slot->has_exit = level->map.has_exit;
    slot->has_sublevel_entrance = level->map.has_sublevel_entrance;
    slot->theme = level->map.theme;
    slot->width = level->map.width;
    slot->height = level->map.height;
    slot->budget = level_hazard_budget(level);
    for (int i = 0; i < level->runtime.item_count; ++i)
        slot->bazookas += level->runtime.items[i].type == ITEM_BAZOOKA;
    slot->rhythm_len =
        level_storey_rhythm(&level->map, slot->rhythm, MAX_LEVEL_HEIGHT);
    if (number > campaign->count)
        campaign->count = number;
}

/* ---- Structure the parser insists on --------------------------------- */

static void check_characters(const EditorDoc *doc, EdReport *report)
{
    int unknown = 0;
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            char cell = doc_at(doc, col, row);
            unsigned char index = (unsigned char)cell;
            if (index < 128)
                report->counts[index]++;
            if (editor_symbol(cell) != NULL)
                continue;
            unknown++;
            if (unknown <= 6)
            {
                report_add(report, ED_SEV_ERROR, col, row,
                           "'%c' is not in the legend; the loader reads it as air",
                           cell);
            }
        }
    }
    if (unknown > 6)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "%d more characters are not in the legend", unknown - 6);
    }
}

static void check_destinations(const EditorDoc *doc, EdReport *report)
{
    int starts = count_of(report, 'S');
    int exits = count_of(report, 'E');
    int windows = count_of(report, 'Y');
    int returns = count_of(report, 'R');
    int entrances = count_of(report, 'U');
    bool facade = doc->grid.facade;

    if (starts != 1)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "The map needs exactly one player start 'S' (it has %d)",
                   starts);
    }

    if (returns > 0)
    {
        if (returns != 1)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "A sublevel needs exactly one return door 'R' (it has %d)",
                       returns);
        }
        if (exits > 0 || windows > 0)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "A sublevel leaves by its 'R' alone: remove the %d 'E' and %d 'Y'",
                       exits, windows);
        }
    }
    else if (facade)
    {
        if (windows != 1)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "A climb needs exactly one window 'Y' back inside (it has %d)",
                       windows);
        }
        if (exits > 0)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "There is no stair door on a wall: remove the 'E'");
        }
    }
    else
    {
        if (exits != 1)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "An interior needs exactly one exit 'E' (it has %d)",
                       exits);
        }
        if (windows > 1)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "At most one window 'Y' per sector (it has %d)", windows);
        }
    }

    if (entrances > 1)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "Only one restroom door 'U' is loaded; the map has %d",
                   entrances);
    }
    if (entrances > 0 && facade)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "A climb has no restroom to open onto: remove the 'U'");
    }
}

static void check_one_cap(EdReport *report, int used, int cap, const char *what)
{
    if (used > cap)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "%d %s, but the loader keeps %d and silently drops the rest",
                   used, what, cap);
    }
}

static void check_caps(const EditorDoc *doc, EdReport *report)
{
    check_one_cap(report, count_of(report, 'M') + count_of(report, 'W'),
                  MAX_ENEMIES, "guards");
    check_one_cap(report,
                  count_of(report, 'C') + count_of(report, 'G') +
                      count_of(report, 'N') + count_of(report, 'K') +
                      count_of(report, 'Z'),
                  MAX_ITEMS, "items");
    check_one_cap(report, count_of(report, 'X'), MAX_MINES, "mines");
    check_one_cap(report, count_of(report, '^'), MAX_SPIKES, "spikes");
    check_one_cap(report, count_of(report, 'O'), MAX_CEILING_FANS, "fans");
    check_one_cap(report, count_of(report, 'T'), MAX_TERMINALS, "terminals");
    check_one_cap(report, count_of(report, 'A'), MAX_ALARM_SWITCHES,
                  "alarm switches");
    check_one_cap(report, count_of(report, 'B'), MAX_CRATES, "crates");
    check_one_cap(report, count_of(report, 'L'), MAX_GAS_CANISTERS,
                  "gas canisters");
    check_one_cap(report, count_of(report, 'D'), MAX_DOORS, "doors");
    check_one_cap(report, count_of(report, 'J'), MAX_JANITORS, "janitors");
    check_one_cap(report, count_of(report, 'f'), MAX_CIVILIANS, "civilians");
    check_one_cap(report, count_of(report, 'k'), MAX_RECEPTIONISTS,
                  "receptionists");
    check_one_cap(report, count_of(report, 'F'), MAX_FALL_PLATFORMS,
                  "falling panels");
    check_one_cap(report, count_of(report, 'P'), MAX_MOVING_PLATFORMS,
                  "moving platforms");
    check_one_cap(report, count_of(report, 'r') + count_of(report, 'v'),
                  MAX_FACADE_HAZARD_SPAWNS, "facade hazards");
    check_one_cap(report,
                  count_of(report, 'c') + count_of(report, 'd') +
                      count_of(report, 'i') + count_of(report, 'n') +
                      count_of(report, 's') + count_of(report, 't') +
                      count_of(report, 'g') + count_of(report, 'q') +
                      count_of(report, 'b') + count_of(report, 'u') +
                      count_of(report, 'p') + count_of(report, 'o') +
                      count_of(report, 'z'),
                  MAX_DECORATIONS, "decorations");

    if (doc->grid.width >= MAX_LEVEL_WIDTH)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "The map is at the %d-tile width limit", MAX_LEVEL_WIDTH);
    }
    if (doc->grid.height >= MAX_LEVEL_HEIGHT)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "The map is at the %d-tile height limit", MAX_LEVEL_HEIGHT);
    }
}

static void check_doors(const EditorDoc *doc, EdReport *report)
{
    int doors = 0;
    int last_col = -1;
    int last_row = -1;
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'D')
                continue;
            doors++;
            last_col = col;
            last_row = row;
            if (doc_at(doc, col, row + 1) == 'H')
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "This door hangs over a ladder; the route model will not stand on it");
            }
        }
    }

    if (doors % 2 != 0)
    {
        report_add(report, ED_SEV_WARN, last_col, last_row,
                   "%d doors: they pair up 0-1, 2-3, so the last one leads nowhere",
                   doors);
    }

    if (doc->grid.has_spawns && doc->grid.spawn_count != doors)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "SPAWNS lists %d counts for %d doors; the loader rejects the map",
                   doc->grid.spawn_count, doors);
    }
}

static void check_decorations(const EditorDoc *doc, EdReport *report)
{
    int floating = 0;
    int first_col = -1;
    int first_row = -1;
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            const EdSymbol *symbol = editor_symbol(doc_at(doc, col, row));
            if (symbol == NULL)
                continue;
            if (symbol->group != ED_GROUP_OFFICE &&
                symbol->group != ED_GROUP_LOBBY &&
                symbol->group != ED_GROUP_RESTROOM)
            {
                continue;
            }
            if (doc_solid(doc, col, row + 1))
                continue;
            floating++;
            if (first_col < 0)
            {
                first_col = col;
                first_row = row;
            }
        }
    }
    if (floating == 1)
    {
        report_add(report, ED_SEV_WARN, first_col, first_row,
                   "This prop has no wall under it, so the loader drops it");
    }
    else if (floating > 1)
    {
        report_add(report, ED_SEV_WARN, first_col, first_row,
                   "%d props have no wall under them; the loader drops every one",
                   floating);
    }
}

/* ---- Authoring rules for interiors ------------------------------------ */

/* The floor the band containing (col,row) stands on, or -1 for open air. */
static int band_floor_row(const EditorDoc *doc, int col, int row, int spread)
{
    for (int r = row + 1; r < doc->grid.height; ++r)
    {
        for (int c = col - spread; c <= col + spread; ++c)
        {
            if (doc_solid(doc, c, r))
                return r;
        }
    }
    return -1;
}

static void check_fans(const EditorDoc *doc, EdReport *report)
{
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'O')
                continue;

            if (doc_at(doc, col, row + 1) == 'B')
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "A crate under the blades is a step into them");
            }

            for (int c = col - 2; c <= col + 2; ++c)
            {
                char neighbour = doc_at(doc, c, row);
                if (neighbour != 'H' && neighbour != 'V')
                    continue;
                report_add(report, ED_SEV_WARN, col, row,
                           "The blades overhang the %s two columns away",
                           neighbour == 'H' ? "ladder" : "lift shaft");
                break;
            }

            int floor = band_floor_row(doc, col, row, 2);
            if (floor < 0)
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "Nothing under this fan for the player to be caught over");
                continue;
            }
            for (int c = col - 2; c <= col + 2; ++c)
            {
                if (c < 0 || c >= doc->grid.width || doc_solid(doc, c, floor))
                    continue;
                report_add(report, ED_SEV_WARN, col, row,
                           "A hole in the floor two columns away: the route model calls that gap crossed and the blades make it lethal");
                break;
            }
            if (floor == row + 1)
            {
                report_add(report, ED_SEV_NOTE, col, row,
                           "A fan in the standing row kills anyone who walks into it, crawling included");
            }
        }
    }
}

static void check_spikes(const EditorDoc *doc, EdReport *report)
{
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col + 1 < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != '^' || doc_at(doc, col + 1, row) != '^')
                continue;
            report_add(report, ED_SEV_NOTE, col, row,
                       "Two spikes side by side cannot be jumped at all; the floor is cut in half here");
            break; /* one note per row is enough */
        }
    }
}

static void check_moving_platforms(const EditorDoc *doc, EdReport *report)
{
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'P')
                continue;

            int left = col;
            while (left - 1 >= 0)
            {
                char cell = doc_at(doc, left - 1, row);
                if (cell == '#' || cell == 'D' || cell == 'V')
                    break;
                left--;
            }
            int right = col;
            while (right + 1 < doc->grid.width)
            {
                char cell = doc_at(doc, right + 1, row);
                if (cell == '#' || cell == 'D' || cell == 'V')
                    break;
                right++;
            }
            for (int c = left; c <= right; ++c)
            {
                if (doc_at(doc, c, row) != 'H')
                    continue;
                report_add(report, ED_SEV_WARN, col, row,
                           "The platform patrols columns %d-%d, past the ladder at %d; wall the run in",
                           left, right, c);
                break;
            }
        }
    }
}

static void check_people(const EditorDoc *doc, const Level *level,
                         EdReport *report)
{
    for (int i = 0; i < level->map.civilian_count; ++i)
    {
        float centre = level->map.civilian_spawns[i].x + CIVILIAN_W * 0.5f;
        float way_in = level->map.start_x + PLAYER_W * 0.5f;
        if (fabsf(centre - way_in) > CIVILIAN_FADE_DISTANCE)
            continue;
        report_add(report, ED_SEV_ERROR,
                   (int)(level->map.civilian_spawns[i].x / TILE_SIZE),
                   (int)(level->map.civilian_spawns[i].y / TILE_SIZE),
                   "This civilian starts inside the dissolve radius of the way in and fades before running anywhere");
    }

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'k')
                continue;
            int room = 0;
            for (int step = -1; step <= 1; step += 2)
            {
                for (int reach = 1; reach <= 2; ++reach)
                {
                    int c = col + step * reach;
                    if (doc_solid(doc, c, row) || !doc_solid(doc, c, row + 1))
                        break;
                    if (reach == 2)
                        room++;
                }
            }
            if (room == 0)
            {
                report_add(report, ED_SEV_NOTE, col, row,
                           "The receptionist has no two tiles of floor to walk an errand into, so the part is wasted");
            }
        }
    }
}

/* ---- Authoring rules for facades -------------------------------------- */

static void check_facade(const EditorDoc *doc, EdReport *report)
{
    int last_column = doc->grid.width - 1 - ED_FACADE_FIRST_COLUMN;

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            char cell = doc_at(doc, col, row);
            if (cell == 'H')
            {
                report_add(report, ED_SEV_ERROR, col, row,
                           "There are no ladders on a wall; the climb is four-way movement over masonry");
            }
            if (cell != '#')
                continue;
            if (col < ED_FACADE_FIRST_COLUMN || col > last_column)
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "Masonry outside columns %d-%d sits behind the 80px inset, where the climber can never go",
                           ED_FACADE_FIRST_COLUMN, last_column);
            }
            if (!doc_solid(doc, col - 1, row) && !doc_solid(doc, col + 1, row))
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "A lone block seals its two-row band: the climber is exactly one tile tall");
            }
        }
    }

    /* Cornice gaps have to be wide enough to walk through. */
    for (int row = 0; row < doc->grid.height; ++row)
    {
        int run = 0;
        bool after_masonry = false;
        for (int col = ED_FACADE_FIRST_COLUMN; col <= last_column; ++col)
        {
            if (doc_solid(doc, col, row))
            {
                if (after_masonry && run > 0 && run < 3)
                {
                    report_add(report, ED_SEV_WARN, col - run, row,
                               "A %d-tile gap in this cornice; breaches need three tiles or more",
                               run);
                }
                after_masonry = true;
                run = 0;
            }
            else
            {
                run++;
            }
        }
    }

    /* The grid the painted windows sit on. */
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            char cell = doc_at(doc, col, row);
            if (cell != 'S' && cell != 'Y' && cell != 'r' && cell != 'v')
                continue;
            if (row % 3 == 0 && col % 4 == 0)
                continue;
            report_add(report, ED_SEV_NOTE, col, row,
                       "'%c' is off the window grid (rows of 3, columns of 4), so it covers a painted window instead of replacing one",
                       cell);
        }
    }

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'S')
                continue;
            if (doc_solid(doc, col, row - 1))
            {
                report_add(report, ED_SEV_ERROR, col, row - 1,
                           "The cornice above the start seals the climb in before it begins");
            }
        }
    }
}

/* Four-way reachability over the masonry, which is how a climb is traversed. */
static void check_facade_route(const EditorDoc *doc, EdReport *report)
{
    static bool seen[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    static RouteCell queue[MAX_LEVEL_HEIGHT * MAX_LEVEL_WIDTH];
    memset(seen, 0, sizeof(seen));

    int first = ED_FACADE_FIRST_COLUMN;
    int last = doc->grid.width - 1 - ED_FACADE_FIRST_COLUMN;
    RouteCell start = {-1, -1};
    RouteCell window = {-1, -1};
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            char cell = doc_at(doc, col, row);
            if (cell == 'S')
                start = (RouteCell){col, row};
            else if (cell == 'Y')
                window = (RouteCell){col, row};
        }
    }
    if (start.col < 0 || window.col < 0)
        return;

    int head = 0;
    int tail = 0;
    seen[start.row][start.col] = true;
    queue[tail++] = start;
    while (head < tail)
    {
        RouteCell at = queue[head++];
        static const int dc[4] = {-1, 1, 0, 0};
        static const int dr[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; ++i)
        {
            int col = at.col + dc[i];
            int row = at.row + dr[i];
            if (col < first || col > last || row < 0 || row >= doc->grid.height)
                continue;
            if (seen[row][col] || doc_solid(doc, col, row))
                continue;
            seen[row][col] = true;
            queue[tail++] = (RouteCell){col, row};
        }
    }

    if (!seen[window.row][window.col])
    {
        report_add(report, ED_SEV_ERROR, window.col, window.row,
                   "The window cannot be climbed to from the start: the masonry closes the wall off");
    }
}

/* ---- Can the sector actually be finished? ----------------------------- */

static void check_route(const Level *level, EdReport *report)
{
    if (count_of(report, 'S') != 1)
        return;

    route_map_init(&report->route, level);
    report->start = route_player_start(&report->route);
    route_flood(&report->route, report->start);
    report->route_valid = true;

    RouteCell goal;
    if (level->map.has_window)
        goal = (RouteCell){level->map.window_col, level->map.window_row};
    else if (level->map.has_exit)
        goal = (RouteCell){level->map.exit_col, level->map.exit_row};
    else if (level->map.has_sublevel_return)
    {
        goal = (RouteCell){level->map.sublevel_return_col,
                           level->map.sublevel_return_row};
    }
    else
    {
        report->route_valid = false;
        return;
    }
    report->goal = goal;

    report->goal_reached = route_reaches(&report->route, goal.col, goal.row);
    if (!report->goal_reached)
    {
        report_add(report, ED_SEV_ERROR, goal.col, goal.row,
                   "The way out cannot be reached from the start");
    }

    for (int i = 0; i < level->runtime.item_count; ++i)
    {
        const Item *item = &level->runtime.items[i];
        int col = (int)(item->x / TILE_SIZE);
        int row = (int)(item->y / TILE_SIZE);
        if (route_reaches(&report->route, col, row))
            continue;
        if (item->type == ITEM_CARD)
        {
            report_add(report, ED_SEV_ERROR, col, row,
                       "This key card cannot be reached, and the seed may well make it the live one");
        }
        else
        {
            report_add(report, ED_SEV_NOTE, col, row,
                       "This pickup cannot be reached");
        }
    }

    for (int i = 0; i < level->map.terminal_count; ++i)
    {
        const Terminal *terminal = &level->map.terminals[i];
        if (route_reaches(&report->route, terminal->col, terminal->row))
            continue;
        report_add(report, ED_SEV_ERROR, terminal->col, terminal->row,
                   "This terminal cannot be reached, and the seed may well make it the live one");
    }

    if (level->map.has_sublevel_entrance &&
        !route_reaches(&report->route, level->map.sublevel_entrance_col,
                       level->map.sublevel_entrance_row))
    {
        report_add(report, ED_SEV_ERROR, level->map.sublevel_entrance_col,
                   level->map.sublevel_entrance_row,
                   "The restroom door cannot be reached");
    }

    RouteCell landing;
    RouteCell escape = goal;
    if (!route_standing(&report->route, escape.col, escape.row) &&
        route_landing(&report->route, escape.col, escape.row, &landing))
    {
        escape = landing;
    }
    report->no_stranding = route_never_strands(&report->route, escape);
    if (!report->no_stranding)
    {
        int stranded = 0;
        int col = -1;
        int row = -1;
        for (int r = 0; r < level->map.height; ++r)
        {
            for (int c = 0; c < level->map.width; ++c)
            {
                if (!report->route.seen[r][c] || report->route.escapes[r][c])
                    continue;
                stranded++;
                if (col < 0)
                {
                    col = c;
                    row = r;
                }
            }
        }
        report_add(report, ED_SEV_ERROR, col, row,
                   "%d tiles the player can drop into cannot get back to the way out",
                   stranded);
    }
}

/* ---- Rules that span the campaign ------------------------------------- */

static bool same_rhythm(const EdCampaignLevel *a, const EdCampaignLevel *b)
{
    if (a->rhythm_len != b->rhythm_len)
        return false;
    for (int i = 0; i < a->rhythm_len; ++i)
    {
        if (a->rhythm[i] != b->rhythm[i])
            return false;
    }
    return true;
}

static void check_campaign(int number, const EdCampaignLevel *self,
                           const EdCampaign *campaign, EdReport *report)
{
    const EdCampaignLevel *previous =
        number >= 2 && campaign->levels[number - 2].loaded
            ? &campaign->levels[number - 2]
            : NULL;
    const EdCampaignLevel *next =
        number < campaign->count && campaign->levels[number].loaded
            ? &campaign->levels[number]
            : NULL;

    /* Theme. */
    bool facade_theme = self->theme >= LEVEL_THEME_FACADE_NIGHT;
    if (facade_theme != self->facade)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "%s is %s theme, and this sector is %s",
                   level_theme_name(self->theme),
                   facade_theme ? "a FACADE_*" : "an interior",
                   self->facade ? "a climb" : "an interior");
    }
    if (self->theme == LEVEL_THEME_RESTROOM)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "RESTROOM belongs to the sublevel, never to a sector");
    }
    if (previous != NULL && previous->theme == self->theme)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "Sector %d already wears %s; back-to-back sectors must look and sound different",
                   number - 1, level_theme_name(self->theme));
    }
    if (next != NULL && next->theme == self->theme)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "Sector %d also wears %s; back-to-back sectors must look and sound different",
                   number + 1, level_theme_name(self->theme));
    }

    /* Size and storey rhythm. */
    for (int i = 0; i < campaign->count; ++i)
    {
        const EdCampaignLevel *other = &campaign->levels[i];
        if (i + 1 == number || !other->loaded)
            continue;
        if (other->width == self->width && other->height == self->height)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "Sector %d is also %dx%d; two sectors the same size read as one place",
                       i + 1, self->width, self->height);
        }
        if (!self->facade && !other->facade && same_rhythm(self, other))
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "Sector %d is built on the same storey rhythm; change a band height",
                       i + 1);
        }
    }

    /* Pressure only rises. */
    int previous_interior = -1;
    int previous_climb = -1;
    int previous_climb_height = -1;
    for (int i = 0; i + 1 < number; ++i)
    {
        const EdCampaignLevel *before = &campaign->levels[i];
        if (!before->loaded)
            continue;
        if (before->facade)
        {
            previous_climb = before->budget;
            previous_climb_height = before->height;
        }
        else
        {
            previous_interior = before->budget;
        }
    }
    if (self->facade)
    {
        if (self->budget <= previous_climb)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "Hazard budget %d does not beat the previous climb's %d (3 per thrower, 2 per bird)",
                       self->budget, previous_climb);
        }
        if (self->height <= previous_climb_height)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "This climb is %d rows, no taller than the previous one's %d",
                       self->height, previous_climb_height);
        }
    }
    else if (self->budget <= previous_interior)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "Hazard budget %d does not beat the previous interior's %d (3 per guard, 2 per dog, 2 per mine, 1 per spike and fan)",
                   self->budget, previous_interior);
    }

    /* A later sector must still beat this one. */
    for (int i = number; i < campaign->count; ++i)
    {
        const EdCampaignLevel *after = &campaign->levels[i];
        if (!after->loaded || after->facade != self->facade)
            continue;
        if (after->budget <= self->budget)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "Sector %d comes later on a budget of %d, which no longer beats this sector's %d",
                       i + 1, after->budget, self->budget);
        }
        break;
    }

    /* The alternation: a climb is entered through the window below it. */
    bool climb_expected = previous != NULL && !previous->facade &&
                          previous->has_window;
    if (number == 1)
        climb_expected = false;
    if (previous != NULL || number == 1)
    {
        if (self->facade != climb_expected)
        {
            if (climb_expected)
            {
                report_add(report, ED_SEV_ERROR, -1, -1,
                           "Sector %d hands over through its window, so this one has to be MODE FACADE",
                           number - 1);
            }
            else
            {
                report_add(report, ED_SEV_ERROR, -1, -1,
                           "Nothing hands over to a climb here: sector %d has no window out",
                           number - 1);
            }
        }
    }
    if (!self->facade && self->has_window && next == NULL &&
        number >= campaign->count)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "The campaign ends inside the building, so the last sector cannot open a window onto a climb");
    }

    /* The rocket rota. */
    int expected_bazookas = number % 2 == 0 ? 1 : 0;
    if (self->bazookas != expected_bazookas)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "Sector %d carries %d bazookas; the campaign gives every even-numbered interior exactly one",
                   number, self->bazookas);
    }

    if (campaign->count != ED_CAMPAIGN_LENGTH)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "The campaign is %d sectors; the tests pin %d",
                   campaign->count, ED_CAMPAIGN_LENGTH);
    }
    int facades = 0;
    int restrooms = 0;
    for (int i = 0; i < campaign->count; ++i)
    {
        if (!campaign->levels[i].loaded)
            continue;
        facades += campaign->levels[i].facade;
        restrooms += campaign->levels[i].has_sublevel_entrance;
    }
    if (facades != ED_CAMPAIGN_FACADES)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "The campaign has %d climbs; the tests pin %d", facades,
                   ED_CAMPAIGN_FACADES);
    }
    if (restrooms != ED_CAMPAIGN_RESTROOM_DOORS)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "The campaign has %d restroom doors; the tests pin %d",
                   restrooms, ED_CAMPAIGN_RESTROOM_DOORS);
    }
}

static void check_campaign_contents(int number, const Level *level,
                                    EdReport *report)
{
    bool facade = level->map.mode == LEVEL_MODE_FACADE;
    if (facade)
    {
        if (level->map.facade_hazard_spawn_count < ED_FACADE_MIN_HAZARDS)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "A climb needs at least %d throwers and birds; this one has %d",
                       ED_FACADE_MIN_HAZARDS,
                       level->map.facade_hazard_spawn_count);
        }
        if (level->runtime.item_count < 1 ||
            level->runtime.item_count > ED_FACADE_MAX_ITEMS)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "A climb carries 1 to %d pickups as optional detours; this one has %d",
                       ED_FACADE_MAX_ITEMS, level->runtime.item_count);
        }
        if (level->map.enemy_count > 0)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "Guards do not patrol a wall: remove the %d 'M'/'W'",
                       level->map.enemy_count);
        }
        if (level->map.door_count > 0)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "There are no paired doors on a wall: remove the %d 'D'",
                       level->map.door_count);
        }
        if (count_of(report, '#') <= ED_FACADE_MIN_WALLS)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "A climb is routed around masonry; %d tiles of it is not a route",
                       count_of(report, '#'));
        }
    }
    else
    {
        if (level->map.alarm_switch_count < ED_INTERIOR_MIN_ALARMS)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "An interior needs at least %d alarm switches 'A'; this one has %d",
                       ED_INTERIOR_MIN_ALARMS, level->map.alarm_switch_count);
        }
    }

    if (number == 1)
    {
        if (level->map.civilian_count < 4)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "The lobby empties as Chuck walks in: it needs at least 4 civilians 'f', not %d",
                       level->map.civilian_count);
        }
        if (level->map.receptionist_count != 1)
        {
            report_add(report, ED_SEV_ERROR, -1, -1,
                       "The lobby keeps exactly one receptionist 'k' once the hall has emptied, not %d",
                       level->map.receptionist_count);
        }
    }
}

/* ---- Entry point ------------------------------------------------------- */

void editor_validate(const EditorDoc *doc, const Level *level, bool parsed,
                     const EdCampaign *campaign, EdReport *report)
{
    memset(report, 0, sizeof(*report));
    report->parsed = parsed;

    check_characters(doc, report);
    check_destinations(doc, report);
    check_caps(doc, report);
    check_doors(doc, report);
    check_decorations(doc, report);

    if (doc->grid.facade)
    {
        check_facade(doc, report);
        check_facade_route(doc, report);
    }
    else
    {
        check_fans(doc, report);
        check_spikes(doc, report);
        check_moving_platforms(doc, report);
    }

    if (!doc->grid.has_theme)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "No THEME line, so the map keeps the %s default",
                   doc->grid.facade ? "FACADE_NIGHT" : "PLANT");
    }

    if (!parsed)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "The loader rejects this map, so the game cannot start it");
        return;
    }

    report->budget = level_hazard_budget(level);

    if (!doc->grid.facade)
    {
        check_people(doc, level, report);
        check_route(level, report);
    }

    int number = editor_path_level_number(doc->path);
    if (number >= 1 && number <= ED_MAX_CAMPAIGN && campaign != NULL)
    {
        EdCampaign local = *campaign;
        editor_campaign_record(&local, number, level);
        check_campaign(number, &local.levels[number - 1], &local, report);
        check_campaign_contents(number, level, report);
    }
}
