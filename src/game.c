#include "game.h"
#include "embedded_levels.h"
#include "gameplay_ai.h"
#include "gameplay_combat.h"
#include "gameplay_interaction.h"
#include "gameplay_physics.h"
#include "gameplay_world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void game_enter_state(Game *game, GameState next_state);

static void update_camera_shake(Game *game, float dt)
{
    if (game->presentation.camera_shake_timer <= 0.0f ||
        game->presentation.camera_shake_duration <= 0.0f)
    {
        game->presentation.camera_shake_timer = 0.0f;
        game->presentation.camera_shake_x = 0.0f;
        game->presentation.camera_shake_y = 0.0f;
        return;
    }

    game->presentation.camera_shake_timer -= dt;
    if (game->presentation.camera_shake_timer <= 0.0f)
    {
        game->presentation.camera_shake_timer = 0.0f;
        game->presentation.camera_shake_x = 0.0f;
        game->presentation.camera_shake_y = 0.0f;
        return;
    }

    float fade = game->presentation.camera_shake_timer / game->presentation.camera_shake_duration;
    float amplitude = game->presentation.camera_shake_strength * fade * fade;
    float random_x = (float)SDL_rand(2001) / 1000.0f - 1.0f;
    float random_y = (float)SDL_rand(2001) / 1000.0f - 1.0f;
    game->presentation.camera_shake_x = roundf(random_x * amplitude);
    game->presentation.camera_shake_y = roundf(random_y * amplitude);
}

static void dispatch_game_events(Game *game)
{
    GameEventBuffer *events = &game->gameplay.events;
    float player_height = game->gameplay.player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;
    float listener_x = game->gameplay.player.x + PLAYER_W * 0.5f;
    float listener_y = game->gameplay.player.y + player_height * 0.5f;

    for (int i = 0; i < events->count; ++i)
    {
        const GameEvent *event = &events->items[i];
        switch (event->type)
        {
        case GAME_EVENT_SOUND:
            audio_play(&game->platform.audio, event->data.sound.effect);
            break;
        case GAME_EVENT_WORLD_SOUND:
            audio_play_at(&game->platform.audio, event->data.sound.effect,
                          event->data.sound.x, event->data.sound.y,
                          listener_x, listener_y);
            break;
        case GAME_EVENT_PARTICLES:
            particle_system_emit(&game->presentation.particles,
                                 event->data.particles.x,
                                 event->data.particles.y,
                                 event->data.particles.count,
                                 event->data.particles.direction);
            break;
        case GAME_EVENT_EXPLOSION:
            particle_system_explosion(&game->presentation.particles,
                                      event->data.explosion.x,
                                      event->data.explosion.y,
                                      event->data.explosion.count);
            break;
        case GAME_EVENT_CAMERA_SHAKE:
            game->presentation.camera_shake_strength =
                event->data.shake.strength;
            game->presentation.camera_shake_duration =
                event->data.shake.duration;
            game->presentation.camera_shake_timer =
                event->data.shake.duration;
            break;
        }
    }

    if (events->overflowed)
        SDL_Log("Gameplay event buffer overflowed");
}

static void reset_level_presentation(Game *game)
{
    game->presentation.message_timer = 0.0f;
    game->presentation.exit_unlocked_timer = 0.0f;
    game->presentation.camera_shake_timer = 0.0f;
    game->presentation.camera_shake_duration = 0.0f;
    game->presentation.camera_shake_strength = 0.0f;
    game->presentation.camera_shake_x = 0.0f;
    game->presentation.camera_shake_y = 0.0f;
    game->presentation.footstep_audio_timer = 0.0f;
    game->presentation.ladder_audio_timer = 0.0f;
    game->presentation.footstep_alternate = false;
    SDL_memset(game->presentation.fall_platform_sounded, 0,
               sizeof(game->presentation.fall_platform_sounded));
}

static void reset_sublevel_visit(Game *game)
{
    memset(&game->inactive_gameplay, 0, sizeof(game->inactive_gameplay));
    game->sublevel_initialized = false;
    game->in_sublevel = false;
    game->main_level_cam_x = 0.0f;
}

static void transfer_player_loadout(Player *destination,
                                    const Player *source)
{
    destination->bullets = source->bullets;
    destination->grenades = source->grenades;
    destination->bazooka_rockets = source->bazooka_rockets;
    destination->facing = source->facing;
}

static void snap_camera_to_player(Game *game)
{
    int win_w = 0, win_h = 0;
    game_get_view_size(game, &win_w, &win_h);
    (void)win_h;
    float desired = game->gameplay.player.x + PLAYER_W * 0.5f -
                    (float)win_w * 0.5f;
    float max_cam = game->gameplay.level.map.width * (float)TILE_SIZE -
                    (float)win_w;
    if (max_cam < 0.0f)
        max_cam = 0.0f;
    if (desired < 0.0f)
        desired = 0.0f;
    if (desired > max_cam)
        desired = max_cam;
    game->presentation.cam_x = desired;
}

static bool initialize_restroom(Game *game)
{
    if (EMBEDDED_SUBLEVEL_COUNT == 0)
    {
        SDL_Log("No embedded restroom sublevel is available");
        return false;
    }

    GameplayState *restroom = &game->inactive_gameplay;
    memset(restroom, 0, sizeof(*restroom));
    restroom->rng = game->gameplay.rng;
    gameplay_state_begin_level(restroom);

    const EmbeddedLevelData *source = &EMBEDDED_SUBLEVELS[0];
    if (!level_load_data(&restroom->level, source->name,
                         source->data, source->size, &restroom->rng))
    {
        SDL_Log("Could not load restroom sublevel");
        return false;
    }

    player_reset(&restroom->player, &restroom->level);
    level_reveal_init(&restroom->level);
    level_reveal_step(&restroom->level, 10.0f);
    gameplay_ai_spawn_level_entities(restroom);
    game->sublevel_initialized = true;
    return true;
}

static void swap_gameplay_areas(Game *game)
{
    GameplayState temporary = game->gameplay;
    game->gameplay = game->inactive_gameplay;
    game->inactive_gameplay = temporary;
}

static bool enter_restroom(Game *game)
{
    if (game->in_sublevel)
        return false;
    if (!game->sublevel_initialized && !initialize_restroom(game))
        return false;

    Player travelling_player = game->gameplay.player;
    game->main_level_cam_x = game->presentation.cam_x;
    swap_gameplay_areas(game);
    transfer_player_loadout(&game->gameplay.player, &travelling_player);
    game->gameplay.teleport_cooldown = TELEPORT_COOLDOWN;
    game_events_clear(&game->gameplay.events);
    game_events_sound(&game->gameplay.events, SFX_DOOR);
    particle_system_clear(&game->presentation.particles);
    game->in_sublevel = true;
    snap_camera_to_player(game);
    return true;
}

static bool leave_restroom(Game *game)
{
    if (!game->in_sublevel)
        return false;

    Player travelling_player = game->gameplay.player;
    swap_gameplay_areas(game);
    transfer_player_loadout(&game->gameplay.player, &travelling_player);
    game->gameplay.teleport_cooldown = TELEPORT_COOLDOWN;
    game_events_clear(&game->gameplay.events);
    game_events_sound(&game->gameplay.events, SFX_DOOR);
    particle_system_clear(&game->presentation.particles);
    game->in_sublevel = false;
    game->presentation.cam_x = game->main_level_cam_x;
    return true;
}

static bool load_level(Game *game, int index)
{
    if (index < 0 || (size_t)index >= EMBEDDED_LEVEL_COUNT)
    {
        SDL_Log("Level index %d is out of range", index);
        return false;
    }

    reset_sublevel_visit(game);

    gameplay_state_begin_level(&game->gameplay);
    const EmbeddedLevelData *source = &EMBEDDED_LEVELS[index];
    if (!level_load_data(&game->gameplay.level, source->name,
                         source->data, source->size, &game->gameplay.rng))
    {
        return false;
    }
    game->campaign.current_level = index;
    int gameplay_track_count = MUSIC_TRACK_COUNT - MUSIC_LEVEL_ONE;
    MusicTrack track = (MusicTrack)(MUSIC_LEVEL_ONE +
                                    index % gameplay_track_count);
    audio_play_music(&game->platform.audio, track);

    player_reset(&game->gameplay.player, &game->gameplay.level);

    game->campaign.level_elapsed_time = 0.0f;
    game->campaign.level_start_score = game->campaign.score;
    reset_level_presentation(game);
    game_enter_state(game, STATE_LEVEL_START);

    /* Initialise per-door state */
    for (int i = 0; i < game->gameplay.level.map.door_count; ++i)
    {
        game->gameplay.door_spawns[i] = game->gameplay.level.map.door_spawn_counts[i];
        /* Stagger initial spawn times so doors don't all fire at once */
        game->gameplay.door_timers[i] =
            DOOR_SPAWN_INTERVAL *
            (0.4f + rng_range(&game->gameplay.rng, 60) * 0.01f);
    }
    /* Keep the existing window size; initialise camera to centre on player. */
    int win_w = 0, win_h = 0;
    game_get_view_size(game, &win_w, &win_h);
    float max_cam = game->gameplay.level.map.width * (float)TILE_SIZE - (float)win_w;
    if (max_cam < 0.0f)
        max_cam = 0.0f;
    float desired = game->gameplay.player.x + PLAYER_W * 0.5f - (float)win_w * 0.5f;
    if (desired < 0.0f)
        desired = 0.0f;
    if (desired > max_cam)
        desired = max_cam;
    game->presentation.cam_x = desired;

    return true;
}

static void restart_game(Game *game)
{
    game->campaign.lives = PLAYER_LIVES;
    game->campaign.score = 0;
    load_level(game, 0);
    audio_play(&game->platform.audio, SFX_MENU_START);
}

bool game_init(Game *game)
{
    return game_init_seeded(game, (uint64_t)time(NULL));
}

bool game_init_seeded(Game *game, uint64_t seed)
{
    SDL_zerop(game);
    rng_seed(&game->gameplay.rng, seed);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_CreateWindowAndRenderer("Chuck", 800, 552, 0,
                                     &game->platform.window, &game->platform.renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    /* Use a fixed logical presentation so the game's coordinate system stays
     * consistent when the window is resized or when toggling fullscreen.
     * This makes the game look identical but scaled when entering fullscreen. */
    SDL_SetRenderLogicalPresentation(game->platform.renderer, 800, 552, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_SetRenderVSync(game->platform.renderer, 1);
    SDL_srand(SDL_GetTicksNS());

    game->campaign.lives = PLAYER_LIVES;
    game->campaign.score = 0;
    if (!load_level(game, 0))
    {
        SDL_DestroyRenderer(game->platform.renderer);
        game->platform.renderer = NULL;
        SDL_DestroyWindow(game->platform.window);
        game->platform.window = NULL;
        SDL_Quit();
        return false;
    }

    /* Sound is optional. If no playback device exists, gameplay stays intact. */
    audio_init(&game->platform.audio);
    audio_play_music(&game->platform.audio, MUSIC_INTRO);

    /* Initialise particle system */
    particle_system_init(&game->presentation.particles);

    /* Boot straight to the title screen; the opening cutscene now plays after
     * the player presses START. */
    game_enter_state(game, STATE_INTRO);

    game->platform.last_tick = SDL_GetTicksNS();
    return true;
}

void game_return_to_intro(Game *game)
{
    particle_system_clear(&game->presentation.particles);
    audio_stop_effects(&game->platform.audio);
    audio_play_music(&game->platform.audio, MUSIC_INTRO);
    audio_play(&game->platform.audio, SFX_MENU_BACK);
    game_enter_state(game, STATE_INTRO);
}

static void advance_level(Game *game)
{
    if ((size_t)(game->campaign.current_level + 1) < EMBEDDED_LEVEL_COUNT)
    {
        load_level(game, game->campaign.current_level + 1);
    }
    else
    {
        audio_stop_music(&game->platform.audio);
        audio_stop_effects(&game->platform.audio);
        audio_play_music(&game->platform.audio, MUSIC_INTRO);
        game_enter_state(game, STATE_OUTRO);
    }
}

static void finish_player_death(Game *game)
{
    /* Apply the actual hit effects after the death animation */
    game->gameplay.player.dying = false;
    game->gameplay.player.death_timer = 0.0f;
    particle_system_clear(&game->presentation.particles);

    game->campaign.lives -= 1;
    game->gameplay.invuln_timer = INVULN_TIME;

    if (game->campaign.lives <= 0)
    {
        audio_stop_music(&game->platform.audio);
        game_enter_state(game, STATE_GAME_OVER);
        game_events_sound(&game->gameplay.events, SFX_GAME_OVER);
    }
    else
    {
        player_reset(&game->gameplay.player, &game->gameplay.level);
        game_events_sound(&game->gameplay.events, SFX_RESPAWN);
    }
}

static void clear_edge_input(Game *game)
{
    game->input.jump = false;
    game->input.shoot = false;
    game->input.use_door = false;
    game->input.confirm = false;
    game->input.restart = false;
}

static void game_enter_state(Game *game, GameState next_state)
{
    switch (next_state)
    {
    case STATE_OPENING_CUTSCENE:
        opening_cutscene_init(&game->presentation.opening_cutscene);
        break;
    case STATE_INTRO:
    {
        int win_w = 0;
        int win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        intro_init(&game->presentation.intro, win_w, win_h);
        break;
    }
    case STATE_LEVEL_START:
        level_reveal_init(&game->gameplay.level);
        break;
    case STATE_OUTRO:
        outro_cutscene_init(&game->presentation.outro_cutscene);
        break;
    case STATE_LEVEL_CLEARED:
        game->presentation.message_timer = 1.2f;
        break;
    case STATE_SHOW_KEYCARD:
    case STATE_PLAYING:
    case STATE_LEVEL_TRANSITION:
    case STATE_GAME_OVER:
        break;
    }
    game->state = next_state;
}

static bool update_scene(Game *game, float dt)
{

    if (game->state == STATE_OPENING_CUTSCENE)
    {
        Uint32 cues = 0;
        bool finished =
            opening_cutscene_update(&game->presentation.opening_cutscene, dt, &cues);

        if (cues & OPENING_CUE_RAIN)
            audio_play(&game->platform.audio, SFX_OPENING_RAIN);
        if (cues & OPENING_CUE_SUV_ENGINE)
            audio_play(&game->platform.audio, SFX_OPENING_SUV_ENGINE);
        if (cues & OPENING_CUE_CAR_ENGINE)
            audio_play(&game->platform.audio, SFX_OPENING_CAR_ENGINE);
        if (cues & (OPENING_CUE_SUV_BRAKE | OPENING_CUE_CAR_BRAKE))
            audio_play(&game->platform.audio, SFX_OPENING_BRAKE);
        if (cues & OPENING_CUE_CAR_DOOR)
            audio_play(&game->platform.audio, SFX_OPENING_CAR_DOOR);
        if (cues & (OPENING_CUE_ESCORT_STEP_A | OPENING_CUE_CHUCK_STEP_A))
            audio_play(&game->platform.audio, SFX_STEP_A);
        if (cues & (OPENING_CUE_ESCORT_STEP_B | OPENING_CUE_CHUCK_STEP_B))
            audio_play(&game->platform.audio, SFX_STEP_B);
        if (cues & OPENING_CUE_BUILDING_DOOR)
            audio_play(&game->platform.audio, SFX_DOOR);

        if (finished || game->input.confirm)
        {
            /* Cutscene over: begin the campaign on level one. */
            audio_stop_effects(&game->platform.audio);
            restart_game(game);
        }
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_INTRO)
    {
        float win_x = 0.0f, win_y = 0.0f, mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&win_x, &win_y);
        SDL_RenderCoordinatesFromWindow(game->platform.renderer, win_x, win_y, &mx, &my);

        int win_w = 0, win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        intro_update(&game->presentation.intro, dt, win_w, win_h, mx, my);

        if (game->input.confirm)
        {
            /* START pressed: play the opening cutscene before gameplay begins. */
            game->input.confirm = false;
            game_enter_state(game, STATE_OPENING_CUTSCENE);
        }
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_LEVEL_TRANSITION)
    {
        Uint32 cues = 0;
        bool finished =
            level_transition_update(&game->presentation.level_transition, dt, &cues);

        if (cues & LEVEL_TRANSITION_CUE_STEP_A)
            audio_play(&game->platform.audio, SFX_STEP_A);
        if (cues & LEVEL_TRANSITION_CUE_STEP_B)
            audio_play(&game->platform.audio, SFX_STEP_B);
        if (cues & (LEVEL_TRANSITION_CUE_DOOR_OPEN |
                    LEVEL_TRANSITION_CUE_DOOR_CLOSE))
            audio_play(&game->platform.audio, SFX_DOOR);

        if (finished || game->input.confirm)
        {
            audio_stop_effects(&game->platform.audio);
            advance_level(game);
        }
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_OUTRO)
    {
        if (game->input.restart &&
            game->presentation.outro_cutscene.time >= OUTRO_FINAL_REVEAL_TIME)
        {
            restart_game(game);
            clear_edge_input(game);
            return true;
        }

        /*
         * Skipping preserves the happy ending instead of dropping the player
         * onto a separate results screen.
         */
        if (game->input.confirm &&
            game->presentation.outro_cutscene.time < OUTRO_FINAL_REVEAL_TIME)
        {
            audio_stop_effects(&game->platform.audio);
            game->presentation.outro_cutscene.time = OUTRO_FINAL_REVEAL_TIME;
            audio_play(&game->platform.audio, SFX_WIN);
            clear_edge_input(game);
            return true;
        }

        Uint32 cues = 0;
        outro_cutscene_update(&game->presentation.outro_cutscene, dt, &cues);
        if (cues & OUTRO_CUE_DOOR)
            audio_play(&game->platform.audio, SFX_DOOR);
        if (cues & OUTRO_CUE_STEP_A)
            audio_play(&game->platform.audio, SFX_STEP_A);
        if (cues & OUTRO_CUE_STEP_B)
            audio_play(&game->platform.audio, SFX_STEP_B);
        if (cues & OUTRO_CUE_HELICOPTER)
            audio_play(&game->platform.audio, SFX_OUTRO_HELICOPTER);
        if (cues & OUTRO_CUE_PLAYER_SHOT)
            audio_play(&game->platform.audio, SFX_PLAYER_SHOT);
        if (cues & OUTRO_CUE_ENEMY_DOWN)
            audio_play(&game->platform.audio, SFX_ENEMY_DOWN);
        if (cues & OUTRO_CUE_EXPLOSION)
            audio_play(&game->platform.audio, SFX_EXPLOSION);
        if (cues & OUTRO_CUE_WIN)
            audio_play(&game->platform.audio, SFX_WIN);

        clear_edge_input(game);
        return true;
    }

    /* If restart requested via input handler while in end state, restart. */
    if (game->input.restart &&
        game->state == STATE_GAME_OVER)
    {
        restart_game(game);
        clear_edge_input(game);
    }

    /* Level start reveal animation: show tiles progressively, then spawn entities. */
    if (game->state == STATE_LEVEL_START)
    {
        /* Do not defer actions pressed during a non-interactive transition. */
        clear_edge_input(game);

        /* Advance reveal; if not finished yet, skip the rest of update. */
        int reveal_row = game->gameplay.level.reveal.next_row;
        int reveal_col = game->gameplay.level.reveal.next_col;
        level_reveal_step(&game->gameplay.level, (float)dt);
        if (reveal_row != game->gameplay.level.reveal.next_row ||
            reveal_col != game->gameplay.level.reveal.next_col)
        {
            audio_play(&game->platform.audio, SFX_REVEAL_TICK);
        }
        if (!game->gameplay.level.reveal.done)
        {
            return true;
        }
        /* Reveal finished: either start key-card intro animation or spawn entities. */
        if (game->gameplay.level.runtime.card_count > 0 && game->gameplay.level.runtime.active_card_index >= 0)
        {
            /* Determine active card position among card items. */
            int active_pos = 0;
            for (int i = 0; i < game->gameplay.level.runtime.item_count; ++i)
            {
                if (game->gameplay.level.runtime.items[i].type != ITEM_CARD)
                    continue;
                if (i == game->gameplay.level.runtime.active_card_index)
                    break;
                active_pos++;
            }
            game->presentation.card_anim_count = game->gameplay.level.runtime.card_count;
            game->presentation.card_anim_current = 0;
            game->presentation.card_anim_step = 0;
            game->presentation.card_anim_timer = 0.0f;
            /* Run a few cycles before landing on the chosen card. Compute
             * interval so the total animation duration is roughly constant
             * regardless of number of cards. */
            int cycles = 3;
            game->presentation.card_anim_total_steps = cycles * game->presentation.card_anim_count + active_pos;
            if (game->presentation.card_anim_total_steps <= 0)
            {
                game->presentation.card_anim_interval = 0.075f;
            }
            else
            {
                const float target_total = 0.75f; /* total seconds for keycard intro */
                float interval = target_total / (float)game->presentation.card_anim_total_steps;
                const float min_interval = 0.02f; /* don't go too fast per step */
                const float max_interval = 0.15f; /* don't go too slow per step */
                if (interval < min_interval)
                    interval = min_interval;
                if (interval > max_interval)
                    interval = max_interval;
                game->presentation.card_anim_interval = interval;
            }
            game_enter_state(game, STATE_SHOW_KEYCARD);
        }
        else
        {
            gameplay_ai_spawn_level_entities(&game->gameplay);
            game_enter_state(game, STATE_PLAYING);
        }
    }

    /* Key-card intro animation: cycle highlight until target reached, then begin play */
    if (game->state == STATE_SHOW_KEYCARD)
    {
        game->presentation.card_anim_timer += dt;
        if (game->presentation.card_anim_timer >= game->presentation.card_anim_interval)
        {
            game->presentation.card_anim_timer -= game->presentation.card_anim_interval;
            game->presentation.card_anim_step++;
            if (game->presentation.card_anim_count > 0)
                game->presentation.card_anim_current = (game->presentation.card_anim_current + 1) % game->presentation.card_anim_count;
            if (game->presentation.card_anim_step >= game->presentation.card_anim_total_steps)
            {
                /* Animation finished: spawn entities and start playing. */
                audio_play(&game->platform.audio, SFX_CARD_TARGET);
                gameplay_ai_spawn_level_entities(&game->gameplay);
                game_enter_state(game, STATE_PLAYING);
            }
            else
            {
                audio_play(&game->platform.audio, SFX_CARD_SCAN);
            }
        }
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_LEVEL_CLEARED)
    {
        game->presentation.message_timer -= dt;
        if (game->presentation.message_timer <= 0.0f)
        {
            advance_level(game);
        }
        clear_edge_input(game);
        return true;
    }

    /* If player is in dying animation, update particles and wait */
    if (game->gameplay.player.dying)
    {
        particle_system_update(&game->presentation.particles, dt);
        game->gameplay.player.death_timer -= dt;
        if (game->gameplay.player.death_timer <= 0.0f)
            finish_player_death(game);
        clear_edge_input(game);
        return true;
    }

    if (game->state != STATE_PLAYING)
    {
        clear_edge_input(game);
        return true;
    }

    return false;
}

static void update_playing(Game *game, float dt)
{
    game->campaign.level_elapsed_time += dt;

    if (game->gameplay.invuln_timer > 0.0f)
    {
        game->gameplay.invuln_timer -= dt;
    }
    if (game->presentation.exit_unlocked_timer > 0.0f)
    {
        game->presentation.exit_unlocked_timer -= dt;
        if (game->presentation.exit_unlocked_timer < 0.0f)
            game->presentation.exit_unlocked_timer = 0.0f;
    }

    gameplay_prepare_terminal(&game->gameplay, &game->input, dt);

    bool player_was_grounded = game->gameplay.player.on_ground;
    bool jump_requested =
        !game->gameplay.terminal_hacking &&
        game->input.jump &&
        game->gameplay.player.on_ground;
    float player_fall_speed = game->gameplay.player.vy;
    int previous_elevator = game->gameplay.player_on_elevator;
    int previous_moving_platform =
        game->gameplay.player_on_moving_platform;

    /* --- Elevator: pre-carry (upward), player physics, update platforms, snap --- */
    {
        /* If riding an upward-moving elevator, nudge the player up before physics
         * so gravity doesn't immediately drop them off. */
        if (previous_elevator >= 0 &&
            previous_elevator < game->gameplay.level.runtime.elevator_count)
        {
            const Elevator *el = &game->gameplay.level.runtime.elevators[previous_elevator];
            if (el->vy < 0.0f)
            {
                game->gameplay.player.y += el->vy * dt;
                game->gameplay.player.vy = 0.0f;
            }
        }
    }

    /* Crush detection: if anything pushed the player into a solid tile overhead, they die. */
    {
        int col_left = (int)floorf(game->gameplay.player.x / (float)TILE_SIZE);
        int col_right = (int)floorf((game->gameplay.player.x + PLAYER_W - 1.0f) / (float)TILE_SIZE);
        int row_top = (int)floorf(game->gameplay.player.y / (float)TILE_SIZE);
        for (int c = col_left; c <= col_right; ++c)
        {
            if (level_is_solid(&game->gameplay.level, c, row_top))
            {
                gameplay_hit_player(&game->gameplay);
                break;
            }
        }
    }

    float prev_player_x = game->gameplay.player.x;
    float prev_player_y = game->gameplay.player.y;
    float prev_player_h = game->gameplay.player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;

    Input player_input = game->input;
    if (game->gameplay.terminal_hacking)
    {
        player_input.left = false;
        player_input.right = false;
        player_input.up = false;
        player_input.down = false;
        player_input.jump = false;
        player_input.shoot = false;
        player_input.use_door = false;
        game->input.shoot = false;
        game->input.use_door = false;
        game->gameplay.player.vx = 0.0f;
    }
    player_update(&game->gameplay.player, &game->gameplay.level, &player_input, dt);
    game->input.jump = false;

    if (gameplay_advance_terminal(&game->gameplay, &game->campaign, dt))
        game->presentation.exit_unlocked_timer = 2.5f;

    level_update_elevators(&game->gameplay.level, dt);
    level_update_falling_platforms(&game->gameplay.level, dt);
    level_update_moving_platforms(&game->gameplay.level, dt);
    gameplay_update_crates(&game->gameplay, &game->campaign, dt);
    gameplay_resolve_player_crates(&game->gameplay, prev_player_x, prev_player_y, prev_player_h);
    game->gameplay.player_on_elevator = -1;
    for (int i = 0; i < game->gameplay.level.runtime.elevator_count; ++i)
    {
        const Elevator *el = &game->gameplay.level.runtime.elevators[i];
        float plat_x = el->col * (float)TILE_SIZE;
        float player_cx = game->gameplay.player.x + PLAYER_W * 0.5f;
        float ph = game->gameplay.player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_feet = game->gameplay.player.y + ph;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            game->gameplay.player.vy >= 0.0f &&
            player_feet >= el->y - 2.0f &&
            player_feet <= el->y + ELEVATOR_PLAT_H + 8.0f)
        {
            game->gameplay.player.y = el->y - ph;
            game->gameplay.player.vy = 0.0f;
            game->gameplay.player.on_ground = true;
            game->gameplay.player_on_elevator = i;
        }
    }

    /* Moving platforms: detect if player stands on one and snap/carry them. */
    game->gameplay.player_on_moving_platform = -1;
    for (int i = 0; i < game->gameplay.level.runtime.moving_platform_count; ++i)
    {
        const MovingPlatform *mp = &game->gameplay.level.runtime.moving_platforms[i];
        float plat_x = mp->x;
        float player_cx = game->gameplay.player.x + PLAYER_W * 0.5f;
        float ph = game->gameplay.player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_feet = game->gameplay.player.y + ph;
        float plat_top = mp->row * (float)TILE_SIZE;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            game->gameplay.player.vy >= 0.0f &&
            player_feet >= plat_top - 2.0f &&
            player_feet <= plat_top + MOVING_PLATFORM_H + 8.0f)
        {
            game->gameplay.player.y = plat_top - ph;
            game->gameplay.player.vy = 0.0f;
            game->gameplay.player.on_ground = true;
            game->gameplay.player_on_moving_platform = i;
            break;
        }
    }

    /* If riding a moving platform, carry the player horizontally. */
    if (game->gameplay.player_on_moving_platform >= 0 && game->gameplay.player_on_moving_platform < game->gameplay.level.runtime.moving_platform_count)
    {
        const MovingPlatform *mp = &game->gameplay.level.runtime.moving_platforms[game->gameplay.player_on_moving_platform];
        game->gameplay.player.x += mp->vx * dt;
    }

    if (jump_requested && game->gameplay.player.vy < -1.0f)
        game_events_sound(&game->gameplay.events, SFX_JUMP);
    if (!player_was_grounded && game->gameplay.player.on_ground &&
        player_fall_speed > 150.0f)
    {
        game_events_sound(&game->gameplay.events, SFX_LAND);
    }
    if (game->gameplay.player_on_elevator >= 0 && previous_elevator < 0)
        game_events_sound(&game->gameplay.events, SFX_ELEVATOR);
    if (game->gameplay.player_on_moving_platform >= 0 &&
        previous_moving_platform < 0)
    {
        const MovingPlatform *platform =
            &game->gameplay.level.runtime.moving_platforms[
                game->gameplay.player_on_moving_platform];
        gameplay_world_sound(
            &game->gameplay, SFX_MOVING_PLATFORM,
            platform->x + TILE_SIZE * 0.5f,
            platform->row * (float)TILE_SIZE + MOVING_PLATFORM_H * 0.5f);
    }

    if (game->gameplay.player.on_ladder && fabsf(game->gameplay.player.vy) > 1.0f)
    {
        game->presentation.ladder_audio_timer -= dt;
        if (game->presentation.ladder_audio_timer <= 0.0f)
        {
            game_events_sound(&game->gameplay.events, SFX_LADDER);
            game->presentation.ladder_audio_timer = 0.27f;
        }
    }
    else
    {
        game->presentation.ladder_audio_timer = 0.0f;
    }

    if (game->gameplay.player.on_ground && !game->gameplay.player.on_ladder &&
        fabsf(game->gameplay.player.vx) > 8.0f)
    {
        game->presentation.footstep_audio_timer -= dt;
        if (game->presentation.footstep_audio_timer <= 0.0f)
        {
            game_events_sound(&game->gameplay.events, game->presentation.footstep_alternate ? SFX_STEP_A : SFX_STEP_B);
            game->presentation.footstep_alternate = !game->presentation.footstep_alternate;
            game->presentation.footstep_audio_timer = game->gameplay.player.crawling ? 0.40f : 0.27f;
        }
    }
    else
    {
        game->presentation.footstep_audio_timer = 0.0f;
    }

    if (game->gameplay.teleport_cooldown > 0.0f)
        game->gameplay.teleport_cooldown -= dt;

    gameplay_combat_update_explosives(&game->gameplay, &game->campaign, dt);

    SublevelDoorAction sublevel_action =
        gameplay_use_sublevel_door(&game->gameplay, &game->input);
    if ((sublevel_action == SUBLEVEL_DOOR_ENTER && enter_restroom(game)) ||
        (sublevel_action == SUBLEVEL_DOOR_RETURN && leave_restroom(game)))
    {
        return;
    }
    gameplay_use_door(&game->gameplay, &game->input);

    gameplay_ai_update_spawns(&game->gameplay, dt);

    gameplay_combat_handle_player_action(&game->gameplay, &game->campaign,
                                         &game->input);

    gameplay_ai_update_movement(&game->gameplay, dt);

    for (int i = 0; i < game->gameplay.level.runtime.fall_platform_count; ++i)
    {
        FallPlatform *platform = &game->gameplay.level.runtime.fall_platforms[i];
        if (platform->triggered && !game->presentation.fall_platform_sounded[i])
        {
            game->presentation.fall_platform_sounded[i] = true;
            gameplay_world_sound(&game->gameplay, SFX_PLATFORM_CRACK,
                platform->col * (float)TILE_SIZE + TILE_SIZE * 0.5f,
                platform->y + FALL_PLATFORM_H * 0.5f);
        }
    }

    bool exit_was_unlocked = game->gameplay.level.runtime.exit_unlocked;
    gameplay_collect_items(&game->gameplay, &game->campaign, dt);
    if (!exit_was_unlocked && game->gameplay.level.runtime.exit_unlocked)
        game->presentation.exit_unlocked_timer = 2.5f;

    gameplay_combat_update_hazards(&game->gameplay);

    gameplay_combat_update_player_bullets(&game->gameplay, &game->campaign, dt);

    gameplay_ai_update_combat(&game->gameplay, dt);

    gameplay_combat_update_enemy_bullets(&game->gameplay, dt);

    gameplay_combat_check_contacts(&game->gameplay);

    /* Tick the calm countdown after perception updates, so a guard or dog
     * seeing Chuck on the would-be final frame keeps the alarm alive. */
    gameplay_update_alarm(&game->gameplay, dt);

    if (gameplay_player_reached_exit(&game->gameplay))
    {
        if ((size_t)(game->campaign.current_level + 1) < EMBEDDED_LEVEL_COUNT)
        {
            level_transition_init(
                &game->presentation.level_transition,
                game->campaign.current_level,
                game->campaign.current_level + 1,
                game->campaign.level_elapsed_time,
                game->campaign.score - game->campaign.level_start_score,
                gameplay_neutralized_hostiles(&game->gameplay));
            audio_stop_music(&game->platform.audio);
            game_enter_state(game, STATE_LEVEL_TRANSITION);
        }
        else
            game_enter_state(game, STATE_LEVEL_CLEARED);
        game_events_sound(&game->gameplay.events, SFX_LEVEL_CLEAR);
    }

    /* Camera: follow player horizontally, clamped to level bounds */
    {
        int win_w = 0, win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        float desired = game->gameplay.player.x + PLAYER_W * 0.5f - (float)win_w * 0.5f;
        float max_cam = game->gameplay.level.map.width * (float)TILE_SIZE - (float)win_w;
        if (max_cam < 0.0f)
            max_cam = 0.0f;
        if (desired < 0.0f)
            desired = 0.0f;
        if (desired > max_cam)
            desired = max_cam;
        /* Smoothly interpolate camera towards desired position (lerp). */
        float alpha = 8.0f * dt; /* responsiveness factor */
        if (alpha > 1.0f)
            alpha = 1.0f;
        game->presentation.cam_x += (desired - game->presentation.cam_x) * alpha;
    }
}

void game_update(Game *game, float dt)
{
    game_events_clear(&game->gameplay.events);
    game_read_input(game);
    audio_update_music(&game->platform.audio);
    bool scene_handled_frame = update_scene(game, dt);
    if (!scene_handled_frame)
        update_playing(game, dt);
    dispatch_game_events(game);
    if (!scene_handled_frame)
    {
        particle_system_update(&game->presentation.particles, dt);
        update_camera_shake(game, dt);
    }
}

/* Shutdown and free game resources. */

void game_shutdown(Game *game)
{
    audio_shutdown(&game->platform.audio);
    if (game->platform.renderer)
    {
        SDL_DestroyRenderer(game->platform.renderer);
    }
    if (game->platform.window)
    {
        SDL_DestroyWindow(game->platform.window);
    }
    SDL_Quit();
}
