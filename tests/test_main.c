#include "embedded_levels.h"
#include "game_event.h"
#include "gameplay_ai.h"
#include "gameplay_combat.h"
#include "gameplay_interaction.h"
#include "gameplay_physics.h"
#include "gameplay_state.h"
#include "level.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            failures++;                                                        \
        }                                                                      \
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

static void test_all_embedded_levels_parse(void)
{
    CHECK(EMBEDDED_LEVEL_COUNT > 0);
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
        CHECK(level.map.has_exit);
    }
}

static void test_gameplay_reset_preserves_rng_only(void)
{
    GameplayState state = {0};
    rng_seed(&state.rng, 4567);
    Rng expected = state.rng;
    state.enemy_count = 3;
    state.janitor_count = 2;
    state.grenade_count = 2;
    state.terminal_hacking = true;
    state.events.count = 4;
    state.player_on_elevator = 5;
    state.player_on_moving_platform = 6;

    gameplay_state_begin_level(&state);

    CHECK(state.rng.state == expected.state);
    CHECK(state.enemy_count == 0);
    CHECK(state.janitor_count == 0);
    CHECK(state.grenade_count == 0);
    CHECK(!state.terminal_hacking);
    CHECK(state.events.count == 0);
    CHECK(state.player_on_elevator == -1);
    CHECK(state.player_on_moving_platform == -1);
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
    CHECK(events_have_sound(&state.events, GAME_EVENT_WORLD_SOUND,
                            SFX_TERMINAL_ALARM));
    CHECK(gameplay_advance_terminal(&state, &campaign,
                                    TERMINAL_HACK_TIME));
    CHECK(state.level.runtime.exit_unlocked);
    CHECK(state.level.runtime.terminal_hacked);
    CHECK(campaign.score == 250);
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

int main(void)
{
    test_rng_is_reproducible();
    test_level_parser_and_seeded_choices();
    test_all_embedded_levels_parse();
    test_gameplay_reset_preserves_rng_only();
    test_level_collision_stops_at_wall();
    test_level_reveal_finishes();
    test_event_buffer_reports_overflow();
    test_terminal_unlocks_deterministically();
    test_key_cards_keep_scoring_and_unlock_rules();
    test_mine_damage_emits_feedback();
    test_grenade_fuse_and_explosion_emit_sounds();
    test_crate_movement_emits_sounds();
    test_hazards_emit_specific_impact_sounds();
    test_enemy_spawn_uses_seeded_rng();
    test_janitor_ai_is_seeded_and_visual_only();

    if (failures != 0)
    {
        fprintf(stderr, "%d test check(s) failed\n", failures);
        return 1;
    }
    puts("all core tests passed");
    return 0;
}
