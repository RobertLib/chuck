#include "camera.h"
#include "chase.h"
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
#include "level.h"
#include "level_route.h"
#include "rng.h"

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

static void test_embedded_restroom_sublevel(void)
{
    CHECK(EMBEDDED_SUBLEVEL_COUNT == 1);
    Level restroom;
    Rng rng;
    rng_seed(&rng, 2026);
    CHECK(level_load_data(&restroom, EMBEDDED_SUBLEVELS[0].name,
                          EMBEDDED_SUBLEVELS[0].data,
                          EMBEDDED_SUBLEVELS[0].size, &rng));
    CHECK(restroom.map.theme == LEVEL_THEME_RESTROOM);
    CHECK(restroom.map.has_sublevel_return);
    CHECK(!restroom.map.has_exit);
    CHECK(restroom.map.door_count == 0);

    int basins = 0;
    int urinals = 0;
    int open_stalls = 0;
    int closed_stalls = 0;
    for (int i = 0; i < restroom.map.decoration_count; ++i)
    {
        basins += restroom.map.decorations[i].type ==
                  DECOR_RESTROOM_BASIN;
        urinals += restroom.map.decorations[i].type ==
                   DECOR_RESTROOM_URINAL;
        open_stalls += restroom.map.decorations[i].type ==
                       DECOR_RESTROOM_STALL_OPEN;
        closed_stalls += restroom.map.decorations[i].type ==
                         DECOR_RESTROOM_STALL_CLOSED;
    }
    CHECK(basins == 3);
    CHECK(urinals == 2);
    CHECK(open_stalls == 2);
    CHECK(closed_stalls == 1);

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

    /* The side room earns its detour: it is guarded, it has an upper service
     * walkway to climb to, and it has something to shove and something that
     * explodes. */
    CHECK(restroom.map.enemy_count == 1);
    CHECK(restroom.map.janitor_count == 1);
    CHECK(restroom.runtime.crate_count == 1);
    CHECK(restroom.runtime.gas_canister_count == 1);

    int ladder_tiles = 0;
    for (int row = 0; row < restroom.map.height; ++row)
        for (int col = 0; col < restroom.map.width; ++col)
            ladder_tiles += restroom.map.tiles[row][col] == TILE_LADDER;
    CHECK(ladder_tiles >= 4);

    /* Both walkway pickups have to sit above the floor the door is on, or
     * the climb up is decorative. */
    int high_items = 0;
    for (int i = 0; i < restroom.runtime.item_count; ++i)
        if (restroom.runtime.items[i].y <
            restroom.map.sublevel_return_row * (float)TILE_SIZE)
            high_items++;
    CHECK(high_items == 2);

    /*
     * And high up is not the same as gettable.
     *
     * Every campaign sector is walked by the route model; this room is the one
     * place that model never ran, because it is not a campaign sector — so the
     * medkit sat across a two-tile gap under a two-row ceiling for a while.
     * That is a jump the legend says is not on and the model agrees is not on,
     * and a player could in fact only land it inside a 25px window of where
     * they started the run-up. The whole reason to spend the detour must not
     * be a timing trick, so the room answers the same question the sectors do.
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
    CHECK(editor_path_level_number("levels/sublevels/restroom.txt") == 0);
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
                  "THEME FACADE_DAWN\n",
                  NULL, &report);
    CHECK(report_mentions(&report, ED_SEV_ERROR, "cannot be climbed to"));
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

    /* The restroom is not a sector, so none of the campaign rules apply to it
     * and it still has to be finishable. */
    CHECK(editor_doc_parse(&doc, EMBEDDED_SUBLEVELS[0].data,
                           EMBEDDED_SUBLEVELS[0].size));
    snprintf(doc.path, sizeof(doc.path), "levels/sublevels/restroom.txt");
    CHECK(editor_doc_build_level(&doc, &level, 3));
    editor_validate(&doc, &level, true, &campaign, &report);
    CHECK(report.route_valid);
    CHECK(report.goal_reached);
    CHECK(report.errors == 0);
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
    CHECK(ammo != NULL);
    CHECK(grenade != NULL);

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
    CHECK(state.player.active_weapon == PLAYER_WEAPON_BAZOOKA);
    CHECK(events_have_sound(&state.events, GAME_EVENT_SOUND,
                            SFX_PICKUP_BAZOOKA));

    /* The unique pickup stays consumed; it cannot supply repeated rockets. */
    gameplay_collect_items(&state, &campaign, ITEM_RESPAWN_TIME * 2.0f);
    CHECK(bazooka->collected);

    game_events_clear(&state.events);
    state.player.facing = 1;
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
    CHECK(witness->alerted_by_body);
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

/* ---- Forgiveness and fairness ----------------------------------------- */

/* A jump pressed a beat after the boots leave the ledge still happens; the
 * same press once the window has closed does not. */
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

int main(void)
{
    test_camera_axis_target();
    test_rng_is_reproducible();
    test_level_parser_and_seeded_choices();
    test_level_theme_metadata();
    test_campaign_themes_keep_changing();
    test_all_embedded_levels_parse();
    test_campaign_levels_are_distinct_and_solvable();
    test_embedded_restroom_sublevel();
    test_editor_round_trips_every_map_file();
    test_editor_edits_and_undo();
    test_editor_report_catches_broken_maps();
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
    test_ladder_remembers_climb_direction_for_shooting();
    test_ladder_side_step_advances_the_animation_clock();
    test_a_busy_launcher_answers_the_trigger();
    test_ladder_explosives_follow_aim_direction();
    test_vertical_rocket_hits_targets();
    test_level_reveal_finishes();
    test_event_buffer_reports_overflow();
    test_terminal_unlocks_deterministically();
    test_alarm_switch_parsing_and_quiet_timeout();
    test_guards_choose_attack_or_alarm_and_operate_switch();
    test_alarm_increases_guard_aggression_and_search();
    test_door_interaction_reports_range_and_teleports();
    test_sublevel_doors_are_not_paired_teleports();
    test_key_cards_keep_scoring_and_unlock_rules();
    test_mine_damage_emits_feedback();
    test_grenade_fuse_and_explosion_emit_sounds();
    test_only_the_magazine_comes_back();
    test_bazooka_pickup_and_rocket_explosion();
    test_player_can_switch_between_carried_weapons();
    test_weapon_cycle_runs_both_ways();
    test_gas_canister_requires_crawling_shot();
    test_every_blast_reaches_the_same_things();
    test_the_kill_tally_survives_a_reused_slot();
    test_reinforcements_take_a_fresh_slot_before_a_body();
    test_a_body_falls_to_the_floor();
    test_a_fast_round_cannot_step_over_a_dog();
    test_weak_wall_only_opens_to_a_blast();
    test_weak_wall_is_masonry_to_the_route_model();
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
    test_dog_escapes_ladder_perch_without_spinning();
    test_hazards_emit_specific_impact_sounds();
    test_stomp_on_enemy_bounces_player_and_damages_it();
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
    test_pursuing_guard_hops_small_gap();
    test_pursuing_guard_searches_away_from_blocking_wall();
    test_pursuing_guards_route_up_from_wall_below_player();
    test_pursuing_guard_refuses_high_drop();
    test_guard_rides_elevator_and_leaves_at_target_floor();
    test_pursuing_guard_walks_onto_falling_platform();
    test_coyote_time_allows_a_late_jump();
    test_jump_buffer_executes_on_landing();
    test_releasing_jump_cuts_the_rise();
    test_contact_costs_a_heart_with_mercy_window();
    test_explosion_costs_two_hearts();
    test_interior_checkpoint_resumes_progress();
    test_guard_downed_in_combat_drops_ammo();
    test_dog_bite_is_announced_and_survivable();
    test_chase_failure_rewinds_instead_of_restarting();
    test_chase_skippable_after_repeated_failures();
    test_fresh_sighting_waits_before_aiming();
    test_medkit_heals_before_granting_life();
    test_night_props_ask_for_the_right_wall();
    test_lone_guard_calls_in_without_going_blind();
    test_no_radio_checks_while_the_alarm_is_up();
    test_chase_cordon_thickens_toward_the_building();

    if (failures != 0)
    {
        fprintf(stderr, "%d test check(s) failed\n", failures);
        return 1;
    }
    puts("all core tests passed");
    return 0;
}
