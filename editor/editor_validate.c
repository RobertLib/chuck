#include "editor_validate.h"

#include "editor_legend.h"

/* For `CAMPAIGN_CLIMB_SECTOR_COUNT` — see the note on ED_CAMPAIGN_* below. */
#include "manual_pages.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The campaign shape the suite pins, and where each number comes from.
 *
 * The first two were `15` and `4` written out here, and they stayed at those
 * values through the edit that made the campaign seventeen sectors with five
 * climbs — so the editor told every author who opened a shipped map that the
 * campaign disagreed with the tests, which was the editor being wrong about the
 * one thing it exists to check. Worse, nothing could see it: `editor_validate`
 * takes the sector number off `doc->path`, the suite's own campaign test handed
 * it a document with no path at all, and the whole cross-sector block was
 * therefore skipped over every shipped map. Both halves are fixed —
 * `test_the_editor_has_nothing_to_say_about_the_shipped_campaign` names the file
 * now — and these two read the constants the maps are held against rather than
 * repeating them.
 *
 * `ED_CAMPAIGN_RESTROOM_DOORS` is deliberately still a literal, because it is a
 * rule rather than a copy: four sectors carry a `U` and each opens on a room of
 * its own, which is what `levels/LEGEND.md` states and
 * `test_embedded_restroom_sublevels` holds the four rooms to. There is no
 * constant elsewhere for it to drift from.
 */
#define ED_CAMPAIGN_LENGTH CAMPAIGN_SECTORS
#define ED_CAMPAIGN_FACADES CAMPAIGN_CLIMB_SECTOR_COUNT
#define ED_CAMPAIGN_RESTROOM_DOORS 4
#define ED_FACADE_MIN_HAZARDS 8
#define ED_FACADE_MIN_WALLS 40
#define ED_FACADE_MAX_ITEMS 4
#define ED_INTERIOR_MIN_ALARMS 2
/* How much of a sector's own walk the cheapest way to unlock its door has to
 * cost. Proportional for the reason the docket sheet's bar is: a floor plan
 * runs from 38 steps to 58, so a fixed number of steps is a decision on one
 * and a rounding error on another. The suite pins the same figure over the
 * shipped campaign in `test_a_locked_door_makes_the_player_look_for_the_key`. */
#define ED_KEY_DETOUR_PERCENT 30
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

/* Wall the loader will stand something on. Props are dropped unless there is a
 * '#' under them, and a patch that may be blown away is deliberately not one:
 * a desk left hanging in mid-air is worse than a desk somewhere else. */
static bool doc_solid(const EditorDoc *doc, int col, int row)
{
    return doc_at(doc, col, row) == '#';
}

/* Everything that stops the player. A weak wall collides exactly as a wall
 * does until an explosion opens it, so every reachability and clearance rule
 * has to count it — which is also what makes a sector work before it is
 * opened, the only state the route model is allowed to assume.
 *
 * A duct counts too, and for the same reason one step further on: it stops a
 * man on his feet, and standing is the state every clearance rule here is
 * written about. This is `route_masonry` in src/level_route.c said again on
 * this side of the fence, and the two have to keep agreeing — a sector the
 * editor calls solvable that `make test` then rejects is worse than no check
 * at all. */
static bool doc_blocks(const EditorDoc *doc, int col, int row)
{
    char cell = doc_at(doc, col, row);
    return cell == '#' || cell == '%' || cell == '=';
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

/*
 * Lift shafts, counted the way `level_load_data` counts them rather than by
 * tile: a shaft is a *run* of two or more `V` down one column, and a lone `V`
 * builds nothing at all. Counting the character instead would report a
 * fourteen-tile shaft as fourteen lifts and fail a map that is well inside the
 * limit, which is worse than the silence this check replaces.
 */
static int count_elevator_shafts(const EditorGrid *grid)
{
    int shafts = 0;
    for (int col = 0; col < grid->width; ++col)
    {
        int row = 0;
        while (row < grid->height)
        {
            if (grid->cells[row][col] != 'V')
            {
                ++row;
                continue;
            }
            int first = row;
            while (row < grid->height && grid->cells[row][col] == 'V')
                ++row;
            if (row - 1 > first)
                ++shafts;
        }
    }
    return shafts;
}

static void check_caps(const EditorDoc *doc, EdReport *report)
{
    /* A `Q` is a guard as well — the same `place_enemy` into the same
     * `enemy_spawns` — so he belongs in this sum and not only in the hazard
     * budget. Left out, as he was from the day he was drawn, a floor could be
     * painted past the ceiling entirely in heavies and the loader would drop
     * them without this report saying a word. */
    check_one_cap(report,
                  count_of(report, 'M') + count_of(report, 'W') +
                      count_of(report, 'Q'),
                  MAX_ENEMIES, "guards");
    /* A `W` is a guard *and* his dog, so it is counted against both ceilings.
     * The loader takes the dog through `find_dog_slot`, which hands back
     * nothing once the array is full — the same silent drop as every other cap
     * on this list, and the reason this line and the lift one below were worth
     * adding: they were the two the list was missing. */
    check_one_cap(report, count_of(report, 'W'), MAX_DOGS, "guard dogs");
    /*
     * And a floor has to seat the men it can *send for*, not only the ones
     * drawn on it. A console hacked under the alarm calls up to
     * `TERMINAL_REINFORCEMENT_MAX_COUNT` out of a door, once per console, and
     * they need slots beside the men already standing there and the corpses of
     * the ones that have gone down.
     *
     * Over the ceiling nothing fails, which is why this needs saying out loud:
     * `find_enemy_slot` hands the arrival the corpse furthest from Chuck, so
     * the floor quietly deletes a body and with it the thing
     * `update_body_discovery` sends the next calm guard over to look at. A
     * warning rather than an error, because the map loads and plays — it just
     * stops playing the way it reads.
     *
     * **And an arrival is not only a console's.** `SPAWNS` is the other way men
     * come out of a door, it is the *deterministic* one — `game.c` copies the
     * line into `door_spawns` at level start and `gameplay_ai_update_spawns`
     * drips them out on a timer with no alarm required — and it went into
     * neither this sum nor the suite's for as long as both existed. Measured on
     * the shipped campaign, sector 14 reaches 21 of `MAX_ENEMIES` and this
     * check reported 18; the omission cost half the floor's headroom. Worse,
     * the `called > 0` guard this replaces meant a map whose whole
     * over-subscription came from `SPAWNS` was never warned about at all:
     * `SPAWNS 9999 9999` on a floor with no console produced the same report as
     * `SPAWNS 0 0`, and nothing bounds the value the parser accepts.
     *
     * Both paths land in `spawn_enemy_from_door`, so they are one sum here.
     */
    int consoles = count_of(report, 'T');
    int doors = count_of(report, 'D');
    int dripped = 0;
    if (doc->grid.has_spawns)
    {
        for (int i = 0; i < doc->grid.spawn_count && i < MAX_DOORS; ++i)
            dripped += doc->grid.spawns[i];
    }
    int called = doors > 0 ? consoles * TERMINAL_REINFORCEMENT_MAX_COUNT : 0;
    int arrivals = called + dripped;
    int drawn = count_of(report, 'M') + count_of(report, 'W') +
                count_of(report, 'Q');
    if (drawn + arrivals > MAX_ENEMIES)
    {
        report_add(report, ED_SEV_WARN, -1, -1,
                   "%d men could stand here at once — %d drawn, %d called by "
                   "%d console(s), %d out of SPAWNS — but the array seats %d, "
                   "so an arrival overwrites a body",
                   drawn + arrivals, drawn, called, consoles, dripped,
                   MAX_ENEMIES);
    }
    /*
     * And the dogs answer the same sum, which is the half this check never had.
     * `spawn_enemy_from_door` rolls `DOG_DOOR_HANDLER_CHANCE` on every arrival
     * from either path, so the worst case is one handler apiece — and
     * `find_dog_slot` deletes a *dog's* corpse out of a full array for exactly
     * the reason the men's line above says not to. The authored `W` alone was
     * held (a line up from here); what a floor can send for was not.
     */
    if (count_of(report, 'W') + arrivals > MAX_DOGS)
    {
        report_add(report, ED_SEV_WARN, -1, -1,
                   "%d dogs could stand here at once — %d drawn plus a handler "
                   "with each of %d arrival(s) — but the array seats %d, so an "
                   "arrival overwrites a body",
                   count_of(report, 'W') + arrivals, count_of(report, 'W'),
                   arrivals, MAX_DOGS);
    }
    /* Every character that reaches `place_item`, which is what the ceiling is
     * on: the docket sheet and the flash charge go into the same array as the
     * cards and the magazines. */
    check_one_cap(report,
                  count_of(report, 'C') + count_of(report, 'G') +
                      count_of(report, 'N') + count_of(report, 'K') +
                      count_of(report, 'Z') + count_of(report, '*') +
                      count_of(report, '!'),
                  MAX_ITEMS, "items");
    check_one_cap(report, count_of(report, 'X'), MAX_MINES, "mines");
    check_one_cap(report, count_of(report, '^'), MAX_SPIKES, "spikes");
    check_one_cap(report, count_of(report, 'O'), MAX_CEILING_FANS, "fans");
    check_one_cap(report, count_of(report, 'T'), MAX_TERMINALS, "terminals");
    check_one_cap(report, count_of(report, 'A'), MAX_ALARM_SWITCHES,
                  "alarm switches");
    /* `MAX_CAMERAS` is the tightest ceiling on this list, and it was the one
     * with no line at all. */
    check_one_cap(report, count_of(report, 'I'), MAX_CAMERAS, "cameras");
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
    check_one_cap(report, count_elevator_shafts(&doc->grid), MAX_ELEVATORS,
                  "lift shafts");
    check_one_cap(report, count_of(report, 'r') + count_of(report, 'v'),
                  MAX_FACADE_HAZARD_SPAWNS, "facade hazards");
    /*
     * The props, and this is deliberately not a list of them.
     *
     * It was one — fifteen `count_of` calls — and that made it the fifth copy of
     * a list that already lives in the legend, the parser, the editor's palette
     * and `check_docs.py`. It fell behind the day the plant set was added: `a`
     * `e` `j` `l` are decorations the loader counts against `MAX_DECORATIONS`
     * and this sum did not, so a floor painted past the ceiling in pallets was a
     * floor the editor called clean while the loader quietly dropped the
     * overflow. Asked of the palette instead: every symbol filed under one of
     * the prop bins is a prop, so a sixth set is counted by having been given a
     * bin.
     */
    int props = 0;
    for (int i = 0; i < ED_SYMBOL_COUNT; ++i)
    {
        switch (ED_SYMBOLS[i].group)
        {
        case ED_GROUP_OFFICE:
        case ED_GROUP_LOBBY:
        case ED_GROUP_RESTROOM:
        case ED_GROUP_PLANT:
        case ED_GROUP_NIGHT:
            props += count_of(report, ED_SYMBOLS[i].symbol);
            break;
        case ED_GROUP_TERRAIN:
        case ED_GROUP_ROUTE:
        case ED_GROUP_ITEMS:
        case ED_GROUP_ENEMIES:
        case ED_GROUP_PEOPLE:
        case ED_GROUP_HAZARDS:
        case ED_GROUP_FITTINGS:
        case ED_GROUP_FACADE:
        case ED_GROUP_COUNT:
            break;
        }
    }
    check_one_cap(report, props, MAX_DECORATIONS, "decorations");

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
    int unhung = 0;
    int first_col = -1;
    int first_row = -1;
    int hung_col = -1;
    int hung_row = -1;
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            char c = doc_at(doc, col, row);
            const EdSymbol *symbol = editor_symbol(c);
            if (symbol == NULL)
                continue;
            if (symbol->group != ED_GROUP_OFFICE &&
                symbol->group != ED_GROUP_LOBBY &&
                symbol->group != ED_GROUP_RESTROOM &&
                symbol->group != ED_GROUP_NIGHT)
            {
                continue;
            }
            /* A hanging prop asks the tile above; everything else asks the
             * tile below. Both are the same rule the loader applies, and both
             * end the same way — the prop is silently dropped. */
            if (editor_symbol_hangs(c))
            {
                if (doc_solid(doc, col, row - 1))
                    continue;
                unhung++;
                if (hung_col < 0)
                {
                    hung_col = col;
                    hung_row = row;
                }
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
    if (unhung == 1)
    {
        report_add(report, ED_SEV_WARN, hung_col, hung_row,
                   "This prop hangs from the slab above it, and there is none");
    }
    else if (unhung > 1)
    {
        report_add(report, ED_SEV_WARN, hung_col, hung_row,
                   "%d hanging props have no slab above them; the loader drops every one",
                   unhung);
    }
}

/*
 * The flight cases, and the one thing they are for.
 *
 * An `m` is a stencilled *Meridian Facility Services* box — one of the ones the
 * crew wheeled in through the goods entrance in March and nobody inspected. It
 * is scenery in every mechanical sense: non-solid, no score, nothing in the
 * simulation reads it. What it does is carry the plot. `levels/LEGEND.md` says
 * how: *the case the bazooka came out of, two tiles from the bazooka, is the
 * whole plot said without a line of text.*
 *
 * Which made it worth asking whether the campaign was doing it, and the answer
 * was mostly. Nine interiors put their case within two tiles of a grenade or a
 * bazooka; sector 8 had its nineteen tiles away and sector 14 eleven, so on those
 * two floors the box was a box. Sector 12 was the third and it was found later, at
 * seventeen: its one case sat at the near mouth of the sixteen-tile duct run and
 * the bazooka at the far end of it, which reads as two unrelated props rather than
 * as one sentence. It is five tiles along the same corridor now — the case moved
 * rather than the bazooka, because where that rocket sits is the reward for
 * crawling the shaft and the case is scenery.
 *
 * **Asked of the sector rather than of each case**, which is the distinction the
 * legend already draws two sentences earlier: the variants are chosen from the
 * tile position *"so a run of them is not a run of the same box"*, which sanctions
 * a run. The roof's service deck is exactly that — four cases along the length of
 * it, three of them nowhere near the one grenade, and rightly so. One case making
 * the point is the point; every case making it would be the same sentence four
 * times.
 *
 * A note, not a warning, because it is what the legend calls it: *worth* putting
 * near one. A floor that keeps its cases away from the explosives still loads and
 * still plays. It just stops saying the thing they are there to say.
 */
#define ED_CASE_NEAR_EXPLOSIVE 6

static void check_flight_cases(const EditorDoc *doc, EdReport *report)
{
    int cases = 0;
    int first_col = -1;
    int first_row = -1;
    int nearest = -1;

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'm')
                continue;
            cases++;
            if (first_col < 0)
            {
                first_col = col;
                first_row = row;
            }
            for (int r = 0; r < doc->grid.height; ++r)
            {
                for (int c = 0; c < doc->grid.width; ++c)
                {
                    char other = doc_at(doc, c, r);
                    /* The two things that open a `%` and the two things that
                     * come out of a case. A flash charge is deliberately not on
                     * this list: the legend is explicit that `!` is not a
                     * weapon, and a case it came out of would be a case with
                     * nothing in it worth the sentence. */
                    if (other != 'N' && other != 'Z')
                        continue;
                    int distance = abs(c - col) + abs(r - row);
                    if (nearest < 0 || distance < nearest)
                        nearest = distance;
                }
            }
        }
    }

    /* No cases is no claim, and no explosive means the sector cannot make the
     * point at all — sectors 1 and 5 carry neither a grenade nor a bazooka, and
     * telling their author to move a box nearer to nothing would be a note that
     * cannot be acted on. */
    if (cases == 0 || nearest < 0)
        return;
    if (nearest <= ED_CASE_NEAR_EXPLOSIVE)
        return;

    report_add(report, ED_SEV_NOTE, first_col, first_row,
               "The nearest flight case is %d tiles from an explosive; within "
               "%d is what makes it read as the case one came out of",
               nearest, ED_CASE_NEAR_EXPLOSIVE);
}

/* ---- Authoring rules for interiors ------------------------------------ */

/* The floor the band containing (col,row) stands on, or -1 for open air. */
static int band_floor_row(const EditorDoc *doc, int col, int row, int spread)
{
    for (int r = row + 1; r < doc->grid.height; ++r)
    {
        for (int c = col - spread; c <= col + spread; ++c)
        {
            if (doc_blocks(doc, c, r))
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
                if (c < 0 || c >= doc->grid.width || doc_blocks(doc, c, floor))
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

/*
 * Mines, which had no authoring rule of any kind.
 *
 * The hazard that costs the most had the least said about it: a fan takes one
 * heart and `check_fans` above asks five questions of it, a spike takes one and
 * `check_spikes` below asks one, and an `X` takes **two of three** — an
 * ordinary blast, the same one that opens a `%` — and nothing asked anything.
 * The route model does not help either: it knows a spike (`RouteMap.spike`, and
 * a hop rule for one) and has no notion that a mine exists, so it certifies a
 * path straight over every one of them.
 *
 * What a mine is *for* is that it makes the player look at the floor, and the
 * answers are all still there — see it and step round it, hop it, or set it off
 * from a distance with anything that explodes. So this is not a rule about
 * where a mine may go. It is a rule about a mine whose answers have been taken
 * away by something else on the plan, which is the one thing no rule about a
 * single tile can see and the reason the fan's checks read the way they do.
 */
static void check_mines(const EditorDoc *doc, EdReport *report)
{
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'X')
                continue;

            /*
             * Blades over a mine close the mine's own way out. Hopping an `X`
             * is the free answer to it, a hop puts the player a row up, and
             * `CEILING_FAN_BLADE_LENGTH` reaches better than a tile either side
             * of the fan's column — so the jump that clears the charge lands in
             * the fan and the walk that ducks the fan lands on the charge. One
             * heart or two, and the only way past for nothing is to spend a
             * throwable on it.
             *
             * This is `A crate under the blades is a step into them` with the
             * prop swapped, and the mine is the worse of the two.
             */
            for (int r = row - 2; r <= row; ++r)
            {
                if (doc_at(doc, col, r) != 'O')
                    continue;
                report_add(report, ED_SEV_WARN, col, row,
                           "Blades over this mine: hopping it goes into them "
                           "and ducking them goes onto it");
                break;
            }

            /*
             * And a mine right against a riser is stepped on rather than walked
             * into. A ladder or a shaft is ridden looking at the rungs, the
             * charge is on the floor, and the arrival is a step sideways with
             * no tile in between to stop on and no run-up to hop from.
             *
             * **One column, not the two `check_fans` uses**, and the difference
             * is the whole of why that number is not copied. Blades *reach*:
             * `CEILING_FAN_BLADE_LENGTH` hangs better than a tile either side of
             * the fan's own column, so a fan two columns off a ladder genuinely
             * overhangs the arrival and the span is measuring something. A mine
             * has no reach at all — it is exactly its tile — so a span of two
             * would be a number borrowed from a rule whose justification does
             * not come with it, which is this repository's most reliable way of
             * being wrong. Measured, ±2 flags six tiles across four of the seven
             * mined floors and ±1 flags the two where the step really has
             * nowhere to land.
             */
            char riser = 0;
            for (int side = -1; side <= 1 && riser == 0; side += 2)
            {
                for (int step = 1; step <= 1; ++step)
                {
                    char cell = doc_at(doc, col + side * step, row);
                    /* Walked outward and stopped at masonry rather than
                     * measured across it. Sector 8 keeps its rocket pocket one
                     * column from a ladder with the pocket's own wall in
                     * between — two columns apart and no way from one to the
                     * other, which a straight span reports and a step-off
                     * cannot happen from. `check_fans` measures a span for
                     * blades that genuinely overhang; a floor a man steps onto
                     * is a different question and wants walking. */
                    if (doc_blocks(doc, col + side * step, row))
                        break;
                    if (cell == 'H' || cell == 'V')
                    {
                        riser = cell;
                        break;
                    }
                    /* And the *top* of one, which is the same arrival read off
                     * the row below: a rung column that ends under this floor
                     * is climbed up to and stepped off at this height, so the
                     * charge is met facing the wrong way exactly as it is at
                     * the foot. Looking only at the mine's own row made this
                     * silent on half the risers in the building. */
                    char below = doc_at(doc, col + side * step, row + 1);
                    if (below == 'H' || below == 'V')
                    {
                        riser = below;
                        break;
                    }
                }
            }
            if (riser != 0)
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "This mine is two columns off the %s: it is stepped "
                           "on coming off it, not walked into",
                           riser == 'H' ? "ladder" : "lift shaft");
            }

            /*
             * Two abreast is the spike's rule and it bites harder here. A
             * single charge is hoppable, a pair is not, so the floor is cut in
             * half at a cost of two hearts rather than one — and unlike a spike
             * bed, which pops the boots back out, a blast throws the player.
             */
            if (doc_at(doc, col + 1, row) == 'X')
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "Two mines abreast cannot be hopped; the floor is "
                           "cut in half here at two hearts");
            }

            /*
             * A charge with nothing under it is the prop rule again: only the
             * player's weight arms an `X`, and weight arrives on a floor. Left
             * in open air it is a thing drawn on the map that nothing can ever
             * reach, which is the same finding an unreachable pickup gets.
             */
            if (band_floor_row(doc, col, row, 0) != row + 1)
            {
                report_add(report, ED_SEV_NOTE, col, row,
                           "Nothing directly under this mine for the player to "
                           "stand on and arm it");
            }
        }
    }
}

/*
 * A standing player fills exactly one tile, which is what lets the check below
 * ask about cells rather than about boxes: his centre is the centre of the tile
 * his feet are on. The whole of `camera_watches_cell` rests on that, so it is
 * asserted rather than assumed.
 */
_Static_assert((int)PLAYER_H == TILE_SIZE,
               "the camera check reads a standing player as one tile tall");

/*
 * Somewhere a standing player can be, read off the grid rather than off the
 * route model.
 *
 * Deliberately generous: a rung and a lift shaft carry a man with no floor
 * under him, and a plate is floor that happens to be moving. Every tile that
 * counts here is one more place a camera might legitimately be watching, so
 * erring wide is erring towards saying nothing — which is the right direction
 * for a rule about whether a fitting is pointed at anything at all.
 *
 * `F` is the one left out, for the reason the route model leaves it out: a map
 * has to still work once every panel has gone, so a camera whose only subject
 * is a falling panel is watching a floor that is about to stop existing.
 *
 * Measured, none of the generous arms is load-bearing on the campaign as it
 * ships: every shipped camera watches masonry-floored ground, so cutting this
 * function back to `doc_blocks(col, row + 1)` alone flags nothing. That is
 * worth writing down rather than leaving as an implication — the arms are here
 * for the author who points one down a rung column, which is a legitimate thing
 * to do and which the narrow rule would refuse. `test_the_editor_reports_a_broken_map`
 * drives the ladder arm on a fixture rather than trusting it, because an arm
 * nothing reaches is an arm nobody has checked.
 */
static bool doc_can_stand(const EditorDoc *doc, int col, int row)
{
    if (doc_blocks(doc, col, row))
        return false;
    char here = doc_at(doc, col, row);
    if (here == 'H' || here == 'V')
        return true;
    if (doc_blocks(doc, col, row + 1))
        return true;
    char below = doc_at(doc, col, row + 1);
    return below == 'H' || below == 'V' || below == 'P';
}

/*
 * Would a camera at (cc,cr) ever have a standing player in cell (col,row) in
 * shot?
 *
 * The sweep needs no sampling, which is worth saying because the first draft of
 * this walked it in two hundred steps. `camera_sees_player` tests
 * `cos(theta - a) >= cos(CAMERA_CONE_HALF_ANGLE)`, where `theta` is the target's
 * angle from straight down and `a` is the beam's; `gameplay_camera_angle` covers
 * every `a` in `[-CAMERA_SWEEP_ARC, +CAMERA_SWEEP_ARC]` on the way round. So
 * "seen at some moment" is exactly `|theta| <= arc + half`, and the closed form
 * is not an approximation of the loop, it is the loop's limit.
 *
 * `dy > 0` is the clause that carries the finding this check exists for, and it
 * is the lens's own rule rather than a simplification: nothing at or above the
 * mounting is ever in shot. A camera hung in a one-tile-high space is therefore
 * blind to anybody standing in that space, because a standing man's centre is
 * level with it to the pixel.
 *
 * **The arc is dominated and is kept anyway**, which is worth writing down
 * rather than leaving as an untested line. Tile centres are 32 apart, so the
 * only offset the sweep excludes that the range does not is five tiles across
 * and one down — `atan2(160, 32)` is 1.3734 against a limit of 1.37, a margin
 * of 0.2% — and reaching that cell needs the tiles between it and the lens
 * open, which on a grid nearly always puts nearer standable floor in shot
 * first. So no fixture isolates it without balancing on that margin, and one
 * that did would fail on a hair's change to either constant rather than on a
 * defect. It stays because it is the lens's own rule: a check that drops a
 * clause of the thing it models is a check that disagrees with the game the
 * moment `CAMERA_RANGE` or `CAMERA_SWEEP_ARC` moves. What is pinned instead is
 * the relationship — see `test_the_editor_reports_a_broken_map`, which requires
 * the arc to bite at five tiles across and not at four.
 */
static bool camera_watches_cell(const EditorDoc *doc, int cc, int cr,
                                int col, int row)
{
    float cx = ((float)cc + 0.5f) * (float)TILE_SIZE;
    float cy = ((float)cr + 0.5f) * (float)TILE_SIZE;
    float px = ((float)col + 0.5f) * (float)TILE_SIZE;
    float py = ((float)row + 0.5f) * (float)TILE_SIZE;
    float dx = px - cx;
    float dy = py - cy;
    if (dy <= 0.0f)
        return false;
    float distance = sqrtf(dx * dx + dy * dy);
    if (distance > CAMERA_RANGE)
        return false;
    if (fabsf(atan2f(dx, dy)) > CAMERA_SWEEP_ARC + CAMERA_CONE_HALF_ANGLE)
        return false;
    /* The same walk `gameplay_sight_line_clear` makes, at the same step, over
     * the same three solid tiles `level_is_solid` answers for — which is what
     * `doc_blocks` already is. */
    int steps = (int)(distance / ENEMY_LOS_STEP) + 1;
    for (int s = 1; s < steps; ++s)
    {
        float t = (float)s / (float)steps;
        if (doc_blocks(doc, (int)floorf((cx + dx * t) / (float)TILE_SIZE),
                       (int)floorf((cy + dy * t) / (float)TILE_SIZE)))
            return false;
    }
    return true;
}

/*
 * The camera was the one hazard on the legend with a placement rule and nothing
 * measuring it, and both halves of that were live on the shipped campaign.
 *
 * **It hangs, and nothing said so.** `level_load_data` drops a camera with no
 * slab above it, and the comment beside that loop says the editor "says so while
 * it is being drawn". It did not. `editor_symbol_hangs` answers for the wall
 * clock `w` and the camera `I`, and its only caller was inside
 * `check_decorations`, which filters to the four decoration groups — so the
 * clock was caught and the camera, an `ED_GROUP_FITTINGS`, was never asked. A
 * function that answers for two things, reached from a place that can only ever
 * see one of them: the rationale claimed the cover and the call site did not
 * deliver it.
 *
 * **And a camera can be pointed at nothing.** `levels/LEGEND.md` says to place
 * one "where the beam crosses ground the player has to walk, not over a dead
 * end: a camera nobody has to pass is a fitting rather than an obstacle". That
 * was prose for as long as the mechanic existed, and on the DUCTS floor it was
 * false twice: sector 12 hung both of its cameras in the one-tile service gap
 * over a duct run, where the tile below the lens is trunking and therefore
 * solid. Measured, each watched **nought** cells a standing player could occupy
 * against five to fourteen for every other camera in the game, and the only
 * thing either could see at all was a crawler within about a tile — on the
 * fitting whose own manual sheet promises "it looks down - crawling is no help".
 * They cost two of the hazard budget each, so the floor's stated pressure was
 * four higher than the floor it described.
 *
 * **Reachability is deliberately not the bar, and neither is the shortest
 * route.** Seven of the campaign's eleven cameras watch only ground that costs
 * a detour of +10 to +61 steps, and every one of those is a camera guarding a
 * pickup or a side route, which is exactly what the fitting is for. The
 * question that separates an obstacle from an ornament is not *how far* the
 * ground is but whether there is any: a beam with nowhere to land is watching
 * nothing at any distance.
 */
static void check_cameras(const EditorDoc *doc, EdReport *report)
{
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'I')
                continue;

            /* Asked first, because a camera the loader throws away has no beam
             * to say anything about and two findings on one tile would bury the
             * one the author can act on. */
            if (!doc_blocks(doc, col, row - 1))
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "This camera hangs from the slab above it, and "
                           "there is none, so the loader drops it");
                continue;
            }

            /*
             * Every row, not just the ones under the mounting. The first draft
             * started this loop at `row + 1`, which reads as an optimisation and
             * is really a second copy of `camera_watches_cell`'s own `dy > 0`
             * rule — with the loop bounded, that clause was dead code and
             * deleting it changed no answer. A check that quietly re-states the
             * rule it is testing is a check that agrees with the bug.
             */
            int watched = 0;
            for (int r = 0; r < doc->grid.height && watched == 0; ++r)
            {
                for (int c = 0; c < doc->grid.width; ++c)
                {
                    if (!doc_can_stand(doc, c, r))
                        continue;
                    if (!camera_watches_cell(doc, col, row, c, r))
                        continue;
                    watched++;
                    break;
                }
            }
            if (watched == 0)
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "This camera watches no floor a standing player can "
                           "occupy, so it is a fitting rather than an obstacle");
            }
        }
    }
}

/*
 * Falling panels, and the question nobody had asked about them: what is *under*
 * one.
 *
 * Every rule this tree has about an `F` is about the fallen state of the **map**
 * — the parser makes the tile air and hangs the panel off the runtime, so the
 * route model already judges a sector as though every panel had gone, which is
 * what `levels/LEGEND.md` means by "a shortcut, never a lifeline". Not one of
 * them is about what happens to the *player* on the way down, and two of the
 * campaign's five panelled floors were paying for that.
 *
 * **Sector 9's pair was an outright death.** A player standing on a panel has
 * his feet at the top of its tile, so he is standing in the row *above* it and
 * the drop is measured from there — 192px into the archive hall against a
 * `PLAYER_FATAL_FALL_HEIGHT` of 160. Measured, he arrives at 592px/s against a
 * `PLAYER_FATAL_FALL_SPEED` of 560: three hearts, which is the life, not a hit.
 * Every column of that slab gives the same drop, so there was no better place
 * for it and the panels are masonry now. The sharp part is that **the route
 * model already knew** — `route_survivable_fall(5, 11)` is false, so the model
 * refused to route through the hole and certified the sector by another path,
 * while the player could walk into it and nothing told the author. A model that
 * refuses a move is not a warning to whoever drew it.
 *
 * **And sector 17's dropped through a ceiling fan**, which is one heart every
 * single time the mechanic is used. `check_fans` asks five questions and every
 * one of them looks *down* from the blades — a hole in the floor below, a crate
 * below, a ladder beside — so a hole in the slab directly *above* a fan was the
 * one arrangement none of them could see. That is this file's most reliable
 * shape: one half of a symmetric pair, checked.
 *
 * The severities differ on the same argument `check_opened_walls_leave_a_way_out`
 * uses. A fatal drop is an **error** because it is not "the floor will not play
 * the way it reads", it is a floor that can eat a run. Blades or a spike bed on
 * the way down are a **warning**: they cost a heart, the sector still finishes,
 * and an author may well mean it.
 */
static void check_panels(const EditorDoc *doc, EdReport *report)
{
    for (int row = 1; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != 'F')
                continue;

            /* Feet on the panel's own top edge, so the fall starts one row up.
             * Getting this off by a row is the difference between 160px and
             * 192px, which on sector 9 was the difference between a bruise and
             * the run. */
            int from_row = row - 1;
            int landing = -1;
            for (int r = row + 1; r < doc->grid.height; ++r)
            {
                /* Masonry right under the panel: it is a floor tile over a
                 * floor, so there is no fall to ask about. */
                if (doc_blocks(doc, col, r))
                    break;
                if (doc_can_stand(doc, col, r))
                {
                    landing = r;
                    break;
                }
            }
            if (landing < 0)
                continue;

            /*
             * Blades in the drop, and the span is one column either side rather
             * than the panel's own — measured rather than copied. A fan's hazard
             * band reaches `CEILING_FAN_BLADE_LENGTH` (23px) each way from its
             * tile centre and the falling box is 26 wide in a 32 tile, so a fan
             * one column over still overlaps it. Swept across every x a player
             * can rest on a panel at, ±1 costs a heart and ±2 costs nothing,
             * which is also what says the roof's fix — the fan moved two columns
             * — is far enough by design rather than by luck.
             *
             * Deliberately not the ±2 `check_fans` uses for a ladder: that span
             * is about a climber stepping *off* into the blades, which starts
             * from a tile the fan may overhang. This one is a body falling down
             * a known column, and borrowing a number whose justification does
             * not come with it is the mistake `check_mines` is written up for.
             */
            int blades = -1;
            int blades_col = -1;
            for (int r = row + 1; r <= landing && blades < 0; ++r)
            {
                for (int c = col - 1; c <= col + 1; ++c)
                {
                    if (doc_at(doc, c, r) != 'O')
                        continue;
                    blades = r;
                    blades_col = c;
                    break;
                }
            }

            /* The model's own rule rather than a second copy of the
             * arithmetic — `route_survivable_fall` takes two rows and nothing
             * else, so there is no reason for this to guess at it. */
            if (!route_survivable_fall(from_row, landing))
            {
                report_add(report, ED_SEV_ERROR, col, row,
                           "This panel drops the player %d px onto row %d, "
                           "past the %d px a landing is survived: it kills "
                           "outright rather than costing a heart",
                           (landing - from_row) * TILE_SIZE, landing,
                           (int)PLAYER_FATAL_FALL_HEIGHT);
                continue;
            }
            if (blades >= 0)
            {
                /* Reported at the *fan*, because that is the tile that has to
                 * move: a panel is a hole in a slab and usually has only the
                 * one column it can occupy, while blades can go anywhere two
                 * columns clear of the falling player's box. */
                report_add(report, ED_SEV_WARN, blades_col, blades,
                           "This fan is in the drop of the panel on row %d, so "
                           "using it costs a heart every time",
                           row);
            }
            if (doc_at(doc, col, landing) == '^')
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "This panel drops the player onto a spike bed on "
                           "row %d",
                           landing);
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

/*
 * Weak walls.
 *
 * The route model counts a '%' as wall, so the sector is judged in the state it
 * is authored in and a blocked-up opening can never be the way out. That makes
 * two things worth saying out loud: a patch nothing in the sector can open is
 * scenery, and a patch let into nothing is a block standing in mid-air.
 */
/*
 * Whether a charge already lying on the floor can bring this tile down where it
 * is.
 *
 * A gas canister opens a patch exactly as a grenade does — every blast in the
 * game goes through `apply_blast` and that function deliberately does not pick
 * which of the things beside it are real — so a canister within reach of a patch
 * is a way to open that patch, and a good one: the manual teaches shooting one
 * by name and it costs the sector no explosive at all.
 *
 * **A mine is not, and that is the distinction this rule turns on.** Nothing can
 * set one off but the player's own weight or somebody else's blast: it cannot be
 * shot. So a floor whose only opener is a mine offers the player one way through
 * the wall — standing on two of three hearts — and telling an author that patch
 * is provided for would be worse than saying nothing.
 *
 * Measured tile centre to tile centre and straight through masonry, because that
 * is what `gameplay_break_walls_in_radius` does: a blast has no line of sight to
 * lose. A canister rests on the floor of its tile, so its real centre is a little
 * below the one used here and this errs very slightly towards saying yes — which
 * is the right direction for a warning whose whole failure mode was firing when
 * it should not.
 */
static bool a_fixed_charge_reaches(const EditorDoc *doc, int col, int row)
{
    /* The furthest a fixed charge can reach, in whole tiles, so the scan is
     * bounded by the mechanic rather than by a number chosen here. */
    int span = (int)(GAS_CANISTER_RADIUS / (float)TILE_SIZE) + 1;
    float cx = ((float)col + 0.5f) * (float)TILE_SIZE;
    float cy = ((float)row + 0.5f) * (float)TILE_SIZE;

    for (int r = row - span; r <= row + span; ++r)
    {
        for (int c = col - span; c <= col + span; ++c)
        {
            if (doc_at(doc, c, r) != 'L')
                continue;
            float bx = ((float)c + 0.5f) * (float)TILE_SIZE;
            float by = ((float)r + 0.5f) * (float)TILE_SIZE;
            float dx = bx - cx;
            float dy = by - cy;
            if (dx * dx + dy * dy <= GAS_CANISTER_RADIUS * GAS_CANISTER_RADIUS)
                return true;
        }
    }
    return false;
}

static void check_weak_walls(const EditorDoc *doc, EdReport *report)
{
    int patches = count_of(report, '%');
    if (patches == 0)
        return;

    /*
     * What can open a patch, asked of the mechanic rather than of a list.
     *
     * This used to be `no 'N' and no 'Z'` and it said "nothing to open them
     * with", which is a claim about the floor and was false on any floor
     * carrying a gas canister beside the patch: all four blasts in the game
     * — grenade, rocket, mine, canister — go through `apply_blast` and open a
     * `%`, which is what [levels/LEGEND.md](../levels/LEGEND.md) has always
     * said. Because the suite holds the shipped campaign to nought warnings,
     * a check that is wrong in this direction does not merely misinform: it
     * forbids a design the game supports, and a good one, since a patch opened
     * by shooting the canister next to it costs the sector's only rocket
     * nothing.
     *
     * A grenade or a rocket can be carried to any patch on the plan, so either
     * settles the whole floor and nothing further needs measuring. A canister
     * cannot be carried, so what settles it is a distance — see
     * `a_fixed_charge_reaches`, which also says why a mine is not counted.
     */
    bool portable = count_of(report, 'N') > 0 || count_of(report, 'Z') > 0;
    int stranded = 0;
    int stranded_col = -1;
    int stranded_row = -1;

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != '%')
                continue;

            if (!portable && !a_fixed_charge_reaches(doc, col, row))
            {
                if (stranded == 0)
                {
                    stranded_col = col;
                    stranded_row = row;
                }
                ++stranded;
            }

            bool let_in = doc_blocks(doc, col - 1, row) ||
                          doc_blocks(doc, col + 1, row) ||
                          doc_blocks(doc, col, row - 1) ||
                          doc_blocks(doc, col, row + 1);
            if (!let_in)
            {
                report_add(report, ED_SEV_NOTE, col, row,
                           "This patch has no wall to be let into, so it reads "
                           "as a block standing in mid-air");
            }
        }
    }

    /* Reported at the first one rather than as a fact about the map, because the
     * author's next question is which patch. */
    if (stranded > 0)
    {
        report_add(report, ED_SEV_WARN, stranded_col, stranded_row,
                   /* Kept inside ED_FINDING_LEN with the widest pair of numbers
                    * in it: an author-facing sentence cut off mid-clause is a
                    * refusal nobody can act on. */
                   "%d of %d patches have no blast to open them: bring one in "
                   "('N' or 'Z'), or set a canister 'L' beside the patch. "
                   "A mine needs a boot, so it does not count",
                   stranded, patches);
    }
}

/*
 * Trunking, which is the one tile a posture changes the answer to — wall
 * standing, a way through crawling.
 *
 * A crawl is horizontal, so what holds the player up inside a duct is whatever
 * the map put underneath it. That makes the floor under a run part of the run,
 * and its absence the one mistake here that no other rule can see: the tile
 * loads, draws as trunking, and drops the player out of the bottom of it. A
 * warning rather than a note, because it will not play the way it reads.
 *
 * The mouths are the route model's business (`route_in_duct` reaches a duct only
 * from a tile the player can stand on and leaves it only onto another), so a
 * shaft with one mouth already shows up as whatever it strands.
 */
static void check_ducts(const EditorDoc *doc, EdReport *report)
{
    if (count_of(report, '=') == 0)
        return;

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            if (doc_at(doc, col, row) != '=')
                continue;

            /* A duct underneath a duct is not a floor: a crawl carries nobody
             * downward, so the player falls through both. */
            char below = doc_at(doc, col, row + 1);
            if (below != '#' && below != '%')
            {
                report_add(report, ED_SEV_WARN, col, row,
                           "This duct has no floor under it, so a crawl drops "
                           "out of the bottom of it: put a wall '#' below");
            }

            /* A run sealed at both ends by masonry is trunking nobody can be
             * inside, which is a picture rather than a route. */
            bool open_side = !doc_blocks(doc, col - 1, row) ||
                             !doc_blocks(doc, col + 1, row) ||
                             doc_at(doc, col - 1, row) == '=' ||
                             doc_at(doc, col + 1, row) == '=';
            if (!open_side)
            {
                report_add(report, ED_SEV_NOTE, col, row,
                           "This duct is walled in on both sides, so nobody can "
                           "crawl into it");
            }
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
                if (cell == '#' || cell == '%' || cell == 'D' || cell == 'V')
                    break;
                left--;
            }
            int right = col;
            while (right + 1 < doc->grid.width)
            {
                char cell = doc_at(doc, right + 1, row);
                if (cell == '#' || cell == '%' || cell == 'D' || cell == 'V')
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
                    if (doc_blocks(doc, c, row) || !doc_blocks(doc, c, row + 1))
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
            if (cell == '%')
            {
                /* It collides like the cornice it is standing in, so the climb
                 * still works — but nothing on a wall throws a grenade, so it
                 * can never be opened and is only a '#' the author will expect
                 * to be able to break. */
                report_add(report, ED_SEV_WARN, col, row,
                           "Nothing on a climb can set off a blast, so this patch never opens: paint it as '#'");
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
            if (seen[row][col] || doc_blocks(doc, col, row))
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

/*
 * Is a patch actually a shortcut?
 *
 * [../docs/levels.md](../docs/levels.md) settles what a `%` is *for* in one
 * clause — the route model counting it as wall in both directions "is what keeps
 * a `%` a shortcut and never the way out" — and then nothing measured the
 * shortcut. `check_weak_walls` above asks whether the sector can open one and
 * whether it is let into masonry; both are about whether the tile makes sense,
 * neither about whether it buys anything. So the campaign shipped seven
 * interiors carrying a patch and, on four of them, opening every one of them
 * shortened the walk to the way out and to every pickup by **nought steps** —
 * sector 10 with six of the campaign's seventeen patches among them.
 *
 * That is the same gap the docket sheet had. `*` carried an authoring rule that
 * it should "cost a detour" and no measurement, and seven of the twelve turned
 * out to be sitting on a shortest path to the door. This is the mirror image of
 * it: a thing that is meant to *save* a walk and does not.
 *
 * Sector 2 is the one to read if you want to know how it happens. Its patch is a
 * two-tile partition across the basement, and a paired `D` door twelve tiles
 * away already crosses the same partition for nothing — so the patch is a second
 * route past a wall that was never closed, and taking it costs the sector's only
 * bazooka rocket. Nothing about the tile is wrong. It is the *floor around it*
 * that makes it scenery, which is exactly the kind of fact no rule about a single
 * tile can see and no author can hold in their head.
 *
 * **Nought is the threshold and it is not a number anybody chose.** The docket's
 * rule needed a bar — a tenth of the sector's own walk — because "costs a detour"
 * is a matter of degree. This does not: a shortcut that shortens nothing is not a
 * shortcut in any degree, and picking a bar here would be inventing a design rule
 * rather than measuring the one already written down. A note rather than a
 * warning for the same reason: the map loads, it plays, and whether a bolt-hole
 * that saves no steps is worth a rocket under an alarm is the author's call. The
 * tool's job is that they get to make it knowingly.
 */
static int distances[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];

static void walk_distances(RouteMap *route, RouteCell start)
{
    static RouteCell queue[MAX_LEVEL_HEIGHT * MAX_LEVEL_WIDTH];
    int head = 0;
    int tail = 0;

    for (int row = 0; row < MAX_LEVEL_HEIGHT; ++row)
        for (int col = 0; col < MAX_LEVEL_WIDTH; ++col)
            distances[row][col] = -1;
    if (!route_inside(route, start.col, start.row))
        return;
    distances[start.row][start.col] = 0;
    queue[tail++] = start;
    while (head < tail)
    {
        RouteCell current = queue[head++];
        RouteCell next[ROUTE_MAX_NEIGHBOURS];
        int count = route_neighbours(route, current.col, current.row, next);
        for (int i = 0; i < count; ++i)
        {
            if (distances[next[i].row][next[i].col] >= 0)
                continue;
            distances[next[i].row][next[i].col] =
                distances[current.row][current.col] + 1;
            queue[tail++] = next[i];
        }
    }
}

/*
 * How far the flood got to a tile, or to the floor a thing drawn in mid-air is
 * collected from — the same fallback `route_reaches` makes, for the same reason.
 *
 * `out_direct` says which of the two answers came back, and it exists because
 * this function is asked the same question twice — once before the patches are
 * blown and once after — and the two answers have to be about the same thing.
 * Left out, as it was, the comparison could be satisfied by a patch that
 * **deletes the destination**: open a hole in the slab under a tile and that
 * tile stops being somewhere the player can stand, so the flood no longer
 * reaches it, so the fallback answers with the landing three storeys below —
 * which is nearer, and reads as a saving. Sector 4 had a position like that
 * worth an apparent eighty steps, directly under the window the sector leaves
 * by; the patch would have taken the floor out from under the way out and this
 * check would have called it the best shortcut on the floor.
 *
 * A check that can be satisfied by making the map worse is worse than no check,
 * because it is the one an author will reach for when the note will not go away.
 */
static int distance_to(RouteMap *route, int col, int row, bool *out_direct)
{
    if (out_direct != NULL)
        *out_direct = false;
    if (route_inside(route, col, row) && distances[row][col] >= 0)
    {
        if (out_direct != NULL)
            *out_direct = true;
        return distances[row][col];
    }
    RouteCell landing;
    if (route_landing(route, col, row, &landing) &&
        distances[landing.row][landing.col] >= 0)
    {
        return distances[landing.row][landing.col];
    }
    return -1;
}

/*
 * The saving one target is worth, or nought when it is not worth anything.
 *
 * `before`/`after` are the two distances and `was_direct`/`is_direct` how each
 * was arrived at. A target the player could walk to and now cannot has not got
 * nearer however small the number is — see the note on `distance_to`.
 */
static int honest_saving(int before, bool was_direct, int after, bool is_direct)
{
    if (before < 0 || after < 0)
        return 0;
    if (was_direct && !is_direct)
        return 0;
    return before - after > 0 ? before - after : 0;
}

/*
 * And the floor the patches leave behind, which nothing had asked about.
 *
 * Every route question above is asked of the map *as authored*, because a `%` is
 * masonry to the model in both directions — that is the rule that keeps a patch
 * a shortcut and never the way out. It is also why the stranding check has never
 * seen the world a run is actually in once a rocket has been spent: the hole
 * lasts as long as the visit does (a death keeps it, only a reload puts the wall
 * back), so for most of a sector the player is walking a floor plan no check in
 * this tree had ever looked at.
 *
 * What that misses is the one thing a hole can do that a doorway cannot: drop
 * somebody. A patch whose lower tile is in a slab is a one-way fall, and a fall
 * into a room whose only other exit was the wall you just came through is a run
 * ended by using the mechanic correctly — with every gate green, because the
 * authored map is fine and the authored map is all anybody measured.
 *
 * It arrived the day sector 2's patch moved to the foot of the partition beside
 * the start, where blowing it drops Chuck into the basement instead of walking
 * the long way round through the door pair. That is a good shortcut and it is
 * safe, and the only thing that established either was a throwaway program.
 *
 * An error rather than a warning: this is not "it will not play the way it
 * reads", it is a floor that can eat a run.
 *
 * **It asks one question, and the other two it could have asked cannot fail.**
 * Worth writing down rather than guarding, because an arm nothing can reach is
 * an arm nobody has checked. Opening a wall only ever *adds* passable tiles, so
 * a cell that was reachable and is still somewhere the player can stand is still
 * reachable. The one thing a hole takes away is **support** — and for that,
 * `route_reaches` already answers with the tile the thing would fall to, which
 * is reachable down the very shaft the hole just made. So "the way out is gone"
 * and "a pickup is gone" both resolve to the drop, and what is actually wrong in
 * that case is that the player is now standing at the bottom of it. That is this
 * check.
 */
static void check_opened_walls_leave_a_way_out(const Level *level,
                                               EdReport *report,
                                               RouteCell start, RouteCell goal)
{
    if (count_of(report, '%') == 0)
        return;

    /* A copy, for the reason the shortcut check below makes one: a validator
     * that edits the map it is judging is the bug nobody would look for here. */
    static Level opened;
    static RouteMap route;
    opened = *level;
    for (int row = 0; row < opened.map.height; ++row)
        for (int col = 0; col < opened.map.width; ++col)
            if (opened.map.tiles[row][col] == TILE_WEAK_WALL)
                opened.map.tiles[row][col] = TILE_EMPTY;

    route_map_init(&route, &opened);
    route_flood(&route, start);

    RouteCell landing;
    RouteCell escape = goal;
    if (!route_standing(&route, escape.col, escape.row) &&
        route_landing(&route, escape.col, escape.row, &landing))
        escape = landing;

    if (!route_never_strands(&route, escape))
    {
        int stranded = 0;
        int col = -1;
        int row = -1;
        for (int r = 0; r < opened.map.height; ++r)
        {
            for (int c = 0; c < opened.map.width; ++c)
            {
                if (!route.seen[r][c] || route.escapes[r][c])
                    continue;
                ++stranded;
                if (col < 0)
                {
                    col = c;
                    row = r;
                }
            }
        }
        report_add(report, ED_SEV_ERROR, col, row,
                   "%d tile(s) a blown patch drops the player into cannot get "
                   "back to the way out",
                   stranded);
    }
}

static void check_weak_wall_shortcut(const Level *level, EdReport *report,
                                     RouteCell start, RouteCell goal)
{
    int patches = count_of(report, '%');
    if (patches == 0)
        return;

    /* Everything on the floor the player might be walking towards: the way out,
     * and every pickup. A patch that shortens the route to none of them has
     * shortened the route to nothing, since those are the only places a sector
     * asks anybody to go. */
    walk_distances(&report->route, start);
    bool goal_was_direct = false;
    int before_goal =
        distance_to(&report->route, goal.col, goal.row, &goal_was_direct);
    static int before_items[MAX_ITEMS];
    static bool item_was_direct[MAX_ITEMS];
    for (int i = 0; i < level->runtime.item_count; ++i)
    {
        before_items[i] = distance_to(
            &report->route, (int)(level->runtime.items[i].x / TILE_SIZE),
            (int)(level->runtime.items[i].y / TILE_SIZE),
            &item_was_direct[i]);
    }

    /*
     * The same map with every patch blown, which is a copy rather than a
     * mutation: this is a `const Level *` on purpose — the caller is a validator
     * and a validator that edits the thing it is judging is the one bug nobody
     * would look for here. Static because a `Level` is 55KB and the editor asks
     * this question once a keystroke.
     */
    static Level opened;
    static RouteMap route;
    opened = *level;
    for (int row = 0; row < opened.map.height; ++row)
    {
        for (int col = 0; col < opened.map.width; ++col)
        {
            if (opened.map.tiles[row][col] == TILE_WEAK_WALL)
                opened.map.tiles[row][col] = TILE_EMPTY;
        }
    }
    route_map_init(&route, &opened);
    walk_distances(&route, start);

    int best = 0;
    bool goal_is_direct = false;
    int after = distance_to(&route, goal.col, goal.row, &goal_is_direct);
    int saved = honest_saving(before_goal, goal_was_direct, after,
                              goal_is_direct);
    if (saved > best)
        best = saved;
    for (int i = 0; i < level->runtime.item_count; ++i)
    {
        bool is_direct = false;
        int now = distance_to(&route,
                              (int)(level->runtime.items[i].x / TILE_SIZE),
                              (int)(level->runtime.items[i].y / TILE_SIZE),
                              &is_direct);
        saved = honest_saving(before_items[i], item_was_direct[i], now,
                             is_direct);
        if (saved > best)
            best = saved;
    }

    if (best == 0)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "Opening all %d weak wall(s) shortens the route to the way "
                   "out and to every pickup by nothing: a blast spent here buys "
                   "no walk back, so the patch reads as scenery",
                   patches);
    }
}

/* ---- Can the sector actually be finished? ----------------------------- */

/*
 * A locked door is a thing the player goes looking for the key to.
 *
 * The premise of a `C` and a `T` is a search: one card of the two or three on
 * the floor is live and the rest buzz, and a console costs `TERMINAL_HACK_TIME`
 * standing still. Nothing measured whether either was ever a decision, and
 * measured, six of the seven interiors that leave by a locked door handed it
 * over for free — every card and every terminal on the vault's 132-step walk
 * sat on a *shortest* path to its own exit, so a player who walked to the door
 * arrived to find it already open.
 *
 * That is the weak wall's rule and the docket sheet's a third time: a claim
 * about what a mechanic is for, settled prose for as long as it went
 * unmeasured, and the one thing no rule about a single tile can see. So it is
 * asked here, of the floor plan, in two halves with two severities.
 *
 * A key on a *shortest* path is a **warning**, on the same reasoning as the
 * patch that shortens nothing: nought is not a threshold somebody picked, it is
 * the mechanic not existing in any degree — and which of them the seed makes
 * live is exactly what the player cannot know, so one free key hands over the
 * floor whatever the others cost. The cheapest key being a short walk is a
 * **note**, because how much of a detour is worth making is a judgement about
 * risk — a card under an alarm two rooms away is not the same walk as the same
 * card on a quiet floor — and this model counts steps.
 */
static void check_key_detour(const Level *level, EdReport *report,
                             RouteCell start, RouteCell goal)
{
    /* Only the floors where the lock is the way forward: a `Y` leaves the
     * security door barricaded, so cards and consoles there are score and a
     * checkpoint rather than the route. */
    if (level->map.has_window || !level->map.has_exit)
        return;
    if (level->runtime.card_count == 0 && level->map.terminal_count == 0)
        return;

    walk_distances(&report->route, start);
    int direct = distance_to(&report->route, goal.col, goal.row, NULL);
    if (direct <= 0)
        return; /* the way out is already reported unreachable */

    static int from_start[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    memcpy(from_start, distances, sizeof(from_start));

    int cheapest = -1;
    int cheapest_col = -1;
    int cheapest_row = -1;
    int keys = level->runtime.item_count + level->map.terminal_count;
    for (int k = 0; k < keys; ++k)
    {
        bool card = k < level->runtime.item_count;
        int col;
        int row;
        if (card)
        {
            if (level->runtime.items[k].type != ITEM_CARD)
                continue;
            col = (int)(level->runtime.items[k].x / TILE_SIZE);
            row = (int)(level->runtime.items[k].y / TILE_SIZE);
        }
        else
        {
            const Terminal *terminal =
                &level->map.terminals[k - level->runtime.item_count];
            col = terminal->col;
            row = terminal->row;
        }

        /* Where the player has to stand to use it, which is `route_reaches`'s
         * own rule: the tile when that is standable, else the floor a body
         * dropped there lands on. */
        RouteCell stand = {col, row};
        RouteCell landing;
        if (!route_standing(&report->route, col, row))
        {
            if (!route_landing(&report->route, col, row, &landing))
                continue; /* unreachable, and already reported as that */
            stand = landing;
        }
        int there = from_start[stand.row][stand.col];
        if (there < 0)
            continue;

        walk_distances(&report->route, stand);
        int back = distance_to(&report->route, goal.col, goal.row, NULL);
        if (back < 0)
            continue;

        int detour = there + back - direct;
        if (detour <= 0)
        {
            report_add(report, ED_SEV_WARN, col, row,
                       "This %s is on a shortest way to the door it opens, so "
                       "the door opens by walking to it",
                       card ? "key card" : "terminal");
        }
        if (cheapest < 0 || detour < cheapest)
        {
            cheapest = detour;
            cheapest_col = col;
            cheapest_row = row;
        }
    }

    if (cheapest > 0 && cheapest * 100 < direct * ED_KEY_DETOUR_PERCENT)
    {
        report_add(report, ED_SEV_NOTE, cheapest_col, cheapest_row,
                   "The cheapest way to open the door costs %d steps of a "
                   "%d-step walk; %d%% is what makes the search a decision",
                   cheapest, direct, ED_KEY_DETOUR_PERCENT);
    }
}

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
            /*
             * A warning rather than a note, and the campaign is the argument.
             *
             * An optional pickup is optional to *take*, never optional to
             * reach: nothing in an ordinary run reveals one placed where the
             * player cannot stand, so it is a thing drawn on the map that no
             * player will ever meet — which is exactly "it loads, but it will
             * not play the way it reads", the definition of a warning here.
             * Sector 14's flash charge sat on top of a partition five rows
             * above its floor and this rule said so all along, in the one
             * severity `test_the_editor_has_nothing_to_say_about_the_shipped_campaign`
             * deliberately allows. As a warning it is a failed build instead.
             */
            report_add(report, ED_SEV_WARN, col, row,
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

    /*
     * Last, and only once the sector is known to be finishable at all: a
     * question about how much a patch saves is meaningless on a map whose way
     * out cannot be reached, and asking it would put a note about scenery
     * underneath an error about the door. `report->route` is flooded and
     * `escape` is the goal resolved onto the floor it is entered from, which is
     * the pair this needs.
     */
    check_key_detour(level, report, report->start, goal);
    check_weak_wall_shortcut(level, report, report->start, escape);
    /* And the same map once the rockets have been spent, which is the floor plan
     * a run spends most of a sector in and the one nothing here had looked at. */
    check_opened_walls_leave_a_way_out(level, report, report->start, goal);
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
        check_mines(doc, report);
        check_cameras(doc, report);
        check_panels(doc, report);
        check_spikes(doc, report);
        check_weak_walls(doc, report);
        check_ducts(doc, report);
        check_moving_platforms(doc, report);
        /* Interiors only, which is where an `m` may stand at all. */
        check_flight_cases(doc, report);
    }

    if (!parsed)
    {
        report_add(report, ED_SEV_ERROR, -1, -1,
                   "The loader rejects this map, so the game cannot start it");
        return;
    }

    /* Asked of the parsed level rather than guessed from the mode, because the
     * grid names a theme too: restroom fittings set RESTROOM on a map that
     * carries no THEME line at all, so a note that only ever printed the mode's
     * default told the author the WC was a plant room. */
    if (!doc->grid.has_theme)
    {
        report_add(report, ED_SEV_NOTE, -1, -1,
                   "No THEME line, so the map loads as %s",
                   level_theme_name(level->map.theme));
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
