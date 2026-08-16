#include "camera.h"
#include "chase.h"
#include "credits.h"
#include "crew.h"
#include "demo.h"
#include "editor_doc.h"
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
#include "progress.h"
#include "rng.h"
#include "settings.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
    CHECK(EMBEDDED_LEVEL_COUNT == 15);
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
    CHECK(facade_levels == 4);
    CHECK(sublevel_entrances == 4);
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

        /* Which card opens the door and which terminal is live is decided by
         * the seed, so every one of them has to be gettable. */
        for (int item = 0; item < level->runtime.item_count; ++item)
        {
            if (level->runtime.items[item].type != ITEM_CARD)
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
         * fourteen, and that line moves the moment one room pays differently. */
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

#define CHASE_STEP (1.0f / 60.0f)

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
    campaign_reset(&campaign);
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
    campaign_reset(&campaign);
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
    campaign_reset(&campaign);
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
    CHECK(SECTOR_PAR_SECONDS == NIGHT_CLOCK_MINUTES_PER_SECTOR * 60.0f);
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
    campaign_reset(&campaign);
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus == (int)SECTOR_PAR_SECONDS * SECTOR_TIME_BONUS_PER_SECOND);
    CHECK(clean_bonus == SECTOR_CLEAN_BONUS);
    CHECK(campaign.score == time_bonus + clean_bonus);

    /* Half the slot spent, so half of it paid. */
    campaign_reset(&campaign);
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS * 0.5f;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus ==
          (int)(SECTOR_PAR_SECONDS * 0.5f) * SECTOR_TIME_BONUS_PER_SECOND);

    /* Exactly on par pays nothing for the clock, and a floor that ran over it
     * is not charged for the overrun — the bonus floors at nought rather than
     * going negative, because a sector must never be worth less than not
     * finishing it. */
    campaign_reset(&campaign);
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus == 0);

    campaign_reset(&campaign);
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS * 40.0f;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(time_bonus == 0);
    CHECK(campaign.score == SECTOR_CLEAN_BONUS);

    /* One death is enough to lose the clean bonus, and it does not touch what
     * the clock paid: they are two separate answers to two separate fields. */
    campaign_reset(&campaign);
    campaign.level_deaths = 1;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(clean_bonus == 0);
    CHECK(time_bonus > 0);
    CHECK(campaign.score == time_bonus);

    /* The score is added to, never assigned: a sector's pay stacks on the run
     * the player already has. */
    campaign_reset(&campaign);
    campaign.score = 1234;
    campaign.level_elapsed_time = SECTOR_PAR_SECONDS;
    campaign.level_deaths = 3;
    campaign_award_sector_bonus(&campaign, &time_bonus, &clean_bonus);
    CHECK(campaign.score == 1234);

    /* Both outputs are optional, for a caller that wants only the money. */
    campaign_reset(&campaign);
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
    campaign_reset(&campaign);
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
    campaign_reset(&campaign);
    CHECK(!campaign.sector_bonus_paid);
}

/*
 * The scripted hand, held to the two things that make it worth having.
 *
 * It is a testing tool, so nothing here asserts that it plays well — what it
 * has to do is press every control and press each of them *once*. The edge
 * half of that is the whole reason `demo_edge` exists: the first draft set the
 * edge fields from an interval, which at sixty steps a second is twelve
 * presses rather than one, and twelve presses of `switch_weapon` walk a
 * four-weapon cycle back to where it started. That failure was invisible —
 * the smoke stays silent either way — and only turned up as two drawings still
 * reading zero under `llvm-cov`. A press is a crossing, so this counts them.
 */
static void test_the_demo_hand_presses_every_control_once(void)
{
    static const char data[] =
        "##############\n"
        "#S      H   T#\n"
        "#########H####\n"
        "#   M    H  E#\n"
        "##############\n";
    GameplayState state = {0};
    rng_seed(&state.rng, 5150);
    REQUIRE(level_load_data(&state.level, "demo", data, strlen(data),
                            &state.rng));

    DemoHand hand;
    demo_hand_init(&hand);

    /* The kit is granted rather than found, for the reason written down in
     * demo.h: a scripted hand cannot be relied on to walk to a pickup, and two
     * of each is what one lap spends. */
    demo_grant_loadout(&state);
    CHECK(state.player.bullets == MAX_AMMO);
    CHECK(state.player.grenades == 2);
    CHECK(state.player.bazooka_rockets == 2);

    int shoots = 0, switches = 0, switches_back = 0, jumps = 0, doors = 0;
    int crawls = 0, climbs = 0, walks = 0;
    Input input;
    /* Two laps at the simulation's own step, which is what the real thing is
     * driven at. */
    const float step = SIM_STEP_DT;
    int steps = (int)(16.0f / step);
    for (int i = 0; i < steps; ++i)
    {
        demo_hand_drive(&hand, &state, &input, step);
        shoots += input.shoot;
        switches += input.switch_weapon;
        switches_back += input.switch_weapon_back;
        jumps += input.jump;
        doors += input.use_door;
        crawls += input.down;
        climbs += input.up;
        walks += input.left || input.right;
    }

    /* Every control is touched. `climbs` is not among them here and that is
     * the point of the pass below: nothing is stepping the simulation, so this
     * hand never actually arrives anywhere, and up is spent on arriving. */
    CHECK(shoots > 0);
    CHECK(switches > 0);
    CHECK(switches_back > 0);
    CHECK(jumps > 0);
    CHECK(doors > 0);
    CHECK(crawls > 0);
    CHECK(walks > 0);

    /* Parked on the rung it was walking at, the ladder half stops walking and
     * starts climbing — which is what puts the vertical shot within reach. */
    demo_hand_init(&hand);
    state.player.x = 9.0f * (float)TILE_SIZE;
    climbs = 0;
    walks = 0;
    for (int i = 0; i < steps; ++i)
    {
        demo_hand_drive(&hand, &state, &input, step);
        climbs += input.up;
    }
    CHECK(climbs > 0);

    /*
     * And the edges are edges. Two laps of a script that presses
     * `switch_weapon` four times a half is sixteen presses; a script that set
     * the field from an interval instead would count them in the thousands, so
     * a generous ceiling still catches the bug this is here for.
     */
    CHECK(switches <= 32);
    CHECK(switches_back <= 8);
    CHECK(jumps <= 16);
    CHECK(doors <= 8);
    CHECK(shoots <= 64);

    /* Up and down never arrive together: `player_ladder_attack_direction`
     * reads that as no aim at all, which would silently cost the vertical
     * shot the ladder half of the lap exists to fire. */
    demo_hand_init(&hand);
    state.player.on_ladder = true;
    for (int i = 0; i < steps; ++i)
    {
        demo_hand_drive(&hand, &state, &input, step);
        CHECK(!(input.up && input.down));
    }

    /* A climb answers none of this — there is nothing to shoot on a wall — so
     * the hand up there is movement and nothing else. */
    static const char wall[] =
        "....Y....\n"
        ".........\n"
        "..#####..\n"
        ".........\n"
        "....S....\n"
        "\n"
        "MODE FACADE\n";
    GameplayState climb = {0};
    rng_seed(&climb.rng, 5151);
    REQUIRE(level_load_data(&climb.level, "demo climb", wall, strlen(wall),
                            &climb.rng));
    CHECK(climb.level.map.mode == LEVEL_MODE_FACADE);

    demo_hand_init(&hand);
    for (int i = 0; i < steps; ++i)
    {
        demo_hand_drive(&hand, &climb, &input, step);
        CHECK(!input.shoot);
        CHECK(!input.switch_weapon);
        CHECK(!input.use_door);
        CHECK(input.up);
    }
}

/*
 * And the lap actually spends both grenades, which is the claim the loadout
 * above is only half of.
 *
 * `demo_grant_loadout` hands over two of each one-shot for a stated reason: each
 * is fired once from the floor and once from a rung, and the vertical throw is
 * its own drawing rather than the horizontal one rotated. The grant was honest
 * and the spend was not — the throw *cleared* `grenades` instead of decrementing
 * it, so the first throw destroyed the second grenade and
 * `render_figures.c`'s vertical grenade arm was reached by nothing in the tree,
 * in the one file written to reach exactly that kind of thing. Measured across
 * two laps: one throw before, two after.
 *
 * The test above drives the hand without stepping anything, which is why it
 * could not see this: nothing consumed a press, so no grenade was ever spent.
 * This one hands the presses to the combat module, which is the half that
 * counts. The rung is held rather than climbed to, because what is under test is
 * the aim and not the pathfinding — `player_ladder_attack_direction` returns
 * nought off a ladder, so a hand that never arrives fires horizontally forever.
 */
static void test_the_demo_lap_throws_a_grenade_both_ways(void)
{
    static const char data[] =
        "##############\n"
        "#S      H   T#\n"
        "#########H####\n"
        "#   M    H  E#\n"
        "##############\n";
    GameplayState state = {0};
    CampaignState campaign = {0};
    rng_seed(&state.rng, 5150);
    REQUIRE(level_load_data(&state.level, "demo", data, strlen(data),
                            &state.rng));
    player_reset(&state.player, &state.level);
    demo_grant_loadout(&state);

    DemoHand hand;
    demo_hand_init(&hand);

    int horizontal = 0;
    int vertical = 0;
    Input input;
    const float step = SIM_STEP_DT;
    int steps = (int)(16.0f / step);
    for (int i = 0; i < steps; ++i)
    {
        demo_hand_drive(&hand, &state, &input, step);
        state.player.on_ladder = true;
        int before = state.player.grenades;
        gameplay_combat_handle_player_action(&state, &campaign, &input);
        if (state.player.grenades >= before)
            continue;
        if (state.player.shot_vertical != 0)
            vertical++;
        else
            horizontal++;
    }

    /* Both poses drawn, and the pocket emptied rather than discarded. */
    CHECK(horizontal >= 1);
    CHECK(vertical >= 1);
    CHECK(horizontal + vertical == 2);
    CHECK(state.player.grenades == 0);
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

    gameplay_climb_update(&first, 0.016f);
    gameplay_climb_update(&second, 0.016f);
    CHECK(first.birds[0].active);
    CHECK(fabsf(first.birds[0].vx - second.birds[0].vx) < 0.0001f);

    /* The thrower shouts first, so its brick appears a beat later. */
    for (int frame = 0; frame < 60 && !first.thrown_objects[0].active; ++frame)
    {
        gameplay_climb_update(&first, 0.016f);
        gameplay_climb_update(&second, 0.016f);
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
    for (int frame = 0; frame < 60; ++frame)
        gameplay_climb_update_player(&state, &up, 0.05f);
    /* Stopped underneath the ledge rather than passing through it. */
    CHECK(state.player.y > 3 * (float)TILE_SIZE);
    float blocked_y = state.player.y;

    Input up_right = {.up = true, .right = true};
    for (int frame = 0; frame < 20; ++frame)
        gameplay_climb_update_player(&state, &up_right, 0.05f);
    CHECK(state.player.x > state.level.map.start_x);
    CHECK(state.player.y < blocked_y);

    for (int frame = 0; frame < 80; ++frame)
        gameplay_climb_update_player(&state, &up, 0.05f);
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

    gameplay_climb_update(&state, 0.016f);

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
static bool facade_bot_reaches_window(GameplayState *state, int max_frames)
{
    const float step = 0.05f;
    int scan_dir = -1;
    bool scanning = false;

    for (int frame = 0; frame < max_frames; ++frame)
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
        CHECK(facade_bot_reaches_window(&state,
                                        60 * state.level.map.height + 800));
    }
    CHECK(climbs == 4);
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
    for (int frame = 0; frame < 40; ++frame)
        gameplay_climb_update_player(&state, &up, 0.05f);
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
    for (int frame = 0; frame < 40; ++frame)
    {
        Input down = {.down = true};
        gameplay_climb_update_player(&state, &down, 0.05f);
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
    for (int frame = 0; frame < 1200 &&
                        state.facade_wind_phase != FACADE_WIND_WARNING;
         ++frame)
    {
        game_events_clear(&state.events);
        gameplay_climb_update(&state, 0.05f);
    }
    CHECK(state.facade_wind_phase == FACADE_WIND_WARNING);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND, SFX_WIND_GUST));
    /* The warning beat itself never pushes. */
    CHECK(gameplay_climb_wind_push(&state) == 0.0f);

    for (int frame = 0; frame < 120 &&
                        state.facade_wind_phase != FACADE_WIND_GUSTING;
         ++frame)
    {
        game_events_clear(&state.events);
        gameplay_climb_update(&state, 0.05f);
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
    gameplay_climb_update(&state, 0.016f);
    CHECK(state.facade_wind_sheltered);
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
    gameplay_climb_update(&state, 0.016f);
    /* The shout lands first and nothing is in the air yet. */
    CHECK(state.facade_hazard_windup_timers[0] > 0.0f);
    CHECK(!state.thrown_objects[0].active);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_GUARD_TALK));

    game_events_clear(&state.events);
    for (int frame = 0; frame < 60 && !state.thrown_objects[0].active; ++frame)
        gameplay_climb_update(&state, 0.016f);
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
    for (int i = 0; i < 20; ++i)
        level_move(&level, &x, &y, &vx, &vy,
                   PLAYER_W, PLAYER_H, 0.05f, false, &on_ground, false);

    CHECK(x + PLAYER_W <= 4.0f * TILE_SIZE + 0.01f);
    CHECK(fabsf(vx) < 0.01f);

    y -= 2.0f;
    vy = 100.0f;
    level_move(&level, &x, &y, &vx, &vy,
               PLAYER_W, PLAYER_H, 0.05f, false, &on_ground, false);
    CHECK(on_ground);
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

    for (int frame = 0; frame < 120 && !state.player.dying; ++frame)
    {
        bool was_grounded = state.player.on_ground;
        float fall_speed =
            player_update(&state.player, &state.level, &input, 1.0f / 60.0f);
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
        gameplay_ride_platforms(&state, 1.0f / 60.0f);
        CHECK(state.player_on_elevator == 0);
        bool topped_out = false;
        for (int frame = 0; frame < 240 && !state.player.dying && !topped_out;
             ++frame)
        {
            gameplay_carry_player_on_elevator(&state, 1.0f / 60.0f);
            gameplay_resolve_player_crush(&state);
            player_update(&state.player, &state.level, &idle, 1.0f / 60.0f);
            level_update_elevators(&state.level, 1.0f / 60.0f);
            gameplay_ride_platforms(&state, 1.0f / 60.0f);
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

        player_update(&player, &level, &up, 1.0f / 60.0f);
        CHECK(player.on_ladder);
        CHECK(fabsf(player.x - centred) < 0.01f);

        for (int frame = 0; frame < 240; ++frame)
            player_update(&player, &level, &up, 1.0f / 60.0f);
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

    player_update(&player, &level, &down, 1.0f / 60.0f);

    CHECK(player.on_ladder);
    CHECK(!player.crawling);
    CHECK(player.y > start_y);
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

    const float dt = 1.0f / 60.0f;
    Input climbing = {.up = true, .jump_held = true};
    player_update(&player, &level, &climbing, dt);
    CHECK(player.on_ladder);

    /* Climb a little first, so the jump is taken from inside the run rather
     * than off the bottom of it. */
    for (int frame = 0; frame < 10; ++frame)
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
    for (int frame = 0; frame < 8; ++frame)
    {
        player_update(&player, &level, &climbing, dt);
        CHECK(!player.on_ladder);
    }
    CHECK(launched_from - player.y > (float)TILE_SIZE);

    /* The lockout is a beat, not a ban: the rungs are still there afterwards,
     * so a jump up a shaft reads as a boost rather than as the ladder dying. */
    for (int frame = 0; frame < 30; ++frame)
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
        player_update(&state.player, &state.level, &climb, 1.0f / 60.0f);
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
        player_update(&state.player, &state.level, &climb, 1.0f / 60.0f);
        CHECK(state.player.on_ladder);
        CHECK(state.player.ladder_direction == -1);

        Input side_step = {
            .left = direction < 0,
            .right = direction > 0};
        player_update(&state.player, &state.level, &side_step,
                      1.0f / 60.0f);
        CHECK(state.player.on_ladder);
        CHECK(state.player.facing == direction);
        CHECK(state.player.ladder_direction == 0);

        Input idle = {0};
        player_update(&state.player, &state.level, &idle, 1.0f / 60.0f);
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

    const float dt = 1.0f / 60.0f;
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
    for (int i = 0; i < 30; ++i)
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

static void test_ladder_explosives_follow_aim_direction(void)
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
    for (int frame = 0; frame < 8; ++frame)
        gameplay_prepare_terminal(&quiet, &input, 1.0f / 60.0f);
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
                   state.level.map.enemy_spawns[0].y, &state.rng);
        state.enemies[0].dir = -1;
        state.enemies[0].on_ground = true;
        state.enemies[0].shoot_cooldown = 10.0f;

        gameplay_ai_update_combat(&state, 0.016f);
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
    enemy_init(enemy, 0.0f, 0.0f, &state.rng);
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
    state.level.runtime.items_remaining = 1;
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
    CHECK(state.level.runtime.items_remaining == 0);
}

/*
 * The live card is never the silent one.
 *
 * `gameplay_unlock_exit` says nothing when there is no door left to open — an
 * interior whose stair core is welded and whose route on is the window, or an
 * exit a finished hack already opened. Taken as the card's only voice, that
 * made the *right* card the one pickup in the game that answered with nothing
 * at all, in exactly the sectors where the strip reads BLOCKED and cannot
 * report it either, while a decoy went on buzzing. No shipped map puts a `C` in
 * a window sector, which is the only reason nobody has heard it; this is what
 * stops the next map finding out.
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
    blocked.level.runtime.items_remaining = 1;
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
    opens.level.runtime.items_remaining = 1;
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
 * a player already carrying one, so nobody playing the game ever holds two. The
 * demo hand does, on purpose: a grenade is thrown once from the floor and once
 * from a rung, the vertical throw is a drawing of its own rather than the
 * horizontal one rotated, and clearing the count on the first throw meant the
 * second never happened and that drawing was reached by nothing in the tree.
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

    for (int frame = 0; frame < 120 && state.rockets[0].active; ++frame)
        gameplay_combat_update_player_bullets(&state, &campaign,
                                              1.0f / 120.0f);

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
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);
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
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);

    input = (Input){.switch_weapon = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_GRENADE);

    input = (Input){.switch_weapon_back = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(!input.switch_weapon_back);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);

    input = (Input){.switch_weapon_back = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_PISTOL);

    /* With only the knife left, neither bumper can leave it. */
    state.player.bullets = 0;
    state.player.grenades = 0;
    state.player.active_weapon = PLAYER_WEAPON_KNIFE;
    input = (Input){.switch_weapon_back = true};
    gameplay_combat_handle_player_action(&state, &campaign, &input);
    CHECK(state.player.active_weapon == PLAYER_WEAPON_KNIFE);
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
 * mid-wall on every one of the four climbs is a pickup whose entire value is
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
    CHECK(climbs == 4);
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
    CHECK(climbs == 4);
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
    for (int step = 0; step < 12 && round->active; ++step)
        gameplay_combat_update_enemy_bullets(&state, &campaign, 0.016f);
    CHECK(canister->active);
    CHECK(round->x + BULLET_W < canister->x); /* it really did pass it */

    /* One that comes in low finds it. */
    round->active = true;
    round->x = canister->x + 48.0f;
    round->y = canister->y + GAS_CANISTER_H * 0.5f;
    game_events_clear(&state.events);
    for (int step = 0; step < 12 && canister->active; ++step)
        gameplay_combat_update_enemy_bullets(&state, &campaign, 0.016f);

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
    enemy_init(&mine_state.enemies[0], MINE_W * 0.5f, 10.0f, &mine_state.rng);
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
        enemy_init(&state.enemies[0], guard_x, guard_y, &state.rng);
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
        enemy_init(&state.enemies[0], guard_x, guard_y, &state.rng);
        state.enemy_count = 1;

        state.rockets[0] = (Rocket){
            .x = 2.0f * TILE_SIZE,
            .y = 1.0f * TILE_SIZE + (TILE_SIZE - ROCKET_H) * 0.5f,
            .vx = ROCKET_SPEED,
            .vy = 0.0f,
            .active = true};

        /* Let it fly into the wall. Small steps, so nothing steps over a tile. */
        for (int i = 0; i < 60 && state.rockets[0].active; ++i)
            gameplay_combat_update_player_bullets(&state, &campaign,
                                                  1.0f / 240.0f);

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
        enemy_init(&state.enemies[i], 64.0f, 32.0f, &state.rng);
        state.enemy_count++;
    }
    CHECK(state.enemy_count == MAX_ENEMIES);
    enemy_init(&state.enemies[0], 64.0f, 32.0f, &state.rng);
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
    enemy_init(&state.enemies[0], 2.0f * TILE_SIZE, 1.0f * TILE_SIZE,
               &state.rng);
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
        gameplay_ai_update_spawns(&state, 0.1f);
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

    enemy_init(&state.enemies[0], 4.0f * TILE_SIZE, 1.0f * TILE_SIZE,
               &state.rng);
    state.enemies[0].dead = true;
    state.enemy_count = 1;
    float dropped_from = state.enemies[0].y;

    for (int step = 0; step < 60; ++step)
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);

    CHECK(state.enemies[0].y > dropped_from);
    /* Standing on the floor slab of the two-row band, not through it. */
    CHECK(fabsf(state.enemies[0].y - (3.0f * TILE_SIZE - ENEMY_H)) < 1.0f);
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
    for (int frame = 0; frame < 60 && state.bullets[0].active; ++frame)
        gameplay_combat_update_player_bullets(&state, &campaign, 1.0f / 120.0f);
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
    for (int frame = 0; frame < 30; ++frame)
    {
        level_move(&state.level, &state.player.x, &state.player.y, &vx, &vy,
                   PLAYER_W, PLAYER_H, 1.0f / 60.0f, false, &on_ground, false);
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

    for (int i = 0; i < 240 && !state.enemies[0].dead; ++i)
        gameplay_update_crates(&state, &campaign, 1.0f / 120.0f);
    CHECK(state.enemies[0].dead);
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_ENEMY_DOWN));
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_CRATE_LAND));

    for (int i = 0; i < 240 && !crate->on_ground; ++i)
        gameplay_update_crates(&state, &campaign, 1.0f / 120.0f);
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
    for (int frame = 0; frame < 720; ++frame)
    {
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);
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
    for (int frame = 0; frame < 720; ++frame)
    {
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);
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
    for (int frame = 0; frame < 720; ++frame)
    {
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);
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
    enemy_init(&enemy, ladder_x, 3.0f * TILE_SIZE, &rng);
    enemy.dir = -1;
    enemy.on_ground = true;
    /* A crate collision starts this timer. It should suppress steering back
     * into the crate, but must not turn a required ladder into a random patrol
     * choice while the guard is pursuing a target on another floor. */
    enemy.obstacle_avoid_timer = ENEMY_OBSTACLE_AVOID_TIME;
    rng_seed(&rng, 1); /* The old random patrol check declines this ladder. */

    enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
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
    enemy_init(&enemy, ladder_x, 5.0f * TILE_SIZE, &rng);
    enemy.on_ground = true;
    enemy.climb_cooldown = 0.0f;

    /* This sequence accepts the patrol climb and would then accept a random
       side exit on the next frame, while still on the starting floor. */
    rng_seed(&rng, 389);
    enemy_update(&enemy, &level, 1.0f / 60.0f, false, false,
                 0.0f, 0.0f, false, 1.0f, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == -1);

    float climb_start_y = enemy.y;
    enemy_update(&enemy, &level, 1.0f / 60.0f, false, false,
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
    enemy_init(&enemy, ladder_x - 10.0f, 5.0f * TILE_SIZE, &rng);
    enemy.dir = -1;
    enemy.on_ground = true;

    enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                 level.map.start_x + PLAYER_W * 0.5f,
                 2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);
    CHECK(enemy.climbing);

    float climb_start_y = enemy.y;
    float off_ladder_x = enemy.x;
    enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                 level.map.start_x + PLAYER_W * 0.5f,
                 2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);

    CHECK(enemy.x > off_ladder_x);
    CHECK(fabsf(enemy.y - climb_start_y) < 0.01f);

    for (int frame = 0; frame < 480 && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 120.0f, true, false,
                     level.map.start_x + PLAYER_W * 0.5f,
                     2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);

    CHECK(!enemy.climbing);
    CHECK(fabsf(enemy.x - ladder_x) < 0.01f);
    CHECK(fabsf(enemy.y - 2.0f * TILE_SIZE) < 0.01f);

    enemy.on_ground = true;
    enemy.dir = 1;
    enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                 ladder_x + ENEMY_W * 0.5f,
                 5.0f * TILE_SIZE + ENEMY_H * 0.5f, false, 1.0f, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == 1);

    for (int frame = 0; frame < 480 && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 120.0f, true, false,
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
    enemy_init(&enemy, ladder_x, 4.0f * TILE_SIZE, &rng);
    enemy.on_ground = true;
    enemy.climb_cooldown = 0.0f;

    /* The player up on the storey the ladder reaches. */
    float target_x = level.map.start_x + PLAYER_W * 0.5f;
    float target_y = 2.0f * TILE_SIZE - PLAYER_H * 0.5f;

    for (int frame = 0; frame < 600 && !enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                     target_x, target_y, false, 1.0f, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == -1);

    for (int frame = 0; frame < 600 && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                     target_x, target_y, false, 1.0f, &rng);

    CHECK(!enemy.climbing);
    /* Standing on the slab, not inside the hole through it. */
    CHECK(fabsf(enemy.y - (2.0f * TILE_SIZE - ENEMY_H)) < 0.01f);

    /* And walking again: a guard boxed in at body height goes nowhere. */
    float stranded_x = enemy.x;
    for (int frame = 0; frame < 60; ++frame)
        enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
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
    enemy_init(&enemy, ladder_x, 4.0f * TILE_SIZE, &rng);
    enemy.on_ground = true;
    enemy.climb_cooldown = 0.0f;

    /* The player standing on the block's roof, to the right of the ladder. */
    float target_x = 4.5f * TILE_SIZE;
    float target_y = 3.0f * TILE_SIZE - PLAYER_H * 0.5f;

    for (int frame = 0; frame < 600 && !enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                     target_x, target_y, false, 1.0f, &rng);
    CHECK(enemy.climbing);

    for (int frame = 0; frame < 600 && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
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
    for (int frame = 0; frame < 180; ++frame)
    {
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);
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
    for (int frame = 0; frame < 300 && !state.enemies[0].dead; ++frame)
    {
        player_update(&state.player, &state.level, &down, 1.0f / 60.0f);
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
    for (int frame = 0; frame < 600; ++frame)
    {
        gameplay_ai_update_movement(&first, 1.0f / 60.0f);
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
    for (int frame = 0; frame < 600; ++frame)
    {
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);
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
        JANITOR_W + JANITOR_CART_SIDE_EXTENT, JANITOR_H));
    CHECK(!gameplay_box_tiles_clear(
        &state, janitor->x, janitor->y,
        JANITOR_W + JANITOR_CART_SIDE_EXTENT, JANITOR_H));

    janitor->activity = JANITOR_WALK;
    janitor->activity_timer = 100.0f;
    for (int frame = 0; frame < 30; ++frame)
    {
        gameplay_ai_update_movement(&state, 0.05f);
        float collision_x = janitor->cart_dir > 0
                                ? janitor->x - JANITOR_CART_SIDE_EXTENT
                                : janitor->x;
        CHECK(gameplay_box_tiles_clear(
            &state, collision_x, janitor->y,
            JANITOR_W + JANITOR_CART_SIDE_EXTENT, JANITOR_H));
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
    for (int frame = 0; frame < 2400; ++frame)
    {
        gameplay_ai_update_movement(&first, 0.05f);
        gameplay_ai_update_movement(&second, 0.05f);
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
    for (int frame = 0; frame < 1200; ++frame)
    {
        gameplay_ai_update_movement(&state, 0.05f);
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
    gameplay_ai_update_combat(&state, 0.016f);
    CHECK(guard->aim_timer > 0.0f);

    guard->aim_timer = 0.0f;
    guard->shoot_cooldown = 0.0f;
    state.player.crawling = true;
    gameplay_ai_update_combat(&state, 0.016f);
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
    gameplay_ai_update_combat(&walled, 0.016f);
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

    gameplay_ai_update_combat(&state, 0.016f);
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

    gameplay_ai_update_combat(&state, 0.016f);
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

    gameplay_ai_update_combat(&state, 0.016f);
    CHECK((witness->bodies_investigated & enemy_body_bit(1, false)) != 0);

    /* Done with the first: clear what the sighting started, so nothing but the
     * mask can be what stops him reacting again. */
    witness->investigate_timer = 0.0f;
    witness->raising_alarm = false;
    witness->alarm_switch_index = -1;
    CHECK(!gameplay_alarm_active(&state));

    /* The same corpse is not worth a second walk. */
    gameplay_ai_update_combat(&state, 0.016f);
    CHECK(witness->investigate_timer <= 0.0f && !witness->raising_alarm);

    /* A different one is. */
    state.enemies[2].dead = true;
    gameplay_ai_update_combat(&state, 0.016f);
    CHECK((witness->bodies_investigated & enemy_body_bit(2, false)) != 0);
    CHECK(witness->investigate_timer > 0.0f || witness->raising_alarm);
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

    gameplay_ai_update_movement(&state, 0.016f);
    CHECK(!guard->on_ground);
    CHECK(guard->vy < 0.0f); /* leapt the gap instead of stalling at the edge */

    bool reached_far_side = false;
    for (int frame = 0; frame < 180; ++frame)
    {
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);
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
    for (int frame = 0; frame < 480 &&
                        guard->x < wall_turn_x - 0.01f;
         ++frame)
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);
    CHECK(fabsf(guard->x - wall_turn_x) < 1.0f);

    for (int frame = 0; frame < 60; ++frame)
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);

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

    for (int frame = 0; frame < 1800; ++frame)
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);

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
               level.map.enemy_spawns[0].y, &rng);
    guard.on_ground = true;
    guard.dir = 1;
    float ledge_y = guard.y;
    bool turned_back = false;

    for (int frame = 0; frame < 180; ++frame)
    {
        enemy_update(&guard, &level, 1.0f / 120.0f, true, false,
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
               6.0f * TILE_SIZE - ENEMY_H, &rng);
    guard.dir = 1;
    guard.on_ground = true;

    /* At a shaft leading toward the target floor, the guard waits for and
     * boards the aligned platform instead of jumping across the opening. */
    elevator->y = elevator->bot_limit - TILE_SIZE;
    float waiting_x = guard.x;
    enemy_update(&guard, &level, 0.016f, true, false,
                 5.5f * TILE_SIZE,
                 elevator->top_limit - ENEMY_H * 0.5f,
                 false, 1.0f, &rng);
    CHECK(guard.on_elevator == -1);
    CHECK(guard.on_ground);
    CHECK(fabsf(guard.x - waiting_x) < 0.01f);

    elevator->y = elevator->bot_limit;
    enemy_update(&guard, &level, 0.016f, true, false,
                 5.5f * TILE_SIZE,
                 elevator->top_limit - ENEMY_H * 0.5f,
                 false, 1.0f, &rng);
    CHECK(guard.on_elevator == 0);
    CHECK(guard.on_ground);
    CHECK(fabsf(guard.y - (elevator->y - ENEMY_H)) < 0.01f);
    float riding_x = guard.x;

    for (int frame = 0; frame < 30; ++frame)
    {
        level_update_elevators(&level, 0.016f);
        enemy_update(&guard, &level, 0.016f, true, false,
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
    for (int frame = 0; frame < 60 && guard.on_elevator >= 0; ++frame)
    {
        enemy_update(&guard, &level, 1.0f / 120.0f, true, false,
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
               level.map.enemy_spawns[0].y, &rng);
    guard.on_ground = true;
    guard.dir = 1;
    float previous_x = guard.x;

    enemy_update(&guard, &level, 0.016f, true, false,
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
                                     PLAYER_W, (float)PLAYER_H));

    for (int frame = 0; frame < 240; ++frame)
    {
        platform->x = platform->right_limit;
        gameplay_ride_platforms(&state, 1.0f / 60.0f);
        REQUIRE(state.player_on_moving_platform == 0);
        /* The whole claim, every frame: still aboard, still in air. */
        CHECK(gameplay_box_tiles_clear(&state, state.player.x, state.player.y,
                                       PLAYER_W, (float)PLAYER_H));
    }

    /* And the guard must not have turned the ride off: out in the middle of the
     * run, where there is air to be carried into, the rider still travels. */
    platform->x = platform->left_limit + TILE_SIZE;
    platform->vx = MOVING_PLATFORM_SPEED;
    state.player.x = platform->x + (TILE_SIZE - PLAYER_W) * 0.5f;
    state.player.y = ride_y;
    state.player.vy = 0.0f;
    float before = state.player.x;
    gameplay_ride_platforms(&state, 1.0f / 60.0f);
    CHECK(state.player_on_moving_platform == 0);
    CHECK(state.player.x > before);
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

    for (int late_frames = 2; late_frames <= 14; late_frames += 12)
    {
        GameplayState state = {0};
        rng_seed(&state.rng, 11);
        CHECK(level_load_data(&state.level, "ledge", data, strlen(data),
                              &state.rng));
        player_reset(&state.player, &state.level);

        Input run = {.right = true};
        int frame = 0;
        while (frame < 200 &&
               (state.player.on_ground || state.player.vy <= 0.0f))
        {
            player_update(&state.player, &state.level, &run, 1.0f / 60.0f);
            frame++;
        }
        CHECK(frame < 200);

        Input idle = {0};
        for (int wait = 0; wait < late_frames; ++wait)
            player_update(&state.player, &state.level, &idle, 1.0f / 60.0f);

        Input jump = {.jump = true, .jump_held = true};
        player_update(&state.player, &state.level, &jump, 1.0f / 60.0f);
        if (late_frames * (1.0f / 60.0f) < PLAYER_COYOTE_TIME)
        {
            CHECK(state.player.jumped);
            CHECK(state.player.vy < 0.0f);
        }
        else
        {
            CHECK(!state.player.jumped);
            CHECK(state.player.vy > 0.0f);
        }
    }
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
    for (int frame = 0; frame < 300 && !jumped; ++frame)
    {
        input.jump = false;
        if (!pressed && state.player.vy > 0.0f &&
            floor_stand_y - state.player.y < 22.0f)
        {
            input.jump = true;
            input.jump_held = true;
            pressed = true;
        }
        player_update(&state.player, &state.level, &input, 1.0f / 60.0f);
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
    for (int frame = 0; frame < 30 && !state.player.on_ground; ++frame)
        player_update(&state.player, &state.level, &idle, 1.0f / 60.0f);
    CHECK(state.player.on_ground);

    Input jump = {.jump = true, .jump_held = true};
    player_update(&state.player, &state.level, &jump, 1.0f / 60.0f);
    CHECK(state.player.jumped);
    float full_rise = state.player.vy;
    CHECK(full_rise < -PLAYER_JUMP_SPEED * 0.9f);

    Input released = {0};
    player_update(&state.player, &state.level, &released, 1.0f / 60.0f);
    CHECK(state.player.vy >=
          -PLAYER_JUMP_SPEED * PLAYER_JUMP_CUT_FACTOR - 0.001f);

    /* The bounce off a stomped guard is not player-started: releasing the
     * key must not shorten it back down into the guard. */
    state.player.jump_cut_ok = false;
    state.player.vy = -ENEMY_STOMP_BOUNCE_SPEED;
    player_update(&state.player, &state.level, &released, 1.0f / 60.0f);
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
    gameplay_collect_items(&state, &campaign, 1.0f / 60.0f);
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
    gameplay_climb_restore_checkpoint(&climb);
    CHECK(!climb.thrown_objects[0].active);
    CHECK(!climb.birds[0].active);
    CHECK(climb.player.y == start_y);
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
    gameplay_combat_update_player_bullets(&state, &campaign, 1.0f / 60.0f);
    CHECK(state.enemies[0].dead);
    CHECK(state.ammo_drops[0].active);

    /* A full sidearm leaves the magazine lying where it fell. */
    state.player.hp = PLAYER_MAX_HP;
    state.player.bullets = MAX_AMMO;
    state.player.x = state.ammo_drops[0].x - 4.0f;
    state.player.y = 2.0f * TILE_SIZE - PLAYER_H;
    gameplay_update_ammo_drops(&state, 1.0f / 60.0f);
    CHECK(state.ammo_drops[0].active);

    state.player.bullets = 0;
    gameplay_update_ammo_drops(&state, 1.0f / 60.0f);
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
    for (int i = 0; i < 120; ++i)
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);

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
    gameplay_ai_update_movement(&state, 0.05f);
    CHECK(state.dogs[0].bite_windup == 0.0f);
    CHECK(!state.dogs[0].bite_ready);

    /* Standing in it, the growl becomes a bite one windup later. */
    state.player.x = held_x;
    state.dogs[0].x = held_x;
    gameplay_combat_check_contacts(&state, &campaign);
    CHECK(state.dogs[0].bite_windup > 0.0f);
    for (int i = 0; i < 10 && !state.dogs[0].bite_ready; ++i)
    {
        state.dogs[0].x = state.player.x;
        gameplay_ai_update_movement(&state, 0.05f);
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
    while (guard->aim_timer <= 0.0f && steps < 10)
    {
        gameplay_ai_update_combat(&state, 0.1f);
        steps++;
    }
    CHECK(guard->aim_timer > 0.0f);
    /* 0.35s of notice at 0.1s a step lands the first aim on the fourth. */
    CHECK(steps == 4);

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
    gameplay_ai_update_combat(&provoked, 0.016f);
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
    gameplay_collect_items(&state, &campaign, 1.0f / 60.0f);
    CHECK(state.player.hp == PLAYER_MAX_HP);
    CHECK(campaign.lives == 0);
    CHECK(state.interior_has_checkpoint);

    state.level.runtime.items[0].collected = false;
    gameplay_collect_items(&state, &campaign, 1.0f / 60.0f);
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
    for (int step = 0; step < 60 * 60 && !called; ++step)
    {
        game_events_clear(&state.events);
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);
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
    for (int step = 0; step < 60 * 6 && guard->talking; ++step)
    {
        game_events_clear(&state.events);
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);
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
    for (int step = 0; step < 60 * 60 && spoke == NULL; ++step)
    {
        game_events_clear(&state.events);
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);
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
 * MARCO.` and `KARL: KARL, ANSWER YOUR HANDSET!`. Rare is not the same as
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
     * KARL is on the docket, so this is the case the scan has to get right. */
    CHECK(crew_line_said_by(CHATTER_RADIO, 0, "KARL") != NULL);
    CHECK(!test_line_names("KARLSSON WANTS THE LIGHTS ON.", "KARL"));
    CHECK(test_line_names("TELL KARL NO.", "KARL"));
    CHECK(test_line_names("KARL!", "KARL"));
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
    /* And it comes to rest inside the dwell the smoke run gives this screen.
     * `tools/smoke.sh` reads CREDITS_MAX_DURATION out of the header, so a roll
     * that grew past it would be a screen the only thing that executes it stops
     * short of — silently, and on the beat a finished campaign ends on. */
    CHECK(roll.duration <= CREDITS_MAX_DURATION);
    CHECK(!credits_at_rest(&roll));

    bool finished = false;
    for (int step = 0; step < 60 * 300 && !finished; ++step)
        finished = credits_update(&roll, 1.0f / 60.0f);
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

    for (int step = 0; step < 60 * 30; ++step)
    {
        game_events_clear(&state.events);
        /* Hold the alarm up for the whole window. */
        state.terminal_alarm_timer = ALARM_CALM_TIME;
        gameplay_ai_update_movement(&state, 1.0f / 60.0f);
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

        for (int step = 0; step < 60 * 40; ++step)
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
        for (int step = 0; step < 60 * 20; ++step)
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
        for (int step = 0; step < 60 * 60; ++step)
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

    /* Taking a key takes it off whoever had it. Binding LEFT's arrow onto the
     * jump has to leave LEFT holding only A — a key that fires two actions is
     * indistinguishable from the game being broken. */
    CHECK(keybind_set(&b, BIND_JUMP, 0, 80));
    CHECK(keybind_action_has(&b, BIND_JUMP, 80));
    CHECK(!keybind_action_has(&b, BIND_LEFT, 80));
    CHECK(keybind_action_has(&b, BIND_LEFT, 4));

    /* An action may be emptied, and that is the player's business rather than
     * an error: the sheet shows it as an empty cap with the reset row beside
     * it. */
    CHECK(keybind_set(&b, BIND_WEAPON_PREV, 0, 4));
    CHECK(!keybind_action_has(&b, BIND_LEFT, 4));
    CHECK(b.keys[BIND_LEFT][0] == KEYBIND_NONE);
    CHECK(b.keys[BIND_LEFT][1] == KEYBIND_NONE);

    /* And putting a key the action already holds into its other slot must not
     * leave it holding the same key twice. */
    keybind_defaults(&b);
    CHECK(keybind_set(&b, BIND_LEFT, 1, 80));
    CHECK(b.keys[BIND_LEFT][1] == 80);
    CHECK(b.keys[BIND_LEFT][0] == KEYBIND_NONE);

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
    };
    for (size_t i = 0; i < sizeof(LOOSE) / sizeof(LOOSE[0]); ++i)
    {
        CHECK(LOOSE[i][0] != '\0');
        CHECK(SETTINGS_LABEL_X + (float)strlen(LOOSE[i]) * SETTINGS_GLYPH_W <=
              right);
    }
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

    /* Taking a button from another action clears it there. */
    CHECK(keybind_set_pad(&bindings, BIND_JUMP, 0, 3)); /* jump onto Y */
    CHECK(keybind_action_has_pad(&bindings, BIND_JUMP, 3));
    CHECK(!keybind_action_has_pad(&bindings, BIND_USE, 3));

    /* And from the other slot of the same action, which is the case that
     * leaves an action answering one button twice and a slot short. */
    CHECK(keybind_set_pad(&bindings, BIND_SHOOT, 0, 1)); /* X slot takes B */
    CHECK(keybind_action_has_pad(&bindings, BIND_SHOOT, 1));
    CHECK(bindings.pad[BIND_SHOOT][1] == PADBIND_NONE);

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

/* Bindings survive the file, and a damaged binding line is not a reset — the
 * same rule every other value in this file keeps. */
static void test_bindings_survive_the_file(void)
{
    Settings written;
    settings_defaults(&written);
    CHECK(keybind_set(&written.bindings, BIND_JUMP, 0, 29));      /* Z */
    CHECK(keybind_set(&written.bindings, BIND_SHOOT, 1, 224));    /* LCTRL */
    /* An emptied action, which has to survive as empty rather than coming back
     * as the default. */
    CHECK(keybind_set(&written.bindings, BIND_WEAPON_PREV, 0, 20)); /* Q */
    CHECK(keybind_set(&written.bindings, BIND_WEAPON_PREV, 1, 43)); /* TAB */

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

    /* Whatever WEAPON_NEXT was left holding after Q and TAB were taken off it
     * has to come back the same, empty slots included. */
    CHECK(read.bindings.keys[BIND_WEAPON_NEXT][0] == KEYBIND_NONE);

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
 * The campaign is fifteen sectors and a prologue, which is more than one
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

static void test_progress_survives_the_file(void)
{
    Progress written;
    progress_defaults(&written);
    progress_note_score(&written, 43120);
    progress_note_sector(&written, 11);

    char text[256];
    size_t len = progress_serialize(&written, text, sizeof(text));
    CHECK(len > 0 && len < sizeof(text));
    CHECK(text[len] == '\0');

    Progress read;
    progress_defaults(&read);
    progress_parse(&read, text);
    CHECK(read.best_score == written.best_score);
    CHECK(read.furthest_sector == written.furthest_sector);

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
    test_embedded_restroom_sublevels();
    test_every_restroom_theme_names_a_room_that_exists();
    test_editor_round_trips_every_map_file();
    test_editor_edits_and_undo();
    test_editor_report_catches_broken_maps();
    test_editor_report_counts_dogs_and_lift_shafts();
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
    test_the_demo_hand_presses_every_control_once();
    test_the_demo_lap_throws_a_grenade_both_ways();
    test_blocked_exit_uses_separate_window();
    test_facade_mode_and_hazards_are_seeded();
    test_facade_bird_hits_player();
    test_facade_ledges_block_and_are_routed_around();
    test_facade_ledge_stops_thrown_object_and_bird();
    test_embedded_facades_have_a_route_to_the_window();
    test_facade_checkpoint_banks_height();
    test_facade_wind_warns_then_pushes_unless_sheltered();
    test_facade_thrower_winds_up_before_releasing();
    test_level_collision_stops_at_wall();
    test_player_dies_from_a_high_fall();
    test_elevator_carries_an_off_centre_rider_through_a_slab();
    test_player_under_a_slab_is_crushed();
    test_ladder_mount_centres_the_player();
    test_player_descends_from_top_of_ladder();
    test_every_ladder_in_the_campaign_can_be_climbed_down();
    test_a_jump_off_a_ladder_survives_a_held_climb_key();
    test_ladder_remembers_climb_direction_for_shooting();
    test_ladder_side_step_advances_the_animation_clock();
    test_a_busy_launcher_answers_the_trigger();
    test_ladder_explosives_follow_aim_direction();
    test_vertical_rocket_hits_targets();
    test_level_reveal_finishes();
    test_event_buffer_reports_overflow();
    test_terminal_unlocks_deterministically();
    test_the_terminal_calls_its_reinforcements_under_an_alarm();
    test_alarm_switch_parsing_and_quiet_timeout();
    test_guards_choose_attack_or_alarm_and_operate_switch();
    test_alarm_increases_guard_aggression_and_search();
    test_door_interaction_reports_range_and_teleports();
    test_sublevel_doors_are_not_paired_teleports();
    test_key_cards_keep_scoring_and_unlock_rules();
    test_the_live_card_is_never_silent();
    test_mine_damage_emits_feedback();
    test_grenade_fuse_and_explosion_emit_sounds();
    test_a_throw_spends_one_grenade();
    test_only_the_magazine_comes_back();
    test_bazooka_pickup_and_rocket_explosion();
    test_player_can_switch_between_carried_weapons();
    test_weapon_cycle_runs_both_ways();
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
    test_a_body_falls_to_the_floor();
    test_a_fast_round_cannot_step_over_a_dog();
    test_weak_wall_only_opens_to_a_blast();
    test_weak_wall_is_masonry_to_the_route_model();
    test_the_route_model_will_not_take_a_fatal_fall();
    test_empty_pistol_uses_close_range_knife();
    test_ladder_knife_attacks_in_aimed_direction();
    test_crate_movement_emits_sounds();
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
    test_hazards_emit_specific_impact_sounds();
    test_stomp_on_enemy_bounces_player_and_damages_it();
    test_a_stomp_has_to_come_from_above();
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
    test_pursuing_guard_hops_small_gap();
    test_pursuing_guard_searches_away_from_blocking_wall();
    test_pursuing_guards_route_up_from_wall_below_player();
    test_pursuing_guard_refuses_high_drop();
    test_guard_rides_elevator_and_leaves_at_target_floor();
    test_pursuing_guard_walks_onto_falling_platform();
    test_a_moving_platform_never_carries_the_rider_into_a_wall();
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
    test_manual_sheets_fit_the_column();
    test_no_radio_checks_while_the_alarm_is_up();
    test_chase_cordon_thickens_toward_the_building();
    test_chase_cordon_survives_a_crash();
    test_key_bindings_keep_one_key_to_one_job();
    test_every_bindable_key_fits_its_cap();
    test_every_bindable_button_fits_its_cap();
    test_every_word_on_the_options_sheet_fits_the_plate();
    test_a_binding_row_fits_the_plate();
    test_pad_buttons_keep_one_button_to_one_job();
    test_bindings_survive_the_file();
    test_the_settings_file_fits_the_buffer_that_writes_it();
    test_settings_cursor_only_lands_on_rows();
    test_the_audio_heading_is_found_by_what_it_holds();
    test_settings_sliders_step_and_stop();
    test_settings_survive_the_file();
    test_settings_file_damage_is_not_a_reset();
    test_progress_only_ever_climbs();
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
