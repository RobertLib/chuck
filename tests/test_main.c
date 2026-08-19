#include "camera.h"
#include "chase.h"
#include "credits.h"
#include "crew.h"
#include "editor_doc.h"
#include "editor_legend.h"
#include "editor_validate.h"
#include "embedded_levels.h"
#include "game_event.h"
#include "gameplay_ai.h"
#include "gameplay_climb.h"
#include "gameplay_combat.h"
#include "gameplay_interaction.h"
#include "gameplay_physics.h"
#include "gameplay_state.h"
#include "gameplay_world.h"
#include "intel.h"
#include "level.h"
#include "level_route.h"
#include "manual_pages.h"
#include "pad_hint.h"
#include "pause_sheet.h"
#include "progress.h"
#include "run_tally.h"
#include "sector_tally.h"
#include "rng.h"
#include "settings.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures;

#define CHECK(condition)                                 \
    do                                                   \
    {                                                    \
        if (!(condition))                                \
        {                                                \
            fprintf(stderr, "%s:%d: check failed: %s\n", \
                    __FILE__, __LINE__, #condition);     \
            failures++;                                  \
        }                                                \
    } while (0)

/*
 * A check the rest of the test cannot survive.
 *
 * `CHECK` counts a failure and carries on, which is right for an assertion
 * about a value: one wrong number must not hide the twenty checks after it.
 * It is wrong for a pointer. A helper that did not find the item it was looking
 * for hands back NULL and the next line dereferences it, so a test that should
 * have printed one legible `check failed:` takes the whole suite down with a
 * segfault instead — and a crash reports nothing, including the hundred tests
 * that had not run yet. Every `test_*` returns void, so bailing out of this one
 * and letting the rest of the suite run is the whole of the fix.
 */
#define REQUIRE(condition)                                  \
    do                                                      \
    {                                                       \
        if (!(condition))                                   \
        {                                                   \
            fprintf(stderr, "%s:%d: required: %s\n",        \
                    __FILE__, __LINE__, #condition);        \
            failures++;                                     \
            return;                                         \
        }                                                   \
    } while (0)

/*
 * Drive the simulation the way the shipped game drives it.
 *
 * `SDL_AppIterate` banks the real elapsed time and spends it in whole
 * `SIM_STEP_DT` slices, so 240Hz is the only rate anything is ever stepped at.
 * A test written against `1.0f / 60.0f` is a test of a rate that does not
 * exist, and this repository has already paid for that once: the grab that
 * starts a climb *down* needs the box to travel 1px into the rung, a 240Hz step
 * carries it 0.42 and a 60Hz step 1.67, so descending a ladder was deadlocked
 * on every map in the campaign and green in a suite written at 1/60.
 *
 * The rate was the half that got fixed. The other half is that a loop written
 * `for (frame = 0; frame < 240; ++frame)` does not say four seconds, it says
 * 240 steps — which is four seconds at 60Hz and one at 240 — so swapping the
 * `dt` alone quietly shortens every test that has one. **Count in seconds**:
 * `SIM_STEPS(4.0f)` is how long, and it says how long.
 *
 * The one place a rate other than `SIM_STEP_DT` is still correct is a test of
 * rate-independence itself, which sweeps several on purpose and says so.
 */
#define SIM_STEPS(seconds) ((int)((seconds) / SIM_STEP_DT + 0.5f))

static bool events_have_sound(const GameEventBuffer *events,
                              GameEventType type, SoundEffect effect)
{
    for (int i = 0; i < events->count; ++i)
    {
        if (events->items[i].type == type &&
            events->items[i].data.sound.effect == effect)
            return true;
    }
    return false;
}

static void test_rng_is_reproducible(void)
{
    Rng first;
    Rng second;
    rng_seed(&first, 42);
    rng_seed(&second, 42);

    for (int i = 0; i < 64; ++i)
        CHECK(rng_next(&first) == rng_next(&second));

    for (int i = 0; i < 128; ++i)
    {
        int value = rng_range(&first, 7);
        CHECK(value >= 0 && value < 7);
    }
}

static void test_camera_axis_target(void)
{
    CHECK(fabsf(camera_axis_target(100.0f, 1600.0f, 800.0f)) < 0.001f);
    CHECK(fabsf(camera_axis_target(700.0f, 1600.0f, 800.0f) - 300.0f) <
          0.001f);
    CHECK(fabsf(camera_axis_target(1500.0f, 1600.0f, 800.0f) - 800.0f) <
          0.001f);
    CHECK(fabsf(camera_axis_target(300.0f, 640.0f, 800.0f)) < 0.001f);

    /* A map only one tile taller than the playfield must expose that tile;
     * this used to be suppressed by a special vertical-only threshold. */
    CHECK(fabsf(camera_axis_target(520.0f, 17.0f * TILE_SIZE, 512.0f) -
                TILE_SIZE) < 0.001f);
}

static void test_level_parser_and_seeded_choices(void)
{
    static const char data[] =
        "##########\n"
        "#S C T CE#\n"
        "##########\n";
    Level first;
    Level second;
    Rng first_rng;
    Rng second_rng;
    rng_seed(&first_rng, 1234);
    rng_seed(&second_rng, 1234);

    CHECK(level_load_data(&first, "test", data, strlen(data), &first_rng));
    CHECK(level_load_data(&second, "test", data, strlen(data), &second_rng));
    CHECK(first.map.width == 10);
    CHECK(first.map.height == 3);
    CHECK(first.runtime.card_count == 2);
    CHECK(first.map.terminal_count == 1);
    CHECK(!first.runtime.exit_unlocked);
    CHECK(first.runtime.active_card_index == second.runtime.active_card_index);
    CHECK(first.runtime.active_terminal_index == second.runtime.active_terminal_index);
}

static void test_level_theme_metadata(void)
{
    static const char themed[] =
        "##########\n"
        "#S     TE#\n"
        "##########\n"
        "\n"
        "THEME ARCHIVE\n";
    static const char unthemed[] =
        "##########\n"
        "#S     TE#\n"
        "##########\n";
    static const char facade[] =
        "......Y..................\n"
        "......S..................\n"
        "\n"
        "MODE FACADE\n";
    static const char misspelt[] =
        "##########\n"
        "#S     TE#\n"
        "##########\n"
        "\n"
        "THEME BASEMENT\n";
    Level level;
    Rng rng;

    rng_seed(&rng, 7);
    CHECK(level_load_data(&level, "themed", themed, strlen(themed), &rng));
    CHECK(level.map.theme == LEVEL_THEME_ARCHIVE);

    /* A map without the line still loads: a new sector drops in and picks up
     * the default look for its mode rather than failing to parse. */
    CHECK(level_load_data(&level, "plain", unthemed, strlen(unthemed), &rng));
    CHECK(level.map.theme == LEVEL_THEME_PLANT);
    CHECK(level_load_data(&level, "wall", facade, strlen(facade), &rng));
    CHECK(level.map.theme == LEVEL_THEME_FACADE_NIGHT);

    /* A misspelt name is an error: silently falling back would ship a level
     * wearing a look nobody chose. */
    CHECK(!level_load_data(&level, "expected-parse-failure", misspelt, strlen(misspelt), &rng));

    LevelTheme parsed = LEVEL_THEME_PLANT;
    CHECK(level_theme_from_name("SERVER", 6, &parsed));
    CHECK(parsed == LEVEL_THEME_SERVER);
    CHECK(!level_theme_from_name("SERVERS", 7, &parsed));
    CHECK(!level_theme_from_name("SERV", 4, &parsed));
}

/*
 * The point of the themes is that the campaign never shows the same room
 * twice in a row. That is a property of the level set, so it is pinned here
 * rather than left to whoever adds the next map.
 */
static void test_campaign_themes_keep_changing(void)
{
    bool used[LEVEL_THEME_COUNT] = {false};
    LevelTheme previous = LEVEL_THEME_COUNT;
    int distinct = 0;

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Level level;
        Rng rng;
        rng_seed(&rng, 500 + i);
        CHECK(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data,
                              EMBEDDED_LEVELS[i].size, &rng));
        LevelTheme theme = level.map.theme;
        CHECK(theme >= 0 && theme < LEVEL_THEME_COUNT);
        /* Back-to-back sectors must never wear the same look. */
        CHECK(theme != previous);
        previous = theme;
        if (!used[theme])
        {
            used[theme] = true;
            distinct++;
        }

        bool facade_theme = theme >= LEVEL_THEME_FACADE_NIGHT;
        CHECK(facade_theme == (level.map.mode == LEVEL_MODE_FACADE));
        /* The restroom look belongs to the sublevel, never to a sector. */
        CHECK(theme != LEVEL_THEME_RESTROOM);
    }
    /* Fifteen levels, and no look reused more than once. */
    CHECK(distinct >= 14);
}

static void test_all_embedded_levels_parse(void)
{
    CHECK(EMBEDDED_LEVEL_COUNT == 17);
    int sublevel_entrances = 0;
    int facade_levels = 0;
    /* The campaign alternates interior sectors with climbs, and the only way
     * onto a facade is the window of the sector below it. */
    bool climb_expected = false;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Level level;
        Rng rng;
        rng_seed(&rng, 1000 + i);
        CHECK(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data,
                              EMBEDDED_LEVELS[i].size, &rng));
        CHECK(level.map.width > 0);
        CHECK(level.map.height > 0);
        CHECK(level.map.has_exit || level.map.has_window);

        bool facade = level.map.mode == LEVEL_MODE_FACADE;
        CHECK(facade == climb_expected);
        if (facade)
        {
            facade_levels++;
            CHECK(!level.map.has_exit);
            CHECK(level.map.has_window);
            CHECK(level.map.facade_hazard_spawn_count >= 8);
            CHECK(level.map.door_count == 0);
            /* Pickups on the wall are optional detours, never the route. */
            CHECK(level.runtime.item_count > 0);
            CHECK(level.runtime.item_count <= 4);
            CHECK(level.map.enemy_count == 0);
            /* Masonry is what the climb is routed around; it must exist, and
             * the exterior never uses interior traversal aids. */
            int facade_walls = 0;
            for (int row = 0; row < level.map.height; ++row)
                for (int col = 0; col < level.map.width; ++col)
                {
                    CHECK(level.map.tiles[row][col] != TILE_LADDER);
                    facade_walls += level.map.tiles[row][col] == TILE_WALL;
                }
            CHECK(facade_walls > 40);
        }
        else
        {
            CHECK(level.map.has_exit);
            CHECK(level.map.alarm_switch_count >= 2);
            /* A sector that hands over to a climb has its stair door welded
             * shut, so neither a card nor a terminal can open it. */
            if (level.map.has_window)
                CHECK(!level.runtime.exit_unlocked);
        }
        climb_expected = !facade && level.map.has_window;

        if (level.map.has_sublevel_entrance)
            sublevel_entrances++;

        /* The lobby is the sector the crew walked her through, so it is the
         * one that empties as Chuck walks in. Anyone planted inside the
         * dissolve radius of the way in would fade out before running
         * anywhere, so the evacuation has to start further into the room. */
        CHECK(i != 0 || level.map.civilian_count >= 4);
        /* Once they have gone the hall would be empty, which is not what a
         * building with a staffed front desk looks like at any hour. */
        CHECK(i != 0 || level.map.receptionist_count == 1);
        for (int person = 0; person < level.map.civilian_count; ++person)
        {
            float centre = level.map.civilian_spawns[person].x +
                           CIVILIAN_W * 0.5f;
            float way_in = level.map.start_x + PLAYER_W * 0.5f;
            CHECK(fabsf(centre - way_in) > CIVILIAN_FADE_DISTANCE);
        }

        int bazooka_count = 0;
        int grenade_count = 0;
        for (int item = 0; item < level.runtime.item_count; ++item)
        {
            if (level.runtime.items[item].type == ITEM_BAZOOKA)
                bazooka_count++;
            if (level.runtime.items[item].type == ITEM_GRENADE)
                grenade_count++;
        }
        CHECK(bazooka_count == (((i + 1) % 2 == 0) ? 1 : 0));

        /* Weak walls are shortcuts, and only a blast opens one: a sector that
         * blocks an opening up without carrying anything that could reopen it
         * has painted a wall the player can read as a route and never use.
         * They never appear on a climb, where nothing can be set off at all. */
        int weak_walls = 0;
        for (int row = 0; row < level.map.height; ++row)
            for (int col = 0; col < level.map.width; ++col)
                weak_walls += level.map.tiles[row][col] == TILE_WEAK_WALL;
        CHECK(weak_walls == 0 || bazooka_count > 0 || grenade_count > 0);
        CHECK(!facade || weak_walls == 0);
        /* Nothing can be fired on the wall, so a rocket out there is dead
         * weight; every bazooka belongs to an interior sector. */
        CHECK(!facade || bazooka_count == 0);
    }
    /* The campaign ends inside the building, not hanging off it. */
    CHECK(!climb_expected);
    CHECK(facade_levels == 5);
    CHECK(sublevel_entrances == 4);
}

/*
 * The four restrooms are four rooms.
 *
 * `test_campaign_levels_are_distinct_and_solvable` requires that no two sectors
 * share their dimensions or their storey rhythm, because two floors built the
 * same way are one floor drawn twice however differently they are dressed. The
 * sublevels were never asked, and two of them were the same room: the plant's
 * washroom and the penthouse's both ran a two-row gallery over a slab with two
 * gaps in it over a three-row floor, so the only thing telling them apart was
 * the tiling and which fittings stood where.
 *
 * A washroom looking like a washroom is the point of them, so this asks for
 * less than the campaign's rule does: the loadout is deliberately identical in
 * all four — a gun, a grenade and a medkit for the same detour — and nothing
 * here objects to that. What it objects to is the *shape*, which is the part
 * the player walks through.
 */
static void test_the_restrooms_are_four_rooms_rather_than_one(void)
{
    static Level rooms[8];
    static int rhythm[8][MAX_LEVEL_HEIGHT];
    static int rhythm_len[8];

    REQUIRE(EMBEDDED_SUBLEVEL_COUNT > 1);
    REQUIRE(EMBEDDED_SUBLEVEL_COUNT <= 8);
    for (size_t i = 0; i < EMBEDDED_SUBLEVEL_COUNT; ++i)
    {
        Rng rng;
        rng_seed(&rng, 8100 + i);
        REQUIRE(level_load_data(&rooms[i], EMBEDDED_SUBLEVELS[i].name,
                                EMBEDDED_SUBLEVELS[i].data,
                                EMBEDDED_SUBLEVELS[i].size, &rng));
        rhythm_len[i] = level_storey_rhythm(&rooms[i].map, rhythm[i],
                                            MAX_LEVEL_HEIGHT);

        /* The detour pays the same everywhere, which is a decision rather than
         * an accident: the rooms hang off sectors at very different pressures
         * and a reward that scaled with the floor would make the early ones not
         * worth the seconds. */
        int gun = 0;
        int grenade = 0;
        int medkit = 0;
        for (int k = 0; k < rooms[i].runtime.item_count; ++k)
        {
            gun += rooms[i].runtime.items[k].type == ITEM_GUN;
            grenade += rooms[i].runtime.items[k].type == ITEM_GRENADE;
            medkit += rooms[i].runtime.items[k].type == ITEM_MEDKIT;
        }
        CHECK(gun == 1);
        CHECK(grenade == 1);
        CHECK(medkit == 1);
        /* And no sheet of the docket: one to an interior, and a washroom is not
         * one. */
        for (int k = 0; k < rooms[i].runtime.item_count; ++k)
            CHECK(rooms[i].runtime.items[k].type != ITEM_EVIDENCE);
    }

    for (size_t a = 0; a < EMBEDDED_SUBLEVEL_COUNT; ++a)
    {
        for (size_t b = a + 1; b < EMBEDDED_SUBLEVEL_COUNT; ++b)
        {
            CHECK(rooms[a].map.width != rooms[b].map.width ||
                  rooms[a].map.height != rooms[b].map.height);
            bool same_rhythm = rhythm_len[a] == rhythm_len[b];
            for (int i = 0; i < rhythm_len[a] && same_rhythm; ++i)
                same_rhythm = rhythm[a][i] == rhythm[b][i];
            CHECK(!same_rhythm);
        }
    }
}

/*
 * The furthest a sector may ask the player to walk with nothing banked behind
 * them, in steps of the route model.
 *
 * A death costs a life and puts Chuck back on the last banked checkpoint, and
 * only four things bank one: a card, a finished hack, a step through a door
 * pair and a medkit (`gameplay_bank_checkpoint`'s callers in
 * gameplay_interaction.c). Nothing else does — not distance, not a storey, not
 * the docket sheet, which is deliberate and argued where it is collected.
 *
 * Which means the safety net is a property of the *map*, and three sectors had
 * none at all: the ones that leave by a window carry no card and no terminal,
 * because their stair door is welded and there is nothing to unlock. Sectors 2,
 * 6 and 12 put every bank they had off the route entirely, so a death anywhere
 * on those floors replayed the whole of them — 133 route steps of mines and
 * fans on sector 12, which is the longest walk in the campaign. Nothing said
 * so: the hazard budget only counts what is on the floor, and the route model
 * only asked whether the way out could be reached at all.
 *
 * Fifty is chosen off the campaign rather than out of the air: the nine sectors
 * that always had a bank on the line run between 17 and 35 steps, so this is
 * the shape they already were, with room for an author who wants a longer
 * stretch than any shipped sector has and not room for a floor with nothing on
 * it.
 */
#define CHECKPOINT_MAX_STRETCH 50

/* How far a bank may sit off the shortest way out and still count as being on
 * it: five steps out and five back. A card down a dead end is a detour the
 * player chooses, and a detour nobody takes banks nothing. */
#define CHECKPOINT_ROUTE_SLACK 10

static int checkpoint_from_start[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
static int checkpoint_from_exit[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];

/* Breadth-first over exactly the moves `route_neighbours` allows, which is the
 * same conservative walk the editor and every other route check use. */
static void checkpoint_bfs(RouteMap *route,
                           int dist[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH],
                           RouteCell start)
{
    static RouteCell queue[MAX_LEVEL_HEIGHT * MAX_LEVEL_WIDTH];
    for (int row = 0; row < MAX_LEVEL_HEIGHT; ++row)
        for (int col = 0; col < MAX_LEVEL_WIDTH; ++col)
            dist[row][col] = -1;

    int head = 0;
    int tail = 0;
    queue[tail++] = start;
    dist[start.row][start.col] = 0;
    while (head < tail)
    {
        RouteCell cur = queue[head++];
        RouteCell out[ROUTE_MAX_NEIGHBOURS];
        int found = route_neighbours(route, cur.col, cur.row, out);
        for (int i = 0; i < found; ++i)
        {
            if (dist[out[i].row][out[i].col] >= 0)
                continue;
            dist[out[i].row][out[i].col] = dist[cur.row][cur.col] + 1;
            queue[tail++] = out[i];
        }
    }
}

/* Where the player has to stand to use a thing drawn at (col,row): the cell
 * itself when that is standable, otherwise the one a body dropped there lands
 * on — `route_reaches`'s own rule, because a card floating over a floor is
 * taken by walking under it. */
static bool checkpoint_standing_cell(RouteMap *route, int col, int row,
                                     RouteCell *out)
{
    if (col < 0 || row < 0 || col >= MAX_LEVEL_WIDTH || row >= MAX_LEVEL_HEIGHT)
        return false;
    if (checkpoint_from_start[row][col] >= 0)
    {
        *out = (RouteCell){col, row};
        return true;
    }
    RouteCell landing;
    if (route_landing(route, col, row, &landing) &&
        checkpoint_from_start[landing.row][landing.col] >= 0)
    {
        *out = landing;
        return true;
    }
    return false;
}

/*
 * Every interior banks a checkpoint on the way out, and no stretch of the walk
 * goes unbanked for longer than `CHECKPOINT_MAX_STRETCH`.
 *
 * The measurement is two floods — from the spawn and from the way out — so that
 * "on the route" means what it says: a bank counts when standing at it costs at
 * most `CHECKPOINT_ROUTE_SLACK` steps more than not standing at it. That
 * distinction is the whole test. Sector 2's medkit used to sit one tile from
 * the spawn and *seventy-two steps* along the graph, on the far side of the
 * floor from the window, which reads as a bank at the start and is a detour
 * nobody takes.
 */
static void test_no_sector_asks_for_a_long_walk_with_nothing_banked(void)
{
    static Level level;
    static RouteMap route;

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Rng rng;
        rng_seed(&rng, 6100 + i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        /* A climb banks by height as it is climbed
         * (`gameplay_climb_update_player`), so it has a checkpoint every
         * `FACADE_CHECKPOINT_STEP` and nothing to measure here. */
        if (level.map.mode == LEVEL_MODE_FACADE)
            continue;

        route_map_init(&route, &level);
        checkpoint_bfs(&route, checkpoint_from_start, route_player_start(&route));

        int exit_col = level.map.has_window ? level.map.window_col
                                            : level.map.exit_col;
        int exit_row = level.map.has_window ? level.map.window_row
                                            : level.map.exit_row;
        RouteCell goal;
        REQUIRE(checkpoint_standing_cell(&route, exit_col, exit_row, &goal));
        checkpoint_bfs(&route, checkpoint_from_exit, goal);
        int walk = checkpoint_from_start[goal.row][goal.col];
        REQUIRE(walk > 0);

        /* Every bank on the line, as its distance from the spawn. */
        int banks[MAX_ITEMS + MAX_TERMINALS + MAX_DOORS];
        int bank_count = 0;
        RouteCell cell;
        for (int k = 0; k < level.runtime.item_count; ++k)
        {
            const Item *item = &level.runtime.items[k];
            if (item->type != ITEM_CARD && item->type != ITEM_MEDKIT)
                continue;
            if (!checkpoint_standing_cell(&route, (int)(item->x / TILE_SIZE),
                                          (int)(item->y / TILE_SIZE), &cell))
                continue;
            int there = checkpoint_from_start[cell.row][cell.col];
            int back = checkpoint_from_exit[cell.row][cell.col];
            if (back >= 0 && there + back <= walk + CHECKPOINT_ROUTE_SLACK)
                banks[bank_count++] = there;
        }
        for (int k = 0; k < level.map.terminal_count; ++k)
        {
            if (!checkpoint_standing_cell(&route, level.map.terminals[k].col,
                                          level.map.terminals[k].row, &cell))
                continue;
            int there = checkpoint_from_start[cell.row][cell.col];
            int back = checkpoint_from_exit[cell.row][cell.col];
            if (back >= 0 && there + back <= walk + CHECKPOINT_ROUTE_SLACK)
                banks[bank_count++] = there;
        }
        for (int k = 0; k < level.map.door_count; ++k)
        {
            if (!checkpoint_standing_cell(&route, level.map.doors[k].col,
                                          level.map.doors[k].row, &cell))
                continue;
            int there = checkpoint_from_start[cell.row][cell.col];
            int back = checkpoint_from_exit[cell.row][cell.col];
            if (back >= 0 && there + back <= walk + CHECKPOINT_ROUTE_SLACK)
                banks[bank_count++] = there;
        }

        /* A floor with nothing on its route is the failure this exists for, and
         * it is worth saying separately from the length: a sector that banks
         * nothing replays whole however short it is. */
        CHECK(bank_count > 0);

        for (int a = 0; a < bank_count; ++a)
            for (int b = a + 1; b < bank_count; ++b)
                if (banks[b] < banks[a])
                {
                    int swap = banks[a];
                    banks[a] = banks[b];
                    banks[b] = swap;
                }

        int previous = 0;
        int worst = 0;
        for (int a = 0; a < bank_count; ++a)
        {
            if (banks[a] > walk)
                continue;
            if (banks[a] - previous > worst)
                worst = banks[a] - previous;
            previous = banks[a];
        }
        /* The last stretch counts too: reaching the way out is not a bank, and
         * a floor that banks early and then asks for sixty steps has the same
         * problem at the other end. */
        if (walk - previous > worst)
            worst = walk - previous;

        CHECK(worst <= CHECKPOINT_MAX_STRETCH);
    }
}

/*
 * Three properties of the campaign as a whole, none of which the parser or the
 * theme rules can see: no sector is built like another one, the pressure only
 * ever rises, and every interior can actually be finished.
 */
static void test_campaign_levels_are_distinct_and_solvable(void)
{
#define MAX_CAMPAIGN_LEVELS 32
    static Level levels[MAX_CAMPAIGN_LEVELS];
    static int rhythm[MAX_CAMPAIGN_LEVELS][MAX_LEVEL_HEIGHT];
    static int rhythm_len[MAX_CAMPAIGN_LEVELS];

    CHECK(EMBEDDED_LEVEL_COUNT <= MAX_CAMPAIGN_LEVELS);
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Rng rng;
        rng_seed(&rng, 7000 + i);
        CHECK(level_load_data(&levels[i], EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data,
                              EMBEDDED_LEVELS[i].size, &rng));
        rhythm_len[i] = level_storey_rhythm(&levels[i].map, rhythm[i],
                                            MAX_LEVEL_HEIGHT);
    }

    for (size_t a = 0; a < EMBEDDED_LEVEL_COUNT; ++a)
    {
        for (size_t b = a + 1; b < EMBEDDED_LEVEL_COUNT; ++b)
        {
            /* Two sectors the same size are two sectors that look the same on
             * the map screen before the player has walked a step of either. */
            CHECK(levels[a].map.width != levels[b].map.width ||
                  levels[a].map.height != levels[b].map.height);

            if (levels[a].map.mode != levels[b].map.mode)
                continue;
            if (levels[a].map.mode == LEVEL_MODE_FACADE)
                continue;
            bool same_rhythm = rhythm_len[a] == rhythm_len[b];
            for (int i = 0; i < rhythm_len[a] && same_rhythm; ++i)
                same_rhythm = rhythm[a][i] == rhythm[b][i];
            CHECK(!same_rhythm);
        }
    }

    /* The climb gets longer and busier every time, and so does the walk. */
    int previous_interior = -1;
    int previous_climb = -1;
    int previous_climb_height = -1;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        int budget = level_hazard_budget(&levels[i]);
        if (levels[i].map.mode == LEVEL_MODE_FACADE)
        {
            CHECK(budget > previous_climb);
            CHECK(levels[i].map.height > previous_climb_height);
            previous_climb = budget;
            previous_climb_height = levels[i].map.height;
            continue;
        }
        CHECK(budget > previous_interior);
        previous_interior = budget;
    }

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        const Level *level = &levels[i];
        if (level->map.mode == LEVEL_MODE_FACADE)
            continue; /* climbs are pinned by the bot in their own test */

        static RouteMap route;
        route_map_init(&route, level);

        RouteCell landing;
        route_flood(&route, route_player_start(&route));

        /* The way out of the sector: the window when there is one, because the
         * stair door beside it is welded shut. */
        RouteCell goal = level->map.has_window
                             ? (RouteCell){level->map.window_col,
                                           level->map.window_row}
                             : (RouteCell){level->map.exit_col,
                                           level->map.exit_row};
        CHECK(route_reaches(&route, goal.col, goal.row));

        /*
         * Which card opens the door and which terminal is live is decided by
         * the seed, so every one of them has to be gettable — and the docket
         * sheet is held to the same bar for a different reason: it is optional,
         * which means nothing in a *run* would ever reveal one placed somewhere
         * the player cannot stand. A collectable behind a wall is a collectable
         * the completionist looks for all night.
         */
        for (int item = 0; item < level->runtime.item_count; ++item)
        {
            ItemType type = level->runtime.items[item].type;
            if (type != ITEM_CARD && type != ITEM_EVIDENCE)
                continue;
            CHECK(route_reaches(&route,
                                (int)(level->runtime.items[item].x / TILE_SIZE),
                                (int)(level->runtime.items[item].y / TILE_SIZE)));
        }
        for (int t = 0; t < level->map.terminal_count; ++t)
        {
            CHECK(route_reaches(&route, level->map.terminals[t].col,
                                level->map.terminals[t].row));
        }
        if (level->map.has_sublevel_entrance)
        {
            CHECK(route_reaches(&route, level->map.sublevel_entrance_col,
                                level->map.sublevel_entrance_row));
        }

        if (!route_standing(&route, goal.col, goal.row) &&
            route_landing(&route, goal.col, goal.row, &landing))
        {
            goal = landing;
        }
        CHECK(route_never_strands(&route, goal));
    }
}

/*
 * A floor has to be able to seat the men it can send for.
 *
 * `MAX_ENEMIES` is a seating limit rather than a design statement, and the
 * demand on it is not the count drawn on the map: a terminal hacked under the
 * alarm sends for up to `TERMINAL_REINFORCEMENT_MAX_COUNT` out of a door, once
 * per console, and every one of them needs a slot at the same time as the men
 * already standing there and the corpses of the ones that have gone down.
 *
 * Sector 14 was over it — twelve men, three consoles, two doors, so eighteen
 * against a ceiling of sixteen — and what that cost was not a crash. Nothing
 * fails when the array is full: `find_enemy_slot` hands the arrival the corpse
 * furthest from Chuck, so the floor deleted a body in front of the player and
 * with it the thing `update_body_discovery` sends the next calm guard over to
 * look at. The whole quiet route rests on bodies being readable, and on the
 * busiest floor in the game that has doors it silently stopped being true.
 *
 * The requirement is derived here rather than trusted, which is the point: a
 * fourth console on a floor, or a raised `TERMINAL_REINFORCEMENT_MAX_COUNT`,
 * fails the build instead of quietly eating a corpse. A sector with no door
 * asks for nothing, because a reinforcement has nowhere to come out of —
 * sectors 16 and 17 are the two, which is why the fourteenth floor and not the
 * seventeenth was the one over the line.
 */
static void test_every_sector_can_seat_the_reinforcements_it_can_call(void)
{
    int worst_men = 0;
    int worst_dogs = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Level level;
        Rng rng;
        rng_seed(&rng, 900 + i);
        CHECK(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data, EMBEDDED_LEVELS[i].size,
                              &rng));
        if (level.map.mode == LEVEL_MODE_FACADE)
            continue;

        int dogs = 0;
        for (int e = 0; e < level.map.enemy_count; ++e)
            dogs += level.map.enemy_spawns[e].has_dog;

        /* No door is no reinforcement, however many consoles the floor has. */
        int called = level.map.door_count > 0
                         ? level.map.terminal_count *
                               TERMINAL_REINFORCEMENT_MAX_COUNT
                         : 0;
        int men = level.map.enemy_count + called;
        /* Every arrival can bring a handler, so the dogs answer the same sum. */
        int with_dogs = dogs + called;

        CHECK(men <= MAX_ENEMIES);
        CHECK(with_dogs <= MAX_DOGS);
        if (men > worst_men)
            worst_men = men;
        if (with_dogs > worst_dogs)
            worst_dogs = with_dogs;
    }

    /* And the ceilings are not so far above the campaign that this check has
     * stopped meaning anything — if a rewrite leaves the worst floor asking for
     * a third of the array, the number below wants revisiting rather than the
     * check quietly passing forever. */
    CHECK(worst_men * 2 >= MAX_ENEMIES);
    CHECK(worst_dogs * 2 >= MAX_DOGS);
}

/*
 * A sector has to give the player a moment to read it.
 *
 * Every other campaign rule in this file is about the *map* — a size no other
 * sector has, a storey rhythm no other sector has, a route the conservative
 * model can walk. None of them asked what the floor does to a player who has
 * just been handed the controls, and the answer was that six of the seventeen
 * cost him a heart before he could plausibly have decided anything: sector 6
 * opened two seconds from a rifle, sector 14 put a guard dog eight tiles from
 * the spawn on an open floor, and 5, 8, 9 and 10 were all inside four seconds.
 *
 * The reveal freezes the simulation until it has finished, so the clock this
 * measures starts where the player's does.
 *
 * What it asks is the weakest useful thing. Not that standing still is safe —
 * a floor that never noticed a man standing in it would be a floor with nobody
 * on it, and `test_a_sector_notices_a_man_standing_in_it` is the other end of
 * this. Only that the first `SPAWN_GRACE_SECONDS` belong to the player, because
 * that window is where the choice between the quiet route and the loud one is
 * made, and a spawn already inside somebody's engagement has no choice on it.
 *
 * Several seeds, because which way a patrol faces when the sector loads is a
 * seeded choice and one seed would pin whichever answer it happened to draw.
 */
#define SPAWN_GRACE_SECONDS 3.0f
#define SPAWN_GRACE_SEEDS 16

/* Everything in the frame that can cost a heart while the player does nothing:
 * the men, the dogs, their rounds, the hazards he is standing in and the alarm
 * clock. Deliberately not the whole frame — see
 * `test_the_whole_frame_survives_a_monkey_on_the_controls` for that — because
 * what this measures is what the *floor* does, and a stationary player moves
 * no crate and rides no lift. */
static void step_the_floor_around_a_still_player(GameplayState *state,
                                                 CampaignState *campaign)
{
    state->events.count = 0;
    gameplay_ai_update_spawns(state, SIM_STEP_DT);
    gameplay_ai_update_movement(state, SIM_STEP_DT);
    gameplay_ai_update_combat(state, SIM_STEP_DT);
    gameplay_combat_update_enemy_bullets(state, campaign, SIM_STEP_DT);
    gameplay_combat_check_contacts(state, campaign);
    gameplay_update_alarm(state, SIM_STEP_DT);
}

/* A sector loaded and staged the way the shell stages it, with the player put
 * where the map says and nothing pressed. */
static bool stage_sector_at_its_spawn(GameplayState *state,
                                      CampaignState *campaign,
                                      size_t index, uint64_t seed)
{
    memset(state, 0, sizeof(*state));
    memset(campaign, 0, sizeof(*campaign));
    rng_seed(&state->rng, seed);
    gameplay_state_begin_level(state);
    Rng load = state->rng;
    if (!level_load_data(&state->level, EMBEDDED_LEVELS[index].name,
                         EMBEDDED_LEVELS[index].data,
                         EMBEDDED_LEVELS[index].size, &load))
        return false;
    gameplay_ai_spawn_level_entities(state);
    player_reset(&state->player, &state->level);
    state->player.hp = gameplay_player_max_hp(state);
    return true;
}

static void test_a_sector_gives_the_player_a_moment_to_read_it(void)
{
    static GameplayState state;
    static CampaignState campaign;

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        for (int seed = 0; seed < SPAWN_GRACE_SEEDS; ++seed)
        {
            REQUIRE(stage_sector_at_its_spawn(&state, &campaign, i,
                                              7000u * (uint64_t)(seed + 1) + i));
            int full = state.player.hp;

            float lost_at = -1.0f;
            for (int step = 0; step < SIM_STEPS(SPAWN_GRACE_SECONDS); ++step)
            {
                step_the_floor_around_a_still_player(&state, &campaign);
                if (state.player.hp < full || state.player.dying)
                {
                    lost_at = (float)step * SIM_STEP_DT;
                    break;
                }
            }
            /* Said out loud before the check, because `CHECK` prints the
             * expression and the expression names neither the sector nor how
             * far inside the window it went. The fix is a character in a map:
             * the nearest guard on the spawn floor wants to be about twelve
             * tiles off rather than eight, which is what the four moves this
             * test was written with all were — sectors 5, 6, 8 and 14. */
            if (lost_at >= 0.0f)
                fprintf(stderr,
                        "  sector %d costs a heart %.1fs after the reveal "
                        "(seed %d), inside the %.1fs the player is given to "
                        "read the floor\n",
                        (int)i + 1, lost_at, seed, SPAWN_GRACE_SECONDS);
            CHECK(lost_at < 0.0f);
        }
    }
}

/*
 * The other end of it, and the reason the rule above cannot be satisfied by
 * emptying the building.
 *
 * "Move the guard further from the spawn" is a fix that keeps working all the
 * way to "delete the guard", and while the rising hazard budget in
 * `test_campaign_levels_are_distinct_and_solvable` forbids the deletion, what it
 * cannot see is a man parked somewhere his patrol never comes back from. So this
 * asks the behaviour: does the floor eventually find somebody standing in the
 * middle of it?
 *
 * Campaign-wide rather than per sector, because one sector legitimately opens on
 * a pocket nothing watches and it is sector 5. Its spawn sits at the bottom of
 * the lift-shaft tower with a crate between it and the rest of the corridor, so
 * the man on that floor patrols the far half and is meant to be *seen before he
 * is met* — the whole plant is read from that corner. One such sector is a
 * design; three would be a campaign of empty rooms, which is what this counts.
 *
 * The climbs are exempt and structurally so: there is nobody out there at all.
 */
#define UNWATCHED_SPAWNS_ALLOWED 1

static void test_the_grace_period_is_not_an_empty_building(void)
{
    static GameplayState state;
    static CampaignState campaign;
    int unwatched = 0;
    int pockets[8] = {0};

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        bool noticed = false;
        for (int seed = 0; seed < SPAWN_GRACE_SEEDS && !noticed; ++seed)
        {
            REQUIRE(stage_sector_at_its_spawn(&state, &campaign, i,
                                              7000u * (uint64_t)(seed + 1) + i));
            if (state.level.map.mode == LEVEL_MODE_FACADE)
            {
                noticed = true; /* nobody out here to do the noticing */
                break;
            }
            /* Hearts topped up every step: what is being measured is whether
             * anybody comes, not how long the man lasts once they have. */
            for (int step = 0; step < SIM_STEPS(45.0f) && !noticed; ++step)
            {
                state.player.hp = gameplay_player_max_hp(&state);
                state.player.dying = false;
                step_the_floor_around_a_still_player(&state, &campaign);
                for (int e = 0; e < state.enemy_count; ++e)
                    if (!state.enemies[e].dead &&
                        state.enemies[e].encounter_decided)
                        noticed = true;
            }
        }
        if (!noticed)
        {
            /* Named only when the count is wrong, because one of these is the
             * expected answer and a gate that talks on a green run is a gate
             * whose output nobody reads. */
            if (unwatched < (int)(sizeof(pockets) / sizeof(pockets[0])))
                pockets[unwatched] = (int)i + 1;
            unwatched++;
        }
    }
    if (unwatched != UNWATCHED_SPAWNS_ALLOWED)
    {
        fprintf(stderr,
                "  %d sector(s) open on a pocket nothing watches, against %d "
                "expected:", unwatched, UNWATCHED_SPAWNS_ALLOWED);
        for (int i = 0; i < unwatched &&
                        i < (int)(sizeof(pockets) / sizeof(pockets[0])); ++i)
            fprintf(stderr, " %d", pockets[i]);
        fprintf(stderr, unwatched ? "\n" : " none\n");
    }
    /* Equality rather than a ceiling, and both halves matter. More than one is
     * a campaign going empty; *fewer* than one means the crate that splits
     * sector 5's corridor has moved and this test has stopped describing the
     * campaign it is named after. */
    CHECK(unwatched == UNWATCHED_SPAWNS_ALLOWED);
}

/* Every tile the box covers is masonry, which is a thing that can happen to a
 * corpse settling into a slab and must never happen to anything alive: a guard
 * in that state is invisible, unshootable and standing in a wall. */
static bool box_is_walled_in(const GameplayState *state,
                             float x, float y, float w, float h)
{
    int left = (int)floorf(x / TILE_SIZE);
    int right = (int)floorf((x + w - 1.0f) / TILE_SIZE);
    int top = (int)floorf(y / TILE_SIZE);
    int bottom = (int)floorf((y + h - 1.0f) / TILE_SIZE);
    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col)
            if (!level_is_solid(&state->level, col, row))
                return false;
    return true;
}

/*
 * The whole frame, in the shell's own order, with somebody leaning on the pad.
 *
 * Every other test in this suite drives a hand-picked handful of the frame —
 * the movement pass, or the combat pass, or the crates — which is what makes
 * them legible. What none of them does is run the passes *together*, in the
 * order [game.c](../src/game.c)'s `update_playing` runs them, so the one thing
 * this suite could not see was a pass leaving state the next pass mishandles.
 * The soak sweep runs the real order and stands still, so between them the two
 * gaps met in the middle: the ordering was covered by a player who did nothing
 * and the actions were covered outside the ordering.
 *
 * The input is seeded noise rather than a script, for the same reason
 * `test_the_loader_and_the_editor_survive_nonsense` generates its maps: what a
 * scripted player does is what the author thought to try. It asks only for
 * invariants — nothing carrying a NaN, nothing off the map, nothing alive
 * inside masonry, no count past its ceiling — because what a monkey *should*
 * achieve is not a question with an answer. It is worth most under
 * `make sanitize`, where it becomes a few hundred thousand steps of the real
 * frame with ASan watching.
 *
 * The two doors are left out on purpose: `gameplay_use_door` and
 * `gameplay_use_sublevel_door` hand the shell a level change, and the shell is
 * what loads the next map.
 *
 * **The climbs are in it, and leaving them out was the same defect one floor
 * down.** This said they were covered by "the facade's own frame and its own
 * bot", and `facade_bot_reaches_window` is not a frame: it drives
 * `gameplay_climb_update_player` and nothing else, because what it exists to ask
 * is whether a map has a dead end. So `update_facade_playing`'s order — the
 * player, then the hazards and the wind, then the pickups — was run by nothing
 * that pressed anything, on five of the seventeen sectors, while a comment said
 * otherwise. That is a check whose *name* overstated it, which is this tree's
 * own recurring shape. Each mode is driven in its own shell function's order and
 * the counts at the end require both to have been walked.
 */
#define MONKEY_SECONDS 6.0f

static void test_the_whole_frame_survives_a_monkey_on_the_controls(void)
{
    static GameplayState state;
    static CampaignState campaign;
    int interiors = 0;
    int climbs = 0;

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        REQUIRE(stage_sector_at_its_spawn(&state, &campaign, i, 31337u + i));
        bool facade = state.level.map.mode == LEVEL_MODE_FACADE;
        if (facade)
            climbs++;
        else
            interiors++;

        /* A stream of its own, so what the monkey presses cannot shift a single
         * seeded choice the simulation makes. */
        Rng thumbs;
        rng_seed(&thumbs, 4242u + i);

        float world_w = (float)state.level.map.width * TILE_SIZE;
        float world_h = (float)state.level.map.height * TILE_SIZE;

        for (int step = 0; step < SIM_STEPS(MONKEY_SECONDS); ++step)
        {
            Input input = {0};
            int roll = rng_range(&thumbs, 100);
            input.left = roll < 25;
            input.right = roll >= 25 && roll < 55;
            input.down = roll >= 55 && roll < 62;
            input.up = roll >= 62 && roll < 69;
            input.jump = rng_range(&thumbs, 100) < 6;
            input.jump_held = input.jump;
            input.shoot = rng_range(&thumbs, 100) < 8;
            input.interact = rng_range(&thumbs, 100) < 10;
            input.switch_weapon = rng_range(&thumbs, 100) < 3;
            input.switch_weapon_back = rng_range(&thumbs, 100) < 2;

            state.events.count = 0;
            if (facade)
            {
                /* `update_facade_playing`'s order. The five flags the shell
                 * clears between the two passes are cleared here too: they are
                 * press-edges, and a monkey holding one down every step is not
                 * the input the climb is written against. */
                campaign.level_elapsed_time += SIM_STEP_DT;
                if (state.invuln_timer > 0.0f)
                    state.invuln_timer -= SIM_STEP_DT;
                gameplay_climb_update_player(&state, &input, SIM_STEP_DT);
                input.jump = false;
                input.shoot = false;
                input.use_door = false;
                input.switch_weapon = false;
                input.switch_weapon_back = false;
                gameplay_climb_update(&state, SIM_STEP_DT);
                gameplay_collect_items(&state, &campaign, SIM_STEP_DT);
                while (campaign_check_extra_life(&campaign))
                    ;
            }
            else
            {
                bool was_grounded = state.player.on_ground;
                float previous_x = state.player.x;
                float previous_y = state.player.y;
                float previous_h = state.player.crawling
                                       ? (float)PLAYER_CRAWL_H
                                       : (float)PLAYER_H;
                gameplay_prepare_terminal(&state, &input, SIM_STEP_DT);
                gameplay_carry_player_on_elevator(&state, SIM_STEP_DT);
                gameplay_resolve_player_crush(&state);
                float fall_speed = player_update(&state.player, &state.level,
                                                 &input, SIM_STEP_DT);
                gameplay_update_body_drag(&state, &input);
                level_update_elevators(&state.level, SIM_STEP_DT);
                level_update_falling_platforms(&state.level, SIM_STEP_DT);
                level_update_moving_platforms(&state.level, SIM_STEP_DT);
                gameplay_update_crates(&state, &campaign, SIM_STEP_DT);
                gameplay_resolve_player_crates(&state, previous_x, previous_y,
                                               previous_h);
                gameplay_ride_platforms(&state, SIM_STEP_DT);
                gameplay_handle_player_landing(&state, was_grounded, fall_speed);
                gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
                gameplay_ai_update_spawns(&state, SIM_STEP_DT);
                gameplay_combat_handle_player_action(&state, &campaign, &input);
                gameplay_ai_update_movement(&state, SIM_STEP_DT);
                gameplay_collect_items(&state, &campaign, SIM_STEP_DT);
                gameplay_update_ammo_drops(&state, SIM_STEP_DT);
                gameplay_combat_update_hazards(&state);
                gameplay_combat_update_player_bullets(&state, &campaign,
                                                      SIM_STEP_DT);
                gameplay_combat_update_decoys(&state, SIM_STEP_DT);
                gameplay_ai_update_combat(&state, SIM_STEP_DT);
                gameplay_combat_update_enemy_bullets(&state, &campaign,
                                                     SIM_STEP_DT);
                gameplay_combat_check_contacts(&state, &campaign);
                gameplay_update_alarm(&state, SIM_STEP_DT);
            }

            /* A death is an ordinary outcome for a monkey; the shell would
             * respawn him at the checkpoint, so this does the same rather than
             * carrying a corpse through the rest of the run. */
            if (state.player.dying || state.player.hp <= 0)
            {
                gameplay_restore_checkpoint(&state);
                state.player.hp = gameplay_player_max_hp(&state);
                state.player.dying = false;
            }

            REQUIRE(!isnan(state.player.x) && !isnan(state.player.y));
            REQUIRE(state.player.x > -world_w && state.player.x < 2.0f * world_w);
            REQUIRE(state.player.y > -world_h && state.player.y < 2.0f * world_h);
            REQUIRE(state.player.hp <= gameplay_player_max_hp(&state));
            REQUIRE(state.enemy_count >= 0 && state.enemy_count <= MAX_ENEMIES);
            REQUIRE(state.dog_count >= 0 && state.dog_count <= MAX_DOGS);
            REQUIRE(state.events.count >= 0 &&
                    state.events.count <= MAX_GAME_EVENTS);

            for (int e = 0; e < state.enemy_count; ++e)
            {
                const Enemy *guard = &state.enemies[e];
                REQUIRE(!isnan(guard->x) && !isnan(guard->y));
                if (guard->dead || guard->climbing)
                    continue;
                REQUIRE(!box_is_walled_in(&state, guard->x, guard->y,
                                          ENEMY_W, ENEMY_H));
            }
            for (int d = 0; d < state.dog_count; ++d)
            {
                const Dog *dog = &state.dogs[d];
                REQUIRE(!isnan(dog->x) && !isnan(dog->y));
                if (dog->dead)
                    continue;
                REQUIRE(!box_is_walled_in(&state, dog->x, dog->y,
                                          DOG_W, DOG_H));
            }
        }
    }
    /* And it actually walked the campaign rather than skipping all of it —
     * both halves of it, which is what the facade branch is here for. */
    CHECK(interiors == (int)EMBEDDED_LEVEL_COUNT - CAMPAIGN_CLIMB_SECTOR_COUNT);
    CHECK(climbs == CAMPAIGN_CLIMB_SECTOR_COUNT);
}

/*
 * Four rooms, one per sector that has a `U`, and every one held to the bar the
 * single shipped room was.
 *
 * There used to be one room and `EMBEDDED_SUBLEVELS[0]` hard-wired into the
 * shell, so this test only ever had one map to walk. Now the door is resolved
 * by theme (`level_theme_sublevel`) and the set has to answer as a set: same
 * three pickups in each, because the campaign is balanced on four grenades
 * coming out of these doors, and a different plan in each, because the
 * repetition is what was wrong with one room four times.
 */
static void test_embedded_restroom_sublevels(void)
{
    CHECK(EMBEDDED_SUBLEVEL_COUNT == 4);

    int widths[8] = {0};
    int heights[8] = {0};

    for (size_t index = 0; index < EMBEDDED_SUBLEVEL_COUNT; ++index)
    {
        static Level restroom;
        Rng rng;
        rng_seed(&rng, 2026 + (uint64_t)index);
        CHECK(level_load_data(&restroom, EMBEDDED_SUBLEVELS[index].name,
                              EMBEDDED_SUBLEVELS[index].data,
                              EMBEDDED_SUBLEVELS[index].size, &rng));

        /* The fittings are what set the theme, so a room that forgot them is
         * also a room drawn in the plant hall's steel. */
        CHECK(restroom.map.theme == LEVEL_THEME_RESTROOM);
        CHECK(restroom.map.has_sublevel_return);
        CHECK(!restroom.map.has_exit);
        CHECK(!restroom.map.has_window);
        CHECK(restroom.map.door_count == 0);

        int basins = 0;
        int stalls = 0;
        for (int i = 0; i < restroom.map.decoration_count; ++i)
        {
            DecorationType type = restroom.map.decorations[i].type;
            basins += type == DECOR_RESTROOM_BASIN;
            stalls += type == DECOR_RESTROOM_STALL_OPEN ||
                      type == DECOR_RESTROOM_STALL_CLOSED ||
                      type == DECOR_RESTROOM_TOILET;
        }
        CHECK(basins >= 2);
        CHECK(stalls >= 2);

        /* The reward is deliberately identical in all four: docs/gameplay.md
         * counts four grenades out of these doors on top of the campaign's own
         * sixteen, and that line moves the moment one room pays differently. */
        int guns = 0;
        int grenades = 0;
        int medkits = 0;
        for (int i = 0; i < restroom.runtime.item_count; ++i)
        {
            guns += restroom.runtime.items[i].type == ITEM_GUN;
            grenades += restroom.runtime.items[i].type == ITEM_GRENADE;
            medkits += restroom.runtime.items[i].type == ITEM_MEDKIT;
        }
        CHECK(guns == 1);
        CHECK(grenades == 1);
        CHECK(medkits == 1);

        /* Every room earns its detour: it is guarded and it is climbed. */
        CHECK(restroom.map.enemy_count >= 1);

        int ladder_tiles = 0;
        for (int row = 0; row < restroom.map.height; ++row)
            for (int col = 0; col < restroom.map.width; ++col)
                ladder_tiles += restroom.map.tiles[row][col] == TILE_LADDER;
        CHECK(ladder_tiles >= 4);

        /* Both the grenade and the medkit sit above the floor the door is on,
         * or the climb up is decorative and the room is a corridor with a
         * medkit in it. */
        int high_items = 0;
        for (int i = 0; i < restroom.runtime.item_count; ++i)
            if (restroom.runtime.items[i].y <
                restroom.map.sublevel_return_row * (float)TILE_SIZE)
                high_items++;
        CHECK(high_items == 2);

        /*
         * And high up is not the same as gettable.
         *
         * Every campaign sector is walked by the route model; these rooms are
         * not campaign sectors, so for a while the model never ran here at all
         * and the one medkit sat across a two-tile gap under a two-row ceiling
         * — the exact jump the legend says is not on, and one a player could
         * only land inside a 25px window of where they started the run-up. The
         * whole reason to spend the detour must not be a timing trick, in any
         * of the four.
         */
        static RouteMap route;
        route_map_init(&route, &restroom);
        route_flood(&route, route_player_start(&route));
        for (int i = 0; i < restroom.runtime.item_count; ++i)
        {
            CHECK(route_reaches(&route,
                                (int)(restroom.runtime.items[i].x / TILE_SIZE),
                                (int)(restroom.runtime.items[i].y / TILE_SIZE)));
        }
        /* And the way back out, from wherever the detour ended. */
        CHECK(route_reaches(&route, restroom.map.sublevel_return_col,
                            restroom.map.sublevel_return_row));
        CHECK(route_never_strands(&route,
                                  (RouteCell){restroom.map.sublevel_return_col,
                                              restroom.map.sublevel_return_row}));

        /* No two rooms share a footprint, for the reason no two sectors do:
         * the shape is the first thing the player recognises, and four rooms
         * the same shape are one room shown four times however they are
         * furnished. */
        for (size_t seen = 0; seen < index; ++seen)
        {
            CHECK(widths[seen] != restroom.map.width ||
                  heights[seen] != restroom.map.height);
        }
        widths[index] = restroom.map.width;
        heights[index] = restroom.map.height;
    }
}

/*
 * Every theme names a score, and never the title's.
 *
 * `THEME_MUSIC` is a designated-initializer array indexed by `LevelTheme`, and
 * that shape does not fail to build when a row is missing — it zero-fills. The
 * zero here is `MUSIC_INTRO`, so an eighteenth theme added without a line would
 * play the *title screen theme* over a sector, and the only thing that would
 * ever have said so is somebody playing that floor with the sound on.
 *
 * It went unchecked because the table sat in `level_art.c`, which links SDL and
 * so is reachable by no test at all — the exact reason the note on
 * `THEME_SUBLEVEL` gives for keeping the room table out of there. The score is
 * level data by the same argument, so it moved next to it and this is the check
 * the move was for. `MUSIC_PURSUIT` is excluded with the title's for the same
 * reason: it belongs to the drive, and a sector scored with it would be a
 * prologue cue playing inside the building.
 */
static void test_every_theme_names_a_score_of_its_own(void)
{
    bool taken[MUSIC_TRACK_COUNT] = {false};
    for (int theme = 0; theme < LEVEL_THEME_COUNT; ++theme)
    {
        MusicTrack track = level_theme_music((LevelTheme)theme);
        CHECK(track > MUSIC_PURSUIT && track < MUSIC_TRACK_COUNT);
        /* One to one, which is what `test_campaign_themes_keep_changing` rests
         * on when it concludes that no two consecutive sectors share a loop. */
        CHECK(!taken[track]);
        taken[track] = true;
    }

    /* And the campaign uses every one of them: eighteen themes, eighteen
     * floors' worth of sound, none of it written for a sector nobody visits.
     * That is also what makes the soak's coverage of the art table real —
     * `THEME_ART` cannot be reached from here, but a sweep that plays all
     * seventeen sectors and a restroom draws every row of it. */
    bool seen[LEVEL_THEME_COUNT] = {false};
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        static Level level;
        Rng rng;
        rng_seed(&rng, 1400 + (uint64_t)i);
        CHECK(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data,
                              EMBEDDED_LEVELS[i].size, &rng));
        seen[level.map.theme] = true;
    }
    for (size_t i = 0; i < EMBEDDED_SUBLEVEL_COUNT; ++i)
    {
        static Level room;
        Rng rng;
        rng_seed(&rng, 1500 + (uint64_t)i);
        CHECK(level_load_data(&room, EMBEDDED_SUBLEVELS[i].name,
                              EMBEDDED_SUBLEVELS[i].data,
                              EMBEDDED_SUBLEVELS[i].size, &rng));
        seen[room.map.theme] = true;
    }
    for (int theme = 0; theme < LEVEL_THEME_COUNT; ++theme)
        CHECK(seen[theme]);
}

/*
 * The cordon fades as the climb rises, and it is asked of the theme.
 *
 * `facade_cordon` washes the lower face of a climb in the blue and red of the
 * street below, strongest on the first and lowest wall and gone by the top:
 * "the climb is also a climb away from it", as docs/story.md puts it. That was
 * a `switch` in `level_art.c` keyed on `art->backdrop`, and there are four
 * backdrops for five climbs, because `FACADE_SLEET` borrows the storm's. So the
 * highest wall in the game, two floors under the roof, answered as the second
 * one and washed itself with more street than the two climbs below it.
 *
 * Nothing failed and nothing could: the number lived in a renderer, and this
 * suite links no SDL. It is a table in `level.c` now for the same reason
 * `THEME_MUSIC` is, and this is the check that reason buys.
 *
 * On what a missing row does: a climb added at the *top* with no row of its own
 * zero-fills to "no cordon", which is the right answer for the top of a tower,
 * and this deliberately does not fail it. One added *below* an existing climb
 * gets the same nought and lands above a value that is not, which is what the
 * monotonicity below catches — and that is the direction the mistake actually
 * comes from, because a new climb is a new sector inserted into a campaign.
 */
static void test_the_cordon_fades_as_the_climb_rises(void)
{
    float previous = 2.0f;
    int climbs = 0;
    int lit = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        static Level level;
        Rng rng;
        rng_seed(&rng, 1600 + (uint64_t)i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        float cordon = level_theme_cordon(level.map.theme);
        if (level.map.mode != LEVEL_MODE_FACADE)
        {
            /* No wall to wash: an interior that answered anything would put a
             * squad car's lights on a corridor. */
            CHECK(cordon == 0.0f);
            continue;
        }
        climbs++;
        CHECK(cordon >= 0.0f && cordon <= 1.0f);
        /* Up the tower is away from the street, every time. */
        CHECK(cordon <= previous);
        if (climbs == 1)
            CHECK(cordon == 1.0f); /* the first climb, and the lowest */
        lit += cordon > 0.0f;
        previous = cordon;
    }
    CHECK(climbs == CAMPAIGN_CLIMB_SECTOR_COUNT);
    /* It is a fade rather than a switch: more than one wall carries some of it
     * and the top of the tower carries none. */
    CHECK(lit >= 2);
    CHECK(lit < climbs);
    CHECK(previous == 0.0f);

    /* And the sleet wall is the case that broke it: it shares the storm's
     * backdrop and must not share the storm's street. */
    CHECK(level_theme_cordon(LEVEL_THEME_FACADE_SLEET) <
          level_theme_cordon(LEVEL_THEME_FACADE_STORM));
    CHECK(level_theme_cordon(LEVEL_THEME_RESTROOM) == 0.0f);
}

/*
 * The door opens on a room that exists, and the four that have one get four
 * different rooms.
 *
 * `level_theme_sublevel` is a filename written down in C, which is only safe
 * while something checks it against the files actually embedded — otherwise a
 * renamed map is a `U` that silently falls back to the lobby's washroom in a
 * sector two floors under the roof, and nothing but playing it would say so.
 */
static void test_every_restroom_theme_names_a_room_that_exists(void)
{
    for (int theme = 0; theme < LEVEL_THEME_COUNT; ++theme)
    {
        const char *stem = level_theme_sublevel((LevelTheme)theme);
        CHECK(stem != NULL);
        int matches = 0;
        for (size_t i = 0; i < EMBEDDED_SUBLEVEL_COUNT; ++i)
            matches += level_sublevel_name_is(EMBEDDED_SUBLEVELS[i].name, stem);
        CHECK(matches == 1);
    }

    /* And the sectors that actually have a `U` do not share one between them,
     * which is the whole point of resolving the door by theme. */
    const char *taken[8] = {0};
    int doors = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        static Level level;
        Rng rng;
        rng_seed(&rng, 900 + (uint64_t)i);
        CHECK(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data,
                              EMBEDDED_LEVELS[i].size, &rng));
        if (!level.map.has_sublevel_entrance)
            continue;

        const char *stem = level_theme_sublevel(level.map.theme);
        for (int seen = 0; seen < doors; ++seen)
            CHECK(strcmp(taken[seen], stem) != 0);
        taken[doors++] = stem;
    }
    CHECK(doors == 4);
}

/* ---- Level editor ------------------------------------------------------ */

/*
 * The editor keeps a map as the characters it was authored with and hands the
 * text back to `level_load_data` to find out what it means. That only holds if
 * a load followed by a save is a no-op: the moment saving reflows a map, using
 * the editor on one sector would rewrite it wholesale and bury the actual edit
 * in the diff.
 */
static void test_editor_round_trips_every_map_file(void)
{
    static EditorDoc doc;
    static char text[MAX_LEVEL_WIDTH * MAX_LEVEL_HEIGHT * 2];

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT + EMBEDDED_SUBLEVEL_COUNT; ++i)
    {
        const EmbeddedLevelData *source =
            i < EMBEDDED_LEVEL_COUNT
                ? &EMBEDDED_LEVELS[i]
                : &EMBEDDED_SUBLEVELS[i - EMBEDDED_LEVEL_COUNT];

        CHECK(editor_doc_parse(&doc, source->data, source->size));
        size_t length = editor_doc_serialize(&doc, text, sizeof(text));
        CHECK(length == source->size);
        CHECK(memcmp(text, source->data, source->size) == 0);

        /* And the document agrees with the parser about what it holds. */
        Level from_doc;
        Level from_file;
        Rng doc_rng;
        Rng file_rng;
        rng_seed(&doc_rng, 31);
        rng_seed(&file_rng, 31);
        CHECK(editor_doc_build_level(&doc, &from_doc, 31));
        CHECK(level_load_data(&from_file, source->name, source->data,
                              source->size, &file_rng));
        (void)doc_rng;
        CHECK(from_doc.map.width == from_file.map.width);
        CHECK(from_doc.map.height == from_file.map.height);
        CHECK(from_doc.map.mode == from_file.map.mode);
        CHECK(from_doc.map.theme == from_file.map.theme);
        CHECK(from_doc.map.enemy_count == from_file.map.enemy_count);
        CHECK(from_doc.map.door_count == from_file.map.door_count);
        for (int door = 0; door < from_file.map.door_count; ++door)
        {
            CHECK(from_doc.map.door_spawn_counts[door] ==
                  from_file.map.door_spawn_counts[door]);
        }
    }

    /* A campaign path carries its sector number; anything else does not. */
    CHECK(editor_path_level_number("levels/level7.txt") == 7);
    CHECK(editor_path_level_number("levels/level15.txt") == 15);
    CHECK(editor_path_level_number("levels/sublevels/restroom_lobby.txt") == 0);
    CHECK(editor_path_level_number("levels/level7.bak") == 0);
}

static void test_editor_edits_and_undo(void)
{
    static EditorDoc doc;
    editor_doc_new(&doc, 12, 8, false, LEVEL_THEME_OFFICE);

    /* A new interior is a sealed box the player can already stand in. */
    CHECK(editor_doc_get(&doc, 0, 0) == '#');
    CHECK(editor_doc_get(&doc, 5, 7) == '#');
    CHECK(editor_doc_get(&doc, 2, 6) == 'S');

    editor_doc_checkpoint(&doc);
    CHECK(editor_doc_set(&doc, 5, 6, 'M'));
    CHECK(editor_doc_get(&doc, 5, 6) == 'M');
    CHECK(editor_doc_undo(&doc));
    CHECK(editor_doc_get(&doc, 5, 6) == ' ');
    CHECK(editor_doc_redo(&doc));
    CHECK(editor_doc_get(&doc, 5, 6) == 'M');

    /* Inserting a row pushes the floor down rather than overwriting it. */
    editor_doc_checkpoint(&doc);
    CHECK(editor_doc_insert_row(&doc, 6));
    CHECK(doc.grid.height == 9);
    CHECK(editor_doc_get(&doc, 5, 7) == 'M');
    CHECK(editor_doc_get(&doc, 5, 6) == ' ');
    CHECK(editor_doc_undo(&doc));
    CHECK(doc.grid.height == 8);
    CHECK(editor_doc_get(&doc, 5, 6) == 'M');

    /* Column geometry works the same way, and mirroring is its own edit. */
    editor_doc_checkpoint(&doc);
    CHECK(editor_doc_insert_col(&doc, 3));
    CHECK(doc.grid.width == 13);
    CHECK(editor_doc_get(&doc, 6, 6) == 'M');
    CHECK(editor_doc_delete_col(&doc, 3));
    CHECK(doc.grid.width == 12);
    CHECK(editor_doc_get(&doc, 5, 6) == 'M');

    editor_doc_mirror(&doc, 0, 6, 11, 6, true);
    CHECK(editor_doc_get(&doc, 6, 6) == 'M');
    CHECK(editor_doc_get(&doc, 9, 6) == 'S');

    /* A facade pads with '.' so a blank column reads as sky, not as a hole. */
    editor_doc_new(&doc, 10, 10, true, LEVEL_THEME_FACADE_STORM);
    CHECK(editor_doc_fill_char(&doc) == '.');
    CHECK(editor_doc_get(&doc, 0, 0) == '.');

    static char text[4096];
    size_t length = editor_doc_serialize(&doc, text, sizeof(text));
    CHECK(length > 0);
    CHECK(strstr(text, "\nMODE FACADE\n") != NULL);
    CHECK(strstr(text, "\nTHEME FACADE_STORM\n") != NULL);

    Level level;
    CHECK(editor_doc_build_level(&doc, &level, 5));
    CHECK(level.map.mode == LEVEL_MODE_FACADE);
    CHECK(level.map.theme == LEVEL_THEME_FACADE_STORM);
}

/*
 * Resizing a map, deleting a row, and the trip through an actual file.
 *
 * `editor_doc_resize`, `editor_doc_delete_row`, `editor_doc_load` and
 * `editor_doc_save` were four of the fourteen functions on the SDL-free side of
 * the tree that the suite had never executed. The others in this file are
 * tested because they are edits; these are the two shapes an edit is *not* —
 * changing how big the paper is, and putting it on disk — and both are things
 * an author does to a shipped map before saving over it.
 *
 * `test_editor_round_trips_every_map_file` already proves the *text* survives,
 * by parsing and serialising in memory. It has never touched the filesystem, so
 * the pair of functions that actually read and write one were the part of that
 * claim nothing stood behind.
 */
static void test_editor_resizes_deletes_and_survives_a_real_file(void)
{
    static EditorDoc doc;
    editor_doc_new(&doc, 12, 8, false, LEVEL_THEME_OFFICE);
    CHECK(editor_doc_set(&doc, 5, 6, 'M'));
    CHECK(editor_doc_set(&doc, 7, 6, 'K'));

    /* Growing keeps what was drawn and fills the new ground with the sector's
     * own empty tile. */
    editor_doc_checkpoint(&doc);
    CHECK(editor_doc_resize(&doc, 16, 10));
    CHECK(doc.grid.width == 16);
    CHECK(doc.grid.height == 10);
    CHECK(editor_doc_get(&doc, 5, 6) == 'M');
    CHECK(editor_doc_get(&doc, 14, 9) == editor_doc_fill_char(&doc));

    /* Shrinking past something drops it, which is the whole risk of the tool
     * and the reason the edit is undoable. */
    CHECK(editor_doc_resize(&doc, 6, 10));
    CHECK(doc.grid.width == 6);
    CHECK(editor_doc_get(&doc, 5, 6) == 'M');
    CHECK(editor_doc_undo(&doc));
    CHECK(doc.grid.width == 12);
    CHECK(doc.grid.height == 8);
    CHECK(editor_doc_get(&doc, 7, 6) == 'K');

    /* A deleted row pulls everything under it up by one. */
    editor_doc_checkpoint(&doc);
    int height = doc.grid.height;
    CHECK(editor_doc_delete_row(&doc, 5));
    CHECK(doc.grid.height == height - 1);
    CHECK(editor_doc_get(&doc, 5, 5) == 'M');
    CHECK(editor_doc_undo(&doc));
    CHECK(doc.grid.height == height);
    CHECK(editor_doc_get(&doc, 5, 6) == 'M');

    /* And the round trip, through a file rather than through a buffer. The
     * path is under build/, which is where everything else this suite is built
     * from already lives and is not part of the repository. */
    static const char *path = "build/editor_doc_round_trip.txt";
    static char before[8192];
    static char after[8192];
    size_t written = editor_doc_serialize(&doc, before, sizeof(before));
    REQUIRE(written > 0);

    REQUIRE(editor_doc_save(&doc, path));
    /* Saving is what makes a document clean, and it remembers where it went. */
    CHECK(!doc.dirty);
    CHECK(strcmp(doc.path, path) == 0);

    static EditorDoc reopened;
    REQUIRE(editor_doc_load(&reopened, path));
    CHECK(reopened.grid.width == doc.grid.width);
    CHECK(reopened.grid.height == doc.grid.height);
    CHECK(reopened.grid.theme == doc.grid.theme);
    CHECK(strcmp(reopened.path, path) == 0);
    CHECK(!reopened.dirty);
    /* A freshly opened map has nothing to undo: the history is the session's,
     * not the file's. */
    CHECK(!editor_doc_can_undo(&reopened));
    CHECK(!editor_doc_can_redo(&reopened));
    size_t read_back = editor_doc_serialize(&reopened, after, sizeof(after));
    CHECK(read_back == written);
    CHECK(strcmp(before, after) == 0);

    /*
     * The save went through a temporary and moved onto the target, so there is
     * nothing beside the map when it is done.
     *
     * A `.tmp` left lying next to a level file is worse than untidy: the editor
     * opens what it is pointed at, `check_docs.py` and the embed step walk
     * `levels/`, and a stale half-map in that directory is a map as far as any
     * of them can tell.
     */
    static char leftover[ED_MAX_PATH + 8];
    snprintf(leftover, sizeof(leftover), "%s.tmp", path);
    CHECK(!editor_doc_load(&reopened, leftover));

    /*
     * And saving over a map that is already there replaces it whole.
     *
     * This is the case the old `fopen(path, "wb")` could not survive: it
     * truncated first and wrote afterwards, so anything that went wrong in
     * between cost the author the file they started from. The rename cannot
     * leave a shorter one behind, and the reopened document proves the target
     * carries the *second* map rather than a mixture of the two.
     */
    CHECK(editor_doc_set(&doc, 3, 3, 'X'));
    REQUIRE(editor_doc_save(&doc, path));
    written = editor_doc_serialize(&doc, before, sizeof(before));
    REQUIRE(editor_doc_load(&reopened, path));
    read_back = editor_doc_serialize(&reopened, after, sizeof(after));
    CHECK(read_back == written);
    CHECK(strcmp(before, after) == 0);
    CHECK(editor_doc_get(&reopened, 3, 3) == 'X');
    CHECK(!editor_doc_load(&reopened, leftover));

    /* A path that is not there is a false rather than a half-loaded document. */
    static EditorDoc missing;
    CHECK(!editor_doc_load(&missing, "build/no_such_map_file.txt"));

    /* A directory nobody made is a refusal rather than a crash — and it must
     * not leave its own temporary behind in the attempt. */
    static const char *nowhere = "build/no_such_dir/map.txt";
    CHECK(!editor_doc_save(&doc, nowhere));
    static char nowhere_temp[ED_MAX_PATH + 8];
    snprintf(nowhere_temp, sizeof(nowhere_temp), "%s.tmp", nowhere);
    CHECK(!editor_doc_load(&reopened, nowhere_temp));

    remove(path);
}

/* Point fd 2 at nothing, and hand back the stream so the caller can close it.
 * NULL if the platform would not have it, in which case the only cost is that
 * the sweep below is noisy. */
static FILE *silence_stderr(void)
{
    FILE *null = fopen("/dev/null", "w");
    if (null == NULL)
        return NULL;
    fflush(stderr);
    dup2(fileno(null), STDERR_FILENO);
    return null;
}

/*
 * Nonsense is refused rather than crashed on.
 *
 * Every other test in this file hands the loader a map somebody meant. The
 * editor's whole job is to open a file somebody is *part way through* meaning —
 * half a grid, a `THEME` line that is a typo, a row of characters the legend has
 * never heard of, two hundred `S` in a column — and both the parser and the
 * validator have to come back with an answer rather than with a signal. Nothing
 * had ever put a byte in front of them that was not already a level.
 *
 * Deterministic, because a fuzzer whose corpus changes every run is a test that
 * fails on somebody else's machine and passes on yours: the generator is this
 * file's own xorshift, seeded once. What it is worth is mostly what it is worth
 * under `make sanitize`, where it is a few thousand passes over the parser's
 * bounds arithmetic with ASan watching — which is the half of this the ordinary
 * build cannot check at all.
 *
 * The only assertion is that both halves *answer*. What a refusal says is the
 * business of the tests above; that there is one, and that it does not run off
 * the end of a grid on the way, is this one's.
 */
static void test_the_loader_and_the_editor_survive_nonsense(void)
{
    /* Every legend character, the metadata leaders, and a handful of bytes that
     * are in no map at all — because "not in the legend" is a path too. */
    static const char alphabet[] =
        "#%H .CGNKZ!*MWQJfkXO^BLTAIcdinstgmaejlwSEYDURqbupozVFPrv\n\t|@1";
    static const char *tails[] = {
        "", "\nMODE FACADE\n", "\nTHEME LOBBY\n", "\nTHEME BOGUS\n",
        "\nSPAWNS 1 2 3\n", "\nSPAWNS\n", "\nMODE\n", "\nSPAWNS -1 9999999\n",
        /* Longer than an `int`, which is the number this corpus did not have.
         * The loader has guarded its digit run against overflow since it was
         * written and the editor's copy of the same loop had not, so the one
         * input that would have told anybody was a number of ten digits or
         * more — and the longest here was seven. Signed overflow is undefined
         * behaviour and `make sanitize` compiles `editor_doc.c` with UBSan
         * watching, so this line is what turns that job into the check it
         * already looked like. */
        "\nSPAWNS 99999999999999\n", "\nSPAWNS 2147483648\n",
        "\nSPAWNS 1x2\n"};

    static char text[24 * 1024];
    static Level level;
    static EditorDoc doc;
    static EdReport report;
    static EdCampaign campaign;
    Rng rng;
    rng_seed(&rng, 20240818);

    /*
     * Stderr goes in the bin for the duration, and only here.
     *
     * A refusal *should* print: `level_load_data` explaining itself is how an
     * author finds out why the map they just drew will not load, and every
     * other test in this file that hands it a bad map is checking exactly that
     * line. This one hands it three thousand, so the explanation is three
     * thousand lines of noise around the one line that says whether the suite
     * passed. Restored with `dup` rather than by reopening a terminal, because
     * `make test` is nearly always looking at a pipe.
     */
    fflush(stderr);
    int saved_stderr = dup(STDERR_FILENO);
    FILE *sink = silence_stderr();

    int parsed_any = 0;
    int refused_any = 0;
    int overflowed = 0;
    int miscounted = 0;
    const int rounds = 3000;
    for (int round = 0; round < rounds; ++round)
    {
        /* Sizes either side of the loader's own caps, so the paths that refuse
         * an oversized map are walked as well as the ones that accept. */
        int width = 1 + rng_range(&rng, MAX_LEVEL_WIDTH + 8);
        int height = 1 + rng_range(&rng, MAX_LEVEL_HEIGHT + 8);
        size_t length = 0;
        for (int row = 0; row < height; ++row)
        {
            if (length + (size_t)width + 2 >= sizeof(text))
                break;
            for (int col = 0; col < width; ++col)
                text[length++] = alphabet[rng_range(&rng, (int)sizeof(alphabet) - 1)];
            text[length++] = '\n';
        }
        const char *tail = tails[rng_range(&rng, (int)(sizeof(tails) /
                                                       sizeof(tails[0])))];
        size_t tail_length = strlen(tail);
        if (length + tail_length + 1 < sizeof(text))
        {
            memcpy(text + length, tail, tail_length);
            length += tail_length;
        }
        text[length] = '\0';

        Rng level_rng;
        rng_seed(&level_rng, (uint64_t)round + 1);
        bool parsed = level_load_data(&level, "fuzz", text, length, &level_rng);
        parsed_any += parsed;
        refused_any += !parsed;

        /* And the editor's opinion of the same bytes. It is handed a sector
         * number, because that is what turns on the cross-sector rules — the
         * block a shipped-campaign test once skipped entirely by handing over an
         * empty path. */
        if (editor_doc_parse(&doc, text, length))
        {
            snprintf(doc.path, sizeof(doc.path), "levels/level%d.txt",
                     1 + (round % CAMPAIGN_SECTORS));
            editor_validate(&doc, &level, parsed, &campaign, &report);
            /* Counted rather than CHECKed, because stderr is in the bin right
             * now: a failing CHECK in here would bump the count and post its
             * `file:line` to /dev/null, leaving the suite reporting a failure
             * with no way to find it. Nothing inside this loop may assert. */
            overflowed += report.count > ED_MAX_FINDINGS;
            miscounted += report.errors + report.warnings + report.notes < 0;
        }
    }

    fflush(stderr);
    if (saved_stderr >= 0)
    {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    if (sink != NULL)
        fclose(sink);

    /* A report that overflowed is a report that stopped counting, which is the
     * one way this could pass while saying nothing. */
    CHECK(overflowed == 0);
    CHECK(miscounted == 0);
    /* And both outcomes actually happened, or the generator has drifted into
     * making only one kind of file and this has stopped being a sweep. */
    CHECK(refused_any > 0);
    CHECK(parsed_any > 0);
}

/*
 * The editor's report is the test suite's own opinion, given while the map is
 * being drawn. These pin that the checks actually fire: a report that says
 * nothing about a broken map is worse than no report, because the author
 * believes it.
 */
static bool report_mentions(const EdReport *report, EdSeverity severity,
                            const char *fragment)
{
    for (int i = 0; i < report->count; ++i)
    {
        if (report->findings[i].severity != severity)
            continue;
        if (strstr(report->findings[i].text, fragment) != NULL)
            return true;
    }
    return false;
}

static void validate_text(const char *text, const char *path, EdReport *report)
{
    static EditorDoc doc;
    static Level level;
    static EdCampaign campaign;
    CHECK(editor_doc_parse(&doc, text, strlen(text)));
    if (path != NULL)
        snprintf(doc.path, sizeof(doc.path), "%s", path);
    bool parsed = editor_doc_build_level(&doc, &level, 11);
    editor_validate(&doc, &level, parsed, &campaign, report);
}

/*
 * The editor and the loader read a SPAWNS line the same way.
 *
 * The same reasoning written out twice, and only one of the two was finished.
 * `level_load_data` refuses a token that is not a number, refuses a digit run
 * that will not fit an `int`, and refuses junk hanging off the end of one;
 * `editor_doc_parse` stopped at the first non-digit without a word and
 * multiplied by ten until it wrapped. Both halves showed. `SPAWNS -1 4` opened
 * in the editor as *no* spawns at all, so a hand-edited map could be opened,
 * drawn on and saved with its door counts silently gone — `editor_doc_serialize`
 * writes the line only when there is something in it. And
 * `SPAWNS 99999999999999` is signed integer overflow: undefined behaviour, in a
 * translation unit this repository already builds under UBSan, reached by
 * nothing because the fuzz corpus stopped at seven digits.
 *
 * The trap in checking this is that the editor answers in **two halves**, and
 * only one of them is the parser. Whether the *line* is well formed is a parse
 * question. Whether there is one count per door is a question about the map and
 * belongs to `editor_validate`, because an author halfway through adding a door
 * must still be able to open the file and be told. A check that asked only
 * "does the editor end up refusing this" would therefore pass with the parser
 * fully broken, because the validator catches the miscount that the discarded
 * values leave behind — and the values would still be gone.
 *
 * So the fault is separated from the map rather than listed by hand: with two
 * doors, a line carrying **two** tokens can only be refused by the loader for
 * being malformed, and that is the refusal the parser owes. Counting tokens
 * needs neither parser's opinion, which is what keeps this from being a copy of
 * the thing it checks.
 *
 * And the same split is what the loader's *message* owes an author. It used to
 * be one sentence naming two numbers, so `SPAWNS -1` on a map with no doors
 * printed `expected 0 values, found 0` and refused — a refusal nobody can act
 * on, quoting a pair of figures that agree. The last check below reads what the
 * loader actually said, because a diagnostic that blames the wrong thing is the
 * one kind of defect a passing build will never mention.
 */
static int spawns_token_count(const char *line)
{
    int tokens = 0;
    for (const char *at = line + 7; *at != '\0';)
    {
        while (*at == ' ' || *at == '\t')
            at++;
        if (*at == '\0')
            break;
        tokens++;
        while (*at != '\0' && *at != ' ' && *at != '\t')
            at++;
    }
    return tokens;
}

static void test_the_editor_and_the_loader_read_a_spawns_line_the_same_way(void)
{
    /* One pair of doors, so a well-formed line is exactly two counts. */
    static const char head[] =
        "########\n"
        "#S D  D#\n"
        "#     E#\n"
        "########\n"
        "\n"
        "THEME OFFICE\n";
    static const char *lines[] = {
        "SPAWNS 2 3",              /* well formed */
        "SPAWNS 0 0",              /* well formed, and nought is a count */
        "SPAWNS  2   3 ",          /* well formed, spaced out */
        "SPAWNS -1 4",             /* the live one: a sign is not a digit */
        "SPAWNS x 3",              /* not a number at all */
        "SPAWNS 1x 2",             /* junk on the end of a number */
        "SPAWNS 2147483648 3",     /* one past INT_MAX */
        "SPAWNS 99999999999999 3", /* the overflow the corpus never reached */
        "SPAWNS 2",                /* one count short: the map's fault, not the
                                    * line's, so the validator owns it */
        "SPAWNS 2 3 4",            /* one count long, likewise */
    };

    static char text[512];
    static EditorDoc doc;
    static Level level;
    static Level built;
    static EdCampaign campaign;
    static EdReport report;
    Rng rng;
    rng_seed(&rng, 8171);

    /* Caught rather than binned, because the message is half of what was
     * wrong: it is read back below. Off the terminal either way — ten
     * refusals around one line of result is how a suite's real output gets
     * lost, which is why the nonsense sweep above sends its own to /dev/null. */
    fflush(stderr);
    int saved_stderr = dup(STDERR_FILENO);
    FILE *sink = tmpfile();
    if (sink != NULL)
        dup2(fileno(sink), STDERR_FILENO);

    int agreed = 0;
    int accepted = 0;
    int malformed_refused_at_parse = 0;
    int malformed = 0;
    int blamed_the_count = 0;
    int miscounted = 0;
    int miscounts_named = 0;
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i)
    {
        int written = snprintf(text, sizeof(text), "%s%s\n", head, lines[i]);
        if (written < 0 || (size_t)written >= sizeof(text))
            continue;
        char said[256];
        said[0] = '\0';
        if (sink != NULL)
        {
            fflush(stderr);
            rewind(sink);
            if (ftruncate(fileno(sink), 0) != 0)
                said[0] = '\0';
        }
        Rng load = rng;
        bool game = level_load_data(&level, "spawns", text, (size_t)written,
                                    &load);
        int doors = level.map.door_count;
        if (sink != NULL)
        {
            fflush(stderr);
            rewind(sink);
            size_t read_back = fread(said, 1, sizeof(said) - 1, sink);
            said[read_back] = '\0';
        }
        bool says_miscount = strstr(said, "one count per door") != NULL;

        bool parsed = editor_doc_parse(&doc, text, (size_t)written);
        bool editor = parsed;
        if (parsed)
        {
            memset(&campaign, 0, sizeof(campaign));
            bool built_ok = editor_doc_build_level(&doc, &built, 11);
            editor_validate(&doc, &built, built_ok, &campaign, &report);
            if (report_mentions(&report, ED_SEV_ERROR, "SPAWNS"))
                editor = false;
        }

        /* The whole verdict first: whatever the game will not load, the editor
         * will not let an author believe is finished. */
        if (game == editor)
            agreed++;
        accepted += game;

        /* And then the half that belongs to the parser. The token count is
         * right, so a refusal from the loader is a refusal about the line — and
         * a parser that shrugged at it has thrown the counts away. */
        if (!game && spawns_token_count(lines[i]) == doors)
        {
            malformed++;
            malformed_refused_at_parse += !parsed;
            /* And the sentence the author reads must not send them counting
               doors that are already right. */
            blamed_the_count += says_miscount;
        }
        else if (!game)
        {
            miscounted++;
            miscounts_named += says_miscount;
        }
    }

    fflush(stderr);
    if (saved_stderr >= 0)
    {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    if (sink != NULL)
        fclose(sink);

    CHECK(agreed == (int)(sizeof(lines) / sizeof(lines[0])));
    CHECK(malformed_refused_at_parse == malformed);
    CHECK(blamed_the_count == 0);
    CHECK(miscounts_named == miscounted);
    /* And the list really holds all three kinds, or the checks above are
     * counting empty sets. */
    CHECK(accepted == 3);
    CHECK(malformed == 5);
    CHECK(miscounted == 2);
}

/*
 * The editor has nothing to say about the maps that ship.
 *
 * [editor_validate.h](../editor/editor_validate.h) opens by saying it asks the
 * campaign's own questions while a map is being drawn, so the author does not
 * have to build and run the suite to hear the answer. The half nobody had
 * written is the other direction: the editor knows *more* rules than the suite
 * does — a prop that hangs from a slab that is not there, blades that reach a
 * ladder, a crate under them — and none of those were ever asked of the
 * seventeen maps in the box, because the validator only ever ran on maps
 * somebody happened to open.
 *
 * Two were sitting there. Sector 16's wall clock hung under open sky, so
 * `level_load_data` dropped it exactly as documented and the vault's dial simply
 * did not exist. Sector 17 had a ceiling fan two columns from the ladder up to
 * the helicopter pad, which is the last climb in the campaign, so the blades
 * overhanging it cost a heart nobody could avoid on the final approach. Neither
 * is the kind of thing a screenshot shows and neither breaks a rule the suite
 * held, so both survived a green build indefinitely.
 *
 * Warnings count, not just errors. A warning in this tool means "it loads, but it
 * will not play the way it reads", which is the entire class of defect that
 * cannot be found any other way than by asking.
 */
static void test_the_editor_has_nothing_to_say_about_the_shipped_campaign(void)
{
    static EdCampaign campaign;
    static EditorDoc doc;
    static Level level;
    static EdReport report;
    Rng rng;

    /* Every sector recorded first: the cross-sector rules — the rising hazard
     * budget, the repeated storey rhythm, the neighbouring theme — need the whole
     * campaign in hand before any one map can be judged against it. */
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        rng_seed(&rng, 900 + (uint64_t)i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        editor_campaign_record(&campaign, (int)i + 1, &level);
    }

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        rng_seed(&rng, 900 + (uint64_t)i);
        bool parsed = level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                     EMBEDDED_LEVELS[i].data,
                                     EMBEDDED_LEVELS[i].size, &rng);
        CHECK(parsed);
        REQUIRE(editor_doc_parse(&doc, EMBEDDED_LEVELS[i].data,
                                 EMBEDDED_LEVELS[i].size));
        /*
         * The path, and it is load-bearing rather than tidiness.
         *
         * `editor_validate` takes the sector number off `doc->path` — that is
         * how the editor knows which slot of the campaign the map on screen is
         * — and `editor_doc_parse` memsets the whole document, so the path this
         * test handed it was the empty string on every iteration.
         * `editor_path_level_number("")` is nought, so the entire cross-sector
         * block was skipped: the rising hazard budget, the repeated storey
         * rhythm, the neighbouring theme and every per-sector content rule ran
         * over none of the seventeen maps, for as long as this test existed,
         * while the comment above said they did.
         *
         * What that hid was both kinds of thing at once. The editor's own
         * `ED_CAMPAIGN_LENGTH` and `ED_CAMPAIGN_FACADES` had stayed at fifteen
         * and four, so an author opening any shipped sector was told the
         * campaign disagreed with the tests; and sector 14's flash charge sat on
         * top of a partition five rows above the floor, where the route model
         * cannot reach it, which the editor calls out by name.
         *
         * The embedded name *is* the path, so there is nothing to keep in step
         * here — a renamed map brings its own number with it.
         */
        snprintf(doc.path, sizeof(doc.path), "%s", EMBEDDED_LEVELS[i].name);
        editor_validate(&doc, &level, parsed, &campaign, &report);
        /* Notes are deliberately allowed: a note is "worth knowing before it
         * becomes one of the above", which is guidance and not a defect. */
        CHECK(report.errors == 0);
        CHECK(report.warnings == 0);
        /* And the report itself did not overflow, or the two counts above are
         * measuring less than the map actually said. */
        CHECK(report.dropped == 0);
    }

    /* The restrooms are maps too, and they are held to the same tool. They are
     * not campaign sectors, so the cross-sector rules do not apply — an empty
     * campaign is what says so. */
    static EdCampaign no_campaign;
    for (size_t i = 0; i < EMBEDDED_SUBLEVEL_COUNT; ++i)
    {
        rng_seed(&rng, 500 + (uint64_t)i);
        bool parsed = level_load_data(&level, EMBEDDED_SUBLEVELS[i].name,
                                     EMBEDDED_SUBLEVELS[i].data,
                                     EMBEDDED_SUBLEVELS[i].size, &rng);
        CHECK(parsed);
        REQUIRE(editor_doc_parse(&doc, EMBEDDED_SUBLEVELS[i].data,
                                 EMBEDDED_SUBLEVELS[i].size));
        /* Its own name again, which for a restroom is not `levelN.txt` and so
         * resolves to no sector — the empty campaign above says the same thing,
         * and saying it twice is what proves the two agree. */
        snprintf(doc.path, sizeof(doc.path), "%s", EMBEDDED_SUBLEVELS[i].name);
        editor_validate(&doc, &level, parsed, &no_campaign, &report);
        CHECK(report.errors == 0);
        CHECK(report.warnings == 0);
        CHECK(report.dropped == 0);
    }
}

static void test_editor_report_catches_broken_maps(void)
{
    static EdReport report;

    /* A sealed room with the exit behind a wall: it loads, and it cannot be
     * finished. The route model is what knows the difference. */
    validate_text("##########\n"
                  "#S     # E#\n"
                  "##########\n"
                  "\n"
                  "THEME OFFICE\n",
                  NULL, &report);
    CHECK(report.parsed);
    CHECK(report_mentions(&report, ED_SEV_ERROR, "way out cannot be reached"));

    /* Open the wall and the same map is clean. */
    validate_text("##########\n"
                  "#S       E#\n"
                  "##########\n"
                  "\n"
                  "THEME OFFICE\n",
                  NULL, &report);
    CHECK(report.errors == 0);
    CHECK(report.route_valid);
    CHECK(report.goal_reached);

    /* A character nobody has heard of is air in the game and silence in the
     * diff, so it has to be loud here. */
    validate_text("##########\n"
                  "#S  @    E#\n"
                  "##########\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_ERROR, "not in the legend"));

    /* Two exits, which the loader itself rejects. */
    validate_text("###########\n"
                  "#S   E   E#\n"
                  "###########\n",
                  NULL, &report);
    CHECK(!report.parsed);
    CHECK(report_mentions(&report, ED_SEV_ERROR, "exactly one exit"));

    /* A fan two columns from a hole the route model calls crossed. */
    validate_text("############\n"
                  "#          #\n"
                  "#S   O    E#\n"
                  "#### #######\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_WARN, "hole in the floor"));

    /* SPAWNS has to name every door. */
    validate_text("############\n"
                  "#S D    D E#\n"
                  "############\n"
                  "\n"
                  "SPAWNS 1\n"
                  "THEME LAB\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_ERROR, "SPAWNS lists"));

    /* A prop with nothing under it is dropped by the loader. */
    validate_text("############\n"
                  "#S   d    E#\n"
                  "#          #\n"
                  "############\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_WARN, "no wall under it"));

    /* A weak wall nothing in the sector can open is scenery. */
    validate_text("############\n"
                  "#S   %    E#\n"
                  "############\n"
                  "\n"
                  "THEME LAB\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_WARN, "nothing to open them with"));

    /* Give it a grenade and the warning goes; the route model still refuses to
     * walk through the patch, which is what keeps it a shortcut. */
    validate_text("#############\n"
                  "#S N %    E #\n"
                  "#############\n"
                  "\n"
                  "THEME LAB\n",
                  NULL, &report);
    CHECK(!report_mentions(&report, ED_SEV_WARN, "nothing to open them with"));
    CHECK(report_mentions(&report, ED_SEV_ERROR, "way out cannot be reached"));

    /* On a wall there is nothing to set a blast off with, so a patch there
     * never opens. */
    validate_text("....Y....\n"
                  ".........\n"
                  "..#####..\n"
                  "....%....\n"
                  ".........\n"
                  "....S....\n"
                  "\n"
                  "MODE FACADE\n"
                  "THEME FACADE_STORM\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_WARN, "never opens"));

    /* On a wall, a lone block seals the band it sits in. */
    validate_text("....Y....\n"
                  ".........\n"
                  "..##.##..\n"
                  ".........\n"
                  ".....#...\n"
                  ".........\n"
                  "....S....\n"
                  "\n"
                  "MODE FACADE\n"
                  "THEME FACADE_HIGH\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_WARN, "lone block"));

    /* And a climb whose masonry closes the wall off has no route up. */
    validate_text("....Y....\n"
                  ".........\n"
                  ".#######.\n"
                  ".........\n"
                  "....S....\n"
                  "\n"
                  "MODE FACADE\n"
                  "THEME FACADE_MOON\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_ERROR, "cannot be climbed to"));
}

/*
 * The two ceilings the cap list was missing, and why they are worth their own
 * test rather than a line in the one above.
 *
 * `check_caps` covers seventeen limits and covered neither dogs nor lift
 * shafts, both of which the loader drops in silence — `find_dog_slot` hands
 * back nothing past `MAX_DOGS`, and `level_load_data`'s shaft scan stops at
 * `MAX_ELEVATORS`. A map over either limit therefore played as a map somebody
 * had quietly edited, which is exactly the failure the rest of that function
 * exists to make loud. The shaft count is the awkward one: a shaft is a run of
 * `V` rather than a tile, so counting the character would fail a perfectly
 * legal map with one tall lift in it, and that wrong answer is checked here
 * too.
 */
static void test_editor_report_counts_dogs_and_lift_shafts(void)
{
    static EdReport report;
    char map[8192];

    /* One `W` over the limit. Each is a guard and his dog, so this trips the
     * guard ceiling as well — MAX_DOGS is the lower of the two, which is the
     * whole reason a `W`-heavy map needs its own line. */
    {
        int at = 0;
        at += snprintf(map + at, sizeof(map) - (size_t)at, "#");
        for (int i = 0; i < MAX_DOGS + 1; ++i)
            at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
        at += snprintf(map + at, sizeof(map) - (size_t)at, "###\n#S");
        for (int i = 0; i < MAX_DOGS + 1; ++i)
            at += snprintf(map + at, sizeof(map) - (size_t)at, " W");
        at += snprintf(map + at, sizeof(map) - (size_t)at, "  E#\n#");
        for (int i = 0; i < MAX_DOGS + 1; ++i)
            at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
        snprintf(map + at, sizeof(map) - (size_t)at, "###\n");
        validate_text(map, NULL, &report);
        CHECK(report_mentions(&report, ED_SEV_ERROR, "guard dogs"));
    }

    /* One tall shaft is one lift, however many tiles it is written with. A
     * count by character would call this fifteen and refuse it. */
    {
        int at = 0;
        at += snprintf(map + at, sizeof(map) - (size_t)at, "#######\n");
        for (int i = 0; i < MAX_ELEVATORS + 2; ++i)
            at += snprintf(map + at, sizeof(map) - (size_t)at, "#  V  #\n");
        at += snprintf(map + at, sizeof(map) - (size_t)at, "#S   E#\n");
        snprintf(map + at, sizeof(map) - (size_t)at, "#######\n");
        validate_text(map, NULL, &report);
        CHECK(!report_mentions(&report, ED_SEV_ERROR, "lift shafts"));
    }

    /* And one shaft per column, each two tiles tall, one column over the
     * limit. That is what the ceiling is actually about. */
    {
        int at = 0;
        int columns = MAX_ELEVATORS + 1;
        at += snprintf(map + at, sizeof(map) - (size_t)at, "#");
        for (int i = 0; i < columns; ++i)
            at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
        at += snprintf(map + at, sizeof(map) - (size_t)at, "###\n");
        for (int row = 0; row < 2; ++row)
        {
            at += snprintf(map + at, sizeof(map) - (size_t)at, "#");
            for (int i = 0; i < columns; ++i)
                at += snprintf(map + at, sizeof(map) - (size_t)at, " V");
            at += snprintf(map + at, sizeof(map) - (size_t)at, "   #\n");
        }
        at += snprintf(map + at, sizeof(map) - (size_t)at, "#S");
        for (int i = 0; i < columns; ++i)
            at += snprintf(map + at, sizeof(map) - (size_t)at, "  ");
        at += snprintf(map + at, sizeof(map) - (size_t)at, "  E#\n#");
        for (int i = 0; i < columns; ++i)
            at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
        snprintf(map + at, sizeof(map) - (size_t)at, "###\n");
        validate_text(map, NULL, &report);
        CHECK(report_mentions(&report, ED_SEV_ERROR, "lift shafts"));
    }
}

/*
 * Every ceiling the loader drops at, found by walking the palette rather than
 * by naming the ones somebody remembered.
 *
 * The test above pins two caps that had been missing, one at a time, which is
 * how this list has been maintained and is also why it kept going wrong: a cap
 * line names a *character*, and the characters are added somewhere else. `Q`,
 * `I`, `*` and `!` all arrived in one commit that taught the parser and the
 * editor's palette about them and never touched `check_caps`, so a heavy went
 * uncounted against `MAX_ENEMIES`, `MAX_CAMERAS` — the tightest ceiling on the
 * list — had no line at all, and the docket sheet and the flash charge were
 * absent from the item sum. Sector 17 ships fifteen guards against a ceiling
 * of sixteen, so the margin on the one that mattered was a single man.
 *
 * So this asks the question from outside instead, and asks it of every symbol
 * the editor can paint: flood a map with one character, flood it again with
 * more of the same, and if the second map holds no more than the first then
 * the loader is saturated — that is what a cap looks like from the outside,
 * whichever array it belongs to. A saturated map that the report says nothing
 * about is a map the author was let edit past the ceiling in silence, which is
 * the whole failure `check_one_cap`'s own message describes. A new character
 * is covered the day it enters the palette, because the palette is what this
 * walks.
 *
 * A character that places nothing — a wall, a rung, air, the restroom door
 * that is a flag rather than an array — is skipped, and so is one that cannot
 * be flooded into a map that loads at all: two starts or two windows never
 * parse. Those are the five lines the suite prints about a flood of `S`, `E`,
 * `Y`, `U` or `R` — the loader explaining a refusal this test asked for, not a
 * failure. A lift is the one ceiling this shape cannot reach, because a shaft
 * is a *run* of `V` and flooding rows of them makes none, which is why the
 * test above still owns that case.
 */
static int level_total_placed(const Level *level)
{
    return level->runtime.item_count + level->map.enemy_count +
           level->map.mine_count + level->map.spike_count +
           level->map.ceiling_fan_count + level->runtime.crate_count +
           level->runtime.gas_canister_count + level->map.terminal_count +
           level->map.alarm_switch_count + level->map.camera_count +
           level->map.decoration_count + level->map.janitor_count +
           level->map.civilian_count + level->map.receptionist_count +
           level->map.facade_hazard_spawn_count +
           level->runtime.fall_platform_count +
           level->runtime.moving_platform_count + level->map.door_count +
           level->runtime.elevator_count;
}

/*
 * `want` copies of `ch`, laid on open rows with a solid row above and below
 * every one of them. Both walls matter: a prop that hangs from the tile above
 * and a prop that stands on the tile below are then equally supported, so
 * nothing is dropped by the loader's own support pass and a saturated count
 * can only mean a ceiling.
 */
static void build_flood_map(char *out, size_t cap, char ch, int want)
{
    const int width = 60;
    const int open_rows = 6;
    int at = 0;
    int painted = 0;
    for (int r = 0; r < open_rows; ++r)
    {
        for (int c = 0; c < width; ++c)
            at += snprintf(out + at, cap - (size_t)at, "#");
        at += snprintf(out + at, cap - (size_t)at, "\n#");
        for (int c = 1; c < width - 1; ++c)
        {
            char put = ' ';
            if (r == open_rows - 1 && c == 1)
                put = 'S';
            else if (r == open_rows - 1 && c == 2)
                put = 'E';
            else if (painted < want)
            {
                put = ch;
                ++painted;
            }
            at += snprintf(out + at, cap - (size_t)at, "%c", put);
        }
        at += snprintf(out + at, cap - (size_t)at, "#\n");
    }
    for (int c = 0; c < width; ++c)
        at += snprintf(out + at, cap - (size_t)at, "#");
    snprintf(out + at, cap - (size_t)at, "\n");
}

static void test_the_editor_reports_every_ceiling_the_loader_drops_at(void)
{
    static char map[16384];
    static EditorDoc doc;
    static Level lean;
    static Level full;
    static EdCampaign no_campaign;
    static EdReport report;
    int ceilings_found = 0;

    for (int i = 0; i < ED_SYMBOL_COUNT; ++i)
    {
        char ch = ED_SYMBOLS[i].symbol;

        build_flood_map(map, sizeof(map), ch, 260);
        CHECK(editor_doc_parse(&doc, map, strlen(map)));
        if (!editor_doc_build_level(&doc, &lean, 11))
            continue;
        int kept_lean = level_total_placed(&lean);

        build_flood_map(map, sizeof(map), ch, 320);
        CHECK(editor_doc_parse(&doc, map, strlen(map)));
        if (!editor_doc_build_level(&doc, &full, 11))
            continue;
        int kept_full = level_total_placed(&full);

        if (kept_lean == 0 || kept_full != kept_lean)
            continue;

        editor_validate(&doc, &full, true, &no_campaign, &report);
        CHECK(report_mentions(&report, ED_SEV_ERROR, "silently drops"));
        ++ceilings_found;
    }

    /* And the walk has to have actually reached some: a build where every
     * flood parsed away to nothing would pass the loop above without asking
     * the report a single question. */
    CHECK(ceilings_found >= 15);
}

/*
 * A floor that can send for more men than it can seat, said out loud while it
 * is being drawn.
 *
 * `test_every_sector_can_seat_the_reinforcements_it_can_call` holds the shipped
 * maps to this, but the suite is the wrong place to find it out: the author is
 * in the editor, and the sum they have to keep is not one they can read off the
 * grid — it is the guards drawn plus two per console, and only if the map has a
 * door. It is a warning rather than an error because nothing fails; the floor
 * simply deletes a corpse to make room, which is the one thing the quiet route
 * cannot afford and the one thing no error message would ever have said.
 */
/*
 * A duct is horizontal, so the floor under a run is part of the run.
 *
 * This is the one duct mistake no other rule in the tree can see. The tile
 * parses, draws as trunking, passes every reachability question the route model
 * asks — and drops the player out of the bottom of it the moment they crawl in,
 * because what holds a crawler up inside a duct is whatever the map put
 * underneath. A warning rather than a note: it loads, and it will not play the
 * way it reads.
 */
static void test_the_editor_wants_a_floor_under_a_duct(void)
{
    static EdReport report;

    /* Trunking over open air. */
    static const char floating[] =
        "###########\n"
        "#S       E#\n"
        "#   ===   #\n"
        "#         #\n"
        "###########\n";
    validate_text(floating, NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_WARN, "no floor under it"));

    /* The same run with its own slab under it says nothing. */
    static const char floored[] =
        "###########\n"
        "#S  ===  E#\n"
        "###########\n";
    validate_text(floored, NULL, &report);
    CHECK(!report_mentions(&report, ED_SEV_WARN, "no floor under it"));

    /* And trunking sealed by masonry on both sides is a picture of a duct
     * rather than a route through one. */
    static const char sealed[] =
        "###########\n"
        "#S      E #\n"
        "######=####\n"
        "###########\n";
    validate_text(sealed, NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_NOTE, "crawl into it"));
}

static void test_the_editor_warns_when_a_floor_cannot_seat_its_reinforcements(void)
{
    static EdReport report;
    char map[8192];

    /* Twenty men drawn — under the ceiling on their own — three consoles and a
     * pair of doors for the arrivals to come out of. Twenty plus six is over. */
    const int drawn = 20;
    const int consoles = 3;
    CHECK(drawn <= MAX_ENEMIES);
    CHECK(drawn + consoles * TERMINAL_REINFORCEMENT_MAX_COUNT > MAX_ENEMIES);

    int at = 0;
    at += snprintf(map + at, sizeof(map) - (size_t)at, "#");
    for (int i = 0; i < drawn + consoles + 4; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
    at += snprintf(map + at, sizeof(map) - (size_t)at, "#\n#S");
    for (int i = 0; i < drawn; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, " M");
    for (int i = 0; i < consoles; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, " T");
    at += snprintf(map + at, sizeof(map) - (size_t)at, " D D  E#\n#");
    for (int i = 0; i < drawn + consoles + 4; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
    snprintf(map + at, sizeof(map) - (size_t)at, "#\n");

    validate_text(map, NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_WARN, "overwrites a body"));
    /* And it is not reported as a hard drop, because nothing is dropped. */
    CHECK(!report_mentions(&report, ED_SEV_ERROR, "silently drops"));

    /* Take the doors away and there is nowhere for a reinforcement to arrive
     * from, so the same twenty men and three consoles are fine. */
    at = 0;
    at += snprintf(map + at, sizeof(map) - (size_t)at, "#");
    for (int i = 0; i < drawn + consoles + 4; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
    at += snprintf(map + at, sizeof(map) - (size_t)at, "#\n#S");
    for (int i = 0; i < drawn; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, " M");
    for (int i = 0; i < consoles; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, " T");
    at += snprintf(map + at, sizeof(map) - (size_t)at, "      E#\n#");
    for (int i = 0; i < drawn + consoles + 4; ++i)
        at += snprintf(map + at, sizeof(map) - (size_t)at, "##");
    snprintf(map + at, sizeof(map) - (size_t)at, "#\n");

    validate_text(map, NULL, &report);
    CHECK(!report_mentions(&report, ED_SEV_WARN, "overwrites a body"));
}

static void test_editor_report_reads_the_campaign(void)
{
    static EdCampaign campaign;
    static EditorDoc doc;
    static Level level;
    static EdReport report;
    static Level neighbour;

    /* Build the campaign context out of the shipped sectors, then hand the
     * editor a sector 2 that repeats sector 1's theme. */
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Rng rng;
        rng_seed(&rng, 4000 + i);
        CHECK(level_load_data(&neighbour, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data, EMBEDDED_LEVELS[i].size,
                              &rng));
        editor_campaign_record(&campaign, (int)i + 1, &neighbour);
    }
    CHECK(campaign.count == (int)EMBEDDED_LEVEL_COUNT);

    /* Sector 1 is the lobby, so a second LOBBY next door is a repeat, and a
     * tiny map is both under-budget and the wrong shape. */
    const char *repeat = "##########\n"
                         "#S      E#\n"
                         "##########\n"
                         "\n"
                         "THEME LOBBY\n";
    CHECK(editor_doc_parse(&doc, repeat, strlen(repeat)));
    snprintf(doc.path, sizeof(doc.path), "levels/level2.txt");
    CHECK(editor_doc_build_level(&doc, &level, 3));
    editor_validate(&doc, &level, true, &campaign, &report);
    CHECK(report_mentions(&report, ED_SEV_ERROR, "already wears LOBBY"));
    CHECK(report_mentions(&report, ED_SEV_ERROR, "does not beat the previous"));
    CHECK(report_mentions(&report, ED_SEV_ERROR, "alarm switches"));
    CHECK(report_mentions(&report, ED_SEV_ERROR, "bazookas"));

    /* The shipped sector 2 itself has nothing to answer for. */
    CHECK(editor_doc_parse(&doc, EMBEDDED_LEVELS[1].data,
                           EMBEDDED_LEVELS[1].size));
    snprintf(doc.path, sizeof(doc.path), "levels/level2.txt");
    CHECK(editor_doc_build_level(&doc, &level, 3));
    editor_validate(&doc, &level, true, &campaign, &report);
    CHECK(report.errors == 0);

    /* Every shipped sector, in fact: the editor and `make test` have to agree
     * about the campaign that is already in the tree. */
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        char path[64];
        snprintf(path, sizeof(path), "levels/level%d.txt", (int)i + 1);
        CHECK(editor_doc_parse(&doc, EMBEDDED_LEVELS[i].data,
                               EMBEDDED_LEVELS[i].size));
        snprintf(doc.path, sizeof(doc.path), "%s", path);
        CHECK(editor_doc_build_level(&doc, &level, 3));
        editor_validate(&doc, &level, true, &campaign, &report);
        if (report.errors != 0)
            printf("%s: %s\n", path, report.findings[0].text);
        CHECK(report.errors == 0);
    }

    /* A restroom is not a sector, so none of the campaign rules apply to it and
     * it still has to be finishable — and all four of them are shipped maps
     * somebody will open in the editor, so all four have to come back clean.
     * This ran on `EMBEDDED_SUBLEVELS[0]` alone while there was only one room
     * to run on; a new room the editor calls broken and the game ships anyway
     * is the disagreement between the two that this whole test exists to
     * prevent. */
    for (size_t i = 0; i < EMBEDDED_SUBLEVEL_COUNT; ++i)
    {
        CHECK(editor_doc_parse(&doc, EMBEDDED_SUBLEVELS[i].data,
                               EMBEDDED_SUBLEVELS[i].size));
        snprintf(doc.path, sizeof(doc.path), "%s", EMBEDDED_SUBLEVELS[i].name);
        CHECK(editor_doc_build_level(&doc, &level, 3));
        editor_validate(&doc, &level, true, &campaign, &report);
        if (report.errors > 0)
            printf("%s: %s\n", EMBEDDED_SUBLEVELS[i].name,
                   report.findings[0].text);
        CHECK(report.route_valid);
        CHECK(report.goal_reached);
        CHECK(report.errors == 0);
    }
}

/* ---- Prologue car chase ---------------------------------------------- */

/* The drive is stepped by `game_update` like everything else on the frame, so
 * it gets the same slice. Named because `chase_run` divides a duration by it. */
#define CHASE_STEP SIM_STEP_DT

static ChaseOutcome chase_step(Chase *chase, const Input *input)
{
    game_events_clear(&chase->events);
    return chase_update(chase, input, CHASE_STEP);
}

static void chase_run(Chase *chase, const Input *input, float seconds)
{
    int frames = (int)(seconds / CHASE_STEP);
    for (int i = 0; i < frames; ++i)
        chase_step(chase, input);
}

/* Jumps straight to the drive, the way pressing Space in the opening does. */
static void chase_skip_departure(Chase *chase)
{
    Input input = {0};
    input.confirm = true;
    chase_step(chase, &input);
}

static void chase_clear_traffic(Chase *chase)
{
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
        chase->cars[i].active = false;
}

static ChaseCar *chase_place_car_ahead(Chase *chase, int slot)
{
    ChaseCar *car = &chase->cars[slot];
    memset(car, 0, sizeof(*car));
    car->active = true;
    car->kind = CHASE_CAR_TRAFFIC;
    car->x = chase->player.x;
    car->y = chase->player.y + CHASE_CAR_LENGTH * 0.6f;
    return car;
}

static void test_chase_is_reproducible_from_a_seed(void)
{
    Chase first;
    Chase second;
    Chase other_seed;
    chase_init(&first, 20260725u);
    chase_init(&second, 20260725u);
    chase_init(&other_seed, 20260726u);

    Input input = {0};
    input.gas = true;
    input.left = true;
    for (int frame = 0; frame < 900; ++frame)
    {
        /* Same seed, same inputs: the whole drive has to match frame for frame. */
        input.left = (frame / 40) % 2 == 0;
        input.right = !input.left;
        chase_step(&first, &input);
        chase_step(&second, &input);
        chase_step(&other_seed, &input);
    }

    CHECK(first.phase == second.phase);
    CHECK(first.player.x == second.player.x);
    CHECK(first.player.y == second.player.y);
    CHECK(first.player.integrity == second.player.integrity);
    CHECK(first.target.y == second.target.y);
    CHECK(first.rng.state == second.rng.state);
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        CHECK(first.cars[i].active == second.cars[i].active);
        CHECK(first.cars[i].x == second.cars[i].x);
        CHECK(first.cars[i].y == second.cars[i].y);
    }
    CHECK(other_seed.rng.state != first.rng.state);
}

static void test_chase_departure_hands_over_to_the_drive(void)
{
    Chase chase;
    chase_init(&chase, 4242);
    CHECK(chase.phase == CHASE_PHASE_DEPARTURE);
    CHECK(!chase.player.engine_running);
    CHECK(chase.player.speed == 0.0f);
    CHECK(chase_gap(&chase) > 0.0f);

    Input input = {0};
    bool heard_door = false;
    bool heard_engine = false;
    int frames = (int)(CHASE_DEPARTURE_DURATION / CHASE_STEP) + 2;
    for (int i = 0; i < frames; ++i)
    {
        chase_step(&chase, &input);
        if (events_have_sound(&chase.events, GAME_EVENT_SOUND,
                              SFX_OPENING_CAR_DOOR))
            heard_door = true;
        if (events_have_sound(&chase.events, GAME_EVENT_SOUND, SFX_CHASE_ENGINE))
            heard_engine = true;
    }

    CHECK(heard_door);
    CHECK(heard_engine);
    CHECK(chase.phase == CHASE_PHASE_PURSUIT);
    CHECK(chase.player.engine_running);
    CHECK(chase.player.integrity == CHASE_INTEGRITY);
    /* The SUV's head start is pulled back to one fixed opening gap. */
    CHECK(fabsf(chase_gap(&chase) - CHASE_START_GAP) < 12.0f);

    /* Skipping the beat reaches the same phase without waiting it out. */
    Chase skipped;
    chase_init(&skipped, 4242);
    chase_skip_departure(&skipped);
    CHECK(skipped.phase == CHASE_PHASE_PURSUIT);
    CHECK(skipped.player.engine_running);
}

static void test_chase_collision_costs_integrity_and_speed(void)
{
    Chase chase;
    chase_init(&chase, 77);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);
    chase.player.invuln_timer = 0.0f;

    ChaseCar *car = chase_place_car_ahead(&chase, 0);
    Input input = {0};
    chase_step(&chase, &input);

    CHECK(chase.player.integrity == CHASE_INTEGRITY - 1);
    CHECK(chase.player.speed == CHASE_CRASH_SPEED);
    CHECK(chase.player.invuln_timer > 0.0f);
    CHECK(car->wreck_time > 0.0f);
    CHECK(events_have_sound(&chase.events, GAME_EVENT_SOUND, SFX_CHASE_CRASH));
    bool shook = false;
    for (int i = 0; i < chase.events.count; ++i)
    {
        if (chase.events.items[i].type == GAME_EVENT_CAMERA_SHAKE)
            shook = true;
    }
    CHECK(shook);

    /* The crash cooldown stops one pile-up from emptying the whole car. */
    chase_place_car_ahead(&chase, 1);
    chase_step(&chase, &input);
    CHECK(chase.player.integrity == CHASE_INTEGRITY - 1);
}

static void test_chase_kerb_scrape_bleeds_speed_without_damage(void)
{
    Chase chase;
    chase_init(&chase, 31);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);

    Input input = {0};
    input.right = true;
    bool heard_tyres = false;
    for (int i = 0; i < 90; ++i)
    {
        chase_clear_traffic(&chase);
        chase_step(&chase, &input);
        if (events_have_sound(&chase.events, GAME_EVENT_SOUND, SFX_CHASE_TIRES))
            heard_tyres = true;
    }

    CHECK(heard_tyres);
    CHECK(chase.player.integrity == CHASE_INTEGRITY);
    CHECK(chase.player.x <=
          CHASE_ROAD_WIDTH - CHASE_CAR_WIDTH * 0.5f - CHASE_KERB_MARGIN + 0.01f);
    /* Grinding the kerb costs speed, which is what opens the gap. */
    CHECK(chase.player.speed < CHASE_CRUISE_SPEED);
}

/*
 * The car is driven with two pedals of its own. The shell folds the letter
 * under each thumb into them — A into `gas`, B into `brake` — alongside the
 * stick and the arrows, and the drive reads nothing else: a raw direction
 * arriving here must not move the car, because the day a face button also had
 * to mean "up" is the day the accelerator started climbing ladders.
 */
static void test_chase_pedals_drive_the_car(void)
{
    Chase chase;
    chase_init(&chase, 5150);
    chase_skip_departure(&chase);

    float cruise = chase.player.speed;
    Input input = {0};
    input.gas = true;
    for (int i = 0; i < 30; ++i)
    {
        chase_clear_traffic(&chase);
        chase_step(&chase, &input);
    }
    CHECK(chase.player.speed > cruise);

    float fast = chase.player.speed;
    input.gas = false;
    input.brake = true;
    for (int i = 0; i < 30; ++i)
    {
        chase_clear_traffic(&chase);
        chase_step(&chase, &input);
    }
    CHECK(chase.player.speed < fast);

    /* Up and down on their own are not pedals: with neither held the car only
     * ever coasts back toward the cruise speed. */
    Input directions = {0};
    directions.up = true;
    for (int i = 0; i < 30; ++i)
    {
        chase_clear_traffic(&chase);
        chase_step(&chase, &directions);
    }
    CHECK(chase.player.speed <= CHASE_CRUISE_SPEED + 0.01f);
}

/*
 * Which leaves the skip needing a button of its own on a pad, since A is now
 * the throttle: it arrives as `use_door`, the letter Y, and has to work both
 * where confirm used to — the departure, and the drive itself once it has been
 * failed often enough to stop insisting.
 */
static void test_chase_skip_answers_the_pad_letter(void)
{
    Chase chase;
    chase_init(&chase, 4242);

    Input input = {0};
    input.use_door = true;
    chase_step(&chase, &input);
    CHECK(chase.phase == CHASE_PHASE_PURSUIT);
    CHECK(chase.player.engine_running);

    chase.attempts = CHASE_SKIP_AFTER_ATTEMPTS;
    chase_step(&chase, &input);
    CHECK(chase.phase == CHASE_PHASE_ARRIVAL);
}

static void test_chase_holding_the_throttle_never_catches_the_suv(void)
{
    Chase chase;
    chase_init(&chase, 606);
    chase_skip_departure(&chase);

    Input input = {0};
    input.gas = true;
    float smallest_gap = chase_gap(&chase);
    for (int i = 0; i < 900; ++i)
    {
        chase_clear_traffic(&chase);
        chase_step(&chase, &input);
        float gap = chase_gap(&chase);
        if (gap < smallest_gap)
            smallest_gap = gap;
    }

    CHECK(chase.phase == CHASE_PHASE_PURSUIT);
    CHECK(smallest_gap > CHASE_MIN_GAP - 30.0f);
    CHECK(chase_gap(&chase) < CHASE_LOSE_GAP);
    /* No traffic to hit and no way to ram them: the car stays intact. */
    CHECK(chase.player.integrity == CHASE_INTEGRITY);
}

static void test_chase_lost_trail_restarts_the_pursuit(void)
{
    Chase chase;
    chase_init(&chase, 8181);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);

    Input input = {0};
    chase.target.y = chase.player.y + CHASE_LOSE_GAP + 20.0f;
    chase_step(&chase, &input);

    CHECK(chase.phase == CHASE_PHASE_FAILED);
    CHECK(chase.failure == CHASE_FAILURE_LOST);
    CHECK(chase.attempts == 0);

    chase_run(&chase, &input, CHASE_FAILED_DURATION + 0.1f);
    CHECK(chase.phase == CHASE_PHASE_PURSUIT);
    CHECK(chase.attempts == 1);
    CHECK(chase.player.integrity == CHASE_INTEGRITY);
    CHECK(chase.pursuit_time < 0.3f);
    CHECK(fabsf(chase_gap(&chase) - CHASE_START_GAP) < 60.0f);
}

static void test_chase_wreck_restarts_the_pursuit(void)
{
    Chase chase;
    chase_init(&chase, 9191);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);
    chase.player.invuln_timer = 0.0f;
    chase.player.integrity = 1;

    chase_place_car_ahead(&chase, 0);
    Input input = {0};
    chase_step(&chase, &input);

    CHECK(chase.phase == CHASE_PHASE_FAILED);
    CHECK(chase.failure == CHASE_FAILURE_WRECKED);
    CHECK(events_have_sound(&chase.events, GAME_EVENT_SOUND, SFX_EXPLOSION));

    chase_run(&chase, &input, CHASE_FAILED_DURATION + 0.1f);
    CHECK(chase.phase == CHASE_PHASE_PURSUIT);
    CHECK(chase.player.integrity == CHASE_INTEGRITY);
}

/*
 * The rewind gives road back, and giving road back forever is a drive that
 * never ends.
 *
 * A player who crashes more often than every `CHASE_FAIL_REWIND` seconds hands
 * back more of the pursuit than they make, so the clock never reaches
 * `CHASE_PURSUIT_DURATION`: measured before this, a pad held on the throttle
 * with no steering never arrived in three minutes of driving, while one that
 * did nothing at all always did. The rewind therefore stops at the same
 * attempt the skip prompt appears on — from there progress is monotonic, and
 * the prologue ends whether or not anybody takes the skip it is offering.
 */
static void test_chase_stops_giving_road_back_once_it_offers_the_skip(void)
{
    Chase chase;
    chase_init(&chase, 606);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);

    Input input = {0};

    /* Early on the failure still costs a stretch of the drive. */
    chase.attempts = CHASE_SKIP_AFTER_ATTEMPTS - 2;
    chase.pursuit_time = CHASE_FAIL_REWIND + 8.0f;
    chase.target.y = chase.player.y + CHASE_LOSE_GAP + 20.0f;
    chase_step(&chase, &input);
    CHECK(chase.phase == CHASE_PHASE_FAILED);
    chase_run(&chase, &input, CHASE_FAILED_DURATION + 0.1f);
    CHECK(chase.attempts == CHASE_SKIP_AFTER_ATTEMPTS - 1);
    CHECK(chase.pursuit_time < 8.5f);

    /* On the attempt that earns the skip prompt it stops costing anything. */
    chase_clear_traffic(&chase);
    chase.pursuit_time = CHASE_FAIL_REWIND + 8.0f;
    chase.target.y = chase.player.y + CHASE_LOSE_GAP + 20.0f;
    chase_step(&chase, &input);
    CHECK(chase.phase == CHASE_PHASE_FAILED);
    chase_run(&chase, &input, CHASE_FAILED_DURATION + 0.1f);
    CHECK(chase.attempts == CHASE_SKIP_AFTER_ATTEMPTS);
    CHECK(chase.pursuit_time >= CHASE_FAIL_REWIND + 8.0f);
}

/*
 * And the property that follows from it: the worst player in the world still
 * gets out of the prologue. Holding the accelerator into whatever is in front
 * of you is the most naive thing a first-time player can do with a car, and it
 * used to be the input that never finished the drive.
 */
static void test_chase_always_ends_even_for_a_player_who_only_accelerates(void)
{
    const unsigned seeds[] = {606u, 4242u, 8181u};
    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); ++s)
    {
        Chase chase;
        chase_init(&chase, seeds[s]);
        chase_skip_departure(&chase);

        Input input = {0};
        input.gas = true; /* no steering, and never the skip */
        ChaseOutcome outcome = CHASE_RUNNING;
        int frames = (int)(240.0f / CHASE_STEP);
        for (int i = 0; i < frames && outcome != CHASE_REACHED_BUILDING; ++i)
            outcome = chase_step(&chase, &input);

        CHECK(outcome == CHASE_REACHED_BUILDING);
        CHECK(chase.phase == CHASE_PHASE_DONE);
    }
}

static void test_chase_surviving_the_drive_parks_at_the_building(void)
{
    Chase chase;
    chase_init(&chase, 5150);
    chase_skip_departure(&chase);

    Input input = {0};
    chase.pursuit_time = CHASE_PURSUIT_DURATION;
    CHECK(chase_step(&chase, &input) == CHASE_RUNNING);
    CHECK(chase.phase == CHASE_PHASE_ARRIVAL);
    CHECK(chase.building_y > chase.player.y);
    CHECK(chase_route_progress(&chase) == 1.0f);
    /* Nothing is left driving in the forecourt the cars are pulling into. */
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
        CHECK(!(chase.cars[i].active && chase.cars[i].y > chase.target.y));

    ChaseOutcome outcome = CHASE_RUNNING;
    int frames = (int)((CHASE_ARRIVAL_DURATION + 0.2f) / CHASE_STEP);
    for (int i = 0; i < frames; ++i)
        outcome = chase_step(&chase, &input);

    CHECK(outcome == CHASE_REACHED_BUILDING);
    CHECK(chase.phase == CHASE_PHASE_DONE);
    CHECK(chase.player.speed == 0.0f);
    CHECK(chase.target.speed == 0.0f);
    /* Both cars stop short of the entrance, the SUV closest to it. */
    CHECK(chase.target.y > chase.player.y);
    CHECK(chase.target.y < chase.building_y);
    CHECK(chase.player.y < chase.building_y);
    /* Reporting the outcome again is safe: the shell polls it once per frame. */
    CHECK(chase_step(&chase, &input) == CHASE_REACHED_BUILDING);
}

static void test_chase_cross_traffic_obeys_the_signal(void)
{
    Chase chase;
    chase_init(&chase, 1234);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);
    for (int i = 1; i < CHASE_MAX_INTERSECTIONS; ++i)
        chase.intersections[i].active = false;

    ChaseIntersection *junction = &chase.intersections[0];
    junction->active = true;
    junction->y = chase.player.y + 420.0f;
    junction->signal_offset = 0.0f;
    junction->cross_spawn_timer = 0.05f;

    /* Red for the cross street: nothing may pull into the junction. */
    chase.time = CHASE_SIGNAL_CROSS_GREEN + 0.4f;
    CHECK(!chase_cross_has_green(junction, chase.time));
    Input input = {0};
    for (int i = 0; i < 30; ++i)
    {
        chase_step(&chase, &input);
        for (int c = 0; c < CHASE_MAX_CARS; ++c)
            CHECK(!(chase.cars[c].active &&
                    chase.cars[c].kind == CHASE_CAR_CROSSING));
    }

    /* Green: cars enter from the kerbs and drive straight across. */
    chase.time = 0.05f;
    junction->cross_spawn_timer = 0.02f;
    CHECK(chase_cross_has_green(junction, chase.time));
    bool crossing_seen = false;
    for (int i = 0; i < 30 && !crossing_seen; ++i)
    {
        chase_step(&chase, &input);
        for (int c = 0; c < CHASE_MAX_CARS; ++c)
        {
            const ChaseCar *car = &chase.cars[c];
            if (!car->active || car->kind != CHASE_CAR_CROSSING)
                continue;
            crossing_seen = true;
            CHECK(car->vy == 0.0f);
            CHECK(fabsf(car->vx) >= CHASE_CROSS_SPEED_MIN);
            CHECK(fabsf(car->y - junction->y) <= CHASE_CROSS_LANE_OFFSET + 1.0f);
            /* Right-hand rule on the cross street too. */
            CHECK((car->vx > 0.0f) == (car->y < junction->y));
        }
    }
    CHECK(crossing_seen);
}

static void test_chase_generated_traffic_matches_its_lane(void)
{
    Chase chase;
    chase_init(&chase, 606060);
    chase_skip_departure(&chase);

    Input input = {0};
    int junctions = 0;
    int lane_cars = 0;
    int oncoming_cars = 0;
    for (int frame = 0; frame < 600; ++frame)
    {
        chase_step(&chase, &input);
        for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
        {
            if (chase.intersections[i].active)
                junctions++;
        }
        for (int i = 0; i < CHASE_MAX_CARS; ++i)
        {
            const ChaseCar *car = &chase.cars[i];
            if (!car->active || car->wreck_time > 0.0f)
                continue;
            if (car->kind == CHASE_CAR_TRAFFIC)
            {
                lane_cars++;
                /* Runs with the pursuit, on the pursuit's side, and slower. */
                CHECK(car->vy > 0.0f);
                CHECK(car->vy <= CHASE_TRAFFIC_SPEED_MAX);
                CHECK(car->x > CHASE_ROAD_WIDTH * 0.5f);
            }
            else if (car->kind == CHASE_CAR_ONCOMING)
            {
                oncoming_cars++;
                CHECK(car->vy < 0.0f);
                CHECK(car->x < CHASE_ROAD_WIDTH * 0.5f);
            }
        }
    }

    CHECK(junctions > 0);
    CHECK(lane_cars > 0);
    CHECK(oncoming_cars > 0);
}

static void test_gameplay_reset_preserves_rng_only(void)
{
    GameplayState state = {0};
    rng_seed(&state.rng, 4567);
    Rng expected = state.rng;
    state.enemy_count = 3;
    state.janitor_count = 2;
    state.civilian_count = 2;
    state.receptionist_count = 1;
    state.grenade_count = 2;
    state.terminal_hacking = true;
    state.events.count = 4;
    state.player_on_elevator = 5;
    state.player_on_moving_platform = 6;
    state.facade_hazards_initialized = true;
    state.thrown_objects[0].active = true;
    state.birds[0].active = true;
    state.facade_wind_phase = FACADE_WIND_GUSTING;
    state.facade_wind_timer = 3.0f;
    state.facade_wind_dir = -1;
    state.facade_hazard_windup_timers[0] = 0.5f;
    state.facade_has_checkpoint = true;
    state.facade_checkpoint_y = 128.0f;
    state.level.runtime.wall_broken[3][4] = true;

    gameplay_state_begin_level(&state);

    CHECK(state.rng.state == expected.state);
    CHECK(state.enemy_count == 0);
    CHECK(state.janitor_count == 0);
    CHECK(state.civilian_count == 0);
    CHECK(state.receptionist_count == 0);
    CHECK(state.grenade_count == 0);
    CHECK(!state.terminal_hacking);
    CHECK(state.events.count == 0);
    CHECK(state.player_on_elevator == -1);
    CHECK(state.player_on_moving_platform == -1);
    CHECK(state.active_alarm_switch == -1);
    CHECK(!state.facade_hazards_initialized);
    CHECK(!state.thrown_objects[0].active);
    CHECK(!state.birds[0].active);
    CHECK(state.facade_wind_phase == FACADE_WIND_CALM);
    CHECK(state.facade_wind_timer == 0.0f);
    CHECK(state.facade_wind_dir == 0);
    CHECK(state.facade_hazard_windup_timers[0] == 0.0f);
    CHECK(!state.facade_has_checkpoint);
    CHECK(state.facade_checkpoint_y == 0.0f);
    CHECK(!state.interior_has_checkpoint);
    /* Assist flags come back to their neutral defaults; the shell re-applies
     * the player's choices right after. */
    CHECK(!state.assist_slow_enemies);
    CHECK(!state.assist_more_hearts);
    CHECK(gameplay_player_max_hp(&state) == PLAYER_MAX_HP);
    CHECK(gameplay_enemy_speed_scale(&state) == 1.0f);
    /* A sector loaded again is the sector as authored, walls included. */
    CHECK(!state.level.runtime.wall_broken[3][4]);
}

static void test_campaign_continue_flow(void)
{
    CampaignState campaign;
    campaign_reset(&campaign, false);
    CHECK(campaign.current_level == 0);
    CHECK(campaign.lives == PLAYER_LIVES);
    CHECK(campaign.continues_remaining == PLAYER_CONTINUES);
    CHECK(campaign.score == 0);
    CHECK(campaign.continue_timer == 0.0f);

    campaign.current_level = 2;
    campaign.score = 1234;
    for (int life = 1; life < PLAYER_LIVES; ++life)
        CHECK(!campaign_lose_life(&campaign));
    CHECK(campaign_lose_life(&campaign));
    CHECK(campaign_begin_continue(&campaign));
    CHECK(fabsf(campaign.continue_timer - CONTINUE_COUNTDOWN_TIME) < 0.001f);
    CHECK(!campaign_update_continue(&campaign,
                                    CONTINUE_COUNTDOWN_TIME * 0.5f));
    CHECK(campaign_accept_continue(&campaign));
    CHECK(campaign.current_level == 2);
    CHECK(campaign.score == 1234);
    CHECK(campaign.lives == PLAYER_LIVES);
    CHECK(campaign.continues_remaining == PLAYER_CONTINUES - 1);
    CHECK(campaign.continue_timer == 0.0f);

    while (campaign.continues_remaining > 0)
    {
        campaign.lives = 1;
        CHECK(campaign_lose_life(&campaign));
        CHECK(campaign_begin_continue(&campaign));
        CHECK(campaign_accept_continue(&campaign));
        CHECK(campaign.score == 1234);
    }

    /* Out of reserve continues the retry is still on offer — it just stops
     * insuring the score. Nobody is sent back to level one against their
     * will. */
    campaign.lives = 1;
    CHECK(campaign_lose_life(&campaign));
    CHECK(campaign_begin_continue(&campaign));
    CHECK(campaign_accept_continue(&campaign));
    CHECK(campaign.score == 0);
    CHECK(campaign.next_extra_life_score == EXTRA_LIFE_SCORE_STEP);
    CHECK(campaign.lives == PLAYER_LIVES);
    CHECK(campaign.current_level == 2);
}

static void test_campaign_continue_countdown_expires(void)
{
    CampaignState campaign;
    campaign_reset(&campaign, false);
    campaign.lives = 1;
    CHECK(campaign_lose_life(&campaign));
    CHECK(campaign_begin_continue(&campaign));
    CHECK(!campaign_update_continue(&campaign,
                                    CONTINUE_COUNTDOWN_TIME - 0.1f));
    CHECK(campaign_update_continue(&campaign, 0.11f));
    CHECK(campaign.continue_timer == 0.0f);
    CHECK(!campaign_accept_continue(&campaign));
    CHECK(campaign.continues_remaining == PLAYER_CONTINUES);
}

static void test_score_pays_out_extra_lives(void)
{
    CampaignState campaign;
    campaign_reset(&campaign, false);
    CHECK(!campaign_check_extra_life(&campaign));

    campaign.score = EXTRA_LIFE_SCORE_STEP;
    CHECK(campaign_check_extra_life(&campaign));
    CHECK(campaign.lives == PLAYER_LIVES + 1);
    CHECK(campaign.next_extra_life_score == 2 * EXTRA_LIFE_SCORE_STEP);
    CHECK(!campaign_check_extra_life(&campaign));

    /* Two thresholds crossed in one burst pay out one at a time. */
    campaign.score = 3 * EXTRA_LIFE_SCORE_STEP;
    CHECK(campaign_check_extra_life(&campaign));
    CHECK(campaign_check_extra_life(&campaign));
    CHECK(!campaign_check_extra_life(&campaign));
    CHECK(campaign.lives == PLAYER_LIVES + 3);

    /* At the cap the threshold still moves on, so a run parked on MAX_LIVES
     * does not bank every milestone it passes. What it must not do is report a
     * payout: the return value is what flashes 1UP on the strip, and a counter
     * that cannot go up announcing that it did is the HUD miscounting aloud. */
    campaign.lives = MAX_LIVES;
    campaign.score = 4 * EXTRA_LIFE_SCORE_STEP;
    CHECK(!campaign_check_extra_life(&campaign));
    CHECK(campaign.lives == MAX_LIVES);
    CHECK(campaign.next_extra_life_score == 5 * EXTRA_LIFE_SCORE_STEP);

    /* And one below the cap it still pays, so the guard is a cap and not an
     * off-by-one that swallows the last life the score ever buys. */
    campaign.lives = MAX_LIVES - 1;
    campaign.score = 5 * EXTRA_LIFE_SCORE_STEP;
    CHECK(campaign_check_extra_life(&campaign));
    CHECK(campaign.lives == MAX_LIVES);
}

/*
 * The par is the night clock's own allowance, and this is what stops the two
 * drifting apart.
 *
 * `SECTOR_PAR_SECONDS` is derived from `NIGHT_CLOCK_MINUTES_PER_SECTOR` rather
 * than written down beside it, so the arithmetic cannot go wrong — but a later
 * hand is free to replace the derivation with a number, which is exactly the
 * edit this repository keeps finding on the floor. Stated here as the rule it
 * is: the seconds a floor is given by the dial upstairs are the seconds it is
 * given down here.
 */
static void test_the_sector_par_is_the_night_clock_s_own(void)
{
    /* The slot the dial gives a floor, to the second: never longer than it, and
     * never more than a second short. It was an exact equality while the night
     * divided evenly into fifteen; seventeen sectors do not, and truncating is
     * what keeps the par a whole number the report can print. */
    CHECK(SECTOR_PAR_SECONDS <= NIGHT_CLOCK_MINUTES_PER_SECTOR * 60.0f);
    CHECK(SECTOR_PAR_SECONDS > NIGHT_CLOCK_MINUTES_PER_SECTOR * 60.0f - 1.0f);
    /* And it is a whole number of seconds, because the bonus is counted in
     * them and the report prints them. */
    CHECK((float)(int)SECTOR_PAR_SECONDS == SECTOR_PAR_SECONDS);
}

static void test_a_sector_pays_for_the_clock_and_for_a_clean_run(void)
{
    CampaignState campaign;
    int time_bonus = -1;
    int clean_bonus = -1;

    /* Out on the first frame: the whole par is spare, and nobody died. */
    campaign_reset(&campaign, false);
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus == (int)SECTOR_PAR_SECONDS * SECTOR_TIME_BONUS_PER_SECOND);
    CHECK(clean_bonus == SECTOR_CLEAN_BONUS);
    CHECK(campaign.score == time_bonus + clean_bonus);

    /* Half the slot spent, so half of it paid. */
    campaign_reset(&campaign, false);
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS * 0.5f;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus ==
          (int)(SECTOR_PAR_SECONDS * 0.5f) * SECTOR_TIME_BONUS_PER_SECOND);

    /* Exactly on par pays nothing for the clock, and a floor that ran over it
     * is not charged for the overrun — the bonus floors at nought rather than
     * going negative, because a sector must never be worth less than not
     * finishing it. */
    campaign_reset(&campaign, false);
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus == 0);

    campaign_reset(&campaign, false);
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS * 40.0f;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus == 0);
    CHECK(campaign.score == SECTOR_CLEAN_BONUS);

    /* One death is enough to lose the clean bonus, and it does not touch what
     * the clock paid: they are two separate answers to two separate fields. */
    campaign_reset(&campaign, false);
    campaign.level_deaths = 1;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(clean_bonus == 0);
    CHECK(time_bonus > 0);
    CHECK(campaign.score == time_bonus);

    /* The score is added to, never assigned: a sector's pay stacks on the run
     * the player already has. */
    campaign_reset(&campaign, false);
    campaign.score = 1234;
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS;
    campaign.level_deaths = 3;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(campaign.score == 1234);

    /* Both outputs are optional, for a caller that wants only the money. */
    campaign_reset(&campaign, false);
    campaign_award_sector_bonus(&campaign, NULL, NULL);
    CHECK(campaign.score > 0);

    /*
     * And a sector pays exactly once, however many times it is asked.
     *
     * `try_finish_current_level` runs every frame the player stands in the
     * exit, and it is only usually the last thing that happens on a floor: the
     * window route onto a climb can fail to load the sector above and returns
     * having changed nothing, so the next frame asks again. With the pay-out
     * above that branch — which is where it belongs, since all three ways out
     * pass through it and only one draws a report — an unlatched bonus is a
     * score fountain running at the frame rate.
     */
    campaign_reset(&campaign, false);
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    int once = campaign.score;
    CHECK(once > 0);
    for (int again = 0; again < 5; ++again)
    {
        campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
        /* Nothing paid, and nothing *reported* either: the report reads these
         * two, and a second call handing back the first call's numbers would
         * print a bonus the score never got. */
        CHECK(time_bonus == 0);
        CHECK(clean_bonus == 0);
        CHECK(campaign.score == once);
    }

    /* The next sector clears the latch, along with the three counters the
     * report reads off it. */
    campaign.score = 4321;
    campaign_begin_sector(&campaign);
    CHECK(!campaign.sector_bonus_paid);
    CHECK(campaign.level_elapsed_time == 0.0f);
    CHECK(campaign.level_deaths == 0);
    CHECK(campaign.level_start_score == 4321);
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus > 0);
    CHECK(campaign.score > 4321);

    /* A fresh run starts unlatched, so the lobby is paid for like every other
     * floor. */
    campaign_reset(&campaign, false);
    CHECK(!campaign.sector_bonus_paid);
}

static void test_blocked_exit_uses_separate_window(void)
{
    static const char data[] =
        "##########\n"
        "#Y S C TE#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 2027);
    CHECK(level_load_data(&state.level, "blocked exit", data, strlen(data),
                          &state.rng));
    CHECK(state.level.map.mode == LEVEL_MODE_INTERIOR);
    CHECK(state.level.map.has_exit);
    CHECK(state.level.map.has_window);
    CHECK(!state.level.runtime.exit_unlocked);

    gameplay_unlock_exit(&state);
    CHECK(!state.level.runtime.exit_unlocked);

    state.player.x = state.level.map.window_col * (float)TILE_SIZE;
    state.player.y = state.level.map.window_row * (float)TILE_SIZE;
    CHECK(gameplay_player_reached_exit(&state));

    state.player.x = state.level.map.exit_col * (float)TILE_SIZE;
    state.player.y = state.level.map.exit_row * (float)TILE_SIZE;
    CHECK(!gameplay_player_reached_exit(&state));
}

/*
 * And the stair door is a way out only once something has opened it.
 *
 * The twin of the test above, and for a long time it was the twin that did not
 * exist. `gameplay_player_reached_exit` answers the *window* first and returns,
 * so every caller in the suite — this file's own bots included — handed it a map
 * with a `Y` on it: measured with `make coverage`, the window branch had 66 900
 * executions against the door's nought, and the line that refuses a *locked*
 * door had never run at all. Seven of the seventeen sectors leave by their stair
 * core, the last of them being the roof, so what went unchecked was the finish
 * condition of the floor the whole campaign ends on.
 *
 * The crawl is in here because the box this is measured with is shorter on
 * elbows (`player_height`): a doorway is entered flat by anybody coming out of a
 * duct beside it, and it has to count when they do.
 */
static void test_the_stair_door_is_a_way_out_only_once_it_opens(void)
{
    static const char data[] =
        "##########\n"
        "#S  C  TE#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 4703);
    REQUIRE(level_load_data(&state.level, "stair", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);
    CHECK(state.level.map.has_exit);
    CHECK(!state.level.map.has_window);
    CHECK(!state.level.runtime.exit_unlocked);

    state.player.x = state.level.map.exit_col * (float)TILE_SIZE;
    state.player.y = state.level.map.exit_row * (float)TILE_SIZE;
    /* Standing in a locked doorway is not arriving, which is the whole of what
     * a card is for. */
    CHECK(!gameplay_player_reached_exit(&state));

    gameplay_unlock_exit(&state);
    CHECK(state.level.runtime.exit_unlocked);
    CHECK(gameplay_player_reached_exit(&state));

    /* One tile short of it is short of it. */
    state.player.x -= (float)TILE_SIZE;
    CHECK(!gameplay_player_reached_exit(&state));

    /* And on his elbows in the doorway, where the box is PLAYER_CRAWL_H tall
     * and sits on the floor of the tile rather than filling it. */
    state.player.x = state.level.map.exit_col * (float)TILE_SIZE;
    state.player.crawling = true;
    state.player.y = state.level.map.exit_row * (float)TILE_SIZE +
                     (float)(PLAYER_H - PLAYER_CRAWL_H);
    CHECK(gameplay_player_reached_exit(&state));
}

static void test_facade_mode_and_hazards_are_seeded(void)
{
    static const char data[] =
        ".........\n"
        ".Y.r.v...\n"
        ".S.......\n"
        ".........\n"
        "\n"
        "MODE FACADE\n";
    GameplayState first = {0};
    GameplayState second = {0};
    rng_seed(&first.rng, 2027);
    rng_seed(&second.rng, 2027);
    CHECK(level_load_data(&first.level, "facade", data, strlen(data),
                          &first.rng));
    CHECK(level_load_data(&second.level, "facade", data, strlen(data),
                          &second.rng));
    CHECK(first.level.map.mode == LEVEL_MODE_FACADE);
    CHECK(first.level.map.has_window);
    CHECK(!first.level.map.has_exit);
    CHECK(first.level.map.facade_hazard_spawn_count == 2);

    player_reset(&first.player, &first.level);
    player_reset(&second.player, &second.level);
    gameplay_climb_init(&first);
    gameplay_climb_init(&second);
    for (int i = 0; i < first.level.map.facade_hazard_spawn_count; ++i)
    {
        CHECK(fabsf(first.facade_hazard_spawn_timers[i] -
                    second.facade_hazard_spawn_timers[i]) < 0.0001f);
        first.facade_hazard_spawn_timers[i] = 0.0f;
        second.facade_hazard_spawn_timers[i] = 0.0f;
    }

    gameplay_climb_update(&first, SIM_STEP_DT);
    gameplay_climb_update(&second, SIM_STEP_DT);
    CHECK(first.birds[0].active);
    CHECK(fabsf(first.birds[0].vx - second.birds[0].vx) < 0.0001f);

    /* The thrower shouts first, so its brick appears a beat later. */
    for (int frame = 0; frame < SIM_STEPS(1.0f) &&
         !first.thrown_objects[0].active;
         ++frame)
    {
        gameplay_climb_update(&first, SIM_STEP_DT);
        gameplay_climb_update(&second, SIM_STEP_DT);
    }
    CHECK(first.thrown_objects[0].active);
    CHECK(first.thrown_objects[0].variant ==
          second.thrown_objects[0].variant);
    CHECK(fabsf(first.thrown_objects[0].vx -
                second.thrown_objects[0].vx) < 0.0001f);

    float previous_x = first.player.x;
    float previous_y = first.player.y;
    Input input = {.right = true, .up = true};
    gameplay_climb_update_player(&first, &input, 0.1f);
    CHECK(first.player.x > previous_x);
    CHECK(first.player.y < previous_y);
    CHECK(first.player.facade_climbing);
    CHECK(!first.player.on_ladder);
    CHECK(!first.player.on_ground);
}

static void test_facade_bird_hits_player(void)
{
    GameplayState state = {0};
    state.level.map.mode = LEVEL_MODE_FACADE;
    state.level.map.width = 10;
    state.level.map.height = 10;
    state.facade_hazards_initialized = true;
    state.player.x = 64.0f;
    state.player.y = 64.0f;
    state.player.facing = 1;
    state.birds[0] = (Bird){.x = 66.0f, .y = 68.0f, .active = true};

    gameplay_climb_update(&state, 0.0f);

    CHECK(state.player.dying);
    CHECK(!state.birds[0].active);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_FAN_HIT));
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_PLAYER_HIT));
}

/*
 * And the other hazard on the same wall, which had no test at all.
 *
 * A climb carries exactly two things that can cost a heart — a thrown object
 * and a bird — and in [gameplay_climb.c](../src/gameplay_climb.c) they are the
 * same twelve lines twice: shatter on masonry, leave the world, or hit the man.
 * The bird half has been held by the test above since it was written. The other
 * half was never simulated once: `make coverage` reported the whole
 * `gameplay_damage_player` block dark, on a mechanic that is live on five of the
 * seventeen sectors. The soak sweep even stages both of them for the *renderer*
 * (`--screen aftermath --level N` on a climb, which exists precisely because a
 * two-second window catching one is luck rather than coverage), so the picture
 * was covered on both and the simulation on one.
 *
 * Two symmetrical hazards with one test between them is not a gap you find by
 * reading either file; it is a gap you find by counting. Which is the argument
 * for counting.
 *
 * A heart rather than a death, unlike the bird test above, because that is the
 * assertion that says *one*: staged at full health a hit that took two hearts,
 * or took one twice in a step, would pass any check that only asked whether the
 * man went down.
 */
static void test_facade_thrown_object_hits_player(void)
{
    GameplayState state = {0};
    state.level.map.mode = LEVEL_MODE_FACADE;
    state.level.map.width = 10;
    state.level.map.height = 10;
    state.facade_hazards_initialized = true;
    state.player.x = 64.0f;
    state.player.y = 64.0f;
    state.player.facing = 1;
    state.player.hp = 3;
    state.thrown_objects[0] = (ThrownObject){
        .x = 66.0f, .y = 68.0f, .active = true};

    gameplay_climb_update(&state, 0.0f);

    CHECK(state.player.hp == 2);
    CHECK(!state.player.dying);
    /* Spent on the hit, so one brick cannot bill twice. */
    CHECK(!state.thrown_objects[0].active);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_CRATE_BREAK));
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_PLAYER_HIT));

    /*
     * And the guard that makes the mechanic survivable: the invulnerability
     * window after a hit. Without it the wall's own spawn rate would stack
     * against a climber who has just been hit and cannot move out of the way
     * yet, and the object has to *survive* the non-hit as well — consumed by a
     * hit it did not land, it would clear the wall for free every time a heart
     * was already spent.
     */
    GameplayState invulnerable = {0};
    invulnerable.level.map.mode = LEVEL_MODE_FACADE;
    invulnerable.level.map.width = 10;
    invulnerable.level.map.height = 10;
    invulnerable.facade_hazards_initialized = true;
    invulnerable.player.x = 64.0f;
    invulnerable.player.y = 64.0f;
    invulnerable.player.hp = 3;
    invulnerable.invuln_timer = 1.0f;
    invulnerable.thrown_objects[0] = (ThrownObject){
        .x = 66.0f, .y = 68.0f, .active = true};

    gameplay_climb_update(&invulnerable, 0.0f);

    CHECK(invulnerable.player.hp == 3);
    CHECK(invulnerable.thrown_objects[0].active);
}

/*
 * And a hazard that hits nothing at all has to go away.
 *
 * Both wall hazards live in fixed arrays — `MAX_THROWN_OBJECTS` and `MAX_BIRDS`
 * — and the spawn side of `gameplay_climb_update` only ever fills a free slot.
 * So a miss that never clears its slot is not a leak that grows; it is a wall
 * that quietly stops throwing, some way up the climb, with no symptom a player
 * could report beyond the top of a facade feeling oddly safe. There is no
 * assertion anywhere that would fire, which is what makes it worth pinning: the
 * ledge case and the player case were both tested and the *miss* — the ordinary
 * outcome, the one that happens most often — was not.
 *
 * Off each edge in turn rather than one, because the four bounds are four
 * comparisons and a sign error in any of them strands objects on that side only.
 */
static void test_a_facade_hazard_that_misses_is_cleaned_up(void)
{
    const float width = 10.0f * TILE_SIZE;
    const float height = 10.0f * TILE_SIZE;

    /* Each entry is a place a hazard can end up that is no longer on the wall,
     * plus the velocity that takes it further that way. */
    const struct
    {
        const char *where;
        float x, y, vx, vy;
    } gone[] = {
        {"off the left", -3.0f * TILE_SIZE, height * 0.5f, -200.0f, 0.0f},
        {"off the right", width + TILE_SIZE, height * 0.5f, 200.0f, 0.0f},
        {"past the pavement", width * 0.5f, height + 3.0f * TILE_SIZE, 0.0f,
         200.0f},
        {"above the roofline", width * 0.5f, -3.0f * TILE_SIZE, 0.0f, -200.0f},
    };

    for (size_t i = 0; i < sizeof(gone) / sizeof(gone[0]); ++i)
    {
        GameplayState state = {0};
        state.level.map.mode = LEVEL_MODE_FACADE;
        state.level.map.width = 10;
        state.level.map.height = 10;
        state.facade_hazards_initialized = true;
        /* Parked in a corner the hazards are travelling away from, at full
         * health, so a hit would be visible as a lost heart rather than read as
         * a clean-up. */
        state.player.x = 0.0f;
        state.player.y = 0.0f;
        state.player.hp = 3;

        state.thrown_objects[0] = (ThrownObject){
            .x = gone[i].x, .y = gone[i].y,
            .vx = gone[i].vx, .vy = gone[i].vy, .active = true};
        state.birds[0] = (Bird){
            .x = gone[i].x, .y = gone[i].y,
            .vx = gone[i].vx, .vy = gone[i].vy, .active = true};

        for (int step = 0; step < SIM_STEPS(4.0f); ++step)
            gameplay_climb_update(&state, SIM_STEP_DT);

        CHECK(!state.thrown_objects[0].active);
        CHECK(!state.birds[0].active);
        CHECK(state.player.hp == 3);
    }
}

static void test_facade_ledges_block_and_are_routed_around(void)
{
    /* A ledge with one gap: pressing up alone stalls, and adding a sideways
     * press slides along the ledge until the gap is found. */
    static const char data[] =
        ".........\n"
        ".Y.......\n"
        ".........\n"
        "###...###\n"
        ".........\n"
        ".S.......\n"
        ".........\n"
        "\n"
        "MODE FACADE\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 4242);
    CHECK(level_load_data(&state.level, "ledges", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    Input up = {.up = true};
    for (int frame = 0; frame < SIM_STEPS(3.0f); ++frame)
        gameplay_climb_update_player(&state, &up, SIM_STEP_DT);
    /* Stopped underneath the ledge rather than passing through it. */
    CHECK(state.player.y > 3 * (float)TILE_SIZE);
    float blocked_y = state.player.y;

    Input up_right = {.up = true, .right = true};
    for (int frame = 0; frame < SIM_STEPS(1.0f); ++frame)
        gameplay_climb_update_player(&state, &up_right, SIM_STEP_DT);
    CHECK(state.player.x > state.level.map.start_x);
    CHECK(state.player.y < blocked_y);

    for (int frame = 0; frame < SIM_STEPS(4.0f); ++frame)
        gameplay_climb_update_player(&state, &up, SIM_STEP_DT);
    CHECK(state.player.y < 2 * (float)TILE_SIZE);
}

static void test_facade_ledge_stops_thrown_object_and_bird(void)
{
    static const char data[] =
        ".........\n"
        ".Y.......\n"
        "#########\n"
        ".S.......\n"
        "\n"
        "MODE FACADE\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 909);
    CHECK(level_load_data(&state.level, "cover", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    state.facade_hazards_initialized = true;

    state.thrown_objects[0] = (ThrownObject){
        .x = 4 * (float)TILE_SIZE, .y = 2 * (float)TILE_SIZE + 8.0f, .vx = 0.0f, .vy = 0.0f, .active = true};
    state.birds[0] = (Bird){
        .x = 6 * (float)TILE_SIZE, .y = 2 * (float)TILE_SIZE + 8.0f, .vx = 10.0f, .vy = 0.0f, .active = true};

    gameplay_climb_update(&state, SIM_STEP_DT);

    CHECK(!state.thrown_objects[0].active);
    CHECK(!state.birds[0].active);
    CHECK(!state.player.dying);
}

/*
 * A deliberately dumb climber: hold up, and whenever that stops making
 * progress, sweep sideways until it can rise again, reversing at whatever
 * stops the sweep. If even this reaches the window then the map has no dead
 * end, which is the property worth pinning about the level itself.
 */
static bool facade_bot_reaches_window(GameplayState *state, float seconds)
{
    const float step = SIM_STEP_DT;
    int scan_dir = -1;
    bool scanning = false;

    for (int frame = 0; frame < SIM_STEPS(seconds); ++frame)
    {
        if (gameplay_player_reached_exit(state))
            return true;

        Input input = {.up = true};
        if (scanning)
        {
            input.left = scan_dir < 0;
            input.right = scan_dir > 0;
        }
        float previous_x = state->player.x;
        float previous_y = state->player.y;
        gameplay_climb_update_player(state, &input, step);

        if (state->player.y < previous_y - 0.001f)
        {
            scanning = false;
            continue;
        }
        if (!scanning)
        {
            scanning = true;
            continue;
        }
        /* The sweep ran into masonry or the edge of the face: turn around. */
        if (fabsf(state->player.x - previous_x) < 0.001f)
            scan_dir = -scan_dir;
    }
    return gameplay_player_reached_exit(state);
}

static void test_embedded_facades_have_a_route_to_the_window(void)
{
    int climbs = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        GameplayState state = {0};
        rng_seed(&state.rng, 3030 + i);
        CHECK(level_load_data(&state.level, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data,
                              EMBEDDED_LEVELS[i].size, &state.rng));
        if (state.level.map.mode != LEVEL_MODE_FACADE)
            continue;
        climbs++;
        player_reset(&state.player, &state.level);
        /* Allowance scales with the wall: a taller face is more sweeping,
         * not a different kind of route. */
        CHECK(facade_bot_reaches_window(
            &state, 3.0f * (float)state.level.map.height + 40.0f));
    }
    CHECK(climbs == 5);
}

static void test_facade_checkpoint_banks_height(void)
{
    GameplayState state = {0};
    rng_seed(&state.rng, 606);
    CHECK(level_load_data(&state.level, EMBEDDED_LEVELS[2].name,
                          EMBEDDED_LEVELS[2].data,
                          EMBEDDED_LEVELS[2].size, &state.rng));
    player_reset(&state.player, &state.level);
    float start_y = state.player.y;

    Input up = {.up = true};
    for (int frame = 0; frame < SIM_STEPS(2.0f); ++frame)
        gameplay_climb_update_player(&state, &up, SIM_STEP_DT);
    CHECK(state.facade_has_checkpoint);
    CHECK(state.facade_checkpoint_y < start_y);
    float banked_y = state.facade_checkpoint_y;
    CHECK(state.player.y <= banked_y);

    /* Losing a life restarts at the map spawn, and the climb is then handed
     * back the height it had already earned. */
    state.thrown_objects[0].active = true;
    player_reset(&state.player, &state.level);
    CHECK(state.player.y == start_y);
    gameplay_climb_restore_checkpoint(&state);
    CHECK(state.player.y == banked_y);
    CHECK(state.player.x == state.facade_checkpoint_x);
    CHECK(!state.thrown_objects[0].active);

    /* Banking only ever moves up the wall. */
    for (int frame = 0; frame < SIM_STEPS(2.0f); ++frame)
    {
        Input down = {.down = true};
        gameplay_climb_update_player(&state, &down, SIM_STEP_DT);
    }
    CHECK(state.facade_checkpoint_y == banked_y);
}

static void test_facade_wind_warns_then_pushes_unless_sheltered(void)
{
    static const char data[] =
        ".........\n"
        ".Y.......\n"
        ".........\n"
        ".S.......\n"
        "\n"
        "MODE FACADE\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 77);
    CHECK(level_load_data(&state.level, "wind", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    gameplay_climb_init(&state);
    CHECK(state.facade_wind_phase == FACADE_WIND_CALM);
    CHECK(gameplay_climb_wind_push(&state) == 0.0f);

    /* Run the cycle forward: calm, an announced warning, then the gust. */
    for (int frame = 0; frame < SIM_STEPS(60.0f) &&
                        state.facade_wind_phase != FACADE_WIND_WARNING;
         ++frame)
    {
        game_events_clear(&state.events);
        gameplay_climb_update(&state, SIM_STEP_DT);
    }
    CHECK(state.facade_wind_phase == FACADE_WIND_WARNING);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND, SFX_WIND_GUST));
    /* The warning beat itself never pushes. */
    CHECK(gameplay_climb_wind_push(&state) == 0.0f);

    for (int frame = 0; frame < SIM_STEPS(6.0f) &&
                        state.facade_wind_phase != FACADE_WIND_GUSTING;
         ++frame)
    {
        game_events_clear(&state.events);
        gameplay_climb_update(&state, SIM_STEP_DT);
    }
    CHECK(state.facade_wind_phase == FACADE_WIND_GUSTING);
    float push = gameplay_climb_wind_push(&state);
    CHECK(fabsf(push) > 0.0f);

    Input idle = {0};
    /* Park him mid-face so there is room for masonry on either side. */
    state.player.x = 4 * (float)TILE_SIZE;
    float before_x = state.player.x;
    gameplay_climb_update_player(&state, &idle, 0.1f);
    CHECK(!state.facade_wind_sheltered);
    CHECK(fabsf(state.player.x - before_x) > 1.0f);
    CHECK((state.player.x > before_x) == (push > 0.0f));

    /* Standing in the lee of masonry upwind cancels the same gust. */
    int shelter_col = 4 + (push > 0.0f ? -2 : 2);
    int shelter_row = (int)((state.player.y + PLAYER_H * 0.5f) / TILE_SIZE);
    state.level.map.tiles[shelter_row][shelter_col] = TILE_WALL;
    state.player.x = 4 * (float)TILE_SIZE;
    before_x = state.player.x;
    gameplay_climb_update_player(&state, &idle, 0.1f);
    CHECK(state.facade_wind_sheltered);
    CHECK(fabsf(state.player.x - before_x) < 0.001f);

    /* The flag has to survive the rest of the frame: the HUD and the world
     * renderer read it after the hazards have been stepped. */
    gameplay_climb_update(&state, SIM_STEP_DT);
    CHECK(state.facade_wind_sheltered);

    /*
     * And the half of the cycle that was missing: the gust has to *stop*.
     *
     * Everything above walked calm, warning, gust and then stopped asking,
     * which left the `FACADE_WIND_GUSTING` arm of `update_wind`'s switch — the
     * arm that hands the wall back to calm — never simulated. `make coverage`
     * had it dark. What that hid is not subtle: a gust that never expires is a
     * climb permanently pushed sideways, on the five tallest maps in the game,
     * and every assertion in this test would still have passed.
     *
     * This is `test_coyote_time_allows_a_late_jump` again — a test whose two
     * branches were both taking the same one — and the same fix: require one of
     * each. The wall must come back to calm, stop pushing when it does, and
     * announce the next gust rather than resuming one silently.
     */
    int returned_to_calm = 0;
    for (int frame = 0; frame < SIM_STEPS(30.0f); ++frame)
    {
        game_events_clear(&state.events);
        gameplay_climb_update(&state, SIM_STEP_DT);
        if (state.facade_wind_phase == FACADE_WIND_CALM)
        {
            returned_to_calm = 1;
            break;
        }
    }
    CHECK(returned_to_calm);
    CHECK(gameplay_climb_wind_push(&state) == 0.0f);

    /* A calm wall is not a finished one: the cycle has to come round, and it
     * has to announce itself when it does. A second gust arriving unannounced
     * would be the one hazard on the facade with no tell. */
    int warned_again = 0;
    for (int frame = 0; frame < SIM_STEPS(60.0f); ++frame)
    {
        game_events_clear(&state.events);
        gameplay_climb_update(&state, SIM_STEP_DT);
        if (state.facade_wind_phase == FACADE_WIND_WARNING)
        {
            warned_again = events_have_sound(&state.events, GAME_EVENT_SOUND,
                                             SFX_WIND_GUST);
            break;
        }
    }
    CHECK(warned_again);
}

static void test_facade_thrower_winds_up_before_releasing(void)
{
    static const char data[] =
        ".........\n"
        ".Y.......\n"
        ".r.......\n"
        ".S.......\n"
        "\n"
        "MODE FACADE\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 5150);
    CHECK(level_load_data(&state.level, "windup", data, strlen(data),
                          &state.rng));
    CHECK(state.level.map.facade_hazard_spawn_count == 1);
    player_reset(&state.player, &state.level);
    gameplay_climb_init(&state);
    state.facade_hazard_spawn_timers[0] = 0.0f;

    game_events_clear(&state.events);
    gameplay_climb_update(&state, SIM_STEP_DT);
    /* The shout lands first and nothing is in the air yet. */
    CHECK(state.facade_hazard_windup_timers[0] > 0.0f);
    CHECK(!state.thrown_objects[0].active);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_GUARD_TALK));

    game_events_clear(&state.events);
    for (int frame = 0; frame < SIM_STEPS(1.0f) &&
         !state.thrown_objects[0].active;
         ++frame)
        gameplay_climb_update(&state, SIM_STEP_DT);
    CHECK(state.thrown_objects[0].active);
    CHECK(state.facade_hazard_windup_timers[0] == 0.0f);
}

static void test_level_collision_stops_at_wall(void)
{
    static const char data[] =
        "#####\n"
        "#S E#\n"
        "#####\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 7);
    CHECK(level_load_data(&level, "collision", data, strlen(data), &rng));

    float x = level.map.start_x;
    float y = level.map.start_y;
    float vx = 100.0f;
    float vy = 0.0f;
    bool on_ground = false;
    for (int i = 0; i < SIM_STEPS(1.0f); ++i)
        level_move(&level, &x, &y, &vx, &vy,
                   PLAYER_W, PLAYER_H, SIM_STEP_DT, false, &on_ground, false,
                   STANCE_UPRIGHT);

    CHECK(x + PLAYER_W <= 4.0f * TILE_SIZE + 0.01f);
    CHECK(fabsf(vx) < 0.01f);

    y -= 2.0f;
    vy = 100.0f;
    /* `level_move` reports the ground on the step the fall is *stopped*,
     * so this walks the 2px rather than covering it in one 50ms move and
     * stops on the landing: kept running, the next step has no downward
     * velocity left to be blocked and hands back false. */
    for (int i = 0; i < SIM_STEPS(0.05f) && !on_ground; ++i)
        level_move(&level, &x, &y, &vx, &vy,
                   PLAYER_W, PLAYER_H, SIM_STEP_DT, false, &on_ground,
                   false, STANCE_UPRIGHT);
    CHECK(on_ground);
}

/*
 * Solidity is two questions: what stops a bullet, a line of sight, a crate or a
 * guard is the building, and what stops a *body* is the building as that body
 * happens to be shaped. The duct is what the split was cut for — masonry to a
 * man on his feet, a way through to a man on his elbows.
 *
 * This test used to require the two questions to agree on every tile, which was
 * true until a tile meant something by the distinction. It now pins the shape
 * that actually matters and is easy to lose: **exactly one tile in the enum is
 * stance-aware.** It walks the enum rather than a list, so a second one added
 * anywhere — deliberately or by a stray case in `level_blocks_stance` — fails
 * here without anybody having to remember to come and look.
 */
static void test_only_the_duct_answers_the_two_stances_differently(void)
{
    static const char data[] =
        "#####\n"
        "#S E#\n"
        "#####\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 23);
    CHECK(level_load_data(&level, "stance", data, strlen(data), &rng));

    int stance_aware = 0;
    for (int tile = TILE_EMPTY; tile < TILE_TYPE_COUNT; ++tile)
    {
        level.map.tiles[1][2] = (TileType)tile;
        bool building = level_is_solid(&level, 2, 1);
        /* Upright is the building's own answer for every tile there is. */
        CHECK(level_blocks_stance(&level, 2, 1, STANCE_UPRIGHT) == building);
        if (level_blocks_stance(&level, 2, 1, STANCE_CRAWLING) != building)
            stance_aware++;
    }
    CHECK(stance_aware == 1);

    /* And it is the duct, in the direction that makes it a duct rather than a
     * hole: wall standing, gap crawling. */
    level.map.tiles[1][2] = TILE_VENT;
    CHECK(level_blocks_stance(&level, 2, 1, STANCE_UPRIGHT));
    CHECK(!level_blocks_stance(&level, 2, 1, STANCE_CRAWLING));

    /* The one tile whose answer is runtime state rather than the map, asked in
     * both of its states: the patch as it was authored, and the hole a blast
     * left in it. Neither state cares about the posture. */
    level.map.tiles[1][2] = TILE_WEAK_WALL;
    CHECK(level_blocks_stance(&level, 2, 1, STANCE_CRAWLING));
    CHECK(level_break_wall(&level, 2, 1));
    CHECK(!level_blocks_stance(&level, 2, 1, STANCE_UPRIGHT));
    CHECK(!level_blocks_stance(&level, 2, 1, STANCE_CRAWLING));

    /* Out of bounds is the world edge rather than a tile, and it is wall to
     * anybody in any posture. */
    CHECK(level_blocks_stance(&level, -1, 1, STANCE_CRAWLING));
    CHECK(level_blocks_stance(&level, 2, -1, STANCE_CRAWLING));
    CHECK(level_blocks_stance(&level, level.map.width, 1, STANCE_CRAWLING));

    /* And then the same question of `level_move`, which is the half a predicate
     * cannot answer: the four collision tests inside it have to be asking with
     * the stance they were handed rather than with a constant. Same map, same
     * box height, same push — the only thing varied is the posture, and a duct
     * in the way is what makes the two outcomes differ.
     *
     * This is the check that was impossible to write before a stance-aware tile
     * existed: while every tile answered both stances the same, a `level_move`
     * that hardcoded `STANCE_UPRIGHT` was indistinguishable from one that
     * threaded the parameter through. It is distinguishable now.
     */
    static const char ducted[] =
        "##########\n"
        "#S  =   E#\n"
        "##########\n";
    Level shaft;
    CHECK(level_load_data(&shaft, "duct crawl", ducted, strlen(ducted), &rng));
    float duct_left = 4.0f * (float)TILE_SIZE;

    float reached[2] = {0.0f, 0.0f};
    for (int pass = 0; pass < 2; ++pass)
    {
        Stance stance = pass == 0 ? STANCE_UPRIGHT : STANCE_CRAWLING;
        float x = shaft.map.start_x;
        float y = shaft.map.start_y + (float)(PLAYER_H - PLAYER_CRAWL_H);
        float vx = 100.0f;
        float vy = 0.0f;
        bool on_ground = false;
        for (int i = 0; i < SIM_STEPS(2.0f); ++i)
            level_move(&shaft, &x, &y, &vx, &vy,
                       PLAYER_W, (float)PLAYER_CRAWL_H, SIM_STEP_DT, false,
                       &on_ground, false, stance);
        reached[pass] = x;
    }
    /* Upright: stopped dead on the near face of the trunking. */
    CHECK(reached[0] + PLAYER_W <= duct_left + 0.01f);
    /* Crawling: through it and on down the corridor. */
    CHECK(reached[1] > duct_left + (float)TILE_SIZE);
}

static void test_player_dies_from_a_high_fall(void)
{
    GameplayState safe = {0};
    safe.player.on_ground = true;
    safe.player.facing = 1;

    gameplay_handle_player_landing(&safe, false,
                                   PLAYER_FATAL_FALL_SPEED - 1.0f);
    CHECK(!safe.player.dying);
    CHECK(events_have_sound(&safe.events, GAME_EVENT_SOUND, SFX_LAND));

    static const char data[] =
        "#####\n"
        "#S E#\n"
        "#   #\n"
        "#   #\n"
        "#   #\n"
        "#   #\n"
        "#   #\n"
        "#   #\n"
        "#   #\n"
        "#####\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 81);
    CHECK(level_load_data(&state.level, "fatal fall", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    Input input = {0};

    for (int frame = 0; frame < SIM_STEPS(2.0f) && !state.player.dying; ++frame)
    {
        bool was_grounded = state.player.on_ground;
        float fall_speed =
            player_update(&state.player, &state.level, &input, SIM_STEP_DT);
        gameplay_handle_player_landing(&state, was_grounded, fall_speed);
    }

    CHECK(state.player.dying);
    CHECK(state.player.death_timer > 0.0f);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_PLAYER_HIT));
}

/*
 * An elevator carries its rider through the slabs the shaft is drilled through.
 *
 * The shaft is one tile wide and the player box is 26 of those 32 pixels, so
 * anyone who stepped aboard off-centre still overlaps the column next door.
 * That overlap is free in the open storeys and used to be fatal the instant the
 * lift lifted his head into a floor slab: the wall arrived above him rather
 * than beside him, so nothing had pushed him clear, and the crush check killed
 * him for it. Every position the platform will accept has to reach the top.
 */
static void test_elevator_carries_an_off_centre_rider_through_a_slab(void)
{
    static const char data[] =
        "##########\n"
        "#        #\n" /* the storey the lift tops out in */
        "#   V    #\n"
        "####V#####\n" /* the slab the shaft is drilled through */
        "#   V    #\n"
        "# S V   E#\n"
        "##########\n";
    const int shaft_col = 4;

    int tried = 0;
    int arrived = 0;
    for (float offset = 0.5f; offset < (float)TILE_SIZE; offset += 0.5f)
    {
        GameplayState state = {0};
        rng_seed(&state.rng, 91);
        CHECK(level_load_data(&state.level, "shaft", data, strlen(data),
                              &state.rng));
        CHECK(state.level.runtime.elevator_count == 1);
        player_reset(&state.player, &state.level);

        float x = (float)shaft_col * TILE_SIZE - PLAYER_W * 0.5f + offset;
        if ((int)floorf((x + PLAYER_W * 0.5f) / TILE_SIZE) != shaft_col)
            continue;
        state.player.x = x;
        state.player.y = state.level.runtime.elevators[0].y - PLAYER_H;
        state.player.on_ground = true;

        /* The order update_playing runs these in: carry, crush, physics, move
         * the platforms, then stand the player back on his. */
        Input idle = {0};
        gameplay_ride_platforms(&state, SIM_STEP_DT);
        CHECK(state.player_on_elevator == 0);
        bool topped_out = false;
        for (int frame = 0;
             frame < SIM_STEPS(4.0f) && !state.player.dying && !topped_out;
             ++frame)
        {
            gameplay_carry_player_on_elevator(&state, SIM_STEP_DT);
            gameplay_resolve_player_crush(&state);
            player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
            level_update_elevators(&state.level, SIM_STEP_DT);
            gameplay_ride_platforms(&state, SIM_STEP_DT);
            topped_out = state.player_on_elevator == 0 &&
                         state.player.y < 2.0f * TILE_SIZE;
        }

        tried++;
        arrived += topped_out && !state.player.dying;
    }
    CHECK(tried > 40);
    CHECK(arrived == tried);
}

/* Being squeezed aside is not the same as having nowhere to go: a player whose
 * whole box is under a slab is still crushed. */
static void test_player_under_a_slab_is_crushed(void)
{
    static const char data[] =
        "######\n"
        "#S  E#\n"
        "######\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 5);
    CHECK(level_load_data(&state.level, "crush", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    state.player.x = (float)TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y -= 2.0f;

    CHECK(gameplay_resolve_player_crush(&state));
    CHECK(state.player.dying);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND, SFX_PLAYER_HIT));
}

/*
 * Grabbing a ladder centres the player in the rung column.
 *
 * The player box is 26 of a 32px tile, so stopping anywhere in the outer 6px
 * leaves it overlapping the neighbouring column. When a floor slab sits beside
 * the ladder that overlap blocks the climb on the vertical axis and pressing up
 * silently does nothing, from roughly the outer third of every ladder in the
 * campaign. The snap is what makes a ladder catch from wherever the player
 * happened to stop walking.
 */
static void test_ladder_mount_centres_the_player(void)
{
    static const char data[] =
        "########\n"
        "#  H  E#\n" /* upper standing row: the run ends level with the floor */
        "###H####\n" /* the slab either side of the ladder is what blocks */
        "#  H   #\n"
        "# SH   #\n" /* lower standing row: the run starts level with it */
        "########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 7);
    CHECK(level_load_data(&level, "ladder", data, strlen(data), &rng));

    const int ladder_col = 3;
    const float centred =
        (float)ladder_col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    Input up = {0};
    up.up = true;

    /* Every standing position that is genuinely inside the rung column must
     * reach the floor above, not just the ones near its middle. */
    int climbed = 0;
    int tried = 0;
    for (float offset = 0.5f; offset < (float)TILE_SIZE; offset += 0.5f)
    {
        float x = (float)ladder_col * TILE_SIZE - PLAYER_W * 0.5f + offset;
        if ((int)floorf((x + PLAYER_W * 0.5f) / TILE_SIZE) != ladder_col)
            continue;

        Player player;
        player_reset(&player, &level);
        player.x = x;
        player.y = 4.0f * TILE_SIZE;
        player.on_ground = true;

        player_update(&player, &level, &up, SIM_STEP_DT);
        CHECK(player.on_ladder);
        CHECK(fabsf(player.x - centred) < 0.01f);

        for (int frame = 0; frame < SIM_STEPS(4.0f); ++frame)
            player_update(&player, &level, &up, SIM_STEP_DT);
        tried++;
        climbed += player.y < 2.0f * TILE_SIZE;
    }
    CHECK(tried > 40);
    CHECK(climbed == tried);
}

/* A ladder tile is also a one-way platform. When its run ends flush with the
 * landing, the player standing on top does not overlap the rung: it begins
 * immediately below his feet. Down must grab that rung instead of selecting
 * the grounded crawling posture. */
static void test_player_descends_from_top_of_ladder(void)
{
    static const char data[] =
        "########\n"
        "#      #\n"
        "#  S  E#\n"
        "#  H   #\n"
        "########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 17);
    CHECK(level_load_data(&level, "ladder descent", data, strlen(data), &rng));

    Player player;
    player_reset(&player, &level);
    player.on_ground = true;
    float start_y = player.y;
    Input down = {.down = true};

    player_update(&player, &level, &down, SIM_STEP_DT);

    CHECK(player.on_ladder);
    CHECK(!player.crawling);
    CHECK(player.y > start_y);
}

/*
 * Down on the ground is the crawl, and standing up asks the ceiling first.
 *
 * This suite called `player_update` about two hundred thousand times without
 * one of those calls ever holding `down` on a floor, so the posture change
 * itself — the box shrinking, the feet staying put, and the ceiling that
 * refuses to give the height back — was compiled and never executed. Every
 * other test of crawling sets `player.crawling` by hand, which is exactly the
 * shape `make coverage` reported as covered: the function ran, the mechanic
 * inside it did not.
 *
 * **And the third branch is only reachable off the tile grid**, which is worth
 * writing down because it looks like a bug and is not. `PLAYER_H` is 32 and so
 * is `TILE_SIZE`, so a man standing with his feet on a tile boundary occupies
 * exactly the one row a crawling man does — no ordinary corridor in the
 * building is too low to stand up in, and crawling is a shooting height and a
 * way of being hard to see rather than a way through anything. The refusal
 * exists for the surfaces that are *not* on the grid: a lift car
 * (`level_update_elevators` moves it continuously) and a panel on its way down.
 * A crawl begun on one of those, under a slab, is the case the branch is for,
 * and the arithmetic is exact: the crawl box is `PLAYER_CRAWL_H` tall and the
 * standing box `PLAYER_H`, so the refusal fires on any surface that leaves the
 * feet more than the first and no more than the second below the nearest tile
 * boundary above them.
 */
static void test_holding_down_enters_and_leaves_the_crawl(void)
{
    static const char data[] =
        "############\n"
        "#          #\n"
        "#   ####   #\n"
        "#S        E#\n"
        "############\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 17);
    REQUIRE(level_load_data(&level, "crawl space", data, strlen(data), &rng));

    Player player;
    player_reset(&player, &level);
    player.on_ground = true;
    float feet = player.y + (float)PLAYER_H;

    Input down = {.down = true};
    Input nothing = {0};

    /* The posture drops and the feet do not: a crawl that moved the man down
     * would put him through the floor he is lying on, and one that moved him up
     * would lift him off it. */
    player_update(&player, &level, &down, SIM_STEP_DT);
    CHECK(player.crawling);
    CHECK(fabsf((player.y + (float)PLAYER_CRAWL_H) - feet) < 1.0f);

    player_update(&player, &level, &nothing, SIM_STEP_DT);
    CHECK(!player.crawling);
    CHECK(fabsf((player.y + (float)PLAYER_H) - feet) < 1.0f);

    /* On the grid, the slab overhead changes nothing — see the note above. This
     * is asserted rather than assumed, because if it ever stops being true the
     * comment is what is wrong. */
    player_reset(&player, &level);
    player.x = 4.0f * TILE_SIZE + 3.0f; /* under the `####` run */
    player.on_ground = true;
    player_update(&player, &level, &down, SIM_STEP_DT);
    CHECK(player.crawling);
    player_update(&player, &level, &nothing, SIM_STEP_DT);
    CHECK(!player.crawling);

    /* Off the grid, it does. The feet are put 8px above the floor — a lift car
     * part way between two storeys — which leaves them 24 below the boundary at
     * the top of their own row: more than `PLAYER_CRAWL_H` and less than
     * `PLAYER_H`, so the slab is inside the standing box and outside the
     * crawling one. */
    player_reset(&player, &level);
    player.x = 4.0f * TILE_SIZE + 3.0f;
    player.on_ground = true;
    player.vy = 0.0f;
    player.crawling = true;
    player.y = (feet - 8.0f) - (float)PLAYER_CRAWL_H;
    for (int step = 0; step < SIM_STEPS(0.1f); ++step)
    {
        /* Held on the car rather than allowed to fall, which is what
         * `gameplay_carry_player_on_elevator` does for the real thing. */
        player.y = (feet - 8.0f) - (float)PLAYER_CRAWL_H;
        player.vy = 0.0f;
        player.on_ground = true;
        player_update(&player, &level, &nothing, SIM_STEP_DT);
        CHECK(player.crawling);
    }
}

/*
 * The lid of a shaft is a walkway, not somewhere to lie down.
 *
 * Trunking is the only tile in the building that answers the two postures
 * differently, and the crawl was written when nothing did. So a player standing
 * on top of a duct who pressed DOWN lowered his box, lost the floor he was
 * standing on — `level_blocks_stance` opens a shaft to a crawler — lost
 * `on_ground` with it, and was stood straight back up by the next step, because
 * `want_crawl` requires `on_ground`. Then it happened again: **240 stand/crawl
 * flips a second** for as long as the key was held, the box 14px taller and
 * shorter by turns, the pose the renderer draws alternating with them, and
 * `crawling` — one of the two ways of being hard to see — true on half the
 * sight checks a guard makes. It was live on all four runs in sector 12,
 * because a duct is let into a storey and every one of them has that storey's
 * own air above it, and nothing said a word: both existing duct tests ask only
 * about the *horizontal* crawl, which is the direction that works.
 *
 * Counting the flips is the assertion, because the symptom is oscillation
 * rather than a wrong value: a check that read `crawling` once would have
 * passed on whichever step it happened to land on.
 *
 * The three blocks after it are what make this a fix rather than a deletion.
 * The crawl still has to work on ordinary masonry; it has to work *inside* the
 * shaft, which is the whole reason the shaft exists; and it has to survive a
 * surface that holds the player up with no solid tile under his feet at all.
 * That last one is how a careless version of this rule breaks the game rather
 * than mending it — a rung, a cracked panel and a moving platform are caught by
 * `level_move`'s own one-way tests, which know nothing about posture, so "the
 * tile under the feet must be masonry" would take the crawl away on seven
 * shipped floors to close a hole on one.
 */
static void test_the_lid_of_a_shaft_is_not_somewhere_to_lie_down(void)
{
    static const char data[] =
        "############\n"
        "#          #\n"
        "#          #\n"
        "#S ======  E\n"
        "############\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 41);
    REQUIRE(level_load_data(&level, "shaft lid", data, strlen(data), &rng));

    Input down = {.down = true};
    Input nothing = {0};

    /* On the lid: the run is row 3, so his feet rest on top of it and his box
     * stands in row 2. */
    Player player;
    player_reset(&player, &level);
    player.x = 5.0f * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    player.y = 3.0f * TILE_SIZE - (float)PLAYER_H;
    player.vy = 0.0f;
    player.crawling = false;
    for (int step = 0; step < SIM_STEPS(0.1f); ++step)
        player_update(&player, &level, &nothing, SIM_STEP_DT);
    REQUIRE(player.on_ground);
    REQUIRE((int)floorf((player.y + (float)PLAYER_H) / TILE_SIZE) == 3);

    int flips = 0;
    bool was_crawling = player.crawling;
    float lowest = player.y;
    float highest = player.y;
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
    {
        player_update(&player, &level, &down, SIM_STEP_DT);
        if (player.crawling != was_crawling)
        {
            flips++;
            was_crawling = player.crawling;
        }
        if (player.y < highest)
            highest = player.y;
        if (player.y > lowest)
            lowest = player.y;
    }
    CHECK(flips == 0);
    CHECK(!player.crawling);
    /* And he is where he was, rather than jittering by the difference between
     * the two box heights. */
    CHECK(lowest - highest < 1.0f);

    /* The crawl itself still works on masonry. */
    player_reset(&player, &level);
    player.on_ground = true;
    player_update(&player, &level, &down, SIM_STEP_DT);
    CHECK(player.crawling);

    /* And inside the shaft, which is the posture the shaft exists for: he is
     * held by the slab under the trunking and stays down for as long as the key
     * is held. */
    player_reset(&player, &level);
    player.crawling = true;
    player.x = 5.0f * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    player.y = 4.0f * TILE_SIZE - (float)PLAYER_CRAWL_H;
    player.vy = 0.0f;
    player.on_ground = true;
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
    {
        player_update(&player, &level, &down, SIM_STEP_DT);
        CHECK(player.crawling);
    }
    CHECK(level_tile(&level,
                     (int)floorf((player.x + PLAYER_W * 0.5f) / TILE_SIZE),
                     (int)floorf((player.y + PLAYER_CRAWL_H * 0.5f) /
                                 TILE_SIZE)) == TILE_VENT);

    /* And on a moving platform, where nothing under the feet is a tile at all.
     * `P` is on seven shipped floors and the crawl has to survive every one of
     * them. */
    static const char riding[] =
        "##########\n"
        "#        #\n"
        "#  PPPP  #\n"
        "#        #\n"
        "#S      E#\n"
        "##########\n";
    Level patrol;
    REQUIRE(level_load_data(&patrol, "rider", riding, strlen(riding), &rng));
    REQUIRE(patrol.runtime.moving_platform_count > 0);
    const MovingPlatform *car = &patrol.runtime.moving_platforms[0];
    player_reset(&player, &patrol);
    player.x = car->x + (TILE_SIZE - PLAYER_W) * 0.5f;
    player.y = (float)car->row * TILE_SIZE - (float)PLAYER_H - 2.0f;
    player.vy = 0.0f;
    player.crawling = false;
    for (int step = 0; step < SIM_STEPS(0.2f); ++step)
        player_update(&player, &patrol, &nothing, SIM_STEP_DT);
    REQUIRE(player.on_ground);
    /* Nothing solid is holding him: that is the point of the case. */
    REQUIRE(!level_is_solid(&patrol,
                            (int)floorf((player.x + PLAYER_W * 0.5f) /
                                        TILE_SIZE),
                            (int)floorf((player.y + (float)PLAYER_H) /
                                        TILE_SIZE)));
    player_update(&player, &patrol, &down, SIM_STEP_DT);
    CHECK(player.crawling);
}

/*
 * A shaft is left by its mouths, which is what the whole rule rests on.
 *
 * [../levels/LEGEND.md](../levels/LEGEND.md) says the crawl is the only move a
 * duct allows from inside it — no jump, no step up, no hole hop — and the route
 * model and the editor's two-mouth check are both built on that sentence. The
 * simulation did not keep it: the rise is resolved with `STANCE_CRAWLING` and
 * trunking is open to that in every direction, so one press of JUMP from the
 * middle of sector 12's sixteen-tile run put Chuck standing on the lid. A
 * shaft could be left anywhere along its length, "a duct with one mouth is not
 * a route" described nothing, and the bet the player takes crawling in — the
 * louvres are opaque both ways, so he cannot see the room he is about to come
 * out in — was a periscope.
 *
 * Both halves are asserted, because a guard that simply broke the jump would
 * pass the first one: he stays down while he is in the shaft, and the same
 * press lifts him the moment he is standing beside it.
 */
static void test_a_shaft_is_left_by_its_mouths(void)
{
    static const char data[] =
        "############\n"
        "#          #\n"
        "#          #\n"
        "#S ======  E\n"
        "############\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 43);
    REQUIRE(level_load_data(&level, "shaft mouth", data, strlen(data), &rng));

    Input jump = {.jump = true, .jump_held = true, .down = true};
    Input held = {.jump_held = true, .down = true};

    Player player;
    player_reset(&player, &level);
    player.crawling = true;
    player.x = 5.0f * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    player.y = 4.0f * TILE_SIZE - (float)PLAYER_CRAWL_H;
    player.vy = 0.0f;
    player.on_ground = true;

    player_update(&player, &level, &jump, SIM_STEP_DT);
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
        player_update(&player, &level, &held, SIM_STEP_DT);
    CHECK(player.crawling);
    CHECK(level_tile(&level,
                     (int)floorf((player.x + PLAYER_W * 0.5f) / TILE_SIZE),
                     (int)floorf((player.y + PLAYER_CRAWL_H * 0.5f) /
                                 TILE_SIZE)) == TILE_VENT);

    /* Out of the mouth and standing on the floor beside it, the same press is
     * an ordinary jump. */
    Input hop = {.jump = true, .jump_held = true};
    Input hold_only = {.jump_held = true};
    player_reset(&player, &level);
    player.on_ground = true;
    float floor_y = player.y;
    player_update(&player, &level, &hop, SIM_STEP_DT);
    CHECK(player.jumped);
    for (int step = 0; step < SIM_STEPS(0.2f); ++step)
        player_update(&player, &level, &hold_only, SIM_STEP_DT);
    CHECK(player.y < floor_y - (float)TILE_SIZE);
}


/*
 * A jump off a ladder has to survive the climb key that is still held.
 *
 * Over a ladder `UP` means climb, which is why the keyboard has a separate jump
 * key at all — so the player who presses it is nearly always already holding
 * up, and the pad has the same shape under A. The grab only asks that the box
 * is over a rung with up or down held, so without a lockout the rung was taken
 * back on the very next frame and the ladder branch wrote the climb speed over
 * the jump: the most natural way to ask for this jump was the one way of asking
 * that did nothing. `PLAYER_LADDER_JUMP_LOCKOUT` is the same answer the stomp
 * bounce already had.
 */
/*
 * Every ladder in the campaign can be got *down*, from the floor above it.
 *
 * Going up and coming down are not the same move and only one of them was
 * covered. Climbing needs `player_over_ladder` — the box already overlaps a
 * rung — while stepping on from the top needs `player_has_ladder_below`,
 * because a player standing on the top edge overlaps no rung at all: his feet
 * are on the tile and the ladder is under them. Miss that and `want_crawl`
 * takes the same press instead, so Down at the top of a shaft crouches rather
 * than descends and the ladder reads as one-way.
 *
 * It is checked against the shipped maps rather than an inline one because
 * what makes it work is a *map* property — the top rung has to be let into the
 * slab the player is standing on — and an authored ladder that stops one tile
 * short of its floor is the way this breaks. Five approaches per rung, because
 * a player walks up to a ladder and stops wherever they stop: the column is
 * decided by the box's centre, so an off-centre stop is the ordinary case and
 * dead centre is the one that never happens.
 *
 * **And it is driven at `SIM_STEP_DT`, which is the whole reason it catches
 * anything.** The first draft of this ran at a comfortable 1/60 and passed on
 * every ladder in the campaign while the game itself was deadlocked on all of
 * them, because the bug is a distance: the grab needs the box to travel far
 * enough into the rung to overlap it, a 1/60 step covers that in one frame and
 * the real 1/240 step does not. A test that invents its own timestep is a test
 * of a game nobody is playing — the same lesson
 * `test_the_jump_apex_does_not_depend_on_the_frame_rate` is here for, learned
 * again from the other side. The rates below bracket the real one so a future
 * change to `SIM_STEPS_PER_SECOND` cannot walk back into it.
 */
static void test_every_ladder_in_the_campaign_can_be_climbed_down(void)
{
    static Level level;
    int checked = 0;

    for (size_t index = 0; index < EMBEDDED_LEVEL_COUNT; ++index)
    {
        Rng rng;
        rng_seed(&rng, 7000 + index);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[index].name,
                                EMBEDDED_LEVELS[index].data,
                                EMBEDDED_LEVELS[index].size, &rng));
        /* Nothing on a wall is a ladder; the climb is four-way movement. */
        if (level.map.mode == LEVEL_MODE_FACADE)
            continue;

        for (int col = 0; col < level.map.width; ++col)
        {
            for (int row = 1; row < level.map.height; ++row)
            {
                if (!level_is_ladder(&level, col, row))
                    continue;
                /* The top rung, and only if there is anywhere to stand on it
                 * from: a rung with masonry over it is not reachable from
                 * above and is not this test's business. */
                if (level_is_ladder(&level, col, row - 1))
                    continue;
                if (level_is_solid(&level, col, row - 1))
                    continue;

                static const float RATES[] = {
                    SIM_STEP_DT, 1.0f / 60.0f, 1.0f / 144.0f, 1.0f / 480.0f};
                const int rate_count =
                    (int)(sizeof(RATES) / sizeof(RATES[0]));

                for (int offset = -6; offset <= 6; offset += 3)
                {
                    for (int rate = 0; rate < rate_count; ++rate)
                    {
                        Player player = {0};
                        player.hp = PLAYER_MAX_HP;
                        player.facing = 1;
                        player.on_ground = true;
                        player.x = (float)col * TILE_SIZE +
                                   ((float)TILE_SIZE - PLAYER_W) * 0.5f +
                                   (float)offset;
                        player.y = (float)row * TILE_SIZE - (float)PLAYER_H;

                        Input down = {0};
                        down.down = true;
                        float from_y = player.y;
                        /* A third of a second, whatever the step is. */
                        int frames = (int)(0.34f / RATES[rate]);
                        for (int frame = 0; frame < frames; ++frame)
                            player_update(&player, &level, &down, RATES[rate]);

                        ++checked;
                        /* On the rungs, and actually lower than he started: a
                         * player who grabbed the ladder and then hung there
                         * would pass a check that only asked about
                         * `on_ladder`, and hanging is exactly what the
                         * deadlock this test was written for looked like. */
                        CHECK(player.on_ladder);
                        CHECK(player.y > from_y + 4.0f);
                        if (!player.on_ladder || player.y <= from_y + 4.0f)
                        {
                            printf("  %s col=%d row=%d offset=%d hz=%.0f: "
                                   "on_ladder=%d crawling=%d dy=%.2f\n",
                                   EMBEDDED_LEVELS[index].name, col, row,
                                   offset, 1.0f / RATES[rate],
                                   player.on_ladder, player.crawling,
                                   player.y - from_y);
                        }
                    }
                }
            }
        }
    }

    /* And the campaign really does have ladders to have checked, so a loader
     * that started handing back empty maps could not pass this quietly. */
    CHECK(checked > 100);
}

static void test_a_jump_off_a_ladder_survives_a_held_climb_key(void)
{
    static const char data[] =
        "########\n"
        "#  H  E#\n"
        "#  H   #\n"
        "#  H   #\n"
        "# SH   #\n"
        "########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 91);
    CHECK(level_load_data(&level, "ladder jump", data, strlen(data), &rng));

    Player player;
    player_reset(&player, &level);
    /* `S` stands beside the run rather than in it — a tile is one thing or the
     * other — so put him in the rung column the way the mount test does. */
    const int ladder_col = 3;
    player.x = (float)ladder_col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    player.y = 4.0f * TILE_SIZE;
    player.on_ground = true;

    const float dt = SIM_STEP_DT;
    Input climbing = {.up = true, .jump_held = true};
    player_update(&player, &level, &climbing, dt);
    CHECK(player.on_ladder);

    /* Climb a little first, so the jump is taken from inside the run rather
     * than off the bottom of it. */
    for (int frame = 0; frame < SIM_STEPS(0.167f); ++frame)
        player_update(&player, &level, &climbing, dt);
    CHECK(player.on_ladder);
    float launched_from = player.y;

    /* The press, with up still held — which is the whole point. */
    Input leap = climbing;
    leap.jump = true;
    player_update(&player, &level, &leap, dt);
    CHECK(player.jumped);
    CHECK(!player.on_ladder);
    CHECK(player.vy < -PLAYER_CLIMB_SPEED);

    /* And the rung does not take him back before the rise has gone anywhere.
     * Climbing would cover a tile in a third of a second; the jump has to beat
     * that over the same stretch or it was overwritten. */
    for (int frame = 0; frame < SIM_STEPS(0.133f); ++frame)
    {
        player_update(&player, &level, &climbing, dt);
        CHECK(!player.on_ladder);
    }
    CHECK(launched_from - player.y > (float)TILE_SIZE);

    /* The lockout is a beat, not a ban: the rungs are still there afterwards,
     * so a jump up a shaft reads as a boost rather than as the ladder dying. */
    for (int frame = 0; frame < SIM_STEPS(0.5f); ++frame)
        player_update(&player, &level, &climbing, dt);
    CHECK(player.on_ladder);
}

static void test_ladder_remembers_climb_direction_for_shooting(void)
{
    static const char data[] =
        "########\n"
        "#  H  E#\n"
        "#  H   #\n"
        "# SH   #\n"
        "########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 73);
    CHECK(level_load_data(&state.level, "ladder shooting", data,
                          strlen(data), &state.rng));

    const int ladder_col = 3;
    const float ladder_x =
        ladder_col * (float)TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    for (int direction = -1; direction <= 1; direction += 2)
    {
        memset(state.bullets, 0, sizeof(state.bullets));
        player_reset(&state.player, &state.level);
        state.player.x = ladder_x;
        state.player.y = direction < 0 ? 3.0f * TILE_SIZE : TILE_SIZE;

        Input climb = {
            .up = direction < 0,
            .down = direction > 0};
        player_update(&state.player, &state.level, &climb, SIM_STEP_DT);
        CHECK(state.player.on_ladder);
        CHECK(state.player.ladder_direction == direction);

        Input shoot = {.shoot = true};
        gameplay_combat_handle_player_action(&state, &campaign, &shoot);
        CHECK(state.bullets[0].active);
        CHECK(state.bullets[0].vx == 0.0f);
        CHECK(state.bullets[0].vy == direction * BULLET_SPEED);
        CHECK(state.player.shot_vertical == direction);
    }

    /* A held horizontal direction remains an explicit request to fire off the
     * side of the ladder rather than reusing the remembered climb direction. */
    memset(state.bullets, 0, sizeof(state.bullets));
    state.player.bullets = 1;
    state.player.on_ladder = true;
    state.player.ladder_direction = -1;
    state.player.facing = 1;
    Input side_shot = {.right = true, .shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &side_shot);
    CHECK(state.bullets[0].active);
    CHECK(state.bullets[0].vx == BULLET_SPEED);
    CHECK(state.bullets[0].vy == 0.0f);
    CHECK(state.player.shot_vertical == 0);

    /* Moving sideways without firing replaces the remembered vertical aim,
     * so a later idle shot follows the last side step. */
    for (int direction = -1; direction <= 1; direction += 2)
    {
        memset(state.bullets, 0, sizeof(state.bullets));
        player_reset(&state.player, &state.level);
        state.player.x = ladder_x;
        state.player.y = 3.0f * TILE_SIZE;

        Input climb = {.up = true};
        player_update(&state.player, &state.level, &climb, SIM_STEP_DT);
        CHECK(state.player.on_ladder);
        CHECK(state.player.ladder_direction == -1);

        Input side_step = {
            .left = direction < 0,
            .right = direction > 0};
        player_update(&state.player, &state.level, &side_step,
                      SIM_STEP_DT);
        CHECK(state.player.on_ladder);
        CHECK(state.player.facing == direction);
        CHECK(state.player.ladder_direction == 0);

        Input idle = {0};
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
        Input idle_shot = {.shoot = true};
        gameplay_combat_handle_player_action(&state, &campaign, &idle_shot);
        CHECK(state.bullets[0].active);
        CHECK(state.bullets[0].vx == direction * BULLET_SPEED);
        CHECK(state.bullets[0].vy == 0.0f);
        CHECK(state.player.shot_vertical == 0);
    }
}

/*
 * Moving across the rungs drives the animation clock; standing on them does not.
 *
 * The renderer poses the climber entirely from `anim_time`, so a clock that only
 * ran on vertical travel left the figure sliding sideways off a ladder frozen in
 * one grip — the pose dragged along rather than anyone shifting their weight
 * across. The idle half of the rule is the reason the clock is gated at all: a
 * player parked on a ladder has to hold his grip instead of shuffling in place.
 */
static void test_ladder_side_step_advances_the_animation_clock(void)
{
    /* The rungs run against the wall, so a side step is stopped while the box
     * still overlaps them — the pose has to hold there rather than the player
     * dropping off the ladder and the check going with him. */
    static const char data[] =
        "########\n"
        "#H    E#\n"
        "#H     #\n"
        "#H S   #\n"
        "########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 91);
    CHECK(level_load_data(&level, "ladder shuffle", data, strlen(data), &rng));

    Player player;
    player_reset(&player, &level);
    player.x = 1.0f * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    player.y = 2.0f * TILE_SIZE;

    const float dt = SIM_STEP_DT;
    Input climb = {.up = true};
    player_update(&player, &level, &climb, dt);
    CHECK(player.on_ladder);

    Input idle = {0};
    player_update(&player, &level, &idle, dt);
    float held = player.anim_time;
    player_update(&player, &level, &idle, dt);
    CHECK(player.on_ladder);
    CHECK(player.anim_time == held);

    /* One step toward the wall, still overlapping the rungs. */
    Input side_step = {.left = true};
    player_update(&player, &level, &side_step, dt);
    CHECK(player.on_ladder);
    CHECK(player.anim_time > held);

    /* A shuffle that a wall has stopped is not motion, so the pose holds. */
    for (int i = 0; i < SIM_STEPS(0.5f); ++i)
        player_update(&player, &level, &side_step, dt);
    CHECK(player.on_ladder);
    CHECK(player.vx == 0.0f);
    float blocked = player.anim_time;
    player_update(&player, &level, &side_step, dt);
    CHECK(player.anim_time == blocked);
}

/*
 * A trigger pulled on a weapon that is loaded but busy has to say something.
 *
 * MAX_ROCKETS is one, and a rocket carried in from the sector below can still
 * be in the air when this sector's own `Z` is picked up — so the tube reads
 * loaded, the HUD shows the rocket, and the press does nothing whatever. The
 * click is the whole difference between "the weapon is busy" and "the pad
 * missed that".
 */
static void test_a_busy_launcher_answers_the_trigger(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.x = 100.0f;
    state.player.y = 96.0f;
    state.player.facing = 1;
    state.player.bazooka_rockets = 1;
    state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;

    Input input = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.rockets[0].active);
    CHECK(state.player.bazooka_rockets == 0);

    /* The rocket is still flying when the next one is picked up. */
    state.player.bazooka_rockets = 1;
    state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
    game_events_clear(&state.events);
    input.shoot = true;
    gameplay_combat_handle_player_action(&state, &campaign, &input);

    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_EMPTY_CLICK));
    /* And the round is still in hand: a dead press must not eat it. */
    CHECK(state.player.bazooka_rockets == 1);
    CHECK(!events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                             SFX_ROCKET_LAUNCH));
}

/*
 * The four numbers that say how long an attack's pose is held, and the two
 * relationships between them that nothing else can hold.
 *
 * Three of the four used to be literals — `0.18f` in each of the three throw
 * branches and `0.12f` for the pistol — sitting beside a
 * `PLAYER_KNIFE_ACTION_TIME` that is also 0.18 and a `ROCKET_ACTION_TIME` that is
 * not, so which of the five were equal on purpose was not written down anywhere.
 * They are named now, and what a name cannot do is keep two of them in step: a
 * float comparison in a `_Static_assert` is a GNU extension and this tree is
 * built `-Wpedantic`, which is why this is a test rather than a line in
 * `game_config.h`. Same division of labour as the note beside `WEAPON_CYCLE`.
 *
 * The second one is the one with teeth. `PLAYER_MUZZLE_FLASH_TIME` is what four
 * places in `render_figures.c` compare `action_timer` against to decide whether to
 * draw the flash at all, and it lives on the side of the SDL line no test can
 * reach. Shorten a shot's pose below it — a plausible thing to do while tuning the
 * feel of the sidearm — and the muzzle flash silently stops existing, with nothing
 * anywhere to say so and no way to see it except by firing.
 */
/*
 * Every underarm throw rises at the cap, which is not what the formula says.
 *
 * `throw_arc_speed` solves the rise from the horizontal speed and then clamps it,
 * and at all three speeds the game ships — 234px/s for the grenade, 225 for the
 * flash charge, 300 for the bolt — the solve comes out at 335, 348 and 261 against
 * a `THROW_ARC_MAX_RISE` of 220. So the derivation is inert and the cap is the
 * whole of the behaviour: all three things leave the hand on exactly the same arc.
 *
 * Pinned because it is the kind of fact that is either true or interesting. If a
 * throw speed is raised past about 2600px/s, or the cap is lifted, the three arcs
 * start to differ and the comment beside the function starts describing something
 * that happens — and whoever did it should find out from a failing test rather than
 * from a bolt that suddenly lobs differently from a grenade.
 */
static void test_every_underarm_throw_rises_at_the_cap(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};

    /* The three of them thrown flat, off a ladder-free floor, facing right. */
    state.player.grenades = 1;
    state.player.facing = 1;
    state.player.active_weapon = PLAYER_WEAPON_GRENADE;
    Input lob = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &lob);
    REQUIRE(state.grenades[0].active);
    CHECK(state.grenades[0].vy == -THROW_ARC_MAX_RISE);

    GameplayState with_flash = {0};
    with_flash.player.flashbangs = 1;
    with_flash.player.facing = 1;
    with_flash.player.active_weapon = PLAYER_WEAPON_FLASH;
    Input toss = {.shoot = true};
    gameplay_combat_handle_player_action(&with_flash, &campaign, &toss);
    REQUIRE(with_flash.flashbangs[0].active);
    CHECK(with_flash.flashbangs[0].vy == -THROW_ARC_MAX_RISE);

    GameplayState with_bolt = {0};
    with_bolt.player.facing = 1;
    with_bolt.player.active_weapon = PLAYER_WEAPON_DECOY;
    Input pitch = {.shoot = true};
    gameplay_combat_handle_player_action(&with_bolt, &campaign, &pitch);
    REQUIRE(with_bolt.decoys[0].active);
    CHECK(with_bolt.decoys[0].vy == -THROW_ARC_MAX_RISE);

    /* And the reason all three agree: every one of them is over the cap before it
     * is clamped. This is the line that fails when a speed is changed enough to
     * bring the solve back inside the clamps. */
    float speeds[] = {GRENADE_THROW_SPEED * 0.9f, FLASH_THROW_SPEED * 0.9f,
                      DECOY_THROW_SPEED};
    for (unsigned i = 0; i < sizeof(speeds) / sizeof(speeds[0]); ++i)
        CHECK(THROW_ARC_STRENGTH * GRAVITY / (2.0f * speeds[i]) >
              THROW_ARC_MAX_RISE);
    CHECK(THROW_ARC_MIN_RISE < THROW_ARC_MAX_RISE);
}

static void test_the_attack_poses_agree_with_what_is_drawn_on_them(void)
{
    /* An underarm throw is held exactly as long as a knife stroke. The three
     * throwables share the pose and the flag it is drawn from, so they share the
     * duration; parting them is a decision, not a tweak. */
    CHECK(PLAYER_THROW_ACTION_TIME == PLAYER_KNIFE_ACTION_TIME);

    /* And every pose that has a muzzle flash on it outlasts the flash. The
     * launcher's own pose is the long one, the sidearm's is the short one, and it
     * is the short one that decides whether the flash is ever reached. */
    CHECK(PLAYER_MUZZLE_FLASH_TIME < PLAYER_SHOT_ACTION_TIME);
    CHECK(PLAYER_MUZZLE_FLASH_TIME < ROCKET_ACTION_TIME);

    /* Every one of them is a positive duration: a nought here is a pose that is
     * over before the frame it was set on is drawn. */
    CHECK(PLAYER_THROW_ACTION_TIME > 0.0f);
    CHECK(PLAYER_SHOT_ACTION_TIME > 0.0f);
    CHECK(PLAYER_KNIFE_ACTION_TIME > 0.0f);
    CHECK(ROCKET_ACTION_TIME > 0.0f);
    CHECK(PLAYER_MUZZLE_FLASH_TIME > 0.0f);

    /* And the numbers the simulation actually writes are those constants, which
     * is the half a header cannot check: a throw, a shot and a stroke all set
     * `action_timer`, and until this was named they set three literals. */
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.grenades = 1;
    state.player.active_weapon = PLAYER_WEAPON_GRENADE;
    state.player.facing = 1;
    Input throw_it = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &throw_it);
    CHECK(state.player.action_timer == PLAYER_THROW_ACTION_TIME);

    GameplayState shooting = {0};
    shooting.player.bullets = 1;
    shooting.player.active_weapon = PLAYER_WEAPON_PISTOL;
    shooting.player.facing = 1;
    Input fire = {.shoot = true};
    gameplay_combat_handle_player_action(&shooting, &campaign, &fire);
    CHECK(shooting.player.action_timer == PLAYER_SHOT_ACTION_TIME);
    CHECK(shooting.player.action_timer > PLAYER_MUZZLE_FLASH_TIME);
}

static void test_every_ladder_throw_follows_the_aim(void)
{
    for (int direction = -1; direction <= 1; direction += 2)
    {
        GameplayState rocket_state = {0};
        CampaignState campaign = {0};
        rocket_state.player.x = 100.0f;
        rocket_state.player.y = 96.0f;
        rocket_state.player.on_ladder = true;
        rocket_state.player.facing = 1;
        rocket_state.player.bazooka_rockets = 1;
        rocket_state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
        Input rocket_input = {
            .up = direction < 0,
            .down = direction > 0,
            .shoot = true};

        gameplay_combat_handle_player_action(&rocket_state, &campaign,
                                             &rocket_input);

        CHECK(rocket_state.rockets[0].active);
        CHECK(rocket_state.rockets[0].vx == 0.0f);
        CHECK(rocket_state.rockets[0].vy == direction * ROCKET_SPEED);
        CHECK(rocket_state.player.shot_vertical == direction);
        CHECK(rocket_state.player.bazooka_firing);

        GameplayState grenade_state = {0};
        grenade_state.player.x = 100.0f;
        grenade_state.player.y = 96.0f;
        grenade_state.player.on_ladder = true;
        grenade_state.player.facing = 1;
        grenade_state.player.grenades = 1;
        grenade_state.player.active_weapon = PLAYER_WEAPON_GRENADE;
        Input grenade_input = {
            .up = direction < 0,
            .down = direction > 0,
            .shoot = true};

        gameplay_combat_handle_player_action(&grenade_state, &campaign,
                                             &grenade_input);

        CHECK(grenade_state.grenade_count == 1);
        CHECK(grenade_state.grenades[0].active);
        CHECK(grenade_state.grenades[0].vx == 0.0f);
        CHECK(grenade_state.grenades[0].vy ==
              direction * GRENADE_THROW_SPEED);
        CHECK(grenade_state.player.shot_vertical == direction);
        CHECK(grenade_state.player.grenade_throwing);

        /*
         * And the other two things that leave the hand, which is why this test
         * is no longer named after the explosives.
         *
         * The rocket and the grenade were the two it walked, and there are four
         * of these blocks in `gameplay_combat_handle_player_action` — the same
         * twelve lines four times over, one per throwable. The flash charge's
         * copy and the bolt's had never been executed by anything: a hand-aimed
         * throw from a ladder is the whole of how either is used up a shaft, and
         * a `vx` left in a copy-pasted arm would drop the charge past Chuck's own
         * feet instead of onto the floor he is aiming at, with the fuse already
         * running.
         */
        GameplayState flash_state = {0};
        flash_state.player.x = 100.0f;
        flash_state.player.y = 96.0f;
        flash_state.player.on_ladder = true;
        flash_state.player.facing = 1;
        flash_state.player.flashbangs = 1;
        flash_state.player.active_weapon = PLAYER_WEAPON_FLASH;
        Input flash_input = {
            .up = direction < 0,
            .down = direction > 0,
            .shoot = true};

        gameplay_combat_handle_player_action(&flash_state, &campaign,
                                            &flash_input);

        CHECK(flash_state.flashbangs[0].active);
        CHECK(flash_state.flashbangs[0].vx == 0.0f);
        CHECK(flash_state.flashbangs[0].vy == direction * FLASH_THROW_SPEED);
        CHECK(flash_state.player.shot_vertical == direction);
        CHECK(flash_state.player.grenade_throwing);
        CHECK(flash_state.player.flashbangs == 0); /* spent on the throw */

        GameplayState decoy_state = {0};
        decoy_state.player.x = 100.0f;
        decoy_state.player.y = 96.0f;
        decoy_state.player.on_ladder = true;
        decoy_state.player.facing = 1;
        decoy_state.player.active_weapon = PLAYER_WEAPON_DECOY;
        Input decoy_input = {
            .up = direction < 0,
            .down = direction > 0,
            .shoot = true};

        gameplay_combat_handle_player_action(&decoy_state, &campaign,
                                            &decoy_input);

        CHECK(decoy_state.decoys[0].active);
        CHECK(decoy_state.decoys[0].vx == 0.0f);
        CHECK(decoy_state.decoys[0].vy == direction * DECOY_THROW_SPEED);
        CHECK(decoy_state.player.shot_vertical == direction);
        CHECK(decoy_state.player.grenade_throwing);
        /* Nothing is spent, only the clock, and the bolts stay in the hand —
         * the pair of throws is the whole mechanic. */
        CHECK(decoy_state.player.decoy_cooldown == DECOY_COOLDOWN);
        CHECK(decoy_state.player.active_weapon == PLAYER_WEAPON_DECOY);
    }
}

static void test_vertical_rocket_hits_targets(void)
{
    for (int direction = -1; direction <= 1; direction += 2)
    {
        GameplayState state = {0};
        CampaignState campaign = {0};
        state.level.map.width = 20;
        state.level.map.height = 20;
        state.player.x = 100.0f;
        state.player.y = 160.0f;
        state.player.on_ladder = true;
        state.player.bazooka_rockets = 1;
        state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
        state.enemy_count = 1;
        state.enemies[0] = (Enemy){
            .x = state.player.x,
            .y = direction < 0 ? 50.0f : 270.0f,
            .dir = -1,
            .hp = ENEMY_HP};
        Input input = {
            .up = direction < 0,
            .down = direction > 0,
            .shoot = true};

        gameplay_combat_handle_player_action(&state, &campaign, &input);
        gameplay_combat_update_player_bullets(&state, &campaign, 0.20f);

        CHECK(!state.rockets[0].active);
        CHECK(state.enemies[0].dead);
        CHECK(campaign.score == 150);
        CHECK(!state.player.dying);
        CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                                SFX_EXPLOSION));
    }
}

/*
 * A rocket bursts on the first thing it meets, and three of the four kinds of
 * thing it can meet had never been put in front of one.
 *
 * `gameplay_combat_update_player_bullets` tests the rocket's swept box against
 * masonry, then against the crates, then the gas canisters, then the dogs —
 * thirty-six lines in three near-identical loops, and only the masonry arm and
 * the guard had ever been executed. Every one of the three is an ordinary shot:
 * a crate is the thing most likely to be standing between Chuck and what he is
 * aiming at, a canister is a bomb somebody else built for him, and a dog is the
 * fastest thing on the floor.
 *
 * What a copy-paste costs here is not a crash. The loops differ only in which
 * array and which pair of dimensions they name, so one of them walking the wrong
 * count — or comparing against `CRATE_W` where it means `DOG_W` — lets the round
 * pass straight through and detonate somewhere behind the target, on a weapon the
 * sector hands the player exactly one of.
 *
 * Driven at `SIM_STEP_DT` rather than in one long step, because the sweep is a
 * per-step box and a single stride of a fifth of a second is not the shape the
 * game ever asks it for.
 */
/*
 * The shot line is chest high, and two things standing on the floor are under it.
 *
 * `test_gas_canister_requires_crawling_shot` pins one half of this as the
 * canister's own mechanic: a standing round passes over a cylinder 16px tall and
 * the crawl is what puts one into it. What that test cannot say is that the rule
 * is not about canisters at all — it is about the height the muzzle sits at, and
 * `DOG_H` is the same sixteen. So a dog on the same floor is under the line too,
 * and the sidearm cannot answer one from a standing position: the round goes over
 * its shoulders and on into the far wall.
 *
 * Written down because the margin is *0.8 pixels* and neither half of it was
 * stated anywhere. `PLAYER_H * 0.35` puts the round's underside at 15.2 and a
 * 16-tall box standing on the floor starts at 16, so the whole of the canister
 * mechanic — and the whole of what a dog is answered with — turns on a number
 * nobody would think to be careful with. Lowered by one pixel, the canister stops
 * needing the crawl; raised, nothing changes for the canister and nothing changes
 * for the dog either. It is the kind of coincidence that stops being one the moment
 * somebody writes it down.
 *
 * A dog has four other answers and that is why this is a rule rather than a
 * defect: the blade (which reaches it, unlike the takedown behind a man), the same
 * round from a crawl, the same round up or down a ladder, and anything that goes
 * off. What it is not is the thing a player reaches for without thinking.
 */
static void test_the_shot_line_is_chest_high(void)
{
    /* The two boxes are the same height, which is the whole of why the two
     * behaviours are the same behaviour. */
    CHECK(DOG_H == GAS_CANISTER_H);

    static const char data[] =
        "####################\n"
        "#                  #\n"
        "#S   W            E#\n"
        "####################\n";
    static GameplayState state;
    CampaignState campaign = {0};

    /* Standing: over its shoulders and away. */
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 8811);
    REQUIRE(level_load_data(&state.level, "shot line", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);
    REQUIRE(state.dog_count == 1);
    /* The handler out of the line, so the only thing in front of the muzzle is
     * the animal. */
    state.enemies[0].x = 5000.0f;
    Dog *dog = &state.dogs[0];
    dog->on_ground = true;
    dog->x = state.player.x + 4.0f * TILE_SIZE;
    dog->y = state.player.y + (float)PLAYER_H - (float)DOG_H;

    state.player.facing = 1;
    state.player.on_ground = true;
    state.player.bullets = MAX_AMMO;
    state.player.active_weapon = PLAYER_WEAPON_PISTOL;
    Input standing = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &standing);
    Bullet *round = &state.bullets[0];
    REQUIRE(round->active);
    CHECK(round->y + BULLET_H <= dog->y); /* the same margin the canister has */

    for (int step = 0; step < SIM_STEPS(2.0f) && round->active; ++step)
        gameplay_combat_update_player_bullets(&state, &campaign, SIM_STEP_DT);
    CHECK(!dog->dead);
    CHECK(dog->hp == DOG_HP);
    /* And it went past rather than stopping short of it: what saved the animal is
     * the height, not the round running out of map before it arrived. */
    CHECK(round->x > dog->x + DOG_W);

    /* Crawling: the same round, into it. */
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 8812);
    REQUIRE(level_load_data(&state.level, "shot line", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);
    state.enemies[0].x = 5000.0f;
    dog = &state.dogs[0];
    dog->on_ground = true;
    dog->x = state.player.x + 4.0f * TILE_SIZE;
    dog->y = state.player.y + (float)PLAYER_H - (float)DOG_H;

    state.player.crawling = true;
    state.player.y += (float)(PLAYER_H - PLAYER_CRAWL_H);
    state.player.facing = 1;
    state.player.on_ground = true;
    state.player.bullets = MAX_AMMO;
    state.player.active_weapon = PLAYER_WEAPON_PISTOL;
    Input crawling = {.shoot = true, .down = true};
    gameplay_combat_handle_player_action(&state, &campaign, &crawling);
    Bullet *low = &state.bullets[0];
    REQUIRE(low->active);
    CHECK(low->y + BULLET_H > dog->y);

    for (int step = 0; step < SIM_STEPS(2.0f) && low->active; ++step)
        gameplay_combat_update_player_bullets(&state, &campaign, SIM_STEP_DT);
    CHECK(state.dogs[0].dead);

    /* And the blade reaches it standing, which is the answer the crawl is not.
     * A dog is never taken from *behind* — that is the rule about the animal —
     * but the stroke itself lands on one like anything else. */
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 8813);
    REQUIRE(level_load_data(&state.level, "shot line", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);
    state.enemies[0].x = 5000.0f;
    dog = &state.dogs[0];
    dog->on_ground = true;
    dog->x = state.player.x + PLAYER_KNIFE_RANGE - 2.0f;
    dog->y = state.player.y + (float)PLAYER_H - (float)DOG_H;
    state.player.facing = 1;
    state.player.on_ground = true;
    state.player.bullets = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    Input blade = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &blade);
    CHECK(state.dogs[0].dead);
}

static void test_a_rocket_bursts_on_what_it_meets(void)
{
    /* Room enough for a rocket to be in flight, with a floor for what it is
     * aimed at to stand on. */
    static const char data[] =
        "####################\n"
        "#                  #\n"
        "#S                E#\n"
        "####################\n";

    /* The crate. */
    {
        static GameplayState state;
        CampaignState campaign = {0};
        memset(&state, 0, sizeof(state));
        rng_seed(&state.rng, 4041);
        REQUIRE(level_load_data(&state.level, "rocket crate", data,
                                strlen(data), &state.rng));
        player_reset(&state.player, &state.level);
        state.level.runtime.crate_count = 1;
        Crate *crate = &state.level.runtime.crates[0];
        crate->active = true;
        crate->on_ground = true;
        crate->x = state.player.x + 6.0f * TILE_SIZE;
        crate->y = state.player.y + (float)PLAYER_H - (float)CRATE_H;

        state.player.facing = 1;
        state.player.bazooka_rockets = 1;
        state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
        Input input = {.shoot = true};
        gameplay_combat_handle_player_action(&state, &campaign, &input);
        REQUIRE(state.rockets[0].active);

        for (int step = 0; step < SIM_STEPS(2.0f) && state.rockets[0].active;
             ++step)
            gameplay_combat_update_player_bullets(&state, &campaign,
                                                  SIM_STEP_DT);

        CHECK(!state.rockets[0].active);
        CHECK(!crate->active); /* the blast breaks what stopped the round */
        CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                                SFX_EXPLOSION));
    }

    /*
     * The gas canister, which is a second explosion rather than a broken prop —
     * and which has to be shot from the crawl, because a standing round passes
     * over a thing 16px tall standing on the floor. That is the canister's whole
     * mechanic (`test_gas_canister_requires_crawling_shot`) and the launcher
     * obeys the same geometry as the sidearm.
     */
    {
        static GameplayState state;
        CampaignState campaign = {0};
        memset(&state, 0, sizeof(state));
        rng_seed(&state.rng, 4042);
        REQUIRE(level_load_data(&state.level, "rocket canister", data,
                                strlen(data), &state.rng));
        player_reset(&state.player, &state.level);
        state.level.runtime.gas_canister_count = 1;
        GasCanister *canister = &state.level.runtime.gas_canisters[0];
        canister->active = true;
        canister->x = state.player.x + 6.0f * TILE_SIZE;
        canister->y = state.player.y + (float)PLAYER_H - (float)GAS_CANISTER_H;

        state.player.crawling = true;
        state.player.y += (float)PLAYER_H - (float)PLAYER_CRAWL_H;
        state.player.facing = 1;
        state.player.bazooka_rockets = 1;
        state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
        Input input = {.shoot = true, .down = true};
        gameplay_combat_handle_player_action(&state, &campaign, &input);
        REQUIRE(state.rockets[0].active);

        for (int step = 0; step < SIM_STEPS(2.0f) && state.rockets[0].active;
             ++step)
            gameplay_combat_update_player_bullets(&state, &campaign,
                                                  SIM_STEP_DT);

        CHECK(!state.rockets[0].active);
        CHECK(!canister->active);
        CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                                SFX_EXPLOSION));
    }

    /*
     * The animal, from the crawl for the same reason: a dog is the same 16px
     * tall as the canister and stands on the same floor.
     */
    {
        static GameplayState state;
        CampaignState campaign = {0};
        memset(&state, 0, sizeof(state));
        rng_seed(&state.rng, 4043);
        REQUIRE(level_load_data(&state.level, "rocket dog", data,
                                strlen(data), &state.rng));
        player_reset(&state.player, &state.level);
        state.dog_count = 1;
        Dog *dog = &state.dogs[0];
        dog->hp = DOG_HP;
        dog->on_ground = true;
        dog->dir = -1;
        dog->x = state.player.x + 6.0f * TILE_SIZE;
        dog->y = state.player.y + (float)PLAYER_H - (float)DOG_H;

        state.player.crawling = true;
        state.player.y += (float)PLAYER_H - (float)PLAYER_CRAWL_H;
        state.player.facing = 1;
        state.player.bazooka_rockets = 1;
        state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
        Input input = {.shoot = true, .down = true};
        gameplay_combat_handle_player_action(&state, &campaign, &input);
        REQUIRE(state.rockets[0].active);

        for (int step = 0; step < SIM_STEPS(2.0f) && state.rockets[0].active;
             ++step)
            gameplay_combat_update_player_bullets(&state, &campaign,
                                                  SIM_STEP_DT);

        CHECK(!state.rockets[0].active);
        CHECK(state.dogs[0].dead);
        CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                                SFX_EXPLOSION));
    }
}

/*
 * A crate stops a guard's round, and a round that leaves the map stops being a
 * round. Two housekeeping arms, one on each side of the firing line, and neither
 * had ever been executed.
 *
 * The crate is the half that matters to play: `test_a_crate_is_a_floor_a_wall_and_a_brake`
 * pins what a crate is to a man walking into it and
 * `test_a_blast_breaks_the_crates_it_reaches` pins what a blast does to it, and
 * between them nobody had ever put one in front of a bullet coming the *other*
 * way. Cover is most of the reason a player shoves one anywhere, and the arm that
 * provides it was compiled and never run — while the identical arm in the player's
 * own bullet loop was covered, which is how a twin hides.
 *
 * The map edge is the quieter half. Every test map in the suite is walled, so a
 * round has always found masonry before it found the boundary; the four
 * comparisons that catch the one that does not were dead in both loops. What they
 * prevent is a projectile slot occupied forever by something off the map — a
 * `MAX_BULLETS` of 8 that quietly becomes 7 for the rest of the sector, on the
 * one map shape where it can happen: an open window on a facade, or any floor
 * whose exit tile is at the edge of the grid.
 */
static void test_a_crate_is_cover_and_a_lost_round_is_cleaned_up(void)
{
    /* Bottom floor open at both ends, so a round can be fired out of the grid
     * rather than into a wall. */
    static const char data[] =
        "####################\n"
        "#                  #\n"
        "#S                E#\n"
        "####################\n";
    static GameplayState state;
    CampaignState campaign = {0};
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 5150);
    REQUIRE(level_load_data(&state.level, "cover", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);

    /* A crate on the floor between the two of them. */
    state.level.runtime.crate_count = 1;
    Crate *crate = &state.level.runtime.crates[0];
    crate->active = true;
    crate->on_ground = true;
    crate->x = state.player.x + 4.0f * TILE_SIZE;
    crate->y = state.player.y + (float)PLAYER_H - (float)CRATE_H;

    /* The guard's round, coming from beyond the crate at the height a standing
     * man fires at. Stepped a frame at a time rather than teleported past,
     * because the guard's round is deliberately not swept. */
    Bullet *round = &state.enemy_bullets[0];
    round->active = true;
    round->vx = -ENEMY_BULLET_SPEED;
    round->vy = 0.0f;
    round->x = crate->x + 3.0f * TILE_SIZE;
    round->y = crate->y + 6.0f;
    float player_x = state.player.x;
    int hp = state.player.hp;

    for (int step = 0; step < SIM_STEPS(1.0f) && round->active; ++step)
        gameplay_combat_update_enemy_bullets(&state, &campaign, SIM_STEP_DT);

    CHECK(!round->active);
    /* It stopped at the crate rather than at Chuck, and it stopped the crate
     * being anything else: a round is not a blast, so the crate is still there
     * to hide behind. */
    CHECK(round->x > crate->x);
    CHECK(round->x > player_x + (float)PLAYER_W);
    CHECK(crate->active);
    CHECK(state.player.hp == hp);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_BULLET_IMPACT));

    /* And the same round fired the other way, out of the grid entirely, with
     * nothing in front of it and no masonry to reach: the slot has to come
     * back. */
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 5151);
    REQUIRE(level_load_data(&state.level, "cover", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);
    float world_w = (float)state.level.map.width * TILE_SIZE;

    Bullet *theirs = &state.enemy_bullets[0];
    theirs->active = true;
    theirs->vx = ENEMY_BULLET_SPEED;
    theirs->vy = 0.0f;
    theirs->x = world_w - (float)BULLET_W;
    theirs->y = state.player.y + 8.0f;

    Bullet *ours = &state.bullets[0];
    ours->active = true;
    ours->vx = -BULLET_SPEED;
    ours->vy = 0.0f;
    ours->x = 0.0f;
    ours->y = state.player.y + 8.0f;

    for (int step = 0; step < SIM_STEPS(1.0f) &&
                       (theirs->active || ours->active);
         ++step)
    {
        gameplay_combat_update_player_bullets(&state, &campaign, SIM_STEP_DT);
        gameplay_combat_update_enemy_bullets(&state, &campaign, SIM_STEP_DT);
    }
    CHECK(!theirs->active);
    CHECK(!ours->active);
}

static void test_level_reveal_finishes(void)
{
    static const char data[] =
        "#####\n"
        "#S E#\n"
        "#####\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 99);
    CHECK(level_load_data(&level, "reveal", data, strlen(data), &rng));

    level_reveal_init(&level);
    CHECK(!level.reveal.done);
    CHECK(level_reveal_step(&level, 10.0f));
    CHECK(level.reveal.done);
    for (int row = 0; row < level.map.height; ++row)
        for (int col = 0; col < level.map.width; ++col)
            CHECK(level.reveal.tiles_visible[row][col]);
}

static void test_event_buffer_reports_overflow(void)
{
    GameEventBuffer events = {0};
    for (int i = 0; i < MAX_GAME_EVENTS; ++i)
        CHECK(game_events_sound(&events, SFX_STEP_A));
    CHECK(!game_events_sound(&events, SFX_STEP_B));
    CHECK(events.count == MAX_GAME_EVENTS);
    CHECK(events.overflowed);
}

static void test_terminal_unlocks_deterministically(void)
{
    static const char data[] =
        "########\n"
        "#S T  E#\n"
        "########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    Input input = {0};
    rng_seed(&state.rng, 55);
    CHECK(level_load_data(&state.level, "terminal", data, strlen(data),
                          &state.rng));
    int terminal_index = state.level.runtime.active_terminal_index;
    CHECK(terminal_index >= 0);
    const Terminal *terminal = &state.level.map.terminals[terminal_index];
    state.player.x = terminal->col * TILE_SIZE +
                     (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = (terminal->row + 1) * TILE_SIZE - PLAYER_H;
    state.player.on_ground = true;
    input.interact = true;

    gameplay_prepare_terminal(&state, &input, 0.0f);
    CHECK(state.terminal_hacking);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_TERMINAL_ALARM));
    CHECK(gameplay_advance_terminal(&state, &campaign,
                                    TERMINAL_HACK_TIME));
    CHECK(state.level.runtime.exit_unlocked);
    CHECK(state.level.runtime.terminal_hacked);
    CHECK(campaign.score == 250);
}

static void stand_at_active_terminal(GameplayState *state)
{
    int index = state->level.runtime.active_terminal_index;
    CHECK(index >= 0);
    const Terminal *terminal = &state->level.map.terminals[index];
    state->player.x = terminal->col * TILE_SIZE +
                      (TILE_SIZE - PLAYER_W) * 0.5f;
    state->player.y = (terminal->row + 1) * TILE_SIZE - PLAYER_H;
    state->player.on_ground = true;
}

/*
 * The doors answer the hack, not the silence before it.
 *
 * `terminal_reinforcements_pending` used to be queued off the frame the hack
 * *raised* the alarm, which made the cost conditional on the floor having been
 * quiet: a player who had already been spotted, with the alarm ringing before
 * they ever reached the console, hacked it and nobody came out of the doors at
 * all. That is the incentive backwards — the loud approach was cheaper than the
 * careful one on exactly the sectors that have both a terminal and a door pair
 * to send anybody out of. This walks the same terminal twice, once quiet and
 * once under an alarm somebody else put up, and requires the same answer.
 */
static void test_the_terminal_calls_its_reinforcements_under_an_alarm(void)
{
    static const char data[] =
        "#############\n"
        "#S T A D D E#\n"
        "#############\n";

    /* A quiet floor: the hack puts the alarm up itself and calls the doors. */
    GameplayState quiet = {0};
    Input input = {0};
    rng_seed(&quiet.rng, 4242);
    CHECK(level_load_data(&quiet.level, "terminal-quiet", data, strlen(data),
                          &quiet.rng));
    CHECK(quiet.level.map.door_count == 2);
    CHECK(quiet.level.map.alarm_switch_count == 1);
    stand_at_active_terminal(&quiet);
    input.interact = true;
    CHECK(!gameplay_alarm_active(&quiet));
    gameplay_prepare_terminal(&quiet, &input, 0.0f);
    CHECK(quiet.terminal_hacking);
    CHECK(gameplay_alarm_active(&quiet));
    CHECK(quiet.terminal_reinforcements_pending >=
          TERMINAL_REINFORCEMENT_MIN_COUNT);

    /* And holding the button down is one hack, not one a frame. */
    int queued = quiet.terminal_reinforcements_pending;
    for (int frame = 0; frame < SIM_STEPS(0.133f); ++frame)
        gameplay_prepare_terminal(&quiet, &input, SIM_STEP_DT);
    CHECK(quiet.terminal_reinforcements_pending == queued);

    /* A loud one: a guard has already reached a switch, so the alarm is up
     * before Chuck touches the console. The hack has to cost the same. This is
     * the case that used to come back nought. */
    GameplayState loud = {0};
    rng_seed(&loud.rng, 4242);
    CHECK(level_load_data(&loud.level, "terminal-loud", data, strlen(data),
                          &loud.rng));
    stand_at_active_terminal(&loud);
    const AlarmSwitch *pulled = &loud.level.map.alarm_switches[0];
    gameplay_trigger_alarm(&loud, (pulled->col + 0.5f) * TILE_SIZE,
                           (pulled->row + 0.5f) * TILE_SIZE, 0);
    CHECK(gameplay_alarm_active(&loud));
    gameplay_prepare_terminal(&loud, &input, 0.0f);
    CHECK(loud.terminal_hacking);
    CHECK(loud.terminal_reinforcements_pending >=
          TERMINAL_REINFORCEMENT_MIN_COUNT);

    /* Either way the floor is now converging on the console rather than on
     * wherever the player was last seen, which is the half of this that was
     * always right and must stay so. */
    const Terminal *console =
        &loud.level.map.terminals[loud.level.runtime.active_terminal_index];
    CHECK(fabsf(loud.alarm_target_x -
                (console->col + 0.5f) * TILE_SIZE) < 0.001f);
}
/*
 * And the men a console calls actually come out of a door.
 *
 * The test above pins the *queue*: a hack raises the alarm and books
 * `terminal_reinforcements_pending` arrivals. Nothing then called
 * `gameplay_ai_update_spawns`, so the twenty-five lines that turn that number
 * into men walking onto the floor had never run — which is the whole feature,
 * and the reason `test_every_sector_can_seat_the_reinforcements_it_can_call`
 * exists two hundred lines further up. A ceiling was being derived and checked
 * against a campaign for a queue nothing in the suite had ever drained.
 */
static void test_the_men_a_console_calls_come_out_of_a_door(void)
{
    static const char data[] =
        "#############\n"
        "#S T A D D E#\n"
        "#############\n";
    static GameplayState state;
    Input input = {0};
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 4242);
    REQUIRE(level_load_data(&state.level, "reinforcements", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.map.door_count == 2);
    gameplay_ai_spawn_level_entities(&state);
    int before = state.enemy_count;

    stand_at_active_terminal(&state);
    input.interact = true;
    gameplay_prepare_terminal(&state, &input, 0.0f);
    REQUIRE(state.terminal_hacking);
    REQUIRE(gameplay_alarm_active(&state));
    int booked = state.terminal_reinforcements_pending;
    REQUIRE(booked >= TERMINAL_REINFORCEMENT_MIN_COUNT);

    /* Long enough for the worst gap the table allows twice over, and still well
     * inside `ALARM_CALM_TIME` — a queue that outlived the alarm would be a
     * queue that silently cancels itself. */
    for (int step = 0; step < SIM_STEPS(TERMINAL_REINFORCEMENT_FIRST_MAX +
                                        2.0f * TERMINAL_REINFORCEMENT_GAP_MAX);
         ++step)
    {
        state.events.count = 0;
        gameplay_ai_update_spawns(&state, SIM_STEP_DT);
    }

    CHECK(state.terminal_reinforcements_pending == 0);
    CHECK(state.enemy_count == before + booked);
    /* Every arrival stands on a door rather than in the middle of the room, and
     * is a live man rather than a slot that was merely counted. */
    for (int e = before; e < state.enemy_count; ++e)
    {
        const Enemy *arrival = &state.enemies[e];
        CHECK(!arrival->dead);
        bool on_a_door = false;
        for (int d = 0; d < state.level.map.door_count; ++d)
            if (fabsf((state.level.map.doors[d].col + 0.5f) * TILE_SIZE -
                      (arrival->x + ENEMY_W * 0.5f)) < TILE_SIZE)
                on_a_door = true;
        CHECK(on_a_door);
    }
}


static void test_alarm_switch_parsing_and_quiet_timeout(void)
{
    static const char data[] =
        "#########\n"
        "#S A M E#\n"
        "#########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 808);
    CHECK(level_load_data(&state.level, "alarm", data, strlen(data),
                          &state.rng));
    CHECK(state.level.map.alarm_switch_count == 1);

    const AlarmSwitch *alarm_switch = &state.level.map.alarm_switches[0];
    float switch_x = (alarm_switch->col + 0.5f) * TILE_SIZE;
    float switch_y = (alarm_switch->row + 0.5f) * TILE_SIZE;
    gameplay_trigger_alarm(&state, switch_x, switch_y, 0);
    CHECK(gameplay_alarm_active(&state));
    CHECK(state.active_alarm_switch == 0);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_TERMINAL_ALARM));
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_CARD_SCAN));

    game_events_clear(&state.events);
    state.alarm_siren_timer = 0.01f;
    gameplay_update_alarm(&state, 0.02f);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_TERMINAL_ALARM));

    gameplay_update_alarm(&state, ALARM_CALM_TIME * 0.5f);
    CHECK(gameplay_alarm_active(&state));
    state.player.x = 5.0f * TILE_SIZE;
    state.player.y = 2.0f * TILE_SIZE - PLAYER_H;
    gameplay_refresh_alarm_from_player(&state);
    CHECK(fabsf(state.terminal_alarm_timer - ALARM_CALM_TIME) < 0.001f);
    CHECK(fabsf(state.alarm_target_x -
                (state.player.x + PLAYER_W * 0.5f)) < 0.001f);

    gameplay_update_alarm(&state, ALARM_CALM_TIME);
    CHECK(!gameplay_alarm_active(&state));
    CHECK(state.active_alarm_switch == -1);
}

static void test_guards_choose_attack_or_alarm_and_operate_switch(void)
{
    static const char data[] =
        "##########\n"
        "#S M A  E#\n"
        "##########\n";
    Level level;
    Rng level_rng;
    rng_seed(&level_rng, 909);
    CHECK(level_load_data(&level, "guard alarm", data, strlen(data),
                          &level_rng));

    int alarm_choices = 0;
    int attack_choices = 0;
    for (uint64_t seed = 1; seed <= 64; ++seed)
    {
        GameplayState state = {0};
        state.level = level;
        rng_seed(&state.rng, seed);
        state.player.x = state.level.map.start_x;
        state.player.y = state.level.map.start_y;
        state.enemy_count = 1;
        enemy_init(&state.enemies[0],
                   state.level.map.enemy_spawns[0].x,
                   state.level.map.enemy_spawns[0].y, ENEMY_KIND_GUARD, &state.rng);
        state.enemies[0].dir = -1;
        state.enemies[0].on_ground = true;
        state.enemies[0].shoot_cooldown = 10.0f;

        gameplay_ai_update_combat(&state, SIM_STEP_DT);
        CHECK(state.enemies[0].encounter_decided);
        if (state.enemies[0].raising_alarm)
            alarm_choices++;
        else
            attack_choices++;
    }
    CHECK(alarm_choices > 0);
    CHECK(attack_choices > 0);

    GameplayState state = {0};
    state.level = level;
    rng_seed(&state.rng, 17);
    state.enemy_count = 1;
    Enemy *enemy = &state.enemies[0];
    enemy_init(enemy, 0.0f, 0.0f, ENEMY_KIND_GUARD, &state.rng);
    const AlarmSwitch *alarm_switch = &state.level.map.alarm_switches[0];
    float switch_x = (alarm_switch->col + 0.5f) * TILE_SIZE;
    float switch_y = (alarm_switch->row + 0.5f) * TILE_SIZE;
    enemy->x = switch_x - ENEMY_W * 0.5f;
    enemy->y = switch_y - ENEMY_H * 0.5f;
    enemy->on_ground = true;
    enemy->raising_alarm = true;
    enemy->alarm_switch_index = 0;

    gameplay_ai_update_movement(&state, ALARM_SWITCH_USE_TIME);
    CHECK(gameplay_alarm_active(&state));
    CHECK(state.active_alarm_switch == 0);
    CHECK(!enemy->raising_alarm);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_TERMINAL_ALARM));
}

static void test_alarm_increases_guard_aggression_and_search(void)
{
    static const char data[] =
        "################\n"
        "#S M      A   E#\n"
        "################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 5150);
    CHECK(level_load_data(&state.level, "alarm response", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *enemy = &state.enemies[0];
    enemy->dir = -1;
    enemy->on_ground = true;
    enemy->shoot_cooldown = 3.0f;
    enemy->aim_timer = ENEMY_AIM_TIME;
    state.player.x = state.level.map.start_x;
    state.player.y = state.level.map.start_y;

    const AlarmSwitch *alarm_switch = &state.level.map.alarm_switches[0];
    gameplay_trigger_alarm(&state,
                           (alarm_switch->col + 0.5f) * TILE_SIZE,
                           (alarm_switch->row + 0.5f) * TILE_SIZE, 0);
    CHECK(fabsf(enemy->shoot_cooldown -
                ENEMY_ALARM_INITIAL_SHOT_DELAY) < 0.001f);
    CHECK(fabsf(enemy->aim_timer -
                ENEMY_AIM_TIME * ENEMY_ALARM_AIM_MULTIPLIER) < 0.001f);

    enemy->aim_timer = 0.0f;
    enemy->shoot_cooldown = 0.0f;
    gameplay_ai_update_combat(&state, 0.0f);
    CHECK(fabsf(enemy->aim_timer -
                ENEMY_AIM_TIME * ENEMY_ALARM_AIM_MULTIPLIER) < 0.001f);

    enemy->aim_timer = 0.0f;
    enemy->anim_time = 1.0f;
    state.player.x = 1000.0f;
    float last_x = enemy->x + ENEMY_W * 0.5f;
    float last_y = enemy->y + ENEMY_H * 0.5f;
    state.alarm_target_x = last_x;
    state.alarm_target_y = last_y;
    gameplay_ai_update_movement(&state, 0.1f);
    CHECK(fabsf(enemy->vx) > ENEMY_WALK_SPEED);
    CHECK(fabsf(enemy->pursuit_target_x - last_x) > 1.0f);
}

static void test_door_interaction_reports_range_and_teleports(void)
{
    static const char data[] =
        "########\n"
        "#SD D E#\n"
        "########\n";
    GameplayState state = {0};
    Input input = {0};
    rng_seed(&state.rng, 77);
    CHECK(level_load_data(&state.level, "doors", data, strlen(data),
                          &state.rng));
    CHECK(state.level.map.door_count == 2);

    const Door *entrance = &state.level.map.doors[0];
    const Door *destination = &state.level.map.doors[1];
    state.player.x = entrance->col * TILE_SIZE +
                     (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = (entrance->row + 1) * TILE_SIZE - PLAYER_H;
    state.player.on_ground = true;

    CHECK(gameplay_player_door_index(&state) == 0);
    input.use_door = true;
    gameplay_use_door(&state, &input);

    CHECK(fabsf(state.player.x -
                (destination->col * TILE_SIZE +
                 (TILE_SIZE - PLAYER_W) * 0.5f)) < 0.01f);
    CHECK(fabsf(state.player.y -
                ((destination->row + 1) * TILE_SIZE - PLAYER_H)) < 0.01f);
    CHECK(state.teleport_cooldown == TELEPORT_COOLDOWN);
    CHECK(!input.use_door);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND, SFX_DOOR));
    CHECK(gameplay_player_door_index(&state) == -1);
}

static void test_sublevel_doors_are_not_paired_teleports(void)
{
    static const char entrance_data[] =
        "#######\n"
        "#SU  E#\n"
        "#######\n";
    static const char return_data[] =
        "#####\n"
        "#SR #\n"
        "#####\n";
    GameplayState state = {0};
    Input input = {0};
    rng_seed(&state.rng, 78);
    CHECK(level_load_data(&state.level, "sublevel entrance", entrance_data,
                          strlen(entrance_data), &state.rng));
    CHECK(state.level.map.door_count == 0);
    state.player.x = state.level.map.sublevel_entrance_col * TILE_SIZE +
                     (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = (state.level.map.sublevel_entrance_row + 1) * TILE_SIZE -
                     PLAYER_H;
    state.player.on_ground = true;
    CHECK(gameplay_player_sublevel_door_action(&state) ==
          SUBLEVEL_DOOR_ENTER);
    input.use_door = true;
    CHECK(gameplay_use_sublevel_door(&state, &input) ==
          SUBLEVEL_DOOR_ENTER);
    CHECK(!input.use_door);

    rng_seed(&state.rng, 79);
    CHECK(level_load_data(&state.level, "sublevel return", return_data,
                          strlen(return_data), &state.rng));
    CHECK(state.level.map.door_count == 0);
    state.player.x = state.level.map.sublevel_return_col * TILE_SIZE +
                     (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = (state.level.map.sublevel_return_row + 1) * TILE_SIZE -
                     PLAYER_H;
    state.player.on_ground = true;
    state.teleport_cooldown = 0.0f;
    CHECK(gameplay_player_sublevel_door_action(&state) ==
          SUBLEVEL_DOOR_RETURN);
    input.use_door = true;
    CHECK(gameplay_use_sublevel_door(&state, &input) ==
          SUBLEVEL_DOOR_RETURN);
    CHECK(!input.use_door);
}

static void test_key_cards_keep_scoring_and_unlock_rules(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.x = 0.0f;
    state.player.y = 0.0f;
    state.level.runtime.item_count = 2;
    state.level.runtime.card_count = 2;
    state.level.runtime.active_card_index = 1;
    state.level.runtime.items[0] =
        (Item){.x = 8.0f, .y = 8.0f, .type = ITEM_CARD};
    state.level.runtime.items[1] =
        (Item){.x = 128.0f, .y = 8.0f, .type = ITEM_CARD};

    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(campaign.score == 100);
    CHECK(!state.level.runtime.exit_unlocked);
    CHECK(state.events.count == 1);
    CHECK(state.events.items[0].type == GAME_EVENT_SOUND);
    CHECK(state.events.items[0].data.sound.effect == SFX_CARD_WRONG);

    state.player.x = 120.0f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(campaign.score == 200);
    CHECK(state.level.runtime.exit_unlocked);
}

/*
 * The live card is never the silent one.
 *
 * `gameplay_unlock_exit` says nothing when there is no door left to open — an
 * interior whose stair core is welded and whose route on is the window, or an
 * exit a finished hack already opened. Taken as the card's only voice, that
 * made the *right* card the one pickup in the game that answered with nothing
 * at all, in exactly the sectors where the strip reads BLOCKED and cannot
 * report it either, while a decoy went on buzzing.
 *
 * This used to end "no shipped map puts a `C` in a window sector, which is the
 * only reason nobody has heard it". Sector 14 puts two there, behind a `Y` and
 * a welded `E`, so the branch was live all along — and the decoy half of the
 * same sentence was the bug nobody then looked for. See
 * `test_no_card_is_wrong_where_no_card_can_be`.
 */
static void test_the_live_card_is_never_silent(void)
{
    CampaignState campaign = {0};

    /* A window sector: the welded stair door cannot be unlocked, so the door's
     * own fanfare never fires and the card has to speak for itself. */
    GameplayState blocked = {0};
    blocked.level.map.has_window = true;
    blocked.level.runtime.item_count = 1;
    blocked.level.runtime.card_count = 1;
    blocked.level.runtime.active_card_index = 0;
    blocked.level.runtime.items[0] =
        (Item){.x = 8.0f, .y = 8.0f, .type = ITEM_CARD};

    gameplay_collect_items(&blocked, &campaign, 0.0f);
    CHECK(blocked.level.runtime.items[0].collected);
    CHECK(!blocked.level.runtime.exit_unlocked); /* the window is the route */
    CHECK(blocked.events.count == 1);
    CHECK(blocked.events.items[0].type == GAME_EVENT_SOUND);
    CHECK(blocked.events.items[0].data.sound.effect == SFX_CARD_SCAN);

    /* An ordinary sector still answers with the door's own sound, and only
     * that one: the accept tone must not double up on the fanfare. */
    GameplayState opens = {0};
    opens.level.runtime.item_count = 1;
    opens.level.runtime.card_count = 1;
    opens.level.runtime.active_card_index = 0;
    opens.level.runtime.items[0] =
        (Item){.x = 8.0f, .y = 8.0f, .type = ITEM_CARD};

    gameplay_collect_items(&opens, &campaign, 0.0f);
    CHECK(opens.level.runtime.exit_unlocked);
    CHECK(opens.events.count == 1);
    CHECK(opens.events.items[0].data.sound.effect == SFX_EXIT_UNLOCKED);

    /* And the card found after a finished hack has already opened the way: the
     * fanfare is spent, the card is still live, and it still answers. */
    GameplayState already = {0};
    already.level.runtime.exit_unlocked = true;
    already.level.runtime.item_count = 1;
    already.level.runtime.card_count = 1;
    already.level.runtime.active_card_index = 0;
    already.level.runtime.items[0] =
        (Item){.x = 8.0f, .y = 8.0f, .type = ITEM_CARD};

    gameplay_collect_items(&already, &campaign, 0.0f);
    CHECK(already.events.count == 1);
    CHECK(already.events.items[0].data.sound.effect == SFX_CARD_SCAN);
}

/*
 * No card is wrong where no card can be.
 *
 * The seed still picks a live card in a window sector and it still opens the
 * nothing it always opened — but the strip reads BLOCKED, the stair door is
 * welded, and there is no second card to go and find. A decoy buzzing
 * `SFX_CARD_WRONG` there says "wrong one, keep looking" to a player whose only
 * route out is a window they have already been shown, and sector 14 says it to
 * them across twelve men, two cameras and three consoles.
 *
 * The sector that has a `Y` and a `C` is the sector the fix above was written
 * without: its comment reasoned that no shipped map had one.
 */
static void test_no_card_is_wrong_where_no_card_can_be(void)
{
    CampaignState campaign = {0};

    /* Two cards behind a window, and the one the seed did not pick. */
    GameplayState blocked = {0};
    blocked.level.map.has_window = true;
    blocked.level.runtime.item_count = 2;
    blocked.level.runtime.card_count = 2;
    blocked.level.runtime.active_card_index = 1;
    blocked.level.runtime.items[0] =
        (Item){.x = 8.0f, .y = 8.0f, .type = ITEM_CARD};
    blocked.level.runtime.items[1] =
        (Item){.x = 512.0f, .y = 8.0f, .type = ITEM_CARD};

    gameplay_collect_items(&blocked, &campaign, 0.0f);
    CHECK(blocked.level.runtime.items[0].collected);
    CHECK(blocked.events.count == 1);
    CHECK(blocked.events.items[0].type == GAME_EVENT_SOUND);
    CHECK(blocked.events.items[0].data.sound.effect == SFX_CARD_SCAN);

    /* It still scores and still banks the progress it always did. */
    CHECK(campaign.score == CARD_SCORE);

    /* And a decoy behind a stair door that a card *can* open still buzzes:
     * there the sound is information, because another card is out there. */
    GameplayState locked = {0};
    locked.level.runtime.item_count = 2;
    locked.level.runtime.card_count = 2;
    locked.level.runtime.active_card_index = 1;
    locked.level.runtime.items[0] =
        (Item){.x = 8.0f, .y = 8.0f, .type = ITEM_CARD};
    locked.level.runtime.items[1] =
        (Item){.x = 512.0f, .y = 8.0f, .type = ITEM_CARD};

    gameplay_collect_items(&locked, &campaign, 0.0f);
    CHECK(locked.events.count == 1);
    CHECK(locked.events.items[0].data.sound.effect == SFX_CARD_WRONG);
}

/*
 * A finished hack always answers, the way the live card does.
 *
 * `gameplay_advance_terminal` returns "did this open the door", and the shell
 * spends that on the banner and on `SFX_EXIT_UNLOCKED`. In a window sector the
 * answer is no and it used to be the whole of the feedback: the prompt reads
 * `BREACHING SECURITY... 97%` and then vanishes mid-count, because
 * `gameplay_player_near_active_terminal` stops answering the moment
 * `terminal_hacked` is set. Four seconds of standing still on the busiest floor
 * in the building, ending in the HUD deleting itself.
 *
 * Sector 14 is a window sector with three consoles on it, so this is not a
 * hypothetical map — it is the same floor the card branch was getting wrong.
 */
static void test_a_finished_hack_is_never_silent(void)
{
    CampaignState campaign = {0};

    GameplayState blocked = {0};
    blocked.level.map.has_window = true;
    blocked.terminal_hacking = true;
    blocked.terminal_hack_progress = TERMINAL_HACK_TIME - 0.01f;
    blocked.terminal_hack_tick_timer = 1.0f; /* no tick lands on this step */

    bool opened = gameplay_advance_terminal(&blocked, &campaign, 0.02f);
    CHECK(!opened); /* the welded door is still welded */
    CHECK(blocked.level.runtime.terminal_hacked);
    CHECK(campaign.score == TERMINAL_SCORE);
    CHECK(blocked.events.count == 1);
    CHECK(blocked.events.items[0].type == GAME_EVENT_SOUND);
    /* Not the `SFX_CARD_SCAN` the progress ticks are made of: the last of those
     * lands 0.4s before this does, so scanning again reads as one more tick. */
    CHECK(blocked.events.items[0].data.sound.effect == SFX_CARD_TARGET);

    /* An ordinary sector still answers with the door's own fanfare, and only
     * that one — the shell's banner rides on the return value. */
    GameplayState opens = {0};
    opens.terminal_hacking = true;
    opens.terminal_hack_progress = TERMINAL_HACK_TIME - 0.01f;
    opens.terminal_hack_tick_timer = 1.0f;

    CHECK(gameplay_advance_terminal(&opens, &campaign, 0.02f));
    CHECK(opens.level.runtime.exit_unlocked);
    CHECK(opens.events.count == 1);
    CHECK(opens.events.items[0].data.sound.effect == SFX_EXIT_UNLOCKED);
}

static void test_mine_damage_emits_feedback(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.facing = 1;
    state.mines[0] = (Mine){.x = 0.0f, .y = 10.0f, .active = true};
    state.mine_count = 1;

    gameplay_combat_update_explosives(&state, &campaign, 0.01f);
    CHECK(state.mines[0].triggered);
    gameplay_combat_update_explosives(&state, &campaign,
                                      MINE_TRIGGER_DELAY + 0.01f);
    CHECK(!state.mines[0].active);
    CHECK(state.player.dying);

    bool found_explosion = false;
    bool found_shake = false;
    for (int i = 0; i < state.events.count; ++i)
    {
        found_explosion |= state.events.items[i].type == GAME_EVENT_EXPLOSION;
        found_shake |= state.events.items[i].type == GAME_EVENT_CAMERA_SHAKE;
    }
    CHECK(found_explosion);
    CHECK(found_shake);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_EXPLOSION));
}

static void test_grenade_fuse_and_explosion_emit_sounds(void)
{
    static const char data[] =
        "########\n"
        "#S    E#\n"
        "########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 34);
    CHECK(level_load_data(&state.level, "grenade", data, strlen(data),
                          &state.rng));
    state.grenade_count = 1;
    state.grenades[0] = (Grenade){
        .x = 96.0f,
        .y = TILE_SIZE + 4.0f,
        .active = true,
        .timer = 0.25f,
        .fuse_sound_timer = 0.01f};

    gameplay_combat_update_explosives(&state, &campaign, 0.02f);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_GRENADE_FUSE));

    gameplay_combat_update_explosives(&state, &campaign, 0.30f);
    CHECK(!state.grenades[0].active);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_EXPLOSION));
}

/*
 * A throw spends one grenade, not the pocket.
 *
 * The count used to be *cleared* on the throw, which is indistinguishable from a
 * decrement everywhere in the campaign — `item_would_be_wasted` refuses an `N` to
 * a player already carrying one, so nobody playing the game ever holds two. That
 * is exactly why it went unnoticed: the bug only shows on the second grenade,
 * and the campaign never hands one over. It is still a count, and a count that
 * is emptied by spending one of it is wrong however few reach it.
 *
 * Both ends are held here, because the fix is only correct if it changed nothing
 * for the one who carries one and everything for the one who carries two.
 */
static void test_a_throw_spends_one_grenade(void)
{
    static const char data[] =
        "##########\n"
        "#S      E#\n"
        "##########\n";

    /* Two in the pocket: the second survives the first throw. */
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 91);
    CHECK(level_load_data(&state.level, "one-throw", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    state.player.grenades = 2;
    state.player.active_weapon = PLAYER_WEAPON_GRENADE;

    Input input = {0};
    input.shoot = true;
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.grenades == 1);
    CHECK(state.grenades[0].active);
    /* And the hand still drops back to the sidearm: an explosive is only ever
     * raised on purpose, however many are left. */
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    /* Selected again by hand, the second one throws too. */
    state.player.active_weapon = PLAYER_WEAPON_GRENADE;
    input.shoot = true;
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.grenades == 0);
    CHECK(state.grenades[1].active);

    /* The campaign's own case, unchanged: one in, none out. */
    GameplayState single = {0};
    CampaignState single_campaign = {0};
    rng_seed(&single.rng, 92);
    CHECK(level_load_data(&single.level, "one-throw", data, strlen(data),
                          &single.rng));
    player_reset(&single.player, &single.level);
    single.player.grenades = 1;
    single.player.active_weapon = PLAYER_WEAPON_GRENADE;

    Input single_input = {0};
    single_input.shoot = true;
    gameplay_combat_handle_player_action(&single, &single_campaign,
                                         &single_input);
    CHECK(single.player.grenades == 0);
    CHECK(!player_weapon_available(&single.player, PLAYER_WEAPON_GRENADE));
}

/*
 * Only the magazine comes back.
 *
 * The sidearm is what a sector is played with, so its box respawns and a
 * player who has spent it is never left with only the knife. Nothing else is
 * in that position: the grenade used to regrow with it, which made a single
 * `N` an unlimited supply at ITEM_RESPAWN_TIME apiece — enough to clear a
 * floor a blast at a time, and enough to open every blocked-up patch in the
 * campaign without the bazooka the patches were placed for.
 */
static void test_only_the_magazine_comes_back(void)
{
    static const char data[] =
        "##########\n"
        "#S G N  E#\n"
        "##########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 71);
    CHECK(level_load_data(&state.level, "pickups", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    Item *ammo = NULL;
    Item *grenade = NULL;
    for (int i = 0; i < state.level.runtime.item_count; ++i)
    {
        if (state.level.runtime.items[i].type == ITEM_GUN)
            ammo = &state.level.runtime.items[i];
        if (state.level.runtime.items[i].type == ITEM_GRENADE)
            grenade = &state.level.runtime.items[i];
    }
    REQUIRE(ammo != NULL);
    REQUIRE(grenade != NULL);

    state.player.x = grenade->x - PLAYER_W * 0.5f;
    state.player.y = grenade->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(grenade->collected);
    CHECK(state.player.grenades == 1);

    state.player.x = ammo->x - PLAYER_W * 0.5f;
    state.player.y = ammo->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(ammo->collected);

    /* Stand well clear, then wait out anything that is coming back. */
    state.player.x = 0.0f;
    state.player.y = 0.0f;
    gameplay_collect_items(&state, &campaign, ITEM_RESPAWN_TIME * 2.0f);
    CHECK(!ammo->collected);
    CHECK(grenade->collected);
}

static void test_bazooka_pickup_and_rocket_explosion(void)
{
    static const char data[] =
        "##############\n"
        "#S Z   MM  E #\n"
        "##############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 142);
    CHECK(level_load_data(&state.level, "bazooka", data, strlen(data),
                          &state.rng));
    CHECK(state.level.runtime.item_count == 1);
    CHECK(state.level.runtime.items[0].type == ITEM_BAZOOKA);
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 2);
    player_reset(&state.player, &state.level);

    Item *bazooka = &state.level.runtime.items[0];
    state.player.x = bazooka->x - PLAYER_W * 0.5f;
    state.player.y = bazooka->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(bazooka->collected);
    CHECK(state.player.bazooka_rockets == BAZOOKA_AMMO);
    /* Carried, not raised: the tube reaches the hand on a bumper press. */
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_PICKUP_BAZOOKA));

    /* The unique pickup stays consumed; it cannot supply repeated rockets. */
    gameplay_collect_items(&state, &campaign, ITEM_RESPAWN_TIME * 2.0f);
    CHECK(bazooka->collected);

    game_events_clear(&state.events);
    state.player.facing = 1;
    state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
    Input input = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(!input.shoot);
    CHECK(state.player.bazooka_rockets == 0);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);
    CHECK(state.player.bazooka_firing);
    CHECK(state.rockets[0].active);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_ROCKET_LAUNCH));

    for (int frame = 0; frame < SIM_STEPS(1.0f) && state.rockets[0].active; ++frame)
        gameplay_combat_update_player_bullets(&state, &campaign,
                                              SIM_STEP_DT);

    CHECK(!state.rockets[0].active);
    CHECK(state.enemies[0].dead);
    CHECK(state.enemies[1].dead);
    CHECK(campaign.score == 300);
    CHECK(!state.player.dying);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_EXPLOSION));
}

static void test_player_can_switch_between_carried_weapons(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.x = 100.0f;
    state.player.y = 64.0f;
    state.player.facing = 1;
    state.player.bullets = 2;
    state.player.grenades = 1;
    state.player.bazooka_rockets = 1;
    state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;

    Input input = {.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(!input.switch_weapon);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_GRENADE);

    input = (Input){.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    input = (Input){.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.bullets == 1);
    CHECK(state.player.grenades == 1);
    CHECK(state.player.bazooka_rockets == 1);
    CHECK(state.bullets[0].active);
    CHECK(!state.rockets[0].active);

    /* Past the sidearm sit the bolts, which are never carried and so are never
     * stepped over, and the knife is the step after them. */
    input = (Input){.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_DECOY);

    input = (Input){.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);
    input = (Input){.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.knife_attacking);
    CHECK(state.player.bullets == 1);

    state.player.bullets = 0;
    state.player.grenades = 0;
    state.player.bazooka_rockets = 0;
    input = (Input){.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_DECOY);
}

/*
 * The bumpers cycle, which is the job every platform's own guidance gives
 * them, so the ring has to run both ways: RB takes the next usable weapon and
 * LB the one before it, both skipping whatever is out of ammo. One step each
 * way has to land back where it started, or the two bumpers are walking two
 * different lists.
 */
static void test_weapon_cycle_runs_both_ways(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.bullets = 5;
    state.player.grenades = 1;
    state.player.bazooka_rockets = 0; /* nothing to stop on */
    state.player.active_weapon = PLAYER_WEAPON_PISTOL;

    Input input = {.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_DECOY);

    input = (Input){.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);

    input = (Input){.switch_weapon_back = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(!input.switch_weapon_back);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_DECOY);

    input = (Input){.switch_weapon_back = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    /*
     * With the clip dry and the explosives gone, two things are still in reach
     * and the bumpers have to be able to move between exactly those two.
     *
     * This used to be the assertion that neither bumper could leave the knife,
     * which was true while the knife was the only thing that never ran out. The
     * bolts are the second, and they are the reason a player with nothing left
     * still has a choice to make.
     */
    state.player.bullets = 0;
    state.player.grenades = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    input = (Input){.switch_weapon_back = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_DECOY);

    input = (Input){.switch_weapon_back = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);
}

/*
 * The ring names every weapon exactly once, which is what the `_Static_assert`
 * beside `WEAPON_CYCLE` says and not what it checks.
 *
 * That assertion compares the array's length to `PLAYER_WEAPON_COUNT`, so it
 * catches the failure it was written for — a weapon added to the enum and not
 * to the ring, which would index past the end of the array on every bumper
 * press — and is blind to the other shape of the same mistake: one weapon
 * written twice and another left out keeps the length correct. What that costs
 * is a weapon the bumpers can never reach, on a control the player uses
 * mid-fight, and no assertion, no warning and no crash to say so. A comment
 * that promises coverage owes the suite the check that delivers it.
 *
 * Asked as behaviour rather than by exporting the array, because behaviour is
 * what the defect would be: with everything carried, `PLAYER_WEAPON_COUNT`
 * steps forward have to visit `PLAYER_WEAPON_COUNT` distinct weapons and come
 * home. A duplicate makes the walk short by one distinct weapon and lands
 * somewhere else.
 */
static void test_the_weapon_ring_names_every_weapon_exactly_once(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    /* Everything in hand, so nothing is skipped for being empty and the walk
     * is the ring itself rather than the ring minus what is spent. */
    state.player.bullets = MAX_AMMO;
    state.player.grenades = 1;
    state.player.bazooka_rockets = 1;
    state.player.flashbangs = 1;
    state.player.active_weapon = PLAYER_WEAPON_PISTOL;

    bool seen[PLAYER_WEAPON_COUNT] = {false};
    seen[PLAYER_WEAPON_PISTOL] = true;
    int distinct = 1;
    for (int step = 1; step < PLAYER_WEAPON_COUNT; ++step)
    {
        Input input = {.switch_weapon = true};
        gameplay_combat_handle_player_action(&state, &campaign, &input);
        PlayerWeapon at = state.player.active_weapon;
        CHECK(at >= 0 && at < PLAYER_WEAPON_COUNT);
        CHECK(!seen[at]);
        seen[at] = true;
        ++distinct;
    }
    CHECK(distinct == PLAYER_WEAPON_COUNT);
    for (int i = 0; i < PLAYER_WEAPON_COUNT; ++i)
        CHECK(seen[i]);

    /* And the last step closes it. A ring that visited everything once and
     * then landed somewhere other than where it started is not a ring. */
    Input input = {.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);
}

/*
 * Picking a weapon up is not deciding to use it.
 *
 * An explosive that armed itself on contact turned one step across a floor
 * into a spent grenade: the trigger was already being held on the pistol, and
 * the next press threw the scarcest thing in the sector at whatever happened
 * to be in front of Chuck. Firing the last rocket did the same in reverse,
 * walking the cycle straight onto the grenade so the follow-up shot threw it.
 * So a pickup never changes what is in the hand, a spent weapon always falls
 * back to the sidearm, and the one exception is the magazine that ends a dry
 * clip — the only pickup that costs nothing to be holding.
 */
static void test_a_pickup_never_arms_itself(void)
{
    static const char data[] =
        "############\n"
        "#S G N Z E #\n"
        "############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 97);
    CHECK(level_load_data(&state.level, "pickup-arming", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    Item *ammo = NULL, *grenade = NULL, *bazooka = NULL;
    for (int i = 0; i < state.level.runtime.item_count; ++i)
    {
        Item *item = &state.level.runtime.items[i];
        if (item->type == ITEM_GUN)
            ammo = item;
        if (item->type == ITEM_GRENADE)
            grenade = item;
        if (item->type == ITEM_BAZOOKA)
            bazooka = item;
    }
    REQUIRE(ammo != NULL && grenade != NULL && bazooka != NULL);

    /* Walking over an explosive with the sidearm up leaves the sidearm up. */
    state.player.x = grenade->x - PLAYER_W * 0.5f;
    state.player.y = grenade->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(state.player.grenades == 1);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    state.player.x = bazooka->x - PLAYER_W * 0.5f;
    state.player.y = bazooka->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(state.player.bazooka_rockets == BAZOOKA_AMMO);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    /* And the press that follows fires the pistol, not the grenade. */
    Input input = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.bullets[0].active);
    CHECK(state.player.grenades == 1);
    CHECK(state.player.bazooka_rockets == BAZOOKA_AMMO);

    /* The last rocket hands the frame back to the sidearm, never to the
     * grenade still on Chuck's belt. */
    state.player.active_weapon = PLAYER_WEAPON_BAZOOKA;
    input = (Input){.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.bazooka_rockets == 0);
    CHECK(state.player.grenades == 1);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    /* Spending the clip drops to the knife rather than to the grenade. */
    state.player.bullets = 1;
    input = (Input){.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.bullets == 0);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);

    /* The one exception: the magazine that ends a dry clip arms itself, so a
     * player who found one is not left stabbing. */
    state.player.x = ammo->x - PLAYER_W * 0.5f;
    state.player.y = ammo->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(state.player.bullets == MAX_AMMO);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    /* A knife chosen on purpose, with rounds already in the clip, is not that
     * case and survives the next magazine. */
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    state.player.bullets = 2;
    gameplay_collect_items(&state, &campaign, ITEM_RESPAWN_TIME * 2.0f);
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(state.player.bullets == MAX_AMMO);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);
}

/*
 * A pickup that would change nothing is left where it is.
 *
 * The three that cannot come back used to be spent on a counter that was
 * already full: walking over a second `N` while carrying a grenade set
 * `collected` with a nought respawn timer and played the pickup sound, so the
 * scarcest thing in the sector was destroyed by crossing a tile and announced
 * as a success. Sector 12 carries two grenades, sectors 10, 12 and 15 two
 * medkits apiece, and every restroom hands out the grenade the campaign's own
 * budget is balanced on — so this was the middle of four maps, not a corner.
 *
 * The boxed magazine is deliberately not on the list and is checked here for
 * it: `ITEM_GUN` respawns, so taking one with a full clip costs nothing and
 * the box is back before it is wanted.
 */
static void test_a_pickup_that_would_be_wasted_is_left_alone(void)
{
    static const char data[] =
        "#################\n"
        "#S N N Z Z K G E#\n"
        "#################\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    campaign.lives = MAX_LIVES;
    rng_seed(&state.rng, 23);
    CHECK(level_load_data(&state.level, "wasted-pickups", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    Item *grenade[2] = {NULL, NULL};
    Item *rocket[2] = {NULL, NULL};
    Item *medkit = NULL;
    Item *ammo = NULL;
    int grenades_found = 0;
    int rockets_found = 0;
    for (int i = 0; i < state.level.runtime.item_count; ++i)
    {
        Item *item = &state.level.runtime.items[i];
        if (item->type == ITEM_GRENADE && grenades_found < 2)
            grenade[grenades_found++] = item;
        if (item->type == ITEM_BAZOOKA && rockets_found < 2)
            rocket[rockets_found++] = item;
        if (item->type == ITEM_MEDKIT)
            medkit = item;
        if (item->type == ITEM_GUN)
            ammo = item;
    }
    REQUIRE(grenades_found == 2 && rockets_found == 2);
    REQUIRE(medkit != NULL);

    /* The first of each is taken normally. */
    state.player.x = grenade[0]->x - PLAYER_W * 0.5f;
    state.player.y = grenade[0]->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(grenade[0]->collected);
    CHECK(state.player.grenades == 1);

    state.player.x = rocket[0]->x - PLAYER_W * 0.5f;
    state.player.y = rocket[0]->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(rocket[0]->collected);
    CHECK(state.player.bazooka_rockets == BAZOOKA_AMMO);

    /* The second of each is walked straight over and stays on the floor, so it
     * is still there when the first has actually been spent. */
    state.player.x = grenade[1]->x - PLAYER_W * 0.5f;
    state.player.y = grenade[1]->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(!grenade[1]->collected);

    state.player.x = rocket[1]->x - PLAYER_W * 0.5f;
    state.player.y = rocket[1]->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(!rocket[1]->collected);

    /* Spend them, come back, and both are collectable after all. */
    state.player.grenades = 0;
    state.player.bazooka_rockets = 0;
    state.player.x = grenade[1]->x - PLAYER_W * 0.5f;
    state.player.y = grenade[1]->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(grenade[1]->collected);
    CHECK(state.player.grenades == 1);
    state.player.x = rocket[1]->x - PLAYER_W * 0.5f;
    state.player.y = rocket[1]->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(rocket[1]->collected);

    /* The kit answers the hearts first and the spare lives second, so it is
     * only wasted when both are at their cap — and taken again the moment
     * either has room. */
    state.player.hp = gameplay_player_max_hp(&state);
    campaign.lives = MAX_LIVES;
    state.player.x = medkit->x - PLAYER_W * 0.5f;
    state.player.y = medkit->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(!medkit->collected);
    CHECK(campaign.lives == MAX_LIVES);

    campaign.lives = MAX_LIVES - 1;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(medkit->collected);
    CHECK(campaign.lives == MAX_LIVES);

    /* And the one that is allowed to be spent on a full counter, because it is
     * the one that comes back. */
    REQUIRE(ammo != NULL);
    CHECK(state.player.bullets == MAX_AMMO);
    state.player.x = ammo->x - PLAYER_W * 0.5f;
    state.player.y = ammo->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, 0.0f);
    CHECK(ammo->collected);
}

/*
 * What a sector hands to the one above it.
 *
 * The facade is why this rule exists. Nothing on a climb can be thrown or
 * fired — the shell clears `shoot` for the whole of `update_facade_playing`
 * and gameplay_climb.c has no notion of a weapon — so the `N` standing
 * mid-wall on every one of the five climbs is a pickup whose entire value is
 * in the sector above it. Wiped at the doorway, as it used to be, that was a
 * detour paid for in wind and thrown bricks that bought nothing at all, and
 * the campaign's fourteen grenades included four that could never be spent.
 *
 * The rule lives in player.c rather than in the shell's `load_level` precisely
 * so this test can reach it: `load_level` is SDL-side and the suite links no
 * SDL, which is how the doorway went unexamined in the first place.
 */
static void test_a_sector_hands_its_explosives_to_the_next(void)
{
    static const char data[] =
        "##########\n"
        "#S  N   E#\n"
        "##########\n";
    Level level = {0};
    Rng rng;
    rng_seed(&rng, 37);
    CHECK(level_load_data(&level, "handover", data, strlen(data), &rng));

    Player departed = {0};
    player_reset(&departed, &level);
    departed.grenades = 1;
    departed.bazooka_rockets = BAZOOKA_AMMO;
    departed.bullets = 1;
    departed.hp = 1;
    departed.active_weapon = PLAYER_WEAPON_GRENADE;

    /* A campaign step: the explosives arrive, and nothing else does. */
    Player arrived = {0};
    player_begin_sector(&arrived, &level, &departed);
    CHECK(arrived.grenades == 1);
    CHECK(arrived.bazooka_rockets == BAZOOKA_AMMO);
    /* A doorway is a pickup as far as this rule goes: the sector opens on the
     * sidearm, so the first press of the trigger cannot throw the grenade the
     * player was saving. */
    CHECK(arrived.active_weapon == PLAYER_WEAPON_PISTOL);
    CHECK(arrived.bullets == MAX_AMMO);
    CHECK(arrived.hp == PLAYER_MAX_HP);
    CHECK(arrived.x == level.map.start_x && arrived.y == level.map.start_y);

    /* Nobody walked out of the sector below: the first sector of a run, the
     * retry after a continue, and `--level N`. */
    Player fresh = {0};
    player_begin_sector(&fresh, &level, NULL);
    CHECK(fresh.grenades == 0);
    CHECK(fresh.bazooka_rockets == 0);
    CHECK(fresh.active_weapon == PLAYER_WEAPON_PISTOL);

    /* An empty-handed departure hands over nothing, which is the ordinary
     * case and must not turn into a free grenade. */
    Player empty = {0};
    player_reset(&empty, &level);
    Player after_empty = {0};
    player_begin_sector(&after_empty, &level, &empty);
    CHECK(after_empty.grenades == 0);
    CHECK(after_empty.bazooka_rockets == 0);
}

/*
 * Every climb carries an explosive it cannot spend, which is the whole reason
 * the handover above exists. If a climb ever stops carrying one this test is
 * the place to notice, because the rule would then be paying for nothing.
 */
static void test_every_climb_carries_an_explosive_out(void)
{
    int climbs = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Level level = {0};
        Rng rng;
        rng_seed(&rng, 500 + (int)i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        if (level.map.mode != LEVEL_MODE_FACADE)
            continue;
        climbs++;
        int explosives = 0;
        for (int item = 0; item < level.runtime.item_count; ++item)
        {
            ItemType type = level.runtime.items[item].type;
            if (type == ITEM_GRENADE || type == ITEM_BAZOOKA)
                explosives++;
        }
        CHECK(explosives >= 1);
    }
    CHECK(climbs == 5);
}

/*
 * And the other half of that question, which nothing asked for a long time: a
 * climb may not lay out a pickup that cannot do anything at all.
 *
 * All four climbs carried a `G`, and a magazine is the one pickup a wall has no
 * use for whatever. Nothing up there can be fired — `update_facade_playing`
 * clears `shoot` every frame and [gameplay_climb.c](../src/gameplay_climb.c) has
 * no notion of a weapon — and the clip does not cross the doorway either, because
 * `player_begin_sector` carries only the grenade and the rocket and
 * `player_reset` hands the next sector a full one regardless. So `bullets` is
 * MAX_AMMO for the whole ascent, the box changed no counter, paid no score, and
 * played `SFX_PICKUP_AMMO` — the sound of a pickup that worked.
 *
 * That is exactly the failure `item_would_be_wasted` was written to stop for the
 * grenade, the rocket and the medkit. `ITEM_GUN` is deliberately exempt from it
 * because the box comes back on `ITEM_RESPAWN_TIME`, which is a correct argument
 * about an interior floor and an empty one about a wall — so the rule is kept
 * here instead, where the *map* is what fails. A climb has no use for a
 * magazine and must not draw one.
 *
 * Written as "no ITEM_GUN" rather than as a list of what is allowed, so a pickup
 * added to the game later is allowed on a wall until somebody decides otherwise
 * rather than being refused by a test nobody remembers writing.
 */
static void test_no_climb_lays_out_a_pickup_it_cannot_use(void)
{
    int climbs = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Level level = {0};
        Rng rng;
        rng_seed(&rng, 700 + (int)i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        if (level.map.mode != LEVEL_MODE_FACADE)
            continue;
        climbs++;
        for (int item = 0; item < level.runtime.item_count; ++item)
            CHECK(level.runtime.items[item].type != ITEM_GUN);
    }
    CHECK(climbs == 5);
}

static void test_gas_canister_requires_crawling_shot(void)
{
    static const char data[] =
        "###########\n"
        "#S   LM E #\n"
        "###########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 61);
    CHECK(level_load_data(&state.level, "gas-canister", data, strlen(data),
                          &state.rng));
    CHECK(state.level.runtime.gas_canister_count == 1);
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    player_reset(&state.player, &state.level);

    GasCanister *canister = &state.level.runtime.gas_canisters[0];
    Input input = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    Bullet *standing_bullet = &state.bullets[0];
    CHECK(standing_bullet->active);
    CHECK(standing_bullet->y + BULLET_H <= canister->y);
    float travel_time =
        (canister->x + GAS_CANISTER_W + 1.0f - standing_bullet->x) /
        standing_bullet->vx;
    gameplay_combat_update_player_bullets(&state, &campaign, travel_time);
    CHECK(canister->active);

    memset(state.bullets, 0, sizeof(state.bullets));
    game_events_clear(&state.events);
    state.player.y += (float)(PLAYER_H - PLAYER_CRAWL_H);
    state.player.crawling = true;
    input = (Input){.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    Bullet *crawling_bullet = &state.bullets[0];
    CHECK(crawling_bullet->active);
    CHECK(crawling_bullet->y < canister->y + GAS_CANISTER_H);
    CHECK(crawling_bullet->y + BULLET_H > canister->y);
    travel_time =
        (canister->x + GAS_CANISTER_W + 1.0f - crawling_bullet->x) /
        crawling_bullet->vx;
    gameplay_combat_update_player_bullets(&state, &campaign, travel_time);

    CHECK(!canister->active);
    CHECK(!crawling_bullet->active);
    CHECK(state.enemies[0].dead);
    CHECK(campaign.score == 150);
    bool found_explosion = false;
    for (int i = 0; i < state.events.count; ++i)
        found_explosion |=
            state.events.items[i].type == GAME_EVENT_EXPLOSION;
    CHECK(found_explosion);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_EXPLOSION));
}

/*
 * And the cylinder is just as real from the other end of the room.
 *
 * A guard's round used to be tested against tiles, crates and Chuck and
 * nothing else, so it went through a gas canister as though the steel were
 * air: the player could not shelter behind one and a guard could not set one
 * off, not even the one he was standing beside. The manual teaches this as a
 * rule about the world, and a rule only one of the two people in the room
 * obeys is a special case nothing on screen explains.
 */
static void test_a_guard_s_round_sets_off_a_gas_canister(void)
{
    static const char data[] =
        "###########\n"
        "#S   LM E #\n"
        "###########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 61);
    CHECK(level_load_data(&state.level, "guard-canister", data, strlen(data),
                          &state.rng));
    CHECK(state.level.runtime.gas_canister_count == 1);
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    player_reset(&state.player, &state.level);

    GasCanister *canister = &state.level.runtime.gas_canisters[0];
    Bullet *round = &state.enemy_bullets[0];

    /* Stepped a frame at a time rather than teleported past in one call: the
     * guard's round is deliberately not swept, so what is being tested is that
     * a real frame of travel finds the cylinder. */
    round->active = true;
    round->vx = -ENEMY_BULLET_SPEED;
    round->vy = 0.0f;
    round->x = canister->x + 48.0f;
    /* Over the top of it — the same standing shot the player's own pistol
     * takes, so the low profile is not a rule that only applies to Chuck. */
    round->y = canister->y - (float)BULLET_H - 2.0f;
    for (int step = 0; step < SIM_STEPS(0.2f) && round->active; ++step)
        gameplay_combat_update_enemy_bullets(&state, &campaign, SIM_STEP_DT);
    CHECK(canister->active);
    CHECK(round->x + BULLET_W < canister->x); /* it really did pass it */

    /* One that comes in low finds it. */
    round->active = true;
    round->x = canister->x + 48.0f;
    round->y = canister->y + GAS_CANISTER_H * 0.5f;
    game_events_clear(&state.events);
    for (int step = 0; step < SIM_STEPS(0.2f) && canister->active; ++step)
        gameplay_combat_update_enemy_bullets(&state, &campaign, SIM_STEP_DT);

    CHECK(!canister->active);
    CHECK(!round->active);
    /* And the blast is an ordinary blast: it takes the man who fired it. */
    CHECK(state.enemies[0].dead);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_EXPLOSION));
}

/*
 * A blast is one rule, whichever explosive delivered it.
 *
 * A player who has been taught to shoot a gas canister will throw a grenade at
 * the next one, and a rocket already set them off — so a grenade that left the
 * canister standing was the game disagreeing with what it had just taught. The
 * two halves below pin the pair that used to be missing: a grenade chains into
 * a canister, and a mine is lethal to whoever is standing in it rather than
 * only to the walls and the player.
 */
static void test_every_blast_reaches_the_same_things(void)
{
    static const char grenade_map[] =
        "############\n"
        "#S   N  L E#\n"
        "############\n";
    GameplayState grenade_state = {0};
    CampaignState grenade_campaign = {0};
    rng_seed(&grenade_state.rng, 91);
    CHECK(level_load_data(&grenade_state.level, "grenade-chain", grenade_map,
                          strlen(grenade_map), &grenade_state.rng));
    CHECK(grenade_state.level.runtime.gas_canister_count == 1);
    player_reset(&grenade_state.player, &grenade_state.level);

    GasCanister *canister = &grenade_state.level.runtime.gas_canisters[0];
    /* Put the grenade on the canister rather than throwing it across the room:
     * this is about what the blast reaches, not about the arc. */
    grenade_state.grenades[0] = (Grenade){
        .x = canister->x, .y = canister->y, .active = true, .timer = 0.01f};
    grenade_state.grenade_count = 1;
    gameplay_combat_update_explosives(&grenade_state, &grenade_campaign, 0.02f);
    CHECK(!grenade_state.grenades[0].active);
    CHECK(!canister->active);

    /* A mine goes off under a guard who followed Chuck onto it. */
    GameplayState mine_state = {0};
    CampaignState mine_campaign = {0};
    rng_seed(&mine_state.rng, 23);
    mine_state.player.facing = 1;
    mine_state.player.hp = PLAYER_MAX_HP;
    mine_state.mines[0] = (Mine){.x = 0.0f, .y = 10.0f, .active = true};
    mine_state.mine_count = 1;
    enemy_init(&mine_state.enemies[0], MINE_W * 0.5f, 10.0f, ENEMY_KIND_GUARD, &mine_state.rng);
    mine_state.enemy_count = 1;

    gameplay_combat_update_explosives(&mine_state, &mine_campaign, 0.01f);
    CHECK(mine_state.mines[0].triggered);
    gameplay_combat_update_explosives(&mine_state, &mine_campaign,
                                      MINE_TRIGGER_DELAY + 0.01f);
    CHECK(!mine_state.mines[0].active);
    CHECK(mine_state.enemies[0].dead);
    CHECK(mine_campaign.score == 150);
    CHECK(events_have_sound(&mine_state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_ENEMY_DOWN));
}

/*
 * And a charge in the blast is a charge that goes off — all three kinds.
 *
 * The canister chained on its own for a long time while the mine and the
 * grenade sat in the same fireball untouched, so a rocket into a mined corridor
 * cleared the canisters and stepped over the mines. Nothing on screen said why,
 * and nothing could: it was a list, not a rule.
 *
 * The mine half also pins the part that is easy to get wrong twice over — an
 * *unarmed* mine goes off, because the player's weight is what arms one and
 * pressure is what sets one off, and it goes off *now* rather than after
 * MINE_TRIGGER_DELAY, because the delay is the beat between a boot and the bang
 * and there is no boot in this.
 */
static void test_a_blast_sets_off_every_charge_it_reaches(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 137);
    state.player.hp = PLAYER_MAX_HP;
    /* Well clear of the blast: this is about the charges, not about Chuck. */
    state.player.x = 40.0f * TILE_SIZE;
    state.player.y = 0.0f;

    /* One grenade about to go off, with an untouched mine and a second grenade
     * lying beside it — neither of them stepped on, neither of them lit. */
    state.grenades[0] = (Grenade){
        .x = 100.0f, .y = 100.0f, .active = true, .timer = 0.01f};
    state.grenades[1] = (Grenade){
        .x = 100.0f + GRENADE_RADIUS * 0.5f, .y = 100.0f,
        .active = true, .timer = GRENADE_FUSE_TIME};
    state.grenade_count = 2;
    state.mines[0] = (Mine){.x = 100.0f - GRENADE_RADIUS * 0.5f, .y = 100.0f,
                            .active = true};
    state.mine_count = 1;
    CHECK(!state.mines[0].triggered);
    /* And a rocket crossing the same room. It was the one charge a blast used
     * to step over, which made "everything explosive in reach" a list rather
     * than a rule — the four hand-written copies this function replaced are
     * exactly what a list turns into. */
    state.rockets[0] = (Rocket){.x = 100.0f, .y = 100.0f - GRENADE_RADIUS * 0.5f,
                                .vx = ROCKET_SPEED, .vy = 0.0f, .active = true};

    gameplay_combat_update_explosives(&state, &campaign, 0.02f);

    CHECK(!state.grenades[0].active);
    /* Every neighbour is spent, in the same frame, without the fuse, the
     * trigger delay or the impact any of them was waiting on. */
    CHECK(!state.grenades[1].active);
    CHECK(!state.mines[0].active);
    CHECK(!state.rockets[0].active);

    /* A chain terminates: each charge is deactivated before its own blast, so
     * nothing can come back round through its own radius. Reaching here at all
     * is most of that proof; this is the rest of it. */
    for (int i = 0; i < state.grenade_count; ++i)
        CHECK(!state.grenades[i].active);
    for (int i = 0; i < state.mine_count; ++i)
        CHECK(!state.mines[i].active);
    for (int i = 0; i < MAX_ROCKETS; ++i)
        CHECK(!state.rockets[i].active);
}

/*
 * A blast is a radius, and masonry does not stop it.
 *
 * This is the one rule in the game that does not go through `level_is_solid`,
 * and it is deliberate: an explosive spent bringing a `%` patch down is meant
 * to take whoever stood behind it along with the wall. What decides whether a
 * given explosive actually reaches through is its radius against the geometry,
 * and the four of them land on both sides of that line — which is a real
 * difference in how they play, and exactly the kind of difference a tuning
 * pass moves without noticing. So both ends are pinned here: the grenade that
 * must not carry, and the rocket that must.
 *
 * Same map, same wall, same guard both times; only the explosive changes.
 */
static void test_a_blast_carries_through_a_wall(void)
{
    /* cols:  0123456789
     * row 1: #S  #M  E#   -> player col 1, solid wall col 4, guard col 5 */
    static const char map[] =
        "##########\n"
        "#S  #M  E#\n"
        "##########\n";

    /* Where the guard stands, and the wall he is standing behind. */
    const float guard_x = 5.0f * TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
    const float guard_y = 1.0f * TILE_SIZE;

    /* The grenade: resting against the near face of the wall, so this is the
     * closest it can get to him without the wall being gone. */
    {
        GameplayState state = {0};
        CampaignState campaign = {0};
        rng_seed(&state.rng, 17);
        CHECK(level_load_data(&state.level, "blast-through-wall", map,
                              strlen(map), &state.rng));
        CHECK(level_is_solid(&state.level, 4, 1));
        player_reset(&state.player, &state.level);
        enemy_init(&state.enemies[0], guard_x, guard_y, ENEMY_KIND_GUARD, &state.rng);
        state.enemy_count = 1;

        state.grenades[0] = (Grenade){
            .x = 4.0f * TILE_SIZE - GRENADE_W,
            .y = 1.0f * TILE_SIZE,
            .active = true,
            .timer = 0.01f};
        state.grenade_count = 1;
        gameplay_combat_update_explosives(&state, &campaign, 0.02f);

        CHECK(!state.grenades[0].active);
        /* 48 of radius against a tile of wall plus half of each body: short. */
        CHECK(!state.enemies[0].dead);
        CHECK(campaign.score == 0);
    }

    /* The rocket, fired at the same wall from the same side. It detonates on
     * the masonry and the guard behind it goes down with the wall. */
    {
        GameplayState state = {0};
        CampaignState campaign = {0};
        rng_seed(&state.rng, 17);
        CHECK(level_load_data(&state.level, "blast-through-wall", map,
                              strlen(map), &state.rng));
        CHECK(level_is_solid(&state.level, 4, 1));
        player_reset(&state.player, &state.level);
        enemy_init(&state.enemies[0], guard_x, guard_y, ENEMY_KIND_GUARD, &state.rng);
        state.enemy_count = 1;

        state.rockets[0] = (Rocket){
            .x = 2.0f * TILE_SIZE,
            .y = 1.0f * TILE_SIZE + (TILE_SIZE - ROCKET_H) * 0.5f,
            .vx = ROCKET_SPEED,
            .vy = 0.0f,
            .active = true};

        /* Let it fly into the wall. Small steps, so nothing steps over a tile. */
        for (int i = 0; i < SIM_STEPS(0.25f) && state.rockets[0].active; ++i)
            gameplay_combat_update_player_bullets(&state, &campaign,
                                                  SIM_STEP_DT);

        CHECK(!state.rockets[0].active);
        /* The wall is still a wall — a plain `#` is not a `%` and no blast
         * opens one, so this really is a kill through standing masonry. */
        CHECK(level_is_solid(&state.level, 4, 1));
        CHECK(state.enemies[0].dead);
        CHECK(campaign.score == 150);
    }
}

/*
 * The tally is what the player did, not who is left standing.
 *
 * A reinforcement takes over the slot of a guard already down, so counting the
 * `dead` flags at the end of a sector quietly credited one kill fewer for every
 * guard the doors sent after the first — the report between sectors reads out
 * of this, and it was reading the surviving population.
 */
static void test_the_kill_tally_survives_a_reused_slot(void)
{
    static const char map[] =
        "############\n"
        "#S  M     E#\n"
        "############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 31);
    CHECK(level_load_data(&state.level, "tally", map, strlen(map), &state.rng));
    player_reset(&state.player, &state.level);
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    CHECK(gameplay_neutralized_hostiles(&state) == 0);

    state.enemies[0].hp = 1;
    Bullet *bullet = &state.bullets[0];
    bullet->active = true;
    bullet->x = state.enemies[0].x + ENEMY_W * 0.5f;
    bullet->y = state.enemies[0].y + ENEMY_H * 0.5f;
    bullet->vx = BULLET_SPEED;
    bullet->vy = 0.0f;
    gameplay_combat_update_player_bullets(&state, &campaign, 0.01f);
    CHECK(state.enemies[0].dead);
    CHECK(gameplay_neutralized_hostiles(&state) == 1);

    /* Fill the array so the next arrival has to take the body's slot back. */
    for (int i = state.enemy_count; i < MAX_ENEMIES; ++i)
    {
        enemy_init(&state.enemies[i], 64.0f, 32.0f, ENEMY_KIND_GUARD, &state.rng);
        state.enemy_count++;
    }
    CHECK(state.enemy_count == MAX_ENEMIES);
    enemy_init(&state.enemies[0], 64.0f, 32.0f, ENEMY_KIND_GUARD, &state.rng);
    CHECK(!state.enemies[0].dead);
    /* Nobody on the floor is dead any more, and the kill still counts. */
    CHECK(gameplay_neutralized_hostiles(&state) == 1);
}

/* A fresh slot before a body's, so a reinforcement never deletes a corpse the
 * player is looking at — and the corpse is what sends the next guard to the
 * alarm, so it has to outlive the door that answered it. */
static void test_reinforcements_take_a_fresh_slot_before_a_body(void)
{
    static const char map[] =
        "##########\n"
        "#S  D  DE#\n"
        "##########\n"
        "\n"
        "SPAWNS 1 0\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 12);
    CHECK(level_load_data(&state.level, "reinforce", map, strlen(map),
                          &state.rng));
    player_reset(&state.player, &state.level);
    CHECK(state.level.map.door_count == 2);

    /* One guard, already down, in the only slot there is. */
    enemy_init(&state.enemies[0], 2.0f * TILE_SIZE, 1.0f * TILE_SIZE, ENEMY_KIND_GUARD, &state.rng);
    state.enemies[0].dead = true;
    state.enemy_count = 1;

    state.door_spawns[0] = 1;
    state.door_timers[0] = 0.0f;
    gameplay_ai_update_spawns(&state, 0.1f);

    CHECK(state.enemy_count == 2);
    CHECK(state.enemies[0].dead);
    CHECK(!state.enemies[1].dead);
}

/*
 * And the animal keeps the same rule, which it spent a long time not keeping.
 *
 * The fix above was made for the guards alone, while `find_dog_slot` went on
 * taking a dead dog's slot before a fresh one — so a handler arriving with a dog
 * deleted a corpse off the floor in front of the player. Both halves of the
 * argument apply to a dog word for word: the body is drawn, and
 * `update_body_discovery` sends a calm guard to look at a fallen animal exactly
 * as it does at a fallen man.
 */
static void test_a_reinforcement_dog_takes_a_fresh_slot_before_a_body(void)
{
    static const char map[] =
        "##################\n"
        "#S  D        D  E#\n"
        "##################\n"
        "\n"
        "SPAWNS 1 0\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 7);
    CHECK(level_load_data(&state.level, "reinforce-dog", map, strlen(map),
                          &state.rng));
    player_reset(&state.player, &state.level);
    CHECK(state.level.map.door_count == 2);

    /* One dog, already down, in the only slot there is. */
    dog_init(&state.dogs[0], 2.0f * TILE_SIZE, 1.0f * TILE_SIZE, -1,
             &state.rng);
    state.dogs[0].dead = true;
    state.dog_count = 1;
    float corpse_x = state.dogs[0].x;
    float corpse_y = state.dogs[0].y;

    /* The door keeps sending guards until one of them arrives with a handler's
     * dog (DOG_DOOR_HANDLER_CHANCE, so not every arrival brings one). Each one
     * is walked out of the doorway first, or the next is refused for standing
     * where the last one is. */
    for (int attempt = 0; attempt < MAX_ENEMIES && state.dog_count < 2;
         ++attempt)
    {
        state.door_spawns[0] = 1;
        state.door_timers[0] = 0.0f;
        gameplay_ai_update_spawns(&state, SIM_STEP_DT);
        for (int i = 0; i < state.enemy_count; ++i)
            state.enemies[i].x = 8.0f * TILE_SIZE;
    }

    CHECK(state.dog_count == 2);
    CHECK(!state.dogs[1].dead);
    /* The corpse is still a corpse, and still where it fell. */
    CHECK(state.dogs[0].dead);
    CHECK(state.dogs[0].x == corpse_x);
    CHECK(state.dogs[0].y == corpse_y);
}

/* A body is on the floor of the frame now, and it is also the place a guard is
 * sent to investigate. One shot off a ladder must not leave it in the air. */
static void test_a_body_falls_to_the_floor(void)
{
    static const char map[] =
        "############\n"
        "#S       #E#\n"
        "#         #\n"
        "############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 44);
    CHECK(level_load_data(&state.level, "body-fall", map, strlen(map),
                          &state.rng));
    player_reset(&state.player, &state.level);

    enemy_init(&state.enemies[0], 4.0f * TILE_SIZE, 1.0f * TILE_SIZE, ENEMY_KIND_GUARD, &state.rng);
    state.enemies[0].dead = true;
    state.enemy_count = 1;
    float dropped_from = state.enemies[0].y;

    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);

    CHECK(state.enemies[0].y > dropped_from);
    /* Standing on the floor slab of the two-row band, not through it. */
    CHECK(fabsf(state.enemies[0].y - (3.0f * TILE_SIZE - ENEMY_H)) < 1.0f);
}

/*
 * When the array really is full, the corpse that is overwritten stops being a
 * corpse anybody remembers.
 *
 * `release_body_bit` is four regions, it had never been executed, and the
 * reason is the same reason it is right: `find_enemy_slot` only reaches it once
 * `MAX_ENEMIES` is exhausted, and `MAX_ENEMIES` is sized so that no shipped
 * floor gets there. That makes it dead code on the campaign and live code on
 * anything anybody draws in the editor — which is exactly the kind of fallback
 * that is wrong for years before it is noticed.
 *
 * What it must not do is leave the bit set. `Enemy.bodies_investigated` is one
 * bit per corpse, so a bit left standing for a slot now holding a *live* guard
 * silently cancels the walk over for whoever finds **him** dead later: the one
 * rule the quiet route rests on, switched off for one man, for the rest of the
 * floor, with nothing on screen to say so.
 */
static void test_a_reused_corpse_slot_is_forgotten_by_whoever_looked_at_it(void)
{
    static const char map[] =
        "##########################\n"
        "#S  D              D    E#\n"
        "##########################\n"
        "\n"
        "SPAWNS 1 0\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 4711);
    REQUIRE(level_load_data(&state.level, "full floor", map, strlen(map),
                            &state.rng));
    player_reset(&state.player, &state.level);
    REQUIRE(state.level.map.door_count == 2);

    /* A full array: one man still standing and every other slot a body. The
     * live one is parked away from the doorway, because a live guard in it is
     * the other reason a spawn is refused. */
    const float floor_y = 1.0f * TILE_SIZE;
    for (int i = 0; i < MAX_ENEMIES; ++i)
    {
        enemy_init(&state.enemies[i], (float)(i + 1) * 10.0f, floor_y,
                   ENEMY_KIND_GUARD, &state.rng);
        state.enemies[i].dead = i != 0;
    }
    state.enemy_count = MAX_ENEMIES;
    state.enemies[0].x = 20.0f * TILE_SIZE;

    /* The furthest body from Chuck is the one the arrival takes, and he has
     * already been over to look at it — and at one other, which must survive. */
    const int reused = MAX_ENEMIES - 1;
    const int untouched = 5;
    state.enemies[0].bodies_investigated =
        enemy_body_bit(reused, false) | enemy_body_bit(untouched, false);

    state.door_spawns[0] = 1;
    state.door_timers[0] = 0.0f;
    gameplay_ai_update_spawns(&state, SIM_STEP_DT);

    /* No room was made: the arrival stood in a dead man's place. */
    CHECK(state.enemy_count == MAX_ENEMIES);
    CHECK(!state.enemies[reused].dead);
    /* And the memory of that dead man went with him. */
    CHECK((state.enemies[0].bodies_investigated &
           enemy_body_bit(reused, false)) == 0);
    /* Only his: a mask that cleared itself would disarm the rule outright,
     * which is the bug the mask replaced. */
    CHECK((state.enemies[0].bodies_investigated &
           enemy_body_bit(untouched, false)) != 0);
    CHECK(state.enemies[untouched].dead);
    (void)campaign;
}

/* A round is tested against the ground it crossed, not against where it landed.
 * Fired up a ladder it is 8px long and a dog is 16 tall, so one step at the
 * frame clamp carries it clean past the animal. */
static void test_a_fast_round_cannot_step_over_a_dog(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 55);
    static const char map[] =
        "######\n"
        "#    #\n"
        "#    #\n"
        "#S  E#\n"
        "######\n";
    CHECK(level_load_data(&state.level, "swept", map, strlen(map), &state.rng));

    dog_init(&state.dogs[0], 2.0f * TILE_SIZE, 1.5f * TILE_SIZE, -1,
             &state.rng);
    state.dog_count = 1;

    /* Sitting entirely below the animal and fired straight up. One step at the
     * frame clamp is 30px; the dog plus the round is 24, so the destination is
     * entirely above it and the two boxes never overlap at either end. */
    Bullet *bullet = &state.bullets[0];
    bullet->active = true;
    bullet->x = state.dogs[0].x + DOG_W * 0.5f - BULLET_H * 0.5f;
    bullet->y = state.dogs[0].y + DOG_H;
    bullet->vx = 0.0f;
    bullet->vy = -BULLET_SPEED;
    CHECK(BULLET_SPEED * MAX_FRAME_DT > DOG_H + BULLET_W);

    gameplay_combat_update_player_bullets(&state, &campaign, MAX_FRAME_DT);
    CHECK(!bullet->active);
    CHECK(state.dogs[0].dead);
}

/*
 * The duct, as far as the building is concerned.
 *
 * A shaft is worth taking because a guard cannot see into one and a round
 * cannot be put through one, and *that is not a feature of the shaft* — it is
 * `level_is_solid` answering the way it answers for masonry, which is why the
 * mechanic costs no special case in the AI, the ballistics or the lighting. So
 * this is the half worth pinning first: everything a wall does, a duct does.
 * The crawl through it is a separate question with a separate test, on the
 * other side of `level_blocks_stance`.
 *
 * It also pins the two places a duct is deliberately *not* a `%`: no blast
 * opens one, and nothing stands on one.
 */
static void test_a_duct_is_masonry_to_the_whole_building(void)
{
    static const char data[] =
        "###########\n"
        "#S   =   E#\n"
        "###########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 55);
    CHECK(level_load_data(&level, "duct", data, strlen(data), &rng));
    CHECK(level.map.tiles[1][5] == TILE_VENT);
    CHECK(level_is_solid(&level, 5, 1));

    /* Not a patch: an explosive has nothing to open here, and the tile is
     * still there afterwards. A duct that blew open would be a `%` with a
     * different picture. */
    CHECK(!level_break_wall(&level, 5, 1));
    CHECK(level_is_solid(&level, 5, 1));

    /* Nothing stands on a grille. The loader asks for a `#` under a prop and
     * the editor asks the same question, so a desk on a duct is a desk the
     * player never sees. */
    static const char propped[] =
        "#########\n"
        "#S  d  E#\n"
        "####=####\n"
        "#########\n";
    Level shelf;
    CHECK(level_load_data(&shelf, "duct shelf", propped, strlen(propped),
                          &rng));
    CHECK(shelf.map.decoration_count == 0);

    /* A guard does not see through one. Same geometry as the pillar in
     * `test_enemy_vision_cone_stealth_and_walls`, with the pillar swapped for
     * trunking — a standing Chuck six tiles off, past the notice beat, and no
     * aim raised. */
    static const char sightline[] =
        "#############\n"
        "#S  =    M E#\n"
        "#############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 321);
    CHECK(level_load_data(&state.level, "duct sight", sightline,
                          strlen(sightline), &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->dir = -1;
    guard->on_ground = true;
    guard->shoot_cooldown = 0.0f;
    guard->encounter_decided = true;
    guard->sight_timer = ENEMY_NOTICE_TIME;
    state.player.y = guard->y;
    state.player.x = guard->x - 6.0f * TILE_SIZE;
    state.player.crawling = false;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(guard->aim_timer == 0.0f);

    /* And a round stops on it. Fired at the trunking from a tile away, the
     * bullet is gone before it reaches the far side. */
    GameplayState firing = {0};
    CampaignState campaign = {0};
    rng_seed(&firing.rng, 11);
    CHECK(level_load_data(&firing.level, "duct stops a round", sightline,
                          strlen(sightline), &firing.rng));
    firing.player.hp = PLAYER_MAX_HP;
    firing.player.bullets = 1;
    firing.player.facing = 1;
    firing.player.on_ground = true;
    firing.player.x = 2.0f * TILE_SIZE;
    firing.player.y = (float)TILE_SIZE;
    Input shot = {.shoot = true};
    gameplay_combat_handle_player_action(&firing, &campaign, &shot);
    CHECK(firing.bullets[0].active);
    float duct_left = 4.0f * TILE_SIZE;
    for (int frame = 0; frame < SIM_STEPS(1.0f) && firing.bullets[0].active;
         ++frame)
    {
        gameplay_combat_update_player_bullets(&firing, &campaign, SIM_STEP_DT);
        /* Never past the far face of the trunking while still alive. */
        CHECK(!firing.bullets[0].active ||
              firing.bullets[0].x <= duct_left + (float)TILE_SIZE);
    }
    CHECK(!firing.bullets[0].active);
}

static void test_weak_wall_only_opens_to_a_blast(void)
{
    /* The patch is set into the standing row, which is the useful case: the
     * hole it leaves is exactly one tile tall, and the player box is exactly
     * one tile tall, so the opening is walked through rather than climbed. */
    static const char map[] =
        "############\n"
        "#S      % E#\n"
        "############\n";

    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 77);
    CHECK(level_load_data(&state.level, "weak-wall", map, strlen(map),
                          &state.rng));
    CHECK(state.level.map.tiles[1][8] == TILE_WEAK_WALL);
    CHECK(level_is_solid(&state.level, 8, 1));
    CHECK(!level_wall_broken(&state.level, 8, 1));
    player_reset(&state.player, &state.level);

    /* A pistol round stops on it and leaves it standing: a blocked-up opening
     * is a wall to everything except an explosion. */
    state.player.bullets = 1;
    state.player.active_weapon = PLAYER_WEAPON_PISTOL;
    state.player.facing = 1;
    Input input = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.bullets[0].active);
    for (int frame = 0; frame < SIM_STEPS(0.5f) && state.bullets[0].active; ++frame)
        gameplay_combat_update_player_bullets(&state, &campaign, SIM_STEP_DT);
    CHECK(!state.bullets[0].active);
    CHECK(level_is_solid(&state.level, 8, 1));
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_BULLET_IMPACT));
    CHECK(!events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                             SFX_WALL_BREAK));

    /* A grenade going off against it does open it, and says so. */
    game_events_clear(&state.events);
    int score_before = campaign.score;
    state.grenade_count = 1;
    state.grenades[0] = (Grenade){
        .x = 7.0f * TILE_SIZE + 8.0f,
        .y = 1.0f * TILE_SIZE + 12.0f,
        .active = true,
        .timer = 0.01f,
        .grounded = true};
    gameplay_combat_update_explosives(&state, &campaign, 0.02f);

    CHECK(level_wall_broken(&state.level, 8, 1));
    CHECK(!level_is_solid(&state.level, 8, 1));
    CHECK(campaign.score == score_before + WEAK_WALL_SCORE);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_WALL_BREAK));
    bool found_dust = false;
    for (int i = 0; i < state.events.count; ++i)
        found_dust |= state.events.items[i].type == GAME_EVENT_DUST;
    CHECK(found_dust);
    /* The blast was three tiles from the player, so opening a route is not
     * paid for with a life. */
    CHECK(!state.player.dying);

    /* The hole is a hole: the player walks the tile he could not walk before,
     * and a second blast has nothing left to take out. */
    state.player.x = 7.0f * TILE_SIZE;
    state.player.y = 1.0f * TILE_SIZE;
    float vx = 200.0f;
    float vy = 0.0f;
    bool on_ground = false;
    for (int frame = 0; frame < SIM_STEPS(0.5f); ++frame)
    {
        level_move(&state.level, &state.player.x, &state.player.y, &vx, &vy,
                   PLAYER_W, PLAYER_H, SIM_STEP_DT, false, &on_ground, false,
                   STANCE_UPRIGHT);
    }
    CHECK(state.player.x > 9.0f * TILE_SIZE);
    CHECK(gameplay_break_walls_in_radius(&state, &campaign,
                                         8.5f * TILE_SIZE,
                                         1.5f * TILE_SIZE,
                                         GRENADE_RADIUS) == 0);

    /* Loading the sector again brings the wall back: the hole is per-run state
     * and the map is still what the file says it is. */
    GameplayState reloaded = {0};
    rng_seed(&reloaded.rng, 77);
    CHECK(level_load_data(&reloaded.level, "weak-wall", map, strlen(map),
                          &reloaded.rng));
    CHECK(level_is_solid(&reloaded.level, 8, 1));
}

/*
 * The route model is the safety net under every sector — the editor asks it
 * while a map is being drawn and `make test` asks it of all seventeen — so a
 * duct it cannot see is a duct no shipped map may depend on, and a duct it
 * over-reads is worse: a sector it certifies that cannot actually be finished.
 *
 * A crawl is its own move rather than a cheaper kind of walking, and this pins
 * both halves of that. The shaft is crossed; and none of the moves an upright
 * player has come along with it, because `route_masonry` still answers wall to
 * every one of them. That last half is the one that would rot quietly: making
 * trunking `route_passable` would have got this test's first assertion passing
 * on its own, and handed a jump a foot of headroom inside a duct on the way.
 */
/*
 * The duct on the floor it ships on, driven rather than modelled.
 *
 * Everything else about trunking is checked against a hand-written map or
 * against `level_route.c`, and both are arguments about what *ought* to happen.
 * This one holds the pad down and walks: the sector the campaign actually ships,
 * the man where the map actually puts him, `player_update` and the crush pass in
 * the order `update_playing` runs them, until he is out the far side.
 *
 * It is here because the whole mechanic can be right in the predicate, right in
 * `level_move` and right in the route model and still be unplayable — the crush
 * pass alone would have taken a heart on the first step inside, and no test
 * above this one would have said a word. A shaft nobody has crawled is a shaft
 * nobody has checked.
 */
static void test_the_ducts_sector_can_actually_be_crawled_through(void)
{
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 1212);

    /* Sector 12, DUCTS: the floor the trunking is set into. */
    const EmbeddedLevelData *sector = &EMBEDDED_LEVELS[11];
    REQUIRE(level_load_data(&state.level, sector->name, sector->data,
                            sector->size, &state.rng));
    REQUIRE(state.level.map.theme == LEVEL_THEME_DUCTS);

    /* Find the *longest* run of trunking the sector was given, rather than
     * naming coordinates here: a map that moves its ducts must not quietly stop
     * being the thing this test drives. Longest rather than first, because the
     * first one found is wherever the top-left scan happens to land, and it took
     * a heart to learn that — a run starting two tiles from the left wall left
     * nowhere to put a man who has to walk *into* it. */
    int duct_row = -1, duct_left = -1, duct_right = -1;
    for (int row = 0; row < state.level.map.height; ++row)
    {
        int col = 0;
        while (col < state.level.map.width)
        {
            if (state.level.map.tiles[row][col] != TILE_VENT)
            {
                ++col;
                continue;
            }
            int start = col;
            while (col < state.level.map.width &&
                   state.level.map.tiles[row][col] == TILE_VENT)
            {
                ++col;
            }
            int length = col - start;
            /* Two tiles of standing approach on the left, so the walk in is a
             * walk: clear of masonry, and with floor under both of them. Left
             * out, the search picked a mouth with a hole beside it and the test
             * measured a man falling rather than a man stopped by trunking. */
            bool approach = start >= 3;
            for (int back = 1; back <= 2 && approach; ++back)
            {
                approach = !level_is_solid(&state.level, start - back, row) &&
                           level_is_solid(&state.level, start - back, row + 1);
            }
            if (approach && length > duct_right - duct_left + 1)
            {
                duct_row = row;
                duct_left = start;
                duct_right = col - 1;
            }
        }
    }
    REQUIRE(duct_row > 0);
    /* A run rather than a single tile, and long enough to be a crawl rather
     * than a step: the sector this drives is the one named after ducts. */
    CHECK(duct_right - duct_left + 1 >= 8);

    /* Stand him on the floor two tiles to the left of the mouth, at the height
     * the storey's own slab puts him. */
    player_reset(&state.player, &state.level);
    state.player.x = (float)(duct_left - 2) * TILE_SIZE;
    state.player.y = (float)duct_row * TILE_SIZE;
    state.player.on_ground = true;
    state.player.hp = PLAYER_MAX_HP;
    int hearts = state.player.hp;

    float far_side = (float)(duct_right + 1) * TILE_SIZE;
    bool through = false;
    /* Down for the crawl, right to move: the two keys a player would hold. */
    Input crawl_right = {.down = true, .right = true};
    for (int step = 0; step < SIM_STEPS(12.0f) && !through; ++step)
    {
        player_update(&state.player, &state.level, &crawl_right, SIM_STEP_DT);
        gameplay_resolve_player_crush(&state);
        /* No heart may be spent getting through. The crush pass is what would
         * have taken one, silently, on the frame he entered. */
        CHECK(state.player.hp == hearts);
        if (state.player.x >= far_side)
            through = true;
    }
    CHECK(through);
    CHECK(state.player.crawling);

    /* And the same walk on his feet does not get through: he is stopped on the
     * near face of the trunking, which is what makes the shaft a decision
     * rather than a corridor. */
    player_reset(&state.player, &state.level);
    state.player.x = (float)(duct_left - 2) * TILE_SIZE;
    state.player.y = (float)duct_row * TILE_SIZE;
    state.player.on_ground = true;
    state.player.hp = PLAYER_MAX_HP;
    Input walk_right = {.right = true};
    for (int step = 0; step < SIM_STEPS(12.0f); ++step)
    {
        player_update(&state.player, &state.level, &walk_right, SIM_STEP_DT);
        gameplay_resolve_player_crush(&state);
    }
    CHECK(!state.player.crawling);
    CHECK(state.player.x + PLAYER_W <= (float)duct_left * TILE_SIZE + 0.01f);
}

static void test_the_route_model_crawls_a_duct_and_nothing_else(void)
{
    static RouteMap route;
    Rng rng;
    rng_seed(&rng, 404);

    /* The way out is through the trunking, and the model finds it. */
    static const char through[] =
        "###########\n"
        "#S   =   E#\n"
        "###########\n";
    Level crossed;
    CHECK(level_load_data(&crossed, "duct crossed", through, strlen(through),
                          &rng));
    route_map_init(&route, &crossed);
    route_flood(&route, route_player_start(&route));
    CHECK(route_reaches(&route, crossed.map.exit_col, crossed.map.exit_row));

    /* A duct with no floor at its far mouth is crawled into and not out of, so
     * whatever is past it stays unreached. A shaft is a route between two
     * places somebody can stand, and nothing else. */
    static const char blind[] =
        "###########\n"
        "#S   =    #\n"
        "#####  ####\n"
        "#       E #\n"
        "###########\n";
    Level dead_end;
    CHECK(level_load_data(&dead_end, "duct blind", blind, strlen(blind), &rng));
    route_map_init(&route, &dead_end);
    route_flood(&route, route_player_start(&route));
    CHECK(route_reaches(&route, 5, 1));
    CHECK(!route_reaches(&route, dead_end.map.exit_col,
                         dead_end.map.exit_row));

    /* And trunking is still a ceiling. This map's only way on is a two-tile
     * hole hop, which needs a clear row over the jump — and that row is duct.
     * The model has to refuse it: a man in a duct is on his elbows, and a man
     * on his elbows is not jumping anything.
     */
    static const char lidded[] =
        "###########\n"
        "#   ===   #\n"
        "#S  # #  E#\n"
        "###########\n";
    Level low;
    CHECK(level_load_data(&low, "duct headroom", lidded, strlen(lidded), &rng));
    route_map_init(&route, &low);
    route_flood(&route, route_player_start(&route));
    CHECK(!route_reaches(&route, low.map.exit_col, low.map.exit_row));
}

static void test_weak_wall_is_masonry_to_the_route_model(void)
{
    /* Judged as authored: a way out behind a patch is a way out the model
     * cannot reach, because opening it costs an explosive the model knows
     * nothing about. */
    static const char sealed[] =
        "###########\n"
        "#S   %   E#\n"
        "###########\n";
    static RouteMap route;
    Level level;
    Rng rng;
    rng_seed(&rng, 91);
    CHECK(level_load_data(&level, "weak-wall route", sealed, strlen(sealed),
                          &rng));
    route_map_init(&route, &level);
    route_flood(&route, route_player_start(&route));
    CHECK(!route_reaches(&route, level.map.exit_col, level.map.exit_row));

    /* And it is floor: a patch set into a slab must not cut the storey it is
     * part of in two. */
    static const char floor_patch[] =
        "#########\n"
        "#S     E#\n"
        "###%#####\n";
    CHECK(level_load_data(&level, "weak-wall floor", floor_patch,
                          strlen(floor_patch), &rng));
    route_map_init(&route, &level);
    CHECK(route_standing(&route, 3, 1));
    route_flood(&route, route_player_start(&route));
    CHECK(route_reaches(&route, level.map.exit_col, level.map.exit_row));
}

/*
 * The model is the safety net under every sector, and it was conservative
 * about everything except the one thing that kills outright. `route_landing`
 * walked a column down with no limit at all, so a fall of any depth was a move
 * the player could make — and both `make test` and the editor would certify a
 * sector whose only way to a card or the door was a drop nobody survives.
 *
 * A landing past PLAYER_FATAL_FALL_HEIGHT is not a route. Hearts do not enter
 * into it: a fatal fall calls gameplay_hit_player directly.
 */
static void test_the_route_model_will_not_take_a_fatal_fall(void)
{
    /* The drop this is all measured against, so the maps below cannot quietly
     * stop straddling it if the physics is retuned. */
    const int fatal_rows = (int)(PLAYER_FATAL_FALL_HEIGHT / TILE_SIZE);
    CHECK(fatal_rows >= 3 && fatal_rows <= 12);

    /* A ledge over a shaft with the way out at the bottom of it. Stepping off
     * that ledge is the only move the map offers, and the drop is eight rows —
     * comfortably past a landing anybody survives. */
    static const char killer[] =
        "#####\n"
        "#S ##\n"
        "## ##\n"
        "## ##\n"
        "## ##\n"
        "## ##\n"
        "## ##\n"
        "## ##\n"
        "## ##\n"
        "##E##\n"
        "#####\n";
    static RouteMap route;
    Level level;
    Rng rng;
    rng_seed(&rng, 4242);
    CHECK(level_load_data(&level, "fatal drop", killer, strlen(killer), &rng));
    /* The map only poses the question while eight rows really is past the
     * line, which a retuned gravity could quietly undo. */
    CHECK(8 > fatal_rows);
    route_map_init(&route, &level);
    route_flood(&route, route_player_start(&route));
    CHECK(!route_reaches(&route, level.map.exit_col, level.map.exit_row));

    /* The cap belongs to the step off a ledge alone. route_landing still walks
     * the whole shaft, because its other callers are not falls the player
     * takes: a card hanging in the air is collected off the floor under it,
     * and the map's own S settles onto the floor beneath wherever it was
     * drawn. */
    RouteCell landing;
    CHECK(route_landing(&route, 2, 1, &landing));
    CHECK(landing.row == 9);
    CHECK(!route_survivable_fall(1, landing.row));

    /* The same shaft with its floor brought up inside the survivable drop is
     * a route again: the rule is the height, not a refusal to fall. */
    static const char survivable[] =
        "#####\n"
        "#S ##\n"
        "## ##\n"
        "##E##\n"
        "#####\n";
    CHECK(level_load_data(&level, "short drop", survivable, strlen(survivable),
                          &rng));
    CHECK(2 <= fatal_rows);
    route_map_init(&route, &level);
    route_flood(&route, route_player_start(&route));
    CHECK(route_reaches(&route, level.map.exit_col, level.map.exit_row));
    CHECK(route_survivable_fall(1, 3));
}

/* A probe floor for the test below: one gap in a flat run, the way out on the
 * far side, and two open rows over the walk row — which is what the model's own
 * two-tile hop asks for. `hazard` fills the gap with spikes instead of air. */
static void route_probe_build(char *out, size_t size, int hole, char hazard)
{
    const int width = 20;
    const int height = 5;
    const int walk = height - 2;
    const int floor = height - 1;
    size_t at = 0;
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            char c = ' ';
            if (row == 0 || col == 0 || col == width - 1)
                c = '#';
            else if (row == floor)
                c = (hazard == 0 && col >= 8 && col < 8 + hole) ? ' ' : '#';
            else if (row == walk)
            {
                if (col == 2)
                    c = 'S';
                else if (col == width - 3)
                    c = 'E';
                else if (hazard != 0 && col >= 8 && col < 8 + hole)
                    c = hazard;
            }
            if (at + 1 < size)
                out[at++] = c;
        }
        if (at + 1 < size)
            out[at++] = '\n';
    }
    out[at] = '\0';
}

/* The best a hand could do: run right, and press jump on the last frame there
 * is still something under the boots. */
static bool route_probe_walk_across(GameplayState *state, int hole,
                                    bool *unhurt)
{
    const float far_side = (float)((8 + hole) * TILE_SIZE);
    int hearts = state->player.hp;
    for (int step = 0; step < SIM_STEPS(8.0f); ++step)
    {
        Input in = {0};
        in.right = true;
        int foot_col = (int)floorf((state->player.x + PLAYER_W + 2.0f) / TILE_SIZE);
        int foot_row = (int)floorf((state->player.y + PLAYER_H + 2.0f) / TILE_SIZE);
        if (state->player.on_ground &&
            !level_is_solid(&state->level, foot_col, foot_row))
        {
            in.jump = true;
            in.jump_held = true;
        }
        else if (!state->player.on_ground)
            in.jump_held = true;
        player_update(&state->player, &state->level, &in, SIM_STEP_DT);
        gameplay_combat_update_hazards(state);
        if (state->player.hp < hearts)
            *unhurt = false;
        if (state->player.x > far_side && state->player.on_ground)
            return true;
    }
    return false;
}

/* Whether the model will walk a probe map, which is the promise under test. */
static bool route_probe_model_crosses(const Level *level)
{
    static RouteMap route;
    route_map_init(&route, level);
    route_flood(&route, route_player_start(&route));
    return route_reaches(&route, level->map.exit_col, level->map.exit_row);
}

/*
 * Nothing the model promises is a move the man cannot make.
 *
 * The route model is what certifies every shipped map: `route_reaches` is how
 * the suite knows a card can be got to and how the editor knows a floor can be
 * finished. Two of its rules are *claims about the player's body* written as
 * literals beside it — a hole is clearable one tile wide, or two with a row
 * spare overhead, and a spike bed is hoppable one tile wide — and neither was
 * tied to the physics that has to deliver them. `PLAYER_JUMP_SPEED`,
 * `PLAYER_WALK_SPEED` and `GRAVITY` could all be retuned and the model would go
 * on promising the same two tiles: a floor the player cannot cross, certified
 * playable by the test suite, is the one failure this model exists to prevent.
 *
 * Measured the day this was written, the margin is one tile in each direction:
 * the physics clears a **three**-tile hole with the jump pressed at the lip, and
 * a two-tile spike bed can be crossed for one heart. So the model is the
 * conservative side of the truth, which is the side to be on.
 *
 * **The widths are asked of the model rather than written down here**, which is
 * the difference between this test and the first draft of it. Hard-coded at one
 * and two tiles it pinned today's literals: widening `route_neighbours` to a
 * three-tile hop — a defensible change, since the body can do it — went
 * unnoticed, and so would widening the *spike* hop to two, which the body
 * cannot do without paying a heart. Asked instead, the test tracks whatever the
 * model claims: every width it calls reachable is a width the simulation has to
 * deliver, and the day somebody widens it past the jump the failure lands on the
 * change rather than on a player.
 */
static void test_the_route_model_promises_only_moves_the_player_can_make(void)
{
    /* Holes, widening until the model refuses. Every width it accepts is one the
     * simulation has to walk; the first it refuses ends the sweep, because a
     * model that stops promising owes nothing. */
    int holes_promised = 0;
    for (int hole = 1; hole <= 6; ++hole)
    {
        static char data[512];
        route_probe_build(data, sizeof(data), hole, 0);

        static GameplayState state;
        memset(&state, 0, sizeof(state));
        rng_seed(&state.rng, 7001 + hole);
        REQUIRE(level_load_data(&state.level, "hole", data, strlen(data),
                                &state.rng));
        player_reset(&state.player, &state.level);
        if (!route_probe_model_crosses(&state.level))
            continue;

        holes_promised++;
        bool unhurt = true;
        CHECK(route_probe_walk_across(&state, hole, &unhurt));
        /* Across, on his feet, and for nothing: a hole is not a hazard, so a
         * heart lost here would mean the landing itself was costing one. */
        CHECK(unhurt);
        CHECK(state.player.y < (float)(state.level.map.height * TILE_SIZE));
    }
    /* Two today. Nought would mean the probe map stopped being a hole and this
     * test stopped asking anything, which is the failure mode of every sweep in
     * this repository. */
    CHECK(holes_promised >= 2);

    /* And the spike hop, which is the same claim with a heart on it: a bed the
     * model walks over has to be clearable without paying for it. The press is
     * swept because *when* to jump is the player's half, and one hand-picked
     * moment proves nothing about the map. */
    int beds_promised = 0;
    for (int bed = 1; bed <= 4; ++bed)
    {
        static char spikes[512];
        route_probe_build(spikes, sizeof(spikes), bed, '^');

        static GameplayState probe;
        memset(&probe, 0, sizeof(probe));
        rng_seed(&probe.rng, 7100 + bed);
        REQUIRE(level_load_data(&probe.level, "spikes", spikes, strlen(spikes),
                                &probe.rng));
        REQUIRE(probe.level.map.spike_count == bed);
        if (!route_probe_model_crosses(&probe.level))
            continue;

        beds_promised++;
        int crossed_unhurt = 0;
        for (int lead = 0; lead <= 96; lead += 2)
        {
            static GameplayState state;
            memset(&state, 0, sizeof(state));
            rng_seed(&state.rng, 7100 + bed);
            REQUIRE(level_load_data(&state.level, "spikes", spikes,
                                    strlen(spikes), &state.rng));
            player_reset(&state.player, &state.level);

            const float bed_x = (float)(8 * TILE_SIZE);
            const float far_side = bed_x + (float)(bed * TILE_SIZE);
            bool jumped = false;
            int hearts = state.player.hp;
            for (int step = 0; step < SIM_STEPS(8.0f); ++step)
            {
                Input in = {0};
                in.right = true;
                if (!jumped && state.player.on_ground &&
                    state.player.x + PLAYER_W >= bed_x - (float)lead)
                {
                    in.jump = true;
                    in.jump_held = true;
                    jumped = true;
                }
                else if (!state.player.on_ground)
                    in.jump_held = true;
                player_update(&state.player, &state.level, &in, SIM_STEP_DT);
                gameplay_combat_update_hazards(&state);
                if (state.player.x > far_side && state.player.on_ground)
                {
                    if (state.player.hp == hearts)
                        crossed_unhurt++;
                    break;
                }
            }
            if (crossed_unhurt > 0)
                break;
        }
        CHECK(crossed_unhurt > 0);
    }
    CHECK(beds_promised >= 1);
}

static void test_empty_pistol_uses_close_range_knife(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.x = 100.0f;
    state.player.y = 64.0f;
    state.player.facing = 1;
    state.player.bullets = 0;

    float attack_edge = state.player.x + PLAYER_W;
    state.enemy_count = 3;
    state.enemies[0] = (Enemy){
        .x = attack_edge + PLAYER_KNIFE_RANGE - 1.0f,
        .y = state.player.y,
        .dir = -1,
        .hp = ENEMY_HP};
    state.enemies[1] = (Enemy){
        .x = attack_edge + PLAYER_KNIFE_RANGE,
        .y = state.player.y,
        .dir = -1,
        .hp = ENEMY_HP};
    state.enemies[2] = (Enemy){
        .x = state.player.x - ENEMY_W,
        .y = state.player.y,
        .dir = 1,
        .hp = ENEMY_HP};
    state.dog_count = 1;
    state.dogs[0] = (Dog){
        .x = attack_edge + 2.0f,
        .y = state.player.y + PLAYER_H - DOG_H,
        .dir = -1,
        .hp = DOG_HP};

    Input input = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);

    CHECK(!input.shoot);
    CHECK(state.player.bullets == 0);
    CHECK(state.player.knife_attacking);
    CHECK(state.player.action_timer == PLAYER_KNIFE_ACTION_TIME);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);
    CHECK(state.enemies[1].hp == ENEMY_HP);
    CHECK(state.enemies[2].hp == ENEMY_HP);
    CHECK(state.dogs[0].dead);
    CHECK(campaign.score == 75);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_KNIFE_SWING));
    CHECK(!events_have_sound(&state.events, GAME_EVENT_SOUND,
                             SFX_EMPTY_CLICK));
    for (int i = 0; i < MAX_BULLETS; ++i)
        CHECK(!state.bullets[i].active);
}

static void test_ladder_knife_attacks_in_aimed_direction(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.x = 100.0f;
    state.player.y = 64.0f;
    state.player.facing = 1;
    state.player.bullets = 0;
    state.player.on_ladder = true;
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){
        .x = state.player.x + PLAYER_W + 2.0f,
        .y = state.player.y,
        .dir = -1,
        .hp = ENEMY_HP};

    Input input = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.knife_attacking);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_KNIFE_SWING));

    for (int vertical = -1; vertical <= 1; vertical += 2)
    {
        state.player.knife_attacking = false;
        state.player.action_timer = 0.0f;
        state.player.shot_vertical = 0;
        state.enemy_count = 2;
        state.enemies[0] = (Enemy){
            .x = state.player.x,
            .y = vertical < 0
                     ? state.player.y - ENEMY_H - 1.0f
                     : state.player.y + PLAYER_H + 2.0f,
            .dir = -1,
            .hp = ENEMY_HP};
        state.enemies[1] = (Enemy){
            .x = state.player.x + PLAYER_W + 2.0f,
            .y = state.player.y,
            .dir = -1,
            .hp = ENEMY_HP};
        game_events_clear(&state.events);
        input = (Input){
            .up = vertical < 0,
            .down = vertical > 0,
            .shoot = true};

        gameplay_combat_handle_player_action(&state, &campaign, &input);

        CHECK(!input.shoot);
        CHECK(state.player.knife_attacking);
        CHECK(state.player.action_timer == PLAYER_KNIFE_ACTION_TIME);
        CHECK(state.player.shot_vertical == vertical);
        CHECK(state.enemies[0].hp == ENEMY_HP - 1);
        CHECK(state.enemies[1].hp == ENEMY_HP);
        CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                                SFX_KNIFE_SWING));
    }
}

static void test_crate_movement_emits_sounds(void)
{
    static const char data[] =
        "########\n"
        "#  B   #\n"
        "#      #\n"
        "#S M  E#\n"
        "########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 87);
    CHECK(level_load_data(&state.level, "crate", data, strlen(data),
                          &state.rng));
    CHECK(state.level.runtime.crate_count == 1);
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Crate *crate = &state.level.runtime.crates[0];

    for (int i = 0; i < SIM_STEPS(2.0f) && !state.enemies[0].dead; ++i)
        gameplay_update_crates(&state, &campaign, SIM_STEP_DT);
    CHECK(state.enemies[0].dead);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_ENEMY_DOWN));
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_CRATE_LAND));

    for (int i = 0; i < SIM_STEPS(2.0f) && !crate->on_ground; ++i)
        gameplay_update_crates(&state, &campaign, SIM_STEP_DT);
    CHECK(crate->on_ground);

    game_events_clear(&state.events);
    state.player.x = crate->x - PLAYER_W + 4.0f;
    state.player.y = 4.0f * TILE_SIZE - PLAYER_H;
    state.player.vx = PLAYER_WALK_SPEED;
    gameplay_resolve_player_crates(&state,
                                   crate->x - PLAYER_W,
                                   state.player.y, PLAYER_H);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_CRATE_PUSH));
}

/*
 * The half of the crate that is a floor, a wall and a brake.
 *
 * `gameplay_physics.c` carried the weakest branch coverage in the tree while a
 * `B` stood on ten of the twelve interiors, and what was never executed was not
 * an edge case: it was standing on one, shoving one into another, and the
 * friction that decides where a shoved one stops. The tests that existed pushed
 * a crate into a guard and dropped one on a dog — the crate as a weapon — and
 * nothing had ever asked about the crate as a *step*, which is what the maps
 * actually use it for.
 *
 * Driven at `SIM_STEP_DT` in the order `update_playing` runs them, because a
 * landing is a comparison against the previous frame's box and a test that
 * banks a different previous frame is testing its own harness.
 */
static void step_player_over_crates(GameplayState *state,
                                    CampaignState *campaign, Input *input)
{
    float previous_x = state->player.x;
    float previous_y = state->player.y;
    float previous_h = state->player.crawling ? (float)PLAYER_CRAWL_H
                                              : (float)PLAYER_H;
    player_update(&state->player, &state->level, input, SIM_STEP_DT);
    gameplay_update_crates(state, campaign, SIM_STEP_DT);
    gameplay_resolve_player_crates(state, previous_x, previous_y, previous_h);
}

static void test_a_crate_is_a_floor_a_wall_and_a_brake(void)
{
    static const char data[] =
        "##############\n"
        "#            #\n"
        "#            #\n"
        "#S   B     BE#\n"
        "##############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 4242);
    REQUIRE(level_load_data(&state.level, "crate floor", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.runtime.crate_count == 2);
    player_reset(&state.player, &state.level);

    Crate *near = &state.level.runtime.crates[0];
    Crate *far = &state.level.runtime.crates[1];
    Input idle = {0};

    /* Let both settle onto the floor before anything is measured. */
    for (int i = 0; i < SIM_STEPS(1.0f); ++i)
        gameplay_update_crates(&state, &campaign, SIM_STEP_DT);
    REQUIRE(near->on_ground);
    REQUIRE(far->on_ground);
    float near_start = near->x;
    float far_start = far->x;

    /* Dropped onto a crate, Chuck stands on its lid rather than through it —
     * and the crate does not squirt out from under him. */
    state.player.x = near->x + (CRATE_W - PLAYER_W) * 0.5f;
    state.player.y = near->y - PLAYER_H - 12.0f;
    state.player.vy = 0.0f;
    state.player.on_ground = false;
    for (int i = 0; i < SIM_STEPS(1.5f); ++i)
        step_player_over_crates(&state, &campaign, &idle);
    CHECK(state.player.on_ground);
    CHECK(fabsf(state.player.y - (near->y - PLAYER_H)) < 1.0f);
    CHECK(fabsf(near->x - near_start) < 1.0f);
    CHECK(near->on_ground);

    /* Walked into from the side, it is shoved rather than walked through, and
     * it stops on the second crate rather than in it: two boxes cannot occupy
     * the same ground, which is the branch that keeps a stack of them honest. */
    state.player.x = near_start - PLAYER_W - 2.0f;
    state.player.y = near->y + CRATE_H - PLAYER_H;
    state.player.vx = 0.0f;
    state.player.vy = 0.0f;
    state.player.on_ground = true;
    Input walk_right = {.right = true};
    for (int i = 0; i < SIM_STEPS(8.0f); ++i)
        step_player_over_crates(&state, &campaign, &walk_right);

    CHECK(near->x > near_start + TILE_SIZE);
    CHECK(fabsf(far->x - far_start) < 1.0f);
    CHECK(near->x + CRATE_W <= far->x + 0.5f);
    CHECK(!gameplay_boxes_overlap(near->x, near->y, CRATE_W, CRATE_H,
                                  far->x, far->y, CRATE_W, CRATE_H));
    /* And the player is outside the box he has been leaning on the whole time,
     * rather than folded into it. */
    CHECK(!gameplay_boxes_overlap(state.player.x, state.player.y, PLAYER_W,
                                  PLAYER_H, near->x, near->y, CRATE_W,
                                  CRATE_H));

    /* Let go and it coasts to a stop instead of sliding for ever: the friction
     * branch, which nothing had run either. Put back on open floor first,
     * because the box it was just shoved against is a wall and a wall answers
     * the question with the wrong branch. */
    near->x = near_start;
    near->vx = CRATE_PUSH_SPEED;
    state.player.x = 2.0f * TILE_SIZE;
    state.player.y = near->y - PLAYER_H;
    float coasting = near->x;
    for (int i = 0; i < SIM_STEPS(3.0f); ++i)
        gameplay_update_crates(&state, &campaign, SIM_STEP_DT);
    CHECK(near->vx == 0.0f);
    CHECK(near->x > coasting);
    /* And it stopped short of where it was headed rather than at the far box:
     * friction, not a collision. */
    CHECK(near->x + CRATE_W < far->x - 1.0f);
}

/*
 * And a dog meets the same box.
 *
 * `gameplay_resolve_dog_crates` was executed by nothing at all — not one of its
 * branches — while `W` puts a handler and an animal on eleven floors and every
 * one of those floors has crates on it. A dog that walked through a crate would
 * have been a stealth route quietly not working, since a crate parked in a
 * doorway is one of the two things a player can do about an animal.
 */
static void test_a_dog_is_stopped_by_a_crate(void)
{
    static const char data[] =
        "############\n"
        "#          #\n"
        "#S  B    WE#\n"
        "############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 515);
    REQUIRE(level_load_data(&state.level, "dog crate", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.runtime.crate_count == 1);
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.dog_count == 1);
    player_reset(&state.player, &state.level);

    Crate *crate = &state.level.runtime.crates[0];
    for (int i = 0; i < SIM_STEPS(1.0f); ++i)
        gameplay_update_crates(&state, &campaign, SIM_STEP_DT);
    REQUIRE(crate->on_ground);

    /* Sent at the crate from the right, at the height a dog runs at. */
    Dog *dog = &state.dogs[0];
    dog->x = crate->x + CRATE_W + 6.0f;
    dog->y = crate->y + CRATE_H - DOG_H;
    dog->on_ground = true;
    dog->dir = -1;

    for (int i = 0; i < SIM_STEPS(1.0f); ++i)
    {
        float previous_x = dog->x;
        float previous_y = dog->y;
        dog->x -= 60.0f * SIM_STEP_DT;
        gameplay_resolve_dog_crates(&state, dog, previous_x, previous_y);
    }

    CHECK(!gameplay_boxes_overlap(dog->x, dog->y, DOG_W, DOG_H,
                                  crate->x, crate->y, CRATE_W, CRATE_H));
    CHECK(dog->x >= crate->x + CRATE_W - 0.5f);
    /* Turned by it rather than merely stopped: a dog nosing a crate for ever is
     * an animal that has stopped being a threat and has not said so. */
    CHECK(dog->dir == 1);

    /* Dropped onto the lid, it stands on it, which is the other half of the
     * same resolver. */
    dog->x = crate->x + (CRATE_W - DOG_W) * 0.5f;
    dog->y = crate->y - DOG_H - 4.0f;
    dog->vy = 120.0f;
    dog->on_ground = false;
    for (int i = 0; i < SIM_STEPS(1.0f) && !dog->on_ground; ++i)
    {
        float previous_x = dog->x;
        float previous_y = dog->y;
        dog->vy += GRAVITY * SIM_STEP_DT;
        dog->y += dog->vy * SIM_STEP_DT;
        gameplay_resolve_dog_crates(&state, dog, previous_x, previous_y);
    }
    CHECK(dog->on_ground);
    CHECK(fabsf(dog->y - (crate->y - DOG_H)) < 1.0f);
    CHECK(dog->vy == 0.0f);
}

/*
 * A blast takes the boxes beside it with everything else.
 *
 * `gameplay_destroy_crate` had never been executed by the suite, which is how a
 * function with two callers and a score attached goes untested: the crate tests
 * push crates and drop them on people, and the explosive tests count men and
 * walls. Nothing had ever put the two in the same room. It matters to the route
 * rather than to the points — a crate is cover and a step up, and a player who
 * opens a `%` with a rocket has often just removed the thing they were going to
 * stand on.
 */
static void test_a_blast_breaks_the_crates_it_reaches(void)
{
    static const char data[] =
        "##############\n"
        "#S B      B E#\n"
        "##############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 1212);
    REQUIRE(level_load_data(&state.level, "blast crates", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.runtime.crate_count == 2);
    Crate *near = &state.level.runtime.crates[0];
    Crate *far = &state.level.runtime.crates[1];
    REQUIRE(near->active && far->active);

    /* On top of the near one, and nowhere near the far one. A fuse short
     * enough to go off inside one step, because a grenade falls while it burns
     * and a long step would drop it through the floor before it did. */
    state.grenade_count = 1;
    state.grenades[0] = (Grenade){
        .x = near->x + CRATE_W * 0.5f,
        .y = near->y + CRATE_H * 0.5f,
        .active = true,
        .timer = SIM_STEP_DT * 0.5f};

    gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);

    CHECK(!state.grenades[0].active);
    CHECK(!near->active);
    /* A blast has a radius, and the other side of the room is outside it. */
    CHECK(far->active);
    CHECK(campaign.score == CRATE_SCORE);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_CRATE_BREAK));
    /* Broken means gone rather than parked: a crate still carrying velocity
     * would be shoved about by the next thing that touched it. */
    CHECK(near->vx == 0.0f);
    CHECK(near->vy == 0.0f);
}

/*
 * And a crate dropped on the animal kills the animal.
 *
 * `gameplay_kill_dog_with_crate` is the guard's twin one line down in
 * `resolve_falling_crate_hits`, and the twin was tested while this was not —
 * the same asymmetry `find_dog_slot` was in, and the same fix. A dog crushed
 * has to count on both tallies and leave a body, because the body is what the
 * next calm guard walks over to look at.
 */
static void test_a_falling_crate_kills_the_dog_under_it(void)
{
    static const char data[] =
        "##########\n"
        "#  B     #\n"
        "#        #\n"
        "#S W    E#\n"
        "##########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 1313);
    REQUIRE(level_load_data(&state.level, "crate dog", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.level.runtime.crate_count == 1);
    REQUIRE(state.dog_count == 1);
    REQUIRE(state.enemy_count == 1);

    Crate *crate = &state.level.runtime.crates[0];
    Dog *dog = &state.dogs[0];
    /* The handler out of the way, so what the box lands on is the dog and the
     * guard's own path through `gameplay_kill_enemy_with_crate` is not what
     * this is measuring. */
    state.enemies[0].x = 7.0f * TILE_SIZE;
    dog->x = crate->x + (CRATE_W - DOG_W) * 0.5f;
    dog->y = 3.0f * TILE_SIZE - DOG_H;
    dog->on_ground = true;

    for (int step = 0; step < SIM_STEPS(2.0f) && !dog->dead; ++step)
        gameplay_update_crates(&state, &campaign, SIM_STEP_DT);

    CHECK(dog->dead);
    CHECK(dog->hp == 0);
    CHECK(!state.enemies[0].dead);
    CHECK(campaign.score == DOG_SCORE);
    /* Both counters, because they answer different questions: the floor's is
     * what the report prints and the run's is what the crew's net reads. */
    CHECK(state.hostiles_neutralized == 1);
    CHECK(campaign.hostiles_down == 1);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_DOG_YELP));
}

static void test_crate_stops_at_enemy_and_triggers_counterattack(void)
{
    static const char data[] =
        "#########\n"
        "#S B M E#\n"
        "#########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 314);
    CHECK(level_load_data(&state.level, "crate enemy", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.level.runtime.crate_count == 1);
    CHECK(state.enemy_count == 1);

    Crate *crate = &state.level.runtime.crates[0];
    Enemy *enemy = &state.enemies[0];
    crate->x = enemy->x - CRATE_W - 2.0f;
    float enemy_x = enemy->x;
    float crate_contact_x = enemy->x - CRATE_W;

    state.player.x = crate->x - PLAYER_W + 4.0f;
    state.player.y = enemy->y;
    state.player.vx = PLAYER_WALK_SPEED;
    gameplay_resolve_player_crates(&state,
                                   crate->x - PLAYER_W,
                                   state.player.y, PLAYER_H);

    CHECK(fabsf(crate->x - crate_contact_x) < 0.01f);
    CHECK(fabsf(crate->vx) < 0.01f);
    CHECK(fabsf(enemy->x - enemy_x) < 0.01f);
    CHECK(enemy->provoked);
    CHECK(enemy->dir == -1);
    CHECK(fabsf(enemy->aim_timer - ENEMY_AIM_TIME) < 0.01f);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_ENEMY_ALERT));

    game_events_clear(&state.events);
    enemy->aim_timer = ENEMY_AIM_TIME * 0.5f;
    state.player.x = crate->x - PLAYER_W + 4.0f;
    state.player.vx = PLAYER_WALK_SPEED;
    gameplay_resolve_player_crates(&state,
                                   crate->x - PLAYER_W,
                                   state.player.y, PLAYER_H);

    CHECK(fabsf(crate->x - crate_contact_x) < 0.01f);
    CHECK(fabsf(enemy->x - enemy_x) < 0.01f);
    CHECK(fabsf(enemy->aim_timer - ENEMY_AIM_TIME * 0.5f) < 0.01f);
    CHECK(!events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                             SFX_ENEMY_ALERT));
}

static void test_enemy_walks_in_front_of_unjumpable_crate(void)
{
    static const char data[] =
        "##########\n"
        "#S B  M E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 2718);
    CHECK(level_load_data(&state.level, "enemy crate route", data,
                          strlen(data), &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.level.runtime.crate_count == 1);
    CHECK(state.enemy_count == 1);

    Crate *crate = &state.level.runtime.crates[0];
    Enemy *enemy = &state.enemies[0];
    enemy->dir = -1;
    enemy->on_ground = true;
    enemy->aim_timer = 0.0f;
    enemy->provoked = true;
    enemy->has_pursuit_target = true;
    enemy->pursuit_target_x = state.player.x + PLAYER_W * 0.5f;
    enemy->pursuit_target_y = enemy->y + ENEMY_H * 0.5f;

    bool overlapped_crate = false;
    bool jumped = false;
    for (int frame = 0; frame < SIM_STEPS(6.0f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (enemy->vy < 0.0f)
            jumped = true;
        if (enemy->x < crate->x + CRATE_W &&
            enemy->x + ENEMY_W > crate->x)
            overlapped_crate = true;
        if (enemy->x + ENEMY_W <= crate->x)
            break;
    }

    CHECK(overlapped_crate);
    CHECK(!jumped);
    CHECK(enemy->x + ENEMY_W <= crate->x);
    CHECK(enemy->dir < 0);
}

static void test_enemy_climbs_over_blocking_crate(void)
{
    static const char data[] =
        "##########\n"
        "#        #\n"
        "#S B  M E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 2719);
    CHECK(level_load_data(&state.level, "enemy crate jump", data,
                          strlen(data), &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.level.runtime.crate_count == 1);
    CHECK(state.enemy_count == 1);

    Crate *crate = &state.level.runtime.crates[0];
    Enemy *enemy = &state.enemies[0];
    crate->on_ground = true;
    enemy->dir = -1;
    enemy->on_ground = true;
    enemy->aim_timer = 0.0f;
    enemy->provoked = true;
    enemy->has_pursuit_target = true;
    enemy->pursuit_target_x = state.player.x + PLAYER_W * 0.5f;
    enemy->pursuit_target_y = enemy->y + ENEMY_H * 0.5f;
    /* The first route roll is 11, selecting the mount branch. */
    rng_seed(&state.rng, 2);

    bool jumped = false;
    bool landed_on_crate = false;
    bool landed_beyond_crate = false;
    for (int frame = 0; frame < SIM_STEPS(6.0f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (enemy->vy < 0.0f)
            jumped = true;
        bool overlaps_crate =
            enemy->x < crate->x + CRATE_W &&
            enemy->x + ENEMY_W > crate->x;
        if (jumped && enemy->on_ground && overlaps_crate &&
            fabsf(enemy->y + ENEMY_H - crate->y) < 0.01f)
            landed_on_crate = true;
        if (landed_on_crate && enemy->on_ground &&
            enemy->x + ENEMY_W <= crate->x + 0.01f)
        {
            landed_beyond_crate = true;
            break;
        }
    }

    CHECK(jumped);
    CHECK(landed_on_crate);
    CHECK(landed_beyond_crate);
    CHECK(enemy->obstacle_avoid_timer == 0.0f);
}

static void test_patrol_enemy_may_walk_in_front_of_jumpable_crate(void)
{
    static const char data[] =
        "##########\n"
        "#        #\n"
        "#S B  M E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 2720);
    CHECK(level_load_data(&state.level, "patrol crate choice", data,
                          strlen(data), &state.rng));
    gameplay_ai_spawn_level_entities(&state);

    Crate *crate = &state.level.runtime.crates[0];
    Enemy *enemy = &state.enemies[0];
    crate->on_ground = true;
    enemy->dir = -1;
    enemy->on_ground = true;
    enemy->aim_timer = 0.0f;
    /* The first patrol route roll is 55, selecting the foreground branch. */
    rng_seed(&state.rng, 1);

    bool jumped = false;
    bool overlapped_crate = false;
    bool passed_crate = false;
    for (int frame = 0; frame < SIM_STEPS(6.0f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (enemy->vy < 0.0f)
            jumped = true;
        if (enemy->x < crate->x + CRATE_W &&
            enemy->x + ENEMY_W > crate->x)
            overlapped_crate = true;
        if (enemy->x + ENEMY_W <= crate->x)
        {
            passed_crate = true;
            break;
        }
    }

    CHECK(!jumped);
    CHECK(overlapped_crate);
    CHECK(passed_crate);
    CHECK(enemy->dir < 0);
}

static void test_enemy_uses_ladder_while_avoiding_crate(void)
{
    static const char data[] =
        "########\n"
        "#S H  E#\n"
        "###H####\n"
        "#  H B #\n"
        "########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 4242);
    CHECK(level_load_data(&level, "enemy crate ladder route", data,
                          strlen(data), &rng));

    float ladder_x = 3.0f * TILE_SIZE +
                     (TILE_SIZE - ENEMY_W) * 0.5f;
    Enemy enemy;
    enemy_init(&enemy, ladder_x, 3.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    enemy.dir = -1;
    enemy.on_ground = true;
    /* A crate collision starts this timer. It should suppress steering back
     * into the crate, but must not turn a required ladder into a random patrol
     * choice while the guard is pursuing a target on another floor. */
    enemy.obstacle_avoid_timer = ENEMY_OBSTACLE_AVOID_TIME;
    rng_seed(&rng, 1); /* The old random patrol check declines this ladder. */

    enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                 level.map.start_x + PLAYER_W * 0.5f,
                 1.0f * TILE_SIZE + PLAYER_H * 0.5f,
                 false, 1.0f, &rng);

    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == -1);
}

static void test_patrol_enemy_does_not_immediately_leave_ladder(void)
{
    static const char data[] =
        "########\n"
        "#S H  E#\n"
        "#  H   #\n"
        "#  H   #\n"
        "#  H   #\n"
        "#  H   #\n"
        "########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 4242);
    CHECK(level_load_data(&level, "enemy patrol ladder commitment", data,
                          strlen(data), &rng));

    float ladder_x = 3.0f * TILE_SIZE +
                     (TILE_SIZE - ENEMY_W) * 0.5f;
    Enemy enemy;
    enemy_init(&enemy, ladder_x, 5.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    enemy.on_ground = true;
    enemy.climb_cooldown = 0.0f;

    /* This sequence accepts the patrol climb and would then accept a random
       side exit on the next frame, while still on the starting floor. */
    rng_seed(&rng, 389);
    enemy_update(&enemy, &level, SIM_STEP_DT, false, false,
                 0.0f, 0.0f, false, 1.0f, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == -1);

    float climb_start_y = enemy.y;
    enemy_update(&enemy, &level, SIM_STEP_DT, false, false,
                 0.0f, 0.0f, false, 1.0f, &rng);

    CHECK(enemy.climbing);
    CHECK(enemy.y < climb_start_y);
}

static void test_enemy_leaves_climb_state_when_landing_on_crate(void)
{
    GameplayState state = {0};
    Enemy *enemy = &state.enemies[0];
    Crate *crate = &state.level.runtime.crates[0];
    state.enemy_count = 1;
    state.level.runtime.crate_count = 1;
    *crate = (Crate){
        .x = 96.0f,
        .y = 160.0f,
        .active = true,
        .on_ground = true};
    *enemy = (Enemy){
        .x = crate->x,
        .y = crate->y - ENEMY_H + 1.0f,
        .vy = ENEMY_CLIMB_SPEED,
        .dir = 1,
        .climbing = true,
        .climb_dir = 1,
        .hp = ENEMY_HP};

    gameplay_resolve_enemy_crates(&state, enemy, crate->y - ENEMY_H);

    CHECK(!enemy->climbing);
    CHECK(enemy->on_ground);
    CHECK(fabsf(enemy->y - (crate->y - ENEMY_H)) < 0.01f);
    CHECK(enemy->obstacle_avoid_timer == ENEMY_OBSTACLE_AVOID_TIME);
}

static void test_enemy_aligns_before_vertical_climb(void)
{
    static const char data[] =
        "########\n"
        "#  H   #\n"
        "#S H E #\n"
        "###H####\n"
        "#  H   #\n"
        "#  H   #\n"
        "###H####\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 1618);
    CHECK(level_load_data(&level, "enemy ladder alignment", data,
                          strlen(data), &rng));

    float ladder_x = 3.0f * TILE_SIZE +
                     (TILE_SIZE - ENEMY_W) * 0.5f;
    Enemy enemy;
    enemy_init(&enemy, ladder_x - 10.0f, 5.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    enemy.dir = -1;
    enemy.on_ground = true;

    enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                 level.map.start_x + PLAYER_W * 0.5f,
                 2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);
    CHECK(enemy.climbing);

    float climb_start_y = enemy.y;
    float off_ladder_x = enemy.x;
    enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                 level.map.start_x + PLAYER_W * 0.5f,
                 2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);

    CHECK(enemy.x > off_ladder_x);
    CHECK(fabsf(enemy.y - climb_start_y) < 0.01f);

    for (int frame = 0; frame < SIM_STEPS(4.0f) && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                     level.map.start_x + PLAYER_W * 0.5f,
                     2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);

    CHECK(!enemy.climbing);
    CHECK(fabsf(enemy.x - ladder_x) < 0.01f);
    CHECK(fabsf(enemy.y - 2.0f * TILE_SIZE) < 0.01f);

    enemy.on_ground = true;
    enemy.dir = 1;
    enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                 ladder_x + ENEMY_W * 0.5f,
                 5.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == 1);

    for (int frame = 0; frame < SIM_STEPS(4.0f) && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                     ladder_x + ENEMY_W * 0.5f,
                     5.0f * TILE_SIZE + ENEMY_H * 0.5f,
                     false, 1.0f, &rng);

    CHECK(!enemy.climbing);
    CHECK(fabsf(enemy.x - ladder_x) < 0.01f);
    CHECK(fabsf(enemy.y - 5.0f * TILE_SIZE) < 0.01f);
}

static void test_enemy_climbs_out_of_the_hole_at_a_ladder_top(void)
{
    /* Every ladder in the campaign is threaded through a slab, so the topmost
     * tile of the run IS the hole cut in that slab. A guard who let go as soon
     * as there was no rung above his head let go standing in the hole: the slab
     * against both shoulders, no floor to step onto, and the rung under his
     * feet catching him every time he tried to fall back down. He stood there
     * for the rest of the sector. The last tile of the climb is what puts him
     * on the storey the ladder serves. */
    static const char data[] =
        "##########\n"
        "#S       #\n"
        "####H#####\n"
        "#   H    #\n"
        "#   H  E #\n"
        "##########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 4242);
    CHECK(level_load_data(&level, "ladder through a slab", data,
                          strlen(data), &rng));

    float ladder_x = 4.0f * TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
    Enemy enemy;
    enemy_init(&enemy, ladder_x, 4.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    enemy.on_ground = true;
    enemy.climb_cooldown = 0.0f;

    /* The player up on the storey the ladder reaches. */
    float target_x = level.map.start_x + PLAYER_W * 0.5f;
    float target_y = 2.0f * TILE_SIZE - PLAYER_H * 0.5f;

    for (int frame = 0; frame < SIM_STEPS(10.0f) && !enemy.climbing; ++frame)
        enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                     target_x, target_y, false, 1.0f, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == -1);

    for (int frame = 0; frame < SIM_STEPS(10.0f) && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                     target_x, target_y, false, 1.0f, &rng);

    CHECK(!enemy.climbing);
    /* Standing on the slab, not inside the hole through it. */
    CHECK(fabsf(enemy.y - (2.0f * TILE_SIZE - ENEMY_H)) < 0.01f);

    /* And walking again: a guard boxed in at body height goes nowhere. */
    float stranded_x = enemy.x;
    for (int frame = 0; frame < SIM_STEPS(1.0f); ++frame)
        enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                     target_x, target_y, false, 1.0f, &rng);
    CHECK(enemy.x < stranded_x - TILE_SIZE);
}

static void test_enemy_leaves_a_ladder_that_already_reaches_a_floor(void)
{
    /* The rooftop's ladders run up the flank of a block instead of through a
     * slab, so the top rung is level with the roof it serves. A climb that
     * always spent one more tile clearing the last rung would carry the guard
     * a tile above that roof and drop him back onto it. Arriving beside a
     * floor is arriving. */
    static const char data[] =
        "##########\n"
        "#        #\n"
        "#  H     #\n"
        "#  H##   #\n"
        "#S H##  E#\n"
        "##########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 4242);
    CHECK(level_load_data(&level, "ladder up a rooftop block", data,
                          strlen(data), &rng));

    float ladder_x = 3.0f * TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
    Enemy enemy;
    enemy_init(&enemy, ladder_x, 4.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    enemy.on_ground = true;
    enemy.climb_cooldown = 0.0f;

    /* The player standing on the block's roof, to the right of the ladder. */
    float target_x = 4.5f * TILE_SIZE;
    float target_y = 3.0f * TILE_SIZE - PLAYER_H * 0.5f;

    for (int frame = 0; frame < SIM_STEPS(10.0f) && !enemy.climbing; ++frame)
        enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                     target_x, target_y, false, 1.0f, &rng);
    CHECK(enemy.climbing);

    for (int frame = 0; frame < SIM_STEPS(10.0f) && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, SIM_STEP_DT, true, false,
                     target_x, target_y, false, 1.0f, &rng);

    CHECK(!enemy.climbing);
    CHECK(fabsf(enemy.y - (3.0f * TILE_SIZE - ENEMY_H)) < 0.01f);
}

static void test_dog_escapes_ladder_perch_without_spinning(void)
{
    /* A dog stranded on a ladder rung one tile above the floor used to see a
     * cliff on both sides (its ledge probes only scan the feet row) and flip
     * direction every frame, spinning in place. It must now step down off the
     * rung instead of spinning. */
    static const char data[] =
        "##########\n"
        "#S  H    #\n"
        "####H#####\n"
        "#   H    #\n"
        "#   H    #\n"
        "####H#####\n"
        "#       E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 24601);
    CHECK(level_load_data(&state.level, "dog ladder perch", data,
                          strlen(data), &state.rng));

    int perch_col = 4;
    int perch_row = 3; /* rung one tile above the lower floor (row 5) */
    float perch_x = perch_col * (float)TILE_SIZE + (TILE_SIZE - DOG_W) * 0.5f;
    float perch_y = (perch_row + 1) * (float)TILE_SIZE - DOG_H;

    state.dog_count = 1;
    Dog *dog = &state.dogs[0];
    *dog = (Dog){0};
    dog->x = perch_x;
    dog->y = perch_y;
    dog->dir = 1;
    dog->owner = -1;
    dog->hp = DOG_HP;
    dog->state = DOG_GUARD;
    dog->guard_x = 0.0f; /* anchor to the far left, across the ladder */
    dog->guard_y = perch_y;
    dog->roam_target_x = 0.0f;
    dog->chase_target_x = 0.0f;
    dog->on_ground = true;

    /* Keep the player far away so the dog just tries to return home. */
    state.player.x = 10000.0f;
    state.player.y = 10000.0f;

    int flips = 0;
    int prev_dir = dog->dir;
    for (int frame = 0; frame < SIM_STEPS(3.0f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (dog->dir != prev_dir)
        {
            flips++;
            prev_dir = dog->dir;
        }
    }

    CHECK(flips <= 3);                          /* no frantic spinning */
    CHECK(dog->y > perch_y + TILE_SIZE * 0.5f); /* dropped off the rung */
    CHECK(fabsf(dog->x - perch_x) > TILE_SIZE); /* left the ladder column */
}

/*
 * A dog crosses a gap it can clear and stops at one it cannot.
 *
 * `dog_can_jump_gap` and `dog_can_advance` are the two halves of how an animal
 * reads a hole in the floor, and neither had ever been executed by the suite —
 * thirty-seven regions and five, nought per cent. The guards' equivalents are
 * tested; the dogs' were written to match them and then left. A dog is on ten
 * of the twelve interiors, and the difference between the two answers is the
 * difference between an animal that hunts a floor and one that walks off it.
 *
 * Both maps are the same shape and differ only in the width of the hole:
 * `DOG_JUMP_MAX_GAP_TILES` is two, so two is a jump and four is a ledge. The
 * pit is deep enough in both that `dog_can_step_down` — two tiles, the other
 * way across — has nothing to offer, or the animal would simply climb down.
 */
static void test_a_dog_jumps_a_gap_it_can_clear(void)
{
    static const char data[] =
        "##################\n"
        "#                #\n"
        "#  W    S      E #\n"
        "####  ############\n"
        "#                #\n"
        "#                #\n"
        "##################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 515);
    REQUIRE(level_load_data(&state.level, "dog gap", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.dog_count == 1);
    Dog *dog = &state.dogs[0];

    /* Eye to eye, so the animal is chasing rather than patrolling: the sight
     * check wants the two centres within nine tenths of a tile. */
    state.player.x = 8.0f * TILE_SIZE;
    state.player.y = dog->y + DOG_H * 0.5f - PLAYER_H * 0.5f;
    float started_at = dog->x;

    bool left_the_ground = false;
    for (int step = 0; step < SIM_STEPS(4.0f); ++step)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (!dog->on_ground && dog->vy < 0.0f)
            left_the_ground = true;
        /* Whatever it does, it does not end up in the pit. */
        CHECK(dog->y < 4.0f * TILE_SIZE);
    }

    CHECK(left_the_ground);
    /* Across: past the far lip of the hole, which is where the floor starts
     * again. */
    CHECK(dog->x > 6.0f * TILE_SIZE);
    CHECK(dog->x > started_at);
}

static void test_a_dog_turns_back_at_a_gap_it_cannot_clear(void)
{
    static const char data[] =
        "####################\n"
        "#                  #\n"
        "#  W    S        E #\n"
        "####    ############\n"
        "#                  #\n"
        "#                  #\n"
        "####################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 616);
    REQUIRE(level_load_data(&state.level, "dog ledge", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.dog_count == 1);
    Dog *dog = &state.dogs[0];

    /* Five tiles, so it is inside `DOG_VIEW_RANGE` and chasing — the pit has to
     * be refused by something that badly wants to cross it. */
    state.player.x = 8.0f * TILE_SIZE;
    state.player.y = dog->y + DOG_H * 0.5f - PLAYER_H * 0.5f;
    CHECK(fabsf(state.player.x - dog->x) < DOG_VIEW_RANGE);

    bool faced_the_player = false;
    bool turned_away = false;
    for (int step = 0; step < SIM_STEPS(4.0f); ++step)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        /* The whole claim: four tiles is a ledge, and it is never walked off.
         * A dog in the pit is a dog the player watched commit suicide. */
        CHECK(dog->y < 4.0f * TILE_SIZE);
        CHECK(dog->x + DOG_W <= 4.0f * TILE_SIZE + 1.0f);
        if (dog->dir > 0)
            faced_the_player = true;
        else if (faced_the_player)
            turned_away = true;
    }

    /* It came to the lip and then turned rather than standing on it shaking:
     * `dog_can_advance` is asked whether the *other* way is passable before the
     * flip, so a dog with a floor behind it goes back down it and a dog boxed in
     * stands still instead of spinning. */
    CHECK(faced_the_player);
    CHECK(turned_away);
    CHECK(!dog->dead);
}

/*
 * With nothing to chase, a dog works a patch around its handler.
 *
 * `dog_pick_roam_target` is the third of the three that had never run. It is
 * what stops a guard's dog standing welded to his heel all night, and the one
 * rule inside it worth pinning is the one that is easy to get backwards: the
 * offset it picks is pushed out to at least `DOG_HANDLER_DISTANCE`, because a
 * roam target under the handler's own feet is a roam that never moves.
 */
static void test_a_dog_with_nothing_to_chase_roams_around_its_handler(void)
{
    static const char data[] =
        "##########################\n"
        "#                        #\n"
        "#S                  W   E#\n"
        "##########################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 2024);
    REQUIRE(level_load_data(&state.level, "dog roam", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.dog_count == 1);
    Dog *dog = &state.dogs[0];

    /* Nineteen tiles away, against a `DOG_VIEW_RANGE` of six: the animal has
     * nothing to look at, which is the state this is about. */
    CHECK(fabsf(state.player.x - dog->x) > DOG_VIEW_RANGE);

    int roams = 0;
    DogState previous = dog->state;
    for (int step = 0; step < SIM_STEPS(30.0f); ++step)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (dog->state == DOG_ROAM && previous != DOG_ROAM)
        {
            roams++;
            float reach = fabsf(dog->roam_target_x - dog->guard_x);
            /* Far enough to be a walk, near enough to still be his handler's
             * dog. */
            CHECK(reach >= DOG_HANDLER_DISTANCE - 0.01f);
            CHECK(reach <= DOG_ROAM_RADIUS + 0.01f);
        }
        previous = dog->state;
        /* It is a patch, not a patrol of its own. */
        CHECK(fabsf(dog->x - dog->guard_x) <
              DOG_RETURN_RADIUS + DOG_ROAM_RADIUS);
        CHECK(!dog->dead);
    }
    CHECK(roams > 0);
}

/*
 * A guard nobody has spawned is nobody's partner.
 *
 * `talk_partner` is an index with -1 for "none", and nought is a real slot — so
 * a zeroed `Enemy` answers "in conversation with guard nought" to anything that
 * asks the field on its own. Three disqualifiers in `update_conversations` and
 * `update_radio_checks` did exactly that, which would have taken a man out of
 * every chat and every net check on the floor for the rest of the sector.
 *
 * Nothing shipped that way because every spawn path clears it, and this is the
 * half that says so: the sentinel is checked where it is written, and the two
 * derived questions are checked against a struct nobody initialised. It is the
 * rule `Player.dragging` is a flag to avoid needing, kept here as a test because
 * the index is worth keeping — the pair have to be able to name each other.
 */
static void test_a_zeroed_guard_is_nobody_s_partner(void)
{
    /* The state every test and every fresh sector starts from. */
    Enemy blank = {0};
    CHECK(!enemy_has_talk_partner(&blank));
    CHECK(!enemy_on_radio(&blank));

    /* And every path that puts a man on a floor leaves the sentinel set, which
     * is what makes the index safe to read once `talking` is true. */
    static const char data[] =
        "############\n"
        "#S M W  M E#\n"
        "############\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 77);
    REQUIRE(level_load_data(&state.level, "blank", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.enemy_count == 3);
    for (int i = 0; i < state.enemy_count; ++i)
    {
        CHECK(state.enemies[i].talk_partner == -1);
        CHECK(!enemy_has_talk_partner(&state.enemies[i]));
    }

    /* A man on his own handset is talking and unpartnered, which is the one
     * combination the two helpers have to disagree about. */
    Enemy solo = {0};
    solo.talking = true;
    solo.talk_partner = -1;
    CHECK(enemy_on_radio(&solo));
    CHECK(!enemy_has_talk_partner(&solo));

    /* And a real pair is the other way round. */
    Enemy paired = {0};
    paired.talking = true;
    paired.talk_partner = 0;
    CHECK(!enemy_on_radio(&paired));
    CHECK(enemy_has_talk_partner(&paired));
}

/*
 * Two calm guards who have walked into each other fall into conversation.
 *
 * `update_conversations` is forty lines of `gameplay_ai.c` and none of it had
 * ever run. The pose was covered — `test_a_takedown_does_not_wake_the_man_he
 * _was_talking_to` sets `talking` and `talk_partner` by hand, which is what it
 * needs — so the *pairing* was the half nothing reached: the dozen conditions
 * that disqualify a man, the shove that stands the two of them a body's width
 * apart, the tile check that refuses the shove where there is no room for it,
 * and the pair of indices that have to point at each other afterwards.
 *
 * That is a mechanic the player meets on every quiet floor in the game, and it
 * is also what the blade is played against: a pair in conversation is two men
 * facing each other and neither facing the room.
 *
 * The two are put deliberately *overlapping*, which is what makes this a test
 * of the shove rather than of the pose. `ENEMY_W` is 26 against a 32px tile, so
 * two men on neighbouring tiles do not overlap at all and the shove is a no-op —
 * the case that needs it is two patrols that have met, which is also the only
 * case where the pose would read as one man standing inside another.
 */
static void test_two_calm_guards_standing_together_start_talking(void)
{
    /* Chuck parked at the far end, well past anything that would provoke
     * either of them, and an open floor in the middle so the shove has
     * somewhere to go both ways. */
    static const char data[] =
        "########################\n"
        "#S           MM       E#\n"
        "########################\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "chat", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.enemy_count == 2);
    for (int i = 0; i < 2; ++i)
        state.enemies[i].on_ground = true;

    /* Shoulder to shoulder, six pixels apart, as two patrols meeting leaves
     * them. */
    float met_at = state.enemies[0].x;
    state.enemies[1].x = met_at + 6.0f;
    state.enemies[1].y = state.enemies[0].y;
    REQUIRE(state.enemies[1].x - state.enemies[0].x < (float)ENEMY_W);

    /* The roll is one in `ENEMY_TALK_CHANCE` per frame per candidate pair, so
     * the question is whether it ever fires rather than whether it fires now.
     * Both are held still, because a patrol that walks apart stops being a
     * candidate and this is a test of the pairing rather than of the walk. */
    bool paired = false;
    for (int step = 0; step < SIM_STEPS(4.0f) && !paired; ++step)
    {
        state.events.count = 0;
        state.enemies[0].vx = 0.0f;
        state.enemies[1].vx = 0.0f;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        paired = state.enemies[0].talking && state.enemies[1].talking;
    }
    REQUIRE(paired);

    /* Each names the other, which is the invariant the rest of the AI reads:
     * `gameplay_provoke_enemy` wakes a man's partner through this index, so a
     * pair that disagreed about who they were talking to would wake a
     * bystander. */
    CHECK(state.enemies[0].talk_partner == 1);
    CHECK(state.enemies[1].talk_partner == 0);
    /* Stood apart rather than inside one another, and neither shoved into
     * masonry. */
    CHECK(state.enemies[1].x - state.enemies[0].x >= (float)ENEMY_W);
    CHECK(gameplay_box_tiles_clear(&state, state.enemies[0].x,
                                   state.enemies[0].y, ENEMY_W, ENEMY_H, STANCE_UPRIGHT));
    CHECK(gameplay_box_tiles_clear(&state, state.enemies[1].x,
                                   state.enemies[1].y, ENEMY_W, ENEMY_H, STANCE_UPRIGHT));
    /* And shoved symmetrically about where they met, rather than one man being
     * walked backwards while the other stands still. */
    float left_moved = met_at - state.enemies[0].x;
    float right_moved = state.enemies[1].x - (met_at + 6.0f);
    CHECK(left_moved > 0.0f);
    CHECK(fabsf(left_moved - right_moved) < 1.0f);
    /* Facing each other rather than the room. */
    CHECK(state.enemies[0].dir == 1);
    CHECK(state.enemies[1].dir == -1);
    /* A conversation is a noise on the net as well as a pose. */
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_GUARD_TALK));

    /* And it ends, releasing both of them rather than leaving one man pointing
     * at a partner who has moved on. */
    for (int step = 0; step < SIM_STEPS(ENEMY_TALK_DURATION + 1.0f); ++step)
    {
        state.events.count = 0;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    }
    CHECK(!state.enemies[0].talking);
    CHECK(!state.enemies[1].talking);
    CHECK(state.enemies[0].talk_partner == -1);
    CHECK(state.enemies[1].talk_partner == -1);
}

/*
 * A pair with nowhere to stand apart is left alone.
 *
 * The shove is what makes the pose readable, so where it cannot be made the
 * pairing has to be refused rather than made anyway — otherwise the corner of a
 * floor is where two men hold a conversation half inside the wall.
 * `update_conversations` asks `gameplay_box_tiles_clear` about both
 * destinations before it commits to anything, and that refusal was the branch
 * inside the branch: unreachable while the pairing itself was.
 */
static void test_a_pair_with_nowhere_to_stand_does_not_talk(void)
{
    static const char data[] =
        "########################\n"
        "#S           MM       E#\n"
        "########################\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "corner", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.enemy_count == 2);

    /* The same meeting, in the corner: the left man is already against the end
     * wall, so the room the shove needs is masonry on one side. */
    float corner = (float)TILE_SIZE;
    float floor_y = state.enemies[0].y;
    for (int step = 0; step < SIM_STEPS(6.0f); ++step)
    {
        /* Re-pinned every step, so the geometry the rule is asked about is the
         * same one on every frame and the only thing under test is its answer. */
        state.enemies[0].x = corner;
        state.enemies[1].x = corner + 6.0f;
        for (int i = 0; i < 2; ++i)
        {
            state.enemies[i].y = floor_y;
            state.enemies[i].vx = 0.0f;
            state.enemies[i].on_ground = true;
        }
        state.events.count = 0;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        /* A man may still be on his own handset — that is `enemy_on_radio` and
         * a different mechanic — but neither may be paired with the other. */
        CHECK(state.enemies[0].talk_partner == -1);
        CHECK(state.enemies[1].talk_partner == -1);
    }

    /* And the refusal is about the room rather than about the two of them: the
     * same pair, put back in the middle of the same floor, do pair up. */
    float open_floor = 12.0f * TILE_SIZE;
    bool paired = false;
    for (int step = 0; step < SIM_STEPS(4.0f) && !paired; ++step)
    {
        if (step == 0)
        {
            state.enemies[0].x = open_floor;
            state.enemies[1].x = open_floor + 6.0f;
        }
        for (int i = 0; i < 2; ++i)
        {
            state.enemies[i].vx = 0.0f;
            state.enemies[i].on_ground = true;
        }
        state.events.count = 0;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        paired = state.enemies[0].talking && state.enemies[1].talking;
    }
    CHECK(paired);
}

static void test_hazards_emit_specific_impact_sounds(void)
{
    GameplayState state = {0};
    state.player.facing = 1;
    state.level.map.ceiling_fan_count = 1;
    state.level.map.ceiling_fans[0] = (CeilingFan){.x = 13.0f, .y = 1.0f};

    gameplay_combat_update_hazards(&state);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_FAN_HIT));

    state = (GameplayState){0};
    state.player.facing = 1;
    state.level.map.spike_count = 1;
    state.level.map.spike_spawns[0] = (SpikeSpawn){.x = 0.0f, .y = 0.0f};
    gameplay_combat_update_hazards(&state);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_SPIKE_HIT));
}

/*
 * The man with the mop does his three jobs and turns round at the wall.
 *
 * A janitor changes nothing: he is a visual-only NPC, he cannot be hurt and he
 * hurts nobody. What he does is make a floor look staffed, and he had never
 * been stepped once — sixty lines of `update_janitor` compiled and unexecuted,
 * which is the whole of the ambience on nine shipped floors. It is worth a test
 * for the same reason the wet spot is worth drawing: this is the only thing in
 * the building doing something other than looking for Chuck, and a mop that
 * stopped cycling or a cart that walked into a wall would be visible to every
 * player and invisible to this suite.
 */
static void test_the_janitor_walks_mops_and_turns_at_the_wall(void)
{
    /* A short floor, so the walk reaches a wall inside the run, and Chuck far
     * enough off that nothing about him matters — nothing about him does. */
    static const char data[] =
        "############\n"
        "#S  J     E#\n"
        "############\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 1207);
    REQUIRE(level_load_data(&state.level, "night shift", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.janitor_count == 1);
    Janitor *janitor = &state.janitors[0];
    janitor->on_ground = true;

    bool walked = false;
    bool mopped = false;
    bool paused = false;
    bool turned = false;
    bool spot_appeared = false;
    bool spot_expired = false;
    int starting_dir = janitor->dir;
    float furthest_left = janitor->x;
    float furthest_right = janitor->x;

    /* Long enough for the activity cycle to come round several times and for a
     * wet spot laid early to outlive `JANITOR_WET_LIFETIME`. */
    for (int step = 0; step < SIM_STEPS(60.0f); ++step)
    {
        state.events.count = 0;
        float previous_x = janitor->x;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);

        walked |= janitor->activity == JANITOR_WALK &&
                  fabsf(janitor->x - previous_x) > 0.0f;
        mopped |= janitor->activity == JANITOR_MOP;
        paused |= janitor->activity == JANITOR_PAUSE;
        turned |= janitor->dir != starting_dir;

        int active = 0;
        for (int i = 0; i < JANITOR_WET_SPOTS; ++i)
            active += janitor->wet_spots[i].active ? 1 : 0;
        if (active > 0)
            spot_appeared = true;
        if (spot_appeared && active == 0)
            spot_expired = true;

        if (janitor->x < furthest_left)
            furthest_left = janitor->x;
        if (janitor->x > furthest_right)
            furthest_right = janitor->x;

        /* Whatever he is doing, he is doing it on the floor and not inside it —
         * the cart is what makes that worth asking, because it is carried on
         * the trailing side and the turn at a wall is where it would be pushed
         * through one. */
        REQUIRE(!isnan(janitor->x) && !isnan(janitor->y));
        CHECK(janitor->x > (float)TILE_SIZE - (float)JANITOR_W);
        CHECK(janitor->x < (float)(state.level.map.width - 1) * TILE_SIZE);
    }

    CHECK(walked);
    CHECK(mopped);
    CHECK(paused);
    CHECK(turned);
    CHECK(spot_appeared);
    CHECK(spot_expired);
    /* And he covered ground rather than shuffling on one tile. */
    CHECK(furthest_right - furthest_left > (float)TILE_SIZE);
}


/* The mercy window after a hit stops the guard hurting Chuck. It must not stop
 * Chuck landing on the guard: a stomp that silently does nothing reads as the
 * move failing at random, because the player cannot see the timer. */
static void test_stomp_still_lands_during_the_mercy_window(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP};
    state.invuln_timer = PLAYER_HIT_INVULN;
    state.player.hp = PLAYER_MAX_HP;
    state.player.x = 100.0f;
    state.player.y = 200.0f - (float)PLAYER_H + 5.0f;
    state.player.vy = 50.0f;

    gameplay_combat_check_contacts(&state, &campaign);

    CHECK(state.player.vy == -ENEMY_STOMP_BOUNCE_SPEED);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);

    /* The other half of the same window: a side contact still costs nothing. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP};
    state.invuln_timer = PLAYER_HIT_INVULN;
    state.player.hp = PLAYER_MAX_HP;
    state.player.x = 100.0f + ENEMY_W - 3.0f;
    state.player.y = 200.0f;
    state.player.vy = 50.0f;

    gameplay_combat_check_contacts(&state, &campaign);

    CHECK(state.player.hp == PLAYER_MAX_HP);
    CHECK(!state.player.dying);
    CHECK(state.enemies[0].hp == ENEMY_HP);
}

static void test_stomp_on_enemy_bounces_player_and_damages_it(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP};

    /* Aligned horizontally with the guard, feet only just into its head. */
    state.player.x = 100.0f;
    state.player.y = 200.0f - (float)PLAYER_H + 5.0f;
    state.player.vy = 50.0f; /* falling */

    gameplay_combat_check_contacts(&state, &campaign);

    CHECK(!state.player.dying);
    CHECK(state.player.vy == -ENEMY_STOMP_BOUNCE_SPEED);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);
    CHECK(!state.enemies[0].dead);

    /* A side contact at the same height still kills the player as before. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP};
    state.player.x = 100.0f + ENEMY_W - 3.0f;
    state.player.y = 200.0f;
    state.player.vy = 50.0f;

    gameplay_combat_check_contacts(&state, &campaign);

    CHECK(state.player.dying);
    CHECK(state.enemies[0].hp == ENEMY_HP);
}

/*
 * A stomp is landed from above, and a shallow vertical overlap does not say
 * that on its own — it says only that the two boxes just met on that axis.
 * Jumping up into a guard standing on the ledge overhead makes the same
 * shallow overlap, and the moment the rise turns into a fall it satisfied
 * every other condition: Chuck's head butting a man standing over him read as
 * a boot landing on that man's helmet, and wounded him for it.
 */
static void test_a_stomp_has_to_come_from_above(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP};

    /* Chuck under the guard, his head only just into the guard's boots, and
     * already tipping over into the fall. */
    state.player.x = 100.0f;
    state.player.y = 200.0f + (float)ENEMY_H - 5.0f;
    state.player.vy = 50.0f;

    gameplay_combat_check_contacts(&state, &campaign);

    /* Contact from below is contact: it costs a heart and leaves the guard
     * untouched. What it must never do is bounce and wound. */
    CHECK(state.enemies[0].hp == ENEMY_HP);
    CHECK(state.player.vy != -ENEMY_STOMP_BOUNCE_SPEED);
    CHECK(campaign.score == 0);
}

/*
 * A bolt lands somewhere Chuck is not, and the room walks over to it.
 *
 * Every part of the perception model this exercises was already here — the
 * investigate timer, the walk to a heard disturbance, the scan, the return to
 * patrol — and until the bolts there was exactly one way for the player to
 * reach any of it, which was to fire a gun. That put the noise and the man who
 * made it in the same place every time, so the whole branch could only ever be
 * used *against* the player. The one thing this test has to hold is therefore
 * not that a guard becomes curious: it is that he becomes curious about the
 * bolt's position rather than about Chuck's.
 */
/*
 * The heavy, and the one answer he is here to take away.
 *
 * The campaign ran fifteen sectors on one kind of man, and every mechanic the
 * player learns answers that man. The stomp is the *free* one — no ammunition,
 * no position, no noise — and it is what anybody reaches for when a floor gets
 * busy, so it is the answer worth denying. Everything else about him has to
 * stay exactly as it was, or he stops being the same guard in a vest and
 * becomes a second AI to keep in step with the first.
 */
static void test_a_heavy_cannot_be_stomped_but_can_still_be_knifed(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 616);

    /* Landing on his helmet costs the heart a side contact costs, and leaves
     * him untouched. */
    state.enemy_count = 1;
    enemy_init(&state.enemies[0], 100.0f, 200.0f, ENEMY_KIND_HEAVY,
               &state.rng);
    CHECK(state.enemies[0].hp == ENEMY_HEAVY_HP);
    state.player.hp = PLAYER_MAX_HP;
    state.player.x = 100.0f;
    state.player.y = 200.0f - (float)PLAYER_H + 5.0f;
    state.player.vy = 50.0f;
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.enemies[0].hp == ENEMY_HEAVY_HP);
    CHECK(state.player.vy != -ENEMY_STOMP_BOUNCE_SPEED);
    CHECK(state.player.hp < PLAYER_MAX_HP || state.player.dying);

    /* The same landing on an ordinary guard still bounces, so the rule is the
     * armour rather than something that quietly broke the stomp for everyone. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    rng_seed(&state.rng, 616);
    state.enemy_count = 1;
    enemy_init(&state.enemies[0], 100.0f, 200.0f, ENEMY_KIND_GUARD,
               &state.rng);
    state.player.hp = PLAYER_MAX_HP;
    state.player.x = 100.0f;
    state.player.y = 200.0f - (float)PLAYER_H + 5.0f;
    state.player.vy = 50.0f;
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.player.vy == -ENEMY_STOMP_BOUNCE_SPEED);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);

    /* And the blade behind him is unchanged, which is the half that makes him
     * an argument for the quiet route rather than a wall to unload into: a
     * takedown is a knife across a throat, not damage, so the vest is no help. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    rng_seed(&state.rng, 616);
    state.enemy_count = 1;
    enemy_init(&state.enemies[0], 100.0f, 200.0f, ENEMY_KIND_HEAVY,
               &state.rng);
    state.enemies[0].dir = 1;
    state.enemies[0].on_ground = true;
    state.player.x = 80.0f;
    state.player.y = 200.0f;
    state.player.facing = 1;
    state.player.bullets = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    Input swing = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &swing);
    CHECK(state.enemies[0].dead);
    CHECK(campaign.score == PLAYER_TAKEDOWN_SCORE);

    /* From the front he is what the vest says he is: `ENEMY_HEAVY_HP` swings
     * rather than `ENEMY_HP`. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    rng_seed(&state.rng, 616);
    state.enemy_count = 1;
    enemy_init(&state.enemies[0], 100.0f, 200.0f, ENEMY_KIND_HEAVY,
               &state.rng);
    state.enemies[0].dir = -1;
    state.enemies[0].on_ground = true;
    state.player.x = 80.0f;
    state.player.y = 200.0f;
    state.player.facing = 1;
    state.player.bullets = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    int swings = 0;
    while (!state.enemies[0].dead && swings < 20)
    {
        Input hit = {.shoot = true};
        gameplay_combat_handle_player_action(&state, &campaign, &hit);
        ++swings;
    }
    CHECK(swings == ENEMY_HEAVY_HP);
}

/* He is slower on his feet, and that has to be visible rather than merely
 * configured: carrying the plate costs him something the player can watch. */
static void test_a_heavy_walks_slower_than_a_guard(void)
{
    static const char data[] =
        "####################\n"
        "#S     M    Q     E#\n"
        "####################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 2468);
    REQUIRE(level_load_data(&state.level, "heavy", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 2);
    CHECK(state.enemies[0].kind == ENEMY_KIND_GUARD);
    CHECK(state.enemies[1].kind == ENEMY_KIND_HEAVY);
    CHECK(state.enemies[1].hp == ENEMY_HEAVY_HP);

    /* Both chasing the same point, from the same standing start. */
    state.player.x = 5000.0f;
    state.player.y = 5000.0f;
    float start_guard = state.enemies[0].x;
    float start_heavy = state.enemies[1].x;
    for (int i = 0; i < 2; ++i)
    {
        state.enemies[i].on_ground = true;
        state.enemies[i].dir = 1;
        state.enemies[i].provoked = true;
        state.enemies[i].obstacle_avoid_timer = 0.0f;
        state.enemies[i].pursuit_target_x = 18.0f * TILE_SIZE;
        state.enemies[i].pursuit_target_y = state.enemies[i].y + ENEMY_H * 0.5f;
        state.enemies[i].has_pursuit_target = true;
    }
    for (int step = 0; step < SIM_STEPS(4.0f); ++step)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);

    float guard_moved = state.enemies[0].x - start_guard;
    float heavy_moved = state.enemies[1].x - start_heavy;
    CHECK(guard_moved > 0.0f);
    CHECK(heavy_moved > 0.0f);
    CHECK(heavy_moved < guard_moved);
}

/*
 * The flash charge, and the situation nothing else in the game answers.
 *
 * Every quiet mechanic is about *before* — a bolt moves attention, a blade
 * removes a man who never looked, a dragged body removes the reason the next
 * one does — and all of them stop being available the moment somebody is
 * shooting. This is the answer to *after*, and the whole of it is that the room
 * stops seeing for a few seconds and then carries on exactly as it was. Both
 * halves of that sentence are load-bearing: a charge that killed would be a bad
 * grenade, and one that *cleared* the encounter would make being seen free.
 */
static void test_a_flash_charge_blinds_the_room_without_changing_it(void)
{
    static const char data[] =
        "################\n"
        "#S   M        E#\n"
        "################\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 8080);
    REQUIRE(level_load_data(&state.level, "flash", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.enemy_count == 1);
    player_reset(&state.player, &state.level);

    Enemy *guard = &state.enemies[0];
    guard->on_ground = true;
    guard->dir = -1;
    guard->provoked = true;
    guard->has_pursuit_target = true;
    guard->pursuit_target_x = state.player.x;
    guard->pursuit_target_y = state.player.y;
    guard->raising_alarm = true;
    guard->alarm_switch_index = 0;
    guard->aim_timer = ENEMY_AIM_TIME;
    int hp_before = guard->hp;

    /* Thrown at his feet and left to go off. */
    state.player.flashbangs = 1;
    state.player.facing = 1;
    state.player.active_weapon = PLAYER_WEAPON_FLASH;
    Input throw_it = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &throw_it);
    CHECK(state.flashbangs[0].active);
    CHECK(state.player.flashbangs == 0);
    /* Spent, so the hand falls back to the sidearm like every other one-shot. */
    CHECK(state.player.active_weapon != PLAYER_WEAPON_FLASH);

    state.flashbangs[0].x = guard->x;
    state.flashbangs[0].y = guard->y;
    for (int step = 0; step < SIM_STEPS(10.0f) &&
         state.flashbangs[0].active;
         ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    CHECK(!state.flashbangs[0].active);

    /* Blinded, stopped, and on his way to no alarm switch. */
    CHECK(guard->blind_timer > 0.0f);
    CHECK(guard->aim_timer == 0.0f);
    CHECK(!guard->raising_alarm);
    /* And untouched: it is not a weapon. */
    CHECK(guard->hp == hp_before);
    CHECK(!guard->dead);
    CHECK(state.player.hp == PLAYER_MAX_HP);
    CHECK(!state.player.dying);
    /* It does not clear the encounter either — he still knows. */
    CHECK(guard->provoked);
    CHECK(guard->has_pursuit_target);

    /* While it burns he sees nothing, however plainly Chuck stands in front of
     * him: the sight timer is what everything downstream reads. */
    state.player.x = guard->x + ENEMY_W + 4.0f;
    state.player.y = guard->y;
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
        gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(guard->sight_timer == 0.0f);
    CHECK(guard->aim_timer == 0.0f);

    /* And he comes back. A flash that lasted for ever would be a kill with
     * extra steps. */
    int steps = 0;
    while (guard->blind_timer > 0.0f && steps < SIM_STEPS(20.0f))
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        ++steps;
    }
    CHECK(guard->blind_timer <= 0.0f);
    for (int step = 0; step < SIM_STEPS(4.0f); ++step)
        gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(guard->sight_timer > 0.0f);
}

/*
 * And the charge reaches the animal, which is the one thing in the room that
 * has an actual pair of eyes and was the one thing it did not touch.
 *
 * `detonate_flashbang` blinded the guards, then reached the cameras on the
 * stated grounds that a lens "is glass and a sensor, and a charge this bright in
 * front of one is the same event it is for a pair of eyes" — and stepped over
 * the dog. So the charge stopped the men and the fittings and left the teeth
 * coming, with nothing on screen to say why: exactly the failure `apply_blast`
 * exists to refuse, a blast that picks which of the things beside it are real.
 * It is also the case the mechanic was written for, because a dog is the one
 * enemy that has *already* found you — `DOG_BITE_WINDUP` announces itself for
 * that reason — and "the one answer in the game to having already been seen" was
 * no answer at all to it.
 *
 * The bite in progress is cancelled, because that is the animal's `aim_timer`
 * and a charge going off in its face must not let the teeth land anyway. The
 * chase deliberately is not: the guards keep theirs, and the charge buys seconds
 * rather than the encounter.
 */
/*
 * A flash charge is light, so it stops where light stops.
 *
 * `FLASH_RADIUS` is five tiles, and `game_config.h` says out loud what that
 * number is for: *"it has to catch the room the player is in, and it must not
 * reach the one next door, or 'throw it and walk' would be the answer to every
 * floor."* `detonate_flashbang` tested distance and nothing else, and five tiles
 * is wider than any partition in the building and wider than a storey is tall —
 * so one charge blinded the men in the next room and on the floors above and
 * below, straight through the masonry. Sector 12 is six crawl levels one riser
 * apart and sector 14 is panelled rooms off single doorways; both were a whole
 * floor that a single charge switched off.
 *
 * Both halves are the test, and the near one matters as much as the far one: a
 * sight line added carelessly would be a charge that reaches nobody at all, and
 * a check that only proved the wall stops it would pass that too.
 */
static void test_a_flash_charge_stops_at_the_masonry(void)
{
    /* One partition, one guard either side of it, both inside the radius. */
    static const char data[] =
        "###############\n"
        "#S   M #M    E#\n"
        "###############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "flash-wall", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.enemy_count == 2);
    player_reset(&state.player, &state.level);

    /* The two are told apart by which side of the partition they stand on
     * rather than by slot order, which the spawner does not promise. */
    int wall_col = 7;
    Enemy *near_man = NULL;
    Enemy *far_man = NULL;
    for (int i = 0; i < state.enemy_count; ++i)
    {
        Enemy *man = &state.enemies[i];
        if ((man->x + ENEMY_W * 0.5f) / TILE_SIZE < (float)wall_col)
            near_man = man;
        else
            far_man = man;
    }
    REQUIRE(near_man != NULL && far_man != NULL);
    REQUIRE(level_is_solid(&state.level, wall_col, 1));

    /* Thrown in the near man's room, and inside the radius of both: the far one
     * is only spared by the wall. */
    float x = near_man->x + ENEMY_W * 0.5f;
    float y = near_man->y + ENEMY_H * 0.5f;
    state.flashbangs[0].active = true;
    state.flashbangs[0].x = x - FLASH_W * 0.5f;
    state.flashbangs[0].y = y - FLASH_H * 0.5f;
    state.flashbangs[0].timer = 0.0f;

    float far_dx = (far_man->x + ENEMY_W * 0.5f) - x;
    float far_dy = (far_man->y + ENEMY_H * 0.5f) - y;
    REQUIRE(sqrtf(far_dx * far_dx + far_dy * far_dy) < FLASH_RADIUS);

    for (int step = 0; step < SIM_STEPS(10.0f) &&
         state.flashbangs[0].active;
         ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    CHECK(!state.flashbangs[0].active);

    /* In the room: blinded, which is the mechanic working at all. */
    CHECK(near_man->blind_timer > 0.0f);
    /* Next door: not, however close the arithmetic says he is. */
    CHECK(far_man->blind_timer == 0.0f);
}
/*
 * A blast takes the animal as well as the man.
 *
 * `apply_blast` runs two loops, the guards and the dogs, and only the first had
 * ever been driven: twenty lines that kill a dog, bank `DOG_SCORE`, count it
 * through `gameplay_record_neutralized` and yelp were compiled and never run.
 * That is not an edge — a dog is the one thing on a floor there is no silent
 * answer to, so an explosive is a perfectly ordinary way to meet one, and the
 * score it pays is the only thing on screen that says the animal counted.
 *
 * The same loop is what every explosive in the game goes through, which is the
 * point of `apply_blast` existing at all — see [One blast, one
 * rule](../docs/gameplay.md#one-blast-one-rule) — so a grenade here covers the
 * rocket, the mine and the canister.
 */
static void test_a_blast_kills_a_dog_in_reach(void)
{
    static const char data[] =
        "################\n"
        "#S   W        E#\n"
        "################\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 34);
    REQUIRE(level_load_data(&state.level, "blast-dog", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);
    REQUIRE(state.dog_count == 1);
    Dog *dog = &state.dogs[0];
    dog->on_ground = true;
    REQUIRE(!dog->dead);

    /* Right under the animal, and one guard — its handler — deliberately left
     * standing outside the radius, so what the score and the tally move by can
     * only be the dog. */
    REQUIRE(state.enemy_count == 1);
    state.enemies[0].x = dog->x + 8.0f * TILE_SIZE;
    int before_score = campaign.score;
    int before_down = gameplay_neutralized_hostiles(&state);

    state.grenade_count = 1;
    state.grenades[0] = (Grenade){.x = dog->x + DOG_W * 0.5f,
                                  .y = dog->y + DOG_H * 0.5f,
                                  .active = true,
                                  .timer = 0.01f,
                                  .fuse_sound_timer = 1.0f};
    for (int step = 0; step < SIM_STEPS(1.0f) && state.grenades[0].active;
         ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    REQUIRE(!state.grenades[0].active);

    CHECK(dog->dead);
    CHECK(dog->hp == 0);
    CHECK(campaign.score == before_score + DOG_SCORE);
    CHECK(gameplay_neutralized_hostiles(&state) == before_down + 1);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_DOG_YELP));
    /* The handler is untouched, which is what makes the numbers above readable
     * and is also the rule: a blast is a radius, not a floor. */
    CHECK(!state.enemies[0].dead);

    /* And a second blast on the same corpse pays nothing. The tally is what the
     * player is scored and ranked on, so a body that could be counted twice
     * would make an explosive the cheapest way to inflate it. */
    int after_score = campaign.score;
    int after_down = gameplay_neutralized_hostiles(&state);
    state.grenades[0] = (Grenade){.x = dog->x + DOG_W * 0.5f,
                                  .y = dog->y + DOG_H * 0.5f,
                                  .active = true,
                                  .timer = 0.01f,
                                  .fuse_sound_timer = 1.0f};
    for (int step = 0; step < SIM_STEPS(1.0f) && state.grenades[0].active;
         ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    CHECK(campaign.score == after_score);
    CHECK(gameplay_neutralized_hostiles(&state) == after_down);
}

/*
 * And a flash charge makes the ceiling forget too.
 *
 * [The charge that answers being seen](../docs/gameplay.md#the-charge-that-answers-being-seen)
 * says it takes the attention of *everything in the room with eyes*, and lists
 * the camera among them by name. The men were tested, the dogs were tested, and
 * the lenses were a documented promise with nothing behind it — fourteen lines
 * at the end of the flash that had never run.
 *
 * It matters because it is the one thing the charge does that is not about
 * buying seconds: a camera that has been staring at Chuck for
 * `CAMERA_NOTICE_TIME` is a second away from putting the whole floor on him, and
 * what the charge does is take that count back to nought. Nothing on screen says
 * so, which is exactly why it needs a test rather than a play-through.
 */
static void test_a_flash_charge_makes_a_camera_forget(void)
{
    static const char data[] =
        "################\n"
        "#     I        #\n"
        "#S            E#\n"
        "################\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 4242);
    REQUIRE(level_load_data(&state.level, "flash-camera", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.map.camera_count == 1);
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);

    /* A lens part way through making up its mind, which is the only state in
     * which forgetting means anything. */
    state.cameras_initialized = true;
    state.cameras[0].working = true;
    state.cameras[0].notice = CAMERA_NOTICE_TIME * 0.75f;
    state.cameras[0].suspicion = CAMERA_SUSPICION_FADE;

    float cx = 0.0f, cy = 0.0f, cw = 0.0f, ch = 0.0f;
    gameplay_camera_box(&state.level.map.cameras[0], &cx, &cy, &cw, &ch);
    state.flashbangs[0].active = true;
    state.flashbangs[0].x = cx + cw * 0.5f;
    state.flashbangs[0].y = cy + ch * 0.5f;
    state.flashbangs[0].timer = 0.0f;
    for (int step = 0; step < SIM_STEPS(10.0f) && state.flashbangs[0].active;
         ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    REQUIRE(!state.flashbangs[0].active);

    CHECK(state.cameras[0].notice == 0.0f);
    CHECK(state.cameras[0].suspicion == 0.0f);
    /* Still on the ceiling, and still sweeping: only a round or a blast takes a
     * camera off it, and a charge that quietly destroyed one would make the
     * flash the answer to a fitting the player is meant to route around. */
    CHECK(state.cameras[0].working);
    CHECK(!gameplay_alarm_active(&state));

    /* Out of reach is out of reach — the same charge across the room leaves the
     * count where it was, or `FLASH_RADIUS` would not be a radius. */
    state.cameras[0].notice = CAMERA_NOTICE_TIME * 0.75f;
    state.flashbangs[0].active = true;
    state.flashbangs[0].x = cx + cw * 0.5f + 3.0f * FLASH_RADIUS;
    state.flashbangs[0].y = cy + ch * 0.5f;
    state.flashbangs[0].timer = 0.0f;
    for (int step = 0; step < SIM_STEPS(10.0f) && state.flashbangs[0].active;
         ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    CHECK(state.cameras[0].notice > 0.0f);
}

static void test_a_flash_charge_reaches_the_dog_as_well(void)
{
    static const char data[] =
        "################\n"
        "#S   W        E#\n"
        "################\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 4242);
    REQUIRE(level_load_data(&state.level, "flash-dog", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.dog_count == 1);
    player_reset(&state.player, &state.level);

    Dog *dog = &state.dogs[0];
    dog->on_ground = true;
    dog->state = DOG_CHASE;
    dog->has_chase_target = true;
    dog->chase_target_x = state.player.x;
    /* Mid-lunge, with the teeth one beat away. */
    dog->bite_windup = DOG_BITE_WINDUP;
    dog->bite_ready = false;
    int hp_before = dog->hp;

    state.flashbangs[0].active = true;
    state.flashbangs[0].x = dog->x;
    state.flashbangs[0].y = dog->y;
    state.flashbangs[0].timer = 0.0f;
    for (int step = 0; step < SIM_STEPS(10.0f) &&
         state.flashbangs[0].active;
         ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    CHECK(!state.flashbangs[0].active);

    /* Blinded, and the lunge taken off him. */
    CHECK(dog->blind_timer > 0.0f);
    CHECK(dog->bite_windup == 0.0f);
    CHECK(!dog->bite_ready);
    /* Untouched — it is not a weapon, to him either. */
    CHECK(dog->hp == hp_before);
    CHECK(!dog->dead);
    CHECK(state.player.hp == PLAYER_MAX_HP);
    /* And still hunting: the charge buys seconds, never the encounter. */
    CHECK(dog->has_chase_target);

    /* While it burns he sees nothing, and he stays where he was put however
     * plainly Chuck stands beside him. */
    state.player.x = dog->x + DOG_W + 4.0f;
    state.player.y = dog->y;
    float stood_at = dog->x;
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    CHECK(dog->bite_windup == 0.0f);
    CHECK(fabsf(dog->x - stood_at) < 1.0f);

    /* And he comes back, for the reason the man does. */
    int steps = 0;
    while (dog->blind_timer > 0.0f && steps < SIM_STEPS(20.0f))
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        ++steps;
    }
    CHECK(dog->blind_timer <= 0.0f);
    for (int step = 0; step < SIM_STEPS(2.0f); ++step)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    CHECK(dog->state == DOG_CHASE);
}

static void test_a_bolt_pulls_a_guard_to_where_it_landed(void)
{
    static const char data[] =
        "##########################\n"
        "#                        #\n"
        "#S        M             E#\n"
        "##########################\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 4242);
    REQUIRE(level_load_data(&state.level, "bolt", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->on_ground = true;

    float player_x = state.player.x + PLAYER_W * 0.5f;
    state.player.facing = 1;
    state.player.active_weapon = PLAYER_WEAPON_DECOY;
    Input throw_bolt = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &throw_bolt);
    CHECK(state.decoys[0].active);
    /* Nothing was spent: the bolts are a clock, not a count, and the hand keeps
     * them so the second throw does not need a bumper press first. */
    CHECK(state.player.active_weapon == PLAYER_WEAPON_DECOY);
    CHECK(state.player.decoy_cooldown == DECOY_COOLDOWN);

    for (int step = 0; step < SIM_STEPS(8.0f) && state.decoys[0].active; ++step)
        gameplay_combat_update_decoys(&state, SIM_STEP_DT);
    CHECK(!state.decoys[0].active); /* it landed rather than flying forever */
    /* Deactivating a bolt does not move it, so the slot still holds the tile it
     * came down on — which is the position the noise was reported from. */
    float landing_x = state.decoys[0].x;
    float landing_y = state.decoys[0].y;
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_GRENADE_BOUNCE));

    /* It came down well away from the man who threw it. Without this the check
     * below would pass for a bolt that never left Chuck's hand. */
    CHECK(landing_x - player_x > 4.0f * TILE_SIZE);

    /* And that is where the guard is going — not to Chuck. */
    CHECK(guard->investigate_timer > 0.0f);
    CHECK(fabsf(guard->investigate_x -
                (landing_x + DECOY_W * 0.5f)) < 1.0f);
    CHECK(fabsf(guard->investigate_y -
                (landing_y + DECOY_H * 0.5f)) < 1.0f);
    CHECK(fabsf(guard->investigate_x - player_x) > 4.0f * TILE_SIZE);

    /* A bolt is not a weapon. It hurts nobody, wakes nobody into a fight, and
     * leaves the floor exactly as quiet as it found it. */
    CHECK(guard->hp == ENEMY_HP);
    CHECK(!guard->provoked);
    CHECK(!guard->raising_alarm);
    CHECK(!gameplay_alarm_active(&state));

    /* The cooldown is the whole limit on them, so it has to actually hold. */
    state.events.count = 0;
    Input second = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &second);
    for (int i = 0; i < MAX_DECOYS; ++i)
        CHECK(!state.decoys[i].active);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND, SFX_EMPTY_CLICK));

    /* And it runs down rather than latching. */
    for (int step = 0; step < SIM_STEPS(8.0f) &&
         state.player.decoy_cooldown > 0.0f;
         ++step)
        gameplay_combat_update_decoys(&state, SIM_STEP_DT);
    CHECK(state.player.decoy_cooldown == 0.0f);
    Input third = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &third);
    CHECK(state.decoys[0].active);
}

/*
 * A bolt makes its noise where it hit the wall, not at the foot of it.
 *
 * The test above throws down a corridor, so the floor is always what stops the
 * bolt and a broken wall check passes it regardless — which is how the wall half
 * of `gameplay_combat_update_decoys` shipped inverted. `level_move` *zeroes* the
 * axis it blocks, so the old `decoy->vx != 0.0f` was the one condition a strike
 * can never meet: the bolt stayed live, slid down the masonry and reported from
 * the floor up to a storey below where the player aimed. In the ducts, where six
 * crawl levels are stacked, that is the wrong level entirely.
 *
 * So this one throws at a wall across a shaft with the floor a long way down,
 * and pins the height the noise came from. A guard standing on the walkway at
 * the impact goes to look; the one on the floor below does not.
 */
static void test_a_bolt_makes_its_noise_where_it_hit_the_wall(void)
{
    static const char data[] =
        "##########\n"
        "#S   #   #\n"
        "#    #   #\n"
        "#    #   #\n"
        "#    #   #\n"
        "#       E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "bolt-wall", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);

    /* Thrown flat at head height on the top storey, at the masonry two tiles
     * along. The floor is four storeys below it. */
    const float throw_row_y = 1.0f * TILE_SIZE + 4.0f;
    state.decoys[0].active = true;
    state.decoys[0].x = 2.0f * TILE_SIZE;
    state.decoys[0].y = throw_row_y;
    state.decoys[0].vx = DECOY_THROW_SPEED;
    state.decoys[0].vy = 0.0f;

    for (int step = 0; step < SIM_STEPS(10.0f) && state.decoys[0].active; ++step)
        gameplay_combat_update_decoys(&state, SIM_STEP_DT);
    CHECK(!state.decoys[0].active);

    /* It stopped against the wall face rather than carrying through it. */
    const float wall_face = 5.0f * TILE_SIZE;
    CHECK(state.decoys[0].x + DECOY_W <= wall_face + 1.0f);
    CHECK(state.decoys[0].x > 4.0f * TILE_SIZE);

    /*
     * And it stopped *up there*. Gravity is on it for the two tiles of flight,
     * so it arrives about a tile and a half below the throw — but nothing like
     * the four storeys it used to slide. Two tiles of tolerance is the whole
     * assertion, and the margin either side of it is wide: the impact sits near
     * 81px and the floor of the shaft, where the broken version reported from,
     * is at 186.
     */
    CHECK(state.decoys[0].y < throw_row_y + 2.0f * TILE_SIZE);
    CHECK(state.decoys[0].y < 4.0f * TILE_SIZE);
}

/* Stand Chuck behind a guard with the blade in his hand and swing once. The
 * caller decides which way the guard is looking and what else he knows. */
static void knife_one_guard(GameplayState *state, CampaignState *campaign,
                            int guard_dir)
{
    state->enemy_count = 1;
    state->enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f,
                                .hp = ENEMY_HP, .dir = guard_dir,
                                .on_ground = true};
    state->player.x = 80.0f; /* the knife reaches from 106 to 124 */
    state->player.y = 200.0f;
    state->player.facing = 1;
    state->player.bullets = 0;
    state->player.active_weapon = PLAYER_WEAPON_KNIFE;
    Input swing = {.shoot = true};
    gameplay_combat_handle_player_action(state, campaign, &swing);
}

/*
 * The blade behind a man who has not seen you.
 *
 * The knife used to be what Chuck was left holding when the clip ran dry:
 * three swings at somebody who turns round after the first. That made the one
 * weapon which never runs out the one weapon nobody would ever choose, and it
 * left the whole perception model — the cone, the peripheral radius, the
 * encounter window — with exactly one answer available to the player, which was
 * to shoot first and deal with the floor that heard it.
 *
 * All four halves of the rule are here, because each of them is a way the
 * mechanic could quietly become "the knife is now a one-hit kill".
 */
static void test_a_guard_who_never_saw_it_coming_goes_down_at_once(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};

    /* Facing away, alarm down, nobody has shot him: one swing. */
    knife_one_guard(&state, &campaign, 1);
    CHECK(state.enemies[0].dead);
    CHECK(campaign.score == PLAYER_TAKEDOWN_SCORE);
    CHECK(state.hostiles_neutralized == 1);
    CHECK(campaign.hostiles_down == 1);
    /* Direct combat, so the magazine still drops. */
    CHECK(state.ammo_drops[0].active);

    /* Facing him, it is the ordinary knife: one hit of three, and he is now
     * hunting the man who swung it. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    knife_one_guard(&state, &campaign, -1);
    CHECK(!state.enemies[0].dead);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);
    CHECK(state.enemies[0].provoked);
    CHECK(campaign.score == 0);

    /* A ringing alarm is the whole floor knowing, so there is no such thing as
     * an unaware man in it — even one still facing the other way. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    state.terminal_alarm_timer = ALARM_CALM_TIME;
    knife_one_guard(&state, &campaign, 1);
    CHECK(!state.enemies[0].dead);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);

    /* And a guard who has already decided an encounter has seen Chuck, whatever
     * way his patrol happens to be pointing this frame. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP,
                               .dir = 1, .on_ground = true,
                               .encounter_decided = true};
    state.player.x = 80.0f;
    state.player.y = 200.0f;
    state.player.facing = 1;
    state.player.bullets = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    Input swing = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &swing);
    CHECK(!state.enemies[0].dead);
    CHECK(state.enemies[0].hp == ENEMY_HP - 1);
}

/*
 * And the half that is the actual point of it: what the rest of the room hears.
 *
 * `damage_enemy` calls `gameplay_provoke_enemy`, which wakes the man who was hit
 * *and the partner he was talking to* — that is what turns one guard going down
 * into two guards hunting. A takedown skips it, and if it ever stops skipping
 * it, the mechanic still looks exactly right on screen while having lost the
 * only thing it was for.
 */
static void test_a_takedown_does_not_wake_the_man_he_was_talking_to(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.enemy_count = 2;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP,
                               .dir = 1, .on_ground = true,
                               .talking = true, .talk_partner = 1};
    state.enemies[1] = (Enemy){.x = 160.0f, .y = 200.0f, .hp = ENEMY_HP,
                               .dir = -1, .on_ground = true,
                               .talking = true, .talk_partner = 0};
    state.player.x = 80.0f;
    state.player.y = 200.0f;
    state.player.facing = 1;
    state.player.bullets = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    Input swing = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &swing);

    CHECK(state.enemies[0].dead);
    CHECK(!state.enemies[1].provoked);
    CHECK(!state.enemies[1].has_pursuit_target);

    /* The same pair and the same swing, with the near man facing Chuck so it
     * lands as an ordinary knife hit: his partner is hunting within the frame.
     * That is the contrast the rule above exists to draw, and taking it from
     * the same code path is what makes the comparison worth anything. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    state.enemy_count = 2;
    state.enemies[0] = (Enemy){.x = 100.0f, .y = 200.0f, .hp = ENEMY_HP,
                               .dir = -1, .on_ground = true,
                               .talking = true, .talk_partner = 1};
    state.enemies[1] = (Enemy){.x = 160.0f, .y = 200.0f, .hp = ENEMY_HP,
                               .dir = -1, .on_ground = true,
                               .talking = true, .talk_partner = 0};
    state.player.x = 80.0f;
    state.player.y = 200.0f;
    state.player.facing = 1;
    state.player.bullets = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    Input alerting_swing = {.shoot = true};
    gameplay_combat_handle_player_action(&state, &campaign, &alerting_swing);

    CHECK(!state.enemies[0].dead);
    CHECK(state.enemies[1].provoked);
}

/* Climbing down onto a guard blocking the ladder used to still die: the
 * bounce set an upward vy, but the ladder's own climb logic overwrote it
 * with the climb speed the very next frame, driving Chuck back down into a
 * lethal, deep contact. A lockout must hold the player off the ladder long
 * enough for the bounce to actually clear the guard. */
static void test_ladder_descent_onto_enemy_bounces_instead_of_killing(void)
{
    static const char data[] =
        "########\n"
        "#  S  E#\n"
        "#  H   #\n"
        "#  H   #\n"
        "#  H   #\n"
        "#      #\n"
        "########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 5);
    CHECK(level_load_data(&state.level, "ladder stomp", data, strlen(data),
                          &state.rng));

    const int ladder_col = 3;
    state.player.x =
        (float)ladder_col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = 2.0f * TILE_SIZE;

    state.enemy_count = 1;
    state.enemies[0] = (Enemy){
        .x = (float)ladder_col * TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f,
        .y = 5.0f * TILE_SIZE - (float)ENEMY_H,
        .hp = ENEMY_HP,
        .on_ground = true};

    Input down = {.down = true};
    for (int frame = 0; frame < SIM_STEPS(5.0f) && !state.enemies[0].dead; ++frame)
    {
        player_update(&state.player, &state.level, &down, SIM_STEP_DT);
        gameplay_combat_check_contacts(&state, &campaign);
        CHECK(!state.player.dying);
    }

    CHECK(state.enemies[0].dead);
    CHECK(campaign.score == 150);
}

static void test_enemy_spawn_uses_seeded_rng(void)
{
    static const char data[] =
        "#######\n"
        "#S M E#\n"
        "#######\n";
    GameplayState first = {0};
    GameplayState second = {0};
    rng_seed(&first.rng, 9876);
    rng_seed(&second.rng, 9876);
    CHECK(level_load_data(&first.level, "ai", data, strlen(data),
                          &first.rng));
    CHECK(level_load_data(&second.level, "ai", data, strlen(data),
                          &second.rng));
    gameplay_ai_spawn_level_entities(&first);
    gameplay_ai_spawn_level_entities(&second);
    CHECK(first.enemy_count == 1);
    CHECK(second.enemy_count == 1);
    CHECK(first.enemies[0].dir == second.enemies[0].dir);
    CHECK(fabsf(first.enemies[0].shoot_cooldown -
                second.enemies[0].shoot_cooldown) < 0.0001f);
}

static void test_janitor_ai_is_seeded_and_visual_only(void)
{
    static const char data[] =
        "############\n"
        "#S J     E #\n"
        "############\n";
    GameplayState first = {0};
    GameplayState second = {0};
    rng_seed(&first.rng, 2468);
    rng_seed(&second.rng, 2468);
    CHECK(level_load_data(&first.level, "janitor", data, strlen(data),
                          &first.rng));
    CHECK(level_load_data(&second.level, "janitor", data, strlen(data),
                          &second.rng));
    CHECK(first.level.map.janitor_count == 1);
    gameplay_ai_spawn_level_entities(&first);
    gameplay_ai_spawn_level_entities(&second);
    CHECK(first.janitor_count == 1);
    CHECK(second.janitor_count == 1);
    CHECK(first.janitors[0].dir == second.janitors[0].dir);
    CHECK(first.janitors[0].activity == JANITOR_MOP);
    CHECK(fabsf(first.janitors[0].activity_timer -
                second.janitors[0].activity_timer) < 0.0001f);

    first.player.x = 71.0f;
    first.player.y = 19.0f;
    gameplay_ai_update_movement(&first, 0.1f);
    CHECK(first.player.x == 71.0f);
    CHECK(first.player.y == 19.0f);
    CHECK(first.events.count == 0);
    CHECK(first.janitors[0].wet_spots[0].active);
}

static void test_civilians_flee_to_the_way_in_and_vanish(void)
{
    static const char data[] =
        "################\n"
        "#S     f    f E#\n"
        "################\n";
    GameplayState first = {0};
    GameplayState second = {0};
    rng_seed(&first.rng, 8642);
    rng_seed(&second.rng, 8642);
    CHECK(level_load_data(&first.level, "flee", data, strlen(data),
                          &first.rng));
    CHECK(level_load_data(&second.level, "flee", data, strlen(data),
                          &second.rng));
    CHECK(first.level.map.civilian_count == 2);
    gameplay_ai_spawn_level_entities(&first);
    gameplay_ai_spawn_level_entities(&second);
    CHECK(first.civilian_count == 2);
    /* One seed, one evacuation: who bolts first and who trips is fixed. */
    CHECK(first.civilians[0].activity_timer ==
          second.civilians[0].activity_timer);
    CHECK(first.civilians[1].speed == second.civilians[1].speed);
    /* Both start turned toward what came through the door, and both leave
     * towards the tile the player entered on. */
    CHECK(first.civilians[0].activity == CIVILIAN_STARTLED);
    CHECK(first.civilians[0].flee_dir == -1);
    CHECK(first.civilians[0].dir == 1);

    float player_x = first.player.x;
    float player_y = first.player.y;
    bool shouted = false;
    bool ran = false;
    for (int frame = 0; frame < SIM_STEPS(10.0f); ++frame)
    {
        gameplay_ai_update_movement(&first, SIM_STEP_DT);
        shouted = shouted ||
                  events_have_sound(&first.events, GAME_EVENT_WORLD_SOUND,
                                    SFX_CIVILIAN_SHOUT) ||
                  events_have_sound(&first.events, GAME_EVENT_WORLD_SOUND,
                                    SFX_CIVILIAN_SCREAM);
        ran = ran || first.civilians[0].activity == CIVILIAN_FLEEING;
        game_events_clear(&first.events);
    }
    CHECK(shouted);
    CHECK(ran);
    /* The room is empty afterwards, and the evacuation moved nothing else. */
    CHECK(first.civilians[0].activity == CIVILIAN_GONE);
    CHECK(first.civilians[1].activity == CIVILIAN_GONE);
    CHECK(first.civilians[0].fade == 0.0f);
    CHECK(first.player.x == player_x);
    CHECK(first.player.y == player_y);
    /* Nobody walks over the tile the player is standing on to get out. */
    CHECK(first.civilians[0].x > first.level.map.start_x);
}

static void test_walled_in_civilian_leaves_instead_of_running_on_the_spot(void)
{
    static const char data[] =
        "############\n"
        "#S  #E    f#\n"
        "############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 24680);
    CHECK(level_load_data(&state.level, "walled flee", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.civilian_count == 1);

    Civilian *civilian = &state.civilians[0];
    civilian->activity_timer = 0.0f;
    float wall_right = 5.0f * TILE_SIZE;
    for (int frame = 0; frame < SIM_STEPS(10.0f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        game_events_clear(&state.events);
        CHECK(civilian->x >= wall_right - 1.0f);
    }
    CHECK(civilian->activity == CIVILIAN_GONE);
}

static void test_janitor_cart_stays_clear_when_turning_at_wall(void)
{
    static const char data[] =
        "##########\n"
        "#S E  J###\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 1357);
    CHECK(level_load_data(&state.level, "janitor turn", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.janitor_count == 1);

    Janitor *janitor = &state.janitors[0];
    janitor->dir = 1;
    janitor->cart_dir = 1;
    janitor->activity = JANITOR_WALK;
    janitor->activity_timer = 100.0f;
    janitor->on_ground = true;

    gameplay_ai_update_movement(&state, 0.1f);
    CHECK(janitor->dir == -1);
    CHECK(janitor->cart_dir == 1);
    CHECK(gameplay_box_tiles_clear(
        &state, janitor->x - JANITOR_CART_SIDE_EXTENT, janitor->y,
        JANITOR_W + JANITOR_CART_SIDE_EXTENT, JANITOR_H, STANCE_UPRIGHT));
    CHECK(!gameplay_box_tiles_clear(
        &state, janitor->x, janitor->y,
        JANITOR_W + JANITOR_CART_SIDE_EXTENT, JANITOR_H, STANCE_UPRIGHT));

    janitor->activity = JANITOR_WALK;
    janitor->activity_timer = 100.0f;
    for (int frame = 0; frame < SIM_STEPS(1.5f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        float collision_x = janitor->cart_dir > 0
                                ? janitor->x - JANITOR_CART_SIDE_EXTENT
                                : janitor->x;
        CHECK(gameplay_box_tiles_clear(
            &state, collision_x, janitor->y,
            JANITOR_W + JANITOR_CART_SIDE_EXTENT, JANITOR_H, STANCE_UPRIGHT));
    }
    CHECK(janitor->cart_dir == -1);
}

/*
 * The desk post. What is being pinned is that the errand is a round trip: the
 * receptionist leaves the counter, gets far enough away for it to be worth
 * watching, and is back on the exact spawn tile afterwards. A patrol that
 * merely happens to pass the desk would leave the lobby unstaffed for the
 * second half of the level.
 */
static void test_receptionist_works_a_post_and_returns_to_it(void)
{
    static const char data[] =
        "###############\n"
        "#SE        nnk#\n"
        "###############\n";
    GameplayState first = {0};
    GameplayState second = {0};
    rng_seed(&first.rng, 9182);
    rng_seed(&second.rng, 9182);
    CHECK(level_load_data(&first.level, "desk", data, strlen(data),
                          &first.rng));
    CHECK(level_load_data(&second.level, "desk", data, strlen(data),
                          &second.rng));
    CHECK(first.level.map.receptionist_count == 1);
    gameplay_ai_spawn_level_entities(&first);
    gameplay_ai_spawn_level_entities(&second);
    CHECK(first.receptionist_count == 1);
    CHECK(second.receptionist_count == 1);

    Receptionist *desk = &first.receptionists[0];
    /* The post is against the right-hand wall, so the room — and the only way
     * an errand can go — is to the left. */
    CHECK(desk->desk_dir == -1);
    CHECK(desk->activity == RECEPTIONIST_DESK);
    CHECK(desk->post_x == first.level.map.receptionist_spawns[0].x);
    CHECK(fabsf(desk->activity_timer -
                second.receptionists[0].activity_timer) < 0.0001f);

    first.player.x = 48.0f;
    first.player.y = 32.0f;
    bool left_the_desk = false;
    bool ran_an_errand = false;
    bool came_back = false;
    float furthest = 0.0f;
    for (int frame = 0; frame < SIM_STEPS(120.0f); ++frame)
    {
        gameplay_ai_update_movement(&first, SIM_STEP_DT);
        gameplay_ai_update_movement(&second, SIM_STEP_DT);
        float away = fabsf(desk->x - desk->post_x);
        if (away > furthest)
            furthest = away;
        if (desk->activity == RECEPTIONIST_WALK)
            left_the_desk = true;
        if (desk->activity == RECEPTIONIST_ERRAND)
            ran_an_errand = true;
        if (left_the_desk && ran_an_errand &&
            desk->activity == RECEPTIONIST_DESK)
            came_back = true;
        /* An errand that walks off the map, or through the counter's wall,
         * would be a route rather than staging. */
        CHECK(desk->x > (float)TILE_SIZE);
        CHECK(desk->x + RECEPTIONIST_W < 14.0f * TILE_SIZE);
        CHECK(desk->x == second.receptionists[0].x);
    }
    CHECK(left_the_desk);
    CHECK(ran_an_errand);
    CHECK(came_back);
    /* Worth watching: the walk out clears the counter run itself. */
    CHECK(furthest >= RECEPTIONIST_ERRAND_MIN_REACH - 1.0f);
    /* And the post is the spawn tile again, not near it. */
    CHECK(desk->activity != RECEPTIONIST_DESK ||
          fabsf(desk->x - desk->post_x) < 0.001f);

    /* Scenery, like the janitor and the civilians: nothing was touched and
     * nothing was announced. */
    CHECK(first.player.x == 48.0f);
    CHECK(first.player.y == 32.0f);
    CHECK(first.events.count == 0);
}

/* A post with no room either side is a waste of the part, not a crash: the
 * errand ends where it started and the desk goes on being staffed. */
static void test_boxed_in_receptionist_stays_on_the_desk(void)
{
    static const char data[] =
        "########\n"
        "#S E #k#\n"
        "########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 3141);
    CHECK(level_load_data(&state.level, "boxed desk", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.receptionist_count == 1);

    Receptionist *desk = &state.receptionists[0];
    float post_x = desk->post_x;
    for (int frame = 0; frame < SIM_STEPS(60.0f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        CHECK(fabsf(desk->x - post_x) < (float)TILE_SIZE);
    }
    CHECK(state.events.count == 0);
}

static void test_enemy_vision_cone_stealth_and_walls(void)
{
    /* Open corridor: a standing Chuck five tiles ahead is spotted and aimed at;
     * the same spot while crawling is beyond the reduced detection range. */
    static const char open_data[] =
        "############\n"
        "#S     M  E#\n"
        "############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 321);
    CHECK(level_load_data(&state.level, "sight", open_data, strlen(open_data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->dir = -1;
    guard->on_ground = true;
    guard->shoot_cooldown = 0.0f;
    guard->encounter_decided = true; /* isolate the aim decision from alarms */
    /* The sighting has already been held through the notice beat; this test
     * is about the vision cone, not the suspicion ramp. */
    guard->sight_timer = ENEMY_NOTICE_TIME;
    state.player.y = guard->y;
    state.player.x = guard->x - 5.0f * TILE_SIZE;

    state.player.crawling = false;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(guard->aim_timer > 0.0f);

    guard->aim_timer = 0.0f;
    guard->shoot_cooldown = 0.0f;
    state.player.crawling = true;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(guard->aim_timer == 0.0f);

    /* A pillar between guard and Chuck blocks the ray-cast line of sight. */
    static const char wall_data[] =
        "#############\n"
        "#S  #    M E#\n"
        "#############\n";
    GameplayState walled = {0};
    rng_seed(&walled.rng, 321);
    CHECK(level_load_data(&walled.level, "wall", wall_data, strlen(wall_data),
                          &walled.rng));
    gameplay_ai_spawn_level_entities(&walled);
    Enemy *wguard = &walled.enemies[0];
    wguard->dir = -1;
    wguard->on_ground = true;
    wguard->shoot_cooldown = 0.0f;
    wguard->encounter_decided = true;
    walled.player.y = wguard->y;
    walled.player.x = wguard->x - 6.0f * TILE_SIZE;
    walled.player.crawling = false;
    gameplay_ai_update_combat(&walled, SIM_STEP_DT);
    CHECK(wguard->aim_timer == 0.0f);
}

static void test_enemy_fires_vertical_shot_up_a_shaft(void)
{
    static const char data[] =
        "#S   #\n"
        "#    #\n"
        "#    #\n"
        "#M  E#\n"
        "######\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 77);
    CHECK(level_load_data(&state.level, "shaft", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->on_ground = true;
    guard->shoot_cooldown = 0.0f;
    guard->encounter_decided = true;
    guard->sight_timer = ENEMY_NOTICE_TIME; /* past the suspicion ramp */
    /* Chuck is directly above the guard, three tiles up the shaft. */
    state.player.x = guard->x;
    state.player.y = guard->y - 3.0f * TILE_SIZE;

    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(guard->aim_timer > 0.0f);
    CHECK(guard->aim_vdir == -1);

    gameplay_ai_update_combat(&state, guard->aim_timer + 0.001f);
    bool fired_up = false;
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        const Bullet *bullet = &state.enemy_bullets[i];
        if (bullet->active && bullet->vx == 0.0f && bullet->vy < 0.0f)
        {
            fired_up = true;
            CHECK(fabsf((bullet->x + BULLET_H * 0.5f) -
                        (guard->x + ENEMY_W * 0.5f)) < 0.01f);
            CHECK(fabsf(bullet->y - (guard->y - BULLET_W)) < 0.01f);
        }
    }
    CHECK(fired_up);
}

static void test_noise_draws_guards_to_investigate(void)
{
    static const char data[] =
        "##############\n"
        "#S  M        E#\n"
        "##############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 4242);
    CHECK(level_load_data(&state.level, "noise", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->on_ground = true;
    guard->dir = -1; /* start facing away from the disturbance */

    float noise_x = guard->x + ENEMY_W * 0.5f + 4.0f * TILE_SIZE;
    float noise_y = guard->y + ENEMY_H * 0.5f;
    gameplay_alert_enemies_to_noise(&state, noise_x, noise_y,
                                    ENEMY_HEAR_RADIUS_SHOT);
    CHECK(guard->investigate_timer > 0.0f);
    CHECK(guard->dir == 1); /* turned toward the sound */

    float previous_x = guard->x;
    gameplay_ai_update_movement(&state, 0.1f);
    CHECK(guard->x > previous_x); /* walked toward the disturbance */
}

static void test_guard_investigates_fallen_comrade(void)
{
    static const char data[] =
        "##########\n"
        "#S M M  E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 88);
    CHECK(level_load_data(&state.level, "body", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 2);
    Enemy *witness = &state.enemies[0];
    witness->on_ground = true;
    witness->dir = 1;             /* face the neighbouring guard, away from Chuck */
    state.enemies[1].dead = true; /* the comrade lies dead nearby */

    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK((witness->bodies_investigated & enemy_body_bit(1, false)) != 0);
    CHECK(witness->investigate_timer > 0.0f || witness->raising_alarm);
}

/*
 * And he reacts to the next one too.
 *
 * The flag this replaced was one bool per guard, so a man who had walked over
 * to look at a corpse was blind to every other for the rest of the sector —
 * which meant the whole rule quietly switched itself off at exactly the point
 * a player starts leaving bodies about. One bit per body says what was meant:
 * the corpse he has already dealt with stops calling him, and a second one
 * lying somewhere else still does.
 */
static void test_a_guard_notices_the_second_body_as_well(void)
{
    /* Both bodies inside ENEMY_BODY_NOTICE_RANGE of the witness — one and two
     * tiles out of three — and Chuck parked eight tiles the other way, well
     * past ENEMY_RETALIATE_RADIUS, so nothing turns the man round mid-test. */
    static const char data[] =
        "##################\n"
        "#S       MMM    E#\n"
        "##################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 88);
    CHECK(level_load_data(&state.level, "bodies", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 3);
    Enemy *witness = &state.enemies[0];
    witness->on_ground = true;
    witness->dir = 1; /* facing the two of them, away from Chuck */
    state.enemies[1].dead = true;

    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK((witness->bodies_investigated & enemy_body_bit(1, false)) != 0);

    /* Done with the first: clear what the sighting started, so nothing but the
     * mask can be what stops him reacting again. */
    witness->investigate_timer = 0.0f;
    witness->raising_alarm = false;
    witness->alarm_switch_index = -1;
    CHECK(!gameplay_alarm_active(&state));

    /* The same corpse is not worth a second walk. */
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(witness->investigate_timer <= 0.0f && !witness->raising_alarm);

    /* A different one is. */
    state.enemies[2].dead = true;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK((witness->bodies_investigated & enemy_body_bit(2, false)) != 0);
    CHECK(witness->investigate_timer > 0.0f || witness->raising_alarm);
}

/*
 * What a guard *does* about the body he has just found, which is the half of
 * body discovery nothing asked.
 *
 * Everything above and around it covers the noticing: the range, the mask that
 * stops a corpse being walked to twice, the second body, the dragged one, the
 * slot that got recycled. All of them then assert
 * `investigate_timer > 0 || raising_alarm` — an `||`, so a suite in which the
 * left side is always the one taken passes every time, and that is exactly what
 * `make coverage` found. `guard_run_to_alarm` was reached from the *sighting*
 * route and never from this one; the corpse route's own call, and the roll of
 * `GUARD_BODY_ALARM_CHANCE` in front of it, were dark.
 *
 * That is the same defect as `test_coyote_time_allows_a_late_jump`'s two
 * branches taking one path, on the mechanic the whole quiet route is played
 * around. A player hides a body *because* the man who finds it may go for a
 * switch; if that never happened the route would have no risk in it and nothing
 * in the suite would notice the difference.
 *
 * Seeded many times over rather than once, because it is a roll and one seed
 * pins whichever way it happened to land. Both outcomes are required: a floor
 * where every witness runs for the alarm leaves no reason to hide anything, and
 * one where none of them does is the bug this exists to catch.
 */
static void test_a_guard_who_finds_a_body_may_run_for_the_alarm(void)
{
    /* The witness starts facing the corpse and away from Chuck, who is parked
     * at the far end past ENEMY_RETALIATE_RADIUS so nothing but the body is
     * worth reacting to. Two switches, so the choice of *which* is a claim as
     * well: the near one is four tiles from the body, the far one seventeen. */
    static const char data[] =
        "########################\n"
        "#S        A       A    E#\n"
        "#         MM          ##\n"
        "########################\n";
    Level level;
    Rng level_rng;
    rng_seed(&level_rng, 1861);
    CHECK(level_load_data(&level, "body alarm", data, strlen(data),
                          &level_rng));
    REQUIRE(level.map.alarm_switch_count == 2);

    int ran_for_switch = 0;
    int shouted_instead = 0;
    for (uint64_t seed = 1; seed <= 64; ++seed)
    {
        static GameplayState state;
        memset(&state, 0, sizeof(state));
        state.level = level;
        rng_seed(&state.rng, seed);
        gameplay_ai_spawn_level_entities(&state);
        REQUIRE(state.enemy_count == 2);

        state.player.x = (float)state.level.map.start_x;
        state.player.y = (float)state.level.map.start_y;

        Enemy *witness = &state.enemies[0];
        witness->on_ground = true;
        witness->dir = 1;
        state.enemies[1].dead = true;

        gameplay_ai_update_combat(&state, SIM_STEP_DT);

        /* Either way he has dealt with this corpse and must not deal with it
         * again — the alarm branch owes the mask exactly what the shout does. */
        CHECK((witness->bodies_investigated & enemy_body_bit(1, false)) != 0);

        if (witness->raising_alarm)
        {
            ran_for_switch++;
            /* A man who has decided to raise it has to know where, and it has
             * to be the near switch: `nearest_alarm_switch` picking the far one
             * would send him the length of the floor past the body he found. */
            CHECK(witness->alarm_switch_index == 0);
            CHECK(!gameplay_alarm_active(&state)); /* not yet — he has to reach it */
        }
        else
        {
            shouted_instead++;
            CHECK(witness->alarm_switch_index < 0);
            CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                                    SFX_ENEMY_ALERT));
        }
    }
    CHECK(ran_for_switch > 0);
    CHECK(shouted_instead > 0);

    /* And the run has to end in an alarm, or the decision costs the player
     * nothing. Walked from the body to the switch and held there. */
    static GameplayState raised;
    memset(&raised, 0, sizeof(raised));
    raised.level = level;
    rng_seed(&raised.rng, 4);
    gameplay_ai_spawn_level_entities(&raised);
    REQUIRE(raised.enemy_count == 2);
    raised.enemies[1].dead = true;

    Enemy *runner = &raised.enemies[0];
    const AlarmSwitch *target = &raised.level.map.alarm_switches[0];
    runner->x = (target->col + 0.5f) * TILE_SIZE - ENEMY_W * 0.5f;
    runner->y = (target->row + 0.5f) * TILE_SIZE - ENEMY_H * 0.5f;
    runner->on_ground = true;
    runner->raising_alarm = true;
    runner->alarm_switch_index = 0;

    gameplay_ai_update_movement(&raised, ALARM_SWITCH_USE_TIME);
    CHECK(gameplay_alarm_active(&raised));
    CHECK(!runner->raising_alarm);
}
/*
 * And what he does when he gets there, which is the other half of the same gap.
 *
 * The test above pins the *decision* a witness makes about a corpse. This one
 * pins the walk: `make coverage` counted 4 390 executions of the suspicion block
 * in `update_enemy_pursuit` and **nought** of the branch inside it that fires
 * when the man actually reaches the spot. Every test of body discovery had
 * either asserted the first frame of it or driven a guard who never arrived, so
 * the shortening to `ENEMY_INVESTIGATE_LOOK_TIME`, the turn on the spot and the
 * drop back to patrol were staged by nothing at all.
 *
 * It is the beat the quiet route is actually played against: the cost of being
 * found out is a man walking over, looking round and *giving up* — a guard who
 * arrived and stood there for the rest of the sector, or who never stopped
 * walking, would read as a floor the player had ruined, and both of those are
 * one line from here.
 *
 * The wall beside the spawn is what makes it a test of suspicion rather than of
 * pursuit: a guard who can see Chuck is provoked, and a provoked guard does not
 * investigate anything (the timer only runs while `!provoked`).
 */
static void test_a_guard_sent_to_look_arrives_looks_and_goes_back(void)
{
    static const char data[] =
        "########################\n"
        "#S #      M           E#\n"
        "########################\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 4211);
    REQUIRE(level_load_data(&state.level, "look", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);
    REQUIRE(state.enemy_count >= 1);

    Enemy *guard = &state.enemies[0];
    CHECK(!guard->provoked);

    /* Sent four tiles back the way he came, exactly as `update_body_discovery`
     * and `gameplay_alert_enemies_to_noise` send him. */
    const float spot_x = guard->x - 4.0f * TILE_SIZE;
    guard->investigate_x = spot_x;
    guard->investigate_y = guard->y + ENEMY_H * 0.5f;
    guard->investigate_timer = ENEMY_INVESTIGATE_TIME;
    guard->investigate_scan_timer = ENEMY_INVESTIGATE_SCAN_FLIP;

    bool arrived = false;
    float arrival_time = -1.0f;
    /* The clamp is read a few steps after arriving rather than on the frame this
     * loop first sees him in reach: the pursuit pass has already run by the time
     * the position is sampled out here, so the two are a step out of phase and
     * demanding it on the nose would be pinning the phase rather than the rule. */
    const int clamp_grace = 4;
    int steps_since_arrival = 0;
    bool clamped = false;
    int flips = 0;
    int last_dir = guard->dir;
    float gave_up_time = -1.0f;
    for (int step = 0; step < SIM_STEPS(20.0f); ++step)
    {
        float now = (float)step * SIM_STEP_DT;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        gameplay_ai_update_combat(&state, SIM_STEP_DT);
        if (guard->provoked)
            break; /* the wall failed; the assertions below will say so */
        if (!arrived &&
            fabsf(spot_x - (guard->x + ENEMY_W * 0.5f)) <= ENEMY_INVESTIGATE_REACH)
        {
            arrived = true;
            arrival_time = now;
        }
        if (arrived)
        {
            if (steps_since_arrival <= clamp_grace &&
                guard->investigate_timer > 0.0f &&
                guard->investigate_timer <= ENEMY_INVESTIGATE_LOOK_TIME)
                clamped = true;
            steps_since_arrival++;
        }
        if (arrived && guard->dir != last_dir)
        {
            flips++;
            last_dir = guard->dir;
        }
        if (arrived && guard->investigate_timer <= 0.0f)
        {
            gave_up_time = now;
            break;
        }
    }

    /* He got there, and under his own steam rather than by being placed. */
    CHECK(arrived);
    CHECK(arrival_time > 0.0f);
    /* Arriving shortens what is left to a look. Without the clamp the timer
     * would still be most of ENEMY_INVESTIGATE_TIME here, and he would stand
     * over the spot for seconds after finishing with it. */
    CHECK(clamped);
    /* And he looks *round*: the scan flips him on the spot rather than leaving
     * him facing the way he walked in. */
    CHECK(flips >= 2);
    /* Then he gives up, which is the half a player is counting on. */
    CHECK(gave_up_time > arrival_time);
    CHECK(gave_up_time - arrival_time <=
          ENEMY_INVESTIGATE_LOOK_TIME + (float)clamp_grace * SIM_STEP_DT);
    CHECK(guard->investigate_timer <= 0.0f);
    CHECK(!guard->provoked);
}

/*
 * A dog that has seen nobody still hunts a raised alarm.
 *
 * The animal's own senses are covered from every side — a range, a heading, the
 * announced bite, the crate that stops it, the gap it will and will not jump —
 * and all of them start from `dog_sees_player`. What none of them reached is the
 * branch after it: the alarm is up, the dog has seen nothing at all, and it goes
 * to where the building says Chuck is anyway. That is the difference between a
 * floor with a dog on it and a floor that has been *told*, and it is the reason
 * a flash charge buys seconds rather than the encounter — the animal comes back
 * still hunting, and this is what it hunts with.
 */
static void test_a_dog_hunts_the_alarm_it_was_told_about(void)
{
    /* A long floor with the handler and his dog at the left end and Chuck at
     * the right, far outside anything the animal can sense. */
    static const char data[] =
        "##################################################\n"
        "# W                                            SE#\n"
        "##################################################\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 5150);
    REQUIRE(level_load_data(&state.level, "told", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    /* `gameplay_ai_spawn_level_entities` places the building's own people and
     * nothing else — the player is the shell's job, and left out he sits at the
     * origin, which is inside the wall at the far end from where the map says he
     * is. The alarm below is raised at his position, so without this the animal
     * is told to hunt the wrong end of the floor and this test passes the day it
     * is written by asserting it went somewhere. */
    player_reset(&state.player, &state.level);
    REQUIRE(state.dog_count == 1);
    Dog *dog = &state.dogs[0];
    dog->on_ground = true;
    /* Well outside `DOG_VIEW_RANGE`, and it stays outside it for the whole run:
     * what is under test is an animal acting on a thing it has not seen. */
    REQUIRE(state.player.x - dog->x > 2.0f * DOG_VIEW_RANGE);

    /* Nothing has been seen: no chase target, and the animal is not already in
     * a chase from some earlier frame. */
    REQUIRE(!dog->has_chase_target);
    REQUIRE(dog->state != DOG_CHASE);

    /* The alarm goes up at the far end of the floor, pointed at Chuck rather
     * than at the dog — which is what `gameplay_trigger_alarm` does for a
     * camera and for a man reaching a wall switch alike. */
    float called_at = state.player.x + PLAYER_W * 0.5f;
    gameplay_trigger_alarm(&state, called_at,
                           state.player.y + PLAYER_H * 0.5f, -1);
    REQUIRE(gameplay_alarm_active(&state));

    float started_at = dog->x;
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
    {
        state.events.count = 0;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    }

    /* It took the alarm's own target rather than a sighting it never had, and
     * it is moving toward it. */
    CHECK(dog->state == DOG_CHASE);
    CHECK(dog->has_chase_target);
    CHECK(fabsf(dog->chase_target_x - called_at) < TILE_SIZE);
    CHECK(dog->x > started_at);

    /* And when the building goes quiet again the animal gives up rather than
     * running at a target nobody is standing behind. `DOG_LOST_TIME` is the
     * patience, and it only starts counting once the alarm has lapsed. */
    state.terminal_alarm_timer = 0.0f;
    state.active_alarm_switch = -1;
    for (int step = 0; step < SIM_STEPS(DOG_LOST_TIME + 1.0f); ++step)
    {
        state.events.count = 0;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    }
    CHECK(dog->state != DOG_CHASE);
}

/*
 * A calm guard walks over to a dead dog as readily as to a dead man.
 *
 * `update_body_discovery` runs two loops — the corpses and the animals — and
 * only the first had ever been driven, because every test of it kills a guard.
 * The animal half is the one the quiet route actually meets: a dog is the thing
 * a player is most likely to have had to shoot, since there is no silent answer
 * to one, and a shot dog left lying in a corridor is a floor about to go loud.
 *
 * The bit is per body and the animals share the mask with the men, which is
 * what `enemy_body_bit(slot, true)` is for — so this also pins that a dead dog
 * in slot nought does not read as the dead guard in slot nought.
 */
static void test_a_guard_investigates_a_fallen_dog(void)
{
    /* The witness in the middle of the floor with the handler and his animal
     * two tiles along, and Chuck back at the door behind him — the same shape
     * `test_guard_investigates_fallen_comrade` uses, and for the same reason:
     * the witness faces the bodies, which is away from Chuck, so nothing turns
     * him round mid-test. */
    static const char data[] =
        "##################\n"
        "#S       M W    E#\n"
        "##################\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 88);
    REQUIRE(level_load_data(&state.level, "dead dog", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);
    REQUIRE(state.enemy_count == 2);
    REQUIRE(state.dog_count == 1);

    Enemy *witness = &state.enemies[0];
    witness->on_ground = true;
    witness->dir = 1; /* facing the animal, away from Chuck */
    /* The handler is taken out of the question entirely: he stands over his own
     * dog, so leaving him alive would make it ambiguous which of the two men
     * this test is about. */
    state.enemies[1].dead = true;
    state.enemies[1].bodies_investigated = ~(uint64_t)0;
    /* And his corpse is masked on the witness as well, so the only unclaimed
     * body on the floor is the animal. */
    witness->bodies_investigated = enemy_body_bit(1, false);

    Dog *dog = &state.dogs[0];
    dog->dead = true;
    dog->x = witness->x + 2.0f * TILE_SIZE;
    dog->y = witness->y + (float)ENEMY_H - (float)DOG_H;
    REQUIRE(2.0f * TILE_SIZE < ENEMY_BODY_NOTICE_RANGE);

    gameplay_ai_update_combat(&state, SIM_STEP_DT);

    CHECK((witness->bodies_investigated & enemy_body_bit(0, true)) != 0);
    CHECK(witness->investigate_timer > 0.0f || witness->raising_alarm);

    /* The same animal is not worth a second walk, and the mask is what says so
     * rather than the timer. */
    witness->investigate_timer = 0.0f;
    witness->raising_alarm = false;
    witness->alarm_switch_index = -1;
    state.terminal_alarm_timer = 0.0f;
    state.active_alarm_switch = -1;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(witness->investigate_timer <= 0.0f && !witness->raising_alarm);
}


/* A floor with a witness, a body two tiles from him, and room to the right to
 * haul it. Returns with Chuck standing over the body and the witness facing it.
 */
static void load_body_drag_floor(GameplayState *state)
{
    static const char data[] =
        "##############################\n"
        "#                            #\n"
        "#S   M M                    E#\n"
        "##############################\n";
    rng_seed(&state->rng, 3131);
    REQUIRE(level_load_data(&state->level, "drag", data, strlen(data),
                            &state->rng));
    player_reset(&state->player, &state->level);
    gameplay_ai_spawn_level_entities(state);
    REQUIRE(state->enemy_count == 2);

    state->enemies[0].on_ground = true;
    state->enemies[0].dir = 1; /* looking at the body, away from the wall */
    state->enemies[1].dead = true;

    state->player.y = state->enemies[1].y;
    state->player.x = state->enemies[1].x - 14.0f;
    Input still = {0};
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
        player_update(&state->player, &state->level, &still, SIM_STEP_DT);
}

/* Everything the witness learned, unlearned, so the next look is a fresh one.
 * The mask is the important half: a body is worth one walk over per guard, so
 * without clearing it the second half of the test would pass whatever the
 * corpse had been moved to. */
static void forget_the_body(GameplayState *state)
{
    state->enemies[0].bodies_investigated = 0;
    state->enemies[0].investigate_timer = 0.0f;
    state->enemies[0].raising_alarm = false;
    state->enemies[0].alarm_switch_index = -1;
    state->enemies[0].encounter_decided = false;
    state->enemies[0].provoked = false;
    state->terminal_alarm_timer = 0.0f;
    state->active_alarm_switch = -1;
}

/*
 * Hauling a body out of the room it was killed in.
 *
 * `update_body_discovery` has always sent the next calm guard who sees a corpse
 * over to look at it, and often on to the nearest alarm switch — a rule the
 * campaign's whole hazard budget assumes, and one the player could do nothing
 * whatever about: a patrol route is the single thing about a sector that cannot
 * be read off the map, so "kill him where nobody walks" was a hope rather than
 * a plan. Dragging is the answer to it, and this is the test that the answer
 * actually answers: not that the body moves, but that the *perception model*
 * reads the new position.
 *
 * Chuck is parked far off screen before each look, because the witness would
 * otherwise see the man standing over the corpse and decide an encounter, which
 * is a different rule entirely and would pass this test for the wrong reason.
 */
static void test_a_dragged_body_stops_being_found_where_it_fell(void)
{
    GameplayState state = {0};
    load_body_drag_floor(&state);

    /* The control: left where it fell, the witness finds it. */
    float parked_x = state.player.x;
    float parked_y = state.player.y;
    float witness_x = state.enemies[0].x;
    float witness_y = state.enemies[0].y;
    state.player.x = 5000.0f;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(state.enemies[0].investigate_timer > 0.0f ||
          state.enemies[0].raising_alarm);

    forget_the_body(&state);
    state.player.x = parked_x;
    state.player.y = parked_y;

    /* Take hold of it and walk right, away from the man who was about to find
     * it. Held rather than pressed: this is the same button the terminal is
     * hacked with, and both are read every frame. */
    float body_started_at = state.enemies[1].x;
    Input haul = {.interact = true, .right = true};
    gameplay_update_body_drag(&state, &haul);
    CHECK(state.player.dragging);
    CHECK(state.player.dragging_body == 1);
    CHECK(!state.player.dragging_is_dog);
    CHECK(state.player.drag_side == 1); /* it was lying on his right */

    for (int step = 0; step < SIM_STEPS(10.0f); ++step)
    {
        player_update(&state.player, &state.level, &haul, SIM_STEP_DT);
        gameplay_update_body_drag(&state, &haul);
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    }
    CHECK(state.player.dragging); /* an ordinary walk never trips the leash */

    /* The body came with him, and it is still standing on the floor rather than
     * hanging where the drag put it. */
    float travelled = state.enemies[1].x - body_started_at;
    CHECK(travelled > 4.0f * TILE_SIZE);
    CHECK(fabsf(state.enemies[1].x - (state.player.x + BODY_DRAG_OFFSET)) <
          1.0f);
    CHECK(state.enemies[1].y == parked_y);

    /* And now the witness has nothing to find.
     *
     * He is put back on the tile he was standing on for the control look, and
     * that is the point of the test rather than a convenience: he has spent the
     * last two and a half seconds walking his patrol, so leaving him wherever
     * that ended would make the two looks differ in *both* positions and the
     * result would say nothing about which of them mattered. One variable
     * changes between the halves, and it is where the body is. */
    forget_the_body(&state);
    state.enemies[0].x = witness_x;
    state.enemies[0].y = witness_y;
    state.enemies[0].dir = 1;
    state.player.x = 5000.0f;
    state.player.dragging = false;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(state.enemies[0].investigate_timer <= 0.0f);
    CHECK(!state.enemies[0].raising_alarm);
    CHECK(!gameplay_alarm_active(&state));
}

/*
 * And the animal, which is the same mechanic and had no test at all.
 *
 * `nearest_body_in_reach` scans the dog corpses after the men and
 * `Player.dragging_is_dog` says which of the two arrays the index is in, so the
 * whole of the drag is written twice — and only the man's half had ever been
 * simulated: `dragging_is_dog` was asserted in the suite exactly once, as
 * `false`. `body_slot`, the function that turns that pair into a position, ran at
 * half its regions.
 *
 * It is not a curiosity of the code either. `update_body_discovery` sends a calm
 * guard to a fallen animal as readily as to a fallen man — which
 * `test_a_guard_investigates_a_fallen_dog` already holds — and `W` puts a dog on
 * ten of the seventeen sectors, so a shot dog is one of the ordinary ways a quiet
 * floor stops being quiet. Half a mechanic covered on the half of the world that
 * happens to be human is the twin-with-one-test shape this tree keeps finding.
 *
 * Same three beats as the man's: the witness finds it, it is hauled away, the
 * witness has nothing to find.
 */
static void test_a_dragged_dog_stops_being_found_where_it_fell(void)
{
    /* The witness at the left facing right, the handler and his animal two tiles
     * along, and open floor beyond them to haul into — the same shape the man's
     * drag floor uses, and for the same reason: the witness faces the body,
     * which is away from Chuck, so nothing turns him round mid-test. */
    static const char data[] =
        "##############################\n"
        "#                            #\n"
        "#S  M    W                  E#\n"
        "##############################\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "dog drag", data, strlen(data),
                            &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    player_reset(&state.player, &state.level);
    REQUIRE(state.enemy_count == 2);
    REQUIRE(state.dog_count == 1);

    /* The handler is taken out of the question the way the discovery test takes
     * him out: he stands over his own animal, so leaving him alive would make it
     * ambiguous which man this is about. Enemy 0 is the witness. */
    state.enemies[1].dead = true;
    state.enemies[1].bodies_investigated = ~(uint64_t)0;
    Enemy *witness = &state.enemies[0];
    witness->on_ground = true;
    witness->dir = 1; /* facing the animal, away from Chuck */
    witness->bodies_investigated = enemy_body_bit(1, false);

    Dog *dog = &state.dogs[0];
    dog->dead = true;
    dog->x = witness->x + 2.0f * TILE_SIZE;
    dog->y = witness->y + (float)ENEMY_H - (float)DOG_H;
    REQUIRE(2.0f * TILE_SIZE < ENEMY_BODY_NOTICE_RANGE);

    /* The control: left where it fell, the witness finds it. Chuck is parked off
     * screen for every look, because a man standing over a corpse is an
     * encounter and that is a different rule. */
    float witness_x = witness->x;
    float witness_y = witness->y;
    state.player.x = 5000.0f;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK((witness->bodies_investigated & enemy_body_bit(0, true)) != 0);
    CHECK(witness->investigate_timer > 0.0f || witness->raising_alarm);

    /* Everything the witness learned, unlearned. The mask is the important
     * half here too: one walk over per body, so without clearing it the second
     * look would pass wherever the animal had been put. */
    witness->bodies_investigated = enemy_body_bit(1, false);
    witness->investigate_timer = 0.0f;
    witness->raising_alarm = false;
    witness->alarm_switch_index = -1;
    witness->encounter_decided = false;
    witness->provoked = false;
    state.terminal_alarm_timer = 0.0f;
    state.active_alarm_switch = -1;

    /* Take hold of it. Chuck stands to the animal's left and hauls right, away
     * from the man who was about to find it. */
    state.player.y = witness->y;
    state.player.x = dog->x - 14.0f;
    Input still = {0};
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
        player_update(&state.player, &state.level, &still, SIM_STEP_DT);

    float body_started_at = dog->x;
    Input haul = {.interact = true, .right = true};
    gameplay_update_body_drag(&state, &haul);
    CHECK(state.player.dragging);
    CHECK(state.player.dragging_is_dog);
    CHECK(state.player.dragging_body == 0);
    CHECK(state.player.drag_side == 1); /* it was lying on his right */

    for (int step = 0; step < SIM_STEPS(10.0f); ++step)
    {
        player_update(&state.player, &state.level, &haul, SIM_STEP_DT);
        gameplay_update_body_drag(&state, &haul);
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    }
    CHECK(state.player.dragging); /* an ordinary walk never trips the leash */

    /* It came with him, and it is still standing on the floor rather than
     * hanging where the drag put it. */
    float travelled = dog->x - body_started_at;
    CHECK(travelled > 4.0f * TILE_SIZE);
    CHECK(fabsf(dog->x - (state.player.x + BODY_DRAG_OFFSET)) < 1.0f);
    CHECK(dog->y == witness_y + (float)ENEMY_H - (float)DOG_H);

    /* And now the witness has nothing to find. He is put back on the tile he
     * stood on for the control look, so the one thing that differs between the
     * two halves is where the animal is. */
    witness->x = witness_x;
    witness->y = witness_y;
    witness->dir = 1;
    witness->bodies_investigated = enemy_body_bit(1, false);
    witness->investigate_timer = 0.0f;
    witness->raising_alarm = false;
    witness->alarm_switch_index = -1;
    witness->encounter_decided = false;
    witness->provoked = false;
    state.terminal_alarm_timer = 0.0f;
    state.active_alarm_switch = -1;
    state.player.x = 5000.0f;
    state.player.dragging = false;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(witness->investigate_timer <= 0.0f);
    CHECK(!witness->raising_alarm);
    CHECK(!gameplay_alarm_active(&state));
}

/*
 * Every way of letting go, and the one thing that may not take the button.
 *
 * Each of these is a state that would otherwise put a corpse somewhere it
 * cannot be — up a ladder, off the ground, or trailing behind a man on his
 * elbows — and the terminal is here because it answers the very same held
 * button. A hack is a decision the player made; a body is furniture they
 * happened to stop beside, so the console takes the press.
 */
/*
 * One `USE` press, one action — and the doorway is the claim that was missing.
 *
 * `USE` is a single physical key read two ways: `Input.interact` while it is
 * down, `Input.use_door` on the press. `nearest_body_in_reach` already stood
 * aside for a terminal in range, because the prompt and the grab have to be the
 * same question, and it did not stand aside for a door. So standing on a `D` or
 * a `U` with a corpse at his feet, Chuck was told `HOLD E TO DRAG BODY` and the
 * press took hold of the body *and* walked him through the doorway — a teleport
 * across the sector, a checkpoint banked, and the corpse back on the floor a
 * frame later when the leash broke. The prompt named one action and the key did a
 * different, more expensive one.
 *
 * Three things are checked, and the third is the one that keeps the fix from
 * being a loss: hauling a body *across* a door tile still works, because
 * carrying it means holding `USE` and the door only answers a fresh press.
 */
static void test_the_doorway_and_the_body_do_not_answer_the_same_press(void)
{
    /* Two paired doors on the bottom floor, and a corpse on the near one. */
    static const char data[] =
        "##################\n"
        "#  T   C         #\n"
        "#  D          D  #\n"
        "##################\n"
        "#S    A         E#\n"
        "##################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 31);
    REQUIRE(level_load_data(&state.level, "door-drag", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.map.door_count == 2);
    player_reset(&state.player, &state.level);

    const Door *door = &state.level.map.doors[0];
    state.player.x =
        door->col * (float)TILE_SIZE + ((float)TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = (door->row + 1) * (float)TILE_SIZE - (float)PLAYER_H;
    state.player.on_ground = true;

    state.enemy_count = 1;
    state.enemies[0].dead = true;
    state.enemies[0].x = state.player.x + 2.0f;
    state.enemies[0].y = state.player.y;

    /* The body is within arm's reach by distance alone... */
    float dx = (state.enemies[0].x + ENEMY_W * 0.5f) -
               (state.player.x + PLAYER_W * 0.5f);
    float dy = (state.enemies[0].y + ENEMY_H * 0.5f) -
               (state.player.y + PLAYER_H * 0.5f);
    REQUIRE(sqrtf(dx * dx + dy * dy) < BODY_DRAG_REACH);
    REQUIRE(gameplay_player_door_index(&state) == 0);

    /* ...and the prompt says so no longer, because the door has the press. */
    CHECK(!gameplay_body_within_reach(&state));

    Input press = {.interact = true, .use_door = true};
    float before_x = state.player.x;
    gameplay_update_body_drag(&state, &press);
    CHECK(!state.player.dragging);
    gameplay_use_door(&state, &press);
    /* The door did what the prompt named, and only that. */
    CHECK(fabsf(state.player.x - before_x) > (float)TILE_SIZE);

    /* One tile off the doorway the grab is back, exactly as it is beside a
     * terminal: the refusal is a tile, not a rule about corpses near doors. */
    GameplayState beside = {0};
    rng_seed(&beside.rng, 31);
    REQUIRE(level_load_data(&beside.level, "door-drag", data, strlen(data),
                            &beside.rng));
    player_reset(&beside.player, &beside.level);
    const Door *d = &beside.level.map.doors[0];
    beside.player.x = (d->col + 1) * (float)TILE_SIZE +
                      ((float)TILE_SIZE - PLAYER_W) * 0.5f;
    beside.player.y = (d->row + 1) * (float)TILE_SIZE - (float)PLAYER_H;
    beside.player.on_ground = true;
    beside.enemy_count = 1;
    beside.enemies[0].dead = true;
    beside.enemies[0].x = beside.player.x + 2.0f;
    beside.enemies[0].y = beside.player.y;
    REQUIRE(gameplay_player_door_index(&beside) < 0);
    CHECK(gameplay_body_within_reach(&beside));
    Input hold = {.interact = true};
    gameplay_update_body_drag(&beside, &hold);
    CHECK(beside.player.dragging);

    /* And a body already in hand crosses the door tile without being dropped:
     * the button is held, so nothing offers the door an edge to answer. */
    beside.player.x =
        d->col * (float)TILE_SIZE + ((float)TILE_SIZE - PLAYER_W) * 0.5f;
    REQUIRE(gameplay_player_door_index(&beside) == 0);
    gameplay_update_body_drag(&beside, &hold);
    CHECK(beside.player.dragging);
}

static void test_a_dragged_body_is_dropped_by_everything_that_should_drop_it(void)
{
    Input haul = {.interact = true};

    /* Letting the button go. */
    GameplayState state = {0};
    load_body_drag_floor(&state);
    gameplay_update_body_drag(&state, &haul);
    CHECK(state.player.dragging);
    Input empty = {0};
    gameplay_update_body_drag(&state, &empty);
    CHECK(!state.player.dragging);

    /* A ladder. */
    state = (GameplayState){0};
    load_body_drag_floor(&state);
    gameplay_update_body_drag(&state, &haul);
    CHECK(state.player.dragging);
    state.player.on_ladder = true;
    gameplay_update_body_drag(&state, &haul);
    CHECK(!state.player.dragging);

    /* Going to ground: a crawl is the other quiet way across a floor and it
     * needs both elbows. */
    state = (GameplayState){0};
    load_body_drag_floor(&state);
    gameplay_update_body_drag(&state, &haul);
    CHECK(state.player.dragging);
    state.player.crawling = true;
    gameplay_update_body_drag(&state, &haul);
    CHECK(!state.player.dragging);

    /* Leaving the ground at all. */
    state = (GameplayState){0};
    load_body_drag_floor(&state);
    gameplay_update_body_drag(&state, &haul);
    CHECK(state.player.dragging);
    state.player.on_ground = false;
    gameplay_update_body_drag(&state, &haul);
    CHECK(!state.player.dragging);

    /* The console's press, not the body's. */
    state = (GameplayState){0};
    load_body_drag_floor(&state);
    state.terminal_in_range = true;
    gameplay_update_body_drag(&state, &haul);
    CHECK(!state.player.dragging);

    /* And a slot that has stopped being a corpse — which is what happens when
     * the doors send a reinforcement into a full array — is let go of rather
     * than hauled about alive. */
    state = (GameplayState){0};
    load_body_drag_floor(&state);
    gameplay_update_body_drag(&state, &haul);
    CHECK(state.player.dragging);
    state.enemies[1].dead = false;
    gameplay_update_body_drag(&state, &haul);
    CHECK(!state.player.dragging);
}

/*
 * The one thing in the building that cannot be talked to.
 *
 * A guard has a facing, so there is a side of him to be on; he has ears, so a
 * bolt moves him; he can be taken from behind and carried away afterwards. A
 * camera has none of that, and this test is mostly a list of the answers that
 * must *not* work on it — because each of them working would quietly turn the
 * fitting into a fourth guard.
 */
static void test_a_camera_sweeps_and_raises_the_alarm(void)
{
    static const char data[] =
        "##########\n"
        "#   I    #\n"
        "#        #\n"
        "#S      E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 707);
    REQUIRE(level_load_data(&state.level, "camera", data, strlen(data),
                            &state.rng));
    CHECK(state.level.map.camera_count == 1);
    /* Mounted on the slab above, and the tile it was drawn on is air: a camera
     * the player could walk into would be a wall with a lens on it. */
    CHECK(state.level.map.cameras[0].col == 4);
    CHECK(state.level.map.cameras[0].row == 1);
    CHECK(!level_is_solid(&state.level, 4, 1));

    /* Directly under it, well inside the beam whichever way it is pointing. */
    player_reset(&state.player, &state.level);
    state.player.x = 4.0f * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;

    /* Long enough for more than one full pass of the beam, because the sweep is
     * the point: the lens is looking elsewhere for most of its arc, and a limit
     * shorter than `CAMERA_SWEEP_PERIOD` would be a test of where the sweep
     * happened to start rather than of whether it ever arrives. */
    const int one_full_sweep = (int)(CAMERA_SWEEP_PERIOD / SIM_STEP_DT) + 1;
    const int two_sweeps = one_full_sweep * 2;
    for (int step = 0; step < two_sweeps && !gameplay_alarm_active(&state);
         ++step)
        gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(gameplay_alarm_active(&state));
    /* The floor is sent to Chuck, not to the fitting on the ceiling. An alarm
     * that pointed at the camera would be an alarm that helped. */
    CHECK(fabsf(state.alarm_target_x -
                (state.player.x + PLAYER_W * 0.5f)) < 1.0f);

    /* Crawling is the answer to a guard's cone and must not be the answer to
     * this: the lens is above, looking at the floor the crawl is on. */
    state.terminal_alarm_timer = 0.0f;
    state.cameras[0].notice = 0.0f;
    state.player.crawling = true;
    state.player.y += (float)(PLAYER_H - PLAYER_CRAWL_H);
    for (int step = 0; step < two_sweeps && !gameplay_alarm_active(&state);
         ++step)
        gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(gameplay_alarm_active(&state));

    /* And a beam is not instant: crossing it is survivable, standing in it is
     * not, so a single frame under the lens must never be enough. */
    state.terminal_alarm_timer = 0.0f;
    state.cameras[0].notice = 0.0f;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(!gameplay_alarm_active(&state));
    CHECK(state.cameras[0].notice > 0.0f);

    /* Out of shot, and the count it had built up is given back rather than
     * banked — otherwise two safe crossings would add up to one detection. */
    state.player.x = 5000.0f;
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(state.cameras[0].notice == 0.0f);
}

/*
 * The sweep is a clock, and the clock is the whole answer to the fitting: it is
 * pointing somewhere else half the time. A beam that never left one side of its
 * arc would be an unpassable corridor rather than a timed one.
 */
static void test_a_camera_beam_actually_sweeps_both_ways(void)
{
    float lowest = 0.0f;
    float highest = 0.0f;
    for (int step = 0; step <= 240; ++step)
    {
        float angle =
            gameplay_camera_angle((float)step * CAMERA_SWEEP_PERIOD / 240.0f);
        if (angle < lowest)
            lowest = angle;
        if (angle > highest)
            highest = angle;
    }
    /* Both extremes are reached, and neither overshoots the arc it was given —
     * a beam that swung past the arc would look through the wall it is bolted
     * to. */
    CHECK(highest > CAMERA_SWEEP_ARC * 0.98f);
    CHECK(lowest < -CAMERA_SWEEP_ARC * 0.98f);
    CHECK(highest <= CAMERA_SWEEP_ARC + 0.001f);
    CHECK(lowest >= -CAMERA_SWEEP_ARC - 0.001f);
    /* And it comes back to where it started, so the pass is a loop rather than
     * a drift. */
    CHECK(fabsf(gameplay_camera_angle(0.0f) -
                gameplay_camera_angle(CAMERA_SWEEP_PERIOD)) < 0.001f);
}

/*
 * Two cameras on one ceiling are two beams, not one beam drawn twice.
 *
 * The stagger is what makes a pair of them a corridor the player can thread, and
 * it is the sort of intent that reads as satisfied while being quietly wrong: the
 * phases were spread over `MAX_CAMERAS` instead of over the cameras the map has,
 * so on every sector in the campaign — all of which carry exactly two — the pair
 * started 0.65s apart out of a 5.2s sweep and travelled all but in step, an
 * average of 21 degrees between two beams whose cone is 60 wide.
 *
 * What is measured is therefore the separation and not the phase numbers: the
 * beams must spend real time pointing meaningfully different ways. Half a period
 * on a triangle wave is the exact mirror, so the pair is one going left while the
 * other goes right, and the mean gap comes out near 49 degrees.
 */
static void test_two_cameras_on_a_ceiling_sweep_out_of_step(void)
{
    static const char data[] =
        "##################\n"
        "#   I        I   #\n"
        "#                #\n"
        "#S              E#\n"
        "##################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 606);
    REQUIRE(level_load_data(&state.level, "two-cameras", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.map.camera_count == 2);
    player_reset(&state.player, &state.level);

    /* One combat step is what switches them on and lays the phases in. */
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(state.cameras[0].working);
    CHECK(state.cameras[1].working);

    /* Half a period apart, which is what a pair divided by its own count gets. */
    float offset = fabsf(state.cameras[0].sweep - state.cameras[1].sweep);
    CHECK(fabsf(offset - CAMERA_SWEEP_PERIOD * 0.5f) < 0.05f);

    /*
     * And the separation that buys, averaged over a whole sweep. Anything at or
     * under the cone's own half-angle would mean two fittings watching the same
     * ground at the same moment, which is the arrangement this stagger exists to
     * avoid; the broken version sat well under it.
     */
    const int samples = 240;
    float total = 0.0f;
    for (int i = 0; i < samples; ++i)
    {
        float t = (float)i * CAMERA_SWEEP_PERIOD / (float)samples;
        total += fabsf(gameplay_camera_angle(state.cameras[0].sweep + t) -
                       gameplay_camera_angle(state.cameras[1].sweep + t));
    }
    CHECK(total / (float)samples > CAMERA_CONE_HALF_ANGLE);

    /* A ceiling with one on it is untouched by any of this: nothing to stagger
     * against, and a lone camera must still start at the beginning of its arc
     * rather than at a phase divided by nought. */
    GameplayState single = {0};
    static const char one[] =
        "############\n"
        "#     I    #\n"
        "#S        E#\n"
        "############\n";
    rng_seed(&single.rng, 606);
    REQUIRE(level_load_data(&single.level, "one-camera", one, strlen(one),
                            &single.rng));
    REQUIRE(single.level.map.camera_count == 1);
    player_reset(&single.player, &single.level);
    gameplay_ai_update_combat(&single, SIM_STEP_DT);
    CHECK(single.cameras[0].working);
    CHECK(single.cameras[0].sweep >= 0.0f);
    CHECK(single.cameras[0].sweep <= CAMERA_SWEEP_PERIOD);
}

/*
 * What takes one off the ceiling, and what does not.
 *
 * Shooting a camera is meant to be a real decision: it is permanent, it scores,
 * and it is the loudest thing the player can do. The two halves that would ruin
 * it are a bolt being allowed to break one — the bolts are not a weapon and the
 * whole feature rests on that — and a guard's own round taking one out, which
 * would hand the player the answer for free on ground a guard shoots across
 * constantly.
 */
static void test_a_camera_is_taken_out_by_a_shot_and_not_by_a_bolt(void)
{
    static const char data[] =
        "############\n"
        "#     I    #\n"
        "#          #\n"
        "#S        E#\n"
        "############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "camera", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.map.camera_count == 1);
    player_reset(&state.player, &state.level);
    gameplay_ai_update_combat(&state, SIM_STEP_DT); /* switches them on */
    CHECK(state.cameras[0].working);

    /* A bolt thrown through it leaves it working: it is a noise, not a round. */
    float cx = 0.0f;
    float cy = 0.0f;
    float cw = 0.0f;
    float ch = 0.0f;
    gameplay_camera_box(&state.level.map.cameras[0], &cx, &cy, &cw, &ch);
    state.decoys[0] = (Decoy){.x = cx, .y = cy, .vx = 200.0f, .active = true};
    gameplay_combat_update_decoys(&state, SIM_STEP_DT);
    CHECK(state.cameras[0].working);

    /* A round does not. */
    state.bullets[0] = (Bullet){.x = cx - 4.0f, .y = cy + ch * 0.5f,
                                .vx = BULLET_SPEED, .active = true};
    for (int step = 0; step < SIM_STEPS(1.0f) && state.cameras[0].working; ++step)
        gameplay_combat_update_player_bullets(&state, &campaign, SIM_STEP_DT);
    CHECK(!state.cameras[0].working);
    CHECK(campaign.score == CAMERA_SCORE);
    CHECK(!state.bullets[0].active);

    /* Dead is dead for the visit, and a dead lens sees nothing however long
     * Chuck stands under it. */
    state.player.x = (float)state.level.map.cameras[0].col * TILE_SIZE;
    for (int step = 0; step < (int)(CAMERA_SWEEP_PERIOD / SIM_STEP_DT) * 2;
         ++step)
        gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(!gameplay_alarm_active(&state));

    /* A guard's round passes it by, on a fresh sector. */
    state = (GameplayState){0};
    campaign = (CampaignState){0};
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "camera", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    gameplay_camera_box(&state.level.map.cameras[0], &cx, &cy, &cw, &ch);
    state.enemy_bullets[0] = (Bullet){.x = cx - 4.0f, .y = cy + ch * 0.5f,
                                      .vx = ENEMY_BULLET_SPEED,
                                      .active = true};
    for (int step = 0; step < SIM_STEPS(1.0f); ++step)
        gameplay_combat_update_enemy_bullets(&state, &campaign, SIM_STEP_DT);
    CHECK(state.cameras[0].working);
}

/*
 * A lens takes longer to be sure than a pair of eyes does, and the gap is the
 * whole reason a beam is crossable.
 *
 * `CAMERA_NOTICE_TIME` over `ENEMY_NOTICE_TIME` is what makes the sweep a timed
 * corridor rather than a wall: a man who spots Chuck shoots him, and a camera
 * only ever tells everybody else, so it is allowed to think about it. Reverse
 * the two and the fitting becomes the deadlier of the two things in the room,
 * which is not what any of the art or the amber fade says it is.
 *
 * It is checked here rather than beside the constants because a comparison of
 * two floats cannot be a `_Static_assert` without the GNU folding extension
 * this tree refuses — the reason `MIN_FRAME_RATE` is a whole number of steps.
 * The bolt's radius and the drag speed are the two invariants of this kind that
 * *could* be written as integers, and they are asserted in
 * [game_config.h](../src/game_config.h) instead.
 */
static void test_a_camera_takes_longer_to_be_sure_than_a_man_does(void)
{
    CHECK(CAMERA_NOTICE_TIME > ENEMY_NOTICE_TIME);

    /* And the suspicion fade outlives the notice, or a player who got clear
     * would never see that they nearly did not. */
    CHECK(CAMERA_SUSPICION_FADE > 0.0f);
}

/* A blast takes the fittings with it, which is the rule `apply_blast` exists to
 * keep: a grenade that brought the wall down and left the camera bolted to what
 * was left of it looking at the hole would be a blast that picked which of the
 * things beside it were real. */
static void test_a_blast_takes_a_camera_with_it(void)
{
    static const char data[] =
        "##########\n"
        "#   I    #\n"
        "#        #\n"
        "#S      E#\n"
        "##########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 1234);
    REQUIRE(level_load_data(&state.level, "camera", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.map.camera_count == 1);
    player_reset(&state.player, &state.level);
    gameplay_ai_update_combat(&state, SIM_STEP_DT);
    CHECK(state.cameras[0].working);

    float cx = 0.0f;
    float cy = 0.0f;
    float cw = 0.0f;
    float ch = 0.0f;
    gameplay_camera_box(&state.level.map.cameras[0], &cx, &cy, &cw, &ch);
    /* Chuck well clear of his own grenade, which is a rule about the test
     * rather than about the camera. */
    state.player.x = 5000.0f;
    state.player.y = 5000.0f;
    state.grenade_count = 1;
    state.grenades[0] = (Grenade){.x = cx + cw * 0.5f, .y = cy + ch + 8.0f,
                                  .active = true, .timer = 0.001f,
                                  .grounded = true};
    for (int step = 0; step < SIM_STEPS(1.0f) && state.cameras[0].working; ++step)
        gameplay_combat_update_explosives(&state, &campaign, SIM_STEP_DT);
    CHECK(!state.cameras[0].working);
}

static void test_pursuing_guard_hops_small_gap(void)
{
    static const char data[] =
        "##########\n"
        "#S       #\n"
        "#M      E#\n"
        "##  ######\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 55);
    CHECK(level_load_data(&state.level, "gap", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->on_ground = true;
    guard->dir = 1;
    guard->provoked = true;
    guard->obstacle_avoid_timer = 0.0f;
    guard->pursuit_target_x = 6.0f * TILE_SIZE + TILE_SIZE * 0.5f;
    guard->pursuit_target_y = guard->y + ENEMY_H * 0.5f;
    guard->has_pursuit_target = true;
    state.player.x = 5000.0f; /* keep Chuck out of sight for this test */
    state.player.y = 5000.0f;

    gameplay_ai_update_movement(&state, SIM_STEP_DT);
    CHECK(!guard->on_ground);
    CHECK(guard->vy < 0.0f); /* leapt the gap instead of stalling at the edge */

    bool reached_far_side = false;
    for (int frame = 0; frame < SIM_STEPS(1.5f); ++frame)
    {
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (guard->on_ground && guard->x >= 4.0f * TILE_SIZE)
        {
            reached_far_side = true;
            break;
        }
    }
    CHECK(reached_far_side);            /* completing the jump matters, not just starting it */
    CHECK(guard->x < 4.5f * TILE_SIZE); /* land just beyond the far edge */
}

static void test_pursuing_guard_searches_away_from_blocking_wall(void)
{
    static const char data[] =
        "############\n"
        "#S M #    E#\n"
        "############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 54);
    CHECK(level_load_data(&state.level, "guard blocked pursuit", data,
                          strlen(data), &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->on_ground = true;
    guard->dir = 1;
    guard->provoked = true;
    guard->has_pursuit_target = true;
    guard->pursuit_target_x = 8.5f * TILE_SIZE;
    guard->pursuit_target_y = guard->y + ENEMY_H * 0.5f;
    state.player.x = 5000.0f; /* keep the remembered target behind the wall */
    state.player.y = 5000.0f;

    float wall_turn_x = 5.0f * TILE_SIZE - ENEMY_W - 1.0f;
    for (int frame = 0; frame < SIM_STEPS(4.0f) &&
                        guard->x < wall_turn_x - 0.01f;
         ++frame)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    CHECK(fabsf(guard->x - wall_turn_x) < 1.0f);

    for (int frame = 0; frame < SIM_STEPS(0.5f); ++frame)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);

    CHECK(guard->x < wall_turn_x - 20.0f);
    CHECK(guard->obstacle_avoid_timer > 0.0f);
}

static void test_pursuing_guards_route_up_from_wall_below_player(void)
{
    static const char data[] =
        "############\n"
        "#S H      E#\n"
        "###H########\n"
        "#  H    MMM#\n"
        "############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 53);
    CHECK(level_load_data(&state.level, "guards below player", data,
                          strlen(data), &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 3);

    float target_x = state.player.x + PLAYER_W * 0.5f;
    float target_y = state.player.y + PLAYER_H * 0.5f;
    for (int i = 0; i < state.enemy_count; ++i)
    {
        Enemy *guard = &state.enemies[i];
        guard->on_ground = true;
        guard->dir = 1; /* first search the dead end away from the ladder */
        guard->provoked = true;
        guard->has_pursuit_target = true;
        guard->pursuit_target_x = target_x;
        guard->pursuit_target_y = target_y;
    }

    for (int frame = 0; frame < SIM_STEPS(15.0f); ++frame)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);

    for (int i = 0; i < state.enemy_count; ++i)
        CHECK(state.enemies[i].y < 2.0f * TILE_SIZE);
}

static void test_pursuing_guard_refuses_high_drop(void)
{
    static const char data[] =
        "##########\n"
        "#S       #\n"
        "# M      #\n"
        "###      #\n"
        "#        #\n"
        "#       E#\n"
        "##########\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 58);
    CHECK(level_load_data(&level, "guard high drop", data,
                          strlen(data), &rng));
    CHECK(level.map.enemy_count == 1);

    Enemy guard;
    enemy_init(&guard, level.map.enemy_spawns[0].x,
               level.map.enemy_spawns[0].y, ENEMY_KIND_GUARD, &rng);
    guard.on_ground = true;
    guard.dir = 1;
    float ledge_y = guard.y;
    bool turned_back = false;

    for (int frame = 0; frame < SIM_STEPS(1.5f); ++frame)
    {
        enemy_update(&guard, &level, SIM_STEP_DT, true, false,
                     7.5f * TILE_SIZE, 5.5f * TILE_SIZE,
                     false, 1.0f, &rng);
        if (guard.dir < 0)
            turned_back = true;
        CHECK(fabsf(guard.y - ledge_y) < 0.01f);
        CHECK(guard.on_ground);
    }

    CHECK(turned_back);
}

static void test_guard_rides_elevator_and_leaves_at_target_floor(void)
{
    static const char data[] =
        "#######\n"
        "#     #\n"
        "###V###\n"
        "#  V  #\n"
        "###V###\n"
        "#S V E#\n"
        "#######\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 57);
    CHECK(level_load_data(&level, "enemy elevator route", data,
                          strlen(data), &rng));
    CHECK(level.runtime.elevator_count == 1);

    Elevator *elevator = &level.runtime.elevators[0];
    float elevator_x = elevator->col * (float)TILE_SIZE;
    Enemy guard;
    enemy_init(&guard,
               elevator_x - ENEMY_W,
               6.0f * TILE_SIZE - ENEMY_H, ENEMY_KIND_GUARD, &rng);
    guard.dir = 1;
    guard.on_ground = true;

    /* At a shaft leading toward the target floor, the guard waits for and
     * boards the aligned platform instead of jumping across the opening. */
    elevator->y = elevator->bot_limit - TILE_SIZE;
    float waiting_x = guard.x;
    enemy_update(&guard, &level, SIM_STEP_DT, true, false,
                 5.5f * TILE_SIZE,
                 elevator->top_limit - ENEMY_H * 0.5f,
                 false, 1.0f, &rng);
    CHECK(guard.on_elevator == -1);
    CHECK(guard.on_ground);
    CHECK(fabsf(guard.x - waiting_x) < 0.01f);

    elevator->y = elevator->bot_limit;
    enemy_update(&guard, &level, SIM_STEP_DT, true, false,
                 5.5f * TILE_SIZE,
                 elevator->top_limit - ENEMY_H * 0.5f,
                 false, 1.0f, &rng);
    CHECK(guard.on_elevator == 0);
    CHECK(guard.on_ground);
    CHECK(fabsf(guard.y - (elevator->y - ENEMY_H)) < 0.01f);
    float riding_x = guard.x;

    for (int frame = 0; frame < SIM_STEPS(0.5f); ++frame)
    {
        level_update_elevators(&level, SIM_STEP_DT);
        enemy_update(&guard, &level, SIM_STEP_DT, true, false,
                     5.5f * TILE_SIZE,
                     elevator->top_limit - ENEMY_H * 0.5f,
                     false, 1.0f, &rng);
        CHECK(guard.on_elevator == 0);
        CHECK(guard.on_ground);
        CHECK(fabsf(guard.y - (elevator->y - ENEMY_H)) < 0.01f);
    }
    CHECK(elevator->y < elevator->bot_limit - 30.0f);
    CHECK(fabsf(guard.x - riding_x) < 0.01f);

    /* Once the platform lines up with the requested storey, the guard may
     * walk off instead of waiting through another lift cycle. */
    elevator->y = elevator->top_limit;
    elevator->vy = ELEVATOR_SPEED;
    guard.y = elevator->y - ENEMY_H;
    for (int frame = 0; frame < SIM_STEPS(0.5f) && guard.on_elevator >= 0; ++frame)
    {
        enemy_update(&guard, &level, SIM_STEP_DT, true, false,
                     5.5f * TILE_SIZE,
                     elevator->top_limit - ENEMY_H * 0.5f,
                     false, 1.0f, &rng);
    }
    CHECK(guard.on_elevator == -1);
    CHECK(guard.on_ground);
    CHECK(guard.x + ENEMY_W * 0.5f >= elevator_x + TILE_SIZE);
}

static void test_pursuing_guard_walks_onto_falling_platform(void)
{
    static const char data[] =
        "##########\n"
        "#S       #\n"
        "#M      E#\n"
        "##F#######\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 56);
    CHECK(level_load_data(&level, "falling platform route", data,
                          strlen(data), &rng));
    CHECK(level.map.enemy_count == 1);
    CHECK(level.runtime.fall_platform_count == 1);

    Enemy guard;
    enemy_init(&guard, level.map.enemy_spawns[0].x,
               level.map.enemy_spawns[0].y, ENEMY_KIND_GUARD, &rng);
    guard.on_ground = true;
    guard.dir = 1;
    float previous_x = guard.x;

    enemy_update(&guard, &level, SIM_STEP_DT, true, false,
                 6.5f * TILE_SIZE,
                 guard.y + ENEMY_H * 0.5f, false, 1.0f, &rng);

    CHECK(guard.on_ground);
    CHECK(fabsf(guard.vy) < 0.01f);
    CHECK(guard.x > previous_x); /* walked instead of jumping over it */
}

/*
 * A moving platform carries its rider into air and never into masonry.
 *
 * The platform's own limits are the ends of its row's clear run, so the
 * platform stops a tile short of the wall and the *rider* is the one that
 * meets it: a tile is 32, the box is 26, and the ride only asks that Chuck's
 * centre is over the platform, so standing on its edge hangs him 13px past it.
 * Carried in regardless he stayed in, because `level_move` resolves the
 * horizontal axis only when `vx` is non-zero and a rider holding no direction
 * has none — a third of the figure inside the wall until he touched a key.
 */
static void test_a_moving_platform_never_carries_the_rider_into_a_wall(void)
{
    static const char data[] =
        "##########\n"
        "#S      E#\n"
        "#   P    #\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 91);
    CHECK(level_load_data(&state.level, "platform into a wall", data,
                          strlen(data), &state.rng));
    REQUIRE(state.level.runtime.moving_platform_count == 1);

    MovingPlatform *platform = &state.level.runtime.moving_platforms[0];
    float ride_y = platform->row * (float)TILE_SIZE - (float)PLAYER_H;
    float wall_left = (float)(state.level.map.width - 1) * TILE_SIZE;

    /* Park it hard against the right-hand end of its run and hold it there, so
     * every frame below is a frame the rider could be pushed in on, and stand
     * Chuck one pixel clear of the masonry with his centre still over the
     * platform — the only place both are true at once. */
    platform->x = platform->right_limit;
    platform->vx = MOVING_PLATFORM_SPEED;
    player_reset(&state.player, &state.level);
    state.player.x = wall_left - PLAYER_W - 1.0f;
    state.player.y = ride_y;
    state.player.vy = 0.0f;
    REQUIRE(gameplay_box_tiles_clear(&state, state.player.x, state.player.y,
                                     PLAYER_W, (float)PLAYER_H, STANCE_UPRIGHT));

    for (int frame = 0; frame < SIM_STEPS(4.0f); ++frame)
    {
        platform->x = platform->right_limit;
        gameplay_ride_platforms(&state, SIM_STEP_DT);
        REQUIRE(state.player_on_moving_platform == 0);
        /* The whole claim, every frame: still aboard, still in air. */
        CHECK(gameplay_box_tiles_clear(&state, state.player.x, state.player.y,
                                       PLAYER_W, (float)PLAYER_H, STANCE_UPRIGHT));
    }

    /* And the guard must not have turned the ride off: out in the middle of the
     * run, where there is air to be carried into, the rider still travels. */
    platform->x = platform->left_limit + TILE_SIZE;
    platform->vx = MOVING_PLATFORM_SPEED;
    state.player.x = platform->x + (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = ride_y;
    state.player.vy = 0.0f;
    float before = state.player.x;
    gameplay_ride_platforms(&state, SIM_STEP_DT);
    CHECK(state.player_on_moving_platform == 0);
    CHECK(state.player.x > before);
}

/*
 * The platform moves itself, and the rider goes the whole way with it.
 *
 * `level_update_moving_platforms` and `level_update_falling_platforms` had no
 * test at all, and the reason is worth writing down because it is structural
 * rather than an oversight: both live in `level.c`, on the *core* side of the
 * SDL line, but their only caller is `update_playing` in game.c, on the shell
 * side. So a suite that links every gameplay module and drives it directly
 * never called them, and the coverage of the half of the tree this project
 * calls testable was the half nobody had measured — the renderers had been.
 *
 * The test above pins the one thing that used to go wrong, a rider shoved into
 * masonry, and pins it by parking the platform on its limit by hand every
 * frame. That is the right way to test a wall and it never asks the platform to
 * travel. `P` is on sectors 5, 14 and 17, and not one of them had ever
 * moved under a test.
 */
static void test_a_moving_platform_runs_its_span_and_takes_its_rider(void)
{
    static const char data[] =
        "##############\n"
        "#            #\n"
        "#    P       #\n"
        "#S          E#\n"
        "##############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 404);
    REQUIRE(level_load_data(&state.level, "moving span", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.runtime.moving_platform_count == 1);
    MovingPlatform *platform = &state.level.runtime.moving_platforms[0];

    /* The span is the clear run of its own row, so the platform itself never
     * reaches masonry — which is what makes the rider the only thing that can. */
    CHECK(platform->left_limit == 1.0f * TILE_SIZE);
    CHECK(platform->right_limit == 12.0f * TILE_SIZE);
    CHECK(platform->vx == MOVING_PLATFORM_SPEED);

    player_reset(&state.player, &state.level);
    state.player.x = platform->x + (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = platform->row * (float)TILE_SIZE - (float)PLAYER_H;
    state.player.vy = 0.0f;
    Input idle = {0};

    float span = platform->right_limit - platform->left_limit;
    /* Long enough for a full circuit and then some: the span is eleven tiles
     * and the platform crosses MOVING_PLATFORM_SPEED pixels of it a second, so
     * three crossings is out, back, and out again. */
    int steps = SIM_STEPS(3.0f * span / MOVING_PLATFORM_SPEED);
    int reversals = 0;
    int aboard = 0;
    int in_masonry = 0;
    float previous_vx = platform->vx;
    float rider_min = state.player.x;
    float rider_max = state.player.x;
    for (int step = 0; step < steps; ++step)
    {
        /* The order `update_playing` runs them in. */
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
        level_update_moving_platforms(&state.level, SIM_STEP_DT);
        gameplay_ride_platforms(&state, SIM_STEP_DT);

        CHECK(platform->x >= platform->left_limit - 0.01f);
        CHECK(platform->x <= platform->right_limit + 0.01f);
        if (platform->vx * previous_vx < 0.0f)
            reversals++;
        previous_vx = platform->vx;

        aboard += state.player_on_moving_platform == 0;
        in_masonry += !gameplay_box_tiles_clear(&state, state.player.x,
                                                state.player.y, PLAYER_W,
                                                (float)PLAYER_H, STANCE_UPRIGHT);
        if (state.player.x < rider_min)
            rider_min = state.player.x;
        if (state.player.x > rider_max)
            rider_max = state.player.x;
    }

    /* It patrols rather than parking on an end. */
    CHECK(reversals >= 2);
    /* And it never once dropped him or pushed him into the wall it stops short
     * of: the two halves of the ride, over a whole circuit rather than a frame. */
    CHECK(aboard == steps);
    CHECK(in_masonry == 0);
    /* Carried, not merely stood on. Saying it as "he stayed near the platform"
     * would be restating the ride condition — `gameplay_ride_platforms` only
     * claims a rider whose centre is over the tile — so what is measured is the
     * ground he covered: near enough the whole span, which he can only have got
     * by being taken along it. */
    CHECK(rider_max - rider_min > span - (float)TILE_SIZE);
}

/*
 * A cracked panel waits, then goes, then is not there any more.
 *
 * The other half of the pair above, and the same reason it had no test. What it
 * pins is the shape of the mechanic rather than its numbers: standing on one
 * arms it, `FALL_PLATFORM_TRIGGER_DELAY` of that is the beat the player has to
 * read it in and nothing moves during it, and once it is gone it is gone and
 * the floor under it is what catches him. `F` is on sectors 2, 4, 9, 12 and
 * 17, and the same was true of every one of those.
 */
static void test_a_falling_platform_waits_its_beat_then_drops_its_rider(void)
{
    static const char data[] =
        "##########\n"
        "#        #\n"
        "#   F    #\n"
        "#        #\n"
        "#S      E#\n"
        "##########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 909);
    REQUIRE(level_load_data(&state.level, "cracked panel", data, strlen(data),
                            &state.rng));
    REQUIRE(state.level.runtime.fall_platform_count == 1);
    FallPlatform *panel = &state.level.runtime.fall_platforms[0];
    float rested_at = panel->y;

    player_reset(&state.player, &state.level);
    state.player.x = panel->col * (float)TILE_SIZE +
                     ((float)TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = panel->y - (float)PLAYER_H - 4.0f;
    state.player.vy = 0.0f;
    state.player.on_ground = false;
    Input idle = {0};

    /* Land on it. Nothing is armed until something stands on it. */
    CHECK(!panel->triggered);
    for (int step = 0; step < SIM_STEPS(1.0f) && !panel->triggered; ++step)
    {
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
        level_update_falling_platforms(&state.level, SIM_STEP_DT);
    }
    REQUIRE(panel->triggered);
    CHECK(state.player.on_ground);

    /* The beat: armed, but still exactly where it was drawn. */
    for (int step = 0; step < SIM_STEPS(FALL_PLATFORM_TRIGGER_DELAY * 0.5f);
         ++step)
    {
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
        level_update_falling_platforms(&state.level, SIM_STEP_DT);
        CHECK(panel->y == rested_at);
        CHECK(!panel->removed);
    }
    CHECK(state.player.on_ground);

    /* Then it goes, and it accelerates away rather than being switched off:
     * `FALL_PLATFORM_ACCEL` is what makes the panel read as giving way under a
     * weight instead of vanishing, so the speed has to keep growing. */
    float previous_vy = panel->vy;
    float lowest = panel->y;
    for (int step = 0; step < SIM_STEPS(4.0f) && !panel->removed; ++step)
    {
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
        level_update_falling_platforms(&state.level, SIM_STEP_DT);
        CHECK(panel->vy >= previous_vy);
        CHECK(panel->y >= lowest);
        previous_vy = panel->vy;
        lowest = panel->y;
    }
    REQUIRE(panel->removed);
    CHECK(panel->vy > 0.0f);
    CHECK(panel->y >= (panel->row + 1) * (float)TILE_SIZE);

    /* And the player is on the floor it was standing over rather than on air. */
    for (int step = 0; step < SIM_STEPS(2.0f); ++step)
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
    CHECK(state.player.on_ground);
    CHECK(state.player.y > rested_at);
}

/*
 * A cracked panel is Chuck's weight and nobody else's.
 *
 * `level_move` takes a `triggers_falling` flag and every actor in the building
 * passes false — the guards, the janitor, the receptionist, a fleeing civilian,
 * every dropped body — except the dog, which passed true and was the only thing
 * besides Chuck that could arm an `F`. Nothing said so: the field on
 * `FallPlatform` was commented "player/enemy stepped on it", which was true of
 * neither, and the test above pins the mechanic from the player's side only.
 *
 * It cost a mechanic and a patrol, on a shipped map. Sector 12's panel sits in a
 * handler's dog's roaming range, so it went at about a second into every run:
 * the one-shot route down from the duct corridor was spent before Chuck had left
 * the bottom corner, and the hole it left boxed the guard beside it into the
 * single tile between the trunking and the drop, where he reversed every ninth
 * step for the rest of the sector — see
 * `test_a_guard_in_a_one_tile_dead_end_stands_still`.
 *
 * Both halves are here on purpose. A test that only watched the dog would pass
 * just as well over a panel nothing can arm at all.
 */
static void test_only_chucks_weight_arms_a_cracked_panel(void)
{
    static const char data[] =
        "##########\n"
        "#S  W   E#\n"
        "###F######\n"
        "#        #\n"
        "##########\n";
    static GameplayState state;
    memset(&state, 0, sizeof(state));
    rng_seed(&state.rng, 4141);
    Rng load = state.rng;
    REQUIRE(level_load_data(&state.level, "dog over a cracked panel", data,
                            strlen(data), &load));
    REQUIRE(state.level.runtime.fall_platform_count == 1);
    gameplay_ai_spawn_level_entities(&state);
    REQUIRE(state.dog_count == 1);
    player_reset(&state.player, &state.level);
    state.player.hp = gameplay_player_max_hp(&state);

    FallPlatform *panel = &state.level.runtime.fall_platforms[0];
    Dog *dog = &state.dogs[0];

    /* Walked over rather than dropped on, because walking over is what happened:
     * the panel is level with the floor it is let into, so an animal trotting
     * across it lands on it once per step, which is the path that armed it. Off
     * its handler and sent to an anchor on the far side, so the crossing is a
     * fact of the staging rather than a roll of the roaming dice. */
    dog->owner = -1;
    dog->x = 5.0f * TILE_SIZE + ((float)TILE_SIZE - DOG_W) * 0.5f;
    dog->y = panel->y - (float)DOG_H;
    dog->vx = 0.0f;
    dog->vy = 0.0f;
    dog->on_ground = true;
    dog->guard_x = 1.0f * TILE_SIZE;
    dog->guard_y = dog->y;
    dog->state = DOG_RETURN;

    float panel_left = panel->col * (float)TILE_SIZE;
    bool dog_crossed_it = false;
    for (int step = 0; step < SIM_STEPS(2.0f); ++step)
    {
        state.events.count = 0;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        level_update_falling_platforms(&state.level, SIM_STEP_DT);
        if (dog->on_ground && fabsf(dog->y + DOG_H - panel->y) < 1.0f &&
            dog->x + DOG_W * 0.5f > panel_left &&
            dog->x + DOG_W * 0.5f < panel_left + TILE_SIZE)
            dog_crossed_it = true;
        CHECK(!panel->triggered);
        CHECK(!panel->removed);
    }
    /* The half that makes the assertions above mean anything: its whole weight
     * really was on that tile. */
    CHECK(dog_crossed_it);

    /* And Chuck arms it, from the same tile, so the panel is not simply inert. */
    state.player.x = panel->col * (float)TILE_SIZE +
                     ((float)TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = panel->y - (float)PLAYER_H - 4.0f;
    state.player.vx = 0.0f;
    state.player.vy = 0.0f;
    state.player.on_ground = false;
    Input idle = {0};
    for (int step = 0; step < SIM_STEPS(1.0f) && !panel->triggered; ++step)
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
    CHECK(panel->triggered);
}

/*
 * No sector arms its own panels while the player is still reading the floor.
 *
 * The campaign-side half of the rule above, and the check whose absence let the
 * dog keep its flag: every other test of an `F` hands one a body on purpose, so
 * a panel springing itself on a shipped map was nobody's assertion. Sector 12's
 * did, every run, about a second in.
 *
 * `F` is on sectors 2, 4, 9, 12 and 17 — five chances for an ambient body to
 * spend a one-shot route before Chuck reaches it.
 */
static void test_no_sector_springs_its_own_panels(void)
{
    static GameplayState state;
    static CampaignState campaign;
    int panels = 0;

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        for (int seed = 0; seed < 4; ++seed)
        {
            REQUIRE(stage_sector_at_its_spawn(&state, &campaign, i,
                                              5100u * (uint64_t)(seed + 1) + i));
            panels += state.level.runtime.fall_platform_count;
            for (int step = 0; step < SIM_STEPS(4.0f); ++step)
            {
                step_the_floor_around_a_still_player(&state, &campaign);
                level_update_falling_platforms(&state.level, SIM_STEP_DT);
            }
            for (int p = 0; p < state.level.runtime.fall_platform_count; ++p)
            {
                CHECK(!state.level.runtime.fall_platforms[p].triggered);
                CHECK(!state.level.runtime.fall_platforms[p].removed);
            }
        }
    }
    /* Otherwise the loop above is four seconds of asserting nothing. */
    CHECK(panels > 0);
}

/*
 * A guard with nowhere to go stands there; a guard with one way out takes it.
 *
 * Two rules in `enemy_update_walking` reverse a patrol — masonry at his
 * shoulder, and no floor for his next step — and they probe from the body edge
 * at different offsets, 1px for the wall and 3px for the floor. That leaves a
 * 2px band in which neither fires, which is invisible in a corridor and is the
 * whole of the behaviour in a pocket one tile wide: each rule turns him into the
 * band where the other one fires, so he reverses every ninth simulation step —
 * about a dozen times a second, on the spot. His dog goes with him, because
 * `dog_anchor_x` is derived from the handler's facing, so the animal's target
 * teleports from one side of him to the other at the same rate.
 *
 * That is what a duct and a fallen panel two tiles apart did on sector 12. The
 * fix is the one `update_dog` already had — `dog_can_advance`, and its note
 * about a boxed-in dog standing still rather than spinning in place — asked by
 * tile so that the two rules cannot disagree about the answer.
 *
 * The map holds all three cases at once, because the risk in a fix like this is
 * that it swallows the reversals it was meant to make stable: the pocket at
 * col 4, a ledge with open floor behind it at col 7, and an ordinary corridor
 * along the bottom.
 */
static void test_a_guard_in_a_one_tile_dead_end_stands_still(void)
{
    static const char data[] =
        "############\n"
        "#          #\n"
        "#  #       #\n"
        "#####  #####\n"
        "#          #\n"
        "#          #\n"
        "#S        E#\n"
        "############\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 77);
    REQUIRE(level_load_data(&level, "a one tile dead end", data, strlen(data),
                            &rng));

    /* The pocket: trunking-thick masonry at col 3 on his own row, and a drop at
     * col 5 too deep for `ENEMY_STEP_DOWN_MAX_TILES`. */
    Enemy trapped;
    enemy_init(&trapped, 4.0f * TILE_SIZE + ((float)TILE_SIZE - ENEMY_W) * 0.5f,
               2.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    trapped.on_ground = true;
    trapped.dir = -1;
    float stood_at = trapped.x;
    int facing = trapped.dir;
    int turns = 0;
    for (int step = 0; step < SIM_STEPS(6.0f); ++step)
    {
        enemy_update(&trapped, &level, SIM_STEP_DT, false, false, 0.0f, 0.0f,
                     false, 1.0f, &rng);
        if (trapped.dir != facing)
            turns++;
        facing = trapped.dir;
    }
    CHECK(turns == 0);
    CHECK(fabsf(trapped.x - stood_at) < 1.0f);
    CHECK(fabsf(trapped.vx) < 0.01f);
    CHECK(trapped.on_ground);

    /* The ledge at col 6, approached from a floor that goes on behind him: he
     * still turns rather than walking into the hole, and he still walks. */
    Enemy edge;
    enemy_init(&edge, 8.0f * TILE_SIZE + ((float)TILE_SIZE - ENEMY_W) * 0.5f,
               2.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    edge.on_ground = true;
    edge.dir = -1;
    float west = edge.x;
    float east = edge.x;
    int edge_turns = 0;
    facing = edge.dir;
    for (int step = 0; step < SIM_STEPS(8.0f); ++step)
    {
        enemy_update(&edge, &level, SIM_STEP_DT, false, false, 0.0f, 0.0f,
                     false, 1.0f, &rng);
        if (edge.dir != facing)
            edge_turns++;
        facing = edge.dir;
        west = fminf(west, edge.x);
        east = fmaxf(east, edge.x);
    }
    CHECK(edge_turns >= 2);
    CHECK(east - west > TILE_SIZE);
    /* Never over the hole: the floor ends at col 7, so his left edge stops
     * inside it. */
    CHECK(west > 7.0f * TILE_SIZE - 4.0f);
    CHECK(edge.on_ground);

    /* And an ordinary corridor still reverses at both ends of its run. */
    Enemy corridor;
    enemy_init(&corridor,
               4.0f * TILE_SIZE + ((float)TILE_SIZE - ENEMY_W) * 0.5f,
               6.0f * TILE_SIZE, ENEMY_KIND_GUARD, &rng);
    corridor.on_ground = true;
    corridor.dir = 1;
    int corridor_turns = 0;
    facing = corridor.dir;
    bool went_east = false;
    bool went_west = false;
    for (int step = 0; step < SIM_STEPS(12.0f); ++step)
    {
        enemy_update(&corridor, &level, SIM_STEP_DT, false, false, 0.0f, 0.0f,
                     false, 1.0f, &rng);
        if (corridor.dir != facing)
            corridor_turns++;
        facing = corridor.dir;
        if (corridor.x > 8.0f * TILE_SIZE)
            went_east = true;
        if (corridor.x < 2.0f * TILE_SIZE)
            went_west = true;
    }
    CHECK(corridor_turns >= 2);
    CHECK(went_east);
    CHECK(went_west);
}

/* ---- Forgiveness and fairness ----------------------------------------- */

/* A jump pressed a beat after the boots leave the ledge still happens; the
 * same press once the window has closed does not. */
/*
 * The jump reaches the same height on every machine, and that is a rule rather
 * than an accident of the integrator.
 *
 * `player_update` writes the jump impulse into `vy` *after* that step's
 * gravity, so the launch step carries the undecayed `PLAYER_JUMP_SPEED` and the
 * apex comes out at `v0^2/2g + v0*dt/2` — a number with the step length in it.
 * Fed the real frame time, as `SDL_AppIterate` used to do, that was 68.7px at
 * 240Hz, 71.0px at 60Hz and 77.4px at the MIN_FRAME_RATE floor: nearly a third
 * of a tile between a fast machine and a stuttering one, in the one quantity
 * every map's geometry is drawn against. A ceiling placed to cap a jump was
 * therefore clearable on a slow machine and not on a quick one.
 *
 * The loop below is the one in `SDL_AppIterate`, which is the only place this
 * can be checked from without SDL: clamp the frame, bank it, spend it in whole
 * `SIM_STEP_DT` slices. The apex is sampled per step rather than per frame,
 * because sampling it per frame would measure the display and not the physics.
 */
static void test_the_jump_apex_does_not_depend_on_the_frame_rate(void)
{
    /* Tall enough that the ceiling never caps the rise. */
    static const char data[] =
        "########\n"
        "#      #\n"
        "#      #\n"
        "#      #\n"
        "#S    E#\n"
        "########\n";

    /* Every rate a display is plausibly running at, plus the MIN_FRAME_RATE
     * floor a stuttering one is clamped to. */
    static const float refresh[] = {20.0f, 30.0f,  50.0f,  60.0f,  75.0f,
                                    90.0f, 120.0f, 144.0f, 165.0f, 240.0f};

    float first_apex = 0.0f;
    for (unsigned r = 0; r < sizeof(refresh) / sizeof(refresh[0]); ++r)
    {
        GameplayState state = {0};
        rng_seed(&state.rng, 11);
        CHECK(level_load_data(&state.level, "apex", data, strlen(data),
                              &state.rng));
        player_reset(&state.player, &state.level);

        float frame_dt = 1.0f / refresh[r];
        float accumulator = 0.0f;
        Input in = {0};
        float best = 0.0f;
        /* `Player.jumped` names the *step* a jump started on, and a frame is
         * several steps now, so it has to be caught inside the loop: read
         * after the frame it is already false again. */
        bool launched = false;

        /*
         * One rendered frame, exactly as the shell spends it — including the
         * edge clear. `jump` is edge-triggered and `clear_edge_input` wipes it
         * after the state that consumed it, so a press is answered by the
         * *first* substep of a frame and not by all twelve of them. Leaving it
         * set is not a smaller version of the same thing: it re-fires the jump
         * every step the coyote window is still open, which is a different
         * move and would measure the display all over again.
         */
#define APEX_FRAME()                                                          \
    do                                                                        \
    {                                                                         \
        float elapsed = frame_dt;                                             \
        if (elapsed > MAX_FRAME_DT)                                           \
            elapsed = MAX_FRAME_DT;                                           \
        accumulator += elapsed;                                               \
        while (accumulator >= SIM_STEP_DT)                                    \
        {                                                                     \
            accumulator -= SIM_STEP_DT;                                       \
            player_update(&state.player, &state.level, &in, SIM_STEP_DT);     \
            in.jump = false;                                                  \
            if (state.player.jumped)                                          \
                launched = true;                                              \
            if (state.player.y < best)                                        \
                best = state.player.y;                                        \
        }                                                                     \
    } while (0)

        best = state.player.y;
        for (int settle = 0; settle < 200; ++settle)
            APEX_FRAME();
        CHECK(state.player.on_ground);

        float ground = state.player.y;
        best = state.player.y;

        in.jump = true;
        in.jump_held = true;
        launched = false;
        APEX_FRAME();
        CHECK(launched);

        /* Held for the whole rise: this measures the full arc, not a hop cut
         * short by `PLAYER_JUMP_CUT_FACTOR`. */
        for (int air = 0; air < 900 && !state.player.on_ground; ++air)
            APEX_FRAME();
        CHECK(state.player.on_ground);

#undef APEX_FRAME

        float apex = ground - best;
        /* It really did leave the floor, and the ceiling really did not catch
         * it — otherwise every rate would agree on a height set by the map. */
        CHECK(apex > 2.0f * TILE_SIZE);
        CHECK(apex < 3.0f * TILE_SIZE);

        if (r == 0)
            first_apex = apex;
        /* A tenth of a pixel. The steps are identical, so the only thing left
         * to differ is float summation order in the accumulator; anything
         * bigger than this means the frame has got back into the physics. */
        CHECK(fabsf(apex - first_apex) < 0.1f);
    }
}

static void test_coyote_time_allows_a_late_jump(void)
{
    /* The walk row keeps an open row overhead so the coyote jump can rise
     * instead of being clipped flat by the ceiling. */
    static const char data[] =
        "######\n"
        "#    #\n"
        "#S  E#\n"
        "##   #\n"
        "#    #\n"
        "######\n";

    /*
     * One press inside the window and one outside it, said as fractions of the
     * window rather than as a number of steps — and this is the case that shows
     * why the rule exists. It read `late_frames` 2 and 14, which at the 1/60
     * this was written against were 0.03s and 0.23s either side of a
     * `PLAYER_COYOTE_TIME` of 0.10. At the rate the game actually runs, 14
     * steps is 0.058s, which is *inside* the window: the half of this test that
     * checks the window closing had quietly stopped checking it, and it went on
     * passing because both halves now took the same branch.
     */
    static const float waits[] = {PLAYER_COYOTE_TIME * 0.25f,
                                  PLAYER_COYOTE_TIME * 2.5f};
    int allowed = 0;
    int refused = 0;
    for (unsigned w = 0; w < sizeof(waits) / sizeof(waits[0]); ++w)
    {
        int late_steps = SIM_STEPS(waits[w]);
        GameplayState state = {0};
        rng_seed(&state.rng, 11);
        CHECK(level_load_data(&state.level, "ledge", data, strlen(data),
                              &state.rng));
        player_reset(&state.player, &state.level);

        Input run = {.right = true};
        int frame = 0;
        while (frame < SIM_STEPS(3.0f) &&
               (state.player.on_ground || state.player.vy <= 0.0f))
        {
            player_update(&state.player, &state.level, &run, SIM_STEP_DT);
            frame++;
        }
        CHECK(frame < SIM_STEPS(3.0f));

        Input idle = {0};
        for (int wait = 0; wait < late_steps; ++wait)
            player_update(&state.player, &state.level, &idle, SIM_STEP_DT);

        Input jump = {.jump = true, .jump_held = true};
        player_update(&state.player, &state.level, &jump, SIM_STEP_DT);
        if ((float)late_steps * SIM_STEP_DT < PLAYER_COYOTE_TIME)
        {
            allowed++;
            CHECK(state.player.jumped);
            CHECK(state.player.vy < 0.0f);
        }
        else
        {
            refused++;
            CHECK(!state.player.jumped);
            CHECK(state.player.vy > 0.0f);
        }
    }
    /* Both halves were actually taken. Without this the test above passes with
     * both presses on the same side of the window, which is exactly the state
     * it spent a long time in. */
    CHECK(allowed == 1);
    CHECK(refused == 1);
}

/* A jump pressed just before touchdown is kept until the boots arrive. */
static void test_jump_buffer_executes_on_landing(void)
{
    static const char data[] =
        "#####\n"
        "#S E#\n"
        "#   #\n"
        "#   #\n"
        "#####\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 12);
    CHECK(level_load_data(&state.level, "drop", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    float floor_stand_y = 4.0f * TILE_SIZE - (float)PLAYER_H;
    bool pressed = false;
    bool jumped = false;
    Input input = {0};
    for (int frame = 0; frame < SIM_STEPS(5.0f) && !jumped; ++frame)
    {
        input.jump = false;
        if (!pressed && state.player.vy > 0.0f &&
            floor_stand_y - state.player.y < 22.0f)
        {
            input.jump = true;
            input.jump_held = true;
            pressed = true;
        }
        player_update(&state.player, &state.level, &input, SIM_STEP_DT);
        jumped = state.player.jumped;
    }
    CHECK(pressed);
    CHECK(jumped);
    CHECK(state.player.vy < 0.0f);
}

/* Releasing the button mid-rise caps the jump; a stomp bounce is never cut. */
static void test_releasing_jump_cuts_the_rise(void)
{
    static const char data[] =
        "#####\n"
        "#   #\n"
        "#   #\n"
        "#S E#\n"
        "#####\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 13);
    CHECK(level_load_data(&state.level, "flat", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    Input idle = {0};
    for (int frame = 0; frame < SIM_STEPS(0.5f) && !state.player.on_ground; ++frame)
        player_update(&state.player, &state.level, &idle, SIM_STEP_DT);
    CHECK(state.player.on_ground);

    Input jump = {.jump = true, .jump_held = true};
    player_update(&state.player, &state.level, &jump, SIM_STEP_DT);
    CHECK(state.player.jumped);
    float full_rise = state.player.vy;
    CHECK(full_rise < -PLAYER_JUMP_SPEED * 0.9f);

    Input released = {0};
    player_update(&state.player, &state.level, &released, SIM_STEP_DT);
    CHECK(state.player.vy >=
          -PLAYER_JUMP_SPEED * PLAYER_JUMP_CUT_FACTOR - 0.001f);

    /* The bounce off a stomped guard is not player-started: releasing the
     * key must not shorten it back down into the guard. */
    state.player.jump_cut_ok = false;
    state.player.vy = -ENEMY_STOMP_BOUNCE_SPEED;
    player_update(&state.player, &state.level, &released, SIM_STEP_DT);
    CHECK(state.player.vy < -PLAYER_JUMP_SPEED * PLAYER_JUMP_CUT_FACTOR - 40.0f);
}

/* Contact costs a heart and opens a mercy window; only the last heart kills. */
static void test_contact_costs_a_heart_with_mercy_window(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.hp = PLAYER_MAX_HP;
    state.player.facing = 1;
    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 10.0f, .y = 0.0f, .hp = ENEMY_HP};
    state.player.x = 10.0f + ENEMY_W - 4.0f;
    state.player.y = 0.0f;

    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.player.hp == PLAYER_MAX_HP - 1);
    CHECK(!state.player.dying);
    CHECK(state.invuln_timer > 0.0f);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND, SFX_PLAYER_HIT));

    /* Inside the mercy window the same contact costs nothing further. */
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.player.hp == PLAYER_MAX_HP - 1);

    state.invuln_timer = 0.0f;
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.player.hp == PLAYER_MAX_HP - 2);
    CHECK(!state.player.dying);

    state.invuln_timer = 0.0f;
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.player.dying);
    CHECK(state.player.hp == 0);
}

/* Explosions are the heavy hit: two hearts, still survivable from full. */
static void test_explosion_costs_two_hearts(void)
{
    GameplayState state = {0};
    CampaignState campaign = {0};
    state.player.hp = PLAYER_MAX_HP;
    state.player.facing = 1;
    state.mines[0] = (Mine){.x = 0.0f, .y = 10.0f, .active = true};
    state.mine_count = 1;

    gameplay_combat_update_explosives(&state, &campaign, 0.01f);
    CHECK(state.mines[0].triggered);
    gameplay_combat_update_explosives(&state, &campaign,
                                      MINE_TRIGGER_DELAY + 0.01f);
    CHECK(!state.mines[0].active);
    CHECK(!state.player.dying);
    CHECK(state.player.hp == PLAYER_MAX_HP - EXPLOSION_DAMAGE);
    CHECK(state.invuln_timer > 0.0f);
}

/* Real progress banks an interior checkpoint, and a respawn resumes there
 * with nothing already in flight. */
static void test_interior_checkpoint_resumes_progress(void)
{
    static const char data[] =
        "########\n"
        "#S C  E#\n"
        "########\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 14);
    CHECK(level_load_data(&state.level, "checkpoint", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    CHECK(!state.interior_has_checkpoint);

    const Item *card = &state.level.runtime.items[0];
    state.player.x = card->x - PLAYER_W * 0.5f;
    state.player.y = card->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, SIM_STEP_DT);
    CHECK(state.level.runtime.items[0].collected);
    CHECK(state.interior_has_checkpoint);
    float banked_x = state.interior_checkpoint_x;

    state.player.x = state.level.map.start_x;
    state.player.vy = 120.0f;
    state.enemy_bullets[0] = (Bullet){.x = 60.0f, .y = 40.0f,
                                      .vx = 100.0f, .active = true};
    gameplay_restore_checkpoint(&state);
    CHECK(state.player.x == banked_x);
    CHECK(state.player.vy == 0.0f);
    CHECK(!state.enemy_bullets[0].active);
}

/*
 * The air is cleared for a respawn that has no checkpoint to go back to, which
 * is the death the rule used to skip.
 *
 * Both restores used to clear what was in flight only *after* deciding there
 * was somewhere to put the player, so the first death of a sector — before any
 * card, terminal, door or medkit, and on a climb before the first banked step —
 * was the one respawn the promise was not kept for, and it is also the one the
 * player is least equipped to survive. Both directions are pinned here because
 * the interior and the climb keep the same rule in two different functions.
 */
static void test_a_respawn_with_no_checkpoint_still_clears_the_air(void)
{
    static const char data[] =
        "########\n"
        "#S    E#\n"
        "########\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 4242);
    CHECK(level_load_data(&state.level, "no-checkpoint", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    CHECK(!state.interior_has_checkpoint);

    state.enemy_bullets[0] = (Bullet){.x = 60.0f, .y = 40.0f,
                                      .vx = 100.0f, .active = true};
    gameplay_restore_checkpoint(&state);
    CHECK(!state.enemy_bullets[0].active);
    /* Nothing was banked, so the start tile player_reset chose is kept. */
    CHECK(state.player.x == state.level.map.start_x);

    /* The same promise on the wall, where a brick already falling is what a
     * climber put back on the opening course would land under. */
    GameplayState climb = {0};
    rng_seed(&climb.rng, 4243);
    CHECK(level_load_data(&climb.level, EMBEDDED_LEVELS[2].name,
                          EMBEDDED_LEVELS[2].data,
                          EMBEDDED_LEVELS[2].size, &climb.rng));
    player_reset(&climb.player, &climb.level);
    CHECK(climb.level.map.mode == LEVEL_MODE_FACADE);
    CHECK(!climb.facade_has_checkpoint);

    float start_y = climb.player.y;
    climb.thrown_objects[0].active = true;
    climb.birds[0].active = true;
    /*
     * Through the *generic* entry point, which is the whole point of this half.
     *
     * It called `gameplay_climb_restore_checkpoint` directly, and every other
     * test of the climb's restore did too — so `gameplay_restore_checkpoint`'s
     * facade arm, the two lines that decide which of the two rules a death gets,
     * were never executed by anything. `make coverage` had them dark while the
     * function on either side of them was covered.
     *
     * That matters because the shell calls the generic one: `game.c` has a
     * single respawn path and hands it whatever sector is loaded. Testing the
     * arm's destination instead of the arm is this file's own recurring lesson
     * one floor up — a function is not reached because its file is linked, and a
     * *branch* is not reached because the function it calls is covered. Wired to
     * the interior rule, a death on a climb would have put Chuck on the map's
     * start tile with his banked height thrown away, and nothing here would have
     * said a word.
     */
    gameplay_restore_checkpoint(&climb);
    CHECK(!climb.thrown_objects[0].active);
    CHECK(!climb.birds[0].active);
    CHECK(climb.player.y == start_y);

    /* And the arm has to be chosen by the mode rather than by luck: banked
     * height is the thing only the facade rule hands back, so a climb that has
     * earned some must get it from this same call. */
    Input up = {.up = true};
    for (int frame = 0; frame < SIM_STEPS(2.0f); ++frame)
        gameplay_climb_update_player(&climb, &up, SIM_STEP_DT);
    REQUIRE(climb.facade_has_checkpoint);
    float banked_y = climb.facade_checkpoint_y;
    CHECK(banked_y < start_y);

    player_reset(&climb.player, &climb.level);
    CHECK(climb.player.y == start_y);
    gameplay_restore_checkpoint(&climb);
    CHECK(climb.player.y == banked_y);
}

/* A guard downed in direct combat drops a magazine worth picking up — but
 * only once the player can actually use it. */
static void test_guard_downed_in_combat_drops_ammo(void)
{
    static const char data[] =
        "#####\n"
        "#S E#\n"
        "#####\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 15);
    CHECK(level_load_data(&state.level, "drops", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 2.0f * TILE_SIZE,
                               .y = 2.0f * TILE_SIZE - ENEMY_H,
                               .hp = 1, .dir = -1};
    state.bullets[0] = (Bullet){.x = state.enemies[0].x - 6.0f,
                                .y = state.enemies[0].y + 12.0f,
                                .vx = BULLET_SPEED, .active = true};
    gameplay_combat_update_player_bullets(&state, &campaign, SIM_STEP_DT);
    CHECK(state.enemies[0].dead);
    CHECK(state.ammo_drops[0].active);

    /* A full sidearm leaves the magazine lying where it fell. */
    state.player.hp = PLAYER_MAX_HP;
    state.player.bullets = MAX_AMMO;
    state.player.x = state.ammo_drops[0].x - 4.0f;
    state.player.y = 2.0f * TILE_SIZE - PLAYER_H;
    gameplay_update_ammo_drops(&state, SIM_STEP_DT);
    CHECK(state.ammo_drops[0].active);

    state.player.bullets = 0;
    gameplay_update_ammo_drops(&state, SIM_STEP_DT);
    CHECK(!state.ammo_drops[0].active);
    CHECK(state.player.bullets == AMMO_DROP_BULLETS);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_PICKUP_AMMO));
}

/* A body is not a climber. Shot halfway up a shaft it falls past the rungs to
 * the floor of it, rather than being caught by the next rung and left lying
 * across the ladder in mid-air — which is both a body resting on nothing the
 * player can stand on and a body no comrade sent to look at it could reach. */
static void test_body_falls_past_the_rungs(void)
{
    static const char data[] =
        "#####\n"
        "#S H#\n"
        "#  H#\n"
        "#E H#\n"
        "#####\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 21);
    CHECK(level_load_data(&state.level, "shaft", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    state.enemy_count = 1;
    state.enemies[0] = (Enemy){.x = 3.0f * TILE_SIZE,
                               .y = 1.0f * TILE_SIZE + 8.0f,
                               .dir = -1,
                               .dead = true,
                               .climbing = true};
    for (int i = 0; i < SIM_STEPS(2.0f); ++i)
        gameplay_ai_update_movement(&state, SIM_STEP_DT);

    /* The floor of the shaft, not the rung below where he was hit. */
    CHECK(fabsf(state.enemies[0].y - (4.0f * TILE_SIZE - ENEMY_H)) < 0.5f);
    CHECK(state.enemies[0].vy == 0.0f);
}

/* A dog bite is announced: the first contact only starts the crouch, the
 * teeth land a beat later if Chuck is still there, and stepping clear
 * cancels the lunge entirely. */
static void test_dog_bite_is_announced_and_survivable(void)
{
    static const char data[] =
        "#####\n"
        "#S E#\n"
        "#####\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 16);
    CHECK(level_load_data(&state.level, "kennel", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);
    state.player.hp = PLAYER_MAX_HP;
    state.player.y = 2.0f * TILE_SIZE - PLAYER_H;

    state.dog_count = 1;
    state.dogs[0] = (Dog){.x = state.player.x,
                          .y = 2.0f * TILE_SIZE - DOG_H,
                          .hp = DOG_HP, .owner = -1, .dir = 1,
                          .state = DOG_GUARD, .state_timer = 100.0f,
                          .guard_x = state.player.x,
                          .guard_y = 2.0f * TILE_SIZE - DOG_H};

    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.player.hp == PLAYER_MAX_HP);
    CHECK(state.dogs[0].bite_windup > 0.0f);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_DOG_GROWL));

    /* Stepping clear during the crouch cancels the lunge. */
    float held_x = state.player.x;
    state.player.x += 3.0f * TILE_SIZE;
    gameplay_ai_update_movement(&state, SIM_STEP_DT);
    CHECK(state.dogs[0].bite_windup == 0.0f);
    CHECK(!state.dogs[0].bite_ready);

    /* Standing in it, the growl becomes a bite one windup later. */
    state.player.x = held_x;
    state.dogs[0].x = held_x;
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.dogs[0].bite_windup > 0.0f);
    for (int i = 0; i < SIM_STEPS(0.5f) && !state.dogs[0].bite_ready; ++i)
    {
        state.dogs[0].x = state.player.x;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    }
    CHECK(state.dogs[0].bite_ready);
    state.dogs[0].x = state.player.x;
    state.dogs[0].y = state.player.y + PLAYER_H - DOG_H;
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.player.hp == PLAYER_MAX_HP - 1);
    CHECK(!state.player.dying);
    CHECK(state.invuln_timer > 0.0f);
    CHECK(state.dogs[0].bite_cooldown > 0.0f);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_DOG_BITE));
}

/* A failed drive rewinds a stretch, not to zero. */
static void test_chase_failure_rewinds_instead_of_restarting(void)
{
    Chase chase;
    chase_init(&chase, 2222);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);

    Input input = {0};
    chase.pursuit_time = 20.0f;
    chase.target.y = chase.player.y + CHASE_LOSE_GAP + 20.0f;
    chase_step(&chase, &input);
    CHECK(chase.phase == CHASE_PHASE_FAILED);

    chase_run(&chase, &input, CHASE_FAILED_DURATION + 0.1f);
    CHECK(chase.phase == CHASE_PHASE_PURSUIT);
    CHECK(fabsf(chase.pursuit_time - (20.0f - CHASE_FAIL_REWIND)) < 0.5f);
}

/* After enough failed attempts, confirm skips the rest of the drive. */
static void test_chase_skippable_after_repeated_failures(void)
{
    Chase chase;
    chase_init(&chase, 3333);
    chase_skip_departure(&chase);
    chase_clear_traffic(&chase);

    Input confirm = {.confirm = true};
    chase_step(&chase, &confirm);
    CHECK(chase.phase == CHASE_PHASE_PURSUIT);

    chase.attempts = CHASE_SKIP_AFTER_ATTEMPTS;
    chase_step(&chase, &confirm);
    CHECK(chase.phase == CHASE_PHASE_ARRIVAL);
}

/* A fresh sighting is noticed for a beat before the aim telegraph starts;
 * provoked guards are past noticing. */
static void test_fresh_sighting_waits_before_aiming(void)
{
    static const char data[] =
        "############\n"
        "#S     M  E#\n"
        "############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 17);
    CHECK(level_load_data(&state.level, "notice", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->dir = -1;
    guard->on_ground = true;
    guard->shoot_cooldown = 0.0f;
    guard->encounter_decided = true;
    state.player.y = guard->y;
    state.player.x = guard->x - 5.0f * TILE_SIZE;

    int steps = 0;
    while (guard->aim_timer <= 0.0f && steps < SIM_STEPS(1.0f))
    {
        gameplay_ai_update_combat(&state, SIM_STEP_DT);
        steps++;
    }
    CHECK(guard->aim_timer > 0.0f);
    /* The notice is a duration and not a number of steps, which is the
     * whole reason this is counted at `SIM_STEP_DT`: it read `steps == 4`
     * against a hand-picked 0.1s, so it was pinning the rate the test made
     * up rather than `ENEMY_NOTICE_TIME`. One step either side, because the
     * timer is crossed inside a step rather than on its boundary. */
    CHECK(steps >= SIM_STEPS(ENEMY_NOTICE_TIME) - 1);
    CHECK(steps <= SIM_STEPS(ENEMY_NOTICE_TIME) + 1);

    GameplayState provoked = {0};
    rng_seed(&provoked.rng, 17);
    CHECK(level_load_data(&provoked.level, "provoked", data, strlen(data),
                          &provoked.rng));
    gameplay_ai_spawn_level_entities(&provoked);
    Enemy *pguard = &provoked.enemies[0];
    pguard->dir = -1;
    pguard->on_ground = true;
    pguard->shoot_cooldown = 0.0f;
    pguard->encounter_decided = true;
    pguard->provoked = true;
    provoked.player.y = pguard->y;
    provoked.player.x = pguard->x - 5.0f * TILE_SIZE;
    gameplay_ai_update_combat(&provoked, SIM_STEP_DT);
    CHECK(pguard->aim_timer > 0.0f);
}

/* A medkit refills the hearts first and only banks a spare life once they
 * are already full. */
static void test_medkit_heals_before_granting_life(void)
{
    static const char data[] =
        "######\n"
        "#S KE#\n"
        "######\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 18);
    CHECK(level_load_data(&state.level, "aid", data, strlen(data),
                          &state.rng));
    player_reset(&state.player, &state.level);

    const Item *kit = &state.level.runtime.items[0];
    state.player.hp = 1;
    state.player.x = kit->x - PLAYER_W * 0.5f;
    state.player.y = kit->y - PLAYER_H * 0.5f;
    gameplay_collect_items(&state, &campaign, SIM_STEP_DT);
    CHECK(state.player.hp == PLAYER_MAX_HP);
    CHECK(campaign.lives == 0);
    CHECK(state.interior_has_checkpoint);

    state.level.runtime.items[0].collected = false;
    gameplay_collect_items(&state, &campaign, SIM_STEP_DT);
    CHECK(state.player.hp == PLAYER_MAX_HP);
    CHECK(campaign.lives == 1);
}

/* ---- The night's own dressing ---------------------------------------- */

/*
 * The flight case stands on the floor and the clock hangs off the slab, and
 * the loader has to ask each of them the right question. A prop kept with no
 * support would float; one dropped that had support would be an author's work
 * silently thrown away, which is the worse of the two because nothing says so.
 */
static void test_night_props_ask_for_the_right_wall(void)
{
    static const char data[] =
        "########\n"
        "#w  w E#\n"
        "#### ###\n"
        "#S m m #\n"
        "###  ###\n";
    Level level;
    Rng rng;
    rng_seed(&rng, 5150);
    CHECK(level_load_data(&level, "night", data, strlen(data), &rng));

    /* Row 1 col 1 has the slab of row 0 over it and is kept; row 1 col 4 sits
     * under the gap in that row's own... it does not — row 0 is solid all the
     * way across, so both clocks hang. The case at row 3 col 3 has floor and
     * is kept; the one at col 5 is over the gap in row 4 and is dropped. */
    int clocks = 0;
    int cases = 0;
    for (int i = 0; i < level.map.decoration_count; ++i)
    {
        if (level.map.decorations[i].type == DECOR_WALL_CLOCK)
            clocks++;
        if (level.map.decorations[i].type == DECOR_FLIGHT_CASE)
            cases++;
    }
    CHECK(clocks == 2);
    CHECK(cases == 1);

    /* And the mirror image of each: a case with air under it and a clock with
     * air over it are both unsupported, whatever is on the far side of them. */
    static const char swapped[] =
        "########\n"
        "#     E#\n"
        "#  w   #\n"
        "#Sm m  #\n"
        "## ## ##\n"
        "#      #\n"
        "########\n";
    CHECK(level_load_data(&level, "swapped", swapped, strlen(swapped), &rng));
    /* The clock has open air above it and goes; of the two cases only the one
     * standing on a solid tile of the row below survives. */
    CHECK(level.map.decoration_count == 1);
    CHECK(level.map.decorations[0].type == DECOR_FLIGHT_CASE);
    CHECK(level.map.decorations[0].col == 4);
}

/*
 * A guard on his own calls in, and the beat is colour and nothing else: he
 * stops for it exactly the way he stops for a chat, and he is no easier to
 * walk past while it lasts. The second half of that is the part worth pinning
 * — an ambient detail that quietly hands the player a stealth window is a
 * balance change wearing a costume.
 */
static void test_lone_guard_calls_in_without_going_blind(void)
{
    static const char data[] =
        "##############################\n"
        "#S           M              E#\n"
        "##############################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 7731);
    CHECK(level_load_data(&state.level, "radio", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    Enemy *guard = &state.enemies[0];
    guard->on_ground = true;
    /* Well outside the guard's cone, so nothing else can interrupt the beat. */
    state.player.x = 32.0f;
    state.player.y = 32.0f;

    bool called = false;
    for (int step = 0; step < SIM_STEPS(60.0f) && !called; ++step)
    {
        game_events_clear(&state.events);
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        if (events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                              SFX_GUARD_RADIO))
            called = true;
    }
    CHECK(called);
    CHECK(guard->talking);
    CHECK(guard->talk_partner < 0);
    CHECK(enemy_on_radio(guard));
    /* It is a standing beat, like the chat it borrows from. */
    CHECK(fabsf(guard->vx) < 0.001f);

    /* And it ends on its own, back into the patrol. */
    for (int step = 0; step < SIM_STEPS(6.0f) && guard->talking; ++step)
    {
        game_events_clear(&state.events);
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
    }
    CHECK(!guard->talking);
    CHECK(!enemy_on_radio(guard));
}

/*
 * The same beat, now with words in it. What is pinned here is the boundary
 * rather than the joke: the simulation reports that somebody spoke, where, and
 * one opaque number, and knows nothing else. If a gameplay module ever grows
 * an opinion about *which* line was said, every new piece of flavour text
 * becomes a change to a deterministic module and this test is what says so.
 */
static void test_the_net_carries_words(void)
{
    static const char data[] =
        "##############################\n"
        "#S           M              E#\n"
        "##############################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 7731);
    CHECK(level_load_data(&state.level, "radio", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    CHECK(state.enemy_count == 1);
    state.enemies[0].on_ground = true;
    state.player.x = 32.0f;
    state.player.y = 32.0f;

    const GameEvent *spoke = NULL;
    for (int step = 0; step < SIM_STEPS(60.0f) && spoke == NULL; ++step)
    {
        game_events_clear(&state.events);
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        for (int i = 0; i < state.events.count; ++i)
        {
            if (state.events.items[i].type == GAME_EVENT_CHATTER)
                spoke = &state.events.items[i];
        }
    }
    CHECK(spoke != NULL);
    if (spoke == NULL)
        return;

    /* A man on his own is on the handset, and the line is credited to the slot
     * he stands in — never drawn — so the archive's guard answers to the same
     * name every time that sector loads. */
    CHECK(spoke->data.chatter.kind == CHATTER_RADIO);
    CHECK(spoke->data.chatter.speaker == 0);
    /* Spoken where he is standing, because earshot is measured off it. */
    CHECK(fabsf(spoke->data.chatter.x -
                (state.enemies[0].x + ENEMY_W * 0.5f)) < 0.001f);
    /* And the shell can spell it from the roll alone. */
    CHECK(crew_line(spoke->data.chatter.kind, spoke->data.chatter.roll) !=
          NULL);
}

/*
 * The plate along the top of the screen is one line high and starts 14px in,
 * so a callsign, a colon and the longest line in the tables have to fit inside
 * 800px of 8x8 cells. A line that outgrows it does not wrap — it runs off the
 * edge of the frame, which is the one failure nobody would see in a
 * screenshot.
 */
static void test_crew_traffic_fits_the_plate(void)
{
    size_t widest_name = 0;
    for (int i = 0; i < CREW_SIZE; ++i)
    {
        const char *name = crew_callsign(i);
        CHECK(name != NULL && name[0] != '\0');
        if (strlen(name) > widest_name)
            widest_name = strlen(name);
    }
    /* The roster wraps rather than running off the end: a sector may hold more
     * bodies than the crew has men, and the facade hands in a window index
     * instead of an enemy slot. */
    CHECK(strcmp(crew_callsign(CREW_SIZE), crew_callsign(0)) == 0);
    CHECK(crew_callsign(-3) != NULL);

    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
    {
        int count = crew_line_count((ChatterKind)kind);
        CHECK(count > 0);
        for (int i = 0; i < count; ++i)
        {
            const char *line = crew_line((ChatterKind)kind, i);
            CHECK(line != NULL && line[0] != '\0');
            if (line != NULL)
                CHECK(strlen(line) <= CREW_LINE_MAX);
        }
        /* Any roll at all lands on a line: the fold is the table's own job, so
         * no caller can reach past the end of one. */
        CHECK(crew_line((ChatterKind)kind, 0x7fff) != NULL);
        CHECK(crew_line((ChatterKind)kind, -1) != NULL);
    }

    /* "<NAME>: <line>" at 8px a cell, inside the 800px frame with its 14px
     * inset and the plate's own padding. */
    CHECK((widest_name + 2 + CREW_LINE_MAX) * 8 + 14 + 22 <= 800);
}

static bool test_line_names(const char *line, const char *name);

/*
 * The net says things about where the player is, so it has to be told.
 *
 * A good half of this writing asserts something — the vault is empty, the roof
 * goes in two minutes, nothing has come back from MARCO — and the tables are
 * rolled from with no idea of any of it, so a man standing in the lobby at
 * 00:22 was free to report all three before Chuck had climbed a floor or
 * touched anybody. These lines are the version of the plot a player who never
 * opens the manual actually gets, so arriving out of order is the same failure
 * as a report between sectors that spoils its own ending, only shuffled.
 *
 * Two things are pinned. Every kind has something sayable at every point of
 * the campaign, including the very first sector with nobody down — a gate that
 * empties a table falls through to the ungated fallback, which is a silent
 * repair of exactly the bug this is here to prevent. And whatever comes back
 * is a line that actually passes its own gate, for every roll and every
 * speaker, so the walk forward cannot quietly hand over a line it should have
 * stepped past.
 */
static void test_the_net_always_has_something_to_say(void)
{
    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
    {
        int count = crew_line_count((ChatterKind)kind);
        REQUIRE(count > 0);
        for (int sector = 1; sector <= (int)EMBEDDED_LEVEL_COUNT; ++sector)
        {
            /* Nobody down: the hardest case, and the one a pacifist run and
             * every player's first minutes are actually in. */
            CrewSituation situation = {sector, 0};
            int allowed = 0;
            for (int i = 0; i < count; ++i)
                if (crew_line_allowed((ChatterKind)kind, i, &situation))
                    allowed++;
            CHECK(allowed > 0);

            for (int roll = 0; roll < count + 3; ++roll)
            {
                const char *line =
                    crew_line_in((ChatterKind)kind, roll, NULL, &situation);
                REQUIRE(line != NULL);
                /* The pointer that came back has to be one of the lines this
                 * situation allows. Compared by identity against the table's
                 * own strings, which `crew_line` hands over unfiltered. */
                bool from_the_allowed_set = false;
                for (int i = 0; i < count && !from_the_allowed_set; ++i)
                    if (crew_line_allowed((ChatterKind)kind, i, &situation) &&
                        crew_line((ChatterKind)kind, i) == line)
                        from_the_allowed_set = true;
                CHECK(from_the_allowed_set);
            }
        }
    }

    /* The gate and the speaker filter are separate rules that have to hold at
     * once: the man saying it is ruled out by name, and the hour rules out
     * what may be said at all. */
    CrewSituation lobby = {1, 0};
    for (int speaker = 0; speaker < CREW_SIZE; ++speaker)
    {
        const char *who = crew_callsign(speaker);
        for (int roll = 0; roll < crew_line_count(CHATTER_RADIO); ++roll)
        {
            const char *line =
                crew_line_in(CHATTER_RADIO, roll, who, &lobby);
            REQUIRE(line != NULL);
            CHECK(!test_line_names(line, who));
            CHECK(strlen(line) <= CREW_LINE_MAX);
        }
    }

    /* A situation nobody handed over is every situation, which is what lets
     * the two tests above this one measure the whole table. */
    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
        for (int i = 0; i < crew_line_count((ChatterKind)kind); ++i)
            CHECK(crew_line_allowed((ChatterKind)kind, i, NULL));

    /*
     * Every line is reachable somewhere in the campaign, which is the other
     * way to lose one.
     *
     * This used to be a single situation — the last sector with the whole crew
     * down — and every line had to pass it. That worked only while the gate
     * had a floor and no ceiling. Now that a line which counts men also stops
     * being true (`until_down`), no one moment can satisfy the whole table:
     * `I COUNTED ELEVEN OF US TONIGHT` is written to be sayable while exactly
     * one man is down and at no other time. So the question is asked properly
     * — is there *any* moment of *any* run at which this line may be spoken —
     * which catches both ways a line dies: a `from_sector` past the end of the
     * campaign, and a window whose ceiling is at or below its own floor.
     */
    const int deepest_tally = CREW_SIZE * 8;
    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
    {
        for (int i = 0; i < crew_line_count((ChatterKind)kind); ++i)
        {
            bool ever_sayable = false;
            for (int sector = 1;
                 sector <= (int)EMBEDDED_LEVEL_COUNT && !ever_sayable; ++sector)
            {
                for (int down = 0; down <= deepest_tally; ++down)
                {
                    CrewSituation moment = {sector, down};
                    if (crew_line_allowed((ChatterKind)kind, i, &moment))
                    {
                        ever_sayable = true;
                        break;
                    }
                }
            }
            CHECK(ever_sayable);
        }
    }

    /*
     * And the mirror of the "nobody down" sweep at the top: a ceiling that
     * empties a table is the same failure as a floor that empties one, and it
     * arrives in the half of the campaign where the player has been fighting
     * hardest. A run that has cleared several floors is not an exotic case —
     * it is what the last five sectors are — so every kind still has to have
     * something to say with far more men down than the crew ever had.
     */
    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
    {
        int count = crew_line_count((ChatterKind)kind);
        for (int sector = 1; sector <= (int)EMBEDDED_LEVEL_COUNT; ++sector)
        {
            CrewSituation bloody = {sector, deepest_tally};
            int allowed = 0;
            for (int i = 0; i < count; ++i)
                if (crew_line_allowed((ChatterKind)kind, i, &bloody))
                    allowed++;
            CHECK(allowed > 0);

            for (int roll = 0; roll < count + 3; ++roll)
            {
                const char *line =
                    crew_line_in((ChatterKind)kind, roll, NULL, &bloody);
                REQUIRE(line != NULL);
                bool from_the_allowed_set = false;
                for (int i = 0; i < count && !from_the_allowed_set; ++i)
                    if (crew_line_allowed((ChatterKind)kind, i, &bloody) &&
                        crew_line((ChatterKind)kind, i) == line)
                        from_the_allowed_set = true;
                CHECK(from_the_allowed_set);
            }
        }
    }
}

/* Whether `line` uses `name` as a word of its own — the same question the
 * strip asks, asked again here so the table cannot answer for itself. */
static bool test_line_names(const char *line, const char *name)
{
    size_t n = strlen(name);
    for (const char *at = line; *at != '\0'; ++at)
    {
        if (at != line && isalnum((unsigned char)at[-1]))
            continue;
        if (strncmp(at, name, n) == 0 && !isalnum((unsigned char)at[n]))
            return true;
    }
    return false;
}

/*
 * Nobody on the net talks about himself in the third person.
 *
 * Several lines name a man by callsign, and the roll that picks a line and the
 * slot that picks a name are drawn from different places, so the two used to be
 * free to land on the same one: the strip printed `MARCO: STILL NOTHING FROM
 * MARCO.` and `LENZ: LENZ, ANSWER YOUR HANDSET!`. Rare is not the same as
 * never, and over a campaign of radio checks it is not even rare.
 *
 * Every speaker against every roll, which is the whole space the shell can ask
 * for. The unnamed kind is checked too: the lobby's civilians print no callsign
 * at all, so nothing may be filtered out from under them.
 */
static void test_nobody_on_the_net_names_himself(void)
{
    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
    {
        int count = crew_line_count((ChatterKind)kind);
        CHECK(count > 0);
        for (int speaker = 0; speaker < CREW_SIZE * 2 + 1; ++speaker)
        {
            const char *who = crew_callsign(speaker);
            REQUIRE(who != NULL);
            for (int roll = 0; roll < count + 3; ++roll)
            {
                const char *line =
                    crew_line_said_by((ChatterKind)kind, roll, who);
                CHECK(line != NULL && line[0] != '\0');
                if (line != NULL)
                {
                    CHECK(!test_line_names(line, who));
                    /* Still a line off this kind's own table, and still short
                     * enough for the plate. */
                    CHECK(strlen(line) <= CREW_LINE_MAX);
                }
            }
        }
        /* No callsign, no filter: the roll alone decides, exactly as before. */
        for (int roll = 0; roll < count; ++roll)
        {
            CHECK(crew_line_said_by((ChatterKind)kind, roll, NULL) ==
                  crew_line((ChatterKind)kind, roll));
        }
    }

    /* The rule is a whole-word one: a name inside a longer word is not the man.
     * LENZ is on the docket, so this is the case the scan has to get right. */
    CHECK(crew_line_said_by(CHATTER_RADIO, 0, "LENZ") != NULL);
    CHECK(!test_line_names("LENZMANN WANTS THE LIGHTS ON.", "LENZ"));
    CHECK(test_line_names("TELL LENZ NO.", "LENZ"));
    CHECK(test_line_names("LENZ!", "LENZ"));
}

/*
 * The roll of names after the ending, held to the two things a screen of
 * nothing but type can get wrong.
 *
 * A line wider than the frame it is centred in runs off both edges of the one
 * screen nobody plays through twice, so every line is measured here at its own
 * scale rather than eyeballed. And a roll that never comes to rest never
 * reaches the title screen, which would strand a finished campaign on a
 * scrolling list: the clock is walked end to end to prove it stops.
 */
static void test_credits_fit_the_frame(void)
{
    int count = 0;
    const CreditLine *lines = credits_lines(&count);
    CHECK(count > 0);

    for (int i = 0; i < count; ++i)
    {
        CHECK(credits_line_height(lines[i].kind) > 0.0f);

        float scale = credits_line_scale(lines[i].kind);
        if (scale <= 0.0f)
        {
            /* A rule and a gap are shapes, not sentences. */
            CHECK(lines[i].text == NULL);
            continue;
        }

        CHECK(lines[i].text != NULL && lines[i].text[0] != '\0');
        if (lines[i].text != NULL)
        {
            /* 8px a cell, centred, inside the frame's own inset. */
            float width = (float)strlen(lines[i].text) * 8.0f * scale;
            CHECK(width <= CREDITS_FRAME_W - 2.0f * CREDITS_SIDE_MARGIN);
        }
    }

    CreditsRoll roll;
    credits_init(&roll, 552.0f);
    CHECK(roll.travel > 0.0f);
    CHECK(roll.duration > CREDITS_HOLD_TIME);
    /* And it comes to rest inside the ceiling the header states, so a roll that
     * grew past it fails here rather than quietly doubling the length of the
     * screen a finished campaign always ends on. */
    CHECK(roll.duration <= CREDITS_MAX_DURATION);
    CHECK(!credits_at_rest(&roll));

    bool finished = false;
    for (int step = 0; step < SIM_STEPS(300.0f) && !finished; ++step)
        finished = credits_update(&roll, SIM_STEP_DT);
    CHECK(finished);
    CHECK(credits_at_rest(&roll));
    /* Whatever the clock does past the end, the roll never climbs past its
     * mark: the closing card is where it stops, not a frame it passes. */
    CHECK(credits_scroll(&roll) >= roll.travel);
    CHECK(credits_scroll(&roll) <= roll.travel);

    /* Skipping lands on that same card with the closing hold still to run,
     * rather than on the title screen: confirm means "get on with it" once and
     * "done" the second time. */
    CreditsRoll skipped;
    credits_init(&skipped, 552.0f);
    credits_skip_to_rest(&skipped);
    CHECK(credits_at_rest(&skipped));
    CHECK(credits_scroll(&skipped) >= skipped.travel);
    CHECK(!credits_update(&skipped, CREDITS_HOLD_TIME * 0.5f));
    CHECK(credits_update(&skipped, CREDITS_HOLD_TIME));
}

/*
 * The same rule again, said to the report between sectors.
 *
 * This is the sixth place the plot reaches the player and the last one that
 * had no measurement at all: the table sat inside [cutscene.c](../src/cutscene.c)
 * with its ceiling written down as a sentence — "the first divider stands at
 * x=526" — and nothing holding it there. A line that runs under that divider
 * is exactly as invisible as the manual bullet that fell off the bottom of its
 * own column, and it is worse to lose here, because six of these rows are the
 * whole of the story the player meets while actually playing.
 *
 * So the table moved to [intel.c](../src/intel.c), beside the crew's net, the
 * credits and the manual's sheets, and the renderer now lays the line out from
 * the same two constants this walks it off. Every row is measured, not the six
 * the shipped layout happens to show: a sector that later gains a stair door
 * gains its line with it, and it must not gain it already too long.
 */
static void test_the_report_between_sectors_fits_its_column(void)
{
    int count = intel_line_count();
    CHECK(count > 0);
    /* One row per campaign sector that can be finished. The last sector ends
     * the campaign at the outro rather than at a report, which is why the
     * table is one shorter than the campaign. */
    CHECK(count == (int)EMBEDDED_LEVEL_COUNT - 1);

    float column = INTEL_TEXT_RIGHT - INTEL_TEXT_LEFT;
    CHECK(column > 0.0f);

    for (int i = 0; i < count; ++i)
    {
        const char *line = intel_line(i);
        CHECK(line != NULL);
        if (line == NULL)
            continue;
        /* Not a blank row: the table is indexed by sector, and a sector whose
         * line was never written would show an empty report rather than none
         * at all. */
        CHECK(line[0] != '\0');
        /* 8px a cell at scale 1.0, which is the only size the font is sharp
         * at — a line that does not fit is cut in words, never in scale. */
        CHECK((float)strlen(line) * INTEL_GLYPH_W <= column);
    }

    /* Off both ends, the way the renderer asks for a sector it has no line
     * for: nothing rather than a read past the table. */
    CHECK(intel_line(-1) == NULL);
    CHECK(intel_line(count) == NULL);
}

/*
 * And the line the eleven clears that never reach that report get instead.
 *
 * It is a table of words the player reads, so it owes the same measurement:
 * the frame is in [sector_tally.h](../src/sector_tally.h) and the renderer
 * lays the line out from those constants. What makes this one worth walking
 * rather than eyeballing is that the widest line is not the one anybody would
 * type out to check — it needs the last sector, a clock in the minutes, a
 * record it did not beat, a full par bonus and a clean bonus all at once, and
 * every field of it is produced by a different part of the game.
 *
 * The buffer is measured too. `sector_tally_format` truncates rather than
 * overruns, which means a `SECTOR_TALLY_MAX` that had fallen behind the words
 * would show a line with its end cut off and nothing anywhere would say so —
 * the same failure the report's own column had before it was measured.
 */
static void test_the_sector_tally_fits_the_frame_it_is_drawn_in(void)
{
    float column = SECTOR_TALLY_TEXT_RIGHT - SECTOR_TALLY_TEXT_LEFT;
    CHECK(column > 0.0f);

    /* The full par handed back, which is the largest time bonus that exists:
     * the same arithmetic `campaign_award_sector_bonus` does with an elapsed
     * time of nought. */
    const int widest_time_bonus =
        (int)SECTOR_PAR_SECONDS * SECTOR_TIME_BONUS_PER_SECOND;

    /* Every shape the line can take, and the widest of each field in it.
     * `PROGRESS_MAX_TIME` is the longest clock either half can print, because
     * anything longer is refused by `progress_note_sector_time` and shows as
     * the no-record form instead. */
    const struct
    {
        float elapsed;
        float best;
        bool best_is_new;
        int time_bonus;
        int clean_bonus;
    } shapes[] = {
        /* A record it did not beat: two clocks, both at their widest. */
        {5999.0f, 5999.0f, false, widest_time_bonus, SECTOR_CLEAN_BONUS},
        /* A record it did beat, which spends words where the clock was. */
        {5999.0f, 5999.0f, true, widest_time_bonus, SECTOR_CLEAN_BONUS},
        /* Nothing banked at all. */
        {5999.0f, PROGRESS_NO_TIME, false, widest_time_bonus,
         SECTOR_CLEAN_BONUS},
        /* Over par, and over par having also died. */
        {5999.0f, 5999.0f, false, 0, SECTOR_CLEAN_BONUS},
        {5999.0f, 5999.0f, false, 0, 0},
        /* And the ordinary one, so a shape nobody plays is not the only thing
         * holding the frame. */
        {74.0f, 91.0f, false, 1200, SECTOR_CLEAN_BONUS},
    };
    const int shape_count = (int)(sizeof(shapes) / sizeof(shapes[0]));

    for (int i = 0; i < shape_count; ++i)
    {
        /* Every sector, because the number is in the line and seventeen is two
         * cells where one would have been. */
        for (int sector = 0; sector < (int)EMBEDDED_LEVEL_COUNT; ++sector)
        {
            SectorTally tally;
            sector_tally_clear(&tally);
            /* The widest docket cell is the full collection, which is two
             * digits either side of the slash on a campaign this length. */
            sector_tally_set(&tally, sector, shapes[i].elapsed,
                             shapes[i].best, shapes[i].best_is_new,
                             shapes[i].time_bonus, shapes[i].clean_bonus,
                             CAMPAIGN_SECTORS);

            char line[SECTOR_TALLY_MAX];
            int written = sector_tally_format(&tally, line, sizeof(line));
            CHECK(written > 0);
            CHECK((size_t)written == strlen(line));
            /* Room left in the buffer, or the line above is one that was cut
             * rather than one that fitted. */
            CHECK((size_t)written < sizeof(line) - 1);
            CHECK((float)written * SECTOR_TALLY_GLYPH_W <= column);
            /* The sector is named the way every other screen names it. */
            char expected[16];
            snprintf(expected, sizeof(expected), "SECTOR %02d CLEAR",
                     sector + 1);
            CHECK(strncmp(line, expected, strlen(expected)) == 0);
            /* And the run's paper is on it, clamped to the collection that
             * actually exists rather than to whatever the caller passed. */
            CHECK(tally.docket_total ==
                  CAMPAIGN_SECTORS - CAMPAIGN_CLIMB_SECTOR_COUNT);
            CHECK(tally.docket_sheets == tally.docket_total);
            char docket[24];
            snprintf(docket, sizeof(docket), "DOCKET %02d/%02d",
                     tally.docket_sheets, tally.docket_total);
            CHECK(strstr(line, docket) != NULL);
        }
    }

    /*
     * A run carrying nothing still prints the cell, because the player who has
     * walked past every sheet is the one the cell is for.
     */
    {
        SectorTally none;
        sector_tally_clear(&none);
        sector_tally_set(&none, 0, 74.0f, 91.0f, false, 1200,
                         SECTOR_CLEAN_BONUS, 0);
        char empty_run[SECTOR_TALLY_MAX];
        int written = sector_tally_format(&none, empty_run, sizeof(empty_run));
        CHECK(written > 0);
        CHECK((float)written * SECTOR_TALLY_GLYPH_W <=
              SECTOR_TALLY_TEXT_RIGHT - SECTOR_TALLY_TEXT_LEFT);
        CHECK(strstr(empty_run, "DOCKET 00/") != NULL);
    }

    /* Nothing pending is an empty string rather than the stack, which is the
     * promise the header makes so a caller that draws unconditionally draws
     * nothing. */
    SectorTally empty;
    sector_tally_clear(&empty);
    CHECK(!empty.pending);
    char line[SECTOR_TALLY_MAX];
    memset(line, 'x', sizeof(line));
    CHECK(sector_tally_format(&empty, line, sizeof(line)) == 0);
    CHECK(line[0] == '\0');
    /* And a caller that hands it no room at all is answered rather than
     * written through. */
    CHECK(sector_tally_format(&empty, line, 0) == 0);
    CHECK(sector_tally_format(NULL, line, sizeof(line)) == 0);
}

/*
 * A record that is written and never readable is a record nobody has.
 *
 * `progress_note_sector_time` banks a time for every one of the seventeen
 * sectors, and the report that prints one is shown after six of them: the ten
 * that leave by a window cut straight to the next sector, and the last one
 * cuts to the outro. So eleven per-sector records were being written to the
 * player's disk and kept across sessions with no screen in the game able to
 * show them, while the same eleven clears paid a time bonus and a clean bonus
 * with nothing on screen connecting the score to either.
 *
 * This is the arithmetic half of that, held here so the two halves cannot come
 * apart again: every sector the arc does *not* report on is a sector the tally
 * has to speak for, and the two sets together have to be the whole campaign.
 * A `Y` added to a map moves a sector from one set to the other, which is
 * exactly the edit that took the report off sector 14 in the first place.
 */
static void test_every_sector_reports_or_tallies_and_none_does_neither(void)
{
    int reported = 0;
    int tallied = 0;

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Level level = {0};
        Rng rng;
        rng_seed(&rng, 20250818u + (uint32_t)i);
        CHECK(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                              EMBEDDED_LEVELS[i].data,
                              EMBEDDED_LEVELS[i].size, &rng));

        /* `try_finish_current_level`'s own two tests, in its own order: the
         * last sector ends the campaign however it is left, and below that a
         * window is what suppresses the report. */
        bool last = i + 1 == EMBEDDED_LEVEL_COUNT;
        bool shows_report = !last && !level.map.has_window;
        if (shows_report)
            ++reported;
        else
            ++tallied;
    }

    CHECK(reported + tallied == (int)EMBEDDED_LEVEL_COUNT);
    /* The six the arc is told on, which `INTEL_ARC_SECTORS` already holds
     * against the maps from the other direction. */
    CHECK(reported == INTEL_ARC_SECTOR_COUNT);
    /* And the eleven that had nothing at all before the tally existed. */
    CHECK(tallied == (int)EMBEDDED_LEVEL_COUNT - INTEL_ARC_SECTOR_COUNT);
    CHECK(tallied > 0);
}

/*
 * The manual describes the campaign that is actually in the box.
 *
 * `THE MISSION`'s illustration draws the route as one tick a sector with the
 * climbs in amber, and it is the only picture in the game that states a fact
 * about the campaign rather than about a mechanic. Both numbers it needs used to
 * be written into the drawing loop, so it kept drawing fifteen sectors and four
 * climbs for as long as the tree said seventeen and five everywhere else. The fit
 * checks could not see it — they measure words in a column, and this is a
 * rectangle — so the mismatch had no way of surfacing at all.
 *
 * The climb list is also what `THE CLIMB`'s strap spells out in prose. This
 * check covers the *picture*; the sentence beside it is held by
 * `test_the_manual_says_the_campaign_it_draws` below.
 *
 * **The two used to be one claim in this comment and one check in the code**,
 * which is worse than having neither: the comment said the strap was covered,
 * the strap was a hard-coded string literal, and a reader who trusted the
 * sentence had no reason to look. A comment that promises coverage owes the
 * suite the check that delivers it.
 */
static void test_the_manual_draws_the_campaign_it_ships_with(void)
{
    static Level level;
    Rng rng;

    /* The count the illustration's loop runs to. Pinned elsewhere against the
     * night clock as well, which is what makes it one number rather than two. */
    CHECK(CAMPAIGN_SECTORS == (int)EMBEDDED_LEVEL_COUNT);

    REQUIRE(CAMPAIGN_CLIMB_SECTOR_COUNT > 0);
    int climbs_found = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        rng_seed(&rng, 1717);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        bool is_climb = level.map.mode == LEVEL_MODE_FACADE;
        bool listed = false;
        for (int c = 0; c < CAMPAIGN_CLIMB_SECTOR_COUNT; ++c)
            if (CAMPAIGN_CLIMB_SECTORS[c] == (int)i + 1)
                listed = true;
        /* Both directions: a `MODE FACADE` map the list does not name would be
         * drawn as an interior, and a sector named that is not a facade would be
         * drawn amber for a floor the player walks. */
        CHECK(is_climb == listed);
        if (is_climb)
            ++climbs_found;
    }
    CHECK(climbs_found == CAMPAIGN_CLIMB_SECTOR_COUNT);

    /* And the list is in ascending order and inside the campaign, because the
     * strap prints it as a sentence in exactly this order. */
    for (int c = 0; c < CAMPAIGN_CLIMB_SECTOR_COUNT; ++c)
    {
        CHECK(CAMPAIGN_CLIMB_SECTORS[c] >= 1);
        CHECK(CAMPAIGN_CLIMB_SECTORS[c] <= CAMPAIGN_SECTORS);
        if (c > 0)
            CHECK(CAMPAIGN_CLIMB_SECTORS[c] > CAMPAIGN_CLIMB_SECTORS[c - 1]);
    }
}

/* A small number spelled the way the sheets spell one. NULL past the table,
 * which is a failure at the call site rather than a silent skip. */
static const char *spelled_number(int value)
{
    static const char *const WORDS[] = {
        "ZERO",     "ONE",     "TWO",       "THREE",    "FOUR",
        "FIVE",     "SIX",     "SEVEN",     "EIGHT",    "NINE",
        "TEN",      "ELEVEN",  "TWELVE",    "THIRTEEN", "FOURTEEN",
        "FIFTEEN",  "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN",
        "TWENTY",   "TWENTY-ONE"};
    if (value < 0 || value >= (int)(sizeof(WORDS) / sizeof(WORDS[0])))
        return NULL;
    return WORDS[value];
}

/* By title rather than by index, so reordering the book moves the check with
 * the sheet instead of pointing it at whatever landed in the old slot. */
static const ManualPageText *manual_page_titled(const char *title)
{
    for (int i = 0; i < MANUAL_PAGE_COUNT; ++i)
        if (strcmp(MANUAL_PAGES[i].title, title) == 0)
            return &MANUAL_PAGES[i];
    return NULL;
}

/*
 * The manual *says* the campaign it draws.
 *
 * `THE MISSION`'s illustration reads `CAMPAIGN_SECTORS` and
 * `CAMPAIGN_CLIMB_SECTORS`, which is the fix the comment above describes — and
 * the strap and caption printed on the same sheet, in the same numbers, stayed
 * hard-coded English: `SEVENTEEN SECTORS`, `FIVE OF THEM`, and `THE CLIMB`'s
 * `SECTORS 3, 7, 11, 13 AND 15`. So the picture was pinned to the maps and the
 * sentence beside it was not, on the one screen whose whole job is to show the
 * player the shape of the night. They agreed only because nobody had added a
 * sector since.
 *
 * This is the same trade `INTEL_ARC_SECTORS` makes: the numbers stay written
 * out in the prose, because a strap assembled at runtime would be a sentence
 * nobody can read in the source, and the suite compares the two halves. A
 * sixth climb now fails the build with the sheet named.
 */
static void test_the_manual_says_the_campaign_it_draws(void)
{
    const ManualPageText *mission = manual_page_titled("THE MISSION");
    const ManualPageText *climb = manual_page_titled("THE CLIMB");
    REQUIRE(mission != NULL);
    REQUIRE(climb != NULL);

    char expected[MANUAL_CAPTION_MAX + 64];

    /* "SEVENTEEN SECTORS BETWEEN THE LOBBY AND THE ROOF" */
    const char *sectors_word = spelled_number(CAMPAIGN_SECTORS);
    REQUIRE(sectors_word != NULL);
    snprintf(expected, sizeof expected, "%s SECTORS", sectors_word);
    CHECK(strncmp(mission->strap, expected, strlen(expected)) == 0);

    /* "FIVE OF THEM ARE ON THE OUTSIDE" */
    const char *climbs_word = spelled_number(CAMPAIGN_CLIMB_SECTOR_COUNT);
    REQUIRE(climbs_word != NULL);
    snprintf(expected, sizeof expected, "%s OF THEM", climbs_word);
    CHECK(strncmp(mission->caption, expected, strlen(expected)) == 0);

    /* "SECTORS 3, 7, 11, 13 AND 15 ARE CLIMBED, NOT WALKED", assembled in the
     * order the list is already required to be in a few lines above. */
    size_t used = 0;
    int written = snprintf(expected, sizeof expected, "SECTORS ");
    REQUIRE(written > 0 && (size_t)written < sizeof expected);
    used = (size_t)written;
    for (int c = 0; c < CAMPAIGN_CLIMB_SECTOR_COUNT; ++c)
    {
        const char *separator =
            c == 0 ? ""
                   : (c == CAMPAIGN_CLIMB_SECTOR_COUNT - 1 ? " AND " : ", ");
        written = snprintf(expected + used, sizeof expected - used, "%s%d",
                           separator, CAMPAIGN_CLIMB_SECTORS[c]);
        REQUIRE(written > 0 && (size_t)written < sizeof expected - used);
        used += (size_t)written;
    }
    CHECK(strncmp(climb->strap, expected, used) == 0);
}

/*
 * The same rule again, said to the two tables that were still spelling the
 * campaign out of memory.
 *
 * `test_the_manual_says_the_campaign_it_draws` above exists because the
 * manual's straps were hard-coded English about a campaign that had grown, and
 * the fix was to hold the sentence against the constant. Two tables of words
 * were left out of it, and both are read by the player.
 *
 * The credits roll opens on `FORTY FLOORS. SEVENTEEN SECTORS. ONE NIGHT.` and
 * closes on `TWELVE NAMES ON THEIR DOCKET.` — the campaign's length and the
 * crew's size, written out as words, on the one screen a finished campaign
 * always ends on. `test_credits_fit_the_frame` measures every line of that roll
 * against the frame and asks nothing at all about what any of them says, so a
 * seventeen-sector night would have gone on being credited as fifteen for as
 * long as nobody sat through the ending with the map count in mind.
 *
 * The crew's own net is the other one: two lines do the arithmetic out loud, and
 * the gate beside them was already written off `CREW_SIZE` while the words next
 * to it were not.
 *
 * Both are found by what they say rather than by their index, so reordering
 * either table moves the check with the line instead of pointing it at whatever
 * landed in the old slot. A line that stops matching is a failed `REQUIRE`,
 * which is the loud half of the rule `check_docs.py` keeps for the prose: a
 * check that can no longer find its claim has to say so.
 */
static const char *credits_note_containing(const char *needle)
{
    int count = 0;
    const CreditLine *lines = credits_lines(&count);
    for (int i = 0; i < count; ++i)
        if (lines[i].text != NULL && strstr(lines[i].text, needle) != NULL)
            return lines[i].text;
    return NULL;
}

static void test_the_credits_say_the_campaign_they_roll_over(void)
{
    char expected[64];

    const char *sectors = credits_note_containing(" SECTORS");
    REQUIRE(sectors != NULL);
    const char *sectors_word = spelled_number(CAMPAIGN_SECTORS);
    REQUIRE(sectors_word != NULL);
    snprintf(expected, sizeof expected, "%s SECTORS", sectors_word);
    CHECK(strstr(sectors, expected) != NULL);

    const char *docket = credits_note_containing("ON THEIR DOCKET");
    REQUIRE(docket != NULL);
    const char *crew_word = spelled_number(CREW_SIZE);
    REQUIRE(crew_word != NULL);
    snprintf(expected, sizeof expected, "%s NAMES ON THEIR DOCKET", crew_word);
    CHECK(strstr(docket, expected) != NULL);

    /* And the roll's own boast, which is the reason it names the docket at all:
     * one name against theirs. A second name in `LINES` is a credit this game
     * has not earned, and the sentence above it would be false. */
    int count = 0;
    const CreditLine *lines = credits_lines(&count);
    const char *only = NULL;
    for (int i = 0; i < count; ++i)
    {
        if (lines[i].kind != CREDIT_NAME)
            continue;
        if (only == NULL)
            only = lines[i].text;
        REQUIRE(lines[i].text != NULL);
    }
    REQUIRE(only != NULL);
}

/*
 * The two lines on the net that count the crew out loud.
 *
 * Found by the shape of the claim rather than by index, and both directions are
 * checked: a line that says TWELVE has to mean `CREW_SIZE`, and a line that says
 * ELEVEN beside it has to mean the eleven still standing. The gate on both rows
 * was already `CREW_SIZE / 2`; the words were not.
 */
static void test_the_net_counts_the_crew_it_has(void)
{
    const char *crew_word = spelled_number(CREW_SIZE);
    const char *rest_word = spelled_number(CREW_SIZE - 1);
    REQUIRE(crew_word != NULL && rest_word != NULL);

    int counting_lines = 0;
    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
    {
        int count = crew_line_count((ChatterKind)kind);
        for (int i = 0; i < count; ++i)
        {
            const char *line = crew_line((ChatterKind)kind, i);
            REQUIRE(line != NULL);
            if (strstr(line, " OF US") == NULL)
                continue;
            counting_lines++;
            /* "TWELVE OF US, ONE OF HIM" and "TWELVE OF US! WHERE ARE THE
             * OTHER ELEVEN?" — the roster, spelled. */
            CHECK(strstr(line, crew_word) != NULL);
            if (strstr(line, "OTHER ") != NULL)
                CHECK(strstr(line, rest_word) != NULL);
        }
    }
    /* And the claim is still findable: nought of them means the lines were
     * reworded and this check has stopped checking anything. */
    CHECK(counting_lines >= 2);
}

/*
 * The plot is told on the sectors that actually reach a report.
 *
 * This is the check the report table went without, and going without it cost
 * the game a plot beat in the shipped build. `try_finish_current_level` shows a
 * report only after a sector that is not the last and does not leave by a
 * window, because a window is a continuous physical route onto the facade and a
 * cut to a briefing screen would contradict what is on the display. So *which*
 * rows of `TRANSITION_INTEL` a player ever reads is decided by the maps, in a
 * file the maps know nothing about.
 *
 * The day sector 15 became a climb, sector 14 had to gain a `Y` to put Chuck on
 * the wall — and that `Y` silently deleted the report after it, which was
 * `TWO-KEY DOOR. SHE IS THE SECOND.`: the answer to sector 8's turn and the only
 * place the game says why the hostage is still alive. Every other check passed.
 * The fit test measured the row, the suite loaded the map, and nothing compared
 * the two, so the beat was simply gone.
 *
 * Both halves are asserted, in both directions: every arc sector really does
 * reach a report, and every sector that reaches one really is on the arc. One
 * direction alone would let a new stair door quietly add a sixth-and-a-half
 * beat nobody wrote for that slot.
 */
static void test_the_arc_lands_on_the_sectors_that_show_a_report(void)
{
    static Level level;
    Rng rng;
    bool shows_report[64] = {false};

    REQUIRE(EMBEDDED_LEVEL_COUNT > 0);
    REQUIRE(EMBEDDED_LEVEL_COUNT <= sizeof(shows_report) / sizeof(shows_report[0]));
    REQUIRE(INTEL_ARC_SECTOR_COUNT > 0);

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        rng_seed(&rng, 4242);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        /* `game.c`'s own two conditions, and nothing else: there has to be a
         * next sector to report on the way to, and the finished one has to have
         * left by its stair door. */
        bool has_next = (i + 1) < EMBEDDED_LEVEL_COUNT;
        shows_report[i] = has_next && !level.map.has_window;
    }

    /* Every sector the arc is written for reaches a report. */
    for (int i = 0; i < INTEL_ARC_SECTOR_COUNT; ++i)
    {
        int sector = INTEL_ARC_SECTORS[i];
        REQUIRE(sector >= 1 && (size_t)sector <= EMBEDDED_LEVEL_COUNT);
        /* Fails as "sector N carries the arc but shows no report" — which is
         * either a `Y` that wants taking back out of the map, or a beat that
         * wants moving to a sector the player still passes through. */
        CHECK(shows_report[sector - 1]);
        /* And it has a line to show. */
        CHECK(intel_line(sector - 1) != NULL);
    }

    /* And no sector shows one the arc did not account for. */
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        if (!shows_report[i])
            continue;
        bool on_the_arc = false;
        for (int a = 0; a < INTEL_ARC_SECTOR_COUNT; ++a)
            if (INTEL_ARC_SECTORS[a] == (int)i + 1)
                on_the_arc = true;
        CHECK(on_the_arc);
    }
}

/* Whether the net gains a line on arriving at `sector` that it did not have on
 * the sector below — which is what a `from_sector` gate is for, and therefore
 * what "the crew has something new to say here" means.
 *
 * Asked through `crew_line_allowed` rather than by reading the gates, because
 * the gates are private to [crew.c](../src/crew.c) and should stay that way: the
 * table is presentation, and a test that reached into it would be a second copy
 * of the schedule instead of a question about it. Both situations hold
 * `hostiles_down` at nought so that only the *sector* gate can flip — a line
 * that also waits on a body is invisible to this, which makes the answer
 * conservative and the check stricter rather than looser.
 */
static bool the_net_gains_a_line_at(int sector)
{
    for (int kind = 0; kind < CHATTER_KIND_COUNT; ++kind)
    {
        int count = crew_line_count((ChatterKind)kind);
        for (int i = 0; i < count; ++i)
        {
            CrewSituation here = {sector, 0};
            CrewSituation below = {sector - 1, 0};
            if (crew_line_allowed((ChatterKind)kind, i, &here) &&
                !crew_line_allowed((ChatterKind)kind, i, &below))
                return true;
        }
    }
    return false;
}

/*
 * No stretch of the campaign goes quiet.
 *
 * The test above holds the arc to the sectors that show a report. What neither
 * it nor anything else asked is the question a *player* would: how long can you
 * play without the story saying anything new? The answer, when this was written,
 * was six sectors — and nothing failed, because every individual check passed.
 * The reports land after sectors 1, 4, 5, 8, 9 and 16, which is five beats in the
 * first nine sectors and then silence from 10 to 15, a third of the campaign at
 * par.
 *
 * **The cause is structural rather than an authoring slip**, which is why it
 * needs a check of its own. A report is shown only after a sector that leaves by
 * its stair door, and from sector 10 the campaign alternates interior and climb —
 * so 10, 12 and 14 all need a `Y` to put Chuck on the wall, and a `Y` suppresses
 * the report. The gap is not something anybody typed; it is what the sector
 * ordering does, and the next reshuffle can widen it just as silently.
 *
 * So the two channels are counted together, because between them they are what
 * the player actually meets: the report card, which cannot be missed, and the
 * crew's net gaining a line it did not have a sector ago. A sector carries a beat
 * if either happens on it.
 *
 * One sector may pass without one — that is the climbs, which have no guards to
 * talk and no report on the way in or out, and a wordless climb is the point of a
 * climb. Two in a row may not: that is the stretch where a player starts to
 * wonder whether the game has forgotten what it was about.
 */
#define QUIET_SECTORS_ALLOWED_IN_A_ROW 1

static void test_no_two_sectors_in_a_row_go_quiet(void)
{
    static Level level;
    Rng rng;
    bool carries_a_beat[64] = {false};
    bool on_the_wall[64] = {false};

    REQUIRE(EMBEDDED_LEVEL_COUNT > 0);
    REQUIRE(EMBEDDED_LEVEL_COUNT <=
            sizeof(carries_a_beat) / sizeof(carries_a_beat[0]));

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        rng_seed(&rng, 4243);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        /* `game.c`'s own two conditions for a report, the same pair the arc test
         * derives, plus the net's own question. */
        bool has_next = (i + 1) < EMBEDDED_LEVEL_COUNT;
        bool shows_report = has_next && !level.map.has_window;
        carries_a_beat[i] =
            shows_report || the_net_gains_a_line_at((int)i + 1);
        on_the_wall[i] = level.map.mode == LEVEL_MODE_FACADE;
    }

    /*
     * A quiet sector has to be a climb, which is the sentence the run length
     * below was standing in for and did not say.
     *
     * The allowance is one in a row and the argument for it is the wall: no
     * guards to talk, no report either side, and a wordless climb is the point
     * of a climb. Sector 17 was spending that allowance instead. The last of the
     * six reports lands on the vault at 16 and the net's highest gate was 14, so
     * the roof — the floor the whole night is played for, with eight men, three
     * dogs and four heavies on it — arrived with nothing new to say on either
     * channel, and the run-length check waved it through because sector 16 in
     * front of it carried one.
     *
     * That is the failure this file keeps finding in a different costume: a check
     * that passes for a reason other than the one written above it. The run
     * length is still worth keeping — two quiet sectors in a row is a different
     * complaint — but the rule is this one.
     */
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        if (carries_a_beat[i] || on_the_wall[i])
            continue;
        fprintf(stderr,
                "  sector %d is an interior and carries no beat: no report "
                "after it and nothing new on the net at it\n", (int)i + 1);
        CHECK(carries_a_beat[i]);
    }

    int quiet_run = 0;
    int worst_run = 0;
    int worst_ended_at = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        if (carries_a_beat[i])
        {
            quiet_run = 0;
            continue;
        }
        quiet_run++;
        if (quiet_run > worst_run)
        {
            worst_run = quiet_run;
            worst_ended_at = (int)i + 1;
        }
    }

    /*
     * Said out loud before the check, because the check alone cannot say it.
     * `CHECK` prints the expression, and `worst_run <= 1` names neither the
     * length of the silence nor where it is — which would leave whoever hit this
     * re-deriving both by hand from seventeen maps and five chatter tables. The
     * fix is one of two edits and the numbers are what tell you which: a beat
     * gated onto one of those sectors in `crew.c`, or a sector ordering that
     * gives one of them its stair door back.
     */
    if (worst_run > QUIET_SECTORS_ALLOWED_IN_A_ROW)
    {
        fprintf(stderr,
                "  sectors %d-%d carry no beat: %d in a row against %d "
                "allowed\n",
                worst_ended_at - worst_run + 1, worst_ended_at, worst_run,
                QUIET_SECTORS_ALLOWED_IN_A_ROW);
    }
    CHECK(worst_run <= QUIET_SECTORS_ALLOWED_IN_A_ROW);

    /* And the count is not vacuously nought: a run of sectors that all carry a
     * beat would pass the check above while meaning the two channels had stopped
     * being distinguishable. At least one climb has to be quiet, or this test is
     * measuring something other than what it claims. */
    CHECK(worst_run >= 1);
}

/*
 * The same rule as the credits' own fit test, said to the manual.
 *
 * `render_text_column` stops at MANUAL_BODY_BOTTOM rather than drawing past it
 * and never wraps a line, which is right for a frame and silent for whoever is
 * writing the sheet: a page that outgrows its column loses its last lines and
 * says nothing about it. CONTROLS did exactly that, and what fell off the
 * bottom was the only line in the game that named the key which closes it — a
 * rule documented, written down and never drawn.
 *
 * It used to be a `CHUCK_DEBUG` assert inside `manual_init`, so it only ever
 * ran for somebody who opened the book in a debug build; the sheets moved to
 * [manual_pages.c](../src/manual_pages.c) so this could hold them on every
 * `make test` instead.
 *
 * The width half was never checked at all, and a line that runs off the side is
 * as invisible as one below the bottom. A control row's chip columns are as
 * wide as the widest label on its own sheet and the pad column is spelled for
 * whatever is plugged in, so the widest spelling the game can meet — a
 * PlayStation pad's `[]`, `/\` and OPTIONS — is what the sheets are measured
 * against here.
 */
static void test_manual_sheets_fit_the_column(void)
{
    /* How the widest pad in circulation spells the tokens the control rows are
     * written in. Kept here rather than read off `PadHints`, which needs SDL. */
    static const struct
    {
        const char *token;
        const char *widest;
    } PAD_SPELLINGS[] = {
        {"$A", "X"},   {"$B", "O"},        {"$X", "[]"},   {"$Y", "/\\"},
        {"$LB", "L1"}, {"$RB", "R1"},      {"$START", "OPTIONS"},
        {"$SELECT", "CREATE"},
    };

    for (int page_index = 0; page_index < MANUAL_PAGE_COUNT; ++page_index)
    {
        const ManualPageText *page = &MANUAL_PAGES[page_index];

        CHECK(page->title != NULL && page->title[0] != '\0');
        CHECK(page->strap != NULL && page->strap[0] != '\0');
        REQUIRE(page->caption != NULL);
        CHECK(page->line_count > 0);
        /* A caption is clipped to the plate rather than trusted to be short, so
         * one written past it is a sentence with its end cut off. */
        CHECK((int)strlen(page->caption) <= MANUAL_CAPTION_MAX);

        /* Every line reaches the frame at all. */
        CHECK(manual_page_lines_fit(page));

        /* And the two chip columns, sized off this sheet's widest labels the
         * way `key_columns` sizes them, with the pad column spelled wide. */
        size_t key_cells = 0;
        size_t pad_cells = 0;
        for (int i = 0; i < page->line_count; ++i)
        {
            const ManualLine *line = &page->lines[i];
            if (line->kind != LINE_KEY)
            {
                /* A gap carries no words; everything else has to. */
                if (line->kind == LINE_GAP)
                    CHECK(line->text == NULL);
                else
                    CHECK(line->text != NULL && line->text[0] != '\0');
                continue;
            }

            REQUIRE(line->text != NULL);
            const char *first = strchr(line->text, '|');
            CHECK(first != NULL);
            if (first == NULL)
                continue;
            const char *second = strchr(first + 1, '|');
            /* Three fields: keyboard, pad, action. A row short of one would be
             * drawn with an empty chip rather than rejected. */
            CHECK(second != NULL);
            if (second == NULL)
                continue;

            if ((size_t)(first - line->text) > key_cells)
                key_cells = (size_t)(first - line->text);

            /* The pad field, with each token grown to its widest spelling. */
            char pad_field[64];
            size_t field_len = (size_t)(second - first - 1);
            CHECK(field_len < sizeof(pad_field));
            memcpy(pad_field, first + 1, field_len);
            pad_field[field_len] = '\0';

            size_t spelled = 0;
            for (size_t at = 0; at < field_len;)
            {
                if (pad_field[at] != '$')
                {
                    spelled++;
                    at++;
                    continue;
                }
                size_t matched = 0;
                for (size_t t = 0; t < sizeof(PAD_SPELLINGS) /
                                           sizeof(PAD_SPELLINGS[0]);
                     ++t)
                {
                    size_t token_len = strlen(PAD_SPELLINGS[t].token);
                    if (token_len > matched &&
                        strncmp(pad_field + at, PAD_SPELLINGS[t].token,
                                token_len) == 0)
                    {
                        matched = token_len;
                        spelled += strlen(PAD_SPELLINGS[t].widest);
                    }
                }
                /* A `$` token this test does not know is one the spelling table
                 * in pad_hint.c does not know either. */
                CHECK(matched > 0);
                at += matched > 0 ? matched : 1;
            }
            if (spelled > pad_cells)
                pad_cells = spelled;
        }

        float key_w = MANUAL_CH * (float)key_cells + MANUAL_KEY_CHIP_PAD;
        float pad_w = MANUAL_CH * (float)pad_cells + MANUAL_KEY_CHIP_PAD;
        CHECK(manual_page_lines_fit_width(page, key_w, pad_w));
    }
}

/* The other half of the same rule: the building has to be quiet for it. A
 * guard hunting Chuck is not filing a routine report. */
static void test_no_radio_checks_while_the_alarm_is_up(void)
{
    static const char data[] =
        "##############################\n"
        "#S           M              E#\n"
        "##############################\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 24601);
    CHECK(level_load_data(&state.level, "radio", data, strlen(data),
                          &state.rng));
    gameplay_ai_spawn_level_entities(&state);
    state.enemies[0].on_ground = true;
    state.player.x = 32.0f;
    state.player.y = 32.0f;
    gameplay_trigger_alarm(&state, 32.0f, 32.0f, -1);

    for (int step = 0; step < SIM_STEPS(30.0f); ++step)
    {
        game_events_clear(&state.events);
        /* Hold the alarm up for the whole window. */
        state.terminal_alarm_timer = ALARM_CALM_TIME;
        gameplay_ai_update_movement(&state, SIM_STEP_DT);
        CHECK(!events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                                 SFX_GUARD_RADIO));
    }
}

/*
 * The cordon closing around the tower. It is scenery — nothing about it is
 * simulated and nothing can be hit — but it has to arrive in the right order,
 * because the point of it is that the city gets tighter the nearer the drive
 * gets to the building. The first blocks are an ordinary night; after that the
 * junctions start being held.
 */
static void test_chase_cordon_thickens_toward_the_building(void)
{
    int early_held = 0;
    int early_total = 0;
    int late_held = 0;
    int late_total = 0;

    /* One drive is a handful of junctions, and a handful of coin flips proves
     * nothing about a ramp. A dozen seeds is still deterministic and is enough
     * of the route to read the shape off. */
    for (unsigned seed = 0; seed < 12u; ++seed)
    {
        Chase chase;
        chase_init(&chase, 90210u + seed * 7919u);
        chase_skip_departure(&chase);

        Input input = {0};
        input.gas = true;
        bool seen[CHASE_MAX_INTERSECTIONS] = {false};
        float last_y[CHASE_MAX_INTERSECTIONS] = {0.0f};

        for (int step = 0; step < SIM_STEPS(40.0f); ++step)
        {
            chase_step(&chase, &input);
            for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
            {
                const ChaseIntersection *junction = &chase.intersections[i];
                if (!junction->active)
                {
                    seen[i] = false;
                    continue;
                }
                /* A recycled slot is a new junction: its y jumps forward. */
                if (seen[i] && fabsf(junction->y - last_y[i]) < 1.0f)
                    continue;
                seen[i] = true;
                last_y[i] = junction->y;
                /* Whatever it is, it is one of three states and nothing else:
                 * absent, on the near pavement, or on the far one. */
                CHECK(junction->cordon_side >= -1 &&
                      junction->cordon_side <= 1);

                int block = (int)(junction->y / CHASE_BLOCK_LENGTH);
                bool held = junction->cordon_side != 0;
                if (block < CHASE_CORDON_FIRST_BLOCK)
                {
                    /* The first blocks are an ordinary night out on the ring
                     * road, and this half of the rule is exact. */
                    CHECK(!held);
                }
                else if (block < CHASE_CORDON_FIRST_BLOCK + 3)
                {
                    early_total++;
                    early_held += held ? 1 : 0;
                }
                else
                {
                    late_total++;
                    late_held += held ? 1 : 0;
                }
            }
        }
    }

    CHECK(early_total > 0);
    CHECK(late_total > 0);
    /* The city gets tighter the closer the drive gets to the tower. */
    CHECK(late_held * early_total > early_held * late_total);
}

/*
 * And it keeps thickening across a crash.
 *
 * The ramp is read off the block a junction is generated in, so it is only a
 * ramp while the route the player is on runs forward. A retry used to put the
 * car back on block zero while keeping most of the pursuit clock, which spent
 * the last of the drive rebuilding the ring from its thinnest end: measured,
 * a crash two thirds of the way in arrived at the tower past a street with
 * nought to three junctions of twelve held, where a clean run arrives past a
 * near-solid one. The test above only ever drives clean, which is why this one
 * exists — the bug it pins lives entirely on the other side of a wreck.
 */
static void test_chase_cordon_survives_a_crash(void)
{
    for (unsigned seed = 0; seed < 6u; ++seed)
    {
        Chase chase;
        chase_init(&chase, 5150u + seed * 4441u);
        chase_skip_departure(&chase);

        Input input = {0};
        input.gas = true;
        /* Twenty clean seconds. The road is swept every frame so the only
         * wreck in this test is the one it stages below — a stray collision
         * would spend the rewind before the interesting one happens. */
        for (int step = 0; step < SIM_STEPS(20.0f); ++step)
        {
            chase_clear_traffic(&chase);
            chase_step(&chase, &input);
        }
        CHECK(chase.attempts == 0);

        /* Wreck the car outright: one hit left, and something parked on it. */
        float crashed_at_y = chase.player.y;
        chase.player.integrity = 1;
        chase.player.invuln_timer = 0.0f;
        chase_clear_traffic(&chase);
        ChaseCar *car = chase_place_car_ahead(&chase, 0);
        car->y = chase.player.y;
        chase_step(&chase, &input);
        CHECK(chase.phase == CHASE_PHASE_FAILED);

        /* Let the failure play out into the next attempt. */
        chase_run(&chase, &input, CHASE_FAILED_DURATION + 0.5f);
        CHECK(chase.phase == CHASE_PHASE_PURSUIT);

        /* The retry hands back a beat of road, not the whole route: the car is
         * behind where it crashed and well ahead of the kerb it started on. */
        CHECK(chase.player.y < crashed_at_y);
        CHECK(chase.player.y >
              crashed_at_y - CHASE_FAIL_REWIND * CHASE_CRUISE_SPEED - 1.0f);
        CHECK(chase.player.y > CHASE_BLOCK_LENGTH);

        /* Drive it out and read the ring the arrival is reached through. */
        int held = 0;
        int total = 0;
        bool seen[CHASE_MAX_INTERSECTIONS] = {false};
        float last_y[CHASE_MAX_INTERSECTIONS] = {0.0f};
        for (int step = 0; step < SIM_STEPS(60.0f); ++step)
        {
            chase_step(&chase, &input);
            for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
            {
                const ChaseIntersection *junction = &chase.intersections[i];
                if (!junction->active)
                {
                    seen[i] = false;
                    continue;
                }
                if (seen[i] && fabsf(junction->y - last_y[i]) < 1.0f)
                    continue;
                seen[i] = true;
                last_y[i] = junction->y;
                total++;
                held += junction->cordon_side != 0 ? 1 : 0;
            }
            if (chase.phase == CHASE_PHASE_ARRIVAL ||
                chase.phase == CHASE_PHASE_DONE)
                break;
        }
        CHECK(chase.phase == CHASE_PHASE_ARRIVAL ||
              chase.phase == CHASE_PHASE_DONE);
        /* Everything past the wreck is deep in the ramp, so most of it is
         * held. Before the fix this side of the drive ran at a quarter. */
        CHECK(total >= 4);
        CHECK(held * 2 > total);
    }
}

/*
 * The options sheet is a table and a struct, and these pin the three things
 * that make it one: the cursor cannot land on a heading, the file survives a
 * round trip, and a file that has been edited into nonsense loads as the
 * settings it does not mention rather than as a reset.
 */
/*
 * The binding model: one key does one job, every key prints a name that fits,
 * and the sheet cannot be locked shut.
 */
static void test_key_bindings_keep_one_key_to_one_job(void)
{
    KeyBindings b;
    keybind_defaults(&b);

    /* What the game has always been played with. */
    CHECK(keybind_action_has(&b, BIND_LEFT, 80));  /* LEFT */
    CHECK(keybind_action_has(&b, BIND_LEFT, 4));   /* A */
    CHECK(keybind_action_has(&b, BIND_JUMP, 225)); /* LSHIFT */
    CHECK(keybind_action_has(&b, BIND_SHOOT, 44)); /* SPACE */

    /* No key starts out doing two things, which is the invariant every rebind
     * below has to preserve. */
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        for (int slot = 0; slot < BIND_SLOTS; ++slot)
        {
            int code = b.keys[a][slot];
            if (code == KEYBIND_NONE)
                continue;
            int owners = 0;
            for (int other = 0; other < BIND_COUNT; ++other)
                if (keybind_action_has(&b, (BindAction)other, code))
                    ++owners;
            CHECK(owners == 1);
        }
    }

    /* Taking a key takes it off whoever had it — and hands that action the key
     * this slot was holding, which is the half this test used to have backwards.
     * Binding LEFT's arrow onto the jump leaves LEFT holding A and the LSHIFT
     * the jump gave up. A key that fires two actions is indistinguishable from
     * the game being broken; an action that fires nothing is worse. */
    CHECK(keybind_set(&b, BIND_JUMP, 0, 80));
    CHECK(keybind_action_has(&b, BIND_JUMP, 80));
    CHECK(!keybind_action_has(&b, BIND_LEFT, 80));
    CHECK(keybind_action_has(&b, BIND_LEFT, 4));
    CHECK(keybind_action_has(&b, BIND_LEFT, 225)); /* the jump's LSHIFT */

    /*
     * The edit this rule exists for, and the one everybody makes: jump onto
     * SPACE.
     *
     * Cleared, ATTACK came out of it answering nothing at all and the run
     * carried on with no way to fire — announced by a "-" on a prompt and
     * nowhere else. Swapped, the two keys change places and both actions still
     * work, which is what the player asked for and all they asked for.
     */
    keybind_defaults(&b);
    CHECK(keybind_set(&b, BIND_JUMP, 0, 44));      /* SPACE onto the jump */
    CHECK(keybind_action_has(&b, BIND_JUMP, 44));
    CHECK(keybind_action_has(&b, BIND_SHOOT, 225)); /* ATTACK takes LSHIFT */
    CHECK(!keybind_action_has(&b, BIND_SHOOT, 44));

    /*
     * And when there is nothing to swap back, the bind is refused rather than
     * emptying the action.
     *
     * SPACE into the jump's *second* slot is the same request with nothing on
     * offer in exchange: the slot is empty, so ATTACK would simply lose its
     * only key. The sheet answers it the way it answers an unbindable key, and
     * nothing on the struct moves. USE is why this is a refusal and not a
     * shrug — see `test_no_sector_is_locked_behind_an_unbindable_action`.
     */
    keybind_defaults(&b);
    KeyBindings untouched = b;
    CHECK(!keybind_set(&b, BIND_JUMP, 1, 44));
    CHECK(memcmp(&untouched, &b, sizeof(b)) == 0);

    /*
     * A hand-edited file can also put one key on two actions, and the first
     * bind after it has to leave the table honest.
     *
     * `settings_parse` writes the rows straight in — a loader that refused a
     * line would be a file that can stop the game starting — so this is the
     * only place the invariant can be restored, and the clearing version used
     * to restore it for free.
     */
    keybind_defaults(&b);
    b.keys[BIND_USE][1] = 44; /* SPACE, which ATTACK already holds */
    CHECK(keybind_action_has(&b, BIND_SHOOT, 44));
    CHECK(keybind_action_has(&b, BIND_USE, 44));
    CHECK(keybind_set(&b, BIND_WEAPON_PREV, 0, 44));
    CHECK(keybind_action_has(&b, BIND_WEAPON_PREV, 44));
    CHECK(!keybind_action_has(&b, BIND_SHOOT, 44));
    CHECK(!keybind_action_has(&b, BIND_USE, 44));
    /* USE keeps its own key through it; only the copy it should never have had
     * is gone. */
    CHECK(keybind_action_has(&b, BIND_USE, 8));

    /* An action left empty by a hand-edited file is still a state the rest of
     * the game draws — the sheet shows an empty cap, the in-sector prompt shows
     * "-" — so emptiness is legal to *hold* and only illegal to *cause*. */
    keybind_defaults(&b);
    b.keys[BIND_USE][0] = KEYBIND_NONE;
    CHECK(!keybind_action_has(&b, BIND_USE, 8));
    CHECK(keybind_key_name(b.keys[BIND_USE][0])[0] == '\0');

    /* And putting a key the action already holds into its other slot must not
     * leave it holding the same key twice — the same swap, inside one action,
     * so both keys survive it. */
    keybind_defaults(&b);
    CHECK(keybind_set(&b, BIND_LEFT, 1, 80));
    CHECK(b.keys[BIND_LEFT][1] == 80);
    CHECK(b.keys[BIND_LEFT][0] == 4); /* the A it swapped with */
    CHECK(!keybind_action_has(&b, BIND_JUMP, 80));

    /*
     * The three keys that are the way back to this sheet. ESC, ENTER and
     * BACKSPACE are pause, confirm and back, so a settings screen that let them
     * be bound is a settings screen that can lock the player out of itself.
     */
    keybind_defaults(&b);
    const int escape = 41;
    const int ret = 40;
    const int backspace = 42;
    CHECK(!keybind_is_bindable(escape));
    CHECK(!keybind_is_bindable(ret));
    CHECK(!keybind_is_bindable(backspace));
    CHECK(!keybind_set(&b, BIND_JUMP, 0, escape));
    CHECK(keybind_action_has(&b, BIND_JUMP, 225)); /* unchanged */
    CHECK(!keybind_is_bindable(KEYBIND_NONE));

    /* Out-of-range slots and actions change nothing rather than writing past
     * the table. */
    CHECK(!keybind_set(&b, BIND_JUMP, BIND_SLOTS, 5));
    CHECK(!keybind_set(&b, BIND_COUNT, 0, 5));
    CHECK(!keybind_set(&b, (BindAction)-1, 0, 5));
}

/*
 * Every bindable key spells itself inside the cap the sheet draws, and every
 * name round-trips.
 *
 * The cap is sized from `KEYBIND_NAME_MAX` and two of them sit on one row, so a
 * key naming itself longer than that is a row hanging off the plate — the
 * failure the manual's control sheet has already had once, which is why this is
 * a check and not a comment.
 */
static void test_every_bindable_key_fits_its_cap(void)
{
    static const int CODES[] = {
#define CHUCK_KEY_CODE(ident, code, name) code,
        CHUCK_KEY_LIST(CHUCK_KEY_CODE)
#undef CHUCK_KEY_CODE
    };
    static const char *const NAMES[] = {
#define CHUCK_KEY_NAME(ident, code, name) name,
        CHUCK_KEY_LIST(CHUCK_KEY_NAME)
#undef CHUCK_KEY_NAME
    };
    const int count = (int)(sizeof(CODES) / sizeof(CODES[0]));
    CHECK(count > 0);

    for (int i = 0; i < count; ++i)
    {
        size_t len = strlen(NAMES[i]);
        CHECK(len > 0);
        CHECK(len <= KEYBIND_NAME_MAX);

        /* The table is the only thing that maps either way, so both ways are
         * checked: a name that does not come back is a binding the settings
         * file cannot reload. */
        CHECK(strcmp(keybind_key_name(CODES[i]), NAMES[i]) == 0);
        CHECK(keybind_key_from_name(NAMES[i], len) == CODES[i]);
        CHECK(keybind_is_bindable(CODES[i]));

        /* And no code or name appears twice, which would make one of the two
         * lookups pick a winner arbitrarily. */
        for (int j = i + 1; j < count; ++j)
        {
            CHECK(CODES[i] != CODES[j]);
            CHECK(strcmp(NAMES[i], NAMES[j]) != 0);
        }
    }

    /* A name from a build that knew more keys is not a key. */
    CHECK(keybind_key_from_name("NOPE", 4) == KEYBIND_NONE);
    CHECK(keybind_key_from_name(NULL, 0) == KEYBIND_NONE);
    CHECK(keybind_key_name(4242)[0] == '\0');

    /* Every action names itself and files itself — twice now, once for each
     * half of its row — and no two share any of the three. A pad file key that
     * collided with a keyboard one would be the two halves of one action
     * overwriting each other on load. */
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        const char *label = keybind_action_label((BindAction)a);
        const char *key = keybind_action_file_key((BindAction)a);
        const char *pad_key = keybind_action_pad_file_key((BindAction)a);
        CHECK(label[0] != '\0');
        CHECK(key[0] != '\0');
        CHECK(pad_key[0] != '\0');
        CHECK(strcmp(key, pad_key) != 0);
        for (int b = a + 1; b < BIND_COUNT; ++b)
        {
            CHECK(strcmp(label, keybind_action_label((BindAction)b)) != 0);
            CHECK(strcmp(key, keybind_action_file_key((BindAction)b)) != 0);
            CHECK(strcmp(pad_key,
                         keybind_action_pad_file_key((BindAction)b)) != 0);
        }
        /* And no pad key may be a prefix of a keyboard one or the other way
         * round: `line_key_is` matches on a prefix, so a pair like `bind_up`
         * and `bind_up_alt` is one line applied as the other. */
        for (int b = 0; b < BIND_COUNT; ++b)
        {
            const char *other = keybind_action_file_key((BindAction)b);
            CHECK(strncmp(pad_key, other, strlen(other)) != 0);
        }
    }
}

/*
 * The pad's half of the same rules, and the one rule it has of its own.
 *
 * That one is the important one: **a face binding is stored as a letter, never
 * as a position.** A Switch pad prints A where an Xbox pad prints B, which is
 * the whole reason [pad_hint.h](../src/pad_hint.h) exists, and a settings file
 * that kept the raw position a Switch player pressed would move their jump
 * button the day they plugged in an Xbox pad. The four canonical positions
 * stand for the four letters and nothing else may.
 */
static void test_every_bindable_button_fits_its_cap(void)
{
    static const int BUTTONS[] = {
#define CHUCK_PAD_CODE(ident, button, file_name, shown) button,
        CHUCK_PAD_LIST(CHUCK_PAD_CODE)
#undef CHUCK_PAD_CODE
    };
    static const char *const FILE_NAMES[] = {
#define CHUCK_PAD_FILE(ident, button, file_name, shown) file_name,
        CHUCK_PAD_LIST(CHUCK_PAD_FILE)
#undef CHUCK_PAD_FILE
    };
    static const char *const SHOWN[] = {
#define CHUCK_PAD_SHOWN(ident, button, file_name, shown) shown,
        CHUCK_PAD_LIST(CHUCK_PAD_SHOWN)
#undef CHUCK_PAD_SHOWN
    };
    const int count = (int)(sizeof(BUTTONS) / sizeof(BUTTONS[0]));
    CHECK(count > 0);

    for (int i = 0; i < count; ++i)
    {
        CHECK(strlen(FILE_NAMES[i]) > 0);
        /*
         * The width the cap is drawn at is measured against what a pad
         * actually *prints*, not against the template: `$A` is two characters
         * of source and one or two of ink. A template is expanded by
         * `pad_hint`, and the widest thing it can produce for a face is `[]`
         * or `/\` — two — while a plain name like `DP UP` is spelled as it
         * stands. So a `$`-form is measured at 2 and everything else at its
         * own length.
         */
        size_t shown_len = SHOWN[i][0] == '$' ? 2u : strlen(SHOWN[i]);
        CHECK(shown_len > 0);
        CHECK(shown_len <= PADBIND_NAME_MAX);

        CHECK(strcmp(keybind_pad_file_name(BUTTONS[i]), FILE_NAMES[i]) == 0);
        CHECK(keybind_pad_from_file_name(FILE_NAMES[i],
                                         strlen(FILE_NAMES[i])) == BUTTONS[i]);
        CHECK(keybind_pad_is_bindable(BUTTONS[i]));
        CHECK(strcmp(keybind_pad_name(BUTTONS[i]), SHOWN[i]) == 0);

        for (int j = i + 1; j < count; ++j)
        {
            CHECK(BUTTONS[i] != BUTTONS[j]);
            CHECK(strcmp(FILE_NAMES[i], FILE_NAMES[j]) != 0);
        }
    }

    /* START, BACK and GUIDE are off the list on purpose — they are the pad's
     * pause and its way back out of a sheet, exactly as ESC and BACKSPACE are
     * the keyboard's. A build that quietly made one of them bindable is a
     * build a player can lock themselves out of the options with. */
    CHECK(!keybind_pad_is_bindable(4)); /* BACK  */
    CHECK(!keybind_pad_is_bindable(5)); /* GUIDE */
    CHECK(!keybind_pad_is_bindable(6)); /* START */
    CHECK(!keybind_pad_is_bindable(PADBIND_NONE));
    CHECK(keybind_pad_from_file_name("NOPE", 4) == PADBIND_NONE);
    CHECK(keybind_pad_from_file_name(NULL, 0) == PADBIND_NONE);
    CHECK(keybind_pad_name(4242)[0] == '\0');

    /* The four letters, and only the four. */
    for (int i = 0; i < 4; ++i)
    {
        int button = keybind_pad_face_button(i);
        CHECK(button != PADBIND_NONE);
        CHECK(keybind_pad_face_index(button) == i);
        CHECK(keybind_pad_name(button)[0] == '$');
    }
    CHECK(keybind_pad_face_button(-1) == PADBIND_NONE);
    CHECK(keybind_pad_face_button(4) == PADBIND_NONE);
    /* A bumper carries no letter and must not be translated as one, or a
     * Switch player's LB would come back as a face. */
    CHECK(keybind_pad_face_index(9) < 0);
    CHECK(keybind_pad_face_index(11) < 0);
}

/*
 * Every word on the options sheet, measured against the plate it is drawn on.
 *
 * This is the check the sheet never had, and it was overdue: the rule in
 * AGENTS.md is that a table of words the player reads keeps the geometry it
 * has to fit as constants and gets a `make test` check measuring one against
 * the other — and it names the manual's control sheet, the report's intel line
 * and the credits as the places that learned it. The options sheet is a table
 * of words the player reads and it was not on the list. It was already over
 * the edge when this was written: the controls heading's detail line ran about
 * thirty pixels off the right of the plate, drawn by a renderer that neither
 * wraps nor clips, so the last few characters of the one sentence explaining
 * how to rebind anything were simply painted outside the sheet.
 *
 * Both indents are checked because the sheet has two: a heading and the strap
 * start at `SETTINGS_LABEL_X`, a row's own label and detail are pushed in past
 * the cursor gutter to `SETTINGS_ROW_TEXT_X`.
 */
/*
 * The two lines a finished run ends on, measured on the screen that draws them
 * biggest.
 *
 * The game-over card draws the score at double scale, so the card is the frame
 * that binds and the outro's copy of the same line fits it with room to spare.
 * Every shape either screen can produce is walked, including the assisted one —
 * which is the longest of the three, because it spends words where the other two
 * print a number.
 */
/*
 * The record card's cells, measured against the grid the sheet steps by.
 *
 * THE RECORD draws the seventeen per-sector bests in two columns, and the column
 * width is a constant in [run_tally.h](../src/run_tally.h) that the illustration reads
 * — so this is the check that keeps a cell inside its own column. Every sector is
 * walked because seventeen is two digits where nine would have been one, and both
 * a banked time and an unbanked one are asked, because they are different lengths
 * of nothing.
 */
static void test_the_record_card_cells_fit_their_column(void)
{
    const float clear = RUN_TALLY_SECTOR_CELL_W - RUN_TALLY_GLYPH_W * 2.0f;
    CHECK(clear > 0.0f);

    const float times[] = {PROGRESS_NO_TIME, 1.0f, 74.0f, 599.0f, 5999.0f,
                           99999.0f};
    for (size_t i = 0; i < sizeof(times) / sizeof(times[0]); ++i)
    {
        for (int sector = 0; sector < (int)EMBEDDED_LEVEL_COUNT; ++sector)
        {
            char cell[RUN_TALLY_SECTOR_MAX];
            int written = run_tally_format_sector_time(sector, times[i], cell,
                                                       sizeof(cell));
            CHECK(written > 0);
            CHECK((size_t)written == strlen(cell));
            CHECK((size_t)written < sizeof(cell) - 1);
            /* Inside its own column with a two-cell gutter, or the next column
             * is being written over. */
            CHECK((float)written * RUN_TALLY_GLYPH_W <= clear);

            /* The sector is named the way every other screen names it. */
            char expected[8];
            snprintf(expected, sizeof(expected), "%02d", sector + 1);
            CHECK(strncmp(cell, expected, strlen(expected)) == 0);

            /* An unfinished sector reads as absent rather than as perfect, which
             * is the whole reason `PROGRESS_NO_TIME` is not just nought. */
            if (times[i] <= PROGRESS_NO_TIME)
                CHECK(strstr(cell, "--:--") != NULL);
            else
                CHECK(strstr(cell, "--:--") == NULL);
        }
    }

    /* A time longer than the file will keep still prints in five characters, so
     * a hand-edited progress file cannot widen the card. */
    char cell[RUN_TALLY_SECTOR_MAX];
    CHECK(run_tally_format_sector_time(0, 999999.0f, cell, sizeof(cell)) > 0);
    CHECK(strstr(cell, "99:59") != NULL);
    CHECK(run_tally_format_sector_time(-1, 74.0f, cell, sizeof(cell)) == 0);

    /* And the campaign fits the card: two columns of `per_column` is what the
     * illustration lays out, so a campaign long enough to need a third would run
     * off the plate. */
    CHECK((int)EMBEDDED_LEVEL_COUNT == CAMPAIGN_SECTORS);
    CHECK(CAMPAIGN_SECTORS <= PROGRESS_MAX_TRACKED_SECTORS);
}

static void test_the_run_tally_fits_the_frame_it_is_drawn_in(void)
{
    const float column = RUN_TALLY_TEXT_RIGHT - RUN_TALLY_TEXT_LEFT;
    CHECK(column > 0.0f);

    const struct
    {
        int score;
        int best_score;
        bool assisted;
    } score_shapes[] = {
        /* Both figures at the ceiling `Progress` will store, which is what
         * `run_tally_format_score` clamps to and therefore the widest line the
         * game can produce. */
        {PROGRESS_MAX_SCORE, PROGRESS_MAX_SCORE, false},
        {PROGRESS_MAX_SCORE, 0, false},
        {0, PROGRESS_MAX_SCORE, false},
        {PROGRESS_MAX_SCORE, PROGRESS_MAX_SCORE, true},
        /* And a run somebody actually had, so a shape nobody reaches is not the
         * only thing holding the frame. */
        {24680, 31500, false},
        {0, 0, false},
    };

    for (size_t i = 0; i < sizeof(score_shapes) / sizeof(score_shapes[0]); ++i)
    {
        char line[RUN_TALLY_MAX];
        int written = run_tally_format_score(score_shapes[i].score,
                                            score_shapes[i].best_score,
                                            score_shapes[i].assisted, line,
                                            sizeof(line));
        CHECK(written > 0);
        CHECK((size_t)written == strlen(line));
        /* Room left in the buffer, or the line above is one that was cut rather
         * than one that fitted. */
        CHECK((size_t)written < sizeof(line) - 1);
        CHECK((float)written * RUN_TALLY_GLYPH_W * RUN_TALLY_CARD_SCALE <=
              column);
        /* An assisted run never quotes a record: there is no ladder it is on,
         * and printing one would compare it to a game it did not play. */
        if (score_shapes[i].assisted)
            CHECK(strstr(line, "BEST") == NULL);
    }

    const struct
    {
        int sheets;
        int best;
        bool assisted;
        bool drawn;
    } docket_shapes[] = {
        {PROGRESS_MAX_EVIDENCE, PROGRESS_MAX_EVIDENCE, false, true},
        {0, PROGRESS_MAX_EVIDENCE, false, true},
        {PROGRESS_MAX_EVIDENCE, 0, false, true},
        {PROGRESS_MAX_EVIDENCE, PROGRESS_MAX_EVIDENCE, true, true},
        {12, 9, false, true},
        /* Nothing tonight and nothing ever: the docket has not entered this
         * player's game and neither screen draws a line about it. An assisted
         * run with no sheets is the same answer, because the best it would
         * otherwise quote belongs to runs it is not being measured against. */
        {0, 0, false, false},
        {0, 12, true, false},
    };

    for (size_t i = 0; i < sizeof(docket_shapes) / sizeof(docket_shapes[0]);
         ++i)
    {
        char line[RUN_TALLY_MAX];
        memset(line, 'x', sizeof(line));
        int written = run_tally_format_docket(docket_shapes[i].sheets,
                                             docket_shapes[i].best,
                                             docket_shapes[i].assisted, line,
                                             sizeof(line));
        if (!docket_shapes[i].drawn)
        {
            CHECK(written == 0);
            /* Emptied rather than left as the stack, so a caller that draws
             * unconditionally draws nothing. */
            CHECK(line[0] == '\0');
            continue;
        }
        CHECK(written > 0);
        CHECK((size_t)written == strlen(line));
        CHECK((size_t)written < sizeof(line) - 1);
        CHECK((float)written * RUN_TALLY_GLYPH_W <= column);
        if (docket_shapes[i].assisted)
            CHECK(strstr(line, "BEST") == NULL);
    }

    /* A nought-capacity buffer is refused rather than written to, which is the
     * one thing every formatter in this tree promises. */
    CHECK(run_tally_format_score(10, 10, false, NULL, 0) == 0);
    CHECK(run_tally_format_docket(10, 10, false, NULL, 0) == 0);
}

/*
 * An assisted run banks nothing, and one sector of assist is enough.
 *
 * The switches take effect the frame they are flipped, so the question "was this
 * run assisted" cannot be answered by looking at the sheet when the run ends —
 * `CampaignState.assisted` is sticky and this is what says so. The gate itself is
 * one function, because the shell has four `progress_note_*` calls and three of
 * them have to agree about it.
 */
static void test_an_assisted_run_banks_no_records(void)
{
    CampaignState campaign;
    campaign_reset(&campaign, false);
    CHECK(!campaign.assisted);
    CHECK(campaign_records_count(&campaign));

    /* Off stays off, however many times it is asked. */
    campaign_note_assist(&campaign, false);
    CHECK(campaign_records_count(&campaign));

    /* One sector with a switch on, then the player turns it back off: the run is
     * assisted for the rest of the night. */
    campaign_note_assist(&campaign, true);
    CHECK(campaign.assisted);
    campaign_note_assist(&campaign, false);
    CHECK(campaign.assisted);
    CHECK(!campaign_records_count(&campaign));

    /* And a new run starts clean, because that is what `campaign_reset` is. */
    campaign_reset(&campaign, false);
    CHECK(campaign_records_count(&campaign));

    /* Veteran is the same lever the other way and does not touch it: a harder
     * run has no reason to be kept off the ladder it is beating. */
    campaign_reset(&campaign, true);
    CHECK(campaign_records_count(&campaign));
    CHECK(campaign.lives == VETERAN_LIVES);

    /* The sheet's own answer, which is what the shell hands to the flag. A
     * fourth assist switch is one line in settings.c rather than a fourth term
     * three call sites have to remember. */
    Settings settings;
    settings_defaults(&settings);
    CHECK(!settings_assist_any(&settings));
    settings.challenge.veteran = true;
    CHECK(!settings_assist_any(&settings));
    settings.assist.more_hearts = true;
    CHECK(settings_assist_any(&settings));
    settings.assist.more_hearts = false;
    settings.assist.slower_guards = true;
    CHECK(settings_assist_any(&settings));
    settings.assist.slower_guards = false;
    settings.assist.infinite_lives = true;
    CHECK(settings_assist_any(&settings));
    CHECK(!settings_assist_any(NULL));
}

/*
 * Clearing the records keeps the resume.
 *
 * The one row on the options sheet whose action cannot be undone, so the split it
 * makes is worth pinning: the three ratchets a run competes on go, and
 * `furthest_sector` — which is the title screen's resume chip, not a record —
 * stays. Answering "my times are polluted" by also throwing away the campaign
 * the player is in the middle of would be a worse bug than the one the row
 * exists to fix.
 */
static void test_clearing_the_records_keeps_the_resume(void)
{
    Progress progress;
    progress_defaults(&progress);
    CHECK(progress_note_score(&progress, 4200));
    CHECK(progress_note_evidence(&progress, 7));
    CHECK(progress_note_sector_time(&progress, 3, 61.0f));
    CHECK(progress_note_sector(&progress, 8));

    progress_clear_records(&progress);

    CHECK(progress.best_score == 0);
    CHECK(progress.best_evidence == 0);
    for (int i = 0; i < PROGRESS_MAX_TRACKED_SECTORS; ++i)
        CHECK(progress_sector_time(&progress, i) == PROGRESS_NO_TIME);
    CHECK(progress.furthest_sector == 8);

    /* And the ratchets are ratchets again afterwards: the first clear of a
     * sector counts, which is the one way the time differs from the other two. */
    CHECK(progress_note_sector_time(&progress, 3, 120.0f));
    CHECK(progress_note_score(&progress, 1));

    progress_clear_records(NULL); /* must not fault */
}

/*
 * The widest a `pad_hint` template can ever draw, expanded rather than argued.
 *
 * Both sheets carry a footer written as a template — `$A`, `$B`, `$START` — and
 * both used to be measured by counting the *template's* characters, on the
 * argument that a two-character token stands for at most two glyphs. That is
 * true of `$A` through `$Y`, whose widest spellings are a PlayStation's `[]` and
 * `/\`, and it is false of `$START`: six characters standing for `OPTIONS`,
 * which is seven. So the ceiling was right for the tokens in use and wrong for
 * one that could be added at any time, with a comment beside it promising the
 * check covered whatever was written there.
 *
 * Expanding it properly needs `pad_hint` itself, which is why this could not be
 * done before: [pad_hint.c](../src/pad_hint.c) linked SDL and the suite links
 * none of it. It is on this side of the line now, so the ceiling is measured by
 * asking the same function the renderer asks, with every field set to the widest
 * spelling any pad in `read_named_buttons` can produce.
 */
static const PadHints *widest_pad_spelling(void)
{
    static PadHints widest;
    widest = PAD_HINTS_XBOX;
    /* A PlayStation's square and triangle: two glyphs where Xbox has one. */
    widest.face[PAD_FACE_CONFIRM] = "X";
    widest.face[PAD_FACE_CANCEL] = "O";
    widest.face[PAD_FACE_ATTACK] = "[]";
    widest.face[PAD_FACE_DOOR] = "/\\";
    widest.start = "OPTIONS";  /* PS4 and PS5 */
    widest.select = "SELECT";  /* PS3; CREATE on a PS5 is the same width */
    widest.shoulder_l = "L1";
    widest.shoulder_r = "R1";
    return &widest;
}

/* One template, measured at that widest expansion against a given column. */
static bool pad_template_fits(const char *pad_form, const char *key_form,
                              float glyph_w, float scale, float room)
{
    char spelled[160];
    pad_hint(widest_pad_spelling(), spelled, sizeof(spelled), pad_form,
             key_form);
    return (float)strlen(spelled) * glyph_w * scale <= room &&
           (float)strlen(key_form) * glyph_w * scale <= room;
}

static void test_every_word_on_the_options_sheet_fits_the_plate(void)
{
    const float right = SETTINGS_PANEL_W - SETTINGS_LABEL_X;

    const SettingsPage pages[] = {SETTINGS_PAGE_MAIN, SETTINGS_PAGE_CONTROLS};
    for (int p = 0; p < 2; ++p)
    {
        SettingsPage page = pages[p];

        const char *strap = settings_page_strap(page);
        CHECK(strap != NULL && strap[0] != '\0');
        CHECK(SETTINGS_LABEL_X + (float)strlen(strap) * SETTINGS_GLYPH_W <=
              right);

        /* The title is the one thing drawn at double scale. */
        const char *title = settings_page_title(page);
        CHECK(title != NULL && title[0] != '\0');
        CHECK(SETTINGS_LABEL_X +
                  (float)strlen(title) * SETTINGS_GLYPH_W * 2.0f <= right);

        int count = 0;
        const SettingRow *rows = settings_rows(page, &count);
        REQUIRE(rows != NULL);
        for (int i = 0; i < count; ++i)
        {
            bool heading = rows[i].kind == SETTING_ROW_HEADING;
            float x = heading ? SETTINGS_LABEL_X : SETTINGS_ROW_TEXT_X;

            CHECK(rows[i].label != NULL && rows[i].label[0] != '\0');
            CHECK(x + (float)strlen(rows[i].label) * SETTINGS_GLYPH_W <= right);

            if (rows[i].detail == NULL)
                continue;
            CHECK(rows[i].detail[0] != '\0');
            CHECK(x + (float)strlen(rows[i].detail) * SETTINGS_GLYPH_W <=
                  right);
        }
    }

    /* And the three lines the sheet draws that are not rows of the table: the
     * two that tell an armed cap what it is waiting for, and the mute warning,
     * which is a correction to the audio heading rather than a description of
     * it. They are on the same plate and just as able to run off it. */
    static const char *const LOOSE[] = {
        SETTINGS_CAPTURE_KEY_LINE,
        SETTINGS_CAPTURE_PAD_LINE,
        SETTINGS_MUTED_LINE,
        /* The records row swaps its detail for this once it has been pressed
         * once, so it is drawn at a row's indent rather than the heading's —
         * measured from `SETTINGS_LABEL_X` with the rest of them, which is the
         * stricter of the two and therefore the safe one to hold it to. */
        SETTINGS_RECORDS_ARMED_DETAIL,
    };
    for (size_t i = 0; i < sizeof(LOOSE) / sizeof(LOOSE[0]); ++i)
    {
        CHECK(LOOSE[i][0] != '\0');
        CHECK(SETTINGS_LABEL_X + (float)strlen(LOOSE[i]) * SETTINGS_GLYPH_W <=
              right);
    }

    /*
     * And the footer, which is the widest line the sheet draws and was the last
     * one nothing measured.
     *
     * It sat as a literal inside `draw_settings_menu` while every other word
     * here had been moved out to be measured — the strap, the title, every label
     * and detail, the two capture lines, the mute warning — and the comment
     * beside it said the capture lines were measured, which was true and read as
     * though it covered the whole block. The line it left out is the one naming
     * the way out of the sheet, at 446px of a 490px column: 44px of air, less
     * than anything else on the plate. The pause sheet has measured its own
     * footer in both alphabets since that table was split out, so the two sheets
     * simply disagreed about whether a footer is words a player reads.
     */
    CHECK(pad_template_fits(SETTINGS_FOOTER_PAD_LINE, SETTINGS_FOOTER_KEY_LINE,
                            SETTINGS_GLYPH_W, 1.0f,
                            SETTINGS_PANEL_W - 2.0f * SETTINGS_LABEL_X));
}

/*
 * A binding row has to fit the plate it is drawn on, and now that it carries
 * four caps instead of two, something has to say so.
 *
 * This is the manual's control sheet all over again: that page lost its last
 * line — the only line in the game naming the key that closes it — because the
 * words and the frame they had to live in were two separate facts and nothing
 * compared them. A row here is a label on the left and a run of caps on the
 * right, and the failure mode is exactly as quiet: the caps grow leftwards, so
 * the first thing a fifth one does is draw over the end of `USE DOOR / HACK`.
 */
/*
 * The pause menu, held to its own plate.
 *
 * It is the sixth table of words in this game and the second one nobody had
 * noticed was one: three rows the player reads, a title, a strap and a footer
 * prompt, with its labels and its plate width written as literals inside
 * `draw_pause_menu`. Every other sheet — the crew's net, the credits roll, the
 * report, the manual's pages, the options sheet — has a check like this one,
 * and each of them was written because a line had already been lost or was
 * about to be.
 *
 * This one is different in the one way worth recording: measured at the point
 * the table was split out, every line already fitted, with the widest — `GIVE UP
 * THIS RUN AND RETURN TO THE TITLE` at 320px inside a 420px plate — leaving 60px
 * of air. So it is not paying off a bug. It is what will notice the fourth row,
 * or the day `ABANDON RUN` grows a clause.
 *
 * The footer is measured in both alphabets, because `pad_hint` expands `$A` and
 * `$B` to up to two glyphs each and the longer of the two forms is not
 * necessarily the one that looks longer in the header.
 */
static void test_every_word_on_the_pause_sheet_fits_the_plate(void)
{
    /* The table, the enum and the count are one list — see PAUSE_ROWS. */
    CHECK(pause_sheet_row_count() == PAUSE_ITEM_COUNT);

    /* A row's text is indented past the cursor caret; the title, the strap and
     * the footer sit at the plate's own margin. Both are measured against the
     * same right-hand edge, which is the plate less that margin again. */
    const float row_room =
        PAUSE_PANEL_W - PAUSE_ROW_TEXT_X - PAUSE_LABEL_X;
    const float plate_room = PAUSE_PANEL_W - 2.0f * PAUSE_LABEL_X;

    for (int i = 0; i < PAUSE_ITEM_COUNT; ++i)
    {
        const PauseRow *row = &PAUSE_SHEET_ROWS[i];
        REQUIRE(row->label != NULL && row->detail != NULL);
        CHECK(row->label[0] != '\0');
        CHECK(row->detail[0] != '\0');
        CHECK((float)strlen(row->label) * PAUSE_GLYPH_W * PAUSE_LABEL_SCALE <=
              row_room);
        CHECK((float)strlen(row->detail) * PAUSE_GLYPH_W * PAUSE_DETAIL_SCALE <=
              row_room);
    }

    /* The armed warning is a fourth detail line, drawn in the ABANDON row's own
     * place and therefore held to the row's room rather than the plate's. It is
     * measured here because it is the one line on this sheet the player only
     * sees when something is about to be spent: a warning that runs off the
     * plate is a warning nobody reads. See `PAUSE_ABANDON_ARMED`. */
    CHECK(PAUSE_ABANDON_ARMED[0] != '\0');
    CHECK((float)strlen(PAUSE_ABANDON_ARMED) * PAUSE_GLYPH_W *
              PAUSE_DETAIL_SCALE <= row_room);

    CHECK((float)strlen(PAUSE_TITLE) * PAUSE_GLYPH_W * PAUSE_LABEL_SCALE <=
          plate_room);
    CHECK((float)strlen(PAUSE_STRAP) * PAUSE_GLYPH_W * PAUSE_DETAIL_SCALE <=
          plate_room);

    /*
     * The keyboard form as written, and the pad form actually expanded.
     *
     * This counted the template's own characters and argued that a
     * two-character token stands for at most two glyphs, so the template was
     * already the ceiling — and claimed that made "a token added to the line
     * covered without this check having to know how a PlayStation spells a
     * circle". The claim was the wrong half of true: it holds for `$A` through
     * `$Y`, and `$START` is six characters standing for `OPTIONS`, which is
     * seven. Add that token to this line and the ceiling silently stops being
     * one, with a comment promising otherwise.
     *
     * `pad_hint` is reachable from the suite now, so the line is spelled at its
     * widest instead of estimated. See `widest_pad_spelling`.
     */
    CHECK(pad_template_fits(PAUSE_HINT_PAD, PAUSE_HINT_KEYS, PAUSE_GLYPH_W,
                            PAUSE_DETAIL_SCALE, plate_room));

    /* And the plate is tall enough for the rows it lists plus the footer the
     * prompt is drawn into — the one dimension a width check cannot see. */
    float panel_h = PAUSE_ROWS_TOP +
                    PAUSE_ROW_H * (float)PAUSE_ITEM_COUNT + PAUSE_ROW_FOOT;
    float last_row_bottom =
        PAUSE_ROWS_TOP + PAUSE_ROW_H * (float)(PAUSE_ITEM_COUNT - 1) + 20.0f +
        PAUSE_GLYPH_W * PAUSE_DETAIL_SCALE;
    CHECK(last_row_bottom < panel_h - 26.0f);
}

static void test_a_binding_row_fits_the_plate(void)
{
    const float cap_w =
        (float)KEYBIND_NAME_MAX * SETTINGS_GLYPH_W + SETTINGS_CAP_PAD;
    const float pad_cap_w =
        (float)PADBIND_NAME_MAX * SETTINGS_GLYPH_W + SETTINGS_CAP_PAD;

    /* The run, laid out exactly as `draw_setting_keys` lays it out. */
    float keys_w = (float)BIND_SLOTS * cap_w +
                   (float)(BIND_SLOTS - 1) * SETTINGS_CAP_GAP;
    float pads_w = (float)BIND_SLOTS * pad_cap_w +
                   (float)(BIND_SLOTS - 1) * SETTINGS_CAP_GAP;
    float caps_w = keys_w + SETTINGS_CAP_GROUP_GAP + pads_w;

    float caps_left = SETTINGS_PANEL_W - SETTINGS_CONTROL_INSET - caps_w;

    /* The longest label on the sheet, whichever row that turns out to be. */
    size_t widest = 0;
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        size_t len = strlen(keybind_action_label((BindAction)a));
        if (len > widest)
            widest = len;
    }
    CHECK(widest > 0);
    float label_right = SETTINGS_LABEL_X + (float)widest * SETTINGS_GLYPH_W;

    /* Not merely "does not overlap": a clear gap, or the next thing added to
     * either end is the one that starts the collision. */
    CHECK(caps_left > label_right + SETTINGS_CAP_GAP);
    /* And the run stays on the plate at both ends. */
    CHECK(caps_left > SETTINGS_LABEL_X);
    CHECK(caps_left + caps_w <= SETTINGS_PANEL_W - SETTINGS_CONTROL_INSET);
}

/*
 * The sectors whose way out cannot be reached without stepping through a door
 * pair, 1-based.
 *
 * This list is why `keybind_set` refuses a bind that would leave an action with
 * no key: a door pair is opened by `gameplay_use_door` and by nothing else, so
 * on these sectors an emptied USE is a run that cannot be finished — and the
 * sheet used to be able to empty USE in one press, by putting anything at all
 * on `E`. The rule is general; this is the shipped campaign's own proof that it
 * is load-bearing rather than tidy.
 *
 * It is written down twice on purpose, exactly as `INTEL_ARC_SECTORS` is: the
 * route model answers the same question off the maps, and a map edit that adds
 * or removes a door dependency fails the build with a sector number rather than
 * quietly changing what a rebind can cost.
 */
static const int DOOR_LOCKED_SECTORS[] = {14};

static bool sector_way_out_is_reachable(Level *level, bool with_doors)
{
    static RouteMap route;
    int doors = level->map.door_count;
    if (!with_doors)
        level->map.door_count = 0;
    route_map_init(&route, level);
    route_flood(&route, route_player_start(&route));
    int col = level->map.has_window ? level->map.window_col
                                    : level->map.exit_col;
    int row = level->map.has_window ? level->map.window_row
                                    : level->map.exit_row;
    bool reached = route_reaches(&route, col, row);
    level->map.door_count = doors;
    return reached;
}

/*
 * No sector is locked behind an action the sheet can switch off.
 *
 * Two halves, and the first is the invariant: from the defaults, *every*
 * accepted rebind of every key onto every slot of every action has to leave all
 * nine actions still answering something. The bind that cannot manage it is
 * refused instead, so there is no sequence of presses on the controls page that
 * ends with a verb the player no longer has.
 *
 * The second half is why that matters, measured off the maps rather than
 * asserted in a comment: sector 14's window is on the far side of a door pair,
 * so USE is not a convenience there, it is the sector.
 */
static void test_no_sector_is_locked_behind_an_unbindable_action(void)
{
    KeyBindings defaults;
    keybind_defaults(&defaults);

    /* Every key the defaults hold, which is every key a collision can be made
     * out of without inventing one the player has not got. */
    int codes[BIND_COUNT * BIND_SLOTS];
    int code_count = 0;
    for (int a = 0; a < BIND_COUNT; ++a)
        for (int s = 0; s < BIND_SLOTS; ++s)
            if (defaults.keys[a][s] != KEYBIND_NONE)
                codes[code_count++] = defaults.keys[a][s];
    CHECK(code_count > 0);

    int accepted = 0;
    int refused = 0;
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        for (int slot = 0; slot < BIND_SLOTS; ++slot)
        {
            for (int i = 0; i < code_count; ++i)
            {
                KeyBindings b = defaults;
                if (keybind_set(&b, (BindAction)a, slot, codes[i]))
                {
                    ++accepted;
                    for (int other = 0; other < BIND_COUNT; ++other)
                    {
                        bool answers = false;
                        for (int s = 0; s < BIND_SLOTS; ++s)
                            answers = answers || b.keys[other][s] != KEYBIND_NONE;
                        CHECK(answers);
                    }
                }
                else
                {
                    ++refused;
                    /* A refusal changes nothing at all. */
                    CHECK(memcmp(&b, &defaults, sizeof(b)) == 0);
                }
            }
        }
    }
    /* Both outcomes really happened, or the sweep has drifted into testing one
     * branch and reporting on two. */
    CHECK(accepted > 0);
    CHECK(refused > 0);

    /* The same sweep on the other table. */
    int pad_accepted = 0;
    int pad_refused = 0;
    int buttons[BIND_COUNT * BIND_SLOTS];
    int button_count = 0;
    for (int a = 0; a < BIND_COUNT; ++a)
        for (int s = 0; s < BIND_SLOTS; ++s)
            if (defaults.pad[a][s] != PADBIND_NONE)
                buttons[button_count++] = defaults.pad[a][s];
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        for (int slot = 0; slot < BIND_SLOTS; ++slot)
        {
            for (int i = 0; i < button_count; ++i)
            {
                KeyBindings b = defaults;
                if (keybind_set_pad(&b, (BindAction)a, slot, buttons[i]))
                {
                    ++pad_accepted;
                    for (int other = 0; other < BIND_COUNT; ++other)
                    {
                        bool answers = false;
                        for (int s = 0; s < BIND_SLOTS; ++s)
                            answers = answers || b.pad[other][s] != PADBIND_NONE;
                        CHECK(answers);
                    }
                }
                else
                {
                    ++pad_refused;
                    CHECK(memcmp(&b, &defaults, sizeof(b)) == 0);
                }
            }
        }
    }
    CHECK(pad_accepted > 0);
    CHECK(pad_refused > 0);

    /* And the maps, which are what makes the rule worth having. */
    static Level level;
    size_t listed = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Rng rng;
        rng_seed(&rng, 4100 + i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        if (level.map.mode == LEVEL_MODE_FACADE)
            continue;

        /* Whatever else is true of a sector, the way out has to be reachable
         * with everything the player has. */
        CHECK(sector_way_out_is_reachable(&level, true));

        bool needs_a_door = !sector_way_out_is_reachable(&level, false);
        bool on_the_list = false;
        for (size_t k = 0; k < sizeof(DOOR_LOCKED_SECTORS) / sizeof(DOOR_LOCKED_SECTORS[0]); ++k)
            on_the_list = on_the_list || DOOR_LOCKED_SECTORS[k] == (int)i + 1;
        CHECK(needs_a_door == on_the_list);
        listed += on_the_list;
    }
    CHECK(listed == sizeof(DOOR_LOCKED_SECTORS) / sizeof(DOOR_LOCKED_SECTORS[0]));
}

/*
 * One button does one job, the pad half — the same rule `keybind_set` keeps,
 * and it has to be kept by the same mechanism or it is not the same rule.
 */
static void test_pad_buttons_keep_one_button_to_one_job(void)
{
    KeyBindings bindings;
    keybind_defaults(&bindings);

    /* The shipped layout is what the game had welded into its source before
     * any of this was a table, and the defaults have to reproduce it exactly:
     * a "rebindable pad" that quietly moved a button on first launch would be
     * a regression wearing a feature's clothes. */
    CHECK(keybind_action_has_pad(&bindings, BIND_JUMP, 0));   /* A  */
    CHECK(keybind_action_has_pad(&bindings, BIND_SHOOT, 2));  /* X  */
    CHECK(keybind_action_has_pad(&bindings, BIND_SHOOT, 1));  /* B  */
    CHECK(keybind_action_has_pad(&bindings, BIND_USE, 3));    /* Y  */
    CHECK(keybind_action_has_pad(&bindings, BIND_WEAPON_NEXT, 10));
    CHECK(keybind_action_has_pad(&bindings, BIND_WEAPON_PREV, 9));
    CHECK(keybind_action_has_pad(&bindings, BIND_LEFT, 13));
    CHECK(keybind_action_has_pad(&bindings, BIND_RIGHT, 14));
    CHECK(keybind_action_has_pad(&bindings, BIND_UP, 11));
    CHECK(keybind_action_has_pad(&bindings, BIND_DOWN, 12));

    /* Taking a button from another action swaps it there, exactly as the
     * keyboard does: USE keeps a button rather than being left with none. */
    CHECK(keybind_set_pad(&bindings, BIND_JUMP, 0, 3)); /* jump onto Y */
    CHECK(keybind_action_has_pad(&bindings, BIND_JUMP, 3));
    CHECK(!keybind_action_has_pad(&bindings, BIND_USE, 3));
    CHECK(keybind_action_has_pad(&bindings, BIND_USE, 0)); /* the jump's A */

    /* And from the other slot of the same action, which is the case that
     * leaves an action answering one button twice and a slot short. Both
     * buttons survive it; only their slots change places. */
    CHECK(keybind_set_pad(&bindings, BIND_SHOOT, 0, 1)); /* X slot takes B */
    CHECK(keybind_action_has_pad(&bindings, BIND_SHOOT, 1));
    CHECK(bindings.pad[BIND_SHOOT][0] == 1);
    CHECK(bindings.pad[BIND_SHOOT][1] == 2);

    /* A pad bind that would empty an action is refused, the keyboard's rule on
     * the other table: the d-pad's left is MOVE LEFT's only button, so putting
     * it in a free slot of another action is a pad that has silently lost a
     * direction. */
    {
        KeyBindings pad_before = bindings;
        CHECK(!keybind_set_pad(&bindings, BIND_WEAPON_NEXT, 1, 13));
        CHECK(memcmp(&pad_before, &bindings, sizeof(bindings)) == 0);
    }

    /* No button is ever on two actions at once. */
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        for (int s = 0; s < BIND_SLOTS; ++s)
        {
            int button = bindings.pad[a][s];
            if (button == PADBIND_NONE)
                continue;
            for (int b = 0; b < BIND_COUNT; ++b)
            {
                for (int t = 0; t < BIND_SLOTS; ++t)
                {
                    if (a == b && s == t)
                        continue;
                    CHECK(bindings.pad[b][t] != button);
                }
            }
        }
    }

    /* Refusals change nothing. */
    KeyBindings before = bindings;
    CHECK(!keybind_set_pad(&bindings, BIND_JUMP, 0, 6));  /* START     */
    CHECK(!keybind_set_pad(&bindings, BIND_JUMP, -1, 0)); /* bad slot  */
    CHECK(!keybind_set_pad(&bindings, BIND_JUMP, BIND_SLOTS, 0));
    CHECK(!keybind_set_pad(&bindings, BIND_COUNT, 0, 0)); /* bad action */
    CHECK(!keybind_set_pad(NULL, BIND_JUMP, 0, 0));
    CHECK(memcmp(&before, &bindings, sizeof(bindings)) == 0);

    CHECK(!keybind_action_has_pad(&bindings, BIND_JUMP, PADBIND_NONE));
    CHECK(!keybind_action_has_pad(NULL, BIND_JUMP, 0));
}

/*
 * The letter on the button, which nothing in this suite could reach until now.
 *
 * [pad_hint.c](../src/pad_hint.c) linked SDL, so it sat on the far side of the
 * one boundary this codebase says is its most important, and `make coverage`
 * duly listed four of its functions as never executed — the four that decide
 * which letter goes on which button, which is the whole of the fix that file
 * exists for. The bug it was written to stop is not a subtle one: a Switch pad
 * prints A where an Xbox pad prints B, and the title screen asked for A while
 * the button printed A quit the game.
 *
 * Nothing about that decision needs a gamepad. It needs *what a gamepad said* —
 * four label numbers and a type number — so the decision is now `pad_hints_apply`
 * on this side of the line, the asking stays in `game_input.c`, and the numbers
 * are held to `SDL_GAMEPAD_BUTTON_LABEL_*` and `SDL_GAMEPAD_TYPE_*` there by one
 * `_Static_assert` per row. That is [keybind.c](../src/keybind.c)'s arrangement
 * exactly, for the same reason and with the same guarantee.
 */
static void test_a_pad_is_read_by_the_letter_not_the_position(void)
{
    PadHints hints;

    /* No pad: the Xbox letters in the Xbox places, which is also what an
     * unplugged pad leaves behind and what the manual's tables are written in. */
    pad_hints_apply(&hints, PAD_TYPE_UNKNOWN, NULL, 0);
    CHECK(strcmp(hints.face[PAD_FACE_CONFIRM], "A") == 0);
    CHECK(strcmp(hints.face[PAD_FACE_CANCEL], "B") == 0);
    CHECK(hints.at[PAD_FACE_CONFIRM] == PAD_BUTTON_SOUTH);
    CHECK(hints.at[PAD_FACE_CANCEL] == PAD_BUTTON_EAST);
    CHECK(strcmp(hints.start, "START") == 0);
    CHECK(strcmp(hints.select, "BACK") == 0);

    /*
     * The Nintendo swap, which is the bug in one assertion.
     *
     * A Switch pad prints A on the *east* button and B on the *south* one, the
     * other way round from an Xbox pad. Confirm must follow the letter, so it
     * must come out on EAST — and if this ever reads SOUTH again, the button
     * under the thumb that the title screen asked for is the one that quits.
     */
    static const PadButtonLabel switch_labels[] = {
        PAD_LABEL_B,  /* south */
        PAD_LABEL_A,  /* east  */
        PAD_LABEL_Y,  /* west  */
        PAD_LABEL_X}; /* north */
    pad_hints_apply(&hints, PAD_TYPE_NINTENDO_SWITCH_PRO, switch_labels,
                    PAD_FACE_COUNT);
    CHECK(strcmp(hints.face[PAD_FACE_CONFIRM], "A") == 0);
    CHECK(hints.at[PAD_FACE_CONFIRM] == PAD_BUTTON_EAST);
    CHECK(hints.at[PAD_FACE_CANCEL] == PAD_BUTTON_SOUTH);
    CHECK(hints.at[PAD_FACE_ATTACK] == PAD_BUTTON_NORTH); /* X is north here */
    CHECK(hints.at[PAD_FACE_DOOR] == PAD_BUTTON_WEST);
    /* And the buttons that only ever change name. */
    CHECK(strcmp(hints.start, "+") == 0);
    CHECK(strcmp(hints.select, "-") == 0);

    /*
     * A PlayStation pad needs no swap, only a spelling: cross, circle, square
     * and triangle already sit where A, B, X and Y do. The shapes are written
     * in ASCII because every prompt goes through `SDL_RenderDebugText`, whose
     * 8x8 font has no others.
     */
    static const PadButtonLabel sony_labels[] = {
        PAD_LABEL_CROSS, PAD_LABEL_CIRCLE, PAD_LABEL_SQUARE,
        PAD_LABEL_TRIANGLE};
    pad_hints_apply(&hints, PAD_TYPE_PS5, sony_labels, PAD_FACE_COUNT);
    CHECK(hints.at[PAD_FACE_CONFIRM] == PAD_BUTTON_SOUTH);
    CHECK(strcmp(hints.face[PAD_FACE_CONFIRM], "X") == 0);
    CHECK(strcmp(hints.face[PAD_FACE_CANCEL], "O") == 0);
    CHECK(strcmp(hints.face[PAD_FACE_ATTACK], "[]") == 0);
    CHECK(strcmp(hints.face[PAD_FACE_DOOR], "/\\") == 0);
    CHECK(strcmp(hints.start, "OPTIONS") == 0);
    CHECK(strcmp(hints.select, "CREATE") == 0);
    CHECK(strcmp(hints.shoulder_l, "L1") == 0);

    /* A PS3 is the one Sony pad that still calls them START and SELECT, and it
     * is the row that separates "named after the type" from "named after the
     * shapes": same four glyphs as the PS5 above, different two words. */
    pad_hints_apply(&hints, PAD_TYPE_PS3, sony_labels, PAD_FACE_COUNT);
    CHECK(strcmp(hints.start, "START") == 0);
    CHECK(strcmp(hints.select, "SELECT") == 0);
    CHECK(strcmp(hints.shoulder_l, "L1") == 0);
    CHECK(strcmp(hints.face[PAD_FACE_DOOR], "/\\") == 0);

    /* A PS4 shares the PS5's OPTIONS and spells the other one differently
     * again, which is the whole reason these are three rows and not one. */
    pad_hints_apply(&hints, PAD_TYPE_PS4, sony_labels, PAD_FACE_COUNT);
    CHECK(strcmp(hints.start, "OPTIONS") == 0);
    CHECK(strcmp(hints.select, "SHARE") == 0);

    /* A type this file does not list — an Xbox 360 pad is 2 — takes the
     * fall-through and keeps the Xbox names. That branch is why
     * `read_named_buttons` has a `default` as well as a named row: the enum in
     * pad_hint.h is deliberately a subset of SDL's. */
    pad_hints_apply(&hints, (PadType)2, sony_labels, PAD_FACE_COUNT);
    CHECK(strcmp(hints.start, "START") == 0);
    CHECK(strcmp(hints.select, "BACK") == 0);
    CHECK(strcmp(hints.face[PAD_FACE_CONFIRM], "X") == 0);

    /*
     * Half a translation is worse than none: a pad that letters only some of its
     * buttons, or letters two of them the same, must fall back to the whole Xbox
     * set rather than a mixture nothing on screen agrees with.
     *
     * The staging matters, and getting it wrong is instructive. A partial set
     * that only fails to *move* a face — `{A, UNKNOWN, X, Y}` — is indetectable,
     * because the Xbox default it would have kept is the answer either way, so
     * the whole guard could be deleted and the test would pass. The set below
     * letters south as B instead, so committing it half-done leaves confirm on
     * the Xbox default and cancel on that same button: two letters, one thumb.
     * That is what a mixture actually looks like and it is what this asks about.
     */
    static const PadButtonLabel partial[] = {
        PAD_LABEL_B,        /* south, which would move cancel off east */
        PAD_LABEL_UNKNOWN,  /* east, unreadable — so nothing claims confirm */
        PAD_LABEL_X, PAD_LABEL_Y};
    pad_hints_apply(&hints, PAD_TYPE_UNKNOWN, partial, PAD_FACE_COUNT);
    CHECK(hints.at[PAD_FACE_CONFIRM] == PAD_BUTTON_SOUTH);
    CHECK(hints.at[PAD_FACE_CANCEL] == PAD_BUTTON_EAST);
    CHECK(strcmp(hints.face[PAD_FACE_CANCEL], "B") == 0);

    /* And the same for a pad that letters two buttons the same, which is the
     * other way `found` can come back short. */
    static const PadButtonLabel doubled[] = {
        PAD_LABEL_B, PAD_LABEL_B, PAD_LABEL_X, PAD_LABEL_Y};
    pad_hints_apply(&hints, PAD_TYPE_UNKNOWN, doubled, PAD_FACE_COUNT);
    CHECK(hints.at[PAD_FACE_CONFIRM] == PAD_BUTTON_SOUTH);
    CHECK(hints.at[PAD_FACE_CANCEL] == PAD_BUTTON_EAST);

    /* Whatever it fell back to, no two letters may share a button — that is the
     * property a mixture breaks, stated without naming which pad broke it. */
    for (int a = 0; a < PAD_FACE_COUNT; ++a)
        for (int b = a + 1; b < PAD_FACE_COUNT; ++b)
            CHECK(hints.at[a] != hints.at[b]);

    /* A short read is the same refusal — a caller that managed three questions
     * out of four has not read a pad. */
    pad_hints_apply(&hints, PAD_TYPE_UNKNOWN, switch_labels, 3);
    CHECK(hints.at[PAD_FACE_CONFIRM] == PAD_BUTTON_SOUTH);
    CHECK(hints.at[PAD_FACE_CANCEL] == PAD_BUTTON_EAST);
}

/* The round trip a binding takes: a letter to a button and back. */
/*
 * And what a stick says to a menu, which used to be six lines behind
 * `SDL_GetGamepadAxis`.
 *
 * `make coverage-shell` — the target that measures the half of the tree the
 * suite cannot link — found the whole gamepad path in `game_input.c` executed by
 * neither gate, and most of it genuinely needs a pad in a hand. This did not: it
 * needs the two numbers a pad reports, exactly as the letters above needed four
 * label numbers, which is the argument that file already makes about "it needs
 * hardware". The reading stayed with SDL; the decision came over here.
 *
 * It is worth a test rather than a move for its own sake, because it decides how
 * a menu feels under a thumb: a dead zone too small walks two rows on a resting
 * stick, and a diagonal that resolves sideways on a column of rows moves nothing
 * where the player pushed.
 */
static void test_the_stick_answers_a_menu_like_a_d_pad(void)
{
    /* Centred, and anything inside the dead zone, is not a push. */
    CHECK(pad_stick_direction(0, 0) == PAD_BUTTON_NONE);
    CHECK(pad_stick_direction(GAMEPAD_AXIS_DEAD_ZONE - 1,
                              GAMEPAD_AXIS_DEAD_ZONE - 1) == PAD_BUTTON_NONE);
    CHECK(pad_stick_direction(-(GAMEPAD_AXIS_DEAD_ZONE - 1),
                              -(GAMEPAD_AXIS_DEAD_ZONE - 1)) ==
          PAD_BUTTON_NONE);

    /* And the first count past it is, on either axis and in either direction.
     * SDL's Y grows downward, which is the one thing here that reads backwards
     * and the reason a test says so out loud. */
    CHECK(pad_stick_direction(0, -GAMEPAD_AXIS_DEAD_ZONE) ==
          PAD_BUTTON_DPAD_UP);
    CHECK(pad_stick_direction(0, GAMEPAD_AXIS_DEAD_ZONE) ==
          PAD_BUTTON_DPAD_DOWN);
    CHECK(pad_stick_direction(-GAMEPAD_AXIS_DEAD_ZONE, 0) ==
          PAD_BUTTON_DPAD_LEFT);
    CHECK(pad_stick_direction(GAMEPAD_AXIS_DEAD_ZONE, 0) ==
          PAD_BUTTON_DPAD_RIGHT);

    /* A full push at the corners resolves to one direction rather than to
     * nothing, and the dominant axis is the one that wins. */
    CHECK(pad_stick_direction(-32768, -4000) == PAD_BUTTON_DPAD_LEFT);
    CHECK(pad_stick_direction(32767, 4000) == PAD_BUTTON_DPAD_RIGHT);
    CHECK(pad_stick_direction(-4000, -32768) == PAD_BUTTON_DPAD_UP);
    CHECK(pad_stick_direction(4000, 32767) == PAD_BUTTON_DPAD_DOWN);

    /* An exact diagonal goes to the vertical, because every cursor this feeds
     * runs down a column: a tie that went sideways would be a stick pushed
     * corner-wise moving nothing at all on three of the game's screens. */
    CHECK(pad_stick_direction(20000, 20000) == PAD_BUTTON_DPAD_DOWN);
    CHECK(pad_stick_direction(-20000, -20000) == PAD_BUTTON_DPAD_UP);
    CHECK(pad_stick_direction(20000, -20000) == PAD_BUTTON_DPAD_UP);

    /* One axis parked outside the zone while the other is inside it still
     * answers the axis that is pushed, which is the case a resting thumb on a
     * worn stick produces all day. */
    CHECK(pad_stick_direction(GAMEPAD_AXIS_DEAD_ZONE - 1, 30000) ==
          PAD_BUTTON_DPAD_DOWN);
    CHECK(pad_stick_direction(-30000, GAMEPAD_AXIS_DEAD_ZONE - 1) ==
          PAD_BUTTON_DPAD_LEFT);
}

static void test_a_letter_and_a_button_find_each_other(void)
{
    PadHints hints;
    static const PadButtonLabel switch_labels[] = {
        PAD_LABEL_B, PAD_LABEL_A, PAD_LABEL_Y, PAD_LABEL_X};
    pad_hints_apply(&hints, PAD_TYPE_NINTENDO_SWITCH_PRO, switch_labels,
                    PAD_FACE_COUNT);

    /* Every face resolves to a button and that button resolves back to the same
     * face. `game_input.c` reads through one direction and writes a capture
     * through the other, so a pair that disagreed would store one player's
     * chosen button as a different one. */
    for (int face = 0; face < PAD_FACE_COUNT; ++face)
    {
        int button = pad_hints_button(&hints, (PadFace)face);
        CHECK(button != PAD_BUTTON_NONE);
        CHECK(pad_hints_face(&hints, button) == (PadFace)face);
    }

    /* No two faces on one button, on a pad whose letters moved. */
    for (int a = 0; a < PAD_FACE_COUNT; ++a)
        for (int b = a + 1; b < PAD_FACE_COUNT; ++b)
            CHECK(hints.at[a] != hints.at[b]);

    /* Everything that is not a face goes through as itself: the shoulders and
     * the d-pad mean the same thing wherever they are printed, so they carry no
     * letter and must not be handed one. (START is absent from the enum
     * entirely — `CHUCK_PAD_LIST` leaves it out because it is pause, and a sheet
     * that lets you rebind pause is a sheet you can lock yourself out of.) */
    CHECK(pad_hints_face(&hints, PAD_BUTTON_LEFT_SHOULDER) == PAD_FACE_NONE);
    CHECK(pad_hints_face(&hints, PAD_BUTTON_DPAD_LEFT) == PAD_FACE_NONE);
    CHECK(pad_hints_face(&hints, PAD_BUTTON_NONE) == PAD_FACE_NONE);
    CHECK(pad_hints_button(&hints, PAD_FACE_NONE) == PAD_BUTTON_NONE);
    CHECK(pad_hints_button(&hints, PAD_FACE_COUNT) == PAD_BUTTON_NONE);
}

/*
 * The prompt lines, and the promise that the answer is always in the buffer.
 *
 * A caller that ignores the return value and draws the buffer it passed in has
 * to get the right line, because one of them did exactly that and drew a line of
 * uninitialised stack across the drive's control prompt — the prompt that teaches
 * the only part of the game nobody guesses.
 */
static void test_a_prompt_is_spelled_for_the_pad_in_hand(void)
{
    PadHints hints;
    static const PadButtonLabel sony_labels[] = {
        PAD_LABEL_CROSS, PAD_LABEL_CIRCLE, PAD_LABEL_SQUARE,
        PAD_LABEL_TRIANGLE};
    pad_hints_apply(&hints, PAD_TYPE_PS4, sony_labels, PAD_FACE_COUNT);

    char buf[96];
    CHECK(strcmp(pad_hint(&hints, buf, sizeof(buf), "PRESS $A TO START",
                          "PRESS ENTER TO START"),
                 "PRESS X TO START") == 0);
    CHECK(strcmp(pad_hint(&hints, buf, sizeof(buf), "$X $B ATTACK",
                          "keys"),
                 "[] O ATTACK") == 0);
    /* Longest token first, so `$SELECT` is never eaten as `$S` + `ELECT`. */
    CHECK(strcmp(pad_hint(&hints, buf, sizeof(buf), "$START/$SELECT", "keys"),
                 "OPTIONS/SHARE") == 0);
    CHECK(strcmp(pad_hint(&hints, buf, sizeof(buf), "$LB $RB", "keys"),
                 "L1 R1") == 0);

    /* A stray `$` is copied through rather than swallowed: a template with a
     * typo in it should read oddly, not lose the rest of its line. */
    CHECK(strcmp(pad_hint(&hints, buf, sizeof(buf), "$Q COST $5", "keys"),
                 "$Q COST $5") == 0);

    /* No pad: the keyboard form, and it must be *in the buffer*. */
    buf[0] = 'z';
    const char *keys = pad_hint(NULL, buf, sizeof(buf), "PRESS $A", "PRESS H");
    CHECK(keys == buf);
    CHECK(strcmp(buf, "PRESS H") == 0);

    /* Truncation stays terminated, in both alphabets — the buffers these go
     * into are stack arrays and a token can expand past the end of one. */
    char tight[8];
    pad_hint(&hints, tight, sizeof(tight), "$Y AAAAAAAAAAAA", "keys");
    CHECK(strlen(tight) < sizeof(tight));
    pad_hint(NULL, tight, sizeof(tight), "$Y", "ENTER AND MORE BESIDES");
    CHECK(strlen(tight) < sizeof(tight));

    /* A caller with no buffer at all still gets a drawable line rather than
     * NULL, which is the one case that cannot write the answer into `buf`. */
    CHECK(strcmp(pad_hint(&hints, NULL, 0, "$A", "ENTER"), "ENTER") == 0);
}

/* Bindings survive the file, and a damaged binding line is not a reset — the
 * same rule every other value in this file keeps. */
static void test_bindings_survive_the_file(void)
{
    Settings written;
    settings_defaults(&written);
    CHECK(keybind_set(&written.bindings, BIND_JUMP, 0, 29));      /* Z */
    CHECK(keybind_set(&written.bindings, BIND_SHOOT, 1, 224));    /* LCTRL */
    CHECK(keybind_set(&written.bindings, BIND_WEAPON_PREV, 0, 20)); /* Q */
    CHECK(keybind_set(&written.bindings, BIND_WEAPON_PREV, 1, 43)); /* TAB */
    /*
     * An emptied action, which has to survive as empty rather than coming back
     * as the default — and which is written straight into the struct because
     * the sheet can no longer produce one. `keybind_set` swaps rather than
     * clears and refuses the bind that would leave an action answering nothing,
     * so emptiness now arrives only from a hand-edited file. That is exactly
     * the input this test is about, and it still has to round trip.
     */
    written.bindings.keys[BIND_WEAPON_NEXT][0] = KEYBIND_NONE;
    written.bindings.keys[BIND_WEAPON_NEXT][1] = KEYBIND_NONE;

    char text[2048];
    size_t len = settings_serialize(&written, text, sizeof(text));
    CHECK(len > 0);
    CHECK(len < sizeof(text));
    CHECK(text[len] == '\0');

    Settings read;
    settings_defaults(&read);
    settings_parse(&read, text);
    for (int a = 0; a < BIND_COUNT; ++a)
        for (int slot = 0; slot < BIND_SLOTS; ++slot)
            CHECK(read.bindings.keys[a][slot] ==
                  written.bindings.keys[a][slot]);

    /* And the action with nothing on it comes back with nothing on it, rather
     * than as the defaults the parser would otherwise be tempted to restore. */
    CHECK(read.bindings.keys[BIND_WEAPON_NEXT][0] == KEYBIND_NONE);
    CHECK(read.bindings.keys[BIND_WEAPON_NEXT][1] == KEYBIND_NONE);

    /* A line naming a key this build has never heard of leaves the action at
     * whatever it already had, rather than half-applying it. */
    Settings damaged;
    settings_defaults(&damaged);
    settings_parse(&damaged, "bind_jump LSHIFT NOSUCHKEY\n");
    CHECK(keybind_action_has(&damaged.bindings, BIND_JUMP, 225));
    settings_parse(&damaged, "bind_jump\n");
    CHECK(keybind_action_has(&damaged.bindings, BIND_JUMP, 225));

    /* And `-` really is an empty slot rather than an unreadable one. */
    Settings emptied;
    settings_defaults(&emptied);
    settings_parse(&emptied, "bind_jump - -\n");
    CHECK(emptied.bindings.keys[BIND_JUMP][0] == KEYBIND_NONE);
    CHECK(emptied.bindings.keys[BIND_JUMP][1] == KEYBIND_NONE);
}

/*
 * The whole sheet, written out at its longest, fits the buffer the shell hands
 * it — with room left over.
 *
 * `settings_serialize` truncates cleanly rather than overrunning, which is the
 * right behaviour and also why this needs checking: a buffer one line too small
 * loses the last binding *silently*, and what the player sees is the sheet
 * forgetting one row between launches. The buffer went from comfortable to
 * nearly full the moment nine binding lines were added to it, so the number is
 * pinned here rather than left to be rediscovered.
 */
static void test_the_settings_file_fits_the_buffer_that_writes_it(void)
{
    /* What `game_save_settings` declares. Written here as its own number so a
     * shrink there fails this rather than a player's controls. */
    char text[2048];

    Settings widest;
    settings_defaults(&widest);
    /* The longest line each action can produce: two six-character names. */
    int wide[] = {225, 229}; /* LSHIFT, RSHIFT */
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        /* Both slots of every action, which no real binding table can hold at
         * once — one key does one job — but which is the ceiling the buffer has
         * to clear anyway. */
        widest.bindings.keys[a][0] = wide[0];
        widest.bindings.keys[a][1] = wide[1];
        /* And the pad's own nine lines, at their longest spelling: the file
         * keeps positional names, so DPAD_RIGHT is the widest one. */
        widest.bindings.pad[a][0] = 14; /* DPAD_RIGHT */
        widest.bindings.pad[a][1] = 13; /* DPAD_LEFT  */
    }
    widest.music_volume = 100;
    widest.sfx_volume = 100;

    size_t len = settings_serialize(&widest, text, sizeof(text));
    CHECK(len > 0);
    /* Not merely "did not overrun": it has to be short of the end, or the next
     * row added to the sheet is the one that silently falls off. */
    CHECK(len < sizeof(text) - 128);
    CHECK(text[len] == '\0');

    /* And it really does round-trip at that size. */
    Settings back;
    settings_defaults(&back);
    settings_parse(&back, text);
    for (int a = 0; a < BIND_COUNT; ++a)
    {
        CHECK(back.bindings.keys[a][0] == wide[0]);
        CHECK(back.bindings.keys[a][1] == wide[1]);
        CHECK(back.bindings.pad[a][0] == 14);
        CHECK(back.bindings.pad[a][1] == 13);
    }

    /* A buffer too small loses the tail rather than the stack, and says how
     * much it wrote. */
    char cramped[64];
    size_t short_len = settings_serialize(&widest, cramped, sizeof(cramped));
    CHECK(short_len < sizeof(cramped));
    CHECK(cramped[short_len] == '\0');
}

static void test_settings_cursor_only_lands_on_rows(void)
{
    /* Both pages, because the second one is a table of the same kind and the
     * cursor walks it with the same function. A page added without being put
     * on this loop is a page nothing checks. */
    for (int p = 0; p < SETTINGS_PAGE_COUNT; ++p)
    {
        SettingsPage page = (SettingsPage)p;
        int row_count = 0;
        const SettingRow *rows = settings_rows(page, &row_count);
        CHECK(row_count > 0);
        CHECK(settings_page_title(page) != NULL);
        CHECK(settings_page_strap(page) != NULL);

        int cursor = settings_first_row(page);
        CHECK(rows[cursor].kind != SETTING_ROW_HEADING);

        /* Two full laps in each direction: every stop is a real row, and the
         * walk comes back to where it started rather than getting stuck at an
         * end. */
        for (int direction = -1; direction <= 1; direction += 2)
        {
            int at = settings_first_row(page);
            int seen = 0;
            for (int step = 0; step < row_count * 2; ++step)
            {
                at = settings_move_cursor(page, at, direction);
                CHECK(rows[at].kind != SETTING_ROW_HEADING);
                CHECK(rows[at].id != SETTING_NONE);
                if (at == settings_first_row(page))
                    ++seen;
            }
            CHECK(seen == 2);
        }

        /*
         * Every row the cursor can reach says something about itself: a switch
         * the player cannot see the point of is a switch they will not touch.
         *
         * A binding row is the one exception, and it is an exception to the
         * letter of the rule rather than its point: MOVE LEFT is the whole of
         * what that row does, and nine sentences repeating their own labels
         * would push the controls page past the frame. What the section needs
         * saying is said once, on its heading — which is checked here, so the
         * exception cannot be taken without the heading carrying it.
         */
        bool binding_section_explained = true;
        for (int i = 0; i < row_count; ++i)
        {
            CHECK(rows[i].label != NULL);
            if (rows[i].kind == SETTING_ROW_HEADING ||
                rows[i].kind == SETTING_ROW_BINDING)
            {
                continue;
            }
            CHECK(rows[i].detail != NULL);
        }
        for (int i = 0; i < row_count; ++i)
        {
            if (rows[i].kind != SETTING_ROW_BINDING)
                continue;
            /* Walk back to the heading this row sits under. */
            bool explained = false;
            for (int up = i; up >= 0; --up)
            {
                if (rows[up].kind != SETTING_ROW_HEADING)
                    continue;
                explained = rows[up].detail != NULL;
                break;
            }
            if (!explained)
                binding_section_explained = false;
        }
        CHECK(binding_section_explained);
    }
}

/*
 * The line the mute puts on the audio heading, and the reason this test is
 * here rather than nowhere.
 *
 * `settings_heading_governs_levels` used to live in the renderer that draws
 * the line, behind `muted &&` — and nothing in the tree is ever muted, so a
 * rule about the one screen the mix is read off was executed by nothing at
 * all. It reads the row table and nothing else, which makes it a question
 * about the table, which is where it now lives and where this can hold it.
 *
 * The rule is "found by what it contains, never by its label", so the test is
 * written that way too: it does not name AUDIO anywhere.
 */
static void test_the_audio_heading_is_found_by_what_it_holds(void)
{
    int count = 0;
    const SettingRow *rows = settings_rows(SETTINGS_PAGE_MAIN, &count);
    CHECK(rows != NULL);
    CHECK(count > 0);

    /* Exactly one heading on the sheet governs sliders. Two would mean the
     * warning is printed twice; none would mean a silent game says so
     * nowhere. */
    int governing = 0;
    int found_at = -1;
    for (int i = 0; i < count; ++i)
    {
        if (settings_heading_governs_levels(rows, count, i))
        {
            ++governing;
            found_at = i;
        }
    }
    CHECK(governing == 1);

    /* And it is the one the sliders actually sit under: walk forward from it
     * and a slider is reached before the next heading is. */
    CHECK(found_at >= 0);
    CHECK(rows[found_at].kind == SETTING_ROW_HEADING);
    bool slider_first = false;
    for (int i = found_at + 1; i < count; ++i)
    {
        if (rows[i].kind == SETTING_ROW_HEADING)
            break;
        if (rows[i].kind == SETTING_ROW_SLIDER)
        {
            slider_first = true;
            break;
        }
    }
    CHECK(slider_first);

    /* A row that is not a heading never governs anything, however close to one
     * it is standing. */
    for (int i = 0; i < count; ++i)
    {
        if (rows[i].kind != SETTING_ROW_HEADING)
            CHECK(!settings_heading_governs_levels(rows, count, i));
    }

    /* The controls page carries no sliders at all, so no heading on it governs
     * levels and the mute has nothing to say there. */
    int control_count = 0;
    const SettingRow *control_rows =
        settings_rows(SETTINGS_PAGE_CONTROLS, &control_count);
    for (int i = 0; i < control_count; ++i)
        CHECK(!settings_heading_governs_levels(control_rows, control_count, i));

    /* Off both ends and off a null table, the way a renderer asks for a row it
     * has no business asking for: false rather than a read past the end. */
    CHECK(!settings_heading_governs_levels(rows, count, -1));
    CHECK(!settings_heading_governs_levels(rows, count, count));
    CHECK(!settings_heading_governs_levels(NULL, count, 0));
}

static void test_settings_sliders_step_and_stop(void)
{
    Settings settings;
    settings_defaults(&settings);

    /* Both levels open at the top, so the mix a fresh install hears is the one
     * the effects and the scores were balanced at. */
    CHECK(settings.music_volume == 100);
    CHECK(settings.sfx_volume == 100);

    /* At the top there is nothing to give: an unchanged value must report as
     * unchanged, because that is what stops a slider clicking at its own end. */
    CHECK(!settings_adjust(&settings, SETTING_MUSIC_VOLUME, 1));
    CHECK(settings_adjust(&settings, SETTING_MUSIC_VOLUME, -1));
    CHECK(settings.music_volume == 100 - SETTING_VOLUME_STEP);

    for (int i = 0; i < 100; ++i)
        settings_adjust(&settings, SETTING_MUSIC_VOLUME, -1);
    CHECK(settings.music_volume == 0);
    CHECK(!settings_adjust(&settings, SETTING_MUSIC_VOLUME, -1));
    /* One bus at a time: the score going quiet must not take the shots with it. */
    CHECK(settings.sfx_volume == 100);

    /* A switch has no "more" and no "less", so both directions flip it. */
    CHECK(settings_adjust(&settings, SETTING_MORE_HEARTS, 1));
    CHECK(settings_value_bool(&settings, SETTING_MORE_HEARTS));
    CHECK(settings_adjust(&settings, SETTING_MORE_HEARTS, -1));
    CHECK(!settings_value_bool(&settings, SETTING_MORE_HEARTS));
}

/*
 * Every row on the sheet, and the one thing the test above could not ask.
 *
 * That test drives two rows by name — the music slider and one assist switch —
 * and it is the only thing in the suite that had ever called `settings_adjust`.
 * Nine rows answer to it. `make coverage` reported seven of the nine arms of its
 * switch as never executed: the effects slider, fullscreen, the CRT filter,
 * reduced motion, slower guards, infinite lives and **veteran** — the last three
 * being the ones that change the simulation rather than the picture.
 *
 * That is a `switch` on an enum where every arm is one line of the same shape,
 * which is the exact shape a copy-paste gets wrong, and getting it wrong is
 * silent in both directions: a row wired to its neighbour's field means the
 * player toggles veteran and infinite lives comes on. Naming rows one at a time
 * is how six of them came to have no test — so this walks the enum instead, and
 * a tenth row added tomorrow is walked by having been added.
 *
 * What it asks is deliberately not "did the right field move", because a test
 * that names the field per row is the same list again, wrong in the same way.
 * It asks that each row moves *a* field and that no two rows move the *same*
 * one, which is the property a copy-paste breaks and needs no list to state.
 */
static void test_every_options_row_moves_its_own_field(void)
{
    /* One fingerprint per row: which bytes of `Settings` this row disturbed.
     * `settings_defaults` clears the struct whole, so two sheets that agree
     * about every setting agree byte for byte and a difference is a real one. */
    typedef struct
    {
        SettingId id;
        unsigned char before[sizeof(Settings)];
        unsigned char after[sizeof(Settings)];
    } Fingerprint;

    static Fingerprint prints[SETTING_BIND_FIRST];
    int adjustable = 0;

    for (int id = 0; id < SETTING_BIND_FIRST; ++id)
    {
        Settings baseline;
        Settings moved;
        settings_defaults(&baseline);
        settings_defaults(&moved);

        /* Both directions, because a slider already at its cap reports no
         * change for the direction it cannot go — which is correct, and is why
         * asking only one way found seven of these rows "unadjustable". */
        bool changed = settings_adjust(&moved, (SettingId)id, 1);
        if (!changed)
            changed = settings_adjust(&moved, (SettingId)id, -1);
        if (!changed)
        {
            /* The rows that hold no value: a binding is taken rather than
             * adjusted, and the other three are pressed. They must also leave
             * the sheet exactly as they found it — a row that reports no change
             * and changes something is worse than one that does neither. */
            CHECK(memcmp(&baseline, &moved, sizeof(Settings)) == 0);
            continue;
        }

        Fingerprint *print = &prints[adjustable++];
        print->id = (SettingId)id;
        memcpy(print->before, &baseline, sizeof(Settings));
        memcpy(print->after, &moved, sizeof(Settings));

        /* Reporting a change and making none is the lie that would let a dead
         * row click as though it worked. */
        CHECK(memcmp(print->before, print->after, sizeof(Settings)) != 0);
    }

    /* Nine: two sliders, three display switches, three assists and veteran. A
     * row that stops answering is as much a defect as one that answers wrongly,
     * and without this the loop above passes an empty sheet. */
    CHECK(adjustable == 9);

    /* No two rows may move the same bytes. This is the copy-paste itself: two
     * arms of the switch naming one field is a control the player cannot reach
     * and a control that does something else's job. */
    for (int a = 0; a < adjustable; ++a)
    {
        int moved_bytes = 0;
        for (size_t byte = 0; byte < sizeof(Settings); ++byte)
        {
            if (prints[a].before[byte] == prints[a].after[byte])
                continue;
            ++moved_bytes;
            for (int b = 0; b < adjustable; ++b)
            {
                if (b == a)
                    continue;
                CHECK(prints[b].before[byte] == prints[b].after[byte]);
            }
        }
        CHECK(moved_bytes > 0);
    }
}

/*
 * And what the sheet *prints* for each of those rows, which is the same
 * question asked of the other half of the pair.
 *
 * `settings_adjust` got the walk above after seven of its nine arms turned out
 * to have no test; `settings_value_bool` — the one function that decides whether
 * a switch reads ON or OFF on screen — kept the one-arm-at-a-time treatment, and
 * six of its seven arms had never been executed at all. Every caller of it is in
 * `game_render.c`, so nothing on this side of the SDL line had ever asked it
 * anything except about `SETTING_MORE_HEARTS`.
 *
 * What that costs is not a crash. An arm reading the wrong field — VETERAN
 * returning `infinite_lives` — prints the wrong state for a switch the player
 * has just flipped, on the one screen in the game whose entire job is to report
 * state, and the run then behaves the way the sheet is *not* showing. A missing
 * arm is worse and quieter: the `default` returns false, so the switch simply
 * never lights, and the player concludes the option does not work.
 *
 * Asked as the same property the setter is held to, because it is the same
 * defect: every toggle reports *a* field, the one its own row moves, and no two
 * of them report the same field. The rows come off the sheet's own tables rather
 * than being listed here, so a toggle added without a reader fails this.
 */
static void test_every_toggle_on_the_options_sheet_reports_its_own_field(void)
{
    SettingId toggles[SETTING_BIND_FIRST];
    int toggle_count = 0;
    for (int page = 0; page < SETTINGS_PAGE_COUNT; ++page)
    {
        int row_count = 0;
        const SettingRow *rows = settings_rows((SettingsPage)page, &row_count);
        for (int i = 0; i < row_count; ++i)
        {
            if (rows[i].kind != SETTING_ROW_TOGGLE)
                continue;
            REQUIRE(toggle_count < (int)(sizeof(toggles) / sizeof(toggles[0])));
            toggles[toggle_count++] = rows[i].id;
        }
    }
    /* Three display switches, three assists and veteran. Without this the two
     * loops below pass a sheet with no toggles on it. */
    CHECK(toggle_count == 7);

    for (int a = 0; a < toggle_count; ++a)
    {
        Settings before;
        Settings after;
        settings_defaults(&before);
        settings_defaults(&after);
        CHECK(settings_adjust(&after, toggles[a], 1));

        /* The row that moved says so ... */
        CHECK(settings_value_bool(&after, toggles[a]) !=
              settings_value_bool(&before, toggles[a]));
        /* ... and every other row says exactly what it said, which is the
         * copy-paste itself: two arms naming one field is a switch that reports
         * another switch's state. */
        for (int b = 0; b < toggle_count; ++b)
        {
            if (b == a)
                continue;
            CHECK(settings_value_bool(&after, toggles[b]) ==
                  settings_value_bool(&before, toggles[b]));
        }
    }

    /* And a slider is not a switch. The sheet asks `settings_value_bool` only of
     * toggle rows, and a slider arm quietly added to it would be a bar drawn as
     * a switch by whatever asked the wrong question. */
    Settings loud;
    settings_defaults(&loud);
    loud.music_volume = 100;
    loud.sfx_volume = 100;
    CHECK(!settings_value_bool(&loud, SETTING_MUSIC_VOLUME));
    CHECK(!settings_value_bool(&loud, SETTING_SFX_VOLUME));
}

/*
 * What a row *is*, asked the two ways the sheet asks it.
 *
 * `settings_row_action` and `settings_value_percent` are the two questions the
 * renderer puts to a row before drawing it — which action's key caps go on this
 * line, and what number goes in this slider — and neither had ever been called
 * by the suite, because every one of their callers is in `game.c` or
 * `game_render.c`, on the far side of the SDL line. Eleven regions of
 * arithmetic on an enum between them, which is precisely the kind of thing that
 * is quietly off by one after somebody adds a row.
 */
static void test_a_settings_row_says_which_action_and_which_number_it_is(void)
{
    /* The binding rows are one contiguous run starting at SETTING_BIND_FIRST,
     * so the sheet can walk the nine actions without a table of its own. */
    for (int action = 0; action < BIND_COUNT; ++action)
    {
        SettingId id = (SettingId)(SETTING_BIND_FIRST + action);
        CHECK(settings_row_action(id) == (BindAction)action);
    }
    /* One past the last is not a tenth action, it is nothing — which is what
     * keeps a row id that has drifted from drawing somebody else's key caps. */
    CHECK(settings_row_action((SettingId)(SETTING_BIND_FIRST + BIND_COUNT)) ==
          BIND_COUNT);
    /* And nothing below the run is a binding at all. */
    CHECK(settings_row_action(SETTING_NONE) == BIND_COUNT);
    CHECK(settings_row_action(SETTING_MUSIC_VOLUME) == BIND_COUNT);
    CHECK(settings_row_action(SETTING_BINDINGS_RESET) == BIND_COUNT);

    Settings settings;
    settings_defaults(&settings);
    CHECK(settings_value_percent(&settings, SETTING_MUSIC_VOLUME) ==
          settings.music_volume);
    CHECK(settings_value_percent(&settings, SETTING_SFX_VOLUME) ==
          settings.sfx_volume);

    /* The two buses are separate all the way to the number that is drawn. */
    CHECK(settings_adjust(&settings, SETTING_MUSIC_VOLUME, -1));
    CHECK(settings_value_percent(&settings, SETTING_MUSIC_VOLUME) ==
          100 - SETTING_VOLUME_STEP);
    CHECK(settings_value_percent(&settings, SETTING_SFX_VOLUME) == 100);

    /* A row that carries no level reads nought rather than the last one asked
     * for, because the slider is drawn from this and a stale value would draw a
     * bar on a toggle. */
    CHECK(settings_value_percent(&settings, SETTING_FULLSCREEN) == 0);
    CHECK(settings_value_percent(&settings, SETTING_NONE) == 0);
}

static void test_settings_survive_the_file(void)
{
    Settings written;
    settings_defaults(&written);
    written.music_volume = 40;
    written.sfx_volume = 70;
    written.fullscreen = true;
    written.crt_filter = false;
    written.reduced_motion = true;
    written.assist.more_hearts = true;
    written.assist.infinite_lives = true;

    char text[512];
    size_t len = settings_serialize(&written, text, sizeof(text));
    CHECK(len > 0);
    CHECK(len < sizeof(text));
    CHECK(text[len] == '\0');

    Settings read;
    settings_defaults(&read);
    settings_parse(&read, text);
    CHECK(read.music_volume == 40);
    CHECK(read.sfx_volume == 70);
    CHECK(read.fullscreen);
    CHECK(!read.crt_filter);
    CHECK(read.reduced_motion);
    CHECK(read.assist.more_hearts);
    CHECK(!read.assist.slower_guards);
    CHECK(read.assist.infinite_lives);
}

static void test_settings_file_damage_is_not_a_reset(void)
{
    Settings settings;
    settings_defaults(&settings);
    settings.sfx_volume = 30;

    /* A comment, a blank line, a key from a build this one has never heard of,
     * a line with no value and a level well outside the bar. None of it may
     * take a setting the file did not mention with it. */
    settings_parse(&settings,
                   "# written by some other version\n"
                   "\n"
                   "shadows 3\n"
                   "fullscreen\n"
                   "music 4000\n");
    CHECK(settings.sfx_volume == 30);
    CHECK(!settings.fullscreen);
    CHECK(settings.music_volume == 100);

    settings_parse(&settings, "music -20\n");
    CHECK(settings.music_volume == 0);

    /* And an empty file is the defaults, not a crash. */
    Settings empty;
    settings_defaults(&empty);
    settings_parse(&empty, "");
    settings_parse(&empty, NULL);
    CHECK(empty.music_volume == 100);
    CHECK(empty.crt_filter);
}

/*
 * What outlives the process.
 *
 * The campaign is seventeen sectors and a prologue, which is more than one
 * sitting for most people, and none of it used to survive the game being
 * closed: a run abandoned on sector nine started again in the lobby. Two
 * numbers now do survive, and both are ratchets — a worse run must never take
 * anything off a better one, however a file is edited or a session ends.
 */
static void test_progress_only_ever_climbs(void)
{
    Progress progress;
    progress_defaults(&progress);
    CHECK(progress.best_score == 0);
    /* A fresh install offers no resume: the lobby is what START already is. */
    CHECK(progress.furthest_sector == 0);

    CHECK(progress_note_sector(&progress, 8));
    CHECK(progress.furthest_sector == 8);
    /* Dying back on sector three does not un-earn sector nine, and neither
     * does replaying it: only "did this move a number" writes the file. */
    CHECK(!progress_note_sector(&progress, 3));
    CHECK(!progress_note_sector(&progress, 8));
    CHECK(!progress_note_sector(&progress, -2));
    CHECK(progress.furthest_sector == 8);

    CHECK(progress_note_score(&progress, 12500));
    CHECK(!progress_note_score(&progress, 900));
    CHECK(!progress_note_score(&progress, 12500));
    CHECK(progress.best_score == 12500);
}

/*
 * The third ratchet, and the one that does not start where the other two do.
 *
 * A score starts at nought and a sector at the lobby, so "did this beat what
 * was there" is the whole rule for both. A time starts at *nobody has cleared
 * this*, which is not a time at all — and taking nought as one would file a
 * perfect run against every sector the moment the file was created, which is
 * the shape of bug that makes a record sheet useless the first time it is read.
 */
/*
 * One sheet of the docket in every interior, and none out on a wall.
 *
 * The count is the whole of what makes it a collection rather than a scattering
 * of bonus points: a player who has eleven has the case, and one who has ten
 * knows exactly which floor they left it on. Written down here rather than
 * trusted to the maps, because "one per sector" is the kind of rule that
 * survives right up until somebody adds a sixteenth sector and forgets.
 *
 * A climb carries none for the reason it carries no magazine: `update_facade_playing`
 * does run `gameplay_collect_items`, so a sheet out there would work — and it
 * would also be the one collectable the player could be blown off a wall
 * reaching for, in a mode with no combat and no way to answer anything.
 */
static void test_every_interior_lays_out_exactly_one_docket_sheet(void)
{
    int interiors = 0;
    int sheets_total = 0;
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        static Level level;
        Rng rng;
        rng_seed(&rng, 4001 + (int)i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        int sheets = 0;
        for (int item = 0; item < level.runtime.item_count; ++item)
            sheets += level.runtime.items[item].type == ITEM_EVIDENCE;

        if (level.map.mode == LEVEL_MODE_FACADE)
        {
            CHECK(sheets == 0);
            continue;
        }
        ++interiors;
        sheets_total += sheets;
        CHECK(sheets == 1);
    }
    /* Twelve interiors, twelve sheets: the number the game over card counts
     * against and the number the fiction is holding out for. It was eleven
     * until the vault joined the campaign, and this line is the one that has to
     * move with it — a sixteenth interior without a sheet would be a collection
     * the player cannot complete and nothing on screen to say why. */
    CHECK(interiors == 12);
    CHECK(sheets_total == interiors);
}

static int docket_from_start[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
static int docket_from_sheet[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];

/*
 * And it costs a detour, which is the half of the rule nothing measured.
 *
 * [levels/LEGEND.md](../levels/LEGEND.md) has always said "put it somewhere that
 * costs a detour rather than on the route to the door", and the test above
 * counts the sheets while the campaign check counts whether they can be
 * *reached*. Neither asks the question the sentence is about, and measured
 * through the route model the answer was that **seven of the twelve cost
 * nothing at all**: sectors 1, 2, 4, 5, 6, 9 and 16 had their sheet sitting on a
 * shortest path to the way out, and sector 12's cost one step. So the one
 * collectable in the game that is meant to be a decision was, on eight floors
 * out of twelve, picked up by walking to the door — which also means the
 * collection completed itself and the game-over card's DOCKET line reported
 * nothing the player had chosen to do.
 *
 * Measured both ways round, because the model's moves are not all reversible: a
 * step off a ledge is a one-way edge, so "how far back from the sheet to the way
 * out" needs its own flood *from the sheet* rather than a flood from the exit
 * read backwards.
 *
 * The bar is a tenth of the sector's own walk rather than a number of steps,
 * because a step is a tile and the floors run from 19 to 136 of them: eight steps
 * is a decision on sector 14 and a rounding error on sector 4. Everything the
 * campaign ships clears it with margin — the tightest is sector 4 at 22 against
 * 136 — and the point of the bar is not to be tight, it is to fail when a sheet
 * is dropped on the walk to the door.
 */
static void test_the_docket_sheet_costs_a_detour(void)
{
    static Level level;
    static RouteMap route;
    int measured = 0;

    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        Rng rng;
        rng_seed(&rng, 7300 + (int)i);
        REQUIRE(level_load_data(&level, EMBEDDED_LEVELS[i].name,
                                EMBEDDED_LEVELS[i].data,
                                EMBEDDED_LEVELS[i].size, &rng));
        if (level.map.mode == LEVEL_MODE_FACADE)
            continue; /* no sheet out on a wall, by the test above */

        route_map_init(&route, &level);
        RouteCell start = route_player_start(&route);
        RouteCell goal = level.map.has_window
                             ? (RouteCell){level.map.window_col,
                                           level.map.window_row}
                             : (RouteCell){level.map.exit_col,
                                           level.map.exit_row};
        checkpoint_bfs(&route, docket_from_start, start);
        int direct = docket_from_start[goal.row][goal.col];
        REQUIRE(direct > 0);

        RouteCell sheet = {-1, -1};
        for (int item = 0; item < level.runtime.item_count; ++item)
        {
            if (level.runtime.items[item].type != ITEM_EVIDENCE)
                continue;
            sheet = (RouteCell){(int)(level.runtime.items[item].x / TILE_SIZE),
                                (int)(level.runtime.items[item].y / TILE_SIZE)};
        }
        REQUIRE(sheet.col >= 0);
        /* Taken by standing where it can be reached from, the same rule
         * `route_reaches` uses for a card hanging over a floor. */
        if (!route_standing(&route, sheet.col, sheet.row))
        {
            RouteCell landing;
            REQUIRE(route_landing(&route, sheet.col, sheet.row, &landing));
            sheet = landing;
        }
        REQUIRE(docket_from_start[sheet.row][sheet.col] >= 0);

        checkpoint_bfs(&route, docket_from_sheet, sheet);
        int back = docket_from_sheet[goal.row][goal.col];
        REQUIRE(back >= 0);

        int detour = docket_from_start[sheet.row][sheet.col] + back - direct;
        CHECK(detour > 0);
        CHECK(detour * 10 >= direct);
        measured++;
    }
    /* And every interior was actually measured. A sweep that quietly stops
     * reaching its subject is the failure this file keeps finding — here it
     * would take one `continue` in the wrong place to check nothing and pass. */
    CHECK(measured == 12);
}

/*
 * Picking one up, and everything it deliberately does not do.
 *
 * It is the only pickup in the game that is worth nothing to the man carrying
 * it, and each of these is a way it could quietly stop being that: banking a
 * checkpoint would make "touch the collectable first" the safest route through
 * a floor, and counting it on the sector rather than on the run would throw it
 * away at the next doorway.
 */
static void test_a_docket_sheet_is_counted_by_the_run(void)
{
    static const char data[] =
        "############\n"
        "#S  *     E#\n"
        "############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 5150);
    REQUIRE(level_load_data(&state.level, "docket", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);

    int sheet = -1;
    for (int i = 0; i < state.level.runtime.item_count; ++i)
        if (state.level.runtime.items[i].type == ITEM_EVIDENCE)
            sheet = i;
    REQUIRE(sheet >= 0);

    state.player.x = state.level.runtime.items[sheet].x - PLAYER_W * 0.5f;
    gameplay_collect_items(&state, &campaign, SIM_STEP_DT);

    CHECK(state.level.runtime.items[sheet].collected);
    CHECK(campaign.evidence_collected == 1);
    CHECK(campaign.score == EVIDENCE_SCORE);
    /* Paper is not progress: nothing about it is worth resuming at, and banking
     * one would make the collectable the safest thing on the floor to run at. */
    CHECK(!state.interior_has_checkpoint);
    /* And it does not come back. The magazine is the only pickup that does. */
    CHECK(state.level.runtime.items[sheet].respawn_timer == 0.0f);
    for (int step = 0; step < SIM_STEPS(24.0f); ++step)
        gameplay_collect_items(&state, &campaign, SIM_STEP_DT);
    CHECK(state.level.runtime.items[sheet].collected);
    CHECK(campaign.evidence_collected == 1);
}

/*
 * The veteran run, and the three numbers it is made of.
 *
 * A second difficulty *tuning* would be a second campaign to balance — every
 * map, hazard budget and jump in this tree is drawn against the pace in
 * game_config.h. So this moves the crew's speed, the lives and the continues,
 * all read at the places the assist switches are already read, and nothing
 * else. Each half of the test is a way that could quietly stop being true.
 */
static void test_the_veteran_run_is_three_numbers_and_no_more(void)
{
    CampaignState ordinary;
    CampaignState veteran;
    campaign_reset(&ordinary, false);
    campaign_reset(&veteran, true);
    CHECK(ordinary.lives == PLAYER_LIVES);
    CHECK(ordinary.continues_remaining == PLAYER_CONTINUES);
    CHECK(veteran.lives == VETERAN_LIVES);
    CHECK(veteran.continues_remaining == VETERAN_CONTINUES);
    CHECK(veteran.lives < ordinary.lives);

    /*
     * And it is still those numbers after the run has been continued, which is
     * the half this test used to leave out.
     *
     * `campaign_reset` is not the only place lives are handed out —
     * `campaign_accept_continue` is the other — and it used to hand out
     * `PLAYER_LIVES` flat, because the flag it needed had been spent above and
     * forgotten. A veteran run opens with no continues at all, so the *first*
     * death is the one that takes the score-reset branch: one mistake, which on
     * a one-life run is the whole of the run, and it came back with three lives.
     * Checking `campaign_reset` alone is checking the mode for as long as
     * nothing has happened yet.
     */
    CHECK(campaign_lose_life(&veteran)); /* out of lives on the first one */
    CHECK(campaign_begin_continue(&veteran));
    CHECK(campaign_accept_continue(&veteran));
    CHECK(veteran.lives == VETERAN_LIVES);
    CHECK(veteran.score == 0); /* no continue to spend, so the score goes */

    /* The ordinary run's continue is unchanged by that, and it has continues to
     * spend, so this walks the other branch as well. */
    while (!campaign_lose_life(&ordinary))
        ;
    CHECK(campaign_begin_continue(&ordinary));
    CHECK(campaign_accept_continue(&ordinary));
    CHECK(ordinary.lives == PLAYER_LIVES);
    CHECK(ordinary.continues_remaining == PLAYER_CONTINUES - 1);

    /* The switch can be reached from the pause sheet mid-run, so the flag
     * follows the sheet in both directions rather than being latched at the
     * start of the run the way `assisted` is. */
    campaign_note_veteran(&ordinary, true);
    while (!campaign_lose_life(&ordinary))
        ;
    CHECK(campaign_begin_continue(&ordinary));
    CHECK(campaign_accept_continue(&ordinary));
    CHECK(ordinary.lives == VETERAN_LIVES);
    campaign_note_veteran(&ordinary, false);
    while (!campaign_lose_life(&ordinary))
        ;
    CHECK(campaign_begin_continue(&ordinary));
    CHECK(campaign_accept_continue(&ordinary));
    CHECK(ordinary.lives == PLAYER_LIVES);

    /* A zeroed campaign is the authored difficulty, the same rule the assist
     * flags keep, so nothing that writes `{0}` gets a one-life continue. */
    CampaignState zeroed = {0};
    zeroed.continue_timer = CONTINUE_COUNTDOWN_TIME;
    CHECK(campaign_accept_continue(&zeroed));
    CHECK(zeroed.lives == PLAYER_LIVES);

    GameplayState state = {0};
    /* A zeroed state is the authored difficulty, which is the rule the assist
     * flags already keep and the reason every plainly-initialised test runs at
     * the pace the maps were drawn against. */
    CHECK(gameplay_enemy_speed_scale(&state) == 1.0f);

    state.veteran = true;
    CHECK(gameplay_enemy_speed_scale(&state) == VETERAN_ENEMY_SPEED);
    /* And still slower than Chuck: a crew that outran him on open floor would
     * mean there is no such thing as breaking off, which is a different game
     * rather than a harder one. */
    CHECK(ENEMY_WALK_SPEED * VETERAN_ENEMY_SPEED < PLAYER_WALK_SPEED);

    /* Both switches on is somebody asking for help, and the help wins. */
    state.assist_slow_enemies = true;
    CHECK(gameplay_enemy_speed_scale(&state) == ASSIST_ENEMY_SPEED);

    /* It changes nothing else about the simulation: hearts are the assist's
     * business and this must not quietly take one. */
    GameplayState hard = {0};
    hard.veteran = true;
    CHECK(gameplay_player_max_hp(&hard) == PLAYER_MAX_HP);
}

/*
 * The night is thirty-eight minutes long and the campaign has to fill it.
 *
 * The clock is stated in five places the player reads — two cutscenes, the
 * manual, the intel table and the wall dial in every interior — and all of them
 * agree that Chuck walks in at 00:22 and the bonds leave at 01:00. The per
 * sector step is the only part of that which is arithmetic, so it is the only
 * part that can quietly stop being true: a sector added without touching
 * `NIGHT_CLOCK_SECTORS` is a campaign whose last dial reads something other
 * than one o'clock, and nothing on screen would say so.
 */
static void test_the_night_clock_fills_the_night(void)
{
    CHECK(NIGHT_CLOCK_SECTORS == (int)EMBEDDED_LEVEL_COUNT);
    float last_dial = NIGHT_CLOCK_FIRST_MINUTE +
                      NIGHT_CLOCK_MINUTES_PER_SECTOR *
                          (float)NIGHT_CLOCK_SECTORS;
    /* 00:22 plus the whole night is 01:00. */
    CHECK(fabsf(last_dial - 60.0f) < 0.01f);
    /* And the par the score pays is still the slot the dial gives a floor,
     * which is what `test_the_sector_par_is_the_night_clock_s_own` pins from
     * the other end. */
    CHECK(SECTOR_PAR_SECONDS > 120.0f);
    CHECK(SECTOR_PAR_SECONDS < 150.0f);
}

static void test_a_sector_time_is_a_record_rather_than_a_number(void)
{
    Progress progress;
    progress_defaults(&progress);
    for (int i = 0; i < PROGRESS_MAX_TRACKED_SECTORS; ++i)
        CHECK(progress_sector_time(&progress, i) == PROGRESS_NO_TIME);

    /* A first clear is a record by definition. */
    CHECK(progress_note_sector_time(&progress, 4, 132.5f));
    CHECK(progress_sector_time(&progress, 4) == 132.5f);

    /* Quicker takes it; slower and identical do not, so the file is only
     * written on the frames that moved a number. */
    CHECK(progress_note_sector_time(&progress, 4, 121.0f));
    CHECK(!progress_note_sector_time(&progress, 4, 121.0f));
    CHECK(!progress_note_sector_time(&progress, 4, 200.0f));
    CHECK(progress_sector_time(&progress, 4) == 121.0f);

    /* Sectors keep their own records rather than sharing one. */
    CHECK(progress_note_sector_time(&progress, 5, 300.0f));
    CHECK(progress_sector_time(&progress, 4) == 121.0f);
    CHECK(progress_sector_time(&progress, 5) == 300.0f);

    /* Out of range is dropped rather than clamped: a time written into the last
     * tracked slot would be a record standing against a floor nobody played. */
    CHECK(!progress_note_sector_time(&progress, -1, 100.0f));
    CHECK(!progress_note_sector_time(&progress,
                                     PROGRESS_MAX_TRACKED_SECTORS, 100.0f));
    CHECK(progress_sector_time(&progress, PROGRESS_MAX_TRACKED_SECTORS - 1) ==
          PROGRESS_NO_TIME);

    /* And a figure no run could have produced is refused at both ends, because
     * either would sit at the top of the sheet for ever. */
    CHECK(!progress_note_sector_time(&progress, 6, 0.0f));
    CHECK(!progress_note_sector_time(&progress, 6, -30.0f));
    CHECK(!progress_note_sector_time(&progress, 6, 999999.0f));
    CHECK(progress_sector_time(&progress, 6) == PROGRESS_NO_TIME);
}

static void test_progress_survives_the_file(void)
{
    Progress written;
    progress_defaults(&written);
    progress_note_score(&written, 43120);
    progress_note_sector(&written, 11);
    progress_note_sector_time(&written, 0, 61.25f);
    progress_note_sector_time(&written, 11, 148.75f);

    char text[2048];
    size_t len = progress_serialize(&written, text, sizeof(text));
    CHECK(len > 0 && len < sizeof(text));
    CHECK(text[len] == '\0');

    Progress read;
    progress_defaults(&read);
    progress_parse(&read, text);
    CHECK(read.best_score == written.best_score);
    CHECK(read.furthest_sector == written.furthest_sector);
    /* Two hundredths, because the file writes two decimals: a round trip that
     * only held whole seconds would quietly throw away the difference between
     * two runs a tenth apart, which is the whole of what a record is for. */
    CHECK(fabsf(progress_sector_time(&read, 0) - 61.25f) < 0.02f);
    CHECK(fabsf(progress_sector_time(&read, 11) - 148.75f) < 0.02f);
    /* And a sector nobody has cleared stays uncleared rather than arriving as
     * a nought. */
    CHECK(progress_sector_time(&read, 5) == PROGRESS_NO_TIME);

    /* A buffer too small truncates rather than running off the end of one. */
    char cramped[8];
    size_t short_len = progress_serialize(&written, cramped, sizeof(cramped));
    CHECK(short_len < sizeof(cramped));
}

static void test_progress_file_damage_is_not_a_reset(void)
{
    Progress progress;
    progress_defaults(&progress);
    progress_note_score(&progress, 7700);
    progress_note_sector(&progress, 6);

    /* A comment, a blank line, a key from a build that knew more, a line with
     * no value at all, and a sector nobody could have reached. None of it may
     * take a number the file did not mention with it. */
    progress_parse(&progress,
                   "# written by some other version\n"
                   "\n"
                   "deaths 41\n"
                   "furthest_sector\n");
    CHECK(progress.best_score == 7700);
    CHECK(progress.furthest_sector == 6);

    /* Out-of-range values are clamped, never taken as written: the title
     * screen offers whatever this says, so a hand-edited file must not be able
     * to point it at a sector that does not exist. */
    /* The same rule for the two-field line: an index with no time after it is
     * the half-written line this whole function is about, and taking it would
     * file a nought — a perfect run — against a sector nobody had cleared. */
    progress_note_sector_time(&progress, 3, 90.0f);
    progress_parse(&progress, "sector_time 3\n");
    CHECK(progress_sector_time(&progress, 3) == 90.0f);
    progress_parse(&progress, "sector_time \n");
    CHECK(progress_sector_time(&progress, 3) == 90.0f);
    progress_parse(&progress, "sector_time 900 12.0\n");
    CHECK(progress_sector_time(&progress, 3) == 90.0f);
    progress_parse(&progress, "sector_time 3 nonsense\n");
    CHECK(progress_sector_time(&progress, 3) == 90.0f);

    progress_parse(&progress, "furthest_sector 100000\nbest_score -5\n");
    CHECK(progress.furthest_sector > 0 && progress.furthest_sector < 100000);
    CHECK(progress.best_score == 0);

    /* And an empty file is a fresh start, not a crash. */
    Progress empty;
    progress_defaults(&empty);
    progress_parse(&empty, "");
    progress_parse(&empty, NULL);
    progress_note_sector(NULL, 3);
    progress_note_score(NULL, 3);
    CHECK(empty.furthest_sector == 0);
    CHECK(empty.best_score == 0);
}

/*
 * The half-written line, which is the damage a player actually gets: a write
 * interrupted by a full disk or a pulled plug leaves the key behind with no
 * digits after it. strtol reads that as a nought, so both files used to answer
 * it by wiping the very number they exist to keep — a campaign and a best
 * score gone because a save was cut short, and the CRT filter switched off
 * because `crt ` is not `crt 1`.
 *
 * A key this build recognises with a value it cannot read is worth exactly
 * what a key it does not recognise is worth: nothing at all.
 */
static void test_a_value_that_is_not_a_number_changes_nothing(void)
{
    Progress progress;
    progress_defaults(&progress);
    progress_note_score(&progress, 7700);
    progress_note_sector(&progress, 11);

    progress_parse(&progress, "best_score whoops\n");
    CHECK(progress.best_score == 7700);
    progress_parse(&progress, "furthest_sector \n");
    CHECK(progress.furthest_sector == 11);
    progress_parse(&progress, "best_score\tnot-a-number\nfurthest_sector =\n");
    CHECK(progress.best_score == 7700);
    CHECK(progress.furthest_sector == 11);

    /* A number that is there is still read, and still clamped. */
    progress_parse(&progress, "best_score 9100\nfurthest_sector 12\n");
    CHECK(progress.best_score == 9100);
    CHECK(progress.furthest_sector == 12);

    Settings settings;
    settings_defaults(&settings);
    settings.music_volume = 70;
    settings.sfx_volume = 40;
    CHECK(settings.crt_filter);

    settings_parse(&settings, "music oops\nsfx \ncrt \nassist_lives x\n");
    CHECK(settings.music_volume == 70);
    CHECK(settings.sfx_volume == 40);
    CHECK(settings.crt_filter);
    CHECK(!settings.assist.infinite_lives);

    /* And a switch a file does spell out is still obeyed, both ways. */
    settings_parse(&settings, "crt 0\nassist_lives 1\n");
    CHECK(!settings.crt_filter);
    CHECK(settings.assist.infinite_lives);
}

int main(void)
{
    test_camera_axis_target();
    test_rng_is_reproducible();
    test_level_parser_and_seeded_choices();
    test_level_theme_metadata();
    test_campaign_themes_keep_changing();
    test_all_embedded_levels_parse();
    test_campaign_levels_are_distinct_and_solvable();
    test_no_sector_asks_for_a_long_walk_with_nothing_banked();
    test_the_restrooms_are_four_rooms_rather_than_one();
    test_every_sector_can_seat_the_reinforcements_it_can_call();
    test_a_sector_gives_the_player_a_moment_to_read_it();
    test_the_grace_period_is_not_an_empty_building();
    test_the_whole_frame_survives_a_monkey_on_the_controls();
    test_embedded_restroom_sublevels();
    test_every_restroom_theme_names_a_room_that_exists();
    test_every_theme_names_a_score_of_its_own();
    test_the_cordon_fades_as_the_climb_rises();
    test_editor_round_trips_every_map_file();
    test_editor_edits_and_undo();
    test_editor_resizes_deletes_and_survives_a_real_file();
    test_the_editor_has_nothing_to_say_about_the_shipped_campaign();
    test_the_loader_and_the_editor_survive_nonsense();
    test_the_editor_and_the_loader_read_a_spawns_line_the_same_way();
    test_editor_report_catches_broken_maps();
    test_editor_report_counts_dogs_and_lift_shafts();
    test_the_editor_reports_every_ceiling_the_loader_drops_at();
    test_the_editor_wants_a_floor_under_a_duct();
    test_the_editor_warns_when_a_floor_cannot_seat_its_reinforcements();
    test_editor_report_reads_the_campaign();
    test_gameplay_reset_preserves_rng_only();
    test_chase_is_reproducible_from_a_seed();
    test_chase_departure_hands_over_to_the_drive();
    test_chase_collision_costs_integrity_and_speed();
    test_chase_kerb_scrape_bleeds_speed_without_damage();
    test_chase_pedals_drive_the_car();
    test_chase_skip_answers_the_pad_letter();
    test_chase_holding_the_throttle_never_catches_the_suv();
    test_chase_lost_trail_restarts_the_pursuit();
    test_chase_wreck_restarts_the_pursuit();
    test_chase_stops_giving_road_back_once_it_offers_the_skip();
    test_chase_always_ends_even_for_a_player_who_only_accelerates();
    test_chase_surviving_the_drive_parks_at_the_building();
    test_chase_cross_traffic_obeys_the_signal();
    test_chase_generated_traffic_matches_its_lane();
    test_campaign_continue_flow();
    test_campaign_continue_countdown_expires();
    test_score_pays_out_extra_lives();
    test_the_sector_par_is_the_night_clock_s_own();
    test_a_sector_pays_for_the_clock_and_for_a_clean_run();
    test_blocked_exit_uses_separate_window();
    test_the_stair_door_is_a_way_out_only_once_it_opens();
    test_facade_mode_and_hazards_are_seeded();
    test_facade_bird_hits_player();
    test_facade_thrown_object_hits_player();
    test_a_facade_hazard_that_misses_is_cleaned_up();
    test_facade_ledges_block_and_are_routed_around();
    test_facade_ledge_stops_thrown_object_and_bird();
    test_embedded_facades_have_a_route_to_the_window();
    test_facade_checkpoint_banks_height();
    test_facade_wind_warns_then_pushes_unless_sheltered();
    test_facade_thrower_winds_up_before_releasing();
    test_level_collision_stops_at_wall();
    test_only_the_duct_answers_the_two_stances_differently();
    test_player_dies_from_a_high_fall();
    test_elevator_carries_an_off_centre_rider_through_a_slab();
    test_player_under_a_slab_is_crushed();
    test_ladder_mount_centres_the_player();
    test_player_descends_from_top_of_ladder();
    test_holding_down_enters_and_leaves_the_crawl();
    test_the_lid_of_a_shaft_is_not_somewhere_to_lie_down();
    test_a_shaft_is_left_by_its_mouths();
    test_every_ladder_in_the_campaign_can_be_climbed_down();
    test_a_jump_off_a_ladder_survives_a_held_climb_key();
    test_ladder_remembers_climb_direction_for_shooting();
    test_ladder_side_step_advances_the_animation_clock();
    test_a_busy_launcher_answers_the_trigger();
    test_every_ladder_throw_follows_the_aim();
    test_the_attack_poses_agree_with_what_is_drawn_on_them();
    test_every_underarm_throw_rises_at_the_cap();
    test_vertical_rocket_hits_targets();
    test_the_shot_line_is_chest_high();
    test_a_rocket_bursts_on_what_it_meets();
    test_a_crate_is_cover_and_a_lost_round_is_cleaned_up();
    test_level_reveal_finishes();
    test_event_buffer_reports_overflow();
    test_terminal_unlocks_deterministically();
    test_the_terminal_calls_its_reinforcements_under_an_alarm();
    test_the_men_a_console_calls_come_out_of_a_door();
    test_alarm_switch_parsing_and_quiet_timeout();
    test_guards_choose_attack_or_alarm_and_operate_switch();
    test_alarm_increases_guard_aggression_and_search();
    test_door_interaction_reports_range_and_teleports();
    test_sublevel_doors_are_not_paired_teleports();
    test_key_cards_keep_scoring_and_unlock_rules();
    test_the_live_card_is_never_silent();
    test_no_card_is_wrong_where_no_card_can_be();
    test_a_finished_hack_is_never_silent();
    test_mine_damage_emits_feedback();
    test_grenade_fuse_and_explosion_emit_sounds();
    test_a_throw_spends_one_grenade();
    test_only_the_magazine_comes_back();
    test_bazooka_pickup_and_rocket_explosion();
    test_player_can_switch_between_carried_weapons();
    test_weapon_cycle_runs_both_ways();
    test_the_weapon_ring_names_every_weapon_exactly_once();
    test_a_pickup_never_arms_itself();
    test_a_pickup_that_would_be_wasted_is_left_alone();
    test_a_sector_hands_its_explosives_to_the_next();
    test_every_climb_carries_an_explosive_out();
    test_no_climb_lays_out_a_pickup_it_cannot_use();
    test_gas_canister_requires_crawling_shot();
    test_a_guard_s_round_sets_off_a_gas_canister();
    test_every_blast_reaches_the_same_things();
    test_a_blast_sets_off_every_charge_it_reaches();
    test_a_blast_carries_through_a_wall();
    test_the_kill_tally_survives_a_reused_slot();
    test_reinforcements_take_a_fresh_slot_before_a_body();
    test_a_reinforcement_dog_takes_a_fresh_slot_before_a_body();
    test_a_reused_corpse_slot_is_forgotten_by_whoever_looked_at_it();
    test_a_body_falls_to_the_floor();
    test_a_fast_round_cannot_step_over_a_dog();
    test_a_duct_is_masonry_to_the_whole_building();
    test_weak_wall_only_opens_to_a_blast();
    test_the_route_model_crawls_a_duct_and_nothing_else();
    test_the_ducts_sector_can_actually_be_crawled_through();
    test_weak_wall_is_masonry_to_the_route_model();
    test_the_route_model_will_not_take_a_fatal_fall();
    test_the_route_model_promises_only_moves_the_player_can_make();
    test_empty_pistol_uses_close_range_knife();
    test_ladder_knife_attacks_in_aimed_direction();
    test_crate_movement_emits_sounds();
    test_a_crate_is_a_floor_a_wall_and_a_brake();
    test_a_dog_is_stopped_by_a_crate();
    test_a_blast_breaks_the_crates_it_reaches();
    test_a_falling_crate_kills_the_dog_under_it();
    test_crate_stops_at_enemy_and_triggers_counterattack();
    test_enemy_walks_in_front_of_unjumpable_crate();
    test_enemy_climbs_over_blocking_crate();
    test_patrol_enemy_may_walk_in_front_of_jumpable_crate();
    test_enemy_uses_ladder_while_avoiding_crate();
    test_patrol_enemy_does_not_immediately_leave_ladder();
    test_enemy_leaves_climb_state_when_landing_on_crate();
    test_enemy_aligns_before_vertical_climb();
    test_enemy_climbs_out_of_the_hole_at_a_ladder_top();
    test_enemy_leaves_a_ladder_that_already_reaches_a_floor();
    test_dog_escapes_ladder_perch_without_spinning();
    test_a_dog_jumps_a_gap_it_can_clear();
    test_a_dog_turns_back_at_a_gap_it_cannot_clear();
    test_a_dog_with_nothing_to_chase_roams_around_its_handler();
    test_a_dog_hunts_the_alarm_it_was_told_about();
    test_a_zeroed_guard_is_nobody_s_partner();
    test_two_calm_guards_standing_together_start_talking();
    test_a_pair_with_nowhere_to_stand_does_not_talk();
    test_the_janitor_walks_mops_and_turns_at_the_wall();
    test_hazards_emit_specific_impact_sounds();
    test_stomp_on_enemy_bounces_player_and_damages_it();
    test_a_stomp_has_to_come_from_above();
    test_a_heavy_cannot_be_stomped_but_can_still_be_knifed();
    test_a_heavy_walks_slower_than_a_guard();
    test_a_flash_charge_blinds_the_room_without_changing_it();
    test_a_flash_charge_stops_at_the_masonry();
    test_a_blast_kills_a_dog_in_reach();
    test_a_flash_charge_makes_a_camera_forget();
    test_a_flash_charge_reaches_the_dog_as_well();
    test_a_bolt_pulls_a_guard_to_where_it_landed();
    test_a_bolt_makes_its_noise_where_it_hit_the_wall();
    test_a_guard_who_never_saw_it_coming_goes_down_at_once();
    test_a_takedown_does_not_wake_the_man_he_was_talking_to();
    test_stomp_still_lands_during_the_mercy_window();
    test_ladder_descent_onto_enemy_bounces_instead_of_killing();
    test_enemy_spawn_uses_seeded_rng();
    test_janitor_ai_is_seeded_and_visual_only();
    test_civilians_flee_to_the_way_in_and_vanish();
    test_receptionist_works_a_post_and_returns_to_it();
    test_boxed_in_receptionist_stays_on_the_desk();
    test_walled_in_civilian_leaves_instead_of_running_on_the_spot();
    test_janitor_cart_stays_clear_when_turning_at_wall();
    test_enemy_vision_cone_stealth_and_walls();
    test_enemy_fires_vertical_shot_up_a_shaft();
    test_noise_draws_guards_to_investigate();
    test_guard_investigates_fallen_comrade();
    test_a_guard_notices_the_second_body_as_well();
    test_a_guard_who_finds_a_body_may_run_for_the_alarm();
    test_a_guard_sent_to_look_arrives_looks_and_goes_back();
    test_a_guard_investigates_a_fallen_dog();
    test_a_camera_sweeps_and_raises_the_alarm();
    test_a_camera_beam_actually_sweeps_both_ways();
    test_two_cameras_on_a_ceiling_sweep_out_of_step();
    test_a_camera_is_taken_out_by_a_shot_and_not_by_a_bolt();
    test_a_camera_takes_longer_to_be_sure_than_a_man_does();
    test_a_blast_takes_a_camera_with_it();
    test_a_dragged_body_stops_being_found_where_it_fell();
    test_a_dragged_dog_stops_being_found_where_it_fell();
    test_a_dragged_body_is_dropped_by_everything_that_should_drop_it();
    test_the_doorway_and_the_body_do_not_answer_the_same_press();
    test_pursuing_guard_hops_small_gap();
    test_pursuing_guard_searches_away_from_blocking_wall();
    test_pursuing_guards_route_up_from_wall_below_player();
    test_pursuing_guard_refuses_high_drop();
    test_guard_rides_elevator_and_leaves_at_target_floor();
    test_pursuing_guard_walks_onto_falling_platform();
    test_a_moving_platform_never_carries_the_rider_into_a_wall();
    test_a_moving_platform_runs_its_span_and_takes_its_rider();
    test_a_falling_platform_waits_its_beat_then_drops_its_rider();
    test_only_chucks_weight_arms_a_cracked_panel();
    test_no_sector_springs_its_own_panels();
    test_a_guard_in_a_one_tile_dead_end_stands_still();
    test_the_jump_apex_does_not_depend_on_the_frame_rate();
    test_coyote_time_allows_a_late_jump();
    test_jump_buffer_executes_on_landing();
    test_releasing_jump_cuts_the_rise();
    test_contact_costs_a_heart_with_mercy_window();
    test_explosion_costs_two_hearts();
    test_interior_checkpoint_resumes_progress();
    test_a_respawn_with_no_checkpoint_still_clears_the_air();
    test_guard_downed_in_combat_drops_ammo();
    test_body_falls_past_the_rungs();
    test_dog_bite_is_announced_and_survivable();
    test_chase_failure_rewinds_instead_of_restarting();
    test_chase_skippable_after_repeated_failures();
    test_fresh_sighting_waits_before_aiming();
    test_medkit_heals_before_granting_life();
    test_night_props_ask_for_the_right_wall();
    test_lone_guard_calls_in_without_going_blind();
    test_the_net_carries_words();
    test_crew_traffic_fits_the_plate();
    test_nobody_on_the_net_names_himself();
    test_the_net_always_has_something_to_say();
    test_credits_fit_the_frame();
    test_the_report_between_sectors_fits_its_column();
    test_the_sector_tally_fits_the_frame_it_is_drawn_in();
    test_every_sector_reports_or_tallies_and_none_does_neither();
    test_the_arc_lands_on_the_sectors_that_show_a_report();
    test_no_two_sectors_in_a_row_go_quiet();
    test_the_manual_draws_the_campaign_it_ships_with();
    test_the_manual_says_the_campaign_it_draws();
    test_the_credits_say_the_campaign_they_roll_over();
    test_the_net_counts_the_crew_it_has();
    test_manual_sheets_fit_the_column();
    test_no_radio_checks_while_the_alarm_is_up();
    test_chase_cordon_thickens_toward_the_building();
    test_chase_cordon_survives_a_crash();
    test_key_bindings_keep_one_key_to_one_job();
    test_every_bindable_key_fits_its_cap();
    test_every_bindable_button_fits_its_cap();
    test_the_run_tally_fits_the_frame_it_is_drawn_in();
    test_the_record_card_cells_fit_their_column();
    test_an_assisted_run_banks_no_records();
    test_clearing_the_records_keeps_the_resume();
    test_every_word_on_the_options_sheet_fits_the_plate();
    test_every_word_on_the_pause_sheet_fits_the_plate();
    test_a_binding_row_fits_the_plate();
    test_pad_buttons_keep_one_button_to_one_job();
    test_a_pad_is_read_by_the_letter_not_the_position();
    test_the_stick_answers_a_menu_like_a_d_pad();
    test_a_letter_and_a_button_find_each_other();
    test_a_prompt_is_spelled_for_the_pad_in_hand();
    test_no_sector_is_locked_behind_an_unbindable_action();
    test_bindings_survive_the_file();
    test_the_settings_file_fits_the_buffer_that_writes_it();
    test_settings_cursor_only_lands_on_rows();
    test_the_audio_heading_is_found_by_what_it_holds();
    test_settings_sliders_step_and_stop();
    test_every_options_row_moves_its_own_field();
    test_every_toggle_on_the_options_sheet_reports_its_own_field();
    test_a_settings_row_says_which_action_and_which_number_it_is();
    test_settings_survive_the_file();
    test_settings_file_damage_is_not_a_reset();
    test_progress_only_ever_climbs();
    test_every_interior_lays_out_exactly_one_docket_sheet();
    test_the_docket_sheet_costs_a_detour();
    test_a_docket_sheet_is_counted_by_the_run();
    test_the_veteran_run_is_three_numbers_and_no_more();
    test_the_night_clock_fills_the_night();
    test_a_sector_time_is_a_record_rather_than_a_number();
    test_progress_survives_the_file();
    test_progress_file_damage_is_not_a_reset();
    test_a_value_that_is_not_a_number_changes_nothing();

    if (failures != 0)
    {
        fprintf(stderr, "%d test check(s) failed\n", failures);
        return 1;
    }
    puts("all core tests passed");
    return 0;
}
