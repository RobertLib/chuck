#include "embedded_levels.h"
#include "game_event.h"
#include "gameplay_ai.h"
#include "gameplay_combat.h"
#include "gameplay_interaction.h"
#include "gameplay_physics.h"
#include "gameplay_state.h"
#include "gameplay_world.h"
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
        CHECK(level.map.alarm_switch_count >= 2);

        int bazooka_count = 0;
        for (int item = 0; item < level.runtime.item_count; ++item)
            if (level.runtime.items[item].type == ITEM_BAZOOKA)
                bazooka_count++;
        CHECK(bazooka_count == (((i + 1) % 2 == 0) ? 1 : 0));
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
    CHECK(state.active_alarm_switch == -1);
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

static void test_ladder_knife_is_horizontal_only(void)
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
        state.enemies[0].hp = ENEMY_HP;
        game_events_clear(&state.events);
        input = (Input){
            .up = vertical < 0,
            .down = vertical > 0,
            .shoot = true};

        gameplay_combat_handle_player_action(&state, &campaign, &input);

        CHECK(!input.shoot);
        CHECK(!state.player.knife_attacking);
        CHECK(state.player.action_timer == 0.0f);
        CHECK(state.enemies[0].hp == ENEMY_HP);
        CHECK(!events_have_sound(&state.events, GAME_EVENT_SOUND,
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

static void test_enemy_moves_away_from_blocking_crate(void)
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

    float contact_x = crate->x + CRATE_W;
    for (int frame = 0; frame < 480 && enemy->x > contact_x + 0.01f;
         ++frame)
    {
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);
    }
    CHECK(fabsf(enemy->x - contact_x) < 0.02f);

    for (int frame = 0; frame < 120; ++frame)
        gameplay_ai_update_movement(&state, 1.0f / 120.0f);

    CHECK(enemy->x > contact_x + 20.0f);
    CHECK(enemy->obstacle_avoid_timer > 0.0f);
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
                 false, &rng);

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
                 0.0f, 0.0f, false, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == -1);

    float climb_start_y = enemy.y;
    enemy_update(&enemy, &level, 1.0f / 60.0f, false, false,
                 0.0f, 0.0f, false, &rng);

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

    gameplay_resolve_enemy_crates(&state, enemy,
                                  enemy->x, crate->y - ENEMY_H);

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
                 2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, &rng);
    CHECK(enemy.climbing);

    float climb_start_y = enemy.y;
    float off_ladder_x = enemy.x;
    enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                 level.map.start_x + PLAYER_W * 0.5f,
                 2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, &rng);

    CHECK(enemy.x > off_ladder_x);
    CHECK(fabsf(enemy.y - climb_start_y) < 0.01f);

    for (int frame = 0; frame < 480 && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 120.0f, true, false,
                     level.map.start_x + PLAYER_W * 0.5f,
                     2.0f * TILE_SIZE + ENEMY_H * 0.5f, false, &rng);

    CHECK(!enemy.climbing);
    CHECK(fabsf(enemy.x - ladder_x) < 0.01f);
    CHECK(fabsf(enemy.y - 2.0f * TILE_SIZE) < 0.01f);

    enemy.on_ground = true;
    enemy.dir = 1;
    enemy_update(&enemy, &level, 1.0f / 60.0f, true, false,
                 ladder_x + ENEMY_W * 0.5f,
                 5.0f * TILE_SIZE + ENEMY_H * 0.5f, false, &rng);
    CHECK(enemy.climbing);
    CHECK(enemy.climb_dir == 1);

    for (int frame = 0; frame < 480 && enemy.climbing; ++frame)
        enemy_update(&enemy, &level, 1.0f / 120.0f, true, false,
                     ladder_x + ENEMY_W * 0.5f,
                     5.0f * TILE_SIZE + ENEMY_H * 0.5f,
                     false, &rng);

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

    CHECK(flips <= 3);                            /* no frantic spinning */
    CHECK(dog->y > perch_y + TILE_SIZE * 0.5f);   /* dropped off the rung */
    CHECK(fabsf(dog->x - perch_x) > TILE_SIZE);   /* left the ladder column */
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
            fired_up = true;
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
    witness->dir = 1; /* face the neighbouring guard, away from Chuck */
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
                 guard.y + ENEMY_H * 0.5f, false, &rng);

    CHECK(guard.on_ground);
    CHECK(fabsf(guard.vy) < 0.01f);
    CHECK(guard.x > previous_x); /* walked instead of jumping over it */
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
    test_alarm_switch_parsing_and_quiet_timeout();
    test_guards_choose_attack_or_alarm_and_operate_switch();
    test_alarm_increases_guard_aggression_and_search();
    test_door_interaction_reports_range_and_teleports();
    test_key_cards_keep_scoring_and_unlock_rules();
    test_mine_damage_emits_feedback();
    test_grenade_fuse_and_explosion_emit_sounds();
    test_bazooka_pickup_and_rocket_explosion();
    test_gas_canister_requires_crawling_shot();
    test_empty_pistol_uses_close_range_knife();
    test_ladder_knife_is_horizontal_only();
    test_crate_movement_emits_sounds();
    test_crate_stops_at_enemy_and_triggers_counterattack();
    test_enemy_moves_away_from_blocking_crate();
    test_enemy_uses_ladder_while_avoiding_crate();
    test_patrol_enemy_does_not_immediately_leave_ladder();
    test_enemy_leaves_climb_state_when_landing_on_crate();
    test_enemy_aligns_before_vertical_climb();
    test_dog_escapes_ladder_perch_without_spinning();
    test_hazards_emit_specific_impact_sounds();
    test_enemy_spawn_uses_seeded_rng();
    test_janitor_ai_is_seeded_and_visual_only();
    test_enemy_vision_cone_stealth_and_walls();
    test_enemy_fires_vertical_shot_up_a_shaft();
    test_noise_draws_guards_to_investigate();
    test_guard_investigates_fallen_comrade();
    test_pursuing_guard_hops_small_gap();
    test_pursuing_guard_walks_onto_falling_platform();

    if (failures != 0)
    {
        fprintf(stderr, "%d test check(s) failed\n", failures);
        return 1;
    }
    puts("all core tests passed");
    return 0;
}
